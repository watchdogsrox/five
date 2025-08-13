#ifndef TASK_PLAYER_ON_HORSE_H
#define TASK_PLAYER_ON_HORSE_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

class CHorseFollowTargetHelper
{
public:
	CHorseFollowTargetHelper();

	// Similar to CTaskPlayerOnHorse::DoJumpCheck
	bool DoFollowTargetCheck(CPed& player);

	//has the player met the input conditions to speed match?
	bool IsNewHorseFollowReady() const;

	void UpdateNewHorseFollow(CPed* pPlayerPed, bool& bMaintainSpeedOut, bool bSpurredThisFrame);
	
private:
	float	m_HorseFollowActivationTime;
	float	m_HorseFollowTimeLastActive;

	static float	sm_HorseFollowActivationTime;
	static float	sm_HorseFollowFadeoutTime;
};


class CTaskPlayerOnHorse : public CTask
{
public:
	enum ePlayerOnHorseState
	{
		STATE_INITIAL,
		STATE_PLAYER_IDLES,		
		STATE_AIM_USE_GUN,	
		STATE_RIDE_USE_GUN,		//Right now this is used only for reloading when not in aim mode, but this could be extended to also include shooting when not in aim mode.
		STATE_USE_THROWING_WEAPON,
		STATE_JUMP,
		STATE_FOLLOW_TARGET,
		STATE_DISMOUNT,
		STATE_SWAP_WEAPON,
		STATE_INVALID
	};

	CTaskPlayerOnHorse();
	~CTaskPlayerOnHorse();

	// Virtual task functions
	virtual aiTask*	Copy() const;
	virtual int		GetTaskTypeInternal() const {return CTaskTypes::TASK_PLAYER_ON_HORSE;}	

	// Virtual FSM functions
	virtual FSM_Return	ProcessPreFSM				();
	virtual FSM_Return	UpdateFSM					(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return	ProcessPostFSM				();
	virtual s32			GetDefaultStateAfterAbort	() const { return STATE_INITIAL; }

	// FSM state functions //
	FSM_Return		Initial_OnUpdate			(CPed* pPlayerPed);
	void			PlayerIdles_OnEnter			(CPed* pPlayerPed);
	FSM_Return		PlayerIdles_OnUpdate		(CPed* pPlayerPed);	
	void			UseGunAiming_OnEnter		(CPed* pPlayerPed);
	FSM_Return		UseGunAiming_OnUpdate		(CPed* pPlayerPed);
	void			UseGunRiding_OnEnter		(CPed* pPlayerPed);
	FSM_Return		UseGunRiding_OnUpdate		(CPed* pPlayerPed);
	void			UseThrowingWeapon_OnEnter	(CPed* pPlayerPed);
	FSM_Return		UseThrowingWeapon_OnUpdate	(CPed* pPlayerPed);
	void			Jump_OnEnter				(CPed* pPlayerPed);
	FSM_Return		Jump_OnUpdate				(CPed* pPlayerPed);
	void			FollowTarget_OnEnter		(CPed* pPlayerPed);
	FSM_Return		FollowTarget_OnUpdate		(CPed* pPlayerPed);
	void			Dismount_OnEnter			(CPed* pPlayerPed);
	FSM_Return		Dismount_OnUpdate			(CPed* pPlayerPed);
	void			SwapWeapon_OnEnter			(CPed* pPlayerPed);
	FSM_Return		SwapWeapon_OnUpdate			(CPed* pPlayerPed);

//	void UpdateMovementStickHistory( CControl * pControl );

	virtual bool IsValidForMotionTask(CTaskMotionBase& pTask) const;

	void DetachReins(CPed* myMount);
	bool IsInValidStateToBeginPerformingACarrierAction() const { return (GetState() == STATE_PLAYER_IDLES); }
	// Debug functions
#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif
	
	static void DriveHorse(CPed* pPlayerPed, bool bHeadingOnly=false);

protected:	

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);


	bool CheckForDismount( CPed* pPlayerPed );
	bool CheckForSwapWeapon( CPed* pPlayerPed );
	void PreStreamDrivebyClipSets( CPed* pPed );

	bool DoJumpCheck(CPed* pPlayerPed);
	VehicleEnterExitFlags GetDefaultDismountFlags() const;
	void CheckForWeaponAutoHolster();

	s32		   m_iDismountTargetEntryPoint;
	bool	   m_bDriveByClipSetsLoaded;
	bool	   m_bHorseJumping;
	bool	   m_bAutoJump;
	bool	   m_bDoAutoJumpTest;
	static bool	   m_sbStickHeadingUpdatedLastFrame;

	static const s32 MAX_DRIVE_BY_CLIPSETS = 4;
	CMultipleClipSetRequestHelper<MAX_DRIVE_BY_CLIPSETS> m_MultipleClipSetRequestHelper;

	CHorseFollowTargetHelper m_FollowTargetHelper;
};

#endif // ENABLE_HORSE

#endif // TASK_PLAYER_ON_HORSE_H


