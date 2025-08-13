//
// Task/Default/TaskArrest.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_ARREST_H
#define INC_TASK_ARREST_H

// Game headers
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"

//////////////////////////////////////////////////////////////////////////
// CTaskArrest
//////////////////////////////////////////////////////////////////////////

#define ENABLE_TASKS_ARREST_CUFFED 0

#if ENABLE_TASKS_ARREST_CUFFED
# define ENABLE_TASKS_ARREST_CUFFED_ONLY(x) x
#else 
# define ENABLE_TASKS_ARREST_CUFFED_ONLY(x) 
#endif

#if ENABLE_TASKS_ARREST_CUFFED

class CTaskCuffed;
class CTaskInCustody;

enum ArrestFlags
{
	AF_UnCuff					= BIT0,				//Indicates arrest task is un-cuffing.
	AF_TakeCustody				= BIT1,				//Indicates arrest task is taking custody.
	AF_FallbackSelected			= BIT2,				//Indicates that the fallback anim was chosen.
	AF_FullSelected				= BIT3,				//Indicates that the full mechanic was chosen.
};

enum SyncSceneOption
{
	SSO_ArrestOnFloor			= 0,
	SSO_Uncuff,
	SSO_Max
};

struct tArrestSyncSceneClips
{
	strStreamingObjectName m_ArresterClipId;
	strStreamingObjectName m_ArresteeClipId;
	strStreamingObjectName m_CameraClipId;
};

class CTaskArrest : public CTaskFSMClone
{
public:
	CTaskArrest(CPed* pTargetPed, fwFlags8 iFlags = 0);
	
	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_ARREST; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskArrest(m_pTargetPed, m_iFlags); }

	// FSM states
	enum State
	{
		State_Start = 0,

		// Synced scene cuffing states
		State_SyncedScene_Prepare,
		State_SyncedScene_Playing,

		// Full Attest State
		State_Full_Prepare,
		State_Full_Phase1,
		State_Full_Phase2,
		State_Full_Phase3,

		// Fallback procedural cuffing states
		State_Fallback_MoveToTarget,
		State_Fallback_FaceTarget,
		State_Fallback_PreparingToShove,
		State_Fallback_Shoving,
		State_Fallback_Cuffing,

		// Taking custody states
		State_TakeCustody,

		// Finish handshake
		State_FinishedHandshake,

		State_Finish,
	};

	// PURPOSE: Returns if currently arresting using a synchronized scene
	bool IsPerformingSyncedScene() const;

	// PURPOSE: Returns if currently arresting using full arrest mechanic.
	bool IsPerformingFullArrest() const;

	// PURPOSE: Returns if currently uncuffing using full uncuff mechanic.
	bool IsPerformingFullUncuff() const;

	// PURPOSE: Returns if currently arresting using a synchronized scene
	bool IsPerformingSyncedSceneArrest() const;

	// PURPOSE: Returns if currently arresting using the procedural fallback method
	bool IsPerformingFallbackArrest() const;

	// PURPOSE: Returns if currently arresting the target
	bool IsArresting() const;

	// PURPOSE: Returns if currently uncuffing the target
	bool IsUnCuffing() const;

	// PURPOSE: Returns true if in an state that can be canceled by the user
	bool CanCancel() const;

	// PURPOSE: Returns if this is an un-cuff (as opposed to a cuff).
	bool IsUnCuffTask() const;

	// PURPOSE: Returns if currently uncuffing using a synchronized scene
	bool IsPerformingSyncedSceneUnCuff() const;

	// PURPOSE: Returns if currently taking custody the target
	bool IsTakingCustody() const { return GetState() == State_TakeCustody; }

	// PURPOSE: Returns the ped being cuffed or taken custody of
	CPed* GetTargetPed() { return m_pTargetPed; }

	// PURPOSE: Returns the id of the currently running synchronized scene
	const fwSyncedSceneId& GetSyncedSceneId() const { return m_SyncedSceneId; }

	// PURPOSE: Returns the anim clip selected for the target ped
	const strStreamingObjectName *GetArresteeClip() const;

	// PURPOSE: Returns a value in R[0, 1].
	float GetPhase() const;

	// PURPOSE: Returns a value in R[0, 1] for fallback arrest.
	float GetFallbackPhase() const;

	// PURPOSE: Returns a value in R[0, 1] for full arrest.
	float GetFullPhase() const;

	// PURPOSE: Did we choose sync scene arrest? Cloned peds shouldn't query state for this as it could have missed it
	// entirely.
	bool HasChosenSyncScene() const { return (m_iSyncSceneOption != SSO_Max); }

	// PURPOSE: Did we choose fallback arrest? Cloned peds shouldn't query state for this as it could have missed it
	// entirely.
	bool HasChosenFallback() const { return m_iFlags.IsFlagSet(AF_FallbackSelected); }

	// PURPOSE: Did we choose new arrest? Cloned peds shouldn't query state for this as it could have missed it
	// entirely.
	bool HasChosenFull() const { return m_iFlags.IsFlagSet(AF_FullSelected); }

	// PURPOSE: Get desired heading for full arrest.
	float GetDesiredFullArrestHeading() const;

	// PURPOSE: Get desired heading for cuffed ped get up anim.
	float GetDesiredCuffedPedGetupHeading() const;

	// PURPOSE: Get desired paired anim angle.
	float GetPairedAnimAngle() const;

	// PURPOSE: Can we move to finish state?
	bool CanMoveToFinishHandshake();

	static bool IsEnabled();

	// PURPOSE: Returns the anim dictionary for the synchronized scene
	static const strStreamingObjectName& GetArrestDict() { return ms_ArrestDict; }

	// PURPOSE: Returns the arrest/uncuff anim dictionaries.
	static const strStreamingObjectName& GetFullArrestDict() { return ms_FullArrestDict; }
	static const strStreamingObjectName& GetFullUncuffDict() { return ms_FullUncuffDict; }

protected:

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Manipulate ped position/orientation to get to correct arrest spot.
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

    // Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                OverridesNetworkBlender(CPed *pPed);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnEnter();
	FSM_Return Start_OnUpdate();
	FSM_Return SyncedScenePrepare_OnEnter();
	FSM_Return SyncedScenePrepare_OnUpdate();
	FSM_Return SyncedScenePlaying_OnEnter();
	FSM_Return SyncedScenePlaying_OnUpdate();
	FSM_Return SyncedScenePlaying_OnExit();
	FSM_Return FullPrepare_OnEnter();
	FSM_Return FullPrepare_OnUpdate();
	FSM_Return FullPhase1_OnEnter();
	FSM_Return FullPhase1_OnUpdate();
	FSM_Return FullPhase2_OnUpdate();
	FSM_Return FullPhase3_OnUpdate();
	FSM_Return FallbackMoveToTarget_OnEnter();
	FSM_Return FallbackMoveToTarget_OnUpdate();
	FSM_Return FallbackFaceTarget_OnEnter();
	FSM_Return FallbackFaceTarget_OnUpdate();
	FSM_Return FallbackPreparingToShove_OnEnter();
	FSM_Return FallbackPreparingToShove_OnUpdate();
	FSM_Return FallbackShoving_OnUpdate();
	FSM_Return FallbackCuffing_OnUpdate();
	FSM_Return FallbackCuffing_OnExit();
	FSM_Return TakeCustody_OnEnter();
	FSM_Return TakeCustody_OnUpdate();
	FSM_Return TakeCustody_OnExit();
	FSM_Return FinishedHandShake_OnEnter();
	FSM_Return FinishedHandShake_OnUpdate();
	FSM_Return Finished_OnEnter();

	////////////////////////////////////////////////////////////////////////////////

private:

	////////////////////////////////////////////////////////////////////////////////

	bool IsTargetPedRunningCuffedTask() const;
	bool IsTargetPedRunningInCustodyTask() const;
	s32 GetTargetPedCuffedState() const;
	CTaskCuffed *GetTargetPedCuffedTask() const;
	CTaskInCustody *GetTargetPedInCustodyTask() const;

	void SelectUnCuffState();
	void SelectArrestState();
	void CreateSyncedSceneCamera();
	void DestroySyncedSceneCamera();
	void ReleaseSyncedScene();

	void CalculateCuffedPedGetupHeading(float fArrestPedHeading, float fCuffedPedHeading);
	Vector3 CalculateFullArrestTargetPosition();
	void GetFullArrestDisplacementAndOrientation(Vector3 &vDisplacementOut, Quaternion &qOrientationOut, bool bCalcRemaining, float fStartPhaseOverride = 0.0f) const;

	Vector3 GetTargetPedPosition() const;

	void	UpdateMoVE();

	//
	// Members
	//

	// PURPOSE: The ped being cuffed/uncuffed/taken custody
	RegdPed m_pTargetPed;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	// PURPOSE: Helpers for streaming in the anim dictionaries.
	CClipRequestHelper m_ClipRequestHelper;
	CClipRequestHelper m_ClipRequestHelperFull;

	// PURPOSE: Helper to calculate a new attachment position
	CPlayAttachedClipHelper m_PlayAttachedClipHelper;

	Vector3 m_vFullArrestTargetDirection;
	Vector3 m_vFullArrestTargetPos;
	Quaternion m_qFullArrestTargetOrientation;

	// PURPOSE: The scene id when running a synced scene arrest
	fwSyncedSceneId m_SyncedSceneId;

	// PURPOSE: The ped/target heading for full arrest.
	float m_fFullArrestPedHeading;

	// PURPOSE: The ped/target heading for cuffed ped.
	float m_fDesiredCuffedPedGetupHeading;

	// PURPOSE: Paired anim angle that we have chosen.
	float m_fPairedAnimAngle;

	// PURPOSE: Phase of anim that we started attaching to target ped.
	float m_fStartAttachmentPhase;

	// PURPOSE: Start time of full arrest sequence.
	u32 m_uFullStartTimeMs;

	// PURPOSE: Blocking bound ID.
	TDynamicObjectIndex m_BlockingBoundID;

	// PURPOSE: Arrest Flags.
	fwFlags8 m_iFlags;

	// PURPOSE: ID of sync scene option.
	SyncSceneOption m_iSyncSceneOption;

	// PURPOSE: Have we started attaching yet?
	bool m_bStartedTargetAttachment : 1;

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvFloatId ms_HeadingDelta;
	static const fwMvFloatId ms_Distance;
	static const fwMvFloatId ms_PairedAnimAngle;
	static const fwMvFlagId	ms_MoveRight;
	static const fwMvFlagId ms_UseFallback;
	static const fwMvFlagId ms_UseUncuff;
	static const fwMvFlagId ms_UseNewAnims;
	static const fwMvBooleanId ms_Phase1Complete;
	static const fwMvBooleanId ms_Phase2Complete;
	static const fwMvBooleanId	ms_Ended;

	// PURPOSE: Tags triggered by shove animation
	static const fwMvPropertyId ms_Shove;
	static const fwMvPropertyId ms_Handcuffed;
	static const fwMvPropertyId ms_Interruptible;
	static const fwMvFloatId ms_ShortPhase;
	static const fwMvFloatId ms_LongPhase;
	static const fwMvFloatId ms_DistanceWeightId;
	static const fwMvFloatId ms_DistanceDirectionWeightId;
	static const fwMvFloatId ms_AngleWeightId;
	static const fwMvFloatId ms_ShortDirectionPhase;
	static const fwMvFloatId ms_LongDirectionPhase;
	static const fwMvClipId ms_ShortClipId;
	static const fwMvClipId ms_LongClipId;
	static const fwMvClipId ms_ShortDirectionClipId;
	static const fwMvClipId ms_LongDirectionClipId;

	// PURPOSE: Arrest Dictionaries.
	static const strStreamingObjectName ms_ArrestDict;
	static const strStreamingObjectName ms_FullArrestDict;
	static const strStreamingObjectName ms_FullUncuffDict;

	// PURPOSE: Array of sync scene options that we can play.
	static tArrestSyncSceneClips ms_SyncSceneOptions[SSO_Max]; 

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
public:

	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

#if __BANK
	Vector3 m_vFullArrestDebugPosition;
#endif

	////////////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// CClonedTaskArrestInfo
//////////////////////////////////////////////////////////////////////////

class CClonedTaskArrestInfo : public CSerialisedFSMTaskInfo
{
public:

	static const int SIZEOF_FLAGS = 8;
	static const int SIZEOF_SYNC_SCENE_OPTION = 8;
	static const int SIZEOF_HEADING = 16;

	CClonedTaskArrestInfo(s32 iState, CPed* pTargetPed, u8 iSyncSceneOption, u8 iFlags);	
	CClonedTaskArrestInfo();
	~CClonedTaskArrestInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_ARREST;}	
	virtual CTaskFSMClone *CreateCloneFSMTask() { return rage_new CTaskArrest(GetTargetPed(), m_iFlags); }

	virtual bool HasState() const { return true; }
	virtual u32 GetSizeOfState() const { return datBitsNeeded< CTaskArrest::State_Finish >::COUNT; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_OBJECTID(serialiser, m_TargetPedId, "Target Ped");
		SERIALISE_UNSIGNED(serialiser, m_iSyncSceneOption, SIZEOF_SYNC_SCENE_OPTION, "Sync Scene Option");
		SERIALISE_UNSIGNED(serialiser, m_iFlags, SIZEOF_FLAGS, "Flags");
	}

	CPed* GetTargetPed();
	u8 GetFlags() const { return m_iFlags; }
	u8 GetSyncSceneOption() const { return m_iSyncSceneOption; }

private:
	CClonedTaskArrestInfo(const CClonedTaskArrestInfo &);
	CClonedTaskArrestInfo &operator=(const CClonedTaskArrestInfo &);

	ObjectId m_TargetPedId;
	u8 m_iFlags;
	u8 m_iSyncSceneOption;
};

#endif // ENABLE_TASKS_ARREST_CUFFED

#endif // INC_TASK_ARREST_H
