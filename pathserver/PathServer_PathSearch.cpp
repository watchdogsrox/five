#include "PathServer/PathServer.h"

// Rage headers 
#include "atl/inmap.h"
#include "math/angmath.h"
#include "system/xtl.h"

// Framework headers
#include "ai/navmesh/priqueue.h"
#include "ai/navmesh/tessellation.h"
#include "fwmaths/geomutil.h"
#include "fwmaths/random.h"


NAVMESH_OPTIMISATIONS()

bank_float CPathFindMovementCosts::ms_fDefaultCostFromTargetMultiplier			= 5.0f; //4.0f; // 2.0f
bank_float CPathFindMovementCosts::ms_fDefaultDistanceTravelledMultiplier		= 5.0f; //1.0f; // 2.0f
bank_float CPathFindMovementCosts::ms_fDefaultNonStraightMovementPenalty		= 1.0f;
bank_float CPathFindMovementCosts::ms_fDefaultClimbHighPenalty					= 800.0f;
bank_float CPathFindMovementCosts::ms_fDefaultClimbLowPenalty					= 400.0f;
bank_float CPathFindMovementCosts::ms_fDefaultDropDownPenalty					= 100.0f;
bank_float CPathFindMovementCosts::ms_fDefaultDropDownPenaltyPerMetre			= 5.0f;
bank_float CPathFindMovementCosts::ms_fDefaultClimbLadderPenalty				= 50.0f;
bank_float CPathFindMovementCosts::ms_fDefaultClimbLadderPenaltyPerMetre		= 5.0f;
bank_float CPathFindMovementCosts::ms_fDefaultEnterWaterPenalty					= 50.0f;
bank_float CPathFindMovementCosts::ms_fDefaultLeaveWaterPenalty					= 25.0f;
bank_float CPathFindMovementCosts::ms_fDefaultBeInWaterPenalty					= 20.0f;
bank_float CPathFindMovementCosts::ms_fDefaultClimbObjectPenaltyPerMetre		= 10.0f;
bank_float CPathFindMovementCosts::ms_fDefaultWanderPenaltyForZeroPedDensity	= 100.0f;
bank_float CPathFindMovementCosts::ms_fDefaultMoveOntoSteepSurfacePenalty		= 100.0f;
bank_float CPathFindMovementCosts::ms_fDefaultPenaltyForNoDirectionalCover		= 500.0f;
bank_float CPathFindMovementCosts::ms_fDefaultPenaltyForFavourCoverPerUnsetBit	= 75.0f;
bank_s32 CPathFindMovementCosts::ms_iDefaultPenaltyForMovingToLowerPedDensity	= 10;
bank_float CPathFindMovementCosts::ms_fDefaultAvoidTrainTracksPenalty			= 16000.0f;

bank_float CPathFindMovementCosts::ms_fFleePathDoubleBackCost					= 400.0f;
bank_float CPathFindMovementCosts::ms_fFleePathDoubleBackCostSofter				= 200.0f;
bank_float CPathFindMovementCosts::ms_fFleePathDoubleBackMultiplier				= 0.25f;

void CPathFindMovementCosts::SetDefault()
{
	m_fClimbHighPenalty						= ms_fDefaultClimbHighPenalty;
	m_fClimbLowPenalty						= ms_fDefaultClimbLowPenalty;
	m_fDropDownPenalty						= ms_fDefaultDropDownPenalty;
	m_fDropDownPenaltyPerMetre				= ms_fDefaultDropDownPenaltyPerMetre;
	m_fClimbLadderPenalty					= ms_fDefaultClimbLadderPenalty;
	m_fClimbLadderPenaltyPerMetre			= ms_fDefaultClimbLadderPenaltyPerMetre;
	m_fEnterWaterPenalty					= ms_fDefaultEnterWaterPenalty;
	m_fLeaveWaterPenalty					= ms_fDefaultLeaveWaterPenalty;
	m_fBeInWaterPenalty						= ms_fDefaultBeInWaterPenalty;
	m_fClimbObjectPenaltyPerMetre			= ms_fDefaultClimbObjectPenaltyPerMetre;
	m_fWanderPenaltyForZeroPedDensity		= ms_fDefaultWanderPenaltyForZeroPedDensity;
	m_fMoveOntoSteepSurfacePenalty			= ms_fDefaultMoveOntoSteepSurfacePenalty;
	m_fPenaltyForNoDirectionalCover			= ms_fDefaultPenaltyForNoDirectionalCover;
	m_fPenaltyForFavourCoverPerUnsetBit		= ms_fDefaultPenaltyForFavourCoverPerUnsetBit;
	m_iPenaltyForMovingToLowerPedDensity	= ms_iDefaultPenaltyForMovingToLowerPedDensity;
	m_fAvoidTrainTracksPenalty				= ms_fDefaultAvoidTrainTracksPenalty;
}

const float CPathServerThread::ms_fWanderOriginPullBackDistance			= 1.0f;		// was 2.0f;


// Define this as non-zero, to mark all starting polys with an invalid point-enum (126)

#define	__MARK_START_POLYS_WITH_POINTENUM_126		0	//1

#if __DEV
void
CPathServerThread::CheckVisitedPolys(void)
{
	for(u32 i=0; i<m_Vars.m_iNumVisitedPolys; i++)
	{
		Assert(m_VisitedPolys[i]);
		Assert(m_VisitedPolys[i]->m_AStarTimeStamp == m_iAStarTimeStamp);
	}
}
#endif

#if __DEV
void
CPathServerThread::ResetVisitedPolysFlags(void)
{
	const u32 iMask = ~((u32)(NAVMESHPOLY_OPEN | NAVMESHPOLY_CLOSED | NAVMESHPOLY_REACHEDBY_MASK | NAVMESHPOLY_ALTERNATIVE_STARTING_POLY));

	for(u32 i=0; i<m_Vars.m_iNumVisitedPolys; i++)
	{
		if(m_VisitedPolys[i])
			m_VisitedPolys[i]->AndFlags(iMask);
	}
}
#endif

#if __DEV
void
CPathServerThread::ResetVisitedPolysFlags(u32 iFlagsToReset)
{
	u32 iMask = ~iFlagsToReset;
	for(u32 i=0; i<m_Vars.m_iNumVisitedPolys; i++)
	{
		if(m_VisitedPolys[i])
			m_VisitedPolys[i]->AndFlags(iMask);
	}
}
#endif

#if __DEV
void
CPathServerThread::ClearDebugMarkedPolys(aiNavDomain domain)
{
	u32 n,p,i;

	for(n=0; n<=NAVMESH_MAX_MAP_INDEX; n++)
	{
		CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(n, domain);
		if(pNavMesh)
		{
			for(p=0; p<pNavMesh->GetNumPolys(); p++)
			{
				TNavMeshPoly * pPoly = pNavMesh->GetPoly(p);
				pPoly->SetDebugMarked(false);
			}
		}
		CHierarchicalNavData * pNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex(n);
		if(pNavData)
		{
			for(i=0; i<pNavData->GetNumNodes(); i++)
			{
				CHierarchicalNavNode * pNode = pNavData->GetNode(i);
				pNode->SetIsDebugMarked(false);
			}
		}
	}
	if(CPathServer::m_pTessellationNavMesh)
	{
		for(p=0; p<CPathServer::m_pTessellationNavMesh->GetNumPolys(); p++)
		{
			CPathServer::m_pTessellationNavMesh->GetPoly(p)->SetDebugMarked(false);
		}
	}
	for(p=0; p<MAX_PATH_VISITED_POLYS; p++)
	{
		m_VisitedPolys[p] = NULL;
	}
}
#endif

static const float fScale = (1.0f / TWO_PI) * 8.0f;

u32 GetCoverDirBitfield(const Vector3 & vTarget, const Vector3 & vSource)
{
	float fAngle;

	// Get XY differences
	const float fXdiff = vTarget.x - vSource.x; 
	float fYdiff = vTarget.y - vSource.y; 

	// Stop divide by zero
	if(fYdiff == 0.0f)
		fYdiff = 0.0001f;

	if(fXdiff > 0.0f)
	{
		if(fYdiff > 0.0f)
		{
			// Lower left quad
			fAngle = HALF_PI + (HALF_PI - (rage::Atan2f(fXdiff / fYdiff, 1.0f)));
		}
		else
		{
			// Upper Left quad
			fAngle = HALF_PI - (HALF_PI + (rage::Atan2f(fXdiff / fYdiff, 1.0f)));
		}
	}
	else
	{
		if(fYdiff > 0.0f)
		{
			// Lower right quad
			fAngle = -HALF_PI - (HALF_PI + (rage::Atan2f(fXdiff / fYdiff, 1.0f)));
		}
		else
		{
			// Upper right quad
			fAngle = -HALF_PI + (HALF_PI - (rage::Atan2f(fXdiff / fYdiff, 1.0f)));
		}
	}

	if(fAngle < 0.0f)
		fAngle += TWO_PI;

	const float fOctant = fAngle * fScale;
	const int iBitShift = (int)fOctant;
	pathAssertf(!(iBitShift<0||iBitShift>7), "bitshift is wrong");

	int iBitShift1 = iBitShift-1;
	if(iBitShift1 < 0)
		iBitShift1 += 8;

	int iBitShift2 = iBitShift+1;
	if(iBitShift2 > 7)
		iBitShift2 -= 8;

	return ( (1<<iBitShift) | (1<<iBitShift1) | (1<<iBitShift2) );
}

bool FindAdjacencyBetween(TNavMeshPoly * pPoly1, CNavMesh * pNavMesh1, TNavMeshPoly * pPoly2, CNavMesh * pNavMesh2, s32 & iAdj1, s32 & iAdj2)
{
	s32 i,j,a,b;
	for(i=0; i<pPoly1->GetNumVertices(); i++)
	{
		a = pPoly1->GetFirstVertexIndex()+i;
		if(pNavMesh1->GetAdjacentPoly(a).GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
		{
			const s32 iNavMesh1 = pNavMesh1->GetAdjacentPoly(a).GetNavMeshIndex(pNavMesh1->GetAdjacentMeshes());
			if(iNavMesh1 != NAVMESH_NAVMESH_INDEX_NONE)
			{
				const s32 iPoly1 = pNavMesh1->GetAdjacentPoly(a).GetPolyIndex();

				for(j=0; j<pPoly2->GetNumVertices(); j++)
				{
					b = pPoly2->GetFirstVertexIndex()+j;
					const TAdjPoly & adjPoly = pNavMesh2->GetAdjacentPoly(b);
					if(adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
					{
						const s32 iPoly2 = adjPoly.GetPolyIndex();
						const s32 iNavMesh2 = adjPoly.GetNavMeshIndex(pNavMesh2->GetAdjacentMeshes());

						if(iPoly1==iPoly2 && iNavMesh1==iNavMesh2)
						{
							iAdj1 = i;
							iAdj2 = j;
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool DoesEdgeIntervalOverlap(CNavMesh * pNavMesh1, TNavMeshPoly * pPoly1, const s32 iEdge1, CNavMesh * pNavMesh2, TNavMeshPoly * pPoly2, const s32 iEdge2)
{
	// Now get the vertices of the two edges we are concerned with
	Vector3 vInitialEdge[2];
	pNavMesh1->GetVertex( pNavMesh1->GetPolyVertexIndex(pPoly1, iEdge1), vInitialEdge[0]);
	pNavMesh1->GetVertex( pNavMesh1->GetPolyVertexIndex(pPoly1, (iEdge1+1)%pPoly1->GetNumVertices()), vInitialEdge[1]);

	Vector3 vAdjacentEdge[2];
	pNavMesh2->GetVertex( pNavMesh2->GetPolyVertexIndex(pPoly2, iEdge2), vAdjacentEdge[0]);
	pNavMesh2->GetVertex( pNavMesh2->GetPolyVertexIndex(pPoly2, (iEdge2+1)%pPoly2->GetNumVertices()), vAdjacentEdge[1]);

	Vector3 vInitialEdgeDir = vInitialEdge[1] - vInitialEdge[0];
	vInitialEdgeDir.Normalize();

	Vector3 vAdjacentEdgeDir = vAdjacentEdge[1] - vAdjacentEdge[0];
	vAdjacentEdgeDir.Normalize();

	ASSERT_ONLY(const float fDot = DotProduct(vInitialEdgeDir, vAdjacentEdgeDir);)
		Assert(Abs(fDot) > 0.9f);

	float fInitialEdgeMin = vAdjacentEdgeDir.Dot(vInitialEdge[0]);
	float fInitialEdgeMax = vAdjacentEdgeDir.Dot(vInitialEdge[1]);
	if(fInitialEdgeMin > fInitialEdgeMax)
		SwapEm(fInitialEdgeMin, fInitialEdgeMax);

	float fAdjacentEdgeMin = vAdjacentEdgeDir.Dot(vAdjacentEdge[0]);
	float fAdjacentEdgeMax = vAdjacentEdgeDir.Dot(vAdjacentEdge[1]);
	if(fAdjacentEdgeMin > fAdjacentEdgeMax)
		SwapEm(fAdjacentEdgeMin, fAdjacentEdgeMax);

	const float fIntervalDistance = (fInitialEdgeMin < fAdjacentEdgeMin) ?
		fAdjacentEdgeMin - fInitialEdgeMax : fInitialEdgeMin - fAdjacentEdgeMax;

	if(fIntervalDistance > 0.0f)
		return false;
	return true;
}

static dev_bool bAllowAdjacencyToNonStandard = false;

void CPathServerThread::ObtainAdjacentPolys( TNavMeshPoly ** ppOutPolys, const TAdjPoly ** ppOutAdjacencies, s32 & iNumPolys, const s32 iMaxNumPolys, TNavMeshPoly * pRealOrConnectionPoly, const TAdjPoly * pAdjacencyToPoly, CNavMesh * pNavMesh, const Vector3 & vExitEdge, const Vector3 & vExitEdgeV1, const Vector3 & vExitEdgeV2, const aiNavDomain domain ) const
{
	// Avoid possibility of infinite recursion by timestamping polygons we visit
	pRealOrConnectionPoly->m_TimeStamp = m_iNavMeshPolyTimeStamp;

	// When we recurse down to a real polygon, we add it to the list
	if( !pRealOrConnectionPoly->GetIsDegenerateConnectionPoly())
	{
		Assert(iNumPolys < iMaxNumPolys);
		if(iNumPolys < iMaxNumPolys)
		{
			ppOutAdjacencies[iNumPolys] = pAdjacencyToPoly;
			ppOutPolys[iNumPolys++] = pRealOrConnectionPoly;
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
			if( iNavMesh != NAVMESH_NAVMESH_INDEX_NONE && (adjacentPoly.GetAdjacencyType()==ADJACENCY_TYPE_NORMAL || bAllowAdjacencyToNonStandard))
			{
				pNavMesh->GetVertex( vi, vVert);
				vAdjacentEdge = vVert - vLast;

				NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(vAdjacentEdge.Mag2() > SMALL_FLOAT); )

				if(vAdjacentEdge.Mag2() > SMALL_FLOAT)
				{
					vAdjacentEdge.NormalizeFast();

					const float fDot = (vAdjacentEdge.x * vExitEdge.x) + (vAdjacentEdge.y * vExitEdge.y); //DotProduct(vExitEdge, vAdjacentEdge);)

					//NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(Abs(fDot) > 0.707f); )

					if( fDot > 0.0f )
					{
						// Test for interval overlap
						float fExitEdgeMin = vAdjacentEdge.Dot( vExitEdgeV1 );
						float fExitEdgeMax = vAdjacentEdge.Dot( vExitEdgeV2 );
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
									ObtainAdjacentPolys( ppOutPolys, ppOutAdjacencies, iNumPolys, iMaxNumPolys, pNextPoly, &adjacentPoly, pAdjNavMesh, vExitEdge, vExitEdgeV1, vExitEdgeV2, domain );
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
}

// FUNCTION : CanLeaveConnectionPolyViaThisEdge
// PURPOSE : Prevent pathfinder from leaving a connection polygon in an illegal move, defined as
// a) out of an edge whose outward-pointing normal points the same direction as the outward-pointing normal of the edge we entered by
// b) out of an edge which doesn't overlap the edge by which we entered the parent (if the parent is also a connection poly)
bool CPathServerThread::CanLeaveConnectionPolyViaThisEdge(TNavMeshPoly * pConnectionPoly, CNavMesh * pConnectionPolyNavMesh, const s32 iEdge, const u32 UNUSED_PARAM(iReachedByConnectionPolyIndex))
{
#if NAVMESH_OPTIMISATIONS_OFF
	Assert(pConnectionPoly->GetIsDegenerateConnectionPoly());	// must be a connection poly
	Assert(pConnectionPoly->m_PathParentPoly);					// must have already been visited as an adjacency
	Assert(pConnectionPolyNavMesh->GetIndexOfMesh() == NAVMESH_INDEX_TESSELLATION);
#endif

	if(!pConnectionPoly->GetIsDegenerateConnectionPoly() || !pConnectionPoly->m_PathParentPoly)
		return true;

	Vector3 p1_v1, p1_v2, p2_v1, p2_v2;

	//------------------
	// handle case (a)
	// ensure that we aren't leaving the connection poly by the same side as we entered

	CNavMesh * pParentNavMesh = fwPathServer::GetNavMeshFromIndex(pConnectionPoly->m_PathParentPoly->GetNavMeshIndex(), kNavDomainRegular);
	Assert(pParentNavMesh);

	s32 iAdjEdge;

	iAdjEdge = pParentNavMesh->GetLinkBackAdjacency(
		pConnectionPoly->m_PathParentPoly,
		pConnectionPoly->GetNavMeshIndex(),
		pConnectionPolyNavMesh->GetPolyIndex(pConnectionPoly),
		NULL,
		true);

#if NAVMESH_OPTIMISATIONS_OFF
	Assertf(iAdjEdge != -1, "between 0x%p and 0x%p", pConnectionPoly->m_PathParentPoly, pConnectionPoly);
#endif

	if(iAdjEdge == -1)
		return false;

	// this is the edge leading out of pConnectionPoly
	pConnectionPolyNavMesh->GetVertex( pConnectionPolyNavMesh->GetPolyVertexIndex( pConnectionPoly, iEdge ), p1_v1 );
	pConnectionPolyNavMesh->GetVertex( pConnectionPolyNavMesh->GetPolyVertexIndex( pConnectionPoly, (iEdge+1)%pConnectionPoly->GetNumVertices() ), p1_v2 );
	const float fThisEdgeAngle = fwAngle::GetRadianAngleBetweenPoints(p1_v2.x, p1_v2.y, p1_v1.x, p1_v1.y);

	// this is the edge which was leading in to the pConnectionPoly from its parent
	pParentNavMesh->GetVertex( pParentNavMesh->GetPolyVertexIndex( pConnectionPoly->m_PathParentPoly, iAdjEdge ), p2_v1 );
	pParentNavMesh->GetVertex( pParentNavMesh->GetPolyVertexIndex( pConnectionPoly->m_PathParentPoly, (iAdjEdge+1)%pConnectionPoly->m_PathParentPoly->GetNumVertices() ), p2_v2 );
	const float fParentEdgeAngle = fwAngle::GetRadianAngleBetweenPoints(p2_v1.x, p2_v1.y, p2_v2.x, p2_v2.y);

	if(IsClose(fParentEdgeAngle, fThisEdgeAngle, HALF_PI))
		return false;

	//------------------
	// handle case (b)
	// ensure that there is an over lap between the edge we are leaving the pConnectionPoly,
	// and the edge by which we first started visiting connection polys, from a non-connetion poly

	TNavMeshPoly * pNextPoly = pConnectionPoly;
	TNavMeshPoly * pFirstPoly = pConnectionPoly->GetPathParentPoly();
	while(pFirstPoly && pFirstPoly->GetIsDegenerateConnectionPoly())
	{
		pNextPoly = pFirstPoly;
		pFirstPoly = pFirstPoly->GetPathParentPoly();
	}

	if(!pFirstPoly)
		return true;

	Assert(!pFirstPoly->GetIsDegenerateConnectionPoly());
	Assert(pNextPoly->GetIsDegenerateConnectionPoly());

	// Obtain the edge by which we entered from pFirstPoly into pNextPoly

	CNavMesh * pFirstNavmesh = fwPathServer::GetNavMeshFromIndex(pFirstPoly->GetNavMeshIndex(), kNavDomainRegular);
	CNavMesh * pNextNavmesh = fwPathServer::GetNavMeshFromIndex(pNextPoly->GetNavMeshIndex(), kNavDomainRegular);

	s32 iInitialEdge;

	iInitialEdge = pFirstNavmesh->GetLinkBackAdjacency(
		pFirstPoly,
		pNextPoly->GetNavMeshIndex(),
		pNextNavmesh->GetPolyIndex(pNextPoly),
		NULL,
		true
	);

	NAVMESH_OPTIMISATIONS_OFF_ONLY( Assertf(iInitialEdge != -1, "between 0x%p and 0x%p", pFirstPoly, pNextPoly ); )
	if(iInitialEdge == -1)
		return false;

	Vector3 vExitEdge = p1_v2 - p1_v1;
	vExitEdge.Normalize();

	Vector3 p3_v1, p3_v2;
	pFirstNavmesh->GetVertex( pFirstNavmesh->GetPolyVertexIndex( pFirstPoly, iInitialEdge ), p3_v1 );
	pFirstNavmesh->GetVertex( pFirstNavmesh->GetPolyVertexIndex( pFirstPoly, (iInitialEdge+1)%pFirstPoly->GetNumVertices() ), p3_v2 );

#if __ASSERT && NAVMESH_OPTIMISATIONS_OFF
	Vector3 vEntranceEdge = p3_v2 - p3_v1;
	vEntranceEdge.Normalize();
	const float fDot = DotProduct(vExitEdge, vEntranceEdge);
	Assertf( Abs(fDot) > 0.9f, "between 0x%p and 0x%p", pConnectionPoly, pFirstPoly );
#endif

	float fInitialEdgeMin = vExitEdge.Dot(p1_v1);
	float fInitialEdgeMax = vExitEdge.Dot(p1_v2);
	if(fInitialEdgeMin > fInitialEdgeMax)
		SwapEm(fInitialEdgeMin, fInitialEdgeMax);

	float fAdjacentEdgeMin = vExitEdge.Dot(p3_v1);
	float fAdjacentEdgeMax = vExitEdge.Dot(p3_v2);
	if(fAdjacentEdgeMin > fAdjacentEdgeMax)
		SwapEm(fAdjacentEdgeMin, fAdjacentEdgeMax);

	const float fIntervalDistance = (fInitialEdgeMin < fAdjacentEdgeMin) ?
		fAdjacentEdgeMin - fInitialEdgeMax : fInitialEdgeMin - fAdjacentEdgeMax;

	if(fIntervalDistance >= 0.0f)
		return false;

	return true;	// overlap exists
}

void FitSearchExtentsToMinMax( CPathRequest * pPathRequest, Vector3 & vOut_Min, Vector3 & vOut_Max)
{
	vOut_Min.x = Min(pPathRequest->m_vPathStart.x, pPathRequest->m_vPathEnd.x);
	vOut_Min.y = Min(pPathRequest->m_vPathStart.y, pPathRequest->m_vPathEnd.y);
	vOut_Min.z = Min(pPathRequest->m_vPathStart.z, pPathRequest->m_vPathEnd.z);

	vOut_Max.x = Max(pPathRequest->m_vPathStart.x, pPathRequest->m_vPathEnd.x);
	vOut_Max.y = Max(pPathRequest->m_vPathStart.y, pPathRequest->m_vPathEnd.y);
	vOut_Max.z = Max(pPathRequest->m_vPathStart.z, pPathRequest->m_vPathEnd.z);

	static dev_float fSearchExtraXY = 50.0f;
	static dev_float fSearchExtraZ = 100.0f;

	vOut_Min.x -= fSearchExtraXY;
	vOut_Min.y -= fSearchExtraXY;
	vOut_Min.z -= fSearchExtraZ;

	vOut_Max.x += fSearchExtraXY;
	vOut_Max.y += fSearchExtraXY;
	vOut_Max.z += fSearchExtraZ;
}

void CPathServerThread::CalculateSearchExtents(	CPathRequest * pPathRequest, TShortMinMax & pathSearchDistanceMinMax, Vector3 & vExtentsMin, Vector3 & vExtentsMax )
{
	static dev_bool bUseFittedMinMax = true;

	float fMaxSearchDist = 0.0f;

	if(!pPathRequest->m_bWander && !pPathRequest->m_bFleeTarget)
	{
		Vector3 vToTarget = pPathRequest->m_vPathEnd - pPathRequest->m_vPathStart;

		// Given the distance from the start to the target, calculate a maximum distance that we will allow the search to spread from the start
		static const float fSearchExtentsDistMultiplier = 3.0f;
		fMaxSearchDist = vToTarget.Mag() * fSearchExtentsDistMultiplier;
		fMaxSearchDist = Min(fMaxSearchDist, m_Vars.m_fMaxPathFindDistFromOrigin);
		fMaxSearchDist = Max(fMaxSearchDist, m_Vars.m_fMinPathFindDistFromOrigin);
	}
	else
	{
		static dev_float fScritpedMaxFleeDist	= 100.0f;
		static dev_float fAmbientMaxFleeDist	= 50.0f;

		const float fMaxFleeDist = (pPathRequest->m_bMissionPed || pPathRequest->m_bScriptedRoute) ? fScritpedMaxFleeDist : fAmbientMaxFleeDist;

		if(pPathRequest->m_bFleeTarget)
		{
			pPathRequest->m_fReferenceDistance = Min(pPathRequest->m_fReferenceDistance, fMaxFleeDist);
		}

		fMaxSearchDist = pPathRequest->m_fReferenceDistance * 2.0f;
		fMaxSearchDist = Min(fMaxSearchDist, m_Vars.m_fMaxPathFindDistFromOrigin);
	}

	if(!pPathRequest->m_bFleeTarget)
	{
		// Allow user to over-ride maximum search distance
		if(pPathRequest->m_bUseLargerSearchExtents || pPathRequest->m_bScriptedRoute)
			fMaxSearchDist = 250.0f;

		if(pPathRequest->m_bDontLimitSearchExtents)
			fMaxSearchDist = 2000.0f;
	}

	// Allow a smaller search distance clamp to be passed in via the path request
	if(pPathRequest->m_fClampMaxSearchDistance != 0.0f)
		fMaxSearchDist = Min(fMaxSearchDist, pPathRequest->m_fClampMaxSearchDistance);

	// Calcaulte extents based upon path start and search distance
	vExtentsMin = Vector3( pPathRequest->m_vPathStart.x - fMaxSearchDist, pPathRequest->m_vPathStart.y - fMaxSearchDist, pPathRequest->m_vPathStart.z - fMaxSearchDist );
	vExtentsMax = Vector3( pPathRequest->m_vPathStart.x + fMaxSearchDist, pPathRequest->m_vPathStart.y + fMaxSearchDist, pPathRequest->m_vPathStart.z + fMaxSearchDist );

	// If using a shortest path search, now also calculate some extents based upon a region containing path start & path end
	// Only for non-mission/non-scripted peds as we don't want to alter existing behaviour
	// The idea here is to reduce the worst case search space for ambient peds (cops/swat really, who hammer the pathserver like crazy)
	if(bUseFittedMinMax && !pPathRequest->m_bWander && !pPathRequest->m_bFleeTarget && !pPathRequest->m_bMissionPed && !pPathRequest->m_bScriptedRoute)
	{
		Vector3 vFittedMin, vFittedMax;
		FitSearchExtentsToMinMax( pPathRequest, vFittedMin, vFittedMax);

		// Choose whichever area has the smallest XY search space
		float fNormalArea = (vExtentsMax.x - vExtentsMin.x) * (vExtentsMax.y - vExtentsMin.y);
		float fFittedArea = (vFittedMax.x - vFittedMin.x) * (vFittedMax.y - vFittedMin.y);

		if(fFittedArea < fNormalArea)
		{
			vExtentsMin = vFittedMin;
			vExtentsMax = vFittedMax;
		}
	}

	// Restrict calculated extents to the size of the map
	vExtentsMin.x = Max(vExtentsMin.x, -TShortMinMax::ms_fMaxRepresentableValue);
	vExtentsMin.y = Max(vExtentsMin.y, -TShortMinMax::ms_fMaxRepresentableValue);
	vExtentsMin.z = Max(vExtentsMin.z, -TShortMinMax::ms_fMaxRepresentableValue);

	vExtentsMax.x = Min(vExtentsMax.x, TShortMinMax::ms_fMaxRepresentableValue);
	vExtentsMax.y = Min(vExtentsMax.y, TShortMinMax::ms_fMaxRepresentableValue);
	vExtentsMax.z = Min(vExtentsMax.z, TShortMinMax::ms_fMaxRepresentableValue);

	pathSearchDistanceMinMax.SetFloat(vExtentsMin.x, vExtentsMin.y, vExtentsMin.z, vExtentsMax.x, vExtentsMax.y, vExtentsMax.z);

#if __DEV && NAVMESH_OPTIMISATIONS_OFF
	if(!pPathRequest->m_bWander && !pPathRequest->m_bFleeTarget)
	{
		s16 iPathEnd[3] = {
			MINMAX_FIXEDPT_FROM_FLOAT(pPathRequest->m_vPathEnd.x),
			MINMAX_FIXEDPT_FROM_FLOAT(pPathRequest->m_vPathEnd.y),
			MINMAX_FIXEDPT_FROM_FLOAT(pPathRequest->m_vPathEnd.z)
		};
		if(!pathSearchDistanceMinMax.LiesWithin(iPathEnd[0], iPathEnd[1], iPathEnd[2]))
		{
			pathAssertf(false, "CPathServer : WARNING - path's end (%.1f, %.1f, %.1f) is outside of search extents! (handle:0x%p)\n", pPathRequest->m_vPathEnd.x, pPathRequest->m_vPathEnd.y, pPathRequest->m_vPathEnd.z, pPathRequest->m_pContext);
		}
	}
#endif
}



EPathFindRetVal CPathServerThread::FindPath(CNavMesh * pStartNavMesh, CNavMesh * pEndNavMesh, CPathRequest * pPathRequest)
{
	TExplorePolyFunc * pExplorePolyFn;

	if(pPathRequest->m_bWander)
	{
		pExplorePolyFn = VisitPoly_Wander;
	}
	else if(pPathRequest->m_bFleeTarget)
	{
		pExplorePolyFn = VisitPoly_Flee;
	}
	else
	{
		pExplorePolyFn = VisitPoly_ShortestPath;
	}

	memset(&m_VisitPolyVars, 0, sizeof(m_VisitPolyVars));

	m_PathSearchPriorityQueue->Clear();

	m_Vars.m_iNumVisitedPolys = 0;
	m_iNumFindPathIterations = 0;

	// The penalty for moving off a pavement, when the path is instructed to keep to it.
	// ie. The ped will go this distance out of their way, in order to remain on a pavement.

	if(pPathRequest->m_bWander)
	{
		m_Vars.m_fSurfaceNotPavementPenalty = ms_fNonPavementPenalty_Wander;
	}
	else if(pPathRequest->m_bFleeTarget)
	{
		m_Vars.m_fSurfaceNotPavementPenalty = (pPathRequest->m_bSofterFleeHeuristics ? ms_fNonPavementPenalty_FleeSofter : ms_fNonPavementPenalty_Flee);
	}
	else
	{
		m_Vars.m_fSurfaceNotPavementPenalty = ms_fNonPavementPenalty_ShortestPath;
	}

	// Used for LOS tests against dynamic objects
	Vector3 vClosestPoint(0.0f, 0.0f, 0.0f);
	m_Vars.m_vLastClosestPoint = Vector3(0.0f, 0.0f, 0.0f);
	m_Vars.m_vDirFromPrevious = Vector3(0.0f, 0.0f, 0.0f);
	m_Vars.m_vVecFromLastPointToCurrentPoint = Vector3(0.0f, 0.0f, 0.0f);
	m_Vars.m_fInvRefDist = (pPathRequest->m_fReferenceDistance > 0.001f) ? (1.0f / pPathRequest->m_fReferenceDistance) : 1.0f;

	// Used for finding an alternative end-poly within some close distance of the path end pos.
	// Only when not wandering/fleeing, and a completion radius is specified in the path request.
	float fDistSqrFromTarget;
	float fCompletionRadiusSqr = pPathRequest->m_fPathSearchCompletionRadius * pPathRequest->m_fPathSearchCompletionRadius;
	float fClosestDistSqrFromTarget = FLT_MAX;
	float fClosestPolyDistSqrFromTarget = FLT_MAX;
	const bool bUseCompletionRadius = ((!pPathRequest->m_bWander && !pPathRequest->m_bFleeTarget) && fCompletionRadiusSqr > 0.0f);
	const bool bUseBestAlternativeEndPoly = pPathRequest->m_bUseBestAlternateRouteIfNoneFound;
	TNavMeshPoly * pBestAlternativeEndPoly = NULL;
	float fMaxDistSqrAchievedSoFar = 0.0f;

	pPathRequest->m_PathResultInfo.m_vClosestPointFoundToTarget = pPathRequest->m_vPathStart;

	TShortMinMax completionRadiusMinMax;
	completionRadiusMinMax.SetFloat(
		Max(pPathRequest->m_vPathEnd.x - pPathRequest->m_fPathSearchCompletionRadius, -TShortMinMax::ms_fMaxRepresentableValue),
		Max(pPathRequest->m_vPathEnd.y - pPathRequest->m_fPathSearchCompletionRadius, -TShortMinMax::ms_fMaxRepresentableValue),
		Max(pPathRequest->m_vPathEnd.z - pPathRequest->m_fPathSearchCompletionRadius, -TShortMinMax::ms_fMaxRepresentableValue),
		Min(pPathRequest->m_vPathEnd.x + pPathRequest->m_fPathSearchCompletionRadius, TShortMinMax::ms_fMaxRepresentableValue),
		Min(pPathRequest->m_vPathEnd.y + pPathRequest->m_fPathSearchCompletionRadius, TShortMinMax::ms_fMaxRepresentableValue),
		Min(pPathRequest->m_vPathEnd.z + pPathRequest->m_fPathSearchCompletionRadius, TShortMinMax::ms_fMaxRepresentableValue)
	);

	// The combined min/max of the pPoly & pAdjPoly (the move-from poly & move-to poly in the A* search)
	TShortMinMax bothPolysMinMax;

	TNavMeshPoly * pPoly = NULL;
	TNavMeshPoly * pAdjPoly;
	const TAdjPoly * pAdjacency;
	TNavMeshPoly * pAdjConnectionPoly;

	float fParentCost, fAdjacencyTypePenalty, fPenaltyForAdjPoly;
	u32 v;
	s32 iPointEnum = -1;
	CNavMesh * pNavMesh, * pAdjNavMesh;
	
	//-------------------------------------------------------------------------------------------------------------

	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	CPathServer::ms_bNoNeedToCheckObjectsForThisPoly = false;

#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	Vector3 vAdjustedStartPos, vAdjustedEndPos;

	//****************************************************************************************************
	//	Temp HACK - if either the start/end navmesh is a dynamic navmesh, then turn OFF dynamic object
	//	avoidance.  This is because the object which the navmesh is attached to was being avoided itself.

	if((pStartNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC) || (pEndNavMesh && (pEndNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC)))
	{
		pPathRequest->m_bDontAvoidDynamicObjects = true;
	}

	// All the polys' MinMax members recalculated in worldspace
	if(pStartNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC)
	{
		pStartNavMesh->CalculateAllPolyMinMaxesForDynamicNavMesh();
	}

	//***********************************************************************************
	//	Set up the LOS flags used in TestNavMeshLos for the duration of this path-search

	m_Vars.m_iLosFlags = CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_WATER_BOUNDARY | CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_TOOSTEEP_BOUNDARY;

	if(pPathRequest->m_bNeverLeavePavements)
		m_Vars.m_iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_PAVEMENT_BOUNDARY;
	if(pPathRequest->m_bConsiderFreeSpaceAroundPoly)
		m_Vars.m_iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_FREESPACE_BOUNDARY;
	if(pPathRequest->m_bUseMaxSlopeNavigable) 
		m_Vars.m_iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_MAXSLOPE_BOUNDARY;
	if(pPathRequest->m_iNumInfluenceSpheres)
		m_Vars.m_iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_INFLUENCESPHERES_BOUNDARY;
	if(pPathRequest->m_bUseVariableEntityRadius)
		m_Vars.m_iLosFlags |= CTestNavMeshLosVars::FLAG_USE_ENTITY_RADIUS;

	m_LosVars.m_fEntityRadius = pPathRequest->m_fEntityRadius - PATHSERVER_PED_RADIUS;

	//************************************************
	//	Find the polygon at which this path starts

	const bool bCheckDynamic = ((pStartNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC)!=0);
	const bool bStartPosWasChanged = (pPathRequest->m_vPathStart - pPathRequest->m_vUnadjustedStartPoint).Mag2() > 0.0f;

	m_Vars.m_pStartPoly = NULL;
	m_Vars.m_pEndPoly = NULL;

	EPathServerErrorCode eStartRet = GetNavMeshPolyForRouteEndPoints(
		pPathRequest->m_vPathStart,
		(bStartPosWasChanged || bCheckDynamic) ? NULL : &pPathRequest->m_StartNavmeshAndPoly,
		pStartNavMesh,
		m_Vars.m_pStartPoly,
		&vAdjustedStartPos,
		pPathRequest->m_fDistanceBelowStartToLookForPoly,
		pPathRequest->m_fDistanceAboveStartToLookForPoly,
		bCheckDynamic,
		true,
		domain,
		pPathRequest->m_fMaxDistanceToAdjustPathStart,
		&pPathRequest->m_vPolySearchDir
	);

	if(!m_Vars.m_pStartPoly)
	{
		eStartRet = PATH_NOT_FOUND;
	}

	// In some circumstances we don't want to allow paths to ever start in water
	if(eStartRet != PATH_NO_ERROR && pPathRequest->m_bNeverStartInWater && m_Vars.m_pStartPoly && m_Vars.m_pStartPoly->GetIsWater())
	{
		eStartRet = PATH_NOT_FOUND;
	}

	if(eStartRet != PATH_NO_ERROR)
	{
		pPathRequest->m_bRequestActive = false;
		pPathRequest->m_bComplete = true;
		pPathRequest->m_iNumPoints = 0;
		pPathRequest->m_iCompletionCode = eStartRet;
		return EPathNotFound;
	}

	Assert(m_Vars.m_pStartPoly);
	Assert(!m_Vars.m_pStartPoly->GetIsDisabled());

	pPathRequest->m_vPathStart = vAdjustedStartPos;

	m_Vars.m_vLastClosestPoint = pPathRequest->m_vPathStart;
	m_Vars.m_fMinSlopeDotProductWithUpVector = rage::Cosf(pPathRequest->m_fMaxSlopeNavigable);

	float fReferenceDistanceSqr = pPathRequest->m_fReferenceDistance * pPathRequest->m_fReferenceDistance;
	TNavMeshPoly * pBestEndPolySoFar = NULL;

	if(pPathRequest->m_bWander)
	{
		// If the starting polygon is not on pavement, then increase the requested wander-distance.
		// The purpose of this is to ensure that peds who stray from the pavement will get pulled
		// back onto it if there is any nearby.  This is combined with an increased non-pavement penalty;

		// JB: This doesn't actually increase the m_fReferenceDistance - is this a bug?
		if((pPathRequest->m_bNeverLeavePavements || pPathRequest->m_bPreferPavements) && !m_Vars.m_pStartPoly->GetIsPavement())
		{
			m_Vars.m_fSurfaceNotPavementPenalty	= (ms_fNonPavementPenalty_Wander * 3.0f);

			/*
			m_Vars.m_fMaxSearchDist = Max(m_Vars.m_fMaxSearchDist, pPathRequest->m_fReferenceDistance * 2.0f);
			m_Vars.m_fMaxSearchDist = Min(m_Vars.m_fMaxSearchDist, m_Vars.m_fMaxPathFindDistFromOrigin);

			m_Vars.m_PathSearchDistanceMinMax.SetFloat(
				pPathRequest->m_vPathStart.x - m_Vars.m_fMaxSearchDist, pPathRequest->m_vPathStart.y - m_Vars.m_fMaxSearchDist, pPathRequest->m_vPathStart.z - m_Vars.m_fMaxSearchDist,
				pPathRequest->m_vPathStart.x + m_Vars.m_fMaxSearchDist, pPathRequest->m_vPathStart.y + m_Vars.m_fMaxSearchDist, pPathRequest->m_vPathStart.z + m_Vars.m_fMaxSearchDist);
			*/

			fReferenceDistanceSqr = pPathRequest->m_fReferenceDistance * pPathRequest->m_fReferenceDistance;

			m_Vars.m_fWanderTurnPenalty			= ms_fWanderTurnPlaneWeight;
			m_Vars.m_fWander180TurnMultiplier	= ms_fWanderTurnWeight;
		}
		else
		{
			m_Vars.m_fWanderTurnPenalty			= ms_fWanderTurnPlaneWeight;
			m_Vars.m_fWander180TurnMultiplier	= ms_fWanderTurnWeight;
		}

		// The direction to initially wander
		m_Vars.m_vWanderPlaneNormal = pPathRequest->m_vReferenceVector;
		m_Vars.m_vDirFromPrevious = m_Vars.m_vWanderPlaneNormal;
		m_Vars.m_vWanderOrigin = pPathRequest->m_vPathStart - (pPathRequest->m_vReferenceVector * ms_fWanderOriginPullBackDistance);
		m_Vars.m_fWanderPlaneDist = -Dot(m_Vars.m_vWanderPlaneNormal, m_Vars.m_vWanderOrigin);
		m_Vars.m_fInitialWanderPlaneDist = m_Vars.m_fWanderPlaneDist;

		m_Vars.m_vWanderPlaneRightNormal.x = m_Vars.m_vWanderPlaneNormal.y;
		m_Vars.m_vWanderPlaneRightNormal.y = - m_Vars.m_vWanderPlaneNormal.x;
		m_Vars.m_vWanderPlaneRightNormal.z = 0.0f;
		m_Vars.m_fWanderPlaneRightDist = -Dot(m_Vars.m_vWanderPlaneRightNormal, m_Vars.m_vWanderOrigin);
		
		m_Vars.m_vLastClosestPoint = m_Vars.m_vWanderOrigin;

		// Set this flag, so that points-in-polys are selected on proximity to plane as well as distance from/to target
		m_Vars.m_bConsiderDistanceFromWanderPlanes = true;
	}
	else
	{
		m_Vars.m_bConsiderDistanceFromWanderPlanes = false;
	}

	// If we are fleeing the target or wandering, then we will not have an end poly
	if(pPathRequest->m_bFleeTarget || pPathRequest->m_bWander)
	{
		// TODO : Flee paths & wander paths can also have an early-out code path - just attempt a
		// straight LOS in the direction of the flee/wander for the desired distance.
		m_Vars.m_pEndPoly = NULL;
		m_Vars.m_pUnderwaterSecondEndPoly = NULL;
	}
	else
	{
		const EPathServerErrorCode eEndRet = GetNavMeshPolyForRouteEndPoints(
			pPathRequest->m_vPathEnd,
			NULL,
			pEndNavMesh,
			m_Vars.m_pEndPoly,
			&vAdjustedEndPos,
			pPathRequest->m_fDistanceBelowEndToLookForPoly,
			pPathRequest->m_fDistanceAboveEndToLookForPoly,
			((pEndNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC)!=0),
			true,
			domain,
			pPathRequest->m_fMaxDistanceToAdjustPathEnd
		);
		if(eEndRet != PATH_NO_ERROR)
		{
			pPathRequest->m_bRequestActive = false;
			pPathRequest->m_bComplete = true;
			pPathRequest->m_iNumPoints = 0;
			pPathRequest->m_iCompletionCode = eEndRet;
			return EPathNotFound;
		}

		Assert(m_Vars.m_pEndPoly);
		Assert(!m_Vars.m_pEndPoly->GetIsDisabled());

		if(m_Vars.m_pEndPoly)
			pPathRequest->m_vPathEnd = vAdjustedEndPos;

		// If the start and end points are identical, then return a path instantly
		if(!pPathRequest->m_bFleeTarget && !pPathRequest->m_bWander && (pPathRequest->m_vPathEnd-pPathRequest->m_vPathStart).Mag2() < 0.01f)
		{
			pPathRequest->m_iNumPoints = 1;

			pPathRequest->m_PathPoints[0] = pPathRequest->m_vPathStart;
			//pPathRequest->m_PathPoints[1] = pPathRequest->m_vPathEnd;
			pPathRequest->m_WaypointFlags[0].Clear();
			//pPathRequest->m_WaypointFlags[1].Clear();
			pPathRequest->m_iCompletionCode = PATH_FOUND;
			return EPathFoundEarlyOut;
		}

		// This should never return FALSE, as we will never reach max visited polys at this early stage
		OnFirstVisitingPoly(m_Vars.m_pEndPoly);

		// If the ending poly is a 'volume' poly, then allow this path to alternatively end upon the
		// poly which may be linked to it above or below.
		m_Vars.m_pUnderwaterSecondEndPoly = NULL;

		// Since we are requesting a normal path, let's do a basic LOS (with dynamic objects) towards the target.
		// If the LOS is clear, then we can just return a 2-point path from start to end, and save a lot of time.

		static dev_float fEarlyOutMaxDist = 10.0f;
		static dev_float fEarlyOutMaxDeltaZ = 5.0f;

		const bool bCanAttemptEarlyOut =
			(pPathRequest->m_iNumInfluenceSpheres==0) &&
			(!pPathRequest->m_bConsiderFreeSpaceAroundPoly) &&
			(!pPathRequest->m_bUseDirectionalCover) &&
			(!pPathRequest->m_bFavourEnclosedSpaces) &&
			(!pPathRequest->m_bFleeTarget) &&
			(!pPathRequest->m_bWander) &&
			((pPathRequest->m_vPathEnd - pPathRequest->m_vPathStart).Mag2() < fEarlyOutMaxDist) &&
			(Abs(pPathRequest->m_vPathEnd.z - pPathRequest->m_vPathStart.z) < fEarlyOutMaxDeltaZ);

		if(bCanAttemptEarlyOut && CPathServer::ms_bUseLosToEarlyOutPathRequests)
		{
			bool bObjectsLos;

			if(CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance)
			{
				bObjectsLos = TestDynamicObjectLOSWithRadius(pPathRequest->m_vPathStart, pPathRequest->m_vPathEnd, PATHSERVER_PED_RADIUS);
				// TODO: is this necessary seeing as ped radius is built into objects' LOS test?
			}
			else
			{
				bObjectsLos = true;
			}

			if(bObjectsLos)
			{
				bool bLos = false;

				IncTimeStamp();

				bLos = TestNavMeshLOS(
					pPathRequest->m_vPathStart, pPathRequest->m_vPathEnd,
					m_Vars.m_pStartPoly, m_Vars.m_pEndPoly, NULL,
					m_Vars.m_iLosFlags, domain);

				if(bLos)
				{
#if !__FINAL
					pPathRequest->m_iNumVisitedPolygons = 0;
					pPathRequest->m_iNumInitiallyFoundPolys = 0;
#endif

					m_Vars.m_iNumPathPolys = 0;
					m_Vars.m_iPolyWaypointFlags[0].Clear();
					m_Vars.m_iPolyWaypointFlags[1].Clear();
					pPathRequest->m_PathPoints[0] = pPathRequest->m_vPathStart;
					pPathRequest->m_PathPoints[1] = pPathRequest->m_vPathEnd;
					pPathRequest->m_PathPolys[0] = m_Vars.m_pStartPoly;
					pPathRequest->m_PathPolys[1] = m_Vars.m_pEndPoly;
					pPathRequest->m_iNumPoints = 2;

					return EPathFound;
				}
			}
		}	// bAttemptEarlyOut

		if(m_Vars.m_pStartPoly == m_Vars.m_pEndPoly)	// debuggy thing
		{
			pPathRequest->m_bProblemPathStartsAndEndsOnSamePoly = true;
		}
	}

#if !__FINAL
	m_PerfTimer->Stop();
	pPathRequest->m_fMillisecsToFindPolys += (float) m_PerfTimer->GetTimeMS();
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	bool bDontCloseThisPoly;
	float fInitialCost = 0.0f;
	vClosestPoint = pPathRequest->m_vPathStart;

	if(pPathRequest->m_bWander)
	{
		fInitialCost = 0.0f;

		if(pPathRequest->m_bIfStartNotOnPavementAllowDropsAndClimbs && !m_Vars.m_pStartPoly->GetIsPavement() && m_Vars.m_pStartPoly->GetPedDensity()==0)
		{
			pPathRequest->m_bNeverDropFromHeight = false;
		}
	}
	// If fleeing the target, invert the cost wrt the desired flee distance
	else if(pPathRequest->m_bFleeTarget)
	{
		fInitialCost = (pPathRequest->m_vPathStart - pPathRequest->m_vPathEnd).Mag2();
		if(fInitialCost > 0.0f)
			fInitialCost = (1.0f / invsqrtf_fast(fInitialCost));

		fInitialCost = rage::Max(pPathRequest->m_fReferenceDistance - fInitialCost, 0.0f);

		m_Vars.m_vVecFromLastPointToCurrentPoint = vClosestPoint - pPathRequest->m_vPathStart;
		m_Vars.m_vVecFromLastPointToCurrentPoint.z = 0.0f;
		float fMag2 = m_Vars.m_vVecFromLastPointToCurrentPoint.Mag2();
		if(fMag2 > 0.0f)
			m_Vars.m_vVecFromLastPointToCurrentPoint *= invsqrtf_fast(fMag2);

		Vector3 vVecFromLastPointToTarget = pPathRequest->m_vPathEnd - vClosestPoint;
		vVecFromLastPointToTarget.z = 0.0f;
		fMag2 = vVecFromLastPointToTarget.Mag2();
		if(fMag2 > 0.0f)
			vVecFromLastPointToTarget *= invsqrtf_fast(fMag2);

		float fSteerAwayDot = Dot(m_Vars.m_vVecFromLastPointToCurrentPoint, vVecFromLastPointToTarget);

		if(fSteerAwayDot > 0.0f)
		{
			fInitialCost += (50.0f * fSteerAwayDot);
		}
	}
	else
	{
		fInitialCost = (pPathRequest->m_vPathStart - pPathRequest->m_vPathEnd).Mag2();
		if(fInitialCost > 0.0f)
			fInitialCost = (1.0f / invsqrtf_fast(fInitialCost));
	}

#if !__FINAL && __WIN32
	if(CPathServer::ms_bDebugPathFinding)
	{
		char tmp[256];
		OutputDebugString("**********************************************************************\n");
		sprintf(tmp, "Path Handle : 0%x\n", pPathRequest->m_hHandle);
		OutputDebugString(tmp);
		sprintf(tmp, "Context : 0%p\n", pPathRequest->m_pContext);
		OutputDebugString(tmp);
		sprintf(tmp, "StartPos (%.2f, %.2f, %.2f)\n", pPathRequest->m_vPathStart.x, pPathRequest->m_vPathStart.y, pPathRequest->m_vPathStart.z);
		OutputDebugString(tmp);
		sprintf(tmp, "EndPos (%.2f, %.2f, %.2f)\n", pPathRequest->m_vPathEnd.x, pPathRequest->m_vPathEnd.y, pPathRequest->m_vPathEnd.z);
		OutputDebugString(tmp);

#if defined(GTA_ENGINE) && defined(DEBUG_DRAW) && DEBUG_DRAW
		if(m_Vars.m_pStartPoly && pStartNavMesh)
		{
			OutputDebugString(">> START POLY\n");
			CPathServer::DebugPolyText(pStartNavMesh, pStartNavMesh->GetPolyIndex(m_Vars.m_pStartPoly));
		}
		if(m_Vars.m_pEndPoly && pEndNavMesh)
		{
			OutputDebugString(">> END POLY\n");
			CPathServer::DebugPolyText(pEndNavMesh, pEndNavMesh->GetPolyIndex(m_Vars.m_pEndPoly));
		}
#endif
	}
#endif

	// This should never return FALSE, as we will never reach max visited polys at this early stage
	OnFirstVisitingPoly(m_Vars.m_pStartPoly);

	//***************************************************************************************
	//	In some circumstances we shall add a number of potential starting polys to the
	//	priority queue.  This should help in situations where only a small section of the
	//	*actual* poly under the start-pos is 'visible' outside of dynamic objects.
	//***************************************************************************************

	if(!pPathRequest->m_bDontAvoidDynamicObjects)
	{
		// The tesselation code below only supports the regular mesh domain at this time.
		// Other code was meant to force m_bDontAvoidDynamicObjects on for the others.
		Assert(domain == kNavDomainRegular);

		if(!MaybeTessellateAndMarkPolysSurroundingPathStart(pStartNavMesh, pPathRequest, fInitialCost))
			return EPathNotFound;

		Assert(m_Vars.m_pStartPoly);
		if(!m_Vars.m_pStartPoly)
			return EPathNotFound;

		NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(!m_Vars.m_pStartPoly->GetReplacedByTessellation()); )

		pStartNavMesh = CPathServer::GetNavMeshFromIndex(m_Vars.m_pStartPoly->GetNavMeshIndex(), domain);

		if(m_Vars.m_pEndPoly)
		{
			pEndNavMesh = CPathServer::GetNavMeshFromIndex(m_Vars.m_pEndPoly->GetNavMeshIndex(), domain);

			MaybeTessellatePolysSurroundingPathEnd(pEndNavMesh, pPathRequest);
			Assert(m_Vars.m_pEndPoly);
			if(!m_Vars.m_pEndPoly)
				return EPathNotFound;

			NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(!m_Vars.m_pEndPoly->GetReplacedByTessellation()); )

		}
	}
	else
	{
		m_PathSearchPriorityQueue->Insert(fInitialCost, 0.0f, m_Vars.m_pStartPoly, pPathRequest->m_vPathStart, m_Vars.m_vDirFromPrevious, 0, NAVMESH_POLY_INDEX_NONE);
	}

#ifndef GTA_ENGINE
	printf("m_pStartPoly : 0x%x\n", m_Vars.m_pStartPoly);
	printf("m_pEndPoly : 0x%x\n", m_Vars.m_pEndPoly);
#endif

	u32 iNumPathFindIterations=0;

	Assert(!(m_Vars.m_pStartPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY)));	// Actual starting poly mustn't be flagged (maybe change this flag to indicate that these are ALTERNATIVE starting polys?)
	Assert(m_PathSearchPriorityQueue->GetNodeCount());				// Must have *something* in the binheap to start with


	pPathRequest->m_PathResultInfo.m_bPathStartedOnPavement = m_Vars.m_pStartPoly->GetIsPavement();

	float fAdjPolyCost;

	//*******************************************************************************************************************************

	const bool bSleepDuringPathfind = CPathServer::m_bSleepPathServerThreadOnLongPathRequests;

	//*******************************************************************************************************************************

	u32 iQueueNodeFlags = 0;
	u32 iReachedByConnectionPolyIndex = 0;

	while(m_PathSearchPriorityQueue->RemoveTop(fParentCost, m_Vars.m_fParentDistanceTravelled, pPoly, m_Vars.m_vLastClosestPoint, m_Vars.m_vDirFromPrevious, iQueueNodeFlags, iReachedByConnectionPolyIndex))
	{
#if NAVMESH_OPTIMISATIONS_OFF
		Assert(!pPoly->GetIsDisabled());
		Assert(!pPoly->GetIsDegenerateConnectionPoly());
#endif
		if(pPoly->GetReplacedByTessellation() || pPoly->GetIsDisabled())
			continue;

		//***************************************************************************************************
		// If this is a particularly long path-request, and it ok to interrupt it (bSleepDuringPathfind)
		// then put this thread to sleep for a while.  This help situations where another thread on the
		// same HW thread/core is needing to do some processing before the game can continue, but is not
		// able to evict this current pathfinding thread due to its priority.
		// This is also present during other costly parts of the path finding - path refinement & smoothing.

#if !__FINAL
		pPathRequest->m_iNumFindPathIterations++;
#endif
		iNumPathFindIterations++;

		if(bSleepDuringPathfind && iNumPathFindIterations >= CPathServer::m_iNumFindPathIterationsBetweenSleepChecks)
		{
			iNumPathFindIterations = 0;

			m_PathRequestSleepTimer->Stop();
			const float iTimeMS = m_PathRequestSleepTimer->GetTimeMS();
			
			if(iTimeMS >= ms_iPathRequestNumMillisecsBeforeSleepingThread)
			{
#if !__FINAL
				m_PerfTimer->Stop();
				m_RequestProcessingOverallTimer->Stop();
#endif

				fwPathServer::m_bPathServerThreadSleepingDuringSearch = true;

				// give up the rest of our timeslice
				sysIpcYield(0);

				fwPathServer::m_bPathServerThreadSleepingDuringSearch = false;

#if !__FINAL
				m_RequestProcessingOverallTimer->Start();
				m_PerfTimer->Start();
				
				pPathRequest->m_iNumTimesSlept++;
#endif

				m_PathRequestSleepTimer->Reset();
			}

			m_PathRequestSleepTimer->Start();
		}

		//**********************************************************************************
		// If the thread has been forced to abandon its current path request, then quit out
		// here.  This path will be reissued the next time this thread wakes up.
		// The calling function (CPathServerThread::Run()) will ensure that the navmesh data
		// gets unlocked, and will then sleep briefly - so that the waiting thread can
		// immediately gain access to the shared navmesh data (this is usually going to be
		// the streaming system wishing to place a newly loaded a navmesh)

		if(CPathServer::m_bForceAbortCurrentRequest)
		{
			pPathRequest->m_iCompletionCode = PATH_ABORTED_DUE_TO_ANOTHER_THREAD;
			return EPathNotFound;
		}
		// Early out if path is cancelled during processing
		if(pPathRequest->m_iCompletionCode == PATH_CANCELLED)
		{
			return EPathNotFound;
		}

		//********************************************************************************************
		//	Since the pathfinding bin-heap can store both TNavMeshPoly's and CHierarchicalNavNode's
		//	in a union, it is necessary for us to be able to differentiate.  Although this leads to
		//	more complicated code - I think it'll prove very useful to be able to do mixed-mode
		//	pathfinding across bath navmesh & navnodes as necessary.
//		if(iQueueNodeFlags & PATHSERVERBINHEAP_ENTRY_IS_HIERARCHICAL_NODE)
//		{
//			continue;
//		}

		if(!OnFirstVisitingPoly(pPoly))
		{
			pPathRequest->m_iCompletionCode = PATH_RAN_OUT_VISITED_POLYS_SPACE;
			return EPathNotFound;
		}

		bDontCloseThisPoly = false;

		if(!pPoly->GetIsDegenerateConnectionPoly())
		{
#if defined(GTA_ENGINE) && defined(DEBUG_DRAW) && DEBUG_DRAW
			if(CPathServer::ms_bDebugPathFinding)
			{
				CNavMesh * pDebugNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);
				CPathServer::DebugPolyText(pDebugNavMesh, pDebugNavMesh->GetPolyIndex(pPoly));
			}
#endif

			// Has this poly been removed from it's navmesh by being tessellated into smaller fragments?
			// If so, then we shall just ignore it and continue - the starting fragment polys will be in the pathfinding stack.
			if(pPoly->TestFlags(NAVMESHPOLY_REPLACED_BY_TESSELLATION))
			{
				continue;
			}

			if(!pPathRequest->m_bWander && !pPathRequest->m_bFleeTarget)
			{
				fDistSqrFromTarget = (m_Vars.m_vLastClosestPoint - pPathRequest->m_vPathEnd).Mag2();
				if(fDistSqrFromTarget < fClosestPolyDistSqrFromTarget)
				{
					fClosestPolyDistSqrFromTarget = fDistSqrFromTarget;
					pPathRequest->m_PathResultInfo.m_vClosestPointFoundToTarget = m_Vars.m_vLastClosestPoint;
				}
			}

			if(pPathRequest->m_bWander || pPathRequest->m_bFleeTarget)
			{
				if (!pPoly->GetIsWater() || !pPathRequest->m_bFleeNeverEndInWater)
				{
					const float fDistToTargetSqr = (m_Vars.m_vLastClosestPoint - pPathRequest->m_vPathStart).Mag2();
					if(fDistToTargetSqr > fReferenceDistanceSqr)
					{
						pBestEndPolySoFar = pPoly;
						break;
					}
					else if(fDistToTargetSqr > fMaxDistSqrAchievedSoFar)
					{
						fMaxDistSqrAchievedSoFar = fDistToTargetSqr;
						pBestAlternativeEndPoly = pPoly;
					}
				}
			}

			// Have we finished successfully?
			if(pPoly == m_Vars.m_pEndPoly || pPoly == m_Vars.m_pUnderwaterSecondEndPoly)
			{
				if(pPoly == m_Vars.m_pUnderwaterSecondEndPoly)
					pBestAlternativeEndPoly = m_Vars.m_pUnderwaterSecondEndPoly;

				bool bCanEnd = true;
				if (pPathRequest->m_bEnsureLosBeforeEnding && !pPathRequest->m_bScriptedRoute)
				{
					if(CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance && !pPathRequest->m_bFleeTarget && !pPathRequest->m_bWander)
					{
						Vector3 vPrevPolyPts[NAVMESHPOLY_MAX_NUM_VERTICES];
						Vector3 vPointInPreviousPoly;

						TNavMeshPoly * pPrevPoly = pPoly->m_PathParentPoly;
						while(pPrevPoly && pPrevPoly->GetIsDegenerateConnectionPoly())
							pPrevPoly = pPrevPoly->m_PathParentPoly;

						if(pPrevPoly)
						{
							if(pPrevPoly == m_Vars.m_pStartPoly)
							{
								vPointInPreviousPoly = pPathRequest->m_vPathStart;
							}
							else
							{
								CNavMesh * pPrevPolyNavMesh = CPathServer::GetNavMeshFromIndex(pPrevPoly->GetNavMeshIndex(), domain);
								if(pPrevPoly->GetPointEnum() == NAVMESHPOLY_POINTENUM_CENTROID)
								{
									pPrevPolyNavMesh->GetPolyCentroidQuick(pPrevPoly, vPointInPreviousPoly);
								}
								else
								{
									for(v=0; v<pPrevPoly->GetNumVertices(); v++)
									{
										pPrevPolyNavMesh->GetVertex( pPrevPolyNavMesh->GetPolyVertexIndex(pPrevPoly, v), vPrevPolyPts[v] );
									}

									GetPointInPolyFromPointEnum(
										pPrevPolyNavMesh,
										pPrevPoly,
										vPrevPolyPts,
										&vPointInPreviousPoly,
										pPrevPoly->GetPointEnum(),
										ShouldUseMorePointsForPoly(*pPrevPoly, pPathRequest) );
								}
							}

							if(!TestDynamicObjectLOS(vPointInPreviousPoly, pPathRequest->m_vPathEnd))
								bCanEnd = false;
						}
					}
				}

				//***********************************************************************************
				// If we are considering dynamic objects and the start & end polys are the same,
				// and the search has completed instantly - then check if we have a LOS clear of
				// objects.  If not, then we will try to find an alternative route:
				//
				// i)  See if we can find a point to create a simple 3-vertex path inside the poly
				//
				// ii) Don't close the poly, but flag it with NAVMESHPOLY_BLOCKED_POLY.  As we visit
				//	   the adjacent polys we mark them with NAVMESHPOLY_BLOCKED_ADJACENT_POLY.

				if(bCanEnd)
				{
					bool bEndPolyIsStartingPoly = ( pPoly == m_Vars.m_pStartPoly && !pPoly->m_PathParentPoly);

					if(CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance && !pPathRequest->m_bFleeTarget && !pPathRequest->m_bWander && bEndPolyIsStartingPoly)
					{
						if(!TestDynamicObjectLOS(pPathRequest->m_vPathStart, pPathRequest->m_vPathEnd))
						{
							//******************************************************************************************
							// NB : This case warrants an Assert, but I've removed it for now whilst I work on it.
							// The bug is not fatal, but will cause small routes starting & ending on the same small
							// poly, but with no dynamic-object LOS, to fail & cause a bad (usually small) route which
							// frequently passes through the object.  Needs fixing.
							// We shall continue as normal, but must not close this poly for further processing.
							// Hopefully, we will be able to step off this poly and back into it from another
							// polygon and complete our path that way.

							bDontCloseThisPoly = true;
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}
			}
		}

		pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);

#if SANITY_CHECK_TESSELLATION
		if(pNavMesh->m_iIndexOfMesh == NAVMESH_INDEX_TESSELLATION)
		{
			char tmp[256];
			u32 iPolyIndex = pNavMesh->GetPolyIndex(pPoly);
			if(!CPathServer::m_TessellationPolysInUse.IsSet(iPolyIndex))
			{
				sprintf(tmp, "ERROR - m_TessellationPolysInUse(%i) not set, but poly has that navmesh index\n", iPolyIndex);
				OutputDebugString(tmp);
				Assert(0);
			}
			TNavMeshPoly * pPoly = pNavMesh->GetPoly(iPolyIndex);
			Assert(!pPoly->GetReplacedByTessellation());
			Assert(pPoly->GetIsTessellatedFragment());
		}
#endif

		PREFETCH_ONLY( PREFETCH_NAVMESH_POLY_ADJACENT_POLYS(pNavMesh, pPoly) );

		s32 c;

		// Visit the surrounding polys
		for(c=0; c<pPoly->GetNumVertices(); c++)
		{
RESTART_DUE_TO_TESSELLATION:	// Evil goto usage

#if NAVMESH_OPTIMISATIONS_OFF
			Assert(!pPoly->GetReplacedByTessellation());
#endif
			const TAdjPoly adjConnectionPoly = pNavMesh->GetAdjacentPoly( pPoly->GetFirstVertexIndex() + c );

#if SANITY_CHECK_TESSELLATION
			Assert(!((adjConnectionPoly.GetNavMeshIndex() != NAVMESH_INDEX_TESSELLATION && adjConnectionPoly.GetNavMeshIndex() == adjConnectionPoly.m_OriginalNavMeshIndex) && (adjConnectionPoly.m_PolyIndex != adjConnectionPoly.m_OriginalPolyIndex)));
#endif

			// Only if it exists
			if(adjConnectionPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes()) == NAVMESH_NAVMESH_INDEX_NONE || adjConnectionPoly.GetPolyIndex() == NAVMESH_POLY_INDEX_NONE)
			{
				continue;
			}

			const TNavMeshIndex iConnectionNavMesh = adjConnectionPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes());
			Assert(iConnectionNavMesh != NAVMESH_NAVMESH_INDEX_NONE);

			CNavMesh * pAdjConnectionNavMesh = CPathServer::GetNavMeshFromIndex(iConnectionNavMesh, domain);
			if(!pAdjConnectionNavMesh)
			{
				continue;
			}

			// Safeguard against a rare memory overwrite bug - this may not be the cause of the crash, but is added just in case.
#if __ASSERT
			if(adjConnectionPoly.GetPolyIndex() >= pAdjConnectionNavMesh->GetNumPolys())
			{
				Displayf("----------------------------------------------------------------------------------------------");
				Displayf("ERROR - adjConnectionPoly.GetPolyIndex() >= pAdjConnectionNavMesh->GetNumPolys()");
				Displayf("adjConnectionPoly (navmesh:%i, polyindex:%i, navIndexLookup:%i)", iConnectionNavMesh, adjConnectionPoly.GetPolyIndex(), adjConnectionPoly.GetNavMeshLookupIndex());
				Displayf("pAdjConnectionNavMesh (indexOfMesh:%i, numpolys:%i)", pAdjConnectionNavMesh->GetIndexOfMesh(), pAdjConnectionNavMesh->GetNumPolys());
				Displayf("----------------------------------------------------------------------------------------------");

				Assertf(adjConnectionPoly.GetPolyIndex() < pAdjConnectionNavMesh->GetNumPolys(), "ERROR - please include output in TTY");

				continue;
			}
#endif

			pAdjConnectionPoly = pAdjConnectionNavMesh->GetPoly(adjConnectionPoly.GetPolyIndex());

			Assert(pAdjConnectionPoly);

#if NAVMESH_OPTIMISATIONS_OFF
			//Assertf(!pAdjConnectionPoly->GetReplacedByTessellation(), "0x%x", pAdjConnectionPoly);
#endif
			if(pAdjConnectionPoly->GetReplacedByTessellation())
			{
				continue;
			}

			// Simple early out to catch cyclic traversal on untessellated mesh
			if(pAdjConnectionPoly == pPoly->GetPathParentPoly())
			{
				continue;
			}

			//--------------------------------------------------------------------
			// Obtain all the adjacent polygons along this edge

			TNavMeshPoly * pAdjacentPolys[64];
			const TAdjPoly * pAdjPolys[64];
			s32 iNumAdjacent;

			if(!pAdjConnectionPoly->GetIsDegenerateConnectionPoly())
			{
				iNumAdjacent = 1;
				pAdjacentPolys[0] = pAdjConnectionPoly;
				pAdjPolys[0] = &adjConnectionPoly;
			}
			else
			{
				Vector3 v1,v2,vEdge;
				pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex( pPoly, c ), v1 );
				pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex( pPoly, (c+1) % pPoly->GetNumVertices() ), v2 );
				vEdge = v2 - v1;

				NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(vEdge.Mag2() > SMALL_FLOAT); )
				if(vEdge.Mag2() > SMALL_FLOAT)
				{
					vEdge.NormalizeFast();

					iNumAdjacent = 0;

					IncTimeStamp();
					ObtainAdjacentPolys( pAdjacentPolys, pAdjPolys, iNumAdjacent, 64, pAdjConnectionPoly, &adjConnectionPoly, pAdjConnectionNavMesh, vEdge, v1, v2, domain );
				}
				else
				{
					continue;
				}
			}

			//-------------------------------------
			// Visit all the adjacent polygons

			for(s32 adj=0; adj<iNumAdjacent; adj++)
			{
				pAdjPoly = pAdjacentPolys[adj];
				pAdjacency = pAdjPolys[adj];

				Assert(pAdjPoly);
				Assert(!pAdjPoly->GetIsDegenerateConnectionPoly());

#if NAVMESH_OPTIMISATIONS_OFF
				//Assertf(!pAdjPoly->GetReplacedByTessellation(), "0x%x", pAdjPoly);
#endif
				if(pAdjPoly->GetReplacedByTessellation())
				{
					continue;
				}

				pAdjNavMesh = CPathServer::GetNavMeshFromIndex(pAdjPoly->GetNavMeshIndex(), domain);
				if(!pAdjNavMesh)
				{
					continue;
				}

				if(pAdjPoly->GetIsDisabled())
					continue;

				if(!OnFirstVisitingPoly(pAdjPoly))
				{
					pPathRequest->m_iCompletionCode = PATH_RAN_OUT_VISITED_POLYS_SPACE;
					return EPathNotFound;
				}

				const u32 iAdjPolyFlags = pAdjPoly->GetFlags();

				if(CPathServer::ms_bDontRevisitOpenNodes)
				{
					// Don't visit if either already open (in stack) or closed (already processed)
					if((iAdjPolyFlags & NAVMESHPOLY_OPEN) || (iAdjPolyFlags & NAVMESHPOLY_CLOSED))
					{
						continue;
					}
				}
				else
				{
					// Don't visit if closed (already processed)
					if(iAdjPolyFlags & NAVMESHPOLY_CLOSED)
					{
						continue;
					}
				}

				// Has this poly been removed from it's navmesh by being tessellated into smaller fragments?
				// If so, then we shall just ignore it and continue - the starting fragment polys will be in the pathfinding stack.
				if(iAdjPolyFlags & NAVMESHPOLY_REPLACED_BY_TESSELLATION)
				{
					continue;
				}

				// Check if this polygon intersects the maximum extents of this pathsearch (not applicable for wander/flee paths)
				if(!pAdjPoly->m_MinMax.Intersects(m_Vars.m_PathSearchDistanceMinMax))
				{
					continue;
				}

				// If considering the poly's actual slope as opposed to ignoring too-steep polys
				if(pPathRequest->m_bUseMaxSlopeNavigable)
				{
					// If the midpoint of the adjacent poly is above the top of our last poly, we are approaching from below
					if(pAdjPoly->m_MinMax.m_iMaxZ > pPoly->m_MinMax.m_iMaxZ)
					{
						// Get the normal of this pAdjPoly
						// NB: Hugely inefficient.  This needs to be cached per-poly.  Quake had an lookup-table of
						// normals, indexed by a single byte - we could do something similar, and quite possibly
						// at a lower resolution.

						Vector3 vAdjPolyNormal;
						if(pAdjNavMesh->GetPolyNormal(vAdjPolyNormal, pAdjPoly))
						{
							if(DotProduct(vAdjPolyNormal, ZAXIS) < m_Vars.m_fMinSlopeDotProductWithUpVector)
								continue;
						}
					}
				}
				/*
				// Otherwise default behaviour - don't let paths move over polys which are too steep unless we are coming from above them
				else if(iAdjPolyFlags & NAVMESHPOLY_TOO_STEEP_TO_WALK_ON)
				{
					// If the adjacent poly is steep & the midpoint of the adjacent poly is above our last position, then don't visit it
					if(pAdjPoly->m_MinMax.GetMidZAsFloat() > m_Vars.m_vLastClosestPoint.z)
						continue;
				}
				*/

				// If this is a pavement wander path which started on a pavement, then don't let it leave the pavement at all.
				if(pPathRequest->m_bWander)
				{
					// Don't let wandering peds enter the water, if they are not already in it
					if(!pPoly->GetIsWater() && pAdjPoly->GetIsWater())
					{
						continue;
					}
				}

				if(pPathRequest->m_bNeverLeavePavements && pPoly->GetIsPavement() && !pAdjPoly->GetIsPavement())
				{
					continue;
				}

				if(pPathRequest->m_bNeverEnterWater && pAdjPoly->GetIsWater() && (!pPoly->GetIsWater()))
				{
					continue;
				}

				if(pPathRequest->m_bNeverLeaveWater && (!pAdjPoly->GetIsWater()) && pPoly->GetIsWater())
				{
					continue;
				}

				if(pPathRequest->m_bNeverLeaveDeepWater && (!pAdjPoly->GetIsWater() || pAdjPoly->GetIsShallow()) && (pPoly->GetIsWater() && !pPoly->GetIsShallow()))
				{
					continue;
				}

				m_Vars.m_bAdjPolyPointsCalculated = false;


				// Create a list of all the dynamic objects we may have to consider when moving from pPoly to pAdjPoly
				CPathServerThread::ms_dynamicObjectsIntersectingPolygons.clear();

				// Only go to the bother of scanning for dynamic objects to avoid, if avoidance is actually switched on
				// OR if this adjacency type is not normal.  Even when not avoiding objects we still want to check for
				// them if we are using a drop-down or a climb.
				// However, turn off avoidance if this is a polygon with 'special-links' (ladders, etc).. This is a
				// hack because we are not able to tessellate these polygons since we lose the links (todo: fix this)

				if((CPathServer::m_eObjectAvoidanceMode!=CPathServer::ENoObjectAvoidance))
				{
					static const u32 iObjFlags = TDynamicObject::FLAG_IGNORE_NOT_OBSTACLE;

					// We need to consider objects intersecting both the pAdjPoly we are aiming to move onto,
					// and also the last *real* (non-connection) polygon we were on.  To obtain this we have
					// to rewind through our parent pointers (TODO: keep a track of this in the binheap, to
					// avoid having to follow pointers below);

					TNavMeshPoly * pPrevRealPathPoly = pPoly;
					while(pPrevRealPathPoly && pPrevRealPathPoly->GetIsDegenerateConnectionPoly())
						pPrevRealPathPoly = pPrevRealPathPoly->m_PathParentPoly;

					if(pPrevRealPathPoly)
						bothPolysMinMax.Union(pPrevRealPathPoly->m_MinMax, pAdjPoly->m_MinMax);
					else
						bothPolysMinMax = pAdjPoly->m_MinMax;

					/*
					bool bObjectInFragCache = false;
					if( CPathServer::ms_bUseTessellatedPolyObjectCache && pPoly->GetIsTessellatedFragment() && pAdjPoly->GetIsTessellatedFragment() )
					{
						TTessInfo * pPolyTessInfo = CPathServer::GetTessInfo(pPoly);
						TTessInfo * pAdjPolyTessInfo = CPathServer::GetTessInfo(pAdjPoly);
						TNavMeshPoly * pOriginalPoly = CPathServer::GetNavMeshFromIndex(pPolyTessInfo->m_iNavMeshIndex, pPathRequest->m_NavDomain)->GetPoly(pPolyTessInfo->m_iPolyIndex);
						TNavMeshPoly * pOriginalAdjPoly = CPathServer::GetNavMeshFromIndex(pAdjPolyTessInfo->m_iNavMeshIndex, pPathRequest->m_NavDomain)->GetPoly(pAdjPolyTessInfo->m_iPolyIndex);

						if(pOriginalPoly->GetIsInObjectCache() && pOriginalAdjPoly->GetIsInObjectCache())
						{
							// Single polygon
							if(pOriginalPoly == pOriginalAdjPoly)
							{
								bObjectInFragCache = m_PolyObjectsCache->GetFromCache(pPolyTessInfo->m_iNavMeshIndex, pPolyTessInfo->m_iPolyIndex, CPathServerThread::ms_dynamicObjectsIntersectingPolygons);
							}
							// Two separate polygons
							else
							{
								bObjectInFragCache = m_PolyObjectsCache->GetFromCache(pPolyTessInfo->m_iNavMeshIndex, pPolyTessInfo->m_iPolyIndex, pAdjPolyTessInfo->m_iNavMeshIndex, pAdjPolyTessInfo->m_iPolyIndex, CPathServerThread::ms_dynamicObjectsIntersectingPolygons);
							}
						}
					}
					if(!bObjectInFragCache)
					*/
					{
						if( CDynamicObjectsContainer::GetCacheEnabled() )
						{
							CDynamicObjectsContainer::GetObjectsIntersectingRegionUsingCache(bothPolysMinMax, CPathServerThread::ms_dynamicObjectsIntersectingPolygons, iObjFlags);
						}
						else
						{
							CDynamicObjectsContainer::GetObjectsIntersectingRegion(bothPolysMinMax, CPathServerThread::ms_dynamicObjectsIntersectingPolygons, iObjFlags);
						}
					}
				}

				bool bCanBeTessellated =
					fwPathServer::CanTessellateThisPoly(pAdjPoly) &&
					pAdjacency->GetAdjacencyType() == ADJACENCY_TYPE_NORMAL &&
					(CPathServer::ms_bAllowTessellationOfOpenNodes || !pAdjPoly->GetIsOpen());

				// For now, don't allow tesselation of anything but the regular mesh.
				if(domain != kNavDomainRegular)
				{
					bCanBeTessellated = false;
				}

				CPathServer::ms_bNoNeedToCheckObjectsForThisPoly = (CPathServerThread::ms_dynamicObjectsIntersectingPolygons.GetCount()==0);

				// Make sure that if we are going to evaluate climbing that we atleast got LOS
				if (pAdjacency->GetAdjacencyType() == ADJACENCY_TYPE_CLIMB_LOW || pAdjacency->GetAdjacencyType() == ADJACENCY_TYPE_CLIMB_HIGH)
				{
					if (!TestIsClimbClearOfObjects(pAdjacency->GetAdjacencyType(), pPoly, pAdjPoly, pNavMesh, pAdjNavMesh))
						continue;
				}

				// Special case for peds who are stuck on isolated tables, etc and are fleeing but unable to use drop-downs
				// due to obstructions by surrounding objects.  Just ignore the objects in these cases.
				if(pPathRequest->m_bFleeTarget &&
					pAdjacency->GetAdjacencyType()==ADJACENCY_TYPE_DROPDOWN &&
					pPoly->GetIsIsolated())
				{
					CPathServer::ms_bNoNeedToCheckObjectsForThisPoly = true;
				}

				//-----------------------------------------------------------------------------------------

				m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = false;
				m_Vars.m_bMustOpenDoor = false;

				iPointEnum = -1;
				fAdjPolyCost = 0.0f;

				eExploreFuncRetVal retVal;

				// If preemptively tessellating & this poly is near objects and suitable for tessellation, then force the retVal to be eMustTessellateThisPoly.
				if( CPathServer::ms_bDoPolyTessellation &&
					CPathServer::ms_bDoPreemptiveTessellation &&
					bCanBeTessellated &&
					(CPathServer::ms_bNoNeedToCheckObjectsForThisPoly==false) &&
					(CPathServer::m_eObjectAvoidanceMode == CPathServer::EPolyTessellation) )
				{
					retVal = eMustTessellateThisPoly;
				}

				// See if we can traverse across this adjacency type
				else if( !IsThisAdjacencyTypeOk(pPathRequest, *pAdjacency))
				{
					retVal = eCannotVisitThisPoly;
				}
				// Otherwise evaluate this poly as normal.  If tessellation is required retVal will be set appropriately
				else
				{
					m_VisitPolyVars.m_pFromNavMesh = pNavMesh;
					m_VisitPolyVars.m_pFromPoly = pPoly;
					m_VisitPolyVars.m_pToPoly = pAdjPoly;
					m_VisitPolyVars.m_pToNavMesh = pAdjNavMesh;		
					m_VisitPolyVars.m_vPointInFromPoly = m_Vars.m_vLastClosestPoint;
					m_VisitPolyVars.m_vTargetPos = pPathRequest->m_vPathEnd;
					m_VisitPolyVars.m_iAdjacencyIndex = c;

					// Call the specific search function here.
					retVal = pExplorePolyFn(
						pPoly,
						pAdjPoly,
						pAdjNavMesh,
						vClosestPoint,
						fAdjPolyCost,
						iPointEnum,
						bCanBeTessellated);
				}

				if(retVal == eCannotVisitThisPoly)
				{
					continue;
				}
				else if(retVal == eMustTessellateThisPoly)
				{
					// We will only ever get a return value of "eMustTessellateThisPoly" when the mode is EPolyTessellation.
					Assert(CPathServer::m_eObjectAvoidanceMode == CPathServer::EPolyTessellation);
					if(CPathServer::m_eObjectAvoidanceMode != CPathServer::EPolyTessellation)
						continue;

					// If we couldn't get to our destination because of dynamic objects, then perhaps
					// we can tessellate this adjacent polygon.  The fragment associated with the edge
					// we are crossing should then be put at the head of the queue.

#if SANITY_CHECK_TESSELLATION
					Assert(!pPoly->GetReplacedByTessellation());
					Assert(!pAdjPoly->GetReplacedByTessellation());
#endif

					if(bCanBeTessellated &&
						fwPathServer::IsRoomToTessellateThisPoly(pAdjPoly) &&
						(CPathServer::ms_bDoPreemptiveTessellation || m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly) &&
						/*(!pAdjPoly->TestFlags(NAVMESHPOLY_HAS_SPECIAL_LINKS)) &&*/
						CPathServer::ms_bDoPolyTessellation)
					{
						u32 iPolyIndex = pNavMesh->GetPolyIndex(pPoly);

#if SANITY_CHECK_TESSELLATION
						if(pNavMesh->m_iIndexOfMesh == NAVMESH_INDEX_TESSELLATION)
						{
							char tmp[256];
							if(!CPathServer::m_TessellationPolysInUse.IsSet(iPolyIndex))
							{
								sprintf(tmp, "ERROR - m_TessellationPolysInUse(%i) not set, but poly has that navmesh index\n");
								OutputDebugString(tmp);
								Assert(0);
							}
							TNavMeshPoly * pPrevPoly = pNavMesh->GetPoly(iPolyIndex);
							Assert(!pPrevPoly->GetReplacedByTessellation());
							Assert(pPrevPoly->GetIsTessellatedFragment());
						}
#endif

						/*
						if(CPathServer::ms_bUseTessellatedPolyObjectCache)
						{
							if(!pAdjPoly->GetIsTessellatedFragment())
							{
								Assert(!pAdjPoly->GetIsInObjectCache());

								CPathServerThread::ms_dynamicObjectsIntersectingPolygons.clear();

								if( CDynamicObjectsContainer::GetCacheEnabled() )
								{
									CDynamicObjectsContainer::GetObjectsIntersectingRegionUsingCache(pAdjPoly->m_MinMax, CPathServerThread::ms_dynamicObjectsIntersectingPolygons, TDynamicObject::FLAG_IGNORE_NOT_OBSTACLE);
								}
								else
								{
									CDynamicObjectsContainer::GetObjectsIntersectingRegion(pAdjPoly->m_MinMax, CPathServerThread::ms_dynamicObjectsIntersectingPolygons, TDynamicObject::FLAG_IGNORE_NOT_OBSTACLE);
								}

								if( CPathServerThread::ms_dynamicObjectsIntersectingPolygons.GetCount() < POLY_OBJECTS_CACHE_MAX_OBJECTS &&
									m_PolyObjectsCache->AddToCache(pAdjNavMesh->GetIndexOfMesh(), pAdjNavMesh->GetPolyIndex(pAdjPoly), CPathServerThread::ms_dynamicObjectsIntersectingPolygons) )
								{
									pAdjPoly->SetIsInObjectCache(true);
								}
							}
						}
						*/

						Assert(domain == kNavDomainRegular);	// Should have checked earlier, the tesselation stuff we're going to run next doesn't support other data sets.
						const bool bTessellatedOk = TessellateNavMeshPolyAndCreateDegenerateConnections(
							pAdjNavMesh, pAdjPoly, pAdjNavMesh->GetPolyIndex(pAdjPoly),
							false, (pAdjPoly == m_Vars.m_pEndPoly),
							NULL, //&pStartingTessellatedPoly,
							pNavMesh->GetIndexOfMesh(),
							iPolyIndex
							);
						Assert(bTessellatedOk);

						if(bTessellatedOk)
						{
#if SANITY_CHECK_TESSELLATION
							Assert(pStartingTessellatedPoly->GetIsTessellatedFragment());
							u32 iTessIdx = CPathServer::m_pTessellationNavMesh->GetPolyIndex(pStartingTessellatedPoly);
							Assert(CPathServer::m_TessellationPolysInUse.IsSet(iTessIdx));
#endif
							// Restart the visiting of pAdjPoly, which will be the newly tessellated fragment
							goto RESTART_DUE_TO_TESSELLATION;
						}
						else
						{
#if SANITY_CHECK_TESSELLATION
							Assert(0);
#endif
						}
					}
				}

#if NAVMESH_OPTIMISATIONS_OFF
				Assert(!pPoly->GetIsDegenerateConnectionPoly());
				Assert(!pAdjPoly->GetIsDegenerateConnectionPoly());

				Assert(!pPoly->GetReplacedByTessellation());
				Assert(!pAdjPoly->GetReplacedByTessellation());
#endif
				if(pAdjPoly->GetReplacedByTessellation())
					continue;

				// TODO: ALL THE LOGIC BELOW SHOULD USE THE PREVIOUS *REAL* POLY, AND NOT THE
				// "pPoly" POINTER WHICH MAY BE A DEGENERATE CONNECTION POLY..
				if(iPointEnum < 0)
				{
					// We couldn't find a point in the adjacent poly that we can see, but we
					// know that the adjacent poly is the m_pEndPoly we are looking for.
					if(bUseBestAlternativeEndPoly || pAdjPoly == m_Vars.m_pEndPoly)
					{
						if(bUseBestAlternativeEndPoly || bUseCompletionRadius)
						{
#ifdef GTA_ENGINE
#if AI_OPTIMISATIONS_OFF
							Assert((!pPathRequest->m_bWander) && (!pPathRequest->m_bFleeTarget));
#endif
#endif
							// New version of this may help paths to complete more frequently.
							// However it may cause paths to be found, when there is in fact no route there..
							// If the end poly overlaps the completion radius sufficiently (which it should)
							// then mark this poly as an alternative ending poly.
							if(bUseBestAlternativeEndPoly || completionRadiusMinMax.Intersects(pAdjPoly->m_MinMax))
							{
								fDistSqrFromTarget = (m_Vars.m_vLastClosestPoint - pPathRequest->m_vPathEnd).Mag2();
								if(fDistSqrFromTarget < fClosestDistSqrFromTarget)
								{
									pBestAlternativeEndPoly = pPoly;
									fClosestDistSqrFromTarget = fDistSqrFromTarget;
								}
							}
						}
					}
					continue;
				}

				//***************************************************************************************
				//	If this was a drop-down then check there is a dynamic object LOS from the start pos
				//	to the endpos's XY at the starting height.  Why?  This ensures that we can actually
				//	walk over the edge to accomplish the drop-down.  Although the pExplorePolyFn does test
				//	line-of-sight, this can sometimes miss dynamic objects since the Z values between the
				//	poly & adjacent poly can be very different.

				if(CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance && pAdjacency->GetAdjacencyType()==ADJACENCY_TYPE_DROPDOWN)
				{
					const Vector3 vDropDownEnd(vClosestPoint.x, vClosestPoint.y, m_Vars.m_vLastClosestPoint.z);
					if(!TestDynamicObjectLOS(m_Vars.m_vLastClosestPoint, vDropDownEnd, CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
					{
						continue;
					}
				}

				//***************************************************************************************************************

				// the penalty applied for distance traveled
				fPenaltyForAdjPoly = CPathFindMovementCosts::ms_fDefaultDistanceTravelledMultiplier;
				//float fDistanceTravelled;	// = 0.0f; //(m_Vars.m_fDistanceFromLastPointToChosenPoint * fPenaltyForAdjPoly);
				float fCost;				// = 0.0f;

				// Add penalty for entering/being-in water
				if(pAdjPoly->GetIsWater())
				{
					if(!pPoly->GetIsWater())
						fPenaltyForAdjPoly += pPathRequest->m_MovementCosts.m_fEnterWaterPenalty;

					// Only penalise being in water if neither start nor end is underwater
					fPenaltyForAdjPoly += pPathRequest->m_MovementCosts.m_fBeInWaterPenalty;
				}

				// Don't let paths move over polys which are too steep unless we are coming from above them
				// If the adjacent poly is steep & the midpoint of the adjacent poly is above our last position, then don't visit it
				if(((iAdjPolyFlags & NAVMESHPOLY_TOO_STEEP_TO_WALK_ON)!=0) && (!pPathRequest->m_bAllowToNavigateUpSteepPolygons))
				{
					if(vClosestPoint.z > m_Vars.m_vLastClosestPoint.z)
						continue;

					// If moving from a non-steep polygon then always add a penalty
					if(!pPoly->TestFlags(NAVMESHPOLY_TOO_STEEP_TO_WALK_ON))
						fPenaltyForAdjPoly += pPathRequest->m_MovementCosts.m_fMoveOntoSteepSurfacePenalty;
				}

				// For wandering, accumulate parent's cost - this will prevent us from wandering off the pavement too long
				if(pPathRequest->m_bWander)
				{
					fCost = fParentCost;
					// Add the cost of movement scaled by distance moved, and accumulate the parent's distance
					fCost += (m_Vars.m_fDistanceFromLastPointToChosenPoint * (fPenaltyForAdjPoly + fAdjPolyCost)) + m_Vars.m_fParentDistanceTravelled;

					if(pPathRequest->m_bPreferDownHill)
					{
						// If we are wandering and doing a task thats likes downhill (skiing/skateboarding/rollerblading/etc)
						// then use the altitude of the nav square as an absolute cost

						const float fUphillCost = (vClosestPoint.z - m_Vars.m_vLastClosestPoint.z) * ms_fPreferDownhillScaleFactor;
						fCost += MAX(fUphillCost, 0.0f);
					}

					int iDeltaDensity = pPoly->GetPedDensity() - pAdjPoly->GetPedDensity();
					if(iDeltaDensity > 0)
					{
						fCost += (iDeltaDensity * pPathRequest->m_MovementCosts.m_iPenaltyForMovingToLowerPedDensity) * m_Vars.m_fDistanceFromLastPointToChosenPoint;
					}
				}
				else if(pPathRequest->m_bFleeTarget)
				{
					fCost = fParentCost;
					fCost += m_Vars.m_fDistanceFromLastPointToChosenPoint * fPenaltyForAdjPoly + fAdjPolyCost;// + m_Vars.m_fParentDistanceTravelled; // Isnt parent distance travelled already factored in to the cost?

					if(pPathRequest->m_bPreferDownHill)
					{
						// subtract the difference in the minimum heights of each poly
						// if we are moving to a lower poly, we subtract a pos number so cost is reduced
						//fCost += MINMAX_FIXEDPT_TO_FLOAT(pPoly->m_MinMax.m_iMinZ - pAdjPoly->m_MinMax.m_iMinZ) * ms_fPreferDownhillScaleFactor; 

						const float fUphillCost = (vClosestPoint.z - m_Vars.m_vLastClosestPoint.z) * ms_fPreferDownhillScaleFactor;
						fCost += MAX(fUphillCost, 0.0f);
					}
				}
				else
				{
					// Cost becomes parent cost plus cost of moving onto this polygon
	//				fCost = fParentCost + (fAdjPolyCost * CPathFindMovementCosts::ms_fDefaultCostFromTargetMultiplier);
					// Add the cost of movement scaled by distance moved
	//				fCost += m_Vars.m_fDistanceFromLastPointToChosenPoint * fPenaltyForAdjPoly;

					fCost =
						(m_Vars.m_fParentDistanceTravelled * CPathFindMovementCosts::ms_fDefaultDistanceTravelledMultiplier) +
							(m_Vars.m_fDistanceFromLastPointToChosenPoint * fPenaltyForAdjPoly) +
								(fAdjPolyCost * CPathFindMovementCosts::ms_fDefaultCostFromTargetMultiplier);

					if(pPathRequest->m_bPreferDownHill)
					{
						// subtract the difference in the minimum heights of each poly
						// if we are moving to a lower poly, we subtract a pos number so cost is reduced
						//fCost += MINMAX_FIXEDPT_TO_FLOAT(pPoly->m_MinMax.m_iMinZ - pAdjPoly->m_MinMax.m_iMinZ) * ms_fPreferDownhillScaleFactor; 

						const float fUphillCost = (vClosestPoint.z - m_Vars.m_vLastClosestPoint.z) * ms_fPreferDownhillScaleFactor;
						fCost += MAX(fUphillCost, 0.0f);
					}
				}

				//*********************************************************************************
				// Add a fixed penalty if we reached the ending poly from a direction which doesn't
				// give us a clear dynamic-object LOS.  This is a nasty case.  Hopefully this will
				// encourage the search to continue and find a more acceptable route to this poly
				// before this poly becomes closed & the search is terminated.

				/*
				if(pAdjPoly == m_Vars.m_pEndPoly && CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance &&
					!TestDynamicObjectLOS(vClosestPoint, pPathRequest->m_vPathEnd, CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
				{
					static const float fFixedPenaltyForNoLosWithinFinalPoly = 100.0f;
					fCost += fFixedPenaltyForNoLosWithinFinalPoly;
				}
				*/

				fAdjacencyTypePenalty = GetAdjacencyTypePenalty(pPathRequest, *pAdjacency, pPoly, pAdjPoly, m_Vars.m_vLastClosestPoint);
				fCost += fAdjacencyTypePenalty;

				u32 iOrFlags = 0;
				u32 iAndFlags = 0;

				// Apply penalty to keep path on a pavement ?
				if(pPathRequest->m_bPreferPavements || pPathRequest->m_bNeverLeavePavements)
				{
					if(!pAdjPoly->GetIsPavement())
					{
						// NB : This _must_ be multiplied by the distance traveled inside this polygon.
						// Otherwise the algorithm favors simple paths which touch the least number of
						// polygons, to avoid the non-pavement penalty
						fCost += (m_Vars.m_fSurfaceNotPavementPenalty * m_Vars.m_fDistanceFromLastPointToChosenPoint);

						if(pPoly->GetIsPavement())
						{
							static dev_float fLeavePavementFixedCost = 50.0f;
							fCost += fLeavePavementFixedCost;
						}
					}
				}

				// Apply a penalty to keep path off train tracks?
				if(pPathRequest->m_bAvoidTrainTracks)
				{
					if(pAdjPoly->GetIsTrainTrack())
					{
						fCost += (pPathRequest->m_MovementCosts.m_fAvoidTrainTracksPenalty * m_Vars.m_fDistanceFromLastPointToChosenPoint);

						if(!pPoly->GetIsTrainTrack())
						{
							static dev_float fMoveOntoTrainTracksFixedPenalty = 20000.0f;
							fCost += fMoveOntoTrainTracksFixedPenalty;
						}
					}
				}

				// Store the adjacency type, or update if this was already visited
				iAndFlags |= ~NAVMESHPOLY_REACHEDBY_MASK;

				// If this poly's point intersects any of the optional "influence spheres" in this path request, then
				// modify the cost value accordingly - proportional to how close to the sphere's center the point it.

				iAndFlags |= ~NAVMESHPOLY_IS_WITHIN_INFLUENCE_SPHERE;

				if(pPathRequest->m_iNumInfluenceSpheres)
				{
					float fTotalSphereInfluence = 0.0f;

					fwGeom::fwLine line(m_Vars.m_vLastClosestPoint, vClosestPoint);
					Vector3 vClosest;

					s32 nInfluenceSpheres = pPathRequest->m_iNumInfluenceSpheres;
					for(s32 s=0; s<nInfluenceSpheres; s++)
					{
						Vector3 vSphereOrigin = pPathRequest->m_InfluenceSpheres[s].GetOrigin();
						if (line.FindClosestPointOnLine(vSphereOrigin, vClosest) > 0.001f )	// True only if we are moving towards the sphere!
						{
							float fSphereDistSqr = (vClosest - vSphereOrigin).Mag2();
							if(fSphereDistSqr < pPathRequest->m_InfluenceSpheres[s].GetRadiusSqr())
							{
								iOrFlags |= NAVMESHPOLY_IS_WITHIN_INFLUENCE_SPHERE;

								Assert(fSphereDistSqr > 0.0f);

								float fSphereDist = (1.0f / invsqrtf_fast(fSphereDistSqr));
								float fSphereCost = 1.0f - (fSphereDist / pPathRequest->m_InfluenceSpheres[s].GetRadius());
								fSphereCost *= pPathRequest->m_InfluenceSpheres[s].GetInnerWeighting() - pPathRequest->m_InfluenceSpheres[s].GetOuterWeighting();
								fSphereCost += pPathRequest->m_InfluenceSpheres[s].GetOuterWeighting();

								fTotalSphereInfluence += fSphereCost;
							}
						}
					}

					// Add the accumulated influence onto the cost
					fCost += fTotalSphereInfluence;
				}

				if(pPathRequest->m_bFleeTarget)
				{
					// Possibly change this, we want to make sure that the last polygon is the best one also, not just the first within range
					m_Vars.m_fDistToTargetSqr = (vClosestPoint - pPathRequest->m_vPathStart).Mag2();
				}


				//******************************************************************************************
				// If we are factoring in the per-poly directional cover into the pathsearch, do that here.

				if(pPathRequest->m_bUseDirectionalCover)
				{
					// Get a bitfield describing the direction of the cover we wish to stay within.
					// TODO: INLINE this once it is proven..
					const u32 iCoverBits = GetCoverDirBitfield(vClosestPoint, pPathRequest->m_vCoverOrigin);

					// If a bitwise AND is non-zero then the poly contains this cover direction
					if((iCoverBits & pAdjPoly->GetCoverDirections())!=0)
					{

					}
					else
					{
						// Impose a penalty for not being in cover
						fCost += pPathRequest->m_MovementCosts.m_fPenaltyForNoDirectionalCover;
					}
				}

				//*******************************************************************************
				// If we are favouring enclosed spaces then factor this into the heuristic here.
				// Penalize moving onto polygons with a lesser amount of cover-directions.
				// Scale the penalty with the distance from the flee-from-position, so help avoid
				// the enclosed-spaces heuristic from outweighing the flee-from heuristic.

				if(pPathRequest->m_bFavourEnclosedSpaces)
				{
					const u32 iNumCoverBits = g_iCountCoverBitsTable[pAdjPoly->GetCoverDirections()];
					const u32 iPrevNumCoverBits = g_iCountCoverBitsTable[pPoly->GetCoverDirections()];
					const s32 iCoverBitsDelta = iPrevNumCoverBits - iNumCoverBits;

					if(iCoverBitsDelta > 0)
					{
						if(pPathRequest->m_bFleeTarget)
						{
							float fDistScale = (1.0f / invsqrtf_fast(m_Vars.m_fDistToTargetSqr)) * m_Vars.m_fInvRefDist;
							fDistScale = Clamp(fDistScale, 0.0f, 1.0f);
							fDistScale *= fDistScale;
							Assert(fDistScale >= 0.0f && fDistScale <= 1.0f);

							fCost += ( ((float)iCoverBitsDelta) * (pPathRequest->m_MovementCosts.m_fPenaltyForFavourCoverPerUnsetBit * fDistScale));
						}
						else
						{
							fCost += ( ((float)iCoverBitsDelta) * pPathRequest->m_MovementCosts.m_fPenaltyForFavourCoverPerUnsetBit);
						}
					}
				}

				// If this poly is already in the 'open' list, then compare it's cost value in the list
				// with our newly calculated cost value.  If the new value is less, then we remove the
				// entry & reinsert into the list in a new position.
				CPathServerBinHeap::Node * pBinHeapNode = NULL;

				if(pAdjPoly->TestFlags(NAVMESHPOLY_OPEN))
				{
					pBinHeapNode = m_PathSearchPriorityQueue->GetBinHeap()->FindNode(pAdjPoly);
					if(pBinHeapNode && pBinHeapNode->Key <= fCost)
						continue;

					pAdjPoly->AndFlags(~NAVMESHPOLY_REACHEDBY_MASK);
				}
				else
				{
					// Mark poly as 'open' & add to list of visited polys (so that we can clear the flags later..)
					pAdjPoly->OrFlags(NAVMESHPOLY_OPEN);
				}

				// Can now set the point enum in the poly
				Assert(iPointEnum >= 0 && iPointEnum < 128);
				pAdjPoly->SetPointEnum(iPointEnum);

				// We can also now set state flags
				pAdjPoly->AndFlags(iAndFlags);
				pAdjPoly->OrFlags(iOrFlags);

				// Check whether the distance from this point-in-poly to the path end point is within
				// a specified completion radius.  If so, then if we cannot find an exact path to the
				// target then we can at least get within this distance.
				if(bUseBestAlternativeEndPoly || bUseCompletionRadius)
				{
#if defined(GTA_ENGINE) && AI_OPTIMISATIONS_OFF
					Assert((!pPathRequest->m_bWander) && (!pPathRequest->m_bFleeTarget));
#endif
					// New version of this may help paths to complete more frequently.
					// However it may cause paths to be found, when there is in fact no route there..
					if(bUseBestAlternativeEndPoly || completionRadiusMinMax.Intersects(pAdjPoly->m_MinMax))
					{
						fDistSqrFromTarget = (vClosestPoint - pPathRequest->m_vPathEnd).Mag2();
						if(fDistSqrFromTarget < fClosestDistSqrFromTarget)
						{
							pBestAlternativeEndPoly = pAdjPoly;
							fClosestDistSqrFromTarget = fDistSqrFromTarget;
						}
					}
				}

				// Store the parent poly which we arrived at this poly from, so that we can trace back our route later
				pAdjPoly->m_PathParentPoly = pPoly;
				pAdjPoly->m_Struct4.m_iParentExitEdge = c;

#if NAVMESH_OPTIMISATIONS_OFF
				Assert(pAdjPoly->m_Struct4.m_iParentExitEdge != 0xf);
#endif

				if( pAdjacency->GetAdjacencyType() != ADJACENCY_TYPE_NORMAL )
				{
					Assert(CPathServer::GetNavMeshFromIndex(pAdjPoly->GetNavMeshIndex(), domain));
					pAdjPoly->OrFlags(NAVMESHPOLY_WAS_REACHED_VIA_NONSTANDARD_ADJACENCY);
				}

#ifdef NAVGEN_TOOL
				pAdjPoly->m_fPathCost = fCost;
				pAdjPoly->m_fDistanceTravelled = m_Vars.m_fDistanceFromLastPointToChosenPoint + m_Vars.m_fParentDistanceTravelled;
#endif
				Assert(!pPoly->GetIsDegenerateConnectionPoly());

				// If we have a pointer to a node (because this poly was already open) - then
				// we can just decrease it's value more efficiently that re-inserting it.
				if(pBinHeapNode && fCost < pBinHeapNode->Key)
				{
					pBinHeapNode->vPointInPoly[0] = vClosestPoint.x;
					pBinHeapNode->vPointInPoly[1] = vClosestPoint.y;
					pBinHeapNode->vPointInPoly[2] = vClosestPoint.z;
					pBinHeapNode->vDirFromPrevious[0] = m_Vars.m_vVecFromLastPointToCurrentPoint.x;
					pBinHeapNode->vDirFromPrevious[1] = m_Vars.m_vVecFromLastPointToCurrentPoint.y;
					pBinHeapNode->vDirFromPrevious[2] = m_Vars.m_vVecFromLastPointToCurrentPoint.z;
					pBinHeapNode->fDistanceTravelled = m_Vars.m_fDistanceFromLastPointToChosenPoint + m_Vars.m_fParentDistanceTravelled;
					pBinHeapNode->iReachedByConnectionPoly = pPoly->GetIsDegenerateConnectionPoly() ? (u16)pNavMesh->GetPolyIndex(pPoly): (u16)NAVMESH_POLY_INDEX_NONE;
					m_PathSearchPriorityQueue->GetBinHeap()->DecreaseKey(pBinHeapNode, fCost);
				}
				// Otherwise we'll need to add a new entry from scratch
				else
				{
					// Now add an entry in the pathfinding stack
					if(!m_PathSearchPriorityQueue->Insert(
						fCost,
						m_Vars.m_fDistanceFromLastPointToChosenPoint + m_Vars.m_fParentDistanceTravelled,
						pAdjPoly,
						vClosestPoint,
						m_Vars.m_vVecFromLastPointToCurrentPoint,
						0,
						pPoly->GetIsDegenerateConnectionPoly() ? pNavMesh->GetPolyIndex(pPoly): NAVMESH_POLY_INDEX_NONE) )
					{
						// We've run out of space in "m_PathStackEntryStore" for the path-search..
						pPathRequest->m_bRequestActive = false;
						pPathRequest->m_bComplete = true;
						pPathRequest->m_iNumPoints = 0;
						pPathRequest->m_iCompletionCode = PATH_RAN_OUT_OF_PATH_STACK_SPACE;

						return EPathNotFound;
					}

				}

			}	// Repeat for all adjacent polys we obtained

		}	// Repeat for all connection (or real) polygons

		//********************************************************************************************
		// Visit any special links which may lead from this polygon.  This includes things such
		// ladders, elevators etc.  TODO: maybe put climbs & jumps in here as well as we have
		// more control than via using the TAdjPoly poly adjacency info in each edge.

		if(pPoly->GetNumSpecialLinks() != 0)
		{
			const u32 iPolyIndex = pNavMesh->GetPolyIndex(pPoly);

			CNavMesh * pOriginalFromNavMesh;
			TNavMeshPoly * pOriginalFromPoly;

			// If this is the tessellation navmesh then find the original navmesh & poly the fragment came from.
			if(pNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION)
			{
				int iPolyIndexInTessMesh = pNavMesh->GetPolyIndex(pPoly);
				TTessInfo & pTessInfo = CPathServer::m_PolysTessellatedFrom[iPolyIndexInTessMesh];
				pOriginalFromNavMesh = CPathServer::GetNavMeshFromIndex(pTessInfo.m_iNavMeshIndex, domain);
				pOriginalFromPoly = pOriginalFromNavMesh->GetPoly(pTessInfo.m_iPolyIndex);
			}
			else
			{
				pOriginalFromNavMesh = pNavMesh;
				pOriginalFromPoly = pPoly;
			}

			Assert((pOriginalFromNavMesh->GetFlags() & NAVMESH_HAS_SPECIAL_LINKS_SECTION )&&(pOriginalFromNavMesh->GetNumSpecialLinks()));

			CNavMesh * pLinkToNavMesh;
			TNavMeshPoly * pLinkToPoly;
			Vector3 vLinkFromPos, vLinkToPos;
#if !__FINAL
			TAdjPoly unusedAdjPolyParam;
#endif
			// Arbitrary value just to get started with
			// static const float fCostFromTargetMultiplier = 1.0f;	// was 4.0f	// was 2.0f

			fAdjPolyCost = 0.0f;

			s32 iLinkLookupIndex = pPoly->GetSpecialLinksStartIndex();

			for(s32 s=0; s<pPoly->GetNumSpecialLinks(); s++)
			{
				u16 iLinkIndex = pOriginalFromNavMesh->GetSpecialLinkIndex(iLinkLookupIndex++);
				Assert(iLinkIndex < pOriginalFromNavMesh->GetNumSpecialLinks());
				CSpecialLinkInfo & linkInfo = pOriginalFromNavMesh->GetSpecialLinks()[iLinkIndex];

				if(linkInfo.GetIsDisabled())
					continue;

				if(linkInfo.GetAStarTimeStamp() != m_iAStarTimeStamp)
				{
					linkInfo.Reset();
					linkInfo.SetAStarTimeStamp(m_iAStarTimeStamp);
				}

				// Exclude ladders?
				if( (pPathRequest->m_bNeverUseLadders) && (linkInfo.GetLinkType()==NAVMESH_LINKTYPE_CLIMB_LADDER || linkInfo.GetLinkType()==NAVMESH_LINKTYPE_DESCEND_LADDER))
					continue;
				// Exclude climbs ?
				if( pPathRequest->m_bNeverClimbOverStuff && linkInfo.GetLinkType()==NAVMESH_LINKTYPE_CLIMB_OBJECT)
					continue;

				// Does link start on this polygon ?
				if(linkInfo.GetLinkFromPoly() == iPolyIndex)
				{
					pLinkToNavMesh = CPathServer::GetNavMeshFromIndex(linkInfo.GetLinkToNavMesh(), domain);
					if(!pLinkToNavMesh)
						continue;

					pLinkToPoly = pLinkToNavMesh->GetPoly(linkInfo.GetLinkToPoly());

					// Avoid a useless cyclical traversal
					if(pLinkToPoly == pPoly->GetPathParentPoly())
						continue;
					// Disabled ?
					if(pLinkToPoly->GetIsDisabled())
						continue;

					// Temporary toggle to prevent use of new climbing links until thoroughly tested
					if( CPathServer::ms_bDisallowClimbObjectLinks && linkInfo.GetLinkType()==NAVMESH_LINKTYPE_CLIMB_OBJECT )
						continue;

					// Have to reset polys flags if this is first time we're visiting it
					if(!OnFirstVisitingPoly(pLinkToPoly))
					{
						pPathRequest->m_iCompletionCode = PATH_RAN_OUT_VISITED_POLYS_SPACE;
						return EPathNotFound;
					}

					// Already closed to the search ?
					if(pLinkToPoly->GetIsClosed())
						continue;

					// NB: This shouldn't happen, because when tessellating a polygon we always
					// retarget any special links which start/end on that polygon
					if(pLinkToPoly->GetReplacedByTessellation())
					{
						continue;
					}

					if(pPathRequest->m_bNeverEnterWater && pLinkToPoly->GetIsWater() && (!pPoly->GetIsWater()))
						continue;

					if(pPathRequest->m_bNeverLeaveWater && (!pLinkToPoly->GetIsWater()) && pPoly->GetIsWater())
						continue;

					CNavMesh * pOriginalToNavMesh;
					if(pLinkToNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION)
					{
						int iPolyIndexInTessMesh = pLinkToNavMesh->GetPolyIndex(pLinkToPoly);
						TTessInfo & pTessInfo = CPathServer::m_PolysTessellatedFrom[iPolyIndexInTessMesh];
						pOriginalToNavMesh = CPathServer::GetNavMeshFromIndex(pTessInfo.m_iNavMeshIndex, domain);
					}
					else
					{
						pOriginalToNavMesh = pLinkToNavMesh;
					}

					// Get the link positions
					CNavMesh::DecompressVertex(vLinkFromPos, linkInfo.GetLinkFromPosX(), linkInfo.GetLinkFromPosY(), linkInfo.GetLinkFromPosZ(), pOriginalFromNavMesh->GetQuadTree()->m_Mins, pOriginalFromNavMesh->GetExtents());
					CNavMesh::DecompressVertex(vLinkToPos, linkInfo.GetLinkToPosX(), linkInfo.GetLinkToPosY(), linkInfo.GetLinkToPosZ(), pOriginalToNavMesh->GetQuadTree()->m_Mins, pOriginalToNavMesh->GetExtents());

#ifdef GTA_ENGINE
					// Exclude this ladder if it was used recently?
					if(linkInfo.GetLinkType()==NAVMESH_LINKTYPE_CLIMB_LADDER || linkInfo.GetLinkType()==NAVMESH_LINKTYPE_DESCEND_LADDER)
					{
						if (!pPathRequest->m_bScriptedRoute && !pPathRequest->m_bMissionPed && !pPathRequest->m_bCoverFinderPath)
						{
							if( m_BlockedLadders.IsLadderBlocked( vLinkFromPos.z < vLinkToPos.z ? vLinkFromPos : vLinkToPos) )
								continue;
						}
					}
#endif
					bool bAddPolyToStack = false;
					fPenaltyForAdjPoly = CPathFindMovementCosts::ms_fDefaultDistanceTravelledMultiplier;
					fAdjacencyTypePenalty = 0.0f;

					switch(linkInfo.GetLinkType())
					{
						case NAVMESH_LINKTYPE_CLIMB_LADDER:
						{
							fAdjacencyTypePenalty = pPathRequest->m_MovementCosts.m_fClimbLadderPenalty;
							fPenaltyForAdjPoly = pPathRequest->m_MovementCosts.m_fClimbLadderPenaltyPerMetre;
							bAddPolyToStack = true;
							break;
						}
						case NAVMESH_LINKTYPE_DESCEND_LADDER:
						{
							fAdjacencyTypePenalty = pPathRequest->m_MovementCosts.m_fClimbLadderPenalty;
							fPenaltyForAdjPoly = pPathRequest->m_MovementCosts.m_fClimbLadderPenaltyPerMetre;
							bAddPolyToStack = true;
							break;
						}
						case NAVMESH_LINKTYPE_CLIMB_OBJECT:
						{
							fAdjacencyTypePenalty = pPathRequest->m_MovementCosts.m_fClimbLowPenalty;
							fPenaltyForAdjPoly = pPathRequest->m_MovementCosts.m_fClimbObjectPenaltyPerMetre;
							bAddPolyToStack = true;
							break;
						}
					}

					fAdjPolyCost = 0.0f;

					if(pPathRequest->m_bWander)
					{
						// Not really sure what to do for wandering; unlikely to happen
						fAdjPolyCost = 1.0f;
					}
					else if(pPathRequest->m_bFleeTarget)
					{
						const Vector3 vFromTarget = pPathRequest->m_vPathEnd - vLinkToPos;
						const float fFromTargetSqr = vFromTarget.Mag2();
						if(fFromTargetSqr > 0.0f)
							fAdjPolyCost = 1.0f / (invsqrtf_fast(fFromTargetSqr));
					}
					else
					{
						const Vector3 vFromTarget = pPathRequest->m_vPathEnd - vLinkToPos;
						const float fFromTargetSqr = vFromTarget.Mag2();
						if(fFromTargetSqr > 0.0f)
							fAdjPolyCost = 1.0f / (invsqrtf_fast(fFromTargetSqr));
					}

					Vector3 vPointInPolyOffset;

					// NB : Need to stop point enum being used for this poly?  Or maybe find the closest point in the poly to the target pos?
					s32 iPointEnum = -1;
					bool bIsLadderClimb = false;
					switch(linkInfo.GetLinkType())
					{
						// For climbs, we wish to choose a point in the target polygon which is in front
						// of a plane placed at the link to position; this helps ensure that when the pathfinding
						// resumes on this polygon, it will not be immediately inside an object (the object we just climbed).
						case NAVMESH_LINKTYPE_CLIMB_OBJECT:
						{
							Vector3 vLinkVec = vLinkToPos - vLinkFromPos;
							vLinkVec.z = 0.0f;
							vLinkVec.Normalize();

							//vPointInPolyOffset = vLinkVec * 0.01f;
							vPointInPolyOffset = vLinkVec * (pPathRequest->m_fEntityRadius + 0.1f);

							/*
							const float fLinkPlaneD = -DotProduct(vLinkToPos, vLinkVec);

							// TODO : set other variables in m_VisitPolyVars ??
							m_VisitPolyVars.m_pFromNavMesh = pNavMesh;
							m_VisitPolyVars.m_pFromPoly = pPoly;
							m_VisitPolyVars.m_pToPoly = pAdjPoly;
							m_VisitPolyVars.m_pToNavMesh = pAdjNavMesh;		
							m_VisitPolyVars.m_vPointInFromPoly = m_Vars.m_vLastClosestPoint;
							m_VisitPolyVars.m_vTargetPos = pPathRequest->m_vPathEnd;
							m_VisitPolyVars.m_iAdjacencyIndex = c;
							iPointEnum = GetClosestPointInFrontOfPlane( m_VisitPolyVars, pLinkToPoly, pLinkToNavMesh, vLinkToPos, vLinkVec, fLinkPlaneD );

							// If we could not find a point beyond the plane, we don't add this poly
							if(iPointEnum == -1)
								bAddPolyToStack = false;
							*/

							

							iPointEnum = NAVMESHPOLY_POINTENUM_SPECIAL_LINK_ENDPOS;
							break;
						}
						case NAVMESH_LINKTYPE_CLIMB_LADDER:
						case NAVMESH_LINKTYPE_DESCEND_LADDER:
						default:
						{
							vPointInPolyOffset.Zero();
							iPointEnum = NAVMESHPOLY_POINTENUM_CENTROID;
							bIsLadderClimb = true;
							break;
						}
					}


					if(bAddPolyToStack)
					{
						// Same basic heuristic as for normal pathsearch (not wander or flee)
						m_Vars.m_vVecFromLastPointToCurrentPoint = vLinkToPos - m_Vars.m_vLastClosestPoint;
						m_Vars.m_fDistanceFromLastPointToChosenPoint = m_Vars.m_vVecFromLastPointToCurrentPoint.Mag();

						float fCost = 0.f;

						if (pPathRequest->m_bFleeTarget)
						{
							if (bIsLadderClimb)
							{
								Vector3 vClimbDir = vLinkToPos - m_Vars.m_vLastClosestPoint;
								vClimbDir.z = 0.f;
								vClimbDir.Normalize();

								Vector3 vFleeDir = pPathRequest->m_vPathEnd - m_Vars.m_vLastClosestPoint;
								vFleeDir.z = 0.f;
								vFleeDir.Normalize();

								if (vClimbDir.Dot(vFleeDir) > 0.f)	// Not sure what kind of dot value we should go with really
									fPenaltyForAdjPoly += 150.f;		// We don't want to climb "towards" our flee target and be exposed
							}

							fCost = fParentCost;
							fCost += m_Vars.m_fDistanceFromLastPointToChosenPoint * fPenaltyForAdjPoly + fAdjPolyCost + fAdjacencyTypePenalty;
						}
						else
						{
							fCost = (m_Vars.m_fParentDistanceTravelled * CPathFindMovementCosts::ms_fDefaultDistanceTravelledMultiplier) +
									(m_Vars.m_fDistanceFromLastPointToChosenPoint * fPenaltyForAdjPoly) +
										(fAdjPolyCost * CPathFindMovementCosts::ms_fDefaultCostFromTargetMultiplier) +
											fAdjacencyTypePenalty;
						}

						// If this poly is already open, then only replace it if the new cost is less
						if(pLinkToPoly->TestFlags(NAVMESHPOLY_OPEN))
						{
							CPathServerBinHeap::Node * pBinHeapNode = m_PathSearchPriorityQueue->GetBinHeap()->FindNode(pLinkToPoly);
							if(pBinHeapNode && pBinHeapNode->Key <= fCost)
								continue;
						}
						else
						{
							// Mark poly as 'open' & add to list of visible polys (so that we can clear the flags later..)
							pLinkToPoly->OrFlags(NAVMESHPOLY_OPEN);
						}

						// Point enum was decided earlier
						pLinkToPoly->SetPointEnum(iPointEnum);

						// Mark poly as 'open' & add to list of visible polys (so that we can clear the flags later..)
						pLinkToPoly->OrFlags(NAVMESHPOLY_OPEN);

						// Store the parent poly which we arrived at this poly from, so that we can trace back our route later
						pLinkToPoly->m_PathParentPoly = pPoly;	// Note that we use pPoly again here.. We only use the original untessellated poly for the link info (if req'd)
						pLinkToPoly->m_Struct4.m_iParentExitEdge = 0xF;

						// Store the adjacency type, or update if this was already visited
						pLinkToPoly->AndFlags(~NAVMESHPOLY_REACHEDBY_MASK);

						pLinkToPoly->OrFlags(NAVMESHPOLY_WAS_REACHED_VIA_SPECIAL_LINK);

#ifdef NAVGEN_TOOL
						pLinkToPoly->m_fPathCost = fCost;
#endif
						// Add the link-to poly to the navmesh.  Use the link-to-position as the last point, because
						// this is at the other end of the link.
						if(!m_PathSearchPriorityQueue->Insert(
							fCost,
							m_Vars.m_fDistanceFromLastPointToChosenPoint + m_Vars.m_fParentDistanceTravelled,
							pLinkToPoly,
							vLinkToPos + vPointInPolyOffset,
							m_Vars.m_vVecFromLastPointToCurrentPoint,
							0,
							NAVMESH_POLY_INDEX_NONE))
						{
							// We've run out of space in "m_PathStackEntryStore" for the path-search..
							pPathRequest->m_bRequestActive = false;
							pPathRequest->m_bComplete = true;
							pPathRequest->m_iNumPoints = 0;
							pPathRequest->m_iCompletionCode = PATH_RAN_OUT_OF_PATH_STACK_SPACE;

							return EPathNotFound;
						}
					}
				}
			}
		}

		//******************************************
		//
		//	Final stuff before repeating the loop
		//

		// No longer open
		pPoly->AndFlags(~NAVMESHPOLY_OPEN);

		if(!bDontCloseThisPoly)
		{
			pPoly->OrFlags(NAVMESHPOLY_CLOSED);
		}

	}	// Repeat until stack is empty
#if !__FINAL
	pPathRequest->m_iNumVisitedPolygons = m_Vars.m_iNumVisitedPolys;
#endif

	// Did we find a decent ending poly for a wander or flee path ?
	if((pPathRequest->m_bWander || pPathRequest->m_bFleeTarget) && pBestEndPolySoFar)
	{
		pPoly = pBestEndPolySoFar;
		m_Vars.m_pEndPoly = pBestEndPolySoFar;
	}
	// Else, if not wandering - did we not get to target, but have a best alternative poly ? Why not wandering anyway?
	else if(!pPathRequest->m_bWander && (pPoly != m_Vars.m_pEndPoly) && pBestAlternativeEndPoly)
	{
		pPoly = pBestAlternativeEndPoly;
		m_Vars.m_pEndPoly = pBestAlternativeEndPoly;
		pPathRequest->m_PathResultInfo.m_bUsedCompletionRadius = true;
	}


	// If there is a crash, we may be able to save out the current path-req to debug - drag the PC here.
#if __DEV
	static bool bOutputCurrentPathReqProb=false;
	if(bOutputCurrentPathReqProb)
	{
		for(int iReq=0; iReq<MAX_NUM_PATH_REQUESTS; iReq++)
		{
			if(CPathServer::m_PathRequests[iReq].m_hHandle == pPathRequest->m_hHandle)
			{
				CPathServer::m_iSelectedPathRequest = iReq;
				CPathServer::OutputPathRequestProblem(CPathServer::m_iSelectedPathRequest);
				break;
			}
		}
	}
#endif

	// Was a path found?`
	m_Vars.m_iNumPathPolys = 0;

	Assert(pPoly);

#if NAVMESH_OPTIMISATIONS_OFF
	Assert(!pPoly->GetReplacedByTessellation());
#endif

	// url:bugstar:965355
	// As yet not entirely sure how this happened
	// Seems rare, so for safety lets treat this is a path failure
	if(pPoly->GetReplacedByTessellation())	
	{
		// No path was found, so set as complete but with no points..
		pPathRequest->m_iCompletionCode = PATH_NOT_FOUND;
		pPathRequest->m_iNumPoints = 0;
		pPathRequest->m_bComplete = true;

		return EPathNotFound;
	}

	if(pPoly && pPoly == m_Vars.m_pEndPoly)
	{
		TNavMeshPoly * pPolyPtr = pPoly;
		TNavMeshPoly * pFirstPoly = pPoly;

		while(m_Vars.m_iNumPathPolys < MAX_PATH_POLYS)
		{
			Assert(!pPolyPtr->GetReplacedByTessellation());

			while(pPolyPtr && pPolyPtr->GetIsDegenerateConnectionPoly())
			{
				pPolyPtr = pPolyPtr->m_PathParentPoly;
			}
			Assert(pPolyPtr);
			if(!pPolyPtr)
				break;

			Assert(!pPolyPtr->GetIsDisabled());

			// Poly
			m_Vars.m_PathPolys[m_Vars.m_iNumPathPolys] = pPolyPtr;
			m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].Clear();

			if(pPolyPtr->GetIsWater())
			{
				pPathRequest->m_PathResultInfo.m_bPathIncludesWater = true;
			}

			//*******************************************************************************
			// If this poly was reached in a non-standard way (jump, drop, climb, etc) then
			// examine the edge linking from the parent poly & see which type it was.  We
			// then set a special waypoint flag to indicate this.
			// (NB: This method is an alternative to having a set of flags for each poly -
			//  we now only have the 'NAVMESHPOLY_REACHED_BY_NONSTANDARD_ADJACENCY' flag).

			if(pPolyPtr->TestFlags(NAVMESHPOLY_WAS_REACHED_VIA_NONSTANDARD_ADJACENCY) && pPolyPtr->m_PathParentPoly)
			{
				pNavMesh = CPathServer::GetNavMeshFromIndex(pPolyPtr->GetNavMeshIndex(), domain);
				u32 iPolyIndex = pNavMesh->GetPolyIndex(pPolyPtr);

				Assert(pPolyPtr->m_PathParentPoly);

				CNavMesh * pParentNavMesh = CPathServer::GetNavMeshFromIndex(pPolyPtr->m_PathParentPoly->GetNavMeshIndex(), domain);
				TNavMeshPoly * pParentPoly = pPolyPtr->m_PathParentPoly;

				// If the parent polygon was a tessellated fragment, then look to the original untesselated polygon
				// to locate the TAdjPoly which contains the adjacency info.

				// NB: We need to do the same for the pPolyPtr polygon, since this might have been tessellated - and the nonstandard
				// adjacency in the parent polygon will still point to the original polygon/navmesh

				if(pParentPoly->GetIsTessellatedFragment())
				{
					Assert(pParentNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION);

					TTessInfo * pTessInfo = CPathServer::GetTessInfo(pParentPoly);
					Assert(pTessInfo);

					pParentNavMesh = CPathServer::GetNavMeshFromIndex(pTessInfo->m_iNavMeshIndex, pPathRequest->GetMeshDataSet());
					pParentPoly = pParentNavMesh->GetPoly(pTessInfo->m_iPolyIndex);
				}

				for(v=0; v<pParentPoly->GetNumVertices(); v++)
				{
					const TAdjPoly & adjPoly = pParentNavMesh->GetAdjacentPoly(pParentPoly->GetFirstVertexIndex() + v);

					if(adjPoly.GetNavMeshIndex(pParentNavMesh->GetAdjacentMeshes()) == pPolyPtr->GetNavMeshIndex() && adjPoly.GetPolyIndex() == iPolyIndex)
					{
						// Waypoint flags
						switch(adjPoly.GetAdjacencyType())
						{
							case ADJACENCY_TYPE_CLIMB_LOW:
							{
								m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = WAYPOINT_FLAG_CLIMB_LOW;
								break;
							}
							case ADJACENCY_TYPE_CLIMB_HIGH:
							{
								m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = WAYPOINT_FLAG_CLIMB_HIGH;
								break;
							}
							case ADJACENCY_TYPE_DROPDOWN:
							{
								if(pPolyPtr->m_PathParentPoly->GetIsWater() && !pPolyPtr->GetIsWater())	// upgrade low "dwropdown" adjacencies to a climb if leaving water?
								{
									m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = WAYPOINT_FLAG_CLIMB_LOW;
								}
								else
								{
									m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = WAYPOINT_FLAG_DROPDOWN;
								}
								break;
							}
							default:
							{
								// ERROR!
								// We cannot locate this adjacency
								// This will mean that path will miss this waypoint action, and may therefore fail to climb when needed.
								m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].Clear();
								pPolyPtr->AndFlags(~NAVMESHPOLY_WAS_REACHED_VIA_NONSTANDARD_ADJACENCY);
								break;
							}
						}
						break;
					}
				}
			}

			//*******************************************************************************
			// Poly was reached by a special link in the navmesh (eg. ladders)

			else if(pPolyPtr->TestFlags(NAVMESHPOLY_WAS_REACHED_VIA_SPECIAL_LINK) && pPolyPtr->m_PathParentPoly)
			{
				pNavMesh = CPathServer::GetNavMeshFromIndex(pPolyPtr->GetNavMeshIndex(), domain);

				u32 iToNavMesh = pPolyPtr->GetNavMeshIndex();
				u32 iToPoly = pNavMesh->GetPolyIndex(pPolyPtr);

				CNavMesh * pParentNavMesh = CPathServer::GetNavMeshFromIndex(pPolyPtr->m_PathParentPoly->GetNavMeshIndex(), domain);
				
				u32 iFromNavMesh = pPolyPtr->m_PathParentPoly->GetNavMeshIndex();
				u32 iFromPoly = pParentNavMesh->GetPolyIndex(pPolyPtr->m_PathParentPoly);

				CNavMesh * pOriginalParentNavMesh;
				if(pParentNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION)
				{
					TTessInfo * pTessInfo = fwPathServer::GetTessInfo(pPolyPtr->m_PathParentPoly);
					pOriginalParentNavMesh = CPathServer::GetNavMeshFromIndex(pTessInfo->m_iNavMeshIndex, domain);

					//iFromNavMesh = pTessInfo->m_iNavMeshIndex;
					//iFromPoly = pTessInfo->m_iPolyIndex;
				}
				else
				{
					pOriginalParentNavMesh = pParentNavMesh;
				}
				
				CSpecialLinkInfo * pLink = NULL;

				// Look through all the special links in the original parent navmesh, to locate the one which connected from the
				// parent polygon to the child polygon (this assumes that only one special link connects the two).
				for(s32 l=0; l<pOriginalParentNavMesh->GetNumSpecialLinks(); l++)
				{
					CSpecialLinkInfo & link = pOriginalParentNavMesh->GetSpecialLinks()[l];
					if(link.GetLinkFromNavMesh() == iFromNavMesh &&
						link.GetLinkToNavMesh() == iToNavMesh &&
						 link.GetLinkFromPoly() == iFromPoly &&
						  link.GetLinkToPoly() == iToPoly)
					{
						pLink = &link;
						break;
					}
				}

				// Set waypoint flags accordingly..
#if NAVMESH_OPTIMISATIONS_OFF
				Assert(pLink && !pLink->GetIsDisabled());
#endif
				if(pLink)
				{
					switch(pLink->GetLinkType())
					{
						case NAVMESH_LINKTYPE_CLIMB_LADDER:
						case NAVMESH_LINKTYPE_DESCEND_LADDER:
							m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags =
								(pPolyPtr->m_MinMax.GetMidZAsFloat() > pPolyPtr->m_PathParentPoly->m_MinMax.GetMidZAsFloat()) ?
									(u16)WAYPOINT_FLAG_CLIMB_LADDER : (u16)WAYPOINT_FLAG_DESCEND_LADDER;
							break;
						case NAVMESH_LINKTYPE_CLIMB_OBJECT:
							m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = (u16) WAYPOINT_FLAG_CLIMB_OBJECT;
							break;
						default:
							m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = (u16) WAYPOINT_FLAG_NOTHING_TO_DO;
					}
				}
				else
				{
					m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = WAYPOINT_FLAG_NOTHING_TO_DO;
				}
			}

			//************************************************************************************
			// Was poly reached by coming out of the water, but not via a climb?
			// Check to see whether height difference requires a climb or not.
			// TODO: Replace this by changing the navmsh compiler to insert climbs as appropriate
/*
			else if(pPolyPtr->m_PathParentPoly && pPolyPtr->m_PathParentPoly->GetIsWater() && !pPolyPtr->GetIsWater())
			{
				Assert(pPolyPtr->m_PathParentPoly);

				// Safeguard against a rare memory overwrite bug, ensure that this poly pointer belongs to a
				// navmesh which is actually loaded.  This may not be the cause of the crash, but is added just in case.
				CNavMesh * pThisPolyNavMesh = CPathServer::GetNavMeshFromIndex(pPolyPtr->GetNavMeshIndex(), domain);
				Assert(pThisPolyNavMesh);
				if(pThisPolyNavMesh)
				{
					pPolyPtr->OrFlags(NAVMESHPOLY_WAS_REACHED_VIA_NONSTANDARD_ADJACENCY);
					const u32 iWayPtFlag = WAYPOINT_FLAG_CLIMB_LOW;
					m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = (u16) iWayPtFlag;
				}
				else
				{
					m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = WAYPOINT_FLAG_NOTHING_TO_DO;
				}
			}
*/
			//*******************************************************************************
			// Else, poly was reached by a standard poly-edge connection

			else
			{
				m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags = WAYPOINT_FLAG_NOTHING_TO_DO;
			}

			// Set the interior flag
			if(pPolyPtr->GetIsInterior())
			{
				m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags |= WAYPOINT_FLAG_IS_INTERIOR;
			}
			// Set the in-water flag
			if(pPolyPtr->GetIsWater())
			{
				m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].m_iSpecialActionFlags |= WAYPOINT_FLAG_IS_ON_WATER_SURFACE;
			}


#if __STORE_POLYS_IN_PATH_REQUEST
			pPathRequest->m_InitiallyFoundPathPolys[m_Vars.m_iNumPathPolys] = pPolyPtr;
#endif

			m_Vars.m_iNumPathPolys++;

			if(!pPolyPtr->m_PathParentPoly)
				break;

			pPolyPtr = pPolyPtr->m_PathParentPoly;

			// Stop this path from looping infinitely.  This will happen if a path started & ended on the same
			// poly, but there was no direct LOS due to a dynamic object.
			if(pPolyPtr == pFirstPoly)
			{
				break;
			}
		}

		if(m_Vars.m_iNumPathPolys >= MAX_PATH_POLYS)
		{
			// No path was found, so set as complete but with no points..
			pPathRequest->m_iCompletionCode = PATH_REACHED_MAX_PATH_POLYS;
			pPathRequest->m_iNumPoints = 0;
			pPathRequest->m_bComplete = true;
			return EPathNotFound;
		}

		// If the final poly we reached during the traceback (ie. the first poly in the path) is marked as
		// an *alternative* starting poly, then add the actual starting poly itself as a final step.
		if(pPolyPtr && pPolyPtr->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY))
		{
			Assert(pPolyPtr != m_Vars.m_pStartPoly);			// Actual start poly should never have this flag set
			Assert(m_Vars.m_iNumPathPolys < MAX_PATH_POLYS+2);	// The m_Vars.m_iNumPathPolys array is purposefully enlarged to handle this case

			m_Vars.m_PathPolys[m_Vars.m_iNumPathPolys] = m_Vars.m_pStartPoly;
			m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys].Clear();

#if __STORE_POLYS_IN_PATH_REQUEST
			pPathRequest->m_InitiallyFoundPathPolys[m_Vars.m_iNumPathPolys] = m_Vars.m_pStartPoly;
#endif

			if(m_Vars.m_pStartPoly->GetIsWater())
				pPathRequest->m_PathResultInfo.m_bPathIncludesWater = true;

			m_Vars.m_iNumPathPolys++;
		}

		// Now reverse the path, since it currently goes from end to start..
		// NB : we could just find the path in the reverse direction to start with ?..
		int iHalfSize = m_Vars.m_iNumPathPolys/2;
		TNavMeshWaypointFlag iWptFlag;
		for(int i=0; i<iHalfSize; i++)
		{
			// Swap polys
			pPolyPtr = m_Vars.m_PathPolys[i];
			m_Vars.m_PathPolys[i] = m_Vars.m_PathPolys[m_Vars.m_iNumPathPolys-i-1];
			m_Vars.m_PathPolys[m_Vars.m_iNumPathPolys-i-1] = pPolyPtr;

			// Swap waypoint flags
			iWptFlag = m_Vars.m_iPolyWaypointFlags[i];
			m_Vars.m_iPolyWaypointFlags[i] = m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys-i-1];
			m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys-i-1] = iWptFlag;
		}

		return EPathFound;
	}

	// No path was found, so set as complete but with no points..
	pPathRequest->m_iCompletionCode = PATH_NOT_FOUND;
	pPathRequest->m_iNumPoints = 0;
	pPathRequest->m_bComplete = true;

	return EPathNotFound;
}


bool
CPathServerThread::IsThisAdjacencyTypeOk(CPathRequest * pPathRequest, const TAdjPoly & adjPoly)
{
	const u32 iAdjType = adjPoly.GetAdjacencyType();

	if(iAdjType != ADJACENCY_TYPE_NORMAL)
	{
		// Can't visit any polys if the non-standard adjacency over this edge has been disabled.
		// This can happen when a navmesh route AI task finds that a ped cannot climb this edge.
		if(adjPoly.GetAdjacencyDisabled())
		{
			return false;
		}
		// If there is a potentially fatal drop over this edge, then we may not consider it
		if(adjPoly.GetHighDropOverEdge() && !pPathRequest->m_bMayUseFatalDrops)
		{
			return false;
		}
		// If this adjacency has a drop down associated with it, and we are set never to drop from heights, then continue
		if((iAdjType==ADJACENCY_TYPE_DROPDOWN || adjPoly.GetHighDropOverEdge()) && pPathRequest->m_bNeverDropFromHeight)
		{
			return false;
		}
		// If path request specifies to never use non-standard adjacency, then continue
		if((iAdjType==ADJACENCY_TYPE_CLIMB_LOW || iAdjType==ADJACENCY_TYPE_CLIMB_HIGH) && pPathRequest->m_bNeverClimbOverStuff)
		{
			return false;
		}
	}

	return true;
}


float
CPathServerThread::GetAdjacencyTypePenalty(CPathRequest * pPathRequest, const TAdjPoly & adjPoly, const TNavMeshPoly * pPoly, const TNavMeshPoly * pAdjPoly, const Vector3 & vPositionInFromPoly)
{
	float fPenalty;

	switch(adjPoly.GetAdjacencyType())
	{
	case ADJACENCY_TYPE_CLIMB_HIGH:
		fPenalty = pPathRequest->m_MovementCosts.m_fClimbHighPenalty;
		break;
	case ADJACENCY_TYPE_CLIMB_LOW:
		fPenalty = pPathRequest->m_MovementCosts.m_fClimbLowPenalty;
		break;
	case ADJACENCY_TYPE_DROPDOWN:
		fPenalty = pPathRequest->m_MovementCosts.m_fDropDownPenalty;
		break;
	default:
		fPenalty = 0.0f;
		break;
	}

	//********************************************************************************
	// Penalize the use of non-standard adjacency connections when in/under water
	// and moving onto another in/under water polygon.  The reason for this is that
	// we would like instead to encourage the use of moving between seabed & surface
	// to get over obstacles, as this uses the TVolumePolyInfo array and gives a much
	// approximation of volumetric movement (the ADJACENCY_TYPE_DROPDOWN adjacencies
	// are only analyzed between adjacent water polys in order to help with special
	// underwater navigation problems where we may need to connect off underwater
	// obstacles in caverns below the water surface.  :-)

	if(pPoly->GetIsWater() && pAdjPoly->GetIsWater())
	{
		//static const float fWaterNonStandardLinkPenalty = 10.0f;
		//fPenalty *= fWaterNonStandardLinkPenalty;
	}

	//***************************************************************************
	// Penalize large falls.  Obviously not relevant when travelling underwater.

	else
	{
		// Factor in a penalty value for how far the fall is.  If this is marked as a high drop (possibly fatal) then penalize heavily.
		const int iFixedPtHeightDiff = pPoly->m_MinMax.m_iMinZ - pAdjPoly->m_MinMax.m_iMinZ;
		if(iFixedPtHeightDiff > 0)
		{
			const float fApproxDropDist = MINMAX_FIXEDPT_TO_FLOAT(pPoly->m_MinMax.m_iMinZ - pAdjPoly->m_MinMax.m_iMinZ);
			const float fFatalDropMultiplier = 10.0f;
			float fDropCost = (fApproxDropDist * pPathRequest->m_MovementCosts.m_fDropDownPenaltyPerMetre);
			if(adjPoly.GetHighDropOverEdge()) fDropCost *= fFatalDropMultiplier;
				fPenalty += fDropCost;		
		}
	}

	// If we are fleeing our target, then penalize climbs which are close to the flee-from position.
	// The idea being that a ped would be unlikely to expose themselves to danger whilst in proximity
	// to the threat, and more likely to attain some safe distance prior to attempting a climb.
	// Fixes url:bugstar:1231520
	if(pPathRequest->m_bFleeTarget)
	{
		if(adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_CLIMB_HIGH || adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_CLIMB_LOW)
		{
			static dev_float fMinSafeDistSqr = 25.0f * 25.0f;
			static dev_float fFixedPenaltyForThreatClimb = 300.0f * CPathServerThread::ms_fClimbMultiplier_Flee;

			Vector3 vFromThreat = vPositionInFromPoly - pPathRequest->m_vPathEnd;
			const float fFromThreatSqr = vFromThreat.Mag2();
			if(fFromThreatSqr < fMinSafeDistSqr)
			{
				fPenalty += fFixedPenaltyForThreatClimb;
			}
		}
	}

	return fPenalty;
}


bool CPathServerThread::ShouldUseMorePointsForPoly(const TNavMeshPoly & poly, CPathRequest * pPathRequest) const
{
	if( pPathRequest->m_bScriptedRoute || pPathRequest->m_bMissionPed 
		BANK_ONLY( || CPathServer::ms_bAlwaysUseMorePointsInPolys )
		)
	{
		return true;
	}
	else
	{
		return poly.TestFlags(NAVMESHPOLY_LARGE);
	}
}

CPathServerThread::eExploreFuncRetVal
CPathServerThread::VisitPoly_ShortestPath(
	const TNavMeshPoly * UNUSED_PARAM(pPrevPoly), const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, 
	Vector3 & vClosestPoint, float & fCost, s32 & iPointEnum, const bool bCouldBeTessellatedFurther)
{
	CPathServerThread & pathServerThread = CPathServer::m_PathServerThread;
	CPathRequest * pPathRequest = pathServerThread.m_pCurrentActivePathRequest;

	m_VisitPolyVars.m_pToPolyPoints = pathServerThread.m_Vars.m_vPolyPts;

	eExploreFuncRetVal retVal;
	bool bLosClear;

	// If object avoidance is switched off or we have no object intersection on the previous &
	// current polygons, then we can consider this polygon without any object LOS tests.
	if( (CPathServer::m_eObjectAvoidanceMode == CPathServer::ENoObjectAvoidance || CPathServer::ms_bNoNeedToCheckObjectsForThisPoly) && !pAdjPoly->GetIsTessellatedFragment())
	{
		CalcAdjPolyPointsNow(pPathRequest, pAdjNavMesh, pAdjPoly);

		if(pPathRequest->m_bUseVariableEntityRadius)
		{
			const u32 iFlags = TVisitPolyStruct::FLAG_CONSIDER_MAXIMUM_NUM_VERTICES;
			fCost = pathServerThread.GetClosestPointInPolyToTargetDistSqrWithRadius(
				m_VisitPolyVars,
				iFlags,
				pPathRequest->m_fEntityRadius,
				&vClosestPoint, iPointEnum );
			
			if(iPointEnum == -1)
				return eCannotVisitThisPoly;
		}
		else
		{
			s32 iPointFlags = CPathServer::ms_bUseOptimisedPolyCentroids ? TVisitPolyStruct::FLAG_CONSIDER_ONLY_CENTROID_FOR_FRAGMENTS : 0;

			fCost = pathServerThread.GetClosestPointInPolyToTargetDistSqr(
				m_VisitPolyVars,
				iPointFlags,
				&vClosestPoint,
				iPointEnum );
		}

		if(fCost > 0.0f)
			fCost = (1.0f / invsqrtf_fast(fCost));

		retVal = eOkayAddToBinHeap;
		bLosClear = true;
	}

	// Otherwise we will have to do LOS tests from the previous point we found, to the point
	// we are considering moving towards in the current poly
	else
	{
		CalcAdjPolyPointsNow(pPathRequest, pAdjNavMesh, pAdjPoly);

		// If the path starts & ends on the same polygon, and the LOS between the two points is blocked,
		// then find a starting point as close as possible to the path start.
		// TODO : Change m_pStartPoly to check for NAVMESHPOLY_STARTING_POLY flag test?

		if(pAdjPoly == pathServerThread.m_Vars.m_pStartPoly && pAdjPoly == pathServerThread.m_Vars.m_pEndPoly &&
			!pathServerThread.TestDynamicObjectLOS(pPathRequest->m_vPathStart, pPathRequest->m_vPathEnd))
		{
			fCost = pathServerThread.GetClosestPointInPolyToTargetDistSqr( m_VisitPolyVars, 0, &vClosestPoint, iPointEnum );

			// The fCost value is now the distance to the target
			if(fCost > 0.0f)
				fCost = (1.0f / invsqrtf_fast(fCost));

			retVal = eOkayAddToBinHeap;
			bLosClear = true;
		}
		else
		{
			bLosClear = pathServerThread.GetClosestPointInPolyToTargetWithLos(
				m_VisitPolyVars,
				bCouldBeTessellatedFurther ? TVisitPolyStruct::FLAG_ABORT_IF_HIT_ANY_OBJECT : 0,
				&vClosestPoint, iPointEnum, fCost );

//			fCost = (vClosestPoint - pPathRequest->m_vPathEnd).Mag2();
//			if(fCost > 0.0f)
//				fCost = (1.0f / invsqrtf_fast(fCost));

			retVal = bLosClear ? eOkayAddToBinHeap : eMustTessellateThisPoly;
		}
	}

	// Calculate how far we've travelled from last waypoint to this waypoint.
	pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint = vClosestPoint - pathServerThread.m_Vars.m_vLastClosestPoint;
	float fMag2 = pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint.Mag2();
	if(fMag2 > 0.0f)
		pathServerThread.m_Vars.m_fDistanceFromLastPointToChosenPoint = (1.0f / invsqrtf_fast(fMag2));
	else
		pathServerThread.m_Vars.m_fDistanceFromLastPointToChosenPoint = 0.0f;

	return retVal;
}

CPathServerThread::eExploreFuncRetVal
CPathServerThread::VisitPoly_Wander(
	const TNavMeshPoly * pPrevPoly, const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh, 
	Vector3 & vClosestPoint, float & fCost, s32 & iPointEnum, const bool bCouldBeTessellatedFurther)
{
	// If this is a wander path-request, and this poly is in an area switched off for ped-wandering, then continue
	if(!pPrevPoly->TestFlags(NAVMESHPOLY_SWITCHED_OFF_FOR_AMBIENT_PEDS) &&
		pAdjPoly->TestFlags(NAVMESHPOLY_SWITCHED_OFF_FOR_AMBIENT_PEDS))
	{
		return eCannotVisitThisPoly;
	}

	CPathServerThread & pathServerThread = CPathServer::m_PathServerThread;
	CPathRequest * pPathRequest = pathServerThread.m_pCurrentActivePathRequest;

	m_VisitPolyVars.m_pToPolyPoints = pathServerThread.m_Vars.m_vPolyPts;
//	m_VisitPolyVars.m_pToAdjacentPolys = &(const_cast<CNavMesh*>(pAdjNavMesh))->GetAdjacentPolyPool()[pAdjPoly->GetFirstVertexIndex()];

	// Update the wander plane normal & dist with the direction from the previous point.
	static const bool bUpdateWanderPlaneDuringSearch=false;
	if(bUpdateWanderPlaneDuringSearch)
	{
		pathServerThread.m_Vars.m_vWanderOrigin = pathServerThread.m_Vars.m_vLastClosestPoint;
		pathServerThread.m_Vars.m_vWanderPlaneNormal = pathServerThread.m_Vars.m_vDirFromPrevious;

		pathServerThread.m_Vars.m_fWanderPlaneDist = -Dot(pathServerThread.m_Vars.m_vWanderPlaneNormal, pathServerThread.m_Vars.m_vWanderOrigin);
		pathServerThread.m_Vars.m_fInitialWanderPlaneDist = pathServerThread.m_Vars.m_fWanderPlaneDist;

		pathServerThread.m_Vars.m_vWanderPlaneRightNormal.x = pathServerThread.m_Vars.m_vWanderPlaneNormal.y;
		pathServerThread.m_Vars.m_vWanderPlaneRightNormal.y = - pathServerThread.m_Vars.m_vWanderPlaneNormal.x;
		pathServerThread.m_Vars.m_vWanderPlaneRightNormal.z = 0.0f;
		pathServerThread.m_Vars.m_fWanderPlaneRightDist = -Dot(pathServerThread.m_Vars.m_vWanderPlaneRightNormal, pathServerThread.m_Vars.m_vWanderOrigin);
	}

	bool bLosClear;
	bool bDoObjLOS =
		CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance &&
		(!CPathServer::ms_bNoNeedToCheckObjectsForThisPoly);

	if(CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance)
	{
		CalcAdjPolyPointsNow(pPathRequest, pAdjNavMesh, pAdjPoly);

		u32 iFlags = 0;
		// We are trying to avoid objects, so do object LOS tests
		if(bDoObjLOS) iFlags |= TVisitPolyStruct::FLAG_PERFORM_DYNAMIC_OBJECT_LOS;
		// If we can tessellate this poly an more, then abort if we hit any objects so we will do poly tessellation
		if(bCouldBeTessellatedFurther) iFlags |= TVisitPolyStruct::FLAG_ABORT_IF_HIT_ANY_OBJECT;
		// We assume that we want to continue in the same direction, so we extend this check further
		if(bDoObjLOS && bCouldBeTessellatedFurther) iFlags |= TVisitPolyStruct::FLAG_PERFORM_DYNAMIC_OBJECT_LOS_FURTHER;

		bLosClear = pathServerThread.GetPointInPolyWithBestAngleFromStartWithLos(
			m_VisitPolyVars,
			iFlags,
			&vClosestPoint,
			&fCost,
			iPointEnum);		
	}
	else
	{
		CalcAdjPolyPointsNow(pPathRequest, pAdjNavMesh, pAdjPoly);

		// NB : We may want to allow this 1st point to intersect a dynamic object..
		pathServerThread.GetPointInPolyWithBestAngleFromStartWithLos(
			m_VisitPolyVars,
			bDoObjLOS ? TVisitPolyStruct::FLAG_PERFORM_DYNAMIC_OBJECT_LOS : 0,
			&vClosestPoint,
			&fCost,
			iPointEnum);

		bLosClear = true;
	}

	if(iPointEnum != -1)
	{
		pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint = vClosestPoint - pathServerThread.m_Vars.m_vLastClosestPoint;

		// New code.  Unoptimized.  Calculates how far we've traveled from last waypoint to this waypoint.
		pathServerThread.m_Vars.m_fDistanceFromLastPointToChosenPoint = pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint.Mag();

		pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint.z = 0.0f;
		float fMag2 = pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint.Mag2();
		if(fMag2 > 0.0f)
			pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint *= invsqrtf_fast(fMag2);
	}
	else
	{
		pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint = Vector3(0.0f,0.0f,0.0f);
		pathServerThread.m_Vars.m_fDistanceFromLastPointToChosenPoint = 0.0f;
	}

	if(bLosClear)
	{
		return eOkayAddToBinHeap;
	}
	else
	{
		return eMustTessellateThisPoly;
	}
}

CPathServerThread::eExploreFuncRetVal
CPathServerThread::VisitPoly_Flee(
  const TNavMeshPoly * UNUSED_PARAM(pPrevPoly), const TNavMeshPoly * pAdjPoly, const CNavMesh * pAdjNavMesh,
  Vector3 & vClosestPoint,
  float & fCost,
  s32 & iPointEnum,
  const bool bCouldBeTessellatedFurther)
{
	CPathServerThread & pathServerThread = CPathServer::m_PathServerThread;
	CPathRequest * pPathRequest = pathServerThread.m_pCurrentActivePathRequest;

	m_VisitPolyVars.m_pToPolyPoints = pathServerThread.m_Vars.m_vPolyPts;
//	m_VisitPolyVars.m_pToAdjacentPolys = &(const_cast<CNavMesh*>(pAdjNavMesh))->GetAdjacentPolyPool()[pAdjPoly->GetFirstVertexIndex()];

	bool bDoObjLOS = (CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance)
		&& !CPathServer::ms_bNoNeedToCheckObjectsForThisPoly;

	eExploreFuncRetVal retVal;

	if(bDoObjLOS)
	{
		// If the path starts & ends on the same polygon, and the LOS between the two points is blocked,
		// then find a starting point as close as possible to the path start.
		// TODO : Change m_pStartPoly to check for NAVMESHPOLY_STARTING_POLY flag test?
		if(pAdjPoly == pathServerThread.m_Vars.m_pStartPoly && pAdjPoly == pathServerThread.m_Vars.m_pEndPoly &&
			!pathServerThread.TestDynamicObjectLOS(pPathRequest->m_vPathStart, pPathRequest->m_vPathEnd))
		{
			CalcAdjPolyPointsNow(pPathRequest, pAdjNavMesh, pAdjPoly);

			fCost = pathServerThread.GetClosestPointInPolyFromLastPointDistSqr(
				m_VisitPolyVars,
				0,
				&vClosestPoint,
				iPointEnum,
				pathServerThread.m_Vars.m_vLastClosestPoint);

			// The fCost value is now the distance to the target
			if(fCost > 0.0f)
				fCost = (1.0f / invsqrtf_fast(fCost));

			retVal = eOkayAddToBinHeap;
		}
		else
		{
			CalcAdjPolyPointsNow(pPathRequest, pAdjNavMesh, pAdjPoly);

			bool bLosClear = pathServerThread.GetClosestPointInPolyFromLastPointWithLos(
				m_VisitPolyVars,
				bCouldBeTessellatedFurther ? TVisitPolyStruct::FLAG_ABORT_IF_HIT_ANY_OBJECT : 0,
				&vClosestPoint,
				iPointEnum,
				pathServerThread.m_Vars.m_vLastClosestPoint);

			fCost = (vClosestPoint - pPathRequest->m_vPathEnd).Mag2();
			if(fCost > 0.0f)
				fCost = (1.0f / invsqrtf_fast(fCost));

			retVal = bLosClear ? eOkayAddToBinHeap : eMustTessellateThisPoly;
		}
	}
	else
	{
		CalcAdjPolyPointsNow(pPathRequest, pAdjNavMesh, pAdjPoly);

		fCost = pathServerThread.GetClosestPointInPolyFromLastPointDistSqr(
			m_VisitPolyVars,
			CPathServer::ms_bUseOptimisedPolyCentroids ? TVisitPolyStruct::FLAG_CONSIDER_ONLY_VERTICES_AND_EDGES : 0,
			&vClosestPoint,
			iPointEnum,
			pathServerThread.m_Vars.m_vLastClosestPoint);

		if(fCost > 0.0f)
			fCost = (1.0f / invsqrtf_fast(fCost));

		retVal = eOkayAddToBinHeap;
	}

	//************************************************************************************
	// Make sure that the line from the prev point to the current point, doesn't take us
	// any closer to the flee-from pos than we already are (penalize heavily if so)

	{
		Vector3 ForwardDir = pathServerThread.m_Vars.m_vLastClosestPoint - pPathRequest->m_vPathEnd;
		ForwardDir.z = 0.f;
		ForwardDir.Normalize();

	//	Vector3 RightDir = Vector3(ForwardDir.y, -ForwardDir.x, 0.f);
			
			// Penalize for when the point is not in our preferred direction
		Vector3 VecToNext = vClosestPoint - pathServerThread.m_Vars.m_vLastClosestPoint;
		VecToNext.z = 0.f;
		const float fDistFromFleePos = VecToNext.Mag();
		fCost = fDistFromFleePos;//rage::Max(pPathRequest->m_fReferenceDistance - fCost, 0.0f); // We accumulate the cost instead
		if(fDistFromFleePos > SMALL_FLOAT)
		{
			const float fSofterScale = pPathRequest->m_bSofterFleeHeuristics ? 0.5f : 1.f;
			const float fDoubleBackCost = fDistFromFleePos * (pPathRequest->m_bSofterFleeHeuristics ? CPathFindMovementCosts::ms_fFleePathDoubleBackCostSofter : CPathFindMovementCosts::ms_fFleePathDoubleBackCost);

			static bank_float fFleeDirectionWeight = 45.f * fSofterScale;
			static bank_float fFleeThreatRadius = 3.f * fSofterScale;

			VecToNext.NormalizeFast();

			// This is awesome and simple so...
			// You will only be penalized when fleeing close to a ped against the original direction from the target
			// or when you are fleeing towards the target
			// This is to prevent doubleback and passing the flee target but it will not make the ped look stupid
			// when it is trying to climb or taking obscure routes to avoid the target.
			if (fFleeThreatRadius > fCost)	// fCost represent the distance to the target
			{
				Vector3 vRefVec = pPathRequest->m_vPathStart - pPathRequest->m_vPathEnd;	// Should maybe use the cached refvec and modify in the flee task
			//	vRefVec.Normalize();
				if (Dot(VecToNext, vRefVec) < 0.f)
				{
					fCost += fDoubleBackCost * 1.2f;	// This extra multiplier just felt good, wasn't necessary in any of my findings
				}
			}
			else
			{
				const float fAbsPlaneAngle = AcosfSafe(Dot(VecToNext, ForwardDir)) * 0.3183099f;// 1.0f / M_PI;
				fCost += fDistFromFleePos * fAbsPlaneAngle * fFleeDirectionWeight;

				if(Dot(VecToNext, ForwardDir) < -0.4f)	// Double back
				{
					//fDoubleBackCost = Min(fDoubleBackCost, pPathRequest->m_fReferenceDistance * CPathFindMovementCosts::ms_fFleePathDoubleBackMultiplier);
					//fDoubleBackCost = Max(fDoubleBackCost, pPathRequest->m_fReferenceDistance * CPathFindMovementCosts::ms_fFleePathDoubleBackMultiplier);
					fCost += fDoubleBackCost;
				}
			}
		}
	}

	// Calculate how far we've traveled from last waypoint to this waypoint.
	pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint = vClosestPoint - pathServerThread.m_Vars.m_vLastClosestPoint;
	float fMag2 = pathServerThread.m_Vars.m_vVecFromLastPointToCurrentPoint.Mag2();
	if(fMag2 > 0.0f)
		pathServerThread.m_Vars.m_fDistanceFromLastPointToChosenPoint = (1.0f / invsqrtf_fast(fMag2));
	else
		pathServerThread.m_Vars.m_fDistanceFromLastPointToChosenPoint = 0.0f;

	return retVal;
}




//******************************************************
//	This function measures a number of distances squared
//	from the poly's edges & vertices (with a slight
//	displacement to help reduce errors) - and returns
//	the least distance & optionally, the point as well.
//******************************************************

float CPathServerThread::GetClosestPointInPolyToTargetDistSqr( TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vClosestPoint, s32 & iPointEnum )
{
	EPointsInPolyType ePointInPolyType;

	if( iFlags & TVisitPolyStruct::FLAG_CONSIDER_ONLY_VERTICES_AND_EDGES )
	{
		ePointInPolyType = POINTSINPOLY_VERTICESANDCENTROID;
	}
	else if( (iFlags & TVisitPolyStruct::FLAG_CONSIDER_ONLY_CENTROID_FOR_FRAGMENTS) && (vars.m_pToPoly->TestFlags(NAVMESHPOLY_TESSELLATED_FRAGMENT)))
	{
		ePointInPolyType = POINTSINPOLY_CENTROID_ONLY;
	}
	else
	{
		ePointInPolyType = ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL;
	}

	const int iNumClosePts = CreatePointsInPoly( vars.m_pToNavMesh, vars.m_pToPoly, vars.m_pToPolyPoints, ePointInPolyType, m_Vars.g_vClosestPtsInPoly );

	int iBestDistToPrevIndex = -1;
	float fLeastDistToPrevSqr = g_fVeryLargeValue;
	float fLeastDistToTargetSqr = g_fVeryLargeValue;

	Vector3 vDiff;
	float fDistSqr;

	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vars.m_vPointInFromPoly - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < fLeastDistToPrevSqr)
		{
			fLeastDistToPrevSqr = fDistSqr;
			iBestDistToPrevIndex = t;
		}

		vDiff = vars.m_vTargetPos - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < fLeastDistToTargetSqr)
		{
			fLeastDistToTargetSqr = fDistSqr;
		}
	}

	// Return the point
	if(vClosestPoint)
	{
		*vClosestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistToPrevIndex];
	}

	iPointEnum = iBestDistToPrevIndex;

	if(ePointInPolyType == POINTSINPOLY_VERTICESANDCENTROID)
		iPointEnum += NAVMESHPOLY_POINTENUM_VERTSANDEDGES_FIRST;

	return fLeastDistToTargetSqr;
}

float CPathServerThread::GetClosestPointInPolyToTargetDistSqrWithRadius( TVisitPolyStruct & vars, const u32 iFlags, const float fRadius, Vector3 * vClosestPoint, s32 & iPointEnum )
{
	EPointsInPolyType ePointInPolyType;

	if( iFlags & TVisitPolyStruct::FLAG_CONSIDER_ONLY_VERTICES_AND_EDGES )
	{
		ePointInPolyType = POINTSINPOLY_VERTICESANDCENTROID;
	}
	else if( (iFlags & TVisitPolyStruct::FLAG_CONSIDER_ONLY_CENTROID_FOR_FRAGMENTS) && (vars.m_pToPoly->TestFlags(NAVMESHPOLY_TESSELLATED_FRAGMENT)))
	{
		ePointInPolyType = POINTSINPOLY_CENTROID_ONLY;
	}
	else if( iFlags & TVisitPolyStruct::FLAG_CONSIDER_MAXIMUM_NUM_VERTICES)
	{
		ePointInPolyType = POINTSINPOLY_EXTRA;
	}
	else
	{
		ePointInPolyType = ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL;
	}

	const int iNumClosePts = CreatePointsInPoly( vars.m_pToNavMesh, vars.m_pToPoly, vars.m_pToPolyPoints, ePointInPolyType, m_Vars.g_vClosestPtsInPoly );

	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
	vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
	vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
	fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
	fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();


	Vector3 vDiff;
	float fDistSqr;

	int iBestDistToPrevIndex = -1;
	float fLeastDistToPrevSqr = g_fVeryLargeValue;
	int iBestDistToTargetIndex = -1;
	float fLeastDistToTargetSqr = g_fVeryLargeValue;

	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vars.m_vPointInFromPoly - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < fLeastDistToPrevSqr)
		{
			if(PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], fRadius, vExitVerts, &fFreeSpace[0]))
			{
				fLeastDistToPrevSqr = fDistSqr;
				iBestDistToPrevIndex = t;
			}
		}

		vDiff = vars.m_vTargetPos - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < fLeastDistToTargetSqr)
		{
			fLeastDistToTargetSqr = fDistSqr;
			iBestDistToTargetIndex = t;
		}
	}

	if(iBestDistToPrevIndex == -1)
	{
		return fLeastDistToTargetSqr;
	}

	// Return the point
	if(vClosestPoint)
	{
		*vClosestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistToPrevIndex];
	}

	iPointEnum = iBestDistToPrevIndex;

	if(ePointInPolyType == POINTSINPOLY_VERTICESANDCENTROID)
		iPointEnum += NAVMESHPOLY_POINTENUM_VERTSANDEDGES_FIRST;

	return fLeastDistToTargetSqr;
}

float CPathServerThread::GetFurthestPointInPolyFromTargetDistSqr( TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum )
{
	EPointsInPolyType ePointInPolyType;

	if( iFlags & TVisitPolyStruct::FLAG_CONSIDER_ONLY_VERTICES_AND_EDGES )
	{
		ePointInPolyType = POINTSINPOLY_VERTICESANDCENTROID;
	}
	else if( (iFlags & TVisitPolyStruct::FLAG_CONSIDER_ONLY_CENTROID_FOR_FRAGMENTS) && (vars.m_pToPoly->TestFlags(NAVMESHPOLY_TESSELLATED_FRAGMENT)))
	{
		ePointInPolyType = POINTSINPOLY_CENTROID_ONLY;
	}
	else
	{
		ePointInPolyType = ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL;
	}

	const int iNumClosePts = CreatePointsInPoly( vars.m_pToNavMesh, vars.m_pToPoly, vars.m_pToPolyPoints, ePointInPolyType, m_Vars.g_vClosestPtsInPoly );

	const bool bAdjacencyTest = m_pCurrentActivePathRequest->m_bUseVariableEntityRadius;

	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	if(bAdjacencyTest)
	{
		const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
		fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
		fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();
	}

	Vector3 vDiff;
	int iBestDistIndex = -1;
	float fDistSqr;

	float fGreatestDistSqr = 0.0f;

	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vars.m_vTargetPos - m_Vars.g_vClosestPtsInPoly[t];
		//vDiff.z = 0.f;
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < g_fVeryLargeValue && fDistSqr > fGreatestDistSqr)
		{
			if(!bAdjacencyTest || PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], m_pCurrentActivePathRequest->m_fEntityRadius, vExitVerts, &fFreeSpace[0]))
			{
				fGreatestDistSqr = fDistSqr;
				iBestDistIndex = t;
			}
		}
	}

	fDistSqr = fGreatestDistSqr;

	// Return the point
	if(vFurthestPoint)
	{
		*vFurthestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistIndex];
	}

	iPointEnum = iBestDistIndex;

	return fDistSqr;
}






bool CPathServerThread::GetClosestPointInPolyToTargetWithLos( TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vClosestPoint, s32 & iPointEnum, float & fCost )
{
	const int iNumClosePts = CreatePointsInPoly(
		vars.m_pToNavMesh,
		vars.m_pToPoly,
		vars.m_pToPolyPoints,
		ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL,
		m_Vars.g_vClosestPtsInPoly );

	const bool bAdjacencyTest = m_pCurrentActivePathRequest->m_fEntityRadius > PATHSERVER_PED_RADIUS;
	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	if(bAdjacencyTest)
	{
		const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
		fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
		fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();
	}

	static const float fPenaltyForOpeningDoor = 50.0f;
	const bool bAbortIfHitAnyObject = (iFlags & TVisitPolyStruct::FLAG_ABORT_IF_HIT_ANY_OBJECT) != 0;

	int iBestDistIndexToPrev = -1;
	float fLeastDistToPrevSqr = g_fVeryLargeValue;

	int iBestDistIndexToTarget = -1;
	float fLeastDistToTargetSqr = g_fVeryLargeValue;

	Vector3 vDiff;
	float fDistSqr;


	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vars.m_vPointInFromPoly - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < fLeastDistToPrevSqr)
		{
			if(!bAdjacencyTest || PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], m_pCurrentActivePathRequest->m_fEntityRadius, vExitVerts, &fFreeSpace[0]))
			{
				// Is the vector from the vTestFromPos to this point clear of dynamic object ?
				if(TestDynamicObjectLOS(vars.m_vPointInFromPoly, m_Vars.g_vClosestPtsInPoly[t], CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
				{
					if(m_iDynamicObjLos_NumOpenableObjectsHit > 0 && m_iDynamicObjLos_NumOpenableObjectsHit==m_iDynamicObjLos_TotalNumObjectsHit)
					{
						fDistSqr += fPenaltyForOpeningDoor;
					}

					fLeastDistToPrevSqr = fDistSqr;
					iBestDistIndexToPrev = t;
				}
				else
				{
					m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = true;
					if(bAbortIfHitAnyObject)
						return false;
				}
			}
		}

		vDiff = vars.m_vTargetPos - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < fLeastDistToTargetSqr)
		{
			iBestDistIndexToTarget = t;
			fLeastDistToTargetSqr = fDistSqr;
		}
	}

	// Return the point
	if(iBestDistIndexToPrev != -1 && iBestDistIndexToTarget != -1)
	{
		iPointEnum = iBestDistIndexToPrev;

		Assert(iPointEnum <= MAX_POINT_ENUM_VALUE);

		if(vClosestPoint)
		{
			*vClosestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistIndexToPrev];
		}

		Assert(iBestDistIndexToTarget != -1);
		fCost = (m_Vars.g_vClosestPtsInPoly[iBestDistIndexToTarget] - vars.m_vTargetPos).Mag2();
		if(fCost > 0.0f)
			fCost = (1.0f / invsqrtf_fast(fCost));

		return true;
	}

	return false;
}


bool CPathServerThread::GetClosestPointInPolyToTargetWithLosToTarget( TVisitPolyStruct & vars, const u32 UNUSED_PARAM(iFlags), Vector3 * vClosestPoint, s32 & iPointEnum, float & fCost )
{
	const int iNumClosePts = CreatePointsInPoly(
		vars.m_pToNavMesh,
		vars.m_pToPoly,
		vars.m_pToPolyPoints,
		ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL,
		m_Vars.g_vClosestPtsInPoly );

	const bool bAdjacencyTest = m_pCurrentActivePathRequest->m_bUseVariableEntityRadius;
	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	if(bAdjacencyTest)
	{
		const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
		fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
		fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();
	}

	Vector3 vDiff;
	float fDistSqr;

	bool bHitOpenableObject1 = false;
	bool bHitOpenableObject2 = false;
	static const float fPenaltyForOpeningDoor = 50.0f;

	float fLeastDistToPrevSqr = g_fVeryLargeValue;
	int iBestDistToPrevIndex = -1;
	float fLeastDistToTargetSqr = g_fVeryLargeValue;
	int iBestDistToTargetIndex = -1;

	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vars.m_vPointInFromPoly - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < fLeastDistToPrevSqr)
		{
			if(!bAdjacencyTest || PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], m_pCurrentActivePathRequest->m_fEntityRadius, vExitVerts, &fFreeSpace[0]))
			{
				// Is the vector from the vTestFromPos to this point clear of dynamic object ?
				if(TestDynamicObjectLOS(vars.m_vPointInFromPoly, m_Vars.g_vClosestPtsInPoly[t], CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
				{
					bHitOpenableObject1 = m_iDynamicObjLos_NumOpenableObjectsHit > 0;

					// Is the vector from the vTarget to this point clear of dynamic object ?
					if(TestDynamicObjectLOS(vars.m_vTargetPos, m_Vars.g_vClosestPtsInPoly[t], CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
					{
						bHitOpenableObject2 = m_iDynamicObjLos_NumOpenableObjectsHit > 0;

						if(bHitOpenableObject1 || bHitOpenableObject2)
							fDistSqr += fPenaltyForOpeningDoor;

						fLeastDistToPrevSqr = fDistSqr;
						iBestDistToPrevIndex = t;
					}
					else
					{
						m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = true;
					}
				}
				else
				{
					m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = true;
				}
			}
		}

		vDiff = vars.m_vTargetPos - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < fLeastDistToTargetSqr)
		{
			fLeastDistToTargetSqr = fDistSqr;
			iBestDistToTargetIndex = t;
		}
	}

	// Return the point
	if(iBestDistToPrevIndex != -1)
	{
		iPointEnum = iBestDistToPrevIndex;

		Assert(iPointEnum <= MAX_POINT_ENUM_VALUE);

		if(vClosestPoint)
		{
			*vClosestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistToPrevIndex];
		}

		Assert(iBestDistToTargetIndex != -1);
		fCost = (m_Vars.g_vClosestPtsInPoly[iBestDistToTargetIndex] - vars.m_vTargetPos).Mag2();
		if(fCost > 0.0f)
			fCost = (1.0f / invsqrtf_fast(fCost));

		return true;
	}

	return false;
}


bool CPathServerThread::GetFurthestPointInPolyFromTargetWithLos( TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum )
{
	const int iNumClosePts = CreatePointsInPoly(
		vars.m_pToNavMesh,
		vars.m_pToPoly,
		vars.m_pToPolyPoints,
		ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL,
		m_Vars.g_vClosestPtsInPoly );

	const bool bAdjacencyTest = m_pCurrentActivePathRequest->m_bUseVariableEntityRadius;
	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	if(bAdjacencyTest)
	{
		const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
		fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
		fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();
	}

	Vector3 vDiff;
	int iBestDistIndex = -1;
	float fDistSqr;

	static const float fPenaltyForOpeningDoor = 50.0f;

	float fGreatstDistSqr = 0.0f;

	const bool bAbortIfHitAnyObject = (iFlags & TVisitPolyStruct::FLAG_ABORT_IF_HIT_ANY_OBJECT) != 0;

	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vars.m_vTargetPos - m_Vars.g_vClosestPtsInPoly[t];
		//vDiff.z = 0.f;
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < g_fVeryLargeValue && fDistSqr > fGreatstDistSqr)
		{
			if(!bAdjacencyTest || PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], m_pCurrentActivePathRequest->m_fEntityRadius, vExitVerts, &fFreeSpace[0]))
			{
				// Is the vector from the vTestFromPos to this point clear of dynamic object ?
				if(TestDynamicObjectLOS(vars.m_vPointInFromPoly, m_Vars.g_vClosestPtsInPoly[t], CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
				{
					if(m_iDynamicObjLos_NumOpenableObjectsHit > 0)
						fDistSqr += fPenaltyForOpeningDoor;

					fGreatstDistSqr = fDistSqr;
					iBestDistIndex = t;
				}
				else
				{
					m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = true;
					if(bAbortIfHitAnyObject)
						return false;
				}
			}
		}
	}

	// Return the point
	if(iBestDistIndex != -1)
	{
		iPointEnum = iBestDistIndex;

		Assert(iPointEnum <= MAX_POINT_ENUM_VALUE);

		if(vFurthestPoint)
		{
			*vFurthestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistIndex];
		}
	}

	return(iBestDistIndex != -1);
}

bool CPathServerThread::GetFurthestPointInPolyFromTargetWithLosToTarget( TVisitPolyStruct & vars, const u32 UNUSED_PARAM(iFlags), Vector3 * vFurthestPoint, s32 & iPointEnum )
{
	const int iNumClosePts = CreatePointsInPoly(
		vars.m_pToNavMesh,
		vars.m_pToPoly,
		vars.m_pToPolyPoints,
		ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL,
		m_Vars.g_vClosestPtsInPoly );

	const bool bAdjacencyTest = m_pCurrentActivePathRequest->m_bUseVariableEntityRadius;
	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	if(bAdjacencyTest)
	{
		const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
		fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
		fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();
	}

	Vector3 vDiff;
	int iBestDistIndex = -1;
	float fDistSqr;
	bool bHitOpenableObject1 = false;
	bool bHitOpenableObject2 = false;

	static const float fPenaltyForOpeningDoor = 50.0f;
	float fGreatestDistSqr = 0.0f;

	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vars.m_vTargetPos - m_Vars.g_vClosestPtsInPoly[t];
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < g_fVeryLargeValue && fDistSqr > fGreatestDistSqr)
		{
			if(!bAdjacencyTest || PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], m_pCurrentActivePathRequest->m_fEntityRadius, vExitVerts, &fFreeSpace[0]))
			{
				// Is the vector from the vTestFromPos to this point clear of dynamic object ?
				if(TestDynamicObjectLOS(vars.m_vPointInFromPoly, m_Vars.g_vClosestPtsInPoly[t], CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
				{
					bHitOpenableObject1 = m_iDynamicObjLos_NumOpenableObjectsHit > 0;

					// Is the vector from the vTarget to this point clear of dynamic object ?
					if(TestDynamicObjectLOS(vars.m_vTargetPos, m_Vars.g_vClosestPtsInPoly[t], CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
					{
						bHitOpenableObject2 = m_iDynamicObjLos_NumOpenableObjectsHit > 0;

						if(bHitOpenableObject1 || bHitOpenableObject2)
							fDistSqr += fPenaltyForOpeningDoor;

						fGreatestDistSqr = fDistSqr;
						iBestDistIndex = t;
					}
					else
					{
						m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = true;
					}
				}
				else
				{
					m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = true;
				}
			}
		}
	}

	// Return the point
	if(iBestDistIndex != -1)
	{
		iPointEnum = iBestDistIndex;

		Assert(iPointEnum <= MAX_POINT_ENUM_VALUE);

		if(vFurthestPoint)
		{
			*vFurthestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistIndex];
		}
	}

	return(iBestDistIndex != -1);
}


//********************************************************************
//	GetPointInPolyWithBestAngleToHeadingWithLos
//	This function finds the point in the poly, for which the angle
//	between the heading vector and the vector from the start-pos to the
//	point is least.  This is used for wandering, to keep the path
//	heading in the same direction
//********************************************************************

bool CPathServerThread::GetPointInPolyWithBestAngleFromStartWithLos( TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vBestPoint, float * fOutputScore, s32 & iPointEnum )
{
	const int iNumClosePts = CreatePointsInPoly(
		vars.m_pToNavMesh,
		vars.m_pToPoly,
		vars.m_pToPolyPoints,
		ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL,
		m_Vars.g_vClosestPtsInPoly );

	const bool bAdjacencyTest = m_pCurrentActivePathRequest->m_bUseVariableEntityRadius;
	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	if(bAdjacencyTest)
	{
		const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
		fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
		fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();
	}

	Vector3 vDiff;
	int iBestDistIndex = -1;

	static const float fPenaltyForOpeningDoor = 50.0f;

	float fBestValueSoFar = g_fVeryLargeValue;

	const bool bPerformDynamicObjectLos = (iFlags & TVisitPolyStruct::FLAG_PERFORM_DYNAMIC_OBJECT_LOS) != 0;
	const bool bAbortIfHitAnyObject = (iFlags & TVisitPolyStruct::FLAG_ABORT_IF_HIT_ANY_OBJECT) != 0;
	const bool bPerformDynamicObjectLosFurther = (iFlags & TVisitPolyStruct::FLAG_PERFORM_DYNAMIC_OBJECT_LOS_FURTHER) != 0;
	float fScore;

	for(int t=0; t<iNumClosePts; t++)
	{
		// Penalize for when the point is not in our preferred direction
		Vector3 VecToNext = m_Vars.g_vClosestPtsInPoly[t] - m_Vars.m_vLastClosestPoint;
		const float fMag2 = VecToNext.Mag2();
		if(fMag2 > SMALL_FLOAT)
		{
			VecToNext.NormalizeFast();
			const float fAbsPlaneAngle = AcosfSafe(Dot(VecToNext, m_Vars.m_vWanderPlaneNormal)) * 0.3183099f;// 1.0f / M_PI;
			fScore = fAbsPlaneAngle * m_Vars.m_fWander180TurnMultiplier;
		}
		else
		{
			fScore = 0.0f;
		}

		// And when it is wandering off far from our direction or going backwards
		const float fPlaneBackwardDist = Dot(m_Vars.g_vClosestPtsInPoly[t], m_Vars.m_vWanderPlaneNormal) + m_Vars.m_fWanderPlaneDist;
		const float fAbsRightPlaneDist = Abs(Dot(m_Vars.g_vClosestPtsInPoly[t], m_Vars.m_vWanderPlaneRightNormal) + m_Vars.m_fWanderPlaneRightDist);
		fScore += Max(fAbsRightPlaneDist, -fPlaneBackwardDist) * m_Vars.m_fWanderTurnPenalty;

		if(fScore < fBestValueSoFar)
		{
			if(!bAdjacencyTest || PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], m_pCurrentActivePathRequest->m_fEntityRadius, vExitVerts, &fFreeSpace[0]))
			{
				// This has a good score, but does a line-of-sight exist to it?
				if(!bPerformDynamicObjectLos)
				{
					fBestValueSoFar = fScore;
					iBestDistIndex = t;
				}
				else
				{
					const Vector3 vTestPos = (bPerformDynamicObjectLosFurther ? m_Vars.g_vClosestPtsInPoly[t] + VecToNext : m_Vars.g_vClosestPtsInPoly[t]);
					if(TestDynamicObjectLOS(vars.m_vPointInFromPoly, vTestPos, CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
					{
						if(m_iDynamicObjLos_NumOpenableObjectsHit > 0)
							fScore += fPenaltyForOpeningDoor;

						if (fScore < fBestValueSoFar)
						{
							fBestValueSoFar = fScore;
							iBestDistIndex = t;
						}
					}
					else
					{
						m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = true;
						if(bAbortIfHitAnyObject)
							return false;
					}
				}
			}
		}
	}

	// Return the point
	if(iBestDistIndex != -1)
	{
		iPointEnum = iBestDistIndex;

		Assert(iPointEnum <= MAX_POINT_ENUM_VALUE);

		if(vBestPoint)
		{
			*vBestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistIndex];
		}
		if(fOutputScore)
		{
			*fOutputScore = fBestValueSoFar;
		}

		return true;
	}

	return false;
}


float CPathServerThread::GetClosestPointInPolyFromLastPointDistSqr(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum, const Vector3& vLastPoint)
{
	EPointsInPolyType ePointInPolyType;

	if( iFlags & TVisitPolyStruct::FLAG_CONSIDER_ONLY_VERTICES_AND_EDGES )
	{
		ePointInPolyType = POINTSINPOLY_VERTICESANDCENTROID;
	}
	else if( (iFlags & TVisitPolyStruct::FLAG_CONSIDER_ONLY_CENTROID_FOR_FRAGMENTS) && (vars.m_pToPoly->TestFlags(NAVMESHPOLY_TESSELLATED_FRAGMENT)))
	{
		ePointInPolyType = POINTSINPOLY_CENTROID_ONLY;
	}
	else
	{
		ePointInPolyType = ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL;
	}

	const int iNumClosePts = CreatePointsInPoly( vars.m_pToNavMesh, vars.m_pToPoly, vars.m_pToPolyPoints, ePointInPolyType, m_Vars.g_vClosestPtsInPoly );

	const bool bAdjacencyTest = m_pCurrentActivePathRequest->m_bUseVariableEntityRadius;

	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	if(bAdjacencyTest)
	{
		const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
		fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
		fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();
	}

	Vector3 vDiff;
	int iBestDistIndex = -1;
	float fDistSqr;

	float fGreatestDistSqr = g_fVeryLargeValue - 1.f;

	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vLastPoint - m_Vars.g_vClosestPtsInPoly[t];
		//vDiff.z = 0.f;
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < g_fVeryLargeValue && fDistSqr < fGreatestDistSqr)
		{
			if(!bAdjacencyTest || PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], m_pCurrentActivePathRequest->m_fEntityRadius, vExitVerts, &fFreeSpace[0]))
			{
				fGreatestDistSqr = fDistSqr;
				iBestDistIndex = t;
			}
		}
	}

	fDistSqr = fGreatestDistSqr;

	// Return the point
	if(vFurthestPoint)
	{
		*vFurthestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistIndex];
	}

	iPointEnum = iBestDistIndex;

	return fDistSqr;
}

bool CPathServerThread::GetClosestPointInPolyFromLastPointWithLos(TVisitPolyStruct & vars, const u32 iFlags, Vector3 * vFurthestPoint, s32 & iPointEnum, const Vector3& vLastPoint)
{
	const int iNumClosePts = CreatePointsInPoly(
		vars.m_pToNavMesh,
		vars.m_pToPoly,
		vars.m_pToPolyPoints,
		ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL,
		m_Vars.g_vClosestPtsInPoly );

	const bool bAdjacencyTest = m_pCurrentActivePathRequest->m_bUseVariableEntityRadius;
	// Needed for adjacency entity-radius test
	Vector3 vExitVerts[2];
	float fFreeSpace[2];
	if(bAdjacencyTest)
	{
		const s32 iNextEdge = (vars.m_iAdjacencyIndex+1)%vars.m_pFromPoly->GetNumVertices();
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, vars.m_iAdjacencyIndex), vExitVerts[0]);
		vars.m_pFromNavMesh->GetVertex( vars.m_pFromNavMesh->GetPolyVertexIndex(vars.m_pFromPoly, iNextEdge), vExitVerts[1]);
		fFreeSpace[0] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+vars.m_iAdjacencyIndex).GetFreeSpaceAroundVertex();
		fFreeSpace[1] = vars.m_pFromNavMesh->GetAdjacentPoly(vars.m_pFromPoly->GetFirstVertexIndex()+iNextEdge).GetFreeSpaceAroundVertex();
	}

	Vector3 vDiff;
	int iBestDistIndex = -1;
	float fDistSqr;

	static const float fPenaltyForOpeningDoor = 50.0f;

	float fGreatstDistSqr = g_fVeryLargeValue - 1.f;

	const bool bAbortIfHitAnyObject = (iFlags & TVisitPolyStruct::FLAG_ABORT_IF_HIT_ANY_OBJECT) != 0;

	for(int t=0; t<iNumClosePts; t++)
	{
		vDiff = vLastPoint - m_Vars.g_vClosestPtsInPoly[t];
		//vDiff.z = 0.f;
		fDistSqr = vDiff.Mag2();

		if(fDistSqr < g_fVeryLargeValue && fDistSqr < fGreatstDistSqr)
		{
			if(!bAdjacencyTest || PathSearch_TestAdjacencyWithRadius(vars.m_pFromNavMesh, vars.m_pFromPoly, vars.m_vPointInFromPoly, vars.m_pToNavMesh, vars.m_pToPoly, m_Vars.g_vClosestPtsInPoly[t], m_pCurrentActivePathRequest->m_fEntityRadius, vExitVerts, &fFreeSpace[0]))
			{
				// Is the vector from the vTestFromPos to this point clear of dynamic object ?
				if(TestDynamicObjectLOS(vars.m_vPointInFromPoly, m_Vars.g_vClosestPtsInPoly[t], CPathServerThread::ms_dynamicObjectsIntersectingPolygons))
				{
					if(m_iDynamicObjLos_NumOpenableObjectsHit > 0)
						fDistSqr += fPenaltyForOpeningDoor;

					fGreatstDistSqr = fDistSqr;
					iBestDistIndex = t;
				}
				else
				{
					m_Vars.m_bHitDynamicObjectWhilstConsideringPathPoly = true;
					if(bAbortIfHitAnyObject)
						return false;
				}
			}
		}
	}

	// Return the point
	if(iBestDistIndex != -1)
	{
		iPointEnum = iBestDistIndex;

		Assert(iPointEnum <= MAX_POINT_ENUM_VALUE);

		if(vFurthestPoint)
		{
			*vFurthestPoint = m_Vars.g_vClosestPtsInPoly[iBestDistIndex];
		}
	}

	return(iBestDistIndex != -1);
}

s32 CPathServerThread::GetClosestPointInFrontOfPlane( TVisitPolyStruct & vars, TNavMeshPoly * UNUSED_PARAM(pPoly), CNavMesh * UNUSED_PARAM(pNavMesh), const Vector3 & vCloseToPos, const Vector3 & vPlaneDir, const float fPlaneD )
{
	const EPointsInPolyType ePointInPolyType = ShouldUseMorePointsForPoly(*vars.m_pToPoly, m_pCurrentActivePathRequest) ? POINTSINPOLY_EXTRA : POINTSINPOLY_NORMAL;

	const int iNumClosePts = CreatePointsInPoly( vars.m_pToNavMesh, vars.m_pToPoly, vars.m_pToPolyPoints, ePointInPolyType, m_Vars.g_vClosestPtsInPoly );

	const float fPlaneEps = 0.01f;
	float fClosestDistSqr = FLT_MAX;
	s32 iClosest = -1;

	for(int t=0; t<iNumClosePts; t++)
	{
		const float fPlanarDist = DotProduct( m_Vars.g_vClosestPtsInPoly[t], vPlaneDir ) + fPlaneD;

		if( fPlanarDist > fPlaneEps )
		{
			const Vector3 vDiff = m_Vars.g_vClosestPtsInPoly[t] - vCloseToPos;
			const float fDiffSqr = vDiff.XYMag2();
			if( fDiffSqr < fClosestDistSqr )
			{
				fClosestDistSqr = fDiffSqr;
				iClosest = t;
			}
		}
	}
	return iClosest;
}

//-----------------------------------------------------------------------------------------------------------------------------
// PathSearch_TestAdjacencyWithRadius
// Test whether moving from pFromPoly to pToPoly, with points given, will leave the navmesh if the entity is of fEntityWidth
// NB: For now this will be an extremely inefficient implementation with full LOS tests.  If this works as a proof of concept,
// then it will be optimised with custom code tailored for this case.

bool CPathServerThread::PathSearch_TestAdjacencyWithRadius(
	CNavMesh * UNUSED_PARAM(pFromNavMesh), TNavMeshPoly * UNUSED_PARAM(pFromPoly), const Vector3 & vFromPos,
	CNavMesh * UNUSED_PARAM(pToNavMesh), TNavMeshPoly * UNUSED_PARAM(pToPoly), const Vector3 & vToPos,
	float fEntityRadius, Vector3 * pExitVerts, float * pExitFreeSpace)
{
	fEntityRadius -= PATHSERVER_PED_RADIUS;

	u32 e;
	const Vector3 vMoveVec = vToPos - vFromPos;
	Vector3 vClosestPoint;

	//-------------------------------------------------------------------------------------------
	// Test the two vertices which are either side of the edge by which we are leaving pFromPoly

	for(e=0; e<2; e++)
	{
		const float fVertexSpace = pExitFreeSpace[e];

		const float fT = geomTValues::FindTValueOpenSegToPoint( vFromPos, vMoveVec, pExitVerts[e] );

		vClosestPoint = vFromPos + (vMoveVec * fT);

		const float fDistToVert = (vClosestPoint - pExitVerts[e]).Mag();

		if(fDistToVert - fEntityRadius + fVertexSpace < 0.0f)
		{
			if(m_Vars.m_fParentDistanceTravelled > fEntityRadius)
				return false;
		}
	}

	return true;

}


