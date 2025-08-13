#include "PathServer\PathServer.h"

#ifdef GTA_ENGINE
#include "task\Combat\Cover\Cover.h"
#include "scene/world/GameWorld.h"
#include "fwsys/timer.h"

NAVMESH_OPTIMISATIONS()

//****************************************************************************
//	AddCoverPoints
//	This is the main interface function from the main game, which adds
//	coverpoints to the CCover class.  Is operates on the 'm_CoverPointsBuffer'
//	rather than directly on any navmeshes, in order to eliminate any thread-
//	stalls to the main game thread.
//****************************************************************************

void
CPathServer::AddCoverPoints(const Vector3 & UNUSED_PARAM(vOrigin), float UNUSED_PARAM(fMaxDistance), int maxNumPointsToAdd)
{
	// If there's no coverpoints waiting for us, then return straight away
	if(m_iNumCoverPointsInBuffer == 0)
		return;

	// Check 'm_bCoverPointsBufferInUse' to make sure the CPathServerThread has finished
	// doing its stuff with the buffer.
	// NB : shouldn't we be using a critical section again here.  We could *test* the critical
	// section rather than waiting on it..
	if(m_bCoverPointsBufferInUse)
	{
		return;
	}
	m_bCoverPointsBufferInUse = true;

	// Add coverpoints
	Vector3 vCoverVertex;

	// Start at the tail end of the contents and read as many points as we can this frame
	// Eventually we will empty the contents
	s32 iStartIndex = Max(0, m_iNumCoverPointsInBuffer - maxNumPointsToAdd);
	
	for(s32 c=iStartIndex; c < m_iNumCoverPointsInBuffer; c++)
	{
		TCachedCoverPoint & coverPoint = m_CoverPointsBuffer[c];

		vCoverVertex.x = coverPoint.m_fXPos;
		vCoverVertex.y = coverPoint.m_fYPos;
		vCoverVertex.z = coverPoint.m_fZPos;

		if(!CCover::IsPointWithinValidArea(vCoverVertex,CCoverPoint::COVTYPE_POINTONMAP))
		{
			continue;
		}

		if(coverPoint.m_CoverPointFlags.GetCoverDir() == 255)
		{
			coverPoint.m_CoverPointFlags.SetCoverDir(254);
		}

		CCoverPoint::eCoverUsage coverUsage = CCoverPoint::COVUSE_WALLTOBOTH;
		CCoverPoint::eCoverHeight coverHeight = CCoverPoint::COVHEIGHT_LOW;

		// DEBUG-PH
		// Convert the older style cover usage type into the new cover usage and cover height
		if(coverPoint.m_CoverPointFlags.GetCoverType() == NAVMESH_COVERPOINT_LOW_WALL)
		{
			coverUsage = CCoverPoint::COVUSE_WALLTOBOTH;
			coverHeight = CCoverPoint::COVHEIGHT_LOW;
		}
		else if(coverPoint.m_CoverPointFlags.GetCoverType() == NAVMESH_COVERPOINT_LOW_WALL_TO_LEFT)
		{
			coverUsage = CCoverPoint::COVUSE_WALLTOLEFT;
			coverHeight = CCoverPoint::COVHEIGHT_LOW;
		}
		else if(coverPoint.m_CoverPointFlags.GetCoverType() == NAVMESH_COVERPOINT_LOW_WALL_TO_RIGHT)
		{
			coverUsage = CCoverPoint::COVUSE_WALLTORIGHT;
			coverHeight = CCoverPoint::COVHEIGHT_LOW;
		}
		else if(coverPoint.m_CoverPointFlags.GetCoverType() == NAVMESH_COVERPOINT_WALL_TO_LEFT)
		{
			coverUsage = CCoverPoint::COVUSE_WALLTOLEFT;
			coverHeight = CCoverPoint::COVHEIGHT_TOOHIGH;
		}
		else if(coverPoint.m_CoverPointFlags.GetCoverType() == NAVMESH_COVERPOINT_WALL_TO_RIGHT)
		{
			coverUsage = CCoverPoint::COVUSE_WALLTORIGHT;
			coverHeight = CCoverPoint::COVHEIGHT_TOOHIGH;
		}
		else if(coverPoint.m_CoverPointFlags.GetCoverType() == NAVMESH_COVERPOINT_WALL_TO_NEITHER)
		{
			coverUsage = CCoverPoint::COVUSE_WALLTONEITHER;
			coverHeight = CCoverPoint::COVHEIGHT_TOOHIGH;
		}
		else
		{
			Assert(0);
		}
		// END DEBUG

		/*s16 iCoverPoint =*/ CCover::AddCoverPoint(
			CCover::CP_NavmeshAndDynamicPoints,
			CCoverPoint::COVTYPE_POINTONMAP,
			NULL,
			&vCoverVertex,
			coverHeight,
			coverUsage,
			(u8)coverPoint.m_CoverPointFlags.GetCoverDir(),
			CCoverPoint::COVARC_90
		);
	}

	// Update the number of coverpoints remaining in the buffer
	m_iNumCoverPointsInBuffer = iStartIndex;

	// Mustn't forget to release the buffer again..
	m_bCoverPointsBufferInUse = false;
}




// This examines the cover-points stored in each quadtree leaf, and adds them if within range
bool
CPathServer::ExtractCoverPointsFromQuadtree(CNavMeshQuadTree * pTree, const Vector3 & vMin, const Vector3 & vMax, const Vector3 & vNavMeshMin, const Vector3 & vNavMeshSize)
{
	if(pTree->m_pLeafData)
	{
		int iStartingCoverPoint = 0;

		// Do we have a leaf node to continue our algorithm from ?
		if(m_pFindCover_ContinueFromThisQuadTreeLeaf)
		{
			// If we do, and it is not this leaf node - then continue until we find it
			if(pTree != m_pFindCover_ContinueFromThisQuadTreeLeaf)
			{
				return true;
			}
			// We've found the leaf node - so NULL it out and continue our algorithm from this point
			else
			{
				m_pFindCover_ContinueFromThisQuadTreeLeaf = NULL;
				iStartingCoverPoint = m_iFindCover_NavMeshPolyProgress;
			}
		}

		//************************************************************************************
		//	Water-Edges

		if((m_bExtractWaterEdgesThisTimeslice && ((m_pFindCoverCurrentNavMesh->GetFlags() & NAVMESH_HAS_WATER)!=0) &&
			m_iNumWaterEdgesInBuffer >= SIZE_OF_WATEREDGES_BUFFER) ||
			m_vFindWaterEdgesMin.x > vMax.x || m_vFindWaterEdgesMin.y > vMax.y || m_vFindWaterEdgesMin.z > vMax.z ||
			vMin.x > m_vFindWaterEdgesMax.x || vMin.y > m_vFindWaterEdgesMax.y || vMin.z > m_vFindWaterEdgesMax.z)
		{
			// No intersection.  I find it easier to visualise the logic like this!
		}
		else
		{
			u32 p,v;

			const float fMaxDistSqr = ms_fFindWaterEdgesMaxDist*ms_fFindWaterEdgesMaxDist;

			for(p=0; p<pTree->m_pLeafData->m_iNumPolys; p++)
			{
				const u16 iPolyIndex = pTree->m_pLeafData->m_Polys[p];
				const TNavMeshPoly & poly = *m_pFindCoverCurrentNavMesh->GetPoly(iPolyIndex);

				if(poly.GetIsWater() && m_FindWaterEdgesMinMax.Intersects(poly.m_MinMax))
				{
					for(v=0; v<poly.GetNumVertices(); v++)
					{
						const TAdjPoly & adjPoly = m_pFindCoverCurrentNavMesh->GetAdjacentPoly(poly.GetFirstVertexIndex() + v);
						if(adjPoly.GetPolyIndex()==NAVMESH_POLY_INDEX_NONE)
						{
							// This is a water edge.
							break;			
						}
					}
					if(v != poly.GetNumVertices())
					{
						const int v2 = (v+1) % poly.GetNumVertices();
						Vector3 vVert1, vVert2;
						m_pFindCoverCurrentNavMesh->GetVertex( m_pFindCoverCurrentNavMesh->GetPolyVertexIndex(&poly, v), vVert1 );
						m_pFindCoverCurrentNavMesh->GetVertex( m_pFindCoverCurrentNavMesh->GetPolyVertexIndex(&poly, v2), vVert2 );
						const Vector3 vMidEdge = (vVert1+vVert2)*0.5f;

						const Vector3 vDiffFromOrigin = vMidEdge - m_vFindWaterEdgesOrigin;
						if(vDiffFromOrigin.Mag2() < fMaxDistSqr)
						{
							AddWaterEdgeToBuffer( vMidEdge, poly.GetAudioProperties() );
							if(m_iNumWaterEdgesInBuffer >= SIZE_OF_WATEREDGES_BUFFER)
								break;
						}
					}
				}
			}
		}

		//************************************************************************************
		//	Coverpoints

		Vector3 vCoverVertex;
		Vector3 vDiff;

		for(int c=iStartingCoverPoint; c<pTree->m_pLeafData->m_iNumCoverPoints; c++)
		{
			CNavMeshCoverPoint & coverPoint = pTree->m_pLeafData->m_CoverPoints[c];
			if(coverPoint.m_CoverPointFlags.GetIsDisabled())
				continue;

			CNavMesh::DecompressVertex(
				vCoverVertex,
				coverPoint.m_iX,
				coverPoint.m_iY,
				coverPoint.m_iZ,
				vNavMeshMin, vNavMeshSize
			);

			// I'm not sure whether its a good idea to perform the cover-area containment
			// tests here or not.  They will be repeated when we do the AddCoverPoints()
			// call anyhow.  Lets leave it in for now for completeness, and maybe remove
			// it later if it is redundant.

			for(u32 a=0; a<m_iCoverNumberAreas; a++)
			{
				if(m_CoverBoundingAreas[a].ContainsPoint(vCoverVertex))
				{
					m_CoverPointsBuffer[m_iNumCoverPointsInBuffer].m_fXPos = vCoverVertex.x;
					m_CoverPointsBuffer[m_iNumCoverPointsInBuffer].m_fYPos = vCoverVertex.y;
					m_CoverPointsBuffer[m_iNumCoverPointsInBuffer].m_fZPos = vCoverVertex.z;
					m_CoverPointsBuffer[m_iNumCoverPointsInBuffer].m_CoverPointFlags = coverPoint.m_CoverPointFlags;
					m_iNumCoverPointsInBuffer++;
					m_iFindCover_NumIterationsRemainingThisTimeslice--;

					if(m_iFindCover_NumIterationsRemainingThisTimeslice == 0 || m_iNumCoverPointsInBuffer == SIZE_OF_COVERPOINTS_BUFFER)
					{
						// Store the information that we need to be able to restart from this point next timeslice..
						m_pFindCover_ContinueFromThisQuadTreeLeaf = pTree;
						m_iFindCover_NavMeshPolyProgress = c;

						return false;
					}

					break;
				}
			}
		}

		return true;
	}

	for(int c=0; c<4; c++)
	{
		if(m_iFindCover_NumIterationsRemainingThisTimeslice == 0 || m_iNumCoverPointsInBuffer == SIZE_OF_COVERPOINTS_BUFFER)
			return false;

		if(pTree->m_pChildren[c]->m_Mins.x > vMax.x || pTree->m_pChildren[c]->m_Mins.y > vMax.y ||
			vMin.x > pTree->m_pChildren[c]->m_Maxs.x || vMin.y > pTree->m_pChildren[c]->m_Maxs.y)
		{
			// No overlap
		}
		else
		{
			if(!ExtractCoverPointsFromQuadtree(pTree->m_pChildren[c], vMin, vMax, vNavMeshMin, vNavMeshSize))
			{
				return false;
			}
		}
	}

	return true;
}

// Set the origin of the position to scan for water-edges
void
CPathServer::SetWaterEdgeParams(const Vector3 & vOrigin)
{
	if(m_iNumCoverPointsInBuffer)
		return;

	m_vFindWaterEdgesOrigin = vOrigin;
}

bool
CPathServer::GetWaterEdgePoints(int & iOutNumPts, Vector3 * pOutPoints, u32 * pOutPerPolyAudioProperties)
{
	if(m_bCoverPointsBufferInUse)
	{
		iOutNumPts = 0;
		return false;
	}
	m_bCoverPointsBufferInUse = true;

	iOutNumPts = m_iNumWaterEdgesInBuffer;

	int v;
	for(v=0; v<m_iNumWaterEdgesInBuffer; v++)
	{
		pOutPoints[v] = m_WaterEdgesBuffer[v];
	}
	if(pOutPerPolyAudioProperties)
	{
		for(v=0; v<m_iNumWaterEdgesInBuffer; v++)
		{
			pOutPerPolyAudioProperties[v] = m_WaterEdgeAudioProperties[v];

		}
	}

	m_bCoverPointsBufferInUse = false;

	return true;
}

// Add a point to the buffer, testing tolerance
bool
CPathServer::AddWaterEdgeToBuffer(const Vector3 & vMidEdge, u32 iAudioProperties)
{
	if(m_iNumWaterEdgesInBuffer >= SIZE_OF_WATEREDGES_BUFFER)
		return false;

	for(int v=0; v<m_iNumWaterEdgesInBuffer; v++)
	{
		const Vector3 vDiff = m_WaterEdgesBuffer[v] - vMidEdge;
		if(vDiff.Mag2() < ms_fMinDistSqrBetweenWaterEdgePoints)
			return false;
	}

	m_WaterEdgesBuffer[m_iNumWaterEdgesInBuffer] = vMidEdge;
	m_WaterEdgeAudioProperties[m_iNumWaterEdgesInBuffer] = iAudioProperties;
	m_iNumWaterEdgesInBuffer++;
	return true;
}

// Clear out the edges which are too far away
void CPathServer::ClearWaterEdges()
{
	const float fMaxDistSqr = ms_fFindWaterEdgesMaxDist*ms_fFindWaterEdgesMaxDist;

	for(int v=0; v<m_iNumWaterEdgesInBuffer; v++)
	{
		const Vector3 vDiff = m_WaterEdgesBuffer[v] - m_vFindWaterEdgesOrigin;

		// Is this edge is now outside of our max range?
		if(vDiff.Mag2() > fMaxDistSqr)
		{
			for(int v2=v; v2<m_iNumWaterEdgesInBuffer; v2++)
			{
				m_WaterEdgesBuffer[v2] = m_WaterEdgesBuffer[v2+1];
				m_WaterEdgeAudioProperties[v2] = m_WaterEdgeAudioProperties[v2+1];
			}

			v--;
			m_iNumWaterEdgesInBuffer--;
			if(m_iNumWaterEdgesInBuffer==0)
				break;
		}
	}
}

//***********************************************************************************
//	MaybeExtractCoverPointsFromNavMeshes
//	This function is only ever called by the CPathServerThread::Run() function.
//	Its purpose is to incrementally run the algorithm which visits all the navmeshes
//	near the origin, and extracts coverpoints into a buffer for subsequent use by
//	the main game.  In this way we should be able to avoid stalling the main game,
//	and the CPathServer thread (and in turn potentially the streaming threads) since
//	the coverpoint extraction can take a relatively long time in dense maps. (100ms+)
//***********************************************************************************

void CPathServer::MaybeExtractCoverPointsFromNavMeshes()
{
	Vector3 vOrigin = m_vOrigin;
	float fMaxDistance = CCover::ms_fMaxCoverPointDistance;

	// Has enough time elapsed for us to do another timeslice of the algorithm ?
	const u32 iTimeMs = fwTimer::GetTimeInMilliseconds();
	const u32 iDeltaMs = iTimeMs - m_iTimeOfLastCoverPointTimeslice_Millisecs;
	if(iDeltaMs < m_iFrequencyOfCoverPointTimeslices_Millisecs || m_PathServerThread.m_bForcePerformThreadHousekeeping)
		return;

	// If there are coverpoints left in the buffer, then the game thread has not yet grabbed the last lot.
	// If the buffer is locked, then the game thread is grabbing the coverpoints right now.
	if(m_iNumCoverPointsInBuffer)
		return;

	// Fairly gay way of stopping this stuff from updating whilst the main thread is doing the
	// very lightweight task of retrieving the extracted data.  TODO: make this use a critical
	// section or spinlock..
	while(m_bCoverPointsBufferInUse)
	{
		sysIpcYield(1);
	}

	m_bCoverPointsBufferInUse = true;

	m_iTimeOfLastCoverPointTimeslice_Millisecs = iTimeMs;

	//****************************************************************************************
	//	Water-Edges; these are now extracted at the same time as coverpoints..
	//	Set up the minmax struct for which area of the navmesh to extract water-edges from.
	//	This will probably need to have its own origin rather than using the m_vOrigin which
	//	is normally the player.

	const u32 iWaterEdgesDeltaMs = iTimeMs - m_iTimeOfLastWaterEdgeTimeslice_Millisecs;
	if(iWaterEdgesDeltaMs > m_iFrequencyOfWaterEdgeExtraction_Millisecs)
	{
		m_bExtractWaterEdgesThisTimeslice = true;
		m_iTimeOfLastWaterEdgeTimeslice_Millisecs = iTimeMs;

		Vector3 vWaterDist(ms_fFindWaterEdgesMaxDist, ms_fFindWaterEdgesMaxDist, ms_fFindWaterEdgesMaxDist);
		m_vFindWaterEdgesMin = m_vFindWaterEdgesOrigin - vWaterDist;
		m_vFindWaterEdgesMax = m_vFindWaterEdgesOrigin + vWaterDist;

		m_FindWaterEdgesMinMax.SetFloat(
			m_vFindWaterEdgesMin.x, m_vFindWaterEdgesMin.y, m_vFindWaterEdgesMin.z,
			m_vFindWaterEdgesMax.x, m_vFindWaterEdgesMax.y, m_vFindWaterEdgesMax.z
		);

		// Clear water edges which are now outside of the max range
		ClearWaterEdges();
	}
	else
	{
		m_bExtractWaterEdgesThisTimeslice = false;
	}


#if !__FINAL
	m_ExtractCoverPointsTimer->Reset();
	m_ExtractCoverPointsTimer->Start();
#endif

	// If the navmeshes have been loaded/unloaded, then it will have invalidated our timeslicing approach &
	// we'll need to start again.  Similarly, if we completed the timesliced algorithm last time - then we
	// should start over.
	if(m_bFindCover_NavMeshesHaveChanged || m_bFindCover_CompletedTimeslice)
	{
		// This just sets up the timeslicing data, etc
		ExtractCoverPoints_StartNewTimeslice(vOrigin, fMaxDistance);
	}

	// Run the algorithm for one iteration..
	ExtractCoverPoints_ContinueTimeslice(vOrigin, fMaxDistance);

#if !__FINAL
	m_ExtractCoverPointsTimer->Stop();
	m_fLastTimeTakenToExtractCoverPointsMs = (float) m_ExtractCoverPointsTimer->GetTimeMS();
#endif

	m_bCoverPointsBufferInUse = false;
}

#if !__FINAL
void
CPathServer::SpewOutCoverAreasRequired()
{
	printf("Oh dear, we ran out of cover areas again.. Please copy the info below :\n\n");

	printf("m_iCoverNumberAreas : %i\n", m_iCoverNumberAreas);

	Vector3 vAreaMins, vAreaMaxs;
	u32 a,n;

	// Spew out the cover areas
	for(a=0; a<m_iCoverNumberAreas; a++)
	{
		m_CoverBoundingAreas[a].GetMin(vAreaMins);
		m_CoverBoundingAreas[a].GetMax(vAreaMaxs);

		printf("  %i) vMin(%.1f,%.1f,%.1f) vMax(%.1f,%.1f,%.1f)\n", a, vAreaMins.x, vAreaMins.y, vAreaMins.z, vAreaMaxs.x, vAreaMaxs.y, vAreaMaxs.z);
	}

	printf("m_iFindCover_NumNavMeshesIndices : %i\n", m_iFindCover_NumNavMeshesIndices);
	printf("> ");

	// Spew out the navmesh indices
	for(n=0; n<m_iFindCover_NumNavMeshesIndices; n++)
	{
		TNavMeshIndex index = m_iFindCover_NavMeshesIndices[n];
		printf("%i, ", index);
	}

	printf("\n\n");
}
#endif

// Called to add cover points near the origin to the global CCover class
void
CPathServer::ExtractCoverPoints_StartNewTimeslice(const Vector3 & UNUSED_PARAM(vOrigin), float UNUSED_PARAM(fMaxDistance))
{
	// Cover extraction only takes place in the regular nav domain for now.
	const aiNavDomain domain = kNavDomainRegular;

	static const float fSectorWidth = CPathServerExtents::GetWorldWidthOfSector();
	float fNavMeshWidth = ((float)m_pNavMeshStores[domain]->GetNumSectorsPerMesh()) * fSectorWidth;

	m_iFindCover_NumNavMeshesIndices = 0;

	Vector3 vAreaMins, vAreaMaxs;
	u32 a,n;
	TNavMeshIndex iNavMeshIndex;
	Vector3 vPos(0,0,0);

	u32 iNumAreas = m_iCoverNumberAreas;

	// Only process the water-edges if the origin has moved since last time.
	//const Vector3 vWaterEdgeOriginDiff = m_vFindWaterEdgesOrigin - m_vFindWaterEdgesLastOrigin;
	//if(vWaterEdgeOriginDiff.Mag2() > 16.0f)
	if(m_bExtractWaterEdgesThisTimeslice)
	{
		iNumAreas++;
	}

	// Go through all the cover areas
	for(a=0; a<iNumAreas; a++)
	{
		if(a==m_iCoverNumberAreas)
		{
			vAreaMins = m_vFindWaterEdgesMin;
			vAreaMaxs = m_vFindWaterEdgesMax;
		}
		else
		{
			m_CoverBoundingAreas[a].GetMin(vAreaMins);
			m_CoverBoundingAreas[a].GetMax(vAreaMaxs);
		}

		// We might need to increase the maxs on this area, unless they start & end within the same navmesh
		// This is so that we process the navmesh column/row which the end coords are within
		if(((int)(vAreaMins.x / fNavMeshWidth) != (int)(vAreaMaxs.x / fNavMeshWidth)) ||
			(Abs(vAreaMaxs.x - vAreaMins.x) < fNavMeshWidth && Sign(vAreaMins.x) != Sign(vAreaMaxs.x)))
			vAreaMaxs.x += fNavMeshWidth;
		if(((int)(vAreaMins.y / fNavMeshWidth) != (int)(vAreaMaxs.y / fNavMeshWidth)) ||
			(Abs(vAreaMaxs.y - vAreaMins.y) < fNavMeshWidth && Sign(vAreaMins.y) != Sign(vAreaMaxs.y)))
			vAreaMaxs.y += fNavMeshWidth;
		if(((int)(vAreaMins.z / fNavMeshWidth) != (int)(vAreaMaxs.z / fNavMeshWidth)) ||
			(Abs(vAreaMaxs.z - vAreaMins.z) < fNavMeshWidth && Sign(vAreaMins.z) != Sign(vAreaMaxs.z)))
			vAreaMaxs.z += fNavMeshWidth;

		// Step across this cover area, and find all the navmeshes intersecting it
		for(vPos.y=vAreaMins.y; vPos.y<vAreaMaxs.y; vPos.y+=fNavMeshWidth)
		{
			for(vPos.x=vAreaMins.x; vPos.x<vAreaMaxs.x; vPos.x+=fNavMeshWidth)
			{
				// Get the navmesh at this position
				iNavMeshIndex = GetNavMeshIndexFromPosition(vPos, domain);

				// Make sure the index is valid (ie. navmesh is on the map).
				// NB : We should probably check that the navmeshes is loaded as well, or we may
				// run the risk of bloating out this list if some of the areas are large.
				CNavMesh * pCoverNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);
				if(pCoverNavMesh)
				{
					// Do we already have this navmesh in our list ?
					for(n=0; n<m_iFindCover_NumNavMeshesIndices; n++)
					{
						if(m_iFindCover_NavMeshesIndices[n] == iNavMeshIndex)
							break;
					}

					// If we reached the end of our list without finding this navmesh index, then we'll add it..
					if(n == m_iFindCover_NumNavMeshesIndices)
					{
						// Store this index
						m_iFindCover_NavMeshesIndices[m_iFindCover_NumNavMeshesIndices++] = iNavMeshIndex;
						
						// If we've reached the limits of our indices store, then complain.  This shouldn't need to happen.
						if(m_iFindCover_NumNavMeshesIndices == MAX_NUM_NAVMESHES_FOR_COVERAREAS)
						{
#if !__FINAL
							SpewOutCoverAreasRequired();
#endif
							Assertf(0, "Warning - ran out of storage for navmesh indices to span cover areas.\nPlease copy the info from the console window & include in a bug report to James.\n");
							vPos.x = vAreaMaxs.x;
							vPos.y = vAreaMaxs.y;
							a = m_iCoverNumberAreas;
							break;
						}
					}
				}
			}
		}
	}

	m_iFindCover_NavMeshProgress = 0;
	m_pFindCover_ContinueFromThisQuadTreeLeaf = NULL;
	m_iFindCover_NavMeshPolyProgress = 0;
	m_bFindCover_NavMeshesHaveChanged = false;
	m_bFindCover_CompletedTimeslice = false;
}

// Called to add cover points near the origin, to the global CCover class
void
CPathServer::ExtractCoverPoints_ContinueTimeslice(const Vector3 & UNUSED_PARAM(vOrigin), float UNUSED_PARAM(fMaxDistance))
{
	// Cover extraction only takes place in the regular nav domain for now.
	const aiNavDomain domain = kNavDomainRegular;

	bool bContinueProcessing = true;
	m_iFindCover_NumIterationsRemainingThisTimeslice = m_iFindCover_NumIterationsPerTimeslice;


	//********************************************************************
	//	Try to enter the critical section which protects the navmesh data.
	//  NOTE : IS THIS IN FACT NECESSARY??
	//********************************************************************
	
	LOCK_NAVMESH_DATA;

	u32 n;

	for(n=m_iFindCover_NavMeshProgress; n<m_iFindCover_NumNavMeshesIndices; n++)
	{
		if(m_iFindCover_NavMeshesIndices[n] == NAVMESH_NAVMESH_INDEX_NONE)
			continue;

		m_pFindCoverCurrentNavMesh = GetNavMeshFromIndex(m_iFindCover_NavMeshesIndices[n], domain);
		if(!m_pFindCoverCurrentNavMesh)
			continue;

		m_iFindCover_CurrentlyExtractingNavMeshIndex = m_iFindCover_NavMeshesIndices[n];

		bContinueProcessing = ExtractCoverPointsFromQuadtree(
			m_pFindCoverCurrentNavMesh->GetQuadTree(),
			m_pFindCoverCurrentNavMesh->GetQuadTree()->m_Mins,	//vCoverMin,
			m_pFindCoverCurrentNavMesh->GetQuadTree()->m_Maxs,	//vCoverMax,
			m_pFindCoverCurrentNavMesh->GetQuadTree()->m_Mins,
			m_pFindCoverCurrentNavMesh->GetExtents()
		);

		if(!bContinueProcessing)
		{
			break;
		}
	}

	// Did we complete the processing entirely, without doing an early-out ?
	if(n == m_iFindCover_NumNavMeshesIndices && bContinueProcessing == true)
	{
		m_bFindCover_CompletedTimeslice = true;
	}

}



#endif


