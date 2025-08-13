////////////////////////////////////////////////////////////////////////////////////
// Title	:	PortalTracker.h
// Author	:	John
// Started	:	23/12/05
//			:	Used to track positions of objects and rendering through interiors
////////////////////////////////////////////////////////////////////////////////////

#ifndef _PORTALTRACKER_H_
#define _PORTALTRACKER_H_
 
// Rage headers
#include "security/obfuscatedtypes.h"

// Game headers
#include "scene/portals/interiorInst.h"

class CEntity;
class CInteriorInst;
class CViewport;
class CRenderPhase;
class CRenderPhase;
class CPortalVisTracker;

namespace rage {
	class fwPortalCorners;
	class grcViewport;
	class sysStackCollector;
};

//Codes used in Network Interior Update of Clones
//Makes it easier to identify problems due to interior update bugs.
enum eNetInteriorUpdateCloneCode
{
	eNetIntUpdateNullEntity       = BIT(0)
	,eNetIntUpdateSameLocation    = BIT(1)
	,eNetIntUpdateOk              = BIT(2)
	,eNetIntUpdateAddToWorld      = BIT(3)
	,eNetIntUpdateAddToRetainList = BIT(4)
	,eNetIntUpdateNoDestIntInst   = BIT(5)
	,eNetIntUpdatePortalTracker   = BIT(6)
	,eNetIntUpdateHasDestIntInst  = BIT(7)
	,eNetIntUpdateNotInWorld      = BIT(8)
};

class CPortalTrackerBase		// make sure vptr is first
{
public:
	virtual ~CPortalTrackerBase();
};


// used to track position of an object through a portal interior 
class CPortalTracker: public CPortalTrackerBase {
public:

	enum eProbeType
	{
		PROBE_TYPE_DEFAULT  = 0,
		PROBE_TYPE_SHORT,
		PROBE_TYPE_NEAR,
		PROBE_TYPE_FAR,
		PROBE_TYPE_SUPER_FAR,
		PROBE_TYPE_STADIUM,
	};

	enum
	{
		CAPTURE_PROBE_DIST  = 1,
		SHORT_PROBE_DIST	= 2,			// minimum useful distance to probe out to
		MP_SCRIPT_EXTRA_PROBE_DIST = 2,		// extend script obj probes by this amount - only in MP! (to handle stuff on ceilings etc.)
		NEAR_PROBE_DIST		= 4,			// usual distance to probe out to
		FAR_PROBE_DIST		= 16,			// max distance to probe out to
		SUPER_FAR_PROBE_DIST = 32,
		FAR_PROBE_DIST_VIS	= 64,
		STADIUM_PROBE_DIST = 150
	};

	CEntity*				m_pOwner;
	s32						m_roomIdx;
	CInteriorProxy*			m_pInteriorProxy;

	Vector3					m_oldPos;
	Vector3					m_currPos;
#if RSG_PC
	sysObfuscated<Vector3>	m_currPosObfuscated;
#endif
	spdSphere				m_portalsSafeSphere;

	s32						m_safeInteriorPopulationScanCode;

	CPortalTracker();
	virtual ~CPortalTracker();
	static void ShutdownLevel();

    void SetLoadsCollisions(bool bLoadsCollisions);
    bool GetLoadsCollisions() { return m_bLoadsCollisions; }

	virtual void Update(const Vector3& pos);		// pass new location into the tracker & check if moved through portals

public:
	s32 GetInteriorProxyIdx(void) { if (m_pInteriorProxy) { return(m_pInteriorProxy->GetInteriorProxyPoolIdx()) ; } else { return (-1); } }
	u32 GetInteriorNameHash(void) const { if (m_pInteriorProxy) { return(m_pInteriorProxy->GetNameHash()); } else { return (0); }}
	CInteriorInst* GetInteriorInst(void) { return((m_pInteriorProxy==NULL) ? NULL : m_pInteriorProxy->GetInteriorInst()); }
	const CInteriorInst* GetInteriorInst(void) const { return((m_pInteriorProxy==NULL) ? NULL : m_pInteriorProxy->GetInteriorInst()); }

#if __DEV
	const char* GetInteriorName(void) { if (m_pInteriorProxy) { return(m_pInteriorProxy->GetModelName()); } else { return 0; }}
#endif

	u32 MoveToNewLocation(fwInteriorLocation destLoc);
	void MoveToNewLocation(CInteriorInst* pNewInterior, s32 roomIdx);

	void Teleport(const Vector3& pos) { SetCurrentPos(pos); m_oldPos = pos; } // move to location without checking portals
	void Teleport(void) { m_bUpdated = false;}		// first update after call is ignore. Pos just copied in instead.
	const Vector3& GetCurrentPos() const { return m_currPos; }
	inline void SetCurrentPos(const Vector3& pos);

  	void RequestRescanNextUpdate(void);
	void DontRequestScaneNextUpdate(void) { m_bRequestRescanNextUpdate = false;}
  	void ScanUntilProbeTrue(void);
	void StopScanUntilProbeTrue(void)	{ m_bScanUntilProbeTrue = false;}
  	void RequestCaptureForInterior(void);
	void DontRequestCaptureForInterior(void) { m_bCaptureForInterior = false; }
	void SetProbeType(eProbeType bProbeType) { m_bProbeType = bProbeType; }

	virtual void	SetOwner(CEntity* pEntity);
	CEntity*		GetOwner(void) { return(m_pOwner); }

 	void	SetInteriorAndRoom(CInteriorInst* pIntInst, s32 roomIdx);

	bool	IsInsideTunnel()					{ return(GetInteriorInst() && (m_roomIdx > 0) && GetInteriorInst()->IsSubwayMLO()); }
	bool	IsInsideInterior()					{ return ((GetInteriorInst() != NULL) && (m_roomIdx > 0));}
	bool	IsAllowedToRunInInterior() const	{ return(GetInteriorInst() && (m_roomIdx > 0) && GetInteriorInst()->IsAllowRunMLO()); }

	void	SetDontProcessUpdates(bool set) { m_bDontProcessUpdates = set; }

	bool	WasFlushedFromRetainList()	{ return(m_bFlushedFromRetainList); }
	void	SetFlushedFromRetainList(bool val)	{ m_bFlushedFromRetainList = val;}

	const char*		GetCurrRoomName(void);
	u32				GetCurrRoomHash(void);
	bool			SetCurrRoomByHash(u32 hashcode);

	void			RemoveFromInterior();

	virtual bool	IsVisPortalTracker(void){return(false);}

	void SetIsCutsceneControlled(bool bSet) { m_bIsCutsceneControlled = bSet; }
	bool GetIsCutsceneControlled() const	{ return m_bIsCutsceneControlled; }

	void SetIsReplayControlled(bool bSet)	{ m_bIsReplayControlled = bSet; }
	bool GetIsReplayControlled() const		{ return m_bIsReplayControlled; }

	void EnableProbeFallback(const fwInteriorLocation fallbackIntLoc) { m_bIsFallbackEnabled = 1; m_fallbackInteriorLocation = fallbackIntLoc; }
	bool IsProbeFallBackEnabled()	{ return (bool)m_bIsFallbackEnabled; }

	bool IsActivatingTracker() const { return m_bActivatingTracker; }

	void SetIsForcedToBeActivating(bool bSet)	{ m_bForcedToBeActivating = bSet; }
	bool GetIsForcedToBeActivating() const		{ return m_bForcedToBeActivating; }

	// static functions
	static bool	IsActivatingCollisions(CEntity* pEntity);
	static bool	AddToActivatingTrackerList(CPortalTracker* pPT);
	static void	RemoveFromActivatingTrackerList(CPortalTracker* pPT);
	static fwPtrList& GetActivatingTrackerList(void) {return(m_activatingTrackerList);}

	static bool ProbeForInterior(const Vector3& pos, CInteriorInst*& pIntInst, s32& roomIdx,  Vector3* pProbePoint, float probeLength);
	static bool ProbeForInteriorProxy(const Vector3& pos, CInteriorProxy*& pIntProxy, s32& roomIdx,  Vector3* pProbePoint, float probeLength);

	static bool ArePositionsWithinRoomDistance(const Vector3& firstPos, const Vector3& secondPos, int maxAllowedRoomDistance=2, bool bTraverseFromExteriorToInterior=true, bool bSkipDisabledPortals=true);
	static bool ArePositionsWithinRoomDistance(fwInteriorLocation firstInteriorLoc, const Vector3& secondPos, int maxAllowedRoomDistance=2, bool bTraverseFromExteriorToInterior=true, bool bSkipDisabledPortals=true);
	static bool ArePositionsWithinRoomDistance(fwInteriorLocation firstInteriorLoc, fwInteriorLocation secondInteriorLoc, int maxAllowedRoomDistance=2, bool bTraverseFromExteriorToInterior=true, bool bSkipDisabledPortals=true);

	static void ClearInteriorStateForEntity(CEntity* pEntity);

	static void	RenderTrackerDebug(void);
	
	static CPortalInst*	GetPlayerProximityPortal(void) { return(ms_pPlayerProximityPortal); }

	static CInteriorInst* GetFailedPlayerProbeDestination(void) { return(ms_failedPlayerProbeDestination); }

	static void Init(u32 initMode);
	static void ShutDown(u32 initMode);

#if __BANK
	static void DebugDraw();

	static void SetEntityTypeToTrackCallStacks(u32 entityType) { ms_entityTypeToCollectCallStacksFor = entityType; }
	static void SetTraceVisTracker(CPortalVisTracker *pTraceVisTracker) { ms_pTraceVisTracker = pTraceVisTracker; }
#endif

	void SetIgnoreHighSpeedDeltaTest(bool ignore) { m_bIgnoreHighSpeedDeltaTest = ignore; }
	void SetWaitForAllCollisionsBeforeProbe(bool bWait) { m_bWaitForAllCollisionsBeforeProbe = bWait; }
	void SetIsExteriorOnly(bool bIsExteriorOnly) { m_bIsExteriorOnly = bIsExteriorOnly; }

	static void TriggerForceAllActiveInteriorCapture();

	static bool IsPlayerInsideStiltApartment();
	static bool IsPedInsideStiltApartment(const CPed* pPed);

	static void EnableStadiumProbesThisFrame(bool bVal) {ms_bEnableStadiumProbesThisFrame = bVal;}

private:
	bool DetermineInteriorLocationFromCoord(fwInteriorLocation& loc, Vector3* pProbePoint, bool useCustomProbeDistance, float probeDistance);
	bool DetermineInteriorLocationFromCoord(fwInteriorLocation& loc, Vector3* pProbePoint) { return DetermineInteriorLocationFromCoord(loc, pProbePoint, false, 0.0f); }
	fwInteriorLocation UpdateLocationFromPortalProximity(fwInteriorLocation loc, bool* goneOutOfMap);

	void	EntryExitPortalCheck(fwPtrList& scanList, fwInteriorLocation& loc, bool &isStraddlingPortal, const Vector3& delta);

	bool Visualise(u8 red, u8 green, u8 blue);

#if __BANK
	bool	CheckConsistency();		// check portal tracker and entity data about interiors matches correctly.
	void	CollectStack();
#endif // __BANK

	fwInteriorLocation			m_fallbackInteriorLocation;

	u32	m_bActivatingTracker		: 1;
	u32	m_bLoadsCollisions			: 1;		// activates collisions for interiors when nearby
	u32	m_bUsingProbeData			: 1;

	u32	m_bRequestRescanNextUpdate	: 1;		// next time update is called, do a full rescan to determine interior status
	u32	m_bScanUntilProbeTrue		: 1;		// scan each update until the probe return a hit

	u32	m_bIsCutsceneControlled		: 1;		// don't assume frame to frame coherence. Probe every frame
	u32 m_bIsFallbackEnabled		: 1;

	u32	m_bUpdated					: 1;

	u32	m_bDontProcessUpdates		: 1;		// don't perform updates - for dynamic objects attached to portals...

	u32 m_bFlushedFromRetainList	: 1;		// set flag if this tracker is flushed out of the retain list
	u32 m_bIgnoreHighSpeedDeltaTest : 1;
	u32 m_bCaptureForInterior       : 1;
	u32 m_bProbeType				: 3;
	u32 m_bIsReplayControlled		: 1;

	u32 m_bWaitForAllCollisionsBeforeProbe	: 1;

	u32 m_bIsExteriorOnly			: 1;

	u32 m_bForcedToBeActivating		: 1;

	u32 m_padding					: 13;

#if __BANK
protected:
	u32 m_callstackTrackerTag;
private:
	u16 m_callstackCount : 15;
	u16 m_setownerCalled : 1;
#endif //__BANK

	// ---

	BANK_ONLY(static fwPtrListSingleLink	m_globalTrackerList;)		// list of all trackers (for debug display)
	static fwPtrListSingleLink	m_activatingTrackerList;	// list of trackers which keep interiors active

	static CPortalInst*				ms_pPlayerProximityPortal;

	static CInteriorInst			*ms_failedPlayerProbeDestination;

#if __BANK
	static void SprintfCallstack(size_t addr, const char *sym, size_t offset);
	static void CallstackPrintStackInfo(u32 tagCount, u32 stackCount, const size_t* stack, u32 stackSize);

	static rage::sysStackCollector *ms_pStackCollector;
	static u16						ms_lastRenderedCallstackCount;
	static CPortalTracker		   *ms_lastRenderPortalTrackerForCallstack;
	static char					   *ms_callstackText;
protected:
	static CPortalVisTracker       *ms_pTraceVisTracker;
	static s32						ms_entityTypeToCollectCallStacksFor;
#endif
protected:
	static bool ms_enableTraversalOfMulitpleLinkPortals;

	static bool ms_bRestoringFromSaveGame;

	static bool ms_bEnableStadiumProbesThisFrame;
};

// ToDo: remove visibility determination from portalTracker base class
// tracks an object through a portal interior and can determine visibility through portals
class CPortalVisTracker : public CPortalTracker {
public:
	CPortalVisTracker();
	~CPortalVisTracker();

	virtual void Update(const Vector3& pos);	// pass new location into the tracker & check if moved through portals
	void UpdateUsingTarget(const CEntity* pTarget, const Vector3& pos);

	virtual void	SetOwner(CEntity*) {Assert(false);}		// shouldn't call this for VisTrackeras

	const grcViewport *	GetGrcViewport() const				{ return m_pGrcViewport; }
	void				SetGrcViewport(const grcViewport *viewport)	{ m_pGrcViewport = viewport; }

	virtual bool	IsVisPortalTracker(void){return(true);}

	bool			ForceUpdateUsingTarget() const { return m_bForceUpdateUsingTarget; }
	void			SetForceUpdateUsingTarget(bool bVal) { m_bForceUpdateUsingTarget = bVal; }

	void			SetLastTargetEntityId(entitySelectID id) { m_LastTargetEntity = id; }
	entitySelectID	GetLastTargetEntityId() const { return m_LastTargetEntity; }

	static CInteriorProxy*	GetPrimaryInteriorProxy() { return(ms_pPrimaryIntProxy); }
	static CInteriorInst*	GetPrimaryInteriorInst() { return((ms_pPrimaryIntProxy==NULL) ? NULL : ms_pPrimaryIntProxy->GetInteriorInst()); }
	static s32				GetPrimaryRoomIdx() { return(ms_primaryRoomIdx); }

	static void StorePrimaryData(CInteriorProxy* pIntProxy, s32 roomIdx);
	static void ResetPrimaryData(void);
	static fwInteriorLocation GetInteriorLocation(void);
	static s32 GetCameraFloorId(void); 

	const grcViewport*		m_pGrcViewport;

	entitySelectID	m_LastTargetEntity;

	float			m_ExtClipUpAngle;
	float			m_ExtClipDownAngle;
	float			m_ExtClipLeftAngle;
	float			m_ExtClipRightAngle;

	float			m_ExtClipUpLimit;
	float			m_ExtClipDownLimit;
	float			m_ExtClipLeftLimit;
	float			m_ExtClipRightLimit;

	bool			m_bExtClipActive;

	bool			m_bDeterminesVisibility;	// tracker used to determine visibility within interiors
	bool			m_bForceUpdateUsingTarget;

	//when rendering out of an interior, this is the clipping frustum to clip the rest of the world against
	Matrix34			m_camInvMatrix;

	// static
	static void				CheckForResetPrimaryData(CInteriorInst*);
	static	void			RequestResetRenderPhaseTrackers(const spdSphere *pSphere);
	static	void			RequestResetRenderPhaseTrackers() { RequestResetRenderPhaseTrackers(NULL); }

	static	CInteriorProxy*	ms_pPrimaryIntProxy;
	static	s32				ms_primaryRoomIdx;
};


inline void CPortalTracker::RequestRescanNextUpdate(void) 
{
	if (!m_bScanUntilProbeTrue)
	{
		m_bRequestRescanNextUpdate = true;
		m_bCaptureForInterior = false;
		#if __BANK
			CollectStack();
		#endif
	}
}

inline void CPortalTracker::ScanUntilProbeTrue(void)
{
	m_bScanUntilProbeTrue = true; 
	m_bRequestRescanNextUpdate = false;
	m_bCaptureForInterior = false;
#if __BANK
	CollectStack();
#endif
}

inline void CPortalTracker::RequestCaptureForInterior(void) 
{ 
	if (!m_bScanUntilProbeTrue && !m_bRequestRescanNextUpdate) 
	{
		m_bCaptureForInterior = true; 
#if __BANK
		CollectStack();
#endif
	}
}

inline void CPortalTracker::SetCurrentPos(const Vector3& pos)
{
#if RSG_PC
	// First see if our position has been screwed with externally
	if(m_currPos != m_currPosObfuscated.Get())
	{
		#if __FINAL
			// Tamper action TODO
		#endif

		Assertf(false, "CPortalTracker obfuscated positions out of sync: m_currPos=(%.3f, %.3f, %.3f), m_currPosObfuscated=(%.3f, %.3f, %.3f)",
			m_currPos.x, m_currPos.y, m_currPos.z, m_currPosObfuscated.Get().x, m_currPosObfuscated.Get().y, m_currPosObfuscated.Get().z);
	}
	m_currPosObfuscated = pos;
#endif

	m_currPos = pos;
}

#endif //_PORTALTRACKER_H_
