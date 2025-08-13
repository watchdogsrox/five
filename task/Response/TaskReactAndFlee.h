// FILE :    TaskReactAndFlee.h
// CREATED : 11-8-2011

#ifndef TASK_REACT_AND_FLEE_H
#define TASK_REACT_AND_FLEE_H

// Game headers
#include "ai/AITarget.h"
#include "event/EventWeapon.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

#define TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE 1
#if TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK
	#define TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT		if(NetworkDebug::IsTaskDebugStateLoggingEnabled()) { Displayf("Task %p : %s : Ped %p %s", this, __FUNCTION__, GetEntity(), GetEntity() ? GetPed()->GetDebugName() : "NO PED"); }
	#define TASK_REACT_AND_FLEE_TTY_UPDATE_FSM_OUTPUT	if(NetworkDebug::IsTaskDebugStateLoggingEnabled()) { Displayf("\nTask %p : %s : Ped %p %s : iState %s : iStateFromNetwork %s : iEvent %d", this, __FUNCTION__, GetEntity(), GetEntity() ? GetPed()->GetDebugName() : "NO PED", GetStaticStateName(GetState()), GetPed()->IsNetworkClone() && (GetStateFromNetwork() != -1) ? GetStaticStateName(GetStateFromNetwork()) : "None / -1", iEvent); }
	#define TASK_REACT_AND_FLEE_TTY_SET_STATE_OUTPUT	if(NetworkDebug::IsTaskDebugStateLoggingEnabled()) { Displayf("Task %p : %s : Ped %p %s : From %s to %s", this, __FUNCTION__, GetEntity(), GetEntity() ? GetPed()->GetDebugName() : "NO PED", GetStaticStateName(GetState()), GetStaticStateName(iState)); }
#else 
	#define TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT 
	#define TASK_REACT_AND_FLEE_TTY_UPDATE_FSM_OUTPUT
	#define TASK_REACT_AND_FLEE_TTY_SET_STATE_OUTPUT
#endif /* TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK*/

// Game forward declarations
class CTaskMoveFollowNavMesh;

////////////////////////////////////////////////////////////////////////////////
// CTaskReactAndFlee
////////////////////////////////////////////////////////////////////////////////

class CTaskReactAndFlee : public CTaskFSMClone
{

public:

	enum BasicAnimations
	{
		BA_None,
		BA_Gunfire,
		BA_ShockingEvent,
	};

	enum ConfigFlags
	{
		CF_DisableReact				= BIT0,
		CF_AcquireExistingMoveTask	= BIT1,
		CF_ForceForwardReactToFlee	= BIT2,
	};

	enum Direction
	{
		D_None,
		D_Back,
		D_Left,
		D_Front,
		D_Right,
		D_Max,
	};

	enum State
	{
		State_Start = 0,
		State_Resume,
		State_StreamReact,
		State_React,
		State_StreamReactToFlee,
		State_ReactToFlee,
		State_Flee,
		State_StreamReactComplete_Clone,
		State_Finish,
	};

public:

	struct Tunables : CTuning
	{
		Tunables();
		
		float	m_MinFleeMoveBlendRatio;
		float	m_MaxFleeMoveBlendRatio;
		bool	m_OverrideDirections;
		float	m_OverrideReactDirection;
		float	m_OverrideFleeDirection;
		float	m_MaxReactionTime;
		float	m_MinRate;
		float	m_MaxRate;
		float	m_HeadingChangeRate;
		float	m_MinTimeToRepeatLastAnimation;
		float	m_SPMinFleeStopDistance;
		float   m_SPMaxFleeStopDistance;
		float	m_MPMinFleeStopDistance;
		float   m_MPMaxFleeStopDistance;
		float	m_TargetInvalidatedMinFleeStopDistance;
		float	m_TargetInvalidatedMaxFleeStopDistance;
		float   m_IgnorePreferPavementsTimeWhenInCrowd;

		PAR_PARSABLE;
	};

	// Class Constants

	// NOTE[egarcia]: These two values must be changed together (they are used for sync)
	static const u8  REACTION_COUNT_MAX_VALUE = 1; 
	static const s32 REACTION_COUNT_NUM_BITS  = 1;
	
public:

	explicit CTaskReactAndFlee(const CAITarget& rTarget, fwMvClipSetId reactClipSetId, fwMvClipSetId reactToFleeClipSetId, fwFlags8 uConfigFlags = 0, eEventType eIsResponseToEventType = EVENT_NONE, bool bScreamActive = false);
	explicit CTaskReactAndFlee(const CAITarget& rTarget, BasicAnimations nBasicAnimations, fwFlags8 uConfigFlags = 0, eEventType eIsResponseToEventType = EVENT_NONE, bool bScreamActive = false);
	virtual ~CTaskReactAndFlee();

public:

	void SetConfigFlagsForFlee(fwFlags16 uFlags)	{ m_uConfigFlagsForFlee = uFlags; }
	void SetLastTargetPosition(Vec3V_In vPosition)	{ m_vLastTargetPosition = vPosition; }

	void SetIsResponseToEventType(eEventType eventType) { m_eIsResponseToEventType = eventType; }
	void SetReactionCount(u8 reactionCount) { m_reactionCount = reactionCount; }

public:

	void SetMoveBlendRatio(float fMoveBlendRatio);

public:

	static bool SpheresOfInfluenceBuilder(TInfluenceSphere* apSpheres, int& iNumSpheres, const CPed* pPed);
	
	virtual const CEntity* GetTargetPositionEntity() const
	{ 
		if(GetSubTask() && !m_Target.GetEntity())
		{
			return GetSubTask()->GetTargetPositionEntity();
		}

		return m_Target.GetEntity();
	}
	
public:
	
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;
	virtual void CleanUp();

	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_REACT_AND_FLEE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Resume; }
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	
	virtual eEventType GetIsResponseForEventType() const { return m_eIsResponseToEventType; }

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL
	
			fwFlags8&	GetConfigFlags()		{ return m_uConfigFlags; }
	const	fwFlags8&	GetConfigFlags() const	{ return m_uConfigFlags; }

	// Helper functions for setting the reaction clip.
	static fwMvClipSetId PickAppropriateReactionClipSet(Vec3V_In vEventPos, const CConditionalAnimsGroup& rAnimGroup, const CPed& rPed);
	static fwMvClipSetId PickAppropriateReactionClipSetFromHash(Vec3V_In vEventPos, atHashWithStringNotFinal conditionalGroupHash, const CPed& rPed);
	static fwMvClipSetId PickDefaultGunfireReactionClipSet(Vec3V_In vEventPos, const CPed& rPed, int iReactionNumber = 0);
	static fwMvClipSetId PickDefaultShockingEventReactionClipSet(Vec3V_In vEventPos, const CPed& rPed);

	static void ResetLastTimeReactedInDirection() { ms_uLastTimeReactedInDirection = 0; }
	
	void SetAmbientClips(atHashWithStringNotFinal hAmbientClips) { m_hAmbientClips = hAmbientClips; }
	
	// PURPOSE: Notify task of a new GunAimedAt event
	void OnGunAimedAt(const CEventGunAimedAt& rEvent);

private:

	// States
	void					Start_OnEnter();
	FSM_Return				Start_OnUpdate();

	FSM_Return				Resume_OnUpdate();
	
	FSM_Return				StreamReact_OnUpdate();
	
	void					React_OnEnter();
	FSM_Return				React_OnUpdate();
	void					React_OnExit();
	
	FSM_Return				StreamReactToFlee_OnUpdate();
	
	void					ReactToFlee_OnEnter();
	FSM_Return				ReactToFlee_OnUpdate();
	void					ReactToFlee_OnExit();
	
	void					Flee_OnEnter();
	FSM_Return				Flee_OnUpdate();
	FSM_Return				Flee_Clone_OnUpdate();

	void					StreamReactComplete_Clone_OnEnter();
	FSM_Return				StreamReactComplete_Clone_OnUpdate();

	int						ChooseStateToResumeTo(bool& bKeepSubTask) const;
	void					ClearReactDirectionFlag();
	float					GenerateDirection(Vec3V_In vPos) const;
	Direction				GenerateDirectionFromValue(float& fValue) const;
	float					GenerateFleeDirectionValue();
	float					GenerateReactDirectionValue();
	bool					GetFirstRoutePosition(Vec3V_InOut vPosition);
	bool					CanDoFullReactionAndGetWpt(Vector3& vPrevWaypoint, Vector3& vNextWaypoint);
	CTaskMoveFollowNavMesh*	GetNavMeshTask();
	bool					HasRoute();
	bool					IsFleeDirectionValid() const;
	bool					IsOnTrain() const;
	void					ProcessMoveTask();
	void					ProcessStreaming();
	bool					ShouldProcessMoveTask() const;
	bool					ShouldProcessTargets() const;
	bool					ShouldStreamInClipSetForReact() const;
	bool					ShouldStreamInClipSetForReactToFlee() const;
	void					UseExistingMoveTask(CTask* pTask);
	void					AdjustMoveTaskTargetPositionByNearByPeds(const CPed* pPed, CTask* pMoveTask, bool bForceScanNearByPeds);

	void					RestartReaction();
	void					IncReactionCount();

private:

	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();

	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed));
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);

public:
	bool							GetOutgoingMigratingCower()					const {	return m_bOutgoingMigratedWhileCowering ;	}
	bool							GetIncomingMigratingCower()					const {	return m_bIncomingMigratedWhileCowering ;	}
	static bool						CheckForCowerMigrate(const CPed* pPed);

private:

	fwMvClipSetId				GetClipSetForState() const;
	void						SetClipSetForState();
	void						TaskSetState(u32 const iState);
	void 						GenerateAIScreamEvents();
	bool 						CanScream() const;
	void 						TryToScream();
	void 						Scream();

	float						CalculateFleeStopDistance() const;

	// PURPOSE: Check if the target entity is valid (Eg: not a dead player)
	bool						IsValidTargetEntity(const CEntity* pTarget) const;

	// PURPOSE: If the target entity is no longer valid, we have to stop updating info from it
	void						OnTargetEntityNoLongerValid();

	// PURPOSE: Reminds if the target entity was invalidated (it can affect to the following states (Eg: dead player -> stop distance)
	void						SetTargetEntityWasInvalidated(bool bNoLongerValid) { m_bTargetEntityWasInvalidated = bNoLongerValid; }

	// PURPOSE: Assign ped a hurry away task
	void						GivePedHurryAwayTask();

private:

	Vector3						m_vPrevWaypoint;
	Vector3						m_vNextWaypoint;
	Vec3V						m_vLastTargetPosition;
	fwClipSetRequestHelper		m_ReactClipSetRequestHelper;
	fwClipSetRequestHelper		m_ReactToFleeClipSetRequestHelper;
	CMoveNetworkHelper			m_MoveNetworkHelper;
	CAITarget					m_Target;
	bool						m_bTargetEntityWasInvalidated;
	bool						m_screamActive;
	CAITarget					m_SecondaryTarget;
	fwMvClipSetId				m_ReactClipSetId;
	fwMvClipSetId				m_ReactToFleeClipSetId;
	RegdTask					m_pExistingMoveTask;
	BasicAnimations				m_nBasicAnimations;
	float						m_fRate;
	float						m_fMoveBlendRatio;
	float						m_fExitBlendDuration;
	atHashWithStringNotFinal	m_hAmbientClips;
	Direction					m_nReactDirection;
	Direction					m_nFleeDirection;
	u32							m_clipHash;
	fwFlags16					m_uConfigFlagsForFlee;
	fwFlags8					m_uConfigFlags;
	eEventType					m_eIsResponseToEventType;
	bool						m_reacted : 1;
	bool						m_restartReaction : 1;
	u8							m_reactionCount; 
	u8							m_networkReactionCount;
	bool						m_bOutgoingMigratedWhileCowering : 1;
	bool						m_bIncomingMigratedWhileCowering : 1;

	// PURPOSE: Keep params for the creation of the next smart flee task
	struct SCreateSmartFleeTaskParams
	{
		SCreateSmartFleeTaskParams() 
			: bValid(false)
			, fIgnorePreferPavementsTimeLeft(-1.0f)
		{

		}

		void Set(const float _fIgnorePreferPavementsTimeLeft)
		{
			bValid = true;
			fIgnorePreferPavementsTimeLeft = _fIgnorePreferPavementsTimeLeft;
		}

		void Clear()
		{
			bValid = false;
		}

		bool   bValid;
		float  fIgnorePreferPavementsTimeLeft;

	};

	SCreateSmartFleeTaskParams m_CreateSmartFleeTaskParams; 

private:

	static fwMvBooleanId ms_BlendOutId;
	static fwMvBooleanId ms_ReactClipEndedId;
	static fwMvBooleanId ms_ReactToFleeClipEndedId;
	
	static fwMvClipId ms_ReactClipId;
	static fwMvClipId ms_ReactToFleeClipId;
	
	static fwMvFlagId ms_FleeBackId;
	static fwMvFlagId ms_FleeLeftId;
	static fwMvFlagId ms_FleeFrontId;
	static fwMvFlagId ms_FleeRightId;
	static fwMvFlagId ms_ReactBackId;
	static fwMvFlagId ms_ReactLeftId;
	static fwMvFlagId ms_ReactFrontId;
	static fwMvFlagId ms_ReactRightId;
	
	static fwMvFloatId ms_RateId;
	static fwMvFloatId ms_PhaseId;
	
	static fwMvRequestId ms_ReactId;
	static fwMvRequestId ms_ReactToFleeId;

	static u32 ms_uLastTimeReactedInDirection;

public:

	static Tunables	sm_Tunables;

private:

#if !__FINAL
	ScalarV m_DEBUG_scFleeCrowdSize;
	Vec3V m_DEBUG_vFleePedInitPosition;
	Vec3V m_DEBUG_vFleeTargetInitPosition;
	ScalarV m_DEBUG_scFleeNearbyPedsTotalWeight;
	Vec3V m_DEBUG_vFleeNearbyPedsCenterOfMass;
	Vec3V m_DEBUG_vFleeTargetAdjustedPosition;
	atArray<Vec3V> m_DEBUG_aFleePositions;
	atArray<ScalarV> m_DEBUG_aFleeWeights;
	Vec3V m_DEBUG_vWallCollisionPosition;
	Vec3V m_DEBUG_vWallCollisionNormal;
	Vec3V m_DEBUG_vBeforeWallCheckFleeAdjustedDir;
	Vec3V m_DEBUG_vExitCrowdDir;
#endif // !__FINAL

#if !__FINAL && DEBUG_DRAW 

	void DebugRender() const;

#endif /* !__FINAL && DEBUG_DRAW */
};

////////////////////
//Main combat task response info
////////////////////

class CClonedReactAndFleeInfo : public CSerialisedFSMTaskInfo
{
public:

	struct InitParams
	{
		InitParams
		(
			s32 const _state, 
			u8  const _controlFlags, 
			u32 const _ambientClipHash, 
			float const _rate,
			CEntity const* const _targetEntity,
			bool const _targetEntityWasInvalidated,
			Vec3V_In _targetPosition, 
			fwMvClipSetId const& reactClipId, 
			fwMvClipSetId const& reactToFleeClipId,
			bool const _reacted,
			u8 const _reactionCount,
			u32 const  _clipHash,
			eEventType const _isResponseToEventType,
			bool const _screamActive
		)
		:
			m_state(_state),
			m_controlFlags(_controlFlags),
			m_ambientClipHash(_ambientClipHash),
			m_targetEntity(_targetEntity),
			m_targetEntityWasInvalidated(_targetEntityWasInvalidated),
			m_targetPosition(_targetPosition),
			m_rate(_rate),
			m_reactClipSetId(reactClipId),
			m_reactToFleeClipSetId(reactToFleeClipId),
			m_reacted(_reacted),
			m_reactionCount(_reactionCount),
			m_clipHash(_clipHash),
			m_isResponseToEventType(_isResponseToEventType),
			m_screamActive(_screamActive)
		{}
		
		u32				m_state;
		u8				m_controlFlags;
		u32				m_ambientClipHash;
		CEntity const*	m_targetEntity;
		bool			m_targetEntityWasInvalidated;
		Vec3V			m_targetPosition;
		float			m_rate;
		fwMvClipSetId	m_reactClipSetId;
		fwMvClipSetId	m_reactToFleeClipSetId;		
		bool			m_reacted;
		u8				m_reactionCount;
		u32				m_clipHash;
		eEventType		m_isResponseToEventType;
		bool			m_screamActive;
	};


public:
    CClonedReactAndFleeInfo( InitParams const& _initParams );

    CClonedReactAndFleeInfo();
    ~CClonedReactAndFleeInfo();

	virtual s32							GetTaskInfoType( ) const		{ return INFO_TYPE_REACT_AND_FLEE;		}

	virtual CTaskFSMClone*				CreateCloneFSMTask();

	inline const u32					GetAmbientClipHash() const			{ return m_ambientClipHash;		}
	inline const CEntity*				GetTargetEntity() const				{ return m_targetEntity.GetEntity(); }
	inline const Vec3V_ConstRef			GetTargetPosition() const			{ return m_targetPosition;			}
	inline const u8						GetControlFlags() const				{ return m_controlFlags;			}
	inline const float					GetRate() const						{ return m_rate;					}
	inline const fwMvClipSetId			GetReactClipSetId() const			{ return m_reactClipSetId;			}
	inline const fwMvClipSetId			GetReactToFleeClipSetId() const		{ return m_reactToFleeClipSetId;	}
	inline const bool					GetReacted() const					{ return m_reacted;					}
	inline const u8						GetReactionCount() const			{ return m_reactionCount; }
	inline const u32					GetClipHash() const					{ return m_clipHash;				}
	inline const s32					GetIsResponseToEventType() const	{ return m_isResponseToEventType;		}
	inline const bool					GetScreamActive() const				{ return m_screamActive; }

    virtual bool						HasState() const					{ return true; }
	virtual u32							GetSizeOfState() const				{ return datBitsNeeded<CTaskReactAndFlee::State_Finish>::COUNT; }
    virtual const char*					GetStatusName(u32) const			{ return "Status";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool bHasTargetEntity = m_targetEntity.IsValid();

		SERIALISE_BOOL(serialiser, bHasTargetEntity,"Has target entity");
		if (bHasTargetEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			ObjectId targetID = m_targetEntity.GetEntityID();
			SERIALISE_OBJECTID(serialiser, targetID, "TargetEntity");
			m_targetEntity.SetEntityID(targetID);
		}

		SERIALISE_BOOL(serialiser, m_targetEntityWasInvalidated, "TargetEntity was invalidated");

		Vector3 pos = VEC3V_TO_VECTOR3(m_targetPosition);
		SERIALISE_POSITION(serialiser, pos, "Target Pos");
		m_targetPosition = VECTOR3_TO_VEC3V(pos);

		SERIALISE_UNSIGNED(serialiser, m_controlFlags,		SIZEOF_CONTROL_FLAGS, "Control Flags");
		SERIALISE_UNSIGNED(serialiser, m_ambientClipHash,		SIZEOF_CLIP_HASH,  "Ambient Hash");
		
		SERIALISE_PACKED_FLOAT(serialiser, m_rate,	CTaskReactAndFlee::sm_Tunables.m_MaxRate, SIZEOF_RATE, "Rate");

		SERIALISE_ANIMATION_CLIP(serialiser, m_reactClipSetId,		m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall, "React Clip Set");
		SERIALISE_ANIMATION_CLIP(serialiser, m_reactToFleeClipSetId,	m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall, "React To Flee Clip Set");

		SERIALISE_UNSIGNED(serialiser, m_clipHash,		SIZEOF_CLIP_HASH,  "Clip Hash");
		
		SERIALISE_BOOL(serialiser, m_reacted);

		SERIALISE_UNSIGNED(serialiser, m_reactionCount, CTaskReactAndFlee::REACTION_COUNT_NUM_BITS, "Reaction Count");

		SERIALISE_UNSIGNED(serialiser, m_isResponseToEventType, 8, "Is Response To Event Type");

		SERIALISE_BOOL(serialiser, m_screamActive, "Scream Active");
	}

	virtual CTask*	CreateLocalTask(fwEntity *pEntity);

private:

	static const u32 SIZEOF_CLIP_HASH				= 32;
	static const u32 SIZEOF_CONTROL_FLAGS			= 8;
	static const u32 SIZEOF_REACTION_TIME			= 32;
	static const u32 SIZEOF_RATE					= 32;
	static const u32 SIZEOF_FLEE_REACT_DIRECTION	= datBitsNeeded<CTaskReactAndFlee::D_Max>::COUNT;

    CClonedReactAndFleeInfo(const CClonedReactAndFleeInfo &);
    CClonedReactAndFleeInfo &operator=(const CClonedReactAndFleeInfo &);

	CAITarget GetFleeAITarget() const;

	Vec3V			m_targetPosition;
	CSyncedEntity   m_targetEntity;
	bool			m_targetEntityWasInvalidated;

	u32				m_ambientClipHash;
	u32				m_clipHash;
	float			m_rate;

	u8				m_controlFlags;

	bool			m_reacted;
	u8				m_reactionCount;

	fwMvClipSetId	m_reactClipSetId;
	fwMvClipSetId	m_reactToFleeClipSetId;

	u8				m_isResponseToEventType;

	bool			m_screamActive;

private:

	fwMvClipId		m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;
};

CompileTimeAssert(eEVENTTYPE_MAX <= 255); // NOTE[egarcia]: We are using a byte in CClonedReactAndFleeInfo to sync m_isResponseToEventType

#endif //TASK_REACT_AND_FLEE_H
