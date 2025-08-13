#ifndef PED_GEOMETRY_ANALYSER
#define PED_GEOMETRY_ANALYSER

// Rage headers
#include "spatialdata/sphere.h"

// Framework headers

// Game headers
#include "Control/Route.h"
#include "ModelInfo/PedModelInfo.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/HierarchyIds.h"

class CEntity;
class CPhysical;
class CPed;
class CObject;
class CPedGroup;
class CVehicle;
class CWorld;
class CPointRoute;
class C2dEffect;
class CTask;
class CTaskSimple;
class CTaskSimpleMoveSwim;
class CTaskCrouch;
class CTaskSimpleInAir;
class CTaskSimpleGoTo;
class CEvent;
class CEventGroup;
class CEventHandler;
class CTaskManager;
class CPedDebugVisualiser;
class CPointRoute;
class CWorld;

namespace rage
{
	class phBound;
	class fwPtrList;
};

#include "Vector/Vector3.h"


extern int gs_iNumWorldProcessLineOfSightUnCached;
inline void ResetDebugPedAICounters(void){gs_iNumWorldProcessLineOfSightUnCached=0;}
void IncPedAiLosCounter(void);

#if !__FINAL
#define INC_PEDAI_LOS_COUNTER {IncPedAiLosCounter();}	
#else
#define INC_PEDAI_LOS_COUNTER {void(0);}	
#endif


/////////////////////////
//Entity bound for ai
//Computes bounding rectangle round an entity with padding to account for ped radius.
//Bound can be queried with points and lines.  This class is useful to work out if a ped
//will collide with an entity, which side of an entity a ped is situated, etc.
/////////////////////////

class CEntityBoundAI
{
public:

	static const u32 ms_iMaxNumBoxes;
	static const float ms_fPedHalfHeight;

	enum
    {
        FRONT_LEFT	= 0,
        REAR_LEFT,
        REAR_RIGHT,
        FRONT_RIGHT
    };
    
    enum Direction
    {
        FRONT		= 0,
        LEFT,
        REAR,
        RIGHT,
        UP,
        DOWN
    };

	static const u32 ms_iSideFlags[4];
	static const u32 ms_iCornerFlags[4];

	// If the input iCornerBitset equals one of the combination of edge-flags which indicate a corner
	// then the corner index is returned.  Otherwise this function will return -1;
	static inline s32 GetCornerIndexFromBitset(const u32 iCornerBitset)
	{
		for(s32 i=0; i<4; i++)
			if(iCornerBitset == ms_iCornerFlags[i])
				return i;
		return -1;
	}
	// If the input iSideBitset equals one of the edge bit-flags, then the edge index is returned.
	// Otherwise this function will return -1;
	static inline s32 GetSideIndexFromBitset(const u32 iSideBitset)
	{
		for(s32 i=0; i<4; i++)
			if(iSideBitset == ms_iSideFlags[i])
				return i;
		return -1;
	}

	CEntityBoundAI(){}
	CEntityBoundAI(const CEntity & entity, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight=false, const Matrix34* pOptionalOverrideMatrix = NULL, const s32 iNumSubBoundsToIgnore = 0, const s32 * iSubBoundIndicesToIgnore = NULL, const s32 iNumSubBoundsToUseExclusively = 0, const s32 * iSubBoundIndicesToUseExclusively = NULL, const float fPedHeight=ms_fPedHalfHeight);
	CEntityBoundAI(const Vector3 & vEntityMin, const Vector3 & vEntityMax, const Matrix34 & matOrientation, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight);
	CEntityBoundAI(const Vector3 & vEntityMin, const Vector3 & vEntityMax, const Matrix34 & matOrientation, const float& fZPlane, const Vector3& vExpandBox, const bool bCalculateTopAndBottomHeight);
	~CEntityBoundAI(){}

	// TODO: Not sure what the purpose is of having a public Set() function which just calls the private Init(); why not copy the code from Init() into Set() and avoid a function call?
	void Set(const CEntity& entity, const float fZPlane, const float fPedRadius, const bool bCalculateTopAndBottomHeight, const Matrix34 * pOptionalOverrideMatrix, const s32 iNumSubBoundsToIgnore = 0, const s32 * iSubBoundIndicesToIgnore = NULL, const s32 iNumSubBoundsToUseExclusively = 0, const s32 * iSubBoundIndicesToUseExclusively = NULL, const float fPedHeight=ms_fPedHalfHeight)
	{
		Init(entity,fZPlane,fPedRadius,bCalculateTopAndBottomHeight, pOptionalOverrideMatrix, iNumSubBoundsToIgnore, iSubBoundIndicesToIgnore, iNumSubBoundsToUseExclusively, iSubBoundIndicesToUseExclusively, fPedHeight);
	}
	bool Set(const CCarDoor & carDoor, const CVehicle * pParent, const float fZPlane, const float fPedRadius, const bool bCalculateTopAndBottomHeight)
	{
		return Init(carDoor, pParent, fZPlane, fPedRadius, bCalculateTopAndBottomHeight);
	}
	void Set(const Vector3 & vEntityMin, const Vector3 & vEntityMax, const Matrix34 & matOrientation, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight)
	{
		Init(vEntityMin, vEntityMax, matOrientation, fZPlane, fPedRadius,  bCalculateTopAndBottomHeight);
	}

	void GetCentre(Vector3& vCentre) const {vCentre=m_centre;}
	void GetCorners(Vector3* corners) const;
	void GetSphere(spdSphere& sphere) const;
	void GetPlanes(Vector3* normals, float* ds) const;
	void GetPlanes(Vector4* planes) const;
	void GetSegmentPlanes(Vector3* normals, float* ds) const;

	// ComputeCorners() *must* have been called before trying to retrieve the top/bottom Z for this bound
	inline float GetTopZ(void) const { return m_fTopZ; }
	inline float GetBottomZ(void) const { return m_fBottomZ; }

	inline ScalarV_Out GetTopZV() const { return LoadScalar32IntoScalarV(m_fTopZ); }
	inline ScalarV_Out GetBottomZV() const { return LoadScalar32IntoScalarV(m_fBottomZ); }

	int ComputeHitSideByPosition(const Vector3& vPos) const;
	int ComputeHitSideByVelocity(const Vector3& vVel) const;

	// Returns the bitset of 'eSideFlags'
	int ComputePlaneFlags(const Vector3& vPos) const;

	bool LiesInside(const Vector3& vPos, const float fEps=0.0f) const;
	bool LiesInside(const spdSphere& sphere) const;
	void MoveOutside(const Vector3 & vIn, Vector3 & vOut, int iHitSide=-1) const;

	bool TestLineOfSight(Vec3V_In startV, Vec3V_In endV) const;
	bool TestLineOfSightReturnDistance(Vec3V_In startV, Vec3V_In endV, ScalarV_InOut distIfIsectOut) const;

	bool ComputeCrossingPoints(const Vector3& v, const Vector3& w, Vector3& v1, Vector3& v2) const;

	//bool ComputeDistanceToIntersection(const Vector3& v1, const Vector3& v2, float& fDistToIsect) const;

    bool ComputeClosestSurfacePoint(const Vector3& pos, Vector3& vClosestPoint) const;

	static bool LiesInside(const Vector3& vPos, Vector3* normals, float* ds, const int numPlanes, const float fPlaneEps=0.0f);
	static bool LiesInside(const spdSphere& sphere, Vector3* normals, float* ds, const int numPlanes);
	static bool TestLineOfSightOld(const Vector3 & vStart, const Vector3 & vEnd, Vector3* normals, float* ds, const int numPlanes);
	static bool ComputeCrossingPoints(const Vector3& v, const Vector3& w, Vector3* normals, float* ds, const int numPlanes, Vector3& v1, Vector3& v2);
	static bool ComputeDistanceToIntersection(const Vector3& vStart, const Vector3& vEnd, Vector3* normals, float* ds, const int numPlanes, float& fDistToIsect);

#if __BANK
	void fwDebugDraw(Color32 col);
#endif

private:

	Vector3 m_centre;			// centre of bound.
	Vector3 m_corners[4];		// corners of bounding rectangle.
	spdSphere m_sphere;			// bounding sphere that bounds rectangle.
	float m_fTopZ;				// top-most position in Z
	float m_fBottomZ;			// bottom-most position in Z

	void Init(const CEntity & entity, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight, const Matrix34* pOptionalOverrideMatrix, const s32 iNumSubBoundsToIgnore, const s32 * iSubBoundIndicesToIgnore, const s32 iNumSubBoundsToUseExclusively = 0, const s32 * iSubBoundIndicesToUseExclusively = NULL, const float fPedHeight=1.0f);
	bool Init(const CCarDoor & carDoor, const CVehicle * pParent, const float fZPlane, const float fPedRadius, const bool bCalculateTopAndBottomHeight);
	void Init(const Vector3 & vEntityMin, const Vector3 & vEntityMax, const Matrix34 & matOrientation, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight);
	void ComputeCentre(const float& fHeight);
	void ComputeCorners(const CEntity& entity, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight, Mat34V_ConstRef matrix, const s32 iNumSubBoundsToIgnore, const s32 * iSubBoundIndicesToIgnore, const s32 iNumSubBoundsToUseExclusively = 0, const s32 * iSubBoundIndicesToUseExclusively = NULL, const float fPedHeight = 1.0f);
	void ComputeSphere();
	void ComputeBoxes(const CEntity& entity, const phBound* pBound, Vec4V* RESTRICT pMinPosAndZ, Vec4V* RESTRICT pMaxPosAndZ, int& numBoxes, Mat34V_ConstRef mWorld, const s32 iNumSubBoundsToIgnore, const s32 * iSubBoundIndicesToIgnore, const s32 iNumSubBoundsToUseExclusively = 0, const s32 * iSubBoundIndicesToUseExclusively = NULL) const;
	void ComputeBox(const CEntity& entity, const phBound* pBound, Vec4V* RESTRICT pMinPosAndZ, Vec4V* RESTRICT pMaxPosAndZ, int& numBoxes, Mat34V_ConstRef worldMtrx, Mat34V_ConstRef boundMtrx) const;

	void ComputeBoundingBoxPlanes(Vector3* normals, float* ds) const;
	void ComputeBoundingBoxPlanes(Vector4 * planes) const;

	void ProjectBoundingBoxOntoZPlane(const Vector3 & vBoxMinIn, const Vector3 & vBoxMaxIn, const Matrix34 & orientationMat, const float fZPlane, const bool bCalculateTopAndBottomHeight);

public:

	void ComputeSegmentPlanes(Vector2* segmentPlaneNormals, float* segmentPlaneDs) const;
	void ComputeSegmentPlanes(Vector3* semgnetPlaneNormals, float* segmentPlaneDs) const;
};


class CPedGrouteCalculator
{
public:

	enum
    {
    	DIR_UNSPECIFIED		= 0,
    	DIR_ANTICLOCKWISE	= -1,
    	DIR_CLOCKWISE		= 1
   	};

	enum ERouteRndEntityRet
	{
		RouteRndEntityRet_Unspecified		= -1,
		RouteRndEntityRet_StraightToTarget	= 0,
		RouteRndEntityRet_Clockwise			= 1,
		RouteRndEntityRet_AntiClockwise		= 2,
		RouteRndEntityRet_OutToEdge			= 4,
		RouteRndEntityRet_BlockedBothWays	= 5
	};

	CPedGrouteCalculator(){}
	~CPedGrouteCalculator(){}
    
    //Returns 0 if no route was found (point-route will be empty as well).  This can happen when the supplied
    //start & end positions don't in fact intersect the entity at all.
    //Otherwise returns -1 if route goes anticlockwise, and 1 for clockwise.
    static ERouteRndEntityRet ComputeRouteRoundEntityBoundingBox(
		const CPed& ped,
		CEntity& entity,
		const Vector3& vTarget,
		const Vector3 & vTargetPosForLosTest,
		CPointRoute& route,
		const u32 iTestFlags,
		const int iForceDirection = 0,
		const bool bTestCollisionModel = false);

	static ERouteRndEntityRet ComputeRouteRoundEntityBoundingBox(
		const CPed & ped,
		const Vector3 & vStartPos,
		CEntity & entity,
		const Vector3 & vTargetPos,
		const Vector3 & vTargetPosForLosTest,
		CPointRoute & route,
		const u32 iTestFlags,
		const int iForceDirection = 0,
		const bool bTestCollisionModel = false,
		const int iCollisionComponentID = VEH_INVALID_ID);

	static void ComputeRouteOutToEdgeOfBound(const Vector3 & vStartPos, const Vector3 & vTargetPos, CEntityBoundAI & bound, CPointRoute & route, const int iForceThisSide=-1, CEntity * pEntity = NULL, const eHierarchyId iDoorPedIsHeadingFor = VEH_INVALID_ID, const float fSideExtendAmount=0.0f);

	static bool ComputeBestRouteRoundEntity(
		const Vector3 & vStartPos,
		const Vector3& vTargetPos,
		CEntity & entity,
		CPed & ped,
		CPointRoute & route,
		const u32 iTestFlags,
		const float fExpandRouteWidth,
		const s32 iSubBoundIndexToIgnore = -1
	);

	static CPedGrouteCalculator::ERouteRndEntityRet ComputeRouteToDoor(
		CVehicle * pVehicle, CPed * pPed, CPointRoute * pRoute,
		const Vector3 & vStartPos, const Vector3 & vTargetPos, const Vector3 & vTargetPosForLosTest,
		const u32 iLosIncludeFlags, const bool bTestCollisionModel, const eHierarchyId iDoorPedIsHeadingFor);

	//Compute a route around a circle.
	//Returns 0 if no detour was required, else -1 or +1 to indicate whether route went to the left/right of the sphere
	//if "iSideToPrefer" in non-zero, then if a detour is made it will be to left/right depending on this member
   	static int ComputeRouteRoundSphere(
		const Vector3 & vPedPos, const spdSphere& sphere, const Vector3& vStartPoint, const Vector3& vTarget, 
		 Vector3& vNewTarget, Vector3& vDetourTarget, const int iSideToPrefer=0);

	static int ComputeRouteRoundSphereWithLineOfSight(
		const CPed& ped, const spdSphere& sphere, const Vector3& vStartPoint, const Vector3& vTarget, Vector3& vNewTarget, Vector3& vDetourTarget,
		const bool bTestWorld, const bool bTestVehicles, const bool bTestObjects, const bool bTestPeds);
};

#define MAX_DOOR_COMPONENTS		2						// door + window
#define MAX_HELI_COMPONENTS		6+MAX_DOOR_COMPONENTS	// front & rear wings

class CRouteToDoorCalculator
{
public:
	CRouteToDoorCalculator(CPed * pPed, const Vector3 & vStartPos, const Vector3 & vEndPos, CVehicle * pVehicle, CPointRoute * pOutRoute, const eHierarchyId iDoorPedIsHeadingFor=VEH_INVALID_ID );
	~CRouteToDoorCalculator();

	bool CalculateRouteToDoor();

private:

	void GetStartingCornersFromSideBitFlags(const u32 iStartingSideBitflags, int & iStartingCornerClockwise, int & iStartingCornerAntiClockwise);
	bool TestRouteForObstruction(CPointRoute * pRoute, float & fObstructionDistance, CEntity ** ppBlockingEntity);
	void PostProcessToAvoidComponents();

	Vector3 m_vStartPos;
	Vector3 m_vEndPos;
	CVehicle * m_pVehicle;
	CPed * m_pPed;
	CEntityBoundAI m_AiBound;
	eHierarchyId m_iDoorPedIsHeadingFor;
	CPointRoute * m_pRoute;
	u32 m_iRouteSideFlags[CPointRoute::MAX_NUM_ROUTE_ELEMENTS];

	int m_iDoorComponentsToExclude[MAX_DOOR_COMPONENTS*4];
	int m_iNumDoorComponentsToExclude;
};


enum ECanTargetResult
{
	ECanNotTarget,
	ECanTarget,
	ENotSureYet,
	EReadyForNewPedPair
};

#define CAN_TARGET_CACHE_SIZE		40
#define CAN_TARGET_CACHE_TIMEOUT	2000

class CCanTargetCacheEntry
{
public:
	Vector3 m_vFromPoint;							// The point which the Los was done from
	Vector3 m_vToPoint;								// The point which the Los is done to
	Vector3 m_vBlockedPosition;						// The intersection at which Los is blocked (only if blocked!)
	RegdPed m_pFromPed;								// The ped who is doing the targetting
	RegdPed m_pToPed;								// The ped being targetted
	WorldProbe::CShapeTestSingleResult m_hShapeTestHandle;	// Handle to asynchronous linetest query
	u32 m_iTimeIssuedMs;							// The time this query was issued, so we can timeout
	u32 m_bLosExists							:1;	// Whether a line-of-sight between the 2 peds exists
	u32 m_bSuccesfulResultQueried			:1;	// Whether the last Los was invalidated due to positional delta being too great.

	void Init(CPed * pFromPed, CPed * pToPed);
	inline bool MatchesPeds(const CPed * pFromPed, const CPed * pToPed) const { return (pFromPed==m_pFromPed && pToPed==m_pToPed); }
	void Clear();
};

class CCanTargetCache
{
public:

	CCanTargetCache();

	// Returns whether the pTargeter can target the pTarget.
	ECanTargetResult GetCachedResult(const CPed * pTargeter, const CPed * pTarget, CCanTargetCacheEntry ** pNextFreeCacheEntry, const float fMaxPosDeltaSqr, const u32 iMaxTimeDeltaMs, Vector3 * pProbeStartPosition = NULL, Vector3 * pIntersectionIfLosBlocked=NULL, bool bReturnOldestIfFull=false);
	bool AddCachedResult(const CPed * pTargeter, const CPed * pTarget, bool bLineOfSightExists);
	void ClearCachedResult(const CPed* pTargeter, const CPed* pTarget);
	void CancelAsyncProbe(const CPed* pTargeter, const CPed* pTarget);
	void ClearExpiredEntries();
	void Clear();
	void GetLosResults();
	void Debug();

protected:

	CCanTargetCacheEntry m_Cache[CAN_TARGET_CACHE_SIZE];
};

///////////////////////
//Geometry analyser
///////////////////////

enum 
{ 
	TargetOption_doDirectionalTest			= (1<<0), 
	TargetOption_IgnoreTargetsCover			= (1<<1), 
	TargetOption_IgnoreVehicles				= (1<<2), 
	TargetOption_IgnoreLowVehicles			= (1<<3),
	TargetOption_IgnoreSmoke				= (1<<4),
	TargetOption_UseCameraPosIfOnScreen		= (1<<5), 
	TargetOption_TargetInVehicle			= (1<<6), 
	TargetOption_IgnoreLowObjects			= (1<<7), 
	TargetOption_UseFOVPerception			= (1<<8),
	TargetOption_NoLOSCheck					= (1<<9),
	TargetOption_UseTargetableDistance		= (1<<10),
	TargetOption_TargetIsWantedAndInHeli	= (1<<11),
	TargetOption_UseCoverVantagePoint		= (1<<12),
	TargetOption_TargetIsInSubmergedSub		= (1<<13),
	TargetOption_UseMeleeTargetPosition		= (1<<14),
	TargetOption_TestForPotentialPedTargets = (1<<15),
	TargetOption_UseExtendedCombatRange		= (1<<16),
	TargetOption_IgnoreVehicleThisPedIsIn	= (1<<17)
};

class CPedGeometryAnalyser
{
public:
    
    CPedGeometryAnalyser(){}
    ~CPedGeometryAnalyser(){}

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static dev_float ms_MaxPedWaterDepthForVisibility;
	static const float ms_fClearTargetSearchDistance;
	static bool ms_bProcessCanPedTargetPedAsynchronously;
	static float ms_fCanTargetCacheMaxPosChangeSqr;
	static dev_float ms_fExtendedCombatRange;
	static dev_u32 ms_uTimeToUseExtendedCombatRange;

#if !__FINAL
	static bool ms_bDisplayAILosInfo;
	static bool ms_bDebugCanPedTargetPed;
	static int ms_iNumLineTestsSavedLastFrame;
	static int ms_iNumAsyncLineTestsRejectedLastFrame;
	static int ms_iNumCacheFullEventsLastFrame;
	static int ms_iNumLineTestsNotProcessedInTime;
	static int ms_iNumLineTestsNotAsync;
	static int ms_iTotalNumCanPedTargetPointCallsLastFrame;
	static int ms_iTotalNumCanPedTargetPedCallsLastFrame;
#endif

	//Compute move dir to avoid a vehicle.
    static void ComputeMoveDirToAvoidEntity(const CPed& ped, CEntity& entity, Vector3& vMoveDir);
 
	//Test whether the ped's movement is brushing up against the side of an entity & does not constitute a collision/potential-collision
	static bool IsBrushingContact(const CPed& ped, CEntity& entity, Vector3& vMoveDir);

	//Compute if edge crosses the entity bounding box.
	static bool GetIsLineOfSightClear(const Vector3& vStart, const Vector3& vEnd, CEntity& entity);

	//Compute if the lineseg intersects any of the entities surrounding the ped
	static bool GetIsLineOfSightClearOfNearbyEntities(const CPed * pPed, const Vector3& vStart, const Vector3& vEnd, bool bVehicles, bool bObjects, bool bPeds);

	static phBound* TestCapsuleAgainstComposite(CEntity* pEntity, const Vector3& vecStart, const Vector3& vecEnd, const float fRadius, const bool bJustTestBoundingBoxes, const int iNumPartsToIgnore=0, int * ppPartsToIgnore=NULL, const int iNumPartsToTest=0, int * ppPartsToTest=NULL);
	static phBound* TestCapsuleAgainstCompositeR(phInst *pInst, phBound* pBound, const Matrix34* pCurrMat, const Vector3& vecStart, const Vector3& vecEnd, const float fRadius, const bool bJustTestBoundingBoxes, phBoundBox * boundBox, const int iNumPartsToIgnore=0, int * ppPartsToIgnore=NULL, const int iNumPartsToTest=0, int * ppPartsToTest=NULL);

	//Tests the ped's capsule at a prospective position given through "pMatrix".
	static bool TestPedCapsule(const CPed* pPed, const Matrix34* pMatrix, const CEntity** pExceptions=NULL, int nNumExceptions=0,
		 u32 exceptionOptions=EXCLUDE_ENTITY_OPTIONS_NONE, u32 includeFlags=INCLUDE_FLAGS_ALL, u32 typeFlags=TYPE_FLAGS_ALL, u32 excludeTypeFlags=TYPE_FLAGS_NONE,
		 CEntity** pFirstHitEntity=0, s32* pFirstHitComponent=0, 
		 float fCapsuleRadiusMultipier = 1.0f, float fBoundHeading = 0.0f, float fBoundPitch = 0.0f, const Vector3& vBoundOffset = VEC3_ZERO, float fStartZOffset = 0.0f);
	//Tests the point route for a clear line-of-sight against the world
	static bool TestPointRouteLineOfSight(CPointRoute & pointRoute, CPed * pPed, u32 iTestFlags, CEntity * pRouteEntity = NULL);
	static bool TestPointRouteLineOfSight_PerEntityCallback(CEntity * pEntity, void * pData);
 
    //Compute if a ped can jump an obstacle.
    static bool CanPedJumpObstacle(const CPed& ped, const CEntity& entity, const Vector3& vContactNormal, const Vector3& vContactPos);    
    //Compute if a ped can jump an obstacle.
    static bool CanPedJumpObstacle(const CPed& ped, const CEntity& entity);    
    
    //Compute if a ped can target a point.
    static bool CanPedTargetPoint(const CPed& ped, const Vector3& vTarget, s32 iTargetOptions = 0, const CEntity** const ppExcludeEnts = NULL, const int nNumExcludeEnts = 0, CCanTargetCacheEntry * pEmptyCacheEntry = NULL, Vector3 * pProbeStartPosition = NULL, Vector3* pvIntersectionPosition = NULL, const CPed *pTargetee = NULL);
	//Compute if a ped can target another ped.
    static bool CanPedTargetPed(const CPed& targetter, const CPed& targettee, s32 iTargetOptions = 0, CCanTargetCacheEntry * pEmptyCacheEntry = NULL, Vector3 * pProbeStartPosition = NULL, Vector3* pvIntersectionPosition = NULL);
	//As above, but the los is processed asynchronously.  If this returns ENotSureYet, then this function should be called
	//again the next frame by which time it should have a result.
	static ECanTargetResult CanPedTargetPedAsync(CPed& targetter, CPed& targettee, s32 iTargetOptions = 0, const bool bDontIssueAsyncProbe = false, const float fMaxPositionDeltaSqr = ms_fCanTargetCacheMaxPosChangeSqr, const u32 iMaxTimeDeltaMs = CAN_TARGET_CACHE_TIMEOUT, Vector3 * pProbeStartPosition = NULL, Vector3 * pIntersectionIfLosBlocked=NULL);
	//Returns if the ped is currently in a known hot car
	static bool IsPedInUnknownCar(const CPed& ped);
	static bool IsPedInBush(const CPed& ped);
	static bool IsPedInWaterAtDepth(const CPed& ped, float fDepth);
	//Compute if a ped can see another ped whose currently in an unknown car, must be facing the car and in a short range
	static bool CanPedSeePedInUnknownCar(const CPed& targetter, const CPed& targettee);
	static bool CanPedSeePedInBush(const CPed& targetter, const CPed& targettee);

	static float GetDistanceSpottedInUnknownVehicle();
	static float GetDistanceSpottedInBush();

	//Clears a cached result for the aggressor and target (if one exists)
	static void ClearCachedResult(const CPed* pTargeter, const CPed* pTarget) { ms_CanTargetCache.ClearCachedResult(pTargeter, pTarget); }

	// Cancels an asynchronous probe between two peds if one is in flight.
	static void CancelAsyncProbe(const CPed* pTargeter, const CPed* pTarget) { ms_CanTargetCache.CancelAsyncProbe(pTargeter, pTarget); }

    //Compute if a ped is in free-fall.
    static bool IsInAir(const CPed& ped);
    
    //Get the ped nearest to a point
	static const CPed* GetNearestPed(const Vector3& vPos, const CPed* pException = NULL );

	// See whether the ped is blocked in by objects & vehicles, likely to obstruct route-finding
	static bool IsPedPennedInByObjects(CPed * pPed);

	// Adjust the given position based on our cover information
	static void AdjustPositionForCover(const CPed& ped, const Vector3& vTarget, Vector3& vPositionToAdjust);

	static bool TestIfCollisionTypeIsValid(const CPed* pPed, u32 includeFlags, const WorldProbe::CShapeTestResults& results);

	enum
	{
		MAX_NUM_COLLISION_POINTS=8
	};
	static int ms_iNumCollisionPoints;
	static Vector3 ms_vecCollisionPoints[MAX_NUM_COLLISION_POINTS];

	static u32 m_nAsyncProbeTimeout;

	static void Process();
	static void RetrieveAsyncResults();

	static void GetModifiedTargetPosition(const CPed & targetter, 
		const CPed & targettee, 
		const bool bUseDirectionTest, 
		const bool bIgnoreTargetsCover, 
		const bool bUseTargetableDistance, 
		const bool bUseCoverVantagePoint, 
		const bool bUseMeleePosition,
		Vector3 & vTargetPos);

private:

	static float ms_fWldCallbackPedHeight;
	static bool ms_bWldCallbackLosStatus;
	static CEntity * ms_pEntityPointRouteIsAround;

	static CCanTargetCache ms_CanTargetCache;

};



#endif

