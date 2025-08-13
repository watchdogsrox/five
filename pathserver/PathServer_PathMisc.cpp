#include "PathServer\PathServer.h"

// Rage headers
#include "atl\inmap.h"
#include "system/xtl.h"

// Framework headers
#include "ai/navmesh/priqueue.h"
#include "fwmaths\Vector.h"
#include "fwmaths/random.h"


NAVMESH_OPTIMISATIONS()

#define MAX_NUM_TESSELLATED_FRAGMENTS			768		// was 512	// was 256	// was 64

atArray<TNavMeshPoly*> startPolys(0,MAX_NUM_TESSELLATED_FRAGMENTS);
atArray<TNavMeshPoly*> tessellatedStartPolys(0,MAX_NUM_TESSELLATED_FRAGMENTS);

// The distances from adjacent waypoints towards the corner-point, for vCtrlPt2 and vCtrlPt3
//const float g_fPathSplineDists[PATHSPLINE_NUM_TEST_DISTANCES] = { 0.0f, 0.25f, 0.5f, 0.9f };
const float g_fPathSplineDists[PATHSPLINE_NUM_TEST_DISTANCES] = { 0.1f, 0.25f, 0.5f, 0.9f };
// The magnitude that the outer control pts are pulled back (scales the length of the line-seg)
const float g_fOuterCtrlPtDists[PATHSPLINE_NUM_TEST_DISTANCES] = { 1.0f, 0.75f, 0.5f, 0.25f };

namespace
{
	bool CalcCornerPushDir(Vector3& vPushDir, const Vector3& vPrevPt, const Vector3& vMidPt, const Vector3& vNextPt, float fPushThreshold)
	{
		Vector3 vEntry = vMidPt - vPrevPt;
		vEntry.z = 0.f;
		vEntry.Normalize();

		Vector3 vExit = vMidPt - vNextPt;
		vExit.z = 0.f;
		vExit.Normalize();

		if (abs(vEntry.Dot(vExit)) < fPushThreshold)
		{
			vPushDir = vEntry + vExit;
			vPushDir.Normalize();
			return true;
		}

		return false;
	}

	/* Not sued in the code base.  Orbis complains about it 
	TNavMeshPoly* DigOutPolyBelowPoint(const Vector3& vNewPathPos, aiNavDomain domain)
	{
		const u32 iStartNavMesh = CPathServer::GetNavMeshIndexFromPosition(vNewPathPos, domain);
		if (iStartNavMesh == NAVMESH_NAVMESH_INDEX_NONE)
			return NULL;

		CNavMesh * pStartNavMesh = CPathServer::GetNavMeshFromIndex(iStartNavMesh, domain);
		if (!pStartNavMesh)
			return NULL;

		Vector3 vIntersectPoint;
		const u32 iStartPolyIndex = pStartNavMesh->GetPolyBelowPoint(vNewPathPos + ZAXIS, vIntersectPoint, 5.0f);
		if (iStartPolyIndex == NAVMESH_POLY_INDEX_NONE)
			return NULL;

		return pStartNavMesh->GetPoly(iStartPolyIndex);
	}
	*/

	bool IsPointsMakingAQuirk(const Vector3& vPrevPrevPt, const Vector3& vPrevPt, const Vector3& vThisPt, const Vector3& vNextPt, float UNUSED_PARAM(fTolerance))
	{
		Vector3 vToPrev = vPrevPt - vPrevPrevPt;
		vToPrev.z = 0.f;
		vToPrev.Normalize();

		Vector3 vToThis = vThisPt - vPrevPt;
		vToThis.z = 0.f;
		vToThis.Normalize();

		Vector3 vToNext = vNextPt - vThisPt;
		vToNext.z = 0.f;
		vToNext.Normalize();

	//	if (vToPrev.Dot(vToThis) < fTolerance && vToThis.Dot(vToNext) < fTolerance)
		{
			// Change of direction?
			if ((int)Sign(-CrossProduct(vToThis, vToNext).z) == (int)Sign(-CrossProduct(vToPrev, vToThis).z))
				return false;

			return true;
		}

	//	return false;
	}
}

//*************************************************************************************************
// This function tessellates polys around the path start, and adds them to the priority queue
// ready for the pathfinding algorithm to use.  It only does this if there are any dynamic
// objects in the vicinity of the start position.  The original (untessellated) start poly is
// already in the priority queue at this time, and if it becomes tessellated here it will be
// marked as "replaced by tessellation" & subsequently ignored by the pathfinder when encountered.
//*************************************************************************************************

bool CPathServerThread::MaybeTessellateAndMarkPolysSurroundingPathStart(CNavMesh * pNavMesh, CPathRequest * pPathRequest, const float fInitialCost)
{
	dev_float fNearbyPolyRadius = (pPathRequest->m_bExpandStartEndPolyTessellationRadius ? 2.5f : 0.25f);
	static const float fStartPosRadius = 0.25f;		
	static const float fZSize = 5.0f;				

	// This tesselation stuff is only for the normal navigation domain for now.
	const aiNavDomain domain = kNavDomainRegular;

	const bool bUseMultipleStartPolys = DoesMinMaxIntersectAnyDynamicObjectsApproximate(m_Vars.m_pStartPoly->m_MinMax);

	if(bUseMultipleStartPolys)
	{
		TShortMinMax startPosMinMax;
		startPosMinMax.SetFloat(
			pPathRequest->m_vPathStart.x - fStartPosRadius, pPathRequest->m_vPathStart.y - fStartPosRadius, pPathRequest->m_vPathStart.z - fZSize,
			pPathRequest->m_vPathStart.x + fStartPosRadius, pPathRequest->m_vPathStart.y + fStartPosRadius, pPathRequest->m_vPathStart.z + fZSize
			);

		startPolys.clear();
		startPolys.Append()=m_Vars.m_pStartPoly;

		const float fAlternativeStartPenalty = 50.0f;

		// Get a list of polys which are very close to the start position.
		GetMultipleNavMeshPolysForRouteEndPoints(pPathRequest, m_Vars.m_pStartPoly, pNavMesh, pPathRequest->m_vPathStart, fNearbyPolyRadius, startPolys, domain);

		for(int u=0; u<startPolys.GetCount(); u++)
		{
			TNavMeshPoly * pNavMeshPoly = startPolys[u];

			CPathServerThread::OnFirstVisitingPoly(pNavMeshPoly);

			if(pNavMeshPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY) ||
				pNavMeshPoly->GetReplacedByTessellation())
				continue;

			s32 tp;
			tessellatedStartPolys.clear();
			tessellatedStartPolys.Append() = pNavMeshPoly;

			for(tp=0; tp<tessellatedStartPolys.GetCount(); tp++)
			{
				TNavMeshPoly * pTessPoly = tessellatedStartPolys[tp];

				// This should never return FALSE, as we will never reach max visited polys at this early stage
				OnFirstVisitingPoly(pTessPoly);

				Assert(!(pTessPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY)));

				Assert(!pTessPoly->GetReplacedByTessellation());
				CNavMesh * pTessNavMesh = CPathServer::GetNavMeshFromIndex(pTessPoly->GetNavMeshIndex(), domain);

				if(fwPathServer::CanTessellateThisPoly(pTessPoly) &&
					fwPathServer::IsRoomToTessellateThisPoly(pTessPoly))
				{
					CPathServerThread::ms_dynamicObjectsIntersectingPolygons.clear();
					CDynamicObjectsContainer::GetObjectsIntersectingRegion(pTessPoly->m_MinMax, CPathServerThread::ms_dynamicObjectsIntersectingPolygons, TDynamicObject::FLAG_IGNORE_OPENABLE|TDynamicObject::FLAG_IGNORE_NOT_OBSTACLE);

					if(CPathServerThread::ms_dynamicObjectsIntersectingPolygons.GetCount())
					{
						Assert(!(pTessPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY)));

						const bool bOk = TessellateNavMeshPolyAndCreateDegenerateConnections(
							pTessNavMesh, pTessPoly,
							pTessNavMesh->GetPolyIndex(pTessPoly),
							(pTessPoly == m_Vars.m_pStartPoly),		// don't bother updating m_pStartPoly when we're using multiple starting polys
							(pTessPoly == m_Vars.m_pEndPoly),		// but we do want to update the m_pEndPoly
							NULL, NAVMESH_NAVMESH_INDEX_NONE, NAVMESH_POLY_INDEX_NONE,
							&tessellatedStartPolys
							);

						NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(bOk); );
						
						if(!bOk)
						{
							if(!m_PathSearchPriorityQueue->GetNodeCount())
							{
								Assert(m_Vars.m_pStartPoly);
								if(!m_Vars.m_pStartPoly)
									return false;

								m_PathSearchPriorityQueue->Insert(fInitialCost, 0.0f, m_Vars.m_pStartPoly, pPathRequest->m_vPathStart, m_Vars.m_vDirFromPrevious, 0, NAVMESH_POLY_INDEX_NONE);
							}
							return true;
						}

						// This should never return FALSE, as we will never reach max visited polys at this early stage
						OnFirstVisitingPoly(pTessPoly);
						tessellatedStartPolys.Delete(tp);
						tp--;

						if(!m_Vars.m_pStartPoly)
						{
							float fClosestEdgeDistSqr = FLT_MAX;
							TNavMeshPoly * pClosestPoly = NULL;
							Vector3 vClosestPt;
							for(tp=0; tp<tessellatedStartPolys.GetCount(); tp++)
							{
								pTessPoly = tessellatedStartPolys[tp];
								if(pTessPoly->GetIsTessellatedFragment())
								{
									const s32 iEdge = fwPathServer::GetTessellationNavMesh()->GetPolyClosestEdgeMidToPoint(
										pPathRequest->m_vPathStart, pTessPoly, &vClosestPt);
									if(iEdge >= 0)
									{
										const float fDistSqr = (vClosestPt-pPathRequest->m_vPathStart).XYMag2();
										if(fDistSqr < fClosestEdgeDistSqr)
										{
											fClosestEdgeDistSqr = fDistSqr;
											pClosestPoly = pTessPoly;
										}
									}
								}
							}
							// There's a chance this still isn't found (eg. if all polygons were removed);
							m_Vars.m_pStartPoly = pClosestPoly;
							Assertf(m_Vars.m_pStartPoly, "Starting poly must have been clipped to nothing");
							if(!m_Vars.m_pStartPoly)
								return false;
						}
					}
				}
			}

			for(tp=0; tp<tessellatedStartPolys.GetCount(); tp++)
			{
				TNavMeshPoly * pTessPoly = tessellatedStartPolys[tp];

				if(pTessPoly == m_Vars.m_pStartPoly)
					continue;

				if(!pTessPoly->m_MinMax.Intersects(startPosMinMax))
					continue;

				// This should never return FALSE, as we will never reach max visited polys at this early stage
				OnFirstVisitingPoly(pTessPoly);

				Assert(!(pTessPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY)));

				// We are about to see whether this poly could be a valid alternative starting poly for the pathsearch.
				// If so then will be added to the binheap, and thus when the pathfind algorithm traces back the
				// route to the start, it could end up at this polygon.
				// Therefore its important that we chose a point-enum for this poly from which the path startpos can be
				// seen in an unobstructed line.

				pTessPoly->m_PathParentPoly = NULL;

				CNavMesh * pTessNavMesh = CPathServer::GetNavMeshFromIndex(pTessPoly->GetNavMeshIndex(), domain);

				u32 v;
				for(v=0; v<pTessPoly->GetNumVertices(); v++)
				{
					pTessNavMesh->GetVertex( pTessNavMesh->GetPolyVertexIndex(pTessPoly, v), m_Vars.m_vPolyPts[v] );
				}

				const u32 iNumCandidatePts = CreatePointsInPoly(
					pTessNavMesh,
					pTessPoly,
					m_Vars.m_vPolyPts,
					ShouldUseMorePointsForPoly(*pTessPoly, pPathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL,
					m_Vars.g_vClosestPtsInPoly
					);

				for(v=0; v<iNumCandidatePts; v++)
				{
					if(TestDynamicObjectLOS(pPathRequest->m_vPathStart, m_Vars.g_vClosestPtsInPoly[v]))
						break;
				}

				// If we have found a point enum value which can see the start position, then we can safely use this poly
				// and point-enum as an alternative starting point for the pathsearch - thus increasing the chance of the
				// pathsearch completing successfully.
				if(v != iNumCandidatePts)
				{
					pTessPoly->OrFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY);
					pTessPoly->SetPointEnum(v);

					float fDist = (m_Vars.g_vClosestPtsInPoly[v] - pPathRequest->m_vPathStart).Mag();

					const float fCost = fInitialCost + fAlternativeStartPenalty;

					m_PathSearchPriorityQueue->Insert(fCost, fDist, pTessPoly, m_Vars.g_vClosestPtsInPoly[v], m_Vars.m_vDirFromPrevious, 0, NAVMESH_POLY_INDEX_NONE);
				}
			}
		}

		// If the m_pStartPoly didn't get added added to the binheap, then make sure it is - we don't want to end up with an empty binheap!
		
		Assert(!(m_Vars.m_pStartPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY)));

		m_PathSearchPriorityQueue->Insert(fInitialCost, 0.0f, m_Vars.m_pStartPoly, pPathRequest->m_vPathStart, m_Vars.m_vDirFromPrevious, 0, NAVMESH_POLY_INDEX_NONE);
	}
	else
	{
		m_Vars.m_pStartPoly->m_PathParentPoly = NULL;

		m_PathSearchPriorityQueue->Insert(fInitialCost, 0.0f, m_Vars.m_pStartPoly, pPathRequest->m_vPathStart, m_Vars.m_vDirFromPrevious, 0, NAVMESH_POLY_INDEX_NONE);
	}

	// Finally make sure that the actual m_pStartPoly itself doesn't have the NAVMESHPOLY_ALTERNATIVE_STARTING_POLY flag set.
	// This is important, because if our route started on a polygon marked with this flag - then we will automatically
	// prepend the m_pStartPoly itself.
	m_Vars.m_pStartPoly->AndFlags(~NAVMESHPOLY_ALTERNATIVE_STARTING_POLY);

	return true;
}


void
CPathServerThread::MaybeTessellatePolysSurroundingPathEnd(CNavMesh * pNavMesh, CPathRequest * pPathRequest)
{
	Assert(m_Vars.m_pEndPoly);

	const float fOriginalEdgeThreshold = CNavMesh::ms_fSmallestTessellatedEdgeThresholdSqr;
	static dev_float fSmallEdgeThreshold = 2.0f * 2.0f;

	dev_float fNearbyPolyRadius = (pPathRequest->m_bExpandStartEndPolyTessellationRadius ? 2.5f : 0.25f);
	const bool bUseMultipleEndPolys = DoesMinMaxIntersectAnyDynamicObjectsApproximate(m_Vars.m_pEndPoly->m_MinMax);

	// This tesselation stuff is only for the normal navigation domain for now.
	const aiNavDomain domain = kNavDomainRegular;

	if(bUseMultipleEndPolys)
	{
		startPolys.clear();
		startPolys.Append() = m_Vars.m_pEndPoly;

		// Get a list of polys which are very close to the end position.
		GetMultipleNavMeshPolysForRouteEndPoints(pPathRequest, m_Vars.m_pEndPoly, pNavMesh, pPathRequest->m_vPathEnd, fNearbyPolyRadius, startPolys, domain);

		s32 u;
		for(u=0; u<startPolys.GetCount(); u++)
		{
			TNavMeshPoly * pNavMeshPoly = startPolys[u];

			// This should never return FALSE, as we will never reach max visited polys at this early stage
			OnFirstVisitingPoly(pNavMeshPoly);

			if(pNavMeshPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY) ||
				pNavMeshPoly->GetReplacedByTessellation() ||
				pNavMeshPoly == m_Vars.m_pStartPoly)
				continue;

			s32 tp;
			tessellatedStartPolys.clear();
			tessellatedStartPolys.Append() = pNavMeshPoly;

			for(tp=0; tp<tessellatedStartPolys.GetCount(); tp++)
			{
				TNavMeshPoly * pTessPoly = tessellatedStartPolys[tp];

				// This should never return FALSE, as we will never reach max visited polys at this early stage
				OnFirstVisitingPoly(pTessPoly);

				Assert(!pTessPoly->GetReplacedByTessellation());
				CNavMesh * pTessNavMesh = CPathServer::GetNavMeshFromIndex(pTessPoly->GetNavMeshIndex(), domain);

				if(pTessPoly == m_Vars.m_pEndPoly)
				{
					CNavMesh::ms_fSmallestTessellatedEdgeThresholdSqr = fSmallEdgeThreshold;
				}

				if(fwPathServer::CanTessellateThisPoly(pTessPoly) &&
					fwPathServer::IsRoomToTessellateThisPoly(pTessPoly))
				{
					CPathServerThread::ms_dynamicObjectsIntersectingPolygons.clear();
					CDynamicObjectsContainer::GetObjectsIntersectingRegion(pTessPoly->m_MinMax, CPathServerThread::ms_dynamicObjectsIntersectingPolygons, TDynamicObject::FLAG_IGNORE_OPENABLE|TDynamicObject::FLAG_IGNORE_NOT_OBSTACLE);

					if(CPathServerThread::ms_dynamicObjectsIntersectingPolygons.GetCount())
					{
						bool bOk = TessellateNavMeshPolyAndCreateDegenerateConnections(
							pTessNavMesh, pTessPoly,
							pTessNavMesh->GetPolyIndex(pTessPoly),
							false,									// don't bother updating m_pStartPoly when we're using multiple starting polys
							(pTessPoly == m_Vars.m_pEndPoly),		// but we do want to update the m_pEndPoly
							NULL, NAVMESH_NAVMESH_INDEX_NONE, NAVMESH_POLY_INDEX_NONE,
							&tessellatedStartPolys
						);
						Assert(bOk);
						if(bOk)
						{
							// This should never return FALSE, as we will never reach max visited polys at this early stage
							OnFirstVisitingPoly(pTessPoly);
							tessellatedStartPolys.Delete(tp);
							tp--;
						}
					}
				}

				CNavMesh::ms_fSmallestTessellatedEdgeThresholdSqr = fOriginalEdgeThreshold;
			}

			float fClosestEdgeDistSqr = FLT_MAX;
			TNavMeshPoly * pClosestPoly = NULL;
			Vector3 vClosestPt;

			for(tp=0; tp<tessellatedStartPolys.GetCount(); tp++)
			{
				TNavMeshPoly * pTessPoly = tessellatedStartPolys[tp];
				OnFirstVisitingPoly(pTessPoly);
				pTessPoly->m_PathParentPoly = NULL;

				// If no replacement end poly was found underneath the end pos,
				// select the one whose edge is closest to the end pos
				if(!m_Vars.m_pEndPoly && pTessPoly->GetIsTessellatedFragment())
				{
					const s32 iEdge = fwPathServer::GetTessellationNavMesh()->GetPolyClosestEdgeMidToPoint(
						pPathRequest->m_vPathEnd, pTessPoly, &vClosestPt);
					if(iEdge >= 0)
					{
						const float fDistSqr = (vClosestPt-pPathRequest->m_vPathEnd).XYMag2();
						if(fDistSqr < fClosestEdgeDistSqr)
						{
							fClosestEdgeDistSqr = fDistSqr;
							pClosestPoly = pTessPoly;
						}
					}
				}
			}
			// There's a chance this still isn't found (eg. if all polygons were removed);
			if(!m_Vars.m_pEndPoly)
				m_Vars.m_pEndPoly = pClosestPoly;
		}
	}

	CNavMesh::ms_fSmallestTessellatedEdgeThresholdSqr = fOriginalEdgeThreshold;
}


//******************************************************************************************************************
//	AddWaypointsToPreserveHeightChanges
//	After finding & refining (string-pulling) the path, we are left with a set of straight line segments.  The
//	string-pulling operates in 2d & ignores height changes on the polys underneath.  If we need to have waypoints
//	placed at significant changes in *slope* in the navmesh underneath the path - then use this function to
//	traverse path inserting waypoints wherever the slope changes.
//
//******************************************************************************************************************

void
CPathServerThread::AddWaypointsToPreserveHeightChanges(CPathRequest * pPathRequest)
{
#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

#if __ASSERT
	if( pPathRequest->m_iNumPoints < 2 )
	{
		pPathRequest->m_pSavePathFileName = "x:\\gta5\\build\\dev\\807928.prob";

		Displayf( "Less than 2 points in path..\n");
		Displayf( "m_vPathStart (%.2f, %.2f, %.2f)\n", pPathRequest->m_vPathStart.x, pPathRequest->m_vPathStart.y, pPathRequest->m_vPathStart.z);
		Displayf( "m_vPathEnd (%.2f, %.2f, %.2f)\n", pPathRequest->m_vPathEnd.x, pPathRequest->m_vPathEnd.y, pPathRequest->m_vPathEnd.z);
		Displayf( "m_iNumPoints %i \n", pPathRequest->m_iNumPoints);
		Displayf( "m_PathPolys[0] 0x%p", pPathRequest->m_PathPolys[0]);
		Displayf( "m_PathPolys[1] 0x%p", pPathRequest->m_PathPolys[1]);
		Displayf( "m_iNumInitiallyFoundPolys %i", pPathRequest->m_iNumInitiallyFoundPolys);
		Displayf( "m_WaypointFlags[0] %u", pPathRequest->m_WaypointFlags[0].GetSpecialActionFlags());
		Displayf( "m_WaypointFlags[1] %u", pPathRequest->m_WaypointFlags[1].GetSpecialActionFlags());

		Assertf(pPathRequest->m_iNumPoints >= 2, "see (url:bugstar:807928).  Please include the TTY as it will have some more details, and attach the file \"%s\" after closing this dialog box.", pPathRequest->m_pSavePathFileName);
	}
#endif

	TNavMeshPoly * pHeightChangePoly = NULL;
	Vector3 vHeightChangePos;

	const Vector3 vEpsilon(0.25f, 0.25f, 0.5f);

	static Vector3 vFinalPoints[PRESERVE_HEIGHT_CHANGE_MAX_NUM_PTS];
	static TNavMeshPoly * pFinalPolys[PRESERVE_HEIGHT_CHANGE_MAX_NUM_PTS];
	static TNavMeshWaypointFlag iFinalWaypointFlags[PRESERVE_HEIGHT_CHANGE_MAX_NUM_PTS];

	vFinalPoints[0] = pPathRequest->m_PathPoints[0];
	pFinalPolys[0] = pPathRequest->m_PathPolys[0];
	iFinalWaypointFlags[0] = pPathRequest->m_WaypointFlags[0];
	u32 iNumFinalPoints=1;

	s32 i;

	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	for(i=0; i<pPathRequest->m_iNumPoints-1; i++)
	{
		Vector3 & vVert = pPathRequest->m_PathPoints[i];
		const Vector3 & vNextVert = pPathRequest->m_PathPoints[i+1];

		TNavMeshPoly * pPoly = pPathRequest->m_PathPolys[i];
		TNavMeshPoly * pNextPoly = pPathRequest->m_PathPolys[i+1];

		if(pPoly && pNextPoly)
		{
			// If any of these polygons are tessellated, then replace with the original polygons
			// for the purpose of our tests.  This is in line with our strategy for navmesh LOS
			// queries: it results in considerably less processing, and less complicated code than
			// having to deal with connection polygons, etc.

			if(pPoly->GetNavMeshIndex() == NAVMESH_INDEX_TESSELLATION)
			{
				const u32 iPoly = CPathServer::GetTessellationNavMesh()->GetPolyIndex(pPoly);
				TTessInfo * pTess = CPathServer::GetTessInfo(iPoly);
				Assert(pTess);
				CNavMesh * pOriginalNavMesh = CPathServer::GetNavMeshFromIndex(pTess->m_iNavMeshIndex, kNavDomainRegular);
				NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(pOriginalNavMesh); )
				if(pOriginalNavMesh)
					pPoly = pOriginalNavMesh->GetPoly(pTess->m_iPolyIndex);
				else
					pPoly = NULL;
			}
			if(pNextPoly->GetNavMeshIndex() == NAVMESH_INDEX_TESSELLATION)
			{
				const u32 iPoly = CPathServer::GetTessellationNavMesh()->GetPolyIndex(pNextPoly);
				TTessInfo * pTess = CPathServer::GetTessInfo(iPoly);
				Assert(pTess);
				CNavMesh * pOriginalNavMesh = CPathServer::GetNavMeshFromIndex(pTess->m_iNavMeshIndex, kNavDomainRegular);
				NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(pOriginalNavMesh); )
				if(pOriginalNavMesh)
					pNextPoly = pOriginalNavMesh->GetPoly(pTess->m_iPolyIndex);
				else
					pNextPoly = NULL;
			}

			if(pPoly && pNextPoly)
			{
				Vector3 vVecToNext = vNextVert - vVert;
				vVecToNext.Normalize();

				IncTimeStamp();

				bool bHasHeightChange = FindHeightChangePosAlongLine(vVert, vNextVert, pNextPoly, pPoly, pHeightChangePoly, vHeightChangePos, false, domain);

				while(bHasHeightChange)
				{
					Assert(pHeightChangePoly && pHeightChangePoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);

					const bool bAlmostSamePositionAsVert1 = vHeightChangePos.IsClose(vVert, vEpsilon);
					const bool bAlmostSamePositionAsVert2 = vHeightChangePos.IsClose(vNextVert, vEpsilon);

					if(!bAlmostSamePositionAsVert1 && !bAlmostSamePositionAsVert2)
					{
						vFinalPoints[iNumFinalPoints] = vHeightChangePos;
						pFinalPolys[iNumFinalPoints] = pHeightChangePoly;
						iFinalWaypointFlags[iNumFinalPoints].Clear();

						//**************************
						// Preserve certain flags

						if(((pPathRequest->m_WaypointFlags[i].m_iSpecialActionFlags) & WAYPOINT_FLAG_IS_INTERIOR) &&
							((pPathRequest->m_WaypointFlags[i+1].m_iSpecialActionFlags) & WAYPOINT_FLAG_IS_INTERIOR))
							iFinalWaypointFlags[iNumFinalPoints].m_iSpecialActionFlags |= WAYPOINT_FLAG_IS_INTERIOR;

						if(((pPathRequest->m_WaypointFlags[i].m_iSpecialActionFlags) & WAYPOINT_FLAG_IS_ON_WATER_SURFACE) &&
							((pPathRequest->m_WaypointFlags[i+1].m_iSpecialActionFlags) & WAYPOINT_FLAG_IS_ON_WATER_SURFACE))
							iFinalWaypointFlags[iNumFinalPoints].m_iSpecialActionFlags |= WAYPOINT_FLAG_IS_ON_WATER_SURFACE;

						iNumFinalPoints++;

						if(iNumFinalPoints==PRESERVE_HEIGHT_CHANGE_MAX_NUM_PTS-1)
							goto __BreakOut;
					}

					pPoly = pHeightChangePoly;
					vVert = vHeightChangePos;

					vVecToNext = vNextVert - vVert;
					vVecToNext.Normalize();

					bHasHeightChange = FindHeightChangePosAlongLine(vVert, vNextVert, pNextPoly, pPoly, pHeightChangePoly, vHeightChangePos, false, domain);
				}
			}
		}

		vFinalPoints[iNumFinalPoints] = vNextVert;
		pFinalPolys[iNumFinalPoints] = pNextPoly;
		iFinalWaypointFlags[iNumFinalPoints] = pPathRequest->m_WaypointFlags[i+1];
		iNumFinalPoints++;

		if(iNumFinalPoints==PRESERVE_HEIGHT_CHANGE_MAX_NUM_PTS-1)
			break;
	}

__BreakOut:

	pPathRequest->m_PathPoints[0] = vFinalPoints[0];
	pPathRequest->m_PathPolys[0] = pFinalPolys[0];
	pPathRequest->m_WaypointFlags[0] = iFinalWaypointFlags[0];

	vFinalPoints[iNumFinalPoints] = pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints-1];
	pFinalPolys[iNumFinalPoints] = pPathRequest->m_PathPolys[pPathRequest->m_iNumPoints-1];
	iFinalWaypointFlags[iNumFinalPoints] = pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints-1];
	iNumFinalPoints++;

	pPathRequest->m_iNumPoints = 1;

	static const float fMaxDot = rage::Cosf(( DtoR * 1.0f));

	// Now go through points, and remove any between which there is no significant gradient change
	u32 p;
	for(p=1; p<iNumFinalPoints-1; p++)
	{
		const Vector3 & vLastPt = vFinalPoints[p-1];
		const Vector3 & vPt = vFinalPoints[p];
		const Vector3 & vNextPt = vFinalPoints[p+1];

		Vector3 vFromLast = vPt - vLastPt;
		Vector3 vToNext = vNextPt - vPt;
		vFromLast.Normalize();
		vToNext.Normalize();

		const float fDot = DotProduct(vFromLast, vToNext);

		// Gradients are similar, so central point can be removed
		if(fDot > fMaxDot && (iFinalWaypointFlags[p-1].GetSpecialAction() == iFinalWaypointFlags[p].GetSpecialAction() 
			&& iFinalWaypointFlags[p].GetSpecialAction() == iFinalWaypointFlags[p+1].GetSpecialAction()))
		{

		}
		else
		{
			pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints] = vFinalPoints[p];
			pPathRequest->m_PathPolys[pPathRequest->m_iNumPoints] = pFinalPolys[p];
			pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints] = iFinalWaypointFlags[p];
			pPathRequest->m_iNumPoints++;
		}

		if(pPathRequest->m_iNumPoints == MAX_NUM_PATH_POINTS-1)
			break;
	}

#if __ASSERT
	for(s32 t=0; t<pPathRequest->m_iNumPoints; t++)
	{
		Assert( rage::FPIsFinite(pPathRequest->m_PathPoints[t].x) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].y) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].z) );
	}
#endif

#if !__FINAL
	m_PerfTimer->Stop();
	pPathRequest->m_fMillisecsToPostProcessZHeights = (float) m_PerfTimer->GetTimeMS();
#endif
}


//********************************************************************
//	MinimisePathDistance
//	This function minimises the distance of the path by examining
//	each set of 3 nodes, and moving the center one within its
//	polygon to find a position where the total length of the two
//	connected line segments are least - and the LOS is maintained
//	to each of the adjoined points.
//********************************************************************

#define MINIMISE_PATH_RANDDIST_SQR	36.0f

void CPathServerThread::MinimisePathDistance(CPathRequest * pPathRequest)
{
	if(pPathRequest->m_iNumPoints <= 2)
		return;

	Assert(pPathRequest->m_iNumPoints > 2);	// Try to catch weird num-points bug

	float fRandErrorRangeSqr = 0.0f;
	if(pPathRequest->m_bRandomisePoints)
	{
		m_RandomNumGen.Reset(pPathRequest->m_iPedRandomSeed);
	}

#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	static Vector3 vPolyPts[NAVMESHPOLY_MAX_NUM_VERTICES];

	s32 p;
	u32 v;
	u32 n;
	Vector3 * pPrevPt, * pCurrentPt, * pNextPt;
	TNavMeshPoly * pPrevPolyPt, * pNextPolyPt;
	CNavMesh * pNavMesh;
	TNavMeshPoly * pPoly;

	float fPrevToCurrDist;
	float fCurrToNextDist;
	float fPrevToCurrDistSqr;
	float fCurrToNextDistSqr;

	float fTrialPrevToCurrDist;
	float fTrialCurrToNextDist;
	float fTrialPrevToCurrDistSqr;
	float fTrialCurrToNextDistSqr;

	static const float fSmallDistance = 0.1f;
	static const float fSmallDistanceSqr = 0.1f * 0.1f;

	float fBestTotalDiff, fDiff;

	s32 iNumPoints = pPathRequest->m_iNumPoints;
	Assert( ((iNumPoints-1) > 0) && ((iNumPoints-1) < (s32)pPathRequest->m_iNumPoints));	// Try to catch weird num-points bug

	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	for(p=1; p<iNumPoints-1; p++)
	{
		Assert(iNumPoints > 2);	// Try to catch weird num-points bug

		pPoly = pPathRequest->m_PathPolys[p];

		// If there was no poly set for this point, it means it was either the start/end point (but not in this case)
		// or was a point generated along the edge between 2 polys for which there was no LOS between.
		// For now, we shall skip the processing of these cases - hoping that the generated point is okay.
		if(!pPoly)
			continue;

		const u32 iPointEnum = pPoly->GetPointEnum();
//		Assert(iPointEnum != NAVMESHPOLY_POINTENUM_SPECIAL_LINK_ENDPOS);

		if((iPointEnum == NAVMESHPOLY_POINTENUM_CENTROID /*|| iPointEnum == NAVMESHPOLY_POINTENUM_UNUSED*/ )
			&& !pPathRequest->m_bRandomisePoints)
			continue;

		if(pPathRequest->m_bRandomisePoints)
			fRandErrorRangeSqr = m_RandomNumGen.GetVaried(MINIMISE_PATH_RANDDIST_SQR);

		pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);
		if(!pNavMesh)
			continue;

		Assert(p+1 < iNumPoints);

		pPrevPt = &pPathRequest->m_PathPoints[p-1];
		pCurrentPt = &pPathRequest->m_PathPoints[p];
		pNextPt = &pPathRequest->m_PathPoints[p+1];

		Assert( rage::FPIsFinite(pPrevPt->x) && rage::FPIsFinite(pPrevPt->y) && rage::FPIsFinite(pPrevPt->z) );
		Assert( rage::FPIsFinite(pCurrentPt->x) && rage::FPIsFinite(pCurrentPt->y) && rage::FPIsFinite(pCurrentPt->z) );
		Assert( rage::FPIsFinite(pNextPt->x) && rage::FPIsFinite(pNextPt->y) && rage::FPIsFinite(pNextPt->z) );

		pPrevPolyPt = pPathRequest->m_PathPolys[p-1];
		pNextPolyPt = pPathRequest->m_PathPolys[p+1];

		fPrevToCurrDistSqr = (*pCurrentPt - *pPrevPt).Mag2() + fRandErrorRangeSqr;
		fCurrToNextDistSqr = (*pNextPt - *pCurrentPt).Mag2() + fRandErrorRangeSqr;

		// Calc the actual distances from the squared distances
		if(fPrevToCurrDistSqr > fSmallDistanceSqr)
		{
			Assert(fPrevToCurrDistSqr > 0.0f);
			fPrevToCurrDist = 1.0f / invsqrtf_fast(fPrevToCurrDistSqr);
		}
		else
		{
			fPrevToCurrDist = fSmallDistance;
		}

		if(fCurrToNextDistSqr > fSmallDistanceSqr)
		{
			Assert(fCurrToNextDistSqr > 0.0f);
			fCurrToNextDist = 1.0f / invsqrtf_fast(fCurrToNextDistSqr);
		}
		else
		{
			fCurrToNextDist = fSmallDistance;
		}

		for(v=0; v<pPoly->GetNumVertices(); v++)
		{
			pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), vPolyPts[v]);
		}

		const u32 iNumClosePts = CreatePointsInPoly(
			pNavMesh,
			pPoly,
			vPolyPts,
			ShouldUseMorePointsForPoly(*pPoly, pPathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL,
			m_Vars.g_vClosestPtsInPoly);

		Assert(iNumClosePts < NUM_CLOSE_PTS_IN_POLY);

		if(pPathRequest->m_bRandomisePoints && iNumClosePts > 1)
		{
			for(n=0; n<iNumClosePts; n++)
			{
				if(n != iPointEnum)
				{
					m_Vars.g_vClosestPtsInPoly[n] = CreateRandomPointInPoly(pPoly, vPolyPts);
				}
			}
		}

		// To start with we'll set our best choice as out existing (current) point
		fBestTotalDiff = 0.0f;

		for(n=0; n<iNumClosePts; n++)
		{
			// Don't bother processing for the point which we already have
			if(n == pPoly->GetPointEnum())
				continue;

			if(pPathRequest->m_bRandomisePoints)
				fRandErrorRangeSqr = m_RandomNumGen.GetVaried(MINIMISE_PATH_RANDDIST_SQR);

			fTrialPrevToCurrDistSqr = (m_Vars.g_vClosestPtsInPoly[n] - *pPrevPt).Mag2() + fRandErrorRangeSqr;
			fTrialCurrToNextDistSqr = (*pNextPt - m_Vars.g_vClosestPtsInPoly[n]).Mag2() + fRandErrorRangeSqr;

			// We can early-out by comparing both the squared distances
			if(fTrialPrevToCurrDistSqr > fPrevToCurrDistSqr && fTrialCurrToNextDistSqr > fCurrToNextDistSqr)
				continue;

			// Calc the actual distances from the squared distances
			if(fTrialPrevToCurrDistSqr > fSmallDistanceSqr)
			{
				Assert(fTrialPrevToCurrDistSqr > 0.0f);
				fTrialPrevToCurrDist = 1.0f / invsqrtf_fast(fTrialPrevToCurrDistSqr);
			}
			else
			{
				fTrialPrevToCurrDist = fSmallDistance;
			}

			if(fTrialCurrToNextDistSqr > fSmallDistanceSqr)
			{
				Assert(fTrialCurrToNextDistSqr > 0.0f);
				fTrialCurrToNextDist = 1.0f / invsqrtf_fast(fTrialCurrToNextDistSqr);
			}
			else
			{
				fTrialCurrToNextDist = fSmallDistance;
			}

			// If this is a better path, fDiff will be a positive value
			fDiff = (fPrevToCurrDist + fCurrToNextDist) - (fTrialPrevToCurrDist + fTrialCurrToNextDist);

			// If this point placement is better than our current best effort, see if the LOS is clear
			if(fDiff > fBestTotalDiff)
			{
				// If there was no previous poly, it indicates that the prev point was a waypoint
				// inserted on the polygon edge in order to see round a corner - so it is guaranteed
				// to be visible from anywhere in this poly
				if(pPrevPolyPt)
				{
					// Test LOS from current point to last point
					IncTimeStamp();

					if(!TestNavMeshLOS(m_Vars.g_vClosestPtsInPoly[n], *pPrevPt, pPrevPolyPt, pPoly, NULL, m_Vars.m_iLosFlags, domain))
					{
						continue;
					}
				}

				// If there is no next poly, it indicates that the next point was a waypoint
				// inserted on the polygon edge in order to see round a corner - so it is guaranteed
				// to be visible from anywhere in this poly
				if(pNextPolyPt)
				{
					// Test LOS from current point to next point
					IncTimeStamp();

					if(!TestNavMeshLOS(m_Vars.g_vClosestPtsInPoly[n], *pNextPt, pNextPolyPt, pPoly, NULL, m_Vars.m_iLosFlags, domain))
					{
						continue;
					}
				}

				// If we're considering dynamic objects, then test those as well
				if(!TestDynamicObjectLOS(m_Vars.g_vClosestPtsInPoly[n], *pPrevPt))
				{
					continue;
				}
				// If we're considering dynamic objects, then test those as well
				if(!TestDynamicObjectLOS(m_Vars.g_vClosestPtsInPoly[n], *pNextPt))
				{
					continue;
				}

				// The distance is shorter, and all the LOS tests have passed - so this is a better point for the route.
				fBestTotalDiff = fDiff;

				Assert(p < MAX_NUM_PATH_POINTS+1);
				Assert(n < NUM_CLOSE_PTS_IN_POLY);

				pPathRequest->m_PathPoints[p] = m_Vars.g_vClosestPtsInPoly[n];
				pPoly->SetPointEnum(n);

				fPrevToCurrDistSqr = fTrialPrevToCurrDistSqr;
				fCurrToNextDistSqr = fTrialCurrToNextDistSqr;
				fPrevToCurrDist = fTrialPrevToCurrDist;
				fCurrToNextDist = fTrialCurrToNextDist;

			}

		}

	}

#ifdef GTA_ENGINE
#if AI_OPTIMISATIONS_OFF || NAVMESH_OPTIMISATIONS_OFF
	Assertf(pPathRequest->m_iNumPoints > 2, "path handle:%p", pPathRequest->m_hHandle);	// Try to catch weird num-points bug
#endif
#endif

#if !__FINAL
	m_PerfTimer->Stop();
	pPathRequest->m_fMillisecsToMinimisePathLength = (float) m_PerfTimer->GetTimeMS();
#endif

}

void CPathServerThread::PullPathOutFromEdges(CPathRequest * pPathRequest, bool bDoStringPull)
{
	if (pPathRequest->m_iNumPoints <= 2 && pPathRequest->m_iNumPoints < MAX_NUM_PATH_POINTS)
		return;

#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	const float fPushDist = (pPathRequest->m_bPullFromEdgeExtra ? CPathServerThread::ms_fPullOutFromEdgesDistanceExtra : CPathServerThread::ms_fPullOutFromEdgesDistance);
	static const float fPushThreshold = 0.999f;
	Vector3 vCacheVec;

	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	Vector3* pCachePushDir = NULL;
	int nPoints = pPathRequest->m_iNumPoints - 1;	// We check one node ahead
	for (int i = 1; i < nPoints; ++i)				// and one behind
	{
		Vector3 vPushDir;
		const Vector3 vPrevPt = pPathRequest->m_PathPoints[i-1];
		const Vector3 vThisPt = pPathRequest->m_PathPoints[i];
		const Vector3 vNextPt = pPathRequest->m_PathPoints[i+1];

		bool bSkip = false;
		bool bFirstFrame = (i == 1);
		if (pCachePushDir)
			vPushDir = *pCachePushDir;
		else if (!bFirstFrame)
			bSkip = true;

		//Scan ahead 2 nodes and cache that direction to push since if we push without caching
		//we will be pushing in the wrong direction since the last push will then move the "prev vector"
		if (i < nPoints - 1)
		{
			const Vector3 vNextNextPt = pPathRequest->m_PathPoints[i+2];
			if (CalcCornerPushDir(vCacheVec, vThisPt, vNextPt, vNextNextPt, fPushThreshold))
				pCachePushDir = &vCacheVec;
			else
				pCachePushDir = NULL;
		}

		if (bFirstFrame && !CalcCornerPushDir(vPushDir, vPrevPt, vThisPt, vNextPt, fPushThreshold))
			continue;
		else if (bSkip)
			continue;	// The cache was invalid (straight)

		// Not having valid polys might cause a crash?
		TNavMeshPoly* pPrevPoly = pPathRequest->m_PathPolys[i-1];
		TNavMeshPoly* pThisPoly = pPathRequest->m_PathPolys[i];
		TNavMeshPoly* pNextPoly = pPathRequest->m_PathPolys[i+1];
		if (!pPrevPoly || !pThisPoly || !pNextPoly)
			continue;

		// Don't pull and stuff if we got any special action near, such as climbs
		if (pPathRequest->m_WaypointFlags[i-1].GetSpecialAction() ||
			pPathRequest->m_WaypointFlags[i].GetSpecialAction() ||
			pPathRequest->m_WaypointFlags[i+1].GetSpecialAction())
		{
			continue;
		}

		//// If we pull points too close to each other and one of them fail the angle between them will be quite bad
		//if (vPrevPt.Dist2(vThisPt) < rage::square(fPushDist * 2.f) ||
		//	vNextPt.Dist2(vThisPt) < rage::square(fPushDist * 2.f))
		//{
		//	continue;
		//}

		// Do those LOS tests...
		// Consider a table like this: [7,6,5,4 (middle) 3,2,1,0], where middle is our current position
		// We loop through and test from middle and see how far we got LOS on each side
		// The left in the table represent pulling the corner point out, the right is pulling it in
		// We do consider LOS distance on each side, and try to find a good position to pull or push our position to
		// We skew this algorithm slightly towards pushing out from corners as that should be better
		Vector3 vIntersectPoint;
		const Vector2 vLosDir = Vector2(vPushDir * fPushDist, Vector2::kXY);

		int nIn, nOut;
		float fIn = 0.f;
		float fOut = 0.f;
		const float fPushTableScale[5] = {1.0f, 0.75f, 0.5f, 0.25f, 0.0f};

		// We first check LOS distance how close we are to the corner
		for (nIn = 0; nIn < 4; ++nIn, fIn += 1.f)
		{
			IncTimeStamp();
			if (TestNavMeshLOS(vThisPt, -vLosDir * fPushTableScale[nIn], &vIntersectPoint, pThisPoly, m_Vars.m_iLosFlags, domain) && 
				TestDynamicObjectLOS(vThisPt, vThisPt - vPushDir * fPushDist * fPushTableScale[nIn]))
			{
				break;
			}
		}

		// And how far we can push out from that corner
		for (nOut = 0; nOut < 4; ++nOut, fOut += 1.f)
		{
			IncTimeStamp();
			if (TestNavMeshLOS(vThisPt, vLosDir * fPushTableScale[nOut], &vIntersectPoint, pThisPoly, m_Vars.m_iLosFlags, domain) && 
				TestDynamicObjectLOS(vThisPt, vThisPt + vPushDir * fPushDist * fPushTableScale[nOut]))
			{
				break;
			}
		}

		// Full LOS to either side, no need to move this path position around
		if (nIn == 0 && nOut ==0)
			continue;

		// Map the values to match the table [8,7,6,5 (4) 3,2,1,0], this should be skewed by adding the middle as a value, and round up
		const int iAvg = (int)ceil(((fIn + (8.f - fOut)) * 0.5f) + 0.5f);

		if (nIn > 0)
			pPathRequest->m_WaypointFlags[i].m_iSpecialActionFlags |= WAYPOINT_FLAG_IS_NARROW_IN;
		if (nOut > 0)
			pPathRequest->m_WaypointFlags[i].m_iSpecialActionFlags |= WAYPOINT_FLAG_IS_NARROW_OUT;

		// Map the values to match the table [7,6,5,4 (middle) 3,2,1,0], should not be skewed or we won't favor pushing the corner out
		const bool bIsPullingOut = iAvg > 4;
		const int iPushIndex = (bIsPullingOut ? 7 - iAvg : iAvg);
		
		// 
		const Vector3 vNewPathPos = vThisPt + vPushDir * fPushDist * fPushTableScale[iPushIndex] * (bIsPullingOut ? 1.f : -1.f);
		
		IncTimeStamp();
		const Vector2 vLosDirFinal = Vector2(vNewPathPos - vThisPt, Vector2::kXY);
		TNavMeshPoly* pNewPoly = TestNavMeshLOS(vThisPt, vLosDirFinal, &vIntersectPoint, pThisPoly, m_Vars.m_iLosFlags, domain);
		if (pNewPoly)
		{
			// And then we just test to see if this pull is a valid position to reach in the path
			IncTimeStamp();
			if (!TestNavMeshLOS(vNewPathPos, vPrevPt, pPrevPoly, pNewPoly, NULL, m_Vars.m_iLosFlags, domain))
				continue;

			if (!TestDynamicObjectLOS(vNewPathPos, vPrevPt))
				continue;

			IncTimeStamp();
			if (!TestNavMeshLOS(vNewPathPos, vNextPt, pNextPoly, pNewPoly, NULL, m_Vars.m_iLosFlags, domain))
				continue;

			if (!TestDynamicObjectLOS(vNewPathPos, vNextPt))
				continue;

			pPathRequest->m_PathPoints[i] = vNewPathPos;
			pPathRequest->m_PathPolys[i] = pNewPoly;
		}
	}

	// We do another pass of string pull, in case we can get rid of smaller quirky points hidden behind corners
	// This might defeat our purpose slightly though, we should only string pull quirks so we might need to
	// improve this part. Maybe it is better to avoid the quirk in the pull out instead though.
	if (bDoStringPull)
	{
		for (int i = 2; i < nPoints; ++i)
		{
			const Vector3 vPrevPrevPt = pPathRequest->m_PathPoints[i-2];
			const Vector3 vPrevPt = pPathRequest->m_PathPoints[i-1];
			const Vector3 vThisPt = pPathRequest->m_PathPoints[i];
			const Vector3 vNextPt = pPathRequest->m_PathPoints[i+1];
	//		const Vector3 vNextNextPt = pPathRequest->m_PathPoints[i+2];

			// Not having valid polys might cause a crash?
			TNavMeshPoly* pPrevPrevPoly = pPathRequest->m_PathPolys[i-2];
			TNavMeshPoly* pPrevPoly = pPathRequest->m_PathPolys[i-1];
			TNavMeshPoly* pThisPoly = pPathRequest->m_PathPolys[i];
			TNavMeshPoly* pNextPoly = pPathRequest->m_PathPolys[i+1];
			if (!pPrevPoly || !pNextPoly)
				continue;

			// Don't pull and stuff if we got any special action near, such as climbs
			if (pPathRequest->m_WaypointFlags[i-1].GetSpecialAction() || pPathRequest->m_WaypointFlags[i+1].GetSpecialAction())
				continue;

			// Pull out should not cause greater dist than this
			const float fMinDistSqr = rage::square(fPushDist * 2.0f);

			// Don't stringpull when points are not close as that could defeat our purpose
			if (vThisPt.Dist2(vPrevPt) > fMinDistSqr &&	vThisPt.Dist2(vNextPt) > fMinDistSqr)
				continue;

			// And the angle has to make a sharp corner "quirk"
			static const float fTolerance = 0.15f;		// Almost 90deg
			if (!IsPointsMakingAQuirk(vPrevPrevPt, vPrevPt, vThisPt, vNextPt, fTolerance) && vThisPt.Dist2(vPrevPt) > rage::square(0.1f))
				continue;

			int iPointToRemove = i;

			// Do those LOS tests...
			IncTimeStamp();
			if (!TestNavMeshLOS(vPrevPt, vNextPt, pPrevPoly, pNextPoly, NULL, m_Vars.m_iLosFlags, domain) || !TestDynamicObjectLOS(vPrevPt, vNextPt))
			{
				// Not having valid polys might cause a crash
				if (!pPrevPrevPoly || !pThisPoly)
					continue;

				// Shit ok, let's try with the previous one instead
				// Don't pull and stuff if we got any special action near, such as climbs
				if (pPathRequest->m_WaypointFlags[i-2].GetSpecialAction() || pPathRequest->m_WaypointFlags[i].GetSpecialAction())
					continue;

				// Don't stringpull when points are not close as that could defeat our purpose
				if (vThisPt.Dist2(vPrevPt) > fMinDistSqr)
					continue;

				// Do those LOS tests...
				IncTimeStamp();
				if (!TestNavMeshLOS(vPrevPrevPt, vThisPt, pPrevPrevPoly, pThisPoly, NULL, m_Vars.m_iLosFlags, domain) || !TestDynamicObjectLOS(vPrevPrevPt, vThisPt))
					continue;

				// So remove the one behind us instead!
				iPointToRemove = i - 1;
			}

			for (int j = iPointToRemove; j < nPoints; ++j)
			{
				pPathRequest->m_PathPoints[j] = pPathRequest->m_PathPoints[j+1];
				pPathRequest->m_PathPolys[j] = pPathRequest->m_PathPolys[j+1];
				pPathRequest->m_WaypointFlags[j] = pPathRequest->m_WaypointFlags[j+1];
			}

			--i;
			--nPoints;
			--pPathRequest->m_iNumPoints;
		}
	}

#if __ASSERT
	for(s32 t=0; t<pPathRequest->m_iNumPoints; t++)
	{
		Assert( rage::FPIsFinite(pPathRequest->m_PathPoints[t].x) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].y) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].z) );
	}
#endif

#if !__FINAL
	m_PerfTimer->Stop();
	pPathRequest->m_fMillisecsToPullOutFromEdges = (float) m_PerfTimer->GetTimeMS();
#endif
}

//****************************************************************************
//	CalcPathControlPoints
//	This is a new approach to the problem which SmoothPath() tries to fix.
//	At each sharp bend in the path, a bezier curve is created to try and
//	cut off as much of the sharp corner as possible.  The bezier is evaluated
//	to a fixed resolution, and a number of line-segments are tested against
//	the world in an attempt to determine whether the new route is clear of
//	obstacles.  If so, then some information is stored with the path about
//	the control points which were chosen.  If obstacles do exist, then we can
//	attempt again with a different set of control points - or abort the
//	operation for this corner in the path.

void
CPathServerThread::CalcPathBezierControlPoints(CPathRequest * pPathRequest)
{
	// If this is a straight line, then there's nothing to do..
	if(pPathRequest->m_iNumPoints < 3)
	{
		return;
	}

#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	static const float fMinSmoothDist	= 1.0f;
	static const float fSplineStep		= 1.0f / ((float)PATHSPLINE_NUM_TEST_SUBDIVISIONS);

	// Use this variable to prevent spline overlaps
	float fDistPreviousPt3ToNext = FLT_MAX;

	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	for(s32 iCurrent=1; iCurrent<pPathRequest->m_iNumPoints-1; iCurrent++)
	{
		const int iLast = iCurrent-1;
		const int iNext = iCurrent+1;
		const Vector3 & vLast = pPathRequest->m_PathPoints[iLast];
		const Vector3 & vCurrent = pPathRequest->m_PathPoints[iCurrent];
		const Vector3 & vNext = pPathRequest->m_PathPoints[iNext];

		const Vector3 vLastToCurrent = vCurrent - vLast;
		const Vector3 vCurrentToNext = vNext - vCurrent;
		const float fDistLastToCurrent = vLastToCurrent.Mag();
		const float fDistCurrentToNext = vCurrentToNext.Mag();

		// If the current waypoint is a climb/drop/etc - then don't use splines here
		const u32 iWpt = pPathRequest->m_WaypointFlags[iCurrent].GetSpecialActionFlags();
		if(IsSpecialActionClimbOrDrop(iWpt))
		{
			fDistPreviousPt3ToNext = FLT_MAX;
			continue;
		}
		if(fDistLastToCurrent < fMinSmoothDist || fDistCurrentToNext < fMinSmoothDist)
		{
			fDistPreviousPt3ToNext = FLT_MAX;
			continue;
		}

		// If the angle between the two waypoints is shallow, then don't bother with using splines
		Vector3 vLastToCurrentNormalized = vLastToCurrent;
		Vector3 vCurrentToNextNormalized = vCurrentToNext;
		NormalizeAndMag(vLastToCurrentNormalized);
		vCurrentToNextNormalized.Normalize();
		static const float fDeadAhead = rage::Cosf(( DtoR * 8.0f));
		if(DotProduct(vLastToCurrentNormalized, vCurrentToNextNormalized) > fDeadAhead)
		{
			fDistPreviousPt3ToNext = FLT_MAX;
			continue;
		}

		Vector3 vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4;

		for(int s=0; s<PATHSPLINE_NUM_TEST_DISTANCES; s++)
		{
			// If the distance from vCtrlPt2->vCurrent is MORE than the previous vCtrlPt3->vNext
			// Then we CANNOT use this enumerant value of 's' since it will OVERLAP the last spline,
			// and cause awkward situations when following the path (such as doubling-back)
			vCtrlPt2 = vLast + (vLastToCurrent * g_fPathSplineDists[s]);

			const float fDistPt2ToCurrent = (vCurrent-vCtrlPt2).Mag();
			if(fDistPt2ToCurrent > fDistPreviousPt3ToNext)
			{
				pPathRequest->m_PathResultInfo.m_fPathCtrlPt2TVals[iCurrent] = 1.0f - (fDistPreviousPt3ToNext/fDistLastToCurrent);
			}
			else
			{
				pPathRequest->m_PathResultInfo.m_fPathCtrlPt2TVals[iCurrent] = 0.0f;
			}

			CalcPathSplineCtrlPts(s, pPathRequest->m_PathResultInfo.m_fPathCtrlPt2TVals[iCurrent], vLast, vNext, vLastToCurrent, vCurrentToNext, vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4);

			// Now create PATHSPLINE_NUM_TEST_SUBDIVISIONS points along this spline.
			Vector3 vSplinePtA = vCtrlPt2;
			Vector3 vEndPos;
			float u;
			bool bSplineOk = true;
			TNavMeshPoly * pEndingPoly = NULL;

			// Efficiently find the initial pEndingPoly.
			// We already know which polygon vCurrent is upon, so we can do a navmesh LOS
			// to vCtrlPt2 (the starting point for this spline curve) to obtain the polygon
			// directly under that point.  This should be hugely more efficient than doing
			// a full polygon query in the navmesh.
			TNavMeshPoly * pPolyUnderCurrent = pPathRequest->m_PathPolys[iCurrent];
			// pPolyUnderCurrent may be NULL if this point was added before/after a non-standard adjacency
			// such as a climb or drop-down, etc.
			if(pPolyUnderCurrent)
			{
				IncTimeStamp();
				const Vector2 vDirToCtrlPt2(vCtrlPt2.x - vCurrent.x, vCtrlPt2.y - vCurrent.y);
				pEndingPoly = TestNavMeshLOS(vCurrent, vDirToCtrlPt2, &vEndPos, pPolyUnderCurrent, m_Vars.m_iLosFlags, domain);

				if(pEndingPoly)
				{
					for(u=fSplineStep; u<=1.0f; u+=fSplineStep)
					{
						const Vector3 vSplinePtB = PathSpline(vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4, u);
						const Vector2 vDir(vSplinePtB.x - vSplinePtA.x, vSplinePtB.y - vSplinePtA.y);

						IncTimeStamp();
						pEndingPoly = TestNavMeshLOS(vSplinePtA, vDir, &vEndPos, pEndingPoly, m_Vars.m_iLosFlags, domain);

						if(pEndingPoly && !TestDynamicObjectLOS(vSplinePtA, vSplinePtB))
							pEndingPoly = NULL;

						if(!pEndingPoly)
						{
							// Fail!
							bSplineOk = false;
							break;
						}

						vSplinePtA = vSplinePtB;
					}
					// If this spline worked ok (ie. all the test segments had a clear LOS, then we'll use this)
					// Otherwise the algorithm will test another spline for this corner, using different ctrl pts.
					if(bSplineOk)
					{
						pPathRequest->m_PathResultInfo.m_iPathSplineDistances[iCurrent] = s;
						break;
					}
				}
			}
		}
		// We have a spline at this corner, so calculate this distance to use in the overlap test at the next corner
		if(pPathRequest->m_PathResultInfo.m_iPathSplineDistances[iCurrent] != -1)
		{
			fDistPreviousPt3ToNext = (vNext - vCtrlPt3).Mag();	// xy?
		}
		// No spline at this corner, so reset this value
		else
		{
			fDistPreviousPt3ToNext = FLT_MAX;
			pPathRequest->m_PathResultInfo.m_fPathCtrlPt2TVals[iCurrent] = 0.0f;
		}
	}

#if __ASSERT
	for(s32 t=0; t<pPathRequest->m_iNumPoints; t++)
	{
		Assert( rage::FPIsFinite(pPathRequest->m_PathPoints[t].x) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].y) && rage::FPIsFinite(pPathRequest->m_PathPoints[t].z) );
	}
#endif

#if !__FINAL
	m_PerfTimer->Stop();
	pPathRequest->m_fMillisecsToSmoothPath = (float) m_PerfTimer->GetTimeMS();
#endif // !__FINAL
}

//******************************************************************************
//	SmoothPath
//	Smooth the path by identifying sharp corners, and cutting the corners off.
//	We do this by moving one vertex & inserting another.
//	If this creates a path which is larger than 'iMaxNumPoints' then we will
//	have to truncate the path up to where we ended processing it..
//******************************************************************************

// Add all the path points into a list
atArray<Vector3> newPoints(0, 32);
atArray<TNavMeshWaypointFlag> newWayPoints(0, 32);

void
CPathServerThread::SmoothPath(CPathRequest * pPathRequest)
{
	// If this is a straight line, then there's nothing to do..
	if(pPathRequest->m_iNumPoints < 3)
	{
		return;
	}

#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	static const float fCosSharpAngle = rage::Cosf(( DtoR * 67.5f));
	static const float fSmallDistanceSqr = 0.35f * 0.35f;

	static const int NUM_TESTS = 5;
	float fLastToCurrTestDists[NUM_TESTS];
	float fCurrToNextTestDists[NUM_TESTS];
	Vector3 vTmp;

	newPoints.clear();
	newWayPoints.clear();

	s32 p;
	for(p=0; p<pPathRequest->m_iNumPoints; p++)
	{
		newPoints.Append() = pPathRequest->m_PathPoints[p];
		newWayPoints.Append() = pPathRequest->m_WaypointFlags[p];
	}

	bool bMadeChanges = false;
	int iCurrentPt = 1;

	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	while(iCurrentPt < newPoints.GetCount()-1 && iCurrentPt < MAX_NUM_PATH_POINTS)
	{
		// Can only cut corners which aren't special waypoints..
		if(newWayPoints[iCurrentPt].m_iSpecialActionFlags != 0)
		{
			iCurrentPt++;
			continue;
		}

		const Vector3 & vLastPt = newPoints[iCurrentPt-1];
		const Vector3 & vCurrentPt = newPoints[iCurrentPt];
		const Vector3 & vNextPt = newPoints[iCurrentPt+1];

		Vector3 vLastToCurr = vCurrentPt - vLastPt;
		Vector3 vCurrToNext = vNextPt - vCurrentPt;

		float fLastToCurrDist2 = vLastToCurr.Mag2();
		float fCurrToNextDist2 = vCurrToNext.Mag2();

		if(fLastToCurrDist2 < fSmallDistanceSqr || fCurrToNextDist2 < fSmallDistanceSqr)
		{
			iCurrentPt++;
			continue;
		}

		vLastToCurr.NormalizeFast();
		vCurrToNext.NormalizeFast();

		const float fDot = Dot(vLastToCurr, vCurrToNext);
		if(fDot < fCosSharpAngle)
		{
			bool bGotLos=false;
			Vector3 vMidPointFromLast, vMidPointToNext;

			float fLastToCurrDist = Sqrtf(fLastToCurrDist2);
			float fCurrToNextDist = Sqrtf(fCurrToNextDist2);

			// Work out a number of test distances for each vector
			fLastToCurrTestDists[0] = fLastToCurrDist * 0.5f;
			fLastToCurrTestDists[1] = fLastToCurrDist * 0.75f;
			fLastToCurrTestDists[2] = fLastToCurrDist * 0.85f;
			fLastToCurrTestDists[3] = Max(fLastToCurrDist - 1.0f, 1.0f);
			fLastToCurrTestDists[4] = Max(fLastToCurrDist - 0.5f, 1.0f);

			fCurrToNextTestDists[0] = fCurrToNextDist * 0.5f;
			fCurrToNextTestDists[1] = fCurrToNextDist * 0.25f;
			fCurrToNextTestDists[2] = fCurrToNextDist * 0.15f;
			fCurrToNextTestDists[3] = Min(1.0f, fCurrToNextDist);
			fCurrToNextTestDists[4] = Min(0.5f, fCurrToNextDist);

			for(int iTest=0; iTest<NUM_TESTS; iTest++)
			{
				vMidPointFromLast = vLastPt + (vLastToCurr * fLastToCurrTestDists[iTest]);
				vMidPointToNext = vCurrentPt + (vCurrToNext * fCurrToNextTestDists[iTest]);

				// Is there a clear navmesh LOS between these new midpoints?

				u32 iStartNavMesh = CPathServer::GetNavMeshIndexFromPosition(vMidPointFromLast, domain);
				if(iStartNavMesh == NAVMESH_NAVMESH_INDEX_NONE)
					continue;
				CNavMesh * pStartNavMesh = CPathServer::GetNavMeshFromIndex(iStartNavMesh, domain);
				if(!pStartNavMesh)
					continue;
				u32 iStartPolyIndex = pStartNavMesh->GetPolyBelowPoint(vMidPointFromLast + Vector3(0, 0, 1.0f), vTmp, 5.0f);
				if(iStartPolyIndex == NAVMESH_POLY_INDEX_NONE)
					continue;
				TNavMeshPoly * pStartPoly = pStartNavMesh->GetPoly(iStartPolyIndex);
				Assert(pStartPoly);

				u32 iEndNavMesh = CPathServer::GetNavMeshIndexFromPosition(vMidPointToNext, domain);
				if(iEndNavMesh == NAVMESH_NAVMESH_INDEX_NONE)
					continue;
				CNavMesh * pEndNavMesh = CPathServer::GetNavMeshFromIndex(iEndNavMesh, domain);
				if(!pEndNavMesh)
					continue;
				u32 iEndPolyIndex = pEndNavMesh->GetPolyBelowPoint(vMidPointToNext + Vector3(0, 0, 1.0f), vTmp, 5.0f);
				if(iEndPolyIndex == NAVMESH_POLY_INDEX_NONE)
					continue;
				TNavMeshPoly * pEndPoly = pEndNavMesh->GetPoly(iEndPolyIndex);
				Assert(pEndPoly);

				IncTimeStamp();
				bGotLos = TestNavMeshLOS(vMidPointFromLast, vMidPointToNext, pEndPoly, pStartPoly, NULL, m_Vars.m_iLosFlags, domain);
				if(bGotLos)
				{
					bGotLos = TestDynamicObjectLOS(vMidPointFromLast, vMidPointToNext);
					if(bGotLos)
						break;
				}
			}

			if(bGotLos)
			{
				newPoints[iCurrentPt] = vMidPointFromLast;
				newWayPoints[iCurrentPt].Clear();

				newPoints.Insert(iCurrentPt+1) = vMidPointToNext;
				newWayPoints.Insert(iCurrentPt+1).Clear();

				bMadeChanges = true;
			}
		}

		iCurrentPt++;
	}

	// Now copy the new points back into the path request
	if(bMadeChanges)
	{
		s32 iNum = Min(newPoints.GetCount(), MAX_NUM_PATH_POINTS);
		for(p=0; p<iNum; p++)
		{
			pPathRequest->m_PathPoints[p] = newPoints[p];
			pPathRequest->m_WaypointFlags[p] = newWayPoints[p];
			pPathRequest->m_PathPolys[p] = NULL;
		}
		pPathRequest->m_iNumPoints = iNum;
	}

#if !__FINAL
	m_PerfTimer->Stop();
	pPathRequest->m_fMillisecsToSmoothPath = (float) m_PerfTimer->GetTimeMS();
#endif
}



bool
CPathServerThread::GetSharedEdgeEndPoints(TNavMeshPoly * pPoly1, TNavMeshPoly * pPoly2, Vector3 & vPoint1, Vector3 & vPoint2, aiNavDomain domain)
{
	CNavMesh * pPoly1NavMesh = CPathServer::GetNavMeshFromIndex(pPoly1->GetNavMeshIndex(), domain);
	if(!pPoly1NavMesh)
		return false;

	CNavMesh * pPoly2NavMesh = CPathServer::GetNavMeshFromIndex(pPoly2->GetNavMeshIndex(), domain);
	if(!pPoly2NavMesh)
		return false;

	// Go through pPoly1's edges
	for(u32 v=0; v<pPoly1->GetNumVertices(); v++)
	{
		const TAdjPoly & adjPoly = pPoly1NavMesh->GetAdjacentPoly(pPoly1->GetFirstVertexIndex()+v);

		// Adjacent poly is in same NavMesh as pPoly2
		if(adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_NORMAL && adjPoly.GetNavMeshIndex(pPoly1NavMesh->GetAdjacentMeshes()) == pPoly2->GetNavMeshIndex())
		{
			// Is this pPoly2 ?
			if(pPoly2NavMesh->GetPoly(adjPoly.GetPolyIndex()) == pPoly2)
			{
				// Okay, so we've found the shared edge between the 2 polys.
				// Now take the line defined by "vBestPointInPoly1 to vBestPointInPoly2" and test
				// both endpoints of the shared edge against it.  Boint endpoints should be
				// to the same side of the line.
				// The vertex result is to be the endpoint closest to the line.

				const int nextv = (v+1 < pPoly1->GetNumVertices()) ? v+1 : 0;

				pPoly1NavMesh->GetVertex( pPoly1NavMesh->GetPolyVertexIndex(pPoly1, v), vPoint1 );
				pPoly1NavMesh->GetVertex( pPoly1NavMesh->GetPolyVertexIndex(pPoly1, nextv), vPoint2 );
				return true;
			}
		}
	}
	return false;
}


// NAME : ObtainEntryEdgeViaConnectionPoly
// PURPOSE : Given pParentPoly and pChildPoly, which are separated by any number of connection polys -
// we wish to find which edge pChildPoly was entered by.
bool CPathServerThread::ObtainEntryEdgeViaConnectionPoly(
	TNavMeshPoly * pStartPoly, CNavMesh * pStartNavMesh,
	TNavMeshPoly * pTargetPoly, CNavMesh * pTargetNavMesh, 
	TNavMeshPoly * pRealOrConnectionPoly, CNavMesh * pNavMesh,
	TNavMeshPoly * pParentPoly, CNavMesh * pParentNavMesh,
	TNavMeshAndPoly & outPrevNavMeshAndPoly,
	const Vector3 & vStartExitEdge, const Vector3 & vStartExitEdgeV1, const Vector3 & vStartExitEdgeV2, const aiNavDomain domain ) const
{
	// Avoid possibility of infinite recursion by timestamping polygons we visit
	pRealOrConnectionPoly->m_TimeStamp = m_iNavMeshPolyTimeStamp;

	// When we recurse down to a real polygon, we see if this is the pChildPoly
	if( !pRealOrConnectionPoly->GetIsDegenerateConnectionPoly())
	{
		if(pRealOrConnectionPoly == pTargetPoly)
		{
			Assert(pParentNavMesh && pParentPoly);
			if(pParentNavMesh && pParentPoly)
			{
				outPrevNavMeshAndPoly.m_iNavMeshIndex = pParentNavMesh->GetIndexOfMesh();
				outPrevNavMeshAndPoly.m_iPolyIndex = pParentNavMesh->GetPolyIndex(pParentPoly);
			}
			return true;
		}
	}
	// For connection polygons, we find all edges pointing in opposite direction to the exit edge
	else
	{
		s32 lasta = pRealOrConnectionPoly->GetNumVertices()-1;
		Vector3 vLast, vVert, vAdjacentEdge;

		s32 lastvi = pNavMesh->GetPolyVertexIndex(pRealOrConnectionPoly, lasta);
		pNavMesh->GetVertex( lastvi, vLast);

		for( s32 a=0; a<pRealOrConnectionPoly->GetNumVertices(); a++ )
		{
			const s32 vi = pNavMesh->GetPolyVertexIndex(pRealOrConnectionPoly, a);

			const TAdjPoly & adjacentPoly = pNavMesh->GetAdjacentPoly(lastvi);
			const TNavMeshIndex iNavMesh = adjacentPoly.GetNavMeshIndex( pNavMesh->GetAdjacentMeshes() );
			if( iNavMesh != NAVMESH_NAVMESH_INDEX_NONE && adjacentPoly.GetAdjacencyType()==ADJACENCY_TYPE_NORMAL )
			{
				pNavMesh->GetVertex( vi, vVert);
				vAdjacentEdge = vVert - vLast;

				NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(vAdjacentEdge.Mag2() > SMALL_FLOAT); )

					if(vAdjacentEdge.Mag2() > SMALL_FLOAT)
					{
						vAdjacentEdge.NormalizeFast();

						const float fDot = (vAdjacentEdge.x * vStartExitEdge.x) + (vAdjacentEdge.y * vStartExitEdge.y);

						NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(Abs(fDot) > 0.707f); )

							if( fDot > 0.0f )
							{
								// Test for interval overlap
								float fExitEdgeMin = vAdjacentEdge.Dot( vStartExitEdgeV1 );
								float fExitEdgeMax = vAdjacentEdge.Dot( vStartExitEdgeV2 );
								if(fExitEdgeMin > fExitEdgeMax)
									SwapEm(fExitEdgeMin, fExitEdgeMax);

								float fAdjacentEdgeMin = vAdjacentEdge.Dot( vLast );
								float fAdjacentEdgeMax = vAdjacentEdge.Dot( vVert );
								if(fAdjacentEdgeMin > fAdjacentEdgeMax)
									SwapEm(fAdjacentEdgeMin, fAdjacentEdgeMax);

								const float fIntervalDistance = (fExitEdgeMin < fAdjacentEdgeMin) ?
									fAdjacentEdgeMin - fExitEdgeMax : fExitEdgeMin - fAdjacentEdgeMax;

								// Overlap exists, so this is an edge leaving the connection poly on the opposite side to the
								// exit edge of the previous polyg) and with overlapping extents : ie. a candidate to visit
								if(fIntervalDistance < 0.0f)
								{
									CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(iNavMesh, domain);
									if(pAdjNavMesh)
									{
										TNavMeshPoly * pNextPoly = pAdjNavMesh->GetPoly( pNavMesh->GetAdjacentPoly(lastvi).GetPolyIndex() );
										// Only visit a polygon we've not already visited
										if( pNextPoly->m_TimeStamp != m_iNavMeshPolyTimeStamp )
										{
											if( ObtainEntryEdgeViaConnectionPoly( pStartPoly, pStartNavMesh, pTargetPoly, pTargetNavMesh, pNextPoly, pAdjNavMesh, pRealOrConnectionPoly, pNavMesh, outPrevNavMeshAndPoly, vStartExitEdge, vStartExitEdgeV1, vStartExitEdgeV2, domain ) )
											{
												return true;
											}
										}
									}
								}
							}
					}

					vLast = vVert;
					lasta = a;
					lastvi = vi;
			}
		}
	}
	return false;
}

const float g_fEdgeOffsetDist = 0.1f;

bool CPathServerThread::GetPointAlongSharedEdge(
	const Vector3 & vBestPointInParentPoly,
	const Vector3 & vBestPointInChildPoly,
	TNavMeshPoly * pParentPoly,
	TNavMeshPoly * pChildPoly,
	float fEntityRadius,
	Vector3 & vPoint,
	bool & bOut_LineActuallyIntersectsEdge,
	aiNavDomain domain)
{
	if(pChildPoly->m_PathParentPoly==NULL)
	{
		Assert(pChildPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY));
		return false;
	}

	CNavMesh * pParentNavMesh = CPathServer::GetNavMeshFromIndex(pParentPoly->GetNavMeshIndex(), domain);
	Assert(pParentNavMesh);
	CNavMesh * pChildNavMesh = CPathServer::GetNavMeshFromIndex(pChildPoly->GetNavMeshIndex(), domain);
	Assert(pChildNavMesh);

	// Find out if we reached the pChildPoly via a connection polygon.
	// If so we will have to perform some more complex logic, if not then we
	// can simply traverse the child's edges to find the parent.

	const s32 iParentEdge = pChildPoly->m_Struct4.m_iParentExitEdge;
	const TAdjPoly adjPoly = pParentNavMesh->GetAdjacentPoly( pParentPoly->GetFirstVertexIndex()+iParentEdge );

	const u32 iAdjNavMesh = adjPoly.GetNavMeshIndex( pParentNavMesh->GetAdjacentMeshes() );
	if(iAdjNavMesh==NAVMESH_NAVMESH_INDEX_NONE)
		return false;

	CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(iAdjNavMesh, domain);
	if(!pAdjNavMesh)
		return false;

	TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetPolyIndex());

	Vector3 vert1, vert2;

	if(pAdjPoly->GetIsDegenerateConnectionPoly())
	{
		TNavMeshAndPoly prevNavMeshAndPoly;
		prevNavMeshAndPoly.Reset();

		Vector3 vParentV1, vParentV2, vParentEdge;
		pParentNavMesh->GetVertex( pParentNavMesh->GetPolyVertexIndex( pParentPoly, iParentEdge ), vParentV1 );
		pParentNavMesh->GetVertex( pParentNavMesh->GetPolyVertexIndex( pParentPoly, (iParentEdge+1)%pParentPoly->GetNumVertices() ), vParentV2 );
		vParentEdge = vParentV2 - vParentV1;
		vParentEdge.Normalize();

		IncTimeStamp();
		if( !ObtainEntryEdgeViaConnectionPoly( pParentPoly, pParentNavMesh, pChildPoly, pChildNavMesh, pAdjPoly, pAdjNavMesh, NULL, NULL, prevNavMeshAndPoly, vParentEdge, vParentV1, vParentV2, domain ) )
		{
#if NAVMESH_OPTIMISATIONS_OFF
			//Assert(false);
#endif
			return false;
		}

		if(prevNavMeshAndPoly.m_iNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
			return false;

		CNavMesh * pPriorNavMesh = CPathServer::GetNavMeshFromIndex(prevNavMeshAndPoly.m_iNavMeshIndex, domain);
		Assert(pPriorNavMesh && pPriorNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION);
		TNavMeshPoly * pPriorPoly = pPriorNavMesh->GetPoly(prevNavMeshAndPoly.m_iPolyIndex);

		if(!GetSharedEdgeEndPoints(pPriorPoly, pChildPoly, vert1, vert2, domain))
			return false;

	}
	else
	{
 		if(!GetSharedEdgeEndPoints(pParentPoly, pChildPoly, vert1, vert2, domain))
			return false;
	}

	Vector3 vEdge = vert2 - vert1;

	if(vEdge.Mag2() > SMALL_FLOAT)
	{
		Assert(fEntityRadius >= PATHSERVER_PED_RADIUS);

		fEntityRadius -= PATHSERVER_PED_RADIUS;

		// Test to see if the line segments actually intersect.
		// If so then we'll just return the intersection point.
		if(CNavMesh::LineSegsIntersect2D(vBestPointInParentPoly, vBestPointInChildPoly, vert1, vert2, &vPoint)==SEGMENTS_INTERSECT)
		{
			bOut_LineActuallyIntersectsEdge = true;

			// If we have a non-standard entity radius, we may have to offset this point along the edge
			// to account for the difference between the entity's radius & the PATHSERVER_PED_RADIUS.
			if(fEntityRadius > 0.0f)
			{
				vEdge.NormalizeFast();
				Vector3 vVert1ToPoint = vPoint - vert1;
				Vector3 vVert2ToPoint = vPoint - vert2;
				const float fV1PLength = NormalizeAndMag(vVert1ToPoint);
				const float fV2PLength = NormalizeAndMag(vVert2ToPoint);

				if(fV1PLength < fEntityRadius)
					vPoint += vEdge * (fEntityRadius-fV1PLength);
				else if(fV2PLength < fEntityRadius)
					vPoint -= vEdge * (fEntityRadius-fV2PLength);
			}

			return true;
		}
	}

	bOut_LineActuallyIntersectsEdge = false;

	// This vector is used to nudge the chosen point away from the poly edge a bit
	Vector3 vPolyEdgeVec = vert2 - vert1;
	vPolyEdgeVec.Normalize();

	Vector3 vLineVec = vBestPointInChildPoly - vBestPointInParentPoly;
	vLineVec.Normalize();

	Vector3 vEdgeNormal;
	vEdgeNormal.Cross(vLineVec, Vector3(0, 0, 1.0f));
	float fEdgePlaneDist = - Dot(vEdgeNormal, vBestPointInParentPoly);

	float fVert1Dist = Dot(vEdgeNormal, vert1) + fEdgePlaneDist;
	float fVert2Dist = Dot(vEdgeNormal, vert2) + fEdgePlaneDist;

	if(rage::Abs(fVert1Dist) < rage::Abs(fVert2Dist))
	{
		vPoint = vert1;
		vPoint += vPolyEdgeVec * (fEntityRadius + g_fEdgeOffsetDist);
	}
	else
	{
		vPoint = vert2;
		vPoint -= vPolyEdgeVec * (fEntityRadius + g_fEdgeOffsetDist);
	}

	return true;
}

bool CPathServerThread::GetPointWithDynamicObjectVisiblityAlongSharedEdge(const Vector3 & vBestPointInPoly1, const Vector3 & vBestPointInPoly2, TNavMeshPoly * pPoly1, TNavMeshPoly * pPoly2, float fEntityRadius, Vector3 & vPoint, aiNavDomain domain)
{
	if(pPoly2->m_PathParentPoly==NULL)
	{
		NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(pPoly2->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY)); )
		return false;
	}

	CNavMesh * pPoly1NavMesh = CPathServer::GetNavMeshFromIndex(pPoly1->GetNavMeshIndex(), domain);
	Assert(pPoly1NavMesh);
	CNavMesh * pPoly2NavMesh = CPathServer::GetNavMeshFromIndex(pPoly2->GetNavMeshIndex(), domain);
	Assert(pPoly2NavMesh);

	// Find out if we reached the pChildPoly via a connection polygon.
	// If so we will have to perform some more complex logic, if not then we
	// can simply traverse the child's edges to find the parent.

	const s32 iParentEdge = pPoly2->m_Struct4.m_iParentExitEdge;
	const TAdjPoly adjPoly = pPoly1NavMesh->GetAdjacentPoly( pPoly1->GetFirstVertexIndex()+iParentEdge );

	const u32 iAdjNavMesh = adjPoly.GetNavMeshIndex( pPoly1NavMesh->GetAdjacentMeshes() );
	if(iAdjNavMesh == NAVMESH_NAVMESH_INDEX_NONE)
		return false;

	CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(iAdjNavMesh, domain);
	if(!pAdjNavMesh)
		return false;

	TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetPolyIndex());

	Vector3 vEdgeVert1, vEdgeVert2;

	if(pAdjPoly->GetIsDegenerateConnectionPoly())
	{
		TNavMeshAndPoly prevNavMeshAndPoly;
		prevNavMeshAndPoly.Reset();

		Vector3 vParentV1, vParentV2, vParentEdge;
		pPoly1NavMesh->GetVertex( pPoly1NavMesh->GetPolyVertexIndex( pPoly1, iParentEdge ), vParentV1 );
		pPoly1NavMesh->GetVertex( pPoly1NavMesh->GetPolyVertexIndex( pPoly1, (iParentEdge+1)%pPoly1->GetNumVertices() ), vParentV2 );
		vParentEdge = vParentV2 - vParentV1;
		vParentEdge.Normalize();

		IncTimeStamp();
		if( !ObtainEntryEdgeViaConnectionPoly( pPoly1, pPoly1NavMesh, pPoly2, pPoly2NavMesh, pAdjPoly, pAdjNavMesh, NULL, NULL, prevNavMeshAndPoly, vParentEdge, vParentV1, vParentV2, domain ) )
		{
#if NAVMESH_OPTIMISATIONS_OFF
			//Assert(false);
#endif
			return false;
		}

		if(prevNavMeshAndPoly.m_iNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
			return false;

		CNavMesh * pPriorNavMesh = CPathServer::GetNavMeshFromIndex(prevNavMeshAndPoly.m_iNavMeshIndex, domain);
		Assert(pPriorNavMesh && pPriorNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION);
		TNavMeshPoly * pPriorPoly = pPriorNavMesh->GetPoly(prevNavMeshAndPoly.m_iPolyIndex);

		if(!GetSharedEdgeEndPoints(pPriorPoly, pPoly2, vEdgeVert1, vEdgeVert2, domain))
			return false;
	}
	else
	{
		if(!GetSharedEdgeEndPoints(pPoly1, pPoly2, vEdgeVert1, vEdgeVert2, domain))
			return false;
	}

	Vector3 vEdge = vEdgeVert2 - vEdgeVert1;

	// If we have a non-standard actor radius then we must move edge points in by the
	// difference between the radius and the default radius which is baked into the navmesh
	if(fEntityRadius != PATHSERVER_PED_RADIUS)
	{
		Assert(fEntityRadius > PATHSERVER_PED_RADIUS);
		// Subtract the PATHSERVER_PED_RADIUS, which is baked into the navmesh
		fEntityRadius -= PATHSERVER_PED_RADIUS;

		// If this edge is too small for us to offset points by radius - they just give up on this
		if(vEdge.Mag2() < PATHSERVER_PED_RADIUS*PATHSERVER_PED_RADIUS)
			return false;
		vEdge.NormalizeFast();
		vEdge *= fEntityRadius;
		vEdgeVert1 += vEdge;
		vEdgeVert2 -= vEdge;
	}
	else
	{
		vEdge.NormalizeFast();
	}

	Vector3 vPointsToTry[3] = { vEdgeVert1 + (vEdge * g_fEdgeOffsetDist), vEdgeVert2 - (vEdge * g_fEdgeOffsetDist), (vEdgeVert1+vEdgeVert2)*0.5f };
	float fDists[3] =
	{
		((vPointsToTry[0] - vBestPointInPoly1).Mag() + (vPointsToTry[0] - vBestPointInPoly2).Mag()),
		((vPointsToTry[1] - vBestPointInPoly1).Mag() + (vPointsToTry[1] - vBestPointInPoly2).Mag()),
		((vPointsToTry[2] - vBestPointInPoly1).Mag() + (vPointsToTry[2] - vBestPointInPoly2).Mag())
	};

	// Cheesy bubble-sort by total distance.
	// NB: This is actually flawed, need to take a look...
	if(fDists[2] < fDists[1])
	{
		SwapEm(fDists[1], fDists[2]);
		SwapEm(vPointsToTry[1], vPointsToTry[2]);
	}
	if(fDists[1] < fDists[0])
	{
		SwapEm(fDists[0], fDists[1]);
		SwapEm(vPointsToTry[0], vPointsToTry[1]);
	}

	for(int t=0; t<3; t++)
	{
		if(TestDynamicObjectLOS(vBestPointInPoly1, vPointsToTry[t]) && TestDynamicObjectLOS(vPointsToTry[t], vBestPointInPoly2))
		{
			vPoint = vPointsToTry[t];
			return true;
		}
	}

	// None of the vPointsToTry had visibility.
	return false;
}


void CPathServer::ModifyMovementCosts(CPathFindMovementCosts* pMovementCosts, bool bWander, bool bFlee)
{
	if(bWander)
	{
		pMovementCosts->m_fClimbHighPenalty *= CPathServerThread::ms_fClimbMultiplier_Wander;
		pMovementCosts->m_fClimbLowPenalty *= CPathServerThread::ms_fClimbMultiplier_Wander;
	}
	else if(bFlee)
	{
		pMovementCosts->m_fClimbHighPenalty *= CPathServerThread::ms_fClimbMultiplier_Flee;
		pMovementCosts->m_fClimbLowPenalty *= CPathServerThread::ms_fClimbMultiplier_Flee;
		pMovementCosts->m_fEnterWaterPenalty *= CPathServerThread::ms_fWaterMultiplier_Flee;
		pMovementCosts->m_fLeaveWaterPenalty *= CPathServerThread::ms_fWaterMultiplier_Flee;
		pMovementCosts->m_fBeInWaterPenalty *= CPathServerThread::ms_fWaterMultiplier_Flee;
	}
	else
	{
		pMovementCosts->m_fClimbHighPenalty *= CPathServerThread::ms_fClimbMultiplier_ShortestPath;
		pMovementCosts->m_fClimbLowPenalty *= CPathServerThread::ms_fClimbMultiplier_ShortestPath;
	}
}

