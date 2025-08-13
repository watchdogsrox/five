//
// Task/Combat/Cover/TaskCover.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_COVER_H
#define INC_TASK_COVER_H

////////////////////////////////////////////////////////////////////////////////

// Game Headers
#include "Peds/QueriableInterface.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "task/Movement/TaskNavBase.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFsmClone.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/WeaponController.h"

// Rage forward declarations
namespace rage { struct CollisionMaterialSettings; }

// Game forward declarations
class CEventCallForCover;
class CEventGunAimedAt;
class CTaskMotionAiming;

#define DISABLE_LOW_COVER_EDGE_PEEK 1
////////////////////////////////////////////////////////////////////////////////

typedef struct 
{
	Vector3 m_CoverProtectionDir;
	Vector3 m_CoverSearchDir;
	Vector3 m_vPreviousCoverDir;
	float   m_CoverSearchDist;
	float	m_BestScore;
	u32	m_iSearchFlags;
	CPed* m_pPlayerPed;
	bool	m_HasThreat;
} PlayerCoverSearchData;

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Main cover task, handles finding and taking cover
////////////////////////////////////////////////////////////////////////////////

class CTaskCover : public CTaskFSMClone
{
	friend class CTaskEnterCover;
	friend class CTaskInCover;

public:

#if __BANK
	static void SpewStreamingDebugToTTY(const CPed& rPed, const fwMvClipSetId clipSetId);
#endif // __BANK

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_FPSBlindFireOutroBlendOutPelvisOffsetTime;
		float m_FPSAimOutroBlendOutPelvisOffsetTime;
		float m_FPSPeekToAimBlendDurationHigh;
		float m_FPSPeekToAimBlendDurationLow;
		float m_FPSPeekToBlindFireBlendDurationHigh;
		float m_FPSPeekToBlindFireBlendDurationLow;
		float m_FPSDefaultBlendDurationHigh;
		float m_FPSDefaultBlendDurationLow;
		float m_AngleToCameraWeighting;
		float m_AngleToDynamicCoverWeighting;
		float m_PriorityCoverWeighting;
		float m_DistanceWeighting;
		float m_AngleToCoverWeighting;
		float m_AngleOfCoverWeighting;
		float m_EdgeWeighting;
		float m_NetworkBlendOutDurationRun;
		float m_NetworkBlendOutDurationRunStart;
		float m_NetworkBlendOutDuration;
		float m_MaxAngularDiffBetweenDynamicAndStaticCover;	
		float m_RangeToUseDynamicCoverPointMin;	
		float m_RangeToUseDynamicCoverPointMax;	
		float m_DistToDynamicUseRangeMax;	
		float m_MinDistToCoverAnyDir;
		float m_MinDistToPriorityCoverToForce;
		float m_MinDistToCoverSpecificDir;
		float m_BehindPedToCoverCosTolerance;
		float m_SearchToCoverCosTolerance;
		float m_CapsuleZOffset;
		float m_TimeBetweenTestSpheresIntersectingRoute;
		float m_MaxDistToCoverWhenPlayerIsClose;
		float m_MinCoverToPlayerCoverDist;
		float m_MinMoveToCoverDistForCoverMeAudio;
		float m_MaxSecondsAsTopLevelTask;
		bool  m_ForceStreamingFailure;

		fwMvClipSetId GetStreamedUnarmedCoverMovementFirstPersonClipSetId() const { return ms_Tunables.m_StreamedUnarmedCoverMovementFPSClipSetId; }
		fwMvClipSetId GetStreamedUnarmedCoverMovementThirdPersonClipSetId() const { return ms_Tunables.m_StreamedUnarmedCoverMovementClipSetId; }
		fwMvClipSetId GetStreamedUnarmedCoverMovementClipSetIdForPed(const CPed& rPed) const;
		fwMvClipSetId GetCoreWeaponAimingFirstPersonClipSetId() const { return ms_Tunables.m_CoreWeaponAimingFPSClipSetId; }
		fwMvClipSetId GetCoreWeaponAimingThirdPersonClipSetId() const { return ms_Tunables.m_CoreWeaponAimingClipSetId; }
		fwMvClipSetId GetCoreWeaponAimingClipSetIdForPed(const CPed& rPed) const;
		float GetMaxPlayerToCoverDist(const CPed& rPed) const;

		fwMvClipSetId m_StreamedOneHandedCoverMovementClipSetId;
		fwMvClipSetId m_AIOneHandedAimingClipSetId;
		fwMvClipSetId m_AITwoHandedAimingClipSetId;
		fwMvClipSetId m_CoreWeaponClipSetId;

		private:

		fwMvClipSetId m_StreamedUnarmedCoverMovementClipSetId;
		fwMvClipSetId m_StreamedUnarmedCoverMovementFPSClipSetId;
		fwMvClipSetId m_CoreWeaponAimingClipSetId;
		fwMvClipSetId m_CoreWeaponAimingFPSClipSetId;
		float m_MaxPlayerToCoverDist;
		float m_MaxPlayerToCoverDistFPS;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvNetworkId	ms_MotionTaskNetworkId;
	static const fwMvNetworkId	ms_PrimaryTaskNetworkId;

	// MoveStateRequest Signals
	static const fwMvRequestId	ms_TaskRequestId;

	static const fwMvRequestId  ms_NoTaskIgnoreMoverBlendRequestId;
	static const fwMvRequestId	ms_NoTaskRequestId;
	static const fwMvRequestId	ms_NoMotionTaskRequestId;
	static const fwMvRequestId	ms_MotionTaskRequestId;
	static const fwMvRequestId	ms_MotionTaskIgnoreMoverBlendRequestId;

	static const fwMvFloatId	ms_TaskBlendDurationId;
	static const fwMvFloatId	ms_NoMotionTaskBlendDurationId;
	static const fwMvFloatId	ms_NoTaskBlendDurationId;
	static const fwMvBooleanId	ms_MotionTaskOnEnterId;
	static const fwMvFlagId		ms_IsCrouchedId;
	static const fwMvFilterId	ms_IgnoreMoverBlendFilterId;
	static const fwMvBooleanId	ms_CoverSearchInterruptId;
	static const float			INVALID_COVER_SCORE;

	////////////////////////////////////////////////////////////////////////////////

	static inline void RequestCoverClipSet(fwMvClipSetId clipSetId, eStreamingPriority priority = SP_Invalid, float fLifeTime = -1.0f)
	{
		if (clipSetId != CLIP_SET_ID_INVALID && !ms_Tunables.m_ForceStreamingFailure)
		{
			CPrioritizedClipSetRequestManager::GetInstance().Request(CPrioritizedClipSetRequestManager::RC_Cover, clipSetId, priority, fLifeTime, false);
		}
	}

	static inline bool RequestCoverClipSetReturnIfLoaded(fwMvClipSetId clipSetId, eStreamingPriority priority = SP_Invalid, float fLifeTime = -1.0f)
	{
		if (clipSetId != CLIP_SET_ID_INVALID && !ms_Tunables.m_ForceStreamingFailure)
		{
			return CPrioritizedClipSetRequestManager::GetInstance().Request(CPrioritizedClipSetRequestManager::RC_Cover, clipSetId, priority, fLifeTime, true);
		}
		return false;
	}

	static inline bool IsCoverClipSetLoaded(fwMvClipSetId clipSetId)
	{
		if (clipSetId != CLIP_SET_ID_INVALID && !ms_Tunables.m_ForceStreamingFailure)
		{
			return CPrioritizedClipSetRequestManager::GetInstance().IsLoaded(clipSetId);
		}
		return false;
	}

	////////////////////////////////////////////////////////////////////////////////

#if FPS_MODE_SUPPORTED
	// We use custom cover animations in third person
	static bool GetShouldUseCustomLowCoverAnims(const CPed& rPed);
	static bool IsPlayerAimingDirectlyInFirstPerson(const CPed& rPed);
#endif // FPS_MODE_SUPPORTED

	static bool CanUseThirdPersonCoverInFirstPerson(const CPed& rPed);

	static bool HitInstIsFoliage(const phInst& rInst, u16 uHitComponent);
	static bool IsPlayerPedInCoverAgainstVehicleDoor(const CPed& rPed, const CCoverPoint& rCoverPoint);
	static CEntity* GetEntityPlayerIsInCoverAgainst(const CPed& rPed, const CCoverPoint& rCoverPoint);

	// Returns true if the coverpoints is considered valid
	static bool IsPedValidThreat(const CPed& rPed, const CPed& rPlayerPed, bool& bHasLos);
	static bool ComputeDesiredProtectionDirection(const CPed& rPlayerPed, const Vector3& vCoverPosition, Vector3& vDesiredProtection);
	static float PlayerCoverpointScoringCallback(const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData);
	static bool CalculateCoverSearchVariables(PlayerCoverSearchData& coverSearchData);
	static fwMvClipSetId GetWeaponHoldingClipSetForArmament(const CPed* pPed, bool bIgnoreWeaponObjectCheck = false);
	static fwMvClipSetId GetWeaponHoldingExtraClipSetForArmament(const CPed& rPed);
	static fwMvClipSetId GetStreamedClipSetForArmament(const CPed* pPed, bool bForceUseOneHandedSetIfApplicable);
	static fwMvClipSetId GetWeaponClipSetForArmament(const CPed* pPed, bool bUseAIAimingAnims);
	static fwMvClipSetId GetAIAimingClipSetHashForArmament(const CPed* pPed);

	static u32 ComputeCameraHash(const CPed& ped);
	static Vector3 GetCamPosForCover();
	static Vector3 GetCamFrontForCover();

	// PURPOSE: Compute the stick input relative to the cover we're in
	static Vector2 ComputeStickInput(const CPed& ped, const Vector3& vCoverDirection);
	static Vector2 ComputeStickInput(const CPed& ped);
	static Vector3 ComputeCameraHeadingVec(const CPed& ped);

	// PURPOSE: Make sure we're not going to go through any peds or vehicles
	static bool IsPathClearForPed(const CPed& ped, const Vector3& vStart, const Vector3& vEnd, const Vector3& vOffset, s32 iTypeFlags);
	static bool DoCapsuleTest(const CPed& ped, const Vector3& vStart, const Vector3& vEnd, s32 iTypeFlags);

	//called from other cover tasks for simple left hand IK
	static void UpdateLeftHandIk(CPed* pPed);
	static bool	IsCoverValid(const CCoverPoint& rCoverPoint, const CPed& rPed);
	static bool	IsCoverPositionValid(const Vector3& vCoverPosition, const CPed& rPed);

	static bool IsCoverPointPositionTooFarAndAcute(const CPed& rPed, const Vector3& vCoverPosition);
	static bool DoesCoverPointProvideCoverFromThreat(const Vector3& vCoverDir, const Vector3& vThreatDir);
	static bool VerifyCoverPointDirection(const CCoverPoint& rCoverPoint);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Control if and how we seek cover
	enum eSeekCoverFlags
	{
		// Should we seek cover, if so which search type
		CF_SeekCover							= BIT0,
		CF_CoverSearchAny						= BIT1,
		CF_CoverSearchByPos						= BIT2,
		CF_CoverSearchScripted					= BIT3,

		// Flags to alter specify cover search conditions
		CF_RequiresLosToTarget					= BIT4,
		CF_DontConsiderBackwardsPoints			= BIT5,
		CF_SearchInAnArcAwayFromTarget			= BIT6,
		CF_FindClosestPointAtStart				= BIT7,

		// Flags to specify how any specified subtask should run
		CF_RunSubtaskWhenMoving					= BIT8,
		CF_RunSubtaskWhenStationary 			= BIT9,

		// Should the ped move to the search location specified even if no cover is found
		CF_DontMoveIfNoCoverFound				= BIT10,

		// Misc
		CF_IgnoreNonSignificantObjects			= BIT11,
		CF_QuitAfterCoverEntry					= BIT12,
		CF_MoveToPedsCover						= BIT13,
		CF_QuitAfterMovingToCover				= BIT14,
		CF_ScriptedSeekCover					= BIT15,
		CF_NeverEnterWater						= BIT16,
		CF_DontUseLadders						= BIT17,
		CF_DontUseClimbs						= BIT18,
		CF_KeepMovingWhilstWaitingForPath		= BIT19,
		CF_UseLargerSearchExtents				= BIT20,
		CF_AllowClimbLadderSubtask				= BIT21,
	};

	////////////////////////////////////////////////////////////////////////////////

	enum eCoverFlags
	{
		// Running Flags synced over network
		CF_FacingLeft							= BIT0, 
		CF_TooHighCoverPoint					= BIT1,
		CF_EnterLeft							= BIT2,
		CF_CrouchingAtStart						= BIT3,
		CF_CloseToPossibleCorner				= BIT4,
		CF_AtCorner								= BIT5,
		CF_UseOverTheTopFiring					= BIT6, 
		CF_AimDirectly							= BIT7, // last flag synced over network

		CF_LastSyncedFlag						= CF_AimDirectly,

		// Config Flags
		CF_SpecifyInitialHeading				= BIT8,
		CF_SkipIdleCoverTransition				= BIT9,
		CF_AimAtTargetIfNoCover					= BIT10,
		CF_PutPedDirectlyIntoCover				= BIT11,
		CF_DisableAimingAndPeeking				= BIT12,

		// Running Flags not synced over network
		CF_AimOnly								= BIT13,
		CF_CanBlindFire							= BIT14,
		CF_UseOverTheTopBlindFiring				= BIT15,
		CF_UseOverTheTopBlindFiringDueToAngle	= BIT16,
		CF_ReleasedFireToThrow					= BIT17,
		CF_ShouldSlidePedIntoCover				= BIT18,
		CF_ShouldSlidePedAgainstCover			= BIT19,
		CF_FacePedInCorrectDirection			= BIT20,
		CF_AbortTask							= BIT21,
		CF_CoverFire							= BIT22,
		CF_ShouldReturnToIdle					= BIT23,
		CF_TurningLeft							= BIT24,
		CF_AimIntroRequested					= BIT25,
		CF_StrafeToCover						= BIT26,
		CF_RunningPeekVariation					= BIT27,
		CF_RunningPinnedVariation				= BIT28,
		CF_RunningIdleVariation					= BIT29,
		CF_DesiredFacingLeft					= BIT30, // only used in the clone task, this is the facing left flag received in network updates
		CF_WarpPedToCoverPosition				= BIT31
	};

	////////////////////////////////////////////////////////////////////////////////

	enum eAnimFlags
	{
		AF_Low			= BIT0,
		AF_EnterLeft	= BIT1,
		AF_FaceLeft		= BIT2,
		AF_AtEdge		= BIT3,
		AF_ToLow		= BIT4,
		AF_AimDirect	= BIT5,
		AF_EnterCenter	= BIT6,
		AF_ToPeek		= BIT7,
		AF_Scope		= BIT8
	};

	////////////////////////////////////////////////////////////////////////////////

	// ClipIds are defined in the combat metadata, chosen based on the cover flags and passed into the move network 
	struct CoverAnimStateInfo
	{
		atArray<fwMvClipId> m_Clips;
		s32					m_Flags;
		PAR_SIMPLE_PARSABLE;
	};

	static const CTaskCover::CoverAnimStateInfo* GetAnimStateInfoForState(const atArray<CTaskCover::CoverAnimStateInfo>& aCoverAnimStateInfos, s32 iFlags);
	static float ComputeMvCamHeadingForPed(const CPed& rPed);

	////////////////////////////////////////////////////////////////////////////////

	enum eInCoverTerminationReason
	{
		TR_IdleExit,
		TR_IdleExitToRun,
		TR_IdleExitToRunStart,
		TR_CornerExit,
		TR_NormalExit,
		NUM_TerminationReasons
	};

	////////////////////////////////////////////////////////////////////////////////

	enum eForcedExit
	{
		FE_None,
		FE_Idle,
		FE_Aim,
		FE_Corner
	};

	////////////////////////////////////////////////////////////////////////////////

	CTaskCover(const CWeaponTarget& coverThreat, s32 iCoverFlags = 0, eForcedExit forcedExit = FE_None);
	virtual ~CTaskCover();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define COVER_STATES(X)	X(State_Start),					\
							X(State_Resume),				\
							X(State_ExitVehicle),			\
							X(State_SeekCover),				\
							X(State_MoveToCover),			\
							X(State_MoveToSearchLocation),	\
							X(State_EnterCover),			\
							X(State_UseCover),				\
							X(State_ExitCover),				\
							X(State_Finish)

	// PURPOSE: FSM states
	enum eCoverState
	{
		COVER_STATES(DEFINE_TASK_ENUM)
	};

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_COVER; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const;

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	bool IsConsideredInCoverForCamera() const;

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_TaskUseCoverMoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_TaskUseCoverMoveNetworkHelper; }

	// PURPOSE: Set the search flags
	void SetSearchFlags(s32 iFlags) { m_SeekCoverFlags = iFlags; }

	// PURPOSE: Set the search position
	void SetSearchPosition(const Vector3& vSearchPos) { m_vSearchPosition = vSearchPos; }
	Vector3& GetSearchPosition() { return m_vSearchPosition; }

	// PURPOSE: Set the move blend ratio
	void SetMoveBlendRatio(float fMoveBlendRatio) { m_fMoveBlendRatio = fMoveBlendRatio; }

	// PURPOSE: Set the time in cover
	void SetTimeInCover(float fTimeInCover) { m_fTimeInCover = fTimeInCover; }

	void SetVehicleStoodOn(const CVehicle* pVehicleStoodOn) { m_pVehicleStoodOn = pVehicleStoodOn; }
	const CVehicle* GetVehicleStoodOn() const { return m_pVehicleStoodOn; }

	// PURPOSE: Set/Get the cover search type
	void SetSearchType(s32 iSearchType) { m_iSearchType = iSearchType; }
	s32 GetCoverSearchType() const { return m_iSearchType; }

	// PURPOSE: Get the target we're taking cover from
	const CWeaponTarget& GetWeaponTarget() const { return m_CoverThreat; }
	CWeaponTarget& GetWeaponTarget() { return m_CoverThreat; }

	// PURPOSE: Change the minimum and maximum search distances
	void SetMinMaxSearchDistances( float fMinDistance, float fMaxDistance ) {m_fSearchDistanceMin = fMinDistance; m_fSearchDistanceMax = fMaxDistance;}

	// PURPOSE: Set the cover index
	void SetScriptedCoverIndex(s32 iScriptedCoverIndex) { m_iScriptedCoverIndex = iScriptedCoverIndex; }
	s32 GetScriptedCoverIndex() const { return m_iScriptedCoverIndex; }

	// PURPOSE: Set the subtask to use whilst searching
	void SetSubtaskToUseWhilstSearching(aiTask* pSubtask);
	aiTask* GetSubTaskWhilstSearching() const { return m_pSubTaskWhilstSearching; }

	// PURPOSE: Function prototype of a callback fn used to construct a filter object, which checks the validity of a selected cover point
	void SetCoverPointFilterConstructFn( CCover::CoverPointFilterConstructFn* pFn ) { m_pCoverPointFilterConstructFn = pFn;}

	void SetSpheresOfInfluenceBuilder(CTaskNavBase::SpheresOfInfluenceBuilder* pBuilder) { m_pSpheresOfInfluenceBuilder = pBuilder; }

	// PURPOSE: Check if a cover flag is set
	bool IsCoverFlagSet(u32 iFlag) const { return m_CoverFlags.IsFlagSet(iFlag); }

	// PURPOSE: Check if our cover finder is active
	bool IsSeekingCover() const { return m_pCoverFinder && m_pCoverFinder->IsActive(); }

	// PURPOSE: Starts the main move network 'TaskCover'
	bool StartCoverNetwork(bool bTagSync = false, bool bRestartMotionNetwork = true, bool bRestartTaskNetwork = true, bool bIgnoreMoverBlend = false, float fCustomBlendDuration = -1.0f);

	bool RestartUseCoverNetwork(CPed* pPed);
	void RestartMotionInCoverNetwork(CPed* pPed, bool bIgnoreMoverBlend = false);

	// PURPOSE: Get/Set the task networks blend in duration when putting ped directly into cover
	void SetBlendInDuration(float fBlendInDuration) { m_fBlendInDuration = fBlendInDuration; }
	float GetBlendInDuration() const { return m_fBlendInDuration; }

	// PURPOSE: Set reason the in cover task is quitting to determine how we exit cover
	void SetInCoverTerminationReason(eInCoverTerminationReason reason) { m_eInCoverTerminationReason = reason; }
	eInCoverTerminationReason GetInCoverTerminationReason() const { return m_eInCoverTerminationReason; }

	// PURPOSE: Get the streamed clip set request helper
	bool IsCoreWeaponClipSetLoaded() const { return CTaskCover::IsCoverClipSetLoaded(ms_Tunables.m_CoreWeaponClipSetId); }
	bool IsWeaponClipSetLoaded(bool bSwitchingWeapons = false) const;
	bool IsCurrentWeaponClipSetLoaded() const;

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool				OverridesNetworkBlender(CPed*);
	virtual bool				OverridesNetworkHeadingBlender(CPed* pPed) { return OverridesNetworkBlender(pPed); }
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual void				LeaveScope(const CPed* pPed);
	virtual bool				IsInScope(const CPed* pPed);

	// PURPOSE: Returns true if the ped has a cover point, can be used by local or clone peds
	bool HasNetCoverPoint(const CPed& ped) const;
	// PURPOSE: Returns the cover point position, can be used by local or clone peds
	bool GetNetCoverPointPosition(const CPed& ped, Vector3& vCoverPointPos) const;
	// PURPOSE: Returns the cover point direction, can be used by local or clone peds
	bool GetNetCoverPointDirection(const CPed& ped, Vector3& vCoverPointDirection) const;
	// PURPOSE: Returns the cover point usage, can be used by local or clone peds
	bool GetNetCoverPointUsage(const CPed& ped, CCoverPoint::eCoverUsage& coverPointUsage) const;
	// PURPOSE: Returns the cover point height, can be used by local or clone peds
	bool GetNetCoverPointHeight(const CPed& ped, CCoverPoint::eCoverHeight& coverPointHeight) const;
	// PURPOSE: Returns if the ped is in cover stood on a vehicle
	bool GetNetIsStoodOnVehicle(const CPed& ped) const;

	// PURPOSE: Interface for getting/setting parent cover task flags
	void						SetCoverFlag(s32 iFlag) { m_CoverFlags.SetFlag(iFlag); }
	bool						IsCoverFlagSet(s32 iFlag) { return m_CoverFlags.IsFlagSet(iFlag); }
	void						ClearCoverFlag(s32 iFlag) { m_CoverFlags.ClearFlag(iFlag); }
	eForcedExit					GetForcedExit() const { return m_eForcedExit; }
	void						SetForcedExit(eForcedExit forcedExit) { m_eForcedExit = forcedExit; }

	// PURPOSE: Event interface
	void OnGunAimedAt(const CEventGunAimedAt& rEvent);

	s32						GetAllCoverFlags() const { return m_CoverFlags.GetAllFlags(); }

	void					SetIsMigrating(bool const isMigrating);

	// PURPOSE: Check if we should interrupt the cover because the cover is moving away
	bool CheckForMovingCover(CPed* pPed);

protected:

	////////////////////////////////////////////////////////////////////////////////
	
	CWeaponTarget&			GetTarget() { return m_CoverThreat; }
	const CWeaponTarget&	GetTarget() const { return m_CoverThreat; }
	void					UpdateTarget(const CEntity* pEntity, Vector3* pvPosition = NULL);

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Resume; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

#if FPS_MODE_SUPPORTED
	virtual void ProcessPreRender2();
#endif //FPS_MODE_SUPPORTED

	// PURPOSE: This function returns whether changes from the specified state
	// are managed by the clone FSM update, if not the state must be
	// kept in sync with the network update
	bool StateChangeHandledByCloneFSM(s32 iState);
private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return Start_OnUpdateClone();
	FSM_Return Resume_OnUpdate();
	FSM_Return ExitVehicle_OnEnter();
	FSM_Return ExitVehicle_OnUpdate();
	FSM_Return ExitVehicle_OnUpdateClone();
	FSM_Return SeekCover_OnEnter();
	FSM_Return SeekCover_OnUpdate();
	FSM_Return MoveToCover_OnEnter();
	FSM_Return MoveToCover_OnUpdate();
	FSM_Return MoveToCover_OnExit();
    FSM_Return MoveToCover_OnUpdateClone();
	FSM_Return MoveToSearchLocation_OnEnter();
	FSM_Return MoveToSearchLocation_OnUpdate();
	FSM_Return EnterCover_OnEnter();
	FSM_Return EnterCover_OnEnterClone();
	FSM_Return EnterCover_OnUpdate();
	FSM_Return EnterCover_OnUpdateClone();
	FSM_Return UseCover_OnEnter();
	FSM_Return UseCover_OnUpdate();
	FSM_Return UseCover_OnUpdateClone();
	FSM_Return UseCover_OnExit();
	FSM_Return ExitCover_OnEnter();
	FSM_Return ExitCover_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Get the cover position (where the peds mover should end up) and the vantage position from the peds cover point
	bool GetCoverPointPosition(const Vector3 &vThreatPos, CPed* pPed, Vector3& vCoverCoors, Vector3& vVantagePos);

	// PURPOSE: Checks if the nav task the ped is running has failed
	bool HasNavigationTaskFailed(CPed* pPed);

	// PURPOSE: Checks to see if the ped can play the cover entry clips
	bool CheckForEnteringCover(CPed* pPed);

	// PURPOSE: Determine which way to end up facing
	bool ComputeFacingDirection(const Vector3& vThreatPosition, bool& bFoundCloseCorner, bool bTestNormalIsSimilar);

	bool IsAtSearchPosition() const;

	// PURPOSE: Determine whether we should exit cover during cover entry
	bool CheckForCoverExit() const;

	// PURPOSE: Determine whether we should enter cover during cover exit
	bool CheckForCoverEntry();

	// PURPOSE: If allowed, this will choose which state we resume to after aborting
	int	ChooseStateToResumeTo() const;

	// PURPOSE: manage nav mesh blocking object
	void AddBlockingObject( CPed* pPed );
	void UpdateBlockingObject( CPed* pPed );
	void RemoveBlockingObject();

	// PURPOSE: To determine if the player is entering cover that is close to the cover we are moving to or entering
	bool IsLocalPlayerEnteringCloseCover() const;

	void ActivateTimeslicing();

	void SetWantedToReloadDuringEntry(bool bVal) { m_bWantedToReloadDuringEntry = bVal; }
	bool GetWantedToReloadDuringEntry() const { return m_bWantedToReloadDuringEntry; }
	void ProcessClearFPSCoverHelpText();

	float GetDesiredHeadingForPedWithCoverPoint(const CPed* pPed, const CCoverPoint* pCoverPoint) const;

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Position to search for cover
	Vector3 m_vSearchPosition;

	// PURPOSE: Cover coordinates
	Vector3 m_vCoverCoors;

	// Velocity tracking
	float m_fLastFramesCoverSpeed;

	// PURPOSE: to set cover search distances, MIN/Max
	float m_fSearchDistanceMin;
	float m_fSearchDistanceMax;

	// PURPOSE: Heading to end up in cover
	float m_fEndHeading;

	// PURPOSE: How fast should we move to cover
	float m_fMoveBlendRatio;

	// PURPOSE: How long to stay in cover
	float m_fTimeInCover;

	// PURPOSE: Duration to blend the idle state in if putting ped directly into cover
	float m_fBlendInDuration;

	float m_TimeBeforeDisplayHelpText;

	// PURPOSE: Timer to prevent TestSpheresIntersectingRoute from getting called each frame
	float m_fTestSpheresIntersectingRouteTimer;

	// PURPOSE: Flags to influence seeking cover
	fwFlags32 m_SeekCoverFlags;

	// PURPOSE: Flags to influence task behaviour
	fwFlags32 m_CoverFlags;

	// PURPOSE: Whether the cover should be any good?
	s32 m_iSearchType;

	// PURPOSE: Script can specify the cover to go into
	s32 m_iScriptedCoverIndex;

	// PURPOSE: Subtask task to be running when searching and/or moving (depending on flags)
	RegdaiTask m_pSubTaskWhilstSearching;

	// PURPOSE: Cover point filter construction callback
	CCover::CoverPointFilterConstructFn* m_pCoverPointFilterConstructFn;

	// PURPOSE: Spheres of influence builder
	CTaskNavBase::SpheresOfInfluenceBuilder* m_pSpheresOfInfluenceBuilder;

	// PURPOSE: To help this task find cover
	RegdCoverFinder	m_pCoverFinder;

	// PURPOSE: Cache whether we took cover on a vehicle door cover point
	RegdVeh m_pCoverVehicle;

	// PURPOSE: We need to cache the initial distance to our target when starting to move to cover
	float m_fInitialDistToTarget;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_TaskCoverMoveNetworkHelper;
	CMoveNetworkHelper m_TaskUseCoverMoveNetworkHelper;
	CMoveNetworkHelper m_TaskMotionInCoverMoveNetworkHelper;

	// PURPOSE: Entity/Position to take cover from
	CWeaponTarget m_CoverThreat;

	// PURPOSE: Wait until we've blended into locomotion 
	CTaskGameTimer m_EntryTimer;

	// PURPOSE: Store which foot we wish to start the entry anim on
	bool m_bRightFootPlant;
	bool m_bWantedToReloadDuringEntry;
	bool m_bHelpTextNeedsClearing;

	// PURPOSE: Store why we terminated the in cover task
	eInCoverTerminationReason m_eInCoverTerminationReason;

	// PURPOSE: Script can force exit from cover
	eForcedExit m_eForcedExit;

	eHierarchyId m_eCoverVehicleDoorId;

	//NavMesh blocking bound
	TDynamicObjectIndex m_iDynamicObjectIndex;
	u32 m_iDynamicObjectLastUpdateTime;

	// clone task members
	bool    m_bHasNetCoverPointInfo;
	bool    m_bHasVehicleStoodOn;
	Vector3 m_vNetCoverPointPosition;
	Vector3 m_vNetCoverPointDirection;
	u32		m_iNetCoverPointUsage;
	RegdConstVeh m_pVehicleStoodOn;


public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, COVER_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task info for being in cover
////////////////////////////////////////////////////////////////////////////////

class CClonedCoverInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedCoverInfo();
	CClonedCoverInfo(const CWeaponTarget* pTarget, const CPed* pPed, s32 coverState, u32 iFlags, bool IsLowCorner, u32 eTerminationReason, const CVehicle* pVehicleStoodOn);
	~CClonedCoverInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_COVER_CLONED_FSM;}

	inline bool				HasCoverPoint(void) const			{ return m_bHasCoverPoint; }
	inline bool				HasVehicleStoodOn() const			{ return m_bHasVehicleStoodOn; }
	inline const Vector3&	GetCoverPosition() const			{ return m_vCoverPosition; }
	inline const Vector3&	GetCoverDirection() const			{ return m_vCoverDirection; }
	inline s32				GetCoverUsage() const				{ return m_coverUsage; }
	inline u32				GetFlags() const					{ return m_coverFlags; }
	inline bool				GetIsLowCorner() const				{ return m_bIsLowCorner; }
	inline u32				GetTerminationReason() const		{ return m_eTerminationReason; }
	inline const CVehicle*  GetVehicleStoodOn() const			{ return m_VehicleStoodOn.GetVehicle(); }
	inline ObjectId			GetVehicleStoodOnObjId() const		{ return m_VehicleStoodOn.GetVehicleID(); }

	inline CClonedTaskInfoWeaponTargetHelper const &	GetWeaponTargetHelper() const	{ return m_weaponTargetHelper; }
	inline CClonedTaskInfoWeaponTargetHelper&			GetWeaponTargetHelper()			{ return m_weaponTargetHelper; }

	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTaskFSMClone *CreateLocalTask(fwEntity* pEntity);

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskCover::State_Finish>::COUNT;}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		m_weaponTargetHelper.Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_bHasCoverPoint);

		if (m_bHasCoverPoint || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_POSITION_SIZE(serialiser, m_vCoverPosition, "Cover Position", SIZEOF_COVER_POSITION);
			SERIALISE_VECTOR(serialiser, m_vCoverDirection, 1.0f, SIZEOF_COVER_DIRECTION, "Cover Direction");
			SERIALISE_UNSIGNED(serialiser, m_coverUsage, SIZEOF_COVER_USAGE, "Cover Usage");
			SERIALISE_BOOL(serialiser, m_bIsLowCorner, "Low Corner");
			SERIALISE_BOOL(serialiser, m_bHasVehicleStoodOn, "Has Vehicle Stood On");
			SERIALISE_UNSIGNED(serialiser, m_eTerminationReason, SIZEOF_COVER_TERMINATION_REASON, "Cover Exit Termination Reason");
			if (m_bHasVehicleStoodOn)
			{
				m_VehicleStoodOn.Serialise(serialiser);
			}
			else
			{
				m_VehicleStoodOn.Invalidate();
			}
		}
		else
		{
			m_vCoverPosition = VEC3_ZERO;
			m_vCoverDirection = VEC3_ZERO;
			m_coverUsage = 0;
			m_bIsLowCorner = false;
			m_eTerminationReason = CTaskCover::TR_IdleExit;
			m_VehicleStoodOn = NULL;
			m_bHasVehicleStoodOn = false;
		}

		SERIALISE_UNSIGNED(serialiser, m_coverFlags, SIZEOF_COVERFLAGS, "Cover Flags");
	}

private:

	static const unsigned SIZEOF_COVER_POSITION = 24;
	static const int SIZEOF_COVER_DIRECTION = 12;
	static const int SIZEOF_COVER_TERMINATION_REASON = 4;
	static const int SIZEOF_COVER_USAGE	= datBitsNeeded<CCoverPoint::COVUSE_NUM>::COUNT;
	static const unsigned int SIZEOF_COVERFLAGS = datBitsNeeded<CTaskCover::CF_LastSyncedFlag>::COUNT;

	CClonedTaskInfoWeaponTargetHelper m_weaponTargetHelper;

	bool			m_bHasCoverPoint;
	Vector3			m_vCoverPosition;
	Vector3			m_vCoverDirection;
	u32				m_coverUsage;
	u32				m_coverFlags;
	bool			m_bIsLowCorner;
	bool			m_bHasVehicleStoodOn;
	u32				m_eTerminationReason;
	CSyncedVehicle	m_VehicleStoodOn;
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls cover entry clips
////////////////////////////////////////////////////////////////////////////////

class CTaskEnterCover : public CTaskFSMClone
{
public:

	////////////////////////////////////////////////////////////////////////////////

	struct EnterClip
	{
		fwMvClipId	m_EnterClipId;
		s32			m_Flags;
		PAR_SIMPLE_PARSABLE;
	};

	////////////////////////////////////////////////////////////////////////////////

	struct StandingEnterClips
	{
		fwMvClipId	m_StandClip0Id;
		fwMvClipId	m_StandClip1Id;
		fwMvClipId	m_StandClip2Id;
		s32			m_Flags;
		PAR_SIMPLE_PARSABLE;
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_CoverEntryRatePlayer;
		float m_CoverEntryRateAI;
		float m_CoverEntryShortDistanceAI;
		float m_CoverEntryShortDistancePlayer;
		float m_CoverEntryStandDistance;
		float m_CoverEntryStandStrafeDistance;
		float m_CoverEntryMinDistance;
		float m_CoverEntryMaxDistance;
		float m_CoverEntryMinDistanceAI;
		float m_CoverEntryMaxDistanceAI;
		float m_CoverEntryMaxDirectDistance;
		s32 m_CoverEntryMinTimeNavigatingAI;
		float m_CoverEntryMinAngleToScale;
		float m_CoverEntryHeadingReachedTol;
		float m_CoverEntryPositionReachedTol;
		float m_FromCoverExitDistance;
		float m_DistFromCoverToAllowReloadCache;

		float m_NetworkBlendInDuration;

		bool m_EnableFootTagSyncing;
		bool m_ForceToTarget;
		bool m_EnableInitialHeadingBlend;
		bool m_EnableTranslationScaling;
		bool m_EnableRotationScaling;
		bool m_PreventTranslationOvershoot;
		bool m_PreventRotationOvershoot;
		bool m_DoInitialHeadingBlend;
		bool m_DoFinalHeadingFixUp;

		float m_MinDistToScale;
		float m_MaxSpeed;
		float m_MinTransScale;
		float m_MaxTransScale;
		float m_MinRotScale;
		float m_MaxRotScale;
		float m_DeltaTolerance;
		float m_MinRotDelta;
		float m_MaxAngleToSetDirectly;
		float m_MaxRotSpeed;
		float m_InCoverTolerance;
		float m_DotThresholdForCenterEnter;
		bool m_WaitForFootPlant;
		float m_MinDistToPlayEntryAnim;
		bool m_EnableNewAICoverEntry;
		bool m_EnableUseSwatClipSet;
		bool m_UseShortDistAngleRotation;
		bool m_DisableAiCoverEntryStreamCheck;
		float m_DistToUseShortestRotation;
		float m_AiEntryHalfAngleTolerance;
		float m_AiEntryMinRate;
		float m_AiEntryMaxRate;
		float m_PlayerSprintEntryRate;

		float m_DefaultPlayerStandEntryStartMovementPhase;
		float m_DefaultPlayerStandEntryEndMovementPhase;
		float m_MaxAngleToBeginRotationScale;
		float m_MaxDefaultAngularVelocity;
		float m_EnterCoverInterruptMinTime;
		float m_EnterCoverInterruptDistanceTolerance;
		float m_EnterCoverInterruptHeadingTolerance;
		float m_EnterCoverAimInterruptDistanceTolerance;
		float m_EnterCoverAimInterruptHeadingTolerance;

		atArray<EnterClip> m_AIEnterCoverClips;
		atArray<StandingEnterClips> m_AIStandEnterCoverClips;
		atArray<EnterClip> m_AIEnterTransitionClips;
		fwMvClipSetId	   m_EnterCoverAIAimingBase1H;
		fwMvClipSetId	   m_EnterCoverAIAimingBase2H;
		fwMvClipSetId	   m_EnterCoverAIAimingSwat1H;
		fwMvClipSetId	   m_EnterCoverAIAimingSwat2H;
		fwMvClipSetId	   m_EnterCoverAITransition1H;
		fwMvClipSetId	   m_EnterCoverAITransition2H;

		const atArray<CTaskCover::CoverAnimStateInfo>& GetSlidingEnterCoverAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetPlayerStandEnterCoverAnimStateInfoForPed(const CPed& rPed);

		struct AnimStateInfos
		{
			atArray<CTaskCover::CoverAnimStateInfo> m_SlidingEnterCoverAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_PlayerStandEnterCoverAnimStateInfos;

			PAR_SIMPLE_PARSABLE;
		};

		private:

		AnimStateInfos m_ThirdPersonAnimStateInfos;

#if FPS_MODE_SUPPORTED
		AnimStateInfos m_FirstPersonAnimStateInfos;
#endif // FPS_MODE_SUPPORTED

		PAR_PARSABLE;
	};

	// PURPOSE: Check if the ped is within the angle tolerance for AI moving entry anims
	static bool IsPedAngleToCoverAcceptableForMovingEntry(const CPed& ped);
	static bool ShouldPedDoStandingEntry(const CPed& ped);
	static bool GetFacingDirectionIfEdgeCover(const CCoverPoint& coverPoint, bool& bShouldFaceLeft);
	static bool GetFacingDirectionIfBlocked(const CPed& ped, const Vector3& vCoverPosition, const Vector3& vDir, bool& bShouldFaceLeft, bool bIsLowCover, bool bReturnFalseIfNotBlockedLaterally);
	static bool CheckIfHitVehicleCoverDoor(const phInst& rHitInst, s32 iHitComponentIndex);
	static bool CheckIfHitDoor(const phInst& rHitInst);

	// PURPOSE: Try to stream in ai entry clipsets before we will use them
	static void PrestreamAiCoverEntryAnims(const CPed& ped);

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvRequestId	ms_EnterCoverRequestId;
	static const fwMvBooleanId	ms_EnterCoverOnEnterId;
	static const fwMvBooleanId	ms_EnterCoverClipFinishedId;
	static const fwMvBooleanId	ms_EnterCoverTransitionClipFinishedId;
	static const fwMvBooleanId	ms_EnterCoverInterruptId;
	static const fwMvClipId		ms_EnterCoverClipId;
	static const fwMvClipId		ms_EnterCoverWeaponClipId;
	static const fwMvClipId		ms_EnterCoverWeaponClip0Id;
	static const fwMvClipId		ms_EnterCoverWeaponClip1Id;
	static const fwMvClipId		ms_EnterCoverWeaponClip2Id;
	static const fwMvClipId		ms_EnterCoverTransitionClipId;
	static const fwMvFloatId	ms_EnterCoverClipInitialPhaseId;
	static const fwMvFloatId	ms_EnterCoverClipCurrentPhaseId;
	static const fwMvFloatId	ms_EnterCoverClipRateId;
	static const fwMvFloatId	ms_AngleToCoverId;
	static const fwMvFlagId		ms_AIEntryFlagId;
	static const fwMvFlagId		ms_FromStandFlagId;
	static const fwMvRequestId	ms_AITransitionId;
	static const fwMvFloatId	ms_AIEntryRateId;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Makes sure the clipsets we need to enter cover are streamed in
	static bool AreCoreCoverClipsetsLoaded(CPed* pPed);
	static bool AreAICoverEntryClipsetsLoaded(const CPed& ped);
	static bool StreamClipset(const fwMvClipSetId& clipsetId);

	// PURPOSE: Choose the direction to end up at once the enter clip has finished
	static float ChooseEndDirectionForPed(CPed* pPed, Vector3& vCoverDir, const Vector3& vCoverCoords, bool& bOutFacingRight, bool& bFoundCloseCorner, bool bTestNormalIsSimilar);

	// PURPOSE: Compute the approach direction based on the peds current position
	static void ComputeApproachDirectionForPed(CPed* pPed, Vector3& vCoverDir, const Vector3& vCoverCoords, bool& bApproachFromRight);

	// PURPOSE: Compute the facing direction based on the peds current orientation
	static void ComputeCloseFacingDirectionForPed(CPed* pPed, const Vector3& vCoverDir, bool& bApproachFromRight);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Find the cover intro clip id matching the flags passed in
	static fwMvClipId FindAIIntroClipFromFlags(const atArray<EnterClip>& aEnterClips, s32 flags);
	static void FindStandIntroClipFromFlags(const atArray<StandingEnterClips>& aEnterClips, atArray<fwMvClipId>& aClipIdsOut, s32 flags);

	s32 ComputeEnterCoverDirection(Vec3V_ConstRef vPedPos, Vec3V_ConstRef vCoverPos, Vec3V_ConstRef vCoverDir);
	s32 ComputeStandingEnterCoverDirection(Vec3V_ConstRef vPedDir, Vec3V_ConstRef vCoverDir, float& fAngle, bool bUseFacingDirection = false);

	static fwMvClipSetId GetAiEntryClipsetFromWeapon(const CPed& ped);
	static fwMvClipSetId GetAiTransitionClipsetFromWeapon(const CPed& ped);

	static bool ShouldPedUseAiEntryAnims(const CPed& ped);

	// PURPOSE: Check if player can exit cover
	static bool CheckForPlayerExit(const CPed& ped, const Vector3& vCoverPosition, const Vector3& vCoverDirection);
	static const CPed* GetPedBlockingMyCover(const CPed& rPed, const Vector3& vPedCoverPos, bool bUsePedPosition = false);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define ENTER_COVER_STATES(X)	X(State_Start),					\
									X(State_StreamAssets),			\
									X(State_OrientateToCover),		\
									X(State_StartCoverNetwork),		\
									X(State_Sliding),				\
									X(State_StandEntry),			\
									X(State_MovingEntry),			\
									X(State_TransitionToIdle),		\
									X(State_Finish)

	// PURPOSE: FSM states
	enum eEnterCoverState
	{
		ENTER_COVER_STATES(DEFINE_TASK_ENUM)
	};

	////////////////////////////////////////////////////////////////////////////////

	CTaskEnterCover(const Vector3& vPosition, const float endDirection, const bool bCheckCoverDistance = true);
	CTaskEnterCover(); // used by network clones
	virtual ~CTaskEnterCover();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_ENTER_COVER; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskEnterCover(m_vTargetPos, m_fEndDirection, m_bCheckCoverDistance); }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return m_pMoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return m_pMoveNetworkHelper; }

	// PURPOSE: For clones, set whether we should use the fast rate
	void SetUseFastRate(bool bUseFastRate) { m_bUseFastRate = bUseFastRate; }

	// PURPOSE: Ensure move network and clipset are loaded
	bool StreamAssets();

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual bool				OverridesNetworkBlender(CPed*) { return true; }
	virtual bool				OverridesNetworkHeadingBlender(CPed*) {  return true;  }
	virtual bool				NetworkHeadingBlenderIsOverridden(CPed*) { return true; }
	Vector3						GetCoverPosition() const { return m_vTargetPos; }
	void						ForceCloneQuit() { m_bForceCloneQuit = true; }

	// PURPOSE: Returns true if the ped has a cover point, can be used by local or clone peds
	bool HasNetCoverPoint(const CPed& ped) const { return SafeCast(const CTaskCover, GetParent())->HasNetCoverPoint(ped); }
	// PURPOSE: Returns the cover point position, can be used by local or clone peds
	bool GetNetCoverPointPosition(const CPed& ped, Vector3& vCoverPointPos) const { return SafeCast(const CTaskCover, GetParent())->GetNetCoverPointPosition(ped, vCoverPointPos); }
	// PURPOSE: Returns the cover point direction, can be used by local or clone peds
	bool GetNetCoverPointDirection(const CPed& ped, Vector3& vCoverPointDirection) const { return SafeCast(const CTaskCover, GetParent())->GetNetCoverPointDirection(ped, vCoverPointDirection); }
	// PURPOSE: Returns the cover point usage, can be used by local or clone peds
	bool GetNetCoverPointUsage(const CPed& ped, CCoverPoint::eCoverUsage& usage) const { return SafeCast(const CTaskCover, GetParent())->GetNetCoverPointUsage(ped, usage); }
	// PURPOSE: Returns if the ped is in cover stood on a vehicle
	bool GetNetIsStoodOnVehicle(const CPed& ped) const { return SafeCast(const CTaskCover, GetParent())->GetNetIsStoodOnVehicle(ped); }

#if FPS_MODE_SUPPORTED
	float GetTimeSinceEnteringCameraLerpRange() const { return m_fTimeSinceEnteringCameraLerpRange; }
#endif // FPS_MODE_SUPPORTED

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Finish; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Called after the skeleton update but before the Ik (I think this is now before the skeleton update)
	virtual void ProcessPreRender2();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return	Start_OnUpdate();
	FSM_Return	StreamAssets_OnUpdate();
	FSM_Return	OrientateToCover_OnUpdate();
	FSM_Return	StartCoverNetwork_OnEnter();
	FSM_Return	StartCoverNetwork_OnUpdate();	
	FSM_Return	Sliding_OnEnter();
	FSM_Return	Sliding_OnUpdate();
	FSM_Return	StandEntry_OnEnter();
	FSM_Return	StandEntry_OnUpdate();
	FSM_Return	MovingEntry_OnEnter();
	FSM_Return	MovingEntry_OnUpdate();
	FSM_Return	TransitionToIdle_OnEnter();
	FSM_Return	TransitionToIdle_OnUpdate();
	FSM_Return	Finish_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Check the parent cover tasks flags
	const CWeaponTarget& GetTarget() const { return SafeCast(const CTaskCover, GetParent())->GetTarget(); }
	bool IsCoverFlagSet(u32 iFlag) const { taskAssertf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_COVER, "Expected parent cover task"); return GetParent() ? static_cast<const CTaskCover*>(GetParent())->IsCoverFlagSet(iFlag) : false; }

	// PURPOSE: Compute the initial phase to start the intro clip at based on the distance to cover and distance marker tags in the clips
	void ComputeInitialPhase(const crClip* pClip);

	// PURPOSE: After we have computed the initial phase we then search for the closest desired foot tag to that phase
	void RecomputeInitialPhaseFromFootPlant(const crClip* pClip);

	// PURPOSE: Find the beginning and end phases to fix up translation/rotation
	bool FindMoverFixUpPhases(const crClip* pClip);

	// PURPOSE: Compute a new desired velocity
	void ScaleTranslationalVelocity(float& fDesiredSpeed, const crClip* pClip, float fClipPhase, float fDistToCover);

	// PURPOSE: Compute a new desired angular velocity
	void ScaleRotationalVelocity(float& fDesiredAngSpeed, const crClip* pClip, float fClipPhase, const Vector3& vToTarget, float fTimeStep, bool bLeftApproach, float fAngToCover, bool bUsingShortestAngleRotation);

	// PURPOSE: Computes offsets to the target that, given the start phase of the clip, if we played the anim from that location, would nail the target location
	void ComputeIdealStartTransformRelativeToTarget(Vec3V_Ref vIdealStartPosOffset, QuatV_Ref qIdealStartRotOffset, const crClip* pClip, float fStartPhase);

	// PURPOSE: Transforms offsets by the target matrix and splats them
	void ComputeTransformFromOffsetsSplat(Vec3V_Ref vPosOffsetInGlobalOut, QuatV_Ref qRotOffsetInGlobalOut, Mat34V_ConstRef targetMtx);

	// PURPOSE: Compute where we'd like the ai cover entry anim to take us to
	Mat34V ComputeTargetMatrix(Vec3V_ConstRef vCoverPos, QuatV_Ref qTargetRot);

	// PURPOSE: Check if we should interrupt the cover entry if we're trying to do something and are in the correct position
	bool CheckForPlayerInterrupt() const;

	// PURPOSE: Check if we should interrupt the cover entry because we are lagging too far behind the main ped
	bool CheckForNetworkInterrupt() const;	
	
	// PURPOSE: Remove weapons if not usable in cover
	void CheckForWeaponRemoval(CPed* pPed, float fClipPhase);

	// PURPOSE: Prevents overshooting the cover point
	void ScaleDesiredVelocityToPreventOvershoot(const CPed& rPed, Vector3& vDesiredVelocity, const crClip* pClip, float fClipPhase, float fTimeStep);

	void ProcessCheckForWeaponReload();

	bool SetClipsFromAnimStateInfo(s32 flags, bool bWillSlide);

	////////////////////////////////////////////////////////////////////////////////

private:

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper* m_pMoveNetworkHelper;

	// PURPOSE: Stream and ref clipsets
	fwClipSetRequestHelper m_CoreStreamedClipsetRequestHelper;
	fwClipSetRequestHelper m_WeaponStreamedClipsetRequestHelper;

	// PURPOSE: Cover position
	Vector3	m_vTargetPos;

	// PURPOSE: Stores the peds initial position upon starting the cover enter
	Vector3 m_vInitialPosition;

	// PURPOSE: Start / target heading
	float m_fInitialHeading;
	float m_fInitialEndDirection;
	float m_fEndDirection;
	float m_fEnterDirection; // used in clone task
	float m_fTimeNotMovingToTarget;

	// PURPOSE: Phase to start the long clips at
	float m_fInitialPhase;

	// PURPOSE: Periods in the clip where we should apply extra velocity to make the ped nail the target
	float m_fTranslationFixUpStartPhase;
	float m_fTranslationFixUpEndPhase;
	float m_fRotationFixUpStartPhase;
	float m_fRotationFixUpEndPhase;

	// PURPOSE: Variables for ai entry anims
	float m_fEstBlendedInPhase;
	float m_fBlendedInPhase;
	Vec3V m_vInitialPos;
	Vec3V m_vIdealInitialPos;
	QuatV m_qInitialRot;
	QuatV m_qIdealInitialRot;

#if FPS_MODE_SUPPORTED
	float m_fTimeSinceEnteringCameraLerpRange;
#endif // FPS_MODE_SUPPORTED

	// PURPOSE: Should we check the distance to cover at the end of the task
	bool m_bCheckCoverDistance		: 1;
	bool m_bHasReachedHeading		: 1;
	bool m_bHasReachedPosition		: 1;
	bool m_bUseShortestRotationAngle : 1;
	bool m_bComputedIdealStartTransform : 1;
	bool m_bRightFootPlant : 1;
	bool m_bUseFastRate : 1;
	bool m_bStandingEntry : 1;
	bool m_bForceCloneQuit : 1;
	bool m_bApproachLeft : 1;
	bool m_bSetApproachDirection : 1;
	bool m_bShouldDoInitialHeadingLerp : 1;

public:

	////////////////////////////////////////////////////////////////////////////////

#if DEBUG_DRAW
	// Takes offsets relative to the parent matrix and renders in global space
	void RenderRelativeAxisInGlobalSpace(Vec3V_ConstRef vPosOffset, QuatV_ConstRef qRotOffset, Mat34V_ConstRef parentMtx);
#endif 

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, ENTER_COVER_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task info for the cover intro
////////////////////////////////////////////////////////////////////////////////

class CClonedCoverIntroInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedCoverIntroInfo() : m_fEndDir(0.0f), m_bUseFastRate(false)  {}
	CClonedCoverIntroInfo(const float endDirection, const bool bUseFastRate) 
		: m_fEndDir(endDirection), m_bUseFastRate(bUseFastRate) {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_COVER_INTRO_CLONED_FSM;}

	float	 GetEndDirection() const { return m_fEndDir;}
	bool	 GetUseFastRate() const { return m_bUseFastRate; }

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_fEndDir, TWO_PI, SIZEOF_ENDDIR, "End Direction");
		SERIALISE_BOOL(serialiser, m_bUseFastRate);
	}

private:

	static const unsigned int SIZEOF_ENDDIR = 10;

	float	m_fEndDir;			// the cover direction 
	bool	m_bUseFastRate;		// whether we should increase the rate of entry
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task to handle exiting cover gracefully
////////////////////////////////////////////////////////////////////////////////

class CTaskExitCover : public CTask
{
public:

	////////////////////////////////////////////////////////////////////////////////

	enum eExitClipType
	{
		ET_Idle,
		ET_Corner
	};

	////////////////////////////////////////////////////////////////////////////////

	struct ExitClip
	{
		fwMvClipId	m_ExitClipId;
		s32			m_Flags;
		PAR_SIMPLE_PARSABLE;
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();
		
		atArray<ExitClip> m_CornerExitClips;
	
		fwMvClipSetId	  GetExitCoverBaseClipSetIdForPed(const CPed& rPed);
		fwMvClipSetId	  GetExitCoverExtraClipSetIdForPed(const CPed& rPed);

		float			  m_MinInputToInterruptIdle;
		float			  m_CornerExitHeadingModifier;
		float			  m_ExitCornerZOffset;
		float			  m_ExitCornerYOffset;
		float			  m_ExitCornerDirOffset;

		const atArray<CTaskCover::CoverAnimStateInfo>& GetStandExitCoverAnimStateInfoForPed(const CPed& rPed);

		struct AnimStateInfos
		{
			atArray<CTaskCover::CoverAnimStateInfo> m_StandExitAnimStateInfos;

			PAR_SIMPLE_PARSABLE;
		};

	private:

		AnimStateInfos m_ThirdPersonAnimStateInfos;

#if FPS_MODE_SUPPORTED
		AnimStateInfos m_FirstPersonAnimStateInfos;
#endif // FPS_MODE_SUPPORTED

		fwMvClipSetId	  m_ExitCoverBaseClipSetId;
		fwMvClipSetId	  m_ExitCoverBaseFPSClipSetId;
		fwMvClipSetId	  m_ExitCoverExtraClipSetId;
		fwMvClipSetId	  m_ExitCoverExtraFPSClipSetId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvRequestId	ms_ExitCoverRequestId;
	static const fwMvBooleanId	ms_ExitCoverOnEnterId;
	static const fwMvBooleanId	ms_ExitCoverClipFinishedId;
	static const fwMvBooleanId	ms_CoverExitInterruptId;
	static const fwMvBooleanId	ms_CoverExitSprintInterruptId;
	static const fwMvBooleanId  ms_CoverExitPhaseOutId;
	static const fwMvBooleanId  ms_CoverBreakInterruptId;
	static const fwMvBooleanId  ms_CanAdjustHeadingInterruptId;
	static const fwMvClipId		ms_ExitCoverClipId;
	static const fwMvClipId		ms_ExitCoverWeaponClipId;
	static const fwMvClipId		ms_ExitCoverWeaponClip0Id;
	static const fwMvClipId		ms_ExitCoverWeaponClip1Id;
	static const fwMvClipId		ms_ExitCoverWeaponClip2Id;
	static const fwMvFlagId		ms_DirectionalFlagId;
	static const fwMvFloatId	ms_AngleToCoverId;
	
	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Can we do a corner exit?
	static bool CanPedUseCornerExit(const CPed& ped, bool bIsFacingLeft);

	// PURPOSE: Determine if this task is valid
	static bool IsTaskValid(const CPed& ped);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define EXIT_COVER_STATES(X)	X(State_Start),				\
									X(State_StreamAssets),		\
									X(State_IdleExit),			\
									X(State_CornerExit),		\
									X(State_Finish)

	// PURPOSE: FSM states
	enum eExitCoverState
	{
		EXIT_COVER_STATES(DEFINE_TASK_ENUM)
	};

	////////////////////////////////////////////////////////////////////////////////

	CTaskExitCover(CMoveNetworkHelper& coverMoveNetworkHelper, CMoveNetworkHelper& useCoverMoveNetworkHelper, CTaskCover::eInCoverTerminationReason eExitType);
	virtual ~CTaskExitCover();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_EXIT_COVER; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskExitCover(m_coverMoveNetworkHelper, m_useCoverMoveNetworkHelper, m_eExitType); }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_coverMoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_coverMoveNetworkHelper; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Finish; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: Called after the skeleton update but before the Ik (I think this is now before the skeleton update)
	virtual void ProcessPreRender2();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Find the cover exit clip id matching the flags passed in
	static fwMvClipId FindExitClipFromFlags(s32 flags);	

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return	Start_OnUpdate();
	FSM_Return	StreamAssets_OnUpdate();
	FSM_Return	IdleExit_OnEnter();
	FSM_Return	IdleExit_OnUpdate();
	FSM_Return	CornerExit_OnEnter();
	FSM_Return	CornerExit_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Check the parent cover tasks flags
	bool IsCoverFlagSet(u32 iFlag) const { taskAssertf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_COVER, "Expected parent cover task"); return GetParent() ? static_cast<const CTaskCover*>(GetParent())->IsCoverFlagSet(iFlag) : false; }
	s32 GetCoverFlags() const;
	
	// PURPOSE: Request exit cover and setup move params
	void SetUpCoverExit(CPed& ped, eExitClipType exitType);
	void UpdateExitDirection(CPed &ped);
	const fwMvClipId GetWeaponClipId(s32 i) const;	

	// PURPOSE: Get exit clipsets
	static fwMvClipSetId GetBaseClipSetId(eExitClipType exitType, const CPed& rPed);
	static fwMvClipSetId GetWeaponHoldingClipSetId(const CPed& ped, eExitClipType exitType);

	//Purpose: store tagged interrupt phases
	float	m_fCoverExitInterruptPhase;
	float	m_fCoverExitSprintInterruptPhase;
	float	m_fCoverExitPhaseOutPhase;	
	bool    m_bInterruptPhasesLoaded;

	float   m_fCoverExitDirection;
	float   m_fCoverExitInitialHeading;

	////////////////////////////////////////////////////////////////////////////////

private:

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper& m_coverMoveNetworkHelper;
	CMoveNetworkHelper& m_useCoverMoveNetworkHelper;

	// PURPOSE: Determines what type of exit cover we should do
	CTaskCover::eInCoverTerminationReason m_eExitType;

	// PURPOSE: Helpers to stream in exit clipsets
	fwClipSetRequestHelper m_BaseClipSetRequestHelper;
	fwClipSetRequestHelper m_WeaponClipSetRequestHelper;

public:

	////////////////////////////////////////////////////////////////////////////////


#if !__FINAL

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, EXIT_COVER_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls ped whilst in cover
////////////////////////////////////////////////////////////////////////////////

class CTaskInCover : public CTaskFSMClone
{
	friend class CCoverDebug;

public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define IN_COVER_STATES(X)	X(State_Start),						\
								X(State_StreamAssets),				\
								X(State_Idle),						\
								X(State_Peeking),					\
								X(State_PeekingToReload),			\
								X(State_Reloading),					\
								X(State_AimIntro),					\
								X(State_Aim),						\
								X(State_AimOutro),					\
								X(State_BlindFiring),				\
								X(State_SwapWeapon),				\
								X(State_ThrowingProjectile),		\
								X(State_UseMobile),					\
								X(State_Finish)

	// PURPOSE: FSM states
	enum eInCoverState
	{
		IN_COVER_STATES(DEFINE_TASK_ENUM)
	};

	////////////////////////////////////////////////////////////////////////////////

	// ClipIds are defined in the combat metadata, chosen based on the cover flags and passed into the move network 
	struct ThrowProjectileClip
	{
		fwMvClipId	m_IntroClipId;
		fwMvClipId	m_PullPinClipId;
		fwMvClipId	m_BaseClipId;
		fwMvClipId	m_ThrowLongClipId;
		fwMvClipId	m_ThrowShortClipId;
		fwMvClipId	m_ThrowLongFaceCoverClipId;
		fwMvClipId	m_ThrowShortFaceCoverClipId;
		s32			m_Flags;

		PAR_SIMPLE_PARSABLE;
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //	// NOTES:

		float m_MovementClipRate;
		float m_TurnClipRate;
		float m_ControlDebugXPos;
		float m_ControlDebugYPos;
		float m_ControlDebugRadius;

		float m_ControlDebugBeginAngle;
		float m_ControlDebugEndAngle;

		// ---	END_DEV_TO_REMOVE	--- //

		float m_MinStickInputToMoveInCover;
		float m_MinStickInputXAxisToTurnInCover;
		float m_MaxInputForIdleExit;
		float m_InputYAxisCornerExitValue;
		float m_InputYAxisQuitValueAimDirect;
		float m_InputYAxisQuitValue;
		float m_StartExtendedProbeTime;
		float m_MinTimeToSpendInTask;
		float m_DesiredDistanceToCover;
		float m_DesiredDistanceToCoverToRequestStep;
		float m_OptimumDistToRightCoverEdgeCrouched;
		float m_OptimumDistToLeftCoverEdgeCrouched;
		float m_OptimumDistToRightCoverEdge;
		float m_OptimumDistToLeftCoverEdge;
		float m_MinMovingProbeOffset;
		float m_MaxMovingProbeOffset;
		float m_MinTurnProbeOffset;
		float m_MaxTurnProbeOffset;
		float m_DefaultProbeOffset;
		float m_MinStoppingEdgeCheckProbeOffset;
		float m_MaxStoppingEdgeCheckProbeOffset;
		float m_MinStoppingProbeOffset;
		float m_MaxStoppingProbeOffset;
		float m_HeadingChangeRate;
		float m_MinTimeBeforeAllowingCornerMove;
		float m_CrouchedLeftFireOffset;
		float m_CrouchedRightFireOffset;
		float m_CoverLeftFireModifier;
		float m_CoverRightFireModifier;
		float m_CoverLeftFireModifierLow;
		float m_CoverRightFireModifierLow;
		float m_CoverLeftFireModifierCloseToEdge;
		float m_CoverRightFireModifierCloseToEdge;
		float m_CoverLeftIncreaseModifier;
		float m_CoverRightIncreaseModifier;
		float m_AimTurnCosAngleTolerance;
		float m_SteppingMovementSpeed;
		float m_InCoverMovementSpeedEnterCover;
		float m_InCoverMovementSpeed;
		bool  m_UseAutoPeekAimFromCoverControls;
		bool  m_ComeBackInWhenAimDirectChangeInHighCover;
		float m_AlternateControlStickInputThreshold;
		float m_EdgeCapsuleRadius;
		float m_EdgeStartXOffset;
		float m_EdgeEndXOffset;
		float m_EdgeStartYOffset;
		float m_EdgeEndYOffset;
		float m_InsideEdgeStartYOffset;
		float m_InsideEdgeEndYOffset;
		float m_InsideEdgeStartXOffset;
		float m_InsideEdgeEndXOffset;
		float m_WallTestYOffset;
		float m_InitialLowEdgeWallTestYOffset;
		float m_HighCloseEdgeWallTestYOffset;
		float m_WallTestStartXOffset;
		float m_WallTestEndXOffset;
		float m_WallHighTestZOffset;
		float m_EdgeHighZOffset;
		float m_MovingEdgeTestStartYOffset;
		float m_MovingEdgeTestEndYOffset;
		float m_CoverToCoverEdgeTestStartYOffset;
		float m_CoverToCoverEdgeTestEndYOffset;
		float m_SteppingEdgeTestStartYOffset;
		float m_SteppingEdgeTestEndYOffset;
		float m_InitialLowEdgeTestStartYOffset;
		float m_InitialLowEdgeTestEndYOffset;
		float m_EdgeLowZOffset;
		float m_EdgeMinimumOffsetDiff;
		float m_EdgeMaximumOffsetDiff;
		float m_PinnedDownPeekChance;
		float m_PinnedDownBlindFireChance;		
		float m_MinTimeBeforeAllowingAutoPeek;
		bool m_EnableAimDirectlyIntros;
		float m_BlindFireHighCoverMinPitchLimit;
		float m_BlindFireHighCoverMaxPitchLimit;
		float m_PedDirToPedCoverCosAngleTol;
		float m_CamToPedDirCosAngleTol;
		float m_CamToCoverDirCosAngleTol;
		float m_MinDistanceToTargetForPeek;
		float m_TimeBetweenPeeksWithoutLOS;
		s32 m_RecreateWeaponTime;
		atArray<ThrowProjectileClip>	m_ThrowProjectileClips;
		bool m_EnableLeftHandIkInCover;
		bool m_EnableReloadingWhilstMovingInCover;
		float m_AimIntroRateForAi;
		float m_AimOutroRateForAi;
		float m_MinReactToFireRate;
		float m_MaxReactToFireRate;
		float m_MaxReactToFireDelay;
		float m_MinTimeUntilReturnToIdleFromAimAfterAimedAt;
		float m_MaxTimeUntilReturnToIdleFromAimAfterAimedAt;
		float m_MinTimeUntilReturnToIdleFromAimDefault;
		float m_MaxTimeUntilReturnToIdleFromAimDefault;
		float m_GlobalLateralTorsoOffsetInLeftCover;
		float m_WeaponLongBlockingOffsetInLeftCover;
		float m_WeaponLongBlockingOffsetInLeftCoverFPS;
		float m_WeaponLongBlockingOffsetInHighLeftCover;
		float m_WeaponBlockingOffsetInLeftCover;
		float m_WeaponBlockingOffsetInLeftCoverFPS;
		float m_WeaponBlockingOffsetInRightCover;
		float m_WeaponBlockingOffsetInRightCoverFPS;
		float m_WeaponBlockingLengthOffset;
		float m_WeaponBlockingLengthOffsetFPS;
		float m_WeaponBlockingLengthLongWeaponOffset;
		float m_WeaponBlockingLengthLongWeaponOffsetFPS;
		fwMvClipSetId m_CoverStepClipSetId;
		float m_PinnedDownTakeCoverAmount;
		float m_AmountPinnedDownByDamage;
		float m_AmountPinnedDownByBullet;
		float m_AmountPinnedDownByWitnessKill;
		float m_PinnedDownByBulletRange;
		float m_PinnedDownDecreaseAmountPerSecond;
		float m_AimIntroTaskBlendOutDuration;
		u32 m_MinTimeToBePinnedDown;
		float m_TimeBetweenBurstsMaxRandomPercent;
		u32 m_AimOutroDelayTime;
		u32 m_AimOutroDelayTimeFPS;
		u32 m_AimOutroDelayTimeFPSScope;
		bool m_AimOutroDelayWhenPeekingOnly;
		bool m_EnableAimOutroDelay;

		fwMvClipSetId GetThrowProjectileClipSetIdForPed(const CPed& rPed, bool bForceThirdPerson = false) const;

		private:

		fwMvClipSetId m_ThrowProjectileClipSetId;
		fwMvClipSetId m_ThrowProjectileFPSClipSetId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used when hotswapping to players who are in cover
	static CCoverPoint			ms_aPlayerCoverPoints[3];
	static CCoverPoint&			GetStaticCoverPointForPlayer(CPed* pPed);
	static void					SlidePedIntoCover(CPed& rPed, CTaskInCover* pInCoverTask, const Vector3& vCoverCoords);

	static const fwMvFloatId	ms_PitchId;
	static const fwMvFlagId		ms_IsCrouchedFlagId;
	static const fwMvFilterId	ms_AimIntroFilterId;
	static atHashWithStringNotFinal ms_FullFilter;
	static atHashWithStringNotFinal ms_NoMoverFilter;

	static const fwMvFloatId	ms_Clip1BlendId;

	// ClipInput Signals
	static const fwMvClipId		ms_Clip0Id;
	static const fwMvClipId		ms_Clip1Id;
	static const fwMvClipId		ms_Clip2Id;
	static const fwMvClipId		ms_Clip3Id;

	static const fwMvClipId		ms_WeaponClip0Id;
	static const fwMvClipId		ms_WeaponClip1Id;
	static const fwMvClipId		ms_WeaponClip2Id;
	static const fwMvClipId		ms_WeaponClip3Id;
	static const fwMvClipId		ms_WeaponClip4Id;
	static const fwMvClipId		ms_WeaponClip5Id;

public:

	////////////////////////////////////////////////////////////////////////////////

	CTaskInCover(CMoveNetworkHelper& taskCoverMoveNetworkHelper, CMoveNetworkHelper& taskUseCoverMoveNetworkHelper, CMoveNetworkHelper& taskMotionInCoverMoveNetworkHelper );
	~CTaskInCover();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_IN_COVER; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskInCover(m_TaskCoverMoveNetworkHelper, m_TaskUseCoverMoveNetworkHelper, m_TaskMotionInCoverMoveNetworkHelper); }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_TaskUseCoverMoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_TaskUseCoverMoveNetworkHelper; }

	// PURPOSE: Interface Functions
	bool ComputeCoverAimPositionAndHeading(Vector3& vAimPos, float& fAimHeading, bool& bAimDirectly, bool bShouldConsiderTurningDirection = false, bool bReturnMoverPosition = false) const;

	void						SetTimeInCover(float fTimeInCover) { m_fTimeInCover = fTimeInCover; }
	CWeaponTarget&				GetTarget() { return SafeCast(CTaskCover, GetParent())->GetTarget(); }
	const CWeaponTarget&		GetTarget() const { return SafeCast(const CTaskCover, GetParent())->GetTarget(); }
	void						UpdateTarget(const CEntity* pEntity, Vector3* pvPosition = NULL) { return SafeCast(CTaskCover, GetParent())->UpdateTarget(pEntity, pvPosition); }
	void						SetCoverFlag(s32 iFlag) { SafeCast(CTaskCover, GetParent())->SetCoverFlag(iFlag); }
	void						ClearCoverFlag(s32 iFlag) { SafeCast(CTaskCover, GetParent())->ClearCoverFlag(iFlag); }
	inline bool					IsCoverFlagSet(s32 iFlag) const { return SafeCast(const CTaskCover, GetParent())->IsCoverFlagSet(iFlag); }
	s32							GetAllCoverFlags() const { return SafeCast(const CTaskCover, GetParent())->GetAllCoverFlags(); }
	static float				GetCoverOutModifier( const bool bLeft, const Vector3& coverDir, const Vector3& targetDir, float& fDot, bool bClamp = true, bool bLow = false, bool bCloseToEdge = false);
	bool						IsCoverPointTooHigh() const;
	bool						IsForwardAimBlocked() const { return m_bForwardAimBlocked; }
	bool						IsSideAimBlocked() const { return m_bSideAimBlocked; }
	bool						GetForcedBlockAimDirectly() const { return m_bForcedBlockAimDirectly; }
	bool						GetForcedStepBack() const { return m_bForcedStepBack; }
	bool						IsBlindFireWithAim() const { return m_bBlindFireWithAim; }	
	bool						GetSwitchedCoverDirection() const { return m_bSwitchedCoverDirection; }
	bool						GetWasScopedDuringAim() const { return m_WasScopedDuringAim; }
	void						CheckForBlockedAim();
	inline bool UseOverTheTopFiring() const
	{
		return IsCoverFlagSet(CTaskCover::CF_UseOverTheTopBlindFiring) || IsCoverFlagSet(CTaskCover::CF_UseOverTheTopBlindFiringDueToAngle);
	}
	void						RestorePreviousEquippedWeapon();

	// PURPOSE: Check any required assets, return true when they are streamed in
	static bool AreTaskAssetsLoaded(CPed& ped, bool bRequireWeaponAssets = true);

	void						RestartWeaponHolding();
	static bool					ComputeStickInputPolarForm(const Vector2& vStickInput, float& fMagnitude, float& fAngle);
	void						CacheStickAngle();

	// Used by ThirdPersonPedAimCamera.cpp to set camera offsets
	bool						IsFacingLeft() const { return IsCoverFlagSet(CTaskCover::CF_FacingLeft); }
	bool						GetCanFireRoundCorner(bool bShouldConsiderTurningDirection = false) const;
	static bool					CanPedFireRoundCorner(const CPed& ped, bool bFacingLeft);
	static bool					CanPedFireRoundCorner(const CPed& ped, bool bFacingLeft, bool bIsLowCover, CCoverPoint::eCoverUsage coverUsage);
	static fwMvClipId			GetClipIdForIndex(s32 j);
	static fwMvClipId			GetWeaponClipIdForIndex(s32 j);
	static inline bool			IsClassifiedAsIdle( s32 state )
	{
								return state == State_Idle || state == State_Peeking;
	}

	bool						CheckForAimOutroInterrupt() const;
	CMoveNetworkHelper*			GetMovementNetworkHelper() { return &m_TaskMotionInCoverMoveNetworkHelper; }
	static fwMvClipSetId		GetClipSetHashForArmament(const CPed* pPed);
	bool						CalculateCoverPosition();

	// PURPOSE: Return whether the ped running this task could throw a projectile if requested
	bool						CouldThrowProjectile(bool bIsPlayer);

	// PURPOSE: Provide interface for smoke grenade throw request
	void RequestThrowSmokeGrenade() { m_bSmokeGrenadeThrowRequested = true; }

	// PURPOSE: Search for the edge of cover so we can update the cover to the optimal distance away from the ped
	static void FindCoverEdgeAndUpdateCoverPosition(CPed* pPed, Vector3& vCoverCoords, float& fEdgeDistance, bool bPedFacingLeft, bool bEdgeOnly = false, float fDistTol = -1.0f, bool bFromEntry = false);
	void FindCoverEdgeAndUpdateCoverPosition();
	static bool ShouldDoLowTest(CCoverPoint::eCoverHeight coverHeight, CCoverPoint::eCoverUsage coverUsage);
	static bool ShouldAimDirectly(const CPed& ped, bool bIsHighCover, bool& bForcedBlockAimDirectly, float fTolerance=0.0f, bool bLastResult=false, bool bIsBlindFire = false);

	bool GetStoodUpToFire() const { return m_bStoodUpToFire; }
	const Vector3& GetCoverCoords() const { return m_vCoverCoords; }	
	const Vector3& GetCoverDirection() const { return m_vCoverDirection; }
	const Vector2& GetStickInput() const { return m_vecStick; }
	void SetStickHeadingLock(bool bLock) {m_bStickHeadingLockEnabled = bLock;}	
	static const Vector3& GetCameraHeadingVec();

	void SetAimIntroFilter(const atHashWithStringNotFinal& newFilter);

	bool IsAimButtonHeldDown();

#if FPS_MODE_SUPPORTED
	float GetPeekHeightOffsetFromMover() const { return m_fPeekHeightOffsetFromMover; }
#endif // FPS_MODE_SUPPORTED

	fwMvClipId					GetCoverStepClipId() const { return m_CoverStepClipId; }
	bool						GetWantsToTriggerStep() const { return m_bWantsToTriggerStep; }
	void						SetWantsToTriggerStep(bool bWantsToTriggerStep) { m_bWantsToTriggerStep = bWantsToTriggerStep; }
	s32							GetCurrentCoverStepNodeIndex() const { return m_iCurrentCoverStepNodeIndex; }
	void						SetCurrentCoverStepNodeIndex(s32 iCoverStepNodeIndex) { m_iCurrentCoverStepNodeIndex = iCoverStepNodeIndex; }
	Vector3						GetInitialStepPosition() const { return m_vInitialPosition; }
	float						GetStepClipExtraTranslation() const { return m_fExtraTranslation; }
	void						SetStepClipExtraTranslation(float fExtraTranslation) { m_fExtraTranslation = fExtraTranslation; }
	void						SetSteppedOutToAim(bool bSteppedOutToAim) { m_bSteppedOutToAim = bSteppedOutToAim; }
	bool						GetSteppedOutToAim() const { return m_bSteppedOutToAim; }

	static bool IsClipSetStreamedIn(const fwMvClipSetId& clipsetId);

	static bool ShouldBeginFacingLeft(const Vector3& vTargetPos, const CPed& ped);

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	bool						ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual bool				OverridesNetworkBlender(CPed* pPed);
	virtual bool				OverridesNetworkHeadingBlender(CPed* pPed) { return OverridesNetworkBlender(pPed); }

	// PURPOSE: Called after the skeleton update but before the Ik (I think this is now before the skeleton update)
	virtual void ProcessPreRender2();

	static float GetXYDot(const Vector3& v1, const Vector3& v2)
	{
		const Vector2 vec1(v1.x,v1.y);
		const Vector2 vec2(v2.x,v2.y);
		return vec1.Dot(vec2);
	}

	//! Allows us to downgrade targeting priority.
	bool IsInDangerousTargetingState() const;

	// PURPOSE: Event interface
	void OnGunAimedAt(const CEventGunAimedAt& rEvent);
	void OnCallForCover(const CEventCallForCover& rEvent);
	void OnBulletEvent();

	// PURPOSE: Compute the cos(angle) between the cover right and the aim vector
	static float ComputeAimModifier(const CPed& ped, Vector3& vTargetDir, float& fTargetHeading, Vector3& vCoverCoords, Vector3& vCoverDir);

	static void ComputeTargetIntroPositionAndHeading(const CPed& ped, bool bIsFacingLeft, bool bIsLowCover, Vector3& vTargetPosition, float& fTargetHeading, bool bInHighCoverNotAtEdge, bool bHighCloseToEdge, bool bForceShortStep = false);

	static s32 ComputeDesiredArc(const CPed& ped, bool bIsFacingLeft, s32 iCurrentIndex = -1);

	static bool DetermineIfDesiredNodeIsNextNode(bool bIsFacingLeft, s32 iCurrentNode, s32 iDesiredNode);

	static bool	GetThrownProjectileClipSetAndClipIds(CPed* pPed, CTaskInCover* pInCoverTask, fwMvClipSetId& clipSetId, fwMvClipId& introClipId, fwMvClipId& baseClipId, fwMvClipId& throwLongClipId, fwMvClipId& throwShortClipId, fwMvClipId& throwLongFaceCoverClipId, fwMvClipId& throwShortFaceCoverClipId, bool bForceEdge = false);

	static bool CanBlindFireCorner(const CPed* pPed);

	static bool CheckForBlindFireOverheadClearance(CPed* pPed, u8& uBlindFireBreakGlass);

	// PURPOSE: Returns true if the ped has a cover point, can be used by local or clone peds
	bool HasNetCoverPoint(const CPed& ped) const { return SafeCast(const CTaskCover, GetParent())->HasNetCoverPoint(ped); }
	// PURPOSE: Returns the cover point position, can be used by local or clone peds
	bool GetNetCoverPointPosition(const CPed& ped, Vector3& vCoverPointPos) const { return SafeCast(const CTaskCover, GetParent())->GetNetCoverPointPosition(ped, vCoverPointPos); }
	// PURPOSE: Returns the cover point direction, can be used by local or clone peds
	bool GetNetCoverPointDirection(const CPed& ped, Vector3& vCoverPointDirection) const { return SafeCast(const CTaskCover, GetParent())->GetNetCoverPointDirection(ped, vCoverPointDirection); }
	// PURPOSE: Returns the cover point usage, can be used by local or clone peds
	bool GetNetCoverPointUsage(const CPed& ped, CCoverPoint::eCoverUsage& usage) const { return SafeCast(const CTaskCover, GetParent())->GetNetCoverPointUsage(ped, usage); }

	bool IsDelayedExit() const { return m_bDelayedExit; }

	bool IsInMovingMotionState(bool bReturnTrueIfNoSubtask = true) const;

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Finish; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Update called every frame after RequestProcessMoveSignalCalls is called.
	// RETURNS: Whether the ProcessMoveSignals method should continue to be called, otherwise cease.
	virtual bool ProcessMoveSignals();

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Called after the camera system update
	virtual bool ProcessPostCamera();

	// PURPOSE: Will continually update the player desired target
	void UpdateTarget(CPed* pPed);

	// PURPOSE:	Tell the ped LOD system that this task should be able to handle timeslicing.
	void ActivateTimeslicing();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return					Start_OnEnter();
	FSM_Return					Start_OnUpdate();
	FSM_Return					Start_OnUpdateClone();
	FSM_Return					StreamAssets_OnUpdate();
	FSM_Return					StreamAssets_OnUpdateClone();
	void						Idle_OnEnter();
	FSM_Return					Idle_OnUpdate();
	FSM_Return					Idle_OnUpdateClone();
	void						Idle_OnExit();
	void						Peeking_OnEnter();
	FSM_Return					Peeking_OnUpdate();
	void						Peeking_OnExit();
	FSM_Return					Peeking_OnUpdateClone();
	void						PeekingToReload_OnEnter();
	FSM_Return					PeekingToReload_OnUpdate();
	void						BlindFiring_OnEnter();
	FSM_Return					BlindFiring_OnUpdate();
	FSM_Return					BlindFiring_OnExit();
	FSM_Return					BlindFiring_OnUpdateClone();
	FSM_Return					AimIntro_OnEnter();
	FSM_Return					AimIntro_OnUpdate();
	FSM_Return					AimIntro_OnExit();
	FSM_Return					Aim_OnEnter();
	FSM_Return					Aim_OnUpdate();
	FSM_Return					Aim_OnUpdateClone();
	FSM_Return					Aim_OnExit();
	FSM_Return					AimOutro_OnEnter();
	FSM_Return					AimOutro_OnUpdate();
	FSM_Return					AimOutro_OnExit();
	void						ThrowingProjectile_OnEnter();
	FSM_Return					ThrowingProjectile_OnUpdate();
	FSM_Return					ThrowingProjectile_OnUpdateClone();
	void						ThrowingProjectile_OnExit();
	void						Reloading_OnEnter();
	FSM_Return					Reloading_OnUpdate();
	FSM_Return					Reloading_OnUpdateClone();
	void						Reloading_OnExit();
	FSM_Return					SwapWeapon_OnEnter();
	FSM_Return					SwapWeapon_OnUpdate();
	FSM_Return					SwapWeapon_OnUpdateClone();
	FSM_Return					SwapWeapon_OnExit();
	FSM_Return					UseMobile_OnEnter();
	FSM_Return					UseMobile_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	// Internal Helper Functions
	bool IsInMotionState(s32 iState, bool bReturnTrueIfNoSubtask = true) const;
	bool WasInMotionState(s32 iState) const;
	bool RequestStepping();
	bool IsAtOutsideEdge() const;
#if FPS_MODE_SUPPORTED
	bool ForceAimDueToDirectAimInFPM(bool bShouldAimDirectly) const;
	void ComputePeekHeightOffsetFromMover();
	void ProcessPelvisOffset();
#endif // FPS_MODE_SUPPORTED
	void ProcessCoverSwitchDirection(const CPed& ped, float fTolerance = 0.0f, bool bCheckAimDirectly = false, bool bCheckAtCorner = false);

	// Checks for state changes
	bool						CheckForExit();
	bool						CheckForPeeking();
	bool						CheckForThrowingProjectile();
	bool						CheckForSwitchingWeapon();
	bool						CheckForMovingOutToFire(s32& newState);
	bool						CheckForReload(bool bIgnoreMotionChecks = false);
	bool						CheckForDetonation();
	bool						CheckDesiredPinnedDownState(s32& newState);
	bool						IsCoverValid() const;
	void						ProcessReloadSecondaryTask();
	void						ProcessWeaponSwapSecondaryTask();
	void						ProcessCoverStep(CTaskMotionAiming* pMotionAimingTask, CPed* pPed);

	// Utility funcs
	bool						IsPedsVehicleCoverOnFire(const CPed& rPed) const;
	CTaskGun*					CreateBlindFireGunTask();
	void						ProcessMovement();
	bool ShouldStepBack(const CObject* pWeaponObject, const CWeapon* pWeapon) const;
	float						GetEdgeOffsetFromMotionTask() const;
	bool						CheckBlindFireRange(const CPed* pPed);
	void						ProcessBlindFireBreakGlass(CPed* pPed);

	//  To be called within ProcessPhysics
	void						SlidePedAgainstCover();
	void						FacePedInCorrectDirection(float fTimeStep);

	// Other
	void						CalculateInitialHeadingDir(bool bAutoFaceCornerDir = true);
	void						StoreCoverInformation();
	void						SetCorrectCrouchStatus();
	s32							CalculateDesiredDirection(const Vector3& vTargetPos);
	aiTask*						CreateAimTask();
	aiTask*						CreateFireTask();
	void						SetStateThrowingProjectile(const float fCookTime);

	void						UpdatePlayerInput(CPed *pPlayerPed);
	void						UpdateCornerMoveStatus();
	
	void						CheckForOverTheTopFiring();
	void						CheckForBlockedBlindFire();	
	void						UpdatePlayerTargetting();
	bool						IsValidForFPSIk(const CPed& rPed);

	// clone task functions
	void						SetNextStateFromNetwork();	
	void						SetState(s32 iState);

	void						SetTerminationReason(CTaskCover::eInCoverTerminationReason reason);
	
	float						CalculateRateForAimIntro() const;
	float						CalculateRateForAimOutro(bool bToPeek) const;

	u32							CalculateTimeNextPeekAllowed() const;
	void						ResetTimeBetweenBurstsRandomness();

	void						ProcessAudio();
	bool						AreWeaponClipsetsLoadedForPed(const CPed& rPed) const;
	bool						IsFPSPeekPositionObstructed(const CPed& rPed, bool bFacingLeft) const;

	enum eEdgeTestType
	{
		eIdleEdgeTest,
		eCoverToCoverEdgeTest,
		eMovingEdgeTest,
		eSteppingEdgeTest,
		eLowInitialEdgeTest,
		eHighCloseToEdgeTest
	};

	// PURPOSE: Do a capsule test to find a cover edge
	static bool FindWallIntersection(const CPed& rPed, const Vector3& vTestPos, const Vector3& vCoverDir, const Vector3& vPedCoverForward, Vector3& vIntersectionPos, eEdgeTestType testType, const CVehicle* pVehicle, bool bLowCover);
	static bool FindInsideEdgeIntersection(const CPed& rPed, const Vector3& vTestPos, const Vector3& vCoverDir, const Vector3& vPedCoverForward, Vector3& vIntersectionPos, eEdgeTestType testType, const CVehicle* pVehicle, bool bIsLeft);
	static bool FindEdgeIntersection(const CPed& rPed, const Vector3& vTestPos, const Vector3& vCoverDir, const Vector3& vPedCoverForward, Vector3& vIntersectionPos, eEdgeTestType testType, const CVehicle* pVehicle, bool bIsLeft);

	static float GetEdgeYStartOffset(eEdgeTestType testType);
	static float GetEdgeYEndOffset(eEdgeTestType testType);
	static float GetEdgeXStartOffset(eEdgeTestType testType);
	static float GetEdgeXEndOffset(eEdgeTestType testType);	

	static float GetInsideEdgeYStartOffset(eEdgeTestType testType);
	static float GetInsideEdgeYEndOffset(eEdgeTestType testType);
	static float GetInsideEdgeXStartOffset(eEdgeTestType testType);
	static float GetInsideEdgeXEndOffset(eEdgeTestType testType);	

	static float ComputeStepTransitionMinAngle(const CPed& ped, const int index, const bool bFacingLeft, const float fDesiredAngle);
private:

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper&	 m_TaskCoverMoveNetworkHelper;
	CMoveNetworkHelper&	 m_TaskUseCoverMoveNetworkHelper;
	CMoveNetworkHelper&	 m_TaskMotionInCoverMoveNetworkHelper;

	// PURPOSE: Time to stay in cover
	float m_fTimeInCover;

	float				m_fTimeSinceLastCrouchChange;
	CAsyncProbeHelper	m_probeTestHelper;
	Vector3				m_vCoverOffset;
	float				m_fTimeSinceLastDynamicCoverCheck;

	//
	bool				m_bStickHeadingLockEnabled;
	float				m_fLockedStickHeading;
	Vector2				m_vecStick;


	Vector3				m_vCoverDirection;
	Vector3				m_vCoverCoords;	

	// a variable storing the time we are allowed to peek next
	u32					m_iTimeNextPeekAllowed;
	u32					m_uPreviousWeaponHash;

	// Anim phases for early outs, maybe better to check for these when needed, rather than caching initially
	float				m_fProjectileCookingTime;
	float				m_fTimeToSpendInState;
	float				m_fCachedStickAngle;
	bool				m_bWaitingForInitialState;
	bool				m_bCachedStickValid;

	// external request to throw a smoke grenade
	bool				m_bSmokeGrenadeThrowRequested;
	bool				m_bSmokeGrenadeThrowInProgress;

	// bool that keeps track if we went to fire while already pinned down
	bool				m_bIsFiringAndPinnedDown;
	bool				m_bOutroToReload;
	bool				m_bReactToFire;
	bool				m_bWantsToExit;
	bool				m_bForceFireBullet;
	bool 				m_bForwardAimBlocked;
	bool 				m_bSideAimBlocked;
	bool				m_bForcedBlockAimDirectly;
	bool				m_bStoodUpToFire;
	bool				m_bLetGoOfAimAfterStartingReload;
	bool				m_bAimWithoutAmmo;
	bool				m_bDelayedExit;
	bool				m_bForcedStepBack;
	bool				m_bBlindFireWithAim;
	bool				m_bSwitchedCoverDirection;
	bool				m_bSteppedOutToAim;
	bool				m_bDelayedBlindFireExit;
	CTaskGameTimer		m_RecreateWeaponTimer;

	//Smooth FacePedInCorrectDirection
	float				m_fLastHeadingChangeRate;
	float				m_fLastAngleDelta;
	float				m_fNoTaskBlendDuration;

	//Timers
	float				m_fTimeUntilReturnToIdleFromAimedAt;

	fwMvFilterId				m_AimIntroFilterId;
	crFrameFilterMultiWeight*	m_pAimIntroFilter;

	bool m_bWantsToTriggerStep;
	bool m_bWantsToReload;
	u32 m_uWeaponHash;
	bool m_bLeftHandIkActive;
	fwMvClipId m_CoverStepClipId;
	s32 m_iPreviousCoverStepNodeIndex;
	s32 m_iCurrentCoverStepNodeIndex;
	Vector3 m_vInitialPosition;
	Vector3 m_vClipTranslation;
	float m_fExtraTranslation;
	float m_fTimeBetweenBurstsRandomness;
#if FPS_MODE_SUPPORTED
	float m_fPeekHeightOffsetFromMover;
	float m_fPelvisOffset;
	bool m_bPeekToggleEnabled;
	bool m_bNeedToPopHeading;
	bool m_WasScopedDuringAim;
	CFirstPersonIkHelper*	m_pFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

	//if blind fire state needs to smash glass, here is where it is
	enum {
		BF_WINDOWBREAK_NONE = 0,
		BF_WINDOWBREAK_SEARCHING,
		BF_WINDOWBREAK_BREAKING
	};
	u8 m_uBlindFireBreakGlass;

	u32 m_uReleasedAimControlsTime;

	u8  m_ShouldReloadWeaponDelay;

	// Heights
	enum 
	{ 
		CH_Low = 0, 
		CH_High, 
		CH_Count 
	} ;

	// Direction
	enum 
	{ 
		CD_Left = 0, 
		CD_Right, 
		CD_Count 
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvNetworkId ms_ReloadNetworkId;
	static const fwMvNetworkId ms_WeaponSwapNetworkId;

	static const fwMvRequestId ms_AimIntroRequestId;
	static const fwMvRequestId ms_AimOutroRequestId;
	static const fwMvRequestId ms_BlindFiringRequestId;
	static const fwMvRequestId ms_ReloadingRequestId;
	static const fwMvRequestId ms_SwapWeaponRequestId;
	
	static const fwMvBooleanId ms_AimIntroOnEnterId;
	static const fwMvBooleanId ms_AimOutroOnEnterId;
	static const fwMvBooleanId ms_BlindFiringOnEnterId;
	static const fwMvBooleanId ms_ReloadOnEnterId;
	static const fwMvBooleanId ms_WeaponSwapOnEnterId;
	
	static const fwMvFloatId ms_MoveBackFromFiringRateId;
	static const fwMvFloatId ms_MoveOutToFireRateId;
	static const fwMvFloatId ms_IntroPhaseId;
	static const fwMvFloatId ms_AimIntroRateId;
	static const fwMvFloatId ms_AimOutroRateId;

	public:

		////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
		// PURPOSE: Display debug information specific to this task
		void Debug() const;

		void DrawDebug() const;

		// PURPOSE: Get the name of the specified state
		DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, IN_COVER_STATES)

#endif // !__FINAL

		////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task info for being in cover
////////////////////////////////////////////////////////////////////////////////

class CClonedUseCoverInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedUseCoverInfo() {}
	CClonedUseCoverInfo(s32 coverState, bool needToReload, u32 uWeaponHash, bool bForcedStepBack) { SetStatusFromMainTaskState(coverState); m_needToReload = needToReload; m_uWeaponHash = uWeaponHash; m_bForcedStepBack = bForcedStepBack; }
	~CClonedUseCoverInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_USE_COVER_CLONED_FSM;}

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskInCover::State_Finish>::COUNT;}
	virtual const char*	GetStatusName() const { return "State"; }

	bool GetNeedsToReload() const { return m_needToReload; }
	bool GetForcedStepBack() const { return m_bForcedStepBack; }
	u32 GetWeaponHashUsed() const { return m_uWeaponHash; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_WEAPONHASH = 32;

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_needToReload, "Need To Reload");
		SERIALISE_BOOL(serialiser, m_bForcedStepBack, "Forced Step Back");
		SERIALISE_UNSIGNED(serialiser, m_uWeaponHash, SIZEOF_WEAPONHASH, "Weapon Hash Used");
	}

private:

	bool m_needToReload;
	bool m_bForcedStepBack;
	u32  m_uWeaponHash; 
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls motion clips whilst in cover
////////////////////////////////////////////////////////////////////////////////

class CTaskMotionInCover : public CTaskFSMClone
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define MOTION_IN_COVER_STATES(X)	X(State_Start),						\
										X(State_StreamAssets),				\
										X(State_Idle),						\
										X(State_Stepping),					\
										X(State_WalkStart),					\
										X(State_Settle),					\
										X(State_PlayingIdleVariation),		\
										X(State_Stopping),					\
										X(State_StoppingAtEdge),			\
										X(State_AtEdge),					\
										X(State_Turning),					\
										X(State_TurnEnter),					\
										X(State_TurnEnd),					\
										X(State_Moving),					\
										X(State_StoppingAtInsideEdge),		\
										X(State_EdgeTurn),					\
										X(State_InsideCornerTransition),	\
										X(State_CoverToCover),				\
										X(State_AtEdgeLowCover),			\
										X(State_Peeking),					\
										X(State_PinnedIntro),				\
										X(State_Pinned),					\
										X(State_PinnedOutro),				\
										X(State_Finish)

	// PURPOSE: FSM states
	enum eMotionInCoverState
	{
		MOTION_IN_COVER_STATES(DEFINE_TASK_ENUM)
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //	// NOTES:
	
		float m_CoverToCoverClipRate;
		bool m_UseButtonToMoveAroundCorner;
		bool m_DisableCoverToCoverTranslationScaling;
		bool m_DisableCoverToCoverRotationScaling;

		bool m_EnableCoverPeekingVariations;
		bool m_EnableCoverPinnedVariations;
		bool m_EnableCoverIdleVariations;
		
		float m_CoverToCoverDuration;

		float m_CoverToCoverMinScalePhase;
		float m_CoverToCoverMaxScalePhase;

		float m_CoverToCoverMinRotScalePhase;
		float m_CoverToCoverMaxRotScalePhase;

		// ---	END_DEV_TO_REMOVE	--- //

		bool  m_EnableWalkStops;
		bool  m_EnableCoverToCover;
		bool  m_UseSprintButtonForCoverToCover;

		u8  m_VerifyCoverInterval;
		u32 m_MinTimeForCornerMove;

		float m_DefaultSettleBlendDuration;
		float m_HeightChangeSettleBlendDuration;
		float m_MaxRotationalSpeedScale;
		float m_MaxRotationalSpeed;
		float m_MinStickInputToEnableMoveAroundCorner;
		float m_MinStickInputToEnableCoverToCover;
		float m_MinStickInputToMoveAroundCorner;
		float m_MaxStoppingDuration;
		float m_MinStoppingDist;
		float m_MinTimeToScale;
		float m_CTCDepthDistanceCompletionOffset;
		float m_EdgeLowCoverMoveTime;
		float m_MinTimeToStandUp;
		float m_CoverToCoverMinDistToScale;
		float m_CoverToCoverMinAngToScale;
		float m_CoverToCoverMinAng;
		float m_CoverToCoverDistTol;
		float m_CoverToCoverMaxDistToStep;
		float m_CoverToCoverAngTol;
		float m_CoverToCoverMaxAngToStep;
		float m_CoverToCoverMaxAccel;
		float m_ForwardDistToStartSideScale;
		float m_CoverToCoverMinDepthToScale;
		float m_CoverToCoverSmallAnimDist;
		float m_HeadingReachedTolerance;
		float m_BlendToIdleTime;
		float m_InsideCornerStopDistance;
		float m_MinTimeStayPinned;
		float m_MaxTimeStayPinned;
		u32	  m_PinnedDownThreshold;
		bool  m_ForcePinnedDown;
		float m_MinDistanceToTargetForIdleVariations;
		float m_MinTimeBetweenIdleVariations;
		float m_MaxTimeBetweenIdleVariations;
		float m_MinWaitTimeToPlayPlayerIdleVariations;
		float m_MinTimeBetweenPlayerIdleVariations;
		float m_MaxTimeBetweenPlayerIdleVariations;
		float m_CoverHeadingCloseEnough;
		float m_CoverHeadingCloseEnoughTurn;
		float m_CoverPositionCloseEnough;
		float m_DefaultStillToTurnBlendDuration;
		float m_DefaultEdgeTurnBlendDuration;
		float m_PeekToEdgeTurnBlendDuration;
		float m_MaxMoveSpeedInCover;
		float m_MinEdgeDistanceForStoppingAnim;
		bool  m_UseNewStepAndWalkStarts;
		bool  m_UseNewTurns;
		bool  m_UseNewTurnWalkStarts;

		fwMvClipSetId m_CoreAIMotionClipSetId;

		// TODO: Could structure this so its even more data-driven
		atArray<fwMvClipSetId> m_PeekingLow1HVariationClipsets;
		atArray<fwMvClipSetId> m_PeekingLow2HVariationClipsets;
		atArray<fwMvClipSetId> m_PeekingHigh1HVariationClipsets;
		atArray<fwMvClipSetId> m_PeekingHigh2HVariationClipsets;

		atArray<fwMvClipSetId> m_PinnedLow1HVariationClipsets;
		atArray<fwMvClipSetId> m_PinnedLow2HVariationClipsets;
		atArray<fwMvClipSetId> m_PinnedHigh1HVariationClipsets;
		atArray<fwMvClipSetId> m_PinnedHigh2HVariationClipsets;

		atArray<fwMvClipSetId> m_OutroReact1HVariationClipsets;		
		atArray<fwMvClipSetId> m_OutroReact2HVariationClipsets;		

		atArray<fwMvClipSetId> m_IdleLow1HVariationClipsets;
		atArray<fwMvClipSetId> m_IdleLow2HVariationClipsets;
		atArray<fwMvClipSetId> m_IdleHigh1HVariationClipsets;
		atArray<fwMvClipSetId> m_IdleHigh2HVariationClipsets;

		atArray<fwMvClipSetId> m_PlayerIdleLow0HVariationClipsets;
		atArray<fwMvClipSetId> m_PlayerIdleLow1HVariationClipsets;
		atArray<fwMvClipSetId> m_PlayerIdleLow2HVariationClipsets;
		atArray<fwMvClipSetId> m_PlayerIdleHigh0HVariationClipsets;
		atArray<fwMvClipSetId> m_PlayerIdleHigh1HVariationClipsets;
		atArray<fwMvClipSetId> m_PlayerIdleHigh2HVariationClipsets;

		atArray<CTaskCover::CoverAnimStateInfo> m_PeekingVariationAnimStateInfos;	
		atArray<CTaskCover::CoverAnimStateInfo> m_PinnedIntroAnimStateInfos;
		atArray<CTaskCover::CoverAnimStateInfo> m_PinnedIdleAnimStateInfos;
		atArray<CTaskCover::CoverAnimStateInfo> m_PinnedOutroAnimStateInfos;
		atArray<CTaskCover::CoverAnimStateInfo> m_IdleVariationAnimStateInfos;

		fwMvClipSetId GetCoreMotionClipSetIdForPed(const CPed& rPed) const;

		const atArray<CTaskCover::CoverAnimStateInfo>& GetIdleAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetAtEdgeAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetPeekingAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetStoppingAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetMovingAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetEdgeTurnAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetCoverToCoverAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetSteppingAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetWalkStartAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetSettleAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetTurnEnterAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetTurnEndAnimStateInfoForPed(const CPed& rPed);
		const atArray<CTaskCover::CoverAnimStateInfo>& GetTurnWalkStartAnimStateInfoForPed(const CPed& rPed);
		const fwMvFilterId GetWeaponHoldingFilterIdForPed(const CPed& rPed);

		struct AnimStateInfos
		{
			atArray<CTaskCover::CoverAnimStateInfo> m_IdleAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_AtEdgeAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_PeekingAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_StoppingAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_MovingAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_EdgeTurnAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_CoverToCoverAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_SteppingAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_WalkStartAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_SettleAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_TurnEnterAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_TurnEndAnimStateInfos;
			atArray<CTaskCover::CoverAnimStateInfo> m_TurnWalkStartAnimStateInfos;
			fwMvFilterId m_WeaponHoldingFilterId;

			PAR_SIMPLE_PARSABLE;
		};

#if FPS_MODE_SUPPORTED
		bool m_EnableFirstPersonLocoAnimations;
		bool m_EnableFirstPersonAimingAnimations;
		bool m_EnableAimOutroToPeekAnims;
		bool m_EnableBlindFireToPeek;
#endif // FPS_MODE_SUPPORTED

		private:

		AnimStateInfos m_ThirdPersonAnimStateInfos;
#if FPS_MODE_SUPPORTED
		AnimStateInfos m_FirstPersonAnimStateInfos;
#endif // FPS_MODE_SUPPORTED

		fwMvClipSetId m_CoreMotionClipSetId;
		fwMvClipSetId m_CoreMotionFPSClipSetId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// PURPOSE: Move network ids corresponding to control parameters in the move network

	// OnEnter Signals
	static const fwMvBooleanId 	ms_IdleOnEnterId;
	static const fwMvBooleanId	ms_IdleVariationOnEnterId;
	static const fwMvBooleanId 	ms_AtEdgeOnEnterId;
	static const fwMvBooleanId 	ms_MovingOnEnterId;
	static const fwMvBooleanId 	ms_PeekingOnEnterId;
	static const fwMvBooleanId 	ms_StoppingOnEnterId;
	static const fwMvBooleanId 	ms_NormalTurnOnEnterId;
	static const fwMvBooleanId 	ms_EdgeTurnOnEnterId;
	static const fwMvBooleanId 	ms_CoverToCoverOnEnterId;
	static const fwMvBooleanId	ms_PinnedIntroOnEnterId;
	static const fwMvBooleanId	ms_PinnedIdleOnEnterId;
	static const fwMvBooleanId	ms_PinnedOutroOnEnterId;
	static const fwMvBooleanId	ms_SteppingOnEnterId;
	static const fwMvBooleanId	ms_WalkStartOnEnterId;
	static const fwMvBooleanId	ms_SettleOnEnterId;
	static const fwMvBooleanId 	ms_TurnEnterOnEnterId;
	static const fwMvBooleanId 	ms_TurnEndOnEnterId;

	// Loop Signals
	static const fwMvBooleanId	ms_LoopPeekClipId;
	static const fwMvBooleanId	ms_LoopPinnedIdleId;

	// ClipFinished Signals
	static const fwMvBooleanId 	ms_StoppingClipFinishedId;
	static const fwMvBooleanId 	ms_NormalTurnClipFinishedId;
	static const fwMvBooleanId 	ms_EdgeTurnClipFinishedId;
	static const fwMvBooleanId 	ms_CoverToCoverClipFinishedId;
	static const fwMvBooleanId 	ms_PeekingClipFinishedId;
	static const fwMvBooleanId	ms_PinnedIntroClipFinishedId;
	static const fwMvBooleanId	ms_PinnedIdleClipFinishedId;
	static const fwMvBooleanId	ms_PinnedOutroClipFinishedId;
	static const fwMvBooleanId	ms_IdleVariationClipFinishedId;
	static const fwMvBooleanId	ms_StepClipFinishedId;
	static const fwMvBooleanId	ms_SettleClipFinishedId;
	static const fwMvBooleanId	ms_ToStandLSettleId;
	static const fwMvBooleanId	ms_ToStandRSettleId;
	static const fwMvBooleanId	ms_ToStandLId;
	static const fwMvBooleanId	ms_ToStandRId;
	static const fwMvBooleanId	ms_ToCrouchLSettleId;
	static const fwMvBooleanId	ms_ToCrouchRSettleId;
	static const fwMvBooleanId	ms_ToCrouchLId;
	static const fwMvBooleanId	ms_ToCrouchRId;
	static const fwMvBooleanId	ms_ToWalkId;
	static const fwMvBooleanId  ms_CanEarlyOutForMovementId;
	static const fwMvBooleanId  ms_ToStepId;
	static const fwMvBooleanId	ms_ToTurnId;
	static const fwMvBooleanId	ms_FixupRotationId;
	static const fwMvBooleanId	ms_TurnEnterClipFinishedId;
	static const fwMvBooleanId	ms_TurnEndClipFinishedId;
	static const fwMvBooleanId	ms_WalkStartClipFinishedId;

	// ClipInterrupt Signals
	static const fwMvBooleanId  ms_EdgeTurnFixupId;
	static const fwMvBooleanId 	ms_EdgeTurnInterruptId;
	static const fwMvBooleanId 	ms_TurnInterruptId;
	static const fwMvBooleanId 	ms_TurnInterruptToMovingId;
	static const fwMvBooleanId 	ms_TurnInterruptToTurnId;
	static const fwMvBooleanId	ms_TurnInterruptToAimId;

	// MoveStateRequest Signals
	static const fwMvRequestId 	ms_StillRequestId;
	static const fwMvRequestId 	ms_MovingRequestId;
	static const fwMvRequestId 	ms_StoppingRequestId;
	static const fwMvRequestId	ms_RestartNormalTurnRequestId;
	static const fwMvRequestId	ms_RestartEdgeTurnRequestId;
	static const fwMvRequestId 	ms_TurningRequestId;
	static const fwMvRequestId 	ms_CoverToCoverRequestId;
	static const fwMvRequestId	ms_RestartWeaponHoldingRequestId;
	static const fwMvRequestId	ms_PinnedIdleRequestId;
	static const fwMvRequestId	ms_PinnedOutroRequestId;
	static const fwMvRequestId	ms_SteppingRequestId;
	static const fwMvRequestId	ms_WalkStartRequestId;
	static const fwMvRequestId	ms_SettleRequestId;
	static const fwMvRequestId	ms_TurnEnterRequestId;
	static const fwMvRequestId	ms_TurnEndRequestId;

	// ClipInput Signals
	static const fwMvClipId		ms_Clip0Id;
	static const fwMvClipId		ms_Clip1Id;
	static const fwMvClipId		ms_Clip2Id;
	static const fwMvClipId		ms_Clip3Id;
	static const fwMvClipId		ms_Clip4Id;
	static const fwMvClipId		ms_Clip5Id;
	static const fwMvClipId		ms_WeaponClip0Id;
	static const fwMvClipId		ms_WeaponClip1Id;
	static const fwMvClipId		ms_WeaponClip2Id;
	static const fwMvClipId		ms_WeaponClip3Id;

	// ClipOutput Signals
	static const fwMvClipId 	ms_IdleClipId;
	static const fwMvClipId 	ms_TurnClipId;
	static const fwMvClipId 	ms_EdgeClipId;
	static const fwMvClipId 	ms_GripClipId;
	static const fwMvClipId 	ms_CoverToCoverClipId;

	// Rate Signals
	static const fwMvFloatId	ms_StepRateId;
	static const fwMvFloatId	ms_TurnRateId;
	static const fwMvFloatId	ms_TurnStepRateId;
	static const fwMvFloatId	ms_WalkStartRateId;
	static const fwMvFloatId 	ms_MovementRateId;
	static const fwMvFloatId 	ms_CoverToCoverRateId;
	static const fwMvFloatId	ms_TurnEndToTurnEnterBlendDurationId;

	// PhaseOutput Signals
	static const fwMvFloatId 	ms_TurnClipCurrentPhaseId;
	static const fwMvFloatId 	ms_CoverToCoverClipCurrentPhaseId;
	static const fwMvFloatId 	ms_SteppingClipCurrentPhaseId;
	static const fwMvFloatId 	ms_IdleVariationCurrentPhaseID;
	static const fwMvFloatId	ms_RestartStillBlendDurationId;

	// Flags
	static const fwMvFlagId		ms_IdleFlagId;
	static const fwMvFlagId		ms_IdleVariationFlagId;
	static const fwMvFlagId 	ms_AtEdgeFlagId;
	static const fwMvFlagId 	ms_PeekingFlagId;
	static const fwMvFlagId		ms_EdgeTurnFlagId;
	static const fwMvFlagId 	ms_PinnedFlagId;
	static const fwMvFlagId		ms_UseWeaponHoldingId;
	static const fwMvFlagId		ms_UseGripClipId;

	// Blend Input Params
	static const fwMvFloatId 	ms_HeightId;
	static const fwMvFloatId 	ms_SpeedId;
	static const fwMvFloatId	ms_StepDistanceId;
	static const fwMvFloatId	ms_MovementClipRateId;
	static const fwMvFloatId	ms_StillToTurnBlendDurationId;
	static const fwMvFloatId	ms_SettleBlendDurationId;
	static const fwMvFloatId	ms_CamHeadingId;
	static const fwMvFloatId	ms_StoppingToTurnEnterBlendDurationId;

	// Variable ClipSetIds
	static const fwMvClipSetVarId ms_CoreMotionClipSetId;
	static const fwMvClipSetVarId ms_UpperBodyWeaponMotionClipSetId;
	static const fwMvClipSetVarId ms_WeaponClipSetId;
	static const fwMvClipSetVarId ms_FPSIntroGripClipSetId;

	// Filters
	static const fwMvFilterId ms_GripFilterId;
	static const fwMvFilterId ms_WeaponHoldingFilterId;

#if FPS_MODE_SUPPORTED
	static const fwMvFlagId			ms_IsFirstPersonFlagId;
#endif // FPS_MODE_SUPPORTED

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Static helper functions
#if FPS_MODE_SUPPORTED
	static bool ShouldUseFirstPersonLocoAnimations(const CPed& rPed);
	static bool ShouldUseFirstPersonAimingAnimations(const CPed& rPed);
#endif // FPS_MODE_SUPPORTED

	static bool IsPedsheadingCloseEnoughToFacingDirection(const CPed& ped, const Vector3& vCoverDirection, bool bIsFacingLeft, float fCustomTolerance = -1.0f);
	static bool IsPedCloseEnoughToCover(const CPed& ped, const Vector3& vCoverPosition, float fCustomTolerance = -1.0f);
	static fwMvClipId GetCoreClipIdFromIndex(s32 i);
	static fwMvClipId GetWeaponClipIdFromIndex(s32 i);

	static bool StartMotionInCoverNetwork(CMoveNetworkHelper& taskCoverMoveNetworkHelper, CMoveNetworkHelper& moveNetworkHelper, CPed* pPed);

	static bool PlayerCheckForMovingAbout(const CPed& ped, bool bFacingLeft, const Vector2& vStickInput, u32 uTimeOfLastRoundCornerMove = 0);

	static bool PlayerCheckForTurning(const CPed& ped, bool bFacingLeft, bool bAtEdge, bool bFromTurn, const Vector2& vStickInput, const Vector3& vCamHeading, bool bIsHighCover, bool bPeeking, bool& bDisallowTurnUntilInputZeroed);

	static bool PlayerCheckForCoverToCover(CCoverPoint& newCoverPoint, CPed& ped, bool bFacingLeft, const Vector2& vStickInput, bool bLowCover);

	static bool PlayerCheckForMoveAroundCorner(CPed& ped, bool bFacingLeft, const Vector2& vStickInput, bool bLowCover, u32 uTimeJumpLastPressed);

	static bool PlayerCheckForCornerExit(const CPed& ped, bool bFacingLeft, const Vector2& vStickInput);

	static fwMvClipSetId CheckForPinnedAndGetClipSet(const CPed& ped, bool bIsHighCover, bool bIgnoreMinPinned = false);
	static fwMvClipSetId CheckForIdleVariationAndGetClipSet(const CPed& ped, bool bIsHighCover);

	// PURPOSE: Update the players dynamic cover
	static bool FindNewDynamicCoverPoint(CPed* pPed, float fProbeOffset = CTaskInCover::ms_Tunables.m_DefaultProbeOffset, bool bIsFacingLeft = false, bool bMovedAroundCorner = false, bool bIdle = false, bool bUseFacingDirection = false, bool bFromIdleTurn = false);

	CTaskMotionInCover(CMoveNetworkHelper& taskCoverMoveNetworkHelper, CMoveNetworkHelper& moveNetworkHelper);
	virtual ~CTaskMotionInCover();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_MOTION_IN_COVER; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskMotionInCover(m_TaskCoverMoveNetworkHelper, m_MoveNetworkHelper); }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	// PURPOSE: Ensure move network and clipset are loaded
	bool StreamAssets();

	// PURPOSE: Setup grip clip if holding a projectile or melee weapon
	static void SetGripClip(CPed* pPed, CMoveNetworkHelper& moveNetworkHelper);

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual bool				OverridesNetworkBlender(CPed* pPed);
	virtual bool				OverridesNetworkHeadingBlender(CPed*);

	// PURPOSE: Run the mobile phone subtask
	void ProcessUseMobilePhone(bool bPutUpAnswerCall = false);
	void ProcessLookIK(CPed* pPed);
	bool ShouldRestartStateDueToCameraSwitch(CMoveNetworkHelper& rMoveNetworkHelper);

	bool CheckForTurn(bool bFromTurn = false, bool bCheckStickInput = true, bool bPeeking = false);
	bool CheckForSettle() const;

	void SetCoverToCoverPoint(const CCoverPoint& newCoverPoint);
	bool GetStoppedScalingCTCTranslation() const { return m_bStoppedScalingCTCTrans; }
	bool GetEnteredPeekState() const { return m_bEnteredPeekState; }
	bool GetPeekVariationStarted() const { return m_bPeekVariationStarted; }
	bool GetPeekVariationFinished() const { return m_bPeekVariationFinished || !IsCoverFlagSet(CTaskCover::CF_RunningPeekVariation); }
	void ResetEnteredPeekState() { m_bEnteredPeekState = false; }
	void ResetPeekVariationState() { m_bPeekVariationStarted = false; m_bPeekVariationFinished = false; }	
	bool GetPlayerPeekReset() {return m_bPlayerPeekReset;}
	bool GetUseHighCoverAnims() const {return m_bUseHighCoverAnims;}

	bool CanInterruptIdleVariation();

	bool IsTurningLeft() const { return ms_Tunables.m_UseNewTurns ? m_bTurningLeft : IsCoverFlagSet(CTaskCover::CF_TurningLeft); }
	bool IsLeftStickTriggeredTurn() const { return m_bLeftStickTriggeredTurn; }
	const CTaskCover::CoverAnimStateInfo* GetAnimStateInfoForState();
	
	static float ComputeGaussianValueForRestrictedCameraPitch(float fX);
	static float ComputeCoverHeading(const CPed& rPed);
	static float ComputeFacingHeadingForCoverDirection(const Vector3& vCoverDir, bool bFacingLeft);
	static float ComputeFacingHeadingForCoverDirection(float fCoverHeading, bool bFacingLeft);

#if FPS_MODE_SUPPORTED
	bool IsIdleForFirstPerson() const;
#endif // FPS_MODE_SUPPORTED

	// PURPOSE: Is currently idle
	bool IsIdle() const;

	// PURPOSE: Returns true if the ped has a cover point, can be used by local or clone peds
	bool HasNetCoverPoint(const CPed& ped) const { return SafeCast(const CTaskCover, GetParent())->HasNetCoverPoint(ped); }
	// PURPOSE: Returns the cover point position, can be used by local or clone peds
	bool GetNetCoverPointPosition(const CPed& ped, Vector3& vCoverPointPos) const { return SafeCast(const CTaskInCover, GetParent())->GetNetCoverPointPosition(ped, vCoverPointPos); }
	// PURPOSE: Returns the cover point direction, can be used by local or clone peds
	bool GetNetCoverPointDirection(const CPed& ped, Vector3& vCoverPointDirection) const { return SafeCast(const CTaskInCover, GetParent())->GetNetCoverPointDirection(ped, vCoverPointDirection); }
	// PURPOSE: Returns the cover point usage, can be used by local or clone peds
	bool GetNetCoverPointUsage(const CPed& ped, CCoverPoint::eCoverUsage& usage) const { return SafeCast(const CTaskInCover, GetParent())->GetNetCoverPointUsage(ped, usage); }

	bool IsDelayedExit() const { return SafeCast(const CTaskInCover, GetParent())->IsDelayedExit(); }


	void DoCloneTurn();

	bool RequestStepping() {if (m_bSteppingRequestComplete) return false; m_bSteppingRequest = true; return true;}

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Finish; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Called after clip and Ik
	virtual bool ProcessPostPreRender();

	// PURPOSE:	Handle communication with MoVE to support timeslicing
	virtual bool ProcessMoveSignals();

	// PURPOSE:	Send some parameters down to MoVE, regarding cover height, etc.
	void SendCommonMoveSignals();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return StreamAssets_OnUpdate();
	FSM_Return Idle_OnEnter();
	FSM_Return Idle_OnUpdate();
	void Idle_OnProcessMoveSignals();
	FSM_Return Idle_OnExit();
	FSM_Return Stepping_OnEnter();
	FSM_Return Stepping_OnUpdate();
	FSM_Return WalkStart_OnEnter();
	FSM_Return WalkStart_OnUpdate();
	FSM_Return Settle_OnEnter();
	FSM_Return Settle_OnUpdate();
	void Settle_OnProcessMoveSignals();
	FSM_Return PlayingIdleVariation_OnEnter();
	FSM_Return PlayingIdleVariation_OnUpdate();
	FSM_Return PlayingIdleVariation_OnExit();
	void PlayingIdleVariation_OnProcessMoveSignals();
	FSM_Return Stopping_OnEnter();
	FSM_Return Stopping_OnUpdate();
	FSM_Return StoppingAtEdge_OnEnter();
	FSM_Return StoppingAtEdge_OnUpdate();
	FSM_Return AtEdge_OnEnter();
	FSM_Return AtEdge_OnUpdate();
	void AtEdge_OnProcessMoveSignals();
	FSM_Return AtEdge_OnExit();
	FSM_Return Turning_OnEnter();
	FSM_Return Turning_OnUpdate();
	FSM_Return Turning_OnExit();
	FSM_Return TurnEnter_OnEnter();
	FSM_Return TurnEnter_OnUpdate();
	void TurnEnter_OnProcessMoveSignals();
	FSM_Return TurnEnter_OnExit();
	FSM_Return TurnEnd_OnEnter();
	FSM_Return TurnEnd_OnUpdate();
	void TurnEnd_OnProcessMoveSignals();
	FSM_Return TurnEnd_OnExit();
	FSM_Return Moving_OnEnter();
	FSM_Return Moving_OnUpdate();
	FSM_Return Moving_OnExit();
	FSM_Return StoppingAtInsideEdge_OnEnter();
	FSM_Return StoppingAtInsideEdge_OnUpdate();
	FSM_Return EdgeTurn_OnEnter();
	FSM_Return EdgeTurn_OnUpdate();
	FSM_Return EdgeTurn_OnExit();
	void EdgeTurn_OnProcessMoveSignals();
	FSM_Return InsideCornerTransition_OnEnter();
	FSM_Return InsideCornerTransition_OnUpdate();	
	FSM_Return InsideCornerTransition_OnExit();
	FSM_Return CoverToCover_OnEnter();
	FSM_Return CoverToCover_OnUpdate();
	FSM_Return CoverToCover_OnExit();
	FSM_Return AtEdgeLowCover_OnEnter();
	FSM_Return AtEdgeLowCover_OnUpdate();
	FSM_Return Peeking_OnEnter();
	FSM_Return Peeking_OnUpdate();
	void Peeking_OnProcessMoveSignals();
	FSM_Return Peeking_OnExit();
	FSM_Return PinnedIntro_OnEnter();
	FSM_Return PinnedIntro_OnUpdate();
	void PinnedIntro_OnProcessMoveSignals();
	FSM_Return Pinned_OnEnter();
	FSM_Return Pinned_OnUpdate();
	void Pinned_OnProcessMoveSignals();
	FSM_Return PinnedOutro_OnEnter();
	FSM_Return PinnedOutro_OnUpdate();
	void PinnedOutro_OnProcessMoveSignals();

	////////////////////////////////////////////////////////////////////////////////

	void ProcessStopping(s32 iNextState);
	void ProcessTurn(bool bLeft = true);
	void FindCoverEdgesAndUpdateCoverPos() 
	{ 
		taskAssert(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_IN_COVER);
		static_cast<CTaskInCover*>(GetParent())->FindCoverEdgeAndUpdateCoverPosition(); 
	}

	bool CalculateCoverPosition()
	{ 
		taskAssert(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_IN_COVER);
		return static_cast<CTaskInCover*>(GetParent())->CalculateCoverPosition(); 
	}

	// PURPOSE: Checks for changes in motion
	bool CheckForMovingAbout() const;
	bool CheckForIdle(bool bSearchForNewCover = true);
	bool CheckForAtEdge(bool bIgnoreEdgeCheck = false);
	bool CheckForPinned(CPed& ped, bool bIgnoreMinPinned = false);
	bool CheckForIdleVariation(CPed& ped);
	bool CheckForMovingAroundCorner();
	bool CheckForInsideCornerTransition();
	bool CheckForCoverToCover();
	bool CheckForPeeking(bool bWaitForEndOfPeekVariation = false);
	bool CheckForStepping();
	bool CheckForWalkStart() const;
	bool HasJustEnteredCover() const;

	// PURPOSE: Functions to extract the correct clips for the network from metadata
	void SetClipsFromAnimStateInfo();
	void SetClipsFromAnimStateInfo(s32 iState);
	void SetVariationClip(fwMvClipSetId clipSetId, const atArray<CTaskCover::CoverAnimStateInfo>& clipArray);

	// PURPOSE: Called within ProcessPhysics to scale the animated velocity to get to a specific target
	bool ProcessStoppingPhysics(float fTimeStep);
	bool ProcessCoverToCoverPhysics(float fTimeStep);
	bool ProcessTurningPhysics(float fTimeStep);

	void ScaleTranslationalVelocity(const crClip& clip, float fAnimPhase, const Vector3& vCoverPos, const Vector3& vCoverDir, float fTimeStep);

	bool ComputeDesiredSpeedAlongVector(float& fAnimatedSpeed, float fRemainingDistance, float fRemainingAnimatedDistance, float fAnimPhase, float fStartPhase, float fEndPhase, float fClipDuration, float fTimeStep, bool bForward = false);

	void ScaleRotationalVelocity(const crClip& clip, float fAnimPhase, float fFacingHeading, float fTimeStep);
	float ComputeDesiredPedHeadingFromCoverDirection(bool bFacingLeft, const Vector3& vCoverDir);
	bool NeedsToFacePedInCorrectDirection();

	s32	CalculateDesiredDirection(const Vector3& vTargetPos);
	void SetCoverFlag(s32 iFlag);
	void ClearCoverFlag(s32 iFlag);
	bool IsCoverFlagSet(s32 iFlag) const;
	Vector3 GetCoverCoords() const;
	Vector3 GetCoverDirection() const;
	Vector2 GetStickInput() const;
	Vector3 GetCameraHeadingVec() const;
	const CWeaponTarget& GetTarget() const;
	bool IsCoverPointTooHigh() const;
	void ComputeInterruptPhase();

	// PURPOSE: Send signals to MoVE network
	void SendSpeedSignal();
	void SendHeightSignal();
	bool GetIsHeightTransitioning() const;

	const crClip* GetClipAndPhaseForState(float& fPhase);

	bool CanUseTaskState(s32 iState) const;
	bool AreStreamedAnimsLoaded(s32 iOverrideState) const;
	bool DoesTaskStateRequireStreamedAnim(s32 iState) const;
	bool TestForObstructionsBehindPlayer(const CPed& rPed);

	bool IsCameraPointingInWrongDirectionForTurn(const CPed& rPed) const;

	fwMvClipSetId ChoosePeekingVariationClipSetForPed(CPed& ped, bool bIsHigh) const;
	static fwMvClipSetId ChoosePinnedVariationClipSetForPed(const CPed& ped, bool bIsHigh);
	static fwMvClipSetId ChooseIdleVariationClipSetForPed(const CPed& ped, bool bIsHigh);

	float ComputeMvCamHeading() const;
	void ForceResendOfSyncData(CPed& ped);

private:

	// PURPOSE:	Set by ProcessMoveSignals() in some states when it's been detected that the clip finished playing.
	bool m_MoveClipFinished;

	// PURPOSE:	Set by ProcessMoveSignals() when the MoVE network is in its target state.
	bool m_MoveInTargetState;

	// PURPOSE:	Used by ProcessMoveSignals() in the TurnEnd state, to reflect the ms_ToTurnId signal from MoVE.
	bool m_MoveSignalToTurn;

	// PURPOSE: Used by ProcessMoveSignals() in the Settle state to reflect if one of the possible transitions to idle has passed
	bool m_MoveCanTransitionToIdle;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper& m_TaskCoverMoveNetworkHelper;
	CMoveNetworkHelper& m_MoveNetworkHelper;
	float m_fTurnInterruptPhase;
	bool m_bCalculatedTurnInterruptPhase;
	Vector3 m_vInitialPedStoppingPosition;
	float m_fInitialAnimatedSpeed;
	float m_fProbeOffset;
	float m_fHeight;
	float m_fCoverToCoverDist;
	float m_fStandUpTimer;
	bool m_bCoverToCoverHeadingReached;
	bool m_bTurnHeadingReached;
	Vector3 m_vInitialPedCoverToCoverPosition;
	float m_fInitialPedCoverToCoverHeading;
	float m_fSideStartFixupPhase;
	float m_fSideEndFixupPhase;
	float m_fTurnEndToTurnEnterBlendDuration;
	bool m_bUseHighCoverAnims;
	bool  m_bEnteredPeekState;
	bool m_bPeekVariationStarted;
	bool  m_bPeekVariationFinished;	
	bool m_bUsingWalkStop;
	bool m_bIgnoreStillRequest;
	bool  m_bFire; //cover to cover shooting
	bool m_bStoppedScalingCTCTrans;	
	bool m_bTurningLeft;
	bool m_bCoverToCoverPointValid;
	bool m_bIsInLowCover;
	bool m_bMovedSinceTurning;
	bool m_bTurningIntoEdge;
	bool m_bTurnedFromLowCover;
	bool m_bDontChangeCoverPositionWhenTurning;
	bool m_bAbortCoverRequest;
	bool m_bRoundCornering;
	bool m_bDoCloneTurn;
	bool m_bMasterTurning;
	bool m_bSteppingRequest;
	bool m_bSteppingRequestComplete; //used to prevent multiple step corrections when impossible to succeed
	bool m_bWasUsingFirstPersonAnims;
	bool m_bRestartNextFrame;
#if FPS_MODE_SUPPORTED
	bool m_bDisallowTurnUntilInputZeroed;
	bool m_bLeftStickTriggeredTurn;
#endif
	CTaskSimpleTimer m_StayPinnedTimer;
	CTaskSimpleTimer m_IdleVariationTimer;
	fwMvClipSetId m_PinnedClipSetId;
	fwMvClipSetId m_IdleVarationClipSetId;
	fwClipSetRequestHelper m_CoreStreamedClipsetRequestHelper;
	float m_fStepDistance;
	CCoverPoint m_coverToCoverPoint;
	u8 m_uRoundCoverProcessed;
	u8 m_uVerifyCoverTimer;
	u32 m_uTimeJumpLastPressed;
	u32 m_uTimeOfLastRoundCornerMove;

	//player peek
	bool	m_bPlayerPeekReset;

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, MOTION_IN_COVER_STATES)

	void Debug() const;
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task info for motion in cover
////////////////////////////////////////////////////////////////////////////////

class CClonedMotionInCoverInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedMotionInCoverInfo() {}
	CClonedMotionInCoverInfo(s32 coverState, float stepDistance) : m_stepDistance(stepDistance) 
	{ 
		SetStatusFromMainTaskState(coverState); 
	}

	~CClonedMotionInCoverInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_MOTION_IN_COVER_CLONED_FSM;}

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskMotionInCover::State_Finish>::COUNT;}
	virtual const char*	GetStatusName() const { return "State"; }

	float GetStepDistance() const { return m_stepDistance; }

	void Serialise(CSyncDataBase& serialiser)
	{
		const float MAX_STEP_DISTANCE = 1.2f;

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool hasStepDist = m_stepDistance > 0.0f;

		SERIALISE_BOOL(serialiser, hasStepDist);

		if (hasStepDist || serialiser.GetIsMaximumSizeSerialiser())
		{
			bool hasDefaultDist = m_stepDistance > 0.49f && m_stepDistance < 0.51f ;

			SERIALISE_BOOL(serialiser, hasDefaultDist);

			if (!hasDefaultDist || serialiser.GetIsMaximumSizeSerialiser())
			{
				// clamp step distance
				if (m_stepDistance > MAX_STEP_DISTANCE)
				{
					m_stepDistance = MAX_STEP_DISTANCE;
				}

				SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_stepDistance, MAX_STEP_DISTANCE, SIZEOF_STEP_DISTANCE, "Step distance");
			}
			else
			{
				m_stepDistance = 0.5f;
			}
		}
		else
		{
			m_stepDistance = 0.0f;
		}
	}

private:

	static const unsigned SIZEOF_STEP_DISTANCE = 5;

	float	m_stepDistance;
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls motion whilst moving out to aim
////////////////////////////////////////////////////////////////////////////////

class CTaskAimGunFromCoverIntro : public CTask
{
public:

	// PURPOSE: Control whether and how far we move away from cover
	enum eCoverAimIntroFlags
	{
		CAF_MoveAwayFromCover					= BIT0,
		CAF_ShortStep							= BIT1,
		CAF_LongStep							= BIT2,
		CAF_NoStep								= BIT3
	};

	struct AimIntroClip
	{
		atArray<fwMvClipId>	m_Clips;
		s32					m_Flags;
		PAR_SIMPLE_PARSABLE;
	};

	struct AimStepInfo
	{
		float m_StepOutX;
		float m_StepOutY;
		float m_StepTransitionMinAngle;
		float m_StepTransitionMaxAngle;
		float m_PreviousTransitionExtraScalar;
		float m_NextTransitionExtraScalar;
		fwMvClipId m_PreviousTransitionClipId;
		fwMvClipId m_NextTransitionClipId;
		PAR_SIMPLE_PARSABLE;
	};

	struct AimStepInfoSet
	{
		atArray<AimStepInfo>	m_StepInfos;
		PAR_SIMPLE_PARSABLE;
	};

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //	// NOTES:

		float m_UpperBodyAimBlendInDuration; // Clip duration to begin blending the upper body aiming in
		float m_IntroMovementDuration;
		bool  m_DisableIntroScaling;
		bool  m_DisableTranslationScaling;
		bool  m_DisableRotationScaling;
		bool  m_DisableIntroOverShootCheck;
		bool  m_UseConstantIntroScaling;
		bool  m_RenderArcsAtCoverPosition;
		bool  m_RenderAimArcDebug;
		bool  m_UseMoverPositionWhilePeeking;
		bool  m_DisableWeaponBlocking;
		float m_ArcRadius;
		float m_IntroScalingDefaultStartPhase;
		float m_IntroScalingDefaultEndPhase;
		float m_IntroRotScalingDefaultStartPhase;
		float m_IntroRotScalingDefaultEndPhase;
		float m_IntroMaxScale;
		float m_SteppingApproachRateSlow;
		float m_SteppingApproachRate;
		float m_SteppingApproachRateFast;
		float m_SteppingHeadingApproachRate;

		// ---	END_DEV_TO_REMOVE	--- //

		float m_AiAimIntroCloseEnoughTolerance;
		float m_MaxStepBackDist; 	// The maximum distances we will step back/out as part of the intro
		float m_MinStepOutDist;		// 
		float m_MaxStepOutDist;		// 
		float m_IntroRate;			// Rate at which to play the intro anims
		float m_IntroRateToPeekFPS;
		float m_OutroRate;			// Rate at which to play the outro anims
		float m_OutroRateFromPeekFPS;
		float m_MinRotationalSpeedScale;
		float m_MaxRotationalSpeedScale;
		float m_HeadingReachedTolerance;
		float m_StepOutCapsuleRadiusScale;
		float m_AimDirectlyMaxAngle;

		float m_StepOutLeftX;
		float m_StepOutRightX;
		float m_StepOutY;

		float m_LowXClearOffsetCapsuleTest;
		float m_LowXOffsetCapsuleTest;
		float m_LowYOffsetCapsuleTest;
		float m_LowZOffsetCapsuleTest;
		float m_LowOffsetCapsuleLength;
		float m_LowOffsetCapsuleRadius;
		float m_LowSideZOffset;

		Vector2 m_LowLeftStep;
		Vector2 m_LowRightStep;
		float m_LowBlockedBlend;

		float m_LowStepOutLeftXBlocked;
		float m_LowStepOutLeftYBlocked;
		float m_LowStepBackLeftXBlocked;
		float m_LowStepBackLeftYBlocked;
		float m_LowStepOutRightXBlocked;
		float m_LowStepOutRightYBlocked;
		float m_LowStepBackRightXBlocked;
		float m_LowStepBackRightYBlocked;
		float m_DistConsideredAtAimPosition;

		float m_MinPhaseToApplyExtraHeadingAi;
		float m_MinPhaseToApplyExtraHeadingPlayer;
		float m_MaxAngularHeadingVelocityAi;
		float m_MaxAngularHeadingVelocityPlayer;
		float m_MaxAngularHeadingVelocityPlayerForcedStandAim;

		//////////////////////////////////////////////////////////////////////////

		const AimStepInfoSet& GetAimStepInfoSet(bool bIsFacingLeft) const { return bIsFacingLeft ? m_HighLeftAimStepInfoSet : m_HighRightAimStepInfoSet; }
		AimStepInfoSet m_HighLeftAimStepInfoSet;	
		AimStepInfoSet m_HighRightAimStepInfoSet;	
		atArray<AimIntroClip>	m_AimIntroClips;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	static const fwMvBooleanId	ms_AimIntroInterruptId;
	static const fwMvFloatId	ms_AimIntroBlendId;

	static Vector3 ComputeCurrentStepOutPosition(bool bIsFacingLeft, const CPed& ped, s32 iCurrentNode, bool bInHighCoverNotAtEdge = false, bool bIsCloseToPossibleCorner = false);
	static Vector3 ComputeHighCoverStepOutPosition(bool bIsFacingLeft, const CPed& ped, const CCoverPoint& coverPoint);
	static void	CheckForBlockedAimPosition(bool& bForwardAimBlocked, bool& bSideAimBlocked, const CPed& rPed, bool bFacingLeft, bool bTooHighCover);
	static void ApplyDesiredVelocityTowardsTarget(CPed& rPed, const Vector3& vTarget, float fTimeStep);

#if DEBUG_DRAW
	static Color32 GetAimStepColor(s32 i);
#endif // DEBUG_DRAW

	////////////////////////////////////////////////////////////////////////////////

	CTaskAimGunFromCoverIntro(const Vector3& vTargetCoords, u32 iTaskConfigFlags, CMoveNetworkHelper& moveNetworkHelper);
	~CTaskAimGunFromCoverIntro();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_AIM_GUN_FROM_COVER_INTRO; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const {return rage_new CTaskAimGunFromCoverIntro(m_vTargetCoords, m_Flags.GetAllFlags(), m_MoveNetworkHelper);}

	// PURPOSE: Get the tasks main move network helper
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }

	bool GetForwardBlocked() const { return m_bForwardBlocked; }
	bool GetSideBlocked() const { return m_bSideBlocked; }

	Vector3 GetTargetIntroPosition() { return m_vTargetIntroPosition; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define AIM_GUN_FROM_COVER_INTRO_STATES(X)	X(State_Start),		\
												X(State_Intro),		\
												X(State_Finish)

	// PURPOSE: FSM states
	enum eAimGunFromCoverIntroState
	{
		AIM_GUN_FROM_COVER_INTRO_STATES(DEFINE_TASK_ENUM)
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Finish; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);
	
	virtual bool ProcessMoveSignals();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return	Start_OnUpdate();
	FSM_Return	Intro_OnEnter();
	FSM_Return	Intro_OnUpdate();
	FSM_Return	Finish_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	float   ComputeAimIntroBlendSignal() const;
	void	SetClipsFromAnimStateInfo();
	bool	IsAimIntroFromPeek() const;
	void	ProcessIntroRotation(CPed& rPed, float fPhase, bool bUseGameTimeStep = false);
	bool	IsCoverFlagSet(s32 iFlag) const { taskAssertf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_IN_COVER, "Expected parent use cover task"); return GetParent() ? static_cast<const CTaskInCover*>(GetParent())->IsCoverFlagSet(iFlag) : false; }

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Task Config Flags
	fwFlags32 m_Flags;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper& m_MoveNetworkHelper;

	// Handles the upper body aiming anims
	CUpperBodyAimPitchHelper m_UpperBodyAimPitchHelper;
	CUpperBodyAimPitchHelper m_SecondUpperBodyAimPitchHelper;

	// PURPOSE:	Coords to the threat target we are taking cover from
	Vector3	m_vTargetCoords;
	Vector3 m_vDirToTarget;

	// PURPOSE: Initial and target positions/headings for the ped doing the intro
	Vector3 m_vInitialIntroPosition;
	Vector3 m_vTargetIntroPosition;
	float	m_fInitialHeading;
	float	m_fPreviousTargetIntroHeading;
	float	m_fTargetIntroHeading;

	// PURPOSE: Desired pitch parameter
	float m_fDesiredPitch;

	// PURPOSE: Current pitch parameter
	float m_fCurrentPitch;

	// PURPOSE: Step out length parameter
	float m_fAimIntroBlendSignal;

	float m_fClipHeading;
	float m_fExtraHeadingNeeded;
	float m_fTargetHeading;
	float m_fClipDistance;
	float m_fTargetDistance;

	bool m_bNoMoverFilterSet;
	bool m_bForwardBlocked;
	bool m_bSideBlocked;
	bool m_bForcedStepOut;

	// PURPOSE:	Set by ProcessMoveSignals() when the clip has finished.
	bool	m_bMoveClipFinished;

	// DEV ONLY?
	float m_fAnimatedDistanceMoved;
	float m_fDesiredDistanceMoved;
	float m_fExtraHeadingMoved;
	float m_fAnimatedHeadingMoved;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId	ms_AimIntroClipFinishedId;
	static const fwMvBooleanId	ms_BlendInUpperBodyAimId;
	static const fwMvRequestId	ms_AimingId;
	static const fwMvFlagId		ms_AimDirectlyFlagId;
	static const fwMvClipId		ms_AimIntroClip0Id;
	static const fwMvClipId		ms_AimIntroClip1Id;
	static const fwMvFloatId	ms_AimIntroClipCurrentPhaseId;

	////////////////////////////////////////////////////////////////////////////////

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, AIM_GUN_FROM_COVER_INTRO_STATES)
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

class CTaskAimGunFromCoverOutro : public CTask
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Control flags
	enum eCoverAimOutroFlags
	{
		CAF_ReactToFire					= BIT0,
		CAF_ShortStep					= BIT1,
		CAF_LongStep					= BIT2,
		CAF_NoStep						= BIT3,
		CAF_ToPeek						= BIT4
	};

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //	// NOTES:

		bool  m_DisableOutroScaling;
		bool  m_DisableRotationScaling;
		bool  m_DisableOutroOverShootCheck;
		bool  m_UseConstantOutroScaling;
		float m_OutroScalingDefaultStartPhase;
		float m_OutroScalingDefaultEndPhase;
		float m_OutroRotationScalingDefaultStartPhase;
		float m_OutroRotationScalingDefaultEndPhase;
		float m_OutroMaxScale;

		// ---	END_DEV_TO_REMOVE	--- //

		float m_AdditionalModifier;
		float m_EndHeadingTolerance;
		float m_DesiredDistanceToCover;
		float m_InCoverMovementSpeed;
		float m_OutroMovementDuration;
		float m_UpperBodyAimBlendOutDuration;

		float m_MaxAngularHeadingVelocityAi;
		float m_MaxAngularHeadingVelocityPlayer;
		float m_MaxAngularHeadingVelocityPlayerFPS;
		float m_MaxAngularHeadingVelocityPlayerForcedStandAim;

		const atArray<CTaskCover::CoverAnimStateInfo>& GetAimOutroAnimStateInfoForPed(const CPed& rPed);

		struct AnimStateInfos
		{
			atArray<CTaskCover::CoverAnimStateInfo> m_AimOutroAnimStateInfos;

			PAR_SIMPLE_PARSABLE;
		};

		private:

		AnimStateInfos m_ThirdPersonAnimStateInfos;

#if FPS_MODE_SUPPORTED
		AnimStateInfos m_FirstPersonAnimStateInfos;
#endif // FPS_MODE_SUPPORTED

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	static const fwMvBooleanId	ms_OutroToIntroAimInterruptId;
	static const fwMvBooleanId	ms_OutroAimInterruptId;
	static const fwMvFloatId	ms_AimOutroBlendId;

	////////////////////////////////////////////////////////////////////////////////

	CTaskAimGunFromCoverOutro(const Vector3& vTargetCoords, u32 iTaskConfigFlags, CMoveNetworkHelper& moveNetworkHelper);
	~CTaskAimGunFromCoverOutro();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_AIM_GUN_FROM_COVER_OUTRO; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask*	Copy() const {return rage_new CTaskAimGunFromCoverOutro(m_vTargetCoords, m_Flags.GetAllFlags(), m_MoveNetworkHelper);}

	// PURPOSE: Get the tasks main move network helper
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	bool IsOutroFlagSet(eCoverAimOutroFlags flag) const { return m_Flags.IsFlagSet(flag); }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define AIM_GUN_FROM_COVER_OUTRO_STATES(X)	X(State_Start),		\
												X(State_Outro),		\
												X(State_Finish)

	// PURPOSE: FSM states
	enum eAimGunFromCoverOutroState
	{
		AIM_GUN_FROM_COVER_OUTRO_STATES(DEFINE_TASK_ENUM)
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Finish; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE:	Handle communication with MoVE to support timeslicing
	virtual bool ProcessMoveSignals();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return	Start_OnUpdate();
	FSM_Return	Outro_OnEnter();
	FSM_Return	Outro_OnUpdate();
	FSM_Return	Finish_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	float			ComputeAimOutroBlendSignal() const;
	bool			IsCoverFlagSet(s32 iFlag) const { taskAssertf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_IN_COVER, "Expected parent use cover task"); return GetParent() ? static_cast<const CTaskInCover*>(GetParent())->IsCoverFlagSet(iFlag) : false; }
	fwMvClipSetId	ChooseOutroVariationClipSetForPed(CPed& ped) const;
	void			SetClipsFromAnimStateInfo();
	void			ProcessOutroRotation(CPed& rPed, bool bUseGameTimeStep = false);
	bool			ShouldUseLowEdgeAnims() const;

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Task Config Flags
	fwFlags32 m_Flags;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper& m_MoveNetworkHelper;

	// Handles the upper body aiming anims
	CUpperBodyAimPitchHelper m_UpperBodyAimPitchHelper;

	// PURPOSE:	Coords to the threat target we are taking cover from
	Vector3	m_vTargetCoords;
	Vector3 m_vDirToTarget;
	Vector3 m_vInitialOutroPosition;

	float	m_fCachedGroundHeight;
	float	m_fPreviousTargetOutroHeading;
	float	m_fTargetOutroHeading;

	// PURPOSE: Desired pitch parameter
	float m_fDesiredPitch;

	// PURPOSE: Current pitch parameter
	float m_fCurrentPitch;

	// PURPOSE: Initial heading/position of the ped when beginning the outro
	float	m_fInitialHeading;
	float	m_fOutroBlendParam;

	// PURPOSE:	Set by ProcessMoveSignals() when the clip has finished.
	bool	m_bMoveClipFinished;

	bool	m_bHeadingReached;
	bool	m_bPositionReached;
	bool	m_bReverseRotation;

	float m_fClipHeading;
	float m_fAnimatedHeadingMoved;
	float m_fExtraHeadingMoved;
	float m_fExtraHeadingNeeded;
	float m_fTargetHeading;

	float m_fClipDistance;
	float m_fAnimatedDistanceMoved;
	float m_fDesiredDistanceMoved;
	float m_fTargetDistance;
	float m_fExtraDistanceNeeded;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId	ms_AimOutroClipFinishedId;
	static const fwMvBooleanId	ms_BlendOutUpperBodyAimId;

	////////////////////////////////////////////////////////////////////////////////

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, AIM_GUN_FROM_COVER_OUTRO_STATES)
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls clips whilst blind firing in cover
////////////////////////////////////////////////////////////////////////////////

class CTaskAimGunBlindFire : public CTaskAimGun
{
public:

	////////////////////////////////////////////////////////////////////////////////

	static const fwMvBooleanId	ms_BlindFireOutroInterruptId;
	static const fwMvRequestId ms_NewBlindFireSolutionRequestId;

	struct BlindFireAnimStateInfo
	{
		fwMvClipId	m_IntroClipId;
		fwMvClipId	m_BaseClipId;
		fwMvClipId	m_OutroClipId;
		s32			m_Flags;
		PAR_SIMPLE_PARSABLE;
	};

	////////////////////////////////////////////////////////////////////////////////

	struct BlindFirePitchAnimAngles
	{
		float		m_MinPitchAngle;
		float		m_MaxPitchAngle;
		float		m_MinPitchAngle2H;
		float		m_MaxPitchAngle2H;
		PAR_SIMPLE_PARSABLE;
	};

	struct BlindFireAnimStateInfoNew
	{
		fwMvClipId	m_IntroClip0Id;
		fwMvClipId	m_IntroClip1Id;
		fwMvClipId	m_SweepClip0Id;
		fwMvClipId	m_SweepClip1Id;
		fwMvClipId	m_SweepClip2Id;
		fwMvClipId	m_OutroClip0Id;
		fwMvClipId	m_OutroClip1Id;
		fwMvClipId	m_CockGunClip0Id;
		fwMvClipId	m_CockGunClip1Id;
		fwMvClipId	m_CockGunWeaponClipId;
		s32			m_Flags;
		float		m_MinHeadingAngle;
		float		m_MaxHeadingAngle;

		float GetBlindFireMinPitchAngleForPed(const CPed& rPed) const;
		float GetBlindFireMaxPitchAngleForPed(const CPed& rPed) const;
		float GetBlindFireMinPitchAngle2HForPed(const CPed& rPed) const;
		float GetBlindFireMaxPitchAngle2HForPed(const CPed& rPed) const;

	private:

		BlindFirePitchAnimAngles m_ThirdPersonBlindFirePitchAngles;
		BlindFirePitchAnimAngles m_FirstPersonPersonBlindFirePitchAngles;

		PAR_SIMPLE_PARSABLE;
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_LowBlindFireAimingDirectlyLimitAngle;
		float m_HighBlindFireAimingDirectlyLimitAngle;
		bool m_RemoveReticuleDuringBlindFire;
		bool m_DontRemoveReticuleDuringBlindFireNew;
		atArray<BlindFireAnimStateInfoNew> m_BlindFireAnimStateNewInfos;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// Calculates the yaw angle required to rotate by from the start position to point to the end position
	static float CalculateDesiredYaw(const Matrix34& mRefMtx, const Vector3& vStart, const Vector3& vEnd);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define AIM_GUN_BLIND_FIRE_STATES(X)	X(State_Start),				\
											X(State_IntroNew),			\
											X(State_FireNew),			\
											X(State_OutroNew),			\
											X(State_CockGun),			\
											X(State_Finish)

	// PURPOSE: FSM states
	enum eAimGunBlindFireState
	{
		AIM_GUN_BLIND_FIRE_STATES(DEFINE_TASK_ENUM)
	};

	////////////////////////////////////////////////////////////////////////////////

	CTaskAimGunBlindFire(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const CWeaponTarget& target, CMoveNetworkHelper& moveNetworkHelper, const CGunIkInfo& ikInfo);
	virtual ~CTaskAimGunBlindFire();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_AIM_GUN_BLIND_FIRE; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask*	Copy() const { return rage_new CTaskAimGunBlindFire(m_weaponControllerType, m_iFlags, m_target, m_MoveNetworkHelper, m_ikInfo); }

	// PURPOSE: Get the tasks main move network helper
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }

	//
	// Accessors
	//

	// PURPOSE: Returns the current weapon controller state for the specified ped
	virtual WeaponControllerState GetWeaponControllerState(const CPed* pPed) const;

	virtual bool GetIsReadyToFire() const { return GetState() == State_FireNew;  }
	virtual bool GetIsFiring() const { return GetState() == State_FireNew; }
	bool GetIKLeftHandUseGripBone() const { return m_bMoveIKLeftHandUseGripBone; }
	bool UsingOverTheTopFiring() const { return m_bIsOverTheTop; }
	bool GetFiredOnce() const { return m_bFiredOnce; }
	bool ShouldBeDoingOutro() const { return GetState() == State_OutroNew || GetState() == State_CockGun || m_bOutroPending; }
	bool IsUsingAtEdgeAnims() const { return m_bUsingAtEdgeAnims; }
	bool IsValidForFPSIk(bool bLowCover) const;
	bool CanInterruptOutro() const;

	// PURPOSE: Clone task implementation
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

	void RequestTaskEnd() {m_bEndTaskRequested = true;}

protected:

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Finish; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Called after the camera system update
	virtual bool ProcessPostCamera();

	// PURPOSE: Called after clip and Ik
	virtual bool ProcessPostPreRender();

	// PURPOSE: 
	virtual bool ProcessMoveSignals();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return IntroNew_OnEnter();
	FSM_Return IntroNew_OnUpdate();
	void	   IntroNew_OnExit();
	void	   IntroNew_OnProcessMoveSignals();
	FSM_Return FireNew_OnEnter();
	FSM_Return FireNew_OnUpdate();
	FSM_Return FireNew_OnExit();
	void	   FireNew_OnProcessMoveSignals();
	FSM_Return OutroNew_OnEnter();
	FSM_Return OutroNew_OnUpdate();
	void	   OutroNew_OnExit();
	void	   OutroNew_OnProcessMoveSignals();
	FSM_Return CockGun_OnEnter();
	FSM_Return CockGun_OnUpdate();
	void	   CockGun_OnProcessMoveSignals();

	////////////////////////////////////////////////////////////////////////////////

	bool	ShouldFire();
	void	DoFire();
	void	ResetFire();
	bool	WantsToReload() const;
	void	ProcessIK();

	CMoveNetworkHelper&	m_MoveNetworkHelper;
	CMoveNetworkHelper*	m_pTaskUseCoverMoveNetworkHelper;

	BlindFireAnimStateInfoNew* m_pCachedAnimStateInfoNew;

	float				m_fIntroHeadingBlend;
	float				m_fDesiredHeading;
	float				m_fCurrentHeading;
	float				m_fDesiredPitch;
	float				m_fCurrentPitch;
	float				m_fOutroHeadingBlend;
	float				m_fBlindFireAimBlendDuration;

	u32					m_CachedWeaponHash;

	// Ik constraints
	CGunIkInfo m_ikInfo;

	u8					m_targetFireCounter; // used when a clone task

	bool				m_bUsesFireEvents : 1;
	bool				m_bFire : 1;
	bool				m_bFirstFire : 1;
	bool				m_bFireLoopClipFinished : 1;
	bool				m_bFiredOnce : 1;
	bool				m_bFiredTwice : 1;
	bool				m_bFireLoopOnEnter : 1;
	bool				m_bOutroPending : 1;
	bool				m_bMoveFireLoopClipFinished : 1;
	bool				m_bMoveFireLoopOnEnter : 1;
	bool				m_bMoveFire : 1;
	bool				m_bMoveFireLoop1 : 1;
	bool				m_bMoveFireLoop2 : 1;
	bool				m_bMoveInterruptToCockGun : 1;
	bool				m_bMoveBlindFireIntroClipFinished : 1;
	bool				m_bMoveBlindFireOutroClipFinished : 1;
	bool				m_bMoveCockGunClipFinished : 1;
	bool				m_bMoveCockInterrupt : 1;
	bool				m_bMoveAimIntroInterrupt : 1;
	bool				m_bMoveIKLeftHandUseGripBone : 1;
	bool				m_bCornerToTopBlindFireTransition : 1;
	bool				m_bBlockOutroRefire : 1;
	bool				m_bAtDoor : 1;
	bool				m_bIsOverTheTop : 1;
	bool				m_bPopAimParams : 1;
	bool				m_bEndTaskRequested : 1;
	bool				m_bUsingAtEdgeAnims : 1;

	static const fwMvBooleanId ms_BlockTransitionFromAimOutroId;
	static const fwMvBooleanId ms_BlindFireIntroClipFinishedId;
	static const fwMvBooleanId ms_BlindFireLoopClipFinishedId;
	static const fwMvBooleanId ms_BlindFireOutroClipFinishedId;
	static const fwMvBooleanId ms_CockGunClipFinishedId;
	static const fwMvBooleanId ms_CockInterruptId;
	static const fwMvBooleanId ms_InterruptToCockGunId;

	static const fwMvRequestId ms_BlindFireLoopRequestId;
	static const fwMvRequestId ms_BlindFireLoopRestartRequestId;
	static const fwMvRequestId ms_BlindFireOutroRequestId;
	static const fwMvRequestId ms_BlindFireIntroRequestId;
	static const fwMvRequestId ms_CockGunRequestId;

	static const fwMvBooleanId ms_BlindFiringIntroOnEnterId;
	static const fwMvBooleanId ms_BlindFiringLoopOnEnterId;
	static const fwMvBooleanId ms_BlindFiringOutroOnEnterId;
	static const fwMvBooleanId ms_CockGunOnEnterId;
	static const fwMvBooleanId ms_IKLeftHandUseGripBoneId;

	static const fwMvBooleanId ms_FireClipFinishedId;

	static const fwMvClipId ms_BlindFireIntroClipId;
	static const fwMvClipId ms_BlindFireLoopClipId;
	static const fwMvClipId ms_BlindFireOutroClipId;
	static const fwMvClipId ms_BlindFireOutroClip1Id;
	
	static const fwMvFloatId ms_BlindFireIntroPhaseId;
	static const fwMvFloatId ms_BlindFireIntroBlendId;
	static const fwMvFloatId ms_BlindFireSweepBlendId;
	static const fwMvFloatId ms_BlindFireSweepPhaseId;

	static const fwMvFloatId ms_BlindFireAimBlendDurationId;

	// Recoil support
	static const fwMvClipSetVarId ms_FireClipSetVarId;
	static const fwMvClipId ms_FireClipId;
	static const fwMvClipId ms_FireRecoilClipId;

	static const fwMvBooleanId ms_FireId;
	static const fwMvBooleanId ms_FireLoopOnEnterId;
	static const fwMvBooleanId ms_FireLoop1OnExitId;
	static const fwMvBooleanId ms_FireLoop2OnExitId;

	static const fwMvRequestId ms_FireRequestId;
	static const fwMvRequestId ms_FireFinishedRequestId;

	static const fwMvFloatId ms_FireRateId;

	static const fwMvFlagId ms_FiringFlagId;
	static const fwMvFlagId ms_IsRightSideId;

public:


	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, AIM_GUN_BLIND_FIRE_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task info for aiming a gun while blind firing
////////////////////////////////////////////////////////////////////////////////

class CClonedAimGunBlindFireInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedAimGunBlindFireInfo();
	CClonedAimGunBlindFireInfo(s32 aimGunBlindFireState, u8 fireCounter, u8 ammoInClip, u32 seed);
	~CClonedAimGunBlindFireInfo() {}

	virtual s32		GetTaskInfoType( ) const {return INFO_TYPE_AIM_GUN_BLIND_FIRE;}

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskAimGunBlindFire::State_Finish>::COUNT; }

	u8 GetFireCounter() const { return m_fireCounter; }
	u8 GetAmmoInClip()  const { return m_iAmmoInClip; }

	u32 GetSeed() const { return m_seed; }

	void Serialise(CSyncDataBase& serialiser);

private:

	u32		m_seed;
	u8		m_iAmmoInClip;
	u8		m_fireCounter;
	bool    m_bHasSeed;
};


////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Helper classes for updating cover variations
////////////////////////////////////////////////////////////////////////////////

struct CoverVariationsInfo
{
	static const u32 sMaxCoverVariationClipsets = 2;

	enum eVariationType
	{
		VT_AiPeeking,
		VT_AiPinned,
		VT_AiIdle,
		VT_PlayerIdle,
		VT_AiOutroReact
	};

	enum eArmamentType
	{
		AT_Unarmed,
		AT_OneHanded,
		AT_TwoHanded
	};

	CoverVariationsInfo(u32 uMinUsesForVariationChange, u32 uMaxUsesForVariationChange)
		: m_aVariationClipsetRequestHelpers(sMaxCoverVariationClipsets)
		, m_uNumTimesUsed(0)
		, m_uMaxNumTimesUsed(0)
		, m_iCurrentIndex(0)
		, m_CoverClipsetShuffler(1)
		, m_iCoverClipsetIndex(0)
		, m_uMinUsesForVariationChange(uMinUsesForVariationChange)
		, m_uMaxUsesForVariationChange(uMaxUsesForVariationChange)
	{
		Reset();
	}

	static const atArray<fwMvClipSetId>* GetClipSetArrayForVariationType(eVariationType variationType, eArmamentType armamentType, bool bHigh);

	static void	InitCoverVariations(const atArray<fwMvClipSetId>& clipsets, 
									atFixedArray<CPrioritizedClipSetRequestHelper, CoverVariationsInfo::sMaxCoverVariationClipsets>& requesthelpers,
									SemiShuffledSequence& shuffler,
									s32& shufflerIndex);

	fwMvClipSetId ChooseClipSetForPed(const CPed& ped);

	bool HasActiveRequests() const
	{
		for(int i = 0; i < m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			if(m_aVariationClipsetRequestHelpers[i].IsActive())
			{
				return true;
			}
		}

		return false;
	}

	void Reset() 
	{ 
		m_uNumTimesUsed = 0; 
		m_uMaxNumTimesUsed = static_cast<u32>(fwRandom::GetRandomNumberInRange((s32)m_uMinUsesForVariationChange, (s32)m_uMaxUsesForVariationChange));
		m_iCurrentIndex = 0; 
	}

	atFixedArray<CPrioritizedClipSetRequestHelper, sMaxCoverVariationClipsets> m_aVariationClipsetRequestHelpers;
	u32	m_uNumTimesUsed;
	u32 m_uMaxNumTimesUsed;
	s32 m_iCurrentIndex;
	SemiShuffledSequence m_CoverClipsetShuffler;
	s32 m_iCoverClipsetIndex;
	u32 m_uMinUsesForVariationChange;
	u32 m_uMaxUsesForVariationChange;
};

////////////////////////////////////////////////////////////////////////////////

class CAiCoverClipVariationHelper
{
public:
	
	struct Tunables : public CTuning
	{
		Tunables();

		u32 m_MinUsesForPeekingVariationChange;
		u32 m_MaxUsesForPeekingVariationChange;
		u32 m_MinUsesForPinnedVariationChange;
		u32 m_MaxUsesForPinnedVariationChange;
		u32 m_MinUsesForOutroReactVariationChange;
		u32 m_MaxUsesForOutroReactVariationChange;
		u32 m_MinUsesForIdleVariationChange;
		u32 m_MaxUsesForIdleVariationChange;

		PAR_PARSABLE;
	};

	CAiCoverClipVariationHelper();
	~CAiCoverClipVariationHelper();

	void Init();

	void UpdateClipVariationStreaming(u32 uNumPedsWith1HWeapons, u32 uNumPedsWith2HWeapons);

	CoverVariationsInfo& GetCoverPeekingVariationsInfo(bool bTwoHanded, bool bHigh) 
	{ 
		return bHigh ? (bTwoHanded ? m_coverHighPeeking2HVariationsInfo : m_coverHighPeeking1HVariationsInfo) :
					   (bTwoHanded ? m_coverLowPeeking2HVariationsInfo : m_coverLowPeeking1HVariationsInfo);
	}

	CoverVariationsInfo& GetCoverPinnedVariationsInfo(bool bTwoHanded, bool bHigh) 
	{ 
		return bHigh ? (bTwoHanded ? m_coverHighPinned2HVariationsInfo : m_coverHighPinned1HVariationsInfo) :
					   (bTwoHanded ? m_coverLowPinned2HVariationsInfo : m_coverLowPinned1HVariationsInfo);
	}

	CoverVariationsInfo& GetCoverReactOutroVariationsInfo(bool bTwoHanded) 
	{ 
		return (bTwoHanded ? m_coverReactOutro2HVariationsInfo : m_coverReactOutro1HVariationsInfo);
	}

	CoverVariationsInfo& GetCoverIdleVariationsInfo(bool bTwoHanded, bool bHigh) 
	{ 
		return bHigh ? (bTwoHanded ? m_coverHighIdle2HVariationsInfo : m_coverHighIdle1HVariationsInfo) :
					   (bTwoHanded ? m_coverLowIdle2HVariationsInfo : m_coverLowIdle1HVariationsInfo);
	}

private:
	
	static Tunables	sm_Tunables;

private:

	void	UpdateCover1HVariations();
	void	UpdateCover2HVariations();

	void	InitCoverVariationsFromInfo(CoverVariationsInfo& variationInfo, bool bTwoHanded, bool bHigh, CoverVariationsInfo::eVariationType variationType);
	void	UpdateCoverVariationsFromInfo(CoverVariationsInfo& variationInfo, bool bTwoHanded, bool bHigh, CoverVariationsInfo::eVariationType variationType);

private:

	CoverVariationsInfo 		m_coverLowPeeking1HVariationsInfo;
	CoverVariationsInfo 		m_coverLowPeeking2HVariationsInfo;
	CoverVariationsInfo 		m_coverHighPeeking1HVariationsInfo;
	CoverVariationsInfo 		m_coverHighPeeking2HVariationsInfo;

	CoverVariationsInfo			m_coverLowPinned1HVariationsInfo;
	CoverVariationsInfo			m_coverLowPinned2HVariationsInfo;
	CoverVariationsInfo			m_coverHighPinned1HVariationsInfo;
	CoverVariationsInfo			m_coverHighPinned2HVariationsInfo;

	CoverVariationsInfo			m_coverReactOutro1HVariationsInfo;
	CoverVariationsInfo			m_coverReactOutro2HVariationsInfo;	

	CoverVariationsInfo			m_coverLowIdle1HVariationsInfo;
	CoverVariationsInfo			m_coverLowIdle2HVariationsInfo;
	CoverVariationsInfo			m_coverHighIdle1HVariationsInfo;
	CoverVariationsInfo			m_coverHighIdle2HVariationsInfo;

};

////////////////////////////////////////////////////////////////////////////////

class CPlayerCoverClipVariationHelper
{
public:

	static CPlayerCoverClipVariationHelper& GetInstance() { return sm_Instance; }

	struct Tunables : public CTuning
	{
		Tunables();

		u32 m_MinUsesForIdleVariationChange;
		u32 m_MaxUsesForIdleVariationChange;

		PAR_PARSABLE;
	};

	enum eVariationType
	{
		VT_Idle
	};

	CPlayerCoverClipVariationHelper();
	~CPlayerCoverClipVariationHelper();

	void ClearClipVariationStreaming();
	void UpdateClipVariationStreaming(const CPed& ped);

	CoverVariationsInfo& GetCoverIdleVariationsInfo(CoverVariationsInfo::eArmamentType armamentType, bool bHigh) 
	{ 
		switch (armamentType)
		{
			case CoverVariationsInfo::AT_Unarmed:	return bHigh ? m_coverHighIdle0HVariationsInfo : m_coverLowIdle0HVariationsInfo;
			case CoverVariationsInfo::AT_OneHanded: return bHigh ? m_coverHighIdle1HVariationsInfo : m_coverLowIdle1HVariationsInfo;
			case CoverVariationsInfo::AT_TwoHanded: return bHigh ? m_coverHighIdle2HVariationsInfo : m_coverLowIdle2HVariationsInfo;
			default: aiAssertf(0, "Unhandled armament type %i", armamentType);
		}
		return m_coverHighIdle0HVariationsInfo;
	}

private:

	static Tunables	sm_Tunables;
	static CPlayerCoverClipVariationHelper sm_Instance;

private:

	void	UpdateCover0HVariations();
	void	UpdateCover1HVariations();
	void	UpdateCover2HVariations();

	void	InitCoverVariationsFromInfo(CoverVariationsInfo& variationInfo, CoverVariationsInfo::eArmamentType armamentType, bool bHigh, CoverVariationsInfo::eVariationType variationType);
	void	UpdateCoverVariationsFromInfo(CoverVariationsInfo& variationInfo, CoverVariationsInfo::eArmamentType armamentType, bool bHigh, CoverVariationsInfo::eVariationType variationType, bool bStreamIn);

private:

	CoverVariationsInfo			m_coverLowIdle0HVariationsInfo;
	CoverVariationsInfo			m_coverLowIdle1HVariationsInfo;
	CoverVariationsInfo			m_coverLowIdle2HVariationsInfo;
	CoverVariationsInfo			m_coverHighIdle0HVariationsInfo;
	CoverVariationsInfo			m_coverHighIdle1HVariationsInfo;
	CoverVariationsInfo			m_coverHighIdle2HVariationsInfo;
	bool						m_bInitialised;

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Moves the player around dynamic cover in the world, i hope
////////////////////////////////////////////////////////////////////////////////

typedef enum
{
	LP_LeftLow,
	LP_FrontLowL,
	LP_FrontLowC,
	LP_FrontLowR,
	LP_RightLow,
	LP_LeftHigh,
	LP_FrontHighL,
	LP_FrontHighC,
	LP_FrontHighR,
	LP_RightHigh,
	LP_LowCentre,
	LP_Max
} LineProbes;

typedef enum
{
	CTT_Unknown = -1,
	CTT_Capsule = 0,
	CTT_Los
} CollisionTestType;

typedef struct  
{
	bool	m_bCollided;
	bool	m_bCollidedWithFixed;
	bool	m_bCollidedWithSeeThrough;
	bool	m_bHitRoundCover;
	bool	m_bRejectedBecauseOfAngularCondition;
	Vector3 m_vRoundCoverCenter;
	Vector3 m_vCapsuleStart;
	Vector3 m_vNormal;
	Vector3 m_vPosition;
	float	m_fRoundCoverRadius;
	float	m_fDepth;
	CEntity* m_pHitEntity;
} ProbeResults;

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Probes the world to find a suitable place to create
//			a new dynamic coverpoint
////////////////////////////////////////////////////////////////////////////////

class CDynamicCoverHelper : public CTaskHelperFSM
{
	friend class CCoverDebug;

public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //	// NOTES:


		// ---	END_DEV_TO_REMOVE	--- //

		bool m_EnableConflictingNormalCollisionRemoval;
		bool  m_UseStickHistoryForCoverSearch;
		float m_StickDownMinRange;
		u32 m_StickDownDuration;
		bool m_UseCameraOrientationWeighting;
		bool m_UseCameraOrientationWhenStill;
		bool m_UseCameraOrientationForBackwardsDirection;
		float m_BehindThreshold;
		float m_DistanceToWallStanding;
		float m_DistanceToWallCrouching;
		float m_DistanceToWallCoverToCover;

		float m_OCMCrouchedForwardClearanceOffset;	// Outside corner move variables
		float m_OCMStandingForwardClearanceOffset;
		float m_OCMSideClearanceDepth;
		float m_OCMClearanceCapsuleRadius;
		float m_OCMSideTestDepth;
		float m_OCMCrouchedHeightOffset;
		float m_OCMStandingHeightOffset;
		float m_CTCSideOffset;
		float m_CTCProbeDepth;
		float m_CTCForwardOffset;
		float m_CTCSpacingOffset;
		float m_CTCCapsuleRadius;
		float m_CTCHeightOffset;
		float m_LowCoverProbeHeight;
		float m_HighCoverProbeHeight;
		float m_CTCClearanceCapsuleRadius;
		float m_CTCClearanceCapsuleStartForwardOffset;
		float m_CTCClearanceCapsuleEndForwardOffset;
		float m_CTCClearanceCapsuleStartZOffset;
		float m_CTCClearanceCapsuleEndZOffset;
		float m_CTCMinDistance;
		float m_VehicleEdgeProbeXOffset;
		float m_VehicleEdgeProbeZOffset;
		float m_MaxZDiffBetweenCoverPoints;
		float m_MaxZDiffBetweenPedPos;
		float m_MaxHeadingDiffBetweenCTCPoints;
		float m_PedToCoverCapsuleRadius;
		float m_PedToCoverEndPullBackDistance;
		float m_PedToCoverEndZOffset;
		float m_MaxStickInputAngleInfluence;
		float m_IdleYStartOffset;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	static float LimitRadianAngle2PI(float fLimitAngle);

	static Vector2 GetStickInputForCoverSearch(const CPed& ped);
	static bool VehicleHasCoverBounds(const CVehicle& rVeh);
	static bool CanPedBeInCoverOnEntity(const CPed& rPed, const CEntity* pEnt, bool& bIsVehicleThatDoesntProvideCover);

	// Returns true if it hits something
	static bool DoLineTestAgainstVehicleDummyBounds(const CPed& rPed, const CVehicle& vehicle, const Vector3& vStart, const Vector3& vEnd, Vector3& vIntersectionPos);
	static const phInst* DoLineProbeTest(const Vector3& vStart, const Vector3& vEnd, Vector3& vIntersectionPos, s32* pHitComponentIndex = NULL, bool* pbHitStairs = NULL, bool bAllowNotCover = false, Vector3* vIntersectionNormal = NULL);
	static const phInst* DoCapsuleTest(const Vector3& vStart, const Vector3& vEnd, Vector3& vIntersectionPos, float fCapsuleRadius = ms_Tunables.m_OCMClearanceCapsuleRadius);
	static const phInst* IsThereAnObstructionsBetweenPedAndCover(const CPed& ped, const Vector3& vCoverPos);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Search Modes
	enum eSearchMode
	{
		eInvalidSearchMode,
		eFindNewCover,
		eUpdateCoverWhileMoving,
		eUpdateCoverWhileIdle,
		eTestForMovingAroundCorner,
		eTestForInsideCornerTransition,
		eTestForCoverToCover
	};

	// PURPOSE: FSM states
	enum
	{
		State_Start = 0,
		State_FindNewCover,
		State_UpdateCoverWhileMoving,
		State_UpdateCoverWhileIdle,
		State_TestForMovingAroundCorner,
		State_TestForInsideCornerTransition,
		State_TestForCoverToCover,
		State_Finished
	};

	////////////////////////////////////////////////////////////////////////////////

	CDynamicCoverHelper();
	~CDynamicCoverHelper();

	// PURPOSE: Clear if ped gets recreated
	void Reset();

	// PURPOSE: Set the search mode to find an initial cover point
	bool FindNewCoverPoint(CCoverPoint* pNewCoverPoint, CPed* pPed, const Vector3& vTestPos, float fOverrideDir = -999.0f, bool bIgnoreOldCoverDir = false);

	// PURPOSE: Set the search mode to update the current cover point
	bool UpdateCoverPoint(const CCoverPoint* pOldCoverPoint, CCoverPoint* pNewCoverPoint, CPed* pPed, const Vector3& vTestPos, bool bFromIdle = false, bool bFacingLeft = false, bool bFromIdleTurn = false);

	// PURPOSE: Find a new cover point around the corner of an edge
	bool FindNewCoverPointAroundCorner(CCoverPoint* pNewCoverPoint, CPed* pPed, bool bFacingLeft = false, bool bCrouched = false, bool bIgnoreClearCheck = false);

	// PURPOSE: Find a new cover point for an inside corner transition
	bool FindNewCoverPointForInsideCornerTransition(CCoverPoint* pNewCoverPoint, CPed* pPed, bool bFacingLeft, bool bCrouched);

	// PURPOSE: Find a new cover point in front of the peds current position
	bool FindNewCoverToCoverPoint(CCoverPoint* pNewCoverPoint, CPed* pPed, bool bFacingLeft = false, bool bCrouched = false);

	// PURPOSE: Did the last search find a dynamic cover point?
	bool FoundDynamicCoverLastSearch() const { return m_bFoundDynamicCover; }

	// PURPOSE: Update Inside corner info
	bool DetectObstructionsInFrontOfPed(bool bFacingLeft);

	// PURPOSE: Get the entity we're taking cover against
	CEntity* GetCoverEntryEntity() const { return m_pCoverEntryEntity; }
	s32 GetActiveVehicleCoverBoundIndex() const { return m_iActiveBoundIndex; }

	bool ProcessRoundCover(CPed* pPed);
	void ResetRoundCoverTest();
	Vector3 ProcessRoundCoverDirection(const CPed* pPed, const Vector3& vCenter, const Vector3* pvCollisionPos = NULL);

	// PURPOSE: Get the material of the cover we're entering
	CollisionMaterialSettings* GetCoverEntryMaterial() const { return m_pCoverEntryMaterial; }
	void SetCoverEntryEntity(CEntity* pEntity);
	Vector3 GetCoverEntryEntityCachedPosition() const { return m_vCachedCoverEntryEntityPosition; }

	float ComputeInitialSearchDirection(CPed* pPed);

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return FindNewCover_OnUpdate();
	FSM_Return UpdateCoverWhileMoving_OnUpdate();
	FSM_Return UpdateCoverWhileIdle_OnUpdate();
	FSM_Return TestForMovingAroundCorner_OnUpdate();
	FSM_Return TestForInsideCornerTransition_OnUpdate();
	FSM_Return TestForCoverToCover_OnUpdate();
	FSM_Return Finished_OnEnter();
	FSM_Return Finished_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

private:

	////////////////////////////////////////////////////////////////////////////////

	// Line probe / capsule test info
	struct sShapeTestInfo 
	{ 
		Vector3 vStart; 
		Vector3 vEnd; 
		float fCapsuleRadius; 
	};

	CVehicle* GetCoverVehicle(const CPed& ped);

	void SetSegment( s32 iProbe,
					 Mat34V_ConstRef testMtx, 
					 Vec3V_ConstRef vStart,
					 Vec3V_ConstRef vEnd,
					 float fExtendedCheckLength,
					 phSegment* aSegments,
					 float fHeightOffset);

	void DoVehicleCoverBoundBoxTests(	Mat34V_ConstRef testMtx, 
							const CVehicle& vehicle,
							s32 iMaxShapeTests,
							float fExtendedCheckLength,
							sShapeTestInfo* aShapeTestInfos, 
							phIntersection* aIntersections, 
							phSegment* aSegments,
							ProbeResults* apProbeResults,
							s32& iActiveBoundIndex,
							const CPed* pPed = NULL, 
							bool bUseClosestProbeForActiveIndex = false);

	void DoVehicleBoxTests(	Mat34V_ConstRef testMtx, 
							const CVehicle& vehicle,
							s32 iMaxShapeTests,
							float fExtendedCheckLength,
							sShapeTestInfo* aShapeTestInfos, 
							phIntersection* aIntersections, 
							phSegment* aSegments,
							ProbeResults* apProbeResults);

	void DetermineIfCoverShouldBeLow( Mat34V_ConstRef testMtx, 
							sShapeTestInfo* aShapeTestInfos,
							phIntersection* aIntersections);

	void DoBatchShapeTests(	Mat34V_Ref testMtx, 
							CollisionTestType overrideMainTestType,
							s32 iMaxShapeTests,
							float fExtendedCheckLength,
							sShapeTestInfo* aShapeTestInfos, 
							phIntersection* aIntersections, 
							phSegment* aSegments,
							ProbeResults* apProbeResults,
							float fTestLength = 2.5f);

	void DoCoverToCoverBatchShapeTests(	float fDirection,
										Mat34V_Ref testMtx, 
										s32 iMaxShapeTests,
										sShapeTestInfo* aShapeTestInfos, 
										phIntersection* aIntersections, 
										phSegment* aSegments,
										ProbeResults* apProbeResults);

	
	bool IsAreaAroundCornerClear(CPed* pPed, CVehicle* pVehicle);
	bool FindNearbyCollisionAndComputeTestPosition(CPed* pPed);

	void DoInitialCollisionCheck(CPed* pPed, Vector3& vStartPos, float fDir, ProbeResults* apProbeResults, CollisionTestType overrideMainTestType = CTT_Unknown, float fTestLength=2.5f);
	void DoCollisionCheckForMovementInCover(CPed* pPed, Vector3& vStartPos, float fDir, ProbeResults* apProbeResults, CollisionTestType testType, bool bFacingLeft);
	void DoCollisionCheckWhenIdleInCover(CPed* pPed, Vector3& vStartPos, float fDir, ProbeResults* apProbeResults, CollisionTestType overrideMainTestType = CTT_Unknown, float fTestLength=2.5f);
	// PURPOSE: Returns true if cover is found and fills the normal into vNormal
	bool CheckForCoverAndCalculateNormalWhenMoving(const CPed& ped,  ProbeResults* apProbeResults, Vector3& vPedPosition, Vector3& vNormal, Vector3& vCoverPosition, s32 iNumRequiredConcurrentCollisions, float fSearchDir);
	bool CheckForCoverAndCalculateNormal(const CPed& ped, ProbeResults* apProbeResults, Vector3& vPedPosition, Vector3& vNormal, Vector3& vCoverPosition, s32 iNumRequiredConcurrentCollisions, float fSearchDir, bool bCalledWhilstAlreadyInCover, bool bIsCoverToCover = false, bool bIsFinalCover = false, Vector3* pRoundCoverCenter = NULL, float* pfRoundCoverRadius = NULL);

	void ComputeTestMatrix(CPed* pPed, Vector3& vStartPos, float fDir, Mat34V& testMtx, const Vector3* vGroundNormalOverride = NULL);

	bool RemoveFarEdgeResults(ProbeResults* apProbeResults, bool bCheckForCorners);
	void RemoveFarEdgeResultsWhenMoving(ProbeResults* apProbeResults);
	void RemoveConflictingNormalResults(ProbeResults* apProbeResults);
	bool IsOutsideAngularRange(const Vector3& vNormal, const Vector3& vOtherNormal, s32 iNumNormals);
	bool IsOutsideDepthRange(const float& fDepth, const float& fOtherDepth, s32 iNumDepths);

	float GetCapsuleRadiusFromPed(const CPed& ped) const;
	float GetPedRootToGroundOffsetFromPed(const CPed& ped) const;

	bool CheckForRoundCover(const phIntersection* pIntersection, Vector3& vRoundCoverCenter, float& fRadius);

	void FindConcurrentCollisions( ProbeResults* apProbeResults, 
		Vector3 &vCoverPosition, 
		s32 &iNumCollisions, 
		float &fNearestCollision, 
		Vector3 &vNormalFromCollisionNormal,
		float &fLowDepth, 
		float &fMidDepth, 
		float &fHighDepth, 
		s32 &iMaxNumLowConcurrentIntersections, 
		Vector3 &vNormalFromLowPosition, 
		s32 &iMaxNumHighConcurrentIntersections, 
		Vector3 &vNormalFromHighPosition );

	void FindConcurrentCollisionsWhenMoving( ProbeResults* apProbeResults, 
		Vector3 &vCoverPosition, 
		s32 &iNumCollisions, 
		float &fNearestCollision, 
		Vector3 &vNormalFromCollisionNormal,
		float &fLowDepth, 
		float &fMidDepth, 
		float &fHighDepth, 
		s32 &iMaxNumLowConcurrentIntersections, 
		Vector3 &vNormalFromLowPosition, 
		s32 &iMaxNumHighConcurrentIntersections, 
		Vector3 &vNormalFromHighPosition );

	bool DetectAndRemoveSlopes( float fLowDepth, float fMidDepth, float fHighDepth, s32 iMaxNumLowConcurrentIntersections, s32 &iMaxNumHighConcurrentIntersections, ProbeResults* apProbeResults );
	float ComputeSearchDirectionFromCoverPoint(CPed* pPed);
	void UpdateCoverPointFromResults(CPed* pPed, ProbeResults* apProbeResults, Vector3& vNormal, Vector3& vCoverPosition, bool bMoving, bool bCovToCov = false);
	bool IsNewCoverToCoverPointValid(const CPed& ped, const CCoverPoint& coverPoint) const;

private:

	////////////////////////////////////////////////////////////////////////////////

	static bank_float ms_fCenterCPWidth;
	static bank_float ms_fCenterCPWidth2;
	static bank_float ms_fCenterYOffset;
	static bank_float ms_fOuterCPWidth;
	static bank_float ms_fOuterCPWidth2;
	static bank_float ms_fOuterYOffset;

	// Moving in cover
	static bank_float ms_fMovingCenterBackwardCPWidth;
	static bank_float ms_fMovingCenterForwardCPWidth;
	static bank_float ms_fMovingInnerForwardCPWidth;
	static bank_float ms_fMovingInnerForwardCPWidth2;
	static bank_float ms_fMovingOuterForwardCPWidth;
	static bank_float ms_fMovingOuterForwardCPWidth2;
	static bank_float ms_fYInnerStartOffset;
	static bank_float ms_fYInnerEndOffset;
	static bank_float ms_fYOuterStartOffset;
	static bank_float ms_fYOuterEndOffset;
	
	static bank_float ms_fCapsuleRadius;
	static bank_float ms_fExtraLowCapsuleRadius;
	static bank_float ms_fExtraLowHeight;

	static bank_float ms_fObstructionTestHeight;
	static bank_float ms_fObstructionTestHeadingTol;
	static bank_float ms_fOptimumDistToLeftInsideEdge;
	static bank_float ms_fOptimumDistToRightInsideEdge;
	static bank_float ms_fOptimumDistToLeftInsideEdgeCrouched;
	static bank_float ms_fOptimumDistToRightInsideEdgeCrouched;
	static bank_float ms_fMinInsideEdgeOffset;
	static bank_float ms_fMaxInsideEdgeOffset;

	typedef enum
	{
		MLP_CenterBackwardLow,
		MLP_CenterLow,
		MLP_CenterForwardLow,
		MLP_InnerForwardLow,
		MLP_OuterForwardLow,
		MLP_CenterBackwardHigh,
		MLP_CenterHigh,
		MLP_CenterForwardHigh,
		MLP_InnerForwardHigh,
		MLP_OuterForwardHigh,
		MLP_LowCentre,
		MLP_Max
	} MovingLineProbes;

	typedef enum
	{
		ILP_LeftLow,
		ILP_FrontLowL,
		ILP_FrontLowC,
		ILP_FrontLowR,
		ILP_RightLow,
		ILP_LeftHigh,
		ILP_FrontHighL,
		ILP_FrontHighC,
		ILP_FrontHighR,
		ILP_RightHigh,
		ILP_LowCentre,
		ILP_Max
	} IdleLineProbes;

#if __BANK
	static const char* ms_IdleLineProbeStrings[];
	static const char* ms_MovingLineProbeStrings[];
#endif

	typedef enum
	{
		FOLP_LeftLow,
		FOLP_RightLow,
		FOLP_LeftHigh,
		FOLP_RightHigh,
		FOLP_Max
	} FrontObstructionLineProbes;

	typedef enum
	{
		CTCP_First,
		CTCP_Second,
		CTCP_Third,
		CTCP_Fourth,
		CTCP_Fifth,
		CTCP_Sixth,
		CTCP_Max
	} CoverToCoverProbes;

	struct sProbeInfo
	{ 
		Vector3 vStart; 
		Vector3 vEnd; 
	};

	s32	 m_iActiveBoundIndex;
	bool m_bFacingLeft : 1;
	bool m_bSearchStarted : 1;
	bool m_bFoundDynamicCover : 1;
	bool m_bCrouched : 1;	
	bool m_bFromIdleTurn : 1;
	bool m_bIgnoreClearCheck : 1;

#if __BANK
	bool m_bIsDebugMode : 1;
	float m_fDebugSearchHeading;
#endif

	eSearchMode m_eSearchMode;
	RegdPed m_pPed;
	Vector3 m_vTestPos;
	const CCoverPoint* m_pOldCoverPoint;
	CCoverPoint* m_pNewCoverPoint;
	float m_fOverrideDir;
	float m_fLastTestZHeight;
	bool m_bIgnoreOldCoverDir;
	Vector3 m_vCachedGroundNormal;

	// Asynchronous probe result storage
	typedef enum
	{
		RCP_Mid,
		//RCP_Left,
		//RCP_Right,
		//RCP_Rear,		
		RCP_Max
	} RoundCoverProbes;
	WorldProbe::CShapeTestResults m_RoundCoverCapsuleResults[RCP_Max];

	// PURPOSE: The entity and material we're entering cover against
	RegdEnt m_pCoverEntryEntity;
	Vector3 m_vCachedCoverEntryEntityPosition;
	CollisionMaterialSettings* m_pCoverEntryMaterial;

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

#if DEBUG_DRAW
	// PURPOSE: Render the cover search direction
	static void RenderSearchDirection(const CPed& ped, float fSearchDirection, Color32 colour, bool bOffset = false);

	// PURPOSE: Render the initial cover probes
	static void RenderSearchProbes(const CPed& ped, float fSearchDirection);

	// PURPOSE: Render the test result
	static void RenderCollisionResultNew(Vec3V_ConstRef vStart, Vec3V_ConstRef vEnd, Vec3V_ConstRef vIntersectionPos, CollisionTestType testType, ProbeResults* apProbeResults, s32 iResultIndex);
	static void RenderCollisionResult(Vec3V_ConstRef vStart, Vec3V_ConstRef vEnd, CollisionTestType testType, ProbeResults* apProbeResults, s32 iResultIndex, bool bMoving, const char* szName = NULL);
	static void RenderNewCoverPositionAndNormal(Vec3V_ConstRef vPos, Vec3V_ConstRef vNorm, bool bLos);

	// PURPOSE: Render the normal computed from the collisions
	static void RenderCollisionNormal(Vec3V_ConstRef vPos, Vec3V_ConstRef vNormal, bool bLow); 

	// PURPOSE: Render the cover normal (collision normal rotated 90 degs)
	static void RenderCoverNormal(Vec3V_ConstRef vPos, Vec3V_ConstRef vNormal, bool bLow); 

	// PURPOSE: Render a cover edge computed from the collision
	static void RenderCoverEdge(Vec3V_ConstRef vPos1, Vec3V_ConstRef vPos2, s32 iResultIndex);

	// PURPOSE: Clear all cover edge debug
	static void ClearCoverEdgeDebug();

	// PURPOSE: Render a cover turn point
	static void RenderCoverTurnPoint(ProbeResults* apProbeResults, s32 iTurnPointNumber, s32 iResultIndex, s32 iPreTurnEdges, s32 iPostTurnEdges);

	// PURPOSE: Clear all cover turn point debug
	static void ClearCoverTurnPointDebug();

#endif // DEBUG_DRAW

	// PURPOSE: Display debug information specific to this task
	virtual void		Debug(s32 UNUSED_PARAM(iIndentLevel)) const { }

	// PURPOSE: Get the name of the specified state
	static const char* GetStaticStateName( s32 iState );
#endif // !__FINAL

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Helper class to process the rotational and/or translational
// velocity scaling of 1 or more anim clips being played via a move network
// to nail a specific heading/position
////////////////////////////////////////////////////////////////////////////////

class CClipScalingHelper
{
public:

	// Supports two blended clips currently, to support more we 'should' just need to add 
	// more ids and update the getters for blend weight / phase / clip
	static const u32  ms_uMaxNumBlendedClips = 2;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		bool m_DisableRotationScaling;
		bool m_DisableRotationOvershoot;
		bool m_DisableTranslationScaling;
		bool m_DisableTranslationOvershoot;
		float m_MinAnimRotationDeltaToScale;
		float m_MinAnimTranslationDeltaToScale;
		float m_MinCurrentRotationDeltaToScale;
		float m_MinRemainingAnimDurationToScale;
		float m_MinVelocityToScale;
		float m_MaxTransVelocity;
		float m_DefaultMinRotationScalingValue;
		float m_DefaultMaxRotationScalingValue;
		float m_DefaultMinTranslationScalingValue;
		float m_DefaultMaxTranslationScalingValue;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;

	// To make this helper generic, we need to conform to a naming convention for our parameters
	static fwMvClipId	ms_Clip0Id;
	static fwMvClipId	ms_Clip1Id;
	static fwMvClipId	ms_Clip2Id;
	static fwMvClipId	ms_Clip3Id;
	static fwMvClipId	ms_Clip4Id;
	static fwMvClipId	ms_Clip5Id;
	static fwMvFloatId	ms_Clip0PhaseId;
	static fwMvFloatId	ms_Clip1PhaseId;
	static fwMvFloatId	ms_Clip2PhaseId;
	static fwMvFloatId	ms_Clip0BlendWeightId;
	static fwMvFloatId	ms_Clip1BlendWeightId;
	static fwMvFloatId	ms_Clip2BlendWeightId;
	static fwMvBooleanId ms_Clip0FinishedId;
	static fwMvBooleanId ms_Clip1FinishedId;

	struct sScalingParams
	{
		sScalingParams(const CMoveNetworkHelper& MoveNetworkHelper,
							   float& rotationalVelocity,
							   Vector3& translationalVelocity,
							   float currentTimeStep,
							   float previousPhysicsTimestep,
							   float targetHeading,
							   float currentheading,
							   const Vector3& targetPosition,
							   const Vector3& currentPosition,
							   const Vector3& originalPosition,
							   u32 numClips,
							   bool approachLeft,
							   bool& orientationReached,
							   bool& positionReached,
							   float& translationScalingValue)
							   : moveNetworkHelper(MoveNetworkHelper)
							   , fRotationalVelocity(rotationalVelocity)
							   , vTranslationalVelocity(translationalVelocity)
							   , fCurrentTimeStep(currentTimeStep)
							   , fPreviousPhysicsTimestep(previousPhysicsTimestep)
							   , fTargetHeading(targetHeading)
							   , fCurrentHeading(currentheading)
							   , vTargetPosition(targetPosition)
							   , vCurrentPosition(currentPosition)
							   , vOriginalPosition(originalPosition)
							   , uNumClips(numClips)
							   , bApproachLeft(approachLeft)
							   , bOrientationReached(orientationReached)
							   , bPositionReached(positionReached)
							   , fTranslationScalingValue(translationScalingValue)
		{

		}

		const CMoveNetworkHelper& moveNetworkHelper;
		float&					  fRotationalVelocity;
		Vector3&				  vTranslationalVelocity;
		float					  fCurrentTimeStep;
		float					  fPreviousPhysicsTimestep;
		float					  fTargetHeading;
		float					  fCurrentHeading;
		Vector3					  vTargetPosition;
		Vector3					  vCurrentPosition;
		Vector3					  vOriginalPosition;
		u32						  uNumClips;
		bool					  bApproachLeft;
		bool&					  bOrientationReached;
		bool&					  bPositionReached;
		float&					  fTranslationScalingValue;
	};

	// PURPOSE: Main function which passes all the necessary information to achieve scaling
	static void				ProcessScaling(const sScalingParams& scalingParams);

	// PURPOSE: Utility functions to get output params from the move network for each clip being scaled
	static float 			GetBlendWeight(const CMoveNetworkHelper& moveNetworkHelper, s32 i);
	static float 			GetPhase(const CMoveNetworkHelper& moveNetworkHelper, s32 i);
	static const crClip*	GetClip(const CMoveNetworkHelper& moveNetworkHelper, s32 i);
	static fwMvClipId		GetClipId(s32 i);

	// PURPOSE: Stores important output params from the move network and obtains scale tagging info from each clip
	static float			CacheMoveParamsAndComputeTotalBlendWeight(const sScalingParams& scalingParams, 
											atArray<float>& afCachedBlendWeights,
											atArray<float>& afCachedPhases,
											atArray<const crClip*>& apCachedClips,
											atArray<float>& afRotationScalingStartPhases,
											atArray<float>& afRotationScalingEndPhases,
											atArray<float>& afTranslationScalingStartPhases,
											atArray<float>& afTranslationScalingEndPhases);

	// PURPOSE: Ensure blend weights sum to 1 and determine the most blended clip
	static u32				NormaliseBlendWeightsAndComputeDominantClip(atArray<float>& afCachedBlendWeights, float fTotalWeight, u32 uNumClips);
	
	// PURPOSE: Compute the overall start and end scaling phases based on the blends of each clip
	static void				ComputeScalingPhases(float& fStartScalingPhase, 
												 float& fEndScalingPhase,
												 const atArray<float>& afCachedBlendWeights,
												 const atArray<float>& afScalingStartPhases, 
												 const atArray<float>& afScalingEndPhases, 
												 u32 uNumClips);

	// PURPOSE: Apply rotation scaling and overshoot checks
	static void				ProcessRotationalScaling(const sScalingParams& scalingParams, 
													 const atArray<float>& afCachedWeights, 
													 const atArray<float>& afCachedPhases, 
													 const atArray<const crClip*>& apCachedClips, 
													 const atArray<float>& afRotationScalingStartPhases, 
													 const atArray<float>& afRotationScalingEndPhases, 
													 u32 uDominantClip);

	// PURPOSE: Apply translation scaling and overshoot checks
	static void				ProcessTranslationalScaling(const sScalingParams& scalingParams, 
														const atArray<float>& afCachedWeights, 
														const atArray<float>& afCachedPhases, 
														const atArray<const crClip*>& apCachedClips, 
														const atArray<float>& afTranslationScalingStartPhases, 
														const atArray<float>& afTranslationScalingEndPhases, 
														u32 uDominantClip);

	// PURPOSE: Computes a new active phase and timestep to use if we're crossing tag boundaries during the update
	static void				ComputeActivePhaseAndTimeStep(float& fActivePhase, 
														  float& fActiveTimestep, 
														  float fScalingStartPhase,
														  float fScalingEndPhase,
														  const crClip* pDominantClip);


	// PURPOSE: Determine the amount of rotation (about z axis) remaining before and after the scaling phases 
	//			in radians for all clips based on the current phases and blend weights.
	static void				ComputeRemainingRotations(float fEndScalingPhase,
													  float& fRemainingRotationToEndOfScaling,
													  float& fRemainingRotationFromEndOfScaling,
													  const atArray<float>& afCachedBlendWeights,
													  const atArray<float>& afCachedPhases,
													  const atArray<const crClip*>& apCachedClips,
													  float fPreviousTimestep,
													  u32 uNumClips);

	// PURPOSE: Determine the amount of translation remaining before and after the scaling phases 
	//			for all clips based on the current phases and blend weights.
	static void				ComputeRemainingTranslations(float fEndScalingPhase,
														 float& fRemainingTranslationToEndOfScaling,
														 float& fRemainingTranslationFromEndOfScaling,
														 const atArray<float>& afCachedBlendWeights,
														 const atArray<float>& afCachedPhases,
														 const atArray<const crClip*>& apCachedClips,
														 float fPreviousTimestep,
														 u32 uNumClips);

	// PURPOSE: Given the current rotational velocity, the remaining rotation left to the end of the scaling phase
	//			we compute and apply a scaling value to fixup the remaining rotation delta before the end of scaling.
	//			After the scaling period we distribute any remaining error over the remaining anim duration
	static void				ScaleRotationalVelocity(float& fRotationalVelocity,
													float fRemainingRotationToEndOfScaling,
													float fDeltaToFixUp,
													float fRemainingTime);

	// PURPOSE: Given the current Translational velocity, the remaining Translation left to the end of the scaling phase
	//			we compute and apply a scaling value to fixup the remaining Translation delta before the end of scaling.
	//			After the scaling period we distribute any remaining error over the remaining anim duration
	static void				ScaleTranslationalVelocity(const sScalingParams& scalingParams,
													   float& fTranslationalVelocity,
													   float  fDistInClipBlend,
													   float  fDistToTarget,
													   float  fRemainingTime);

};

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
// A class used to contain debug functionality for cover
class CCoverDebug
{
public:

	static atHashWithStringNotFinal DEFAULT;
	static atHashWithStringNotFinal INITIAL_SEARCH_SIMPLE;
	static atHashWithStringNotFinal STATIC_COVER_EVALUATION;
	static atHashWithStringNotFinal STATIC_COVER_POINTS;
	static atHashWithStringNotFinal FIRST_DYNAMIC_COLLISION_LOS_TESTS;
	static atHashWithStringNotFinal FIRST_DYNAMIC_COLLISION_CAPSULE_TESTS;
	static atHashWithStringNotFinal SECOND_DYNAMIC_COLLISION_LOS_TESTS;
	static atHashWithStringNotFinal SECOND_DYNAMIC_COLLISION_CAPSULE_TESTS;
	static atHashWithStringNotFinal FIRST_STATIC_COLLISION_LOS_TESTS;
	static atHashWithStringNotFinal FIRST_STATIC_COLLISION_CAPSULE_TESTS;
	static atHashWithStringNotFinal LEFT_EDGE_TEST;
	static atHashWithStringNotFinal RIGHT_EDGE_TEST;
	static atHashWithStringNotFinal EDGE_TEST_RESULTS;
	static atHashWithStringNotFinal INSIDE_EDGE_TEST;
	static atHashWithStringNotFinal CAPSULE_CLEARANCE_TESTS;
	static atHashWithStringNotFinal THREATS;

	struct sStaticCoverDebug
	{
		sStaticCoverDebug() {};
		sStaticCoverDebug(	const Vector3& vPos, 
							const Vector3& vDir, 
							float fScore, 
							float fDistScore, 
							float fDirToCoverScore,
							float fDirOfCoverScore,
							float fEdgeScore,
							float fAngleScore,
							float fAngleToCameraScore)
			: vCoverPos(vPos)
			, vCoverDir(vDir)
			, fCoverScore(fScore)
			, fDistanceScore(fDistScore)
			, fDirectionToCoverScore(fDirToCoverScore)
			, fDirectionOfCoverScore(fDirOfCoverScore)
			, fEdgeCoverScore(fEdgeScore)
			, fAngleToDynamicCoverScore(fAngleScore)
			, fAngleToCameraCoverScore(fAngleToCameraScore)
		{

		}

		Vector3 vCoverPos;
		Vector3 vCoverDir;
		float	fCoverScore;
		float	fDistanceScore;
		float	fDirectionToCoverScore;
		float	fDirectionOfCoverScore;
		float   fEdgeCoverScore;
		float	fAngleToDynamicCoverScore;
		float	fAngleToCameraCoverScore;
	};

	struct sContextDebugInfo
	{
		virtual ~sContextDebugInfo() {};

		atHashWithStringNotFinal	m_Name;
		atArray<sContextDebugInfo>	m_SubContexts;
		s32							m_MaxNumDebugDrawables;
#if DEBUG_DRAW
		CDebugDrawStore				m_DebugDrawStore;
#endif  // DEBUG_DRAW

		sContextDebugInfo*	FindDebugSubContextByHash(u32 uHash);
		void				Init();
		void				Render();

		PAR_PARSABLE;
	};

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		struct BoundInfo
		{
			bool	m_RenderCells;
			bool	m_RenderBoundingAreas;
			bool	m_RenderBlockingAreas;
			bool	m_BlockObject;
			bool	m_BlockVehicle;
			bool	m_BlockMap;
			bool	m_BlockPlayer;
			float	m_BlockingBoundHeight;
			float	m_BlockingBoundGroundExtraOffset;

			PAR_SIMPLE_PARSABLE;
		};

		BoundInfo	m_BoundingAreas;

#if DEBUG_DRAW
		CDebugDrawStore& 	FindDebugContextByHash(u32 uHash);
		CDebugDrawStore* 	FindDebugSubContextByHash(u32 uHash);
#endif  // DEBUG_DRAW

		sContextDebugInfo	m_DefaultCoverDebugContext;
		s32					m_CurrentSelectedContext;
		float				m_CoverModifier;
		bool				m_RenderOtherDebug;
		bool				m_EnableDebugDynamicCoverFinder;
		bool				m_RenderCoverProbeNames;
		bool				m_RenderEdges;
		bool				m_RenderTurnPoints;
		bool				m_RenderAimOutroTests;
		bool				m_RenderCornerMoveTestPos;
		bool				m_RenderBatchProbeOBB;
		bool				m_RenderPlayerCoverSearch;
		bool				m_RenderInitialSearchDirection;
		bool				m_RenderInitialCoverProbes;
		bool				m_RenderCapsuleSearchDirection;
		bool				m_RenderCoverCapsuleTests;
		bool				m_RenderCollisions;
		bool				m_RenderNormals;
		bool				m_RenderCoverPoint;
		bool				m_RenderDebug;
		bool				m_RenderCoverMoveHelpText;
		bool				m_RenderCoverInfo;
		bool				m_RenderCoverPoints;
		bool				m_RenderCoverPointHelpText;
		bool				m_RenderCoverPointAddresses;
		bool				m_RenderCoverPointArcs;
		bool				m_RenderCoverPointHeightRulers;
		bool				m_RenderCoverPointUsage;
		bool				m_RenderCoverPointTypes;
		bool				m_RenderCoverPointLowCorners;
		bool				m_RenderSearchDirectionVec;
		bool				m_RenderProtectionDirectionVec;
		bool				m_RenderCoverArcSearch;
		bool				m_RenderCoverPointScores;
		bool				m_OutputStickInputToTTY;
		bool				m_EnableTaskDebugSpew;
		bool				m_EnableMoveStateDebugSpew;
		bool				m_UseRagValues;
		bool				m_EnableNewCoverDebugRendering;
		bool				m_DebugSearchIgnoreMovement;
		s32 				m_TextOffset;
		s32 				m_DefaultRenderDuration;
		s32					m_LastCreatedCoverPoint;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable params
	static Tunables			ms_Tunables;

	// Debug rendering

#if DEBUG_DRAW
	static Vec3V GetGamePlayCamPosition();
	static Vec3V GetGamePlayCamDirection();
	static bool IsContextValid(s32 iContext);
	static CDebugDrawStore& GetDebugContext(u32 uHash) { return ms_Tunables.FindDebugContextByHash(uHash); }

	struct sDebugCommonParams
	{
		sDebugCommonParams() 
			: color(Color_black)
			, szName("NO NAME")
			, iDuration(-1)
			, bAddText(false)
			, iYTextOffset(0)
			, uContextHash(DEFAULT)
		{

		}

		sDebugCommonParams(Color32 col, const char* name, const bool addText, const s32 YTextOffset, const u32 context) 
			: color(col)
			, szName(name)
			, iDuration(-1)
			, bAddText(addText)
			, iYTextOffset(YTextOffset)
			, uContextHash(context)
		{

		}

		Color32 color;
		const char* szName;
		s32 iDuration;
		bool bAddText;
		s32 iYTextOffset;
		u32 uContextHash;
	};

	struct sDebugLineParams : public sDebugCommonParams
	{
		sDebugLineParams() 
			: sDebugCommonParams()
			, vStart(V_ZERO)
			, vEnd(V_ZERO)
		{

		}

		Vec3V vStart;
		Vec3V vEnd;
	};

	struct sDebugCapsuleParams : public sDebugCommonParams
	{
		sDebugCapsuleParams() 
			: sDebugCommonParams()
			, vStart(V_ZERO)
			, vEnd(V_ZERO)
			, fRadius(0.0f)
		{

		}

		Vec3V vStart;
		Vec3V vEnd;
		float fRadius;
	};

	struct sDebugSphereParams : public sDebugCommonParams
	{
		sDebugSphereParams() 
			: sDebugCommonParams()
			, vPos(V_ZERO)
			, fRadius(0.25f)
		{

		}

		Vec3V vPos;
		float fRadius;
	};

	struct sDebugArrowParams : public sDebugCommonParams
	{
		sDebugArrowParams() 
			: sDebugCommonParams()
			, vPos(V_ZERO)
			, vDir(V_ZERO)
		{

		}

		sDebugArrowParams(Vec3V_ConstRef pos, Vec3V_ConstRef dir, Color32 col, const char* name, const bool addText, const s32 iYOffset, const s32 iContext) 
			: sDebugCommonParams(col, name, addText, iYOffset, iContext)
			, vPos(pos)
			, vDir(dir)
		{

		}

		Vec3V vPos;
		Vec3V vDir;
	};

	static void AddDebugLine(const sDebugLineParams& rLineParams);
	static void AddDebugDirectionArrow(const sDebugArrowParams& rArrowParams);
	static void AddDebugPositionSphere(const sDebugSphereParams& rSphereParams);
	static void AddDebugCapsule(const sDebugCapsuleParams& rCapsuleParams);
	static void AddDebugText(Vec3V_ConstRef vPos, Color32 color, const char* szName, s32 iDuration = -1, s32 iYTextOffset = 0, u32 uContextHash = CCoverDebug::DEFAULT, bool bUnique = false);
	static void AddDebugPositionSphereAndDirection(Vec3V_ConstRef vPos, Vec3V_ConstRef vDir, Color32 color, const char* szName, s32 iYTextOffset = 0, u32 uContextHash = CCoverDebug::DEFAULT, bool bRenderMainText = true);

	static bool		ms_bInitializedDebugDrawStores;
	static CDebugDrawStore ms_debugDraw;
	static CDebugDrawStore ms_searchDebugDraw;
#endif // DEBUG_DRAW

	static CDynamicCoverHelper ms_DynamicCoverHelper;

	// Other
	enum eInitialCheckType
	{
		FIRST_DYNAMIC_CHECK,
		SECOND_DYNAMIC_CHECK,
		FIRST_STATIC_CHECK
	};

	static eInitialCheckType ms_eInitialCheckType;

#if __BANK
	static bkGroup* FindSubGroupInBank(bkBank& rBank, const char* szGroupToFind);
	static void		InitWidgets();
	static void		Debug();
	static void		CreateCoverBlockingAreaCB();
	static void		RemoveAllCoverBlockingAreasCB();
	static void		RemoveCoverBlockingAreaAtPlayerPosCB();
	static void		RemoveSpecificCoverBlockingArea();
	static void		CreateCoverPointAtLocationCB();
	static void		DoesCoverPointProvideCoverCB();
	static void		SetNearestCoverPointAsHighPriorityCB();
	static void		ClearNearestCoverPointAsHighPriorityCB();
	static void		CheckIfScriptedCoverPointExistsCB();
	static void		SetNearestCoverPointStatusPositionBlockedCB();
#endif // __BANK

};
#endif // __FINAL


#endif // INC_TASK_COVER_H
