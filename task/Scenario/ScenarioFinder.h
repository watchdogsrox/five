//
// task/Scenario/ScenarioFinder.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIO_FINDER_H
#define TASK_SCENARIO_SCENARIO_FINDER_H

#include "task/Scenario/Info/ScenarioInfoFlags.h"	// CScenarioInfoFlags
#include "task/Scenario/ScenarioPoint.h"
#include "task/System/TaskHelperFSM.h"

class C2dEffect;
class CEntity;
class CPed;
class CScenarioChainSearchResults;

//-----------------------------------------------------------------------------

class CScenarioFinder : public CTaskHelperFSM
{
public:
	// PURPOSE:	Options for which scenarios to find, which the user can set.
	struct FindOptions
	{
		explicit FindOptions();

		Vec3V							m_FleeFromPos;
		CScenarioInfoFlags::FlagsBitSet	m_RequiredScenarioInfoFlags;
		u32								m_DesiredScenarioClassId;

		// PURPOSE:	Pointer to a point that should be ignored during the search.
		CScenarioPoint* m_IgnorePoint;
		
		// PURPOSE:	scenario that should be ignored during the search.
		int								m_sIgnoreScenarioType;

		CEntity*						m_pRequiredAttachEntity;

		// PURPOSE:	If set, the only chained scenario points that will be found
		//			are the ones without incoming links.
		bool							m_NoChainingNodesWithIncomingLinks;

		// PURPOSE: If set, no chained points will be found.
		bool							m_bExcludeChainNodes;

		bool							m_bRandom;

		bool							m_bCheckBlockingAreas;

		// PURPOSE:	If true, check if the scenario is currently considered enabled based on
		//			its "spawn probability".
		bool							m_bCheckProbability;

		// PURPOSE:	If set, if scenarios require props from the environment, we check if those
		//			exist before returning anything to the user.
		bool							m_bFindPropsInEnvironment;
		
		// PURPOSE: If set, ignore the time of day constraints.
		bool							m_bIgnoreTime;

		// PURPOSE: If set, ignore the prop check when searching for a matching conditional animation.
		bool							m_bIgnoreHasPropCheck;

		// PURPOSE: If the point is behind the ped then ignore it look for one ahead of the ped
		bool							m_bIgnorePointsBehindPed;

		// PURPOSE: Biases the randomization of points towards those closer to the player
		bool							m_bBiasToNearPlayer;

		// PURPOSE: If set, dont exclude points that would be considered "NoSpawn" when doing search
		bool							m_bDontIgnoreNoSpawnPoints;

		// PURPOSE: If set, scenario must be reusable.
		bool							m_bMustBeReusable;

		// PURPOSE: If set, the scenario point must not be flagged for stationary reactions.
		bool							m_bMustNotHaveStationaryReactionsEnabled;

		// PURPOSE: If set, the approach angle to the scenario point must not be too steep.
		bool							m_bCheckApproachAngle;
	};


	enum
	{
		State_Waiting,
		State_SearchForScenario,
		State_FoundScenario,
		State_SearchForNextScenarioInChain,
	};
	
	enum InformationFlags
	{
		IF_ChainedScenarioMayBecomeValidSoon	= BIT0,
	};

	CScenarioFinder();
	virtual ~CScenarioFinder();

	void Reset();
	void StartSearch(CPed& ped, const FindOptions& opts);
	void StartSearchForNextInChain(CPed& ped, const FindOptions& opts);

	virtual FSM_Return ProcessPreFSM();

    void SetScenarioPoint(CScenarioPoint* pScenarioPoint, int realScenarioType, bool increaseUseCount = true);
    CScenarioPoint* GetScenarioPoint() const { return m_pScenarioPoint; }
	int GetRealScenarioPointType() const { return m_iRealScenarioPointType; }

	bool IsActive() const				{ return m_bIsActive; }

	int GetAction() const				{ return m_Action; }
	u8 GetNavMode() const				{ return m_NavMode; }
	int GetNavSpeed() const				{ return m_NavSpeed; }

	void SetAction(int action)			{ Assert(action >= 0 && action <= 0xff); m_Action = (u8)action; } 
	void SetNavMode(u8 iNavMode)		{ m_NavMode = iNavMode; }
	void SetNavSpeed(u8 iNavSpeed)		{ m_NavSpeed = iNavSpeed; }

	
	const FindOptions& GetFindOptions() const { return m_FindOptions; }
	
	fwFlags8 GetInformationFlags() const { return m_uInformationFlags; }

	static int PickRealScenarioTypeFromGroup(const int* realTypesToPickFrom, const float* probToPick, int numRealTypesToPickFrom, float probSum);

	static CScenarioPoint* FindNewScenario(const CPed* pPed, const Vector3& vCenterPos, float fRange, const FindOptions& opts, int& realScenarioTypeOut);

	static bool CheckCanScenarioBeUsed(CScenarioPoint& pScenarioPoint, const CPed& ped, const FindOptions& opts, int indexWithinTypeGroup, bool* pMayBecomeValidSoonOut, int edgeAction);

protected:
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual	void CleanUp();

	FSM_Return	Waiting_OnUpdate();
	void		SearchForScenario_OnEnter();
	FSM_Return	SearchForScenario_OnUpdate();
	void		FoundScenario_OnEnter();
	FSM_Return	FoundScenario_OnUpdate();
	void		SearchForNextScenarioInChain_OnEnter();
	FSM_Return	SearchForNextScenarioInChain_OnUpdate();

	// PURPOSE:	Perform some final checks before accepting a point. In particular, this function
	//			now looks for props in the world, to use with the scenario.
	static bool CheckFinalConditions(const CScenarioPoint& pt, int realScenarioType, const FindOptions& opts);

	// PURPOSE:	Make a random choice of a scenario, from an array (or rather, a set of parallel arrays) of candidates,
	//			calling CheckFinalConditions() before accepting anything.
	static CScenarioPoint* MakeRandomChoiceFromArrays(CScenarioChainSearchResults* searchResultsArray
			, u16* realScenarioTypeArray
			, int numPoints
			, int& realScenarioTypeOut
			, u8& actionOut
			, u8& navModeOut
			, u8& navSpeedOut
			, const FindOptions& opts);

    RegdScenarioPnt m_pScenarioPoint;

	static const int kMaxAttemptsToFindPropsInEnvironment = 10;

	CPed*		m_pPed;

	// PURPOSE:	Various options for how to find the scenarios.
	FindOptions	m_FindOptions;

	// PURPOSE:	The "real" scenario type of m_pScenarioPoint.
	int			m_iRealScenarioPointType;

	// PURPOSE:	Action (or type of relation) from the chaining edge leading to m_pScenarioPoint,
	//			from CScenarioChainingEdge::eAction.
	u8			m_Action;

	// PURPOSE:	Navigation mode, from the chaining edge leading to m_pScenarioPoint,
	//			from CScenarioChainingEdge::eNavMode.
	u8			m_NavMode;

	// PURPOSE:	Navigation speed, from the chaining edge leading to m_pScenarioPoint,
	//			from CScenarioChainingEdge::eNavSpeed.
	u8			m_NavSpeed;
	
	// PURPOSE: Retrieve information about the search
	fwFlags8	m_uInformationFlags;

	// PURPOSE:	True if we currently hold a use count on m_pScenarioPoint.
	bool		m_bHoldsUseCountOnScenario;

	// PURPOSE:	Whether or not we are currently active in searching for scenarios.
	bool		m_bIsActive;

	bool		m_bNewSearch;

	bool		m_bSearchForNextInChain;
};

//-----------------------------------------------------------------------------

#endif	// TASK_SCENARIO_SCENARIO_FINDER_H

// End of file 'task/Scenario/ScenarioFinder.h'
