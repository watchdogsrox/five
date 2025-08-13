#ifndef TASK_PLAYER_H
#define TASK_PLAYER_H

// Framework headers

// Game headers
#include "Control/Route.h"
#include "Peds/AssistedMovement.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskMove.h"
#include "Task/System/TaskTypes.h"
#include "Task/Movement/Climbing/ClimbDetector.h"


class CPed;
class CVehicle;
class CClipPlayer;
class CEntity;
class CObject;
class CCoverPoint;
class CLadderClipSetRequestHelper;

#if FPS_MODE_SUPPORTED
class CFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

struct SaveHouseStruct;

//Rage headers.
#include "Vector/Vector3.h"

class CTaskPlayerOnFoot : public CTask
{
public:
	enum ePlayerOnFootState
	{
		STATE_INITIAL,
		STATE_PLAYER_IDLES,
		STATE_JUMP,
		STATE_MELEE,
		STATE_SWAP_WEAPON,
		STATE_USE_COVER,
		STATE_CLIMB_LADDER,
		STATE_PLAYER_GUN,		
		STATE_GET_IN_VEHICLE,
		STATE_TAKE_OFF_HELMET,
		STATE_MELEE_REACTION,
		STATE_ARREST,
		STATE_UNCUFF,
		STATE_MOUNT_ANIMAL,
		STATE_DUCK_AND_COVER,
		STATE_SLOPE_SCRAMBLE,
		STATE_SCRIPTED_TASK,
		STATE_DROPDOWN,
		STATE_TAKE_OFF_PARACHUTE_PACK,
		STATE_TAKE_OFF_JETPACK,
		STATE_TAKE_OFF_SCUBA_GEAR,
		STATE_RIDE_TRAIN,
		STATE_JETPACK,
		STATE_BIRD_PROJECTILE,
		STATE_INVALID
	};

	struct Tunables : public CTuning
	{
		struct ParachutePack
		{
			struct VelocityInheritance
			{
				VelocityInheritance()
				{}

				bool m_Enabled;

				float m_X;
				float m_Y;
				float m_Z;

				PAR_SIMPLE_PARSABLE;
			};

			ParachutePack()
			{}

			VelocityInheritance m_VelocityInheritance;

			float m_AttachOffsetX;
			float m_AttachOffsetY;
			float m_AttachOffsetZ;
			float m_AttachOrientationX;
			float m_AttachOrientationY;
			float m_AttachOrientationZ;
			float m_BlendInDeltaForPed;
			float m_BlendInDeltaForProp;
			float m_PhaseToBlendOut;
			float m_BlendOutDelta;

			PAR_SIMPLE_PARSABLE;
		};

		struct ScubaGear
		{
			struct VelocityInheritance
			{
				VelocityInheritance()
				{}

				bool m_Enabled;

				float m_X;
				float m_Y;
				float m_Z;

				PAR_SIMPLE_PARSABLE;
			};

			ScubaGear()
			{}

			VelocityInheritance m_VelocityInheritance;

			float m_AttachOffsetX;
			float m_AttachOffsetY;
			float m_AttachOffsetZ;
			float m_AttachOrientationX;
			float m_AttachOrientationY;
			float m_AttachOrientationZ;
			float m_PhaseToBlendOut;
			float m_BlendOutDelta;

			PAR_SIMPLE_PARSABLE;
		};

		struct JetpackData
		{
			struct VelocityInheritance
			{
				VelocityInheritance()
				{}

				bool m_Enabled;

				float m_X;
				float m_Y;
				float m_Z;

				PAR_SIMPLE_PARSABLE;
			};

			JetpackData()
			{}

			VelocityInheritance m_VelocityInheritance;
			
			float m_PhaseToBlendOut;
			float m_BlendOutDelta;
			float m_ForceToApplyAfterInterrupt;

			atHashString	m_ClipSetHash;
			atHashString	m_PedClipHash;
			atHashString	m_PropClipHash;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		ParachutePack	m_ParachutePack;
		ScubaGear		m_ScubaGear;
		JetpackData		m_JetpackData;

		bool  m_EvaluateThreatFromCoverPoints;
		bool  m_CanMountFromInAir;
		float m_ArrestDistance;
		float m_ArrestDot;
		float m_ArrestVehicleSpeed;
		float m_TakeCustodyDistance;
		float m_UncuffDistance;
		float m_MaxDistanceToTalk;
		float m_MinDotToTalk;
		float m_TimeBetweenPlayerEvents;
		float m_MaxEncumberedClimbHeight;
		float m_MaxTrainClimbHeight;
		float m_DistanceBetweenAiPedsCoverAndPlayersCover;
		float m_MaxDistanceAiPedFromTheirCoverToAbortPlayerEnterCover;
		float m_SmallCapsuleCoverPenalty;
		float m_SmallCapsuleCoverRadius;
		float m_PriorityCoverWeight;
		float m_EdgeCoverWeight;
		float m_VeryCloseToCoverDist;
		float m_VeryCloseToCoverWeight;
		float m_DistToCoverWeightThreat;
		float m_DistToCoverWeight;
		float m_DistToCoverWeightNoStickBonus;
		float m_DesiredDirToCoverWeight;
		float m_DesiredDirToCoverAimingWeight;
		float m_ThreatDirWeight;
		float m_ThreatEngageDirWeight;
		float m_CoverDirToCameraWeightMin;
		float m_CoverDirToCameraWeightMax;
		float m_CoverDirToCameraWeightMaxAimGun;
		float m_CoverDirToCameraWeightMaxScaleDist;
		float m_DesiredDirToCoverMinDot;
		float m_CameraDirToCoverMinDot;
		float m_StaticLosTest1Offset;
		float m_StaticLosTest2Offset;
		float m_CollisionLosHeightOffset;
		float m_VeryCloseIgnoreDesAndCamToleranceDist;
		float m_VeryCloseIgnoreDesAndCamToleranceDistAimGun;
		float m_DeadZoneStickNorm;
		float m_SearchThreatMaxDot;
		atHashString m_BirdWeaponHash;
		bool m_HideBirdProjectile;
		bool m_AllowFPSAnalogStickRunInInteriors;
		bool GetUseThreatWeighting(const CPed& rPed) const;

	private:

		bool  m_UseThreatWeighting;
		bool  m_UseThreatWeightingFPS;

		PAR_PARSABLE;
	};

	static void InitTunables();

	// Cached arrest type - used in Arrest_OnEnter.
	enum ArrestType
	{
		eAT_None = 0,
		eAT_Cuff,
		eAT_PutInCustody,
	};

	static bool IsAPlayerCopInMP(const CPed& ped);
	static bool CheckForUseMobilePhone(const CPed& ped);
	static bool TestIfRouteToCoverPointIsClear(CCoverPoint& rCoverPoint, CPed& rPed, const Vector3& vDesiredProtectionDir);
	static bool DidCapsuleTestHitSomething(const CPed& rPed, const Vector3& vStartPos, const Vector3& vEndPos, const float fCapsuleRadius);
	static bool DidProbeTestHitSomething(const Vector3& vStartPos, const Vector3& vEndPos, Vector3& vIntersectionPos, Vector3& vNormal);

	CTaskPlayerOnFoot();
	~CTaskPlayerOnFoot();

	// Virtual task functions
	virtual aiTask*	Copy() const;
	virtual int		GetTaskTypeInternal() const {return CTaskTypes::TASK_PLAYER_ON_FOOT;}
	virtual bool	ProcessPostPreRender();
	virtual void	ProcessPreRender2();

	// Virtual FSM functions
	virtual FSM_Return	ProcessPreFSM				();
	virtual FSM_Return	ProcessPostFSM				();
	virtual FSM_Return	UpdateFSM					(const s32 iState, const FSM_Event iEvent);
	virtual s32		GetDefaultStateAfterAbort	() const { return (m_bResumeGetInVehicle && GetState() == STATE_GET_IN_VEHICLE) ? STATE_GET_IN_VEHICLE : STATE_INITIAL; }
	virtual bool	ShouldAlwaysDeleteSubTaskOnAbort() const { return true; }	// Can resume, but doesn't need to preserve the subtasks.

	// FSM state functions //
	FSM_Return		Initial_OnUpdate			(CPed* pPlayerPed);
	void			PlayerIdles_OnEnter			(CPed* pPlayerPed);
	FSM_Return		PlayerIdles_OnUpdate		(CPed* pPlayerPed);
	void			PlayerIdles_OnExit			(CPed* pPlayerPed);
	void			Jump_OnEnter				(CPed* pPlayerPed);
	FSM_Return		Jump_OnUpdate				(CPed* pPlayerPed);
	void			DropDown_OnEnter			(CPed* pPlayerPed);
	FSM_Return		DropDown_OnUpdate			(CPed* pPlayerPed);
	void			Melee_OnEnter				(CPed* pPlayerPed);
	FSM_Return		Melee_OnUpdate				(CPed* pPlayerPed);
	void			Melee_OnExit				();
	void			SwapWeapon_OnEnter			(CPed* pPlayerPed);
	FSM_Return		SwapWeapon_OnUpdate			(CPed* pPlayerPed);
	void			UseCover_OnEnter			(CPed* pPlayerPed);
	FSM_Return		UseCover_OnUpdate			(CPed* pPlayerPed);
	FSM_Return		UseCover_OnExit				();
	void			ClimbLadder_OnEnter			(CPed* pPlayerPed);
	FSM_Return		ClimbLadder_OnUpdate		(CPed* pPlayerPed);
	FSM_Return		ClimbLadder_OnExit			(CPed* pPlayerPed);
	void			PlayerGun_OnEnter			(CPed* pPlayerPed);
	FSM_Return		PlayerGun_OnUpdate			(CPed* pPlayerPed);	
	FSM_Return		PlayerGun_OnExit			(CPed* pPlayerPed);	
	void			GetInVehicle_OnEnter		(CPed* pPlayerPed);
	FSM_Return		GetInVehicle_OnUpdate		(CPed* pPlayerPed);
	void			GetMountAnimal_OnEnter		(CPed* pPlayerPed);
	FSM_Return		GetMountAnimal_OnUpdate		(CPed* pPlayerPed);
	void			TakeOffHelmet_OnEnter		(CPed* pPlayerPed);
	FSM_Return		TakeOffHelmet_OnUpdate		(CPed* pPlayerPed);
	void			MeleeReaction_OnEnter		(CPed* pPlayerPed);
	FSM_Return		MeleeReaction_OnUpdate		(CPed* pPlayerPed);
	void			AutoDrop_OnEnter			(CPed* pPlayerPed);
	FSM_Return		AutoDrop_OnUpdate			(CPed* pPlayerPed);
	void			Arrest_OnEnter				(CPed* pPlayerPed);
	FSM_Return		Arrest_OnUpdate				(CPed* pPlayerPed);
	void			Uncuff_OnEnter				(CPed* pPlayerPed);
	FSM_Return		Uncuff_OnUpdate				(CPed* pPlayerPed);

	void			DuckAndCover_OnEnter		(CPed* pPlayerPed);
	FSM_Return		DuckAndCover_OnUpdate		(CPed* pPlayerPed);

	void			SlopeScramble_OnEnter		(CPed* pPlayerPed);
	FSM_Return		SlopeScramble_OnUpdate		(CPed* pPlayerPed);

	void			ScriptedTask_OnEnter		(CPed* pPlayerPed);
	FSM_Return		ScriptedTask_OnUpdate		(CPed* pPlayerPed);

	void			TakeOffParachutePack_OnEnter	(CPed* pPlayerPed);
	FSM_Return		TakeOffParachutePack_OnUpdate	(CPed* pPlayerPed);

	void			TakeOffJetpack_OnEnter	(CPed* pPlayerPed);
	FSM_Return		TakeOffJetpack_OnUpdate	(CPed* pPlayerPed);
	void			TakeOffJetpack_OnExit(CPed *UpPlayerPed);

	void			TakeOffScubaGear_OnEnter	(CPed* pPlayerPed);
	FSM_Return		TakeOffScubaGear_OnUpdate	(CPed* pPlayerPed);

	void			RideTrain_OnEnter();
	FSM_Return		RideTrain_OnUpdate();

	void			Jetpack_OnEnter();
	FSM_Return		Jetpack_OnUpdate();

	void			BirdProjectile_OnEnter();
	FSM_Return		BirdProjectile_OnUpdate();
	void			BirdProjectile_OnExit();

	void			SetMeleeRequestTask(aiTask* pMeleeTask) { m_pMeleeRequestTask = pMeleeTask; }

	void			SetScriptedTask(aiTask* pScriptedTask);
	aiTask*			GetScriptedTask() const { return m_pScriptedTask; }
	void			ClearScriptedTask();

	bool			GetShouldStartGunTask			(void) const { return m_bShouldStartGunTask; }
	void			SetShouldStartGunTask			(bool bShouldStartGunTask) { m_bShouldStartGunTask = bShouldStartGunTask; }

	bool			GetCanStealthKill				(void) const { return m_bCanStealthKill; }

	bool			GetCoverSprintReleased			(void) const { return m_bCoverSprintReleased; }	

	// Scan for requested state changes and fill out transitional member variables with appropriate data
	s32 GetDesiredStateFromPlayerInput(CPed* pPlayerPed);
	bool GetLastStickInputIfValid(Vector2& vStickInput) const;

	void UpdateMovementStickHistory( CControl * pControl );
	void SetScriptedToGoIntoCover(bool bTrue) {m_bScriptedToGoIntoCover = bTrue;}

	aiTask* GetForcedTask() const { return m_pMeleeReactionTask; }

	virtual bool IsValidForMotionTask(CTaskMotionBase& pTask) const;
	
	void SetLastTimeInCoverTask( u32 uLastTimeInCoverTask ) { m_uLastTimeInCoverTask = uLastTimeInCoverTask; }
	u32 GetLastTimeInCoverTask() const { return m_uLastTimeInCoverTask; }

	void SetLastTimeInMeleeTask( u32 uLastTimeInMeleeTask ) { m_uLastTimeInMeleeTask = uLastTimeInMeleeTask; }
	u32 GetLastTimeInMeleeTask() const { return m_uLastTimeInMeleeTask; }
	void SetWasMeleeStrafing( bool bWasStrafing ) { m_bWasMeleeStrafing = bWasStrafing; }
	bool GetWasMeleeStrafing() const { return m_bWasMeleeStrafing; }
	void SetCheckTimeInStateForCover( bool bCheckTimeInStateForCover )  { m_bCheckTimeInStateForCover = bCheckTimeInStateForCover; }
	bool GetPlayerExitedCoverThisUpdate() const { return m_bPlayerExitedCoverThisUpdate; }

	bool CheckForUsingJetpack(CPed *pPlayerPed);

	bool CheckForBirdProjectile(CPed* pPlayerPed);
	void SpawnBirdProjectile(CPed* pPlayerPed);

	bool CanPerformUncuff();

	bool CanUncuffPed(const CPed *pOtherPed, float &fOutScore) const;

	CPed *GetTargetUncuffPed() const { return m_pTargetUncuffPed; }
	CPed *GetTargetArrestPed() const { return m_pTargetArrestPed; }

	// returns true if the player is slope scrambling, or potentially about to slope scramble
	bool IsSlopeScrambling() const { return m_nFramesOfSlopeScramble > 0 || GetState() == STATE_SLOPE_SCRAMBLE; }

	bool ProcessSpecialAbilities();

	s32 GetBreakOutToMovementState(CPed* pPlayerPed);
	bool KeepWaitingForLadder(CPed* pPed, float fBlockHeading);

	void CheckForArmedMeleeAction(CPed* pPlayerPed);

	bool CheckForNearbyCover(CPed* pPlayerPed, u32 iFlags = CSF_DefaultFlags, bool bForceControl = false );

	void SetLastTimeEnterVehiclePressed(u32 uTimeInMS) { m_uLastTimeEnterVehiclePressed = uTimeInMS; }
	void SetLastTimeEnterCoverPressed(u32 uTimeInMS) { m_uLastTimeEnterCoverPressed = uTimeInMS; }
	bool HasValidEnterVehicleInput(const CControl& rControl) const;
	bool HasValidEnterCoverInput(const CControl& rControl) const;

	inline bool GetIdleWeaponBlocked(void) const {	return m_bIdleWeaponBlocked;	}

	bool CheckShouldPerformArrest(bool bIgnoreInput = false, bool bIsEnteringVehicle = false);

	bool CheckIfPedIsSuitableForArrest(bool bIgnoreInput, CPed* pTargetPed);

	static bool	CNC_PlayerCopShouldTriggerActionMode(const CPed* pPlayerPed);

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper* GetFirstPersonIkHelper() const { return m_pFirstPersonIkHelper; }
#endif // FPS_MODE_SUPPORTED

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug( ) const;
#endif

#if __BANK
	void DebugVehicleGetIns();
#endif 

	enum CoverSearchFlags 
	{ 
		CSF_ConsiderCloseCover		= BIT0, 
		CSF_ConsiderFarCover		= BIT1, 
		CSF_ConsiderDynamicCover	= BIT2, // Use collision checks to detect collision
		CSF_ConsiderStaticCover		= BIT3, // Use static coverpoints located in the map
		CSF_DontCheckButtonPress	= BIT4, 
		CSF_MatchCurrentCoverDir	= BIT5,

		CSF_DefaultFlags = CSF_ConsiderCloseCover|CSF_ConsiderFarCover|CSF_ConsiderDynamicCover|CSF_ConsiderStaticCover,
	};

	static bool		IsPlayerStickInputValid(const CPed& rPlayerPed);
	static float	StaticCoverpointScoringCB(const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData);
	static bool		StaticCoverpointValidityCallback (const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData);
	static bool		IsCoverValid(const Vector3& vPedPos, const Vector3& vCamFwd, const CPed& rPed, const CCoverPoint& rCover, bool bIsStaticCover, s32* piNoTexts);
	static float	ScoreCoverPoint(const Vector3& vPedPos, const Vector3& vCamFwd, const CPed& rPed, const CCoverPoint& rCover, bool bIgnoreValidityCheck, const Vector3& vDesiredCoverProtectionDir, bool bStickInputValid, bool bHasThreat, s32* piNoTexts = NULL);
	static void GetClimbDetectorParam(	s32 state,
										CPed* pPlayerPed, 
										sPedTestParams &pedTestParams, 
										bool bDoManualClimbCheck, 
										bool bAutoVaultOnly, 
										bool bAttemptingToVaultFromSwimming, 
										bool bHeavyWeapon,
										bool bCanDoStandAutoStepUp = true);

	static bool	CanJumpOrVault(CPed* pPlayerPed, 
		sPedTestResults &pedTestResults, 
		bool bHeavyWeapon,
		s32 nState,
		bool bForceVaultCheck = false,
		bool bCanDoStandAutoStepUp = true);

protected:

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

private:

	bool	ShouldMeleeTargetPed(const CPed* pTargetPed, const CWeapon* pEquippedWeapon, bool bIsLowerPriorityMeleeTarget);

	void	UpdatePlayerLockon					(CPed* pPlayerPed);
	CTask*	ChcekForWeaponHolstering			( CPed * pPlayerPed, CControl * pControl );
	void	DebugStrafing						( CPed * pPlayerPed );	
	bool	CheckCanEnterVehicle				( CVehicle* pTargetVehicle);
	
	float	ComputeNewCoverSearchDirection		(CPed* pPlayerPed);
	ePlayerOnFootState	CheckForAutoCover		(CPed* pPed);
	bool	CheckForAutoDrop					(CPed* pPed);
	bool	DoJumpCheck							(CPed* pPlayerPed, CControl * pControl, bool bHeavyWeapon, bool bForceVaultCheck = false);
	
	bool	CheckForDropDown					(CPed* pPlayerPed, CControl *pControl, bool bBiasRightSide = false);

	bool	CheckForDuckAndCover				(CPed* pPlayerPed);

	fwFlags32 MakeJumpFlags(CPed* pPlayerPed, const sPedTestResults &pedTestResults);

	bool	CheckForAirMount(CPed* pPlayerPed);
	bool	CheckShouldPerformUncuff();

	void	TriggerArrestForPed(CPed* pPed);
	void	ArrestTargetPed(CPed* pPed);

	bool    CheckShouldClimbLadder(CPed* pPlayerPed);
	bool	CanReachLadder(const CPed* pPed, const CEntity* pEntity, const Vector3& vLadderEmbarkPos);
	s8		CheckShouldEnterVehicle(CPed* pPlayerPed,CControl *pControl, bool bForceControl = false);
	bool	CheckShouldRideTrain() const;
	bool	CheckShouldSlopeScramble();

	void	ProcessPlayerEvents();

	bool	IsStateValidForIK() const; 
#if FPS_MODE_SUPPORTED
	bool	IsStateValidForFPSIK(bool bExcludeUnarmed = false) const;
#endif // FPS_MODE_SUPPORTED

	static bool HasPlayerSelectedGunTask(CPed* pPlayerPed, s32 nState);

	void	UpdateTalk(CPed* pPlayerPed, bool bCanBeFriendly, bool bCanBeUnfriendly);

#if DEBUG_DRAW
	void	DrawInitialCoverDebug(const CPed& rPed, s32& iNumPlayerTexts, s32 iTextOffset, const Vector3& vDesiredCoverProtectionDir);
#endif // DEBUG_DRAW

	bool LaddersAreClose(CEntity* m_pTargetLadder, s32 iLadderIndex, const Vector2& vHackedLadderPos, float fSearchRadius);

#if FPS_MODE_SUPPORTED
	bool UseAnalogRun(const CPed* pPlayerPed) const;
	void ProcessViewModeSwitch(CPed* pPlayerPed);
#endif // FPS_MODE_SUPPORTED

	//
	// Members
	//
	CPed* m_pTargetPed;
	CPed* m_pLastTargetPed;

	u32 m_uArrestFinishTime;
	u32 m_uArrestSprintBreakoutTime;
	bool m_bArrestTimerActive;

	// Aligned variables first
	Vector3 m_vLastCoverDir;
	Vector3 m_vLastStickInput;

	Vector3 m_vBlockPos;
	float m_fBlockHeading;

	CLadderClipSetRequestHelper* m_pLadderClipRequestHelper;	// Initiate from pool
	CAmbientClipRequestHelper m_optionalVehicleHelper;
	fwMvClipSetId m_chosenOptionalVehicleSet;

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper* m_pFirstPersonIkHelper;
	bool m_WasInFirstPerson;
	bool m_bForceInstantIkBlendInThisFrame;
#endif // FPS_MODE_SUPPORTED

	//////////////////////////////////////////////////////////////////////////
	// Local data for transitioning between states
	// Reset every frame
	//////////////////////////////////////////////////////////////////////////

	// Transition to melee
	RegdaiTask m_pMeleeRequestTask;
	RegdaiTask m_pMeleeReactionTask;

	// Scripted task
	RegdaiTask m_pScriptedTask;

	// Transition to get in vehicle
	RegdVeh	m_pTargetVehicle;

	// Transition to get on animal
	RegdPed	m_pTargetAnimal;

	// Transition to climb ladder
	RegdEnt	m_pTargetLadder;
	RegdEnt	m_pPreviousTargetLadder;
	int m_iLadderIndex;

	// Results structure to check if the ped can reach a ladder
	WorldProbe::CShapeTestResults m_LadderProbeResults;
	// Results structure to check if ladder is clear vertically
	WorldProbe::CShapeTestResults m_LadderVerticalProbeResults;
	// Boolean kept to remember the last result in the async test
	bool m_bCanReachLadder;
	// Boolean kept to remember the last result of vertical probe test
	bool m_bLadderVerticalClear;

	bool m_bUseSmallerVerticalProbeCheck;

	// Target uncuff/arrest peds.
	RegdPed	m_pTargetUncuffPed;
	RegdPed	m_pTargetArrestPed;

	float m_fIdleTime;	
	RegdPed m_pPedPlayerIntimidating;
	float m_fTimeIntimidating;
	float m_fTimeBetweenIntimidations;
	u32 m_uLastTimeInCoverTask;
	u32 m_uLastTimeInMeleeTask;
	u32 m_uLastTimeEnterVehiclePressed;
	u32 m_uLastTimeEnterCoverPressed;
	bool m_bWasMeleeStrafing;
	float m_fTimeSinceLastStickInput;
	float m_fTimeSinceLastValidSlopeScramble;
	float m_fTimeSinceBirdProjectile;

	float m_fTimeSinceLastPlayerEventOpportunity;

	fwFlags32 m_iJumpFlags;

	ArrestType m_eArrestType;

	u8 m_nFramesOfSlopeScramble;

	sPedTestResults m_VaultTestResults;
	float m_fStandAutoStepUpTimer;

	s32 m_nCachedBreakOutToMovementState;

#if FPS_MODE_SUPPORTED
	float m_fGetInVehicleFPSIkZOffset;
	float m_fTimeSinceLastSwimSprinted;
#endif // FPS_MODE_SUPPORTED

	// Store the last time the jump/sprint buttons were pressed (for vaulting)
	static u32 m_uLastTimeJumpPressed;
	static u32 m_uLastTimeSprintDown;
	static u32 m_uReducingGaitTimeMS;
	static u32 m_nLastFrameProcessedClimbDetector;

	// Flags
	bool m_bWaitingForPedArrestUp : 1;
	bool m_bOptionVehicleGroupLoaded : 1;
	bool m_bScriptedToGoIntoCover : 1;
	bool m_bShouldStartGunTask : 1;
	bool m_bCanStealthKill : 1;
	bool m_bReloadFromIdle : 1;
	bool m_bUsingStealthClips : 1;
	bool m_bJumpedFromCover : 1;
	bool m_bCoverSprintReleased : 1;
	bool m_bPlayerExitedCoverThisUpdate : 1;	// Flag used to prevent the player from being able to enter cover the same frame as he exited cover
	bool m_bGettingOnBottom : 1;
	bool m_b1stTimeSeenWhenWanted : 1;
	bool m_bResumeGetInVehicle : 1;
	bool m_bCanBreakOutToMovement : 1;
	bool m_bIdleWeaponBlocked : 1;
	bool m_bCheckTimeInStateForCover : 1;
	bool m_bWaitingForLadder : 1;
	bool m_bEasyLadderEntry : 1;
	bool m_bHasAttemptedToSpawnBirdProjectile : 1;
	bool m_bNeedToSpawnBirdProjectile : 1;
#if FPS_MODE_SUPPORTED
	bool m_bGetInVehicleAfterTakingOffHelmet : 1;
#endif // FPS_MODE_SUPPORTED
	bool m_bGetInVehicleAfterCombatRolling : 1;

public:
	
	//////////////////////////////////////
	// Static member variables

	static bank_u32 ms_iQuickSwitchWeaponPickUpTime;

	// Distance at which the player will enter melee (when targeting)
	static dev_float ms_fMeleeStartDistance;
	static dev_float ms_fMinTargetCheckDistSq;
	static dev_u32 ms_nRestartMeleeDelayInMS;
	static dev_u32 ms_nIntroAnimDelayInMS;

	// Allow the player to strafe when target is down & player is unarmed?
	static bank_bool ms_bAllowStrafingWhenUnarmed;

	// Cannot jump in snow over this depth (ratio 0..1)
	static bank_float ms_fMaxSnowDepthRatioForJump;
	static dev_float ms_fStreamClosestLadderMinDistSqr;

	// Whether to allow the player to climb ladders
	static bank_bool ms_bAllowLadderClimbing;
	static bank_bool ms_bRenderPlayerVehicleSearch;
	static bank_bool ms_bAnalogueLockonAimControl;
	static bank_bool ms_bDisableTalkingThePlayerDown;
	static bank_bool ms_bEnableDuckAndCover;
	static bank_bool ms_bEnableSlopeScramble;

	static Tunables	sm_Tunables;
};


class CTaskPlayerIdles : public CTask
{
public:

	// FSM states
	enum
	{
		State_Start = 0,
		State_MovementAndAmbientClips,
		State_Finish
	};

public:

	CTaskPlayerIdles();
	~CTaskPlayerIdles();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskPlayerIdles(); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_PLAYER_IDLES;}
	s32 GetDefaultStateAfterAbort() const { return State_Start; }
	
	inline bool HasSpaceToPlayIdleAnims() const { return m_bHasSpaceToPlayIdleAnims; }
private:

	// State Machine Update Functions
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	FSM_Return					Start_OnUpdate						();
	void								MovementAndAmbientClips_OnEnter		();
	FSM_Return					MovementAndAmbientClips_OnUpdate	();
	FSM_Return					MovementAndAmbientClips_OnExit	();

private:
	bool m_bHasSpaceToPlayIdleAnims:1;
	bool m_bIsMoving:1;

#if !__FINAL
public:
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif
};

class CTaskMovePlayer : public CTaskMove
{
public:
	CTaskMovePlayer();
	~CTaskMovePlayer();

	enum // States
	{
		Running
	};

	static float ms_fWalkBlend;
	static float ms_fRunBlend;
	static float ms_fSprintBlend;
	static float ms_fStrafeMinMoveBlendRatio;
	static float ms_fSmallMoveBlendRatioToDoIdleTurn;
	static float ms_fTimeStickHeldBeforeDoingIdleTurn;
	static bool ms_bStickyRunButton;
	static bank_float ms_fStickInCentreMag;
	static bool ms_bAllowRunningWhilstStrafing;
	static bool ms_bDefaultNoSprintingInInteriors;
	static bool ms_bScriptOverrideNoRunningOnPhone;
	static bool ms_bUseMultiPlayerControlInSinglePlayer;
	static bank_bool ms_bFlipSkiControlsWhenBackwards;
	static float ms_fUnderwaterIdleAutoLevelTime;
	static dev_float ms_fDeadZone;
	static dev_float ms_fMinDepthForPlayerFish;

	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_PLAYER; }

	virtual s32 GetDefaultStateAfterAbort() const { return Running; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return StateRunning_OnUpdate(CPed * pPed);

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "State_Running";  };
#endif

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const {Assertf(0, "CTaskMovePlayer : Get target called on move task with move blend ratio MOVEBLENDRATIO_STILL" ); return Vector3(0.0f, 0.0f, 0.0f);}
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const {Assertf(0, "CTaskMovePlayer : Get target radius called on move task with move blend ratio MOVEBLENDRATIO_STILL" ); return 0.0f;}

	void SetMaxMoveBlendRatio(float fMoveBlendRatio) { m_fMaxMoveBlendRatio = fMoveBlendRatio; }

#if __PLAYER_ASSISTED_MOVEMENT
	inline CPlayerAssistedMovementControl * GetAssistedMovementControl() { return m_pAssistedMovementControl; }
	inline const CPlayerAssistedMovementControl * GetAssistedMovementControl() const { return m_pAssistedMovementControl; }
#endif

	void LockStickHeading(bool bUseCurrentCamera);

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

private:

	void ProcessStdMove(CPed* pPlayerPed);
	void ProcessStrafeMove(CPed* pPlayerPed);
	void ProcessSkiMove(CPed* pPlayerPed);
	void ProcessSwimMove(CPed* pPlayerPed);
	void ProcessBirdMove(CPed& playerPed);
	void ProcessFishMove(CPed& playerPed);

	float GetFishMaxPitch(const CPed& playerPed);
	float GetFishMinPitch(const CPed& playerPed);
	
#if FPS_MODE_SUPPORTED
	bool ShouldBeRunningDueToAnalogRun(const CPed* pPlayerPed, const float fCurrentMBR, const Vector2& vecStickInput) const;
	bool ShouldCheckRunInsteadOfSprint(CPed* pPlayerPed, float fMoveBlendRatio) const;
#endif // FPS_MODE_SUPPORTED

	bool m_bStickHasReturnedToCentre;
	float m_fUnderwaterGlideTimer;
	float m_fUnderwaterIdleTimer;	
	float m_fSwitchStrokeTimerOnSurface;
	float m_iStroke;	// (0==breast, 1==sprint)
	float m_fMaxMoveBlendRatio;
	float m_fLockedStickHeading;
	float m_fLockedHeading;
	float m_fStdPreviousDesiredHeading;

	float				m_fStickAngle;
	bool				m_bStickAngleTweaked;

#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
	bool m_bWasSprintingOnPCMouseAndKeyboard;
#endif	// FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT

#if __PLAYER_ASSISTED_MOVEMENT
	CPlayerAssistedMovementControl * m_pAssistedMovementControl;
#endif
};

//****************************************************************************
//	CTaskMoveInAir
//	A movement task which is run whilst a ped is jumping, so that player/AI
//	input can continue whilst the jump task is active.  This allows running
//	landings and rolling-landings.

class CTaskMoveInAir : public CTaskMove
{
public:
	CTaskMoveInAir();
	~CTaskMoveInAir() { }

	enum // States
	{
		Running
	};

	virtual aiTask* Copy() const { return rage_new CTaskMoveInAir(); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_IN_AIR;}

	virtual s32 GetDefaultStateAfterAbort() const { return Running; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return StateRunning_OnUpdate(CPed * pPed);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 )
	{ 
		return "State_Running"; 
	}
#endif

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const {Assertf(0, "CTaskMoveInAir : Get target called on move task with move blend ratio MOVEBLENDRATIO_STILL" ); return Vector3(0.0f, 0.0f, 0.0f);}
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const {Assertf(0, "CTaskMoveInAir : Get target radius called on move task with move blend ratio MOVEBLENDRATIO_STILL" ); return 0.0f;}

	inline bool GetWantsToUseParachute() { return m_bWantsToUseParachute; }

private:

	void ProcessAiInput(CPed* pPed);
	void ProcessPlayerInput(CPed* pPed);

	bool m_bWantsToUseParachute;
};

////////////////////////////////////////////////////////////////////////////////

#endif // TASK_PLAYER_H


