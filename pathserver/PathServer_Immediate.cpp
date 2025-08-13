
// Rage headers
#include "file\device.h"
#include "file\stream.h"
#include "data\struct.h"
#include "file\asset.h"
#include "paging\rscbuilder.h"
#include "vector/geometry.h"

// Framework headers
#include "pathserver/PathServer.h"
#include "ai/navmesh/priqueue.h"
#include "fwmaths\random.h"
#include "fwmaths\vector.h"

#ifdef GTA_ENGINE
AI_NAVIGATION_OPTIMISATIONS()
NAVMESH_OPTIMISATIONS()
#endif




//***********************************************************************************************************






//************************************************************************************************************************************************
//	TestShortLineOfSightImmediate
//	Tests a line-of-sight against the navmesh immediately, bypassing the processing thread (ie. this function is intended to be called from the
//	main game thread - and musn't be used by the pathfinder).  The LOS must be short, and will fail if over SHORT_LINE_OF_SIGHT_MAXDIST in
//	length.  This function does not consider dynamic objects.
//	TODO : Enforce in some way that the calling thread is the main game thread - its essential that no attempt is made to load/unload navmeshes
//	whilst this function is running!

TNavMeshPoly * CPathServer::TestShortLineOfSightImmediate(const Vector3 & vStart, const Vector3 & vEnd, Vector3 & vIsectPos, aiNavDomain domain)
{
#if __BANK && defined(GTA_ENGINE)
	m_iNumImmediateModeLosCallsThisFrame++;
	float fTimer = 0.0f;
	START_PERF_TIMER(m_ImmediateModeTimer);
#endif

#ifdef GTA_ENGINE
	// Wait for any streaming, etc access to navmeshes to complete
	CHECK_FOR_THREAD_STALLS(m_NavMeshImmediateAccessCriticalSectionToken);
	sysCriticalSection navMeshImmediateModeDataCriticalSection(m_NavMeshImmediateAccessCriticalSectionToken);
#endif // GTA_ENGINE

	static const float fMaxDistSqr = SHORT_LINE_OF_SIGHT_MAXDIST*SHORT_LINE_OF_SIGHT_MAXDIST;
	float fMag2 = (vEnd - vStart).Mag2();
	Assert(fMag2 < fMaxDistSqr);

	if(fMag2 >= fMaxDistSqr)
	{
#if __BANK && defined(GTA_ENGINE)
		STOP_PERF_TIMER(m_ImmediateModeTimer, fTimer);
		m_fTimeSpentOnImmediateModeCallsThisFrame += fTimer;
#endif
		return NULL;
	}
	
	CNavMesh * pStartNavMesh = GetNavMeshFromIndex( GetNavMeshIndexFromPosition(vStart, domain), domain );
	if(!pStartNavMesh)
	{
#if __BANK && defined(GTA_ENGINE)
		STOP_PERF_TIMER(m_ImmediateModeTimer, fTimer);
		m_fTimeSpentOnImmediateModeCallsThisFrame += fTimer;
#endif
		return NULL;
	}
	CNavMesh * pEndNavMesh = GetNavMeshFromIndex( GetNavMeshIndexFromPosition(vEnd, domain), domain );
	if(!pEndNavMesh)
	{
#if __BANK && defined(GTA_ENGINE)
		STOP_PERF_TIMER(m_ImmediateModeTimer, fTimer);
		m_fTimeSpentOnImmediateModeCallsThisFrame += fTimer;
#endif
		return NULL;
	}

	Assert(m_iImmediateModeNumVisitedPolys == 0);

	static const float fDistBelow = 4.0f;
	Vector3 vTmp;
	u32 iStartPolyIndex = pStartNavMesh->GetPolyBelowPoint(vStart, vTmp, fDistBelow);
	u32 iEndPolyIndex = pEndNavMesh->GetPolyBelowPoint(vEnd, vTmp, fDistBelow);

	TNavMeshPoly * pStartPoly = (iStartPolyIndex!=NAVMESH_POLY_INDEX_NONE) ? pStartNavMesh->GetPoly(iStartPolyIndex) : NULL;
	TNavMeshPoly * pEndPoly = (iEndPolyIndex!=NAVMESH_POLY_INDEX_NONE) ? pEndNavMesh->GetPoly(iEndPolyIndex) : NULL;

	if(pStartPoly && !pStartPoly->GetIsDisabled() && pEndPoly && !pEndPoly->GetIsDisabled())
	{
		bool bLos = TestShortLineOfSightImmediateR(vStart, vEnd, pEndPoly, pStartPoly, NULL, domain);

		CPathServer::ResetImmediateModeVisitedPolys();

		if(bLos)
		{
#if __BANK && defined(GTA_ENGINE)
			STOP_PERF_TIMER(m_ImmediateModeTimer, fTimer);
			m_fTimeSpentOnImmediateModeCallsThisFrame += fTimer;
#endif
			return pEndPoly;
		}

		vIsectPos = m_vImmediateModeLosIsectPos;

#if __BANK && defined(GTA_ENGINE)
		STOP_PERF_TIMER(m_ImmediateModeTimer, fTimer);
		m_fTimeSpentOnImmediateModeCallsThisFrame += fTimer;
#endif
		return NULL;
	}

	vIsectPos = vStart;

#if __BANK && defined(GTA_ENGINE)
	STOP_PERF_TIMER(m_ImmediateModeTimer, fTimer);
	m_fTimeSpentOnImmediateModeCallsThisFrame += fTimer;
#endif

	return NULL;
}

bool CPathServer::TestShortLineOfSightImmediateR(const Vector3 & vStartPos, const Vector3 & vEndPos, const TNavMeshPoly * pToPoly, TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, aiNavDomain domain)
{
#if __BANK && defined(GTA_ENGINE)
	// Note, this'll be counting the num *recursions* & not the actual number of top-level calls..
	CPathServer::ms_iNumImmediateTestNavMeshLOS++;
#endif

	if(pTestPoly == pToPoly)
	{
		return true;
	}

	u32 v, lastv;
	int bIsectRet;
	bool bLOS;
	Vector3 vEdgeVert1, vEdgeVert2;

	CNavMesh * pTestPolyNavMesh = CPathServer::GetNavMeshFromIndex(pTestPoly->GetNavMeshIndex(), domain);

	lastv = pTestPoly->GetNumVertices()-1;
	for(v=0; v<pTestPoly->GetNumVertices(); v++)
	{
		const TAdjPoly & adjPoly = pTestPolyNavMesh->GetAdjacentPoly(pTestPoly->GetFirstVertexIndex()+lastv);

		// We cannot complete a LOS over an adjacency which is a climb-up/drop-down etc.
		if(adjPoly.GetOriginalPolyIndex() != NAVMESH_POLY_INDEX_NONE && adjPoly.GetAdjacencyType() == ADJACENCY_TYPE_NORMAL)
		{
			// Check that this NavMesh exists & is loaded. (It's quite possible for the refinement algorithm to
			// visit polys which the poly-pathfinder didn't..)
			CNavMesh * pAdjMesh = CPathServer::GetNavMeshFromIndex(adjPoly.GetNavMeshIndex(pTestPolyNavMesh->GetAdjacentMeshes()), domain);
			if(!pAdjMesh)
			{
				lastv = v;
				continue;
			}

			TNavMeshPoly * pAdjPoly = pAdjMesh->GetPoly(adjPoly.GetOriginalPolyIndex());
			if(pAdjPoly->GetIsDisabled())
			{
				lastv = v;
				continue;
			}

			// Make sure we don't recurse back & forwards indefinately.  Instead of using the timestamp in the poly
			// (we can't because the pathserver-thread may be running right at this moment, and uses that variable)
			// we set a flag in the poly to indicate we've visited it.  We also add the poly to a small list, and
			// then blank out the flags in all the polys we've visited afterwards.

			if(pAdjPoly != pLastPoly && (!pAdjPoly->m_iImmediateModeFlags || pAdjPoly == pToPoly))
			{
				pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, lastv), vEdgeVert1);
				pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, v), vEdgeVert2);

				bIsectRet = CNavMesh::LineSegsIntersect2D(vStartPos, vEndPos, vEdgeVert1, vEdgeVert2, &m_vImmediateModeLosIsectPos);

				if(bIsectRet == SEGMENTS_INTERSECT)
				{
					pAdjPoly->m_iImmediateModeFlags = 255;
					m_pImmediateModeVisitedPolys[m_iImmediateModeNumVisitedPolys++] = pAdjPoly;

					if(m_iImmediateModeNumVisitedPolys==IMMEDIATE_MODE_QUERY_MAXNUMVISITEDPOLYS)
					{
						return true;
					}

					bLOS = TestShortLineOfSightImmediateR(vStartPos, vEndPos, pToPoly, pAdjPoly, pTestPoly, domain);

					if(bLOS)
					{
						return true;
					}
				}
			}
		}

		lastv = v;
	}

	return false;
}




Vector3 gLosImmediate_vDirNormalized;
bool gLosImmediate_bTestForEdgeIntersection;
bool gLosImmediate_bEdgeIntersected;
Vector3 gLosImmediate_vEdgeVec;
int gLosImmediate_iEdgeHitIndex;
const float gLosImmediate_fMaxIsectDistFromEdge = 0.2f*0.2f;

bool IsDirPushingAgainstPolyClosedEdge(CNavMesh * pNavmesh, TNavMeshPoly * pPoly, const Vector3 & vStartPos, Vector3 & vOut_EdgeVec, Vector3 & vOut_IsectPos, int & vOut_iEdgeHitIndex)
{
	Vector3 vVert3d, vLastVert3d;
	Vector3 vVert2d, vLastVert2d;
	int iNumVerts = pPoly->GetNumVertices(); 
	int lastv = iNumVerts-1;

	for(int v=0; v<iNumVerts; v++)
	{
		const TAdjPoly & adjPoly = pNavmesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex()+lastv);
		if(adjPoly.GetOriginalPolyIndex()==NAVMESH_POLY_INDEX_NONE || adjPoly.GetAdjacencyType()!=ADJACENCY_TYPE_NORMAL)
		{
			pNavmesh->GetVertex( pNavmesh->GetPolyVertexIndex(pPoly, lastv), vLastVert3d);
			pNavmesh->GetVertex( pNavmesh->GetPolyVertexIndex(pPoly, v), vVert3d);

			Vector3 vEdgeVec3d = vVert3d - vLastVert3d;
			vLastVert2d = Vector3(vLastVert3d.x, vLastVert3d.y, vStartPos.z);
			vVert2d = Vector3(vVert3d.x, vVert3d.y, vStartPos.z);

			const Vector3 vEdgeVec2d = vVert2d - vLastVert2d;
			const float T = (vEdgeVec2d.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vLastVert2d, vEdgeVec2d, vStartPos) : 0.0f;
			const Vector3 vEdgePos2d = vLastVert2d + (vEdgeVec2d * T);
			const float fD2 = (vEdgePos2d - vStartPos).XYMag2();

			if(fD2 < gLosImmediate_fMaxIsectDistFromEdge)
			{
				// Is the direction pushing against the edge?
				vEdgeVec3d.Normalize();
				Vector3 vEdgeNormal = CrossProduct(ZAXIS, vEdgeVec3d);
				vEdgeNormal.z = 0.0f;
				vEdgeNormal.Normalize();

				const float fDot = DotProduct(vEdgeNormal, gLosImmediate_vDirNormalized);
				if(fDot < 0.0f)
				{
					vOut_EdgeVec = vEdgeVec3d;
					vOut_IsectPos = vLastVert3d + ((vVert3d-vLastVert3d)*T);
					vOut_iEdgeHitIndex = lastv;
					return true;
				}
			}
		}

		lastv = v;
	}

	return false;
}

TNavMeshPoly * CPathServer::TestShortLineOfSightImmediate(const Vector3 & vStartPos, const Vector2 & vLosDir, TNavMeshPoly * pStartPoly, Vector3 * pvOut_IntersectionPos, Vector3 * pvOut_HitEdgeVec, int * pOut_iEdgeHitIndex, aiNavDomain domain)
{
#if __BANK && defined(GTA_ENGINE)
	m_iNumImmediateModeLosCallsThisFrame++;
	float fTimer = 0.0f;
	START_PERF_TIMER(m_ImmediateModeTimer);
#endif

#ifdef GTA_ENGINE
	// Wait for any streaming, etc access to navmeshes to complete
	CHECK_FOR_THREAD_STALLS(m_NavMeshImmediateAccessCriticalSectionToken);
	sysCriticalSection navMeshImmediateModeDataCriticalSection(m_NavMeshImmediateAccessCriticalSectionToken);
#endif	// GTA_ENGINE

	Assert(pStartPoly);

	Vector3 vEndPos(vStartPos.x + vLosDir.x, vStartPos.y + vLosDir.y, vStartPos.z);
	static dev_float fExpandEndMinMax = 0.25f;
	TShortMinMax endMinMax;
	endMinMax.SetFloat(vEndPos.x - fExpandEndMinMax, vEndPos.y - fExpandEndMinMax, vEndPos.z - 4000.0f, vEndPos.x + fExpandEndMinMax, vEndPos.y + fExpandEndMinMax, vEndPos.z + 4000.0f);

	Vector3 vIsectPos(0.0f,0.0f,0.0f);
	gLosImmediate_bTestForEdgeIntersection = pvOut_HitEdgeVec != NULL;
	gLosImmediate_bEdgeIntersected = false;
	gLosImmediate_vEdgeVec.Zero();

	if(gLosImmediate_bTestForEdgeIntersection)
	{
		gLosImmediate_iEdgeHitIndex = -1;
		gLosImmediate_vDirNormalized = vEndPos - vStartPos;
		gLosImmediate_vDirNormalized.Normalize();
	}

	TNavMeshPoly * pEndPoly = TestShortLineOfSightImmediate_R(vStartPos, vLosDir, pStartPoly, NULL, endMinMax, vIsectPos, domain);

	if(pvOut_IntersectionPos)
		*pvOut_IntersectionPos = vIsectPos;
	if(pvOut_HitEdgeVec)
		*pvOut_HitEdgeVec = gLosImmediate_vEdgeVec;
	if(pOut_iEdgeHitIndex)
		*pOut_iEdgeHitIndex = gLosImmediate_iEdgeHitIndex;

	CPathServer::ResetImmediateModeVisitedPolys();

#if __BANK && defined(GTA_ENGINE)
	STOP_PERF_TIMER(m_ImmediateModeTimer, fTimer);
	m_fTimeSpentOnImmediateModeCallsThisFrame += fTimer;
#endif

	return pEndPoly;
}



bool CPathServer::TestShortLineOfSightImmediate_RG(Vector3& o_Vertex1, Vector3& o_Vertex2, const Vector3& vStartPos, const Vector3& vEndPos, TNavMeshPoly& testPoly, aiNavDomain domain)
{

	// mark this poly visited
	if(m_iImmediateModeNumVisitedPolys < IMMEDIATE_MODE_QUERY_MAXNUMVISITEDPOLYS)
	{
		m_pImmediateModeVisitedPolys[m_iImmediateModeNumVisitedPolys++] = &testPoly;
		testPoly.m_iImmediateModeFlags = 1;
	}
	else
	{
		return false;
	}

	CNavMesh* pTestPolyNavMesh = CPathServer::GetNavMeshFromIndex(testPoly.GetNavMeshIndex(), domain);
	if(pTestPolyNavMesh)
	{
		u32	lastv = testPoly.GetNumVertices()-1;
		for(u32 v2=0; v2<testPoly.GetNumVertices(); v2++)
		{
			u32 v1 = lastv;
			lastv = v2;
			Vector3 vEdgeVert1, vEdgeVert2, vIntersection;
			pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(&testPoly, v2), vEdgeVert1);
			pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(&testPoly, v1), vEdgeVert2);
			
			const Vector3 vStart(vStartPos.x, vStartPos.y, vEdgeVert1.z);
			const Vector3 vEnd(vEndPos.x, vEndPos.y, vEdgeVert2.z);
			/// static const float fDegenerateEdgeLength = 0.01f;

			// Otherwise use a 2d line-seg intersection function
			if(CNavMesh::LineSegsIntersect2D(vStartPos, vEndPos, vEdgeVert1, vEdgeVert2, &vIntersection) == SEGMENTS_INTERSECT )
			{
				o_Vertex1 = vEdgeVert1;
				o_Vertex2 = vEdgeVert2;
	
				TAdjPoly* pAdjacency = pTestPolyNavMesh->GetAdjacentPolysArray().Get(testPoly.GetFirstVertexIndex()+v1);
	
				// We cannot complete a LOS over an adjacency which is a climb-up/drop-down etc.
				if(pAdjacency->GetOriginalPolyIndex() != NAVMESH_POLY_INDEX_NONE 
					&& pAdjacency->GetAdjacencyType() == ADJACENCY_TYPE_NORMAL)
				{
					// Check that this NavMesh exists & is loaded. (It's quite possible for the refinement algorithm to
					// visit polys which the poly-pathfinder didn't..)
					CNavMesh * pAdjMesh = CPathServer::GetNavMeshFromIndex(pAdjacency->GetOriginalNavMeshIndex(pTestPolyNavMesh->GetAdjacentMeshes()), domain);
					if(pAdjMesh)
					{
						TNavMeshPoly* pAdjPoly = pAdjMesh->GetPoly(pAdjacency->GetOriginalPolyIndex());
						if(!pAdjPoly->GetIsDisabled())
						{
							if ( !pAdjPoly->m_iImmediateModeFlags )
							{
								return TestShortLineOfSightImmediate_RG(o_Vertex1, o_Vertex2, vIntersection, vEndPos, *pAdjPoly, domain );
							}
							else
							{
								continue;
							}
						}
					}
				}
				return true;
			}
		}
	}

	return false;
}


// NB: May need to rewrite this with a queue/stack instead of using recursion..
TNavMeshPoly * CPathServer::TestShortLineOfSightImmediate_R(const Vector3 & vStartPos, const Vector2 & vDir, TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, const TShortMinMax & endMinMax, Vector3 & vOut_IsectPos, aiNavDomain domain)
{
	const Vector3 vEndPos(vStartPos.x + vDir.x, vStartPos.y + vDir.y, vStartPos.z);
	Vector3 vEndAbove = vEndPos;
	Vector3 vEndBelow = vEndPos;
	Vector3 vEdgeVert1, vEdgeVert2;
	TAdjPoly * pAdjacency;
	
	u32 v;
	s32 lastv;

	static const float fDegenerateEdgeLengthSqr = 0.01f * 0.01f;
	static const float fPointOnLineEps = 0.01f;

RESTART_FOR_RECURSION:

	CNavMesh * pTestPolyNavMesh = CPathServer::GetNavMeshFromIndex(pTestPoly->GetNavMeshIndex(), domain);
	Assert(pTestPolyNavMesh);
	if(!pTestPolyNavMesh)
		return NULL;

	// In cases where we have no destination poly, then check for an minmax intersection with the known endpos
	// versus this polygon.  If this succeeds, then try a full polygon containment test.

	if(endMinMax.IntersectsXY(pTestPoly->m_MinMax))
	{
		if(gLosImmediate_bTestForEdgeIntersection)
		{
			if(IsDirPushingAgainstPolyClosedEdge(pTestPolyNavMesh, pTestPoly, vStartPos, gLosImmediate_vEdgeVec, vOut_IsectPos, gLosImmediate_iEdgeHitIndex))
			{
				gLosImmediate_bEdgeIntersected = true;
				vOut_IsectPos = vStartPos;
				return pTestPoly;
			}
		}

		vEndAbove.z = pTestPoly->m_MinMax.GetMaxZAsFloat() + 1.0f;
		vEndBelow.z = pTestPoly->m_MinMax.GetMinZAsFloat() - 1.0f;
		if(pTestPolyNavMesh->RayIntersectsPoly(vEndAbove, vEndBelow, pTestPoly, vOut_IsectPos))
		{
			m_iImmediateModeTestLosStackNumEntries = 0;
			return pTestPoly;
		}
	}

	lastv = pTestPoly->GetNumVertices()-1;
	for(v=0; v<pTestPoly->GetNumVertices(); v++)
	{
		pAdjacency = pTestPolyNavMesh->GetAdjacentPolysArray().Get(pTestPoly->GetFirstVertexIndex()+lastv);

		// We cannot complete a LOS over an adjacency which is a climb-up/drop-down etc.
		if(pAdjacency->GetOriginalPolyIndex() != NAVMESH_POLY_INDEX_NONE && pAdjacency->GetAdjacencyType() == ADJACENCY_TYPE_NORMAL)
		{
			// Check that this NavMesh exists & is loaded. (It's quite possible for the refinement algorithm to
			// visit polys which the poly-pathfinder didn't..)
			CNavMesh * pAdjMesh = CPathServer::GetNavMeshFromIndex(pAdjacency->GetOriginalNavMeshIndex(pTestPolyNavMesh->GetAdjacentMeshes()), domain);
			if(!pAdjMesh)
			{
				lastv = v;
				continue;
			}

			TNavMeshPoly * pAdjPoly = pAdjMesh->GetPoly(pAdjacency->GetOriginalPolyIndex());
			if(pAdjPoly->GetIsDisabled())
			{
				lastv = v;
				continue;
			}

			Assert(pAdjPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);
			Assert(!pAdjPoly->GetIsDegenerateConnectionPoly());

			//*************************************************************************************************
			// Make sure we don't recurse back & forwards indefinitely
			// However, disregard the timestamp for the "pToPoly" because we don't want to screw our chances
			// of getting a decent LOS via another polygon if the "pToPoly" is adjacent to the "pFromPoly"
			// but happens to be not directly visible by the adjoining edge..

			if(pAdjPoly != pLastPoly && pAdjPoly->m_iImmediateModeFlags == 0)
			{
				pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, lastv), vEdgeVert1);
				pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, v), vEdgeVert2);

				// If this edge is very small then the CNavMesh::LineSegsIntersect2D() function may fail.
				// In this case see if the line from vStarPos to vEndPos crosses either vertex.
				const Vector3 vEdge = vEdgeVert2 - vEdgeVert1;
				if(vEdge.XYMag2() <= fDegenerateEdgeLengthSqr)
				{
					const Vector3 vStart(vStartPos.x, vStartPos.y, vEdgeVert1.z);
					const Vector3 vEnd(vEndPos.x, vEndPos.y, vEdgeVert1.z);
					const float fDistToPoint = geomDistances::DistanceSegToPoint(vStart, vEnd-vStart, vEdgeVert1);
					if(fDistToPoint > fPointOnLineEps)
					{
						lastv = v;
						continue;
					}
				}
				// Otherwise use a 2d line-seg intersection function
				else if(!CNavMesh::LineSegsIntersect2D(vStartPos, vEndPos, vEdgeVert1, vEdgeVert2))
				{
					lastv = v;
					continue;
				}

				if(m_iImmediateModeNumVisitedPolys < IMMEDIATE_MODE_QUERY_MAXNUMVISITEDPOLYS)
				{
					m_pImmediateModeVisitedPolys[m_iImmediateModeNumVisitedPolys++] = pAdjPoly;
					pAdjPoly->m_iImmediateModeFlags = 1;

					//****************************************************************
					// This is the point of recursion in the original algorithm.
					// Instead of recursing, we'll store off the algorithm state in a
					// stack entry in "m_TestLosStack", and will jump to the label
					// "RESTART_FOR_RECURSION".

					if(m_iImmediateModeTestLosStackNumEntries >= SIZE_IMMEDIATEMODE_TEST_LOS_STACK)
					{
						Assertf(m_iImmediateModeTestLosStackNumEntries < SIZE_IMMEDIATEMODE_TEST_LOS_STACK, "TestNavMeshLOS recursion stack reached its limit of %i.", SIZE_IMMEDIATEMODE_TEST_LOS_STACK);
						m_iImmediateModeTestLosStackNumEntries = 0;
						return NULL;
					}

					TTestLosStack & stackEntry = m_ImmediateModeTestLosStack[m_iImmediateModeTestLosStackNumEntries];
					m_iImmediateModeTestLosStackNumEntries++;

					stackEntry.pTestPoly = pTestPoly;
					stackEntry.pLastPoly = pLastPoly;
					stackEntry.iVertex = v;

					// Set pTestPoly & pLastPoly to their new values, as would occur in recursive call.
					pTestPoly = pAdjPoly;
					pLastPoly = pTestPoly;
					
					goto RESTART_FOR_RECURSION;
				}

				lastv = v;
				continue; // not reqd
			}
		}
		else if(gLosImmediate_bTestForEdgeIntersection && !gLosImmediate_bEdgeIntersected)
		{
			pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, lastv), vEdgeVert1);
			pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, v), vEdgeVert2);

			Vector3 vEdgeVec3d = vEdgeVert2 - vEdgeVert1;
			Vector3 vLastVert2d = Vector3(vEdgeVert1.x, vEdgeVert1.y, vStartPos.z);
			Vector3 vVert2d = Vector3(vEdgeVert2.x, vEdgeVert2.y, vStartPos.z);

			const Vector3 vEdgeVec2d = vVert2d - vLastVert2d;
			const float T = (vEdgeVec2d.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vLastVert2d, vEdgeVec2d, vStartPos) : 0.0f;
			const Vector3 vEdgePos2d = vLastVert2d + (vEdgeVec2d * T);
			const float fD2 = (vEdgePos2d - vStartPos).XYMag2();

			if(fD2 < gLosImmediate_fMaxIsectDistFromEdge)
			{
				// Is the direction pushing against the edge?
				vEdgeVec3d.Normalize();
				Vector3 vEdgeNormal = CrossProduct(ZAXIS, vEdgeVec3d);
				vEdgeNormal.z = 0.0f;
				vEdgeNormal.Normalize();

				const float fDot = DotProduct(vEdgeNormal, gLosImmediate_vDirNormalized);
				if(fDot < 0.0f)
				{
					gLosImmediate_vEdgeVec = vEdgeVec3d;
					vOut_IsectPos = vEdgeVert1 + ((vEdgeVert2-vEdgeVert1)*T);
					gLosImmediate_iEdgeHitIndex = lastv;
					return pTestPoly;
				}
			}
		}

DROP_OUT_OF_RECURSION:	;

		lastv = v;
	}

	//********************************************************************
	// This is the point at which we would drop out of recursion.  If we
	// have any points of recursion stored in out stack, then re-enter
	// them now.

	if(m_iImmediateModeTestLosStackNumEntries > 0)
	{
		m_iImmediateModeTestLosStackNumEntries--;

		TTestLosStack & stackEntry = m_ImmediateModeTestLosStack[m_iImmediateModeTestLosStackNumEntries];

		pTestPoly = stackEntry.pTestPoly;
		pLastPoly = stackEntry.pLastPoly;
		v = stackEntry.iVertex;
		lastv = stackEntry.iVertex-1;
		if(lastv < 0)
			lastv = pTestPoly->GetNumVertices()-1;
		
		pTestPolyNavMesh = CPathServer::GetNavMeshFromIndex(pTestPoly->GetNavMeshIndex(), domain);

		goto DROP_OUT_OF_RECURSION;
	}

	m_iImmediateModeTestLosStackNumEntries = 0;
	return NULL;
}

//*************************************************************************************************************************************

TNavMeshPoly * CPathServer::SlidePointAlongNavMeshEdgeImmediate(const Vector3 & vStartPos, const Vector2 & vLosDir, TNavMeshPoly * pPoly, int & iInOut_HitEdge, Vector3 & vOutPos, aiNavDomain domain)
{
	const int iNextHitEdge = (iInOut_HitEdge+1)%pPoly->GetNumVertices();

	Vector3 vDir(vLosDir.x, vLosDir.y, 0.0f);
	vDir.Normalize();

	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);
	Assert(pNavMesh);

	Vector3 vLastVert, vVert;
	pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, iInOut_HitEdge), vLastVert);
	pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, iNextHitEdge), vVert);
	Vector3 vEdge(vVert.x - vLastVert.x, vVert.y - vLastVert.y, 0.0f);
	vEdge.Normalize();
	
	const Vector3 vNormal = CrossProduct(ZAXIS, vEdge);
	const float fDotNormal = DotProduct(vNormal, vDir);

	// If fDotNormal > 0.0f then it indicates that we are pulling away from the edge,
	// and so we should therefore disengage from it..
	if(fDotNormal > 0.0f)
	{
		iInOut_HitEdge = -1;
		vOutPos = vStartPos;
		return pPoly;
	}
	static dev_float fMinScale = 0.025f;	// Use a minimum scale to ensure we slide along edges, even when in a head-on impact
	const float fScale = Max(fDotNormal + 1.0f, fMinScale);
	const float fDot = DotProduct(vEdge, vDir);
	const float fDistLeft = vLosDir.Mag() * fScale;
	const int iMaxNumAttempts = 6;
	int iNumAttempts = 0;
	
	// Move along the direction of edge
	if(fDot > 0.0f)
	{
		Vector3 vToVert = vVert - vStartPos;
		const float fMag = vToVert.Mag();
		if(fMag < fDistLeft)
		{
			vOutPos = vVert;
			iInOut_HitEdge = iNextHitEdge;

			// Lets see if this next edge leads onto another polygon?
			TAdjPoly adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex()+iNextHitEdge);
			if(adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes())!=NAVMESH_NAVMESH_INDEX_NONE && adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
			{
				iInOut_HitEdge = -1;
				CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes()), domain);
				if(pAdjNavMesh)
				{
					TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetOriginalPolyIndex());

					// Find which vertex in the adjacent poly is the same as the one we have just reached
					// If it leads onto another closed edge, then set up as an edge collision.
					// This assumes anticlockwise winding.
					const int iThisVi = pNavMesh->GetPolyVertexIndex(pPoly, iNextHitEdge);

					while(iInOut_HitEdge==-1 && iNumAttempts < iMaxNumAttempts)
					{
						for(u32 a=0; a<pAdjPoly->GetNumVertices(); a++)
						{
							if(pAdjNavMesh->GetPolyVertexIndex(pAdjPoly, a)==iThisVi)
							{
								adjPoly = pAdjNavMesh->GetAdjacentPoly(pAdjPoly->GetFirstVertexIndex()+a);
								if(adjPoly.GetOriginalNavMeshIndex(pAdjNavMesh->GetAdjacentMeshes())==NAVMESH_NAVMESH_INDEX_NONE ||
									adjPoly.GetAdjacencyType()!=ADJACENCY_TYPE_NORMAL)
								{
									iInOut_HitEdge = a;
									break;
								}
								else if(adjPoly.GetOriginalNavMeshIndex(pAdjNavMesh->GetAdjacentMeshes())!=NAVMESH_NAVMESH_INDEX_NONE &&
									adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
								{
									pAdjNavMesh = CPathServer::GetNavMeshFromIndex(adjPoly.GetOriginalNavMeshIndex(pAdjNavMesh->GetAdjacentMeshes()), domain);
									if(pAdjNavMesh)
									{
										pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetOriginalPolyIndex());
										break;
									}
								}
							}
						}
						iNumAttempts++;
					}

					return pAdjPoly;
				}
			}
		}
		else
		{
			vToVert.Normalize();
			vOutPos = vStartPos + (vToVert * fDistLeft);
			return pPoly;
		}
	}
	// Move in the reverse direction
	else
	{
		Vector3 vToLastVert = vLastVert - vStartPos;
		const float fMag = vToLastVert.Mag();
		if(fMag < fDistLeft)
		{
			vOutPos = vLastVert;

			int iPrevHitEdge = iInOut_HitEdge-1;
			if(iPrevHitEdge < 0)
				iPrevHitEdge = pPoly->GetNumVertices()-1;

			TAdjPoly adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex()+iPrevHitEdge);
			if(adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes())!=NAVMESH_NAVMESH_INDEX_NONE && adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
			{
				const int iThisVi = pNavMesh->GetPolyVertexIndex(pPoly, iInOut_HitEdge);
				iInOut_HitEdge = -1;

				CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes()), domain);
				if(pAdjNavMesh)
				{
					TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetOriginalPolyIndex());

					// Find which vertex in the adjacent poly is the same as the one we have just reached
					// If it leads onto another closed edge, then set up as an edge collision.
					// This assumes anticlockwise winding.
					while(iInOut_HitEdge==-1 && iNumAttempts < iMaxNumAttempts)
					{
						for(u32 a=0; a<pAdjPoly->GetNumVertices(); a++)
						{
							if(pAdjNavMesh->GetPolyVertexIndex(pAdjPoly, a)==iThisVi)
							{
								int iPrevAdj = a-1;
								if(iPrevAdj < 0)
									iPrevAdj += pAdjPoly->GetNumVertices();

								adjPoly = pAdjNavMesh->GetAdjacentPoly(pAdjPoly->GetFirstVertexIndex()+iPrevAdj);
								if(adjPoly.GetOriginalNavMeshIndex(pAdjNavMesh->GetAdjacentMeshes())==NAVMESH_NAVMESH_INDEX_NONE ||
									adjPoly.GetAdjacencyType()!=ADJACENCY_TYPE_NORMAL)
								{
									iInOut_HitEdge = iPrevAdj;
									break;
								}
								else if(adjPoly.GetOriginalNavMeshIndex(pAdjNavMesh->GetAdjacentMeshes())!=NAVMESH_NAVMESH_INDEX_NONE &&
									adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
								{
									pAdjNavMesh = CPathServer::GetNavMeshFromIndex(adjPoly.GetOriginalNavMeshIndex(pAdjNavMesh->GetAdjacentMeshes()), domain);
									if(pAdjNavMesh)
									{
										pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetOriginalPolyIndex());
										break;
									}
								}
							}
						}
						iNumAttempts++;
					}
					return pAdjPoly;
				}
			}
			else
			{
				iInOut_HitEdge = iPrevHitEdge;
			}
		}
		else
		{
			vToLastVert.Normalize();
			vOutPos = vStartPos + (vToLastVert * fDistLeft);
			return pPoly;
		}
	}

	return pPoly;
}


//*************************************************************************************************************************************

TNavMeshPoly * CPathServer::GetClosestNavMeshPolyImmediate(const Vector3 & vPos, const bool bMustHavePedDensityOrPavement, const bool bMustNotBeIsolated, const float fSearchRadius, aiNavDomain domain)
{
#ifdef GTA_ENGINE
	// Wait for any streaming, etc access to navmeshes to complete
	CHECK_FOR_THREAD_STALLS(m_NavMeshImmediateAccessCriticalSectionToken);
	sysCriticalSection navMeshImmediateModeDataCriticalSection(m_NavMeshImmediateAccessCriticalSectionToken);
#endif	// GTA_ENGINE

	TNavMeshIndex iNavMesh = GetNavMeshIndexFromPosition(vPos, domain);
	if(iNavMesh==NAVMESH_NAVMESH_INDEX_NONE) return NULL;

	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMesh, domain);
	if(!pNavMesh) return NULL;

	TNavMeshPoly * pPoly = pNavMesh->GetClosestNavMeshPolyImmediate(vPos, fSearchRadius, bMustHavePedDensityOrPavement, bMustNotBeIsolated);

	CPathServer::ResetImmediateModeVisitedPolys();

	return pPoly;
}






//***************************************************************************
// This creates a single evenly distributed (by area) within the polygon.
// Note: It assumes that the polgon is convex (otherwise a more complex
// triangulation algorithm is necessary).

struct PolyPtTriangle
{
	Vector3 m_polyPts[3];
};

Vector3
CPathServerThread::CreateRandomPointInPoly(const TNavMeshPoly * pPoly, Vector3 * pPolyPts)
{
	// Generate triangles (with associated areas) from the poly.
	typedef std::pair<float,PolyPtTriangle> AreaAndTriangle;
	AreaAndTriangle AreasAndTriangles[NAVMESHPOLY_MAX_NUM_VERTICES];
	s32 AreaAndTriangleCount = 0;
	float totalPolyArea = 0.0f;
	{
		// Find out how many points are in the poly.
		int iNumPts = pPoly->GetNumVertices();
		if(iNumPts == 3)
		{
			PolyPtTriangle tri;
			tri.m_polyPts[0] = pPolyPts[0];
			tri.m_polyPts[1] = pPolyPts[1];
			tri.m_polyPts[2] = pPolyPts[2];

			const Vector3 vAB =	tri.m_polyPts[1] - tri.m_polyPts[0];
			const Vector3 vAC =	tri.m_polyPts[2] - tri.m_polyPts[0];
			const float area = (vAB * vAC).Mag();

			AreasAndTriangles[AreaAndTriangleCount].first = area;
			AreasAndTriangles[AreaAndTriangleCount].second = tri;
			++AreaAndTriangleCount;

			totalPolyArea += area;
		}
		else
		{
			// Triangulate the polygon with a very simplistic
			// and inefficient algorith (but is correct when the
			// polygon is convex).

			// Generate the poly centroid.
			Vector3 vCentroid(0.0f,0.0f,0.0f);
			for(s32 i = 0; i < iNumPts; ++i)
			{
				vCentroid += pPolyPts[i];
			}
			vCentroid /= (float)iNumPts;

			// Generate the triangles.
			for(s32 i = 0; i < iNumPts; ++i)
			{
				PolyPtTriangle tri;
				tri.m_polyPts[0] = vCentroid;
				tri.m_polyPts[1] = pPolyPts[i];
				tri.m_polyPts[2] = pPolyPts[((i+1)%iNumPts)];

				const Vector3 vAB =	tri.m_polyPts[1] - tri.m_polyPts[0];
				const Vector3 vAC =	tri.m_polyPts[2] - tri.m_polyPts[0];
				const float area = (vAB * vAC).Mag();

				AreasAndTriangles[AreaAndTriangleCount].first = area;
				AreasAndTriangles[AreaAndTriangleCount].second = tri;
				++AreaAndTriangleCount;

				totalPolyArea += area;
			}
		}
	}

	// Pick which triangle (based on area) that we should
	// choose to create the point in.
	float rouletteValue = m_RandomNumGen.GetVaried(totalPolyArea);
	s32 indexOfTriToUse = 0;
	float areaSoFar = 0.0f;
	for(; indexOfTriToUse < AreaAndTriangleCount; ++indexOfTriToUse)
	{
		areaSoFar += AreasAndTriangles[indexOfTriToUse].first;
		if(areaSoFar >= rouletteValue)
		{
			break;
		}
	}

	// Create a random point in the triangle.
	Vector3 point(0.0f, 0.0f, 0.0f);

	{
		Vector3 vAB =	AreasAndTriangles[indexOfTriToUse].second.m_polyPts[1] -
			AreasAndTriangles[indexOfTriToUse].second.m_polyPts[0];
		Vector3 vAC =	AreasAndTriangles[indexOfTriToUse].second.m_polyPts[2] -
			AreasAndTriangles[indexOfTriToUse].second.m_polyPts[0];

		float u = m_RandomNumGen.GetFloat();
		float v = m_RandomNumGen.GetFloat();
		if((u + v) > 1.0f)
		{
			u = 1 - u;
			v = 1 - v;
		}

		point = (vAB * u) + (vAC * v);

		point += AreasAndTriangles[indexOfTriToUse].second.m_polyPts[0];
	}

#if __VALIDATE_PEDGEN_COORDS_ARE_ON_CORRECT_POLYS

	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex());
	Assert(pNavMesh);

	Vector3 vRayStart = point;
	vRayStart.z += 10.0f;
	Vector3 vRayEnd = point;
	vRayEnd.z -= 10.0f;
	Vector3 vIntersect;

	bool bIntersects = pNavMesh->RayIntersectsPoly(vRayStart, vRayEnd, pPoly->GetNumVertices(), pPolyPts, vIntersect);
	Assert(bIntersects);

#endif

	return point;
}






