#include "FollowNearestPathHelper.h"

#if ENABLE_HORSE

#include "peds/ped.h"

AI_OPTIMISATIONS()
// OPTIMISATIONS_OFF()

////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CFollowNearestPathHelper, CONFIGURED_FROM_FILE, "CFollowNearestPathHelper");

CFollowNearestPathHelper::CFollowNearestPathHelper()
: m_FollowNearestPathStatus(FNPS_Invalid)
, m_vGoalPosition(rage::VEC3_ZERO)
, m_GoalNode()
, m_Settings()
#if DEBUG_DRAW
, m_vInitialSouthPosition(Vector3::ZeroType)
, m_vInitialNorthPosition(Vector3::ZeroType)
, m_vProjectedPosition(Vector3::ZeroType)
, m_vTestPosition(Vector3::ZeroType)
, m_vClosestPointOnPath(Vector3::ZeroType)
, m_bCanDrawProjectedPositionTest(false)
#endif // DEBUG_DRAW
{
}

CFollowNearestPathHelper::~CFollowNearestPathHelper()
{
}

#if __BANK
void CFollowNearestPathHelper::RenderDebug(const CEntity* pEntity) const
{
	// Statics
	static bool sbSolidSphere = false;
	const float sfSphereRadius = 1.0f;
	const float sfArrowHeadSize = 0.75f;
	const int siFramesToLive = 1;
	const rage::Color32 InitialStartPosition = Color_AliceBlue;
	const rage::Color32 InitialEndPosition = Color_RosyBrown;
	const rage::Color32 ProjectedPosition = Color_gold;
	const rage::Color32 ClosestPosition = Color_red;
	const rage::Color32 MovingTowardsPosition = Color_green;

	// If we should render the initial path positions
	TUNE_GROUP_BOOL(FOLLOW_NEAREST_PATH_HELPER, bDrawInitialPathPositions, false);
	if (bDrawInitialPathPositions)
	{
		grcDebugDraw::Sphere(m_vInitialSouthPosition, sfSphereRadius, InitialStartPosition, sbSolidSphere, siFramesToLive);
		grcDebugDraw::Sphere(m_vInitialNorthPosition, sfSphereRadius, InitialEndPosition, sbSolidSphere, siFramesToLive);
	}

	TUNE_GROUP_BOOL(FOLLOW_NEAREST_PATH_HELPER, bDrawProjectedPositionTest, false);
	if (bDrawProjectedPositionTest && m_bCanDrawProjectedPositionTest)
	{
		// Projection outward from my heading
		grcDebugDraw::Sphere(m_vProjectedPosition, sfSphereRadius * 0.5f, ProjectedPosition, sbSolidSphere, siFramesToLive);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(m_vTestPosition), VECTOR3_TO_VEC3V(m_vProjectedPosition), sfArrowHeadSize, ProjectedPosition, siFramesToLive);

		// Closest point I should head towards
		grcDebugDraw::Sphere(m_vClosestPointOnPath, sfSphereRadius * 0.5f, ClosestPosition, sbSolidSphere, siFramesToLive);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(m_vTestPosition), VECTOR3_TO_VEC3V(m_vClosestPointOnPath), sfArrowHeadSize, ClosestPosition, siFramesToLive);

		// Line from projected point to closest point
		grcDebugDraw::Line(m_vProjectedPosition, m_vClosestPointOnPath, ProjectedPosition, ClosestPosition, siFramesToLive);
	}
	// Get coordinates that the helper is moving towards
	Vector3 vPointToMoveTowards(Vector3::ZeroType);

	// If we are moving towards a node
	if (!m_GoalNode.IsEmpty())
	{
		const CPathNode* pNode = ThePaths.FindNodePointerSafe(m_GoalNode);
		if (pNode)
		{
			pNode->GetCoors(vPointToMoveTowards);
			grcDebugDraw::Sphere(vPointToMoveTowards, sfSphereRadius * 1.5f, MovingTowardsPosition, sbSolidSphere, siFramesToLive);
	
			// If we have an entity to check against
			if (pEntity)
			{
				// Draw a line from the Ped towards our requested destination
				Vector3 vPedPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
				grcDebugDraw::Line(vPedPosition, vPointToMoveTowards, MovingTowardsPosition);
			}
		}
	}
}
#endif // __BANK

bool CFollowNearestPathHelper::IsCloseEnoughToPath(Vec3V_In vPathStartPosition, Vec3V_In vPathEndPosition, Vec3V_In vTestPosition, float fMaxDistSq)
{
	// Closest point I should head towards
	Vec3V vClosestPointOnPath = geomPoints::FindClosestPointSegToPoint(vPathStartPosition, (vPathEndPosition - vPathStartPosition), vTestPosition);

	// Get the dist sq from the test position
	vClosestPointOnPath -= vTestPosition;
	const float fDistSq = rage::MagXY(vClosestPointOnPath).Getf();

	// If we are close enough to the path
	return (fDistSq <= fMaxDistSq);
}

bool CFollowNearestPathHelper::IsEntityProjectionWithinForwardTolerance(Vec3V_In vPathStartPosition, Vec3V_In vPathEndPosition, Vec3V_In vEntityPosition, Vec3V_In vEntityHeading, Vec3V_In vEntityVelocity, float fForwardProjectionInSeconds)
{
	// Project position outward and get the closest points to determine if the entity
	// can head towards the nodes.
	Vec3V vProjectedPositionV = vEntityPosition + (vEntityVelocity * LoadScalar32IntoScalarV(fForwardProjectionInSeconds));
	if (MagSquared(vEntityVelocity).Getf() < 1.0f)
	{
		vProjectedPositionV = vEntityPosition + (vEntityHeading * LoadScalar32IntoScalarV(5.0f));
	}
	Vec3V vClosestPointOnPath = geomPoints::FindClosestPointSegToPoint(vPathStartPosition, (vPathEndPosition - vPathStartPosition), vProjectedPositionV);

#if DEBUG_DRAW
	m_vProjectedPosition = VEC3V_TO_VECTOR3(vProjectedPositionV);
	m_vTestPosition = VEC3V_TO_VECTOR3(vEntityPosition);
	m_vClosestPointOnPath = VEC3V_TO_VECTOR3(vClosestPointOnPath);
	m_bCanDrawProjectedPositionTest = true;
#endif // DEBUG_DRAW

	return (IsPosWithinForwardTolerance(vClosestPointOnPath, vEntityPosition, vEntityHeading, m_Settings.m_fMaxHeadingAdjustAllowed));
}

const CPathNode* CFollowNearestPathHelper::DetermineBestForwardNode(const CPathNode* pNode1, const CPathNode* pNode2, Vec3V_In vTestPosition, Vec3V_In vTestHeading)
{
	// Determine which nodes are viable
	bool bCanHeadTowardNode1 = IsNodePosWithinForwardTolerance(pNode1, vTestPosition, vTestHeading);
	bool bCanHeadTowardNode2 = IsNodePosWithinForwardTolerance(pNode2, vTestPosition, vTestHeading);

	// If both nodes are viable
	if (bCanHeadTowardNode1 && bCanHeadTowardNode2)
	{
		// roll for a decision
		if (fwRandom::GetRandomTrueFalse())
		{
			return pNode1;
		}
		else
		{
			return pNode2;
		}
	}
	else if (bCanHeadTowardNode1)
	{
		return pNode1;
	}
	else if (bCanHeadTowardNode2)
	{
		return pNode2;
	}
	// Couldn't head towards either node
	return NULL;
}

void CFollowNearestPathHelper::StartSearch(const HelperSettings& context)
{
	//////////////////////////////////////////////////////////////////////////
	// Default to no path
	m_GoalNode.SetEmpty();
	m_FollowNearestPathStatus = FNPS_NoRoute;

#if DEBUG_DRAW
	m_bCanDrawProjectedPositionTest = false;
#endif // DEBUG_DRAW

	//////////////////////////////////////////////////////////////////////////
	// Set the settings
	m_Settings = context;

	//////////////////////////////////////////////////////////////////////////
	// Set the search coordinates
	CNodeAddress SouthOrWestNodeAddress, NorthOrEastNodeAddress;
	float Orientation;
	const Vector3 SearchCoords = VEC3V_TO_VECTOR3(m_Settings.m_vPositionToTestFrom);

	//////////////////////////////////////////////////////////////////////////
	// Try to find the closest road segment to the coordinates
	ThePaths.FindNodePairClosestToCoors(
		SearchCoords, 
		&SouthOrWestNodeAddress, 
		&NorthOrEastNodeAddress, 
		&Orientation, 
		m_Settings.m_fMinDistanceBetweenNodes, 
		m_Settings.m_fCutoffDistForNodeSearch, 
		m_Settings.m_bIgnoreSwitchedOffNodes, 
		m_Settings.m_bUseWaterNodes, 
		m_Settings.m_bUseOnlyHighwayNodes, 
		m_Settings.m_bSearchUpFromPosition);

	//////////////////////////////////////////////////////////////////////////
	// If either node is invalid
	if (!IsNodeValid(SouthOrWestNodeAddress) || !IsNodeValid(NorthOrEastNodeAddress))
	{
		m_FollowNearestPathStatus = FNPS_NoRoute;
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// If either pointer is null
	const CPathNode* pSouthOrWestNode = ThePaths.FindNodePointerSafe(SouthOrWestNodeAddress);
	const CPathNode* pNorthOrEastNode = ThePaths.FindNodePointerSafe(NorthOrEastNodeAddress);
	if (!pSouthOrWestNode || !pNorthOrEastNode)
	{
		m_FollowNearestPathStatus = FNPS_NoRoute;
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// Get the node positions
	Vector3 vSouthPosition, vNorthPosition;
	pSouthOrWestNode->GetCoors(vSouthPosition);
	pNorthOrEastNode->GetCoors(vNorthPosition);

#if DEBUG_DRAW
	m_vInitialSouthPosition = vSouthPosition;
	m_vInitialNorthPosition = vNorthPosition;
#endif // DEBUG_DRAW

	//////////////////////////////////////////////////////////////////////////
	// Convert the path start/end position
	const Vec3V vStartPosition = VECTOR3_TO_VEC3V(vSouthPosition);
	const Vec3V vEndPosition = VECTOR3_TO_VEC3V(vNorthPosition);
	
	//////////////////////////////////////////////////////////////////////////
	// If we are too far away from the path
	if (!IsCloseEnoughToPath(vStartPosition, vEndPosition, m_Settings.m_vPositionToTestFrom, m_Settings.m_fMaxDistSqFromRoad))
	{
		m_FollowNearestPathStatus = FNPS_NoRoute;
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// Project position outward and get the closest points to determine if the entity
	// can head towards the nodes.
	if (!IsEntityProjectionWithinForwardTolerance(vStartPosition, vEndPosition, m_Settings.m_vPositionToTestFrom, m_Settings.m_vHeadingToTestWith, m_Settings.m_vVelocity, m_Settings.m_fForwardProjectionInSeconds))
	{
		m_FollowNearestPathStatus = FNPS_NoRoute;
		return;
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Determine which node to go towards
	const CPathNode* pNodeToMoveTowards = DetermineBestForwardNode(pSouthOrWestNode, pNorthOrEastNode, m_Settings.m_vPositionToTestFrom, m_Settings.m_vHeadingToTestWith);
	if (!pNodeToMoveTowards)
	{
		m_FollowNearestPathStatus = FNPS_NoRoute;
		return;
	}
	// Set the address
	m_GoalNode = pNodeToMoveTowards->m_address;

	//////////////////////////////////////////////////////////////////////////
	// Set the goal position to the node's position
	Vector3 vCoors;
	pNodeToMoveTowards->GetCoors(vCoors);
	m_vGoalPosition = VECTOR3_TO_VEC3V(vCoors);

	//////////////////////////////////////////////////////////////////////////
	// We now have a valid path
	m_FollowNearestPathStatus = FNPS_ValidRoute;
}

bool CFollowNearestPathHelper::ShouldMoveToNextNode(Vec3V_In vEntityPos, Vec3V_In vEntityHeading) const
{
	// If we are close enough to our target position
	const ScalarV DistSq = rage::DistSquared(m_vGoalPosition.GetXY(), vEntityPos.GetXY());
	if (DistSq.Getf() <= m_Settings.m_fDistSqBeforeMovingToNextNode)
	{
		// Request the next node
		return true;
	}

	// If we have moved past our target position
	Vec3V vHalfSpaceTest = m_vGoalPosition - vEntityPos;
	if (rage::Dot(vHalfSpaceTest, vEntityHeading).Getf() < -SMALL_FLOAT)
	{
		// Request the next node
		return true;
	}
	// Continue moving towards the current node
	return false;
}

void CFollowNearestPathHelper::FindNextNodeInForwardTolerance(const CPathNode* pNode, CNodeAddress& nodeAddr, Vec3V_In vEntityPos, Vec3V_In vEntityDir)
{
	// Make sure our node is valid
	if (aiVerify(pNode))
	{
		// Loop through the links
		const u32 uNumLinks = pNode->NumLinks();
		CNodeAddress nextNodeAddr;
		for (u32 uLink = 0; uLink < uNumLinks; ++uLink)
		{
			// Get the connected node
			nextNodeAddr = ThePaths.GetNodesLinkedNodeAddr(pNode, uLink);

			// If the next node can be used
			if (IsNodePosWithinForwardTolerance(nextNodeAddr, vEntityPos, vEntityDir))
			{
				break;
			}
			else // Reset the node address
			{
				nextNodeAddr.SetEmpty();
			}
		}
		// Set our next node address
		nodeAddr = nextNodeAddr;
	}
}

bool CFollowNearestPathHelper::IsNodeValid(const CNodeAddress& nodeAddr)
{
	// If the address is invalid
	if (nodeAddr.IsEmpty())
	{
		return false;
	}
	// If the region isn't loaded
	if (ThePaths.IsRegionLoaded(nodeAddr) == false)
	{
		return false;
	}
	// Node is valid
	return true;
}

bool CFollowNearestPathHelper::IsNodePosWithinForwardTolerance(const CNodeAddress& nodeAddr, Vec3V_In vEntityPos, Vec3V_In vEntityDir)
{
	// If the node is valid, and it is not currently referenced
	if (IsNodeValid(nodeAddr))
	{
		// Get the node
		CPathNode* pNode = ThePaths.FindNodePointerSafe(nodeAddr);
		// If the node is within heading constraints
		if (IsNodePosWithinForwardTolerance(pNode, vEntityPos, vEntityDir))
		{
			return true;
		}
	}
	// The node cannot be used
	return false;
}

void CFollowNearestPathHelper::RequestNextPathNode(Vec3V_In vEntityPos, Vec3V_In vEntityDir)
{
	// Flag for no path, will be updated if next node is valid
	m_FollowNearestPathStatus = FNPS_NoRoute;

	// Get the next node address
	CNodeAddress nextNodeAddress;
	const CPathNode* pCurrentNode = ThePaths.FindNodePointerSafe(m_GoalNode);
	FindNextNodeInForwardTolerance(pCurrentNode, nextNodeAddress, vEntityPos, vEntityDir);

	// If the next node along the path exists and is valid
	pCurrentNode = ThePaths.FindNodePointerSafe(nextNodeAddress);
	if (pCurrentNode)
	{
		// Update the position we are moving towards
		Vector3 vCoors;
		pCurrentNode->GetCoors(vCoors);
		m_vGoalPosition = VECTOR3_TO_VEC3V(vCoors);

		// Update our current node address
		m_GoalNode = nextNodeAddress;

		// Set that the path is valid
		m_FollowNearestPathStatus = FNPS_ValidRoute;
	}
}

bool CFollowNearestPathHelper::IsPosWithinForwardTolerance(Vec3V_In vQueryPos, Vec3V_In vEntityPos, Vec3V_In vEntityDir, float fHeadingTolerance)
{
	Vec3V vEntityToPos = vQueryPos - vEntityPos;
	float fHeadingToPos = rage::Atan2f(-vEntityToPos.GetXf(), vEntityToPos.GetYf());
	float fEntityForwardHeading = rage::Atan2f(-vEntityDir.GetXf(), vEntityDir.GetYf());
	float fHeadingDiff = fEntityForwardHeading - fHeadingToPos;
	
	return (rage::Abs(fHeadingDiff) <= fHeadingTolerance);
}

bool CFollowNearestPathHelper::IsNodePosWithinForwardTolerance(const CPathNode* pNode, Vec3V_In vEntityPos, Vec3V_In vEntityDir)
{
	// If we have valid info
	if (taskVerifyf(pNode, "CFollowNearestPathHelper::IsNodePosWithinForwardTolerance - Invalid node"))
	{
		// Get the node position
		Vector3 vNodePosition;
		pNode->GetCoors(vNodePosition);
		return IsPosWithinForwardTolerance(VECTOR3_TO_VEC3V(vNodePosition), vEntityPos, vEntityDir, m_Settings.m_fMaxHeadingAdjustAllowed);
	}
	return false;
}

void CFollowNearestPathHelper::Process(const CEntity* pEntity)
{
	// Needs an entity
	Assert(pEntity);
	if (!pEntity)
	{
		return;
	}

	// If the path is valid
	if (m_FollowNearestPathStatus == FNPS_ValidRoute)
	{
		// If we should move to the next node
		Vec3V vPosition = pEntity->GetTransform().GetPosition();
		Vec3V vHeading = pEntity->GetTransform().GetForward();
		if (ShouldMoveToNextNode(vPosition, vHeading))
		{
			// Request the next node
			RequestNextPathNode(vPosition, vHeading);
		}
	}
}

#endif // ENABLE_HORSE