#ifndef INC_TASK_CLIMB_LADDER_H
#define INC_TASK_CLIMB_LADDER_H

// Game Headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"

////////////////////////////////////////
//		CLIMB LADDER DEBUG			  //
////////////////////////////////////////

// PURPOSE : Contains the debug functions for climb ladder tasks

#if __BANK
class CClimbLadderDebug
{
public:
	// Initialise RAG widgets
	static void InitWidgets();

	static void Draw() { DEBUG_DRAW_ONLY(ms_debugDraw.Render();) }

	static void DrawLadderInfo(CEntity* pEnt, int LadderIndex); 

	static void Debug();

#if DEBUG_DRAW
	static CDebugDrawStore ms_debugDraw;
#endif // DEBUG_DRAW

	static bool	ms_bRenderDebug;
};
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
// CTaskGoToAndClimbLadder
//////////////////////////////////////////////////////////////////////////

// PURPOSE : Makes a ped goto the specified ladder and try to climb it

// Declaration
class CTaskGoToAndClimbLadder : public CTask
{
#if __BANK
	friend class CClimbLadderDebug;
#endif // __BANK

public:

	static bank_s32 ms_iMaxSlideToLadderTime;
	static bank_float ms_fMaxTimeToWaitForNavMeshRoute;
	static float	 ms_fGettingOnOffsetAtBase;
	static float	 ms_fGettingOnOffsetAtTop;
	static float	 ms_fSlideOffset;
	static float	 ms_fOffsetFrom2dFxToCapsule;
	static bank_bool ms_bUseEasyLadderConditionsInMP;
	
	static atArray<RegdEnt> ms_Ladders; 

	// FSM states
	enum
	{
		State_Start = 0,
		State_MoveToLadderViaNavMesh,
		State_MoveToLadderViaGoToPoint,
		State_SlideToLadder,
		State_ClimbingLadder,
		State_Finish
	};

	enum eAutoClimbMode
	{
		DontAutoClimb,
		AutoClimbNormal,
		AutoClimbFast,
		AutoClimbExtraFast,	// as above, but slides down
		MaxAutoClimb
	};

	enum eClimbMode
	{
		EUndefined,
		EGetOnAtBottom,
		EGetOffAtBottom,
		EOnLadder,
		EGetOnAtTop,
		EGetOffAtTop,
		EMaxClimbMode
	};

	// AI input states
	enum InputState
	{
		InputState_Nothing,
		InputState_WantsToClimbUp,
		InputState_WantsToClimbDown,
		InputState_WantsToClimbUpFast,
		InputState_WantsToClimbDownFast,
		InputState_WantsToSlideDown,
		InputState_WantsToLetGo,
		InputState_Max
	};

	CTaskGoToAndClimbLadder(eAutoClimbMode autoClimb=DontAutoClimb);
	CTaskGoToAndClimbLadder(CEntity *pLadder, s32 ladderIndex, eAutoClimbMode autoClimb=DontAutoClimb, InputState aiAction=InputState_Nothing, bool bIsEasyEntry = false);
	~CTaskGoToAndClimbLadder();	
	
	// Task required functions
	virtual aiTask*	Copy() const { return rage_new CTaskGoToAndClimbLadder(m_pLadder, m_LadderIndex, m_eAutoClimbMode, m_eAIAction, m_bIsEasyEntry); }
	virtual int		GetTaskTypeInternal() const {return CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER;}
	// FSM required functions
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Finish; }

	// Optional functions
	virtual	void		CleanUp();
	virtual bool		ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	virtual FSM_Return	ProcessPreFSM();

	// Interface functions
	bool		IsClimbingUp();
	bool		IsClimbingDown();
	bool		IsSlidingDown();

	static void AddLadder(CEntity* pEntity); 
	static void RemoveLadder(CEntity* pEntity); 

	// Looks for a nearby ladder in the direction the ped is facing
	static CEntity* ScanForLadderToClimb(const CPed & ped,s32 &ladderIndex, Vector3& GetOnPostion, bool returnNearestLadder = false );
	static CEntity* FindLadderUsingPosHash(const CPed & ped, u32 const hash);
	static u32		GenerateHash(Vector3 const& pos, const u32 modelnameHash);

	// Examines the ladder and finds the position of the top and bottom of the ladder, worked out
	// from bounding box.  This will also take into account multiple adjoined ladder sections, and
	// return the top of the topmost section & the bottom of the lowest section.
	static bool FindTopAndBottomOfAllLadderSections(CEntity* pLadder, s32 EffectIndex, Vector3& vPositionOfTop, Vector3& vPositionOfBottom, Vector3& vNormal, bool& bCanGetOffAtTopOfLadder);
	static bool FindTopAndBottomOfSingleLadderSectionFrom2dFx(CEntity * pLadder,s32 EffectIndex, Vector3 & vOutTop, Vector3 & vOutBottom,Vector3 &vOutNormal, bool & bCanGetOffAtTopOfThisLadderSection);
	static bool GetNormalOfLadderFrom2dFx(CEntity * pLadder, Vector3 & vOutNormal, s32 EffectIndex );
	static bool IsPedOrientatedCorrectlyToLadderToGetOn(CEntity * pLadder, const CPed& ped, s32 EffectIndex, bool bIdealMountWalkDist, bool& bIsEasyEntry); 
	static bool IsGettingOnBottom(CEntity * pLadder, const CPed& ped, s32 EffectIndex, Vector3* pvBottomPos = NULL); 
	// Debug functions
#if !__FINAL
	virtual void				Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	static void					DrawNearbyLadder2dEffects(const CPed * pPed, const CEntity * pLadderToIgnore = NULL);
#endif    

	static bool CalcLadderInfo(CEntity &pLadder, s32 effectIndex,Vector3 &vBasePos,Vector3 &vTopPos,Vector3 &vNormal,bool &bCanGetOffAtTopOfThisLadderSection);
	static bool IsLadderTopBlockedMP(CEntity* pEntity, const CPed& ped, s32 ladderIndex);

protected:

	// State Functions
	FSM_Return		Start_OnUpdate							(CPed* pPed);
	
	void			MoveToLadderViaNavMesh_OnEnter			(CPed* pPed);
	FSM_Return		MoveToLadderViaNavMesh_OnUpdate			(CPed* pPed);

	void			MoveToLadderViaGoToPoint_OnEnter		(CPed* pPed);
	FSM_Return		MoveToLadderViaGoToPoint_OnUpdate		(CPed* pPed);

	void			SlideToLadder_OnEnter					(CPed* pPed);
	FSM_Return		SlideToLadder_OnUpdate					(CPed* pPed);
		
	void			ClimbingLadder_OnEnter					(CPed* pPed);
	FSM_Return		ClimbingLadder_OnUpdate					(CPed* pPed);

	const CEntity *GetLadder() const
	{
		return m_pLadder;
	}

private:

	void CalculateAIAction( );

	// Scanning for ladders is now down on the entity scan list + a list of ladders contained within buildings
	static bool IsClosestLadder(CEntity * pEntity, const CPed & ped, s32 &ladderIndex, Vec3V& getOnPosition, ScalarV& closestDistance, bool returnNearestLadder = false);
	
	bool GenerateLadderStartUpData(CPed* pPed);

	void EnsurePedIsInPositionForLadder(CPed * pPed);
	void CalcShouldGetOnForwardsOrBackwards(CPed * pPed);
	bool HandlePlayerInput(CPed* pPed);

	bool CalculatePositionsForThisLadder(const CPed* pPed);
	Vector3 CalculatePositionToGetOnLadder();
	
	bool ShouldAbortIfStuck(); 

	void OnAIActionFailed(CPed* pPed);

	eClimbMode m_eClimbMode;
	InputState m_eAIAction;

	// Variables about the ladder being climbed
	Vector3 m_vPositionOfTop;
	Vector3 m_vPositionOfBottom;
	Vector3 m_vPositionToGetOn;
	Vector3 m_vLadderNormal;
	float	m_fLadderHeading;

	Vector3 m_vSlideTargetAtTop;
	bool	m_bCanGetOffAtTop;
	bool	m_bGetOnBackwards;
	bool	m_bGotOnAtBottom;
	bool	m_bCheckedNavMeshRoute;
	// Attempt to handle ladders which have been placed incorrectly & not tested (will look messy)
	bool	m_bTopOfLadderIsTooHigh;
	bool	m_bIsEasyEntry;

	eAutoClimbMode m_eAutoClimbMode;
	Vector3 m_vPositionForPedAfterGettingOffAtTop;
	RegdEnt m_pLadder;
	s32	m_LadderIndex;

	static float ms_fClimbingClipOffsetFromCentreOfLadder;
	static float ms_fGoToPointDist;
	
	float m_fTargetHeading; 

	CTaskTimer m_getToLadderTimer;
};

//////////////////////////////////////////////////////////////////////////
// CTaskClimbLadderFully
//////////////////////////////////////////////////////////////////////////

// PURPOSE : Used by the CTaskUseLadderOnRoute in TaskNavMesh.cpp,
//			 basically a wrapper for the GoToAndClimbLadderTask which scans for
//			 a nearby ladder and climbs it in a mode related to the peds move state.

// TODO: Remove this and incorporate it into GoToAndClimbLadderTask

// Declaration
class CTaskClimbLadderFully : public CTask
{
public:

	enum
	{
		State_Start,
		State_WaitToUseLadder,
		State_GoToAndClimbLadder,
		State_Finish
	};

	CTaskClimbLadderFully(const float fMoveBlendRatio);
	~CTaskClimbLadderFully();

	virtual aiTask* Copy() const 
	{
		return rage_new CTaskClimbLadderFully(m_fMoveBlendRatio);
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_CLIMB_LADDER_FULLY; }

	// FSM required functions
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32		GetDefaultStateAfterAbort()	const { return State_Finish; }

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif    

protected:

	// State Functions
	FSM_Return		Start_OnUpdate					(CPed* pPed);

	FSM_Return		WaitToUseLadder_OnUpdate		(CPed* pPed);

	void			GoToAndClimbLadder_OnEnter		(CPed* pPed);
	FSM_Return		GoToAndClimbLadder_OnUpdate		(CPed* pPed);

private:
	float m_fMoveBlendRatio;
	s32 m_iLadderIndex; 
	RegdEnt m_pLadder;
	u32 m_BlockLadderFlag;

	struct SLadderBlock
	{
		SLadderBlock()
		{
			Reset();
		}

		void Reset() 
		{
			m_pLadder	= NULL;
			m_iLadder	= -1;
			m_TimeStamp = 0;
			m_Flags		= 0;
		}

		RegdEnt m_pLadder;
		s32		m_iLadder;
		u32		m_TimeStamp;
		u32		m_Flags;
	};

	static const int ms_nMaxBlockedLadders = 64;
	static SLadderBlock ms_pBlockedLadders[ms_nMaxBlockedLadders];
	static int ms_nBlockedLadders;

public:

	enum eBlockFlags
	{
//		BF_PLAYER = 0,	// Maybe the player should follow the same rules as the AI after all
		BF_CLIMBING_UP = BIT0,
		BF_CLIMBING_DOWN = BIT1,
	};

	static void BlockLadder(CEntity* pLadder, s32 iLadder, u32 TimeStamp, u32 Flags);
	static void ReleaseLadder(CEntity* pLadder, s32 iLadder);
	static int GetNewestBlockedLadderIndex(CEntity* pLadder, s32 iLadder);
	static int GetOldestBlockedLadderIndex(CEntity* pLadder, s32 iLadder);
	static bool IsLadderBlocked(CEntity* pLadder, s32 iLadder, u32 TimeStamp, u32 Flags);
	
	
	static bool BlockLadderMP(CEntity* pLadder, s32 iLadder, u32 Flags, s32& index);
	static bool ReleaseLadderByIndexMP(s32 index);
	static int  GetBlockedLadderIndexMP(CEntity* pLadder, s32 iLadder, u32 Flags);
	static bool IsLadderBaseOrTopBlockedByPedMP(CEntity* pLadder, s32 iLadder, u32 Flags);
	static bool IsLadderPhysicallyBlockedAtBaseMP(CPed const* pPed, CEntity* pLadder, s32 iLadder);
	static bool IsLadderNPCBlockedByPedClimbingInOppositeDirectionMP(CPed const* pPed, CEntity* pLadder, s32 iLadder, u32 const flags);

#if !__FINAL && DEBUG_DRAW
	static void DebugRenderBlockedInfo(void);
#endif /* !__FINAL && DEBUG_DRAW */
};

#endif // INC_TASK_CLIMB_LADDER_H
