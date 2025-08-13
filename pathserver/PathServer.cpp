#include "PathServer\PathServer.h"

// Rage headers
#include "bank/bkmgr.h"
#include "profile/cputrace.h"
#include "system/memory.h"
#include "system/threadtype.h"

// Framework headers
#include "ai/navmesh/priqueue.h"
#include "fwgeovis/geovis.h"
#include "fwsys/fileExts.h"
#ifdef GTA_ENGINE

#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "modelinfo/PedModelInfo.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "renderer/Water.h"		// Water
#include "scene/FocusEntity.h"
#include "script/script_memory_tracking.h"
#include "streaming/streamingvisualize.h"
#include "streaming/streaming.h"
#include "task/Movement/TaskNavBase.h"
#include "Vfx/Misc/Fire.h"
#if !__FINAL
#include "PathServer\ExportCollision.h"
#endif

PARAM(nonav, "turns off navmesh/navnode streaming & streaming-module registration");

#else	// GTA_ENGINE

// This include for when developing the system outside of GTA
#include "GTATypes.h"
#include "system/stockallocator.h"

#endif	// GTA_ENGINE


// All unused, it seems: /FF
// Whether to load resourced platform-dependent navmeshes, or platform-independent ones from the 'common' folder
// #define __RESOURCE_NAVMESHES					1
// Whether to load the low-load hierarchical nodes
// #define __LOAD_HIERARCHICAL_NODES				1
// (This is pretty much mandatory now, I don't think it can be un-defined).
// #define __NO_QUADTREES_FOR_DYNAMIC_NAVMESHES	1
// Whether to use the navmesh/navnodes index remapping
#define __REMAP_NAVMESH_INDICES					0

NAVMESH_OPTIMISATIONS()

#ifdef GTA_ENGINE

void CPedGenBlockedArea::Init(const Vector3 & vOrigin, const float fRadius, const u32 iDuration, const EOwner owner, const bool bShrink)
{
	m_bActive = true;
	Assert(owner >= 0 && owner < EOwner(1 << 2));	// Only got two bits for it right now.
	m_iOwner = owner;
	m_bShrink = bShrink;
	m_iType = ESphere;

	m_Sphere.SetOrigin(vOrigin);
	m_Sphere.m_fActiveRadius = fRadius;
	m_Sphere.m_fRadiusSqr = fRadius*fRadius;
	m_Sphere.m_fOriginalRadius = fRadius;
	m_iStartTime = fwTimer::GetTimeInMilliseconds();
	m_iDuration = iDuration;
}

void CPedGenBlockedArea::Init(const Vector3 * pCornerPts, const float fTopZ, const float fBottomZ, const u32 iDuration, const EOwner owner)
{
	m_bActive = true;
	Assert(owner >= 0 && owner < EOwner(1 << 2));	// Only got two bits for it right now.
	m_iOwner = owner;
	m_iType = EAngledArea;

	int p;
	for(p=0; p<4; p++)
	{
		m_Area.SetPoint(p, Vector2(pCornerPts[p].x, pCornerPts[p].y));
	}

	int lastp = 3;
	for(p=0; p<4; p++)
	{
		Vector2 vEdge = m_Area.GetPoint2(p) - m_Area.GetPoint2(lastp);
		vEdge.Normalize();

		m_Area.SetNormal(p, Vector2(vEdge.y, -vEdge.x));

		const Vector3 vNormal = m_Area.GetNormal3(p);
		const Vector3 vPt = m_Area.GetPoint3(p);
		m_Area.m_PlaneDists[p] = - DotProduct(vNormal, vPt);

		lastp = p;
	}

	m_Area.m_fTopZ = fTopZ;
	m_Area.m_fBottomZ = fBottomZ;

	m_iStartTime = fwTimer::GetTimeInMilliseconds();
	m_iDuration = iDuration;
}

bool TSphereArea::LiesWithin(const Vector3 & p)
{
	const Vector3 vOrigin(m_fOrigin[0], m_fOrigin[1], m_fOrigin[2]);
	float fDistSqr = vOrigin.Dist2(p);
	return(fDistSqr < m_fRadiusSqr);
}

bool TAngledArea::LiesWithin(const Vector3 & pt)
{
	if(pt.z < m_fBottomZ || pt.z > m_fTopZ)
		return false;

	for(int p=0; p<4; p++)
	{
		// The normals of each edge points OUTWARDS.
		// Therefore if we are behind all 4 planes, we are inside the region.
		const float fDist = DotProduct(GetNormal3(p), pt) + m_PlaneDists[p];
		if(fDist > 0.0f)
			return false;
	}

	return true;
}
#endif


//*************************************************************
//	CPathServer
//	This implements the interface which game-code uses to
//	find paths via the navigation-mesh system.
//	The actual pathfinding is done via a separate thread within
//	the CPathServerThread class.
//*************************************************************

CPathServer::EProcessingMode CPathServer::m_eProcessingMode = CPathServer::EUndefined;
#if __HIERARCHICAL_NODES_ENABLED
aiNavNodesStore * CPathServer::m_pNavNodesStore = NULL;
#endif
int CPathServer::m_iNumDynamicNavMeshesInExistence = 0;

#if !__FINAL
u32 CPathServer::ms_iNumTessellationsThisPath = 0;
u32 CPathServer::ms_iNumTestDynamicObjectLOS = 0;
u32 CPathServer::ms_iNumTestNavMeshLOS = 0;
u32 CPathServer::ms_iNumImmediateTestNavMeshLOS = 0;
u32 CPathServer::ms_iNumGetObjectsIntersectingRegion = 0;
u32 CPathServer::ms_iNumCacheHitsOnDynamicObjects = 0;
float CPathServer::m_fRunningTotalOfTimeOnPathRequests = 0.0f;
float CPathServer::m_fTimeSpentProcessingThisGameTurn = 0.0f;
#ifdef GTA_ENGINE
bool CPathServer::ms_bDrawPedGenBlockingAreas = false;
bool CPathServer::ms_bVisualiseOpponentPedGeneration = false;
bool CPathServer::ms_bDrawPedSwitchAreas = false;
#endif
#endif

u32 CPathServer::m_iThreadHousekeepingFrequencyInMillisecs = 1000;
u32 CPathServer::m_iLastTimeThreadDidHousekeeping = 0;
u32 CPathServer::m_iLastTimeCheckedForStaleRequests = 0;
u32 CPathServer::m_iCheckForStaleRequestsFreq = 2000;

bank_u32 CPathServer::m_iTimeToSleepWaitingForRequestsMs = 8;
bank_u32 CPathServer::m_iTimeToSleepDuringLongRequestsMs = 2;
bool CPathServer::m_bSleepPathServerThreadOnLongPathRequests = false;
u32 CPathServer::m_iNumFindPathIterationsBetweenSleepChecks = 64;


bool CPathServer::ms_bInitialised = false;
#ifdef GTA_ENGINE
bool CPathServer::ms_bGameInSession = false;
#else
bool CPathServer::ms_bGameInSession = true;
#endif
bool CPathServer::ms_bStreamingDisabled = false;
bool CPathServer::m_bDisablePathFinding = false;
s32 CPathServer::ms_iTimeToNextRequestAndEvict = 0;
CPathServer::EObjectAvoidanceMode CPathServer::m_eObjectAvoidanceMode = CPathServer::EPolyTessellation;
bool CPathServer::ms_bNoNeedToCheckObjectsForThisPoly = false;
bool CPathServer::ms_bHaveAnyDynamicObjectsChanged = false;
float CPathServer::ms_fDefaultDynamicObjectPlaneEpsilon = 0.0f;
bank_float CPathServer::ms_fDynamicObjectVelocityThreshold = 1.0f;
bank_bool CPathServer::ms_bTestConnectionPolyExit = true;
bank_bool CPathServer::ms_bDisallowClimbObjectLinks = false;

bank_s32 CPathServer::ms_iRequestAndEvictFreqMS = 1000;
bank_bool CPathServer::ms_bRefinePaths = true;
bank_bool CPathServer::ms_bSmoothRoutesUsingSplines = true;
bank_bool CPathServer::ms_bCutCornersOffAllPaths = false;
bank_bool CPathServer::ms_bMinimisePathDistance = true;
bank_bool CPathServer::ms_bMinimiseBeforeRefine = false;
bank_bool CPathServer::ms_bPullPathOutFromEdges = true;
bank_bool CPathServer::ms_bPullPathOutFromEdgesTwice = false;
bank_bool CPathServer::ms_bPullPathOutFromEdgesStringPull = true;
bank_bool CPathServer::ms_bDoPolyTessellation = true;
bank_bool CPathServer::ms_bDoPolyUnTessellation = true;
bank_bool CPathServer::ms_bDoPreemptiveTessellation = false;						// Tessellate any poly we visit which is near a dynamic object
bank_bool CPathServer::ms_bUseGridCellCache = false;
bank_bool CPathServer::ms_bUseTessellatedPolyObjectCache = false;
bank_bool CPathServer::ms_bAlwaysUseMorePointsInPolys = false;
//bank_bool CPathServer::ms_bSphereRayEarlyOutTestOnObjects = false;
bank_bool CPathServer::ms_bUseLosToEarlyOutPathRequests = true;
bank_bool CPathServer::ms_bDontRevisitOpenNodes = false;
bank_bool CPathServer::ms_bAllowTessellationOfOpenNodes = false;
bank_bool CPathServer::ms_bOutputDetailsOfPaths = false;
bank_bool CPathServer::ms_bTestObjectsForEveryPoly = false;
bank_bool CPathServer::ms_bUseOptimisedPolyCentroids = true;
bank_bool CPathServer::ms_bUseGridToCullObjectLOS = true;
bank_bool CPathServer::ms_bUseNewPathPriorityScoring = true;

#ifdef GTA_ENGINE
u32 CPathServer::m_iLastTimeProcessedPedGeneration = 0;
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS) // higher frequency in NG
dev_u32 CPathServer::m_iProcessPedGenerationFreq = 30;
#else
dev_u32 CPathServer::m_iProcessPedGenerationFreq = 500;
#endif
CPathServerAmbientPedGen CPathServer::m_AmbientPedGeneration;
CPathServerOpponentPedGen CPathServer::m_OpponentPedGeneration;
#endif

#if !__FINAL
bool CPathServer::ms_bDebugPathFinding = false;
bool CPathServer::ms_bEnsureLosBeforeEnding = true;
s32 CPathServer::m_iVisualiseNavMeshes = CPathServer::NavMeshVis_Off;
bool CPathServer::m_bVisualiseHierarchicalNodes = false;
bool CPathServer::m_bVisualiseLinkWidths = false;
bool CPathServer::m_bVisualisePaths = false;
int CPathServer::m_iVisualiseDataSet = kNavDomainRegular;
s32 CPathServer::m_iVisualisePathServerInfo = CPathServer::PathServerVis_Off;
bool CPathServer::m_bVisualiseQuadTrees = false;
bool CPathServer::m_bVisualiseAllCoverPoints = false;
bool CPathServer::m_bDebugShowLowClimbOvers = false;
bool CPathServer::m_bDebugShowHighClimbOvers = false;
bool CPathServer::m_bDebugShowDropDowns = false;
bool CPathServer::m_bDebugShowSpecialLinks = true;
bool CPathServer::m_bDebugShowPolyVolumesAbove = false;
bool CPathServer::m_bDebugShowPolyVolumesBelow = false;
bool CPathServer::m_bVerifyAdjacencies = false;
bool CPathServer::m_bRepeatPathQuery = false;
bool CPathServer::ms_bUseXTraceOnTheNextPathRequest = false;
bool CPathServer::m_bDisableObjectAvoidance = false;
bool CPathServer::m_bDisableAudioRequests = false;
bool CPathServer::m_bDisableGetClosestPositionForPed = false;

bool CPathServer::ms_bAllowObjectClimbing = true;
bool CPathServer::ms_bAllowNavMeshClimbs = true;
bool CPathServer::ms_bAllowObjectPushing = false;	// Needs fixing if you want to enable this.
bool CPathServer::m_bStressTestPathFinding = false;
bool CPathServer::m_bStressTestGetClosestPos = false;
bool CPathServer::m_bVisualisePolygonRequests = false;
TPathHandle CPathServer::m_iStressTestPathHandles[NUM_STRESS_TEST_PATHS];
s32 CPathServer::m_iSelectedPathRequest = 0;
s32 CPathServer::m_iSelectedLosRequest = 0;
bool CPathServer::ms_bOnlyDisplaySelectedRoute = false;
bool CPathServer::ms_bShowSearchExtents = false;
s32 CPathServer::m_iDebugThreadRunMode = 1;
Vector3 CPathServer::ms_vPathTestStartPos(0.0f,0.0f,0.0f);
Vector3 CPathServer::ms_vPathTestEndPos(0.0f,0.0f,0.0f);
Vector3 CPathServer::ms_vPathTestCoverOrigin(0.0f,0.0f,0.0f);
Vector3 CPathServer::ms_vPathTestReferenceVector(0.0f,0.0f,0.0f);
float CPathServer::ms_fPathTestReferenceDist = 0.0f;
float CPathServer::ms_fPathTestCompletionRadius = 0.0f;
float CPathServer::ms_fPathTestEntityRadius = PATHSERVER_PED_RADIUS;
int CPathServer::ms_iPathTestNumInfluenceSpheres = 0;
TInfluenceSphere CPathServer::ms_PathTestInfluenceSpheres[MAX_NUM_INFLUENCE_SPHERES];
float CPathServer::ms_fPathTestInfluenceSphereRadius = 8.0f;
float CPathServer::ms_fPathTestInfluenceMin = 1.0f;
float CPathServer::ms_fPathTestInfluenceMax = 50.0f;

bool CPathServer::ms_bMarkVisitedPolys = false;
bool CPathServer::ms_bUnMarkVisitedPolys = false;
bool CPathServer::ms_bDrawDynamicObjectGrids = false;
bool CPathServer::ms_bDrawDynamicObjectMinMaxs = false;
bool CPathServer::ms_bDrawDynamicObjectAxes = false;
bool CPathServer::ms_bRenderMiscThings = false;

#if __BANK
bkCombo * CPathServer::ms_pObjAvoidanceModeCombo = NULL;
#endif	// __BANK

s32 CPathServer::m_iObjAvoidanceModeComboIndex = (s32)CPathServer::m_eObjectAvoidanceMode;

#if !__FINAL

sysPerformanceTimer	* CPathServer::m_RequestTimer = NULL;
sysPerformanceTimer	* CPathServer::m_PedGenTimer = NULL;
sysPerformanceTimer	* CPathServer::m_NewPedGenTimer = NULL;
sysPerformanceTimer	* CPathServer::m_MainGameThreadStallTimer = NULL;
sysPerformanceTimer	* CPathServer::m_MiscTimer = NULL;
sysPerformanceTimer	* CPathServer::m_ImmediateModeTimer = NULL;
float CPathServer::m_fTimeTakenToIssueRequestsInMSecs = 0.0f;
float CPathServer::m_fTimeTakenToAddRemoveUpdateObjects = 0.0f;
float CPathServer::m_fTimeTakenToDoPedGenInMSecs = 0.0f;
#endif // !__PPU
float CPathServer::ms_fDebugVisPolysMaxDist = 40.0f;
float CPathServer::ms_fDebugVisNodesMaxDist = 60.0f;
float CPathServer::m_fVisNavMeshVertexShiftAmount = 0.1f;
bool CPathServer::m_bDebugPolyUnderfoot = false;

#endif // !__FINAL

s32 CPathServer::m_iNumPathRegionSwitches = 0;
TPathRegionSwitch CPathServer::m_PathRegionSwitches[MAX_NUM_PATH_REGION_SWITCHES];

s32 CPathServer::m_iNumDeferredAddDynamicObjects = 0;
s32 CPathServer::m_iNextScriptObjectHandle = 1;
s32 CPathServer::m_iNumScriptBlockingObjects = 0;
TScriptDeferredAddDynamicObject CPathServer::m_ScriptDeferredAddDynamicObjects[TScriptDeferredAddDynamicObject::ms_iMaxNum];
TScriptObjHandlePair CPathServer::m_ScriptedDynamicObjects[MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS];

bool CPathServer::ms_bLoadAllHierarchicalData = true;
Vector3 CPathServer::m_vOrigin = Vector3(0,0,0);
Vector3 CPathServer::m_vLastOrigin = Vector3(0,0,0);
float CPathServer::m_fNavMeshLoadProximity = 100.0f;
float CPathServer::m_fHierarchicalNodesLoadProximity = 600.0f;
int CPathServer::ms_iRequestAndEvictMeshesFreqMs = 0;
int CPathServer::m_iTimeOfLastRequestAndEvictMeshes = 0;
bool CPathServer::ms_bRunningRequestAndEvictMeshes = false;

const fwPathServerGameInterface* CPathServer::m_GameInterface = NULL;
TNavMeshRequiredRegion CPathServer::m_NavMeshRequiredRegions[NAVMESH_MAX_REQUIRED_REGIONS];

CPathRequest CPathServer::m_PathRequests[MAX_NUM_PATH_REQUESTS];
CGridRequest CPathServer::m_GridRequests[MAX_NUM_GRID_REQUESTS];
CLineOfSightRequest CPathServer::m_LineOfSightRequests[MAX_NUM_LOS_REQUESTS];
CAudioRequest CPathServer::m_AudioRequests[MAX_NUM_AUDIO_REQUESTS];
CFloodFillRequest CPathServer::m_FloodFillRequests[MAX_NUM_FLOODFILL_REQUESTS];
CClearAreaRequest CPathServer::m_ClearAreaRequests[MAX_NUM_CLEARAREA_REQUESTS];
CClosestPositionRequest CPathServer::m_ClosestPositionRequests[MAX_NUM_CLOSESTPOSITION_REQUESTS];

u32 CPathServer::m_iNextHandle = 1;
u32 CPathServer::m_iNextPathIndexToStartFrom = 0;
u32 CPathServer::m_iNextGridIndexToStartFrom = 0;
u32 CPathServer::m_iNextLosIndexToStartFrom = 0;
u32 CPathServer::m_iNextAudioIndexToStartFrom = 0;
u32 CPathServer::m_iNextFloodFillIndexToStartFrom = 0;
u32 CPathServer::m_iNextClearAreaIndexToStartFrom = 0;
u32 CPathServer::m_iNextClosestPositionIndexToStartFrom = 0;

u32 CPathServer::m_iNumDynamicObjects = 0;
CPathServerThread CPathServer::m_PathServerThread;

CPathSearchPriorityQueue * CPathServer::m_pImmediateModePrioriryQueue = NULL;
TNavMeshPoly * CPathServer::m_pImmediateModeVisitedPolys[IMMEDIATE_MODE_QUERY_MAXNUMVISITEDPOLYS];
int CPathServer::m_iImmediateModeNumVisitedPolys = 0;
Vector3 CPathServer::m_vImmediateModeLosIsectPos(0.0f,0.0f,0.0f);
Vector3 CPathServer::m_vImmediateModeWanderOrigin(0.0f,0.0f,0.0f);
Vector3 CPathServer::m_vImmediateModeWanderDir(0.0f,0.0f,0.0f);
TTestLosStack CPathServer::m_ImmediateModeTestLosStack[SIZE_TEST_LOS_STACK];
int CPathServer::m_iImmediateModeTestLosStackNumEntries =0;

#ifdef GTA_ENGINE
#if __BANK
bool CPathServer::m_bCheckForThreadStalls = false;
u32 CPathServer::m_iNumThreadStallsForNavMeshes = 0;
u32 CPathServer::m_iNumThreadStallsForDynamicObjects = 0;
u32 CPathServer::m_iNumTimesCouldntUpdateObjectsDueToGameThreadAccess = 0;
float CPathServer::m_fMainGameTimeSpendOnThreadStalls = 0.0f;
s32 CPathServer::m_iNumImmediateModeLosCallsThisFrame = 0;
s32 CPathServer::m_iNumImmediateModeWanderPathsThisFrame = 0;
s32 CPathServer::m_iNumImmediateModeFleePathsThisFrame = 0;
s32 CPathServer::m_iNumImmediateModeSeekPathsThisFrame = 0;
float CPathServer::m_fTimeSpentOnImmediateModeCallsThisFrame = 0.0f;
#endif
#endif

//******************************************************************************
// Audio parameters
f32 CPathServer::m_fAudioMinPolySizeForRadiusSearch = 0.0f;
f32 CPathServer::m_fAudioVelocityCubeSize = 1.0f;
f32 CPathServer::m_fAudioVelocityDotProduct = 0.86f;

#ifdef GTA_ENGINE
//******************************************************************************
//	Ped-generation member variables.  These will replace the old system

bool CPathServer::m_bGameRunning = false;
#else
bool CPathServer::m_bGameRunning = true;
#endif

bool CPathServer::m_bDisplayTimeTakenToAddDynamicObjects = false;
u32 CPathServer::m_iNumTimesGaveTime = 0;
bool CPathServer::ms_bCurrentlyRemovingAndAddingObjects = false;
bool CPathServer::ms_bUseEventsForRequests = true;
bool CPathServer::ms_bProcessAllPendingPathsAtOnce = true;
u32 CPathServer::ms_iBlockRequestsFlags = 0;
s32 CPathServer::m_iTotalMemoryUsedByPrecalcPaths = 0;

#ifdef GTA_ENGINE

bool CPathServer::m_bCoverPointsBufferInUse = false;
s32 CPathServer::m_iNumCoverPointsInBuffer = 0;
TCachedCoverPoint CPathServer::m_CoverPointsBuffer[SIZE_OF_COVERPOINTS_BUFFER];

bool CPathServer::m_bWaterEdgesBufferInUse = false;
bool CPathServer::m_bExtractWaterEdgesThisTimeslice = false;
s32 CPathServer::m_iNumWaterEdgesInBuffer = 0;
const float CPathServer::ms_fMinDistSqrBetweenWaterEdgePoints = 5.0f*5.0f;
const float CPathServer::ms_fFindWaterEdgesMaxDist = 20.0f;
TShortMinMax CPathServer::m_FindWaterEdgesMinMax;
Vector3 CPathServer::m_vFindWaterEdgesOrigin(0.0f,0.0f,0.0f);
Vector3 CPathServer::m_vFindWaterEdgesMin(0.0f,0.0f,0.0f);
Vector3 CPathServer::m_vFindWaterEdgesMax(0.0f,0.0f,0.0f);
Vector3 CPathServer::m_WaterEdgesBuffer[SIZE_OF_WATEREDGES_BUFFER];
u32 CPathServer::m_WaterEdgeAudioProperties[SIZE_OF_WATEREDGES_BUFFER];
bool CPathServer::m_bFindCover_CompletedTimeslice = false;
u32 CPathServer::m_iFindCover_NumNavMeshesIndices = 0;
TNavMeshIndex CPathServer::m_iFindCover_CurrentlyExtractingNavMeshIndex = 0;
CNavMesh * CPathServer::m_pFindCoverCurrentNavMesh = NULL;
TNavMeshIndex CPathServer::m_iFindCover_NavMeshesIndices[MAX_NUM_NAVMESHES_FOR_COVERAREAS];
u32 CPathServer::m_iCoverNumberAreas = 0;
CCoverBoundingArea CPathServer::m_CoverBoundingAreas[CCover::MAX_BOUNDING_AREAS];
u32 CPathServer::m_iFindCover_NavMeshProgress = 0;
CNavMeshQuadTree * CPathServer::m_pFindCover_ContinueFromThisQuadTreeLeaf = NULL;
u32 CPathServer::m_iFindCover_NavMeshPolyProgress = 0;
u32 CPathServer::m_iFindCover_NumIterationsRemainingThisTimeslice = 0;
u32 CPathServer::m_iFindCover_NumIterationsPerTimeslice = 64;
u32 CPathServer::m_iFrequencyOfCoverPointTimeslices_Millisecs = 600;
u32 CPathServer::m_iTimeOfLastCoverPointTimeslice_Millisecs = 0;
u32 CPathServer::m_iFrequencyOfWaterEdgeExtraction_Millisecs = 4000;	// *must* be greater than the coverpoint freq
u32 CPathServer::m_iTimeOfLastWaterEdgeTimeslice_Millisecs = 0;

#if !__FINAL
sysPerformanceTimer	* CPathServer::m_ExtractCoverPointsTimer = NULL;
sysPerformanceTimer	* CPathServer::m_TrackObjectsTimer = NULL;
float CPathServer::m_fLastTimeTakenToExtractCoverPointsMs = 0.0f;
float CPathServer::m_fTimeToLockDataForTracking = 0.0f;
float CPathServer::m_fTimeToTrackAllPeds = 0.0f;
#endif

#endif	// GTA_ENGINE

float CPathServer::ms_fPedGenAlgorithmNodeFloodFillDist		= 30.0f;

void InitCountCoverBitsTable()
{
	for(int i=0; i<256; i++)
	{
		int iNumBits = 0;
		if(i&1) iNumBits++;
		if(i&2) iNumBits++;
		if(i&4) iNumBits++;
		if(i&8) iNumBits++;
		if(i&16) iNumBits++;
		if(i&32) iNumBits++;
		if(i&64) iNumBits++;
		if(i&128) iNumBits++;
		g_iCountCoverBitsTable[i] = (u8)iNumBits;
	}
}

CPathServer::CPathServer()
{

}

CPathServer::~CPathServer()
{
	Shutdown();
}



//***********************************************************************************************************
//	Init()
//
//	This initialises the system, and creates CPathServerThread class.
//	iProcessorIndex' - allows the worker thread to be run on a specific CPU.  Leave as zero for now.
//	pRelativePathAndFilenameForNavMeshes - specifies a ".img" file which contains all the navmeshes for
//		the level.  The path should be relative to the game's "data/models/cdimages" folder.
//	pPathForDatFile - path to the ".dat" file used to initialise the navigation system.  Usually "data".
//	pDebugPathForNavFiles - for development only, specifies a folder where the ".nav" files can be found
//
//***********************************************************************************************************

bool
CPathServer::Init(const fwPathServerGameInterface &gameInterface
				, EProcessingMode eProcessingMode
				, u32 iProcessorIndex
				, const char * pRelativePathAndFilenameForNavMeshes, const char *
#ifndef GTA_ENGINE
				  pPathForDatFile
#endif
				  , const char * pDebugPathForNavFiles)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);
	Assert(eProcessingMode == ESingleThreaded || eProcessingMode == EMultiThreaded);

	Assert(!m_GameInterface);		// Not expected to get called more than once without a matching Shutdown().
	m_GameInterface = &gameInterface;
	//@@: location CPATHSERVER_INIT
	m_eProcessingMode = eProcessingMode;
	m_iTotalMemoryUsed = sizeof(CPathServer);

	InitCountCoverBitsTable();

#if !__FINAL
	m_iVisualiseNavMeshes = CPathServer::NavMeshVis_Off;
	m_iVisualisePathServerInfo = CPathServer::PathServerVis_Off;
	m_RequestTimer = rage_new sysPerformanceTimer("PathServer Requests Timer");
	m_PedGenTimer = rage_new sysPerformanceTimer("Ped-Gen Timer");
	m_NavMeshLoadingTimer = rage_new sysPerformanceTimer("NavMesh Loading Timer");
	m_NavMesh2ndLoadingTimer = rage_new sysPerformanceTimer("NavMesh 2nd Loading Timer");
	m_MainGameThreadStallTimer = rage_new sysPerformanceTimer("Main Game Thread-Stall Timer");
	m_MiscTimer = rage_new sysPerformanceTimer("Misc Timer");
	m_ImmediateModeTimer = rage_new sysPerformanceTimer("Immediate-Mode Timer");
#ifdef GTA_ENGINE
	m_ExtractCoverPointsTimer = rage_new sysPerformanceTimer("Extract CoverPoints Timer");
	m_TrackObjectsTimer = rage_new sysPerformanceTimer("Track Objects Timer");
#endif
#endif

	m_iNumTimesGaveTime = 0;
	ms_bInitialised = true;

	if(!pRelativePathAndFilenameForNavMeshes)
	{
		pRelativePathAndFilenameForNavMeshes = "navmeshes.img";
	}

#ifdef GTA_ENGINE
	if(!m_pNavMeshStores[kNavDomainRegular])
	{
		Assertf(false, "PathServer::Init() - didn't initialise properly.");
		return false;
	}
#endif

	m_DynamicNavMeshStore.Init(m_iMaxDynamicNavmeshTypes);

	//*********************************************************************************************

#ifdef GTA_ENGINE

	(void)pDebugPathForNavFiles;	// This variable is used offline

	//RegisterStreamingModule();

#else	// GTA_ENGINE

	RegisterStreamingModule(pPathForDatFile);

	//*********************************************************************************************

	if(!LoadAllMeshes(pDebugPathForNavFiles))
	{
		return false;
	}
#endif	// GTA_ENGINE

	// Create the tessellation navmesh, which stores all the polys temporarily tessellated around dynamic objects
	if(ms_bDoPolyTessellation)
	{
		CreateTessellationNavMesh();
	}

	// TODO: Figure out what to do here. If TAdjPoly::ms_iAdjacentNavmeshOffsets is going to be used for something,
	// we may need to keep one array for each domain. Right now, nothing uses it though, so we just initialize
	// from the regular navigation domain.
	TAdjPoly::InitAdjacentNavmeshOffsets(m_pNavMeshStores[kNavDomainRegular]->GetNumMeshesInX());

	if(!m_PathServerThread.Init(iProcessorIndex))
	{
		return false;
	}

	m_pImmediateModePrioriryQueue = rage_new CPathSearchPriorityQueue(IMMEDIATE_MODE_QUERY_MAXNUMVISITEDPOLYS);

	Reset();

	return true;
}

//************************************************************
//	ReadDatFile
//	This function reads in the "nav.dat" file, which details
//	how many sectors there are for each navmesh in the world.
//************************************************************
bool
CPathServer::ReadDatFile(const char * pGameDataPath, CNavDatInfo & navMeshesInfo, CNavDatInfo & GTA_ENGINE_ONLY(navNodesInfo))
{
#ifdef GTA_ENGINE

	(void)pGameDataPath;	// This variable is used offline

	ASSET.PushFolder("common:/data/");
	fiStream * stream = ASSET.Open("nav", "dat", true);
	ASSET.PopFolder();

	if(stream)
	{
		fiAsciiTokenizer token;
		token.Init(NULL, stream);

		// Read in each line
		char lineBuff[1024];
		while(token.GetLine(lineBuff, 1024) > 0)
		{
			if(lineBuff[0] == '#')
			{
				continue;
			}
			int iNumMatched = 0;
			int iValue;

			// SECTORS_PER_NAVMESH defines the resolution which this game's navmesh system works at.
			// A lower value equals more files but smaller sized sections.
			// A higher values means less files, but larger filesizes.
			iNumMatched = sscanf(lineBuff, "SECTORS_PER_NAVMESH = %i", &iValue);
			if(iNumMatched)
			{
#if HEIGHTMAP_GENERATOR_TOOL
				iValue = gv::WORLD_CELLS_PER_TILE;
#endif
				const int iNumSectorsPerMesh = iValue;
				const float fMeshSize = CPathServerExtents::GetWorldWidthOfSector() * ((float)iNumSectorsPerMesh);

				navMeshesInfo.iSectorsPerMesh = iNumSectorsPerMesh;
				navMeshesInfo.fMeshSize = fMeshSize;
#if HEIGHTMAP_GENERATOR_TOOL
				navMeshesInfo.iNumMeshesInX = (gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X)/gv::WORLD_TILE_SIZE;
				navMeshesInfo.iNumMeshesInY = (gv::WORLD_BOUNDS_MAX_Y - gv::WORLD_BOUNDS_MIN_Y)/gv::WORLD_TILE_SIZE;
#else
				navMeshesInfo.iNumMeshesInX = 100;	//(WORLD_WIDTHINSECTORS / iNumSectorsPerMesh);
				navMeshesInfo.iNumMeshesInY = 100;  //(WORLD_DEPTHINSECTORS / iNumSectorsPerMesh);
#endif
				navMeshesInfo.iMaxMeshIndex = navMeshesInfo.iNumMeshesInX * navMeshesInfo.iNumMeshesInY;

				continue;
			}
			// SECTORS_PER_NAVMESH defines the resolution which this game's navnodes system works at.
			// Currently this *must* be a multiple of SECTORS_PER_NAVMESH, above
			iNumMatched = sscanf(lineBuff, "SECTORS_PER_NAVNODES = %i", &iValue);
			if(iNumMatched)
			{
#if __HIERARCHICAL_NODES_ENABLED
				const int iNumSectorsPerMesh = iValue;
				const float fMeshSize = CPathServerExtents::GetWorldWidthOfSector() * ((float)iNumSectorsPerMesh);

				navNodesInfo.iSectorsPerMesh = iNumSectorsPerMesh;
				navNodesInfo.fMeshSize = fMeshSize;
				navNodesInfo.iNumMeshesInX = 100 / (navNodesInfo.iSectorsPerMesh/navMeshesInfo.iSectorsPerMesh);
				navNodesInfo.iNumMeshesInY = 100 / (navNodesInfo.iSectorsPerMesh/navMeshesInfo.iSectorsPerMesh);
				navNodesInfo.iMaxMeshIndex = navNodesInfo.iNumMeshesInX * navNodesInfo.iNumMeshesInY;
#endif
				continue;
			}
			// NAVMESH_LOAD_DISTANCE defines the distance at which meshes are loaded/streamed in.
			iNumMatched = sscanf(lineBuff, "NAVMESH_LOAD_DISTANCE = %i", &iValue);
			if(iNumMatched)
			{
				m_fNavMeshLoadProximity = (float)iValue;
				continue;
			}
			// MAX_NUM_NAVMESHES_IN_ANY_PACKFILE
			iNumMatched = sscanf(lineBuff, "MAX_NUM_NAVMESHES_IN_ANY_LEVEL = %i", &iValue);
			if(iNumMatched)
			{
				navMeshesInfo.iNumMeshesInAnyLevel = iValue;
				continue;
			}
			// MAX_NUM_NAVNODES_IN_ANY_PACKFILE
			iNumMatched = sscanf(lineBuff, "MAX_NUM_NAVNODES_IN_ANY_LEVEL = %i", &iValue);
			if(iNumMatched)
			{
				Assert(iValue >= 0 && iValue <= 10000);
				navNodesInfo.iNumMeshesInAnyLevel = iValue;
				continue;
			}
			// MAX_NUM_DYNAMIC_NAVMESH_TYPES
			iNumMatched = sscanf(lineBuff, "MAX_NUM_DYNAMIC_NAVMESH_TYPES = %i", &iValue);
			if(iNumMatched)
			{
				Assert(iValue >= 0 && iValue <= 10000);
				m_iMaxDynamicNavmeshTypes = iValue;
				continue;
			}
		}
		stream->Close();
		return true;
	}

	return false;

#else	//GTA_ENGINE

	char fullFileName[512];
	sprintf(fullFileName, "%s\\nav.dat", pGameDataPath);

	FILE * pFile = fopen(fullFileName, "rt");
	if(!pFile)
	{
		return false;
	}

	char string[256];

	while(fgets(string, 256, pFile))
	{
		if(string[0] == '#')
			continue;

		int iNumMatched = 0;
		int iValue;

		// SECTORS_PER_NAVMESH defines the resolution which this game's navmesh system works at.
		iNumMatched = sscanf(string, "SECTORS_PER_NAVMESH = %i", &iValue);
		if(iNumMatched)
		{
#if HEIGHTMAP_GENERATOR_TOOL
			iValue = CGameWorldHeightMap::WORLD_CELLS_PER_TILE;
#endif
			const int iNumSectorsPerMesh = iValue;
			const float fMeshSize = CPathServerExtents::GetWorldWidthOfSector() * ((float)iNumSectorsPerMesh);

			navMeshesInfo.iSectorsPerMesh = iNumSectorsPerMesh;
			navMeshesInfo.fMeshSize = fMeshSize;
#if HEIGHTMAP_GENERATOR_TOOL
			navMeshesInfo.iNumMeshesInX = (gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X)/gv::WORLD_TILE_SIZE;
			navMeshesInfo.iNumMeshesInY = (gv::WORLD_BOUNDS_MAX_Y - gv::WORLD_BOUNDS_MIN_Y)/gv::WORLD_TILE_SIZE;
#else
			navMeshesInfo.iNumMeshesInX = 100;	//(WORLD_WIDTHINSECTORS / iNumSectorsPerMesh);
			navMeshesInfo.iNumMeshesInY = 100;  //(WORLD_DEPTHINSECTORS / iNumSectorsPerMesh);
#endif
			navMeshesInfo.iMaxMeshIndex = navMeshesInfo.iNumMeshesInX * navMeshesInfo.iNumMeshesInY;
			continue;
		}
		// SECTORS_PER_NAVMESH defines the resolution which this game's navnodes system works at.
		// Currently this *must* be a multiple of SECTORS_PER_NAVMESH, above
		iNumMatched = sscanf(string, "SECTORS_PER_NAVNODES = %i", &iValue);
		if(iNumMatched)
		{
#if __HIERARCHICAL_NODES_ENABLED
			const int iNumSectorsPerMesh = iValue;
			const float fMeshSize = CPathServerExtents::GetWorldWidthOfSector() * ((float)iNumSectorsPerMesh);

			navNodesInfo.iSectorsPerMesh = iNumSectorsPerMesh;
			navNodesInfo.fMeshSize = fMeshSize;
			navNodesInfo.iNumMeshesInX = 100 / (navNodesInfo.iSectorsPerMesh/navMeshesInfo.iSectorsPerMesh);
			navNodesInfo.iNumMeshesInY = 100 / (navNodesInfo.iSectorsPerMesh/navMeshesInfo.iSectorsPerMesh);
			navNodesInfo.iMaxMeshIndex = navNodesInfo.iNumMeshesInX * navNodesInfo.iNumMeshesInY;
#endif
			continue;
		}
	}

	fclose(pFile);
	return true;

#endif
}

void CPathServer::ReadIndexMappingFiles()
{
#ifdef GTA_ENGINE
#if __REMAP_NAVMESH_INDICES
	static const CDataFileMgr::DataFileType s_FileTypesForDataSets[] =
	{
		CDataFileMgr::NAVMESH_INDEXREMAPPING_FILE,
		CDataFileMgr::HEIGHTMESH_INDEXREMAPPING_FILE
	};
	CompileTimeAssert(NELEM(s_FileTypesForDataSets) == kNumNavDomains);

	for(int i = 0; i < kNumNavDomains; i++)
	{
		if(!fwPathServer::GetIsNavDomainEnabled((aiNavDomain)i))
			continue;
		aiNavMeshStore* pStore = m_pNavMeshStores[i];
		pStore->AllocateMapping(pStore->GetMaxMeshIndex());
		CDataFileMgr::DataFileType type = s_FileTypesForDataSets[i];
		if(!ReadIndexMappingFile(*pStore, type))
		{
			// The backup code below for creating an identity mapping I found to not
			// be very helpful - if we couldn't load the index mapping file for
			// heightmeshes, just leaving the indices at their existing values means
			// that we don't try to load anything, which is what I would expect.
			// So, we only do that for the regular navigation domain. /FF

			if(i == kNavDomainRegular)
			{
				for(int m=0; m<pStore->GetMaxMeshIndex(); m++)
				{
					pStore->SetMapping(m, (s16)m);
				}
			}
		}
	}
#endif
#endif
}


#if !__FINAL && SANITY_CHECK_TESSELLATION

void CPathServer::SanityCheckPolyConnectionsForAllNavMeshes(void)
{
	u32 i,p;
	for(i=0; i<m_iTotalNumNavMeshes; i++)
	{
		CNavMesh * pNavMesh = m_pNavMeshStore.GetPtr(i);

		if(!pNavMesh)
			continue;

		for(p=0; p<pNavMesh->m_iNumPolys; p++)
		{
			TNavMeshPoly * pPoly = pNavMesh->GetPoly(p);
			if(pPoly->GetReplacedByTessellation())
				continue;

			m_PathServerThread.SanityCheckPolyConnections(pNavMesh, pPoly);
		}
	}
}

#endif

//*****************************************************************************
// Remove any tessellated polys which are derived from polys in this navmesh,
// by resetting the in-use flag in the 'm_TessellationPolysInUse' bit-array.
// Visit all of their adjacent polys, and if they link to the poly we are
// removing - then we reset their adjacency to its initial values
//*****************************************************************************

#ifdef GTA_ENGINE

void CPathServer::PrepareToUnloadNavMeshDataSetNormal(void * UNUSED_PARAM(pNavMesh))
{
	// NB: We might want to add a 'ForceAbortCurrentPathRequest()' call here?
	// Otherwise we may risk incurring a stall on the main thread, due to the
	// pathserver's activity.
	CPathServer::ForceAbortCurrentPathRequest();

	// Wait for access from pathfinding thread (and/or main game thread) to finish
	LOCK_NAVMESH_DATA;

	DetessellateAllPolys();
}

void
CPathServer::PrepareToUnloadNavMeshDataSetHeightMesh(void * UNUSED_PARAM(pNavMesh))
{
	// Nothing to do here, I think. /FF
}

void
CPathServer::PrepareToUnloadHierarchicalNavData(void * UNUSED_PARAM(pNavData))
{
	// Wait for access from pathfinding thread (and/or main game thread) to finish
//	LOCK_NAVMESH_DATA;
}

#endif





//*********************************************************************************
//	CPathServer::Process
//	This function must be called once a frame from the main game thread.  Here is
//	where we request/evict navmeshes, and also make copies of some data from the
//	CCover class.
//*********************************************************************************

void
CPathServer::Process(const Vector3 & vOrigin)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);
	m_vOrigin = vOrigin;
	m_iNumTimesGaveTime = 0;

	//
#if !__FINAL
	m_fTimeSpentProcessingThisGameTurn = m_fRunningTotalOfTimeOnPathRequests;
	m_fRunningTotalOfTimeOnPathRequests = 0.0f;
#endif

	// Update the player's origin (always entry zero in this array)
	m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_bActive = true;
	m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_vOrigin = Vector2(vOrigin.x, vOrigin.y);
	m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_fNavMeshLoadRadius = m_fNavMeshLoadProximity;
	m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_fHierarchicalNodesLoadRadius = m_fHierarchicalNodesLoadProximity;
	m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_iThreadId = static_cast<scrThreadId>(0xFFFFFFFF);

#ifdef GTA_ENGINE

	static dev_float fForceUpdateMagSqr = 50.0f * 50.0f;
	if( (m_vLastOrigin - m_vOrigin).XYMag2() > fForceUpdateMagSqr)
	{
		CPathServer::RequestAndEvictNextFrame();
	}

	// Process the requesting & removal of navmeshes
	RequestAndEvictMeshes();

#endif

	m_vLastOrigin = m_vOrigin;


	//******************************************************************************
	// We need to make a copy of the bounding areas from the CCover class.
	// We'll only do this when the 'm_bCoverPointsBufferInUse' is set to false.
	// We needn't worry about being locked out of access to this buffer, since
	// this Process() call occurs every frame - whereas the update function runs
	// at only every 200ms (or so).

#ifdef GTA_ENGINE

	if(!m_bCoverPointsBufferInUse)
	{
		m_bCoverPointsBufferInUse = true;
		m_iCoverNumberAreas = CCover::CopyBoundingAreas(&m_CoverBoundingAreas[0]);
		m_bCoverPointsBufferInUse = false;
	}

	m_AmbientPedGeneration.ProcessPedGenBlockedAreas();

#if !__FINAL
	// If stress-testing the pathfinder, then request (up to) 10 paths every frame between
	// the player and the focus ped.
	if(m_bStressTestPathFinding)
	{
		for(int i=0; i<NUM_STRESS_TEST_PATHS; i++)
		{
			TPathHandle hPath = m_iStressTestPathHandles[i];
			if(hPath != PATH_HANDLE_NULL)
			{
				if(IsRequestResultReady(hPath)==ERequest_Ready)
				{
					CancelRequestInternal(hPath, false);
					m_iStressTestPathHandles[i] = PATH_HANDLE_NULL;
				}
			}
			else
			{
				CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
				if(pFocusEntity && pFocusEntity->GetIsTypePed())
				{
					CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);

					m_iStressTestPathHandles[i] = RequestPath(VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), 0, 1.0f, pFocusPed );
				}
			}
		}
	}

	if(m_bStressTestGetClosestPos)
	{
		Vector3 vPlayerPos = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
		static const int iNumIters = 20;
		for(int t=0; t<iNumIters; t++)
		{
			const Vector3 vRandom(fwRandom::GetRandomNumberInRange(-50.0f,50.0f), fwRandom::GetRandomNumberInRange(-50.0f,50.0f), fwRandom::GetRandomNumberInRange(-50.0f,50.0f));
			Vector3 vOut;
			CPathServer::GetClosestPositionForPed(vPlayerPos + vRandom, vOut, 50.0f);
		}
	}
#endif


	// Look through all the path requests and cancel those which haven't been polled in the timeout period.
	// This is to stop those requests from clogging up the free slots in the pathfinder.  This function will assert
	// also, as it is up to clients to ensure that all requests they issue are either retrieved or canceled.
	//
	// JB: This has been moved forwards from within the navmesh lock below, to avoid a rare deadlock.  Checking
	// for state requests requires a LOCK_NAVMESH_REQUESTS lock, but sometimes a path request from the main thread
	// may do the opposite - locking the requests and then the navmeshes.  If unlucky we get could a stalemate.

	if(fwTimer::GetTimeInMilliseconds() - CPathServer::m_iLastTimeCheckedForStaleRequests >= CPathServer::m_iCheckForStaleRequestsFreq)
	{
		m_PathServerThread.CheckForStaleRequests();

		CPathServer::m_iLastTimeCheckedForStaleRequests = fwTimer::GetTimeInMilliseconds();
	}


	LOCK_REQUESTS

	//-------------------------------------------------------------------------------------------------------
	// Handle the disabling of timeslicing on peds who are waiting for a request which has completed.
	// For now only performed for path requests
	// NB: We could also disable timeslicing for peds who have a request pending in order to reduce latency
	// by a further frame - but this could adversely affect performance - we can have a lot of peds waiting
	// for paths.
	
	for(s32 r=0; r<MAX_NUM_PATH_REQUESTS; r++)
	{
		if(m_PathRequests[r].m_bComplete && !m_PathRequests[r].m_bSlotEmpty)
		{
			CPed * pPed = (CPed*)m_PathRequests[r].m_PedWaitingForThisRequest.Get();
			if(pPed)
			{
				pPed->SetPedResetFlag( CPED_RESET_FLAG_WaitingForCompletedPathRequest, true );
			}
		}
	}
	
	for(s32 r=0; r<MAX_NUM_PATH_REQUESTS; r++)
	{
		if(m_PathRequests[r].m_bRequestPending && !m_PathRequests[r].m_bRequestActive)
		{
			if (m_PathRequests[r].m_bKeepUpdatingPedStartPosition && m_PathRequests[r].m_PedWaitingForThisRequest.Get())
			{
				CPed* pPed = static_cast<CPed*>(m_PathRequests[r].m_PedWaitingForThisRequest.Get());
				Vector3 vPathStartPosition = VEC3V_TO_VECTOR3(m_PathRequests[r].m_PedWaitingForThisRequest.Get()->GetTransform().GetPosition());

				if (m_PathRequests[r].m_fDistAheadOfPed > 0.f)
				{
					CTask* pTask = pPed->GetPedIntelligence()->GetActiveMovementTask();
					if (pTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
					{
						vPathStartPosition = ((CTaskNavBase*)pTask)->PathRequest_DistanceAheadCalculation(m_PathRequests[r].m_fDistAheadOfPed, pPed);
					}
				}

				if(pPed->GetNavMeshTracker().IsUpToDate(vPathStartPosition))
				{
					m_PathRequests[r].m_vPathStart = pPed->GetNavMeshTracker().GetLastPosition();
					m_PathRequests[r].m_StartNavmeshAndPoly = pPed->GetNavMeshTracker().GetNavMeshAndPoly();
				}
				else
				{
					m_PathRequests[r].m_vPathStart = vPathStartPosition;
					m_PathRequests[r].m_StartNavmeshAndPoly.Reset();
				}

				// This chunk is to make it less likely to end up on the other side of a thin wall due to dynamic objects
				if (!pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().IsLastNavMeshIntersectionValid())
				{
					Vector3 vNavDiff = m_PathRequests[r].m_vPathStart - pPed->GetNavMeshTracker().GetLastNavMeshIntersection();
					if (vNavDiff.XYMag2() < 0.3f * 0.3f)
					{
						m_PathRequests[r].m_vPathStart = pPed->GetNavMeshTracker().GetLastNavMeshIntersection();
						m_PathRequests[r].m_vPathStart.z += 1.0f;
					}
				}
			}

			if (m_PathRequests[r].m_EntityEndPosition.Get())
				m_PathRequests[r].m_vPathEnd = VEC3V_TO_VECTOR3(m_PathRequests[r].m_EntityEndPosition.Get()->GetTransform().GetPosition());

		}
	}

	UNLOCK_REQUESTS

	// Processed the deferred adding of scripted blocking objects.
	// This is done at a safe part of the frame when we can ensure no navmesh queries are ongoing.
	// The pathserver thread yields whenever ms_iBlockRequestsFlags is non-zero

	if( m_iNumDeferredAddDynamicObjects > 0 )
	{
		const u32 iFlag = BLOCK_REQUESTS_ON_ADD_DYNAMIC_OBJECTS;
		sysInterlockedOr(&ms_iBlockRequestsFlags, iFlag);

		ForceAbortCurrentPathRequest();
		ProcessAddDeferredDynamicObjects();

		sysInterlockedAnd(&ms_iBlockRequestsFlags, ~iFlag);
	}

#endif	// GTA_ENGINE

}


void
CPathServer::Process(void)
{
#ifdef GTA_ENGINE

	m_bGameRunning = true;

	static const u32 iMaxNullTime = 10000;
	static u32 iLastTimePedWasNonNull = 0;

	iLastTimePedWasNonNull = fwTimer::GetTimeInMilliseconds();
	Process(CFocusEntityMgr::GetMgr().GetPos());

	if((fwTimer::GetTimeInMilliseconds() - iLastTimePedWasNonNull) > iMaxNullTime)
	{
		Assertf(false, "FindPlayerPed() has been returning NULL for over 10 seconds.  Something is wrong.");
	}

#endif
}

//**********************************************************************************************
// This function requests new navmeshes, and removes others based upon the m_vOrigin member.
// Meshes which overlap the 'm_fNavMeshLoadProximity' radius of interest from the m_vOrigin,
// are requested (if not already loaded).
// We use the last origin, in order to remove meshes which are no longer with range.
//
// NB : It will be necessary to delay the evicting of meshes, so that we don't have situations
// where the player repeatedly crosses the request/evict boundary and causes the same
// navmeshes to be repeatedly requested & evicted, thereby screwing up the streaming.
//
// NB2 : We should implement a 2nd origin, so that game-designers can force navmeshes to be
// loaded in around another location - in much the same way that the collision system can
// specify a 2nd origin as well.
//
//**********************************************************************************************

void CPathServer::RequestAndEvictNextFrame()
{
	ms_iTimeToNextRequestAndEvict = 0;
}

#ifdef GTA_ENGINE

atArray<aiMeshLoadRegion> loadRegions(0,64);
u32 g_bTryLockNumTimesFailed = 0;

void CPathServer::RequestAndEvictMeshes()
{
	if(ms_bStreamingDisabled)
		return;

	//--------------------------------------------------------------------------

	s32 iTimeBefore = ms_iTimeToNextRequestAndEvict;
	ms_iTimeToNextRequestAndEvict -= fwTimer::GetTimeStepInMilliseconds();
	if(ms_iTimeToNextRequestAndEvict >= 0)
		return;

	//-------------------------------------------
	// Stop pathserver from processing requests

	if(iTimeBefore >= 0)
	{
		const u32 iFlag = BLOCK_REQUESTS_ON_REQUEST_AND_EVICT;
		sysInterlockedOr(&ms_iBlockRequestsFlags, iFlag);
	}


	// Can we take the critical section for the navmeshes?
	// If so we can proceed, otherwise wait until available - the pathserver thread will
	// not attempt to service any more requests while ms_iTimeToNextRequestAndEvict <= 0

	if( !m_NavMeshDataCriticalSectionToken.TryLock())
	{
		g_bTryLockNumTimesFailed++;
		return;
	}

	//--------------------------------------------------------------------------

	ms_bRunningRequestAndEvictMeshes = true;

	int r;

#ifdef GTA_ENGINE
	STRVIS_SET_CONTEXT(strStreamingVisualize::PATHSERVER);
#endif // GTA_ENGINE

	//************************************************************************
	// Copy regions for passing into navmesh RequestAndEvict() function

	loadRegions.clear();
	for(r=0; r<NAVMESH_MAX_REQUIRED_REGIONS; r++)
	{
		aiMeshLoadRegion region;

		if(m_NavMeshRequiredRegions[r].m_bActive)
		{
			region.m_vOrigin = m_NavMeshRequiredRegions[r].m_vOrigin;
			region.m_fRadius = m_NavMeshRequiredRegions[r].m_fNavMeshLoadRadius;
			loadRegions.Append() = region;
		}
	}

	// If player switch is active, prevent request/evict of navmeshes until we are close to the destination
	// Without this step we find that the streaming has a deluge of requests to service once it gets to the
	// destination - many of which are no longer required (I suppose they must get buffered during the switch)
	bool bBlockRequestEvict = false;

	if( g_PlayerSwitch.IsActive() )
	{
		const Vector3 vPos = CFocusEntityMgr::GetMgr().GetPos();
		const Vector3 vDest = VEC3V_TO_VECTOR3(g_PlayerSwitch.GetMgr(g_PlayerSwitch.GetSwitchType())->GetDestPos());

		if((vPos - vDest).XYMag2() > 100.0f)
		{
			bBlockRequestEvict = true;
		}
	}

	//*********************************************************************

	LOCK_STORE_LOADED_MESHES;

	if(!bBlockRequestEvict)
	{
		m_pNavMeshStores[kNavDomainRegular]->RequestAndEvict(loadRegions, PrepareToUnloadNavMeshDataSetNormal);

		if(fwPathServer::GetIsNavDomainEnabled(kNavDomainHeightMeshes))
			m_pNavMeshStores[kNavDomainHeightMeshes]->RequestAndEvict(loadRegions, PrepareToUnloadNavMeshDataSetHeightMesh);

#if __HIERARCHICAL_NODES_ENABLED
		//*********************************************************************
		// Expand regions for passing into navnode RequestAndEvict() function

		for(r=0; r<loadRegions.GetCount(); r++)
		{
			loadRegions[r].m_fRadius *= m_fHierarchicalNodesLoadProximity / m_fNavMeshLoadProximity;
		}

		m_pNavNodesStore->RequestAndEvict(loadRegions, PrepareToUnloadHierarchicalNavData);
#endif

	}

	//**********************************************************************************
	//	Process adding/removal of dynamic navmeshes.

	aiNavMeshStore* pMeshStoreForDynamicMeshes = m_pNavMeshStores[kNavDomainRegular];

	for(int i=0; i<m_DynamicNavMeshStore.GetNum(); i++)
	{
		const CModelInfoNavMeshRef & ref = m_DynamicNavMeshStore.Get(i);

		// Request
		if(ref.m_iNumRefs > 0 && !ref.m_pBackUpNavmeshCopy)
		{
			if(!pMeshStoreForDynamicMeshes->HasObjectLoaded(ref.m_iStreamingIndex))
			{
				// These RequestObject() calls are sometimes ignored by the streaming - so I've added these flags.
				pMeshStoreForDynamicMeshes->StreamingRequest(ref.m_iStreamingIndex, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			}
		}
		// Remove
		else if(ref.m_iNumRefs <= 0 && ref.m_pBackUpNavmeshCopy)
		{
			PrepareToUnloadNavMeshDataSetNormal(ref.m_pBackUpNavmeshCopy);
			pMeshStoreForDynamicMeshes->StreamingRemove(ref.m_iStreamingIndex);
		}
	}

#ifdef GTA_ENGINE
	STRVIS_SET_CONTEXT(strStreamingVisualize::NONE);
#endif // GTA_ENGINE

	ms_bRunningRequestAndEvictMeshes = false;
	ms_iTimeToNextRequestAndEvict = ms_iRequestAndEvictFreqMS;
	m_NavMeshDataCriticalSectionToken.Unlock();


	//---------------------------------------------
	// Allow pathserver to process requests again

	const u32 iFlag = BLOCK_REQUESTS_ON_REQUEST_AND_EVICT;
	sysInterlockedAnd(&ms_iBlockRequestsFlags, ~iFlag);
}



// Requests navmeshes to be loaded in the specified area
bool CPathServer::AddNavMeshRegion( const NavMeshRequiredRegion region, const scrThreadId iThreadId, const float fOriginX, const float fOriginY, const float fRadius )
{
	Assertf( region != NMR_GameplayOrigin, "You cannot manually add a NMR_GameplayOrigin region!" );

#if __BANK
	char callingScriptName[64] = { 0 };
#endif

	if( region == NMR_Script)
	{
		if(iThreadId == THREAD_INVALID)
		{
			Assertf(iThreadId != THREAD_INVALID, "Invalid scrThreadId");
			return false;
		}

#if __BANK
		GtaThread * pScriptThread = GtaThread::GetThreadWithThreadId(iThreadId);
		if(pScriptThread == NULL)
		{
			Assertf(pScriptThread != NULL, "NULL script thread");
			return false;
		}

		const char * pScriptName = pScriptThread->GetScriptName();
		if(pScriptName == NULL)
		{
			Assertf(pScriptName != NULL && *pScriptName != 0, "Script has no name");
			return false;
		}

		strcpy(callingScriptName, pScriptName);

		if(m_NavMeshRequiredRegions[NMR_Script].m_iThreadId != THREAD_INVALID && m_NavMeshRequiredRegions[NMR_Script].m_iThreadId != iThreadId)
		{
			Assertf(m_NavMeshRequiredRegions[NMR_Script].m_iThreadId == THREAD_INVALID, "NMR_Script region already in use by script \"%s\".\nScript \"%s\" is trying to add the NMR_Script navmesh region whilst the original script is still using it (the original script must call REMOVE_NAVMESH_REQUIRED_REGION first)",
				m_NavMeshRequiredRegions[NMR_Script].m_ScriptName, callingScriptName);
			return false;
		}

		Printf("Adding scripted navmesh region at (%.1f, %.1f) - script \"%s\"\n", fOriginX, fOriginY, pScriptName );
#endif // __BANK

	}
	else
	{
		Assertf( iThreadId==0, "Why is there a thread ID specified for a navmesh region in the NMR_NetworkRespawnMgr slot??" );

		if(!Verifyf( m_NavMeshRequiredRegions[NMR_NetworkRespawnMgr].m_bActive==false, "NMR_NetworkRespawnMgr slot for navmesh required region is already in use.  You have to call \"NETWORK_CANCEL_RESPAWN_SEARCH\" to cancel it.") )
		{
			return false;
		}
	}

	m_NavMeshRequiredRegions[region].m_vOrigin = Vector2(fOriginX, fOriginY);
	m_NavMeshRequiredRegions[region].m_fNavMeshLoadRadius = fRadius;
	m_NavMeshRequiredRegions[region].m_fHierarchicalNodesLoadRadius = 0.0f;
	m_NavMeshRequiredRegions[region].m_iThreadId = iThreadId;
	m_NavMeshRequiredRegions[region].m_bActive = true;

#if __BANK
	if(iThreadId != THREAD_INVALID)
	{
		strcpy(m_NavMeshRequiredRegions[region].m_ScriptName, callingScriptName);
	}
#endif

	return true;
}

scrThreadId CPathServer::GetNavmeshMeshRegionScriptID()
{
	if(m_NavMeshRequiredRegions[NMR_Script].m_bActive)
	{
		return m_NavMeshRequiredRegions[NMR_Script].m_iThreadId;
	}
	else
	{
		return THREAD_INVALID;
	}
}

bool CPathServer::GetIsNavMeshRegionRequired( const NavMeshRequiredRegion region )
{
	return m_NavMeshRequiredRegions[region].m_bActive;
}

bool CPathServer::RemoveNavMeshRegion( const NavMeshRequiredRegion region, const scrThreadId iThreadId )
{
	Assertf( region != NMR_GameplayOrigin, "You cannot remove the NMR_GameplayOrigin region!");

	if( region == NMR_NetworkRespawnMgr )
	{
		Assertf( m_NavMeshRequiredRegions[NMR_NetworkRespawnMgr].m_bActive, "Attempting to remove a navmesh region NMR_NetworkRespawnMgr which is not active.");
		Assertf( iThreadId==0, "Why is there a thread ID specified for a navmesh region in the NMR_NetworkRespawnMgr slot??" );

		Printf("Removing network respawn navmesh region at (%.1f, %.1f)\n",
			m_NavMeshRequiredRegions[NMR_NetworkRespawnMgr].m_vOrigin.x, m_NavMeshRequiredRegions[NMR_NetworkRespawnMgr].m_vOrigin.y );

		m_NavMeshRequiredRegions[NMR_NetworkRespawnMgr].m_bActive = false;

		return true;
	}

	else if( region == NMR_Script )
	{
		// Nothing to remove?
		if(m_NavMeshRequiredRegions[NMR_Script].m_iThreadId == THREAD_INVALID)
		{
			return true;
		}

		if(iThreadId == THREAD_INVALID)
		{
			Assertf(iThreadId != THREAD_INVALID, "Invalid scrThreadId");
			return false;
		}

#if __BANK
		GtaThread * pScriptThread = GtaThread::GetThreadWithThreadId(iThreadId);
		if(pScriptThread == NULL)
		{
			Assertf(pScriptThread != NULL, "NULL script thread");
			return false;
		}

		const char * pScriptName = pScriptThread->GetScriptName();
		if(pScriptName == NULL)
		{
			Assertf(pScriptName != NULL && *pScriptName != 0, "Script has no name");
			return false;
		}
/*
		if(m_NavMeshRequiredRegions[NMR_Script].m_iThreadId != iThreadId)
		{
		//	This assert removed at Neil & Will's request (url:bugstar:1207781)
			Assertf(m_NavMeshRequiredRegions[NMR_Script].m_iThreadId == iThreadId, "NMR_Script region is being removed by a script which did not set it up.  This will cause errors.\nThe script which originally called ADD_NAVMESH_REQUIRED_REGION is \"%s\", and the script which has just called REMOVE_NAVMESH_REQUIRED_REGION is \"%s\"", 
				m_NavMeshRequiredRegions[NMR_Script].m_ScriptName, pScriptName);
			return false;
		}
*/
		Printf("Removing scripted navmesh region at (%.1f, %.1f) - script \"%s\"\n",
			m_NavMeshRequiredRegions[NMR_Script].m_vOrigin.x, m_NavMeshRequiredRegions[NMR_Script].m_vOrigin.y, pScriptName );
#endif

		m_NavMeshRequiredRegions[NMR_Script].m_bActive = false;
		m_NavMeshRequiredRegions[NMR_Script].m_iThreadId = THREAD_INVALID;

#if __BANK
		strcpy(m_NavMeshRequiredRegions[NMR_Script].m_ScriptName, "NMR_Script");
#endif
		return true;
	}

	return false;
}

// Returns whether the navmeshes around the given area are loaded
bool CPathServer::AreAllNavMeshRegionsLoaded(aiNavDomain domain)
{
	LOCK_NAVMESH_DATA;
	LOCK_STORE_LOADED_MESHES;

	const atArray<s32> & loadedMeshes = GetNavMeshStore(domain)->GetLoadedMeshes();

	s32 i;
	for(i=0; i<loadedMeshes.GetCount(); i++)
	{
		CNavMesh * pNavMesh = GetNavMeshFromIndex(loadedMeshes[i], domain);

		if(!pNavMesh)
		{
			return false;
		}
	}

#ifndef GTA_ENGINE
	UNLOCK_STORE_LOADED_MESHES;
	UNLOCK_NAVMESH_DATA;
#endif

	return true;
}


#endif // GTA_ENGINE

void CPathServer::RemoveAllNavMeshRegions()
{
	for(s32 r=0; r<NAVMESH_MAX_REQUIRED_REGIONS; r++)
	{
		if(r != NMR_GameplayOrigin)
		{
			m_NavMeshRequiredRegions[r].m_bActive = false;
#ifdef GTA_ENGINE
			m_NavMeshRequiredRegions[r].m_iThreadId = THREAD_INVALID;
#endif
		}
	}

#if __BANK
	strcpy(m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_ScriptName, "NMR_GameplayOrigin");
	strcpy(m_NavMeshRequiredRegions[NMR_NetworkRespawnMgr].m_ScriptName, "NMR_NetworkRespawnMgr");
	strcpy(m_NavMeshRequiredRegions[NMR_Script].m_ScriptName, "NMR_Script");
#endif
}

#ifdef GTA_ENGINE
#if __SCRIPT_MEM_CALC
// TODO: Move this functionality into aiMeshStore
void CPathServer::CountScriptMemoryUsage(u32& nVirtualSize, u32& nPhysicalSize)
{
	nVirtualSize = 0;
	nPhysicalSize = 0;

	if(!m_NavMeshRequiredRegions[NMR_Script].m_bActive)
		return;

	atArray<strIndex> streamingIndices;
	aiNavMeshStore * pStore = GetNavMeshStore(kNavDomainRegular);

	const float fMeshHalfSize = pStore->GetMeshSize() * 0.5f;
	const float fMeshRadius = rage::Sqrtf((fMeshHalfSize*fMeshHalfSize)+(fMeshHalfSize*fMeshHalfSize));

	const float fScriptLoadProximity = m_NavMeshRequiredRegions[NMR_Script].m_fNavMeshLoadRadius + fMeshRadius;
	const Vector3 vScriptRegionMin(m_NavMeshRequiredRegions[NMR_Script].m_vOrigin.x - fScriptLoadProximity, m_NavMeshRequiredRegions[NMR_Script].m_vOrigin.y - fScriptLoadProximity, 0.0f);
	const Vector3 vScriptRegionMax(m_NavMeshRequiredRegions[NMR_Script].m_vOrigin.x + fScriptLoadProximity, m_NavMeshRequiredRegions[NMR_Script].m_vOrigin.y + fScriptLoadProximity, 0.0f);

	const float fGameplayLoadProximity = m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_fNavMeshLoadRadius + fMeshRadius;

	float x,y;
	Vector2 vCenter;
	Vector2 vDiff;

	for(y=vScriptRegionMin.y; y<vScriptRegionMax.y; y+=pStore->GetMeshSize())
	{
		for(x=vScriptRegionMin.x; x<vScriptRegionMax.x; x+=pStore->GetMeshSize())
		{
			const strLocalIndex iMeshIndex = strLocalIndex(pStore->GetMeshIndexFromPosition( Vector3(x,y,0.0f) ));
			if(iMeshIndex != pStore->GetMeshIndexNone())
			{
				const int iY = iMeshIndex.Get() / pStore->GetNumMeshesInX();
				const int iX = iMeshIndex.Get() - (iY * pStore->GetNumMeshesInY());

				vCenter.x = ((((float)iX) * pStore->GetMeshSize()) + CPathServerExtents::m_vWorldMin.x) + fMeshHalfSize;
				vCenter.y = ((((float)iY) * pStore->GetMeshSize()) + CPathServerExtents::m_vWorldMin.y) + fMeshHalfSize;

				vDiff.x = vCenter.x - m_NavMeshRequiredRegions[NMR_Script].m_vOrigin.x;
				vDiff.y = vCenter.y - m_NavMeshRequiredRegions[NMR_Script].m_vOrigin.y;

				const float fDistSqrFromScriptOrigin = vDiff.Mag2();

				if(fDistSqrFromScriptOrigin < fScriptLoadProximity*fScriptLoadProximity)
				{
					vDiff.x = vCenter.x - m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_vOrigin.x;
					vDiff.y = vCenter.y - m_NavMeshRequiredRegions[NMR_GameplayOrigin].m_vOrigin.y;

					if(vDiff.Mag2() > fGameplayLoadProximity*fGameplayLoadProximity)
					{
						streamingIndices.Grow() = pStore->GetStreamingIndex(iMeshIndex).Get();
					}
				}
			}
		}
	}

	strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndices, nVirtualSize, nPhysicalSize, NULL, 0, true);
}
#endif
#endif

#ifndef GTA_ENGINE
bool
CPathServer::LoadAllHierarchicalData(const char * UNUSED_PARAM(pPathForHierarchicalData))
{
	/*
	const s32 iSectorStep = m_iNumSectorsPerNavMesh;
	const s32 iBlockSize = m_iNumSectorsPerNavNodes;
	const s32 iStepSize = iSectorStep * iBlockSize;
	s32 x, y;

	for(y=0; y<WORLD_DEPTHINSECTORS; y+=iStepSize)
	{
		for(x=0; x<WORLD_WIDTHINSECTORS; x+=iStepSize)
		{
			const u32 index = GetNavMeshIndexFromSector(x, y);

			if(index != NAVMESH_NAVMESH_INDEX_NONE)
			{
				Assert(!m_StaticNavMeshStore.GetHierarchicalNodes(index));

				char fileName[256];
				sprintf(fileName, "%s/hier_nav/%i_%i.ihn", pPathForHierarchicalData, x, y);
				CHierarchicalNavData * pHierNav = CHierarchicalNavData::Load(fileName);
				if(pHierNav)
				{
					m_StaticNavMeshStore.SetHierarchicalNodes(index, pHierNav);
				}
			}
		}
	}
	*/

	return true;
}
#endif

//****************************************************************************
//	LoadMeshes
//	This function loads in ALL the CNavMeshes.  This could be a lot of data.
//****************************************************************************
#ifndef GTA_ENGINE
bool
CPathServer::LoadAllMeshes(const char * pPathForNavMeshes)
{
	const aiNavDomain domain = kNavDomainRegular;

	int x,y;
	int iSectorStep = m_pNavMeshStores[domain]->GetNumSectorsPerMesh();

	u32 iTotalNumVerticesInWorld = 0;
	u32 iTotalNumPolysInWorld = 0;
	u32 iTotalNumCoverPointsInWorld = 0;

	u32 iGreatestNumCoverPointsInAnyNavMesh = 0;


	char sectorName[256];
	char sectorFilename[256];
#if __HIERARCHICAL_NODES_ENABLED
	char hierNavName[256];
#endif
	for(y=0; y<CPathServerExtents::GetWorldDepthInSectors(); y+=iSectorStep)
	{
		for(x=0; x<CPathServerExtents::GetWorldWidthInSectors(); x+=iSectorStep)
		{
			const u32 index = GetNavMeshIndexFromSector(x, y, domain);
			Assert(!m_pNavMeshStores[domain]->GetMeshByIndex(index));

			sprintf(sectorName, "navmesh[%i][%i].inv", x, y);
			sprintf(sectorFilename, "%s\\navmesh[%i][%i].inv", pPathForNavMeshes, x, y);

			CNavMesh * pNavMesh = CNavMesh::LoadBinary(sectorFilename);
			if(!pNavMesh)
				continue;

			if(pNavMesh->GetIndexOfMesh()!=index)
			{
				pNavMesh->SetIndexOfMesh(index);
				for(u32 p=0; p<pNavMesh->GetNumPolys(); p++)
					pNavMesh->GetPoly(p)->SetNavMeshIndex(index);

				for(u32 a=0; a<pNavMesh->GetSizeOfPools(); a++)
				{
					pNavMesh->GetAdjacentPolysArray().Get(a)->SetNavMeshIndex(index,  pNavMesh->GetAdjacentMeshes());
					pNavMesh->GetAdjacentPolysArray().Get(a)->SetOriginalNavMeshIndex(index,  pNavMesh->GetAdjacentMeshes());
				}
			}

#if __ENSURE_THAT_POLY_QUICK_CENTROIDS_ARE_REALLY_WITHIN_POLYS
			pNavMesh->CheckAllQuickPolyCentroidsAreWithinPolys();
#endif

			m_pNavMeshStores[domain]->Set(index, pNavMesh);

			// total up
			iTotalNumVerticesInWorld += pNavMesh->GetNumVertices();
			iTotalNumPolysInWorld += pNavMesh->GetNumPolys();
			iTotalNumCoverPointsInWorld += pNavMesh->GetNumCoverPoints();

			if(pNavMesh->GetNumCoverPoints() > iGreatestNumCoverPointsInAnyNavMesh)
				iGreatestNumCoverPointsInAnyNavMesh = pNavMesh->GetNumCoverPoints();

#if __HIERARCHICAL_NODES_ENABLED
			// Load hierarchical navigation data
			if(ms_bLoadAllHierarchicalData)
			{
				sprintf(hierNavName, "%s\\%i_%i.ihn", pPathForNavMeshes, x, y);
				CHierarchicalNavData * pHierNav = CHierarchicalNavData::Load(hierNavName);

				if(pHierNav)
				{
					s32 iNodexIndes = m_pNavNodesStores[domain]->GetMeshIndexFromSectorCoords(x, y);
					m_pNavNodesStore->Set(iNodexIndes, pHierNav);
				}
			}
#endif
		}
	}

#if __DEV
#if __WIN32PC

	char tmp[256];
	Printf("*****************************************************************\n");
	Printf("*****************************************************************\n");

	sprintf(tmp, "Total num vertices in all navmeshes : %i\n", iTotalNumVerticesInWorld);
	Printf(tmp);
	OutputDebugString(tmp);
	sprintf(tmp, "Total num polys in all navmeshes : %i\n", iTotalNumPolysInWorld);
	Printf(tmp);
	OutputDebugString(tmp);
	sprintf(tmp, "Total num cover-points in all navmeshes : %i\n", iTotalNumCoverPointsInWorld);
	Printf(tmp);
	OutputDebugString(tmp);
	sprintf(tmp, "Greatest num coverpoints in any navmesh : %i\n(max is 8191 stored in 13 bits in TLinkedCoverPoint)\n", iGreatestNumCoverPointsInAnyNavMesh);
	Printf(tmp);
	OutputDebugString(tmp);

	Printf("*****************************************************************\n");
	Printf("*****************************************************************\n");

#endif
#endif

	return true;
}
#endif // #ifndef GTA_ENGINE

//******************************************************************************************
//	UnloadAllMeshes
//	Removes & deletes all the navmeshes
//******************************************************************************************

bool CPathServer::UnloadAllMeshes(bool bForceRemove)
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision())
		return true;
#endif

	LOCK_NAVMESH_DATA;

	s32 i;

	for(int meshDataSet = 0; meshDataSet < kNumNavDomains; meshDataSet++)
	{
		if(!fwPathServer::GetIsNavDomainEnabled((aiNavDomain)meshDataSet))
			continue;

		aiNavMeshStore* pStore = m_pNavMeshStores[meshDataSet];

		// Main-map navmeshes
		for(i=0; i<pStore->GetMaxMeshIndex(); i++)
		{
			if(pStore->GetMeshByIndex(i))
			{
				pStore->SetIsMeshRequired(i, false);

//				if(bForceRemove)
//				{
//					CStreaming::RemoveObject( pStore->GetStreamingIndex(i), pStore->GetStreamingModuleId() );
//				}
			}

			pStore->GetStreamingModule()->ClearRequiredFlag(i, STRFLAG_DONTDELETE);

			if(bForceRemove)
			{
				CStreaming::RemoveObject( pStore->GetStreamingIndex(strLocalIndex(i)), pStore->GetStreamingModuleId() );
				pStore->GetStreamingModule()->ResetAllRefs( pStore->GetStreamingIndex(strLocalIndex(i)) );
			}
		}
	}

	UNLOCK_NAVMESH_DATA;

	return true;
}


//******************************************************************************************
//	Shutdown
//	This function shuts down CPathServer.  In turn it shuts down the CPathServerThread -
//	this may not be instant : it sets a flag within the class, which is examined between
//	each path request.  Therefore it may have to wait for pending request to complete
//	before terminating.
//******************************************************************************************
void
CPathServer::Shutdown(void)
{
	if(ms_bInitialised)
	{
		m_PathServerThread.Close();

		UnloadAllMeshes(true);

		// The main store of dynamic navmeshes for the streaming
		m_DynamicNavMeshStore.Shutdown();

		CDynamicObjectsContainer::Shutdown();

		if(m_pTessellationNavMesh)
		{
			delete m_pTessellationNavMesh;
			m_pTessellationNavMesh = NULL;
		}
		if(m_PolysTessellatedFrom)
		{
			delete[] m_PolysTessellatedFrom;
			m_PolysTessellatedFrom = NULL;
		}

		for(int meshDataSet = 0; meshDataSet < kNumNavDomains; meshDataSet++)
		{
			if(fwPathServer::GetIsNavDomainEnabled((aiNavDomain)meshDataSet))
				m_pNavMeshStores[meshDataSet]->Shutdown();
		}


#if !__FINAL
		if(m_RequestTimer) {
			delete m_RequestTimer;
			m_RequestTimer = NULL;
		}
		if(m_PedGenTimer) {
			delete m_PedGenTimer;
			m_PedGenTimer = NULL;
		}
		if(m_NavMeshLoadingTimer){
			delete m_NavMeshLoadingTimer;
			m_NavMeshLoadingTimer = NULL;
		}
		if(m_NavMesh2ndLoadingTimer){
			delete m_NavMesh2ndLoadingTimer;
			m_NavMesh2ndLoadingTimer = NULL;
		}
		if(m_MainGameThreadStallTimer){
			delete m_MainGameThreadStallTimer;
			m_MainGameThreadStallTimer = NULL;
		}
		if(m_MiscTimer){
			delete m_MiscTimer;
			m_MiscTimer = NULL;
		}
		if(m_ImmediateModeTimer){
			delete m_ImmediateModeTimer;
			m_ImmediateModeTimer = NULL;
		}
#ifdef GTA_ENGINE
		if(m_ExtractCoverPointsTimer) {
			delete m_ExtractCoverPointsTimer;
			m_ExtractCoverPointsTimer = NULL;
		}
		if(m_TrackObjectsTimer) {
			delete m_TrackObjectsTimer;
			m_TrackObjectsTimer = NULL;
		}
#endif
#endif

		if(m_pImmediateModePrioriryQueue)
		{
			delete m_pImmediateModePrioriryQueue;
			m_pImmediateModePrioriryQueue = NULL;
		}

		m_GameInterface = NULL;
	}


	ms_bInitialised = false;
}

#if __HIERARCHICAL_NODES_ENABLED
u32 CPathServer::NavMeshIndexToNavNodesIndex(const TNavMeshIndex iNavMesh)
{
	Assert(iNavMesh != NAVMESH_NAVMESH_INDEX_NONE);

	s32 iSectorX, iSectorY;

	m_pNavMeshStore->GetSectorCoordsFromMeshIndex(iNavMesh, iSectorX, iSectorY);
	const u32 iNodesIndex = m_pNavNodesStore->GetMeshIndexFromSectorCoords(iSectorX, iSectorY);

	return iNodesIndex;
}
#endif
#if __HIERARCHICAL_NODES_ENABLED
u32 CPathServer::NavNodesIndexToNavMeshIndex(const TNavMeshIndex iNavNodes)
{
	Assert(iNavNodes != NAVMESH_NODE_INDEX_NONE);

	s32 iSectorX, iSectorY;

	m_pNavNodesStore->GetSectorCoordsFromMeshIndex(iNavNodes, iSectorX, iSectorY);
	const u32 iNavIndex = m_pNavMeshStore->GetMeshIndexFromSectorCoords(iSectorX, iSectorY);

	return iNavIndex;
}
#endif
bool CPathServer::GetDoNavMeshesAdjoin(const u32 iNavMesh1, const u32 iNavMesh2, aiNavDomain domain)
{
	const int iNumSectorsPerNavMesh = m_pNavMeshStores[domain]->GetNumSectorsPerMesh();

	s32 iX1, iY1, iX2, iY2;
	GetSectorFromNavMeshIndex(iNavMesh1, iX1, iY1, domain);
	GetSectorFromNavMeshIndex(iNavMesh2, iX2, iY2, domain);

	if(iX1==iX2 && (iY1==iY2-iNumSectorsPerNavMesh || iY1==iY2+iNumSectorsPerNavMesh))
		return true;
	if(iY1==iY2 && (iX1==iX2-iNumSectorsPerNavMesh || iX1==iX2+iNumSectorsPerNavMesh))
		return true;
	return false;
}

//****************************************************************************
//	GetNavMeshIndexFromPosition
//	Given coordinates in worldspace, this function returns the index of the
//	navmesh at that location.  This indexes into the navmeshes array.
//****************************************************************************
u32
CPathServer::GetNavMeshIndexFromPosition(const Vector3 & vPos, aiNavDomain domain)
{
//	int iSectorX = WORLD_WORLDTOSECTORX(vPos.x);
//	int iSectorY = WORLD_WORLDTOSECTORX(vPos.y);
	const int iSectorX = CPathServerExtents::GetWorldToSectorX(vPos.x);
	const int iSectorY = CPathServerExtents::GetWorldToSectorY(vPos.y);

	return GetNavMeshIndexFromSector(iSectorX, iSectorY, domain);
}

CHierarchicalNavData * CPathServer::GetHierarchicalNavFromNavMeshIndex(const u32 UNUSED_PARAM(index))
{
#if __HIERARCHICAL_NODES_ENABLED
	if(index <= NAVMESH_MAX_MAP_INDEX)
	{
		const s32 iHierIndex = NavMeshIndexToNavNodesIndex(index);
		return m_pNavNodesStore->GetMeshByIndex(iHierIndex);
	}
#endif
	return NULL;
}

u32
CPathServer::GetAdjacentNavMeshIndex(u32 iCurrentNavMeshIndex, eNavMeshEdge iDir, aiNavDomain domain)
{
	aiNavMeshStore* pStore = m_pNavMeshStores[domain];

	s32 iY = iCurrentNavMeshIndex / pStore->GetNumMeshesInX();
	s32 iX = iCurrentNavMeshIndex - (iY * pStore->GetNumMeshesInY());

	s32 iAdjIndex;

	switch(iDir)
	{
		case eNegX:
			iAdjIndex = (iX > 0) ? iCurrentNavMeshIndex - 1 : NAVMESH_NAVMESH_INDEX_NONE;
			break;
		case ePosX:
			iAdjIndex = (iX < pStore->GetNumMeshesInX()-1) ? iCurrentNavMeshIndex + 1 : NAVMESH_NAVMESH_INDEX_NONE;
			break;
		case eNegY:
			iAdjIndex = (iY > 0) ? iCurrentNavMeshIndex - pStore->GetNumMeshesInY() : NAVMESH_NAVMESH_INDEX_NONE;
			break;
		case ePosY:
			iAdjIndex = (iY < pStore->GetNumMeshesInY()-1) ? iCurrentNavMeshIndex + pStore->GetNumMeshesInY() : NAVMESH_NAVMESH_INDEX_NONE;
			break;
		default:
			Assert(0);
			iAdjIndex = NAVMESH_NAVMESH_INDEX_NONE;
	}

	return iAdjIndex;
}

//****************************************************************************
//	Finds up to 4 navmeshes intersecting the give position & radius.
//	The radius MUST therefore be less than 1/2 a navmesh width, or we may
//	intersect more than 4 navmeshes.
//	pIndices = an array of 4 TNavMeshIndex's
//****************************************************************************
s32
CPathServer::GetNavMeshesIntersectingPosition(const Vector3 & vPos, const float fRadius, TNavMeshIndex * pIndices, aiNavDomain domain)
{
	static const Vector2 vOffsets[9] =
	{
		Vector2(0,0),
		Vector2(-1.0f,0),
		Vector2(0,1.0f),
		Vector2(1.0f,0),
		Vector2(0,-1.0f),
		Vector2(0.70710f,0.70710f),
		Vector2(0.70710f,-0.70710f),
		Vector2(-0.70710f,-0.70710f),
		Vector2(-0.70710f,0.70710f)
	};
	s32 iNumIndices=0;
	Vector3 vTestPos;
	vTestPos.z = vPos.z;
	s32 i,j;
	for(i=0; i<9; i++)
	{
		vTestPos.x = vPos.x + (vOffsets[i].x * fRadius);
		vTestPos.y = vPos.y + (vOffsets[i].y * fRadius);
		u32 iNavMeshIndex = CPathServer::GetNavMeshIndexFromPosition(vTestPos, domain);
		for(j=0; j<iNumIndices; j++)
			if(pIndices[j] == iNavMeshIndex) break;
		if(j==iNumIndices)
			pIndices[iNumIndices++] = iNavMeshIndex;
		Assert(iNumIndices<=4);
	}
	Assert(iNumIndices<=4);
	return iNumIndices;
}

CNavMesh * CPathServer::FindClosestLoadedNavMeshToPosition(const Vector3 & vPos, aiNavDomain domain)
{
	float fClosestDistSqr = FLT_MAX;
	CNavMesh * pClosestNavMesh = NULL;

#ifdef GTA_ENGINE
	LOCK_STORE_LOADED_MESHES;
#endif

	const atArray<s32> & loadedMeshes = GetNavMeshStore((aiNavDomain)domain)->GetLoadedMeshes();

	for(int i=0; i<loadedMeshes.GetCount(); i++)
	{
		const TNavMeshIndex iNavMesh = loadedMeshes[i];
		CNavMesh * pNavMesh = GetNavMeshStore((aiNavDomain)domain)->GetMeshByIndex(iNavMesh);
		if(pNavMesh)
		{
			Assert(pNavMesh->GetQuadTree());

			Vector3 vMid = (pNavMesh->GetQuadTree()->m_Mins + pNavMesh->GetQuadTree()->m_Maxs) * 0.5f;
			vMid.z = vPos.z;
			Vector3 vDiff = vMid - vPos;
			float fDistSqr = vDiff.Mag2();

			if(fDistSqr < fClosestDistSqr)
			{
				fClosestDistSqr = fDistSqr;
				pClosestNavMesh = pNavMesh;
			}
		}
	}

	return pClosestNavMesh;
}

#if 0	// Disabled, appears to be unused at the moment. /FF

void
CPathServer::UpdateDynamicNavMeshMatrix(CDynamicNavMeshEntry & entry)
{
	if(entry.m_pNavMesh && !entry.m_bCurrentlyCopyingMatrix)
	{
		Assert(entry.GetEntity());
		if(!entry.GetEntity())
			return;

		entry.m_bCurrentlyCopyingMatrix = true;

		entry.m_pNavMesh->GetMatrixRef() = entry.m_Matrix;
		const Vector3 & vEntityPos = entry.m_Matrix.d;

		CEntityBoundAI bound(
			*entry.GetEntity(),
			vEntityPos.z,
			PATHSERVER_PED_RADIUS,
			true
		);
		bound.GetPlanes(entry.m_vBoundingPlanes, entry.m_fBoundingPlaneDists);

#ifdef GTA_ENGINE
		float fPushThroughGroundAmt = entry.GetEntity()->GetIsTypeVehicle() ? 0.5f : 0.25f;
		float fTopZ = bound.GetTopZ() + 4.0f;	// Add an extra amount onto top for dynamic navmeshes
		float fBottomZ = bound.GetBottomZ() - fPushThroughGroundAmt;
#else
		float fTopZ = vEntityPos.z + 1.0f;
		float fBottomZ = vEntityPos.z - 1.0f;
#endif

		// Construct the top & bottom planes for this object, using world UP & DOWN as normal vectors
		entry.m_vBoundingPlanes[4] = Vector3(0.0f, 0.0f, 1.0f);
		entry.m_fBoundingPlaneDists[4] = - DotProduct(entry.m_vBoundingPlanes[4], Vector3(vEntityPos.x, vEntityPos.y, fTopZ));
		entry.m_vBoundingPlanes[5] = Vector3(0.0f, 0.0f, -1.0f);
		entry.m_fBoundingPlaneDists[5] = - DotProduct(entry.m_vBoundingPlanes[5], Vector3(vEntityPos.x, vEntityPos.y, fBottomZ));

		entry.m_bCurrentlyCopyingMatrix = false;
	}
}

#endif	// 0

// Simplified interface for basic paths
TPathHandle
CPathServer::RequestPath(const Vector3 & vPathStart, const Vector3 & vPathEnd, u64 iPathStyleFlags, float fCompletionRadius, fwEntity * pPed)
{
	TRequestPathStruct reqStruct;
	reqStruct.m_vPathStart = vPathStart;
	reqStruct.m_vPathEnd = vPathEnd;
	reqStruct.m_iFlags = iPathStyleFlags;
	reqStruct.m_fCompletionRadius = fCompletionRadius;
	reqStruct.m_pPed = pPed;

	return RequestPath(reqStruct);
}

//*********************************************************************
//	RequestPath
//	A general-purpose path request function, which is called by the
//	more specific request functions.
//	The 'iFlags' field is a bitset which specifies which type of path
//	is being requested, and the "vPathStart,vPathEnd,vReferenceVector &
//	fRederenceDistance" variables are interpreted differently depending
//	upon the path-type.
//	The influence spheres provide a method for steering a path away-
//	from or towards areas of the map, whilst still maintaining the
//	overall path objetives.
//	pEntityPathIsOn is passed in if this path is known to exist upon
//	a dynamic navmesh attached to an entity.
//*********************************************************************
TPathHandle
CPathServer::RequestPath(
	const Vector3 & vPathStart,
	const Vector3 & vPathEnd,
	const Vector3 & vReferenceVector,
	float fReferenceDistance,
	u64 iFlags,
	float fCompletionRadius,
	u32 iNumInfluenceSpheres,
	TInfluenceSphere * pInfluenceSpheres,
	fwEntity * pPed,
	const fwEntity * pEntityPathIsOn)
{
	TRequestPathStruct reqStruct;
	reqStruct.m_vPathStart = vPathStart;
	reqStruct.m_vPathEnd = vPathEnd;
	reqStruct.m_vReferenceVector = vReferenceVector;
	reqStruct.m_fReferenceDistance = fReferenceDistance;
	reqStruct.m_iFlags = iFlags;
	reqStruct.m_fCompletionRadius = fCompletionRadius;
	reqStruct.m_iNumInfluenceSpheres = iNumInfluenceSpheres;
	sysMemCpy(reqStruct.m_InfluenceSpheres, pInfluenceSpheres, sizeof(TInfluenceSphere)*iNumInfluenceSpheres);
	reqStruct.m_pPed = pPed;
	reqStruct.m_pEntityPathIsOn = pEntityPathIsOn;

	return RequestPath(reqStruct);
}

TPathHandle CPathServer::RequestPath(const TRequestPathStruct & reqStruct)
{
	Assert(ms_bGameInSession);
	if(!ms_bGameInSession)
		return PATH_HANDLE_NULL;

#ifdef GTA_ENGINE
#if __DEV
	const bool bStartPosIsValid = rage::FPIsFinite(reqStruct.m_vPathStart.x) && rage::FPIsFinite(reqStruct.m_vPathStart.y) && rage::FPIsFinite(reqStruct.m_vPathStart.z);
	const bool bEndPosIsValid = rage::FPIsFinite(reqStruct.m_vPathEnd.x) && rage::FPIsFinite(reqStruct.m_vPathEnd.y) && rage::FPIsFinite(reqStruct.m_vPathEnd.z);
	// A quick sanity check on the path request endpoints.
	Assert(bStartPosIsValid && bEndPosIsValid);
	Assert(rage::FPIsFinite(reqStruct.m_vReferenceVector.x) && rage::FPIsFinite(reqStruct.m_vReferenceVector.y) && rage::FPIsFinite(reqStruct.m_vReferenceVector.z));
	Assert(rage::FPIsFinite(reqStruct.m_fReferenceDistance));
#endif
#endif

#if !__FINAL
	if(m_RequestTimer)
	{
		m_RequestTimer->Reset();
		m_RequestTimer->Start();
	}
#endif

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i = m_iNextPathIndexToStartFrom;

	m_iNextPathIndexToStartFrom++;
	if(m_iNextPathIndexToStartFrom >= MAX_NUM_PATH_REQUESTS)
		m_iNextPathIndexToStartFrom = 0;

	int c = MAX_NUM_PATH_REQUESTS;

	while(c)
	{
		if( m_PathRequests[i].IsReadyForUse() )
		{
			m_PathRequests[i].Clear();
#ifdef GTA_ENGINE
			m_PathRequests[i].m_PedWaitingForThisRequest = reqStruct.m_pPed;
#endif
			m_PathRequests[i].m_NavDomain = reqStruct.m_NavDomain;
			m_PathRequests[i].m_PathResultInfo.Clear();
			m_PathRequests[i].m_iType = EPath;
			m_PathRequests[i].m_hHandle = m_iNextHandle++;

#ifdef GTA_ENGINE
			m_PathRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
#endif

			m_PathRequests[i].m_bSlotEmpty = false;
			m_PathRequests[i].m_bComplete = false;
			m_PathRequests[i].m_bRequestActive = false;
			m_PathRequests[i].m_iNumPoints = 0;

			m_PathRequests[i].m_vUnadjustedStartPoint = reqStruct.m_vPathStart;
			m_PathRequests[i].m_vUnadjustedEndPoint = reqStruct.m_vPathEnd;

			m_PathRequests[i].m_vPathStart = reqStruct.m_vPathStart;
			m_PathRequests[i].m_vPathEnd = reqStruct.m_vPathEnd;
			m_PathRequests[i].m_vPolySearchDir = reqStruct.m_vPolySearchDir;

			m_PathRequests[i].m_vReferenceVector = reqStruct.m_vReferenceVector;
			m_PathRequests[i].m_fReferenceDistance = reqStruct.m_fReferenceDistance;
			m_PathRequests[i].m_fInitialReferenceDistance = reqStruct.m_fReferenceDistance;

			Assert(!(reqStruct.m_StartNavmeshAndPoly.m_iNavMeshIndex==NAVMESH_NAVMESH_INDEX_NONE && reqStruct.m_StartNavmeshAndPoly.m_iPolyIndex!=NAVMESH_POLY_INDEX_NONE));
			Assert(!(reqStruct.m_StartNavmeshAndPoly.m_iNavMeshIndex!=NAVMESH_NAVMESH_INDEX_NONE && reqStruct.m_StartNavmeshAndPoly.m_iPolyIndex==NAVMESH_POLY_INDEX_NONE));

			m_PathRequests[i].m_StartNavmeshAndPoly = reqStruct.m_StartNavmeshAndPoly;
#ifdef GTA_ENGINE
			m_PathRequests[i].m_EntityEndPosition = reqStruct.m_pPositionEntity;
#endif
			m_PathRequests[i].m_bScriptedRoute = ((reqStruct.m_iFlags & PATH_FLAG_SCRIPTED_ROUTE)!=0);
			m_PathRequests[i].m_bHighPrioRoute = ((reqStruct.m_iFlags & PATH_FLAG_HIGH_PRIO_ROUTE)!=0);
			m_PathRequests[i].m_bNeverLeaveWater = ((reqStruct.m_iFlags & PATH_FLAG_NEVER_LEAVE_WATER)!=0);
			m_PathRequests[i].m_bNeverLeaveDeepWater = ((reqStruct.m_iFlags & PATH_FLAG_NEVER_LEAVE_DEEP_WATER)!=0);
			m_PathRequests[i].m_bEnsureLosBeforeEnding = ((reqStruct.m_iFlags & PATH_FLAG_ENSURE_LOS_BEFORE_ENDING)!=0);
			m_PathRequests[i].m_bExpandStartEndPolyTessellationRadius = ((reqStruct.m_iFlags & PATH_FLAG_EXPAND_START_END_TESSELLATION_RADIUS)!=0);
			m_PathRequests[i].m_bKeepUpdatingPedStartPosition = ((reqStruct.m_iFlags & PATH_FLAG_KEEP_UPDATING_PED_START_POSITION)!=0);
			m_PathRequests[i].m_bPullFromEdgeExtra = ((reqStruct.m_iFlags & PATH_FLAG_PULL_FROM_EDGE_EXTRA)!=0);
			m_PathRequests[i].m_bSofterFleeHeuristics = ((reqStruct.m_iFlags & PATH_FLAG_SOFTER_FLEE_HEURISTICS)!=0);
			m_PathRequests[i].m_bAvoidTrainTracks = ((reqStruct.m_iFlags & PATH_FLAG_AVOID_TRAIN_TRACKS)!=0);

			m_PathRequests[i].m_fDistAheadOfPed = reqStruct.m_fDistAheadOfPed;

			if((reqStruct.m_iFlags & PATH_FLAG_FLEE_TARGET)!=0)
			{
				m_PathRequests[i].m_bFleeTarget = true;
			}
			else
			{
				m_PathRequests[i].m_bFleeTarget = false;

				Assertf(!m_PathRequests[i].m_bAvoidTrainTracks, "NB: The heuristics for PATH_FLAG_AVOID_TRAIN_TRACKS are currently only tuned for fleeing peds.");
			}

			if((reqStruct.m_iFlags & PATH_FLAG_WANDER)!=0)
			{
				m_PathRequests[i].m_bWander = true;
				m_PathRequests[i].m_vReferenceVector.z = 0.0f;				// Reference vector is a 2d unit vector here
				m_PathRequests[i].m_vReferenceVector.Normalize();
			}
			else
			{
				m_PathRequests[i].m_bWander = false;
			}

			m_PathRequests[i].m_vCoverOrigin = reqStruct.m_vCoverOrigin;

			// Allow the limit on path search extents to be overridden (with care!)
			m_PathRequests[i].m_bUseLargerSearchExtents = ((reqStruct.m_iFlags & PATH_FLAG_USE_LARGER_SEARCH_EXTENTS)!=0);
			m_PathRequests[i].m_bDontLimitSearchExtents = ((reqStruct.m_iFlags & PATH_FLAG_DONT_LIMIT_SEARCH_EXTENTS)!=0);

#if !__FINAL
			m_PathRequests[i].m_bClimbObjects = ms_bAllowObjectClimbing;
			m_PathRequests[i].m_bPushObjects = ms_bAllowObjectPushing;
			m_PathRequests[i].m_iFrameRequestCompleted = 0;
#ifdef GTA_ENGINE
			m_PathRequests[i].m_iFrameRequestIssued = fwTimer::GetFrameCount();
#endif
#endif

			// Can't flee & wander at the same time..
			Assert(!(m_PathRequests[i].m_bFleeTarget && m_PathRequests[i].m_bWander));

			m_PathRequests[i].m_bUseBestAlternateRouteIfNoneFound = ((reqStruct.m_iFlags & PATH_FLAG_USE_BEST_ALTERNATE_ROUTE_IF_NONE_FOUND)!=0);
			m_PathRequests[i].m_bPreferDownHill = ((reqStruct.m_iFlags & PATH_FLAG_PREFER_DOWNHILL)!=0);
			m_PathRequests[i].m_bPreferPavements = ((reqStruct.m_iFlags & PATH_FLAG_PREFER_PAVEMENTS)!=0);
			m_PathRequests[i].m_bNeverLeavePavements = ((reqStruct.m_iFlags & PATH_FLAG_NEVER_LEAVE_PAVEMENTS)!=0);
			m_PathRequests[i].m_bNeverClimbOverStuff = ((reqStruct.m_iFlags & PATH_FLAG_NEVER_CLIMB_OVER_STUFF)!=0);
			m_PathRequests[i].m_bNeverDropFromHeight = ((reqStruct.m_iFlags & PATH_FLAG_NEVER_DROP_FROM_HEIGHT)!=0);
			m_PathRequests[i].m_bMayUseFatalDrops = ((reqStruct.m_iFlags & PATH_FLAG_MAY_USE_FATAL_DROPS)!=0);
			m_PathRequests[i].m_bNeverUseLadders = ((reqStruct.m_iFlags & PATH_FLAG_NEVER_USE_LADDERS)!=0);
			m_PathRequests[i].m_bDontAvoidDynamicObjects = ((reqStruct.m_iFlags & PATH_FLAG_DONT_AVOID_DYNAMIC_OBJECTS)!=0);
			m_PathRequests[i].m_bGoAsFarAsPossibleIfNavMeshNotLoaded = ((reqStruct.m_iFlags & PATH_FLAG_SELECT_CLOSEST_LOADED_NAVMESH_TO_TARGET)!=0);
			m_PathRequests[i].m_bDoPostProcessToPreserveSlopeInfo = ((reqStruct.m_iFlags & PATH_FLAG_PRESERVE_SLOPE_INFO_IN_PATH)!=0);
			m_PathRequests[i].m_bSmoothSharpCorners = ((reqStruct.m_iFlags & PATH_FLAG_CUT_SHARP_CORNERS)!=0);
			m_PathRequests[i].m_bDynamicNavMeshRoute = ((reqStruct.m_iFlags & PATH_FLAG_DYNAMIC_NAVMESH_ROUTE)!=0);
			m_PathRequests[i].m_bNeverEnterWater = ((reqStruct.m_iFlags & PATH_FLAG_NEVER_ENTER_WATER)!=0);
			m_PathRequests[i].m_bNeverStartInWater = ((reqStruct.m_iFlags & PATH_FLAG_NEVER_START_IN_WATER)!=0);
			m_PathRequests[i].m_bReduceObjectBoundingBoxes = ((reqStruct.m_iFlags & PATH_FLAG_REDUCE_OBJECT_BBOXES)!=0);
			m_PathRequests[i].m_bIgnoreNonSignificantObjects = ((reqStruct.m_iFlags & PATH_FLAG_IGNORE_NON_SIGNIFICANT_OBJECTS)!=0);
			m_PathRequests[i].m_bIfStartNotOnPavementAllowDropsAndClimbs = ((reqStruct.m_iFlags & PATH_FLAG_IF_NOT_ON_PAVEMENT_ALLOW_DROPS_AND_CLIMBS)!=0);

			m_PathRequests[i].m_bFleeNeverEndInWater = ((reqStruct.m_iFlags & PATH_FLAG_FLEE_NEVER_END_IN_WATER)!=0);

			m_PathRequests[i].m_bUseMaxSlopeNavigable = (reqStruct.m_fMaxSlopeNavigable != 0.0f);
			m_PathRequests[i].m_bUseDirectionalCover = ((reqStruct.m_iFlags & PATH_FLAG_USE_DIRECTIONAL_COVER)!=0);
			m_PathRequests[i].m_bFavourEnclosedSpaces = ((reqStruct.m_iFlags & PATH_FLAG_FAVOUR_ENCLOSED_SPACES)!=0);
			m_PathRequests[i].m_bAvoidPotentialExplosions = ((reqStruct.m_iFlags & PATH_FLAG_AVOID_POTENTIAL_EXPLOSIONS)!=0);
			m_PathRequests[i].m_bAvoidTearGas = ((reqStruct.m_iFlags & PATH_FLAG_AVOID_TEAR_GAS)!=0);
			m_PathRequests[i].m_bAllowToNavigateUpSteepPolygons = ((reqStruct.m_iFlags & PATH_FLAG_ALLOW_TO_NAVIGATE_UP_STEEP_POLYGONS)!=0);
			m_PathRequests[i].m_bAllowToPushVehicleDoorsClosed = ((reqStruct.m_iFlags & PATH_FLAG_ALLOW_TO_PUSH_VEHICLE_DOORS_CLOSED)!=0);
			m_PathRequests[i].m_bMissionPed = ((reqStruct.m_iFlags & PATH_FLAG_MISSION_PED)!=0);
			m_PathRequests[i].m_bDeactivateObjectsIfCantResolveEndPoints = ((reqStruct.m_iFlags & PATH_FLAG_DEACTIVATE_OBJECTS_IF_CANT_RESOLVE_ENDPOINTS)!=0);
			m_PathRequests[i].m_bDontAvoidFire = ((reqStruct.m_iFlags & PATH_FLAG_DONT_AVOID_FIRE)!=0);
			m_PathRequests[i].m_bCoverFinderPath = ((reqStruct.m_iFlags & PATH_FLAG_COVERFINDER)!=0);

			m_PathRequests[i].m_fMaxDistanceToAdjustPathStart = reqStruct.m_fMaxDistanceToAdjustPathStart;
			m_PathRequests[i].m_fMaxDistanceToAdjustPathEnd = reqStruct.m_fMaxDistanceToAdjustPathEnd;
			

			// Randomising of points can be triggered either by path flag, or by a ped reset flag
			// (now applied later, through CPathServerGameInterfaceGta).
			m_PathRequests[i].m_bRandomisePoints = ((reqStruct.m_iFlags & PATH_FLAG_RANDOMISE_POINTS)!=0);

			Assert(reqStruct.m_fMaxSlopeNavigable >= 0.0f && reqStruct.m_fMaxSlopeNavigable <= PI/2.0f);
			m_PathRequests[i].m_fMaxSlopeNavigable = reqStruct.m_fMaxSlopeNavigable;

			m_PathRequests[i].m_fClampMaxSearchDistance = reqStruct.m_fClampMaxSearchDistance;

			// Initialise as if there is no underwater navigation. If there is, OverridePathRequestParameters()
			// may change these.
			m_PathRequests[i].m_fDistanceBelowStartToLookForPoly = CPathServerThread::ms_fNormalDistBelowToLookForPoly;
			m_PathRequests[i].m_fDistanceAboveStartToLookForPoly = CPathServerThread::ms_fNormalDistAboveToLookForPoly;
			m_PathRequests[i].m_fDistanceBelowEndToLookForPoly = CPathServerThread::ms_fNormalDistBelowToLookForPoly;
			m_PathRequests[i].m_fDistanceAboveEndToLookForPoly = CPathServerThread::ms_fNormalDistAboveToLookForPoly;

#if !__FINAL
			if(ms_bCutCornersOffAllPaths) m_PathRequests[i].m_bSmoothSharpCorners = true;
			m_PathRequests[i].m_hHandleThisWas = m_PathRequests[i].m_hHandle;

			if(m_bDisableObjectAvoidance)
				m_PathRequests[i].m_bDontAvoidDynamicObjects = true;
#endif

			// Tesselation code used for avoidance of dynamic objects hasn't been adapted
			// to work with other data sets than the regular mesh, it's not clear yet
			// if we'll need that or not. Thus, in that case, we force m_bDontAvoidDynamicObjects
			// to true, to make use of existing code paths to not do avoidance.
			if(reqStruct.m_NavDomain != kNavDomainRegular)
			{
				m_PathRequests[i].m_bDontAvoidDynamicObjects = true;
			}

			m_PathRequests[i].m_bProblemPathStartsAndEndsOnSamePoly = false;
			m_PathRequests[i].m_fPathSearchCompletionRadius = reqStruct.m_fCompletionRadius;
			m_PathRequests[i].m_fEntityRadius = reqStruct.m_fEntityRadius;
			m_PathRequests[i].m_fEntityRadius = Clamp(m_PathRequests[i].m_fEntityRadius, PATHSERVER_PED_RADIUS, PATHSERVER_MAX_PED_RADIUS);

			m_PathRequests[i].m_bUseVariableEntityRadius = Abs(m_PathRequests[i].m_fEntityRadius - PATHSERVER_PED_RADIUS) > 0.01f;
			m_PathRequests[i].m_bHasAdjustedDynamicObjectsMinMaxForWidth = false;


			// Store spheres of influence which the path will favour or avoid, depending upon the fWeighting member
			m_PathRequests[i].m_iNumInfluenceSpheres = Min((int)reqStruct.m_iNumInfluenceSpheres, MAX_NUM_INFLUENCE_SPHERES);

			if(m_PathRequests[i].m_iNumInfluenceSpheres)
			{
				sysMemCpy(m_PathRequests[i].m_InfluenceSpheres, reqStruct.m_InfluenceSpheres, sizeof(TInfluenceSphere) * m_PathRequests[i].m_iNumInfluenceSpheres);
			}
#ifdef GTA_ENGINE
			if(m_PathRequests[i].m_bAvoidPotentialExplosions && m_PathRequests[i].m_iNumInfluenceSpheres < MAX_NUM_INFLUENCE_SPHERES)
			{
				m_PathRequests[i].m_iNumInfluenceSpheres += CreateInfluenceSpheresForPotentialExplosions(
					m_PathRequests[i].m_vPathStart,
					&m_PathRequests[i].m_InfluenceSpheres[ m_PathRequests[i].m_iNumInfluenceSpheres ],
					MAX_NUM_INFLUENCE_SPHERES - m_PathRequests[i].m_iNumInfluenceSpheres );

				Assert(m_PathRequests[i].m_iNumInfluenceSpheres <= MAX_NUM_INFLUENCE_SPHERES);
			}
			if(m_PathRequests[i].m_bAvoidTearGas && m_PathRequests[i].m_iNumInfluenceSpheres < MAX_NUM_INFLUENCE_SPHERES)
			{
				m_PathRequests[i].m_iNumInfluenceSpheres += CreateInfluenceSpheresForTearGas(
					m_PathRequests[i].m_vPathStart,
					&m_PathRequests[i].m_InfluenceSpheres[ m_PathRequests[i].m_iNumInfluenceSpheres ],
					MAX_NUM_INFLUENCE_SPHERES - m_PathRequests[i].m_iNumInfluenceSpheres);

				Assert(m_PathRequests[i].m_iNumInfluenceSpheres <= MAX_NUM_INFLUENCE_SPHERES);
			}
			m_PathRequests[i].m_iNumInfluenceSpheres = Min((int)m_PathRequests[i].m_iNumInfluenceSpheres, MAX_NUM_INFLUENCE_SPHERES);
#endif
			m_PathRequests[i].m_bIgnoreTypeVehicles = reqStruct.m_bIgnoreTypeVehicles;
			m_PathRequests[i].m_bIgnoreTypeObjects = reqStruct.m_bIgnoreTypeObjects;

			m_PathRequests[i].m_iNumIncludeObjects = reqStruct.m_iNumIncludeObjects;
			m_PathRequests[i].m_iNumExcludeObjects = reqStruct.m_iNumExcludeObjects;

#ifdef GTA_ENGINE
			int o;
			for(o=0; o<reqStruct.m_iNumIncludeObjects; o++)
			{
				switch(reqStruct.m_IncludeObjects[o]->GetType())
				{
					case ENTITY_TYPE_VEHICLE:
						m_PathRequests[i].m_IncludeObjects[o] = ((CVehicle*)reqStruct.m_IncludeObjects[o])->GetPathServerDynamicObjectIndex();
						break;
					case ENTITY_TYPE_OBJECT:
						m_PathRequests[i].m_IncludeObjects[o] = ((CObject*)reqStruct.m_IncludeObjects[o])->GetPathServerDynamicObjectIndex();
						break;
					default:
						Assertf(false, "Entity type isn't supported in pathserver.");
						m_PathRequests[i].m_iNumIncludeObjects--;
				}
			}
			for(o=0; o<reqStruct.m_iNumExcludeObjects; o++)
			{
				switch(reqStruct.m_ExcludeObjects[o]->GetType())
				{
					case ENTITY_TYPE_VEHICLE:
						m_PathRequests[i].m_ExcludeObjects[o] = ((CVehicle*)reqStruct.m_ExcludeObjects[o])->GetPathServerDynamicObjectIndex();
						break;
					case ENTITY_TYPE_OBJECT:
						m_PathRequests[i].m_ExcludeObjects[o] = ((CObject*)reqStruct.m_ExcludeObjects[o])->GetPathServerDynamicObjectIndex();
						break;
					default:
						Assertf(false, "Entity type isn't supported in pathserver.");
						m_PathRequests[i].m_iNumExcludeObjects--;
				}
			}
#endif // GTA_ENGINE

#if __DEV
			m_PathRequests[i].m_pContext = (void*)reqStruct.m_pPed;
#endif

			m_PathRequests[i].m_iPedRandomSeed = 0;

			// Set the default movement costs
			m_PathRequests[i].m_MovementCosts.SetDefault();

			//m_PathRequests[i].m_pEntityThisPathIsOn = reqStruct.m_pEntityPathIsOn;
			m_PathRequests[i].m_iIndexOfDynamicNavMesh = NAVMESH_NAVMESH_INDEX_NONE;

			m_PathRequests[i].m_PathResultInfo.m_iDynObjIndex = DYNAMIC_OBJECT_INDEX_NONE;

			m_PathRequests[i].m_PathResultInfo.m_vClosestPointFoundToTarget = reqStruct.m_vPathStart;

			// Now, give the game code a chance to modify the path request, through the
			// fwPathServerGameInterface object. In the first version of this for GTA,
			// this may change iPedRandomSeed, m_MovementCosts, m_bRandomisePoints,
			// and the dynamic mesh info, depending on the user ped and the entity it
			// may be standing on.
			GetGameInterface().OverridePathRequestParameters(reqStruct, m_PathRequests[i]);

			// Do this after OverridePathRequestParameters
			CPathServer::ModifyMovementCosts(&m_PathRequests[i].m_MovementCosts, m_PathRequests[i].m_bWander, m_PathRequests[i].m_bFleeTarget);

			// Fix errors/performance issues arising from certain bad usage configurations of influence spheres
			ValidateInfluenceSpheres(&m_PathRequests[i]);

			// Ready
			m_PathRequests[i].m_bWasAborted = false;
			m_PathRequests[i].m_bRequestPending = true;

#if !__FINAL
			if(m_RequestTimer)
			{
				m_RequestTimer->Stop();
				m_fTimeTakenToIssueRequestsInMSecs += (float) m_RequestTimer->GetTimeMS();
			}
#endif
			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#ifdef GTA_ENGINE
			if(CPathServer::ms_bUseEventsForRequests)
			{
				// Signal the event which the pathserver thread is waiting upon
				// sysIpcSetEvent(CPathServer::m_PathRequestEvent);
				sysIpcSignalSema(CPathServer::m_PathRequestSema);
			}
#endif
			return m_PathRequests[i].m_hHandle;
		}

		i++;
		if(i >= MAX_NUM_PATH_REQUESTS)
			i = 0;
		c--;
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#if !__FINAL
	if(m_RequestTimer)
	{
		m_RequestTimer->Stop();
		m_fTimeTakenToIssueRequestsInMSecs += (float) m_RequestTimer->GetTimeMS();
	}
#endif

#if AI_OPTIMISATIONS_OFF || AI_VEHICLE_OPTIMISATIONS_OFF || AI_NAVIGATION_OPTIMSATIONS_OFF || NAVMESH_OPTIMISATIONS_OFF
	Errorf("CPathServer : WARNING - no available slots for path request!");
#endif

	return PATH_HANDLE_NULL;
}


// NAME : ValidateInfluenceSpheres
// PURPOSE : Check for & fix cases where we have influence spheres which may cause problems for navigation:
// 1) a repelling sphere is placed over a route end point (will cause route to exhaust entire search area before completing)
void CPathServer::ValidateInfluenceSpheres(CPathRequest * UNUSED_PARAM(pPathRequest))
{

}

// FUNCTION: CreateInfluenceSpheresForPotentialExplosions
// PURPOSE: Scan active fires which may result in an explosion, and create influence sphere for them.
// Given that we have only a limited number of influence spheres - sort fires by proximity to path start
// and add as many as we can.
// TODO: Cull those which are out of range wrt search extents? (will require path extents to be calculated up-front)

#ifdef GTA_ENGINE

#define NAV_MAX_AVOID_FIRES 8
atArray<TPotentialExplosion> g_explosiveFires(0, NAV_MAX_AVOID_FIRES+1);

int CPathServer::CreateInfluenceSpheresForPotentialExplosions(const Vector3 & vPathStart, TInfluenceSphere * influenceSpheres, const int iMaxNumInfluenceSpheres)
{
	if(iMaxNumInfluenceSpheres <= 0)
		return 0;

	g_explosiveFires.clear();

	// Hacky cull to ignore fires which are sufficiently far from the path's origin (they will explode before we get there)
	const float fMaxRangeFromStart = 40.0f*40.0f;

	int i,f;
	for (i=0; i<g_fireMan.GetNumActiveFires(); i++)
	{
		CFire* pFire = g_fireMan.GetActiveFire(i);
		if(pFire)
		{
			// don't add multiple fires from the same entity
			bool bAlreadyAvoided = false;
			if(pFire->GetEntity())
			{
				for(f=0; f<g_explosiveFires.GetCount(); f++)
				{
					if(g_explosiveFires[f].pFire->GetEntity() == pFire->GetEntity())
					{
						bAlreadyAvoided = true;
						break;
					}
				}
			}

			if(bAlreadyAvoided)
				continue;

			if (pFire->GetFireType()==FIRETYPE_REGD_VEH_PETROL_TANK || pFire->GetFireType()==FIRETYPE_TIMED_PETROL_POOL || pFire->GetFireType()==FIRETYPE_TIMED_PETROL_TRAIL)
			{
				const Vec3V vVec = pFire->GetPositionWorld() - RCC_VEC3V(vPathStart);
				const float fDistSqr = MagSquared(vVec).Getf();

				if(fDistSqr < fMaxRangeFromStart)
				{
					for(f=0; f<g_explosiveFires.GetCount(); f++)
					{
						if(fDistSqr < g_explosiveFires[f].fDistSqr)
						{
							TPotentialExplosion newFire;
							newFire.pFire = pFire;
							newFire.fDistSqr = fDistSqr;

							g_explosiveFires.Insert(f) = newFire;
							if(g_explosiveFires.GetCount() > NAV_MAX_AVOID_FIRES)
								g_explosiveFires.Resize(NAV_MAX_AVOID_FIRES);
							break;
						}
					}
					if(f == g_explosiveFires.GetCount() && g_explosiveFires.GetCount() < NAV_MAX_AVOID_FIRES)
					{
						TPotentialExplosion newFire;
						newFire.pFire = pFire;
						newFire.fDistSqr = fDistSqr;
						g_explosiveFires.Push(newFire);
					}
				}
			}
		}
	}

	const int iMinCount = Min(iMaxNumInfluenceSpheres, NAV_MAX_AVOID_FIRES);
	if(g_explosiveFires.GetCount() > iMinCount)
		g_explosiveFires.Resize(iMinCount);

	for(i=0; i<g_explosiveFires.GetCount(); i++)
	{
		float blastRadius = 0.0f;
		switch(g_explosiveFires[i].pFire->GetFireType())
		{
			case FIRETYPE_REGD_VEH_PETROL_TANK:
				if(g_explosiveFires[i].pFire->GetEntity())
					blastRadius = g_explosiveFires[i].pFire->GetEntity()->GetBoundRadius();
				break;
			case FIRETYPE_TIMED_PETROL_TRAIL:
			case FIRETYPE_TIMED_PETROL_POOL:
				blastRadius = 3.0f;	// MAGIC!
				break;
			default:
				Assertf(0, "found unsupported fire type");
				break;
		}

		static float sfSafeDistance = 5.0f;
		static dev_float fInnerWeighting = 100.0f;
		static dev_float fOuterWeighting = 100.0f;

		influenceSpheres[i].SetOrigin( VEC3V_TO_VECTOR3( g_explosiveFires[i].pFire->GetPositionWorld() ) );
		influenceSpheres[i].SetRadius( blastRadius + sfSafeDistance );
		influenceSpheres[i].SetInnerWeighting(fInnerWeighting);
		influenceSpheres[i].SetOuterWeighting(fOuterWeighting);
	}

	return g_explosiveFires.GetCount();
}

// FUNCTION: CreateInfluenceSpheresForTearGas
// PURPOSE: Scan active tear gas clouds, and create influence sphere for them.
int CPathServer::CreateInfluenceSpheresForTearGas(const Vector3& vPathStart, TInfluenceSphere* influenceSpheres, const int iMaxNumInfluenceSpheres)
{
	// First check to see if there is no room for influence spheres to be added
	if( iMaxNumInfluenceSpheres <= 0 )
	{
		// in which case there is no work to do here
		return 0;
	}

	// Create a list for active tear gas explosions
	static const int NAV_MAX_AVOID_TEAR_GAS = 8;
	atFixedArray<phGtaExplosionInst*, NAV_MAX_AVOID_TEAR_GAS> activeTearGasExplosionList(NAV_MAX_AVOID_TEAR_GAS);

	// Query the explosion manager
	int numExplosionsFound = CExplosionManager::FindExplosionsByTag(EXP_TAG_SMOKEGRENADE, &activeTearGasExplosionList[0], NAV_MAX_AVOID_TEAR_GAS);

	// Create a list for the gas clouds
	// Size this list according to the specified number of influences to add
	atArray<TGasCloud> gasPositionsList(0, iMaxNumInfluenceSpheres);

	// Traverse the list of active explosions
	for(int iExplosion = 0; iExplosion < numExplosionsFound; iExplosion++)
	{
		phGtaExplosionInst* pExplosionInst = activeTearGasExplosionList[iExplosion];
		if( pExplosionInst )
		{
			// Compute distance squared from path start to explosion position
			Vec3V candidateExplosionPos = pExplosionInst->GetPosWld();
			float fCandidateDistSq = DistSquared(VECTOR3_TO_VEC3V(vPathStart), candidateExplosionPos).Getf();

			// Setup the element to consider adding to the list
			TGasCloud candidateCloud;
			candidateCloud.worldPos = candidateExplosionPos;
			candidateCloud.fDistSqr = fCandidateDistSq;

			// If the list is empty
			if( gasPositionsList.GetCount() == 0 )
			{
				// append to the list 
				gasPositionsList.Push(candidateCloud);
			}
			else // list is not empty
			{
				// scan from back to front to find an insertion point, if any
				for(int scanIndex=gasPositionsList.GetCount()-1; scanIndex >= 0; scanIndex--)
				{
					// if we find an entry more distant than the candidate
					if( fCandidateDistSq < gasPositionsList[scanIndex].fDistSqr )
					{
						// check if the list is full
						if( gasPositionsList.GetCount() >= gasPositionsList.GetCapacity() )
						{
							// Delete the most distant entry (always tail of the list)
							gasPositionsList.Delete(gasPositionsList.GetCount()-1);
						}

						// replace entry with this closer explosion data
						gasPositionsList.Insert(scanIndex) = candidateCloud;

						// break out of the scanIndex traversal
						break;
					}
				}
			}
		}
	}

	const int iMinCount = Min(iMaxNumInfluenceSpheres, NAV_MAX_AVOID_TEAR_GAS);
	if(gasPositionsList.GetCount() > iMinCount)
		gasPositionsList.Resize(iMinCount);

	// Now we have the list of gas positions closest to path start
	// Use this list to fill in the influences requested
	for(int i = 0; i < gasPositionsList.GetCount(); i++)
	{
		static dev_float fTearGasRadius = 15.0f; // MAGIC!
		static dev_float fTearGasInnerWeighting = 100.0f; // MAGIC!
		static dev_float fTearGasOuterWeighting = 100.0f; // MAGIC!
		influenceSpheres[i].SetOrigin(VEC3V_TO_VECTOR3(gasPositionsList[i].worldPos));
		influenceSpheres[i].SetRadius(fTearGasRadius);
		influenceSpheres[i].SetInnerWeighting(fTearGasInnerWeighting);
		influenceSpheres[i].SetOuterWeighting(fTearGasOuterWeighting);
	}

	// Report the number of influence spheres processed
	return gasPositionsList.GetCount();
}

#endif // GTA_ENGINE

TPathHandle CPathServer::RequestGrid(const Vector3 & vOrigin, u32 iSize, float fResolution, void * DEV_ONLY(pContext), aiNavDomain domain)
{
	Assert(ms_bGameInSession);
	if(!ms_bGameInSession)
		return PATH_HANDLE_NULL;

	if(iSize > WALKRNDOBJGRID_MAXSIZE) iSize = WALKRNDOBJGRID_MAXSIZE;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i = m_iNextGridIndexToStartFrom;

	m_iNextGridIndexToStartFrom++;
	if(m_iNextGridIndexToStartFrom >= MAX_NUM_GRID_REQUESTS)
		m_iNextGridIndexToStartFrom = 0;

	int c = MAX_NUM_GRID_REQUESTS;

	while(c)
	{
		if( m_GridRequests[i].IsReadyForUse() )
		{
			m_GridRequests[i].Clear();

			m_GridRequests[i].m_hHandle = m_iNextHandle++;
			m_GridRequests[i].m_NavDomain = domain;

#ifdef GTA_ENGINE
			m_GridRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
#endif

			m_GridRequests[i].m_bSlotEmpty = false;
			m_GridRequests[i].m_bComplete = false;
			m_GridRequests[i].m_bRequestActive = false;

			m_GridRequests[i].m_iType = EGrid;

			m_GridRequests[i].m_WalkRndObjGrid.m_vOrigin = vOrigin;
			m_GridRequests[i].m_WalkRndObjGrid.m_iSize = (u16)iSize;
			m_GridRequests[i].m_WalkRndObjGrid.m_iCentreCell = (u16)(iSize/2);
			m_GridRequests[i].m_WalkRndObjGrid.m_fResolution = fResolution;

#if __DEV
			m_GridRequests[i].m_pContext = pContext;
#endif

			m_GridRequests[i].m_bWasAborted = false;
			m_GridRequests[i].m_bRequestPending = true;

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#ifdef GTA_ENGINE
			if(CPathServer::ms_bUseEventsForRequests)
			{
				// Signal the event which the pathserver thread is waiting upon
				sysIpcSignalSema(CPathServer::m_PathRequestSema);
			}
#if !__FINAL
			m_GridRequests[i].m_hHandleThisWas = m_GridRequests[i].m_hHandle;
			m_GridRequests[i].m_iFrameRequestIssued = fwTimer::GetFrameCount();
			m_GridRequests[i].m_iFrameRequestCompleted = 0;
#endif
#endif
			return m_GridRequests[i].m_hHandle;
		}

		i++;
		if(i >= MAX_NUM_GRID_REQUESTS)
			i = 0;
		c--;
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#if AI_OPTIMISATIONS_OFF || AI_VEHICLE_OPTIMISATIONS_OFF || AI_NAVIGATION_OPTIMSATIONS_OFF || NAVMESH_OPTIMISATIONS_OFF
	Assertf(0, "CPathServer - no available slots for grid request");
#endif

	return PATH_HANDLE_NULL;
}


TPathHandle CPathServer::RequestLineOfSight(const Vector3 & vStart, const Vector3 & vEnd, float fRadius, bool bDynamicObjects, bool bNoLosAcrossWaterBoundary, bool bStartInWater, int iNumExcludeObjects, CEntity ** ppExcludeObjects, const float fMaxSlopeAngle, void * pContext, aiNavDomain domain)
{
	Vector3 vPts[2] = { vStart, vEnd };

	return RequestLineOfSight(vPts, 2, fRadius, bDynamicObjects, true, bNoLosAcrossWaterBoundary, bStartInWater, iNumExcludeObjects, ppExcludeObjects, fMaxSlopeAngle, pContext, domain);
}

TPathHandle CPathServer::RequestLineOfSight(const Vector3 * vPts, int iNumPts, float fRadius, bool bDynamicObjects, bool bQuitAtFirstLosFail, bool bNoLosAcrossWaterBoundary, bool bStartInWater, int iNumExcludeObjects, CEntity ** GTA_ENGINE_ONLY(ppExcludeObjects), const float fMaxSlopeAngle, void * DEV_ONLY(pContext), aiNavDomain domain)
{
	Assert(ms_bGameInSession);
	if(!ms_bGameInSession)
		return PATH_HANDLE_NULL;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i = m_iNextLosIndexToStartFrom;

	m_iNextLosIndexToStartFrom++;
	if(m_iNextLosIndexToStartFrom >= MAX_NUM_LOS_REQUESTS)
		m_iNextLosIndexToStartFrom = 0;

	int c = MAX_NUM_LOS_REQUESTS;
	int p;

	while(c)
	{
		if(m_LineOfSightRequests[i].IsReadyForUse() )
		{
			m_LineOfSightRequests[i].Clear();

			m_LineOfSightRequests[i].m_hHandle = m_iNextHandle++;

#ifdef GTA_ENGINE
			m_LineOfSightRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
#endif

			// If too many points are passed in, notify the user with an assert, and limit the number
			// to avoid array overruns.
			if(!Verifyf(iNumPts <= MAX_LINEOFSIGHT_POINTS, "Too many points (%d) in pathserver line of sight test, max %d supported.", iNumPts, MAX_LINEOFSIGHT_POINTS))
			{
				iNumPts = MAX_LINEOFSIGHT_POINTS;
			}

			m_LineOfSightRequests[i].m_bSlotEmpty = false;
			m_LineOfSightRequests[i].m_bComplete = false;
			m_LineOfSightRequests[i].m_bRequestActive = false;

			m_LineOfSightRequests[i].m_iType = ELineOfSight;
			m_LineOfSightRequests[i].m_NavDomain = domain;

			m_LineOfSightRequests[i].m_iNumPts = iNumPts;

			for(p=0; p<iNumPts; p++)
			{
				m_LineOfSightRequests[i].m_vPoints[p] = vPts[p];
			}
			for(p=0; p<MAX_LINEOFSIGHT_POINTS; p++)
			{
				m_LineOfSightRequests[i].m_bLosResults[p] = false;
			}

			m_LineOfSightRequests[i].m_fRadius = fRadius;
			m_LineOfSightRequests[i].m_bQuitAtFirstLosFail = bQuitAtFirstLosFail;
			m_LineOfSightRequests[i].m_bNoLosAcrossWaterBoundary = bNoLosAcrossWaterBoundary;
			m_LineOfSightRequests[i].m_bStartsInWater = bStartInWater;
			m_LineOfSightRequests[i].m_bDynamicObjects = bDynamicObjects;

			m_LineOfSightRequests[i].m_iNumExcludeObjects = iNumExcludeObjects;
			m_LineOfSightRequests[i].m_fMaxAngle = fMaxSlopeAngle;

#ifdef GTA_ENGINE
			for(s32 o=0; o<iNumExcludeObjects; o++)
			{
				switch(ppExcludeObjects[o]->GetType())
				{
				case ENTITY_TYPE_VEHICLE:
					m_LineOfSightRequests[i].m_ExcludeObjects[o] = ((CVehicle*)ppExcludeObjects[o])->GetPathServerDynamicObjectIndex();
					break;
				case ENTITY_TYPE_OBJECT:
					m_LineOfSightRequests[i].m_ExcludeObjects[o] = ((CObject*)ppExcludeObjects[o])->GetPathServerDynamicObjectIndex();
					break;
				default:
					Assertf(false, "Entity type isn't supported in pathserver.");
					m_LineOfSightRequests[i].m_iNumExcludeObjects--;
				}
			}
#endif // GTA_ENGINE

#if __DEV
			m_LineOfSightRequests[i].m_pContext = pContext;
#endif

			m_LineOfSightRequests[i].m_bWasAborted = false;
			m_LineOfSightRequests[i].m_bRequestPending = true;

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#ifdef GTA_ENGINE
			if(CPathServer::ms_bUseEventsForRequests)
			{
				// Signal the event which the pathserver thread is waiting upon
				sysIpcSignalSema(CPathServer::m_PathRequestSema);
			}
#if !__FINAL
			m_LineOfSightRequests[i].m_hHandleThisWas = m_LineOfSightRequests[i].m_hHandle;
			m_LineOfSightRequests[i].m_iFrameRequestIssued = fwTimer::GetFrameCount();
			m_LineOfSightRequests[i].m_iFrameRequestCompleted = 0;
#endif
#endif
			return m_LineOfSightRequests[i].m_hHandle;
		}

		i++;
		if(i >= MAX_NUM_LOS_REQUESTS)
			i = 0;
		c--;
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return PATH_HANDLE_NULL;
}

TPathHandle
CPathServer::RequestAudioProperties(const Vector3 & vPosition, TNavMeshAndPoly * pKnownNavmeshPosition, float fRadius, const Vector3 & vDirection, const bool bPriorityRequest, void * DEV_ONLY(pContext), aiNavDomain domain)
{
	Assert(ms_bGameInSession);
	if(!ms_bGameInSession)
		return PATH_HANDLE_NULL;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i = m_iNextAudioIndexToStartFrom;

	m_iNextAudioIndexToStartFrom++;
	if(m_iNextAudioIndexToStartFrom >= MAX_NUM_AUDIO_REQUESTS)
		m_iNextAudioIndexToStartFrom = 0;

	int c = MAX_NUM_AUDIO_REQUESTS;

	while(c)
	{
		if(m_AudioRequests[i].IsReadyForUse() )
		{
			m_AudioRequests[i].Clear();

			m_AudioRequests[i].m_hHandle = m_iNextHandle++;

#ifdef GTA_ENGINE
			m_AudioRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
#endif

			m_AudioRequests[i].m_bSlotEmpty = false;
			m_AudioRequests[i].m_bComplete = false;
			m_AudioRequests[i].m_bRequestActive = false;
			m_AudioRequests[i].m_iType = EAudioProperties;
			m_AudioRequests[i].m_NavDomain = domain;

			m_AudioRequests[i].m_bPriorityRequest = bPriorityRequest;

			m_AudioRequests[i].m_vPosition = vPosition;
			m_AudioRequests[i].m_vDirection = vDirection;
			m_AudioRequests[i].m_fRadius = fRadius;

#if __DEV
			m_AudioRequests[i].m_pContext = pContext;
#endif
			if(pKnownNavmeshPosition && pKnownNavmeshPosition->m_iNavMeshIndex!=NAVMESH_NAVMESH_INDEX_NONE && pKnownNavmeshPosition->m_iPolyIndex!=NAVMESH_POLY_INDEX_NONE)
			{
				m_AudioRequests[i].m_KnownNavmeshPosition = *pKnownNavmeshPosition;
			}
			else
			{
				m_AudioRequests[i].m_KnownNavmeshPosition.Reset();
			}

			m_AudioRequests[i].m_bWasAborted = false;
			m_AudioRequests[i].m_bRequestPending = true;

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#ifdef GTA_ENGINE
			if(CPathServer::ms_bUseEventsForRequests)
			{
				// Signal the event which the pathserver thread is waiting upon
				sysIpcSignalSema(CPathServer::m_PathRequestSema);
			}
#if !__FINAL
			m_AudioRequests[i].m_hHandleThisWas = m_AudioRequests[i].m_hHandle;
			m_AudioRequests[i].m_iFrameRequestIssued = fwTimer::GetFrameCount();
			m_AudioRequests[i].m_iFrameRequestCompleted = 0;
#endif
#endif
			return m_AudioRequests[i].m_hHandle;
		}

		i++;
		if(i >= MAX_NUM_AUDIO_REQUESTS)
			i = 0;
		c--;
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return PATH_HANDLE_NULL;
}


// Requests a flood-fill search, to find the closest accessible sheltered navmesh poly within the given radius
TPathHandle
CPathServer::RequestClosestShelteredPolySearch(const Vector3 & vStartPos, const float fMaxRadius, void * pContext)
{
	return RequestClosestViaFloodFill(vStartPos, fMaxRadius, CFloodFillRequest::EFindClosestShelteredPolyFloodFill, true, false, false, pContext);
}

// Requests a flood-fill search, to find the closest accessible unsheltered navmesh poly within the given radius
TPathHandle
CPathServer::RequestClosestUnshelteredPolySearch(const Vector3 & vStartPos, const float fMaxRadius, void * pContext)
{
	return RequestClosestViaFloodFill(vStartPos, fMaxRadius, CFloodFillRequest::EFindClosestUnshelteredPolyFloodFill, true, false, false, pContext);
}

// Requests a flood-fill search, to find the closest accessible sheltered navmesh poly within the given radius
TPathHandle
CPathServer::RequestCalcAreaUnderfootSearch(const Vector3 & vStartPos, const float fMaxRadius, void * pContext)
{
	return RequestClosestViaFloodFill(vStartPos, fMaxRadius, CFloodFillRequest::ECalcAreaUnderfoot, false, false, false, pContext);
}

// Requests a flood-fill search, to find the closest accessible carnode within the given radius
TPathHandle
CPathServer::RequestClosestCarNodeSearch(const Vector3 & vStartPos, const float fMaxRadius, void * pContext)
{
	return RequestClosestViaFloodFill(vStartPos, fMaxRadius, CFloodFillRequest::EFindClosestCarNodeFloodFill, false, true, true, pContext);
}

TPathHandle 
CPathServer::RequestHasNearbyPavementSearch(const Vector3& vStartPos, const float fMaxRadius, void* pContext)
{
	return RequestClosestViaFloodFill(vStartPos, fMaxRadius, CFloodFillRequest::EFindNearbyPavementFloodFill, false, false, false, pContext);
}

// Requests a flood-fill search, to find the closest accessible subtype within the given radius
TPathHandle CPathServer::RequestClosestViaFloodFill(const Vector3 & vStartPos, const float fMaxRadius, CFloodFillRequest::EType eFloodFillType, const bool bCheckDynamicObjects, const bool bUseClimbsAndDrops, const bool bUseLadders, void * DEV_ONLY(pContext), aiNavDomain domain)
{
	Assert(ms_bGameInSession);
	if(!ms_bGameInSession)
		return PATH_HANDLE_NULL;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i = m_iNextFloodFillIndexToStartFrom;

	m_iNextFloodFillIndexToStartFrom++;
	if(m_iNextFloodFillIndexToStartFrom >= MAX_NUM_FLOODFILL_REQUESTS)
		m_iNextFloodFillIndexToStartFrom = 0;

	int c = MAX_NUM_FLOODFILL_REQUESTS;

	while(c)
	{
		if(m_FloodFillRequests[i].IsReadyForUse() )
		{
			m_FloodFillRequests[i].Clear();

			m_FloodFillRequests[i].m_hHandle = m_iNextHandle++;

#ifdef GTA_ENGINE
			m_FloodFillRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
#endif

			m_FloodFillRequests[i].m_bSlotEmpty = false;
			m_FloodFillRequests[i].m_bComplete = false;
			m_FloodFillRequests[i].m_bRequestActive = false;
			m_FloodFillRequests[i].m_iType = EFloodFill;
			m_FloodFillRequests[i].m_NavDomain = domain;
			m_FloodFillRequests[i].m_FloodFillType = eFloodFillType;

			m_FloodFillRequests[i].m_bConsiderDynamicObjects = bCheckDynamicObjects;
			m_FloodFillRequests[i].m_bUseClimbsAndDrops = bUseClimbsAndDrops;
			m_FloodFillRequests[i].m_bUseLadders = bUseLadders;

			m_FloodFillRequests[i].m_vStartPos = vStartPos;
			m_FloodFillRequests[i].m_fMaxRadius = fMaxRadius;

#if __DEV
			m_FloodFillRequests[i].m_pContext = pContext;
#endif

			m_FloodFillRequests[i].m_bWasAborted = false;
			m_FloodFillRequests[i].m_bRequestPending = true;

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#ifdef GTA_ENGINE
			if(CPathServer::ms_bUseEventsForRequests)
			{
				// Signal the event which the pathserver thread is waiting upon
				sysIpcSignalSema(CPathServer::m_PathRequestSema);
			}
#if !__FINAL
			m_FloodFillRequests[i].m_hHandleThisWas = m_FloodFillRequests[i].m_hHandle;
			m_FloodFillRequests[i].m_iFrameRequestIssued = fwTimer::GetFrameCount();
			m_FloodFillRequests[i].m_iFrameRequestCompleted = 0;
#endif
#endif
			return m_FloodFillRequests[i].m_hHandle;
		}

		i++;
		if(i >= MAX_NUM_FLOODFILL_REQUESTS)
			i = 0;
		c--;
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return PATH_HANDLE_NULL;
}

// Requests a clear-area search, to locate a clear patch of navmesh within search extents
TPathHandle CPathServer::RequestClearArea(const Vector3 & vSearchOrigin, const float fSearchRadiusXY, const float fSearchDistZ, const float fDesiredClearRadius, const float fOptionalMinimumRadius, const bool bConsiderDynamicObjects, const bool bSearchInteriors, const bool bSearchExterior, const bool bConsiderWater, const bool bConsiderSheltered, void * DEV_ONLY(pContext), aiNavDomain domain)
{
	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i = m_iNextClearAreaIndexToStartFrom;

	m_iNextClearAreaIndexToStartFrom++;
	if(m_iNextClearAreaIndexToStartFrom >= MAX_NUM_CLEARAREA_REQUESTS)
		m_iNextClearAreaIndexToStartFrom = 0;

	int c = MAX_NUM_CLEARAREA_REQUESTS;

	while(c)
	{
		if(m_ClearAreaRequests[i].IsReadyForUse() )
		{
			m_ClearAreaRequests[i].Clear();

			m_ClearAreaRequests[i].m_hHandle = m_iNextHandle++;

#ifdef GTA_ENGINE
			m_ClearAreaRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
#endif

			m_ClearAreaRequests[i].m_bSlotEmpty = false;
			m_ClearAreaRequests[i].m_bComplete = false;
			m_ClearAreaRequests[i].m_bRequestActive = false;
			m_ClearAreaRequests[i].m_iType = EClearArea;
			m_ClearAreaRequests[i].m_NavDomain = domain;

			m_ClearAreaRequests[i].m_vSearchOrigin = vSearchOrigin;
			m_ClearAreaRequests[i].m_fSearchRadiusXY = fSearchRadiusXY;
			m_ClearAreaRequests[i].m_fSearchDistZ = fSearchDistZ;
			m_ClearAreaRequests[i].m_fDesiredClearAreaRadius = fDesiredClearRadius;
			m_ClearAreaRequests[i].m_fMinimumDistanceFromOrigin = fOptionalMinimumRadius;

			m_ClearAreaRequests[i].m_bConsiderDynamicObjects = bConsiderDynamicObjects;
			m_ClearAreaRequests[i].m_bConsiderInterior = bSearchInteriors;
			m_ClearAreaRequests[i].m_bConsiderExterior = bSearchExterior;
			m_ClearAreaRequests[i].m_bConsiderWater = bConsiderWater;
			m_ClearAreaRequests[i].m_bConsiderSheltered = bConsiderSheltered;

#if __DEV
			m_ClearAreaRequests[i].m_pContext = pContext;
#endif

			m_ClearAreaRequests[i].m_bWasAborted = false;
			m_ClearAreaRequests[i].m_bRequestPending = true;

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#ifdef GTA_ENGINE
				if(CPathServer::ms_bUseEventsForRequests)
				{
					// Signal the event which the pathserver thread is waiting upon
					sysIpcSignalSema(CPathServer::m_PathRequestSema);
				}
#if !__FINAL
				m_ClearAreaRequests[i].m_hHandleThisWas = m_ClearAreaRequests[i].m_hHandle;
				m_ClearAreaRequests[i].m_iFrameRequestIssued = fwTimer::GetFrameCount();
				m_ClearAreaRequests[i].m_iFrameRequestCompleted = 0;
#endif
#endif
				return m_ClearAreaRequests[i].m_hHandle;
		}

		i++;
		if(i >= MAX_NUM_CLEARAREA_REQUESTS)
			i = 0;
		c--;
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return PATH_HANDLE_NULL;
}

// Requests a clear-area search, to locate a clear patch of navmesh within search extents
TPathHandle CPathServer::RequestClosestPosition(const Vector3 & vSearchOrigin, const float fSearchRadius, const u32 iFlags, const float fZWeightingAbove, const float fZWeightingAtOrBelow, const float fMinimumSpacing, const s32 iNumAvoidSpheres, const spdSphere * pAvoidSpheres, const s32 iMaxNumResults, void * DEV_ONLY(pContext), aiNavDomain domain)
{
	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i = m_iNextClosestPositionIndexToStartFrom;

	m_iNextClosestPositionIndexToStartFrom++;
	if(m_iNextClosestPositionIndexToStartFrom >= MAX_NUM_CLOSESTPOSITION_REQUESTS)
		m_iNextClosestPositionIndexToStartFrom = 0;

	int c = MAX_NUM_CLOSESTPOSITION_REQUESTS;

	while(c)
	{
		if(m_ClosestPositionRequests[i].IsReadyForUse() )
		{
			m_ClosestPositionRequests[i].Clear();

			m_ClosestPositionRequests[i].m_hHandle = m_iNextHandle++;

#ifdef GTA_ENGINE
			m_ClosestPositionRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
#endif

			m_ClosestPositionRequests[i].m_bSlotEmpty = false;
			m_ClosestPositionRequests[i].m_bComplete = false;
			m_ClosestPositionRequests[i].m_bRequestActive = false;
			m_ClosestPositionRequests[i].m_iType = EClosestPosition;
			m_ClosestPositionRequests[i].m_NavDomain = domain;

			//---------------------------------------------------------------------

			Assert(iNumAvoidSpheres < CClosestPositionRequest::MAX_NUM_IGNORE_SPHERES);
			m_ClosestPositionRequests[i].m_iNumAvoidSpheres = Min(iNumAvoidSpheres, CClosestPositionRequest::MAX_NUM_IGNORE_SPHERES);

			sysMemCpy(m_ClosestPositionRequests[i].m_AvoidSpheres, pAvoidSpheres, sizeof(spdSphere)*iNumAvoidSpheres);

			m_ClosestPositionRequests[i].m_vSearchOrigin = vSearchOrigin;
			m_ClosestPositionRequests[i].m_fSearchRadius = fSearchRadius;

			m_ClosestPositionRequests[i].m_fZWeightingAbove = fZWeightingAbove;
			m_ClosestPositionRequests[i].m_fZWeightingAtOrBelow = fZWeightingAtOrBelow;

			Assert(fMinimumSpacing >= 0.0f);
			m_ClosestPositionRequests[i].m_fMinimumSpacing = Max(fMinimumSpacing, 0.0f);

			Assert(iMaxNumResults <= CClosestPositionRequest::MAX_NUM_RESULTS);
			m_ClosestPositionRequests[i].m_iMaxResults = Min(iMaxNumResults, CClosestPositionRequest::MAX_NUM_RESULTS);

			m_ClosestPositionRequests[i].m_bConsiderDynamicObjects = ((iFlags & CClosestPositionRequest::Flag_ConsiderDynamicObjects)!=0);
			m_ClosestPositionRequests[i].m_bConsiderInterior = ((iFlags & CClosestPositionRequest::Flag_ConsiderInterior)!=0);
			m_ClosestPositionRequests[i].m_bConsiderExterior = ((iFlags & CClosestPositionRequest::Flag_ConsiderExterior)!=0);
			m_ClosestPositionRequests[i].m_bConsiderOnlyLand = ((iFlags & CClosestPositionRequest::Flag_ConsiderOnlyLand)!=0);
			m_ClosestPositionRequests[i].m_bConsiderOnlyWater = ((iFlags & CClosestPositionRequest::Flag_ConsiderOnlyWater)!=0);
			m_ClosestPositionRequests[i].m_bConsiderOnlyPavement = ((iFlags & CClosestPositionRequest::Flag_ConsiderOnlyPavement)!=0);
			m_ClosestPositionRequests[i].m_bConsiderOnlyNonIsolated = ((iFlags & CClosestPositionRequest::Flag_ConsiderOnlyNonIsolated)!=0);
			m_ClosestPositionRequests[i].m_bConsiderOnlySheltered = ((iFlags & CClosestPositionRequest::Flag_ConsiderOnlySheltered)!=0);
			m_ClosestPositionRequests[i].m_bConsiderOnlyNetworkSpawn = ((iFlags & CClosestPositionRequest::Flag_ConsiderOnlyNetworkSpawn)!=0);

#if __DEV
			m_ClosestPositionRequests[i].m_pContext = pContext;
#endif

			m_ClosestPositionRequests[i].m_bWasAborted = false;
			m_ClosestPositionRequests[i].m_bRequestPending = true;

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

#ifdef GTA_ENGINE
				if(CPathServer::ms_bUseEventsForRequests)
				{
					// Signal the event which the pathserver thread is waiting upon
					sysIpcSignalSema(CPathServer::m_PathRequestSema);
				}
#if !__FINAL
				m_ClosestPositionRequests[i].m_hHandleThisWas = m_ClosestPositionRequests[i].m_hHandle;
				m_ClosestPositionRequests[i].m_iFrameRequestIssued = fwTimer::GetFrameCount();
				m_ClosestPositionRequests[i].m_iFrameRequestCompleted = 0;
#endif
#endif
				return m_ClosestPositionRequests[i].m_hHandle;
		}

		i++;
		if(i >= MAX_NUM_CLOSESTPOSITION_REQUESTS)
			i = 0;
		c--;
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return PATH_HANDLE_NULL;
}

// Use this version of the function when the type of the request is known.
EPathServerRequestResult CPathServer::IsRequestResultReady(TPathHandle hPath, EPathServerRequestType eType)
{
	// We shouldn't really be calling this with a null handle..
	if(hPath == PATH_HANDLE_NULL)
	{
		Assertf(false, "Called IsRequestResultReady() with PATH_HANDLE_NULL");
		return ERequest_NotFound;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	switch(eType)
	{
		case EPath:
		{
			for(int i=0; i<MAX_NUM_PATH_REQUESTS; i++)
			{
				if(m_PathRequests[i].m_hHandle == hPath)
				{
					if(m_PathRequests[i].m_bComplete)
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_Ready;
					}
					else
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_NotReady;
					}
				}
			}
			break;
		}
		case EGrid:
		{
			for(int i=0; i<MAX_NUM_GRID_REQUESTS; i++)
			{
				if(m_GridRequests[i].m_hHandle == hPath)
				{
					if(m_GridRequests[i].m_bComplete)
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_Ready;
					}
					else
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_NotReady;
					}
				}
			}
			break;
		}
		case ELineOfSight:
		{
			for(int i=0; i<MAX_NUM_LOS_REQUESTS; i++)
			{
				if(m_LineOfSightRequests[i].m_hHandle == hPath)
				{
					if(m_LineOfSightRequests[i].m_bComplete)
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_Ready;
					}
					else
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_NotReady;
					}
				}
			}
			break;
		}
		case EAudioProperties:
		{
			for(int i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
			{
				if(m_AudioRequests[i].m_hHandle == hPath)
				{
					if(m_AudioRequests[i].m_bComplete)
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_Ready;
					}
					else
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_NotReady;
					}
				}
			}
			break;
		}
		case EFloodFill:
		{
			for(int i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
			{
				if(m_FloodFillRequests[i].m_hHandle == hPath)
				{
					if(m_FloodFillRequests[i].m_bComplete)
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_Ready;
					}
					else
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_NotReady;
					}
				}
			}
			break;
		}
		case EClearArea:
		{
			for(int i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
			{
				if(m_ClearAreaRequests[i].m_hHandle == hPath)
				{
					if(m_ClearAreaRequests[i].m_bComplete)
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
							return ERequest_Ready;
					}
					else
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
							return ERequest_NotReady;
					}
				}
			}
			break;
		}
		case EClosestPosition:
		{
			for(int i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
			{
				if(m_ClosestPositionRequests[i].m_hHandle == hPath)
				{
					if(m_ClosestPositionRequests[i].m_bComplete)
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_Ready;
					}
					else
					{
						GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
						return ERequest_NotReady;
					}
				}
			}
			break;
		}
		default:
		{
			Assert(false);
			break;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
	return ERequest_NotFound;
}

EPathServerRequestResult CPathServer::IsRequestResultReady(TPathHandle hPath)
{
	// We shouldn't really be calling this with a null handle..
	if(hPath == PATH_HANDLE_NULL)
	{
		Assertf(false, "Called IsRequestResultReady() with PATH_HANDLE_NULL");
		return ERequest_NotFound;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(int i=0; i<MAX_NUM_PATH_REQUESTS; i++)
	{
		if(m_PathRequests[i].m_hHandle == hPath)
		{
			if(m_PathRequests[i].m_bComplete)
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_Ready;
			}
			else
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_NotReady;
			}
		}
	}
	for(int i=0; i<MAX_NUM_GRID_REQUESTS; i++)
	{
		if(m_GridRequests[i].m_hHandle == hPath)
		{
			if(m_GridRequests[i].m_bComplete)
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_Ready;
			}
			else
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_NotReady;
			}
		}
	}
	for(int i=0; i<MAX_NUM_LOS_REQUESTS; i++)
	{
		if(m_LineOfSightRequests[i].m_hHandle == hPath)
		{
			if(m_LineOfSightRequests[i].m_bComplete)
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_Ready;
			}
			else
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_NotReady;
			}
		}
	}
	for(int i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
	{
		if(m_AudioRequests[i].m_hHandle == hPath)
		{
			if(m_AudioRequests[i].m_bComplete)
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_Ready;
			}
			else
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_NotReady;
			}
		}
	}
	for(int i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
	{
		if(m_FloodFillRequests[i].m_hHandle == hPath)
		{
			if(m_FloodFillRequests[i].m_bComplete)
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_Ready;
			}
			else
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return ERequest_NotReady;
			}
		}
	}
	for(int i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
	{
		if(m_ClearAreaRequests[i].m_hHandle == hPath)
		{
			if(m_ClearAreaRequests[i].m_bComplete)
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
					return ERequest_Ready;
			}
			else
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
					return ERequest_NotReady;
			}
		}
	}
	for(int i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
	{
		if(m_ClosestPositionRequests[i].m_hHandle == hPath)
		{
			if(m_ClosestPositionRequests[i].m_bComplete)
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
					return ERequest_Ready;
			}
			else
			{
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
					return ERequest_NotReady;
			}
		}
	}
	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
	return ERequest_NotFound;
}




EPathServerErrorCode
CPathServer::LockPathResult(TPathHandle hPath, int * iNumPoints, Vector3 *& pPoints, TNavMeshWaypointFlag *& pWaypointFlags, TPathResultInfo * pPathInfo)
{
	// We really shouldn't be calling this function with a null handle
	if(hPath == PATH_HANDLE_NULL)
	{
		Assert(hPath != PATH_HANDLE_NULL);
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(int i=0; i<MAX_NUM_PATH_REQUESTS; i++)
	{
		if(m_PathRequests[i].m_hHandle == hPath)
		{
			if(m_PathRequests[i].m_bComplete)
			{
				*iNumPoints = m_PathRequests[i].m_iNumPoints;
				pPoints = &m_PathRequests[i].m_PathPoints[0];
				pWaypointFlags = &m_PathRequests[i].m_WaypointFlags[0];

				if(pPathInfo)
				{
					sysMemCpy(pPathInfo, &m_PathRequests[i].m_PathResultInfo, sizeof(TPathResultInfo));
				}

#if NAVMESH_OPTIMISATIONS_OFF && __ASSERT
				if(m_PathRequests[i].m_iNumPoints >= 2)
				{
					const Vector3 vDiff = m_PathRequests[i].m_PathPoints[m_PathRequests[i].m_iNumPoints-1] - m_PathRequests[i].m_PathPoints[m_PathRequests[i].m_iNumPoints-2];
					Assert(vDiff.Mag2() > SMALL_FLOAT);
				}
#endif


#if !__FINAL
#ifdef GTA_ENGINE
#if DEBUG_DRAW
				if(m_iVisualiseNavMeshes && m_PathRequests[i].m_iNumPoints)
				{
					for(s32 p=0; p<m_PathRequests[i].m_iNumPoints-1; p++)
					{
						grcDebugDraw::Line(
							m_PathRequests[i].m_PathPoints[p] + Vector3(0, 0, 0.5f),
							m_PathRequests[i].m_PathPoints[p+1] + Vector3(0, 0, 0.5f),
							Color32(255,0,0,255)	//0xff0000ff
						);
					}
				}
#endif // DEBUG_DRAW
#endif
				if(ms_bOutputDetailsOfPaths)
				{
#if __BANK
#if __DEV
					DebugOutPathRequestResult(&m_PathRequests[i]);
#endif // __DEV
#endif // BANK
				}
#endif
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return m_PathRequests[i].m_iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
	return PATH_ERROR_INVALID_HANDLE;
}


bool CPathServer::UnlockPathResultAndClear(TPathHandle hPath)
{
	// We really shouldn't be calling this function with a null handle
	if(hPath == PATH_HANDLE_NULL)
	{
		Assert(hPath != PATH_HANDLE_NULL);
		return false;
	}

	return CancelRequestInternal(hPath, false);
}



// Copies the data into the 'destGrid', and thereafter hPath is invalid.
EPathServerErrorCode CPathServer::GetGridResultAndClear(TPathHandle hPath, CWalkRndObjGrid & destGrid)
{
	// We really shouldn't be calling this function with a null handle
	if(hPath == PATH_HANDLE_NULL)
	{
		Assert(hPath != PATH_HANDLE_NULL);
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(int i=0; i<MAX_NUM_GRID_REQUESTS; i++)
	{
		if(m_GridRequests[i].m_hHandle == hPath)
		{
			Assert(m_GridRequests[i].m_iType == EGrid);

			if(m_GridRequests[i].m_bComplete)
			{
				sysMemCpy(&destGrid, &m_GridRequests[i].m_WalkRndObjGrid, sizeof(CWalkRndObjGrid));

				EPathServerErrorCode iCompletionCode = m_GridRequests[i].m_iCompletionCode;
				m_GridRequests[i].Reset();

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

				return iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
	return PATH_ERROR_INVALID_HANDLE;
}


// Copies the result into bLineOfSightIsClear, and clears the request
EPathServerErrorCode CPathServer::GetLineOfSightResult(TPathHandle handle, bool & bLineOfSightIsClear)
{
	// We really shouldn't be calling this function with a null handle
	if(handle == PATH_HANDLE_NULL)
	{
		Assert(handle != PATH_HANDLE_NULL);
		bLineOfSightIsClear = false;
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(u32 i=0; i<MAX_NUM_LOS_REQUESTS; i++)
	{
		if(m_LineOfSightRequests[i].m_hHandle == handle)
		{
			Assert(m_LineOfSightRequests[i].m_iType == ELineOfSight);

			if(m_LineOfSightRequests[i].m_bComplete)
			{
				bLineOfSightIsClear = m_LineOfSightRequests[i].m_bLineOfSightExists;
				EPathServerErrorCode iCompletionCode = m_LineOfSightRequests[i].m_iCompletionCode;

				m_LineOfSightRequests[i].Reset();

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

				return iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
			bLineOfSightIsClear = false;
			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
	bLineOfSightIsClear = false;
	return PATH_ERROR_INVALID_HANDLE;
}

// Copies the overall result into bLineOfSightIsClear.  Individual lineseg results are then copied into the array of
// bools pointed to by bLineOfSightResults.  This array MUST be enough to accomodate the results (it's recommended
// that this array be dimensioned to MAX_LINEOFSIGHT_POINTS.  The last parameter passed in should equal the number
// of points in the original LOS request - it is here just for error checking.

EPathServerErrorCode CPathServer::GetLineOfSightResult(TPathHandle handle, bool & bLineOfSightIsClear, bool * bLineOfSightResults, int ASSERT_ONLY(iNumPts))
{
	Assert(iNumPts > 0 && iNumPts < MAX_LINEOFSIGHT_POINTS);

	// We really shouldn't be calling this function with a null handle
	if(handle == PATH_HANDLE_NULL)
	{
		Assert(handle != PATH_HANDLE_NULL);
		bLineOfSightIsClear = false;
		bLineOfSightResults = NULL;
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(u32 i=0; i<MAX_NUM_LOS_REQUESTS; i++)
	{
		if(m_LineOfSightRequests[i].m_hHandle == handle)
		{
			Assert(m_LineOfSightRequests[i].m_iType == ELineOfSight);

			if(m_LineOfSightRequests[i].m_bComplete)
			{
				bLineOfSightIsClear = m_LineOfSightRequests[i].m_bLineOfSightExists;

				// Just a safety measure
				Assert(iNumPts == m_LineOfSightRequests[i].m_iNumPts);

				// Copy in the results for the individual line segments
				for(int p=0; p<m_LineOfSightRequests[i].m_iNumPts; p++)
				{
					bLineOfSightResults[p] = m_LineOfSightRequests[i].m_bLosResults[p];
				}

				EPathServerErrorCode iCompletionCode = m_LineOfSightRequests[i].m_iCompletionCode;

				m_LineOfSightRequests[i].Reset();

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

				return iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

			bLineOfSightIsClear = false;
			bLineOfSightResults = NULL;
			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	bLineOfSightIsClear = false;
	bLineOfSightResults = NULL;
	return PATH_ERROR_INVALID_HANDLE;
}


// Copies the result of the audio properties query into iAudioProperties, and clears the request
EPathServerErrorCode CPathServer::GetAudioPropertiesResult(TPathHandle handle, u32 & iAudioProperty)
{
	// We really shouldn't be calling this function with a null handle
	if(handle == PATH_HANDLE_NULL)
	{
		Assert(handle != PATH_HANDLE_NULL);
		iAudioProperty = 0;
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(u32 i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
	{
		if(m_AudioRequests[i].m_hHandle == handle)
		{
			// For now, to be on the safe side, if any audio requests are encountered with the
			// wrong m_iType, I'll cancel the request and return PATH_NOT_FOUND.  This is better
			// than returning potentially bad data to the audio system, and since this only seems
			// to happen extremely rarely it should be ok (until I track down the real cause!)
			if(m_AudioRequests[i].m_iType != EAudioProperties)
			{
				Assert(false);
				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

				CancelRequestInternal(handle, false);
				return PATH_NOT_FOUND;
			}

			if(m_AudioRequests[i].m_bComplete)
			{
				Assert(m_AudioRequests[i].m_iNumAudioProperties > 0);
				iAudioProperty = m_AudioRequests[i].m_iAudioProperties[0];

				EPathServerErrorCode iCompletionCode = m_AudioRequests[i].m_iCompletionCode;
				m_AudioRequests[i].Reset();

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

				return iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

			iAudioProperty = 0;
			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	iAudioProperty = 0;
	return PATH_ERROR_INVALID_HANDLE;
}

// Copies the result of the audio properties query into iAudioProperties, and clears the request
// NOTE : pAudioProperties & pPolyAreas *must* point to arrays of MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES in size.
EPathServerErrorCode CPathServer::GetAudioPropertiesResult(TPathHandle handle, u32 & iNumProperties, u32 * pAudioProperties, float * pPolyAreas, Vector3 * pPolyCentroids, u8 * pAdditionalFlags)
{
	Assert(handle != PATH_HANDLE_NULL);

	// We really shouldn't be calling this function with a null handle
	if(handle == PATH_HANDLE_NULL)
	{
		iNumProperties = 0;
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(u32 i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
	{
		if(m_AudioRequests[i].m_hHandle == handle)
		{
			Assertf(m_AudioRequests[i].m_iType == EAudioProperties, "Navmesh floodfill warning : was expecting EAudioProperties but found %i", m_AudioRequests[i].m_iType);

			if(m_AudioRequests[i].m_bComplete)
			{
				iNumProperties = m_AudioRequests[i].m_iNumAudioProperties;
				for(int p=0; p<m_AudioRequests[i].m_iNumAudioProperties; p++)
				{
					pAudioProperties[p] = m_AudioRequests[i].m_iAudioProperties[p];
					pPolyAreas[p] = m_AudioRequests[i].m_fPolyAreas[p];
					pPolyCentroids[p] = m_AudioRequests[i].m_vPolyCentroids[p];
					pAdditionalFlags[p] = m_AudioRequests[i].m_iAdditionalFlags[p];
				}

				EPathServerErrorCode iCompletionCode = m_AudioRequests[i].m_iCompletionCode;
				m_AudioRequests[i].Reset();

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

				return iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

			iNumProperties = 0;
			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	iNumProperties = 0;
	return PATH_ERROR_INVALID_HANDLE;
}


EPathServerErrorCode
CPathServer::GetClosestCarNodeSearchResultAndClear(const TPathHandle hPath, Vector3 & vClosestCarNode)
{
	float fTmp;
	return GetClosestFloodFillResultAndClear(hPath, vClosestCarNode, fTmp, CFloodFillRequest::EFindClosestCarNodeFloodFill);
}
EPathServerErrorCode
CPathServer::GetClosestShelteredPolySearchResultAndClear(const TPathHandle hPath, Vector3 & vClosestShelteredPoly)
{
	float fTmp;
	return GetClosestFloodFillResultAndClear(hPath, vClosestShelteredPoly, fTmp, CFloodFillRequest::EFindClosestShelteredPolyFloodFill);
}
EPathServerErrorCode
CPathServer::GetClosestUnshelteredPolySearchResultAndClear(const TPathHandle hPath, float & closestDistSquared)
{
	Vector3 vTmp;
	return GetClosestFloodFillResultAndClear(hPath, vTmp, closestDistSquared, CFloodFillRequest::EFindClosestUnshelteredPolyFloodFill);
}
EPathServerErrorCode
CPathServer::GetAreaUnderfootSearchResultAndClear(const TPathHandle hPath, float & fArea)
{
	Vector3 vTmp;
	return GetClosestFloodFillResultAndClear(hPath, vTmp, fArea, CFloodFillRequest::ECalcAreaUnderfoot);
}
EPathServerErrorCode
CPathServer::GetHasNearbyPavementSearchResultAndClear(const TPathHandle hPath)
{
	Vector3 vTmp;
	float fTmp;
	return GetClosestFloodFillResultAndClear(hPath, vTmp, fTmp, CFloodFillRequest::EFindNearbyPavementFloodFill);
}

EPathServerErrorCode CPathServer::GetClosestFloodFillResultAndClear(const TPathHandle hPath, Vector3 & vClosest, float & fValue, const CFloodFillRequest::EType ASSERT_ONLY(eType))
{
	// We really shouldn't be calling this function with a null handle
	if(hPath == PATH_HANDLE_NULL)
	{
		Assert(hPath != PATH_HANDLE_NULL);
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(int i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
	{
		if(m_FloodFillRequests[i].m_hHandle == hPath)
		{
			Assert(m_FloodFillRequests[i].m_iType == EFloodFill);
			Assert(m_FloodFillRequests[i].m_FloodFillType == eType);

			if(m_FloodFillRequests[i].m_bComplete)
			{
				bool bFoundPosition=false;

				switch(m_FloodFillRequests[i].m_FloodFillType)
				{
					case CFloodFillRequest::EAudioPropertiesFloodFill:
					{
						Assertf(0, "EAudioPropertiesFloodFill - not implemented yet!");
						break;
					}
					case CFloodFillRequest::EFindClosestCarNodeFloodFill:
					{
						vClosest = m_FloodFillRequests[i].GetFindClosestCarNodeData()->GetCarNodePos();
						bFoundPosition = m_FloodFillRequests[i].GetFindClosestCarNodeData()->m_bFoundCarNode;
						break;
					}
					case CFloodFillRequest::EFindClosestShelteredPolyFloodFill:
					{
						vClosest = m_FloodFillRequests[i].GetFindClosestShelteredPolyData()->GetPos();
						bFoundPosition = m_FloodFillRequests[i].GetFindClosestShelteredPolyData()->m_bFound;
						break;
					}
					case CFloodFillRequest::EFindClosestUnshelteredPolyFloodFill:
					{
						fValue = m_FloodFillRequests[i].GetFindClosestUnshelteredPolyData()->m_fClosestDistSqr;
						break;
					}
					case CFloodFillRequest::ECalcAreaUnderfoot:
					{
						fValue = m_FloodFillRequests[i].GetCalcAreaUnderfootData()->m_fArea;
						bFoundPosition = true;
						break;
					}
					case CFloodFillRequest::EFindClosestPositionOnLand:
					{
						vClosest = m_FloodFillRequests[i].GetFindClosestPositionOnLandData()->GetPos();
						bFoundPosition = m_FloodFillRequests[i].GetFindClosestPositionOnLandData()->m_bFound;
						break;
					}
					case CFloodFillRequest::EFindNearbyPavementFloodFill:
					{
						bFoundPosition = m_FloodFillRequests[i].GetFindNearbyPavementPolyData()->m_bFound;
						break;
					}
					default:
					{
						Assertf(0, "Unhandled floodfill type!");
						break;
					}
				}

				EPathServerErrorCode iCompletionCode = m_FloodFillRequests[i].m_iCompletionCode;

				if(iCompletionCode == PATH_NO_ERROR && !bFoundPosition)
				{
					iCompletionCode = PATH_NOT_FOUND;
				}
				m_FloodFillRequests[i].Reset();

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

				return iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return PATH_ERROR_INVALID_HANDLE;
}


// Copies the data into the 'destGrid', and thereafter hPath is invalid.
EPathServerErrorCode CPathServer::GetClearAreaResultAndClear(const TPathHandle hPath, Vector3 & vResultOrigin)
{
	// We really shouldn't be calling this function with a null handle
	if(hPath == PATH_HANDLE_NULL)
	{
		Assert(hPath != PATH_HANDLE_NULL);
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(int i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
	{
		if(m_ClearAreaRequests[i].m_hHandle == hPath)
		{
			Assert(m_ClearAreaRequests[i].m_iType == EClearArea);

			if(m_ClearAreaRequests[i].m_bComplete)
			{
				EPathServerErrorCode iCompletionCode = m_ClearAreaRequests[i].m_iCompletionCode;
				vResultOrigin = m_ClearAreaRequests[i].m_vResultOrigin;

				m_ClearAreaRequests[i].Reset();

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

				return iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return PATH_ERROR_INVALID_HANDLE;
}

// Copies the data into the 'destGrid', and thereafter hPath is invalid.
EPathServerErrorCode CPathServer::GetClosestPositionResultAndClear(const TPathHandle hPath, CClosestPositionRequest::TResults & results)
{
	// We really shouldn't be calling this function with a null handle
	if(hPath == PATH_HANDLE_NULL)
	{
		Assert(hPath != PATH_HANDLE_NULL);
		return PATH_ERROR_INVALID_HANDLE;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	for(int i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
	{
		if(m_ClosestPositionRequests[i].m_hHandle == hPath)
		{
			Assert(m_ClosestPositionRequests[i].m_iType == EClosestPosition);

			if(m_ClosestPositionRequests[i].m_bComplete)
			{
				EPathServerErrorCode iCompletionCode = m_ClosestPositionRequests[i].m_iCompletionCode;

				sysMemCpy(&results, &m_ClosestPositionRequests[i].m_Results, sizeof(CClosestPositionRequest::TResults));

				m_ClosestPositionRequests[i].Reset();

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return iCompletionCode;
			}

			GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
			return PATH_STILL_PENDING;
		}
	}

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
	return PATH_ERROR_INVALID_HANDLE;
}

void CPathServer::InitBeforeMapLoaded(unsigned UNUSED_PARAM(iInitMode))
{
	Reset(true);
}
void CPathServer::InitSession(unsigned UNUSED_PARAM(iInitMode))
{
	ms_bGameInSession = true;
	Reset();
}
void CPathServer::ShutdownSession(unsigned UNUSED_PARAM(iShutdownMode))
{
	ms_bGameInSession = false;
	Reset();
}

void CPathServer::Reset(const bool bLoadingNewMap)
{
#ifdef GTA_ENGINE

	bool bOldVal = CPathServer::ms_bGameInSession;
	CPathServer::ms_bGameInSession = false;

	ForceAbortCurrentPathRequest();

	while(fwPathServer::m_bPathServerThreadIsActive)
	{
		sysIpcYield(0);
	}

	m_AmbientPedGeneration.ClearAllPedGenBlockingAreas(false, false);
	m_AmbientPedGeneration.Reset();
#endif

	// Remove all requested regions
	RemoveAllNavMeshRegions();

	//LOCK_REQUESTS

	{
		int r;
		for(r=0; r<MAX_NUM_PATH_REQUESTS; r++)
		{
			m_PathRequests[r].Clear();
			m_PathRequests[r].m_bSlotEmpty = true;
		}
		for(r=0; r<MAX_NUM_GRID_REQUESTS; r++)
		{
			m_GridRequests[r].Clear();
			m_GridRequests[r].m_bSlotEmpty = true;
		}
		for(r=0; r<MAX_NUM_LOS_REQUESTS; r++)
		{
			m_LineOfSightRequests[r].Clear();
			m_LineOfSightRequests[r].m_bSlotEmpty = true;
		}
		for(r=0; r<MAX_NUM_AUDIO_REQUESTS; r++)
		{
			m_AudioRequests[r].Clear();
			m_AudioRequests[r].m_bSlotEmpty = true;
		}
		for(r=0; r<MAX_NUM_FLOODFILL_REQUESTS; r++)
		{
			m_FloodFillRequests[r].Clear();
			m_FloodFillRequests[r].m_bSlotEmpty = true;
		}
		for(r=0; r<MAX_NUM_CLEARAREA_REQUESTS; r++)
		{
			m_ClearAreaRequests[r].Clear();
			m_ClearAreaRequests[r].m_bSlotEmpty = true;
		}
		for(r=0; r<MAX_NUM_CLOSESTPOSITION_REQUESTS; r++)
		{
			m_ClosestPositionRequests[r].Clear();
			m_ClosestPositionRequests[r].m_bSlotEmpty = true;
		}
	}

#ifdef GTA_ENGINE

	LOCK_NAVMESH_DATA;
	LOCK_IMMEDIATE_DATA;
	LOCK_DYNAMIC_OBJECTS_DATA;

	if(bLoadingNewMap)
	{
		//---------------------------------
		// Unload all main-map navmeshes

		for(int domain = 0; domain < kNumNavDomains; domain++)
		{
			if(!fwPathServer::GetIsNavDomainEnabled((aiNavDomain)domain))
				continue;

			aiNavMeshStore* pStore = m_pNavMeshStores[domain];
			
			pStore->ResetBuildID();

			for(u32 s=0; s<(u32)pStore->GetSize(); s++)
			{
				const strLocalIndex iStreamingIndex = strLocalIndex(s);
				CNavMesh * pNavMesh = pStore->GetMeshByStreamingIndex(iStreamingIndex.Get());

				// If a mesh is not dynamic, then its a main-map navmesh
				if(pNavMesh && !pNavMesh->GetIsDynamic())
				{
					if(domain == kNavDomainRegular)
					{
						PrepareToUnloadNavMeshDataSetNormal(pNavMesh);
					}
					else
					{
						Assert(domain == kNavDomainHeightMeshes);
						PrepareToUnloadNavMeshDataSetHeightMesh(pNavMesh);
					}
					pStore->ClearRequiredFlag(iStreamingIndex.Get(), STRFLAG_DONTDELETE);
					pStore->StreamingRemove(iStreamingIndex);
				}
			}
		}

		//------------------------------
		// Unload all dynamic navmeshes

		for(int d=0; d<m_DynamicNavMeshStore.GetNum(); d++)
		{
			CModelInfoNavMeshRef & dynNavInf = m_DynamicNavMeshStore.Get(d);
			dynNavInf.m_iNumRefs = 0;

			PrepareToUnloadNavMeshDataSetNormal(dynNavInf.m_pBackUpNavmeshCopy);

			m_pNavMeshStores[kNavDomainRegular]->StreamingRemove(dynNavInf.m_iStreamingIndex);
		}
	}

#endif

	m_PathServerThread.Reset();
	ms_iBlockRequestsFlags = 0;

	m_vOrigin = Vector3(g_fVeryLargeValue, g_fVeryLargeValue, g_fVeryLargeValue);

	// Reset timers, etc
	m_iLastTimeThreadDidHousekeeping = 0;
	m_iTimeOfLastRequestAndEvictMeshes = 0;
	m_iLastTimeCheckedForStaleRequests = 0;
	m_iLastTimeProcessedPedGeneration = 0;

	m_iNextHandle = 1;
	m_iNextPathIndexToStartFrom = 0;
	m_iNextGridIndexToStartFrom = 0;
	m_iNextLosIndexToStartFrom = 0;
	m_iNextFloodFillIndexToStartFrom = 0;
	m_iNextClearAreaIndexToStartFrom = 0;
	m_iNextClosestPositionIndexToStartFrom = 0;

	m_iNumDynamicObjects = 0;
	m_iNumPathRegionSwitches = 0;
	ms_bCurrentlyRemovingAndAddingObjects = false;
	m_iNumDeferredAddDynamicObjects = 0;
	m_iNumScriptBlockingObjects = 0;

	m_iTimeOfLastCoverPointTimeslice_Millisecs = 0;

	for(s32 i=0; i<MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS; i++)
	{
		m_ScriptedDynamicObjects[i].m_iScriptObjectHandle = -1;
		m_ScriptedDynamicObjects[i].m_iObjectIndex = DYNAMIC_OBJECT_INDEX_NONE;
	}

#ifdef GTA_ENGINE
	m_bGameRunning = false;
	CPathServer::ms_bGameInSession = bOldVal;
#endif
}

bool CPathServer::CancelRequest(TPathHandle handle)
{
	return CancelRequestInternal(handle, true);
}

bool CPathServer::CancelRequestInternal(TPathHandle handle, bool bCancelledByClient)
{
	// We really shouldn't be calling this function with a null handle
	if(handle == PATH_HANDLE_NULL)
	{
		Assert(handle != PATH_HANDLE_NULL);
		return false;
	}

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	u32 i;
	for(i=0; i<MAX_NUM_PATH_REQUESTS; i++)
	{
		if(m_PathRequests[i].m_hHandle == handle)
		{
			sysCriticalSection critSec(m_PathRequests[i].m_CriticalSectionToken);

			if(m_PathRequests[i].m_bRequestPending || m_PathRequests[i].m_bRequestActive || m_PathRequests[i].m_bComplete)
			{
				m_PathRequests[i].Reset();
				if(bCancelledByClient)
					m_PathRequests[i].m_iCompletionCode = PATH_CANCELLED;
				m_PathRequests[i].m_iType = EPath;

				if(m_PathRequests[i].m_bRequestActive)
				{
					m_PathRequests[i].m_bWaitingToAbort = true;
				}

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return true;
			}
		}
	}
	for(i=0; i<MAX_NUM_GRID_REQUESTS; i++)
	{
		if(m_GridRequests[i].m_hHandle == handle)
		{
			sysCriticalSection critSec(m_GridRequests[i].m_CriticalSectionToken);

			if(m_GridRequests[i].m_bRequestPending || m_GridRequests[i].m_bRequestActive || m_GridRequests[i].m_bComplete)
			{
				m_GridRequests[i].Reset();
				if(bCancelledByClient)
					m_GridRequests[i].m_iCompletionCode = PATH_CANCELLED;
				m_GridRequests[i].m_iType = EGrid;

				if(m_GridRequests[i].m_bRequestActive)
				{
					m_GridRequests[i].m_bWaitingToAbort = true;
				}

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return true;
			}
		}
	}
	for(i=0; i<MAX_NUM_LOS_REQUESTS; i++)
	{
		if(m_LineOfSightRequests[i].m_hHandle == handle)
		{
			sysCriticalSection critSec(m_LineOfSightRequests[i].m_CriticalSectionToken);

			if(m_LineOfSightRequests[i].m_bRequestPending || m_LineOfSightRequests[i].m_bRequestActive || m_LineOfSightRequests[i].m_bComplete)
			{
				m_LineOfSightRequests[i].Reset();
				if(bCancelledByClient)
					m_LineOfSightRequests[i].m_iCompletionCode = PATH_CANCELLED;
				m_LineOfSightRequests[i].m_iType = ELineOfSight;

				if(m_LineOfSightRequests[i].m_bRequestActive)
				{
					m_LineOfSightRequests[i].m_bWaitingToAbort = true;
				}

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return true;
			}
		}
	}
	for(i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
	{
		if(m_AudioRequests[i].m_hHandle == handle)
		{
			sysCriticalSection critSec(m_AudioRequests[i].m_CriticalSectionToken);

			if(m_AudioRequests[i].m_bRequestPending || m_AudioRequests[i].m_bRequestActive || m_AudioRequests[i].m_bComplete)
			{
				m_AudioRequests[i].Reset();
				if(bCancelledByClient)
					m_AudioRequests[i].m_iCompletionCode = PATH_CANCELLED;
				m_AudioRequests[i].m_iType = EAudioProperties;

				if(m_AudioRequests[i].m_bRequestActive)
				{
					m_AudioRequests[i].m_bWaitingToAbort = true;
				}

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return true;
			}
		}
	}
	for(i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
	{
		if(m_FloodFillRequests[i].m_hHandle == handle)
		{
			sysCriticalSection critSec(m_FloodFillRequests[i].m_CriticalSectionToken);

			if(m_FloodFillRequests[i].m_bRequestPending || m_FloodFillRequests[i].m_bRequestActive || m_FloodFillRequests[i].m_bComplete)
			{
				m_FloodFillRequests[i].Reset();
				if(bCancelledByClient)
					m_FloodFillRequests[i].m_iCompletionCode = PATH_CANCELLED;
				m_FloodFillRequests[i].m_iType = EFloodFill;

				if(m_FloodFillRequests[i].m_bRequestActive)
				{
					m_FloodFillRequests[i].m_bWaitingToAbort = true;
				}

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return true;
			}
		}
	}
	for(i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
	{
		if(m_ClearAreaRequests[i].m_hHandle == handle)
		{
			sysCriticalSection critSec(m_ClearAreaRequests[i].m_CriticalSectionToken);

			if(m_ClearAreaRequests[i].m_bRequestPending || m_ClearAreaRequests[i].m_bRequestActive || m_ClearAreaRequests[i].m_bComplete)
			{
				m_ClearAreaRequests[i].Reset();
				if(bCancelledByClient)
					m_ClearAreaRequests[i].m_iCompletionCode = PATH_CANCELLED;
				m_ClearAreaRequests[i].m_iType = EClearArea;

				if(m_ClearAreaRequests[i].m_bRequestActive)
				{
					m_ClearAreaRequests[i].m_bWaitingToAbort = true;
				}

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return true;
			}
		}
	}
	for(i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
	{
		if(m_ClosestPositionRequests[i].m_hHandle == handle)
		{
			sysCriticalSection critSec(m_ClosestPositionRequests[i].m_CriticalSectionToken);

			if(m_ClosestPositionRequests[i].m_bRequestPending || m_ClosestPositionRequests[i].m_bRequestActive || m_ClosestPositionRequests[i].m_bComplete)
			{
				m_ClosestPositionRequests[i].Reset();
				if(bCancelledByClient)
					m_ClosestPositionRequests[i].m_iCompletionCode = PATH_CANCELLED;
				m_ClosestPositionRequests[i].m_iType = EClosestPosition;

				if(m_ClosestPositionRequests[i].m_bRequestActive)
				{
					m_ClosestPositionRequests[i].m_bWaitingToAbort = true;
				}

				GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
				return true;
			}
		}
	}
	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
	return false;
}





//***************************************************************************
//	PathServerThreadFunc
//	This is the entry-point for the CPathServerThread's worker thread.
//	The thread spins until a request is made, and then works to process it
//	and fill in the results.  This function calls members of the
//	CPathServerThread class which created it.
//***************************************************************************

#ifdef GTA_ENGINE
DECLARE_THREAD_FUNC(PathServerThreadFunc) {
#else
DWORD WINAPI PathServerThreadFunc(LPVOID ptr) {
#endif

	TPathServerThreadFuncParams * pParams = (TPathServerThreadFuncParams*) ptr;

	CPathServerThread * pPathServerThread = pParams->m_pPathServerThread;

#ifdef GTA_ENGINE
	RAGETRACE_INITTHREAD("PathServer", 64, 1);
	sysThreadType::AddCurrentThreadType(SYS_THREAD_PATHSERVER);
#else
	// because memory allocation on pathserver thread has stopped working in navmesh test app
	static char theAllocatorBuffer[sizeof(stockAllocator)];
	sysMemAllocator * theAllocator = (::new (theAllocatorBuffer) stockAllocator);
	sysMemAllocator::SetCurrent(*theAllocator);
	sysMemAllocator::SetMaster(*theAllocator);
	sysMemAllocator::SetContainer(*theAllocator);
#endif

	pPathServerThread->Run();

#ifndef GTA_ENGINE
	return 1;
#endif
}

#ifdef GTA_ENGINE
void
CPathServer::SuspendThread(void)
{
	Assert(m_eProcessingMode == EMultiThreaded);

	// Wait until all the critical sections are free - so the CPathServerThread is not stopped mid-request
	// Try to enter the critical section
	LOCK_NAVMESH_DATA;
	LOCK_IMMEDIATE_DATA;
	LOCK_DYNAMIC_OBJECTS_DATA;

	Assertf(false, "sysIpcSuspendThread not supported");

}

void CPathServer::ResumeThread()
{
	Assert(m_eProcessingMode == EMultiThreaded);

	// Wait until all the critical sections are free - so the CPathServerThread is not stopped mid-request
	// Try to enter the critical section
	LOCK_NAVMESH_DATA;
	LOCK_IMMEDIATE_DATA;
	LOCK_DYNAMIC_OBJECTS_DATA;

	Assertf(false, "sysIpcResumeThread not supported");

	dynamicObjectsCriticalSection.Exit();
	navMeshDataCriticalSection.Exit();
}
#endif





bool CPathServer::IsNavMeshLoadedAtPosition(const Vector3 & vPos, aiNavDomain domain)
{
	TNavMeshIndex iNavMeshIndex = GetNavMeshIndexFromPosition(vPos, domain);
	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);

	return (pNavMesh != NULL);
}

#ifdef GTA_ENGINE

void CPathServer::DeferredDisableClimbAtPosition(const Vector3 & vPos, const bool bNavmeshClimb, const bool bClimbableObject)
{
	m_PathServerThread.AddDeferredDisableClimbAtPosition(vPos, bNavmeshClimb, bClimbableObject);
}

bool CPathServer::DisableClimbAtPosition(const Vector3 & vPos, const bool bClimbableObject)
{
	LOCK_NAVMESH_DATA

	// This function always operates on the regular data set at this point.
	aiNavDomain domain = kNavDomainRegular;

	TNavMeshIndex iNavMesh = GetNavMeshIndexFromPosition(vPos, domain);
	if(iNavMesh==NAVMESH_NAVMESH_INDEX_NONE)
	{
		return false;
	}

	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMesh, domain);
	if(!pNavMesh)
	{
		return false;
	}

	bool bDisabled = false;
	if(bClimbableObject)
	{
		bDisabled = pNavMesh->DisableSpecialLinksSpanningPosition(vPos, 2.0f);
	}
	else
	{
		bDisabled = pNavMesh->DisableNonStandardAdjacancyAtPosition(vPos, 20.0f);
	}

	return bDisabled;
}

#endif

#ifdef GTA_ENGINE
static aiNavMeshStoreInterfaceGta s_NavMeshStoreInterface;
#endif

void CPathServer::RegisterStreamingModule(const char* pPathForDatFile)
{
	m_NavDomainsEnabled[kNavDomainHeightMeshes] = false;

	if(!pPathForDatFile)
	{
		pPathForDatFile = "common:/data/";
	}

	// Read the "nav.dat" file
	CNavDatInfo navMeshesInfo, navNodesInfo;
	ReadDatFile(pPathForDatFile, navMeshesInfo, navNodesInfo);

	if(navMeshesInfo.iMaxMeshIndex == 0)
	{
		Assertf(false, "PathServer::Init() - didn't initialise properly.");
		return;
	}

	for(int domain = 0; domain < kNumNavDomains; domain++)
	{
		m_pNavMeshStores[domain] = NULL;
	}


#ifdef GTA_ENGINE
#if __REMAP_NAVMESH_INDICES
	m_pNavMeshStores[kNavDomainRegular] = rage_new aiNavMeshStore(kNavDomainRegular, s_NavMeshStoreInterface, "NavMeshes", NAVMESH_FILE_ID, navMeshesInfo.iNumMeshesInAnyLevel+NAVMESH_INDEX_GAP_BETWEEN_STATIC_AND_DYNAMIC+m_iMaxDynamicNavmeshTypes, 2048, false, __DEFRAG_NAVMESHES);
#else
	m_pNavMeshStores[kNavDomainRegular] = rage_new aiNavMeshStore(kNavDomainRegular, s_NavMeshStoreInterface, "NavMeshes", NAVMESH_FILE_ID, NAVMESH_INDEX_FINAL_DYNAMIC+1, 2048, false, __DEFRAG_NAVMESHES);
#endif	// __REMAP_NAVMESH_INDICES
#else
	m_pNavMeshStores[kNavDomainRegular] = rage_new aiNavMeshStore("NavMeshes", NAVMESH_FILE_ID, navMeshesInfo.iNumMeshesInAnyLevel+NAVMESH_INDEX_GAP_BETWEEN_STATIC_AND_DYNAMIC+m_iMaxDynamicNavmeshTypes, false);
#endif

	// At this point, fwConfigManager should have been initialized already, so
	// we can just force the pools to be allocated right here, unlike what we do for
	// the statically constructed fwAssetStore objects (g_IplStore, etc). It needs
	// to be done before the aiNavMeshStore::Init() call.
#ifdef GTA_ENGINE
	m_pNavMeshStores[kNavDomainRegular]->FinalizeSize();
#endif

	m_pNavMeshStores[kNavDomainRegular]->SetMaxMeshIndex(navMeshesInfo.iMaxMeshIndex);
	m_pNavMeshStores[kNavDomainRegular]->SetMeshSize(navMeshesInfo.fMeshSize);
	m_pNavMeshStores[kNavDomainRegular]->SetNumMeshesInX(navMeshesInfo.iNumMeshesInX);
	m_pNavMeshStores[kNavDomainRegular]->SetNumMeshesInY(navMeshesInfo.iNumMeshesInY);
	m_pNavMeshStores[kNavDomainRegular]->SetNumMeshesInAnyLevel(navMeshesInfo.iNumMeshesInAnyLevel);
	m_pNavMeshStores[kNavDomainRegular]->SetNumSectorsPerMesh(navMeshesInfo.iSectorsPerMesh);
	m_pNavMeshStores[kNavDomainRegular]->Init();

	m_pNavMeshStores[kNavDomainRegular]->CreateNameHashTable();

	// To conserve memory, we don't reserve space for a bunch of heightmeshes here. It should be possible
	// to use the pool name "HeightMeshes" to specify the size of the aiNavMeshStore from the configuration file.
	if(fwPathServer::GetIsNavDomainEnabled(kNavDomainHeightMeshes))
	{
		const int kDefaultMaxHeightMeshes = 1;
#ifdef GTA_ENGINE
		m_pNavMeshStores[kNavDomainHeightMeshes] = rage_new aiNavMeshStore(kNavDomainHeightMeshes, s_NavMeshStoreInterface, "HeightMeshes", HEIGHTMESH_FILE_ID, kDefaultMaxHeightMeshes, 2048, false, __DEFRAG_NAVMESHES);
		m_pNavMeshStores[kNavDomainHeightMeshes]->FinalizeSize();
#else
		m_pNavMeshStores[kNavDomainHeightMeshes] = rage_new aiNavMeshStore("HeightMeshes", HEIGHTMESH_FILE_ID, kDefaultMaxHeightMeshes, false);
#endif
		m_pNavMeshStores[kNavDomainHeightMeshes]->SetMaxMeshIndex(navMeshesInfo.iMaxMeshIndex);
		m_pNavMeshStores[kNavDomainHeightMeshes]->SetMeshSize(navMeshesInfo.fMeshSize);
		m_pNavMeshStores[kNavDomainHeightMeshes]->SetNumMeshesInX(navMeshesInfo.iNumMeshesInX);
		m_pNavMeshStores[kNavDomainHeightMeshes]->SetNumMeshesInY(navMeshesInfo.iNumMeshesInY);
		m_pNavMeshStores[kNavDomainHeightMeshes]->SetNumMeshesInAnyLevel(navMeshesInfo.iNumMeshesInAnyLevel);
		m_pNavMeshStores[kNavDomainHeightMeshes]->SetNumSectorsPerMesh(navMeshesInfo.iSectorsPerMesh);
		m_pNavMeshStores[kNavDomainHeightMeshes]->Init();

		m_pNavMeshStores[kNavDomainHeightMeshes]->CreateNameHashTable();
	}

#if __HIERARCHICAL_NODES_ENABLED
	m_pNavNodesStore = rage_new aiNavNodesStore(s_NavMeshStoreInterface, "NavNodes", NAVNODES_FILE_ID, navNodesInfo.iNumMeshesInAnyLevel, false);

#ifdef GTA_ENGINE
	m_pNavNodesStore->FinalizeSize();
#endif

	m_pNavNodesStore->SetMaxMeshIndex(navNodesInfo.iMaxMeshIndex);
	m_pNavNodesStore->SetMeshSize(navNodesInfo.fMeshSize);
	m_pNavNodesStore->SetNumMeshesInX(navNodesInfo.iNumMeshesInX);
	m_pNavNodesStore->SetNumMeshesInY(navNodesInfo.iNumMeshesInY);
	m_pNavNodesStore->SetNumMeshesInAnyLevel(navNodesInfo.iNumMeshesInAnyLevel);
	m_pNavNodesStore->SetNumSectorsPerMesh(navNodesInfo.iSectorsPerMesh);
	m_pNavNodesStore->Init();
#endif

#ifdef GTA_ENGINE
#if __BANK
	if(PARAM_nonav.Get())
		ms_bStreamingDisabled = true;
#endif
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision())
		ms_bStreamingDisabled = true;
#endif

	if(!ms_bStreamingDisabled)
	{
		for(int domain = 0; domain < kNumNavDomains; domain++)
		{
			if(!fwPathServer::GetIsNavDomainEnabled((aiNavDomain)domain))
				continue;

			if(m_pNavMeshStores[domain])
			{
				strStreamingEngine::GetInfo().GetModuleMgr().AddModule(m_pNavMeshStores[domain]);
#if USE_PAGED_POOLS_FOR_STREAMING
				m_pNavMeshStores[domain]->AllocateSlot(NAVMESH_INDEX_FINAL_DYNAMIC);
#endif // USE_PAGED_POOLS_FOR_STREAMING
			}
		}

#if __HIERARCHICAL_NODES_ENABLED
		strStreamingEngine::GetInfo().GetModuleMgr().AddModule(m_pNavNodesStore);
#endif
	}



#endif	// GTA_ENGINE
}


#if !__FINAL && __BANK

void DbgWriteFile(fiStream * pStream, const char * pTxt, ...)
{
	char tmp[256];
	va_list argptr;
	va_start(argptr, pTxt);
	vformatf(tmp, 256, pTxt, argptr);
	pStream->Write(tmp, istrlen(tmp));
}

bool
CPathServer::OutputPathRequestProblem(u32 iPathRequestIndex, const char * pFilename)
{
	CPathRequest * pRequest = &m_PathRequests[iPathRequestIndex];
	return OutputPathRequestProblem(pRequest, pFilename);
}

bool
CPathServer::OutputPathRequestProblem(CPathRequest * pRequest, const char * pFilename)
{
	// Wait until dynamic objects are available
#ifdef GTA_ENGINE
	sysCriticalSection dynamicObjectsCriticalSection(m_DynamicObjectsCriticalSectionToken);
#else
	EnterCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
#endif

	char filename[512] = { 0 };

	if(!pFilename)
	{
		bool bOk = BANKMGR.OpenFile(filename, 512, "*.prob", true, "PathFinding Problem Files");
		if(!bOk)
		{
			#ifndef GTA_ENGINE
			LeaveCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
			#endif
			return false;
		}
	}
	else
	{
		strcpy(filename, pFilename);
	}

	fiStream * pStream = fiStream::Create(filename);

	if(!pStream)
	{
		Printf("Couldn't open file \"%s\" to write.\n", filename);

		#ifndef GTA_ENGINE
		LeaveCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
		#endif
		return false;
	}

	int o;

	//******************************************************************************
	//	Dump out info about the path
	//******************************************************************************

	s32 iWander = (s32)pRequest->m_bWander;
	s32 iFlee = (s32)pRequest->m_bFleeTarget;

	int iStartNavX = CPathServerExtents::GetWorldToSectorX(pRequest->m_vUnadjustedStartPoint.x);
	int iStartNavY = CPathServerExtents::GetWorldToSectorY(pRequest->m_vUnadjustedStartPoint.y);
	int iEndNavX = CPathServerExtents::GetWorldToSectorX(pRequest->m_vUnadjustedEndPoint.x);
	int iEndNavY = CPathServerExtents::GetWorldToSectorY(pRequest->m_vUnadjustedEndPoint.y);

	static int iFileVersion = 2;
	DbgWriteFile(pStream, "PATHFINDING PROBLEM FILE VERSION %i\n\n\n", iFileVersion);

	DbgWriteFile(pStream, "PATH DETAILS:\n\n");
	DbgWriteFile(pStream, "*STARTPOS %.3f %.3f %.3f\n", pRequest->m_vUnadjustedStartPoint.x, pRequest->m_vUnadjustedStartPoint.y, pRequest->m_vUnadjustedStartPoint.z);
	DbgWriteFile(pStream, "START NAVMESH:[%i,%i]\n", iStartNavX, iStartNavY);
	DbgWriteFile(pStream, "*ENDPOS %.3f %.3f %.3f\n", pRequest->m_vUnadjustedEndPoint.x, pRequest->m_vUnadjustedEndPoint.y, pRequest->m_vUnadjustedEndPoint.z);
	if(!iWander && !iFlee)
		DbgWriteFile(pStream, "END NAVMESH:[%i,%i]\n", iEndNavX, iEndNavY);
	DbgWriteFile(pStream, "*REFVEC %.3f %.3f %.3f\n", pRequest->m_vReferenceVector.x, pRequest->m_vReferenceVector.y, pRequest->m_vReferenceVector.z);
	DbgWriteFile(pStream, "*REFDIST %.3f\n", pRequest->m_fReferenceDistance);
	DbgWriteFile(pStream, "*COMPLETION_RADIUS %.3f\n", pRequest->m_fPathSearchCompletionRadius);



	DbgWriteFile(pStream, "*m_bPreferPavements = %s\n", pRequest->m_bPreferPavements ? "true" : "false");
	DbgWriteFile(pStream, "*m_bNeverLeavePavements = %s\n", pRequest->m_bNeverLeavePavements ? "true" : "false");
	DbgWriteFile(pStream, "*m_bNeverClimbOverStuff = %s\n", pRequest->m_bNeverClimbOverStuff ? "true" : "false");
	DbgWriteFile(pStream, "*m_bNeverDropFromHeight = %s\n", pRequest->m_bNeverDropFromHeight ? "true" : "false");
	DbgWriteFile(pStream, "*m_bMayUseFatalDrops = %s\n", pRequest->m_bMayUseFatalDrops ? "true" : "false");
	DbgWriteFile(pStream, "*m_bNeverUseLadders = %s\n", pRequest->m_bNeverUseLadders ? "true" : "false");
	DbgWriteFile(pStream, "*m_bAllowFirstPointToBeInsideDynamicObject = %s\n", pRequest->m_bAllowFirstPointToBeInsideDynamicObject ? "true" : "false");
	DbgWriteFile(pStream, "*m_bFleeTarget = %s\n", pRequest->m_bFleeTarget ? "true" : "false");
	DbgWriteFile(pStream, "*m_bWander = %s\n", pRequest->m_bWander ? "true" : "false");
	DbgWriteFile(pStream, "*m_bNeverEnterWater = %s\n", pRequest->m_bNeverEnterWater ? "true" : "false");
	DbgWriteFile(pStream, "*m_bClimbObjects = %s\n", pRequest->m_bClimbObjects ? "true" : "false");
	DbgWriteFile(pStream, "*m_bPushObjects = %s\n", pRequest->m_bPushObjects ? "true" : "false");
	DbgWriteFile(pStream, "*m_bDontAvoidDynamicObjects = %s\n", pRequest->m_bDontAvoidDynamicObjects ? "true" : "false");
	DbgWriteFile(pStream, "*m_bEndPointWasAdjusted = %s\n", pRequest->m_bEndPointWasAdjusted ? "true" : "false");
	DbgWriteFile(pStream, "*m_bProblemPathStartsAndEndsOnSamePoly = %s\n", pRequest->m_bProblemPathStartsAndEndsOnSamePoly ? "true" : "false");
	DbgWriteFile(pStream, "*m_bPreferDownHill = %s\n", pRequest->m_bPreferDownHill ? "true" : "false");
	DbgWriteFile(pStream, "*m_bGoAsFarAsPossibleIfNavMeshNotLoaded = %s\n", pRequest->m_bGoAsFarAsPossibleIfNavMeshNotLoaded ? "true" : "false");
	DbgWriteFile(pStream, "*m_bSmoothSharpCorners = %s\n", pRequest->m_bSmoothSharpCorners ? "true" : "false");
	DbgWriteFile(pStream, "*m_bDoPostProcessToPreserveSlopeInfo = %s\n", pRequest->m_bDoPostProcessToPreserveSlopeInfo ? "true" : "false");
	DbgWriteFile(pStream, "*m_bScriptedRoute = %s\n", pRequest->m_bScriptedRoute ? "true" : "false");
	DbgWriteFile(pStream, "*m_bReduceObjectBoundingBoxes = %s\n", pRequest->m_bReduceObjectBoundingBoxes ? "true" : "false");
	DbgWriteFile(pStream, "*m_bUseLargerSearchExtents = %s\n", pRequest->m_bUseLargerSearchExtents ? "true" : "false");
	DbgWriteFile(pStream, "*m_bDontLimitSearchExtents = %s\n", pRequest->m_bDontLimitSearchExtents ? "true" : "false");
	DbgWriteFile(pStream, "*m_bDynamicNavMeshRoute = %s\n", pRequest->m_bDynamicNavMeshRoute ? "true" : "false");
	DbgWriteFile(pStream, "*m_bIfStartNotOnPavementAllowDropsAndClimbs = %s\n", pRequest->m_bIfStartNotOnPavementAllowDropsAndClimbs ? "true" : "false");
	DbgWriteFile(pStream, "*m_bNeverStartInWater = %s\n", pRequest->m_bNeverStartInWater ? "true" : "false");
	DbgWriteFile(pStream, "*m_bIgnoreNonSignificantObjects = %s\n", pRequest->m_bIgnoreNonSignificantObjects ? "true" : "false");
	DbgWriteFile(pStream, "*m_bIgnoreTypeVehicles = %s\n", pRequest->m_bIgnoreTypeVehicles ? "true" : "false");
	DbgWriteFile(pStream, "*m_bIgnoreTypeObjects = %s\n", pRequest->m_bIgnoreTypeObjects ? "true" : "false");
	DbgWriteFile(pStream, "*m_bFleeNeverEndInWater = %s\n", pRequest->m_bFleeNeverEndInWater ? "true" : "false");
	DbgWriteFile(pStream, "*m_bConsiderFreeSpaceAroundPoly = %s\n", pRequest->m_bConsiderFreeSpaceAroundPoly ? "true" : "false");
	DbgWriteFile(pStream, "*m_bUseMaxSlopeNavigable = %s\n", pRequest->m_bUseMaxSlopeNavigable ? "true" : "false");
	DbgWriteFile(pStream, "*m_bRandomisePoints = %s\n", pRequest->m_bRandomisePoints ? "true" : "false");
	DbgWriteFile(pStream, "*m_bUseDirectionalCover = %s\n", pRequest->m_bUseDirectionalCover ? "true" : "false");
	DbgWriteFile(pStream, "*m_bFavourEnclosedSpaces = %s\n", pRequest->m_bFavourEnclosedSpaces ? "true" : "false");
	DbgWriteFile(pStream, "*m_bUseVariableEntityRadius = %s\n", pRequest->m_bUseVariableEntityRadius ? "true" : "false");
	DbgWriteFile(pStream, "*m_bAvoidPotentialExplosions = %s\n", pRequest->m_bAvoidPotentialExplosions ? "true" : "false");
	DbgWriteFile(pStream, "*m_bAvoidTearGas = %s\n", pRequest->m_bAvoidTearGas ? "true" : "false");
	DbgWriteFile(pStream, "*m_bAllowToNavigateUpSteepPolygons = %s\n", pRequest->m_bAllowToNavigateUpSteepPolygons ? "true" : "false");
	DbgWriteFile(pStream, "*m_bAllowToPushVehicleDoorsClosed = %s\n", pRequest->m_bAllowToPushVehicleDoorsClosed ? "true" : "false");
	DbgWriteFile(pStream, "*m_bMissionPed = %s\n", pRequest->m_bMissionPed ? "true" : "false");
	DbgWriteFile(pStream, "*m_bEnsureLosBeforeEnding = %s\n", pRequest->m_bEnsureLosBeforeEnding ? "true" : "false");
	DbgWriteFile(pStream, "*m_bExpandStartEndPolyTessellationRadius = %s\n", pRequest->m_bExpandStartEndPolyTessellationRadius ? "true" : "false");
	DbgWriteFile(pStream, "*m_bPullFromEdgeExtra = %s\n", pRequest->m_bPullFromEdgeExtra ? "true" : "false");
	DbgWriteFile(pStream, "*m_bSofterFleeHeuristics = %s\n", pRequest->m_bSofterFleeHeuristics? "true" : "false");
	DbgWriteFile(pStream, "*m_bAvoidTrainTracks = %s\n", pRequest->m_bAvoidTrainTracks? "true" : "false");

	// Write movement costs
	DbgWriteFile(pStream, "*m_fClimbHighPenalty = %.3f\n", pRequest->m_MovementCosts.m_fClimbHighPenalty);
	DbgWriteFile(pStream, "*m_fClimbLowPenalty = %.3f\n", pRequest->m_MovementCosts.m_fClimbLowPenalty);
	DbgWriteFile(pStream, "*m_fDropDownPenalty = %.3f\n", pRequest->m_MovementCosts.m_fDropDownPenalty);
	DbgWriteFile(pStream, "*m_fDropDownPenaltyPerMetre = %.3f\n", pRequest->m_MovementCosts.m_fDropDownPenaltyPerMetre);
	DbgWriteFile(pStream, "*m_fClimbLadderPenalty = %.3f\n", pRequest->m_MovementCosts.m_fClimbLadderPenalty);
	DbgWriteFile(pStream, "*m_fClimbLadderPenaltyPerMetre = %.3f\n", pRequest->m_MovementCosts.m_fClimbLadderPenaltyPerMetre);
	DbgWriteFile(pStream, "*m_fEnterWaterPenalty = %.3f\n", pRequest->m_MovementCosts.m_fEnterWaterPenalty);
	DbgWriteFile(pStream, "*m_fLeaveWaterPenalty = %.3f\n", pRequest->m_MovementCosts.m_fLeaveWaterPenalty);
	DbgWriteFile(pStream, "*m_fBeInWaterPenalty = %.3f\n", pRequest->m_MovementCosts.m_fBeInWaterPenalty);
	DbgWriteFile(pStream, "*m_fClimbObjectPenaltyPerMetre = %.3f\n", pRequest->m_MovementCosts.m_fClimbObjectPenaltyPerMetre);
	DbgWriteFile(pStream, "*m_fWanderPenaltyForZeroPedDensity = %.3f\n", pRequest->m_MovementCosts.m_fWanderPenaltyForZeroPedDensity);
	DbgWriteFile(pStream, "*m_fMoveOntoSteepSurfacePenalty = %.3f\n", pRequest->m_MovementCosts.m_fMoveOntoSteepSurfacePenalty);
	DbgWriteFile(pStream, "*m_fPenaltyForNoDirectionalCover = %.3f\n", pRequest->m_MovementCosts.m_fPenaltyForNoDirectionalCover);
	DbgWriteFile(pStream, "*m_fPenaltyForFavourCoverPerUnsetBit = %.3f\n", pRequest->m_MovementCosts.m_fPenaltyForFavourCoverPerUnsetBit);
	DbgWriteFile(pStream, "*m_iPenaltyForMovingToLowerPedDensity = %i\n", pRequest->m_MovementCosts.m_iPenaltyForMovingToLowerPedDensity);
	DbgWriteFile(pStream, "*m_iPenaltyPerPolyFreeSpaceDifference = %i\n", pRequest->m_MovementCosts.m_iPenaltyPerPolyFreeSpaceDifference);
	DbgWriteFile(pStream, "*m_fAvoidTrainTracksPenalty = %.3f\n", pRequest->m_MovementCosts.m_fAvoidTrainTracksPenalty);


	DbgWriteFile(pStream, "*RANDSEED %i\n", pRequest->m_iPedRandomSeed);
	DbgWriteFile(pStream, "*ENTITY_RADIUS %.3f\n", pRequest->m_fEntityRadius);

	DbgWriteFile(pStream, "*NUM_INCLUDE %i\n", pRequest->m_iNumIncludeObjects);
	for(o=0; o<pRequest->m_iNumIncludeObjects; o++)
		DbgWriteFile(pStream, "*OBJ %i\n", pRequest->m_IncludeObjects[o]);
	DbgWriteFile(pStream, "*NUM_EXCLUDE %i\n", pRequest->m_iNumExcludeObjects);
	for(o=0; o<pRequest->m_iNumExcludeObjects; o++)
		DbgWriteFile(pStream, "*OBJ %i\n", pRequest->m_ExcludeObjects[o]);

	DbgWriteFile(pStream, "*NUM_INFLUENCE_SPHERES %i\n", pRequest->m_iNumInfluenceSpheres);
	for(o=0; o<pRequest->m_iNumInfluenceSpheres; o++)
	{
		DbgWriteFile(pStream, "*SPHERE %.3f %.3f %.3f %.3f %.3f %.3f\n",
			pRequest->m_InfluenceSpheres[o].GetOrigin().x, pRequest->m_InfluenceSpheres[o].GetOrigin().y, pRequest->m_InfluenceSpheres[o].GetOrigin().z,
			pRequest->m_InfluenceSpheres[o].GetRadius(), pRequest->m_InfluenceSpheres[o].GetInnerWeighting(), pRequest->m_InfluenceSpheres[o].GetOuterWeighting() );
	}

	DbgWriteFile(pStream, "\n\n\n");
	DbgWriteFile(pStream, "DYNAMIC OBJECTS:\n\n");

	//******************************************************************************
	//	Dump out the details of all the dynamic objects
	//******************************************************************************

	TDynamicObject * pObj = m_PathServerThread.m_pFirstDynamicObject;
	int iNumObjs = 0;
	while(pObj)
	{
		iNumObjs++;
		pObj = pObj->m_pNext;
	}

	DbgWriteFile(pStream, "*NUMOBJECTS %i\n\n", iNumObjs);

	pObj = m_PathServerThread.m_pFirstDynamicObject;

	while(pObj)
	{
		DbgWriteFile(pStream, "*OBJECTPOS %f %f %f\n", pObj->m_Bounds.GetOrigin().x, pObj->m_Bounds.GetOrigin().y, pObj->m_Bounds.GetOrigin().z);
		DbgWriteFile(pStream, "*CORNERS %f %f %f   %f %f %f   %f %f %f   %f %f %f\n",
			pObj->m_vVertices[0].x, pObj->m_vVertices[0].y, pObj->m_Bounds.GetOrigin().z,
			pObj->m_vVertices[1].x, pObj->m_vVertices[1].y, pObj->m_Bounds.GetOrigin().z,
			pObj->m_vVertices[2].x, pObj->m_vVertices[2].y, pObj->m_Bounds.GetOrigin().z,
			pObj->m_vVertices[3].x, pObj->m_vVertices[3].y, pObj->m_Bounds.GetOrigin().z
		);
		DbgWriteFile(pStream, "*EDGEPLANES %f %f %f %f   %f %f %f %f   %f %f %f %f   %f %f %f %f   %f %f %f %f   %f %f %f %f\n",
			pObj->m_Bounds.m_vEdgePlaneNormals[0].x, pObj->m_Bounds.m_vEdgePlaneNormals[0].y, pObj->m_Bounds.m_vEdgePlaneNormals[0].z, pObj->m_Bounds.m_vEdgePlaneNormals[0].w,
			pObj->m_Bounds.m_vEdgePlaneNormals[1].x, pObj->m_Bounds.m_vEdgePlaneNormals[1].y, pObj->m_Bounds.m_vEdgePlaneNormals[1].z, pObj->m_Bounds.m_vEdgePlaneNormals[1].w,
			pObj->m_Bounds.m_vEdgePlaneNormals[2].x, pObj->m_Bounds.m_vEdgePlaneNormals[2].y, pObj->m_Bounds.m_vEdgePlaneNormals[2].z, pObj->m_Bounds.m_vEdgePlaneNormals[2].w,
			pObj->m_Bounds.m_vEdgePlaneNormals[3].x, pObj->m_Bounds.m_vEdgePlaneNormals[3].y, pObj->m_Bounds.m_vEdgePlaneNormals[3].z, pObj->m_Bounds.m_vEdgePlaneNormals[3].w,
			pObj->m_Bounds.m_vEdgePlaneNormals[4].x, pObj->m_Bounds.m_vEdgePlaneNormals[4].y, pObj->m_Bounds.m_vEdgePlaneNormals[4].z, pObj->m_Bounds.m_vEdgePlaneNormals[4].w,
			pObj->m_Bounds.m_vEdgePlaneNormals[5].x, pObj->m_Bounds.m_vEdgePlaneNormals[5].y, pObj->m_Bounds.m_vEdgePlaneNormals[5].z, pObj->m_Bounds.m_vEdgePlaneNormals[5].w
		);

		Vector3 vSegmentPlanes[4];
		pObj->m_Bounds.CalculateSegmentPlanes(vSegmentPlanes, pObj->m_vVertices);

		DbgWriteFile(pStream, "*SEGPLANES %f %f %f   %f %f %f   %f %f %f   %f %f %f\n",
			vSegmentPlanes[0].x, vSegmentPlanes[0].y, vSegmentPlanes[0].z,
			vSegmentPlanes[1].x, vSegmentPlanes[1].y, vSegmentPlanes[1].z,
			vSegmentPlanes[2].x, vSegmentPlanes[2].y, vSegmentPlanes[2].z,
			vSegmentPlanes[3].x, vSegmentPlanes[3].y, vSegmentPlanes[3].z
		);
		DbgWriteFile(pStream, "*TOPZ %f\n", pObj->m_Bounds.GetTopZ());
		DbgWriteFile(pStream, "*BOTTOMZ %f\n", pObj->m_Bounds.GetBottomZ());
		//DbgWriteFile(pStream, "*BOUNDINGRADIUS %f\n", pObj->m_Bounds.m_fBoundingRadius);
		DbgWriteFile(pStream, "*MINMAX %i %i %i %i %i %i \n", pObj->m_MinMax.m_iMinX, pObj->m_MinMax.m_iMinY, pObj->m_MinMax.m_iMinZ, pObj->m_MinMax.m_iMaxX, pObj->m_MinMax.m_iMaxY, pObj->m_MinMax.m_iMaxZ);
		DbgWriteFile(pStream, "*OPENABLE %i\n", pObj->m_bIsOpenable);
		DbgWriteFile(pStream, "*CLIMBABLE %i\n", pObj->m_bIsClimbable);
		DbgWriteFile(pStream, "*PUSHABLE %i\n", pObj->m_bIsPushable);

		DbgWriteFile(pStream, "\n");

#if __ASSERT
		// Error check the bounds of this entity
		static const float fPlaneEps = 0.05f;
		for(int pl=0; pl<4; pl++)
		{
			Vector3 vCorner(pObj->m_vVertices[pl].x, pObj->m_vVertices[pl].y, pObj->m_Bounds.GetOrigin().z);
			float fPlaneDist = pObj->m_Bounds.m_vEdgePlaneNormals[pl].Dot3(vCorner) + pObj->m_Bounds.m_vEdgePlaneNormals[pl].w;
			Assert(Abs(fPlaneDist) < fPlaneEps);
		}
#endif

		pObj = pObj->m_pNext;
	}

	DbgWriteFile(pStream, "\n\n\n\n");

	pStream->Close();


#ifndef GTA_ENGINE
	LeaveCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
#endif

	return true;
}

#ifndef GTA_ENGINE

bool CPathServer::LoadPathRequestProblem()
{
	char filename[512] = { 0 };

	bool bOk = BANKMGR.OpenFile(filename, 512, "*.prob", false, "PathFinding Problem Files");
	if(!bOk)
	{
		return false;
	}

	return LoadPathRequestProblem(filename);
}

bool ScanTF(char *& pBuffer, const char * pToken)
{
	char tokenBuffer[256];
	sprintf(tokenBuffer, "%s = %%s", pToken);

	char tmp[256];

	while(*pBuffer != '*') pBuffer++;
	int n = sscanf(pBuffer, tokenBuffer, tmp);
	pBuffer++;
	if(n != 1)
	{
		Assert(false);
		return false;
	}
	return (stricmp(tmp, "true")==0) ? true : false;
}

bool CPathServer::LoadPathRequestProblem(char * filename)
{
	// Wait until dynamic objects are available
	EnterCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);

	FILE * pFile = fopen(filename, "rt");
	if(!pFile)
	{
		LeaveCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
		return false;
	}

	fseek(pFile, 0, SEEK_END);
	int iSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	char * buffer = rage_new char[iSize];
	int iNumBytes = fread(buffer, 1, iSize, pFile);
	buffer[iNumBytes] = 0;
	fclose(pFile);

	char * ptr = buffer;

	int o;
	u32 iRandSeed = 0;
	//float fCompletionRadius = 0.0f;
	int iNumObjs = 0;

	// Just choose slot 0 for now..
	CPathRequest * pRequest = &m_PathRequests[0];

	// Skip to path details section
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "STARTPOS %f %f %f", &pRequest->m_vUnadjustedStartPoint.x, &pRequest->m_vUnadjustedStartPoint.y, &pRequest->m_vUnadjustedStartPoint.z);

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "ENDPOS %f %f %f", &pRequest->m_vUnadjustedEndPoint.x, &pRequest->m_vUnadjustedEndPoint.y, &pRequest->m_vUnadjustedEndPoint.z);

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "REFVEC %f %f %f", &pRequest->m_vReferenceVector.x, &pRequest->m_vReferenceVector.y, &pRequest->m_vReferenceVector.z);

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "REFDIST %f", &pRequest->m_fReferenceDistance);
	pRequest->m_fInitialReferenceDistance = pRequest->m_fReferenceDistance;

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "COMPLETION_RADIUS %f", &pRequest->m_fPathSearchCompletionRadius);


	


	pRequest->m_bPreferPavements = ScanTF(ptr, "*m_bPreferPavements");
	pRequest->m_bNeverLeavePavements = ScanTF(ptr, "*m_bNeverLeavePavements");
	pRequest->m_bNeverClimbOverStuff = ScanTF(ptr, "*m_bNeverClimbOverStuff");
	pRequest->m_bNeverDropFromHeight = ScanTF(ptr, "*m_bNeverDropFromHeight");
	pRequest->m_bMayUseFatalDrops = ScanTF(ptr, "*m_bMayUseFatalDrops");
	pRequest->m_bNeverUseLadders = ScanTF(ptr, "*m_bNeverUseLadders");
	pRequest->m_bAllowFirstPointToBeInsideDynamicObject = ScanTF(ptr, "*m_bAllowFirstPointToBeInsideDynamicObject");
	pRequest->m_bFleeTarget = ScanTF(ptr, "*m_bFleeTarget");
	pRequest->m_bWander = ScanTF(ptr, "*m_bWander");
	pRequest->m_bNeverEnterWater = ScanTF(ptr, "*m_bNeverEnterWater");
	pRequest->m_bClimbObjects = ScanTF(ptr, "*m_bClimbObjects");
	pRequest->m_bPushObjects = ScanTF(ptr, "*m_bPushObjects");
	pRequest->m_bDontAvoidDynamicObjects = ScanTF(ptr, "*m_bDontAvoidDynamicObjects");
	pRequest->m_bEndPointWasAdjusted = ScanTF(ptr, "*m_bEndPointWasAdjusted");
	pRequest->m_bProblemPathStartsAndEndsOnSamePoly = ScanTF(ptr, "*m_bProblemPathStartsAndEndsOnSamePoly");
	pRequest->m_bPreferDownHill = ScanTF(ptr, "*m_bPreferDownHill");
	pRequest->m_bGoAsFarAsPossibleIfNavMeshNotLoaded = ScanTF(ptr, "*m_bGoAsFarAsPossibleIfNavMeshNotLoaded");
	pRequest->m_bSmoothSharpCorners = ScanTF(ptr, "*m_bSmoothSharpCorners");
	pRequest->m_bDoPostProcessToPreserveSlopeInfo = ScanTF(ptr, "*m_bDoPostProcessToPreserveSlopeInfo");
	pRequest->m_bScriptedRoute = ScanTF(ptr, "*m_bScriptedRoute");
	pRequest->m_bReduceObjectBoundingBoxes = ScanTF(ptr, "*m_bReduceObjectBoundingBoxes");
	pRequest->m_bUseLargerSearchExtents = ScanTF(ptr, "*m_bUseLargerSearchExtents");
	pRequest->m_bDontLimitSearchExtents = ScanTF(ptr, "*m_bDontLimitSearchExtents");
	pRequest->m_bDynamicNavMeshRoute = ScanTF(ptr, "*m_bDynamicNavMeshRoute");
	pRequest->m_bIfStartNotOnPavementAllowDropsAndClimbs = ScanTF(ptr, "*m_bIfStartNotOnPavementAllowDropsAndClimbs");
	pRequest->m_bNeverStartInWater = ScanTF(ptr, "*m_bNeverStartInWater");
	pRequest->m_bIgnoreNonSignificantObjects = ScanTF(ptr, "*m_bIgnoreNonSignificantObjects");
	pRequest->m_bIgnoreTypeVehicles = ScanTF(ptr, "*m_bIgnoreTypeVehicles");
	pRequest->m_bIgnoreTypeObjects = ScanTF(ptr, "*m_bIgnoreTypeObjects");
	pRequest->m_bFleeNeverEndInWater = ScanTF(ptr, "*m_bFleeNeverEndInWater");
	pRequest->m_bConsiderFreeSpaceAroundPoly = ScanTF(ptr, "*m_bConsiderFreeSpaceAroundPoly");
	pRequest->m_bUseMaxSlopeNavigable = ScanTF(ptr, "*m_bUseMaxSlopeNavigable");
	pRequest->m_bRandomisePoints = ScanTF(ptr, "*m_bRandomisePoints");
	pRequest->m_bUseDirectionalCover = ScanTF(ptr, "*m_bUseDirectionalCover");
	pRequest->m_bFavourEnclosedSpaces = ScanTF(ptr, "*m_bFavourEnclosedSpaces");
	pRequest->m_bUseVariableEntityRadius = ScanTF(ptr, "*m_bUseVariableEntityRadius");
	pRequest->m_bAvoidPotentialExplosions = ScanTF(ptr, "*m_bAvoidPotentialExplosions");
	pRequest->m_bAvoidTearGas = ScanTF(ptr, "*m_bAvoidTearGas");
	pRequest->m_bAllowToNavigateUpSteepPolygons = ScanTF(ptr, "*m_bAllowToNavigateUpSteepPolygons");
	pRequest->m_bAllowToPushVehicleDoorsClosed = ScanTF(ptr, "*m_bAllowToPushVehicleDoorsClosed");
	pRequest->m_bMissionPed = ScanTF(ptr, "*m_bMissionPed");
	pRequest->m_bEnsureLosBeforeEnding = ScanTF(ptr, "*m_bEnsureLosBeforeEnding");
	pRequest->m_bExpandStartEndPolyTessellationRadius = ScanTF(ptr, "*m_bExpandStartEndPolyTessellationRadius");
	pRequest->m_bPullFromEdgeExtra = ScanTF(ptr, "*m_bPullFromEdgeExtra");
	pRequest->m_bSofterFleeHeuristics = ScanTF(ptr, "*m_bSofterFleeHeuristics");
	pRequest->m_bAvoidTrainTracks = ScanTF(ptr, "*m_bAvoidTrainTracks");

	// Load movement costs
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fClimbHighPenalty = %f", &pRequest->m_MovementCosts.m_fClimbHighPenalty);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fClimbLowPenalty = %f", &pRequest->m_MovementCosts.m_fClimbLowPenalty);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fDropDownPenalty = %f", &pRequest->m_MovementCosts.m_fDropDownPenalty);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fDropDownPenaltyPerMetre = %f", &pRequest->m_MovementCosts.m_fDropDownPenaltyPerMetre);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fClimbLadderPenalty = %f", &pRequest->m_MovementCosts.m_fClimbLadderPenalty);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fClimbLadderPenaltyPerMetre = %f", &pRequest->m_MovementCosts.m_fClimbLadderPenaltyPerMetre);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fEnterWaterPenalty = %f", &pRequest->m_MovementCosts.m_fEnterWaterPenalty);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fLeaveWaterPenalty = %f", &pRequest->m_MovementCosts.m_fLeaveWaterPenalty);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fBeInWaterPenalty = %f", &pRequest->m_MovementCosts.m_fBeInWaterPenalty);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fClimbObjectPenaltyPerMetre = %f", &pRequest->m_MovementCosts.m_fClimbObjectPenaltyPerMetre);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fWanderPenaltyForZeroPedDensity = %f", &pRequest->m_MovementCosts.m_fWanderPenaltyForZeroPedDensity);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fMoveOntoSteepSurfacePenalty = %f", &pRequest->m_MovementCosts.m_fMoveOntoSteepSurfacePenalty);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fPenaltyForNoDirectionalCover = %f", &pRequest->m_MovementCosts.m_fPenaltyForNoDirectionalCover);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fPenaltyForFavourCoverPerUnsetBit = %f", &pRequest->m_MovementCosts.m_fPenaltyForFavourCoverPerUnsetBit);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_iPenaltyForMovingToLowerPedDensity = %i", &pRequest->m_MovementCosts.m_iPenaltyForMovingToLowerPedDensity);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_iPenaltyPerPolyFreeSpaceDifference = %i", &pRequest->m_MovementCosts.m_iPenaltyPerPolyFreeSpaceDifference);
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "m_fAvoidTrainTracksPenalty = %f", &pRequest->m_MovementCosts.m_fAvoidTrainTracksPenalty);

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "RANDSEED %u", &iRandSeed);
	pRequest->m_iPedRandomSeed = iRandSeed;

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "ENTITY_RADIUS %f", &pRequest->m_fEntityRadius);

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "NUMINCLUDE %i", &pRequest->m_iNumIncludeObjects);
	for(o=0; o<pRequest->m_iNumIncludeObjects; o++)
	{
		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "OBJ %i", &pRequest->m_IncludeObjects[o]);
	}

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "NUMEXCLUDE %i", &pRequest->m_iNumExcludeObjects);
	for(o=0; o<pRequest->m_iNumExcludeObjects; o++)
	{
		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "OBJ %i", &pRequest->m_ExcludeObjects[o]);
	}

	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "NUM_INFLUENCE_SPHERES %i", &pRequest->m_iNumInfluenceSpheres);
	for(o=0; o<pRequest->m_iNumInfluenceSpheres; o++)
	{
		while(*ptr != '*') ptr++;	ptr++;
		Vector3 vOrigin;
		float fRadius, fInner, fOuter;
		sscanf(ptr, "SPHERE %f %f %f %f %f %f",
			&vOrigin.x, &vOrigin.y, &vOrigin.z, &fRadius, &fInner, &fOuter);
		pRequest->m_InfluenceSpheres[o].Init(vOrigin, fRadius, fInner, fOuter);
	}

	// Skip to dynamic objects section
	while(*ptr != '*') ptr++;	ptr++;
	sscanf(ptr, "NUMOBJECTS %i", &iNumObjs);

	for(o=0; o<iNumObjs; o++)
	{
		TDynamicObject * pObj = rage_new TDynamicObject();
		memset(pObj, 0, sizeof(TDynamicObject));

		pObj->m_pEntity = (fwEntity*)(new CEntity());

		while(*ptr != '*') ptr++;	ptr++;
		Vector3 vOrigin;
		sscanf(ptr, "OBJECTPOS %f %f %f", &vOrigin.x, &vOrigin.y, &vOrigin.z);
		pObj->m_Bounds.SetOrigin(vOrigin);

		((CEntity*)pObj->m_pEntity)->m_vPosition = pObj->m_Bounds.GetOrigin();

		float fTmp;
		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "CORNERS %f %f %f   %f %f %f   %f %f %f   %f %f %f",
			&pObj->m_vVertices[0].x, &pObj->m_vVertices[0].y, &fTmp,
			&pObj->m_vVertices[1].x, &pObj->m_vVertices[1].y, &fTmp,
			&pObj->m_vVertices[2].x, &pObj->m_vVertices[2].y, &fTmp,
			&pObj->m_vVertices[3].x, &pObj->m_vVertices[3].y, &fTmp
		);

		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "EDGEPLANES %f %f %f %f   %f %f %f %f   %f %f %f %f   %f %f %f %f   %f %f %f %f   %f %f %f %f",
			&pObj->m_Bounds.m_vEdgePlaneNormals[0].x, &pObj->m_Bounds.m_vEdgePlaneNormals[0].y, &pObj->m_Bounds.m_vEdgePlaneNormals[0].z, &pObj->m_Bounds.m_vEdgePlaneNormals[0].w,
			&pObj->m_Bounds.m_vEdgePlaneNormals[1].x, &pObj->m_Bounds.m_vEdgePlaneNormals[1].y, &pObj->m_Bounds.m_vEdgePlaneNormals[1].z, &pObj->m_Bounds.m_vEdgePlaneNormals[1].w,
			&pObj->m_Bounds.m_vEdgePlaneNormals[2].x, &pObj->m_Bounds.m_vEdgePlaneNormals[2].y, &pObj->m_Bounds.m_vEdgePlaneNormals[2].z, &pObj->m_Bounds.m_vEdgePlaneNormals[2].w,
			&pObj->m_Bounds.m_vEdgePlaneNormals[3].x, &pObj->m_Bounds.m_vEdgePlaneNormals[3].y, &pObj->m_Bounds.m_vEdgePlaneNormals[3].z, &pObj->m_Bounds.m_vEdgePlaneNormals[3].w,
			&pObj->m_Bounds.m_vEdgePlaneNormals[4].x, &pObj->m_Bounds.m_vEdgePlaneNormals[4].y, &pObj->m_Bounds.m_vEdgePlaneNormals[4].z, &pObj->m_Bounds.m_vEdgePlaneNormals[4].w,
			&pObj->m_Bounds.m_vEdgePlaneNormals[5].x, &pObj->m_Bounds.m_vEdgePlaneNormals[5].y, &pObj->m_Bounds.m_vEdgePlaneNormals[5].z, &pObj->m_Bounds.m_vEdgePlaneNormals[5].w
		);
		while(*ptr != '*') ptr++;	ptr++;

		Vector3 tempPlanes[4];
		sscanf(ptr, "SEGPLANES %f %f %f   %f %f %f   %f %f %f   %f %f %f",
			&tempPlanes[0].x, &tempPlanes[0].y, &tempPlanes[0].z,
			&tempPlanes[1].x, &tempPlanes[1].y, &tempPlanes[1].z,
			&tempPlanes[2].x, &tempPlanes[2].y, &tempPlanes[2].z,
			&tempPlanes[3].x, &tempPlanes[3].y, &tempPlanes[3].z
		);

		float fTopZ, fBottomZ;
		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "TOPZ %f", &fTopZ);
		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "BOTTOMZ %f", &fBottomZ);

		pObj->m_Bounds.SetTopZ(fTopZ);
		pObj->m_Bounds.SetBottomZ(fBottomZ);

		while(*ptr != '*') ptr++;	ptr++;
		int iMinX,iMinY,iMinZ,iMaxX,iMaxY,iMaxZ;
		sscanf(ptr, "MINMAX %i %i %i %i %i %i", &iMinX,&iMinY,&iMinZ,&iMaxX,&iMaxY,&iMaxZ);
		pObj->m_MinMax.m_iMinX = (s16)iMinX;
		pObj->m_MinMax.m_iMinY = (s16)iMinY;
		pObj->m_MinMax.m_iMinZ = (s16)iMinZ;
		pObj->m_MinMax.m_iMaxX = (s16)iMaxX;
		pObj->m_MinMax.m_iMaxY = (s16)iMaxY;
		pObj->m_MinMax.m_iMaxZ = (s16)iMaxZ;

		int bIsOpenable;
		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "OPENABLE %i", &bIsOpenable);
		pObj->m_bIsOpenable = bIsOpenable;

		int bIsClimbable;
		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "CLIMBABLE %i", &bIsClimbable);
		pObj->m_bIsClimbable = bIsClimbable;

		int bIsPushable;
		while(*ptr != '*') ptr++;	ptr++;
		sscanf(ptr, "PUSHABLE %i", &bIsPushable);
		pObj->m_bIsPushable = bIsPushable;

		// Error check the bounds of this entity
		static const float fPlaneEps = 0.05f;
		for(int pl=0; pl<4; pl++)
		{
#if __ASSERT
			Vector3 vCorner(pObj->m_vVertices[pl].x, pObj->m_vVertices[pl].y, pObj->m_Bounds.GetOrigin().z);
			float fPlaneDist = pObj->m_Bounds.m_vEdgePlaneNormals[pl].Dot3(vCorner) + pObj->m_Bounds.m_vEdgePlaneNormals[pl].w;
			Assert(Abs(fPlaneDist) < fPlaneEps);
#endif
		}

#ifdef GTA_ENGINE
		pObj->m_bIsActive = false;
#else
		pObj->m_bIsActive = true;
#endif
		pObj->m_bInactiveDueToVelocity = false;

		pObj->m_bFlaggedForDeletion = false;
		pObj->m_pEntity = NULL;
		pObj->m_bCurrentlyCopyingBounds = false;
		pObj->m_bCurrentlyUpdatingNewBounds = false;
		pObj->m_bNewBounds = false;
		pObj->m_bPossibleIntersection = false;

		pObj->m_bNeedsReInsertingIntoGridCells = true;

		pObj->m_pOwningGridCell = NULL;
		pObj->m_pPrevObjInGridCell = NULL;
		pObj->m_pNextObjInGridCell = NULL;


		pObj->m_pNext = m_PathServerThread.m_pFirstDynamicObject;
		m_PathServerThread.m_pFirstDynamicObject = pObj;
	}

	CPathServer::ms_bHaveAnyDynamicObjectsChanged = true;

	LeaveCriticalSection(&m_PathServerThread.m_DynamicObjectsCriticalSection);
	return true;
}

#endif // GTA_ENGINE

#endif


//*************************************************************************************************

#ifdef GTA_ENGINE

void CPathServerGameInterfaceGta::OverridePathRequestParameters(const TRequestPathStruct &reqStruct, CPathRequest &pathRequest) const
{
	if(reqStruct.m_pPed)
	{
		// We don't expect anything not derived from CEntity, at this point.
		Assert(reqStruct.m_pPed->GetIsClassId(CEntity::GetStaticClassId()));

		const CEntity* pEntity = static_cast<const CEntity*>(reqStruct.m_pPed);
		if(pEntity->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			if(pPed->GetPedResetFlag( CPED_RESET_FLAG_RandomisePointsDuringNavigation ))
			{
				pathRequest.m_bRandomisePoints = true;
			}
			pathRequest.m_iPedRandomSeed = (pPed->GetRandomSeed() ^ fwTimer::GetTimeInMilliseconds());

			if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_SEARCH_FOR_PATHS_ABOVE_PED))
			{
				pathRequest.m_fDistanceBelowStartToLookForPoly = 1.0f;
				pathRequest.m_fDistanceAboveStartToLookForPoly = LARGE_FLOAT;
				pathRequest.m_fMaxDistanceToAdjustPathStart = LARGE_FLOAT;
			}

			// Override some of the movement costs based on the ped's stats
			SetPathFindMovementCostsFromPed(pathRequest.m_MovementCosts, pPed);
		}
	}

	//*******************************************************************************
	// If this path is to be found on an entity which owns a dynamic navmesh, then
	// we should note the index of the mesh.  We cannot use CEntity pointers within
	// the pathfinding as it is not threadsafe (even with the RegdPtr mechanism)

	if(reqStruct.m_pEntityPathIsOn)
	{
		LOCK_NAVMESH_DATA;

		// We don't expect anything not derived from CEntity, at this point.
		Assert(reqStruct.m_pEntityPathIsOn->GetIsClassId(CEntity::GetStaticClassId()));
		const CEntity* pEntityPathIsOn = static_cast<const CEntity*>(reqStruct.m_pEntityPathIsOn);

#if __DEV
		Assert(pEntityPathIsOn->GetIsTypeVehicle());
		const CVehicle * pVehicle = (CVehicle*)pEntityPathIsOn;
		Assert(pVehicle->m_nVehicleFlags.bHasDynamicNavMesh);
#endif

		const u32 iModelIndex = pEntityPathIsOn->GetModelIndex();
		Assert(pEntityPathIsOn->GetIsTypeVehicle());
		const TDynamicObjectIndex iDynObjIndex = ((CVehicle*)pEntityPathIsOn)->GetPathServerDynamicObjectIndex();

		int n;
		for(n=0; n<fwPathServer::GetDynamicNavMeshStore().GetNum(); n++)
		{
			CModelInfoNavMeshRef & ref = fwPathServer::GetDynamicNavMeshStore().Get(n);

			if(ref.m_iModelIndex.Get() == (s32)iModelIndex)
			{
#if AI_OPTIMISATIONS_OFF
				Assertf(ref.m_pBackUpNavmeshCopy, "Path request was on a dynamic entity whose model type supports having navmesh, but entity's navmesh wasn't loaded.  Maybe it wasn't streamed in yet?");
#endif
				if(ref.m_pBackUpNavmeshCopy)
				{
					pathRequest.m_iIndexOfDynamicNavMesh = NAVMESH_INDEX_FIRST_DYNAMIC + n;
				}
				else
				{
					pathRequest.m_iIndexOfDynamicNavMesh = NAVMESH_NAVMESH_INDEX_NONE;
				}

#ifdef NAVGEN_TOOL
				pathRequest.m_PathResultInfo.m_MatrixOfEntityAtPathCreationTime = pEntityPathIsOn->GetMatrix();
#else
				pathRequest.m_PathResultInfo.m_MatrixOfEntityAtPathCreationTime = MAT34V_TO_MATRIX34(pEntityPathIsOn->GetMatrix());
#endif

				pathRequest.m_PathResultInfo.m_bFoundDynamicEntityPathIsOn = true;
				pathRequest.m_PathResultInfo.m_iDynObjIndex = iDynObjIndex;
				break;
			}
		}

		Assertf(n!=fwPathServer::GetDynamicNavMeshStore().GetNum(), "Didn't find dynamic navmesh instance to navigate on");
	}

	//*********************************************************************************************************
}


void CPathServerGameInterfaceGta::SetPathFindMovementCostsFromPed(CPathFindMovementCosts& costsOut, const CPed * pPed)
{
	float fMod = pPed->GetPedModelInfo()->GetPersonalitySettings().GetMovementCostModifier();
	Assert(fMod >= 1.f && fMod <= 5.f);

	costsOut.m_fClimbHighPenalty = CPathFindMovementCosts::ms_fDefaultClimbHighPenalty * fMod;
	costsOut.m_fClimbLowPenalty = CPathFindMovementCosts::ms_fDefaultClimbLowPenalty * fMod;
	costsOut.m_fDropDownPenalty = CPathFindMovementCosts::ms_fDefaultDropDownPenalty * fMod;
	costsOut.m_fClimbLadderPenalty = CPathFindMovementCosts::ms_fDefaultClimbLadderPenalty * fMod;
	costsOut.m_fEnterWaterPenalty = CPathFindMovementCosts::ms_fDefaultEnterWaterPenalty * fMod;

	// designers have ability to modify climbing costs with a multiplier
	costsOut.m_fClimbHighPenalty *= pPed->GetPedIntelligence()->GetNavCapabilities().GetClimbCostModifier();
	costsOut.m_fClimbLowPenalty *= pPed->GetPedIntelligence()->GetNavCapabilities().GetClimbCostModifier();

	if( pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_PREFER_TO_AVOID_WATER) == false )
	{
		costsOut.m_fEnterWaterPenalty = 0.0f;
		costsOut.m_fLeaveWaterPenalty = 0.0f;
		costsOut.m_fBeInWaterPenalty = 0.0f;
	}
}

#endif	// GTA_ENGINE
