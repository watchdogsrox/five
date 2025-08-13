#include "PathServer\PathServer.h"

#ifdef GTA_ENGINE
#include "fwsys/timer.h"
#endif

//*******************************************************************
//	These functions are concerned with sampling an axis-aligned and
//	evenly-spaced grid of points from an origin.  We sample whether
//	each grid-cell is deemed 'walkable' or not.
//	This is for use in collision-response tasks such as walking
//	round cars & objects.
//*******************************************************************

void
CPathServerThread::SampleWalkableGrid(CGridRequest * UNUSED_PARAM(pGridRequest))
{
	Assertf(false, "SampleWalkableGrid - currently commented out.\nIf you wish to use the pathserver's grid requests, speak to James.");

#if 0
	Vector3 vOrigin = pGridRequest->m_WalkRndObjGrid.m_vOrigin;

	// Let's see if we have a cached grid at these coordinates.  If so we can
	// save a load of processing time by returning it directly.
	static const float fWalkRndGridCachedPositionEps = 0.25f;
	u32 iTimeMs = fwTimer::GetTimeInMilliseconds();
	if(iTimeMs == 0) iTimeMs = 1;

	u32 g;

	for(g=0; g<WALKRNDOBJGRID_CACHESIZE; g++)
	{
		if(m_WalkRndObjGridCache[g].m_iLastUsedTime != 0)
		{
			if(m_WalkRndObjGridCache[g].m_vOrigin.IsClose(vOrigin, fWalkRndGridCachedPositionEps))
			{
				m_WalkRndObjGridCache[g].m_iLastUsedTime = iTimeMs;

				pGridRequest->m_WalkRndObjGrid.m_fResolution = m_WalkRndObjGridCache[g].m_fResolution;
				pGridRequest->m_WalkRndObjGrid.m_iSize = m_WalkRndObjGridCache[g].m_iSize;
				pGridRequest->m_WalkRndObjGrid.m_iCentreCell = m_WalkRndObjGridCache[g].m_iCentreCell;

				sysMemCpy(pGridRequest->m_WalkRndObjGrid.m_Data, m_WalkRndObjGridCache[g].m_Data, WALKRNDOBJGRID_NUMBYTES);

				pGridRequest->m_iCompletionCode = PATH_FOUND;
				pGridRequest->m_bRequestActive = false;
				pGridRequest->m_bComplete = true;

				return;
			}
		}
	}

	u32 iNavMeshIndex = CPathServer::GetNavMeshIndexFromPosition(vOrigin);

	if(iNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
	{
		pGridRequest->m_iCompletionCode = PATH_INVALID_COORDINATES;
		pGridRequest->m_bComplete = true;
		pGridRequest->m_bRequestActive = false;
		pGridRequest->m_WalkRndObjGrid.SetAllAsWalkable();
		return;
	}

	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iNavMeshIndex);
	if(!pNavMesh)
	{
		pGridRequest->m_iCompletionCode = PATH_NAVMESH_NOT_LOADED;
		pGridRequest->m_bComplete = true;
		pGridRequest->m_bRequestActive = false;
		pGridRequest->m_WalkRndObjGrid.SetAllAsWalkable();
		return;
	}

	float fResolution = pGridRequest->m_WalkRndObjGrid.m_fResolution;
	float fHalfCellSize = fResolution * 0.5f;

	// Get the min/max bounds of the grid area to be sampled
	Vector3 vMin = vOrigin;
	vMin.x -= ((fResolution * pGridRequest->m_WalkRndObjGrid.m_iCentreCell) + fHalfCellSize);
	vMin.y -= ((fResolution * pGridRequest->m_WalkRndObjGrid.m_iCentreCell) + fHalfCellSize);

	Vector3 vMax = vOrigin;
	vMax.x += ((fResolution * pGridRequest->m_WalkRndObjGrid.m_iCentreCell) + fHalfCellSize);
	vMax.y += ((fResolution * pGridRequest->m_WalkRndObjGrid.m_iCentreCell) + fHalfCellSize);

	// Get the poly which is directly underneath the point passed in as the origin
	u32 iPolyIndex = pNavMesh->GetPolyBelowPoint(vOrigin + Vector3(0, 0, 1.0f), 3.0f);
	
	// If we didn't find one exactly below, then try again with a volume test
	if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
	{
		// If we didn't find one exactly below, then try again with a volume test
		TAdjPoly adjPoly = GetClosestPolyBelowPoint(vOrigin, 3.0f);
		iPolyIndex = adjPoly.m_PolyIndex;

		// Couldn't find any nav mesh nearby, so quit.
		if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
		{
			pGridRequest->m_bRequestActive = false;
			pGridRequest->m_bComplete = true;
			pGridRequest->m_iCompletionCode = PATH_NO_SURFACE_AT_COORDINATES;
			pGridRequest->m_WalkRndObjGrid.SetAllAsWalkable();
			return;
		}
	}

	float fResolutionRecip = 1.0f / fResolution;

	// Gather the polys from the CNavMeshes, by flood-filling outwards from the origin and 
	// quitting once we've exceeded the bounds of the vMin/vMax bounding box.
	IncTimeStamp();

	m_iNumWalkableGridPolys = 0;

	TNavMeshPoly * pStartPoly = &pNavMesh->m_Polys[iPolyIndex];

#if !__FINAL && !__PPU
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	// This is the recursive function call which flood-fills the walkable polys
	SampleWalkableGrid_FloodFillWalkablePolys(pNavMesh, pStartPoly, vMin, vMax);

#if !__FINAL && !__PPU
	m_PerfTimer->Stop();
	pGridRequest->m_fMillisecsToFindPolys = (float) m_PerfTimer->GetTimeMS();
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	// Now go through each poly, and find it's max/max.  We then sample it at each
	// grid-cell to determine whether this cell is walkable.
	// First, lets set all the cells to be non-walkable.
	pGridRequest->m_WalkRndObjGrid.SetAllAsNotWalkable();

	Vector3 vPolyMin, vPolyMax, vVertex, vecIntersect;
	Vector3 vPolyVerts[NAVMESHPOLY_MAX_NUM_VERTICES];
	float x,y;

	for(u32 p=0; p<m_iNumWalkableGridPolys; p++)
	{
		TNavMeshPoly * pPoly = m_VisitedPolys[p];
		CNavMesh * pPolyNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->m_iNavMeshIndex);
		if(!pPolyNavMesh)
			continue;

		vPolyMin.x = FLT_MAX;
		vPolyMin.y = FLT_MAX;
		vPolyMin.z = FLT_MAX;
		vPolyMax.x = -FLT_MAX;
		vPolyMax.y = -FLT_MAX;
		vPolyMax.z = -FLT_MAX;

		for(u32 v=0; v<pPoly->m_iNumVertices; v++)
		{
			pPolyNavMesh->GetVertex( pPolyNavMesh->GetPolyVertexIndex(pPoly, v), vVertex );
			vPolyVerts[v] = vVertex;

			if(vVertex.x < vPolyMin.x) vPolyMin.x = vVertex.x;
			if(vVertex.y < vPolyMin.y) vPolyMin.y = vVertex.y;
			if(vVertex.x > vPolyMax.x) vPolyMax.x = vVertex.x;
			if(vVertex.y > vPolyMax.y) vPolyMax.y = vVertex.y;

			// We need to record the min/max value, so we know where to start & end our ray tests from..
			if(vVertex.z < vPolyMin.z) vPolyMin.z = vVertex.z;
			if(vVertex.z > vPolyMax.z) vPolyMax.z = vVertex.z;
		}

		// Clamp the extents to the bounds of the grid's bbox
		vPolyMin.x = rage::Max(vPolyMin.x, vMin.x);
		vPolyMin.y = rage::Max(vPolyMin.y, vMin.y);
		vPolyMax.x = rage::Min(vPolyMax.x, vMax.x);
		vPolyMax.y = rage::Min(vPolyMax.y, vMax.y);

		vPolyMin.z -= 1.0f;
		vPolyMax.z += 1.0f;

		// Make sure that all the points we sample are evenly spaced at fResolution, and starting from vMin
		float fDiffX = vPolyMin.x - vMin.x;
		float fDiffY = vPolyMin.y - vMin.y;

		fDiffX = ((int) (fDiffX / fResolution)) * fResolution;
		fDiffY = ((int) (fDiffY / fResolution)) * fResolution;

		vPolyMin.x = vMin.x + fDiffX;
		vPolyMin.y = vMin.y + fDiffY;


		for(y=vPolyMin.y; y<=vPolyMax.y; y+=fResolution)
		{
			// It's possible for the polys to be (sometimes considerably) outside of the
			// bbox which we're interested in sampling, so let's do another early-out test
			// here before we go doing the costly ray intersection..
			if(y < vMin.y || y > vMax.y)
			{
				continue;
			}

			for(x=vPolyMin.x; x<=vPolyMax.x; x+=fResolution)
			{
				// And another early-out test on the x-coord
				if(x < vMin.x || x > vMax.x)
				{
					continue;
				}

				// Intersect this sample plot with the poly to see if the surface is walkable here
				if(pPolyNavMesh->RayIntersectsPoly(
					Vector3(x,y,vPolyMax.z),
					Vector3(x,y,vPolyMin.z),
					pPoly->m_iNumVertices,
					vPolyVerts,
					vecIntersect))
				{
					int ix = (int) ((x - vOrigin.x) * fResolutionRecip);
					int iy = (int) ((y - vOrigin.y) * fResolutionRecip);

					pGridRequest->m_WalkRndObjGrid.SetWalkable(ix, iy);
				}
			}
		}
	}

#if !__FINAL && !__PPU
	m_PerfTimer->Stop();
	pGridRequest->m_fMillisecsToSampleGrid = (float) m_PerfTimer->GetTimeMS();
#endif

	// Store the results for this grid in our cache, using a LRU scheme
	u32 iLargestDeltaMs = 0;
	s32 iBestCacheSlot = -1;

	for(g=0; g<WALKRNDOBJGRID_CACHESIZE; g++)
	{
		u32 iDeltaMs = iTimeMs - m_WalkRndObjGridCache[g].m_iLastUsedTime;

		if(iDeltaMs > iLargestDeltaMs)
		{
			iLargestDeltaMs = iDeltaMs;
			iBestCacheSlot = g;
		}
	}
	// Use the cache slot which had the greatest delta time in milliseconds since its last use
	if(iBestCacheSlot != -1)
	{
		m_WalkRndObjGridCache[iBestCacheSlot].m_iLastUsedTime = iTimeMs;

		m_WalkRndObjGridCache[iBestCacheSlot].m_vOrigin = vOrigin;
		m_WalkRndObjGridCache[iBestCacheSlot].m_fResolution = pGridRequest->m_WalkRndObjGrid.m_fResolution;
		m_WalkRndObjGridCache[iBestCacheSlot].m_iSize = pGridRequest->m_WalkRndObjGrid.m_iSize;
		m_WalkRndObjGridCache[iBestCacheSlot].m_iCentreCell = pGridRequest->m_WalkRndObjGrid.m_iCentreCell;

		sysMemCpy(m_WalkRndObjGridCache[iBestCacheSlot].m_Data, pGridRequest->m_WalkRndObjGrid.m_Data, WALKRNDOBJGRID_NUMBYTES);
	}

	pGridRequest->m_iCompletionCode = PATH_FOUND;
	pGridRequest->m_bRequestActive = false;
	pGridRequest->m_bComplete = true;
#endif
}

void
CPathServerThread::SampleWalkableGrid_FloodFillWalkablePolys(CNavMesh * UNUSED_PARAM(pNavMesh), TNavMeshPoly * UNUSED_PARAM(pPoly), const Vector3 & UNUSED_PARAM(vMin), const Vector3 & UNUSED_PARAM(vMax))
{
#if 0
	pPoly->m_TimeStamp = m_iNavMeshPolyTimeStamp;

	if(m_iNumWalkableGridPolys < MAX_PATH_VISITED_POLYS)
	{
		m_VisitedPolys[m_iNumWalkableGridPolys++] = pPoly;
	}
	else
	{
		// We've reached the maximum number of polys we can floodfill for this purpose (MAX_PATH_VISITED_POLYS)
		return;
	}

	u32 a,v;
	Vector3 vec, vPolyMin, vPolyMax;
	TAdjPoly adjPoly;
	CNavMesh * pAdjNavMesh;
	TNavMeshPoly * pAdjacentPoly;

	for(a=0; a<pPoly->m_iNumVertices; a++)
	{
		pNavMesh->GetAdjacentPoly(pPoly, a, adjPoly);
		if(adjPoly.m_NavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE && adjPoly.m_PolyIndex != NAVMESH_POLY_INDEX_NONE)
		{
			pAdjNavMesh = CPathServer::GetNavMeshFromIndex(adjPoly.m_NavMeshIndex);

			if(!pAdjNavMesh)
			{
				continue;
			}

			pAdjacentPoly = &pAdjNavMesh->m_Polys[ adjPoly.m_PolyIndex ];
			if(pAdjacentPoly->m_TimeStamp != m_iNavMeshPolyTimeStamp)
			{
				vPolyMin.x = FLT_MAX;
				vPolyMin.y = FLT_MAX;
				vPolyMax.x = -FLT_MAX;
				vPolyMax.y = -FLT_MAX;

				// Only flood-fill into this poly if it is not outside the bbox bounds
				for(v=0; v<pAdjacentPoly->m_iNumVertices; v++)
				{
					pAdjNavMesh->GetVertex(pAdjNavMesh->GetPolyVertexIndex(pAdjacentPoly, v), vec);

					if(vec.x < vPolyMin.x) vPolyMin.x = vec.x;
					if(vec.y < vPolyMin.y) vPolyMin.y = vec.y;
					if(vec.x > vPolyMax.x) vPolyMax.x = vec.x;
					if(vec.y > vPolyMax.y) vPolyMax.y = vec.y;
				}

				// No bbox intersection with grid bounds ?
				if(vPolyMax.x < vMin.x || vPolyMax.y < vMin.y || vPolyMin.x > vMax.x || vPolyMin.y > vMax.y)
				{
					continue;
				}

				SampleWalkableGrid_FloodFillWalkablePolys(pAdjNavMesh, pAdjacentPoly, vMin, vMax);
			}
		}
	}
#endif
}


