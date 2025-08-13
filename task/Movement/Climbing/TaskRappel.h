#ifndef INC_TASK_RAPPEL_H
#define INC_TASK_RAPPEL_H

// FILE :    TaskRappel.h
// PURPOSE : Controlling rappelling in difference areas
// CREATED : 18-10-2011

//Game headers
#include "parser/manager.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(weapon)

#define rappelAssert(cond)						RAGE_ASSERT(rappel,cond)
#define rappelAssertf(cond,fmt,...)				RAGE_ASSERTF(rappel,cond,fmt,##__VA_ARGS__)
#define rappelFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(rappel,cond,fmt,##__VA_ARGS__)
#define rappelVerifyf(cond,fmt,...)				RAGE_VERIFYF(rappel,cond,fmt,##__VA_ARGS__)
#define rappelFatalf(fmt,...)					RAGE_FATALF(rappel,fmt,##__VA_ARGS__)
#define rappelErrorf(fmt,...)					RAGE_ERRORF(rappel,fmt,##__VA_ARGS__)
#define rappelWarningf(fmt,...)					RAGE_WARNINGF(rappel,fmt,##__VA_ARGS__)
#define rappelDisplayf(fmt,...)					RAGE_DISPLAYF(rappel,fmt,##__VA_ARGS__)
#define rappelDebugf1(fmt,...)					RAGE_DEBUGF1(rappel,fmt,##__VA_ARGS__)
#define rappelDebugf2(fmt,...)					RAGE_DEBUGF2(rappel,fmt,##__VA_ARGS__)
#define rappelDebugf3(fmt,...)					RAGE_DEBUGF3(rappel,fmt,##__VA_ARGS__)
#define rappelLogf(severity,fmt,...)			RAGE_LOGF(rappel,severity,fmt,##__VA_ARGS__)
#define rappelCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,rappel,severity,fmt,##__VA_ARGS__)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace rage
{
	class ropeInstance;
	struct ropeAttachment;
}

////////////////////////////////////////
//   TASK_SWAT_HELI_PASSENGER_RAPPEL   //
////////////////////////////////////////

class CTaskHeliPassengerRappel : public CTaskFSMClone
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_fDefaultRopeLength;
		float m_fExtraRopeLength;
		float m_fExitDescendRate;
		float m_fDefaultDescendRate;
		float m_fStartDescendingDistToTargetSq;
		float m_fRopeUnwindRate;
		float m_fMinHeightToRappel;
		float m_fMaxHeliSpeedForRappel;

		PAR_PARSABLE;
	};

	// FSM states
	enum
	{
		State_Start = 0,
		State_CreateRope,
		State_ExitHeli,
		State_Idle,
		State_Jump,
		State_Descend,
		State_DetachFromRope,
		State_ExitOnGround,
		State_Finish
	};

	CTaskHeliPassengerRappel(float fMinRappelHeight, float fRopeLengthOverride = -1.0f);
	virtual ~CTaskHeliPassengerRappel();

	virtual void				CleanUp();
	virtual bool				ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// Task required implementations
	virtual aiTask*				Copy() const						{ return rage_new CTaskHeliPassengerRappel(m_fMinRappelHeight, m_fRopeLengthOverride); }
	virtual int					GetTaskTypeInternal() const					{ return CTaskTypes::TASK_HELI_PASSENGER_RAPPEL; }
	virtual s32					GetDefaultStateAfterAbort() const	{ return State_Finish; }

	// Help to control velocity here
	virtual bool				ProcessPhysics(float fTimeStep, int nTimeSlice);
	virtual bool				ProcessPostMovement();
	virtual bool				ProcessMoveSignals();

	s32							GetSeatIndex() { return m_iPedSeatIndex; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Clone task implementation
	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return true; }
    virtual bool                OverridesNetworkAttachment(CPed *UNUSED_PARAM(pPed)) { return true; }
	virtual bool				OverridesVehicleState() const	{ return GetState() > State_Start; }
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
 	virtual bool				ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType)) { return false; }
	virtual void                OnCloneTaskNoLongerRunningOnOwner();
	virtual bool				AllowsLocalHitReactions() const { return false; }
	virtual bool				IsInScope(const CPed* UNUSED_PARAM(pPed)) { return true; }

	// Local state function calls
	void						Start_OnEnter				();
	FSM_Return					Start_OnUpdate				();
	void						CreateRope_OnEnter			();
	FSM_Return					CreateRope_OnUpdate			();
	void						ExitHeli_OnEnter			();
	FSM_Return					ExitHeli_OnUpdate			();
	void						ExitHeli_OnProcessMoveSignals();
	void						Idle_OnEnter				();
	FSM_Return					Idle_OnUpdate				();
	void						Idle_OnExit					();
	void						Jump_OnEnter				();
	FSM_Return					Jump_OnUpdate				();
	void						Jump_OnExit					();
	void						Jump_OnProcessMoveSignals	();
	void						Descend_OnEnter				();
	FSM_Return					Descend_OnUpdate			();
	void						DetachFromRope_OnEnter		();
	FSM_Return					DetachFromRope_OnUpdate		();
	void						DetachFromRope_OnProcessMoveSignals();
	void						ExitOnGround_OnEnter		();
	FSM_Return					ExitOnGround_OnUpdate		();

	void						ActivateTimeslicing();
	bool						GetPedToGroundDiff			(float &fPedToGroundDiff, bool bUseStagingLocation);
	void						DetachPedFromHeli			(CPed* pPed);
	bool						HasValidCollision			();
	bool						IsSeatOnLeft				() { return m_iPedSeatIndex % 2 == 0; }
	fwMvClipSetId				GetRappelClipSet			();

	bool						IsUsingAccurateRopeMode() const;

	// The rope attachment for the ped who is running this task
	ropeAttachment* m_pRopeAttachment;

	// The position we should end up at after exiting
	Vector3 m_vHeliAttachOffset;

	// The minimum height required before we are allowed to rappel
	float m_fMinRappelHeight;

	// Override for the rope length
	float m_fRopeLengthOverride;

	// We need to store what seat index we used so we know what anim set to load for the move network
	s32 m_iPedSeatIndex;

	// Bool to control if we've hit our critical frame for pinning a vert to our hand
	bool m_bPinRopeToHand;

	// in MP the task needs to wait for a couple of seconds in the start state, to give the network code time to create the ped on other machines (generally swat
	// peds are created in the heli, then immediately start rappelling)
	u32 m_startTimer;

	// This is going to be our move network helper for the passengers
	CMoveNetworkHelper		m_moveNetworkHelper;
	fwClipSetRequestHelper	m_clipSetRequestHelper;

	// static variables	
	static const fwMvFlagId		ms_LandId;
	static const fwMvFlagId		ms_LandUnarmedId;
	static const fwMvBooleanId  ms_DetachClipFinishedId;
	static const fwMvBooleanId  ms_JumpClipFinishedId;
	static const fwMvBooleanId  ms_CriticalFrameId;
	static const fwMvBooleanId  ms_StartDescentId;
	static const fwMvClipId		ms_ExitSeatClipId;
	static const fwMvBooleanId  ms_CreateWeaponId;

	// MoVE signals
	bool m_bMoveExitHeliCriticalFrame : 1;
	bool m_bMoveJumpClipFinished : 1;
	bool m_bMoveJumpCriticalFrame : 1;
	bool m_bMoveJumpStartDescent : 1;
	bool m_bMoveDeatchCreateWeapon : 1;
	bool m_bMoveDetachClipFinished : 1;

	// our tunables
	static Tunables				sm_Tunables;
};

//-------------------------------------------------------------------------
// Task info for rappelling from a helicopter as a passenger
//-------------------------------------------------------------------------
class CClonedHeliPassengerRappelInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedHeliPassengerRappelInfo(s32 state);
	CClonedHeliPassengerRappelInfo() {}
	~CClonedHeliPassengerRappelInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_HELI_PASSENGER_RAPPEL;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskHeliPassengerRappel::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedHeliPassengerRappelInfo(const CClonedHeliPassengerRappelInfo &);

	CClonedHeliPassengerRappelInfo &operator=(const CClonedHeliPassengerRappelInfo &);
};

class CTaskRappel : public CTaskFSMClone
{
public:

	// FSM states
	// NOTE: Make sure these are in sync w/ the states in commands_task.sch!
	enum
	{
		State_Start = 0,
		State_ClimbOverWall,
        State_WaitForNetworkRope,
		State_Idle,
		State_IdleAtDestHeight,
		State_Descend,
		State_Jump,
		State_SmashWindow,
		State_Finish
	};

	struct Tunables : public CTuning
	{
		Tunables();

		float m_fJumpDescendRate;
		float m_fLongJumpDescendRate;
		float m_fJumpToSmashWindowPhaseChange;
		float m_fMinJumpPhaseAllowDescend;
		float m_fMaxJumpPhaseAllowDescend;
		float m_fMinJumpPhaseAllowSmashWindow;
		float m_fMaxJumpPhaseAllowSmashWindow;
		float m_fMinSmashWindowPhase;
		float m_fGlassBreakRadius;
		float m_fGlassDamage;
		float m_fMinDistanceToBreakWindow;
		float m_fMinStickValueAllowDescend;

		bool m_bAllowSmashDuringJump;

		PAR_PARSABLE;
	};

	CTaskRappel(float fDestinationHeight, const Vector3& vStartPos, const Vector3& vAnchorPos, int ropeID, fwMvClipSetId clipSetID, bool bSkipClimbOverWallState = false, u16 uOwnerJumpID = 0);
	virtual ~CTaskRappel();

	virtual void				CleanUp();

	// Task required implementations
	virtual aiTask*				Copy() const { return rage_new CTaskRappel(m_fDestinationHeight, m_vStartPos, m_vAnchorPos, m_iRopeID, m_OverrideBaseClipSetId, m_bSkipClimbOverWallState, m_uOwnerJumpID); }
	virtual int					GetTaskTypeInternal() const					{ return CTaskTypes::TASK_RAPPEL; }
	virtual s32					GetDefaultStateAfterAbort() const	{ return State_Finish; }

	virtual bool				ProcessPhysics(float fTimeStep, int nTimeSlice);
	virtual bool				ProcessPostMovement();

	bool						IsDescending() const { return m_bIsDescending; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

    // Clone task implementation
	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return GetState() >= State_ClimbOverWall; }
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void                OnCloneTaskNoLongerRunningOnOwner();
	virtual bool				IsInScope(const CPed* UNUSED_PARAM(pPed)) { return true; }

	// Local state function calls
	void						Start_OnEnter				();
	FSM_Return					Start_OnUpdate				();
	void						ClimbOverWall_OnEnter		();
	FSM_Return					ClimbOverWall_OnUpdate		();
    void                        WaitForNetworkRope_OnEnter  ();
    FSM_Return					WaitForNetworkRope_OnUpdate ();
	void						Idle_OnEnter				();
	FSM_Return					Idle_OnUpdate				();
    FSM_Return					Idle_OnUpdateClone			();
	void						Descend_OnEnter				();
	FSM_Return					Descend_OnUpdate			();
	void						Jump_OnEnter				();
	FSM_Return					Jump_OnUpdate				();
	void						SmashWindow_OnEnter			();
	FSM_Return					SmashWindow_OnUpdate		();
	FSM_Return					Finish_OnUpdate				();


	fwMvClipSetId				GetBaseClipSet() const;

	// Update our rope (pinning, unpinning, etc). Returns false if the rope doesn't exist
	bool						UpdateRope					();

	// Unpin all of the rope verts except the anchor point
	void						UnpinRopeVerts				();

	// This will attempt to find glass relative to the ped's position and break it
	void						FindAndBreakGlass			(bool bBreakGlass);

	// This will check if the ped is facing the correct direction, if climb over wall state is skipped
	bool						IsPedRappelHeadingCorrect	(CPed* pPed);

	// This will rotate the ped's heading 180
	void						ChangeRappelHeading			(CPed* pPed);
;
	// This is going to be our move network helper for the rappelling ped
	CMoveNetworkHelper			m_moveNetworkHelper;

	// Our multiple clip set request helper
	static const int NUM_CLIP_SETS = 2;
	CMultipleClipSetRequestHelper<NUM_CLIP_SETS> m_ClipSetRequestHelper;

	// this says that we decided to jump while already walking down
	bool						m_bJumpedFromDescend;

	// says if the jump is a long jump (has a higher descend rate)
	bool						m_bIsLongJumping;

	// this tells us we need to translate when in a jump
	bool						m_bIsDescending;

	// Have we already broken our glass?
	bool						m_bHasBrokenGlass;

	// This tells us if we should be pinning to the end attach position
	bool						m_bShouldPinToEndVertex;

	// This tells us if we should start updating the rope to match the position of the hand
	bool						m_bShouldUpdateRope;

	// Says if we're allowd to break the glass yet
	bool						m_bCanBreakGlass;

	// This is the ID of the rope we should be using
	int							m_iRopeID;

	// The next rope vertex along the rope as we descend
	int							m_iNextRopeVertex;

	// Should we be pinning a specific vert? If so this will be the vert #, otherwise -1
	int							m_iEndAttachVertex;

	// Our starting position, the position where the rope is anchored and destination height
    float                       m_fStartHeading;
	Vector3						m_vStartPos;
	Vector3						m_vAnchorPos;
	Vec3V						m_vEndAttachPos;
	Vec3V						m_vWindowPosition;
	float						m_fDestinationHeight;

	// Used to keep track of unique jumps, so clones know not to re-jump.
	u16							m_uOwnerJumpID;
	u16							m_uCloneJumpID;

	// static variables for checking events from the move network
	static const fwMvBooleanId  sm_ClimbWallClipFinishedId;
	static const fwMvBooleanId  sm_AttachRopeId;
	static const fwMvBooleanId  sm_JumpClipFinishedId;
	static const fwMvBooleanId  sm_SmashWindowClipFinishedId;
	static const fwMvBooleanId  sm_OnEnterIdleId;
	static const fwMvBooleanId  sm_OnEnterDescendId;
	static const fwMvBooleanId  sm_TurnOffExtractedZId;
	static const fwMvFloatId	sm_JumpPhaseId;
	static const fwMvFloatId	sm_SmashWindowPhaseId;
	static const fwMvClipSetId	sm_BaseClipSetId;
	static const fwMvClipSetId	sm_AdditionalClipSetId;

	fwMvClipSetId m_OverrideBaseClipSetId;

	bool m_bSetWallClimbToFinished;
	bool m_bSkipClimbOverWallState;
	bool m_bRotatedForClimbSkip;
    bool m_bAlreadyWaitingForTargetStateIdle;

	float m_fDesiredRotation;
	static const float sm_fDesiredHeadingForCasinoHeist;
	static const Vector3 sm_vCasinoHeistRappelPos;

	// our tunables
	static Tunables				sm_Tunables;
};

//-------------------------------------------------------------------------
// Task info for rappelling down a wall
//-------------------------------------------------------------------------
class CClonedRappelDownWallInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedRappelDownWallInfo(s32 iState, float fDestinationHeight, float fStartHeading, const Vector3& vStartPos, const Vector3& vAnchorPos, int ropeID, u32 overrideBaseClipSetIdHash, bool skipClimbOverWallState, u16 uOwnerJumpID);
	CClonedRappelDownWallInfo() {}
	~CClonedRappelDownWallInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_RAPPEL_DOWN_WALL;}
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_RAPPEL_DOWN_WALL; }

	virtual CTaskFSMClone *CreateCloneFSMTask();
    virtual CTask         *CreateLocalTask(fwEntity *pEntity);

	virtual bool  HasState() const { return true; }
	virtual u32	  GetSizeOfState() const { return datBitsNeeded<CTaskRappel::State_Finish>::COUNT;}
	virtual const char *GetStatusName(u32) const { return "State";}

    float GetDestinationHeight()  const { return m_fDestinationHeight; }
    float GetStartHeading()       const { return m_fStartHeading; }
    const Vector3 &GetStartPos()  const { return m_vStartPos; }
    const Vector3 &GetAnchorPos() const { return m_vAnchorPos; }
    int   GetNetworkRopeID()      const { return m_iNetworkRopeIndex; }
    u32   GetOverrideBaseClipSetIdHash() const { return m_OverrideBaseClipSetIdHash; }
	bool GetSkipClimbOverWallState() const { return m_bSkipClimbOverWall; }
	u16 GetOwnerJumpID()  const { return m_uOwnerJumpID; }

	void Serialise(CSyncDataBase& serialiser)
	{
        const float    MAX_HEIGHT                       = 4000.0f;
        const unsigned SIZEOF_DESTINATION_HEIGHT        = 20;
        const unsigned SIZEOF_HEADING                   = 8;
        const unsigned SIZEOF_ROPE_INDEX                = 8;
        const unsigned SIZEOF_OVERRIDE_BASE_CLIP_SET_ID = 32;
		const unsigned SIZEOF_JUMP_ID					= 16;

		CSerialisedFSMTaskInfo::Serialise(serialiser);

        SERIALISE_PACKED_FLOAT(serialiser, m_fDestinationHeight, MAX_HEIGHT, SIZEOF_DESTINATION_HEIGHT, "Destination Height");
        SERIALISE_PACKED_FLOAT(serialiser, m_fStartHeading, TWO_PI, SIZEOF_HEADING, "Current Heading");
        SERIALISE_POSITION(serialiser, m_vStartPos,  "Ped Start Position");
        SERIALISE_POSITION(serialiser, m_vAnchorPos, "Rope Anchor Position");
        SERIALISE_INTEGER(serialiser, m_iNetworkRopeIndex, SIZEOF_ROPE_INDEX, "Network Rope Index");

        bool bHasClipSetOverride = m_OverrideBaseClipSetIdHash != CLIP_SET_ID_INVALID.GetHash();
        SERIALISE_BOOL(serialiser, bHasClipSetOverride, "Has Clip Set Override");
        if (bHasClipSetOverride || serialiser.GetIsMaximumSizeSerialiser())
        {
            SERIALISE_UNSIGNED(serialiser, m_OverrideBaseClipSetIdHash, SIZEOF_OVERRIDE_BASE_CLIP_SET_ID, "Override Base Clip Set Id Hash");
        }
        else
        {
            m_OverrideBaseClipSetIdHash = CLIP_SET_ID_INVALID.GetHash();
        }
		SERIALISE_BOOL(serialiser, m_bSkipClimbOverWall, "Has climb over wall skip");
		SERIALISE_UNSIGNED(serialiser, m_uOwnerJumpID, SIZEOF_JUMP_ID, "Jump ID");

	}

private:

	CClonedRappelDownWallInfo(const CClonedHeliPassengerRappelInfo &);

	CClonedRappelDownWallInfo &operator=(const CClonedRappelDownWallInfo &);

    float   m_fDestinationHeight;           // destination height of ped rapelling (i.e. height they stop descending)
    float   m_fStartHeading;                // start heading of the ped
    Vector3 m_vStartPos;                    // start position of the ped
    Vector3 m_vAnchorPos;                   // position the rope is anchored
    int     m_iNetworkRopeIndex;            // network ID of the rope used to rappel
    u32     m_OverrideBaseClipSetIdHash;    // override clipSet used during rappel
	bool	m_bSkipClimbOverWall;			// skip climb over wall state
	u16		m_uOwnerJumpID;
};

#endif // INC_TASK_RAPPEL_H
