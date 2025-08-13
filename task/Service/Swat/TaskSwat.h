#ifndef INC_TASK_SWAT_H
#define INC_TASK_SWAT_H

// FILE :    TaskSwat.h
// PURPOSE : Controlling task for swat
// CREATED : 30-06-2009

//Game headers
#include "AI/AITarget.h"
#include "game/Dispatch/DispatchHelpers.h"
#include "Peds/QueriableInterface.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/TaskFSMClone.h"

// Game forward declarations
class CHeli;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CPedGroup;
class CSwatOrder;

/////////////////////////////////
//		TASK SWAT		       //
/////////////////////////////////

// Class declaration
class CTaskSwat : public CTask
{
public:

	// FSM states
	enum 
	{ 
		State_Start = 0,
		State_EnterVehicle, 
		State_ExitVehicle,
		State_RespondToOrder,
		State_WanderOnFoot,
		State_WanderInVehicle,
		State_Finish
	};

public:

	static CPed* GetGroupLeader(const CPed& ped);
	static void  SortPedsGroup(const CPed& ped);

	CTaskSwat();
	virtual ~CTaskSwat();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskSwat(); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_SWAT; }
	virtual s32 GetDefaultStateAfterAbort() const { return GetState(); }

private:

	// State Machine Update Functions
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();

	// Local state function calls
	FSM_Return Start_OnUpdate();
	void	   EnterVehicle_OnEnter();
	FSM_Return EnterVehicle_OnUpdate();
	void	   ExitVehicle_OnEnter();
	FSM_Return ExitVehicle_OnUpdate();
	FSM_Return RespondToOrder_OnEnter();
	FSM_Return RespondToOrder_OnUpdate();
	void       RespondToOrder_OnExit();
	FSM_Return WanderOnFoot_OnEnter();
	FSM_Return WanderOnFoot_OnUpdate();
	void	   WanderOnFoot_OnExit();
	FSM_Return WanderInVehicle_OnEnter();
	FSM_Return WanderInVehicle_OnUpdate();

	void ActivateTimeslicing();
	bool ShouldWaitForGroupMembers() const;
	bool ShouldJumpOutOfVehicle() const;

	CVehicleClipRequestHelper m_VehicleClipRequestHelper;
	fwClipSetRequestHelper m_ExitToAimClipSetRequestHelper;
	fwMvClipSetId m_ExitToAimClipSetId;

public:

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////
//		TASK SWAT ORDER RESPONSE      //
////////////////////////////////////////

class CTaskSwatOrderResponse : public CTask
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		float	m_MinTimeToWander;
		float	m_MaxTimeToWander;

		PAR_PARSABLE;
	};

	// FSM states
	enum
	{
		State_Start = 0,
		State_Resume,
		State_RespondToOrder,
		State_GoToStagingArea,
		State_AdvanceToTarget,
		State_HeliResponse,
		State_CombatTarget,
		State_FindPositionToSearchOnFoot,
		State_SearchOnFoot,
		State_Wander,
		State_Finish
	};

public:

	CTaskSwatOrderResponse();
	virtual ~CTaskSwatOrderResponse();

	// Task required implementations
	virtual aiTask*				Copy() const						{ return rage_new CTaskSwatOrderResponse(); }
	virtual int					GetTaskTypeInternal() const					{ return CTaskTypes::TASK_SWAT_ORDER_RESPONSE; }
	virtual s32					GetDefaultStateAfterAbort() const	{ return State_Resume; }

private:

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	FSM_Return					Start_OnUpdate				();
	FSM_Return					Resume_OnUpdate				();
	void						RespondToOrder_OnEnter		();
	FSM_Return					RespondToOrder_OnUpdate		();
	void						GoToStagingArea_OnEnter		();
	FSM_Return					GoToStagingArea_OnUpdate	();
	void						AdvanceToTarget_OnEnter		();
	FSM_Return					AdvanceToTarget_OnUpdate	(const COrder* pOrder);
	FSM_Return					AdvanceToTarget_OnExit		(const COrder* pOrder);
	void						CombatTarget_OnEnter		();
	FSM_Return					CombatTarget_OnUpdate		();
	void						FindPositionToSearchOnFoot_OnEnter	();
	FSM_Return					FindPositionToSearchOnFoot_OnUpdate	();
	void						SearchOnFoot_OnEnter		();
	FSM_Return					SearchOnFoot_OnUpdate		();
	void						Wander_OnEnter				();
	FSM_Return					Wander_OnUpdate				();

	bool			AreTargetsWhereaboutsKnown() const;
	bool			CanSeeTarget() const;
	int				ChooseStateToResumeTo(bool& bKeepSubTask) const;
	const CEntity*	GetIncidentEntity() const;

private:

	Vec3V						m_vPositionToSearchOnFoot;
	CDispatchHelperSearchOnFoot	m_HelperForSearchOnFoot;
	RegdPed						m_pTarget;
	float						m_fTimeToWander;

	static Tunables	sm_Tunables;

public:

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL
};

///////////////////////////////////////////////////
//		    TASK_SWAT_FOLLOW_IN_LINE			 //
///////////////////////////////////////////////////

class CTaskSwatFollowInLine : public CTask
{
public:

	// FSM States
	enum
	{
		State_Start = 0,
		State_GatherFollowers,
		State_AdvanceToTarget,
		State_LeaderCrouch,
		State_Finish
	};

	// These flags have to match the enum in script in the file commands_task.sch
	enum AdvanceFlags
	{
		AF_DisableAutoCrouch	= BIT0,
		AF_DisableHandSignals	= BIT1
	};

public:

	CTaskSwatFollowInLine();
	CTaskSwatFollowInLine(const CAITarget& pTarget, float fDesiredDist, float fTimeToHoldAtEnd, fwFlags32 iFlags);
	virtual ~CTaskSwatFollowInLine();

	// Task required implementations
	virtual aiTask*				Copy() const						{ return rage_new CTaskSwatFollowInLine(m_target, m_fDesiredDist, m_fTimeToHoldAtEnd, m_iFlags); }
	virtual int					GetTaskTypeInternal() const					{ return CTaskTypes::TASK_SWAT_FOLLOW_IN_LINE; }
	virtual s32					GetDefaultStateAfterAbort() const	{ return GetState(); }

private:

	/////////////////////
	// State functions //
	/////////////////////

	// State Machine Update Functions
	virtual void				CleanUp();
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return					Start_OnUpdate				();
	void						GatherFollowers_OnEnter		(const COrder* pOrder);
	FSM_Return					GatherFollowers_OnUpdate	();
	void						AdvanceToTarget_OnEnter		(const COrder* pOrder);
	FSM_Return					AdvanceToTarget_OnUpdate	(const COrder* pOrder);
	void						LeaderCrouch_OnEnter		();
	FSM_Return					LeaderCrouch_OnUpdate		();
	void						Finish_OnEnter				();
	FSM_Return					Finish_OnUpdate				();

	void						LeaderStartAdvancing		(const COrder* pOrder, const CPedGroup* pPedsGroup, bool bIsInitialAdvance);
	bool						CheckForCorner				(const CPed* pPed);

	//////////////////////
	// Member variables //
	//////////////////////

	CClipRequestHelper			m_clipRequestHelper;
	s32							m_iDictionaryIndex;

	fwFlags32					m_iFlags;

	Vector3						m_vLastCornerCleared;

	// PURPOSE: Has multiple. For the leader it dictates both how long between crouches and how long to crouch for.
	//			For followers it controls how long to crouch after the leader as stopped crouching
	float						m_fCrouchTimer;
	float						m_fDesiredDist;
	float						m_fTimeToHoldAtEnd;

	CAITarget					m_target;

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////
//		TASK SWAT GO TO STAGING AREA		     //
///////////////////////////////////////////////////

// Class declaration
class CTaskSwatGoToStagingArea : public CTask
{
public:
	// FSM states
	enum 
	{ 
		State_Start = 0, 
		State_RespondToOrder, 
		State_GoToIncidentLocationInVehicle,
		State_GoToIncidentLocationAsPassenger,
		State_SearchInVehicle,
		State_ExitVehicle,
		State_Finish
	};

public:

	CTaskSwatGoToStagingArea();
	virtual ~CTaskSwatGoToStagingArea();

	// Task required implementations
	virtual aiTask*				Copy() const						{ return rage_new CTaskSwatGoToStagingArea(); }
	virtual int					GetTaskTypeInternal() const					{ return CTaskTypes::TASK_SWAT_GO_TO_STAGING_AREA; }
	virtual s32				GetDefaultStateAfterAbort() const	{ return GetState(); }

private:

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	FSM_Return					Start_OnUpdate				();
	FSM_Return					RespondToOrder_OnUpdate		();
	void						GoToIncidentLocationInVehicle_OnEnter	();
	FSM_Return					GoToIncidentLocationInVehicle_OnUpdate();
	void						GoToIncidentLocationAsPassenger_OnEnter	();
	FSM_Return					GoToIncidentLocationAsPassenger_OnUpdate();
	void						SearchInVehicle_OnEnter();
	FSM_Return					SearchInVehicle_OnUpdate();
	void						ExitVehicle_OnEnter				();
	FSM_Return					ExitVehicle_OnUpdate			();

private:

			bool		AreTargetsWhereaboutsKnown() const;
	const	CEntity*	GetIncidentEntity() const;
	const	CPed*		GetIncidentPed() const;
			bool		IsTargetInVehicle() const;

public:

// debug:
#if !__FINAL
	void						Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

};

/////////////////////////////////////////////////
//		TASK SWAT WANTED RESPONSE			   //
/////////////////////////////////////////////////

// Purpose: Task run when a swat gets a wanted aquaintance event

class CTaskSwatWantedResponse : public CTask
{
public:

	// Constructor
	CTaskSwatWantedResponse(CPed* pWantedPed);

	// Destructor
	virtual ~CTaskSwatWantedResponse();

	// Task required functions
	virtual aiTask*				Copy() const		{ return rage_new CTaskSwatWantedResponse(m_pWantedPed); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_SWAT_WANTED_RESPONSE; }
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Resume; }

	// Task optional functions
	virtual bool				ShouldAbort		(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual FSM_Return			ProcessPreFSM	();
	virtual void				CleanUp			();

	virtual bool		UseCustomGetup() { return true; }
	virtual bool		AddGetUpSets ( CNmBlendOutSetList& sets, CPed* pPed );
	virtual void		GetUpComplete ( eNmBlendOutSet setMatched, CNmBlendOutItem* lastItem, CTaskMove* pMoveTask, CPed* pPed );
	virtual void		RequestGetupAssets (CPed* pPed);

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	// FSM states
	enum 
	{ 
		State_Start = 0,
		State_Resume,
		State_Combat,
		State_HeliFlee,
		State_Finish
	};

	// State Functions
	FSM_Return					Start_OnUpdate			();
	FSM_Return					Resume_OnUpdate			();
	void						Combat_OnEnter			();
	FSM_Return					Combat_OnUpdate			();
	void						HeliFlee_OnEnter		();
	FSM_Return					HeliFlee_OnUpdate		();
	void						HeliFlee_OnExit			();

private:

	int			ChooseStateToResumeTo(bool& bKeepSubTask) const;
	CHeli*		GetHeli() const;

private:

	RegdPed	m_pWantedPed;

	static const u32 ms_iMaxCombatGetupHelpers = 9;

	CGetupSetRequestHelper m_GetupReqHelpers[ms_iMaxCombatGetupHelpers];
};

////////////////////////////////////////
//		TASK HELI ORDER RESPONSE      //
////////////////////////////////////////

class CTaskHeliOrderResponse : public CTask
{
public:

	// FSM states
	enum
	{
		State_Start = 0,
		State_GoToStagingAreaAsDriver,
		State_DriverFlee,
		State_PlayEntryAnim,
		State_GoToStagingAreaAsPassenger,
		State_PassengerRappel,
		State_ExitVehicle,
		State_PassengerMoveToTarget,
		State_PassengerCombatTarget,
		State_FindPositionToSearchOnFoot,
		State_SearchOnFoot,
		State_SearchWantedAreaAsDriver,
		State_Finish
	};

public:

public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_MaxTimeSpentLandedBeforeFlee;
		float m_MaxTimeAfterDropOffBeforeFlee;
		float m_MinTimeSpentLandedBeforeExit;
		float m_MaxTimeSpentLandedBeforeExit;
		float m_MaxTimeCollidingBeforeExit;
		float m_MinTimeBeforeOrderChangeDueToBlockedLocation;

		PAR_PARSABLE;
	};

public:

	CTaskHeliOrderResponse();
	virtual ~CTaskHeliOrderResponse();

public:

	virtual bool				ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// Task required implementations
	virtual aiTask*				Copy() const						{ return rage_new CTaskHeliOrderResponse(); }
	virtual int					GetTaskTypeInternal() const					{ return CTaskTypes::TASK_HELI_ORDER_RESPONSE; }
	virtual s32					GetDefaultStateAfterAbort() const	{ return GetState(); }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	static const Tunables& GetTunables() { return sm_Tunables; }

private:

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	void						Start_OnEnter						();
	FSM_Return					Start_OnUpdate						();
	void						GoToStagingAreaAsDriver_OnEnter		();
	FSM_Return					GoToStagingAreaAsDriver_OnUpdate	();
	void						GoToStagingAreaAsDriver_OnExit		();
	void						DriverFlee_OnEnter					();
	FSM_Return					DriverFlee_OnUpdate					();
	void						DriverFlee_OnExit					();
	void						PlayEntryAnim_OnEnter				();
	FSM_Return					PlayEntryAnim_OnUpdate				();
	void						GoToStagingAreaAsPassenger_OnEnter	();
	FSM_Return					GoToStagingAreaAsPassenger_OnUpdate	();
	void						PassengerRappel_OnEnter				();
	FSM_Return					PassengerRappel_OnUpdate			();
	void						ExitVehicle_OnEnter					();
	FSM_Return					ExitVehicle_OnUpdate				();
	void						PassengerMoveToTarget_OnEnter		();
	FSM_Return					PassengerMoveToTarget_OnUpdate		();
	void						PassengerCombatTarget_OnEnter		();
	FSM_Return					PassengerCombatTarget_OnUpdate		();
	void						FindPositionToSearchOnFoot_OnEnter	();
	FSM_Return					FindPositionToSearchOnFoot_OnUpdate	();
	void						SearchOnFoot_OnEnter				();
	FSM_Return					SearchOnFoot_OnUpdate				();
	void						SearchWantedAreaAsDriver_OnEnter	();
	FSM_Return					SearchWantedAreaAsDriver_OnUpdate	();

private:

			void		ActivateTimeslicing();
			bool		AreTargetsWhereaboutsKnown() const;
			bool		CanSeeTarget() const;
			CHeli*		GetHeli() const;
	const	CEntity*	GetIncidentEntity() const;
			int			GetSwatOrderType() const;
			bool		HasHeliLanded() const;
			bool		HasPedsWaitingToBeDroppedOff() const;
			bool		IsWaitingToBeDroppedOff() const;
			bool		ArePedsInOrAttachedToHeli(CHeli* pHeli);
	const	CSwatOrder*	GetHeliDriverSwatOrder();

private:

	static int	GetSwatOrderTypeForPed(const CPed& rPed);
	static bool	IsPedWaitingToBeDroppedOff(const CPed& rPed);

private:
	
	void						FindTargetPosition(CPed* pPed, const COrder* pOrder, Vector3& vTargetPosition, bool bIsSearch);
	void						CheckForLocationUpdate(CPed* pPed);
	bool						HasRappellingPeds() const;

	Vec3V						m_vPositionToSearchOnFoot;
	CDispatchHelperSearchOnFoot	m_HelperForSearchOnFoot;
	fwClipSetRequestHelper		m_clipSetRequestHelper;
	fwClipSetRequestHelper		m_aimIntroClipRequestHelper;
	RegdPed						m_pTarget;			// This is our target ped
	Vector3						m_GoToLocation;		// This is the location we want to fly to, calculated per state change
	float						m_fTimeAfterDropOff;
	float						m_fTimeOrderChangeHasBeenValid;
	bool						m_bOverrideRopeLength;

private:

	static Tunables	sm_Tunables;

};

#endif // INC_TASK_SWAT_H
