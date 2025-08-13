#include "PathServer\PathServer.h"

// Rage headers
#include "atl/inmap.h"
#include "system/xtl.h"
#include "vector/geometry.h"

// Framework headers
#include "fwmaths/Vector.h"
#include "fwmaths/random.h"

NAVMESH_OPTIMISATIONS()

//******************************************************************************************************************
//	RefinePathAndCreateNodes
//	This function takes the list of polygons (and the chosen waypoints within these polygons) which were found
//	by the A* pathfind function.  It then preforms the process known as "string-pulling" which removes kinks in
//	the path, and reduces it to a small number of straight line-segments with visibility between them.
//	These become the final waypoints which make up the path.
//******************************************************************************************************************

#define __BREAK_ON_POS	0

bool CPathServerThread::ConvertPointsInPolyToVertexList(
	CPathRequest * pPathRequest,
	const Vector3 & vStartPos,
	const Vector3 & vEndPos,
	Vector3 & vOut_StartWaypoint,
	TNavMeshPoly *& pOut_StartPoly,
	Vector3 & vOut_EndWaypoint,
	TNavMeshPoly *& pOut_EndPoly,
	s32 & iOut_NumPointsToTest)
{
	if(m_Vars.m_iNumPathPolys == 0)
	{
		// If m_iNumPathPolys was zero, and pPathRequest->m_iNumPoints is 2 - it means
		// we resolved the path with a simple LOS.
		if(pPathRequest->m_iNumPoints == 2)
			return false;

		// Otherwise, we couldn't find a path - so reset the number of points
		pPathRequest->m_iNumPoints = 0;
	}

	pPathRequest->m_iNumPoints = 0;

	Vector3 vPolyPts[NAVMESHPOLY_MAX_NUM_VERTICES];

	pOut_StartPoly = m_Vars.m_PathPolys[0];
	pOut_EndPoly = m_Vars.m_PathPolys[ m_Vars.m_iNumPathPolys-1 ];
	vOut_StartWaypoint = vStartPos;
	vOut_EndWaypoint = vEndPos;

	static const Vector3 vRaiseVecForVolumePolys(0.0f, 0.0f, UNDERWATER_DEFAULT_RAISE_UP_Z_AMT);


	m_Vars.m_PathPolys[m_Vars.m_iNumPathPolys] = pOut_EndPoly;

	//const float fEntityRadius = pPathRequest->m_fEntityRadius;
	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	u32 v;
	u32 t;

	int iStartOfPts;
	CNavMesh * pNavMesh;

	// If the start poly in the path is actually the m_pStartPoly, then use the vStartWaypoint
	// for the point-in-poly.  m_pStartPoly won't have a valid point enum.
	if(m_Vars.m_PathPolys[0]==m_Vars.m_pStartPoly)
	{
		m_Vars.m_vClosestPointInPathPolys[0] = vOut_StartWaypoint;
		iStartOfPts = 1;
	}
	// Otherwise we must have started from a poly added during the tessellation around the start-pos.
	// In this case the vStartWaypoint may be well outside of the first polygon, so use its pointenum.
	// NB : Will poly[0] even have a correct iPointEnum - its never set for starting polys..
	else
	{
		iStartOfPts = 0;
	}

	for(t=iStartOfPts; t<m_Vars.m_iNumPathPolys; t++)
	{
		pNavMesh = CPathServer::GetNavMeshFromIndex(m_Vars.m_PathPolys[t]->GetNavMeshIndex(), domain);
		Assert(pNavMesh);

		Assert(!m_Vars.m_PathPolys[t]->GetIsDegenerateConnectionPoly());	

		if(m_Vars.m_PathPolys[t]->GetPointEnum() == NAVMESHPOLY_POINTENUM_CENTROID)
		{
			if( pNavMesh->GetIndexOfMesh() != NAVMESH_INDEX_TESSELLATION)
			{
				pNavMesh->GetPolyCentroidQuick(m_Vars.m_PathPolys[t], m_Vars.m_vClosestPointInPathPolys[t]);
			}
			else
			{
				pNavMesh->GetPolyCentroid( pNavMesh->GetPolyIndex(m_Vars.m_PathPolys[t]), m_Vars.m_vClosestPointInPathPolys[t]);
			}
		}
		else
		{
			for(v=0; v<m_Vars.m_PathPolys[t]->GetNumVertices(); v++)
			{
				pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(m_Vars.m_PathPolys[t], v), vPolyPts[v] );
			}

			bool bAddedLinkPos = false;

			const s32 iPointEnum = m_Vars.m_PathPolys[t]->GetPointEnum();

			if(iPointEnum == NAVMESHPOLY_POINTENUM_SPECIAL_LINK_ENDPOS)
			{
				Assert(t > 0);
				if(t > 0)
				{
					// Find the special link from the pPathParent poly, to this poly
					TNavMeshPoly * pSpecialLinkStartPoly = m_Vars.m_PathPolys[t-1];
					if(pSpecialLinkStartPoly)
					{
						CNavMesh * pSpecialLinkStartNavMesh = CPathServer::GetNavMeshFromIndex(pSpecialLinkStartPoly->GetNavMeshIndex(), pPathRequest->GetMeshDataSet());
						TNavMeshPoly * pSpecialLinkEndPoly = m_Vars.m_PathPolys[t];
						CNavMesh * pSpecialLinkEndNavMesh = CPathServer::GetNavMeshFromIndex(pSpecialLinkEndPoly->GetNavMeshIndex(), pPathRequest->GetMeshDataSet());

						const u32 iStartNavMesh = pSpecialLinkStartNavMesh->GetIndexOfMesh();
						const u32 iEndNavMesh = pSpecialLinkEndNavMesh->GetIndexOfMesh();
						const u32 iStartPoly = pSpecialLinkStartNavMesh->GetPolyIndex( pSpecialLinkStartPoly );
						const u32 iEndPoly = pSpecialLinkEndNavMesh->GetPolyIndex( pSpecialLinkEndPoly );

						if(pSpecialLinkStartPoly->GetIsTessellatedFragment())
						{
							pSpecialLinkStartNavMesh = CPathServer::GetNavMeshFromIndex( CPathServer::GetTessInfo(pSpecialLinkStartPoly)->m_iNavMeshIndex, pPathRequest->GetMeshDataSet() );
							pSpecialLinkStartPoly = pSpecialLinkStartNavMesh->GetPoly( CPathServer::GetTessInfo(pSpecialLinkStartPoly)->m_iPolyIndex );
						}
						if(pSpecialLinkEndPoly->GetIsTessellatedFragment())
						{
							pSpecialLinkEndNavMesh = CPathServer::GetNavMeshFromIndex( CPathServer::GetTessInfo(pSpecialLinkEndPoly)->m_iNavMeshIndex, pPathRequest->GetMeshDataSet() );
							pSpecialLinkEndPoly = pSpecialLinkEndNavMesh->GetPoly( CPathServer::GetTessInfo(pSpecialLinkEndPoly)->m_iPolyIndex );
						}

						s32 iLinkLookupIndex = pSpecialLinkStartPoly->GetSpecialLinksStartIndex();
						for(s32 s=0; s<pSpecialLinkStartPoly->GetNumSpecialLinks(); s++)
						{
							u16 iLinkIndex = pSpecialLinkStartNavMesh->GetSpecialLinkIndex(iLinkLookupIndex++);
							Assert(iLinkIndex < pSpecialLinkStartNavMesh->GetNumSpecialLinks());
							CSpecialLinkInfo & linkInfo = pSpecialLinkStartNavMesh->GetSpecialLinks()[iLinkIndex];

							if( linkInfo.GetLinkFromNavMesh()==iStartNavMesh &&
								linkInfo.GetLinkFromPoly()==iStartPoly &&
								linkInfo.GetLinkToNavMesh()==iEndNavMesh &&
								linkInfo.GetLinkToPoly()==iEndPoly )
							{
								Vector3 vLinkFromPos, vLinkToPos;
								CNavMesh::DecompressVertex(vLinkFromPos, linkInfo.GetLinkFromPosX(), linkInfo.GetLinkFromPosY(), linkInfo.GetLinkFromPosZ(), pSpecialLinkStartNavMesh->GetQuadTree()->m_Mins, pSpecialLinkStartNavMesh->GetExtents());
								CNavMesh::DecompressVertex(vLinkToPos, linkInfo.GetLinkToPosX(), linkInfo.GetLinkToPosY(), linkInfo.GetLinkToPosZ(), pSpecialLinkEndNavMesh->GetQuadTree()->m_Mins, pSpecialLinkEndNavMesh->GetExtents());

								Vector3 vLinkVec = vLinkToPos - vLinkFromPos;
								vLinkVec.z = 0.0f;
								vLinkVec.Normalize();

								Vector3 vPointInPolyOffset = vLinkVec * (pPathRequest->m_fEntityRadius + 0.1f);
								m_Vars.m_vClosestPointInPathPolys[t] = vLinkToPos + vPointInPolyOffset;

								bAddedLinkPos = true;
								break;
							}
						}
					}
				}
				if(!bAddedLinkPos)
				{
					m_Vars.m_PathPolys[t]->SetPointEnum(NAVMESHPOLY_POINTENUM_CENTROID);

					if( pNavMesh->GetIndexOfMesh() != NAVMESH_INDEX_TESSELLATION)
					{
						pNavMesh->GetPolyCentroidQuick(m_Vars.m_PathPolys[t], m_Vars.m_vClosestPointInPathPolys[t]);
					}
					else
					{
						pNavMesh->GetPolyCentroid( pNavMesh->GetPolyIndex(m_Vars.m_PathPolys[t]), m_Vars.m_vClosestPointInPathPolys[t]);
					}

					bAddedLinkPos = true;
				}
			}
			
			if(!bAddedLinkPos)
			{
				GetPointInPolyFromPointEnum(
					pNavMesh,
					m_Vars.m_PathPolys[t],
					vPolyPts,
					&m_Vars.m_vClosestPointInPathPolys[t],
					iPointEnum,
					ShouldUseMorePointsForPoly(*m_Vars.m_PathPolys[t], pPathRequest) );
			}
		}

#if __STORE_POLYS_IN_PATH_REQUEST 
		pPathRequest->m_InitiallyFoundPathPointInPolys[t] = m_Vars.m_vClosestPointInPathPolys[t];
#endif
	}

#if __STORE_POLYS_IN_PATH_REQUEST
	pPathRequest->m_InitiallyFoundPathPointInPolys[0] = vOut_StartWaypoint;
	pPathRequest->m_InitiallyFoundPathPointInPolys[m_Vars.m_iNumPathPolys-1] = vOut_EndWaypoint;
#endif

	if(!CPathServer::ms_bRefinePaths)
	{
		for(u32 f=1; f<MIN(m_Vars.m_iNumPathPolys,MAX_NUM_PATH_POINTS) ; f++)
		{
#if __STORE_POLYS_IN_PATH_REQUEST
			pPathRequest->m_InitiallyFoundPathPointInPolys[f] = m_Vars.m_vClosestPointInPathPolys[f];
#endif
			pPathRequest->m_PathPoints[f] = m_Vars.m_vClosestPointInPathPolys[f];
			pPathRequest->m_PathPolys[f] = m_Vars.m_PathPolys[f];
			pPathRequest->m_WaypointFlags[f] = m_Vars.m_iPolyWaypointFlags[f];
		}

		pPathRequest->m_PathPolys[0] = m_Vars.m_PathPolys[0];
		pPathRequest->m_PathPoints[0] = vOut_StartWaypoint;

		pPathRequest->m_PathPolys[m_Vars.m_iNumPathPolys-1] = m_Vars.m_PathPolys[m_Vars.m_iNumPathPolys-1];
		pPathRequest->m_PathPoints[m_Vars.m_iNumPathPolys-1] = vOut_EndWaypoint;

		pPathRequest->m_iNumPoints = MIN(m_Vars.m_iNumPathPolys,MAX_NUM_PATH_POINTS);

		return false;
	}

	iOut_NumPointsToTest = 0;

	// If we're not fleeing, then the end point is taken explicitly from the path request
	if(!pPathRequest->m_bFleeTarget && !pPathRequest->m_bWander)
	{
		// If we didn't actually get exactly to the endpoint, but terminated within the completion radius -
		// then see how far off we were.  If outside some set distance, then we shall use the endpoint
		// we achieved instead of the actual path endpoint.
		if(pPathRequest->m_PathResultInfo.m_bUsedCompletionRadius)
		{
			static const float fUseRealEndPointEpsSqr = 0.5f*0.5f;
			Vector3 vDiffToActualEndPoint = m_Vars.m_vClosestPointInPathPolys[m_Vars.m_iNumPathPolys-1] - pPathRequest->m_vPathEnd;
			if(vDiffToActualEndPoint.Mag2() < fUseRealEndPointEpsSqr)
			{
				m_Vars.m_vClosestPointInPathPolys[m_Vars.m_iNumPathPolys] = vOut_EndWaypoint;
			}
			else
			{
				vOut_EndWaypoint = m_Vars.m_vClosestPointInPathPolys[m_Vars.m_iNumPathPolys-1];
				m_Vars.m_vClosestPointInPathPolys[m_Vars.m_iNumPathPolys] = vOut_EndWaypoint;
			}
		}
		else
		{
			m_Vars.m_vClosestPointInPathPolys[m_Vars.m_iNumPathPolys] = vOut_EndWaypoint;
		}

		iOut_NumPointsToTest = m_Vars.m_iNumPathPolys;
	}
	// Otherwise, it is the position in the poly at which we terminated the flee path
	else
	{
		vOut_EndWaypoint = m_Vars.m_vClosestPointInPathPolys[m_Vars.m_iNumPathPolys-1];
		m_Vars.m_vClosestPointInPathPolys[m_Vars.m_iNumPathPolys] = vOut_EndWaypoint;

		iOut_NumPointsToTest = m_Vars.m_iNumPathPolys-1;
	}

	pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = vOut_StartWaypoint;
	pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = pOut_StartPoly;
	pPathRequest->m_WaypointFlags[ pPathRequest->m_iNumPoints ] = m_Vars.m_iPolyWaypointFlags[0];
	pPathRequest->m_iNumPoints++;

	return true;
}


bool CPathServerThread::RefinePathAndCreateNodes(CPathRequest * pPathRequest, const Vector3 & vStartPos, const Vector3 & vEndPos, const s32 iMaxNumPoints)
{
#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

#if __BREAK_ON_POS
	static Vector3 vBreakOnPos(9999.9f,9999.9f,9999.9f);
	static bool bPause = false;
#endif

	//*************************************************************

	Vector3 vStartWaypoint, vEndWaypoint;
	TNavMeshPoly * pStartPoly, * pEndPoly;
	s32 iNumPointsToTest;

	if( !ConvertPointsInPolyToVertexList(pPathRequest, vStartPos, vEndPos, vStartWaypoint, pStartPoly, vEndWaypoint, pEndPoly, iNumPointsToTest))
	{
		return true;
	}

	//*************************************************************

	const float fEntityRadius = pPathRequest->m_fEntityRadius;
	const aiNavDomain domain = pPathRequest->GetMeshDataSet();

	m_iNumRefinePathIterations = 0;

	Vector3 vCurrentStartPos = vStartWaypoint;
	TNavMeshPoly * pCurrentStartPoly = pStartPoly;

	u32 iCurrentStartIndex = 0;
	u32 iCurrentTestIndex = 1;
	bool bLOS = false, bDynObjLOS = false, bNavMeshLOS = false;
	DEV_ONLY(s32 iNumIterations=0;)

	while(iCurrentTestIndex <= iNumPointsToTest)
	{
#if __DEV
		iNumIterations++;
		Assertf(iNumIterations < 1000, "Possible infinite loop found in string pull (see url:bugstar:984258)");
#endif
		if(CPathServer::m_bForceAbortCurrentRequest)
		{
			pPathRequest->m_iCompletionCode = PATH_ABORTED_DUE_TO_ANOTHER_THREAD;
			return false;
		}
		// Early out if path is cancelled during processing
		if(pPathRequest->m_iCompletionCode == PATH_CANCELLED)
		{
			return false;
		}

		//**************************************************************************************

		TNavMeshPoly * pTestPoly = m_Vars.m_PathPolys[iCurrentTestIndex];

		Vector3 vTestPos = m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex];

		// Increment timestamp before LOS.  This prevents infinite recursion in some tricky boundary cases.
		IncTimeStamp();

		m_bLosHitInfluenceBoundary = false;
		m_bLosCrossedCoverBoundary = false;

		bool bPointAdded = false;
		static dev_bool bDoExtend = false;
		static const float fExtendDist = 0.1f;

		Vector3 vExtend = VEC3_ZERO;
		if(bDoExtend)
		{
			vExtend = m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex] - m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex];
			vExtend.Normalize();
			vExtend *= fExtendDist;
		}

		bNavMeshLOS = TestNavMeshLOS(
			m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex] - vExtend,
			m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex] + vExtend,
			m_Vars.m_PathPolys[iCurrentTestIndex],
			pCurrentStartPoly,
			NULL,
			m_Vars.m_iLosFlags,
			domain);

		if(/*bNavMeshLOS && */CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance)
		{
			bDynObjLOS = TestDynamicObjectLOS(
				m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex],
				m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex]
				);
		}
		else
		{
			bDynObjLOS = true;
		}

		if(iCurrentTestIndex <= iCurrentStartIndex+1)
		{
			m_bLosHitInfluenceBoundary = false;
			m_bLosCrossedCoverBoundary = false;
		}

		bLOS = (bNavMeshLOS && bDynObjLOS);

		//***************************************************************************
		// No line-of-sight at all, or we hit an openable/pushable/climbable obejct

		if((!bLOS) || m_bLosHitInfluenceBoundary || m_bLosCrossedCoverBoundary)
		{
			//***********************************************************************************************************
			// If the current poly was reached via a non-standard adjacency (ie. a climb-up, drop-down, etc), then
			// we must ensure that one point is added exactly halfway along the edge from which this polygon was
			// reached & another must be added as close to is as possible within this poly.  It should ideally be
			// exactly in the direction pointed by the previously traversed edge's normal.  (NB: In fact, we could
			// generate it manually since we're not restricted to the pointEnum scheme here...)
			//***********************************************************************************************************

			if(m_Vars.m_PathPolys[iCurrentTestIndex]->TestFlags(NAVMESHPOLY_REACHEDBY_MASK) && 
				((iCurrentTestIndex <= iCurrentStartIndex + 1) || bDynObjLOS))
			{
				TNavMeshPoly * pCurrentTestPoly = m_Vars.m_PathPolys[iCurrentTestIndex];
				CNavMesh * pCurrentNavMesh = CPathServer::GetNavMeshFromIndex(pCurrentTestPoly->GetNavMeshIndex(), domain);

				// CL:3638784 (B* 1024539: If a climb point is blocked by a dynamic object in the path we now fail the path.)
				// JB: Unless this is a special link
				if (!bDynObjLOS && pCurrentTestPoly->TestFlags(NAVMESHPOLY_WAS_REACHED_VIA_SPECIAL_LINK)==false)
				{
					pPathRequest->m_iCompletionCode = PATH_NOT_FOUND;
					return false;
				}

				//**********************************************************
				//	Special link type of adjacency led to this poly

				if(pCurrentTestPoly->TestFlags(NAVMESHPOLY_WAS_REACHED_VIA_SPECIAL_LINK))
				{
					Assert(pCurrentTestPoly->m_PathParentPoly != NULL);

					TNavMeshPoly * pParentPoly = pCurrentTestPoly->m_PathParentPoly;
					CNavMesh * pParentNavMesh = CPathServer::GetNavMeshFromIndex(pParentPoly->GetNavMeshIndex(), domain);

					// Find the special link from the previous poly to this poly
					CNavMesh * pOriginalFromNavMesh, * pOriginalToNavMesh;
					TNavMeshPoly * pOriginalFromPoly, * pOriginalToPoly;

					// Find the original 'from' poly, if the pParentPoly is in the tessellation navmesh
					if(pParentNavMesh->GetIndexOfMesh() == NAVMESH_INDEX_TESSELLATION)
					{
						const int iPolyIndexInTessMesh = pParentNavMesh->GetPolyIndex(pParentPoly);
						TTessInfo & pTessInfo = CPathServer::m_PolysTessellatedFrom[iPolyIndexInTessMesh];
						pOriginalFromNavMesh = CPathServer::GetNavMeshFromIndex(pTessInfo.m_iNavMeshIndex, domain);
						pOriginalFromPoly = pOriginalFromNavMesh->GetPoly(pTessInfo.m_iPolyIndex);
					}
					else
					{
						pOriginalFromNavMesh = pParentNavMesh;
						pOriginalFromPoly = pParentPoly;
					}

					// Find the original 'to' poly, if the pCurrentTestPoly is in the tessellation navmesh
					if(pCurrentNavMesh->GetIndexOfMesh() == NAVMESH_INDEX_TESSELLATION)
					{
						int iPolyIndexInTessMesh = pCurrentNavMesh->GetPolyIndex(pCurrentTestPoly);
						TTessInfo & pTessInfo = CPathServer::m_PolysTessellatedFrom[iPolyIndexInTessMesh];
						pOriginalToNavMesh = CPathServer::GetNavMeshFromIndex(pTessInfo.m_iNavMeshIndex, domain);
						pOriginalToPoly = pOriginalFromNavMesh->GetPoly(pTessInfo.m_iPolyIndex);
					}
					else
					{
						pOriginalToNavMesh = pCurrentNavMesh;
						pOriginalToPoly = pCurrentTestPoly;
					}

					//s32 iLinkLookupIndex = pParentPoly->GetSpecialLinksStartIndex();
					//for(s32 s=0; s<pParentPoly->GetNumSpecialLinks(); s++)

					s32 iLinkLookupIndex = pOriginalFromPoly->GetSpecialLinksStartIndex();
					for(s32 s=0; s<pOriginalFromPoly->GetNumSpecialLinks(); s++)
					{
						u16 iLinkIndex = pOriginalFromNavMesh->GetSpecialLinkIndex(iLinkLookupIndex++);
						Assert(iLinkIndex < pOriginalFromNavMesh->GetNumSpecialLinks());
						CSpecialLinkInfo & linkInfo = pOriginalFromNavMesh->GetSpecialLinks()[iLinkIndex];

//						if(linkInfo.m_Struct1.m_iOriginalLinkFromNavMesh==pOriginalFromPoly->GetNavMeshIndex() &&
//							linkInfo.m_iOriginalLinkFromPoly==pOriginalFromNavMesh->GetPolyIndex(pOriginalFromPoly) &&
//							linkInfo.m_Struct2.m_iOriginalLinkToNavMesh==pOriginalToPoly->GetNavMeshIndex() &&
//							linkInfo.m_iOriginalLinkToPoly==pOriginalToNavMesh->GetPolyIndex(pOriginalToPoly))

						if( linkInfo.GetLinkFromNavMesh()==pParentNavMesh->GetIndexOfMesh() &&
							 linkInfo.GetLinkFromPoly()==pParentNavMesh->GetPolyIndex(pParentPoly) &&
							  linkInfo.GetLinkToNavMesh()==pCurrentNavMesh->GetIndexOfMesh() &&
							   linkInfo.GetLinkToPoly()==pCurrentNavMesh->GetPolyIndex(pCurrentTestPoly) )
						{
							Assert(linkInfo.GetAStarTimeStamp()==m_iAStarTimeStamp);

							// This is the special link from the previous poly to this poly.
							// We need to add the start & end point to the path
							Vector3 vSpecialLinkStart, vSpecialLinkEnd;
							pOriginalFromNavMesh->DecompressVertex(vSpecialLinkStart, linkInfo.GetLinkFromPosX(), linkInfo.GetLinkFromPosY(), linkInfo.GetLinkFromPosZ());
							pOriginalToNavMesh->DecompressVertex(vSpecialLinkEnd, linkInfo.GetLinkToPosX(), linkInfo.GetLinkToPosY(), linkInfo.GetLinkToPosZ());
							switch(linkInfo.GetLinkType())
							{
								case NAVMESH_LINKTYPE_CLIMB_LADDER:
								case NAVMESH_LINKTYPE_DESCEND_LADDER:
								case NAVMESH_LINKTYPE_CLIMB_OBJECT:
								{
									// Bail out if we don't have enough room to set up this climb
									if(pPathRequest->m_iNumPoints + 3 >= iMaxNumPoints)
									{
										return true;
									}

#ifdef GTA_ENGINE
									// Locally turn off ladders if too many wants to use them within a short time
									if ( !pPathRequest->m_bCoverFinderPath &&
										(linkInfo.GetLinkType() == NAVMESH_LINKTYPE_CLIMB_LADDER ||
										linkInfo.GetLinkType() == NAVMESH_LINKTYPE_DESCEND_LADDER))
									{
										m_BlockedLadders.UpdateUsage(vSpecialLinkStart.z < vSpecialLinkEnd.z ? vSpecialLinkStart : vSpecialLinkEnd);
									}
#endif // GTA_ENGINE
									// We have to add normally the point in the previous polygon we visited prior to
									// this ladder climb's top/bottom points.  This is because we cannot necessarily
									// see the get-on point from the last route point we added.
									if(iCurrentTestIndex > 0)
									{
										pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex-1];
										pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = m_Vars.m_PathPolys[iCurrentTestIndex-1];
										// Prev point will have been standard
										pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].Clear();
										pPathRequest->m_iNumPoints++;

										Assert(pPathRequest->m_iNumPoints < iMaxNumPoints);
									}

									pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = vSpecialLinkStart;
									pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = NULL;

									// Set the special action depending upon the link-type
									switch(linkInfo.GetLinkType())
									{
										case NAVMESH_LINKTYPE_CLIMB_LADDER:
											pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].m_iSpecialActionFlags = (u16)WAYPOINT_FLAG_CLIMB_LADDER;
											break;
										case NAVMESH_LINKTYPE_DESCEND_LADDER:
											pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].m_iSpecialActionFlags = (u16)WAYPOINT_FLAG_DESCEND_LADDER;
											break;
										case NAVMESH_LINKTYPE_CLIMB_OBJECT:
											pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].m_iSpecialActionFlags = (u16)WAYPOINT_FLAG_CLIMB_OBJECT;
											break;
										default:
											Assert(false);
											break;
									}

									// We must set the water flag or it gets lost and we get troubles navigating to climb points in water
									if (m_Vars.m_iPolyWaypointFlags[iCurrentTestIndex-1].m_iSpecialActionFlags & WAYPOINT_FLAG_IS_ON_WATER_SURFACE)
										pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].m_iSpecialActionFlags |= WAYPOINT_FLAG_IS_ON_WATER_SURFACE;

									// NEW: Set the heading of this waypoint from the heading of the link (store in m_iLinkFlags)
									pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].m_iHeading = (u8)linkInfo.GetLinkFlags();

									pPathRequest->m_iNumPoints++;

									Assert(pPathRequest->m_iNumPoints < iMaxNumPoints);

									pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = vSpecialLinkEnd;
									pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = NULL;
									pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].Clear();
									pPathRequest->m_iNumPoints++;

									Assert(pPathRequest->m_iNumPoints < iMaxNumPoints);

									break;
								}
								default:
								{
									Assert(0);
									break;
								}
							}
							// We've found the special link, so quit
							break;
						}
					}
				}

				//****************************************************************************************************
				//	We arrived in this poly via a navmesh non-standard adjacency.   In other words, the adjacency
				//  is encoded in the previous path-poly's TAdjPoly adjacency array.

				else
				{
					// Find the edge on the previous poly which adjoined to this poly
					if(iCurrentTestIndex > 0)
					{
						TNavMeshPoly * pPrevPoly = m_Vars.m_PathPolys[iCurrentTestIndex-1];
						CNavMesh * pPrevNavMesh = CPathServer::GetNavMeshFromIndex(pPrevPoly->GetNavMeshIndex(), domain);

						// If the parent polygon was a tessellated fragment, then look to the original untesselated polygon
						// to locate the TAdjPoly which contains the adjacency info.

						// NB: We need to do the same for the pCurrentTestPoly polygon & pCurrentNavMesh, since this might have been
						// tessellated- and the nonstandard adjacency in the parent polygon will still point to the original polygon/navmesh

						if(pPrevPoly->GetIsTessellatedFragment())
						{
							Assert(pPrevNavMesh->GetIndexOfMesh()==NAVMESH_INDEX_TESSELLATION);

							TTessInfo * pTessInfo = CPathServer::GetTessInfo(pPrevPoly);
							Assert(pTessInfo);

							pPrevNavMesh = CPathServer::GetNavMeshFromIndex(pTessInfo->m_iNavMeshIndex, pPathRequest->GetMeshDataSet());
							pPrevPoly = pPrevNavMesh->GetPoly(pTessInfo->m_iPolyIndex);
						}

						CNavMesh * pCurrentNavMesh = CPathServer::GetNavMeshFromIndex(pCurrentTestPoly->GetNavMeshIndex(), domain);

						if(pPrevNavMesh && pCurrentNavMesh)
						{
							u32 e;
							for(e=0; e<pPrevPoly->GetNumVertices(); e++)
							{
								const TAdjPoly & adjPoly = pPrevNavMesh->GetAdjacentPoly( pPrevPoly->GetFirstVertexIndex() + e );

								if(adjPoly.GetNavMeshIndex(pPrevNavMesh->GetAdjacentMeshes()) == pCurrentTestPoly->GetNavMeshIndex())
								{
									TNavMeshPoly * pNextPoly = pCurrentNavMesh->GetPoly(adjPoly.GetPolyIndex());
									if(pNextPoly == pCurrentTestPoly)
										break;
								}
							}

							//************************************************************************************
							//	Add a vertex just before the adjacency.  The vertex is half-way along the
							//	previous poly's edge which we arrived in this poly from.  (That is the per-edge
							//	location which is checked for jumps/drop-downs/etc during the navmesh creation)

							if(e != pPrevPoly->GetNumVertices())
							{
								Vector3 vEdgeVec1, vEdgeVec2, vPointOnEdge;
								pPrevNavMesh->GetVertex( pPrevNavMesh->GetPolyVertexIndex(pPrevPoly, e), vEdgeVec1);
								pPrevNavMesh->GetVertex( pPrevNavMesh->GetPolyVertexIndex(pPrevPoly, ((e+1)%pPrevPoly->GetNumVertices()) ), vEdgeVec2);
								vPointOnEdge = (vEdgeVec1 + vEdgeVec2) * 0.5f;

								Vector3 vEdgeVec = vEdgeVec2 - vEdgeVec1;
								vEdgeVec.Normalize();
								Vector3 vEdgeNormal;
								vEdgeNormal.Cross(vEdgeVec, Vector3(0,0,1.0f));


								pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = vPointOnEdge;
								pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = NULL;
								// Prev point will have been standard
								pPathRequest->m_WaypointFlags[ pPathRequest->m_iNumPoints ] = m_Vars.m_iPolyWaypointFlags[iCurrentTestIndex];

#if __BREAK_ON_POS
								if(vBreakOnPos.IsClose(pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ], 0.25f))
									bPause = true;
#endif

								// Find the closest point in pCurrentTestPoly, to the just-added vertex
								
								Vector3 vClosestPoint1;
								Vector3 vPolyPts[NAVMESHPOLY_MAX_NUM_VERTICES];
								u32 v;
								for(v=0; v<pCurrentTestPoly->GetNumVertices(); v++)
								{
									pCurrentNavMesh->GetVertex( pCurrentNavMesh->GetPolyVertexIndex(pCurrentTestPoly, v), vPolyPts[v]);
								}

								// Firstly find the closest point on the polygon edge
								float fLeastDist2 = FLT_MAX;
								u32 lastv = pCurrentTestPoly->GetNumVertices()-1;
								for(v=0; v<pCurrentTestPoly->GetNumVertices(); v++)
								{
									const Vector3 vVec = vPolyPts[v]-vPolyPts[lastv];
									const float T = geomTValues::FindTValueSegToPoint(vPolyPts[lastv], vVec, vPointOnEdge);
									const Vector3 vPos = vPolyPts[lastv] + (vVec * T);
									const float fDiff2 = (vPos - vPointOnEdge).XYMag2();
									if( fDiff2 < fLeastDist2 )
									{
										fLeastDist2 = fDiff2;
										vClosestPoint1 = vPos;
									}

									lastv = v;
								}

								// Secondly, generate a set of candidate points within the polygon body
								m_VisitPolyVars.m_pFromNavMesh = NULL;
								m_VisitPolyVars.m_pFromPoly = NULL;
								m_VisitPolyVars.m_pToNavMesh = pCurrentNavMesh;
								m_VisitPolyVars.m_pToPoly = pCurrentTestPoly;
								m_VisitPolyVars.m_pToPolyPoints = vPolyPts;
								m_VisitPolyVars.m_vTargetPos = vPointOnEdge;
							
								s32 iDummyPointEnum;
								Vector3 vClosestPoint2;
								GetClosestPointInPolyToTargetDistSqr(
									m_VisitPolyVars,
									TVisitPolyStruct::FLAG_CONSIDER_MAXIMUM_NUM_VERTICES,
									&vClosestPoint2,
									iDummyPointEnum);

								const Vector3 vClosestPoint = (vClosestPoint1 - vPointOnEdge).XYMag2() < (vClosestPoint2 - vPointOnEdge).XYMag2() ? vClosestPoint1 : vClosestPoint2;

#ifdef GTA_ENGINE
								Vector3 vVecToClosestPoint = vClosestPoint - vPointOnEdge;
								//float fWptHeading = fwAngle::GetRadianAngleBetweenPoints(vClosestPoint.x + vEdgeNormal.x, vClosestPoint.y + vEdgeNormal.y, vPointOnEdge.x, vPointOnEdge.y);
								float fWptHeading = fwAngle::GetRadianAngleBetweenPoints(vVecToClosestPoint.x, vVecToClosestPoint.y, 0.0f, 0.0f);

								while(fWptHeading < 0.0f) fWptHeading += TWO_PI;
								while(fWptHeading >= TWO_PI) fWptHeading -= TWO_PI;
								pPathRequest->m_WaypointFlags[ pPathRequest->m_iNumPoints ].m_iHeading = (u8) ((fWptHeading / TWO_PI) * 255.0f);
#endif
								pPathRequest->m_iNumPoints++;
								if(pPathRequest->m_iNumPoints >= iMaxNumPoints)
									return true;

								// ( NB : Maybe move the point away from the edge, because if we're not careful we may end up climbing
								// up the wall again in our attempt to get to this waypoint ? )

								// Add this point too
								pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = vClosestPoint + vEdgeNormal;
								pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = NULL;
								// The poly at 'iCurrentTestIndex' was connected to by a non-standard adjacency
								pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].Clear();

#if __BREAK_ON_POS
								if(vBreakOnPos.IsClose(pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ], 0.25f))
									bPause = true;
#endif
								pPathRequest->m_iNumPoints++;

								if(pPathRequest->m_iNumPoints >= iMaxNumPoints)
									return true;
							}
						}
					}
				}

				// JB: No need for this, since it is done later at the end of the loop.
				// Doing it here means that the progress gets advanced too far..

				/*
				iCurrentStartIndex = iCurrentTestIndex;
				iCurrentTestIndex++;

				pCurrentStartPoly = m_Vars.m_PathPolys[iCurrentStartIndex];
				vCurrentStartPos = m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex];
				*/
			}

			//**********************************************************************************************
			//	Otherwise - this is a normal poly->poly connection in the navmesh
			//
			// If the poly we are testing is further on than the one we are starting from.
			// ie. the LOS is blocked, but not between adjacent polys (we'll handle that
			// case in a sec).

			else if((iCurrentTestIndex > iCurrentStartIndex + 1) || m_bLosHitInfluenceBoundary || m_bLosCrossedCoverBoundary)
			{
				// What we could do here to improve the path, is to adjust the point
				// "m_vClosestPointInPathPolys[iCurrentStartIndex]" repeatedly until we get
				// a better point.  This should be a point which is closer to lying on the
				// line between the last point and the next point ??

				// In the case of an influence boundary, we will just start from the next poly.
				// It's not important whether or not we have a direct LOS, as there is no physical obstruction
				if(bLOS && (m_bLosHitInfluenceBoundary || m_bLosCrossedCoverBoundary))
				{
					iCurrentStartIndex = iCurrentTestIndex;
					//iCurrentTestIndex++;
				}
				else
				{
					iCurrentStartIndex = iCurrentTestIndex - 1;
					iCurrentTestIndex = iCurrentTestIndex - 1;
				}

				/*
				pCurrentStartPoly = m_Vars.m_PathPolys[iCurrentStartIndex];
				vCurrentStartPos = m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex];

				pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = vCurrentStartPos;
				pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = pCurrentStartPoly;
				// Prev point will have been standard
				pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].Clear();

#if __BREAK_ON_POS
				if(vBreakOnPos.IsClose(pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ], 0.25f))
					bPause = true;
#endif

				pPathRequest->m_iNumPoints++;

				if(pPathRequest->m_iNumPoints >= iMaxNumPoints)
					return true;
				*/
			}
			else
			{
				// We cannot see from one adjacent poly to the next poly.
				// Add a point along the shared edge between each poly.


				bool bFoundNewPoint = false;
				bool bLineActuallyIntersectsEdge = false;

				if(bNavMeshLOS && !bDynObjLOS)
				{
					// If the obstruction was purely due to a dynamic object, then choose a position along
					// the shared edge for which there is visibility between the last two points.
					bFoundNewPoint = GetPointWithDynamicObjectVisiblityAlongSharedEdge(
						m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex],
						m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex],
						pCurrentStartPoly,
						pTestPoly,
						fEntityRadius,
						pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ],
						domain
					);
				}
				else
				{
					// Find which side of the line defined by the best points in "pCurrentStartPoly to pTestPoly", 
					// the two points defining the shared edge between the polys lies.
					bFoundNewPoint = GetPointAlongSharedEdge(
						m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex],
						m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex],
						pCurrentStartPoly,
						pTestPoly,
						fEntityRadius,
						pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ],
						bLineActuallyIntersectsEdge,
						domain
						);
				}

#if __BREAK_ON_POS
				if(vBreakOnPos.IsClose(m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex], 0.25f))
					bPause = true;
#endif

				if(bLineActuallyIntersectsEdge)
				{
					// No need to add a new point here after all, since the line from start->test points
					// in fact intersects the shared edge between the two polygons.  By NOT adding the point
					// here, we'll enable ourselves more useful path points later on in the route.

				}
				else if(!bFoundNewPoint)
				{
					// If the pTestPoly is an alternative starting polygon, then there is not necessarily an adjacency with the
					// previous polygon (which will be the m_pStartPoly).  In this case just assume we have LOS, because this was
					// a prerequisive of having marked the polygon as 'NAVMESHPOLY_ALTERNATIVE_STARTING_POLY'
					if( pTestPoly->TestFlags(NAVMESHPOLY_ALTERNATIVE_STARTING_POLY) && (pTestPoly->m_PathParentPoly == m_Vars.m_pStartPoly || pTestPoly->m_PathParentPoly == NULL))
					{

					}
					else
					{

#if NAVMESH_OPTIMISATIONS_OFF
						//Assertf(false, "!bFoundNewPoint :-(");
#endif
						// 7/12/05 - This line used to just return FALSE.  However that was causing problems because the
						// last point in the route might not be at/near the target & we'd get navmesh routes not completing.
						// I've just added the midpoint between the two polys & will hope for the best.
						pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = 
							(m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex] +
							m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex]) * 0.5f;

#if __BREAK_ON_POS
						if(vBreakOnPos.IsClose(m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex], 0.25f))
							bPause = true;
#endif
						pPathRequest->m_iNumPoints++;
						if(pPathRequest->m_iNumPoints >= iMaxNumPoints)
							return true;

						bPointAdded = m_Vars.InsertPoly(iCurrentStartIndex + 1, 
							pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints -1], 
							pCurrentStartPoly, 
							m_Vars.m_iPolyWaypointFlags[iCurrentStartIndex]);

					}
				}
				else
				{
					pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = pCurrentStartPoly; //pTestPoly; //NULL;

					pPathRequest->m_iNumPoints++;
					if(pPathRequest->m_iNumPoints >= iMaxNumPoints)
						return true;

					bPointAdded = m_Vars.InsertPoly(iCurrentStartIndex + 1, 
						pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints - 1], 
						pPathRequest->m_PathPolys[pPathRequest->m_iNumPoints - 1], 
						m_Vars.m_iPolyWaypointFlags[iCurrentStartIndex]);

				}
			}

			iCurrentStartIndex = iCurrentTestIndex;
			pCurrentStartPoly = m_Vars.m_PathPolys[iCurrentStartIndex];
			vCurrentStartPos = m_Vars.m_vClosestPointInPathPolys[iCurrentStartIndex];

			if(iCurrentTestIndex != 0 && !bPointAdded)
			{
				// Add a new point in the destination poly, this will become the new starting point
				pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = m_Vars.m_vClosestPointInPathPolys[iCurrentTestIndex];
				// NB : we could set this to be either poly, and we allow us to modify the point later??
				pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = pCurrentStartPoly;
				// Prev point will have been standard
				pPathRequest->m_WaypointFlags[pPathRequest->m_iNumPoints].Clear();

				pPathRequest->m_iNumPoints++;

				if(pPathRequest->m_iNumPoints >= iMaxNumPoints)
					return true;
			}
			iCurrentTestIndex++;
		}

		//************************************************************************
		// Clear line-of-sight / didn't hit an openable/pushable/climbable object
		else
		{
			iCurrentTestIndex++;
		}
	}

	if( pPathRequest->m_iNumPoints == 1)
	{
		if( (pPathRequest->m_PathPoints[0] - m_Vars.m_vClosestPointInPathPolys[0]).Mag2() < 0.001f )
		{
			pPathRequest->m_PathPoints[1] = m_Vars.m_vClosestPointInPathPolys[m_Vars.m_iNumPathPolys-1];
			pPathRequest->m_PathPolys[1] = m_Vars.m_PathPolys[m_Vars.m_iNumPathPolys-1];
			pPathRequest->m_WaypointFlags[1] = m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys-1];

			pPathRequest->m_iNumPoints++;
		}
	}

	//**************************************************************************
	// Make sure that the final waypoint is added to the path, if not already

	if( (pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints-1 ] - vEndWaypoint).XYMag2() > 0.01f )
	{
		//*********************************************************************************
		// If we can directly see the final waypoint, from the penultimate point, then we
		// just replace the final point rather than appending another..

		if( pPathRequest->m_iNumPoints >= 2 )
		{
			bNavMeshLOS = TestNavMeshLOS(
				pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints-2 ],
				vEndWaypoint,
				pEndPoly,
				m_Vars.m_PathPolys[ pPathRequest->m_iNumPoints-2 ],
				NULL,
				m_Vars.m_iLosFlags,
				domain);

			if(bNavMeshLOS && CPathServer::m_eObjectAvoidanceMode != CPathServer::ENoObjectAvoidance)
			{
				bDynObjLOS = TestDynamicObjectLOS(
					pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints-2 ],
					vEndWaypoint
				);
			}
		}

		if( ! (bNavMeshLOS && bDynObjLOS) )
		{
			pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints ] = vEndWaypoint;
			pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints ] = pEndPoly;
			pPathRequest->m_WaypointFlags[ pPathRequest->m_iNumPoints ] = m_Vars.m_iPolyWaypointFlags[m_Vars.m_iNumPathPolys-1];

			pPathRequest->m_iNumPoints++;
		}
		else
		{
			pPathRequest->m_PathPoints[ pPathRequest->m_iNumPoints-1 ] = vEndWaypoint;
			pPathRequest->m_PathPolys[ pPathRequest->m_iNumPoints-1 ] = pEndPoly;
		}
	}

	return true;
}
