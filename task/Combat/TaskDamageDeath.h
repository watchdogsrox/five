#ifndef TASK_DAMAGE_DEATH_H
#define TASK_DAMAGE_DEATH_H
 
#include "Event/Events.h"
#include "network/General/NetworkUtil.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Physics/NmDefines.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskFSMClone.h"

class CEntity;
class CPed;
class CTaskNMControl;

#if __BANK
#define STATE_HISTORY_BUFFER_SIZE 6
#endif

// Small class containing damage impact information
class CDamageInfo
{
public:
	CDamageInfo(u32 weaponHash, const Vector3* pvStartPos, WorldProbe::CShapeTestHitPoint* pResult, Vector3* pvImpulseDir, float fImpulseMag, CPed *pPed);
	CDamageInfo()
		: m_weaponHash(0)
		, m_vStartPos(VEC3_ZERO)
		, m_vImpulseDir(VEC3_ZERO)
		, m_fImpulseMag(0.0f)
		, m_bValidIntersectionInfo(false)
		, m_iIntersectionComponent(0)
		, m_vLocalIntersectionPos(VEC3_ZERO)
	{
	}

	u32	GetWeaponHash() const { return m_weaponHash; }
	Vector3 GetStartPos() const { return m_vStartPos; }
	Vector3 GetImpulseDir() const { return m_vImpulseDir; }
	float	GetImpulseMag() const { return m_fImpulseMag; }
	u16	GetIntersectionComponent() const { return m_iIntersectionComponent; }
	Vector3 GetWorldIntersectionPos(CPed *pPed);
	bool	GetIntersectionInfo() const { return m_bValidIntersectionInfo; }
private:
	u32				m_weaponHash;
	Vector3			m_vStartPos;
	Vector3			m_vImpulseDir;
	float			m_fImpulseMag;
	// Intersection info
	bool			m_bValidIntersectionInfo : 1;
	u16				m_iIntersectionComponent;
	Vector3			m_vLocalIntersectionPos;
};

// information on the cause of death
class CDeathSourceInfo
{
public:

	static const CTaskFallOver::eFallDirection DEFAULT_FALL_DIRECTION	= CTaskFallOver::kDirFront;
	static const CTaskFallOver::eFallContext   DEFAULT_FALL_CONTEXT		= CTaskFallOver::kContextStandard;

public:

	CDeathSourceInfo();
	CDeathSourceInfo(CEntity* pEntity, u32 weapon = 0);
	CDeathSourceInfo(CEntity* pEntity, u32 weapon, CTaskFallOver::eFallContext fallContext, CTaskFallOver::eFallDirection fallDirection);
	CDeathSourceInfo(CEntity* pEntity, u32 weapon, float fDirection, int boneTag);
	CDeathSourceInfo(const CDeathSourceInfo& info);

	CEntity*								GetEntity() 				{ return m_entity.GetEntity(); }
	const CEntity*							GetEntity() const			{ return m_entity.GetEntity(); }
	u32										GetWeapon() const			{ return m_weapon; }
	CTaskFallOver::eFallContext				GetFallContext() const		{ return m_fallContext; }
	CTaskFallOver::eFallDirection			GetFallDirection() const	{ return m_fallDirection; }

	void									ClearEntity()				{ m_entity.Invalidate(); }
	bool IsValid() const 
	{ 
		return m_entity.IsValid() || m_weapon != 0 || m_fallContext != DEFAULT_FALL_CONTEXT || m_fallDirection != DEFAULT_FALL_DIRECTION; 
	}

	void Serialise(CSyncDataBase& serialiser);

	void ComputeFallDirection(CPed& ped);

private:

	CSyncedEntity						m_entity;				// the entity which caused the ped's death
	u32									m_weapon;				// the weapon which caused the ped's death
	CTaskFallOver::eFallContext			m_fallContext;			// the context with which the ped will fall
	CTaskFallOver::eFallDirection		m_fallDirection;		// the direction the ped will fall
};

class CTaskDyingDead : public CTaskFSMClone
{
public:

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_VehicleForwardInitialScale;
		float m_VehicleForwardScale;
		float m_TimeToApplyPushFromVehicleForce;
		float m_ForceToApply;

		float m_MinFallingSpeedForAnimatedDyingFall;

		float m_SphereTestRadiusForDeadWaterSettle;
		float m_RagdollAbortPoseDistanceThreshold;
		float m_RagdollAbortPoseMaxVelocity;

		u32 m_TimeToThrowWeaponMS;
		u32 m_TimeToThrowWeaponPlayerMS;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,

		State_StreamAssets,
		State_DyingAnimated,
		State_DyingAnimatedFall,
		State_DyingRagdoll,

		State_RagdollAborted,
		State_DeadAnimated,

		State_DeadRagdoll,
		State_DeadRagdollFrame,

		State_FallOutOfVehicle,

		State_Finish
	} DyingDeadState;

	enum
	{
		// Prevents the peds ragdoll being activated if the player walks into this ped
		Flag_disableRagdollActivationFromPlayerImpact	= BIT0,
		Flag_clipsOverridenByClipSetHash				= BIT1,
		Flag_clipsOverridenByDictHash					= BIT2,
		Flag_naturalMotionTaskSpecified					= BIT3,
		Flag_ragdollDeath								= BIT4,
		Flag_inVehicleDeath								= BIT5,
		Flag_startDead									= BIT6, 
		Flag_ragdollWhenAble							= BIT7,// this is the last flag synced over the network
		Flag_damageImpactRecorded						= BIT8,
		Flag_ragdollFramePosedInWater					= BIT9,
		Flag_doInjuredOnGround							= BIT10,
		Flag_cloneLocalSwitch							= BIT11,
		Flag_disableCapsuleInRagdollFrameState			= BIT12,
		Flag_ragdollHasRelaxed							= BIT14,
		Flag_droppedWeapon								= BIT15,
		Flag_RagdollHasSettled							= BIT16,
		Flag_GoStraightToRagdollFrame					= BIT17,
		Flag_PickupsDropped								= BIT18,
		Flag_RunningAnimatedReactionSubtask				= BIT19,
		Flag_HasReactivatedShallowWaterPed				= BIT20,
		Flag_SleepingIsAllowed							= BIT21,
		Flag_DeathAnimSetInternally						= BIT22,
		Flag_DisableVehicleImpactsWhilstDying			= BIT23,
		Flag_UnderwaterCorpseConstraintsApplied			= BIT24,
		Flag_EndingAnimatedDeathFall					= BIT25,
		Flag_DontSendPedKilledShockingEvent				= BIT26
	};

	enum kSnapToGroundStage
	{
		kSnapNotBegun,
		kSnapFailed,
		kSnapPoseRequested,
		kSnapPoseReceived
	};


	CTaskDyingDead(const CDeathSourceInfo* pSourceInfo = NULL, s32 iFlags = 0, u32 timeOfDeath = 0);
	~CTaskDyingDead();

	void AddToStateHistory(DyingDeadState state);
#if __BANK
	struct StateData
	{
		DyingDeadState m_state;
		u16 m_startTimeInStateMS;
	};
	atQueue<StateData, STATE_HISTORY_BUFFER_SIZE> m_StateHistory;
	atQueue<StateData, STATE_HISTORY_BUFFER_SIZE> &GetStateHistory() { return m_StateHistory; }
#endif
	kSnapToGroundStage GetSnapToGroundStage() const { return m_SnapToGroundStage; }

	// Basic task implementations
	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const {return CTaskTypes::TASK_DYING_DEAD;}
	virtual	void	CleanUp				();

	// Basic FSM implementations
	FSM_Return		ProcessPreFSM		();
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	bool			ProcessPostPreRender();
	virtual bool	ProcessMoveSignals();
	virtual s32	GetDefaultStateAfterAbort()	const {return State_Start;}

	// Additional task construction options
	// Set a specific dying clip to be played
	void SetDeathAnimationBySet(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float blendDelta = NORMAL_BLEND_DURATION, float startPhase = 0.0f );
	void SetDeathAnimationByDictionary(s32 clipDictIndex, u32 clipHashKey, float blendDelta = NORMAL_BLEND_DURATION );
	// Sets a specific ragdoll task
	void SetForcedNaturalMotionTask(aiTask* pSubtask);
	// Gets the forced natural motion task
	aiTask* GetForcedNaturalMotionTask() const { return m_pForcedNaturalMotionTask; }
	// Record a damage impact to be responded to in the next update
	virtual bool RecordDamage(CDamageInfo &damageInfo);

	void MakeAnonymousDamageByEntity(const CEntity* pEntity);

	void InitSleep(CPed *pPed);
	void DisableSleep(CPed *pPed);

	bool IsInDyingState() { return GetState()== State_DyingAnimated || GetState() == State_DyingRagdoll; }
	bool IsInDeadState() { return GetState()>State_DyingRagdoll && GetState()<State_Finish; }

	void SetHasCorpseRelaxed() { m_flags.SetFlag(Flag_ragdollHasRelaxed); }
	bool HasCorpseRelaxed() const { return m_flags.IsFlagSet(Flag_ragdollHasRelaxed); }

	bool StartDead() const { return m_flags.IsFlagSet(Flag_startDead); }

	virtual bool HandlesDeadPed() {return true; }
	virtual bool HandlesRagdoll(const CPed*) const { return true; }

	// PURPOSE: Instructs the dead task to go immediately to the ragdoll frame state when it
	// first runs, even if the ped isn't using ragdoll. Only use this when the ped is already
	// in an appropriate death pose.
	void GoStraightToRagdollFrame() { m_flags.SetFlag(Flag_GoStraightToRagdollFrame); }

	void DisableVehicleImpactsWhilstDying() { m_flags.SetFlag(Flag_DisableVehicleImpactsWhilstDying); }

#if !__NO_OUTPUT
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	void DealWithDeactivatedPhysics( CPed* pPed, bool ragdollIsAsleep = false );
	void SwitchToRagdoll( CPed* pPed );

	bool ShouldUseReducedRagdollLOD(CPed* pPed);

	void SetHasPlayedHornAudio(bool bHasPlayedHorn) { m_bPlayedHornAudio = bHasPlayedHorn; }

	// Clone task implementation
	virtual bool            OverridesNetworkBlender(CPed*) { return true; }
	virtual bool            OverridesNetworkHeadingBlender(CPed*) { return true; }
	virtual CTaskInfo*		CreateQueriableState() const;
    virtual void            ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void            OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return		UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual bool			IsInScope(const CPed*);
	virtual bool			CanBeReplacedByCloneTask(u32 taskType) const;

	virtual CTaskFSMClone *CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone *CreateTaskForLocalPed(CPed* pPed);
	virtual void           HandleLocalToCloneSwitch(CPed* pPed);
	virtual void           HandleCloneToLocalSwitch(CPed* pPed);
	virtual bool           HandleLocalToRemoteSwitch(CPed* pPed, CClonedFSMTaskInfo* pTaskInfo);

protected:

	// Determine whether or not the task should abort
	virtual bool MakeAbortable(const AbortPriority priority, const aiEvent* pEvent);
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	bool IsRagdollSettledOnMapCollision(CPed* pPed);

private:
	// State implementations
	// Start
	void			Start_OnEnter		(CPed* pPed);
	FSM_Return		Start_OnUpdate		(CPed* pPed);
	// Streaming	
	void			StreamAssets_OnEnter (CPed* pPed);
	FSM_Return		StreamAssets_OnUpdate(CPed* pPed);
	// DyingAnimated
	void			DyingAnimated_OnEnter	(CPed* pPed);
	FSM_Return		DyingAnimated_OnUpdate	(CPed* pPed);
	void			DyingAnimated_OnExit	(CPed* pPed);

	void			DyingAnimatedFall_OnEnter	(CPed* pPed);
	FSM_Return		DyingAnimatedFall_OnUpdate	(CPed* pPed);
	void			DyingAnimatedFall_OnExit	(CPed* pPed);

	// DyingRagdoll
	void			DyingRagdoll_OnEnter	(CPed* pPed);
	FSM_Return		DyingRagdoll_OnUpdate	(CPed* pPed);

	// State_RagdollAborted
	void			RagdollAborted_OnEnter(CPed* pPed);
	FSM_Return		RagdollAborted_OnUpdate(CPed* pPed);
	void			RagdollAborted_OnExit(CPed* pPed);    

	// State_DeadAnimated
	void			DeadAnimated_OnEnter(CPed* pPed);
	FSM_Return		DeadAnimated_OnUpdate(CPed* pPed);
	void			DeadAnimated_OnExit(CPed* pPed);    

	// State_DeadRagdoll
	void			DeadRagdoll_OnEnter(CPed* pPed);
	FSM_Return		DeadRagdoll_OnUpdate(CPed* pPed);
	void			DeadRagdoll_OnExit(CPed* pPed);    

	// State_DeadRagdollFrame
	void			DeadRagdollFrame_OnEnter(CPed* pPed);
	FSM_Return		DeadRagdollFrame_OnUpdate(CPed* pPed);
	void			DeadRagdollFrame_OnExit(CPed* pPed);    

	// State_FallOutOfVehicle
	void			FallOutOfVehicle_OnEnter(CPed* pPed);    
	FSM_Return		FallOutOfVehicle_OnUpdate(CPed* pPed);    
	FSM_Return		FallOutOfVehicle_OnExit(CPed* pPed );

	void SwitchToRagdollFrame( CPed* pPed );

	// Dying helper functions
	void BlendUpperBodyReactionClip(CPed* pPed);

	// Dead helper functions
	void SetIKSlopeAngle( CPed* pPed );
	void DisablePhysicsWhilstDead( CPed* pPed );
	bool CheckRagdollFrameDepthConditions( CPed* pPed );
	void TriggerBloodPool( CPed* pPed );
	void ProcessBloodPool( CPed* pPed );
	void CheckForDeadInCarAudioEvents(CPed* pPed);
	void DropWeapon( CPed* pPed );
	// Common helper function called from all the _dead_ pathway entry
	// Called basically whenever we engage
	void OnDeadEnter( CPed &ped );
	// Common helper function called from all the _dead_ pathways.
	// Caleld on start of RagdollFrame or end of the others.
	void OnDeadExit( CPed &ped );

	void ComputeDeathAnim(CPed* pPed);

	//helper function to get the death clip base on flags and values
	const crClip* GetDeathClip(bool bAssert = true);

	// Do some common processing to ready the ped for the dead states
	void InitDeadState();
	
	// Set the death info on the ped
	void SetDeathInfo();

	void SetProcessBloodTimer(float fBloodTimer);

	void ProcessPushFromVehicle();

	void AddBlockingObject( CPed* pPed );
	void UpdateBlockingObject( CPed* pPed );

	bool ShouldFallOutOfVehicle() const;

	bool IsOffscreenAndCanSnapToGround( CPed* pPed );

	void StartMoveNetwork(CPed* pPed, float blendDuration);
protected:
	// Basic flags
	fwFlags32	m_flags;
private:

	CDeathSourceInfo m_deathSourceInfo;

	// Animation variables
	// Dying clip - by set
	u32	m_clipSetId;	// could be an clip set id or dictionary hash - see Flag_clipsOverridenByClipSetHash and Flag_clipsOverridenByDictHash
	u32	m_clipId;		// Clip ids and clip hash keys are now the same thing... huzzah!

	// Dying clip playback variables
	float	m_fBlendDuration;
	float	m_fStartPhase;

	// Phase in the clip the 
	float		m_fRagdollStartPhase;		// MoVE should pass out this event as and when it becomes available
	float		m_fRootSlopeFixupPhase;
	float		m_fDropWeaponPhase;

	float		m_fFallTime;

	//bool		m_bDoInjuredOnGround;		// replaced with a flag (should have been one already)

	// Forced first subtask
	RegdaiTask	m_pForcedNaturalMotionTask; 

	// Ground physical
	phHandle	m_PhysicalGroundHandle;

	// Damage info from an impact
	atArray<CDamageInfo> m_damageInfo;

	// Record of this peds death time
	u32 m_iTimeOfDeath; 

	bool m_QuitTask;
	bool m_bUseRagdollDuringClip;
	bool m_bPlayedHornAudio;
	bool m_bDyingClipEnded;
	bool m_bCanAnimatedDeadFall;

	kSnapToGroundStage m_SnapToGroundStage;

	// Network helper for clip playback
	CMoveNetworkHelper m_moveNetworkHelper;

	// timer to signal the pool to keep growing as the task exists
 	CTaskTimer m_tProcessBloodPoolTimer;

	fwClipSetRequestHelper m_ClipSetRequestHelper;

	float m_bloodPoolStartSize;
	float m_bloodPoolEndSize;
	float m_bloodPoolGrowRate;

	TDynamicObjectIndex m_iDynamicObjectIndex;
	u32 m_iDynamicObjectLastUpdateTime;

	u32 m_NetworkStartTime;

	s32 m_iExitPointForVehicleFallOut;

	float m_fRagdollAbortVelMag;

	s32 m_clonePedLastSignificantBoneTag; 

	bool m_bDontUseRagdollFrameDueToInVehicleDeath : 1;

	// statics

	// move network signals

	//requests
	static fwMvRequestId ms_DyingAnimatedId;
	static fwMvRequestId ms_DyingRagdollId;
	static fwMvRequestId ms_DeadAnimatedId;
	static fwMvRequestId ms_DeadRagdollId;
	static fwMvRequestId ms_DeadRagdollFrameId;

	// state enter events
	static fwMvBooleanId ms_DyingAnimatedEnterId;
	static fwMvBooleanId ms_DyingRagdollEnterId;
	static fwMvBooleanId ms_DeadAnimatedEnterId;
	static fwMvBooleanId ms_DeadRagdollEnterId;
	static fwMvBooleanId ms_DeadRagdollFrameEnterId;

	//other signals
	static fwMvBooleanId ms_HornAudioOnId;
	static fwMvBooleanId ms_DyingClipEndedId;
	static fwMvBooleanId ms_DropWeaponRnd;
	static fwMvClipId	 ms_DeathClipId;
	static fwMvClipId	 ms_UpperBodyReactionClipId;
	static fwMvRequestId ms_UpperBodyReactionId;
	static fwMvRequestId ms_ReactToDamageId;
	static fwMvFloatId	 ms_DeathClipPhaseId;
	static fwMvFloatId	 ms_DyingClipStartPhaseId;

	static float ms_fAbortAnimatedDyingFallWaterLevel;
	static u32 ms_iLastVehicleHornTime;

public:
	static bool	 ms_bForceFixMatrixOnDeadRagdollFrame;
};

//-------------------------------------------------------------------------
// Task info for CTaskDyingDead
//-------------------------------------------------------------------------
class CClonedDyingDeadInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedDyingDeadInfo(u32 const state, const CDeathSourceInfo& deathSourceInfo, s32 shotBoneTag, s32 flags, const u32 &clipSetId = CLIP_SET_ID_INVALID, const u32 &clipId = CLIP_ID_INVALID, float blendDuration = NORMAL_BLEND_DURATION);
	CClonedDyingDeadInfo();
	~CClonedDyingDeadInfo() {}

    virtual bool								HasState() const				{ return true; }
	virtual u32									GetSizeOfState() const			{ return datBitsNeeded<CTaskDyingDead::State_Finish>::COUNT; }
    virtual const char*								GetStatusName(u32) const		{ return "Status";}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_DYINGDEAD;}

	const CDeathSourceInfo& GetDeathSourceInfo() const  { return m_deathSourceInfo; }
    s32 GetShotBoneTag() const { return m_lastSignificantShotBoneTag; }

	bool	HasClip()			{ return m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID; }

	void    GetFlags(fwFlags32& flags)        
	{
		// only copy synced flags
		flags.ClearFlag(CClonedDyingDeadInfo::FLAGS_MASK);
		flags.SetFlag(m_flags);
	}

	void	GetClipSet(fwMvClipSetId &clipSetId, fwMvClipId &clipId) 
	{ 
		clipSetId.SetHash(m_clipSetId); 
		clipId.SetHash(m_clipId); 
	}

	void	OverrideClipSet(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float blendDuration = -1.0f)
	{
		m_clipSetId = clipSetId;
		m_clipId = clipId;
		if (blendDuration >= 0.0f)
		{
			m_blendDuration = blendDuration;
		}

		m_flags |= CTaskDyingDead::Flag_clipsOverridenByClipSetHash;
	}

	void	GetClipDict(s32 &iClipDictIndex, u32& uClipHash) 
	{ 
		iClipDictIndex	= m_clipSetId; 
		uClipHash		= m_clipId; 
	}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		m_deathSourceInfo.Serialise(serialiser);

        static const unsigned SIZEOF_BONE_TAG = 32;
        SERIALISE_INTEGER(serialiser, m_lastSignificantShotBoneTag, SIZEOF_BONE_TAG, "Shot Bone Tag");
		SERIALISE_UNSIGNED(serialiser, m_flags, SIZEOF_FLAGS, "Flags");

		bool bHasClip = HasClip();

		SERIALISE_BOOL(serialiser, bHasClip, "Has clip");

		if (bHasClip || serialiser.GetIsMaximumSizeSerialiser())
		{
			if (m_flags & CTaskDyingDead::Flag_clipsOverridenByClipSetHash)
			{
				fwMvClipSetId clipSetId(m_clipSetId);
				fwMvClipId clipId(m_clipId);
				SERIALISE_ANIMATION_CLIP(serialiser, clipSetId, clipId, "Animation");
				m_clipSetId = clipSetId.GetHash();
				m_clipId = clipId.GetHash();
			}
			else if (AssertVerify((m_flags & CTaskDyingDead::Flag_clipsOverridenByDictHash) || serialiser.GetIsMaximumSizeSerialiser()))
			{
				s32 animSlot = (s32)m_clipSetId;
				SERIALISE_ANIMATION_CLIP(serialiser, animSlot, m_clipId, "Animation");
				m_clipSetId = (u32) animSlot;
			}
			else
			{
				m_clipSetId = CLIP_SET_ID_INVALID;
				m_clipId = CLIP_ID_INVALID;
			}

			bool bHasBlendDuration = m_blendDuration != NORMAL_BLEND_DURATION;

			SERIALISE_BOOL(serialiser, bHasBlendDuration, "bHasBlendDuration");

			if (bHasBlendDuration)
			{
				SERIALISE_PACKED_FLOAT(serialiser, m_blendDuration, 4.0f, SIZEOF_BLEND_DURATION, "Blend duration");
			}
			else
			{
				m_blendDuration = NORMAL_BLEND_DURATION;
			}
		}
		else
		{
			m_clipSetId = CLIP_SET_ID_INVALID;
			m_clipId = CLIP_ID_INVALID;
		}
	}

private:

	static const unsigned int SIZEOF_FLAGS			= 8;
	static const unsigned int SIZEOF_BLEND_DURATION = 8;
	static const unsigned int FLAGS_MASK			= (1<<SIZEOF_FLAGS)-1;

	CClonedDyingDeadInfo(const CClonedDyingDeadInfo &);
	CClonedDyingDeadInfo &operator=(const CClonedDyingDeadInfo &);

	CDeathSourceInfo m_deathSourceInfo;
    s32              m_lastSignificantShotBoneTag;
	s32				 m_flags;
	u32				 m_clipSetId; 
	u32				 m_clipId; 
	float			 m_blendDuration;
};


///////////////
//Hit response
///////////////

class CTaskHitResponse : public CTask
{
public:

	enum
	{
		State_Initial,
		State_PlayingClip
	};

	CTaskHitResponse(const int iHitSide);
	~CTaskHitResponse();

	virtual aiTask* Copy() const { return rage_new CTaskHitResponse(m_iHitSide); }

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_HIT_RESPONSE;}
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

#if !__FINAL
	virtual void Debug() const {}
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial : return "Initial";
		case State_PlayingClip : return "PlayingAnim";
		default: Assert(0); return NULL;
		}
	}
#endif    

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StatePlayingClip_OnEnter(CPed * pPed);
	FSM_Return StatePlayingClip_OnUpdate(CPed * pPed);

private:

	int m_iHitSide;
};

//////////////////
//On fire
//////////////////

class CTaskComplexOnFire : public CTaskComplex
{
public:
	CTaskComplexOnFire(u32 weaponHash);
	~CTaskComplexOnFire();

	virtual aiTask* Copy() const {return rage_new CTaskComplexOnFire(m_nWeaponHash);}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COMPLEX_ON_FIRE;} 
#if !__NO_OUTPUT
	virtual void Debug() const {}
	virtual atString GetName() const {return atString("OnFire");}
#endif 

	static u32 GetMaxPlayerOnFireTime() { return ms_nMaxPlayerOnFireTime; }

	s32 GetDefaultStateAfterAbort() const { return CTaskComplex::State_ControlSubTask; }

	virtual aiTask* CreateNextSubTask(CPed* pPed);
	virtual aiTask* CreateFirstSubTask(CPed* pPed);
	virtual aiTask* ControlSubTask(CPed* pPed);

	bool ApplyPerFrameFiredamage( CPed* pPed, u32 weaponHash );

	//    static void ComputeFireDamage(CPed* pPed, CPedDamageResponse& damageResponse);

	// PURPOSE: Called by ped event scanner when checking dead peds tasks if this returns true a damage event is not generated if a dead ped runs this task
	virtual bool HandlesDeadPed() { return true; }

	virtual bool HandlesRagdoll(const CPed*) const { return true; }

	virtual void GetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* lastItem, CTaskMove* pMoveTask, CPed* pPed);
	virtual bool UseCustomGetup() { return true; }
	virtual bool AddGetUpSets(CNmBlendOutSetList& sets, CPed* pPed);

	static const float ms_fHealthRate;

protected:

	// PURPOSE: Update the peds facial clip
	void ProcessFacialIdle();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

private:

	aiTask* CreateSubTask(CPed* pPed, const int iSubTaskType) const;

private:
	//	u32 m_nStartTime;
	u32 m_nWeaponHash;

	static const float ms_fSafeDistance;
	static const u32 ms_nMaxFleeTime;
	static const u32 ms_nMaxPlayerOnFireTime;
};

////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Task info for electric damage
//-------------------------------------------------------------------------
class CClonedDamageElectricInfo : public CSerialisedFSMTaskInfo
{

public:
	CClonedDamageElectricInfo( u32 _damageTotalTime, fwMvClipSetId _clipSetId, eRagdollTriggerTypes _ragdollType, bool _animated );
	CClonedDamageElectricInfo();
	~CClonedDamageElectricInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_DAMAGE_ELECTRIC;}

	u32 GetDamageTotalTime() const { return m_uDamageTotalTime; }
	fwMvClipSetId GetClipSetId() const { return m_ClipSetId; }
	eRagdollTriggerTypes GetRagdollType() const { return m_RagdollType; }
	bool GetAnimated() const { return m_Animated; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_uDamageTotalTime, SIZEOF_DAMAGE_TOTAL_TIME, "Damage Total Time");
		SERIALISE_ANIMATION_CLIP(serialiser, m_ClipSetId, m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall, "Animation");
		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_RagdollType), SIZEOF_RAGDOLL_TYPE, "Ragdoll Type");
		SERIALISE_BOOL(serialiser, m_Animated, "Animated");
	}

private:

	CClonedDamageElectricInfo(const CClonedDamageElectricInfo &);
	CClonedDamageElectricInfo &operator=(const CClonedDamageElectricInfo &);

	static const unsigned int SIZEOF_DAMAGE_TOTAL_TIME	= 32;
	static const unsigned int SIZEOF_RAGDOLL_TYPE = datBitsNeeded<RAGDOLL_TRIGGER_NUM-1>::COUNT;

	fwMvClipSetId m_ClipSetId;
	u32 m_uDamageTotalTime;
	eRagdollTriggerTypes m_RagdollType;
	bool m_Animated;

	fwMvClipId m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;
};

////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Task covers all damage reactions that trigger ragdoll (stun gun, rubber bullets, tranquilizer), not just electric!
//-------------------------------------------------------------------------
class CTaskDamageElectric : public CTaskFSMClone
{
public:

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_FallsOutofVehicleVelocity;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	CTaskDamageElectric(fwMvClipSetId clipSetId, u32 uTime, eRagdollTriggerTypes ragdollType, CTaskNMControl* forcedNMTask = NULL, bool forceAnimated = false);
	CTaskDamageElectric(fwMvClipSetId clipSetId, u32 uTime, eRagdollTriggerTypes ragdollType,
		CEntity *pFiringEntity, const u32 uWeaponHash, u16 hitComponent, const Vector3 &vHitPosition, const Vector3 &vHitDirection, 
		const Vector3 &vHitPolyNormal);
	virtual ~CTaskDamageElectric();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_DAMAGE_ELECTRIC; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const;

#if !__FINAL
	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// PURPOSE: Reset the damage start time and set a new total time, and update the impact info
	void Update(u32 uTime, CEntity *pFiringEntity, const u32 uWeaponHash, u16 hitComponent, const Vector3 &vHitPosition, const Vector3 &vHitDirection,
		const Vector3 &vHitPolyNormal);

	// PURPOSE: Called by ped event scanner when checking dead peds tasks if this returns true a damage event is not generated if a dead ped runs this task
	virtual bool HandlesDeadPed() { return true; }

	// PURPOSE: Used by nm behaviours and getups to decide if they can abort for another task.
	//			If this isn't defined, events triggering this task will wait till another nm behaviour
	//			ends before taking effect
	virtual bool HandlesRagdoll(const CPed*) const { return true; }

	// PURPOSE: Called by the damage event generation system to decide whether or not to send a new damage event.
	//			We don't want to do this with electrocute??
	virtual bool RecordDamage(CDamageInfo& UNUSED_PARAM(damageInfo)){ return true; }

	// PURPOSE: When not using the nm shot task, set the contact position so that
	//			a force can be applied away from the electrocuting object.
	void SetContactPosition(const Vector3& pos) { m_vHitPosition = pos; }

protected:

	// PURPOSE: Update the peds facial clip
	void ProcessFacialIdle();

	enum State
	{
		State_Init,
		State_DamageAnimated,
		State_DamageNM,
		State_Quit,
	};

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

	// PURPOSE: Helper function to handle dropping the held weapon.
	void DropWeapon( CPed* pPed );

	// Clone task implementation
	// Returns true below as it always takes its state from the remote machine
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
    virtual void				OnCloneTaskNoLongerRunningOnOwner();
	CTaskFSMClone*				CreateTaskForClonePed(CPed *pPed);
	CTaskFSMClone*				CreateTaskForLocalPed(CPed *pPed);

private:

	////////////////////////////////////////////////////////////////////////////////

	FSM_Return StateInit_OnUpdate();
	FSM_Return StateDamageAnimated_OnEnter();
	FSM_Return StateDamageAnimated_OnUpdate();
	FSM_Return StateDamageNM_OnEnter();
	FSM_Return StateDamageNM_OnEnterClone();
	FSM_Return StateDamageNM_OnUpdate();

	// Are we out of time?
	bool GetIsOutOfTime() const;

	////////////////////////////////////////////////////////////////////////////////

	Vector3 m_vHitPosition;
	Vector3 m_vHitDirection;
	Vector3 m_vHitPolyNormal;
	CEntity *m_pFiringEntity;
	u32 m_uWeaponHash;
	fwMvClipSetId m_ClipSetId;
	u32 m_uDamageStartTime;
	u32 m_uDamageTotalTime;
	eRagdollTriggerTypes m_RagdollType;
	RegdaiTask m_forcedNMTask;
	f32 m_fSteeringBias;
	bool m_bUseShotReaction;
	u16 m_hitComponent;
	bool m_bRestartNMTask : 1;
	bool m_bForceAnimated : 1;

	CGenericClipHelper m_networkHelper;
	
	static const fwMvClipId ms_damageClipId;
	static const fwMvClipId ms_damageVehicleClipId;
};

////////////////////////////////////////////////////////////////////////////////

class CTaskRevive : public CTaskFSMClone
{

public:

	enum
	{
		State_CreateFirstSubTask,
		State_ControlSubTask,
		State_CreateNextSubTask,
		State_Quit,
	};

public:

	CTaskRevive(CPed* pMedic, CVehicle* pAmbulance);
	~CTaskRevive();

	virtual aiTask* Copy() const { return rage_new CTaskRevive(m_pMedic, m_pAmbulance); }

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_REVIVE;}

#if !__FINAL
	virtual void Debug() const {GetSubTask()->Debug();}
	virtual atString GetName() const {return atString("Revive");}
#endif 

	// Search sub tasks for the first instance of the specified task type
	virtual bool				IsValidForMotionTask(CTaskMotionBase& pTask) const;

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	s32							GetDefaultStateAfterAbort() const {return State_Quit;}

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	// Basic FSM implementation, for the inherited class
	virtual	FSM_Return			UpdateFSM	(const s32 UNUSED_PARAM(iState), const FSM_Event UNUSED_PARAM(iEvent));

	// Clone task implementation
	// Returns true below as it always takes its state from the remote machine
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed));

public:

	static bool IsRevivableDamageType(u32 weaponHash);

private:

	FSM_Return	StateCreateFirstSubTask_OnEnter(CPed* pPed);
	FSM_Return	StateCreateFirstSubTask_OnUpdate(CPed* pPed);

	FSM_Return	StateControlSubTask_OnEnter(CPed* pPed);
	FSM_Return	StateControlSubTask_OnUpdate(CPed* pPed);

	FSM_Return	StateCreateNextSubTask_OnEnter(CPed* pPed);
	FSM_Return	StateCreateNextSubTask_OnUpdate(CPed* pPed);

private:

	virtual aiTask* CreateNextSubTask(CPed* pPed );
	virtual aiTask* ControlSubTask(CPed* pPed);
	virtual aiTask* CreateFirstSubTask(CPed* pPed);

private:

	void HelperRequestInjuredClipset(CPed* pPed);

	CClipRequestHelper		m_moveGroupRequestHelper;
	RegdPed					m_pMedic;
	RegdVeh					m_pAmbulance;

private:

	aiTask *m_pNewTaskToSet; 

};

//-------------------------------------------------------------------------
// TaskInfo class for TaskRevive
//-------------------------------------------------------------------------

class CClonedReviveInfo : public CSerialisedFSMTaskInfo
{

public:
	CClonedReviveInfo( 
		s32 const			_state, 
		CEntity*			_pMedic,
		CEntity*			_pAmbulance
		);

	CClonedReviveInfo();
	~CClonedReviveInfo();

	virtual s32							GetTaskInfoType( ) const	{ return INFO_TYPE_REVIVE;}

	virtual CTaskFSMClone*				CreateCloneFSMTask();

	inline const CEntity*				GetMedic()	const			{ return m_pMedic.GetEntity();	}
	inline CEntity*						GetMedic()					{ return m_pMedic.GetEntity();	}

	inline const CEntity*				GetAmbulance()	const		{ return m_pAmbulance.GetEntity();	}
	inline CEntity*						GetAmbulance()				{ return m_pAmbulance.GetEntity();	}

	virtual bool						HasState() const			{ return true; }
	virtual u32							GetSizeOfState() const		{ return datBitsNeeded<CTaskRevive::State_Quit>::COUNT; }
	virtual const char*						GetStatusName(u32) const	{ return "Status";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		ObjectId targetID = m_pMedic.GetEntityID();
		SERIALISE_OBJECTID(serialiser, targetID, "Medic Entity");
		m_pMedic.SetEntityID(targetID);

		targetID = m_pAmbulance.GetEntityID();
		SERIALISE_OBJECTID(serialiser, targetID, "Ambulance Entity");
		m_pAmbulance.SetEntityID(targetID);
	}

	virtual CTask *CreateLocalTask(fwEntity *pEntity);

private:

	CClonedReviveInfo(const CClonedReviveInfo &);
	CClonedReviveInfo &operator=(const CClonedReviveInfo &);

	CSyncedEntity m_pMedic;
	CSyncedEntity m_pAmbulance;
};


#endif
