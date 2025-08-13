//rage headers
#include "math/vecmath.h"

//game headers
#include "vehicleAi\VehicleNodeList.h"
#include "Vehicles/vehicle.h"

VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//-----------------------------------------------------------------
//	CVehicleNodeList
//	A list of road nodes used by the vehicle AI, to determine the
//	vehicle's future direction - and to maintain a short history

CVehicleNodeList::CVehicleNodeList()
{
	Clear();
}
CVehicleNodeList::~CVehicleNodeList()
{
	// In __DEV builds we'll make sure we set these to invalid values
	// This is for protection against the use of stale pointer, when this
	// class lives inside tasks rather than as a member of CVehicle
#if __DEV
	for(s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		m_Entries[i].m_Address.SetEmpty();
		m_Entries[i].m_iLink = -1;
		m_Entries[i].m_iLaneIndex = -1;
	}
#endif
}
void CVehicleNodeList::Clear()
{
	m_targetNodeIndex = 1;
	m_bContainsUTurn = false;
	m_bDirty = false;
	for(s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		m_Entries[i].m_Address.SetEmpty();
		m_Entries[i].m_iLink = -1;
		m_Entries[i].m_iLaneIndex = 0;
	}
}
void CVehicleNodeList::ClearPathNodes()
{
	for(s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
		m_Entries[i].m_Address.SetEmpty();

	m_bDirty = true;
}
void CVehicleNodeList::ClearFutureNodes()
{
	for (s32 i = m_targetNodeIndex+1; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
		m_Entries[i].m_Address.SetEmpty();

	m_bDirty = true;
}
void CVehicleNodeList::ClearFrom(u32 iFirstNodeToClear)
{
	if (iFirstNodeToClear > 0)
	{
		m_Entries[iFirstNodeToClear - 1].m_iLink = -1;
	}

	for (u32 i = iFirstNodeToClear; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		m_Entries[i].m_iLink = -1;
		m_Entries[i].m_iLaneIndex = 0;
		m_Entries[i].m_Address.SetEmpty();
	}

	m_bDirty = true;
}
void CVehicleNodeList::ClearNonEssentialNodes()
{
	s32 i;
	for(i=0; i<m_targetNodeIndex-1; i++)
	{
		m_Entries[i].m_Address.SetEmpty();
	}
	for(i=m_targetNodeIndex+1; i<CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		m_Entries[i].m_Address.SetEmpty();
	}

	m_bDirty = true;
}
void CVehicleNodeList::ClearLanes()
{
	for(s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
		m_Entries[i].m_iLaneIndex = 0;

	m_bDirty = true;
}
void CVehicleNodeList::ShiftNodesByOne()
{
	DEV_ONLY(CheckLinks();)

	for(s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED-1; i++)
	{
		m_Entries[i].m_Address = m_Entries[i+1].m_Address;
		m_Entries[i].m_iLink = m_Entries[i+1].m_iLink;
		m_Entries[i].m_iLaneIndex = m_Entries[i+1].m_iLaneIndex;
	}
	m_Entries[CAR_MAX_NUM_PATHNODES_STORED-1].m_Address.SetEmpty();
	m_Entries[CAR_MAX_NUM_PATHNODES_STORED-1].m_iLink = -1;
	m_Entries[CAR_MAX_NUM_PATHNODES_STORED-1].m_iLaneIndex = 0;

	m_bDirty = true;
}
void CVehicleNodeList::ShiftNodesByOneWrongWay()
{
	for(s32 i = CAR_MAX_NUM_PATHNODES_STORED-2; i >= 0; i--)
	{
		m_Entries[i+1].m_Address = m_Entries[i].m_Address;
		m_Entries[i+1].m_iLink = m_Entries[i].m_iLink;
		m_Entries[i+1].m_iLaneIndex = m_Entries[i].m_iLaneIndex;
	}
	m_Entries[0].m_Address.SetEmpty();
	m_Entries[0].m_iLink = -1;
	m_Entries[0].m_iLaneIndex = 0;

	m_bDirty = true;

	DEV_ONLY(CheckLinks();)
}
s32 CVehicleNodeList::FindNumPathNodes() const
{
	s32 iNodes = 0;

	for (int i=0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		if (!m_Entries[i].m_Address.IsEmpty())
		{
			++iNodes;
		}
		else
		{
			break;
		}
	}

	return iNodes;
}

s32 CVehicleNodeList::FindLastGoodNode() const
{
	s32 iNodes = CAR_MAX_NUM_PATHNODES_STORED-1;

	while(iNodes > 0 && m_Entries[iNodes].m_Address.IsEmpty())
	{
		iNodes--;
	}
	return iNodes;
}

void CVehicleNodeList::FindLinksWithNodes(const s32 iStartNode)
{
	m_bDirty = true;

	for(s32 i=iStartNode; i<CAR_MAX_NUM_PATHNODES_STORED-1; i++)
	{
		if (
			!m_Entries[i].m_Address.IsEmpty()
			&& !m_Entries[i+1].m_Address.IsEmpty()
			&& ThePaths.IsRegionLoaded(m_Entries[i].m_Address)
			/*&& ThePaths.IsRegionLoaded(m_Entries[i+1].m_Address)*/
			)
		{
			m_Entries[i].m_iLink = ThePaths.FindRegionLinkIndexBetween2Nodes(m_Entries[i].m_Address, m_Entries[i+1].m_Address);
		}
		else
		{
			m_Entries[i].m_iLink = -1;
		}
	}
	DEV_ONLY(CheckLinks();)
}
void CVehicleNodeList::FindLanesWithNodes(const s32 iStartNode, const Vector3* pvStartPos, const bool bMayRandomizeLanes)
{
	s32 i = iStartNode;
	m_bDirty = true;
	while(i < CAR_MAX_NUM_PATHNODES_STORED)
	{
		if (i==0 && pvStartPos)
		{
			//if we're starting out, find the closest lane to start in
			if (!m_Entries[0].m_Address.IsEmpty() 
				&& !m_Entries[1].m_Address.IsEmpty()
				&& ThePaths.IsRegionLoaded(GetPathNodeAddr(0).GetRegion()))
			{
				m_Entries[0].m_iLaneIndex = (s8)ThePaths.FindLaneForVehicleWithLink(GetPathNodeAddr(0), GetPathNodeAddr(1)
					, *ThePaths.FindLinkPointerSafe(GetPathNodeAddr(0).GetRegion(),GetPathLinkIndex(0)), *pvStartPos);
			}
			else
			{
				m_Entries[0].m_iLaneIndex = 0;
			}
		}
		else if (i==0 || m_Entries[i-1].m_Address.IsEmpty() || !ThePaths.IsRegionLoaded(m_Entries[i-1].m_Address))
		{
			m_Entries[i].m_iLaneIndex = 0;
		}
		else
		{
			//did we reach the end of our valid links?
			const int iCurrLinkIndex = GetPathLinkIndex(i-1);
			if (iCurrLinkIndex < 0)
			{
				break;
			}

			//if we have knowledge of what came before, use it as a default
			SetPathLaneIndex(i, GetPathLaneIndex(i-1));
			const CNodeAddress& CurrNode = GetPathNodeAddr(i-1);
			const CPathNode* pCurrNode = ThePaths.FindNodePointerSafe(CurrNode);
			Assert(pCurrNode);
			Assert(ThePaths.IsRegionLoaded(CurrNode));

			CPathNodeLink *pLink = ThePaths.FindLinkPointerSafe(CurrNode.GetRegion(),iCurrLinkIndex);
			Assert(pLink);

			if(!pLink || !pCurrNode)
			{
				break;
			}
			
			const s8 LanesToChooseFrom = pLink->m_1.m_LanesToOtherNode;
			if (i > 1 && !m_Entries[i-2].m_Address.IsEmpty() && ThePaths.IsRegionLoaded(m_Entries[i-2].m_Address))
			{
				const CNodeAddress& FromNode = GetPathNodeAddr(i-2);
				//CPathNode* pFromNode = ThePaths.FindNodePointer(FromNode);
				Assert(ThePaths.IsRegionLoaded(FromNode));

				//exceptions: going onto a road with more lanes
				const int iFromLinkIndex = GetPathLinkIndex(i-2);
				Assert(iFromLinkIndex >= 0);
				CPathNodeLink *pFromLink = ThePaths.FindLinkPointerSafe(FromNode.GetRegion(),iFromLinkIndex);
				Assert(pFromLink);
				if(!pFromLink)
				{
					break;
				}

				const s32 LanesOnCurrentLink = pFromLink->m_1.m_LanesToOtherNode;
				if (LanesToChooseFrom > LanesOnCurrentLink)
				{
					float fraction = bMayRandomizeLanes ? (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) + GetPathLaneIndex(i-1)) / rage::Max(1, LanesOnCurrentLink) : 0.5f;
					//Assertf(fraction >= 0.0f && fraction <= 1.0f, "fraction %.2f < 0.0f || >1.0f, crsNode1: %.2f, %.2f, crsNode2: %.2f, %.2f"
					//	, fraction, crsNode1.x, crsNode1.y, crsNode2.x, crsNode2.y);
					fraction = Clamp(fraction, 0.0f, 1.0f);
					SetPathLaneIndex(i, ((s8)(fraction * LanesToChooseFrom)) );
				}

				//junctions
				if(pCurrNode->NumLinks() > 2)		// Only for junctions.
				{
					float	leftness;
					float	fDirectionDot;
					static dev_float thr = 0.92f;
					bool bSharpTurn = false;
					const u32 PathDirection = CPathNodeRouteSearchHelper::FindPathDirection(FromNode, CurrNode, GetPathNodeAddr(i), &bSharpTurn, &leftness, thr, &fDirectionDot);

					if(GetPathLaneIndex(i-1) == LanesOnCurrentLink - 1)		// If we're in the outermost lane(to the right)
					{
						if(PathDirection == BIT_TURN_RIGHT)								// and we're turning right
						{
							s8 iLane = LanesToChooseFrom - 1;
							iLane = rage::Max(iLane, (s8)0);
							SetPathLaneIndex(i, iLane);	// go to the far right lane
						}
					}

					if(GetPathLaneIndex(i-1) == 0)							// If we're in the innermost lane(to the left)
					{
						if(PathDirection == BIT_TURN_LEFT)									// and we're turning left
						{
							SetPathLaneIndex(i, 0);		// go to the far left lane
						}
					}

					//and clamp it to our next link's number of nodes regardless
					s8 iNextPathLaneIndex = GetPathLaneIndex(i);
					iNextPathLaneIndex = rage::Min(iNextPathLaneIndex, (s8)(LanesToChooseFrom-1));
					iNextPathLaneIndex = rage::Max(iNextPathLaneIndex, (s8)0);
					SetPathLaneIndex(i, iNextPathLaneIndex);

					//whatever we chose for after the turn, apply that to the junction
					SetPathLaneIndex(i-1, GetPathLaneIndex(i));
				}
			}
			else
			{
				//still clamp it anyway
				s8 iLane = GetPathLaneIndex(i);
				iLane = rage::Min(iLane, (s8)(LanesToChooseFrom - 1));
				iLane = rage::Max(iLane, (s8)0);
				SetPathLaneIndex(i, iLane);
			}			
		}
		
		i++;
	}
	return;
}
bool CVehicleNodeList::GetClosestPosOnPath(const Vector3 & vPos, Vector3 & vClosestPos_out, int & iPathIndexClosest_out, float & iPathSegmentPortion_out, const int iStartNode, const int iEndNode) const
{
	iPathIndexClosest_out = -1;

	float fPathSegClosestDistSqr = FLT_MAX; //100000.0f;// Large sentinel value.
	Vector3 nodeAPos, nodeBPos;
	Vector3 vClosestPos;
	for(s32 iAutopilotNodeIndex = iStartNode; iAutopilotNodeIndex < iEndNode; ++iAutopilotNodeIndex)
	{
		const CPathNode* pNodeA = GetPathNode(iAutopilotNodeIndex);
		if(!pNodeA){ continue; }

		const CPathNode* pNodeB = GetPathNode(iAutopilotNodeIndex+1);
		if(!pNodeB){ continue; }

		pNodeA->GetCoors(nodeAPos);
		pNodeB->GetCoors(nodeBPos);

		// Find the closest position to this knot and portion along it's segment to the next knot.
		const Vector3 nodeABDelta = nodeBPos - nodeAPos;
		const Vector3 nodeAToPosDelta(vPos - nodeAPos);
		const Vector3 nodeAToPosDeltaFlat(nodeAToPosDelta.x, nodeAToPosDelta.y, 0.0f);
		float dotPr = nodeAToPosDeltaFlat.Dot(nodeABDelta);
		Vector3 closestPointOnSegment = nodeAPos;
		float segmentPortion = 0.0f;
		if(dotPr > 0.0f)
		{
			const float length2 = nodeABDelta.Mag2();
			dotPr /= length2;
			if (dotPr >= 1.0f)
			{
				closestPointOnSegment = nodeBPos;
				segmentPortion = 1.0f;
			}
			else
			{
				closestPointOnSegment = nodeAPos + nodeABDelta * dotPr;
				segmentPortion = dotPr;
			}
		}
		closestPointOnSegment.z = 0.0f;

		// Check if this is the closest one so far.
		const Vector3 vPosFlat(vPos.x, vPos.y, 0.0f);
		const float distToSegSqr = (vPosFlat - closestPointOnSegment).Mag2();
		if(distToSegSqr < fPathSegClosestDistSqr)
		{
			iPathIndexClosest_out = iAutopilotNodeIndex;
			iPathSegmentPortion_out = segmentPortion;

			fPathSegClosestDistSqr = distToSegSqr;
			vClosestPos = nodeAPos + (nodeABDelta * segmentPortion);
		}
	}

	vClosestPos_out = vClosestPos;

	return (iPathIndexClosest_out != -1);
}

bool CVehicleNodeList::JoinToRoadSystem(const CVehicle * pVehicle, const float fRejectDist)
{
	return JoinToRoadSystem(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()),
						VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()),
						(pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT),
						0,
						1,
						fRejectDist);
}

bool CVehicleNodeList::JoinToRoadSystem( const Vector3& vStartPosition, const Vector3& vStartOrientation, const bool bIncludeWaterNodes, s32 iCurrentNode, s32 iNewNode, const float fRejectDist, const bool bIgnoreDirection )
{
	// Set some default values in case things fail miserably
	ClearPathNodes();
	ClearLanes();

	m_bDirty = true;

	// We go through all(!) the nodes in the world and find the one that
	// is closest. Might have to be improved eventually.
	//	ClosestNode = ThePaths.FindNodeClosestToCoorsFavourDirection(pVeh->GetPosition(), pVeh->GetB().x, pVeh->GetB().y);

	CNodeAddress addrOld = GetPathNodeAddr(iCurrentNode);
	CNodeAddress addrNew = GetPathNodeAddr(iNewNode);
	s16 iPathLinkIndex = GetPathLinkIndex(iCurrentNode);
	s8 iPathLaneIndex = GetPathLaneIndex(iCurrentNode);

	bool bFoundNodePair = ThePaths.FindNodePairToMatchVehicle(
		vStartPosition,
		vStartOrientation,
		addrOld, addrNew,
		bIncludeWaterNodes,
		iPathLinkIndex,
		iPathLaneIndex,
		fRejectDist,
		bIgnoreDirection);

	if(!bFoundNodePair)
	{
		return false;
	}

	SetPathNodeAddr(iCurrentNode, addrOld);
	SetPathNodeAddr(iNewNode, addrNew);
	SetPathLinkIndex(iCurrentNode, iPathLinkIndex);

	SetPathLaneIndex(iCurrentNode, iPathLaneIndex);
	SetPathLaneIndex(iNewNode, iPathLaneIndex);

	if(GetPathNodeAddr(iCurrentNode).IsEmpty())
	{
		// This is pretty bad but it can happen if none of the nodes are loaded in yet.
#if __DEV
		CheckLinks();
#endif
		return false;
	}

	return true;
}

bool CVehicleNodeList::ContainsNode(const CNodeAddress & iNode) const
{
	if (iNode.IsEmpty())
	{
		return false;
	}

	for(s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		if (iNode == m_Entries[i].m_Address)
		{
			return true;
		}
	}

	return false;
}

s32 CVehicleNodeList::FindNodeIndex(const CNodeAddress & iNode) const
{
	if (iNode.IsEmpty())
	{
		return -1;
	}

	for(s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		if (iNode == m_Entries[i].m_Address)
		{
			return i;
		}
	}

	return -1;
}

bool CVehicleNodeList::GivesWayBeforeNextJunction() const
{
	for (int i = m_targetNodeIndex-1; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		const CPathNode* pCurNode = GetPathNode(i);
		if (!pCurNode)
		{
			return false;
		}

		//we reached a junction, guess we didn't give way first 
		if (pCurNode->IsJunctionNode())
		{
			return false;
		}

		if (pCurNode->IsGiveWay())
		{
			return true;
		}
	}

	return false;
}

bool CVehicleNodeList::IsTargetNodeAfterUTurn() const
{
	//see if our target node also appears in any previous position in the list
	if (m_targetNodeIndex <= 0 || m_targetNodeIndex >= CAR_MAX_NUM_PATHNODES_STORED)
	{
		//return on 0 bc nothing can be before it
		return false;
	}

	//if the path hasn't changed, return the cached value
	if (!m_bDirty)
	{
		return m_bContainsUTurn;
	}

	const CNodeAddress& targetNode = m_Entries[m_targetNodeIndex].m_Address;

	if (targetNode.IsEmpty())
	{
		return false;
	}

	for (int i = m_targetNodeIndex-1; i >= 0; i--)
	{
		if (m_Entries[i].m_Address == targetNode)
		{
			m_bContainsUTurn = true;
			m_bDirty = false;
			return true;
		}
	}

	m_bContainsUTurn = false;
	m_bDirty = false;
	return false;
}

bool CVehicleNodeList::IsPointWithinBoundariesOfSegment(const Vector3& vPos, const s32 iSegment) const
{
	if (!aiVerify(iSegment < CAR_MAX_NUM_PATHNODES_STORED-1))
	{
		return false;
	}

	Vector3 vLeftEnd, vRightEnd, vLeftStart, vRightStart;
	Vector3 vPosFlat = vPos;
	vPosFlat.z = 0.0f;
	CNodeAddress fromNode = GetPathNodeAddr(iSegment);
	CNodeAddress toNode = GetPathNodeAddr(iSegment+1);
	CNodeAddress nextNode;
	if (iSegment < CAR_MAX_NUM_PATHNODES_STORED-2)
	{
		nextNode = GetPathNodeAddr(iSegment+2);
	}

	if (ThePaths.GetBoundarySegmentsBetweenNodes(fromNode, toNode, nextNode, false, vLeftEnd, vRightEnd, vLeftStart, vRightStart))
	{
		//Flatten
		vLeftEnd.z = vRightEnd.z = vLeftStart.z = vRightStart.z = 0.0f;

		Vector3 vPosToLeft = vLeftStart - vPosFlat;
		Vector3 vPosToRight = vRightStart - vPosFlat;
		Vector3 vRightToLeft = vLeftStart - vRightStart;

		const float fDotLeft = Dot(vRightToLeft, vPosToLeft);
		const float fDotRight = Dot(vRightToLeft, vPosToRight);

		if (fDotRight * fDotLeft < 0.0f)
		{
			return true;
		}
	}

	return false;
}

#if __DEV
void CVehicleNodeList::CheckLinks() const
{
	// For all the nodes that aren't empty we expect a valid link.
	for (s32 t = 0; t < CAR_MAX_NUM_PATHNODES_STORED-2; t++)
	{
		if (!m_Entries[t].m_Address.IsEmpty() && !m_Entries[t+1].m_Address.IsEmpty())
		{
			if (!(m_Entries[t].m_iLink >= 0 && (int)m_Entries[t].m_iLink < MAXLINKSPERREGION))
			{
				Displayf("Target Node: %d", m_targetNodeIndex);
				for (s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
				{
					Displayf("aNodes[%d]=%d:%d aLinks[%d]=%d\n", i, m_Entries[i].m_Address.GetRegion(), m_Entries[i].m_Address.GetIndex(), i, m_Entries[i].m_iLink);
				}

				Assertf(0, "Link index %d [0-%u]", m_Entries[t].m_iLink, MAXLINKSPERREGION);
			}
			else if (ThePaths.apRegions[m_Entries[t].m_Address.GetRegion()] && ThePaths.apRegions[m_Entries[t+1].m_Address.GetRegion()] &&
				!(m_Entries[t].m_iLink >= 0 && m_Entries[t].m_iLink < ThePaths.apRegions[m_Entries[t].m_Address.GetRegion()]->NumLinks))
			{
				Assertf(0, "Link %d is less than MAXLINKSPERREGION, but greater than the actual NumLinks of this region, %d"
					, m_Entries[t].m_iLink, ThePaths.apRegions[m_Entries[t].m_Address.GetRegion()]->NumLinks);

				Assertf(m_Entries[t].m_iLink >= ThePaths.apRegions[m_Entries[t+1].m_Address.GetRegion()]->NumLinks
					, "...But it is within adjacent region %d's link count of %d. Maybe using the wrong region for this link?"
					, m_Entries[t+1].m_Address.GetRegion(), ThePaths.apRegions[m_Entries[t+1].m_Address.GetRegion()]->NumLinks);

				Displayf("Target Node: %d", m_targetNodeIndex);
				for (s32 i = 0; i < CAR_MAX_NUM_PATHNODES_STORED; i++)
				{
					Displayf("aNodes[%d]=%d:%d aLinks[%d]=%d\n", i, m_Entries[i].m_Address.GetRegion(), m_Entries[i].m_Address.GetIndex(), i, m_Entries[i].m_iLink);
				}
			}
		}
		if (!m_Entries[t].m_Address.IsEmpty())
		{
			Assert(m_Entries[t].m_iLaneIndex >= 0 && m_Entries[t].m_iLaneIndex < 10);
		}
	}
}

void CVehicleNodeList::VerifyNodesAreDistinct() const
{
	for (int i = 0; i < CAR_MAX_NUM_PATHNODES_STORED-1; i++)
	{
		if (m_Entries[i].m_Address.IsEmpty())
		{
			continue;
		}

		//otherwise, iterate through every other node and make sure we don't find it there
		for (int j = i+1; j < CAR_MAX_NUM_PATHNODES_STORED-1; j++)
		{
			Assertf(m_Entries[i].m_Address != m_Entries[j].m_Address, "Found a duplicate node in our nodelist!  This is Bad News.");
		}
	}
}
#endif

#if __BANK && DEBUG_DRAW
void CVehicleNodeList::DebugDraw(int iConsiderationStart, int iConsiderationEnd) const
{
	for(u32 i = 0; i<CAR_MAX_NUM_PATHNODES_STORED-1; i++)
	{
		const CPathNode* pNodeA = GetPathNode(i);
		const CPathNode* pNodeB = GetPathNode(i+1);
		if(!pNodeA || !pNodeB){ continue; }

		Vector3 vNodeAPos, vNodeBPos;
		pNodeA->GetCoors(vNodeAPos);
		pNodeB->GetCoors(vNodeBPos);
		Color32 lineColor = (i<iConsiderationStart) ? Color_yellow : ((i<iConsiderationEnd) ? Color_LimeGreen : Color_cyan);

		grcDebugDraw::Line(vNodeAPos,vNodeBPos,lineColor,-1);
		const Vector3 vOffset(0.0f,0.0f,0.10f);
		vNodeAPos += vOffset;
		vNodeBPos += vOffset;
		grcDebugDraw::Line(RCC_VEC3V(vNodeAPos),RCC_VEC3V(vNodeBPos),lineColor,-1);
		grcDebugDraw::Cross(RCC_VEC3V(vNodeAPos),0.25f,Color_grey70,-1);
	}
}
#endif
