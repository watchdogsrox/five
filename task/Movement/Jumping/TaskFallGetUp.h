#ifndef TASK_FALL_GET_UP_H
#define TASK_FALL_GET_UP_H

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Network/General/NetworkUtil.h"
#include "Peds/QueriableInterface.h"

//-------------------------------------------------------------------------
// Task info for falling over
//-------------------------------------------------------------------------
class CClonedFallOverInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedFallOverInfo(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fDownTime, float fStartPhase, bool bKnockedIntoAir, bool bIncreasedRate, bool bCapsuleRestrictionsDisabled);
	CClonedFallOverInfo();
	~CClonedFallOverInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_FALL_OVER;}

	virtual bool AutoCreateCloneTask(CPed* pPed);

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual fwMvClipSetId	GetClipSetId() const	{ return m_clipSetId; }
	virtual fwMvClipId		GetClipId()				{ return m_clipId; }
	float		GetDownTime()		{ return m_fDownTime; }
	float		GetStartPhase() const { return m_fStartPhase; }
	bool		GetKnockedIntoAir() { return m_bKnockedIntoAir; }
	bool		GetIncreasedRate()	{ return m_bIncreasedRate; }
	bool		GetCapsuleRestrictionsDisabled()	{ return m_bCapsuleRestrictionsDisabled; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

        SERIALISE_ANIMATION_CLIP(serialiser, m_clipSetId, m_clipId, "Animation");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fDownTime, 8.0f, SIZEOF_DOWNTIME, "Down time");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fStartPhase, 1.0f, SIZEOF_STARTPHASE, "Start phase");
		SERIALISE_BOOL(serialiser, m_bKnockedIntoAir, "Knocked into air");
		SERIALISE_BOOL(serialiser, m_bIncreasedRate, "Increased rate");
		SERIALISE_BOOL(serialiser, m_bCapsuleRestrictionsDisabled, "Capsule Restrictions Disabled");
	}

private:

	CClonedFallOverInfo(const CClonedFallOverInfo &);
	CClonedFallOverInfo &operator=(const CClonedFallOverInfo &);

	static const unsigned int SIZEOF_DOWNTIME = 4;
	static const unsigned int SIZEOF_STARTPHASE = 8;

	fwMvClipSetId m_clipSetId;
	fwMvClipId		m_clipId;
	float		m_fDownTime;
	float		m_fStartPhase;
	bool		m_bKnockedIntoAir;
	bool		m_bIncreasedRate;
	bool		m_bCapsuleRestrictionsDisabled;
};

//////////////////////////////////////////////////////////////////////////
// CTaskFallOver
//////////////////////////////////////////////////////////////////////////

class CTaskFallOver : public CTaskFSMClone
{
public:

	typedef enum
	{
		State_Fall = 0,
		State_LieOnGround,
		State_Quit,
	} State;

	enum eFallDirection
	{
		kDirBack,
		kDirFront,
		kDirLeft,
		kDirRight,
		kNumFallDirections
	};

	enum eFallContext
	{
		kContextStandard,
		kContextVehicleHit,
		kContextFallImpact,
		kContextDrown,
		kContextOnFire,
		kContextHeadShot,
		kContextShotPistol,
		kContextShotSMG,
		kContextShotAutomatic,
		kContextShotShotgun,
		kContextShotSniper,
		kContextLegShot,
		kContextExplosion,
		kNumFallContexts
	};

	CTaskFallOver(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fDownTime = 0.0f, float fStartPhase = 0.0f, float fRootSlopeFixupPhase = -1.0f);
	CTaskFallOver(const CTaskFallOver& other);

	static eFallContext ComputeFallContext(u32 weaponHash, u32 hitBoneTag);
	static bool PickFallAnimation(CPed* pPed, eFallContext context, eFallDirection dir, fwMvClipSetId& clipSetOut, fwMvClipId& clipOut);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskFallOver(m_clipSetId, m_clipId, m_fDownTimeTimer, m_fStartPhase, m_fRootSlopeFixupPhase); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_FALL_OVER; }

	// Used to apply the large initial impulses from a vehicle collision
	static void ApplyInitialVehicleImpact(CVehicle* pVehicle, CPed* pPed);

	void SetCapsuleRestrictionsDisabled(bool set) { m_bCapsuleRestrictionsDisabled = set; }

	// Clone task implementation
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	fwMvClipSetId GetClipSetId() const { return m_clipSetId; }
	fwMvClipId GetClipId() const { return m_clipId; }
	float GetClipPhase() const { return GetClipHelper() ? GetClipHelper()->GetPhase() : m_fStartPhase; }

	static bank_float			ms_fCapsuleRadiusMult;
	static bank_float			ms_fCapsuleRadiusGrowSpeedMult;

protected:

	virtual FSM_Return ProcessPreFSM();

	virtual bool ProcessPostPreRender();
	
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual	void CleanUp();

	// Determine whether or not the task should abort
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	//
	// State functions
	//

	FSM_Return StateFallOnEnter(CPed* pPed);
	FSM_Return StateFallOnUpdate(CPed* pPed);
	FSM_Return StateLieOnGroundOnEnter(CPed* pPed);
	FSM_Return StateLieOnGroundOnUpdate(CPed* pPed);
	FSM_Return StateLieOnGroundOnUpdateClone(CPed* pPed);

	//
	// Members
	//

	// The fall over clip to perform
	fwMvClipSetId m_clipSetId;
	fwMvClipId m_clipId;

	// The downtime timer
	float m_fDownTimeTimer;

	// The phase at which to start the fall
	float m_fStartPhase;

	// The phase to start root slope fixup
	float m_fRootSlopeFixupPhase;

	// The amount of time the ped has been falling for
	float m_fFallTime;

	// set when the ped is being knocked into the air
	bool m_bKnockedIntoAir;

	// set when the fall over clip is sped up
	bool m_bIncreasedRate;

	// set on clone task when main task has ended
	bool m_bFinishTask;

	// set when capsule restrictions are disabled
	bool m_bCapsuleRestrictionsDisabled;
};

//-------------------------------------------------------------------------
// Task info for fall and get up
//-------------------------------------------------------------------------
class CClonedFallAndGetUpInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedFallAndGetUpInfo() {}
	~CClonedFallAndGetUpInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_FALL_AND_GET_UP;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedFallAndGetUpInfo(const CClonedFallAndGetUpInfo &);
	CClonedFallAndGetUpInfo &operator=(const CClonedFallAndGetUpInfo &);
};

//////////////////////////////////////////////////////////////////////////
// CTaskFallAndGetUp
//////////////////////////////////////////////////////////////////////////

class CTaskFallAndGetUp : public CTaskFSMClone
{
public:

	CTaskFallAndGetUp(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fDownTime, float fStartPhase = 0.0f);
	CTaskFallAndGetUp(s32 iDirection, float fDownTime, float fStartPhase = 0.0f);
	CTaskFallAndGetUp(const CTaskFallAndGetUp& other);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskFallAndGetUp(*this); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_FALL_AND_GET_UP; }

	// Give the specified fall direction, return an clip set id and clip id
	void GetClipSetIdAndClipIdFromFallDirection(s32 iDirection, fwMvClipSetId &clipSetId, fwMvClipId &clipId);

	fwMvClipSetId GetClipSetId() { return m_clipSetId; }
	fwMvClipId GetClipId() { return m_clipId; }
	float GetClipPhase();
	// Clone task implementation
	virtual bool				HasState()							{ return false; }
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	void SetCapsuleRestrictionsDisabled(bool set) { m_bCapsuleRestrictionsDisabled = set; }

protected:

	typedef enum
	{
		State_Fall = 0,
		State_GetUp,
		State_Quit,
	} State;

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	//
	// Members
	//

	// Specified clip for falling over
	fwMvClipSetId m_clipSetId;
	fwMvClipId m_clipId;

	// How long to stay down on the ground after falling over
	float m_fDownTime;

	// At what point in the animation to start (used when task migrates in the network game)
	float m_fStartPhase;

	// The user can specify the direction to fall in.
	s32 m_iFallDirection;

	// set when capsule restrictions are disabled
	bool m_bCapsuleRestrictionsDisabled;
};

//////////////////////////////////////////////////////////////////////////
// CTaskGetUpAndStandStill
//////////////////////////////////////////////////////////////////////////

class CTaskGetUpAndStandStill : public CTask
{
public:

	CTaskGetUpAndStandStill();

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskGetUpAndStandStill(); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_GET_UP_AND_STAND_STILL; }

protected:

	typedef enum
	{
		State_GetUp = 0,
		State_StandStill,
		State_Quit,
	} State;

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL
};

#endif // TASK_FALL_GET_UP_H
