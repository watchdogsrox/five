//
// Task/Weapons/Gun/TaskAimGunScripted.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_AIM_GUN_SCRIPTED_H
#define INC_TASK_AIM_GUN_SCRIPTED_H

////////////////////////////////////////////////////////////////////////////////

// Game Headers
#include "Peds/QueriableInterface.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/Gun/TaskAimGun.h"

////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// PURPOSE: Gun task for script mission
//////////////////////////////////////////////////////////////////////////

class CTaskAimGunScripted : public CTaskAimGun
{
public:
	friend class CWeapon;

	// PURPOSE: Statics
	static bank_float ms_fNetworkBlendInDuration;
	static bank_float ms_fNetworkBlendOutDuration;
	static bank_float ms_fMaxHeadingChangeRate;
	static bank_float ms_fMaxPitchChangeRate;

	// PURPOSE: FSM states
	enum eScriptedGunTaskState
	{
		State_Start = 0,
		State_Idle,
		State_Intro,
		State_Aim,
		State_Fire,
		State_Outro,
		State_Reload,
		State_Blocked,
		State_SwapWeapon,
		State_Finish,
	};

	// PURPOSE: Constructor
	CTaskAimGunScripted(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const CWeaponTarget& target);

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_AIM_GUN_SCRIPTED; }
	
	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskAimGunScripted(m_weaponControllerType, m_iFlags, m_target); }

	// PURPOSE: This function allows an active task to override the game play camera and associated settings.
	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	// PURPOSE: Aim gun task implementations
	virtual bool GetIsReadyToFire() const { return GetState() == State_Aim || GetState() == State_Fire; }
	virtual bool GetIsFiring() const { return GetState() == State_Fire; }
	virtual bool GetIsWeaponBlocked() const { return GetState() == State_Blocked; }

	// PURPOSE: Clone task implementation
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void		ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return UpdateClonedFSM( const s32 , const FSM_Event );
	virtual void		OnCloneTaskNoLongerRunningOnOwner();
	virtual float		GetScopeDistance() const;

	FSM_Return			Fire_OnUpdateClone();

	// PURPOSE: Returns a 0.0f to 1.0f normalized value determining the where in the scripted animation tasks
	// aiming range the ped is trying to hit.
	float GetDesiredHeading() { return m_fDesiredHeading; }

	// PURPOSE: Returns the minimum aim sweep pitch angle, in radians.
	float GetMinPitchAngle() const { return m_fMinPitchAngle; }
	// PURPOSE: Returns the maximum aim sweep pitch angle, in radians.
	float GetMaxPitchAngle() const { return m_fMaxPitchAngle; }

	// PURPOSE:
	void SetRopeOrientationEntity(CEntity* pEntity) { m_pRopeOrientationEntity = pEntity; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);


	// PURPOSE: Task update that is called after camera update, once per frame.
	virtual bool ProcessPostCamera();

	// PURPOSE: Called after the PreRender
	virtual bool ProcessPostPreRender();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on FSM events within main UpdateFSM function
	FSM_Return	Start_OnUpdate();
	FSM_Return	Idle_OnEnter();
	FSM_Return	Idle_OnUpdate();
	FSM_Return	Intro_OnEnter();
	FSM_Return	Intro_OnUpdate();
	FSM_Return	Aim_OnEnter();
	FSM_Return	Aim_OnUpdate();
	FSM_Return	Fire_OnEnter();
	FSM_Return	Fire_OnUpdate();
	FSM_Return	Fire_OnExit();
	FSM_Return	Outro_OnEnter();
	FSM_Return	Outro_OnUpdate();
	FSM_Return	Reload_OnEnter();
	FSM_Return	Reload_OnUpdate();
	FSM_Return	Reload_OnExit();
	FSM_Return	Blocked_OnEnter();
	FSM_Return	Blocked_OnUpdate();
	FSM_Return  SwapWeapon_OnEnter();
	FSM_Return  SwapWeapon_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Returns the current weapon controller state for the specified ped
	virtual WeaponControllerState GetWeaponControllerState(const CPed* pPed) const;

	// PURPOSE: Update the signals sent to the move network
	void UpdateMoVE();

	// PURPOSE: Is the ped aiming or firing
	bool IsPedAimingOrFiring(const CPed* pPed) const;

	// PURPOSE: Compute the intro parameter based on the desired yaw in radians
	void CalculateIntroBlendParam(float fDesiredYaw);

	// PURPOSE: Compute the outro parameter based on the desired yaw in radians
	void CalculateOutroBlendParam(float fDesiredYaw);

	// PURPOSE: Compute if the additive fire anim uses fire events
	bool UsesFireEvents();

	// PURPOSE: Compute if the current clup set has a additive fire anim
	const bool HasAdditiveFireAnim();

	// PURPOSE: Compute if we can fire targetted ped
	const bool CheckCanFireAtPed(CPed* pPed);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_MoveNetworkHelper;

	// PURPOSE: ClipSet id 
	fwMvClipSetId m_ClipSetId;

	// PURPOSE: Min/Max sweep angles for heading/pitch
	float m_fMinHeadingAngle;
	float m_fMaxHeadingAngle;
	float m_fMinPitchAngle;
	float m_fMaxPitchAngle;
	
	// PURPOSE: Offsets to current heading and pitch
	float m_fHeadingOffset;

	// PURPOSE: Cameras to be used
	u32 m_uIdleCameraHash;
	u32 m_uCameraHash;

	// PURPOSE: Reference to an entity which determines the orientation of the ped when dangling from a rope. Usually just
	// set to the object at the other end of the rope.
	RegdEnt m_pRopeOrientationEntity;

	// PURPOSE: Allow going from phase 0 to 1 and vice versa
	bool m_bAllowWrapping : 1;
	// PURPOSE: Allow going from phase 0 to 1 and vice versa
	bool m_bAbortOnIdle : 1;
	// PURPOSE: Used to determine if a fire has happened yet
	bool m_bFiredOnce : 1;

	// PURPOSE: Force the ped's matrix to match the orientation of the rope he is attached to in those contexts.
	bool m_bTrackRopeOrientation : 1;

	// PURPOSE: Firing is an additive contained within the aim state in the move network,
	// so we do not want to wait for the target state when going from firing to aiming as we're already in the correct state
	// ditto for reloading
	bool m_bIgnoreRequestToAim : 1;

	// PURPOSE: Should we fire the weapon?
	bool m_bFire : 1;

	// PURPOSE: Desired heading/pitch sweep values, the current ones get interpolated to the desired
	float m_fDesiredHeading;
	float m_fDesiredPitch;

	// PURPOSE: Intro parameter, calculated once to blend clips to reach a particular heading from idle
	float m_fIntroHeadingBlend;

	// PURPOSE: Current heading/pitch params
	float m_fCurrentHeading;
	float m_fCurrentPitch;

	// PURPOSE: Outro parameter, calculated once to blend clips to move back to idle from a particular heading
	float m_fOutroHeadingBlend;

	// PURPOSE: Force a remove from idle and go into aim first.
	bool m_bForceReload; 


	// PURPOSE: Defines the maximum amount we allow the phase of the heading/pitch params in one step
	float m_fHeadingChangeRate;
	float m_fPitchChangeRate;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvRequestId ms_IdleRequestId;
	static const fwMvRequestId ms_IntroRequestId;
	static const fwMvRequestId ms_OutroRequestId;
	static const fwMvRequestId ms_ReloadRequestId;
	static const fwMvRequestId ms_BlockedRequestId;
	static const fwMvRequestId ms_AimRequestId;
	static const fwMvBooleanId ms_IntroOnEnterId;
	static const fwMvBooleanId ms_AimOnEnterId;
	static const fwMvBooleanId ms_ReloadingOnEnterId;
	static const fwMvBooleanId ms_BlockedOnEnterId;
	static const fwMvBooleanId ms_IdleOnEnterId;
	static const fwMvBooleanId ms_OutroOnEnterId;
	static const fwMvBooleanId ms_OutroFinishedId;
	static const fwMvBooleanId ms_ReloadClipFinishedId;
	static const fwMvBooleanId ms_FireLoopedId;
	static const fwMvBooleanId ms_FireId;
	static const fwMvBooleanId ms_FireLoopedOneFinishedId;
	static const fwMvBooleanId ms_FireLoopedTwoFinishedId;
	static const fwMvFloatId ms_IntroRateId;
	static const fwMvFloatId ms_OutroRateId;
	static const fwMvFloatId ms_HeadingId;
	static const fwMvFloatId ms_IntroHeadingId;
	static const fwMvFloatId ms_OutroHeadingId;
	static const fwMvFloatId ms_PitchId;
	static const fwMvFlagId ms_FiringFlagId;
	static const fwMvFlagId ms_FiringLoopId;
	static const fwMvFlagId ms_AbortOnIdleId;
	static const fwMvFlagId ms_UseAdditiveFiringId;
	static const fwMvFlagId ms_UseDirectionalBreatheAdditivesId;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL

#if __BANK
	// PURPOSE: Disables blocking
	static bool ms_bDisableBlocking;

	// PURPOSE: Add widgets to RAG
	static void AddWidgets(bkBank& bank);
#endif // __BANK

	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// CClonedAimGunScriptedInfo
// Purpose: Task info for aim gun scripted task
//////////////////////////////////////////////////////////////////////////

class CClonedAimGunScriptedInfo : public CSerialisedFSMTaskInfo
{
public:
    CClonedAimGunScriptedInfo();
	CClonedAimGunScriptedInfo(s32 aimGunScriptedState, float fPitch, float fHeading);
    ~CClonedAimGunScriptedInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_AIM_GUN_SCRIPTED;}

    virtual bool    HasState() const { return true; }
    virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskAimGunScripted::State_Finish>::COUNT;;}
    virtual const char*	GetStatusName(u32) const { return "Status";}

	float GetPitch()	{ return m_fPitch; }
	float GetHeading()	{ return m_fHeading; }

    void Serialise(CSyncDataBase& serialiser)
    {
        CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_PACKED_FLOAT(serialiser, m_fPitch,	1.0f, SIZEOF_PITCH,		"Aim Scripted Pitch");
		SERIALISE_PACKED_FLOAT(serialiser, m_fHeading,	1.0f, SIZEOF_HEADING,	"Aim Scripted Heading");
    }

private:

    //static const unsigned int SIZEOF_AIM_GUN_SCRIPTED = 3;
	static const unsigned int SIZEOF_PITCH			= 8;
	static const unsigned int SIZEOF_HEADING		= 8;

	float m_fPitch;
	float m_fHeading;
};


#endif // INC_TASK_AIM_GUN_SCRIPTED_H
