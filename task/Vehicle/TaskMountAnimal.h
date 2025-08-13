//-------------------------------------------------------------------------
// TaskMountAnimal.h
// Contains the Get On Horse Task
//
// Created on: 14-04-11
//
// Author: Bryan Musson
//-------------------------------------------------------------------------


#ifndef TASK_MOUNT_ANIMAL_H
#define TASK_MOUNT_ANIMAL_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "modelinfo/ModelSeatInfo.h"
#include "Network/General/NetworkUtil.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"

class CPed;
class CComponentReservation;
class CEntryExitPoint;
struct tGameplayCameraSettings;

//-------------------------------------------------------------------------
// Base class information for mount clone tasks
//-------------------------------------------------------------------------
class CClonedMountFSMInfoBase
{

public: //constructors, destructor

	CClonedMountFSMInfoBase()
	: m_pMount(NULL)
	, m_iTargetEntryPoint(0)
	, m_iSeatRequestType(SR_Any)
	, m_iSeat(-1)
	{
        m_iRunningFlags.BitSet().Reset();
	}

	CClonedMountFSMInfoBase(CPed* pMount, const	SeatRequestType iSeatRequestType, const	s32	iSeat, const s32 iTargetEntryPoint, const VehicleEnterExitFlags iRunningFlags)
	: m_pMount(pMount)
	, m_iTargetEntryPoint(iTargetEntryPoint)
	, m_iRunningFlags(iRunningFlags)
	, m_iSeatRequestType(iSeatRequestType)
	, m_iSeat(iSeat)
	{
	}

	virtual ~CClonedMountFSMInfoBase()
	{
	}

public: //getters

	CPed*			      GetMount()				const	{ return m_pMount.GetPed();		}
	VehicleEnterExitFlags GetRunningFlags()		const	{ return m_iRunningFlags;		}
	s32		              GetSeat()				const	{ return m_iSeat;				}
	SeatRequestType	      GetSeatRequestType()    const	{ return m_iSeatRequestType;	}
	s32		              GetTargetEntryPoint()	const	{ return m_iTargetEntryPoint;	}

protected: //variables

	CSyncedPed		      m_pMount;				// target mount
	s32			          m_iTargetEntryPoint;	// Which seat is the current target
	VehicleEnterExitFlags m_iRunningFlags;		// Flags that change the behavior of the task
	SeatRequestType	      m_iSeatRequestType;		// The seat request type (e.g. any, specific, prefer)
	s32			          m_iSeat;				// Which seat has been requested

};

//-------------------------------------------------------------------------
// Derived class information for mount clone tasks
//-------------------------------------------------------------------------
class CClonedMountFSMInfo : public CSerialisedFSMTaskInfo, public CClonedMountFSMInfoBase
{

public: //constructors, destructor

	CClonedMountFSMInfo()
	{
	}

	CClonedMountFSMInfo(const s32 enterState, CPed*	pMount, const SeatRequestType iSeatRequestType, const s32 iSeat, const s32 iTargetEntryPoint, const	VehicleEnterExitFlags iRunningFlags)
	: CClonedMountFSMInfoBase(pMount, iSeatRequestType, iSeat, iTargetEntryPoint, iRunningFlags)
	{
		CSerialisedFSMTaskInfo::SetStatusFromMainTaskState(enterState);
	}

	virtual ~CClonedMountFSMInfo()
	{
	}

protected: //base class overrides

	void Serialise(CSyncDataBase& serialiser)
	{
		//Call the base class version.
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		//Serialize the mount ID.
		ObjectId mountID = m_pMount.GetPedID();
		serialiser.SerialiseObjectID(mountID, "Mount ID");
		m_pMount.SetPedID(mountID);

		//Serialize the target entry point.
		serialiser.SerialiseInteger(m_iTargetEntryPoint, SIZEOF_ENTRY_POINT, "Target Entry Point");

		//Serialize the running flags.
		SerialiseVehicleEnterExitFlags(serialiser, m_iRunningFlags,	"Running Flags");

		//Serialize the seat request type.
		serialiser.SerialiseInteger(reinterpret_cast<s32&>(m_iSeatRequestType), SIZEOF_SEAT_REQUEST_TYPE, "Seat Request Type");

		//Serialize the seat.
		serialiser.SerialiseInteger(m_iSeat, SIZEOF_SEAT, "Seat");
	}

private: //static variables

	static const unsigned int SIZEOF_ENTRY_POINT       = 5;
	static const unsigned int SIZEOF_RUNNING_FLAGS     = 32;
	static const unsigned int SIZEOF_SEAT_REQUEST_TYPE = 5;
	static const unsigned int SIZEOF_SEAT              = 5;

};

//-------------------------------------------------------------------------
// Base class for all mount tasks
//-------------------------------------------------------------------------
class CTaskMountFSM : public CTaskFSMClone
{

public: //constructors, destructor

	CTaskMountFSM(CPed*	pMount, const SeatRequestType iSeatRequestType, const s32 iSeat, const VehicleEnterExitFlags& iRunningFlags, const	s32	iTargetEntryPoint);

	virtual ~CTaskMountFSM();

public: //getters

	CPed*							GetMount()				const	{ return m_pMount;				}
	VehicleEnterExitFlags			GetRunningFlags()		const	{ return m_iRunningFlags;		}
	s32		        				GetSeat()				const	{ return m_iSeat;				}
	SeatRequestType					GetSeatRequestType()    const	{ return m_iSeatRequestType;	}
	s32		        				GetTargetEntryPoint()	const	{ return m_iTargetEntryPoint;	}

public: //inline interface

	bool IsFlagSet(const s32 iFlag) const { return m_iRunningFlags.BitSet().IsSet(iFlag); }

	void SetRunningFlag(const s32 iFlag) { m_iRunningFlags.BitSet().Set(iFlag); }

protected: //base class overrides

	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);

protected: //variables

	RegdPed			m_pMount;
	s32			    m_iTargetEntryPoint;
	s32			    m_iSeat;
	SeatRequestType	m_iSeatRequestType;
	VehicleEnterExitFlags			    m_iRunningFlags;

};

//-------------------------------------------------------------------------
// Task to control the characters clips whilst getting on a horse
//-------------------------------------------------------------------------
class CTaskMountAnimal : public CTaskMountFSM
{
	friend class CTaskGettingMounted;
public:
	static bank_float ms_fDistanceToEvaluateSeats;

	// FSM states
	enum eMountAnimalState
	{
		State_Start, 	
		State_ExitMount,
		State_GoToMount,
		State_PickSeat,
		State_GoToSeat,
 		State_ReserveSeat,
 		State_Align,
 		State_WaitForGroupLeaderToEnter,
 		State_JackPed,
 		State_EnterSeat,
 		State_UnReserveSeat,
 		State_SetPedInSeat,
        State_WaitForNetworkSync,
		State_Finish
	};

public:

	CTaskMountAnimal(CPed* pAnimal, SeatRequestType iRequest, s32 iTargetSeatIndex, VehicleEnterExitFlags iRunningFlags = VehicleEnterExitFlags(), float fTimer = 0.0f, const float fMoveBlendRatio = MOVEBLENDRATIO_RUN);
	virtual ~CTaskMountAnimal();

	// Task required implementations
	virtual aiTask*				Copy() const						{ return rage_new CTaskMountAnimal(m_pMount, m_iSeatRequestType, m_iSeat, m_iRunningFlags, m_fTimer, m_fMoveBlendRatio); }
	virtual int					GetTaskTypeInternal() const					{ return CTaskTypes::TASK_MOUNT_ANIMAL; }
	virtual s32					GetDefaultStateAfterAbort() const;

	// Interface functions
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const  { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_moveNetworkHelper; }
	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	// Clone task implementation
	// When cloned, this task continuously syncs to the remote state
	virtual bool                OverridesCollision()		{ return true; }
	virtual bool                ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType)) { return GetState() < State_Align; }
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void                OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	// This function returns whether changes from the specified state
	// are managed by the clone FSM update, if not the state must be
	// kept in sync with the network update
	bool						StateChangeHandledByCloneFSM(s32 iState);	

	void CheckForNextStateAfterCloneAlign();

	// Pedmove state for each movement
	void	SetPedMoveBlendRatio(float fMoveBlendRatio) {m_fMoveBlendRatio = fMoveBlendRatio;}
	float	GetPedMoveBlendRatio() const {return m_fMoveBlendRatio;}

	// DAN TEMP - function for ensuring a valid clip transition is available based on the current move network state
	//            this will be replaced when the move network interface is extended to include this
	bool                IsClipTransitionAvailable(s32 newState);

	void UpdateMountAttachmentOffsets();

	static bool StartMoveNetwork(CPed* ped, const CPed* pMount, CMoveNetworkHelper& moveNetworkHelper, s32 iEntryPointIndex, float fBlendInDuration);

	static bool	RequestAnimations( const CPed& ped, const CPed& mount, s32 iEntryPoint, s32 iSeat );

private:

	bool CanJackPed( const CPed* pPed, CPed* pOtherPed, VehicleEnterExitFlags iFlags );

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	void DoJackPed();

	virtual FSM_Return	ProcessPostFSM();	

	// FSM optional functions
	virtual bool		ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);
	virtual void		DoAbort(const AbortPriority priority, const aiEvent* pEvent);
	virtual	void		CleanUp			();
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// Local state function calls
	FSM_Return			Start_OnUpdate();
    FSM_Return			Start_OnUpdateClone();
	void				ExitMount_OnEnter();
	FSM_Return			ExitMount_OnUpdate();
 	void				GoToMount_OnEnter();
 	FSM_Return			GoToMount_OnUpdate();
 	FSM_Return			PickSeat_OnUpdate();
 	void				GoToSeat_OnEnter();
 	FSM_Return			GoToSeat_OnUpdate();
 	FSM_Return			ReserveSeat_OnUpdate();
 	void				Align_OnEnter();
    void				Align_OnEnterClone();
 	FSM_Return			Align_OnUpdate();
    FSM_Return			Align_OnUpdateClone();
 	FSM_Return			WaitForGroupLeaderToEnter_OnUpdate();
 	void				JackPed_OnEnter();
 	FSM_Return			JackPed_OnUpdate();
 	void				EnterSeat_OnEnter();
 	FSM_Return			EnterSeat_OnUpdate();
 	FSM_Return			UnReserveSeat_OnUpdate();
 	void				SetPedInSeat_OnEnter();
 	FSM_Return			SetPedInSeat_OnUpdate();
    FSM_Return			WaitForNetworkSync_OnUpdate();

	// Internal helper functions	
	void				PedEnteredMount();
	bool				PedShouldWaitForLeaderToEnter();		
	bool				CheckPlayerExitConditions(CPed& ped);
	const crClip*		GetClipAndPhaseForState(float& fPhase);
	void				PossiblyBroadcastMountStolenEvent();
	void				StoreInitialOffsets();		
	float				ComputeMountZone();
	
private: //utilities

	void StreamClipSets();
	void WarpPedToEntryPositionAndOrientate();

protected:
	void				StartMountAnimation();


	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper	m_moveNetworkHelper;

	CPlayAttachedClipHelper m_PlayAttachedClipHelper;
	CTaskTimer			m_TaskTimer;

	static const fwMvFlagId ms_IsDeadId;
	static const fwMvBooleanId ms_JackClipFinishedId;
	static const fwMvClipId ms_JackPedClipId;
	static const fwMvFloatId ms_JackPedPhaseId;

	s32					m_iCurrentSeatIndex;
	s32                 m_iNetworkWaitState; // state to wait for GetStateFromNetwork() to change from when waiting for a network update
	// DAN TEMP - variable for ensuring a valid clip transition is available based on the current move network state
	s32                 m_iLastCompletedState;
	RegdPed				m_pJackedPed;

	float				m_fTimer;	
	float				m_fSeatCheckInterval;
	float				m_fMoveBlendRatio;
	float				m_fClipBlend;

	bool				m_bWarping:1;
	bool				m_bWaitForSeatToBeEmpty:1;
	bool				m_bWaitForSeatToBeOccupied:1;
	bool				m_bTimerRanOut:1;
	bool				m_ClipsReady:1;
	bool				m_bQuickMount:1;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void				Debug() const;
#endif
};

//////////////////////////////////////////////////////////////////////////
// CClonedMountAnimalInfo
//////////////////////////////////////////////////////////////////////////
class CClonedMountAnimalInfo : public CClonedMountFSMInfo
{
public:
	CClonedMountAnimalInfo();
	CClonedMountAnimalInfo(s32 enterState, CPed* pMount, SeatRequestType iSeatRequest, s32 iSeat, s32 iTargetEntryPoint, VehicleEnterExitFlags iRunningFlags, CPed* pJackedPed );
	virtual CTaskFSMClone* CreateCloneFSMTask();
	virtual s32		GetTaskInfoType( ) const { return CTaskInfo::INFO_TYPE_MOUNT_ANIMAL_CLONED; }

	virtual bool    HasState() const		{ return true; }
	virtual u32		GetSizeOfState() const	{ return datBitsNeeded<CTaskMountAnimal::State_Finish>::COUNT;}
	static const char*	GetStateName(s32) { return "FSM State";}
	
	CPed* GetJackedPed() const { return m_pJackedPed.GetPed(); }

	void Serialise(CSyncDataBase& serialiser)
	{
		CClonedMountFSMInfo::Serialise(serialiser);
		
		//Serialize the jacked ped ID.
		ObjectId jackedPedID = m_pJackedPed.GetPedID();
		serialiser.SerialiseObjectID(jackedPedID, "Jacked Ped ID");
		m_pJackedPed.SetPedID(jackedPedID);
	}
	
private: //variables

	CSyncedPed m_pJackedPed;
	
};


//////////////////////////////////////////////////////////////////////////
// CTaskDismountAnimal
//////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------
// Task to control the characters clips whilst dismounting 
//-------------------------------------------------------------------------
class CTaskDismountAnimal : public CTaskMountFSM
{
	friend class CTaskGettingDismounted;
public:

	// FSM states
	enum eDismountAnimalState
	{
		State_Start,		
		State_ReserveSeat,		
		State_ExitSeat,		
		State_UnReserveSeatToFinish,				
		State_Finish
	};

public:

	CTaskDismountAnimal(VehicleEnterExitFlags iRunningFlags = VehicleEnterExitFlags(), float fDelayTime = 0.0f, CPed* pPedJacker = NULL, s32 iTargetEntryPoint = -1, bool bUseHitchVariant=false);
	virtual ~CTaskDismountAnimal();

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskDismountAnimal(m_iRunningFlags, m_fDelayTime, m_pPedJacker, m_iTargetEntryPoint, m_bUseHitchVariant); }
	virtual int			GetTaskTypeInternal() const			{ return CTaskTypes::TASK_DISMOUNT_ANIMAL; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	// Interface functions
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_moveNetworkHelper; }
	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	// Clone task implementation
	// When cloned, this task continuously syncs to the remote state
	virtual bool        OverridesCollision()		{ return true; }	
	virtual CTaskInfo*	CreateQueriableState() const;
	virtual void		ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void        OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return	UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual bool		HandlesDeadPed() { return true; }
	// This function returns whether changes from the specified state
	// are managed by the clone FSM update, if not the state must be
	// kept in sync with the network update
	bool				StateChangeHandledByCloneFSM(s32 iState);

	void				UseHitchVariant(){m_bUseHitchVariant = true;}

private:	

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return	ProcessPostFSM();

	// FSM optional functions
	virtual	void		CleanUp			();	

	// Local state function calls
	void				Start_OnEnter();
	void				Start_OnEnterClone();
	FSM_Return			Start_OnUpdate();
	FSM_Return			Start_OnUpdateClone();
	FSM_Return			ReserveSeat_OnUpdate();
	void				ExitSeat_OnEnter();
	FSM_Return			ExitSeat_OnUpdate();	
	FSM_Return			UnReserveSeatToFinish_OnUpdate();		
	
private: //utilities

	void StreamClipSets();
	static const fwMvFlagId ms_RunningDismountFlagId;
	static const fwMvFlagId ms_JumpingDismountFlagId;
	static const fwMvFlagId ms_UseHitchVariantFlagId;
	static s32 ms_CameraInterpolationDuration;

protected:
	void				DetachReins();
	void				StartDismountAnimation();
	bool				m_bTimerRanOut;

	// Setup collision while dismounting
	void				SetUpRagdollOnCollision(CPed& rPed, CPed& rMount);
	void				ResetRagdollOnCollision(CPed& rPed);

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper		m_moveNetworkHelper;
	float					m_fDelayTime;
	RegdPed					m_pPedJacker;

	fwClipSetRequestHelper	m_EntryClipSetRequestHelper;
	bool					m_ClipsReady;

	// DAN TEMP - variable for ensuring a valid clip transition is available based on the current move network state
	s32                     m_iLastCompletedState;	

	bool					m_bUseHitchVariant;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void				Debug() const;
#endif
};

//////////////////////////////////////////////////////////////////////////
// CClonedDismountAnimalInfo
//////////////////////////////////////////////////////////////////////////
class CClonedDismountAnimalInfo: public CClonedMountFSMInfo
{
public:
	CClonedDismountAnimalInfo();
	CClonedDismountAnimalInfo(s32 enterState, CPed* pMount, SeatRequestType iSeatRequestType, s32 iSeat, s32 iTargetEntryPoint, VehicleEnterExitFlags iRunningFlags, CPed* pForcedOutPed);
	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTask *CreateLocalTask(fwEntity* pEntity);
	virtual s32 GetTaskInfoType( ) const {return CTaskInfo::INFO_TYPE_DISMOUNT_ANIMAL_CLONED;}

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskDismountAnimal::State_Finish>::COUNT;}
	static const char*	GetStateName(s32)  { return "FSM State";}
	
	CPed* GetPedJacker() const { return m_pPedJacker.GetPed(); }

	void Serialise(CSyncDataBase& serialiser)
	{
		CClonedMountFSMInfo::Serialise(serialiser);
		
		//Serialize the ped jacker ID.
		ObjectId pedJackerID = m_pPedJacker.GetPedID();
		serialiser.SerialiseObjectID(pedJackerID, "Ped Jacker ID");
		m_pPedJacker.SetPedID(pedJackerID);
	}
	
private: //variables

	CSyncedPed m_pPedJacker;
	
};

//-------------------------------------------------------------------------
// Task to control the characters clips whilst mounting
//-------------------------------------------------------------------------
class CTaskMountSeat : public CTaskFSMClone
{	
public:

	// FSM states
	enum eMountSeatState
	{
		State_Start,
		State_EnterSeat,
		State_Finish
	};

public:

	CTaskMountSeat(const VehicleEnterExitFlags& iRunningFlags, CPed* pMount, const s32 iTargetSeat, bool bQuickMount = false, float fMountZone = 0.0f);
	virtual ~CTaskMountSeat();

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskMountSeat(m_iRunningFlags, m_pMount, m_iSeat, m_bQuickMount, m_fMountZone); }
	virtual int			GetTaskTypeInternal() const			{ return CTaskTypes::TASK_MOUNT_SEAT; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	// Clone task implementation
	// When cloned, this task continuously syncs to the remote state
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) {}
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

	const crClip*		GetClipAndPhaseForState(float& fPhase);

	void SetMountReady()	{m_bMountReady = true;}	
	bool GetMountReady()	{return m_bMountReady;}
private:

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual bool		ProcessPostPreRender();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// FSM optional functions
	virtual	void		CleanUp			();

	// Local state function calls
	FSM_Return			Start_OnUpdate();
	void				EnterSeat_OnEnter();
	FSM_Return			EnterSeat_OnUpdate();

	void				FinalizeMount(CPed* pPed);

private:

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper*	m_pParentNetworkHelper;
	RegdPed				m_pMount;	
	bool				m_bMountReady;	
	bool				m_bQuickMount;
	bool				m_bFinalizeMount;

	s32					m_iSeat;

	float m_fRStirrupPhase;
	float m_fLStirrupPhase;
	float m_fMountZone;

	VehicleEnterExitFlags	m_iRunningFlags;

	static const fwMvBooleanId ms_EnterSeat_OnEnterId;
	static const fwMvBooleanId ms_EnterSeatClipFinishedId;
	static const fwMvFlagId ms_HasMountQuickAnimId;
	static const fwMvFlagId ms_UseFarMountId;
	static const fwMvRequestId ms_EnterSeatId;
	static const fwMvClipId ms_EnterSeatClipId;
	static const fwMvFlagId ms_QuickMountId;
	static const fwMvFlagId ms_AirMountId;
	static const fwMvFloatId ms_MountDirectionId;
	static const fwMvFloatId ms_EnterSeatPhaseId;
	static const fwMvFloatId ms_MountZoneId;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void				Debug() const;
#endif
};

//-------------------------------------------------------------------------
// Task to control the mount's clips whilst mounting
//-------------------------------------------------------------------------
class CTaskGettingMounted : public CTaskFSMClone
{	
public:
	
	// FSM states
	enum eMountSeatState
	{
		State_Start,
		State_EnterSeat,
		State_Finish
	};

public:

	CTaskGettingMounted(CTaskMountAnimal* pRiderTask, s32 iEntryPoint);
	virtual ~CTaskGettingMounted() {}

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskGettingMounted(m_pRiderTaskMountAnimal, m_iTargetEntryPoint); }
	virtual int			GetTaskTypeInternal() const					{ return CTaskTypes::TASK_GETTING_MOUNTED; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	// Clone task implementation
	// When cloned, this task continuously syncs to the remote state
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) {}
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

private:

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);


	// Local state function calls
	FSM_Return			Start_OnUpdate();
	FSM_Return			EnterSeat_OnUpdate();

	virtual	void		CleanUp();

private:	
	CTaskMountAnimal*	m_pRiderTaskMountAnimal;
	s32			        m_iTargetEntryPoint;	// Which seat is the current target

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void				Debug() const;
#endif
};

//-------------------------------------------------------------------------
// Task to control the mount's clips whilst dismounting
//-------------------------------------------------------------------------
class CTaskGettingDismounted : public CTaskFSMClone
{	
public:

	// FSM states
	enum eMountSeatState
	{
		State_Start,
		State_Dismount,
		State_RunningDismount,
		State_SlopedDismount,
		State_Finish
	};

public:

	CTaskGettingDismounted(s32 iEntryPoint, const s32 iSeat, bool bIsRunningDismount, bool bIsJacked);
	virtual ~CTaskGettingDismounted() {}

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskGettingDismounted(m_iTargetEntryPoint, m_iSeat, m_bIsRunningDismount, m_bIsJacked); }
	virtual int			GetTaskTypeInternal() const			{ return CTaskTypes::TASK_GETTING_DISMOUNTED; }
		
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	// Clone task implementation
	// When cloned, this task continuously syncs to the remote state
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) {}
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

	const crClip*		GetClipAndPhaseForState(float& fPhase);

private:

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);


	// Local state function calls
	FSM_Return			Start_OnUpdate();
	FSM_Return			Dismount_OnUpdate();
	FSM_Return			RunningDismount_OnUpdate();
	FSM_Return			SlopedDismount_OnUpdate();
	void				CleanUp();

private:	
	s32			        m_iTargetEntryPoint;	// Which seat is the current target
	s32					m_iSeat;
	bool				m_bIsRunningDismount;
	bool				m_bIsJacked;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void				Debug() const;
#endif
};

//-------------------------------------------------------------------------
// Task to control the characters clips whilst dismounting
//-------------------------------------------------------------------------
class CTaskDismountSeat : public CTaskFSMClone
{
public:

	// FSM states
	enum eDismountSeatState
	{
		State_Start,
		State_Dismount,
		State_Finish
	};

public:

	CTaskDismountSeat(CPed* pMount, s32 iExitPointIndex, bool bAnimateToExitPoint);
	virtual ~CTaskDismountSeat();

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskDismountSeat(m_pMount, m_iExitPointIndex, m_bAnimateToExitPoint); }
	virtual int			GetTaskTypeInternal() const			{ return CTaskTypes::TASK_DISMOUNT_SEAT; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	// Clone task implementation
	// When cloned, this task continuously syncs to the remote state
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) {}
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);	

	void SetDismountReady()	{m_bDismountReady = true;}
	bool GetDismountReady()	{return m_bDismountReady;}

	static void UpdateDismountAttachment(CPed* pPed, CEntity* pMount, const crClip* pClip, float fClipPhase, Vector3& vNewPedPosition, Quaternion& qNewPedOrientation, s32 iEntryPointIndex, CPlayAttachedClipHelper& playAttachedClipHelper, bool applyFixup = true );
private:

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

	// FSM optional functions
	virtual	void		CleanUp			();

	// Local state function calls
	FSM_Return			Start_OnUpdate();
	void				Dismount_OnEnter();
	FSM_Return			Dismount_OnUpdate();

	float GetDismountInterruptPhase() const;

	static void				UpdateTarget( CEntity* pMount, CPed* pPed, const crClip* pClip, s32 iEntryPointIndex, CPlayAttachedClipHelper& playAttachedClipHelper );

	CPlayAttachedClipHelper	m_PlayAttachedClipHelper;
	s32						m_iExitPointIndex;
	bool					m_bAnimateToExitPoint;

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper*	m_pParentNetworkHelper;
	RegdPed				m_pMount;	
	bool				m_bDismountReady;

	float m_fRStirrupPhase;
	float m_fLStirrupPhase;

	static const fwMvBooleanId ms_DismountClipFinishedId;	
	static const fwMvBooleanId ms_RunDismountClipFinishedId;	
	static const fwMvClipId ms_DismountClipId;	
	static const fwMvFloatId ms_DismountPhaseId;	

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void				Debug() const;
#endif
};

#endif // ENABLE_HORSE

#endif //TASK_MOUNT_ANIMAL_H
