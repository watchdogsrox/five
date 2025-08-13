// Rage headers
#include "fwscene/world/worldlimits.h"
#include "script/wrapper.h"

// Game headers
#include "control/gps.h"
#include "Network/Network.h"
#include "network/networkinterface.h"
#include "pathserver/pathserver.h"
#include "peds/pedpopulation.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/gameworld.h"
#include "scene/world/gameworldheightmap.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "vehicleai/junctions.h"
#include "vehicleai/pathfind.h"
#include "vehicleai/task/TaskVehicleThreePointTurn.h"
#include "ai/debug/system/AIDebugLogManager.h"

SCRIPT_OPTIMISATIONS ()
AI_OPTIMISATIONS ()
AI_VEHICLE_OPTIMISATIONS ()

namespace path_commands
{


	void SetRoadsInArea (const scrVector & scrVecMin, const scrVector & scrVecMax, bool bActive, bool bNetwork)
	{
		float temp_float;
		Vector3 VecPosMin = Vector3 (scrVecMin);
		Vector3 VecPosMax = Vector3 (scrVecMax);
		//	Maybe allow "main" script to call this command too
		//	Keith needs to call this in other scripts to turn bridges off/on so I've removed this assert

		if (VecPosMin.x > VecPosMax.x)
		{
			temp_float = VecPosMin.x;
			VecPosMin.x = VecPosMax.x;
			VecPosMax.x = temp_float;
		}

		if (VecPosMin.y > VecPosMax.y)
		{
			temp_float = VecPosMin.y;
			VecPosMin.y = VecPosMax.y;
			VecPosMax.y = temp_float;
		}

		if (VecPosMin.z > VecPosMax.z)
		{
			temp_float = VecPosMin.z;
			VecPosMin.z = VecPosMax.z;
			VecPosMax.z = temp_float;
		}

		if (bActive)
		{
			ThePaths.SwitchRoadsOffInArea(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), VecPosMin.x, VecPosMax.x, VecPosMin.y, VecPosMax.y, VecPosMin.z, VecPosMax.z, FALSE, true, false, CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript);

#if !__FINAL
			scriptDebugf1("%s: Switching road on in area locally: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
				CTheScripts::GetCurrentScriptNameAndProgramCounter(),
				VecPosMin.x, VecPosMin.y, VecPosMin.z,
				VecPosMax.x, VecPosMax.y, VecPosMax.z);
			scrThread::PrePrintStackTrace();
#endif // !__FINAL

			if(NetworkInterface::IsGameInProgress() && bNetwork)
			{
#if !__FINAL
                scriptDebugf1("%s: Switching road on in area over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
                    CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                    VecPosMin.x, VecPosMin.y, VecPosMin.z,
                    VecPosMax.x, VecPosMax.y, VecPosMax.z);
                scrThread::PrePrintStackTrace();
#endif // !__FINAL
				NetworkInterface::SwitchRoadNodesOnOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecPosMin, VecPosMax);
			}
		}
		else
		{
			ThePaths.SwitchRoadsOffInArea(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), VecPosMin.x, VecPosMax.x, VecPosMin.y, VecPosMax.y, VecPosMin.z, VecPosMax.z, TRUE, true, false, CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript);

#if !__FINAL
			scriptDebugf1("%s: Switching road off in area locally: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
				CTheScripts::GetCurrentScriptNameAndProgramCounter(),
				VecPosMin.x, VecPosMin.y, VecPosMin.z,
				VecPosMax.x, VecPosMax.y, VecPosMax.z);
			scrThread::PrePrintStackTrace();
#endif // !__FINAL

			if(NetworkInterface::IsGameInProgress() && bNetwork)
			{
#if !__FINAL
                scriptDebugf1("%s: Switching road off in area over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
                    CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                    VecPosMin.x, VecPosMin.y, VecPosMin.z,
                    VecPosMax.x, VecPosMax.y, VecPosMax.z);
                scrThread::PrePrintStackTrace();
#endif // !__FINAL
				NetworkInterface::SwitchRoadNodesOffOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecPosMin, VecPosMax);
			}
		}
	}


	void SetRoadsInAngledArea (const scrVector & scrVecCoors1, const scrVector & scrVecCoors2, float AreaWidth, bool UNUSED_PARAM(HighlightArea), bool bActive, bool bNetwork)
	{
		Vector3 EntityPos;

		Vector3 vStart = Vector3 (scrVecCoors1);
		Vector3 vEnd = Vector3 (scrVecCoors2);

		if (bActive)
		{
#if !__FINAL
			scriptDebugf1("%s: Switching road on in angled area locally: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) Width: %.2f",
				CTheScripts::GetCurrentScriptNameAndProgramCounter(),
				vStart.x, vStart.y, vStart.z,
				vEnd.x, vEnd.y, vEnd.z,
				AreaWidth);
			scrThread::PrePrintStackTrace();
#endif // !__FINAL

			ThePaths.SwitchRoadsOffInAngledArea(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), vStart, vEnd, AreaWidth, FALSE, true, false, CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript);

			if(NetworkInterface::IsGameInProgress() && bNetwork)
			{
#if !__FINAL
                scriptDebugf1("%s: Switching road on in angled area over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) Width: %.2f",
                    CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                    vStart.x, vStart.y, vStart.z,
                    vEnd.x, vEnd.y, vEnd.z,
                    AreaWidth);
                scrThread::PrePrintStackTrace();
#endif // !__FINAL
				NetworkInterface::SwitchRoadNodesOnOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), vStart, vEnd, AreaWidth);
			}
		}
		else
		{
#if __BANK
			scriptDebugf1("%s: Switching road off in angled area locally: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) Width: %.2f",
				CTheScripts::GetCurrentScriptNameAndProgramCounter(),
				vStart.x, vStart.y, vStart.z,
				vEnd.x, vEnd.y, vEnd.z,
				AreaWidth);
			scrThread::PrePrintStackTrace();
#endif // __BANK

			ThePaths.SwitchRoadsOffInAngledArea(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), vStart, vEnd, AreaWidth, TRUE, true, false, CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript);

			if(NetworkInterface::IsGameInProgress() && bNetwork)
			{
#if __BANK
                scriptDebugf1("%s: Switching road off in angled area over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) Width: %.2f",
                    CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                    vStart.x, vStart.y, vStart.z,
                    vEnd.x, vEnd.y, vEnd.z,
                    AreaWidth);
                scrThread::PrePrintStackTrace();
#endif // __BANK
				NetworkInterface::SwitchRoadNodesOffOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), vStart, vEnd, AreaWidth);
			}
		}
	}



void CommandSetPedPathsInArea (const scrVector & scrVecMin, const scrVector & scrVecMax, bool bActive, bool bForceAbortCurrentPath)
{
	// Optionally abort any path-requests which might otherwise occupy the pathserver thread
	if(bForceAbortCurrentPath)
		CPathServer::ForceAbortCurrentPathRequest();

	Vector3 VecPosMin, VecPosMax;

	VecPosMin.x = Min(scrVecMin.x, scrVecMax.x);
	VecPosMin.y = Min(scrVecMin.y, scrVecMax.y);
	VecPosMin.z = Min(scrVecMin.z, scrVecMax.z);
	
	VecPosMax.x = Max(scrVecMin.x, scrVecMax.x);
	VecPosMax.y = Max(scrVecMin.y, scrVecMax.y);
	VecPosMax.z = Max(scrVecMin.z, scrVecMax.z);

	if(SCRIPT_VERIFY((VecPosMax-VecPosMin).Mag() > 0.1f, "Invalid (zero-sized) area specified in SET_PED_PATHS_IN_AREA"))
	{
		if (bActive)
		{	
			CPathServer::SwitchPedsOnInArea(VecPosMin, VecPosMax, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
		}
		else
		{
			CPathServer::SwitchPedsOffInArea(VecPosMin, VecPosMax, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
		}

		CTheScripts::GetCurrentGtaScriptThread()->SetHasUsedPedPathCommand(true);
	}
}

void CommandDisableNavMeshInArea (const scrVector & scrVecMin, const scrVector & scrVecMax, bool bDisable )
{
	Vector3 VecPosMin, VecPosMax;

	VecPosMin.x = Min(scrVecMin.x, scrVecMax.x);
	VecPosMin.y = Min(scrVecMin.y, scrVecMax.y);
	VecPosMin.z = Min(scrVecMin.z, scrVecMax.z);

	VecPosMax.x = Max(scrVecMin.x, scrVecMax.x);
	VecPosMax.y = Max(scrVecMin.y, scrVecMax.y);
	VecPosMax.z = Max(scrVecMin.z, scrVecMax.z);

	if(SCRIPT_VERIFY((VecPosMax-VecPosMin).Mag() > 0.1f, "Invalid (zero-sized) area specified in DISABLE_NAVMESH_IN_AREA"))
	{
		if (bDisable)
		{	
			CPathServer::DisableNavMeshInArea(VecPosMin, VecPosMax, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
		}
		else
		{
			CPathServer::ReEnableNavMeshInArea(VecPosMin, VecPosMax, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
		}

		CTheScripts::GetCurrentGtaScriptThread()->SetHasUsedPedPathCommand(true);
	}
}


enum GetSafePositionFlags
{
	// Only navmesh polygons marked as pavement
	GSC_FLAG_ONLY_PAVEMENT					= 1,
	// Only navmesh polygons not marked as "isolated"
	GSC_FLAG_NOT_ISOLATED					= 2,
	// No navmesh polygons created from interiors
	GSC_FLAG_NOT_INTERIOR					= 4,
	// No navmesh polygons marked as water
	GSC_FLAG_NOT_WATER						= 8,
	// Only navmesh polygons marked as "network spawn candidate"
	GSC_FLAG_ONLY_NETWORK_SPAWN				= 16,
	// Specify whether to use a flood-fill from the starting position, as opposed to scanning all polygons within the search volume
	GSC_FLAG_USE_FLOOD_FILL					= 32
};



// This function replaces CommandGetClosestCharNode. It uses the NavMesh code.
// Return Coords will be the same as Input Coords if this function returns false
bool CommandGetSafePositionForPed(const scrVector & scrVecCoors, bool bOnlyOnPavement, Vector3& vecReturn, int iGetSafePositionFlags)
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	if (VecPos.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecPos.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, VecPos.x, VecPos.y);
	}

	const Vector3 vecIn(VecPos.x, VecPos.y, VecPos.z);

	static const float fFindPolySearchDist = 25.0f;

	u32 iFlags = GetClosestPos_ClearOfObjects;
	if(bOnlyOnPavement)
		iFlags |= GetClosestPos_OnlyPavement;

	if((iGetSafePositionFlags & GSC_FLAG_ONLY_PAVEMENT)!=0)
		iFlags |= GetClosestPos_OnlyPavement;
	if((iGetSafePositionFlags & GSC_FLAG_NOT_ISOLATED)!=0)
		iFlags |= GetClosestPos_OnlyNonIsolated;
	if((iGetSafePositionFlags & GSC_FLAG_NOT_INTERIOR)!=0)
		iFlags |= GetClosestPos_NotInteriors;
	if((iGetSafePositionFlags & GSC_FLAG_NOT_WATER)!=0)
		iFlags |= GetClosestPos_NotWater;
	if((iGetSafePositionFlags & GSC_FLAG_ONLY_NETWORK_SPAWN)!=0)
		iFlags |= GetClosestPos_OnlyNetworkSpawn;
	if ((iGetSafePositionFlags & GSC_FLAG_USE_FLOOD_FILL) != 0)
		iFlags |= GetClosestPos_UseFloodFill;

	EGetClosestPosRet eRet = CPathServer::GetClosestPositionForPed(vecIn, vecReturn, fFindPolySearchDist, iFlags);
	bool LatestCmpFlagResult = (eRet!=ENoPositionFound);

	return LatestCmpFlagResult;
}

enum GetClosestNodeFlags
{
	GCNF_INCLUDE_SWITCHED_OFF_NODES				= 0x1,
	GCNF_INCLUDE_BOAT_NODES						= 0x2,
	GCNF_IGNORE_SLIPLANES						= 0x4,
	GCNF_IGNORE_SWITCHED_OFF_DEADENDS			= 0x8,

	// Not in the script enum
	GCNF_GET_HEADING							= 0x100,
	GCNF_FAVOUR_FACING							= 0x200
};

 
bool BaseGetClosestNode(const Vector3 &VecPos_, const Vector3 &VecFace_, Vector3& vecReturn, float & headingReturn, 
						int & numLanes, int nodeNum, int nodeFlags, CNodeAddress &ResultNode, const float zMeasureMult, const float zTolerance)
{
	Vector3 VecPos = VecPos_;
	ResultNode.SetEmpty();
	bool LatestCmpFlagResult;

	float DirX(0.0f);
	float DirY(0.0f);

	const bool bIgnoreSliplanes = (nodeFlags & GCNF_IGNORE_SLIPLANES) != 0;
	const bool bIgnoreSwitchedOffDeadEnds = (nodeFlags & GCNF_IGNORE_SWITCHED_OFF_DEADENDS) != 0;

	if ( nodeFlags & GCNF_FAVOUR_FACING )
	{
		Vector3 FaceCoors = VecFace_;
		DirX = FaceCoors.x - VecPos.x;
		DirY = FaceCoors.y - VecPos.y;
		float Length = rage::Sqrtf(DirX*DirX + DirY*DirY);
		if ( SCRIPT_VERIFY( (Length != 0.0f), "GET..._NODE... - A favoured aim at point was specified that exactly matched the specified point."))
		{
			DirX /= Length;
			DirY /= Length;
		}
		else
		{
			nodeFlags &= ~GCNF_FAVOUR_FACING;
		}
	}

	if (VecPos.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecPos.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecPos.x, VecPos.y);
	}

	if ( nodeNum == 0 )
	{
		CPathFind::NodeConsiderationType eNodeConsiderationType = 
			(nodeFlags & GCNF_INCLUDE_SWITCHED_OFF_NODES ) ? CPathFind::IncludeSwitchedOffNodes : CPathFind::IgnoreSwitchedOffNodes;

		ResultNode = ThePaths.FindNodeClosestToCoors(VecPos, 999999.9f, eNodeConsiderationType,
			false, (nodeFlags & GCNF_INCLUDE_BOAT_NODES) ? true : false, false, false, 
			(nodeFlags & GCNF_FAVOUR_FACING) ? true : false, DirX, DirY, zMeasureMult, zTolerance, -1, false, false, NULL, bIgnoreSliplanes, bIgnoreSwitchedOffDeadEnds);

	}
	else
	{
		bool bIgnoreSwitchedOffNodes = (nodeFlags & GCNF_INCLUDE_SWITCHED_OFF_NODES ) ? false : true;
		ResultNode = ThePaths.FindNthNodeClosestToCoors(VecPos, 999999.9f, bIgnoreSwitchedOffNodes, (nodeNum - 1), 
			(nodeFlags & GCNF_INCLUDE_BOAT_NODES) ? true : false, NULL, 
			(nodeFlags & GCNF_FAVOUR_FACING) ? true : false, DirX, DirY, zMeasureMult, zTolerance, -1, bIgnoreSliplanes, bIgnoreSwitchedOffDeadEnds);
	}
	bool bSuccess;
	Vector3 Temp = ThePaths.FindNodeCoorsForScript(ResultNode, &bSuccess);
	headingReturn = 0.0f;
	numLanes = 0;
	if (bSuccess)
	{
		vecReturn = Temp;
		LatestCmpFlagResult = true;
		if ( nodeFlags & GCNF_GET_HEADING )
		{
			// Try and find the orientation that makes most sense.
			float	dirX;
			float	dirY;
			if ( nodeFlags & GCNF_FAVOUR_FACING )
			{
				dirX = DirX;
				dirY = DirY;
			}
			else
			{
				Vector3 nodeCoors;
				ThePaths.FindNodePointer(ResultNode)->GetCoors(nodeCoors);
				dirX = nodeCoors.x - VecPos.x;
				dirY = nodeCoors.y - VecPos.y;

			}
			headingReturn = ThePaths.FindNodeOrientationForCarPlacement(ResultNode, dirX, dirY, &numLanes );
		}
	}
	else
	{
		vecReturn.Zero();
		LatestCmpFlagResult = false;
	}
	return LatestCmpFlagResult;
}

bool CommandGetClosestVehicleNode(const scrVector & scrVecCoors, Vector3 &vecReturn, int nodeFlags, float zMeasureMult, float zTolerance)
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	Vector3 VecFaceDummy;
	CNodeAddress resultNode;
	float dummyAngle;
	int dummyLanes;
	return BaseGetClosestNode(VecPos, VecFaceDummy, vecReturn, dummyAngle, dummyLanes, 0, nodeFlags, resultNode, zMeasureMult, zTolerance);
}

bool CommandGetClosestMajorVehicleNode(const scrVector & scrVecCoors, Vector3 &vecReturn, float zMeasureMult, float zTolerance)
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	Vector3 VecFaceDummy;
	CNodeAddress resultNode;
	float dummyAngle;
	int dummyLanes;
	//	For prostitute script, we want to find the closest road node that is switched on and only start the script if 
	//	it is far away
	return BaseGetClosestNode(VecPos, VecFaceDummy, vecReturn, dummyAngle, dummyLanes, 0, GCNF_INCLUDE_SWITCHED_OFF_NODES, resultNode, zMeasureMult, zTolerance);
}

bool CommandGetVehicleNodeProperties(const scrVector & vInCoords, int & Out_iDensity, int & Out_iPropertyFlags)
{
#if __ASSERT
	Vector3 vAssertPos(vInCoords);
	Assertf( !vAssertPos.IsClose(VEC3_ZERO, 0.1f), "You are passing in (0,0,0) to GET_VEHICLE_NODE_PROPERTIES");
#endif

	Out_iDensity = 0;
	Out_iPropertyFlags = 0;

	// Be sure to keep this enum in sync with VEHICLE_NODE_PROPERTIES in "commands_path.sch"
	enum
	{
		VNP_OFF_ROAD				= 1,					// node has been flagged as 'off road', suitable only for 4x4 vehicles, etc
		VNP_ON_PLAYERS_ROAD			= 2,					// node has been dynamically marked as somewhere ahead, possibly on (or near to) the player's current road
		VNP_NO_BIG_VEHICLES			= 4,					// node has been marked as not suitable for big vehicles
		VNP_SWITCHED_OFF			= 8,					// node is switched off for ambient population
		VNP_TUNNEL_OR_INTERIOR		= 16,					// node is in a tunnel or an interior
		VNP_LEADS_TO_DEAD_END		= 32,					// node is, or leads to, a dead end
		VNP_HIGHWAY					= 64,					// node is marked as highway
		VNP_JUNCTION				= 128,					// node qualifies as junction
		VNP_TRAFFIC_LIGHT			= 256,					// node's special function is traffic-light
		VNP_GIVE_WAY				= 512,					// node's special function is give-way
		VNP_WATER					= 1024					// node is water/boat
	};

	CNodeAddress nodeAddress = ThePaths.FindNodeClosestToCoors(vInCoords, 9999.0f, CPathFind::IncludeSwitchedOffNodes);
	if(nodeAddress.IsEmpty())
		return false;

	const CPathNode * pPathNode = ThePaths.FindNodePointerSafe(nodeAddress);
	if(!pPathNode)
		return false;

	Out_iDensity = pPathNode->m_2.m_density;

	if(pPathNode->m_1.m_Offroad) Out_iPropertyFlags |= VNP_OFF_ROAD;
	if(pPathNode->m_1.m_onPlayersRoad) Out_iPropertyFlags |= VNP_ON_PLAYERS_ROAD;
	if(pPathNode->m_1.m_noBigVehicles) Out_iPropertyFlags |= VNP_NO_BIG_VEHICLES;
	if(pPathNode->m_2.m_switchedOff) Out_iPropertyFlags |= VNP_SWITCHED_OFF;
	if(pPathNode->m_2.m_inTunnel) Out_iPropertyFlags |= VNP_TUNNEL_OR_INTERIOR;
	if(pPathNode->m_2.m_deadEndness != 0) Out_iPropertyFlags |= VNP_LEADS_TO_DEAD_END;
	if(pPathNode->IsHighway()) Out_iPropertyFlags |= VNP_HIGHWAY;
	if(pPathNode->IsJunctionNode()) Out_iPropertyFlags |= VNP_JUNCTION;
	if(pPathNode->m_1.m_specialFunction == SPECIAL_TRAFFIC_LIGHT) Out_iPropertyFlags |= VNP_TRAFFIC_LIGHT;
	if(pPathNode->m_1.m_specialFunction == SPECIAL_GIVE_WAY) Out_iPropertyFlags |= VNP_GIVE_WAY;
	if(pPathNode->m_2.m_waterNode) Out_iPropertyFlags |= VNP_WATER;

	return true;
}

bool CommandGetClosestVehicleNodeWithHeading(const scrVector & scrVecCoors, Vector3 &vecReturn, float &ReturnHeading, int nodeFlags, float zMeasureMult, float zTolerance)
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	Vector3 VecFaceDummy;
	CNodeAddress resultNode;
	int dummyLanes;
	return BaseGetClosestNode( VecPos, VecFaceDummy, vecReturn, ReturnHeading, dummyLanes, 0, nodeFlags | GCNF_GET_HEADING, resultNode, zMeasureMult, zTolerance);
}

bool CommandGetNthClosestVehicleNode(const scrVector & scrVecCoors, int NodeNumber, Vector3 &vecReturn, int nodeFlags, float zMeasureMult, float zTolerance)
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	Vector3 VecFaceDummy;
	CNodeAddress resultNode;
	float dummyAngle;
	int dummyLanes;
	return BaseGetClosestNode( VecPos, VecFaceDummy, vecReturn, dummyAngle, dummyLanes, NodeNumber, nodeFlags, resultNode, zMeasureMult, zTolerance );
}

s32 CommandGetNthClosestVehicleNodeId(const scrVector & scrVecCoors, int NodeNumber, int nodeFlags, float zMeasureMult, float zTolerance)
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	Vector3 VecFaceDummy;
	float dummyAngle;
	CNodeAddress resultNode;
	int dummyLanes;
	Vector3 posDummy;
	BaseGetClosestNode( VecPos, VecFaceDummy, posDummy, dummyAngle, dummyLanes, NodeNumber, nodeFlags, resultNode, zMeasureMult, zTolerance);
	s32 ret;
	resultNode.ToInt(ret);
	return ret;
}

bool CommandGetNthClosestVehicleNodeWithHeading(const scrVector & scrVecCoors, int NodeNumber, Vector3 &vecReturn, float &ReturnHeading, int &ReturnNumLanes, int nodeFlags, float zMeasureMult, float zTolerance)

{
	Vector3 VecPos = Vector3 (scrVecCoors);
	Vector3 VecFaceDummy;
	CNodeAddress resultNode;
	return BaseGetClosestNode( VecPos, VecFaceDummy, vecReturn, ReturnHeading, ReturnNumLanes, NodeNumber, nodeFlags | GCNF_GET_HEADING, resultNode, zMeasureMult, zTolerance);
}

s32 CommandGetNthClosestVehicleNodeIdWithHeading(const scrVector & scrVecCoors, int NodeNumber, float &ReturnHeading, int &ReturnNumLanes, int nodeFlags, float zMeasureMult, float zTolerance )
{
	Vector3 VecPos = Vector3 (scrVecCoors);
	Vector3 VecFaceDummy;
	Vector3 posDummy;
	CNodeAddress resultNode;
	BaseGetClosestNode( VecPos, VecFaceDummy, posDummy, ReturnHeading, ReturnNumLanes, NodeNumber, nodeFlags | GCNF_GET_HEADING, resultNode, zMeasureMult, zTolerance );
	s32 ret;
	resultNode.ToInt(ret);
	return ret;
}

bool CommandGetNthClosestVehicleNodeWithHeadingOnIsland(const scrVector & scrVecCoors, int NodeNumber, int mapAreaIndex/*Island*/, Vector3 &vecReturn, float &ReturnHeading, int &numLanes, float zMeasureMult, float zTolerance)
{
	CNodeAddress ResultNode;
	bool LatestCmpFlagResult = false;
	Vector3 VecPos = Vector3 (scrVecCoors);

	if (VecPos.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecPos.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecPos.x, VecPos.y);
	}
	if(SCRIPT_VERIFY( (NodeNumber > 0), "GET_NTH_CLOSEST_Vehicle_NODE_WITH_HEADING - Node number must be greater than 0"))
	{
		ResultNode = ThePaths.FindNthNodeClosestToCoors(VecPos, 999999.9f, false, (NodeNumber - 1), false, NULL, false, 0, 0, zMeasureMult, zTolerance, mapAreaIndex);

		bool bSuccess;
		Vector3	Temp = ThePaths.FindNodeCoorsForScript(ResultNode, &bSuccess);
		if (bSuccess)
		{
			vecReturn = Temp;
			ReturnHeading = ThePaths.FindNodeOrientationForCarPlacement(ResultNode, 0.0f, 0.0f, &numLanes);
			LatestCmpFlagResult = true;
		}
		else
		{
			vecReturn.Zero();
			ReturnHeading = 0.0f;
			numLanes = 0;
			LatestCmpFlagResult = false;
		}
	}
	return LatestCmpFlagResult;
}

bool CommandGetNextClosestVehicleNodeWithHeadingOnIsland(const scrVector & scrVecCoors, int mapAreaIndex/*Island*/, Vector3 &vecReturn, float &ReturnHeading, float zMeasureMult, float zTolerance)
{
	CNodeAddress ResultNode;
	bool LatestCmpFlagResult;
	Vector3 VecPos = Vector3 (scrVecCoors);

	if (VecPos.z <= INVALID_MINIMUM_WORLD_Z)
	{
		VecPos.z = WorldProbe::FindGroundZForCoord(TOP_SURFACE, VecPos.x, VecPos.y);
	}

	ResultNode = ThePaths.FindNextNodeClosestToCoors(VecPos, 999999.9f, false, false, false, 0, 0, zMeasureMult, zTolerance, mapAreaIndex );

	bool bSuccess;
	Vector3	Temp = ThePaths.FindNodeCoorsForScript(ResultNode, &bSuccess);
	if (bSuccess)
	{
		vecReturn = Temp;
		ReturnHeading = ThePaths.FindNodeOrientationForCarPlacement(ResultNode);
		LatestCmpFlagResult = true;
	}
	else
	{
		vecReturn.Zero();
		ReturnHeading = 0.0f;
		LatestCmpFlagResult = false;
	}
	return LatestCmpFlagResult;
}

bool CommandGetNthClosestVehicleNodeFavourDirection(const scrVector & scrInCoors, const scrVector & scrFaceCoors, int NodeNumber, Vector3 &ReturnCoors, float &ReturnHeading,
													int nodeFlags, float zMeasureMult, float zTolerance)
{

	Vector3 InCoors = Vector3(scrInCoors);
	Vector3 FaceCoors = Vector3(scrFaceCoors);
	CNodeAddress resultNode;
	int dummyLanes;
	return BaseGetClosestNode( InCoors, FaceCoors, ReturnCoors, ReturnHeading, dummyLanes, NodeNumber, (nodeFlags | GCNF_FAVOUR_FACING | GCNF_GET_HEADING), resultNode, zMeasureMult, zTolerance);
}

bool CommandIsVehicleNodeIdValid(s32 node)
{
	CNodeAddress addy;
	addy.FromInt(node);
	return !addy.IsEmpty();
}

void CommandGetVehicleNodePosition(s32 node, Vector3 &vecReturn)
{
	CNodeAddress addy;
	addy.FromInt(node);
	if ( SCRIPT_VERIFY( !addy.IsEmpty(), "GET_VEHICLE_NODE_POSITION: Command used with an invalid node ID.  Missing a IS_VEHICLE_NODE_ID_VALID?." ))
	{
		const CPathNode *pNode = ThePaths.FindNodePointerSafe(addy);
		pNode->GetCoors(vecReturn);
	}
	else
	{
		vecReturn.Zero();
	}
}

bool CommandGetVehicleNodeIsGpsAllowed(s32 node)
{
	CNodeAddress addy;
	addy.FromInt(node);
	if ( SCRIPT_VERIFY( !addy.IsEmpty(), "GET_VEHICLE_NODE_IS_GPS_ALLOWED: Command used with an invalid node ID.  Missing a IS_VEHICLE_NODE_ID_VALID?." ))
	{
		const CPathNode *pNode = ThePaths.FindNodePointerSafe(addy);
		return !( pNode->m_2.m_noGps);
	}
	else
	{
		return true;
	}
}

bool CommandGetVehicleNodeIsSwitchedOff(s32 node)
{
	CNodeAddress addy;
	addy.FromInt(node);
	if ( SCRIPT_VERIFY( !addy.IsEmpty(), "GET_VEHICLE_NODE_IS_SWITCHED_OFF: Command used with an invalid node ID.  Missing a IS_VEHICLE_NODE_ID_VALID?." ))
	{
		const CPathNode *pNode = ThePaths.FindNodePointerSafe(addy);
		return ( pNode->m_2.m_switchedOff);
	}
	else
	{
		return true;
	}
}

bool CommandGetClosestRoad(const scrVector & scrTestCoors, float MinLength, int MinLanes, Vector3 &SouthEndNode, Vector3 &NorthEndNode, int &LanesGoingSouth, int &LanesGoingNorth, float &CentralReservationWidth, bool bIgnoreSwitchedOffNodes)
{
	CNodeAddress NodeSouth, NodeNorth;
	float Orientation;

	Vector3 TestCoors = Vector3(scrTestCoors);

	ThePaths.FindNodePairClosestToCoors(TestCoors, &NodeSouth, &NodeNorth, &Orientation, MinLength, 99999.9f, bIgnoreSwitchedOffNodes, false, false, false, MinLanes, &LanesGoingNorth, &LanesGoingSouth, &CentralReservationWidth);

	const CPathNode	*pNodeS = ThePaths.FindNodePointerSafe(NodeSouth);
	const CPathNode	*pNodeN = ThePaths.FindNodePointerSafe(NodeNorth);

	if (!pNodeS || !pNodeN)
	{
		return false;
	}

	pNodeS->GetCoors(SouthEndNode);
	pNodeN->GetCoors(NorthEndNode);

	return true;
}

bool CommandLoadAllPathNodes(bool bLoadAll)
{
	if(bLoadAll == false)
	{
		GtaThread::ClearHasLoadedAllPathNodes( ASSERT_ONLY(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId()) );
	}
	else
	{
		GtaThread::SetHasLoadedAllPathNodes( CTheScripts::GetCurrentGtaScriptThread()->GetThreadId() );
	}

	ThePaths.bLoadAllRegions = bLoadAll;

	return (ThePaths.CPathFind::AreNodesLoadedForArea(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_YMAX));
}

void CommandSetAllowStreamPrologueNodes(bool bAllow)
{
	ThePaths.bWillTryAndStreamPrologueNodes = bAllow;
}

void CommandSetAllowStreamHeistIslandNodes(bool bAllow)
{
	ThePaths.bStreamHeistIslandNodes = bAllow;
}

bool CommandAreNodesLoadedForArea(float MinX, float MinY, float MaxX, float MaxY)
{
	return (ThePaths.CPathFind::AreNodesLoadedForArea(MinX, MaxX, MinY, MaxY));
}

bool CommandRequestPathNodesInAreaThisFrame(float MinX, float MinY, float MaxX, float MaxY)
{
	Vector3 vMin(MinX, MinY, 0.0f);
	Vector3 vMax(MaxX, MaxY, 0.0f);
	return ThePaths.AddNodesRequiredRegionThisFrame( CPathFind::CPathNodeRequiredArea::CONTEXT_SCRIPT, vMin, vMax, CTheScripts::GetCurrentGtaScriptThread()->GetScriptName() );
}

void CommandSwitchRoadsBackToOriginal(const scrVector & scrVecMin, const scrVector & scrVecMax, bool bNetwork)
{
	//	Maybe allow "main" script to call this command too
	//	Keith needs to call this in other scripts to turn bridges off/on so I've removed this assert
	Vector3 VecPosMin = Vector3 (scrVecMin);
	Vector3 VecPosMax = Vector3 (scrVecMax);
	
	float temp_float;

	if (VecPosMin.x > VecPosMax.x)
	{
		temp_float = VecPosMin.x;
		VecPosMin.x = VecPosMax.x;
		VecPosMax.x = temp_float;
	}

	if (VecPosMin.y > VecPosMax.y)
	{
		temp_float = VecPosMin.y;
		VecPosMin.y = VecPosMax.y;
		VecPosMax.y = temp_float;
	}

	if (VecPosMin.z > VecPosMax.z)
	{
		temp_float = VecPosMin.z;
		VecPosMin.z = VecPosMax.z;
		VecPosMax.z = temp_float;
	}

#if __BANK
	scriptDebugf1("%s: Switching road back to original in area locally: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
		CTheScripts::GetCurrentScriptNameAndProgramCounter(),
		VecPosMin.x, VecPosMin.y, VecPosMin.z,
		VecPosMax.x, VecPosMax.y, VecPosMax.z);
	scrThread::PrePrintStackTrace();
#endif // __BANK

	ThePaths.SwitchRoadsOffInArea(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), VecPosMin.x, VecPosMax.x, VecPosMin.y, VecPosMax.y, VecPosMin.z, VecPosMax.z, FALSE, TRUE, TRUE, CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript);

	if(NetworkInterface::IsGameInProgress() && bNetwork)
	{
#if __BANK
		scriptDebugf1("%s: Switching road back to original in area over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),
			VecPosMin.x, VecPosMin.y, VecPosMin.z,
			VecPosMax.x, VecPosMax.y, VecPosMax.z);
		scrThread::PrePrintStackTrace();
#endif // __BANK
		NetworkInterface::SetRoadNodesToOriginalOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), VecPosMin, VecPosMax);
	}
}


void CommandSwitchRoadsBackToOriginalInAngledArea(const scrVector & scrVecCoors1, const scrVector & scrVecCoors2, float AreaWidth, bool bNetwork)
{
	Vector3 EntityPos;

	Vector3 vStart = Vector3 (scrVecCoors1);
	Vector3 vEnd = Vector3 (scrVecCoors2);

#if __BANK
	scriptDebugf1("%s: Switching road back to original in area locally: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
		CTheScripts::GetCurrentScriptNameAndProgramCounter(),
		vStart.x, vStart.y, vStart.z,
		vEnd.x, vEnd.y, vEnd.z);
	scrThread::PrePrintStackTrace();
#endif // __BANK

	ThePaths.SwitchRoadsOffInAngledArea(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), vStart, vEnd, AreaWidth, FALSE, TRUE, TRUE, CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript);

	if(NetworkInterface::IsGameInProgress() && bNetwork)
	{
#if __BANK
		scriptDebugf1("%s: Switching road back to original in area over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),
			vStart.x, vStart.y, vStart.z,
			vEnd.x, vEnd.y, vEnd.z);
		scrThread::PrePrintStackTrace();
#endif // __BANK
		NetworkInterface::SetRoadNodesToOriginalOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), vStart, vEnd, AreaWidth);
	}
}

void CommandSwitchPedRoadsBackToOriginal(const scrVector & scrVecMin, const scrVector & scrVecMax, bool bForceAbortCurrentPath)
{
	Vector3 VecPosMin = Vector3 (scrVecMin);
	Vector3 VecPosMax = Vector3 (scrVecMax);
	float temp_float;
	
	if (VecPosMin.x > VecPosMax.x)
	{
		temp_float = VecPosMin.x;
		VecPosMin.x = VecPosMax.x;
		VecPosMax.x = temp_float;
	}

	if (VecPosMin.y > VecPosMax.y)
	{
		temp_float = VecPosMin.y;
		VecPosMin.y = VecPosMax.y;
		VecPosMax.y = temp_float;
	}

	if (VecPosMin.z > VecPosMax.z)
	{
		temp_float = VecPosMin.z;
		VecPosMin.z = VecPosMax.z;
		VecPosMax.z = temp_float;
	}

	const Vector3 MinVec(VecPosMin);
	const Vector3 MaxVec(VecPosMax);

	// Abort any path-requests which might otherwise occupy the pathserver thread
	if(bForceAbortCurrentPath)
		CPathServer::ForceAbortCurrentPathRequest();

#if __BANK
	scriptDebugf1("%s: Switching road back to original (CommandSwitchPedRoadsBackToOriginal): (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f), bForceAbortCurrentPath - %s",
		CTheScripts::GetCurrentScriptNameAndProgramCounter(),
		VecPosMin.x, VecPosMin.y, VecPosMin.z,
		VecPosMax.x, VecPosMax.y, VecPosMax.z,
		AILogging::GetBooleanAsString(bForceAbortCurrentPath));
	scrThread::PrePrintStackTrace();
#endif // __BANK

	CPathServer::ClearPedSwitchAreas(MinVec, MaxVec);
}

void CommandSetAmbientPedRangeMultiplierThisFrame(float fMultiplier)
{
	CPedPopulation::SetScriptedRangeMultiplierThisFrame(fMultiplier);
}

void CommandAdjustAmbientPedSpawnDensitiesThisFrame(int iModifier, const scrVector & vMin, const scrVector & vMax)
{
	Assert(vMax.x > vMin.x);
	Assert(vMax.y > vMin.y);
	Assert(vMax.z > vMin.z);

	Assert(iModifier != 0);
	Assert(iModifier >= -MAX_PED_DENSITY);
	Assert(iModifier <= MAX_PED_DENSITY);

	iModifier = Clamp(iModifier, -MAX_PED_DENSITY, MAX_PED_DENSITY);

	CPedPopulation::SetAdjustPedSpawnPointDensityThisFrame(iModifier, vMin, vMax);
}

void CommandFindStreetNameAtPosition(const scrVector & scrInCoors, int& streetNameHash1, int& streetNameHash2)
{
	Vector3 vecIn = Vector3(scrInCoors);
	u32	hash1, hash2;

	ThePaths.FindStreetNameAtPosition(vecIn, hash1, hash2 );
	streetNameHash1 = (int)hash1;
	streetNameHash2 = (int)hash2;
}

//****************************************************************************
//	CommandAddNavMeshRequiredRegion
//	Adds a region around which navmeshes need to be loaded.  There are a
//	maximum of NAVMESH_MAX_REQUIRED_REGIONS allowed.  They *must* be removed
//	again after use.  Memory consumption is an issue to watch for!
//****************************************************************************
void CommandAddNavMeshRequiredRegion(float fOriginX, float fOriginY, float fRadius)
{
	Displayf("ADD_NAVMESH_REQUIRED_REGION - at (%.1f, %.1f) radius %.1f.  Script : %s", fOriginX, fOriginY, fRadius, CTheScripts::GetCurrentGtaScriptThread()->GetScriptName());

	Assertf(CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript || CNetwork::IsGameInProgress(), "ADD_NAVMESH_REQUIRED_REGION - Can only call this command in a mission or network script (scriptname:%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	ASSERT_ONLY(bool bAddedOk = ) CPathServer::AddNavMeshRegion( NMR_Script, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), fOriginX, fOriginY, fRadius );
	Assert(bAddedOk);
}

//****************************************************************************
//	CommandRemoveNavMeshRequiredRegion
//	Removes a region previously added with CommandRemoveNavMeshRequiredRegion
//****************************************************************************
void CommandRemoveNavMeshRequiredRegions()
{
	Displayf("REMOVE_NAVMESH_REQUIRED_REGIONS - Script: %s", CTheScripts::GetCurrentGtaScriptThread()->GetScriptName());

	ASSERT_ONLY(bool bRemovedOk = ) CPathServer::RemoveNavMeshRegion( NMR_Script, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId() );
	Assert(bRemovedOk);
}

bool CommandIsNavMeshRequiredRegionInUse()
{
	return (CPathServer::GetNavmeshMeshRegionScriptID() != THREAD_INVALID);
}

//****************************************************************************
//	Checks whether all required navmesh regions are loaded.  This includes all
//	the user-defined regions added with the commands above AND the default
//	region which is always loaded around the player.
//****************************************************************************
bool CommandAreAllNavMeshRegionsLoaded(void)
{
	bool bAllLoaded = CPathServer::AreAllNavMeshRegionsLoaded();
	return bAllLoaded;
}

bool CommandIsNavMeshLoadedInArea(const scrVector & vScrMin, const scrVector & vScrMax)
{
	const Vector3 vMin = vScrMin;
	const Vector3 vMax = vScrMax;

	LOCK_IMMEDIATE_DATA;

	Vector3 vTestPos = vMin;
	const float fStep = CPathServerExtents::GetSizeOfNavMesh();

	for(vTestPos.x=vMin.x; vTestPos.x<=vMax.x; vTestPos.x+=fStep)
	{
		for(vTestPos.y=vMin.y; vTestPos.y<=vMax.y; vTestPos.y+=fStep)
		{
			const TNavMeshIndex iMesh = CPathServer::GetNavMeshIndexFromPosition(vTestPos, kNavDomainRegular);;
			CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iMesh, kNavDomainRegular);
			if(!pNavMesh)
			{
				return false;
			}
		}
	}

	return true;
}

s32 CommandGetNumNavMeshesExistingInArea(const scrVector & vScrMin, const scrVector & vScrMax)
{
	const Vector3 vMin = vScrMin;
	const Vector3 vMax = vScrMax;

	s32 iNumNavMeshes = 0;
	aiNavMeshStore * pStore = CPathServer::GetNavMeshStore(kNavDomainRegular);

	LOCK_IMMEDIATE_DATA;

	Vector3 vTestPos = vMin;
	const float fStep = CPathServerExtents::GetSizeOfNavMesh();

	for(vTestPos.x=vMin.x; vTestPos.x<=vMax.x; vTestPos.x+=fStep)
	{
		for(vTestPos.y=vMin.y; vTestPos.y<=vMax.y; vTestPos.y+=fStep)
		{
			const TNavMeshIndex iMesh = CPathServer::GetNavMeshIndexFromPosition(vTestPos, kNavDomainRegular);
			if(iMesh != NAVMESH_NAVMESH_INDEX_NONE)
			{
				if( pStore->GetIsMeshInImage(iMesh) )
				{
					iNumNavMeshes++;
				}
			}
		}
	}

	return iNumNavMeshes;
}

s32 CommandGetNumNavMeshesLoadedInArea(const scrVector & vScrMin, const scrVector & vScrMax)
{
	const Vector3 vMin = vScrMin;
	const Vector3 vMax = vScrMax;

	s32 iNumNavMeshes = 0;

	LOCK_IMMEDIATE_DATA;

	Vector3 vTestPos = vMin;
	const float fStep = CPathServerExtents::GetSizeOfNavMesh();

	for(vTestPos.x=vMin.x; vTestPos.x<=vMax.x; vTestPos.x+=fStep)
	{
		for(vTestPos.y=vMin.y; vTestPos.y<=vMax.y; vTestPos.y+=fStep)
		{
			const TNavMeshIndex iMesh = CPathServer::GetNavMeshIndexFromPosition(vTestPos, kNavDomainRegular);;
			CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iMesh, kNavDomainRegular);
			if(pNavMesh)
			{
				iNumNavMeshes++;
			}
		}
	}

	return iNumNavMeshes;
}


// see "commands_path.sch" for the script enum, which must match below
enum BLOCKING_OBJECT_FLAGS
{
	BLOCKING_OBJECT_DEFAULT						= 0,
	BLOCKING_OBJECT_WANDERPATH					= 1,	// Blocking object will block wander paths
	BLOCKING_OBJECT_SHORTESTPATH				= 2,	// Blocking object will block (regular) shortest-paths
	BLOCKING_OBJECT_FLEEPATH					= 4,		// Blocking object will block flee paths

	BLOCKING_OBJECT_ALLPATHS					= 7
};

s32 CommandAddNavMeshBlockingObject(const scrVector & vPosition, const scrVector & vSizeXYZ, float fHeading, bool bPermanent, s32 iFlags)
{
	s32 iBlockingFlags = 0;
	if((iFlags & BLOCKING_OBJECT_WANDERPATH)!=0)
		iBlockingFlags |= TDynamicObject::BLOCKINGOBJECT_WANDERPATH;
	if((iFlags & BLOCKING_OBJECT_SHORTESTPATH)!=0)
		iBlockingFlags |= TDynamicObject::BLOCKINGOBJECT_SHORTESTPATH;
	if((iFlags & BLOCKING_OBJECT_FLEEPATH)!=0)
		iBlockingFlags |= TDynamicObject::BLOCKINGOBJECT_FLEEPATH;

	// Don't allow scripters to add permanent objects - this is asking for trouble
	Assertf(!bPermanent, "ADD_NAVMESH_BLOCKING_OBJECT - the 'bPermanent' parameter is being deprecated, please set it to FALSE in this script (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	bPermanent = false;

	const scrThreadId iScriptId = bPermanent ? static_cast<scrThreadId>(0xffffffff) : CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	s32 iObjectIndex = CPathServerGta::AddScriptedBlockingObject( iScriptId, vPosition, vSizeXYZ, fHeading, iBlockingFlags );

	Assertf( iObjectIndex >= 0, "Couldn't add object in command ADD_NAVMESH_BLOCKING_OBJECT");
	scriptAssertf( !Vector3(vPosition).IsZero(), "Script %s addding a navmesh blocking object at the origin (0,0,0) - this is most likely uninitialised script variables.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	Displayf("ADD_NAVMESH_BLOCKING_OBJECT : object %i at (%.1f, %.1f, %.1f)", iObjectIndex, vPosition.x, vPosition.y, vPosition.z);

	return iObjectIndex;
}

void CommandUpdateNavMeshBlockingObject(int iObjectHandle, const scrVector & vPosition, const scrVector & vSizeXYZ, float fHeading, s32 iFlags)
{
	s32 iBlockingFlags = 0;
	if((iFlags & BLOCKING_OBJECT_WANDERPATH)!=0)
		iBlockingFlags |= TDynamicObject::BLOCKINGOBJECT_WANDERPATH;
	if((iFlags & BLOCKING_OBJECT_SHORTESTPATH)!=0)
		iBlockingFlags |= TDynamicObject::BLOCKINGOBJECT_SHORTESTPATH;
	if((iFlags & BLOCKING_OBJECT_FLEEPATH)!=0)
		iBlockingFlags |= TDynamicObject::BLOCKINGOBJECT_FLEEPATH;

	const scrThreadId iScriptId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	ASSERT_ONLY(bool bUpdated =) CPathServerGta::UpdateScriptedBlockingObject( iScriptId, iObjectHandle, vPosition, vSizeXYZ, fHeading, iBlockingFlags );

	Assertf( bUpdated, "Couldn't update object %i in command UPDATE_NAVMESH_BLOCKING_OBJECT", iObjectHandle);
}

void CommandRemoveNavMeshBlockingObject(s32 iObjectIndex)
{
	OUTPUT_ONLY(bool bRemoved =) CPathServerGta::RemoveScriptedBlockingObject( CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), iObjectIndex );

	Displayf("REMOVE_NAVMESH_BLOCKING_OBJECT : object %i removed = %s", iObjectIndex, bRemoved ? "true":"false");
}

bool CommandDoesNavMeshBlockingObjectExist(s32 iObjectIndex)
{
	return CPathServerGta::DoesScriptedBlockingObjectExist( CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), iObjectIndex );
}

scrVector CommandGenerateDirections_OutPos(const scrVector & vDestination, int iFlags, int & iOut_Directions, int & iOut_StreetNameHash, float & fOut_ApproxDistance)
{
	u32	iHash = 0;
	s32	iDir = 0;
	float fDist = 10.f;

	iOut_StreetNameHash = iHash;

	Vector3 vDest = Vector3(vDestination.x, vDestination.y, vDestination.z);
	CGpsSlot& GpsSlot = CGps::GetSlot(GPS_SLOT_DISCRETE);
	if (iFlags & GPS_FLAG_AVOID_HIGHWAY)
		GpsSlot.m_iGpsFlags |= GPS_FLAG_AVOID_HIGHWAY;
	else
		GpsSlot.m_iGpsFlags &= (~GPS_FLAG_AVOID_HIGHWAY);

	if (!vDest.IsClose(GpsSlot.m_Destination, 0.5f))
		GpsSlot.Start(vDest, NULL, 0, false, GPS_SLOT_DISCRETE, Color_magenta);

	Vector3 vVector3OutPos(VEC3_ZERO);
	if (GpsSlot.GetStatus() == CGpsSlot::STATUS_WORKING)
	{
		ThePaths.GenerateDirections(iDir, iHash, fDist, GpsSlot.m_NodeAdress, GpsSlot.m_NumNodes, vVector3OutPos);

		iOut_Directions = iDir;
		fOut_ApproxDistance = fDist;
	}

	return scrVector(vVector3OutPos);
}

int CommandGenerateDirections(const scrVector & vDestination, int iFlags, int & iOut_Directions, int & iOut_StreetNameHash, float & fOut_ApproxDistance)
{
	CommandGenerateDirections_OutPos(vDestination, iFlags, iOut_Directions, iOut_StreetNameHash, fOut_ApproxDistance);
	return 0;
}

//
// Can be used to tell the player which way to go.
// 0 = straight on, 1 = left, 2 = right.
//

void CommandCreateDirections(const scrVector & destination, int& direction, int& streetNameHash)
{
	u32	iHash = 0;
	//s32	iDir = 0;
	float fDist = 10.f;

	streetNameHash = iHash;

	CommandGenerateDirections(destination, 0, direction, streetNameHash, fDist);

	int ScriptDirections[CPathFind::DIRECTIONS_MAX] =
	{
		0,	//DIRECTIONS_UNKNOWN
		0,	//DIRECTIONS_WRONG_WAY
		3,	//DIRECTIONS_KEEP_DRIVING
		1,	//DIRECTIONS_LEFT_AT_JUNCTION
		2,	//DIRECTIONS_RIGHT_AT_JUNCTION
		3,	//DIRECTIONS_STRAIGHT_THROUGH_JUNCTION
		5,	//DIRECTIONS_KEEP_LEFT
		6,	//DIRECTIONS_KEEP_RIGHT
		7,	//DIRECTIONS_UTURN
	};

	Assert(direction >= 0 && direction < CPathFind::DIRECTIONS_MAX);
	direction = ScriptDirections[direction];
}

void CommandSetIgnoreNoGpsFlag(bool bValue)
{
	ThePaths.bIgnoreNoGpsFlag = bValue;
}

void CommandSetIgnoreNoGpsFlagUntilFirstNode(bool bValue)
{
	ThePaths.bIgnoreNoGpsFlagUntilFirstNode = bValue;
}

int CommandGetNextAvailableGpsDisabledZoneIndex()
{
	return ThePaths.GetNextAvailableGpsBlockingRegionForScript();
}

void CommandSetGpsDisabledZoneAtIndex( const scrVector & vScriptMin, const scrVector & vScriptMax, int iIndex )
{
	Vector3 vMin, vMax;
	vMin.x = Min(vScriptMin.x, vScriptMax.x);
	vMin.y = Min(vScriptMin.y, vScriptMax.y);
	vMin.z = Min(vScriptMin.z, vScriptMax.z);
	vMax.x = Max(vScriptMin.x, vScriptMax.x);
	vMax.y = Max(vScriptMin.y, vScriptMax.y);
	vMax.z = Max(vScriptMin.z, vScriptMax.z);

	ThePaths.SetGpsBlockingRegionForScript(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), vMin, vMax, iIndex );
}

void CommandClearGpsDisabledZoneAtIndex(int iIndex)
{
	ThePaths.ClearGpsBlockingRegionForScript( iIndex );
}

void CommandSetGpsDisabledZone( const scrVector & vScriptMin, const scrVector & vScriptMax )
{
	Vector3 vMin, vMax;
	vMin.x = Min(vScriptMin.x, vScriptMax.x);
	vMin.y = Min(vScriptMin.y, vScriptMax.y);
	vMin.z = Min(vScriptMin.z, vScriptMax.z);
	vMax.x = Max(vScriptMin.x, vScriptMax.x);
	vMax.y = Max(vScriptMin.y, vScriptMax.y);
	vMax.z = Max(vScriptMin.z, vScriptMax.z);

	ThePaths.SetGpsBlockingRegionForScript(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), vMin, vMax, 0 );
}

bool CommandGetRandomVehicleNode(const scrVector & scrVecCentrePoint, float radius, s32 minLanes, bool bAvoidDeadEnds, bool bAvoidHighways, Vector3 &vecReturn, s32 &intNodeAddress )
{

	Vector3 centrePoint = Vector3(scrVecCentrePoint);
	CNodeAddress	node;
	const bool bAvoidSwitchedOff = true;
	if (ThePaths.GetRandomCarNode(centrePoint, radius, false, minLanes, bAvoidDeadEnds, bAvoidHighways, bAvoidSwitchedOff, vecReturn, node))
	{
		node.ToInt(intNodeAddress);
		return true;
	}
	intNodeAddress = 0;
	return false;
}


bool CommandGetRandomVehcleNodeIncludingSwitchedOffNodes(const scrVector & scrVecCentrePoint, float radius, s32 minLanes, bool bAvoidDeadEnds, bool bAvoidHighways, Vector3 &vecReturn, s32 &intNodeAddress )
{
	Vector3 centrePoint = Vector3(scrVecCentrePoint);
	CNodeAddress	node;
	const bool bAvoidSwitchedOff = false;
	if (ThePaths.GetRandomCarNode(centrePoint, radius, false, minLanes, bAvoidDeadEnds, bAvoidHighways, bAvoidSwitchedOff, vecReturn, node))
	{
		node.ToInt(intNodeAddress);
		return true;
	}
	intNodeAddress = 0;
	return false;
}

bool CommandGetRandomWaterNode(const scrVector & scrVecCentrePoint, float radius, s32 minLanes, bool bAvoidDeadEnds, bool bAvoidHighways, Vector3 &vecReturn, s32 &intNodeAddress )
{
	Vector3 centrePoint = Vector3(scrVecCentrePoint);
	CNodeAddress	node;
	if (ThePaths.GetRandomCarNode(centrePoint, radius, true, minLanes, bAvoidDeadEnds, bAvoidHighways, true, vecReturn, node))
	{
		node.ToInt(intNodeAddress);
		return true;
	}
	intNodeAddress = 0;
	return false;
}

void CommandGetSpawnCoordinatesForVehicleNode(s32 NodeAddress, const scrVector & scrVecTowardsPos, Vector3 &vecReturn, float &orientation )
{
	if(SCRIPT_VERIFY(NodeAddress!=0, "GET_SPAWN_COORDINATES_FOR_VEHICLE_NODE called with empty node (First param = 0)"))
	{
		Vector3 towardsPos = Vector3(scrVecTowardsPos);
		CNodeAddress	node;
		node.FromInt(NodeAddress);
		if(SCRIPT_VERIFY(!node.IsEmpty(), "GET_SPAWN_COORDINATES_FOR_VEHICLE_NODE called with empty node"))
		{
			if(SCRIPT_VERIFY(node.GetRegion() < PATHFINDREGIONS, "GET_SPAWN_COORDINATES_FOR_VEHICLE_NODE called with invalid node"))
			{
				ThePaths.GetSpawnCoordinatesForCarNode(node, towardsPos, vecReturn, orientation);
			}
		}
	}
}

//
// name:		CommandGetGpsBlipRouteLength
// description:	Gets the length of a gps route, returns 0 if no route
s32 CommandGetGpsBlipRouteLength()
{
	return CGps::GetRouteLength(GPS_SLOT_RADAR_BLIP);
}

//
// name:		CommandGetWaypointRouteEnd
// description:	Gets the length of a gps route, returns 0 if no route
bool CommandGetWaypointRouteEnd(Vector3& vecReturn)
{
	CGpsSlot& GpsSlot = CGps::GetSlot(GPS_SLOT_WAYPOINT);
	if (GpsSlot.IsOn() && GpsSlot.m_NumNodes > 0)
	{
		vecReturn = GpsSlot.GetRouteEnd();
		return true;
	}

	return false;
}

//
// name:		CommandGetWaypointRouteEnd
// description:	Gets the position along a GPS route, returns 0 if no route
bool CommandGetPosAlongGPSRoute(Vector3& vecReturn, bool bStartAtPlayerPos, float fDistanceAlongRoute)
{
	CGpsSlot& GpsSlot = CGps::GetSlot(GPS_SLOT_WAYPOINT);
	if (GpsSlot.IsOn() && GpsSlot.m_NumNodes > 0)
	{
		vecReturn = GpsSlot.GetNextNodePositionAlongRoute(bStartAtPlayerPos, fDistanceAlongRoute);
		return true;
	}

	return false;
}

//does same as CommandGetPosAlongGPSRoute but allows gps slot type to be provided
bool CommandGetPosAlongGPSRouteType(Vector3& vecReturn, bool bStartAtPlayerPos, float fDistanceAlongRoute, int slotType)
{
	if(SCRIPT_VERIFY(slotType < GPS_NUM_SLOTS && slotType >= 0, "GET_POS_ALONG_GPS_TYPE_ROUTE called with invalid slot type"))
	{
		CGpsSlot& GpsSlot = CGps::GetSlot((s32)slotType);
		if (GpsSlot.IsOn() && GpsSlot.m_NumNodes > 0)
		{
			vecReturn = GpsSlot.GetNextNodePositionAlongRoute(bStartAtPlayerPos, fDistanceAlongRoute);
			return true;
		}
	}

	return false;
}

//
// name:		CommandGetGpsDiscreteRouteLength
// description:	Gets the length of the gps discrete route, returns 0 if no route
s32 CommandGetGpsDiscreteRouteLength(const scrVector & vDestination)
{
	Vector3 vDest = Vector3(vDestination);
	CGpsSlot& GpsSlot = CGps::GetSlot(GPS_SLOT_DISCRETE);
	if (!vDest.IsClose(GpsSlot.m_Destination, 0.5f))
		GpsSlot.Start(vDest, NULL, 0, false, GPS_SLOT_DISCRETE, Color_magenta);

	if (GpsSlot.GetStatus() != CGpsSlot::STATUS_WORKING)
		return 0;

	return CGps::GetRouteLength(GPS_SLOT_DISCRETE);
}

//
// name:		CommandGetGpsBlipRouteFound
// description:	Returns true if a blip route has been found and is being displayed
bool CommandGetGpsBlipRouteFound()
{
	return CGps::GetRouteLength(GPS_SLOT_RADAR_BLIP) != 0;
}

bool GetRoadBoundaryUsingHeading(const scrVector & scrVecNodePosition, float fHeading, Vector3 & vPositionByRoad)
{
	Vector3 vNodePosition(scrVecNodePosition);

	CNodeAddress nodeAddress = ThePaths.FindNodeClosestToCoors(vNodePosition, 9999.0f, CPathFind::IncludeSwitchedOffNodes);
	if(nodeAddress.IsEmpty())
		return false;

	const CPathNode * pNode = ThePaths.FindNodePointerSafe(nodeAddress);
	if(!pNode)
		return false;

	if(!pNode->NumLinks())
		return false;

	s32 iLinkIndex = ThePaths.FindBestLinkFromHeading(nodeAddress, fHeading);
	CPathNodeLink& link = ThePaths.GetNodesLink(pNode, iLinkIndex);

	//calculate total road width at this point
	float fLeft, fRight;
	ThePaths.FindRoadBoundaries(link, fLeft, fRight);

	CPathNode* pNextNode = ThePaths.GetNodesLinkedNode(pNode, iLinkIndex);
	if (!pNextNode)
		return false;

	Vector3 nextNodePos;
	pNextNode->GetCoors(nextNodePos);
	Vector3 nodePos;
	pNode->GetCoors(nodePos);

	//get side vector
	Vector2 side = Vector2(nextNodePos.y -  nodePos.y,  -(nextNodePos.x - nodePos.x));
	side.Normalize();

	//calculate road extreme
	vPositionByRoad = Vector3((side.x * fRight),(side.y * fRight), 0.0f) + nodePos;
	return true;
}

bool GetPositionBySideOfRoadUsingHeading(const scrVector & scrVecNodePosition, float fHeading, Vector3 & vPositionByRoad)
{
	Vector3 vNodePosition(scrVecNodePosition);
	return ThePaths.GetPositionBySideOfRoad(vNodePosition, fHeading, vPositionByRoad);
}

bool GetPositionBySideOfRoad(const scrVector & scrVecNodePosition, int iDirection, Vector3 & vPositionByRoad)
{
	Vector3 vNodePosition(scrVecNodePosition);

	Assert(iDirection==0 || iDirection==1 || iDirection==-1);

	return ThePaths.GetPositionBySideOfRoad(vNodePosition, iDirection, vPositionByRoad);
}

bool CommandIsPointOnRoad(const scrVector& scrVecPos, int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex,0);  // don't assert if no vehicle
	//pVehicle might be NULL, and that's ok! IsPointOnRoad checks it

	Vector3 vPos(scrVecPos);
	return CTaskVehicleThreePointTurn::IsPointOnRoad(vPos, pVehicle);
}

s32 Junctions_GetPhaseCount(const scrVector & scrVecJunctionPos)
{
	Vector3 vJunctionPos(scrVecJunctionPos);
	int iJunction = CJunctions::GetJunctionAtPosition(vJunctionPos);
	Assertf(iJunction != -1, "No active junction at this position!");
	CJunction * pJunction = CJunctions::GetJunctionByIndex( iJunction );
	Assertf(pJunction, "No junction found");
	if(pJunction)
	{
		Assertf(pJunction->GetTemplateIndex()!=-1, "This is not a templated junction & can't be manipulated by script.");
		if(pJunction->GetTemplateIndex()!=-1)
		{
			return pJunction->GetNumLightPhases();
		}
	}
	return 0;
}

bool Junctions_ActivatePhase(const scrVector & scrVecJunctionPos, s32 iPhase, float fLightDuration)
{
	Vector3 vJunctionPos(scrVecJunctionPos);
	int iJunction = CJunctions::GetJunctionAtPosition(vJunctionPos);
	Assertf(iJunction != -1, "No active junction at this position!");
	CJunction * pJunction = CJunctions::GetJunctionByIndex( iJunction );
	Assertf(pJunction, "No junction found");
	if(pJunction)
	{
		Assertf(pJunction->GetTemplateIndex()!=-1, "This is not a templated junction & can't be manipulated by script.");
		if(pJunction->GetTemplateIndex()!=-1)
		{
			Assertf(iPhase >= 0 && iPhase < pJunction->GetNumLightPhases(), "Phase index is out of range (%i..%i)", 0, pJunction->GetNumLightPhases());
			if(iPhase >= 0 && iPhase < pJunction->GetNumLightPhases())
			{
				pJunction->SetLightPhase(iPhase);

				if(fLightDuration > 0.0f)
				{
					pJunction->SetLightTimeRemaining(fLightDuration);
				}
				else
				{
					float fDuration = pJunction->GetLightPhaseDuration(iPhase);
					pJunction->SetLightTimeRemaining(fDuration);
				}
				return true;
			}
		}
	}
	return false;
}

s32 Junctions_GetEntranceCount(const scrVector & scrVecJunctionPos)
{
	Vector3 vJunctionPos(scrVecJunctionPos);
	int iJunction = CJunctions::GetJunctionAtPosition(vJunctionPos);
	Assertf(iJunction != -1, "No active junction at this position!");
	CJunction * pJunction = CJunctions::GetJunctionByIndex(iJunction);
	Assertf(pJunction, "No junction found");
	if(pJunction)
	{
		Assertf(pJunction->GetTemplateIndex()!=-1, "This is not a templated junction & can't be manipulated by script.");
		if(pJunction->GetTemplateIndex()!=-1)
		{
			return pJunction->GetNumEntrances();
		}
	}
	return 0;
}

bool Junctions_GetEntranceInfo(const scrVector & scrVecJunctionPos, s32 iEntrance, Vector3 & vEntrancePosition, float & fEntranceHeading, s32 & iPhase, s32 & iFilterLeftPhase, s32 & bFiltersLeftOnly, s32 & bFiltersRight, s32 & bRightLaneRightOnly)
{
	Vector3 vJunctionPos(scrVecJunctionPos);
	int iJunction = CJunctions::GetJunctionAtPosition(vJunctionPos);
	Assertf(iJunction != -1, "No active junction at this position!");
	CJunction * pJunction = CJunctions::GetJunctionByIndex( iJunction );
	Assertf(pJunction, "No junction found");
	if(pJunction)
	{
		Assertf(pJunction->GetTemplateIndex()!=-1, "This is not a templated junction & can't be manipulated by script.");
		if(pJunction->GetTemplateIndex()!=-1)
		{
			CJunctionEntrance & entrance = pJunction->GetEntrance(iEntrance);

			vEntrancePosition = entrance.m_vPositionOfNode;
			fEntranceHeading = fwAngle::GetRadianAngleBetweenPoints(entrance.m_vEntryDir.x, entrance.m_vEntryDir.y, 0.0f, 0.0f);
			fEntranceHeading *= RtoD;
			while(fEntranceHeading < 0.0f) fEntranceHeading += 360.0f;
			while(fEntranceHeading >= 360.0f) fEntranceHeading -= 360.0f;
			iPhase = entrance.m_iPhase;
			iFilterLeftPhase = entrance.m_iLeftFilterPhase;
			bFiltersLeftOnly = entrance.m_bLeftLaneIsAheadOnly;
			bFiltersRight = entrance.m_bCanTurnRightOnRedLight;
			bRightLaneRightOnly = entrance.m_bRightLaneIsRightOnly;

			return true;
		}
	}
	return false;
}

bool Junctions_Restore(const scrVector & scrVecJunctionPos)
{
	Vector3 vJunctionPos(scrVecJunctionPos);
	int iJunction = CJunctions::GetJunctionAtPosition(vJunctionPos);
	Assertf(iJunction != -1, "No active junction at this position!");
	CJunction * pJunction = CJunctions::GetJunctionByIndex( iJunction );
	Assertf(pJunction, "No junction found");
	if(pJunction)
	{
		Assertf(pJunction->GetTemplateIndex()!=-1, "This is not a templated junction & can't be manipulated by script.");
		if(pJunction->GetTemplateIndex()!=-1)
		{
			pJunction->SetToMalfunction(false);

			int iPhase = pJunction->GetLightPhase();
			float fDuration = pJunction->GetLightPhaseDuration(iPhase);
			pJunction->SetLightTimeRemaining(fDuration);

			return true;
		}
	}
	return false;
}

bool Junctions_Malfunction(const scrVector & scrVecJunctionPos, bool b)
{
	Vector3 vJunctionPos(scrVecJunctionPos);
	int iJunction = CJunctions::GetJunctionAtPosition(vJunctionPos);
	Assertf(iJunction != -1, "No active junction at this position!");
	CJunction * pJunction = CJunctions::GetJunctionByIndex( iJunction );
	Assertf(pJunction, "No junction found");
	if(pJunction)
	{
		Assertf(pJunction->GetTemplateIndex()!=-1, "This is not a templated junction & can't be manipulated by script.");
		if(pJunction->GetTemplateIndex()!=-1)
		{
			pJunction->SetToMalfunction(b);
		}
	}
	return false;
}

bool Junctions_CreateTemplatedJunctionForScript(const scrVector & scrVecJunctionPos)
{
	Vector3 vJunctionPos(scrVecJunctionPos);
	return CJunctions::CreateTemplatedJunctionForScript(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), vJunctionPos);
}

float CommandGetApproxHeightForPoint(float x, float y)
{
	return CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(x,y);
}

float CommandGetApproxHeightForArea(float x0, float y0, float x1, float y1)
{
	return CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(x0, y0, x1, y1);
}

float CommandGetApproxFloorForPoint(float x, float y)
{
	return CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(x,y);
}

float CommandGetApproxFloorForArea(float x0, float y0, float x1, float y1)
{
	return CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(x0, y0, x1, y1);
}

float CommandCalculateTravelDistanceBetweenPoints(const scrVector & scrVecNode1Pos, const scrVector & scrVecNode2Pos)
{
	Vector3 vNode1Pos(scrVecNode1Pos);
	Vector3 vNode2Pos(scrVecNode2Pos);

	return ThePaths.CalculateTravelDistanceBetweenNodes(vNode1Pos.x, vNode1Pos.y, vNode1Pos.z, vNode2Pos.x, vNode2Pos.y, vNode2Pos.z);
}


	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(SET_ROADS_IN_AREA,0x286addbe501ff4c2, SetRoadsInArea);
		SCR_REGISTER_SECURE(SET_ROADS_IN_ANGLED_AREA,0x208b14ca225dc5a0, SetRoadsInAngledArea);
		SCR_REGISTER_SECURE(SET_PED_PATHS_IN_AREA,0x780d33653803d9b0, CommandSetPedPathsInArea);

		SCR_REGISTER_SECURE(GET_SAFE_COORD_FOR_PED,0x1555f0fa7ba4fe0f, CommandGetSafePositionForPed);
		SCR_REGISTER_SECURE(GET_CLOSEST_VEHICLE_NODE,0xdfcbbd9528dc1828, CommandGetClosestVehicleNode);
		SCR_REGISTER_SECURE(GET_CLOSEST_MAJOR_VEHICLE_NODE,0x5fcf1af649a7db39, CommandGetClosestMajorVehicleNode);
		SCR_REGISTER_SECURE(GET_CLOSEST_VEHICLE_NODE_WITH_HEADING,0x0fbea22dbe03ac23, CommandGetClosestVehicleNodeWithHeading);
		SCR_REGISTER_SECURE(GET_NTH_CLOSEST_VEHICLE_NODE,0x29579bc5971ca4b6, CommandGetNthClosestVehicleNode);
		SCR_REGISTER_SECURE(GET_NTH_CLOSEST_VEHICLE_NODE_ID,0xea3e64155da5f4d9, CommandGetNthClosestVehicleNodeId);
		SCR_REGISTER_SECURE(GET_NTH_CLOSEST_VEHICLE_NODE_WITH_HEADING,0x15cedab46d800682, CommandGetNthClosestVehicleNodeWithHeading);
		SCR_REGISTER_SECURE(GET_NTH_CLOSEST_VEHICLE_NODE_ID_WITH_HEADING,0x67a3682c37778785, CommandGetNthClosestVehicleNodeIdWithHeading);
		SCR_REGISTER_UNUSED(GET_NTH_CLOSEST_VEHICLE_NODE_WITH_HEADING_ON_ISLAND,0x597f081e6e24c8b9, CommandGetNthClosestVehicleNodeWithHeadingOnIsland);
		SCR_REGISTER_UNUSED(GET_NEXT_CLOSEST_VEHICLE_NODE_WITH_HEADING_ON_ISLAND,0xee1c4f8ee4c60c4d, CommandGetNextClosestVehicleNodeWithHeadingOnIsland);
		SCR_REGISTER_SECURE(GET_NTH_CLOSEST_VEHICLE_NODE_FAVOUR_DIRECTION,0x1b51bebb1f321733, CommandGetNthClosestVehicleNodeFavourDirection);
		SCR_REGISTER_SECURE(GET_VEHICLE_NODE_PROPERTIES,0x4c1eef24cf4b2752, CommandGetVehicleNodeProperties);
		SCR_REGISTER_SECURE(IS_VEHICLE_NODE_ID_VALID,0x958ac759881d0b58, CommandIsVehicleNodeIdValid);
		SCR_REGISTER_SECURE(GET_VEHICLE_NODE_POSITION,0x1b54427560dfe3c3, CommandGetVehicleNodePosition);
		SCR_REGISTER_SECURE(GET_VEHICLE_NODE_IS_GPS_ALLOWED,0xa202f3522092063d, CommandGetVehicleNodeIsGpsAllowed);
		SCR_REGISTER_SECURE(GET_VEHICLE_NODE_IS_SWITCHED_OFF,0x535e2ca2f0da34d4, CommandGetVehicleNodeIsSwitchedOff);
		SCR_REGISTER_SECURE(GET_CLOSEST_ROAD,0x13fd00a258a58752, CommandGetClosestRoad);
		SCR_REGISTER_SECURE(LOAD_ALL_PATH_NODES,0xd268dae911420879, CommandLoadAllPathNodes);
		SCR_REGISTER_SECURE(SET_ALLOW_STREAM_PROLOGUE_NODES,0x3204a8f8ff77eeca, CommandSetAllowStreamPrologueNodes);
		SCR_REGISTER_SECURE(SET_ALLOW_STREAM_HEIST_ISLAND_NODES,0x744c775b133c9131, CommandSetAllowStreamHeistIslandNodes);
		SCR_REGISTER_SECURE(ARE_NODES_LOADED_FOR_AREA,0xf8805443c3db6256, CommandAreNodesLoadedForArea);
		SCR_REGISTER_SECURE(REQUEST_PATH_NODES_IN_AREA_THIS_FRAME,0x2ee5fff3e1e3400d, CommandRequestPathNodesInAreaThisFrame);
		SCR_REGISTER_SECURE(SET_ROADS_BACK_TO_ORIGINAL,0x83e10da4845841b7, CommandSwitchRoadsBackToOriginal);
		SCR_REGISTER_SECURE(SET_ROADS_BACK_TO_ORIGINAL_IN_ANGLED_AREA,0x13a2865660b9b033, CommandSwitchRoadsBackToOriginalInAngledArea);
		SCR_REGISTER_SECURE(SET_AMBIENT_PED_RANGE_MULTIPLIER_THIS_FRAME,0xa8307d8c8abe6270, CommandSetAmbientPedRangeMultiplierThisFrame);
		SCR_REGISTER_SECURE(ADJUST_AMBIENT_PED_SPAWN_DENSITIES_THIS_FRAME,0x2e5c7227cf316022, CommandAdjustAmbientPedSpawnDensitiesThisFrame);

		SCR_REGISTER_SECURE(SET_PED_PATHS_BACK_TO_ORIGINAL,0x7bfef556f237e70a, CommandSwitchPedRoadsBackToOriginal);
		SCR_REGISTER_SECURE(GET_RANDOM_VEHICLE_NODE,0x5b0e3f68e96beb2f, CommandGetRandomVehicleNode);
		SCR_REGISTER_UNUSED(GET_RANDOM_VEHICLE_NODE_INCLUDE_SWITCHED_OFF_NODES, 0x3d6947a3, CommandGetRandomVehcleNodeIncludingSwitchedOffNodes);
		SCR_REGISTER_UNUSED(GET_RANDOM_WATER_NODE,0x2653d2e73d846b21, CommandGetRandomWaterNode);
		SCR_REGISTER_UNUSED(GET_SPAWN_COORDS_FOR_VEHICLE_NODE,0x90facaa3ecb5be0a, CommandGetSpawnCoordinatesForVehicleNode);
		SCR_REGISTER_SECURE(GET_STREET_NAME_AT_COORD,0x05e7aefd2137bb91, CommandFindStreetNameAtPosition);
		SCR_REGISTER_UNUSED(CREATE_DIRECTIONS_TO_COORD,0x029c059238a17c06, CommandCreateDirections);
		SCR_REGISTER_UNUSED(GENERATE_DIRECTIONS_TO_COORD_OUTPOS,0x343d9e70c8b4f39e, CommandGenerateDirections_OutPos);
		SCR_REGISTER_SECURE(GENERATE_DIRECTIONS_TO_COORD,0x34caae581d7e2318, CommandGenerateDirections);

		SCR_REGISTER_SECURE(SET_IGNORE_NO_GPS_FLAG,0xac60fc9ab1b2cc70, CommandSetIgnoreNoGpsFlag);
		SCR_REGISTER_SECURE(SET_IGNORE_NO_GPS_FLAG_UNTIL_FIRST_NORMAL_NODE,0x517f19588645defb, CommandSetIgnoreNoGpsFlagUntilFirstNode);
		SCR_REGISTER_SECURE(SET_GPS_DISABLED_ZONE,0x27ac9e8977e369dd, CommandSetGpsDisabledZone);
		SCR_REGISTER_SECURE(GET_GPS_BLIP_ROUTE_LENGTH,0x8748276d7c932108, CommandGetGpsBlipRouteLength);
		SCR_REGISTER_UNUSED(GET_GPS_WAYPOINT_ROUTE_END,0xcc72e9ac0b3a99b6, CommandGetWaypointRouteEnd);
		SCR_REGISTER_UNUSED(GET_POS_ALONG_GPS_ROUTE,0xe2d65a98f8322e88, CommandGetPosAlongGPSRoute);
		SCR_REGISTER_SECURE(GET_POS_ALONG_GPS_TYPE_ROUTE,0xc97d1ceb93adebbc, CommandGetPosAlongGPSRouteType);
		SCR_REGISTER_UNUSED(GET_GPS_DISCRETE_ROUTE_LENGTH,0x8746ddfee6b3b3a8, CommandGetGpsDiscreteRouteLength);
		SCR_REGISTER_SECURE(GET_GPS_BLIP_ROUTE_FOUND,0x718f1e0d4400c7a0, CommandGetGpsBlipRouteFound);
		SCR_REGISTER_SECURE(GET_ROAD_BOUNDARY_USING_HEADING,0x3748f54b0aec5219, GetRoadBoundaryUsingHeading);
		SCR_REGISTER_SECURE(GET_POSITION_BY_SIDE_OF_ROAD,0xe245e7da125485ec, GetPositionBySideOfRoad);
		SCR_REGISTER_UNUSED(GET_POSITION_BY_SIDE_OF_ROAD_USING_HEADING,0xe11e1d153a55b04b, GetPositionBySideOfRoadUsingHeading);
		SCR_REGISTER_SECURE(IS_POINT_ON_ROAD,0x9598a4bd9a1f848f, CommandIsPointOnRoad);
		SCR_REGISTER_SECURE(GET_NEXT_GPS_DISABLED_ZONE_INDEX,0x708a07ba6f801ce6, CommandGetNextAvailableGpsDisabledZoneIndex);
		SCR_REGISTER_SECURE(SET_GPS_DISABLED_ZONE_AT_INDEX,0x344973f34eb71696, CommandSetGpsDisabledZoneAtIndex);
		SCR_REGISTER_SECURE(CLEAR_GPS_DISABLED_ZONE_AT_INDEX,0xef3f1764ed2dee8a, CommandClearGpsDisabledZoneAtIndex);

		SCR_REGISTER_SECURE(ADD_NAVMESH_REQUIRED_REGION,0x862852388ae23818, CommandAddNavMeshRequiredRegion);
		SCR_REGISTER_SECURE(REMOVE_NAVMESH_REQUIRED_REGIONS,0x0f02061f6faeb6c1, CommandRemoveNavMeshRequiredRegions);
		SCR_REGISTER_SECURE(IS_NAVMESH_REQUIRED_REGION_IN_USE,0x221808c6caa5935e, CommandIsNavMeshRequiredRegionInUse);
		SCR_REGISTER_SECURE(DISABLE_NAVMESH_IN_AREA,0xb2c3d985489123f0, CommandDisableNavMeshInArea);
		SCR_REGISTER_SECURE(ARE_ALL_NAVMESH_REGIONS_LOADED,0x6d56c86cdb6269ae, CommandAreAllNavMeshRegionsLoaded);
		SCR_REGISTER_SECURE(IS_NAVMESH_LOADED_IN_AREA,0x422c66e6b46d5aef, CommandIsNavMeshLoadedInArea);

		SCR_REGISTER_SECURE(GET_NUM_NAVMESHES_EXISTING_IN_AREA,0xe64c2e86eac945dd, CommandGetNumNavMeshesExistingInArea);
		SCR_REGISTER_UNUSED(GET_NUM_NAVMESHES_LOADED_IN_AREA,0xf4ec7530e9772cd0, CommandGetNumNavMeshesLoadedInArea);

		SCR_REGISTER_SECURE(ADD_NAVMESH_BLOCKING_OBJECT,0x7afc31f33cb9b8d5, CommandAddNavMeshBlockingObject);
		SCR_REGISTER_SECURE(UPDATE_NAVMESH_BLOCKING_OBJECT,0x3ff379bef70564fc, CommandUpdateNavMeshBlockingObject);
		SCR_REGISTER_SECURE(REMOVE_NAVMESH_BLOCKING_OBJECT,0xb251ddfa99084c56, CommandRemoveNavMeshBlockingObject);
		SCR_REGISTER_SECURE(DOES_NAVMESH_BLOCKING_OBJECT_EXIST,0x046b574e2cf3f2cc, CommandDoesNavMeshBlockingObjectExist);


		SCR_REGISTER_UNUSED(JUNCTIONS_GET_PHASE_COUNT,0x7a21ed424da0e6ca, Junctions_GetPhaseCount);
		SCR_REGISTER_UNUSED(JUNCTIONS_ACTIVATE_PHASE,0xb37eae203a0c60bf, Junctions_ActivatePhase);
		SCR_REGISTER_UNUSED(JUNCTIONS_GET_ENTRANCE_COUNT,0x74c72db3d1bd5cec, Junctions_GetEntranceCount);
		SCR_REGISTER_UNUSED(JUNCTIONS_GET_ENTRANCE_INFO,0xa5d4826b23edd623, Junctions_GetEntranceInfo);
		SCR_REGISTER_UNUSED(JUNCTIONS_RESTORE,0x38de0601e64f7688, Junctions_Restore);
		SCR_REGISTER_UNUSED(JUNCTIONS_MALFUNCTION,0xd78e13bcef173f7a, Junctions_Malfunction);
		SCR_REGISTER_UNUSED(JUNCTIONS_CREATE_TEMPLATED_JUNCTION_FOR_SCRIPT,0x6a4305d4c1697307, Junctions_CreateTemplatedJunctionForScript);

		SCR_REGISTER_SECURE(GET_APPROX_HEIGHT_FOR_POINT,0xe11e41ec1840ab62, CommandGetApproxHeightForPoint);
		SCR_REGISTER_SECURE(GET_APPROX_HEIGHT_FOR_AREA,0x23555349ef1f5ebe, CommandGetApproxHeightForArea);

		SCR_REGISTER_SECURE(GET_APPROX_FLOOR_FOR_POINT,0x1ada868ded581c1d, CommandGetApproxFloorForPoint);
		SCR_REGISTER_SECURE(GET_APPROX_FLOOR_FOR_AREA,0xc0f26bf4fdb0b07c, CommandGetApproxFloorForArea);

		SCR_REGISTER_SECURE(CALCULATE_TRAVEL_DISTANCE_BETWEEN_POINTS,0x0035bf2ca9a27b39, CommandCalculateTravelDistanceBetweenPoints);
	}
}	//	end of namespace path_commands


