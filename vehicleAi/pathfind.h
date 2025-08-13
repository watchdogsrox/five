/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    Pathfind.h
// PURPOSE : Deals with building a grid for the high level path-finding and
//           with actually finding a route
// AUTHOR :  Obbe
// CREATED : 14-10-99
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef _PATHFIND_H_
#define _PATHFIND_H_

// Game headers
#include "debug/debug.h"
#include "pathserver/navgenparam.h"
#include "fwvehicleai/pathfindtypes.h"

// Rage headers
#include "data/bitfield.h"
#include "file/asset.h"
#include "file/stream.h"
#include "fwutil/Flags.h"
#include "vector/Vector3.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingmodulemgr.h"
#include "system/param.h"
#include "system/taskheader.h"
#include "phcore/materialmgr.h"

#define BUILDPATHSTREAMINGFILES_ONLY(x)

class CNodeRoute;
class CPointRoute;
class CRoutePoint;
class CVehicleNodeList;

#define PF_MAXNUMNODESPEROBJECT	(12)
#define PF_MAXLINKSPERNODE		(32)

enum {	GPS_SLOT_WAYPOINT = 0,
		GPS_SLOT_RADAR_BLIP,
		GPS_SLOT_DISCRETE,
		GPS_NUM_SLOTS
	};

#define PF_VERYLARGENUMBER		(32766)	// Just fits in s16

#define PF_HASHLISTENTRIES		(512)

#define STREAMING_PATH_NODES_DIST_PLAYER	(1765.0f)		// Path nodes are being streamed in this distance around the player.
#define STREAMING_PATH_NODES_DIST			(300.0f)		// Path nodes are being streamed in this distance around an ai dude.
#define STREAMING_PATH_NODES_DIST_ASYNC		(150.0f)		// 

// Every time the level designers switch a bunch of node on or off an extra one of the following structures is stored.
// The idea is that every time a block of nodes is streamed in the appropriate node switches are made.
#define MAX_NUM_NODE_SWITCHES	(100)

#define DEFAULT_REJECT_JOIN_TO_ROAD_DIST	(60.0f)

#define ROAD_EDGE_BUFFER_FOR_CAMBER_CALCULATION (0.37f)
#define LINK_TILT_FALLOFF_WIDTH (3.0f)
#define LINK_TILT_FALLOFF_HEIGHT (0.1f)

struct	CNodesSwitchedOnOrOff
{
	CNodesSwitchedOnOrOff() {}
	CNodesSwitchedOnOrOff(const float minX, const float maxX, const float minY, const float maxY, const float minZ, const float maxZ, const bool bSwitchOff)	: bAxisAlignedSwitch(true), scriptThreadId(0), bOff(bSwitchOff), bOnlyForDurationOfMission(false)
	{
		AA.Set(minX,maxX,minY,maxY,minZ,maxZ);
	}
	CNodesSwitchedOnOrOff(const Vector3 & vStart, const Vector3 & vEnd, const float Width, const bool bSwitchOff)	: bAxisAlignedSwitch(false), scriptThreadId(0), bOff(bSwitchOff), bOnlyForDurationOfMission(false)
	{
		Angled.Set(vStart,vEnd,Width);
	}

	bool Equals(const CNodesSwitchedOnOrOff& other) const
	{
		if (scriptThreadId != other.scriptThreadId
			|| bOff != other.bOff
			|| bOnlyForDurationOfMission != other.bOnlyForDurationOfMission
			|| bAxisAlignedSwitch != other.bAxisAlignedSwitch)
		{
			return false;
		}

		if (bAxisAlignedSwitch)
		{
			if (!AreNearlyEqual(AA.MinX, other.AA.MinX)
				|| !AreNearlyEqual(AA.MaxX, other.AA.MaxX)
				|| !AreNearlyEqual(AA.MinY, other.AA.MinY)
				|| !AreNearlyEqual(AA.MaxY, other.AA.MaxY)
				|| !AreNearlyEqual(AA.MinZ, other.AA.MinZ)
				|| !AreNearlyEqual(AA.MaxZ, other.AA.MaxZ))
			{
				return false;
			}
		}
		else
		{
			if (!AreNearlyEqual(Angled.AreaWidth, other.Angled.AreaWidth)
				|| !AreNearlyEqual(Angled.vAreaStart.x, other.Angled.vAreaStart.x)
				|| !AreNearlyEqual(Angled.vAreaStart.y, other.Angled.vAreaStart.y)
				|| !AreNearlyEqual(Angled.vAreaStart.z, other.Angled.vAreaStart.z)
				|| !AreNearlyEqual(Angled.vAreaEnd.x, other.Angled.vAreaEnd.x)
				|| !AreNearlyEqual(Angled.vAreaEnd.y, other.Angled.vAreaEnd.y)
				|| !AreNearlyEqual(Angled.vAreaEnd.z, other.Angled.vAreaEnd.z))
			{
				return false;
			}
		}

		return true;
	}
	union {
		struct {
			void Set(const float minX, const float maxX, const float minY, const float maxY, const float minZ, const float maxZ) {
				MinX = minX; MaxX = maxX; MinY = minY; MaxY = maxY; MinZ = minZ; MaxZ = maxZ;
			}
			float	MinX, MaxX, MinY, MaxY, MinZ, MaxZ;
		} AA;
		struct {
			void Set(const Vector3 & vStart, const Vector3 & vEnd, const float Width) {
				vAreaStart.x = vStart.x;
				vAreaStart.y = vStart.y;
				vAreaStart.z = vStart.z;
				vAreaEnd.x = vEnd.x;
				vAreaEnd.y = vEnd.y;
				vAreaEnd.z = vEnd.z;
				AreaWidth = Width;
			}
			struct {
				float x;
				float y;
				float z;
				const Vector3 Get() const { return Vector3(x,y,z); }
			} vAreaStart, vAreaEnd;
			float AreaWidth;
		} Angled;
	};
	u32	scriptThreadId;					// Used to remove the node switches in the mission cleanup
	bool	bOff;						// True if nodes are switched on. False otherwise.
	bool	bOnlyForDurationOfMission;	// If this is true this switch will be removed in the mission cleanup
	bool	bAxisAlignedSwitch;
};

extern bool bDontBuildPaths;

#define	MAX_NUM_NODE_REQUESTS	(GPS_NUM_SLOTS+2)		// First the 2 gps ones and then the script request.
#define	NODE_REQUEST_FIRST_GPS	(0)
#define	NODE_REQUEST_SCRIPT		(GPS_NUM_SLOTS)
#define NODE_REQUEST_BLOCKING_BACKUP (GPS_NUM_SLOTS+1)

enum GPSFlags
{
	GPS_FLAG_IGNORE_ONE_WAY					=	0x01,
	GPS_FLAG_FOLLOW_RULES					=	0x02,
	GPS_FLAG_AVOID_HIGHWAY					=	0x04,
	GPS_FLAG_NO_ROUTE_SHIFT					=	0x08,
	GPS_FLAG_CUSTOM_PROXIMITY				=	0x10,
	GPS_FLAG_NO_PULL_PATH_TO_RIGHT_LANE		=	0x20,
	GPS_FLAG_AVOID_OFF_ROAD					=	0x40,
	GPS_FLAG_IGNORE_DESTINATION_Z			=	0x80,
};

#define	NUM_NODE_GROUPS_FOR_NETWORK_RESTART_NODES	(32)

#define	FIND_REGION_INDEX( A , B ) A + ( B * PATHFINDMAPSPLIT)
#define	FIND_X_FROM_REGION_INDEX( A ) (A % PATHFINDMAPSPLIT)
#define	FIND_Y_FROM_REGION_INDEX( A ) (((s32)A) / PATHFINDMAPSPLIT)

extern CNodeAddress EmptyNodeAddress;	// This is here to pass into DoPathSearch function.

#define NUM_NODE_TILT_VALUES (29)
extern float saTiltValues[NUM_NODE_TILT_VALUES];

typedef bool (*FindClosestNodeCB) (CPathNode * pPathNode, void * data);
typedef bool (*ForAllNodesCB) (CPathNode * pPathNode, void * data);

/////////////////////////////////////////////////////////////////////////////////
// This is a temporary structure we use to store the traffic lights until
// we're ready to process them into the path nodes.
/////////////////////////////////////////////////////////////////////////////////
#if __DEV
struct CStoredTrafficLight
{
	Vector3	pos;
	Vector3	a, b;
	u32	mi;
};
#endif // __DEV

struct CAssistedRouteInfo
{
	CPathNode * pEndNode;
	int iDistSqr;
	int iNumEndsFound;
};

//------------------------------------------------------------------------------
// CLASS : CPathfindFloodFillBinHeap
// PURPOSE : Binary heap used to store pathnodes during floodfill operations

struct TPathfindFloodItem
{
	TPathfindFloodItem() { }
	TPathfindFloodItem(CPathNode * pNode, const Vector3 & vNormal) { m_pPathNode = pNode; m_vNormal = vNormal; }
	CPathNode * m_pPathNode;
	Vector3 m_vNormal;
};

/////////////////////////////////////////////////////////////////////////////////
// This is the main struct containing the information needed for the path finding
// on the map.
/////////////////////////////////////////////////////////////////////////////////
class CPathFind : public strStreamingModule
{
public:
	enum { RORC_VERSION = 0 };

	typedef enum
	{
		IgnoreSwitchedOffNodes = 0,
		PreferSwitchedOnNodes,
		IncludeSwitchedOffNodes
	} NodeConsiderationType;

	enum GpsDirections
	{	// Be careful when adding new instructions, make sure to keep ConvertPathDirectionToInstruction() in sync
		DIRECTIONS_UNKNOWN = 0,
		DIRECTIONS_WRONG_WAY,
		DIRECTIONS_KEEP_DRIVING,
		DIRECTIONS_LEFT_AT_JUNCTION,
		DIRECTIONS_RIGHT_AT_JUNCTION,
		DIRECTIONS_STRAIGHT_THROUGH_JUNCTION,		
		DIRECTIONS_KEEP_LEFT,
		DIRECTIONS_KEEP_RIGHT,
		DIRECTIONS_UTURN,
		DIRECTIONS_MAX
	};

	// CLASS : PathQuery
	// PURPOSE : This should replace the huge list of parameters passed in to DoPathSearch
	// We may also wish to process requests asynchronously, in which case these will be maintained in a queue
	class PathQuery
	{
	public:
		Vector3 m_vStartCoords;
		CNodeAddress m_StartNode;
		Vector3 m_vTargetCoords;
		CNodeAddress * m_pNodeList;
		s32 * m_pNumNodesGiven;
		s32 m_iNumNodesRequested;
		float * m_pDistance;
		float m_fCutoffDistForNodeSearch;
		CNodeAddress * m_pGivenTargetNode;
		float m_fMaxSearchDistance;
		CNodeAddress m_NodeToAvoid;
		bool m_bDontGoAgainstTraffic;
		bool m_bAmphibiousVehicle;
		bool m_bBoat;
		bool m_bReducedList;
		bool m_bBlockedByNoGps;
		bool m_bForGps;
		bool m_bClearDistanceToTarget;
		bool m_bMayUseShortCutLinks;
		bool m_bIgnoreSwitchedOffNodes;

		bool m_bPathFound;
		s32 m_iSearch;
		float m_fTotalProcessingTime;
	};

	class CFindPedNodeParams
	{
	public:

		static const int MAX_NUM_SPECIAL_FUNCS = 2;

		CFindPedNodeParams(const Vector3& vSearchPosition, float fCutoffDistance, CNodeAddress* pPrevStartAddr, CNodeAddress* pPrevEndAddr)
			: m_vSearchPosition(vSearchPosition)
			, m_fCutoffDistance(fCutoffDistance)
			, m_pPrevStartAddr(pPrevStartAddr)
			, m_pPrevEndAddr(pPrevEndAddr)
		{

		}

		Vector3 m_vSearchPosition;
		float m_fCutoffDistance;
		CNodeAddress* m_pPrevStartAddr;
		CNodeAddress* m_pPrevEndAddr;
		atFixedArray<u32, MAX_NUM_SPECIAL_FUNCS> m_SpecialFunctions;
	};

	class CPathNodeRequiredArea
	{
	public:

		enum Context
		{
			CONTEXT_SCRIPT,
			CONTEXT_GAME,
			CONTEXT_MAP
		};

		static const u32 MAX_MAP_SLOTS			= 4;
		static const u32 MAX_SCRIPT_SLOTS		= 12;
		static const u32 MAX_GAME_SLOTS			= 20;	
		static const u32 TOTAL_SLOTS			= MAX_MAP_SLOTS + MAX_SCRIPT_SLOTS + MAX_GAME_SLOTS;

		static s32 m_iNumActiveAreas;
		static s32 m_iNumMapAreas;
		static s32 m_iNumScriptAreas;
		static s32 m_iNumGameAreas;

		Vector3 m_vMin;
		Vector3 m_vMax;

		s32 m_iContext;

#if __BANK
		char m_Description[32];
#endif
	};

#if __BANK
	static const s32 ms_iMaxPathHistory = 16;
	s32 m_iLastPathHistory;
	s32 m_iLastSearch;
	PathQuery m_PathHistory[ms_iMaxPathHistory];

	bool m_bTestToolActive;
	PathQuery m_TestToolQuery;
	CNodeAddress m_TestToolNodes[512];
	s32 m_iNumTestToolNodes;

	void UpdateTestTool();
#endif // __BANK

	enum { MAX_GPS_DISABLED_ZONES = 4 };
	struct GPSDisabledZone
	{
		// Variables to allow scripts to disable the GPS functionality from going through a certain area.
		u32				m_iGpsBlockedByScriptID;
		s32				m_iGpsBlockingRegionMinX;
		s32				m_iGpsBlockingRegionMaxX;
		s32				m_iGpsBlockingRegionMinY;
		s32				m_iGpsBlockingRegionMaxY;

		void Clear()
		{
			m_iGpsBlockedByScriptID  = 0;
			m_iGpsBlockingRegionMinX = 0;
			m_iGpsBlockingRegionMinY = 0;
			m_iGpsBlockingRegionMaxX = 0;
			m_iGpsBlockingRegionMaxY = 0;
		}
	};

	CPathFind();
#if !__FINAL
	virtual const char* GetName(strLocalIndex index) const;
#endif // !__FINAL
	virtual strLocalIndex Register(const char* name);
	virtual strLocalIndex FindSlot(const char* name) const;
	virtual void Remove(strLocalIndex index);
	virtual void RemoveSlot(strLocalIndex index);
	virtual void PlaceResource(strLocalIndex index, datResourceMap& map, datResourceInfo& header);
	virtual bool Load(strLocalIndex index, void* object, int size);
	virtual void* GetPtr(strLocalIndex index);

	strLocalIndex GetStreamingIndexForRegion(u32 iRegion) const;
	u32 GetRegionForStreamingIndex(strLocalIndex iIndex) const;

	void	Init(unsigned initMode);

	static void RegisterStreamingModule();
	static void DetermineValidPathNodeFiles();

	void	Shutdown(unsigned shutdownMode);
	int		GetNumRefs(strLocalIndex UNUSED_PARAM(index)) const {return 0;}

	void	SetPlayerSwitchTarget(const Vector3& vSwitchTarget) {m_vPlayerSwitchTarget = vSwitchTarget;}
	void	ResetPlayerSwitchTarget() {m_vPlayerSwitchTarget.Zero();}

	void	CountScriptMemoryUsage(u32& nVirtualSize, u32& nPhysicalSize);

	void	Update(void);

#if __BANK
	void	InitWidgets();
#endif

	void DoPathSearch(
		const Vector3& StartCoors,
		CNodeAddress StartNode,
		const Vector3& TargetCoors,
		CNodeAddress *pNodeList,
		s32 *pNumNodesGiven,
		s32 NumNodesRequested,
		float *pDistance = NULL,
		float CutoffDistForNodeSearch = 999999.9f,
		CNodeAddress *pGivenTargetNode = NULL,
		int MaxSearchDistance = 999999,
		bool bDontGoAgainstTraffic = false,
		CNodeAddress NodeToAvoid = EmptyNodeAddress,
		bool bAmphibiousVehicle = false,
		bool bBoat = false,
		bool bReducedList = false,
		bool bBlockedByNoGps = false,
		bool bForGps = false,
		bool bClearDistanceToTarget = true,
		bool bMayUseShortCutLinks = false,
		bool bIgnoreSwitchedOffNodes = false,
		bool bFollowTrafficRules = false,
		const CVehicleNodeList* pPreferredNodeListForDestination = NULL,
		bool bGpsAvoidHighway = false,
		float *pfLaneOffsetList = NULL,
		float *pfLaneWidthList = NULL,
		bool bAvoidRestrictedAreas = false,
		bool bAvoidOffroad = false);

	void	ClearAllNodesDistanceToTarget(const bool* regionsToClear = NULL);
#if __ASSERT
	void	VerifyAllNodesDistanceToTarget();
#endif // __ASSERT

	bool	FindAndFixOddJunction(int iStartNode, int nNodes, Vector3* pNodes);
	bool	FindWiggle(int iStartNode, int nNodes, Vector3* pNodes, int& iWiggleStart, int& iWiggleEnd) const;
	void	SmoothWiggle(Vector3* pNodes, int iWiggleStart, int iWiggleEnd);
	float	GenerateRoutePointsForGps(CNodeAddress destNodeAddress, const Vector3& DestinationCoors, CEntity * pTargetEntity, Vector3 *pResultArray, CNodeAddress* pNodeAdress, u16 *pNodeInfoArray, s16 *pNodeDistanceToTarget, s32 maxPoints, s32 &numPoints, bool bIgnoreDestZ, const u32 iGpsFlags, bool& searchIgnoredOneWay, bool async = false, bool bReduceMaxSearch = false, bool bIgnoreNoGps = false);
	float	GenerateRoutePointsForGps(const Vector3& srcCoors, const Vector3& DestinationCoors, const Vector3& srcDirFlat, const Vector3& targetDirFlat, Vector3 *pResultArray, CNodeAddress* pNodeAdress, u16 *pNodeInfoArray, s16 *pNodeDistanceToTarget, s32 maxPoints, s32 &numPoints, bool bIgnoreZ, const u32 iGpsFlags, bool& searchIgnoredOneWay, bool async = false, bool bReduceMaxSearch = false, bool bIgnoreNoGps = false);
	float	GenerateRoutePointsForGps(CNodeAddress srcNodeAddress, CNodeAddress destNodeAddress, const Vector3& srcCoors, const Vector3& destCoors, const Vector3& srcDirFlat, const Vector3 & targetDirFlat, Vector3 *pResultArray, CNodeAddress* pNodeAdress, u16 *pNodeInfoArray, s16 *pNodeDistanceToTarget, s32 maxPoints, s32 &numPoints, bool bIgnoreDestZ, const u32 iGpsFlags, bool& searchIgnoredOneWay, bool async = false, bool bReduceMaxSearch = false, bool bIgnoreNoGps = false);
	CPathNode * FindRouteEndNodeForGpsSearch(const Vector3 & vPos, const Vector3 & vDir, const bool bIgnoreZ, Vector3 * pvClosestPosOnLinks=NULL, bool bIgnoreNoGps = false);
	CPathNode * FindRouteEndNodeForGpsSearchAsync(const Vector3 & vPos, const Vector3 & vDir, const bool bIgnoreZ, Vector3 * pvClosestPosOnLinks=NULL, u32 searchSlot = GPS_ASYNC_SEARCH_PERIODIC, bool bIgnoreNoGps = false);

	bool	IsAsyncSearchActive(u32 searchSlot, Vector3& vSearchCoors);
	bool	IsAsyncSearchComplete(u32 searchSlot);
	void	ClearGPSSearchSlot(u32 searchSlot = GPS_ASYNC_SEARCH_PERIODIC);

	void	GenerateRoutePointsForGpsRaceTrack(Vector3 *pResultArray, u16 *pNodeInfoArray, s16 *pNodeDistanceToTarget, s32 maxPoints, s32 &numPoints, const u32 iGpsFlags);

	// To be removed
	void	GenerateDirectionsLegacy(const Vector3& DestinationCoors, s32 &dir_out, u32 &streetNameHash_out);

	u32		GenerateDirections(const Vector3& vDestinationCoors, s32 &iOut_Directions, u32 &iOut_StreetNameHash, float & fOut_DistanceToTurn, Vector3& vOutPos);
	u32		GenerateDirections(s32 &iOut_Directions, u32 &iOut_StreetNameHash, float & fOut_DistanceToTurn, CNodeAddress* pNodeAdress, int NumNodes, Vector3& vOutPos);

	bool	IsJunctionForGenerateDirections(const CRoutePoint * pPrevPrev, const CRoutePoint * pPrev, const CRoutePoint * pNode, const CRoutePoint * pNext, const CRoutePoint * pNextNext, 
		float & fOut_DotTurn, float & fOut_DotSmoothTurn, int & iOut_TurnDir, bool & bIsGenuineJunction, 
		Vector3 & vEntryPos, Vector3 & vEntryVec, Vector3 & vExitPos, Vector3 & vExitVec, bool bIgnoreFirstLink, bool bIgnoreSecondLink);

	void FloodFillFromPosition(
		const Vector3 & vPosition,
		CNodeAddress startNode,
		bool (*OnVisitNode)(CPathNode*),
		bool (*ShouldVisitAdjacentNode)(const CPathNode*,const CPathNodeLink&,const CPathNode*,float,float&,const Vector3&,Vector3&),
		const float fStartNodeCost);

	s32 MarkPlayersRoadNodes(const Vector3 & vStartPosition, const CNodeAddress startNode, const float fMaxRange, const Vector3 & vSearchDirection, CPathNode ** pOut_PlayersRoadNodes, const s32 iMaxNumNodesToMark, const CPathNode **pPlayerJunctionNode, const CPathNode **pPlayerJunctionEntranceNode);

	s32	CountNonSpecialNeighbours(const CPathNode *pNode) const;

	void	IdentifyRoadBlocks(const Vector3& StartCoors, CNodeAddress *pResult, s32 *pNeighbourTowardsPlayer, s32 *pNumCarsNeeded , s32 &roadBlocksFound, float minDist, float maxDist, s32 numRequested);

	float	CalculateTravelDistanceBetweenNodes(float Point1X, float Point1Y, float Point1Z, float Point2X, float Point2Y, float Point2Z);

	void	RemoveBadStartNode(const Vector3& StartCoors, class CVehicleIntelligence *pAutoPilot, s32 *pNumNodes, s32 testNode);

	float	CalculateFitnessValueForNode(const Vector3& Pos, const Vector3& Dir, const CPathNode* pNode) const;

#if __BANK
	void	RenderDebug();
	void	RenderDebugShowAlignmentErrorsOnSegment(Vec3V_In vNodeA, Vec3V_In vNodeB, Vec3V_In vLinkSide, float fFalloffWidth, float fFalloffHeight) const;
	void	RenderDebugGroundHit(Vec3V_In vTestPos, float fGroundHeight) const;
	void	RenderDebugOffsetError(Vec3V_In vTestPos, float fGroundHeight) const;
	void	DisplayPathHistory() const;
	void	DebugDrawNodesRequiredRegions() const;
#endif // __DEV

	float	CalcDistToAnyConnectingLinks(const CPathNode *pNode, const Vector3& Coors) const;

	void	FindNodeClosestInRegion(CNodeAddress *pNodeFound, u32 Region, const Vector3& SearchCoors, float *pClosestDist
		, NodeConsiderationType eNodeConsiderationType, bool bIgnoreAlreadyFound, bool bBoatNodes, bool bHidingNodes
		, bool bNetworkRestart, bool bMatchDirection, float dirToMatchX, float dirToMatchY, float zMeasureMult, float zTolerance
		, s32 mapAreaIndex, bool bForGps, bool bIgnoreOneWayRoads, const float fCutoffDistForNodeSearch=0.0f
		, const CVehicleNodeList* pPreferredNodeList = NULL, const bool bIgnoreSlipLanes = false
		, const bool bIgnoreSwitchedOffDeadEnds=false) const;

	void	FindPedNodeClosestInRegion(CNodeAddress *out_pNodeFound, float *inout_pClosestDist, u32 Region, const CFindPedNodeParams& searchParams) const;

	void	FindNodeClosestInRegion(CNodeAddress * pNodeFound, u32 Region, const Vector3 & SearchCoors, float * pClosestDist
		, FindClosestNodeCB callbackFunction, void * pData=NULL, const float fCutoffDistForNodeSearch=0.0f, bool bForGps = false) const;

	void	ForAllNodesInArea(const Vector3 & vMin, const Vector3 & vMax, ForAllNodesCB callbackFunction, void * pData=NULL);

	CNodeAddress	FindNodeClosestToCoors(const Vector3& SearchCoors, float CutoffDistForNodeSearch = 999999.9f
		, NodeConsiderationType eNodeConsiderationType = IncludeSwitchedOffNodes, bool bIgnoreAlreadyFound = false
		, bool bBoatNodes = false, bool bHidingNodes = false, bool bNetworkRestart = false, bool bMatchDirection = false
		, float dirToMatchX = 0.0f, float dirToMatchY = 0.0f, float zMeasureMult = 3.0f, float zTolerance = 0.0f, s32 mapAreaIndex = -1
		, bool bForGps = false, bool bIgnoreOneWayRoads = false, const CVehicleNodeList* pPreferredNodeList = NULL
		, const bool bIgnoreSlipLanes = false, const bool bIgnoreSwitchedOffDeadEnds=false) const;

	CNodeAddress	FindNthNodeClosestToCoors(const Vector3& SearchCoors, float CutoffDistForNodeSearch = 999999.9f
		, bool bIgnoreSwitchedOff = false, s32 N = 0, bool bBoatNodes = false, CNodeAddress *pNMinus1th = NULL
		, bool bMatchDirection = false, float dirToMatchX = 0.0f, float dirToMatchY = 0.0f 
		, float zMeasureMult = 3.0f, float zTolerance = 0.0f, s32 mapAreaIndex = -1, const bool bIgnoreSlipLanes = false
		, const bool bIgnoreSwitchedOffDeadEnds=false);

	CNodeAddress	FindNextNodeClosestToCoors(const Vector3& SearchCoors, float CutoffDistForNodeSearch = 999999.9f
		, bool bIgnoreSwitchedOff = false, bool bBoatNodes = false, bool bMatchDirection = false
		, float dirToMatchX = 0.0f, float dirToMatchY = 0.0f, float zMeasureMult = 3.0f, float zTolerance = 0.0f, s32 mapAreaIndex = -1);

	CNodeAddress	FindPedNodeClosestToCoors(const CFindPedNodeParams& searchParams) const;

	CNodeAddress	FindNodeClosestToCoors(const Vector3& SearchCoors, FindClosestNodeCB callbackFunction, void * pData = NULL, float CutoffDistForNodeSearch = 999999.9f, bool bForGps = false) const;

	CNodeAddress	FindNodeClosestToCoorsAsync(const Vector3& SearchCoors, FindClosestNodeCB callbackFunction, void * pData = NULL, float CutoffDistForNodeSearch = 999999.9f, bool bForGps = false);


	class TFindClosestNodeLinkGPS
	{
	public:

		Vector3 vSearchOrigin;
		Vector3 vSearchDir;
		Vector3 vClosestPointOnNearbyLinks;
		float fDistToClosestPointOnNearbyLinks;
		float fDistZPenalty;
		float fHeadingPenalty;

		CPathNode * pBestNodeSoFar;
		CNodeAddress resultNodeAddress;

		bool bIgnoreOneWay;
		bool bIgnoreHeading;
		bool bIgnoreJunctions;
		bool bIgnoreNoNav;
		bool bIgnoreNoGps;
		bool bIgnoreWater;

		void Reset()
		{			
			vSearchOrigin = VEC3_ZERO;
			vSearchDir = VEC3_ZERO;
			vClosestPointOnNearbyLinks = VEC3_ZERO;
			fDistToClosestPointOnNearbyLinks = FLT_MAX;
			pBestNodeSoFar = NULL;
			bIgnoreOneWay = false;
			bIgnoreHeading = false;
			bIgnoreJunctions = false;
			bIgnoreNoNav = false;
			bIgnoreNoGps = false;
			bIgnoreWater = true;
			fDistZPenalty = 1.0f;
			fHeadingPenalty = 0.0f;
		}
	};

	struct FindNodeClosestToCoorsAsyncParams{
		Vector3 m_SearchCoors;
		FindClosestNodeCB m_CallbackFunction;
		TFindClosestNodeLinkGPS* m_SearchData;
		float m_CutoffDistance;
		bool m_bForGPS;
		sysTaskHandle m_TaskHandle;
		s32 m_CallingGPSSlot;
		bool m_TaskComplete;

		GPSDisabledZone m_gpsDisabledZones[MAX_GPS_DISABLED_ZONES];

		float m_fZMultNone;
		float m_fZMultNormal;
		float m_fZMultHigher;
		float m_fPreferBothWayProximitySqrXY;
		float m_fPreferBothWayMaxZ;
		float m_fSearchDist;
		float m_fHeadingPenalty;
		float m_fFitnessThreshold;
		float m_fTwoLinkBonusFitness;

		FindNodeClosestToCoorsAsyncParams()
		{
			m_SearchCoors = VEC3_ZERO;
			m_CallbackFunction = NULL;
			m_SearchData = NULL;
			m_CutoffDistance = 0.f;
			m_bForGPS = false;
			m_TaskHandle = 0;
			m_CallingGPSSlot = -1;
			m_TaskComplete = false;

			for(int i = 0; i < MAX_GPS_DISABLED_ZONES; ++i)
			{
				m_gpsDisabledZones[i].Clear();
			}

			m_fZMultNone = 0.f;
			m_fZMultNormal = 0.f;
			m_fZMultHigher = 0.f;
			m_fPreferBothWayProximitySqrXY = 0.f;
			m_fPreferBothWayMaxZ = 0.f;
			m_fSearchDist = 0.f;
			m_fHeadingPenalty = 0.f;
			m_fFitnessThreshold = 0.f;
			m_fTwoLinkBonusFitness = 0.f;
		}		
	};

	void	StartAsyncGPSSearch(FindNodeClosestToCoorsAsyncParams* params);

	int		FindAssistedMovementRoutesInArea(const Vector3 & vOrigin, const float fMaxDist, CPathNode ** pUniqueRouteNodes, int iMaxNumNodes);

	bool	GetPositionBySideOfRoad(const Vector3 & vNodePosition, const float fHeading, Vector3 & vPositionByRoad);
	bool	GetPositionBySideOfRoad(const Vector3 & vNodePosition, const s32 iDirection, Vector3 & vPositionByRoad);

	bool	SnapCoorsToNearestNode(const Vector3& SearchCoors, Vector3& Result, float CutoffDist);

	void 	Find2NodesForCarCreation(const Vector3& SearchCoors, CNodeAddress* ClosestNode, CNodeAddress* OtherNode, bool bIgnoreSwitchedOff);

	void	RecordNodesClosestToCoors(const Vector3& SearchCoors, s32 NoOfNodes, CNodeAddress* ClosestNodes, float CutoffDistSq = FLT_MAX, bool bIgnoreSwitchedOff = false, bool bBoatNodes = false);
	void	RecordNodesClosestToCoors(const Vector3& SearchCoors, u16 Region, s32 NoOfNodes, CNodeAddress* ClosestNodes, float* ClosestNodesDistSq, float& CutoffDistSq, bool bIgnoreSwitchedOff, bool bWaterNode);

	bool 	These2NodesAreAdjacent(CNodeAddress Node1, CNodeAddress Node2) const;

	static float	FindNearestGroundHeightForBuildPaths(const Vector3 &pos, float range, u32* roomId, phMaterialMgr::Id * pMaterialIdOut = NULL);

	s16		FindRegionLinkIndexBetween2Nodes(CNodeAddress NodeFrom, CNodeAddress NodeTo) const;
	s16		FindNodesLinkIndexBetween2Nodes(const CNodeAddress iNodeFrom, const CNodeAddress iNodeTo) const;
	bool	FindNodesLinkIndexBetween2Nodes(const CPathNode * pNodeFrom, const CPathNode * pNodeTo, s16 & iLinkIndex) const;
	bool	FindNodesLinkIndexBetween2Nodes(const CNodeAddress & iNodeFrom, const CNodeAddress & iNodeTo, s16 & iLinkIndex) const;
	const CPathNodeLink * FindLinkBetween2Nodes(const CPathNode * pNodeFrom, const CPathNode * pNodeTo) const;

	void	FindCarGenerationCoordinatesBetween2Nodes(CNodeAddress nodeFrom, CNodeAddress nodeTo, Matrix34 *pMatrix) const;

	s32		FindNumberNonShortcutLinksForNode(const CPathNode* pNode) const;

	CNodeAddress FindRandomNodeNearKnownOne(const CNodeAddress oldNode, const s32 steps, const CNodeAddress avoidNode = EmptyNodeAddress);

	s32   RecordNodesInCircle(const Vector3& Centre, const float Radius, s32 NoOfNodes, CNodeAddress* Nodes, bool bIgnoreSwitchedOff = false, bool bIgnoreAlreadyFound = false, bool bBoatNodes = false);
	s32	  RecordNodesInCircleFacingCentre(const Vector3& Centre, const float MinRadius, const float MaxRadius, s32 NoOfNodes, CNodeAddress* Nodes, bool bIgnoreSwitchedOff, bool bHighwaysOnly, bool treatSlipLanesAsHighways, bool localRegionOnly, bool searchUpOnly, u32& totalNodesInMaxRadius);

	CNodeAddress	FindNodeClosestToCoorsFavourDirection(const Vector3& SearchCoors, float DirX, float DirY, const bool bIncludeSwitchedOffNodes = true);
	
	struct FindNodeClosestToNodeFavorDirectionInput
	{
		FindNodeClosestToNodeFavorDirectionInput(CNodeAddress node, const Vector2& vDirection)
		: m_Node(node)
		, m_Exception()
		, m_vDirection(vDirection)
		, m_fMaxRandomVariance(0.0f)
		, m_bCanFollowOutgoingLinks(true)
		, m_bCanFollowIncomingLinks(false)
		, m_bIncludeSwitchedOffNodes(false)
		, m_bUseOriginalSwitchedOffValue(false)
		, m_bIncludeDeadEndNodes(false)
		{
		
		}
		
		CNodeAddress	m_Node;
		CNodeAddress	m_Exception;
		Vector2			m_vDirection;
		float			m_fMaxRandomVariance;
		bool			m_bCanFollowOutgoingLinks;
		bool			m_bCanFollowIncomingLinks;
		bool			m_bIncludeSwitchedOffNodes;
		bool			m_bUseOriginalSwitchedOffValue;
		bool			m_bIncludeDeadEndNodes;
	};
	struct FindNodeClosestToNodeFavorDirectionOutput
	{
		FindNodeClosestToNodeFavorDirectionOutput()
		: m_Node()
		, m_fDistance(0.0f)
		{
		
		}
		
		CNodeAddress	m_Node;
		float			m_fDistance;
	};
	bool	FindNodeClosestToNodeFavorDirection(const FindNodeClosestToNodeFavorDirectionInput& rInput, FindNodeClosestToNodeFavorDirectionOutput& rOutput);

	float	CalcDirectionMatchValue(const CPathNode *pNode, float DirX, float DirY, float defaultMatch = 0.0f) const;

	s32		FindLaneForVehicleWithLink(const CNodeAddress firstNode, const CNodeAddress secondNode, const CPathNodeLink& rLink, const Vector3& position) const;

	bool	FindNodePairToMatchVehicle(const Vector3& position, const Vector3& forward, CNodeAddress &nodeFrom, CNodeAddress &nodeTo, bool bWaterNodes, s16 &regionLinkIndex_out, s8 &nearestLane, const float fRejectDist=DEFAULT_REJECT_JOIN_TO_ROAD_DIST, const bool bIgnoreDirection = false, CNodeAddress hintNodeFrom = EmptyNodeAddress, CNodeAddress hintNodeTo = EmptyNodeAddress);

	void	FindNodePairToMatchVehicleInRegion(int regionIndex, Vec3V_ConstRef position, Vec3V_ConstRef forward, bool bWaterNodes, float fRejectDist, bool bIgnoreDirection, float& smallestErrorMeasureInOut, CNodeAddress &nodeFromInOut, CNodeAddress &nodeToInOut, s16 &regionLinkIndexInOut, s8 &nearestLaneInOut, int specificNodeToCheck, int specificLinkIndexToCheck);

	void	FindNodePairClosestToCoors(const Vector3& SearchCoors, CNodeAddress *pNode1, CNodeAddress *pNode2, float *pOrientation, float MinDistApart, float CutoffDistForNodeSearch, bool bIgnoreSwitchedOff, bool bWaterNode, bool bHighwaysOnly = false, bool bSearchUpFromPosition = false, s32 RequiredLanes = 0, s32 *pLanesToNode2 = NULL, s32 *pLanesFromNode2 = NULL, float *pCentralReservation = NULL, bool localRegionOnly = false);

	void	FindStartAndEndNodeBasedOnYRange(s32 Region, float searchMinY, float searchMaxY, s32 &startNode, s32 &endNode) const;

	void	FindNodePairWithLineClosestToCoors(const Vector3& SearchCoors, CNodeAddress *pNode1, CNodeAddress *pNode2, float *pOrientation, float MinDistApart, float CutoffDistForSearch, bool bIgnoreSwitchedOff, bool bWaterNode, s32 RequiredLanes = 0, s32 *pLanesToNode2 = NULL, s32 *pLanesFromNode2 = NULL, float *pCentralReservation = NULL, Vector3 *pClosestCoorsOnLine = NULL, const Vector3 *pForward = NULL, float orientationPenaltyMult = 0.0f);

	float	FindNodeOrientationForCarPlacement(CNodeAddress CarNode, float DirX=0.0f, float DirY=0.0f, int *pNumLanes = NULL);

	int		FindBestLinkFromHeading(CNodeAddress CarNode, float heading);

	bool	IsNodeNearbyInZ(const Vector3& SearchCoors) const;

	inline	s32	FindHashValue(s32 Dist) const { return (Dist & (PF_HASHLISTENTRIES-1)); };

	void	RemoveNodeFromList(CPathNode *pNode);

	void	AddNodeToList(CPathNode *pNode, int distToTarget, int distForHash);

	void	FindStreetNameAtPosition(const Vector3& SearchCoors, u32 &streetName1Hash, u32 &streetName2Hash ) const;
#if !(RSG_ORBIS || RSG_PC || RSG_DURANGO)
	void	FindStreetNameAtPosition(const Vector3& SearchCoors, u16 Region, u32 &streetName1Hash, u32 &streetName2Hash, float& streetName1DistSq, float& streetName2DistSq ) const;
#endif //!(RSG_ORBIS || RSG_PC || RSG_DURANGO)

	bool	FindTrailingLinkWithinDistance(CNodeAddress StartNode, const Vector3& vStartPos);

	void	SwitchRoadsOffInArea(const CNodesSwitchedOnOrOff & area, bool bCars = true, bool bBackToOriginal = false);

	void	SwitchRoadsOffInArea(u32	scriptThreadId, float MinX, float MaxX, float MinY, float MaxY, float MinZ, float MaxZ, bool SwitchRoadsOff, bool bCars = true, bool bBackToOriginal = false, bool bOnlyForDurationOfMission = false);

	void	SwitchRoadsOffInAngledArea(u32	scriptThreadId, Vector3& vAreaStart, Vector3& vAreaEnd, float AreaWidth, bool SwitchRoadsOff, bool bCars = true, bool bBackToOriginal = false, bool bOnlyForDurationOfMission = false);

	void	SetGpsBlockingRegionForScript( const u32 iScriptId, const Vector3 & vMin, const Vector3 & vMax, s32 iIndex );

	void	ClearGpsBlockingRegionForScript( s32 index) { Assert(index >=0 && index < MAX_GPS_DISABLED_ZONES); m_GPSDisabledZones[index].m_iGpsBlockedByScriptID = 0; }

	s32		GetNextAvailableGpsBlockingRegionForScript();

	void	TidyUpNodeSwitchesAfterMission(u32 scriptThreadId);

	void	SwitchRoadsOffInAreaForOneRegion(const CNodesSwitchedOnOrOff & node, s32 Region, bool bBackToOriginal = false);

	void 	SwitchOffNodeAndNeighbours(CPathNode *pNode, CPathNode **ppNextNode1, CPathNode **ppNextNode2, bool bWhatToSwitchTo, bool bBackToOriginal);

	s32		CountNeighboursToBeSwitchedOff(const CPathNode *pNode) const;

	bool	ThisNodeHasToBeSwitchedOff(const CPathNode *pNode) const;


	static s32	FindRegionForCoors(float X, float Y);
	s32	FindXRegionForCoors(float X) const;
	s32	FindYRegionForCoors(float Y) const;
#if __DEV
	float	FindFloatXRegionForCoors(float X) const;
	float	FindFloatYRegionForCoors(float Y) const;
#endif // __DEV
	float	FindXCoorsForRegion(s32 Region) const;
	float	FindYCoorsForRegion(s32 Region) const;

	void 	FindStartPointOfRegion(s32 RegionX, s32 RegionY, float& ResultX, float& ResultY) const;

	Vector3 FindNodeCoorsForScript(CNodeAddress Node, bool *pSuccess = NULL) const;
	Vector3 FindNodeCoorsForScript(CNodeAddress CurrentNode, CNodeAddress NewNode, float *pOrientation, bool *pSuccess = NULL) const;

	void	RegisterNodeUsage(CNodeAddress nodeAddr);
	bool	HasNodeBeenUsedRecently(CNodeAddress nodeAddr) const;
	void	UpdateUsedNodes();

	//-----------------------------------------------------------------------------------------

	bool	AddNodesRequiredRegionThisFrame(const s32 iContext, const Vector3 & vMin, const Vector3 & vMax, const char * pDescription);
	bool	ClearNodesRequiredRegions();

	//-----------------------------------------------------------------------------------------

	inline bool				IsRegionLoaded(const s32 r) const
	{
		if(Verifyf(r>=0 && r<PATHFINDREGIONS, "Region %d out of bounds", r))
        {
		    return (apRegions[r] && apRegions[r]->aNodes);
        }
        else
        {
#if !__FINAL
            Quitf("Checking for out of bounds region to be loaded! Checking:%d Max:%d", r, PATHFINDREGIONS);
#endif // !__FINAL
            return false;
        }
	};
	inline bool				IsRegionLoaded(const CNodeAddress Node) const
	{
		Assert(!Node.IsEmpty());
		const u32 iRegion = Node.GetRegion();
		
        if(Verifyf(iRegion<PATHFINDREGIONS, "Region %d out of bounds", iRegion))
        {
		    return (apRegions[iRegion] && apRegions[iRegion]->aNodes);
        }
        else
        {
#if !__FINAL
            Quitf("Checking for out of bounds region to be loaded! Checking:%d Max:%d", iRegion, PATHFINDREGIONS);
#endif // !__FINAL
            return false;
        }
	};
	inline CPathNode*		FindNodePointer(CNodeAddress Node) { Assert(IsRegionLoaded(Node)); return (&(apRegions[Node.GetRegion()]->aNodes)[Node.GetIndex()] ); };
	inline const CPathNode*	FindNodePointer(const CNodeAddress Node) const { Assert(IsRegionLoaded(Node)); return (&(apRegions[Node.GetRegion()]->aNodes)[Node.GetIndex()] ); };

	inline CPathNode*		FindNodePointerSafe(CNodeAddress Node) 
    {
        if(Node.IsEmpty() || !IsRegionLoaded(Node))
        {
            return NULL;
        }
        else if(Node.GetIndex() >= apRegions[Node.GetRegion()]->NumNodes)
        {
            Assertf(0, "Trying to find path node pointer with an out of bounds index! Region:%d, Index:%d, Num region nodes:%d", Node.GetRegion(), Node.GetIndex(), apRegions[Node.GetRegion()]->NumNodes);
            return NULL;
        }
        else
        {
            CPathNode *pathNode = (&(apRegions[Node.GetRegion()]->aNodes)[Node.GetIndex()] );

            if(Verifyf(pathNode->GetAddrRegion() == Node.GetRegion(), "Path node region inconsistency detected! NodeAddressRegion:%d, PathNodeRegion:%d", pathNode->GetAddrRegion(), Node.GetRegion()))
            {
                return pathNode;
            }
            else
            {
#if !__FINAL
				CheckPathNodeIntegrityForZone(Node.GetRegion());
				TestAllLinks();
                Quitf("Path node region inconsistency detected! NodeAddressRegion:%d, PathNodeRegion:%d", pathNode->GetAddrRegion(), Node.GetRegion());
#endif // !__FINAL
                return NULL; 
            }
        }
    };

	inline const CPathNode*	FindNodePointerSafe(const CNodeAddress Node) const 
    { 
        if(Node.IsEmpty() || !IsRegionLoaded(Node))
        {
            return NULL;
        }
        else if(Node.GetIndex() >= apRegions[Node.GetRegion()]->NumNodes)
        {
            Assertf(0, "Trying to find path node pointer with an out of bounds index! Region:%d, Index:%d, Num region nodes:%d", Node.GetRegion(), Node.GetIndex(), apRegions[Node.GetRegion()]->NumNodes);
            return NULL;
        }
        else
        {
            CPathNode *pathNode = (&(apRegions[Node.GetRegion()]->aNodes)[Node.GetIndex()] );

            if(Verifyf(pathNode->GetAddrRegion() == Node.GetRegion(), "Path node region inconsistency detected! NodeAddressRegion:%d, PathNodeRegion:%d", pathNode->GetAddrRegion(), Node.GetRegion()))
            {
                return pathNode;
            }
            else
			{
#if !__FINAL
				CheckPathNodeIntegrityForZone(Node.GetRegion());
				TestAllLinks();
                Quitf("Path node region inconsistency detected! NodeAddressRegion:%d, PathNodeRegion:%d", pathNode->GetAddrRegion(), Node.GetRegion());
#endif // !__FINAL
                return NULL; 
            }
        }
    };

	void	MarkRegionsForCoors(const Vector3& Coors, float Range, const bool bPossiblyRejectIfPrologueNodes=false);
	//	void	SetPathsNeededAtPosition(const Vector3& posn);
	void	UpdateStreaming(bool bForceLoad = false, bool bBlockingLoad = false);
	void 	MakeRequestForNodesToBeLoaded(float MinX, float MaxX, float MinY, float MaxY, s32 requestIndex, bool bBlockingLoad = false);
	void	ReleaseRequestedNodes(s32 requestIndex);
	bool	HaveRequestedNodesBeenLoaded(s32 requestIndex);
	void	LoadSceneForPathNodes(const Vector3& CenterCoors);

	Vector3 FindParkingNodeInArea(float MinX, float MaxX, float MinY, float MaxY, float MinZ = -10000.0f, float MaxZ = 10000.0f) const;

	bool	AreNodesLoadedForArea(float MinX, float MaxX, float MinY, float MaxY) const;
	bool	AreNodesLoadedForRadius(const Vector3& vPos, float fRadius) const;
	bool	AreNodesLoadedForPoint(float testX, float testY) const;

	bool	IsWaterNodeNearby(const Vector3& Coors, float Range) const;

	bool CollisionTriangleIntersectsRoad(const Vector3 * pPts, const bool bIncludeSwitchedOff);

	inline void FindRoadBoundaries(const CPathNodeLink & rLink, float & fWidthLeftOut, float & fWidthRightOut)
	{
		bool bAllLanesThoughCentre = BANK_ONLY(bMakeAllRoadsSingleTrack ||) (rLink.m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);
		FindRoadBoundaries(rLink.m_1.m_LanesToOtherNode, rLink.m_1.m_LanesFromOtherNode, static_cast<float>(rLink.m_1.m_Width), rLink.m_1.m_NarrowRoad, bAllLanesThoughCentre, fWidthLeftOut, fWidthRightOut);
	}
	void FindRoadBoundaries(s32 lanesTo, s32 lanesFrom, float centralReservationWidth, bool bNarrowRoad, bool bAllLanesThoughCentre, float& roadWidthOnLeft_out, float& roadWidthOnRight_out) const;
	void IdentifyDrivableExtremes(CNodeAddress nodeAAddr, CNodeAddress nodeBAddr, CNodeAddress nodeCAddr, bool nodeBIsJunction, float vehHalfWidthToUse, Vector3 &drivableExtremePointLeft_out, Vector3 &drivableExtremePointRight_out, float& widthScalingFactor_out) const;
	bool GetBoundarySegmentsBetweenNodes(const CNodeAddress fromNode, const CNodeAddress toNode, const CNodeAddress nextNode, bool bClampToCenterOfRoad, Vector3& vLeftEnd, Vector3& vRightEnd, Vector3& vLeftStartPos, Vector3& vRightStartPos, const float fVehicleHalfWidthToUse = 0.0f);
	bool DoesVehicleThinkItsOffRoad(const CVehicle* pVehicle, const bool bReverse = false, const CVehicleNodeList* pNodeList = NULL);
	bool IsPositionOffRoadGivenNodelist(const CVehicle* pVehicle, Vec3V_In vTestPosition
		, const CVehicleNodeList* pNodeList = NULL, const float fHalfWidth=0.0f
		, const int iSearchStartNode = 0, int iSearchEndNode = -1);
	static bool HelperIsPointWithinArbitraryArea(float TestPointX, float TestPointY,
		float Point1X, float Point1Y,
		float Point2X, float Point2Y,
		float Point3X, float Point3Y,
		float Point4X, float Point4Y);

	static bool IsVehicleTypeWhichCanStreamInNodes(const CVehicle* pVehicle);
	static bool IsMissionVehicleThatCanStreamInNodes(const CVehicle* pVehicle);

	//enum SpawnNodeFlags { SF_OrderPointsAwayFromPlayers = 1, SF_OrderPointsWithTeamMates = 2, SF_RecalculateCentreAroundTeamMates = 4, SF_SortByZDistance = 8, SF_OrderPointsFurtherAwayFromFriendlyPlayers = 16};

	// FindNodeWithIndex can be removed once Chris R is not using it anymore.
	CNodeAddress FindNodeWithIndex(const s32 index, bool bIgnoreSwitchedOff, bool bIgnoreAlreadyFound, bool bBoatNodes, bool bHidingNodes, bool bNetworkRestart);
	bool	GetRandomCarNode(Vector3 &centrePoint, float radius, bool waterNodesInstead, s32 minLanes, bool bAvoidDeadEnds, bool bAvoidHighways, bool bAvoidSwitchedOff, Vector3 &vecReturn, CNodeAddress &returnNodeAddr);
	void	GetSpawnCoordinatesForCarNode(CNodeAddress nodeAddr, Vector3 &towardsPos, Vector3 &vecReturn, float &orientation);

	void	FindBestNodePair(const Vector3& vTestPos, const Vector3& vForward, CNodeAddress& BestOldNode, CNodeAddress& BestNewNode, const float fSearchProximity);

	s32 GetSlipLaneNodeLinkIndexNew(const CPathNode * pCurrNode);
	u32 FindDirectionOfUpcomingTurn(const CNodeAddress& PrevNode, const CPathNode* pCurrNode, const CNodeAddress& NextNode);

	static int GetNegativeTiltIndex(int i) { CompileTimeAssert(NUM_NODE_TILT_VALUES==29); return (i ? ISelectI(GenerateMaskLT(i,15),i-14,i+14) : 0); }

	bool WasRegionSkippedByPrologueNodeRejection(const s32 RegionX, const s32 RegionY) const;

	bool IsRegionInPrologueMap(const s32 RegionX, const s32 RegionY) const;

	static bool IsNodePositionInPrologueMap(const Vector3& vPos);

#if !__FINAL
	void CheckPathNodeIntegrityForZone(s32 Region) const;
	void TestAllLinks() const;
	//	void	LoadAllNodesForDebug();
#endif // !__FINAL

	struct FindNodeInDirectionInput
	{
		FindNodeInDirectionInput(CNodeAddress node, const Vector3& vInitialDirection, const float fMinDistance)
		: m_Node(node)
		, m_NodeToFind()
		, m_vInitialPosition(VEC3_ZERO)
		, m_vInitialDirection(vInitialDirection)
		, m_fMinDistance(fMinDistance)
		, m_fMaxDistanceTraveled(400.0f)
		, m_fMaxRandomVariance(0.0f)
		, m_uMaxLinks(0)
		, m_bFollowRoad(false)
		, m_bIncludeSwitchedOffNodes(false)
		, m_bDoNotIncludeSwitchedOffNodesAfterSwitchedOnEncountered(false)
		, m_bUseOriginalSwitchedOffValue(false)
		, m_bIncludeDeadEndNodes(false)
		, m_bDoNotIncludeDeadEndNodesAfterNonDeadEndEncountered(false)
		, m_bCanFollowOutgoingLinks(true)
		, m_bCanFollowIncomingLinks(false)
#if __BANK
		, m_bRender(false)
#endif
		{
		
		}
		
		FindNodeInDirectionInput(const Vector3& vInitialPosition, const Vector3& vInitialDirection, const float fMinDistance)
		: m_Node()
		, m_NodeToFind()
		, m_vInitialPosition(vInitialPosition)
		, m_vInitialDirection(vInitialDirection)
		, m_fMinDistance(fMinDistance)
		, m_fMaxDistanceTraveled(400.0f)
		, m_fMaxRandomVariance(0.0f)
		, m_uMaxLinks(0)
		, m_bFollowRoad(false)
		, m_bIncludeSwitchedOffNodes(false)
		, m_bDoNotIncludeSwitchedOffNodesAfterSwitchedOnEncountered(false)
		, m_bUseOriginalSwitchedOffValue(false)
		, m_bIncludeDeadEndNodes(false)
		, m_bDoNotIncludeDeadEndNodesAfterNonDeadEndEncountered(false)
		, m_bCanFollowOutgoingLinks(true)
		, m_bCanFollowOutgoingLinksFromDeadEndNodes(false)
		, m_bCanFollowIncomingLinks(false)
#if __BANK
		, m_bRender(false)
#endif
		{
		
		}
		
		CNodeAddress	m_Node;
		CNodeAddress	m_NodeToFind;
		Vector3			m_vInitialPosition;
		Vector3			m_vInitialDirection;
		float			m_fMinDistance;
		float			m_fMaxDistanceTraveled;
		float			m_fMaxRandomVariance;
		u32				m_uMaxLinks;
		bool			m_bFollowRoad;
		bool			m_bIncludeSwitchedOffNodes;
		bool			m_bDoNotIncludeSwitchedOffNodesAfterSwitchedOnEncountered;
		bool			m_bUseOriginalSwitchedOffValue;
		bool			m_bIncludeDeadEndNodes;
		bool			m_bDoNotIncludeDeadEndNodesAfterNonDeadEndEncountered;
		bool			m_bCanFollowOutgoingLinks;
		bool			m_bCanFollowOutgoingLinksFromDeadEndNodes;
		bool			m_bCanFollowIncomingLinks;

#if __BANK
		bool			m_bRender;
#endif

	};
	
	struct FindNodeInDirectionOutput
	{
		CNodeAddress	m_Node;
		CNodeAddress	m_PreviousNode;
	};

	bool	FindNodeInDirection(const FindNodeInDirectionInput& rInput, FindNodeInDirectionOutput& rOutput);

	inline CPathNodeLink * FindLinkPointerSafe(const s32 iRegion, const s32 iLink) const
	{
		Assert(iRegion >= 0 && iRegion < PATHFINDREGIONS);		
		if(apRegions && apRegions[iRegion] && apRegions[iRegion]->aLinks)
		{
			Assertf(iLink >= 0 && iLink < apRegions[iRegion]->NumLinks, "Link %i is out of range for region (0 to %i)", iLink, apRegions[iRegion]->NumLinks);
			if(iLink >= 0 && iLink < apRegions[iRegion]->NumLinks)
			{
				return &apRegions[iRegion]->aLinks[iLink];
			}
		}
		return NULL;
	}
	inline s32 GetNodesRegionLinkIndex(const CPathNode* pNode, const s32 iLink) const
	{
		Assert(iLink >= 0 && iLink < (s32)pNode->NumLinks());
		return pNode->m_startIndexOfLinks + iLink;
	}
	inline CPathNodeLink& GetNodesLink(const CPathNode* pNode, const s32 iLink)
	{
		Assert(iLink >= 0 && iLink < (s32)pNode->NumLinks());
		return apRegions[pNode->GetAddrRegion()]->aLinks[pNode->m_startIndexOfLinks + iLink];
	}
	inline const CPathNodeLink& GetNodesLink(const CPathNode* pNode, const s32 iLink) const
	{
		Assert(iLink >= 0 && iLink < (s32)pNode->NumLinks());
		return apRegions[pNode->GetAddrRegion()]->aLinks[pNode->m_startIndexOfLinks + iLink];
	}
	inline CNodeAddress GetNodesLinkedNodeAddr(const CPathNode* pNode, const s32 iLink) const
	{
		Assert(iLink >= 0 && iLink < (s32)pNode->NumLinks());
		return apRegions[pNode->GetAddrRegion()]->aLinks[pNode->m_startIndexOfLinks + iLink].m_OtherNode;
	}
	inline CPathNode* GetNodesLinkedNode(const CPathNode* pNode, const s32 iLink)
	{
		Assert(iLink >= 0 && iLink < (s32)pNode->NumLinks());
		CNodeAddress linkedNodeAddr = GetNodesLinkedNodeAddr(pNode, iLink);
		if(!apRegions[linkedNodeAddr.GetRegion()]){return NULL;}
		else{return FindNodePointer(linkedNodeAddr);}
	}
	inline float GetLinksLaneCentreOffset(const CPathNodeLink& rLink, s32 laneIndex) const
	{
#if __BANK
		return (bMakeAllRoadsSingleTrack?0.0f:rLink.InitialLaneCenterOffset()) + (laneIndex * rLink.GetLaneWidth());
#else
		return rLink.InitialLaneCenterOffset() + (laneIndex * rLink.GetLaneWidth());
#endif // _DEV
	}

	Vector3 GetLinkLaneOffset(const CNodeAddress& rNode, const CPathNodeLink& rLink, s32 laneIndex) const
	{
		Assert(IsRegionLoaded(rNode));
		Assert(IsRegionLoaded(rLink.m_OtherNode));

		const CPathNode* pNode = FindNodePointer(rNode);
		const CPathNode* pOtherNode = FindNodePointer(rLink.m_OtherNode);

		Assert(FindLinkBetween2Nodes(pNode, pOtherNode));

		const float fLaneCenterOffset = GetLinksLaneCentreOffset(rLink, laneIndex);

		Vector2 vNode;
		Vector2 vOtherNode;

		pNode->GetCoors2(vNode);
		pOtherNode->GetCoors2(vOtherNode);

		Vector2 vDir = vOtherNode - vNode;
		vDir.Normalize();

		const Vector3 vRight(vDir.y, -vDir.x, 0.0f);

		return vRight * fLaneCenterOffset;
	}

	//--------------------------------------------------------------------------------
	
	// System for ped road crossing start position reservations
	// We will define a number of Slots for a crossing node with a cross direction towards the end node.
	// This helps avoid peds clumping up when waiting to cross a road.
	// We also now assign randomized delays for the peds to begin crossing, with minimum time spread.

	// Is a reservation available for the given crossing node addresses?
	bool IsPedCrossReservationAvailable(const CNodeAddress& startAddress, const CNodeAddress& endAddress) const;

	// Make a reservation for the given crossing node addresses
	// Returns success or failure and sets out variable for reservation slot on success
	bool MakePedCrossReservation(const CNodeAddress& startAddress, const CNodeAddress& endAddress, int& outReservationSlot, int& outAssignedCrossDelayMS);

	// Release the reservation for the given crossing node addresses and slot
	void ReleasePedCrossReservation(const CNodeAddress& startAddress, const CNodeAddress& endAddress, const int& reservationSlot);

	// Compute the proper crossing start position for the given crossing reservation
	// Returns true on success and false on failure
	bool ComputePositionForPedCrossReservation(const CNodeAddress& startAddress, const CNodeAddress& endAddress, const int& reservationSlot, Vector3& outPosition) const;

	// Helper method to find the index of a matching entry for the given node addresses
	// Returns -1 if no match is found
	int FindPedCrossReservationIndex(const CNodeAddress& startAddress, const CNodeAddress& endAddress) const;

	// Helper method to find the index of an entry available for use
	// Returns -1 if no entries are available
	int FindAvailablePedCrossReservationIndex() const;

	// Helper method to set the first available list item to use the given node addresses
	// Returns the index of the newly modified entry if successful
	// Returns -1 if the list is already full
	int AddPedCrossReservation(const CNodeAddress& startAddress, const CNodeAddress& endAddress);

	//
	// A reference centroid will be centered on the start node and projected forward in the crossing direction.
	// We will center a row of square grid cells on the centroid and randomize positions within cells, ensuring spacing
	//
	static bank_float sm_fPedCrossReserveSlotsCentroidForwardOffset;
	static bank_float sm_fPedCrossReserveSlotsPlacementGridSpacing;

private:

	enum { MAX_PED_CROSS_RESERVATION_SLOTS = 3 };
	enum { MAX_PED_CROSSING_RESERVATIONS = 32 };
	struct PedCrossingStartPosReservation
	{
		// The crossing is from a start node to an end node.
		CNodeAddress m_StartNodeAddress;
		CNodeAddress m_EndNodeAddress;
		// Randomized cross delay times with minimum spread enforced
		int m_AssignedCrossDelayMS[MAX_PED_CROSS_RESERVATION_SLOTS];
		// Randomized slot positions.
		float m_ReservationSlotForwardOffset[MAX_PED_CROSS_RESERVATION_SLOTS];
		float m_ReservationSlotRightOffset[MAX_PED_CROSS_RESERVATION_SLOTS];
		// Reservation slot status flags.
		// If a slot is reserved its bit is set, otherwise cleared.
		fwFlags8 m_ReservationSlotStatus;
	};

	// list of reservations
	PedCrossingStartPosReservation m_aPedCrossingReservations[MAX_PED_CROSSING_RESERVATIONS];

	// Helper method to roll randomized crossing delays
	// NOTE: This is to enforce a minimum spread among peds departing the same crossing start
	void ComputeRandomizedCrossDelays(PedCrossingStartPosReservation& outReservation) const;

	// Helper method to generate randomized crossing slot positions
	// NOTE: This is to provide a more organic grouping of peds waiting to cross while
	// still helping to keep the peds spaced apart.
	void ComputeRandomizedCrossPositions(PedCrossingStartPosReservation& outReservation) const;

public:
	//--------------------------------------------------------------------------------

	enum { PF_MAXNUMNODES_INSEARCH = 5000 };	// What is the maximum number of nodes we can expect to be involved in a single search/
	enum { PF_MAXNUMTEMPNODES = 10000 };		// Temp nodes used for building the node lists
	enum { PF_MAXNUMDEBUGMARKERS = 100 };		// How many debug markers can we have
	enum { ANFLAG_CROSSESROAD = (1<<0), ANFLAG_TRAFFICLIGHT = (1<<1) };
	static const float sm_fSlipLaneDotThreshold;
	static const float sm_fAboveThresholdForPathProbes;
	static const u32 sm_iHeistsOffset;
	static const u32 sm_iHeistsSize;

	u8			ColourGroups[2];	// For cars and peds how many colour groups there are
	CPathNode		*apHashTable[PF_HASHLISTENTRIES];

	// The stuff that deals with the streaming of the pathfind code lives here.
	// the world is split up in 8x8 regions. The regions can be loaded up / dumped as they are needed.
	CPathRegion* apRegions[PATHFINDREGIONS + MAXINTERIORS] ;

	//if there is a switch going on, this will be the dest coordinates--otherwise zero
	Vector3		m_vPlayerSwitchTarget;

	//s32			NodesOnListDuringPathfinding;

	// Stuff that deals with switching nodes on/off by the scripts.
	s32			NumNodeSwitches;		// How many are there.
	CNodesSwitchedOnOrOff	NodeSwitches[MAX_NUM_NODE_SWITCHES];

	// Array of areas requested for loading by script/tasks/map
	CPathNodeRequiredArea m_PathNodeRequiredAreas[CPathNodeRequiredArea::TOTAL_SLOTS];

	// For Generate directions
	CNodeAddress m_LastValidDirectionsNode;
	int m_iDirection;
	int m_iStreetNameHash;

	//Stuff that deals with disabling the gps for zones set by script
	GPSDisabledZone m_GPSDisabledZones[MAX_GPS_DISABLED_ZONES];

	// Stuff that deals with the script requesting the nodes in certain areas to be loaded
	bool			bActiveRequestForRegions[MAX_NUM_NODE_REQUESTS];
	float			RequestMinX[MAX_NUM_NODE_REQUESTS], RequestMaxX[MAX_NUM_NODE_REQUESTS], RequestMinY[MAX_NUM_NODE_REQUESTS], RequestMaxY[MAX_NUM_NODE_REQUESTS];
	bool			bLoadAllRegions;
	bool			bWillTryAndStreamPrologueNodes;
	bool			bStreamHeistIslandNodes;
	bool			bWasStreamingHeistIslandNodes;

	// Stuff that deals with node usage. Some nodes (like parking nodes) can be 'used'
	// After they have been used they should not be used for a while by other cars.
	// The following little array keeps track of the recently used nodes.
	enum { PF_NUMUSEDNODES = 32 };
	CNodeAddress	m_aUsedNodes[PF_NUMUSEDNODES];
	u32			m_aTimeUsed[PF_NUMUSEDNODES];

	// Stuff having to do with looking for network restart points belonging to specific groups
	u8			aNodeGroupsForCurrentSearch[NUM_NODE_GROUPS_FOR_NETWORK_RESTART_NODES];
	u8			aNodeGroupsList[NUM_NODE_GROUPS_FOR_NETWORK_RESTART_NODES];

	bool			bIgnoreNoGpsFlag;
	bool			bIgnoreNoGpsFlagUntilFirstNode;

#if __BANK
	bool			bDisplayPathStreamingDebug;
	bool			bDisplayPathHistory;
	bool			bDisplayRequiredNodeRegions;
	bool			bDisplayRegionBoxes;
	bool			bDisplayMultipliers;
	bool			bDisplayPathsDebug_Allow;
	float			fDebgDrawDistFlat;
	bool			bDisplayPathsDebug_RoadMesh_AllowDebugDraw;
	float			fDebugAlphaFringePortion;
	bool			bDisplayPathsDebug_ShowAlignmentErrors;
	int				iDisplayPathsDebug_AlignmentErrorSideSamples;
	float			fDisplayPathsDebug_AlignmentErrorPointOffsetThreshold;
	float			fDisplayPathsDebug_AlignmentErrorLinkOffsetThreshold;
	bool			bDisplayPathsDebug_Nodes_AllowDebugDraw;
	bool			bDisplayPathsDebug_Nodes_StandardInfo;
	bool			bDisplayPathsDebug_Nodes_Pilons;
	bool			bDisplayPathsDebug_Nodes_ToCollisionDiff;
	bool			bDisplayPathsDebug_Nodes_StreetNames;
	bool			bDisplayPathsDebug_Nodes_DistanceHash;
	bool			bDisplayPathsDebug_Nodes_NetworkRestarts;
	bool			bDisplayPathsDebug_Links_AllowDebugDraw;
	bool			bDisplayPathsDebug_Links_DrawLine;
	bool			bDisplayPathsDebug_Links_DrawLaneDirectionArrows;
	bool			bDisplayPathsDebug_Links_DrawTrafficLigths;
	bool			bDisplayPathsDebug_PedCrossing_DrawReservationStatus;
	bool			bDisplayPathsDebug_PedCrossing_DrawReservationSlots;
	bool			bDisplayPathsDebug_PedCrossing_DrawReservationGrid;
	bool			bDisplayPathsDebug_Links_TextInfo;
	bool			bDisplayPathsDebug_Links_LaneCenters;
	bool			bDisplayPathsDebug_Links_RoadExtremes;
	bool			bDisplayPathsDebug_Links_Tilt;
	bool			bDisplayPathsDebug_Links_RegionAndIndex;
	bool			bDisplayPathsDebug_IgnoreSpecial_PedCrossing;
	bool			bDisplayPathsDebug_OnlySpecial_PedCrossing;
	bool			bDisplayPathsDebug_IgnoreSpecial_PedDriveWayCrossing;
	bool			bDisplayPathsDebug_OnlySpecial_PedDriveWayCrossing;
	bool			bDisplayPathsDebug_IgnoreSpecial_PedAssistedMovement;
	bool			bMakeAllRoadsSingleTrack;
	bool			bDisplayPathsOnVMapFlashingForSwitchedOffNodes;
	bool			bDisplayPathsDebug_Directions;
	bool			bDebug_TestGenerateDirections;
	bool			bDisplayPathsOnVMapDontForceLoad;
	bool			bDisplayPathsWithDensityOnVMapDontForceLoad;
	bool			bDisplayPathsOnVMap;
	bool			bDisplayOnlyPlayersRoadNodes;
	bool			bDisplayPathsWithDensityOnVMap;
	bool			bDisplayNetworkRestartsOnVMap;
	bool			bDisplayPathsForTunnelsOnVMapInOrange;
	bool			bDisplayPathsForBadSlopesOnVMapInFlashingPurple;
	bool			bDisplayPathsForLowPathsOnVMapInFlashingWhite;
	bool			bDisplayNodeSwitchesOnVMap;
	bool			bDisplayNodeSwitchesInWorld;
	bool			bDisplayGpsDisabledRegionsOnVMap;
	bool			bDisplayGpsDisabledRegionsInWorld;
	bool			bSpewPathfindQueryProgress;
	bool			bTestAddPathNodeRequiredRegionAtMeasuringToolPos;
	int				DebugGetRandomCarNode;
	int				DebugCurrentRegion;
	int				DebugCurrentNode;

	//for real-time link editing
	u8 DebugLinkbGpsCanGoBothWays;				// If this is true the gps will go both directions even on a one-way street.	
	u8 DebugLinkTilt;							// Roads will store their sideways tilt. The idea is that the transition to fudged physics cars will be smoother.
	u8 DebugLinkTiltFalloff;
	u8 DebugLinkNarrowRoad;						// Some roads are a bit more narrow and traffic has to drive closer together
	u8 DebugLinkWidth;							// Only used by cars. The width of the bit in the middle of the road (in meters)
	u8 DebugLinkDontUseForNavigation;			// Traffic lights enum used to be stored here
	u8 DebugLinkbShortCut;						// Shortcut links are used to allow vehicles to cut corners between lanes which ambient vehicles wouldn't
	u8 DebugLinkLanesFromOtherNode;
	u8 DebugLinkLanesToOtherNode;				// Only used by cars. To and From the other node
	u8 DebugLinkDistance;
	u8 DebugLinkBlockIfNoLanes;
	u32				DebugLinkRegion;
	u32				DebugLinkAddress;
	void			LoadDebugLinkData();
	void			SaveDebugLinkData();
#endif // __DEV

	// Slots for various async gps searches
	enum {
		GPS_ASYNC_SEARCH_PERIODIC = 0,
		GPS_ASYNC_SEARCH_START_NODE = 1,
		GPS_ASYNC_SEARCH_END_NODE = 2,
		GPS_ASYNC_SEARCH_NUM_SLOTS = 3
	};

	private:
	// Array to keep track of async GPS Searches
	FindNodeClosestToCoorsAsyncParams* m_GPSAsyncSearches[GPS_ASYNC_SEARCH_NUM_SLOTS];
} ;

#if __DEV
void PrintStreetNames();
//void FindDontWanderNodes();
void SwitchOffAllLoadedNodes();
void ResetAllLoadedNodes();
void OutputAllNodes();
#endif // __DEV

// wrapper class needed to interface with game skeleton code
class CPathFindWrapper
{
public:

    static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);
};

// Reference to the actual memory for the pathfind stuff
extern CPathFind	ThePaths;

extern CNodeAddress EmptyNodeAddress;	// This is here to pass into DoPathSearch function.

#if __DEV
void MovePlayerToNodeNearMarker();		// A Debug function to make things easier for the level designers
#endif // __DEV

#endif // _PATHFIND_H_
