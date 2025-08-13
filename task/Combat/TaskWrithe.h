#ifndef TASK_WRITHE_H
#define TASK_WRITHE_H

#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Weapons/WeaponTarget.h"
#include "Peds/ped.h"
#include "peds/QueriableInterface.h"

class CTaskWrithe : public CTaskFSMClone
{
public:

	CTaskWrithe(const CWeaponTarget& Target, bool bFromGetUp, bool bEnableRagdollOnCollision = true, bool bForceShootFromGround = false);
	~CTaskWrithe();

	virtual aiTask* Copy() const 
	{ 
		CTaskWrithe* pCopy = rage_new CTaskWrithe(m_Target, m_bFromGetUp, m_bEnableRagdollOnCollision, m_bForceShootFromGround);
		pCopy->SetMinFireLoops(m_nMinFireloops);
		pCopy->SetShootFromGroundTimer(m_shootFromGroundTimer);
		return pCopy;
	}
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_WRITHE; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_WRITHE; }
	virtual s32 GetDefaultStateAfterAbort() const { return State_Start; }
	virtual	void CleanUp();

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32 );
#endif // !__FINAL

	enum
	{
		NEXT_STATE_COLLAPSE = BIT0,
		NEXT_STATE_WRITHE = BIT1,
		NEXT_STATE_TRANSITION = BIT2,
		NEXT_STATE_DIE = BIT3,
	};

	FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool ProcessMoveSignals();

	static fwMvClipSetId GetIdleClipSet(CPed* pPed) { return ((pPed->GetHurtEndTime() & 1) ? m_ClipSetIdLoopVar1 : m_ClipSetIdLoopVar2); }
	static bool GetDieClipSetAndRandClip(fwMvClipSetId& ClipSetId, fwMvClipId& ClipId);
	static bool StreamReqResourcesIn(class CWritheClipSetRequestHelper* pClipSetRequestHelper, CPed* pPed, int iNextState);
	static bool StreamReqResourcesInOldAndBad(CPed* pPed, int iNextState);
	static const crClip* GetRandClipFromClipSet(fwMvClipSetId ClipSet, s32& clipHash);
	static const crClip* GetClipFromClipSet(fwMvClipSetId const& ClipSet, s32 const clipHash);

	void SetMinFireLoops(int nMinFireLoops) {m_nMinFireloops = nMinFireLoops;}
	void SetShootFromGroundTimer(int nShootFromGroundTimer) {m_shootFromGroundTimer = nShootFromGroundTimer;}
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual void				HandleCloneToLocalSwitch(CPed* pPed);
	virtual void				HandleLocalToCloneSwitch(CPed* pPed);
	virtual bool				IsInScope(const CPed* pPed);
	bool						IsInCombat() const;

protected:

	void Start_OnEnter();
	FSM_Return Start_OnUpdate();
	void ToWrithe_OnEnter();
	FSM_Return ToWrithe_OnUpdate();
	bool ToWrithe_OnProcessMoveSignals();
	void FromShootFromGround_OnEnter();
	FSM_Return FromShootFromGround_OnUpdate();
	bool FromShootFromGround_OnProcessMoveSignals();
	void ToShootFromGround_OnEnter();
	FSM_Return ToShootFromGround_OnUpdate();
	bool ToShootFromGround_OnProcessMoveSignals();
	void Loop_OnEnter(CPed* pPed);
	FSM_Return Loop_OnUpdate();
	bool Loop_OnProcessMoveSignals();
	FSM_Return StreamGroundClips_OnUpdate();
	void ShootFromGround_OnEnter();
	FSM_Return ShootFromGround_OnUpdate();
	void ShootFromGround_OnExit();
	bool ShootFromGround_OnProcessMoveSignals();
	FSM_Return StreamDeathAnims_OnUpdate();
	void Die_OnEnter();
	FSM_Return Die_OnUpdate();
	void Finish_OnEnter();
	FSM_Return Finish_OnUpdate();

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	void TaskSetState(u32 const iState);

public:

	// 
	// Clone target management functions (public so CClonedWritheInfo can get / set)....
	//

	void			SetTargetOutOfScopeID(ObjectId _networkId )	{ m_TargetOutOfScopeId_Clone = _networkId;		 }
	inline ObjectId GetTargetOutOfScopeID(void )				{ return m_TargetOutOfScopeId_Clone; }

	void			SetInflictorID(ObjectId _networkId )		{ m_DamageEntity = _networkId;		 }
	inline ObjectId GetInflictorID(void )						{ return m_DamageEntity; }

public:

	enum
	{
		State_Start,
		State_ToWrithe,
		State_FromShootFromGround,
		State_ToShootFromGround,
		State_Loop,
		State_StreamGroundClips,
		State_ShootFromGround,
		State_StreamDeathAnims,
		State_Die,
		State_Finish,
		State_Invalid,
	};

	static const float MIN_ANIM_RATE;
	static const float MAX_ANIM_RATE;
	static const u32 INVALID_CLIP_HASH;

public:

#if DEBUG_DRAW
	void DebugCloneTaskInformation(void);
#endif /* DEBUG_DRAW */

private:

	void UpdateTarget();
	bool GetIsTargetValid(const CEntity* pTarget) const;
	void FindAndRegisterThreats(CPed* pPed, CPedTargetting* pPedTargeting);

	void ActivateTimeslicing();
	bool InitiateMoveNetwork();

	CMoveNetworkHelper m_NetworkHelper;
	fwClipSetRequestHelper m_ClipSetRequestHelper;

	CTaskGameTimer m_Timer;
	CWeaponTarget m_Target;
	int m_nFrameDisableRagdollState;
	int m_nMinFireloops;
	int m_nMaxFireloops;
	int m_nFireLoops;
	float m_fAnimRate;
	float m_fPhase;
	float m_fPhaseStartShooting;

	s32 m_shootFromGroundTimer;
	fwMvClipSetId m_idleClipSetId;
	s32	m_idleClipHash;
	s32 m_startClipHash;
	u32 m_uLastTimeBeggingAudioPlayed;

	ObjectId m_TargetOutOfScopeId_Clone; 	// Target syncing
	ObjectId m_DamageEntity;

	bool m_bFromGetUp;
	bool m_bFromShootFromGround;
	bool m_bDieInLoop;
	bool m_bAnimFinished;
	bool m_bEnableRagdollOnCollision;
	bool m_bStartedByScript;
	bool m_bFetchAsyncTargetTest;
	bool m_bLeaveMoveNetwork;
	bool m_bGoStraightToLoop;
	bool m_bOldInfiniteAmmo;
	bool m_bOldInfiniteClip;
	bool m_bForceShootFromGround;

public:
	static dev_float ms_fMaxDistToValidTarget;
	static dev_s32 ms_uTimeBetweenTargetSearches;

	static const fwMvClipId ms_ToWritheClipId;
	static const fwMvClipId ms_LoopClipId;
	static const fwMvClipId ms_DieClipId;

	static const fwMvFloatId ms_AnimRate;
	static const fwMvFloatId ms_Phase;

	static const fwMvBooleanId ms_AnimFinished;
	static const fwMvBooleanId ms_OnEnterLoop;
	static const fwMvBooleanId ms_OnEnterDie;
	static const fwMvBooleanId ms_OnEnterToShootFromGround;
	static const fwMvBooleanId ms_UseHeadIK;

	//static const fwMvRequestId ms_ToWrithe;
	static const fwMvRequestId ms_ToLoop;
	static const fwMvRequestId ms_ToDie;
	static const fwMvRequestId ms_ToShootFromGround;

	static const fwMvClipSetId m_ClipSetIdToWrithe;
	static const fwMvClipSetId m_ClipSetIdLoop;
	static const fwMvClipSetId m_ClipSetIdLoopVar1;
	static const fwMvClipSetId m_ClipSetIdLoopVar2;
	static const fwMvClipSetId m_ClipSetIdWritheTransition;
	static const fwMvClipSetId ms_ClipSetIdDie;

	static const fwMvFlagId ms_FlagFromGetUp;
	static const fwMvFlagId ms_FlagFromShootFromGround;
};

//-------------------------------------------------------------------------
// Task info for Writhe
//-------------------------------------------------------------------------
class CClonedWritheInfo : public CSerialisedFSMTaskInfo
{
public:

	struct InitParams
	{
		InitParams
		(
			u32 state, 
			CWeaponTarget const * weaponTarget, 
			bool bFromGetUp, 
			bool bDieInLoop, 
			bool bFromShootFromGround,
			bool m_bEnableRagdollFromCollision,
			float fAnimRate,
			ObjectId damageEntity,
			s16 shootFromGroundTime,
			fwMvClipSetId loopClipSetId,
			s32 loopClipHash,
			s32 startClipHash,
			bool bForceShootFromGround
		);

		u32						m_state;
		CWeaponTarget const*	m_weaponTarget;
		bool					m_bFromGetUp;
		bool					m_bDieInLoop;
		bool					m_bFromShootFromGround;
		bool					m_bEnableRagdollFromCollision;
		float					m_fAnimRate;
		ObjectId				m_damageEntity;
		s16						m_shootFromGroundTimer;
		fwMvClipSetId			m_idleClipSetId;
		s32						m_idleClipHash;
		s32						m_startClipHash;
		bool					m_bForceShootFromGround;
	};

public:

    CClonedWritheInfo(InitParams const& initParams);

	CClonedWritheInfo();

    ~CClonedWritheInfo()
	{}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_WRITHE;}
	virtual CTaskFSMClone*	CreateCloneFSMTask();

	virtual bool			HasState() const				{ return true; }
	virtual u32				GetSizeOfState() const			{ return datBitsNeeded<CTaskWrithe::State_Finish>::COUNT; }
	virtual const char*			GetStatusName(u32) const		{ return "Status";}

	inline bool				GetFromGetUp(void) const				{ return m_bFromGetUp; }
	inline bool				GetDieInLoop(void) const				{ return m_bDieInLoop; }
	inline bool				GetFromShootFromGround(void) const		{ return m_bFromShootFromGround; }
	inline bool				GetEnableRagdollFromCollision(void) const { return m_bEnableRagdollOnCollision; }
	inline float			GetAnimRate(void)  const				{ return m_fAnimRate;  }
	inline ObjectId			GetDamageEntity(void) const				{ return m_damageEntity; }
	inline u16				GetShootFromGroundTime(void) const		{ return m_shootFromGroundTimer; }
	inline bool				GetForceShootFromGround(void) const		{ return m_bForceShootFromGround; }

	inline fwMvClipSetId	GetClipSetId(void) const				{ return m_idleClipSetId; }
	inline u32				GetClipHash(void) const					{ return m_idleClipHash; }

	inline u32				GetStartClipHash(void) const			{ return m_startClipHash; }

	virtual const CEntity*	GetTarget()	const			{ return (const CEntity*) m_weaponTargetHelper.GetTargetEntity(); }
	virtual CEntity*		GetTarget()					{ return (CEntity*) m_weaponTargetHelper.GetTargetEntity(); }

	inline CClonedTaskInfoWeaponTargetHelper const &	GetWeaponTargetHelper() const	{ return m_weaponTargetHelper; }
	inline CClonedTaskInfoWeaponTargetHelper&			GetWeaponTargetHelper()			{ return m_weaponTargetHelper; }

public:

	void Serialise(CSyncDataBase& serialiser)
	{
		bool fromGetUp = m_bFromGetUp;
		SERIALISE_BOOL(serialiser, fromGetUp, "From Get up");
		m_bFromGetUp = fromGetUp;

		bool dieInLoop = m_bDieInLoop;
		SERIALISE_BOOL(serialiser, dieInLoop, "Die in loop");
		m_bDieInLoop = dieInLoop;

		bool fromShootFromGround = m_bFromShootFromGround;
		SERIALISE_BOOL(serialiser, fromShootFromGround, "From Shoot From Ground");
		m_bFromShootFromGround = fromShootFromGround;

		bool enableRagdollOnCollision = m_bEnableRagdollOnCollision;
		SERIALISE_BOOL(serialiser, enableRagdollOnCollision, "Enable Ragdoll on Collision");
		m_bEnableRagdollOnCollision = enableRagdollOnCollision;

		bool forceShootFromGround = m_bForceShootFromGround;
		SERIALISE_BOOL(serialiser, forceShootFromGround, "From Get up");
		m_bForceShootFromGround = forceShootFromGround;

		SERIALISE_PACKED_FLOAT(serialiser, m_fAnimRate,			CTaskWrithe::MAX_ANIM_RATE, SIZEOF_ANIM_RATE, "Anim Rate");

		SERIALISE_OBJECTID(serialiser, m_damageEntity, "Damage Entity");
		SERIALISE_INTEGER(serialiser, m_shootFromGroundTimer,		SIZEOF_SHOOT_TIMER, "Shoot From Ground Timer");

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		m_weaponTargetHelper.Serialise(serialiser);

		SERIALISE_ANIMATION_CLIP(serialiser, m_idleClipSetId, m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall, "Loop Clip Set");
		SERIALISE_INTEGER(serialiser, m_idleClipHash, SIZEOF_HASH, "Loop Clip Hash");	

		SERIALISE_INTEGER(serialiser, m_startClipHash, SIZEOF_HASH, "Start Clip Hash");	
	}

private:

	static u32 const SIZEOF_ANIM_RATE	= 7;
	static u32 const SIZEOF_HASH		= 32;
	static u32 const SIZEOF_SHOOT_TIMER = 15;

private:

    CClonedWritheInfo(const CClonedWritheInfo &);
    CClonedWritheInfo &operator=(const CClonedWritheInfo &);

	float			m_fAnimRate;

	CClonedTaskInfoWeaponTargetHelper m_weaponTargetHelper;

	fwMvClipSetId	m_idleClipSetId;
	s32				m_idleClipHash;
	s32				m_startClipHash;


	ObjectId		m_damageEntity;
	s16				m_shootFromGroundTimer;
	bool			m_bFromGetUp:1;
	bool			m_bDieInLoop:1;
	bool			m_bFromShootFromGround:1;
	bool			m_bEnableRagdollOnCollision:1;
	bool			m_bForceShootFromGround:1;

	static fwMvClipId m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;
};

class CWritheClipSetRequestHelper
{
public:
	inline void AddClipSetRequest(const fwMvClipSetId &requestedClipSetId)
	{	
		if (!m_ClipSetRequestHelper.HaveAddedClipSet(requestedClipSetId))
			m_ClipSetRequestHelper.AddClipSetRequest(requestedClipSetId);
	}
	inline bool RequestAllClipSets(){ return m_ClipSetRequestHelper.RequestAllClipSets(); }
	inline void ReleaseAllClipSets(){ m_ClipSetRequestHelper.ReleaseAllClipSets(); }

	CMultipleClipSetRequestHelper<4> m_ClipSetRequestHelper;
};

class CWritheClipSetRequestHelperPool
{
public:
	static CWritheClipSetRequestHelper* GetNextFreeHelper()
	{
//		if (NetworkInterface::IsGameInProgress())
//			return NULL;

		for (int i = 0; i < ms_iPoolSize; ++i)		
		{
			if (!ms_WritheClipSetHelperSlotUsed[i])
			{
				ms_WritheClipSetHelperSlotUsed[i] = true;
				return &ms_WritheClipSetHelpers[i];
			}
		}

		return NULL;	
	}

	static void ReleaseClipSetRequestHelper(CWritheClipSetRequestHelper* pWritheClipSetRequestHelper)
	{
		// Could use memory pos to calc index instead but meh...
		for (int i = 0; i < ms_iPoolSize; ++i)
		{
			if (&ms_WritheClipSetHelpers[i] == pWritheClipSetRequestHelper)
			{
				pWritheClipSetRequestHelper->ReleaseAllClipSets();	// Just to be sure
				ms_WritheClipSetHelperSlotUsed[i] = false;
				break;
			}
		}
	}

private:
	static const int ms_iPoolSize = 8;	// 
	static CWritheClipSetRequestHelper ms_WritheClipSetHelpers[ms_iPoolSize];
	static bool ms_WritheClipSetHelperSlotUsed[ms_iPoolSize];
};


#endif

