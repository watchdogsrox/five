/////////////////////////////////////////////////////////////////////////////////
// Title	:	CombatManager.h
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_TASK_COMBAT_MANAGER_H
#define INC_TASK_COMBAT_MANAGER_H

// rage includes 
#include "atl/array.h"

// game includes
#include "Peds/Ped.h"
#include "Scene/Entity.h"
#include "Scene/RegdRefTypes.h"
#include "vectormath/vec3v.h"
#include "vectormath/scalarv.h"

class CCombatMountedAttacker;
class CCombatMountedAttackerGroup;
class CCombatMeleeGroup;

//-----------------------------------------------------------------------------

class CNavMeshCircleTestHelper
{
public:
	CNavMeshCircleTestHelper();
	~CNavMeshCircleTestHelper();

	bool StartLosTest(Vec3V_In centerV, float radius, bool bTestDynamicObjects);

	// Returns the result of the LOS, false means search not yet complete.
	// bLosIsClear returns the overall status of the line-test
	bool GetTestResults(bool& bLosIsClear);

	// Resets any search currently active
	void ResetTest();
	bool IsTestActive() const { return m_bWaitingForLosResult; }

#if !__FINAL
	static void DebugDraw(Vec3V_In centerV, float radius, Color32 col);
#endif

protected:
	static int GenerateVerts(Vec3V_In centerV, float radius, Vec3V* pVertexArray, int maxVerts);

	static const int kNumSegments = 7;	// Currently can't be more than one less than MAX_LINEOFSIGHT_POINTS (8).

	TPathHandle		m_hPathHandle;
	bool			m_bWaitingForLosResult;
};

//-----------------------------------------------------------------------------

class CCombatMountedAttackerGroup
{
public:
	struct CircleData
	{
		void Reset();

		Vec4V			m_CenterAndRadius;

		atDList<CCombatMountedAttacker*>	m_AttackersUsingCircle;

		int				m_RadiusIndex;

		bool			m_FlagActive;
		bool			m_FlagTested;
		bool			m_FlagTestedFreeFromObstructions;
	};

	CCombatMountedAttackerGroup();
	~CCombatMountedAttackerGroup();

	void Init(const CEntity& tgt);
	void Shutdown();

	bool Update();

	void Insert(CCombatMountedAttacker &attacker);
	void Remove(CCombatMountedAttacker &attacker);

	void RemoveMemberFromCircles(CCombatMountedAttacker& attacker);

	bool IsActive() const				{	return m_FlagActive;	}
	const CEntity* GetTarget() const	{	return m_Target;		}
	void RequestCircleTests()			{	m_CircleTestsRequested = true;	}

#if !__FINAL
	void DebugRender() const;
#endif	// !__FINAL

protected:
	// Not implemented:
	explicit CCombatMountedAttackerGroup(const CCombatMountedAttackerGroup &);
	const CCombatMountedAttackerGroup &operator=(const CCombatMountedAttackerGroup &);

	void BehaviorUpdate(CCombatMountedAttacker &member);

	void CheckExistingCircles();
	void UpdateCircleTests();

	bool HasValidTarget() const
	{
		return m_Target != NULL;
	}

	Vec3V_Out GetTargetPos() const
	{
		return m_Target->GetTransform().GetPosition();
	}

	static const int kMaxCircles = 4;

	Vec4V								m_CircleBeingTestedCenterAndRadius;
	Vec3V								m_TargetPosForFirstCircleTest;

	CircleData							m_Circles[kMaxCircles];
	CNavMeshCircleTestHelper			m_CircleTestHelper;

	float								m_TimeSinceLastCircleJoined;
	float								m_TimeNoGood;

	int									m_LastCircleJoined;
	int									m_CurrentCircleTestRadiusIndex;
	int									m_NextCircleTestRadiusIndex;
	int									m_CircleBeingTested;

	RegdConstEnt						m_Target;
	atDList<CCombatMountedAttacker*>	m_List;
	bool								m_CircleTestsRequested;
	bool								m_FlagActive;
};

//-----------------------------------------------------------------------------

class CCombatMountedManager
{
public:
	CCombatMountedManager();
	~CCombatMountedManager();

	void Update();

	void DestroyAllAttacks();
	void DestroyAttack(CCombatMountedAttackerGroup& atk);

	CCombatMountedAttackerGroup* FindAttackerGroup(const CEntity& tgt);
	CCombatMountedAttackerGroup* FindOrCreateAttackerGroup(const CEntity& tgt);

#if __BANK
	void AddWidgets(bkBank &bank);
	bool m_BankDraw;
#endif

#if !__FINAL
	void DebugRender();
#endif	// !__FINAL

protected:
	CCombatMountedAttackerGroup* FindAvailableAttack();

	CCombatMountedAttackerGroup*	m_Attacks;
	int								m_MaxAttacks;
	int								m_NumAttacks;
};

//-----------------------------------------------------------------------------

class CCombatMountedAttacker
{
public:
	CCombatMountedAttacker();
	~CCombatMountedAttacker();

	void Init(CPed& ped)
	{	Assert(!m_Ped);
	m_Ped = &ped;
	}

	void Shutdown()
	{
		Assert(m_Ped);
		m_Ped = NULL;
	}

	CPed* GetPed() const
	{	return m_Ped;	}

	CCombatMountedAttackerGroup *GetAttackerGroup() const
	{	return m_AttackerGroup;	}

	void SetRequestCircle(bool b)
	{	m_RequestCircle = b;	}

	const CCombatMountedAttackerGroup::CircleData* GetCircle() const
	{	return m_InCircle;	}

	bool GetCircleIsNew() const
	{	return m_CircleIsNew;	}

	void SetCircleIsNew(bool b)
	{	m_CircleIsNew = b;	}

protected:
	friend class CCombatMountedAttackerGroup;

	atDNode<CCombatMountedAttacker*>					m_CircleNode;
	CCombatMountedAttackerGroup::CircleData*			m_InCircle;
	bool												m_CircleIsNew;
	bool												m_RequestCircle;

	CCombatMountedAttackerGroup*						m_AttackerGroup;
	atDNode<CCombatMountedAttacker*>					m_ListNode;
	CPed*												m_Ped;
};

//-----------------------------------------------------------------------------

class CCombatMeleeOpponent
{
	friend class CCombatMeleeGroup;

public:
	CCombatMeleeOpponent();
	~CCombatMeleeOpponent();

	void							Init				( CPed& opponentPed );
	void							Shutdown			( void );

	CCombatMeleeGroup*				GetMeleeGroup		( void ) const { return m_pMeleeGroup; }
	CPed*							GetPed				( void ) const { return m_pPed; }
	Vec3V_Out						GetPedPos			( void ) const { return m_pPed ? m_pPed->GetTransform().GetPosition() : Vec3V( V_ZERO ); }

	void							SetDistanceToTarget	( float fDistanceToTarget ) { m_fDistToTarget = fDistanceToTarget; }
	float							GetDistanceToTarget	( void ) const { return m_fDistToTarget; }

	void 							SetAngleToTarget	( float fAngleToTarget ) { m_fAngleToTaget = fAngleToTarget; }
	float							GetAngleToTarget	( void ) const { return m_fAngleToTaget; }

protected:
	CCombatMeleeGroup*				m_pMeleeGroup;
	RegdPed							m_pPed;
	float							m_fDistToTarget;
	float							m_fAngleToTaget;
};

#define MAX_NUM_OPPONENTS_PER_GROUP 10
typedef atFixedArray<CCombatMeleeOpponent*, MAX_NUM_OPPONENTS_PER_GROUP > CombatMeleeOpponentArray;

//-----------------------------------------------------------------------------

class CCombatMeleeGroup
{
public:
	enum eAllyDirection
	{
		AD_FRONT = 0,
		AD_BACK,
		AD_BACK_LEFT,
		AD_BACK_RIGHT,
	};

	CCombatMeleeGroup();
	~CCombatMeleeGroup();

	void							Init				( const CEntity& target );
	void							Shutdown			( void );

	bool							Update				( void );

	bool							Insert				( CCombatMeleeOpponent& opponent );
	void							Remove				( CCombatMeleeOpponent& opponent );

	bool							IsActive			( void ) const { return m_FlagActive; }
	const CEntity*					GetTarget			( void ) const { return m_Target; }
	int								GetNumOpponents		( void ) const { return m_aOpponents.GetCount(); }
	const CPed*						GetOpponentAtIndex	( u32 iIndex ) const
	{
		if(iIndex < m_aOpponents.GetCount())
		{
			return m_aOpponents[iIndex] ? m_aOpponents[iIndex]->GetPed() : NULL;
		}

		return NULL;
	}

	bool							HasAllyInDirection	( const CPed* pCombatPed, eAllyDirection allyDirection );
	bool							IsMeleeGroupFull	( void ) { return GetNumOpponents() >= MAX_NUM_OPPONENTS_PER_GROUP; }
	bool							IsPedInMeleeGroup	( const CPed* pCombatPed ); 
	bool							HasActiveCombatant	( void );

	u32								GetTimeTargetStartedMovingAwayFromActiveCombatant() const { return m_uTimeTargetStartedMovingAwayFromActiveCombatant; }

	static int						SortByDistance		( CCombatMeleeOpponent* const* pA1, CCombatMeleeOpponent* const* pA2);

#if !__FINAL
	void							DebugRender			( void ) const;
#endif	// !__FINAL

#if __BANK
	bool		 					ContainsOpponent( CCombatMeleeOpponent *pOpponent );
#endif // !__BANK

protected:
	// Not implemented:
	explicit CCombatMeleeGroup( const CCombatMeleeGroup & );
	const CCombatMeleeGroup &operator=( const CCombatMeleeGroup & );

	bool							IsTargetValid		( void ) const { return m_Target != NULL; }
	Vec3V_Out						GetTargetPos		( void ) const { return m_Target ? m_Target->GetTransform().GetPosition() : Vec3V( V_ZERO ); }

	bool							IsTargetMovingAwayFromPed(const CPed& rPed) const;

	const CPed*						GetTargetPed		( void ) const;

	Vec3V							m_v3Center;
	ScalarV							m_fRadius;

	RegdConstEnt					m_Target;
	CombatMeleeOpponentArray		m_aOpponents;
	u32								m_uTimeTargetStartedMovingAwayFromActiveCombatant;
	u32								m_uLastInitialPursuitTime;
	bool							m_FlagActive;
	bool							m_bHasCheckedBattleAwareness;
};

//-----------------------------------------------------------------------------

class CCombatMeleeManager
{
public:
	CCombatMeleeManager();
	~CCombatMeleeManager();

	void							Update					( void );

	void							DestroyGroup			( CCombatMeleeGroup& group );
	void							DestroyAllGroups		( void );

	CCombatMeleeGroup*				FindMeleeGroup			( const CEntity& target );
	CCombatMeleeGroup*				FindOrCreateMeleeGroup	( const CEntity& target );

	int								GetMaxGroupsCount() const { return NetworkInterface::IsGameInProgress() ? m_MaxGroupsMP : m_MaxGroups; }

	static	s32						sm_nMaxNumActiveCombatants;
	static	s32						sm_nMaxNumActiveSupportCombatants;

#if __BANK
	void							AddWidgets				( bkBank& bank );
	bool							IsMeleeOpponentInGroup(CCombatMeleeOpponent *pOpponent);
	bool							m_bDebugRender;
#endif

#if !__FINAL
	void							DebugRender				( void );
#endif	// !__FINAL

protected:
	CCombatMeleeGroup*				FindAvailableMeleeGroup	( void );
	CCombatMeleeGroup*				m_Groups;
	int								m_MaxGroups;
	int								m_MaxGroupsMP;
	int								m_NumGroups;
};

//-----------------------------------------------------------------------------


class CCombatTaskManager
{
public:

	enum CountPedsWithClearLosToTargetFlags
	{
		CPWCLTTF_MustBeOnFoot = BIT0,
	};

	enum OptionFlags
	{
		OF_MustBeLawEnforcement			= BIT0,
		OF_ReturnAfterInitialValidPed	= BIT1,
		OF_MustHaveClearLOS				= BIT2,
		OF_MustHaveGunWeaponEquipped	= BIT3,
		OF_DebugRender					= BIT4,
		OF_MustBeVehicleDriver			= BIT5,
		OF_MustBeOnFoot					= BIT6
	};

	CCombatTaskManager()
		: m_fUpdateTimer(SMALL_FLOAT)
		, m_bPedRecentlyAdded(false)
	{
		m_PedList.Reset();
	}
	~CCombatTaskManager() {}

	void Update();

	void AddPed(CPed* pPed);
	void RemovePed(CPed* pPed);
	int CountPedsInCombatWithTarget(const CPed& pTargetPed, fwFlags8 uOptionFlags = 0, const Vec3V* pvSearchCenter = NULL, const ScalarV* scSearchRadiusSq = NULL);
	int CountPedsInCombatStateWithTarget(const CPed& rTarget, s32 iState) const;
	int CountPedsWithClearLosToTarget(const CPed& rTarget, fwFlags8 uFlags = 0) const;
	CPed* GetPedInCombatWithPlayer(const CPed& pPlayerPed);

	struct sCombatPed
	{
		sCombatPed()
			: m_pPed(NULL)
			, m_fDistToTargetSq(0.0f)
		{
		}

		CPed* m_pPed;
		float m_fDistToTargetSq;
	};

	static bool SortByDistanceFromTarget(const sCombatPed& pFirstPed, const sCombatPed& pSecondPed);

	struct Tunables : public CTuning
	{
		Tunables();

		float m_fTimeBetweenUpdates;
		int m_iMaxPedsInCombatTask;

		PAR_PARSABLE;
	};

	static Tunables	sm_Tunables;

#if __BANK
	void AddWidgets(bkBank& bank);
	static bool sm_bDebugDisplayPedsInCombatWithTarget;
	static bool sm_bDebugDisplay_MustHaveClearLOS;
	static bool sm_bDebugDisplay_MustHaveGunWeaponEquipped;
#endif // __BANK

#if !__FINAL
	void DebugRender( void );
#endif	// !__FINAL

protected:

	bool FindPedsToRemove(sCombatPed* aCombatPeds, u32 iNumPedsInList, s32 &iNumPedsToRemove, bool bIgnoreLaw);
	void FindAndRemoveLawPedsMP(sCombatPed* aCombatPeds, s32 iNumPedsInList, int iMaxPeds);
	void FindAndRemoveHelisMP(sCombatPed* aCombatPeds, s32 iNumPedsInList, int iNumHelis, int iMaxHelis);
	bool HelperPedHasClearLOSToTarget(const CPed* pPed, const CEntity* pTarget) const;
	bool HelperPedHasWeaponEquipped(const CPed* pPed) const;
	int GetNumLawPedsInCombat(sCombatPed* aCombatPeds, s32 iNumPedsInList);
	int GetNumHelis(sCombatPed* aCombatPeds, s32 iNumPedsInList);


	static const int sMaxPedsInArray = 64;
	static const int sMaxPedsInCombatMP = 24;
	static const int sMaxPedsInCombatWithNearbyIncidents[WANTED_LEVEL_LAST];
	static const int sMaxHelisInCombatWithNearbyIncidents[WANTED_LEVEL_LAST];

	atFixedArray<RegdPed, sMaxPedsInArray> m_PedList;
	float m_fUpdateTimer;
	bool m_bPedRecentlyAdded;
};

//-----------------------------------------------------------------------------

class CCombatManager
{
public:
	static void						InitClass				( void );
	static void						ShutdownClass			( void );
	static CCombatManager&			GetInstance				( void ) { FastAssert( sm_Instance ); return *sm_Instance; }
	static CCombatMountedManager*	GetMountedCombatManager	( void ) { FastAssert( sm_Instance ); return &(sm_Instance->m_MountedCombatManager); }
	static CCombatMeleeManager*		GetMeleeCombatManager	( void ) { FastAssert( sm_Instance ); return &(sm_Instance->m_MeleeCombatManager); }
	static CCombatTaskManager*		GetCombatTaskManager	( void ) { FastAssert( sm_Instance ); return &(sm_Instance->m_CombatTaskManager); }

	void							Update					( void );

#if __BANK
	void							AddWidgets				( bkBank& bank );
	bool							m_bDebugRender;
#endif

#if !__FINAL
	void							DebugRender				( void );
#endif	// !__FINAL

protected:
	explicit CCombatManager();
	~CCombatManager();

	CCombatMountedManager			m_MountedCombatManager;
	CCombatMeleeManager				m_MeleeCombatManager;
	CCombatTaskManager				m_CombatTaskManager;
	static CCombatManager*			sm_Instance;
};

//-----------------------------------------------------------------------------

#endif // INC_TASK_COMBAT_MANAGER_H
