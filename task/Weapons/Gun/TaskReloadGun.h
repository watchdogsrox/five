//
// Task/Weapons/Gun/TaskReloadGun.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_RELOAD_GUN_H
#define INC_TASK_RELOAD_GUN_H

// Game headers
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"
#include "Task/Weapons/WeaponController.h"

// Forward declarations
class CWeaponComponentReloadData;
class CWeaponComponentReloadLoopedData;
class CWeaponInfo;

//////////////////////////////////////////////////////////////////////////
// eReloadFlags
//////////////////////////////////////////////////////////////////////////

enum eReloadFlags
{
	// Config flags
	RELOAD_IDLE				= BIT(1),
	RELOAD_SECONDARY		= BIT(2),
	RELOAD_COVER			= BIT(3),

	// this must come after all the config flags
	RELOAD_CONFIG_FLAGS_END	= BIT(4)

	// Runtime flags

};

//////////////////////////////////////////////////////////////////////////
// CTaskReloadGun
//////////////////////////////////////////////////////////////////////////

class CTaskReloadGun : public CTaskFSMClone
{

public:

	static bank_float ms_fReloadNetworkBlendInDuration;
	static bank_float ms_fReloadNetworkBlendOutDuration;

	CTaskReloadGun(const CWeaponController::WeaponControllerType weaponControllerType, s32 iFlags = 0);
	
	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_RELOAD_GUN; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskReloadGun(m_weaponControllerType, m_iFlags); }

	bool IsReloadFlagSet(eReloadFlags flag) const { return (m_iFlags & flag)!=0; }
	void RequestInterrupt() {m_bInterruptRequested = true;}
	void RequestAnimSwap() {m_bAnimSwapRequested = true;}
	bool GetCreatedAmmoClip() const { return m_bCreatedAmmoClip; }
	bool GetCreatedAmmoClipInLeftHand() const { return m_bCreatedAmmoClipInLeftHand; }

	const CObject* GetHeldObject() const { return m_pHeldObject; }
	CObject* GetHeldObject() { return m_pHeldObject; }

	u8 GetOriginalAttachPoint() const { return m_nInitialGunAttachPoint; }
	void SwitchGunHands( u8 nAttachPoint );

protected:

	// FSM states
	enum State
	{
		State_Start = 0,
		State_Reload,
		State_ReloadIn,
		State_ReloadLoop,
		State_ReloadOut,
		State_Finish,
	};

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: This gets called after the skeleton update, but before any Ik
	virtual void	ProcessPreRender2();

	// PURPOSE: Called after the camera system update
	virtual bool ProcessPostCamera();

	// PURPOSE:	Communicate with Move
	virtual bool ProcessMoveSignals();

    // Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();

public:

#if DEBUG_DRAW
	void DebugCloneTaskInformation(void);
#endif /* DEBUG_DRAW */

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return Reload_OnEnter();
	FSM_Return Reload_OnUpdate();
	FSM_Return ReloadIn_OnEnter();
	FSM_Return ReloadIn_OnUpdate();
	FSM_Return ReloadLoop_OnEnter();
	FSM_Return ReloadLoop_OnUpdate();
	FSM_Return ReloadOut_OnEnter();
	FSM_Return ReloadOut_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Start the reload network
	bool StartMoveNetwork(CPed& ped);		

	// Access the reload data from the clip component
	const CWeaponComponentReloadData* GetReloadData();

	// Access the reload data from the clip component
	const CWeaponComponentReloadLoopedData* GetLoopedReloadData() const;

	// PURPOSE: Starts the reload clip for the weapon and sends the request to the reload network to start reloading
	void StartReloadClips(CPed& ped, const CWeaponComponentReloadData* pReloadData, const fwMvClipId& clipId);

	// PURPOSE: Compute the number of times to play the loop clip
	s32 ComputeNumberOfTimesToPlayReloadLoop(const CWeapon& weapon, const CWeaponInfo& weaponInfo) const;

	// PURPOSE: Decrements the loop count and sets the loop boolean
	void UpdateBulletReloadStatus();

	// PURPOSE: Polls for the animation events to release, create and destroy gun components during reload
	void ProcessClipEvents();

	// PURPOSE: Determine if we should interrupt the task and quit
	bool ComputeShouldInterrupt() const;

	// PURPOSE:
	bool ShouldBlendOut();

	// PURPOSE:
	bool CreateHeldClipObject();

	// PURPOSE:
	bool CreateHeldOrdnanceObject();

	// PURPOSE:
	void CreateHeldObject(u32 uModelHash);

	// PURPOSE:
	void DeleteHeldObject();

	// PURPOSE:
	void ReleaseHeldObject();

	void CreateAmmoClip();
	void CreateAmmoClipInLeftHand();
	void CreateOrdnance();
	void DestroyAmmoClip();
	void DestroyAmmoClipInLeftHand();
	void ReleaseAmmoClip();
	void ReleaseAmmoClipInLeftHand();
	bool ShouldCreateAmmoClipOnCleanUp() const;

	// Check if the vector to the target position is blocked by an obstruction
	bool IsWeaponBlocked();

	void ReloadOnCleanUp(CPed& ped);

	void SendPitchParam(CPed& ped);

	//
	// Members
	//

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	// PURPOSE: Determines our gun state based on ai/player input
	const CWeaponController::WeaponControllerType m_weaponControllerType;

	// PURPOSE: Clipset to use for the reload
	fwMvClipSetId m_ClipSetId;

	// PURPOSE: Clip held in left hand
	RegdObj m_pHeldObject;

	// PURPOSE: Rate at which to play the reload clip, cached from the weapon info
	float m_fClipReloadRate;

	// PURPOSE: Number of bullets to insert per clip loop (cached from weapon info)
	s32 m_iBulletsToReloadPerLoop;

	// PURPOSE: For some weapons we have single bullet reload clip which we should loop the required number of times
	s32 m_iNumTimesToPlayReloadLoop;

	// PURPOSE: Pitch angle
	float m_fPitch;

	// PURPOSE: Network blend out duration
	float m_fBlendOutDuration;

	const CWeaponComponentReloadData* m_pCachedReloadData;

	u8 m_nInitialGunAttachPoint;

	// PURPOSE: Cache off the weapon we had at the start of the task, so we know whether it's been force changed mid-reload
	u32 m_uCachedWeaponHash;

	bool m_bIsLoopedReloadFromEmpty;

	// Flags
	fwFlags32 m_iFlags;
	bool m_bLoopReloadClip : 1;					// PURPOSE: Do we need to loop the reload when its finished?
	bool m_bEnteredLoopState : 1;				// PURPOSE: Don't bother resending the loop state request since we are already in the correct state
	bool m_bCreatedProjectileOrdnance : 1;		// PURPOSE: Flag that identifies if we have created the attached projectile
	bool m_bCreatedAmmoClip : 1;				// PURPOSE: Flags used to determine if the ammo clip creation or destruction have occurred
	bool m_bCreatedAmmoClipInLeftHand : 1;
	bool m_bDestroyedAmmoClip : 1;
	bool m_bDestroyedAmmoClipInLeftHand : 1;
	bool m_bReleasedAmmoClip : 1;
	bool m_bReleasedAmmoClipInLeftHand : 1;
	bool m_bHoldingClip : 1;
	bool m_bHoldingOrdnance : 1;
	bool m_bStartedMovingWhenNotAiming : 1;		// PURPOSE: Started moving when not aiming
	bool m_bPressedSprintWhenNotAiming : 1;
	bool m_bInterruptRequested : 1;
	bool m_bAnimSwapRequested : 1;
	bool m_bReloadClipFinished : 1;
	bool m_bGunFeedVisibile : 1;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvRequestId ms_ReloadRequestId;
	static const fwMvRequestId ms_ReloadInRequestId;
	static const fwMvRequestId ms_ReloadLoopRequestId;
	static const fwMvRequestId ms_ReloadOutRequestId;
	static const fwMvBooleanId ms_ReloadOnEnterId;
	static const fwMvBooleanId ms_ReloadInOnEnterId;
	static const fwMvBooleanId ms_ReloadLoopOnEnterId;
	static const fwMvBooleanId ms_ReloadOutOnEnterId;
	static const fwMvBooleanId ms_ReloadLoopedId;
	static const fwMvBooleanId ms_ReloadInFinishedId;
	static const fwMvBooleanId ms_ReloadLoopedFinishedId;
	static const fwMvBooleanId ms_ReloadClipFinishedId;
	static const fwMvBooleanId ms_LoopReloadId;
	static const fwMvBooleanId ms_CreateOrdnance;
	static const fwMvBooleanId ms_CreateAmmoClip;
	static const fwMvBooleanId ms_CreateAmmoClipInLeftHand;
	static const fwMvBooleanId ms_DestroyAmmoClip;
	static const fwMvBooleanId ms_DestroyAmmoClipInLeftHand;
	static const fwMvBooleanId ms_ReleaseAmmoClip;
	static const fwMvBooleanId ms_ReleaseAmmoClipInLeftHand;
	static const fwMvBooleanId ms_Interruptible;
	static const fwMvBooleanId ms_BlendOut;
	static const fwMvBooleanId ms_AllowSwapWeapon;
	static const fwMvBooleanId ms_HideGunFeedId;
	static const fwMvBooleanId ms_AttachGunToLeftHand;
	static const fwMvBooleanId ms_AttachGunToRightHand;
	static const fwMvFloatId ms_ReloadRateId;
	static const fwMvFloatId ms_ReloadPhaseId;
	static const fwMvFloatId ms_PitchId;
	static const fwMvClipId ms_ReloadClipId;
	static const fwMvClipId ms_ReloadInClipId;
	static const fwMvClipId ms_ReloadLoopClipId;
	static const fwMvClipId ms_ReloadOutClipId;
	static const fwMvFilterId ms_UpperBodyFeatheredCoverFilterId;

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
public:
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// CClonedTaskReloadGunInfo
//////////////////////////////////////////////////////////////////////////

class CClonedTaskReloadGunInfo : public CSerialisedFSMTaskInfo
{
	public:

		CClonedTaskReloadGunInfo
		(
			bool const		_bIsIdleReload,
			bool const		_bLoopReloadClip,
			bool const		_bEnteredLoopState,
			s32	const		_iNumTimesToPlayReloadLoop
		);
		
		CClonedTaskReloadGunInfo();

		~CClonedTaskReloadGunInfo();

		virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_RELOAD_GUN;}
		
		inline fwMvClipSetId	GetClipSet()					const { return m_ClipSetId; }
		inline bool				GetIsIdleReload()				const { return m_bIsIdleReload; }
		inline bool				GetLoopReloadClip()				const { return m_bLoopReloadClip; }
		inline bool				GetEnteredLoopState()			const { return m_bEnteredLoopState; }
		inline s32				GetNumTimesToPlayReloadLoop()	const { return m_iNumTimesToPlayReloadLoop; }

		virtual CTaskFSMClone *CreateCloneFSMTask();

		void Serialise(CSyncDataBase& serialiser)
		{
			CSerialisedFSMTaskInfo::Serialise(serialiser);

			SERIALISE_ANIMATION_CLIP(serialiser, m_ClipSetId, m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall, "Animation");

			bool isIdleReload = m_bIsIdleReload;
			SERIALISE_BOOL(serialiser, isIdleReload, "IsIdleReload");
			m_bIsIdleReload = isIdleReload;

			bool loopReloadClip = m_bLoopReloadClip;
			SERIALISE_BOOL(serialiser, loopReloadClip, "LoopReloadAnim");
			m_bLoopReloadClip = loopReloadClip;

			bool enteredLoopState = m_bEnteredLoopState;
			SERIALISE_BOOL(serialiser, enteredLoopState, "EnteredLoopState");
			m_bEnteredLoopState = enteredLoopState; 

			int numTimesToPlayReloadLoop = m_iNumTimesToPlayReloadLoop;
			SERIALISE_INTEGER(serialiser, numTimesToPlayReloadLoop, SIZEOF_NUM_TIMES_TO_PLAY_RELOAD, "NumTimesToPlayReloadLoop");
			m_iNumTimesToPlayReloadLoop = numTimesToPlayReloadLoop;
		}

	private:

		CClonedTaskReloadGunInfo(const CClonedTaskReloadGunInfo &);
		CClonedTaskReloadGunInfo &operator=(const CClonedTaskReloadGunInfo &);

		static const unsigned int SIZEOF_NUM_TIMES_TO_PLAY_RELOAD = 4;

		fwMvClipSetId m_ClipSetId;
		s32 m_iNumTimesToPlayReloadLoop:4;
		bool m_bIsIdleReload:1;
		bool m_bLoopReloadClip:1;
		bool m_bEnteredLoopState:1;
		
		static fwMvClipId m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;
};

#endif // INC_TASK_RELOAD_GUN_H
