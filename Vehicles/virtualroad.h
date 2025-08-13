/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    virtualroad.h
// PURPOSE : Deal with a made up road surface when cars are in ghost state
// AUTHOR :  Obbe.
// CREATED : 22-1-07
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef _VIRTUALROAD_H_
#define _VIRTUALROAD_H_

#include "vehicleAi/junctions.h"

class CVehicle;
class CVirtualJunctionCollection;
class CVirtualJunction;

/////////////////////////////////////////////////////////////////////////////////
// CVirtualRoad

class CVirtualRoad
{
public:

	struct TRouteInfo
	{
		Vec3V m_vPoints[3];
		Vec3V m_vNodeDir[2];
		Vec3V m_vNodeDirNormalised[2];
		Vec4V m_vCambers; // packed values for camber (this side seg 0, other side seg 0, this side seg 1, other side seg 1)
		Vec4V m_vWidths; // packed values for width of road (no buffer for constraints) (right seg 0, left seg 0, right seg 1, left seg 1)
		Vec2V m_vWidthBuffers;	// packed values for buffer for constraints (seg 0, seg 1)
		u8 m_iCamberFalloffs[4]; // packed values for camber-falls-off u8's (this side seg 0, other side seg 0, this side seg 1, other side seg 1)
		bool m_bHasTwoSegments;
		bool m_bCalcWidthConstraint;
		bool m_bConsiderJunction;
		Vec4V m_vJunctionPosAndNodeAddr; // position of the junction, with the node address of the junction stuffed in the W component

		void SetSegment0(Vec3V_In vPos0, Vec3V_In vPos1, float fCamberThisSide0, float fCamberOtherSide0, u8 iCamberFalloffThisSide0, u8 bCamberFalloffOtherSide0);
		void SetSegment0WidthsMod(float fWidthRight0, float fWidthLeft0, float fWidthBuffer0);
		void SetSegment0BufferWidth(const float fWidthBuffer0);
		void SetNoSegment1();
		void SetSegment1(Vec3V_In vPos1, Vec3V_In vPos2, float fCamberThisSide1, float fCamberOtherSide1, u8 iCamberFalloffThisSide1, u8 iCamberFalloffOtherSide1);
		void SetSegment1WidthsMod(float fWidthRight1, float fWidthLeft1, float fWidthBuffer1);
		void SetSegment1BufferWidth(const float fWidthBuffer1);
		void SetNoJunction();
		void SetJunction(Vec3V_In vJunctionPos, const CNodeAddress& junctionNode);

		Vec3V_Out GetJunctionPos() const { return m_vJunctionPosAndNodeAddr.GetXYZ(); }
		const CNodeAddress& GetJunctionNodeAddr() const { return reinterpret_cast<const CNodeAddress*>(&m_vJunctionPosAndNodeAddr)[3]; }

		ASSERT_ONLY(bool Validate() const;)
	};
	struct TWheelCollision
	{
		Vec4V m_vPosition;					// xyz impact pos, and w component holds planar distance above the road
		Vec3V m_vNormal;					// normal for virtual road surface
	};
	struct TRouteConstraint
	{
		TRouteConstraint()
		{
			m_bJunctionConstraint = false;
			m_bWasOffroad = false;
		}
		Vec3V m_vConstraintPosOnRoute;		// used for physical constraint ("closest" calculation includes road radius)
		ScalarV m_fConstraintRadiusOnRoute;	// used for physical constraint ("closest" calculation includes road radius)
		bool m_bJunctionConstraint;			// ignore the constraint value here and re-calculate
		bool m_bWasOffroad;					// if true, be more strict about considering on the junction or not, to avoid a pop
	};

	// NAME : ProcessRoadCollision
	// PURPOSE : Core function for sampling the road surface given a point and a TRouteInfo defining the surface
	static void ProcessRoadCollision(const TRouteInfo & routeInfo, Vec3V_In vWorldWheelPos, TWheelCollision & rWheelCollisionOut, TRouteConstraint * pRouteConstraintOut);

	// NAME: ProcessRouteConstraint
	// PURPOSE : Core function for finding the width of the route at this position for use in creating a constraint
	static void ProcessRouteConstraint(const TRouteInfo & rRouteInfo, Vec3V_In vWorldWheelPos, TRouteConstraint & rRouteConstraintOut, const bool bOnlyTestConstraints);
	static void ProcessRouteConstraintWithPrecalcValues(const TRouteInfo & rRouteInfo, Vec3V_In vWorldWheelPos, ScalarV_In fTSeg, ScalarV_In fT0, ScalarV_In fT1, ScalarV_In fDistsSqrToSeg0, ScalarV_In fDistsSqrToSeg1, Vec3V_In vRightFlat0, Vec3V_In vRightFlat1, TRouteConstraint & rRouteConstraintOut, const bool bOnlyTestConstraints);

	// NAME : TestProbesToVirtualRoad*
	// PURPOSE : Test probes against virtual road as defined in a few different ways.  The core funciton uses a TRouteInfo; the other functions are wrappers to get to a TRouteInfo and use that.
	static void TestProbesToVirtualRoadRouteInfo(const TRouteInfo & rRouteInfo, WorldProbe::CShapeTestResults & rTestResultsOut, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ);
	static void TestProbesToVirtualRoadTwoRouteInfos(const TRouteInfo & rRouteInfoA, Vec3V_In vRouteInfoCenterA, const TRouteInfo & rRouteInfoB, Vec3V_In vRouteInfoCenterB, WorldProbe::CShapeTestResults & rTestResults_Out, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ);
	static void TestProbesToVirtualRoadFollowRouteAndCenterPos(const CVehicleFollowRouteHelper & rFollowRoute, Vec3V_In vCenterPos, WorldProbe::CShapeTestResults & rTestResultsOut, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ, const bool bConsiderJunctions);
	static void TestProbesToVirtualRoadFollowRouteAndTwoPoints(const CVehicleFollowRouteHelper & rFollowRoute, Vec3V_In vA, Vec3V_In vB, WorldProbe::CShapeTestResults & rTestResultsOut, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ, const bool bConsiderJunctions);
	static void TestProbesToVirtualRoadNodeListAndCenterPos(const CVehicleNodeList & rNodeList, Vec3V_In vCenterPos, WorldProbe::CShapeTestResults & rTestResultsOut, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ);
	static void TestProbesToVirtualRoadNodeListAndTwoPoints(const CVehicleNodeList & rNodeList, Vec3V_In vA, Vec3V_In vB, WorldProbe::CShapeTestResults & rTestResultsOut, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ);
	static void TestProbesToVirtualRoadNodeListAndCenterNode(const CVehicleNodeList & pNodeList, int iCenterNode, WorldProbe::CShapeTestResults & rTestResultsOut, phSegment * pTestSegments, int iNumSegments, ScalarV_In fVehicleUpZ);

	// NAME : TestPointsToVirtualRoad*
	// PURPOSE : Test points against virtual road (similar to test probes)
	static bool TestPointsToVirtualRoadNodeListAndCenterPos(const CVehicleNodeList & rNodeList, Vec3V_In vCenterPos, const Vec3V * vTestPoints, int iNumTestPoints, TWheelCollision * pResults);
	static bool TestPointsToVirtualRoadFollowRouteAndCenterPos(const CVehicleFollowRouteHelper& rFollowRoute, Vec3V_In vCenterPos, const Vec3V * vTestPoints, int iNumTestPoints, TWheelCollision * pResults, const bool bConsiderJunctions);

	// NAME : CalculatePathInfo
	// PURPOSE : Ascertain whether a vehicle is close enough to road nodes to be a dummy/superdummy
	// This used to be more widely used by the vehicles in VDM_DUMMY, however now it is only used to
	// determine whether a vehicle is close enough to nodes to convert
	static bool CalculatePathInfo(const CVehicle & rVeh, bool bConstrainToStartAndEnd, bool bStrictPathDist, Vec3V_InOut vClosestPointOnPathOut, float & fMaxValidDistToClosestPointOut, const bool bOnlyTestConstraints);

	// Helpers to create virtual road data structures
	static bool HelperFindClosestSegmentAlongFollowRoute(Vec3V_In vTestPos, const CVehicleFollowRouteHelper & rFollowRoute, int & iSeg_Out, ScalarV_InOut fTAlongSeg_Out, Vec3V_InOut vClosestOnSeg_Out, bool & bBeforeStart, bool & bAfterEnd);
	static void HelperCreateRouteInfoFromRoutePoints(const CRoutePoint* pRoutePoints, int iNumPoints, const CRoutePoint* pOldPoint, const CRoutePoint* pFuturePoint, bool bCalcConstraint, bool bForceIncludeWidths, TRouteInfo & rRouteInfoOut, const bool bConsiderJunctions);
	static bool HelperCreateRouteInfoFromFollowRouteAndCenterPos(const CVehicleFollowRouteHelper & rFollowRoute, Vec3V_In vCenterPos, TRouteInfo & rRouteInfoOut, const bool bConsiderJunctions, const bool bForceIncludeWidths);
	static void HelperCreateRouteInfoFromFollowRouteAndCenterNode(const CVehicleFollowRouteHelper & rFollowRoute, int iCenterNode, bool bCalcConstraint, bool bForceIncludeWidths, TRouteInfo & rRouteInfoOut, const bool bConsiderJunctions);
	static bool HelperCreateRouteInfoFromNodeListAndCenterPos(const CVehicleNodeList & rNodeList, Vec3V_In vPos, TRouteInfo & rRouteInfoOut);
	static bool HelperCreateRouteInfoFromNodeListAndCenterNode(const CVehicleNodeList & rNodeList, int iCenterNode, TRouteInfo & rRouteInfoOut);
	static bool HelperGetLongestJunctionEdge(const CNodeAddress& nodeAddress, const Vector3& vPosIn, float& fLengthOut);

	// Junction stuff
	static void DebugDrawJunctions(Vec3V_In vDrawCentre);
	//static void UpdateJunctionSampling();
	//static CVirtualJunction* AddNewVirtualJunction(Vec3V_In vJunctionCentre, CJunction* pRealJunctionPtr=NULL);
	//static void	RemoveVirtualJunction(CVirtualJunction* pJunction); 

	static bool	ms_bEnableVirtualJunctionHeightmaps;
	static bool ms_bEnableConstraintSlackWhenChangingPaths;
	//static CVirtualJunctionCollection ms_VirtualJunctions;

private:
	static bool CalculatePathInfoUsingFollowRoute(const CVehicleFollowRouteHelper & rFollowRoute, Vec3V_In vVehPos, bool bConstrainToStartAndEnd, bool bStrictPathDist, Vec3V_InOut closestPointOnPathOut, float & fMaxValidDistToClosestPointOut, const bool bOnlyTestConstraints, bool& bWasOffroadInOut);
	static bool CalculatePathInfoUsingNodeList(const CVehicleNodeList & rNodeList, Vec3V_In vVehPos, bool bConstrainToStartAndEnd, bool bStrictPathDist, Vec3V_InOut vClosestPointOnPathOut, float & fMaxValidDistToClosestPointOut);

	static void DebugDrawHit(Vec3V_In vHitPos, Vec3V_In vHitNormal, Color32 col);
	static void DebugDrawRouteInfo(const TRouteInfo & rRouteInfo, Vec3V_In vWorldWheelPos, int iSeg);
};

//lightweight class that contains all gameside functionality for the virtual junction
class CVirtualJunctionInterface
{
public:
	CVirtualJunctionInterface(CPathVirtualJunction* pVirtJunction
		, CPathRegion* pRegion)
	{
		m_pJunction = pVirtJunction;
		m_pPathRegion = pRegion;
	}

	void InitAndSample(const CJunction* pRealJunction, float fGridSpace, const int iStartIndexOfHeightSamples
		, const CPathFind& paths);
	void SampleHeightfield(Vec3V_In vPos, Vec3V_InOut vPos_Out, Vec3V_InOut vNorm_Out) const;
	ScalarV_Out GetHeight(int iX, int iY) const;
	//void SetHeight(int iX, int iY, float fHeight);

	bool SampleFromWorld(ScalarV_In vStartHeight, Vec3V_In vMin, Vec3V_In vMax);
	void ProcessCollision(Vec3V_In vWorldWheelPos, CVirtualRoad::TWheelCollision & rWheelCollision_Out, CVirtualRoad::TRouteConstraint * pRouteConstraint_Out) const;

 	void DebugDraw() const;

	static bool ProcessCollision(Vec3V_In vJunctionCentre, Vec3V_In vWorldWheelPos, CVirtualRoad::TWheelCollision & rWheelCollision_Out, CVirtualRoad::TRouteConstraint * pRouteConstraint_Out);
	static bool ProcessCollisionFromNodeAddr(const CNodeAddress& nodeAddr, Vec3V_In vWorldWheelPos, CVirtualRoad::TWheelCollision & rWheelCollisionOut, CVirtualRoad::TRouteConstraint * pRouteConstraintOut);
 
 	ScalarV GetMinX() const
 	{
 		const float fMinX = INT16_TO_COORS(m_pJunction->m_iMinX);
 		return LoadScalar32IntoScalarV(fMinX);
 	}
 	ScalarV GetMinY() const
 	{
 		const float fMinY = INT16_TO_COORS(m_pJunction->m_iMinY);
 		return LoadScalar32IntoScalarV(fMinY);
 	}
	void SetMinX(ScalarV_In vMinX)
	{
		m_pJunction->m_iMinX = COORS_TO_INT16(vMinX.Getf());
	}
	void SetMinY(ScalarV_In vMinY)
	{
		m_pJunction->m_iMinY = COORS_TO_INT16(vMinY.Getf());
	}

	bool IsPosWithinBoundsXY(Vec3V_In vPos) const;

	ScalarV_Out GetLongestEdge() const;
	
	CPathVirtualJunction*	m_pJunction;	//not owned
	CPathRegion*			m_pPathRegion;	//not owned

	static float	FindNearestGroundHeightForVirtualJunction(const Vector3 &pos, float rangeUp, float rangeDown, float fMinExpectedHeight, float fMaxExpectedHeight);
	static float	FindNearestGroundHeightForVirtualJunctionWithJittering(const Vector3 &pos, float rangeUp, float rangeDown, float fMinExpectedHeight, float fMaxExpectedHeight);
};

 inline ScalarV_Out CVirtualJunctionInterface::GetHeight(int iX, int iY) const
 {
 	const int iIndex = iX + iY*m_pJunction->m_nXSamples;
 
 	const float fMinHeight = UINT16_TO_COORSZ(m_pJunction->m_nHeightBaseWorld);
	const float fMaxHeight = UINT16_TO_COORSZ(m_pJunction->m_uMaxZ);
 	const double fStep = (fMaxHeight - fMinHeight) / 256.0;
	const int iSampleIndex = m_pJunction->m_nStartIndexOfHeightSamples + iIndex;
 	const float fQuantizedHeight 
 		= (float)((double)fMinHeight + (m_pPathRegion->aHeightSamples[iSampleIndex] * (fStep)));
 
 	return LoadScalar32IntoScalarV(fQuantizedHeight);
 }

//  inline void CPathVirtualJunction::SetHeight(int iX, int iY, float fHeight)
//  {
//  	// Set into the height field
//  	int iIndex = iX + iY * m_nXSamples;
//  	m_fHeights[iIndex] = fHeight;
//  }



/////////////////////////////////////////////////////////////////////////////////
// CVirtualRoad inlined functions

inline void CVirtualRoad::TRouteInfo::SetSegment0(Vec3V_In vPos0, Vec3V_In vPos1, float fCamberThisSide0, float fCamberOtherSide0, u8 iCamberFalloffThisSide0, u8 iCamberFalloffOtherSide0)
{
	m_vPoints[0] = vPos0;
	m_vPoints[1] = vPos1;
	const Vec3V vDir0 = vPos1 - vPos0;
	m_vNodeDir[0] = vDir0;
	m_vNodeDirNormalised[0] = NormalizeFast(vDir0);
	m_vCambers.SetXf(fCamberThisSide0);
	m_vCambers.SetYf(fCamberOtherSide0);
	m_iCamberFalloffs[0] = iCamberFalloffThisSide0;
	m_iCamberFalloffs[1] = iCamberFalloffOtherSide0;
	m_vWidths.SetX(V_NEGONE);
	m_vWidths.SetY(V_NEGONE);
}

inline void CVirtualRoad::TRouteInfo::SetSegment0WidthsMod(float fWidthRight0, float fWidthLeft0, float fWidthBuffer0)
{
	m_vWidths.SetXf(fWidthRight0);
	m_vWidths.SetYf(fWidthLeft0);
	SetSegment0BufferWidth(fWidthBuffer0);
}

inline void CVirtualRoad::TRouteInfo::SetSegment0BufferWidth(float fWidthBuffer0)
{
	m_vWidthBuffers.SetXf(fWidthBuffer0);
}

inline void CVirtualRoad::TRouteInfo::SetSegment1(Vec3V_In vPos1, Vec3V_In vPos2, float fCamberThisSide1, float fCamberOtherSide1, u8 iCamberFalloffThisSide1, u8 iCamberFalloffOtherSide1)
{
	m_vPoints[2] = vPos2;
	const Vec3V vDir1 = vPos2 - vPos1;
	m_vNodeDir[1] = vDir1;
	m_vNodeDirNormalised[1] = NormalizeFast(vDir1);
	m_vCambers.SetZf(fCamberThisSide1);
	m_vCambers.SetWf(fCamberOtherSide1);
	m_bHasTwoSegments = true;
	m_iCamberFalloffs[2] = iCamberFalloffThisSide1;
	m_iCamberFalloffs[3] = iCamberFalloffOtherSide1;
}

inline void CVirtualRoad::TRouteInfo::SetSegment1WidthsMod(float fWidthRight1, float fWidthLeft1, float fWidthBuffer1)
{
	m_vWidths.SetZf(fWidthRight1);
	m_vWidths.SetWf(fWidthLeft1);
	SetSegment1BufferWidth(fWidthBuffer1);
}

inline void CVirtualRoad::TRouteInfo::SetSegment1BufferWidth(float fWidthBuffer1)
{
	m_vWidthBuffers.SetYf(fWidthBuffer1);
}

inline void CVirtualRoad::TRouteInfo::SetNoSegment1()
{
	m_vCambers.SetZ(m_vCambers.GetX());
	m_vCambers.SetW(m_vCambers.GetY());
	m_bHasTwoSegments = false;
}

inline void CVirtualRoad::TRouteInfo::SetNoJunction()
{
	m_bConsiderJunction = false;
	m_vJunctionPosAndNodeAddr = Vec4V(V_MASKW);	// (0 0 0 ~0) - zero position, node address 0xFFFFFFFF
	aiAssert(GetJunctionNodeAddr().IsEmpty());
}

inline void CVirtualRoad::TRouteInfo::SetJunction(Vec3V_In vJunctionPos, const CNodeAddress& junctionNode)
{
	CompileTimeAssert(sizeof(CNodeAddress) == 4);

	m_bConsiderJunction = true;
	const ScalarV addrV = LoadScalar32IntoScalarV(*(const u32*)&junctionNode);
	m_vJunctionPosAndNodeAddr = Vec4V(vJunctionPos, addrV);
	aiAssert(GetJunctionNodeAddr() == junctionNode);
}

#if !(DEBUG_DRAW && __BANK)
inline void CVirtualRoad::DebugDrawHit(Vec3V_In UNUSED_PARAM(vHitPos), Vec3V_In UNUSED_PARAM(vHitNormal), Color32 UNUSED_PARAM(col)) { }
inline void CVirtualRoad:: DebugDrawRouteInfo(const TRouteInfo & UNUSED_PARAM(rRouteInfo), Vec3V_In UNUSED_PARAM(vWorldWheelPos), int UNUSED_PARAM(iSeg)) { }
#endif

#endif // _VIRTUALROAD_H_
