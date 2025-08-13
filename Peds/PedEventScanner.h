#ifndef PED_EVENT_SCANNER_H
#define PED_EVENT_SCANNER_H

#include "ai/EntityScanner.h"
#include "ai/ExpensiveProcess.h"
#include "Event/Events.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/PedGeometryAnalyser.h"
#include "Scene/2dEffect.h"
#include "task/Response/Info/AgitatedEnums.h"
#include "Task/System/TaskHelpers.h"

namespace rage {
	class fwPtrList;
}

// Game forward declarations
class CAgitatedTrigger;
class CAgitatedTriggerReaction;
class CAgitatedTriggerSet;
class CEntity;
class CEventGroup;
class CObject;
class CPed;
class CVehicle;

//////////////////
//Ped stuck checker 
//////////////////

class CPedStuckChecker
{
public:
	CPedStuckChecker();

	bool TestPedStuck(CPed *pPed);
	u16 IsPedStuck() {return m_nStuck;}
	void ClearPedStuck() {m_fStuckCounter=0.0f; m_nStuck=0;}

	void SkipStuckCheck(u8 nSkipCheck = 2) {m_nSkipStuckCheck = nSkipCheck;}
	
	enum
	{
		PED_NOT_STUCK = 0,
		PED_STUCK_STAND_FOR_JUMP,
		PED_STUCK_JUMP_FALLOFF_SIDE
	};

private:
	Vector3 m_vecPreviousPos;
	float m_fStuckCounter;
	u16 m_nStuck;
	u8 m_nSkipStuckCheck;
};


//////////////////////////
//Ped collision scanner
//////////////////////////

class CPlayerToPedWalkIntoScanner : public CExpensiveProcess
{
public:

	static const float ms_fPedAvoidDistance;

	CPlayerToPedWalkIntoScanner() : CExpensiveProcess(PPT_PotentialCollisionScanner)
	{
		RegisterSlot();
	}
	~CPlayerToPedWalkIntoScanner()
	{
		UnregisterSlot();
	}
	
    void Scan(CPed & ped, CPed * pClosestPed);

};

///////////////////////////
//Vehicle collision scanner
///////////////////////////

class CVehiclePotentialCollisionScanner : public CExpensiveProcess
{
public:

	static const float ms_fVehiclePotentialRunOverDistance;
	static const float ms_fVehicleThreatMultiplier;
    static const float ms_fMinAvoidSpeed;
    static const float ms_fMinAvoidScaredSpeed;
    static const float ms_fSlowDiveDist;
    static const float ms_fFastDiveDist;
    
    static const float ms_fVehicleAvoidDistance;
    static dev_float ms_fMinIntersectionLength;
    
    static const int ms_iPeriod;
    
	CVehiclePotentialCollisionScanner() : CExpensiveProcess(PPT_VehicleCollisionScanner)
	{
		RegisterSlot();
	}
    ~CVehiclePotentialCollisionScanner()
	{
		UnregisterSlot();
	}

    void Scan(const CPed& ped, bool bForce = false);
    
    inline void ResetTimer()
    {
    	if(m_timer.IsSet())
    	{
    		m_timer.Set(fwTimer::GetTimeInMilliseconds(),-1);
    	}
    }
    
    inline CTaskGameTimer * GetTimer(void) { return &m_timer; }

	static float GetVehicleAvoidDistance(CVehicle * pVehicle);
  
private:

	CTaskGameTimer m_timer;  
};

//////////////////////////
//Building collision scanner
//////////////////////////

class CBuildingPotentialCollisionScanner
{
public:

	static const float ms_fLookAheadDistanceWalking;
	static const float ms_fLookAheadDistanceRunning;
	static const float ms_fNormalZThreshold;
	static const int ms_iPeriod;
	
	CBuildingPotentialCollisionScanner() {}
	~CBuildingPotentialCollisionScanner() {}

	void SetTimer(const int iTime) {m_timer.Set(fwTimer::GetTimeInMilliseconds(),iTime);}
    void ScanForBuildingPotentialCollisionEvents(const CPed& ped);

	inline CTaskGameTimer * GetTimer(void) { return &m_timer; }
	
private:
	
	CTaskGameTimer m_timer;
};


///////////////
//Cover object
//////////////

class CCoverObject
{
	Vector3 m_positions[2];
	bool	m_bUsed[2];
	int 	m_nPositions;
	
public:
	// returns the number of cover objects
	int Count() { return 0; }
	
	// returns true if this cover object is being used by someone already
	bool IsUsed(int ) { return true; }
	
	// marks this cover position as used so that others know not to use it
	void Use(int i) { m_bUsed[i] = true; }
	
	// returns the seek position to use with this cover object
	Vector3& GetCoverPosition(int i) { return m_positions[i]; }
	
	// 1 cover position for attractor - two viable ones for cars and small buildings
	int CountCoverPositions() { return m_nPositions; }
};

/////////////////
//Cover scanner
/////////////////

class CCoverScanner
{
	CCoverObject	m_coverObjects[10];
	
public:
	// returns the number of cover objects
	int Count() { return 0; }
	CCoverObject& GetCover(int i) { return m_coverObjects[i]; }
	
	// Goes through the scanners that are local to the target ped,
	// picks attractors that have the cover flag set and works out viable
	// cover positions at vehicles and small buildings relative to the viewer position.
	// Stores all this data in our array of cover positions so the task allocators
	// can use all the cover objects in the same way.
	void Scan(CPed& UNUSED_PARAM(targetPed), const Vector3& UNUSED_PARAM(viewerPosition)) { }
	
};

///////////////////////////////
//Ped acquaintance scanner
///////////////////////////////

class CPedAcquaintanceScanner : public CExpensiveProcess
{
public:

	static float ms_fThresholdDotProduct;
	static bool ms_bDoPedAcquaintanceScannerLosAsync;
	static bank_bool ms_bFixCheckLoSForSoundEvents;

   CPedAcquaintanceScanner(const bool bActivatedEverywhere=true, const bool bActivatedInVehicle=false, const bool bActivatedDuringScriptCommands=false)
    : CExpensiveProcess(PPT_AcquaintanceScanner)
	, m_uNumScanRequests(0)
	, m_pPedBeingScanned(NULL)
	, m_uNextShouldBeScanningCheckTimeMS(0)
	, m_bActivatedEverywhere(bActivatedEverywhere)
    , m_bActivatedInVehicle(bActivatedInVehicle)
    , m_bActivatedDuringScriptCommands(bActivatedDuringScriptCommands)
	, m_bWaitingOnAsyncTest(false)
    {
		RegisterSlot();
    }
    ~CPedAcquaintanceScanner()
	{
		UnregisterSlot();
	}

 	void SetActivationState
    	(const bool bActivatedEverywhere, 
    	 const bool bActivatedInVehicle, 
    	 const bool bActivatedDuringScriptCommands)
    {
	    m_bActivatedEverywhere=bActivatedEverywhere;
    	m_bActivatedInVehicle=bActivatedInVehicle;
    	m_bActivatedDuringScriptCommands=bActivatedDuringScriptCommands;
    } 

	void Scan(CPed& ped, CEntityScannerIterator& entityList);
    
	void RegisterScanRequest(CPed& rPed, int iAddTargetFlags, int iRemoveTargetFlags);

	DEV_ONLY( ECanTargetResult DebugCanPedSeePed(CPed& ped, CPed& otherPed) { return CanPedSeePed(ped, otherPed); } )

#if __BANK
	static void AddWidgets(rage::bkBank& bank);
#endif // __BANK

protected:
	// Main scanning function
	void ScanNearbyAcquaintances( CPed& ped, CEntityScannerIterator& entityList );
	// Helper functions
	ECanTargetResult CanPedSeePed( CPed& ped, CPed& otherPed );
	bool	PedHasRelationshipsToScan( const CPed &ped ) const;
	bool	PedHasResponseForAcquaintanceType( CPed& ped, CPed& otherPed, s32 iAcquaintanceScanType );
	bool	HasEventResponse(CPed& ped, CEventEditableResponse* pEvent);
	int		GetHighestAcquintanceTypeBetweenPeds( const CPed& ped, const CPed& otherPed );
	bool	PedShouldBeScanning(const CPed& ped) const;
	bool	ShouldScanThisPed( const CPed* pOtherPed, CPed& ped );

	bool AddPedToScan(CPed& ped, CPed* pOtherPed, CPed** aPedsToScan, s32* aAcquaintanceTypes, int& numPedsToScan);

	bool ArePedsInSameVehicle(const CPed& rPed, const CPed& rOtherPed) const;
	bool IsPedInFastMovingVehicle(const CPed& rPed) const;
	bool MustPedBeAbleToSeeOtherPed(const CPed& rPed, const CPed& rOtherPed) const;
	bool ShouldAddAcquaintanceEvent(const CPed& ped, const int iAcquaintanceType, const CPed& rOtherPed) const;
	bool AddAcquaintanceEvent(const CPed& ped, const int iAcquaintanceType, CPed* pAcquaintancePed);
	
	////////////////////////////
	// ScanRequests
	// PURPOSE: Ped scans can be requested from other systems (Eg: CEventFootStepHeard)
	// If we have any request, we try to attend them before we consider other peds.
	////////////////////////////

	// PURPOSE: Store a scan request
	class CScanRequest
	{
	public:
		CScanRequest() : 
			m_pPed(NULL)
		{ /* ... */	}

		void Set(CPed& rPed, s32 iAddTargetFlags, s32 iRemoveTargetFlags)
		{
			aiAssert(!IsValid());
			m_pPed = &rPed;
			m_iAddTargetFlags = iAddTargetFlags;
			m_iRemoveTargetFlags = iRemoveTargetFlags;
		}
		bool  IsValid ( ) const { return m_pPed != NULL; }
		void  Clear   ( ) { m_pPed = NULL; }

		CPed* GetPed			   ( ) const { return m_pPed; }
		s32   GetAddTargetFlags	   ( ) const { return m_iAddTargetFlags; }
		s32   GetRemoveTargetFlags ( ) const { return m_iRemoveTargetFlags; }

	private: 

		RegdPed m_pPed;
		s32 m_iAddTargetFlags;
		s32 m_iRemoveTargetFlags;
	};

	bool				IsScanRequestQueueFull  ( ) const { return m_uNumScanRequests == uMAX_SCAN_REQUESTS; }
	bool				IsScanRequestQueueEmpty ( ) const { return m_uNumScanRequests == 0; }

	const CScanRequest* GetPedScanRequest       ( const CPed& rPed ) const  { int iIdx = FindPedScanRequestIdx(rPed); return iIdx >= 0 ? &m_aScanRequests[iIdx] : NULL; }
	CScanRequest*		GetPedScanRequest       ( const CPed& rPed )	    { int iIdx = FindPedScanRequestIdx(rPed); return iIdx >= 0 ? &m_aScanRequests[iIdx] : NULL; }
	void				ClearPedScanRequest	    ( const CPed& rPed )		{ int iIdx = FindPedScanRequestIdx(rPed); if (iIdx >= 0) { ClearScanRequest(iIdx); } }
	
	int					FindPedScanRequestIdx   ( const CPed& rPed ) const;
	int					FindFreeScanRequestIdx  ( ) const;

	void				ClearAllScanRequests    ( );
	void				ClearScanRequest	    ( int iIdx ); 

	static const u32 uMAX_SCAN_REQUESTS = 3;
	atRangeArray<CScanRequest, uMAX_SCAN_REQUESTS> m_aScanRequests;
	u32 m_uNumScanRequests;
	/////////////////////

	RegdPed m_pPedBeingScanned;
	u32 m_uNextShouldBeScanningCheckTimeMS;

	bool m_bActivatedEverywhere;
	bool m_bActivatedInVehicle;
	bool m_bActivatedDuringScriptCommands;
	bool m_bWaitingOnAsyncTest;
};

///////////////////////////////
//Ped agitation scanner
///////////////////////////////

class CPedAgitationScanner : public CExpensiveProcess
{

private:

	static const int sMaxTriggers = 2;

private:

	struct Trigger
	{
		enum Flags
		{
			HasReacted	= BIT0,
		};

		enum State
		{
			None,
			Start,
			WaitBeforeReaction,
			React,
			WaitBeforeFinish,
			Finish,
		};
		
		Trigger()
		: m_hReaction()
		, m_nState(None)
		, m_fChances(0.0f)
		, m_fDistance(0.0f)
		, m_fOriginalPositionX(0.0f)
		, m_fOriginalPositionY(0.0f)
		, m_fOriginalPositionZ(0.0f)
		, m_fTimeInState(0.0f)
		, m_fTimeBeforeReaction(0.0f)
		, m_fTimeBeforeFinish(0.0f)
		, m_uFlags(0)
		{}

		Vec3V_Out GetOriginalPosition() const
		{
			return Vec3V(m_fOriginalPositionX, m_fOriginalPositionY, m_fOriginalPositionZ);
		}
		
		bool IsActive() const
		{
			return (m_hReaction.IsNotNull());
		}

		void SetOriginalPosition(Vec3V_In vPosition)
		{
			m_fOriginalPositionX = vPosition.GetXf();
			m_fOriginalPositionY = vPosition.GetYf();
			m_fOriginalPositionZ = vPosition.GetZf();
		}
		
		atHashWithStringNotFinal	m_hReaction;
		State						m_nState;
		float						m_fChances;
		float						m_fDistance;
		float						m_fOriginalPositionX;
		float						m_fOriginalPositionY;
		float						m_fOriginalPositionZ;
		float						m_fTimeInState;
		float						m_fTimeBeforeReaction;
		float						m_fTimeBeforeFinish;
		fwFlags8					m_uFlags;
	};
	
public:

	CPedAgitationScanner();
	~CPedAgitationScanner();

public:

	void Scan(CPed& ped, CEntityScannerIterator& entityList);
	
private:

	void	ActivateTrigger(const CPed& rPed, const CPed& rPlayer, const CAgitatedTrigger& rTrigger);
	void	ActivateTriggers(CPed& rPed, CPed& rPlayer);
	bool	AreExpensiveFlagsValid(const CPed& rPed, const CPed& rPlayer, const CAgitatedTriggerReaction& rReaction, bool bIsActive) const;
	bool	AreFlagsValid(const CPed& rPed, const CPed& rPlayer, const CAgitatedTriggerReaction& rReaction, bool bIsActive) const;
	float	CalculateDistanceSqBetweenPedAndVehicle(const CPed& rPed, const CVehicle& rVehicle) const;
	float	CalculateDistanceSqBetweenPeds(const CPed& rPedA, const CPed& rPedB) const;
	bool	CanActivateTrigger(const CPed& rPed, const CPed& rPlayer, const CAgitatedTrigger& rTrigger) const;
	bool	CanActivateTriggers(const CPed& rPed, const CPed& rPlayer);
	bool	CanUpdateTriggers(const CPed& rPed, const CPed& rPlayer) const;
	void	ClearAgitation(CPed& rPed, ePedConfigFlags nFlag, bool& bStorage);
	void	DeactivateTrigger(Trigger& rTrigger);
	void	DeactivateTriggers();
	int		FindFirstActiveTriggerWithType(AgitatedType nType) const;
	int		FindFirstInactiveTrigger() const;
	float	GetDistance(const CPed& rPed, const CAgitatedTrigger& rTrigger, const CAgitatedTriggerReaction& rReaction) const;
	bool	HasInactiveTrigger() const;
	bool	IsBeforeInitialReaction(const CAgitatedTriggerReaction& rReaction) const;
	bool	IsDistanceValid(const CPed& rPed, const CPed& rPlayer, const CAgitatedTrigger& rTrigger, const CAgitatedTriggerReaction& rReaction) const;
	bool	IsDistanceValid(const CPed& rPed, const CPed& rPlayer, float fDistance) const;
	bool	IsDotValid(const CPed& rPed, const CPed& rPlayer, float fMin, float fMax) const;
	bool	IsFriendlyAgitated(const CPed& rPed, const CPed& rPlayer) const;
	bool	IsPedTypeValid(const CPed& rPlayer, const CAgitatedTrigger& rTrigger) const;
	bool	IsSpeedValid(const CPed& rPed, float fMin, float fMax) const;
	bool	IsTriggerActive(const CAgitatedTrigger& rTrigger) const;
	bool	IsTriggerStillValid(const CPed& rPed, const CPed& rPlayer, const Trigger& rTrigger) const;
	bool	IsTypeValid(const CPed& rPed, AgitatedType nType) const;
	bool	ShouldIgnoreTimeSlicing(const CPed& rPed, const CPed& rPlayer) const;
	bool	ShouldPedFleePlayer(const CPed& rPed, const CPed& rPlayer) const;
	void	UpdateAgitation(CPed& rPed, CPed& rPlayer, ePedConfigFlags nFlag, AgitatedType nType, bool& bStorage) const;
	void	UpdateFlags(CPed& rPed, CPed& rPlayer);
	void	UpdateTrigger(CPed& rPed, CPed& rPlayer, Trigger& rTrigger);
	void	UpdateTriggers(CPed& rPed, CPed& rPlayer);

private:

#if __BANK
	void RenderDebug(const CPed& rPed) const;
#endif
	
private:

	atRangeArray<Trigger, sMaxTriggers>	m_aTriggers;
	bool								m_bBumped;
	bool								m_bBumpedByVehicle;
	bool								m_bDodged;
	bool								m_bDodgedVehicle;
	bool								m_bAmbientFriendBumped;
	bool								m_bAmbientFriendBumpedByVehicle;
	
};

//////////////////////////
//Ped encroachment scanner - check for peds entering our personal space
//////////////////////////

class CPedEncroachmentScanner : public CExpensiveProcess
{
public:

	static const float ms_fPedMinimumSpeedSq;
	static const float ms_fSneakyPedMinimumSpeedSq;

	CPedEncroachmentScanner() : CExpensiveProcess(PPT_EncroachmentScanner)
	, m_bCheckedForRelevance(false)
	{

	}

	~CPedEncroachmentScanner()
	{
		if ( IsRegistered() )
		{
			UnregisterSlot();
		}
	}

	void Scan(CPed & ped, CEntityScannerIterator& entityList);

private:

	bool	ShouldForceEncroachmentScanThisFrame() const;

	// PURPOSE:  Whether or not this scanner has checked if a ped's decisionmaker handles CPedEncroachmentEvent yet
	bool	m_bCheckedForRelevance;
};

//////////////////////////
//Vehicle threat scanner
//////////////////////////

class CVehicleThreats
{
public:

	enum
	{
		MAX_NUM_THREATS=4
	};
	
	CVehicleThreats()
	{
		int i;
		for(i=0;i<MAX_NUM_THREATS;i++)
		{
			m_threats[i]=-1;
		}
	}
	
	~CVehicleThreats(){}
	
	void Add(const int iThreat)
	{
		Assert(iThreat>=0);
		int i;
		for(i=0;i<MAX_NUM_THREATS;i++)
		{
			if(-1==m_threats[i])
			{
				m_threats[i]=iThreat;
				break;
			}
		}
	}
	
	bool IsThreat(const CVehicle& UNUSED_PARAM(vehicle)) const
	{
		/*
		int i;
		for(i=0;i<MAX_NUM_THREATS;i++)
		{
			if(ped.GetPedType()==m_threats[i])
			{
				return true;
			}			
		}
		*/
		return false;
	}
	
	void ClearThreats()
	{
		for(int i=0;i<MAX_NUM_THREATS;i++)
			m_threats[i]=-1;
	}

private:
	
	int m_threats[MAX_NUM_THREATS];
};

/////////////////////
//Fire scanner
/////////////////////

class CNearbyFireScanner : public CExpensiveProcess
{
public:

	static const int ms_iLatencyPeriod;
	static const float ms_fNearbyFireRange;
	static const float ms_fPotentialWalkIntoFireRange;

	enum
	{
		NUM_NEARBY_FIRES = 4
	};

	CNearbyFireScanner() : CExpensiveProcess(PPT_FireScanner)
	{
		RegisterSlot();
		m_vNearbyFires[0].Zero();
		m_iFullScanCounter = 0;
	}
	~CNearbyFireScanner()
	{
		UnregisterSlot();
	}
	
	void Scan(CPed& ped, const bool bForce=false);
	inline CTaskGameTimer * GetTimer(void) { return &m_timer; }

	const Vector3 * GetNearbyFires() const { return m_vNearbyFires; }
	void ClearNearbyFires()
	{
		m_vNearbyFires[0].Zero();
		m_iFullScanCounter = 0;
	}
	
private:

	CTaskGameTimer m_timer;
	s32 m_iFullScanCounter;

	// Cached positions and radius in w component
	// If any fire's position is (0,0,0) this signifies the end of the list
	Vector3 m_vNearbyFires[NUM_NEARBY_FIRES];
};

///////////////////////////////////
// Vehicle passenger event scanner
///////////////////////////////////

class CPassengerEventScanner : public CExpensiveProcess
{
public:

	static const int ms_iLatencyPeriod;

	CPassengerEventScanner() : CExpensiveProcess(PPT_PassengerEventScanner)
	{
		RegisterSlot();
	}
	~CPassengerEventScanner()
	{
		UnregisterSlot();
	}

	void Scan(CPed& ped);
	//inline CTaskGameTimer * GetTimer(void) { return &m_timer; }

private:

	CTaskGameTimer m_timer;

};

/////////////////////
// Scan for group events, e.g. getting in and out of cars, attacking...
/////////////////////

class CGroupEventScanner : public CExpensiveProcess
{
public:

	static const int ms_iLatencyPeriod;

	CGroupEventScanner()
		: CExpensiveProcess(PPT_GroupScanner)
	{
		RegisterSlot();
	}
	~CGroupEventScanner()
	{
		UnregisterSlot();
	}

	void Scan(CPed& ped);
	inline CTaskGameTimer * GetTimer(void) { return &m_timer; }

private:
	CTaskGameTimer m_timer;
};


class CShockingEventScanner : public CExpensiveProcess
{
public:

	static const int ms_iLatencyPeriod;

	static Vec3V ms_PlayerVehiclePosition;
	static bool ms_bLocalPlayerVehicleHornOn;

	CShockingEventScanner()
		: CExpensiveProcess(PPT_ShockingEventScanner)
	{
		RegisterSlot();
	}
	~CShockingEventScanner()
	{
		UnregisterSlot();
	}

	void ScanForShockingEvents(CPed* pPed);

	static void UpdateClass();

};

class CInWaterEventScanner
{
public:
	CInWaterEventScanner();
	~CInWaterEventScanner();
	void Scan(CPed& ped);
	void StartDrowningNow(CPed& ped); 
	bool IsDrowningInVehicle(CPed& ped); 
	bool IsDrowningOnLadder(CPed& ped); 
	bool IsTakingDamageDueToFatigue(CPed& ped);
	float GetTimeSubmerged() const { return m_fTimeSubmerged; }
	void SetTimeSubmerged(float fTimeSubmerged) { m_fTimeSubmerged = fTimeSubmerged; }
	bool IsOccupantsHeadUnderWater(CPed& ped); 
	bool CanDrown(CPed& ped); 

	float GetTimeOnSurface() const { return m_fTimeOnSurface; }

private:

	float	m_fTimeInWater;		// Total time in water (submerged or on surface)
	float	m_fTimeSubmerged;	// Time underwater
	float	m_fTimeOnSurface;	// Time on water
	float	m_fApplyDamageTimer;
};

class CPhysicsEventScanner
{
public:
	CPhysicsEventScanner()
	{

	}
	~CPhysicsEventScanner(){};
	void Scan(CPed& ped);

private:

	bool ShouldIgnoreInAirEventDueToFallTask(const CPed& rPed) const;

};


// Scans for desried movement being present but the ped being static, unable to move.
class CStaticMovementScanner
{
public:
	CStaticMovementScanner()
		: m_iStaticCounter(0)
		, m_fMillisecsCounter(0.0f)
		, m_fMillisecsWithoutCollision(0.0f)
		, m_vPedPositionAtFirstCollision(0.0f, 0.0f, 0.0f)
	{

	}
	~CStaticMovementScanner(){};

	void Scan(CPed& ped);

	void ResetStaticCounter()
	{
		m_fMillisecsCounter = 0.0f;
		m_iStaticCounter = 0;
		m_fMillisecsWithoutCollision = 0.0f;
	}
	int GetStaticCounter() const {return m_iStaticCounter;}
	static const int ms_iStaticCounterStuckLimit;
	static const int ms_iStaticCounterStuckLimitPlayerMP;
	static const int ms_iStaticActivateObjectLimit;
	static const float ms_fMillisecsPerCount;

private:
	static const float ms_fMillisecsWithoutCollisionTolerance;
	static const float ms_iStaticCountColPosToleranceSqr;

	float m_fMillisecsCounter;
	s32 m_iStaticCounter;
	float m_fMillisecsWithoutCollision;
	Vector3 m_vPedPositionAtFirstCollision;
};

//////////////////////////
//Collision event scanner
//////////////////////////

class CCollisionEventScanner
{
public:

	static const float KILL_FALL_HEIGHT;
	static const float PLAYER_KILL_FALL_HEIGHT;
	static bank_u32 RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT;

	CCollisionEventScanner() : m_bAlreadyHitByCar(false), m_CollisionEventFrame(0), m_TimeToConsiderWreckedVehicleSource(5000) {}
	~CCollisionEventScanner(){}
	
	void ScanForCollisionEvents(CPed* pPed);
	void ProcessRagdollImpact(CPed* pPed, float fMag, CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId);

	// Function for when peds are in vehicles and the vehicles hit things
	bool ProcessVehicleImpact(CPed* pPed, CEntity* pImpactEntity, float fImpactMag, const Vector3& vecImpactNormal, int nComponent, bool bForceBecauseDriverFell, s32 iSeatIndex);	
	
	void SetHitByCar() {m_bAlreadyHitByCar = true;}
	bool m_bAlreadyHitByCar;

#if __BANK
	static void AddRagdollDamageWidgets(rage::bkBank& bank);
	static void AddKnockOffVehicleWidgets(rage::bkBank& bank);
#endif // __BANK

private:
	bool ProcessBikeImpact(CPed* pPed, CEntity* pImpactEntity, float fImpactMag, const Vector3& vecImpactNormal, int nComponent, bool bForceBecauseDriverFell);
	bool ProcessQuadBikeImpact(CPed* pPed, CEntity* pImpactEntity, float fImpactMag, const Vector3& vecImpactNormal, int nComponent, bool bForceBecauseDriverFell);
	bool ProcessJetSkiImpact(CPed* pPed, CEntity* pImpactEntity, float fImpactMag, const Vector3& vecImpactNormal, int nComponent, bool bForceBecauseDriverFell);
	void ProcessBumps(CPed& rPed);

	void ComputeRelativeSpeedFromImpulse(const CPed* pPed, const CEntity* pEntity, const float fImpulseMag, const Vector3& vPedNormal, float& fRelSpeed, float& fPedEntRelVelMag, float& fPedEntTangVelMagSq);
	float ComputeDamageFromVehicle(const CPed* pPed, const CVehicle* pVehicle, const int nComponent, const float fPedEntRelVelMag, const Vector3& vPedNormal, const bool bPedStanding, const float fRelSpeed);
	CEntity* CalculateRagdollImpactDamageInstigator(const CPed* pPed, CPhysical* pImpactInflictor, const u32 uWeaponHash, bool bHasFallen) const;

	u32 m_CollisionEventFrame;		// Allows us to ignore 2 consecutive frames of collision events 
	u32 m_TimeToConsiderWreckedVehicleSource;
	
	// RAGDOLL DAMAGE PARAMETERS //////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Define the mass categories for different damage bands (minimum mass for each category).
	static bank_float RAGDOLL_DAMAGE_VEHICLE_MASS_HEAVY;
	static bank_float RAGDOLL_DAMAGE_VEHICLE_MASS_MEDIUM;
	// Define the base damage inflicted in each of the different velocity bands for vehicle damage.
	static bank_float RAGDOLL_DAMAGE_VEHICLE_CRAZY;
	static bank_float RAGDOLL_DAMAGE_VEHICLE_FAST;
	static bank_float RAGDOLL_DAMAGE_VEHICLE_MEDIUM;
	static bank_float RAGDOLL_DAMAGE_VEHICLE_SLOW;
	static bank_float RAGDOLL_DAMAGE_VEHICLE_CRAWLING;
	static bank_float RAGDOLL_SPEED_VEHICLE_CRAZY;
	static bank_float RAGDOLL_SPEED_VEHICLE_FAST;
	static bank_float RAGDOLL_SPEED_VEHICLE_MEDIUM;
	static bank_float RAGDOLL_SPEED_VEHICLE_SLOW;
	// Angle impact normal makes with horizontal to decide when to apply vehicle damage.
	static bank_float RAGDOLL_DAMAGE_VEHICLE_IMPACT_ANGLE_PARAM;
	// Dot product value to decide when a ped is under a vehicle.
	static bank_float RAGDOLL_DAMAGE_UNDER_VEHICLE_IMPACT_ANGLE_PARAM;
	// Minimum velocity of train before damage is applied.
	static bank_float RAGDOLL_DAMAGE_TRAIN_VEL_THRESHOLD;
	static bank_float RAGDOLL_DAMAGE_TRAIN_BB_ADJUST_Y;
	// Modifiers for the various damage inflictor types. This is related to the mass and is multiplied by the magnitude of the relative
	// velocity to compute a damage value.
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_TRAIN;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_PLANE;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_BIKE;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_LIGHT_VEHICLE;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_MEDIUM_VEHICLE;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_HEAVY_VEHICLE;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_UNDER_VEHICLE;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_FALL;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_FALL_LAND_FOOT;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_GENERAL_COLLISION;
	static bank_float RAGDOLL_DAMAGE_MIN_SPEED_SLIDING_DAMAGE;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_SLIDING_COLLISION;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_PED;
	// Minimum amount to fall before ped will take any fall damage.
	static bank_float RAGDOLL_DAMAGE_PED_FALL_HEIGHT_THRESHOLD;
	// Minimum relative speed at which a non-zero damage event will be created for each inflictor type.
	static bank_float RAGDOLL_DAMAGE_MIN_SPEED_FALL;
	static bank_float RAGDOLL_DAMAGE_MIN_SPEED_PED;
	static bank_float RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE;
	static bank_float RAGDOLL_DAMAGE_MIN_SPEED_WRECKED_BIKE;
	// Minimum mass of object which causes damage to ped.
	static bank_float RAGDOLL_DAMAGE_MIN_OBJECT_MASS;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_OBJECT_LIGHT;
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_OBJECT_HEAVY;
	// Scaling factors to modify various damage types when they happen to a player ped.
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_PLAYER_VEHICLE_SCALE;
	// Scaling factors to modify various damage types when they happen to an animal.
	static bank_float RAGDOLL_DAMAGE_MULTIPLIER_ANIMAL_SCALE;
	// Per-component scaling values.
	static bank_float RAGDOLL_COMPONENT_SCALES[RAGDOLL_NUM_COMPONENTS];
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// KNOCK OFF VEHICLE PARAMETERS ///////////////////////////////////////////////////////////////////////////////////////////////////////
	static bank_float sfBikeKO_PlayerMult;
	static bank_float sfBikeKO_PlayerMultMP;
	static bank_float sfBikeKO_VehicleMultPlayer;
	static bank_float sfBikeKO_VehicleMultAI;
	static bank_float sfBikeKO_TrainMult;
	static bank_float sfBikeKO_BikeMult;

	static bank_float sfBikeKO_Mag;
	static bank_float sfBikeKO_EasyMag;
	static bank_float sfBikeKO_HardMag;
	static bank_float sfPushBikeKO_Mag;
	static bank_float sfPushBikeKO_EasyMag;
	static bank_float sfPushBikeKO_HardMag;


	static bank_float sfBikeKO_HeadOnDot;

	static bank_float sfBikeKO_FrontMult;
	static bank_float sfBikeKO_FrontZMult;

	static bank_float sfBikeKO_TopMult;
	static bank_float sfBikeKO_TopUpsideDownMult;
	static bank_float sfBikeKO_TopUpsideDownMultAI;
	static bank_float sfBikeKO_BottomMult;
	static bank_float sfBikeKO_SideMult;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
};

class CPedHealthScanner
{
public:
	CPedHealthScanner();
	void Scan(CPed& ped);

private:
	CTask* FindControllingTask(CPed* pPed);

	ObjectId m_DamageEntity;
	float m_fApplyDamageTimer;
	float m_fTimeInValidHunchedTransition;
	int m_nFramesSinceLastUpdate;	// We don't need to update every frame
	bool m_bIsFallingOver;
};


///////////////
//Event scanner
///////////////

class CEventScanner
{

public:

	enum
	{
		VEHICLE_POTENTIAL_COLLISION_SCAN,
		BUILDING_POTENTIAL_COLLISION_SCAN,
		OBJECT_POTENTIAL_COLLISION_SCAN,
		PED_POTENTIAL_COLLISION_SCAN,
		ATTRACTOR_SCAN,
		PED_ACQUAINTANCE_SCAN,
		SEXY_PED_SCAN
	};

	friend class CPedIntelligence;
	
	CEventScanner();
	virtual ~CEventScanner();
	
	// Scans for both core and game specific events
	void	ScanForEvents(CPed& ped, bool bFullUpdate);

	bool ShouldInterruptCurrentTask(const CPed& rPed, const CControl& rControl);
	void CheckForTranquilizerDamage(CPed& ped);
	void CheckTasksAreCompatibleWithMotion( CPed &ped );
	void CheckAmbientFriend(CPed& ped);
	void CheckAmbientFriendDeletion(CPed& ped);
	void ScanForDoorResponses( const CPed &ped );
	void ScanForDeadPedNotRunningCorrectTasks( CPed &ped );
	// Scan for events immediately
	void	ScanForEventsNow(const CPed& ped, const int iScanType);

	// Returns the stuck checker
	CPedStuckChecker&		GetStuckChecker() { return m_stuckChecker; }
	// Returns the collision events scanner
	CCollisionEventScanner& GetCollisionEventScanner() { return m_collisionEventScanner; }
	// Returns the static movement scanner
	CStaticMovementScanner& GetStaticMovementScanner() { return m_staticMovementScanner; }
	
	CInWaterEventScanner& GetInWaterEventScanner() { return m_inWaterScanner; }

	CVehiclePotentialCollisionScanner& GetVehiclePotentialCollisionScanner() { return m_vehicleCollisionScanner; }

	CNearbyFireScanner & GetFireScanner() { return m_fireScanner; }

	CPedAcquaintanceScanner& GetAcquaintanceScanner() { return m_acquaintanceScanner; }

	void Clear();
	
	// Allows a script to stress test the asynchronous ped targeting code.
	DEV_ONLY( void DebugAsyncPedTargetting(CPed& ped, CPed& otherPed) { m_acquaintanceScanner.DebugCanPedSeePed(ped, otherPed); } )
	
	static u32 m_sDeadPedWalkingTimer;
private:
	// Scans for AI responses to walking into a vehicle
	CVehiclePotentialCollisionScanner m_vehicleCollisionScanner;
	// Scans for AI responses to hated or liked peds
    CPedAcquaintanceScanner m_acquaintanceScanner;
	// Scans for AI responses to agitation
	CPedAgitationScanner m_agitationScanner;  
	// Scans for AI responses to peds entering our personal space
	CPedEncroachmentScanner m_encroachmentScanner;
	// Scans if this ped is on fire
    CNearbyFireScanner m_fireScanner;
	// Scans for AI vehicle driver responses
	CPassengerEventScanner m_passengerEventScanner;
	// Scans for group events, followers getting in and out of vehicles etc...
	CGroupEventScanner m_groupScanner;
	// Scans for peds the player is about to walk into and gives them an AI reaction
	CPlayerToPedWalkIntoScanner m_playerToPedWalkIntoScanner;
	// Scans for drowning/in water events
	CInWaterEventScanner m_inWaterScanner;
	// Scans for physics events, i.e. being on the roof of a moving car
	CPhysicsEventScanner m_physicsEventScanner;
	// Scans for peds becoming stuck
	CPedStuckChecker m_stuckChecker;
	// Scans for collision events
	CCollisionEventScanner m_collisionEventScanner;
	// Scans for desried movement being present but the ped being static, unable to move.
	CStaticMovementScanner m_staticMovementScanner;
	// Shocking event scanner
	CShockingEventScanner m_shockingEventsScanner;
	// 
	CPedHealthScanner m_pedHealthScanner;

	fwClipSetRequestHelper m_TranqClipRequestHelper;
	u32 m_TimeTranqDamageStarted;

};


////////////////////////////////////////////////////////////////////////
//
//	CInterestingEvents
//
//	Maintains a list of interesting things happening around the player,
//	as driven by the EventHandler and the AI.  The idea is that the
//	camera-control code can then use this information to pick out things
//	to look at when the idle-cam mode is active.
//
//	Note : the word 'event' here is not equivalent to the CEvent class in
//	the ped AI - although frequently there may be a connection!
//
////////////////////////////////////////////////////////////////////////


struct TInterestingEvent {

	u32	m_eType;			// type of event
	u32	m_iStartTime;		// time at which this event was created
	RegdEnt m_pEntity;		// entity to which this event applies
	bool	m_bVisibleToCamera;	// Can this event be seen by the camera
};

#define MAX_NUM_INTERESTING_EVENTS		16//4


class CInterestingEvents {

public:

	CInterestingEvents(void);
	~CInterestingEvents(void);

	// This enumeration defines the event types only - and not the priority of each event
	enum EType {
	
		ENone,
		EPedsChatting,
		EPedSunbathing,
		EPedUsingAttractor,
		EProzzyNearby,
		ECopNearby,
		ECriminalNearby,
		EGangMemberNearby,
		ESexyCar,
		ESexyPed,
		EPlaneFlyby,
		EPedRevived,
		EEmergencyServicesArrived,
		EPanickedPed,
		EMadDriver,
		EPedRunOver,
		EPedKnockedOffBike,
		ECarCrash,
		ERoadRage,
		EFistFight,
		ECarJacking,
		EHelicopterOverhead,
		EMeleeAction,
		ESeenMeleeAction,
		EGunshotFired,
		EGangAttackingPed,
		EGangFight,
		ECopKillingCriminal,
		ESwatTeamAbseiling,
		EExplosion,
		EPedGotKilled,
		
		ENumCategories
	};

	// Get an event to look at
	const TInterestingEvent * GetInterestingEvent(bool bForCamera);
	// Mark an event as invalid - it will allow it to be replaced by a new one
	void InvalidateEvent(const TInterestingEvent * pInvalidEvent);
	// Tests all events to see if they're still visible, and invalidates those which are not
	void InvalidateNonVisibleEvents(void);

	// Whether the system is active
	inline bool IsActive(void) { return m_bIsActive; }
	inline void SetActive(bool b) { m_bIsActive = b; }

	// Whether to ignore events which occure behind the player
	inline bool GetIgnoreEventsBehindPlayer(void) { return m_bIgnoreEventsBehindPlayer; }
	inline void SetIgnoreEventsBehindPlayer(bool b) { m_bIgnoreEventsBehindPlayer = b; }
	
	// Radius within which events are generated
	inline float GetEventRadius(void) { return m_fEventRadius; }
	inline void SetEventRadius(float r) { m_fEventRadius = r; }
	
	// Add an event as a possible interesting thing to look at
	void Add(EType eType, CEntity * pEntity);

private:

	// Store of events which may be interesting to look at
	TInterestingEvent m_Events[MAX_NUM_INTERESTING_EVENTS];
	// The priority of each event type - ie. how interesting they are	
	u8 m_EventPriorities[ENumCategories];
	// Duration for which to look at each event
	u16 m_EventDurations[ENumCategories];
	// The next time that we will create an event of each type - to give some variety
	u32 m_NextTimeToAcceptEvents[ENumCategories];

	// whether to do any of this processing	
	u8 m_bIsActive								:1;
	u8 m_bIgnoreEventsBehindPlayer				:1;
	u8 m_bWaitForEventDurationToComplete			:1;
	u8 m_bUseTimeDelayBeforeAddingSimilarEvent	:1;
	
	u32 m_iCurrentFrameCounter;
	u32 m_iLastScanTime;
	static const u32 ms_iScanFrequency;
	
	// max radius for events
	float m_fEventRadius;
	Vector3 m_ScanOrigin;
	Vector3 m_ViewVec;
	
	// which event index (0..MAX_NUM_INTERESTING_EVENTS) was last returned - to prevent it getting overwritten
	s8 m_iLookingAtEvent;
};

extern CInterestingEvents g_InterestingEvents;


#endif
