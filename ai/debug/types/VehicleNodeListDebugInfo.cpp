#include "ai\debug\types\VehicleNodeListDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "vehicleAi\VehicleIntelligence.h"
#include "Task/System/TaskHelpers.h"

CVehicleNodeListDebugInfo::CVehicleNodeListDebugInfo(const CVehicle* pVeh, DebugInfoPrintParams printParams, s32 iNumberOfLines)
: CVehicleDebugInfo(pVeh, printParams, iNumberOfLines)
{

}

void CVehicleNodeListDebugInfo::PrintSearchInfo()
{
	const CPathNodeRouteSearchHelper* pSearchHelper = m_Vehicle->GetIntelligence()->GetRouteSearchHelper();
	if(pSearchHelper)
	{
		if(pSearchHelper->IsDoingWanderRoute())
		{
			PrintLn("...Wandering...");
		}
		
		u32 iFlags = pSearchHelper->GetSearchFlags();
		PrintLn("Route search flags: %d", iFlags);
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_IncludeWaterNodes){PrintLn("IncludeWaterNodes");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_JoinRoadInDirection){PrintLn("JoinRoadInDirection");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_DriveIntoOncomingTraffic){PrintLn("DriveIntoOncomingTraffic");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_WanderRoute){PrintLn("WanderRoute");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_UseShortcutLinks){PrintLn("UseShortcutLinks");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_UseExistingNodes){PrintLn("UseExistingNodes");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_UseSwitchedOffNodes){PrintLn("UseSwitchedOffNodes");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_FindRouteAwayFromTarget){PrintLn("FindRouteAwayFromTarget");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_AvoidTurns){PrintLn("AvoidTurns");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_WanderOffroad){PrintLn("WanderOffroad");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_ExtendPathWithResultsFromWander){PrintLn("ExtendPathWithResultsFromWander");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_AvoidHighways){PrintLn("AvoidHighways");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_HighwaysOnly){PrintLn("HighwaysOnly");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_IsMissionVehicle){PrintLn("IsMissionVehicle");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_StartNodesOverridden){PrintLn("StartNodesOverridden");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_AvoidingAdverseConditions){PrintLn("AvoidingAdverseConditions");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_ObeyTrafficRules){PrintLn("ObeyTrafficRules");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_AvoidRestrictedAreas){PrintLn("AvoidRestrictedAreas");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_OnStraightLineUpdate){PrintLn("OnStraightLineUpdate");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_CabAndTrailerExtendRoute){PrintLn("CabAndTrailerExtendRoute");}
		// 	if(iFlags & CPathNodeRouteSearchHelper::Flag_FindWanderRouteTowardsTarget){PrintLn("FindWanderRouteTowardsTarget");}
	}
}

void CVehicleNodeListDebugInfo::PrintRenderedSelection(bool bPrintAll)
{
	if (!m_Vehicle || !m_Vehicle->GetIntelligence())
		return;

	WriteLogEvent("VEHICLE PATHING INFO");
	PushIndent();
	PushIndent();

	PrintSearchInfo();
	PrintFollowRoute(bPrintAll);
	PrintFutureNodes(bPrintAll);

	PopIndent();
	PopIndent();
}

void CVehicleNodeListDebugInfo::Visualise()
{
	if (!m_Vehicle || !m_Vehicle->GetIntelligence())
		return;

	const CVehicleIntelligence* pIntelligence = m_Vehicle->GetIntelligence();
	CVehicleNodeList * pNodeList = pIntelligence->GetNodeList();
	if(CVehicleIntelligence::m_bDisplayDebugFutureNodes && pNodeList)
	{
		const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
		s32 iTargetNode = pNodeList->GetTargetNodeIndex();
		const Vector3 vRaiseAmt(0.0f,0.0f,0.5f);

		static CNodeAddress iNodes[CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED];
		static const CPathNode * pStoredNodes[CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED];
		static Vector3 vStoredNodes[CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED];

		for(s32 n=0; n<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
		{
			iNodes[n] = pNodeList->GetPathNodeAddr(n);
			pStoredNodes[n] = ThePaths.FindNodePointerSafe(iNodes[n]);
			if(pStoredNodes[n])
			{
				pStoredNodes[n]->GetCoors(vStoredNodes[n]);
				grcDebugDraw::Line(vStoredNodes[n], vStoredNodes[n] + vRaiseAmt, Color_green, Color_green);
				vStoredNodes[n] += vRaiseAmt;
			}
		}

		s32 iHistoryNode2 = iTargetNode-4;
		if((iHistoryNode2 >= 0 ) && pStoredNodes[iHistoryNode2])
		{
			grcDebugDraw::Line(vVehiclePosition, vStoredNodes[iHistoryNode2], Color_magenta);
			grcDebugDraw::Text(vStoredNodes[iHistoryNode2], Color_magenta, "NODE_HISTORY_2");
		}
		s32 iHistoryNode1 = iTargetNode-3;
		if((iHistoryNode1 >= 0 ) && pStoredNodes[iHistoryNode1])
		{
			grcDebugDraw::Line(vVehiclePosition, vStoredNodes[iHistoryNode1], Color_magenta1);
			grcDebugDraw::Text(vStoredNodes[iHistoryNode1], Color_magenta1, "NODE_HISTORY_2");
		}
		s32 iVeryOldNode = iTargetNode-2;
		if(iVeryOldNode >= 0 && pStoredNodes[iVeryOldNode])
		{
			grcDebugDraw::Line(vVehiclePosition, vStoredNodes[iVeryOldNode], Color_red);
			grcDebugDraw::Text(vStoredNodes[iVeryOldNode], Color_red, "NODE_VERY_OLD");
		}
		s32 iOldNode = iTargetNode-1;
		if(iOldNode >= 0 && pStoredNodes[iOldNode])
		{
			grcDebugDraw::Line(vVehiclePosition, vStoredNodes[iOldNode], Color_orange);
			grcDebugDraw::Text(vStoredNodes[iOldNode], Color_orange, "NODE_OLD");
		}

		s32 iFutureNode = iTargetNode + 1;
		if(iFutureNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED && pStoredNodes[iTargetNode] && pStoredNodes[iOldNode] && iOldNode >= 0 && pNodeList->GetPathLinkIndex(iTargetNode) >= 0)
		{
			grcDebugDraw::Line(vStoredNodes[iTargetNode], vStoredNodes[iTargetNode] + Vector3(0.0f,0.0f,0.5f), Color32(255, 255, 0, 255));

			Vector3	Coors2;
			pIntelligence->FindTargetCoorsWithNode(m_Vehicle, iNodes[iOldNode], iNodes[iTargetNode], iNodes[iFutureNode],
				pNodeList->GetPathLinkIndex(iOldNode), pNodeList->GetPathLinkIndex(iTargetNode),
				pNodeList->GetPathLaneIndex(iTargetNode), pNodeList->GetPathLaneIndex(iFutureNode), Coors2);

			grcDebugDraw::Line(vVehiclePosition, Coors2, Color32(255, 255, 0, 255));
			grcDebugDraw::Text(Coors2, Color_yellow, "NODE_NEW");
		}

		if(iFutureNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED && pStoredNodes[iFutureNode] && pStoredNodes[iTargetNode] && pNodeList->GetPathLinkIndex(iFutureNode) >= 0)
		{
			Vector3	Coors;
			pStoredNodes[iFutureNode]->GetCoors(Coors);
			grcDebugDraw::Line(Coors, Coors + Vector3(0.0f,0.0f,0.5f), Color32(0, 255, 0, 255));
			Vector3	Coors2;
			pIntelligence->FindTargetCoorsWithNode(m_Vehicle, iNodes[iTargetNode], iNodes[iFutureNode], iNodes[iTargetNode],
				pNodeList->GetPathLinkIndex(iTargetNode), pNodeList->GetPathLinkIndex(iFutureNode),
				pNodeList->GetPathLaneIndex(iFutureNode), pNodeList->GetPathLaneIndex(iTargetNode), Coors2);
			grcDebugDraw::Line(vVehiclePosition, Coors2, Color32(0, 255, 0, 255));

			grcDebugDraw::Text(Coors2, Color_green, "NODE_FUTURE");
		}

		s32 n;
		for(n = iFutureNode+1; n < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
		{
			Vector3 vLastCoord(0.0f,0.0f,0.0f);
			if(pStoredNodes[n] && pStoredNodes[n-1] && pNodeList->GetPathLinkIndex(n) >= 0)
			{
				Color32	Colour;
				Vector3	Coors2;
				int val =  static_cast<int>(255.0f *((n -(iFutureNode+1))) /(CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED -(iFutureNode+1)));
				Colour.Set(0, 255, val, 255);
				Vector3	Coors;
				pStoredNodes[n]->GetCoors(Coors);
				grcDebugDraw::Line(Coors, Coors + Vector3(0.0f,0.0f,0.5f), Colour);

				if(n < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED - 1)
				{
					pIntelligence->FindTargetCoorsWithNode(m_Vehicle, iNodes[n - 1], iNodes[n], iNodes[n + 1],
						pNodeList->GetPathLinkIndex(n - 1), pNodeList->GetPathLinkIndex(n),
						pNodeList->GetPathLaneIndex(n), pNodeList->GetPathLaneIndex(n + 1), Coors2);

					grcDebugDraw::Text(Coors2, Color_green, "FUTURE");
				}
				else
				{
					pIntelligence->FindTargetCoorsWithNode(m_Vehicle, iNodes[n - 1], iNodes[n], EmptyNodeAddress,
						pNodeList->GetPathLinkIndex(n - 1), 0,
						pNodeList->GetPathLaneIndex(n), 0, Coors2);
				}

				if(vLastCoord.IsClose(VEC3_ZERO, 0.01f))
				{
					grcDebugDraw::Line(vVehiclePosition, Coors2, Colour);
				}
				else
				{
					grcDebugDraw::Line(vLastCoord, Coors2, Colour);
				}

				vLastCoord = Coors2;
			}
		}
		char laneText[32];
		Vector3 vTextPos;
		for(n=0; n<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
		{
			if(!pNodeList->GetPathNodeAddr(n).IsEmpty() && pStoredNodes[n])
			{
				s32 iLane = pNodeList->GetPathLaneIndex(n);
				sprintf(laneText, "lane:%i", iLane);
				pStoredNodes[n]->GetCoors(vTextPos);
				vTextPos.z += 1.0f;
				grcDebugDraw::Text(vTextPos,Color_green, laneText);
			}
		}
	}

	const CVehicleFollowRouteHelper* pFollowRouteHelper = pIntelligence->GetFollowRouteHelper();
	if(CVehicleIntelligence::m_bDisplayDebugFutureFollowRoute && pFollowRouteHelper)
	{
		const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());	
		s32 iTargetPoint = pFollowRouteHelper->GetCurrentTargetPoint();
		s32 iNumPoints = pFollowRouteHelper->GetNumPoints();

		const Vector3 vRaiseAmt(0.0f,0.0f,0.5f);

		static CRoutePoint aRoutePoints[CVehicleFollowRouteHelper::MAX_ROUTE_SIZE];
		static Vector3 aStoredPoints[CVehicleFollowRouteHelper::MAX_ROUTE_SIZE];

		for(s32 n=0; n<iNumPoints; n++)
		{
			aRoutePoints[n] = pFollowRouteHelper->GetRoutePoints()[n];
			aStoredPoints[n] = VEC3V_TO_VECTOR3(aRoutePoints[n].GetPosition() + aRoutePoints[n].GetLaneCenterOffset());
			grcDebugDraw::Line(aStoredPoints[n], aStoredPoints[n] + vRaiseAmt, Color_green, Color_green);

			char buffer[32];
			sprintf(buffer, "Required Lane: %i", aRoutePoints[n].m_iRequiredLaneIndex);
			grcDebugDraw::Text(aStoredPoints[n], aRoutePoints[n].m_bIgnoreForNavigation ? Color_white : Color_red, 0, grcDebugDraw::GetScreenSpaceTextHeight(), buffer);

			sprintf(buffer, "Lane Offset: %.3f", aRoutePoints[n].GetLane().Getf());
			grcDebugDraw::Text(aStoredPoints[n], Color_red, 0, grcDebugDraw::GetScreenSpaceTextHeight() * 2, buffer);
		}

		s32 iHistoryPoint2 = iTargetPoint-4;
		if(iHistoryPoint2 >= 0 )
		{
			grcDebugDraw::Line(vVehiclePosition, aStoredPoints[iHistoryPoint2], Color_magenta);
			grcDebugDraw::Text(aStoredPoints[iHistoryPoint2], Color_magenta, "POINT_HISTORY_2");
		}
		s32 iHistoryPoint1 = iTargetPoint-3;
		if(iHistoryPoint1 >= 0 )
		{
			grcDebugDraw::Line(vVehiclePosition, aStoredPoints[iHistoryPoint1], Color_magenta1);
			grcDebugDraw::Text(aStoredPoints[iHistoryPoint1], Color_magenta1, "POINT_HISTORY_1");
		}
		s32 iVeryOldPoint = iTargetPoint-2;
		if(iVeryOldPoint >= 0)
		{
			grcDebugDraw::Line(vVehiclePosition, aStoredPoints[iVeryOldPoint], Color_red);
			grcDebugDraw::Text(aStoredPoints[iVeryOldPoint], Color_red, "POINT_VERY_OLD");
		}
		s32 iOldPoint = iTargetPoint-1;
		if(iOldPoint >= 0)
		{
			grcDebugDraw::Line(vVehiclePosition, aStoredPoints[iOldPoint], Color_orange);
			grcDebugDraw::Text(aStoredPoints[iOldPoint], Color_orange, "POINT_OLD");
		}

		if(iTargetPoint < iNumPoints)
		{
			grcDebugDraw::Line(aStoredPoints[iTargetPoint], aStoredPoints[iTargetPoint] + Vector3(0.0f,0.0f,0.5f), Color32(255, 255, 0, 255));
			grcDebugDraw::Line(vVehiclePosition, aStoredPoints[iTargetPoint], Color32(255, 255, 0, 255));
			grcDebugDraw::Text(aStoredPoints[iTargetPoint], Color_yellow, "POINT_TARGET");
		}

		s32 iFuturePoint = iTargetPoint + 1;
		if(iFuturePoint < iNumPoints)
		{
			grcDebugDraw::Line(aStoredPoints[iFuturePoint], aStoredPoints[iFuturePoint] + Vector3(0.0f,0.0f,0.5f), Color32(255, 255, 0, 255));
			grcDebugDraw::Line(vVehiclePosition, aStoredPoints[iFuturePoint], Color32(0, 255, 0, 255));
			grcDebugDraw::Text(aStoredPoints[iFuturePoint], Color_green, "POINT_FUTURE");
		}

		s32 n;
		for(n = iFuturePoint+1; n < iNumPoints; n++)
		{
			grcDebugDraw::Line(aStoredPoints[n], aStoredPoints[n] + Vector3(0.0f,0.0f,0.5f), Color32(255, 255, 0, 255));
			grcDebugDraw::Line(vVehiclePosition, aStoredPoints[n], Color32(0, 255, 0, 255));
			grcDebugDraw::Text(aStoredPoints[n], Color_green, "FUTURE");
		}
	}
}

void CVehicleNodeListDebugInfo::PrintFollowRoute(bool bPrintAll)
{
	const CVehicleFollowRouteHelper * pFollowRouteHelper = m_Vehicle->GetIntelligence()->GetFollowRouteHelper();
	if((bPrintAll || CVehicleIntelligence::m_bDisplayDebugFutureFollowRoute ) && pFollowRouteHelper)
	{
		WriteHeader("FOLLOW ROUTE NODES");
		PushIndent();
	
		s32 iTargetPoint = pFollowRouteHelper->GetCurrentTargetPoint();
		s32 iNumPoints = pFollowRouteHelper->GetNumPoints();

		for(s32 n=0; n<iNumPoints; n++)
		{
			const CRoutePoint pRoutePoint = pFollowRouteHelper->GetRoutePoints()[n];
			PrintLn("Point %i: (%i:%d) Lane: %i (Required: %i) Offset: %.2f %s", n, pRoutePoint.GetNodeAddress().GetRegion(),pRoutePoint.GetNodeAddress().GetIndex(),
						rage::round(pRoutePoint.GetLane().Getf()), pRoutePoint.m_iRequiredLaneIndex, pRoutePoint.GetLane().Getf(), n==iTargetPoint?(" (Target)"):"");
		}
		PopIndent();
	}
}

void CVehicleNodeListDebugInfo::PrintFutureNodes(bool bPrintAll)
{
	CVehicleNodeList * pNodeList = m_Vehicle->GetIntelligence()->GetNodeList();
	if((bPrintAll || CVehicleIntelligence::m_bDisplayDebugFutureNodes ) && pNodeList)
	{
		WriteHeader("FUTURE NODES");
		PushIndent();

		s32 iTargetNode = pNodeList->GetTargetNodeIndex();
		s32 iNumNodes = pNodeList->FindLastGoodNode();

		for(s32 n = 0; n < iNumNodes; n++)
		{
			const CPathNode* pPathNode = pNodeList->GetPathNode(n);
			if(pPathNode)
			{
				PrintLn("Point %i: (%i:%d)%s", n, pPathNode->GetAddrRegion(), pPathNode->GetAddrIndex(), n==iTargetNode?(" (Target)"):"");
			}
			else
			{
				PrintLn("Point %i: NULL", n);
			}
		}
		PopIndent();		
	}
}

#endif