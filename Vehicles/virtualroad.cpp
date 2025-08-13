/////////////////////////////////////////////////////////////////////////////////
// FILE :		virtualroad.h
// PURPOSE :	Deal with a made up road surface when vehs are in ghost state
// AUTHOR :		Obbe.
//				Adam Croston
// CREATED :	22-1-07
/////////////////////////////////////////////////////////////////////////////////

//Rage headers
#include "Bank/BkMgr.h"
#include "GrCore/DebugDraw.h"

// Game headers
#include "camera/camInterface.h"
#include "peds/PedIntelligence.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/WorldProbe/WorldProbe.h"
#include "VehicleAi/PathFind.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "VehicleAi/Task/TaskVehicleMissionBase.h"
#include "Vehicles/VirtualRoad.h"
#include "Vehicles/VehiclePopulation.h"
#include "scene/LoadScene.h"
#include "Scene/world/GameWorld.h"
#include "Physics/WorldProbe/shapetestresults.h"

AI_OPTIMISATIONS();
AI_VEHICLE_OPTIMISATIONS();
VEHICLE_OPTIMISATIONS();

// PURPOSE:	Enable optimization to get the virtual junction from the node address, rather than by looking up its position.
#define OPT_GET_VIRTUAL_JUNCTION_FROM_NODE_ADDR 1

/////////////////////////////////////////////////////////////////////////////////
// Statics

bool CVirtualRoad::ms_bEnableVirtualJunctionHeightmaps = true;
bool CVirtualRoad::ms_bEnableConstraintSlackWhenChangingPaths = true;

/////////////////////////////////////////////////////////////////////////////////
// CVirtualRoad::TRouteInfo

#if __ASSERT
bool CVirtualRoad::TRouteInfo::Validate() const
{
	bool bSuccess = true;
	const ScalarV fSmallFloat(V_FLT_SMALL_6);
	bSuccess = bSuccess && aiVerify(IsFiniteAll(m_vNodeDir[0]));
	bSuccess = bSuccess && aiVerify(IsGreaterThanAll(Mag(m_vNodeDir[0]),fSmallFloat));
	bSuccess = bSuccess && aiVerify(!m_bHasTwoSegments || IsFiniteAll(m_vNodeDir[1]));
	bSuccess = bSuccess && aiVerify(!m_bHasTwoSegments || IsGreaterThanAll(Mag(m_vNodeDir[1]),fSmallFloat));
	bSuccess = bSuccess && aiVerify(IsFiniteAll(m_vCambers));

	if(m_bCalcWidthConstraint || m_iCamberFalloffs[0] || m_iCamberFalloffs[1])
	{
		bSuccess = bSuccess && aiVerify(IsFiniteAll(m_vWidths.GetX()) && IsGreaterThanAll(m_vWidths.GetX(),ScalarV(V_ZERO)));
		bSuccess = bSuccess && aiVerify(IsFiniteAll(m_vWidths.GetY()) && IsGreaterThanAll(m_vWidths.GetY(),ScalarV(V_ZERO)));
	}

	if(m_bHasTwoSegments)
	{
		if(m_bCalcWidthConstraint || m_iCamberFalloffs[2] || m_iCamberFalloffs[3])
		{
			bSuccess = bSuccess && aiVerify(IsFiniteAll(m_vWidths.GetZ()) && IsGreaterThanAll(m_vWidths.GetZ(),ScalarV(V_ZERO)));
			bSuccess = bSuccess && aiVerify(IsFiniteAll(m_vWidths.GetW()) && IsGreaterThanAll(m_vWidths.GetW(),ScalarV(V_ZERO)));
		}
	}
	else
	{
		bSuccess = bSuccess && aiVerify((m_vCambers.GetX()==m_vCambers.GetZ()).Getb() && (m_vCambers.GetY()==m_vCambers.GetW()).Getb());
	}

	bSuccess = !m_bConsiderJunction || !IsEqualAll(GetJunctionPos(),Vec3V(V_ZERO));

	return bSuccess;
}
#endif

/////////////////////////////////////////////////////////////////////////////////
bool CVirtualRoad::HelperGetLongestJunctionEdge(const CNodeAddress& nodeAddress, const Vector3& vPosIn, float& fLengthOut)
{
	if (nodeAddress.IsEmpty() || !ThePaths.IsRegionLoaded(nodeAddress))
	{
		return false;
	}

	//check if this node is a part of a virtual junction
	const int* virtualJunctionIndexPtr = ThePaths.apRegions[nodeAddress.GetRegion()]->JunctionMap.JunctionMap.SafeGet(nodeAddress.RegionAndIndex());
	if(!virtualJunctionIndexPtr)
	{
		return false;
	}

	//now get the virtual junction data
	const int iVirtualJunctionIndex = *virtualJunctionIndexPtr;
	Assert(iVirtualJunctionIndex >= 0);
	Assert(iVirtualJunctionIndex < ThePaths.apRegions[nodeAddress.GetRegion()]->NumJunctions);
	CPathVirtualJunction* pVirtJunction = &ThePaths.apRegions[nodeAddress.GetRegion()]->aVirtualJunctions[iVirtualJunctionIndex];
	Assert(pVirtJunction);
	CVirtualJunctionInterface jnInterface(pVirtJunction, ThePaths.apRegions[nodeAddress.GetRegion()]);

	fLengthOut = jnInterface.GetLongestEdge().Getf();

#if HACK_GTA4
	//limit the size of particular junctions
	static dev_float s_fOverrideJunctionLength = 10.0f;

	if (vPosIn.Dist2(Vector3(1048.8f, -2261.3f, 29.6f)) < 1.0f * 1.0f
		|| vPosIn.Dist2(Vector3(1054.3f, -2201.3f, 29.8f)) < 1.0f * 1.0f)
	{
		fLengthOut = rage::Min(fLengthOut, s_fOverrideJunctionLength);	
	}
#endif //HACK_GTA4

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// CVirtualRoad

void CVirtualRoad::HelperCreateRouteInfoFromRoutePoints(const CRoutePoint* pRoutePoints, int iNumPoints, const CRoutePoint* pOldPoint, const CRoutePoint* pFuturePoint, bool bCalcConstraint, bool bForceIncludeWidths, TRouteInfo & rRouteInfoOut, const bool bConsiderJunctions)
{
	// Set up TRouteInfo struct which provides the virtual surface to drive along from 2 or 3 points from pRoutePoints.
	Assert(iNumPoints==2 || iNumPoints==3);

	// Constraint?
	rRouteInfoOut.m_bCalcWidthConstraint = bCalcConstraint;

	// Position, camber, falloff ...
	const Vec3V vPos0 = pRoutePoints[0].m_vPositionAndSpeed.GetXYZ();
	const Vec3V vPos1 = pRoutePoints[1].m_vPositionAndSpeed.GetXYZ();
	const float fCamberThisSide0 = -saTiltValues[pRoutePoints[0].m_iCamberThisSide];
	const float fCamberOtherSide0 = saTiltValues[pRoutePoints[0].m_iCamberOtherSide];
	const u8 iCamberFalloffThisSide0 = pRoutePoints[0].m_iCamberFalloffThisSide;
	const u8 iCamberFalloffOtherSide0 = pRoutePoints[0].m_iCamberFalloffOtherSide;
	rRouteInfoOut.SetSegment0(vPos0, vPos1, fCamberThisSide0, fCamberOtherSide0,iCamberFalloffThisSide0,iCamberFalloffOtherSide0);

	bool bUsingJunctionBufferWidth = false;

	// ... and width, which is required if constraint is being calculated or if there is a falloff
	bool bWidthRequired0 = (bForceIncludeWidths || bCalcConstraint || iCamberFalloffThisSide0 || iCamberFalloffOtherSide0);
	if(bWidthRequired0)
	{
		float fLeftDist, fRightDist, fBufferDist;
		pRoutePoints[0].FindRoadBoundaries(fLeftDist, fRightDist);
		//fBufferDist = (pRoutePoints[0].m_bJunctionNode || pRoutePoints[0].m_bIgnoreForNavigation) ? CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction : CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
		if (pRoutePoints[0].m_bJunctionNode)
		{
			float fLength = 0.0f;
			if (HelperGetLongestJunctionEdge(pRoutePoints[0].GetNodeAddress(), VEC3V_TO_VECTOR3(pRoutePoints[0].GetPosition()), fLength))
			{
				fBufferDist = rage::Max(3.0f, fLength * CVehicleAILodManager::ms_fPercentOfJunctionLengthToAddToConstraint);
			}
			else
			{
				fBufferDist = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction;
			}

			bUsingJunctionBufferWidth = true;
		}
		else if (pRoutePoints[1].m_bJunctionNode)
		{
			float fLength = 0.0f;
			if (HelperGetLongestJunctionEdge(pRoutePoints[1].GetNodeAddress(), VEC3V_TO_VECTOR3(pRoutePoints[1].GetPosition()), fLength))
			{
				fBufferDist = rage::Max(3.0f, fLength * CVehicleAILodManager::ms_fPercentOfJunctionLengthToAddToConstraint);
			}
			else
			{
				fBufferDist = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistancePreJunction;
			}

			bUsingJunctionBufferWidth = true;
		}
		else if (pRoutePoints[0].m_bIgnoreForNavigation)
		{
			fBufferDist = pRoutePoints[0].m_fLaneWidth * pRoutePoints[0].NumLanesToUseForPathFollowing();
		}
		else
		{
			fBufferDist = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
		}
		rRouteInfoOut.SetSegment0WidthsMod(fRightDist,fLeftDist,fBufferDist);
	}

	rRouteInfoOut.SetNoJunction();
	if(bConsiderJunctions)
	{
		if(pRoutePoints[0].m_bJunctionNode)
		{
			rRouteInfoOut.SetJunction(pRoutePoints[0].m_vPositionAndSpeed.GetXYZ(), pRoutePoints[0].GetNodeAddress());
		}
		else if(pRoutePoints[1].m_bJunctionNode)
		{
			rRouteInfoOut.SetJunction(pRoutePoints[1].m_vPositionAndSpeed.GetXYZ(), pRoutePoints[1].GetNodeAddress());
		}
		else if(iNumPoints>2 && pRoutePoints[2].m_bJunctionNode)
		{
			rRouteInfoOut.SetJunction(pRoutePoints[2].m_vPositionAndSpeed.GetXYZ(), pRoutePoints[2].GetNodeAddress());
		}
	}

	if(iNumPoints<=2)
	{
		rRouteInfoOut.SetNoSegment1();
	}
	else
	{
		// Repeat for segment 1
		const Vec3V vPos2 = pRoutePoints[2].m_vPositionAndSpeed.GetXYZ();
		const float fCamberThisSide1 = -saTiltValues[pRoutePoints[1].m_iCamberThisSide];
		const float fCamberOtherSide1 = saTiltValues[pRoutePoints[1].m_iCamberOtherSide];
		const u8 iCamberFalloffThisSide1 = pRoutePoints[1].m_iCamberFalloffThisSide;
		const u8 iCamberFalloffOtherSide1 = pRoutePoints[1].m_iCamberFalloffOtherSide;
		rRouteInfoOut.SetSegment1(vPos1, vPos2, fCamberThisSide1, fCamberOtherSide1, iCamberFalloffThisSide1, iCamberFalloffOtherSide1);
		bool bWidthRequired1 = (bForceIncludeWidths || bCalcConstraint || (iCamberFalloffThisSide1!=0) || (iCamberFalloffOtherSide1!=0));
		if(bWidthRequired1)
		{
			float fLeftDist, fRightDist, fBufferDist;
			pRoutePoints[1].FindRoadBoundaries(fLeftDist, fRightDist);
			//fBufferDist = (pRoutePoints[1].m_bJunctionNode || pRoutePoints[1].m_bIgnoreForNavigation) ? CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction : CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
			if (pRoutePoints[1].m_bJunctionNode)
			{
				float fLength = 0.0f;
				if (HelperGetLongestJunctionEdge(pRoutePoints[1].GetNodeAddress(), VEC3V_TO_VECTOR3(pRoutePoints[1].GetPosition()), fLength))
				{
					fBufferDist = rage::Max(3.0f, fLength * CVehicleAILodManager::ms_fPercentOfJunctionLengthToAddToConstraint);
				}
				else
				{
					fBufferDist = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction;
				}

				bUsingJunctionBufferWidth = true;
			}
			else if (pRoutePoints[2].m_bJunctionNode)
			{
				float fLength = 0.0f;
				if (HelperGetLongestJunctionEdge(pRoutePoints[2].GetNodeAddress(), VEC3V_TO_VECTOR3(pRoutePoints[2].GetPosition()), fLength))
				{
					fBufferDist = rage::Max(3.0f, fLength * CVehicleAILodManager::ms_fPercentOfJunctionLengthToAddToConstraint);
				}
				else
				{
					fBufferDist = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistancePreJunction;
				}

				bUsingJunctionBufferWidth = true;
			}
			else if (pRoutePoints[1].m_bIgnoreForNavigation)
			{
				fBufferDist = pRoutePoints[1].m_fLaneWidth * pRoutePoints[1].NumLanesToUseForPathFollowing();
			}
			else
			{
				fBufferDist = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
			}
			rRouteInfoOut.SetSegment1WidthsMod(fRightDist,fLeftDist,fBufferDist);
		}
	}

	if(bUsingJunctionBufferWidth)
	{
		static const ScalarV fCosTenDegrees = ScalarVConstant<FLOAT_TO_INT(0.985f)>();

		bool bDisableJunctionBuffering = iNumPoints == 3 
										&& pOldPoint
										&& pFuturePoint
										&& !pOldPoint->m_bIgnoreForNavigation 
										&& !pRoutePoints[0].m_bIgnoreForNavigation 
										&& !pRoutePoints[1].m_bIgnoreForNavigation 
										&& !pRoutePoints[2].m_bIgnoreForNavigation 
										&& !pFuturePoint->m_bIgnoreForNavigation 
										&& IsTrue(Dot(rRouteInfoOut.m_vNodeDirNormalised[0], rRouteInfoOut.m_vNodeDirNormalised[1]) > fCosTenDegrees);

		if(bDisableJunctionBuffering)
		{
			const Vec3V vOldPos = pOldPoint->m_vPositionAndSpeed.GetXYZ();
			const Vec3V vFuturePos = pFuturePoint->m_vPositionAndSpeed.GetXYZ();

			const Vec3V vOldDir = NormalizeSafe(pRoutePoints[0].m_vPositionAndSpeed.GetXYZ() - vOldPos, rRouteInfoOut.m_vNodeDirNormalised[0]);
			const Vec3V vFutureDir = NormalizeSafe(vFuturePos - pRoutePoints[2].m_vPositionAndSpeed.GetXYZ(), rRouteInfoOut.m_vNodeDirNormalised[1]);

			bDisableJunctionBuffering = IsTrue(Dot(vOldDir, rRouteInfoOut.m_vNodeDirNormalised[0]) > fCosTenDegrees) 
										&& IsTrue(Dot(rRouteInfoOut.m_vNodeDirNormalised[1], vFutureDir) > fCosTenDegrees);

			if(bDisableJunctionBuffering)
			{
				rRouteInfoOut.SetSegment0BufferWidth(CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance);
				rRouteInfoOut.SetSegment1BufferWidth(CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance);

#if DEBUG_DRAW && __BANK
				if(CVehicleAILodManager::ms_bDisplayDummyPathConstraint)
				{
					const Vec3V vOffset = Vec3V(0.0f, 0.0f, 0.5f);

					grcDebugDraw::Sphere(vOldPos + vOffset, 0.6f, Color_black, false, -1);
					grcDebugDraw::Sphere(pRoutePoints[0].GetPosition() + vOffset, 0.6f, Color_red, false, -1);
					grcDebugDraw::Sphere(pRoutePoints[1].GetPosition() + vOffset, 0.6f, Color_green, false, -1);
					grcDebugDraw::Sphere(pRoutePoints[2].GetPosition() + vOffset, 0.6f, Color_cyan, false, -1);
					grcDebugDraw::Sphere(vFuturePos + vOffset, 0.6f, Color_white, false, -1);
				}
#endif // DEBUG_DRAW && __BANK
			}
		}
	}
}

void CVirtualRoad::HelperCreateRouteInfoFromFollowRouteAndCenterNode(const CVehicleFollowRouteHelper & rFollowRoute, int iKnownCenterNode, bool bCalcConstraint, bool bForceIncludeWidths, TRouteInfo & rRouteInfoOut, const bool bConsiderJunctions)
{
	int iNumPoints = ((iKnownCenterNode > 0) && (iKnownCenterNode < rFollowRoute.GetNumPoints()-1)) ? 3 : 2;
	int iStartNode = Max(0,iKnownCenterNode-1);

	const CRoutePoint* pRoutePoints = rFollowRoute.GetRoutePoints();
	const CRoutePoint* pOldPoint = iStartNode > 0 ? pRoutePoints + (iStartNode - 1) : NULL;
	const CRoutePoint* pFuturePoint = iStartNode + iNumPoints < rFollowRoute.GetNumPoints() ? pRoutePoints + (iStartNode + iNumPoints) : NULL;

	HelperCreateRouteInfoFromRoutePoints(pRoutePoints+iStartNode,iNumPoints,pOldPoint,pFuturePoint,bCalcConstraint,bForceIncludeWidths,rRouteInfoOut, bConsiderJunctions);
#if __ASSERT
	static bool bEverFailed = false;
	if(!rRouteInfoOut.Validate() && !bEverFailed)
	{
		Displayf("Validate failed in HelperCreateRouteInfoFromFollowRouteAndCenterNode.  iStartNode = %d, iNumPoints = %d.  rFollowRoute.GetNumPoints() = %d", iStartNode, iNumPoints, rFollowRoute.GetNumPoints());
		for(int i=0; pRoutePoints && i<rFollowRoute.GetNumPoints(); i++)
		{
			Displayf("  - point %d (%0.2f,%0.2f,%0.2f)",i,pRoutePoints[i].m_vPositionAndSpeed.GetXf(),pRoutePoints[i].m_vPositionAndSpeed.GetYf(), pRoutePoints[i].m_vPositionAndSpeed.GetZf());
		}
		bEverFailed = true;
	}
#endif
}

bool CVirtualRoad::HelperCreateRouteInfoFromFollowRouteAndCenterPos(const CVehicleFollowRouteHelper & rFollowRoute, Vec3V_In vCenterPos, TRouteInfo & rRouteInfoOut, const bool bConsiderJunctions, const bool bForceIncludeWidths)
{
	const ScalarV fHalf(V_HALF);
	int iClosestSeg;
	ScalarV fTAlongClosestV;
	Vec3V vClosestPoint;
	bool bBeforeStart, bAfterEnd;
	bool bClosestSegFound = HelperFindClosestSegmentAlongFollowRoute(vCenterPos,rFollowRoute,iClosestSeg,fTAlongClosestV,vClosestPoint,bBeforeStart,bAfterEnd);
	if(!bClosestSegFound)
	{
		return false;
	}
	int iCenterNode = IsGreaterThanAll(fTAlongClosestV,fHalf) ? iClosestSeg+1 : iClosestSeg;
	int iNumPoints = ((iCenterNode > 0) && (iCenterNode < rFollowRoute.GetNumPoints()-1)) ? 3 : 2;
	int iStartNode = Max(0,iCenterNode-1);

	const CRoutePoint* pRoutePoints = rFollowRoute.GetRoutePoints();
	const CRoutePoint* pOldPoint = iStartNode > 0 ? pRoutePoints + (iStartNode - 1) : NULL;
	const CRoutePoint* pFuturePoint = iStartNode + iNumPoints < rFollowRoute.GetNumPoints() ? pRoutePoints + (iStartNode + iNumPoints) : NULL;

	HelperCreateRouteInfoFromRoutePoints(pRoutePoints+iStartNode,iNumPoints,pOldPoint,pFuturePoint,false,bForceIncludeWidths,rRouteInfoOut,bConsiderJunctions);

	return true;
}

bool CVirtualRoad::HelperCreateRouteInfoFromNodeListAndCenterNode(const CVehicleNodeList & rNodeList, int iCenterNode, TRouteInfo & rRouteInfoOut)
{
	Assert(iCenterNode>=0 && iCenterNode<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED);
	int nNumNodesInRouteInfo = 3;
	const CPathNode * pPathNodes[3] =
	{
		(iCenterNode-1>=0) ? rNodeList.GetPathNode(iCenterNode-1) : NULL,
		rNodeList.GetPathNode(iCenterNode),
		(iCenterNode+1<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED) ? rNodeList.GetPathNode(iCenterNode+1) : NULL
	};
	// Might be missing the first node...
	if(!pPathNodes[0])
	{
		pPathNodes[0] = pPathNodes[1];
		pPathNodes[1] = pPathNodes[2];
		pPathNodes[2] = NULL;
		nNumNodesInRouteInfo = 2;
	}
	// But we need at least two...
	if(!pPathNodes[0] || !pPathNodes[1])
	{
		return false;
	}

	// Fill in the TRouteInfo
	Vec3V vPos0, vPos1;
	pPathNodes[0]->GetCoors(RC_VECTOR3(vPos0));
	pPathNodes[1]->GetCoors(RC_VECTOR3(vPos1));

	// Forward and backward link for camber and falloff
	const CPathNodeLink * pLinkForward0 = ThePaths.FindLinkBetween2Nodes(pPathNodes[0],pPathNodes[1]);
	const CPathNodeLink * pLinkBackward0 = ThePaths.FindLinkBetween2Nodes(pPathNodes[1],pPathNodes[0]);
	float fCamberThisSide0 = 0.0f, fCamberOtherSide0 = 0.0f;
	u8 iCamberFalloffThisSide0 = 0, iCamberFalloffOtherSide0 = 0;
	if(pLinkForward0)
	{
		fCamberThisSide0 = -saTiltValues[pLinkForward0->m_1.m_Tilt];
		iCamberFalloffThisSide0 = pLinkForward0->m_1.m_TiltFalloff;
	}
	if(pLinkBackward0)
	{
		fCamberOtherSide0 = saTiltValues[pLinkBackward0->m_1.m_Tilt];
		iCamberFalloffOtherSide0 = pLinkBackward0->m_1.m_TiltFalloff;
	}

	// Fill in rRouteInfoOut
	rRouteInfoOut.m_bCalcWidthConstraint = false;

	// Segment 0
	{
		rRouteInfoOut.SetSegment0(vPos0, vPos1, fCamberThisSide0, fCamberOtherSide0, iCamberFalloffThisSide0, iCamberFalloffOtherSide0);
		float fLeftWidth0 = 0.0f, fRightWidth0 = 0.0f;
		if(iCamberFalloffThisSide0 || iCamberFalloffOtherSide0)
		{
			Assert(pLinkForward0 || pLinkBackward0);
			if(pLinkForward0)
			{
				float fForwardLeftWidth0, fForwardRightWidth0;
				ThePaths.FindRoadBoundaries(*pLinkForward0,fForwardLeftWidth0,fForwardRightWidth0);
				fLeftWidth0 = fForwardLeftWidth0;
				fRightWidth0 = fForwardRightWidth0;
			}
			else if(pLinkBackward0)
			{
				float fBackwardLeftWidth0, fBackwardRightWidth0;
				ThePaths.FindRoadBoundaries(*pLinkBackward0,fBackwardLeftWidth0,fBackwardRightWidth0);
				fLeftWidth0 = fBackwardRightWidth0;
				fRightWidth0 = fBackwardLeftWidth0;
			}
			// Does width forward left == backward right always? // TC
			rRouteInfoOut.SetSegment0WidthsMod(fLeftWidth0,fRightWidth0,0.0f); // No buffer since that is only used for constraints
		}
	}

	// Segment 1 (conditionally)
	if(!pPathNodes[2])
	{
		rRouteInfoOut.SetNoSegment1();
	}
	else
	{
		Vec3V vPos2;
		pPathNodes[2]->GetCoors(RC_VECTOR3(vPos2));

		// Forward and backward link
		const CPathNodeLink * pLinkForward1 = ThePaths.FindLinkBetween2Nodes(pPathNodes[1],pPathNodes[2]);
		const CPathNodeLink * pLinkBackward1 = ThePaths.FindLinkBetween2Nodes(pPathNodes[2],pPathNodes[1]);
		float fCamberThisSide1 = 0.0f, fCamberOtherSide1 = 0.0f;
		u8 iCamberFalloffThisSide1 = 0, iCamberFalloffOtherSide1 = 0;
		if(pLinkForward1)
		{
			fCamberThisSide1 = -saTiltValues[pLinkForward1->m_1.m_Tilt];
			iCamberFalloffThisSide1 = pLinkForward1->m_1.m_TiltFalloff;
		}
		if(pLinkBackward1)
		{
			fCamberOtherSide1 = saTiltValues[pLinkBackward1->m_1.m_Tilt];
			iCamberFalloffOtherSide1 = pLinkBackward1->m_1.m_TiltFalloff;
		}

		rRouteInfoOut.SetSegment1(vPos1, vPos2, fCamberThisSide1, fCamberOtherSide1, iCamberFalloffThisSide1, iCamberFalloffOtherSide1);
		float fLeftWidth1 = 0.0f, fRightWidth1 = 0.0f;
		if(iCamberFalloffThisSide1 || iCamberFalloffOtherSide1)
		{
			Assert(pLinkForward1 || pLinkBackward1);
			if(pLinkForward1)
			{
				float fForwardLeftWidth1, fForwardRightWidth1;
				ThePaths.FindRoadBoundaries(*pLinkForward1,fForwardLeftWidth1,fForwardRightWidth1);
				fLeftWidth1 = fForwardLeftWidth1;
				fRightWidth1 = fForwardRightWidth1;
			}
			else if(pLinkBackward1)
			{
				float fBackwardLeftWidth1, fBackwardRightWidth1;
				ThePaths.FindRoadBoundaries(*pLinkBackward1,fBackwardLeftWidth1,fBackwardRightWidth1);
				fLeftWidth1 = fBackwardRightWidth1;
				fRightWidth1 = fBackwardLeftWidth1;
			}
			// Does width forward left == backward right always?
			rRouteInfoOut.SetSegment1WidthsMod(fLeftWidth1,fRightWidth1,0.0f);
		}
	}

	rRouteInfoOut.SetNoJunction();

	return true;
}

bool CVirtualRoad::HelperCreateRouteInfoFromNodeListAndCenterPos(const CVehicleNodeList & rNodeList, Vec3V_In vCenterPos, TRouteInfo & rRouteInfoOut)
{
	int nNumNodes = rNodeList.FindNumPathNodes();
	Vec3V vClosestPos;
	int iClosestSeg;
	float fClosestSegT;
	if(!rNodeList.GetClosestPosOnPath(RCC_VECTOR3(vCenterPos),RC_VECTOR3(vClosestPos),iClosestSeg,fClosestSegT,0,nNumNodes-1))
	{
		return false;
	}
	return HelperCreateRouteInfoFromNodeListAndCenterNode(rNodeList,iClosestSeg,rRouteInfoOut);
}

bool CVirtualRoad::HelperFindClosestSegmentAlongFollowRoute(Vec3V_In vTestPos, const CVehicleFollowRouteHelper & rFollowRoute, int & iSeg_Out, ScalarV_InOut fTAlongSeg_Out, Vec3V_InOut vClosestOnSeg_Out, bool & bBeforeStart_Out, bool & bAfterEnd_Out)
{
	static const ScalarV fZero(V_ZERO);
	static const ScalarV fOne(V_ONE);
	static const ScalarV fExitLinkThreshold(V_THIRD);
	static const ScalarV fEntryLinkThreshold(0.01f);

	bool bValidLinkFound = false;
	ScalarV fDistToClosest2(FLT_MAX);
	ScalarV fAltDistToClosest2(FLT_MAX);

	// Find the closest segment.
	// Should we search to the target or search to the end?
	const CRoutePoint * pRoutePoints = rFollowRoute.GetRoutePoints();
	const int kNodesBeforeTarget = 4;
	int iStartSeg = rage::Max(0, rFollowRoute.GetCurrentTargetPoint() - kNodesBeforeTarget);
	int iEndSeg = rFollowRoute.GetNumPoints() - 2;
	for(int iSeg = iStartSeg; iSeg <= iEndSeg; iSeg++)
	{
		const CRoutePoint & rPoint1 = pRoutePoints[iSeg];
		const CRoutePoint & rPoint2 = pRoutePoints[iSeg+1];
		Vec3V vNode1Pos = rPoint1.GetPosition();
		Vec3V vNode2Pos = rPoint2.GetPosition();

#if DEBUG_DRAW && __BANK
		bool bIsFocusOrNoFocus = CDebugScene::FocusEntities_IsEmpty() || CDebugScene::FocusEntities_IsNearGroup(vTestPos);
		if((CVehicleAILodManager::ms_bDisplayDummyPath || CVehicleAILodManager::ms_bDisplayDummyPathDetailed) && bIsFocusOrNoFocus)
		{
			Color32 lineColor = (iSeg<=rFollowRoute.GetCurrentTargetPoint()) ? Color_pink : Color_purple;
			grcDebugDraw::Line(vNode1Pos,vNode2Pos,lineColor,-1);
			grcDebugDraw::Cross(vNode1Pos,0.25f,Color_grey70,-1);
		}
#endif

		const Vec3V	vSegDiff = vNode2Pos - vNode1Pos;
		const Vec3V	vToPos = vTestPos - vNode1Pos;
		const ScalarV fSegmentLength2 = MagSquared(vSegDiff);
		const ScalarV fTAlongSeg = Clamp(Dot(vToPos,vSegDiff)/fSegmentLength2,fZero,fOne);
		const Vec3V vClosestOnSeg = vNode1Pos + vSegDiff * fTAlongSeg;
		const ScalarV fDist2 = DistSquared(vClosestOnSeg,vTestPos);

		if(IsLessThanAll(fDist2, fDistToClosest2)
			&& IsLessThanAll(fDist2, fAltDistToClosest2))
		{
			bValidLinkFound = true;
			fDistToClosest2 = fDist2;

			// Update return variables
			iSeg_Out = iSeg;
			fTAlongSeg_Out = fTAlongSeg;
			vClosestOnSeg_Out = vClosestOnSeg;
		}
		else if (bValidLinkFound && iSeg_Out == iSeg-1 
			&& rPoint1.m_bJunctionNode
			&& iSeg > 0 && !pRoutePoints[iSeg-1].m_bJunctionNode
			&& IsGreaterThanAll(fTAlongSeg_Out, fEntryLinkThreshold)
			&& (IsGreaterThanAll(fTAlongSeg,fTAlongSeg_Out) || IsGreaterThanAll(fTAlongSeg, fExitLinkThreshold))		
			)
		{
			//if this is a junction, and we think the previous point is closer,
			//but our T-value is further along this link than the previous one,
			//make the junction node our center point because we're probably
			//cutting the corner here

			//copy-pasted from the first if-block
			bValidLinkFound = true;

			//save off the distance to the actual closest link so far,
			//so if the distance to the junction is further than this,
			//a later link on the path can't steal the closest segment honors
			//by being closer than the junction's link
			fAltDistToClosest2 = fDistToClosest2;

			fDistToClosest2 = fDist2;

			// Update return variables
			iSeg_Out = iSeg;
			fTAlongSeg_Out = fTAlongSeg;
			vClosestOnSeg_Out = vClosestOnSeg;
		}
	}

	bBeforeStart_Out = false;
	bAfterEnd_Out = false;

	if(bValidLinkFound)
	{
		if(iSeg_Out==iStartSeg && (fTAlongSeg_Out==fZero).Getb())
		{
			bBeforeStart_Out = true;
		}
		else if (iSeg_Out==iEndSeg && (fTAlongSeg_Out==fOne).Getb())
		{
			bAfterEnd_Out = true;
		}
	}

	return bValidLinkFound;
}


/////////////////////////////////////////////////////////////////////////////////
// CVirtualRoad::TestProbesToVirtualRoad* functions

void CVirtualRoad::TestProbesToVirtualRoadFollowRouteAndCenterPos(const CVehicleFollowRouteHelper& rFollowRoute, Vec3V_In vCenterPos, WorldProbe::CShapeTestResults & rTestResults, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ, const bool bConsiderJunctions)
{
	TRouteInfo routeInfo;
	const bool bForceIncludeWidths = false;
	if(HelperCreateRouteInfoFromFollowRouteAndCenterPos(rFollowRoute,vCenterPos,routeInfo,bConsiderJunctions, bForceIncludeWidths))
	{
		TestProbesToVirtualRoadRouteInfo(routeInfo,rTestResults,pTestSegments,iNumSegments,fVehicleUpZ);
	}
}

void CVirtualRoad::TestProbesToVirtualRoadNodeListAndCenterPos(const CVehicleNodeList & rNodeList, Vec3V_In vCenterPos, WorldProbe::CShapeTestResults & rTestResults, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ)
{
	TRouteInfo routeInfo;
	if(HelperCreateRouteInfoFromNodeListAndCenterPos(rNodeList,vCenterPos,routeInfo))
	{
		TestProbesToVirtualRoadRouteInfo(routeInfo,rTestResults,pTestSegments,iNumSegments,fVehicleUpZ);
	}
}

void CVirtualRoad::TestProbesToVirtualRoadFollowRouteAndTwoPoints(const CVehicleFollowRouteHelper& rFollowRoute, Vec3V_In vA, Vec3V_In vB, WorldProbe::CShapeTestResults & rTestResults, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ, const bool bConsiderJunctions)
{
	TRouteInfo routeInfoA;
	TRouteInfo routeInfoB;
	const bool bForceIncludeWidths = false;
	bool bRouteInfoACreated = HelperCreateRouteInfoFromFollowRouteAndCenterPos(rFollowRoute,vA,routeInfoA,bConsiderJunctions, bForceIncludeWidths);
	bool bRouteInfoBCreated = HelperCreateRouteInfoFromFollowRouteAndCenterPos(rFollowRoute,vB,routeInfoB,bConsiderJunctions, bForceIncludeWidths);
	if(bRouteInfoACreated && bRouteInfoBCreated)
	{
		TestProbesToVirtualRoadTwoRouteInfos(routeInfoA,vA,routeInfoB,vB,rTestResults,pTestSegments,iNumSegments,fVehicleUpZ);
	}
}

void CVirtualRoad::TestProbesToVirtualRoadNodeListAndTwoPoints(const CVehicleNodeList & rNodeList, Vec3V_In vA, Vec3V_In vB, WorldProbe::CShapeTestResults & rTestResultsOut, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ)
{
	TRouteInfo routeInfoA;
	TRouteInfo routeInfoB;
	bool bRouteInfoACreated = HelperCreateRouteInfoFromNodeListAndCenterPos(rNodeList,vA,routeInfoA);
	bool bRouteInfoBCreated = HelperCreateRouteInfoFromNodeListAndCenterPos(rNodeList,vB,routeInfoB);
	if(bRouteInfoACreated && bRouteInfoBCreated)
	{
		TestProbesToVirtualRoadTwoRouteInfos(routeInfoA,vA,routeInfoB,vB,rTestResultsOut,pTestSegments,iNumSegments,fVehicleUpZ);
	}
}

void CVirtualRoad::TestProbesToVirtualRoadNodeListAndCenterNode(const CVehicleNodeList & rNodeList, int iCenterNode, WorldProbe::CShapeTestResults & rTestResultsOut, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ)
{
	TRouteInfo routeInfo;
	if(HelperCreateRouteInfoFromNodeListAndCenterNode(rNodeList,iCenterNode,routeInfo))
	{
		TestProbesToVirtualRoadRouteInfo(routeInfo,rTestResultsOut,pTestSegments,iNumSegments,fVehicleUpZ);
	}
}

u16 FindFakeLevelIndexForShapeTestResult()
{
	u16 nFakeLevelIndexForShapeTestResult = phInst::INVALID_INDEX;
	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(pLocalPlayer && pLocalPlayer->GetCurrentPhysicsInst())
	{
		nFakeLevelIndexForShapeTestResult = pLocalPlayer->GetCurrentPhysicsInst()->GetLevelIndex();

		// Player might not be in the level [e.g. cutscenes call phSimulator::DeleteObject()]. Try a fall-back before asserting.
		if(nFakeLevelIndexForShapeTestResult==phInst::INVALID_INDEX)
		{
			const int nNumOtherPedsToTry = 2;
			CEntityScannerIterator entityList = pLocalPlayer->GetPedIntelligence()->GetNearbyPeds();
			CEntity* pEnt = entityList.GetFirst();
			for(int i = 0; i < nNumOtherPedsToTry; ++i)
			{
				if (pEnt == NULL)
					break;

				if(pEnt->GetCurrentPhysicsInst())
					nFakeLevelIndexForShapeTestResult = pEnt->GetCurrentPhysicsInst()->GetLevelIndex();
				if(nFakeLevelIndexForShapeTestResult!=phInst::INVALID_INDEX)
					break;

				pEnt = entityList.GetNext();
			}
		}
	}
	// Fall back in-case we still haven't found a valid level index.
	if(nFakeLevelIndexForShapeTestResult==phInst::INVALID_INDEX && CPhysics::GetLevel()->GetNumObjects()>0)
	{
#if ENABLE_PHYSICS_LOCK
		phIterator it(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else	// ENABLE_PHYSICS_LOCK
		phIterator it;
#endif	// ENABLE_PHYSICS_LOCK
		it.InitCull_Sphere(camInterface::GetPos(), 100.0f);
		it.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED|phLevelBase::STATE_FLAG_ACTIVE|phLevelBase::STATE_FLAG_INACTIVE);

		nFakeLevelIndexForShapeTestResult = (u16)CPhysics::GetLevel()->GetFirstCulledObject(it);
		Assert(CPhysics::GetLevel()->IsInLevel(nFakeLevelIndexForShapeTestResult));
	}

	physicsAssertf(nFakeLevelIndexForShapeTestResult!=phInst::INVALID_INDEX || g_LoadScene.IsActive() || camInterface::IsFadedOut(),
		"Couldn't find an object with a valid level index for the fake virtual road probes, dummy cars will likely go mental!");

	return nFakeLevelIndexForShapeTestResult;
}

void CVirtualRoad::TestProbesToVirtualRoadRouteInfo(const TRouteInfo & rRouteInfo, WorldProbe::CShapeTestResults & rTestResults, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ)
{
	const ScalarV fZero(V_ZERO);
	const ScalarV fOne(V_ONE);

	WorldProbe::ResultIterator it = rTestResults.begin();
	CVirtualRoad::TWheelCollision wheelCollision;
	const int vehIsUpright = IsGreaterThanAll(fVehicleUpZ,fZero);

	for(int i=0; i<iNumSegments; i++, it++)
	{
		it->Reset();

		const Vec3V vA(RCC_VEC3V(pTestSegments[i].A));
		const Vec3V vB(RCC_VEC3V(pTestSegments[i].B));
		const Vec3V vTestPos = Average(vA,vB);
		CVirtualRoad::ProcessRoadCollision(rRouteInfo,vTestPos,wheelCollision,NULL);
		const Vec3V vHitPos = wheelCollision.m_vPosition.GetXYZ();
		const Vec3V vHitNormal = wheelCollision.m_vNormal;

		const ScalarV fSegZSpan = (vB.GetZ() - vA.GetZ());

		if(IsLessThanAll(fSegZSpan,fZero) && vehIsUpright)
		{
			const ScalarV fTimeOfImpact = (vHitPos.GetZ() - vA.GetZ()) / fSegZSpan;

			// fTimeImpact > 1 = miss, < 0 is valid and the wheel will deal with it
			if(IsLessThanAll(fTimeOfImpact,fOne))
			{
				const Vec3V vHitAlongSeg = Lerp(fTimeOfImpact,vA,vB);
				it->Set(vHitAlongSeg,vHitNormal,fTimeOfImpact,fZero,0,0,PGTAMATERIALMGR->g_idTarmac);
				it->SetInstance(FindFakeLevelIndexForShapeTestResult(), phInst::INVALID_INDEX);
				CVirtualRoad::DebugDrawHit(vHitAlongSeg,vHitNormal,Color_orange);
			}
		}
	}

	rTestResults.Update();
}

void CVirtualRoad::TestProbesToVirtualRoadTwoRouteInfos(const TRouteInfo & rRouteInfoA, Vec3V_In vRouteInfoCenterA, const TRouteInfo & rRouteInfoB, Vec3V_In vRouteInfoCenterB, WorldProbe::CShapeTestResults & rTestResults, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ)
{
	const ScalarV fZero(V_ZERO);
	const ScalarV fOne(V_ONE);

	WorldProbe::ResultIterator it = rTestResults.begin();
	CVirtualRoad::TWheelCollision wheelCollision;
	const int vehIsUpright = IsGreaterThanAll(fVehicleUpZ,fZero);

	for(int i=0; i<iNumSegments; i++, it++)
	{
		it->Reset();

		const Vec3V vProbeA(RCC_VEC3V(pTestSegments[i].A));
		const Vec3V vProbeB(RCC_VEC3V(pTestSegments[i].B));
		const Vec3V vTestPos = Average(vProbeA,vProbeB);
		const ScalarV fDistASq = DistSquared(vTestPos,vRouteInfoCenterA);
		const ScalarV fDistBSq = DistSquared(vTestPos,vRouteInfoCenterB);
		if(IsLessThanAll(fDistASq,fDistBSq))
		{
			CVirtualRoad::ProcessRoadCollision(rRouteInfoA,vTestPos,wheelCollision,NULL);
		}
		else
		{
			CVirtualRoad::ProcessRoadCollision(rRouteInfoB,vTestPos,wheelCollision,NULL);
		}
		const Vec3V vHitPos = wheelCollision.m_vPosition.GetXYZ();
		const Vec3V vHitNormal = wheelCollision.m_vNormal;

		const ScalarV fSegZSpan = (vProbeB.GetZ() - vProbeA.GetZ());

		if(IsLessThanAll(fSegZSpan,fZero) && vehIsUpright)
		{
			const ScalarV fTimeOfImpact = (vHitPos.GetZ() - vProbeA.GetZ()) / fSegZSpan;

			// fTimeImpact > 1 = miss, < 0 is valid and the wheel will deal with it
			if(IsLessThanAll(fTimeOfImpact,fOne))
			{
				const Vec3V vHitAlongSeg = Lerp(fTimeOfImpact,vProbeA,vProbeB);
				it->Set(vHitAlongSeg,vHitNormal,fTimeOfImpact,fZero,0,0,PGTAMATERIALMGR->g_idTarmac);
				it->SetInstance(FindFakeLevelIndexForShapeTestResult(), phInst::INVALID_INDEX);
				CVirtualRoad::DebugDrawHit(vHitAlongSeg,vHitNormal,Color_orange);
			}
		}
	}

	rTestResults.Update();
}

/////////////////////////////////////////////////////////////////////////////////
// CVirtualRoad::TestPointsToVirtualRoad* functions

bool CVirtualRoad::TestPointsToVirtualRoadNodeListAndCenterPos(const CVehicleNodeList & rNodeList, Vec3V_In vCenterPos, const Vec3V * vTestPoints, int iNumTestPoints, TWheelCollision * pResults)
{
	TRouteInfo routeInfo;
	if(HelperCreateRouteInfoFromNodeListAndCenterPos(rNodeList,vCenterPos,routeInfo))
	{
		for(int i=0; i<iNumTestPoints; i++)
		{
			CVirtualRoad::ProcessRoadCollision(routeInfo,vTestPoints[i],pResults[i],NULL);
			CVirtualRoad::DebugDrawHit(pResults[i].m_vPosition.GetXYZ(),pResults[i].m_vNormal,Color_yellow);
		}
		return true;
	}
	return false;
}

bool CVirtualRoad::TestPointsToVirtualRoadFollowRouteAndCenterPos(const CVehicleFollowRouteHelper& rFollowRoute, Vec3V_In vCenterPos, const Vec3V * vTestPoints, int iNumTestPoints, TWheelCollision * pResults, const bool bConsiderJunctions)
{
	TRouteInfo routeInfo;
	const bool bForceIncludeWidths = false;
	if(HelperCreateRouteInfoFromFollowRouteAndCenterPos(rFollowRoute,vCenterPos,routeInfo, bConsiderJunctions, bForceIncludeWidths))
	{
		for(int i=0; i<iNumTestPoints; i++)
		{
			CVirtualRoad::ProcessRoadCollision(routeInfo,vTestPoints[i],pResults[i],NULL);
			CVirtualRoad::DebugDrawHit(pResults[i].m_vPosition.GetXYZ(),pResults[i].m_vNormal,Color_yellow);
		}
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// CalculatePathInfo
// Wraps the two ways to find the path info - using a CVehicleFollowRouteHelper or a CVehicleNodeList

bool CVirtualRoad::CalculatePathInfo(const CVehicle & rVeh, bool bConstrainToStartAndEnd, bool bStrictPathDist, Vec3V_InOut vClosestPointOnPathOut, float & fMaxValidDistToClosestPointOut
	, const bool bOnlyTestConstraints)
{
	Vec3V vVehPos = rVeh.GetVehiclePosition();
	bool bPathFound = false;
	const CVehicleFollowRouteHelper* pFollowRoute = rVeh.GetIntelligence()->GetFollowRouteHelper();
	if(pFollowRoute)
	{
		bool bWasOffroad = rVeh.m_nVehicleFlags.bWasOffroadWithConstraint;
		bPathFound = CalculatePathInfoUsingFollowRoute(*pFollowRoute,vVehPos,bConstrainToStartAndEnd,bStrictPathDist,vClosestPointOnPathOut,fMaxValidDistToClosestPointOut, bOnlyTestConstraints, bWasOffroad);

		if (!bOnlyTestConstraints)
		{
			//sorry folks
			CVehicle& rVehMutable = const_cast<CVehicle&>(rVeh);

			rVehMutable.m_nVehicleFlags.bWasOffroadWithConstraint = bWasOffroad;

			if (bWasOffroad)
			{
				rVehMutable.ResetRealConversionFailedData();
			}
		}	
	}
	if(!bPathFound)
	{
		CVehicleNodeList * pNodeList = rVeh.GetIntelligence()->GetNodeList();
		if(pNodeList)
		{
			bPathFound = CalculatePathInfoUsingNodeList(*pNodeList,vVehPos,bConstrainToStartAndEnd,bStrictPathDist,vClosestPointOnPathOut,fMaxValidDistToClosestPointOut);
		}
	}
	return bPathFound;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalculatePathInfoUsingNodeList
// PURPOSE :  Using the nodes this function calculates the point where the constraint is
//			  centered on and the max distance from that point.
// PARAMETERS:	pVeh - the vehicle we ant the info for.
//				closestPointOnPath - the point along the path centerlines that is closest to the vehicle.
//				maxValidDistToClosestPoint - how far away from the point is still considered on the path.
// RETURNS:		Whether or not valid info could be generated.
/////////////////////////////////////////////////////////////////////////////////
bool CVirtualRoad::CalculatePathInfoUsingNodeList(const CVehicleNodeList & rNodeList, Vec3V_In vVehPos, bool UNUSED_PARAM(bConstrainToStartOrEnd), bool bStrictPathDist, Vec3V_InOut vClosestPointOnPathOut, float & fMaxValidDistToClosestPointOut)
{
	bool validLinkFound = false;
	const Vector3 vehCoords = RCC_VECTOR3(vVehPos);
	vClosestPointOnPathOut = vVehPos;
	fMaxValidDistToClosestPointOut = 0.0f;
	float	distanceToClosestPoint2 = LARGE_FLOAT;

	// Go through all of the line segments and find the one we are closest to.
	for (s32 l = 0; l < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1; l++)
	{
		const CNodeAddress nodeAddr1 = rNodeList.GetPathNodeAddr(l);
		const CNodeAddress nodeAddr2 = rNodeList.GetPathNodeAddr(l+1);

		if(	!nodeAddr1.IsEmpty() && ThePaths.IsRegionLoaded(nodeAddr1) &&
			!nodeAddr2.IsEmpty() && ThePaths.IsRegionLoaded(nodeAddr2))
		{
			Vector3 node1Coors;
			const CPathNode* pNode1 = ThePaths.FindNodePointer(nodeAddr1);
			Assert(pNode1);
			pNode1->GetCoors(node1Coors); 

			Vector3 node2Coors;
			const CPathNode* pNode2 = ThePaths.FindNodePointer(nodeAddr2);
			Assert(pNode2);
			pNode2->GetCoors(node2Coors);

#if DEBUG_DRAW && __BANK
			bool bIsFocusOrNoFocus = CDebugScene::FocusEntities_IsEmpty() || CDebugScene::FocusEntities_IsNearGroup(vVehPos);
			if((CVehicleAILodManager::ms_bDisplayDummyPath || CVehicleAILodManager::ms_bDisplayDummyPathDetailed) && bIsFocusOrNoFocus)
			{
				grcDebugDraw::Line(RCC_VEC3V(node1Coors),RCC_VEC3V(node2Coors),Color_red,Color_red,-1);
			}
#endif

			// Get the closest point along this segment.
			Vector3	closestPointOnThisSegment;
			float	portionAlongThisSegment;
			{
				// Find the distance of the vehicle to this line.
				const Vector3	linkDiff	= node2Coors - node1Coors;
				const Vector3	diffVehicle	= vehCoords - node1Coors;

				portionAlongThisSegment = diffVehicle.Dot(linkDiff);
				if(portionAlongThisSegment <= 0.0f)
				{
					closestPointOnThisSegment = node1Coors;
				}
				else
				{
					const float length2 = linkDiff.Mag2();
					portionAlongThisSegment /= length2;
					if (portionAlongThisSegment >= 1.0f)
					{
						closestPointOnThisSegment = node2Coors;
					}
					else
					{
						closestPointOnThisSegment = node1Coors + linkDiff * portionAlongThisSegment;
					}
				}
			}

			const float dist2 = (closestPointOnThisSegment - vehCoords).Mag2();
			if(dist2 < distanceToClosestPoint2)
			{
				validLinkFound = true;

				distanceToClosestPoint2 = dist2;
				vClosestPointOnPathOut = RCC_VEC3V(closestPointOnThisSegment);

				const CPathNodeLink& rLink = *ThePaths.FindLinkPointerSafe(nodeAddr1.GetRegion(),rNodeList.GetPathLinkIndex(l));
				const s32 lanesOurWay = rLink.m_1.m_LanesToOtherNode;
				const s32 lanesOtherWay = rLink.m_1.m_LanesFromOtherNode;
				const s32 lanesToUse = lanesOurWay > 0 ? lanesOurWay : rage::Max(lanesOurWay, lanesOtherWay);
				const s32 centralReservationWidth = rLink.m_1.m_Width;
				const float fLaneWidth = rLink.GetLaneWidth();
				fMaxValidDistToClosestPointOut = lanesToUse * fLaneWidth + (centralReservationWidth * 0.5f);

				if(!bStrictPathDist)
				{
					//const float fWidth1 = pNode1->IsJunctionNode() ? CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction : CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
					//const float fWidth2 = pNode2->IsJunctionNode() ? CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction : CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
					float fWidth1, fWidth2;
					if (pNode1->IsJunctionNode())
					{
						float fLength = 0.0f;
						if (HelperGetLongestJunctionEdge(pNode1->m_address, pNode1->GetPos(), fLength))
						{
							fWidth1 = rage::Max(3.0f, fLength * CVehicleAILodManager::ms_fPercentOfJunctionLengthToAddToConstraint);
						}
						else
						{
							fWidth1 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction;
						}
					}
					else if (pNode2->IsJunctionNode())
					{
						float fLength = 0.0f;
						if (HelperGetLongestJunctionEdge(pNode2->m_address, pNode2->GetPos(), fLength))
						{
							fWidth1 = rage::Max(3.0f, fLength * CVehicleAILodManager::ms_fPercentOfJunctionLengthToAddToConstraint);
						}
						else
						{
							fWidth1 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistancePreJunction;
						}
					}
					else if (rLink.IsDontUseForNavigation())
					{
						fWidth1 = fLaneWidth * lanesToUse;
					}
					else
					{
						fWidth1 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
					}

					if (pNode2->IsJunctionNode())
					{
						float fLength = 0.0f;
						if (HelperGetLongestJunctionEdge(pNode2->m_address, pNode2->GetPos(), fLength))
						{
							fWidth2 = rage::Max(3.0f, fLength * CVehicleAILodManager::ms_fPercentOfJunctionLengthToAddToConstraint);
						}
						else
						{
							fWidth2 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction;
						}
					}
					else if (rLink.IsDontUseForNavigation())
					{
						fWidth2 = fLaneWidth * lanesToUse;
					}
					else
					{
						fWidth2 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
					}
					fMaxValidDistToClosestPointOut += (fWidth1 * (1.0f-portionAlongThisSegment)) + (fWidth2 * portionAlongThisSegment);	// to give the car some room to maneuver.
				}
			}
		}
	}

#if DEBUG_DRAW && __BANK
	if(validLinkFound && CVehicleAILodManager::ms_bDisplayDummyPathConstraint)
	{
		Vec3V vA = vClosestPointOnPathOut;
		Vec3V vB = vVehPos;
		grcDebugDraw::Line(vA,vB,Color_orange,Color_orange,-1);
		ScalarV fLerp(fMaxValidDistToClosestPointOut);
		fLerp /= Dist(vA,vB);
		vB = Lerp(fLerp,vA,vB);
		vB.SetZf(vB.GetZf() - 0.05f);
		grcDebugDraw::Line(vA,vB,Color_yellow,Color_yellow,-1);
	}
#endif

	return validLinkFound;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalculatePathInfoUsingFollowRoute
// PURPOSE :  Like CalculatePathInfo, but uses the follow route instead of a nodelist
/////////////////////////////////////////////////////////////////////////////////
bool CVirtualRoad::CalculatePathInfoUsingFollowRoute(const CVehicleFollowRouteHelper & rFollowRoute, Vec3V_In vVehPos, bool /*bConstrainToStartOrEnd*/, bool /*bStrictPathDist*/
	, Vec3V_InOut vClosestPointOnPathOut, float & fMaxValidDistToClosestPointOut, const bool bOnlyTestConstraints, bool& bWasOffroadInOut)
{
	TRouteInfo routeInfo;
	const bool bClosestSegFound = HelperCreateRouteInfoFromFollowRouteAndCenterPos(rFollowRoute, vVehPos, routeInfo, false, true);
	//const bool bClosestSegFound = HelperCreateRouteInfoFromFollowRouteAndCenterNode(rFollowRoute, rFollowRoute.GetCurrentTargetPoint(), false, true, routeInfo, false);

	if (bClosestSegFound)
	{
		TRouteConstraint routeConstraint;
		routeConstraint.m_bWasOffroad = bWasOffroadInOut;
		ProcessRouteConstraint(routeInfo, vVehPos, routeConstraint, bOnlyTestConstraints);

		bWasOffroadInOut = routeConstraint.m_bWasOffroad;
		vClosestPointOnPathOut = routeConstraint.m_vConstraintPosOnRoute;
		fMaxValidDistToClosestPointOut = routeConstraint.m_fConstraintRadiusOnRoute.Getf();
	}

	return bClosestSegFound;

// 	int iClosestSeg;
// 	ScalarV fTAlongClosestV;
// 	bool bBeforeStart, bAfterEnd;
// 	bool bClosestSegFound = CVirtualRoad::HelperFindClosestSegmentAlongFollowRoute(vVehPos,rFollowRoute,iClosestSeg,fTAlongClosestV,vClosestPointOnPathOut,bBeforeStart,bAfterEnd);
// 
// 	if(bClosestSegFound)
// 	{
// 		float fTAlongClosest = fTAlongClosestV.Getf();
// 		const CRoutePoint & rPoint1 = rFollowRoute.GetRoutePoints()[iClosestSeg];
// 		const CRoutePoint & rPoint2 = rFollowRoute.GetRoutePoints()[iClosestSeg+1];
// 
// 		// TODO: I don't think this lane width calculation is very good.  Need to consolidate to a single function that finds the constraint width.
// 		// TODO: Replace below with the new ProcessRouteConstraint() function
// 
// 		const s32 lanesOurWay = rPoint1.m_iNoLanesThisSide;
// 		const s32 lanesOtherWay = rPoint1.m_iNoLanesOtherSide;
// 		const s32 numLanesToUse = lanesOurWay > 0 ? lanesOurWay : rage::Max(lanesOurWay, lanesOtherWay);
// 		const float fExtraLanesWidth = numLanesToUse > 1 ? (numLanesToUse-1) * rPoint1.m_fLaneWidth : 0.0f;
// 		fMaxValidDistToClosestPointOut = rPoint1.m_fCentralLaneWidth 
// 			+ fExtraLanesWidth
// 			+ (rPoint1.m_fLaneWidth * 0.5f);
// 
// 		if((bBeforeStart || bAfterEnd) && !bConstrainToStartOrEnd)
// 		{
// 			// Don't constrain if we are before the start or after the end
// 			float fConstraitBufferForVelocity = 10.0f;
// 			fMaxValidDistToClosestPointOut = RCC_VECTOR3(vVehPos).Dist(RCC_VECTOR3(vClosestPointOnPathOut)) + fConstraitBufferForVelocity;
// 		}
// 		else if(!bStrictPathDist)
// 		{
// #if 0
// 			float fWidth1 = (rPoint1.m_bJunctionNode || rPoint1.m_bIgnoreForNavigation) ? CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction
// 				: ((rPoint2.m_bJunctionNode || rPoint2.m_bIgnoreForNavigation) ? CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction / 2.0f : CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance);
// 			float fWidth2 = (rPoint2.m_bJunctionNode || rPoint2.m_bIgnoreForNavigation) ? CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction
// 				: ((rPoint1.m_bJunctionNode || rPoint1.m_bIgnoreForNavigation) ? CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction / 2.0f : CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance);
// #endif //0
// 			float fWidth1, fWidth2;
// 			if (rPoint1.m_bJunctionNode)
// 			{
// 				fWidth1 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction;
// 			}
// 			else if (rPoint2.m_bJunctionNode)
// 			{
// 				fWidth1 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction / 2.0f;
// 			}
// 			else if (rPoint1.m_bIgnoreForNavigation)
// 			{
// 				fWidth1 = rPoint1.m_fLaneWidth;
// 			}
// 			else if (rPoint2.m_bIgnoreForNavigation)
// 			{
// 				fWidth1 = rPoint2.m_fLaneWidth;
// 			}
// 			else 
// 			{
// 				fWidth1 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
// 			}
// 
// 			if (rPoint2.m_bJunctionNode)
// 			{
// 				fWidth2 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction;
// 			}
// 			else if (rPoint1.m_bJunctionNode)
// 			{
// 				fWidth2 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistanceAtJunction / 2.0f;
// 			}
// 			else if (rPoint2.m_bIgnoreForNavigation)
// 			{
// 				fWidth2 = rPoint2.m_fLaneWidth;
// 			}
// 			else if (rPoint1.m_bIgnoreForNavigation)
// 			{
// 				fWidth2 = rPoint1.m_fLaneWidth;
// 			}
// 			else 
// 			{
// 				fWidth2 = CVehicleAILodManager::ms_fConstrainToRoadsExtraDistance;
// 			}
// 
// 			fMaxValidDistToClosestPointOut += (fWidth1 * (1.0f-fTAlongClosest)) + (fWidth2 * fTAlongClosest);	// to give the car some room to maneuver.
// 		}
// 
// #if DEBUG_DRAW && __BANK
// 		if(CVehicleAILodManager::ms_bDisplayDummyPathConstraint)
// 		{
// 			Vec3V vA = vClosestPointOnPathOut;
// 			Vec3V vB = vVehPos;
// 			Vec3V vOffset(0.0f,0.0f,-0.05f);
// 			grcDebugDraw::Line(vA,vB+vOffset,Color_blue,-1);
// 			ScalarV fLerpT(fMaxValidDistToClosestPointOut);
// 			fLerpT /= Dist(vA,vB);
// 			Color32 col = IsLessThanAll(fLerpT,ScalarV(V_ONE)) ? Color_red : Color_green;
// 			vB = Lerp(fLerpT,vA,vB);
// 			grcDebugDraw::Line(vA,vB+vOffset*ScalarV(V_TWO),col,-1);
// 		}
// #endif
// 	}
// 
// 	return bClosestSegFound;
}

#if DEBUG_DRAW && __BANK
void CVirtualRoad::DebugDrawHit(Vec3V_In vHitPos, Vec3V_In UNUSED_PARAM(vHitNormal), Color32 col)
{
	if(CVehicleAILodManager::ms_bDisplayDummyWheelHits)
	{
		static int iFrames = -1;
		grcDebugDraw::Cross(vHitPos,0.25f,col,iFrames);
		//grcDebugDraw::Line(vHitPos,vHitPos+vHitNormal*ScalarV(V_HALF),col,-1);
	}
}

void CVirtualRoad::DebugDrawRouteInfo(const TRouteInfo & rRouteInfo, Vec3V_In vWorldWheelPos, int iSeg)
{
	bool bIsFocusOrNoFocus = CDebugScene::FocusEntities_IsEmpty() || CDebugScene::FocusEntities_IsNearGroup(vWorldWheelPos,6.0f);
	if(!bIsFocusOrNoFocus || !(CVehicleAILodManager::ms_bDisplayDummyPath || CVehicleAILodManager::ms_bDisplayDummyPathDetailed))
	{
		return;
	}

	// Shared vars
	Mat33V tiltMat;
	const Vec3V vOffset = Vec3V(V_Z_AXIS_WZERO) * ScalarV(0.08f);
	const ScalarV fCamberRightSide0 = rRouteInfo.m_vCambers.GetX();
	const ScalarV fCamberLeftSide0 = rRouteInfo.m_vCambers.GetY();
	const ScalarV fCamberRightSide1 = rRouteInfo.m_vCambers.GetZ();
	const ScalarV fCamberLeftSide1 = rRouteInfo.m_vCambers.GetW();
	const ScalarV fWidthRight0 = !rRouteInfo.m_bCalcWidthConstraint ? ScalarV(V_FOUR) : rRouteInfo.m_vWidths.GetX();
	const ScalarV fWidthRight1 = !rRouteInfo.m_bCalcWidthConstraint ? ScalarV(V_FOUR) : rRouteInfo.m_vWidths.GetZ();
	const ScalarV fWidthLeft0 = !rRouteInfo.m_bCalcWidthConstraint ? ScalarV(V_FOUR) : rRouteInfo.m_vWidths.GetY();
	const ScalarV fWidthLeft1 = !rRouteInfo.m_bCalcWidthConstraint ? ScalarV(V_FOUR) : rRouteInfo.m_vWidths.GetW();

	// Draw center line for both segments
	grcDebugDraw::Line(rRouteInfo.m_vPoints[0]+vOffset,rRouteInfo.m_vPoints[1]+vOffset,Color_LightBlue2,-1);
	if(rRouteInfo.m_bHasTwoSegments)
		grcDebugDraw::Line(rRouteInfo.m_vPoints[1]+vOffset,rRouteInfo.m_vPoints[2]+vOffset,Color_PaleGreen2,-1);

	// Draw info for the current segment
	{
		const Vec3V vNodeDir = NormalizeFast(rRouteInfo.m_vNodeDir[iSeg]);
		const Vec3V vRightFlat = NormalizeFast( Cross( vNodeDir, RCC_VEC3V(ZAXIS) ) );

		// Calc right vector for each side
		//const ScalarV fDistRight = Dot(vWorldWheelPos - rRouteInfo.m_vPoints[iSeg], vRightFlat);
		//const BoolV bOnRight = (fDistRight > ScalarV(V_ZERO));
		const ScalarV fCamberRightSide = (iSeg==0) ? fCamberRightSide0 : fCamberRightSide1;
		const ScalarV fCamberLeftSide = (iSeg==0) ? fCamberLeftSide0 : fCamberLeftSide1;

		Mat33VFromAxisAngle(tiltMat, vNodeDir, fCamberRightSide);
		const Vec3V vRightTiltedThisSide = Multiply(tiltMat, vRightFlat);
		Mat33VFromAxisAngle(tiltMat, vNodeDir, fCamberLeftSide);
		const Vec3V vRightTiltedOtherSide = Multiply(tiltMat, vRightFlat);

		// Draw extents for the segment being used
		if(CVehicleAILodManager::ms_bDisplayDummyPathDetailed)
		{
			const Vec3V vBack = rRouteInfo.m_vPoints[iSeg];
			const Vec3V vFront = rRouteInfo.m_vPoints[iSeg+1];
			const ScalarV fWidthRight = (iSeg==0) ? fWidthRight0 : fWidthRight1;
			const ScalarV fWidthLeft = (iSeg==0) ? fWidthLeft0 : fWidthLeft1;
			const Vec3V vLeftBack = vBack - vRightTiltedOtherSide * fWidthLeft;
			const Vec3V vRightBack = vBack + vRightTiltedThisSide * fWidthRight;
			const Vec3V vLeftFront = vFront - vRightTiltedOtherSide * fWidthLeft;
			const Vec3V vRightFront = vFront + vRightTiltedThisSide * fWidthRight;
			Color32 colQuadRight = rRouteInfo.m_bCalcWidthConstraint ?
				((iSeg==0) ? Color_LightBlue2 : Color_PaleGreen2) :
				((iSeg==0) ? Color_cornsilk2 : Color_LavenderBlush2);
			Color32 colQuadLeft = rRouteInfo.m_bCalcWidthConstraint ?
				((iSeg==0) ? Color_PaleGreen2 : Color_LightBlue2) :
				((iSeg==0) ? Color_LavenderBlush2 : Color_cornsilk2);
			colQuadRight.SetAlpha(100);
			colQuadLeft.SetAlpha(100);
			grcDebugDraw::Quad(vBack+vOffset,vFront+vOffset,vRightFront+vOffset,vRightBack+vOffset,colQuadRight,true,true,-1);
			grcDebugDraw::Quad(vBack+vOffset,vLeftBack+vOffset,vLeftFront+vOffset,vFront+vOffset,colQuadLeft,true,true,-1);
		}
	}

	// Draw tilts for each segment and (calculated tilts) for each node
	if(CVehicleAILodManager::ms_bDisplayDummyPathDetailed)
	{
		// Segment 0
		const Vec3V vPoint0 = rRouteInfo.m_vPoints[0];
		const Vec3V vPoint1 = rRouteInfo.m_vPoints[1];
		const Vec3V vMid0 = Average(vPoint0,vPoint1);
		const Vec3V vNodeDir0 = NormalizeFast(rRouteInfo.m_vNodeDir[0]);
		const Vec3V vRightFlat0 = NormalizeFast(Cross(vNodeDir0,RCC_VEC3V(ZAXIS)));

		Mat33VFromAxisAngle(tiltMat, vNodeDir0, fCamberRightSide0);
		const Vec3V vRightTiltedRightSideSeg0 = Multiply(tiltMat,vRightFlat0);
		Mat33VFromAxisAngle(tiltMat, vNodeDir0, fCamberLeftSide0);
		const Vec3V vRightTiltedLeftSideSeg0 = Multiply(tiltMat,vRightFlat0);

		grcDebugDraw::Line(vMid0+vOffset,vMid0+vRightTiltedRightSideSeg0*fWidthRight0+vOffset,Color_OliveDrab,-1);
		grcDebugDraw::Line(vMid0+vOffset,vMid0-vRightTiltedLeftSideSeg0*fWidthLeft0+vOffset,Color_OliveDrab,-1);

		grcDebugDraw::Line(vPoint0+vOffset,vPoint0+vRightTiltedRightSideSeg0*fWidthRight0+vOffset,Color_OliveDrab,-1);
		grcDebugDraw::Line(vPoint0+vOffset,vPoint0-vRightTiltedLeftSideSeg0*fWidthLeft0+vOffset,Color_OliveDrab,-1);

		grcDebugDraw::Line(vPoint1+vOffset,vPoint1+vRightTiltedRightSideSeg0*fWidthRight0+vOffset,Color_OliveDrab,-1);
		grcDebugDraw::Line(vPoint1+vOffset,vPoint1-vRightTiltedLeftSideSeg0*fWidthLeft0+vOffset,Color_OliveDrab,-1);

		if(rRouteInfo.m_bHasTwoSegments)
		{
			// Segment 1
			const Vec3V vPoint2 = rRouteInfo.m_vPoints[2];
			const Vec3V vMid1 = Average(vPoint1,vPoint2);
			const Vec3V vNodeDir1 = rRouteInfo.m_vNodeDirNormalised[1];
			const Vec3V vRightFlat1 = NormalizeFast(Cross(vNodeDir1,RCC_VEC3V(ZAXIS)));

			Mat33VFromAxisAngle(tiltMat, vNodeDir1, fCamberRightSide1);
			const Vec3V vRightTiltedRightSideSeg1 = Multiply(tiltMat,vRightFlat1);
			Mat33VFromAxisAngle(tiltMat, vNodeDir1, fCamberLeftSide1);
			const Vec3V vRightTiltedLeftSideSeg1 = Multiply(tiltMat,vRightFlat1);

			grcDebugDraw::Line(vMid1+vOffset,vMid1+vRightTiltedRightSideSeg1*fWidthRight1+vOffset,Color_OliveDrab,-1);
			grcDebugDraw::Line(vMid1+vOffset,vMid1-vRightTiltedLeftSideSeg1*fWidthLeft1+vOffset,Color_OliveDrab,-1);

			grcDebugDraw::Line(vPoint1+vOffset,vPoint1+vRightTiltedRightSideSeg1*fWidthRight1+vOffset,Color_OliveDrab,-1);
			grcDebugDraw::Line(vPoint1+vOffset,vPoint1-vRightTiltedLeftSideSeg1*fWidthLeft1+vOffset,Color_OliveDrab,-1);

			grcDebugDraw::Line(vPoint2+vOffset,vPoint2+vRightTiltedRightSideSeg1*fWidthRight1+vOffset,Color_OliveDrab,-1);
			grcDebugDraw::Line(vPoint2+vOffset,vPoint2-vRightTiltedLeftSideSeg1*fWidthLeft1+vOffset,Color_OliveDrab,-1);
		}
	}
}
#endif




//////////////////////////////////////////////

void CVirtualRoad::ProcessRoadCollision(const TRouteInfo & rRouteInfo, Vec3V_In vWorldWheelPos, TWheelCollision & rWheelCollisionOut, TRouteConstraint * pRouteConstraintOut)
{
	if (rRouteInfo.m_bConsiderJunction)
	{
#if OPT_GET_VIRTUAL_JUNCTION_FROM_NODE_ADDR
		if(CVirtualJunctionInterface::ProcessCollisionFromNodeAddr(rRouteInfo.GetJunctionNodeAddr(), vWorldWheelPos, rWheelCollisionOut, pRouteConstraintOut))
		{
			return;
		}
#else
		if(CVirtualJunctionInterface::ProcessCollision(rRouteInfo.GetJunctionPos(), vWorldWheelPos, rWheelCollisionOut, pRouteConstraintOut))
		{
			return;
		}
#endif
	}

	// Vector constants
	const ScalarV fZero(V_ZERO);
	const ScalarV fHalf(V_HALF);
	const Vec3V vZAxis(V_Z_AXIS_WZERO);
	const ScalarV fFalloffHeightV(LINK_TILT_FALLOFF_HEIGHT);
	const ScalarV fFalloffWidth(LINK_TILT_FALLOFF_WIDTH);
	const ScalarV fRoadEdgeCamberBuffer(ROAD_EDGE_BUFFER_FOR_CAMBER_CALCULATION);

	ASSERT_ONLY(rRouteInfo.Validate());

	//
	int iSeg = 0;
	const Vec3V vPoint0(rRouteInfo.m_vPoints[0]);
	const Vec3V vPoint1(rRouteInfo.m_vPoints[1]);
	//const Vec3V vPoint2(rRouteInfo.m_vPoints[2]);
	const Vec3V vNodeDir0(rRouteInfo.m_vNodeDir[0]);
	const Vec3V vNodeDir1(rRouteInfo.m_vNodeDir[1]);
	const Vec3V vNodeDirNorm0(rRouteInfo.m_vNodeDirNormalised[0]);
	const Vec3V vNodeDirNorm1(rRouteInfo.m_vNodeDirNormalised[1]);
	const Vec4V vCambers(rRouteInfo.m_vCambers);
	const Vec3V vRightFlat0 = NormalizeFast(Cross(vNodeDirNorm0,vZAxis));
	Vec3V vRightFlat1, vRightFlatSeg;
	// TODO: Get distance and t-value in one function
	const ScalarV fDistsSqrToSeg0 = geomDistances::Distance2SegToPointV(vPoint0,vNodeDir0,vWorldWheelPos);
	const ScalarV fT0 = geomTValues::FindTValueSegToPoint(vPoint0,vNodeDir0,vWorldWheelPos);
	ScalarV fDistsSqrToSeg1;
	ScalarV fT1, fTSeg = fT0;
	u32 iShouldBlendInSeg0 = 0;
	u32 iShouldBlendInSeg1 = 0;

	if(rRouteInfo.m_bHasTwoSegments)
	{
		vRightFlat1 = NormalizeFast(Cross(vNodeDirNorm1,vZAxis));
		fDistsSqrToSeg1 = geomDistances::Distance2SegToPointV(vPoint1,vNodeDir1,vWorldWheelPos);
		fT1 = geomTValues::FindTValueSegToPoint(vPoint1,vNodeDir1,vWorldWheelPos);
		if(IsLessThanAll(fDistsSqrToSeg0,fDistsSqrToSeg1))
		{
			iSeg = 0;
			fTSeg = fT0;
			vRightFlatSeg = vRightFlat0;
			iShouldBlendInSeg1 = IsGreaterThanAll(fT0,fHalf);
		}
		else
		{
			iSeg = 1;
			fTSeg = fT1;
			vRightFlatSeg = vRightFlat1;
			iShouldBlendInSeg0 = IsLessThanAll(fT1,fHalf);
		}
	}

	// Output values (may be calculated on a single segment or may require blending)
	Vec3V vOutPos, vOutNormal;
	ScalarV fOutHeight;

	// Calculate hit on the required segments (0, 1, or 0 & 1)
	Vec3V vHitPos0, vHitPos1;
	Vec3V vHitNormal0, vHitNormal1;
	ScalarV fHeight0, fHeight1;
	if(iSeg==0 || iShouldBlendInSeg0)
	{
		// Need to calculate hit on segment 0
		const ScalarV fDistRight0 = Dot(vWorldWheelPos - vPoint0, vRightFlat0);
		const BoolV bOnRight0 = (fDistRight0 > fZero);
		const ScalarV fCamber0 = SelectFT(bOnRight0,vCambers.GetY(),vCambers.GetX());
		const QuatV quatRightTilt0 = QuatVFromAxisAngle(vNodeDirNorm0,fCamber0);
		const Vec3V vRightTilted0 = Transform(quatRightTilt0, vRightFlat0);

		// Normal and height
		vHitNormal0 = Cross(vRightTilted0,vNodeDirNorm0);
		fHeight0 = Dot(vWorldWheelPos - vPoint0, vHitNormal0);

		// Falloff offset
		if(rRouteInfo.m_iCamberFalloffs[0] || rRouteInfo.m_iCamberFalloffs[1])
		{
			bool bOnRightBool0 = bOnRight0.Getb();
			if((bOnRightBool0 && rRouteInfo.m_iCamberFalloffs[0]) || (!bOnRightBool0 && rRouteInfo.m_iCamberFalloffs[1]))
			{
				const ScalarV fWidthForFalloff0 = SelectFT(bOnRight0,rRouteInfo.m_vWidths.GetY(),rRouteInfo.m_vWidths.GetX()) + fRoadEdgeCamberBuffer;
				const ScalarV fDistFromEdge0 = fWidthForFalloff0 + SelectFT(bOnRight0,fDistRight0,-fDistRight0);
				const ScalarV fFalloffZ0 = -fFalloffHeightV * Max(fZero,(fFalloffWidth-fDistFromEdge0)/fFalloffWidth);
				const ScalarV fFalloffHeight0 = fFalloffZ0 * Dot(vHitNormal0,vZAxis);
				fHeight0 = fHeight0 - fFalloffHeight0;
				Assert(IsFiniteAll(fHeight0));
#if DEBUG_DRAW && 0
				if(bOnRightBool0)
				{
					const Vec3V vRightBack0 = vPoint0 + fWidthForFalloff0 * vRightFlat0;
					const Vec3V vRightFront0 = vPoint1 + fWidthForFalloff0 * vRightFlat0;
					grcDebugDraw::Line(vRightBack0,vRightFront0,Color_cornsilk2,-1);
					const Vec3V vTOn0 = Lerp(fT0,vPoint0,vPoint1);
					const Vec3V vWheelPos0 = vTOn0 + fDistRight0 * vRightFlat0;
					const Vec3V vEdgePos0 = vTOn0 + fWidthForFalloff0 * vRightFlat0;
					grcDebugDraw::Line(vTOn0,vWheelPos0,Color_purple,-1);
					grcDebugDraw::Line(vWheelPos0,vEdgePos0,Color_pink,-1);
				}
#endif
			}
		}

		// Hit position
		vHitPos0 = vWorldWheelPos - vHitNormal0 * fHeight0;

		// Fill in final output values in case they aren't blended
		vOutPos = vHitPos0;
		vOutNormal = vHitNormal0;
		fOutHeight = fHeight0;

		//DEBUG_DRAW_ONLY(grcDebugDraw::Cross(vHitPos0,0.2f,Color_red,-1);)
		//DEBUG_DRAW_ONLY(grcDebugDraw::Line(vHitPos0,vHitPos0+vHitNormal0,Color_red,-1);)
	}
	if(iSeg==1 || iShouldBlendInSeg1)
	{
		// Need to calculate hit on segment 1
		const ScalarV fDistRight1 = Dot(vWorldWheelPos - vPoint1, vRightFlat1);
		const BoolV bOnRight1 = (fDistRight1 > fZero);
		const ScalarV fCamber1 = SelectFT(bOnRight1,vCambers.GetW(),vCambers.GetZ());
		const QuatV quatRightTilt1 = QuatVFromAxisAngle(vNodeDirNorm1,fCamber1);
		const Vec3V vRightTilted1 = Transform(quatRightTilt1, vRightFlat1);

		// Normal and height
		vHitNormal1 = Cross(vRightTilted1,vNodeDirNorm1);
		fHeight1 = Dot(vWorldWheelPos - vPoint1, vHitNormal1);

		// Falloff offset
		if(rRouteInfo.m_iCamberFalloffs[2] || rRouteInfo.m_iCamberFalloffs[3])
		{
			bool bOnRightBool1 = bOnRight1.Getb();
			if((bOnRightBool1 && rRouteInfo.m_iCamberFalloffs[2]) || (!bOnRightBool1 && rRouteInfo.m_iCamberFalloffs[3]))
			{
				const ScalarV fWidthForFalloff1 = SelectFT(bOnRight1,rRouteInfo.m_vWidths.GetW(),rRouteInfo.m_vWidths.GetZ()) + fRoadEdgeCamberBuffer;
				const ScalarV fDistFromEdge1 = fWidthForFalloff1 + SelectFT(bOnRight1,fDistRight1,-fDistRight1);
				const ScalarV fFalloffZ1 = -fFalloffHeightV * Max(fZero,(fFalloffWidth-fDistFromEdge1)/fFalloffWidth);
				const ScalarV fFalloffHeight1 = fFalloffZ1 * Dot(vHitNormal1,vZAxis);
				fHeight1 = fHeight1 - fFalloffHeight1;
				Assert(IsFiniteAll(fHeight1));
#if DEBUG_DRAW && 0
				if(bOnRightBool1)
				{
					const Vec3V vRightBack1 = vPoint1 + fWidthForFalloff1 * vRightFlat1;
					const Vec3V vRightFront1 = vPoint2 + fWidthForFalloff1 * vRightFlat1;
					grcDebugDraw::Line(vRightBack1,vRightFront1,Color_LimeGreen,-1);
					const Vec3V vTOn1 = Lerp(fT1,vPoint1,vPoint2);
					const Vec3V vWheelPos1 = vTOn1 + fDistRight1 * vRightFlat1;
					const Vec3V vEdgePos1 = vTOn1 + fWidthForFalloff1 * vRightFlat1;
					grcDebugDraw::Line(vTOn1,vWheelPos1,Color_purple,-1);
					grcDebugDraw::Line(vWheelPos1,vEdgePos1,Color_pink,-1);
				}
#endif
			}
		}

		// Hit position
		vHitPos1 = vWorldWheelPos - vHitNormal1 * fHeight1;

		// Fill in final output values in case they aren't blended
		vOutPos = vHitPos1;
		vOutNormal = vHitNormal1;
		fOutHeight = fHeight1;

		//DEBUG_DRAW_ONLY(grcDebugDraw::Cross(vHitPos1,0.2f,Color_green,-1);)
		//DEBUG_DRAW_ONLY(grcDebugDraw::Line(vHitPos1,vHitPos1+vHitNormal1,Color_green,-1);)
	}

	// Blend the values from the two segments if necessary
	if(iShouldBlendInSeg1)
	{
		const ScalarV fAlpha = fT0 - fHalf;
		vOutPos = Lerp(fAlpha,vHitPos0,vHitPos1);
		vOutNormal = NormalizeSafe(vWorldWheelPos - vOutPos, vHitNormal0);
		vOutNormal = SelectFT(vOutNormal.GetZ()>=fZero,Negate(vOutNormal),vOutNormal);
		fOutHeight = Dot(vWorldWheelPos - vOutPos, vOutNormal);
	}
	else if(iShouldBlendInSeg0)
	{
		const ScalarV fAlpha = fHalf + fT1;
		vOutPos = Lerp(fAlpha,vHitPos0,vHitPos1);
		vOutNormal = NormalizeSafe(vWorldWheelPos - vOutPos, vHitNormal1);
		vOutNormal = SelectFT(vOutNormal.GetZ()>=fZero,Negate(vOutNormal),vOutNormal);
		fOutHeight = Dot(vWorldWheelPos - vOutPos, vOutNormal);
	}

	Assert(IsFiniteAll(vOutPos));
	Assert(IsFiniteAll(vOutNormal));
	Assert(IsFiniteAll(fOutHeight));

#if __BANK
	if(CVehicleAILodManager::ms_bRaiseDummyRoad)
	{
		fOutHeight -= ScalarV(CVehicleAILodManager::ms_fRaiseDummyRoadHeight);
		vOutPos = vWorldWheelPos - vOutNormal * fOutHeight;
	}
#endif

	rWheelCollisionOut.m_vPosition = Vec4V(vOutPos,fOutHeight);
	rWheelCollisionOut.m_vNormal = vOutNormal;
	//

	// Calculate the stay-on-road constraint
	if(pRouteConstraintOut && rRouteInfo.m_bCalcWidthConstraint)
	{
		ProcessRouteConstraintWithPrecalcValues(rRouteInfo,vWorldWheelPos,fTSeg,fT0,fT1,fDistsSqrToSeg0,fDistsSqrToSeg1,vRightFlat0,vRightFlat1,*pRouteConstraintOut, false);
	}

	// Confirm results are valid
	aiAssert(IsFiniteAll(rWheelCollisionOut.m_vPosition));

	DebugDrawRouteInfo(rRouteInfo,vWorldWheelPos,iSeg);
}


void CVirtualRoad::ProcessRouteConstraint(const TRouteInfo & rRouteInfo, Vec3V_In vWorldWheelPos, TRouteConstraint & rRouteConstraintOut, const bool bOnlyTestConstraints)
{
	ASSERT_ONLY(rRouteInfo.Validate());

	// Find all the "pre-calc" values and call the core function

	// Constants
	const Vec3V vZAxis(V_Z_AXIS_WZERO);

	// Values from rRouteInfo
	const Vec3V vPoint0(rRouteInfo.m_vPoints[0]);
	const Vec3V vPoint1(rRouteInfo.m_vPoints[1]);
	const Vec3V vNodeDir0(rRouteInfo.m_vNodeDir[0]);
	const Vec3V vNodeDir1(rRouteInfo.m_vNodeDir[1]);
	const Vec3V vNodeDirNorm0(rRouteInfo.m_vNodeDirNormalised[0]);
	const Vec3V vNodeDirNorm1(rRouteInfo.m_vNodeDirNormalised[1]);
	const Vec3V vRightFlat0 = NormalizeFast(Cross(vNodeDirNorm0,vZAxis));
	Vec3V vRightFlat1;
	const ScalarV fDistsSqrToSeg0 = geomDistances::Distance2SegToPointV(vPoint0,vNodeDir0,vWorldWheelPos);
	const ScalarV fT0 = geomTValues::FindTValueSegToPoint(vPoint0,vNodeDir0,vWorldWheelPos);
	ScalarV fDistsSqrToSeg1;
	ScalarV fT1, fTSeg = fT0;

	if(rRouteInfo.m_bHasTwoSegments)
	{
		vRightFlat1 = NormalizeFast(Cross(vNodeDirNorm1,vZAxis));
		fDistsSqrToSeg1 = geomDistances::Distance2SegToPointV(vPoint1,vNodeDir1,vWorldWheelPos);
		fT1 = geomTValues::FindTValueSegToPoint(vPoint1,vNodeDir1,vWorldWheelPos);
		fTSeg = IsLessThanAll(fDistsSqrToSeg0,fDistsSqrToSeg1) ? fT0 : fT1;
	}

	ProcessRouteConstraintWithPrecalcValues(rRouteInfo,vWorldWheelPos,fTSeg,fT0,fT1,fDistsSqrToSeg0,fDistsSqrToSeg1,vRightFlat0,vRightFlat1,rRouteConstraintOut, bOnlyTestConstraints);
}

void CVirtualRoad::ProcessRouteConstraintWithPrecalcValues(const TRouteInfo & rRouteInfo, Vec3V_In vWorldWheelPos, ScalarV_In fTSeg, ScalarV_In fT0, ScalarV_In fT1, ScalarV_In fDistsSqrToSeg0, ScalarV_In fDistsSqrToSeg1, Vec3V_In vRightFlat0, Vec3V_In vRightFlat1, TRouteConstraint & rRouteConstraintOut, const bool bOnlyTestConstraints)
{
	ASSERT_ONLY(rRouteInfo.Validate());

	// Constants
	const ScalarV fZero(V_ZERO);
	const ScalarV fOne(V_ONE);
	const ScalarV fHalf(V_HALF);
	const BoolV bNeverConstrainIfBeforeOrAfterSegT(!bOnlyTestConstraints);

	//TODO: hysteresis
	const ScalarV fOutsideOfBoundsThresholdOnRoad(V_TWO);
	const ScalarV fOutsideOfBoundsThresholdOffRoad(V_ZERO);
	const ScalarV fOutsideOfBoundsThreshold = rRouteConstraintOut.m_bWasOffroad ? fOutsideOfBoundsThresholdOffRoad : fOutsideOfBoundsThresholdOnRoad;

	const BoolV bAllowConstraintSlackWhenChangingPaths(!bOnlyTestConstraints);	//is it allowed for this query?
	const BoolV bEnableConstraintSlackWhenChangingPaths(ms_bEnableConstraintSlackWhenChangingPaths);	//is it allowed in general?

	// When not constraining, add a buffer to the radius that covers the worst-case motion in a frame.  8m covers more than 250mph for 1/15s (fairly worst case).
	// TODO: Really this should simply flag the constraint to turn off. 
	const ScalarV fVelocityBuffer(V_EIGHT);

	rRouteConstraintOut.m_bJunctionConstraint = false;

#if DEBUG_DRAW && __BANK
	const Vec3V vOffset(0.0f, 0.0f, 0.5f);
#endif

	if(rRouteInfo.m_bHasTwoSegments)
	{
		// When calculating the closest point on the route to the wheel, take into account the radius
		// of the road.  This way a wider road can envelop the wheel even if it isn't the closest.
		const Vec3V vToWheel0 = vWorldWheelPos - rRouteInfo.m_vPoints[1];
		const Vec3V vToWheel1 = vToWheel0;
		const BoolV bOnRight0 = Dot(vToWheel0, vRightFlat0) > fZero;
		const BoolV bOnRight1 = Dot(vToWheel1, vRightFlat1) > fZero;
		const BoolV bAfterHalfOn0 = IsGreaterThan(fT0,fHalf);
		const BoolV bBeforeHalfOn1 = IsGreaterThan(fHalf,fT1);

		const ScalarV fWidthOnLeft0 = rRouteInfo.m_vWidths.GetY();
		const ScalarV fWidthOnRight0 = rRouteInfo.m_vWidths.GetX();
		const ScalarV fWidthOnLeft1 = rRouteInfo.m_vWidths.GetW();
		const ScalarV fWidthOnRight1 = rRouteInfo.m_vWidths.GetZ();

		const ScalarV fWidthDeltaLeft = fWidthOnLeft1 - fWidthOnLeft0;
		const ScalarV fWidthDeltaRight = fWidthOnRight1 - fWidthOnRight0;

		const ScalarV fWidthAtTOnLeft0 = SelectFT(bAfterHalfOn0, fWidthOnLeft0, fWidthOnLeft0 + fWidthDeltaLeft * (fT0 - fHalf));
		const ScalarV fWidthAtTOnRight0 = SelectFT(bAfterHalfOn0, fWidthOnRight0, fWidthOnRight0 + fWidthDeltaRight * (fT0 - fHalf));
		const ScalarV fWidthAtTOnLeft1 = SelectFT(bBeforeHalfOn1, fWidthOnLeft1, fWidthOnLeft1 + fWidthDeltaLeft * (fT1 - fHalf));
		const ScalarV fWidthAtTOnRight1 = SelectFT(bBeforeHalfOn1, fWidthOnRight1, fWidthOnRight1 + fWidthDeltaRight * (fT1 - fHalf));

		const ScalarV fWidthBuffer0 = rRouteInfo.m_vWidthBuffers.GetX();
		const ScalarV fWidthBuffer1 = rRouteInfo.m_vWidthBuffers.GetY();

		const ScalarV fWidthAtTOn0 = SelectFT(bOnRight0, fWidthAtTOnLeft0, fWidthAtTOnRight0) + fWidthBuffer0;
		const ScalarV fWidthAtTOn1 = SelectFT(bOnRight1, fWidthAtTOnLeft1, fWidthAtTOnRight1) + fWidthBuffer1;

		const BoolV bWheelOn0 = (SqrtFast(fDistsSqrToSeg0) - fWidthAtTOn0 < SqrtFast(fDistsSqrToSeg1) - fWidthAtTOn1);

		const Vec3V vClosest = SelectFT(bWheelOn0,
			AddScaled(rRouteInfo.m_vPoints[1],rRouteInfo.m_vNodeDir[1],fT1),
			AddScaled(rRouteInfo.m_vPoints[0],rRouteInfo.m_vNodeDir[0],fT0));

		rRouteConstraintOut.m_vConstraintPosOnRoute = vClosest;

		// If we are before the start of T0 or beyond the end of T1, then don't constrain the wheel.  This is the 
		// situation where we haven't even reached the start of the path, or are beyond it, as provided to us by AI.
		const ScalarV fConstraintRadiusFromRoadWidth = SelectFT(bWheelOn0,fWidthAtTOn1,fWidthAtTOn0);
		const ScalarV fNonConstraintRadius = Max(Dist(vWorldWheelPos,vClosest) + fVelocityBuffer, fConstraintRadiusFromRoadWidth);
		const BoolV bBeforeSeg0 = (fDistsSqrToSeg0 < fDistsSqrToSeg1) & (fT0 == fZero);
		const BoolV bBeyondSeg1 = (fDistsSqrToSeg0 > fDistsSqrToSeg1) & (fT1 == fOne);
		const BoolV bOutsideOfSegLengthwise = bBeforeSeg0 | bBeyondSeg1;
		const ScalarV fLateralDistance0 = Abs(Dot(vToWheel0, vRightFlat0));
		const ScalarV fLateralDistance1 = Abs(Dot(vToWheel1, vRightFlat1));
		const BoolV bWithinExtentWidthOn0 = fLateralDistance0 < fWidthAtTOn0;
		const BoolV bWithinExtentWidthOn1 = fLateralDistance1 < fWidthAtTOn1;

		const BoolV bOutsideSeg0ByMargin = fLateralDistance0 > fWidthAtTOn0 + fOutsideOfBoundsThreshold;
		const BoolV bOutsideSeg1ByMargin = fLateralDistance1 > fWidthAtTOn1 + fOutsideOfBoundsThreshold;
		const BoolV bOutsideOfEitherSegWidthwiseByMoreThanMargin = bOutsideSeg0ByMargin | bOutsideSeg1ByMargin;

		if (ms_bEnableConstraintSlackWhenChangingPaths && bAllowConstraintSlackWhenChangingPaths.Getb())
		{
			rRouteConstraintOut.m_bWasOffroad = bOutsideOfEitherSegWidthwiseByMoreThanMargin.Getb();
		}

		const BoolV bDontConstrain = (bOutsideOfSegLengthwise & (bNeverConstrainIfBeforeOrAfterSegT | (bWithinExtentWidthOn0 & bWithinExtentWidthOn1)))
			| (bEnableConstraintSlackWhenChangingPaths & bOutsideOfEitherSegWidthwiseByMoreThanMargin & bAllowConstraintSlackWhenChangingPaths);
		const ScalarV fConstraintRadius = SelectFT(bDontConstrain,fConstraintRadiusFromRoadWidth,fNonConstraintRadius);
		rRouteConstraintOut.m_fConstraintRadiusOnRoute = fConstraintRadius;

#if DEBUG_DRAW && __BANK
		if(CVehicleAILodManager::ms_bDisplayDummyPathConstraint)
		{
			const Vec3V vRight = SelectFT(bWheelOn0, vRightFlat1, vRightFlat0);

			const ScalarV fWidthOnLeft = SelectFT(bWheelOn0, fWidthAtTOnLeft1, fWidthAtTOnLeft0);
			const ScalarV fWidthOnRight = SelectFT(bWheelOn0, fWidthAtTOnRight1, fWidthAtTOnRight0);

			const ScalarV fWidthBuffer = SelectFT(bWheelOn0, fWidthBuffer1, fWidthBuffer0);
			const ScalarV fBufferedWidthOnLeft = fWidthOnLeft + fWidthBuffer;
			const ScalarV fBufferedWidthOnRight = fWidthOnRight + fWidthBuffer;
			
			const BoolV bOnRight = IsTrue(bWheelOn0) ? bOnRight0 : bOnRight1;

			const Vec3V vLeftExtent = rRouteConstraintOut.m_vConstraintPosOnRoute + (-vRight * fWidthOnLeft);
			const Vec3V vBufferedLeftExtent = rRouteConstraintOut.m_vConstraintPosOnRoute + (-vRight * fBufferedWidthOnLeft);

			grcDebugDraw::Line(rRouteConstraintOut.m_vConstraintPosOnRoute + vOffset, vLeftExtent + vOffset, Color_magenta, Color_magenta, -1);
			grcDebugDraw::Line(vLeftExtent + vOffset, vBufferedLeftExtent + vOffset, Color_pink, Color_pink, -1);

			const Vec3V vRightExtent = rRouteConstraintOut.m_vConstraintPosOnRoute + (vRight * fWidthOnRight);
			const Vec3V vBufferedRightExtent = rRouteConstraintOut.m_vConstraintPosOnRoute + (vRight * fBufferedWidthOnRight);
			
			grcDebugDraw::Line(rRouteConstraintOut.m_vConstraintPosOnRoute + vOffset, vRightExtent + vOffset, Color_purple, Color_purple, -1);
			grcDebugDraw::Line(vRightExtent + vOffset, vBufferedRightExtent + vOffset, Color_pink, Color_pink, -1);

			if(IsTrue(And(bOnRight, rRouteConstraintOut.m_fConstraintRadiusOnRoute > fBufferedWidthOnRight)))
			{
				const Vec3V vExtendedRightExtent = vBufferedRightExtent + (vRight * (rRouteConstraintOut.m_fConstraintRadiusOnRoute - fBufferedWidthOnRight));
				grcDebugDraw::Line(vBufferedRightExtent + vOffset, vExtendedRightExtent + vOffset, Color_white, Color_white, -1);
			}
			else if(IsTrue(And(!bOnRight, rRouteConstraintOut.m_fConstraintRadiusOnRoute > fBufferedWidthOnLeft)))
			{
				const Vec3V vExtendedLeftExtent = vBufferedLeftExtent + (-vRight * (rRouteConstraintOut.m_fConstraintRadiusOnRoute - fBufferedWidthOnLeft));
				grcDebugDraw::Line(vBufferedLeftExtent + vOffset, vExtendedLeftExtent + vOffset, Color_white, Color_white, -1);
			}

			if(IsTrue(bWheelOn0))
			{
				grcDebugDraw::Sphere(rRouteInfo.m_vPoints[0] + vOffset, 0.5f, Color_green, true, -1);
				grcDebugDraw::Line(vWorldWheelPos + vOffset, rRouteInfo.m_vPoints[0] + vOffset, Color_green, Color_green, -1);
			}
			else
			{
				grcDebugDraw::Sphere(rRouteInfo.m_vPoints[1] + vOffset, 0.5f, Color_green, true, -1);
				grcDebugDraw::Line(vWorldWheelPos + vOffset, rRouteInfo.m_vPoints[1] + vOffset, Color_green, Color_green, -1);
			}

			grcDebugDraw::Line(rRouteInfo.m_vPoints[1] + vOffset, rRouteInfo.m_vPoints[1] + rRouteInfo.m_vNodeDir[1] + vOffset, Color_cyan, Color_cyan, -1);
		}
#endif // DEBUG_DRAW && __BANK

// 		if (ms_bEnableConstraintSlackWhenChangingPaths && bOutsideOfEitherSegWidthwiseByMoreThanMargin.Getb() && bAllowConstraintSlackWhenChangingPaths.Getb())
// 		{
// 			grcDebugDraw::Line(vWorldWheelPos, vWorldWheelPos + Vec3V(0.0f, 0.0f, 5.0f), Color_red, -1);
// 		}
	}
	else
	{
		const Vec3V vClosest = AddScaled(rRouteInfo.m_vPoints[0],rRouteInfo.m_vNodeDir[0],fTSeg);
		rRouteConstraintOut.m_vConstraintPosOnRoute = vClosest;

		// Similar to above, if we are before the start of the segment or beyond the end of the segment,
		// then create a long constraint radius to not constrain the vehicle.
		BoolV bOffSegment = (fTSeg == fZero) | (fTSeg == fOne);
		// TODO: May want to distinguish left/right here, but at least it isn't arbitrarily picking left like previously.
		const ScalarV fWidthRight = rRouteInfo.m_vWidths.GetX() + rRouteInfo.m_vWidthBuffers.GetX();
		const ScalarV fWidthLeft = rRouteInfo.m_vWidths.GetY() + rRouteInfo.m_vWidthBuffers.GetX();
		const ScalarV fConstraintRadius = Max(fWidthLeft,fWidthRight);
		const ScalarV fNonConstraintRadius = Max(Dist(vWorldWheelPos,vClosest) + fVelocityBuffer, fConstraintRadius);

		const Vec3V vToWheel = vWorldWheelPos - rRouteInfo.m_vPoints[0];
		const ScalarV fLateralDistance = Abs(Dot(vToWheel, vRightFlat0));
		const BoolV bWithinExtentWidth = fLateralDistance < fConstraintRadius;
		const BoolV bOutsideOfSegWidthwiseByMoreThanMargin = fLateralDistance > fConstraintRadius + fOutsideOfBoundsThreshold;
		const BoolV bDontConstrain = (bOffSegment & (bWithinExtentWidth | bNeverConstrainIfBeforeOrAfterSegT)) 
			| (bEnableConstraintSlackWhenChangingPaths & bOutsideOfSegWidthwiseByMoreThanMargin & bAllowConstraintSlackWhenChangingPaths);
		rRouteConstraintOut.m_fConstraintRadiusOnRoute = SelectFT(bDontConstrain,fConstraintRadius,fNonConstraintRadius);

		if (ms_bEnableConstraintSlackWhenChangingPaths && bAllowConstraintSlackWhenChangingPaths.Getb())
		{
			rRouteConstraintOut.m_bWasOffroad = bOutsideOfSegWidthwiseByMoreThanMargin.Getb();
		}

#if DEBUG_DRAW && __BANK
		if(CVehicleAILodManager::ms_bDisplayDummyPathConstraint)
		{
			const Vec3V vRight = vRightFlat0;
			const ScalarV fWidthOnLeft = rRouteInfo.m_vWidths.GetY();
			const ScalarV fWidthOnRight = rRouteInfo.m_vWidths.GetX();
			const ScalarV fWidthBuffer = rRouteInfo.m_vWidthBuffers.GetX();

			const Vec3V vLeftExtent = rRouteConstraintOut.m_vConstraintPosOnRoute + (-vRight * fWidthOnLeft);
			const Vec3V vBufferedLeftExtent = vLeftExtent + (-vRight * fWidthBuffer);

			grcDebugDraw::Line(rRouteConstraintOut.m_vConstraintPosOnRoute + vOffset, vLeftExtent + vOffset, Color_magenta, Color_magenta, -1);
			grcDebugDraw::Line(vLeftExtent + vOffset, vBufferedLeftExtent + vOffset, Color_pink, Color_pink, -1);

			const Vec3V vRightExtent = rRouteConstraintOut.m_vConstraintPosOnRoute + (vRight * fWidthOnRight);
			const Vec3V vBufferedRightExtent = vRightExtent + (vRight * fWidthBuffer);
			
			grcDebugDraw::Line(rRouteConstraintOut.m_vConstraintPosOnRoute + vOffset, vRightExtent + vOffset, Color_purple, Color_purple, -1);
			grcDebugDraw::Line(vRightExtent + vOffset, vBufferedRightExtent + vOffset, Color_pink, Color_pink, -1);

			if(IsTrue(rRouteConstraintOut.m_fConstraintRadiusOnRoute > fConstraintRadius))
			{
				const ScalarV vExtension = rRouteConstraintOut.m_fConstraintRadiusOnRoute - fConstraintRadius;

				const Vec3V vExtendedRightExtent = vBufferedRightExtent + (vRight * vExtension);
				grcDebugDraw::Line(vBufferedRightExtent + vOffset, vExtendedRightExtent + vOffset, Color_white, Color_white, -1);

				const Vec3V vExtendedLeftExtent = vBufferedLeftExtent + (-vRight * vExtension);
				grcDebugDraw::Line(vBufferedLeftExtent + vOffset, vExtendedLeftExtent + vOffset, Color_white, Color_white, -1);
			}

			grcDebugDraw::Sphere(rRouteInfo.m_vPoints[0] + vOffset, 0.5f, Color_green, true, -1);
			grcDebugDraw::Line(vWorldWheelPos + vOffset, rRouteInfo.m_vPoints[0] + vOffset, Color_green, Color_green, -1);
		}
#endif // DEBUG_DRAW && __BANK

// 		if (ms_bEnableConstraintSlackWhenChangingPaths && bOutsideOfSegWidthwiseByMoreThanMargin.Getb() && bAllowConstraintSlackWhenChangingPaths.Getb())
// 		{
// 			grcDebugDraw::Line(vWorldWheelPos, vWorldWheelPos + Vec3V(0.0f, 0.0f, 5.0f), Color_red, -1);
// 		}
	}

#if DEBUG_DRAW && __BANK
	if(CVehicleAILodManager::ms_bDisplayDummyPathConstraint)
	{
		grcDebugDraw::Line(rRouteInfo.m_vPoints[0] + vOffset, rRouteInfo.m_vPoints[0] + rRouteInfo.m_vNodeDir[0] + vOffset, Color_red, Color_red, -1);
	}
#endif // DEBUG_DRAW && __BANK
}

void CVirtualRoad::DebugDrawJunctions(Vec3V_In /*vDrawCentre*/)
{
	for (int i = 0; i < JUNCTIONS_MAX_NUMBER; i++)
	{
		CJunction* pJunction = CJunctions::GetJunctionByIndex(i);
		if (!pJunction)
		{
			continue;
		}
		const CNodeAddress& jnNode = pJunction->GetJunctionNode(0);

		if( pJunction->GetNumJunctionNodes() > 0 &&
			!jnNode.IsEmpty() &&
			ThePaths.IsRegionLoaded(jnNode))
		{
			const int iRegion = jnNode.GetRegion();

			const int iVirtJunction = ThePaths.apRegions[iRegion]->JunctionMap.JunctionMap[pJunction->GetJunctionNode(0).RegionAndIndex()];
			CVirtualJunctionInterface jnInterface(&ThePaths.apRegions[iRegion]->aVirtualJunctions[iVirtJunction], ThePaths.apRegions[iRegion]);

			jnInterface.DebugDraw();
		}
	}
}

// const float CVirtualJunction::ms_fRangeUp = 1.0f;
// 
// /////////////////////////////////////////////////////////////////////////////////
// // CVirtualJunction
// 
// CVirtualJunction::CVirtualJunction()
// {
// 	m_vMin = Vec3V(V_ZERO);
// 	m_vMax = Vec3V(V_ZERO);
// 	m_fGridSpace = 0.0f;
// 	m_nXSamples = 0;
// 	m_nYSamples = 0;
// 	m_bHasSampled = false;
// 	m_fHeights = NULL;
// 	
// 	m_nHeightBaseWorld = 0;
// 	m_nHeightOffsets = NULL;
// }
// 
// CVirtualJunction::~CVirtualJunction()
// {
// 	delete [] m_fHeights;
// 	delete [] m_nHeightOffsets;
// }
// 
// void CVirtualJunction::Init(Vec3V_In vMin, Vec3V_In vMax, float fGridSpace)
// {
// 	m_vMin = vMin;
// 	m_vMax = vMax;
// 	m_fGridSpace = fGridSpace;
// 
// 	const ScalarV scGridSpace = LoadScalar32IntoScalarV(m_fGridSpace);
// 
// 	//ensure a minimum of two samples along each axis
// 	const int iNumXSamples = rage::Max(2, (int)(((m_vMax.GetX()-m_vMin.GetX())/scGridSpace).Getf()) + 1);
// 	const int iNumYSamples = rage::Max(2, (int)(((m_vMax.GetY()-m_vMin.GetY())/scGridSpace).Getf()) + 1);
// 	Assert(iNumXSamples < MAX_UINT8);
// 	Assert(iNumYSamples < MAX_UINT8);
// 	m_nXSamples = (u8)iNumXSamples;
// 	m_nYSamples = (u8)iNumYSamples;
// 	m_fHeights = rage_new float[(m_nXSamples * m_nYSamples + 3)];
// 	m_nHeightOffsets = rage_new u8[(m_nXSamples * m_nYSamples + 3)];
// 	m_bHasSampled = false;
// }
// 
// float CVirtualJunction::FindNearestGroundHeightForVirtualJunction(const Vector3 &posn, float rangeUp, float rangeDown)
// {
// 	static bool bUseTopHitBelowThreshold = true;
// 
// 	phIntersection	result;
// 	float	heightFound = posn.z;
// 	bool	bHitFound = false;
// 
// 	phSegment testSegment(Vector3(posn.x, posn.y, posn.z + rangeUp), Vector3(posn.x, posn.y, posn.z - rangeDown) );
// 	CPhysics::GetLevel()->TestProbe(testSegment, &result, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES);
// 	static dev_float s_fAboveThresholdForPathProves = 0.5f;
// 
// 	while (result.IsAHit())
// 	{
// 		if(bUseTopHitBelowThreshold && (result.GetPosition().GetZf() - posn.z < s_fAboveThresholdForPathProves) && (result.GetPosition().GetZf() - posn.z > 0.0f))
// 		{
// 			heightFound = result.GetPosition().GetZf();
// 			return heightFound;
// 		}
// 		else if (!bHitFound)
// 		{
// 			heightFound = result.GetPosition().GetZf();
// 		}
// 		else
// 		{
// 			if (rage::Abs(result.GetPosition().GetZf() - posn.z) < rage::Abs(heightFound - posn.z))
// 			{
// 				heightFound = result.GetPosition().GetZf();
// 			}
// 			else
// 			{
// 				return heightFound;
// 			}
// 		}
// 		bHitFound = true;
// 
// 		//phSegment testSegment2(Vector3(posn.x, posn.y, heightFound - 0.05f), Vector3(posn.x, posn.y, posn.z - rangeDown) );
// 		//CPhysics::GetLevel()->TestProbe(testSegment2, &result, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES);
// 
// 		WorldProbe::CShapeTestProbeDesc probeData;
// 		probeData.SetStartAndEnd(Vector3(posn.x, posn.y, heightFound - 0.05f),Vector3(posn.x, posn.y, posn.z - rangeDown));
// 		probeData.SetContext(WorldProbe::ENotSpecified);
// 		probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
// 		probeData.SetIsDirected(true);
// 
// 		WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
// 	}
// 
// 	return heightFound;
// }
// 
// bool CVirtualJunction::SampleFromWorld(ScalarV_In fStartHeight)
// {
// 	bool bAllSamplesFound = true;
// 	float fRangeDown = 8.0f;
// 	float fStartHeightAsFloat = fStartHeight.Getf();
// 
// 	Assertf(m_vMax.GetZf() - m_vMin.GetZf() < fRangeDown, "Junction at (%.2f, %.2f, %.2f) has height delta greater than max allowed(%.2f)"
// 		, Average(m_vMin, m_vMax).GetXf(), Average(m_vMin, m_vMax).GetYf(), Average(m_vMin, m_vMax).GetZf(), fRangeDown);
// 
// 	const ScalarV scGridSpace = LoadScalar32IntoScalarV(m_fGridSpace);
// 
// 	float fMinHeight = FLT_MAX;
// 	ScalarV fX = m_vMin.GetX();
// 	for(int iX=0; iX<m_nXSamples; iX++)
// 	{
// 		ScalarV fY = m_vMin.GetY();
// 		for(int iY=0; iY<m_nYSamples; iY++)
// 		{
// 			Vec3V vSamplePos(fX,fY,fStartHeight);
// 			float fHitHeightAsFloat = FindNearestGroundHeightForVirtualJunction(RCC_VECTOR3(vSamplePos), ms_fRangeUp, fRangeDown);
// 			if(fHitHeightAsFloat==fStartHeightAsFloat)
// 			{
// 				// TODO: Getting the exact height back probably means that the probe missed somehow.  Worth investigating? 
// 				bAllSamplesFound = false;
// 			}
// 
// 			SetHeight(iX,iY,fHitHeightAsFloat);
// 			fMinHeight = rage::Min(fMinHeight, fHitHeightAsFloat);
// 
// 			fY = fY + scGridSpace;
// 		}
// 		fX = fX + scGridSpace;
// 	}
// 
// 	m_nHeightBaseWorld = COORSZ_TO_UINT16(fMinHeight);
// 
// 	//iterate through again, and set the offset
// 	const float fStep = ((m_vMax.GetZf() + ms_fRangeUp) - fMinHeight) / 256.0f;
// 	for(int iX=0; iX<m_nXSamples; iX++)
// 	{
// 		for(int iY=0; iY<m_nYSamples; iY++)
// 		{
// 			int iIndex = iX + iY*m_nXSamples;
// 			m_nHeightOffsets[iIndex] = (u8)((m_fHeights[iIndex] - fMinHeight) / fStep);
// 		}
// 	}
// 
// 	// Not necessarily true
// 	m_bHasSampled = true;
// 
// 	return bAllSamplesFound;
// }
// 
// void CVirtualJunction::SampleHeightfield(Vec3V_In vTestPos, Vec3V_InOut vPos_Out, Vec3V_InOut vNorm_Out) const
// {
// 	const ScalarV scGridSpace = LoadScalar32IntoScalarV(m_fGridSpace);
// 	const ScalarV fXGrid = (vTestPos.GetX() - m_vMin.GetX()) / scGridSpace;
// 	const ScalarV fYGrid = (vTestPos.GetY() - m_vMin.GetY()) / scGridSpace;
// 	const ScalarV fXGridInt = Clamp(RoundToNearestIntNegInf(fXGrid),ScalarV(V_ZERO),ScalarV((float)(m_nXSamples-2)));
// 	const ScalarV fYGridInt = Clamp(RoundToNearestIntNegInf(fYGrid),ScalarV(V_ZERO),ScalarV((float)(m_nYSamples-2)));
// 	const ScalarV fXGridRemainder = fXGrid - fYGridInt;
// 	const ScalarV fYGridRemainder = fYGrid - fYGridInt;
// 
// 	// Find the sub-cell triangle (by projecting down)
// 	Vec3V vA, vB, vC;
// 	{
// 		int iX = (int)fXGridInt.Getf(); // Already clamped
// 		Assert(iX>=0 && iX<=m_nXSamples-2);
// 		int iY = (int)fYGridInt.Getf(); // Already clamped
// 		Assert(iY>=0 && iY<=m_nYSamples-2);
// 		const ScalarV fX = m_vMin.GetX() + fXGridInt * scGridSpace;
// 		const ScalarV fY = m_vMin.GetY() + fYGridInt * scGridSpace;
// 
// 		int iXA, iYA, iXB, iYB, iXC, iYC;
// 		ScalarV fXA, fYA, fXB, fYB, fXC, fYC;
// 
// 		if(IsLessThanAll(fXGridRemainder+fYGridRemainder,ScalarV(V_ONE)))
// 		{
// 			iXA = iX,				iYA = iY;
// 			fXA = fX,				fYA = fY;
// 			iXB = iX,				iYB = iY + 1;
// 			fXB = fX,				fYB = fY + scGridSpace;
// 			iXC = iX + 1,			iYC = iY;
// 			fXC = fX + scGridSpace,fYC = fY;
// 		}
// 		else
// 		{
// 			iXA = iX,				iYA = iY + 1;
// 			fXA = fX,				fYA = fY + scGridSpace;
// 			iXB = iX + 1,			iYB = iY + 1;
// 			fXB = fX + scGridSpace,fYB = fY + scGridSpace;
// 			iXC = iX + 1,			iYC = iY;
// 			fXC = fX + scGridSpace,fYC = fY;
// 		}
// 		const ScalarV fHeightA = GetHeight(iXA,iYA);
// 		vA = Vec3V(fXA,fYA,fHeightA);
// 		const ScalarV fHeightB = GetHeight(iXB,iYB);
// 		vB = Vec3V(fXB,fYB,fHeightB);
// 		const ScalarV fHeightC = GetHeight(iXC,iYC);
// 		vC = Vec3V(fXC,fYC,fHeightC);
// 	}
// 
// #if __BANK
// 	if(CVehicleAILodManager::ms_bRaiseDummyJunctions)
// 	{
// 		const ScalarV fHeight(CVehicleAILodManager::ms_fRaiseDummyRoadHeight);
// 		vA.SetZ(vA.GetZ()+fHeight);
// 		vB.SetZ(vB.GetZ()+fHeight);
// 		vC.SetZ(vC.GetZ()+fHeight);
// 	}
// #endif
// 
// 	const Vec3V vPerp = Cross(vC-vA,vB-vA);
// 	vNorm_Out = NormalizeFast(vPerp);
// 	vPos_Out = vTestPos - vNorm_Out * Dot(vNorm_Out,vTestPos-vA);
// 
// #if DEBUG_DRAW
// 	if (CVehicleAILodManager::ms_bDisplayVirtualJunctions)
// 	{
// 		grcDebugDraw::Sphere(vA,0.15f,Color_red,true,-1);
// 		grcDebugDraw::Sphere(vB,0.15f,Color_blue,true,-1);
// 		grcDebugDraw::Sphere(vC,0.15f,Color_green,true,-1);
// 		grcDebugDraw::Sphere(vPos_Out,0.15f,Color_white,true,-1);
// 		grcDebugDraw::Line(vPos_Out,vPos_Out+ScalarV(V_THREE)*vNorm_Out,Color_white,-1);
// 	}
// #endif
// }
// 
// void CVirtualJunction::ProcessCollision(Vec3V_In vWorldWheelPos, CVirtualRoad::TWheelCollision & rWheelCollisionOut, CVirtualRoad::TRouteConstraint * pRouteConstraintOut) const
// {
// 	Vec3V vHitPos, vHitNormal;
// 	SampleHeightfield(vWorldWheelPos,vHitPos,vHitNormal);
// 	ScalarV fHeightAboveRoad = Dot(vHitNormal,vWorldWheelPos-vHitPos);
// 	rWheelCollisionOut.m_vPosition = Vec4V(vHitPos,fHeightAboveRoad);
// 	rWheelCollisionOut.m_vNormal = vHitNormal;
// 	if(pRouteConstraintOut)
// 	{
// 		// TODO: Fake constraint for now
// 		pRouteConstraintOut->m_fConstraintRadiusOnRoute = ScalarV(100.0f);
// 		pRouteConstraintOut->m_vConstraintPosOnRoute = Average(m_vMin,m_vMax);
// 	}
// }
// 
// void CVirtualJunction::DebugDraw() const
// {
// #if DEBUG_DRAW
// 
// 	if (!m_bHasSampled)
// 	{
// 		return;
// 	}
// 
// 	const ScalarV scGridSpace = LoadScalar32IntoScalarV(m_fGridSpace);
// 	ScalarV fX = m_vMin.GetX();
// 	for(int iX=0; iX<m_nXSamples; iX++)
// 	{
// 		ScalarV fY = m_vMin.GetY();
// 		for(int iY=0; iY<m_nYSamples; iY++)
// 		{
// 			//int iIndex = iX + iY * m_nXSamples;
// 			//int iIndexVec = iIndex / 4;
// 			//int iIndexElem = iIndex % 4;
// 			//const Vec4V fHeightAsVec = m_fHeights[iIndexVec];
// 			//ScalarV fHeight = LoadScalar32IntoScalarV(m_fHeights[iIndex]);
// 			ScalarV fHeight = GetHeight(iX, iY);
// // 			switch(iIndexElem)
// // 			{
// // 				case 0:	fHeight = fHeightAsVec.GetX(); break;
// // 				case 1:	fHeight = fHeightAsVec.GetY(); break;
// // 				case 2:	fHeight = fHeightAsVec.GetZ(); break;
// // 				case 3: fHeight = fHeightAsVec.GetW(); break;
// // 				default: break; // shouldn't happen
// // 			}
// 
// 			const Vec3V vSamplePos(fX,fY,fHeight);
// 			Color32 drawColor = Color_coral;
// 			mthRandom rnd(m_vMax.GetZi());	//seed based on height
// 			drawColor = (Color32)rnd.GetInt();
// 			drawColor.SetAlpha(255);
// 			grcDebugDraw::Line(vSamplePos,vSamplePos+Vec3V(V_Z_AXIS_WZERO),drawColor);
// 
// 			fY = fY + scGridSpace;
// 		}
// 		fX = fX + scGridSpace;
// 	}
// #endif
// }

void CVirtualJunctionInterface::InitAndSample(const CJunction* pRealJunction, float fGridSpace, const int iStartIndexOfHeightSamples
											  , const CPathFind& paths)
{
	if (!aiVerify(pRealJunction))
	{
		return;
	}

	const Vec3V vJunctionCenter = VECTOR3_TO_VEC3V(pRealJunction->GetJunctionCenter());
	Vec3V vMin = vJunctionCenter;	
	Vec3V vMax = vJunctionCenter;

	const int iNumEntrances = pRealJunction->GetNumEntrances();
	const bool bUseNewCodePath = pRealJunction->GetNumJunctionNodes() == 1;

	if (iNumEntrances > 0)
	{
		vMin.SetZf(FLT_MAX);
		vMax.SetZf(-FLT_MAX);
	}

	for (int i = 0; i < iNumEntrances; i++)
	{
		const CJunctionEntrance& pEntrance = pRealJunction->GetEntrance(i);

		//no reason this code shouldn't also work for junctions with >1 junction nodes
		//but too much work to properly implement it now
		if (bUseNewCodePath)
		{
			const CNodeAddress& rEntryNode = pEntrance.m_Node;
			Vector3 vRoadExtremeLeft, vRoadExtremeRight;
			float nodeAWidthScalingFactor;

			CNodeAddress PreEntranceNode = EmptyNodeAddress;
			const CPathNode* pEntryNode = paths.FindNodePointerSafe(rEntryNode);
			if (pEntryNode)
			{
				for (int j = 0; j < pEntryNode->NumLinks(); j++)
				{
					const CNodeAddress& otherNode = paths.GetNodesLinkedNodeAddr(pEntryNode, j);
					if (otherNode == pRealJunction->GetJunctionNode(0))
					{
						continue;
					}

					const CPathNodeLink* pOtherLink = &paths.GetNodesLink(pEntryNode, j);

					if (pOtherLink->IsShortCut())
					{
						continue;
					}

					PreEntranceNode = otherNode;
					break;
				}
			}

			if (PreEntranceNode != EmptyNodeAddress)
			{
				paths.IdentifyDrivableExtremes(PreEntranceNode, rEntryNode
					, EmptyNodeAddress, false, 0.0f, vRoadExtremeLeft, vRoadExtremeRight, nodeAWidthScalingFactor);
			}
			else
			{
				paths.IdentifyDrivableExtremes(pRealJunction->GetJunctionNode(0), rEntryNode
					, EmptyNodeAddress, false, 0.0f, vRoadExtremeLeft, vRoadExtremeRight, nodeAWidthScalingFactor);
			}

			const Vec3V vRoadExtremeLeftVec(vRoadExtremeLeft);
			const Vec3V vRoadExtremeRightVec(vRoadExtremeRight);

			Vec3V vEntrancePos = VECTOR3_TO_VEC3V(pEntrance.m_vPositionOfNode);
			vMax.SetX(rage::Max(vMax.GetX(), vEntrancePos.GetX()));
			vMin.SetX(rage::Min(vMin.GetX(), vEntrancePos.GetX()));
			vMax.SetY(rage::Max(vMax.GetY(), vEntrancePos.GetY()));
			vMin.SetY(rage::Min(vMin.GetY(), vEntrancePos.GetY()));
			vMax.SetZ(rage::Max(vMax.GetZ(), vEntrancePos.GetZ()));
			vMin.SetZ(rage::Min(vMin.GetZ(), vEntrancePos.GetZ()));

			vMax.SetX(rage::Max(vMax.GetX(), vRoadExtremeLeftVec.GetX()));
			vMin.SetX(rage::Min(vMin.GetX(), vRoadExtremeLeftVec.GetX()));
			vMax.SetY(rage::Max(vMax.GetY(), vRoadExtremeLeftVec.GetY()));
			vMin.SetY(rage::Min(vMin.GetY(), vRoadExtremeLeftVec.GetY()));
			vMax.SetZ(rage::Max(vMax.GetZ(), vRoadExtremeLeftVec.GetZ()));
			vMin.SetZ(rage::Min(vMin.GetZ(), vRoadExtremeLeftVec.GetZ()));

			vMax.SetX(rage::Max(vMax.GetX(), vRoadExtremeRightVec.GetX()));
			vMin.SetX(rage::Min(vMin.GetX(), vRoadExtremeRightVec.GetX()));
			vMax.SetY(rage::Max(vMax.GetY(), vRoadExtremeRightVec.GetY()));
			vMin.SetY(rage::Min(vMin.GetY(), vRoadExtremeRightVec.GetY()));
			vMax.SetZ(rage::Max(vMax.GetZ(), vRoadExtremeRightVec.GetZ()));
			vMin.SetZ(rage::Min(vMin.GetZ(), vRoadExtremeRightVec.GetZ()));

#if __BANK
			static dev_bool s_bDrawSpheres = false;
			if (s_bDrawSpheres)
			{
				grcDebugDraw::Sphere(vRoadExtremeLeft, 0.25f, Color_red, false, -60);
				grcDebugDraw::Sphere(vRoadExtremeRight, 0.25f, Color_orange, false, -60);
			}
#endif //__BANK
		}
		else
		{
			Vec3V vEntrancePos = VECTOR3_TO_VEC3V(pEntrance.m_vPositionOfNode);
			vMax.SetX(rage::Max(vMax.GetX(), vEntrancePos.GetX()));
			vMin.SetX(rage::Min(vMin.GetX(), vEntrancePos.GetX()));
			vMax.SetY(rage::Max(vMax.GetY(), vEntrancePos.GetY()));
			vMin.SetY(rage::Min(vMin.GetY(), vEntrancePos.GetY()));
			vMax.SetZ(rage::Max(vMax.GetZ(), vEntrancePos.GetZ()));
			vMin.SetZ(rage::Min(vMin.GetZ(), vEntrancePos.GetZ()));
		}
	}

	if (!bUseNewCodePath)
	{
		const ScalarV fDistFromCenterMinX = Abs(vJunctionCenter.GetX() - vMin.GetX());
		const ScalarV fDistFromCenterMinY = Abs(vJunctionCenter.GetY() - vMin.GetY());
		const ScalarV fDistFromCenterMaxX = Abs(vMax.GetX() - vJunctionCenter.GetX());
		const ScalarV fDistFromCenterMaxY = Abs(vMax.GetY() - vJunctionCenter.GetY());
		const ScalarV fXExtent = rage::Max(fDistFromCenterMinX, fDistFromCenterMaxX);
		const ScalarV fYExtent = rage::Max(fDistFromCenterMinY, fDistFromCenterMaxY);
		vMin.SetX(vJunctionCenter.GetX() - fXExtent);
		vMin.SetY(vJunctionCenter.GetY() - fYExtent);
		vMax.SetX(vJunctionCenter.GetX() + fXExtent);
		vMax.SetY(vJunctionCenter.GetY() + fYExtent);
	}

	SetMinX(vMin.GetX());
	SetMinY(vMin.GetY());
	//m_pJunction->m_fGridSpace = fGridSpace;
	m_pJunction->m_nStartIndexOfHeightSamples = (s16)iStartIndexOfHeightSamples;

	const ScalarV scGridSpace = LoadScalar32IntoScalarV(fGridSpace);

	//ensure a minimum of two samples along each axis
	const int iNumXSamples = rage::Max(2, (int)(((vMax.GetX()-vMin.GetX())/scGridSpace).Getf()) + 1);
	const int iNumYSamples = rage::Max(2, (int)(((vMax.GetY()-vMin.GetY())/scGridSpace).Getf()) + 1);
	Assert(iNumXSamples < MAX_UINT8);
	Assert(iNumYSamples < MAX_UINT8);
	m_pJunction->m_nXSamples = (u8)iNumXSamples;
	m_pJunction->m_nYSamples = (u8)iNumYSamples;
	//m_pJunction->m_nHeightOffsets = rage_new u8[(iNumXSamples * iNumYSamples + 3)];
	
	//now sample from the world
	const Vec3V vMinQuantized(GetMinX(), GetMinY(), vMin.GetZ());
	SampleFromWorld(vMax.GetZ(), vMinQuantized, vMax);
}

 bool CVirtualJunctionInterface::SampleFromWorld(ScalarV_In fStartHeight, Vec3V_In vMin, Vec3V_In vMax)
 {
 	bool bAllSamplesFound = true;
 	float fRangeDown = 10.0f;
 	float fStartHeightAsFloat = fStartHeight.Getf();
	const float fMinExpectedHeight = vMin.GetZf();
	const float fMaxExpectedHeight = vMax.GetZf();
 
	if (vMax.GetZf() - vMin.GetZf() > fRangeDown)
	{
		Displayf("Junction at (%.2f, %.2f, %.2f) has height delta greater than max allowed(%.2f)"
			, Average(vMin, vMax).GetXf(), Average(vMin, vMax).GetYf(), Average(vMin, vMax).GetZf(), fRangeDown);
	}
 
	//hardcoded as two, but we can bring this back as a per-junction define
	static const ScalarV scGridSpace(CPathVirtualJunction::ms_fGridSpace);
 
	const int iNumSamples = (m_pJunction->m_nXSamples * m_pJunction->m_nYSamples);
 	float* fHeights = Alloca(float, iNumSamples);
 
 	float fMinHeight = FLT_MAX;
	float fMaxHeight = -FLT_MAX;
 	ScalarV fX = vMin.GetX();
 	for(int iX=0; iX<m_pJunction->m_nXSamples; iX++)
 	{
 		ScalarV fY = vMin.GetY();
 		for(int iY=0; iY<m_pJunction->m_nYSamples; iY++)
 		{
 			const int iIndex = iX + iY * m_pJunction->m_nXSamples;
 			Vec3V vSamplePos(fX,fY,fStartHeight);
			float fHitHeightAsFloat = FindNearestGroundHeightForVirtualJunction(RCC_VECTOR3(vSamplePos), CPathVirtualJunction::ms_fRangeUp, fRangeDown, fMinExpectedHeight, fMaxExpectedHeight);
			const float fHitHeightJittered = FindNearestGroundHeightForVirtualJunctionWithJittering(RCC_VECTOR3(vSamplePos), CPathVirtualJunction::ms_fRangeUp, fRangeDown, fMinExpectedHeight, fMaxExpectedHeight);
			static dev_float s_fAllowableHeightDiff = 0.5f;

			//use the jittered value if the original probe either misses or is significantly higher
			//than the jittered probe--it might have hit some really skinny piece of geometry
 			if(fHitHeightAsFloat==fStartHeightAsFloat || fHitHeightAsFloat > fHitHeightJittered + s_fAllowableHeightDiff)
 			{
				fHitHeightAsFloat = fHitHeightJittered;
				if (fHitHeightAsFloat==fStartHeightAsFloat)
				{
 					bAllSamplesFound = false;
				}
 			}
 
 			fHeights[iIndex] = fHitHeightAsFloat;
 
 			fMinHeight = rage::Min(fMinHeight, fHitHeightAsFloat);
			fMaxHeight = rage::Max(fMaxHeight, fHitHeightAsFloat);
 
 			fY = fY + scGridSpace;
 		}
 		fX = fX + scGridSpace;
 	}
 
 	m_pJunction->m_nHeightBaseWorld = COORSZ_TO_UINT16(fMinHeight);
	m_pJunction->m_uMaxZ = COORSZ_TO_UINT16(fMaxHeight);

	const float fMinHeightQuantized = UINT16_TO_COORSZ(m_pJunction->m_nHeightBaseWorld);
	const float fMaxHeightQuantized = UINT16_TO_COORSZ(m_pJunction->m_uMaxZ);
 
 	//iterate through again, and set the offset
	const double fStep = (fMaxHeightQuantized - fMinHeightQuantized) / 256.0;
//  	for(int iX=0; iX<m_pJunction->m_nXSamples; iX++)
//  	{
//  		for(int iY=0; iY<m_pJunction->m_nYSamples; iY++)
//  		{
//  			int iIndex = iX + iY*m_pJunction->m_nXSamples;
//  			//m_pJunction->m_nHeightOffsets[iIndex] = (u8)((fHeights[iIndex] - fMinHeightQuantized) / fStep);
// 			m_pPathRegion->aHeightSamples[m_pPathRegion->NumHeightSamples] = (u8)((fHeights[iIndex] - fMinHeightQuantized) / fStep);
// 			++m_pPathRegion->NumHeightSamples;
//  		}
//  	}
	for (int i = 0; i < iNumSamples; i++)
	{
		const float fAmountOverBaseHeight = rage::Max(0.0f, fHeights[i] - fMinHeightQuantized);
		const float fNumSteps = (float)((double)fAmountOverBaseHeight / fStep);
		const int iNumSteps = rage::round(fNumSteps);

		FatalAssertf(m_pPathRegion->NumHeightSamples < MAXHEIGHTSAMPLESPERREGION, "Too many height samples in region, quitting");

		m_pPathRegion->aHeightSamples[m_pPathRegion->NumHeightSamples] = (u8)rage::Min(255, iNumSteps);
		++m_pPathRegion->NumHeightSamples;
	}

	static bool bDoThisStuff = false;
	if (bDoThisStuff)
	{
		//print unquantized heights
// 		Displayf("heights unquantized");
// 		for (int i = 0; i < iNumSamples; i++)
// 		{
// 			Displayf("%.4f", fHeights[i]);
// 		}
		//print quantized heights
		//Displayf("heights quantized");
		const float fBaseHeightQuantized = UINT16_TO_COORSZ(m_pJunction->m_nHeightBaseWorld);
		for (int i = m_pJunction->m_nStartIndexOfHeightSamples; i < m_pJunction->m_nStartIndexOfHeightSamples+iNumSamples; i++)
		{
			const float fQuantizedHeight = (float)((double)fBaseHeightQuantized + (m_pPathRegion->aHeightSamples[i] * fStep));
			const int iRelativeIndex = i - m_pJunction->m_nStartIndexOfHeightSamples;
			aiDisplayf("%d : %.3f -> %.3f", iRelativeIndex, fHeights[iRelativeIndex], fQuantizedHeight);
			static dev_float s_fHeightTolerance = 0.10f;
			if (fabsf(fHeights[iRelativeIndex] - fQuantizedHeight) > s_fHeightTolerance)
			{
				Displayf("Junction at %.2f, %.2f, %.2f has sample error %.4f greater than allowed (%.4f)", Average(vMin, vMax).GetXf(), Average(vMin, vMax).GetYf()
					, Average(vMin, vMax).GetZf(), fabsf(fHeights[iRelativeIndex] - fQuantizedHeight), s_fHeightTolerance);
			}
		}
	}
 
 	return bAllSamplesFound;
 }

 void CVirtualJunctionInterface::SampleHeightfield(Vec3V_In vTestPos, Vec3V_InOut vPos_Out, Vec3V_InOut vNorm_Out) const
 {
 	const ScalarV vMinX = GetMinX();
 	const ScalarV vMinY = GetMinY();
 	const ScalarV scGridSpace(CPathVirtualJunction::ms_fGridSpace);
 	const ScalarV fXGrid = (vTestPos.GetX() - vMinX) / scGridSpace;
 	const ScalarV fYGrid = (vTestPos.GetY() - vMinY) / scGridSpace;
 	const ScalarV fXGridInt = Clamp(RoundToNearestIntNegInf(fXGrid),ScalarV(V_ZERO),ScalarV((float)(m_pJunction->m_nXSamples-2)));
 	const ScalarV fYGridInt = Clamp(RoundToNearestIntNegInf(fYGrid),ScalarV(V_ZERO),ScalarV((float)(m_pJunction->m_nYSamples-2)));
 	const ScalarV fXGridRemainder = fXGrid - fYGridInt;
 	const ScalarV fYGridRemainder = fYGrid - fYGridInt;
 
 	// Find the sub-cell triangle (by projecting down)
 	Vec3V vA, vB, vC;
 	{
 		int iX = (int)fXGridInt.Getf(); // Already clamped
 		Assert(iX>=0 && iX <= m_pJunction->m_nXSamples-2);
 		int iY = (int)fYGridInt.Getf(); // Already clamped
 		Assert(iY>=0 && iY <= m_pJunction->m_nYSamples-2);
 		const ScalarV fX = vMinX + fXGridInt * scGridSpace;
 		const ScalarV fY = vMinY + fYGridInt * scGridSpace;
 
 		int iXA, iYA, iXB, iYB, iXC, iYC;
 		ScalarV fXA, fYA, fXB, fYB, fXC, fYC;
 
 		if(IsLessThanAll(fXGridRemainder+fYGridRemainder,ScalarV(V_ONE)))
 		{
 			iXA = iX,				iYA = iY;
 			fXA = fX,				fYA = fY;
 			iXB = iX,				iYB = iY + 1;
 			fXB = fX,				fYB = fY + scGridSpace;
 			iXC = iX + 1,			iYC = iY;
 			fXC = fX + scGridSpace,fYC = fY;
 		}
 		else
 		{
 			iXA = iX,				iYA = iY + 1;
 			fXA = fX,				fYA = fY + scGridSpace;
 			iXB = iX + 1,			iYB = iY + 1;
 			fXB = fX + scGridSpace,fYB = fY + scGridSpace;
 			iXC = iX + 1,			iYC = iY;
 			fXC = fX + scGridSpace,fYC = fY;
 		}
 		const ScalarV fHeightA = GetHeight(iXA,iYA);
 		vA = Vec3V(fXA,fYA,fHeightA);
 		const ScalarV fHeightB = GetHeight(iXB,iYB);
 		vB = Vec3V(fXB,fYB,fHeightB);
 		const ScalarV fHeightC = GetHeight(iXC,iYC);
 		vC = Vec3V(fXC,fYC,fHeightC);
 	}
 
 #if __BANK
 	if(CVehicleAILodManager::ms_bRaiseDummyJunctions)
 	{
 		const ScalarV fHeight(CVehicleAILodManager::ms_fRaiseDummyRoadHeight);
 		vA.SetZ(vA.GetZ()+fHeight);
 		vB.SetZ(vB.GetZ()+fHeight);
 		vC.SetZ(vC.GetZ()+fHeight);
 	}
 #endif
 
 	const Vec3V vPerp = Cross(vC-vA,vB-vA);
 	vNorm_Out = NormalizeFast(vPerp);
 	vPos_Out = vTestPos - vNorm_Out * Dot(vNorm_Out,vTestPos-vA);
 
 #if DEBUG_DRAW
 	if (CVehicleAILodManager::ms_bDisplayVirtualJunctions)
 	{
 		grcDebugDraw::Sphere(vA,0.15f,Color_red,true,-1);
 		grcDebugDraw::Sphere(vB,0.15f,Color_blue,true,-1);
 		grcDebugDraw::Sphere(vC,0.15f,Color_green,true,-1);
 		grcDebugDraw::Sphere(vPos_Out,0.15f,Color_white,true,-1);
 		grcDebugDraw::Line(vPos_Out,vPos_Out+ScalarV(V_THREE)*vNorm_Out,Color_white,-1);
 	}
 #endif
 }

 ScalarV_Out CVirtualJunctionInterface::GetLongestEdge() const
 {
	 const ScalarV vMinX = GetMinX();
	 const ScalarV vMinY = GetMinY();
	 const ScalarV vMaxX = vMinX + ScalarV(rage::Max(0, m_pJunction->m_nXSamples - 1) * CPathVirtualJunction::ms_fGridSpace);
	 const ScalarV vMaxY = vMinY + ScalarV(rage::Max(0, m_pJunction->m_nYSamples - 1) * CPathVirtualJunction::ms_fGridSpace);

	 const ScalarV vXLength = vMaxX - vMinX;
	 const ScalarV vYLength = vMaxY - vMinY;

	 const ScalarV vLargestLength = rage::Max(vXLength, vYLength);

	 return vLargestLength;
 }

 bool CVirtualJunctionInterface::IsPosWithinBoundsXY(Vec3V_In vPos) const
 {
	const ScalarV vMinX = GetMinX();
	const ScalarV vMinY = GetMinY();
	const ScalarV vMaxX = vMinX + ScalarV(rage::Max(0, m_pJunction->m_nXSamples - 1) * CPathVirtualJunction::ms_fGridSpace);
	const ScalarV vMaxY = vMinY + ScalarV(rage::Max(0, m_pJunction->m_nYSamples - 1) * CPathVirtualJunction::ms_fGridSpace);

	//const bool bX = (m_vJunctionMax.x >= vPos.x) & (m_vJunctionMin.x <= vPos.x);
	//const bool bY = (m_vJunctionMax.y >= vPos.y) & (m_vJunctionMin.y <= vPos.y);
	const bool bX = IsLessThanAll(vPos.GetX(), vMaxX) && IsGreaterThanAll(vPos.GetX(), vMinX);
	const bool bY = IsLessThanAll(vPos.GetY(), vMaxY) && IsGreaterThanAll(vPos.GetY(), vMinY);

	return (bX && bY);
 }

 void CVirtualJunctionInterface::ProcessCollision(Vec3V_In vWorldWheelPos, CVirtualRoad::TWheelCollision & rWheelCollisionOut, CVirtualRoad::TRouteConstraint * pRouteConstraintOut) const
 {
 	Vec3V vHitPos, vHitNormal;
 	SampleHeightfield(vWorldWheelPos,vHitPos,vHitNormal);
 	ScalarV fHeightAboveRoad = Dot(vHitNormal,vWorldWheelPos-vHitPos);
 	rWheelCollisionOut.m_vPosition = Vec4V(vHitPos,fHeightAboveRoad);
 	rWheelCollisionOut.m_vNormal = vHitNormal;
 	if(pRouteConstraintOut)
 	{
 		// TODO: Fake constraint for now
 		pRouteConstraintOut->m_fConstraintRadiusOnRoute = ScalarV(100.0f);
 		pRouteConstraintOut->m_vConstraintPosOnRoute = Vec3V(GetMinX(), GetMinY(), GetHeight(0, 0));
		pRouteConstraintOut->m_bJunctionConstraint = true;
		pRouteConstraintOut->m_bWasOffroad = false;
 	}
 }

 void CVirtualJunctionInterface::DebugDraw() const
 {
 #if DEBUG_DRAW
 
	 static const ScalarV scGridSpace(CPathVirtualJunction::ms_fGridSpace);
 	ScalarV fX = GetMinX();
 	for(int iX=0; iX<m_pJunction->m_nXSamples; iX++)
 	{
 		ScalarV fY = GetMinY();
 		for(int iY=0; iY<m_pJunction->m_nYSamples; iY++)
 		{
 			ScalarV fHeight = GetHeight(iX, iY);
 
 			const Vec3V vSamplePos(fX,fY,fHeight);
 			Color32 drawColor = Color_coral;
 			mthRandom rnd(m_pJunction->m_nHeightBaseWorld);	//seed based on height
 			drawColor = (Color32)rnd.GetInt();
 			drawColor.SetAlpha(255);
 			grcDebugDraw::Line(vSamplePos,vSamplePos+Vec3V(V_Z_AXIS_WZERO),drawColor);

			//draw index information
			//TUNE_GROUP_BOOL(ASDF, DrawVirtualJunctionText, true);
			static dev_bool DrawVirtualJunctionText = false;
			if(DrawVirtualJunctionText)
			{
				int iIndex = iX + iY * m_pJunction->m_nXSamples;
				const int iSampleIndex = m_pJunction->m_nStartIndexOfHeightSamples + iIndex;
				const Vec3V vTextDrawpos(vSamplePos+Vec3V(V_Z_AXIS_WZERO));
				
				char buf[32];
				sprintf(buf, "%d : %d", iIndex, m_pPathRegion->aHeightSamples[iSampleIndex]);
				grcDebugDraw::Text(vTextDrawpos, Color_WhiteSmoke, 0, 0, buf);
			}
 
 			fY = fY + scGridSpace;
 		}
 		fX = fX + scGridSpace;
 	}
 #endif
 }

 float CVirtualJunctionInterface::FindNearestGroundHeightForVirtualJunctionWithJittering(const Vector3 &pos, float rangeUp, float rangeDown, float fMinExpectedHeight, float fMaxExpectedHeight)
 {
	 static const s32 s_iNumSamples = 4;
	 atFixedArray<float, s_iNumSamples> fHeights;
	 for (s32 i = 0; i < s_iNumSamples; i++)
	 {
		Vector3 vJitteredPos = pos;

		//some jitterin'
		switch (i)
		{
		case 0:
			vJitteredPos.x += 0.15f;
			vJitteredPos.y += 0.30f;
			break;
		case 1:
			vJitteredPos.x += 0.30f;
			vJitteredPos.y -= 0.15f;
			break;
		case 2:
			vJitteredPos.x -= 0.15f;
			vJitteredPos.y -= 0.30f;
			break;
		case 3:
			vJitteredPos.x -= 0.30f;
			vJitteredPos.y += 0.15f;
			break;
		}

		const float fReturnedHeight = FindNearestGroundHeightForVirtualJunction(vJitteredPos, rangeUp, rangeDown, fMinExpectedHeight, fMaxExpectedHeight);

		fHeights.Push(fReturnedHeight);
	 }

	 std::sort(fHeights.begin(), fHeights.end());

	 //throw out the outlier
	 const float fReturnHeight = fHeights[1];

	 return fReturnHeight;
 }

 float CVirtualJunctionInterface::FindNearestGroundHeightForVirtualJunction(const Vector3 &posn, float rangeUp, float rangeDown, float fMinExpectedHeight, float fMaxExpectedHeight)
 {
	 //static bool bUseTopHitBelowThreshold = true;

	 //phIntersection	result;
	 float	heightFound = posn.z;
	// bool	bHitFound = false;

	 //phSegment testSegment(Vector3(posn.x, posn.y, posn.z + rangeUp), Vector3(posn.x, posn.y, posn.z - rangeDown) );
	 //CPhysics::GetLevel()->TestProbe(testSegment, &result, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES);
	 //static dev_float s_fAboveThresholdForPathProbes = 0.5f;

	 WorldProbe::CShapeTestResults results(4);
	 WorldProbe::CShapeTestProbeDesc probeData;
	 probeData.SetStartAndEnd(Vector3(posn.x, posn.y, posn.z + rangeUp),Vector3(posn.x, posn.y, posn.z - rangeDown));
	 probeData.SetContext(WorldProbe::ENotSpecified);
	 probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	 probeData.SetIsDirected(true);
	 probeData.SetResultsStructure(&results);

	 WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

	 const int iNumHits = results.GetNumHits();

	 //print all the found samples
	 
#if __DEV
	 TUNE_GROUP_BOOL(VIRTUAL_JUNCTIONS, DrawHeightSampleHits, false);
	 if (DrawHeightSampleHits)
	 {
		 for (int i=0; i < iNumHits; i++)
		 {
			 WorldProbe::CShapeTestHitPoint& result = results[i];
			 grcDebugDraw::Sphere(result.GetPosition(), 0.1f, Color_red, false, 60);
		 }
	 } 
#endif //__DEV
	 
	 float fHighestHeight = -FLT_MAX;
	 for (int i=0; i < iNumHits; i++)
	 {
		WorldProbe::CShapeTestHitPoint& result = results[i];
		const float fResultHeight = result.GetPosition().GetZf();

		//if we have a height sample in the good range, that's probably the right one
		//even if it isn't the highest
		if (fResultHeight >= fMinExpectedHeight && fResultHeight <= fMaxExpectedHeight + rangeUp)
		{
			heightFound = fResultHeight;
			return heightFound;
		}

		//otherwise, choose the highest sample we have available
		if (fResultHeight > fHighestHeight)
		{
			heightFound = fResultHeight;
			fHighestHeight = fResultHeight;
		}
	 }

	 return heightFound;
 }

#if !OPT_GET_VIRTUAL_JUNCTION_FROM_NODE_ADDR	// Could probably remove this function now:

 bool CVirtualJunctionInterface::ProcessCollision(Vec3V_In vJunctionCentre, Vec3V_In vWorldWheelPos, CVirtualRoad::TWheelCollision & rWheelCollisionOut, CVirtualRoad::TRouteConstraint * pRouteConstraintOut)
 {
	 if (!CVirtualRoad::ms_bEnableVirtualJunctionHeightmaps)
	 {
		 return false;
	 }

	 const s32 iRealJunctionIndex = CJunctions::GetJunctionAtPosition(VEC3V_TO_VECTOR3(vJunctionCentre));
	 s32 iVirtualJunctionIndex = -1;
	 s32 iJunctionRegion = -1;

	 if (iRealJunctionIndex >= 0)
	 {
		const CJunction* pRealJunction = CJunctions::GetJunctionByIndex(iRealJunctionIndex);
		Assert(pRealJunction);
		iJunctionRegion = pRealJunction->GetJunctionNode(0).GetRegion();
		if (!ThePaths.IsRegionLoaded(iJunctionRegion))
		{
			return false;
		}
		iVirtualJunctionIndex = pRealJunction->GetVirtualJunctionIndex();

		//we only want to test Z here, since we already test XY 
		//against the virtual junction's AABB, which may be larger than the actual jn's
		static dev_float fZTolerance = 3.0f;
		if (!pRealJunction->IsPosWithinBoundsZOnly(VEC3V_TO_VECTOR3(vWorldWheelPos), fZTolerance))
		{
			return false;
		}
	 }

	 if (iVirtualJunctionIndex < 0)
	 {
		return false;
	 }

	 Assert(iVirtualJunctionIndex < ThePaths.apRegions[iJunctionRegion]->NumJunctions);

	 //now get the virtual junction data
	 CPathVirtualJunction* pVirtJunction = &ThePaths.apRegions[iJunctionRegion]->aVirtualJunctions[iVirtualJunctionIndex];
	 Assert(pVirtJunction);
	 CVirtualJunctionInterface jnInterface(pVirtJunction, ThePaths.apRegions[iJunctionRegion]);

	 //test XY extents here
	 if (!jnInterface.IsPosWithinBoundsXY(vWorldWheelPos))
	 {
		 return false;
	 }

	 jnInterface.ProcessCollision(vWorldWheelPos, rWheelCollisionOut, pRouteConstraintOut);

	 return true;
 }

#endif	// !OPT_GET_VIRTUAL_JUNCTION_FROM_NODE_ADDR

bool CVirtualJunctionInterface::ProcessCollisionFromNodeAddr(const CNodeAddress& nodeAddr, Vec3V_In vWorldWheelPos, CVirtualRoad::TWheelCollision & rWheelCollisionOut, CVirtualRoad::TRouteConstraint * pRouteConstraintOut)
{
	if(!CVirtualRoad::ms_bEnableVirtualJunctionHeightmaps)
	{
		return false;
	}

	//check if this node's region is loaded
	const int iJunctionRegion = nodeAddr.GetRegion();
	if(!ThePaths.IsRegionLoaded(iJunctionRegion))
	{
		return false;
	}

	//check if this node is a part of a virtual junction
	const int* virtualJunctionIndexPtr = ThePaths.apRegions[iJunctionRegion]->JunctionMap.JunctionMap.SafeGet(nodeAddr.RegionAndIndex());
	if(!virtualJunctionIndexPtr)
	{
		return false;
	}

	//now get the virtual junction data
	const int iVirtualJunctionIndex = *virtualJunctionIndexPtr;
	Assert(iVirtualJunctionIndex >= 0);
	Assert(iVirtualJunctionIndex < ThePaths.apRegions[iJunctionRegion]->NumJunctions);
	CPathVirtualJunction* pVirtJunction = &ThePaths.apRegions[iJunctionRegion]->aVirtualJunctions[iVirtualJunctionIndex];
	Assert(pVirtJunction);
	CVirtualJunctionInterface jnInterface(pVirtJunction, ThePaths.apRegions[iJunctionRegion]);

	//test XY extents here
	if(!jnInterface.IsPosWithinBoundsXY(vWorldWheelPos))
	{
		return false;
	}

	jnInterface.ProcessCollision(vWorldWheelPos, rWheelCollisionOut, pRouteConstraintOut);
	return true;
}
