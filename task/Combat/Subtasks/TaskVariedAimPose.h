// FILE :    TaskVariedAimPose.h
// PURPOSE : Combat subtask for varying a ped's aim pose
// CREATED : 09-21-2011

#ifndef TASK_VARIED_AIM_POSE_H
#define TASK_VARIED_AIM_POSE_H

// Game headers
#include "peds/QueriableInterface.h"
#include "physics/WorldProbe/shapetestresults.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "Task/System/TaskFSMClone.h"

// Forward declarations
class CEventGunAimedAt;
class CEventGunShotBulletImpact;
class CEventGunShotWhizzedBy;
class CEventShoutBlockingLos;

////////////////////////////////////////////////////////////////////////////////
// CTaskVariedAimPose
////////////////////////////////////////////////////////////////////////////////

class CTaskVariedAimPose : public CTaskFSMClone
{

public:

	enum State
	{
		State_Start = 0,
		State_Shoot,
		State_ChooseTransition,
		State_CheckNextTransition,
		State_StreamClipSet,
		State_ChooseClip,
		State_CheckNextClip,
		State_CheckLineOfSight,
		State_CheckNavMesh,
		State_CheckProbe,
		State_PlayTransition,
		State_Finish
	};

	enum ConfigFlags
	{
		CF_DisableNewPosesFromDefaultStandingPose			= BIT0,
		CF_DisableTransitionsExceptToDefaultStandingPose	= BIT1,
		CF_DisableTransitionsFromDefaultStandingPose		= BIT2,
	};
	
private:

	enum RunningFlags
	{
		RF_IsLineOfSightBlocked				= BIT0,
		RF_FailedToFixLineOfSight			= BIT1,
		RF_IsBlockingLineOfSight			= BIT2,
		RF_MovingAwayFromPosition			= BIT3,
		RF_DoesTransitionHaveSpecificClip	= BIT4,
	};

public:

	struct AimPose
	{
		struct Transition
		{
			enum Flags
			{
				OnlyUseForReactions				= BIT0,
				CanUseForReactions				= BIT1,
				Urgent							= BIT2,
				OnlyUseForLawEnforcementPeds	= BIT3,
				OnlyUseForGangPeds				= BIT4,
			};
			
			Transition()
			: m_ToPose()
			, m_ClipSetId(CLIP_SET_ID_INVALID)
			, m_ClipId(CLIP_ID_INVALID)
			, m_Rate(1.0f)
			, m_Flags(0)
			{}
			
			atHashWithStringNotFinal	m_ToPose;
			fwMvClipSetId				m_ClipSetId;
			fwMvClipId					m_ClipId;
			float						m_Rate;
			fwFlags8					m_Flags;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		AimPose()
		: m_Name()
		, m_IsCrouching(false)
		, m_IsStationary(false)
		, m_LoopClipSetId(CLIP_SET_ID_INVALID)
		, m_LoopClipId(CLIP_ID_INVALID)
		, m_Transitions()
		{}
		
		atHashWithStringNotFinal	m_Name;
		bool						m_IsCrouching;
		bool						m_IsStationary;
		fwMvClipSetId				m_LoopClipSetId;
		fwMvClipId					m_LoopClipId;
		atArray<Transition>			m_Transitions;
		
		PAR_SIMPLE_PARSABLE;
	};

	// PURPOSE: store indexes of the chosen poses/transitions/clips to serialize them over the network
	class CSerializableState
	{
	public:

		friend class CClonedVariedAimPoseInfo;

		static const u32 uMAX_AIM_POSES = 127; 
		static const u32 uMAX_AIM_POSE_TRANSITIONS = 127; 

		struct STransitionPlayStatus
		{
			enum Enum
			{
				UNDEFINED,
				REJECTED,
				ACCEPTED,
				MAX = ACCEPTED
			};
		};

		CSerializableState() 
			: m_iPoseIdx(-1), m_iTransitionIdx(-1), m_uTransitionClipId(CLIP_ID_INVALID), m_eTransitionPlayStatus(STransitionPlayStatus::UNDEFINED)
		{ /* ... */ }

		void Reset()
		{
			m_iPoseIdx = -1;
			m_iTransitionIdx = -1;
			m_uTransitionClipId = CLIP_ID_INVALID;
			m_eTransitionPlayStatus = STransitionPlayStatus::UNDEFINED;
		}

		bool IsTransitionReady() const
		{
			return m_iPoseIdx != -1 && m_iTransitionIdx != -1 && m_uTransitionClipId != CLIP_ID_INVALID; 
		}

		s32 GetPoseIdx () const { return m_iPoseIdx; }
		s32 GetTransitionIdx() const { return m_iTransitionIdx; }
		u32 GetTransitionClipId() const { return m_uTransitionClipId; }
		STransitionPlayStatus::Enum GetTransitionPlayStatus() const { return m_eTransitionPlayStatus; }

		void SetPoseIdx				 (s32 iPoseIdx)			 { m_iPoseIdx = taskVerifyf(iPoseIdx <= uMAX_AIM_POSES, "Invalid aim pose idx") ? iPoseIdx : -1;  }
		void SetTransitionIdx		 (s32 iTransitionIdx)	 { m_iTransitionIdx = taskVerifyf(iTransitionIdx <= uMAX_AIM_POSE_TRANSITIONS, "Invalid aim transition idx") ? iTransitionIdx : -1; }
		void SetTransitionClipId	 (u32 uTransitionClipId) { m_uTransitionClipId = uTransitionClipId; }
		void SetTransitionPlayStatus (STransitionPlayStatus::Enum eTransitionPlayStatus) { m_eTransitionPlayStatus = eTransitionPlayStatus; }

		void ClearTransition()			 { m_iTransitionIdx = -1; ClearTransitionClipId(); }
		void ClearTransitionClipId()	 { m_uTransitionClipId = CLIP_ID_INVALID; ClearTransitionPlayStatus(); }
		void ClearTransitionPlayStatus() { m_eTransitionPlayStatus = STransitionPlayStatus::UNDEFINED; }

	private:

		s32 m_iPoseIdx;
		s32 m_iTransitionIdx;
		u32 m_uTransitionClipId;
		STransitionPlayStatus::Enum m_eTransitionPlayStatus;
	};

	struct Tunables : public CTuning
	{
		Tunables();

		float						m_MinTimeBeforeCanChooseNewPose;
		float						m_MinTimeBeforeNewPose;
		float						m_MaxTimeBeforeNewPose;
		float						m_DistanceForMinTimeBeforeNewPose;
		float						m_DistanceForMaxTimeBeforeNewPose;
		float						m_AvoidNearbyPedHorizontal;
		float						m_AvoidNearbyPedVertical;
		float						m_AvoidNearbyPedDotThreshold;
		float						m_TargetRadius;
		float						m_MinTimeBetweenReactions;
		float						m_MinAnimOffsetMagnitude;
		float						m_Rate;
		float						m_MaxDistanceToCareAboutBlockingLineOfSight;
		float						m_MaxDistanceToUseUrgentTransitions;
		float						m_TimeToUseUrgentTransitionsWhenThreatened;
		float						m_MinTimeBetweenReactionChecksForGunAimedAt;
		float						m_ChancesToReactForGunAimedAt;
		int							m_MaxClipsToCheckPerFrame;
		bool						m_DebugDraw;
		atHashWithStringNotFinal	m_DefaultStandingPose;
		atHashWithStringNotFinal	m_DefaultCrouchingPose;
		
		atArray<AimPose> m_AimPoses;

		PAR_PARSABLE;
	};
	
private:

	struct Reaction
	{
		Reaction()
		{
			Clear();
		}

		void Clear()
		{
			m_bValid = false;
			m_vPos = Vec3V(V_ZERO);
		}

		bool	m_bValid;
		Vec3V	m_vPos;
	};
	
public:

	CTaskVariedAimPose(CTask* pShootTask);
	virtual ~CTaskVariedAimPose();

	// Clone task implementation
	virtual bool ControlPassingAllowed(CPed*, const netPlayer&, eMigrationType migrationType) { if (migrationType == MIGRATE_FORCED || migrationType == MIGRATE_SCRIPT) { return true; } return GetState() == State_Shoot; }
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return GetState() == State_PlayTransition; }
	virtual bool OverridesNetworkHeadingBlender(CPed* pPed) { return OverridesNetworkBlender(pPed); }
	
public:

			fwFlags8&	GetConfigFlags()		{ return m_uConfigFlags; }
	const	fwFlags8	GetConfigFlags() const	{ return m_uConfigFlags; }
	
public:

	bool FailedToFixLineOfSight() const { return m_uRunningFlags.IsFlagSet(RF_FailedToFixLineOfSight); }

public:

	bool IsCurrentPoseCrouching() const
	{
		return (m_pPose && m_pPose->m_IsCrouching);
	}
	
public:

	void MoveAwayFromPosition(Vec3V_In vPosition);
	void OnGunAimedAt(const CEventGunAimedAt& rEvent);
	void OnGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent);
	void OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent);
	void OnShoutBlockingLos(const CEventShoutBlockingLos& rEvent);
	
public:

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL
	
private:

	virtual aiTask*	Copy() const { return rage_new CTaskVariedAimPose(m_pShootTask ? (CTask *)(m_pShootTask->Copy()) : NULL); }
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_VARIED_AIM_POSE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }
	
	virtual	void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	
private:

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	void		Shoot_OnEnter();
	FSM_Return	Shoot_OnUpdate();
	void		ChooseTransition_OnEnter();
	FSM_Return	ChooseTransition_OnUpdate();
	FSM_Return	CheckNextTransition_OnUpdate();
	FSM_Return	StreamClipSet_OnUpdate();
	void		ChooseClip_OnEnter();
	FSM_Return	ChooseClip_OnUpdate();
	FSM_Return	CheckNextClip_OnUpdate();
	void		CheckLineOfSight_OnEnter();
	FSM_Return	CheckLineOfSight_OnUpdate();
	void		CheckNavMesh_OnEnter();
	FSM_Return	CheckNavMesh_OnUpdate();
	void		CheckProbe_OnEnter();
	FSM_Return	CheckProbe_OnUpdate();
	void		CheckProbe_OnExit();
	void		PlayTransition_OnEnter();
	FSM_Return	PlayTransition_OnUpdate();
	void		PlayTransition_OnExit();

	// Cloned states
	FSM_Return	Start_OnUpdate_Clone();
	FSM_Return	Shoot_OnUpdate_Clone();
	FSM_Return  StreamClipSet_OnUpdate_Clone();
	void		PlayTransition_OnEnter_Clone();
	FSM_Return  PlayTransition_OnUpdate_Clone();

private:

	bool						CanChooseClip(const crClip& rClip, Vec3V_InOut vClipOffset) const;
	bool						CanChooseNewPose() const;
	bool						CanChooseTransition(const AimPose::Transition& rTransition) const;
	bool						CanPlayTransition() const;
	bool						CanReact() const;
	void						ChangePose(const s32 iPoseIdx);
	void						SetTransition(const s32 iTransitionIdx);
	void						SetTransitionClipId(const fwMvClipId& ClipId);
	void						SetTransitionPlayStatus(CSerializableState::STransitionPlayStatus::Enum ePlayStatus);
	void						UpdateLocalClipIdFromTransition();
	fwMvClipId					ChooseRandomClipInClipSet(fwMvClipSetId clipSetId) const;
	bool						CleanUpPose();
	void						DoneChoosingTransition(bool bSucceeded);
	const AimPose*				GetAimPoseAt(const s32 uIdx) const;
	s32							FindAimPoseIdx(const u32 uName) const;
	const AimPose::Transition*	FindAimTransitionToPose(const u32 uName) const;
	const CEntity*				GetTarget() const;
	const CPed*					GetTargetPed() const;
	bool						HasPositionToMoveAwayFrom() const;
	bool						IsInDefaultStandingPose() const;
	bool						IsOffsetLargeEnoughToMatter(Vec3V_In vAnimOffset) const;
	bool						IsOffsetOutsideAttackWindow(Vec3V_ConstRef vAnimOffset) const;
	bool						IsOffsetOutsideDefensiveArea(Vec3V_ConstRef vAnimOffset) const;
	bool						IsOffsetTowardsNearbyPed(Vec3V_ConstRef vAnimOffset) const;
	bool						IsOffsetTowardsPosition(Vec3V_ConstRef vAnimOffset, Vec3V_In vPos) const;
	bool						IsOffsetTowardsTarget(Vec3V_ConstRef vAnimOffset) const;
	void						OnThreatened();
	bool						PlayTransition(bool bCrouching);
	bool						PlayTransition(const AimPose::Transition& rTransition);
	void						ProcessCounters();
	void						ProcessFlags();
	void						ProcessHeading();
	void						ProcessTimers();
	void						React(Vec3V_In vPosition);
	void						ReactToGunAimedAt(const CEventGunAimedAt& rEvent);
	void						ReactToGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent);
	void						ReactToGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent);
	void						SetStateAndKeepSubTask(s32 iState);
	bool						ShouldChooseNewPose() const;
	bool						ShouldChooseNewPoseDueToBlockedLineOfSight() const;
	bool						ShouldChooseNewPoseDueToBlockingLineOfSight() const;
	bool						ShouldChooseNewPoseDueToMovingAwayFromPosition() const;
	bool						ShouldChooseNewPoseDueToReaction() const;
	bool						ShouldChooseNewPoseDueToTime() const;
	bool						ShouldFaceTarget() const;
	bool						ShouldUseUrgentTransitions() const;
	bool						ShouldUseUrgentTransitionsDueToAttributes() const;
	bool						ShouldUseUrgentTransitionsDueToDistance() const;
	bool						ShouldUseUrgentTransitionsDueToThreatened() const;
	void						StartTestLineOfSight(Vec3V_ConstRef vPosition);
	
	void						UpdatePoseFromNetwork();
	void						UpdateFromNetwork();

	void						UpdateTransitionFromNetwork();
	void						UpdateTransitionClipIdFromNetwork();
	void						UpdateTransitionPlayStatusFromNetwork();

	void						CancelClonePlayTransition();

private:

	WorldProbe::CShapeTestSingleResult	m_ProbeResults;
	CPrioritizedClipSetRequestHelper	m_ClipSetRequestHelper;
	CAsyncProbeHelper					m_AsyncProbeHelper;
	CNavmeshLineOfSightHelper			m_NavMeshLineOfSightHelper;
	Vec3V								m_vNewPosition;
	Vec3V								m_vPositionToMoveAwayFrom;
	AimPose::Transition					m_Transition;
	SemiShuffledSequence				m_TransitionShuffler;
	SemiShuffledSequence				m_ClipShuffler;
	Reaction							m_Reaction;
	RegdTask							m_pShootTask;
	const AimPose*						m_pPose;
	const AimPose*						m_pInitialPose;
	float								m_fClipDuration;
	float								m_fTimeSinceLastReaction;
	float								m_fTimeSinceLastThreatened;
	float								m_fTimeSinceLastReactionCheckForGunAimedAt;
	s32									m_iTransitionCounter;
	s32									m_iClipCounter;
	int									m_iClipsCheckedThisFrame;
	fwFlags8							m_uConfigFlags;
	fwFlags8							m_uRunningFlags;

	CSerializableState					m_LocalSerializableState;
	CSerializableState					m_FromNetworkSerializableState;

private:
	
	static Tunables	sm_Tunables;
		
};


////////////////////////////////////////////////////////////////////////////////
// CLASS: CClonedVariedAimPoseInfo
// TaskInfo class for CTaskVariedAimPose
////////////////////////////////////////////////////////////////////////////////
class CClonedVariedAimPoseInfo : public CSerialisedFSMTaskInfo
{

public:

	// Initialization / Destruction
	CClonedVariedAimPoseInfo(s32 const iState, const CTaskVariedAimPose::CSerializableState& rSerialisablePoseTransitionInfo);

	CClonedVariedAimPoseInfo();
	~CClonedVariedAimPoseInfo();

	// Getters
	const CTaskVariedAimPose::CSerializableState&  GetPoseTransitionInfo() const { return m_PoseTransitionInfo; }

	// CSerializedFSMTaskInfo overrides
	virtual s32							GetTaskInfoType() const { return INFO_TYPE_VARIED_AIM_POSE; }

	virtual CTask*						CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity));
	virtual CTaskFSMClone*				CreateCloneFSMTask();

	virtual bool						HasState() const			{ return true; }
	virtual u32							GetSizeOfState() const		{ return datBitsNeeded<CTaskVariedAimPose::State_Finish>::COUNT; }
	virtual const char*					GetStatusName(u32) const	{ return "Status";}

	virtual void						Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_INTEGER(serialiser, m_PoseTransitionInfo.m_iPoseIdx, datBitsNeeded<CTaskVariedAimPose::CSerializableState::uMAX_AIM_POSES>::COUNT, "Pose Idx");
		SERIALISE_INTEGER(serialiser, m_PoseTransitionInfo.m_iTransitionIdx, datBitsNeeded<CTaskVariedAimPose::CSerializableState::uMAX_AIM_POSE_TRANSITIONS>::COUNT, "Transition Idx");
		SERIALISE_UNSIGNED(serialiser, m_PoseTransitionInfo.m_uTransitionClipId, 32, "Clip Id");

		u32 uPlayStatus = static_cast<u32>(m_PoseTransitionInfo.m_eTransitionPlayStatus);
		SERIALISE_UNSIGNED(serialiser, uPlayStatus, datBitsNeeded<CTaskVariedAimPose::CSerializableState::STransitionPlayStatus::MAX>::COUNT, "Transition play status");
		m_PoseTransitionInfo.m_eTransitionPlayStatus = static_cast<CTaskVariedAimPose::CSerializableState::STransitionPlayStatus::Enum>(uPlayStatus);
	}

private:

	// Copy forbidden
	CClonedVariedAimPoseInfo(const CClonedVariedAimPoseInfo &);
	CClonedVariedAimPoseInfo &operator=(const CClonedVariedAimPoseInfo &);

	// State
	CTaskVariedAimPose::CSerializableState m_PoseTransitionInfo;
};
#endif //TASK_VARIED_AIM_POSE_H
