#include "PathServer\PathServer.h"

// Rage headers
#include "atl/inmap.h"
#include "system/xtl.h"
#include "vector/geometry.h"

// Framework headers
#include "fwmaths/Vector.h"
#include "fwmaths/random.h"

NAVMESH_OPTIMISATIONS()

bool CPathServerThread::ProcessPathRequest(CPathRequest * pPathRequest)
{
#if !__FINAL

	CPathServer::ms_iNumTessellationsThisPath = 0;
	CPathServer::ms_iNumTestDynamicObjectLOS = 0;
	CPathServer::ms_iNumTestNavMeshLOS = 0;
	CPathServer::ms_iNumGetObjectsIntersectingRegion = 0;
	CPathServer::ms_iNumCacheHitsOnDynamicObjects = 0;

	pPathRequest->m_iNumTessellations = 0;
	pPathRequest->m_iNumTessellatedPolys = 0;
	pPathRequest->m_iNumTestDynamicObjectLOS = 0;

	// Reset timing stats
	pPathRequest->m_fMillisecsToFindPath = 0.0f;
	pPathRequest->m_fMillisecsToFindPolys = 0.0f;
	pPathRequest->m_fMillisecsToMinimisePathLength = 0.0f;
	pPathRequest->m_fMillisecsToPullOutFromEdges = 0.0f;
	pPathRequest->m_fMillisecsToRefinePath = 0.0f;
	pPathRequest->m_fMillisecsToPostProcessZHeights = 0.0f;
	pPathRequest->m_fMillisecsToSmoothPath = 0.0f;
	pPathRequest->m_fMillisecsSpentInTessellation = 0.0f;

	pPathRequest->m_pSavePathFileName = NULL;

	ms_bMarkVisitedPolys = CPathServer::ms_bMarkVisitedPolys;

	for(s32 p=0; p<MAX_NUM_PATH_POINTS+1; p++)
	{
		pPathRequest->m_PathPolys[p] = NULL;
		pPathRequest->m_WaypointFlags[p].Clear();
	}

#if __DEV
	if(CPathServer::ms_bMarkVisitedPolys || CPathServer::ms_bUnMarkVisitedPolys)
	{
		// Marking visible polys really hurts performance, but is a useful debugging tool.
		ClearDebugMarkedPolys(pPathRequest->GetMeshDataSet());
		CPathServer::ms_bUnMarkVisitedPolys = CPathServer::ms_bMarkVisitedPolys;
	}
#endif

#endif	// !__FINAL

	// Determine whether this path starts outside of the area in which we have navmeshes available
	aiNavMeshStore * pNavMeshStore = CPathServer::GetNavMeshStore(pPathRequest->GetMeshDataSet());
	const TNavMeshIndex iStartMesh = CPathServer::GetNavMeshIndexFromPosition(pPathRequest->m_vPathStart, pPathRequest->GetMeshDataSet());
	const bool bPathWithoutNavmesh = (iStartMesh!=NAVMESH_NAVMESH_INDEX_NONE) && (!pNavMeshStore->GetIsMeshInImage(iStartMesh) && !pPathRequest->m_bDynamicNavMeshRoute);

	// If we are outside area where navmesh exists, return a linear path of two points
	// We can also enable this with 'm_bDisablePathFinding' to remove any pathfinding CPU cost
	// from profiles, etc.
	
	if(bPathWithoutNavmesh || CPathServer::m_bDisablePathFinding)
	{
		pPathRequest->m_iNumPoints = 2;
		pPathRequest->m_WaypointFlags[0].Clear();
		pPathRequest->m_WaypointFlags[1].Clear();

		pPathRequest->m_PathPoints[0] = pPathRequest->m_vPathStart;

		if(pPathRequest->m_bFleeTarget)
		{
			Vector3 vFleeDir = pPathRequest->m_vPathStart - pPathRequest->m_vPathEnd;
			vFleeDir.Normalize();
			pPathRequest->m_PathPoints[1] = pPathRequest->m_vPathStart + (vFleeDir * pPathRequest->m_fReferenceDistance);
		}
		else if(pPathRequest->m_bWander)
		{
			Vector3 vWander = pPathRequest->m_vReferenceVector * pPathRequest->m_fReferenceDistance;
			pPathRequest->m_PathPoints[1] = pPathRequest->m_vPathStart + vWander;
		}
		else
		{
			pPathRequest->m_PathPoints[1] = pPathRequest->m_vPathEnd;
		}

		pPathRequest->m_iCompletionCode = PATH_FOUND;
		return true;
	}



#if SANITY_CHECK_TESSELLATION
//	Assert(!CPathServer::m_TessellationPolysInUse.AreAnySet());
#endif

	CPathServerThread::IncAStarTimeStamp();

#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	Assertf( pPathRequest->m_fEntityRadius >= PATHSERVER_PED_RADIUS, "pPathRequest->m_fEntityRadius : %.4f, PATHSERVER_PED_RADIUS : %.2f", pPathRequest->m_fEntityRadius, PATHSERVER_PED_RADIUS );

	pPathRequest->m_fEntityRadius = Max( pPathRequest->m_fEntityRadius, PATHSERVER_PED_RADIUS);

	// Set the ms_fDynObjPlaneEpsilon[] values before we do any tests on dynamic objects.
	// This value can be set to effectively reduce the bounding-box size of objects,
	// so that the pathfinder has a better chance of getting past them (use if the 1st
	// attempt to find a path has failed).
	// Note - it is set for each side plane of the object, whilst planes 4 & 5 (top & bottom)
	// always remain the same.
	float fPlaneEpsilon = (pPathRequest->m_bReduceObjectBoundingBoxes) ? (CPathServer::ms_fDefaultDynamicObjectPlaneEpsilon + PATHSERVER_PED_RADIUS) : CPathServer::ms_fDefaultDynamicObjectPlaneEpsilon;
	float fPlaneEpsilonReduced = CPathServer::ms_fDefaultDynamicObjectPlaneEpsilon + PATHSERVER_PED_RADIUS;
	float fPlaneEpsilonNotReduced = CPathServer::ms_fDefaultDynamicObjectPlaneEpsilon;
	// Reduce epsilon to account for the pathfinding entity's radius
	fPlaneEpsilon -= (pPathRequest->m_fEntityRadius - PATHSERVER_PED_RADIUS);
	fPlaneEpsilonReduced -= (pPathRequest->m_fEntityRadius - PATHSERVER_PED_RADIUS);
	fPlaneEpsilonNotReduced -= (pPathRequest->m_fEntityRadius - PATHSERVER_PED_RADIUS);

	ms_fDynObjPlaneEpsilon[0] = fPlaneEpsilon;
	ms_fDynObjPlaneEpsilon[1] = fPlaneEpsilon;
	ms_fDynObjPlaneEpsilon[2] = fPlaneEpsilon;
	ms_fDynObjPlaneEpsilon[3] = fPlaneEpsilon;

	ms_fDynObjPlaneEpsilonForceReduced[0] = fPlaneEpsilonReduced;
	ms_fDynObjPlaneEpsilonForceReduced[1] = fPlaneEpsilonReduced;
	ms_fDynObjPlaneEpsilonForceReduced[2] = fPlaneEpsilonReduced;
	ms_fDynObjPlaneEpsilonForceReduced[3] = fPlaneEpsilonReduced;

	ms_fDynObjPlaneEpsilonNotReduced[0] = fPlaneEpsilonNotReduced;
	ms_fDynObjPlaneEpsilonNotReduced[1] = fPlaneEpsilonNotReduced;
	ms_fDynObjPlaneEpsilonNotReduced[2] = fPlaneEpsilonNotReduced;
	ms_fDynObjPlaneEpsilonNotReduced[3] = fPlaneEpsilonNotReduced;

	//---------------------------------------------------------------------------------
	// Determine the maximum search extents for this path search
	pPathRequest->m_fReferenceDistance = pPathRequest->m_fInitialReferenceDistance;

	CalculateSearchExtents( pPathRequest, m_Vars.m_PathSearchDistanceMinMax, m_Vars.m_vSearchExtentsMin, m_Vars.m_vSearchExtentsMax );

	// If we are using a regular 'shortest path' pathfinding, and the target is not within the search
	// extents, then quit immediately
	if(!pPathRequest->m_bFleeTarget && !pPathRequest->m_bWander)
	{
		if( !(m_Vars.m_vSearchExtentsMin.IsLessOrEqualThanAll(pPathRequest->m_vPathStart) && pPathRequest->m_vPathStart.IsLessOrEqualThanAll(m_Vars.m_vSearchExtentsMax) ) )
		{
			pPathRequest->m_iCompletionCode = PATH_NOT_FOUND;
			return false;
		}
	}

	// Must always call this to freeze objects positions at this point in time

	RemoveAndAddObjects();
	UpdateAllDynamicObjectsPositions();

	if( CPathServer::ms_bUseGridCellCache )
	{
		CDynamicObjectsContainer::InitGridCellsCache(pPathRequest->m_vPathStart, m_Vars.m_PathSearchDistanceMinMax);
	}
	else
	{
		CDynamicObjectsContainer::InvalidateCache();
	}

	//-----------------------------------------------------------------------------------------
	// If we are pathfinding with an actor whose radius is larger than PATHSERVER_PED_RADIUS then we must
	// expand the TShortMinMax in all dynamic objects which are intersecting the path search - this is so
	// that coarse-grained minmax/minmax culling sees these objects as intersecting the entity.
	// After the pathsearch is finished, we must then contract them again.

	if(pPathRequest->m_bUseVariableEntityRadius)
	{
		const float fRadiusDelta = pPathRequest->m_fEntityRadius - PATHSERVER_PED_RADIUS;
		Assert(fRadiusDelta > 0.0f);
		
		TShortMinMax dummyMinMax;
		CDynamicObjectsContainer::AdjustAllBoundsByAmount(dummyMinMax, fRadiusDelta, true);
		pPathRequest->m_bHasAdjustedDynamicObjectsMinMaxForWidth = true;
	}

	// If we are flagging non-uprootable objects as not an obstacle, then ensure next time a path-request
	// is serviced that we go through and reset that "m_bIsCurrentlyAnObstacle" for all objects.
	if(m_pCurrentActivePathRequest->m_bIgnoreNonSignificantObjects)
	{
		CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;
	}

	//******************************************************************************************************
	// Store the original start/end positions.  The actual pPathRequest->m_vPathStart/End positions will
	// maybe be adjusted by the pathfinder.  After we've found a path we can then compare the final point
	// with the unadjusted point, and if there is a difference then we must add on the unadjusted position
	// at the end.
	//******************************************************************************************************

	pPathRequest->m_vPathStart = pPathRequest->m_vUnadjustedStartPoint;
	pPathRequest->m_vPathEnd = pPathRequest->m_vUnadjustedEndPoint;

	m_Vars.m_vUnadjustedPathStart = pPathRequest->m_vPathStart;
	m_Vars.m_vUnadjustedPathEnd = pPathRequest->m_vPathEnd;

	// Jittering leads to buggy paths, so is disabled
	static const bool bDoLastResortJittering = false;
	// This helps avoid us pushing the start/end positions through walls, etc
	static const bool bPerformLosToPosClearOfObjects = false;//true;
	// We might want to quit if we are trapped amongst objects.  Disabled currently as a test 3/5/07
	static const bool bBailOutIfStartPosIsInsideAnObject = false;

	u32 iStartNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;
	u32 iEndNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;

	if(pPathRequest->m_bDynamicNavMeshRoute && pPathRequest->m_iIndexOfDynamicNavMesh != NAVMESH_NAVMESH_INDEX_NONE)
	{
		iStartNavMeshIndex = pPathRequest->m_iIndexOfDynamicNavMesh;
		iEndNavMeshIndex = pPathRequest->m_iIndexOfDynamicNavMesh;
	}

	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	// If not within a dynamic navmesh, then get the index of the regular navmesh under the start.
	// We will also try to move the start position out of collision with dynamic objects.
	// We can't do this currently for positions on dynamic navmeshes in case we exit the navmesh itself.
	if(iStartNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
	{
		// No need to do this if not avoiding objects
		// Don't move start position if explicitly told not to
		if(!pPathRequest->m_bDontAvoidDynamicObjects && pPathRequest->m_fMaxDistanceToAdjustPathStart > 0.0f)
		{
			// 1st attempt will push position outside of all encountered objects
			bool bStartClearOfObjects = FindPositionClearOfDynamicObjects(pPathRequest->m_vPathStart, true, false, bPerformLosToPosClearOfObjects, domain);

			// If moved position was outside of our max adjustment distance, revert to original path start position
			const float fDistSqr = (pPathRequest->m_vUnadjustedStartPoint - pPathRequest->m_vPathStart).XYMag2();
			if(fDistSqr > pPathRequest->m_fMaxDistanceToAdjustPathStart*pPathRequest->m_fMaxDistanceToAdjustPathStart)
			{
				pPathRequest->m_vPathStart = pPathRequest->m_vUnadjustedStartPoint;
			}
			else
			{
				if(!bStartClearOfObjects && bBailOutIfStartPosIsInsideAnObject)
				{
					pPathRequest->m_iCompletionCode = PATH_ENDPOINTS_INSIDE_OBJECTS;
					pPathRequest->m_iNumPoints = 0;
					pPathRequest->m_bComplete = true;
					pPathRequest->m_bRequestActive = false;
					return false;
				}

				pPathRequest->m_PathResultInfo.m_bStartPositionWasInsideAnObject =
					!bStartClearOfObjects || !pPathRequest->m_vPathStart.IsClose(pPathRequest->m_vUnadjustedStartPoint, 0.25f);
			}
		}

		iStartNavMeshIndex = CPathServer::GetNavMeshIndexFromPosition(pPathRequest->m_vPathStart, domain);
	}
	if(iStartNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
	{
		pPathRequest->m_iCompletionCode = PATH_INVALID_COORDINATES;
		pPathRequest->m_iNumPoints = 0;
		pPathRequest->m_bComplete = true;
		pPathRequest->m_bRequestActive = false;
		return false;
	}

	// Get the index of the navmesh under the end.
	// If the end is not within a dynamic navmesh, then try to move it outside of any intersecting objects.
	// We can't do this currently for positions on dynamic navmeshes in case we exit the navmesh itself.
	if(iEndNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
	{
		// No need to do this if not avoiding objects
		// Don't move end position if explicitly told not to
		if(!pPathRequest->m_bDontAvoidDynamicObjects && pPathRequest->m_fMaxDistanceToAdjustPathEnd > 0.0f)
		{
			FindPositionClearOfDynamicObjects(pPathRequest->m_vPathEnd, false, true, bPerformLosToPosClearOfObjects, domain);

			// If moved position was outside of our max adjustment distance, revert to original path end position
			const float fDistSqr = (pPathRequest->m_vUnadjustedEndPoint - pPathRequest->m_vPathEnd).XYMag2();
			if(fDistSqr > pPathRequest->m_fMaxDistanceToAdjustPathEnd*pPathRequest->m_fMaxDistanceToAdjustPathEnd)
			{
				pPathRequest->m_vPathEnd = pPathRequest->m_vUnadjustedEndPoint;
			}
		}

		iEndNavMeshIndex = CPathServer::GetNavMeshIndexFromPosition(pPathRequest->m_vPathEnd, domain);
	}
	if(iEndNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
	{
		pPathRequest->m_iCompletionCode = PATH_INVALID_COORDINATES;
		pPathRequest->m_iNumPoints = 0;
		pPathRequest->m_bComplete = true;
		pPathRequest->m_bRequestActive = false;
		return false;
	}

	// Get the start navmesh
	CNavMesh * pStartNavMesh = CPathServer::GetNavMeshFromIndex(iStartNavMeshIndex, domain);

	// Get the end navmesh
	CNavMesh * pEndNavMesh = CPathServer::GetNavMeshFromIndex(iEndNavMeshIndex, domain);


#if __DEV
	// We can't have both start & end points on different dynamic objects, that would be just crazy!
	if((pStartNavMesh && (pStartNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC)) && (pEndNavMesh && (pEndNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC)))
	{
		Assertf(pStartNavMesh->GetIndexOfMesh() == pEndNavMesh->GetIndexOfMesh(), "Can't pathfind from one dynamic navmesh onto another.");
	}
#endif

	// Ignore dynamic object which owns the start navmesh IF that is a dynamic navmesh
	if(pStartNavMesh && (pStartNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC))
	{
		Assert(pStartNavMesh == pEndNavMesh);

		const TDynamicObjectIndex iObjIndex = pPathRequest->m_PathResultInfo.m_iDynObjIndex;
		if(iObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
		{
			m_DynamicObjectsStore[iObjIndex].m_bIsCurrentlyAnObstacle = false;
		}

		pStartNavMesh->SetMatrix( pPathRequest->m_PathResultInfo.m_MatrixOfEntityAtPathCreationTime );
	}

	//-----------------------------------------------------------------------------------
	// If we've specified that we want to find the closest loaded navmesh, then try to
	// Also adjust the target position to be near the centre of this navmesh

	if(pPathRequest->m_bGoAsFarAsPossibleIfNavMeshNotLoaded && !pEndNavMesh)
	{
		// Roughly protect against the very rare chance of meshes being loaded/unloaded during a path request.
		if(CPathServer::ms_bRunningRequestAndEvictMeshes)
		{
			pPathRequest->m_iCompletionCode = PATH_ABORTED_DUE_TO_ANOTHER_THREAD;
			return false;
		}

		CNavMesh * pClosestLoadedNavMesh = CPathServer::FindClosestLoadedNavMeshToPosition(pPathRequest->m_vPathEnd, domain);
		if(pClosestLoadedNavMesh)
		{
			Assert(pClosestLoadedNavMesh->GetQuadTree());
			const Vector3 & vNavMin = pClosestLoadedNavMesh->GetQuadTree()->m_Mins;
			const Vector3 & vNavMax = pClosestLoadedNavMesh->GetQuadTree()->m_Maxs;

			// Find the position along the edge of the navmesh which is closest to the target
			const Vector3 vNavMeshEdgePts[4] =
			{
				Vector3(vNavMin.x, vNavMin.y, pPathRequest->m_vPathEnd.z),
				Vector3(vNavMax.x, vNavMin.y, pPathRequest->m_vPathEnd.z),
				Vector3(vNavMax.x, vNavMax.y, pPathRequest->m_vPathEnd.z),
				Vector3(vNavMin.x, vNavMax.y, pPathRequest->m_vPathEnd.z)
			};
			Vector3 vClosestPts[4];
			float fClosestDist2 = FLT_MAX;
			int iClosest = -1;
			for(int e=0; e<4; e++)
			{
				const Vector3 & vEdge1 = vNavMeshEdgePts[e];
				const Vector3 & vEdge2 = vNavMeshEdgePts[(e+1)%4];
				const Vector3 vDiff = vEdge2-vEdge1;
				const float fTVal = (vDiff.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vEdge1, vDiff, pPathRequest->m_vPathEnd) : 0.0f;
				vClosestPts[e] = vEdge1 + ((vEdge2 - vEdge1)*fTVal);
				const float fDist2 = (vClosestPts[e] - pPathRequest->m_vPathEnd).Mag2();
				if(fDist2 < fClosestDist2)
				{
					fClosestDist2 = fDist2;
					iClosest = e;
				}
			}

			Vector3 vNewTarget;
			static const float fFindPolyRadius = 250.0f;

			u32 iFlags = GetClosestPos_CalledFromPathfinder;
			if(!pPathRequest->m_bDontAvoidDynamicObjects)
				iFlags |= GetClosestPos_ClearOfObjects;

			const EGetClosestPosRet eRet = CPathServer::GetClosestPositionForPed(vClosestPts[iClosest], vNewTarget, fFindPolyRadius, iFlags, false, 0, NULL, domain);
			if(eRet!=ENoPositionFound)
			{
				pEndNavMesh = pClosestLoadedNavMesh;
				pPathRequest->m_vPathEnd = vNewTarget;

				if(!pPathRequest->m_bDontAvoidDynamicObjects)
				{
					bool bFirstAttemptSucceeded = FindPositionClearOfDynamicObjects(pPathRequest->m_vPathEnd, false, false, false, domain);
					if(!bFirstAttemptSucceeded)
						FindPositionClearOfDynamicObjects(pPathRequest->m_vPathEnd, bDoLastResortJittering, true, false, domain);
				}

				pPathRequest->m_fPathSearchCompletionRadius = 2000.0f;
				pPathRequest->m_PathResultInfo.m_bWentFarAsPossibleTowardsTarget = true;
			}
		}
	}

#if !__FINAL
	m_PerfTimer->Stop();
	pPathRequest->m_fMillisecsToFindPolys = (float) m_PerfTimer->GetTimeMS();
#endif

	CPathServer::EObjectAvoidanceMode eObjAvoidMode = CPathServer::m_eObjectAvoidanceMode;

	if(pStartNavMesh && (pEndNavMesh || pPathRequest->m_bFleeTarget || pPathRequest->m_bWander))
	{
		m_pCurrentActivePathRequest = pPathRequest;

		if(pPathRequest->m_bDontAvoidDynamicObjects)
		{
			CPathServer::m_eObjectAvoidanceMode = CPathServer::ENoObjectAvoidance;
		}

		// If path is to be found in 'extra-thorough' mode, then we will tessellate any polygon
		// which is near a dymanic object - regardless of whether a passage can be found past
		// it without tessellation.  This helps avoid common failure cases.
//		Assertf(pPathRequest->m_bDoExtraThoroughPathSearch, "not implemented currently");
//		if(pPathRequest->m_bDoExtraThoroughPathSearch)
//			CPathServer::ms_bDoPreemptiveTessellation = true;

		m_PathRequestSleepTimer->Reset();
		m_PathRequestSleepTimer->Start();

		// Try to find a poly path
		const EPathFindRetVal eRetVal = FindPath(pStartNavMesh, pEndNavMesh, pPathRequest);

		m_PathRequestSleepTimer->Stop();

#if !__FINAL
		pPathRequest->m_iNumVisitedPolygons = m_Vars.m_iNumVisitedPolys;
		pPathRequest->m_iNumInitiallyFoundPolys = m_Vars.m_iNumPathPolys;
		m_PerfTimer->Stop();
		pPathRequest->m_fMillisecsToFindPath = (float) m_PerfTimer->GetTimeMS();
#endif
#if !__FINAL
		// Make a note of how many times we had to call TessellateNavMeshPoly()
		pPathRequest->m_iNumTessellations = CPathServer::ms_iNumTessellationsThisPath;
		pPathRequest->m_iNumTessellatedPolys = CPathServer::m_iCurrentNumTessellationPolys;
		pPathRequest->m_iNumTestDynamicObjectLOS = CPathServer::ms_iNumTestDynamicObjectLOS;
		pPathRequest->m_iNumTestNavMeshLOS = CPathServer::ms_iNumTestNavMeshLOS;
		pPathRequest->m_iNumGetObjectsIntersectingRegion = CPathServer::ms_iNumGetObjectsIntersectingRegion;
		pPathRequest->m_iNumCacheHitsOnDynamicObjects = CPathServer::ms_iNumCacheHitsOnDynamicObjects;

#if __DEV
		if(m_eVisitedPolyMarkingMode == EUseBothMethods)
		{
			CheckVisitedPolys();
		}
#endif

#endif

		static bool bDisableSmoothing=false;

		if(eRetVal == EPathNotFound)
		{
#if __DEV
			if(m_eVisitedPolyMarkingMode == EUseBothMethods)
			{
				CheckVisitedPolys();
			}
			// Reset the flags on visited polys
			if(m_eVisitedPolyMarkingMode != EUseAStarTimeStamp)
			{
				ResetVisitedPolysFlags();
			}
#endif
			// Reset back to default
			CPathServer::m_eObjectAvoidanceMode = eObjAvoidMode;
			return false;
		}
		if(eRetVal == EPathFoundEarlyOut)
		{
			// Reset back to default
			CPathServer::m_eObjectAvoidanceMode = eObjAvoidMode;
			return true;
		}

		// Refine the path to a minimal set of way-points
		bool bRefinedPathOk = RefinePathAndCreateNodes(pPathRequest, pPathRequest->m_vPathStart, pPathRequest->m_vPathEnd, MAX_NUM_PATH_POINTS);
		if(!bRefinedPathOk)
		{
			CPathServer::m_eObjectAvoidanceMode = eObjAvoidMode;
			return false;
		}

#if __ASSERT
		for(s32 t=0; t<pPathRequest->m_iNumPoints; t++)
		{
			Assert( rage::FPIsFinite(pPathRequest->m_PathPoints[t].x) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].y) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].z) );
		}
#endif

		{
#if !__FINAL
			m_PerfTimer->Stop();
			pPathRequest->m_fMillisecsToRefinePath = (float) m_PerfTimer->GetTimeMS();
#endif
		}

		if(pPathRequest->m_iNumPoints < 2)
		{
			pPathRequest->m_iNumPoints = 0;
			pPathRequest->m_iCompletionCode = PATH_NO_POINTS;
			return false;
		}

		if(CPathServer::ms_bMinimisePathDistance)
		{
			MinimisePathDistance(pPathRequest);
		}

		if(CPathServer::ms_bPullPathOutFromEdges)
		{
			PullPathOutFromEdges(pPathRequest, CPathServer::ms_bPullPathOutFromEdgesStringPull);
		}

		if(CPathServer::ms_bPullPathOutFromEdgesTwice)
		{
			PullPathOutFromEdges(pPathRequest, CPathServer::ms_bPullPathOutFromEdgesStringPull);
		}

		if(pPathRequest->m_bDoPostProcessToPreserveSlopeInfo)
		{
			AddWaypointsToPreserveHeightChanges(pPathRequest);
		}

		// Minimise the path distance, by modifying the position of the points within the polygons they are in
		if(pPathRequest->m_bSmoothSharpCorners && !bDisableSmoothing)
		{
			if(CPathServer::ms_bSmoothRoutesUsingSplines)
			{
				CalcPathBezierControlPoints(pPathRequest);
			}
		}

		//*********************************************************************************************
		// Check whether the last point on the path actually IS the original unadjusted path endpoint.
		// If not, and we have enough space in the path to add the final point - then do so.
		//*********************************************************************************************

		static const float fMaxXYDistSqrFromEnd = (0.5f * 0.5f);

		const Vector3 vDeltaLastPointToUnadjustedEnd = pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints-1] - m_Vars.m_vUnadjustedPathEnd;
		const Vector3 vDeltaLastPointPathEnd = pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints-1] - pPathRequest->m_vPathEnd;
		const float fXYDistSqrToUnadjustedEnd = vDeltaLastPointToUnadjustedEnd.XYMag2();
		const float fXYDistSqrToPathEnd = vDeltaLastPointPathEnd.XYMag2();

		if(!pPathRequest->m_bWander && !pPathRequest->m_bFleeTarget && fXYDistSqrToUnadjustedEnd > fMaxXYDistSqrFromEnd)
		{
			if(pPathRequest->m_iNumPoints < MAX_NUM_PATH_POINTS && fXYDistSqrToPathEnd < fMaxXYDistSqrFromEnd)
			{
				// If the path was found without exceeding the max num waypoints, but didn't end at the target.
				// Set the flag which indicates that this waypoint is the adjusted endpoint

				pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints-1].m_iSpecialActionFlags |= WAYPOINT_FLAG_ADJUSTED_TARGET_POS;
			}
			else if(!pPathRequest->m_PathResultInfo.m_bUsedCompletionRadius)
			{
				// If the path used up all available waypoints, and didn't end at the target - and this wasn't due to an
				// alternative completion radius being used - then flag that this last waypoint wasn't the target itself.

				pPathRequest->m_PathResultInfo.m_bLastRoutePointIsTarget = false;
			}
		}

		// Ensure that final waypoint doesn't have any climb/drop flags set
		if(pPathRequest->m_iNumPoints > 0)
		{
			static const u32 iBits = WAYPOINT_FLAG_CLIMB_HIGH|WAYPOINT_FLAG_DROPDOWN|WAYPOINT_FLAG_CLIMB_LOW|WAYPOINT_FLAG_CLIMB_LADDER|WAYPOINT_FLAG_DESCEND_LADDER|WAYPOINT_FLAG_CLIMB_OBJECT;
			pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints-1].m_iSpecialActionFlags &= ~iBits;
		}

#if __DEV
		// Reset the flags on visited polys
		if(m_eVisitedPolyMarkingMode!=EUseAStarTimeStamp)
		{
			ResetVisitedPolysFlags();
		}
#endif

		// Reset back to default
		CPathServer::m_eObjectAvoidanceMode = eObjAvoidMode;


#if NAVMESH_OPTIMISATIONS_OFF && __ASSERT
		if( pPathRequest->m_iNumPoints < 2 && pPathRequest->m_pSavePathFileName == NULL)
		{
			pPathRequest->m_pSavePathFileName = "x:\\gta5\\build\\dev\\909543.prob";

			Displayf( "Less than 2 points in path..\n");
			Displayf( "m_vPathStart (%.2f, %.2f, %.2f)\n", pPathRequest->m_vPathStart.x, pPathRequest->m_vPathStart.y, pPathRequest->m_vPathStart.z);
			Displayf( "m_vPathEnd (%.2f, %.2f, %.2f)\n", pPathRequest->m_vPathEnd.x, pPathRequest->m_vPathEnd.y, pPathRequest->m_vPathEnd.z);
			Displayf( "m_iNumPoints %i \n", pPathRequest->m_iNumPoints);
			Displayf( "m_PathPolys[0] 0x%p", pPathRequest->m_PathPolys[0]);
			Displayf( "m_PathPolys[1] 0x%p", pPathRequest->m_PathPolys[1]);
			Displayf( "m_iNumInitiallyFoundPolys %i", pPathRequest->m_iNumInitiallyFoundPolys);
			Displayf( "m_WaypointFlags[0] %u", pPathRequest->m_WaypointFlags[0].GetSpecialActionFlags());
			Displayf( "m_WaypointFlags[1] %u", pPathRequest->m_WaypointFlags[1].GetSpecialActionFlags());

			Assertf(pPathRequest->m_iNumPoints >= 2, "Navmesh path being returned with less than 2 points.  This is an error.  see (url:bugstar:909543).  Please include the TTY as it will have some more details and attach the file \"%s\" after closing this dialog box.", pPathRequest->m_pSavePathFileName);
		}
#endif

#if __ASSERT
		for(s32 t=0; t<pPathRequest->m_iNumPoints; t++)
		{
			Assert( rage::FPIsFinite(pPathRequest->m_PathPoints[t].x) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].y) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].z) );
		}
#endif

#if NAVMESH_OPTIMISATIONS_OFF && __ASSERT
		if(pPathRequest->m_iNumPoints >= 2)
		{
			const Vector3 vDiff = pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints-1] - pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints-2];
			Assert(vDiff.Mag2() > SMALL_FLOAT);
		}
#endif

#if !__FINAL && __BANK && defined(GTA_ENGINE)
		if(pPathRequest->m_pSavePathFileName != NULL)
		{
			CPathServer::OutputPathRequestProblem(pPathRequest, pPathRequest->m_pSavePathFileName);
		}
#endif
	}

	return true;
}

