/////////////////////////////////////////////////////////////////////////////////
// FILE :    Pathfind.cpp
// PURPOSE : Deals with building a grid for the high level path-finding and
//           with actually finding a route
// AUTHOR :  Obbe
// CREATED : 14-10-99
/////////////////////////////////////////////////////////////////////////////////
#include "vehicleai/pathfind.h"

#include "ai/stats.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/Viewport.h"
#include "control/gameLogic.h"
#include "control/route.h"
#include "control/trafficLights.h"
#include "control/record.h"
#include "control/gps.h"
#include "control/slownesszones.h"
#include "core/game.h"
#include "fwmaths/vector.h"
#include "fwmaths/geomutil.h"
#include "fwsys/fileExts.h"
#include "game/PopMultiplierAreas.h"
#include "modelInfo/vehiclemodelinfo.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/water.h"
#include "scene/world/gameWorld.h"
#include "scene/building.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "script/script_areas.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"
#include "system/ControlMgr.h"
#include "system/fileMgr.h"
#include "objects/object.h"
#include "objects/dummyobject.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/buses.h"
#include "vehicles/virtualroad.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/roadspeedzone.h"
#include "debug/vectormap.h"
#include "debug/DebugScene.h"
#include "game/zones.h"
#include "Peds/AssistedMovementStore.h"
#include "Peds/PedPopulation.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PedPopulation.h"
#include "physics/physics.h"
#include "scene/FocusEntity.h"
#include "Objects/ProcObjects.h"
#include "Text/TextConversion.h"
#include "Task/TaskVehicleGoToAutomobile.h"	//for debug draw
#include "Task/TaskVehicleCruise.h"

// Rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "fwscene/world/WorldLimits.h"
#include "script/thread.h"
#include "data/resourceheader.h"
#include "system/performancetimer.h"
#include "system/taskheader.h"

using namespace AIStats;

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
NAVMESH_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

#define SERIOUSDEBUG				0

// PURPOSE:	If enabled, compile support for debug drawing of path searches.
#define PATHSEARCHDRAW				(DEBUG_DRAW && (!(__DEV) || !(__XENON)))	// Disable for 360 Beta to save code space

// The actual memory for the pathfind stuff
CPathFind	ThePaths;

CNodeAddress EmptyNodeAddress;	// This is here to pass into DoPathSearch function.

DECLARE_TASK_INTERFACE(PathfindNodeSearchSPU);


float saTiltValues[NUM_NODE_TILT_VALUES] = {
	(0.0f * PI / 180.0f),	// 0
	(0.5f * PI / 180.0f),	// 1
	(1.0f * PI / 180.0f),	// 2
	(1.5f * PI / 180.0f),	// 3
	(2.0f * PI / 180.0f),	// 4
	(2.5f * PI / 180.0f),	// 5
	(3.0f * PI / 180.0f),	// 6
	(4.0f * PI / 180.0f),	// 7
	(5.0f * PI / 180.0f),	// 8
	(6.0f * PI / 180.0f),	// 9
	(7.0f * PI / 180.0f),	// 10
	(9.0f * PI / 180.0f),	// 11
	(11.0f * PI / 180.0f),	// 12
	(14.0f * PI / 180.0f),	// 13
	(17.0f * PI / 180.0f),	// 14
	(-0.5f * PI / 180.0f),	// 15
	(-1.0f * PI / 180.0f),	// 16
	(-1.5f * PI / 180.0f),	// 17
	(-2.0f * PI / 180.0f),	// 18
	(-2.5f * PI / 180.0f),	// 19
	(-3.0f * PI / 180.0f),	// 20
	(-4.0f * PI / 180.0f),	// 21
	(-5.0f * PI / 180.0f),	// 22
	(-6.0f * PI / 180.0f),	// 23
	(-7.0f * PI / 180.0f),	// 24
	(-9.0f * PI / 180.0f),	// 25
	(-11.0f * PI / 180.0f),	// 26
	(-14.0f * PI / 180.0f),	// 27
	(-17.0f * PI / 180.0f)	// 28
};

bank_float CPathFind::sm_fPedCrossReserveSlotsCentroidForwardOffset = 1.3f;
bank_float CPathFind::sm_fPedCrossReserveSlotsPlacementGridSpacing = 0.9f;

const float CPathFind::sm_fSlipLaneDotThreshold = 0.9f;
const float CPathFind::sm_fAboveThresholdForPathProbes = 0.5f;
const u32 CPathFind::sm_iHeistsOffset = PATHFINDREGIONS;
const u32 CPathFind::sm_iHeistsSize = 1024;

s32 CPathFind::CPathNodeRequiredArea::m_iNumActiveAreas = 0;
s32 CPathFind::CPathNodeRequiredArea::m_iNumMapAreas = 0;
s32 CPathFind::CPathNodeRequiredArea::m_iNumScriptAreas = 0;
s32 CPathFind::CPathNodeRequiredArea::m_iNumGameAreas = 0;

const Vector3 vSpecialLoc1(-1250.f, -710.f, 0.f);
const Vector3 vSpecialLoc2(-1200.f, -775.f, 0.f);

bool PathNodeYCompare(const CPathNode& data, const CPathNode& key)
{
	return (data.m_coorsY < key.m_coorsY);
}


/////////////////////////////////////////////////////////////////////////////////
// CPathFind
/////////////////////////////////////////////////////////////////////////////////
CPathFind::CPathFind()
	:
	strStreamingModule("Paths", PATHS_FILE_ID, PATHFINDREGIONS + sm_iHeistsSize, false, false, CPathFind::RORC_VERSION, 256)// Note we are saying, for now, that it doesn't require any temporary memory- we set it for real in Init.
{
#if __BANK
	bDisplayPathStreamingDebug = false;
	bDisplayPathHistory = false;
	bDisplayRequiredNodeRegions = false;
	bDisplayRegionBoxes = false;
	bDisplayMultipliers = false;
	bDisplayPathsDebug_Allow = false;
	fDebgDrawDistFlat = 40.0f;
	bDisplayPathsDebug_RoadMesh_AllowDebugDraw = false;
	fDebugAlphaFringePortion = 0.2f;
	bDisplayPathsDebug_Nodes_AllowDebugDraw = false;
	bDisplayPathsDebug_ShowAlignmentErrors = false;
	iDisplayPathsDebug_AlignmentErrorSideSamples = 3;
	fDisplayPathsDebug_AlignmentErrorPointOffsetThreshold = 0.10f;
	fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold = 0.03f;
	bDisplayPathsDebug_Nodes_StandardInfo = true;
	bDisplayPathsDebug_Nodes_Pilons = true;
	bDisplayPathsDebug_Nodes_ToCollisionDiff = false;
	bDisplayPathsDebug_Nodes_StreetNames = false;
	bDisplayPathsDebug_Nodes_DistanceHash = false;
	bDisplayPathsDebug_Nodes_NetworkRestarts	= false;
	bDisplayPathsDebug_Links_AllowDebugDraw = false;
	bDisplayPathsDebug_Links_DrawLine = true;
	bDisplayPathsDebug_Links_DrawLaneDirectionArrows = true;
	bDisplayPathsDebug_Links_DrawTrafficLigths = true;
	bDisplayPathsDebug_PedCrossing_DrawReservationStatus = false;
	bDisplayPathsDebug_PedCrossing_DrawReservationSlots = false;
	bDisplayPathsDebug_PedCrossing_DrawReservationGrid = false;
	bDisplayPathsDebug_Links_TextInfo = false;
	bDisplayPathsDebug_Links_LaneCenters = false;
	bDisplayPathsDebug_Links_RoadExtremes = false;
	bDisplayPathsDebug_Links_Tilt = false;
	bDisplayPathsDebug_Links_RegionAndIndex = false;
	bDisplayPathsDebug_IgnoreSpecial_PedCrossing = false;
	bDisplayPathsDebug_OnlySpecial_PedCrossing = false;
	bDisplayPathsDebug_IgnoreSpecial_PedDriveWayCrossing = false;
	bDisplayPathsDebug_OnlySpecial_PedDriveWayCrossing = false;
	bDisplayPathsDebug_IgnoreSpecial_PedAssistedMovement = false;
	bMakeAllRoadsSingleTrack = false;
	bDisplayPathsOnVMapFlashingForSwitchedOffNodes = false;
	bDisplayPathsDebug_Directions = false;
	bDebug_TestGenerateDirections = false;
	bDisplayPathsOnVMapDontForceLoad = false;
	bDisplayPathsWithDensityOnVMapDontForceLoad = false;
	bDisplayPathsOnVMap = false;
	bDisplayOnlyPlayersRoadNodes = false;
	bDisplayPathsWithDensityOnVMap = false;
	bDisplayNetworkRestartsOnVMap = false;
	bDisplayPathsForTunnelsOnVMapInOrange = false;
	bDisplayPathsForBadSlopesOnVMapInFlashingPurple = false;
	bDisplayPathsForLowPathsOnVMapInFlashingWhite = false;
	bSpewPathfindQueryProgress = false;
	bTestAddPathNodeRequiredRegionAtMeasuringToolPos = false;
	DebugGetRandomCarNode = 0;
	DebugCurrentRegion = 0;
	DebugCurrentNode = 0;

	DebugLinkbGpsCanGoBothWays = 0;
	DebugLinkTilt = 0;							
	DebugLinkNarrowRoad = 0;						
	DebugLinkWidth = 0;							
	DebugLinkDontUseForNavigation = 0;						
	DebugLinkbShortCut = 0;						
	DebugLinkLanesFromOtherNode = 0;
	DebugLinkLanesToOtherNode = 0;				
	DebugLinkDistance = 0;
	DebugLinkBlockIfNoLanes = 0;
	DebugLinkRegion = 65535;
	DebugLinkAddress = 65535;

#endif // __BANK

	ResetPlayerSwitchTarget();
}


#if !__FINAL
/////////////////////////////////////////////////////////////////////////////////
//	GetName
// Work out the original filename from the streaming index.
/////////////////////////////////////////////////////////////////////////////////
const char* CPathFind::GetName(strLocalIndex index) const
{
	Assert(index.Get() <= (s32)(PATHFINDREGIONS + sm_iHeistsSize));

	static char s_szName[64] = "\0";
	sprintf(s_szName, "nodes%02d", index.Get());

	return s_szName;
}
#endif // !__FINAL

/////////////////////////////////////////////////////////////////////////////////
// name:		FindSlot
// description:	re-find the streaming id previously returned by Register 
/////////////////////////////////////////////////////////////////////////////////
strLocalIndex CPathFind::FindSlot(const char* name) const
{
	return const_cast<CPathFind*>(this)->Register(name);
}

strLocalIndex CPathFind::GetStreamingIndexForRegion(u32 iRegion) const
{
	strLocalIndex strIndex = strLocalIndex(bStreamHeistIslandNodes ? (iRegion + sm_iHeistsOffset) : iRegion);
	return strIndex;
}

u32 CPathFind::GetRegionForStreamingIndex(strLocalIndex iIndex) const
{
	if (iIndex.Get() > sm_iHeistsOffset)
	{
		return iIndex.Get() - sm_iHeistsOffset;
	}

	return iIndex.Get();
}

/////////////////////////////////////////////////////////////////////////////////
// name:		Register
// description:	Register path find file with module and return streaming id
/////////////////////////////////////////////////////////////////////////////////
strLocalIndex CPathFind::Register(const char* name)
{
	u32 index;
	// convert string to number. Path file name is nodesXX.nod/ind so we ignore the
	// first 5 letters
	sscanf(name+5, "%d", &index);

#if USE_PAGED_POOLS_FOR_STREAMING
	AllocateSlot(strLocalIndex(index));
#endif // USE_PAGED_POOLS_FOR_STREAMING

	return strLocalIndex(index);
}

/////////////////////////////////////////////////////////////////////////////////
// name:		RemoveSlot
// description:	Make a slot available for future use
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::RemoveSlot(strLocalIndex index)
{
	(void)index;
#if __ASSERT
	u32 iRegion = GetRegionForStreamingIndex(index);
	Assert(!apRegions[iRegion]);
#endif

#if USE_PAGED_POOLS_FOR_STREAMING
	FreeObject(index);
#endif // USE_PAGED_POOLS_FOR_STREAMING
}

/////////////////////////////////////////////////////////////////////////////////
// name:		Remove
// description:	Dumps the data in for a node index.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::Remove(strLocalIndex index)
{
	u32 regionIndex = GetRegionForStreamingIndex(index);

#if __DEV
	if(apRegions[regionIndex])
	{
		CheckPathNodeIntegrityForZone(regionIndex);
	}
	Displayf("Unloading Path Region %d", regionIndex);
#endif // __DEV

	if(apRegions[regionIndex])
	{
		apRegions[regionIndex]->Unload();
	}

// DEBUG!! -AC, MEMLEAK: fix?
	if(apRegions[regionIndex])
	{
		delete apRegions[regionIndex];
		apRegions[regionIndex] = NULL;
	}
// END DEBUG!!
}


/////////////////////////////////////////////////////////////////////////////////
// name:		PlaceResource
// description:	
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::PlaceResource(strLocalIndex index, datResourceMap& map, datResourceInfo& header)
{
	if(Verifyf(index.Get() >= 0, "Streaming trying to place a resource at index %i", index.Get()))
	{
		CPathRegion* pPathRegion = NULL;

#if __FINAL
		pgRscBuilder::PlaceStream(pPathRegion, header, map, "<unknown>");
#else
		char tmp[64];
		sprintf(tmp, "PathRegionIndex:%i", GetRegionForStreamingIndex(index));
		pgRscBuilder::PlaceStream(pPathRegion, header, map, tmp);
#endif

		Load(index, map.GetVirtualBase(), 0);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// name:		Load
// description:	
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::Load(strLocalIndex index, void* object, int UNUSED_PARAM(size))
{
	u32 uRegionIndex = GetRegionForStreamingIndex(index);

	// DEBUG!! -AC, MEMLEAK: fix?
	// Clear the region.
	if(apRegions[uRegionIndex])
	{
		delete apRegions[uRegionIndex];
		apRegions[uRegionIndex] = NULL;
	}
	// END DEBUG!!

	// Fill in the region data.
	apRegions[uRegionIndex] = (CPathRegion*)object;

	// don't mess with dummy regions (they are degenerate and have no nodes
	if(GetStreamingInfo(index)->IsFlagSet(STRFLAG_INTERNAL_DUMMY))
	{
		return true;
	}

	// Clear this regions distance to target... used for path searches.
	Assert(apRegions[uRegionIndex]);
	for (s32 Node = 0; Node < apRegions[uRegionIndex]->NumNodes; Node++)
	{
		Assert(apRegions[uRegionIndex]->aNodes);
		CPathNode* pNode = &apRegions[uRegionIndex]->aNodes[Node];
		Assert(pNode);
		pNode->m_distanceToTarget = PF_VERYLARGENUMBER;
	}

	// The level designers switch nodes on and off. Any of those commands still outstanding have to be applied to the
	// nodes just read in.
	for (s32 NodeSwitch = 0; NodeSwitch < NumNodeSwitches; NodeSwitch++)
	{
		SwitchRoadsOffInAreaForOneRegion(NodeSwitches[NodeSwitch], uRegionIndex);
	}

#if __DEV
	CheckPathNodeIntegrityForZone(uRegionIndex);
#endif

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
//	GetPtr
//	This gets the ptr to a path index from its index 
/////////////////////////////////////////////////////////////////////////////////
void* CPathFind::GetPtr(strLocalIndex index)
{
	u32 uRegionIndex = GetRegionForStreamingIndex(index);

	Assert(uRegionIndex <= (s32)PATHFINDREGIONS);
	return apRegions[uRegionIndex];
}


typedef struct
{
	Vector3 m_vRecentlySpawnedPos;
	u32	m_uiRecentlySpawnedTime;
} SRecentlySpawnedPlayer;
SRecentlySpawnedPlayer gaRecentlySpawnedPlayerPos[MAX_NUM_PHYSICAL_PLAYERS];

#define MAX_NUM_BLOCKING_AREAS 64

typedef struct
{
	Vector3 m_vPos;
	float	m_fRadius;
} SSpawnBlockingAreas;
SSpawnBlockingAreas gaSpawnBlockingAreas[MAX_NUM_BLOCKING_AREAS];
s32				giNumSpawnBlockingAreas = 0;

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Init
// PURPOSE :  Initializes some stuff in the Pathfind struct
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

    if(initMode == INIT_CORE)
    {
	    s32	Region;

	    NumNodeSwitches = 0;
	    for (s32 i = 0; i < MAX_NUM_NODE_REQUESTS; i++)
	    {
		    bActiveRequestForRegions[i] = false;
	    }
	    bLoadAllRegions = false;
		bWillTryAndStreamPrologueNodes = true;	//default to true
		bStreamHeistIslandNodes = false;
		bWasStreamingHeistIslandNodes = false;
		for(int i = 0; i <MAX_GPS_DISABLED_ZONES; ++i)
		{
			m_GPSDisabledZones[i].m_iGpsBlockedByScriptID = 0;
		}
		
    // DEBUG!! -AC, MEMLEAK: fix?
	    // clear the streaming variables. We don't want to build the files afresh.
	    for (Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	    {
		    apRegions[Region] = NULL;
	    }
    // END DEBUG!!

	    m_RequiresTempMemory = false;	//PARAM_buildpaths.Get();
    }
    else if(initMode == INIT_SESSION)
    {
        for (s32 i = 0; i < MAX_NUM_NODE_REQUESTS; i++)
	    {
		    ReleaseRequestedNodes(i);
	    }

	    NumNodeSwitches = 0;		// Make sure nodes switched on/off by the level designers aren't remembered.

	    enum { PF_NUMUSEDNODES = 32 };
	    for (s32 i = 0; i < PF_NUMUSEDNODES; i++)
	    {
		    m_aUsedNodes[i].SetEmpty();
		    m_aTimeUsed[i] = 0;
	    }

	    for( s32 i = 0; i < static_cast<s32>(MAX_NUM_PHYSICAL_PLAYERS); i++ )
	    {
		    gaRecentlySpawnedPlayerPos[i].m_vRecentlySpawnedPos.Zero();
		    gaRecentlySpawnedPlayerPos[i].m_uiRecentlySpawnedTime = 0;
	    }

	    giNumSpawnBlockingAreas = 0;
	    bIgnoreNoGpsFlag = false;
		bIgnoreNoGpsFlagUntilFirstNode = false;

		for(int i = 0; i <MAX_GPS_DISABLED_ZONES; ++i)
		{
			m_GPSDisabledZones[i].Clear();
		}

		DetermineValidPathNodeFiles();
    }

	m_LastValidDirectionsNode.SetEmpty();
	m_iDirection = 0;
	m_iStreetNameHash = 0;

	ResetPlayerSwitchTarget();

#if __BANK
	memset(m_PathHistory, 0, sizeof(PathQuery)*ms_iMaxPathHistory);
	m_iLastPathHistory = ms_iMaxPathHistory-1;
	m_iLastSearch = 0;

	m_bTestToolActive = false;
	memset(&m_TestToolQuery, 0, sizeof(PathQuery));
	m_iNumTestToolNodes = 0;
#endif
}

#if __BANK
void TestGenerateDirections()
{
	CGpsSlot & slot = CGps::GetSlot(GPS_SLOT_WAYPOINT);
	Vector3 vTarget = slot.GetDestination();

	s32 iDir = 0;
	u32 iNameHash = 0;
	float fDistToTurn = 0.0f;
	Vector3 vOutPos;
	ThePaths.GenerateDirections(vTarget, iDir, iNameHash, fDistToTurn, vOutPos);
}

void CPathFind::UpdateTestTool()
{
	static bool bFlash = false;
	if(m_bTestToolActive)
	{
		grcDebugDraw::Sphere(m_TestToolQuery.m_vStartCoords, 1.0f, bFlash ? Color_white : Color_grey, false);
		grcDebugDraw::Sphere(m_TestToolQuery.m_vTargetCoords, 1.0f, bFlash ? Color_white : Color_grey, false);

		for(s32 n=1; n<m_iNumTestToolNodes; n++)
		{
			const CPathNode * n0 = FindNodePointerSafe(m_TestToolNodes[n-1]);
			const CPathNode * n1 = FindNodePointerSafe(m_TestToolNodes[n]);
			if(n0 && n1)
			{
				Vector3 v0,v1;
				n0->GetCoors(v0);
				n1->GetCoors(v1);
				grcDebugDraw::Line(v0+ZAXIS, v1+ZAXIS, Color_magenta, Color_magenta);
			}
		}
	}

	bFlash = !bFlash;
}

void PathfindTestTool_SetStartPos()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
	ThePaths.m_TestToolQuery.m_vStartCoords = vOrigin;
}

void PathfindTestTool_SetEndPos()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
	ThePaths.m_TestToolQuery.m_vTargetCoords = vOrigin;
}

void PathfindTestTool_FindPath()
{
	CNodeAddress empty;
	empty.SetEmpty();

	s32 iNumNodesGiven = 0;

	ThePaths.DoPathSearch(
		ThePaths.m_TestToolQuery.m_vStartCoords,
		empty,
		ThePaths.m_TestToolQuery.m_vTargetCoords,
		ThePaths.m_TestToolNodes,
		&iNumNodesGiven,
		512,
		NULL,
		ThePaths.m_TestToolQuery.m_fCutoffDistForNodeSearch,
		NULL,
		(int)ThePaths.m_TestToolQuery.m_fMaxSearchDistance,
		ThePaths.m_TestToolQuery.m_bDontGoAgainstTraffic,
		empty,
		ThePaths.m_TestToolQuery.m_bAmphibiousVehicle,
		ThePaths.m_TestToolQuery.m_bBoat,
		ThePaths.m_TestToolQuery.m_bReducedList,
		ThePaths.m_TestToolQuery.m_bBlockedByNoGps,
		ThePaths.m_TestToolQuery.m_bForGps,
		ThePaths.m_TestToolQuery.m_bClearDistanceToTarget,
		ThePaths.m_TestToolQuery.m_bMayUseShortCutLinks,
		ThePaths.m_TestToolQuery.m_bIgnoreSwitchedOffNodes
	);
}

#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CPathFind::Update
// PURPOSE :  Does what needs to be done every frame
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::Update(void)
{
	UpdateUsedNodes();

#if __BANK
	if(bDebug_TestGenerateDirections)
	{
		bDisplayPathsDebug_Directions = true;
		TestGenerateDirections();
	}
	UpdateTestTool();

	if(bTestAddPathNodeRequiredRegionAtMeasuringToolPos)
	{
		Vector3 vOrigin = CPhysics::g_vClickedPos[0];
		Vector3 vSize(20.0f, 20.0f, 20.0f);
		AddNodesRequiredRegionThisFrame(CPathFind::CPathNodeRequiredArea::CONTEXT_GAME, vOrigin-vSize, vOrigin+vSize, "test area #1!");

		vOrigin = CPhysics::g_vClickedPos[1];
		AddNodesRequiredRegionThisFrame(CPathFind::CPathNodeRequiredArea::CONTEXT_SCRIPT, vOrigin-vSize, vOrigin+vSize, "test area #2!");
	}
#endif
}





/*
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CheckHashTableIntegrity
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////
void CheckHashTableIntegrity()
{
for (s32 i = 0; i < PF_HASHLISTENTRIES; i++)
{
CPathNode	*pNode = ThePaths.apHashTable[i];

while (pNode)
{
			Assert(pNode->GetAddrRegion() < 64);
			Assert(pNode->GetAddrIndex() < 3000);
Assert(ThePaths.FindNodePointer(pNode->m_address) == pNode);

bool bFound = false;
for (s32 Region = 0; Region < 64; Region++)
{
if (ThePaths.apRegions[Region]->aNodes)
{
if (pNode >= &ThePaths.apRegions[Region]->aNodes[0] &&
pNode <= &ThePaths.apRegions[Region]->aNodes[ThePaths.apRegions[Region]->NumNodes])
{
bFound = true;
}
}
}
Assert(bFound);

pNode = pNode->GetNext();
}
}
}
*/

//-----------------------------------------------------------------------------------
// A version of GetSlipLaneNodeLinkIndex that works on N number of links per junction
// adding this as a new function for now while I evaluate which uses of the old function
// can be switched over safely.
rage::s32 CPathFind::GetSlipLaneNodeLinkIndexNew(const CPathNode * pCurrNode)
{
	if (pCurrNode->IsJunctionNode())
	{
		Vector3 vNodeCoords;
		pCurrNode->GetCoors(vNodeCoords);

		//iterate through each of our links, looking for two that are collinear
		//if we find them, check that one looks like a sliplane and the other doesn't
		for(u32 iLink = 0; iLink < pCurrNode->NumLinks(); iLink++)
		{
			ASSERT_ONLY(CNodeAddress iLinkNodeAddr = GetNodesLinkedNodeAddr(pCurrNode, iLink);)
			Assert(IsRegionLoaded(iLinkNodeAddr));
			const CPathNode* piLinkedNode = GetNodesLinkedNode(pCurrNode, iLink);
			const CPathNodeLink& riLink = GetNodesLink(pCurrNode, iLink);

			Vector3 viNodeCoords;
			piLinkedNode->GetCoors(viNodeCoords);

			Vector3 vBaseToI = viNodeCoords - vNodeCoords;
			vBaseToI.z = 0.0f;
			vBaseToI.NormalizeSafe();

			for (u32 jLink = 0; jLink < pCurrNode->NumLinks(); jLink++)
			{
				if (iLink == jLink)
				{
					continue;
				}

				ASSERT_ONLY(CNodeAddress jLinkNodeAddr = GetNodesLinkedNodeAddr(pCurrNode, jLink);)
				Assert(IsRegionLoaded(jLinkNodeAddr));
				const CPathNode* pjLinkedNode = GetNodesLinkedNode(pCurrNode, jLink);
				const CPathNodeLink& rjLink = GetNodesLink(pCurrNode, jLink);

				{
					//test links
					if (riLink.m_1.m_LanesToOtherNode == 0 &&
						riLink.m_1.m_LanesFromOtherNode == 1 &&
						riLink.m_1.m_Width == 0 &&
						rjLink.m_1.m_Width > 0)
					{
						Vector3 vjNodeCoords;
						pjLinkedNode->GetCoors(vjNodeCoords);

						Vector3 vBaseToJ = vjNodeCoords - vNodeCoords;
						vBaseToJ.z = 0.0f;
						vBaseToJ.NormalizeSafe();

						//test for colinearity
						if (vBaseToI.Dot(vBaseToJ) > 0.9f)
						{
							//i is the sliplane link
							//piLinkedNode->m_2.m_leftTurnsOnly = true;
							//pjLinkedNode->m_1.m_cannotGoLeft = true;
							return iLink;
						}
					}
					else if (rjLink.m_1.m_LanesToOtherNode == 0 &&
						rjLink.m_1.m_LanesFromOtherNode == 1 &&
						rjLink.m_1.m_Width == 0 &&
						riLink.m_1.m_Width > 0)
					{
						Vector3 vjNodeCoords;
						pjLinkedNode->GetCoors(vjNodeCoords);

						Vector3 vBaseToJ = vjNodeCoords - vNodeCoords;
						vBaseToJ.z = 0.0f;
						vBaseToJ.NormalizeSafe();

						//test for colinearity
						if (vBaseToI.Dot(vBaseToJ) > sm_fSlipLaneDotThreshold)
						{
							//j is the sliplane link
							//pjLinkedNode->m_2.m_leftTurnsOnly = true;
							//piLinkedNode->m_1.m_cannotGoLeft = true;
							return jLink;
						}
					}
				}
			}
		}
	}

	return -1;
}

u32 CPathFind::FindDirectionOfUpcomingTurn(const CNodeAddress& /*PrevNode*/, const CPathNode* pCurrNode, const CNodeAddress& NextNode)
{
	if (!pCurrNode)
	{
		return BIT_NOT_SET;
	}

	//Assert(pCurrNode->NumLinks() == 3);

	const CPathNode* pPrevTestNode = pCurrNode;
	const CPathNode* pTestNode = FindNodePointerSafe(NextNode);
	if (!pTestNode)
	{
		return BIT_NOT_SET;
	}

	while (pTestNode && pTestNode->NumLinks() == 2)
	{
		CPathNodeLink rLink = GetNodesLink(pTestNode, 0);
		if (rLink.m_OtherNode == pPrevTestNode->m_address)
		{
			rLink = GetNodesLink(pTestNode, 1);
		}

		pPrevTestNode = pTestNode;
		pTestNode = FindNodePointerSafe(rLink.m_OtherNode);
	}

	if (!pTestNode)
	{
		return BIT_NOT_SET;
	}

	CNodeAddress bestCandidateNode = EmptyNodeAddress;
	//quickly find the exit node of the upcoming junction
	s32 iBestDistance = MAX_INT32;
	for (int i = 0; i < pTestNode->NumLinks(); i++)
	{
		const CPathNodeLink& rLink = GetNodesLink(pTestNode, i);

		//ignore the link we came from
		if (rLink.m_OtherNode == pPrevTestNode->m_address)
		{
			continue;
		}

		const CPathNode* pOtherNode = FindNodePointerSafe(rLink.m_OtherNode);
		Assert(pOtherNode);

		if (pOtherNode->m_distanceToTarget < iBestDistance)
		{
			iBestDistance = pOtherNode->m_distanceToTarget;
			bestCandidateNode = rLink.m_OtherNode;
		}
	}

	float	fLeftness;
	float	fDirectionDot;
	static dev_float thr = 0.92f;
	bool bSharpTurn = false;
	return CPathNodeRouteSearchHelper::FindPathDirection(pPrevTestNode->m_address, pTestNode->m_address, bestCandidateNode, &bSharpTurn, &fLeftness, thr, &fDirectionDot);

}

CNodeAddress HelperFindJunctionPreEntrance(CNodeAddress& junctionEntrance, CNodeAddress& junction)
{
	const CPathNode* pEntrance = ThePaths.FindNodePointerSafe(junctionEntrance);
	if (pEntrance && pEntrance->NumLinks() == 2)
	{
		//only do this if we're sure what the next node is going to be
		if (ThePaths.GetNodesLinkedNodeAddr(pEntrance, 0) == junction)
		{
			return ThePaths.GetNodesLinkedNodeAddr(pEntrance, 1);
		}
		else
		{
			return ThePaths.GetNodesLinkedNodeAddr(pEntrance, 0);
		}
	}

	return EmptyNodeAddress;
}

CNodeAddress HelperFindParentNode(const CPathNode* pCurrentNode)
{
	//find the node for which its m_distanceToTarget == our distance to target + linkDist
	//pBacktrackNode->m_distanceToTarget - linkDist ) == pNeighbourNode->m_distanceToTarget
	Assert(pCurrentNode);
	for (int i = 0; i < pCurrentNode->NumLinks(); i++)
	{
		const CPathNode* pOtherNode = ThePaths.GetNodesLinkedNode(pCurrentNode, i);
		if (!pOtherNode)
		{
			return EmptyNodeAddress;
		}

		const CPathNodeLink& rLink = ThePaths.GetNodesLink(pCurrentNode, i);

		s32 linkDist = rLink.m_1.m_Distance;
		if (pCurrentNode->m_distanceToTarget == pOtherNode->m_distanceToTarget + linkDist)
		{
			return pOtherNode->m_address;
		}
	}

	return EmptyNodeAddress;
}

int HelperGetNumOutboundLinks(const CPathNode* pCurrentNode)
{
	int iNumOutboundLinks = 0;

	Assert(pCurrentNode);
	for (int i = 0; i < pCurrentNode->NumLinks(); i++)
	{
		const CPathNodeLink& rLink = ThePaths.GetNodesLink(pCurrentNode, i);
		if (rLink.m_1.m_LanesToOtherNode > 0)
		{
			++iNumOutboundLinks;
		}
	}

	return iNumOutboundLinks;
}

#if PATHSEARCHDRAW

namespace
{
	const int kPathSearchMaxNodes = 10000;
	Vec3V* s_PathSearchCoordsFrom;
	Vec3V* s_PathSearchCoordsTo;
	Vec3V s_PathSearchStart;
	Vec3V s_PathSearchTarget;
	int s_PathSearchNumCoords;
	bool s_PathSearchEnabled;
	int s_PathDrawSearchLimit = 2000;
	float  s_PathDrawSearchOffsZ = 3.0f;

	void PathSearchDrawInit(Vec3V_In searchStartV, Vec3V_In searchEndV)
	{
		if(!s_PathSearchEnabled)
		{
			return;
		}

		s_PathSearchNumCoords = 0;
		if(!s_PathSearchCoordsFrom)
		{
			USE_DEBUG_MEMORY();
			s_PathSearchCoordsFrom = rage_new Vec3V[kPathSearchMaxNodes];
			s_PathSearchCoordsTo = rage_new Vec3V[kPathSearchMaxNodes];
		}

		s_PathSearchStart = searchStartV;
		s_PathSearchTarget = searchEndV;
	}

	void PathSearchAddLink(const CPathNode& node1, const CPathNode& node2)
	{
		if(!s_PathSearchEnabled || !s_PathSearchCoordsFrom || s_PathSearchNumCoords >= kPathSearchMaxNodes)
		{
			return;
		}

		Vec3V pos1V, pos2V;
		node1.GetCoors(RC_VECTOR3(pos1V));
		node2.GetCoors(RC_VECTOR3(pos2V));

		s_PathSearchCoordsFrom[s_PathSearchNumCoords] = pos1V;
		s_PathSearchCoordsTo[s_PathSearchNumCoords] = pos2V;
		s_PathSearchNumCoords++;
	}

	#if __BANK

	void PathSearchAddWidgets(bkBank& bank)
	{
		bank.AddToggle("Path Search Draw Enabled", &s_PathSearchEnabled);
		bank.AddSlider("Path Search Draw Node count", &s_PathDrawSearchLimit, 0, kPathSearchMaxNodes, 500);
		bank.AddSlider("Path Search Draw Z Offset", &s_PathDrawSearchOffsZ, -1000.0f, 1000.0f, 1.0f);
	}

	#endif	// __BANK

	void PathSearchRender()
	{
		if(!s_PathSearchEnabled || !s_PathSearchCoordsFrom)
		{
			return;
		}

		const Vec3V offsV(0.0f, 0.0f, s_PathDrawSearchOffsZ);
		for(int i = 0; i < Min(s_PathDrawSearchLimit, s_PathSearchNumCoords); i++)
		{
			Vec3V pos1V, pos2V;
			pos1V = s_PathSearchCoordsFrom[i];
			pos2V = s_PathSearchCoordsTo[i];
			Color32 col = Color_orange;
			grcDebugDraw::Line(pos1V + offsV, pos2V + offsV, col, 1);
		}
		grcDebugDraw::Line(s_PathSearchStart + offsV, s_PathSearchTarget + offsV, Color_blue, 1);
	}

}	// anon namespace

#endif	// PATHSEARCHDRAW

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DoPathSearch
// PURPOSE :  Goes through the nodes of the car list and finds the shortest
//			  route between two points. (Current coor and target coor)
//
//			  If (startnode < 0) we use the StartCoors. if it's >= 0 we use that
//			  particular node. This node should not be returned itself.
/////////////////////////////////////////////////////////////////////////////////
static	CNodeAddress	aNodesToBeCleared[CPathFind::PF_MAXNUMNODES_INSEARCH];

static unsigned int sPathSearchHeuristic(const CPathNode& nodeToScore, const CPathNode& nodeTarget)
{
	const int deltaX = nodeToScore.m_coorsX - nodeTarget.m_coorsX;
	const int deltaY = nodeToScore.m_coorsY - nodeTarget.m_coorsY;

	// A few other options for heuristics:
	//
	//	Chebyshev distance, fast to compute, underestimating (except due to the 0.9 modifier we use on some links)
	//		return Max(abs(deltaX), abs(deltaY))/PATHCOORD_XYSHIFT_INT;
	//
	//	Some other estimate with different score for "diagonal movement", if thinking about it as a grid.
	//	This is fast to compute, but probably not actually an underestimation of the Euclidean distance.
	//		const int absDeltaX = abs(deltaX);
	//		const int absDeltaY = abs(deltaY);
	//		const unsigned int h = (256*(absDeltaX + absDeltaY) - 150*rage::Min(absDeltaX, absDeltaY)) >> 8;
	//		return h/PATHCOORD_XYSHIFT_INT;;
	//
	//	Euclidean distance. Slower to compute, but tighter than Chebyshev, expanding quite a lot
	//	fewer nodes. Seems to give us slightly non-optimal paths on occasion though, I think due to
	//	the 0.9 modifier on some costs.
	//		return ((unsigned int)sqrtf(deltaX*deltaX + deltaY*deltaY))/PATHCOORD_XYSHIFT_INT;;
	//	
	// In the end, either the Chebyshev distance or the Euclidean with modifier seems to make
	// the most sense, when comparing in practice. Execution time seems to be roughly the same,
	// with the Euclidean one expanding slightly less nodes but being more expensive to compute,
	// when measuring with a primed cache. It was chosen as it might be the faster choice when
	// taking into account L2 cache misses on a fresh search, guarantees optimality (unlike
	// Chebyshev without a 0.9x modifier), and has no directional bias.

	// Euclidean distance with modifier to make sure it's an underestimation.
	// The 0.9f comes from DISTMULT_SPEED_2 in 'Pathfind_Build.cpp'.
	// Note: may be possible to compute an estimate of this faster without using sqrtf(),
	// in particular when we have a _CountLeadingZeros() instruction.
	return ((unsigned int)(0.9f*sqrtf((float)(deltaX*deltaX + deltaY*deltaY))))/PATHCOORD_XYSHIFT_INT;
}

void CPathFind::DoPathSearch(
	const Vector3& StartCoors, CNodeAddress StartNode, const Vector3& TargetCoors,
	CNodeAddress* pNodeList, s32* pNumNodesGiven, s32 NumNodesRequested,
	float* pDistance, float CutoffDistForNodeSearch, CNodeAddress *pGivenTargetNode, int MaxSearchDistance,
	bool bDontGoAgainstTraffic, CNodeAddress NodeToAvoid, bool bAmphibiousVehicle, bool bBoat,
	bool bReducedList, bool bBlockedByNoGps, bool bForGps, bool bClearDistanceToTarget, bool bMayUseShortCutLinks, 
	bool bIgnoreSwitchedOffNodes, bool bFollowTrafficRules, const CVehicleNodeList* pPreferredNodeListForDestination,
	bool bGpsAvoidHighway, float *pfLaneOffsetList, float *pfLaneWidthList, bool bAvoidRestrictedAreas, bool bAvoidOffroad)
{
	PF_FUNC(DoPathSearch);

#if __BANK

	m_iLastPathHistory = (m_iLastPathHistory+1)%ms_iMaxPathHistory;

	m_PathHistory[m_iLastPathHistory].m_vStartCoords = StartCoors;
	m_PathHistory[m_iLastPathHistory].m_StartNode = StartNode;
	m_PathHistory[m_iLastPathHistory].m_vTargetCoords = TargetCoors;
	m_PathHistory[m_iLastPathHistory].m_fCutoffDistForNodeSearch = CutoffDistForNodeSearch;
	m_PathHistory[m_iLastPathHistory].m_fMaxSearchDistance = (float)MaxSearchDistance;
	m_PathHistory[m_iLastPathHistory].m_bDontGoAgainstTraffic = bDontGoAgainstTraffic;
	m_PathHistory[m_iLastPathHistory].m_NodeToAvoid = NodeToAvoid;
	m_PathHistory[m_iLastPathHistory].m_bAmphibiousVehicle = bAmphibiousVehicle;
	m_PathHistory[m_iLastPathHistory].m_bBoat = bBoat;
	m_PathHistory[m_iLastPathHistory].m_bReducedList = bReducedList;
	m_PathHistory[m_iLastPathHistory].m_bBlockedByNoGps = bBlockedByNoGps;
	m_PathHistory[m_iLastPathHistory].m_bForGps = bForGps;
	m_PathHistory[m_iLastPathHistory].m_bClearDistanceToTarget = bClearDistanceToTarget;
	m_PathHistory[m_iLastPathHistory].m_bMayUseShortCutLinks = bMayUseShortCutLinks;
	m_PathHistory[m_iLastPathHistory].m_bIgnoreSwitchedOffNodes = bIgnoreSwitchedOffNodes;
	
	m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = 0.0f;
	m_PathHistory[m_iLastPathHistory].m_iSearch = m_iLastSearch++;

	sysPerformanceTimer perfTimer("DoPathSearch");
	perfTimer.Start();

#endif // __BANK

	s32			NumNodesToBeCleared;
	s32			Node, CandidateDist;
	s32			C;
	u32			LanesGoingOurWay;
	s32			dbgNodesProcessed = 0;
	//s32			iDistToTarget;
	bool			bStartNodeFound, bDontConsiderThisNeighbour;
	CPathNode		*pCurrNode, *pBacktrackNode, *pCandidateNode, *pNeighbourNode;
	CNodeAddress	TargetNode, CandidateNode, NeighbourNode;

	static dev_s32 s_iOffroadWeightForNoOffroadVehicles = 2;

	bool pathRegionsToClearIfNeeded[PATHFINDREGIONS];
	sysMemSet(pathRegionsToClearIfNeeded, 0, sizeof(pathRegionsToClearIfNeeded));

#ifdef SHARE_SPRITE_VERTEX_BUFFER
	extern CPPSpriteBuffer		PPSpriteBuffer;
	// This must be a buffer big enough for 20K (500 nodes * 
	CNodeAddress	*aNodesToBeCleared = (CNodeAddress*) &PPSpriteBuffer.m_BigImVertexBuffer.vertexBuffer;
	Assert(sizeof(CPPSpriteBuffer)-16 > CPathFind::PF_MAXNUMNODES_INSEARCH * sizeof(CNodeAddress));
#endif

#if PATHSEARCHDRAW
	PathSearchDrawInit(VECTOR3_TO_VEC3V(StartCoors), VECTOR3_TO_VEC3V(TargetCoors));
#endif	// PATHSEARCHDRAW

	if (bIgnoreNoGpsFlag)
	{
		bBlockedByNoGps = false;
	}

#if SERIOUSDEBUG
	ASSERT_ONLY(VerifyAllNodesDistanceToTarget();)
#endif // SERIOUSDEBUG

	NumNodesToBeCleared = 0;			// we keep track of the nodes to be cleared in an array

	// Find the node closest to the target
	if ((!pGivenTargetNode) || pGivenTargetNode->IsEmpty() || !IsRegionLoaded(*pGivenTargetNode))
	{
		TargetNode = FindNodeClosestToCoors(TargetCoors, CutoffDistForNodeSearch, IncludeSwitchedOffNodes, false, bBoat
			, false, false, false, 0.0f, 0.0f, 3.0f, 0.f, -1, false, false, pPreferredNodeListForDestination);
	}
	else
	{
		TargetNode = *pGivenTargetNode;
	}

	if (TargetNode.IsEmpty())
	{
		*pNumNodesGiven = 0;
		if (pDistance) *pDistance = 100000.0f;

#if __BANK
		perfTimer.Stop();
		m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = (float)perfTimer.GetTimeMS();

		if (bSpewPathfindQueryProgress)
		{
			aiDisplayf("DoPathSearch: Target node empty, query failed");
		}
#endif
		return;
	}

	// Find the node closest to the start
	if (StartNode.IsEmpty() || !IsRegionLoaded(StartNode))
	{
		NodeConsiderationType eNodeConsiderationType = IncludeSwitchedOffNodes;
		StartNode = FindNodeClosestToCoors(StartCoors, CutoffDistForNodeSearch, eNodeConsiderationType, false, bBoat);
	}

	if (StartNode.IsEmpty())
	{
		*pNumNodesGiven = 0;
		if (pDistance) *pDistance = 100000.0f;

#if __BANK
		perfTimer.Stop();
		m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = (float)perfTimer.GetTimeMS();

		if (bSpewPathfindQueryProgress)
		{
			aiDisplayf("DoPathSearch: Start node empty, query failed");
		}
#endif
		return;
	}

#if __BANK
	if (bSpewPathfindQueryProgress)
	{
		aiDisplayf("DoPathSearch: Start Node: %d:%d Target Node %d:%d", StartNode.GetRegion(), StartNode.GetIndex(), TargetNode.GetRegion(), TargetNode.GetIndex());
	}
#endif //__BANK

	if (StartNode == TargetNode)
	{
		*pNumNodesGiven = 0;
		if (pDistance) *pDistance = 0.0f;

#if __BANK
		perfTimer.Stop();
		m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = (float)perfTimer.GetTimeMS();

		if (bSpewPathfindQueryProgress)
		{
			aiDisplayf("DoPathSearch: Start Node Same as Target Node, quuery success");
		}
#endif
		return;
	}

	CPathNode* pTargetNode = FindNodePointer(TargetNode);
	CPathNode* pStartNode = FindNodePointer(StartNode);


	bool bUseNoGpsUntilFirstNode = false;
	if (!bIgnoreNoGpsFlag && bIgnoreNoGpsFlagUntilFirstNode && pTargetNode->m_2.m_noGps)
	{
		bBlockedByNoGps = false;
		bUseNoGpsUntilFirstNode = true;
	}

	//aiAssertf(!bIgnoreSwitchedOffNodes || !pTargetNode->m_2.m_switchedOff, "DF_IgnoreSwitchedOffNodes set to TRUE but destination is on a switched off node!  Pathfinding there anyway");
	//aiCondLogf(!bIgnoreSwitchedOffNodes || !pTargetNode->m_2.m_switchedOff, DIAG_SEVERITY_WARNING, "DF_IgnoreSwitchedOffNodes set to TRUE but destination is on a switched off node!  Pathfinding there anyway");

	// This check is no longer valid when going from nogps to nogps
	// Might reinstate this later on when we have fixed all islands
	const bool bOnDifferentIslands = pTargetNode->m_1.m_group != pStartNode->m_1.m_group;
	const bool bCareAboutNoGPSIslands = bBlockedByNoGps && bForGps;
	const bool bIsNoGPSIsland = bOnDifferentIslands && (pTargetNode->m_2.m_noGps || pStartNode->m_2.m_noGps);
	if (bOnDifferentIslands && (bCareAboutNoGPSIslands || !bIsNoGPSIsland))
	{
		*pNumNodesGiven = 0;
		if (pDistance) *pDistance = 100000.0f;

#if __BANK
		perfTimer.Stop();
		m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = (float)perfTimer.GetTimeMS();
#endif
		return;
	}

	// Initially the hash lists are emptyAddr
	for (C = 0; C < PF_HASHLISTENTRIES; C++)
	{
		apHashTable[C] = NULL;
	}

	// Compute an estimate from the first node we will add to the OPEN set.
	const int initialRemainingDist = sPathSearchHeuristic(*pTargetNode, *pStartNode);

	// Put the target node on the list
	AddNodeToList(pTargetNode, 0, initialRemainingDist);

	// We will keep track of the range of distances that are potentially used in the hash table.
	int hashTableSmallestDist = initialRemainingDist;
	int hashTableLargestDist = initialRemainingDist;

	int NodesOnListDuringPathfinding = 1;
	if (NumNodesToBeCleared < CPathFind::PF_MAXNUMNODES_INSEARCH)
	{
		aNodesToBeCleared[NumNodesToBeCleared++] = pTargetNode->m_address;		// Make sure it gets cleared
		Assert(pTargetNode->GetAddrRegion() < PATHFINDREGIONS);
		pathRegionsToClearIfNeeded[pTargetNode->GetAddrRegion()] = true;
	}

	int dbgNumberOfIters = 0;

	//if the start node is switched off, we need to be able to search switched off nodes
	//for it, only bailing out if we reach another switched on node before reaching the start
	const bool bStartNodeIsSwitchedOff = pStartNode->IsSwitchedOff();

	// We are now ready to start processing the list. We will keep processing nodes
	// (and adding their neighbours on the list) until we find the start-node.
	bStartNodeFound = false;
	while (!bStartNodeFound)
	{
		// If we end up in an infinite loop of some sort, break out.
		if(!Verifyf(dbgNumberOfIters++ < 50000, "Stuck in DoPathSearch"))
		{
			break;
		}

		// We want to look at the node with the best estimate for the total cost,
		// so we find the hash table slot associated with the smallest cost, which
		// we have been keeping track of.
		Assert(hashTableSmallestDist >= 0);
		const int currentDist = hashTableSmallestDist;
		const int CurrentHashValue = FindHashValue(hashTableSmallestDist);

		// Go through the list for this Hash value.
		pCurrNode = apHashTable[CurrentHashValue];

		// It's possible that by exploring this node, we find some nodes which use
		// the same hash slot as the one we are iterating over. We will keep track of that.
		bool addedToThisDistList = false;

		// Loop over the nodes at this distance. Also, we break out if hashTableSmallestDist
		// changed - that means that we found a better node than the ones in the current list.
		while (pCurrNode != NULL && hashTableSmallestDist == currentDist)
		{
			// Due to how the hash table slots work using a simple modulus of the distance,
			// if the range of distances is small enough, each hash table slot will only be
			// used for that distance. The case when this isn't true tends to be when we have
			// found NodeToAvoid and added 1000 to the distance estimate, but in practice, that
			// tends to be a node close to the pathfinding start position, so once we are there,
			// hopefully we won't have to explore too many more nodes anyway.
			if(hashTableLargestDist - hashTableSmallestDist >= PF_HASHLISTENTRIES)
			{
				// To deal with this collision, we will check if the total estimated cost is the one we
				// are interested in. We only store the accumulated cost along the edges leading to this node, but we don't
				// store the estimate of the rest, so we have to recompute it - this is probably OK as long
				// as it doesn't happen too much.
				const int distForThisNodeInHash = pCurrNode->m_distanceToTarget + sPathSearchHeuristic(*pCurrNode, *pStartNode);
				if(distForThisNodeInHash != currentDist)
				{
					// This must be a node further away than what we want to look at right now (it should
					// not be possible for it to be closer, since we are looking at hashTableSmallestDist).
					Assert(distForThisNodeInHash >= currentDist);
					pCurrNode = pCurrNode->GetNext();
					continue;
				}
			}
			else
			{
				// This should hold up if we did everything right, ensuring that the node we are about
				// to look at has the right distance.
				Assert((int)(pCurrNode->m_distanceToTarget + sPathSearchHeuristic(*pCurrNode, *pStartNode)) == currentDist);
			}

			dbgNodesProcessed++;
			Assert(CurrentHashValue < PF_HASHLISTENTRIES);

			// Check whether this is one of the nodes we're looking for.
			if ( pCurrNode == pStartNode )
			{
#if __BANK
				if (bSpewPathfindQueryProgress)
				{
					aiDisplayf("DoPathSearch: Start node found");
				}
#endif //__BANK
				bStartNodeFound = true;

				// Don't bother looking at any more nodes at this distance, we have found
				// the destination.
				break;
			}

			// Testing here to avoid a crash where the current node's region would become invalid
			const int Region=pCurrNode->GetAddrRegion();			
			if(!aiVerifyf(Region>=0 && Region<PATHFINDREGIONS+MAXINTERIORS && apRegions[Region] && apRegions[Region]->aNodes, 
				"Something went very wrong during pathfind search, aborting. Last candidate node was region %d node %d", CandidateNode.GetRegion(), CandidateNode.GetIndex()))
			{
				aiErrorf("Pathfind search aborted due to invalid nodes - last valid node was region %d node %d", CandidateNode.GetRegion(), CandidateNode.GetIndex());
				// Don't forget to clear the nodes first.
				if (NumNodesToBeCleared < PF_MAXNUMNODES_INSEARCH)
				{
					for (Node = 0; Node < NumNodesToBeCleared; Node++)
					{
						FindNodePointer(aNodesToBeCleared[Node])->m_distanceToTarget = PF_VERYLARGENUMBER;
					}
				}
				else
				{
					ClearAllNodesDistanceToTarget(pathRegionsToClearIfNeeded);
				}
				if (pDistance) *pDistance = 100000.0f;

#if SERIOUSDEBUG
				ASSERT_ONLY(VerifyAllNodesDistanceToTarget();)
#endif // SERIOUSDEBUG

#if __BANK
				perfTimer.Stop();
				m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = (float)perfTimer.GetTimeMS();

				if (bSpewPathfindQueryProgress)
				{
					aiDisplayf("DoPathSearch: Search didn't find start node, NodesOnList: %d, MaxSearchDist: %d "
						, NodesOnListDuringPathfinding, MaxSearchDistance);
				}

#endif

				return;
			}

			// Process the neighbours
			u32 nLinks = pCurrNode->NumLinks();
			for(u32 link = 0; link < nLinks; link++)
			{
				const CPathNodeLink* pLink = &GetNodesLink(pCurrNode, link);
				CandidateNode = pLink->m_OtherNode;

				Assert(CandidateNode.GetRegion() < PATHFINDREGIONS);		//temp
				//Assert(CandidateNode.GetIndex() < 5000);	//temp

				if (IsRegionLoaded(CandidateNode))		// If this Region is not loaded we ignore this particular node
				{
					pCandidateNode = FindNodePointer(CandidateNode);

					Assert(pCandidateNode->m_address == pLink->m_OtherNode);	//temp

#if PATHSEARCHDRAW
					PathSearchAddLink(*pCurrNode, *pCandidateNode);
#endif	// PATHSEARCHDRAW

					// If going to this node would take us against the flow of traffic we ignore it.
					bDontConsiderThisNeighbour = false;
					LanesGoingOurWay = pLink->m_1.m_LanesFromOtherNode;
					const bool bLinkIntraversableIfNoLanes = pLink->m_1.m_bBlockIfNoLanes && !bForGps;
					if ((bDontGoAgainstTraffic || bLinkIntraversableIfNoLanes) && !(bForGps && pLink->m_1.m_bGpsCanGoBothWays) )
					{	
						// The following bit of code may seem the wrong way round but that is because we are doing a search from the target back to the start point.
						if (LanesGoingOurWay == 0)
						{
#if __BANK					
							if (bSpewPathfindQueryProgress)
							{
								aiDisplayf("DoPathSearch: Node %d:%d not considered, No Lanes Our Way", CandidateNode.GetRegion(), CandidateNode.GetIndex());
							}					
#endif //__BANK
							bDontConsiderThisNeighbour = true;
						}
					}
					// If this is a shortcut link, we may only use it if explicitly allowed
					if(!bDontConsiderThisNeighbour && pLink->IsShortCut() && !bMayUseShortCutLinks) 
					{
#if __BANK					
						if (bSpewPathfindQueryProgress)
						{
							aiDisplayf("DoPathSearch: Node %d:%d not considered, Shortcut links not allowed", CandidateNode.GetRegion(), CandidateNode.GetIndex());
						}					
#endif //__BANK
						bDontConsiderThisNeighbour = true;
					}

					if (!bDontConsiderThisNeighbour && bAvoidRestrictedAreas && pCandidateNode->IsRestrictedArea() && !pCurrNode->IsRestrictedArea())
					{
#if __BANK					
						if (bSpewPathfindQueryProgress)
						{
							aiDisplayf("DoPathSearch: Node %d:%d not considered, Restricted areas not allowed", CandidateNode.GetRegion(), CandidateNode.GetIndex());
						}					
#endif //__BANK
						bDontConsiderThisNeighbour = true;
					}

					// Only amphibious vehicles can go from water to land nodes.
					if (!bDontConsiderThisNeighbour && pCandidateNode->m_2.m_waterNode != pCurrNode->m_2.m_waterNode && !bAmphibiousVehicle) 
					{
#if __BANK					
						if (bSpewPathfindQueryProgress)
						{
							aiDisplayf("DoPathSearch: Node %d:%d not considered, Going from water to land", CandidateNode.GetRegion(), CandidateNode.GetIndex());
						}					
#endif //__BANK
						bDontConsiderThisNeighbour = true;	
					}
					// If we do a search for the gps certain roads are blocked (alleyways, petrol stations etc)
					if (!bDontConsiderThisNeighbour && bBlockedByNoGps && pCandidateNode->m_2.m_noGps) 
					{
#if __BANK					
						if (bSpewPathfindQueryProgress)
						{
							aiDisplayf("DoPathSearch: Node %d:%d not considered, NoGPS flag set", CandidateNode.GetRegion(), CandidateNode.GetIndex());
						}					
#endif //__BANK
						bDontConsiderThisNeighbour = true;
					}
					// In rare cases by mistake pednodes are connected to non ped nodes.
					if (!bDontConsiderThisNeighbour && pCandidateNode->IsPedNode() != pCurrNode->IsPedNode() ) 
					{
#if __BANK					
						if (bSpewPathfindQueryProgress)
						{
							aiDisplayf("DoPathSearch: Node %d:%d not considered, Is a Ped Node", CandidateNode.GetRegion(), CandidateNode.GetIndex());
						}					
#endif //__BANK
						bDontConsiderThisNeighbour = true;
					}

					//if the start node is on a switched off node, allow going from switched on->switched off
					//ideally, we would do some sort of search/flood fill to find the section of switched off nodes
					//the start node is on and only allow considering those neighbors, but because we use the node data
					//sitting in resource memory for temporary pathfinding data, there isn't an easy place to put a new flag
					//to enable doing this without extra searches through an array/list.
					//I think this should be fine though, because we re-pathfind every 7 nodes or so.
					//so any searches that originate once the car is on the road will not have bStartNodeIsSwitchedOff 
					//set to true and therefore won't take any alleyways or whatnot to get to the destination, unless
					//the user requests it with !bIgnoreSwitchedOffNodes
					if (!bDontConsiderThisNeighbour && (bIgnoreSwitchedOffNodes && !bStartNodeIsSwitchedOff) && pCandidateNode->m_2.m_switchedOff && !pCurrNode->m_2.m_switchedOff)
					{
						bDontConsiderThisNeighbour = true;
#if __BANK					
						if (bSpewPathfindQueryProgress)
						{
							aiDisplayf("DoPathSearch: Node %d:%d not considered, Ignoring switched off nodes", CandidateNode.GetRegion(), CandidateNode.GetIndex());
						}					
#endif //__BANK
					}

					for(int i = 0; i < MAX_GPS_DISABLED_ZONES; ++i)
					{
						if(!bDontConsiderThisNeighbour && bForGps && m_GPSDisabledZones[i].m_iGpsBlockedByScriptID )
						{
							if( pCandidateNode->m_coorsX < m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX || pCandidateNode->m_coorsX > m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX ||
								pCandidateNode->m_coorsY < m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY || pCandidateNode->m_coorsY > m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY )
							{
								// no intersection
							}
							else
							{
#if __BANK					
								if (bSpewPathfindQueryProgress)
								{
									aiDisplayf("DoPathSearch: Node %d:%d not considered, GPS blocked by script", CandidateNode.GetRegion(), CandidateNode.GetIndex());
								}					
#endif //__BANK
								bDontConsiderThisNeighbour = true;
							}
						}
					}

					//test for a left turn. if the vehicle wants to make a left turn at this junction,
					//don't consider the straight ahead link. if they don't want to go left, don't consider
					//the slip lane
					//but only if there's more than one way out
					if (!bDontConsiderThisNeighbour && bFollowTrafficRules)
					{
						bool bSharpTurn = false;
						const bool bHasMultipleWaysOutOfJunction = pCurrNode->IsJunctionNode() && (!bDontGoAgainstTraffic || HelperGetNumOutboundLinks(pCurrNode) > 1);
						if (bHasMultipleWaysOutOfJunction)
						{
							CNodeAddress junctionPreEntrance = HelperFindJunctionPreEntrance(CandidateNode, pCurrNode->m_address);
							CNodeAddress junctionExit = HelperFindParentNode(pCurrNode);
							float fLeftness, fDotProduct;
							u32 turnDir = BIT_NOT_SET;
							if (!bForGps)
							{
								turnDir = CPathNodeRouteSearchHelper::FindJunctionTurnDirection(junctionPreEntrance, CandidateNode, pCurrNode->m_address, junctionExit, &bSharpTurn, &fLeftness, CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForRoadStraightAhead, &fDotProduct);
							}
							else
							{
								const CPathNode* pToEnterNode = ThePaths.FindNodePointerSafe(junctionPreEntrance);
								const CPathNode* pEnterNode = ThePaths.FindNodePointerSafe(CandidateNode);
								const CPathNode* pToExitNode = pCurrNode;
								const CPathNode* pExitNode = ThePaths.FindNodePointerSafe(junctionExit);

								if (pToEnterNode && pEnterNode && pToExitNode && pExitNode)
								{
									Vector2 vToEnter, vEnter, vToExit, vExit;
									pToEnterNode->GetCoors2(vToEnter);
									pEnterNode->GetCoors2(vEnter);
									pToExitNode->GetCoors2(vToExit);
									pExitNode->GetCoors2(vExit);

									Vector2 vJEnter = vEnter - vToEnter;
									Vector2 vJExit = vExit - vToExit;

									turnDir = CPathNodeRouteSearchHelper::FindPathDirectionInternal(vJEnter, vJExit, &bSharpTurn, &fLeftness, CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForRoadStraightAhead, &fDotProduct);
								}
							}

							if (turnDir == BIT_TURN_LEFT && pCandidateNode->m_1.m_cannotGoLeft)
							{
#if __BANK					
								if (bSpewPathfindQueryProgress)
								{
									aiDisplayf("DoPathSearch: Node %d:%d not considered, Node doesn't allow turning left", CandidateNode.GetRegion(), CandidateNode.GetIndex());
								}					
#endif //__BANK
								bDontConsiderThisNeighbour = true;
							}
							else if (turnDir != BIT_TURN_LEFT && turnDir != BIT_NOT_SET && pCandidateNode->m_2.m_leftOnly && LanesGoingOurWay <= 1)
							{
#if __BANK					
								if (bSpewPathfindQueryProgress)
								{
									aiDisplayf("DoPathSearch: Node %d:%d not considered, Node forces a left turn here, turnDir is %d", CandidateNode.GetRegion(), CandidateNode.GetIndex(), turnDir);
								}					
#endif //__BANK
								bDontConsiderThisNeighbour = true;
							}
							else if (turnDir == BIT_TURN_RIGHT && pCandidateNode->m_1.m_cannotGoRight)
							{
#if __BANK					
								if (bSpewPathfindQueryProgress)
								{
									aiDisplayf("DoPathSearch: Node %d:%d not considered, Node doesn't allow turning right", CandidateNode.GetRegion(), CandidateNode.GetIndex());
								}					
#endif //__BANK
								bDontConsiderThisNeighbour = true;
							}
						}
					}

					if (!bDontConsiderThisNeighbour)
					{				
						if(!bBlockedByNoGps && bUseNoGpsUntilFirstNode && !pCandidateNode->m_2.m_noGps)
							bBlockedByNoGps = true;

						CandidateDist = pCurrNode->m_distanceToTarget + (s16)pLink->m_1.m_Distance;

						// To prevent going into sliplanes back and forth for the route unless it really should use those slipnodes
						if (bForGps)
						{
							CandidateDist += (s32)pCandidateNode->m_1.m_slipLane;
							CandidateDist += (s32)pLink->m_1.m_bShortCut * 15;
						}

						if (LanesGoingOurWay == 0)		// If we're going against traffic we add a 50% distance penalty to discourage cars from going against traffic. (even if they're allowed to)
						{
							CandidateDist += (s16)(pLink->m_1.m_Distance>>1);
						}

						if (bGpsAvoidHighway && pCandidateNode->IsHighway() && !pCandidateNode->m_2.m_qualifiesAsJunction)
						{
							CandidateDist += 151;	// Like really avoid them
						}

						//add a 50% penalty for switched off nodes
						if (pCandidateNode->IsSwitchedOff())
						{
							CandidateDist += (s16)(pLink->m_1.m_Distance>>1);
						}

						//add a variable penalty for offroad nodes if we don't wanna go offroad
						if (bAvoidOffroad && pCandidateNode->IsOffroad())
						{
							CandidateDist += (s16)(pLink->m_1.m_Distance * s_iOffroadWeightForNoOffroadVehicles);
						}

						//add a 12.5% penalty for slipnodes
						if (pCandidateNode->IsSlipLane())
						{
							CandidateDist += (s16)(pLink->m_1.m_Distance>>3);
						}

						//add a really high score for the NodeToAvoid
						if (CandidateNode == NodeToAvoid)
						{
							CandidateDist += 1000;
						}

						// Assert(pLink->m_1.m_Distance < PF_HASHLISTENTRIES); Always true as m_1.m_Distance = u8 and PF_HASHLISTENTRIES = 512
						Assert(pLink->m_1.m_Distance > 0);
						Assert(pCandidateNode != pCurrNode);
						if (CandidateDist < pCandidateNode->m_distanceToTarget)
						{
							// Use the heuristic function to compute an estimate of the total distance.
							const int totalEstimatedDist = CandidateDist + sPathSearchHeuristic(*pCandidateNode, *pStartNode);

							// Check if this estimate is less than the distance we are willing to search. Since it's
							// supposed to be an underestimate, there is no need to add it otherwise.
							if(totalEstimatedDist <= MaxSearchDistance)
							{
								// The distance we found for this node is smaller than the distance we
								// already had. This means that it has to be added to the linked list
								// of that specific distance. (And it would have to be removed from the
								// linked list it might be in at the moment).
								// Note: the GetPrevious() check is potentially important here: if we are not
								// actually in the OPEN set, we wouldn't want to decrease NodesOnListDuringPathfinding, etc.
								if (pCandidateNode->m_distanceToTarget != PF_VERYLARGENUMBER && pCandidateNode->GetPrevious())
								{
									RemoveNodeFromList(pCandidateNode);
									pCandidateNode->SetNext(NULL);
									pCandidateNode->SetPrevious(NULL);
									--NodesOnListDuringPathfinding;
								}

								// The distance value of this node is going to be written to so we
								// have to make sure it gets cleared when we're done.
								if (pCandidateNode->m_distanceToTarget == PF_VERYLARGENUMBER)
								{
									if (NumNodesToBeCleared < PF_MAXNUMNODES_INSEARCH)	// Only add nodes if there is enough space.
									{
										aNodesToBeCleared[NumNodesToBeCleared++] = pCandidateNode->m_address;
									}
									Assert(pCandidateNode->GetAddrRegion() < PATHFINDREGIONS);
									pathRegionsToClearIfNeeded[pCandidateNode->GetAddrRegion()] = true;
								}

								// Maintain the range of distances in the hash table.
								if(hashTableSmallestDist < 0)
								{
									// Hash table was empty, set smallest and largest to the new distance.
									hashTableSmallestDist = hashTableLargestDist = totalEstimatedDist;
								}
								else if(totalEstimatedDist < hashTableSmallestDist)
								{
									// Update the lower bound.
									hashTableSmallestDist = totalEstimatedDist;
								}
								else if(totalEstimatedDist > hashTableLargestDist)
								{
									// Update the higher bound.
									hashTableLargestDist = totalEstimatedDist;
								}
								// Add the node to the OPEN set. CandidateDist will be stored as
								// pCandidateNode->m_distanceToTarget, and totalEstimatedDist will
								// be used to compute the hash table slot.
								AddNodeToList(pCandidateNode, CandidateDist, totalEstimatedDist);
								++NodesOnListDuringPathfinding;

								// Check if we just added something at the same distance as the
								// node we were visiting.
								if(totalEstimatedDist == currentDist)
								{
									addedToThisDistList = true;
								}
							}
						}
					}
				}
#if __BANK
				else
				{
					if (bSpewPathfindQueryProgress)
					{
						aiDisplayf("DoPathSearch: Node %d:%d region not loaded, not considering", CandidateNode.GetRegion(), CandidateNode.GetIndex());
					}
				}
#endif //__BANK
			}

			// Remove ourselves from the list
			CPathNode* pNextNode = pCurrNode->GetNext();
			RemoveNodeFromList(pCurrNode);
			--NodesOnListDuringPathfinding;

			// It's important to clear these out now (RemoveNodeFromList() doesn't do it internally), since we might
			// (depending on the heuristics) find this node again later in the search, when it's out of the OPEN set.
			// If so, we should not try to remove it again, and we rely on the previous pointer to determine that.
			pCurrNode->SetNext(NULL);
			pCurrNode->SetPrevious(NULL);

			// Find next node to process
			pCurrNode = pNextNode;
		}

		// Nothing more needs to be done here if we have found the start node.
		if(bStartNodeFound)
		{
			break;
		}

		// Check if hashTableSmallestDist is still the same as before we started iterating on this
		// list. If so, we have iterated over all the nodes at this distance and found nothing better.
		// Move on to the next distance. Note: on occasion, we might have added a node to (the beginning
		// of) our current list. If so, we simply stay at the current distance and look in the same list again.
		if(hashTableSmallestDist == currentDist && !addedToThisDistList)
		{
			// Check if this was the last node we had in the hash table, according to our range variables.
			if(hashTableSmallestDist >= hashTableLargestDist)
			{
				Assert(hashTableSmallestDist == hashTableLargestDist);
				Assert(NodesOnListDuringPathfinding == 0);

				// Hash table appears to be empty, set the limits to -1.
				hashTableSmallestDist = hashTableLargestDist = -1;
			}
			else
			{
				// Move on to the next distance.
				hashTableSmallestDist++;
			}
		}
		else
		{
			// Make sure hashTableSmallestDist didn't increase, that would be strange.
			Assert(hashTableSmallestDist <= currentDist);
		}

		/*
		DEBUGPATHFIND({
		if(!bStartNodeFound)	// Make sure we're not already done(in that case list is allowed to be emptyAddr)
		{
		s16	TestHashValues = 0;
		while ( (!aHashTable[TestHashValues].GetNext()) && (TestHashValues < PF_HASHLISTENTRIES) )
		{
		TestHashValues++;
		}
		if (TestHashValues == PF_HASHLISTENTRIES)
		{
		char	Str[80];
		Displayf(Str, "NodesProc:%d (peds:%d cars:%d)", dbgNodesProcessed,
		NumNodesPedNodes, NumNodesCarNodes);
		//			Errorf(Str);	// Path finding got stuck. Unconnected grid ?
		}
		}
		})	
		*/
		if (!bStartNodeFound && NodesOnListDuringPathfinding == 0)
		{		
			// We didn't find the target. Perhaps because a block of path data wasn't streamed in?
			*pNumNodesGiven = 0;

			// Don't forget to clear the nodes first.
			if (NumNodesToBeCleared < PF_MAXNUMNODES_INSEARCH)
			{
				for (Node = 0; Node < NumNodesToBeCleared; Node++)
				{
					FindNodePointer(aNodesToBeCleared[Node])->m_distanceToTarget = PF_VERYLARGENUMBER;
				}
			}
			else
			{
				ClearAllNodesDistanceToTarget(pathRegionsToClearIfNeeded);
			}
			if (pDistance) *pDistance = 100000.0f;

#if SERIOUSDEBUG
			ASSERT_ONLY(VerifyAllNodesDistanceToTarget();)
#endif // SERIOUSDEBUG

#if __BANK
			perfTimer.Stop();
			m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = (float)perfTimer.GetTimeMS();


			if (bSpewPathfindQueryProgress)
			{
				aiDisplayf("DoPathSearch: Search didn't find start node, NodesOnList: %d, MaxSearchDist: %d "
					, NodesOnListDuringPathfinding, MaxSearchDistance);
			}

#endif

			return;
		}

	}

	//const float fSameDirectionDot = 0.992f;
	//const float fSameDirectionDot = Cosf(7.0f * DtoR);
	static dev_float fSameDirectionDot = Cosf(4.0f * DtoR);

	// The nodes now all have a value that represents their distance to the
	// target. We now have to do the steepest descent thing to find the actual
	// nodes on the path.

	// We could start backtracking with any of the nodes on the road tile
	// that we're on. We pick the one that results in the smallest overall
	// distance to the target.(distABFlat to that node + distABFlat from node to target)

	s32 nNodesGiven = 0;
	if (pDistance) 
		*pDistance = pStartNode->m_distanceToTarget;

	//Add the start node.
	if (pNodeList)
		pNodeList[nNodesGiven++] = pStartNode->m_address;

	pBacktrackNode = pStartNode;

#if __BANK
	if (bSpewPathfindQueryProgress)
	{
		aiDisplayf("DoPathSearch: Adding Node %d:%d to path", pBacktrackNode->GetAddrRegion(), pBacktrackNode->GetAddrIndex());
	}
#endif //__BANK

	CPathNode* pNodeCache[2] = {pBacktrackNode, NULL};

	//Add all the remaining nodes.
	bool bLastNoNav = false;
	bool bLastPreserve = false;
	int nShortcutPreserve = 0;
	float fLastOffset = 0.f;
	float fLastWidth = 0.f;
	while (nNodesGiven < NumNodesRequested && pBacktrackNode != pTargetNode)
	{
		const CPathNodeLink* pChosenNeighbourLink = NULL;

		// For each node test the neighbours and pick the one with the right
		// distance value (steepest descent)
		u32 nLinks = pBacktrackNode->NumLinks();
		for(u32 link = 0; link < nLinks; link++)
		{
			const CPathNodeLink* pNeighbourLink = &GetNodesLink(pBacktrackNode, link);
			NeighbourNode = pNeighbourLink->m_OtherNode;

			if (IsRegionLoaded(NeighbourNode))
			{
				pNeighbourNode = FindNodePointer(NeighbourNode);
				s32 linkDist = pNeighbourLink->m_1.m_Distance;

				// To prevent going into sliplanes back and forth for the route unless it really should use those slipnodes
				if (bForGps)
				{
					linkDist += (s32)pBacktrackNode->m_1.m_slipLane;
					linkDist += (s32)pNeighbourLink->m_1.m_bShortCut * 15;
				}

				if (pNeighbourLink->m_1.m_LanesToOtherNode == 0)		// If we're going against traffic we add a 25% distance penalty to discourage cars from going against traffic. (even if they're allowed to)
				{
					linkDist += (s16)(pNeighbourLink->m_1.m_Distance>>1);
				}

				if (bGpsAvoidHighway && pBacktrackNode->IsHighway() && !pBacktrackNode->m_2.m_qualifiesAsJunction)
				{
					linkDist += 151;	// Like really avoid them
				}

				//add a 50% penalty for switched off nodes
				if (pBacktrackNode->IsSwitchedOff())
				{
					linkDist += (s16)(pNeighbourLink->m_1.m_Distance>>1);
				}

				//add a variable penalty for offroad nodes if we don't wanna go offroad
				if (bAvoidOffroad && pBacktrackNode->IsOffroad())
				{
					linkDist += (s16)(pNeighbourLink->m_1.m_Distance * s_iOffroadWeightForNoOffroadVehicles);
				}

				//add a 12.5% penalty for slipnodes
				if (pBacktrackNode->IsSlipLane())
				{
					linkDist += (s16)(pNeighbourLink->m_1.m_Distance>>3);
				}

				//add a really high score for the NodeToAvoid
				if (pBacktrackNode->m_address == NodeToAvoid)
				{
					linkDist += 1000;
				}

				// Compute the distance we would expect on the node neighbor node, if we got
				// to the backtrack node from this link.
				const int expectedNeighbourDistanceToTarget = pBacktrackNode->m_distanceToTarget - linkDist;

				// Check if it matches exactly - this is expected to be the case for one of the links,
				// if we got the cost computation above correct.
				if(expectedNeighbourDistanceToTarget == pNeighbourNode->m_distanceToTarget)
				{
					// This one will do. Thank you very much.
					pChosenNeighbourLink = pNeighbourLink;
					break;
				}
				else if(pNeighbourNode->m_distanceToTarget < expectedNeighbourDistanceToTarget)
				{
					// If we hit this case, we have found a neighbor node that's better than expected.
					// It's likely that this is actually the node that we came from, but that we found
					// a better path to that node, and haven't re-explored it. Store it as a backup,
					// in case we don't find a perfect match. Note that we could possibly always use this
					// rather than the exact node (it does seem to be better, after all), but that might
					// increase the risk of possibly finding a node through a link that we aren't actually
					// allowed to use, which would be bad.
					// Also note that pStartNode->m_distanceToTarget that we may have returned to the user
					// wouldn't actually match if we end up using this, so possibly we should subtract
					// the difference - probably won't matter though.
					pChosenNeighbourLink = pNeighbourLink;
					// Note: no break here, we need to continue to look for an exact match.
				}
			}
		}

		if(pChosenNeighbourLink)
		{
			const CPathNodeLink* pNeighbourLink = pChosenNeighbourLink;
			NeighbourNode = pNeighbourLink->m_OtherNode;

			pNeighbourNode = FindNodePointer(NeighbourNode);

			bool bRemovedNode = false;
			pBacktrackNode = pNeighbourNode;


			float fLaneOffset = pNeighbourLink->InitialLaneCenterOffset();
			float fLaneWidth = pNeighbourLink->GetLaneWidth() * (pNeighbourLink->m_1.m_LanesToOtherNode > 0 ? float(pNeighbourLink->m_1.m_LanesToOtherNode - 1) : 0.f);

			if (bReducedList)
			{
				// For a reduced list we only store the nodes that are not on a straight line (this is used by the gps to reduce the number of points to be rendered)
				// Only do this for nodes which have 2 links, and therefore will not require any special directions to be calculated for them

				bool bPreserveNode = bLastNoNav ||
					pBacktrackNode->NumLinks()!=2 ||
					pBacktrackNode->IsJunctionNode() || 
					//pBacktrackNode->m_2.m_slipJunction ||
					pBacktrackNode->m_1.m_indicateKeepLeft ||
					pBacktrackNode->m_1.m_indicateKeepRight ||
					pNeighbourLink->m_1.m_bDontUseForNavigation ||
					pNeighbourLink->m_1.m_bShortCut;

				if ( nNodesGiven >= 2 && !bPreserveNode && !bLastPreserve && nShortcutPreserve == 0 &&
					IsClose(fLaneOffset, fLastOffset) && IsClose(fLaneWidth, fLastWidth))
				{
					// If the last 2 nodes are in line with this new node we don't need to store the one in the middle.
					Vector3 crs1, crs2, crs3, diff1, diff2;
					pNodeCache[0]->GetCoorsXY(crs2);
					pNodeCache[1]->GetCoorsXY(crs1);
					pBacktrackNode->GetCoorsXY(crs3);
					diff1 = crs2 - crs1;
					diff2 = crs3 - crs2;
					diff1.Normalize();
					diff2.Normalize();

					// This is quite inefficient, we want to stay in the vector register 
					if (diff1.Dot(diff2) > fSameDirectionDot)
					{
						nNodesGiven--;
						bRemovedNode = true;
					}
				}

				bLastPreserve = bPreserveNode;
				bLastNoNav = pNeighbourLink->m_1.m_bDontUseForNavigation;	// Really necessary anymore?
			}

			if (pfLaneOffsetList)
			{
				if (pNeighbourLink->m_1.m_bDontUseForNavigation)
				{
					(*(int*)&pfLaneOffsetList[nNodesGiven]) = GNI_IGNORE_FOR_NAV;
				}
				else
				{
					//// Move last node out!
					//if ((nNodesGiven >= NumNodesRequested || pBacktrackNode == pTargetNode) && pNeighbourLink->m_1.m_LanesToOtherNode > 0)
					//	fLaneOffset += pNeighbourLink->GetLaneWidth() * float(pNeighbourLink->m_1.m_LanesToOtherNode - 1);

					pfLaneOffsetList[nNodesGiven] = fLaneOffset;
				}
			}

			if (pfLaneWidthList)
			{
				pfLaneWidthList[nNodesGiven] = fLaneWidth;
			}

			if (pNeighbourLink->m_1.m_bShortCut/* || pBacktrackNode->IsJunctionNode()*/)
				nShortcutPreserve = 2;
			else if (nShortcutPreserve > 0)
				--nShortcutPreserve;

			fLastOffset = fLaneOffset;
			pNodeList[nNodesGiven++] = pBacktrackNode->m_address;
			if (!bRemovedNode)
				pNodeCache[1] = pNodeCache[0];

			pNodeCache[0] = pBacktrackNode;	// -1

#if __BANK
			if (bSpewPathfindQueryProgress)
			{
				aiDisplayf("DoPathSearch: Adding Node %d:%d to path", pBacktrackNode->GetAddrRegion(), pBacktrackNode->GetAddrIndex());
			}
#endif //__BANK
		}
		else
		{
			Assert(0);	// something has gone wrong in the algorithm


			//------------------------------------------------------------------------------------------
			// JB: unsure what could cause this, let's output all the input params to this path search

#if __ASSERT
			Displayf("\n\n----- CPathFind::DoPathSearch() -----\n\n");
			Displayf("StartCoors: (%.4f, %.4f, %.f)", StartCoors.x, StartCoors.y, StartCoors.z);
			Displayf("StartNode: (%i:%i)", StartNode.GetRegion(), StartNode.GetIndex());
			Displayf("TargetCoors: (%.4f, %.4f, %.f)", TargetCoors.x, TargetCoors.y, TargetCoors.z);
			Displayf("pNumNodesGiven: %.2f", pNumNodesGiven ? *pNumNodesGiven : 0.0f);
			Displayf("NumNodesRequested: %.i", NumNodesRequested);			
			Displayf("pDistance: %.2f", pDistance ? *pDistance : 0.0f);
			Displayf("CutoffDistForNodeSearch: %.2f", CutoffDistForNodeSearch);			
			Displayf("pGivenTargetNode: 0x%p (%i:%i)", pGivenTargetNode, pGivenTargetNode ? pGivenTargetNode->GetRegion() : 0, pGivenTargetNode ? pGivenTargetNode->GetIndex() : 0);
			Displayf("MaxSearchDistance: %.i", MaxSearchDistance);
			Displayf("bDontGoAgainstTraffic: %s", bDontGoAgainstTraffic ? "true":"false");
			Displayf("NodeToAvoid: (%i:%i)", NodeToAvoid.GetRegion(), NodeToAvoid.GetIndex());
			Displayf("bAmphibiousVehicle: %s", bAmphibiousVehicle ? "true":"false");
			Displayf("bBoat: %s", bBoat ? "true":"false");
			Displayf("bReducedList: %s", bReducedList ? "true":"false");
			Displayf("bBlockedByNoGps: %s", bBlockedByNoGps ? "true":"false");
			Displayf("bForGps: %s", bForGps ? "true":"false");
			Displayf("bClearDistanceToTarget: %s", bClearDistanceToTarget ? "true":"false");
			Displayf("bMayUseShortCutLinks: %s", bMayUseShortCutLinks ? "true":"false");
			Displayf("bIgnoreSwitchedOffNodes: %s", bIgnoreSwitchedOffNodes ? "true":"false");			
			Displayf("\n\n-------------------------------------\n\n");

			if (pBacktrackNode)
			{
				Displayf("Backtrack Node: %d:%d. (%.2f, %.2f, %.2f)", pBacktrackNode->GetAddrRegion(), pBacktrackNode->GetAddrIndex()
					, pBacktrackNode->GetPos().x, pBacktrackNode->GetPos().y, pBacktrackNode->GetPos().z);

				const u32 nDebugLinks = pBacktrackNode->NumLinks();
				for(u32 iDebugLink = 0; iDebugLink < nDebugLinks; iDebugLink++)
				{
					const CNodeAddress& debugNeighbor = ThePaths.GetNodesLinkedNodeAddr(pBacktrackNode, iDebugLink);
					Displayf("Neighbor %d, Node: %d:%d, Loaded: %s", iDebugLink, debugNeighbor.GetRegion(), debugNeighbor.GetIndex()
						, ThePaths.IsRegionLoaded(debugNeighbor.GetRegion()) ? "yes" : "no");
				}
			}
#endif

			Assertf(false, "Heinous error, please include TTY");	// something has gone wrong in the algorithm

			// Now attempt some kind of recovery
			if(pNumNodesGiven)
				*pNumNodesGiven = 0;

#if __BANK
			perfTimer.Stop();
			m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = (float)perfTimer.GetTimeMS();
#endif
			break;
		}
	}

	*pNumNodesGiven = nNodesGiven;

	if(bClearDistanceToTarget)
	{
		// We're done. Time to clear the nodes now.
		if (NumNodesToBeCleared < PF_MAXNUMNODES_INSEARCH)
		{
			for (Node = 0; Node < NumNodesToBeCleared; Node++)
			{
				FindNodePointer(aNodesToBeCleared[Node])->m_distanceToTarget = PF_VERYLARGENUMBER;
			}
		}
		else
		{
			ClearAllNodesDistanceToTarget(pathRegionsToClearIfNeeded);
		}
	}

#if SERIOUSDEBUG
	ASSERT_ONLY(VerifyAllNodesDistanceToTarget();)
#endif // SERIOUSDEBUG

#if __BANK
	perfTimer.Stop();
	m_PathHistory[m_iLastPathHistory].m_fTotalProcessingTime = (float)perfTimer.GetTimeMS();
	m_PathHistory[m_iLastPathHistory].m_bPathFound = true;
#endif
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ClearAllNodesDistanceToTarget
// PURPOSE :  All nodes currently streamed in have their distance to target reset
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::ClearAllNodesDistanceToTarget(const bool* regionsToClear/* = NULL*/)
{
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(regionsToClear && !regionsToClear[Region])
		{
			continue; // don't need to clear this region
		}

		if(!apRegions[Region] || !apRegions[Region]->aNodes)
		{
			continue;
		}

		s32 nCarNodes = apRegions[Region]->NumNodesCarNodes;
		for (s32 n = 0; n < nCarNodes; n++)
		{
			apRegions[Region]->aNodes[n].m_distanceToTarget = PF_VERYLARGENUMBER;
		}
	}
}

#if __ASSERT
void CPathFind::VerifyAllNodesDistanceToTarget()
{
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if (ThePaths.apRegions[Region] && ThePaths.apRegions[Region]->aNodes)
		{
			for (s32 Nod = 0; Nod < ThePaths.apRegions[Region]->NumNodes; Nod++)
			{
				Assert(ThePaths.apRegions[Region]->aNodes[Nod].m_distanceToTarget == PF_VERYLARGENUMBER);
			}
		}
	}
}
#endif // __ASSERT


CPathNode * CPathFind::FindRouteEndNodeForGpsSearch(const Vector3 & vPos, const Vector3 & vDir, const bool bIgnoreZ, Vector3 * pvClosestPosOnLinks, bool bIgnoreNoGps)
{
	static dev_float fZMultNone = 0.0f;
	static dev_float fZMultNormal = 7.5f;
	static dev_float fPreferBothWayProximitySqrXY = 20.0f * 20.0f;
	static dev_float fPreferBothWayMaxZ = 5.0f;
	static dev_float fSearchDist = STREAMING_PATH_NODES_DIST_ASYNC;
	static dev_float fHeadingPenalty = 15.0f; //100.0f;
	static dev_float fFitnessThreshold = 2.0f;
	static dev_float fTwoLinkBonusFitness = 0.25f;

	Vector3 vDirIn = vDir;
	float fHeadingPenaltyIn = fHeadingPenalty;
	Vector3 vPosToLoc;

	if (NetworkInterface::IsGameInProgress())
	{
		vPosToLoc = vSpecialLoc2 - vPos;
		if (vPosToLoc.XYMag2() < 2500.f) // 50 meters
		{
			vDirIn = Vector3(0.707f, -0.707f, 0.f);
			fHeadingPenaltyIn = 150.f;
		}
	}

	CPathFind::TFindClosestNodeLinkGPS srcClosestData;
	srcClosestData.Reset();
	srcClosestData.vSearchOrigin = vPos;
	srcClosestData.vSearchDir = vDirIn;
	srcClosestData.fDistZPenalty = bIgnoreZ ? fZMultNone : fZMultNormal;

	vPosToLoc = vSpecialLoc1 - vPos;
	if (vPosToLoc.XYMag2() < 2500.f) // 50 meters
		srcClosestData.fDistZPenalty = 20.f;

	float fDistZPenalty = srcClosestData.fDistZPenalty;

	srcClosestData.bIgnoreHeading = vDirIn.Mag2() < SMALL_FLOAT;
	srcClosestData.fHeadingPenalty = fHeadingPenaltyIn;
	srcClosestData.bIgnoreNoGps = /*ThePaths.bIgnoreNoGpsFlag || */bIgnoreNoGps;
	srcClosestData.bIgnoreWater = !ThePaths.bIgnoreNoGpsFlag;
	FindNodeClosestToCoors(vPos, CGps::FindClosestNodeLinkCB, (void*)&srcClosestData, fSearchDist, true);
	CPathNode * pSourceNode = srcClosestData.pBestNodeSoFar;
	if (!pSourceNode)
		return NULL;

	Vector3 vClosestPosOnLinks = srcClosestData.vClosestPointOnNearbyLinks;
		
	if (pSourceNode->NumLinks()==2 && GetNodesLink(pSourceNode, 0).IsOneWay() && GetNodesLink(pSourceNode, 1).IsOneWay())
	{
		srcClosestData.Reset();
		srcClosestData.vSearchOrigin = vPos;
		srcClosestData.vSearchDir = vDirIn;
		srcClosestData.fDistZPenalty = fDistZPenalty;
		srcClosestData.bIgnoreHeading = vDirIn.Mag2() < SMALL_FLOAT;
		srcClosestData.fHeadingPenalty = fHeadingPenaltyIn;
		srcClosestData.bIgnoreOneWay = true;
		srcClosestData.bIgnoreNoGps = /*ThePaths.bIgnoreNoGpsFlag || */bIgnoreNoGps;
		srcClosestData.bIgnoreWater = !ThePaths.bIgnoreNoGpsFlag;
		FindNodeClosestToCoors(vPos, CGps::FindClosestNodeLinkCB, (void*)&srcClosestData, fSearchDist, true);
		CPathNode * pSourceNodeIgnoreOneWay = srcClosestData.pBestNodeSoFar;
		if (pSourceNodeIgnoreOneWay)
		{
			float FitnessFirstNode = CalculateFitnessValueForNode(vPos, vDir, pSourceNode);
			float FitnessSecondNode = CalculateFitnessValueForNode(vPos, vDir, pSourceNodeIgnoreOneWay);
			if (FitnessFirstNode < fFitnessThreshold && FitnessFirstNode > FitnessSecondNode * fTwoLinkBonusFitness)
			{
				Vector3 vSourceNode, vSourceNodeIgnoreOneWay;
				pSourceNode->GetCoors(vSourceNode);
				pSourceNodeIgnoreOneWay->GetCoors(vSourceNodeIgnoreOneWay);

				pSourceNode =
					(vSourceNode - vSourceNodeIgnoreOneWay).XYMag2() < fPreferBothWayProximitySqrXY &&
					IsClose(vSourceNode.z, vSourceNodeIgnoreOneWay.z, fPreferBothWayMaxZ) ?
						pSourceNodeIgnoreOneWay : pSourceNode;
				
				if (pSourceNode == pSourceNodeIgnoreOneWay)
					vClosestPosOnLinks = srcClosestData.vClosestPointOnNearbyLinks;
			}
		}
	}

	if (pSourceNode && pvClosestPosOnLinks)
		*pvClosestPosOnLinks = vClosestPosOnLinks;

	return pSourceNode;
}

// Runs an async search for the path node closest to the given vector
// On the first call, the search will be started
// A subsequent call will retrieve the search results
// For a successful search, you need to call ClearGPSSearchSlot to clean up the search slot
CPathNode * CPathFind::FindRouteEndNodeForGpsSearchAsync(const Vector3 & vPos, const Vector3 & vDir, const bool bIgnoreZ, Vector3 * pvClosestPosOnLinks, u32 searchSlot, bool bIgnoreNoGps)
{
	CPathFind::TFindClosestNodeLinkGPS* srcClosestData = NULL;
	bool startNewSearch = true;
	if(m_GPSAsyncSearches[searchSlot]) // check for a search already in progress
	{
		startNewSearch = false;
		if(m_GPSAsyncSearches[searchSlot]->m_TaskHandle)
		{
			if(!m_GPSAsyncSearches[searchSlot]->m_TaskComplete && !sysTaskManager::Poll(m_GPSAsyncSearches[searchSlot]->m_TaskHandle))
			{
				PF_PUSH_TIMEBAR("Waiting on GPS Async");
				sysTaskManager::Wait(m_GPSAsyncSearches[searchSlot]->m_TaskHandle);
				PF_POP_TIMEBAR();			
			}
			m_GPSAsyncSearches[searchSlot]->m_TaskHandle = NULL;
		}
		srcClosestData = m_GPSAsyncSearches[searchSlot]->m_SearchData;// grab the results from the completed search
	}

	static dev_float fZMultNone = 0.0f;
	static dev_float fZMultNormal = 7.5f;
	static dev_float fZMultHigher = 10.f;
	static dev_float fPreferBothWayProximitySqrXY = 20.0f * 20.0f;
	static dev_float fPreferBothWayMaxZ = 5.0f;
	static dev_float fSearchDist = STREAMING_PATH_NODES_DIST_ASYNC;
	static dev_float fHeadingPenalty = 15.0f; //100.0f;
	static dev_float fFitnessThreshold = 2.0f;
	static dev_float fTwoLinkBonusFitness = 0.25f;

	if(startNewSearch)
	{
		Vector3 vDirIn = vDir;
		float fHeadingPenaltyIn = fHeadingPenalty;
		Vector3 vPosToLoc;

		if (NetworkInterface::IsGameInProgress())
		{
			vPosToLoc = vSpecialLoc2 - vPos;
			if (vPosToLoc.XYMag2() < 2500.f) // 50 meters
			{
				vDirIn = Vector3(0.707f, -0.707f, 0.f);
				fHeadingPenaltyIn = 150.f;
			}
		}

		// Set up the search parameters
		srcClosestData = rage_aligned_new(16) CPathFind::TFindClosestNodeLinkGPS();
		srcClosestData->Reset();
		srcClosestData->vSearchOrigin = vPos;
		srcClosestData->vSearchDir = vDirIn;
		if (searchSlot == GPS_ASYNC_SEARCH_PERIODIC || searchSlot == GPS_ASYNC_SEARCH_START_NODE)
			srcClosestData->fDistZPenalty = bIgnoreZ ? fZMultNone : fZMultHigher;
		else
			srcClosestData->fDistZPenalty = bIgnoreZ ? fZMultNone : fZMultNormal;

		vPosToLoc = vSpecialLoc1 - vPos;
		if (vPosToLoc.XYMag2() < 2500.f) // 50 meters
			srcClosestData->fDistZPenalty = 20.f;

		srcClosestData->bIgnoreHeading = vDirIn.Mag2() < SMALL_FLOAT;
		srcClosestData->fHeadingPenalty = fHeadingPenaltyIn;
		srcClosestData->bIgnoreNoGps = bIgnoreNoGps/* || ThePaths.bIgnoreNoGpsFlag*/;
		srcClosestData->bIgnoreWater = !ThePaths.bIgnoreNoGpsFlag;
		
		m_GPSAsyncSearches[searchSlot] = rage_aligned_new(16) FindNodeClosestToCoorsAsyncParams();
		m_GPSAsyncSearches[searchSlot]->m_SearchCoors = vPos;
		m_GPSAsyncSearches[searchSlot]->m_CallbackFunction = CGps::FindClosestNodeLinkCB;
		m_GPSAsyncSearches[searchSlot]->m_SearchData = srcClosestData;
		m_GPSAsyncSearches[searchSlot]->m_CutoffDistance = fSearchDist;
		m_GPSAsyncSearches[searchSlot]->m_bForGPS = true;
		m_GPSAsyncSearches[searchSlot]->m_CallingGPSSlot = searchSlot;

		m_GPSAsyncSearches[searchSlot]->m_fZMultNone= fZMultNone;
		m_GPSAsyncSearches[searchSlot]->m_fZMultNormal = fZMultNormal;
		m_GPSAsyncSearches[searchSlot]->m_fZMultHigher = fZMultHigher;
		m_GPSAsyncSearches[searchSlot]->m_fPreferBothWayProximitySqrXY = fPreferBothWayProximitySqrXY;
		m_GPSAsyncSearches[searchSlot]->m_fPreferBothWayMaxZ = fPreferBothWayMaxZ;
		m_GPSAsyncSearches[searchSlot]->m_fSearchDist = fSearchDist;
		m_GPSAsyncSearches[searchSlot]->m_fHeadingPenalty = fHeadingPenaltyIn;
		m_GPSAsyncSearches[searchSlot]->m_fFitnessThreshold = fFitnessThreshold;
		m_GPSAsyncSearches[searchSlot]->m_fTwoLinkBonusFitness = fTwoLinkBonusFitness;

		StartAsyncGPSSearch(m_GPSAsyncSearches[searchSlot]);
		return NULL;
	}
	
	CPathNode * pSourceNode = srcClosestData->pBestNodeSoFar;

#if !__FINAL
	// debug output for bogus nodes if we're about to assert
	if(pSourceNode && pSourceNode->GetAddrRegion() >= PATHFINDREGIONS)
	{
		Displayf("GPS Async Output Due to Bogus Source Node %p:", pSourceNode);
		Displayf("Wanted Output Node: %u in Region %u", srcClosestData->resultNodeAddress.GetIndex(), srcClosestData->resultNodeAddress.GetRegion());
		Displayf("Search inputs:");
		Displayf("New Search Coords: X: %.2f Y: %.2f - Region: %u", vPos.x, vPos.y, FindRegionForCoors(vPos.x, vPos.y));
		Displayf("Bogus Search Coords: X: %.2f Y: %.2f - Region: %u", m_GPSAsyncSearches[searchSlot]->m_SearchCoors.GetX(), m_GPSAsyncSearches[searchSlot]->m_SearchCoors.GetY(), FindRegionForCoors(m_GPSAsyncSearches[searchSlot]->m_SearchCoors.GetX(),m_GPSAsyncSearches[searchSlot]->m_SearchCoors.GetY()));
		Displayf("Search Slot: %u", searchSlot);
		Displayf("Pathfind Node State:");
		for(int i = 0; i < PATHFINDREGIONS; i++)
		{
			Displayf("Region %u Nodes: %p - %p", i, apRegions[i] ? apRegions[i]->aNodes : 0 , apRegions[i] ? apRegions[i]->aNodes + apRegions[i]->NumNodes : 0);
		}
		Displayf("End GPS Async Output");
	}
#endif // !__FINAL

	if(pSourceNode && Verifyf(pSourceNode->GetAddrRegion() < PATHFINDREGIONS, "Source Node %p has invalid region %u, please provide logs!", pSourceNode, pSourceNode->GetAddrRegion()))
	{
		Vector3 v;
		pSourceNode->GetCoors(v);

		// this only makes sense if the search was successful
		if(pvClosestPosOnLinks)
		{
			if(!srcClosestData->vClosestPointOnNearbyLinks.IsClose(VEC3_ZERO, .01f))
				*pvClosestPosOnLinks = srcClosestData->vClosestPointOnNearbyLinks;
			else
				pSourceNode->GetCoors(*pvClosestPosOnLinks);
		}
	}
	else
	{
		// Search was unsuccessful, clear the search slot
		ClearGPSSearchSlot(searchSlot);
		pSourceNode = NULL;
	}
	
	return pSourceNode;
}

bool CPathFind::IsAsyncSearchActive(u32 searchSlot, Vector3& vSearchCoors)
{
	if (m_GPSAsyncSearches[searchSlot] && m_GPSAsyncSearches[searchSlot]->m_SearchData)
	{
		vSearchCoors = m_GPSAsyncSearches[searchSlot]->m_SearchCoors;
		return true;
	}

	return false;
}

bool CPathFind::IsAsyncSearchComplete(u32 searchSlot)
{
	if(m_GPSAsyncSearches[searchSlot] && m_GPSAsyncSearches[searchSlot]->m_TaskHandle)
	{
		if(!m_GPSAsyncSearches[searchSlot]->m_TaskComplete && !sysTaskManager::Poll(m_GPSAsyncSearches[searchSlot]->m_TaskHandle))
		{
			return true;
		}
	}

	return false;
}

// Clean up the GPS search slot so that another search can run
void CPathFind::ClearGPSSearchSlot(u32 searchSlot /*=GPS_ASYNC_SEARCH_PERIODIC*/)
{
	if(m_GPSAsyncSearches[searchSlot])
	{
		// make sure the task, if any, has finished
		if(!m_GPSAsyncSearches[searchSlot]->m_TaskComplete && m_GPSAsyncSearches[searchSlot]->m_TaskHandle && !sysTaskManager::Poll(m_GPSAsyncSearches[searchSlot]->m_TaskHandle))
		{
			Errorf("Waiting on GPS Async in ClearGPSSearchSlot");
			sysTaskManager::Wait(m_GPSAsyncSearches[searchSlot]->m_TaskHandle);		
		}
		m_GPSAsyncSearches[searchSlot]->m_TaskHandle = NULL;

		if(m_GPSAsyncSearches[searchSlot]->m_SearchData)
		{
			delete (char*) m_GPSAsyncSearches[searchSlot]->m_SearchData;
			m_GPSAsyncSearches[searchSlot]->m_SearchData = NULL;
		}

		delete m_GPSAsyncSearches[searchSlot];
		m_GPSAsyncSearches[searchSlot] = NULL;
	}
}

bool CPathFind::FindAndFixOddJunction(int iStartNode, int nNodes, Vector3* pNodes)
{
	TUNE_GROUP_FLOAT(GPS_PATHSMOOTHING, OddJunctionEnterAngleThreshold, 66.f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(GPS_PATHSMOOTHING, OddJunctionExitAngleThreshold, 5.f, 0.0f, 180.0f, 0.1f);

	if (nNodes - iStartNode < 4)
		return false;

	bool bDidCorrection = false;
	static const float OddJunctionEnterThreshold = rage::Cosf(OddJunctionEnterAngleThreshold * DtoR);
	static const float OddJunctionExitThreshold = rage::Cosf(OddJunctionExitAngleThreshold * DtoR);

	Vector3 EnterSegForward = pNodes[iStartNode + 1] - pNodes[iStartNode + 2];
	Vector3 ExitSegForward = pNodes[iStartNode + 3] - pNodes[iStartNode + 2];
	Vector3 FirstSegForward = pNodes[iStartNode + 1] - pNodes[iStartNode + 0];
	EnterSegForward.Normalize();
	ExitSegForward.Normalize();
	FirstSegForward.Normalize();

	Vector3 EnterSegBackward = pNodes[iStartNode + 2] - pNodes[iStartNode + 1];
	Vector3 ExitSegBackward = pNodes[iStartNode + 0] - pNodes[iStartNode + 1];
	Vector3 FirstSegBackward = pNodes[iStartNode + 2] - pNodes[iStartNode + 3];
	EnterSegBackward.Normalize();
	ExitSegBackward.Normalize();
	FirstSegBackward.Normalize();

	// Todo: Please stay in the vector register for ze performance
	if (EnterSegForward.Dot(ExitSegForward) > OddJunctionEnterThreshold &&
		FirstSegForward.Dot(ExitSegForward) < OddJunctionExitThreshold)
	{
		// Project point on from where we come from
		Vector3 ToBeFixedVec = pNodes[iStartNode + 2] - pNodes[iStartNode + 0];
		pNodes[iStartNode + 2] = ToBeFixedVec.Dot(FirstSegForward) * FirstSegForward + pNodes[iStartNode + 0];
		bDidCorrection = true;
	}

	if (EnterSegBackward.Dot(ExitSegBackward) > OddJunctionEnterThreshold &&
		FirstSegBackward.Dot(ExitSegBackward) < OddJunctionExitThreshold)
	{
		// Project point on to where we are going to
		Vector3 ToBeFixedVec = pNodes[iStartNode + 1] - pNodes[iStartNode + 3];
		pNodes[iStartNode + 1] = ToBeFixedVec.Dot(FirstSegBackward) * FirstSegBackward + pNodes[iStartNode + 3];
		bDidCorrection = true;
	}


	return bDidCorrection;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : 
// PURPOSE : 
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::FindWiggle(int iStartNode, int nNodes, Vector3* pNodes, int& iWiggleStart, int& iWiggleEnd) const
{
	TUNE_GROUP_FLOAT(GPS_PATHSMOOTHING, WiggleThresholdAngle, 3.f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(GPS_PATHSMOOTHING, FirstDistReq, 20.f, 0.0f, 200.0f, 1.f);
	TUNE_GROUP_FLOAT(GPS_PATHSMOOTHING, WiggleDistReq, 150.f, 0.0f, 200.0f, 1.f);
	TUNE_GROUP_INT(GPS_PATHSMOOTHING, nNodesLookAhead, 5, 0, 100, 1);
	TUNE_GROUP_INT(GPS_PATHSMOOTHING, nNodesToCheckAfterFirstMatch, 3, 0, 100, 1);

	if (nNodes - iStartNode < 4)
		return false;

	iWiggleStart = 0;
	iWiggleEnd = 0;

	const float WiggleThreshold = rage::Cosf(WiggleThresholdAngle * DtoR);
	Vector3 FirstSeg = pNodes[iStartNode + 1] - pNodes[iStartNode];
	FirstSeg.Normalize();

	// Find first node that doesnt match our path direction
	float FirstSegDist = 0.f;
	for (int i = iStartNode + 2; i < nNodes; ++i)
	{
		Vector3 Wiggle = pNodes[i] - pNodes[i-1];
		Wiggle.Normalize();
		if (Wiggle.Dot(FirstSeg) < WiggleThreshold)
		{
			FirstSegDist = pNodes[iStartNode].Dist(pNodes[i-1]);
			iWiggleStart = i - 1;
			break;
		}
	}

	if (iWiggleStart == 0)
		return false;

	float WiggleSegDist = 0.f;
	int iWiggleMaxTest = Min(iWiggleStart + nNodesLookAhead, nNodes);
	for (int i = iWiggleStart + 1; i < iWiggleMaxTest; ++i)
	{
		Vector3 Wiggle = pNodes[i] - pNodes[iWiggleStart];
		Wiggle.Normalize();
		if (Wiggle.Dot(FirstSeg) > WiggleThreshold)
		{
			WiggleSegDist = pNodes[iWiggleStart].Dist(pNodes[i]);
			iWiggleEnd = i;
			break;
		}
	}

	if (FirstSegDist < FirstDistReq || WiggleSegDist > WiggleDistReq)
		return false;

	// After first wiggle match we must try to find a second wiggle match to make sure we are indeed going in that direction
	bool bIsValidWiggle = false;
	iWiggleMaxTest = Min(iWiggleEnd + nNodesToCheckAfterFirstMatch, nNodes);
	for (int i = iWiggleEnd + 1; i < iWiggleMaxTest; ++i)
	{
		Vector3 Wiggle = pNodes[i] - pNodes[iWiggleStart];
		Wiggle.Normalize();
		if (Wiggle.Dot(FirstSeg) > WiggleThreshold)
		{
			bIsValidWiggle = true;
			break;
		}
	}

	if (iWiggleEnd == 0 || !bIsValidWiggle)
		return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : 
// PURPOSE : 
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::SmoothWiggle(Vector3* pNodes, int iWiggleStart, int iWiggleEnd)
{
	Vector3 StraigthDir = pNodes[iWiggleEnd] - pNodes[iWiggleStart];
	StraigthDir.Normalize();

	// Will move all nodes along the projected line between the wiggle start and end
	for (int i = iWiggleStart; i < iWiggleEnd; ++i)
	{
		Vector3 ToBeFixedVec = pNodes[i] - pNodes[iWiggleStart];
		pNodes[i] = ToBeFixedVec.Dot(StraigthDir) * StraigthDir + pNodes[iWiggleStart];
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateRoutePointsForGps
// PURPOSE :  Given a coordinate on the map that a cars computer wants to go to
//			  this function generates a bunch of points to be rendered on the radar
//			  map.
/////////////////////////////////////////////////////////////////////////////////

float CPathFind::GenerateRoutePointsForGps(CNodeAddress destNodeAddress, const Vector3& DestinationCoors, CEntity * pTargetEntity, Vector3 *pResultArray, CNodeAddress* pNodeAdress, u16 *pNodeInfoArray, s16 *pNodeDistanceToTarget, s32 maxPoints, s32 &numPoints, bool bIgnoreDestZ, const u32 iGpsFlags, bool& searchIgnoredOneWay, bool async/* = false*/, bool bReduceMaxSearch, bool bIgnoreNoGps)
{
	const CPed * pPlayer = CGps::GetRelevantPlayer();//CGameWorld::FindLocalPlayer();
	Assertf(pPlayer, "FindLocalPlayer called when no local player!");
	const CVehicle * pVeh = pPlayer->GetVehiclePedInside();
	const Vector3 srcCoors = VEC3V_TO_VECTOR3((pVeh)?pVeh->GetTransform().GetPosition() : (Vec3V_Out)CGps::GetRelevantPlayerCoords());//pPlayer->GetTransform().GetPosition());
	Vec3V vDirFlat = (pVeh)?pVeh->GetTransform().GetB():pPlayer->GetTransform().GetB();
	Vector3	srcDirFlat = RC_VECTOR3(vDirFlat);
	srcDirFlat.z = 0.0f;
	srcDirFlat.NormalizeSafe(VEC3_ZERO, SMALL_FLOAT);

	Vector3	targetDirFlat = VEC3_ZERO;
	if(pTargetEntity && pTargetEntity->GetIsTypeVehicle())
	{
		// Vec3V vTargetDirFlat = ((CVehicle*)pTargetEntity)->GetTransform().GetB();
		targetDirFlat = RC_VECTOR3(vDirFlat);
		targetDirFlat.z = 0.0f;
		targetDirFlat.NormalizeSafe(VEC3_ZERO, SMALL_FLOAT);
	}

	// For targets in interior we might aswell ignore Z since the buildings might be tall and that aint too good
	if(pTargetEntity && pTargetEntity->GetIsInInterior())
	{
		bIgnoreDestZ = true;
	}

	return GenerateRoutePointsForGps(EmptyNodeAddress, destNodeAddress, srcCoors, DestinationCoors, srcDirFlat, targetDirFlat, pResultArray, pNodeAdress, pNodeInfoArray, pNodeDistanceToTarget, maxPoints, numPoints, bIgnoreDestZ, iGpsFlags, searchIgnoredOneWay, async, bReduceMaxSearch, bIgnoreNoGps);
}

float CPathFind::GenerateRoutePointsForGps(const Vector3& srcCoors, const Vector3& destCoors, const Vector3& srcDirFlat, const Vector3 & targetDirFlat, Vector3 *pResultArray, CNodeAddress* pNodeAdress, u16 *pNodeInfoArray, s16 *pNodeDistanceToTarget, s32 maxPoints, s32 &numPoints, bool bIgnoreDestZ, const u32 iGpsFlags, bool& searchIgnoredOneWay, bool async/* = false*/, bool bReduceMaxSearch, bool bIgnoreNoGps)
{
	CPathNode * pSourceNode = FindRouteEndNodeForGpsSearch(srcCoors, srcDirFlat, false);
	if (!pSourceNode)
		return 0.0f;
	CPathNode * pTargetNode = FindRouteEndNodeForGpsSearch(destCoors, targetDirFlat, bIgnoreDestZ, NULL, bIgnoreNoGps || bIgnoreNoGpsFlagUntilFirstNode);
	if (!pTargetNode)
		return 0.0f;

	return GenerateRoutePointsForGps(pSourceNode->m_address, pTargetNode->m_address, srcCoors, destCoors, srcDirFlat, targetDirFlat, pResultArray, pNodeAdress, pNodeInfoArray, pNodeDistanceToTarget, maxPoints, numPoints, bIgnoreDestZ, iGpsFlags, searchIgnoredOneWay, async, bReduceMaxSearch, bIgnoreNoGps);
}

float CPathFind::GenerateRoutePointsForGps(CNodeAddress srcNodeAddress, CNodeAddress destNodeAddress, const Vector3& srcCoors, const Vector3& destCoors, const Vector3& srcDirFlat, const Vector3 & targetDirFlat, Vector3 *pResultArray, CNodeAddress* pNodeAdress, u16 *pNodeInfoArray, s16 *pNodeDistanceToTarget, s32 maxPoints, s32 &numPoints, bool bIgnoreDestZ, const u32 iGpsFlags, bool& searchIgnoredOneWay, bool async/* = false*/, bool bReduceMaxSearch, bool bIgnoreNoGps)
{
	// check first to see if we've invalidated our search by moving too far
	if(async)
	{
		// So we started another search before last one could finish, therefore we must throw this result away as nodes are streamed in for the new search instead
		bool bThrowAwayResult = false;
		if (m_GPSAsyncSearches[GPS_ASYNC_SEARCH_START_NODE] &&
			srcCoors.Dist2(m_GPSAsyncSearches[GPS_ASYNC_SEARCH_START_NODE]->m_SearchCoors) > rage::square(25.0f))
		{
			bThrowAwayResult = true;
#if __ASSERT
			Vector3 vSearchPos = m_GPSAsyncSearches[GPS_ASYNC_SEARCH_START_NODE]->m_SearchCoors;
			Displayf("GPS_ASYNC_SEARCH_START_NODE, Throwing away result for search pos: %.2f %.2f %.2f Actual: %.2f %.2f %.2f", 
				vSearchPos.x, vSearchPos.y, vSearchPos.z, srcCoors.x, srcCoors.y, srcCoors.z);
#endif
		}

		if (m_GPSAsyncSearches[GPS_ASYNC_SEARCH_END_NODE] &&
			destCoors.Dist2(m_GPSAsyncSearches[GPS_ASYNC_SEARCH_END_NODE]->m_SearchCoors) > rage::square(25.0f))
		{
			bThrowAwayResult = true;
#if __ASSERT
			Vector3 vSearchPos = m_GPSAsyncSearches[GPS_ASYNC_SEARCH_END_NODE]->m_SearchCoors;
			Displayf("GPS_ASYNC_SEARCH_END_NODE, Throwing away result for search pos: %.2f %.2f %.2f Actual: %.2f %.2f %.2f", 
				vSearchPos.x, vSearchPos.y, vSearchPos.z, srcCoors.x, srcCoors.y, srcCoors.z);
#endif
		}		
		
		// If we problems here we could see if we are following a moving entity or such and do some
		// logic about that.
		if (bThrowAwayResult)
		{
			// finished the search, clean up the async slots
			ClearGPSSearchSlot(GPS_ASYNC_SEARCH_START_NODE);
			ClearGPSSearchSlot(GPS_ASYNC_SEARCH_END_NODE);

			return -1.f;
		}
	}


	bool readyForPathCalculation = true;
	if (srcNodeAddress.IsEmpty())
	{		
		// Find the source node, using async search if requested
		CPathNode* pSourceNode = async ? FindRouteEndNodeForGpsSearchAsync(srcCoors, srcDirFlat, false, NULL, GPS_ASYNC_SEARCH_START_NODE, bIgnoreNoGps) : 
			FindRouteEndNodeForGpsSearch(srcCoors, srcDirFlat, false, NULL, bIgnoreNoGps);
		
		if(pSourceNode)
			srcNodeAddress = pSourceNode->m_address;
		else
			readyForPathCalculation = false;
	}

	if (destNodeAddress.IsEmpty())
	{
		// Find the destination node, using async search if requested
		CPathNode* pTargetNode = async ? FindRouteEndNodeForGpsSearchAsync(destCoors, targetDirFlat, bIgnoreDestZ, NULL, GPS_ASYNC_SEARCH_END_NODE, bIgnoreNoGps || bIgnoreNoGpsFlagUntilFirstNode) :
			FindRouteEndNodeForGpsSearch(destCoors, targetDirFlat, bIgnoreDestZ, NULL, bIgnoreNoGps || bIgnoreNoGpsFlagUntilFirstNode);		

		if(pTargetNode)
			destNodeAddress = pTargetNode->m_address;
		else
			readyForPathCalculation = false;
	}

	if (!readyForPathCalculation) // Search was just started or failed to find any nodes
		return -1.0f;

	if(async)
	{
		// finished the search, clean up the async slots
		ClearGPSSearchSlot(GPS_ASYNC_SEARCH_START_NODE);
		ClearGPSSearchSlot(GPS_ASYNC_SEARCH_END_NODE);
	}

	CNodeAddress directonListNodes[CGpsSlot::GPS_NUM_NODES_STORED];	
	float fLaneOffsetList[CGpsSlot::GPS_NUM_NODES_STORED];
	float fLaneWidthList[CGpsSlot::GPS_NUM_NODES_STORED];
	s32	numDirectonListNodes;

	PF_PUSH_TIMEBAR("First Path Search");
	float distance;
	DoPathSearch(
		srcCoors,
		srcNodeAddress,
		destCoors,
		directonListNodes,
		&numDirectonListNodes,
		rage::Min(maxPoints, (s32)CGpsSlot::GPS_NUM_NODES_STORED),
		&distance,
		999999.9f,
		&destNodeAddress,
		(bReduceMaxSearch ? 9999 : 999999),
		((iGpsFlags & GPS_FLAG_IGNORE_ONE_WAY)==0),				// bDontGoAgainstTraffic
		EmptyNodeAddress,
		false,				// bAmphibiousVehicle
		false,				// bBoat
		true,				// bReducedList
		!bIgnoreNoGps,		// bBlockedByNoGps
		true,				// bForGps
		true,				// bClearDistanceToTarget
		true,				// bMayUseShortCutLinks
		false,				// bIgnoreSwitchedOffNodes
		((iGpsFlags & GPS_FLAG_FOLLOW_RULES) ? true : false),
		NULL,
		((iGpsFlags & GPS_FLAG_AVOID_HIGHWAY) ? true : false),
		fLaneOffsetList,
		fLaneWidthList,
		false,
		((iGpsFlags & GPS_FLAG_AVOID_OFF_ROAD) ? true : false)
	);	// TODO@JB: pass these parameters in as flags. really

	PF_POP_TIMEBAR();
	Assertf(numDirectonListNodes < maxPoints, "GPS route reached maximum buffer size : %i\nSome of your GPS route will be missing", maxPoints);
	Assertf(numDirectonListNodes < CGpsSlot::GPS_NUM_NODES_STORED, "GPS route reached maximum buffer size : %i\nSome of your GPS route will be missing", CGpsSlot::GPS_NUM_NODES_STORED);
	Assert(numDirectonListNodes <= CGpsSlot::GPS_NUM_NODES_STORED && numDirectonListNodes <= maxPoints);

	// Check if found a dir_out.
	if (numDirectonListNodes == 0)
	{
		// we're changing the search params, return this info
		searchIgnoredOneWay = true;

		PF_PUSH_TIMEBAR("Second Path Search");
		// Do another search, but be slightly more relaxed about stuff...
		// Don't try to use the source node (just use the position) and
		// allow against one way...
		DoPathSearch(
			srcCoors,
			srcNodeAddress,
			destCoors,
			directonListNodes,
			&numDirectonListNodes,
			rage::Min(maxPoints, (s32)CGpsSlot::GPS_NUM_NODES_STORED),
			&distance,
			999999.9f,
			&destNodeAddress,
			(bReduceMaxSearch ? 9999 : 999999),
			false,					// bDontGoAgainstTraffic
			EmptyNodeAddress,
			false,					// bAmphibiousVehicle
			false,					// bBoat
			true,					// bReducedList
			!bIgnoreNoGps,			// bBlockedByNoGps
			true,					// bForGps
			true,					// bClearDistanceToTarget
			true,					// bMayUseShortCutLinks
			false,					// bIgnoreSwitchedOffNodesd
			false,	//((iGpsFlags & GPS_FLAG_FOLLOW_RULES) ? true : false),
			NULL,
			((iGpsFlags & GPS_FLAG_AVOID_HIGHWAY) ? true : false),
			fLaneOffsetList,
			fLaneWidthList
		);
		Assertf(numDirectonListNodes < maxPoints, "GPS route reached maximum buffer size : %i \nSome of your GPS route will be missing", maxPoints);
		Assertf(numDirectonListNodes < CGpsSlot::GPS_NUM_NODES_STORED, "GPS route reached maximum buffer size : %i \nSome of your GPS route will be missing", CGpsSlot::GPS_NUM_NODES_STORED);
		Assert(numDirectonListNodes <= CGpsSlot::GPS_NUM_NODES_STORED && numDirectonListNodes <= maxPoints);
		PF_POP_TIMEBAR();
	}

	if (numDirectonListNodes > 1)
	{
		fLaneOffsetList[0] = fLaneOffsetList[1];
		fLaneWidthList[0] = fLaneWidthList[1];
	}
	else
	{
		fLaneOffsetList[0] = 0.f;
		fLaneWidthList[0] = 0.f;
	}

	int nPoints = rage::Min(maxPoints, numDirectonListNodes);

	if (!(iGpsFlags & GPS_FLAG_NO_PULL_PATH_TO_RIGHT_LANE))
	{
		float fMaxWidth = (nPoints > 0 ? fLaneWidthList[nPoints - 1] : 0.f);
		for (s32 p = nPoints - 1; p > 0; --p)
		{	
			if ((*(int*)&fLaneOffsetList[p]) == GNI_IGNORE_FOR_NAV || (*(int*)&fLaneOffsetList[p]) == GNI_PLAYER_TRAIL_POS)
				continue;

			const CPathNode* pNode = FindNodePointer(directonListNodes[p]);
			if (pNode->m_2.m_highwayOrLowBridge)
				break;

			fLaneOffsetList[p] += fMaxWidth;
			fMaxWidth = Min(fLaneWidthList[p-1], fLaneWidthList[p]);

			if (fMaxWidth <= 0.f)
				break;
		}
	}

	static fwGeom::fwLine HackLine(Vector3(-233.f, -1430.f, 31.f), Vector3(355.f, -1925.5f, 24.5f));

	Vector3 vPrevNode;
	for (s32 p = 0; p < nPoints; p++)
	{
		Vector3	crs;
		const CPathNode* pNode = FindNodePointer(directonListNodes[p]);
		pNode->GetCoors(crs);

		Vector3 vRight;
		float fLaneOffsetToUse = fLaneOffsetList[p];

		if (p < nPoints - 1)
		{
			// Combine this and next link
			Vector3	vNext;
			const CPathNode* pNode = FindNodePointer(directonListNodes[p+1]);
			pNode->GetCoors(vNext);

			if (p == 0) // Fix offset for first node
			{
				vRight = vNext - crs;
				vRight = Vector3(-vRight.y, vRight.x, 0.f);
				fLaneOffsetToUse = fLaneOffsetList[1];
				vPrevNode = crs;
			}
			else
			{
				vNext = vNext - crs;
				vRight = crs - vPrevNode;
				vRight = Vector3(-vRight.y, vRight.x, 0.f) + Vector3(-vNext.y, vNext.x, 0.f);
			}
		}
		else
		{
			vRight = crs - vPrevNode;
			vRight = Vector3(-vRight.y, vRight.x, 0.f);
		}

		vRight.Normalize();
		if ((*(int*)&fLaneOffsetToUse) != GNI_IGNORE_FOR_NAV)
		{
			vPrevNode = crs;

			if (pNode->m_2.m_highwayOrLowBridge)
				fLaneOffsetToUse += fLaneWidthList[p] * 0.5f;

			// Hack for B* 1548360
			if (p > 0)
			{
				Vector3 vClosest;
				HackLine.FindClosestPointOnLine(crs, vClosest);

				Vector3 vToLine = vClosest - crs;
				if (vToLine.Mag2() < 256.f)
					fLaneOffsetToUse += fLaneWidthList[p];
			}

			crs -= vRight * fLaneOffsetToUse;	// Offset for smoothness
			pResultArray[p] = crs;
			pResultArray[p].w = fLaneWidthList[p];
		}
		else
		{
			pResultArray[p] = crs;
			pResultArray[p].w = fLaneOffsetToUse;
		}

		pNodeAdress[p] = directonListNodes[p];
		pNodeInfoArray[p] = (u8)pNode->FindNumberNonSpecialNeighbours();
		pNodeDistanceToTarget[p] = 0;
		Assert(pNodeInfoArray[p] <= NODEINFO_LINKS_MASK);
		if (pNode->m_2.m_waterNode && pNode->m_2.m_highwayOrLowBridge)
		{
			pNodeInfoArray[p] |= NODEINFO_HIGHWAY_BIT;
		}
		if(pNode->IsJunctionNode())
		{
			pNodeInfoArray[p] |= NODEINFO_JUNCTION_BIT;
		}
	}

	// Fix all those quirky junctions
	for (int i = 0; i < nPoints; ++i)
	{
		FindAndFixOddJunction(i, nPoints, pResultArray);
	}

	// More accurate distance calculation, the problem is elsewhere but this is where we solve it
	// Possibly related to the reduced node list and that the new distance with that regard is getting bit screwed
	if (nPoints > 1)
	{
		pNodeDistanceToTarget[nPoints - 1] = 0;
		for (int i = nPoints - 2; i--;)
		{
			pNodeDistanceToTarget[i] = (s16)pResultArray[i].Dist(pResultArray[i+1]) + pNodeDistanceToTarget[i+1];
		}
	}

	// This is to make sure the GPS path does not overshoot the blip or whatever we are going to
	// Distance is not 100% after this but this error should be pretty small and it is using integer anyway
	if (nPoints >= 2)
	{
		const fwGeom::fwLine line(pResultArray[nPoints-1], pResultArray[nPoints-2]);
		Vector3 vClosestPointOnLine;
		line.FindClosestPointOnLine(destCoors, vClosestPointOnLine);

		distance -= vClosestPointOnLine.Dist(pResultArray[nPoints-1]);	// the distance we return might aswell be accurate
		pResultArray[nPoints-1] = vClosestPointOnLine;

		// Throw this node away in this case or minimap will look very odd
		if (pResultArray[nPoints-2].IsClose(vClosestPointOnLine, 0.05f))
			--nPoints;
	}

	numPoints = nPoints;

	return Max(distance, 0.f);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateRoutePointsForGpsRaceTrack
// PURPOSE :  Given a coordinate on the map that a cars computer wants to go to
//			  this function generates a bunch of points to be rendered on the radar
//			  map.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::GenerateRoutePointsForGpsRaceTrack(Vector3 *pResultArray, u16 *pNodeInfoArray, s16 *pNodeDistanceToTarget, s32 maxPoints, s32 &numPoints, const u32 UNUSED_PARAM(iGpsFlags))
{
	CNodeAddress directonListNodes[CGpsSlot::GPS_NUM_NODES_STORED];	
	s32	numDirectonListNodes;

	Assert(CGps::m_iNumCustomAddedPoints >= 2);
	numPoints = 0;


	// We find the target node first as this is done differently when we're doing the frontend map blip.
	// We don't have any z information for this one and should not take the z into account when picking
	// the node.
	CNodeAddress targetNode;

	CNodeAddress startNode = FindNodeClosestToCoors(CGps::m_aCustomAddedPoints[0], 999999.9f, IncludeSwitchedOffNodes, false, false, false, false, false, 0.0f, 0.0f, 3.0f);
	Assert(!startNode.IsEmpty());

	for (s32 i = 0; i < CGps::m_iNumCustomAddedPoints; i++)
	{
		targetNode = FindNodeClosestToCoors(CGps::m_aCustomAddedPoints[(i+1)%CGps::m_iNumCustomAddedPoints], 999999.9f, IncludeSwitchedOffNodes, false, false, false, false, false, 0.0f, 0.0f, 3.0f);

		if (targetNode != startNode)		// Some level designers specify start node twice (at start and end). This code deals with that.
		{
			DoPathSearch(
				CGps::m_aCustomAddedPoints[i],
				startNode,
				CGps::m_aCustomAddedPoints[(i+1)%CGps::m_iNumCustomAddedPoints],
				directonListNodes,
				&numDirectonListNodes,
				CGpsSlot::GPS_NUM_NODES_STORED,
				NULL,
				999999.9f,
				&targetNode,
				999999,
				false,				// bDontGoAgainstTraffic
				EmptyNodeAddress,
				false,				// bAmphibiousVehicle
				false,				// bBoat
				true,				// bReducedList
				false,				// bBlockedByNoGps
				false,				// bForGps
				false,				// bClearDistanceToTarget
				true				// bMayUseShortCutLinks
				);

			// The points the pathsearch returned are now stored in the array that the caller has provided.
			for (s32 p = (i==0)?0:1; p < numDirectonListNodes; p++)
			{
				if (numPoints < maxPoints)
				{
					Vector3	crs;
					const CPathNode	*pNode = FindNodePointer(directonListNodes[p]);
					pNode->GetCoors(crs);
					pResultArray[numPoints].x = crs.x;
					pResultArray[numPoints].y = crs.y;
					pResultArray[numPoints].z = crs.z;
					//pNodeDistanceToTarget[numPoints] = 0;
					pNodeDistanceToTarget[numPoints] = pNode->m_distanceToTarget;
					pNodeInfoArray[numPoints] = (u8)pNode->FindNumberNonSpecialNeighbours();
					Assert(pNodeInfoArray[numPoints] <= NODEINFO_LINKS_MASK);
					if (pNode->m_2.m_waterNode && pNode->m_2.m_highwayOrLowBridge)
					{
						pNodeInfoArray[numPoints] |= NODEINFO_HIGHWAY_BIT;
					}
					numPoints++;
				}
			}
		}
		startNode = targetNode;
		ClearAllNodesDistanceToTarget();
	}

	//// Found out that using the node distanceToTarget in our case is not quite accurate so
	//// we just go through all nodes and check the closest distance between them and then
	//// accumulate that in the list
	//for (int i = numPoints - 1; i > 0; --i)
	//{
	//	pNodeDistanceToTarget[i - 1] = static_cast<s16>(pResultArray[i - 1].Dist(pResultArray[i]));
	//	pNodeDistanceToTarget[i - 1] += pNodeDistanceToTarget[i];
	//}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateDirectionsLegacy
// PURPOSE :  Given the players position 
/////////////////////////////////////////////////////////////////////////////////

static const s32 MAX_NUM_NODES_FOR_DIRECTION_LIST = 16;

void CPathFind::GenerateDirectionsLegacy(const Vector3& DestinationCoors, s32 &dir_out, u32 &streetNameHash_out)
{
	// Init the output params.
	dir_out = 0;// Nothing.
	streetNameHash_out = 0;
#if __DEV
	s32 bearing = 0;
#endif // __DEV

	// Find the 2 nodes that the player is closest to; taking into account the players orientation.
	const Vector3 playerVehicleDir = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetB());
	Assert((playerVehicleDir.Mag2() > 0.9f) && (playerVehicleDir.Mag2() < 1.1f));
	CNodeAddress nodeFrom, nodeTo;
	s16 regionLinkIndex;
	s8 lane;
	FindNodePairToMatchVehicle(CGameWorld::FindLocalPlayerCoors(), playerVehicleDir, nodeFrom, nodeTo, false, regionLinkIndex, lane);

	// Make sure that the nodes aren't emptyAddr- if they are we're
	// too far away from roads to properly generate directions.
	if(!nodeFrom.IsEmpty() && !nodeTo.IsEmpty())
	{
		// Do a path search (so we can generate step by step directions from it).
		CNodeAddress directonListNodes[MAX_NUM_NODES_FOR_DIRECTION_LIST];	
		s32 numDirectionListNodes = 0;
		DoPathSearch(	CGameWorld::FindLocalPlayerCoors(), nodeTo, DestinationCoors,
			directonListNodes, &numDirectionListNodes, MAX_NUM_NODES_FOR_DIRECTION_LIST,
			NULL, 100.f, NULL, 10000,
			true);//, nodeFrom);// Commented out so it generates a more direct path...

#if __DEV
		// Draw the current path above the ground so we can see it.
		if (bDisplayPathsDebug_Directions)
		{
			for (s32 i = 0; i < numDirectionListNodes-1; i++)
			{
				const CNodeAddress node1 = directonListNodes[i];
				const CNodeAddress node2 = directonListNodes[i+1];

				Assert(!node1.IsEmpty());
				Assert(!node2.IsEmpty());

				const CPathNode* pNode1 = FindNodePointer(node1);
				const CPathNode* pNode2 = FindNodePointer(node2);

				Vector3 coors1, coors2;
				pNode1->GetCoors(coors1);
				pNode2->GetCoors(coors2);

				grcDebugDraw::Line(coors1 + Vector3(0.0f,0.0f,2.0f), coors2 + Vector3(0.0f,0.0f,2.0f), rage::Color32(0xff, 0, 0, 0xff));
			}
		}
#endif // __DEV

		//		// If the player is facing a dead end tell them to turn around.
		//      CPathNode* pNodeFrom = ThePaths.FindNodePointer(nodeFrom);
		//		s32 linkFromTo = FindNodesLinkIndexBetween2Nodes(nodeFrom, nodeTo);
		//		CPathNodeLink *pLinkFromTo = &ThePaths.GetNodesLink(pNodeFrom, linkFromTo);
		//		if (pLinkFromTo->m_1.m_LanesToOtherNode == 0)
		//		{
		//			// Dead end!
		//			dir_out = 5;
		//			return;
		//		}

		// Go over each of the nodes on the temp list (up to the first 9) and
		// check if it is a junction.  when we find the first junction return
		// back what we should do.
		for (s32 i = 0; i < rage::Min(9, numDirectionListNodes) - 3; i++)
		{
			// Consider a group of 4 nodes. Use them to work out whether we go left, right or straight.
			const CNodeAddress node0 = directonListNodes[i];
			const CNodeAddress node1 = directonListNodes[i+1];
			const CNodeAddress node2 = directonListNodes[i+2];
#if __DEV
			const CNodeAddress node3 = directonListNodes[i+3];
#endif // __DEV
			Assert(!node0.IsEmpty());
			Assert(!node1.IsEmpty());
			Assert(!node2.IsEmpty());
#if __DEV
			Assert(!node3.IsEmpty());
#endif // __DEV
			const CPathNode* pNode0 = FindNodePointer(node0);
			const CPathNode* pNode1 = FindNodePointer(node1);
			const CPathNode* pNode2 = FindNodePointer(node2);
#if __DEV
			const CPathNode* pNode3 = FindNodePointer(node3);
#endif // __DEV

			// Get the position and direction along each of the nodes.
			Vector3 coors0, coors1, coors2;
#if __DEV
			Vector3 coors3;
#endif // __DEV
			pNode0->GetCoors(coors0);
			pNode1->GetCoors(coors1);
			pNode2->GetCoors(coors2);
#if __DEV
			pNode3->GetCoors(coors3);
#endif // __DEV
			Vector3 dir0to1 = coors1 - coors0;
			dir0to1.Normalize();
			Vector3 dir1to2 = coors2 - coors1;
			dir1to2.Normalize();
#if __DEV
			Vector3 dir2to3 = coors3 - coors2;
			dir2to3.Normalize();
#endif // __DEV

			// If this is the first node, check to see if we are even facing the right direction...
			// And determine the current path bearing...
			if(i == 0)
			{
				// If our dir isn't aligned with the first links dir then we're facing
				// the wrong way.
				if(dir0to1.Dot(playerVehicleDir) < 0.0f)
				{
					// Wrong way!
#if __DEV
					bearing = 4;
#endif // __DEV
					dir_out = 4;
					break;
				}

#if __DEV
				// Determine how the path is turning.
				// The 3nd crossZ is to catch cases where the road bends back straight away.
				const float a = playerVehicleDir.CrossZ(dir0to1);
				const float b = dir0to1.CrossZ(dir1to2);
				const float c = dir1to2.CrossZ(dir2to3);
				const float pathTurnValue = a + b + c;
				TUNE_GROUP_FLOAT(DRIVE_DIRECTIONS, bearDirThreshold, 0.5f, 0.0f, 2.0f, 0.1f);
				if (pathTurnValue > bearDirThreshold)
				{
					bearing = 1;
				}
				else if (pathTurnValue < -bearDirThreshold)
				{
					bearing = 2;
				}
				else
				{
					bearing = 3;
				}
#endif // __DEV
			}

			// Check if this is a junction.
			if ( CountNonSpecialNeighbours(pNode1) >= 3)
			{
				// It is a junction, so check which way through it we want to go.

				// Compare the direction going into the junction to the direction going out of the junction.
				const float junctionTurnValue = dir0to1.CrossZ(dir1to2);
				TUNE_GROUP_FLOAT(DRIVE_DIRECTIONS, pathDirThreshold, 0.5f, 0.0f, 1.0f, 0.01f);
				if (junctionTurnValue > pathDirThreshold)
				{
					// Turn left.
					streetNameHash_out = pNode2->m_streetNameHash;
					dir_out = 1;
					break;
				}
				else if (junctionTurnValue < -pathDirThreshold)
				{
					// Turn right.
					streetNameHash_out = pNode2->m_streetNameHash;
					dir_out = 2;
					break;
				}
				else
				{
					// Straight on.
					streetNameHash_out = pNode2->m_streetNameHash;
					dir_out = 3;
					break;
				}
			}// END if it's a proper junction.
		}// END for each node on directonListNodes (up to first 9)
	}

#if __DEV
	if(bDisplayPathsDebug_Directions)
	{
		if (dir_out == 1)		{ Displayf("GenerateDirections:1 (unkown/keep going)\n"); }
		else if (dir_out == 1)	{ Displayf("GenerateDirections:1 (left)\n"); }
		else if (dir_out == 2)	{ Displayf("GenerateDirections:2 (right)\n"); }
		else if (dir_out == 3)	{ Displayf("GenerateDirections:3 (straight)\n"); }
		else if (dir_out == 4)	{ Displayf("GenerateDirections:4 (wrong way)\n"); }

		// Display the junction dir on screen.
		static s32 dirOld = 0;
		Color32 color(0.5f, 0.3f, 0.5f, 1.0f);
		if(dir_out != dirOld)
		{
			color.Setf(1.0f, 0.0f, 0.0f, 1.0f);
		}
		static const char* dirStrToUse[5] = {"keep driving", "left at junction", "right at junction", "straight through junction", "Wrong way"};
		grcDebugDraw::Text(Vector2(0.5f, 0.5f), color, dirStrToUse[dir_out]);
		dirOld = dir_out;

		// Display the path bearing on screen.
		static const char* bearingStrToUse[5] = {"bear unknown", "bear left", "bear right", "bear straight", "bearing opposite"};
		grcDebugDraw::Text(Vector2(0.5f, 0.55f), color, bearingStrToUse[bearing]);
	}
#endif //__DEV
}

//-----------------------------------------------------------------------------------------------------------
// NAME : IsJunctionForGenerateDirections
// PURPOSE : Helper function which identifies if pNode can be considered as a node for generating directions

bool CPathFind::IsJunctionForGenerateDirections(
	const CRoutePoint * pPrevPrev, const CRoutePoint * pPrev, const CRoutePoint * pNode, const CRoutePoint * pNext, const CRoutePoint * pNextNext,
	float & fOut_DotTurn, float & fOut_DotSmoothTurn, int & iOut_TurnDir, bool & bIsGenuineJunction,
	Vector3 & vEntryPos, Vector3 & vEntryVec, Vector3 & vExitPos, Vector3 & vExitVec, bool bIgnoreFirstLink, bool bIgnoreSecondLink)
{
	Assert(pNode && pPrev && pNext && pNextNext);

	Vector3 vNode = VEC3V_TO_VECTOR3(pNode->GetPosition() + pNode->GetLaneCenterOffset());
	Vector3 vPrev, vNext, vNodeFromPrev, vNodeToNext, vPrevToNext;

	vPrev = VEC3V_TO_VECTOR3(pPrev->GetPosition() + pPrev->GetLaneCenterOffset());
	vNext = VEC3V_TO_VECTOR3(pNext->GetPosition() + pNext->GetLaneCenterOffset());
	vNodeFromPrev = vNode - vPrev;
	vNodeFromPrev.z = 0.0f;
	vNodeFromPrev.Normalize();
	vNodeToNext = vNext - vNode;
	vNodeToNext.z = 0.0f;
	vNodeToNext.Normalize();

	// Sorry about this
	Vector3 vPrevPrevToPrev, vNextToNextNext;
	Vector3 vPrevPrev, vNextNext;

	vPrevPrev = VEC3V_TO_VECTOR3(pPrevPrev->GetPosition() + pPrevPrev->GetLaneCenterOffset());
	vNextNext = VEC3V_TO_VECTOR3(pNextNext->GetPosition() + pNextNext->GetLaneCenterOffset());
	vPrevPrevToPrev = vPrev - vPrevPrev;
	vNextToNextNext = vNextNext - vNext;
	vPrevPrevToPrev.z = 0.0f;
	vNextToNextNext.z = 0.0f;
	vPrevPrevToPrev.Normalize();
	vNextToNextNext.Normalize();

	// pass these out
	vEntryPos = vPrev;
	vExitPos = vNode;

	// Find which link to use as entry node to this junction (disregard any marked as ignore for nav)
	vEntryVec = vNodeFromPrev;
	if(bIgnoreFirstLink) // pPrev->m_bIgnoreForNavigation ||
	{
		if(pPrevPrev->m_bIgnoreForNavigation)
			return false;

		vEntryVec = vPrevPrevToPrev;
		vEntryPos = vPrevPrev;
	}

	// Find which link to use as exit node from this junction (disregard any marked as ignore for nav)
	vExitVec = vNodeToNext;
	if(bIgnoreSecondLink) // pNext->m_bIgnoreForNavigation ||
	{
		if(pNextNext->m_bIgnoreForNavigation)
			return false;

		vExitVec = vNextToNextNext;
		vExitPos = vNext;
	}

	// handle outputs
	iOut_TurnDir = (int) Sign( -CrossProduct(vEntryVec, vExitVec).z );
	fOut_DotTurn = DotProduct(vEntryVec, vExitVec);
	fOut_DotSmoothTurn = DotProduct(vEntryVec, vNextToNextNext);

	bIsGenuineJunction = pNode->m_bJunctionNode;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateDirections
// PURPOSE :  Given the players position 
/////////////////////////////////////////////////////////////////////////////////

u32 CPathFind::GenerateDirections(const Vector3& vDestinationCoors, s32 &iOut_Directions, u32 &iOut_StreetNameHash, float & fOut_DistanceToTurn, Vector3& vOutPos)
{
	DEV_ONLY( Vector3 vDirectionsNode; )

	// Init the output params.
	iOut_Directions = DIRECTIONS_KEEP_DRIVING;
	iOut_StreetNameHash = 0;
	fOut_DistanceToTurn = 0.0f;

	CVehicle * pVehicle = FindPlayerVehicle();
	if(!pVehicle)
		return 0;

	Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	Vec3V vDirFlat = pVehicle->GetTransform().GetB();
	Vector3	playerVehicleDir = RC_VECTOR3(vDirFlat);
	playerVehicleDir.z = 0.0f;
	playerVehicleDir.Normalize();
	
	Assert((playerVehicleDir.Mag2() > 0.9f) && (playerVehicleDir.Mag2() < 1.1f));

	CPathNode * pSourceNode = FindRouteEndNodeForGpsSearch(vVehiclePos, playerVehicleDir, false);

	//int iLane = 0; // TODO: make this meaningful

	// Make sure that the nodes aren't emptyAddr- if they are we're
	// too far away from roads to properly generate directions.
	if(pSourceNode)
	{
		// Do a path search (so we can generate step by step directions from it).
		CNodeAddress directionListNodes[MAX_NUM_NODES_FOR_DIRECTION_LIST];	
		s32 numDirectionListNodes = 0;

		DoPathSearch(
			CGameWorld::FindLocalPlayerCoors(),
			pSourceNode->m_address,
			vDestinationCoors,
			directionListNodes,
			&numDirectionListNodes,
			MAX_NUM_NODES_FOR_DIRECTION_LIST,
			NULL,
			100.0f,
			NULL,
			9999,
			true);

		GenerateDirections(iOut_Directions, iOut_StreetNameHash, fOut_DistanceToTurn, directionListNodes, numDirectionListNodes, vOutPos);
	}

	return 0;
}

u32 CPathFind::GenerateDirections(s32 &iOut_Directions, u32 &iOut_StreetNameHash, float & fOut_DistanceToTurn, CNodeAddress* pNodeAdress, int NumNodes, Vector3& vOutPos)
{
	DEV_ONLY( Vector3 vDirectionsNode; )

	// Init the output params.
	iOut_Directions = DIRECTIONS_KEEP_DRIVING;
	iOut_StreetNameHash = 0;
	fOut_DistanceToTurn = 0.0f;
	vOutPos = VEC3_ZERO;

	CVehicle * pVehicle = FindPlayerVehicle();
	if(!pVehicle)
		return 0;

	Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	Vec3V vDirFlat = pVehicle->GetTransform().GetB();
	Vector3	playerVehicleDir = RC_VECTOR3(vDirFlat);
	playerVehicleDir.z = 0.0f;
	playerVehicleDir.Normalize();
	
	Assert((playerVehicleDir.Mag2() > 0.9f) && (playerVehicleDir.Mag2() < 1.1f));

	int iLane = 0; // TODO: make this meaningful
	int n;

	// Do a path search (so we can generate step by step directions from it).
	CNodeAddress* directionListNodes = pNodeAdress;	

	CVehicleNodeList nodeList;
	CVehicleFollowRouteHelper routeHelper;
	const CRoutePoint * routePoints;

	nodeList.Clear();
	nodeList.SetTargetNodeIndex(1);

	int iNodeCount = Min(NumNodes, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED);
	if(iNodeCount <= 0)
		return 0;

	for(n=0; n<iNodeCount; n++)
	{
		nodeList.SetPathNodeAddr(n, directionListNodes[n]);
		// GPS instructions represent the most significant nodes -- links between these nodes do not exist
		nodeList.SetPathLinkIndex(n, 0);
	}

	nodeList.SetPathLaneIndex(0, (s8)iLane);
	nodeList.FindLanesWithNodes(0, &vVehiclePos, false);
	

	//CVehicleFollowRouteHelper routeHelper;
	Vec3V vOutTarget;
	float fOutSpeed = 10.0f;	// MAGIC! This was uninitialized before but was actually read by FindTargetCoorsAndSpeed(), so we now set it to some reasonable value - probably doesn't matter much what it is.
	static dev_float fDistAhead = 5.0f;
	static dev_float fDistProgress = 10.0f;
	routeHelper.ConstructFromNodeList(pVehicle, nodeList);
	routeHelper.FindTargetCoorsAndSpeed(pVehicle, fDistAhead, fDistProgress, VECTOR3_TO_VEC3V(vVehiclePos), false, vOutTarget, fOutSpeed, false, false);

	//Assert(routeHelper.GetNumPoints()==iNodeCount);	// This should be ok
	iNodeCount = routeHelper.GetNumPoints();
	routePoints = routeHelper.GetRoutePoints();

#if __DEV

	// Draw the current path above the ground so we can see it.
	if (bDisplayPathsDebug_Directions)
	{
		Vector3 vDbgPts[MAX_NUM_NODES_FOR_DIRECTION_LIST];

		for (n = 0; n < iNodeCount; n++)
		{
			vDbgPts[n] = VEC3V_TO_VECTOR3(routePoints[n].GetPosition() + routePoints[n].GetLaneCenterOffset());
		}

		for (n = 0; n < iNodeCount-1; n++)
		{
			grcDebugDraw::Line(vDbgPts[n] + Vector3(0.0f,0.0f,2.0f), vDbgPts[n+1] + Vector3(0.0f,0.0f,2.0f), rage::Color32(0xff, 0, 0, 0xff));
			//static char tmp[64];
			//sprintf(tmp, "lane: %i", routePoints[n].m_iRequiredLaneIndex);
			//grcDebugDraw::Text(vDbgPts[n] + Vector3(0.0f,0.0f,2.0f), Color_magenta, tmp);
		}
	}
#endif // __DEV

	//-------------------------------------------------------------------------------------------------
	// If the first two nodes are pointing opposite to our direction, then we are going the wrong way

	int iFirstSeg = 0;
	while( iFirstSeg < iNodeCount && routePoints[iFirstSeg].m_bIgnoreForNavigation )
		iFirstSeg++;

	if(iFirstSeg >= iNodeCount-1)
		return 0;

	Vector3 vFirstSeg = VEC3V_TO_VECTOR3(
		(routePoints[iFirstSeg+1].GetPosition() + routePoints[iFirstSeg+1].GetLaneCenterOffset()) -
			(routePoints[iFirstSeg].GetPosition() + routePoints[iFirstSeg].GetLaneCenterOffset()) );

	vFirstSeg.z = 0.0f;
	if(vFirstSeg.Mag2() > SMALL_FLOAT)
	{
		vFirstSeg.Normalize();
		const float fDotVehicle = DotProduct(vFirstSeg, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward()));
		if(fDotVehicle < -0.707f)
		{
			iOut_Directions = DIRECTIONS_WRONG_WAY;
		}
	}
	if(iOut_Directions != DIRECTIONS_WRONG_WAY)
	{
		// Go over each of the nodes on the temp list (up to the first 9) and
		// check if it is a junction.  when we find the first junction return
		// back what we should do.

		const CRoutePoint * nodes[5];
		const s32 iLookAheadNum = 15;
		CNodeAddress straightThroughJunctionNode;
		straightThroughJunctionNode.SetEmpty();
		const s32 nNodesToCut = 5;
		const s32 nNodesToCheck = rage::Min(iLookAheadNum, iNodeCount) - nNodesToCut;
		bool bFoundCacheNode = false;

		TUNE_GROUP_FLOAT(DRIVE_DIRECTIONS, pathDirDotThreshold, 0.707f, -1.0f, 1.0f, 0.01f);
		
		if (nNodesToCheck > 0)
			bFoundCacheNode = directionListNodes[1] == m_LastValidDirectionsNode;


		for (s32 i = 0; i < nNodesToCheck; i++)
		{
			// Consider a group of 5 nodes.
			// If nodes[2] is a junction then we examine [0]->[1] and [3]->[4] to see what direction to take
			// Otherwise examine [0]->[1] and [1]->[2] to see which heading to drive in

			for(n=0; n<nNodesToCut; n++)
			{
				nodes[n] = &routePoints[i+n];
				if(!nodes[n])
					break;
			}

			if(n != nNodesToCut)
				break;

			//------------------------------------
			// Check if pNodes[2] is a junction?

			float fJunctionTurnDot = 1.0f;
			float fJunctionSmoothDot = 1.0f;
			int iJunctionTurnDir = 0;
			bool bIsGenuineJunction = false;
			bool bCanIndicateStraight = false;
			bool bCanIndicateAsNotGenuineJunction = false;
			bool bIndicateTurnNoThreshold = false;
			Vector3 vEntryPos, vEntryVec, vExitPos, vExitVec;

			const CPathNode * prevPathNode = FindNodePointerSafe(directionListNodes[i+1]);
			const CPathNode * pathNode = FindNodePointerSafe(directionListNodes[i+2]);
			const CPathNode * nextPathNode = FindNodePointerSafe(directionListNodes[i+3]);
			if (!prevPathNode || !pathNode || !nextPathNode)	// This is valid now as we don't stream the whole path
				break;

			bool bIgnoreFirstLink = false;
			bool bIgnoreSecondLink = false;
			{	// This shit remove nodes from the angular calculation
				// This is not properly done in one of the functions above which is a good thing after all
				s16 linkTo = 0;
				if (ThePaths.FindNodesLinkIndexBetween2Nodes(prevPathNode, pathNode, linkTo))
				{
					const CPathNodeLink* pLinkTo = &ThePaths.GetNodesLink(prevPathNode, linkTo);
					Assertf(pLinkTo, "GenerateDirections, Link is not loaded!");
					bIgnoreFirstLink = pLinkTo->m_1.m_bDontUseForNavigation;
				}
				if (ThePaths.FindNodesLinkIndexBetween2Nodes(pathNode, nextPathNode, linkTo))
				{
					const CPathNodeLink* pLinkTo = &ThePaths.GetNodesLink(pathNode, linkTo);
					Assertf(pLinkTo, "GenerateDirections, Link is not loaded!");
					bIgnoreSecondLink = pLinkTo->m_1.m_bDontUseForNavigation;
				}
			}

			if (pathNode->m_1.m_indicateKeepLeft || pathNode->m_1.m_indicateKeepRight)
			{
				vOutPos = VEC3V_TO_VECTOR3(nodes[2]->GetPosition());
				fOut_DistanceToTurn = (vOutPos - CGameWorld::FindLocalPlayerCoors()).Mag();
				iOut_Directions = (pathNode->m_1.m_indicateKeepLeft ? DIRECTIONS_KEEP_LEFT : DIRECTIONS_KEEP_RIGHT);
				iOut_StreetNameHash = pathNode->m_streetNameHash;

				m_LastValidDirectionsNode = pathNode->m_address;
				m_iStreetNameHash = iOut_StreetNameHash;
				m_iDirection = iOut_Directions;
				break;
			}

			if (IsJunctionForGenerateDirections( nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], fJunctionTurnDot, fJunctionSmoothDot, iJunctionTurnDir, bIsGenuineJunction, vEntryPos, vEntryVec, vExitPos, vExitVec, bIgnoreFirstLink, bIgnoreSecondLink ) )
			{
				// Attempt to filter out useless Junctions
				if (bIsGenuineJunction)
				{
					int nValidLinks = 0;
					for (int j = 0; j < pathNode->NumLinks(); ++j)
					{
						const CPathNodeLink* pLinkTo = &ThePaths.GetNodesLink(pathNode, j);
						Assertf(pLinkTo, "GenerateDirections, Link is not loaded!");
						if (pLinkTo->m_OtherNode != prevPathNode->m_address && 
							pLinkTo->m_1.m_LanesToOtherNode &&
							!pLinkTo->m_1.m_bShortCut)
						{
							++nValidLinks;
						}
					}

					if (nValidLinks < 2)
						bIsGenuineJunction = false;

					// Maybe check the angle and see if one could indicate left / right properly
					// If it is a fork maybe we want a keep left or right?
					if (nValidLinks > 2)
						bCanIndicateStraight = true;
				}
				
				if (pathNode->FindNumberNonShortcutLinks() > 2)
				{
					bCanIndicateAsNotGenuineJunction = true;
				}

				if (!bCanIndicateAsNotGenuineJunction && pathNode->NumLinks() > 2)
				{
					// So if the prev nodes are in line we can assume that we are latching on to a real road from a side road or so
					Vector3 vPreEntry = VEC3V_TO_VECTOR3(nodes[1]->GetPosition() + nodes[1]->GetLaneCenterOffset()) - VEC3V_TO_VECTOR3(nodes[0]->GetPosition() + nodes[0]->GetLaneCenterOffset());
					vPreEntry.z = 0.f;
					vPreEntry.Normalize();

					static const float COS15 = 0.96f;
					if (vPreEntry.Dot(vEntryVec) > COS15)
						bCanIndicateAsNotGenuineJunction = true;						
				}

				static const float COS30 = 0.866f;
				if (!bIsGenuineJunction && fJunctionSmoothDot < -COS30)
				{
					// In this situation we can be sure we change direction probably due to shortcut between lanes
					bIndicateTurnNoThreshold = true;
					bCanIndicateAsNotGenuineJunction = true;
				}

				if (fJunctionTurnDot > pathDirDotThreshold && !bIndicateTurnNoThreshold)
				{
					if (bIsGenuineJunction && bCanIndicateStraight)
					{
						vOutPos = VEC3V_TO_VECTOR3(nodes[2]->GetPosition());
						fOut_DistanceToTurn = (vOutPos - CGameWorld::FindLocalPlayerCoors()).Mag();
						straightThroughJunctionNode = pathNode->m_address;
						break;
					}
				}
				else
				{
					DEV_ONLY( vDirectionsNode = VEC3V_TO_VECTOR3(nodes[2]->GetPosition()) );

#if __DEV
					if(bDisplayPathsDebug_Directions)
					{
						grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vEntryPos+(ZAXIS*2.0f)), VECTOR3_TO_VEC3V((vEntryPos+vEntryVec)+(ZAXIS*2.0f)), 0.5f, Color_cyan);
						grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vExitPos+(ZAXIS*2.0f)), VECTOR3_TO_VEC3V((vExitPos+vExitVec)+(ZAXIS*2.0f)), 0.5f, Color_cyan);
					}
#endif

					static const float COS15 = 0.96593f;
					if (fJunctionTurnDot < -COS15)
					{
						vOutPos = VEC3V_TO_VECTOR3(nodes[2]->GetPosition());
						fOut_DistanceToTurn = (vOutPos - CGameWorld::FindLocalPlayerCoors()).Mag();
						iOut_StreetNameHash = pathNode->m_streetNameHash;
						iOut_Directions = DIRECTIONS_UTURN;

						m_LastValidDirectionsNode = pathNode->m_address;
						m_iStreetNameHash = iOut_StreetNameHash;
						m_iDirection = iOut_Directions;
						break;
					}

					//-------------------------------------------------------------------------------------------
					// It is a junction, so check which way through it we want to go.
					// Compare the direction going into the junction to the direction going out of the junction.
					if (bIsGenuineJunction || bCanIndicateAsNotGenuineJunction)
					{
						if (iJunctionTurnDir == -1 && (fJunctionTurnDot < pathDirDotThreshold || bIndicateTurnNoThreshold))
						{
							// Turn left.
							vOutPos = VEC3V_TO_VECTOR3(nodes[2]->GetPosition());
							fOut_DistanceToTurn = (vOutPos - CGameWorld::FindLocalPlayerCoors()).Mag();
							iOut_StreetNameHash = pathNode->m_streetNameHash;
							iOut_Directions = bIsGenuineJunction ? DIRECTIONS_LEFT_AT_JUNCTION : DIRECTIONS_KEEP_LEFT;
							m_LastValidDirectionsNode = pathNode->m_address;
							m_iStreetNameHash = iOut_StreetNameHash;
							m_iDirection = iOut_Directions;
							break;
						}
						else if (iJunctionTurnDir == 1 && (fJunctionTurnDot < pathDirDotThreshold || bIndicateTurnNoThreshold))
						{
							// Turn right.
							vOutPos = VEC3V_TO_VECTOR3(nodes[2]->GetPosition());
							fOut_DistanceToTurn = (vOutPos - CGameWorld::FindLocalPlayerCoors()).Mag();
							iOut_StreetNameHash = pathNode->m_streetNameHash;
							iOut_Directions = bIsGenuineJunction ? DIRECTIONS_RIGHT_AT_JUNCTION : DIRECTIONS_KEEP_RIGHT;
							m_LastValidDirectionsNode = pathNode->m_address;
							m_iStreetNameHash = iOut_StreetNameHash;
							m_iDirection = iOut_Directions;
							break;
						}
					}
				}

			}// END if it's a proper junction.

			
			// We got a cached node and only update direction if we actually pass it
			// Be careful to change the check for i size and the way we dig out the node pos
			if (bFoundCacheNode)
			{
				if (iOut_Directions == DIRECTIONS_KEEP_DRIVING) // Directions remained unchanged
				{
					vOutPos = VEC3V_TO_VECTOR3(nodes[1]->GetPosition());
					fOut_DistanceToTurn = (vOutPos - CGameWorld::FindLocalPlayerCoors()).Mag();
					iOut_Directions = m_iDirection;
					iOut_StreetNameHash = m_iStreetNameHash;
				}

				break;
			}

		}// END for each node on directionListNodes (up to first 9)

		if (nNodesToCheck <= 0)		// Not much of the route left! Find the first junction and see where it takes us
		{	
			int nNodesInList = rage::Min(iLookAheadNum, iNodeCount) - 1;
			for (int i = 0; i < nNodesInList; ++i)
			{
				const CRoutePoint* pNode = &routePoints[i];
				const CRoutePoint* pNext = &routePoints[i+1];
				if (!pNode || !pNext)
					break;
				
				const CPathNode * pathNode = FindNodePointerSafe(directionListNodes[i]);
				if (!pathNode)
					break;

				if (pathNode->m_1.m_indicateKeepLeft || pathNode->m_1.m_indicateKeepRight)
				{
					vOutPos = VEC3V_TO_VECTOR3(pNode->GetPosition());
					fOut_DistanceToTurn = (vOutPos - CGameWorld::FindLocalPlayerCoors()).Mag();
					iOut_Directions = (pathNode->m_1.m_indicateKeepLeft ? DIRECTIONS_KEEP_LEFT : DIRECTIONS_KEEP_RIGHT);
					iOut_StreetNameHash = pathNode->m_streetNameHash;

					m_LastValidDirectionsNode = pathNode->m_address;
					m_iStreetNameHash = iOut_StreetNameHash;
					m_iDirection = iOut_Directions;
					break;
				}

				if (pNode->m_bJunctionNode)
				{
					Vector3 vNode = VEC3V_TO_VECTOR3(pNode->GetPosition() + pNode->GetLaneCenterOffset());
					Vector3 vNext = VEC3V_TO_VECTOR3(pNext->GetPosition() + pNext->GetLaneCenterOffset());
					Vector3 JuncDir = vNext - vNode;
					JuncDir.z = 0.f;
					JuncDir.Normalize();
					float JuncDot = DotProduct(JuncDir, playerVehicleDir);
					if (JuncDot < pathDirDotThreshold)
					{
						int TurnDir = (int) Sign( -CrossProduct(playerVehicleDir, JuncDir).z );
						vOutPos = VEC3V_TO_VECTOR3(pNode->GetPosition());

						iOut_StreetNameHash = pathNode->m_streetNameHash;
						iOut_Directions = TurnDir == -1 ? DIRECTIONS_LEFT_AT_JUNCTION : DIRECTIONS_RIGHT_AT_JUNCTION;
						fOut_DistanceToTurn = (vOutPos - CGameWorld::FindLocalPlayerCoors()).Mag();
						break;
					}
				}
			}
		}

		if (iOut_Directions == DIRECTIONS_KEEP_DRIVING && !straightThroughJunctionNode.IsEmpty())
		{
			const CPathNode * pathNode = FindNodePointerSafe(straightThroughJunctionNode);
			if (pathNode)
			{
				iOut_StreetNameHash = pathNode->m_streetNameHash;
				iOut_Directions = DIRECTIONS_STRAIGHT_THROUGH_JUNCTION;

				DEV_ONLY( pathNode->GetCoors(vDirectionsNode) );
			}
		}
	}


#if __DEV
	if(bDisplayPathsDebug_Directions)
	{
		switch(iOut_Directions)
		{
			default : { Assert(false); break; }
			case DIRECTIONS_UNKNOWN : { Displayf("GenerateDirections : (0) DIRECTIONS_UNKNOWN\n"); break; }
			case DIRECTIONS_WRONG_WAY : { Displayf("GenerateDirections : (1) DIRECTIONS_WRONG_WAY\n"); break; }
			case DIRECTIONS_KEEP_DRIVING : { Displayf("GenerateDirections : (2) INSTRUCTIONS_KEEP_DRIVING\n"); break; }
			case DIRECTIONS_LEFT_AT_JUNCTION : { Displayf("GenerateDirections : (3) INSTRUCTIONS_LEFT_AT_JUNCTION\n"); break; }
			case DIRECTIONS_RIGHT_AT_JUNCTION : { Displayf("GenerateDirections : (4) INSTRUCTIONS_RIGHT_AT_JUNCTION\n"); break; }
			case DIRECTIONS_STRAIGHT_THROUGH_JUNCTION : { Displayf("GenerateDirections : (5) INSTRUCTIONS_STRAIGHT_THROUGH_JUNCTION\n"); break; }
			case DIRECTIONS_KEEP_LEFT : { Displayf("GenerateDirections : (6) INSTRUCTIONS_KEEP_LEFT\n"); break; }
			case DIRECTIONS_KEEP_RIGHT : { Displayf("GenerateDirections : (7) INSTRUCTIONS_KEEP_RIGHT\n"); break; }
			case DIRECTIONS_UTURN : { Displayf("GenerateDirections : (8) INSTRUCTIONS_UTURN\n"); break; }
		}

#if DEBUG_DRAW
		grcDebugDraw::Sphere( VECTOR3_TO_VEC3V(vDirectionsNode + ZAXIS), 0.4f, Color_magenta, true, 4);
#endif

		char strDirections[CPathFind::DIRECTIONS_MAX][64] =
		{
			"DIRECTIONS_UNKNOWN",
			"DIRECTIONS_WRONG_WAY",
			"DIRECTIONS_KEEP_DRIVING",
			"DIRECTIONS_LEFT_AT_JUNCTION",
			"DIRECTIONS_RIGHT_AT_JUNCTION",
			"DIRECTIONS_STRAIGHT_THROUGH_JUNCTION",
			"DIRECTIONS_KEEP_LEFT",
			"DIRECTIONS_KEEP_RIGHT",
			"DIRECTIONS_UTURN",
		};

		if (iOut_Directions >= 0 && iOut_Directions < CPathFind::DIRECTIONS_MAX)
		{
			char strDebug[128];
			sprintf(strDebug, "%s, Dist: %.2f", strDirections[iOut_Directions], fOut_DistanceToTurn);
			grcDebugDraw::Text( Vector2(0.1f,0.1f), Color_yellow, strDebug, false, 1.5f, 1.5f, 1 );
		}
		else
			grcDebugDraw::Text( Vector2(0.1f,0.1f), Color_yellow, "INVALID DIRECTION!", false, 1.5f, 1.5f, 1 );
	}
#endif //__DEV

	return 0;
}

//--------------------------------------------------------------------------------------
// Player's road calculation
// Code which attempts to identity which nodes are approximately on the player's road.

s32 g_PlayersRoad_iMaxNumNodesToMark = 0;
s32 g_PlayersRoad_iCurrentNumNodes = 0;
CPathNode ** g_PlayersRoad_ppNodes = NULL;
Vector3 g_PlayersRoad_vSearchOrigin = VEC3_ZERO;
float g_PlayersRoad_fSearchPlaneD = 0.0f;
Vector3 g_PlayersRoad_vSearchDirection = VEC3_ZERO;
float g_PlayersRoad_fMaxRange = 0.0f;
const CPathNode ** g_PlayersRoad_pPrimaryJunctionNode = NULL;
const CPathNode ** g_PlayersRoad_pPrimaryJunctionEntranceNode = NULL;

bool MarkPlayersRoadNodes_OnVisitNode(CPathNode * pNode)
{
	if(g_PlayersRoad_iCurrentNumNodes >= g_PlayersRoad_iMaxNumNodesToMark)
		return false;

	pNode->m_1.m_onPlayersRoad = true;

	g_PlayersRoad_ppNodes[g_PlayersRoad_iCurrentNumNodes++] = pNode;

	return true;
}

bool MarkPlayersRoadNodes_ShouldVisitAdjacentNode(const CPathNode * pParentNode, const CPathNodeLink & link, const CPathNode * pAdjacentNode, float fParentCost, float & fOut_AdjacentCost, const Vector3 & vParentNormal, Vector3 & vOut_AdjacentNormal)
{
	// Don't visit nodes which have been already visited
	if(pAdjacentNode->m_distanceToTarget != PF_VERYLARGENUMBER)
		return false;

	if(link.IsShortCut())
		return false;

	if(fParentCost != 0.0f)
	{
		fOut_AdjacentCost = fParentCost + link.m_1.m_Distance;
	}
	else
	{
		Vector3 vAdjacentNodeCoords;
		pAdjacentNode->GetCoors(vAdjacentNodeCoords);
		Vector3 vOriginToAdjacent = vAdjacentNodeCoords-g_PlayersRoad_vSearchOrigin;

		vOriginToAdjacent.z = 0.0f;
		fOut_AdjacentCost = vOriginToAdjacent.Mag();

		// Don't visit node if its more than 100m behind the search plane (was 15m, was 10m ...)
		if(DotProduct(vAdjacentNodeCoords, g_PlayersRoad_vSearchDirection) + g_PlayersRoad_fSearchPlaneD < 0.0f)
		{
			fOut_AdjacentCost += g_PlayersRoad_fMaxRange - 100.0f;
		}
	}

	// Don't visit nodes outside of the search radius
	if(fOut_AdjacentCost > g_PlayersRoad_fMaxRange)
		return false;

	if(pAdjacentNode->IsJunctionNode())
	{
		if(!pAdjacentNode->IsSlipLane() && !pAdjacentNode->IsFalseJunction() && !*g_PlayersRoad_pPrimaryJunctionNode && fOut_AdjacentCost <= 50.0f)
		{
			*g_PlayersRoad_pPrimaryJunctionNode = pAdjacentNode;
			*g_PlayersRoad_pPrimaryJunctionEntranceNode = pParentNode;
		}

		Vector3 vAdjacentNodeCoords;
		pAdjacentNode->GetCoors(vAdjacentNodeCoords);

		Vector3 vParentNodeCoords;
		pParentNode->GetCoors(vParentNodeCoords);

		vOut_AdjacentNormal = vAdjacentNodeCoords - vParentNodeCoords;

		vOut_AdjacentNormal.z = 0.0f;
		vOut_AdjacentNormal.NormalizeFast();
	}

	if(pParentNode->IsJunctionNode())
	{
		Vector3 vAdjacentNodeCoords;
		pAdjacentNode->GetCoors(vAdjacentNodeCoords);
		
		// Once nodes are over a certain distance away, reject those which are not in a frustum defined by the orientation of the search dir
		if(fOut_AdjacentCost > 20.0f)
		{
			Vector3 vOriginToAdjacent = vAdjacentNodeCoords-g_PlayersRoad_vSearchOrigin;

			vOriginToAdjacent.z = 0.0f;
			vOriginToAdjacent.NormalizeFast();

			if(DotProduct(vOriginToAdjacent, g_PlayersRoad_vSearchDirection) < 0.707f)
			{
				return false;
			}
		}

		if(!pAdjacentNode->IsJunctionNode())
		{
			Vector3 vParentNodeCoords;
			pParentNode->GetCoors(vParentNodeCoords);

			vOut_AdjacentNormal = vAdjacentNodeCoords - vParentNodeCoords;

			vOut_AdjacentNormal.z = 0.0f;
			vOut_AdjacentNormal.NormalizeFast();
		}

		float fDot = DotProduct(vOut_AdjacentNormal, vParentNormal);

		for(s32 i = NUM_PLAYERS_ROAD_CID_PENALTIES - 1; i >= 0; --i)
		{
			if(fDot < CVehiclePopulation::ms_fPlayersRoadChangeInDirectionPenalties[i][PLAYERS_ROAD_CID_ANGLE])
			{
				fOut_AdjacentCost += CVehiclePopulation::ms_fPlayersRoadChangeInDirectionPenalties[i][PLAYERS_ROAD_CID_PENALTY];
				break;
			}
		}

		if(fOut_AdjacentCost > g_PlayersRoad_fMaxRange)
		{
			return false;
		}
	}

	return true;
}

s32 CPathFind::MarkPlayersRoadNodes(const Vector3 & vStartPosition, const CNodeAddress startNode, const float fMaxRange, const Vector3 & vSearchDirection, CPathNode ** pOut_PlayersRoadNodes, const s32 iMaxNumNodesToMark, const CPathNode **pPlayerJunctionNode, const CPathNode **pPlayerJunctionEntranceNode)
{
	g_PlayersRoad_iMaxNumNodesToMark = iMaxNumNodesToMark;
	g_PlayersRoad_iCurrentNumNodes = 0;
	g_PlayersRoad_vSearchOrigin = vStartPosition;
	g_PlayersRoad_vSearchDirection = vSearchDirection;
	g_PlayersRoad_fMaxRange = fMaxRange;
	g_PlayersRoad_ppNodes = pOut_PlayersRoadNodes;
	g_PlayersRoad_fSearchPlaneD = - DotProduct(vSearchDirection, vStartPosition);

	g_PlayersRoad_pPrimaryJunctionNode = pPlayerJunctionNode;
	g_PlayersRoad_pPrimaryJunctionEntranceNode = pPlayerJunctionEntranceNode;

	*g_PlayersRoad_pPrimaryJunctionNode = NULL;
	*g_PlayersRoad_pPrimaryJunctionEntranceNode = NULL;

	FloodFillFromPosition(vStartPosition, startNode, MarkPlayersRoadNodes_OnVisitNode, MarkPlayersRoadNodes_ShouldVisitAdjacentNode, 0.0f);

	return g_PlayersRoad_iCurrentNumNodes;
}

//------------------------------------------------------------------------------------------
// NAME : FloodFillFromPosition
// PURPOSE : Generic floodfill across pathnodes graph
//	Uses a binary heap to visit nodes in an order determined by their cost function
//
// bool OnVisitNode(CPathNode * pNode)
//	- should return false to terminate the search at this node
//
// bool ShouldVisitAdjacentNode(CPathNode * pParent, const CPathNodeLink & link, CPathNode * pAdjacentNode, float fParentCost, float & fOut_AdjacentCost, u32 iParentFlags, u32 & iOut_AdjacentFlags);
//	- returns whether the search is to visit this node
//	- should set the fOut_AdjacentCost to be the cost of the new node
//  - iOut_AdjacentFlags can be used to set custom properties associated with search node

void CPathFind::FloodFillFromPosition(
	const Vector3 & vPosition,
	CNodeAddress startNode,
	bool (*OnVisitNode)(CPathNode*),
	bool (*ShouldVisitAdjacentNode)(const CPathNode*,const CPathNodeLink&,const CPathNode*,float,float&,const Vector3&,Vector3&),
	const float fStartNodeCost)
{
	//----------------------------------------------------------------
	// Search for start node, if not specified
	// NB: For player searches we can used cached node in PlayerInfo

	if(startNode.IsEmpty())
		startNode = FindNodeClosestToCoors(vPosition, 50.0f, IncludeSwitchedOffNodes, false);
	if(startNode.IsEmpty())
		return;
	CPathNode * pStartNode = FindNodePointerSafe(startNode);
	if(!pStartNode)
		return;

#if SERIOUSDEBUG
	ASSERT_ONLY(VerifyAllNodesDistanceToTarget();)
#endif // SERIOUSDEBUG

	// Ensure that this node's distance is encountered as reset when first seen
	Assert(pStartNode->m_distanceToTarget == PF_VERYLARGENUMBER);
	s32 iNumNodesToBeCleared = 0;

	bool pathRegionsToClearIfNeeded[PATHFINDREGIONS];
	sysMemSet(pathRegionsToClearIfNeeded, 0, sizeof(pathRegionsToClearIfNeeded));

	static const s32 MAX_NODES = 256;
	static std::pair<float, TPathfindFloodItem> nodes[MAX_NODES];

	u32 nodeIndex = 0;
	u32 freeIndex = 0;

	pStartNode->SetPrevious(NULL);
	pStartNode->SetNext(NULL);

	nodes[0].first = fStartNodeCost;
	nodes[0].second.m_pPathNode = pStartNode;
	nodes[0].second.m_vNormal = g_PlayersRoad_vSearchDirection;
	++freeIndex;

	//--------------------------------------------------------------------------
	// Process until we've exhausted qualifying nodes

	while(nodeIndex != freeIndex)
	{
		const float &fCurrentKey = nodes[nodeIndex].first;
		TPathfindFloodItem &currentItem = nodes[nodeIndex].second;

		OnVisitNode(currentItem.m_pPathNode);

		currentItem.m_pPathNode->m_distanceToTarget = 0;
		aNodesToBeCleared[iNumNodesToBeCleared++] = currentItem.m_pPathNode->m_address;
		Assert(currentItem.m_pPathNode->GetAddrRegion() < PATHFINDREGIONS);
		pathRegionsToClearIfNeeded[currentItem.m_pPathNode->GetAddrRegion()] = true;

		// We cannot visit more nodes than we have storage to clear
		if(!aiVerify(iNumNodesToBeCleared <= PF_MAXNUMNODES_INSEARCH))
		{
			break;
		}

		for(s32 a=0; a<currentItem.m_pPathNode->NumLinks(); a++)
		{
			const CPathNodeLink & link = GetNodesLink(currentItem.m_pPathNode, a);

			CPathNode * pLinkedNode = FindNodePointerSafe(link.m_OtherNode);

			float fAdjacentKey = 0.0f;
			Vector3 vAdjacentNormal(Vector3::ZeroType);
			if( pLinkedNode )
			{
				if( ShouldVisitAdjacentNode( currentItem.m_pPathNode, link, pLinkedNode, fCurrentKey, fAdjacentKey, currentItem.m_vNormal, vAdjacentNormal ) )
				{
					if(aiVerify( freeIndex != nodeIndex ) )
					{
						pLinkedNode->SetPrevious(currentItem.m_pPathNode);
						pLinkedNode->SetNext(NULL);

						nodes[freeIndex].first = fAdjacentKey;
						nodes[freeIndex].second.m_pPathNode = pLinkedNode;
						nodes[freeIndex].second.m_vNormal = vAdjacentNormal;
						freeIndex = (freeIndex + 1) % MAX_NODES;
					}
				}
				else
				{
					pLinkedNode->m_distanceToTarget = 0;
					aNodesToBeCleared[iNumNodesToBeCleared++] = pLinkedNode->m_address;
					Assert(pLinkedNode->GetAddrRegion() < PATHFINDREGIONS);
					pathRegionsToClearIfNeeded[pLinkedNode->GetAddrRegion()] = true;
				}
			}
		}

		nodeIndex = (nodeIndex + 1) % MAX_NODES;
	}

	//-----------------------------------------------------------
	// Ensure that we reset distance value on all visited nodes

	Assert(iNumNodesToBeCleared <= PF_MAXNUMNODES_INSEARCH);
	if (iNumNodesToBeCleared <= PF_MAXNUMNODES_INSEARCH)
	{
		while(iNumNodesToBeCleared--)
		{
			FindNodePointer(aNodesToBeCleared[iNumNodesToBeCleared])->m_distanceToTarget = PF_VERYLARGENUMBER;
		}
	}
	else
	{
		ClearAllNodesDistanceToTarget(pathRegionsToClearIfNeeded);
	}

#if SERIOUSDEBUG
	ASSERT_ONLY(VerifyAllNodesDistanceToTarget();)
#endif // SERIOUSDEBUG
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CountNonSpecialNeighbours
// PURPOSE :  Counts the number of neighbours this node has that aren't special
/////////////////////////////////////////////////////////////////////////////////
s32 CPathFind::CountNonSpecialNeighbours(const CPathNode *pNode) const
{
	s32	Result = 0;

	for(u32 Link = 0; Link < pNode->NumLinks(); Link++)
	{
		CNodeAddress neighbourNodeAddr = GetNodesLinkedNodeAddr(pNode, Link);

		if(IsRegionLoaded(neighbourNodeAddr))
		{
			const CPathNode* pNeighbourNode = FindNodePointer(neighbourNodeAddr);

			if (!pNeighbourNode->HasSpecialFunction())
			{
				Result++;
			}
		}
	}
	return Result;
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IdentifyRoadBlocks
// PURPOSE :  Does a flood fill around the start area. Junctions and nodes are
//			  marked as roadblocks to contain the floodfill.
//			  This is used by the dispatch code to find the locations of the
//			  roadblocks needed to contain the player.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::IdentifyRoadBlocks(const Vector3& StartCoors, CNodeAddress *pResult, s32 *pNeighbourTowardsPlayer, s32 *pNumCarsNeeded , s32 &roadBlocksFound, float minDist, float maxDist, s32 numRequested)
{
	s32			NumNodesToBeCleared;
	s32			Node, CandidateDist;
	s32			CurrentRoughDist, CurrentHashValue, C;
	u32			link;
	s32			dbgNodesProcessed = 0;
	CPathNode		*pCurrNode, *pCandidateNode;
	CNodeAddress	TargetNode, CandidateNode, NeighbourNode;

#define MAX_NUM (32)
	static	CNodeAddress	aRoadBlockNodes[MAX_NUM];
	static	s32			aRegionLinkIndexNeighbourTowardsPlayer[MAX_NUM];
	static	s32			numRoadBlockNodes;

	numRoadBlockNodes = 0;
	NumNodesToBeCleared = 0;			// we keep track of the nodes to be cleared in an array
	int NodesOnListDuringPathfinding = 0;	// So that we know how many nodes we're processing at the moment

	// Find the node closest to the target
	CNodeAddress StartNode = FindNodeClosestToCoors(StartCoors, 999999.9f, IncludeSwitchedOffNodes, false, false);

	if (StartNode.IsEmpty())
	{
		return;
	}

#if SERIOUSDEBUG
	ASSERT_ONLY(VerifyAllNodesDistanceToTarget();)
#endif // SERIOUSDEBUG

	CPathNode	*pStartNode = FindNodePointer(StartNode);

	// Initially the hash lists are emptyAddr
	for (C = 0; C < PF_HASHLISTENTRIES; C++)
	{
		apHashTable[C] = NULL;
	}

	// Put the target node on the list
	AddNodeToList(pStartNode, 0, 0);
	++NodesOnListDuringPathfinding;
	aNodesToBeCleared[NumNodesToBeCleared++] = pStartNode->m_address;		// Make sure it gets cleared

	CurrentRoughDist = CurrentHashValue = 0;		// Where in the list are we now ?

	u32 dbgNumberOfLoops = 0;

	// We are now ready to start processing the list. We will keep processing nodes
	// (and adding their neighbours on the list)

	while (NodesOnListDuringPathfinding > 0)
	{
		if (CurrentHashValue == (PF_HASHLISTENTRIES - 1) )
		{
			dbgNumberOfLoops++;
			Assertf(dbgNumberOfLoops < 50, "Stuck in DoPathSearch");
		}

		// Go through the list for this Hash value.
		pCurrNode = apHashTable[CurrentHashValue];

		while (pCurrNode != NULL)
		{
			dbgNodesProcessed++;
			Assert(CurrentHashValue < PF_HASHLISTENTRIES);

			// Do we want to place a roadblock here?
			Vector3 currNodeCoors;
			pCurrNode->GetCoors(currNodeCoors);
			float DistToStart = (StartCoors - currNodeCoors).XYMag();

			if((DistToStart > minDist && pCurrNode->NumLinks() > 2) ||
				(DistToStart > maxDist) )
			{			// This guy will have a roadblock placed on it.
				// Make sure it's not already on the list.
				bool bAlreadyOnList = false;
				for (s32 i = 0; i < numRoadBlockNodes; i++)
				{
					if (pCurrNode->m_address == aRoadBlockNodes[i])
					{
						bAlreadyOnList = true;
					}
				}
				if (!bAlreadyOnList)
				{
					Assert(numRoadBlockNodes < MAX_NUM);
					if (numRoadBlockNodes < MAX_NUM)
					{
						aRoadBlockNodes[numRoadBlockNodes] = pCurrNode->m_address;
						numRoadBlockNodes++;
					}
				}
			}
			else
			{	// If this is not a roadblock node we continue the flood fill.
				for(link = 0; link < pCurrNode->NumLinks(); link++)
				{
#if __ASSERT
					const int Region=pCurrNode->GetAddrRegion();
#endif // __ASSERT
					Assert(Region>=0);
					Assert(Region<PATHFINDREGIONS+MAXINTERIORS);
					Assert(apRegions[Region] && apRegions[Region]->aNodes);
					const CPathNodeLink* pLink = &GetNodesLink(pCurrNode, link);

					CandidateNode = pLink->m_OtherNode;

					if (IsRegionLoaded(CandidateNode))		// If this Region is not loaded we ignore this particular node
					{
						pCandidateNode = FindNodePointer(CandidateNode);

						CandidateDist = pCurrNode->m_distanceToTarget + (s16)pLink->m_1.m_Distance;

						Assert(pLink->m_1.m_Distance > 0);

						if (CandidateDist < pCandidateNode->m_distanceToTarget)
						{
							// The distance we found for this node is smaller than the distance we
							// already had. This means that it has to be added to the linked list
							// of that specific distance. (And it would have to be removed from the
							// linked list it might be in at the moment)
							if (pCandidateNode->m_distanceToTarget != PF_VERYLARGENUMBER)
							{
								RemoveNodeFromList(pCandidateNode);
								--NodesOnListDuringPathfinding;
							}
							// The distance value of this node is going to be written to so we
							// have to make sure it gets cleared when we're done.
							if (pCandidateNode->m_distanceToTarget == PF_VERYLARGENUMBER)
							{
								Assertf(NumNodesToBeCleared < PF_MAXNUMNODES_INSEARCH, "Run out of space in the aNodesToBeCleared array");
								if (NumNodesToBeCleared < PF_MAXNUMNODES_INSEARCH)	// Only add nodes if there is enough space.
								{
									aNodesToBeCleared[NumNodesToBeCleared++] = pCandidateNode->m_address;
								}
							}
							AddNodeToList(pCandidateNode, CandidateDist, CandidateDist);
							++NodesOnListDuringPathfinding;
						}
					}
				}
			}

			// Remove ourselves from the list
			RemoveNodeFromList(pCurrNode);
			--NodesOnListDuringPathfinding;

			// Find next node to process (next field has not been affected by removal)
			pCurrNode = pCurrNode->GetNext();
		}
		CurrentHashValue = (++CurrentRoughDist) & (PF_HASHLISTENTRIES-1) ;		// process next linked list
	}

	// For each of the roadblocks we found we have to find the node that is closest to the player (so that the dispatch code can place the cars in the right orientation)
	for (s32 i = 0; i < numRoadBlockNodes; i++)
	{
		const CPathNode *pNode1 = FindNodePointer(aRoadBlockNodes[i]);
		s32 SmallestDist = PF_VERYLARGENUMBER;
		s32 regionLinkIndexBestNeighbour = -1;

		for(u32 n = 0; n < pNode1->NumLinks(); n++)
		{
			const CPathNodeLink *pLink = &GetNodesLink(pNode1, n);
			if (IsRegionLoaded(pLink->m_OtherNode))
			{
				CPathNode *pOtherNode = FindNodePointer(pLink->m_OtherNode);

				if (pOtherNode->m_distanceToTarget < SmallestDist)
				{
					SmallestDist = pOtherNode->m_distanceToTarget;
					regionLinkIndexBestNeighbour = pNode1->m_startIndexOfLinks + n;
				}
			}
		}
		if(regionLinkIndexBestNeighbour >= 0)
		{
			aRegionLinkIndexNeighbourTowardsPlayer[i] = regionLinkIndexBestNeighbour;
		}
		else
		{
			Assert(pNode1->NumLinks());
			aRegionLinkIndexNeighbourTowardsPlayer[i] = pNode1->m_startIndexOfLinks;
		}
	}

	// Sort the roadblocks here. Most important roadblock comes first.
	bool	bChanges = true;
	while (bChanges)
	{
		CNodeAddress	TempNode;
		s32			TempNeighbour;
		bChanges = false;

		for (s32 i = 0; i < numRoadBlockNodes - 1; i++)
		{
			bool	bSwap = false;
			CPathNodeLink *pLink1 = FindLinkPointerSafe(aRoadBlockNodes[i].GetRegion(),aRegionLinkIndexNeighbourTowardsPlayer[i]);
			CPathNodeLink *pLink2 = FindLinkPointerSafe(aRoadBlockNodes[i+1].GetRegion(),aRegionLinkIndexNeighbourTowardsPlayer[i+1]);

			s32 lanes1 = pLink1->m_1.m_LanesToOtherNode + pLink1->m_1.m_LanesFromOtherNode;
			s32 lanes2 = pLink2->m_1.m_LanesToOtherNode + pLink2->m_1.m_LanesFromOtherNode;

			if (lanes1 < lanes2)
			{
				bSwap = true;
			}
			else if (lanes1 == lanes2)
			{		// If lanes are equal; the one closest to the player is more important
				if (FindNodePointer(aRoadBlockNodes[i])->m_distanceToTarget > FindNodePointer(aRoadBlockNodes[i+1])->m_distanceToTarget)
				{
					bSwap = true;
				}
			}

			if (bSwap)
			{
				TempNode = aRoadBlockNodes[i];
				aRoadBlockNodes[i] = aRoadBlockNodes[i+1];
				aRoadBlockNodes[i+1] = TempNode;

				TempNeighbour = aRegionLinkIndexNeighbourTowardsPlayer[i];
				aRegionLinkIndexNeighbourTowardsPlayer[i] = aRegionLinkIndexNeighbourTowardsPlayer[i+1];
				aRegionLinkIndexNeighbourTowardsPlayer[i+1] = TempNeighbour;

				bChanges = true;
			}
		}
	}

	// Copy the coordinates into the result array.
	s32 numFound = rage::Min(numRequested, numRoadBlockNodes);
	for (s32 i = 0; i < numFound; i++)
	{
		// If the roadblock exists already (for a different player) we don't add it again.
		bool bAlreadyThere = false;
		for (s32 k = 0; k < roadBlocksFound; k++)
		{
			if (pResult[k] == aRoadBlockNodes[i])
			{
				pNumCarsNeeded[k]++;
				bAlreadyThere = true;
				break;
			}
		}
		if (!bAlreadyThere)
		{
			pResult[roadBlocksFound] = aRoadBlockNodes[i];
			pNeighbourTowardsPlayer[roadBlocksFound] = aRegionLinkIndexNeighbourTowardsPlayer[i];
			pNumCarsNeeded[roadBlocksFound] = 1;
			roadBlocksFound++;
		}
	}

	// We're done. Time to clear the nodes now.
	for (Node = 0; Node < NumNodesToBeCleared; Node++)
	{
		FindNodePointer(aNodesToBeCleared[Node])->m_distanceToTarget = PF_VERYLARGENUMBER;
	}

#if SERIOUSDEBUG
	ASSERT_ONLY(VerifyAllNodesDistanceToTarget();)
#endif // SERIOUSDEBUG
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalculateTravelDistanceBetweenNodes
// PURPOSE :  Works out how many meters a car would have to travel to go from one node
//			  to the other.
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::CalculateTravelDistanceBetweenNodes(float Point1X, float Point1Y, float Point1Z, float Point2X, float Point2Y, float Point2Z)
{
	s32			PathSize;
	CNodeAddress	NodesFound;
	float			TotalDistance;

	DoPathSearch(Vector3(Point1X, Point1Y, Point1Z), EmptyNodeAddress, Vector3(Point2X, Point2Y, Point2Z),
		&NodesFound, &PathSize, 0,
		&TotalDistance, 999999.9f, &EmptyNodeAddress, 999999,
		false, EmptyNodeAddress, false, false);

	return TotalDistance;	
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveBadStartNode
// PURPOSE :  We might get a node going into the wrong direction. Cut that one out.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::RemoveBadStartNode(const Vector3& StartCoors, CVehicleIntelligence *pAutoPilot, s32 *pNumNodes, s32 testNode)
{
	CVehicleNodeList * pNodeList = pAutoPilot->GetNodeList();
	Assertf(pNodeList, "CPathFind::RemoveBadStartNode - vehicle has no node list");
	if(!pNodeList)
		return;

	float DeltaX1, DeltaY1, DeltaX2, DeltaY2;
	if (*pNumNodes < 2) return;

	if (!IsRegionLoaded(pNodeList->GetPathNodeAddr(testNode+0))) return;
	if (!IsRegionLoaded(pNodeList->GetPathNodeAddr(testNode+1))) return;

	const CPathNode *pNode0 = FindNodePointer(pNodeList->GetPathNodeAddr(testNode+0));
	const CPathNode *pNode1 = FindNodePointer(pNodeList->GetPathNodeAddr(testNode+1));

	// Check dotproduct between first and second node. If it's in the wrong direction we ditch node 0
	Vector3 node0Coors, node1Coors;
	pNode0->GetCoors(node0Coors);
	pNode1->GetCoors(node1Coors);
	DeltaX1 = node0Coors.x - StartCoors.x;
	DeltaY1 = node0Coors.y - StartCoors.y;
	DeltaX2 = node1Coors.x - StartCoors.x;
	DeltaY2 = node1Coors.y - StartCoors.y;

	if (DeltaX1 * DeltaX2 + DeltaY1 * DeltaY2 < 0.0f)
	{
		(*pNumNodes)--;

		pNodeList->ShiftNodesByOne();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalculateFitnessValueForNode
// PURPOSE : Should be used to determine which node is best suited to connect to
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::CalculateFitnessValueForNode(const Vector3& Pos, const Vector3& Dir, const CPathNode* pNode) const
{
	Assert(pNode);
	const Vector3 NodePos = pNode->GetPos();
	float BestDist = Max(Pos.Dist(NodePos) * 10.f, 100.f);	// Bad fitness per default

	u32 nNodeLinks = pNode->NumLinks();
	for (u32 i = 0; i < nNodeLinks; ++i)
	{
		CPathNodeLink NodeLink = GetNodesLink(pNode, i);
		const CPathNode* pOtherNode = FindNodePointerSafe(NodeLink.m_OtherNode);
		if (pOtherNode)
		{
			const Vector3 OtherNodePos = pOtherNode->GetPos();
			float DistToLink = geomDistances::DistanceSegToPoint(NodePos, OtherNodePos, Pos);

			Vector3 LinkDir;
			bool bIgnoreDirCheck = false;
			if (NodeLink.m_1.m_LanesToOtherNode > 0 && NodeLink.m_1.m_LanesFromOtherNode > 0)
				bIgnoreDirCheck = true;
			else if (NodeLink.m_1.m_LanesToOtherNode > 0)
				LinkDir = OtherNodePos - NodePos;
			else
				LinkDir = NodePos - OtherNodePos;

			// Test direction if it is only in one direction
			if (DistToLink < BestDist && (bIgnoreDirCheck || LinkDir.Dot(Dir) > 0.f))
				BestDist = DistToLink;
		}
	}

	return BestDist;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveNodeFromList
// PURPOSE :  Removes this node from the list
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::RemoveNodeFromList(CPathNode* __restrict pNode)
{
	/*
	#if SERIOUSDEBUG
	s32	Test = 0;
	CPathNode *pTestNode = pNode->GetPrevious();
	Assert(pTestNode);
	while (pTestNode->GetNext() != NULL)
	{
	pTestNode = pTestNode->GetNext();
	Test++;
	Assert(Test < 200);
	}
	#endif // SERIOUSDEBUG
	*/

	CPathNode* __restrict pPrevNode = pNode->GetPrevious();
	CPathNode* __restrict pNextNode = pNode->GetNext();

	AI_OPTIMISATIONS_OFF_ONLY(Assertf(pPrevNode, "pPrevNode is null, might need to investigate why!"));
	if (pPrevNode)
		pPrevNode->SetNext(pNextNode);

	if (pNextNode)
		pNextNode->SetPrevious(pPrevNode);

//	NodesOnListDuringPathfinding--;

// #if __BANK
// 	if (bSpewPathfindQueryProgress)
// 	{
// 		aiDisplayf("DoPathSearch: Removing Node %d:%d from list", pNode->GetAddrRegion(), pNode->GetAddrIndex());
// 	}
// #endif //__BANK

#if SERIOUSDEBUG
	Test = 0;
	pTestNode = pNode->GetPrevious();
	Assert(pTestNode);
	while (pTestNode->GetNext() != NULL)
	{
		pTestNode = pTestNode->GetNext();
		Test++;
		Assert(Test < 200);
	}
#endif // SERIOUSDEBUG
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddNodeToList
// PURPOSE :  Adds this node to the list (the start of the list in fact)
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::AddNodeToList(CPathNode *pNode, int distToTarget, int distForHash)
{
	s16	HashEntry;

#if SERIOUSDEBUG
	HashEntry = FindHashValue(distForHash);
	s32	Test = 0;
	//	CPathNode *pTestNode = aHashTable[HashEntry].GetNext();
	CPathNode *pTestNode = apHashTable[HashEntry];
	while (pTestNode != NULL)
	{
		Assert(pTestNode != pNode);
		pTestNode = pTestNode->GetNext();
		Test++;
		Assert(Test < 200);
	}
#endif // SERIOUSDEBUG

	// Try and catch weird bug.
	Assert(pNode->GetAddrRegion() < PATHFINDREGIONS + MAXINTERIORS);
	Assert(pNode->GetAddrIndex() < 10000);

	// Where does this one go in the hash list ?
	HashEntry = (s16)(FindHashValue(distForHash));

	Assert(HashEntry >= 0 && HashEntry < PF_HASHLISTENTRIES);

	pNode->SetNext(apHashTable[HashEntry]);
	pNode->SetPrevious((CPathNode *) &(apHashTable[HashEntry]));

	if (apHashTable[HashEntry])
	{
		apHashTable[HashEntry]->SetPrevious(pNode);	
	}
	apHashTable[HashEntry] = pNode;

	Assert(distToTarget <= 0x7fff);
	pNode->m_distanceToTarget = (s16)(distToTarget);

	Assert(pNode->m_distanceToTarget != PF_VERYLARGENUMBER);

//	NodesOnListDuringPathfinding++;

#if SERIOUSDEBUG
	Test = 0;
	pTestNode = apHashTable[HashEntry];
	Assert(pTestNode);
	while (pTestNode->GetNext() != NULL)
	{
		pTestNode = pTestNode->GetNext();
		Test++;
		Assert(Test < 200);
	}
#endif // SERIOUSDEBUG

// #if __BANK
// 	if (bSpewPathfindQueryProgress)
// 	{
// 		aiDisplayf("DoPathSearch: Adding Node %d:%d from list, dist to target: %d", pNode->GetAddrRegion(), pNode->GetAddrIndex(), Dist);
// 	}
// #endif //__BANK
}



#if __BANK
void DisplayPathsDifferenceDebugForPoint(Vector3& nodeCrs, bool bNode)
{
	static phSegment		testSegment[2];
	static phIntersection	intersections[2];

	for (int i = 0; i < 2; i++)
	{
		intersections[i].Reset();
	}
	testSegment[0].Set(nodeCrs + Vector3(0.0f, 0.0f, 2.0f), nodeCrs);
	testSegment[1].Set(nodeCrs, nodeCrs - Vector3(0.0f, 0.0f, 2.0f));

	Matrix34 matSegBox;
	matSegBox.Identity();
	matSegBox.d = Vector3(5.0f, 5.0f, 5.0f);

	for(int i=0; i<2; i++)
		CPhysics::GetLevel()->TestProbe(testSegment[i], &intersections[i], NULL, ArchetypeFlags::GTA_MAP_TYPE_MOVER);

	float	nearestHeight = -100000.0f;

	if (intersections[0].IsAHit())
	{
		nearestHeight = intersections[0].GetPosition().GetZf();
	}
	if (intersections[1].IsAHit())
	{
		if ( rage::Abs(intersections[1].GetPosition().GetZf()-nodeCrs.z) < rage::Abs(nodeCrs.z-nearestHeight) )
		{
			nearestHeight = intersections[1].GetPosition().GetZf();
		}
	}
	float	diff = nodeCrs.z - nearestHeight;
	CRGBA	colour = CRGBA(100, 100, 255, 255);

	if (bNode || rage::Abs(diff) > 0.2f)
	{
		char debugText[25];

		if (rage::Abs(diff) > 0.5f)
		{
			colour = CRGBA(255, 100, 100, 255);
		}
		else if (rage::Abs(diff) > 0.20f)
		{
			colour = CRGBA(255, 100, 255, 255);
		}

		if (rage::Abs(diff) > 100.0f)
		{
			sprintf(debugText, "No collision %f", diff);
		}
		else
		{
			sprintf(debugText, "%.2f", diff);
		}
		grcDebugDraw::Text(nodeCrs + Vector3(0.f,0.f,0.1f), colour, debugText);

		if (!bNode)
		{
			grcDebugDraw::Line(nodeCrs, nodeCrs + Vector3(0.0f,0.0f,1.0f), CRGBA(0, 0, 255, 255));
		}
	}
}



#if __BANK

void CPathFind::DisplayPathHistory() const
{
	int iX = 64;
	int iY = 64;

	Vector2 vPos;
	char txt[256];

	for(s32 p=0; p<ms_iMaxPathHistory; p++)
	{
		s32 iDiff = m_iLastSearch - m_PathHistory[p].m_iSearch;
		float a = ((float)iDiff) / ((float)ms_iMaxPathHistory);
		a = 1.f-(a*0.75f);

		if(m_PathHistory[p].m_vStartCoords.Mag2() > SMALL_FLOAT && m_PathHistory[p].m_vTargetCoords.Mag2() > SMALL_FLOAT)
		{		
			sprintf(txt, "%i) (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)  gps:%s, path found:%s, time taken:%.2f",
				p,
				m_PathHistory[p].m_vStartCoords.x, m_PathHistory[p].m_vStartCoords.y, m_PathHistory[p].m_vStartCoords.z,
				m_PathHistory[p].m_vTargetCoords.x, m_PathHistory[p].m_vTargetCoords.y, m_PathHistory[p].m_vTargetCoords.z,
				m_PathHistory[p].m_bForGps ? "true" : "false",
				m_PathHistory[p].m_bPathFound ? "true" : "false",
				m_PathHistory[p].m_fTotalProcessingTime
			);

			vPos.x = ((float) iX) / ((float)grcDevice::GetWidth());
			vPos.y = ((float) iY) / ((float)grcDevice::GetWidth());

			Color32 col = Color_white;
			col.SetAlpha((int)(a*255.0f));
			grcDebugDraw::Text(vPos, col, txt);
		}

		iY += grcDebugDraw::GetScreenSpaceTextHeight()*2;
	}
}

#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////

void CPathFind::RenderDebugGroundHit(Vec3V_In vTestPos, float fGroundHeight) const
{
	Vec3V vGroundPos(vTestPos);
	vGroundPos.SetZf(fGroundHeight);
	grcDebugDraw::Cross(vGroundPos,0.25f,Color_cornsilk);
}

void CPathFind::RenderDebugOffsetError(Vec3V_In vTestPos, float fGroundHeight) const
{
	Vec3V vGroundPos(vTestPos);
	vGroundPos.SetZf(fGroundHeight);
	float fError = vTestPos.GetZf() - fGroundHeight;
	if(fabsf(fError)>fDisplayPathsDebug_AlignmentErrorPointOffsetThreshold)
	{
		const Vec3V vDrawPos = (fError > 0.0f) ? Average(vTestPos,vGroundPos) : vGroundPos;
		grcDebugDraw::Sphere(vDrawPos,fabsf(fError),fError>0.0f?Color_red:Color_purple,false);
	}
}

void CPathFind::RenderDebugShowAlignmentErrorsOnSegment(Vec3V_In vNodeA, Vec3V_In vNodeB, Vec3V_In vLinkSide, float fFalloffWidth, float fFalloffHeight) const
{
	const float fProbeDist = 3.0f;

	bool bAllAbove = true;
	bool bAllBelow = true;

	float fGroundHeight, fError;

	// Test vNodeA
	grcDebugDraw::Sphere(vNodeA,0.1f,Color_grey75);
	fGroundHeight = FindNearestGroundHeightForBuildPaths(RCC_VECTOR3(vNodeA),fProbeDist, NULL);
	RenderDebugGroundHit(vNodeA,fGroundHeight);
	fError = vNodeA.GetZf() - fGroundHeight;
	bAllAbove = bAllAbove && (fError > fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold);
	bAllBelow = bAllBelow && (fError < -fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold);
	RenderDebugOffsetError(vNodeA,fGroundHeight);

	// Test vNodeB
	grcDebugDraw::Sphere(vNodeB,0.1f,Color_grey75);
	fGroundHeight = FindNearestGroundHeightForBuildPaths(RCC_VECTOR3(vNodeB),fProbeDist, NULL);
	fError = vNodeB.GetZf() - fGroundHeight;
	bAllAbove = bAllAbove && (fError > fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold);
	bAllBelow = bAllBelow && (fError < -fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold);
	//RenderDebugOffsetError(vNodeB,fGroundHeight);

	// Test link center
	const Vec3V vLinkCenter = Average(vNodeA,vNodeB);
	grcDebugDraw::Sphere(vLinkCenter,0.1f,Color_grey50);
	fGroundHeight = FindNearestGroundHeightForBuildPaths(RCC_VECTOR3(vLinkCenter),fProbeDist, NULL);
	fError = vLinkCenter.GetZf() - fGroundHeight;
	bAllAbove = bAllAbove && (fError > fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold);
	bAllBelow = bAllBelow && (fError < -fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold);
	RenderDebugOffsetError(vLinkCenter,fGroundHeight);

	if(bAllAbove || bAllBelow)
	{
		grcDebugDraw::Line(vLinkCenter,vLinkCenter+Vec3V(0.0f,0.0f,5.0f),bAllAbove?Color_blue:Color_green);
		grcDebugDraw::Line(vNodeA,vNodeB,bAllAbove?Color_blue:Color_green);
	}
	else
	{
		grcDebugDraw::Line(vNodeA,vNodeB,Color_grey75);
	}

	// Test along the tilt
	const ScalarV fLinkSideBuffer(ROAD_EDGE_BUFFER_FOR_CAMBER_CALCULATION);
	const ScalarV fLinkSideDist = Dist(vLinkCenter,vLinkSide);
	const Vec3V vLinkSideMinusBuffer = Lerp((fLinkSideDist-fLinkSideBuffer)/fLinkSideDist,vLinkCenter,vLinkSide);
	const ScalarV fLinkSideMinusBufferDist = Dist(vLinkCenter,vLinkSideMinusBuffer);
	const ScalarV fFalloffHeightV(fFalloffHeight);
	const ScalarV fFalloffWidthV(fFalloffWidth);
	for(int i=1; i<=iDisplayPathsDebug_AlignmentErrorSideSamples; i++)
	{
		ScalarV fT = ScalarV(i / Max(1.0f,(float)iDisplayPathsDebug_AlignmentErrorSideSamples));
		Vec3V vTestPos = Lerp(fT,vLinkCenter,vLinkSideMinusBuffer);

		// Move vTestPos by falloff
		const ScalarV fDistFromEnd = (ScalarV(V_ONE) - fT) * fLinkSideMinusBufferDist;
		const ScalarV fFalloffDist = fFalloffHeightV * Max(ScalarV(V_ZERO),(fFalloffWidthV-fDistFromEnd)/fFalloffWidthV);
		vTestPos += fFalloffDist * Vec3V(V_Z_AXIS_WZERO);

		fGroundHeight = FindNearestGroundHeightForBuildPaths(RCC_VECTOR3(vTestPos),fProbeDist, NULL);
		RenderDebugGroundHit(vTestPos,fGroundHeight);
		RenderDebugOffsetError(vTestPos,fGroundHeight);

		//if(i==iDisplayPathsDebug_AlignmentErrorSideSamples)
		//{
		//	char pText[128];
		//	formatf(pText,"%0.3f %0.3f",fLinkSideDist.Getf(),fLinkSideMinusBufferDist.Getf());
		//	grcDebugDraw::Text(vTestPos,Color_grey80,0,0,pText);	
		//}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DisplayPathData
// PURPOSE :  Displays the path data currently in the pathfind structs.
//			  Simply draws lines between the nodes.
/////////////////////////////////////////////////////////////////////////////////
static Color32 nodeGroupColours[16] = {
Color32(255, 0, 0),		Color32(0, 255, 0),		Color32(0, 0, 255),		Color32(255, 255, 0),
		Color32(255, 0, 255), Color32(0, 255, 255), Color32(0, 0, 0), Color32(255, 255, 255),
		Color32(255, 0, 128), Color32(255, 128, 0), Color32(128, 255, 0), Color32(0, 255, 128),
		Color32(128, 0, 255), Color32(0, 128, 255), Color32(128, 255, 255), Color32(255, 128, 255) };

void  CPathFind::RenderDebug()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 camPos = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

#if __BANK
	if(bDisplayPathHistory)
	{
		DisplayPathHistory();
	}
	if(bDisplayRequiredNodeRegions)
	{
		DebugDrawNodesRequiredRegions();
	}
#endif

#if PATHSEARCHDRAW
	PathSearchRender();
#endif

	if(bDisplayRegionBoxes)
	{
		char tmp[64];
		for(s32 r=0; r<PATHFINDREGIONS; r++)
		{
			s32 iRY = r / PATHFINDMAPSPLIT;
			s32 iRX = r - (iRY * PATHFINDMAPSPLIT);
			Vector3 vRegionMin, vRegionMax;
			FindStartPointOfRegion(iRX, iRY, vRegionMin.x, vRegionMin.y);
			vRegionMin.z = 0.0f;
			vRegionMax.x = vRegionMin.x + PATHFINDREGIONSIZEX;
			vRegionMax.y = vRegionMin.y + PATHFINDREGIONSIZEX;
			vRegionMax.z = 500.0f;
			
			if(apRegions[FIND_REGION_INDEX(iRX, iRY)] && apRegions[FIND_REGION_INDEX(iRX, iRY)]->aNodes)
				grcDebugDraw::BoxAxisAligned(RCC_VEC3V(vRegionMin), RCC_VEC3V(vRegionMax), Color_green, false);
			else
				grcDebugDraw::BoxAxisAligned(RCC_VEC3V(vRegionMin), RCC_VEC3V(vRegionMax), Color_cyan, false);

			sprintf(tmp, "Region %i (%i, %i)", r, iRX, iRY);
			Vector3 vTextPos = (vRegionMin+vRegionMax)*0.5f;
			//vTextPos.z = camPos.z + 50.0f;

			grcDebugDraw::Text( vTextPos , Color_blue, 0, 0, tmp);

			if(apRegions[r])
			{
				sprintf(tmp, "NumNodes %i, NumLinks %i", apRegions[r]->NumNodes, apRegions[r]->NumLinks);
				grcDebugDraw::Text( vTextPos, Color_blue, 0, grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
			}
		}
	}

	char debugText[256];

	if(bDisplayMultipliers)
	{
		//-------------------------------------------------------------------------------
		// Display multipliers for Les
		// Also lets display what spacing values that equates to for density 1 and 15

		Vector2 vPixelSize( 1.0f / ((float)grcDevice::GetWidth()), 1.0f / ((float)grcDevice::GetHeight()) );	
		Vector2 vTextPos( vPixelSize.x * 56.0f, 1.0f );
		vTextPos.y -= (vPixelSize.y * ((float)grcDebugDraw::GetScreenSpaceTextHeight())) * 15.0f;

		int iVehiclesUpperLimit;
		float fDesertedStreetsMult, fInteriorMult, fWantedMult, fCombatMult, fHighwayMult, fBudgetMult;
		float fDesiredNumAmbientVehicles = CVehiclePopulation::GetDesiredNumberAmbientVehicles(&iVehiclesUpperLimit, &fDesertedStreetsMult, &fInteriorMult, &fWantedMult, &fCombatMult, &fHighwayMult, &fBudgetMult);

		s32	TotalAmbientCarsOnMap = CVehiclePopulation::ms_numRandomCars;

		s32 iNumberWreckedEmptyLocal = 0;
		s32 iNumberWreckedEmptyClone = 0;
		s32 iNumberDrivingLocal = 0;
		s32 iNumberDrivingClone = 0;
		s32 iNumberParkedLocal = 0;
		s32 iNumberParkedClone = 0;
		s32 iNumberScriptClone = 0;
		s32 iNumberScriptLocal = 0;
		s32 iNumberMiscClone = 0;
		s32 iNumberMiscLocal = 0;
		s32 iNumberNetRegistered = 0;
		s32 iNumberBeingDeleted = 0;
		s32 i = (s32) CVehicle::GetPool()->GetSize();
		while(i--)
		{
			CVehicle * pVeh = CVehicle::GetPool()->GetSlot(i);
			if (pVeh && !pVeh->GetIsInReusePool())
			{
				if(pVeh->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
				{
					iNumberBeingDeleted++;
				}

				if(pVeh->GetNetworkObject())
				{
					iNumberNetRegistered++;
				}

				switch(pVeh->PopTypeGet())
				{
				case POPTYPE_UNKNOWN:
					Assertf(false, "The vehicle should have a valid pop type.");
					grcDebugDraw::Line(pVeh->GetTransform().GetPosition(), CGameWorld::FindLocalPlayer()->GetTransform().GetPosition(), Color_green);
					break;
				case POPTYPE_PERMANENT:
				case POPTYPE_REPLAY:
				case POPTYPE_CACHE:
				case POPTYPE_TOOL:
					if( pVeh->IsNetworkClone() )
					{
						++iNumberMiscClone;
					}
					else
					{
						++iNumberMiscLocal;
					}
					break;
				case POPTYPE_RANDOM_PARKED :
					if( pVeh->IsNetworkClone() )
					{
						++iNumberParkedClone;
					}
					else
					{
						++iNumberParkedLocal;
					}

					break;
				case POPTYPE_MISSION :
					if( pVeh->IsNetworkClone() )
					{
						++iNumberScriptClone;
					}
					else
					{
						++iNumberScriptLocal;
					}
					break;
				case POPTYPE_RANDOM_PATROL : // Purposeful fall through.
				case POPTYPE_RANDOM_SCENARIO : // Purposeful fall through.
				case POPTYPE_RANDOM_AMBIENT :
					{
						bool bEmptyPopCar = false;
						if( CNetwork::IsGameInProgress() && !pVeh->IsUsingPretendOccupants() && 
							((NetworkInterface::GetSyncedTimeInMilliseconds() - pVeh->m_LastTimeWeHadADriver) > 5000 ) && (!pVeh->GetLastDriver() || !pVeh->GetLastDriver()->IsAPlayerPed() ) )
						{
							bEmptyPopCar = true;
						}

						const bool bWrecked = pVeh->GetStatus() == STATUS_WRECKED || pVeh->m_nVehicleFlags.bIsDrowning;

						const bool bEmptyOrWrecked = !pVeh->PopTypeIsMission() && (pVeh->PopTypeGet() != POPTYPE_RANDOM_PARKED || bWrecked) && (pVeh->IsLawEnforcementVehicle() || bEmptyPopCar || bWrecked || pVeh->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs) &&
							(!pVeh->HasAlivePedsInIt() || pVeh->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt) && !pVeh->HasMissionCharsInIt();

						if( bEmptyOrWrecked )
						{
							if( pVeh->IsNetworkClone() )
							{
								++iNumberWreckedEmptyClone;
							}
							else
							{
								++iNumberWreckedEmptyLocal;
							}
						}
						else
						{
							if( pVeh->IsNetworkClone() )
							{
								++iNumberDrivingClone;
							}
							else
							{
								++iNumberDrivingLocal;
							}
						}

						Assert(pVeh->m_CreationPopType == pVeh->PopTypeGet());
					}
					break;				
				default:
					break;
				}
			}
		}
		float fPopAreaMult = CThePopMultiplierAreas::GetTrafficDensityMultiplier(camPos);
		char popMultTxt[64] = { ' ', 0 };
		if(fPopAreaMult != 1.0f)
		{
			sprintf(popMultTxt, "INSIDE POP-MULT AREA (x%.1f)", fPopAreaMult);
		}

		if(CVehiclePopulation::ms_displayInnerKeyholeDisparities)
		{
			sprintf(debugText, "MAX AMBIENT VEHICLES = %.1f / %i (Current: %i, Reuse: %i, Disparities: %i, Inner Keyhole Disparities: %i) %s", fDesiredNumAmbientVehicles, iVehiclesUpperLimit, TotalAmbientCarsOnMap, CVehiclePopulation::ms_numCarsInReusePool, CVehiclePopulation::GetLinkDisparity(), CVehiclePopulation::ms_innerKeyholeDisparities, popMultTxt);
		}
		else
		{
			sprintf(debugText, "MAX AMBIENT VEHICLES = %.1f / %i (Current: %i Reuse: %i Net: %i Deleted: %i l(w: %i, p: %i, d: %i s: %i m: %i) c(w: %i, p: %i, d: %i s: %i m %i) (Disparities: %i) (Clone Fails: %d Mult: %.2f Dist: %.2f) %s", 
				fDesiredNumAmbientVehicles, iVehiclesUpperLimit, TotalAmbientCarsOnMap, CVehiclePopulation::ms_numCarsInReusePool, iNumberNetRegistered, iNumberBeingDeleted, iNumberWreckedEmptyLocal, iNumberParkedLocal, iNumberDrivingLocal, iNumberScriptLocal, iNumberMiscLocal, iNumberWreckedEmptyClone, iNumberParkedClone, iNumberDrivingClone, iNumberScriptClone, iNumberMiscClone, CVehiclePopulation::GetLinkDisparity(), CVehiclePopulation::ms_NetworkVisibilityFailCount, CVehiclePopulation::ms_CloneSpawnValidationMultiplier, CVehiclePopulation::GetVisibleCloneCreationDistance(), popMultTxt);
		}
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(debugText, "  MULTIPLIERS Total = %.1f ( min(DesertedStreets: %.1f, Wanted: %.1f), Interior: %.1f, Combat: %.1f, Highway: %.1f, VehicleBudget: %.1f)",
			Min(fDesertedStreetsMult, fWantedMult) * fInteriorMult * fCombatMult * fHighwayMult * fBudgetMult,
			fDesertedStreetsMult, fWantedMult, fInteriorMult, fCombatMult, fHighwayMult, fBudgetMult);
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(debugText, "VEHICLE SPACING (Density 15 = one car every %.1f meters, Density 1 = one car every %.1f meters)", CVehiclePopulation::GetCurrentVehicleSpacing(15), CVehiclePopulation::GetCurrentVehicleSpacing(1));
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		float fPopCycleMult, fDensityMult, fRangeScaleMult, fWantedSpacingsMult;
		float fTotalMultiplier = CVehiclePopulation::GetVehicleSpacingsMultiplier(CVehiclePopulation::GetPopulationRangeScale(), &fPopCycleMult, &fDensityMult, &fRangeScaleMult, &fWantedSpacingsMult);
		sprintf(debugText, "  MULTIPLIERS Total = %.1f ( min(PopCycle: %.1f, Wanted: %.1f), VehicleDensity: %.1f, )", fTotalMultiplier, fPopCycleMult, fWantedSpacingsMult, fDensityMult);
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		float fBasicMult, fScriptedMult, fMultiplayerMult;
		float fVehDensityMult = CVehiclePopulation::GetRandomVehDensityMultiplier(&fBasicMult, &fScriptedMult, &fMultiplayerMult);
		sprintf(debugText, "    VehicleDensity = %.1f (BasicMult: %.1f, ScriptedMult: %.1f, MultiplayerMult: %.1f, DebugMult: %.1f)", fVehDensityMult, fBasicMult, fScriptedMult, fMultiplayerMult, CVehiclePopulation::GetDebugOverrideMultiplier() );
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(debugText, "RANGE SCALE = %.2f (Base: %.1f, PosInfo: %.1f, Camera: %.1f, VehType: %.1f, Script: %.1f), VIEW RANGE SCALE = %.1f (HeightScale: %.1f, Renderer: %.1f, ScopeModifier: %.1f), NearDist: %.1f, AverageLinkHeight: %.1f",
			CVehiclePopulation::GetPopulationRangeScale(),
			CVehiclePopulation::ms_RangeScale_fBaseRangeScale,
			CVehiclePopulation::ms_RangeScale_fScalingFromPosInfo,
			CVehiclePopulation::ms_RangeScale_fscalingFromCameraOrientation,
			CVehiclePopulation::ms_RangeScale_fScalingFromVehicleType,
			CVehiclePopulation::ms_RangeScale_fScriptedRangeMultiplier,
			CVehiclePopulation::GetViewRangeScale(),
			CVehiclePopulation::ms_RangeScale_fHeightRangeScale,
			CVehiclePopulation::ms_RangeScale_fRendererRangeMultiplier,
			CVehiclePopulation::ms_RangeScale_fScopeModifier,
			CVehiclePopulation::GetPopulationSpawnFrustumNearDist(),
			CVehiclePopulation::GetAverageLinkHeight());

		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);


		sprintf(debugText, "FOCUS_POP_POS (%.1f,%.1f,%.1f) HEIST_ISLAND:%s LOD_RANGE_SCALE:%.1f  SCRIPTED_CONVERSION_CENTER: %s (%.1f,%.1f,%.1f)",
			CFocusEntityMgr::GetMgr().GetPopPos().x, CFocusEntityMgr::GetMgr().GetPopPos().y, CFocusEntityMgr::GetMgr().GetPopPos().z,
			bStreamHeistIslandNodes ? "true" : "false",
			CVehicleAILodManager::GetLodRangeScale(),
			CPedPopulation::GetIsScriptedConversionCenterActive() ? "true":"false",
			CPedPopulation::GetScriptedConversionCenter().x, CPedPopulation::GetScriptedConversionCenter().y, CPedPopulation::GetScriptedConversionCenter().z
		);

		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);


#if __BANK
		if(CPedPopulation::ms_displayPopControlSphereDebugThisFrame) // can't use ms_useTempPopControlSphereThisFrame because it's reset before this debug gets to run
		{
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();			
			sprintf(debugText, "POPULATION SPHERE = Center (%.1f, %.1f, %.1f) Ped Radius: %.1f, Veh Radius: %.1f", CFocusEntityMgr::GetMgr().GetPopPos().GetX(), CFocusEntityMgr::GetMgr().GetPopPos().GetY(), CFocusEntityMgr::GetMgr().GetPopPos().GetZ(),
				CPedPopulation::ms_TempPopControlSphereRadius,
				CVehiclePopulation::ms_TempPopControlSphereRadius);

			grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		}
#endif

		// Display vehicle population failures
		char failString[1024];
		s32 iNumFails = CVehiclePopulation::GetFailString(failString);

		if(iNumFails)
		{
			vTextPos = Vector2( vPixelSize.x * 56.0f, 1.0f );
			vTextPos.y -= (vPixelSize.y * ((float)grcDebugDraw::GetScreenSpaceTextHeight())) * 17.0f;
#if __BANK
			sprintf(debugText, "POPULATION FAILURES Total = %i (%i)", iNumFails, CVehiclePopulation::GetNumExcessDisparateLinks());
#else
			sprintf(debugText, "POPULATION FAILURES Total = %i", iNumFails);
#endif
			grcDebugDraw::Text( vTextPos, Color_red3, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			vTextPos.x += vPixelSize.x * 16.0f;
			grcDebugDraw::Text( vTextPos, Color_red3, failString, true);
		}
	}

	if(bDisplayPathsDebug_Allow)
	{
		//----------------------------------------------------------

		// DEBUG!! -AC, Experimental road paths debugging...
		if(bDisplayPathsDebug_RoadMesh_AllowDebugDraw)
		{
			TUNE_GROUP_FLOAT(ROAD_MESH, pathMeshDebugHeightA, 1.0f, 0.0f, 100.0f, 0.1f);
			const Vector3 vertDebugOffsetA(0.0f,0.0f,pathMeshDebugHeightA);
			TUNE_GROUP_FLOAT(ROAD_MESH, pathMeshDebugHeightB, 1.0f, 0.0f, 100.0f, 0.1f);
			const Vector3 vertDebugOffsetB(0.0f,0.0f,pathMeshDebugHeightB);
			TUNE_GROUP_FLOAT(ROAD_MESH, pathMeshBaseAlpha, 0.5f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_BOOL(ROAD_MESH, doAddrTestA2, false);
			TUNE_GROUP_BOOL(ROAD_MESH, doMagTestA, false);
			TUNE_GROUP_BOOL(ROAD_MESH, doMagTestB, true);
			TUNE_GROUP_COLOR(ROAD_MESH, roadMeshColor0, Color32(1.0f, 0.5, 1.0f, 1.0f));
			TUNE_GROUP_COLOR(ROAD_MESH, roadMeshColor1, Color32(0.5, 1.0f, 0.5f, 1.0f));
			TUNE_GROUP_COLOR(ROAD_MESH, laneColor0, Color32(1.0f, 1.0, 0.5f, 1.0f));
			TUNE_GROUP_COLOR(ROAD_MESH, laneColor1, Color32(0.5, 0.5f, 1.0f, 1.0f));
			TUNE_GROUP_COLOR(ROAD_MESH, laneColor2, Color32(1.0, 1.0f, 1.0f, 1.0f));

			for(s32 region = 0; region < PATHFINDREGIONS; region++)
			{
				// Make sure this region is actually loaded in.
				if(!apRegions[region] || !apRegions[region]->aNodes){continue;}

				for(s32 nodeAIndex = 0; nodeAIndex < apRegions[region]->NumNodes; nodeAIndex++)
					{
					const CPathNode *pNodeA = &apRegions[region]->aNodes[nodeAIndex];
					Assert(pNodeA);

					Vector3 nodeAPos;
					pNodeA->GetCoors(nodeAPos);

					for(u32 nodeALinkNum = 0; nodeALinkNum < pNodeA->NumLinks(); nodeALinkNum++)
						{
						const CPathNodeLink* pLinkAB = &GetNodesLink(pNodeA, nodeALinkNum);
						Assert(pLinkAB);

						const CNodeAddress nodeBAddr = pLinkAB->m_OtherNode;
						if(!IsRegionLoaded(nodeBAddr))
							{
							continue;
							}

						const CPathNode* pNodeB = FindNodePointer(nodeBAddr);
						Assert(pNodeB);

						Vector3 nodeBPos;
						pNodeB->GetCoors(nodeBPos);

						const float nodeBToCameraDistFlat = rage::Sqrtf((nodeBPos.x - camPos.x)*(nodeBPos.x - camPos.x)+
							(nodeBPos.y - camPos.y)*(nodeBPos.y - camPos.y));
						if(nodeBToCameraDistFlat > fDebgDrawDistFlat)
							{
							continue;
							}
						const float nodeBDrawDistBasedAlpha = 1.0f -(rage::Max(0.0f,((nodeBToCameraDistFlat / fDebgDrawDistFlat)-(1.0f - fDebugAlphaFringePortion)))/ fDebugAlphaFringePortion);

						//const Vector3 linkABCentre =(nodeAPos + nodeBPos)* 0.5f;
						Vector3 linkABDelta = nodeBPos - nodeAPos;
						linkABDelta.Normalize();

						for(u32 nodeBLinkNum = 0; nodeBLinkNum < pNodeB->NumLinks(); nodeBLinkNum++)
						{
							const CPathNodeLink *pLinkBC = &GetNodesLink(pNodeB, nodeBLinkNum);
							const CNodeAddress nodeCAddr = pLinkBC->m_OtherNode;
							Assert(pNodeB->m_address != nodeCAddr);
							if(!IsRegionLoaded(nodeCAddr))
						{
								continue;
							}
							if(pNodeA->m_address == nodeCAddr)
							{
								continue;
							}
							if(doAddrTestA2 &&(pNodeA->m_address < nodeCAddr))
							{
								continue;
							}

							const CPathNode* pNodeC = FindNodePointer(nodeCAddr);
							Assert(pNodeC);

							Vector3	nodeBRoadLeft;
							Vector3 nodeBRoadRight;
							float nodeBWidthScalingFactor;
							IdentifyDrivableExtremes(pNodeA->m_address, pNodeB->m_address, pNodeC->m_address, pNodeB->m_2.m_qualifiesAsJunction, 0.0f, nodeBRoadLeft, nodeBRoadRight, nodeBWidthScalingFactor);
							if(doMagTestA &&((nodeBRoadLeft - nodeBRoadRight).Mag() < 0.1f))
							{
								continue;
							}

							Vector2 nodeBSide;// = Vector2(nodeBRoadLeft.y - nodeBRoadRight.y, nodeBRoadLeft.x - nodeBRoadRight.x);
							(nodeBRoadRight - nodeBPos).GetVector2XY(nodeBSide);
							//float nodeBSideMag = nodeBSide.Mag();
							nodeBSide.Normalize();

							Vector3 nodeCPos;
							pNodeC->GetCoors(nodeCPos);

							const float nodeCToCameraDistFlat = rage::Sqrtf((nodeCPos.x - camPos.x)*(nodeCPos.x - camPos.x)+
								(nodeCPos.y - camPos.y)*(nodeCPos.y - camPos.y));
							if(nodeCToCameraDistFlat > fDebgDrawDistFlat)
							{
								continue;
							}
							const float nodeCDrawDistBasedAlpha = 1.0f -(rage::Max(0.0f,((nodeCToCameraDistFlat / fDebgDrawDistFlat)-(1.0f - fDebugAlphaFringePortion)))/ fDebugAlphaFringePortion);

							for(u32 nodeCLinkNum = 0; nodeCLinkNum < pNodeC->NumLinks(); nodeCLinkNum++)
							{
								const CPathNodeLink* pLinkCD = &GetNodesLink(pNodeC, nodeCLinkNum);
								const CNodeAddress nodeDAddr = pLinkCD->m_OtherNode;
								Assert(pNodeC->m_address != nodeDAddr);
								if(!IsRegionLoaded(nodeDAddr))
							{	
									continue;
								}
								if(pNodeB->m_address == nodeDAddr)
								{
									continue;
								}

								const CPathNode* pNodeD = FindNodePointer(nodeDAddr);
								Assert(pNodeD);

								Vector3	nodeCRoadLeft;
								Vector3 nodeCRoadRight;
								float nodeCWidthScalingFactor;
								IdentifyDrivableExtremes(pNodeB->m_address, pNodeC->m_address, pNodeD->m_address, pNodeC->m_2.m_qualifiesAsJunction, 0.0f, nodeCRoadLeft, nodeCRoadRight, nodeCWidthScalingFactor);
								if(doMagTestB &&((nodeCRoadLeft - nodeCRoadRight).Mag() < 0.1f))
									{				
									continue;
								}

								// Calculate the 'Side' vector.
								Vector2 nodeCSide;// = Vector2(nodeCRoadLeft.y - nodeCRoadRight.y, nodeCRoadLeft.x - nodeCRoadRight.x);
								(nodeCRoadRight - nodeCPos).GetVector2XY(nodeCSide);
								//float nodeCSideMag = nodeCSide.Mag();
								nodeCSide.Normalize();

								for(u32 i = 0; i < pLinkBC->m_1.m_LanesToOtherNode; ++i)
										{
									const float laneABCentreOffset = GetLinksLaneCentreOffset(*pLinkAB, i);//(i * pLinkAB->GetLaneWidth())+(bMakeAllRoadsSingleTrack?0.0f:pLinkAB->InitialLaneCenterOffset());
									const float laneBCCentreOffset = GetLinksLaneCentreOffset(*pLinkBC, i);//(i * pLinkBC->GetLaneWidth())+(bMakeAllRoadsSingleTrack?0.0f:pLinkBC->InitialLaneCenterOffset());
									const float laneCDCentreOffset = GetLinksLaneCentreOffset(*pLinkCD, i);//(i * pLinkCD->GetLaneWidth())+(bMakeAllRoadsSingleTrack?0.0f:pLinkCD->InitialLaneCenterOffset());
									const float nodeBLaneCentreOffset =(laneABCentreOffset + laneBCCentreOffset)* 0.5f * nodeBWidthScalingFactor;
									const float nodeCLaneCentreOffset =(laneBCCentreOffset + laneCDCentreOffset)* 0.5f * nodeCWidthScalingFactor;
									if(nodeBLaneCentreOffset < 0.0f)
										{
										int foo = 0;
										++foo;
									}

									if(nodeCLaneCentreOffset < 0.0f)
									{
										int foo = 0;
										++foo;
									}

									const Vector3 nodeBLaneCentreOffsetVec((nodeBSide.x * nodeBLaneCentreOffset),(nodeBSide.y * nodeBLaneCentreOffset), 0.0f);
									const Vector3 nodeCLaneCentreOffsetVec((nodeCSide.x * nodeCLaneCentreOffset),(nodeCSide.y * nodeCLaneCentreOffset), 0.0f);

											static Vector3 heightOffsetForViewing(0.0f,0.0f,1.0f);
									const Vector3 nodeBLanePos	= nodeBPos + nodeBLaneCentreOffsetVec + heightOffsetForViewing;
									const Vector3 nodeCLanePos	= nodeCPos + nodeCLaneCentreOffsetVec + heightOffsetForViewing;


									Color32 laneCol = laneColor0;
									if(pNodeB->m_address < pNodeC->m_address)
									{
										laneCol = laneColor1;
									}
									if(bMakeAllRoadsSingleTrack ||(pLinkBC->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL))
											{
										laneCol = laneColor2;
											}

									grcDebugDraw::Line(nodeBLanePos, nodeCLanePos, laneCol);
										}

								Color32 roadMeshColor = roadMeshColor0;
								if(pNodeB->m_address < pNodeC->m_address)
								{
									roadMeshColor = roadMeshColor1;
									}
								const Color32 col0B(roadMeshColor.GetRedf(), roadMeshColor.GetGreenf(), roadMeshColor.GetBluef(), nodeBDrawDistBasedAlpha * pathMeshBaseAlpha);
								const Color32 col0C(roadMeshColor.GetRedf(), roadMeshColor.GetGreenf(), roadMeshColor.GetBluef(), nodeCDrawDistBasedAlpha * pathMeshBaseAlpha);
								const Color32 col1B(roadMeshColor.GetRedf()* 0.7f, roadMeshColor.GetGreenf()* 0.7f, roadMeshColor.GetBluef()* 0.7f, nodeBDrawDistBasedAlpha * pathMeshBaseAlpha);
								const Color32 col1C(roadMeshColor.GetRedf()* 0.7f, roadMeshColor.GetGreenf()* 0.7f, roadMeshColor.GetBluef()* 0.7f, nodeCDrawDistBasedAlpha * pathMeshBaseAlpha);

								if((bMakeAllRoadsSingleTrack?0.0f:pLinkBC->InitialLaneCenterOffset()) == 0.0f)
									{
									//if(pNodeB->m_address < pNodeC->m_address)
									if(pLinkBC->m_1.m_LanesToOtherNode)
										{
										grcDebugDraw::Poly(nodeBRoadLeft + vertDebugOffsetA, nodeBRoadRight + vertDebugOffsetA, nodeCPos + vertDebugOffsetA, col0B, col0B, col0C);
										grcDebugDraw::Poly(nodeBRoadLeft + vertDebugOffsetB, nodeCPos + vertDebugOffsetB, nodeCRoadLeft + vertDebugOffsetB, col1B, col1C, col1C);
										grcDebugDraw::Poly(nodeBRoadRight + vertDebugOffsetB, nodeCRoadRight + vertDebugOffsetB, nodeCPos + vertDebugOffsetB, col1B, col1C, col1C);
									}
										}
										else
										{
									Vector3 nodeCMiddle =(nodeCPos + nodeCRoadRight)* 0.5f;
									grcDebugDraw::Poly(nodeBPos + vertDebugOffsetA, nodeBRoadRight + vertDebugOffsetA, nodeCMiddle + vertDebugOffsetA, col0B, col0B, col0C);
									grcDebugDraw::Poly(nodeBPos + vertDebugOffsetB, nodeCMiddle + vertDebugOffsetB, nodeCPos + vertDebugOffsetB, col1B, col1C, col1C);
									grcDebugDraw::Poly(nodeBRoadRight + vertDebugOffsetB, nodeCRoadRight + vertDebugOffsetB, nodeCMiddle + vertDebugOffsetB, col1B, col1C, col1C);
								}
							}// END for(u32 nodeCLinkNum = 0; nodeCLinkNum < pNodeC->NumLinks(); nodeCLinkNum++)
						}// END for(u32 nodeBLinkNum = 0; nodeBLinkNum < pNodeB->NumLinks(); nodeBLinkNum++)
					}// END for(u32 nodeALinkNum = 0; nodeALinkNum < pNodeA->NumLinks(); nodeALinkNum++)
				}// END for(s32 nodeAIndex = 0; nodeAIndex < apRegions[region]->NumNodes; nodeAIndex++)
			}// END for(s32 region = 0; region < PATHFINDREGIONS; region++)
		}// END if(bDisplayPathsDebug_Links_sAsRoadMesh)
		// END DEBUG!!

		if(bDisplayPathsDebug_Nodes_AllowDebugDraw || bDisplayPathsDebug_Links_AllowDebugDraw || bDisplayPathsDebug_ShowAlignmentErrors)
		{
			for(s32 region = 0; region < PATHFINDREGIONS; region++)
			{
				// Make sure this region is actually loaded in.
				if(!apRegions[region] || !apRegions[region]->aNodes)
				{	
					continue;
				}

				for(s32 nodeAIndex = 0; nodeAIndex < apRegions[region]->NumNodes; nodeAIndex++)
				{
					// Only debug draw for nodes withing the desired range.
					const CPathNode *pNodeA = &apRegions[region]->aNodes[nodeAIndex];
					Vector3 nodeAPos;
					pNodeA->GetCoors(nodeAPos);
					float nodeAToCameraDistFlat = rage::Sqrtf((nodeAPos.x - camPos.x)*(nodeAPos.x - camPos.x)+
						(nodeAPos.y - camPos.y)*(nodeAPos.y - camPos.y));
					if(nodeAToCameraDistFlat > fDebgDrawDistFlat)
					{
						continue;
					}

					// Ignore some types of nodes (from widget or in particular modes)
					bool bIgnorePedCrossing = bDisplayPathsDebug_IgnoreSpecial_PedCrossing || bDisplayPathsDebug_ShowAlignmentErrors;
					if(bIgnorePedCrossing && pNodeA->m_1.m_specialFunction == SPECIAL_PED_CROSSING)
					{
						continue;
					}
					bool bIgnoreNonPedCrossing = bDisplayPathsDebug_OnlySpecial_PedCrossing && !bDisplayPathsDebug_IgnoreSpecial_PedCrossing;
					if( bIgnoreNonPedCrossing && pNodeA->m_1.m_specialFunction != SPECIAL_PED_CROSSING )
					{
						continue;
					}
					bool bIgnorePedDriveWayCrossing = bDisplayPathsDebug_IgnoreSpecial_PedDriveWayCrossing || bDisplayPathsDebug_ShowAlignmentErrors;
					if(bIgnorePedDriveWayCrossing && pNodeA->m_1.m_specialFunction == SPECIAL_PED_DRIVEWAY_CROSSING)
					{
						continue;
					}
					bool bIgnoreNonPedDriveWayCrossing = bDisplayPathsDebug_OnlySpecial_PedDriveWayCrossing && !bDisplayPathsDebug_IgnoreSpecial_PedDriveWayCrossing;
					if( bIgnoreNonPedDriveWayCrossing && pNodeA->m_1.m_specialFunction != SPECIAL_PED_DRIVEWAY_CROSSING )
					{
						continue;
					}
					bool bIgnorePedAssistedMovement = bDisplayPathsDebug_IgnoreSpecial_PedAssistedMovement || bDisplayPathsDebug_ShowAlignmentErrors;
					if(bIgnorePedAssistedMovement && pNodeA->m_1.m_specialFunction == SPECIAL_PED_ASSISTED_MOVEMENT)
					{
						continue;
					}

					//float nodeADrawDistBasedAlpha = 1.0f -(rage::Max(0.0f,((nodeAToCameraDistFlat / fDebgDrawDistFlat)-(1.0f - alphaFringePortion)))/ alphaFringePortion);

					if(bDisplayPathsDebug_Nodes_AllowDebugDraw)
					{
						if(bDisplayPathsDebug_Nodes_StandardInfo)
						{
							if(pNodeA->m_2.m_switchedOff)
							{
								grcDebugDraw::Line( nodeAPos + Vector3(0.0f,0.0f,6.1f),
									nodeAPos + Vector3(0.0f,0.0f,6.5f),
									Color32(0, 0, 255));
							}

							if(pNodeA->m_2.m_switchedOffOriginal)
							{
								grcDebugDraw::Line( nodeAPos + Vector3(0.0f,0.0f,6.6f),
									nodeAPos + Vector3(0.0f,0.0f,7.0f),
									Color32(0, 0, 0));
							}

							if(pNodeA->m_2.m_deadEndness)
							{
								sprintf(debugText, "DeadEnd:%d", pNodeA->m_2.m_deadEndness);
								grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,4.5f), CRGBA(255, 255, 0, 255), debugText);
							}

							if(pNodeA->m_1.m_onPlayersRoad)
							{
								grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,5.5f), CRGBA(0, 128, 255, 255), "PR");
							}

							// Print a number for the Special function and speed.
							if(HasNodeBeenUsedRecently(pNodeA->m_address))
							{
								sprintf(debugText, "SpF:%d Spd:%d(Used)", pNodeA->m_1.m_specialFunction, pNodeA->m_2.m_speed);
							}
							else
							{
								sprintf(debugText, "SpF:%d Spd:%d", pNodeA->m_1.m_specialFunction, pNodeA->m_2.m_speed);
							}
							grcDebugDraw::Text(nodeAPos, CRGBA(255, 0, 255, 255), debugText);

							// Print a number for the density
							//if(pNodeA->m_2.m_density != 15)
							{
								sprintf(debugText, "Density:%d", pNodeA->m_2.m_density);	
								grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,0.6f), CRGBA(255, 255, 0, 255), debugText);
							}

							// On/off
							Color32 colour = pNodeA->m_2.m_switchedOff?Color_red:Color_green;
							sprintf(debugText, "%s (%s)", pNodeA->m_2.m_switchedOff?"off":"on", pNodeA->m_2.m_switchedOffOriginal?"off":"on");
							grcDebugDraw::Text(nodeAPos+ Vector3(0.f,0.f,1.2f), colour, debugText);

							debugText[0] = 0;
							if(pNodeA->m_2.m_highwayOrLowBridge)
							{
								if(pNodeA->m_2.m_waterNode)
								{
									sprintf(debugText, "%s LowBridge", debugText );
								}
								else
								{
									sprintf(debugText, "%s Highway", debugText );
								}
							}

							if(pNodeA->m_2.m_inTunnel)
							{
								sprintf(debugText, "%s Tunnel", debugText );
							}

							if(pNodeA->m_2.m_qualifiesAsJunction)
							{
								sprintf(debugText, "%s J", debugText );
							}
							else if (pNodeA->m_2.m_slipJunction)
							{	
								sprintf(debugText, "%s SlipJ", debugText );
							}

							if (pNodeA->m_1.m_slipLane)
							{	
								sprintf(debugText, "%s Slip-Node", debugText );
							}

							if (pNodeA->m_2.m_leftOnly)
							{
								sprintf(debugText, "%s LeftOnly", debugText);
							}
							if (pNodeA->m_1.m_cannotGoLeft)
							{
								sprintf(debugText, "%s NoLeft", debugText);
							}
							if (pNodeA->m_1.m_cannotGoRight)
							{
								sprintf(debugText, "%s NoRight", debugText);
							}
							if (pNodeA->m_1.m_noBigVehicles)
							{
								sprintf(debugText, "%s NoBigVehs", debugText);
							}
							if (pNodeA->m_1.m_Offroad)
							{
								sprintf(debugText, "%s Offroad", debugText);
							}
							if (pNodeA->m_1.m_indicateKeepLeft)
							{
								sprintf(debugText, "%s IndicateKeepLeft", debugText);
							}
							if (pNodeA->m_1.m_indicateKeepRight)
							{
								sprintf(debugText, "%s IndicateKeepRight", debugText);
							}

							if(debugText[0])
							{
								grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,2.2f), CRGBA(50, 250, 255, 255), debugText);
							}

							// Print the node address. region:Node
							sprintf(debugText, "%d:%d", pNodeA->GetAddrRegion(), pNodeA->GetAddrIndex());
							grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,2.0f), CRGBA(255, 255, 255, 255), debugText);

							// Print the node address. region:Node
							if(pNodeA->m_2.m_noGps)
							{
								if (ThePaths.bIgnoreNoGpsFlag)
									grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,1.0f), CRGBA(50, 255, 50, 255), "No Gps (Ignored)");
								else
									grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,1.0f), CRGBA(255, 50, 50, 255), "No Gps");
							}
						}
					
						// Print a text string for the name of the street
						if(bDisplayPathsDebug_Nodes_StreetNames)
						{
							if(pNodeA->m_streetNameHash)
							{
								sprintf(debugText, "Street:%s (0x%u)", TheText.Get( pNodeA->m_streetNameHash, "StreetName" ), pNodeA->m_streetNameHash);
							}
							else
							{
								sprintf(debugText, "Street: No name (0x%u)", pNodeA->m_streetNameHash);
							}
							grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,1.0f), CRGBA(50, 50, 255, 255), debugText);
						}

						if(bDisplayPathsDebug_Nodes_DistanceHash)
						{
							sprintf(debugText, "DistHash:%d", pNodeA->m_2.m_distanceHash );
							grcDebugDraw::Text(nodeAPos + Vector3(0.f,0.f,1.0f), CRGBA(50, 50, 255, 255), debugText);
						}

						if(bDisplayPathsDebug_Nodes_Pilons)
						{
							if(!pNodeA->m_2.m_waterNode)
							{
								grcDebugDraw::Line( nodeAPos + Vector3(0.0f,0.0f,6.0f),
									nodeAPos,
									nodeGroupColours[(pNodeA->m_1.m_group % 8)],
									nodeGroupColours[(pNodeA->m_1.m_group / 8)% 8] );
							}
							else
							{
								Color32 Colour1(0, 0, nodeGroupColours[(pNodeA->m_1.m_group % 8)].GetBlue(), 255);
								Color32 Colour2(0, 0, nodeGroupColours[(pNodeA->m_1.m_group / 8)% 8].GetBlue(), 255);
								grcDebugDraw::Line( nodeAPos + Vector3(0.0f,0.0f,6.0f),
									nodeAPos,
									Colour1, Colour2);
							}
						}

						// Calculate the difference in height between the ground and the node.
						if(bDisplayPathsDebug_Nodes_ToCollisionDiff)
						{
							if (!pNodeA->HasSpecialFunction())
							{
								Vector3 nodeAPos;
								pNodeA->GetCoors(nodeAPos);

								DisplayPathsDifferenceDebugForPoint(nodeAPos, true);

								// Now take samples on the link to neighbouring nodes.
								for(u32 n = 0; n < pNodeA->NumLinks(); n++)
								{
									CNodeAddress nodeBAddr = GetNodesLinkedNodeAddr(pNodeA, n);
									if(IsRegionLoaded(nodeBAddr))
										{
										CPathNode *pNodeB = &apRegions[nodeBAddr.GetRegion()]->aNodes[nodeBAddr.GetIndex()];

										if(pNodeA->m_address < pNodeB->m_address)		// Only do each line segment once
										{
											Vector3 nodeBPos;
											pNodeB->GetCoors(nodeBPos);

											float distABFlat =(nodeBPos - nodeAPos).XYMag();
	#define SAMPLEDIST	(5.0f)
											s32 numSamples =(s32)(distABFlat / SAMPLEDIST);
											for(s32 s = 0; s < numSamples; s++)
											{
												Vector3	sampleCoors = nodeAPos +(nodeBPos - nodeAPos)*(float(s+1)/ float(numSamples+1));

												DisplayPathsDifferenceDebugForPoint(sampleCoors, false);
											}
										}
									}
								}
							}
						}

						// Also show a representation for the traffic lights.

						if(bDisplayPathsDebug_Links_DrawTrafficLigths)
						{
							// Only display traffic lights for nodes which are NOT templated-junction entrances
							if(pNodeA->IsTrafficLight())
							{
								if( CJunctions::GetJunctionTemplateContainingEntrance(nodeAPos) == NULL )
								{
									int pJunctionId = CJunctions::GetJunctionAtPosition(nodeAPos,625.0f);
									CJunction* pJunction = NULL;
									if(pJunctionId != -1)
									{
										pJunction = CJunctions::GetJunctionByIndex(pJunctionId);
									}

									bool pedPhaseOnLights = false;
									s32 cycleOffset = 0;
									float cycleScale = 1.0f;
									if (pJunction)
									{
										pedPhaseOnLights = pJunction->GetHasPedCrossingPhase();
										cycleOffset = pJunction->GetAutoJunctionCycleOffset();
										cycleScale = pJunction->GetAutoJunctionCycleScale();
									}

									s32 lightState;
									s32 iLightCycle = CTrafficLights::CalculateNodeLightCycle(pNodeA);
									switch(iLightCycle)
									{
										case TRAFFIC_LIGHT_CYCLE1:
											lightState = CTrafficLights::LightForCars1(cycleOffset, cycleScale, pedPhaseOnLights);
											break;
										case TRAFFIC_LIGHT_CYCLE2:
											lightState = CTrafficLights::LightForCars2(cycleOffset, cycleScale, pedPhaseOnLights);
											break;
										default:
											Assert(false);
											lightState = LIGHT_RED;
											break;
									}								

									Color32 col;
									switch(lightState)
									{
										case LIGHT_GREEN:
											col = Color32(0, 255, 0, 255);
											break;
										case LIGHT_AMBER:
											col = Color32(255, 128, 0, 255);
											break;
										case LIGHT_RED:
											col = Color32(255, 0, 0, 255);
											break;
										case LIGHT_OFF:
										default:
											col = Color32(0, 0, 0, 255);
											break;
									}

									Vector3 lightPos = nodeAPos;
									grcDebugDraw::Sphere(lightPos + Vector3(0.0f,0.0f,4.0f), 0.4f, col);
								}

								grcDebugDraw::Text( nodeAPos + Vector3(0.0f, 0.0f, 4.5f), Color_white, 0, 0, "(traffic-light)" );
							}

							else if(pNodeA->IsGiveWay())
							{
								Vector3 lightPos = nodeAPos;
								Color32 col = Color32(255, 0, 255, 255);
								grcDebugDraw::Sphere(lightPos + Vector3(0.0f,0.0f,4.0f), 0.4f, col);
							}
						}

						// Draw ped crossing reservation slot locations and/or grid
						if( bDisplayPathsDebug_PedCrossing_DrawReservationSlots || bDisplayPathsDebug_PedCrossing_DrawReservationGrid )
						{
							if(pNodeA->m_1.m_specialFunction == SPECIAL_PED_CROSSING)
							{
								Color32 drawColor = Color_black;

								Vector3 vNodeAPos = pNodeA->GetPos();
								grcDebugDraw::Circle(vNodeAPos + Vector3(0.0f,0.0f,0.1f), 0.4f, drawColor, XAXIS, YAXIS);

								// For each crossing link
								for(u32 iLink=0; iLink < pNodeA->NumLinks(); iLink++)
								{
									const CPathNodeLink* pLinkAB = &GetNodesLink(pNodeA, iLink);
									Assert(pLinkAB);
									CNodeAddress nodeBAddr = pLinkAB->m_OtherNode;
									if(!IsRegionLoaded(nodeBAddr))
									{
										continue; // skip if node B's region is not loaded
									}

									const CPathNode* pNodeB = FindNodePointer(nodeBAddr);
									Assert(pNodeB);
									Vector3 vNodeBPos = pNodeB->GetPos();
									float nodeBToCameraDistFlat = rage::Sqrtf((vNodeBPos.x - camPos.x)*(vNodeBPos.x - camPos.x)+(vNodeBPos.y - camPos.y)*(vNodeBPos.y - camPos.y));
									if(nodeBToCameraDistFlat > fDebgDrawDistFlat)
									{
										continue; // skip if node B is too far from camera
									}

									// draw the reservation slot position circles in various colors
									if( bDisplayPathsDebug_PedCrossing_DrawReservationSlots )
									{
										// Get reservation if any is active for NodeA to NodeB
										int reservationIndex = FindPedCrossReservationIndex(pNodeA->m_address,nodeBAddr);
										if( reservationIndex >= 0 )
										{
											for(int iSlot=0; iSlot < MAX_PED_CROSS_RESERVATION_SLOTS; iSlot++)
											{
												Vector3 vDrawPosition;
												ComputePositionForPedCrossReservation(pNodeA->m_address, nodeBAddr, iSlot, vDrawPosition);
												switch(iSlot)
												{
												case 0 :
													drawColor = Color_green;
													break;
												case 1 :
													drawColor = Color_blue;
													break;
												case 2 :
													drawColor = Color_yellow;
													break;
												default:
													drawColor = Color_red;
													break;
												}

												grcDebugDraw::Circle(vDrawPosition + Vector3(0.0f,0.0f,0.1f), 0.4f, drawColor, XAXIS, YAXIS);
											}
										}
									}

									// draw the reservation grid lines
									if( bDisplayPathsDebug_PedCrossing_DrawReservationGrid )
									{
										// set grid color
										drawColor = Color_black;

										// Get direction vector for this crossing
										Vector3 vCrossDir = vNodeBPos- vNodeAPos;
										vCrossDir.NormalizeFast();
										Vector3 vRightDir = vCrossDir;
										vRightDir.RotateZ(-90.0f * DtoR);// Rotate Z negative 90 gives right

										// Starting at the centroid offset
										Vector3 vCentroidOffsetPos = vNodeAPos + ((CPathFind::sm_fPedCrossReserveSlotsCentroidForwardOffset) * vCrossDir);
										// Lift the position up vertically to see the lines more easily
										vCentroidOffsetPos += Vector3(0.0f,0.0f,0.1f);

										// Compute the endpoints for the lines in the crossing direction and render them:
										const float fGridSize = CPathFind::sm_fPedCrossReserveSlotsPlacementGridSpacing;
										Vector3 vGridLinePosA;
										Vector3 vGridLinePosB;
										// left cell left line
										vGridLinePosA = vCentroidOffsetPos + ((-0.5f * fGridSize) * vCrossDir) + ((-1.5f * fGridSize) * vRightDir);
										vGridLinePosB = vCentroidOffsetPos + ((+0.5f * fGridSize) * vCrossDir) + ((-1.5f * fGridSize) * vRightDir);
										grcDebugDraw::Line(vGridLinePosA, vGridLinePosB, drawColor, drawColor);
										// left cell right line
										vGridLinePosA = vCentroidOffsetPos + ((-0.5f * fGridSize) * vCrossDir) + ((-0.5f * fGridSize) * vRightDir);
										vGridLinePosB = vCentroidOffsetPos + ((+0.5f * fGridSize) * vCrossDir) + ((-0.5f * fGridSize) * vRightDir);
										grcDebugDraw::Line(vGridLinePosA, vGridLinePosB, drawColor, drawColor);
										// right cell right line
										vGridLinePosA = vCentroidOffsetPos + ((-0.5f * fGridSize) * vCrossDir) + ((+1.5f * fGridSize) * vRightDir);
										vGridLinePosB = vCentroidOffsetPos + ((+0.5f * fGridSize) * vCrossDir) + ((+1.5f * fGridSize) * vRightDir);
										grcDebugDraw::Line(vGridLinePosA, vGridLinePosB, drawColor, drawColor);
										// right cell left line
										vGridLinePosA = vCentroidOffsetPos + ((-0.5f * fGridSize) * vCrossDir) + ((+0.5f * fGridSize) * vRightDir);
										vGridLinePosB = vCentroidOffsetPos + ((+0.5f * fGridSize) * vCrossDir) + ((+0.5f * fGridSize) * vRightDir);
										grcDebugDraw::Line(vGridLinePosA, vGridLinePosB, drawColor, drawColor);
										// front line across
										vGridLinePosA = vCentroidOffsetPos + ((+0.5f * fGridSize) * vCrossDir) + ((-1.5f * fGridSize) * vRightDir);
										vGridLinePosB = vCentroidOffsetPos + ((+0.5f * fGridSize) * vCrossDir) + ((+1.5f * fGridSize) * vRightDir);
										grcDebugDraw::Line(vGridLinePosA, vGridLinePosB, drawColor, drawColor);
										// rear line across
										vGridLinePosA = vCentroidOffsetPos + ((-0.5f * fGridSize) * vCrossDir) + ((-1.5f * fGridSize) * vRightDir);
										vGridLinePosB = vCentroidOffsetPos + ((-0.5f * fGridSize) * vCrossDir) + ((+1.5f * fGridSize) * vRightDir);
										grcDebugDraw::Line(vGridLinePosA, vGridLinePosB, drawColor, drawColor);
									}
								}
							}
						}

						// Draw ped crossing reservation status
						if( bDisplayPathsDebug_PedCrossing_DrawReservationStatus )
						{
							if(pNodeA->m_1.m_specialFunction == SPECIAL_PED_CROSSING)
							{
								// For each crossing link
								for(u32 iLink=0; iLink < pNodeA->NumLinks(); iLink++)
								{
									const CPathNodeLink* pLinkAB = &GetNodesLink(pNodeA, iLink);
									Assert(pLinkAB);
									CNodeAddress nodeBAddr = pLinkAB->m_OtherNode;
									if(!IsRegionLoaded(nodeBAddr))
									{
										continue; // skip if node B's region is not loaded
									}

									const CPathNode* pNodeB = FindNodePointer(nodeBAddr);
									Assert(pNodeB);
									Vector3 nodeBPos = pNodeB->GetPos();
									float nodeBToCameraDistFlat = rage::Sqrtf((nodeBPos.x - camPos.x)*(nodeBPos.x - camPos.x)+(nodeBPos.y - camPos.y)*(nodeBPos.y - camPos.y));
									if(nodeBToCameraDistFlat > fDebgDrawDistFlat)
									{
										continue; // skip if node B is too far from camera
									}
								
									// Get reservation if any is active for NodeA to NodeB
									int reservationIndex = FindPedCrossReservationIndex(pNodeA->m_address,nodeBAddr);
									if( reservationIndex >= 0 )
									{
										// traverse the slots
										for(int iSlot = 0; iSlot < MAX_PED_CROSS_RESERVATION_SLOTS; iSlot++)
										{
											// Get the draw position for this slot
											Vector3 vDrawPosition;
											ComputePositionForPedCrossReservation(pNodeA->m_address, nodeBAddr, iSlot, vDrawPosition);

											// if the reservation slot is in use, change draw color
											Color32 drawColor = Color_black;
											if( m_aPedCrossingReservations[reservationIndex].m_ReservationSlotStatus.IsFlagSet((1<<iSlot)) )
											{
												drawColor = Color_green;
											}
											
											grcDebugDraw::Circle(vDrawPosition + Vector3(0.0f,0.0f,0.3f), 0.4f, drawColor, XAXIS, YAXIS);
										}
									}
								}
							}
						}

					}// END if(bDisplayPathsDebug_Nodes_AllowDebugDraw)

					if(bDisplayPathsDebug_Links_AllowDebugDraw || bDisplayPathsDebug_ShowAlignmentErrors)
					{
						for(u32 nodeALinkNum = 0; nodeALinkNum < pNodeA->NumLinks(); nodeALinkNum++)
						{
							const CPathNodeLink* pLinkAB = &GetNodesLink(pNodeA, nodeALinkNum);
							Assert(pLinkAB);
							CNodeAddress nodeBAddr = pLinkAB->m_OtherNode;
							if(!IsRegionLoaded(nodeBAddr))
							{
								continue;
							}

							const CPathNode* pNodeB = FindNodePointer(nodeBAddr);
							Assert(pNodeB);
							Vector3 nodeBPos;
							pNodeB->GetCoors(nodeBPos);

							float nodeBToCameraDistFlat = rage::Sqrtf((nodeBPos.x - camPos.x)*(nodeBPos.x - camPos.x)+
								(nodeBPos.y - camPos.y)*(nodeBPos.y - camPos.y));
							if(nodeBToCameraDistFlat > fDebgDrawDistFlat)
							{
								continue;
							}

							Vector3 linkABCentre =(nodeAPos + nodeBPos)* 0.5f;
							Vector3 linkABDelta = nodeBPos - nodeAPos;
							linkABDelta.Normalize();

							if(bDisplayPathsDebug_Links_DrawLine && !bDisplayPathsDebug_ShowAlignmentErrors)
							{
								// Make sure to only do this once per node pair.
								if(pNodeA->m_address < pNodeB->m_address)
								{
									Color32 linkCol = Color32(160, 160, 160);
									if (pLinkAB->IsShortCut())
									{
										linkCol = Color32(200, 200, 60);
									}
									else if (pLinkAB->m_1.m_bDontUseForNavigation)
									{
										linkCol = Color32(60, 200, 200);
									}
									else if (pLinkAB->m_1.m_bBlockIfNoLanes)
									{
										linkCol = Color_OrangeRed;
									}
									else if (pLinkAB->m_1.m_NarrowRoad)
									{
										linkCol = Color_DarkOrchid;
									}
									grcDebugDraw::Line(	nodeAPos + Vector3(0.0f,0.0f,3.0f),
										nodeBPos + Vector3(0.0f,0.0f,3.0f),
										linkCol);
								}
							}

							if(bDisplayPathsDebug_Links_DrawLaneDirectionArrows && !bDisplayPathsDebug_ShowAlignmentErrors)
							{
								// Make sure to only do this once per node pair.
								if(pNodeA->m_address < pNodeB->m_address)
								{			
									// Show the lanes directions
									s32	displayedLaneDirArrowCount = 0;

									Color32 col(255, 255, 255);
									if (pLinkAB->LeadsToDeadEnd())
									{
										col = Color32(255, 0, 0);
									}
									for(u32 laneDirArrow = 0; laneDirArrow < pLinkAB->m_1.m_LanesToOtherNode; laneDirArrow++)
									{
										const Vector3 arrowVerticalOffset = Vector3(0.0f,0.0f,2.0f + displayedLaneDirArrowCount * 0.5f);
										const Vector3 arrowDelta = linkABDelta;
										const Vector3 arrowBase = linkABCentre + arrowVerticalOffset;
										const Vector3 arrowTip = linkABCentre + arrowDelta + arrowVerticalOffset;
										grcDebugDraw::Line(arrowBase, arrowTip, col);
										grcDebugDraw::Line((arrowBase +(0.8f * arrowDelta)+ Vector3(0.0f,0.0f,0.2f)), arrowTip, col);
										grcDebugDraw::Line((arrowBase +(0.8f * arrowDelta)+ Vector3(0.0f,0.0f,-0.2f)), arrowTip, col);

										displayedLaneDirArrowCount++;
									}

									col.Set(192, 192, 192);
									if (pLinkAB->LeadsFromDeadEnd())
									{
										col = Color32(192, 0, 0);
									}
									for(u32 laneDirArrow = 0; laneDirArrow < pLinkAB->m_1.m_LanesFromOtherNode; laneDirArrow++)
								{
										const Vector3 arrowVerticalOffset = Vector3(0.0f,0.0f,2.0f + displayedLaneDirArrowCount * 0.5f);
										const Vector3 arrowDelta = -linkABDelta;
										const Vector3 arrowBase = linkABCentre + arrowVerticalOffset;
										const Vector3 arrowTip = linkABCentre + arrowDelta + arrowVerticalOffset;
										grcDebugDraw::Line(arrowBase, arrowTip, col);
										grcDebugDraw::Line((arrowBase +(0.8f * arrowDelta)+ Vector3(0.0f,0.0f,0.2f)), arrowTip, col);
										grcDebugDraw::Line((arrowBase +(0.8f * arrowDelta)+ Vector3(0.0f,0.0f,-0.2f)), arrowTip, col);
										displayedLaneDirArrowCount++;
									}
								}
								}

							// DEBUG!! -AC, Experimental lane debugging...
							if(bDisplayPathsDebug_Links_LaneCenters)
							{
								// Calculate the 'Side' vector.
								Vector2 side = Vector2(nodeBPos.y - nodeAPos.y, nodeAPos.x - nodeBPos.x);
								side.Normalize();

								for(u32 i = 0; i < pLinkAB->m_1.m_LanesToOtherNode; ++i)
								{
									float laneCentreOffset = GetLinksLaneCentreOffset(*pLinkAB, i);
									Vector3 laneCentreOffsetVec((side.x * laneCentreOffset),(side.y * laneCentreOffset), 0.0f);

									static Vector3 heightOffsetForViewing(0.0f,0.0f,1.0f);
									Vector3 nodeLane		= nodeAPos + laneCentreOffsetVec + heightOffsetForViewing;
									Vector3 otherNodeLane	= nodeBPos + laneCentreOffsetVec + heightOffsetForViewing;

									Color32 col(255, 160, 160);
									if(bMakeAllRoadsSingleTrack ||(pLinkAB->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL))
									{
										col.Set(160, 255, 160);
									}

									grcDebugDraw::Line(nodeLane, otherNodeLane, col);
								}
							}
							// END DEBUG!!

							if(bDisplayPathsDebug_Links_RoadExtremes || bDisplayPathsDebug_ShowAlignmentErrors)
							{
								const float fNodeDiameter = 0.1f;
								const Vector3 vertDebugOffset(0.0f,0.0f,1.0f);

								// Make sure to only do this once per node pair.
								if(pNodeA->m_address < pNodeB->m_address)
								{
									if(pNodeB->NumLinks() == 0)
									{
										CNodeAddress emptyAddr;
										emptyAddr.SetEmpty();
										Vector3	roadExtremePointLeft;
										Vector3 roadExtremePointRight;
										float nodeAWidthScalingFactor;
										IdentifyDrivableExtremes(pNodeB->m_address, pNodeA->m_address, emptyAddr, pNodeA->m_2.m_qualifiesAsJunction, 0.0f, roadExtremePointLeft, roadExtremePointRight, nodeAWidthScalingFactor);

										if(!bDisplayPathsDebug_ShowAlignmentErrors)
										{
											grcDebugDraw::Line(roadExtremePointLeft + vertDebugOffset, roadExtremePointRight + vertDebugOffset, Color_white);
										}
										grcDebugDraw::Sphere(roadExtremePointLeft, fNodeDiameter, Color_grey75);
										grcDebugDraw::Sphere(roadExtremePointRight, fNodeDiameter, Color_grey75);
									}
									else
									{
										for(u32 nodeBLinkNum = 0; nodeBLinkNum < pNodeB->NumLinks(); nodeBLinkNum++)
										{	
											const CPathNodeLink *pLinkBC = &GetNodesLink(pNodeB, nodeBLinkNum);
											CNodeAddress nodeCAddr = pLinkBC->m_OtherNode;

											const CPathNode* pNodeC = NULL;
											if(IsRegionLoaded(nodeCAddr))
											{
												pNodeC = FindNodePointer(nodeCAddr);
											}
											if(!pNodeC)
											{
												break;
											}

											Vector3	roadExtremePointLeft;
											Vector3 roadExtremePointRight;
											float nodeBWidthScalingFactor;
											IdentifyDrivableExtremes(pNodeA->m_address, pNodeB->m_address, pNodeC->m_address, pNodeB->m_2.m_qualifiesAsJunction, 0.0f, roadExtremePointLeft, roadExtremePointRight, nodeBWidthScalingFactor);

											if(!bDisplayPathsDebug_ShowAlignmentErrors)
											{
												grcDebugDraw::Line(roadExtremePointLeft + vertDebugOffset, roadExtremePointRight + vertDebugOffset, Color_white);
											}
											grcDebugDraw::Sphere(roadExtremePointLeft, fNodeDiameter, Color_grey75);
											grcDebugDraw::Sphere(roadExtremePointRight, fNodeDiameter, Color_grey75);
										}
									}
								}
							}

							if(bDisplayPathsDebug_Links_TextInfo)
							{
								if(pLinkAB->m_1.m_NarrowRoad)
								{
									grcDebugDraw::Text(linkABCentre, CRGBA(255, 255, 0, 255), "Narrow Road");
								}

								if(pLinkAB->m_1.m_bGpsCanGoBothWays)
								{
									grcDebugDraw::Text(linkABCentre + Vector3(0.f,0.f,0.5f), CRGBA(255, 255, 0, 255), "Gps both ways");
								}

// 								if (pLinkAB->m_1.m_bDontUseForNavigation)
// 								{
// 									grcDebugDraw::Text(linkABCentre + Vector3(0.f,0.f,0.75f), CRGBA(255, 255, 0, 255), "Ignore for nav");
// 								}

								if(pLinkAB->LeadsToDeadEnd())
								{
									grcDebugDraw::Text(linkABCentre + Vector3(0.f,0.f,1.0f), CRGBA(255, 255, 0, 255), "Leads to dead end");
								}
								if(pLinkAB->LeadsFromDeadEnd())
								{
									grcDebugDraw::Text(linkABCentre + Vector3(0.f,0.f,1.5f), CRGBA(255, 255, 0, 255), "Leads from dead end");
								}
								if(pLinkAB->IsOneWay())
								{
									grcDebugDraw::Text(linkABCentre + Vector3(0.f,0.f,2.0f), CRGBA(255, 255, 0, 255), "One way");
								}
							}

							if(bDisplayPathsDebug_Links_RegionAndIndex)
							{
								if(pNodeA < pNodeB)
								{
									sprintf(debugText, "%i : %i", pNodeA->GetAddrRegion(), pNodeA->m_startIndexOfLinks + nodeALinkNum);
									grcDebugDraw::Text(linkABCentre + Vector3(0.0f,0.0f,1.0f), Color_magenta, debugText);

									u32 iDrawCount = 1;
									for (int bLinkIndex = 0; bLinkIndex < pNodeB->NumLinks(); bLinkIndex++)
									{
										const CPathNodeLink *pLinkBA = &GetNodesLink(pNodeB, bLinkIndex);
										if (pLinkBA && pLinkBA->m_OtherNode == pNodeA->m_address)
										{
											sprintf(debugText, "%i : %i", pNodeB->GetAddrRegion(), pNodeB->m_startIndexOfLinks + bLinkIndex);
											grcDebugDraw::Text(linkABCentre + Vector3(0.0f,0.0f,1.0f), Color_magenta4, 0, iDrawCount * 10, debugText);
											++iDrawCount;
										}
									}
								}
							}

							// Draw 2 lines to debug tilt: Horizontal and Tilt.
							if(bDisplayPathsDebug_Links_Tilt || bDisplayPathsDebug_ShowAlignmentErrors)
							{
								const Vector3 vOffset(0.0f,0.0f,bDisplayPathsDebug_ShowAlignmentErrors?0.0f:0.1f);
								float fRoadWidthLeft, fRoadWidthRight;
								const bool bAllLanesThoughCentre = (pLinkAB->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);

								FindRoadBoundaries(
									pLinkAB->m_1.m_LanesToOtherNode,
									pLinkAB->m_1.m_LanesFromOtherNode,
									static_cast<float>(pLinkAB->m_1.m_Width),
									pLinkAB->m_1.m_NarrowRoad,
									bAllLanesThoughCentre,
									fRoadWidthLeft, fRoadWidthRight);

								Vector3 vSideFlat = Vector3(linkABDelta.y, -linkABDelta.x, 0.0f);
								vSideFlat.Normalize();

								if(!bDisplayPathsDebug_ShowAlignmentErrors)
								{
									grcDebugDraw::Line(linkABCentre - vSideFlat + Vector3(0.0f, 0.0f, 1.0f), linkABCentre + vSideFlat + Vector3(0.0f, 0.0f, 1.0f), Color_grey70);
								}
								grcDebugDraw::Line(linkABCentre + vOffset, linkABCentre + vOffset + (vSideFlat*fRoadWidthRight), Color_grey70);

								float vUp = rage::Sinf( - saTiltValues[pLinkAB->m_1.m_Tilt]);
								float vSide = rage::Cosf( - saTiltValues[pLinkAB->m_1.m_Tilt]);

								Vector3 vLink = nodeBPos - nodeAPos;
								vLink.Normalize();
								const float fTiltAngle = - saTiltValues[pLinkAB->m_1.m_Tilt];

								Matrix34 tiltMat;
								tiltMat.Identity3x3();
								tiltMat.MakeRotateUnitAxis(vLink, fTiltAngle); // TODO: Yuck, 'saTiltValues' should be contained within CPathFind
								Vector3 vSideTilted(vSideFlat);
								tiltMat.Transform3x3(vSideTilted);

								if(!bDisplayPathsDebug_ShowAlignmentErrors)
								{
									grcDebugDraw::Line( linkABCentre -(vSide * vSideTilted)- Vector3(0.0f, 0.0f, vUp)+ Vector3(0.0f, 0.0f, 1.0f), linkABCentre + (vSide * vSideFlat) + Vector3(0.0f, 0.0f, vUp)+ Vector3(0.0f, 0.0f, 1.0f), Color_white);
								}
								Vector3 vExtremePointRightWithTilt = linkABCentre + vSideTilted * fRoadWidthRight;

								// Color the tilt based on direction to distinguish the two sides of tilt
								const Vector3 vDistinguisher(0.01f,0.01f,0.0f);
								const Color32 colTiltSideA = pLinkAB->m_1.m_TiltFalloff ? Color_red : Color_yellow;
								const Color32 colTiltSideB = pLinkAB->m_1.m_TiltFalloff ? Color_purple : Color_orange;
								const Color32 colTilt = vDistinguisher.Dot(vSideFlat) >= 0.0f ? colTiltSideA : colTiltSideB;
								grcDebugDraw::Line(linkABCentre + vOffset, vExtremePointRightWithTilt + vOffset, colTilt);

								if(pLinkAB->m_1.m_TiltFalloff)
								{
									const Vector3 vFalloffStart = linkABCentre + vSideTilted * (fRoadWidthRight - LINK_TILT_FALLOFF_WIDTH);
									const Vector3 vFalloffEnd = vExtremePointRightWithTilt - ZAXIS * LINK_TILT_FALLOFF_HEIGHT;
									grcDebugDraw::Line(vFalloffStart,vFalloffEnd,Color_blue);
								}

								if(bDisplayPathsDebug_ShowAlignmentErrors)
								{
									RenderDebugShowAlignmentErrorsOnSegment(RCC_VEC3V(nodeAPos),RCC_VEC3V(nodeBPos),RCC_VEC3V(vExtremePointRightWithTilt),LINK_TILT_FALLOFF_WIDTH,pLinkAB->m_1.m_TiltFalloff ? -LINK_TILT_FALLOFF_HEIGHT : 0.0f);
								}
							}
						}// END for all links off node
					}// END if(bDisplayPathsDebug_Links_s_AllowDebugDraw)
				}// END for all nodes in region
			}// END for all regions
		}// END if(bDisplayPathsDebug_Nodes_AllowDebugDraw || bDisplayPathsDebug_Links_s_AllowDebugDraw)
	}

	// We print some info here regarding the streaming of the paths.
	if (bDisplayPathStreamingDebug)
	{
		for (s32 X = 0; X < PATHFINDMAPSPLIT; X++)
		{
			for (s32 Y = 0; Y < PATHFINDMAPSPLIT; Y++)
			{
				u32 region = FIND_REGION_INDEX(X, Y);
				Color32	Colour;
				if(apRegions[region] && apRegions[region]->aNodes)
				{	
					// loaded
					Colour = Color32(255, 255, 255, 255);
				}
				else
				{
					if(IsObjectRequested(GetStreamingIndexForRegion(region)))
					{	
						// requested
						Colour = Color32(255, 0, 0, 255);
					}
					else
					{
						Colour = Color32(0, 0, 0, 255);
					}
				}				
				Vector2 leftCorner = Vector2(FindXCoorsForRegion(X) + 20.0f, FindXCoorsForRegion(Y) + 20.0f);
				Vector2 rightCorner = Vector2(FindXCoorsForRegion(X+1) - 20.0f, FindXCoorsForRegion(Y+1) - 20.0f);

				CVectorMap::DrawLine(Vector3(leftCorner.x, leftCorner.y, 0.0f), Vector3(rightCorner.x, leftCorner.y, 0.0f), Colour, Colour);
				CVectorMap::DrawLine(Vector3(rightCorner.x, rightCorner.y, 0.0f), Vector3(rightCorner.x, leftCorner.y, 0.0f), Colour, Colour);
				CVectorMap::DrawLine(Vector3(rightCorner.x, rightCorner.y, 0.0f), Vector3(leftCorner.x, rightCorner.y, 0.0f), Colour, Colour);
				CVectorMap::DrawLine(Vector3(leftCorner.x, leftCorner.y, 0.0f), Vector3(leftCorner.x, rightCorner.y, 0.0f), Colour, Colour);
			}
		}

		// Print the vehicles that force blocks to be loaded.
		CVehicle::Pool *VehPool = CVehicle::GetPool();
		CVehicle* pVehicle;
		s32 i = (s32) VehPool->GetSize();
		while(i--)
		{
			pVehicle = VehPool->GetSlot(i);
			if(pVehicle)
			{
				if(pVehicle == FindPlayerVehicle() || (pVehicle->PopTypeIsMission()
					&& pVehicle->GetVehicleType() != VEHICLE_TYPE_HELI
					&& pVehicle->GetVehicleType() != VEHICLE_TYPE_PLANE
					&& pVehicle->GetVehicleType() != VEHICLE_TYPE_BOAT
					&& pVehicle->GetVehicleType() != VEHICLE_TYPE_TRAIN
					&& pVehicle->GetVehicleType() != VEHICLE_TYPE_SUBMARINE
					&& pVehicle->GetVehicleType() != VEHICLE_TYPE_BLIMP))
				{
					const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
					Vector2 leftCorner = Vector2(vVehiclePosition.x - 300.0f, vVehiclePosition.y - 300.0f);
					Vector2 rightCorner = Vector2(vVehiclePosition.x + 300.0f, vVehiclePosition.y + 300.0f);

					CVectorMap::DrawLine(Vector3(leftCorner.x, leftCorner.y, 0.0f), Vector3(rightCorner.x, leftCorner.y, 0.0f), Color32(0, 0, 255), Color32(0, 0, 255));
					CVectorMap::DrawLine(Vector3(rightCorner.x, rightCorner.y, 0.0f), Vector3(rightCorner.x, leftCorner.y, 0.0f), Color32(0, 0, 255), Color32(0, 0, 255));
					CVectorMap::DrawLine(Vector3(rightCorner.x, rightCorner.y, 0.0f), Vector3(leftCorner.x, rightCorner.y, 0.0f), Color32(0, 0, 255), Color32(0, 0, 255));
					CVectorMap::DrawLine(Vector3(leftCorner.x, leftCorner.y, 0.0f), Vector3(leftCorner.x, rightCorner.y, 0.0f), Color32(0, 0, 255), Color32(0, 0, 255));
				}
			}
		}		
	}

	static bool bPreviousDisplayPathsOnVMap=false;
	static bool flashOffLinksThisFrame = true;
	flashOffLinksThisFrame = !flashOffLinksThisFrame;
	if(	(bDisplayPathsOnVMap || bDisplayPathsOnVMapDontForceLoad) ||
		(bDisplayPathsWithDensityOnVMap || bDisplayPathsWithDensityOnVMapDontForceLoad))
	{
		bool flashBadSlopesThisFrame = (fwTimer::GetSystemFrameCount() % 5) != 0;

		// Request for all paths to be streamed in
		if (bDisplayPathsOnVMap)
		{
			MakeRequestForNodesToBeLoaded(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_YMAX, NODE_REQUEST_SCRIPT);
		}

		// Go through all possible links and render them in the colour of the group
		for(s32 region = 0; region < PATHFINDREGIONS; region++)
		{
			if(!apRegions[region] || !apRegions[region]->aNodes)		// Make sure this region is actually loaded in.
			{
				continue;
			}

			for(s32 Nodes = 0; Nodes < apRegions[region]->NumNodesCarNodes; Nodes++)			// Only car nodes
			{
				CPathNode *pNode = &apRegions[region]->aNodes[Nodes];

				for(u32 link = 0; link < pNode->NumLinks(); link++)
				{
					CNodeAddress neighbourNodeAddr = GetNodesLinkedNodeAddr(pNode, link);
					if(!IsRegionLoaded(neighbourNodeAddr))
					{
						continue;
					}

					CPathNode *pNeighbourNode = &apRegions[neighbourNodeAddr.GetRegion()]->aNodes[neighbourNodeAddr.GetIndex()];

					// Only print line segments once
					if(!(pNode->m_address < pNeighbourNode->m_address))
					{
						continue;
					}

					Color32 colorToDrawLink = nodeGroupColours[(pNode->m_1.m_group % 8)];
					if(bDisplayPathsWithDensityOnVMap || bDisplayPathsWithDensityOnVMapDontForceLoad)
					{
						const float linkDensityDesirePortion = (static_cast<float>(pNode->m_2.m_density)/15.0f + static_cast<float>(pNeighbourNode->m_2.m_density)/15.0f) / 2.0f;
						colorToDrawLink = Color32(linkDensityDesirePortion,linkDensityDesirePortion,linkDensityDesirePortion);
					}

					bool drawThick = false;
					if(bDisplayPathsForTunnelsOnVMapInOrange && (pNode->m_2.m_inTunnel || pNeighbourNode->m_2.m_inTunnel))
					{
						colorToDrawLink = Color32(255,128,0);
					}

					if(bDisplayPathsForBadSlopesOnVMapInFlashingPurple && flashBadSlopesThisFrame)
					{
						Vector3 crs1;
						pNode->GetCoors(crs1);
						Vector3 crs2;
						pNeighbourNode->GetCoors(crs2);
						const float slope = (crs1.z - crs2.z) / (crs1 - crs2).XYMag();
						if(rage::Abs(slope)>2.0f)
						{
							colorToDrawLink = Color32(255,0,255);
							drawThick = true;
						}
					}
					if(bDisplayPathsForLowPathsOnVMapInFlashingWhite && flashBadSlopesThisFrame)
					{
						Vector3 crs1;
						pNode->GetCoors(crs1);
						Vector3 crs2;
						pNeighbourNode->GetCoors(crs2);
						if(crs1.z < 0.0f || crs2.z < 0.0f)
						{
							colorToDrawLink = Color32(255,0,255);
							drawThick = true;
						}
					}

					const bool linkIsOff = (pNode->m_2.m_switchedOff || pNeighbourNode->m_2.m_switchedOff);
					const bool bPlayersRoad = (pNode->m_1.m_onPlayersRoad || pNeighbourNode->m_1.m_onPlayersRoad);
					if((!linkIsOff || !bDisplayPathsOnVMapFlashingForSwitchedOffNodes || flashOffLinksThisFrame) &&
						(!bDisplayOnlyPlayersRoadNodes || bPlayersRoad ))
					{
						Vector3 nodePos(0.0f, 0.0f, 0.0f);
						pNode->GetCoors(nodePos);
						nodePos.z = 0.0f;
						Vector3 neighbourNodePos(0.0f, 0.0f, 0.0f);
						pNeighbourNode->GetCoors(neighbourNodePos);
						neighbourNodePos.z = 0.0f;
						if(drawThick)
						{
							CVectorMap::DrawLineThick(nodePos, neighbourNodePos, colorToDrawLink, colorToDrawLink);
						}
						else
						{
							CVectorMap::DrawLine(nodePos, neighbourNodePos, colorToDrawLink, colorToDrawLink);
						}
					}

				}// END for all links off the current node.
			}// END for all car nodes in the region.
		}// END for all regions.
	}
	else
	{
		if (bPreviousDisplayPathsOnVMap)
		{
			ReleaseRequestedNodes(NODE_REQUEST_SCRIPT);
		}
	}
	bPreviousDisplayPathsOnVMap = bDisplayPathsOnVMap;

	/*
	static bool bPreviousDisplayNetworkRestarts=false;
	static bool bPreviousDisplayNetworkRestartsOnVMap=false;
	if(bDisplayPathsDebug_Nodes_NetworkRestarts || bDisplayNetworkRestartsOnVMap)
	{
		ThePaths.MakeRequestForNodesToBeLoaded(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_YMAX, NODE_REQUEST_SCRIPT);

		// Go through all possible nodes and render them in the colour of the group
		for(s32 region = 0; region < PATHFINDREGIONS; region++)
		{
			if(!apRegions[region] || !apRegions[region]->aNodes)		// Make sure this region is actually loaded in.
			{
				continue;
			}

			for(s32 Nodes = 0; Nodes < apRegions[region]->NumNodesCarNodes; Nodes++)			// Only car nodes
			{
				CPathNode *pNode = &apRegions[region]->aNodes[Nodes];

				if(!pNode->IsNetworkRestart())
				{
					continue;
				}

				s32 group = pNode->m_1.m_specialFunction - SPECIAL_NETWORK_RESTART_0;
				Color32 col = Color32( (100+group*100)&255, (group*200)&255, (group*60)&255, 255 );

				Vector3 nodePos(0.0f, 0.0f, 0.0f);
				pNode->GetCoors(nodePos);
				nodePos.z = 0.0f;

				if(bDisplayPathsDebug_Nodes_NetworkRestarts)
				{
					grcDebugDraw::Sphere(nodePos, 2.0f, col);
				}
				if(bDisplayNetworkRestartsOnVMap)
				{
					CVectorMap::DrawLine(nodePos, Vector3(nodePos.x, nodePos.y+10.0f, nodePos.z), col, col);
				}
			}
		}
	}
	else
	{
		if (bPreviousDisplayNetworkRestarts || bPreviousDisplayNetworkRestartsOnVMap)
		{
			ThePaths.ReleaseRequestedNodes(NODE_REQUEST_SCRIPT);
		}
	}
	bPreviousDisplayNetworkRestartsOnVMap = bDisplayNetworkRestartsOnVMap;
	bPreviousDisplayNetworkRestarts = bDisplayPathsDebug_Nodes_NetworkRestarts;
	*/

	if(bDisplayGpsDisabledRegionsOnVMap || bDisplayGpsDisabledRegionsInWorld)
	{
		for (s32 i = 0; i < MAX_GPS_DISABLED_ZONES; i++)
		{
			Color32 col(1.0f, 1.0f, 0.0f, 1.0f);

			if(m_GPSDisabledZones[i].m_iGpsBlockedByScriptID)
			{
				if(bDisplayGpsDisabledRegionsOnVMap)
				{
					// Area defined by Min/Max
					CVectorMap::DrawLine(Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY), 0.0f), 
						Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY), 0.0f), col, col);
					CVectorMap::DrawLine(Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY), 0.0f), 
						Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY), 0.0f), col, col);
					CVectorMap::DrawLine(Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY), 0.0f), 
						Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY), 0.0f), col, col);
					CVectorMap::DrawLine(Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY), 0.0f), 
						Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY), 0.0f), col, col);
					CVectorMap::DrawLine(Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY), 0.0f), 
						Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY), 0.0f), col, col);	//diagonal
				}
			
				if(bDisplayGpsDisabledRegionsInWorld)
				{
					grcDebugDraw::BoxAxisAligned(
						VECTOR3_TO_VEC3V(Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY), -100.0f)),
						VECTOR3_TO_VEC3V(Vector3(INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX), INT16_TO_COORS(m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY), 1000.0f)),
						col, false);
				}

				grcDebugDraw::AddDebugOutput("%d: %d:%d -> %d:%d\n", i,
					m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX, m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY,
					m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX, m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY);
			}
		}
	}

	if(bDisplayNodeSwitchesOnVMap || bDisplayNodeSwitchesInWorld)
	{
		for (s32 i = 0; i < NumNodeSwitches; i++)
		{
			Color32 col(1.0f, 1.0f, 1.0f, 1.0f);

			if (NodeSwitches[i].bOnlyForDurationOfMission)
			{
				col = Color32(1.0f, 0.0f, 0.0f, 1.0f);
			}

			if( NodeSwitches[i].bAxisAlignedSwitch )
			{
				if(bDisplayNodeSwitchesOnVMap)
				{
					// Area defined by Min/Max
					CVectorMap::DrawLine(Vector3(NodeSwitches[i].AA.MinX, NodeSwitches[i].AA.MinY, 0.0f), Vector3(NodeSwitches[i].AA.MaxX, NodeSwitches[i].AA.MinY, 0.0f), col, col);
					CVectorMap::DrawLine(Vector3(NodeSwitches[i].AA.MaxX, NodeSwitches[i].AA.MaxY, 0.0f), Vector3(NodeSwitches[i].AA.MaxX, NodeSwitches[i].AA.MinY, 0.0f), col, col);
					CVectorMap::DrawLine(Vector3(NodeSwitches[i].AA.MaxX, NodeSwitches[i].AA.MaxY, 0.0f), Vector3(NodeSwitches[i].AA.MinX, NodeSwitches[i].AA.MaxY, 0.0f), col, col);
					CVectorMap::DrawLine(Vector3(NodeSwitches[i].AA.MinX, NodeSwitches[i].AA.MaxY, 0.0f), Vector3(NodeSwitches[i].AA.MinX, NodeSwitches[i].AA.MinY, 0.0f), col, col);
					CVectorMap::DrawLine(Vector3(NodeSwitches[i].AA.MinX, NodeSwitches[i].AA.MinY, 0.0f), Vector3(NodeSwitches[i].AA.MaxX, NodeSwitches[i].AA.MaxY, 0.0f), col, col);	//diagonal
				}
				if(bDisplayNodeSwitchesInWorld)
				{
					grcDebugDraw::BoxAxisAligned(
						VECTOR3_TO_VEC3V(Vector3(NodeSwitches[i].AA.MinX, NodeSwitches[i].AA.MinY, NodeSwitches[i].AA.MinZ)),
						VECTOR3_TO_VEC3V(Vector3(NodeSwitches[i].AA.MaxX, NodeSwitches[i].AA.MaxY, NodeSwitches[i].AA.MaxZ)),
						col, false);
				}

				grcDebugDraw::AddDebugOutput("%d:   x:%.2f %.2f y:%.2f %.2f\n", i,
					NodeSwitches[i].AA.MinX, NodeSwitches[i].AA.MaxX, NodeSwitches[i].AA.MinY, NodeSwitches[i].AA.MaxY);
			}
			else
			{
				// Area defined by line and width
				float	widthOverTwo = NodeSwitches[i].Angled.AreaWidth * 0.5f;
				Vector3	lineStart = NodeSwitches[i].Angled.vAreaStart.Get();
				Vector3	lineEnd = NodeSwitches[i].Angled.vAreaEnd.Get();
				Vector3	dV = lineEnd - lineStart;
				Vector3 right = CrossProduct(dV, ZAXIS);
				right.Normalize();
				Vector3	vToTop = right * -widthOverTwo;
				Vector3	vToBottom = right * widthOverTwo;

				Vector3 vCoords[4] =
				{
					lineStart + vToTop,
					lineEnd + vToTop,
					lineEnd + vToBottom,
					lineStart + vToBottom
				};

				if(bDisplayNodeSwitchesOnVMap)
				{
					CVectorMap::DrawLine(vCoords[0], vCoords[1], col, col);
					CVectorMap::DrawLine(vCoords[1], vCoords[2], col, col);
					CVectorMap::DrawLine(vCoords[2], vCoords[3], col, col);
					CVectorMap::DrawLine(vCoords[3], vCoords[0], col, col);
				}
				if(bDisplayNodeSwitchesInWorld)
				{
					grcDebugDraw::Line(vCoords[0], vCoords[1], col, col);
					grcDebugDraw::Line(vCoords[1], vCoords[2], col, col);
					grcDebugDraw::Line(vCoords[2], vCoords[3], col, col);
					grcDebugDraw::Line(vCoords[3], vCoords[0], col, col);
				}
			}
		}
	}

#if __DEV
	if (DebugGetRandomCarNode)
	{
		Vector3			vReturn, vOrig;
		CNodeAddress	nodeAddr;

		vOrig = Vector3(0.0f, 0.0f, 0.0f);
		CPathFind::GetRandomCarNode(vOrig, 200.0f, false, 0, true, true, true, vReturn, nodeAddr);	// This call only serves to reset the batch
		for (s32 i = 0; i < DebugGetRandomCarNode; i++)
		{
			Vector3 vPlayer = CGameWorld::FindLocalPlayerCoors();
			if(CPathFind::GetRandomCarNode(vPlayer, 300.0f, false, 0, false, false, true, vReturn, nodeAddr))
			{
				Color32 col = Color32(1.0f, 1.0f, 0.0f, 1.0f);
				CVectorMap::DrawMarker(vReturn, col);
			}
		}
	} 
#endif

	CRoadSpeedZoneManager::GetInstance().DebugDraw();
	// The following bit draws the links between the different tiles
	// and the lanes associated with them.
	//	{
	//		s16	C, LaneC;
	//		s8	Lanes;
	//		float	ZCo;
	//
	//		for (C = 0; C < NumCarPathLinks; C++)
	//		{
	//			// For the z coordinate we simply take the z of the
	//			// adjacent node and a wee bit higher.
	//			ZCo = aNodes[aCarLinks[C].Node1].Coors.z;
	//
	//			// Draw a wee red line for the actual centre point of the iLink.
	//			CLines::RenderLineWithClipping( aCarLinks[C].m_coorsX, aCarLinks[C].m_coorsY, ZCo,
	//								 aCarLinks[C].m_coorsX, aCarLinks[C].m_coorsY, ZCo + 4.0f,
	//								 0xff0000ff, 0xff0000ff);
	//
	//			// Now draw some shit for the lanes
	//			Lanes = aCarLinks[C].LanesTo + aCarLinks[C].LanesFro;
	//
	//			for (LaneC = 0; LaneC < Lanes; LaneC++)
	//			{
	//				Vector3	LaneCo, Perp;
	//
	//				Perp = Vector3(aCarLinks[C].DirY, aCarLinks[C].DirX, 0.0f);
	//				LaneCo = Vector3(aCarLinks[C].m_coorsX, aCarLinks[C].m_coorsY, ZCo);
	//				LaneCo += Perp * (5.0f * (LaneC - Lanes*0.5f + 0.5f));
	//
	//				CLines::RenderLineWithClipping( LaneCo.x - aCarLinks[C].DirX, LaneCo.y - aCarLinks[C].DirY, LaneCo.z + 0.5f,
	//									 LaneCo.x + aCarLinks[C].DirX, LaneCo.y + aCarLinks[C].DirY, LaneCo.z + 0.5f,
	//									 0xff00ffff, 0xffff00ff);
	//			}
	//		}
	//	}
}
#endif // __BANK


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalcDistToAnyConnectingLinks
// PURPOSE :  Works out the distance from a point to any of the connecting links
//			  to this node.
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::CalcDistToAnyConnectingLinks(const CPathNode *pNode, const Vector3& SearchCoors) const
{
	Vector3	Crs1;
	pNode->GetCoors(Crs1);

	float	SmallestDistSqr = 999999.9f*999999.9f;

	if(pNode->NumLinks() == 0)		// Isolated nodes if we're looking for network restart nodes.(Only cropped up with Davids performance stuff).
	{
		return (SearchCoors-Crs1).Mag();
	}

	for(u32 Link = 0; Link < pNode->NumLinks(); Link++)
	{
		CNodeAddress otherNodeAddr = GetNodesLinkedNodeAddr(pNode, Link);

		if(IsRegionLoaded(otherNodeAddr))
		{
			const CPathNode *pOtherNode = FindNodePointer(otherNodeAddr);

			Vector3	Crs2;
			pOtherNode->GetCoors(Crs2);
			SmallestDistSqr = rage::Min(SmallestDistSqr, fwGeom::DistToLineSqr(Crs1, Crs2, SearchCoors));
		}
	}
	return rage::Sqrtf(SmallestDistSqr);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeClosestInRegion
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindNodeClosestInRegion(CNodeAddress *pNodeFound, u32 Region, const Vector3& SearchCoors, float *pClosestDist
	, NodeConsiderationType eNodeConsiderationType, bool bIgnoreAlreadyFound, bool bBoatNodes, bool bHidingNodes
	, bool UNUSED_PARAM(bNetworkRestart), bool bMatchDirection, float dirToMatchX, float dirToMatchY, float zMeasureMult, float zTolerance
	, s32 mapAreaIndex, bool bForGps, bool bIgnoreOneWayRoads, const float fCutoffDistForNodeSearch
	, const CVehicleNodeList* pPreferredNodeList, const bool bIgnoreSlipLanes, const bool bIgnoreSwitchedOffDeadEnds) const
{
	PF_FUNC(FindNodeClosestInRegion1);

	CPathRegion* pReg = apRegions[Region];

	// Make sure this Region is loaded in at the moment
	if(!pReg || !pReg->aNodes)
	{
		return;
	}

	s32	minNode, maxNode;

	// If a cutoff distance is specified, then we can optimise the search a little
	// by searching only within the range of Y coordinates we're interested in
	if(!bForGps && fCutoffDistForNodeSearch > 0.0f)
	{
		const float fMinY = SearchCoors.y - fCutoffDistForNodeSearch;
		const float fMaxY = SearchCoors.y + fCutoffDistForNodeSearch;
		FindStartAndEndNodeBasedOnYRange(Region, fMinY, fMaxY, minNode, maxNode);
	}
	else
	{
		minNode = 0;
		maxNode = pReg->NumNodesCarNodes;
	}

	bool bFoundSwitchedOnNode = false;
	float closestDist = *pClosestDist;
	float fOriginalClosestDist = closestDist;

	// Do a binary search within the range of interest, to find the node(s)
	// with the closest Y coordinate, to use as a starting point.
	int centerNode;

	// This is like COORS_TO_INT16(searchMinY), but clamped to the range that we can represent with an s16.
	const int searchYInt = (int)(SearchCoors.y*PATHCOORD_XYSHIFT);
	const int searchYIntClamped = Clamp(searchYInt, -0x8000, 0x7fff);
	CPathNode key;
	key.m_coorsY = (s16)searchYIntClamped;

	CPathNode* ret = std::lower_bound(pReg->aNodes + minNode, pReg->aNodes + maxNode, key, PathNodeYCompare);
	centerNode = ptrdiff_t_to_int(ret - &pReg->aNodes[0]);

	// Do two passes, one in each direction, starting from centerNode.
	for(int pass = 0; pass < 2; pass++)
	{
		// Prepare for iteration in the current pass.
		int Node;
		int delta;
		int maxNodeCount;
		if(pass == 0)
		{
			// First pass: start from centerNode - 1, and go backwards in the array
			// until we have checked minNode (in the worst case).
			Node = centerNode - 1;
			delta = -1;
			maxNodeCount = centerNode - minNode;
		}
		else
		{
			// Second pass: start from centerNode, and go forward in the array
			// stopping as we are about to reach maxNode (in the worst case).
			Node = centerNode;
			delta = 1;
			maxNodeCount = maxNode - centerNode;
		}

		for(int nodeCount = 0; nodeCount < maxNodeCount; nodeCount++, Node += delta)
		{
			Assert(Node >= minNode && Node < maxNode);

			CPathNode	*pNode = &((pReg->aNodes)[Node]);

			if(	pNode->IsPedNode() ||
				(eNodeConsiderationType == IgnoreSwitchedOffNodes && pNode->m_2.m_switchedOff) ||
				(eNodeConsiderationType == PreferSwitchedOnNodes && pNode->m_2.m_switchedOff && bFoundSwitchedOnNode) ||
				(bForGps && pNode->m_2.m_density == 0) ||
				(bIgnoreAlreadyFound && pNode->m_2.m_alreadyFound) ||
				(bBoatNodes != static_cast<bool>(pNode->m_2.m_waterNode)) ||
				(bHidingNodes != (pNode->m_1.m_specialFunction == SPECIAL_HIDING_NODE)) ||
				(bForGps && pNode->m_2.m_noGps && !bIgnoreNoGpsFlag) ||
				(bIgnoreOneWayRoads && pNode->NumLinks()==2 && GetNodesLink(pNode, 0).IsOneWay() && GetNodesLink(pNode, 1).IsOneWay()) ||
				(bIgnoreSlipLanes && (pNode->IsSlipLane() || pNode->m_2.m_slipJunction)) ||
				(bIgnoreSwitchedOffDeadEnds && pNode->IsSwitchedOff() && pNode->m_2.m_deadEndness > 0)
				)
			{
				continue;
			}

			Vector3	nodeCrs;
			pNode->GetCoors(nodeCrs);
			float fDistanceToTest;
			if(pNodeFound->IsEmpty() ||
				(eNodeConsiderationType == PreferSwitchedOnNodes && !pNode->m_2.m_switchedOff && !bFoundSwitchedOnNode))
			{
				fDistanceToTest = fOriginalClosestDist;
			}
			else
			{
				fDistanceToTest = closestDist;
			}

			// Compute the absolute Y delta between the node and the search coordinate.
			const float absDeltaY = rage::Abs(nodeCrs.y - SearchCoors.y);

			// If the quantized Y position is the same between the search position and the node,
			// we might actually be on the wrong side relative to the direction of the array in which
			// we are searching. Not entirely sure but there might be cases where we would stop too
			// early if we did the usual distance rejection, so just skip that part (should be no
			// more than a handful of nodes anyway).
			if(searchYIntClamped != pNode->m_coorsY)
			{
				// When going down in the array (pass 0), we expect to find only nodes with smaller
				// Y coordinates than the search position, and vice versa for pass 1.
				Assert(pass != 0 || nodeCrs.y <= SearchCoors.y);
				Assert(pass != 1 || nodeCrs.y >= SearchCoors.y);

				// Check to see if we may be able to stop the search right here. For this to be possible,
				// we need to either not be in PreferSwitchedOnNodes mode, or already found a switched on node
				// (otherwise, there is still a chance that we will find a switched on node that we would want to
				// use instead of the best switched off node).
				if(eNodeConsiderationType != PreferSwitchedOnNodes || bFoundSwitchedOnNode)
				{
					// Compute the minimum score we can ever get if we continue to search the array in this
					// direction. The X and Z coordinates can be anything, but the absolute Y delta can only
					// increase. Note that the scoring modifications based on CalcDirectionMatchValue(),
					// ContainsNode(), and CalcDistToAnyConnectingLinks() can only add to the score, so we
					// don't need to add or subtract anything here to make those work properly.
					const float MinDistToFurtherNode = 0.3f*absDeltaY;

					// Now, compare against the best score found so far.
					if(MinDistToFurtherNode > fDistanceToTest)
					{
						// No improvement is possible by going further in this direction of the array,
						// so we can stop.
						break;
					}
				}
			}

			const float fZDelta = rage::Abs(nodeCrs.z - SearchCoors.z);
			const float fZScore = fZDelta > zTolerance ? fZDelta : 0.0f;
			float DistToNode =
				0.3f * (rage::Abs(nodeCrs.x - SearchCoors.x) + absDeltaY + zMeasureMult*fZScore);

			if (bMatchDirection)
			{
				DistToNode -= 50.0f * (CalcDirectionMatchValue(pNode, dirToMatchX, dirToMatchY) - 1.0f);
			}

			if (pPreferredNodeList && !pPreferredNodeList->ContainsNode(pNode->m_address))
			{
				DistToNode += 25.0f;
			}

			if( DistToNode > fDistanceToTest)
			{
				continue;
			}

			// This node could potentially be close enough to replace the current candidate.
			float	TotalDist = DistToNode + 0.2f * CalcDistToAnyConnectingLinks(pNode, SearchCoors);
			if(TotalDist > fDistanceToTest)
			{
				continue;
			}

			if((mapAreaIndex >= 0) && (mapAreaIndex != CMapAreas::GetAreaIndexFromPosition( nodeCrs )))
			{
				continue;
			}

			closestDist = TotalDist;
			pNodeFound->Set(Region, Node);

			if(!pNode->m_2.m_switchedOff)
			{
				bFoundSwitchedOnNode = true;
			}
		}
	}
	
	*pClosestDist = closestDist;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeClosestInRegion
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindNodeClosestInRegion(CNodeAddress * pNodeFound, u32 Region, const Vector3 & SearchCoors, float * pClosestDist, FindClosestNodeCB callbackFunction, void * pData, const float fCutoffDistForNodeSearch, bool bForGps) const
{
	PF_FUNC(FindNodeClosestInRegion2);

	CPathRegion* pReg = apRegions[Region];

	// Make sure this Region is loaded in at the moment
	if(!pReg || !pReg->aNodes)
	{
		return;
	}

	s32	minNode, maxNode;

	// If a cutoff distance is specified, then we can optimise the search a little
	// by searching only within the range of Y coordinates we're interested in
	// For GPS this does not always work so well

	if(!bForGps && fCutoffDistForNodeSearch > 0.0f)
	{
		const float fMinY = SearchCoors.y - (fCutoffDistForNodeSearch + 1.0f);
		const float fMaxY = SearchCoors.y + (fCutoffDistForNodeSearch + 1.0f);
		FindStartAndEndNodeBasedOnYRange(Region, fMinY, fMaxY, minNode, maxNode);
	}
	else
	{
		minNode = 0;
		maxNode = pReg->NumNodesCarNodes;
	}

	float closestDist2 = *pClosestDist * *pClosestDist;

	// Do a binary search within the range of interest, to find the node(s)
	// with the closest Y coordinate, to use as a starting point.
	int centerNode;
	const int searchYInt = COORS_TO_INT16(SearchCoors.y);
	CPathNode key;
	key.m_coorsY = (s16)searchYInt;
	CPathNode* ret = std::lower_bound(pReg->aNodes + minNode, pReg->aNodes + maxNode, key, PathNodeYCompare);
	centerNode = ptrdiff_t_to_int(ret - &pReg->aNodes[0]);

	// Do two passes, one in each direction, starting from centerNode.
	for(int pass = 0; pass < 2; pass++)
	{
		// Prepare for iteration in the current pass.
		int Node;
		int delta;
		int maxNodeCount;
		if(pass == 0)
		{
			// First pass: start from centerNode - 1, and go backwards in the array
			// until we have checked minNode (in the worst case).
			Node = centerNode - 1;
			delta = -1;
			maxNodeCount = centerNode - minNode;
		}
		else
		{
			// Second pass: start from centerNode, and go forward in the array
			// stopping as we are about to reach maxNode (in the worst case).
			Node = centerNode;
			delta = 1;
			maxNodeCount = maxNode - centerNode;
		}

		for(int nodeCount = 0; nodeCount < maxNodeCount; nodeCount++, Node += delta)
		{
			Assert(Node >= minNode && Node < maxNode);

			CPathNode	*pNode = &((pReg->aNodes)[Node]);

			for(int i = 0; i < MAX_GPS_DISABLED_ZONES; ++i)
			{
				if(bForGps && m_GPSDisabledZones[i].m_iGpsBlockedByScriptID)
				{
					if( pNode->m_coorsX < m_GPSDisabledZones[i].m_iGpsBlockingRegionMinX || pNode->m_coorsX > m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxX ||
						pNode->m_coorsY < m_GPSDisabledZones[i].m_iGpsBlockingRegionMinY || pNode->m_coorsY > m_GPSDisabledZones[i].m_iGpsBlockingRegionMaxY )
					{
						// no intersection
					}
					else
					{
						continue;
					}
				}
			}

			if(callbackFunction && !callbackFunction(pNode, pData))
			{
				continue;
			}

			Vector3	nodeCrs;
			pNode->GetCoors(nodeCrs);

			const float deltaY = nodeCrs.y - SearchCoors.y;
			const float deltaYSquared = square(deltaY);

			// If the quantized Y position is the same between the search position and the node,
			// we might actually be on the wrong side relative to the direction of the array in which
			// we are searching. Not entirely sure but there might be cases where we would stop too
			// early if we did the usual distance rejection, so just skip that part (should be no
			// more than a handful of nodes anyway).
			if(searchYInt != pNode->m_coorsY)
			{
				// When going down in the array (pass 0), we expect to find only nodes with smaller
				// Y coordinates than the search position, and vice versa for pass 1.
				Assert(pass != 0 || nodeCrs.y <= SearchCoors.y);
				Assert(pass != 1 || nodeCrs.y >= SearchCoors.y);

				if(deltaYSquared > closestDist2)
				{
					// No improvement is possible by going further in this direction of the array,
					// so we can stop.
					break;
				}
			}

			const float deltaX = nodeCrs.x - SearchCoors.x;
			const float deltaZ = nodeCrs.z - SearchCoors.z;
			float Dist2ToNode = square(deltaX) + deltaYSquared + square(deltaZ);

			if( Dist2ToNode > closestDist2)
			{
				continue;
			}

			closestDist2 = Dist2ToNode;
			pNodeFound->Set(Region, Node);
		}
	}

	*pClosestDist = closestDist2 > 0.0f ? Sqrtf(closestDist2) : 0.0f;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindPedNodeClosestInRegion
// PURPOSE :  Same as FindNodeClosestInRegion but specifically for ped-nodes
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindPedNodeClosestInRegion(CNodeAddress *out_pNodeFound, float *inout_pClosestDist, u32 Region, const CFindPedNodeParams& searchParams) const
{
	CPathRegion* pReg = apRegions[Region];

#if __ASSERT
	// Check that the special functions are all ped ones
	for(int i=0; i < searchParams.m_SpecialFunctions.GetCount(); i++)
	{
		int specialFunction = searchParams.m_SpecialFunctions[i];
		AssertMsg(specialFunction==SPECIAL_PED_CROSSING||specialFunction==SPECIAL_PED_DRIVEWAY_CROSSING||specialFunction==SPECIAL_PED_ASSISTED_MOVEMENT, "FindPedNodeClosestInRegion called with non ped specialFunction");
	}
#endif // __ASSERT

	s32		minNode=0, maxNode=0;

	// Make sure this Region is loaded in at the moment
	if(!pReg || !pReg->aNodes)
	{
		return;
	}

	float closestDist = *inout_pClosestDist;

	minNode = pReg->NumNodesCarNodes;	//0
	maxNode = minNode + pReg->NumNodesPedNodes;		//pReg->NumNodesCarNodes;

	// Do a binary search within the range of interest, to find the node(s)
	// with the closest Y coordinate, to use as a starting point.
	int centerNode;
	const int searchYInt = COORS_TO_INT16(searchParams.m_vSearchPosition.y);
	CPathNode key;
	key.m_coorsY = (s16)searchYInt;
	CPathNode* ret = std::lower_bound(pReg->aNodes + minNode, pReg->aNodes + maxNode, key, PathNodeYCompare);
	centerNode = ptrdiff_t_to_int(ret - &pReg->aNodes[0]);

	// Do two passes, one in each direction, starting from centerNode.
	for(int pass = 0; pass < 2; pass++)
	{
		// Prepare for iteration in the current pass.
		int Node;
		int delta;
		int maxNodeCount;
		if(pass == 0)
		{
			// First pass: start from centerNode - 1, and go backwards in the array
			// until we have checked minNode (in the worst case).
			Node = centerNode - 1;
			delta = -1;
			maxNodeCount = centerNode - minNode;
		}
		else
		{
			// Second pass: start from centerNode, and go forward in the array
			// stopping as we are about to reach maxNode (in the worst case).
			Node = centerNode;
			delta = 1;
			maxNodeCount = maxNode - centerNode;
		}

		for(int nodeCount = 0; nodeCount < maxNodeCount; nodeCount++, Node += delta)
		{
			Assert(Node >= minNode && Node < maxNode);

			CPathNode	*pNode = &((pReg->aNodes)[Node]);

			Vector3 nodePos(0.0f, 0.0f, 0.0f);
			pNode->GetCoors(nodePos);

			// Compute the absolute Y delta between the node and the search coordinate.
			const float absDeltaY = rage::Abs(nodePos.y - searchParams.m_vSearchPosition.y);

			// If the quantized Y position is the same between the search position and the node,
			// we might actually be on the wrong side relative to the direction of the array in which
			// we are searching. Not entirely sure but there might be cases where we would stop too
			// early if we did the usual distance rejection, so just skip that part (should be no
			// more than a handful of nodes anyway).
			if(searchYInt != pNode->m_coorsY)
			{
				// When going down in the array (pass 0), we expect to find only nodes with smaller
				// Y coordinates than the search position, and vice versa for pass 1.
				Assert(pass != 0 || nodePos.y <= searchParams.m_vSearchPosition.y);
				Assert(pass != 1 || nodePos.y >= searchParams.m_vSearchPosition.y);

				// Compute the minimum score we can ever get if we continue to search the array in this
				// direction. The X and Z coordinates can be anything, but the absolute Y delta can only
				// increase. Note that the scoring modifications based on CalcDistToAnyConnectingLinks()
				// can only add to the score, so we don't need to add or subtract anything here to make
				// that work properly.
				const float minDistToFurtherNode = 0.3f*absDeltaY;

				// Now, compare against the best score found so far.
				if(minDistToFurtherNode > closestDist)
				{
					// No improvement is possible by going further in this direction of the array,
					// so we can stop.
					break;
				}
			}

			// Check to see if this node matches the previous start node
			if( searchParams.m_pPrevStartAddr != NULL && *searchParams.m_pPrevStartAddr == pNode->m_address )
			{
				// skip the node
				continue;
			}

			// Check to see if this node matches the previous end node
			if( searchParams.m_pPrevStartAddr != NULL && searchParams.m_pPrevEndAddr != NULL && pNode->m_address == *searchParams.m_pPrevEndAddr )
			{
				// Check if the only link is back to the previous start node
				if(pNode->NumLinks() == 1)
				{
					const CPathNodeLink& link = GetNodesLink(pNode, 0);
					if( !link.m_OtherNode.IsEmpty() && IsRegionLoaded(link.m_OtherNode) && link.m_OtherNode == *searchParams.m_pPrevStartAddr )
					{
						// skip the node
						continue;
					}
				}
			}

			// Check if this node's special function is a match for any of the search param values
			bool bFoundSpecialFunctionMatch = false;
			for(int i=0; i < searchParams.m_SpecialFunctions.GetCount(); i++)
			{
				if(pNode->m_1.m_specialFunction == searchParams.m_SpecialFunctions[i])
				{
					bFoundSpecialFunctionMatch = true;
					break;
				}
			}
			if( !bFoundSpecialFunctionMatch )
			{
				// skip the node
				continue;
			}

			float DistToNode = 0.3f * (rage::Abs(nodePos.x - searchParams.m_vSearchPosition.x) + absDeltaY + 3.0f*rage::Abs(nodePos.z - searchParams.m_vSearchPosition.z));
			if( DistToNode >= closestDist)
			{
				continue;
			}

			// This node could potentially be close enough to replace the current candidate.
			float	TotalDist = DistToNode + 0.2f * CalcDistToAnyConnectingLinks(pNode, searchParams.m_vSearchPosition);
			if(TotalDist >= closestDist)
			{
				continue;
			}

			closestDist = TotalDist;
			out_pNodeFound->Set(Region, Node);
		}
	}

	*inout_pClosestDist = closestDist;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindPedNodeClosestToCoors
// PURPOSE :  Same as FindNodeClosestToCoors but specifically for ped-nodes
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFind::FindPedNodeClosestToCoors(const CFindPedNodeParams& searchParams) const
{
	s32			CenterRegionX, CenterRegionY;
	CNodeAddress	ClosestNode;
	float			ClosestDist = searchParams.m_fCutoffDistance;
	u16			Region;

	ClosestNode.SetEmpty();

	// We start with the Region that the Searchcoors are within.

	// Find the X and Y of the Region we're in.
	CenterRegionX = FindXRegionForCoors(searchParams.m_vSearchPosition.x);
	CenterRegionY = FindYRegionForCoors(searchParams.m_vSearchPosition.y);
	Region = (u16)(FIND_REGION_INDEX(CenterRegionX, CenterRegionY));
	FindPedNodeClosestInRegion(&ClosestNode, &ClosestDist, Region, searchParams);

	// Examine the 8 surrounding regions, if they are intersected by the search extents
	const s32 iRegionOffsets[] =
	{
		-1, -1,
		0, -1,
		1, -1,
		-1, 0,
		1, 0,
		-1, 1,
		0, 1,
		1, 1
	};

	for(s32 r=0; r<8; r++)
	{
		s32 AdjacentRegionX = CenterRegionX+iRegionOffsets[r*2];
		s32 AdjacentRegionY = CenterRegionY+iRegionOffsets[(r*2)+1];

		if(AdjacentRegionX >= 0 && AdjacentRegionX < PATHFINDMAPSPLIT && AdjacentRegionY >= 0 && AdjacentRegionY < PATHFINDMAPSPLIT)
		{
			Region = (u16)(FIND_REGION_INDEX(AdjacentRegionX, AdjacentRegionY));

			float fRegionMinX, fRegionMinY;
			FindStartPointOfRegion(AdjacentRegionX, AdjacentRegionY, fRegionMinX, fRegionMinY);
			float fRegionMaxX = fRegionMinX+PATHFINDREGIONSIZEX;
			float fRegionMaxY = fRegionMinY+PATHFINDREGIONSIZEY;

			bool bIntersectsX =
				searchParams.m_vSearchPosition.x >= fRegionMinX - searchParams.m_fCutoffDistance &&
				searchParams.m_vSearchPosition.x <= fRegionMaxX + searchParams.m_fCutoffDistance;

			bool bIntersectsY =
				searchParams.m_vSearchPosition.y >= fRegionMinY - searchParams.m_fCutoffDistance &&
				searchParams.m_vSearchPosition.y <= fRegionMaxY + searchParams.m_fCutoffDistance;

			if(bIntersectsX && bIntersectsY)
			{
				FindPedNodeClosestInRegion(&ClosestNode, &ClosestDist, Region, searchParams);
			}
		}
	}

	return(ClosestNode);
}


//********************************************************************************************************************
// Go through all the loaded assisted-movement nodes in the given area, and store pointers to one node in each route.
// These pointers will always be an 'end-node' with only one iLink, and since the routes are stricly linear we can
// then easily trace along these routes to get each 'assisted-movement' route. (This function written by JB).
int CPathFind::FindAssistedMovementRoutesInArea(const Vector3 & vOrigin, const float fMaxDist, CPathNode ** pUniqueRouteNodes, int iMaxNumNodes)
{
	static const s32 MAX_NODES_TO_CLEAR = 6000;
	CNodeAddress nodesToBeCleared[MAX_NODES_TO_CLEAR];

	int r,u,a,n,x,y;

	// fMaxDist must be within reasonable value to allow dist-sqr to be stored in a signed 16bit val
	aiAssertf(fMaxDist <= 100.0f, "fMaxDist needs to be smaller than 100.0f");
	const float fMaxDistSqr = (fMaxDist * fMaxDist);

	const Vector3 vAreaMin(vOrigin.x - fMaxDist, vOrigin.y - fMaxDist, vOrigin.z - fMaxDist);
	const Vector3 vAreaMax(vOrigin.x + fMaxDist, vOrigin.y + fMaxDist, vOrigin.z + fMaxDist);

	atArray<CAssistedRouteInfo> routes;
	routes.Reserve(iMaxNumNodes);

	// This is used to clear the 'm_distanceToTarget' value in each node upon exiting this function.
	// We simply use a non-zero 'm_distanceToTarget' to indicate that the node has been visited.
	int iNumNodesToBeCleared = 0;

	for(y=0; y<PATHFINDMAPSPLIT; y++)
	{
		for(x=0; x<PATHFINDMAPSPLIT; x++)
		{
			const int iRegionIndex = FIND_REGION_INDEX(x, y);

			if(!apRegions[iRegionIndex] || !apRegions[iRegionIndex]->aNodes)
			{
				continue;
			}

			const float fRegionMinX = FindXCoorsForRegion(x);
			const float fRegionMinY = FindYCoorsForRegion(y);
			const float fRegionMaxX = fRegionMinX + PATHFINDREGIONSIZEX;
			const float fRegionMaxY = fRegionMinY + PATHFINDREGIONSIZEY;

			if(fRegionMinX > vAreaMax.x || fRegionMinY > vAreaMax.y || vAreaMin.x > fRegionMaxX || vAreaMin.y > fRegionMaxY)
			{
				continue;
			}

			const int iNodesStart = apRegions[iRegionIndex]->NumNodesCarNodes;	// Ped nodes are always stored *after* the car nodes
			const int iNodesEnd = iNodesStart + apRegions[iRegionIndex]->NumNodesPedNodes;	// 

			for(n=iNodesStart; n<iNodesEnd; n++)
			{
				CPathNode * pNode = &((apRegions[iRegionIndex]->aNodes)[n]);
				if(pNode && pNode->m_1.m_specialFunction==SPECIAL_PED_ASSISTED_MOVEMENT && pNode->NumLinks() ==1)
				{
					if(!CAssistedMovementRouteStore::ShouldProcessNode(pNode))
						continue;

					// Check that we've not visited this node already.
					if(pNode->m_distanceToTarget != PF_VERYLARGENUMBER)
					{
						continue;
					}

					Vector3 vNodePos;
					pNode->GetCoors(vNodePos);

					const float fDistSqr = (vNodePos - vOrigin).XYMag2();
					if(fDistSqr > fMaxDistSqr)
					{
						continue;
					}

					aiAssertf(((int)fDistSqr) < PF_VERYLARGENUMBER, "((int)fDistSqr) < PF_VERYLARGENUMBER");
					pNode->m_distanceToTarget = (s16)fDistSqr;

					//***************************************************************************************************
					// Bail out if we've hit the maximum number of nodes.  In practice we should never get 5000+ nodes
					// within the area we are considering unless something has borked up completely.

					nodesToBeCleared[iNumNodesToBeCleared++] = pNode->m_address;

					if(iNumNodesToBeCleared == MAX_NODES_TO_CLEAR)
					{
						aiAssertf(false, "We hit max num nodes (%i) in CPathFind::FindUniqueAssistedMovementRoutesInArea().. Oops!", MAX_NODES_TO_CLEAR);
						for(int a=0; a<MAX_NODES_TO_CLEAR; a++)
						{
							CNodeAddress & nodeAddr = nodesToBeCleared[a];
							aiAssertf(apRegions[nodeAddr.GetRegion()], "NULL node Region specified!");
							aiAssertf(apRegions[nodeAddr.GetRegion()]->aNodes, "NULL node Region specified!");
							((apRegions[nodeAddr.GetRegion()]->aNodes)[nodeAddr.GetIndex()]).m_distanceToTarget = PF_VERYLARGENUMBER;
						}
						return 0;
					}

					//******************************************************************************
					// There's a possibility that this node is the opposite end of the route.
					// Check that we've not already got a node pointer with this unique route ID.

					for(u=0; u<routes.GetCount(); u++)
					{
						if(routes[u].pEndNode->m_streetNameHash == pNode->m_streetNameHash)
						{
							routes[u].iNumEndsFound++;

							aiAssertf(routes[u].iNumEndsFound <= 2, "PedNodes : looks like there's multiple assisted-movement routes with the same \'street-name\' - see nodes at this position(%.1f,%.1f,%.1f)", vNodePos.x, vNodePos.y, vNodePos.z);

							break;
						}
					}

					// Not found?
					if(u==routes.GetCount())
					{
						if(routes.GetCount() == iMaxNumNodes)
						{
							aiAssertf(false, "We ran out of space for unique routes in CPathFind::FindUniqueAssistedMovementRoutesInArea().. Oops!");

							for(int a=0; a<iNumNodesToBeCleared; a++)
							{
								CNodeAddress & nodeAddr = nodesToBeCleared[a];
								aiAssertf(apRegions[nodeAddr.GetRegion()], "NULL node Region specified!");
								aiAssertf(apRegions[nodeAddr.GetRegion()]->aNodes, "NULL node Region specified!");
								((apRegions[nodeAddr.GetRegion()]->aNodes)[nodeAddr.GetIndex()]).m_distanceToTarget = PF_VERYLARGENUMBER;
							}

							for(int r=0; r<routes.GetCount(); r++)
							{
								pUniqueRouteNodes[r] = routes[r].pEndNode;	
							}

							return routes.GetCount();
						}

						// Now sort this node into the list, in order of proximity to vOrigin
						for(r=0; r<routes.GetCount(); r++)
						{
							CAssistedRouteInfo & info = routes[r];
							if(pNode->m_distanceToTarget < info.iDistSqr)
							{
								if(routes.GetCount()==routes.GetCapacity())
									routes.Delete(routes.GetCount()-1);

								CAssistedRouteInfo newInfo;
								newInfo.pEndNode = pNode;
								newInfo.iDistSqr = pNode->m_distanceToTarget;
								newInfo.iNumEndsFound = 1;
								routes.Append() = newInfo;
								break;
							}
						}
						if(r==routes.GetCount() && r!=routes.GetCapacity())
						{
							CAssistedRouteInfo newInfo;
							newInfo.pEndNode = pNode;
							newInfo.iDistSqr = pNode->m_distanceToTarget;
							newInfo.iNumEndsFound = 1;
							routes.Append() = newInfo;
						}
					}
				}
			}
		}
	}

	for(a=0; a<iNumNodesToBeCleared; a++)
	{
		CNodeAddress & nodeAddr = nodesToBeCleared[a];
		aiAssertf(apRegions[nodeAddr.GetRegion()], "NULL node Region specified!");
		aiAssertf(apRegions[nodeAddr.GetRegion()]->aNodes, "NULL node Region specified!");
		((apRegions[nodeAddr.GetRegion()]->aNodes)[nodeAddr.GetIndex()]).m_distanceToTarget = PF_VERYLARGENUMBER;
	}

	for(r=0; r<routes.GetCount(); r++)
	{
		pUniqueRouteNodes[r] = routes[r].pEndNode;	
	}

	return routes.GetCount();
}

bool RejectJunctionNodeCB(CPathNode * pPathNode, void * UNUSED_PARAM(data))
{
	if(pPathNode->m_2.m_qualifiesAsJunction)
		return false;
	return true;
}

bool CPathFind::GetPositionBySideOfRoad(const Vector3 & vNodePosition, const float fHeading, Vector3 & vOutPositionByRoad)
{
	const CNodeAddress iNode = FindNodeClosestToCoors(vNodePosition, RejectJunctionNodeCB, NULL, 30.0f);
	if(iNode.IsEmpty())
		return false;

	const CPathNode * pNode = FindNodePointerSafe(iNode);
	if(!pNode)
		return false;

	if(!pNode->NumLinks())
		return false;

	s32 iLinkIndex = ThePaths.FindBestLinkFromHeading(iNode, fHeading);
	if(iLinkIndex >= 0)
	{
		const CPathNodeLink & link = GetNodesLink(pNode, iLinkIndex);

		const CPathNode * pNextNode = FindNodePointerSafe(link.m_OtherNode);
		if(!pNextNode)
			return false;

		Vector3 vNode, vNextNode, vToNext, vRight;
		pNode->GetCoors(vNode);
		pNextNode->GetCoors(vNextNode);
		vToNext = vNextNode - vNode;
		vToNext.Normalize();
		vRight = CrossProduct(vToNext, ZAXIS);
		vRight.Normalize();

		const float fDistToSide = (link.GetLaneWidth() * ((float)link.m_1.m_LanesToOtherNode)) + link.InitialLaneCenterOffset();
		vOutPositionByRoad = vNode + (vRight * fDistToSide);
		return true;
	}

	return false;
}

bool CPathFind::GetPositionBySideOfRoad(const Vector3 & vNodePosition, const s32 iDirection, Vector3 & vOutPositionByRoad)
{
	const CNodeAddress iNode = FindNodeClosestToCoors(vNodePosition, RejectJunctionNodeCB, NULL, 30.0f);
	if(iNode.IsEmpty())
		return false;

	const CPathNode * pNode = FindNodePointerSafe(iNode);
	if(!pNode)
		return false;

	if(!pNode->NumLinks())
		return false;

	const CPathNodeLink & link = GetNodesLink(pNode, 0);

	const CPathNode * pNextNode = FindNodePointerSafe(link.m_OtherNode);
	if(!pNextNode)
		return false;

	Vector3 vNode, vNextNode, vToNext, vRight;
	pNode->GetCoors(vNode);
	pNextNode->GetCoors(vNextNode);
	vToNext = vNextNode - vNode;
	vToNext.Normalize();
	vRight = CrossProduct(vToNext, ZAXIS);
	vRight.Normalize();

	bool bFound[2] = { false, false };
	Vector3 vPositions[2] = { vNodePosition, vNodePosition };

	if(link.m_1.m_LanesToOtherNode)
	{
		const float fDistToSide = (link.GetLaneWidth() * ((float)link.m_1.m_LanesToOtherNode)) + link.InitialLaneCenterOffset();
		vPositions[0] = vNode + (vRight * fDistToSide);
		bFound[0] = true;
	}
	if(link.m_1.m_LanesFromOtherNode)
	{
		const float fDistToSide = (link.GetLaneWidth() * ((float)link.m_1.m_LanesFromOtherNode)) + link.InitialLaneCenterOffset();
		vPositions[1] = vNode - (vRight * fDistToSide);
		bFound[1] = true;
	}

	if(iDirection==0)
	{
		vOutPositionByRoad = vPositions[0];
		return bFound[0];
	}
	if(iDirection==1)
	{
		vOutPositionByRoad = vPositions[1];
		return bFound[1];
	}

	Assert(iDirection==-1);
	const int iIndex = (bFound[0] && bFound[1]) ?
		(s32)fwRandom::GetRandomTrueFalse() : bFound[0] ? 0 : 1;

	vOutPositionByRoad = vPositions[iIndex];
	return bFound[iIndex];
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeClosestToCoors
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFind::FindNodeClosestToCoors(
	const Vector3& SearchCoors,
	float CutoffDist,
	NodeConsiderationType eNodeConsiderationType,
	bool bIgnoreAlreadyFound,
	bool bBoatNodes,
	bool bHidingNodes,
	bool bNetworkRestart,
	bool bMatchDirection,
	float dirToMatchX,
	float dirToMatchY,
	float zMeasureMult,
	float zTolerance,
	s32 mapAreaIndex,
	bool bForGps,
	bool bIgnoreOneWayRoads,
	const CVehicleNodeList* pPreferredNodeList,
	const bool bIgnoreSlipLanes,
	const bool bIgnoreSwitchedOffDeadEnds) const
{
	s32			CenterRegionX, CenterRegionY, RegionX, RegionY;
	s32			Area;
	CNodeAddress	ClosestNode;
	float			ClosestDist = CutoffDist, DistToBorderOfRegion;
	u16			Region;

	ClosestNode.SetEmpty();

	// To optimise this 
	// We start with the Region that the Searchcoors are within.
	// If we don't find a node or there is a possibility of finding an even
	// closer node we continue to look in ever expanding areas.

	// Find the X and Y of the Region we're in.
	CenterRegionX = FindXRegionForCoors(SearchCoors.x);
	CenterRegionY = FindYRegionForCoors(SearchCoors.y);
	Region = (u16)(FIND_REGION_INDEX(CenterRegionX, CenterRegionY));
	DistToBorderOfRegion = SearchCoors.x - FindXCoorsForRegion(CenterRegionX);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindXCoorsForRegion(CenterRegionX+1) - SearchCoors.x);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, SearchCoors.y - FindYCoorsForRegion(CenterRegionY));
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindYCoorsForRegion(CenterRegionY+1) - SearchCoors.y);
	FindNodeClosestInRegion(&ClosestNode, Region, SearchCoors, &ClosestDist, eNodeConsiderationType, bIgnoreAlreadyFound
		, bBoatNodes, bHidingNodes, bNetworkRestart, bMatchDirection, dirToMatchX, dirToMatchY, zMeasureMult, zTolerance, mapAreaIndex
		, bForGps, bIgnoreOneWayRoads, CutoffDist, pPreferredNodeList, bIgnoreSlipLanes, bIgnoreSwitchedOffDeadEnds);

	// If we cannot possibly find a closer node we stop here.
	if (ClosestDist > DistToBorderOfRegion)
	{
		// Expand the search to a wider area. (The border of a square)
		for (Area = 1; Area < 5; Area++)
		{
			RegionX = CenterRegionX - Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist
							, eNodeConsiderationType, bIgnoreAlreadyFound, bBoatNodes, bHidingNodes, bNetworkRestart, bMatchDirection
							, dirToMatchX, dirToMatchY, zMeasureMult, zTolerance, mapAreaIndex, bForGps, bIgnoreOneWayRoads, CutoffDist
							, pPreferredNodeList, bIgnoreSlipLanes, bIgnoreSwitchedOffDeadEnds);
					}
				}
			}
			RegionX = CenterRegionX + Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist
							, eNodeConsiderationType, bIgnoreAlreadyFound, bBoatNodes, bHidingNodes, bNetworkRestart, bMatchDirection
							, dirToMatchX, dirToMatchY, zMeasureMult, zTolerance, mapAreaIndex, bForGps, bIgnoreOneWayRoads, CutoffDist
							, pPreferredNodeList, bIgnoreSlipLanes, bIgnoreSwitchedOffDeadEnds);
					}
				}
			}

			RegionY = CenterRegionY - Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist
							, eNodeConsiderationType, bIgnoreAlreadyFound, bBoatNodes, bHidingNodes, bNetworkRestart, bMatchDirection
							, dirToMatchX, dirToMatchY, zMeasureMult, zTolerance, mapAreaIndex, bForGps, bIgnoreOneWayRoads, CutoffDist
							, pPreferredNodeList, bIgnoreSlipLanes, bIgnoreSwitchedOffDeadEnds);
					}
				}
			}
			RegionY = CenterRegionY + Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist
							, eNodeConsiderationType, bIgnoreAlreadyFound, bBoatNodes, bHidingNodes, bNetworkRestart, bMatchDirection
							, dirToMatchX, dirToMatchY, zMeasureMult, zTolerance, mapAreaIndex, bForGps, bIgnoreOneWayRoads, CutoffDist
							, pPreferredNodeList, bIgnoreSlipLanes, bIgnoreSwitchedOffDeadEnds);
					}
				}
			}

			DistToBorderOfRegion += rage::Min(PATHFINDREGIONSIZEX, PATHFINDREGIONSIZEY);
			// If we cannot possibly find a closer node we stop here.
			if (ClosestDist < DistToBorderOfRegion) break;
		}
	}

	return(ClosestNode);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeClosestToCoors
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFind::FindNodeClosestToCoors(const Vector3& SearchCoors, FindClosestNodeCB callbackFunction, void * pData, float CutoffDistForNodeSearch, bool bForGps) const
{
	s32			CenterRegionX, CenterRegionY, RegionX, RegionY;
	s32			Area;
	CNodeAddress	ClosestNode;
	float			ClosestDist = CutoffDistForNodeSearch, DistToBorderOfRegion;
	u16			Region;

	ClosestNode.SetEmpty();

	// To optimise this 
	// We start with the Region that the Searchcoors are within.
	// If we don't find a node or there is a possibility of finding an even
	// closer node we continue to look in ever expanding areas.

	// Find the X and Y of the Region we're in.
	CenterRegionX = FindXRegionForCoors(SearchCoors.x);
	CenterRegionY = FindYRegionForCoors(SearchCoors.y);
	Region = (u16)(FIND_REGION_INDEX(CenterRegionX, CenterRegionY));
	DistToBorderOfRegion = SearchCoors.x - FindXCoorsForRegion(CenterRegionX);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindXCoorsForRegion(CenterRegionX+1) - SearchCoors.x);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, SearchCoors.y - FindYCoorsForRegion(CenterRegionY));
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindYCoorsForRegion(CenterRegionY+1) - SearchCoors.y);

	FindNodeClosestInRegion(&ClosestNode, Region, SearchCoors, &ClosestDist, callbackFunction, pData, CutoffDistForNodeSearch, bForGps);

	// This first function only checks for nodes in the relevant cells assuming we have found a node
	// The second one expand the search one border of a square each search, we don't need to search the full border if we assume we found something!
	
	static const bool bFasterFindNodeToCoord = true;
	if (bFasterFindNodeToCoord && ClosestDist < rage::Max(PATHFINDREGIONSIZEX, PATHFINDREGIONSIZEY))
	{
		CenterRegionX = FindXRegionForCoors(SearchCoors.x);
		CenterRegionY = FindYRegionForCoors(SearchCoors.y);

		s32 RegionToRight = FindXRegionForCoors(SearchCoors.x + ClosestDist);
		s32 RegionToLeft = FindXRegionForCoors(SearchCoors.x - ClosestDist);
		s32 RegionToTop = FindYRegionForCoors(SearchCoors.y - ClosestDist);
		s32 RegionToBottom = FindYRegionForCoors(SearchCoors.y + ClosestDist);

		for (int y = RegionToTop; y <= RegionToBottom; ++y)
		{
			for (int x = RegionToLeft; x <= RegionToRight; ++x)
			{
				if (CenterRegionX != x || CenterRegionY != y)
				{
					FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(x, y)), SearchCoors, &ClosestDist, callbackFunction, pData, CutoffDistForNodeSearch, bForGps);
				}
			}
		}
	}
	
	if (ClosestNode.IsEmpty()/* || ClosestDist > DistToBorderOfRegion*/) // If we cannot possibly find a closer node we stop here.
	{
		// Expand the search to a wider area. (The border of a square)
		for (Area = 1; Area < 5; Area++)
		{
			RegionX = CenterRegionX - Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist, callbackFunction, pData, CutoffDistForNodeSearch, bForGps);
					}
				}
			}
			RegionX = CenterRegionX + Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist, callbackFunction, pData, CutoffDistForNodeSearch, bForGps);
					}
				}
			}

			RegionY = CenterRegionY - Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist, callbackFunction, pData, CutoffDistForNodeSearch, bForGps);
					}
				}
			}
			RegionY = CenterRegionY + Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						FindNodeClosestInRegion(&ClosestNode, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), SearchCoors, &ClosestDist, callbackFunction, pData, CutoffDistForNodeSearch, bForGps);
					}
				}
			}

			DistToBorderOfRegion += rage::Min(PATHFINDREGIONSIZEX, PATHFINDREGIONSIZEY);
			// If we cannot possibly find a closer node we stop here.
			if (ClosestDist < DistToBorderOfRegion) break;
		}
	}

	return(ClosestNode);
}

// Start the Async Node Search
void CPathFind::StartAsyncGPSSearch(FindNodeClosestToCoorsAsyncParams* params)
{
	// Pull in pathfind parameters
	sysMemCpy(params->m_gpsDisabledZones, m_GPSDisabledZones, sizeof(m_GPSDisabledZones));
	
	// Set up the sysTaskParams with our input and output data
	sysTaskParameters taskParams;
	taskParams.Input.Data = params;
	taskParams.Input.Size = (sizeof(FindNodeClosestToCoorsAsyncParams) + 15) & ~15;

	taskParams.Output.Data = params->m_SearchData;
	taskParams.Output.Size = sizeof(CPathFind::TFindClosestNodeLinkGPS);

#if __PS3
	int readOnlyCount = 0;
	taskParams.ReadOnly[readOnlyCount].Data	= ThePaths.apRegions;
	taskParams.ReadOnly[readOnlyCount].Size	= ((sizeof(CPathRegion*) * PATHFINDREGIONS) + 15) & ~15;
	++readOnlyCount;

	taskParams.ReadOnly[readOnlyCount].Data = params->m_SearchData;
	taskParams.ReadOnly[readOnlyCount].Size = ((sizeof(CPathFind::TFindClosestNodeLinkGPS) + 15) &~15);
	++readOnlyCount;

	taskParams.ReadOnlyCount = readOnlyCount;

	int schedulerIndex = sysTaskManager::SCHEDULER_DEFAULT_LONG;
#else // __PS3
	int schedulerIndex = 0;
#endif // __PS3

#if __PS3
	taskParams.Scratch.Size = 180*1024;// large scratch size - might be able to be smaller, but this job can pull in a lot of data if a region has a lot of path nodes (sometimes 4K nodes or more at 32 bytes each)
#endif // __PS3


	params->m_TaskHandle = sysTaskManager::Create(TASK_INTERFACE(PathfindNodeSearchSPU), taskParams, schedulerIndex);
}



// NAME : ForAllNodesInArea
// PURPOSE : Invoke callback function for all vehicle nodes in the given area
void CPathFind::ForAllNodesInArea(const Vector3 & vMin, const Vector3 & vMax, ForAllNodesCB callbackFunction, void * pData)
{
	const s32 iRegionMinX = FindXRegionForCoors(vMin.x);
	const s32 iRegionMinY = FindYRegionForCoors(vMin.y);
	const s32 iRegionMaxX = FindXRegionForCoors(vMax.x)+1;
	const s32 iRegionMaxY = FindYRegionForCoors(vMax.y)+1;

	Vector3 vNode;

	for(int ry=iRegionMinY; ry<iRegionMaxY; ry++)
	{
		for(int rx=iRegionMinX; rx<iRegionMaxX; rx++)
		{
			const s32 iRegion = FIND_REGION_INDEX(rx, ry);
			if(iRegion < 0 || iRegion >= PATHFINDREGIONS)
				continue;
			CPathRegion * pRegion = apRegions[iRegion];
			if(!pRegion)
				continue;

			s32 iStartNode, iEndNode;
			FindStartAndEndNodeBasedOnYRange(iRegion, vMin.y, vMax.y, iStartNode, iEndNode);

			for(s32 n=iStartNode; n<iEndNode; n++)
			{
				pRegion->aNodes[n].GetCoors(vNode);
				if(vNode.x >= vMin.x && vNode.y >= vMin.y && vNode.z >= vMin.z &&
					vNode.x < vMax.x && vNode.y < vMax.y && vNode.z < vMax.z)
				{
					if(!callbackFunction(&pRegion->aNodes[n], pData))
						return;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNthNodeClosestToCoors
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFind::FindNthNodeClosestToCoors(const Vector3& SearchCoors, float CutoffDist, bool bIgnoreSwitchedOff, s32 N, bool bWaterNode
	, CNodeAddress *pNMinus1th, bool bMatchDirection, float dirToMatchX, float dirToMatchY, float zMeasureMult, float zTolerance, s32 mapAreaIndex
	, const bool bIgnoreSlipLanes, const bool bIgnoreSwitchedOffDeadEnds)
{
	NodeConsiderationType eNodeConsiderationType = bIgnoreSwitchedOff ? IgnoreSwitchedOffNodes : IncludeSwitchedOffNodes;
	s32			Node, StartNode=0, EndNode=0, Region;
	CNodeAddress	NodeFound;

	if (bMatchDirection)
	{
		float Length = rage::Sqrtf(dirToMatchX * dirToMatchX + dirToMatchY * dirToMatchY);
		if (Length != 0.0f)
		{
			dirToMatchX /= Length;
			dirToMatchY /= Length;
		}
		else
		{
			dirToMatchX = 1.0f;
		}
	}

	for (Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	{
		if (apRegions[Region] && apRegions[Region]->aNodes)		// Make sure this Region is loaded in.
		{
			StartNode = 0;
			EndNode = apRegions[Region]->NumNodesCarNodes;

			for (Node = StartNode; Node < EndNode; Node++)
			{
				CPathNode	*pNode = &((apRegions[Region]->aNodes)[Node]);
				pNode->m_2.m_alreadyFound = false;
			}
		}
	}

	// The following while loop marks the nodes starting with the closest and moving up.
	while (N > 0)
	{
		NodeFound = FindNodeClosestToCoors(SearchCoors, CutoffDist, eNodeConsiderationType, true, bWaterNode, false, false
			, bMatchDirection, dirToMatchX, dirToMatchY, zMeasureMult, zTolerance, mapAreaIndex, false, false, NULL, bIgnoreSlipLanes, bIgnoreSwitchedOffDeadEnds);
		if (pNMinus1th)
		{
			*pNMinus1th = NodeFound;
		}

		if (NodeFound.IsEmpty())
		{
			return NodeFound;
		}
		else
		{
			FindNodePointer(NodeFound)->m_2.m_alreadyFound = true;
		}
		N--;
	}

	return ( FindNodeClosestToCoors(SearchCoors, CutoffDist, eNodeConsiderationType, true, bWaterNode, false, false, bMatchDirection
		, dirToMatchX, dirToMatchY, zMeasureMult, zTolerance, mapAreaIndex, false, false, NULL, bIgnoreSlipLanes, bIgnoreSwitchedOffDeadEnds) );
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNextNodeClosestToCoors
// PURPOSE :  This is a version of the function above (FindNthNodeClosestToCoors)
//			  that will return the nodes in order. So for the script things will be faster if you call
//			  FindNthNodeClosestToCoors(100)
//			  FindNextNodeClosestToCoors()
//			  FindNextNodeClosestToCoors()
//			  Instead of
//			  FindNthNodeClosestToCoors(100)
//			  FindNthNodeClosestToCoors(101)
//			  FindNthNodeClosestToCoors(102)
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFind::FindNextNodeClosestToCoors(const Vector3& SearchCoors, float CutoffDist, bool bIgnoreSwitchedOff, bool bWaterNode, bool bMatchDirection, float dirToMatchX, float dirToMatchY, float zMeasureMult, float zTolerance, s32 mapAreaIndex)
{
	CNodeAddress	NodeFound;

	if (bMatchDirection)
	{
		float Length = rage::Sqrtf(dirToMatchX * dirToMatchX + dirToMatchY * dirToMatchY);
		if (Length != 0.0f)
		{
			dirToMatchX /= Length;
			dirToMatchY /= Length;
		}
		else
		{
			dirToMatchX = 1.0f;
		}
	}

	NodeConsiderationType eNodeConsiderationType = bIgnoreSwitchedOff ? IgnoreSwitchedOffNodes : IncludeSwitchedOffNodes;
	NodeFound = FindNodeClosestToCoors(SearchCoors, CutoffDist, eNodeConsiderationType, true, bWaterNode, false, false, bMatchDirection, dirToMatchX, dirToMatchY, zMeasureMult, zTolerance, mapAreaIndex);

	if (NodeFound.IsEmpty())
	{
		return NodeFound;
	}
	else
	{
		FindNodePointer(NodeFound)->m_2.m_alreadyFound = true;
	}

	return ( NodeFound );
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SnapCoorsToNearestNode
// PURPOSE :  If there is a node to be found this function will output the coordinates
//			  of it.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::SnapCoorsToNearestNode(const Vector3& SearchCoors, Vector3& Result, float CutoffDist)
{
	CNodeAddress nearestNodesAddr = FindNodeClosestToCoors(SearchCoors, CutoffDist);

	if(nearestNodesAddr.IsEmpty())
	{
		Result = SearchCoors;
		return false;
	}

	FindNodePointer(nearestNodesAddr)->GetCoors(Result);

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Find2NodesForCarCreation
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::Find2NodesForCarCreation(const Vector3& SearchCoors, CNodeAddress* ClosestNode, CNodeAddress* OtherNode, bool bIgnoreSwitchedOff)
{
#define NUM_TO_CONSIDER (4)

	CNodeAddress	aNodes[4];

	RecordNodesClosestToCoors(SearchCoors, NUM_TO_CONSIDER, aNodes, FLT_MAX, bIgnoreSwitchedOff, false);

	if (aNodes[0].IsEmpty())
	{
		ClosestNode->SetEmpty();
		OtherNode->SetEmpty();
		return;
	}

	*ClosestNode = aNodes[0];

	for (s32 C = 1; C < NUM_TO_CONSIDER; C++)
	{
		if (!aNodes[C].IsEmpty())
		{
			if (!These2NodesAreAdjacent(*ClosestNode, aNodes[C]))
			{
				*OtherNode = aNodes[C];
				return;
			}	
		}
	}
	OtherNode = ClosestNode;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : These2NodesAreAdjacent
// PURPOSE :  Returns true if these nodes are directly adjacent.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::These2NodesAreAdjacent(CNodeAddress Node1, CNodeAddress Node2) const
{
	const CPathNode *pNode = FindNodePointer(Node1);

	for(u32 Link = 0; Link < pNode->NumLinks(); Link++)
	{
		if(GetNodesLinkedNodeAddr(pNode, Link) == Node2)
		{
			return true;
		}
	}
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : FindNearestGroundHeightForBuildPaths
// PURPOSE :  Given a point in space this function returns the nearest collision with the map but only
//			  in a straight line up or down
//
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::FindNearestGroundHeightForBuildPaths(const Vector3 &posn, float range, u32* pRoomId, phMaterialMgr::Id * pMaterialIdOut)
{
	static bool bUseTopHitBelowThreshold = true;

	phIntersection	result;
	float	heightFound = posn.z;
	bool	bHitFound = false;
	u32		roomFound = 0;

	phSegment testSegment(Vector3(posn.x, posn.y, posn.z + range), Vector3(posn.x, posn.y, posn.z - range) );
	CPhysics::GetLevel()->TestProbe(testSegment, &result, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES);

	while (result.IsAHit())
	{
		if(bUseTopHitBelowThreshold && (result.GetPosition().GetZf() - posn.z < sm_fAboveThresholdForPathProbes) && (result.GetPosition().GetZf() - posn.z > 0.0f))
		{
			heightFound = result.GetPosition().GetZf();
			roomFound = PGTAMATERIALMGR->UnpackRoomId(result.GetMaterialId());

			if(pMaterialIdOut)
			{
				*pMaterialIdOut = result.GetMaterialId();
			}

			break;
		}
		else if (!bHitFound)
		{
			heightFound = result.GetPosition().GetZf();
			roomFound = PGTAMATERIALMGR->UnpackRoomId(result.GetMaterialId());

			if(pMaterialIdOut)
			{
				*pMaterialIdOut = result.GetMaterialId();
			}
		}
		else
		{
			if (rage::Abs(result.GetPosition().GetZf() - posn.z) < rage::Abs(heightFound - posn.z))
			{
				heightFound = result.GetPosition().GetZf();
				roomFound = PGTAMATERIALMGR->UnpackRoomId(result.GetMaterialId());

				if(pMaterialIdOut)
				{
					*pMaterialIdOut = result.GetMaterialId();
				}
			}
			else
			{
				break;
			}
		}
		bHitFound = true;

		phSegment testSegment2(Vector3(posn.x, posn.y, heightFound - 0.05f), Vector3(posn.x, posn.y, posn.z - range) );
		CPhysics::GetLevel()->TestProbe(testSegment2, &result, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES);
	}


	if (pRoomId && roomFound != 0){
		*pRoomId = roomFound;
	}

	return heightFound;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindRegionLinkIndexBetween2Nodes
// PURPOSE :  Returns true if these nodes are directly adjacent.
/////////////////////////////////////////////////////////////////////////////////
s16 CPathFind::FindRegionLinkIndexBetween2Nodes(CNodeAddress NodeFrom, CNodeAddress NodeTo) const
{
	const CPathNode* pNode = FindNodePointer(NodeFrom);

	for(s16 Link = 0; Link < pNode->NumLinks(); Link++)
	{
		if(GetNodesLinkedNodeAddr(pNode, Link) == NodeTo)
		{
			Assert((static_cast<s32>(pNode->m_startIndexOfLinks) + static_cast<s32>(Link)) < MAXLINKSPERREGION);
			return pNode->m_startIndexOfLinks + Link;
		}
	}
	Assert(0);
	return -1;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodesLinkIndexBetween2Nodes
// PURPOSE :  Returns true if these nodes are directly adjacent.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::FindNodesLinkIndexBetween2Nodes(const CPathNode * pNodeFrom, const CPathNode * pNodeTo, s16 & iLinkIndex) const
{
	for(s16 l = 0; l < pNodeFrom->NumLinks(); l++)
	{
		const CNodeAddress iLinkedNode = GetNodesLinkedNodeAddr(pNodeFrom, l);
		if(iLinkedNode == pNodeTo->m_address)
		{
			iLinkIndex = l;
			return true;
		}
	}
	iLinkIndex = -1;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindLinkBetween2Nodes
// PURPOSE :  Returns the link between these two nodes if it exists.
/////////////////////////////////////////////////////////////////////////////////
const CPathNodeLink * CPathFind::FindLinkBetween2Nodes(const CPathNode * pNodeFrom, const CPathNode * pNodeTo) const
{
	if (!aiVerify(pNodeFrom && pNodeTo))
	{
		return NULL;
	}

	for(s16 l = 0; l < pNodeFrom->NumLinks(); l++)
	{
		const CNodeAddress iLinkedNode = GetNodesLinkedNodeAddr(pNodeFrom, l);
		if(iLinkedNode == pNodeTo->m_address)
		{
			return &GetNodesLink(pNodeFrom,l);
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodesLinkIndexBetween2Nodes
// PURPOSE :  Returns true if these nodes are directly adjacent.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::FindNodesLinkIndexBetween2Nodes(const CNodeAddress &iNodeFrom, const CNodeAddress &iNodeTo, s16 & iLinkIndexOut) const
{
	const CPathNode * pNodeFrom = FindNodePointerSafe(iNodeFrom);
	if (!pNodeFrom)
	{
		iLinkIndexOut = -1;
		return false;
	}

	for(s16 l = 0; l < pNodeFrom->NumLinks(); l++)
	{
		const CNodeAddress iLinkedNode = GetNodesLinkedNodeAddr(pNodeFrom, l);
		if(iLinkedNode == iNodeTo)
		{
			iLinkIndexOut = l;
			return true;
		}
	}

	iLinkIndexOut = -1;
/* Debug code to try and find a problem we were having with m_region in iNodeTo not being retrieved properly.
	const CPathNode * pNodeTo = ThePaths.FindNodePointerSafe(iNodeTo);
	Assert(iNodeTo == pNodeTo->m_address);
	for(s32 l = 0; l < pNodeFrom->NumLinks(); l++)
	{
		const CNodeAddress iLinkedNode = GetNodesLinkedNodeAddr(pNodeFrom, l);

		Assertf(false, "iLinkIndexOut.region %d, .index %d , iNodeTo.region %d, .index %d ", 
			iLinkedNode.GetRegion(),
			iLinkedNode.GetIndex(),
			iNodeTo.GetRegion(),
			iNodeTo.GetIndex());

		if(iLinkedNode == iNodeTo)
		{
			iLinkIndexOut = l;
			return true;
		}

		if(iLinkedNode == pNodeTo->m_address)
		{
			iLinkIndexOut = l;
			Assert(iLinkedNode == iNodeTo);
			Assertf(false, "pNodeTo->m_address.region %d, .index %d, iLinkIndexOut.region %d, .index %d , iNodeTo.region %d, .index %d ", 
				pNodeTo->m_address.GetRegion(), 
				pNodeTo->m_address.GetIndex(), 
				iLinkedNode.GetRegion(),
				iLinkedNode.GetIndex(),
				iNodeTo.GetRegion(),
				iNodeTo.GetIndex());

			return true;
		}
	}

	iLinkIndexOut = -1;
*/
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodesLinkIndexBetween2Nodes
// PURPOSE :  Returns true if these nodes are directly adjacent.
/////////////////////////////////////////////////////////////////////////////////
s16 CPathFind::FindNodesLinkIndexBetween2Nodes(CNodeAddress iNodeFrom, CNodeAddress iNodeTo) const
{
	Assertf(false, "FindNodesLinkIndexBetween2Nodes(CNodeAddress iNodeFrom, CNodeAddress iNodeTo) is deprecated use FindNodesLinkIndexBetween2Nodes(CNodeAddress iNodeFrom, CNodeAddress iNodeTo, s32 & iLinkIndexOut)");
	
	const CPathNode * pNodeFrom = FindNodePointerSafe(iNodeFrom);

	if (!pNodeFrom)
	{
		return -1;
	}

	for(s16 l = 0; l < pNodeFrom->NumLinks(); l++)
	{
		const CNodeAddress iLinkedNode = GetNodesLinkedNodeAddr(pNodeFrom, l);
		if(iLinkedNode == iNodeTo)
		{
			return l;
		}
	}

#if __ASSERT
	Vector3 vPos;
	pNodeFrom->GetCoors(vPos);
	if(IsRegionLoaded(iNodeTo))
	{
		const CPathNode * pNodeTo = FindNodePointerSafe(iNodeTo);
		if(pNodeTo)
		{
			Vector3 vPosTo;
			pNodeTo->GetCoors(vPosTo);
			Assertf(false, "FindNodesLinkIndexBetween2Nodes() - link not found from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f)", vPos.x, vPos.y, vPos.z, vPosTo.x, vPosTo.y, vPosTo.z);
		}
		else
		{
			Assertf(false, "FindNodesLinkIndexBetween2Nodes() - link not found from (%.1f, %.1f, %.1f)", vPos.x, vPos.y, vPos.z);
		}
	}
	else
	{
		Assertf(false, "FindNodesLinkIndexBetween2Nodes() - link not found from (%.1f, %.1f, %.1f)", vPos.x, vPos.y, vPos.z);
	}
#endif
	return -1;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindCarGenerationCoordinatesBetween2Nodes
// PURPOSE :  Give an old and new node this function generates an appropriate car
//			  generation coordinate.
/////////////////////////////////////////////////////////////////////////////////
// DEBUG!! -AC, This only every uses the first lane...
void CPathFind::FindCarGenerationCoordinatesBetween2Nodes(CNodeAddress nodeFrom, CNodeAddress nodeTo, Matrix34 *pMatrix) const
{
	const CPathNode *pNodeFrom = FindNodePointer(nodeFrom);
	const CPathNode *pNodeTo = FindNodePointer(nodeTo);

	Vector3 fromCoors,toCoors;
	pNodeFrom->GetCoors(fromCoors);
	pNodeTo->GetCoors(toCoors);

	pMatrix->d = (fromCoors + toCoors) * 0.5f;
	pMatrix->b = toCoors - fromCoors;
	pMatrix->b.Normalize();

	pMatrix->a.x = pMatrix->b.y;
	pMatrix->a.y = -pMatrix->b.x;
	pMatrix->a.z = 0.0f;
	pMatrix->a.Normalize();

	pMatrix->c.Cross(pMatrix->a, pMatrix->b);

	s16 iLink = -1;
	const bool bLinkFound = FindNodesLinkIndexBetween2Nodes(nodeFrom, nodeTo, iLink);
	if(!bLinkFound)//can't find the link so just return
	{
		return;
	}

	const CPathNodeLink *pLink = &GetNodesLink(pNodeFrom, iLink);

	// Take into account the width of the hard shoulder.
#if __DEV
	pMatrix->d +=(Vector3(pMatrix->b.y, -pMatrix->b.x, 0.0f)*(bMakeAllRoadsSingleTrack?0.0f:pLink->InitialLaneCenterOffset()));
#else
	pMatrix->d +=(Vector3(pMatrix->b.y, -pMatrix->b.x, 0.0f)*(pLink->InitialLaneCenterOffset()));
#endif // __DEV
}
// END DEBUG!!

s32 CPathFind::FindNumberNonShortcutLinksForNode(const CPathNode* pNode) const
{
	s32	result = 0;

	for(u32 i = 0; i < pNode->NumLinks(); i++)
	{
		const CPathNodeLink* pLink = &GetNodesLink(pNode, i);
		if (!pLink->IsShortCut())
		{
			++result;
		}
	}

	return result;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindRandomNodeNearKnownOne
// PURPOSE :  Tries to find another node x nodes from this one. It may not get as
//			  far as that and it may return a closer node (or the original node itself)
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFind::FindRandomNodeNearKnownOne(const CNodeAddress oldNode, const s32 steps, const CNodeAddress avoidNode)
{
	if ( IsRegionLoaded(oldNode) )
	{
		const CPathNode *pNode = FindNodePointer(oldNode);

		s32 link = fwRandom::GetRandomNumberInRange(0, pNode->NumLinks());
		s32 tries = pNode->NumLinks();

		while (tries > 0)
		{
			CNodeAddress testNode = GetNodesLinkedNodeAddr(pNode, link);

			if (testNode != avoidNode)
			{
				if (IsRegionLoaded(testNode))
				{
					const CPathNode *ptestNode = FindNodePointer(testNode);

					if (!ptestNode->HasSpecialFunction_Driving())
					{
						// This node will do. Carry on searching further if needed though.
						if (steps <= 1)
						{
							return testNode;
						}
						else
						{
							return FindRandomNodeNearKnownOne(testNode, steps-1, oldNode);
						}
					}
				}
			}

			link =(link+1)% pNode->NumLinks();
			tries--;
		}
	}
	return oldNode;	// We were not successful find another node to jump to. Just return our start point.
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RecordNodesClosestToCoors
// PURPOSE :  Builds a little list of all the nodes within a certain range.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::RecordNodesClosestToCoors(const Vector3& SearchCoors, s32 NoOfNodes, CNodeAddress* ClosestNodes, float CutoffDistSq, bool bIgnoreSwitchedOff, bool bWaterNode)
{
	PF_FUNC(RecordNodesClosestToCoors);

#define MAX_NODES_FOR_RECORD	(32)
	float ClosestNodesDistSq[MAX_NODES_FOR_RECORD];
	Assert(NoOfNodes < MAX_NODES_FOR_RECORD);

	for (int i = 0; i < NoOfNodes; i++)
	{
		ClosestNodes[i].SetEmpty();
		ClosestNodesDistSq[i] = FLT_MAX;
	}

	// To optimise this 
	// We start with the Region that the Searchcoors are within.
	// If we don't find a node or there is a possibility of finding an even
	// closer node we continue to look in ever expanding areas.

	// Find the X and Y of the Region we're in.
	s32 CenterRegionX = FindXRegionForCoors(SearchCoors.x);
	s32 CenterRegionY = FindYRegionForCoors(SearchCoors.y);
	u16 Region = (u16)(FIND_REGION_INDEX(CenterRegionX, CenterRegionY));
	float DistToBorderOfRegion = SearchCoors.x - FindXCoorsForRegion(CenterRegionX);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindXCoorsForRegion(CenterRegionX+1) - SearchCoors.x);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, SearchCoors.y - FindYCoorsForRegion(CenterRegionY));
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindYCoorsForRegion(CenterRegionY+1) - SearchCoors.y);
	RecordNodesClosestToCoors(SearchCoors, Region, NoOfNodes, ClosestNodes, ClosestNodesDistSq, CutoffDistSq, bIgnoreSwitchedOff, bWaterNode);

	// If we cannot possibly find a closer node we stop here.
	if (CutoffDistSq > square(DistToBorderOfRegion))
	{
		// Expand the search to a wider area. (The border of a square)
		for (s32 Area = 1; Area < 5; Area++)
		{
			s32 RegionX = CenterRegionX - Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (s32 RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						RecordNodesClosestToCoors(SearchCoors, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), NoOfNodes, ClosestNodes, ClosestNodesDistSq, CutoffDistSq, bIgnoreSwitchedOff, bWaterNode);
					}
				}
			}
			RegionX = CenterRegionX + Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (s32 RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						RecordNodesClosestToCoors(SearchCoors, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), NoOfNodes, ClosestNodes, ClosestNodesDistSq, CutoffDistSq, bIgnoreSwitchedOff, bWaterNode);
					}
				}
			}

			s32 RegionY = CenterRegionY - Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						RecordNodesClosestToCoors(SearchCoors, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), NoOfNodes, ClosestNodes, ClosestNodesDistSq, CutoffDistSq, bIgnoreSwitchedOff, bWaterNode);
					}
				}
			}
			RegionY = CenterRegionY + Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						RecordNodesClosestToCoors(SearchCoors, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), NoOfNodes, ClosestNodes, ClosestNodesDistSq, CutoffDistSq, bIgnoreSwitchedOff, bWaterNode);
					}
				}
			}

			DistToBorderOfRegion += rage::Min(PATHFINDREGIONSIZEX, PATHFINDREGIONSIZEY);
			// If we cannot possibly find a closer node we stop here.
			if (CutoffDistSq < square(DistToBorderOfRegion)) break;
		}
	}
}

void CPathFind::RecordNodesClosestToCoors(const Vector3& SearchCoors, u16 Region, s32 NoOfNodes, CNodeAddress* ClosestNodes, float* ClosestNodesDistSq, float& CutoffDistSq, bool bIgnoreSwitchedOff, bool bWaterNode)
{
	if(!apRegions[Region] || !apRegions[Region]->aNodes)		// Make sure this Region is loaded in.
	{
		return;
	}

	s32 StartNode = 0;
	s32 EndNode = apRegions[Region]->NumNodesCarNodes;

	for (s32 Node = StartNode; Node < EndNode; Node++)
	{
		CPathNode	*pNode = &(apRegions[Region]->aNodes[Node]);

		if(bWaterNode != static_cast<bool>(pNode->m_2.m_waterNode))
		{
			continue;
		}

		if(bIgnoreSwitchedOff && pNode->m_2.m_switchedOff)
		{
			continue;
		}

		if(pNode->m_1.m_specialFunction == SPECIAL_HIDING_NODE)
		{
			continue;
		}

		Vector3		crs;
		pNode->GetCoors(crs);

		float	distSq = (crs - SearchCoors).Mag2();
		if(distSq >= CutoffDistSq)
		{
			continue;
		}

		// Find out what slot to put it in:
		s32 slot = NoOfNodes;
		while (slot > 0 && distSq < ClosestNodesDistSq[slot-1])
		{
			slot--;
		}
		if (slot < NoOfNodes)
		{
			// This node is indeed close enough to be recorded at position 'slot'

			// Make room for it by shifting everything up one spot.
			for (s32 slotshift = NoOfNodes-1; slotshift > slot; slotshift--)
			{
				ClosestNodes[slotshift] = ClosestNodes[slotshift-1];
				ClosestNodesDistSq[slotshift] = ClosestNodesDistSq[slotshift-1];
			}
			ClosestNodes[slot] = pNode->m_address;
			ClosestNodesDistSq[slot] = distSq;

			//Update the cut-off distance.
			CutoffDistSq = Min(CutoffDistSq, ClosestNodesDistSq[NoOfNodes - 1]);
		}
	}
}

s32 CPathFind::RecordNodesInCircle(const Vector3& Centre, const float Radius, s32 NoOfNodes, CNodeAddress* Nodes, bool bIgnoreSwitchedOff, bool bIgnoreAlreadyFound, bool bBoatNodes)
{
	s32	Node, Region;
	s32	StartNode=0, EndNode=0;
	s32 iNoOfNodes=0;
	const float fRadiusSquared=Radius*Radius;

	for (Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	{
		// Make sure this Region is loaded in.
		if(!apRegions[Region] || !apRegions[Region]->aNodes)
		{
			continue;
		}

		FindStartAndEndNodeBasedOnYRange(Region, Centre.y - Radius, Centre.y + Radius, StartNode, EndNode);

		for (Node = StartNode; Node < EndNode; Node++)
		{
			CPathNode	*pNode = &((apRegions[Region]->aNodes)[Node]);

			if(	(bIgnoreSwitchedOff && pNode->m_2.m_switchedOff) ||
				(bIgnoreAlreadyFound && pNode->m_2.m_alreadyFound) ||
				(bBoatNodes != static_cast<bool>(pNode->m_2.m_waterNode)))
			{
				continue;
			}

			Vector3 vNodePos;
			pNode->GetCoors(vNodePos);

			const Vector3 vDiff=Centre-vNodePos;
			const float fDistanceSquared=vDiff.Mag2();
			if(fDistanceSquared<fRadiusSquared)
			{
				Nodes[iNoOfNodes].Set(Region, Node);
				iNoOfNodes++;
			}

			if(iNoOfNodes==NoOfNodes)
			{
				return iNoOfNodes;
			}
		}
	}

	return iNoOfNodes;
}


s32 CPathFind::RecordNodesInCircleFacingCentre(const Vector3& Centre, const float MinRadius, const float MaxRadius, s32 NoOfNodes, CNodeAddress* Nodes, bool bIgnoreSwitchedOff, bool bHighwaysOnly, bool treatSlipLanesAsHighways, bool localRegionOnly, bool searchUpOnly, u32& totalNodesInMaxRadius)
{
	u32 nodesInMaxRadius = 0;
	s32	Node, Region;
	s32	StartNode=0, EndNode=0;
	s32 iNoOfNodes=0;
	const float fMaxRadiusSquared=MaxRadius*MaxRadius;
	const float fMinRadiusSquared=MinRadius*MinRadius;
	s32 localRegion = FindRegionForCoors(Centre.GetX(), Centre.GetY());

	for (Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	{
		// Make sure this Region is loaded in.
		if(!apRegions[Region] || !apRegions[Region]->aNodes || (localRegionOnly && (Region != localRegion)))
		{
			continue;
		}

		FindStartAndEndNodeBasedOnYRange(Region, Centre.y - MaxRadius, Centre.y + MaxRadius, StartNode, EndNode);

		for (Node = StartNode; Node < EndNode; Node++)
		{
			CPathNode	*pNode = &((apRegions[Region]->aNodes)[Node]);
			bool isHighway = pNode->IsHighway();

			if(isHighway && !treatSlipLanesAsHighways)
			{
				if(pNode->IsSlipLane())
				{
					isHighway = false;
				}
			}

			if((bIgnoreSwitchedOff && pNode->m_2.m_switchedOff) ||
			   (bHighwaysOnly && !isHighway))
			{
				continue;
			}

			Vector3 vNodePos;
			pNode->GetCoors(vNodePos);

			if(searchUpOnly && vNodePos.GetZ() < Centre.GetZ())
			{
				continue;
			}

			const Vector3 vDiff=Centre-vNodePos;
			const float fDistanceSquared=vDiff.Mag2();
			if(fDistanceSquared<fMaxRadiusSquared)
			{
				nodesInMaxRadius++;

				if(fDistanceSquared>fMinRadiusSquared)
				{
					CNodeAddress nodeAddress;
					nodeAddress.Set(Region, Node);

					Vector3 vehicleOrientation = YAXIS;
					vehicleOrientation.RotateZ(DtoR * ThePaths.FindNodeOrientationForCarPlacement(nodeAddress));

					Vector3 vehicleToCentre = Centre - vNodePos;
					vehicleToCentre.SetZ(0.0f);
					vehicleToCentre.NormalizeFast(vehicleToCentre);
					
					vehicleOrientation.SetZ(0.0f);
					vehicleOrientation.NormalizeFast(vehicleOrientation);

					if(vehicleOrientation.Dot(vehicleToCentre) > 0.0f)
					{
						Nodes[iNoOfNodes].Set(Region, Node);
						iNoOfNodes++;
					}
				}
			}

			if(iNoOfNodes==NoOfNodes)
			{
				return iNoOfNodes;
			}
		}
	}

	totalNodesInMaxRadius = nodesInMaxRadius;
	return iNoOfNodes;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeClosestToCoorsFavourDirection
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFind::FindNodeClosestToCoorsFavourDirection(const Vector3& SearchCoors, float DirX, float DirY, const bool bIncludeSwitchedOffNodes)
{
	s32			Node, Region;
	float			ClosestDist, ThisDist;
	s32			StartNode=0, EndNode=0;
	float			Length;
	CNodeAddress	ClosestNode;

	ClosestNode.SetEmpty();

	Length = rage::Sqrtf(DirX * DirX + DirY * DirY);
	if (Length != 0.0f)
	{
		DirX /= Length;
		DirY /= Length;
	}
	else
	{
		DirX = 1.0f;
	}

	ClosestNode.SetEmpty();
	ClosestDist = 10000.0f;

	for (Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	{
		// Make sure this Region is loaded in.
		if(!apRegions[Region] || !apRegions[Region]->aNodes)
		{
			continue;
		}

			StartNode = 0;
			EndNode = apRegions[Region]->NumNodesCarNodes;
			for (Node = StartNode; Node < EndNode; Node++)
			{
				CPathNode	*pNode = &((apRegions[Region]->aNodes)[Node]);

			if(pNode->HasSpecialFunction_Driving())
				{
				continue;
			}
			
			if(!bIncludeSwitchedOffNodes && pNode->m_2.m_switchedOff)
			{
				continue;
			}
		
					Vector3 nodePos(0.0f, 0.0f, 0.0f);
					pNode->GetCoors(nodePos);

			if((ThisDist =(rage::Abs(nodePos.x - SearchCoors.x)+ rage::Abs(nodePos.y - SearchCoors.y)+ 3.0f*rage::Abs(nodePos.z - SearchCoors.z))) >= ClosestDist)
					{
				continue;
			}
			
						// Add a penalty for being in the wrong direction
						/*
						DiffX = pNode->GetCoors().x - SearchCoors.x;
						DiffY = pNode->GetCoors().y - SearchCoors.y;
						Length = rage::Sqrtf(DiffX * DiffX + DiffY * DiffY);
						if (Length != 0.0f)
						{
						DiffX /= Length;
						DiffY /= Length;
						}
						else
						{
						DiffX = 1.0f;
						}
						*/

						// Apply penalty for orientation mismatch
						ThisDist -= 40.0f * (CalcDirectionMatchValue(pNode, DirX, DirY) - 1.0f);
						//						ThisDist += -20.0f * ((DiffX * DirX + DiffY * DirY) - 1.0f);

			if(ThisDist >= ClosestDist)
						{
				continue;
			}
			
							ClosestDist = ThisDist;
							ClosestNode.Set(Region, Node);
						}
					}
	return(ClosestNode);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeClosestToNodeFavorDirection
// PURPOSE :  Finds the node closest to the node in the given direction.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::FindNodeClosestToNodeFavorDirection(const FindNodeClosestToNodeFavorDirectionInput& rInput, FindNodeClosestToNodeFavorDirectionOutput& rOutput)
{
	//Grab the node.
	const CPathNode* pNode = FindNodePointerSafe(rInput.m_Node);
	if(!pNode)
	{
		return false;
	}
	
	//Get the node position.
	Vector2 vPosition;
	pNode->GetCoors2(vPosition);
	
	//Keep track of the best node.
	struct BestNode
	{
		BestNode()
		: m_Address()
		, m_fScore(FLT_MIN)
		, m_fDistance(0.0f)
		{}

		CNodeAddress	m_Address;
		float			m_fScore;
		float			m_fDistance;
	};
	BestNode bestNode;

	//Find the link in the best direction.
	for(u32 uLink = 0; uLink < pNode->NumLinks(); ++uLink)
	{
		//Grab the link.
		const CPathNodeLink& rLink = GetNodesLink(pNode, uLink);
		
		//Check if we can follow the link.
		bool bCanFollowLink = false;
		if(rInput.m_bCanFollowOutgoingLinks)
		{
			//Ensure there is an outgoing link.
			if(rLink.m_1.m_LanesToOtherNode != 0)
			{
				bCanFollowLink = true;
			}
		}
		if(rInput.m_bCanFollowIncomingLinks)
		{
			//Ensure there is an incoming link.
			if(rLink.m_1.m_LanesFromOtherNode != 0)
			{
				bCanFollowLink = true;
			}
		}
		if(!bCanFollowLink)
		{
			continue;
		}
		
		//Ensure the other node is valid.
		const CPathNode* pOtherNode = FindNodePointerSafe(rLink.m_OtherNode);
		if(!pOtherNode)
		{
			continue;
		}
		
		//Ensure the other node is not the exception.
		if(!rInput.m_Exception.IsEmpty() && (rInput.m_Exception == pOtherNode->m_address))
		{
			continue;
		}
		
		//Check if we are not including switched off nodes.
		if(!rInput.m_bIncludeSwitchedOffNodes)
		{
			//Check if the other node is switched off.
			bool bSwitchedOff = rInput.m_bUseOriginalSwitchedOffValue ? pOtherNode->m_2.m_switchedOffOriginal : pOtherNode->m_2.m_switchedOff;
			if(bSwitchedOff)
			{
				continue;
			}
		}

		//Check if we are not including dead end nodes.
		if(!rInput.m_bIncludeDeadEndNodes)
		{
			//Check if the other node is a dead end.
			bool bDeadEnd = (pOtherNode->m_2.m_deadEndness != 0);
			if(bDeadEnd)
			{
				continue;
			}
		}

		//Get the other position.
		Vector2 vOtherPosition;
		pOtherNode->GetCoors2(vOtherPosition);

		//Generate the other direction.
		Vector2 vOtherDirection = vOtherPosition - vPosition;
		if(!vOtherDirection.NormalizeSafe())
		{
			continue;
		}

		//Check the direction.
		float fDot = rInput.m_vDirection.Dot(vOtherDirection);
		float fScore = fDot;

		//Apply some randomness to the scoring.
		float fMaxRandomVariance = rInput.m_fMaxRandomVariance;
		if(fMaxRandomVariance > 0.0f)
		{
			fScore += fwRandom::GetRandomNumberInRange(-fMaxRandomVariance, fMaxRandomVariance);
		}

		//Ensure the score is better.
		if(!bestNode.m_Address.IsEmpty() && (fScore <= bestNode.m_fScore))
		{
			continue;
		}

		//Assign the best node.
		bestNode.m_Address = pOtherNode->m_address;
		bestNode.m_fScore = fScore;
		bestNode.m_fDistance = (float)rLink.m_1.m_Distance;
	}
	
	//Ensure the best node is valid.
	if(bestNode.m_Address.IsEmpty())
	{
		return false;
	}
	
	//Assign the output.
	rOutput.m_Node		= bestNode.m_Address;
	rOutput.m_fDistance	= bestNode.m_fDistance;
	
	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeInDirection
// PURPOSE :  Finds a node in the given direction.
/////////////////////////////////////////////////////////////////////////////////
bool	CPathFind::FindNodeInDirection(const FindNodeInDirectionInput& rInput, FindNodeInDirectionOutput& rOutput)
{
	//Keep track of the initial position.
	Vector2 vInitialPositionXY;
	
	//Convert the initial direction to 2D.
	Vector2 vInitialDirectionXY;
	rInput.m_vInitialDirection.GetVector2XY(vInitialDirectionXY);
	
	//Find the first node.
	const CPathNode* pNode = FindNodePointerSafe(rInput.m_Node);
	if(!pNode)
	{
		//Set the initial position.
		rInput.m_vInitialPosition.GetVector2XY(vInitialPositionXY);
		
		//Find the node.
		CNodeAddress firstNode = FindNodeClosestToCoorsFavourDirection(rInput.m_vInitialPosition, vInitialDirectionXY.x, vInitialDirectionXY.y, rInput.m_bIncludeSwitchedOffNodes);
		
		//Ensure the node is valid.
		pNode = FindNodePointerSafe(firstNode);
		if(!pNode)
		{
			return false;
		}
	}
	else
	{
		//Set the initial position.
		pNode->GetCoors2(vInitialPositionXY);
	}

#if __BANK
	if(rInput.m_bRender)
	{
		Vector3 vSpherePosition;
		pNode->GetCoors(vSpherePosition);
		vSpherePosition.z += 1.0f;
		grcDebugDraw::Sphere(vSpherePosition, 0.1f, Color_red, true, -1);
	}
#endif
	
	//Calculate the target.
	Vector2 vTarget = vInitialPositionXY + (vInitialDirectionXY * rInput.m_fMinDistance);
	
	//Assign the output node.
	rOutput.m_Node = pNode->m_address;
	
	//Assign the direction.
	Vector2 vDirection = vInitialDirectionXY;

#if __BANK
	if(rInput.m_bRender)
	{
		Vector3 vArrowPositionA;
		pNode->GetCoors(vArrowPositionA);
		vArrowPositionA.z += 1.0f;
		Vector3 vArrowDirection(Vector3(vTarget, Vector2::kXY) - vArrowPositionA);
		vArrowDirection.z = 0.0f;
		vArrowDirection.Normalize();
		Vector3 vArrowPositionB = vArrowPositionA + vArrowDirection;
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vArrowPositionA), VECTOR3_TO_VEC3V(vArrowPositionB), 0.25f, Color_red, -1);
	}
#endif
	
	//Square the min distance.
	float fMinDistanceSq = square(rInput.m_fMinDistance);
	
	//Keep track of whether a switched on or non-dead end node has been encountered.
	bool bSwitchedOnEncountered = false;
	bool bNonDeadEndEncountered = false;
	
	//Iterate over the nodes along the road direction.
	float fDistanceTraveled = 0.0f;
	while(fDistanceTraveled < rInput.m_fMaxDistanceTraveled)
	{
		//Assign the old node position.
		Vector2 vOldNodePosition;
		pNode->GetCoors2(vOldNodePosition);
		
		//Update the 'switched on node encountered' flag.
		if(!bSwitchedOnEncountered)
		{
			//Check if the node is switched off.
			bool bSwitchedOff = rInput.m_bUseOriginalSwitchedOffValue ? pNode->m_2.m_switchedOffOriginal : pNode->m_2.m_switchedOff;
			bSwitchedOnEncountered = !bSwitchedOff;
		}
		
		//Update the 'include switched off nodes' flag.
		bool bIncludeSwitchedOffNodes = rInput.m_bIncludeSwitchedOffNodes;
		if(bIncludeSwitchedOffNodes)
		{
			if(rInput.m_bDoNotIncludeSwitchedOffNodesAfterSwitchedOnEncountered)
			{
				bIncludeSwitchedOffNodes = !bSwitchedOnEncountered;
			}
		}

		//Update the 'non-dead end node encountered' flag.
		if(!bNonDeadEndEncountered)
		{
			//Check if the node is a dead end.
			bool bDeadEnd = (pNode->m_2.m_deadEndness != 0);
			bNonDeadEndEncountered = !bDeadEnd;
		}

		//Update the 'include dead end nodes' flag.
		bool bIncludeDeadEndNodes = rInput.m_bIncludeDeadEndNodes;
		if(bIncludeDeadEndNodes)
		{
			if(rInput.m_bDoNotIncludeDeadEndNodesAfterNonDeadEndEncountered)
			{
				bIncludeDeadEndNodes = !bNonDeadEndEncountered;
			}
		}

		//Check if we can follow outgoing links.
		bool bCanFollowOutgoingLinks = (rInput.m_bCanFollowOutgoingLinks ||
			(rInput.m_bCanFollowOutgoingLinksFromDeadEndNodes && (pNode->m_2.m_deadEndness != 0)));
		
		//Find the closest node in the direction.
		FindNodeClosestToNodeFavorDirectionInput input(pNode->m_address, Vector2(vDirection.x, vDirection.y));
		input.m_Exception = rOutput.m_PreviousNode;
		input.m_fMaxRandomVariance = rInput.m_fMaxRandomVariance;
		input.m_bCanFollowOutgoingLinks = bCanFollowOutgoingLinks;
		input.m_bCanFollowIncomingLinks = rInput.m_bCanFollowIncomingLinks;
		input.m_bIncludeSwitchedOffNodes = bIncludeSwitchedOffNodes;
		input.m_bUseOriginalSwitchedOffValue = rInput.m_bUseOriginalSwitchedOffValue;
		input.m_bIncludeDeadEndNodes = bIncludeDeadEndNodes;
		FindNodeClosestToNodeFavorDirectionOutput output;
		if(!FindNodeClosestToNodeFavorDirection(input, output))
		{
			break;
		}

		//Assign the node.
		pNode = FindNodePointerSafe(output.m_Node);
		if(!pNode)
		{
			break;
		}
		
		//Update the distance traveled.
		fDistanceTraveled += output.m_fDistance;
		
		//Assign the output nodes.
		rOutput.m_PreviousNode = rOutput.m_Node;
		rOutput.m_Node = pNode->m_address;

#if __BANK
		if(rInput.m_bRender)
		{
			Vector3 vLinePositionA;
			FindNodePointer(rOutput.m_PreviousNode)->GetCoors(vLinePositionA);
			vLinePositionA.z += 1.0f;
			Vector3 vLinePositionB;
			FindNodePointer(rOutput.m_Node)->GetCoors(vLinePositionB);
			vLinePositionB.z += 1.0f;
			grcDebugDraw::Line(vLinePositionA, vLinePositionB, Color_blue, Color_blue, -1);
		}
#endif
		
		//Assign the new node position.
		Vector2 vNewNodePosition;
		pNode->GetCoors2(vNewNodePosition);
		
		//Check if the distance has exceeded the minimum.
		float fDistSq = vNewNodePosition.Dist2(vInitialPositionXY);
		if(fDistSq >= fMinDistanceSq)
		{
			//Check if the node has not exceeded the maximum links.
			if(rInput.m_uMaxLinks == 0 || (pNode->NumLinks() <= rInput.m_uMaxLinks))
			{
				//Check if this is our node to find.
				if(rInput.m_NodeToFind.IsEmpty() || (rInput.m_NodeToFind == rOutput.m_Node))
				{
#if __BANK
					//Check if we should render.
					if(rInput.m_bRender)
					{
						Vector3 vSpherePosition;
						FindNodePointer(rOutput.m_Node)->GetCoors(vSpherePosition);
						vSpherePosition.z += 1.0f;
						grcDebugDraw::Sphere(vSpherePosition, 0.1f, Color_green, true, -1);
					}
#endif

					return true;
				}
			}
		}
		
		//Check if we are following the road.
		if(rInput.m_bFollowRoad)
		{
			//We are following the road.
			//The next direction should be from old->new node.
			vDirection = vNewNodePosition - vOldNodePosition;
		}
		else
		{
			//We are not following the road.
			//The next direction will be from the new node to the target.
			vDirection = vTarget - vNewNodePosition;
		}
		
		//Normalize the direction.
		if(!vDirection.NormalizeSafe())
		{
			break;
		}
	}

#if __BANK
	if(rInput.m_bRender)
	{
		Vector3 vSpherePosition;
		FindNodePointer(rOutput.m_Node)->GetCoors(vSpherePosition);
		vSpherePosition.z += 1.0f;
		grcDebugDraw::Sphere(vSpherePosition, 0.1f, Color_red, true, -1);
	}
#endif
	
	return false;
}

///////////////////////////////////////////////////////////////////////////

// Is a reservation available for the given crossing node addresses?
bool CPathFind::IsPedCrossReservationAvailable(const CNodeAddress& startAddress, const CNodeAddress& endAddress) const
{
	// Find the reservation matching given addresses, if any
	int reservationIndex = FindPedCrossReservationIndex(startAddress, endAddress);
	// If no match found
	if( reservationIndex < 0 )
	{
		// return whether there is a list entry available for use
		reservationIndex = FindAvailablePedCrossReservationIndex();
		return (reservationIndex >= 0);
	}
	else // If a match was found
	{
		// Get a handle to the reservation
		const PedCrossingStartPosReservation& reservation = m_aPedCrossingReservations[reservationIndex];

		// Check the status flags for an available slot
		for(int i = 0; i < MAX_PED_CROSS_RESERVATION_SLOTS; i++)
		{
			// If an available slot is found
			if( !reservation.m_ReservationSlotStatus.IsFlagSet((1<<i)) )
			{
				// reservation is available
				return true;
			}
		}
		// otherwise fall through to default false
	}

	// by default no reservation available
	return false;
}

// Make a reservation for the given crossing node addresses
// Returns success or failure and sets out variable for reservation slot on success
bool CPathFind::MakePedCrossReservation(const CNodeAddress& startAddress, const CNodeAddress& endAddress, int& outReservationSlot, int& outAssignedCrossDelayMS)
{
	// Find the reservation matching given addresses, if any
	int reservationIndex = FindPedCrossReservationIndex(startAddress, endAddress);
	
	// If no match found
	if( reservationIndex < 0 )
	{
		// try to add an entry
		reservationIndex = AddPedCrossReservation(startAddress,endAddress);
		
		// if add failed
		if( reservationIndex < 0 )
		{
			// report reservation failure
			return false;
		}
	}

	// We should have a valid reservation index at this point
	if( AssertVerify(reservationIndex >= 0) && AssertVerify(reservationIndex < MAX_PED_CROSSING_RESERVATIONS) )
	{
		// Get a handle to the reservation
		PedCrossingStartPosReservation& reservation = m_aPedCrossingReservations[reservationIndex];

		// Check the status flags for an available slot
		for(int i = 0; i < MAX_PED_CROSS_RESERVATION_SLOTS; i++)
		{
			// If an available slot is found
			if( !reservation.m_ReservationSlotStatus.IsFlagSet((1<<i)) )
			{
				// Mark the status flag for the slot
				reservation.m_ReservationSlotStatus.SetFlag((1<<i));

				// Set the out variable to match the slot number
				outReservationSlot = i;

				// Set the out variable for cross delay time
				outAssignedCrossDelayMS = reservation.m_AssignedCrossDelayMS[i];

				// Report successful reservation
				return true;
			}
		}

		// If no available slot, report reservation failure
		return false;
	}
	
	// report reservation failure by default
	return false;
}

// Release the reservation for the given crossing node addresses and slot
void CPathFind::ReleasePedCrossReservation(const CNodeAddress& startAddress, const CNodeAddress& endAddress, const int& reservationSlot)
{
	// Enforce valid range on given reservation slot
	if( AssertVerify(reservationSlot >= 0) && AssertVerify(reservationSlot < MAX_PED_CROSS_RESERVATION_SLOTS) )
	{
		// Find the reservation matching given addresses, if any
		int reservationIndex = FindPedCrossReservationIndex(startAddress, endAddress);
		if( AssertVerify(reservationIndex >= 0) )
		{
			// Local handle to reservation
			PedCrossingStartPosReservation& reservation = m_aPedCrossingReservations[reservationIndex];

			// Clear the given slot
			reservation.m_ReservationSlotStatus.ClearFlag((1 << reservationSlot));

			// If all slots are now clear
			if( reservation.m_ReservationSlotStatus.GetAllFlags() == 0 )
			{
				// Empty the reservation for re-use
				reservation.m_StartNodeAddress.SetEmpty();
				reservation.m_EndNodeAddress.SetEmpty();
			}
		}
	}
}

// Compute the proper crossing start position for the given crossing reservation
// Returns true on success and false on failure
bool CPathFind::ComputePositionForPedCrossReservation(const CNodeAddress& startAddress, const CNodeAddress& endAddress, const int& reservationSlot, Vector3& outPosition) const
{
	const CPathNode* pStartNode = FindNodePointerSafe(startAddress);
	const CPathNode* pEndNode = FindNodePointerSafe(endAddress);
	int reservationIndex = FindPedCrossReservationIndex(startAddress, endAddress);
	if( pStartNode && pEndNode && reservationIndex >= 0 && AssertVerify(reservationSlot >= 0) && AssertVerify(reservationSlot < MAX_PED_CROSS_RESERVATION_SLOTS) )
	{
		// Get direction vectors for this crossing
		Vector3 vStartNodePosition = pStartNode->GetPos();
		Vector3 vEndNodePosition = pEndNode->GetPos();
		Vector3 vCrossDir = vEndNodePosition - vStartNodePosition;
		vCrossDir.NormalizeFast();
		Vector3 vRightDir = vCrossDir;
		vRightDir.RotateZ(-90.0f * DtoR);// Rotate Z negative 90 gives right

		// Starting at the centroid offset
		Vector3 vReservationPosition = vStartNodePosition + ((CPathFind::sm_fPedCrossReserveSlotsCentroidForwardOffset) * vCrossDir);

		// Then apply the stored offsets
		vReservationPosition += ((m_aPedCrossingReservations[reservationIndex].m_ReservationSlotForwardOffset[reservationSlot]) * vCrossDir);
		vReservationPosition += ((m_aPedCrossingReservations[reservationIndex].m_ReservationSlotRightOffset[reservationSlot]) * vRightDir);

		// Update the record start position to be the reservation position
		outPosition = vReservationPosition;

		// report success
		return true;
	}
	
	// failure by default
	return false;
}

// Helper method to find the index of a matching entry for the given node addresses
// Returns -1 if no match is found
int CPathFind::FindPedCrossReservationIndex(const CNodeAddress& startAddress, const CNodeAddress& endAddress) const
{
	// Traverse reservation list to find existing entry for the given node addresses
	for(int i=0; i < MAX_PED_CROSSING_RESERVATIONS; i++ )
	{
		const PedCrossingStartPosReservation& reservation = m_aPedCrossingReservations[i];
		
		// If a matching entry is found, return the index
		if( reservation.m_StartNodeAddress == startAddress && reservation.m_EndNodeAddress == endAddress )
		{
			return i;
		}
	}
	
	// No matching entry was found, return sentinel value
	return -1;
}

// Helper method to find the index of an entry available for use
// Returns -1 if no entries are available
int CPathFind::FindAvailablePedCrossReservationIndex() const
{
	// Traverse the list
	for(u32 i = 0; i < MAX_PED_CROSSING_RESERVATIONS; i++)
	{
		// if an entry has an empty start address then it can be re-used
		if( m_aPedCrossingReservations[i].m_StartNodeAddress.IsEmpty() )
		{
			return i;
		}
	}

	// All entries are in use
	return -1;
}

// Helper method to set the first available list item to use the given node addresses
// Returns the index of the newly modified entry if successful
// Returns -1 if the list is already full
int CPathFind::AddPedCrossReservation(const CNodeAddress& startAddress, const CNodeAddress& endAddress)
{
#if __DEV
	// In DEV builds, assert and fail if trying to Add when an entry already exists
	int existingIndex = FindPedCrossReservationIndex(startAddress,endAddress);
	Assert(existingIndex < 0);
	if( existingIndex >= 0 )
	{
		return -1;
	}
#endif // __DEV

	// Find an available reservation entry index
	int availableIndex = FindAvailablePedCrossReservationIndex();

	// If the first available index is negative
	if( availableIndex < 0 )
	{
		// then the list is full - return sentinel value indicating failure
		Assertf(availableIndex >= 0,"PedCrossingReservations list is full at size[%d]!", MAX_PED_CROSSING_RESERVATIONS);
		return -1;
	}
	// otherwise we have an available index

	// Setup the given address information at available index
	m_aPedCrossingReservations[availableIndex].m_StartNodeAddress.Set(startAddress);
	m_aPedCrossingReservations[availableIndex].m_EndNodeAddress.Set(endAddress);

	// roll the assigned cross delays
	ComputeRandomizedCrossDelays(m_aPedCrossingReservations[availableIndex]);

	// generate randomized cross slot positions
	ComputeRandomizedCrossPositions(m_aPedCrossingReservations[availableIndex]);

	// return the index of the new data
	return availableIndex;
}

// Helper method to roll randomized crossing delays
// NOTE: This is to enforce a minimum spread among peds departing the same crossing start
void CPathFind::ComputeRandomizedCrossDelays(PedCrossingStartPosReservation& outReservation) const
{
	// define the random range
	static dev_s32 s_iDelayMS_MIN = 300;
	static dev_s32 s_iDelayMS_MAX = 500;

	// initialize the delay time at zero
	s32 delayTimeMS = 0;

	// randomly choose among the reservation slots to begin traversal
	// NOTE: for ints, random number in range never returns the given max
	s32 initialIndex = fwRandom::GetRandomNumberInRange(0, MAX_PED_CROSS_RESERVATION_SLOTS);
	Assert(initialIndex < MAX_PED_CROSS_RESERVATION_SLOTS);

	// traverse the slots
	int idx = initialIndex;
	for(int i = 0; i < MAX_PED_CROSS_RESERVATION_SLOTS; i++)
	{
		// assign the delay time
		outReservation.m_AssignedCrossDelayMS[idx] = delayTimeMS;

		// roll a small random range and add to the delay time
		delayTimeMS += fwRandom::GetRandomNumberInRange(s_iDelayMS_MIN, s_iDelayMS_MAX);
		
		// increment index and wrap around if needed
		idx++;
		if( idx >= MAX_PED_CROSS_RESERVATION_SLOTS )
		{
			idx = 0;
		}
	}
}

// Helper method to generate randomized crossing slot positions
// NOTE: This is to provide a more organic grouping of peds waiting to cross while
// still helping to keep the peds spaced apart.
void CPathFind::ComputeRandomizedCrossPositions(PedCrossingStartPosReservation& outReservation) const
{
	// Define the centroid as the center of a row of three grid cells
	// Enumerate the grid cells from centroid forward leftmost as zero.

	// Pick the grid index to start with in the loop randomly
	// NOTE: this will return in the range [0,MAX_PED_CROSS_RESERVATION_SLOTS - 1]
	u32 uCellIndex = fwRandom::GetRandomNumberInRange(0, MAX_PED_CROSS_RESERVATION_SLOTS);

	// Limit how far the random adjustment can go
	const float fRandAdjustTolerance = sm_fPedCrossReserveSlotsPlacementGridSpacing/2.0f;

	// We will set the first cell all the way forward, then the next one centered, then the third back
	float fRandomFwdOffset = fRandAdjustTolerance; // all the way forward initially
	const float fRandomFwdOffsetDelta = -fRandAdjustTolerance; // pull back to zero and then all the way back

	// Then count over the number of positions needing assignment
	for(int i=0; i < MAX_PED_CROSS_RESERVATION_SLOTS; i++)
	{
		// Initialize the position at the center of the corresponding grid cell
		switch(uCellIndex)
		{
		case 0: // left
			outReservation.m_ReservationSlotForwardOffset[uCellIndex] = 0.0f;
			outReservation.m_ReservationSlotRightOffset[uCellIndex] = -sm_fPedCrossReserveSlotsPlacementGridSpacing;
			break;
		case 1: // center
			outReservation.m_ReservationSlotForwardOffset[uCellIndex] = 0.0f;
			outReservation.m_ReservationSlotRightOffset[uCellIndex] = 0.0f;
			break;
		case 2: // right
			outReservation.m_ReservationSlotForwardOffset[uCellIndex] = 0.0f;
			outReservation.m_ReservationSlotRightOffset[uCellIndex] = sm_fPedCrossReserveSlotsPlacementGridSpacing;
			break;
		default:
			aiAssertf(false,"unsupported grid cell index (%d) !", uCellIndex);
			break;
		}

		// Then randomize the position within the cell.
		// We want to keep adjacent positions distinct in the forward direction.
		// To do this we push all the way forward, then leave one centered, then push one back
		if( fRandAdjustTolerance > 0.0f )
		{
			switch(uCellIndex)
			{
			case 0: // left
				outReservation.m_ReservationSlotForwardOffset[uCellIndex] += fRandomFwdOffset;
				outReservation.m_ReservationSlotRightOffset[uCellIndex] += fwRandom::GetRandomNumberInRange(-fRandAdjustTolerance, 0.0f);
				break;
			case 1: // center
				outReservation.m_ReservationSlotForwardOffset[uCellIndex] += fRandomFwdOffset;
				break;
			case 2: // right
				outReservation.m_ReservationSlotForwardOffset[uCellIndex] += fRandomFwdOffset;
				outReservation.m_ReservationSlotRightOffset[uCellIndex] += fwRandom::GetRandomNumberInRange(0.0f, fRandAdjustTolerance);
				break;
			default:
				aiAssertf(false,"unsupported grid cell index (%d) !", uCellIndex);
				break;
			}
		}

		// Adjust the random forward offset for the next cell
		fRandomFwdOffset += fRandomFwdOffsetDelta;

		// Increment the grid cell index, wrapping around if necessary
		uCellIndex = uCellIndex + 1;
		if( uCellIndex >= MAX_PED_CROSS_RESERVATION_SLOTS )
		{
			uCellIndex = 0;
		}
	}
}

///////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalcDirectionMatchValue
// PURPOSE :  For a node and a specified direction returns a value to describe how
//			  match the directions match.
//			  0.0 no match at all.
//			  1.0 as good as the match can be.
/////////////////////////////////////////////////////////////////////////////////
float	CPathFind::CalcDirectionMatchValue(const CPathNode *pNode, float DirX, float DirY, float defaultMatch) const
{
	if(pNode->NumLinks() != 2)		// Junctions are confusing and have bad matches
	{
		return defaultMatch;
	}

	// Find link nodes.
	CNodeAddress	n1 = GetNodesLinkedNodeAddr(pNode, 0);
	CNodeAddress	n2 = GetNodesLinkedNodeAddr(pNode, 1);

	if ( (!IsRegionLoaded(n1)) || (!IsRegionLoaded(n2)) )
	{
		return defaultMatch;
	}

	const CPathNode * pNode1 = FindNodePointer(n1);
	const CPathNode * pNode2 = FindNodePointer(n2);

	const CPathNodeLink * link01 = ThePaths.FindLinkBetween2Nodes(pNode, pNode1);
	const CPathNodeLink * link10 = ThePaths.FindLinkBetween2Nodes(pNode1, pNode);
	const CPathNodeLink * link02 = ThePaths.FindLinkBetween2Nodes(pNode, pNode2);
	const CPathNodeLink * link20 = ThePaths.FindLinkBetween2Nodes(pNode2, pNode);

	// If any links are missing, abort
	if(!link01 || !link10 || !link02 || !link20)
	{
		return defaultMatch;
	}

	// Now, abort if any of these links are ignore for nav or shortcut, which may give crazy values
	if(link01->IsShortCut() || link10->IsShortCut() || link02->IsShortCut() || link20->IsShortCut())
	{
		return defaultMatch;
	}
	if(link01->IsDontUseForNavigation() || link10->IsDontUseForNavigation() || link02->IsDontUseForNavigation() || link20->IsDontUseForNavigation())
	{
		return defaultMatch;
	}

	Vector3 n1Coors, n2Coors;
	pNode1->GetCoors(n1Coors);
	pNode2->GetCoors(n2Coors);

	Vector3 Dir = n1Coors - n2Coors;
	Dir.Normalize();
	float	retVal = rage::Abs(DirX*Dir.x + DirY*Dir.y);

	Assert(retVal >= 0.0f && retVal <= 1.01f);

	return retVal;
}

s32 CPathFind::FindLaneForVehicleWithLink(const CNodeAddress firstNode, const CNodeAddress secondNode, const CPathNodeLink& rLink, const Vector3& position) const
{
	s32 nearestLane = 0;

	Vector3 vFirstNodeCoors, vSecondNodeCoors;

	const CPathNode* pFirstNode = FindNodePointerSafe(firstNode);
	const CPathNode* pSecondNode = FindNodePointerSafe(secondNode);
	if (!pFirstNode || !pSecondNode)
	{
		return nearestLane;
	}

	pFirstNode->GetCoors(vFirstNodeCoors);
	pSecondNode->GetCoors(vSecondNodeCoors);

	Vector3 vSide = Vector3(vSecondNodeCoors.y - vFirstNodeCoors.y, vFirstNodeCoors.x - vSecondNodeCoors.x, 0.0f);
	if(vSide.Mag2() > SMALL_FLOAT)
		vSide.Normalize();
	float sideDist = (position - vFirstNodeCoors).Dot(vSide);

	// If we're on a one way street the link runs through the middle lane. Take this into account.
	if(!rLink.IsSingleTrack())
	{
		sideDist += (rLink.GetLaneWidth() * 0.5f) /** lanesAvailable*/;
	}
	const float fCentralReservation = rLink.InitialLaneCenterOffset();

	nearestLane = (s32)((sideDist-fCentralReservation) / rLink.GetLaneWidth());

	// The lane needs to be clipped against the lanes on this link.
	nearestLane = rage::Min(nearestLane, (s32)rLink.m_1.m_LanesToOtherNode - 1);
	nearestLane = rage::Max(0, nearestLane);

	return nearestLane;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodePairToMatchVehicle
// PURPOSE :  Identify 2 nodes that are nearby and have the same direction as vehicle.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::FindNodePairToMatchVehicle(const Vector3& position, const Vector3& forward, CNodeAddress &nodeFrom, CNodeAddress &nodeTo, bool bWaterNodes, s16 &regionLinkIndex_out, s8 &nearestLane, const float fRejectDist, const bool bIgnoreDirection, CNodeAddress hintNodeFrom, CNodeAddress hintNodeTo)
{
	PF_FUNC(FindNodePairToMatchVehicle);

	nodeFrom.SetEmpty();
	nodeTo.SetEmpty();
	regionLinkIndex_out = -1;
	nearestLane = -1;

	float smallestErrorMeasure = 999999999.9f;

	// If the user supplied a "hint" pair of nodes, evaluate that pair first. That way, assuming
	// that it's a reasonably good hint such as a result from a recent frame, we get an early
	// upper bound on the error measure that we can use to reject other node pairs.
	if(!hintNodeFrom.IsEmpty())
	{
		s16 hintLinkIndex = -1;
		if(FindNodesLinkIndexBetween2Nodes(hintNodeFrom, hintNodeTo, hintLinkIndex))
		{
			const int hintRegion = hintNodeFrom.GetRegion();
			const int hintNodeIndex = hintNodeFrom.GetIndex();
			FindNodePairToMatchVehicleInRegion(hintRegion, RCC_VEC3V(position), RCC_VEC3V(forward), bWaterNodes, fRejectDist, bIgnoreDirection, smallestErrorMeasure, nodeFrom, nodeTo, regionLinkIndex_out, nearestLane, hintNodeIndex, hintLinkIndex);
		}
	}

	// Determine a rectangular window of regions. No nodes outside of this window could possibly
	// be within fRejectDist.
	const float posX = position.x;
	const float posY = position.y;
	const float searchMinXFloat = posX - fRejectDist;
	const float searchMaxXFloat = posX + fRejectDist;
	const float searchMinYFloat = posY - fRejectDist;
	const float searchMaxYFloat = posY + fRejectDist;
	const int minRegX = FindXRegionForCoors(searchMinXFloat);
	const int maxRegX = FindXRegionForCoors(searchMaxXFloat);
	const int minRegY = FindYRegionForCoors(searchMinYFloat);
	const int maxRegY = FindYRegionForCoors(searchMaxYFloat);

	// This code used to loop over PATHFINDREGIONS + MAXINTERIORS. Now, we compute
	// the X/Y ranges to loop over, and don't do anything with the interior regions,
	// since they don't exist. If we get them back in the future, this code would
	// probably need some minor modification to deal with them, which is why
	// this assert is here:
	CompileTimeAssert(!MAXINTERIORS);

	// Loop over the regions in the window we computed.
	for(int regY = minRegY; regY <= maxRegY; regY++)
	{
		Assert(regY >= 0 && regY < PATHFINDMAPSPLIT);

		for(int regX = minRegX; regX <= maxRegX; regX++)
		{
			Assert(regX >= 0 && regX < PATHFINDMAPSPLIT);

			const int Region = FIND_REGION_INDEX(regX, regY);
			Assert(Region >= 0 && Region < PATHFINDREGIONS);
			FindNodePairToMatchVehicleInRegion(Region, RCC_VEC3V(position), RCC_VEC3V(forward), bWaterNodes, fRejectDist, bIgnoreDirection, smallestErrorMeasure, nodeFrom, nodeTo, regionLinkIndex_out, nearestLane, -1, -1);
		}
	}

	return (regionLinkIndex_out != -1);
}

/*
PURPOSE
	Helper function that does most of the work for FindNodePairToMatchVehicle(). It scores
	the nodes within a given region, or, as an option, evaluates just a single node pair
	in the given region.
*/
void CPathFind::FindNodePairToMatchVehicleInRegion(int regionIndex, Vec3V_ConstRef position, Vec3V_ConstRef forward,
		const bool bWaterNodes, const float fRejectDist, const bool bIgnoreDirection,
		float& smallestErrorMeasureInOut, CNodeAddress &nodeFromInOut, CNodeAddress &nodeToInOut,
		s16 &regionLinkIndexInOut, s8 &nearestLaneInOut,
		const int specificNodeToCheck, const int specificLinkIndexToCheck)
{
	// TODO: Would be beneficial to move these out, they probably add some overhead since they are
	// implemented as static variables which the code has to think about a little bit each time
	// the function is called.
	TUNE_GROUP_FLOAT(JOIN_TO_ROAD_SYSTEM, NoLanesPenalty, 20.0f, 0.0f, 1000.0f, 0.1f);
	TUNE_GROUP_FLOAT(JOIN_TO_ROAD_SYSTEM, MaxAllowedHeightDiff, 5.0f, 0.0f, 1000.0f, 0.1f);
	TUNE_GROUP_FLOAT(JOIN_TO_ROAD_SYSTEM, BasePenaltyForHeightDiff, 250.0f, 0.0f, 1000.0f, 0.1f);
	TUNE_GROUP_FLOAT(JOIN_TO_ROAD_SYSTEM, ExtraPenaltyForNonAlign, 10.0f, 0.0f, 1000.0f, 0.1f);
	TUNE_GROUP_FLOAT(JOIN_TO_ROAD_SYSTEM, PenaltyMultiplierForAlignment, 20.0f, 0.0f, 1000.0f, 0.1f);
	TUNE_GROUP_FLOAT(JOIN_TO_ROAD_SYSTEM, PenaltyForDistOutsideOfExtents, 10.0f, 0.0f, 1000.0f, 0.1f);
	TUNE_GROUP_FLOAT(JOIN_TO_ROAD_SYSTEM, ShortcutLinkMultiplier, 2.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(JOIN_TO_ROAD_SYSTEM, HeightPenaltyScale, 2.0f, 0.0f, 100.0f, 0.1f);

	CPathRegion* regPtr = apRegions[regionIndex];
	if(!regPtr || !regPtr->aNodes)	// Make sure this region is loaded in.
	{
		return;
	}

	// Determine a window of integer X coordinates that all nodes within fRejectDist
	// must lie within.
	const float posX = position.GetXf();
	const float posY = position.GetYf();
	const float searchMinXFloat = posX - fRejectDist;
	const float searchMaxXFloat = posX + fRejectDist;
	const int searchMinXInt = (int)(searchMinXFloat*PATHCOORD_XYSHIFT);
	const int searchMaxXInt = (int)(searchMaxXFloat*PATHCOORD_XYSHIFT);

	// Find the X and Y min/max coordinates of this region.
	const int regX = FIND_X_FROM_REGION_INDEX(regionIndex);
	const int regY = FIND_Y_FROM_REGION_INDEX(regionIndex);
	const float thisRegMinX = FindXCoorsForRegion(regX);
	const float thisRegMaxX = thisRegMinX + PATHFINDREGIONSIZEX;
	const float thisRegMinY = FindYCoorsForRegion(regY);
	const float thisRegMaxY = thisRegMinY + PATHFINDREGIONSIZEY;

	// Compute how far we are outside of this region's bounding box,
	// in the X and Y directions.
	const float dx = Max(Max(posX - thisRegMaxX, thisRegMinX - posX), 0.0f);
	const float dy = Max(Max(posY - thisRegMaxY, thisRegMinY - posY), 0.0f);

	// Compute the squared distance to the bounding box. If that's greater than
	// the sphere we are looking at, all nodes will be rejected and we may as well
	// stop right now.
	const float distSq = dx*dx + dy*dy;
	const float fRejectDistSq = square(fRejectDist);
	if(distSq > fRejectDistSq)
	{
		return;
	}

	const CPathNodeLink* pBestLink = NULL;
	const CPathNode* pBestNode = NULL;
	const CPathNode* pBestOtherNode = NULL;
	ScalarV bestSideDistRawV(V_ZERO);
	int bestLinkIndex = 0;

	// Prepare some vector variables.
	const Vec3V positionV = position;
	const Vec3V forwardV = forward;
	ScalarV smallestErrorMeasureV = LoadScalar32IntoScalarV(smallestErrorMeasureInOut);
	ScalarV smallestErrorMeasureSqV(Scale(smallestErrorMeasureV, smallestErrorMeasureV));
	const ScalarV zeroV(V_ZERO);
	const ScalarV rejectDistSqV(fRejectDistSq);

	// Compute the start and end node based on the Y coordinate range, unless
	// the user passed in a single specific node that should be checked
	s32 startNode, endNode;
	if(specificNodeToCheck >= 0)
	{
		Assert(specificNodeToCheck < regPtr->NumNodesCarNodes);
		startNode = specificNodeToCheck;
		endNode = specificNodeToCheck + 1;
	}
	else
	{
		// We don't need to go through all of the nodes. Find the start and end one based on the y search range.
		FindStartAndEndNodeBasedOnYRange(regionIndex, posY - fRejectDist, posY + fRejectDist, startNode, endNode);
	}

	// We will be using the vector pipeline for the scoring, so create some vector
	// variables based on the tuning data. Note: might be worth considering moving
	// some of these back inside the loop, since we may not have enough vector
	// variables to keep them all anyway (at least not on PS3).
	const ScalarV noLanesPenaltyV = LoadScalar32IntoScalarV(NoLanesPenalty);
	const ScalarV maxAllowedHeightDiffV = LoadScalar32IntoScalarV(MaxAllowedHeightDiff);
	const ScalarV basePenaltyForHeightDiffV = LoadScalar32IntoScalarV(BasePenaltyForHeightDiff);
	const ScalarV extraPenaltyForNonAlignV = LoadScalar32IntoScalarV(ExtraPenaltyForNonAlign);
	const ScalarV penaltyMultiplierForAlignmentV = LoadScalar32IntoScalarV(PenaltyMultiplierForAlignment);
	const ScalarV penaltyForDistOutsideOfExtentsV = LoadScalar32IntoScalarV(PenaltyForDistOutsideOfExtents);
	const ScalarV shortcutLinkMultiplierV = LoadScalar32IntoScalarV(ShortcutLinkMultiplier);
	const ScalarV penaltyMultiplierForAlignment2V = Scale(penaltyMultiplierForAlignmentV, ScalarV(V_TWO));
	const ScalarV HeightPenaltyScaleV	= LoadScalar32IntoScalarV(HeightPenaltyScale);

	// Loop over the nodes within the range in the node array.
	const CPathNode* pNodes = regPtr->aNodes;
	for(int node = startNode; node < endNode; node++)
	{
		const CPathNode *pNode = pNodes + node;

		// First, do a quick rejection using the X coordinate. Getting the coordinates,
		// measuring the Euclidean distance, and branching based on that (float/vector)
		// is likely a fair bit more expensive than this check, so if we can avoid that
		// for the majority of nodes, it would be good.
		const int coorsX = pNode->m_coorsX;
		if(coorsX < searchMinXInt || coorsX > searchMaxXInt)
		{
			continue;
		}

		if((pNode->HasSpecialFunction_Driving()) || (bWaterNodes != static_cast<bool>(pNode->m_2.m_waterNode)))
		{
			continue;
		}

		// Get the coordinates of this node.
		Vec3V nodeCoorsV;
		pNode->GetCoors(RC_VECTOR3(nodeCoorsV));

		// Compute the squared distance, and check if it's further away than the reject distance.
		// If so, we can move on to the next node.
		const Vec3V posDeltaV = Subtract(positionV, nodeCoorsV);
		const ScalarV distSqV = MagSquared(posDeltaV);
		if(IsGreaterThanOrEqualAll(distSqV, rejectDistSqV))
		{
			continue;
		}

		const bool bSourceNodeIsSwitchedOff = pNode->IsSwitchedOff();

		// Go through the neighbours and consider all these.
		const int numLinks = pNode->NumLinks();

		// Determine the range of link indices we should look at - we may have been given
		// just a single link by the caller.
		int linkStart, linkEnd;
		if(specificLinkIndexToCheck >= 0)
		{
			Assert(specificLinkIndexToCheck < numLinks);

			linkStart = specificLinkIndexToCheck;
			linkEnd = specificLinkIndexToCheck + 1;
		}
		else
		{
			linkStart = 0;
			linkEnd = numLinks;
		}

		// Compute the negative position delta vector, may be used inside the loop.
		const Vec3V negPosDeltaV = Negate(posDeltaV);

		// Compute a pointer to this node's links within the region link array.
		Assert(pNode->m_startIndexOfLinks >= 0 && pNode->m_startIndexOfLinks < regPtr->NumLinks);
		CPathNodeLink* pLinksForThisNode = &regPtr->aLinks[pNode->m_startIndexOfLinks];
		for(u32 linkIndex = linkStart; linkIndex < linkEnd; linkIndex++)
		{
			// Get the other node, the one we are linked to:
			// CNodeAddress otherNode = GetNodesLinkedNodeAddr(pNode, linkIndex);
			const CPathNodeLink& rLink = pLinksForThisNode[linkIndex];
			CNodeAddress otherNode = rLink.m_OtherNode;
			Assert(!otherNode.IsEmpty());

			// Make sure the other region is loaded.
			if(!IsRegionLoaded(otherNode))
			{
				continue;
			}

			// Get the pointer to the other node.
			const CPathNode	*pOtherNode = FindNodePointer(otherNode);

			// Get the coordinates of the other node.
			Vec3V otherNodeCoorsV;
			pOtherNode->GetCoors(RC_VECTOR3(otherNodeCoorsV));

			// Find the point on this link which is closest to positionV.
			Vec3V closestPointV;
			fwGeom::fwLine line(VEC3V_TO_VECTOR3(nodeCoorsV), VEC3V_TO_VECTOR3(otherNodeCoorsV));
			line.FindClosestPointOnLineV(positionV, closestPointV);

			// Compute the squared distance to the closest point on the link.
			const ScalarV distSqV = DistSquared(closestPointV, positionV);

			// Check if this distance is already greater than the lowest error we've found
			// so far. The scoring code below could only ever increase the error, not decrease it,
			// so we can skip a fair amount of work by doing this.
			if(IsGreaterThanAll(distSqV, smallestErrorMeasureSqV))
			{
				continue;
			}

			// Compute the delta vector from this node to the other, and
			// a vector containing the square of its components.
			const Vec3V nodeDirNonNormV = Subtract(otherNodeCoorsV, nodeCoorsV);
			const Vec3V nodeDirNonNormSqV = Scale(nodeDirNonNormV, nodeDirNonNormV);

			// Compute the squared sum of the XY and XYZ components of the delta.
			const ScalarV nodeDirLengthXYSqV = Add(nodeDirNonNormSqV.GetX(), nodeDirNonNormSqV.GetY());
			const ScalarV nodeDirLengthSqV = Add(nodeDirLengthXYSqV, nodeDirNonNormSqV.GetZ());

			// We have a need here to compute a few square roots, and instead of
			// doing them one by one, it's probably worth stuffing them into a vector
			// and do them all at once.
			Vec3V toComputeRootsForV;
			toComputeRootsForV.SetX(distSqV);
			toComputeRootsForV.SetY(nodeDirLengthXYSqV);
			toComputeRootsForV.SetZ(nodeDirLengthSqV);
			const Vec3V rootsV = Sqrt(toComputeRootsForV);

			const bool bDestNodeIsSwitchedOff = pOtherNode->IsSwitchedOff();

			// Get the distance to the closest point, the length of the link
			// (in XYZ), and compute the direction of the link (normalized in XY).
			const ScalarV distV = rootsV.GetX();
			const ScalarV linkLengthV = rootsV.GetZ();
			const Vec3V nodeDirV = InvScale(nodeDirNonNormV, rootsV.GetY());

			// Determine the road boundaries.
			float fRoadWidthLeft, fRoadWidthRight;
			FindRoadBoundaries(rLink, fRoadWidthLeft, fRoadWidthRight);

			// Compute two dot products, both based on posDeltaV and nodeDirV.
			// One is in the link direction, the other in the orthogonal direction.
			const Vec4V dotTermsDeltaFwdAndSideV(posDeltaV.GetX(), negPosDeltaV.GetY(), posDeltaV.GetX(), posDeltaV.GetY());
			const Vec4V dotTermsNodeDirV        (nodeDirV.GetY(),  nodeDirV.GetX(),     nodeDirV.GetX(),  nodeDirV.GetY());
			const Vec4V dotTermsV = Scale(dotTermsDeltaFwdAndSideV, dotTermsNodeDirV);
			const ScalarV sideDistRawV = Add(dotTermsV.GetX(), dotTermsV.GetY());
			const ScalarV distAlongLinkLineV = Add(dotTermsV.GetZ(), dotTermsV.GetW());

			// Compute how far ahead or how far behind the link we are.
			// The equivalent old float style code for this was:
			//	const float fDistOutsideLinkAlongLinkLine = fDistAlongLinkLine < 0.0f 
			//		? -fDistAlongLinkLine
			//		: rage::Max(0.0f, fDistAlongLinkLine - fLinkLength);
			const ScalarV distOutsideLinkAlongLinkLineV = SelectFT(IsGreaterThanOrEqual(distAlongLinkLineV, zeroV),
					Negate(distAlongLinkLineV),
					Max(zeroV, Subtract(distAlongLinkLineV, linkLengthV)));

			// Determine some central reservation width. The original unoptimized version was:
			//	const float fCentralReservationWidth = rLink.m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL
			//		? 0.0f
			//		: rLink.m_1.m_Width * 1.0f;
			ScalarV centralReservationWidthV;
			if(rLink.m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL)
			{
				centralReservationWidthV = zeroV;
			}
			else
			{
				// Load from integer to vector.
				ScalarV widthAsInt;
				widthAsInt.Seti(rLink.m_1.m_Width);
				centralReservationWidthV = IntToFloatRaw<0>(widthAsInt);
			}

			// Load the road width into a vector.
			const ScalarV roadWidthRightV = LoadScalar32IntoScalarV(fRoadWidthRight);

			// Compute how far off we are laterally. Original code was:
			//	const float fDistOutsideExtent = rage::Max(0.0f, fabsf(fSideDistRaw) - fRoadWidthRight) 
			//		+ rage::Max(0.0f, fCentralReservationWidth - fabsf(fSideDistRaw))
			//		+ fDistOutsideLinkAlongLinkLine;
			const ScalarV absSideDistRawV = Abs(sideDistRawV);
			const ScalarV distOutsideExtentTerm1V = Max(zeroV, Subtract(absSideDistRawV, roadWidthRightV));
			const ScalarV distOutsideExtentTerm2V = Max(zeroV, Subtract(centralReservationWidthV, absSideDistRawV));
			const ScalarV distOutsideExtentV = Add(Add(distOutsideExtentTerm1V, distOutsideExtentTerm2V), distOutsideLinkAlongLinkLineV);
		
			// Compute how far off we are in the Z dimension.
			const ScalarV heightDeltaV = Abs(Subtract(closestPointV.GetZ(), positionV.GetZ()));

			// Compute the dot product between nodeDirV and forwardV, using only the XY components.
			const Vec3V nodeDirDotForwardTermsV = Scale(nodeDirV, forwardV);
			const ScalarV nodeDirDotForwardV = Add(nodeDirDotForwardTermsV.GetX(), nodeDirDotForwardTermsV.GetY());

			const ScalarV oneV(V_ONE);

			// Start the computation of the actual error measure. Unoptimized code was:
			//	float errorMeasure = dist 
			//		+ PenaltyMultiplierForAlignment * (1.0f - nodeDir.Dot(forward)) 
			//		+ (lanesAvailable < 1 ? NoLanesPenalty : 0.0f) 
			//		+ PenaltyForDistOutsideOfExtents * fDistOutsideExtent;
			ScalarV errorMeasureV = AddScaled(distV, penaltyMultiplierForAlignmentV, Subtract(oneV, nodeDirDotForwardV));
			if(rLink.m_1.m_LanesToOtherNode < 1)
			{
				errorMeasureV = Add(errorMeasureV, noLanesPenaltyV);
			}
			errorMeasureV = AddScaled(errorMeasureV, penaltyForDistOutsideOfExtentsV, distOutsideExtentV);

			// If we care about direction, add a big score for links that don't align with us:
			//	if(!bIgnoreDirection && nodeDir.Dot(vForwardFlat) <= 0.0f)
			//	{
			//		errorMeasure += ExtraPenaltyForNonAlign;
			//	}
			if(!bIgnoreDirection)
			{
				const ScalarV lessThanV(IsLessThanOrEqual(nodeDirDotForwardV, zeroV).GetIntrin128());
				const ScalarV andedV = And(lessThanV, extraPenaltyForNonAlignV);
				errorMeasureV = Add(errorMeasureV, andedV);
			}

			// If the link is further away on the Z axis than MaxAllowedHeightDiff, add a penalty to this link's score.
			// This is equivalent to this unoptimized code:
			//	if (fHeightDelta > MaxAllowedHeightDiff)
			//	{
			//		errorMeasure += fHeightDelta + BasePenaltyForHeightDiff;
			//	}
			const ScalarV heightDeltaScaledV = Scale(heightDeltaV, HeightPenaltyScaleV);
			errorMeasureV = Add(errorMeasureV, And(
						ScalarV(IsGreaterThan(heightDeltaV, maxAllowedHeightDiffV).GetIntrin128()),
						Add(heightDeltaScaledV, basePenaltyForHeightDiffV)));

			// If the link is a shortcut link, don't prefer it.
			if(rLink.IsShortCut())
			{
				errorMeasureV = Scale(errorMeasureV, shortcutLinkMultiplierV);
			}

			if (bSourceNodeIsSwitchedOff || bDestNodeIsSwitchedOff)
			{
				//if either node is switched off, add a penalty equivalent to the maximum 
				//penalty that we could apply for a link being in the wrong direction
				errorMeasureV = Add(errorMeasureV, penaltyMultiplierForAlignment2V);
			}

			//aiDisplayf("%d:%d -> %d:%d = %.3f (%.3f)", pNode->m_address.GetRegion(), pNode->m_address.GetIndex()
			//	, otherNode.GetRegion(), otherNode.GetIndex(), errorMeasure, fDistOutsideExtent);

			// Check if this is any improvement over the previously smallest error.
			if(IsGreaterThanAll(errorMeasureV, smallestErrorMeasureV))
			{
				continue;
			}

			// Got a new smallest error, square it and store it.
			smallestErrorMeasureV = errorMeasureV;
			smallestErrorMeasureSqV = Scale(errorMeasureV, errorMeasureV);

			// Store a bunch of other variables we will need to compute some related output.
			// We don't do those other computations inside the loop, since we might still find
			// a better node pair than this.
			pBestLink = &rLink;
			pBestNode = pNode;
			pBestOtherNode = pOtherNode;
			bestSideDistRawV = sideDistRawV;
			bestLinkIndex = linkIndex;
		}
	}

	// Store the smallest error.
	StoreScalar32FromScalarV(smallestErrorMeasureInOut, smallestErrorMeasureV);

	// If we found a new link, compute some related output variables and store them for the user.
	if(pBestLink)
	{
		//we used to add a half lane-width for non single-tracked links, but that's taken care of
		//by getting the center lane offset here
		const float fCenterLaneOffset = pBestLink->InitialLaneCenterOffset();

		s8 nearestLane = (s8) rage::round(((bestSideDistRawV.Getf() - fCenterLaneOffset) / pBestLink->GetLaneWidth()));
		nodeToInOut = pBestOtherNode->m_address;
		nodeFromInOut = pBestNode->m_address;
		regionLinkIndexInOut =(s16)(pBestNode->m_startIndexOfLinks + bestLinkIndex);

		// The lane needs to be clipped against the lanes on this link.
		const s8 lanesAvailable = pBestLink->m_1.m_LanesToOtherNode;
		nearestLane = rage::Min(nearestLane, (s8)(lanesAvailable-1));
		nearestLane = rage::Max((s8)0, nearestLane);

		nearestLaneInOut = nearestLane;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodePairClosestToCoors
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate. Cars or peds depending on GraphType.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindNodePairClosestToCoors(const Vector3& SearchCoors, CNodeAddress *pNode1, CNodeAddress *pNode2, float *pOrientation, float MinDistApart, float CutoffDistForNodeSearch, bool bIgnoreSwitchedOff, bool bWaterNode, bool bHighwaysOnly, bool bSearchUpFromPosition, s32 RequiredLanes, s32 *pLanesToNode2, s32 *pLanesFromNode2, float *pCentralReservation, bool localRegionOnly)
{
	float			ClosestDist, ThisDist, Length;
	s32			StartNode=0, EndNode=0;
	u32			iLink;
	s32			Region, Node;
	CNodeAddress 	ClosestNode, ClosestOtherNode, otherNodeAddr;
	s32			ClosestLanesToOtherNode=0, ClosestLanesFromOtherNode=0;
	float			ClosestCentralReservation=0.0f;
	Vector3	Diff;

	ClosestNode.SetEmpty();
	ClosestOtherNode.SetEmpty();
	ClosestDist = FLT_MAX;
	s32 localRegion = FindRegionForCoors(SearchCoors.GetX(), SearchCoors.GetY());

	for (Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	{
		// Make sure this Region is loaded in.
		if(!apRegions[Region] || !apRegions[Region]->aNodes)		
		{
			continue;
		}

		if(localRegionOnly && Region != localRegion)
		{
			continue;
		}

		FindStartAndEndNodeBasedOnYRange(Region, SearchCoors.y - CutoffDistForNodeSearch, SearchCoors.y + CutoffDistForNodeSearch, StartNode, EndNode);
		
		for (Node = StartNode; Node < EndNode; Node++)
		{
			CPathNode	*pNode = &((apRegions[Region]->aNodes)[Node]);
			Vector3 nodeCoors;

			pNode->GetCoors(nodeCoors);

			if(	(bHighwaysOnly && !pNode->IsHighway()) ||
				(bSearchUpFromPosition && nodeCoors.z < SearchCoors.z) ||
				(bIgnoreSwitchedOff && pNode->m_2.m_switchedOff) ||
				(bWaterNode != static_cast<bool>(pNode->m_2.m_waterNode)))
			{
				continue;
			}
		
			// Make sure this is closer than the ones already found
			if((ThisDist =(rage::Abs(nodeCoors.x - SearchCoors.x)+ rage::Abs(nodeCoors.y - SearchCoors.y)+ 3.0f*rage::Abs(nodeCoors.z - SearchCoors.z))) >= ClosestDist)
			{
				continue;
			}
		
			// Go through all of the neighbours to find a pair.
			for(iLink = 0; iLink < pNode->NumLinks(); iLink++)
			{
				otherNodeAddr = GetNodesLinkedNodeAddr(pNode, iLink);
				if(!IsRegionLoaded(otherNodeAddr))
				{
					continue;
				}
			
				const CPathNode	*pOtherNode = FindNodePointer(otherNodeAddr);
				Vector3 otherNodeCoors;

				pOtherNode->GetCoors(otherNodeCoors);

				if(	(bHighwaysOnly && !pNode->IsHighway()) ||
					(bSearchUpFromPosition && otherNodeCoors.z < SearchCoors.z) ||
					(bIgnoreSwitchedOff && pOtherNode->m_2.m_switchedOff) ||
					(bWaterNode != static_cast<bool>(pOtherNode->m_2.m_waterNode)))
				{
					continue;
				}

				// Make sure we have at least the required number of lanes.
				s16 iLink = -1;
				const bool bLinkFound = FindNodesLinkIndexBetween2Nodes(pNode->m_address, otherNodeAddr, iLink);
				if(!bLinkFound)//can't find the link so just continue
				{
					continue;
				}

				const CPathNodeLink *pLink = &GetNodesLink(pNode, iLink);

				s32 LanesTo = pLink->m_1.m_LanesToOtherNode;
				s32 LanesFrom = pLink->m_1.m_LanesFromOtherNode;

				if(LanesTo + LanesFrom < RequiredLanes)
				{
					continue;
				}
			
				// Calculate the length between the two nodes here
				Length = (nodeCoors - otherNodeCoors).Mag();
				if(Length <= MinDistApart)
				{
					continue;
				}

				ClosestDist = ThisDist;
				ClosestNode.Set(Region, Node);
				ClosestOtherNode = otherNodeAddr;
				ClosestLanesToOtherNode = LanesTo;
				ClosestLanesFromOtherNode = LanesFrom;
				ClosestCentralReservation = static_cast<float>(GetNodesLink(pNode,iLink).m_1.m_Width);

				// The caller expects the nodes to be South first then North.
				// For East West roads it would be West first then East.
				if ( (nodeCoors.y < otherNodeCoors.y) ||
					((nodeCoors.y == otherNodeCoors.y) && (nodeCoors.x > otherNodeCoors.x)))
				{		// Swap the 2 nodes around
					CNodeAddress TempNode = ClosestNode;
					ClosestNode = ClosestOtherNode;
					ClosestOtherNode = TempNode;

					s32 TempLanes = ClosestLanesToOtherNode;
					ClosestLanesToOtherNode = ClosestLanesFromOtherNode;
					ClosestLanesFromOtherNode = TempLanes;
				}
			}
		}
	}

	if (ClosestDist < CutoffDistForNodeSearch)
	{
		*pNode1 = ClosestNode;
		*pNode2 = ClosestOtherNode;

		Assert(IsRegionLoaded(ClosestNode));
		const CPathNode* pNodeClosest = FindNodePointer(ClosestNode);
		Assert(IsRegionLoaded(ClosestOtherNode));
		const CPathNode* pNodeClosestOther = FindNodePointer(ClosestOtherNode);
		Vector3 closestCoors, otherClosestCoors;

		pNodeClosest->GetCoors(closestCoors);
		pNodeClosestOther->GetCoors(otherClosestCoors);

		Diff = closestCoors - otherClosestCoors;
		Diff.z = 0.0f;
		Diff.Normalize();
		*pOrientation = (( RtoD * rage::Atan2f(-Diff.x, Diff.y)));

		if (pLanesToNode2) *pLanesToNode2 = ClosestLanesToOtherNode;
		if (pLanesFromNode2) *pLanesFromNode2 = ClosestLanesFromOtherNode;
		if (pCentralReservation) *pCentralReservation = ClosestCentralReservation;
	}
	else
	{
		pNode1->SetEmpty();
		pNode2->SetEmpty();
		*pOrientation = 0.0f;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindStartAndEndNodeBasedOnYRange
// PURPOSE :  Works out at what node we need to start looking and when we can stop.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindStartAndEndNodeBasedOnYRange(s32 Region, float searchMinY, float searchMaxY, s32 &startNode, s32 &endNode) const
{
	// As nodes are sorted by Y do a binary search to find both start and end nodes
	CPathNode* begin = &apRegions[Region]->aNodes[0];
	CPathNode* end = &apRegions[Region]->aNodes[apRegions[Region]->NumNodesCarNodes];

	CPathNode key;

	// This is like COORS_TO_INT16(searchMinY), but clamped to the range that we can represent with an s16.
	const int searchMinYInt = (int)(searchMinY*PATHCOORD_XYSHIFT);
	const int searchMinYIntClamped = Clamp(searchMinYInt, -0x8000, 0x7fff);
	key.m_coorsY = (s16)searchMinYIntClamped;

	CPathNode* ret = std::lower_bound(begin, end, key, PathNodeYCompare);
	startNode = ptrdiff_t_to_int(ret - begin);

	// This is like COORS_TO_INT16(searchMaxY), but clamped to the range that we can represent with an s16.
	const int searchMaxYInt = (int)(searchMaxY*PATHCOORD_XYSHIFT);
	const int searchMaxYIntClamped = Clamp(searchMaxYInt, -0x8000, 0x7fff);
	key.m_coorsY = (s16)searchMaxYIntClamped;

	ret = std::upper_bound(ret, end, key, PathNodeYCompare);
	endNode = ptrdiff_t_to_int(ret - begin);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodePairWithLineClosestToCoors
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindNodePairWithLineClosestToCoors(const Vector3& SearchCoors, CNodeAddress *pNode1, CNodeAddress *pNode2, float *pOrientation, float MinDistApart, float CutOffDist, bool bIgnoreSwitchedOff, bool bWaterNode, s32 RequiredLanes, s32 *pLanesToNode2, s32 *pLanesFromNode2, float *pCentralReservation, Vector3 *pClosestCoorsOnLine, const Vector3 *pForward, float orientationPenaltyMult)
{
	bool			bOrientationPenalty = false;
	Vector3			orientationHeading;
	if (pForward && orientationPenaltyMult > 0.0f)
	{
		bOrientationPenalty = true;
		orientationHeading = *pForward;
	}

	// Setup our search scratch vars.
	float			ClosestDist = CutOffDist;
	CNodeAddress 	ClosestNode, ClosestOtherNode;
	ClosestNode.SetEmpty();
	ClosestOtherNode.SetEmpty();
	s32			ClosestLanesToOtherNode=0, ClosestLanesFromOtherNode=0;
	float			ClosestCentralReservation=0.0f;
	Vector3			ClosestCoorsOnLine;

	float	searchMinX = SearchCoors.x - CutOffDist - 60.0f;
	float	searchMaxX = SearchCoors.x + CutOffDist + 60.0f;
	float	searchMinY = SearchCoors.y - CutOffDist - 60.0f;
	float	searchMaxY = SearchCoors.y + CutOffDist + 60.0f;

	for(s32 Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	{
		// Make sure this Region is loaded in.
		if(!apRegions[Region] || !apRegions[Region]->aNodes)
		{
			continue;
		}
	
			// We only need to go through this Region if it's close enough
			s32 regionX = FIND_X_FROM_REGION_INDEX(Region);
			s32 regionY = FIND_Y_FROM_REGION_INDEX(Region);
			float	regionMinX = FindXCoorsForRegion(regionX);
			float	regionMaxX = regionMinX + PATHFINDREGIONSIZEX;
			float	regionMinY = FindYCoorsForRegion(regionY);
			float	regionMaxY = regionMinY + PATHFINDREGIONSIZEY;
		if(searchMaxX <= regionMinX || searchMinX >= regionMaxX ||
			searchMaxY <= regionMinY || searchMinY >= regionMaxY)
			{
			continue;
		}

		s32 StartNode=0, EndNode=0;
				FindStartAndEndNodeBasedOnYRange(Region, searchMinY, searchMaxY, StartNode, EndNode);
		for(s32 Node = StartNode; Node < EndNode; Node++)
				{
					CPathNode	*pNode = &((apRegions[Region]->aNodes)[Node]);

			if((bIgnoreSwitchedOff && pNode->m_2.m_switchedOff) ||
				(bWaterNode != static_cast<bool>(pNode->m_2.m_waterNode)))
					{
				continue;
			}
		
						Vector3 nodeCoors;
						pNode->GetCoors(nodeCoors);

						// Make sure this is close enough that it could be a candidate.
			if((rage::Abs(nodeCoors.x - SearchCoors.x)+ rage::Abs(nodeCoors.y - SearchCoors.y)+ 3.0f*rage::Abs(nodeCoors.z - SearchCoors.z)) >= CutOffDist + 50.0f)
						{
				continue;
			}
		
							// Go through all of the neighbours to find a pair.
			for(u32 iLink = 0; iLink < pNode->NumLinks(); iLink++)
							{
				CNodeAddress OtherNode = GetNodesLinkedNodeAddr(pNode, iLink);
				if(!IsRegionLoaded(OtherNode))
								{
					continue;
				}

									// To avoid unnecessary work we only evaluate each pair once
				if(OtherNode < pNode->m_address)
									{
					continue;
				}
			
				const CPathNode	*pOtherNode = GetNodesLinkedNode(pNode, iLink);

				if((bIgnoreSwitchedOff && pOtherNode->m_2.m_switchedOff) ||
					(bWaterNode != static_cast<bool>(pOtherNode->m_2.m_waterNode)))
										{
					continue;
				}

											// Make sure we have at least the required number of lanes.
				const CPathNodeLink *pLink = &GetNodesLink(pNode, iLink);
				s32 LanesTo = pLink->m_1.m_LanesToOtherNode;
				s32 LanesFrom = pLink->m_1.m_LanesFromOtherNode;

				if(LanesTo + LanesFrom < RequiredLanes)
											{
					continue;
				}
			
												Vector3 otherNodeCoors;
												pOtherNode->GetCoors(otherNodeCoors);

												// Calculate the length between the two nodes here
												Vector3 vAlongNodes = nodeCoors - otherNodeCoors;
				float Length = vAlongNodes.Mag();

				if(Length <= MinDistApart)
												{
					continue;
				}

													// Calculate the shortest distance from this line segment to the specified point.
													Vector3	CoorsOnLine;
													fwGeom::fwLine(nodeCoors, otherNodeCoors).FindClosestPointOnLine(SearchCoors, CoorsOnLine);
													float	ActualDist = (SearchCoors - CoorsOnLine).Mag();

				if(ActualDist >= ClosestDist)
													{
					continue;
				}
			
														// We may want to apply a penalty if the orientation of the car that is looking for nodes
														// and the orientation of the nodes are different.
														if (bOrientationPenalty)
														{
															Vector3 crosspr;
															crosspr.Cross(orientationHeading, vAlongNodes);
															ActualDist += orientationPenaltyMult * crosspr.Mag() / Length;
														}

														if (ActualDist < ClosestDist)
														{
															ClosestDist = ActualDist;
															ClosestNode.Set(Region, Node);
															ClosestOtherNode = OtherNode;
															ClosestLanesToOtherNode = LanesTo;
															ClosestLanesFromOtherNode = LanesFrom;
					ClosestCentralReservation = static_cast<float>(GetNodesLink(pNode,iLink).m_1.m_Width);
															ClosestCoorsOnLine = CoorsOnLine;
														}
			}// END for(iLink = 0; iLink < pNode->NumLinks(); iLink++)
		}// END for(Node = StartNode; Node < EndNode; Node++)
	}// END for(Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)

	// Check if we have found a suitable pair
	if(ClosestDist < CutOffDist)
	{
		*pNode1 = ClosestNode;
		*pNode2 = ClosestOtherNode;

		Assert(IsRegionLoaded(ClosestNode));
		const CPathNode* pNodeClosest = FindNodePointer(ClosestNode);
		Assert(IsRegionLoaded(ClosestOtherNode));
		const CPathNode* pNodeClosestOther = FindNodePointer(ClosestOtherNode);

		Vector3 closestCoors,otherClosetsCoors;
		pNodeClosest->GetCoors(closestCoors);
		pNodeClosestOther->GetCoors(otherClosetsCoors);

		Vector3 Diff = closestCoors - otherClosetsCoors;
		Diff.z = 0.0f;
		Diff.Normalize();
		*pOrientation = (( RtoD * rage::Atan2f(-Diff.x, Diff.y)));

		if (pLanesToNode2) *pLanesToNode2 = ClosestLanesToOtherNode;
		if (pLanesFromNode2) *pLanesFromNode2 = ClosestLanesFromOtherNode;
		if (pCentralReservation) *pCentralReservation = ClosestCentralReservation;
		if (pClosestCoorsOnLine) *pClosestCoorsOnLine = ClosestCoorsOnLine;
	}
	else
	{
		pNode1->SetEmpty();
		pNode2->SetEmpty();
		*pOrientation = 0.0f;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeOrientationForCarPlacement
// PURPOSE :  Given a car node we return an orientation as used by the script
//			  so that Chris can place his cars safely.
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::FindNodeOrientationForCarPlacement(CNodeAddress CarNode, float DirX, float DirY, int *pNumLanes)
{
	Assert(IsRegionLoaded(CarNode));
	Vector3			Diff;
	CNodeAddress	LinkNodeAddress;
	u32			LinkToUse;

	if(!IsRegionLoaded(CarNode))
	{
		return 0.0f;		// This segment of the map isn't loaded in.
	}

	const CPathNode	*pNode = FindNodePointer(CarNode);

	// Find the first link and calculate the orientation to that one.
	if(pNode->NumLinks() == 0)return 0.0f;

	float	BestAngle=0.0f, BestShortcutAngle=0.0f;
	float	BestNodeDotProduct = -99999.0f, BestNodeDotProductWithShortcutLinks = -99999.0f;
	int iNumLanes=1, iNumLanesShortcut=1;
	Vector3 nodeCoors;
	bool bFoundNonShortcutLink = false;

	pNode->GetCoors(nodeCoors);

	for(LinkToUse = 0; LinkToUse < pNode->NumLinks(); LinkToUse++)
	{
		const CPathNodeLink *pLink = &GetNodesLink(pNode, LinkToUse);

		if (pLink->m_1.m_LanesToOtherNode)
		{
			// LinkToUse should now point to a node that actually has lanes leaving the current node.
			LinkNodeAddress = GetNodesLinkedNodeAddr(pNode, LinkToUse);
			if(!IsRegionLoaded(LinkNodeAddress))
			{
				return 0.0f;		// This segment of the map isn't loaded in.
			}
			const CPathNode	*pNode = FindNodePointer(LinkNodeAddress);
			Vector3 adjNodeCoors;
			pNode->GetCoors(adjNodeCoors);

			Diff = adjNodeCoors - nodeCoors;

			Diff.z = 0.0f;
			Diff.Normalize();

			float	DotPr = Diff.x * DirX + Diff.y * DirY;

			if (!pLink->IsShortCut())
			{
				if (DotPr > BestNodeDotProduct)
				{		
					// This is the new best node.
					BestNodeDotProduct = DotPr;
					BestAngle = ( RtoD * rage::Atan2f(-Diff.x, Diff.y));
					bFoundNonShortcutLink = true;

					iNumLanes = pLink->m_1.m_LanesToOtherNode + pLink->m_1.m_LanesFromOtherNode;
				}
			}
			else if (!bFoundNonShortcutLink)	//don't bother checking these if we've already found a shortcut link
			{
				if (DotPr > BestNodeDotProductWithShortcutLinks)
				{		// This is the new best node.
					BestNodeDotProductWithShortcutLinks = DotPr;
					BestShortcutAngle = ( RtoD * rage::Atan2f(-Diff.x, Diff.y));

					iNumLanesShortcut = pLink->m_1.m_LanesToOtherNode + pLink->m_1.m_LanesFromOtherNode;
				}
			}

			//		if (pLink->m_1.m_LanesToOtherNode) break;	// This one will do. Jump out.
		}
	}

	//if we didn't find any non shortcut links, copy over
	//the best shortcut link. this may be uninitialized
	if (!bFoundNonShortcutLink)
	{
		BestNodeDotProduct = BestNodeDotProductWithShortcutLinks;
		BestAngle = BestShortcutAngle;
		iNumLanes = iNumLanesShortcut;
	}

	if (pNumLanes)
	{
		*pNumLanes = iNumLanes;
	}

	if(BestNodeDotProduct < -9999.0f)		// It's possible we haven't found anything.
	{
		return 0.0f;
	}

	// To make things a bit easier for the scripters make sure value is in 0-360 range
	while (BestAngle < 0.0f) BestAngle += 360.0f;
	while (BestAngle >= 360.0f) BestAngle -= 360.0f;

	return (BestAngle);
}

//Returns the link index of CarNode that is most orientated with heading
int CPathFind::FindBestLinkFromHeading(CNodeAddress CarNode, float heading)
{
	Assert(IsRegionLoaded(CarNode));
	Vector3			Diff;
	CNodeAddress	LinkNodeAddress;
	u32			LinkToUse;

	if(!IsRegionLoaded(CarNode))
	{
		return -1;
	}

	const CPathNode	*pNode = FindNodePointer(CarNode);

	// Find the first link and calculate the orientation to that one.
	if(pNode->NumLinks() == 0)
	{
		return -1;
	}

	float	BestHeadingDiff = 99999.0f, BestShortcutHeadingDiff = 99999.0f;
	bool bFoundNonShortcutLink = false;
	int iBestLinkId = -1;
	int iShortcutLinkId = 0;

	Vector3 nodeCoors;
	pNode->GetCoors(nodeCoors);

	for(LinkToUse = 0; LinkToUse < pNode->NumLinks(); LinkToUse++)
	{
		const CPathNodeLink *pLink = &GetNodesLink(pNode, LinkToUse);

		if (pLink->m_1.m_LanesToOtherNode)
		{
			// LinkToUse should now point to a node that actually has lanes leaving the current node.
			LinkNodeAddress = GetNodesLinkedNodeAddr(pNode, LinkToUse);
			if(!IsRegionLoaded(LinkNodeAddress))
			{
				return -1;		// This segment of the map isn't loaded in.
			}
			const CPathNode	*pNode = FindNodePointer(LinkNodeAddress);
			Vector3 adjNodeCoors;
			pNode->GetCoors(adjNodeCoors);

			Diff = adjNodeCoors - nodeCoors;

			Diff.z = 0.0f;
			Diff.Normalize();

			float fHeading = (( RtoD * rage::Atan2f(-Diff.x, Diff.y)));
			if(fHeading < 0.0f) fHeading += 360.0f;
			float fHeadingDiff = fabsf(heading - fHeading);
			
			if (!pLink->IsShortCut())
			{
				if (fHeadingDiff < BestHeadingDiff)
				{		
					// This is the new best node.
					BestHeadingDiff = fHeadingDiff;
					bFoundNonShortcutLink = true;
					iBestLinkId = LinkToUse;
				}
			}
			else if (!bFoundNonShortcutLink)	//don't bother checking these if we've already found a shortcut link
			{
				if (fHeadingDiff < BestShortcutHeadingDiff)
				{		// This is the new best node.
					BestShortcutHeadingDiff = fHeadingDiff;
					iShortcutLinkId = LinkToUse;
				}
			}
		}
	}

	//if we didn't find any non shortcut links, copy over
	//the best shortcut link. this may be uninitialized
	if (!bFoundNonShortcutLink)
	{
		BestHeadingDiff = BestShortcutHeadingDiff;
		iBestLinkId = iShortcutLinkId;
	}

	return iBestLinkId;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodePairWithLineClosestToCoors
// PURPOSE :  Goes through the nodes on the map and finds the one closest to the
//			  specified coordinate.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::IsNodeNearbyInZ(const Vector3& SearchCoors) const
{
	float	searchMinX = SearchCoors.x - 50.0f;
	float	searchMaxX = SearchCoors.x + 50.0f;
	float	searchMinY = SearchCoors.y - 50.0f;
	float	searchMaxY = SearchCoors.y + 50.0f;

	for (s32 Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	{
		// Make sure this Region is loaded in.
		if(!apRegions[Region] || !apRegions[Region]->aNodes)
		{
			continue;
		}

		// We only need to go through this Region if it's close enough
		s32 regionX = FIND_X_FROM_REGION_INDEX(Region);
		s32 regionY = FIND_Y_FROM_REGION_INDEX(Region);
		float	regionMinX = FindXCoorsForRegion(regionX);
		float	regionMaxX = regionMinX + PATHFINDREGIONSIZEX;
		float	regionMinY = FindYCoorsForRegion(regionY);
		float	regionMaxY = regionMinY + PATHFINDREGIONSIZEY;
		if(	searchMaxX <= regionMinX || searchMinX >= regionMaxX ||
			searchMaxY <= regionMinY || searchMinY >= regionMaxY)
		{
			continue;
		}

		s32	StartNode, EndNode;
		FindStartAndEndNodeBasedOnYRange(Region, searchMinY, searchMaxY, StartNode, EndNode);

		for (s32 Node = StartNode; Node < EndNode; Node++)
		{
			const CPathNode *pNode = &((apRegions[Region]->aNodes)[Node]);

			if(	pNode->m_2.m_switchedOff ||
				pNode->HasSpecialFunction_Driving() ||
				(pNode->NumLinks() <= 0))
			{
				continue;
			}
			
			Vector3 nodeCoors;
			pNode->GetCoors(nodeCoors);

			if (rage::Abs(nodeCoors.z - SearchCoors.z) < 10.0f)
			{
				return true;
			}
		}
	}
	return false;
}


#if RSG_ORBIS || RSG_PC || RSG_DURANGO
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindStreetNameAtPosition
// PURPOSE :  Finds the 2 nearest street names near the specified coordinates.
//			  The nearest one is passed back first.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindStreetNameAtPosition(const Vector3& SearchCoors, u32 &streetName1Hash, u32 &streetName2Hash ) const
{
	float	streetName1Dist = 999999.9f;
	float	streetName2Dist = 999999.9f;

	streetName1Hash = 0;
	streetName2Hash = 0;

	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if (apRegions[Region] && apRegions[Region]->NumNodes)	// Make sure this Region is loaded in at the moment
		{
			s32 StartNode = 0;
			s32 EndNode = apRegions[Region]->NumNodesCarNodes;

			for (s32 Node = StartNode; Node < EndNode; Node++)
			{
				CPathNode	*pNode = &apRegions[Region]->aNodes[Node];

				u32	nameHash = pNode->m_streetNameHash;
				if (nameHash)		// Make sure this node has a name.
				{
					Vector3 nodeCoors;
					pNode->GetCoors(nodeCoors);
					float	dist = (SearchCoors - nodeCoors).XYMag();

					// Are we close enough to replace the best candidate so far?

					if (dist < streetName1Dist)
					{
						if (nameHash == streetName1Hash)		// We already know about name. Just replace dist.
						{
							streetName1Dist = dist;
						}
						else
						{
							// Shift best to 2nd best
							streetName2Hash = streetName1Hash;
							streetName2Dist = streetName1Dist;
							// Replace best with new one
							streetName1Hash = nameHash;
							streetName1Dist = dist;
						}
					}
					else if (nameHash != streetName1Hash)
					{
						if (dist < streetName2Dist)
						{
							// Replace 2nd best with new one
							streetName2Hash = nameHash;
							streetName2Dist = dist;
						}
					}
				}
			}
		}
	}
	// If the 2nd node is too far away we ingore it.
	if (streetName2Dist > 40.0f)
	{
		streetName2Hash = 0;
	}
	Assert( (streetName1Hash != streetName2Hash) || streetName1Hash == 0);
}
#else
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindStreetNameAtPosition
// PURPOSE :  Finds the 2 nearest street names near the specified coordinates.
//			  The nearest one is passed back first.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindStreetNameAtPosition(const Vector3& SearchCoors, u32 &streetName1Hash, u32 &streetName2Hash ) const
{
	float	streetName1DistSq = FLT_MAX;
	float	streetName2DistSq = FLT_MAX;

	streetName1Hash = 0;
	streetName2Hash = 0;

	// To optimise this 
	// We start with the Region that the Searchcoors are within.
	// If we don't find a node or there is a possibility of finding an even
	// closer node we continue to look in ever expanding areas.

	// Find the X and Y of the Region we're in.
	s32 CenterRegionX = FindXRegionForCoors(SearchCoors.x);
	s32 CenterRegionY = FindYRegionForCoors(SearchCoors.y);
	u16 Region = (u16)(FIND_REGION_INDEX(CenterRegionX, CenterRegionY));
	float DistToBorderOfRegion = SearchCoors.x - FindXCoorsForRegion(CenterRegionX);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindXCoorsForRegion(CenterRegionX+1) - SearchCoors.x);
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, SearchCoors.y - FindYCoorsForRegion(CenterRegionY));
	DistToBorderOfRegion = rage::Min(DistToBorderOfRegion, FindYCoorsForRegion(CenterRegionY+1) - SearchCoors.y);
	FindStreetNameAtPosition(SearchCoors, Region, streetName1Hash, streetName2Hash, streetName1DistSq, streetName2DistSq);

	// If we cannot possibly find a closer node we stop here.
	bool bDone = (streetName1DistSq < square(DistToBorderOfRegion)) && (streetName2DistSq < square(DistToBorderOfRegion));
	if (!bDone)
	{
		// Expand the search to a wider area. (The border of a square)
		for (s32 Area = 1; Area < 5; Area++)
		{
			s32 RegionX = CenterRegionX - Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (s32 RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						FindStreetNameAtPosition(SearchCoors, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), streetName1Hash, streetName2Hash, streetName1DistSq, streetName2DistSq);
					}
				}
			}
			RegionX = CenterRegionX + Area;
			if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
			{
				for (s32 RegionY = CenterRegionY - Area; RegionY <= CenterRegionY + Area; RegionY++)
				{
					if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
					{
						FindStreetNameAtPosition(SearchCoors, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), streetName1Hash, streetName2Hash, streetName1DistSq, streetName2DistSq);
					}
				}
			}

			s32 RegionY = CenterRegionY - Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						FindStreetNameAtPosition(SearchCoors, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), streetName1Hash, streetName2Hash, streetName1DistSq, streetName2DistSq);
					}
				}
			}
			RegionY = CenterRegionY + Area;
			if (RegionY >= 0 && RegionY < PATHFINDMAPSPLIT)
			{
				for (RegionX = CenterRegionX - Area + 1; RegionX < CenterRegionX + Area; RegionX++)
				{
					if (RegionX >= 0 && RegionX < PATHFINDMAPSPLIT)
					{
						FindStreetNameAtPosition(SearchCoors, (u16)(FIND_REGION_INDEX(RegionX, RegionY)), streetName1Hash, streetName2Hash, streetName1DistSq, streetName2DistSq);
					}
				}
			}

			DistToBorderOfRegion += rage::Min(PATHFINDREGIONSIZEX, PATHFINDREGIONSIZEY);
			// If we cannot possibly find a closer node we stop here.
			bDone = (streetName1DistSq < square(DistToBorderOfRegion)) && (streetName2DistSq < square(DistToBorderOfRegion));
			if(bDone) break;
		}
	}

	// If the 2nd node is too far away we ingore it.
	if (streetName2DistSq > square(40.0f))
	{
		streetName2Hash = 0;
	}
	Assert( (streetName1Hash != streetName2Hash) || streetName1Hash == 0);
}

void CPathFind::FindStreetNameAtPosition(const Vector3& SearchCoors, u16 Region, u32 &streetName1Hash, u32 &streetName2Hash, float& streetName1DistSq, float& streetName2DistSq ) const
{
	if (apRegions[Region] && apRegions[Region]->NumNodes)	// Make sure this Region is loaded in at the moment
	{
		float fMaxDistSq = Max(streetName1DistSq, streetName2DistSq);
		float fMaxDist = sqrt(fMaxDistSq);
		float fMinY = SearchCoors.y - fMaxDist;
		float fMaxY = SearchCoors.y + fMaxDist;

		s32 StartNode = 0;
		s32 EndNode = 0;
		FindStartAndEndNodeBasedOnYRange(Region, fMinY, fMaxY, StartNode, EndNode);

		for (s32 Node = StartNode; Node < EndNode; Node++)
		{
			CPathNode	*pNode = &apRegions[Region]->aNodes[Node];

			u32	nameHash = pNode->m_streetNameHash;
			if (nameHash)		// Make sure this node has a name.
			{
				Vector3 nodeCoors;
				pNode->GetCoors(nodeCoors);
				float	distSq = (SearchCoors - nodeCoors).XYMag2();

				// Are we close enough to replace the best candidate so far?

				if (distSq < streetName1DistSq)
				{
					if (nameHash == streetName1Hash)		// We already know about name. Just replace dist.
					{
						streetName1DistSq = distSq;
					}
					else
					{
						// Shift best to 2nd best
						streetName2Hash = streetName1Hash;
						streetName2DistSq = streetName1DistSq;
						// Replace best with new one
						streetName1Hash = nameHash;
						streetName1DistSq = distSq;
					}
				}
				else if (nameHash != streetName1Hash)
				{
					if (distSq < streetName2DistSq)
					{
						// Replace 2nd best with new one
						streetName2Hash = nameHash;
						streetName2DistSq = distSq;
					}
				}
			}
		}
	}
}
#endif // RSG_ORBIS || RSG_PC || RSG_DURANGO


bool CPathFind::FindTrailingLinkWithinDistance(CNodeAddress StartNode, const Vector3& vStartPos)
{
	const int nMaxNodes = 32;
	int iNode = 0;
	int nNode = 1;
	static bank_float fMaxSearchDist = 30.f;
	static bank_float fClosestDist = 10.f;
	Vector3 vStartNodePos;
	CNodeAddress lPathNodeBuffer[nMaxNodes];
	const CPathNode* pCurrNode = ThePaths.FindNodePointerSafe(StartNode);
	if (!pCurrNode)
		return false;

	lPathNodeBuffer[0] = StartNode;
	pCurrNode->GetCoors(vStartNodePos);
	bool bNoGps = pCurrNode->m_2.m_noGps;

	while (pCurrNode && iNode < nMaxNodes)
	{
		Vector3 vCurrCoors;
		pCurrNode->GetCoors(vCurrCoors);

		for (int i = 0; i < pCurrNode->NumLinks(); ++i)
		{
			const CPathNodeLink* pLinkTo = &ThePaths.GetNodesLink(pCurrNode, i);
			Assertf(pLinkTo, "FixupTrailingGpsPath, Link is not loaded!");

			if (pLinkTo->m_1.m_bShortCut)
				continue;

			const CPathNode* pOtherNode = ThePaths.FindNodePointerSafe(pLinkTo->m_OtherNode);
			if (!pOtherNode)
				continue;

			if (!bNoGps && pOtherNode->m_2.m_noGps)
				continue;

			Vector3 vOtherCoors;
			pOtherNode->GetCoors(vOtherCoors);

			if (vOtherCoors.Dist2(vStartNodePos) > fMaxSearchDist * fMaxSearchDist)
				continue;

			int j;
			for (j = 0; j < nNode; ++j)
			{
				if (pLinkTo->m_OtherNode == lPathNodeBuffer[j])
					break;
			}

			if (j == nNode && nNode < nMaxNodes)
			{
				lPathNodeBuffer[nNode++] = pLinkTo->m_OtherNode;
			}

			const fwGeom::fwLine line(vCurrCoors, vOtherCoors);
			Vector3 vClosestPointOnLine;
			line.FindClosestPointOnLine(vStartPos, vClosestPointOnLine);
			float fDistance = vClosestPointOnLine.Dist(vStartPos);
			if (fDistance < fClosestDist)
				return true;

		}
		
		pCurrNode = ThePaths.FindNodePointerSafe(lPathNodeBuffer[iNode++]);
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SwitchRoadsOffInArea
// PURPOSE :  Switches the roads in a particular area off. No new cars are being
//			  generated here and cars don't go this way. (Takes a while for the
//			  area to clear though)
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::SwitchRoadsOffInArea(u32	scriptThreadId, float MinX, float MaxX, float MinY, float MaxY, float MinZ, float MaxZ, bool SwitchRoadsOff, bool bCars, bool bBackToOriginal, bool bOnlyForDurationOfMission)
{
	CNodesSwitchedOnOrOff node(MinX, MaxX, MinY, MaxY, MinZ, MaxZ, SwitchRoadsOff);
	node.scriptThreadId = scriptThreadId;
	node.bOnlyForDurationOfMission = bOnlyForDurationOfMission;

	SwitchRoadsOffInArea( node, bCars, bBackToOriginal );
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SwitchRoadsOffInAngledArea
// PURPOSE :  Switches the roads in a particular area off. No new cars are being
//			  generated here and cars don't go this way. (Takes a while for the
//			  area to clear though)
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::SwitchRoadsOffInAngledArea(u32	scriptThreadId, Vector3& vAreaStart, Vector3& vAreaEnd, float AreaWidth, bool SwitchRoadsOff, bool bCars, bool bBackToOriginal, bool bOnlyForDurationOfMission)
{
	CNodesSwitchedOnOrOff node(vAreaStart, vAreaEnd, AreaWidth, SwitchRoadsOff);
	node.scriptThreadId = scriptThreadId;
	node.bOnlyForDurationOfMission = bOnlyForDurationOfMission;

	SwitchRoadsOffInArea( node, bCars, bBackToOriginal );
}

bool AreAngledAreasIdentical(
	const Vector3 & vArea1_Pos1, const Vector3 & vArea1_Pos2, const float fArea1_Radius,
	const Vector3 & vArea2_Pos1, const Vector3 & vArea2_Pos2, const float fArea2_Radius,
	const float fEpsilon)
{
	if( vArea1_Pos1.IsClose( vArea2_Pos1, fEpsilon ) && vArea1_Pos2.IsClose( vArea2_Pos2, fEpsilon ) && IsClose( fArea1_Radius, fArea2_Radius, fEpsilon ) )
	{
		return true;
	}
	return false;
}

bool IsNodeAreaInNodeArea(const CNodesSwitchedOnOrOff & area1, const CNodesSwitchedOnOrOff & area2)
{
	const float fEps = 1.0f;

	if (area1.bAxisAlignedSwitch==false && area2.bAxisAlignedSwitch==false)
	{
		// Both boxes angled areas
		Vector3 vArea1Start = area1.Angled.vAreaStart.Get();
		Vector3 vArea1End   = area1.Angled.vAreaEnd.Get();
		Vector3 vArea2Start = area2.Angled.vAreaStart.Get();
		Vector3 vArea2End   = area2.Angled.vAreaEnd.Get();

		return AreAngledAreasIdentical(vArea1Start, vArea1End, area1.Angled.AreaWidth, vArea2Start, vArea2End, area2.Angled.AreaWidth, fEps);

//		return CScriptAreas::IsAngledAreaInAngledArea(vArea1Start, vArea1End, area1.Angled.AreaWidth, true,
//			vArea2Start, vArea2End, area2.Angled.AreaWidth, false, true, false);
	}
	if (area1.bAxisAlignedSwitch==true && area2.bAxisAlignedSwitch==true)
	{
		// Both boxes axis aligned
		return  area1.AA.MinX >= area2.AA.MinX &&
			area1.AA.MinY >= area2.AA.MinY &&
			area1.AA.MaxX <= area2.AA.MaxX &&
			area1.AA.MaxY <= area2.AA.MaxY &&
			area1.AA.MinZ >= area2.AA.MinZ &&
			area1.AA.MaxZ <= area2.AA.MaxZ;
	}
	else if (area2.bAxisAlignedSwitch==true)
	{
		// Area1 is angled, area2 axis aligned
		// make an 'angled' box for the second area (even though it is axis aligned)
		Vector3 vArea1Start = area1.Angled.vAreaStart.Get();
		Vector3 vArea1End   = area1.Angled.vAreaEnd.Get();
		float Area2Width = area2.AA.MaxX - area2.AA.MinX;
		Vector3 vArea2Start(area2.AA.MinX + Area2Width*0.5f, area2.AA.MaxY, area2.AA.MaxZ);
		Vector3 vArea2End(vArea2Start.x, area2.AA.MinY, area2.AA.MinZ);

		return AreAngledAreasIdentical(vArea1Start, vArea1End, area1.Angled.AreaWidth, vArea2Start, vArea2End, Area2Width, fEps);

//		return CScriptAreas::IsAngledAreaInAngledArea(vArea1Start, vArea1End, area1.Angled.AreaWidth, true,
//			vArea2Start, vArea2End, Area2Width, false, true, false);
	}
	else
	{
		// Area1 is axis aligned, area2 is angled
		// make an 'angled' box for the first area (even though it is axis aligned)
		float Area1Width = area1.AA.MaxX - area1.AA.MinX;
		Vector3 vArea1Start(area1.AA.MinX + Area1Width*0.5f, area1.AA.MaxY, area1.AA.MaxZ);
		Vector3 vArea1End(vArea1Start.x, area1.AA.MinY, area1.AA.MinZ);
		Vector3 vArea2Start(area2.Angled.vAreaStart.Get());
		Vector3 vArea2End(area2.Angled.vAreaEnd.Get());

		return AreAngledAreasIdentical(vArea1Start, vArea1End, Area1Width, vArea2Start, vArea2End, area2.Angled.AreaWidth, fEps);

//		return CScriptAreas::IsAngledAreaInAngledArea(vArea1Start, vArea1End, Area1Width, false,
//			vArea2Start, vArea2End, area2.Angled.AreaWidth, false, true, false);
	}
}


void CPathFind::SwitchRoadsOffInArea(const CNodesSwitchedOnOrOff & area, bool UNUSED_PARAM(bCars), bool bBackToOriginal)
{
	PF_AUTO_PUSH_DETAIL("SwitchRoadsOffInArea");
	s32	Region;
	s32	OldSwitch, ClearSwitch;

	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		SwitchRoadsOffInAreaForOneRegion(area, Region, bBackToOriginal);
	}

	// In order to make it so that blocks getting streamed in will have the appropriate nodes switched, we have to add this
	// info to a little list of Nodeswitches that we keep. If these coordinates are already present, the old one gets removed (or if the
	// new area is bigger than the old one)
	for (OldSwitch = 0; OldSwitch < NumNodeSwitches; OldSwitch++)
	{
		if ( !(area.bOnlyForDurationOfMission && !NodeSwitches[OldSwitch].bOnlyForDurationOfMission) &&
			IsNodeAreaInNodeArea(NodeSwitches[OldSwitch], area) )
		{		// This one can be removed
			for (ClearSwitch = OldSwitch; ClearSwitch < NumNodeSwitches-1; ClearSwitch++)
			{
				NodeSwitches[ClearSwitch] = NodeSwitches[ClearSwitch+1];
			}
			NumNodeSwitches--;
			OldSwitch--;
		}
	}	

	if (!bBackToOriginal)
	{
		bool bFoundDupe = false;
		for (int i = 0; i < NumNodeSwitches; i++)
		{
			if (area.Equals(NodeSwitches[i]))
			{
				bFoundDupe = true;
			}
		}
		
		if (!bFoundDupe)
		{
			Assertf(NumNodeSwitches < MAX_NUM_NODE_SWITCHES, "Missions have exceeded %i accumulated calls to SET_ROADS_IN_AREA or similar.  This is too many, there is probably some kind of leak/loop in script causing this error", MAX_NUM_NODE_SWITCHES);
			if (NumNodeSwitches < MAX_NUM_NODE_SWITCHES)
			{
				NodeSwitches[NumNodeSwitches] = area;
				NumNodeSwitches++;
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TidyUpNodeSwitchesAfterMission
// PURPOSE :  Called from mission cleanup
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::TidyUpNodeSwitchesAfterMission(u32 scriptThreadId)
{
	bool	bReDoNodeSwitches = false;

	// Remove all of the node switches that were specific to this mission.
	for (s32 Switch = 0; Switch < NumNodeSwitches; Switch++)
	{
		if (NodeSwitches[Switch].bOnlyForDurationOfMission && NodeSwitches[Switch].scriptThreadId == scriptThreadId)
		{
			bReDoNodeSwitches = true;

			for (s32 ClearSwitch = Switch; ClearSwitch < NumNodeSwitches-1; ClearSwitch++)
			{
				NodeSwitches[ClearSwitch] = NodeSwitches[ClearSwitch+1];
			}
			NumNodeSwitches--;
			Switch--;
		}
	}

	if (bReDoNodeSwitches)
	{
		for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
		{
			// First switch the nodes back to their original value.
			CNodesSwitchedOnOrOff resetNode(-10000.0f,10000.0f, -10000.0f,10000.0f, -10000.0f,10000.0f, false);
			SwitchRoadsOffInAreaForOneRegion(resetNode, Region, true);

			for (s32 NodeSwitch = 0; NodeSwitch < NumNodeSwitches; NodeSwitch++)
			{
				SwitchRoadsOffInAreaForOneRegion(NodeSwitches[NodeSwitch], Region, false);
			}
		}
	}

	// Make sure there are enough node switches available.
	Assert(NumNodeSwitches<MAX_NUM_NODE_SWITCHES);

	//-------------------------------------------------------------------
	// Also disable the GPS blocking region if it was set by this script
	for(int i = 0; i < MAX_GPS_DISABLED_ZONES; ++i)
	{
		if(m_GPSDisabledZones[i].m_iGpsBlockedByScriptID && m_GPSDisabledZones[i].m_iGpsBlockedByScriptID == scriptThreadId)
		{
			m_GPSDisabledZones[i].m_iGpsBlockedByScriptID = 0;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SwitchRoadsOffInAreaForOneRegion
// PURPOSE :  Switches the roads in a particular area off. No new cars are being
//			  generated here and cars don't go this way. (Takes a while for the
//			  area to clear though)
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::SwitchRoadsOffInAreaForOneRegion(const CNodesSwitchedOnOrOff & node, s32 Region, bool bBackToOriginal)
{
	
	if(!apRegions[Region] || !apRegions[Region]->aNodes)		// Make sure this Region is read in.
	{
		return;
	}

	s32	StartNode, EndNode, Node;

	// Find the X and Y min/max coordinates of this region.
	const int regX = FIND_X_FROM_REGION_INDEX(Region);
	const int regY = FIND_Y_FROM_REGION_INDEX(Region);
	const float thisRegMinX = FindXCoorsForRegion(regX);
	const float thisRegMaxX = thisRegMinX + PATHFINDREGIONSIZEX;
	const float thisRegMinY = FindYCoorsForRegion(regY);
	const float thisRegMaxY = thisRegMinY + PATHFINDREGIONSIZEY;

	spdAABB regionBox(Vec3V(thisRegMinX, thisRegMinY, 0.f), Vec3V(thisRegMaxX, thisRegMaxY, 0.f));
	spdAABB nodeBox;
	// see if the region intersects the switch at all
	if(node.bAxisAlignedSwitch)
	{		
		nodeBox.Set(Vec3V(node.AA.MinX, node.AA.MinY, node.AA.MinZ), Vec3V(node.AA.MaxX, node.AA.MaxY, node.AA.MaxZ));				
	}
	else
	{
		// angled area
		CScriptAngledArea angledArea;
		Vector3 pvOutMinMax[2]; // store the min/max extend of this region
		angledArea.Set(node.Angled.vAreaStart.Get(), node.Angled.vAreaEnd.Get(), node.Angled.AreaWidth, pvOutMinMax);
		pvOutMinMax[0].z = MIN(node.Angled.vAreaStart.z, node.Angled.vAreaEnd.z); // find min z
		pvOutMinMax[1].z = MAX(node.Angled.vAreaStart.z, node.Angled.vAreaEnd.z); // find max z
		nodeBox.Set(Vec3V(pvOutMinMax[0].x, pvOutMinMax[0].y, pvOutMinMax[0].z), Vec3V(pvOutMinMax[1].x,pvOutMinMax[1].y,pvOutMinMax[1].z));		
	}

	if(!nodeBox.IntersectsAABBFlat(regionBox))
	{
		return; // we don't intersect at all, get out of here
	}
	
	FindStartAndEndNodeBasedOnYRange(Region, nodeBox.GetMinVector3().GetY(), nodeBox.GetMaxVector3().GetY(), StartNode, EndNode);
	Assert(StartNode >= 0);
	Assert(EndNode <=  apRegions[Region]->NumNodesCarNodes);		
	
	for (Node = StartNode; Node < EndNode; Node++)
	{
		CPathNode	*pNode = &apRegions[Region]->aNodes[Node];
		Vector3 nodeCoors;
		pNode->GetCoors(nodeCoors);
		
		if(!nodeBox.ContainsPoint(VECTOR3_TO_VEC3V(nodeCoors)))
			continue;		

		if(!ThisNodeHasToBeSwitchedOff(pNode))
		{
			continue;
		}

		bool bNeedsChanged = false;
		if (bBackToOriginal)
		{
			if (pNode->m_2.m_switchedOff != pNode->m_2.m_switchedOffOriginal)
				bNeedsChanged = true;
		}
		else
		{
			if (static_cast<bool>(pNode->m_2.m_switchedOff) != node.bOff)
				bNeedsChanged = true;
		}
		if(!bNeedsChanged)
		{
			continue;
		}

		// The angled area check is really expensive, so put it off until the last moment.
		if ( !node.bAxisAlignedSwitch )
		{
			// Do the more expensive angled area check			
			Vector3 vAreaStart( node.Angled.vAreaStart.Get() );
			Vector3 vAreaEnd( node.Angled.vAreaEnd.Get() );
			if (!CScriptAreas::IsPointInAngledArea(nodeCoors, vAreaStart, vAreaEnd, node.Angled.AreaWidth, false, true, false)) 
				continue;
		}

		CPathNode	*pNextNodeToBeSwitchedOff1, *pNextNodeToBeSwitchedOff2;
		// Switch off the first node.
		SwitchOffNodeAndNeighbours(pNode, &pNextNodeToBeSwitchedOff1, &pNextNodeToBeSwitchedOff2, node.bOff, bBackToOriginal);
		// Switch off the first chain that may lead from this node.
		CPathNode	*pNodeToBeSwitchedOff = pNextNodeToBeSwitchedOff1;
		while (pNodeToBeSwitchedOff)
		{
			SwitchOffNodeAndNeighbours(pNodeToBeSwitchedOff, &pNextNodeToBeSwitchedOff1, NULL, node.bOff, bBackToOriginal);
			pNodeToBeSwitchedOff = pNextNodeToBeSwitchedOff1;
		}
		// Switch off the second chain that may lead from this node.
		pNodeToBeSwitchedOff = pNextNodeToBeSwitchedOff2;
		while (pNodeToBeSwitchedOff)
		{
			SwitchOffNodeAndNeighbours(pNodeToBeSwitchedOff, &pNextNodeToBeSwitchedOff2, NULL, node.bOff, bBackToOriginal);
			pNodeToBeSwitchedOff = pNextNodeToBeSwitchedOff2;
		}
	}
}


s32 CPathFind::GetNextAvailableGpsBlockingRegionForScript()
{
	for(int i = 0; i < MAX_GPS_DISABLED_ZONES; ++i)
	{
		if(m_GPSDisabledZones[i].m_iGpsBlockedByScriptID == 0)
		{
			return i;
		}
	}
	return -1;
}


void CPathFind::SetGpsBlockingRegionForScript( const u32 iScriptId, const Vector3 & vMin, const Vector3 & vMax, s32 iIndex )
{
	Assertf(iIndex >= 0 && iIndex < MAX_GPS_DISABLED_ZONES, "Invalid gps region index %d [0 - %u]", iIndex, MAX_GPS_DISABLED_ZONES);
	Assertf(!m_GPSDisabledZones[iIndex].m_iGpsBlockedByScriptID || m_GPSDisabledZones[iIndex].m_iGpsBlockedByScriptID==iScriptId, "Another script is already using the GPS blocking region.");

	if(m_GPSDisabledZones[iIndex].m_iGpsBlockedByScriptID && m_GPSDisabledZones[iIndex].m_iGpsBlockedByScriptID != iScriptId)
		return;

	Assertf(vMin.x > WORLDLIMITS_REP_XMIN && vMin.y > WORLDLIMITS_REP_YMIN && vMax.x < WORLDLIMITS_REP_XMAX && vMax.y < WORLDLIMITS_REP_YMAX, "Specified coords are out of this world.");

	m_GPSDisabledZones[iIndex].m_iGpsBlockedByScriptID = iScriptId;
	m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMinX = COORS_TO_INT16(vMin.x);
	m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMinY = COORS_TO_INT16(vMin.y);
	m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMaxX = COORS_TO_INT16(vMax.x);
	m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMaxY = COORS_TO_INT16(vMax.y);

	// Setting a zero-sized zone is the same as turning the blocking area off.  Reset the script ID.
	if(m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMinX==m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMaxX || 
		m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMinY==m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMaxY)
	{
		m_GPSDisabledZones[iIndex].m_iGpsBlockedByScriptID = 0;
#if __ASSERT
		Displayf("SET_GPS_DISABLED_ZONE - disabled by script %s (%d)", scrThread::GetActiveThread()->GetScriptName(), iScriptId);
#endif
	}
#if __ASSERT
	else
	{
		Displayf("SET_GPS_DISABLED_ZONE - enabled by script %s (%d), %d,%d %d,%d", scrThread::GetActiveThread()->GetScriptName(), iScriptId,
			m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMinX, m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMinY, 
			m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMaxX, m_GPSDisabledZones[iIndex].m_iGpsBlockingRegionMaxY);
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SwitchOffNodeAndNeighbours
// PURPOSE :  We switch of the nodes next to this one as long as they have less
//			  than 3 nodes. This way we avoid cars going halfway into roads.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::SwitchOffNodeAndNeighbours(CPathNode *pNode, CPathNode **ppNextNode1, CPathNode **ppNextNode2, bool bWhatToSwitchTo, bool bBackToOriginal)
{
	u32			Link;
	CNodeAddress	NeighbourNode;

	if (bBackToOriginal)
	{
		pNode->m_2.m_switchedOff = pNode->m_2.m_switchedOffOriginal;
	}
	else
	{
		pNode->m_2.m_switchedOff = bWhatToSwitchTo;
	}

	*ppNextNode1 = NULL;
	if (ppNextNode2) *ppNextNode2 = NULL;

	// Go through the neighbours to find out whether we want to switch off neighbouring nodes as well.
	if(CountNeighboursToBeSwitchedOff(pNode) >= 3)
	{
		return;
	}

	for(Link = 0; Link < pNode->NumLinks(); Link++)
	{
		NeighbourNode = GetNodesLinkedNodeAddr(pNode, Link);
		if(!IsRegionLoaded(NeighbourNode))
		{
			continue;
		}

		CPathNode *pNeighbourNode = FindNodePointer(NeighbourNode);
		if(!ThisNodeHasToBeSwitchedOff(pNeighbourNode))
			{
			continue;
		}

					bool bNeedsChanged = false;
					if (bBackToOriginal)
					{
						if (pNeighbourNode->m_2.m_switchedOff != pNeighbourNode->m_2.m_switchedOffOriginal) bNeedsChanged = true;
					}
					else
					{
						if (static_cast<bool>(pNeighbourNode->m_2.m_switchedOff) != bWhatToSwitchTo) bNeedsChanged = true;
					}

					if (bNeedsChanged && CountNeighboursToBeSwitchedOff(pNeighbourNode) < 3)
					{
						// Pass this guy back to be switched off later.
						if (*ppNextNode1 == NULL)
						{
							*ppNextNode1 = pNeighbourNode;
						}
						else
						{
							Assert(ppNextNode2);
							Assert(*ppNextNode2 == NULL);
							*ppNextNode2 = pNeighbourNode;
						}
					}
				}
			}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CountNeighboursToBeSwitchedOff
// PURPOSE :  Counts the number of neighbours this node has that have to be switched
//			  off. This excludes parking nodes.
/////////////////////////////////////////////////////////////////////////////////
s32 CPathFind::CountNeighboursToBeSwitchedOff(const CPathNode *pNode) const
{
	s32	Result = 0;

	for(u32 Link = 0; Link < pNode->NumLinks(); Link++)
	{
		CNodeAddress NeighbourNode = GetNodesLinkedNodeAddr(pNode, Link);
		if(!IsRegionLoaded(NeighbourNode))
		{
			continue;
		}

		const CPathNode *pNeighbourNode = FindNodePointer(NeighbourNode);
		if (ThisNodeHasToBeSwitchedOff(pNeighbourNode))
		{
			Result++;
		}
	}
	return Result;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CountNeighboursToBeSwitchedOff
// PURPOSE :  Counts the number of neighbours this node has that have to be switched
//			  off. This excludes parking nodes.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::ThisNodeHasToBeSwitchedOff(const CPathNode *pNode) const
{
	if (pNode->m_1.m_specialFunction != SPECIAL_PARKING_PARALLEL &&
		pNode->m_1.m_specialFunction != SPECIAL_PARKING_PERPENDICULAR)
	{
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : HasCarStoppedBecauseOfLight
// PURPOSE :  Returns true if the reason this car has stopped is possibly a
//			  red light. In this case we don't want to beeb the horn.
/////////////////////////////////////////////////////////////////////////////////
bool CVehicle::HasCarStoppedBecauseOfLight(bool /*bLookFurther*/) const
{
	return CVehicleJunctionHelper::ApproachingLight(this, false, true, true);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindRegionForCoors
// PURPOSE :  Given a X and Y coordinate this function works out in what Region of the map this node
//			  is located.
/////////////////////////////////////////////////////////////////////////////////
s32 CPathFind::FindRegionForCoors(float X, float Y)
{
	s32	ResultX = (s32)((X - WORLDLIMITS_REP_XMIN) / PATHFINDREGIONSIZEX);
	ResultX = rage::Max(ResultX, 0);
	ResultX = rage::Min(ResultX, PATHFINDMAPSPLIT-1);
	s32	ResultY = (s32)((Y - WORLDLIMITS_REP_YMIN) / PATHFINDREGIONSIZEY);
	ResultY = rage::Max(ResultY, 0);
	ResultY = rage::Min(ResultY, PATHFINDMAPSPLIT-1);

	return ( FIND_REGION_INDEX ( ResultX, ResultY ) );
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindXRegionForCoors
// PURPOSE :  Given a X and Y coordinate this function works out in what Region of the map this node
//			  is located.
/////////////////////////////////////////////////////////////////////////////////
s32 CPathFind::FindXRegionForCoors(float X) const
{
	s32	ResultX = (s32)((X - WORLDLIMITS_REP_XMIN) / PATHFINDREGIONSIZEX);
	ResultX = rage::Max(ResultX, 0);
	ResultX = rage::Min(ResultX, PATHFINDMAPSPLIT-1);
	return ResultX;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindYRegionForCoors
// PURPOSE :  Given a X and Y coordinate this function works out in what Region of the map this node
//			  is located.
/////////////////////////////////////////////////////////////////////////////////
s32 CPathFind::FindYRegionForCoors(float Y) const
{
	s32	ResultY = (s32)((Y - WORLDLIMITS_REP_YMIN) / PATHFINDREGIONSIZEY);
	ResultY = rage::Max(ResultY, 0);
	ResultY = rage::Min(ResultY, PATHFINDMAPSPLIT-1);
	return ResultY;
}


#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindFloatXRegionForCoors
// PURPOSE :  Given a X and Y coordinate this function works out in what Region of the map this node
//			  is located.
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::FindFloatXRegionForCoors(float X) const
{
	float	ResultX = (X - WORLDLIMITS_REP_XMIN) / PATHFINDREGIONSIZEX;
	ResultX = rage::Max(ResultX, 0.0f);
	ResultX = rage::Min(ResultX, (float)PATHFINDMAPSPLIT);
	return ResultX;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindFloatYRegionForCoors
// PURPOSE :  Given a X and Y coordinate this function works out in what Region of the map this node
//			  is located.
/////////////////////////////////////////////////////////////////////////////////
float CPathFind::FindFloatYRegionForCoors(float Y) const
{
	float	ResultY = (Y - WORLDLIMITS_REP_YMIN) / PATHFINDREGIONSIZEY;
	ResultY = rage::Max(ResultY, 0.0f);
	ResultY = rage::Min(ResultY, (float)PATHFINDMAPSPLIT);
	return ResultY;
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindCenterPointOfRegion
// PURPOSE :  Given a X and Y Region this function will return its centerpoint.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindStartPointOfRegion(s32 RegionX, s32 RegionY, float& ResultX, float& ResultY) const
{
	ResultX = WORLDLIMITS_REP_XMIN + RegionX * PATHFINDREGIONSIZEX;
	ResultY = WORLDLIMITS_REP_YMIN + RegionY * PATHFINDREGIONSIZEY;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeCoorsForScript
// PURPOSE :  Given a node returns the coordinate of it. If there is a central
//			  reservation we move the point up a bit.
/////////////////////////////////////////////////////////////////////////////////
Vector3 CPathFind::FindNodeCoorsForScript(CNodeAddress Node, bool *pSuccess) const
{
	if (Node.IsEmpty() || (!IsRegionLoaded(Node)))
	{
		if (pSuccess)
		{
			*pSuccess = false;
		}

		return Vector3(0.0f, 0.0f, 0.0f);
	}
	if (pSuccess)
	{
		*pSuccess = true;
	}

	Assert(!Node.IsEmpty());
	Assert(IsRegionLoaded(Node));

	const CPathNode *pNode = FindNodePointer(Node);
	Vector3 nodeCoors;
	pNode->GetCoors(nodeCoors);

	//	if (pNode->centralReservationWidth == 0)
	{	// No central reservation. Easy!

		return nodeCoors;
	}
	/*
	else
	{
	// Need to find the direction in which to displace the coordinates first.
		if(pNode->NumLinks())
	{
			CNodeAddress linkNodeAddr = ThePaths.GetNodeLinkedNodeAddr(pNode, 0);
			if(!IsRegionLoaded(linkNodeAddr))
	{
				return nodePos;
	}

			CPathNode* pLinkedNode = FindNodePointer(linkNodeAddr);
			Vector2	Dir;
			Dir.x = pLinkedNode->GetCoorsX()- nodePos.x;
			Dir.y = pLinkedNode->GetCoorsY()- nodePos.y;
	Dir.Normalize();
	if (Dir.x < 0.0f)	// Make sure we always go into the same direction (otherwise node pairs would go wrong)
	{
	Dir.x = -Dir.x;
	Dir.y = -Dir.y;
	}

			return(nodePos +(Vector3(-Dir.y, Dir.x, 0.0f)*(LANEWIDTH * 0.5f)));	// 1.0f was 8.0f
	}
	else
	{
			return(nodePos);
	}
	}
	*/
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeCoorsForScript
// PURPOSE :  Given a node returns the coordinate of it. If there is a central
//			  reservation we move the point up a bit.
/////////////////////////////////////////////////////////////////////////////////
Vector3 CPathFind::FindNodeCoorsForScript(CNodeAddress CurrentNode, CNodeAddress NewNode, float *pOrientation, bool *pSuccess) const
{
	if (CurrentNode.IsEmpty() || NewNode.IsEmpty() || (!IsRegionLoaded(CurrentNode)) || (!IsRegionLoaded(NewNode)))
	{
		if (pSuccess)
		{
			*pSuccess = false;
		}

		return Vector3(0.0f, 0.0f, 0.0f);
	}
	if (pSuccess)
	{
		*pSuccess = true;
	}

	const CPathNode *pCurrentNode = FindNodePointer(CurrentNode);
	const CPathNode *pNewNode = FindNodePointer(NewNode);
	Vector3 currentCoors, newCoors;

	pCurrentNode->GetCoors(currentCoors);
	pNewNode->GetCoors(newCoors);

	*pOrientation = (( RtoD * rage::Atan2f(- (newCoors.x - currentCoors.x), newCoors.y - currentCoors.y)));

	//	if (pCurrentNode->centralReservationWidth == 0)
	{	// No central reservation. Easy!
		return currentCoors;
	}
	/*
	else
	{
	// Need to find the direction in which to displace the coordinates first.
	Vector3	Dir;
	Dir.x = newCoors.x - currentCoors.x;
	Dir.y = newCoors.y - currentCoors.y;
	Dir.Normalize();
	return (currentCoors + (Vector3(Dir.y, -Dir.x, 0.0f) * (LANEWIDTH * 0.5f + pCurrentNode->centralReservationWidth / (2.0f * 8.0f)) ) );
	}
	*/
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Shutdown
// PURPOSE :  Shuts down the pathfind stuff so that we can restart the game.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
#if __WIN32PC
	    // Free the path blocks that are loaded in.
	    for (s32 RegionX = 0; RegionX < PATHFINDMAPSPLIT; RegionX++)
	    {
		    for (s32 RegionY = 0; RegionY < PATHFINDMAPSPLIT; RegionY++)
		    {
			    s32	RegionIndex = FIND_REGION_INDEX(RegionX , RegionY);

			    // Make sure it is not loaded in.

			    if (apRegions[RegionIndex] && apRegions[RegionIndex]->aNodes)
			    {
				    Remove(GetStreamingIndexForRegion(RegionIndex));
			    }

		    }
	    }
#endif

	    // clear the streaming variables. We don't want to build the files afresh.
	    s32	Region;
	    for (Region = 0; Region < PATHFINDREGIONS + MAXINTERIORS; Region++)
	    {
		    delete apRegions[Region];
		    //apRegions[Region]->aNodes = NULL;
		    //apRegions[Region]->aLinks = NULL;
	    }
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
        // Remove all paths 
	    for(int RegionIndex=0; RegionIndex<PATHFINDREGIONS; RegionIndex++)
	    {
		    ClearRequiredFlag(RegionIndex, STRFLAG_DONTDELETE|STRFLAG_MISSION_REQUIRED);
		    StreamingRemove(strLocalIndex(RegionIndex));

			int uHeistRegionIndex = RegionIndex + sm_iHeistsOffset;
			ClearRequiredFlag(uHeistRegionIndex, STRFLAG_DONTDELETE|STRFLAG_MISSION_REQUIRED);
		    StreamingRemove(strLocalIndex(uHeistRegionIndex));
	    }
    }
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : For this node works out what the road boundaries are.
// PURPOSE :
// RETURNS :  Road boundary to the left and to the right both in positive numbers and using meters.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::FindRoadBoundaries(s32 lanesTo, s32 lanesFrom, float centralReservationWidth, bool bNarrowRoad, bool bAllLanesThoughCentre, float& roadWidthOnLeft_out, float& roadWidthOnRight_out) const
{
	float laneWidth = LANEWIDTH;
	if(bNarrowRoad)
	{
		laneWidth = LANEWIDTH_NARROW;
	}

	if(bAllLanesThoughCentre)
	{
		roadWidthOnLeft_out = rage::Max(lanesFrom * laneWidth, lanesTo * laneWidth) * 0.5f;
		roadWidthOnRight_out = roadWidthOnLeft_out;
	}
	else if(lanesTo == 0)
	{
		roadWidthOnLeft_out = (lanesFrom * laneWidth) * 0.5f;
		roadWidthOnRight_out = roadWidthOnLeft_out;
	}
	else if(lanesFrom == 0)
	{
		roadWidthOnLeft_out = (lanesTo * laneWidth) * 0.5f;
		roadWidthOnRight_out = roadWidthOnLeft_out;
	}
	else
	{
		roadWidthOnLeft_out = lanesFrom * laneWidth + (centralReservationWidth * 0.5f);
		roadWidthOnRight_out = lanesTo * laneWidth + (centralReservationWidth * 0.5f);
	}
}

//Gets some road boundaries to help with segment tests
//bClampToCenterOfRoad - if true, the vector returned in vLeftStart
//will originate at fromNode's position + centerReservationWidth meters
//to the right.  if false, vLeftStart will be the left extent of the road,
//roughly equal to fromNode's position + centerReservationWidth + all the lane widths
//to the left of fromNode
//RETURNS:  true if boundaries were found, false otherwise
bool CPathFind::GetBoundarySegmentsBetweenNodes(const CNodeAddress fromNode, const CNodeAddress toNode, const CNodeAddress nextNode
	, bool bClampToCenterOfRoad, Vector3& vLeftEnd, Vector3& vRightEnd, Vector3& vLeftStartPos, Vector3& vRightStartPos, const float fVehicleHalfWidthToUse)
{
	//the safe version of these functions includes checking for empty addr's and the region not being loaded
	const CPathNode* pFrom = FindNodePointerSafe(fromNode);
	const CPathNode* pTo = FindNodePointerSafe(toNode);
	if (!pFrom || !pTo)
	{
		return false;
	}

	Vector3 vFromNodeCoords;
	Vector3 vToNodeCoords;
	pTo->GetCoors(vToNodeCoords);
	pFrom->GetCoors(vFromNodeCoords);
	Vector3 vFromToToNode = vToNodeCoords - vFromNodeCoords;

	s16 linkIndex = -1;
	FindNodesLinkIndexBetween2Nodes(pFrom, pTo, linkIndex);
	//aiAssert(linkIndex > -1);
	if (!aiVerify(linkIndex > -1))
	{
		return false;
	}
	const CPathNodeLink* pLink = &GetNodesLink(pFrom, linkIndex);
	
	float fRoadWidthLeft=0.0f, fRoadWidthRight=0.0f;
	FindRoadBoundaries(*pLink,fRoadWidthLeft,fRoadWidthRight);

	Assert(fRoadWidthLeft >= 0.0f);
	Assert(fRoadWidthRight >= 0.0f);

	// Narrow the drive area a bit so that the cars don't get clipped so much.
	fRoadWidthLeft -= fVehicleHalfWidthToUse;
	fRoadWidthLeft = rage::Max(0.0f, fRoadWidthLeft);
	fRoadWidthRight -= fVehicleHalfWidthToUse;
	fRoadWidthRight = rage::Max(0.0f, fRoadWidthRight);

	//give a boost to ignorenav link size
	static dev_float s_fExtraWidthForIgnoreNavLinks = LANEWIDTH;
	if (pLink->m_1.m_bDontUseForNavigation)
	{
		fRoadWidthLeft += s_fExtraWidthForIgnoreNavLinks;
		fRoadWidthRight += s_fExtraWidthForIgnoreNavLinks;
	}

	Vector3 vRight;
	vRight = CrossProduct(vFromToToNode, ZAXIS);
	vRight.NormalizeSafe();

	vRightStartPos = vFromNodeCoords + vRight*fRoadWidthRight;

	//only clamp our search extents to this side of the road if we have lanes coming in the other direction
	if (bClampToCenterOfRoad && pLink->m_1.m_LanesFromOtherNode > 0)
	{
		vLeftStartPos = vFromNodeCoords + vRight*static_cast<float>(pLink->m_1.m_Width * 0.5f);
	}
	else
	{
		vLeftStartPos = vFromNodeCoords + -vRight*fRoadWidthLeft;
	}

	const CPathNode* pNext = FindNodePointerSafe(nextNode);
	if (pNext)
	{
		//if the user passed us a third node, use its start position as our end position
		Vector3 vNextNodeCoords;
		pNext->GetCoors(vNextNodeCoords);
		Vector3 vToToNext = vNextNodeCoords - vToNodeCoords;

		Vector3 vNextRight;
		vNextRight = CrossProduct(vToToNext, ZAXIS);
		vNextRight.NormalizeSafe();

		s16 nextLinkIndex = -1;
		FindNodesLinkIndexBetween2Nodes(pTo, pNext, nextLinkIndex);
		float fNextRoadWidthLeft = fRoadWidthLeft;
		float fNextRoadWidthRight = fRoadWidthRight;
		if (nextLinkIndex > -1)
		{
			const CPathNodeLink* pNextLink = &GetNodesLink(pTo, nextLinkIndex);
			if (pNextLink)
			{
				FindRoadBoundaries(*pNextLink, fNextRoadWidthLeft, fNextRoadWidthRight);
			}
		}
		
		fwGeom::fwLine rightLine(vRightStartPos, vRightStartPos + vFromToToNode*3.0f);
		rightLine.FindClosestPointOnLine(vToNodeCoords + vNextRight*fNextRoadWidthRight, vRightEnd);

		if (bClampToCenterOfRoad && pLink->m_1.m_LanesFromOtherNode > 0)
		{
			//vLeftDelta = (vToNodeCoords + vNextRight*static_cast<float>(pLink->m_1.m_Width * 0.5f)) - vLeftStartPos;
			vLeftEnd =  (vToNodeCoords + vNextRight*static_cast<float>(pLink->m_1.m_Width * 0.5f));
		}
		else
		{
			fwGeom::fwLine leftLine(vLeftStartPos, vLeftStartPos + vFromToToNode*3.0f);
			leftLine.FindClosestPointOnLine(vToNodeCoords + -vNextRight*fNextRoadWidthLeft, vLeftEnd);
		}
			
	}
	else
	{
		//otherwise just use our delta vector
		vLeftEnd = vLeftStartPos + vFromToToNode;
		vRightEnd = vRightStartPos + vFromToToNode;
	}

	return true;
}

bool CPathFind::HelperIsPointWithinArbitraryArea(float TestPointX, float TestPointY,
									  float Point1X, float Point1Y,
									  float Point2X, float Point2Y,
									  float Point3X, float Point3Y,
									  float Point4X, float Point4Y)
{
	if ((TestPointX - Point1X) * (Point3Y - Point1Y) - (TestPointY - Point1Y) * (Point3X - Point1X) < 0.0f) return false;
	if ((TestPointX - Point3X) * (Point4Y - Point3Y) - (TestPointY - Point3Y) * (Point4X - Point3X) < 0.0f) return false;
	if ((TestPointX - Point4X) * (Point2Y - Point4Y) - (TestPointY - Point4Y) * (Point2X - Point4X) < 0.0f) return false;
	if ((TestPointX - Point2X) * (Point1Y - Point2Y) - (TestPointY - Point2Y) * (Point1X - Point2X) < 0.0f) return false;

	return true;
}	

bool CPathFind::DoesVehicleThinkItsOffRoad(const CVehicle* pVehicle, const bool bReverse, const CVehicleNodeList* pNodeList /*= NULL*/)
{
	const int iSearchEndNode = pNodeList ? pNodeList->GetTargetNodeIndex() : CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1;
	const float fVehicleHalfWidthNegated = -pVehicle->GetBoundingBoxMax().x;
	return IsPositionOffRoadGivenNodelist(pVehicle, CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, bReverse), pNodeList, fVehicleHalfWidthNegated, 0, iSearchEndNode);
}

bool CPathFind::IsPositionOffRoadGivenNodelist(const CVehicle* pVehicle, Vec3V_In vTestPosition, const CVehicleNodeList* pNodeList /*= NULL*/
	, const float fHalfWidth /*=0.0f*/, const int iSearchStartNode, int iSearchEndNode)
{
	//assume we're on road unless we have nodes that say otherwise
	if (!pVehicle)
	{
		return false;
	}

	if (!pNodeList)
	{
		pNodeList = pVehicle->GetIntelligence()->GetNodeList();
	}

	if (!pNodeList)
	{
		//we should really do something here--worst case scenario would be a join to road system
		return false;
	}

	if (iSearchEndNode < 0)
	{
		iSearchEndNode = CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1;
	}

	Vector3 vTestPos = VEC3V_TO_VECTOR3(vTestPosition);
	int iClosestNode = -1;
	Vector3 vClosestPos(Vector3::ZeroType);
	float fPathSegmentPortion = 0.0f;
	pNodeList->GetClosestPosOnPath(vTestPos, vClosestPos, iClosestNode, fPathSegmentPortion, iSearchStartNode, iSearchEndNode);

	if (iClosestNode < 0)
	{
		return false;
	}

	//we subtract one later, so this needs to be at least the 2nd point on the path.
	iClosestNode = rage::Max(iClosestNode, 1);

	//const Vec3V vTestPos = pVehicle->GetVehiclePosition();

	//test the 2 or 3 closest points. if we're inside any of them, return false, we're ok
	const int iLastGoodNodeInList = pNodeList->FindLastGoodNode();
	for (int i = rage::Max(iClosestNode-1, 1); i < iClosestNode+2; i++)
	{
		CNodeAddress NextNode = i < iLastGoodNodeInList ? pNodeList->GetPathNodeAddr(i+1)
			: EmptyNodeAddress;

		Vector3 vLeftStart, vRightStart, vLeftEnd, vRightEnd;
		if (!GetBoundarySegmentsBetweenNodes(pNodeList->GetPathNodeAddr(i-1), pNodeList->GetPathNodeAddr(i), NextNode, false, vLeftEnd, vRightEnd, vLeftStart, vRightStart, fHalfWidth))
		{
			return false;
		}

		if (HelperIsPointWithinArbitraryArea(vTestPos.x, vTestPos.y, vLeftStart.x, vLeftStart.y, vRightStart.x, vRightStart.y, vLeftEnd.x, vLeftEnd.y, vRightEnd.x, vRightEnd.y))
		{
			//CVehicle::ms_debugDraw.AddSphere(vTestPosition, 6.0f, Color_green);
// 			if (pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer())
// 			{
// 				//grcDebugDraw::Quad(VECTOR3_TO_VEC3V(vLeftStart), VECTOR3_TO_VEC3V(vLeftEnd), VECTOR3_TO_VEC3V(vRightStart), VECTOR3_TO_VEC3V(vRightEnd), Color_green, true, false, 60);
// 			}
			return false;
		}

// 		if (pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer())
// 		{
// 			grcDebugDraw::Quad(VECTOR3_TO_VEC3V(vLeftStart), VECTOR3_TO_VEC3V(vLeftEnd), VECTOR3_TO_VEC3V(vRightStart), VECTOR3_TO_VEC3V(vRightEnd), Color_red, true, false, 60);
// 		}
	}

	//CVehicle::ms_debugDraw.AddSphere(vTestPosition, 6.0f, Color_red);
	//CVehicle::ms_debugDraw.AddLine(RCC_VEC3V(vLeftStart), RCC_VEC3V(vLeftEnd), Color_OrangeRed);
	//CVehicle::ms_debugDraw.AddLine(RCC_VEC3V(vRightStart), RCC_VEC3V(vRightEnd), Color_OrangeRed);

	//we made it, definitely outside road bounds
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IdentifyDrivableExtremes
// PURPOSE :	Given a set of 3 nodes (that have to be adjacent) this function
//				calculates the 2 points that define the maximum drivable dist
//				from the centre of the road.
//				This function takes into account the number of lanes on the
//				links.
// RETURN :		Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::IdentifyDrivableExtremes(CNodeAddress nodeAAddr, CNodeAddress nodeBAddr, CNodeAddress nodeCAddr, bool UNUSED_PARAM(nodeBIsJunction), float vehHalfWidthToUse, Vector3 &drivableExtremePointLeft_out, Vector3 &drivableExtremePointRight_out, float& widthScalingFactor_out) const
{
	widthScalingFactor_out = 1.0f;

	TUNE_GROUP_BOOL(RACING_AI, RACING_properlyHandleSingleTrackRoads, true);
	Assert(!nodeAAddr.IsEmpty() && IsRegionLoaded(nodeAAddr));
	Assert(!nodeBAddr.IsEmpty() && IsRegionLoaded(nodeBAddr));

	const CPathNode *pNodeA = FindNodePointer(nodeAAddr);
	const CPathNode *pNodeB = FindNodePointer(nodeBAddr);
	Vector3 nodeAPos;
	Vector3 nodeBPos;
	pNodeA->GetCoors(nodeAPos);
	pNodeB->GetCoors(nodeBPos);

	s16 iLink = -1;
	FindNodesLinkIndexBetween2Nodes(pNodeA->m_address, nodeBAddr, iLink);
	Assert(iLink != -1);

	const CPathNodeLink *pLinkAB = &GetNodesLink(pNodeA, iLink);
	Assert(pLinkAB);

	//Assert(linkIndexForNodeAToNodeB);
	bool bNarrowRoad = pLinkAB->m_1.m_NarrowRoad;
	
	//make sure the last node is valid
	bool bIsLastNodeValid = true;
	s16 iLinkBetweenLastNodesIndex = -1;
	if(!nodeCAddr.IsEmpty() && IsRegionLoaded(nodeCAddr))
	{
		// Identify the link between the last 2 nodes.
		const bool bLinkBCFound = FindNodesLinkIndexBetween2Nodes(pNodeB->m_address, nodeCAddr, iLinkBetweenLastNodesIndex);
		if(!bLinkBCFound)
		{
			bIsLastNodeValid = false; //can't find the link
		}
	}
	else
		bIsLastNodeValid = false;//cant find the node


	if(bIsLastNodeValid)
	{
		const CPathNodeLink *pLinkBC = &GetNodesLink(pNodeB, iLinkBetweenLastNodesIndex);
		Assert(pLinkBC);

		// Get the direction through the middle node.
		Vector2	dirThroughNode(0.0f, 0.0f);
// DEBUG!! -AC, Make this smarter...
		//if(!nodeBIsJunction || nodeBIsJunction)
		{
			const CPathNode *pNodeC = FindNodePointer(nodeCAddr);
			Assert(pNodeC);
			Vector3 nodeCPos;
			pNodeC->GetCoors(nodeCPos);

			Vector2 nodeAToBDir = Vector2(nodeBPos.x - nodeAPos.x, nodeBPos.y - nodeAPos.y);
			nodeAToBDir.Normalize();
			Vector2 nodeBToCDir = Vector2(nodeCPos.x - nodeBPos.x, nodeCPos.y - nodeBPos.y);
			nodeBToCDir.Normalize();

			dirThroughNode =(nodeAToBDir + nodeBToCDir);
			TUNE_GROUP_BOOL(RACING_AI, useImprovedWidths, false);
			if(useImprovedWidths)
			{
				float dot = nodeAToBDir.Dot(nodeBToCDir);
				if(dot > -1.0f)
				{
					widthScalingFactor_out = dirThroughNode.Mag()/(1.0f + dot);
				}
			}

			dirThroughNode.Normalize();
		}
// END DEBUG!!

		// Get the road widths.
		float drivableWidthOnLeft = 0.0f;
		float drivableWidthOnRight = 0.0f;
		{
			s32	lanesTo = rage::Min(pLinkAB->m_1.m_LanesToOtherNode, pLinkBC->m_1.m_LanesToOtherNode);
			s32	lanesFrom = rage::Min(pLinkAB->m_1.m_LanesFromOtherNode, pLinkBC->m_1.m_LanesFromOtherNode);
			float	centralReservationWidth = rage::Min(static_cast<float>(pLinkAB->m_1.m_Width), static_cast<float>(pLinkBC->m_1.m_Width));
#if __DEV
			const bool bAllLanesThoughCentre = bMakeAllRoadsSingleTrack ||(RACING_properlyHandleSingleTrackRoads &&(pLinkAB->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL));
#else
			const bool bAllLanesThoughCentre = (RACING_properlyHandleSingleTrackRoads &&(pLinkAB->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL));
#endif // __DEV

			FindRoadBoundaries(lanesTo, lanesFrom, bAllLanesThoughCentre?0.0f:centralReservationWidth, bNarrowRoad, bAllLanesThoughCentre, drivableWidthOnLeft, drivableWidthOnRight);
		}

		// Narrow the drive area a bit so that the cars don't get clipped so much.
		drivableWidthOnLeft -= vehHalfWidthToUse;
		drivableWidthOnLeft = rage::Max(0.0f, drivableWidthOnLeft);
		drivableWidthOnRight -= vehHalfWidthToUse;
		drivableWidthOnRight = rage::Max(0.0f, drivableWidthOnRight);

		// Calculate the two points.
		drivableExtremePointLeft_out = nodeBPos -(Vector3(dirThroughNode.y, -dirThroughNode.x, 0.0f)* drivableWidthOnLeft * widthScalingFactor_out);
		drivableExtremePointRight_out = nodeBPos +(Vector3(dirThroughNode.y, -dirThroughNode.x, 0.0f)* drivableWidthOnRight * widthScalingFactor_out);
	}
	else
	{
		// If the 3rd node isn't present we still have a stab at it.

		// Get the direction through the middle node.
		Vector2 dirThroughNode(nodeBPos.x - nodeAPos.x, nodeBPos.y - nodeAPos.y);
		dirThroughNode.Normalize();

		// Get the road widths.
		float drivableWidthOnLeft = 0.0f;
		float drivableWidthOnRight = 0.0f;
		{
			s32	lanesTo = pLinkAB->m_1.m_LanesToOtherNode;
			s32	lanesFrom = pLinkAB->m_1.m_LanesFromOtherNode;
			float	centralReservationWidth = static_cast<float>(pLinkAB->m_1.m_Width);
#if __DEV
			const bool bAllLanesThoughCentre = bMakeAllRoadsSingleTrack ||(RACING_properlyHandleSingleTrackRoads &&(pLinkAB->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL));
#else
			const bool bAllLanesThoughCentre = (RACING_properlyHandleSingleTrackRoads &&(pLinkAB->m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL));
#endif // __DEV
			FindRoadBoundaries(lanesTo, lanesFrom, bAllLanesThoughCentre?0.0f:centralReservationWidth, bNarrowRoad, bAllLanesThoughCentre, drivableWidthOnLeft, drivableWidthOnRight);
		}

		// Narrow the drive area a bit so that the cars don't get clipped so much.
		drivableWidthOnLeft -= vehHalfWidthToUse;
		drivableWidthOnLeft = rage::Max(0.0f, drivableWidthOnLeft);
		drivableWidthOnRight -= vehHalfWidthToUse;
		drivableWidthOnRight = rage::Max(0.0f, drivableWidthOnRight);

		// Calculate the two points.
		drivableExtremePointLeft_out = nodeBPos - Vector3(dirThroughNode.y, -dirThroughNode.x, 0.0f)* drivableWidthOnLeft;
		drivableExtremePointRight_out = nodeBPos + Vector3(dirThroughNode.y, -dirThroughNode.x, 0.0f)* drivableWidthOnRight;
	}
}


u8	ToBeStreamed[PATHFINDMAPSPLIT][PATHFINDMAPSPLIT];
u8	ToBeStreamedForScript[PATHFINDMAPSPLIT][PATHFINDMAPSPLIT];

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : MarkRegionsForCoors
// PURPOSE :  Works out what regions have to be loaded for these coordinates.
/////////////////////////////////////////////////////////////////////////////////

//CompileTimeAssert(PATHFINDREGIONSIZEX==PATHFINDREGIONSIZEY);	// regions must be square

bool CPathFind::WasRegionSkippedByPrologueNodeRejection(const s32 RegionX, const s32 RegionY) const
{
	if (bStreamHeistIslandNodes)
	{
		return false;
	}

	return !bWillTryAndStreamPrologueNodes && IsRegionInPrologueMap(RegionX, RegionY);
}

bool CPathFind::IsRegionInPrologueMap(const s32 RegionX, const s32 RegionY) const
{
	static const float fRegionHalfSize = PATHFINDREGIONSIZEX * 0.5f;
	
	float fRegionMidX;
	float fRegionMidY;

	FindStartPointOfRegion(RegionX, RegionY, fRegionMidX, fRegionMidY);
	
	fRegionMidX += fRegionHalfSize;
	fRegionMidY += fRegionHalfSize;

	const Vector3 vRegionMid(fRegionMidX, fRegionMidY, 0.0f);

	return IsNodePositionInPrologueMap(vRegionMid);
}

bool CPathFind::IsNodePositionInPrologueMap(const Vector3& vPos)
{
	return vPos.y < -3600.0f;
}

void CPathFind::MarkRegionsForCoors(const Vector3& Coors, float Range, const bool bPossiblyRejectIfPrologueNodes)
{
#if __BANK
	if(bDisplayRegionBoxes)
	{
		grcDebugDraw::Sphere(Coors, Range, Color_green, false);
	}
#endif

	static const float fRegionHalfSize = PATHFINDREGIONSIZEX*0.5f;
	static const float fRegionRadius = rage::Sqrtf((fRegionHalfSize*fRegionHalfSize)+(fRegionHalfSize*fRegionHalfSize));
	
	const float fMarkRangeSqr = square(Range + fRegionRadius);

	Assert(Range < 10000.0f);

	s32 MinRegionX = rage::Max(0, FindXRegionForCoors(Coors.x - Range));
	s32 MaxRegionX = rage::Min(PATHFINDMAPSPLIT-1, FindXRegionForCoors(Coors.x + Range));
	s32 MinRegionY = rage::Max(0, FindXRegionForCoors(Coors.y - Range));
	s32 MaxRegionY = rage::Min(PATHFINDMAPSPLIT-1, FindYRegionForCoors(Coors.y + Range));

	float fRegionMidX, fRegionMidY;
	for (s32 RegionX = MinRegionX; RegionX <= MaxRegionX; RegionX++)
	{
		for (s32 RegionY = MinRegionY; RegionY <= MaxRegionY; RegionY++)
		{
			FindStartPointOfRegion(RegionX, RegionY, fRegionMidX, fRegionMidY);
			fRegionMidX += fRegionHalfSize;
			fRegionMidY += fRegionHalfSize;

			//if we don't want to load prologue nodes, possibly skip the region here
			const Vector3 vRegionMid(fRegionMidX, fRegionMidY, 0.0f);
			if (bPossiblyRejectIfPrologueNodes
				&& !bStreamHeistIslandNodes
				&& !bWillTryAndStreamPrologueNodes 
				&& IsNodePositionInPrologueMap(vRegionMid))
			{
				continue;
			}

			const float fDistSqr = square(fRegionMidX - Coors.x) + square(fRegionMidY - Coors.y);
			if(fDistSqr < fMarkRangeSqr)
			{
				ToBeStreamed[RegionX][RegionY] = 1;
			}
		}
	}	
}


/*
static bool gNeedExtraPaths = false;
static Vector3 gPathPosn;
//
// name:		SetPathsNeededAtPosition
// description:	Set that paths are needed at this frame

void CPathFind::SetPathsNeededAtPosition(const Vector3& posn)
{
gPathPosn = posn;
gNeedExtraPaths = true;
}
*/

bool CPathFind::IsVehicleTypeWhichCanStreamInNodes(const CVehicle* pVehicle)
{
	if (pVehicle->GetVehicleType() != VEHICLE_TYPE_HELI &&
		pVehicle->GetVehicleType() != VEHICLE_TYPE_PLANE &&
		pVehicle->GetVehicleType() != VEHICLE_TYPE_BOAT &&
		pVehicle->GetVehicleType() != VEHICLE_TYPE_TRAIN &&
		pVehicle->GetVehicleType() != VEHICLE_TYPE_SUBMARINE &&
		pVehicle->GetVehicleType() != VEHICLE_TYPE_BLIMP)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CPathFind::IsMissionVehicleThatCanStreamInNodes(const CVehicle* pVehicle)
{
	if (pVehicle->PopTypeIsMission())
	{
		return true;
	}
	else
	{
		return false;
	}
}

void CPathFind::CountScriptMemoryUsage(u32& nVirtualSize, u32& nPhysicalSize)
{
	nVirtualSize = 0;
	nPhysicalSize = 0;

	s32	RegionX, RegionY;
	atArray<strIndex> streamingIndices;

	// Clear the area of regions to stream in.
	for (RegionX = 0; RegionX < PATHFINDMAPSPLIT; RegionX++)
	{
		for (RegionY = 0; RegionY < PATHFINDMAPSPLIT; RegionY++)
		{
			u32 IRegion = FIND_REGION_INDEX(RegionX , RegionY);
			
			strLocalIndex	Region = GetStreamingIndexForRegion(IRegion);

			int iFlags = GetStreamingFlags(Region);

			if ((iFlags & STRFLAG_MISSION_REQUIRED) != 0)
			{
				streamingIndices.Grow() = GetStreamingIndex(Region);
			}
		}
	}

	strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndices, nVirtualSize, nPhysicalSize, NULL, 0, true);
}

#if __BANK
void CPathFind::DebugDrawNodesRequiredRegions() const
{
	for(s32 s=0; s<CPathNodeRequiredArea::m_iNumActiveAreas; s++)
	{
		const CPathNodeRequiredArea & reqArea = m_PathNodeRequiredAreas[s];
		Color32 col = Color_black;
		switch(reqArea.m_iContext)
		{
			case CPathNodeRequiredArea::CONTEXT_MAP:
				col = Color_yellow;
				break;
			case CPathNodeRequiredArea::CONTEXT_SCRIPT:
				col = Color_cyan;
				break;
			case CPathNodeRequiredArea::CONTEXT_GAME:
				col = Color_green;
				break;
		}

		grcDebugDraw::BoxAxisAligned(VECTOR3_TO_VEC3V(reqArea.m_vMin), VECTOR3_TO_VEC3V(reqArea.m_vMax), col, false);
		if(reqArea.m_Description && *reqArea.m_Description)
			grcDebugDraw::Text( (reqArea.m_vMin+reqArea.m_vMax)/2.0f, col, 0, 0, reqArea.m_Description);

		if(CVectorMap::m_bAllowDisplayOfMap)
		{
			CVectorMap::Draw3DBoundingBoxOnVMap(spdAABB(VECTOR3_TO_VEC3V(reqArea.m_vMin), VECTOR3_TO_VEC3V(reqArea.m_vMax)), col);
			CVectorMap::DrawString((reqArea.m_vMin+reqArea.m_vMax)/2.0f, reqArea.m_Description, col, false);
		}
	}
}
#endif

bool CPathFind::AddNodesRequiredRegionThisFrame(const s32 iContext, const Vector3 & vMin, const Vector3 & vMax, const char * BANK_ONLY(pDescription))
{
	if(CPathNodeRequiredArea::m_iNumActiveAreas >= CPathNodeRequiredArea::TOTAL_SLOTS)
	{
		Assertf(false, "No more regions available.");
		return false;
	}

	switch(iContext)
	{
		case CPathNodeRequiredArea::CONTEXT_MAP:
		{
			if(CPathNodeRequiredArea::m_iNumMapAreas >= CPathNodeRequiredArea::MAX_MAP_SLOTS)
			{
				Assertf(false, "No more regions available for CONTEXT_MAP");
				return false;
			}
			CPathNodeRequiredArea::m_iNumMapAreas++;
			break;
		}
		case CPathNodeRequiredArea::CONTEXT_SCRIPT:
		{
			if(CPathNodeRequiredArea::m_iNumScriptAreas >= CPathNodeRequiredArea::MAX_SCRIPT_SLOTS)
			{
				Assertf(false, "No more regions available for CONTEXT_SCRIPT");
				return false;
			}
			CPathNodeRequiredArea::m_iNumScriptAreas++;
			break;
		}
		case CPathNodeRequiredArea::CONTEXT_GAME:
		{
			if(CPathNodeRequiredArea::m_iNumGameAreas >= CPathNodeRequiredArea::MAX_GAME_SLOTS)
			{
				Assertf(false, "No more regions available for CONTEXT_GAME");
				return false;
			}
			CPathNodeRequiredArea::m_iNumGameAreas++;
			break;
		}
		default:
		{
			Assertf(false, "iContext unknown");
			return false;
		}
	}

	m_PathNodeRequiredAreas[CPathNodeRequiredArea::m_iNumActiveAreas].m_iContext = iContext;
	m_PathNodeRequiredAreas[CPathNodeRequiredArea::m_iNumActiveAreas].m_vMin = vMin;
	m_PathNodeRequiredAreas[CPathNodeRequiredArea::m_iNumActiveAreas].m_vMax = vMax;

#if __BANK
	strncpy( m_PathNodeRequiredAreas[CPathNodeRequiredArea::m_iNumActiveAreas].m_Description, pDescription, 32);
#endif

	CPathNodeRequiredArea::m_iNumActiveAreas++;

	return true;
}

bool CPathFind::ClearNodesRequiredRegions()
{
	CPathNodeRequiredArea::m_iNumMapAreas = 0;

	if(!fwTimer::IsScriptPaused())
		CPathNodeRequiredArea::m_iNumScriptAreas = 0;

	if(!fwTimer::IsGamePaused())
		CPathNodeRequiredArea::m_iNumGameAreas = 0;

	CPathNodeRequiredArea::m_iNumActiveAreas =
		CPathNodeRequiredArea::m_iNumMapAreas + 
		CPathNodeRequiredArea::m_iNumScriptAreas + 
		CPathNodeRequiredArea::m_iNumGameAreas;

	return true;
}

const char* HelperGetStringForContext(s32 context)
{
	switch (context)
	{
	case CPathFind::CPathNodeRequiredArea::CONTEXT_SCRIPT:
		return "Script";
		break;
	case CPathFind::CPathNodeRequiredArea::CONTEXT_GAME:
		return "Game";
		break;
	case CPathFind::CPathNodeRequiredArea::CONTEXT_MAP:
		return "Map";
		break;
	}

	return "";
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateStreaming
// PURPOSE :  Works out what regions have to be loaded and puts requests in for them.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::UpdateStreaming(bool bForceLoad, bool bBlockingLoad)
{
#if __BANK
	if (bDisplayRequiredNodeRegions)
	{
		grcDebugDraw::AddDebugOutput("LoadAllPathNodes flag set: %s", (bLoadAllRegions ? "true" : "false"));
		grcDebugDraw::AddDebugOutput("Active requests (reset): ");
		for(s32 i=0; i<CPathNodeRequiredArea::m_iNumActiveAreas; i++)
		{
			const CPathNodeRequiredArea & reqArea = m_PathNodeRequiredAreas[i];
			grcDebugDraw::AddDebugOutput("Context: %s, Area: %.2f, Description: %s"
				, HelperGetStringForContext(reqArea.m_iContext), (reqArea.m_vMax.x - reqArea.m_vMin.x) * (reqArea.m_vMax.y - reqArea.m_vMin.y), reqArea.m_Description);
		}
		grcDebugDraw::AddDebugOutput("Active Requests (toggle): ");
		for (s32 i=0; i < MAX_NUM_NODE_REQUESTS; i++)
		{
			if (bActiveRequestForRegions[i])
			{
				grcDebugDraw::AddDebugOutput("Source: %s, Area: %.2f"
					, i < GPS_NUM_SLOTS ? "GPS" : "Script", (RequestMaxX[i] -RequestMinX[i]) * (RequestMaxY[i] - RequestMinY[i]));
			}
		}
		grcDebugDraw::AddDebugOutput("Implicit requests (will flash on every half second): ");
	}
#endif //__BANK

	if (bWasStreamingHeistIslandNodes != bStreamHeistIslandNodes)
	{
		//Swapping between heist island and main map
		bWasStreamingHeistIslandNodes = bStreamHeistIslandNodes;

		//Clear out all current pointers to data and clear all requests
		for(int RegionIndex=0; RegionIndex<PATHFINDREGIONS; RegionIndex++)
		{
			//This will allow the streaming engine to clear up the data for the map we've just left
			ClearRequiredFlag(RegionIndex, STRFLAG_DONTDELETE|STRFLAG_MISSION_REQUIRED);
			ClearRequiredFlag(RegionIndex + sm_iHeistsOffset, STRFLAG_DONTDELETE|STRFLAG_MISSION_REQUIRED);

			//StreamingRemove calls Remove() which unloads the data, deletes the pointer and sets apRegions[x] to NULL ready to be
			//populated again below where we do a force update toload the current map data
			StreamingRemove(strLocalIndex(RegionIndex));
			StreamingRemove(strLocalIndex(RegionIndex + sm_iHeistsOffset));

			//Question! I'm not sure if this is needed or not. in Remove() we call Unload before deleting the pointer (do we need to call delete as well)?
			//and setting it to NULL. I'm not sure if we need to call unload and to delete the pointer ourself, or if just setting to NULL is ok here
			//Or do we need to call some verison of remove/streaming remove like we do in the Shutdown function...
			//if (apRegions[RegionIndex])
			//{
			//	apRegions[RegionIndex]->Unload();
			//}

			//Clear our internal pointer to the streamed data so we don't try and reference it any more
			//This is needed as the Load() function deletes the pointer (to data that the streaming system owns - not us) if it exists.
			//If we don't clear the pointer here then it won't be removed when we transition and will point to either data that's been unstramed
			//or will be deleted in Load if we try and load the same region on new map
			//apRegions[RegionIndex] = NULL;
		}

		//Force a load so we start requesting the data for the new map straight away
		bForceLoad = true;
	}

	// Update streaming twice a second only. Should be enough.
	if (bForceLoad || 
		//		gNeedExtraPaths || 
		( (fwTimer::GetSystemTimeInMilliseconds() & ~511) != (fwTimer::GetSystemPrevElapsedTimeInMilliseconds() & ~511) ) )
	{
		STRVIS_AUTO_CONTEXT(strStreamingVisualize::PATHFIND);

		s32	RegionX, RegionY;
		// Clear the area of regions to stream in.
		for (RegionX = 0; RegionX < PATHFINDMAPSPLIT; RegionX++)
		{
			for (RegionY = 0; RegionY < PATHFINDMAPSPLIT; RegionY++)
			{
				ToBeStreamed[RegionX][RegionY] = 0;
				ToBeStreamedForScript[RegionX][RegionY] = 0;
			}
		}

		s32	NumRegionsToBeLoaded = 0;

		// Go through all the entities that need nodes.
		if (bLoadAllRegions)
		{
			for (RegionX = 0; RegionX < PATHFINDMAPSPLIT; RegionX++)
			{
				for (RegionY = 0; RegionY < PATHFINDMAPSPLIT; RegionY++)
				{
					ToBeStreamed[RegionX][RegionY] = 1;
					ToBeStreamedForScript[RegionX][RegionY] = 1;
				}
			}
		}
		else
		{
			//if there's a player switch active, also load nodes around it
			if (m_vPlayerSwitchTarget.IsNonZero())
			{
				MarkRegionsForCoors(m_vPlayerSwitchTarget, STREAMING_PATH_NODES_DIST_PLAYER, true);

#if __BANK
				if (bDisplayRequiredNodeRegions)
				{
					grcDebugDraw::AddDebugOutput("Player switch active. Area: %.2f", (STREAMING_PATH_NODES_DIST_PLAYER * STREAMING_PATH_NODES_DIST_PLAYER) * 4);
				}
#endif //__BANK
			}
			else
			{
				MarkRegionsForCoors(CFocusEntityMgr::GetMgr().GetPos(), STREAMING_PATH_NODES_DIST_PLAYER, true);

#if __BANK
				if (bDisplayRequiredNodeRegions)
				{
					grcDebugDraw::AddDebugOutput("Nodes around player. Area: %.2f", (STREAMING_PATH_NODES_DIST_PLAYER * STREAMING_PATH_NODES_DIST_PLAYER) * 4);
				}
#endif //__BANK
			}
			
			// If we got a periodic search active make sure we keep the nodes around it
			if (m_GPSAsyncSearches[GPS_ASYNC_SEARCH_PERIODIC])
			{
				MarkRegionsForCoors(m_GPSAsyncSearches[GPS_ASYNC_SEARCH_PERIODIC]->m_SearchCoors, STREAMING_PATH_NODES_DIST_PLAYER, true);
			}

			// Make sure we don't throw away lingering async search nodes as we are iterating them
			if (m_GPSAsyncSearches[GPS_ASYNC_SEARCH_START_NODE])
			{
				MarkRegionsForCoors(m_GPSAsyncSearches[GPS_ASYNC_SEARCH_START_NODE]->m_SearchCoors, STREAMING_PATH_NODES_DIST_ASYNC, true);
			}

			if (m_GPSAsyncSearches[GPS_ASYNC_SEARCH_END_NODE])
			{
				MarkRegionsForCoors(m_GPSAsyncSearches[GPS_ASYNC_SEARCH_END_NODE]->m_SearchCoors, STREAMING_PATH_NODES_DIST_ASYNC, true);
			}

			/*
			if (gNeedExtraPaths)
			{
			MarkRegionsForCoors(gPathPosn, STREAMING_PATH_NODES_DIST);
			gNeedExtraPaths = false;
			}
			*/

			CVehicle::Pool *VehPool = CVehicle::GetPool();
			CVehicle* pVehicle;
			s32 i = (s32) VehPool->GetSize();
			while(i--)
			{
				pVehicle = VehPool->GetSlot(i);
				if(pVehicle)
				{
					if (IsVehicleTypeWhichCanStreamInNodes(pVehicle))
					{	
						if (IsMissionVehicleThatCanStreamInNodes(pVehicle) && 
							(pVehicle->GetIntelligence()->GetActiveTask() || pVehicle->GetDriver()))
						{
							const float fStreamingRadius = pVehicle->GetIntelligence()->GetCustomPathNodeStreamingRadius() > 0.0f 
								? rage::Max(pVehicle->GetIntelligence()->GetCustomPathNodeStreamingRadius(), STREAMING_PATH_NODES_DIST)
								: STREAMING_PATH_NODES_DIST;
							MarkRegionsForCoors(VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()), fStreamingRadius, true);

#if __BANK
							if (bDisplayRequiredNodeRegions)
							{
								grcDebugDraw::AddDebugOutput("Mission Vehicle %p. Area: %.2f", pVehicle, (fStreamingRadius * fStreamingRadius) * 4);
							}
#endif //__BANK
						}
						else if (pVehicle->IsLawEnforcementCar()
							&& CGameWorld::FindLocalPlayerWanted()->m_WantedLevel != WANTED_CLEAN)
						{
							MarkRegionsForCoors(VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()), STREAMING_PATH_NODES_DIST, true);

#if __BANK
							if (bDisplayRequiredNodeRegions)
							{
								grcDebugDraw::AddDebugOutput("Cop Vehicle %p. Area: %.2f", pVehicle, (STREAMING_PATH_NODES_DIST * STREAMING_PATH_NODES_DIST) * 4);
							}
#endif //__BANK
						}
					}	
				}
			}

			//for (int i = NODE_REQUEST_FIRST_GPS; i < GPS_NUM_SLOTS; ++i)
			//{
			//	const CGpsSlot& GpsSlot = CGps::GetSlot(i);
			//	if (GpsSlot.m_Status != CGpsSlot::STATUS_OFF &&
			//		((i != GPS_SLOT_DISCRETE && CGps::ShouldGpsBeVisible(i, false)) || i == GPS_SLOT_DISCRETE))
			//	{
			//		for (int j = 0; j < GpsSlot.m_NumNodes; ++j)
			//		{
			//			// It is possible we might need to use:
			//			// MarkRegionsForCoors(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), STREAMING_PATH_NODES_DIST);
			//			// And have a radius of like 10 or so, just in case we want to load nodes around these nodes and we are close to a border
			//			const int RegionX = FindXRegionForCoors(GpsSlot.m_NodeCoordinates[j].x);
			//			const int RegionY = FindYRegionForCoors(GpsSlot.m_NodeCoordinates[j].y);
			//			ToBeStreamed[RegionX][RegionY] = 1;

			//			// Just in case
			//			Assert(RegionX >= 0);
			//			Assert(RegionY >= 0);
			//			Assert(RegionX < PATHFINDMAPSPLIT);
			//			Assert(RegionY < PATHFINDMAPSPLIT);
			//		}
			//	}
			//}

			// There is the possibility the level designers or the gps asked for this Region to be loaded up.
			for (s32 i = 0; i < MAX_NUM_NODE_REQUESTS; i++)
			{
				if (bActiveRequestForRegions[i])
				{
					s32	MinRegionX = FindXRegionForCoors(RequestMinX[i]);
					s32	MaxRegionX = FindXRegionForCoors(RequestMaxX[i]);
					s32	MinRegionY = FindYRegionForCoors(RequestMinY[i]);
					s32	MaxRegionY = FindYRegionForCoors(RequestMaxY[i]);

					Assert(MinRegionX >= 0);
					Assert(MinRegionY >= 0);
					Assert(MaxRegionX < PATHFINDMAPSPLIT);
					Assert(MaxRegionY < PATHFINDMAPSPLIT);

					for (RegionX = MinRegionX; RegionX <= MaxRegionX; RegionX++)
					{
						for (RegionY = MinRegionY; RegionY <= MaxRegionY; RegionY++)
						{
							if (WasRegionSkippedByPrologueNodeRejection(RegionX, RegionY))
							{
								continue;
							}

							ToBeStreamed[RegionX][RegionY] = 1;

							if (i >= GPS_NUM_SLOTS)
							{
								ToBeStreamedForScript[RegionX][RegionY] = 1;
							}
						}
					}
				}
			}

			// Now go through the array of CPathNodeRequiredArea's which allow
			// areas to be requested per-frame by script, game-code and the map
			for(s32 s=0; s<CPathNodeRequiredArea::m_iNumActiveAreas; s++)
			{
				const CPathNodeRequiredArea & reqArea = m_PathNodeRequiredAreas[s];

				s32	MinRegionX = FindXRegionForCoors(reqArea.m_vMin.x);
				s32	MaxRegionX = FindXRegionForCoors(reqArea.m_vMax.x);
				s32	MinRegionY = FindYRegionForCoors(reqArea.m_vMin.y);
				s32	MaxRegionY = FindYRegionForCoors(reqArea.m_vMax.y);

				Assert(MinRegionX >= 0);
				Assert(MinRegionY >= 0);
				Assert(MaxRegionX < PATHFINDMAPSPLIT);
				Assert(MaxRegionY < PATHFINDMAPSPLIT);

				for (RegionX = MinRegionX; RegionX <= MaxRegionX; RegionX++)
				{
					for (RegionY = MinRegionY; RegionY <= MaxRegionY; RegionY++)
					{
						if (WasRegionSkippedByPrologueNodeRejection(RegionX, RegionY))
						{
							continue;
						}

						ToBeStreamed[RegionX][RegionY] = 1;

						if(reqArea.m_iContext == CPathNodeRequiredArea::CONTEXT_SCRIPT)
						{
							ToBeStreamedForScript[RegionX][RegionY] = 1;
						}
					}
				}
			}
		}

		// Now go through the sectors and load the ones we require
#define ALLOW_REGION_LOAD_DEBUG __DEV
#if ALLOW_REGION_LOAD_DEBUG
		TUNE_GROUP_INT(VEH_REGION_LOAD_DEBUG, DontLoadRegion, -1, -1, 256, 1);
		TUNE_GROUP_BOOL(VEH_REGION_LOAD_DEBUG, AllowDontLoadRegion, true);
#endif //ALLOW_REGION_LOAD_DEBUG

		for (RegionX = 0; RegionX < PATHFINDMAPSPLIT; RegionX++)
		{
			for (RegionY = 0; RegionY < PATHFINDMAPSPLIT; RegionY++)
			{
				s32	Region = FIND_REGION_INDEX(RegionX , RegionY);
				strLocalIndex strIndex = GetStreamingIndexForRegion(Region);

#if ALLOW_REGION_LOAD_DEBUG
				if (ToBeStreamed[RegionX][RegionY] && (!AllowDontLoadRegion || Region != DontLoadRegion))
#else
				if (ToBeStreamed[RegionX][RegionY])
#endif //ALLOW_REGION_LOAD_DEBUG
				{
					// don't mess with dummy regions (they are degenerate and have no nodes)
					if(GetStreamingInfo(strIndex)->IsFlagSet(STRFLAG_INTERNAL_DUMMY))
					{
						continue;
					}

					NumRegionsToBeLoaded++;

					// Load sector in now.
					if (bBlockingLoad && ToBeStreamedForScript[RegionX][RegionY])
					{
						StreamingBlockingLoad(strIndex, STRFLAG_MISSION_REQUIRED|STRFLAG_FORCE_LOAD);
					}
					else if (ToBeStreamedForScript[RegionX][RegionY])
					{
						StreamingRequest(strIndex, STRFLAG_MISSION_REQUIRED | STRFLAG_PRIORITY_LOAD);
						ClearRequiredFlag(strIndex.Get(), STRFLAG_DONTDELETE);
					}
					else
					{
						StreamingRequest(strIndex, STRFLAG_DONTDELETE | STRFLAG_PRIORITY_LOAD);	
						ClearRequiredFlag(strIndex.Get(), STRFLAG_MISSION_REQUIRED);
					}		
				}
				else
				{
					// Make sure it is not loaded in.
					if (apRegions[Region] && apRegions[Region]->aNodes)
					{
						ClearRequiredFlag(strIndex.Get(), STRFLAG_DONTDELETE|STRFLAG_MISSION_REQUIRED);
					}
				}
			}
		}

		// Print out some shite.
		//	Displayf("NumRegionsToBeLoaded:%d\n", NumRegionsToBeLoaded);
	}

	// Clear all the per-frame required regions
	// Since this function is called by CStreaming::Update() which is one of the first things in the frame,
	// all the game logic and scripts will run subsequently and requests will be processed the next frame.

	ClearNodesRequiredRegions();
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : MakeRequestForNodesToBeLoaded
// PURPOSE :  For the script to request nodes to be loaded in certain regions.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::MakeRequestForNodesToBeLoaded(float MinX, float MaxX, float MinY, float MaxY, s32 requestIndex, bool bBlockingLoad)
{
	Assertf(requestIndex >= 0 && requestIndex < MAX_NUM_NODE_REQUESTS, "MakeRequestForNodesToBeLoaded requestIndex out of range");
	Assertf(bActiveRequestForRegions[requestIndex] == false || requestIndex != NODE_REQUEST_SCRIPT, "You can only have one script node-request at a time");

	bActiveRequestForRegions[requestIndex] = true;
	RequestMinX[requestIndex] = MinX;
	RequestMaxX[requestIndex] = MaxX;
	RequestMinY[requestIndex] = MinY;
	RequestMaxY[requestIndex] = MaxY;

	// Force the requests to be generated just now so that the level designers can call
	// a LOAD_ALL_MODELS this very frame.
	UpdateStreaming(true, bBlockingLoad);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ReleaseRequestedNodes
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::ReleaseRequestedNodes(s32 requestIndex)
{
	Assertf(requestIndex >= 0 && requestIndex < MAX_NUM_NODE_REQUESTS, "ReleaseRequestedNodes requestIndex out of range");
	bActiveRequestForRegions[requestIndex] = false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : HaveRequestedNodesBeenLoaded
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::HaveRequestedNodesBeenLoaded(s32 requestIndex)
{
	Assertf(requestIndex >= 0 && requestIndex < MAX_NUM_NODE_REQUESTS, "HaveRequestedNodesBeenLoaded requestIndex out of range");
	Assertf(bActiveRequestForRegions[requestIndex] == true, "No path nodes have been requested. Why check?");
	return (AreNodesLoadedForArea(RequestMinX[requestIndex], RequestMaxX[requestIndex], RequestMinY[requestIndex], RequestMaxY[requestIndex]));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AreNodesLoadedForArea
// PURPOSE :  Finds out whether the nodes are streamed in for this particular area.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::AreNodesLoadedForArea(float MinX, float MaxX, float MinY, float MaxY) const
{
	s32	BlockXMin, BlockYMin, BlockXMax, BlockYMax, LoopX, LoopY;

	BlockXMin = FindXRegionForCoors(MinX);
	BlockYMin = FindYRegionForCoors(MinY);
	BlockXMax = FindXRegionForCoors(MaxX);
	BlockYMax = FindYRegionForCoors(MaxY);

	for (LoopX = BlockXMin; LoopX <= BlockXMax; LoopX++)
	{
		for (LoopY = BlockYMin; LoopY <= BlockYMax; LoopY++)
		{
			const s32 iRegion = FIND_REGION_INDEX(LoopX, LoopY);
			if (!apRegions[iRegion] || !apRegions[iRegion]->aNodes )
			{
				// if the region isn't a degenerate region
				// AND it wasn't skipped by prologue node rejection
				if(!GetStreamingInfo(GetStreamingIndexForRegion(iRegion))->IsFlagSet(STRFLAG_INTERNAL_DUMMY) && !WasRegionSkippedByPrologueNodeRejection(LoopX, LoopY))
				{
					return(false);
				}
			}
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AreNodesLoadedForRadius
// PURPOSE :  Finds out whether the nodes are streamed in for this particular area.
// NOTE : Kind of copy pasted from void CPathFind::MarkRegionsForCoors(const Vector3& Coors, float Range)
//        So this must keep in sync with that function for making sure the GPS does not search around
//        areas were we need nodes to be streamed in
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::AreNodesLoadedForRadius(const Vector3& vPos, float fRadius) const
{
	static const float fRegionHalfSize = PATHFINDREGIONSIZEX*0.5f;
	static const float fRegionRadius = rage::Sqrtf((fRegionHalfSize*fRegionHalfSize)+(fRegionHalfSize*fRegionHalfSize));
	
	const float fMarkRangeSqr = square(fRadius + fRegionRadius);

	Assert(fRadius < 10000.0f);

	s32 MinRegionX = rage::Max(0, FindXRegionForCoors(vPos.x - fRadius));
	s32 MaxRegionX = rage::Min(PATHFINDMAPSPLIT-1, FindXRegionForCoors(vPos.x + fRadius));
	s32 MinRegionY = rage::Max(0, FindXRegionForCoors(vPos.y - fRadius));
	s32 MaxRegionY = rage::Min(PATHFINDMAPSPLIT-1, FindYRegionForCoors(vPos.y + fRadius));

	float fRegionMidX, fRegionMidY;
	for (s32 RegionX = MinRegionX; RegionX <= MaxRegionX; RegionX++)
	{
		for (s32 RegionY = MinRegionY; RegionY <= MaxRegionY; RegionY++)
		{
			FindStartPointOfRegion(RegionX, RegionY, fRegionMidX, fRegionMidY);
			fRegionMidX += fRegionHalfSize;
			fRegionMidY += fRegionHalfSize;

			const float fDistSqr = square(fRegionMidX - vPos.x) + square(fRegionMidY - vPos.y);
			if(fDistSqr < fMarkRangeSqr)
			{
				const s32 iRegion = FIND_REGION_INDEX(RegionX, RegionY);
				if (!apRegions[iRegion] || !apRegions[iRegion]->aNodes)
				{
					// if the region isn't a degenerate region
					// AND it wasn't skipped by prologue node rejection
					if(!GetStreamingInfo(GetStreamingIndexForRegion(iRegion))->IsFlagSet(STRFLAG_INTERNAL_DUMMY) && !WasRegionSkippedByPrologueNodeRejection(RegionX, RegionY))
					{
						return false;
					}
				}
			}
		}
	}	

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AreNodesLoadedForPoint
// PURPOSE :  Finds out whether the nodes are streamed in for this particular point.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::AreNodesLoadedForPoint(float testX, float testY) const
{
	s32	regionX, regionY;

	regionX = FindXRegionForCoors(testX);
	regionY = FindXRegionForCoors(testY);
	u32 iRegion = FIND_REGION_INDEX(regionX, regionY);

	// if the region is a degenerate region
	// OR it was skipped by prologue node rejection
	if(GetStreamingInfo(GetStreamingIndexForRegion(iRegion))->IsFlagSet(STRFLAG_INTERNAL_DUMMY) || WasRegionSkippedByPrologueNodeRejection(regionX, regionY))
	{
		return true;	// then count it as loaded
	}

	return(apRegions[iRegion] && apRegions[iRegion]->aNodes);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : LoadSceneForPathNodes
// PURPOSE :  Requests all the pathnode block around this particular area.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::LoadSceneForPathNodes(const Vector3& CenterCoors)
{
	s32	RegionX, RegionY;

	for (RegionX = 0; RegionX < PATHFINDMAPSPLIT; RegionX++)
	{
		for (RegionY = 0; RegionY < PATHFINDMAPSPLIT; RegionY++)
		{
			ToBeStreamed[RegionX][RegionY] = 0;
		}
	}
	MarkRegionsForCoors(CenterCoors, STREAMING_PATH_NODES_DIST_PLAYER);

	for (RegionX = 0; RegionX < PATHFINDMAPSPLIT; RegionX++)
	{
		for (RegionY = 0; RegionY < PATHFINDMAPSPLIT; RegionY++)
		{
			if (ToBeStreamed[RegionX][RegionY])
			{
				strLocalIndex	RegionIndex = GetStreamingIndexForRegion(FIND_REGION_INDEX(RegionX , RegionY));
				StreamingRequest(RegionIndex, 0);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AreWaterNodesNearby
// PURPOSE :  Find out whether water nodes are present within a certain range
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::IsWaterNodeNearby(const Vector3& Coors, float Range) const
{
	s32	Region, Node;

	float minY = Coors.y - Range;
	float maxY = Coors.y + Range;

	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->aNodes)
		{
			continue;
		}

		s32 start = 0;
		s32 end = 0;
		FindStartAndEndNodeBasedOnYRange(Region, minY, maxY, start, end);

		for (Node = start; Node < end; Node++)
		{
			if(!apRegions[Region]->aNodes[Node].m_2.m_waterNode)
			{
				continue;
			}

			Vector3 blockCoors;
			apRegions[Region]->aNodes[Node].GetCoors(blockCoors);
			if ((Coors - blockCoors).XYMag() < Range)
			{
				return true;
			}
		}
	}
	return false;
}


bool g_bCollisionTriangleIntersectsRoad;
bool g_bCollisionTriangleIntersectsRoad_IncludeSwitchedOff;
Vector3 g_vColPolyNormal;

bool CollisionTriangleIntersectsRoadCB(CPathNode * pPathNode, void * data)
{
	if(pPathNode->IsSwitchedOff() && !g_bCollisionTriangleIntersectsRoad_IncludeSwitchedOff)
		return true;

	const Vector3 * pPts = (const Vector3 *) data;

//	if(vNormal.z < 0.707f)
//		return true;

	Vector3 vNode;
	pPathNode->GetCoors(vNode);

	s32 i;
	for(i=0; i<pPathNode->NumLinks(); i++)
	{
		const CPathNodeLink & link = ThePaths.GetNodesLink(pPathNode, i);
		const CPathNode * pOtherNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);
		if(pOtherNode)
		{
			if(pOtherNode->IsSwitchedOff() && !g_bCollisionTriangleIntersectsRoad_IncludeSwitchedOff)
				continue;

			Vector3 vOther;
			pOtherNode->GetCoors(vOther);

			Vector3 vEdgeLeft, vEdgeRight;
			float fWidthScaling;

			ThePaths.IdentifyDrivableExtremes(pPathNode->m_address, pOtherNode->m_address, EmptyNodeAddress, pOtherNode->IsJunctionNode(), 0.0f, vEdgeLeft, vEdgeRight, fWidthScaling);

			Vector3 vFwd = vOther - vNode;
			float fLength = vFwd.Mag();
			vFwd.Normalize();

			Vector3 vRight = CrossProduct(vFwd, ZAXIS);
			vRight.Normalize();

			Vector3 vUp = CrossProduct(vRight, vFwd);
			vUp.Normalize();
			Assert(vUp.z > 0.0f);

			Vector3 vBoxMid = (vEdgeLeft + vEdgeRight) * 0.5f;
			
			Matrix34 mat;
			mat.Identity();
			mat.a = vRight;
			mat.b = vFwd;
			mat.c = vUp;
			mat.d = vBoxMid;

			static dev_float fExtra = 0.125f;
			static dev_float fExtraLength = 1.0f;

			Vector3 vSize;
			vSize.x = (vEdgeLeft - vEdgeRight).Mag();
			vSize.y = fLength;
			vSize.z = 4.0f;

			vSize *= 0.5f;
			vSize += Vector3(fExtra, fExtraLength, fExtra);

			bool bIntersects = geomBoxes::TestPolygonToOrientedBoxFP(
				pPts[0], pPts[1], pPts[2],
				g_vColPolyNormal,
				vBoxMid,
				mat,
				vSize);

#if 0
			{
				Vector3 vMin = vSize*-1.0f;
				Vector3 vMax = vSize;
				grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vMin), VECTOR3_TO_VEC3V(vMax), RCC_MAT34V(mat), Color_orange, false, 10);
			}
#endif

			if(bIntersects)
			{
				g_bCollisionTriangleIntersectsRoad = true;
				return false;
			}
		}
	}
	return true;
}

//------------------------------------------------------------------------------------------------
// FUNCTION : CollisionTriangleIntersectsRoad
// PURPOSE : Detects whether the triangle defined by pPts is in intersection with road geometry

bool CPathFind::CollisionTriangleIntersectsRoad(const Vector3 * pPts, const bool bIncludeSwitchedOff)
{
#if __DEV
	{
		grcDebugDraw::Line(pPts[0], pPts[1], Color_green, Color_green, 10);
		grcDebugDraw::Line(pPts[1], pPts[2], Color_green, Color_green, 10);
		grcDebugDraw::Line(pPts[2], pPts[0], Color_green, Color_green, 10);
	}
#endif

	Vector3 vEdge1 = pPts[1] - pPts[0];
	Vector3 vEdge2 = pPts[2] - pPts[0];
	vEdge1.Normalize();
	vEdge2.Normalize();
	g_vColPolyNormal = CrossProduct(vEdge2, vEdge1);


	Vector3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
	Vector3 vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	
	float fExpandAmt = 50.0f;

	s32 p;
	for(p=0; p<3; p++)
	{
		vMin.x = Min(vMin.x, pPts[p].x);
		vMin.y = Min(vMin.y, pPts[p].y);
		vMin.z = Min(vMin.z, pPts[p].z);

		vMax.x = Max(vMax.x, pPts[p].x);
		vMax.y = Max(vMax.y, pPts[p].y);
		vMax.z = Max(vMax.z, pPts[p].z);
	}

	vMin -= Vector3(fExpandAmt, fExpandAmt, fExpandAmt);
	vMax += Vector3(fExpandAmt, fExpandAmt, fExpandAmt);

	g_bCollisionTriangleIntersectsRoad = false;
	g_bCollisionTriangleIntersectsRoad_IncludeSwitchedOff = bIncludeSwitchedOff;

	ForAllNodesInArea( vMin, vMax, CollisionTriangleIntersectsRoadCB, (void*)pPts );

	return g_bCollisionTriangleIntersectsRoad;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindParkingNodeInArea
// PURPOSE :  In a certain area we return an appropriate parking node.
//			  (Try to cycle through them).
/////////////////////////////////////////////////////////////////////////////////
Vector3 CPathFind::FindParkingNodeInArea(float MinX, float MaxX, float MinY, float MaxY, float MinZ, float MaxZ) const
{
	s32	Region, Index = 0;
	s32	StartNode, EndNode, Node;
	static	s32	LookingForIndex = 0;
	Vector3	Found0, FoundIndex;
	bool	bFound0 = false, bFoundIndex = false;

	for (Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->aNodes)	// Make sure this Region is loaded in at the moment
		{
			continue;
		}

		FindStartAndEndNodeBasedOnYRange(Region, MinY, MaxY, StartNode, EndNode);

		for (Node = StartNode; Node < EndNode; Node++)
		{
			const CPathNode *pNode = &((apRegions[Region]->aNodes)[Node]);

			Vector3 nodePos(0.0f, 0.0f, 0.0f);
			pNode->GetCoors(nodePos);
			if(nodePos.x <= MinX || nodePos.x >= MaxX ||
				nodePos.z <= MinZ || nodePos.z >= MaxZ)
			{
				continue;
			}

			if(pNode->m_1.m_specialFunction != SPECIAL_PARKING_PERPENDICULAR)
			{
				continue;
			}
			
			if (Index == 0)
			{
				pNode->GetCoors(Found0);
				bFound0 = true;
			}
			if (Index == LookingForIndex)
			{
				pNode->GetCoors(FoundIndex);
				bFoundIndex = true;
			}
			Index++;			
		}
	}

	LookingForIndex++;
	if (LookingForIndex >= Index)
	{
		LookingForIndex = 0;
	}

	// If we didn't find a parking node we return (0, 0, 0) so that the script can test for this and abort whatever it is doing.
	// (Should probably be changed back for development on next game as it hides the nodes not being streamed in)
	if (!bFound0)
	{
		return Vector3(0.0f, 0.0f, 0.0f);
	}

	//	Assertf( bFound0, "Didn't find parking nodes in area (not streamed in yet?)");
	if (bFoundIndex)
	{
		return FoundIndex;
	}
	return Found0;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RegisterNodeUsage
// PURPOSE :  Registers this node as having been used. This will be remembered for a while
//			  so that other cars can avoid this node.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::RegisterNodeUsage(CNodeAddress nodeAddr)
{
	const CPathNode *pNode = FindNodePointer(nodeAddr);

	if (pNode->m_1.m_specialFunction == SPECIAL_PARKING_PARALLEL ||
		pNode->m_1.m_specialFunction == SPECIAL_PARKING_PERPENDICULAR)
	{
		for (s32 i = 0; i < PF_NUMUSEDNODES; i++)		// Check whether the node has been registered before.
		{
			if(m_aUsedNodes[i] == nodeAddr)
			{
				m_aTimeUsed[i] = fwTimer::GetTimeInMilliseconds();
				return;
			}
		}

		for(s32 i = 0; i < PF_NUMUSEDNODES; i++)		// Stick the new one in an emptyAddr slot.
		{
			if (m_aUsedNodes[i].IsEmpty())
			{
				m_aUsedNodes[i] = nodeAddr;
				m_aTimeUsed[i] = fwTimer::GetTimeInMilliseconds();
				return;
			}
		}

		u32	oldestTime = ~((u32)0);
		s32	oldestSlot = 0;
		for (s32 i = 0; i < PF_NUMUSEDNODES; i++)		// Stick the new one in the oldest.
		{
			if (m_aTimeUsed[i] < oldestTime)
			{
				oldestTime = m_aTimeUsed[i];
				oldestSlot = i;
			}
		}
		m_aUsedNodes[oldestSlot] = nodeAddr;
		m_aTimeUsed[oldestSlot] = fwTimer::GetTimeInMilliseconds();

	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : HasNodeBeenUsedRecently
// PURPOSE :  Registers this node as having been used. This will be remembered for a while
//			  so that other cars can avoid this node.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::HasNodeBeenUsedRecently(CNodeAddress nodeAddr) const
{
	for (s32 i = 0; i < PF_NUMUSEDNODES; i++)
	{
		if(m_aUsedNodes[i] == nodeAddr)
		{
			return true;
		}
	}
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateUsedNodes
// PURPOSE :  Registers this node as having been used. This will be remembered for a while
//			  so that other cars can avoid this node.
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::UpdateUsedNodes()
{
	if ( (fwTimer::GetSystemFrameCount() & 15) == 8)
	{
		for (s32 i = 0; i < PF_NUMUSEDNODES; i++)
		{
			if (fwTimer::GetTimeInMilliseconds() > m_aTimeUsed[i] + 60000)
			{
				m_aUsedNodes[i].SetEmpty();
			}
		}
	}
}




#if !__FINAL
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CheckPathNodeIntegrityForZone
// PURPOSE :  This function will make sure that if node A links up to link B the other way around
//			  works as well.
//			  This is done to catch the 'Tell Obbe it happened again' assert.
/////////////////////////////////////////////////////////////////////////////////

void CPathFind::CheckPathNodeIntegrityForZone(s32 Region) const
{
	Assert(apRegions[Region]);

	for (s32 Node = 0; Node < apRegions[Region]->NumNodes; Node++)
	{
		Assert(apRegions[Region]->aNodes);
		CPathNode *pNode = &apRegions[Region]->aNodes[Node];
		for(u32 Link = 0; Link < pNode->NumLinks(); Link++)
		{
			CNodeAddress otherNodeAddr = GetNodesLinkedNodeAddr(pNode, Link);
			Assert(GetNodesLink(pNode, Link).m_1.m_Distance > 0);

			if(otherNodeAddr.GetRegion() ==(u32)(Region))	// Only test the nodes that are in the same Region.
			{
				// Now make sure that these two nodes match up the other way as well.
				const CPathNode *pOtherNode = FindNodePointer(otherNodeAddr);
				u32 Link = 0;
				while((Link < pOtherNode->NumLinks()) && GetNodesLinkedNodeAddr(pOtherNode, Link) != pNode->m_address )
				{
					Link++;
				}
				if(Link >= pOtherNode->NumLinks())
				{
					Vector3 vNodeA, vNodeB;
					pNode->GetCoors(vNodeA);
					pOtherNode->GetCoors(vNodeB);

					//Assertf(false, "CheckPathNodeIntegrityForZone %d/%d %d/%d", pNode->GetAddrRegion(), pNode->GetAddrIndex(), pOtherNode->GetAddrRegion(), pOtherNode->GetAddrIndex());
					Assertf(false, "Link between node (%.1f,%.1f,%.1f) and (%.1f,%.1f,%.1f) is not bidirectional.",
						vNodeA.x, vNodeA.y, vNodeA.z, vNodeB.x, vNodeB.y, vNodeB.z);

					Displayf("Region: %i, NodeA: %i, NodeB: %i, NodeA.numLinks: %i, NodeB.numLinks: %i",
						Region, pNode->m_address.GetIndex(), pOtherNode->m_address.GetIndex(), pNode->NumLinks(), pOtherNode->NumLinks());
				}
				//Assertf(Link < pOtherNode->NumLinks(), "CheckPathNodeIntegrityForZone %d/%d %d/%d", pNode->GetAddrRegion(), pNode->GetAddrIndex(), pOtherNode->GetAddrRegion(), pOtherNode->GetAddrIndex());
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TestAllLinks
// PURPOSE :  This function runs through all loaded regions in the game and ensures they can all be
//			  loaded and accessed correctly when the region is loaded
/////////////////////////////////////////////////////////////////////////////////
void CPathFind::TestAllLinks() const
{
	for (int RegionX = 0; RegionX < PATHFINDMAPSPLIT; RegionX++)
	{
		for (int RegionY = 0; RegionY < PATHFINDMAPSPLIT; RegionY++)
		{
			s32	Region = FIND_REGION_INDEX(RegionX , RegionY);

			if(GetStreamingInfo(GetStreamingIndexForRegion(Region))->IsFlagSet(STRFLAG_INTERNAL_DUMMY))
				continue;

			//StreamingBlockingLoad(strLocalIndex(Region), STRFLAG_MISSION_REQUIRED|STRFLAG_FORCE_LOAD);

			if(IsRegionLoaded(Region))
			{
				for(int k = 0; k < apRegions[Region]->NumNodes; k++)
				{
					CNodeAddress nodeAddr;
					nodeAddr.Set(Region, k);
					const CPathNode* pNode = FindNodePointerSafe(nodeAddr);
					if(pNode)
					{
						int iNumLinks = pNode->NumLinks();
						for(int j = 0; j < iNumLinks; j++)
						{
							const CPathNodeLink* link = &ThePaths.GetNodesLink(pNode, j);
							if(!link)
							{
								Assertf(false, "No link from node at region %d:%d index %d link %d", RegionX, RegionY, k, j);
							}
						}
					}
					else
					{
						Assertf(false, "Node not loaded for region %d:%d index %d", RegionX, RegionY, k);
					}
				}
			}
		}
	}
}

#endif // !__FINAL


#if __DEV
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PrintStreetNames
// PURPOSE :  Print out a list of all the streetnames in the game and the coordinates
//			  of a node on it.
/////////////////////////////////////////////////////////////////////////////////
void PrintStreetNames()
{
#define	MAX_NUM_STREET_NAMES (200)
	u32 streetNameArray[MAX_NUM_STREET_NAMES];

	for (s32 i = 0; i < MAX_NUM_STREET_NAMES; i++)
	{
		streetNameArray[i] = 0;
	}

	// Make sure all nodes are streamed in
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		ThePaths.StreamingRequest(ThePaths.GetStreamingIndexForRegion(Region), STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
	}
	CStreaming::LoadAllRequestedObjects();

	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		Assert(ThePaths.apRegions[Region] && ThePaths.apRegions[Region]->aNodes);

		for (s32 node = 0; node < ThePaths.apRegions[Region]->NumNodesCarNodes; node++)
		{
			u32 streetName = ThePaths.apRegions[Region]->aNodes[node].m_streetNameHash;

			if (streetName)
			{
				// If we have already printed this one out it is ignored.
				// Otherwise we print it and put it in the list.
				s32 index = 0;
				while (streetNameArray[index] && streetNameArray[index] != streetName)
				{
					index++;
				}
				Assert(index < MAX_NUM_STREET_NAMES-1);
				if (!streetNameArray[index])
				{
					Vector3 crs;
					ThePaths.apRegions[Region]->aNodes[node].GetCoors(crs);
					
					Displayf("%s      %d %d\n", TheText.Get(streetName, "streetname"), ((s32)crs.x), ((s32)crs.y) );
					streetNameArray[index] = streetName;
				}
			}
		}
	}
}

// void FindDontWanderNodes()
// {
// 	// Make sure all nodes are streamed in
// 	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
// 	{
// 		ThePaths.StreamingRequest(Region, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
// 	}
// 	CStreaming::LoadAllRequestedObjects();
// 
// 	int iNumDontWanderNodes = 0;
// 	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
// 	{
// 		Assert(ThePaths.apRegions[Region] && ThePaths.apRegions[Region]->aNodes);
// 
// 		for (s32 node = 0; node < ThePaths.apRegions[Region]->NumNodesCarNodes; node++)
// 		{
// 			if (ThePaths.apRegions[Region]->aNodes[node].m_2.m_dontWanderHere)
// 			{
// 				aiDisplayf("Node %d:%d dont wander here", Region, node);
// 				++iNumDontWanderNodes;
// 			}
// 		}
// 	}
// 
// 	aiDisplayf("%d dont wander here nodes", iNumDontWanderNodes);
// }

void SwitchOffAllLoadedNodes()
{
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if (ThePaths.apRegions[Region] && ThePaths.apRegions[Region]->aNodes)
		{
			for (s32 node = 0; node < ThePaths.apRegions[Region]->NumNodesCarNodes; node++)
			{
				CPathNode	*pNode = &((ThePaths.apRegions[Region]->aNodes)[node]);
				pNode->m_2.m_switchedOff = true;
			}
		}	
	}
}

void PrintSizeOfAllLoadedRegions()
{
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if (ThePaths.apRegions[Region] && ThePaths.apRegions[Region]->aNodes)
		{
			u32 nTotalSize = sizeof(*ThePaths.apRegions[Region]);
			nTotalSize += sizeof(CPathNode) * ThePaths.apRegions[Region]->NumNodes;
			nTotalSize += sizeof(CPathNodeLink) * ThePaths.apRegions[Region]->NumLinks;
			Displayf("Region %d Size %d", Region, nTotalSize);
		}	
	}
}

void CountNumberJunctionsInRegion()
{
	if (ThePaths.DebugCurrentRegion < 0 || ThePaths.DebugCurrentRegion >= PATHFINDREGIONS
		|| !ThePaths.IsRegionLoaded(ThePaths.DebugCurrentRegion))
	{
		return;
	}

	int iNumJunctions = 0;
	for (s32 node = 0; node < ThePaths.apRegions[ThePaths.DebugCurrentRegion]->NumNodesCarNodes; node++)
	{
		CPathNode	*pNode = &((ThePaths.apRegions[ThePaths.DebugCurrentRegion]->aNodes)[node]);
		if (pNode->IsJunctionNode())
		{
			++iNumJunctions;
		}
	}

	aiDisplayf("%d junctions in region %d", iNumJunctions, ThePaths.DebugCurrentRegion);
}

void ResampleVirtualJunctionFromWorld()
{
	if (ThePaths.DebugCurrentRegion < 0 || ThePaths.DebugCurrentRegion >= PATHFINDREGIONS
		|| !ThePaths.IsRegionLoaded(ThePaths.DebugCurrentRegion))
	{
		return;
	}

	u32 nJunctionIndex = ThePaths.DebugCurrentNode;

	CPathRegion* pRegion = ThePaths.apRegions[ThePaths.DebugCurrentRegion];
	CNodeAddress junctionNode;
	junctionNode.Set(ThePaths.DebugCurrentRegion, nJunctionIndex);

	if (junctionNode.IsEmpty())
	{
		return;
	}

	CJunction* pJunction = CJunctions::FindJunctionFromNode(junctionNode);
	if (!pJunction)
	{
		return;
	}

	s32 nVirtJnIndex = pJunction->GetVirtualJunctionIndex();
	CPathVirtualJunction* pVirtJn = &pRegion->aVirtualJunctions[nVirtJnIndex];

	CVirtualJunctionInterface virtJnInterface(pVirtJn, pRegion);
	virtJnInterface.InitAndSample(pJunction, 2.0f, pVirtJn->m_nStartIndexOfHeightSamples, ThePaths);
}

void ResetAllLoadedNodes()
{
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if (ThePaths.apRegions[Region] && ThePaths.apRegions[Region]->aNodes)
		{
			for (s32 node = 0; node < ThePaths.apRegions[Region]->NumNodesCarNodes; node++)
			{
				CPathNode	*pNode = &((ThePaths.apRegions[Region]->aNodes)[node]);
				pNode->m_2.m_switchedOff = pNode->m_2.m_switchedOffOriginal;
			}
		}	
	}
}

void OutputAllNodes()
{
	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if (ThePaths.apRegions[Region] && ThePaths.apRegions[Region]->aNodes)
		{
			Displayf("Region %d", Region);
			for (s32 node = 0; node < ThePaths.apRegions[Region]->NumNodesCarNodes; node++)
			{
				Displayf("Node %d: %s", node, ThePaths.apRegions[Region]->aNodes[node].IsSwitchedOff() ? "OFF" : "ON");
			}
		}	
	}
}

#endif // __BANK


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeWithIndex
// PURPOSE :  Goes through the nodes on the map and finds the one in sequence.
/////////////////////////////////////////////////////////////////////////////////
CNodeAddress CPathFind::FindNodeWithIndex(int index, bool bIgnoreSwitchedOff, bool bIgnoreAlreadyFound, bool bBoatNodes, bool bHidingNodes, bool UNUSED_PARAM(bNetworkRestart))
{
	CNodeAddress	closestNodeAddr;
	s32			StartNode, EndNode;

	for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
	{
		if(!apRegions[Region] || !apRegions[Region]->aNodes)	// Make sure this Region is loaded in at the moment
		{
			continue;
		}

			StartNode = 0;
			EndNode = apRegions[Region]->NumNodesCarNodes;
			for (s32 Node = StartNode; Node < EndNode; Node++)
			{
				CPathNode	*pNode = &((apRegions[Region]->aNodes)[Node]);

			if(	(bIgnoreSwitchedOff && pNode->m_2.m_switchedOff) ||
				(bIgnoreAlreadyFound && pNode->m_2.m_alreadyFound) ||
				(bBoatNodes != static_cast<bool>(pNode->m_2.m_waterNode)) ||
				(bHidingNodes != (static_cast<s32>(pNode->m_1.m_specialFunction) == SPECIAL_HIDING_NODE)))
				{
				continue;
			}

					if (index == 0)
			{		// This is our nodeAddr
						return pNode->m_address;
					}
					else
					{
						index--;
					}
				}
			}

	return(closestNodeAddr);
}


#define MAX_NUM_RESTART_NODES_STORED	(64)
typedef struct
{
	CNodeAddress m_nodeAddr;
	float m_nodeScore;
} SScoredNode;

SScoredNode	aStoredNetworkRestartNodesArray[MAX_NUM_RESTART_NODES_STORED];

s32			numStoredNetworkRestartNodes = 0;
s32			numReturnedNetworkRestartNodes = 0;
s32			numConsideredNetworkRestartNodes = 0;
Vector3		centrePointOfBatchedNodes;
float		radiusOfBatchedNodes;
s32			usageType;				// What function used it last

enum
{
	USAGE_GET_RANDOM_NETWORK_RESTART_NODE = 0,
	USAGE_GET_RANDOM_CAR_NODE,
	USAGE_GET_FURTHEST_NETWORK_RESTART_NODE
};
u8			aNodeGroups[NUM_NODE_GROUPS_FOR_NETWORK_RESTART_NODES];


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindBestNodePair
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CPathFind::FindBestNodePair(const Vector3& vTestPos, const Vector3& vForward, CNodeAddress& BestOldNode, CNodeAddress& BestNewNode, const float fSearchProximity)
{
	const float fAdjacentRegionProximity = fSearchProximity + 50.0f;
	const float fCullDistSqr = fSearchProximity*fSearchProximity;
	
	static const float fZComponentMultiplier = 3.0f;

	float ClosestDist = 99999.9f;
	CNodeAddress BestNode1, BestNode2;

	// Go through all the nodes in our region and any necessary neighbouring ones
	const s32 iCenterRegionX = FindXRegionForCoors(vTestPos.x);
	const s32 iCenterRegionY = FindYRegionForCoors(vTestPos.y);

	const float fCoorsWithinRegionX = vTestPos.x - FindXCoorsForRegion(iCenterRegionX);
	const float fCoorsWithinRegionY = vTestPos.y - FindYCoorsForRegion(iCenterRegionY);

	const float fSearchMinY = vTestPos.y - fSearchProximity;
	const float fSearchMaxY = vTestPos.y + fSearchProximity;

	const s32 iSearchMins[3] =
	{
		static_cast<s32>((vTestPos.x - fSearchProximity) * PATHCOORD_XYSHIFT),
		static_cast<s32>(fSearchMinY * PATHCOORD_XYSHIFT),
		static_cast<s32>((vTestPos.z - fSearchProximity) * PATHCOORD_ZSHIFT)
	};
	const s32 iSearchMaxs[3] =
	{
		static_cast<s32>((vTestPos.x + fSearchProximity) * PATHCOORD_XYSHIFT),
		static_cast<s32>(fSearchMaxY * PATHCOORD_XYSHIFT),
		static_cast<s32>((vTestPos.z + fSearchProximity) * PATHCOORD_ZSHIFT)
	};

	s32 iRegionMinX = iCenterRegionX;
	s32 iRegionMaxX = iCenterRegionX;
	s32 iRegionMinY = iCenterRegionY;
	s32 iRegionMaxY = iCenterRegionY;


	if(fCoorsWithinRegionX < fAdjacentRegionProximity)
	{
		iRegionMinX--;
	}
	if(fCoorsWithinRegionY < fAdjacentRegionProximity)
	{
		iRegionMinY--;
	}
	if(fCoorsWithinRegionX > PATHFINDREGIONSIZEX-fAdjacentRegionProximity)
	{
		iRegionMaxX++;
	}
	if(fCoorsWithinRegionY > PATHFINDREGIONSIZEY-fAdjacentRegionProximity)
	{
		iRegionMaxY++;
	}
	iRegionMinX = rage::Max(0, iRegionMinX);
	iRegionMinY = rage::Max(0, iRegionMinY);
	iRegionMaxX = rage::Min(PATHFINDMAPSPLIT-1, iRegionMaxX);
	iRegionMaxY = rage::Min(PATHFINDMAPSPLIT-1, iRegionMaxY);

	Vector3 vCoors1, vCoors2, vCoorsNode1, vCoorsNode2, vCoorsPoint, vLineDir;

	// Go through the relevant regions
	for(s32 iRegionX = iRegionMinX; iRegionX <= iRegionMaxX; iRegionX++)
	{
		for(s32 iRegionY = iRegionMinY; iRegionY <= iRegionMaxY; iRegionY++)
		{
			s32 iRegion = FIND_REGION_INDEX(iRegionX, iRegionY);
			if(!apRegions[iRegion] || !apRegions[iRegion]->aNodes)	// Make sure nodes are loaded in.
			{
				continue;
			}

			// Go through all of the nodes in this region.
			s32 iStartNode, iEndNode;
			FindStartAndEndNodeBasedOnYRange(iRegion, fSearchMinY, fSearchMaxY, iStartNode, iEndNode);				
		
			for(s32 iNode = iStartNode; iNode < iEndNode; iNode++)
			{
				CPathNode *pNode1 = &(apRegions[iRegion]->aNodes)[iNode];

				// Quick integer min/max compare to cull nodes outside search extents
				if(pNode1->m_coorsX < iSearchMins[0] || pNode1->m_coorsX >= iSearchMaxs[0] ||
					pNode1->m_coorsY < iSearchMins[1] || pNode1->m_coorsY >= iSearchMaxs[1] ||
					 pNode1->m_1.m_coorsZ < iSearchMins[2] || pNode1->m_1.m_coorsZ >= iSearchMaxs[2])
				{
					continue;
				}

				// Get XYZ coords & do a more thorough rejection test
				pNode1->GetCoors(vCoors1);
				if((vCoors1 - vTestPos).Mag2() >= fCullDistSqr)
				{
					continue;
				}

				// Now iterate all the linked nodes
				for(u32 iLink = 0; iLink < pNode1->NumLinks(); iLink++)
				{
					CNodeAddress Node2 = GetNodesLinkedNodeAddr(pNode1, iLink);
					if(!IsRegionLoaded(Node2))
					{
						continue;
					}
				
					const CPathNode *pNode2 = FindNodePointer(Node2);
					pNode2->GetCoors(vCoors2);

					// Evaluate this node pair.
					// Calculate the distance of the vehicle to the node-to-node line.
					// We want the z-component to be more important so that we don't get confused by fly-overs etc
					vCoorsNode1 = Vector3(vCoors1.x, vCoors1.y, vCoors1.z * fZComponentMultiplier);
					vCoorsNode2 = Vector3(vCoors2.x, vCoors2.y, vCoors2.z * fZComponentMultiplier);
					vCoorsPoint = Vector3(vTestPos.x, vTestPos.y, vTestPos.z * fZComponentMultiplier);

					float Dist = fwGeom::DistToLine(vCoorsNode1, vCoorsNode2, vCoorsPoint);

					// We also take the orientation of the car into account.
					vLineDir = vCoors2 - vCoors1;
					vLineDir.NormalizeFast();
					const float DotPr = DotProduct(vLineDir, vForward);
					Dist += (1.0f - DotPr) * 5.0f;

					if(Dist < ClosestDist)
					{
						BestNode1.Set(iRegion, iNode); 
						BestNode2 = Node2;
						ClosestDist = Dist;
					}
				}
			}
		}
	}
	BestOldNode = BestNode1;
	BestNewNode = BestNode2;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetRandomCarNode
// PURPOSE :  Returns a random car node in a radius from a point.
//			  At first it fetches them and then it returns them one by one from the array.
/////////////////////////////////////////////////////////////////////////////////
bool CPathFind::GetRandomCarNode(Vector3 &centrePoint, float radius, bool waterNodesInstead, s32 UNUSED_PARAM(minLanes), bool bAvoidDeadEnds, bool bAvoidHighways, bool bAvoidSwitchedOff, Vector3 &vecReturn, CNodeAddress &returnNodeAddr)
{
	if (numStoredNetworkRestartNodes <= numReturnedNetworkRestartNodes ||
		centrePoint != centrePointOfBatchedNodes || radius != radiusOfBatchedNodes || usageType != USAGE_GET_RANDOM_CAR_NODE)
	{
		centrePointOfBatchedNodes = centrePoint;
		radiusOfBatchedNodes = radius;
		numStoredNetworkRestartNodes = numReturnedNetworkRestartNodes = numConsideredNetworkRestartNodes = 0;
		usageType = USAGE_GET_RANDOM_CAR_NODE;

		for (s32 Region = 0; Region < PATHFINDREGIONS; Region++)
		{
			if (!apRegions[Region] || !apRegions[Region]->aNodes)
			{
				continue;
			}

			for (s32 node = 0; node < apRegions[Region]->NumNodesCarNodes; node++)
			{
				CPathNode *pNode = &apRegions[Region]->aNodes[node];
				if (pNode->HasSpecialFunction_Driving())
				{
					continue;
				}

				if(pNode->NumLinks() != 2)
				{
					continue;
				}

				if(	(!waterNodesInstead && pNode->m_2.m_waterNode) ||
					(waterNodesInstead && !pNode->m_2.m_waterNode))
				{
					continue;
				}

				if(bAvoidDeadEnds && pNode->m_2.m_deadEndness)
				{
					continue;
				}

				if (bAvoidHighways && pNode->IsHighway())
				{
					continue;
				}

				if (bAvoidSwitchedOff &&pNode->m_2.m_switchedOff)
				{
					continue;
				}

				Vector3 crs;
				pNode->GetCoors(crs);
				if ((crs - centrePoint).XYMag() >= radius)
				{
					continue;
				}

				// Actually consider this one.
				numConsideredNetworkRestartNodes++;
				if (numStoredNetworkRestartNodes < MAX_NUM_RESTART_NODES_STORED)
				{
					aStoredNetworkRestartNodesArray[numStoredNetworkRestartNodes++].m_nodeAddr = pNode->m_address;
				}
				else
				{
					if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < (MAX_NUM_RESTART_NODES_STORED / float(numConsideredNetworkRestartNodes)) )
					{
						aStoredNetworkRestartNodesArray[ fwRandom::GetRandomNumberInRange(0, MAX_NUM_RESTART_NODES_STORED)].m_nodeAddr = pNode->m_address;
					}
				}
			}
		}
		// Now randomize things a bit.
		if (numStoredNetworkRestartNodes > 0)
		{
			for (s32 i = 0; i < 512; i++)
			{
				s32	n1 = fwRandom::GetRandomNumberInRange(0, numStoredNetworkRestartNodes);
				s32	n2 = fwRandom::GetRandomNumberInRange(0, numStoredNetworkRestartNodes);
				SScoredNode tempNode = aStoredNetworkRestartNodesArray[n1];
				aStoredNetworkRestartNodesArray[n1] = aStoredNetworkRestartNodesArray[n2];
				aStoredNetworkRestartNodesArray[n2] = tempNode;
			}
		}
	}

	CNodeAddress nodeAddr;
	while (1)
	{		
		if (numReturnedNetworkRestartNodes >= numStoredNetworkRestartNodes)
		{
			nodeAddr.SetEmpty();
			break;
		}
		nodeAddr = aStoredNetworkRestartNodesArray[numReturnedNetworkRestartNodes++].m_nodeAddr;

		if( !IsRegionLoaded(nodeAddr))
		{
			nodeAddr.SetEmpty();
			continue;
		}
		break;
	}

	if(!nodeAddr.IsEmpty())
	{
		CPathNode *pNode = &apRegions[nodeAddr.GetRegion()]->aNodes[nodeAddr.GetIndex()];
		pNode->GetCoors(vecReturn);
		//		Heading = pNodeA->m_2.m_density *(360.0f / 16.0f);
		returnNodeAddr = nodeAddr;
		Assert(returnNodeAddr.GetRegion() < PATHFINDREGIONS && returnNodeAddr.GetIndex() < 10000);
		return true;
	}
	returnNodeAddr.SetEmpty();
	return false;
}


// DEBUG!! -AC, This only every uses the first lane...
void	CPathFind::GetSpawnCoordinatesForCarNode(CNodeAddress nodeAddr, Vector3 &towardsPos, Vector3 &vecReturn, float &heading)
{
	float	bestSuitability = -9999999.9f;
	Assert(!nodeAddr.IsEmpty());
	Assert(IsRegionLoaded(nodeAddr));

	const CPathNode *pNodeFrom = FindNodePointer(nodeAddr);

	pNodeFrom->GetCoors(vecReturn);
	heading = 0.0f;

	for(u32 linkIndex = 0; linkIndex < pNodeFrom->NumLinks(); linkIndex++)
	{
		const CPathNodeLink& link = GetNodesLink(pNodeFrom, linkIndex);
		CNodeAddress	nodeToAddr = link.m_OtherNode;
		if(!IsRegionLoaded(nodeToAddr))
		{
			continue;
		}

		const CPathNode *pNodeTo = FindNodePointer(nodeToAddr);
			Vector3 fromCoors,toCoors;

			pNodeFrom->GetCoors(fromCoors);
			pNodeTo->GetCoors(toCoors);

			Vector3	toVector = toCoors - fromCoors;
			toVector.Normalize();

			Vector3	spawnCoors = (fromCoors + toCoors) * 0.5f;
			Vector3 vUnitToTarget = towardsPos - spawnCoors;
			vUnitToTarget.Normalize();
			float	suitability = toVector.Dot(vUnitToTarget);

			if (suitability > bestSuitability)
			{
				bestSuitability = suitability;
				vecReturn = spawnCoors;
#if __DEV
				vecReturn +=(Vector3(toVector.y, -toVector.x, 0.0f)*(bMakeAllRoadsSingleTrack?0.0f:link.InitialLaneCenterOffset()));
#else
				vecReturn +=(Vector3(toVector.y, -toVector.x, 0.0f)*(link.InitialLaneCenterOffset()));
#endif // __DEV
				heading = (( RtoD * rage::Atan2f(-toVector.x, toVector.y)));
			}
		}
	}
// END DEBUG!!


void CPathFind::RegisterStreamingModule()
{
	strStreamingEngine::GetInfo().GetModuleMgr().AddModule(&ThePaths);
}

void CPathFind::DetermineValidPathNodeFiles()
{	
	for(int i = 0; i < PATHFINDREGIONS; i++)
	{
		strLocalIndex strIndex = strLocalIndex(i);
		strStreamingInfo* info = ThePaths.GetStreamingInfo(strIndex);	
		if(info->IsValid())
		{
			//Displayf("Region %d is legit", i);
			// Register as valid file?
			Assertf(!ThePaths.GetStreamingInfo(strIndex)->IsFlagSet(STRFLAG_INTERNAL_DUMMY), "Region %d is marked as dummy, but exists in the packfile!", i);
		}
		else
		{
			//Displayf("Region %d is degenerate", i);
			CStreaming::RegisterDummyObject("DEGENERATEPATHREGION", strIndex, ThePaths.GetStreamingModuleId());
			Assert(ThePaths.GetStreamingInfo(strIndex)->IsFlagSet(STRFLAG_INTERNAL_DUMMY));
		}

		//also do heist nodes
		strLocalIndex strIndexHeist = strLocalIndex(i + sm_iHeistsOffset);
		strStreamingInfo* infoHeist = ThePaths.GetStreamingInfo(strIndexHeist);	
		if(infoHeist->IsValid())
		{
			//Displayf("Region %d is legit", i);
			// Register as valid file?
			Assertf(!ThePaths.GetStreamingInfo(strIndexHeist)->IsFlagSet(STRFLAG_INTERNAL_DUMMY), "Region %d is marked as dummy, but exists in the packfile!", i);
		}
		else
		{
			//Displayf("Region %d is degenerate", i);
			CStreaming::RegisterDummyObject("DEGENERATEPATHREGION", strIndexHeist, ThePaths.GetStreamingModuleId());
			Assert(ThePaths.GetStreamingInfo(strIndexHeist)->IsFlagSet(STRFLAG_INTERNAL_DUMMY));
		}
	}
}

#if __DEV
void MovePlayerToNodeNearMarker()		// A Debug function to make things easier for the level designers
{
	/*
	static	s32 LastMarkerIndex = -1;
	s32	OldLastMarkerIndex;
	___Again:
	OldLastMarkerIndex = LastMarkerIndex;

	for (s32 nTrace = 0; nTrace < RADAR_MAX_BLIPS; nTrace++)
	{
	switch (CRadar::ms_RadarTrace[nTrace].BlipType)
	{
	case BLIP_TYPE_CONTACT:
	case BLIP_TYPE_COORDS:
	if (nTrace > LastMarkerIndex)
	{
	LastMarkerIndex = nTrace;
	nTrace = RADAR_MAX_BLIPS;
	break;
	}
	break;
	}
	}
	
	if (OldLastMarkerIndex == LastMarkerIndex && LastMarkerIndex < 0)
	{
	return;		// Didn't find any
	}

	if (OldLastMarkerIndex == LastMarkerIndex)
	{
	LastMarkerIndex = -1;
	goto ___Again;
	}

	Vector3 SearchCoors = Vector3(CRadar::ms_RadarTrace[LastMarkerIndex].position.x, CRadar::ms_RadarTrace[LastMarkerIndex].position.y, CRadar::ms_RadarTrace[LastMarkerIndex].position.z);

	// Have to make sure the nodes are streamed in here I suppose.
	s32 index = FIND_REGION_INDEX(ThePaths.FindXRegionForCoors(SearchCoors.x) , ThePaths.FindXRegionForCoors(SearchCoors.y));
	ThePaths.StreamingRequest(index);
	CStreaming::LoadAllRequestedObjects();

	CNodeAddress nearestNodesAddr;
	if (FindPlayerVehicle())
	{
		nearestNodesAddr = ThePaths.FindNodeClosestToCoors(SearchCoors, 100.0f, false, false, false);
	}
	else
	{
		nearestNodesAddr = ThePaths.FindNodeClosestToCoors(SearchCoors, 100.0f, false, false, false);
	}

	Vector3	WarpCoors;
	if(!nearestNodesAddr.IsEmpty())
	{
		if(ThePaths.IsRegionLoaded(nearestNodesAddr))
	{
			WarpCoors = ThePaths.FindNodePointer(nearestNodesAddr)->GetCoors();
	WarpCoors.z += 1.0f;
	}
	}

	if (FindPlayerVehicle())
	{
	FindPlayerVehicle()->Teleport(WarpCoors, false);	
	}
	else
	{
	FindPlayerPed()->Teleport(WarpCoors, false);
	}
	*/
}
#endif // __DEV

void CPathFindWrapper::Init(unsigned initMode)
{
    ThePaths.Init(initMode);
}

void CPathFindWrapper::Shutdown(unsigned shutdownMode)
{
    ThePaths.Shutdown(shutdownMode);
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNumberNonSpecialNeighbours
// PURPOSE :  Goes through the neighbours and calculates the ones that are
//			  non special.
/////////////////////////////////////////////////////////////////////////////////
s32 CPathNode::FindNumberNonSpecialNeighbours() const
{
	s32	result = 0;
	u32 nLinks = NumLinks();
	for(u32 i = 0; i < nLinks; i++)
	{
		CNodeAddress linkNodeAddr = ThePaths.GetNodesLinkedNodeAddr(this, i);
		if(ThePaths.IsRegionLoaded(linkNodeAddr))
		{
			if(!ThePaths.FindNodePointer(linkNodeAddr)->HasSpecialFunction())
			{
				result++;
			}
		}
	}

	return result;
}

s32 CPathNode::FindNumberNonShortcutLinks() const
{
	s32	result = 0;
	u32 nLinks = NumLinks();
	for(u32 i = 0; i < nLinks; i++)
	{
		const CPathNodeLink* pLink = &ThePaths.GetNodesLink(this, i);
		if (!pLink->IsShortCut())
		{
			++result;
		}
	}

	return result;
}

bool CPathNode::HasShortcutLinks() const
{
	u32 nLinks = NumLinks();
	for(u32 i = 0; i < nLinks; i++)
	{
		const CPathNodeLink* pLink = &ThePaths.GetNodesLink(this, i);
		if (pLink->IsShortCut())
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitWidgets
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

#if __BANK
void ClearFocusVehicleNodes()
{
	CVehicle * pVeh = CVehicleDebug::GetFocusVehicle();
	if(pVeh)
	{
		CVehicleNodeList * pNodeList = pVeh->GetIntelligence()->GetNodeList();
		if(pNodeList)
		{
			pNodeList->Clear();
		}
	}
}


void CPathFind::InitWidgets()
{
	// Add the debugging widgets..
	//bkBank * pExistingBank = BANKMGR.FindBankCreateBank
	bkBank & bank = BANKMGR.CreateBank("Vehicle AI and Nodes");

#if __BANK
	bank.PushGroup("Paths", false);
	bank.AddToggle("Load all path regions", &bLoadAllRegions);
	bank.AddToggle("Try and stream prologue nodes", &bWillTryAndStreamPrologueNodes);
	bank.AddToggle("Try and stream heist island nodes", &bStreamHeistIslandNodes);
	bank.AddToggle("Display path history", &bDisplayPathHistory);
	bank.AddToggle("Display region extents", &bDisplayRegionBoxes);
	bank.AddToggle("Display required node regions", &bDisplayRequiredNodeRegions);
	bank.AddToggle("Allow Display path debug", &bDisplayPathsDebug_Allow);
	bank.AddSlider("Debug draw distance flat", &fDebgDrawDistFlat, 0.0f, 10000.0f, 1.0f);
	bank.AddSeparator();
	bank.AddTitle("Road Mesh");
	bank.AddToggle("Allow path lines as road mesh debug", &bDisplayPathsDebug_RoadMesh_AllowDebugDraw);
	bank.AddSlider("Display road mesh alpha fringe portion", &fDebugAlphaFringePortion, 0.0f, 1.0f, 0.01f);
	bank.AddSeparator();
	bank.AddTitle("Node debug display");
	bank.AddToggle("Allow path debug for nodes", &bDisplayPathsDebug_Nodes_AllowDebugDraw);
	bank.AddToggle("Display path node standard info", &bDisplayPathsDebug_Nodes_StandardInfo);
	bank.AddToggle("Display path node pilons", &bDisplayPathsDebug_Nodes_Pilons);
	bank.AddToggle("Display difference node/collision", &bDisplayPathsDebug_Nodes_ToCollisionDiff);
	bank.AddToggle("Display streetnames with path lines", &bDisplayPathsDebug_Nodes_StreetNames);
	bank.AddToggle("Display distance hash and highway with path lines", &bDisplayPathsDebug_Nodes_DistanceHash);
	bank.AddSeparator();
	bank.AddTitle("Link debug display");
	bank.AddToggle("Allow path debug for links", &bDisplayPathsDebug_Links_AllowDebugDraw);
	bank.AddToggle("Display path lines link lines", &bDisplayPathsDebug_Links_DrawLine);
	bank.AddToggle("Display path lane dir arrows", &bDisplayPathsDebug_Links_DrawLaneDirectionArrows);
	bank.AddToggle("Display path traffic lights", &bDisplayPathsDebug_Links_DrawTrafficLigths);
	bank.AddToggle("Display path link text info", &bDisplayPathsDebug_Links_TextInfo);
	bank.AddToggle("Display path lane centers", &bDisplayPathsDebug_Links_LaneCenters);
	bank.AddToggle("Display path road extremes", &bDisplayPathsDebug_Links_RoadExtremes);
	bank.AddToggle("Display tilt with path lines", &bDisplayPathsDebug_Links_Tilt);
	bank.AddToggle("Display links region & index", &bDisplayPathsDebug_Links_RegionAndIndex);
	bank.AddToggle("Display, ignore ped crossings", &bDisplayPathsDebug_IgnoreSpecial_PedCrossing);
	bank.AddToggle("Display, only ped crossings", &bDisplayPathsDebug_OnlySpecial_PedCrossing);
	bank.AddToggle("Display, ignore ped driveway crossings", &bDisplayPathsDebug_IgnoreSpecial_PedDriveWayCrossing);
	bank.AddToggle("Display, only ped driveway crossings", &bDisplayPathsDebug_OnlySpecial_PedDriveWayCrossing);
	bank.AddToggle("Display, ignore ped assisted movement", &bDisplayPathsDebug_IgnoreSpecial_PedAssistedMovement);
	bank.AddSeparator();
	bank.AddTitle("Ped cross reservations");
	bank.AddToggle("Display ped crossing reservation status", &bDisplayPathsDebug_PedCrossing_DrawReservationStatus);
	bank.AddToggle("Display ped crossing reservation slots", &bDisplayPathsDebug_PedCrossing_DrawReservationSlots);
	bank.AddToggle("Display ped crossing reservation grid", &bDisplayPathsDebug_PedCrossing_DrawReservationGrid);
	bank.AddSlider("Slot centroid fwd offset", &sm_fPedCrossReserveSlotsCentroidForwardOffset, 0.0f,3.0f, 0.1f);
	bank.AddSlider("Slot placement grid size", &sm_fPedCrossReserveSlotsPlacementGridSpacing, 0.0f,3.0f, 0.01f);
	bank.AddSeparator();
	bank.AddTitle("Error display");
	bank.AddToggle("Display alignment errors", &bDisplayPathsDebug_ShowAlignmentErrors);
	bank.AddSlider("Display alignment errors, point threshold", &fDisplayPathsDebug_AlignmentErrorPointOffsetThreshold, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("Display alignment errors, link threshold", &fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold, 0.0f, 1.0f, 0.01f);
	bank.PushGroup("Realtime Link Editing", false);
	bank.AddSlider("Debug Link Region", &DebugLinkRegion, 0, 65535, 1);
	bank.AddSlider("Debug Link Address", &DebugLinkAddress, 0, 65535, 1);
	bank.AddButton("Load Data for Specified Link", datCallback(MFA(CPathFind::LoadDebugLinkData), (datBase*)this));
	bank.AddButton("Save Data for Specified Link", datCallback(MFA(CPathFind::SaveDebugLinkData), (datBase*)this));
	bank.PushGroup("Link Data");
	bank.AddSlider("bGpsCanGoBothWays", &DebugLinkbGpsCanGoBothWays, 0, 1, 1);
	bank.AddSlider("Tilt", &DebugLinkTilt, 0, NUM_NODE_TILT_VALUES-1, 1);
	bank.AddSlider("NarrowRoad", &DebugLinkNarrowRoad, 0, 1, 1);
	bank.AddSlider("Width", &DebugLinkWidth, 0, 15, 1);
	bank.AddSlider("DontUseForNavigation", &DebugLinkDontUseForNavigation, 0, 1, 1);
	bank.AddSlider("bShortCut", &DebugLinkbShortCut, 0, 1, 1);
	bank.AddSlider("LanesFromOtherNode", &DebugLinkLanesFromOtherNode, 0, 7, 1);
	bank.AddSlider("LanesToOtherNode", &DebugLinkLanesToOtherNode, 0, 7, 1);
	bank.AddSlider("BlockIfNoLanes", &DebugLinkBlockIfNoLanes, 0, 1, 1);
	bank.AddSlider("Distance", &DebugLinkDistance, 0, 255, 1);
	bank.PopGroup();
	bank.PopGroup();
	bank.AddSeparator();
#if PATHSEARCHDRAW
	bank.AddTitle("Path Search Draw");
	PathSearchAddWidgets(bank);
	bank.AddSeparator();
#endif	// PATHSEARCHDRAW
	bank.AddTitle("Misc");
	bank.AddToggle("Display node switches(in world)", &bDisplayNodeSwitchesInWorld);
	bank.AddToggle("Display gps disabled zones(in world)", &bDisplayGpsDisabledRegionsInWorld);
	bank.AddToggle("Make all roads single track", &bMakeAllRoadsSingleTrack);
	bank.AddToggle("Display Directions", &bDisplayPathsDebug_Directions);
	bank.AddToggle("Test Generate Directions", &bDebug_TestGenerateDirections);
	bank.AddToggle("Display Path Streaming Debug", &bDisplayPathStreamingDebug);
	bank.AddToggle("Spew Pathfinding Query Progress to AI bank", &bSpewPathfindQueryProgress);
	bank.AddToggle("Test add pathnode required region at measuring tool pos", &bTestAddPathNodeRequiredRegionAtMeasuringToolPos);
#if __DEV
	bank.AddButton("Print Street Names", datCallback(&PrintStreetNames));
	//bank.AddButton("Find Dont wander here nodes", datCallback(&FindDontWanderNodes));
	bank.AddButton("Switch Off All Loaded Nodes", datCallback(&SwitchOffAllLoadedNodes));
	bank.AddButton("Reset All Loaded Nodes", datCallback(&ResetAllLoadedNodes));
	bank.AddButton("Output All Nodes", datCallback(&OutputAllNodes));
	bank.AddButton("Count junctions in region", datCallback(&CountNumberJunctionsInRegion));
	bank.AddButton("Print size of loaded regions", datCallback(&PrintSizeOfAllLoadedRegions));
	bank.AddButton("Resample Virtual Junction From World", datCallback(&ResampleVirtualJunctionFromWorld));
	bank.AddSlider("Region to count jns", &DebugCurrentRegion, 0, PATHFINDREGIONS-1, 1);
	bank.AddSlider("Junction node to resample", &DebugCurrentNode, 0, 10000, 1);
#endif
	bank.AddSlider("Debug GetRandomCarNode (num in batch)", &DebugGetRandomCarNode, 0, 10000, 1);
	bank.PopGroup();

#if __BANK
	bank.PushGroup("Paths on Vector Map", false);
	bank.AddToggle("Rotate VM with Player",								&CVectorMap::m_bRotateWithPlayer);
	bank.AddToggle("Rotate VM with Camera",								&CVectorMap::m_bRotateWithCamera);
	bank.AddToggle("Display vehicles (on VM)",							&CDebugScene::bDisplayVehiclesOnVMap);
	bank.AddToggle("Display vehicles uses fade (on VM)",				&CDebugScene::bDisplayVehiclesUsesFadeOnVMap);
	bank.AddToggle("Display path lines on VectorMap (dont force load)", &bDisplayPathsOnVMapDontForceLoad);
	bank.AddToggle("Display path lines with density (on VM)- dont force load", &bDisplayPathsWithDensityOnVMapDontForceLoad);
	bank.AddToggle("Display path lines (on VM)", &bDisplayPathsOnVMap);
	bank.AddToggle("Display path lines with density (on VM)", &bDisplayPathsWithDensityOnVMap);
	bank.AddToggle("Display only players road nodes (on VM)", &bDisplayOnlyPlayersRoadNodes);
	bank.AddToggle("Display network restart nodes", &bDisplayPathsDebug_Nodes_NetworkRestarts);
	bank.AddToggle("Display network restart nodes (on VM)", &bDisplayNetworkRestartsOnVMap);
	bank.AddToggle("Display path lines flashing for switchedOff nodes(on VM)", &bDisplayPathsOnVMapFlashingForSwitchedOffNodes);
	bank.AddToggle("Display path lines for tunnels in orange (on VM)", &bDisplayPathsForTunnelsOnVMapInOrange);
	bank.AddToggle("Display path lines for bad slopes in flashing purple (on VM)", &bDisplayPathsForBadSlopesOnVMapInFlashingPurple);
	bank.AddToggle("Display path lines for low slopes in flashing white (on VM)", &bDisplayPathsForLowPathsOnVMapInFlashingWhite);
	bank.AddToggle("Display node switches(on VM)", &bDisplayNodeSwitchesOnVMap);
	bank.AddToggle("Display gps disabled zones(on VM)", &bDisplayGpsDisabledRegionsOnVMap);
	bank.AddToggle("Display veh creation paths (on VM)",				&CDebugScene::bDisplayVehicleCreationPathsOnVMap);
	bank.AddToggle("Display veh creation paths curr density (on VM)",	&CDebugScene::bDisplayVehicleCreationPathsCurrDensityOnVMap);
	bank.PopGroup();

	bank.PushGroup("VehPop Events on Vector Map", false);
	bank.AddToggle("Display veh pop failed create events (on VM)",		&CDebugScene::bDisplayVehPopFailedCreateEventsOnVMap);
	bank.AddToggle("Display veh pop create events (on VM)",				&CDebugScene::bDisplayVehPopCreateEventsOnVMap);
	bank.AddToggle("Display veh pop destroy events (on VM)",			&CDebugScene::bDisplayVehPopDestroyEventsOnVMap);
	bank.AddToggle("Display veh pop conversion events (on VM)", 		&CDebugScene::bDisplayVehPopConversionEventsOnVMap);
	bank.PopGroup();
#endif

	bank.PushGroup("Individual Veh Ai", false);
	bank.AddToggle("Enable Navmesh vehicle nav", &CTaskVehicleCruiseNew::ms_bAllowCruiseTaskUseNavMesh);
	bank.AddToggle("Display Only for Player vehicle", &CVehicleIntelligence::m_bOnlyForPlayerVehicle);
	bank.AddToggle("Debug Focus Vehicle Only", &CVehicleIntelligence::ms_debugDisplayFocusVehOnly);
	bank.AddToggle("Display Car AI info", &CVehicleIntelligence::m_bDisplayCarAiDebugInfo);
	bank.AddSlider("Display vehicle AI info range", &CVehicleIntelligence::ms_fDisplayVehicleAIRange, 1.0f, 3000.0f, 10.0f);

	const char* VehicleTypes[] = 
	{
		"VEHICLE_TYPE_ALL",
		"VEHICLE_TYPE_CAR",
		"VEHICLE_TYPE_PLANE",
		"VEHICLE_TYPE_TRAILER",
		"VEHICLE_TYPE_QUADBIKE",
		"VEHICLE_TYPE_DRAFT",
		"VEHICLE_TYPE_SUBMARINECAR",
		"VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE",
		"VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE",
		"VEHICLE_TYPE_HELI",
		"VEHICLE_TYPE_BLIMP",
		"VEHICLE_TYPE_AUTOGYRO",
		"VEHICLE_TYPE_BIKE",
		"VEHICLE_TYPE_BICYCLE",
		"VEHICLE_TYPE_BOAT",
		"VEHICLE_TYPE_TRAIN",
		"VEHICLE_TYPE_SUBMARINE",
	};
	CompileTimeAssert(NELEM(VehicleTypes) == VEHICLE_TYPE_NUM + 1 ); // VEHICLE_TYPE_NONE == -1 so add one to compensate

	bank.AddCombo("Filter vehicle display by type",  &CVehicleIntelligence::m_eFilterVehicleDisplayByType, NELEM(VehicleTypes), VehicleTypes );



	bank.AddToggle("Display Addresses", &CVehicleIntelligence::m_bDisplayCarAddresses);
	bank.AddToggle("Display Vehicle Flags info", &CVehicleIntelligence::m_bDisplayVehicleFlagsInfo);
	bank.AddToggle("Display Car AI task info", &CVehicleIntelligence::m_bDisplayCarAiTaskInfo);
	bank.AddToggle("Display Car AI task details", &CVehicleIntelligence::m_bDisplayCarAiTaskDetails);
	bank.AddToggle("Display Car AI task History", &CVehicleIntelligence::m_bDisplayCarAiTaskHistory);
	bank.AddToggle("Display Car AI Future nodes", &CVehicleIntelligence::m_bDisplayDebugFutureNodes);
	bank.AddToggle("Display Car AI Future follow route", &CVehicleIntelligence::m_bDisplayDebugFutureFollowRoute);
	bank.AddToggle("Display Car AI status", &CVehicleIntelligence::m_bDisplayStatusInfo);
	bank.AddToggle("Display Car AI Drivable Extremes", &CVehicleIntelligence::m_bDisplayDrivableExtremes);
	bank.AddToggle("Display Car AI Task Destinations", &CVehicleIntelligence::m_bDisplayCarFinalDestinations);
	bank.AddToggle("Display Find coords and speed", &CVehicleIntelligence::m_bFindCoorsAndSpeed);
	bank.AddToggle("Display Vehicle recording info", &CVehicleIntelligence::m_bDisplayVehicleRecordingInfo);
	bank.AddToggle("Display Car Control info", &CVehicleIntelligence::m_bDisplayControlDebugInfo);
	bank.AddToggle("Display Car Transmission info", &CVehicleIntelligence::m_bDisplayControlTransmissionInfo);
	bank.AddToggle("Display low level AI Control info", &CVehicleIntelligence::m_bDisplayAILowLevelControlInfo);
	bank.AddToggle("Display Car Stuck info", &CVehicleIntelligence::m_bDisplayStuckDebugInfo);
	bank.AddToggle("Display abandoned removal debug", &CVehiclePopulation::ms_bDisplayAbandonedRemovalDebug);
	bank.AddToggle("Display AI Avoidance Tests", &CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_bEnableNewAvoidanceDebugDraw);
	bank.AddToggle("Display obstruction probe scores", &CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_bDisplayObstructionProbeScores);
	bank.AddToggle("Display avoidance road segments", &CTaskVehicleGoToPointWithAvoidanceAutomobile::ms_bDisplayAvoidanceRoadSegments);
	bank.AddToggle("Display AI Slow For Traffic", &CVehicleIntelligence::m_bDisplayDebugSlowForTraffic);
	bank.AddToggle("Display AI Dirty Flag Pilons", &CVehicleIntelligence::m_bDisplayDirtyCarPilons);
	bank.AddToggle("Display Slow down for bend info", &CVehicleIntelligence::m_bDisplaySlowDownForBendDebugInfo);
	bank.AddToggle("Display OkToDropOffPassengers", &CVehicleIntelligence::m_bDisplayDebugInfoOkToDropOffPassengers);
	bank.AddToggle("Display JoinWithRoadSystem", &CVehicleIntelligence::m_bDisplayDebugJoinWithRoadSystem);
	bank.AddToggle("Display Avoidance Cache", &CVehicleIntelligence::m_bDisplayAvoidanceCache);
	bank.AddToggle("Display Route Interception Tests", &CVehicleIntelligence::m_bDisplayRouteInterceptionTests);
	bank.AddToggle("Hide Failed Route Interception Tests", &CVehicleIntelligence::m_bHideFailedRouteInterceptionTests);
	bank.AddToggle("Display AI Car Behind", &CVehicleIntelligence::m_bDisplayDebugCarBehind);
	bank.AddToggle("Display AI Car Behind Search", &CVehicleIntelligence::m_bDisplayDebugCarBehindSearch);
	bank.AddToggle("Display AI Tailgate", &CVehicleIntelligence::m_bDisplayDebugTailgate);
	bank.AddToggle("Display Reckless driving debug", &CPlayerInfo::GetDisplayRecklessDriving());
	bank.AddToggle("Display AI car handling info", &CVehicleIntelligence::m_bAICarHandlingDetails);
	bank.AddToggle("Display AI car handling Curve", &CVehicleIntelligence::m_bAICarHandlingCurve);
	bank.AddToggle("Display engine temperature", &CVehicle::m_bDebugEngineTemperature);
	bank.AddToggle("Display wheel temperatures", &CVehicle::ms_bDebugWheelTemperatures);
	bank.AddToggle("Display wheel flags", &CVehicle::ms_bDebugWheelFlags);
	bank.AddToggle("Display vehicle health", &CVehicle::ms_bDebugVehicleHealth);
	bank.AddToggle("Display vehicle weapons", &CVehicle::ms_bDebugVehicleWeapons);
	bank.AddToggle("Find the car behind", &CVehicleIntelligence::m_bFindCarBehind);
	bank.AddToggle("Tailgate other cars", &CVehicleIntelligence::m_bTailgateOtherCars);
	bank.AddToggle("Use route to find car behind", &CVehicleIntelligence::m_bUseRouteToFindCarBehind);
	bank.AddToggle("Use scanner to find car behind", &CVehicleIntelligence::m_bUseScannerToFindCarBehind);
	bank.AddToggle("Use probes to find car behind", &CVehicleIntelligence::m_bUseProbesToFindCarBehind);
	bank.AddToggle("Use follow route for car behind", &CVehicleIntelligence::m_bUseFollowRouteToFindCarBehind);
	bank.AddToggle("Use node list for car behind", &CVehicleIntelligence::m_bUseNodeListToFindCarBehind);
	bank.AddToggle("Use new method for joining to road system", &CVehicleIntelligence::m_bEnableNewJoinToRoadInAnyDir);
	bank.AddToggle("Use bumper center probe during 3-point turns", &CVehicleIntelligence::m_bEnableThreePointTurnCenterProbe);
	bank.AddToggle("Display Interesting Drivers", &CVehicleIntelligence::m_bDisplayInterestingDrivers);
	bank.AddButton("Clear focus vehicle's nodes", ClearFocusVehicleNodes);
	bank.PopGroup();

	bank.PushGroup("PathFind Test Tool");
	bank.AddToggle("Active", &ThePaths.m_bTestToolActive);
	bank.AddButton("Set start pos", PathfindTestTool_SetStartPos);
	bank.AddButton("Set end pos", PathfindTestTool_SetEndPos);
	bank.AddButton("Find Path", PathfindTestTool_FindPath);

	bank.PushGroup("Path Params");
	bank.AddSlider("m_fCutoffDistForNodeSearch", &ThePaths.m_TestToolQuery.m_fCutoffDistForNodeSearch, 0.0f, 100.0f, 1.0f);
	bank.AddSlider("m_fMaxSearchDistance", &ThePaths.m_TestToolQuery.m_fMaxSearchDistance, 0.0f, 10000.0f, 1.0f);
	bank.AddToggle("m_bDontGoAgainstTraffic", &ThePaths.m_TestToolQuery.m_bDontGoAgainstTraffic);
	bank.AddToggle("m_bAmphibiousVehicle", &ThePaths.m_TestToolQuery.m_bAmphibiousVehicle);
	bank.AddToggle("m_bBoat", &ThePaths.m_TestToolQuery.m_bBoat);
	bank.AddToggle("m_bReducedList", &ThePaths.m_TestToolQuery.m_bReducedList);
	bank.AddToggle("m_bBlockedByNoGps", &ThePaths.m_TestToolQuery.m_bBlockedByNoGps);
	bank.AddToggle("m_bForGps", &ThePaths.m_TestToolQuery.m_bForGps);
	bank.AddToggle("m_bClearDistanceToTarget", &ThePaths.m_TestToolQuery.m_bClearDistanceToTarget);
	bank.AddToggle("m_bMayUseShortCutLinks", &ThePaths.m_TestToolQuery.m_bMayUseShortCutLinks);
	bank.AddToggle("m_bIgnoreSwitchedOffNodes", &ThePaths.m_TestToolQuery.m_bIgnoreSwitchedOffNodes);
	bank.PopGroup(); // path params

	bank.PopGroup(); // test tool


#if __DEV
	CVehicleIntelligence::AddBankWidgets(bank);
#endif

	CRoadSpeedZoneManager::InitWidgets(bank);
#endif // __BANK

	CSlownessZoneManager::InitWidgets(bank);
}

void CPathFind::LoadDebugLinkData()
{
	const CPathNodeLink* pLink = FindLinkPointerSafe(DebugLinkRegion, DebugLinkAddress);
	if (pLink)
	{
		DebugLinkbGpsCanGoBothWays = pLink->m_1.m_bGpsCanGoBothWays;
		DebugLinkTilt = pLink->m_1.m_Tilt;	
		DebugLinkTiltFalloff = pLink->m_1.m_TiltFalloff;
		DebugLinkNarrowRoad = pLink->m_1.m_NarrowRoad;						
		DebugLinkWidth = pLink->m_1.m_Width;							
		DebugLinkDontUseForNavigation = pLink->m_1.m_bDontUseForNavigation;						
		DebugLinkbShortCut = pLink->m_1.m_bShortCut;						
		DebugLinkLanesFromOtherNode = pLink->m_1.m_LanesFromOtherNode;
		DebugLinkLanesToOtherNode = pLink->m_1.m_LanesToOtherNode;				
		DebugLinkDistance = pLink->m_1.m_Distance;
		DebugLinkBlockIfNoLanes = pLink->m_1.m_bBlockIfNoLanes;
	}
	else
	{
		Assertf(0, "Couldn't load link!");
	}
}

void CPathFind::SaveDebugLinkData()
{
	CPathNodeLink* pLink = FindLinkPointerSafe(DebugLinkRegion, DebugLinkAddress);
	if (pLink)
	{
		pLink->m_1.m_bGpsCanGoBothWays = 						DebugLinkbGpsCanGoBothWays;	
		pLink->m_1.m_Tilt =										DebugLinkTilt;
		pLink->m_1.m_TiltFalloff =								DebugLinkTiltFalloff;
		pLink->m_1.m_NarrowRoad	=								DebugLinkNarrowRoad;		
		pLink->m_1.m_Width =									DebugLinkWidth;	
		pLink->m_1.m_bDontUseForNavigation =					DebugLinkDontUseForNavigation;	
		pLink->m_1.m_bShortCut =								DebugLinkbShortCut;	
		pLink->m_1.m_LanesFromOtherNode =						DebugLinkLanesFromOtherNode;
		pLink->m_1.m_LanesToOtherNode =							DebugLinkLanesToOtherNode;	
		pLink->m_1.m_Distance =									DebugLinkDistance;	
		pLink->m_1.m_bBlockIfNoLanes =							DebugLinkBlockIfNoLanes;					
	}
	else
	{
		Assertf(0, "Couldn't save link!");
	}
}

#endif // __BANK



