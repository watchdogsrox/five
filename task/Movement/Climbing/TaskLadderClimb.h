#ifndef INC_TASK_LADDER_CLIMB_H
#define INC_TASK_LADDER_CLIMB_H

#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "fwanimation/move.h"
#include "peds/ped.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"

//-----------------------------------------------------------------------------------------
RAGE_DECLARE_CHANNEL(ladder)

#define ladderAssert(cond)						RAGE_ASSERT(ladder,cond)
#define ladderAssertf(cond,fmt,...)				RAGE_ASSERTF(ladder,cond,fmt,##__VA_ARGS__)
#define ladderFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(ladder,cond,fmt,##__VA_ARGS__)
#define ladderVerifyf(cond,fmt,...)				RAGE_VERIFYF(ladder,cond,fmt,##__VA_ARGS__)
#define ladderErrorf(fmt,...)					RAGE_ERRORF(ladder,fmt,##__VA_ARGS__)
#define ladderWarningf(fmt,...)					RAGE_WARNINGF(ladder,fmt,##__VA_ARGS__)
#define ladderDisplayf(fmt,...)					RAGE_DISPLAYF(ladder,fmt,##__VA_ARGS__)
#define ladderDebugf1(fmt,...)					RAGE_DEBUGF1(ladder,fmt,##__VA_ARGS__)
#define ladderDebugf2(fmt,...)					RAGE_DEBUGF2(ladder,fmt,##__VA_ARGS__)
#define ladderDebugf3(fmt,...)					RAGE_DEBUGF3(ladder,fmt,##__VA_ARGS__)
#define ladderLogf(severity,fmt,...)			RAGE_LOGF(ladder,severity,fmt,##__VA_ARGS__)
#define ladderCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,ladder,severity,fmt,##__VA_ARGS__)
//-----------------------------------------------------------------------------------------

class CTaskClimbLadder : public CTaskFSMClone
{
public:

	static bank_float ms_fHeightBeforeTopToFinishT;
	static bank_float ms_fHeightBeforeBottomToFinishT;

	enum ClimbingState
	{ 
		State_Initial = 0,
		State_WaitForNetworkAfterInitialising,
		State_WaitForNetworkAfterMounting,
		State_MountingAtTop,
		State_MountingAtTopRun,
		State_MountingAtBottom, 
		State_MountingAtBottomRun,
		State_ClimbingIdle, 
		State_ClimbingUp,
		State_ClimbingDown,
		State_DismountAtTop,
		State_DismountAtBottom,
		State_SlidingDown,
		State_SlideDismount, 
		State_Blocked,
		State_Finish
	};

	enum eLadderType
	{
		eSmallLadder =0,
		eStandardLadder,
		eBigLadder,
		eDrainPipe,
		eMaxLadderType
	};

	enum SearchState
	{
		GetBottomPostionOfNextSection,
		GetTopPositionOfNextSection
	};

	struct AnimInfo
	{
		const crClip* pClip; 
		float Blend; 

		AnimInfo()
		{
			pClip = NULL; 
			Blend = 0.0f; 
		}
	};

public:

	CTaskClimbLadder(CEntity* pLadder, s32 EffectIndex, const Vector3& vTop, const Vector3& vBottom, const Vector3 & vStartPos, float fHeading, bool bCanGetOffAtTop,ClimbingState eStartState, bool bGetOnBackwards, eLadderType LadderType, CTaskGoToAndClimbLadder::InputState InputState);

	CTaskClimbLadder();

	~CTaskClimbLadder ();

	// Task required functions
	virtual aiTask* Copy() const
	{
		return rage_new CTaskClimbLadder(m_pLadder, m_EffectIndex, m_vTop, m_vBottom, m_vPositionToEmbarkLadder, m_fHeading, m_bCanGetOffAtTop, m_eStartState, m_bGetOnBackwards, m_LadderType, m_eAIClimbState);
	}

	virtual int	GetTaskTypeInternal() const { return CTaskTypes::TASK_CLIMB_LADDER; }

	virtual bool ProcessMoveSignals();

    // Clone task implementation
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed));
	virtual bool                OverridesNetworkHeadingBlender(CPed *pPed) { return OverridesNetworkBlender(pPed); }
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual bool			    OverridesCollision() { return true;	}
	
	// FSM required functions
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM Optional functions
	virtual	void CleanUp();
	
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return ProcessPostFSM();

	bool ProcessPhysics(float fTimeStep, int nTimeSlice); 

	// Interface functions
	inline CTaskGoToAndClimbLadder::InputState GetAIClimbState(void) {	return m_eAIClimbState; }
	void SetInputStateFromAI(CTaskGoToAndClimbLadder::InputState eState) { m_eAIClimbState = eState; }
	
	bool IsClimbingUp() const	{ return GetState() == State_ClimbingUp; }
	bool IsClimbingDown() const { return GetState() == State_ClimbingDown; }
	bool IsSlidingDown() const	{ return GetState() == State_SlidingDown; }
	float GetLadderHeading() const { return m_fHeading; }
	bool WasGettingOnBehind() const { return m_bGetOnBehind; }
	const Vector3& GetPositionToEmbarkLadder() const { return m_vPositionToEmbarkLadder; }

	enum
	{
		NEXT_STATE_MOUNT = BIT0,			// Not used
		NEXT_STATE_WATERMOUNT = BIT1,		// 
		NEXT_STATE_CLIMBING = BIT2,			// Including mounts, the base and are minimum to mount and climb the ladder
		NEXT_STATE_DISMOUNT = BIT3,			// Not used
		NEXT_STATE_WATERDISMOUNT = BIT4,	// Not used
		NEXT_STATE_HIGHLOD = BIT5,			// Run mounts, the settles and all that "nice" stuff
	};

	static bool StreamReqResourcesIn(class CLadderClipSetRequestHelper* ClipSetRequestHelper, const CPed* pPed, int iNextState);
	static bool IsReqResourcesLoaded(const CPed* pPed, int iNextState);

	static bool LoadMetaDataCanMountBehind(CEntity* pLadderEntity, int iLadderIndex);

	// debug:
#if !__FINAL
	virtual void				Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

#if __BANK
	static void VerifyNoPedsClimbingDeletedLadder(CEntity *pLadder);
#endif

	void TaskSetState(const s32 iState);

	inline CEntity const* GetLadder(void) const { return m_pLadder;		}
	inline s32 GetEffectIndex(void) const		{ return m_EffectIndex; }

	CEntity* GetEntityBlockingMovement() const { return m_pEntityBlockingMovement; }

protected:

	// State Functions
	FSM_Return StateInitial_OnEnter(CPed* pPed); 
	FSM_Return StateInitial_OnUpdate(CPed* pPed);
	
	FSM_Return StateMounting_OnEnter(CPed* pPed); 
	FSM_Return StateMounting_OnUpdate(CPed* pPed); 
	
	FSM_Return StateClimbingIdle_OnEnter(CPed* pPed); 
	FSM_Return StateClimbingIdle_OnUpdate(CPed* pPed); 
	FSM_Return StateClimbingIdle_OnUpdate_Clone(CPed* pPed); 
	
	FSM_Return StateClimbingDown_OnEnter(CPed* pPed); 
	FSM_Return StateClimbingDown_OnUpdate(CPed* pPed);
	
	FSM_Return StateClimbingUp_OnEnter(CPed* pPed); 
	FSM_Return StateClimbingUp_OnUpdate(CPed* pPed); 

	FSM_Return StateSliding_OnEnter(CPed* pPed); 
	FSM_Return StateSliding_OnUpdate(CPed* pPed); 
	
	FSM_Return StateSlideDismount_OnEnter(CPed* pPed); 
	FSM_Return StateSlideDismount_OnUpdate(CPed* pPed); 
	
	FSM_Return StateAtTopDismount_OnEnter(); 
	FSM_Return StateAtTopDismount_OnUpdate(CPed* pPed);
	
	FSM_Return StateAtBottomDismount_OnEnter(); 
	FSM_Return StateAtBottomDismount_OnUpdate(CPed* pPed); 

	FSM_Return StateBlocked_OnEnter(CPed* pPed); 
	FSM_Return StateBlocked_OnUpdate(CPed* pPed); 

    DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private: 

	enum eOwnerPosStatus
	{
		Status_OwnerIsWithinThreshold	= 0,
		Status_OwnerIsLower				= 1,
		Status_OwnerIsHigher			= 2,
		Status_OwnerInvalid				= 3	// Used when IsOwnerZPosHigherOrLower is called on the owner...
	};

	eOwnerPosStatus IsOwnerZPosHigherOrLower(CPed* pPed) const;

	void LoadMetaDataFromLadderType(); 
	void UpdatePlayerInput(CPed* pPed);
	bool CheckForLadderSlide(CPed* pPed);
	
	bool CheckForWaterDismount(CPed* pPed, float &fWaterLevel) const;

	bool IsAtTop(const CPed* pPed) const;
	bool IsAtBottom(const CPed* pPed) const;
	float DistanceToTop(const CPed* pPed) const;

	bool IsMovementBlocked(const CPed* pPed, const Vector3& vMovementDirection, int iPreferredState = 0, bool bIgnorePeds = false, bool bForceSPDistChecks = false, bool bCheckGeo = false, bool bOldLegacyTest = false, CEntity** ppHitEntity_Out = NULL, Vec3V_Ptr pvHitPosition_Out = NULL) const;

	bool IsAlreadyClimbingOrSlidingFromMigration(CPed* pPed);
	void ChooseMigrationState(CPed* pPed);

	bool IsControlledDown(CPed* pPed); 
	bool IsControlledUp(CPed* pPed, bool bIgnorePeds = false); 			

	bool ScanForNextLadderSection(); 
	bool FindNext2DEffect(CEntity *CurrentLadder, CEntity* NextLadder);
	bool GetNextLadderSection(CEntity* CurrentLadder, CEntity* NextLadder, SearchState iSearchState, Vector3& vTarget, bool& bCanGetOff );

	void UpdateRungInfo(float fFromZToTop);
	float CalculateIdealEmbarkZ(bool bGettingOnAtTop);
	float CalculateApproachAngleBlend(bool bGettingOnAtTop ); 
	float CalculateApproachFromBehind();	// 0.0 -> 0.5 is left, 0.5 -> 1.0 is right
	float GetActivePhase(); 
	float GetActivePhaseLeft(); 
	float GetActivePhaseRight(); 

	void CalculatePerfectStart(float _StartPhase);
	void DriveTowardsPerfect(const Matrix34& _StartMat, const Matrix34& _PerfectStartMat, float _StartPhase, float _CurrentPhase, float _BlendDur, float fTimeStep);
	void SteerTowardsPosition(const Vector3& _Position, float _Strength, float _TimeStep);
	void SteerTowardsDirection(const Vector3& _Direction, float _Strength, float _TimeStep);
	void ScaleClimbToMatchRung(float fTimeStep);
	float CalcCurrentRung();
	float GetBottomZ() const;
	bool IsInterruptCondition();
	bool ShouldDismountInsteadOfSlide();
	
	float GetBaseClimbRate(void) const;

	void SetBlendsForMounting(bool bGettingOnTop);

	AnimInfo GetAnimInfo(const fwMvFloatId, const fwMvClipId clip); 
	
	Vector3 GetAnimTranslationForBlend(const crClip* pClip, float StartPhase, float EndPhase, float Blend, const Matrix34& Mat); 
	float GetAnimOrientationForBlend(const crClip* pClip, float StartPhase, float EndPhase, float Blend); 

	Vector3 CalculateTotalAnimTranslation(float StartPhase, float EndPhase, const Matrix34& Mat, bool bUseLeftRight); 
	float CalculateTotalAnimRotation(float StartPhase, float EndPhase, bool bUseLeftRight); 

	CLadderInfo* GetLadderInfo();
	Matrix34 CalculateGetOnMatrix(); 

	static bool ShouldUseFemaleClipSets(const CPed* pPed);

private:

	void UpdateCloneClimbRate(void);

	FSM_Return ProcessLadderCoordinationMP(void);

	// In MP, due to position lag by the clone by half a meter or so, the physics test in IsMovementBlocked( ) can fail when it should pass this 
	bool IsMovementUpBlockedByPedGettingOnTopOfLadderMP(void);
	
	// possibly handy in future...
	//void SwitchFromPlayerToAIControl(CTaskGoToAndClimbLadder::InputState const inputState);
	//void SwitchFromAIToPlayerControl(void);
	//bool IsPlayerUnderAIControl(void);

	void ProcessLadderBlockingMP(void);
	void BlockLadderMP(bool const blockTop, bool const blockBottom);
	void UnblockLadderMP(bool const unblockTop, bool const unblockBottom);

	//clips
	static const fwMvClipId ms_CurrentClip; 
	static const fwMvClipId ms_ClipGetOnBottomFrontHigh; 
	static const fwMvClipId ms_ClipGetOnBottomRightHigh; 
	static const fwMvClipId ms_ClipGetOnBottomLeftHigh; 
	static const fwMvClipId ms_ClipGetOnBottomFrontLow; 
	static const fwMvClipId ms_ClipGetOnBottomRightLow; 
	static const fwMvClipId ms_ClipGetOnBottomLeftLow; 

	static const fwMvClipId ms_ClipGetOnTopFront; 
	static const fwMvClipId ms_ClipGetOnTopLeft; 
	static const fwMvClipId ms_ClipGetOnTopRight; 
	static const fwMvClipId ms_ClipGetOnTopBack; 

	//flags
	static const fwMvFlagId ms_MountForwards;
	static const fwMvFlagId ms_DismountRightHand;
	static const fwMvFlagId ms_DismountWater;
	static const fwMvFlagId ms_SettleDownward;
	static const fwMvFlagId ms_LightWeight;
	static const fwMvFlagId ms_MountRun;
	static const fwMvFlagId ms_MountWater;
	static const fwMvFlagId ms_MountBottomBehind;
	static const fwMvFlagId ms_MountBottomBehindLeft;
	static const fwMvFlagId ms_MountBottomBehindRight;

	//floats
	static const fwMvFloatId ms_Speed; 
	static const fwMvFloatId ms_MountAngle; 
	static const fwMvFloatId ms_PhaseForwardHigh; 
	static const fwMvFloatId ms_PhaseForwardLow; 

	static const fwMvFloatId ms_WeightGetOnBottomFrontHigh; 
	static const fwMvFloatId ms_WeightGetOnBottomRightHigh; 
	static const fwMvFloatId ms_WeightGetOnBottomLeftHigh; 
	static const fwMvFloatId ms_WeightGetOnBottomFrontLow; 
	static const fwMvFloatId ms_WeightGetOnBottomRightLow; 
	static const fwMvFloatId ms_WeightGetOnBottomLeftLow; 

	static const fwMvFloatId ms_WeightGetOnTopFront;
	static const fwMvFloatId ms_WeightGetOnTopLeft;
	static const fwMvFloatId ms_WeightGetOnTopRight;
	static const fwMvFloatId ms_WeightGetOnTopBack;
	
	static const fwMvFloatId ms_Phase;
	static const fwMvFloatId ms_PhaseGetOnTop;
	static const fwMvFloatId ms_PhaseGetOnBottom;
	static const fwMvFloatId ms_PhaseLeft;
	static const fwMvFloatId ms_PhaseRight;
	static const fwMvFloatId ms_MountTopRunStartPhase;
	static const fwMvFloatId ms_InputPhase;
	static const fwMvFloatId ms_InputRate;
	static const fwMvFloatId ms_InputRateFast;

	//booleans
	static const fwMvBooleanId ms_MountEnded; 	
	static const fwMvBooleanId ms_LoopEnded;	
	static const fwMvBooleanId ms_ChangeToFast;	
	static const fwMvBooleanId ms_IsLooped;	
	static const fwMvBooleanId ms_IsInterrupt;
	static const fwMvBooleanId ms_IsInterruptNPC;
	static const fwMvBooleanId ms_EnableHeadIK;

	//enter state checks
	static const fwMvBooleanId ms_OnEnterClimbUp;
	static const fwMvBooleanId ms_OnEnterClimbDown;
	static const fwMvBooleanId ms_OnEnterClimbIdle;
	static const fwMvBooleanId ms_OnEnterDismountTop;
	static const fwMvBooleanId ms_OnEnterDismountBottom;
	static const fwMvBooleanId ms_OnEnterSlide;
	static const fwMvBooleanId ms_OnEnterSlideDismount;
	static const fwMvBooleanId ms_OnEnterMountTop;
	static const fwMvBooleanId ms_OnEnterMountTopRun;
	static const fwMvBooleanId ms_OnEnterMountBottom;
	static const fwMvBooleanId ms_OnEnterMountBottomRun;
	
	//Enter state requests
	static const fwMvRequestId ms_DismountTop;
	static const fwMvRequestId ms_DismountBottom;
	static const fwMvRequestId ms_ClimbUp;
	static const fwMvRequestId ms_ClimbDown;
	static const fwMvRequestId ms_StartSliding; 
	static const fwMvRequestId ms_SlideDismount; 
	static const fwMvRequestId ms_Idle;
	static const fwMvRequestId ms_MountTop;
	static const fwMvRequestId ms_MountTopRun;
	static const fwMvRequestId ms_MountBottom;
	static const fwMvRequestId ms_MountBottomRun;
	static const fwMvRequestId ms_MountBottomWater;
	static const fwMvRequestId ms_Slide;

	// state Ids
	static const fwMvStateId ms_slideState;
	static const fwMvStateId ms_climbUpState;
	static const fwMvStateId ms_climbDownState;
	static const fwMvStateId ms_idleState;
	static const fwMvStateId ms_mountBottomState;
	static const fwMvStateId ms_mountBottomStateRun;
	static const fwMvStateId ms_mountTopState;
	static const fwMvStateId ms_dismountTopState;
	static const fwMvStateId ms_dismountBottomState;
	static const fwMvStateId ms_slideDismountState;

	Matrix34 m_StartMat;
	Matrix34 m_PerfectStartMat;
	Vector3 m_LadderNormal;  
	Vector3	m_vTop;
	Vector3	m_vBottom;
	Vector3	m_vPositionToEmbarkLadder;
	Vector3 m_vBlockPos;
	float   m_fBlockHeading;
	
	// MP ladder blocking...
	float	m_fRandomIsMovementBlockedOffsetMP;
	float	m_minHeightToBlock;
	float   m_maxHeightToBlock;
	s32 m_topOfLadderBlockIndex;
	s32 m_bottomOfLadderBlockIndex;

	u32 m_ladderHash;

#if FPS_MODE_SUPPORTED
	// Mainly to give fps mode a little bit more slack
	u32 m_uTimeWhenAnimWasInterruptible;
	static const u32 sm_uTimeToAllowFPSToBreakOut;
#endif

	CMoveNetworkHelper m_NetworkHelper;
	fwMvClipSetId m_ClipSetId;
	fwMvClipSetId m_FemaleClipSetId;
	static const fwMvClipSetId ms_ClipSetIdWaterMount;
	static const fwMvClipSetId ms_ClipSetIdBase;
	static const fwMvClipSetId ms_ClipSetIdFemaleBase;

	RegdEnt m_pEntityBlockingMovement;

	RegdEnt m_pLadder;
	eLadderType m_LadderType; 
	ClimbingState m_eStartState;
	CTaskGoToAndClimbLadder::InputState	m_eAIClimbState;
	TDynamicObjectIndex m_iDynamicObjectIndex;
	s32 m_EffectIndex;
	float m_fHeading;
	float m_fGoFasterTimer;									// Set to a time when the "go faster" button is pressed and ticks down
	float m_AngleBlend; 
	float m_fSpeed;
	float m_RungSpacing;
	float m_BlendDonePhase;
	float m_WaterLevel;
	float m_fCurrentRung;
	float m_fClimbRate;										// Based on peds strength
	bool m_bCanGetOffAtTop;
	bool m_bCalledStartLadderSlide : 1;						// If we've called the ladder slide, make sure we stop audio if sliding stops
	bool m_bGetOnBackwards : 1;
	bool m_bGetOnBehind : 1;
	bool m_bGetOnBehindLeft : 1;
	bool m_bFoundAbsoluteTop : 1;
	bool m_bFoundAbsoluteBottom : 1; 
	bool m_bNetworkBlendedIn : 1; 
	bool m_bTestedValidity : 1; 
	bool m_bLooped : 1;
	bool m_bCloneAudioSlidingFlag : 1;
	bool m_bIsWaterMounting : 1;
	bool m_bIsWaterDismounting : 1;
	bool m_bCanInterruptMount : 1;
	bool m_bCanSlideDown : 1;
	bool m_bIgnoreNextRungCalc : 1;
	bool m_bOnlyBase : 1;
	bool m_bPDMatInitialized : 1;
	bool m_bDismountRightHand : 1;
	bool m_bRungCalculated : 1;								// the clone hangs on mounting until we've calculated which rung we're getting on so we get on the same rung...
	bool m_bOwnerDismountedAtTop : 1;
	bool m_bOwnerDismountedAtBottom : 1;
	bool m_ladderCheckRestart : 1;
	bool m_bOwnerTaskEnded : 1;
	bool m_bPreventFPSMode : 1;

public:

#if DEBUG_DRAW
	void DebugCloneTaskInformation(void);
#endif /* DEBUG_DRAW */
};

//-------------------------------------------------------------------------
// Task info for climb ladder
//-------------------------------------------------------------------------
class CClonedClimbLadderInfo : public CSerialisedFSMTaskInfo
{
public:

	struct InitParams
	{
		InitParams
		(
			bool								isPlayer,
			s32 								state,  
			u32									ladderPosHash,
			fwMvClipSetId 						clipSetId,
			s16 								effectIndex,
			const Vector3&						startPos,
			float 								heading,
			bool 								speed,	
			CTaskClimbLadder::ClimbingState		startState,
			CTaskClimbLadder::eLadderType		ladderType,
			CTaskGoToAndClimbLadder::InputState inputState,
			bool 								getOnBackwards,
			bool								canSlideDown,
			bool								rungCalculated,
			bool								dismountRightHand,
			float								movementBlockedOffset
		)
		:
			m_isPlayer(isPlayer),
			m_state(state),
			m_ladderPosHash(ladderPosHash),
			m_ClipSetId(clipSetId),
			m_effectIndex(effectIndex),
			m_startPos(startPos),
			m_heading(heading),
			m_speed(speed),
			m_startState(startState),
			m_ladderType(ladderType),
			m_inputState(inputState),
			m_getOnBackwards(getOnBackwards),
			m_bCanSlideDown(canSlideDown),
			m_bRungCalculated(rungCalculated),
			m_bDismountRightHand(dismountRightHand),
			m_movementBlockedOffset(movementBlockedOffset)
		{}
		
		Vector3								m_startPos;
		u32									m_ladderPosHash;
		s32 								m_state;
		fwMvClipSetId 						m_ClipSetId;
		s16 								m_effectIndex;
		float 								m_heading;
		bool 								m_speed;
		CTaskClimbLadder::ClimbingState		m_startState;
		CTaskClimbLadder::eLadderType		m_ladderType;
		CTaskGoToAndClimbLadder::InputState m_inputState;
		bool 								m_getOnBackwards;
		bool								m_bCanSlideDown;
		bool								m_bRungCalculated;
		bool								m_bDismountRightHand;
		float								m_movementBlockedOffset;
		bool								m_isPlayer;
	};

public:

    CClonedClimbLadderInfo(InitParams const& initParams)
	:
		m_isPlayer(initParams.m_isPlayer),
		m_ClipSetId(initParams.m_ClipSetId),
		m_ladderPosHash(initParams.m_ladderPosHash),
		m_effectIndex(initParams.m_effectIndex),
		m_startPos(initParams.m_startPos),
		m_heading(initParams.m_heading),
		m_speed(initParams.m_speed),
		m_startState(initParams.m_startState),
		m_ladderType(initParams.m_ladderType),
		m_inputState(initParams.m_inputState),
		m_getOnBackwards(initParams.m_getOnBackwards),
		m_bCanSlideDown(initParams.m_bCanSlideDown),
		m_rungCalculated(initParams.m_bRungCalculated),
		m_dismountRightHand(initParams.m_bDismountRightHand),
		m_movementBlockedOffset(initParams.m_movementBlockedOffset)
	{
		Assert(m_effectIndex <= 255);

		SetStatusFromMainTaskState(initParams.m_state);
	}

	CClonedClimbLadderInfo()
	:
		m_isPlayer(false),
		m_ClipSetId(CLIP_SET_ID_INVALID),
		m_ladderPosHash(0),
		m_effectIndex(-1),
		m_startPos(VEC3_ZERO),
		m_heading(0.0f),
		m_speed(false),
		m_startState(CTaskClimbLadder::State_Finish),
		m_ladderType(CTaskClimbLadder::eMaxLadderType),
		m_inputState(CTaskGoToAndClimbLadder::InputState_Max),
		m_getOnBackwards(false),
		m_bCanSlideDown(true),
		m_rungCalculated(false),
		m_dismountRightHand(false),
		m_movementBlockedOffset(0.0f)
	{}

    ~CClonedClimbLadderInfo()
	{}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_CLIMB_LADDER;}
	virtual CTaskFSMClone*	CreateCloneFSMTask();

    virtual bool								HasState() const						{ return true; }
	virtual u32									GetSizeOfState() const					{ return datBitsNeeded<CTaskClimbLadder::State_Finish>::COUNT; }
    virtual const char*							GetStatusName(u32) const				{ return "Status";}

	inline bool									GetIsPlayer() const						{ return m_isPlayer;					}
	u32											GetLadderPosHash() const				{ return m_ladderPosHash;				}
	fwMvClipSetId								GetClipSetId() const					{ return m_ClipSetId;					}
	inline s16									GetEffectIndex() const					{ return m_effectIndex;					}
	Vector3 const&								GetStartPos() const						{ return m_startPos;					}
	inline float								GetHeading() const						{ return m_heading;						}
	inline float								GetSpeed() const						{ return (float)m_speed;				}
	inline CTaskClimbLadder::ClimbingState		GetStartState() const					{ return m_startState;					}
	inline CTaskClimbLadder::eLadderType		GetLadderType() const					{ return m_ladderType;					}
	inline CTaskGoToAndClimbLadder::InputState	GetInputState() const					{ return m_inputState;					}
	inline bool									GetOnLadderBackwards() const			{ return m_getOnBackwards;				}
	inline bool									GetCanSlideDown() const					{ return m_bCanSlideDown;				}
	inline bool									GetRungCalculated() const				{ return m_rungCalculated;				}
	inline bool									GetDismountRightHand() const			{ return m_dismountRightHand;			}
	inline float								GetRandomMovementBlockedOffset() const	{ return m_movementBlockedOffset;		}

private:

	// heading is in range -PI to +PI - to save space we pack it up here by converting to 0.0 to 2PI
	inline float							PackHeading() const						{ Assert(m_heading <= PI);		Assert(m_heading >= -PI); return m_heading + PI;							}
	inline void								UnpackHeading(float serialisedVal)		{ Assert(serialisedVal >=0.0f);	Assert(serialisedVal <= (PI * 2.0f));	m_heading = (serialisedVal - PI);	}

public:

	void Serialise(CSyncDataBase& serialiser);

private:

	static const u32 SIZEOF_EFFECT_INDEX			= 8;	// CBaseModelInfo::GetNum2dEffects()
	static const u32 SIZEOF_HEADING					= 8;	// +/- PI (converted to 0.0f to (2.0f * PI))
	static const u32 SIZEOF_START_STATE				= datBitsNeeded<CTaskClimbLadder::State_Finish>::COUNT;
	static const u32 SIZEOF_LADDER_TYPE				= datBitsNeeded<CTaskClimbLadder::eMaxLadderType>::COUNT;
	static const u32 SIZEOF_INPUT_STATE				= datBitsNeeded<CTaskGoToAndClimbLadder::InputState_Max>::COUNT;
	static const u32 SIZEOF_LADDER_POS_HASH			= 32;
	static const u32 SIZEOF_MOVEMENT_BLOCKED_OFFSET = 8;

private:

    CClonedClimbLadderInfo(const CClonedClimbLadderInfo &);
    CClonedClimbLadderInfo &operator=(const CClonedClimbLadderInfo &);

	bool								m_isPlayer;
	Vector3 							m_startPos;
	fwMvClipSetId 						m_ClipSetId;
	float 								m_heading;
	u32									m_ladderPosHash;
	CTaskClimbLadder::ClimbingState		m_startState;
	CTaskClimbLadder::eLadderType		m_ladderType;
	CTaskGoToAndClimbLadder::InputState	m_inputState;
	s16	 								m_effectIndex;
	bool 								m_getOnBackwards:1;
	bool								m_speed:1;			// conversion from float at 0.0f or 1.0f value to bool for space....	
	bool								m_bCanSlideDown:1;

	bool								m_rungCalculated:1;
	bool								m_dismountRightHand:1;

	float								m_movementBlockedOffset;

	fwMvClipId			m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;
};

class CLadderClipSetRequestHelper
{
public:
	CLadderClipSetRequestHelper()
		: m_fLastClosestLadder(0.f)
	{
	}

	inline void AddClipSetRequest(const fwMvClipSetId &requestedClipSetId)
	{	
		if (!m_ClipSetRequestHelper.HaveAddedClipSet(requestedClipSetId))
			m_ClipSetRequestHelper.AddClipSetRequest(requestedClipSetId);
	}
	inline bool RequestAllClipSets(){ return m_ClipSetRequestHelper.RequestAllClipSets(); }
	inline void ReleaseAllClipSets(){ m_ClipSetRequestHelper.ReleaseAllClipSets(); }

	CMultipleClipSetRequestHelper<3> m_ClipSetRequestHelper;
	float m_fLastClosestLadder;
};

class CLadderClipSetRequestHelperPool
{
public:
	static CLadderClipSetRequestHelper* GetNextFreeHelper(CPed* pPed);

	static void ReleaseClipSetRequestHelper(CLadderClipSetRequestHelper* pLadderClipSetRequestHelper);

private:
	static const int ms_iPoolSize = 32;	// Bumped up to 32 since 16 seemed to be not enough
	static CLadderClipSetRequestHelper ms_LadderClipSetHelpers[ms_iPoolSize];
	static bool ms_LadderClipSetHelperSlotUsed[ms_iPoolSize];

	// For ease of debugging (in case we have leaks)
	static CPed* sm_AssociatedPeds[ms_iPoolSize];
};

#endif // INC_TASK_LADDER_CLIMB_H
