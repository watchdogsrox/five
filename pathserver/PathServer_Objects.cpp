#include "PathServer.h"
//****************************************************************************
//	PathServer_Objects
//	This file contains all the functions for dealing with dynamic objects.
//	That includes the functions concerned with tessellating or clipping the
//	navmesh polys around these objects.
//****************************************************************************

#include "ai/navmesh/tessellation.h"
#include "fragment/type.h"
#ifdef GTA_ENGINE
// These includes for using within the GTA engine
#include "game/ModelIndices.h"
#include "Objects\Door.h"
#include "Objects\DoorTuning.h"
#include "Objects\Object.h"
#include "Vehicles\Vehicle.h"
#include "ModelInfo\PedModelInfo.h"
#include "Game\ModelIndices.h"
#include "Peds\Ped.h"
#include "Peds\PedGeometryAnalyser.h"
#include "Peds\PedIntelligence.h"
#include "Task\Movement\TaskNavMesh.h"
#include "Task\Vehicle\TaskCarUtils.h"
#include "Vfx\Misc\Fire.h"
#else	// GTA_ENGINE

// This include for when developing the system outside of GTA
#include "GTATypes.h"
#endif

#include "math/angmath.h"
#include "atl/array.h"
#include "fwmaths/random.h"
#include "fwscene/world/worldlimits.h"

// Define this to have the pathfinder ignore objects which have fallen flat.  The "m_bIsCurrentlyAnObstacle" flag becomes reset if so.
#define __DEACTIVATE_FLAT_OBJECTS			1	//0
#define __DEACTIVATE_UPROOTED_OBJECTS		0	//1
// This define makes the TestDynamicObjectLOS functions use an additional 2nd LOS a little higher than the first.
// NB: This will help the cases where objects aren't placed exactly upon the ground.  The first LOS is quite close
// to the floor, and sometimes misses objects which are above this.
#define __USE_SECOND_OBJECT_LOS_TEST		1

// If not defined then we will never allow paths to push closed vehicle doors (by default the optional flag is in the path request)
#define __ALLOW_TO_PUSH_VEHICLE_DOORS_CLOSED	1

#define __TESSELLATE_USE_EXPANDED_CONNECTION_POLYS		0
#define __TESSELLATED_POLYS_USE_PARENTS_MINMAX			0 //1
#define __CONNECTION_POLYS_USE_PARENTS_MINMAX			1
#define __REUSE_TESSELLATION_POLYS						0	// Makes better use of tessellation pool, makes for very complex code (NB: not working)

#ifdef GTA_ENGINE
AI_NAVIGATION_OPTIMISATIONS()
NAVMESH_OPTIMISATIONS()
#endif


const float CNavMeshTrackedObject::DEFAULT_RESCAN_PROBE_LENGTH = 3.0f;

//-----------------------------------------------------------------------------

#if __TRACK_PEDS_IN_NAVMESH
#ifdef GTA_ENGINE

const float CNavMeshTrackedObject::ms_fMaxTrackingPositionChangeSqr = 10.0f * 10.0f;
const float CNavMeshTrackedObject::ms_fDefaultMaxErrorForPathSearches = 0.1f;
const u32 CNavMeshTrackedObject::ms_iMaxRescanFreq = 1000;
const float CNavMeshTrackedObject::ms_fOffMap = -99999.0f;
#if __BANK && defined(DEBUG_DRAW)
bank_bool CNavMeshTrackedObject::ms_bDrawPedTrackingProbes = false;
#endif

CNavMeshTrackedObject::CNavMeshTrackedObject() :
	m_vLastPosition(ms_fOffMap, ms_fOffMap, ms_fOffMap),
	m_vLastNavMeshIntersection(ms_fOffMap, ms_fOffMap, ms_fOffMap),
	m_iHitEdgeIndex(-1),
	m_bHasValidLastPositionOnNavMesh(false),
	m_bHitNavMeshEdge(false),
	m_bPerformAvoidanceLineTests(false),
	m_bAvoidanceLeftIsBlocked(0),
	m_bAvoidanceRightIsBlocked(0),
    m_bLosAheadIsBlocked(false),
#if __BANK
    m_bPerformedRescanLastFrame(false),
#endif // __BANK
	m_fZPosFromTask(-10000.0f),
	m_iLastRescanTime(0),
	m_bTeleported(true)
{

}
CNavMeshTrackedObject::~CNavMeshTrackedObject()
{

}

bool CNavMeshTrackedObject::IsLastNavMeshIntersectionValid() const
{
	return (m_vLastNavMeshIntersection.x != ms_fOffMap &&
		m_vLastNavMeshIntersection.y != ms_fOffMap &&
		m_vLastNavMeshIntersection.z != ms_fOffMap);
}

bool CNavMeshTrackedObject::IsUpToDate(const Vector3 & vPosition, const float fEps) const
{
	return (vPosition - m_vLastPosition).Mag2() < fEps*fEps;
}

void CNavMeshTrackedObject::SetInvalid()
{
	m_vLastPosition = Vector3(ms_fOffMap, ms_fOffMap, ms_fOffMap);
	m_vPolyNormal.Zero();
	m_NavMeshAndPoly.m_iNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;
	m_NavMeshAndPoly.m_iPolyIndex = NAVMESH_POLY_INDEX_NONE;
	m_bPerformAvoidanceLineTests = false;

	for (int i = 0; i < m_NavMeshLosChecks.GetCount(); i++)
	{
		m_NavMeshLosChecks[i].Reset();
	}
}

void CNavMeshTrackedObject::Teleport(const Vector3 & vNewPosition)
{
	m_vLastPosition = vNewPosition;
	m_vPolyNormal.Zero();
	m_bUpToDate = false;
	m_bTeleported = true;
	m_bHasValidLastPositionOnNavMesh = false;
	m_bHitNavMeshEdge = false;
	m_bAvoidanceLeftIsBlocked = 0;
	m_bAvoidanceRightIsBlocked = 0;
	m_iHitEdgeIndex = -1;
	m_NavMeshAndPoly.m_iNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;
	m_NavMeshAndPoly.m_iPolyIndex = NAVMESH_POLY_INDEX_NONE;
	m_bPerformAvoidanceLineTests = false;

	for (int i = 0; i < m_NavMeshLosChecks.GetCount(); i++)
	{
		m_NavMeshLosChecks[i].Reset();
	}
}




bool CNavMeshTrackedObject::QueryLosCheck(Vector3& o_Intersection, Vector3& o_Vertex1, Vector3& o_Vertex2, const Vector3& in_Position, const Vector3& in_EndPosition) const
{
	for ( int i = 0; i < m_NavMeshLosChecks.GetCount(); i++ )
	{
		o_Vertex1 = m_NavMeshLosChecks[i].m_Vertex1;
		o_Vertex2 = m_NavMeshLosChecks[i].m_Vertex2;

		// check to see if we intersect this segment 
		if(CNavMesh::LineSegsIntersect2D(in_Position, in_EndPosition, o_Vertex1, o_Vertex2, &o_Intersection) == SEGMENTS_INTERSECT )
		{

			// check we're not already on other side of object
			// note assume point ordering here
			Vector3 vSeg = o_Vertex2 - o_Vertex1;
			Vector3 vIntToPos = in_Position - o_Intersection;

			// Vec2V vIntersection = Vec2V(o_Intersection.x, o_Intersection.y);
			Vec2V vSegDir = Normalize(Vec2V(vSeg.x, vSeg.y));
			Vec2V vSegRight = Vec2V(vSegDir.GetY(), -vSegDir.GetX());
			Vec2V vIntersectionToPos = Normalize(Vec2V(vIntToPos.x, vIntToPos.y));
			if ( (Dot(vSegRight, vIntersectionToPos ) >= ScalarV(0.0f) ).Getb() )
			{
				return true;
			}
		}
	}
	return false;
}

void CNavMeshTrackedObject::RequestLosCheck(const Vector3& in_Vector)
{
	// only supporting one right now
	if ( m_NavMeshLosRequests.GetAvailable() > 0 )
	{
		m_NavMeshLosRequests.Push(in_Vector);
	}
}

void CNavMeshTrackedObject::Rescan(const Vector3 & vObjectPos, const float rescanProbeLength)
{
	// The tracking is always done on the regular navmesh for now.
	const aiNavDomain domain = kNavDomainRegular;

	m_iLastRescanTime = fwTimer::GetTimeInMilliseconds();

	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex( CPathServer::GetNavMeshIndexFromPosition(vObjectPos, domain), domain );
	u32 iPolyIndex = NAVMESH_POLY_INDEX_NONE;

	if(pNavMesh)
	{
		const float fDistBelow = rescanProbeLength;
		Vector3 vIntersect;
		iPolyIndex = pNavMesh->GetPolyBelowPoint(vObjectPos, vIntersect, fDistBelow);

		if(iPolyIndex != NAVMESH_POLY_INDEX_NONE)
		{
			m_NavMeshAndPoly.m_iNavMeshIndex = pNavMesh->GetIndexOfMesh();
			m_NavMeshAndPoly.m_iPolyIndex = iPolyIndex;
			m_bHasValidLastPositionOnNavMesh = true;
			m_vLastNavMeshIntersection = vIntersect;

			// Store off data etc from the TNavMeshPoly underfoot
			// Should we store the entire poly? (40 bytes)
			
			TNavMeshPoly * pPoly = pNavMesh->GetPoly(iPolyIndex);

			// TODO: Move this into CNavMeshTrackedObject or TNavPolyData
			m_NavPolyData.m_bPavement = pPoly->GetIsPavement();
			m_NavPolyData.m_bSheltered = pPoly->GetIsSheltered();
			m_NavPolyData.m_bIsolated = pPoly->GetIsIsolated();
			m_NavPolyData.m_bNetworkSpawn = pPoly->GetIsNetworkSpawnCandidate();
			m_NavPolyData.m_bIsRoad = pPoly->GetIsRoad();
			m_NavPolyData.m_bInterior = pPoly->GetIsInterior();
			m_NavPolyData.m_bIsWater = pPoly->GetIsWater();
			m_NavPolyData.m_iAudioProperties = pPoly->GetAudioProperties();
			m_NavPolyData.m_bTrainTracks = pPoly->GetIsTrainTrack();
		}
	}

	if(!pNavMesh || iPolyIndex == NAVMESH_POLY_INDEX_NONE)
	{
		// If we got here, then it means we could not find a navmesh poly underneath the ped
		m_NavMeshAndPoly.m_iNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;
		m_NavMeshAndPoly.m_iPolyIndex = NAVMESH_POLY_INDEX_NONE;
	}
}


void CPathServer::UpdateTrackedObject(CNavMeshTrackedObject & obj, const Vector3 & vObjectPos, const float rescanProbeLength, bool in_bNeedsToReScan)
{
	Assert(m_iImmediateModeNumVisitedPolys==0);
	m_iImmediateModeNumVisitedPolys = 0;

	// The tracking is always done on the regular navmesh for now.
	const aiNavDomain domain = kNavDomainRegular;

    BANK_ONLY(obj.ResetPerformedRescanLastUpdate());

	// If the position of the object has changed over some threshold we will rescan
	bool bNeedToReScan = in_bNeedsToReScan || TrackedObjectNeedsRescan(obj, vObjectPos);

	// If the navmesh:poly info is up-to-date, and the ped hasn't moved then there's no need to rescan
	if(obj.m_bUpToDate && !bNeedToReScan)
		return;

	if(bNeedToReScan)
	{
		const Vector3 vDiff = vObjectPos - obj.m_vLastPosition;

		// If the delta is sufficiently small, then we will try to track the object by simply
		// sliding it across the navmesh surface noting when we exit/enter polygons.  If we
		// can successfully track the object, then update its navmesh:poly indices and set it's
		// m_bUpToDate flag.  If not, we will have to search for the start/end points next
		// time we handle a path request.
		if(vDiff.Mag2() < CNavMeshTrackedObject::ms_fMaxTrackingPositionChangeSqr)
		{
			const Vector2 vDir(vDiff.x, vDiff.y);

			if(vDir.Mag2() > 0.0f)
			{
				if(obj.m_NavMeshAndPoly.m_iNavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE && obj.m_NavMeshAndPoly.m_iPolyIndex != NAVMESH_POLY_INDEX_NONE)
				{
					CNavMesh * pNavMesh = GetNavMeshFromIndex(obj.m_NavMeshAndPoly.m_iNavMeshIndex, domain);
					if(pNavMesh)
					{
						Vector3 vIntersectPos;

						TNavMeshPoly * pStartPoly = pNavMesh->GetPoly(obj.m_NavMeshAndPoly.m_iPolyIndex);
						TNavMeshPoly * pEndPoly = CPathServer::TestShortLineOfSightImmediate(obj.m_vLastPosition, vDir, pStartPoly, &vIntersectPos, NULL, NULL, domain);
						Assert(m_iImmediateModeNumVisitedPolys == 0);

						if(pEndPoly)
						{
							Assert(pEndPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);

							CNavMesh * pEndNavMesh = CPathServer::GetNavMeshFromIndex(pEndPoly->GetNavMeshIndex(), domain);

							obj.m_NavMeshAndPoly.m_iNavMeshIndex = pEndPoly->GetNavMeshIndex();
							obj.m_NavMeshAndPoly.m_iPolyIndex = pEndNavMesh->GetPolyIndex(pEndPoly);
							obj.m_bHasValidLastPositionOnNavMesh = true;
							obj.m_vLastNavMeshIntersection = vIntersectPos;

							// Store off data etc from the TNavMeshPoly underfoot
							// Should we store the entire poly? (40 bytes)

							// TODO: Move this into CNavMeshTrackedObject or TNavPolyData
							obj.m_NavPolyData.m_bPavement = pEndPoly->GetIsPavement();
							obj.m_NavPolyData.m_bSheltered = pEndPoly->GetIsSheltered();
							obj.m_NavPolyData.m_bIsolated = pEndPoly->GetIsIsolated();
							obj.m_NavPolyData.m_bNetworkSpawn = pEndPoly->GetIsNetworkSpawnCandidate();
							obj.m_NavPolyData.m_bIsRoad = pEndPoly->GetIsRoad();
							obj.m_NavPolyData.m_bInterior = pEndPoly->GetIsInterior();
							obj.m_NavPolyData.m_bIsWater = pEndPoly->GetIsWater();
							obj.m_NavPolyData.m_iAudioProperties = pEndPoly->GetAudioProperties();
							obj.m_NavPolyData.m_bTrainTracks = pEndPoly->GetIsTrainTrack();

							bNeedToReScan = false;
						}
					}
				}
			}
		}

		// If we still need to rescan, then the delta update must have failed :-(
		if(bNeedToReScan)
		{
			obj.Rescan(vObjectPos, rescanProbeLength);
            BANK_ONLY(obj.SetPerformedRescanLastUpdate());
		}

		Assert(m_iImmediateModeNumVisitedPolys==0);

		obj.m_bTeleported = false;
		obj.m_bUpToDate = true;
		obj.m_vLastPosition = vObjectPos;
	}
}


bool CPathServer::TrackedObjectNeedsRescan(const CNavMeshTrackedObject& obj, const Vector3& vObjectPos)
{
	// check if the object has been teleported
	if( obj.m_bTeleported )
	{
		// needs rescan
		return true;
	}

	// check if the object has been displaced
	static const float fEpsXY = 0.01f;
	static const float fEpsZ = 0.1f;
	if( (!rage::IsNearZero(vObjectPos.x - obj.m_vLastPosition.x, fEpsXY)) ||
		(!rage::IsNearZero(vObjectPos.y - obj.m_vLastPosition.y, fEpsXY)) ||
		(!rage::IsNearZero(vObjectPos.z - obj.m_vLastPosition.z, fEpsZ)) )
	{
		// needs rescan
		return true;
	}

	// by default object does not need a rescan
	return false;
}


Vector3 CPathServer::UpdateTrackedObjectConstrainedToNavMesh(const Vector3 & vStartPos, const Vector3 & vMoveSpeed, CNavMeshTrackedObject & obj)
{
	// The tracking is always done on the regular navmesh for now.
	const aiNavDomain domain = kNavDomainRegular;

	if(obj.GetNavMeshAndPoly().m_iNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE || obj.GetNavMeshAndPoly().m_iPolyIndex == NAVMESH_POLY_INDEX_NONE)
	{
		// Beware! We don't want a ped to get into a state of repeatedly rescanning, or it will spamn the pathfinder
		if(obj.GetLastRescanTime() + CNavMeshTrackedObject::ms_iMaxRescanFreq < fwTimer::GetTimeInMilliseconds())
		{
			obj.Rescan(vStartPos);
		}
		else
		{
			return vStartPos;
		}
	}

	static const float fDistLeftEps = 0.0f;
	static const float fDistMovedEps = 0.001f;

	Vector2 vMoveDir(vMoveSpeed.x, vMoveSpeed.y);
	vMoveDir *= fwTimer::GetTimeStep();

	float fDistLeft = vMoveDir.Mag();

	if(fDistLeft <= fDistMovedEps)
		return vStartPos;

	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(obj.GetNavMeshAndPoly().m_iNavMeshIndex, domain);
	if(!pNavMesh)
	{
		return vStartPos;
	}

	TNavMeshPoly * pPoly = pNavMesh->GetPoly(obj.GetNavMeshAndPoly().m_iPolyIndex);

	Vector3 vIntersectPos(0.0f,0.0f,0.0f);
	Vector3 vIntersectEdgeVec(0.0f, 0.0f, 0.0f);
	int iEdgeHitIndex = obj.GetHitEdgeIndex();	// get the initial state from last frame

	// vCurrentPos has to be on the navmesh.  The input vec is the ped's position, which is 1m above.
	Vector3 vCurrentPos = Vector3(vStartPos.x, vStartPos.y, vStartPos.z - 1.0f);

	int iIterations = 10;

	while(fDistLeft > fDistLeftEps && iIterations-- > 0)
	{
		// If we're not on an edge, then try to move the position across the mesh
		if(iEdgeHitIndex==-1)
		{
			TNavMeshPoly * pEndPoly = CPathServer::TestShortLineOfSightImmediate(vCurrentPos, vMoveDir, pPoly, &vIntersectPos, &vIntersectEdgeVec, &iEdgeHitIndex, domain);
			if(!pEndPoly)
			{
				//************************************************************************************************
				// Ensure that the start poly is actually underneath the start positiion.
				// If we're really unlucky the position could be exactly between polygons & might not be found..

				const Vector3 vJitteredStart(
					vCurrentPos.x + fwRandom::GetRandomNumberInRange(-0.1f, 0.1f),
					vCurrentPos.y + fwRandom::GetRandomNumberInRange(-0.1f, 0.1f),
					vCurrentPos.z);
				const Vector3 vEnd = vJitteredStart + Vector3(vMoveDir.x, vMoveDir.y, 0.0f);

				pEndPoly = CPathServer::TestShortLineOfSightImmediate(vJitteredStart, vEnd, vIntersectPos, domain);
				Assert(pEndPoly);
			}

			if(!pEndPoly)
			{

				//************************************************************************************************
				// Its possible that the vCurrentPos is on a navmesh poly edge, but the LOS test is not detecting
				// this fact.  Iterate through the pPoly's edges and see if the position is exactly upon an edge.

				Vector3 vCurr, vLast;

				float fClosestEdgeDist = FLT_MAX;
				int iClosestEdge = -1;
				static dev_float fCloseEdgeEps = 0.05f;

				int v2=pPoly->GetNumVertices()-1;
				pNavMesh->GetVertex(pNavMesh->GetPolyVertexIndex(pPoly, v2), vLast);
				vLast.z = vCurrentPos.z;

				for(u32 v1=0; v1<pPoly->GetNumVertices(); v1++)
				{
					pNavMesh->GetVertex(pNavMesh->GetPolyVertexIndex(pPoly, v1), vCurr);
					vCurr.z = vCurrentPos.z;

					const TAdjPoly & closedEdge = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex() + v1);
					if(closedEdge.GetAdjacencyType()!=ADJACENCY_TYPE_NORMAL)
					{
						const float fEdgeDist = geomDistances::Distance2SegToPoint(vLast, vCurr-vLast, vCurrentPos);
						if(fEdgeDist > fCloseEdgeEps && fEdgeDist < fClosestEdgeDist)
						{
							fClosestEdgeDist = fEdgeDist;
							iClosestEdge = v1;
						}
					}

					vLast = vCurr;
					v2 = v1;
				}

				if(iClosestEdge == -1)
				{
					// Still not found
					break;
				}

				iEdgeHitIndex = iClosestEdge;
			}

			else
			{
				pPoly = pEndPoly;
				const float fDistMoved = (vIntersectPos - vCurrentPos).Mag();
				vCurrentPos = vIntersectPos;

				if(fDistMoved < fDistMovedEps && iEdgeHitIndex==-1)
					break;

				if(fDistMoved > 0.0f)
				{
					vMoveDir.Scale(fDistMoved / fDistLeft);
					fDistLeft -= fDistMoved;
				}
				if(fDistLeft <= fDistLeftEps)
					break;
			}
		}

		// If we have hit an edge, then slide the position along the navmesh edges
		if(iEdgeHitIndex!=-1)
		{
			int iEdgeHitIndexBefore = iEdgeHitIndex;
			TNavMeshPoly * pPolyBefore = pPoly;
			TNavMeshPoly * pEndPoly = CPathServer::SlidePointAlongNavMeshEdgeImmediate(vCurrentPos, vMoveDir, pPoly, iEdgeHitIndex, vIntersectPos, domain);
			if(!pEndPoly)
				break;

			pPoly = pEndPoly;
			pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);
			Assert(pNavMesh);

			const float fDistMoved = (vIntersectPos - vCurrentPos).Mag();

			vCurrentPos = vIntersectPos;

			// Disengaged from edge?
			if(iEdgeHitIndex==-1)
				continue;

			if(pEndPoly==pPolyBefore && iEdgeHitIndex==iEdgeHitIndexBefore)
			{
				break;
			}

			vMoveDir.Scale(fDistMoved / fDistLeft);
			fDistLeft -= fDistMoved;
			if(fDistLeft <= fDistLeftEps)
				break;
			if(fDistMoved < fDistMovedEps)
				break;
		}
	}

	if(pPoly)
	{
		pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);

		obj.m_bUpToDate = true;
		obj.m_NavMeshAndPoly.m_iNavMeshIndex = pNavMesh->GetIndexOfMesh();
		obj.m_NavMeshAndPoly.m_iPolyIndex = pNavMesh->GetPolyIndex(pPoly);
		obj.m_vLastPosition = Vector3(vCurrentPos.x, vCurrentPos.y, vCurrentPos.z + 1.0f);
		obj.m_vLastNavMeshIntersection = vCurrentPos;
		obj.m_bHitNavMeshEdge = iEdgeHitIndex != -1;
		obj.m_iHitEdgeIndex = (s16)iEdgeHitIndex;
		obj.m_vNavMeshIntersectEdge = vIntersectEdgeVec;
	}
	else
	{
		obj.m_bUpToDate = true;
		// Leave the poly & navmesh indices unchanged
		// Place the ped back at the last good navmesh intersection position
		obj.m_vLastPosition = Vector3(obj.m_vLastNavMeshIntersection.x, obj.m_vLastNavMeshIntersection.y, obj.m_vLastNavMeshIntersection.z + 1.0f);
		obj.m_bHitNavMeshEdge = false;
		obj.m_iHitEdgeIndex = -1;
		obj.m_vNavMeshIntersectEdge.Zero();
	}

	return obj.m_vLastPosition;
}

#endif	// GTA_ENGINE
#endif	// __TRACK_PEDS_IN_NAVMESH

//-----------------------------------------------------------------------------------
// FUNCTION : TessellateNavMeshPolyAndCreateDegenerateConnections
// PURPOSE : Tessellates the given navmesh into a number of fragments, and sets up
// adjacencies between them.  Creates zero area 'connection' polygons between the
// fragments and the input polygon's neighbours - so as to bisect each edge 1:2


bool CPathServer::OnFirstVisitingPolyCallback(TNavMeshPoly * pPoly)
{
	return m_PathServerThread.OnFirstVisitingPoly(pPoly);
}

//-----------------------------------------------------------------------------------
// FUNCTION : TessellateNavMeshPolyAndCreateDegenerateConnections
// PURPOSE : Tessellates the given navmesh into a number of fragments, and sets up
// adjacencies between them.  Creates zero area 'connection' polygons between the
// fragments and the input polygon's neighbours - so as to bisect each edge 1:2

#if __TESSELLATE_USE_EXPANDED_CONNECTION_POLYS
static const float fExpandWeighting = 0.2f;
static const float fOtherVertWeighting = (1.0f - fExpandWeighting) * 0.5f;
#endif


bool CPathServerThread::TessellateNavMeshPolyAndCreateDegenerateConnections(
	CNavMesh * pNavMesh, TNavMeshPoly * pPoly, u32 iPolyIndex,
	bool bThisIsStartPoly, bool bThisIsEndPoly,
	TNavMeshPoly ** pOutStartingPoly,
	u32 iReturnStartingPoly_NavMeshIndex,
	u32 iReturnStartingPoly_PolyIndex,
	atArray<TNavMeshPoly*> * outputFragments,
	Vector3 * pMidPoint)
{
	// This is only for the regular navmesh domain at this point.
	const aiNavDomain domain = kNavDomainRegular;

#if !__FINAL
	m_PolyTessellationTimer->Reset();
	m_PolyTessellationTimer->Start();
#endif

	Assert(!pPoly->GetIsDegenerateConnectionPoly());

	//*****************************************************************
	// See if we have enough tessellation polys free for tessellating.
	// The number we'll use is iNumVerts*3.

	const u32 iSpaceAvailable = CNavMesh::ms_iNumPolysInTesselationNavMesh - CPathServer::m_iCurrentNumTessellationPolys;
	if(iSpaceAvailable < pPoly->GetNumVertices()*3)
	{
#if !__FINAL
		m_PolyTessellationTimer->Stop();
		m_pCurrentActivePathRequest->m_fMillisecsSpentInTessellation += (float) m_PolyTessellationTimer->GetTimeMS();
#endif
		Assertf(false, "WARNING : No space to tessellate this polygon, earlying out.\n");
		return false;
	}

	static Vector3 vTempVerts[NAVMESHPOLY_MAX_NUM_VERTICES];

	// I've doubled the number of tessellated polys created from each poly.
	// There will still be the fan-shaped tessellation scheme, but triangles will be
	// created with poly-edge midpoints as well as just the existing vertices.
	// In this manner we will bisect each edge and allow better navigation around
	// objects in cramped spaced.  HOWEVER - this will (along with the connection polys)
	// TRIPLE the amount of tessellation polys required.  If this proves a problem the
	// requirements could be reduced by allowing tessellation polys to be QUADS, but
	// this will increase the complexity of the code somewhat..
	static u32 iNewPolyIndices[NAVMESHPOLY_MAX_NUM_VERTICES*2];
	static TNavMeshPoly * pNewPolys[NAVMESHPOLY_MAX_NUM_VERTICES*2];

	Vector3 vCentroid;
	u32 v, lastv, a;
	u32 iTessellatedPolyIndex1, iTessellatedPolyIndex2;
	CNavMesh * pTessellationNavMesh = CPathServer::m_pTessellationNavMesh;

	struct TSpecialLinkRetargetInfo
	{
		Vector3 vPosition;
		CSpecialLinkInfo * pLink;
		bool bRetargetFromLink;	// if true we are retargetting the start position, otherwise its the end position
	};

	static TSpecialLinkRetargetInfo specialLinkInfo[NAVMESHPOLY_MAX_SPECIAL_LINKS];
	CNavMesh * pOriginalNavMesh = NULL;
	s32 iNumSpecialLinks = 0;

	if(pPoly->GetNumSpecialLinks() != 0)
	{
		if(pNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION)
		{
			const TTessInfo & tessInfo = CPathServer::m_PolysTessellatedFrom[iPolyIndex];
			pOriginalNavMesh = CPathServer::GetNavMeshFromIndex(tessInfo.m_iNavMeshIndex, kNavDomainRegular);			
		}
		else
		{
			pOriginalNavMesh = pNavMesh;
		}

		s32 iLinkLookupIndex = pPoly->GetSpecialLinksStartIndex();

		for(s32 s=0; s<pPoly->GetNumSpecialLinks(); s++)
		{
			const u16 iLinkIndex = pOriginalNavMesh->GetSpecialLinkIndex(iLinkLookupIndex++);
			Assert(iLinkIndex < pOriginalNavMesh->GetNumSpecialLinks());
			CSpecialLinkInfo * pLink = &pOriginalNavMesh->GetSpecialLinks()[iLinkIndex];

			if(pLink->GetAStarTimeStamp() != m_iAStarTimeStamp)
			{
				pLink->Reset();
				pLink->SetAStarTimeStamp(m_iAStarTimeStamp);
			}

			if(pLink->GetLinkFromNavMesh() == pNavMesh->GetIndexOfMesh() && pLink->GetLinkFromPoly() == iPolyIndex)
			{
				specialLinkInfo[iNumSpecialLinks].pLink = pLink;
				specialLinkInfo[iNumSpecialLinks].bRetargetFromLink = true;
				CNavMesh::DecompressVertex(
					specialLinkInfo[iNumSpecialLinks].vPosition, pLink->GetLinkFromPosX(), pLink->GetLinkFromPosY(), pLink->GetLinkFromPosZ(),
					pOriginalNavMesh->GetQuadTree()->m_Mins, pOriginalNavMesh->GetExtents() );
				
				iNumSpecialLinks++;
			}
			else if(pLink->GetLinkToNavMesh() == pNavMesh->GetIndexOfMesh() && pLink->GetLinkToPoly() == iPolyIndex)
			{
				specialLinkInfo[iNumSpecialLinks].pLink = pLink;
				specialLinkInfo[iNumSpecialLinks].bRetargetFromLink = false;
				CNavMesh::DecompressVertex(
					specialLinkInfo[iNumSpecialLinks].vPosition, pLink->GetLinkToPosX(), pLink->GetLinkToPosY(), pLink->GetLinkToPosZ(),
					pOriginalNavMesh->GetQuadTree()->m_Mins, pOriginalNavMesh->GetExtents() );
				
				iNumSpecialLinks++;
			}
		}
	}

	// Important note : during navmesh construction all polys with an area less than NAVMESH_SMALL_POLY_AREA have the NAVMESHPOLY_SMALL flag
	// set for them.  No polys with this flag set are tessellated.  By having a small value for NAVMESHPOLY_SMALL (currently 1.0f?) we can
	// increase the number of polys which get tessellated, and aid the pathfinding.  When creating tessellated fragments here we may want to
	// use a slightly higher value when classifying the size of the fragments - otherwise we will risk bogging down the cpu with the
	// tessellation.  In this way original navmesh polys are more likely to be tessellated than their fragments.

	if(pMidPoint)
		vCentroid = *pMidPoint;
	else
		pNavMesh->GetPolyCentroid(iPolyIndex, vCentroid);

	for(v=0; v<pPoly->GetNumVertices(); v++)
	{
		pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), vTempVerts[v] );
	}

	//*************************************
	// 'Allocate' the tessellation polys

	lastv = pPoly->GetNumVertices()-1;
	for(v=0; v<pPoly->GetNumVertices(); v++)
	{
#if __TESSELLATE_USE_EXPANDED_CONNECTION_POLYS
		const Vector3 vEdgeMidpoint = (vTempVerts[lastv]*fOtherVertWeighting) + (vTempVerts[v]*fOtherVertWeighting) + (vCentroid*fExpandWeighting);
#else
		const Vector3 vEdgeMidpoint = (vTempVerts[lastv] + vTempVerts[v]) * 0.5f;
#endif

		//****************************************************************************
		// Set up the FIRST tessellation poly for this original poly edge.
		// This poly's external edge runs from vTempVerts[lastv] to the edge midpoint

		// NOTE : GetNextFreeTessellationPolyIndex() must be replaced by a function which uses a queue of free indices,
		// or something similar. It currently searches through a bitfield to find the first unset bit, which is slow..

		iTessellatedPolyIndex1 = fwPathServer::GetNextFreeTessellationPolyIndex();
		if(iTessellatedPolyIndex1 == NAVMESH_POLY_INDEX_NONE)
		{
#if !__FINAL
			m_PolyTessellationTimer->Stop();
			m_pCurrentActivePathRequest->m_fMillisecsSpentInTessellation += (float) m_PolyTessellationTimer->GetTimeMS();
#endif
			Assertf(false, "ERROR : There was no space for new tessellation poly #1 in navmesh.\n");
			return false;
		}
		Assert(iTessellatedPolyIndex1 < CNavMesh::ms_iNumPolysInTesselationNavMesh);

		// If the poly we are tessellating is already a tessellated poly itself, then record an entry in the
		// m_PolysTessellatedFrom array, which points us back to the original untessellated poly.
		if(pNavMesh == pTessellationNavMesh)
		{
			TTessInfo * pTessInfo = CPathServer::GetTessInfo(iTessellatedPolyIndex1);
			TTessInfo * pParentFragmentTessInfo = CPathServer::GetTessInfo(iPolyIndex);
			pTessInfo->m_iNavMeshIndex = pParentFragmentTessInfo->m_iNavMeshIndex;
			pTessInfo->m_iPolyIndex = pParentFragmentTessInfo->m_iPolyIndex;
			pTessInfo->m_bInUse = true;
		}
		// The poly is untessellated, so set up a record in the m_PolysTessellatedFrom array.
		else
		{
			TTessInfo * pTessInfo = CPathServer::GetTessInfo(iTessellatedPolyIndex1);
			pTessInfo->m_iNavMeshIndex = (u16)pNavMesh->GetIndexOfMesh();
			pTessInfo->m_iPolyIndex = (u16)iPolyIndex;
			pTessInfo->m_bInUse = true;
		}
		// Set up the new poly & vertices
		TNavMeshPoly * pTessPoly1 = pTessellationNavMesh->GetPoly(iTessellatedPolyIndex1);
		pTessPoly1->SetNumVertices(3);
		pTessPoly1->SetFirstVertexIndex(CPathServer::m_iCurrentNumTessellationPolys*3);
		CPathServer::m_iCurrentNumTessellationPolys++;
		pTessPoly1->m_TimeStamp = 0;
		pTessPoly1->m_AStarTimeStamp = 0;
		pTessPoly1->SetIsTessellatedFragment(true);
		pTessPoly1->SetReplacedByTessellation(false);
		pTessPoly1->SetIsDegenerateConnectionPoly(false);

		pTessellationNavMesh->GetVertexPool()[pTessPoly1->GetFirstVertexIndex()] = vCentroid;
		pTessellationNavMesh->GetVertexPool()[pTessPoly1->GetFirstVertexIndex()+1] = vTempVerts[lastv];
		pTessellationNavMesh->GetVertexPool()[pTessPoly1->GetFirstVertexIndex()+2] = vEdgeMidpoint;

#if __TESSELLATED_POLYS_USE_PARENTS_MINMAX
		pTessPoly1->m_MinMax = pPoly->m_MinMax;
#else
		// Calc min/max
		pTessPoly1->m_MinMax.SetFloat(
			Min(vCentroid.x, Min(vEdgeMidpoint.x, vTempVerts[lastv].x)),
			Min(vCentroid.y, Min(vEdgeMidpoint.y, vTempVerts[lastv].y)),
			Min(vCentroid.z, Min(vEdgeMidpoint.z, vTempVerts[lastv].z)),
			Max(vCentroid.x, Max(vEdgeMidpoint.x, vTempVerts[lastv].x)),
			Max(vCentroid.y, Max(vEdgeMidpoint.y, vTempVerts[lastv].y)),
			Max(vCentroid.z, Max(vEdgeMidpoint.z, vTempVerts[lastv].z))
		);
#endif

		if(!CPathServerThread::OnFirstVisitingPoly(pTessPoly1))
		{
#if !__FINAL
			m_PolyTessellationTimer->Stop();
			m_pCurrentActivePathRequest->m_fMillisecsSpentInTessellation += (float) m_PolyTessellationTimer->GetTimeMS();
#endif
			Assert(0);
			return false;
		}
		Assert(pTessPoly1->GetNavMeshIndex() == NAVMESH_INDEX_TESSELLATION);
		iNewPolyIndices[v*2] = iTessellatedPolyIndex1;
		pNewPolys[v*2] = pTessPoly1;

		//****************************************************************************
		// Set up the SECOND tessellation poly for this original poly edge.
		// This poly's external edge runs from vTempVerts[lastv] to the edge midpoint

		iTessellatedPolyIndex2 = fwPathServer::GetNextFreeTessellationPolyIndex();
		if(iTessellatedPolyIndex2 == NAVMESH_POLY_INDEX_NONE)
		{
#if !__FINAL
			m_PolyTessellationTimer->Stop();
			m_pCurrentActivePathRequest->m_fMillisecsSpentInTessellation += (float) m_PolyTessellationTimer->GetTimeMS();
#endif
			Assertf(false, "ERROR : There was no space for new tessellation poly #2 in navmesh.\n");
			return false;
		}
		Assert(iTessellatedPolyIndex2 < CNavMesh::ms_iNumPolysInTesselationNavMesh);

		// If the poly we are tessellating is already a tessellated poly itself, then record an entry in the
		// m_PolysTessellatedFrom array, which points us back to the original untessellated poly.
		if(pNavMesh == pTessellationNavMesh)
		{
			TTessInfo * pTessInfo = CPathServer::GetTessInfo(iTessellatedPolyIndex2);
			TTessInfo * pParentFragmentTessInfo = CPathServer::GetTessInfo(iPolyIndex);
			pTessInfo->m_iNavMeshIndex = pParentFragmentTessInfo->m_iNavMeshIndex;
			pTessInfo->m_iPolyIndex = pParentFragmentTessInfo->m_iPolyIndex;
			pTessInfo->m_bInUse = true;
		}
		// The poly is untessellated, so set up a record in the m_PolysTessellatedFrom array.
		else
		{
			TTessInfo * pTessInfo = CPathServer::GetTessInfo(iTessellatedPolyIndex2);
			pTessInfo->m_iNavMeshIndex = (u16)pNavMesh->GetIndexOfMesh();
			pTessInfo->m_iPolyIndex = (u16)iPolyIndex;
			pTessInfo->m_bInUse = true;
		}

		// Set up the new poly & vertices
		TNavMeshPoly * pTessPoly2 = pTessellationNavMesh->GetPoly(iTessellatedPolyIndex2);
		pTessPoly2->SetNumVertices(3);
		pTessPoly2->SetFirstVertexIndex(CPathServer::m_iCurrentNumTessellationPolys*3);
		CPathServer::m_iCurrentNumTessellationPolys++;
		pTessPoly2->m_TimeStamp = 0;
		pTessPoly2->m_AStarTimeStamp = 0;
		pTessPoly2->SetIsTessellatedFragment(true);
		pTessPoly2->SetReplacedByTessellation(false);
		pTessPoly2->SetIsDegenerateConnectionPoly(false);

		pTessellationNavMesh->GetVertexPool()[pTessPoly2->GetFirstVertexIndex()] = vCentroid;
		pTessellationNavMesh->GetVertexPool()[pTessPoly2->GetFirstVertexIndex()+1] = vEdgeMidpoint;
		pTessellationNavMesh->GetVertexPool()[pTessPoly2->GetFirstVertexIndex()+2] = vTempVerts[v];

#if __TESSELLATED_POLYS_USE_PARENTS_MINMAX
		pTessPoly2->m_MinMax = pPoly->m_MinMax;
#else
		// Calc min/max
		pTessPoly2->m_MinMax.SetFloat(
			Min(vCentroid.x, Min(vEdgeMidpoint.x, vTempVerts[v].x)),
			Min(vCentroid.y, Min(vEdgeMidpoint.y, vTempVerts[v].y)),
			Min(vCentroid.z, Min(vEdgeMidpoint.z, vTempVerts[v].z)),
			Max(vCentroid.x, Max(vEdgeMidpoint.x, vTempVerts[v].x)),
			Max(vCentroid.y, Max(vEdgeMidpoint.y, vTempVerts[v].y)),
			Max(vCentroid.z, Max(vEdgeMidpoint.z, vTempVerts[v].z))
		);
#endif

		if(!CPathServerThread::OnFirstVisitingPoly(pTessPoly2))
		{
#if !__FINAL
			m_PolyTessellationTimer->Stop();
			m_pCurrentActivePathRequest->m_fMillisecsSpentInTessellation += (float) m_PolyTessellationTimer->GetTimeMS();
#endif
			Assert(0);
			return false;
		}
		Assert(pTessPoly2->GetNavMeshIndex() == NAVMESH_INDEX_TESSELLATION);
		iNewPolyIndices[(v*2)+1] = iTessellatedPolyIndex2;
		pNewPolys[(v*2)+1] = pTessPoly2;

		lastv = v;
	}

	//*****************************************************************************
	// 'Allocate' the connection polys.  Connect all the new polys together.
	// NB: For now just take these from the tessellation pool.
	// In time we may decide to have another store for these, since they could be
	// considerably more light-weight that the other polys.

	lastv = pPoly->GetNumVertices()-1;
	for(v=0; v<pPoly->GetNumVertices(); v++)
	{
		const u32 iDegenerateConnectionPolyIndex = fwPathServer::GetNextFreeTessellationPolyIndex();
		if(iDegenerateConnectionPolyIndex == NAVMESH_POLY_INDEX_NONE)
		{
#if !__FINAL
			m_PolyTessellationTimer->Stop();
			m_pCurrentActivePathRequest->m_fMillisecsSpentInTessellation += (float) m_PolyTessellationTimer->GetTimeMS();
#endif
			Assertf(false, "ERROR : There was no space for new connection poly in navmesh.\n");
			return false;
		}
		Assert(iDegenerateConnectionPolyIndex < CNavMesh::ms_iNumPolysInTesselationNavMesh);

		// Set up the new poly & vertices
		TNavMeshPoly * pConnectionPoly = pTessellationNavMesh->GetPoly(iDegenerateConnectionPolyIndex);
		pConnectionPoly->SetNumVertices(3);
		pConnectionPoly->SetFirstVertexIndex(CPathServer::m_iCurrentNumTessellationPolys*3);
		CPathServer::m_iCurrentNumTessellationPolys++;

#if __TESSELLATE_USE_EXPANDED_CONNECTION_POLYS
		const Vector3 vEdgeMidpoint = (vTempVerts[lastv]*fExpandWeighting) + (vTempVerts[v]*fExpandWeighting) + (vCentroid*fOtherVertWeighting);
#else
		const Vector3 vEdgeMidpoint = (vTempVerts[lastv] + vTempVerts[v]) * 0.5f;
#endif

		// If the poly we are tessellating is already a tessellated poly itself, then record an entry in the
		// m_PolysTessellatedFrom array, which points us back to the original untessellated poly.
		if(pNavMesh == pTessellationNavMesh)
		{
			TTessInfo * pTessInfo = CPathServer::GetTessInfo(iDegenerateConnectionPolyIndex);
			TTessInfo * pParentFragmentTessInfo = CPathServer::GetTessInfo(iPolyIndex);
			pTessInfo->m_iNavMeshIndex = pParentFragmentTessInfo->m_iNavMeshIndex;
			pTessInfo->m_iPolyIndex = pParentFragmentTessInfo->m_iPolyIndex;

			pTessInfo->m_bInUse = true;
		}
		// The poly is untessellated, so set up a record in the m_PolysTessellatedFrom array.
		else
		{
			TTessInfo * pTessInfo = CPathServer::GetTessInfo(iDegenerateConnectionPolyIndex);
			pTessInfo->m_iNavMeshIndex = (u16)pNavMesh->GetIndexOfMesh();
			pTessInfo->m_iPolyIndex = (u16)iPolyIndex;
			pTessInfo->m_bInUse = true;
		}

		pPoly->CopyDataIntoTessellatedPoly(*pConnectionPoly);

		pConnectionPoly->m_TimeStamp = 0;
		pConnectionPoly->m_AStarTimeStamp = 0;
		pConnectionPoly->SetIsTessellatedFragment(true);
		pConnectionPoly->SetReplacedByTessellation(false);
		pConnectionPoly->SetIsDegenerateConnectionPoly(true);
		pConnectionPoly->SetIsSmall(true);
		pConnectionPoly->SetIsLarge(false);

		pTessellationNavMesh->GetVertexPool()[pConnectionPoly->GetFirstVertexIndex()] = vTempVerts[lastv];
		pTessellationNavMesh->GetVertexPool()[pConnectionPoly->GetFirstVertexIndex()+1] = vTempVerts[v];
		pTessellationNavMesh->GetVertexPool()[pConnectionPoly->GetFirstVertexIndex()+2] = vEdgeMidpoint;

		// Calc min/max.
		// NB: This shit won't be necessary once the connection polys are ZERO-AREA.
		// But for now they are non-degenerate so I can see the buggers.
#if __CONNECTION_POLYS_USE_PARENTS_MINMAX
		pConnectionPoly->m_MinMax = pPoly->m_MinMax;
#else
		pConnectionPoly->m_MinMax.Set(
			Min(vTempVerts[lastv].x, Min(vEdgeMidpoint.x, vTempVerts[v].x)),
			Min(vTempVerts[lastv].y, Min(vEdgeMidpoint.y, vTempVerts[v].y)),
			Min(vTempVerts[lastv].z, Min(vEdgeMidpoint.z, vTempVerts[v].z)),
			Max(vTempVerts[lastv].x, Max(vEdgeMidpoint.x, vTempVerts[v].x)),
			Max(vTempVerts[lastv].y, Max(vEdgeMidpoint.y, vTempVerts[v].y)),
			Max(vTempVerts[lastv].z, Max(vEdgeMidpoint.z, vTempVerts[v].z))
		);
#endif

		if(!CPathServerThread::OnFirstVisitingPoly(pConnectionPoly))
		{
#if !__FINAL
			m_PolyTessellationTimer->Stop();
			m_pCurrentActivePathRequest->m_fMillisecsSpentInTessellation += (float) m_PolyTessellationTimer->GetTimeMS();
#endif
			Assert(0);
			return false;
		}

		Assert(pConnectionPoly->GetNavMeshIndex() == NAVMESH_INDEX_TESSELLATION);

		//******************************************************************************
		// adjPoly is the polygon connected to this poly by this current lastv->v edge.

		const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex() + lastv);

		//*********************************************************
		// Link from the adjacent poly to this connection-poly

		const u32 iNavMesh = adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes());
		if(iNavMesh != NAVMESH_NAVMESH_INDEX_NONE &&
			adjPoly.GetPolyIndex() != NAVMESH_POLY_INDEX_NONE &&
			adjPoly.GetAdjacencyType() == ADJACENCY_TYPE_NORMAL)
		{
			// Link back the adjacency from the neighbouring poly, to this fragment
			CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(iNavMesh, domain);
			if(pAdjNavMesh)
			{
				TNavMeshPoly * pNeighbourPoly = pAdjNavMesh->GetPoly(adjPoly.GetPolyIndex());

				for(a=0; a<pNeighbourPoly->GetNumVertices(); a++)
				{
					TAdjPoly * linkBack = pAdjNavMesh->GetAdjacentPolysArray().Get( pNeighbourPoly->GetFirstVertexIndex() + a );

					if(linkBack->GetNavMeshIndex(pAdjNavMesh->GetAdjacentMeshes()) == pNavMesh->GetIndexOfMesh() && linkBack->GetPolyIndex() == iPolyIndex)
					{
						linkBack->SetNavMeshIndex(NAVMESH_INDEX_TESSELLATION, pAdjNavMesh->GetAdjacentMeshes());
						linkBack->SetPolyIndex(iDegenerateConnectionPolyIndex);
					}
				}
			}
		}

		// Link the long external edge of the connection-poly to the unchanged adjacent poly.
		// Use the link-type which originally connected this poly (eg. normal, drop-down, etc)
		TAdjPoly & externalAdjPoly = *pTessellationNavMesh->GetAdjacentPolysArray().Get(pConnectionPoly->GetFirstVertexIndex());
		adjPoly.MakeCopy(externalAdjPoly);
		externalAdjPoly.SetNavMeshIndex(iNavMesh, pTessellationNavMesh->GetAdjacentMeshes());
		externalAdjPoly.SetPolyIndex(adjPoly.GetPolyIndex());
		externalAdjPoly.SetOriginalNavMeshIndex(NAVMESH_NAVMESH_INDEX_NONE, pTessellationNavMesh->GetAdjacentMeshes());
		externalAdjPoly.SetOriginalPolyIndex(NAVMESH_POLY_INDEX_NONE);

		// Link edges 1 and 2 from the connection poly, to the two tessellation polys it adjoins.
		TAdjPoly & connectToAdj1 = *pTessellationNavMesh->GetAdjacentPolysArray().Get(pConnectionPoly->GetFirstVertexIndex() + 2);
		TAdjPoly & connectToAdj2 = *pTessellationNavMesh->GetAdjacentPolysArray().Get(pConnectionPoly->GetFirstVertexIndex() + 1);

		connectToAdj1.SetNavMeshIndex(NAVMESH_INDEX_TESSELLATION, pTessellationNavMesh->GetAdjacentMeshes());
		connectToAdj1.SetPolyIndex(iNewPolyIndices[v*2]);
		connectToAdj1.SetAdjacencyType(ADJACENCY_TYPE_NORMAL);
		connectToAdj1.SetEdgeProvidesCover(false);
		connectToAdj1.SetHighDropOverEdge(false);
		connectToAdj1.SetAdjacencyDisabled(false);

		connectToAdj2.SetNavMeshIndex(NAVMESH_INDEX_TESSELLATION, pTessellationNavMesh->GetAdjacentMeshes());
		connectToAdj2.SetPolyIndex(iNewPolyIndices[(v*2)+1]);
		connectToAdj2.SetAdjacencyType(ADJACENCY_TYPE_NORMAL);
		connectToAdj2.SetEdgeProvidesCover(false);
		connectToAdj2.SetHighDropOverEdge(false);
		connectToAdj2.SetAdjacencyDisabled(false);

		TNavMeshPoly * pConnPoly1 = pTessellationNavMesh->GetPoly(iNewPolyIndices[(v*2)+1]);
		TNavMeshPoly * pConnPoly2 = pTessellationNavMesh->GetPoly(iNewPolyIndices[(v*2)]);

		// Link the tessellation polys back to the connection polys
		TAdjPoly & tessPoly1ToConnectPoly = *pTessellationNavMesh->GetAdjacentPolysArray().Get(pConnPoly1->GetFirstVertexIndex()+1);
		tessPoly1ToConnectPoly.SetNavMeshIndex(NAVMESH_INDEX_TESSELLATION, pTessellationNavMesh->GetAdjacentMeshes());
		tessPoly1ToConnectPoly.SetPolyIndex(iDegenerateConnectionPolyIndex);
		tessPoly1ToConnectPoly.SetAdjacencyType(ADJACENCY_TYPE_NORMAL);
		tessPoly1ToConnectPoly.SetEdgeProvidesCover(false);
		tessPoly1ToConnectPoly.SetHighDropOverEdge(false);
		tessPoly1ToConnectPoly.SetAdjacencyDisabled(false);

		TAdjPoly & tessPoly2ToConnectPoly = *pTessellationNavMesh->GetAdjacentPolysArray().Get(pConnPoly2->GetFirstVertexIndex()+1);
		tessPoly2ToConnectPoly.SetNavMeshIndex(NAVMESH_INDEX_TESSELLATION, pTessellationNavMesh->GetAdjacentMeshes());
		tessPoly2ToConnectPoly.SetPolyIndex(iDegenerateConnectionPolyIndex);
		tessPoly2ToConnectPoly.SetAdjacencyType(ADJACENCY_TYPE_NORMAL);
		tessPoly2ToConnectPoly.SetEdgeProvidesCover(false);
		tessPoly2ToConnectPoly.SetHighDropOverEdge(false);
		tessPoly2ToConnectPoly.SetAdjacencyDisabled(false);

		// Identify which connection poly we will initially start on.
		if(pOutStartingPoly && iNavMesh == iReturnStartingPoly_NavMeshIndex && adjPoly.GetPolyIndex() == iReturnStartingPoly_PolyIndex)
		{
			*pOutStartingPoly = pConnectionPoly;
		}

		lastv = v;
	}

	//*****************************************************
	//	Now set up the actual tessellated polygons

	const u32 iNumTessPolys = pPoly->GetNumVertices()*2;

	lastv = iNumTessPolys-1;
	for(v=0; v<iNumTessPolys; v++)
	{
		// Set up the new poly & vertices
		TNavMeshPoly * pTessPoly = pNewPolys[v];

		pTessPoly->SetNumSpecialLinks( 0 );

		if(outputFragments && (outputFragments->GetCount() < outputFragments->GetCapacity()))
		{
			outputFragments->Append() = pTessPoly;
		}

		// Clear this flag, so we know that the poly is now in use
		// NB : Must also set flag(s) and other variables which are inherited from parent poly
		pPoly->CopyDataIntoTessellatedPoly(*pTessPoly);
		pTessellationNavMesh->SetTessellatedTriangleSizeFlags(pTessPoly);

		// This is the edge which connects to the previous tessellated poly in a tri-fan around the central vertex
		TAdjPoly & prevAdjPoly = *pTessellationNavMesh->GetAdjacentPolysArray().Get(pTessPoly->GetFirstVertexIndex());

		prevAdjPoly.SetNavMeshIndex(NAVMESH_INDEX_TESSELLATION, pTessellationNavMesh->GetAdjacentMeshes());
		prevAdjPoly.SetPolyIndex(iNewPolyIndices[lastv]);
		prevAdjPoly.SetOriginalNavMeshIndex(NAVMESH_NAVMESH_INDEX_NONE, pTessellationNavMesh->GetAdjacentMeshes());
		prevAdjPoly.SetOriginalPolyIndex(NAVMESH_POLY_INDEX_NONE);
		prevAdjPoly.SetAdjacencyType(ADJACENCY_TYPE_NORMAL);

		// This is the edge which connects to the next tessellated poly in a tri-fan around the central vertex
		TAdjPoly & nextAdjPoly = *pTessellationNavMesh->GetAdjacentPolysArray().Get(pTessPoly->GetFirstVertexIndex() + 2);

		nextAdjPoly.SetNavMeshIndex(NAVMESH_INDEX_TESSELLATION, pTessellationNavMesh->GetAdjacentMeshes());
		nextAdjPoly.SetPolyIndex(iNewPolyIndices[(v+1)%iNumTessPolys]);
		nextAdjPoly.SetOriginalNavMeshIndex(NAVMESH_NAVMESH_INDEX_NONE, pTessellationNavMesh->GetAdjacentMeshes());
		nextAdjPoly.SetOriginalPolyIndex(NAVMESH_POLY_INDEX_NONE);
		nextAdjPoly.SetAdjacencyType(ADJACENCY_TYPE_NORMAL);

		// If the poly we are tessellating is the end-poly of our pathsearch, then we need to set a new end-poly
		// by testing if the path end-pos is within each fragment.  We will test the end-pos against each edge of
		// each fragment triangle, and if on the same side of all edges then this becomes the new end poly.
		if(bThisIsStartPoly)
		{
			const Vector3 * vTestPt = &m_pCurrentActivePathRequest->m_vPathStart;
			const Vector3 & vVec0 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex()];
			const Vector3 & vVec1 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex() + 1];
			float fLineSide = ((vVec1.x - vVec0.x) * (vTestPt->y - vVec0.y) - (vTestPt->x - vVec0.x) * (vVec1.y - vVec0.y));

			if(fLineSide >= 0.0f)
			{
				Vector3 & vVec2 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex() + 2];
				fLineSide = ((vVec2.x - vVec0.x) * (vTestPt->y - vVec0.y) - (vTestPt->x - vVec0.x) * (vVec2.y - vVec0.y));
				if(fLineSide <= 0.0f)
				{
					m_Vars.m_pStartPoly = pTessPoly;
				}
			}
		}
		if(bThisIsEndPoly)
		{
			const Vector3 * vTestPt = &m_pCurrentActivePathRequest->m_vPathEnd;
			const Vector3 & vVec0 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex()];
			const Vector3 & vVec1 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex() + 1];
			float fLineSide = ((vVec1.x - vVec0.x) * (vTestPt->y - vVec0.y) - (vTestPt->x - vVec0.x) * (vVec1.y - vVec0.y));

			if(fLineSide >= 0.0f)
			{
				Vector3 & vVec2 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex() + 2];
				fLineSide = ((vVec2.x - vVec0.x) * (vTestPt->y - vVec0.y) - (vTestPt->x - vVec0.x) * (vVec2.y - vVec0.y));
				if(fLineSide <= 0.0f)
				{
					m_Vars.m_pEndPoly = pTessPoly;
				}
			}
		}
		if(iNumSpecialLinks)
		{
			for(s32 sp=0; sp<NAVMESHPOLY_MAX_SPECIAL_LINKS; sp++)
			{
				if( specialLinkInfo[sp].pLink )	// Only if we've not already retargetted this link
				{
					const Vector3 * vTestPt = &specialLinkInfo[sp].vPosition;
					const Vector3 & vVec0 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex()];
					const Vector3 & vVec1 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex() + 1];
					float fLineSide = ((vVec1.x - vVec0.x) * (vTestPt->y - vVec0.y) - (vTestPt->x - vVec0.x) * (vVec1.y - vVec0.y));

					if(fLineSide >= 0.0f)
					{
						Vector3 & vVec2 = pTessellationNavMesh->GetVertexPool()[pTessPoly->GetFirstVertexIndex() + 2];
						fLineSide = ((vVec2.x - vVec0.x) * (vTestPt->y - vVec0.y) - (vTestPt->x - vVec0.x) * (vVec2.y - vVec0.y));
						if(fLineSide <= 0.0f)
						{
							if(specialLinkInfo[sp].bRetargetFromLink)	// Retarget the 'from' link
							{
								specialLinkInfo[sp].pLink->SetLinksFromTessellationMesh( (u16)pTessellationNavMesh->GetPolyIndex(pTessPoly) );
							}
							else										// Retarget the 'to' link
							{
								specialLinkInfo[sp].pLink->SetLinksToTessellationMesh( (u16)pTessellationNavMesh->GetPolyIndex(pTessPoly) );
							}

							//pTessPoly->OrFlags(NAVMESHPOLY_HAS_SPECIAL_LINKS);	// flag tessellated poly as having special links

							pTessPoly->SetNumSpecialLinks( pTessPoly->GetNumSpecialLinks()+1 );
							pTessPoly->SetSpecialLinksStartIndex( pPoly->GetSpecialLinksStartIndex() );

							specialLinkInfo[sp].pLink = NULL;	// null the pointer so we know that we've retargetted this link
							iNumSpecialLinks--;					// dec this var, so that once all are found we no longer execute this code
						}
					}
				}
			}
		}

		lastv = v;
	}

	// Flag the old poly so that we know it has been temporarily replaced by tessellated fragments in the m_pTessellationNavMesh
	pPoly->OrFlags(NAVMESHPOLY_REPLACED_BY_TESSELLATION);

	if(pNavMesh == pTessellationNavMesh)
	{
		TTessInfo * pParentFragmentTessInfo = CPathServer::GetTessInfo(iPolyIndex);
		pParentFragmentTessInfo->m_bInUse = false;

#if __REUSE_TESSELLATION_POLYS
		CPathServer::m_iCurrentNumTessellationPolys--;
#endif
	}

#if !__FINAL
	CPathServer::ms_iNumTessellationsThisPath++;
	m_PolyTessellationTimer->Stop();
	if(m_pCurrentActivePathRequest)
		m_pCurrentActivePathRequest->m_fMillisecsSpentInTessellation += (float) m_PolyTessellationTimer->GetTimeMS();
#endif	// !__FINAL

	if(pOutStartingPoly && !(*pOutStartingPoly))
	{
		return false;
	}

	return true;
}




bool
CPathServerThread::DoesNavMeshPolyIntersectAnyDynamicObjects(CNavMesh * UNUSED_PARAM(pNavMesh), TNavMeshPoly * pPoly)
{
	return CDynamicObjectsContainer::DoesRegionIntersectAnyObjects(pPoly->m_MinMax);
}

//*************************************************************************
//	RemoveAndAddObjects
//	This function removes those objects which have been marked for deletion
//	and activates those which are newly added.  This is to ensure that we
//	don't alter the active objects DURING any path request's processing.
//	The whole system is thread-safe, because any UpdateDynamicObject() calls
//	from the main thread address the object by indexing into the array,
//	rather than going through the linked-list structure
//*************************************************************************
void
CPathServerThread::RemoveAndAddObjects(void)
{
	CPathServer::ms_bCurrentlyRemovingAndAddingObjects = true;

#ifdef GTA_ENGINE
	CHECK_FOR_THREAD_STALLS(m_DynamicObjectsCriticalSectionToken)
	sysCriticalSection dynamicObjectsCriticalSection(m_DynamicObjectsCriticalSectionToken);
#else
	EnterCriticalSection(&m_DynamicObjectsCriticalSection);
#endif

	TDynamicObject * pLast = NULL;
	TDynamicObject * pCurr = m_pFirstDynamicObject;

	TDynamicObject * pFirst = m_pFirstDynamicObject;
	s32 iInitialNumDynamic = CPathServer::m_iNumDynamicObjects;
	bool bRelinkList = false;

	while(pCurr)
	{
		// Remove those which have been flagged for deletion
		if(pCurr->m_bFlaggedForDeletion)
		{
			if(pLast)
			{
				pLast->m_pNext = pCurr->m_pNext;
			}
			else
			{
				m_pFirstDynamicObject = pCurr->m_pNext;
			}

			if(pCurr->m_pOwningGridCell)
			{
				Assert(pCurr->m_pOwningGridCell->m_pFirstDynamicObject == pCurr ||
					(pCurr->m_pPrevObjInGridCell || pCurr->m_pNextObjInGridCell));

				CDynamicObjectsContainer::RemoveObjectFromGridCell(pCurr);
			}
#if __DEV
			else
			{
				Assert(!pCurr->m_pPrevObjInGridCell && !pCurr->m_pNextObjInGridCell);
			}
#endif

			TDynamicObject * pTmp = pCurr;
			pCurr = pCurr->m_pNext;

			memset(pTmp, 0, sizeof(TDynamicObject));

			CPathServer::m_iNumDynamicObjects--;
		}
		// Activate those which have been newly added
		else
		{
			pCurr->m_bIsActive = true;
			pLast = pCurr;
			pCurr = pCurr->m_pNext;
		}

		iInitialNumDynamic--;
		if(iInitialNumDynamic < 0)
		{
			Assertf(false, "Dynamic objects list has iterated too many times - is the list cyclic?");
			Displayf("Dynamic objects list has iterated too many times - is the list cyclic?");
			bRelinkList = true;
			break;
		}
		if(pCurr == pFirst)
		{
			Assertf(false, "Dynamic objects list has become cyclic - this loop will never end");
			Displayf("Dynamic objects list has become cyclic - this loop will never end");
			bRelinkList = true;
			break;
		}
	}

	if(bRelinkList)
	{
		RelinkDynamicObjectsList();
	}

#ifndef GTA_ENGINE
	LeaveCriticalSection(&m_DynamicObjectsCriticalSection);
#endif

	CPathServer::ms_bCurrentlyRemovingAndAddingObjects = false;
}


void CPathServerThread::RelinkDynamicObjectsList()
{
	Displayf("RelinkDynamicObjectsList - emergency repairs to cyclic list.");
	Assertf(false, "RelinkDynamicObjectsList - emergency repairs to cyclic list.");

	CPathServer::m_iNumDynamicObjects = 0;

	s32 i;
	for(i=0; i<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; i++)
	{
		if(!m_DynamicObjectsStore[i].IsObjectSlotAvailable())
		{
			CPathServer::m_iNumDynamicObjects++;
		}
	}

	if(!CPathServer::m_iNumDynamicObjects)
	{
		m_pFirstDynamicObject = NULL;
		return;
	}

	m_pFirstDynamicObject = NULL;
	TDynamicObject * pLast = NULL;

	for(i=0; i<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; i++)
	{
		if(!m_DynamicObjectsStore[i].IsObjectSlotAvailable())
		{
			if(!m_pFirstDynamicObject)
			{
				m_pFirstDynamicObject = &m_DynamicObjectsStore[i];
				pLast = m_pFirstDynamicObject;
			}
			else
			{
				pLast->m_pNext = &m_DynamicObjectsStore[i];
				pLast = &m_DynamicObjectsStore[i];
			}
		}
	}

	if(pLast)
	{
		pLast->m_pNext = NULL;
	}
}



//********************************************************************************
//	UpdateAllDynamicObjectsPositions
//	This function updates all dynamic objects "m_Bounds" from the "m_NewBounds"
//	member which is set in the background from the main game thread. (Hence all
//	pathserver thread processing only uses "m_Bounds" member, to avoid syncing
//	problems.  This function also activates new objects, and removes old ones.
//********************************************************************************

void
CPathServerThread::UpdateAllDynamicObjectsPositions()
{
#ifdef GTA_ENGINE
	LOCK_DYNAMIC_OBJECTS_DATA;
#else
	EnterCriticalSection(&m_DynamicObjectsCriticalSection);
#endif

	UpdateAllDynamicObjectsPositionsNoLock();

#ifndef GTA_ENGINE
	LeaveCriticalSection(&m_DynamicObjectsCriticalSection);
#endif
}

#define __SPECIAL_CASE_IGNORE_BIKES_ON_THEIR_SIDE		1

void CPathServerThread::UpdateAllDynamicObjectsPositionsNoLock()
{
	// If no objects have been added, removed or have changed position/orientation - then we don't need
	// to do anything here.  We can save the overhead of locking the critical section, etc.
	if(CPathServer::ms_bHaveAnyDynamicObjectsChanged)
	{
		Vector3 vObjMin, vObjMax;
		CPathServer::ms_bHaveAnyDynamicObjectsChanged = false;

		TDynamicObject * pCurr = m_pFirstDynamicObject;
		while(pCurr)
		{
			// Update the bounds of any entities for whom we have new data
			if(pCurr->m_bNewBounds)
			{
				// If the main game isn't updating the new bounds, then we can copy them.
				// This is the only place that we may need to wait for the other thread
				if(pCurr->m_bCurrentlyUpdatingNewBounds)
				{
					#if __BANK && defined GTA_ENGINE
					CPathServer::m_iNumTimesCouldntUpdateObjectsDueToGameThreadAccess++;
					#endif
				}
				else
				{
					pCurr->m_bCurrentlyCopyingBounds = true;
					sys_lwsync();

					sysMemCpy(&pCurr->m_Bounds, &pCurr->m_NewBounds, sizeof(TDynObjBounds));

					sys_lwsync();
					pCurr->m_bCurrentlyCopyingBounds = false;

					pCurr->m_bNeedsReInsertingIntoGridCells = true;
					pCurr->m_bNewBounds = false;


					pCurr->CalcMinMaxForObject(vObjMin, vObjMax);

					pCurr->m_bIsOutsideWorld = (vObjMin.x < -MINMAX_MAX_FLOAT_VAL || vObjMin.y < -MINMAX_MAX_FLOAT_VAL || vObjMin.z < -MINMAX_MAX_FLOAT_VAL || vObjMax.x >= MINMAX_MAX_FLOAT_VAL || vObjMax.y >= MINMAX_MAX_FLOAT_VAL || vObjMax.z >= MINMAX_MAX_FLOAT_VAL);
				}
			}

			pCurr = pCurr->m_pNext;
		}
	}

	//*********************************************************************************************
	// Now go through all objects which have moved and reclassify them wrt which gridcells they
	// are in.  This also entails recalculating the min/max of all the objects within these cells.
	// The first step is to remove the object from the gridcell it is in, and then to add it
	// to the container again.

	static const float fSmallObjectHeight = 0.5f;

	TDynamicObject * pCurr = m_pFirstDynamicObject;
	while(pCurr)
	{
		if(pCurr->m_bIsActive)
		{
			// Work out if this object is an obstacle to us in its current orientation.
			// Some objects (doors, etc) may fall over and no longer pose an obstruction
			// due to being very thin.

#if __DEACTIVATE_FLAT_OBJECTS
			pCurr->m_bIsCurrentlyAnObstacle = (pCurr->m_Bounds.GetTopZ() - pCurr->m_Bounds.GetBottomZ()) > fSmallObjectHeight;
#else
			pCurr->m_bIsCurrentlyAnObstacle = true;
#endif

			if(pCurr->m_bIsOutsideWorld)
				pCurr->m_bIsCurrentlyAnObstacle = false;

#if __DEACTIVATE_UPROOTED_OBJECTS
			if(pCurr->m_bHasUprootLimit && DotProduct(pCurr->m_Bounds.m_vRight, ZAXIS) <= 0.5f)
			{
				pCurr->m_bIsCurrentlyAnObstacle = false;
			}
#endif

#if __SPECIAL_CASE_IGNORE_BIKES_ON_THEIR_SIDE
			if(pCurr->m_bIsMotorbike)
			{
				// For scripted routes, we consider the bike but with reduced box if it is on the side
				const float fSideThreshold = 0.5f;	// 60 degs
				if(m_pCurrentActivePathRequest && m_pCurrentActivePathRequest->m_bScriptedRoute)
				{
					pCurr->m_bForceReducedBoundingBox = (Abs(pCurr->m_Bounds.m_fUpZ) < fSideThreshold);
					pCurr->m_bIsCurrentlyAnObstacle = true;
				}
				else
				{
					pCurr->m_bForceReducedBoundingBox = false;
					pCurr->m_bIsCurrentlyAnObstacle = (Abs(pCurr->m_Bounds.m_fUpZ) >= fSideThreshold);
				}
			}
#endif

			if(pCurr->m_bInactiveDueToVelocity)
			{
				pCurr->m_bIsCurrentlyAnObstacle = false;
			}

			if(!pCurr->m_bActiveForNavigation)
			{
				pCurr->m_bIsCurrentlyAnObstacle = false;
			}

			// If this path is flagged to ignore all objects except those stuck down?
			if(m_pCurrentActivePathRequest && m_pCurrentActivePathRequest->m_bIgnoreNonSignificantObjects &&
				!pCurr->m_bIsVehicle && !pCurr->m_bHasUprootLimit && !pCurr->m_bIsSignificant)
			{
				pCurr->m_bIsCurrentlyAnObstacle = false;
			}

			if(m_pCurrentActivePathRequest && m_pCurrentActivePathRequest->m_bIgnoreTypeVehicles && pCurr->m_bIsVehicle)
			{
				pCurr->m_bIsCurrentlyAnObstacle = false;
			}
			else if(m_pCurrentActivePathRequest && m_pCurrentActivePathRequest->m_bIgnoreTypeObjects && pCurr->m_bIsObject)
			{
				pCurr->m_bIsCurrentlyAnObstacle = false;
			}
			else if(m_pCurrentActivePathRequest && m_pCurrentActivePathRequest->m_bDontAvoidFire && pCurr->m_bIsFire)
			{
				pCurr->m_bIsCurrentlyAnObstacle = false;
			}

			// Deactivate blocking objects based on type of this path request
			if(m_pCurrentActivePathRequest && pCurr->m_bIsUserAddedBlockingObject)
			{
				if(m_pCurrentActivePathRequest->m_bWander && ((pCurr->m_iBlockingObjectFlags & TDynamicObject::BLOCKINGOBJECT_WANDERPATH)==0))
					pCurr->m_bIsCurrentlyAnObstacle = false;
				else if(m_pCurrentActivePathRequest->m_bFleeTarget && ((pCurr->m_iBlockingObjectFlags & TDynamicObject::BLOCKINGOBJECT_FLEEPATH)==0))
					pCurr->m_bIsCurrentlyAnObstacle = false;
				else if((pCurr->m_iBlockingObjectFlags & TDynamicObject::BLOCKINGOBJECT_SHORTESTPATH)==0)
					pCurr->m_bIsCurrentlyAnObstacle = false;
			}

			// Enable objects specifically in include list
			if(m_pCurrentActivePathRequest && m_pCurrentActivePathRequest->m_iNumIncludeObjects)
			{
				const TNavMeshIndex iThis = CPathServer::GetPathServerThread()->GetDynamicObjectIndex(pCurr);
				for(int o=0; o<m_pCurrentActivePathRequest->m_iNumIncludeObjects; o++)
				{
					if(m_pCurrentActivePathRequest->m_IncludeObjects[o] == iThis)
					{
						pCurr->m_bIsCurrentlyAnObstacle = true;
						break;
					}
				}
			}
			// Disable objects specifically in exclude list
			if(m_pCurrentActiveRequest && m_pCurrentActiveRequest->m_iNumExcludeObjects)
			{
				const TNavMeshIndex iThis = CPathServer::GetPathServerThread()->GetDynamicObjectIndex(pCurr);
				for(int o=0; o<m_pCurrentActiveRequest->m_iNumExcludeObjects; o++)
				{
					if(m_pCurrentActiveRequest->m_ExcludeObjects[o] == iThis)
					{
						pCurr->m_bIsCurrentlyAnObstacle = false;
						break;
					}
				}
			}

			// If this needs reinserting into the grid-cells system, then do so here
			if(pCurr->m_bNeedsReInsertingIntoGridCells)
			{
				if(!pCurr->m_pOwningGridCell)
				{
					CDynamicObjectsContainer::AddObjectToGridCell(pCurr);

					pCurr->m_pOwningGridCell->m_bMinMaxOfObjectsNeedsRecalculating = true;
				}
				else
				{
					pCurr->m_pOwningGridCell->m_bMinMaxOfObjectsNeedsRecalculating = true;

					CDynamicObjectsContainer::RemoveObjectFromGridCell(pCurr);
					CDynamicObjectsContainer::AddObjectToGridCell(pCurr);

					pCurr->m_pOwningGridCell->m_bMinMaxOfObjectsNeedsRecalculating = true;
				}

				pCurr->m_bNeedsReInsertingIntoGridCells = false;
			}
		}

		pCurr = pCurr->m_pNext;
	}

	CDynamicObjectsContainer::RecalculateExtentsOfAllMarkedGrids();
}


//****************************************************************************
//	IsModelIndexConsideredAnObstacle
//	This function compares the model-index with the list of indices of
//	object types which we consider in the pathfinding as dynamic objects.
//****************************************************************************
#ifndef GTA_ENGINE
bool CPathServer::IsModelIndexConsideredAnObstacle(u32 UNUSED_PARAM(iModelIndex)) { return true; }
#else	// GTA_ENGINE

bool
CPathServer::IsModelIndexConsideredAnObstacle(u32 iModelIndex)
{
	CBaseModelInfo * pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(iModelIndex)));

	// If NOT not-avoided-by-peds, then it is considered an obstacle
	return !(pModelInfo->GetNotAvoidedByPeds());
}

#endif	// GTA_ENGINE


//******************************************************************************************
//	MaybeAddDynamicObject
//	CWorld calls this function when objects are added to the world
//	Not all objects & entities are added, only those which pose significant obstructions.
//******************************************************************************************

bool CPathServerGta::MaybeAddDynamicObject(CEntity * pEntity)
{
	if(!ShouldAvoidObject(pEntity))
		return false;

#ifdef GTA_ENGINE
	CBaseModelInfo * pModelInfo = pEntity->GetBaseModelInfo();
#else	// GTA_ENGINE
	CBaseModelInfo * pModelInfo = NULL;
#endif	// GTA_ENGINE

	return AddDynamicObject(pEntity, pModelInfo);
}

//******************************************************************************************
//	MaybeRemoveDynamicObject
//	CWorld calls this function when objects are removed from the world
//******************************************************************************************
void CPathServerGta::MaybeRemoveDynamicObject(CEntity * pEntity)
{
	TDynamicObjectIndex iDynObjIndex;

	if(pEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		iDynObjIndex = ((CObject*)pEntity)->GetPathServerDynamicObjectIndex();
		((CObject*)pEntity)->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE);
	}
	else if(pEntity->GetType() == ENTITY_TYPE_VEHICLE)
	{
		iDynObjIndex = ((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex();
		((CVehicle*)pEntity)->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE);
	}
	else
	{
		return;
	}

	// If iDynObjIndex is a valid index (and is not DYNAMIC_OBJECT_INDEX_NONE) then proceed
	if(iDynObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		FlagDynamicObjectForRemoval(iDynObjIndex);

#ifdef GTA_ENGINE
		if(pEntity->GetType() == ENTITY_TYPE_VEHICLE)
		{
			CVehicle * pVehicle = (CVehicle*)pEntity;
			for(s32 d=0; d<pVehicle->GetNumDoors(); d++)
			{
				CCarDoor * pDoor = pVehicle->GetDoor(d);
				if(pDoor)
				{
					const TDynamicObjectIndex index = pDoor->GetPathServerDynamicObjectIndex();
					if( index < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS )
					{
						FlagDynamicObjectForRemoval(pDoor->GetPathServerDynamicObjectIndex());
						pDoor->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE);
					}
				}
			}
		}
#endif
	}
}

#ifdef GTA_ENGINE
bool CPathServerGta::RemoveFireObject(const CFire * pFire)
{
	if(pFire->GetPathServerObjectIndex()<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		return FlagDynamicObjectForRemoval(pFire->GetPathServerObjectIndex());
	}
	return false;
}
bool CPathServerGta::RemoveVehicleDoorObject(const CCarDoor * pDoor)
{
	if(pDoor->GetPathServerDynamicObjectIndex()<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		return FlagDynamicObjectForRemoval(pDoor->GetPathServerDynamicObjectIndex());
	}
	return false;
}
#endif

float GetHeading(const Matrix34 & mat)
{
	return Selectf(Abs(mat.b.z) - 1.0f + VERY_SMALL_FLOAT, rage::Atan2f(mat.c.x,mat.a.x), rage::Atan2f(-mat.b.x, mat.b.y));
}
float GetRoll(const Matrix34 & mat)
{
	return Selectf(Abs(mat.b.z) - 1.0f + VERY_SMALL_FLOAT, 0.0f, rage::Atan2f(-mat.a.z, mat.c.z));
}
float GetPitch(const Matrix34 & mat)
{
	return AsinfSafe(mat.b.z);
}

#ifdef GTA_ENGINE

//--------------------------------------------------------------------------------------------------
// NAME : ProcessAddDeferredDynamicObjects
// PURPOSE : Process the deferred adding of scripted blocking objects to the pathfinder.
// This is done at a safe point in the frame when we can ensure there are no path requests ongoing,
// because it may be necessary to evict less important objects to add critical mission ones - and
// this cannot be done whilst a path request is in progress.

void CPathServer::ProcessAddDeferredDynamicObjects()
{
	LOCK_NAVMESH_DATA;
	LOCK_DYNAMIC_OBJECTS_DATA;

	for(s32 i=0; i<m_iNumDeferredAddDynamicObjects; i++)
	{
		CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

		TScriptDeferredAddDynamicObject & deferredObj = m_ScriptDeferredAddDynamicObjects[i];

		// Object handle may have been set to -1 if object was removed before it could be added here
		if(deferredObj.m_iObjectHandle == -1)
			continue;

		// Find a free slot to store this object.
		s32 iSlot;
		for(iSlot=0; iSlot<MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS; iSlot++)
		{
			if(m_ScriptedDynamicObjects[iSlot].m_iScriptObjectHandle == -1)
				break;
		}
		Assertf(iSlot != MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS, "Scripts have added too many dynamic objects (%i), try cutting down number - are they all required?", MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS);
		if(iSlot == MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS)
		{
			break;
		}

#if __ASSERT
		// In dev builds, we iterate through the objects array to make sure we don't have an identical scripted object
		// This is just a precaution in case designers are careless about adding objects multiple times.
		for(s32 o=0; o<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; o++)
		{
			TDynamicObject & obj = m_PathServerThread.m_DynamicObjectsStore[o];
			if(!obj.IsObjectSlotAvailable() && obj.m_iScriptThreadId != 0)
			{
				const Vector3 vDiff = obj.m_Bounds.GetOrigin() - deferredObj.m_vPosition;
				if(vDiff.Mag() <= SMALL_FLOAT)
				{
					char existingThreadName[256];
					scrThread * pExistingThread = scrThread::GetThread(static_cast<scrThreadId>(obj.m_iScriptThreadId));
					if(pExistingThread)
						strcpy(existingThreadName, pExistingThread->GetScriptName());
					else
						strcpy(existingThreadName, "(unknown script - maybe already quit)");

					char tmp[512];
					sprintf(tmp,
						"ADD_NAVMESH_BLOCKING_OBJECT - a scripted blocking object already exists with exact same coordinates.  Is this a duplicate?\nThe script which added the original instance was called \"%s\", the script now trying to add a duplicate object is called \"%s\".\nThe position of this object is (%.2f, %.2f, %.2f).",
						existingThreadName, CTheScripts::GetCurrentScriptNameAndProgramCounter(), deferredObj.m_vPosition.x, deferredObj.m_vPosition.y, deferredObj.m_vPosition.z);
				}
			}
		}
#endif

		TDynamicObjectIndex index = AddBlockingObject(deferredObj.m_vPosition, deferredObj.m_vSize, deferredObj.m_fHeading, deferredObj.m_iBlockingFlags);
		Assert(index != DYNAMIC_OBJECT_INDEX_NONE);

		if(index == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
		{
			ASSERT_ONLY(s32 iEvictIndex = ) EvictDynamicObjectForScript();
			
			Assertf(iEvictIndex != -1, "EvictDynamicObjectForScript failed");

			index = AddBlockingObject(deferredObj.m_vPosition, deferredObj.m_vSize, deferredObj.m_fHeading, deferredObj.m_iBlockingFlags);
			
			Assert(index != DYNAMIC_OBJECT_INDEX_NONE);

			if(index >= DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
			{
				Assertf(false, "AddScriptedBlockingObject() - was still unable to add dynamic object, even after evicting one for script.");
			}
			else
			{
				m_iNumScriptBlockingObjects++;
			}
		}

		if(index != DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD && index != DYNAMIC_OBJECT_INDEX_NONE)
		{
			TDynamicObject & obj = m_PathServerThread.m_DynamicObjectsStore[index];

			Assert(obj.m_iScriptThreadId == 0xffffffff);
			obj.m_iScriptThreadId = deferredObj.m_iThreadId;
			obj.m_bScriptedBlockingObject = true;
			obj.m_bHasUprootLimit = true;

			m_ScriptedDynamicObjects[iSlot].m_iScriptObjectHandle = deferredObj.m_iObjectHandle;
			m_ScriptedDynamicObjects[iSlot].m_iObjectIndex = index;
		}
	}

	m_iNumDeferredAddDynamicObjects = 0;
}

s32 CPathServerGta::AddScriptedBlockingObject(const scrThreadId iThreadId, const Vector3 & vPosition, const Vector3 & vSize, const float fHeading, const s32 iBlockingFlags)
{
	Assertf(m_iNumDeferredAddDynamicObjects < TScriptDeferredAddDynamicObject::ms_iMaxNum, "No more space for deferred add of scripted blocking object.  Max %i reached.  Please change your script to add more objects on the next frame,", TScriptDeferredAddDynamicObject::ms_iMaxNum);

#if __ASSERT
	// Assert that we will have a free slot for this object
	s32 iNumFree=0;
	for(s32 s=0; s<MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS; s++)
	{
		if(m_ScriptedDynamicObjects[s].m_iScriptObjectHandle == -1)
			iNumFree++;
	}
	iNumFree -= m_iNumDeferredAddDynamicObjects;
	Assertf(iNumFree > 0, "Scripts have added too many dynamic objects (%i), try cutting down number - are they all required?", MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS);
#endif

	if( m_iNumDeferredAddDynamicObjects < TScriptDeferredAddDynamicObject::ms_iMaxNum )
	{
		m_ScriptDeferredAddDynamicObjects[m_iNumDeferredAddDynamicObjects].m_iObjectHandle = m_iNextScriptObjectHandle;
		m_ScriptDeferredAddDynamicObjects[m_iNumDeferredAddDynamicObjects].m_iThreadId = iThreadId;
		m_ScriptDeferredAddDynamicObjects[m_iNumDeferredAddDynamicObjects].m_vPosition = vPosition;
		m_ScriptDeferredAddDynamicObjects[m_iNumDeferredAddDynamicObjects].m_vSize = vSize;
		m_ScriptDeferredAddDynamicObjects[m_iNumDeferredAddDynamicObjects].m_fHeading = fHeading;
		m_ScriptDeferredAddDynamicObjects[m_iNumDeferredAddDynamicObjects].m_iBlockingFlags = iBlockingFlags;

		const s32 iObjHandle = m_ScriptDeferredAddDynamicObjects[m_iNumDeferredAddDynamicObjects].m_iObjectHandle;
		
		m_iNumDeferredAddDynamicObjects++;

		m_iNextScriptObjectHandle++;
		if(m_iNextScriptObjectHandle == INT_MAX)
			m_iNextScriptObjectHandle = 0;

		return iObjHandle;
	}
	return -1;
}

bool CPathServerGta::UpdateScriptedBlockingObject(const scrThreadId ASSERT_ONLY(iThreadId), const s32 iObjHandle, const Vector3 & vPosition, const Vector3 & vSize, const float fHeading, const s32 iBlockingFlags)
{
	s32 s;
	// Now go through our list which pairs scripted object handles, with the object index
	for(s=0; s<MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS; s++)
	{
		if(m_ScriptedDynamicObjects[s].m_iScriptObjectHandle == iObjHandle)
		{
			const s32 iObjIndex = m_ScriptedDynamicObjects[s].m_iObjectIndex;

			if( iObjIndex >= 0 && iObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS )
			{
				TDynamicObject & obj = m_PathServerThread.m_DynamicObjectsStore[iObjIndex];
				Assertf( obj.m_bIsUserAddedBlockingObject && obj.m_bScriptedBlockingObject, "Object is not a user/scripted dynamic object");
				if( obj.m_bIsUserAddedBlockingObject && obj.m_bScriptedBlockingObject )
				{
					Assertf( obj.m_iScriptThreadId != 0, "Object has no script thread ID; it was probably already removed.");
					Assertf(static_cast<u32>(iThreadId) == obj.m_iScriptThreadId || obj.m_iScriptThreadId==0xffffffff, "A different script is attempting to update blocking object %i to the one which created it", iObjHandle);

					obj.m_iBlockingObjectFlags = iBlockingFlags;
					UpdateBlockingObject((TDynamicObjectIndex)iObjIndex, vPosition, vSize, fHeading, false);

					return true;
				}
			}
		}
	}

	return false;
}

bool CPathServerGta::DoesScriptedBlockingObjectExist(const scrThreadId UNUSED_PARAM(iThreadId), const s32 iObjHandle)
{
	Assert( GetIsValidPathServerDynamicObjectIndex(TDynamicObjectIndex(iObjHandle)) );

	s32 s;
	// Now go through our list which pairs scripted object handles, with the object index
	for(s=0; s<MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS; s++)
	{
		if(m_ScriptedDynamicObjects[s].m_iScriptObjectHandle == iObjHandle)
		{
			return true;
		}
	}
	return false;
}

bool CPathServerGta::RemoveScriptedBlockingObject(const scrThreadId iThreadId, const s32 iObjHandle)
{
	s32 s;

	bool bRemoved = false;

	// Firstly remove any occurance of this which is still in the deferred add list;
	// just in case the script terminated/removed before the object could even be added!
	for(s=0; s<m_iNumDeferredAddDynamicObjects; s++)
	{
		if(m_ScriptDeferredAddDynamicObjects[s].m_iObjectHandle == iObjHandle)
		{
			m_ScriptDeferredAddDynamicObjects[s].m_iObjectHandle = -1;
			bRemoved = true;
			break;
		}
	}

	// Now go through our list which pairs scripted object handles, with the object index
	for(s=0; s<MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS; s++)
	{
		if(m_ScriptedDynamicObjects[s].m_iScriptObjectHandle == iObjHandle)
		{
			const s32 iObjIndex = m_ScriptedDynamicObjects[s].m_iObjectIndex;

			m_ScriptedDynamicObjects[s].m_iScriptObjectHandle = -1;
			m_ScriptedDynamicObjects[s].m_iObjectIndex = DYNAMIC_OBJECT_INDEX_NONE;

			if( iObjIndex >= 0 && iObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS )
			{
				TDynamicObject & obj = m_PathServerThread.m_DynamicObjectsStore[iObjIndex];

				Assertf( obj.m_bIsUserAddedBlockingObject && obj.m_bScriptedBlockingObject, "Object is not a user/scripted dynamic object");
				if( obj.m_bIsUserAddedBlockingObject && obj.m_bScriptedBlockingObject )
				{
					Assertf( obj.m_iScriptThreadId != 0, "Object has no script thread ID; it was probably already removed.");

					// Remove if the object's script thread index matches that passed in, OR it is the dummy 0xffffffff value
					if( obj.m_iScriptThreadId == (u32)iThreadId || obj.m_iScriptThreadId == 0xffffffff )
					{
						FlagDynamicObjectForRemoval( (TDynamicObjectIndex)iObjIndex);
						bRemoved = true;

						m_iNumScriptBlockingObjects--;
						break;
					}
				}
			}
		}
	}

#if __ASSERT
	scrThread * pScrThread = scrThread::GetThread(iThreadId);
	const char * pScriptName = pScrThread ? pScrThread->GetScriptName() : "unknown";
	Assertf(bRemoved, "Script \"%s\" is trying to remove dynamic object %i, which is not found.", pScriptName, iObjHandle);
#endif

	return bRemoved;
}

void CPathServerGta::RemoveAllBlockingObjectsForScript(const scrThreadId iThreadId)
{
	Assert(iThreadId != 0);

	s32 s;

	// First remove any which may still be waiting to be added
	for(s=0; s<m_iNumDeferredAddDynamicObjects; s++)
	{
		if(m_ScriptDeferredAddDynamicObjects[s].m_iThreadId == static_cast<u32>(iThreadId))
		{
			m_ScriptDeferredAddDynamicObjects[s].m_iObjectHandle = -1;
		}
	}

	// Then remove any which are added
	for(s32 o=0; o<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; o++)
	{
		TDynamicObject & obj = m_PathServerThread.m_DynamicObjectsStore[o];
		if( obj.m_bIsUserAddedBlockingObject && obj.m_bScriptedBlockingObject && obj.m_iScriptThreadId == (u32)iThreadId )
		{
			for(s=0; s<MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS; s++)
			{
				if(m_ScriptedDynamicObjects[s].m_iObjectIndex == o)
				{
					// TODO: Ensure that this is the same script ID as added it
					RemoveScriptedBlockingObject( iThreadId, m_ScriptedDynamicObjects[s].m_iScriptObjectHandle );
				}
			}
		}
	}
}

s32 CPathServer::EvictDynamicObjectForScript()
{
	// Make a note that this has been called
	Displayf("CPathServer::EvictDynamicObjectForScript()..");

	const float fMaxDist = 500.0f;
	float fMinScore = FLT_MAX;
	s32 iMinIndex = -1;
	s32 o;
	Vector3 vMin, vMax, vSize;

	// Iterate through array of dynamic objects, looking for a suitable one to remove.
	// We will only choose one which is the bounds for a CObject.
	// Choose the object with the smallest bounding box.
	for(o=0; o<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; o++)
	{
		TDynamicObject & obj = m_PathServerThread.m_DynamicObjectsStore[o];

		// Don't consider certain types for removal
		if(obj.m_bIsUserAddedBlockingObject || obj.m_bScriptedBlockingObject)
			continue;
		// Don't replace objects which are already in line for removal
		if(obj.m_bFlaggedForDeletion)
			continue;

		// Only obstacles which are representing a CObject type, or a CFire
		if(obj.m_bIsObject || obj.m_bIsFire)
		{
			// Rate the object on size
			obj.m_MinMax.GetAsFloats(vMin, vMax);
			vSize = vMax-vMin;
			const float fVol = vSize.x * vSize.y * vSize.z;
			const float fSizeScore = (fVol * fVol);

			// Rate the object on distance; those closer to the navmesh origin score higher -
			// we are looking to remove the object with the lowest score.
			const Vector3 vDelta = m_vOrigin - obj.m_Bounds.GetOrigin();
			float fDistanceScore = Max( 0.0f, fMaxDist - vDelta.Mag() );
			fDistanceScore = (fDistanceScore * fDistanceScore) * (obj.m_bHasUprootLimit ? 1.0f : 0.5f);

			float fScore = fSizeScore + fDistanceScore;

			// Heavily penalize the removal of objects with any uproot limit,
			// we only want to remove one of these as a very last resort.
			// Applying a large penalty essentially splits regular/uproot-limit
			// objects into two bands, except in the case of very large objects
			if(obj.m_bIsSignificant && !obj.m_bIsFire)
				fScore += 100000.0f;
			if(obj.m_bHasUprootLimit)
				fScore += 2000000.0f;

			if(fScore < fMinScore)
			{
				fMinScore = fScore;
				iMinIndex = o;
			}
		}
	}

	// Not found any space? Return now..
	if(iMinIndex == -1)
		return -1;

	// Fix up the linked list of dynamic objects, since we are about to remove one
	TDynamicObject * pChosenObj = &m_PathServerThread.m_DynamicObjectsStore[iMinIndex];

	// IF WE SELECTED AN OBJECT WHICH IS ALREADY FLAGGED FOR DELETION, THEN ITS VERY LIKELY
	// THAT ITS ENTITY POINTER WILL BE POINTING AT AN *ALREADY-DELETED* ENTITY (0XDDDDDDDD)
	if(!pChosenObj->m_bFlaggedForDeletion)
	{
		Assert(pChosenObj->m_bIsObject || pChosenObj->m_bIsFire);

		if(pChosenObj->m_bIsObject)
		{
#if __ASSERT
			Assert(pChosenObj->m_pEntity);
			if(pChosenObj->m_pEntity)
				Assertf(((CEntity*)pChosenObj->m_pEntity)->GetIsTypeObject(), "entity type is %i, but should be ENTITY_TYPE_OBJECT", ((CEntity*)pChosenObj->m_pEntity)->GetType());
#endif

			// Ensure that the entity owning this dynamic object is no longer associated with it.
			// TODO: We could probably set this to 'DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD' and thereby have the object
			// add itself to the pathfinder again once space became available.
			if(pChosenObj->m_pEntity)
				((CObject*)(pChosenObj->m_pEntity))->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE);
		}
		else if(pChosenObj->m_bIsFire)
		{
			g_fireMan.UnregisterPathserverID((TDynamicObjectIndex)iMinIndex);
		}
	}

	pChosenObj->m_pEntity = NULL;

	for(o=0; o<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; o++)
	{
		TDynamicObject * pObj = &m_PathServerThread.m_DynamicObjectsStore[o];
		if(pObj != pChosenObj)
		{
			if(pObj->m_pNext == pChosenObj)
			{
				pObj->m_pNext = pChosenObj->m_pNext;
				break;
			}
		}
	}

	// If we're removing the head of our linked list, we need to set the head element to be the next
	if(m_PathServerThread.m_pFirstDynamicObject == pChosenObj)
	{
		m_PathServerThread.m_pFirstDynamicObject = m_PathServerThread.m_pFirstDynamicObject->m_pNext;
	}

	// Remove item from doubly-linked list
	if(pChosenObj->m_pPrevObjInGridCell)
		pChosenObj->m_pPrevObjInGridCell->m_pNextObjInGridCell = pChosenObj->m_pNextObjInGridCell;
	if(pChosenObj->m_pNextObjInGridCell)
		pChosenObj->m_pNextObjInGridCell->m_pPrevObjInGridCell = pChosenObj->m_pPrevObjInGridCell;
	// Replace list head pointer in grid cell, if its this object
	if(pChosenObj->m_pOwningGridCell && pChosenObj->m_pOwningGridCell->m_pFirstDynamicObject == pChosenObj)
		pChosenObj->m_pOwningGridCell->m_pFirstDynamicObject = pChosenObj->m_pNextObjInGridCell;

	pChosenObj->m_pPrevObjInGridCell = NULL;
	pChosenObj->m_pNextObjInGridCell = NULL;
	pChosenObj->m_pOwningGridCell = NULL;

	pChosenObj->m_pNext = NULL;
	pChosenObj->m_pEntity = NULL;

	m_iNumDynamicObjects--;
	m_iNumDynamicObjects = MAX(m_iNumDynamicObjects, 0);

	return iMinIndex;
}

#ifdef GTA_ENGINE
// NAME : AddBlockingObject
// PURPOSE : Adds a user-defined blocking object to the pathfinder
// Flags are a combination of the TDynamicObject::BlockingObjectFlags enumeration, and define for which types of path the object is active

TDynamicObjectIndex CPathServer::AddBlockingObject(const Vector3 & vPosition, const Vector3 & vSize, const float fHeading, const s32 iBlockingFlags)
{
	if(m_iNumDynamicObjects == PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		return DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD;
	}

	Assertf((iBlockingFlags & TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES)!=0, "Blocking object has to respond to some variety of path request");

	CHECK_FOR_THREAD_STALLS(m_DynamicObjectsCriticalSectionToken);
	sysCriticalSection dynamicObjectsCriticalSection(m_DynamicObjectsCriticalSectionToken);

	// Find a free slot
	u32 s;
	for(s=0; s<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; s++)
	{
		if(m_PathServerThread.m_DynamicObjectsStore[s].IsObjectSlotAvailable())
			break;
	}

	// Couldn't find one ?
	if(s == PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		return DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD;
	}

	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;
	TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[s];
	dynObj.SetFlagsForNewObject();

	TDynObjBoundsFuncs::CalcBoundsForBlockingObject(dynObj.m_NewBounds, vPosition, vSize, fHeading, &dynObj);

	Matrix34 rotMat;
	rotMat.Identity();
	rotMat.RotateZ(fHeading);

	dynObj.m_NewBounds.m_fHeading = GetHeading(rotMat);
	dynObj.m_NewBounds.m_fRoll = GetRoll(rotMat);
	dynObj.m_NewBounds.m_fPitch = GetPitch(rotMat);
	dynObj.m_NewBounds.m_fUpZ = rotMat.c.z;

	sysMemCpy(&dynObj.m_Bounds, &dynObj.m_NewBounds, sizeof(TDynObjBounds));

	dynObj.CalcMinMaxForObject();
	dynObj.m_bIsUserAddedBlockingObject = true;
	dynObj.m_iBlockingObjectFlags = iBlockingFlags;
	dynObj.m_iScriptThreadId = 0xffffffff;

	dynObj.m_pNext = m_PathServerThread.m_pFirstDynamicObject;
	m_PathServerThread.m_pFirstDynamicObject = &m_PathServerThread.m_DynamicObjectsStore[s];
	m_iNumDynamicObjects++;

	return (TDynamicObjectIndex) s;
}
#endif	// GTA_ENGINE

bool CPathServer::RemoveBlockingObject(const TDynamicObjectIndex iObjIndex)
{
	if(iObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		return FlagDynamicObjectForRemoval(iObjIndex);
	}
	return false;
}

//******************************************************************************************
//	UpdateBlockingObject
//	Forces the update of a script/code added dynamic object
//
//	NB: If the only thing which has changed is the size, and bForceUpdate is not specified,
//	then the object will not be updated.
//******************************************************************************************

void CPathServer::UpdateBlockingObject(const TDynamicObjectIndex iObjIndex, const Vector3 & vPosition, const Vector3 & vSize, const float fHeading, bool bForceUpdate, u32 * pBlockingFlags)
{
	Assert(iObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);
	if(iObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[iObjIndex];

#if __ASSERT
		if(!dynObj.m_bIsUserAddedBlockingObject)
		{
			Printf("------------------------------------------\n");
			Printf("dynObj.m_bFlaggedForDeletion: %i\n", dynObj.m_bFlaggedForDeletion ? 1 : 0);
			Printf("dynObj.m_pEntity: 0x%p\n", dynObj.m_pEntity);
			Printf("dynObj.m_bIsActive: %i\n", dynObj.m_bIsActive ? 1 : 0);
			Printf("dynObj.m_bIsObject: %i\n", dynObj.m_bIsObject ? 1 : 0);
			Printf("dynObj.m_bIsVehicle: %i\n", dynObj.m_bIsVehicle ? 1 : 0);
			Printf("dynObj.m_bVehicleDoor: %i\n", dynObj.m_bVehicleDoor ? 1 : 0);
			Printf("dynObj.m_bIsFire: %i\n", dynObj.m_bIsFire ? 1 : 0);
			Printf("dynObj.m_bScriptedBlockingObject: %i\n", dynObj.m_bScriptedBlockingObject ? 1 : 0);
			Printf("dynObj.m_iScriptThreadId: %u\n", dynObj.m_iScriptThreadId);
			Printf("dynObj.m_pOwningGridCell: 0x%p\n", dynObj.m_pOwningGridCell);

			Assertf(0, "Call to UpdateBlockingObject() with wrong obj index  - please add TTY to bug as it contains some info");
		}
#endif

		// Update optional blocking flags
		if(pBlockingFlags)
			dynObj.m_iBlockingObjectFlags = *pBlockingFlags;


		if(dynObj.m_bCurrentlyCopyingBounds)
		{
			// If we are explicitly forcing an update, then we will want to keep trying until it is effective.
			if(bForceUpdate)
			{
				while(dynObj.m_bCurrentlyCopyingBounds)
				{
					sysIpcYield(1);
				}
			}
			// Otherwise we can return now, and the object will be updated later.
			else
			{
				return;
			}
		}

		const TDynObjBounds & objBounds = dynObj.m_bNewBounds ? dynObj.m_NewBounds : dynObj.m_Bounds;

		if( !bForceUpdate )
		{
			static const float fAngEps = 4.0f * DtoR;
			if( IsClose( SubtractAngleShorter(objBounds.m_fHeading, fHeading), 0.0f, fAngEps ) && vPosition.IsClose(objBounds.GetEntityOrigin(), 0.1f) )
			{
				return;
			} 
		}

		CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

		// Mark that we're updating this object
		dynObj.m_bCurrentlyUpdatingNewBounds = true;
		sys_lwsync();

		TDynObjBoundsFuncs::CalcBoundsForBlockingObject(dynObj.m_NewBounds, vPosition, vSize, fHeading, &dynObj);

		dynObj.m_bNewBounds = true;

		sys_lwsync();
		dynObj.m_bCurrentlyUpdatingNewBounds = false;
	}
}

#endif


//******************************************************************************************
//	UpdateDynamicObject
//	this function is called whenever a dynamic object has moved or rotated.
//******************************************************************************************
void CPathServerGta::UpdateDynamicObject(CEntity * pEntity, bool bForceUpdate, bool bForceActivate)
{
	TDynamicObjectIndex iDynObjIndex;
#ifdef GTA_ENGINE
	bool deactivateDoors = false;
	bool nowInactive;
#endif
	if(pEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		iDynObjIndex = ((CObject*)pEntity)->GetPathServerDynamicObjectIndex();

#ifdef GTA_ENGINE
		TDynamicObject & obj = m_PathServerThread.m_DynamicObjectsStore[iDynObjIndex];

		// If the object has fragmented, then this is set from within the CBreakable class
		if(((CObject*)pEntity)->m_nObjectFlags.bBoundsNeedRecalculatingForNavigation)
			bForceUpdate = true;
		// If the object has been uprooted
		// Only if not a door.
		if(obj.m_bHasUprootLimit && !obj.m_bIsDoor &&
			((CObject*)pEntity)->m_nObjectFlags.bHasBeenUprooted)
		{
			obj.m_bHasUprootLimit = false;
			CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;
		}

		nowInactive = ((CObject*)pEntity)->GetVelocity().Mag2() > (ms_fDynamicObjectVelocityThreshold*ms_fDynamicObjectVelocityThreshold) && !bForceActivate;
		obj.m_bInactiveDueToVelocity = nowInactive;
#endif
	}
	else if(pEntity->GetType() == ENTITY_TYPE_VEHICLE)
	{
		iDynObjIndex = ((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex();

#ifdef GTA_ENGINE
		TDynamicObject& dynObj = m_PathServerThread.m_DynamicObjectsStore[iDynObjIndex];
		bool wasInactive = dynObj.m_bInactiveDueToVelocity;

		// Trains never deactivate due to velocity (url:bugstar:1039298)
		if(((CVehicle*)pEntity)->InheritsFromTrain())
		{
			nowInactive = false;
		}
		else
		{
			nowInactive = ((CVehicle*)pEntity)->GetVelocity().Mag2() > (ms_fDynamicObjectVelocityThreshold*ms_fDynamicObjectVelocityThreshold) && !bForceActivate;
		}

		dynObj.m_bInactiveDueToVelocity = nowInactive;
		dynObj.m_bActiveForNavigation = ((CVehicle*)pEntity)->GetActiveForPedNavitation();

		// If we should be inactive now and we weren't before, we may have to deactivate the doors.
		if(nowInactive && !wasInactive)
		{
			deactivateDoors = true;
		}
#endif
	}
	else
	{
		Assert(0);
		return;
	}

	Assert(iDynObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);

#ifdef GTA_ENGINE
	// Early out if this entity is traveling to fast
	if(nowInactive)
	{
		// If this is a vehicle we must also mark its doors as inactive!
		if(deactivateDoors)
		{
			Assert(pEntity->GetIsTypeVehicle());
			const s32 iNumDoors = ((CVehicle*)pEntity)->GetNumDoors();
			for(s32 d=0; d<iNumDoors; d++)
			{
				const TDynamicObjectIndex iDoorObjIndex = ((CVehicle*)pEntity)->GetDoor(d)->GetPathServerDynamicObjectIndex();
				if(iDoorObjIndex!=DYNAMIC_OBJECT_INDEX_NONE && iDoorObjIndex!=DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
					m_PathServerThread.m_DynamicObjectsStore[iDoorObjIndex].m_bInactiveDueToVelocity = true;
			}
		}
		return;
	}
#endif

	//**************************************************************************************
	//	Check whether this object has actually changed position/orientation significantly.
	//	I think we can probably eventually avoid this test, because there is a mechanism
	//	within the world/physics/netcode/etc which will already check for this..
	//**************************************************************************************

	TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[iDynObjIndex];

	// If it just happens that at this moment, the pathserver thread is copying over the
	// m_NewBounds data into the m_Bounds data - then don't bother to update this object.
	// NB : There is still a very small chance that the pathserver may read the data whilst
	// the game-thread is updating it, we'll have to keep an eye out for instances of dodgy
	// bounds.  Hopefully the fact that they will not change significantly from frame to
	// frame will reduce any problems.  I think this is better than forcing a kernel-level
	// synchronisation here, but maybe a spinlock would be a good idea here?
	if(dynObj.m_bCurrentlyCopyingBounds)
	{
		// If we are explicitly forcing an update, then we will want to keep trying until it is effective.
		if(bForceUpdate)
		{
			while(dynObj.m_bCurrentlyCopyingBounds)
			{
				sysIpcYield(1);
			}
		}
		// Otherwise we can return now, and the object will be updated later.
		else
		{
			return;
		}
	}

	// Check the entiy's orientation & position against the ones stored in the dynamic object.
	// If this object has new bound data ready which has not been copied from m_NewBounds to
	// m_Bounds, then check this.
	const TDynObjBounds & objBounds = dynObj.m_bNewBounds ? dynObj.m_NewBounds : dynObj.m_Bounds;

#ifdef NAVGEN_TOOL
	Vector3 vDiff = pEntity->m_vPosition - objBounds.GetEntityOrigin();
	const Matrix34 mat = pEntity->GetMatrix();
#else
	Vector3 vDiff = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - objBounds.GetEntityOrigin();
	const Matrix34 mat = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
#endif

	// Same position ?
	if(!bForceUpdate && (vDiff.Mag2() < 0.1f))
	{
		const float fRotThreshold = dynObj.m_bIsDoor ? (2.0f * DtoR) : (25.8f * DtoR);

		if( Abs(SubtractAngleShorter( objBounds.m_fHeading, GetHeading(mat) ) ) < fRotThreshold )
		{
			if( Abs(SubtractAngleShorter( objBounds.m_fPitch, GetPitch(mat) ) ) < fRotThreshold )
			{
				if( Abs(SubtractAngleShorter( objBounds.m_fRoll, GetRoll(mat) ) ) < fRotThreshold )
				{
					return;
				}
			}
		}
	}

	//********************************************************************************************************
	//	Calculate the new bounds, etc for this object.
	//	Store them in the "TDynamicObject::m_NewBounds" structure, and set the "TDynamicObject::m_bNewBounds"
	//	flag - so that the next time we come across the object, we know to copy in the new values.
	//********************************************************************************************************

	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

	// Mark that we're updating this object
	dynObj.m_bCurrentlyUpdatingNewBounds = true;
	sys_lwsync();

	TDynObjBoundsFuncs::CalcBoundsForEntity(dynObj.m_NewBounds, pEntity, &dynObj);

	dynObj.m_NewBounds.m_fHeading = GetHeading(mat);
	dynObj.m_NewBounds.m_fRoll = GetRoll(mat);
	dynObj.m_NewBounds.m_fPitch = GetPitch(mat);
	dynObj.m_NewBounds.m_fUpZ = mat.c.z;

	dynObj.m_bNewBounds = true;

	sys_lwsync();
	dynObj.m_bCurrentlyUpdatingNewBounds = false;

#ifdef GTA_ENGINE
	if(pEntity->GetIsTypeObject())
	{
		CBaseModelInfo * pModelInfo = pEntity->GetBaseModelInfo();
		const bool bIsFixed = pEntity->IsBaseFlagSet(fwEntity::IS_FIXED) || pModelInfo->GetIsFixed();

		if(((CObject*)pEntity)->IsADoor())
		{
			CDoor * pDoor = (CDoor*)pEntity;
			// Garage doors are a special case, because we can't just walk into them to open them.
			// We'll treat them as immovable (non-uprootable) objects, same as locked doors
			const bool bGargageDoor = (pDoor->GetDoorType()==CDoor::DOOR_TYPE_GARAGE || pDoor->GetDoorType()==CDoor::DOOR_TYPE_GARAGE_SC);
			const bool bOnlyVehicles =
				pDoor->GetTuning() &&
					((pDoor->GetTuning()->m_Flags.IsFlagSet(CDoorTuning::AutoOpensForMPVehicleWithPedsOnly) && CNetwork::IsGameInProgress()) ||
						(pDoor->GetTuning()->m_Flags.IsFlagSet(CDoorTuning::AutoOpensForSPVehicleWithPedsOnly) && !CNetwork::IsGameInProgress()));
			const bool bOnlyPlayer = 
				pDoor->GetTuning() &&
				((pDoor->GetTuning()->m_Flags.IsFlagSet(CDoorTuning::AutoOpensForMPPlayerPedsOnly) && CNetwork::IsGameInProgress()) ||
				(pDoor->GetTuning()->m_Flags.IsFlagSet(CDoorTuning::AutoOpensForSPPlayerPedsOnly) && !CNetwork::IsGameInProgress()));

			// The openable state may have changed
			dynObj.m_bIsOpenable =
				(pModelInfo->GetUsesDoorPhysics() != 0) &&
					CDoor::DoorTypeOpensForPedsWhenNotLocked(pDoor->GetDoorType()) &&
						(!bIsFixed) &&
							(bGargageDoor==false) &&
								(bOnlyVehicles==false) &&
									(bOnlyPlayer == false);	// TODO: allow player peds to pathfind through when 'bOnlyPlayer' is true

			// 'm_bHasUprootLimit' doubles-up as a way of distinguishing locked doors
			dynObj.m_bHasUprootLimit = (bIsFixed || bGargageDoor || bOnlyVehicles);
		}
		else
		{
			fragInst * pFragInst = pEntity->GetFragInst();
			dynObj.m_bHasUprootLimit = (pFragInst!=NULL && (pFragInst->GetTypePhysics()->GetMinMoveForce() > 0.0f) && !((CObject*)pEntity)->m_nObjectFlags.bHasBeenUprooted);
			dynObj.m_bIsOpenable = false;
		}

		// Always reset this flag
		((CObject*)pEntity)->m_nObjectFlags.bBoundsNeedRecalculatingForNavigation = false;
	}
	else if(pEntity->GetIsTypeVehicle())
	{
		// If the vehicle was updated, then force the doors to be updated also
		UpdateVehicleDoorsDynamicObjects((CVehicle*)pEntity, true);
	}
#endif
}

//**************************************************************************
//	GetDoorComponents
//	This function obtains the components of the pVehicle's frag inst which
//	make up its doors.  There will be a maximum of MAX_DOOR_COMPONENTS for
//	each door, and 4 doors will be queried for - making a max of 8 indices.
//	The actual num found will be returned.

#ifdef GTA_ENGINE
int TDynObjBoundsFuncs::GetDoorComponents(const CVehicle * pVehicle, int * iComponents, const s32 iMaxNum)
{
	int iNumComponents = 0;
	for(int d=0; d<pVehicle->GetNumDoors(); d++)
	{
		const CCarDoor * pDoor = pVehicle->GetDoor(d);
		if(pDoor->GetIsIntact(pVehicle) && !pDoor->GetIsClosed())
		{
			iNumComponents += CCarEnterExit::GetDoorComponents(pVehicle, pDoor->GetHierarchyId(), &iComponents[iNumComponents], iMaxNum-iNumComponents);
		}
	}
	return iNumComponents;
}

int TDynObjBoundsFuncs::GetHeliIgnoreComponents(const CVehicle * pVehicle, int * iComponents, const s32 iMaxNum)
{
	int iNumComponents = 0;
	if (pVehicle->InheritsFromHeli())
	{
		const int iGroupIndex = pVehicle->GetFragInst()->GetGroupFromBoneIndex(pVehicle->GetBoneIndex(HELI_ROTOR_MAIN));
		if (iGroupIndex!=-1)
		{
			const fragTypeGroup * pGroup = pVehicle->GetFragInst()->GetTypePhysics()->GetAllGroups()[iGroupIndex];
			const int iFirstChildIndex = pGroup->GetChildFragmentIndex();
			const int iNumChildren = pGroup->GetNumChildren();

			for (int iChild = iFirstChildIndex; iChild < iFirstChildIndex+iNumChildren; iChild++)
			{
				iComponents[iNumComponents++] = iChild;
				if(iNumComponents >= iMaxNum)
					break;
			}
				
		}

		if(iNumComponents < iMaxNum)
			iComponents[iNumComponents++] = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex(HELI_ROTOR_REAR));
		if(iNumComponents < iMaxNum)
			iComponents[iNumComponents++] = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex(VEH_CHASSIS_DUMMY));
	}

	return iNumComponents;
}
s32 TDynObjBoundsFuncs::GetIgnoreComponentsForEntity(const CEntity * pEntity, s32 * pOut_ComponentsArray, const s32 iMaxNum)
{
	int iNumIgnoreComponents = fwPathServer::GetExcludeVehicleDoorsFromNavigation() && pEntity->GetIsTypeVehicle() ? GetDoorComponents((CVehicle*)pEntity, pOut_ComponentsArray, iMaxNum) : 0;
	iNumIgnoreComponents += fwPathServer::GetExcludeVehicleDoorsFromNavigation() && pEntity->GetIsTypeVehicle() ? GetHeliIgnoreComponents((CVehicle*)pEntity, &pOut_ComponentsArray[iNumIgnoreComponents], iMaxNum-iNumIgnoreComponents) : 0;
	return iNumIgnoreComponents;
}
#endif

void TDynObjBoundsFuncs::CalcBoundsForEntity(TDynObjBounds& bnds, CEntity * pEntity, TDynamicObject * pDynObj)
{
	// Note that the entity's origin may well not be at its centre
#ifdef NAVGEN_TOOL
	bnds.SetEntityOrigin(pEntity->m_vPosition);
#else
	bnds.SetEntityOrigin(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
#endif

	const float fExpandRadius = PATHSERVER_PED_RADIUS;

#ifdef GTA_ENGINE

	// Special case hacks
	float fPedHalfHeight = CEntityBoundAI::ms_fPedHalfHeight;
	if(MI_RAILWAY_BARRIER_01.IsValid() && (pEntity->GetModelIndex() == MI_RAILWAY_BARRIER_01.GetModelIndex()))
		fPedHalfHeight = 4.0f;

	// Components to ignore
	int iIgnoreComponents[32];	// Must also fit the helicopter components
	int iNumIgnoreComponents = GetIgnoreComponentsForEntity(pEntity, iIgnoreComponents, 32);
	Assertf(iNumIgnoreComponents <= 32, "This is very bad, memory overwrite!");

	CEntityBoundAI bound(*pEntity, bnds.GetEntityOrigin().z, fExpandRadius, true, NULL, iNumIgnoreComponents, iIgnoreComponents, 0, NULL, fPedHalfHeight);
#else
	CEntityBoundAI bound(*pEntity, bnds.GetEntityOrigin().z, fExpandRadius, true);
#endif

	bound.GetPlanes(bnds.m_vEdgePlaneNormals);

	Vector3 vCorners[4];
	bound.GetCorners(vCorners);

	bnds.SetOrigin( (vCorners[0] + vCorners[1] + vCorners[2] + vCorners[3]) * 0.25f);

	// JB : Now using the CEntityBoundAI's radius, since vehicles don't seem to have a radius set anymore.
	spdSphere sphere;
	bound.GetSphere(sphere);

	pDynObj->m_vVertices[0].x = vCorners[0].x;
	pDynObj->m_vVertices[0].y = vCorners[0].y;
	pDynObj->m_vVertices[1].x = vCorners[1].x;
	pDynObj->m_vVertices[1].y = vCorners[1].y;
	pDynObj->m_vVertices[2].x = vCorners[2].x;
	pDynObj->m_vVertices[2].y = vCorners[2].y;
	pDynObj->m_vVertices[3].x = vCorners[3].x;
	pDynObj->m_vVertices[3].y = vCorners[3].y;

#ifdef GTA_ENGINE
	// NB : New code in CEntityBoundAI now does a better job of finding the top/bottom Z vals for an entity
	// But we will still increase the bottom Z value, to make sure objects correctly sit into the ground.
	// This is most noticable for vehicles whose wheels lift their collision off the ground, we need to
	// pull down their bottom z a bit more than for most objects.
	const float fPushThroughGroundAmt = pEntity->GetIsTypeVehicle() ? 0.5f : 0.25f;
	bnds.SetTopZ( bound.GetTopZ() - (fExpandRadius-0.1f) );
	bnds.SetBottomZ( bound.GetBottomZ() - fPushThroughGroundAmt );
#else
	bnds.SetTopZ( bnds.GetOrigin().z + 2.0f );
	bnds.SetBottomZ( bnds.GetOrigin().z - 2.0f );
#endif

	// Construct the top & bottom planes for this object, using world UP & DOWN as normal vectors
	// TODO : No need to store the whole plane eq's for top & bottom planes (at all?)
	bnds.m_vEdgePlaneNormals[4] = Vector3(0.0f, 0.0f, 1.0f);
	bnds.m_vEdgePlaneNormals[4].w = - bnds.m_vEdgePlaneNormals[4].Dot3( Vector3(bnds.GetOrigin().x, bnds.GetOrigin().y, bnds.GetTopZ() ) );
	bnds.m_vEdgePlaneNormals[5] = Vector3(0.0f, 0.0f, -1.0f);
	bnds.m_vEdgePlaneNormals[5].w = - bnds.m_vEdgePlaneNormals[5].Dot3( Vector3(bnds.GetOrigin().x, bnds.GetOrigin().y, bnds.GetBottomZ()) );
}


#ifdef GTA_ENGINE
bool TDynObjBoundsFuncs::CalcBoundsForVehicleDoor(TDynObjBounds& bnds, CCarDoor * pDoor, CVehicle * pParent, const Matrix34 & doorMat, TDynamicObject * pDynObj)
{
	Assert(pDoor->GetIsIntact(pParent));
	Assert((!pDoor->GetIsClosed()) || pDoor->GetForceOpenForNavigation());

	bnds.SetEntityOrigin(doorMat.d);

	// Use reduced expansion radius if this door is being forced open for navigation.
	// This is a hack, but should help peds fleeing from vehicles who otherwise might get
	// trapped between front/rear doors which are all opened at the same time for peds to flee.
	const float fExpandRadius = pDoor->GetForceOpenForNavigation() ? 0.0f : PATHSERVER_PED_RADIUS;
	Vector3 vBoundBoxMin, vBoundBoxMax;

	if(!pDoor->GetLocalBoundingBox(pParent, vBoundBoxMin, vBoundBoxMax))
	{
		Assert(false);
		return false;
	}

	Vector3 vTinnerDoors(fExpandRadius * 0.2f, fExpandRadius * 1.2f, fExpandRadius);
	CEntityBoundAI bound(vBoundBoxMin, vBoundBoxMax, doorMat, bnds.GetEntityOrigin().z, vTinnerDoors, true);

	bound.GetPlanes(bnds.m_vEdgePlaneNormals);

	Vector3 vCorners[4];
	bound.GetCorners(vCorners);
	bnds.SetOrigin((vCorners[0] + vCorners[1] + vCorners[2] + vCorners[3]) * 0.25f);

	spdSphere sphere;
	bound.GetSphere(sphere);
//	bnds.m_fBoundingRadius = sphere.GetRadiusf();

	pDynObj->m_vVertices[0].x = vCorners[0].x;
	pDynObj->m_vVertices[0].y = vCorners[0].y;
	pDynObj->m_vVertices[1].x = vCorners[1].x;
	pDynObj->m_vVertices[1].y = vCorners[1].y;
	pDynObj->m_vVertices[2].x = vCorners[2].x;
	pDynObj->m_vVertices[2].y = vCorners[2].y;
	pDynObj->m_vVertices[3].x = vCorners[3].x;
	pDynObj->m_vVertices[3].y = vCorners[3].y;

#ifdef GTA_ENGINE
	// NB : New code in CEntityBoundAI now does a better job of finding the top/bottom Z vals for an entity
	// But we will still increase the bottom Z value, to make sure objects correctly sit into the ground.
	// This is most noticable for vehicles whose wheels lift their collision off the ground, we need to
	// pull down their bottom z a bit more than for most objects.
	static const float fPushThroughGroundAmt = 0.25f;
	bnds.SetTopZ( bound.GetTopZ() );
	bnds.SetBottomZ( bound.GetBottomZ() - fPushThroughGroundAmt );
#else
	bnds.SetTopZ( bnds.m_vOrigin.z + 2.0f );
	bnds.SetBottomZ( bnds.m_vOrigin.z - 2.0f );
#endif

	// Construct the top & bottom planes for this object, using world UP & DOWN as normal vectors
	// TODO : No need to store the whole plane eq's for top & bottom planes (at all?)
	bnds.m_vEdgePlaneNormals[4] = Vector3(0.0f, 0.0f, 1.0f);
	bnds.m_vEdgePlaneNormals[4].w = - bnds.m_vEdgePlaneNormals[4].Dot3(Vector3(bnds.GetOrigin().x, bnds.GetOrigin().y, bnds.GetTopZ() ));
	bnds.m_vEdgePlaneNormals[5] = Vector3(0.0f, 0.0f, -1.0f);
	bnds.m_vEdgePlaneNormals[5].w = - bnds.m_vEdgePlaneNormals[5].Dot3(Vector3(bnds.GetOrigin().x, bnds.GetOrigin().y, bnds.GetBottomZ() ));

	return true;
}
#endif // GTA_ENGINE

#ifdef GTA_ENGINE
void TDynObjBoundsFuncs::CalcBoundsForFire(TDynObjBounds& bnds, CFire * pFire, TDynamicObject * pDynObj)
{
	Vector3 vPos;
	if (pFire->GetEntity())
	{
		vPos = VEC3V_TO_VECTOR3(pFire->GetEntity()->GetTransform().GetPosition());
	}
	else
	{
		Vec3V vFirePos = pFire->GetPositionWorld();
		vPos = RCC_VECTOR3(vFirePos);
	}

	bnds.SetEntityOrigin(vPos);
	bnds.SetOrigin(vPos);
	bnds.SetTopZ( vPos.z + 1.5f );
	bnds.SetBottomZ( vPos.z - 1.0f );
	const float fRadius = pFire->GetMaxRadius();	// 0.5f;

	Vector3 vCorners[4] =
	{
		Vector3(-fRadius, fRadius, 0.0f),
		Vector3(-fRadius, -fRadius, 0.0f),
		Vector3(fRadius, -fRadius, 0.0f),
		Vector3(fRadius, fRadius, 0.0f)
	};

	bnds.m_vEdgePlaneNormals[0] = Vector3(0.0f, 1.0f, 0.0f);
	bnds.m_vEdgePlaneNormals[1] = Vector3(-1.0f, 0.0f, 0.0f);
	bnds.m_vEdgePlaneNormals[2] = Vector3(0.0f, -1.0f, 0.0f);
	bnds.m_vEdgePlaneNormals[3] = Vector3(1.0f, 0.0f, 0.0f);
	bnds.m_vEdgePlaneNormals[4] = Vector3(0.0f, 0.0f, 1.0f);
	bnds.m_vEdgePlaneNormals[5] = Vector3(0.0f, 0.0f, -1.0f);

	bnds.m_fHeading = fwRandom::GetRandomNumberInRange(-PI, PI);
	bnds.m_fPitch = 0.0f;

	Matrix34 rotMat;
	rotMat.MakeRotateZ(bnds.m_fHeading);
	rotMat.d.Zero();

	for(s32 p=0; p<4; p++)
	{
		rotMat.Transform3x3(vCorners[p]);

		Vector3 vNormal = bnds.m_vEdgePlaneNormals[p].GetVector3();
		rotMat.Transform3x3(vNormal);
		bnds.m_vEdgePlaneNormals[p].SetVector3(vNormal);

		vCorners[p] += vPos;
	}

	pDynObj->m_vVertices[0].x = vCorners[0].x;
	pDynObj->m_vVertices[0].y = vCorners[0].y;
	pDynObj->m_vVertices[1].x = vCorners[1].x;
	pDynObj->m_vVertices[1].y = vCorners[1].y;
	pDynObj->m_vVertices[2].x = vCorners[2].x;
	pDynObj->m_vVertices[2].y = vCorners[2].y;
	pDynObj->m_vVertices[3].x = vCorners[3].x;
	pDynObj->m_vVertices[3].y = vCorners[3].y;

	bnds.m_vEdgePlaneNormals[0].w = - bnds.m_vEdgePlaneNormals[0].Dot3(vCorners[0]);
	bnds.m_vEdgePlaneNormals[1].w = - bnds.m_vEdgePlaneNormals[1].Dot3(vCorners[1]);
	bnds.m_vEdgePlaneNormals[2].w = - bnds.m_vEdgePlaneNormals[2].Dot3(vCorners[2]);
	bnds.m_vEdgePlaneNormals[3].w = - bnds.m_vEdgePlaneNormals[3].Dot3(vCorners[3]);
	bnds.m_vEdgePlaneNormals[4].w = - bnds.m_vEdgePlaneNormals[4].Dot3(Vector3(bnds.GetOrigin().x, bnds.GetOrigin().y, bnds.GetTopZ() ));
	bnds.m_vEdgePlaneNormals[5].w = - bnds.m_vEdgePlaneNormals[5].Dot3(Vector3(bnds.GetOrigin().x, bnds.GetOrigin().y, bnds.GetBottomZ() ));
}
#endif


#ifdef GTA_ENGINE
void TDynObjBoundsFuncs::CalcBoundsForBlockingObject(TDynObjBounds& bnds, const Vector3 & vPos, const Vector3 & vInputSize, const float fRotation, TDynamicObject * pDynObj)
{
	Vector3 vSize = vInputSize;
	// Expand by standard ped radius
	vSize.x += PATHSERVER_PED_RADIUS * 2.0f;
	vSize.y += PATHSERVER_PED_RADIUS * 2.0f;

	Vector3 vHalfSize = vSize * 0.5f;

	bnds.SetEntityOrigin(vPos);
	bnds.SetOrigin(vPos);
	bnds.SetTopZ( vPos.z + vSize.z );
	bnds.SetBottomZ( vPos.z - 1.0f );
	//bnds.m_fBoundingRadius = vHalfSize.Mag();

	Vector3 vCorners[4] =
	{
		Vector3(-vHalfSize.x, vHalfSize.y, 0.0f),
		Vector3(-vHalfSize.x, -vHalfSize.y, 0.0f),
		Vector3(vHalfSize.x, -vHalfSize.y, 0.0f),
		Vector3(vHalfSize.x, vHalfSize.y, 0.0f)
	};

	bnds.m_vEdgePlaneNormals[0] = Vector3(0.0f, 1.0f, 0.0f);
	bnds.m_vEdgePlaneNormals[1] = Vector3(-1.0f, 0.0f, 0.0f);
	bnds.m_vEdgePlaneNormals[2] = Vector3(0.0f, -1.0f, 0.0f);
	bnds.m_vEdgePlaneNormals[3] = Vector3(1.0f, 0.0f, 0.0f);
	bnds.m_vEdgePlaneNormals[4] = Vector3(0.0f, 0.0, 1.0f);
	bnds.m_vEdgePlaneNormals[5] = Vector3(0.0f, 0.0, -1.0f);

	Matrix34 rotMat;
	rotMat.MakeRotateZ(fRotation);
	rotMat.d.Zero();

	bnds.m_fHeading = fRotation;
	bnds.m_fPitch = 0.0f;

	for(s32 p=0; p<4; p++)
	{
		rotMat.Transform3x3(vCorners[p]);
		
		Vector3 vNormal = bnds.m_vEdgePlaneNormals[p].GetVector3();
		rotMat.Transform3x3(vNormal);
		bnds.m_vEdgePlaneNormals[p].SetVector3(vNormal);

		vCorners[p] += vPos;
	}

	pDynObj->m_vVertices[0].x = vCorners[0].x;
	pDynObj->m_vVertices[0].y = vCorners[0].y;
	pDynObj->m_vVertices[1].x = vCorners[1].x;
	pDynObj->m_vVertices[1].y = vCorners[1].y;
	pDynObj->m_vVertices[2].x = vCorners[2].x;
	pDynObj->m_vVertices[2].y = vCorners[2].y;
	pDynObj->m_vVertices[3].x = vCorners[3].x;
	pDynObj->m_vVertices[3].y = vCorners[3].y;

	bnds.m_vEdgePlaneNormals[0].w = - bnds.m_vEdgePlaneNormals[0].Dot3(vCorners[0]);
	bnds.m_vEdgePlaneNormals[1].w = - bnds.m_vEdgePlaneNormals[1].Dot3(vCorners[1]);
	bnds.m_vEdgePlaneNormals[2].w = - bnds.m_vEdgePlaneNormals[2].Dot3(vCorners[2]);
	bnds.m_vEdgePlaneNormals[3].w = - bnds.m_vEdgePlaneNormals[3].Dot3(vCorners[3]);
	bnds.m_vEdgePlaneNormals[4].w = - bnds.m_vEdgePlaneNormals[4].Dot3(Vector3(bnds.GetOrigin().x, bnds.GetOrigin().y, bnds.GetTopZ() ));
	bnds.m_vEdgePlaneNormals[5].w = - bnds.m_vEdgePlaneNormals[5].Dot3(Vector3(bnds.GetOrigin().x, bnds.GetOrigin().y, bnds.GetBottomZ()));
}
#endif


#ifdef GTA_ENGINE
void CPathServerGta::UpdateFireObject(CFire * pFire)
{
	Assert(pFire->GetPathServerObjectIndex()!=DYNAMIC_OBJECT_INDEX_NONE);
	TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[pFire->GetPathServerObjectIndex()];

	if(dynObj.m_bCurrentlyCopyingBounds)
		return;

#if __ASSERT && AI_OPTIMISATIONS_OFF
	const intptr_t iEntityPtr = reinterpret_cast<intptr_t>(dynObj.m_pEntity);
	if(iEntityPtr != 0xFFFFFFFF)
	{
		if(iEntityPtr==0)
		{
			Assertf(false, "CPathServer::UpdateFireObject : expected m_pEntity to be 0xFFFFFFFF, but it was NULL");
		}
		else
		{
			Assertf(false, "CPathServer::UpdateFireObject : expected m_pEntity to be 0xFFFFFFFF, but it was %p (entity type==%i)", dynObj.m_pEntity, dynObj.m_pEntity->GetType() );
		}
	}
#endif

	// Check the entiy's orientation & position against the ones stored in the dynamic object.
	// If this object has new bound data ready which has not been copied from m_NewBounds to
	// m_Bounds, then check this.
	Vector3 vPos;
	if (pFire->GetEntity())
	{
		vPos = VEC3V_TO_VECTOR3(pFire->GetEntity()->GetTransform().GetPosition());
	}
	else
	{
		Vec3V vFirePos = pFire->GetPositionWorld();
		vPos = RCC_VECTOR3(vFirePos);
	}

	const TDynObjBounds & objBounds = dynObj.m_bNewBounds ? dynObj.m_NewBounds : dynObj.m_Bounds;
	const Vector3 vDiff = vPos - objBounds.GetEntityOrigin();

	// Same position ?
	if(vDiff.Mag2() < 0.1f)
		return;

	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

	// Mark that we're updating this object
	dynObj.m_bCurrentlyUpdatingNewBounds = true;
	sys_lwsync();

	TDynObjBoundsFuncs::CalcBoundsForFire(dynObj.m_NewBounds, pFire, &dynObj);

	dynObj.m_bNewBounds = true;

	sys_lwsync();
	dynObj.m_bCurrentlyUpdatingNewBounds = false;
}
#endif


#ifdef GTA_ENGINE

bool CPathServerGta::AddFireObject(CFire * pFire)
{
	//static const float fMaxPosition = MINMAX_MAX_FLOAT_VAL;
	// const Vector3 vPos = VEC3V_TO_VECTOR3(pFire->GetPositionWorld());

	if(m_iNumDynamicObjects == PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
		return false;

	CHECK_FOR_THREAD_STALLS(m_DynamicObjectsCriticalSectionToken);
	sysCriticalSection dynamicObjectsCriticalSection(m_DynamicObjectsCriticalSectionToken);

	// Find a free slot
	u32 s;
	for(s=0; s<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; s++)
	{
		if(m_PathServerThread.m_DynamicObjectsStore[s].IsObjectSlotAvailable())
			break;
	}

	// Couldn't find one ?
	if(s == PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		return false;
	}

	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;
	TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[s];
	dynObj.SetFlagsForNewObject();

	TDynObjBoundsFuncs::CalcBoundsForFire(dynObj.m_NewBounds, pFire, &dynObj);

	Matrix34 mat = MAT34V_TO_MATRIX34(pFire->GetMatrixOffset());
	dynObj.m_NewBounds.m_fHeading = GetHeading(mat);
	dynObj.m_NewBounds.m_fRoll = GetRoll(mat);
	dynObj.m_NewBounds.m_fPitch = GetPitch(mat);
	dynObj.m_NewBounds.m_fUpZ = mat.c.z;

	sysMemCpy(&dynObj.m_Bounds, &dynObj.m_NewBounds, sizeof(TDynObjBounds));

	dynObj.CalcMinMaxForObject();
	dynObj.m_iScriptThreadId = 0xffffffff;
	dynObj.m_bIsFire = true;
	dynObj.m_bIsSignificant = true;

	dynObj.m_pNext = m_PathServerThread.m_pFirstDynamicObject;
	m_PathServerThread.m_pFirstDynamicObject = &m_PathServerThread.m_DynamicObjectsStore[s];
	m_iNumDynamicObjects++;

	pFire->SetPathServerObjectIndex((TDynamicObjectIndex)s);

	return true;
}
#endif

//******************************************************************************************
//	AddDynamicObject
//	Adds a dynamic object to the pathfinding system.  This function may stall until the
//	dynamic object data is free to use.
//******************************************************************************************
bool CPathServerGta::AddDynamicObject(CEntity * pEntity, CBaseModelInfo * UNUSED_PARAM(pModelInfo))
{
	//*************************************************************************************
	// Check whether the object is within the bounds of the map, and don't add if outside

	const float fMaxPosition = MINMAX_MAX_FLOAT_VAL;
	bool bInsideWorld;

#ifdef NAVGEN_TOOL
	const Vector3 vPos = pEntity->m_vPosition;
#else
	const Vector3 vPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
#endif

	if(vPos.x < -fMaxPosition || vPos.x > fMaxPosition ||
		vPos.y < -fMaxPosition || vPos.y > fMaxPosition ||
		vPos.z < -fMaxPosition || vPos.z > fMaxPosition)
	{
		bInsideWorld = false;
	}
	else
	{
		bInsideWorld = true;
	}

	//*********************************************************************************************
	//	Can't add the object if we are already at max capacity, or the object is outside the world.
	//	NEW : Or if the pathfinder is already busy with the dynamic objects.
#ifndef GTA_ENGINE
	EnterCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
	const bool bObjectsInUse = false;
#else
	const bool bObjectsInUse = (m_DynamicObjectsCriticalSectionToken.TryLock()==false);
#endif


	if(m_iNumDynamicObjects == PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS || !bInsideWorld || bObjectsInUse)
	{
		if(pEntity->GetType() == ENTITY_TYPE_OBJECT)
		{
			Assert(((CObject*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CObject*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
			((CObject*)pEntity)->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		}
		else if(pEntity->GetType() == ENTITY_TYPE_VEHICLE)
		{
			Assert(((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
			((CVehicle*)pEntity)->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		}
		else
		{
			Assert(0);
		}

#ifdef GTA_ENGINE
		if(!bObjectsInUse)
			m_DynamicObjectsCriticalSectionToken.Unlock();
#endif

		return false;
	}

#if !__FINAL
	if(CPathServer::m_bDisplayTimeTakenToAddDynamicObjects)
		START_PERF_TIMER(m_MiscTimer);
#endif

	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

	u32 s;

	// Find a free slot
	for(s=0; s<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; s++)
	{
		if(m_PathServerThread.m_DynamicObjectsStore[s].IsObjectSlotAvailable())
			break;
	}
	// Couldn't find one ?
	if(s == PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		if(pEntity->GetType() == ENTITY_TYPE_OBJECT)
		{
			Assert(((CObject*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CObject*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
			((CObject*)pEntity)->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		}
		else if(pEntity->GetType() == ENTITY_TYPE_VEHICLE)
		{
			Assert(((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
			((CVehicle*)pEntity)->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		}
		else
		{
			Assert(0);
		}

#if !__FINAL
		if(CPathServer::m_bDisplayTimeTakenToAddDynamicObjects)
		{
			float fTimeTakenToAdd;
			STOP_PERF_TIMER(m_MiscTimer, fTimeTakenToAdd);
			printf("Dynamic object not added - time taken : %.3f\n", fTimeTakenToAdd);
		}
#endif

#ifndef GTA_ENGINE
		LeaveCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
#else
		m_DynamicObjectsCriticalSectionToken.Unlock();
#endif
		return false;
	}

#ifdef NAVGEN_TOOL
	const Matrix34 mat = pEntity->GetMatrix();
#else
	const Matrix34 mat = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
#endif

	TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[s];
	dynObj.SetFlagsForNewObject();

	TDynObjBoundsFuncs::CalcBoundsForEntity(dynObj.m_NewBounds, pEntity, &dynObj);

	dynObj.m_NewBounds.m_fHeading = GetHeading(mat);
	dynObj.m_NewBounds.m_fRoll = GetRoll(mat);
	dynObj.m_NewBounds.m_fPitch = GetPitch(mat);
	dynObj.m_NewBounds.m_fUpZ = mat.c.z;

	sysMemCpy(&dynObj.m_Bounds, &dynObj.m_NewBounds, sizeof(TDynObjBounds));

	dynObj.CalcMinMaxForObject();
	dynObj.m_bIsFire = false;
	dynObj.m_bIsUserAddedBlockingObject = false;

#ifdef GTA_ENGINE
	CBaseModelInfo * pModelInfo = pEntity->GetBaseModelInfo();
	const bool bIsFixed = pEntity->IsBaseFlagSet(fwEntity::IS_FIXED) || pModelInfo->GetIsFixed();
#else
	dynObj.m_bIsOpenable = false;
#endif

#ifdef GTA_ENGINE
	dynObj.m_pEntity = pEntity;
#else
	dynObj.m_pEntity = (fwEntity*)pEntity;	// Hack for NavGenTest.
#endif

	dynObj.m_pNext = m_PathServerThread.m_pFirstDynamicObject;
	m_PathServerThread.m_pFirstDynamicObject = &m_PathServerThread.m_DynamicObjectsStore[s];
	m_iNumDynamicObjects++;

	// Set a flag inside the object, to notify that it's being avoided by the path-server.
	// This'll prevent potential collision events from being generated for it, etc.

	if(pEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		Assert(((CObject*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CObject*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		Assert(s < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);

		((CObject*)pEntity)->SetPathServerDynamicObjectIndex((TDynamicObjectIndex) s);

		dynObj.m_bIsPushable = ((CObject*)pEntity)->GetMass() < CPathServerThread::ms_fMaxMassOfPushableObject;

		dynObj.m_bIsObject = true;

#ifdef GTA_ENGINE

		dynObj.m_bInactiveDueToVelocity = ((CObject*)pEntity)->GetVelocity().Mag2() > (ms_fDynamicObjectVelocityThreshold*ms_fDynamicObjectVelocityThreshold);

		if(((CObject*)pEntity)->IsADoor())
		{
			CDoor * pDoor = (CDoor*)pEntity;
			const bool bGargageDoor = (pDoor->GetDoorType()==CDoor::DOOR_TYPE_GARAGE || pDoor->GetDoorType()==CDoor::DOOR_TYPE_GARAGE_SC);
			const bool bOnlyVehicles =
				pDoor->GetTuning() &&
					((pDoor->GetTuning()->m_Flags.IsFlagSet(CDoorTuning::AutoOpensForMPVehicleWithPedsOnly) && CNetwork::IsGameInProgress()) ||
						(pDoor->GetTuning()->m_Flags.IsFlagSet(CDoorTuning::AutoOpensForSPVehicleWithPedsOnly) && !CNetwork::IsGameInProgress()));
			const bool bOnlyPlayer = 
				pDoor->GetTuning() &&
					((pDoor->GetTuning()->m_Flags.IsFlagSet(CDoorTuning::AutoOpensForMPPlayerPedsOnly) && CNetwork::IsGameInProgress()) ||
						(pDoor->GetTuning()->m_Flags.IsFlagSet(CDoorTuning::AutoOpensForSPPlayerPedsOnly) && !CNetwork::IsGameInProgress()));

			// The openable state may have changed
			dynObj.m_bIsOpenable =
				(pModelInfo->GetUsesDoorPhysics() != 0) &&
					CDoor::DoorTypeOpensForPedsWhenNotLocked(pDoor->GetDoorType()) &&
						(!bIsFixed) &&
							(bGargageDoor==false) &&
								(bOnlyVehicles==false) &&
									(bOnlyPlayer == false);	// TODO: allow player peds to pathfind through when 'bOnlyPlayer' is true

			// 'm_bHasUprootLimit' doubles-up as a way of distinguishing locked doors
			dynObj.m_bHasUprootLimit = (bIsFixed || bGargageDoor || bOnlyVehicles);
			dynObj.m_bIsDoor = true;
		}
		else
		{
			fragInst * pFragInst = pEntity->GetFragInst();
			dynObj.m_bHasUprootLimit = (pFragInst!=NULL && (pFragInst->GetTypePhysics()->GetMinMoveForce() > 0.0f) && !((CObject*)pEntity)->m_nObjectFlags.bHasBeenUprooted);

			// treat breakable glass as an uprootable object, until broken
			if(((CObject*)pEntity)->m_nDEflags.bIsBreakableGlass)
			{
				dynObj.m_bHasUprootLimit = true;
				dynObj.m_bIsBreakableGlass = true;
			}

			dynObj.m_bIsSignificant = IsObjectSignificant(pEntity);
		}
#endif
	}
	else if(pEntity->GetType() == ENTITY_TYPE_VEHICLE)
	{
		Assert(((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CVehicle*)pEntity)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		Assert(s < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);

		((CVehicle*)pEntity)->SetPathServerDynamicObjectIndex((TDynamicObjectIndex) s);

		dynObj.m_bIsPushable = ((CVehicle*)pEntity)->GetMass() < CPathServerThread::ms_fMaxMassOfPushableObject;
		dynObj.m_bIsVehicle = true;
		dynObj.m_bIsMotorbike = (((CVehicle*)pEntity)->GetVehicleType()==VEHICLE_TYPE_BIKE);
		dynObj.m_bHasUprootLimit = false;
		dynObj.m_bIsSignificant = true;	// All vehicles for now

#ifdef GTA_ENGINE
		dynObj.m_bActiveForNavigation = ((CVehicle*)pEntity)->GetActiveForPedNavitation();
		dynObj.m_bInactiveDueToVelocity = ((CVehicle*)pEntity)->GetVelocity().Mag2() > (ms_fDynamicObjectVelocityThreshold*ms_fDynamicObjectVelocityThreshold);

		//-------------------------------------------
		// If this is a vehicle, then add the doors
		if(!dynObj.m_bIsMotorbike && ((CVehicle*)pEntity)->GetNumDoors())
		{
			AddVehicleDoorsDynamicObjects((CVehicle*)pEntity);
		}
#endif
	}
	else
	{
		Assert(0);
	}

#ifndef GTA_ENGINE
	LeaveCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
#else
	m_DynamicObjectsCriticalSectionToken.Unlock();
#endif

	return true;
}

#ifdef GTA_ENGINE
void CPathServerGta::AddVehicleDoorsDynamicObjects(CVehicle * pVehicle)
{
	s32 d;
	const s32 iNumDoors = pVehicle->GetNumDoors();

	for(d=0; d<iNumDoors; d++)
	{
		CCarDoor * pDoor = pVehicle->GetDoor(d);
		AddVehicleDoorDynamicObject(pVehicle, pDoor);
	}
}
void CPathServerGta::AddVehicleDoorDynamicObject(CVehicle * pVehicle, CCarDoor * pDoor)
{
	s32 s;

	if( pDoor->GetPathServerDynamicObjectIndex()!=DYNAMIC_OBJECT_INDEX_NONE && pDoor->GetPathServerDynamicObjectIndex()!=DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD )
	{
		/*
#if __ASSERT
		Displayf("url:bugstar:453546 - looks like vehicle door has already been added to navmesh. index=%i", pDoor->GetPathServerDynamicObjectIndex());
		Assert(pDoor->GetPathServerDynamicObjectIndex()==DYNAMIC_OBJECT_INDEX_NONE || pDoor->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
#endif
		*/
		return;
	}


	if( (pDoor->GetIsClosed() && !pDoor->GetForceOpenForNavigation()) || !pDoor->GetIsIntact(pVehicle) || !pDoor->GetFlag(CCarDoor::IS_ARTICULATED) )
		return;

	const eHierarchyId iDoorId = pDoor->GetHierarchyId();
	if(iDoorId!=VEH_DOOR_DSIDE_F && iDoorId!=VEH_DOOR_DSIDE_R && iDoorId!=VEH_DOOR_PSIDE_F && iDoorId!=VEH_DOOR_PSIDE_R)
	{
		return;
	}

	if(m_iNumDynamicObjects == PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		Assert(((CCarDoor*)pDoor)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CCarDoor*)pDoor)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		((CCarDoor*)pDoor)->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		return;
	}
	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

	// Find a free slot
	// TODO: this is crap, we should be able to get a free entry instantly
	for(s=0; s<PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS; s++)
	{
		if(m_PathServerThread.m_DynamicObjectsStore[s].IsObjectSlotAvailable())
			break;
	}
	// Couldn't find one ?
	if(s == PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		Assert(((CCarDoor*)pDoor)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CCarDoor*)pDoor)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		((CCarDoor*)pDoor)->SetPathServerDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
		return;
	}

	TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[s];
	dynObj.SetFlagsForNewObject();

	Matrix34 localMat, doorMat;
	if( !pDoor->GetLocalMatrix(pVehicle, localMat, pDoor->GetForceOpenForNavigation() ) )
	{
		NAVMESH_OPTIMISATIONS_OFF_ONLY( Assert(false); )
		return;
	}

	doorMat.Dot(localMat, MAT34V_TO_MATRIX34(pVehicle->GetMatrix()));

	TDynObjBoundsFuncs::CalcBoundsForVehicleDoor(dynObj.m_NewBounds, pDoor, pVehicle, doorMat, &dynObj);

	dynObj.m_NewBounds.m_fHeading = GetHeading(doorMat);
	dynObj.m_NewBounds.m_fRoll = GetRoll(doorMat);
	dynObj.m_NewBounds.m_fPitch = GetPitch(doorMat);
	dynObj.m_NewBounds.m_fUpZ = doorMat.c.z;

	sysMemCpy(&dynObj.m_Bounds, &dynObj.m_NewBounds, sizeof(TDynObjBounds));
	dynObj.CalcMinMaxForObject();

	dynObj.m_bInactiveDueToVelocity = ((CVehicle*)pVehicle)->GetVelocity().Mag2() > (ms_fDynamicObjectVelocityThreshold*ms_fDynamicObjectVelocityThreshold);

	dynObj.m_pCarDoor = pDoor;

	dynObj.m_pNext = m_PathServerThread.m_pFirstDynamicObject;
	m_PathServerThread.m_pFirstDynamicObject = &m_PathServerThread.m_DynamicObjectsStore[s];
	m_iNumDynamicObjects++;

	Assert(((CCarDoor*)pDoor)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_NONE || ((CCarDoor*)pDoor)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
	Assert(s < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);

	((CCarDoor*)pDoor)->SetPathServerDynamicObjectIndex((TDynamicObjectIndex) s);

	dynObj.m_bIsPushable = false;
	dynObj.m_bIsVehicle = false;
	dynObj.m_bIsMotorbike = false;
	dynObj.m_bIsFire = false;
	dynObj.m_bIsUserAddedBlockingObject = false;
	dynObj.m_bHasUprootLimit = false;

	dynObj.m_bVehicleDoor = true;
	dynObj.m_bVehicleDoorAlignedWithXAxis = (iDoorId==VEH_DOOR_DSIDE_F || iDoorId==VEH_DOOR_DSIDE_R);
}
void CPathServerGta::UpdateVehicleDoorsDynamicObjects(CVehicle * pVehicle, const bool bForceUpdate)
{
	const s32 iNumDoors = pVehicle->GetNumDoors();
	for(s32 d=0; d<iNumDoors; d++)
	{
		CCarDoor * pDoor = pVehicle->GetDoor(d);
		UpdateVehicleDoorDynamicObject(pVehicle, pDoor, bForceUpdate);
	}
}
void CPathServerGta::UpdateVehicleDoorDynamicObject(CVehicle * pVehicle, CCarDoor * pDoor, const bool bForceUpdate)
{
	if(!pDoor->GetIsIntact(pVehicle) || (pDoor->GetIsClosed() && !pDoor->GetForceOpenForNavigation()))
		return;
	const TDynamicObjectIndex iDynObjIndex = pDoor->GetPathServerDynamicObjectIndex();
	if(iDynObjIndex==DYNAMIC_OBJECT_INDEX_NONE || iDynObjIndex==DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
		return;
	TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[iDynObjIndex];
	if(dynObj.m_bCurrentlyCopyingBounds)
	{
		// If we are explicitly forcing an update, then we will want to keep trying until it is effective.
		if(bForceUpdate)
		{
			while(dynObj.m_bCurrentlyCopyingBounds)
				sysIpcYield(1);
		}
		// Otherwise we can return now, and the object will be updated later.
		else
			return;
	}
	const TDynObjBounds & objBounds = dynObj.m_bNewBounds ? dynObj.m_NewBounds : dynObj.m_Bounds;
	Matrix34 localMat, doorMat;
	if( !pDoor->GetLocalMatrix(pVehicle, localMat, pDoor->GetForceOpenForNavigation()) )
	{
		Assert(false);
		return;
	}

	doorMat.Dot(localMat, MAT34V_TO_MATRIX34(pVehicle->GetMatrix()));
	const Vector3 vDiff = doorMat.d - objBounds.GetEntityOrigin();


	// Same position ?
	if(!bForceUpdate && (vDiff.Mag2() < 0.1f))
	{
		const float fRotThreshold = (2.0f * DtoR);

		if( Abs(SubtractAngleShorter( objBounds.m_fHeading, GetHeading(doorMat) ) ) < fRotThreshold )
		{
			if( Abs(SubtractAngleShorter( objBounds.m_fPitch, GetPitch(doorMat) ) ) < fRotThreshold )
			{
				if( Abs(SubtractAngleShorter( objBounds.m_fRoll, GetRoll(doorMat) ) ) < fRotThreshold )
				{
					return;
				}
			}
		}
	}

	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

	// Mark that we're updating this object
	dynObj.m_bCurrentlyUpdatingNewBounds = true;
	sys_lwsync();

	TDynObjBoundsFuncs::CalcBoundsForVehicleDoor(dynObj.m_NewBounds, pDoor, pVehicle, doorMat, &dynObj);

	dynObj.m_NewBounds.m_fHeading = GetHeading(doorMat);
	dynObj.m_NewBounds.m_fRoll = GetRoll(doorMat);
	dynObj.m_NewBounds.m_fPitch = GetPitch(doorMat);
	dynObj.m_NewBounds.m_fUpZ = doorMat.c.z;

	dynObj.m_bNewBounds = true;
	dynObj.m_bInactiveDueToVelocity = ((CVehicle*)pVehicle)->GetVelocity().Mag2() > (ms_fDynamicObjectVelocityThreshold*ms_fDynamicObjectVelocityThreshold);

	sys_lwsync();
	dynObj.m_bCurrentlyUpdatingNewBounds = false;
}
#endif // GTA_ENGINE

bool CPathServer::FlagDynamicObjectForRemoval(const TDynamicObjectIndex iDynObjIndex)
{
	Assert(iDynObjIndex < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);
	TDynamicObject & dynObj = m_PathServerThread.m_DynamicObjectsStore[iDynObjIndex];

	dynObj.m_bFlaggedForDeletion = true;
	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

	return (iDynObjIndex != DYNAMIC_OBJECT_INDEX_NONE);
}


//***********************************************************************************************
// Called by constructor for some entity types.  If this entity's model type has an associated
// navmesh, then add a reference to it so that it will be streamed in by Request/Evict code
// (see CPathServer::RequestAndEvictMeshes)

#ifdef GTA_ENGINE
bool
CPathServerGta::MaybeRequestDynamicObjectNavMesh(CEntity * pEntity)
{
	// Currently only call this for boats
	Assert(pEntity->GetIsTypeVehicle());
	if(pEntity->GetIsTypeVehicle())
	{
		CBaseModelInfo * pModelInfo = pEntity->GetBaseModelInfo();
		CVehicleModelInfo * pVehModelInfo = (CVehicleModelInfo*)pModelInfo;

		if(pVehModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_GEN_NAVMESH))
		{
			int i;
			for(i=0; i<m_DynamicNavMeshStore.GetNum(); i++)
			{
				CModelInfoNavMeshRef & ref = m_DynamicNavMeshStore.Get(i);
				if(ref.m_iModelIndex.Get()==(s32)pEntity->GetModelIndex())
				{
					// Set flag in vehicle so that it knows it has an associated navmesh
					CVehicle * pVehicle = (CVehicle*)pEntity;

					Assert(!pVehicle->m_nVehicleFlags.bHasDynamicNavMesh);
					pVehicle->m_nVehicleFlags.bHasDynamicNavMesh = true;

					ref.m_iNumRefs++;
					CPathServer::m_iNumDynamicNavMeshesInExistence++;

					return true;
				}
			}
		}
	}

	return false;
}
#endif

//***********************************************************************************************
// Called by object destructors to del a ref if there is an associated navmesh

#ifdef GTA_ENGINE
bool
CPathServerGta::MaybeRemoveDynamicObjectNavMesh(CEntity * pEntity)
{
	// Currently only call this for vehicles
	Assert(pEntity->GetIsTypeVehicle());
	if(pEntity->GetIsTypeVehicle())
	{
		CVehicle * pVehicle = (CVehicle*)pEntity;

		if(pVehicle->m_nVehicleFlags.bHasDynamicNavMesh)
		{
			int i;
			for(i=0; i<m_DynamicNavMeshStore.GetNum(); i++)
			{
				CModelInfoNavMeshRef & ref = m_DynamicNavMeshStore.Get(i);
				if(ref.m_iModelIndex.Get() == (s32)pEntity->GetModelIndex())
				{
					// Reset flag in vehicle so that it knows it no longer has an associated navmesh
					CVehicle * pVehicle = (CVehicle*)pEntity;

					Assert(pVehicle->m_nVehicleFlags.bHasDynamicNavMesh);
					pVehicle->m_nVehicleFlags.bHasDynamicNavMesh = false;

					ref.m_iNumRefs--;
					Assert(ref.m_iNumRefs >= 0);

					CPathServer::m_iNumDynamicNavMeshesInExistence--;
					Assert(CPathServer::m_iNumDynamicNavMeshesInExistence >= 0);

					return true;
				}
			}
		}
	}

	return false;
}

#endif// GTA_ENGINE


bool Hack_AllowClothOnDynamicObject(const u32 iModelIndex)
{
	if(MI_PROP_FNCLINK_04J.IsValid() && iModelIndex==MI_PROP_FNCLINK_04J.GetModelIndex())
		return true;
	if(MI_PROP_FNCLINK_04K.IsValid() && iModelIndex==MI_PROP_FNCLINK_04K.GetModelIndex())
		return true;
	if(MI_PROP_FNCLINK_04M.IsValid() && iModelIndex==MI_PROP_FNCLINK_04M.GetModelIndex())
		return true;
	if(MI_PROP_FNCLINK_02I.IsValid() && iModelIndex==MI_PROP_FNCLINK_02I.GetModelIndex())
		return true;
	if(MI_PROP_FNCLINK_02J.IsValid() && iModelIndex==MI_PROP_FNCLINK_02J.GetModelIndex())
		return true;
	if(MI_PROP_AIR_WINDSOCK.IsValid() && iModelIndex==MI_PROP_AIR_WINDSOCK.GetModelIndex())
		return true;
	if(MI_PROP_AIR_CARGO_01A.IsValid() && iModelIndex==MI_PROP_AIR_CARGO_01A.GetModelIndex())
		return true;
	if(MI_PROP_AIR_CARGO_01B.IsValid() && iModelIndex==MI_PROP_AIR_CARGO_01B.GetModelIndex())
		return true;
	if(MI_PROP_AIR_CARGO_01C.IsValid() && iModelIndex==MI_PROP_AIR_CARGO_01C.GetModelIndex())
		return true;
	if(MI_PROP_AIR_CARGO_02A.IsValid() && iModelIndex==MI_PROP_AIR_CARGO_02A.GetModelIndex())
		return true;
	if(MI_PROP_AIR_CARGO_02B.IsValid() && iModelIndex==MI_PROP_AIR_CARGO_02B.GetModelIndex())
		return true;
	if(MI_PROP_AIR_TRAILER_1A.IsValid() && iModelIndex==MI_PROP_AIR_TRAILER_1A.GetModelIndex())
		return true;
	if(MI_PROP_AIR_TRAILER_1B.IsValid() && iModelIndex==MI_PROP_AIR_TRAILER_1B.GetModelIndex())
		return true;
	if(MI_PROP_AIR_TRAILER_1C.IsValid() && iModelIndex==MI_PROP_AIR_TRAILER_1C.GetModelIndex())
		return true;
	if(MI_PROP_SKID_TENT_CLOTH.IsValid() && iModelIndex==MI_PROP_SKID_TENT_CLOTH.GetModelIndex())
		return true;
	if(MI_PROP_SKID_TENT_01.IsValid() && iModelIndex==MI_PROP_SKID_TENT_01.GetModelIndex())
		return true;
	if(MI_PROP_SKID_TENT_01B.IsValid() && iModelIndex==MI_PROP_SKID_TENT_01B.GetModelIndex())
		return true;
	if(MI_PROP_SKID_TENT_03.IsValid() && iModelIndex==MI_PROP_SKID_TENT_03.GetModelIndex())
		return true;

	if(MI_PROP_BEACH_PARASOL_01.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_01.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_02.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_02.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_03.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_03.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_04.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_04.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_05.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_05.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_06.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_06.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_07.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_07.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_08.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_08.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_09.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_09.GetModelIndex())
		return true;
	if(MI_PROP_BEACH_PARASOL_10.IsValid() && iModelIndex==MI_PROP_BEACH_PARASOL_10.GetModelIndex())
		return true;

	return false;
}

bool CPathServerGta::ShouldAvoidObject(const CEntity* pEntity)
{
#ifdef GTA_ENGINE
	if(pEntity->GetType() != ENTITY_TYPE_OBJECT && pEntity->GetType() != ENTITY_TYPE_VEHICLE)
	{
		return false;
	}

	if(!pEntity->GetIsPhysical() || !pEntity->GetCurrentPhysicsInst() || !pEntity->IsCollisionEnabled())
	{
		return false;
	}

	CBaseModelInfo * pModelInfo = pEntity->GetBaseModelInfo();
	const bool bScriptObject = pEntity->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT;

	// Don't avoid things which have been 'baked-into' the navmesh, but also exist as dynamic objects (eg. some fragmentable stuff)
	if(pModelInfo->GetIsFixedForNavigation() && !bScriptObject)
	{
		return false;
	}
	// Don't avoid things which are flagged as being unimportant.
	// But never for doors.  Tsk, why are artists marking doors as not-avoid?
	if(pModelInfo->GetNotAvoidedByPeds() && !pModelInfo->GetUsesDoorPhysics())
	{
		return false;
	}
	// Don't avoid weapons
	if(pModelInfo->GetModelType() == MI_TYPE_WEAPON)
	{
		return false;
	}

	// Don't avoid cloth objects.  If at some point complex objects are created which need avoiding, but which also
	// contain cloth - then we shall have to exclude the cloth phBound when calling ComputeBoxes() in CEntityBoundAI.
	if(pEntity->GetType() != ENTITY_TYPE_VEHICLE)
	{
		gtaFragType * fragType = pModelInfo->GetFragType();
		if(fragType && fragType->GetNumEnvCloths())
		{
			if(!Hack_AllowClothOnDynamicObject(pEntity->GetModelIndex()))
			{
				return false;
			}
		}
	}

	// Do a test on the bounding-box size of the entity.  Really small entities need not be avoided.
	const Vector3 vMin = pEntity->GetBoundingBoxMin();
	const Vector3 vMax = pEntity->GetBoundingBoxMax();
	const Vector3 vSize = vMax - vMin;

	// Set a bottom limit for the size of objects which are avoided.  This is in addition to the
	// artist-assigned CBaseModelInfo::GetNotAvoidedByPeds() flag.
	static const bool bIgnoreFixedObjects = true;
	static const float fMinSizeForNormalObjects = 0.3f;
	static const float fMinSizeForFragObjects = 6.0f;	// this is intentionally very large as missions were being broken by debris confusing the pathfinder
	static const float fMinMassForObjects = 30.0f;

	if(pEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		CObject * pObj = (CObject*)pEntity;

		// Don't avoid pickups
		if(pObj->m_nObjectFlags.bIsPickUp)
			return false;

		// Don't avoid objects which are attached to things
		if(pObj->GetIsAttached())
			return false;

		// Don't avoid objects which are fixed, they are already baked into the navmesh.
		// Unless these are script-created objects, which may be created/repositioned & therefore cannot be baked-in.
		const bool bIsFixed = pModelInfo->GetIsFixed() ||
			(pEntity->IsBaseFlagSet(fwEntity::IS_FIXED) && !pObj->GetFixedByScriptOrCode());

		if(bIgnoreFixedObjects && bIsFixed && !bScriptObject && !pObj->IsADoor())
			return false;

		// Don't avoid objects which are deemed as small enough to ignore
		if(pObj->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
		{
			static const float fMassOfIgnorableFragment = 1000.0f;
			if(pObj->GetMass() < fMassOfIgnorableFragment &&
				(vSize.x < fMinSizeForFragObjects && vSize.y < fMinSizeForFragObjects && vSize.z < fMinSizeForFragObjects) &&
				(!SpecialCaseShouldAvoidFragmentObject(pObj)))
				return false;
		}
		else
		{
			if(vSize.x < fMinSizeForNormalObjects && vSize.y < fMinSizeForNormalObjects && vSize.z < fMinSizeForNormalObjects)
				return false;
		}

		// Don't avoid objects which have no uproot limit, and which are under a certain mass - these will be pushed aside
		// Skip this logic for script objects, which we can assume will always want to be avoided
		const bool bIsBreakableGlass = pObj->m_nDEflags.bIsBreakableGlass;
		const bool bIsSmall = (vSize.x < 0.75f) && (vSize.y < 0.75f) && (vSize.z < 0.75f);

		//Vector3 RefPos = Vector3(317.89f, 2621.94f, 43.51f);
		//RefPos.Dist(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition())) < 0.1f &&
		if (!bScriptObject && !bIsBreakableGlass && bIsSmall && pObj->GetMass() < fMinMassForObjects)
		{
			bool bIsTooStrong = false;
			const fragInst* pFragInst = pObj->GetFragInst();
			const fragType* pFragType = pFragInst ? pFragInst->GetType() : NULL;
			const fragPhysicsLOD* pFragPhysics = pFragType ? pFragType->GetPhysics(0) : NULL; // Force highest lod just to be sure
			//		float minMoveForce = pFragPhysics->GetMinMoveForce();

			int nGroups = pFragPhysics ? pFragPhysics->GetNumChildGroups() : 0;
			for (int i = 0; i < nGroups; ++i)
			{
				fragTypeGroup* pGroup = pFragPhysics->GetGroup(i);
				float fPedScale = pGroup->GetPedScale();
				float fStrength = pGroup->GetStrength();

				// Logic same as walking in fragInstGta::ModifyImpulseMag, we can assume that ped will be at least moving
				float breakingImpulse = 75 * fPedScale;
				if (fStrength > breakingImpulse)
				{
					// Avoid since we cannot "break" it out of its position
					bIsTooStrong = true;
				}
			}

			const bool bHasUprootLimit = (pFragInst!=NULL && (pFragInst->GetTypePhysics()->GetMinMoveForce() > 0.0f) && !((CObject*)pEntity)->m_nObjectFlags.bHasBeenUprooted);

			if (!bIsTooStrong && !bHasUprootLimit)
				return false;
		}
	}
	else
	{
		if(vSize.x < fMinSizeForNormalObjects && vSize.y < fMinSizeForNormalObjects && vSize.z < fMinSizeForNormalObjects)
			return false;
	}

#endif // GTA_ENGINE

	return true;
}

#ifdef GTA_ENGINE
// HACKS for GTA5
// By default we don't avoid fragments who are smaller than 6m on all sides, and whose mass is under a give threshold (1000.0f)
// That's a pretty large mass threshold - but as of writing its too risky to reduce it - so we'll instead special-case certain
// models whose fragments we will always add into the pathfinder regardless of size/mass.
bool CPathServerGta::SpecialCaseShouldAvoidFragmentObject(CObject * pObj)
{
	const u32 mi = pObj->GetModelIndex();
	if(mi == MI_PHONEBOX_4.GetModelIndex())
		return true;

	return false;
}
#endif

bool CPathServerGta::SpecialCaseDontRemoveSmashedGlass(CObject * pObj)
{
	bool bIsGlassDoor = pObj->IsADoor();
	if(!bIsGlassDoor)
		return false;

	static const int iNumPos = 10;
	static const Vec3V vDoorPositions[iNumPos] =
	{
		Vec3V(2509.743f,-266.551f,-38.965f),
		Vec3V(2527.861f,-273.087f,-58.573f),
		Vec3V(2530.856f,-273.880f,-58.573f),
		Vec3V(2536.156f,-283.601f,-58.573f),
		Vec3V(2533.647f,-284.940f,-58.573f),
		Vec3V(2506.558f,-282.690f,-58.573f),
		Vec3V(2503.288f,-276.873f,-58.573f),
		Vec3V(2523.927f,-274.690f,-58.573f),
		Vec3V(2475.100f,-263.802f,-70.546f),
		Vec3V(2467.545f,-272.236f,-70.546f)
	};

	Vec3V vPos = pObj->GetTransform().GetPosition();

	for(int i=0; i<iNumPos; i++)
	{
		Vec3V vMin = vDoorPositions[i] - Vec3V(2.0f,2.0f,2.0f);
		Vec3V vMax = vDoorPositions[i] + Vec3V(2.0f,2.0f,2.0f);

		bool bWithinAABB = IsGreaterThanAll(vPos, vMin) && IsGreaterThanAll(vMax, vPos);
		if(bWithinAABB)
			return true;
	}	

	return false;
}

bool CPathServerGta::ShouldPedAvoidObject(const CPed* pPed, const CEntity* pEntity)
{
#ifdef GTA_ENGINE
	if (IsObjectSignificant(pEntity))	// Always ignore these objects
		return true;

	if (ShouldAvoidObject(pEntity))		// Ignore these if we are in that pathfinder mode were we ignore some objects
	{
		// Do we ever ignore all objects? If not, maybe we don't even need to check this task!
		CTaskMoveFollowNavMesh * pExistingNavMeshTask = (CTaskMoveFollowNavMesh*)pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
		if (pExistingNavMeshTask && pExistingNavMeshTask->IsAvoidingNonMovableObjects())
			return false;
		
		return true;
	}
#endif // GTA_ENGINE
	// 
	return false;
}

bool CPathServerGta::IsObjectSignificant(const CEntity* pEntity)
{
#ifdef GTA_ENGINE
	if (pEntity && pEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		const Vector3 vSize = pEntity->GetBoundingBoxMax() - pEntity->GetBoundingBoxMin();
		const float fVolume = vSize.x * vSize.y * vSize.z;
		const bool bIsMinMass = ((CObject*)pEntity)->GetMass() > CPathServerThread::ms_fMinMassOfSmallSignificant;
		const bool bIsMinSize = fVolume > CPathServerThread::ms_fMinVolumeOfSignificant;
		const bool bIsSignificantSize = fVolume > CPathServerThread::ms_fBigVolumeOfSignificant;

		return ((bIsMinMass && bIsMinSize) || bIsSignificantSize);
	}
#endif // GTA_ENGINE

	return false;
}

bool CPathServerThread::TestDynamicObjectLOSCallback(TDynamicObject * pObject, void * pData)
{
	if(!pObject->m_bIsCurrentlyAnObstacle)
		return true;

	TDynObjLosCallbackData * pCallbackData = (TDynObjLosCallbackData*)pData;

	// Early-out if min/max of line don't overlap with min/max of object
	if(pCallbackData->fMinZOfStartAndEnd > pObject->m_Bounds.GetTopZ())
		return true;

	/*
	if(CPathServer::ms_bSphereRayEarlyOutTestOnObjects)
	{
		if(geomDistances::Distance2SegToPoint(pCallbackData->vStartPos, pCallbackData->vEndPos - pCallbackData->vStartPos, pObject->m_Bounds.m_vOrigin) >= pObject->m_Bounds.m_fBoundingRadius)
			return true;
	}
	*/

#if __ALLOW_TO_PUSH_VEHICLE_DOORS_CLOSED
	if(pObject->m_bVehicleDoor && pCallbackData->pPathServerThread->m_pCurrentActivePathRequest && pCallbackData->pPathServerThread->m_pCurrentActivePathRequest->m_bAllowToPushVehicleDoorsClosed)
	{
		const float fSign = (pObject->m_bVehicleDoorAlignedWithXAxis) ? 1.0f : -1.0f;
		const float dx = pCallbackData->vEndPos.x - pCallbackData->vStartPos.x;
		const float dy = pCallbackData->vEndPos.y - pCallbackData->vStartPos.y;
		const float fRight = pObject->m_Bounds.m_fHeading - HALF_PI;
		const Vector2 vRight(-rage::Sinf(fRight), rage::Cosf(fRight));
		const float fDot = (dx * (vRight.x * fSign)) + (dy * (vRight.y * fSign));
		if(fDot >= 0.0f)
		{
			return true;
		}
	}
#endif

	Vector3 vClippedStart;
	Vector3 vClippedEnd;

#if __USE_SECOND_OBJECT_LOS_TEST
	if(pCallbackData->fMaxZOfStartAndEnd < pObject->m_Bounds.GetBottomZ())
	{
		if(pCallbackData->fMaxZOfStartAndEnd+ms_fHeightAboveFirstTestFor2ndDynObjLOS > pObject->m_Bounds.GetBottomZ())
		{
			vClippedStart = Vector3(pCallbackData->vStartPos.x, pCallbackData->vStartPos.y, pCallbackData->vStartPos.z+ms_fHeightAboveFirstTestFor2ndDynObjLOS);
			vClippedEnd = Vector3(pCallbackData->vEndPos.x, pCallbackData->vEndPos.y, pCallbackData->vEndPos.z+ms_fHeightAboveFirstTestFor2ndDynObjLOS);
		}
		else
		{
			return true;
		}
	}
	else
	{
		vClippedStart = pCallbackData->vStartPos;
		vClippedEnd = pCallbackData->vEndPos;
	}
#else
	if(pCallbackData->fMaxZOfStartAndEnd < pObject->m_Bounds.GetBottomZ())
		return true;
	vClippedStart = pCallbackData->vStartPos;
	vClippedEnd = pCallbackData->vEndPos;
#endif	// __USE_SECOND_OBJECT_LOS_TEST

	s32 iNumPlanesStartPointIsBehind = 0;
	s32 pl;

	const float* fDynObjPlaneEpsilon = GetDynObjPlaneEpsilon(pObject);
	for(pl=0; pl<NUM_DYNAMIC_OBJECT_PLANES; pl++)
	{
		const float fStartDot = (pObject->m_Bounds.m_vEdgePlaneNormals[pl].Dot3(vClippedStart) + pObject->m_Bounds.m_vEdgePlaneNormals[pl].w) + fDynObjPlaneEpsilon[pl];
		const float fEndDot = (pObject->m_Bounds.m_vEdgePlaneNormals[pl].Dot3(vClippedEnd) + pObject->m_Bounds.m_vEdgePlaneNormals[pl].w) + fDynObjPlaneEpsilon[pl];

		// Both start & end are in front of a plane, so no intersection is possible
		if(fStartDot > 0.0f && fEndDot > 0.0f)
		{
			break;
		}
		else if(fStartDot > 0.0f && fEndDot < 0.0f)
		{
			const float s = fStartDot / (fStartDot - fEndDot);
			vClippedStart = vClippedStart + ((vClippedEnd - vClippedStart) * s);
		}
		else if(fStartDot < 0.0f)
		{
			iNumPlanesStartPointIsBehind++;

			if(fEndDot > 0.0f)
			{
				const float s = fStartDot / (fStartDot - fEndDot);
				vClippedEnd = vClippedStart + ((vClippedEnd - vClippedStart) * s);
			}
		}
	}

	// If line wasn't clipped away to any plane, then we must have an intersection
	// But if the start point was inside, we'll let it go - since the ped is either
	// slightly inside the object, or is standing on top of it !
	if(pl == NUM_DYNAMIC_OBJECT_PLANES)
	{
		pCallbackData->pPathServerThread->m_iDynamicObjLos_TotalNumObjectsHit++;

		// Have we hit an object which we can open ?
		if(pObject->m_bIsOpenable)
		{
			pCallbackData->pPathServerThread->m_iDynamicObjLos_NumOpenableObjectsHit++;
		}
		else
		{
			pCallbackData->bIntersection = true;
			return false;
		}
	}

	return true;
}



bool CPathServerThread::TestDynamicObjectLOS(const Vector3 & vStart, const Vector3 & vEnd)
{
#if !__FINAL
	CPathServer::ms_iNumTestDynamicObjectLOS++;
#endif

	m_iDynamicObjLos_TotalNumObjectsHit = 0;
	m_iDynamicObjLos_NumOpenableObjectsHit = 0;

	//********************************************************************
	//	Initial test based on integer coord comparisons
	//********************************************************************

	TShortMinMax minMax;
	float fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ;

	if(vStart.x < vEnd.x) { fMinX = vStart.x; fMaxX = vEnd.x; }
	else { fMaxX = vStart.x; fMinX = vEnd.x; }
	if(vStart.y < vEnd.y) { fMinY = vStart.y; fMaxY = vEnd.y; }
	else { fMaxY = vStart.y; fMinY = vEnd.y; }
	if(vStart.z < vEnd.z) { fMinZ = vStart.z; fMaxZ = vEnd.z; }
	else { fMaxZ = vStart.z; fMinZ = vEnd.z; }

	minMax.SetFloat(fMinX, fMinY, fMinZ - 1.0f, fMaxX, fMaxY, fMaxZ + 1.0f);

	TDynObjLosCallbackData callbackData;
	callbackData.vStartPos = vStart;
	callbackData.vStartPos.z += ms_fHeightAboveNavMeshForDynObjLOS;
	callbackData.vEndPos = vEnd;
	callbackData.vEndPos.z += ms_fHeightAboveNavMeshForDynObjLOS;
	callbackData.fMinZOfStartAndEnd = Min(callbackData.vStartPos.z, callbackData.vEndPos.z);
	callbackData.fMaxZOfStartAndEnd = Max(callbackData.vStartPos.z, callbackData.vEndPos.z);
	callbackData.bIntersection = false;
	callbackData.pPathServerThread = this;

	CDynamicObjectsContainer::ForAllObjectsIntersectingRegion(minMax, TestDynamicObjectLOSCallback, (void*)&callbackData);

	return !(callbackData.bIntersection);
}


bool CPathServerThread::TestDynamicObjectLOSWithRadius(const Vector3 & vStart, const Vector3 & vEnd, const float fRadius)
{
	Vector3 vDir = vEnd - vStart;
	vDir.Normalize();
	const Vector3 vRight = CrossProduct(vDir, Vector3(0.0f,0.0f,1.0f)) * fRadius;

	if(TestDynamicObjectLOS(vStart - vRight, vEnd - vRight))
	{
		if(TestDynamicObjectLOS(vStart + vRight, vEnd + vRight))
		{
			return true;
		}
	}

	return false;
}

bool CPathServerThread::TestDynamicObjectLOS(const Vector3 & vStart, const Vector3 & vEnd, atArray<TDynamicObject*> & objectList)
{
#if !__FINAL
	CPathServer::ms_iNumTestDynamicObjectLOS++;
#endif

	m_iDynamicObjLos_TotalNumObjectsHit = 0;
	m_iDynamicObjLos_NumOpenableObjectsHit = 0;

	// Must add a little bit of height here, since the points will actually be on the NavMesh
	const Vector3 vStartAbove = Vector3(vStart.x, vStart.y, vStart.z + ms_fHeightAboveNavMeshForDynObjLOS);
	const Vector3 vEndAbove = Vector3(vEnd.x, vEnd.y, vEnd.z + ms_fHeightAboveNavMeshForDynObjLOS);

	const float fMinHeight = rage::Min(vStartAbove.z, vEndAbove.z);
	const float fMaxHeight = rage::Max(vStartAbove.z, vEndAbove.z);

#if __USE_SECOND_OBJECT_LOS_TEST
	const float fMaxHeightToTest = fMaxHeight+ms_fHeightAboveFirstTestFor2ndDynObjLOS;
	const Vector3 vStartAbove2 = Vector3(vStart.x, vStart.y, vStart.z + ms_fHeightAboveNavMeshForDynObjLOS + ms_fHeightAboveFirstTestFor2ndDynObjLOS);
	const Vector3 vEndAbove2 = Vector3(vEnd.x, vEnd.y, vEnd.z + ms_fHeightAboveNavMeshForDynObjLOS + ms_fHeightAboveFirstTestFor2ndDynObjLOS);
#endif

	int pl;
	Vector3 vClippedStart, vClippedEnd;
	// Vector3 vVec = vEnd - vStart;

	int o = objectList.GetCount();
	TDynamicObject ** ppElements = objectList.GetElements();
	while(o--)
	{
		if (o)	// Don't waste time prefetching a potentially invalid address (TODO: Maybe make this PC-specific?)
			PrefetchDC(ppElements[o-1]);

		TDynamicObject * pCurr = objectList[o];
		const TDynObjBounds & bnds = pCurr->m_Bounds;

		if(!pCurr->m_bIsCurrentlyAnObstacle)
			continue;

		// Early-out cull if lowest start/end Z of line are both above the top of the object
		if(fMinHeight > bnds.GetTopZ())
			continue;

		/*
		if(CPathServer::ms_bSphereRayEarlyOutTestOnObjects)
		{
			if(geomDistances::Distance2SegToPoint(vStart, vVec, bnds.m_vOrigin) >= bnds.m_fBoundingRadius)
				continue;
		}
		*/

		// For vehicle doors we can instantly discard tests if the start position is in front of the door
		// as in these cases we will always want for the door to be pushed closed
#if __ALLOW_TO_PUSH_VEHICLE_DOORS_CLOSED
		if(pCurr->m_bVehicleDoor && m_pCurrentActivePathRequest && m_pCurrentActivePathRequest->m_bAllowToPushVehicleDoorsClosed)
		{
			const float fSign = (pCurr->m_bVehicleDoorAlignedWithXAxis) ? 1.0f : -1.0f;
			const float dx = vEnd.x - vStart.x;
			const float dy = vEnd.y - vStart.y;
			const float fRight = pCurr->m_Bounds.m_fHeading - HALF_PI;
			const Vector2 vRight(-rage::Sinf(fRight), rage::Cosf(fRight));
			const float fDot = (dx * (vRight.x * fSign)) + (dy * (vRight.y * fSign));
			if(fDot >= 0.0f )
			{
				continue;
			}
		}
#endif

#if __USE_SECOND_OBJECT_LOS_TEST
		// If this object's lowest extent is *above* the height we normally use (ms_fHeightAboveNavMeshForDynObjLOS) for
		// object LOS tests, then use the extra height value instead ms_fHeightAboveFirstTestFor2ndDynObjLOS
		if(fMaxHeight < bnds.GetBottomZ())
		{
			if(fMaxHeightToTest > bnds.GetBottomZ())
			{
				vClippedStart = vStartAbove2;
				vClippedEnd = vEndAbove2;
			}
			else
			{
				continue;
			}
		}
		else
		{
			vClippedStart = vStartAbove;
			vClippedEnd = vEndAbove;
		}
#else
		// Early-out cull if highest start/end Z of line are both below the top of the object
		if(fMaxHeight < bnds.GetBottomZ())
			continue;

		vClippedStart = vStartAbove;
		vClippedEnd = vEndAbove;

#endif	// __USE_SECOND_OBJECT_LOS_TEST

		//-----------------------------------------------------------------
		// Firstly perform quick 2d linetests against the object's vertices

		s32 iNumPlanesStartPointIsBehind = 0;


		// Otherwise if we have a hit against the 2D edges, we can proceed to test top & bottom planes
		// to see whether this hit should be discounted

		{
			const float* fDynObjPlaneEpsilon = GetDynObjPlaneEpsilon(pCurr);
			for( pl=0; pl<NUM_DYNAMIC_OBJECT_PLANES; pl++ )
			{
				const float fStartDot = (bnds.m_vEdgePlaneNormals[pl].Dot3(vClippedStart) + bnds.m_vEdgePlaneNormals[pl].w) + fDynObjPlaneEpsilon[pl];
				const float fEndDot = (bnds.m_vEdgePlaneNormals[pl].Dot3(vClippedEnd) + bnds.m_vEdgePlaneNormals[pl].w) + fDynObjPlaneEpsilon[pl];

				// Both start & end are in front of a plane, so no intersection is possible
				if(fStartDot > 0.0f && fEndDot > 0.0f)
				{
					break;
				}
				else if(fStartDot > 0.0f && fEndDot < 0.0f)
				{
					const float s = fStartDot / (fStartDot - fEndDot);
					vClippedStart = vClippedStart + ((vClippedEnd - vClippedStart) * s);
				}
				else if(fStartDot < 0.0f)
				{
					iNumPlanesStartPointIsBehind++;

					if(fEndDot > 0.0f)
					{
						const float s = fStartDot / (fStartDot - fEndDot);
						vClippedEnd = vClippedStart + ((vClippedEnd - vClippedStart) * s);
					}
				}
			}

			// If line wasn't clipped away to any plane, then we must have an intersection
			if(pl == NUM_DYNAMIC_OBJECT_PLANES)
			{
				m_iDynamicObjLos_TotalNumObjectsHit++;

				// Have we hit an object which we can open ?
				if(pCurr->m_bIsOpenable)
				{
					m_iDynamicObjLos_NumOpenableObjectsHit++;
				}
				else
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool CPathServerThread::TestIsClimbClearOfObjects(u32 AdjacencyType, TNavMeshPoly* pPoly, TNavMeshPoly* pAdjPoly, CNavMesh* pNavMesh, CNavMesh* pAdjNavMesh)
{
	Assertf(AdjacencyType == ADJACENCY_TYPE_CLIMB_LOW || AdjacencyType == ADJACENCY_TYPE_CLIMB_HIGH, "TestIsClimbClearOfObjects expect proper climb adjtype");

	float fTestHeight = 0.f;
	if (AdjacencyType == ADJACENCY_TYPE_CLIMB_LOW)
		fTestHeight = 1.f;
	else if (AdjacencyType == ADJACENCY_TYPE_CLIMB_HIGH)
		fTestHeight = 2.f;
	
	Vector3 vPolyCenter;
	Vector3 vAdjPolyCenter;

	if(pNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION)
		pNavMesh->GetPolyCentroid(pNavMesh->GetPolyIndex(pPoly), vPolyCenter);
	else
		pNavMesh->GetPolyCentroidQuick(pPoly, vPolyCenter);

	if(pAdjNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION)
		pAdjNavMesh->GetPolyCentroid(pAdjNavMesh->GetPolyIndex(pAdjPoly), vAdjPolyCenter);
	else
		pAdjNavMesh->GetPolyCentroidQuick(pAdjPoly, vAdjPolyCenter);

	Vector3 vLinePos = vPolyCenter + ZAXIS * fTestHeight;
	Vector3 vLineTest = vAdjPolyCenter - vPolyCenter;
	vLineTest.z = 0.f;

	return TestDynamicObjectLOS(vLinePos, vLinePos + vLineTest);		
}

bool
CPathServerThread::DoesRegionIntersectAnyDynamicObjects(const Vector3 & vPosition, float fRadius)
{
	//********************************************************************
	//	Initial test based on integer coord comparisons
	//********************************************************************

	TShortMinMax minMax;
	minMax.SetFloat(vPosition.x - fRadius, vPosition.y - fRadius, vPosition.z - fRadius, vPosition.x + fRadius, vPosition.y + fRadius, vPosition.z + fRadius);

	bool bPossibleIntersection = false;

#ifdef GTA_ENGINE
	CHECK_FOR_THREAD_STALLS(m_DynamicObjectsCriticalSectionToken)
	sysCriticalSection dynamicObjectsCriticalSection(m_DynamicObjectsCriticalSectionToken);
#else
	// Wait until dynamic objects are available
	EnterCriticalSection(&m_DynamicObjectsCriticalSection);
#endif

	TDynamicObject * pCurr = m_pFirstDynamicObject;
	while(pCurr)
	{
		if(!pCurr->m_bIsCurrentlyAnObstacle)
		{
			pCurr = pCurr->m_pNext;
			continue;
		}

		if(pCurr->m_bIsActive && minMax.Intersects(pCurr->m_MinMax))
		{
			bPossibleIntersection = true;
			break;
		}

		pCurr = pCurr->m_pNext;
	}

#ifndef GTA_ENGINE
	LeaveCriticalSection(&m_DynamicObjectsCriticalSection);
#endif

	return bPossibleIntersection;
}

//*****************************************************************************************
//	DoesPositionIntersectAnyDynamicObjectsApproximate
//	This function just checks whether the AABBs of any objects and the vPosition overlap.

bool
CPathServerThread::DoesPositionIntersectAnyDynamicObjectsApproximate(const Vector3 & vPosition) const
{
	TShortMinMax minMax;
	minMax.SetFloat(vPosition.x, vPosition.y, vPosition.z - 1.0f, vPosition.x, vPosition.y, vPosition.z + 1.0f);

	return CDynamicObjectsContainer::DoesRegionIntersectAnyObjects(minMax);
}


//*****************************************************************************************
//	DoesPositionIntersectAnyDynamicObjectsApproximate
//	This function just checks whether the AABBs of any objects and the vPosition overlap.

bool
CPathServerThread::DoesMinMaxIntersectAnyDynamicObjectsApproximate(const TShortMinMax & minMax) const
{
	return CDynamicObjectsContainer::DoesRegionIntersectAnyObjects(minMax);
}

//*********************************************************************************************************
//	FindPositionClearOfDynamicObjects
//
//	Given vPosition (typically the start/end point of a path) this function sees if it intersects any
//	dynamic objects, and if so it pushes the point out of the closest object edge by the edge-plane normal.
//	If the position is still inside an object, it continues the process a number of times.  In some cases
//	the point will never get pushed completely clear of objects, and in this case a last-ditch attempt can
//	be made by jittering the position randomly.  (NB: This 'bDoLastResortJittering' is by defualt never
//	used - instead, the navmesh task will look for the path again but will either contract objects' b-boxes
//	or will disable object avoidance completely.
//	"bTestLineOfSight" can be specified to make sure that a navmesh LOS exists (objects not considered) from
//	vPosition to the newly-adjusted position.  This makes sure that we don't push the point clear through a
//	wall or do something else silly.
//	"bIgnoreOpenable", "bIgnoreClimbable" & "bIgnorePushable" can all be specified so that certain objects
//	are not considered obstacles.  In practice only "bIgnoreOpenable" is used, since there's been problems
//	implementing the other two.
//*********************************************************************************************************

bool
CPathServerThread::FindPositionClearOfDynamicObjects(Vector3 & vPosition, bool bDoLastResortJittering, bool bIgnoreOpenable, bool bTestLineOfSight, aiNavDomain domain)
{
	//********************************************************************
	//	Initial test based on integer coord comparisons
	//********************************************************************

	const Vector3 vInputPos = vPosition;

	TShortMinMax minMax;
	minMax.SetFloat(vPosition.x, vPosition.y, vPosition.z - 1.0f, vPosition.x, vPosition.y, vPosition.z + 1.0f);

	bool bPossibleIntersection = false;

#ifdef GTA_ENGINE
	CHECK_FOR_THREAD_STALLS(m_DynamicObjectsCriticalSectionToken)
	sysCriticalSection dynamicObjectsCriticalSection(m_DynamicObjectsCriticalSectionToken);
#else
	// Wait until dynamic objects are available
	EnterCriticalSection(&m_DynamicObjectsCriticalSection);
#endif

	TDynamicObject * pCurr = m_pFirstDynamicObject;
	while(pCurr)
	{
		if(!pCurr->m_bIsActive || !pCurr->m_bIsCurrentlyAnObstacle || !minMax.Intersects(pCurr->m_MinMax) ||
			(bIgnoreOpenable && pCurr->m_bIsOpenable) )
		{
			pCurr->m_bPossibleIntersection = false;
		}
		else
		{
			pCurr->m_bPossibleIntersection = true;
			bPossibleIntersection = true;
		}

		pCurr = pCurr->m_pNext;
	}

	if(!bPossibleIntersection)
	{
#ifndef GTA_ENGINE
		LeaveCriticalSection(&m_DynamicObjectsCriticalSection);
#endif
		return true;
	}

	// We will try a number of times to push the point outside of any dynamic objects it is within.
	// If we fail each time (for example there are two objects right next to each other), then
	// we'll have to try another approach to find a nearby safe position.
	//static const float fExtraPushOutAmount = 0.25f;
	static const float fEdgeEpsilon = 0.1f;
	int iNumTries = 4;
	int p; //, prevp;
	Vector3 vObjMin, vObjMax;
	//float fDist1, fDist2, fDist3;
	//Vector3 vSegmentPlanes[4];
	float fEdgePlaneDist;

	while(iNumTries)
	{
		bool bWasInsideAnyDynamicObject = false;

		pCurr = m_pFirstDynamicObject;
		while(pCurr)
		{
			if(pCurr->m_bPossibleIntersection)
			{
				Vector3 vNewPosition = vPosition;
				bool bInsideThisObject = false;

RestartForThisObject:

				float fMinEdgePlaneDist = FLT_MAX;
				s32 iMinEdgePlane = -1;

				// See if we are inside this object at all
				const float* fDynObjPlaneEpsilon = GetDynObjPlaneEpsilon(pCurr);
				for(p=0; p<6; p++)
				{
					fEdgePlaneDist = (pCurr->m_Bounds.m_vEdgePlaneNormals[p].Dot3(vNewPosition) + pCurr->m_Bounds.m_vEdgePlaneNormals[p].w) + fDynObjPlaneEpsilon[p];

					// If we're outside of any edge plane, then we cannot be within the object at all
					if(fEdgePlaneDist > fEdgeEpsilon)
					{
						// Undo any movement on any other planes
						bInsideThisObject = false;
						break;
					}

					if(p < 4 && Abs(fEdgePlaneDist) < fMinEdgePlaneDist)
					{
						fMinEdgePlaneDist = Abs(fEdgePlaneDist);
						iMinEdgePlane = p;
					}
				}
				if(p==6)
				{
					Assert(iMinEdgePlane != -1);

					vNewPosition += pCurr->m_Bounds.m_vEdgePlaneNormals[ iMinEdgePlane ].GetVector3() * (Abs(fMinEdgePlaneDist) + (fEdgeEpsilon*2.0f));

					bInsideThisObject = true;
					goto RestartForThisObject;
				}

				//---------------------------------------------------------------------------------------
				// Set the Position to that transformed, note this may be the original position if the
				// more accurate test found no intersection
				
				vPosition = vNewPosition;

				if( bInsideThisObject )
				{
					bWasInsideAnyDynamicObject = true;
				}
			}

			pCurr = pCurr->m_pNext;
		}

		// If we ended up being clear of all objects, then we can quit
		if(!bWasInsideAnyDynamicObject)
			break;

		iNumTries--;
	}

	// Now let's have one final assertion that we are no longer inside any dynamic objects.
	// If we find that we ARE, then we shall have to employ some other method to move the
	// point into a safe area.  For example, we could find a candidate navmesh poly for this
	// start/end point - and then sample random points within this poly and its neighbours
	// until a clear point is found.  If we still cannot find a clear point, we will have to
	// return false - and the pathfinding will fail.

	float fRandomOffsetAmt = 0.5f;
	static const float fMaxRandOffset = 3.0f;
	s32 iNumTimesToDoLastResortCase = 8;

	Vector3 vAdjustedPosition = vPosition;

RETRY_LAST_RESORT_CASE:

	float fMinZ = vPosition.z - 1.5f;
	float fMaxZ = vPosition.z + 1.5f;

	bool bPositionIsClear = true;

	pCurr = m_pFirstDynamicObject;
	while(pCurr)
	{
		if(!pCurr->m_bIsActive)
		{
			pCurr = pCurr->m_pNext;
			continue;
		}

		// Is this dynamic object anywhere near vPosition ?
		pCurr->m_MinMax.GetAsFloats(vObjMin, vObjMax);

		/*
		vObjMin.x = pCurr->m_Bounds.m_vOrigin.x - pCurr->m_Bounds.m_fBoundingRadius;
		vObjMin.y = pCurr->m_Bounds.m_vOrigin.y - pCurr->m_Bounds.m_fBoundingRadius;
		vObjMin.z = pCurr->m_Bounds.m_vOrigin.z - pCurr->m_Bounds.m_fBoundingRadius;
		vObjMax.x = pCurr->m_Bounds.m_vOrigin.x + pCurr->m_Bounds.m_fBoundingRadius;
		vObjMax.y = pCurr->m_Bounds.m_vOrigin.y + pCurr->m_Bounds.m_fBoundingRadius;
		vObjMax.z = pCurr->m_Bounds.m_vOrigin.z + pCurr->m_Bounds.m_fBoundingRadius;
		*/

		if(vPosition.x < vObjMin.x || vPosition.y < vObjMin.y || vPosition.z < vObjMin.z ||
			vPosition.x > vObjMax.x || vPosition.y > vObjMax.y || vPosition.z > vObjMax.z ||
			fMaxZ < pCurr->m_Bounds.GetBottomZ() || fMinZ > pCurr->m_Bounds.GetTopZ())
		{
			// No bbox intersection
		}
		else
		{
			const float* fDynObjPlaneEpsilon = GetDynObjPlaneEpsilon(pCurr);
			for(p=0; p<4; p++)
			{
				const float fDist1 = (pCurr->m_Bounds.m_vEdgePlaneNormals[p].Dot3(vPosition) + pCurr->m_Bounds.m_vEdgePlaneNormals[p].w) + fDynObjPlaneEpsilon[p];
				// If we're outside of any edge plane, then we cannot be within the object at all
				if(fDist1 > 0.0f)
					break;
			}
			if(p == 4)
			{
				bPositionIsClear = false;

				if(bDoLastResortJittering && iNumTimesToDoLastResortCase > 0)
				{
					iNumTimesToDoLastResortCase--;

					Vector2 vRandomOffset(
						fwRandom::GetRandomNumberInRange(-fRandomOffsetAmt, fRandomOffsetAmt),
						fwRandom::GetRandomNumberInRange(-fRandomOffsetAmt, fRandomOffsetAmt)
					);

					vPosition.x = vAdjustedPosition.x + vRandomOffset.x;
					vPosition.y = vAdjustedPosition.y + vRandomOffset.y;

					if(fRandomOffsetAmt < fMaxRandOffset)
						fRandomOffsetAmt += 0.5f;

					goto RETRY_LAST_RESORT_CASE;
				}

				break;
			}
		}

		pCurr = pCurr->m_pNext;
	}

#ifndef GTA_ENGINE
	LeaveCriticalSection(&m_DynamicObjectsCriticalSection);
#endif

	if(bTestLineOfSight)
	{
		if(!TestNavMeshLOS(vInputPos, vPosition, m_Vars.m_iLosFlags, domain))
		{
			vPosition = vInputPos;
			return false;
		}
	}

	//---------------------------------------------------------------------
	/*
	if( iNumTries == iMaxNumTries )
	{
		pCurr = m_pFirstDynamicObject;
		while(pCurr)
		{
			if( !pCurr->m_bIsActive || !pCurr->m_bPossibleIntersection )
			{
				pCurr = pCurr->m_pNext;
				continue;
			}
			// See if we are inside this object at all
			const float* fDynObjPlaneEpsilon = GetDynObjPlaneEpsilon(pCurr);
			for(p=0; p<6; p++)
			{
				fDist1 = (DotProduct(pCurr->m_Bounds.m_vEdgePlaneNormals[p], vPosition) + pCurr->m_Bounds.m_fEdgePlaneDists[p]) + fDynObjPlaneEpsilon[p];

				// If we're outside of any edge plane, then we cannot be within the object at all
				if(fDist1 > fEdgeEpsilon)
				{
					break;
				}
			}
			// If we're within all 6 planes..
			if(p==6)
			{
				if( pCurr->m_bIsActive && !pCurr->m_bHasUprootLimit )
				{
					pCurr->m_bIsCurrentlyAnObstacle = false;
				}
			}

			pCurr = pCurr->m_pNext;
		}
	}
	*/
	//---------------------------------------------------------------------

	if (!bPositionIsClear)
		vPosition = vInputPos;

	return bPositionIsClear;
}


void CDynamicObjectsGridCell::RecalculateMinMaxOfObjectsInGrid(void)
{
	m_MinMaxOfObjectsInGrid.SetInvalid();

	const TDynamicObject * pObj = m_pFirstDynamicObject;
	while(pObj)
	{
		m_MinMaxOfObjectsInGrid.Union(pObj->m_MinMax);
		pObj = pObj->m_pNextObjInGridCell;
	}
}


void
CDynamicObjectsContainer::Init(void)
{
	Clear();
}

void
CDynamicObjectsContainer::Shutdown(void)
{
	Clear();
}

void
CDynamicObjectsContainer::Clear(void)
{
	for(int i=0; i<ms_DynamicObjectGrids.GetCount(); i++)
	{
		CDynamicObjectsGridCell * pGridCell = ms_DynamicObjectGrids[i];
		Assert(pGridCell);
		delete pGridCell;
	}

	ms_DynamicObjectGrids.Reset();

	ms_bCacheEnabled = false;
}

//**********************************************************************************************
//	AddObjectToGridCell
//	Go through our array of grid-cells and try to find one which the object lies within.
//	If one is not found, then see if we had an empty (unused) one which we can reuse.
//	Failing that, create a new grid cell and add to the array.
//	Finally add the object to the grid cell, and return a pointer to it.
//**********************************************************************************************

CDynamicObjectsGridCell *
CDynamicObjectsContainer::AddObjectToGridCell(TDynamicObject * pObject)
{
	Assert(!pObject->m_pOwningGridCell);
	Assert(!pObject->m_pPrevObjInGridCell);
	Assert(!pObject->m_pNextObjInGridCell);

	Vector3 vObjPos = pObject->m_Bounds.GetOrigin();

	s32 iObjX = MINMAX_FIXEDPT_FROM_FLOAT(vObjPos.x);
	s32 iObjY = MINMAX_FIXEDPT_FROM_FLOAT(vObjPos.y);
	s32 iObjZ = MINMAX_FIXEDPT_FROM_FLOAT(vObjPos.z);

	CDynamicObjectsGridCell * pGridCell;
	CDynamicObjectsGridCell * pOwnerGridCell = NULL;
	CDynamicObjectsGridCell * pFirstEmptyGridCell = NULL;

	for(int i=0; i<ms_DynamicObjectGrids.GetCount(); i++)
	{
		pGridCell = ms_DynamicObjectGrids[i];
		Assert(pGridCell);

		// Keep a record of the first unused gridcell we come across
		if(!pFirstEmptyGridCell && !pGridCell->m_pFirstDynamicObject)
			pFirstEmptyGridCell = pGridCell;

		// Is the object's origin contained within this grid cell?
		if(!pGridCell->m_MinMaxOfGrid.LiesWithin(iObjX, iObjY, iObjZ))
			continue;

		pOwnerGridCell = pGridCell;
		break;
	}

	if(!pOwnerGridCell)
	{
		// Can we reuse this empty grid cell ?
		if(pFirstEmptyGridCell)
		{
			pOwnerGridCell = pFirstEmptyGridCell;
		}
		// Otherwise we'll have to allocate a new grid cell
		else
		{
			// NB (20/2/07) - These allocations are not threadsafe when more than one thread exists on a core (360) & are likely to cause BAD SHIT
			// to happen.  You *must* change this code to use a Pool or something similar asap..
			pOwnerGridCell = rage_new CDynamicObjectsGridCell();

			if(ms_DynamicObjectGrids.GetCount() >= ms_DynamicObjectGrids.GetCapacity())
			{
				ms_DynamicObjectGrids.Grow() = pOwnerGridCell;
			}
			else
			{
				ms_DynamicObjectGrids.Append() = pOwnerGridCell;
			}
		}

		// Set up this grid cell with the appropriate min/max for this cover point
		float fX = (vObjPos.x / CDynamicObjectsGridCell::ms_fSizeOfGrid);
		float fY = (vObjPos.y / CDynamicObjectsGridCell::ms_fSizeOfGrid);
		float fZ = (vObjPos.z / CDynamicObjectsGridCell::ms_fSizeOfGrid);

		s32 iX = (fX >= 0.0f) ? ((s32)fX) : ((s32)(fX-1.0f));
		s32 iY = (fY >= 0.0f) ? ((s32)fY) : ((s32)(fY-1.0f));
		s32 iZ = (fZ >= 0.0f) ? ((s32)fZ) : ((s32)(fZ-1.0f));

		Vector3 vMins, vMaxs;

		vMins.x = ((float) iX) * CDynamicObjectsGridCell::ms_fSizeOfGrid;
		vMins.y = ((float) iY) * CDynamicObjectsGridCell::ms_fSizeOfGrid;
		vMins.z = ((float) iZ) * CDynamicObjectsGridCell::ms_fSizeOfGrid;

		vMaxs.x = vMins.x + CDynamicObjectsGridCell::ms_fSizeOfGrid;
		vMaxs.y = vMins.y + CDynamicObjectsGridCell::ms_fSizeOfGrid;
		vMaxs.z = vMins.z + CDynamicObjectsGridCell::ms_fSizeOfGrid;

		pOwnerGridCell->m_MinMaxOfGrid.SetFloat(vMins.x, vMins.y, vMins.z, vMaxs.x, vMaxs.y, vMaxs.z);
		pOwnerGridCell->m_MinMaxOfObjectsInGrid.SetInvalid();

#if __DEV
#ifdef PATHSERVER_DEBUG_GRIDCELLS
		if(iObjX < pOwnerGridCell->m_MinMaxOfGrid.m_iMinX || iObjY < pOwnerGridCell->m_MinMaxOfGrid.m_iMinY || iObjZ < pOwnerGridCell->m_MinMaxOfGrid.m_iMinZ ||
			iObjX >= pOwnerGridCell->m_MinMaxOfGrid.m_iMaxX || iObjY >= pOwnerGridCell->m_MinMaxOfGrid.m_iMaxY || iObjZ >= pOwnerGridCell->m_MinMaxOfGrid.m_iMaxZ)
		{
			printf("CDynamicObjectsContainer::AddObjectToGridCell()\n");
			printf("Object position was outside of pOwnerGridCell after the new grid was initialised.\n");
			printf("vObjPos = (%.2f, %.2f, %.2f)\n", vObjPos.x, vObjPos.y, vObjPos.z);
			printf("Grid Mins = (%.2f, %.2f, %.2f), Grid Maxs = (%.2f, %.2f, %.2f)\n", vMins.x, vMins.y, vMins.z, vMaxs.x, vMaxs.y, vMaxs.z);
			printf("iX = %i, iY = %i, iZ = %i\n", iX, iY, iZ);

			Assert(0);
		}
#endif
#endif
	}

	// Set up the pointers to insert this object into the cell's linked-list properly
	Assertf(pOwnerGridCell, "TDynamicObject must have a pOwnerGridCell");

	pObject->m_pNextObjInGridCell = pOwnerGridCell->m_pFirstDynamicObject;
	pObject->m_pPrevObjInGridCell = NULL;
	if(pObject->m_pNextObjInGridCell)
		pObject->m_pNextObjInGridCell->m_pPrevObjInGridCell = pObject;

	pOwnerGridCell->m_pFirstDynamicObject = pObject;
	pObject->m_pOwningGridCell = pOwnerGridCell;

	return pOwnerGridCell;
}

void
CDynamicObjectsContainer::RemoveObjectFromGridCell(TDynamicObject * pObject)
{
	Assert(pObject->m_pOwningGridCell);

	// If we're the first one in the grid's list, then we need to update the grid's pointer accordingly
	if(pObject->m_pOwningGridCell->m_pFirstDynamicObject == pObject)
	{
		Assert(!pObject->m_pPrevObjInGridCell);
		pObject->m_pOwningGridCell->m_pFirstDynamicObject = pObject->m_pNextObjInGridCell;
	}

	// Link up prev/next pointers
	if(pObject->m_pPrevObjInGridCell)
		pObject->m_pPrevObjInGridCell->m_pNextObjInGridCell = pObject->m_pNextObjInGridCell;

	if(pObject->m_pNextObjInGridCell)
		pObject->m_pNextObjInGridCell->m_pPrevObjInGridCell = pObject->m_pPrevObjInGridCell;

	// NULL out the pointer to the grid cell which we are in
	pObject->m_pOwningGridCell = NULL;
	pObject->m_pPrevObjInGridCell = NULL;
	pObject->m_pNextObjInGridCell = NULL;
}


void
CDynamicObjectsContainer::RecalculateExtentsOfAllMarkedGrids()
{
	for(s32 i=0; i<ms_DynamicObjectGrids.GetCount(); i++)
	{
		CDynamicObjectsGridCell * pGrid = ms_DynamicObjectGrids[i];
		if(pGrid->m_bMinMaxOfObjectsNeedsRecalculating)
		{
			pGrid->RecalculateMinMaxOfObjectsInGrid();

			pGrid->m_bMinMaxOfObjectsNeedsRecalculating = false;
		}
	}
}

void CDynamicObjectsContainer::AdjustAllBoundsByAmount(const TShortMinMax & UNUSED_PARAM(minMax), const float fRadiusDelta, const bool bInitialAdjustment)
{
	const s32 iFixedAmount = MINMAX_FIXEDPT_FROM_FLOAT(fRadiusDelta);

	for(int c=0; c<ms_DynamicObjectGrids.size(); c++)
	{
		CDynamicObjectsGridCell * pCell = ms_DynamicObjectGrids[c];

		pCell->m_MinMaxOfGrid.m_iMinX -= (s16)iFixedAmount;
		pCell->m_MinMaxOfGrid.m_iMinY -= (s16)iFixedAmount;
		pCell->m_MinMaxOfGrid.m_iMinZ -= (s16)iFixedAmount;
		pCell->m_MinMaxOfGrid.m_iMaxX += (s16)iFixedAmount;
		pCell->m_MinMaxOfGrid.m_iMaxY += (s16)iFixedAmount;
		pCell->m_MinMaxOfGrid.m_iMaxZ += (s16)iFixedAmount;

		pCell->m_MinMaxOfObjectsInGrid.m_iMinX -= (s16)iFixedAmount;
		pCell->m_MinMaxOfObjectsInGrid.m_iMinY -= (s16)iFixedAmount;
		pCell->m_MinMaxOfObjectsInGrid.m_iMinZ -= (s16)iFixedAmount;
		pCell->m_MinMaxOfObjectsInGrid.m_iMaxX += (s16)iFixedAmount;
		pCell->m_MinMaxOfObjectsInGrid.m_iMaxY += (s16)iFixedAmount;
		pCell->m_MinMaxOfObjectsInGrid.m_iMaxZ += (s16)iFixedAmount;

		TDynamicObject * pObj = pCell->m_pFirstDynamicObject;
		while(pObj)
		{
			if(pObj->m_bIsActive &&
				pObj->m_bIsCurrentlyAnObstacle &&
				((bool)pObj->m_bBoundsAdjustedForEntityWidth) != bInitialAdjustment)
			{
				pObj->m_MinMax.m_iMinX -= (s16)iFixedAmount;
				pObj->m_MinMax.m_iMinY -= (s16)iFixedAmount;
				pObj->m_MinMax.m_iMinZ -= (s16)iFixedAmount;
				pObj->m_MinMax.m_iMaxX += (s16)iFixedAmount;
				pObj->m_MinMax.m_iMaxY += (s16)iFixedAmount;
				pObj->m_MinMax.m_iMaxZ += (s16)iFixedAmount;

				pObj->m_bBoundsAdjustedForEntityWidth = bInitialAdjustment;
			}
			pObj = pObj->m_pNextObjInGridCell;
		}
	}
}

void
CDynamicObjectsContainer::ForAllObjectsIntersectingRegion(const TShortMinMax & minMax, PerObjectCB callBackFn, void * pData)
{
	for(s32 i=0; i<ms_DynamicObjectGrids.GetCount(); i++)
	{
		CDynamicObjectsGridCell * pGrid = ms_DynamicObjectGrids[i];
		Assert(!pGrid->m_bMinMaxOfObjectsNeedsRecalculating);

		if(pGrid->m_pFirstDynamicObject && pGrid->m_MinMaxOfObjectsInGrid.Intersects(minMax))
		{
			TDynamicObject * pObj = pGrid->m_pFirstDynamicObject;
			while(pObj)
			{
				if(pObj->m_MinMax.Intersects(minMax))
				{
					if(!callBackFn(pObj, pData))
						return;
				}

				pObj = pObj->m_pNextObjInGridCell;
			}
		}
	}
}

bool
CDynamicObjectsContainer::DoesRegionIntersectAnyObjects(const TShortMinMax & minMax)
{
	for(s32 i=0; i<ms_DynamicObjectGrids.GetCount(); i++)
	{
		CDynamicObjectsGridCell * pGrid = ms_DynamicObjectGrids[i];
		Assert(!pGrid->m_bMinMaxOfObjectsNeedsRecalculating);

		if(pGrid->m_pFirstDynamicObject && pGrid->m_MinMaxOfObjectsInGrid.Intersects(minMax))
		{
			TDynamicObject * pObj = pGrid->m_pFirstDynamicObject;
			while(pObj)
			{
#ifdef GTA_ENGINE
				if(CNavMesh::ms_bUsePrefetching && pObj->m_pNextObjInGridCell)
					PrefetchObject(pObj->m_pNextObjInGridCell);
#endif
				if(pObj->m_MinMax.Intersects(minMax))
					return true;

				pObj = pObj->m_pNextObjInGridCell;
			}
		}
	}
	return false;
}


void CDynamicObjectsContainer::GetObjectsIntersectingRegion(const TShortMinMax & minMax, atArray<TDynamicObject*> & objectsList, const u32 iFlags, const s32 iMaxCount)
{
#if !__FINAL
	CPathServer::ms_iNumGetObjectsIntersectingRegion++;
#endif

	const bool bIgnoreIfOpenable = ((iFlags & TDynamicObject::FLAG_IGNORE_OPENABLE)!=0);
	const bool bIgnoreIfNotAnObstable = ((iFlags & TDynamicObject::FLAG_IGNORE_NOT_OBSTACLE)!=0);

	for(s32 i=0; i<ms_DynamicObjectGrids.GetCount(); i++)
	{
		CDynamicObjectsGridCell * pGrid = ms_DynamicObjectGrids[i];
		Assert(!pGrid->m_bMinMaxOfObjectsNeedsRecalculating);

		if(pGrid->m_pFirstDynamicObject && pGrid->m_MinMaxOfObjectsInGrid.Intersects(minMax))
		{
			TDynamicObject * pObj = pGrid->m_pFirstDynamicObject;
			while(pObj)
			{
#ifdef GTA_ENGINE
				if(CNavMesh::ms_bUsePrefetching && pObj->m_pNextObjInGridCell)
					PrefetchObject(pObj->m_pNextObjInGridCell);
#endif
				if(bIgnoreIfOpenable && pObj->m_bIsOpenable)
				{
					pObj = pObj->m_pNextObjInGridCell;
					continue;
				}
				if(bIgnoreIfNotAnObstable && !pObj->m_bIsCurrentlyAnObstacle)
				{
					pObj = pObj->m_pNextObjInGridCell;
					continue;
				}

				if(pObj->m_MinMax.Intersects(minMax))
				{
					if(objectsList.GetCount() >= iMaxCount)
					{
						return;
					}
					if(objectsList.GetCount()==objectsList.GetCapacity())
					{
						Assertf(false, "CDynamicObjectsContainer::GetObjectsIntersectingRegion() - LIST FULL!\n");
						return;
					}
					objectsList.Append() = pObj;
				}

				pObj = pObj->m_pNextObjInGridCell;
			}
		}
	}
}

void CDynamicObjectsContainer::GetObjectsIntersectingRegionUsingCache(const TShortMinMax & minMax, atArray<TDynamicObject*> & objectsList, const u32 iFlags, const s32 iMaxCount)
{
#if !__FINAL
	CPathServer::ms_iNumGetObjectsIntersectingRegion++;
#endif

	const bool bIgnoreIfOpenable = ((iFlags & TDynamicObject::FLAG_IGNORE_OPENABLE)!=0);
	const bool bIgnoreIfNotAnObstable = ((iFlags & TDynamicObject::FLAG_IGNORE_NOT_OBSTACLE)!=0);

	CDynamicObjectsGridCell * gridCells[CDynamicObjectsContainer::ms_iMaxNumberOfGrids];
	s32 iNumGridCells = CDynamicObjectsContainer::m_GridCellsCache.GetGridCellsIntersectingRegion(minMax, gridCells, CDynamicObjectsContainer::ms_iMaxNumberOfGrids);
	if(iNumGridCells == -1)
	{
		// Region wasn't in the cache (must be reasonably far away from the origin).
		// Fall back to the uncached code path.  Give a NAVMESH_OPTIMISATION_OFF assert, because I'd like to know if this ever happens in regular gameplay.

		NAVMESH_OPTIMISATIONS_OFF_ONLY( Assertf(false, "Region wasn't in cache - falling back to uncached codepath."); )

		GetObjectsIntersectingRegion(minMax, objectsList, iFlags, iMaxCount);
		return;
	}

	for(s32 i=0; i<iNumGridCells; i++)
	{
		CDynamicObjectsGridCell * pGrid = gridCells[i];
		Assert(!pGrid->m_bMinMaxOfObjectsNeedsRecalculating);

		//if(pGrid->m_pFirstDynamicObject && pGrid->m_MinMaxOfObjectsInGrid.Intersects(minMax))
		{
			TDynamicObject * pObj = pGrid->m_pFirstDynamicObject;
			while(pObj)
			{
#ifdef GTA_ENGINE
				if(CNavMesh::ms_bUsePrefetching && pObj->m_pNextObjInGridCell)
					PrefetchObject(pObj->m_pNextObjInGridCell);
#endif
				if(bIgnoreIfOpenable && pObj->m_bIsOpenable)
				{
					pObj = pObj->m_pNextObjInGridCell;
					continue;
				}
				if(bIgnoreIfNotAnObstable && !pObj->m_bIsCurrentlyAnObstacle)
				{
					pObj = pObj->m_pNextObjInGridCell;
					continue;
				}

				if(pObj->m_MinMax.Intersects(minMax))
				{
					if(objectsList.GetCount() >= iMaxCount)
					{
						return;
					}
					if(objectsList.GetCount()==objectsList.GetCapacity())
					{
						Assertf(false, "CDynamicObjectsContainer::GetObjectsIntersectingRegion() - LIST FULL!\n");
						return;
					}
					objectsList.Append() = pObj;
				}

				pObj = pObj->m_pNextObjInGridCell;
			}
		}
	}
}



void CGridCellsCache::Reset(const Vector3 & vOrigin)
{
	const s32 iMaxNum = CGridCellsCache::ms_iNumCellsAcross*CGridCellsCache::ms_iNumCellsAcross;
	memset(m_pEntries, 0, sizeof(CEntry*)*iMaxNum);
	m_iNextFree = 0;

	const Vector3 vExtents(ms_fMaxRange, ms_fMaxRange, ms_fMaxRange);

	Vector3 vOriginRounded;
	vOriginRounded.x = (float) (((s32)(vOrigin.x/ms_fCacheCellSize)) * ms_fCacheCellSize);
	vOriginRounded.y = (float) (((s32)(vOrigin.y/ms_fCacheCellSize)) * ms_fCacheCellSize);
	vOriginRounded.z = (float) (((s32)(vOrigin.z/ms_fCacheCellSize)) * ms_fCacheCellSize);

	m_vMin = vOriginRounded - vExtents;
	m_vMax = vOriginRounded + vExtents;

	m_MinMax.Set(m_vMin, m_vMax);
}

bool CGridCellsCache::Add(CDynamicObjectsGridCell * pGridCell)
{
	Assert(!pGridCell->m_bMinMaxOfObjectsNeedsRecalculating);

	if(!pGridCell->m_pFirstDynamicObject || !m_MinMax.Intersects(pGridCell->m_MinMaxOfObjectsInGrid))
	{
		return true;
	}

	Vector3 vGridMin, vGridMax;
	pGridCell->m_MinMaxOfObjectsInGrid.GetAsFloats(vGridMin, vGridMax);

	const s32 iGridCellMinX = (s32) ((vGridMin.x - m_vMin.x) / ms_fCacheCellSize);
	const s32 iGridCellMinY = (s32) ((vGridMin.y - m_vMin.y) / ms_fCacheCellSize);
	const s32 iGridCellMaxX = (s32) ((vGridMax.x - m_vMin.x) / ms_fCacheCellSize) + 1;
	const s32 iGridCellMaxY = (s32) ((vGridMax.y - m_vMin.y) / ms_fCacheCellSize) + 1;

	if(iGridCellMinX < 0 || iGridCellMinX >= ms_iNumCellsAcross ||
		iGridCellMinY < 0 || iGridCellMinY >= ms_iNumCellsAcross ||
		iGridCellMaxX < 0 || iGridCellMaxX >= ms_iNumCellsAcross ||
		iGridCellMaxY < 0 || iGridCellMaxY >= ms_iNumCellsAcross)
	{
		return true;
	}

	s32 x,y;
	for(x=iGridCellMinX; x<iGridCellMaxX; x++)
	{
		for(y=iGridCellMinY; y<iGridCellMaxY; y++)
		{
#if NAVMESH_OPTIMISATIONS_OFF
			//Assert(m_iNextFree < ms_iNumCacheCells);
#endif
			if(m_iNextFree >= ms_iNumCacheCells)
			{
				// If we have run out of cached slots then we return false
				// The calling code can then turn off the cache for this path request
				return false;
			}

			const s32 iIndex = (y*ms_iNumCellsAcross)+x;
			m_pPool[m_iNextFree].m_pGridCell = pGridCell;
			m_pPool[m_iNextFree].m_pNextVertical = m_pEntries[iIndex];
			m_pEntries[iIndex] = &m_pPool[m_iNextFree];
			m_iNextFree++;
		}
	}
	return true;
}

s32 CGridCellsCache::GetGridCellsIntersectingRegion(const TShortMinMax & region, CDynamicObjectsGridCell ** ppGridCells, const s32 iMaxNumItems)
{
	if(!region.Intersects(m_MinMax))
		return -1;

	static u32 iTimeStamp = 0;
	iTimeStamp++;

	s32 iNumGrids = 0;

	Vector3 vRegionMin, vRegionMax;
	region.GetAsFloats(vRegionMin, vRegionMax);

	s32 iRegionMinX = (s32) ((vRegionMin.x - m_vMin.x) / ms_fCacheCellSize);
	s32 iRegionMinY = (s32) ((vRegionMin.y - m_vMin.y) / ms_fCacheCellSize);
	s32 iRegionMaxX = (s32) ((vRegionMax.x - m_vMin.x) / ms_fCacheCellSize) + 1;
	s32 iRegionMaxY = (s32) ((vRegionMax.y - m_vMin.y) / ms_fCacheCellSize) + 1;

	iRegionMinX = Max(iRegionMinX, 0);
	iRegionMinY = Max(iRegionMinY, 0);
	iRegionMaxX = Min(iRegionMaxX, ms_iNumCellsAcross);
	iRegionMaxY = Min(iRegionMaxY, ms_iNumCellsAcross);

	s32 x,y;
	for(x=iRegionMinX; x<iRegionMaxX; x++)
	{
		for(y=iRegionMinY; y<iRegionMaxY; y++)
		{
			const s32 iIndex = (y*ms_iNumCellsAcross)+x;

			FastAssert(iIndex >= 0 && iIndex < ms_iNumCellsAcross*ms_iNumCellsAcross);

			CEntry * pEntry = m_pEntries[iIndex];
			while(pEntry)
			{
				if(pEntry->m_pGridCell->m_iTimeStamp != iTimeStamp)
				{
					pEntry->m_pGridCell->m_iTimeStamp = iTimeStamp;

					if(!region.Intersects(pEntry->m_pGridCell->m_MinMaxOfObjectsInGrid))
					{
						// Disjoint
					}
					else
					{
						ppGridCells[iNumGrids++] = pEntry->m_pGridCell;
						if(iNumGrids >= iMaxNumItems)
							return iMaxNumItems;
					}
				}
				pEntry = pEntry->m_pNextVertical;
			}
		}
	}
	return iNumGrids;
}

void CDynamicObjectsContainer::InitGridCellsCache(const Vector3 & vSearchOrigin, const TShortMinMax & searchExtents)
{
	ms_bCacheEnabled = true;

	m_GridCellsCache.Reset(vSearchOrigin);

	for(s32 g=0; g<ms_DynamicObjectGrids.GetCount(); g++)
	{
		CDynamicObjectsGridCell * pGrid = ms_DynamicObjectGrids[g];
		if( pGrid->m_MinMaxOfObjectsInGrid.Intersects(searchExtents) )
		{
			if (!m_GridCellsCache.Add(pGrid))
			{
				ms_bCacheEnabled = false;
				break;
			}
		}
	}
}

bool CPathServer::TestPolyIntersectionAgainstObjectPlanes(const TDynamicObject * pObj, const TNavMeshPoly * pPoly, const Vector3 * pPolyVerts)
{
	static Vector3 vClippedVerts1[NAVMESHPOLY_MAX_NUM_VERTICES*4];	// Large array sizes to be on safe side
	static Vector3 vClippedVerts2[NAVMESHPOLY_MAX_NUM_VERTICES*4];

	const Vector3 * pSrcVerts = pPolyVerts;
	Vector3 * pDestVerts = &vClippedVerts2[0];
	int iNumSrcPts = pPoly->GetNumVertices();

	static const float fEps = 0.0f;

	const float* fDynObjPlaneEpsilon = CPathServerThread::GetDynObjPlaneEpsilon(pObj);
	int pl;
	for(pl=0; pl<4; pl++)
	{
		int iNumDestPts = 0;
		int lastv = iNumSrcPts-1;
		float fLastDist = (pObj->m_Bounds.m_vEdgePlaneNormals[pl].Dot3(pSrcVerts[lastv]) + pObj->m_Bounds.m_vEdgePlaneNormals[pl].w) + fDynObjPlaneEpsilon[pl];
		int iLastSign = (fLastDist >= fEps) ? 1 : -1;

		for(int v=0; v<iNumSrcPts; v++)
		{
			float fDist = (pObj->m_Bounds.m_vEdgePlaneNormals[pl].Dot3(pSrcVerts[v]) + pObj->m_Bounds.m_vEdgePlaneNormals[pl].w) + fDynObjPlaneEpsilon[pl];
			int iSign = (fDist >= fEps) ? 1 : -1;

			if(iLastSign < 0)
			{
				pDestVerts[iNumDestPts++] = pSrcVerts[lastv];
			}

			if(iLastSign != iSign)
			{
				// Create vertex at plane, and add to front & back fragments
				float s = (fLastDist / (fLastDist - fDist));
				pDestVerts[iNumDestPts++] = pSrcVerts[lastv] + ((pSrcVerts[v] - pSrcVerts[lastv]) * s);
			}

			lastv = v;
			fLastDist = fDist;
			iLastSign = iSign;
		}

		// Has the poly been clipped to nothing?  If so then it is outside of the object
		if(!iNumDestPts)
		{
			return false;
		}

		iNumSrcPts = iNumDestPts;

		if(pDestVerts == &vClippedVerts2[0])
		{
			pSrcVerts = &vClippedVerts2[0];
			pDestVerts = &vClippedVerts1[0];
		}
		else
		{
			pSrcVerts = &vClippedVerts1[0];
			pDestVerts = &vClippedVerts2[0];
		}
	}

	return true;
}




//-------------------------------------------------------------------------------------------------------
//
//	New tessellation scheme..
//
//-------------------------------------------------------------------------------------------------------




