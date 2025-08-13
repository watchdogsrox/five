#ifndef _TASK_SLOPE_SCRAMBLE_H_
#define _TASK_SLOPE_SCRAMBLE_H_

#include "Peds/ped.h"
#include "task/system/Task.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "task/Motion/TaskMotionBase.h"

class CTaskSlopeScramble : public CTaskFSMClone
{
public:

	CTaskSlopeScramble();
	virtual ~CTaskSlopeScramble();

	enum
	{
		State_Initial,
		State_DoSlopeScramble,
		State_DoSlopeScrambleOutro,
		State_Quit,
	};

    // Clone task implementation
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual s32			GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*		Copy() const { return rage_new CTaskSlopeScramble(); }
	virtual s32			GetTaskTypeInternal() const { return CTaskTypes::TASK_ON_FOOT_SLOPE_SCRAMBLE; }

	virtual bool		ProcessMoveSignals();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif //!__FINAL

	// FSM optional functions
	virtual	void		CleanUp();

	bool GetHasFPSUpperBodyAnim() {return m_bHasFPSUpperBodyAnim;}

	static	bool		CanDoSlopeScramble(CPed *pPed, bool bActive = false);
	static	bool		GetIsTooSteep(const Vector3 &normal);						// Is this face steep enough to slide?
	static  float		GetTimeBeforeScramble(const Vector3 &groundNormal, const Vector3 &moveVelocity, const Vector3 &upVec);

protected:

private:

	typedef	enum
	{
		SCRAMBLE_FORWARD = 0,
		SCRAMBLE_BACKWARD
	}	SCRAMBLE_DIRECTION;

	SCRAMBLE_DIRECTION	m_scrambleDirection;
	bool				m_SwappedDirection;

	static	float		GetActivationAngle();										// Together they dictate the start and stop poly face angles for the slide
	static	float		GetDeActivationAngle();										// If we're started, we don't stop until we get an angle below the deactivation angle
	static	float		GetProbeTestAngle();										// 

	static	bool		GetIsTooShallow(const Vector3 &normal);						// Is this face shallow enough to prevent slide?

	static	bool		IsStoodOnEntity(CPed *pPed);								// Is the ped stood on an entity?
	static	bool		IsEntitySurface(WorldProbe::CShapeTestHitPoint &result);	// Is the passed probe result hitting an entity?
	static	bool		CanSlideOnSurface(const Vector3 &normal, phMaterialMgr::Id matID);	// Is the passed material slippy enough to slide on?
	static	float		GetCheckDistance(const Vector3& groundNormal);											// Get the distance ahead for probe checks
	static  bool		NoGroundSteepSlopeCheck(CPed *pPed);

	static	bool		CheckSurfaceTypeAndDistance(CPed *pPed, const Vector3 &normal, float distToCheck);
	static	bool		CheckAheadForObjects(CPed *pPed, float distToCheck );
	bool				ShouldContinueSlopeScramble( CPed *pPed );

	SCRAMBLE_DIRECTION	GetFacingDirClipType(CPed *pPed, const bool bExcludeDeadZone = false);

	Vector3				DoSliding( const Vector3 &a_UnitNormal, Vector3 &a_InV, float a_Mass, float a_FrictionK, float a_Gravity, const Vector3 &a_UnitUpVector, float a_tDelta );

	bool				GetHasInterruptedClip(const CPed* pPed) const;

	float				GetBlendInDuration();
	float				GetBlendOutDuration();

	FSM_Return			StateInitial_OnEnter();
	FSM_Return			StateInitial_OnUpdate();

	FSM_Return			StateDoSlopeScramble_OnEnter();
	FSM_Return			StateDoSlopeScramble_OnUpdate();

	FSM_Return			StateDoSlopeScrambleOutro_OnEnter();
	FSM_Return			StateDoSlopeScrambleOutro_OnUpdate();

	float				m_TooShallowTimer;
	float				m_SlopeScrambleTimer;
	float				m_fTimeWithNoGround;

	Vector3				m_vVelocity;

	Vector3				m_vGroundNormal;

	bool				m_bHasFPSUpperBodyAnim;

	fwMvClipSetId		m_clipSetId;
	CMoveNetworkHelper	m_networkHelper;	// Loads the parent move network 

	static const fwMvBooleanId	ms_OnChosenForwardId;
	static const fwMvBooleanId	ms_OnChosenBackwardId;
	static const fwMvRequestId	ms_GoToScrambleForwardId;
	static const fwMvRequestId	ms_GoToScrambleBackwardId;
	static const fwMvRequestId	ms_GoToExitId;
	static const fwMvBooleanId	ms_OnOutroComplete;
	static const fwMvBooleanId	ms_InterruptibleId;
	static const fwMvFlagId		ms_FirstPersonMode;
	static const fwMvFlagId		ms_HasGrip;
	static const fwMvFloatId	ms_PitchId;
	static const fwMvClipSetVarId	ms_UpperBodyClipSetId;
	static const fwMvClipId		ms_WeaponBlockedClipId;
	static const fwMvClipId		ms_GripClipId;
};


class CClonedSlopeScrambleInfo : public CSerialisedFSMTaskInfo
{
public:

	struct InitParams
	{
		InitParams(s32 state, fwMvClipSetId clipSetId)
			: m_state(state)
			, m_ClipSetId(clipSetId)
		{
		}

		s32 m_state;
		fwMvClipSetId m_ClipSetId;
	};

public:

    CClonedSlopeScrambleInfo(InitParams& initParams)
		: m_ClipSetId(initParams.m_ClipSetId)
		, m_ClipId(CLIP_ID_INVALID)
	{
		SetStatusFromMainTaskState(initParams.m_state);
	}

	CClonedSlopeScrambleInfo()
		: m_ClipSetId(CLIP_SET_ID_INVALID)
		, m_ClipId(CLIP_ID_INVALID)
	{
	}

	s32 GetTaskInfoType() const {return INFO_TYPE_SLOPE_SCRAMBLE;}
	CTaskFSMClone* CreateCloneFSMTask();
	CTask* CreateTaskFromTaskInfo(fwEntity *pEntity);

    virtual bool HasState() const { return true; }
	virtual u32 GetSizeOfState() const { return datBitsNeeded<CTaskSlopeScramble::State_Quit>::COUNT; }

	fwMvClipSetId GetClipSetId() const { return m_ClipSetId; }

public:

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_ANIMATION_CLIP(serialiser, m_ClipSetId, m_ClipId, "Animation");
	}

private:

	CClonedSlopeScrambleInfo(const CClonedSlopeScrambleInfo &);
	CClonedSlopeScrambleInfo &operator=(const CClonedSlopeScrambleInfo &);

	fwMvClipSetId m_ClipSetId;
	fwMvClipId m_ClipId;
};

#endif	//_TASK_SLOPE_SCRAMBLE_H_