#ifndef PATHSERVERTHREAD_H
#define PATHSERVERTHREAD_H

// Rage headers
#include "math/random.h"

// Framework headers
#include "ai/navmesh/datatypes.h"
#include "ai/navmesh/pathserverthread.h"
#include "fwmaths/Angle.h"

//**************************************************************************************************
// Define as 1 to use the visited-polys list even when using the A* timestamp method of clearing visited flags

#define AWLAYS_MARK_VISTED_POLYS			0
#define MAX_DEFERRED_CLIMB_DEACTIVATIONS	80

//**************************************************************************************************

//****************************************************************************************
//****************************************************************************************

namespace rage
{
	class CNavGen;
	class CPathSearchPriorityQueue;
}

class CBlockedLadders
{
public:

	void Reset();
	bool IsLadderBlocked(const Vector3 & vBasePosition) const;
	void UpdateUsage(const Vector3 & vBasePosition);

protected:

	static const s32 ms_iMaxNumLadders = 8;

	static dev_s32 ms_iDisableInterval;
	static dev_s32 ms_iMaxNumClimbers;

	class CItem
	{
	public:
		Vector3 m_vBasePosition;
		s32 m_iNumClimbers;
		s32 m_iDisableTimer;
		s32 m_iLastPathfindTime;
	};

	CItem m_Ladders[ms_iMaxNumLadders];
};


//********************************************************************************************************
//	CPathServerThread
//	This is the implementation of the pathfinder, which works continuously to service the path requests
//	which clients have submitted to the CPathServer class.
//********************************************************************************************************


class CPathServerThread : public fwPathServerThread
{
	// At least for now, these classes use CPathServerThread directly. If
	// CPathServerThread moves down to the RAGE framework, we may have to
	// add accessors or something instead. /FF
	friend class CPathServerGameInterfaceGta;
	friend class CPathServerGta;

	friend class ::CPathServer;
	friend class CNavGenApp;
	friend class CNavGen;
	friend class CPathFindMovementCosts;

	friend class fwPolyObjectsCache;

public:
	CPathServerThread();
	~CPathServerThread();

	bool Init(u32 iProcessorIndex);
	void Close(void);

	void Reset();	// This is called by CPathServer::Init/ShutdownLevel()

	void Run(void);
	inline void SignalQuit(void) { m_bQuitNow = true; }

	inline bool IsStarted(void) { return (m_iThreadID != 0); }

	inline u32 GetCurrentProcessorIndex(void) { return m_iProcessorIndex; }

	EPathServerErrorCode GetNavMeshPolyForRouteEndPoints(
		const Vector3 & vPos,
		const TNavMeshAndPoly * pExistingNavMeshAndPoly,
		CNavMesh *& pNavMesh,
		TNavMeshPoly *& pPoly,
		Vector3 * pNewPos,
		const float fDistanceBelowPointToLook,
		const float fDistanceAbovePointToLook,
		const bool bCheckDynamicNavMeshes,
		const bool bSnapPointToNavMesh,		
		aiNavDomain domain,
		const float fMaxDistanceToAdjustEndPoint = TRequestPathStruct::ms_fDefaultMaxDistanceToAdjustPathEndPoints,
		const Vector3* pvSearchDir = NULL);

	void UpdateAllDynamicObjectsPositions();
	void UpdateAllDynamicObjectsPositionsNoLock();

	bool DoesNavMeshPolyIntersectAnyDynamicObjects(CNavMesh * pNavMesh, TNavMeshPoly * pPoly);
	bool DoesRegionIntersectAnyDynamicObjects(const Vector3 & vPosition, float fRadius);

	bool DoesPositionIntersectAnyDynamicObjectsApproximate(const Vector3 & vPosition) const;
	bool DoesMinMaxIntersectAnyDynamicObjectsApproximate(const TShortMinMax & minMax) const;

	void ProcessDeferredDeactivationOfClimbs();

	s32 GetNumPendingRequests() const;
	const TDynamicObject& GetDynamicObject(const TDynamicObjectIndex& in_index) const { return m_DynamicObjectsStore[in_index]; }

private:

	CPathServerRequestBase * GetNextRequest();

	CPathRequest * GetNextPathRequest();
	CPathRequest * GetNextPathRequest_New();
	CGridRequest * GetNextGridRequest();
	CLineOfSightRequest * GetNextLosRequest();
	CAudioRequest * GetNextAudioPropertiesRequest();	
	CFloodFillRequest * GetNextFloodFillRequest();
	CClearAreaRequest * GetNextClearAreaRequest();
	CClosestPositionRequest * GetNextClosestPositionRequest();

	CPathServerRequestBase * WaitForNextRequest();

	// Perform non-pathfinding task which are required to tick over in the background
	//	- extracting coverpoints from navmeshes into the game's CCover class
	//	- finding valid ped generation coordinates for the ped population
	void DoHousekeeping();

	// If game has been paused, then we need to add this time onto all path requests
	void ResetRequestTimers();

	// Make sure that no requests which have been serviced have been waiting for more than REQUEST_TIMEOUT_VAL_IN_MILLISECS ms
	// This could indicate that a task (or something) has issued a request & forgotten to retrieve/cancel it
	void CheckForStaleRequests();

	// Adds an entry to the list of climbs to be disabled in the navmesh during housekeeping
	void AddDeferredDisableClimbAtPosition(const Vector3 & vPos, const bool bNavmeshClimb, const bool bClimbableObject);

	//******************************************************************************************************
	//	Generates a path through the navmeshes, in response to a client request
	//******************************************************************************************************

	bool ProcessPathRequest(CPathRequest * pPathRequest);

	EPathFindRetVal FindPath(CNavMesh * pStartNavMesh, CNavMesh * pEndNavMesh, CPathRequest * pPathRequest);

	void CalculateSearchExtents(CPathRequest * pPathRequest, TShortMinMax & pathSearchDistanceMinMax, Vector3 & vExtentsMin, Vector3 & vExtentsMax);
	bool CanLeaveConnectionPolyViaThisEdge(TNavMeshPoly * pConnectionPoly, CNavMesh * pConnectionPolyNavMesh, const s32 iEdge, const u32 iReachedByConnectionPolyIndex);
	void ObtainAdjacentPolys( TNavMeshPoly ** ppOutPolys, const TAdjPoly ** ppOutAdjacencies, s32 & iNumPolys, const s32 iMaxNumPolys, TNavMeshPoly * pRealOrConnectionPoly, const TAdjPoly * pAdjacencyToPoly, CNavMesh * pNavMesh, const Vector3 & vExitEdge, const Vector3 & vExitEdgeV1, const Vector3 & vExitEdgeV2, const aiNavDomain domain ) const;
	Vector3 CreateRandomPointInPoly(const TNavMeshPoly * pPoly, Vector3 * pPolyPts);

	bool ObtainEntryEdgeViaConnectionPoly(
		TNavMeshPoly * pStartPoly, CNavMesh * pStartNavMesh,
		TNavMeshPoly * pTargetPoly, CNavMesh * pTargetNavMesh, 
		TNavMeshPoly * pRealOrConnectionPoly, CNavMesh * pNavMesh,
		TNavMeshPoly * pParentPoly, CNavMesh * pParentNavMesh,
		TNavMeshAndPoly & outPrevNavMeshAndPoly, const Vector3 & vStartExitEdge, const Vector3 & vStartExitEdgeV1, const Vector3 & vStartExitEdgeV2, const aiNavDomain domain ) const;

	// A suite of functions which evaluate the best point to choose in the adjacent polygon during a pathsearch
	float GetClosestPointInPolyToTargetDistSqr(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vClosestPoint, s32 & iPointEnum);
	float GetClosestPointInPolyToTargetDistSqrWithRadius( TVisitPolyStruct & vars, const u32 iFlags, const float fRadius, Vector3 * vClosestPoint, s32 & iPointEnum );

	bool GetClosestPointInPolyToTargetWithLos(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vClosestPoint, s32 & iPointEnum, float & fCost );
	bool GetClosestPointInPolyToTargetWithLosToTarget(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vClosestPoint, s32 & iPointEnum, float & fCost);
	float GetFurthestPointInPolyFromTargetDistSqr(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum);
	bool GetFurthestPointInPolyFromTargetWithLos(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum);
	bool GetFurthestPointInPolyFromTargetWithLosToTarget(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum);
	bool GetPointInPolyWithBestAngleFromStartWithLos(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vBestPoint, float * fOutputScore, s32 & iPointEnum);
	float GetClosestPointInPolyFromLastPointDistSqr(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum, const Vector3& vLastPoint);
	bool GetClosestPointInPolyFromLastPointWithLos(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum, const Vector3& vLastPoint);

	s32 GetClosestPointInFrontOfPlane( TVisitPolyStruct & vars, TNavMeshPoly * pPoly, CNavMesh * pNavMesh, const Vector3 & vCloseToPos, const Vector3 & vPlaneDir, const float fPlaneDist );

	// Get the polygon below the point, which intersects most with the radius.  This is to be used for
	// selecting start/end polys when there is none *directly* underneath a ped.
	TNavMeshAndPoly GetClosestPolyBelowPoint(aiNavDomain domain, const Vector3 & vPos, float fMaxRadius, float fHeightAbove = 3.5f, float fHeightBelow = 1.5f, bool bOnlyOnPavement = false, bool bClosestToCentre = false);
	// This extension of the above function checks navmeshes up,down,left & right - as well as the one directly under the input point
	TNavMeshAndPoly GetClosestPolyBelowPointInMultipleNavMeshes(aiNavDomain domain, const Vector3 & vPos, float fMaxRadius, float fHeightAbove, float fHeightBelow, bool bOnlyOnPavement, bool bClosestToCentre);

	bool FindApproxNavMeshPolyForRouteEndPoints(const Vector3 & vPos, float fRadius, TNavMeshAndPoly & closestPoly, bool bTessellationNavMesh, const TNavMeshIndex iThisDynamicNavMeshOnly/*=NAVMESH_NAVMESH_INDEX_NONE*/, aiNavDomain domain, const Vector3* pvPolySearchDir=NULL);
	bool FindApproxNavMeshPolyForRouteEndPointsAndAdjustPosition(const Vector3 & vPos, float fRadius, TNavMeshAndPoly & closestPoly, bool bTessellationNavMesh, Vector3 & vNewPos, const TNavMeshIndex iThisDynamicNavMeshOnly/*=NAVMESH_NAVMESH_INDEX_NONE*/, aiNavDomain domain, const Vector3* pvPolySearchDir=NULL);

	EPathServerErrorCode GetMultipleNavMeshPolysForRouteEndPoints(CPathRequest * pPathRequest, TNavMeshPoly * pStartingPoly, CNavMesh * pNavMesh, const Vector3 & vPos, float fRadius, atArray<TNavMeshPoly*> & navMeshPolys, aiNavDomain domain);
	void FloodFillToGetRouteEndPolys(CPathRequest * pPathRequest, TNavMeshPoly * pStartingPoly, CNavMesh * pNavMesh, const TShortMinMax & regionMinMax, atArray<TNavMeshPoly*> & navMeshPolys, aiNavDomain domain);

	bool RefinePathAndCreateNodes(CPathRequest * pPathRequest, const Vector3 & vStartPos, const Vector3 & vEndPos, s32 iMaxNumPoints);
	bool ConvertPointsInPolyToVertexList(CPathRequest * pPathRequest, const Vector3 & vStartPos, const Vector3 & vEndPos, Vector3 & vOut_StartWaypoint, TNavMeshPoly *& pOut_StartPoly, Vector3 & vOut_EndWaypoint, TNavMeshPoly *& pOut_EndPoly, s32 & iOut_NumPointsToTest);

	bool RefineSecondPass(CPathRequest * pPathRequest);

	void AddWaypointsToPreserveHeightChanges(CPathRequest * pPathRequest);

	bool FindHeightChangePosAlongLine(const Vector3 & vStartPos, const Vector3 & vEndPos, const TNavMeshPoly * pToPoly, TNavMeshPoly * pTestPoly, TNavMeshPoly *& pHeightChangePoly, Vector3 & vHeightChangePos, const bool bOnlyUphill /* = false */, aiNavDomain domain);
	bool FindHeightChangePosAlongLineR(TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, aiNavDomain domain);

	void MinimisePathDistance(CPathRequest * pPathRequest);
	void PullPathOutFromEdges(CPathRequest * pPathRequest, bool bDoStringPull);

	void SmoothPath(CPathRequest * pPathRequest);
	void CalcPathBezierControlPoints(CPathRequest * pPathRequest);

	// Tests a LOS across the navmesh, not considering dynamic objects or non-standard links
	bool TestNavMeshLOS(const Vector3 & vStartPos, const Vector3 & vEndPos, const u32 iLosFlags, aiNavDomain domain);
	// As above, but should be used when we already know start & end polys.  Avoids expensive poly querying.
	bool TestNavMeshLOS(const Vector3 & vStartPos, const Vector3 & vEndPos, TNavMeshPoly * pToPoly, TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, const u32 iLosFlags, aiNavDomain domain);
	// The recurwsive component of the above function(s)
	bool TestNavMeshLOS_R(TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, aiNavDomain domain);

	TNavMeshPoly * TestNavMeshLOS(const Vector3 & vStartPos, const Vector2 & vLosDir, Vector3 * vOut_IntersectionPos, TNavMeshPoly * pStartPoly, const u32 iLosFlags, aiNavDomain domain);

	// Tests a LOS through dynamic objects
	bool TestDynamicObjectLOS(const Vector3 & vStart, const Vector3 & vEnd);
	bool TestDynamicObjectLOS(const Vector3 & vStart, const Vector3 & vEnd, atArray<TDynamicObject*> & objectList);

	// Same as the above LOS test, but it performs two tests spaced fRadius apart to simulate the width of a moving ped
	bool TestDynamicObjectLOSWithRadius(const Vector3 & vStart, const Vector3 & vEnd, const float fRadius);

	struct TDynObjLosCallbackData
	{
		Vector3 vStartPos;
		Vector3 vEndPos;
		CPathServerThread * pPathServerThread;
		float fMinZOfStartAndEnd;
		float fMaxZOfStartAndEnd;
		bool bIntersection;
	};

	static bool TestDynamicObjectLOSCallback(TDynamicObject * pObject, void * pData);

	bool TestIsClimbClearOfObjects(u32 AdjacencyType, TNavMeshPoly* pPoly, TNavMeshPoly* pAdjPoly, CNavMesh* pNavMesh, CNavMesh* pAdjNavMesh);

	bool GetSharedEdgeEndPoints(TNavMeshPoly * pPoly1, TNavMeshPoly * pPoly2, Vector3 & vPoint1, Vector3 & vPoint2, aiNavDomain domain);
	bool GetPointAlongSharedEdge(const Vector3 & vBestPointInPoly1, const Vector3 & vBestPointInPoly2, TNavMeshPoly * pPoly1, TNavMeshPoly * pPoly2, float fEntityRadius, Vector3 & vPoint, bool & bOut_LineActuallyIntersectsEdge, aiNavDomain domain);
	bool GetPointWithDynamicObjectVisiblityAlongSharedEdge(const Vector3 & vBestPointInPoly1, const Vector3 & vBestPointInPoly2, TNavMeshPoly * pPoly1, TNavMeshPoly * pPoly2, float fEntityRadius, Vector3 & vPoint, aiNavDomain domain);

	void IncTimeStamp();
	void IncAStarTimeStamp();

	void HandleTimeStampOverFlow();
	void HandleAStarTimeStampOverFlow();

	void RemoveAndAddObjects();

	void RelinkDynamicObjectsList();	// HACK

	inline TNavMeshIndex GetDynamicObjectIndex(const TDynamicObject * pObj) const
	{
		const s32 iOffset = ptrdiff_t_to_int(pObj - m_DynamicObjectsStore);
		Assert(iOffset >= 0 && iOffset < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);
		return (TNavMeshIndex) iOffset;
	}

	bool MaybeTessellateAndMarkPolysSurroundingPathStart(CNavMesh * pNavMesh, CPathRequest * pPathRequest, const float fInitialCost);
	void MaybeTessellatePolysSurroundingPathEnd(CNavMesh * pNavMesh, CPathRequest * pPathRequest);

	// Finds a position for a ped, which is clear of dynamic object obstructions.  This is used to adjust the path start & end
	// points, so that they are not inside objects.
	bool FindPositionClearOfDynamicObjects(Vector3 & vPosition, bool bDoLastResortJittering, bool bIgnoreOpenable, bool bTestLineOfSight, aiNavDomain domain);

	// Tessellates the navmesh poly, and creates fragments in the m_pTessellationNavMesh.  Returns a new starting poly for the pathfinder to use.
	bool TessellateNavMeshPoly(
		CNavMesh * pNavMesh, TNavMeshPoly * pPoly, u32 iPolyIndex,
		bool bThisIsStartPoly, bool bThisIsEndPoly,
		TNavMeshPoly ** pOutStartingPoly,
		u32 iReturnStartingPoly_NavMeshIndex,
		u32 iReturnStartingPoly_PolyIndex,
		atArray<TNavMeshPoly*> * outputFragments = NULL);

	// Tessellates the navmesh poly, and creates fragments in the m_pTessellationNavMesh.  Returns a new starting poly for the pathfinder to use.
	// This newer version also creates double the number of interior polygons, and also creates degenerate zero-area
	// connection polys which allow external edges to be bisected and overall tessellation & object avoidance to be superior.
	bool TessellateNavMeshPolyAndCreateDegenerateConnections(
		CNavMesh * pNavMesh, TNavMeshPoly * pPoly, u32 iPolyIndex,
		bool bThisIsStartPoly, bool bThisIsEndPoly,
		TNavMeshPoly ** pOutStartingPoly,
		u32 iReturnStartingPoly_NavMeshIndex,
		u32 iReturnStartingPoly_PolyIndex,
		atArray<TNavMeshPoly*> * outputFragments = NULL,
		Vector3 * pMidPoint = NULL);

	bool NewTessellateNavMeshPolyAndCreateDegenerateConnections(
		CNavMesh * pNavMesh, TNavMeshPoly * pPoly, u32 iPolyIndex,
		bool bThisIsStartPoly, bool bThisIsEndPoly,
		TNavMeshPoly ** pOutStartingPoly,
		u32 iReturnStartingPoly_NavMeshIndex,
		u32 iReturnStartingPoly_PolyIndex,
		atArray<TNavMeshPoly*> * outputFragments = NULL);
	// Removes all tessellation on the given poly & restores it to its original state

#if !__FINAL && !__PPU && defined(SANITY_CHECK_TESSELLATION)
	void SanityCheckPolyConnections(CNavMesh * pNavMesh, TNavMeshPoly * pPoly);
#endif

	void UnTessellateAllVisitedPolys(TNavMeshPoly ** pVisitedPolys, s32 iNumPolys);

	bool ClipNavMeshPolyToDynamicObjects(CNavMesh * pNavMesh, TNavMeshPoly * pPoly, Vector3 * pPolyPts);
	void ClipToPlanes(s32 iNumPts, Vector3 * pPoints, TAdjPoly * pAdjacencies, u32 iSideFlags, Vector3 * vPlaneNormals, float * fPlaneDists, s32 iPlaneIndex, atArray<CClippedFragmentPoly*> & outputFragmentPolys);

	//******************************************************************************************************
	// Sample an axis aligned regularly spaced grid for walkability, in response to a client request
	// Works by flood-filling polys from the origin until outside the bbox of the area.
	// Then for each poly found, samples aligned vertical lines and sets data in a CWlkRndObjGrid class.
	//******************************************************************************************************
	void SampleWalkableGrid(CGridRequest * pPathRequest);
	void SampleWalkableGrid_FloodFillWalkablePolys(CNavMesh * pNavMesh, TNavMeshPoly * pPoly, const Vector3 & vMin, const Vector3 & vMax);


	//**********************************************************************************************
	//	Tests whether a line of sight exists across the navmeshes, in response to a client request
	//**********************************************************************************************
	bool ProcessLineOfSightRequest(CLineOfSightRequest * pLosRequest);


	//**********************************************************************************************
	//	Processes a request for the audio properties from the navmesh poly under a given position.
	//**********************************************************************************************
	bool ProcessAudioPropertiesRequest(CAudioRequest * pAudioRequest);
	bool ProcessAudioPropertiesRequestR(CNavMesh * pNavMesh, TNavMeshPoly * pPoly, const s32 iNumRecursions);

	//**********************************************************************************************
	//	Processes a request which requires a flood-filling operation from a given start position.
	//	There may be several sub-types of these requests, and hopefully we will be able to handle
	//	them using a base flood-filling algorithm and a specific per-type callback function.
	//**********************************************************************************************
	bool ProcessFloodFillRequest(CFloodFillRequest * pFloodFillRequest);

	static bool FloodFill_FindClosestCarNode_ShouldVisitPolyFn(const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, const TNavMeshPoly * pFromPoly, const CNavMesh * pFromNavMesh, const TAdjPoly * adjacency);
	static bool FloodFill_FindClosestCarNode_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float fDistanceTravelled);
	static bool FloodFill_FindClosestShelteredPoly_ShouldVisitPolyFn(const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, const TNavMeshPoly * pFromPoly, const CNavMesh * pFromNavMesh, const TAdjPoly * pAdjacency);
	static bool FloodFill_FindClosestShelteredPoly_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float fDistanceTravelled);
	static bool FloodFill_FindClosestUnshelteredPoly_ShouldVisitPolyFn(const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, const TNavMeshPoly * pFromPoly, const CNavMesh * pFromNavMesh, const TAdjPoly * pAdjacency);
	static bool FloodFill_FindClosestUnshelteredPoly_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float fDistanceTravelled);
	static bool FloodFill_CalcAreaUnderfoot_ShouldVisitPolyFn(const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, const TNavMeshPoly * pFromPoly, const CNavMesh * pFromNavMesh, const TAdjPoly * pAdjacency);
	static bool FloodFill_CalcAreaUnderfoot_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float fDistanceTravelled);
	static bool FloodFill_FindClosestPositionOnLand_ShouldVisitPolyFn(const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, const TNavMeshPoly * pFromPoly, const CNavMesh * pFromNavMesh, const TAdjPoly * pAdjacency);
	static bool FloodFill_FindClosestPositionOnLand_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float fDistanceTravelled);
	static bool FloodFill_FindNearbyPavement_ShouldVisitPolyFn			(const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, const TNavMeshPoly * pFromPoly, const CNavMesh * pFromNavMesh, const TAdjPoly * pAdjacency);
	static bool FloodFill_FindNearbyPavementPoly_ShouldEndOnThisPolyFn	(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float fDistanceTravelled);

	//-------------------------------------------------------------------------

	bool ProcessClearAreaRequest(CClearAreaRequest * pClearAreaRequest);

	//--------------------------------------------------------------------------------------------------------------------------------

	bool ProcessClosestPositionRequest(CClosestPositionRequest * pClosestPosRequest);

	static bool ClosestPositionRequest_PerPolyCallback(const CNavMesh * pNavMesh, const TNavMeshPoly * pPoly);
	static float ClosestPositionRequest_PerPointCallback(const CNavMesh * pNavMesh, const TNavMeshPoly * pPoly, const Vector3 & vPos);

	//--------------------------------------------------------------------------------------------------------------------------------


	TPathServerThreadFuncParams m_PathServerThreadParams;

#ifndef GTA_ENGINE
	HANDLE m_hThreadHandle;
	//DWORD m_iThreadID;
#endif

	CPathServerRequestBase * m_pCurrentActiveRequest;
	CPathRequest * m_pCurrentActivePathRequest;
	CGridRequest * m_pCurrentActiveGridRequest;
	CLineOfSightRequest * m_pCurrentActiveLosRequest;
	CAudioRequest * m_pCurrentActiveAudioRequest;
	CFloodFillRequest * m_pCurrentActiveFloodFillRequest;
	CClearAreaRequest * m_pCurrentActiveClearAreaRequest;
	CClosestPositionRequest * m_pCurrentActiveClosestPositionRequest;

	static TPathFindVars m_Vars;
	static TVisitPolyStruct m_VisitPolyVars;
	static CTestNavMeshLosVars m_LosVars;
	static CBlockedLadders m_BlockedLadders;

	static bool m_bHandleTimestampOverflow;
	static bool m_bHandleAStarTimestampOverflow;

	// The list of all objects intersecting the "bothPolysMinMax" min/max region
	static atArray<TDynamicObject*> ms_dynamicObjectsIntersectingPolygons;

	// The working value, set at the start of a path search based upon path params
	// The default value - tweakable via a widget
	static float ms_fNonPavementPenalty_ShortestPath;
	static float ms_fNonPavementPenalty_Flee;
	static float ms_fNonPavementPenalty_FleeSofter;
	static float ms_fNonPavementPenalty_Wander;

	static float ms_fClimbMultiplier_ShortestPath;
	static float ms_fClimbMultiplier_Flee;
	static float ms_fClimbMultiplier_Wander;

	static float ms_fWaterMultiplier_Flee;

	static const float ms_fWanderOriginPullBackDistance;	// The distance by which the m_vWanderOrigin is pulled back, each time we consider a polygon when doing a wander path

	// Which was the last request type (path, grid or LOS) ?
	int m_iLastRequestType;

	// Compact random number generator, just for the pathserver thread
	mthRandom m_RandomNumGen;

	// When this flag is set, the CPathServerThread's thread will exit!
	// This is the only way for the thread to quit - and it is set by calling
	// SignalQuit().  The CPathServer class automatically calls this on shutdown.
	bool m_bQuitNow;
	bool m_bHasQuit;

	// This critical section synchronises access to all of the navmesh data.
	// Streaming will need to wait on this before it can add/remove mesh sections.
	// NB : Alternatively the CPathServerThread itself could interface with the
	// streaming, in order to request & obtain the navmeshes.
#ifndef GTA_ENGINE
	CRITICAL_SECTION m_NavMeshDataCriticalSection;
#endif
	// Mutex & access-req'd flags for dynamic objects.  The main game will
	// sometimes need to add/remove dynamic objects, and if these are moving will
	// quite frequently need to update object positions.
#ifndef GTA_ENGINE
	CRITICAL_SECTION m_DynamicObjectsCriticalSection;
#endif

	// Critical section for immediate-mode path queries.  Streaming load & evict functions must
	// wait on both m_NavMeshDataCriticalSection & m_ImmediateAccessCriticalSection.
#ifndef GTA_ENGINE
	CRITICAL_SECTION m_NavMeshImmediateAccessCriticalSection;
#endif

	// These variables define how many iterations of the algorithms to do, before forcing the thread to Sleep()
	static const u32 ms_iNumFindPathIterationsBeforeGivingTime;
	static const u32 ms_iNumRefinePathIterationsBeforeGivingTime;
	static const u32 ms_iNumMinimisePathLengthIterationsBeforeGivingTime;

	static bank_u32 ms_iPathRequestNumMillisecsBeforeSleepingThread;
	static sysPerformanceTimer	* m_PathRequestSleepTimer;
	static bool m_bSleepingDuringPathSearch;

	// If "CPathServer::ms_bProcessAllPendingPathsAtOnce" is set, then the following variable(s) count up
	// how much processing has been done for this batch of requests.  If a set number is exceeded, then we
	// will suspend processing and wait once more.
	static s32 ms_iNumVisitedPolysInThisBatchOfPathRequests;
	static s32 ms_iNumTessellationsInThisBatchOfPathRequests;

	static s32 ms_iMaxNumVisitedPolysInEachBatchOfPathRequests;
	static s32 ms_iMaxNumTessellationsInEachBatchOfPathRequests;

	// Priority queue for A* search across navmesh polys
	CPathSearchPriorityQueue * m_PathSearchPriorityQueue;

	//********************************************************************
	//	TExplorePolyFunc
	//********************************************************************

	static inline void CalcAdjPolyPointsNow(const CPathRequest * UNUSED_PARAM(pPathRequest), const CNavMesh * pAdjNavMesh, const TNavMeshPoly * pAdjPoly)
	{
		if(!m_Vars.m_bAdjPolyPointsCalculated)
		{
			u32 v;
			const u32 iNumVerts = pAdjPoly->GetNumVertices();

			for(v=0; v<iNumVerts; v++)
			{
				pAdjNavMesh->GetVertex( pAdjNavMesh->GetPolyVertexIndex(pAdjPoly, v), m_Vars.m_vPolyPts[v] );
			}

			m_Vars.m_bAdjPolyPointsCalculated = true;
		}
	}

	bool IsThisAdjacencyTypeOk(CPathRequest * pPathRequest, const TAdjPoly & adjPoly);
	float GetAdjacencyTypePenalty(CPathRequest * pPathRequest, const TAdjPoly & adjPoly, const TNavMeshPoly * pPoly, const TNavMeshPoly * pAdjPoly, const Vector3 & vPositionInFromPoly);

	enum eExploreFuncRetVal
	{
		eOkayAddToBinHeap			= 0,
		eMustTessellateThisPoly		= 1,
		eCannotVisitThisPoly		= 2
	};

	typedef eExploreFuncRetVal TExplorePolyFunc(const TNavMeshPoly * pPrevPoly, const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, Vector3 & vClosestPoint, float & fCost, s32 & iPointEnum, const bool bCouldBeTessellatedFurther);

	class CNavSearchTypes
	{
		enum eNavSearchTypes
		{
			ENavSearch_ShortestPath,
			ENavSearch_Flee,
			ENavSearch_Wander,
			ENavSearch_NumTypes
		};
	};

	bool ShouldUseMorePointsForPoly(const TNavMeshPoly & poly, CPathRequest * pPathRequest) const;

	static eExploreFuncRetVal VisitPoly_ShortestPath(const TNavMeshPoly * pPrevPoly, const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, Vector3 & vClosestPoint, float & fCost, s32 & iPointEnum, const bool bCouldBeTessellatedFurther);
	static eExploreFuncRetVal VisitPoly_Flee(const TNavMeshPoly * pPrevPoly, const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, Vector3 & vClosestPoint, float & fCost, s32 & iPointEnum, const bool bCouldBeTessellatedFurther);
	static eExploreFuncRetVal VisitPoly_Wander(const TNavMeshPoly * pPrevPoly, const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, Vector3 & vClosestPoint, float & fCost, s32 & iPointEnum, const bool bCouldBeTessellatedFurther);

	bool PathSearch_TestAdjacencyWithRadius(
		CNavMesh * pFromNavMesh, TNavMeshPoly * pFromPoly, const Vector3 & vFromPos,
		CNavMesh * pToNavMesh, TNavMeshPoly * pToPoly, const Vector3 & vToPos,
		float fEntityRadius, Vector3 * pExitVerts, float * pExitFreeSpace);

	//***************************************************************************************
	// This array stores pointers to all the polys which were touched during a FindPath()
	// Whenever we quit that function, we revisit each poly & reset their open/closed flags.
	// This prevents us having to maintain open & closed lists as in a standard A* algorithm -
	// instead that open/closed information is stored compactly within each navmesh polygon.
#if __DEV
	TNavMeshPoly * m_VisitedPolys[MAX_PATH_VISITED_POLYS];
#endif
	u32 m_iLastNumVisitedPolys;	// This used only for debugging

#if !__FINAL
	static bool ms_bMarkVisitedPolys;
	static bank_bool ms_bUsePrefetching;
#endif

#if __DEV
	enum eVisitedPolyMarkingMode
	{
		EUseVisitedPolysList,
		EUseAStarTimeStamp,
		EUseBothMethods
	};
	static s32 m_eVisitedPolyMarkingMode;
#endif

	void CalculatePolyMinMaxForDynamicNavMesh(TNavMeshPoly * pPoly, aiNavDomain domain);

	// This wrapper should be called the first time a poly is ever seen during pathfinding.
	// It calls the appropriate method which we are using to keep track of which polys are visited per-pathsearch.
	// A return value of false indicates that we've run out of space.
	inline bool OnFirstVisitingPoly(TNavMeshPoly * pPoly)
	{
		if(!m_pCurrentActivePathRequest) return true;

#if !__DEV
		return ResetPolyFlagsAndMarkAsVisited(pPoly);
#else
		pPoly->SetDebugMarked(ms_bMarkVisitedPolys);

		if(m_eVisitedPolyMarkingMode==EUseVisitedPolysList)
		{
			return AddPolyToVisitedList(pPoly, m_pCurrentActivePathRequest);
		}
		else if(m_eVisitedPolyMarkingMode==EUseAStarTimeStamp)
		{
			return ResetPolyFlagsAndMarkAsVisited(pPoly);
		}
		else
		{
#if __ASSERT
			bool b1 = AddPolyToVisitedList(pPoly, m_pCurrentActivePathRequest);
			m_Vars.m_iNumVisitedPolys--;
			bool b2 = ResetPolyFlagsAndMarkAsVisited(pPoly);
			Assert(b1==b2);
			return b1;
#else	// __ASSERT
			AddPolyToVisitedList(pPoly, m_pCurrentActivePathRequest);
			m_Vars.m_iNumVisitedPolys--;
			return ResetPolyFlagsAndMarkAsVisited(pPoly);;
#endif	// __ASSERT
		}
#endif	// !__DEV
	}

	// AddPolyToVisitedList() is used when mode is "EUseVisitedPolysList"
	// Mark this poly as visited & add to the list of which polys to reset after the path processing
	// If this returns FALSE then the poly couldn't be added (list full) and path search will have to abort.
#if __DEV
	inline bool AddPolyToVisitedList(TNavMeshPoly * pPoly, CPathRequest * pPathRequest)
	{
		m_VisitedPolys[m_Vars.m_iNumVisitedPolys++] = pPoly;

		if(m_Vars.m_iNumVisitedPolys == MAX_PATH_VISITED_POLYS)
		{
			// We've run out of space in "m_VisitedPolys" for the path-search..
			// NB : Ahem, there must be a better solution.. How about using
			// the poly timestamp as well, which means we can just go through
			// reseting all the flags now, and then set m_iNumVisitedPolys to 0.
			pPathRequest->m_bRequestActive = false;
			pPathRequest->m_bComplete = true;
			pPathRequest->m_iNumPoints = 0;
			pPathRequest->m_iCompletionCode = PATH_RAN_OUT_VISITED_POLYS_SPACE;

			return false;
		}

		return true;
	}
#endif

	// ResetPolyFlagsAndMarkAsVisited() is used when mode is "EUseAStarTimeStamp"
	// Is must be called every time a poly is looked at, to ensure that flags from
	// the last pathsearch are cleared out.  Then the poly's m_AStartTimeStamp must
	// be set to the current value
	inline bool ResetPolyFlagsAndMarkAsVisited(TNavMeshPoly * pPoly)
	{
		//static const u32 iMask = ~((u32)(NAVMESHPOLY_OPEN | NAVMESHPOLY_CLOSED | NAVMESHPOLY_REACHEDBY_MASK | NAVMESHPOLY_ALTERNATIVE_STARTING_POLY));

		if(pPoly->m_AStarTimeStamp != m_iAStarTimeStamp)
		{
			// Mask off flags, reset any other variables set per path-search
			pPoly->AndFlags( NAVMESHPOLY_VISITED_RESET_FLAGS_MASK );
			// Set the a-star timestamp
			pPoly->m_AStarTimeStamp = m_iAStarTimeStamp;
			// Set the parent to NULL (necessary?)
			pPoly->m_PathParentPoly = NULL;

#ifdef NAVGEN_TOOL
			pPoly->m_fPathCost = 0.0f;
#endif

#if AWLAYS_MARK_VISTED_POLYS
#if __DEV
			// Store the poly pointer - only for debugging purposes
			m_VisitedPolys[m_Vars.m_iNumVisitedPolys] = pPoly;
#endif
			m_Vars.m_iNumVisitedPolys++;

			// Impose an upper limit on the number of polys which can be involved in a path search.
			// If we don't do this, then we run the risk of bogging down the pathfinder.
			if(m_Vars.m_iNumVisitedPolys == MAX_PATH_VISITED_POLYS)
			{
				m_pCurrentActivePathRequest->m_bRequestActive = false;
				m_pCurrentActivePathRequest->m_bComplete = true;
				m_pCurrentActivePathRequest->m_iNumPoints = 0;
				m_pCurrentActivePathRequest->m_iCompletionCode = PATH_RAN_OUT_VISITED_POLYS_SPACE;
				return false;
			}

			return true;
#else // AWLAYS_MARK_VISTED_POLYS

			m_Vars.m_iNumVisitedPolys++;

#endif
		}
		return true;
	}

	// These functions reset the flags on all visited polys.  The 2nd function resets the input bits.
	void ResetVisitedPolysFlags(void);
	void ResetVisitedPolysFlags(u32 iFlagsToReset);

#if __DEV
	void CheckVisitedPolys(void);
	void ClearDebugMarkedPolys(aiNavDomain domain);
#endif

	// This value is used to timestamp TNavMeshPoly's when doing RefinePathAndCreateNodes()
	// This is because in some borderline cases the algorithm can recurse indefinately -
	// for example, when the edge and path vector are parallel & coincident.
	// This may not need to be an entire 32bits, so long as when the maximum value is
	// reached all TNavMeshPoly's are visited and their timestamps are reset to zero.
	u16 m_iNavMeshPolyTimeStamp;
	u16 m_iAStarTimeStamp;

	bool m_bLosHitInfluenceBoundary;						// whether a TestLineOfSight() has passed from a poly within an influence sphere to one which isn't (or vice-versa)
	bool m_bLosCrossedCoverBoundary;						// whether a TestLineOfSight() has passed from a cover-providing poly to one which doesn't provide cover (or vice-versa)

	// Count number of objects hit during TestDynamicObjectLineOfSight
	int m_iDynamicObjLos_TotalNumObjectsHit;
	// Of those how many were openable
	int m_iDynamicObjLos_NumOpenableObjectsHit;

	static float ms_fMaxMassOfPushableObject;
	static float ms_fMinMassOfSmallSignificant;
	static float ms_fMinVolumeOfSignificant;
	static float ms_fBigVolumeOfSignificant;

	static bank_float ms_fSmallPolySizeWhenTessellating;		// poly below this size will be marked as 'small' by the tessellator (preventing further tessellation)
	static const float ms_fNormalDistBelowToLookForPoly;
	static const float ms_fNormalDistAboveToLookForPoly;
	static bank_float ms_fPreferDownhillScaleFactor;
	static bank_float ms_fWanderTurnWeight;
	static bank_float ms_fWanderTurnPlaneWeight;
	static bank_float ms_fPullOutFromEdgesDistance;
	static bank_float ms_fPullOutFromEdgesDistanceExtra;
	static float ms_fHeightAboveNavMeshForDynObjLOS;					// How high up off the navmesh to raise start/end points for TestDynamicObjectLOS()
	static float ms_fHeightAboveFirstTestFor2ndDynObjLOS;					// If __USE_SECOND_OBJECT_LOS_TEST is set, then this is the height for the 2nd test
	static float ms_fDynObjPlaneEpsilon[NUM_DYNAMIC_OBJECT_PLANES];		// The plane-test epsilon used when doing dynamic-object LOS tests.
	static float ms_fDynObjPlaneEpsilonForceReduced[NUM_DYNAMIC_OBJECT_PLANES];		// The plane-test epsilon used when doing dynamic-object LOS tests.
	static float ms_fDynObjPlaneEpsilonNotReduced[NUM_DYNAMIC_OBJECT_PLANES];		// The plane-test epsilon used when doing dynamic-object LOS tests.
	static Vector3 ms_vDynamicObjectIntersectionPos;					// The closest intersection with a dynamic object (NB: actually, this is not necessarily the closest)

	inline static const float* GetDynObjPlaneEpsilon(const TDynamicObject* pDynObj)
	{
		Assertf(pDynObj, "Ptr null check, hitting this is very bad!");
		if (!pDynObj->m_bForceReducedBoundingBox)
		{
			// Might want a new flag for this but vehicle doors are thinner and would be ignored when using reduced boxes
			// Also we don't want to reduce fire bounds, since peds catch fire instantly as though they are made of straw
			if (!pDynObj->m_bVehicleDoor && !pDynObj->m_bIsFire)	
				return ms_fDynObjPlaneEpsilon;
			
			return ms_fDynObjPlaneEpsilonNotReduced;
		}
		
		return ms_fDynObjPlaneEpsilonForceReduced;
	}


	u32 m_iNumFindPathIterations;
	u32 m_iNumRefinePathIterations;

	TDynamicObject * m_pFirstDynamicObject;
	TDynamicObject * m_DynamicObjectsStore;

	// We cache a small number of grids, because we may get several requests for the
	// same position.  These grids represent static geometry only (dynamic objects are
	// not added here - they are added in CWaveFront) and are therefore valid from
	// frame to frame.
	CWalkRndObjGrid m_WalkRndObjGridCache[WALKRNDOBJGRID_CACHESIZE];

	u32 m_iNumWalkableGridPolys;

#if !__FINAL
	sysPerformanceTimer	* m_PerfTimer;
	sysPerformanceTimer	* m_RequestProcessingOverallTimer;
	sysPerformanceTimer	* m_PolyTessellationTimer;
#endif

	struct TDeferredClimbDeactivation 
	{
		enum Type
		{
			EClimbableObject, ENavMeshClimb, EBoth
		};
		float xyz[3];
		s32 type;
	};

	s32 m_iNumDeferredClimbDeactivations;
	TDeferredClimbDeactivation m_DeferredClimbDeactivations[MAX_DEFERRED_CLIMB_DEACTIVATIONS];

	// The sum total of memory used by the CPathServerThread, including all allocations
	u32 m_iTotalMemoryUsed;

	// The index of the hardware thread which is running the CPathServerThread's worker thread.
	// On Xenon this may be 0..5 inclusive, on a multi-core PC this may be any number up to
	// the number of CPU cores.  On the PS3 this is likely to be unused.
	u32 m_iProcessorIndex;

	//*************************************************************************************
	//	Hierarchical Pathfinding - functions which implement pathfinding across the
	//	CHierarchicalNavNode network which is loaded in at a larger distance than navmesh

	void Hierarchical_GetClosestNodeToPos(const Vector3 & vPos, const float fMaxSearchDistSqr, CHierarchicalNavNode ** ppOut_ClosestNode, CHierarchicalNavNode ** ppOut_ClosestPavementNode, CHierarchicalNavNode ** ppOut_ClosestNonIsolatedNode);

	CHierarchicalNavLink * Hierarchical_GetClosestLinkToPos(const Vector3 & vPos, const float fMaxSearchDistSqr);

	bool Hierarchical_FindPath(CPathRequest * pPathRequest);

	bool Hierarchical_LineOfSight(const Vector3 & vStartPosIn, const Vector3 & vEndPos, THierNodeAndLink * pNodeList, const int iNumNodes, Vector3 * pvIsectPoints=NULL, int * piMaxNumIsectPoints=NULL, int * piNumIsectPoints=NULL);

	void Hierarchical_StringPull(CPathRequest * pPathRequest);
};

#if __NAVMESH_USE_PREFETCHING
#define PREFETCH_ONLY(n) if(CPathServerThread::ms_bUsePrefetching) { n }
#else
#define PREFETCH_ONLY(n)
#endif

#endif	// PATHSERVERTHREAD_H

