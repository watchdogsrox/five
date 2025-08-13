
// Rage headers
#include "file\device.h"
#include "file\stream.h"
#include "data\struct.h"
#include "file\asset.h"
#include "paging\rscbuilder.h"
#include "vector/geometry.h"

// Framework headers
#include "ai/navmesh/priqueue.h"
#include "fwmaths\random.h"
#include "fwmaths\vector.h"

// Game headers
#include "PathServer\PathServer.h"

#ifdef GTA_ENGINE
NAVMESH_OPTIMISATIONS()
#endif

#if !__HIERARCHICAL_NODES_ENABLED

CHierarchicalNavLink * CPathServerThread::Hierarchical_GetClosestLinkToPos(const Vector3 & UNUSED_PARAM(vPos), const float UNUSED_PARAM(fMaxSearchDist)) { return NULL; }
void CPathServerThread::Hierarchical_GetClosestNodeToPos(const Vector3 & UNUSED_PARAM(vPos), const float UNUSED_PARAM(fMaxSearchDistSqr), CHierarchicalNavNode ** UNUSED_PARAM(ppOut_ClosestNode), CHierarchicalNavNode ** UNUSED_PARAM(ppOut_ClosestPavementNode), CHierarchicalNavNode ** UNUSED_PARAM(ppOut_ClosestNonIsolatedNode)) { }
bool CPathServerThread::Hierarchical_FindPath(CPathRequest * UNUSED_PARAM(pPathRequest)) { return false; }
bool CPathServerThread::Hierarchical_LineOfSight(const Vector3 & UNUSED_PARAM(vStartPos), const Vector3 & UNUSED_PARAM(vEndPos), THierNodeAndLink * UNUSED_PARAM(pNodeList), const int UNUSED_PARAM(iNumNodes), Vector3 * UNUSED_PARAM(pvIsectPoints), int * UNUSED_PARAM(piMaxNumIsectPoints), int * UNUSED_PARAM(piNumIsectPoints)) { return false; }
void CPathServerThread::Hierarchical_StringPull(CPathRequest * UNUSED_PARAM(pPathRequest)) { }

#else

CHierarchicalNavLink *
CPathServerThread::Hierarchical_GetClosestLinkToPos(const Vector3 & vPos, const float fMaxSearchDist)
{
	const TNavMeshIndex iNavMesh = CPathServer::GetNavMeshIndexFromPosition(vPos);
	if(iNavMesh==NAVMESH_NAVMESH_INDEX_NONE)
		return NULL;

	CHierarchicalNavData * pNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex(iNavMesh);
	if(!pNavData)
		return NULL;

	u32 n, l;
	Vector3 vNodePos, vLinkNodePos, vClosestPosOnLink;

	const Vector3 vMin(vPos.x - fMaxSearchDist, vPos.y - fMaxSearchDist, vPos.z - fMaxSearchDist);
	const Vector3 vMax(vPos.x + fMaxSearchDist, vPos.y + fMaxSearchDist, vPos.z + fMaxSearchDist);

	float fMinDistSqrXY = FLT_MAX;
	CHierarchicalNavLink * pClosestLink = NULL;

	static const float fMaxZ = 8.0f;

	for(n=0; n<pNavData->GetNumNodes(); n++)
	{
		//*****************************************************
		// Examine nodes which are within the search distance

		CHierarchicalNavNode * pNode = pNavData->GetNode(n);
		pNode->GetNodePosition(vNodePos, pNavData->GetMin(), pNavData->GetSize());

		if(vNodePos.x < vMin.x || vNodePos.y < vMin.y || vNodePos.z < vMin.z ||
			vNodePos.x > vMax.x || vNodePos.y > vMax.y || vNodePos.z > vMax.z)
			continue;

		//*************************************************************************************
		// For this node, examine all of its links and see if the position is on any of them

		const u32 iNumLinks = pNode->GetNumLinks();

		for(l=0; l<iNumLinks; l++)
		{
			CHierarchicalNavLink * pLink = pNavData->GetLink(pNode->GetStartOfLinkData()+l);
			CHierarchicalNavData * pLinkEndNav = CPathServer::GetHierarchicalNavFromNavMeshIndex(pLink->GetNavMeshIndex());
			if(!pLinkEndNav)
				continue;
			CHierarchicalNavNode * pLinkEndNode = pLinkEndNav->GetNode(pLink->GetNodeIndex());
			pLinkEndNode->GetNodePosition(vLinkNodePos, pLinkEndNav->GetMin(), pLinkEndNav->GetSize());
			const Vector3 vDiff = vLinkNodePos - vNodePos;
			const float fTVal = (vDiff.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vNodePos, vDiff, vPos) : 0.0f;
			vClosestPosOnLink = vNodePos + ((vLinkNodePos - vNodePos) * fTVal);

			if(Abs(vClosestPosOnLink.z) > fMaxZ)
				continue;

			// TODO: Check that this point is actually on the link, using the left/right width values

			const float fDistSqrXY = (vLinkNodePos - vNodePos).XYMag2();
			if(fDistSqrXY < fMinDistSqrXY)
			{
				pClosestLink = pLink;
				fMinDistSqrXY = fDistSqrXY;
			}
		}
	}

	return pClosestLink;
}

//***********************************************************************************************************

void
CPathServerThread::Hierarchical_GetClosestNodeToPos(
	const Vector3 & vPos,
	const float fMaxSearchDistSqr,
	CHierarchicalNavNode ** ppOut_ClosestNode,
	CHierarchicalNavNode ** ppOut_ClosestPavementNode,
	CHierarchicalNavNode ** ppOut_ClosestNonIsolatedNode)
{
	// We'll just use the hierarchical navdata underfoot for now.
	// In due course we will need to examine all 4 within fMaxSearchDistSqr...
	const int iNumNavMeshes = 1;
	TNavMeshIndex iNavMeshes[4];
	iNavMeshes[0] = CPathServer::GetNavMeshIndexFromPosition(vPos);

	Vector3 vNodePos;
	float fDistSqr;

	float fClosestNodeDistSqr = FLT_MAX;
	float fClosestPavementNodeDistSqr = FLT_MAX;

	CHierarchicalNavNode * pClosestNode = NULL;
	CHierarchicalNavNode * pClosestPavementNode = NULL;

	for(int i=0; i<iNumNavMeshes; i++)
	{
		const TNavMeshIndex iIndex = iNavMeshes[0];
		Assert(iIndex <= NAVMESH_MAX_MAP_INDEX);

		if(CPathServer::GetHierarchicalNavFromNavMeshIndex(iIndex))
		{
			CHierarchicalNavData * pHierNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex(iIndex);
			Assert(pHierNavData);

			const Vector3 & vNavMins = pHierNavData->GetMin();
			const Vector3 & vNavSize = pHierNavData->GetSize();

			for(u32 n=0; n<pHierNavData->GetNumNodes(); n++)
			{
				CHierarchicalNavNode * pNode = pHierNavData->GetNode(n);
				pNode->GetNodePosition(vNodePos, vNavMins, vNavSize);

				fDistSqr = (vNodePos - vPos).Mag2();
				if(fDistSqr < fClosestNodeDistSqr)
				{
					fClosestNodeDistSqr = fDistSqr;
					pClosestNode = pNode;
				}
				if(pNode->GetPedDensity() && fDistSqr < fClosestPavementNodeDistSqr)
				{
					fClosestPavementNodeDistSqr = fDistSqr;
					pClosestPavementNode = pNode;
				}
			}
		}
	}

	if(fClosestNodeDistSqr < fMaxSearchDistSqr && ppOut_ClosestNode)
	{
		*ppOut_ClosestNode = pClosestNode;
	}
	if(fClosestPavementNodeDistSqr < fMaxSearchDistSqr && ppOut_ClosestPavementNode)
	{
		*ppOut_ClosestPavementNode = pClosestPavementNode;
	}
	// TODO: the same for non-isolated nodes
	if(ppOut_ClosestNonIsolatedNode)
		*ppOut_ClosestNonIsolatedNode = NULL;
}

bool
CPathServerThread::Hierarchical_FindPath(CPathRequest * pPathRequest)
{
	static const float fStartNodeSearchDistSqr = 20.0f*20.0f;
	static const float fEndNodeSearchDistSqr = 20.0f*20.0f;

	pPathRequest->m_iCompletionCode = PATH_ENDNODES_NOT_FOUND;
	const Vector3 & vStartPos = pPathRequest->m_vPathStart;
	const Vector3 & vEndPos = pPathRequest->m_vPathEnd;

	const bool bHasPathEnd = (!pPathRequest->m_bWander && !pPathRequest->m_bFleeTarget);

	// Find the start node.  For wander paths we want to make sure we choose a pavement or non-isolated poly
	CHierarchicalNavNode * pStartNodeNormal = NULL, * pStartNodePavement = NULL, * pStartNodeNonIsolated = NULL;
	Hierarchical_GetClosestNodeToPos(vStartPos, fStartNodeSearchDistSqr, &pStartNodeNormal, &pStartNodePavement, &pStartNodeNonIsolated);
	CHierarchicalNavNode * pStartNode = NULL;

	if(pPathRequest->m_bWander)
	{
		if(pStartNodePavement) pStartNode = pStartNodePavement;
		else if(pStartNodeNonIsolated) pStartNode = pStartNodeNonIsolated;
		else pStartNode = pStartNodeNormal;
	}
	else
	{
		pStartNode = pStartNodeNormal;
	}

	if(!pStartNode)
		return false;

	CHierarchicalNavNode * pEndNode = NULL;

	if(bHasPathEnd)
	{
		Hierarchical_GetClosestNodeToPos(vEndPos, fEndNodeSearchDistSqr, &pEndNode, NULL, NULL);
	}

	if(bHasPathEnd && !pEndNode)
		return false;

	CHierarchicalNavData * pStartNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex(pStartNode->GetNavMeshIndex());
	if(!pStartNavData)
		return false;

	CHierarchicalNavData * pEndNavData = bHasPathEnd ? CPathServer::GetHierarchicalNavFromNavMeshIndex(pEndNode->GetNavMeshIndex()) : NULL;
	if(bHasPathEnd && !pEndNavData)
		return false;

	pPathRequest->m_iCompletionCode = PATH_NOT_FOUND;

	Vector3 vStartNodePos, vEndNodePos;
	pStartNode->GetNodePosition(vStartNodePos, pStartNavData->GetMin(), pStartNavData->GetSize());
	if(bHasPathEnd)
		pEndNode->GetNodePosition(vEndNodePos, pEndNavData->GetMin(), pEndNavData->GetSize());


	//********************************************
	// Start performance timer for the pathsearch

#if !__FINAL
	m_PerfTimer->Reset();
	m_PerfTimer->Start();
#endif

	CHierarchicalNavNode * pFinishingNode = NULL;
	CHierarchicalNavNode * pClosestAlternativeFinishingNode = NULL;
	float fClosestAlternativeFinishDist = FLT_MAX;
	float fFurthestAlternativeFinishDist = 0.0f;

	const float fRefDistSqr = (pPathRequest->m_fReferenceDistance * pPathRequest->m_fReferenceDistance);

	m_Vars.m_VisitedHierarchicalPathNodes[0] = pStartNode;
	m_Vars.m_iNumVisitedNodes = 1;

	//*********************************************
	// Put the start node into the priority queue

	pStartNode->SetIsOpen(true);

	const float fInitialCost = (vEndPos - vStartPos).Mag();

	m_PathSearchPriorityQueue->Clear();
	m_PathSearchPriorityQueue->Insert(
		fInitialCost,
		0.0f,
		pStartNode,
		vStartNodePos,
		(pPathRequest->m_bWander) ? pPathRequest->m_vReferenceVector : VEC3_ZERO,
		0
	);

	TBinHeapNodeVars parentNodeVars;
	Vector3 vLinkedNodePos;

	while(m_PathSearchPriorityQueue->RemoveTop(
		parentNodeVars.fCost,
		parentNodeVars.fDistanceTravelled,
		parentNodeVars.pNode,
		parentNodeVars.vPosition,
		parentNodeVars.vDirFromPrev,
		parentNodeVars.iFlags))
	{
		const u32 iParentNavMeshIndex = parentNodeVars.pNode->GetNavMeshIndex();
		CHierarchicalNavData * pParentNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex(iParentNavMeshIndex);
		const u32 iParentNodeIndex = pParentNavData->GetNodeIndex(parentNodeVars.pNode);

		Assert(parentNodeVars.pNode->GetIsOpen());
		Assert(!parentNodeVars.pNode->GetIsClosed());

		//*********************************************
		// Have the path completion criteria been met?

		if(pPathRequest->m_bWander || pPathRequest->m_bFleeTarget)
		{
			const float fDistSqrFromStart = (parentNodeVars.vPosition - vStartPos).Mag2();
			if(fDistSqrFromStart > fRefDistSqr)
				break;
			if(fDistSqrFromStart > fFurthestAlternativeFinishDist)
			{
				fClosestAlternativeFinishDist = fDistSqrFromStart;
				pClosestAlternativeFinishingNode = parentNodeVars.pNode;
			}
		}
		else
		{
			if(parentNodeVars.pNode == pEndNode)
			{
				pFinishingNode = parentNodeVars.pNode;
				pPathRequest->m_iCompletionCode = PATH_FOUND;
				break;
			}
			const float fDistSqrFromEnd = (parentNodeVars.vPosition - vEndPos).Mag2();
			if(fDistSqrFromEnd < fEndNodeSearchDistSqr && fDistSqrFromEnd < fClosestAlternativeFinishDist)
			{
				fClosestAlternativeFinishDist = fDistSqrFromEnd;
				pClosestAlternativeFinishingNode = parentNodeVars.pNode;
			}
		}

		//**********************************************************************
		// Visit the surrounding nodes which are not already in the closed list

		const u32 iLinksStart = parentNodeVars.pNode->GetStartOfLinkData();

		const u32 iNumLinks = parentNodeVars.pNode->GetNumLinks();
		for(u32 l=0; l<iNumLinks; l++)
		{
			CHierarchicalNavLink * pLink = pParentNavData->GetLink(iLinksStart + l);
			CHierarchicalNavData * pLinkedNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex(pLink->GetNavMeshIndex());

			if(!pLinkedNavData)
				continue;

			CHierarchicalNavNode * pLinkedNode = pLinkedNavData->GetNode(pLink->GetNodeIndex());

			//************************************************************
			// Check for any conditions which rule out visiting this node

			// Already closed/visited?
			if(pLinkedNode->GetIsClosed() || (pLinkedNode->GetIsOpen() && CPathServer::ms_bDontRevisitOpenNodes))
				continue;

			if(pPathRequest->m_bWander)
			{
				// Cannot wander off pavement.  TODO: Make this use PAVEMENT and not PED-DENSITY..
				if(parentNodeVars.pNode->GetPedDensity() && !pLinkedNode->GetPedDensity())
					continue;
				// Cannot wander onto a node which has ped spawning disabled (likely via a script cmd)
				if(parentNodeVars.pNode->GetPedSpawningEnabled() && !pLinkedNode->GetPedSpawningEnabled())
					continue;
			}

			pathAssertf(!(pPathRequest->m_bWander && pStartNode->GetPedDensity()>0 && pLinkedNode->GetPedDensity()==0), "WTF!?!?!?");

			//********************************************
			// Calculate the cost for moving to this node

			pLinkedNode->GetNodePosition(vLinkedNodePos, pLinkedNavData->GetMin(), pLinkedNavData->GetSize());

			Vector3 vFromParent = vLinkedNodePos - parentNodeVars.vPosition;
			Vector3 vToTarget = vEndPos - vLinkedNodePos;
			float fDistFromParent = vFromParent.InvMagFast();
			float fDistToTarget = vToTarget.InvMagFast();
			vFromParent.Scale(fDistFromParent);
			vToTarget.Scale(fDistToTarget);
			fDistFromParent = 1.0f / fDistFromParent;
			fDistToTarget = 1.0f / fDistToTarget;

			float fCost;

			if(pPathRequest->m_bFleeTarget)
			{
				fCost = 0.0f;
			}
			else if(pPathRequest->m_bWander)
			{
				const float fDirDot = DotProduct(parentNodeVars.vDirFromPrev, vFromParent);
				static const float fDirectionPenalty = 50.0f;
				if(fDirDot < 0.0f)
				{
					// Incur an extra penalty for going back on oneself
					fCost = (fDirectionPenalty * -fDirDot) + fDirectionPenalty;
				}
				else
				{
					// Penalize going ahead based on direction difference
					fCost = (1.0f - fDirDot) * fDirectionPenalty;
				}
			}
			else
			{
				static const float fCostFromTargetMultiplier = 16.0f;

				//fCost = parentNodeVars.fCost + parentNodeVars.fDistanceTravelled + (fDistToTarget * fCostFromTargetMultiplier);
				fCost = parentNodeVars.fCost +
					parentNodeVars.fDistanceTravelled + fDistFromParent + ((fDistToTarget*fDistToTarget) * fCostFromTargetMultiplier);
			}

			CPathServerBinHeap::Node * pBinHeapNode = NULL;

			if(pLinkedNode->GetIsOpen())
			{
				pBinHeapNode = m_PathSearchPriorityQueue->GetBinHeap()->FindNode(pLinkedNode);
				if(pBinHeapNode && pBinHeapNode->Key <= fCost)
					continue;
			}
			else
			{
				pLinkedNode->SetIsOpen(true);
			}

			pLinkedNode->SetParentNavMeshIndex(iParentNavMeshIndex);
			pLinkedNode->SetParentNodeIndex(iParentNodeIndex);

			if(pBinHeapNode)
			{
				pBinHeapNode->vPointInPoly[0] = vLinkedNodePos.x;
				pBinHeapNode->vPointInPoly[1] = vLinkedNodePos.y;
				pBinHeapNode->vPointInPoly[2] = vLinkedNodePos.z;
				pBinHeapNode->vDirFromPrevious[0] = vFromParent.x;
				pBinHeapNode->vDirFromPrevious[1] = vFromParent.y;
				pBinHeapNode->vDirFromPrevious[2] = vFromParent.z;
				pBinHeapNode->fDistanceTravelled = parentNodeVars.fDistanceTravelled + fDistFromParent;
				m_PathSearchPriorityQueue->GetBinHeap()->DecreaseKey(pBinHeapNode, fCost);
			}
			else
			{
				if(m_Vars.m_iNumVisitedNodes == MAX_PATH_VISITED_NODES)
					break;

				m_Vars.m_VisitedHierarchicalPathNodes[m_Vars.m_iNumVisitedNodes] = pLinkedNode;
				m_Vars.m_iNumVisitedNodes++;

				#if __DEV
				if(CPathServer::ms_bMarkVisitedPolys)
					pLinkedNode->SetIsDebugMarked(true);
				#endif

				if(!m_PathSearchPriorityQueue->Insert(
					fCost,
					parentNodeVars.fDistanceTravelled + fDistFromParent,
					pLinkedNode, vLinkedNodePos, vFromParent, 0))
				{
					// We've run out of space in "m_PathStackEntryStore" for the path-search..
					pPathRequest->m_bRequestActive = false;
					pPathRequest->m_bComplete = true;
					pPathRequest->m_iNumPoints = 0;
					pPathRequest->m_iCompletionCode = PATH_RAN_OUT_OF_PATH_STACK_SPACE;
					return false;
				}
			}
		}

		//*************************************
		// Now mark the parent node as closed

		parentNodeVars.pNode->SetIsClosed(true);
	}

	if(!pFinishingNode && pClosestAlternativeFinishingNode)
	{
		pFinishingNode = pClosestAlternativeFinishingNode;
		pPathRequest->m_iCompletionCode = PATH_FOUND;
	}

#if !__FINAL
	m_PerfTimer->Stop();
	pPathRequest->m_fMillisecsToFindPath = (float) m_PerfTimer->GetTimeMS();
#endif

	//***************************************************************
	// Path-searching has completed, now trace back the path

	if(pPathRequest->m_iCompletionCode == PATH_FOUND)
	{
		Assert(pFinishingNode);

		m_Vars.m_iNumPathNodes = 0;
		CHierarchicalNavNode * pNodePtr = pFinishingNode;

		while(m_Vars.m_iNumPathNodes < MAX_HIERARCHICAL_PATH_NODES)
		{
			Assert(pNodePtr);

			m_Vars.m_HierarchicalPathNodes[m_Vars.m_iNumPathNodes].m_pNode = pNodePtr;
			m_Vars.m_HierarchicalPathNodes[m_Vars.m_iNumPathNodes].m_pLink = NULL;
			m_Vars.m_iNumPathNodes++;

			//*****************************************
			// Obtain the pointer to the parent node.

			const u32 iParentNavmesh = pNodePtr->GetParentNavMeshIndex();
			if(iParentNavmesh == NAVMESH_NAVMESH_INDEX_NONE)
				break;
			CHierarchicalNavData * pParentNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex(iParentNavmesh);
			Assert(pParentNavData);
			if(!pParentNavData)
				break;

			pNodePtr = pParentNavData->GetNode(pNodePtr->GetParentNodeIndex());
		}

		//***************************************************************************************************
		// Now we'll need to reverse the array because it currently goes from pFinishingNode -> pStartNode

		int i,l;

		const int iHalfSize = m_Vars.m_iNumPathNodes/2;
		for(i=0; i<iHalfSize; i++)
		{
			pNodePtr = m_Vars.m_HierarchicalPathNodes[i].m_pNode;
			m_Vars.m_HierarchicalPathNodes[i].m_pNode = m_Vars.m_HierarchicalPathNodes[m_Vars.m_iNumPathNodes-i-1].m_pNode;
			m_Vars.m_HierarchicalPathNodes[m_Vars.m_iNumPathNodes-i-1].m_pNode = pNodePtr;
		}

		//*****************************************************************************
		// Now go through and fill in the links which are stored in the list of nodes

		CHierarchicalNavNode * pNode = m_Vars.m_HierarchicalPathNodes[0].m_pNode;
		CHierarchicalNavData * pNodeNav = CPathServer::GetHierarchicalNavFromNavMeshIndex(pNode->GetNavMeshIndex());

		for(i=0; i<m_Vars.m_iNumPathNodes-1; i++)
		{
			CHierarchicalNavNode * pNextNode = m_Vars.m_HierarchicalPathNodes[i+1].m_pNode;
			CHierarchicalNavData * pNextNodeNav = CPathServer::GetHierarchicalNavFromNavMeshIndex(pNextNode->GetNavMeshIndex());

			s32 iNumLinks = pNode->GetNumLinks();
			for(l=0; l<iNumLinks; l++)
			{
				CHierarchicalNavLink * pLink = pNodeNav->GetLink(pNode->GetStartOfLinkData() + l);

				if(pLink->GetNavMeshIndex()==pNextNode->GetNavMeshIndex() && pLink->GetNodeIndex()==pNextNodeNav->GetNodeIndex(pNextNode))
				{
					m_Vars.m_HierarchicalPathNodes[i].m_pLink = pLink;
					break;
				}
			}
			Assert(l != iNumLinks);	// have to find the link!

			pNode = pNextNode;
			pNodeNav = pNextNodeNav;
		}

		//***********************************************************************************************
		// Do the string-pulling which uses the link width left/right values to provide some measure of
		// the free space along each node link, so we can attempt to remove some of the kinks in the
		// path and create a better shortest-path route

#if !__FINAL
		m_PerfTimer->Reset();
		m_PerfTimer->Start();
#endif

		Hierarchical_StringPull(pPathRequest);

#if !__FINAL
		m_PerfTimer->Stop();
		pPathRequest->m_fMillisecsToRefinePath = (float) m_PerfTimer->GetTimeMS();
#endif

		return true;
	}
	else
	{
		return false;
	}
}

//***************************************************************************************
// Hierarchical_LineOfSight
// Performs a line of sight from start to end, using the left/right width values of each
// link to define a flat volume in which visibility is possible.

bool
CPathServerThread::Hierarchical_LineOfSight(const Vector3 & vStartPos, const Vector3 & vEndPos, THierNodeAndLink * pNodeList, const int iNumNodes, Vector3 * ASSERT_ONLY(pvIsectPoints), int * ASSERT_ONLY(piMaxNumIsectPoints), int * piNumIsectPoints)
{
	Assert((!pvIsectPoints && !piMaxNumIsectPoints && !piNumIsectPoints) || (pvIsectPoints && piMaxNumIsectPoints && piNumIsectPoints));

	if(piNumIsectPoints)
		*piNumIsectPoints = 0;

	Vector3 vCurrentPos, vNextPos, vNextAgainPos;
	Vector3 vIntersectCurr, vIntersectNext;

	for(int i=0; i<iNumNodes; i++)
	{
		THierNodeAndLink * pCurrent = &pNodeList[i];
		THierNodeAndLink * pNext = &pNodeList[i+1];
		THierNodeAndLink * pNextAgain = &pNodeList[i+2];

		CHierarchicalNavNode * pCurrentNode = pCurrent->m_pNode;
		CHierarchicalNavData * pCurrentNav = CPathServer::GetHierarchicalNavFromNavMeshIndex(pCurrentNode->GetNavMeshIndex());
		pCurrentNode->GetNodePosition(vCurrentPos, pCurrentNav->GetMin(), pCurrentNav->GetSize());

		CHierarchicalNavNode * pNextNode = pNext->m_pNode;
		CHierarchicalNavData * pNextNav = CPathServer::GetHierarchicalNavFromNavMeshIndex(pNextNode->GetNavMeshIndex());
		pNextNode->GetNodePosition(vNextPos, pNextNav->GetMin(), pNextNav->GetSize());

		CHierarchicalNavNode * pNextAgainNode = pNextAgain->m_pNode;
		CHierarchicalNavData * pNextAgainNav = CPathServer::GetHierarchicalNavFromNavMeshIndex(pNextAgainNode->GetNavMeshIndex());
		pNextAgainNode->GetNodePosition(vNextAgainPos, pNextAgainNav->GetMin(), pNextAgainNav->GetSize());

		const bool bClockwiseTurn = ( CrossProduct(vNextPos - vCurrentPos, vNextAgainPos - vNextPos).z < 0.0f );

		if(bClockwiseTurn)
		{
			if(pCurrent->m_pLink->GetWidthToRight()==0)
				return false;
		}
		else
		{
			if(pCurrent->m_pLink->GetWidthToLeft()==0)
				return false;
		}

		Vector3 vCurrNearLeft, vCurrNearRight, vCurrFarLeft, vCurrFarRight;
		pCurrent->m_pLink->GetLinkExtents(vCurrentPos, vNextPos, vCurrNearLeft, vCurrNearRight, vCurrFarLeft, vCurrFarRight);

		Vector3 vNextNearLeft, vNextNearRight, vNextFarLeft, vNextFarRight;
		pNext->m_pLink->GetLinkExtents(vNextPos, vNextAgainPos, vNextNearLeft, vNextNearRight, vNextFarLeft, vNextFarRight);

		// Intersect vStart->vEnd with the left edges of the current & next link
		const int iIsectCurrLeft = CNavMesh::LineSegsIntersect2D(vStartPos, vEndPos, vCurrNearLeft, vCurrFarLeft, &vIntersectCurr);
		const int iIsectNextLeft = CNavMesh::LineSegsIntersect2D(vStartPos, vEndPos, vNextNearLeft, vNextFarLeft, &vIntersectNext);

		if((iIsectCurrLeft==SEGMENTS_INTERSECT && iIsectNextLeft==SEGMENTS_INTERSECT) ||
			(iIsectCurrLeft==SEGMENTS_INTERSECT && iIsectNextLeft==LINES_INTERSECT) ||
			(iIsectCurrLeft==LINES_INTERSECT && iIsectNextLeft==SEGMENTS_INTERSECT))
		{
			if((vIntersectCurr - vStartPos).Mag2() < (vIntersectNext - vStartPos).Mag2())
				return false;
		}

		const int iIsectCurrRight = CNavMesh::LineSegsIntersect2D(vStartPos, vEndPos, vCurrNearRight, vCurrFarRight, &vIntersectCurr);
		const int iIsectNextRight = CNavMesh::LineSegsIntersect2D(vStartPos, vEndPos, vNextNearRight, vNextFarRight, &vIntersectNext);

		if((iIsectCurrRight==SEGMENTS_INTERSECT && iIsectNextRight==SEGMENTS_INTERSECT) ||
			(iIsectCurrRight==SEGMENTS_INTERSECT && iIsectNextRight==LINES_INTERSECT) ||
			(iIsectCurrRight==LINES_INTERSECT && iIsectNextRight==SEGMENTS_INTERSECT))
		{
			if((vIntersectCurr - vStartPos).Mag2() < (vIntersectNext - vStartPos).Mag2())
				return false;
		}
	}

	return true;
}

void
CPathServerThread::Hierarchical_StringPull(CPathRequest * pPathRequest)
{
	// Add the initial node.
	CHierarchicalNavData * pNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex( m_Vars.m_HierarchicalPathNodes[0].m_pNode->GetNavMeshIndex() );
	m_Vars.m_HierarchicalPathNodes[0].m_pNode->GetNodePosition(pPathRequest->m_PathPoints[0], pNavData->GetMin(), pNavData->GetSize());
	pPathRequest->m_iNumPoints = 1;

	if(m_Vars.m_iNumPathNodes > 2)
	{
		int iStartIndex = 0;
		int iTestIndex = 2;

		bool bQuit = false;
		while(!bQuit)
		{
			Vector3 vStartPos, vEndPos;

			CHierarchicalNavNode * pStartNode = m_Vars.m_HierarchicalPathNodes[iStartIndex].m_pNode;
			CHierarchicalNavData * pStartNav = CPathServer::GetHierarchicalNavFromNavMeshIndex(pStartNode->GetNavMeshIndex());
			pStartNode->GetNodePosition(vStartPos, pStartNav->GetMin(), pStartNav->GetSize());

			CHierarchicalNavNode * pEndNode = m_Vars.m_HierarchicalPathNodes[iTestIndex].m_pNode;
			CHierarchicalNavData * pEndNav = CPathServer::GetHierarchicalNavFromNavMeshIndex(pEndNode->GetNavMeshIndex());
			pEndNode->GetNodePosition(vEndPos, pEndNav->GetMin(), pEndNav->GetSize());

//			int iNumIsectPts = 0;
//			int iMaxNumIsectPts = MAX_NUM_PATH_POINTS - pPathRequest->m_iNumPoints;
			
			const bool bLOS = Hierarchical_LineOfSight(
				vStartPos, vEndPos, &m_Vars.m_HierarchicalPathNodes[iStartIndex], (iTestIndex-iStartIndex)-1,
				NULL, NULL, NULL
//				&pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints],
//				&iMaxNumIsectPts,
//				&iNumIsectPts
			);

			// If a line-of-sight exists, then proceed to the next node.
			if(bLOS)
			{
				iTestIndex++;
				if(iTestIndex >= m_Vars.m_iNumPathNodes)
					bQuit = true;

//				pPathRequest->m_iNumPoints += iNumIsectPts;
//				if(pPathRequest->m_iNumPoints >= MAX_NUM_PATH_POINTS)
//					bQuit = true;
			}
			else
			{
//				Assert(iNumIsectPts==0);

				pNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex( m_Vars.m_HierarchicalPathNodes[iTestIndex-1].m_pNode->GetNavMeshIndex() );
				m_Vars.m_HierarchicalPathNodes[iTestIndex-1].m_pNode->GetNodePosition(pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints], pNavData->GetMin(), pNavData->GetSize());
				pPathRequest->m_iNumPoints++;

				iStartIndex = iTestIndex-1;
				iTestIndex = iStartIndex + 2;

				if(iTestIndex >= m_Vars.m_iNumPathNodes || pPathRequest->m_iNumPoints >= MAX_NUM_PATH_POINTS)
					bQuit = true;
			}
		}
	}

	// Add the final node.
	if(pPathRequest->m_iNumPoints < MAX_NUM_PATH_POINTS)
	{
		pNavData = CPathServer::GetHierarchicalNavFromNavMeshIndex( m_Vars.m_HierarchicalPathNodes[m_Vars.m_iNumPathNodes-1].m_pNode->GetNavMeshIndex() );
		m_Vars.m_HierarchicalPathNodes[m_Vars.m_iNumPathNodes-1].m_pNode->GetNodePosition(pPathRequest->m_PathPoints[pPathRequest->m_iNumPoints], pNavData->GetMin(), pNavData->GetSize());
		pPathRequest->m_iNumPoints++;
	}
}

#endif // __HIERARCHICAL_NODES_ENABLED

