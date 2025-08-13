
#ifndef PATHSERVER_H
#define PATHSERVER_H

#include "ai/navmesh/datatypes.h"
#include "ai/navmesh/requests.h"
#include "PathServer\PathServerThread.h"
#include "PathServer\NavGenParam.h"

#ifdef GTA_ENGINE
#include "atl/binheap.h"
#include "control/population.h"				// For CPopGenShape
#include "PathServer/PathServer_Store.h"
#include "PathServer/PathServer_Ped.h"
#include "task/Combat/Cover/cover.h"		// For CCoverBoundingArea and CCover::MAX_BOUNDING_AREAS.
#else
#include "viewer_dummystore.h"
#include "GTATypes.h"
#endif

#include "ai/navmesh/pathserverbase.h"

#include "bank/bank.h"

class CEntity;
class CPed;

//************************************************************************************
//
//	Filename : PathServer.h
//	Author : James Broad
//	RockstarNorth (c) 2005
//
//************************************************************************************
//	
//	Overview:
//	---------
//
//	This implementes the CPathServer class, and related classes.
//	The idea is that path requests are made to the CPathServer class by anyone who
//	wants to find a path from A to B in the world.  The CPathServer then returns a
//	handle, and queues the request.  The queued requests are processed by a 
//	separate thread encapsulated in the CPathServerThread class, whose sole purpose
//	is to process path requests and store the results until they are retrieved.
//	Anyone using the CPathServer will need to call the IsRequestReady() function
//	each frame of processing, and if this returns true they can retrieve the points
//	for the path (and some other associated data) with a Lock()/Unlock() syntax.
//	Other request types are possible : a line-of-sight can be tested across the
//	navmeshes (this is actually a line-of-movement) to see if it is possible to
//	get from A to B without obstruction, and an axis-aligned grid can be requested,
//	which describing which areas are walkable or not surrounding a point.
//	The CPathServer class also stores any cover-points which were found during the
//	map analysis - the CCover class interfaces with the path-server to allow these
//	points to be retrieved.
//	Ped generation functions are also contained in the class, and allow suitable
//	coordinates to be found for ped generation - based upon a required 'density'
//	value.  Areas of the map's navmeshes can also be switched on & off as required
//	by missions.  These features mirror the functionality provided by the ped-nodes.
//
//************************************************************************************

class CPathServer : public fwPathServer
{
	friend class CPathServerThread;
	friend class CNavGenApp;
	friend class CNavGen;
	friend class aiNavMeshStoreInterfaceGta;
	friend class CDynamicObjectsContainer;
	friend class CCachedPedGenCoords;
	friend class CNavMeshDataExporter;
	friend class CPathServerAmbientPedGen;
	friend class CPathServerOpponentPedGen;

public:

	CPathServer();
	~CPathServer();

	enum EProcessingMode
	{
		EUndefined,
		ESingleThreaded,	// The Process() function will call the CPathServerThread::Run() function
		EMultiThreaded		// The CPathServerThread will run in the background
	};
	enum EObjectAvoidanceMode
	{
		ENoObjectAvoidance,	// Don't bother testing for dynamic objects
		EPolyTessellation,	// Tessellate polygons intersecting dynamic objects
		EPolyClipping		// Clip polygons intersecting dynamic objects
	};
	enum EPedGenProcessing
	{
		EPedGen_AmbientPeds,		// Ambient peds in the world
		EPedGen_OpponentSpawning	// Enemy AI peds for multiplayer
	};

	static bool ms_bInitialised;
	static bool ms_bGameInSession;
	static bool ms_bStreamingDisabled;
	// Completely disables pathfinding by making every request complete instantly & consist of only the start & end point
	static bool m_bDisablePathFinding;
	static s32 ms_iTimeToNextRequestAndEvict;
	static bool ms_bHaveAnyDynamicObjectsChanged;
	static bool ms_bNoNeedToCheckObjectsForThisPoly;
	static EObjectAvoidanceMode m_eObjectAvoidanceMode;
	static float ms_fDefaultDynamicObjectPlaneEpsilon;
	static bank_float ms_fDynamicObjectVelocityThreshold;
	static bank_bool ms_bTestConnectionPolyExit;
	static bank_bool ms_bDisallowClimbObjectLinks;

	// DEBUGGING TOGGLES ONLY (TODO: change to bank_vars)
	static bank_s32 ms_iRequestAndEvictFreqMS;
	static bank_bool ms_bRefinePaths;
	static bank_bool ms_bSmoothRoutesUsingSplines;
	static bank_bool ms_bCutCornersOffAllPaths;
	static bank_bool ms_bMinimisePathDistance;
	static bank_bool ms_bMinimiseBeforeRefine;
	static bank_bool ms_bPullPathOutFromEdges;
	static bank_bool ms_bPullPathOutFromEdgesTwice;
	static bank_bool ms_bPullPathOutFromEdgesStringPull;
	static bank_bool ms_bDoPolyTessellation;
	static bank_bool ms_bDoPolyUnTessellation;
	static bank_bool ms_bDoPreemptiveTessellation;
	static bank_bool ms_bUseGridCellCache;
	static bank_bool ms_bUseTessellatedPolyObjectCache;
	static bank_bool ms_bAlwaysUseMorePointsInPolys;
	//static bank_bool ms_bSphereRayEarlyOutTestOnObjects;
	static bank_bool ms_bUseLosToEarlyOutPathRequests;
	static bank_bool ms_bDontRevisitOpenNodes;						// If set, this means the pathsearch can never revisit any poly once visited
	static bank_bool ms_bAllowTessellationOfOpenNodes;
	static bank_bool ms_bOutputDetailsOfPaths;
	static bank_bool ms_bTestObjectsForEveryPoly;
	static bank_bool ms_bUseOptimisedPolyCentroids;
	static bank_bool ms_bUseGridToCullObjectLOS;
	static bank_bool ms_bUseNewPathPriorityScoring;
	// END DEBUGGING TOGGLES

	// Initialise the path-server, and create the path server thread on the specified processor
	// The 'pPathForNavMeshes' parameter is used for debugging purposes when streaming is not active
	static bool Init(const fwPathServerGameInterface &gameInterface, EProcessingMode eProcessingMode = EMultiThreaded, u32 iProcessorIndex = 0, const char * pRelativePathAndFilenameForNavMeshes = NULL, const char * pPathForDatFile = NULL, const char * pDebugPathForNavFiles = NULL);

	static void RegisterStreamingModule(const char* pPathForDatFile = NULL);

	static void DisableStreaming(bool b) { ms_bStreamingDisabled = b; }

	// Resets the navmesh system, unloads navmeshes, etc.
	static void Reset(const bool bLoadingNewMap=false);
	static void Shutdown();

	static void InitBeforeMapLoaded(unsigned iInitMode);
	static void InitSession(unsigned iInitMode);
	static void ShutdownSession(unsigned iShutdownMode);

	static void ReadIndexMappingFiles();

	static bool ReadDatFile(const char * pGameDataPath, CNavDatInfo & navMeshesInfo, CNavDatInfo & navNodesInfo);

	// Sets where the navigation system streams its navmeshes from.
	static bool SetNavMeshesImageFile(char * pRelativePath, char * pFilename);

	// Called per-frame to update the player's/viewer's origin
	static void Process(void);
	static void Process(const Vector3 & vOrigin);

#if __HIERARCHICAL_NODES_ENABLED
	static aiNavNodesStore * GetNavNodesStore() { return m_pNavNodesStore; }
#endif
#ifdef GTA_ENGINE

	static void FloodFillNodesForPedDeficit(const Vector3 & vCentrePos, CHierarchicalNavNode * pNode, CHierarchicalNavData * pNavData);

	static void ForcePerformThreadHousekeeping(bool b);

#endif


	//*******************************************************************************
	//	Functions to control extracting water-edges from navmeshes, for use by audio

	// Set the origin of the position to scan for water-edges
	static void SetWaterEdgeParams(const Vector3 & vOrigin);

	// Retrieve the water edges which we have found.
	// pOutPts must point to an array no less than SIZE_OF_WATEREDGES_BUFFER in size.
	// This function may return false if the pathfinding thread was busy at the time of querying.
	static bool GetWaterEdgePoints(int & iOutNumPts, Vector3 * pOutPoints, u32 * pOutPerPolyAudioProperties=NULL);




	// Set/get the distance from the current origin within which navmeshes are streamed into memory
	static inline void SetNavMeshLoadDistance(float fDist) { m_fNavMeshLoadProximity = fDist; }
	static inline float GetNavMeshLoadDistance(void) { return m_fNavMeshLoadProximity; }

	// Called to add cover points near the origin, to the global CCover class
	static void AddCoverPoints(const Vector3 & vOrigin, float fMaxDistance, int maxNumPointsToAdd);

#ifdef GTA_ENGINE

	static CPathServerAmbientPedGen & GetAmbientPedGen() { return m_AmbientPedGeneration; }
	static CPathServerOpponentPedGen & GetOpponentPedGen() { return m_OpponentPedGeneration; }

#endif


	// TODO: remove this once we have a fwPathServerThread to contain the OnFirstVisitingPoly() function
	static bool OnFirstVisitingPolyCallback(TNavMeshPoly * pPoly);

	//***************************************************************************************
	//	Functions to do with requesting path or other queries from the path sever.  Clients
	//	keep a handle to the query, and can then poll for completion in subsequent frames.

	// General purpose RequestPath() function
	static TPathHandle RequestPath(const TRequestPathStruct & reqStruct);

	// (NB: This multi-parameter version will be removed shortly..)
	static TPathHandle RequestPath(
		const Vector3 & vPathStart,				// The start point of the path
		const Vector3 & vPathEnd,				// The end point of the path
		const Vector3 & vReferenceVector,		// reference vector (not always used)
		float fReferenceDistance,				// reference distance (not always used)
		u64 iFlags,								// path flags
		float fCompletionRadius,				// if exact path cannot be found, try to get within this range of target
		u32 iNumInfluenceSpheres,				// num influence spheres
		TInfluenceSphere * pInfluenceSpheres,	// pointer to an array of influence spheres
		fwEntity * pPed,						// context - for debugging (typically use the ped pointer)
		const fwEntity * pEntityPathIsOn		// pointer to the entity this path is known to be on (if it has a dynamic navmesh attached)
	);

	// Simplified interface for basic paths
	static TPathHandle RequestPath(const Vector3 & vPathStart, const Vector3 & vPathEnd, u64 iPathStyleFlags, float fCompletionRadius = 0.0f, fwEntity * pPed = NULL);

	// Requests a path-search which starts at vStartPos, and spreads out attempting to find a position which has a clear navmesh
	// line-of-sight to vVantagePos.  Will favour spreading out in vPreferredDirection, and will not spread out further than fMaxDistToLook.
	static TPathHandle RequestVantagePoint(const Vector3 & vStartPos, const Vector3 & vVantagePos, const Vector3 & vPreferredDirection, float fMaxDistToLook);

	// The method for getting the result path of the path request
	static EPathServerErrorCode LockPathResult(TPathHandle hPath, int * iNumPoints, Vector3 *& pPoints, TNavMeshWaypointFlag *& pWaypointFlags)
	{
		return LockPathResult(hPath, iNumPoints, pPoints, pWaypointFlags, NULL);
	}
	static EPathServerErrorCode LockPathResult(TPathHandle hPath, int * iNumPoints, Vector3 *& pPoints, TNavMeshWaypointFlag *& pWaypointFlags, TPathResultInfo * pPathInfo);
	static bool UnlockPathResultAndClear(TPathHandle hPath);

	// General function to see if a request result is ready yet
	static EPathServerRequestResult IsRequestResultReady(TPathHandle hPath);
	// Function to see if a request result is ready, when the type of the request is known
	static EPathServerRequestResult IsRequestResultReady(TPathHandle hPath, EPathServerRequestType eType);

	// Cancels a pending/active/completed path request
	static bool CancelRequest(TPathHandle handle);

	// Requests a navmesh line-of-sight query from vStart to vEnd, for an entity of fRadius.  NB: the fRadius parameter is *currently ignored*
	static TPathHandle RequestLineOfSight(const Vector3 & vStart, const Vector3 & vEnd, float fRadius, bool bDynamicObjects = true, bool bNoLosAcrossWaterBoundary = false, bool bStartInWater = false, int iNumExcludeObjects = 0, CEntity ** ppExcludeObjects = NULL, const float fMaxSlopeAngle=0.0f, void * pContext=NULL, aiNavDomain domain = kNavDomainRegular);
	static TPathHandle RequestLineOfSight(const Vector3 * vPts, int iNumPts, float fRadius, bool bDynamicObjects = true, bool bQuitAtFirstLosFail = true, bool bNoLosAcrossWaterBoundary = false, bool bStartInWater = false, int iNumExcludeObjects = 0, CEntity ** ppExcludeObjects = NULL, const float fMaxSlopeAngle=0.0f, void * pContext=NULL, aiNavDomain domain = kNavDomainRegular);

	// Request the audio properties of the navmesh poly underneath the input position, or the closest nearby navmesh poly found
	static TPathHandle RequestAudioProperties(const Vector3 & vPosition, TNavMeshAndPoly * pKnownNavmeshPosition = NULL, const bool bPriorityRequest = false, fwEntity * pPed = NULL, aiNavDomain domain = kNavDomainRegular)
	{
		return RequestAudioProperties(vPosition, pKnownNavmeshPosition, 0.0f, bPriorityRequest, pPed, domain);
	}
	static TPathHandle RequestAudioProperties(const Vector3 & vPosition, TNavMeshAndPoly * pKnownNavmeshPosition, float fRadius, const bool bPriorityRequest = false, fwEntity * pPed = NULL, aiNavDomain domain = kNavDomainRegular)
	{
		return RequestAudioProperties(vPosition, pKnownNavmeshPosition, fRadius, Vector3(0.0f, 0.0f, 0.0f), bPriorityRequest, pPed, domain);
	}
	static TPathHandle RequestAudioProperties(const Vector3 & vPosition, TNavMeshAndPoly * pKnownNavmeshPosition, float fRadius, const Vector3 & vDirection, const bool bPriorityRequest = false, void * pContext = NULL, aiNavDomain domain = kNavDomainRegular);

	// Request a grid sampling the walkable regions around the origin.  (Not currently used for anything in-game)
	static TPathHandle RequestGrid(const Vector3 & vOrigin, u32 iSize, float fResolution, void * pContext = NULL, aiNavDomain domain = kNavDomainRegular);

	// Copies the data into the 'destGrid', and thereafter hPath is invalid.
	static EPathServerErrorCode GetGridResultAndClear(TPathHandle hPath, CWalkRndObjGrid & destGrid);

	// Gets the result of a LineOfSight query, and clears the handle after
	static EPathServerErrorCode GetLineOfSightResult(TPathHandle handle, bool & bLineOfSightIsClear);
	// Get the results of a LineOfSight query which consisted of multiple line-segments (ie. a polyline)
	// The overall result for all segments is returned in bLineOfSightIsClear, and individual results copied into bLineOfSightResults.
	static EPathServerErrorCode GetLineOfSightResult(TPathHandle handle, bool & bLineOfSightIsClear, bool * bLineOfSightResults, int iNumPts);

	// Copies the result of the audio properties query into iAudioProperties, and clears the request
	static EPathServerErrorCode GetAudioPropertiesResult(TPathHandle handle, u32 & iAudioProperties);
	// Copies an array of audio properties & poly areas.  The input pAudioProperties & pPolyAreas *must* point to arrays of MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES in size!
	static EPathServerErrorCode GetAudioPropertiesResult(TPathHandle handle, u32 & iNumProperties, u32 * pAudioProperties, float * pPolyAreas, Vector3 * pPolyCentroids, u8 * pAdditionalFlags);

	// Requests a flood-fill search, to find the closest accessible instance of "eFloodFillType" within the given radius
	static TPathHandle RequestClosestViaFloodFill(const Vector3 & vStartPos, const float fMaxRadius, CFloodFillRequest::EType eFloodFillType, const bool bCheckDynamicObjects, const bool bUseClimbsAndDrops, const bool bUseLadders, void * DEV_ONLY(pContext), aiNavDomain domain = kNavDomainRegular);
	// Gets the result of a floodfill search
	static EPathServerErrorCode GetClosestFloodFillResultAndClear(const TPathHandle hPath, Vector3 & vClosest, float & fValue, const CFloodFillRequest::EType eType);

	// Requests a flood-fill search, to find the closest accessible carnode within the given radius
	static TPathHandle RequestClosestCarNodeSearch(const Vector3 & vStartPos, const float fMaxRadius, void * pContext=NULL);
	// Gets the result of a search for the closest car node
	static EPathServerErrorCode GetClosestCarNodeSearchResultAndClear(const TPathHandle hPath, Vector3 & vClosestCarNode);

	// Requests a flood-fill search, to find the closest accessible sheltered navmesh poly within the given radius
	static TPathHandle RequestClosestShelteredPolySearch(const Vector3 & vStartPos, const float fMaxRadius, void * pContext=NULL);
	// Gets the result of a search for a sheltered poly
	static EPathServerErrorCode GetClosestShelteredPolySearchResultAndClear(const TPathHandle hPath, Vector3 & vClosestShelteredPoly);

	// Requests a flood-fill search, to find the closest accessible unsheltered navmesh poly within the given radius
	static TPathHandle RequestClosestUnshelteredPolySearch(const Vector3 & vStartPos, const float fMaxRadius, void * pContext=NULL);
	// Gets the result of a search for a unsheltered poly
	static EPathServerErrorCode GetClosestUnshelteredPolySearchResultAndClear(const TPathHandle hPath, float & vClosestShelteredPoly);

	// Requests a flood-fill to calculate the area of connected navmesh which is under the input position (using the same
	// methods to determine the starting-poly as the regular pathserach does)
	static TPathHandle RequestCalcAreaUnderfootSearch(const Vector3 & vStartPos, const float fMaxRadius, void * pContext);
	// Retrieves the area found by the above search
	static EPathServerErrorCode GetAreaUnderfootSearchResultAndClear(const TPathHandle hPath, float & fArea);

	static TPathHandle RequestClearArea(const Vector3 & vSearchOrigin, const float fSearchRadiusXY, const float fSearchDistZ, const float fDesiredClearRadius, const float fOptionalMinimumRadius, const bool bConsiderDynamicObjects, const bool bSearchInteriors, const bool bSearchExterior, const bool bConsiderWater, const bool bConsidereSheltered, void * DEV_ONLY(pContext), aiNavDomain domain);
	static EPathServerErrorCode GetClearAreaResultAndClear(const TPathHandle hPath, Vector3 & vResultOrigin);

	// Request a flood-fill search to find the closest pavement navmesh poly within a given radius.
	static TPathHandle RequestHasNearbyPavementSearch(const Vector3& vStartPos, const float fMaxRadius, void* pContext = NULL);
	static EPathServerErrorCode GetHasNearbyPavementSearchResultAndClear(const TPathHandle hPath);

	static TPathHandle RequestClosestPosition(const Vector3 & vSearchOrigin, const float fSearchRadius, const u32 iFlags, const float fZWeightingAbove, const float fZWeightingAtOrBelow, const float fMinimumSpacing, const s32 iNumAvoidSpheres, const spdSphere * pAvoidSpheres, const s32 iMaxNumResults, void * pContext, aiNavDomain domain);
	static EPathServerErrorCode GetClosestPositionResultAndClear(const TPathHandle hPath, CClosestPositionRequest::TResults & results);

	// Tests a line-of-sight against the navmesh immediately, bypassing the processing thread (ie. this function is intended to be called from the
	// main game thread - and musn't be used by the pathfinder).  The LOS must be short, and will fail if over SHORT_LINE_OF_SIGHT_MAXDIST in
	// length.  This function does not consider dynamic objects.
	static TNavMeshPoly * TestShortLineOfSightImmediate(const Vector3 & vStart, const Vector3 & vEnd, Vector3 & vIsectPos, aiNavDomain domain);

	static TNavMeshPoly * TestShortLineOfSightImmediate(const Vector3 & vStartPos, const Vector2 & vLosDir, TNavMeshPoly * pStartPoly, Vector3 * pvOut_IntersectionPos, Vector3 * pvOut_HitEdgeVec, int * pOut_iEdgeHitIndex, aiNavDomain domain);
	static TNavMeshPoly * TestShortLineOfSightImmediate_R(const Vector3 & vStartPos, const Vector2 & vDir, TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, const TShortMinMax & endMinMax, Vector3 & vOut_IsectPos, aiNavDomain domain);
	static bool TestShortLineOfSightImmediate_RG(Vector3& o_Vertex1, Vector3& o_Vertex2, const Vector3& vStartPos, const Vector3& vEndPos, TNavMeshPoly& testPoly, aiNavDomain domain);

	static TNavMeshPoly * SlidePointAlongNavMeshEdgeImmediate(const Vector3 & vStartPos, const Vector2 & vLosDir, TNavMeshPoly * pPoly, int & iInOut_HitEdge, Vector3 & vOutPos, aiNavDomain domain);

	enum eImmediatePathResult
	{
		// Everything was fine
		IMPath_NoError,
		// There was no navmesh underneath the input coordinates
		IMPath_ENoNavMeshAtCoords,
		// No path could be found.  This could mean there is nowhere to go.
		IMPath_ENoPathFound,
		// A flee path has become stuck and cannot find its way out of a dead-end
		IMPath_EStuckInDeadEnd
	};

#if __TRACK_PEDS_IN_NAVMESH
	// Updates the tracking of all the CPeds in the game
	static void UpdatePedTracking();
	static void InvalidateTrackers(const u32 iNavMesh=NAVMESH_NAVMESH_INDEX_NONE);
#endif

	// Switched ped generation & wandering on/off for the given region
	static void SwitchPedsOnInArea(const Vector3 & vMin, const Vector3 & vMax, scrThreadId iScriptUID);
	static void SwitchPedsOffInArea(const Vector3 & vMin, const Vector3 & vMax, scrThreadId iScriptUID);

	static void DisableNavMeshInArea(const Vector3 & vMin, const Vector3 & vMax, scrThreadId iScriptUID);
	static void ReEnableNavMeshInArea(const Vector3 & vMin, const Vector3 & vMax, scrThreadId iScriptUID);

	// Removes ped switch areas within region, and resets all polygons to ok for ped generation & wandering
	static void ClearPedSwitchAreas(const Vector3 & vMin, const Vector3 & vMax);
	// Removes all the ped switch areas which were originally created by the specified script (for use during mission cleanup)
	static void ClearPedSwitchAreasForScript(scrThreadId iScriptUID);
	// Removes all ped switch areas, and resets all polygons to ok for ped generation & wandering
	static void ClearAllPedSwitchAreas(void);

	static bool IsCoordInPedSwitchedOffArea(const Vector3 & vPos);

	// Defines a sphere within which will be the only area that peds can be created (provided other pedgen criteria are satisifed)
	static void DefinePedGenConstraintArea(const Vector3 & vPos, const float fRadius);
	// Destroy a constraint area, and go back to normal ped generation
	static void DestroyPedGenConstraintArea();

	// Adds a blocking object, which will block pathfind for the given types of path
	static TDynamicObjectIndex AddBlockingObject(const Vector3 & vPosition, const Vector3 & vSize, const float fHeading, const s32 iBlockingFlags=TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
	// Update a blocking object with new position/orientation
	static void UpdateBlockingObject(const TDynamicObjectIndex iObjIndex, const Vector3 & vPosition, const Vector3 & vSize, const float fHeading, bool bForceUpdate, u32 * pBlockingFlags=NULL);
	// Removes a previously added object
	static bool RemoveBlockingObject(const TDynamicObjectIndex iObjIndex);

	// Schedule the climb at the given position to be disabled asap.  In practice this will be the next time that
	// the CPathServerThread runs its DoHousekeeping() function during idle time between requests.
	// Climb type must be one or both of the supplied booleans, but not neither.
	static void DeferredDisableClimbAtPosition(const Vector3 & vPos, const bool bNavmeshClimb, const bool bClimbableObject);

	// Disables a climb at the given position.  If a ped is repeatedly unable to do a climb whilst following a navmesh
	// route, then we will have to disable the climb which will force them (and other) to try an alternative route.
	// "bClimbableObject" indicates a cimbable-object climb (eg. fence), otherwise this is a navmesh climb.
	static bool DisableClimbAtPosition(const Vector3 & vPos, const bool bClimbableObject);

	// Finds a safe position to place a ped, which is on the navmesh.  If bOnlyOnPavement is set, then
	// only positions on pavements are considered & the function may fail if there are no pavements anywhere.
	static EGetClosestPosRet GetClosestPositionForPed(const Vector3 & vNearThisPosition, Vector3 & vecOut, const float fMaxSearchRadius=15.0f, const u32 iFlags=GetClosestPos_ClearOfObjects, const bool bFailIfNoThreadAccess=false, const int iNumAvoidSpheres=0, const spdSphere * pAvoidSpheresArray=NULL, aiNavDomain domain = kNavDomainRegular);

	// Finds a car node close to the input position which is clear of nearby objects, vehicles, etc.
	static bool GetClosestClearCarNodeForPed(const Vector3 & vNearThisPosition, Vector3 & vecOut, const float fMaxSearchRadius=15.0f);

	static bool TestNavMeshPolyUnderPosition(const Vector3 & vPosition, Vector3& pOutPos, const bool bNotIsolated, const bool bClearOfDynamicObjects, aiNavDomain domain = kNavDomainRegular);

	// Requests navmeshes to be loaded in the specified area
	static bool AddNavMeshRegion( const NavMeshRequiredRegion region, const scrThreadId iThreadId, const float fOriginX, const float fOriginY, const float fRadius );
	static bool RemoveNavMeshRegion( const NavMeshRequiredRegion region, const scrThreadId iThreadId );
	static scrThreadId GetNavmeshMeshRegionScriptID();
	static bool GetIsNavMeshRegionRequired( const NavMeshRequiredRegion region );
	static void RemoveAllNavMeshRegions();
	static void CountScriptMemoryUsage(u32& nVirtualSize, u32& nPhysicalSize);

	// Returns whether the navmeshes around the given area are loaded
	static bool AreAllNavMeshRegionsLoaded(aiNavDomain domain = kNavDomainRegular);

	static bool UnloadAllMeshes(bool bForceRemove);
	static bool LoadAllHierarchicalData(const char * pPathForHierarchicalData);

#ifdef GTA_ENGINE
	static s32 GetNavMeshStreamingModuleId(aiNavDomain domain)
	{
		if(ms_bStreamingDisabled)
			return -1;
		return m_pNavMeshStores[domain]->GetStreamingModuleId();
	}
#if __HIERARCHICAL_NODES_ENABLED
	static s32 GetHierarchicalNodesStreamingModuleId() { return m_pNavNodesStore->GetStreamingModuleId(); }
#endif
	static void SuspendThread(void);
	static void ResumeThread(void);
#else
	static bool LoadAllMeshes(const char * pPathForNavMeshes);
#endif

	static bool m_bGameRunning;
	static u32 ms_iBlockRequestsFlags;

	enum
	{
		BLOCK_REQUESTS_ON_REQUEST_AND_EVICT		=	0x01,
		BLOCK_REQUESTS_ON_ADD_DYNAMIC_OBJECTS	=	0x02,
		BLOCK_REQUESTS_ON_DEFRAG_NAVMESHES		=	0x04
	};

	static void ApplyPedAreaSwitchesToNewlyLoadedNavMesh(CNavMesh * pNavMesh, aiNavDomain domain);

	static const fwPathServerGameInterface& GetGameInterface()
	{	Assert(m_GameInterface);
		return *m_GameInterface;
	}

#if !__FINAL && __BANK
	// Outputs a text ".prob" file with the path request & dynamic objects' positions
	static bool OutputPathRequestProblem(u32 iPathRequestIndex, const char * pFilename=NULL);
	static bool OutputPathRequestProblem(CPathRequest * pRequest, const char * pFilename=NULL);
#ifndef GTA_ENGINE
	static bool LoadPathRequestProblem(void);
	static bool LoadPathRequestProblem(char * filename);
#endif
#endif

#if !__FINAL

	static void InitWidgets();

	static void Visualise(void);
	static void VisualiseNavMesh(CNavMesh * pNavMesh, const Vector3 & vOrigin, const float fHeightAboveMesh, const bool bIsMainNavMeshUnderFoot, aiNavDomain domain);
	static void VisualiseHierarchicalNavData(CHierarchicalNavData * pHierNav, const Vector3 & vOrigin, const float fVisDist);
	static void VisualiseAdjacency(const Vector3 & vPosStart, const Vector3 & vPosEnd, eNavMeshPolyAdjacencyType adjacencyType, bool bIsHighDrop, bool bIsDisabled, s32 iAlpha);
	static void VisualiseSpecialLinks(CNavMesh * pNavMesh, aiNavDomain domain);
	static void RenderMiscThings();
	static bool TransformWorldPosToScreenPos(const Vector3 & vWorldPos, Vector2 & vScreenPos);

	static int VisualisePathServerCommon(aiNavDomain domain, int ypos);
	static int VisualiseCurrentNavMeshDetails(int ypos, CNavMesh * pNavMesh);

	static void VisualiseText(int ypos, aiNavDomain domain);
	static void VisualisePathRequests(int ypos, aiNavDomain domain);
	static void VisualisePathDetails(CPathRequest * pPathReq, int ypos);
	static void VisualiseLineOfSightRequests(int ypos, aiNavDomain domain);
	static void VisualiseAudioPropertiesRequests(int ypos, aiNavDomain domain);
	static void VisualiseFloodFillRequests(int ypos, aiNavDomain domain);
	static void VisualiseClearAreaRequests(int ypos, aiNavDomain domain);
	static void VisualiseClosestPositionRequests(int ypos, aiNavDomain domain);
	static void VisualiseMemoryUsage(int ypos, aiNavDomain domain);
	static void VisualisePolygonInfo(int ypos, CNavMesh * pNavMesh, const Vector3 & vOrigin, aiNavDomain domain);
	static void VisualiseTestPathEndPoints();
	static void DrawPolyDebug(CNavMesh * pNavMesh, u32 iPolyIndex, Color32 iCol, float fAddHeight, bool bOutline, int filledAlpha = 255, bool bDrawCentroidQuick=false);
	static void DrawPolyVolumeDebug(CNavMesh * pNavMesh, int iPolyIndex, Color32 iCol1, Color32 iCol2, float fSpaceAbove);
	static void DebugPolyText(CNavMesh * pNavMesh, u32 iPolyIndex);

#if __BANK
	static void DebugOutPathRequestResult(CPathRequest * pPathRequest);
	static void PathServer_TestCalcPolyAreas();
#endif

	static float ms_fDebugVisPolysMaxDist;
	static float ms_fDebugVisNodesMaxDist;
	static bool m_bDebugPolyUnderfoot;
	static bool ms_bDebugPathFinding;
	static bool ms_bMarkVisitedPolys;
	static bool ms_bUnMarkVisitedPolys;
	static bool ms_bDrawDynamicObjectGrids;
	static bool ms_bDrawDynamicObjectMinMaxs;
	static bool ms_bDrawDynamicObjectAxes;
	static bool ms_bRenderMiscThings;
	static bool ms_bEnsureLosBeforeEnding;

	enum eNavMeshVisMode
	{
		NavMeshVis_Off,
		NavMeshVis_NavMeshWireframe,
		NavMeshVis_NavMeshTransparent,
		NavMeshVis_NavMeshSolid,
		NavMeshVis_Pavements,
		NavMeshVis_Roads,
		NavMeshVis_TrainTracks,
		NavMeshVis_PedDensity,
		NavMeshVis_Disabled,
		NavMeshVis_Shelter,
		NavMeshVis_HasSpecialLinks,
		NavMeshVis_TooSteep,
		NavMeshVis_WaterSurface,
		NavMeshVis_AudioProperties,
		NavMeshVis_CoverDirections,
		NavMeshVis_CarNodes,
		NavMeshVis_Interior,
		NavMeshVis_Isolated,
		NavMeshVis_NetworkSpawnCandidates,
		NavMeshVis_NoWander,
		NavMeshVis_PavementCheckingMode,

		NavMeshVis_NumModes
	};
	static const char * ms_pNavMeshVisModeTxt[NavMeshVis_NumModes];
	static s32 m_iVisualiseNavMeshes;
	static bool m_bVisualiseHierarchicalNodes;
	static bool m_bVisualiseLinkWidths;
	static bool m_bVisualisePaths;
	static bool m_bVisualiseQuadTrees;
	static bool m_bVisualiseAllCoverPoints;
	static float m_fVisNavMeshVertexShiftAmount;

	static int m_iVisualiseDataSet;

	enum ePathServerVisMode
	{
		PathServerVis_Off,
		PathServerVis_PathRequests,
		PathServerVis_LineOfSightRequests,
		PathServerVis_AudioRequests,
		PathServerVis_FloodFillRequests,
		PathServerVis_ClearAreaRequests,
		PathServerVis_ClosestPositionRequests,
		PathServerVis_Text,
		PathServerVis_MemoryUsage,
		PathServerVis_CurrentNavMesh,
		PathServerVis_PolygonInfo,

		PathServerVis_NumModes
	};
	static s32 m_iVisualisePathServerInfo;
	static s32 m_iObjAvoidanceModeComboIndex;

#if __BANK
	static bkCombo * ms_pObjAvoidanceModeCombo;
#endif	// __BANK

	static sysPerformanceTimer	* m_RequestTimer;
	static sysPerformanceTimer	* m_PedGenTimer;
	static sysPerformanceTimer	* m_NewPedGenTimer;
	static sysPerformanceTimer	* m_MainGameThreadStallTimer;
	static sysPerformanceTimer	* m_MiscTimer;
	static sysPerformanceTimer	* m_ImmediateModeTimer;
	
	static float m_fTimeTakenToIssueRequestsInMSecs;
	static float m_fTimeTakenToAddRemoveUpdateObjects;
	static float m_fTimeTakenToDoPedGenInMSecs;

	static bool m_bDebugShowLowClimbOvers;
	static bool m_bDebugShowHighClimbOvers;
	static bool m_bDebugShowDropDowns;
	static bool m_bDebugShowSpecialLinks;
	static bool m_bDebugShowPolyVolumesAbove;
	static bool m_bDebugShowPolyVolumesBelow;

	static bool m_bVerifyAdjacencies;
	static bool m_bRepeatPathQuery;
	static bool ms_bUseXTraceOnTheNextPathRequest;
	
	static s32 m_iSelectedPathRequest;
	static s32 m_iSelectedLosRequest;
	static bool ms_bOnlyDisplaySelectedRoute;
	static bool ms_bShowSearchExtents;

	// Toggles
	static bool m_bDisableObjectAvoidance;
	static bool m_bDisableAudioRequests;
	static bool m_bDisableGetClosestPositionForPed;
	static bool ms_bAllowObjectClimbing;
	static bool ms_bAllowNavMeshClimbs;
	static bool ms_bAllowObjectPushing;
	
	// Stress-test pathfinding, by requesting 10 paths every frame
	static bool m_bStressTestPathFinding;
	static bool m_bStressTestGetClosestPos;
	static bool m_bVisualisePolygonRequests;
	static TPathHandle m_iStressTestPathHandles[NUM_STRESS_TEST_PATHS];

	static int m_iDebugThreadRunMode;

	static Vector3 ms_vPathTestStartPos;
	static Vector3 ms_vPathTestEndPos;
	static Vector3 ms_vPathTestCoverOrigin;
	static Vector3 ms_vPathTestReferenceVector;
	static float ms_fPathTestReferenceDist;
	static float ms_fPathTestCompletionRadius;
	static float ms_fPathTestEntityRadius;
	static int ms_iPathTestNumInfluenceSpheres;
	static TInfluenceSphere ms_PathTestInfluenceSpheres[MAX_NUM_INFLUENCE_SPHERES];

	static float ms_fPathTestInfluenceSphereRadius;
	static float ms_fPathTestInfluenceMin;
	static float ms_fPathTestInfluenceMax;

	static inline CPathRequest * GetPathRequest(s32 iIndex)
	{
		Assert(iIndex >= 0 && iIndex < MAX_NUM_PATH_REQUESTS);
		return &m_PathRequests[iIndex];
	}

	static void SpewOutCoverAreasRequired();

#endif // !__FINAL

public:

	static void RequestAndEvictNextFrame();

	static bool IsModelIndexConsideredAnObstacle(u32 iModelIndex);

	// A running total of the memory used by navmeshes *only*
//	static s32 m_iTotalMemoryUsedByNavMeshes;

	static inline CPathServerThread * GetPathServerThread(void) { return &m_PathServerThread; }

	static u32 GetNavMeshIndexFromPosition(const Vector3 & vPos, aiNavDomain domain);
	static u32 NavMeshIndexToNavNodesIndex(const TNavMeshIndex iNavMesh);
	static u32 NavNodesIndexToNavMeshIndex(const TNavMeshIndex iNavNodes);
	static CHierarchicalNavData * GetHierarchicalNavFromNavMeshIndex(const u32 index);
	static u32 GetAdjacentNavMeshIndex(u32 iCurrentNavMeshIndex, eNavMeshEdge iDir, aiNavDomain domain);
	static bool GetDoNavMeshesAdjoin(const u32 iNavMesh1, const u32 iNavMesh2, aiNavDomain domain);
	static s32 GetNavMeshesIntersectingPosition(const Vector3 & vPos, float fRadius, TNavMeshIndex * pIndices, aiNavDomain domain);
	static CNavMesh * FindClosestLoadedNavMeshToPosition(const Vector3 & vPos, aiNavDomain domain);

	// Thread-safe function to tell whether a navmesh is loaded at a position
	static bool IsNavMeshLoadedAtPosition(const Vector3 & vPos, aiNavDomain meshDataSet);

	// This dynamic navmesh function is ONLY safe to be run from the main game thread
	//	static void UpdateDynamicNavMeshMatrix(CDynamicNavMeshEntry & entry);

#if __TRACK_PEDS_IN_NAVMESH
    static void UpdateTrackedObject(CNavMeshTrackedObject & obj, const Vector3 & vObjectPos, const float rescanProbeLength = CNavMeshTrackedObject::DEFAULT_RESCAN_PROBE_LENGTH, bool in_bNeedsToReScan = false);
	static bool TrackedObjectNeedsRescan(const CNavMeshTrackedObject& obj, const Vector3& vObjectPos);
#endif // __TRACK_PEDS_IN_NAVMESH

protected:		// Was private, now protected, so CPathServerGta can access the internals.

	// Cancels a pending/active/completed path request
	static bool CancelRequestInternal(TPathHandle handle, bool bCancelledByClient);

	static bool FlagDynamicObjectForRemoval(const TDynamicObjectIndex iDynObjIndex);
	static bool TestPolyIntersectionAgainstObjectPlanes(const TDynamicObject * pObj, const TNavMeshPoly * pPoly, const Vector3 * pPolyVerts);

	//*******************************************************************************************************
	//	Immediate-mode private pathfinding functions & members

	static bool TestShortLineOfSightImmediateR(const Vector3 & vStartPos, const Vector3 & vEndPos, const TNavMeshPoly * pToPoly, TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, aiNavDomain domain);

	static CPathSearchPriorityQueue * m_pImmediateModePrioriryQueue;
	static TNavMeshPoly * m_pImmediateModeVisitedPolys[IMMEDIATE_MODE_QUERY_MAXNUMVISITEDPOLYS];
	static int m_iImmediateModeNumVisitedPolys;

	static inline void ResetImmediateModeVisitedPolys()
	{
		for(int p=0; p<m_iImmediateModeNumVisitedPolys; p++)
		{
			Assert(m_pImmediateModeVisitedPolys[p]);
			m_pImmediateModeVisitedPolys[p]->m_iImmediateModeFlags = 0;
		}
		m_iImmediateModeNumVisitedPolys = 0;
	}

	static int CreateInfluenceSpheresForPotentialExplosions(const Vector3 & vPathStart, TInfluenceSphere * influenceSpheres, const int iMaxNumInfluenceSpheres);

	static int CreateInfluenceSpheresForTearGas(const Vector3 & vPathStart, TInfluenceSphere* influenceSpheres, const int iMaxNumInfluenceSpheres);

	static void ValidateInfluenceSpheres(CPathRequest * pPathRequest);

	static Vector3 m_vImmediateModeLosIsectPos;
	static Vector3 m_vImmediateModeWanderOrigin;
	static Vector3 m_vImmediateModeWanderDir;

	static TTestLosStack m_ImmediateModeTestLosStack[SIZE_TEST_LOS_STACK];
	static int m_iImmediateModeTestLosStackNumEntries;

	static TNavMeshPoly * GetClosestNavMeshPolyImmediate(const Vector3 & vPos, const bool bMustHavePedDensityOrPavement, const bool bMustNotBeIsolated, const float fSearchRadius/*=8.0f*/, aiNavDomain domain);

	static EGetClosestPosRet GetClosestPositionForPedNoLock(aiNavDomain domain, const Vector3 & vNearThisPosition, Vector3 & vecOut, const float fMaxSearchRadius, const u32 iFlags, const int iNumAvoidSpheres=0, const spdSphere * pAvoidSpheresArray=NULL);
	static EGetClosestPosRet FloodFillForClosestPosition(const Vector3 & vNearThisPosition, Vector3 & vecOut, const float fMaxSearchRadius, const u32 iFlags, const int iNumAvoidSpheres, const spdSphere * pAvoidSpheresArray, aiNavDomain domain);
	static void FloodFillForClosestPositionNonRecursive(aiNavDomain domain);

#if __TRACK_PEDS_IN_NAVMESH
	static Vector3 UpdateTrackedObjectConstrainedToNavMesh(const Vector3 & vLastPos, const Vector3 & vMoveSpeed, CNavMeshTrackedObject & obj);
#endif

	static void ModifyMovementCosts(CPathFindMovementCosts* pMovementCosts, bool bWander, bool bFlee);

#ifdef GTA_ENGINE

	static void MaybeExtractCoverPointsFromNavMeshes(void);

	static void ExtractCoverPoints_StartNewTimeslice(const Vector3 & vOrigin, float fMaxDistance);
	static void ExtractCoverPoints_ContinueTimeslice(const Vector3 & vOrigin, float fMaxDistance);
	static bool ExtractCoverPointsFromQuadtree(CNavMeshQuadTree * pTree, const Vector3 & vMin, const Vector3 & vMax, const Vector3 & vNavMeshMin, const Vector3 & vNavMeshSize);

	// These members are used to timeslice the algorithm which extracts coverpoints from the navmeshes.
	static bool m_bFindCover_CompletedTimeslice;
	static TNavMeshIndex m_iFindCover_CurrentlyExtractingNavMeshIndex;	// used for calculating coverpoint UIDS
	static CNavMesh * m_pFindCoverCurrentNavMesh;
	static TNavMeshIndex m_iFindCover_NavMeshesIndices[MAX_NUM_NAVMESHES_FOR_COVERAREAS];
	static u32 m_iFindCover_NumNavMeshesIndices;
	static u32 m_iFindCover_NavMeshProgress;
	static u32 m_iFindCover_NavMeshPolyProgress;
	static CNavMeshQuadTree * m_pFindCover_ContinueFromThisQuadTreeLeaf;
	static u32 m_iFindCover_NumIterationsRemainingThisTimeslice;
	static u32 m_iFindCover_NumIterationsPerTimeslice;

	// Nice catchy name!  What this function does is convert the 3-bit value stored in every
	// TLinkedCoverPoint structure, into the index of an adjacent navmesh.
	static inline TNavMeshIndex GetNavMeshIndexFromCoverPointAdjNavMeshCode(aiNavDomain domain, TNavMeshIndex iNavMesh, u32 iAdjCode)
	{
		switch(iAdjCode)
		{
			case 1 : { return iNavMesh; }
			case 2 : { return iNavMesh + m_pNavMeshStores[domain]->GetNumMeshesInX(); }
			case 3 : { return iNavMesh - m_pNavMeshStores[domain]->GetNumMeshesInX(); }
		    case 4 : { return iNavMesh + 1; }
		    case 5 : { return iNavMesh - 1; }
		}
		Assert(0);
		return NAVMESH_NAVMESH_INDEX_NONE;
	}

	// How often to run the cover-points extraction code
	static u32 m_iFrequencyOfCoverPointTimeslices_Millisecs;
	// The last time at which we ran the cover-points extraction code
	static u32 m_iTimeOfLastCoverPointTimeslice_Millisecs;

	static u32 m_iFrequencyOfWaterEdgeExtraction_Millisecs;
	static u32 m_iTimeOfLastWaterEdgeTimeslice_Millisecs;

	// A local buffer is maintained, into which the CNavMeshCoverPoint's are placed.  The CCover class
	// then calls the AddCoverPoints() function, which can traverse this list.  In this way we can
	// avoid stalling the main game loop when coverpoints need to be added from navmeshes.
	static bool m_bCoverPointsBufferInUse;
	static s32 m_iNumCoverPointsInBuffer;
	static TCachedCoverPoint m_CoverPointsBuffer[SIZE_OF_COVERPOINTS_BUFFER];

	static CCoverBoundingArea m_CoverBoundingAreas[CCover::MAX_BOUNDING_AREAS];
	static u32 m_iCoverNumberAreas;

	static bool m_bWaterEdgesBufferInUse;
	static bool m_bExtractWaterEdgesThisTimeslice;
	static s32 m_iNumWaterEdgesInBuffer;
	static const float ms_fMinDistSqrBetweenWaterEdgePoints;
	static const float ms_fFindWaterEdgesMaxDist;
	static TShortMinMax m_FindWaterEdgesMinMax;
	static Vector3 m_vFindWaterEdgesOrigin;
	static Vector3 m_vFindWaterEdgesLastOrigin;
	static Vector3 m_vFindWaterEdgesMin;
	static Vector3 m_vFindWaterEdgesMax;
	static Vector3 m_WaterEdgesBuffer[SIZE_OF_WATEREDGES_BUFFER];
	static u32 m_WaterEdgeAudioProperties[SIZE_OF_WATEREDGES_BUFFER];

	// Add a point to the buffer, testing tolerance
	static bool AddWaterEdgeToBuffer(const Vector3 & vMidEdge, u32 iAudioProperties);

	// Clear out any points which are now outside of the max range
	static void ClearWaterEdges();

#if !__FINAL
	static sysPerformanceTimer	* m_ExtractCoverPointsTimer;
	static float m_fLastTimeTakenToExtractCoverPointsMs;

	static sysPerformanceTimer	* m_TrackObjectsTimer;
	static float m_fTimeToLockDataForTracking;
	static float m_fTimeToTrackAllPeds;
#endif

#endif	// GTA_ENGINE

	//************************************************************
	//	Statics to help with ped-generation recursive calls
	//************************************************************

	static float ms_fPedGenAlgorithmNodeFloodFillDist;

#if !__FINAL
#if SANITY_CHECK_TESSELLATION
	static void SanityCheckPolyConnectionsForAllNavMeshes(void);
#endif
#endif

	static void RequestAndEvictMeshes();

	static int ms_iRequestAndEvictMeshesFreqMs;
	static int m_iTimeOfLastRequestAndEvictMeshes;
	static bool ms_bRunningRequestAndEvictMeshes;

#ifdef GTA_ENGINE
	static void PrepareToUnloadNavMeshDataSetNormal(void * pNavMesh);
	static void PrepareToUnloadNavMeshDataSetHeightMesh(void * pNavMesh);
	static void PrepareToUnloadHierarchicalNavData(void * pNavData);
#endif

	//*****************************************************************
	// Loaders for different types of streamed data/platforms
	//*****************************************************************

	// Origin around which nav-mesh streaming takes place.  Usually the player's position.
	static Vector3 m_vOrigin;
	// The origin during the previous frame
	static Vector3 m_vLastOrigin;

	//********************************************************************************
	//	Ped Generation
	//	The following variables are used for finding coordinates to create new peds,
	//	when done as an asynchronous 'housekeeping' task once every few seconds.

#ifdef GTA_ENGINE

	static u32 m_iLastTimeProcessedPedGeneration;
	static dev_u32 m_iProcessPedGenerationFreq;

	static CPathServerAmbientPedGen m_AmbientPedGeneration;
	static CPathServerOpponentPedGen m_OpponentPedGeneration;

#if !__FINAL
public:
	static bool ms_bDrawPedGenBlockingAreas;
	static bool ms_bVisualiseOpponentPedGeneration;
	static void DebugDrawPedGenBlockingAreas();
	static void ClearAllPathHistory();
	static void ClearAllOpenClosedFlags();
	static void DebugDetessellateNow();
#endif



#endif	// GTA_ENGINE


	// PURPOSE:	Pointer to an interface object for game-specific extensions.
	// NOTE:	This is set by Init(), and is not considered owned by CPathServer.
	static const fwPathServerGameInterface* m_GameInterface;

	// This should never reach zero, since area 0 is always reserved for the player origin
	//static s32 m_iNavMeshNumRequiredRegions;
	// The origins & radii of the areas which needed navmeshes (the player origin is always area zero)
	static TNavMeshRequiredRegion m_NavMeshRequiredRegions[NAVMESH_MAX_REQUIRED_REGIONS];

	// Whether the CPathServer system is running single or multi-threaded.
	static EProcessingMode m_eProcessingMode;

	// The distance at which navmeshes are loaded
	static float m_fNavMeshLoadProximity;
	// The distance at which the hierarchical nodes are loaded
	static float m_fHierarchicalNodesLoadProximity;

	// Navmeshes to cover the entire game map
//	static aiNavMeshStore m_StaticNavMeshStore;

#if __HIERARCHICAL_NODES_ENABLED
	static aiNavNodesStore * m_pNavNodesStore;
#endif
	static int m_iNumDynamicNavMeshesInExistence;

	// How frequently the CPathServerThread should attempt to untessellate navmesh polygons
	static u32 m_iThreadHousekeepingFrequencyInMillisecs;
	static u32 m_iLastTimeThreadDidHousekeeping;

	static u32 m_iLastTimeCheckedForStaleRequests;
	static u32 m_iCheckForStaleRequestsFreq;

	// Whether to explicitly sleep the pathfinding thread, maybe not necessary for multi-core setups
	static bool m_bSleepPathServerThreadOnLongPathRequests;
	// Number of iterations within FindPath() to perform before sleeping (if this feature is enabled)
	static u32 m_iNumFindPathIterationsBetweenSleepChecks;

	// How long to sleep for between checking for new pending path requests
	static bank_u32 m_iTimeToSleepWaitingForRequestsMs;
	static bank_u32 m_iTimeToSleepDuringLongRequestsMs;


#if !__FINAL
	// A debug variable, so we know how many tessellations have occurred for each path request
	static u32 ms_iNumTessellationsThisPath;
	static u32 ms_iNumTestDynamicObjectLOS;
	static u32 ms_iNumTestNavMeshLOS;
	static u32 ms_iNumImmediateTestNavMeshLOS;
	static u32 ms_iNumGetObjectsIntersectingRegion;
	static u32 ms_iNumCacheHitsOnDynamicObjects;

	static float m_fRunningTotalOfTimeOnPathRequests;
	static float m_fTimeSpentProcessingThisGameTurn;
#endif

	static bool ms_bLoadAllHierarchicalData;

	static CPathRequest m_PathRequests[MAX_NUM_PATH_REQUESTS];
	static CGridRequest m_GridRequests[MAX_NUM_GRID_REQUESTS];
	static CLineOfSightRequest m_LineOfSightRequests[MAX_NUM_LOS_REQUESTS];
	static CAudioRequest m_AudioRequests[MAX_NUM_AUDIO_REQUESTS];
	static CFloodFillRequest m_FloodFillRequests[MAX_NUM_FLOODFILL_REQUESTS];
	static CClearAreaRequest m_ClearAreaRequests[MAX_NUM_CLEARAREA_REQUESTS];
	static CClosestPositionRequest m_ClosestPositionRequests[MAX_NUM_CLOSESTPOSITION_REQUESTS];

	static u32 m_iNextHandle;
	static u32 m_iNextPathIndexToStartFrom;
	static u32 m_iNextGridIndexToStartFrom;
	static u32 m_iNextLosIndexToStartFrom;
	static u32 m_iNextAudioIndexToStartFrom;
	static u32 m_iNextFloodFillIndexToStartFrom;
	static u32 m_iNextClearAreaIndexToStartFrom;
	static u32 m_iNextClosestPositionIndexToStartFrom;

	static u32 m_iNumDynamicObjects;

	static void SwitchNavMeshInArea(const Vector3 & vMin, const Vector3 & vMax, ESWITCH_NAVMESH_POLYS eSwitch, scrThreadId iScriptUID);
	static void ApplyPedAreaSwitchesToNewlyLoadedNavNodes(CHierarchicalNavData * pNavNodes);

public:
	static s32 GetNumPathRegionSwitches() { return m_iNumPathRegionSwitches; }
	static const TPathRegionSwitch & GetPathRegionSwitch(s32 i) { return m_PathRegionSwitches[i]; }

protected:
	static s32 m_iNumPathRegionSwitches;
	static TPathRegionSwitch m_PathRegionSwitches[MAX_NUM_PATH_REGION_SWITCHES];

public:
#if !__FINAL
	static bool ms_bDrawPedSwitchAreas;
	static void DebugDrawPedSwitchAreas();
#endif

	static void ProcessAddDeferredDynamicObjects();
	static s32 EvictDynamicObjectForScript();

	// Dynamic objects added from script are added to a list, and added to the pathfinder at a safe point in the frame
	// This is to workaround issues of locking objects/navmeshes in order to evict less important objects.
	static s32 m_iNumDeferredAddDynamicObjects;
	static s32 m_iNextScriptObjectHandle;
	static s32 m_iNumScriptBlockingObjects;
	static TScriptDeferredAddDynamicObject m_ScriptDeferredAddDynamicObjects[TScriptDeferredAddDynamicObject::ms_iMaxNum];
	static TScriptObjHandlePair m_ScriptedDynamicObjects[MAX_NUM_SCRIPTED_DYNAMIC_OBJECTS];

	// This member mainatins the worker thread which does the path processing
	static CPathServerThread m_PathServerThread;

	static bool ms_bUseEventsForRequests;
	static bool ms_bProcessAllPendingPathsAtOnce;
	static u32 m_iNumTimesGaveTime;
	static bool ms_bCurrentlyRemovingAndAddingObjects;

	// The total amount of memory used by the precalc'd paths (aka edge groups), use for hierarchical pathfinding
	static s32 m_iTotalMemoryUsedByPrecalcPaths;

	// How many times the game (or the main worker thread) had to stall waiting for access to shared resources
	// (ie. the navmeshes or the dynamic objects).
	// NB : Don't know if we can get this info now we're using semaphores instead of spinlocks..
	
	static bool m_bDisplayTimeTakenToAddDynamicObjects;

	// Audio parameters
	static f32 m_fAudioVelocityDotProduct;
	static f32 m_fAudioVelocityCubeSize;
	static f32 m_fAudioMinPolySizeForRadiusSearch;

#ifdef GTA_ENGINE
#if __BANK
	static bool m_bCheckForThreadStalls;
	static u32 m_iNumThreadStallsForNavMeshes;
	static u32 m_iNumThreadStallsForDynamicObjects;
	static u32 m_iNumTimesCouldntUpdateObjectsDueToGameThreadAccess;
	static float m_fMainGameTimeSpendOnThreadStalls;

	static float m_fPedGenTimeToObtainCriticalSection;
	static float m_fPedGenTimeToDoAlgorithm;
	static float m_fPedGenTimeToReleaseCriticalSection;

	static s32 m_iNumImmediateModeLosCallsThisFrame;
	static s32 m_iNumImmediateModeWanderPathsThisFrame;
	static s32 m_iNumImmediateModeFleePathsThisFrame;
	static s32 m_iNumImmediateModeSeekPathsThisFrame;
	static float m_fTimeSpentOnImmediateModeCallsThisFrame;

	static inline void CheckForThreadStall(sysCriticalSectionToken & cst)
	{
		if(cst.TryLock())
		{
			cst.Unlock();
		}
		else
		{
			// Must remember to update this section for all the critical section we may be using
			if(&cst==&m_NavMeshDataCriticalSectionToken) m_iNumThreadStallsForNavMeshes++;
			else if(&cst==&m_NavMeshImmediateAccessCriticalSectionToken) m_iNumThreadStallsForNavMeshes++;
			else if(&cst==&m_DynamicObjectsCriticalSectionToken) m_iNumThreadStallsForDynamicObjects++;
		}
	}
#endif
#endif

};

#ifdef GTA_ENGINE
#define GTA_ENGINE_ONLY(x) x
#else
#define GTA_ENGINE_ONLY(x)
#endif

//extern CPathServer g_PathServer;
#if __BANK
#ifndef __TOOL
extern void PathServer_AddWidgets(bkBank & bank);
#endif //__TOOL
#endif //__BANK


struct TFloodFillForClosest
{
	CNavMesh * pNavMesh;	// the current navmesh
	TNavMeshPoly * pPoly;	// the current poly
};

class CFire;
struct TPotentialExplosion
{
	CFire * pFire;
	float fDistSqr;
};

struct TGasCloud
{
	Vec3V worldPos;
	float fDistSqr;
};

// How many potential positions to choose for catmull-rom control points
#define PATHSPLINE_NUM_TEST_DISTANCES		4
// The distances from adjacent waypoints towards the corner-point, for vCtrlPt2 and vCtrlPt3
extern const float g_fPathSplineDists[PATHSPLINE_NUM_TEST_DISTANCES];
// The magnitude that the outer control pts are pulled back (scales the length of the line-seg)
extern const float g_fOuterCtrlPtDists[PATHSPLINE_NUM_TEST_DISTANCES];
// The number of subdivisions used when testing the spline against the navmesh (crude approximation of final realtime curve)
#define PATHSPLINE_NUM_TEST_SUBDIVISIONS	4

inline void CalcPathSplineCtrlPts(
	const int iSplineEnum, const float fCtrlPt2TValOverride,
	const Vector3 & vLast, const Vector3 & vNext,
	const Vector3 & vLastToCurrent, const Vector3 & vCurrentToNext,
	Vector3 & vCtrlPt1, Vector3 & vCtrlPt2, Vector3 & vCtrlPt3, Vector3 & vCtrlPt4)
{
	Assert(iSplineEnum >= 0 && iSplineEnum<PATHSPLINE_NUM_TEST_DISTANCES);

	if(fCtrlPt2TValOverride != 0.0f)
		vCtrlPt2 = vLast + (vLastToCurrent * fCtrlPt2TValOverride);
	else
		vCtrlPt2 = vLast + (vLastToCurrent * g_fPathSplineDists[iSplineEnum]);

	vCtrlPt1 = vCtrlPt2 - (vLastToCurrent * g_fOuterCtrlPtDists[iSplineEnum]);
	vCtrlPt3 = vNext - (vCurrentToNext * g_fPathSplineDists[iSplineEnum]);
	vCtrlPt4 = vCtrlPt3 + (vCurrentToNext * g_fOuterCtrlPtDists[iSplineEnum]);
}

inline void CalcPathSplineCtrlPtsV(
	const int iSplineEnum, ScalarV_In vCtrlPt2TValOverride,
	Vec3V_In vLast, Vec3V_In vNext,
	Vec3V_In vLastToCurrent, Vec3V_In vCurrentToNext,
	Vec3V_InOut vCtrlPt1, Vec3V_InOut vCtrlPt2, Vec3V_InOut vCtrlPt3, Vec3V_InOut vCtrlPt4)
{
	Assert(iSplineEnum >= 0 && iSplineEnum<PATHSPLINE_NUM_TEST_DISTANCES);

	const ScalarV pathSplineDistV = LoadScalar32IntoScalarV(g_fPathSplineDists[iSplineEnum]);
	const ScalarV outerCtrlPtDistsV = LoadScalar32IntoScalarV(g_fOuterCtrlPtDists[iSplineEnum]);

	const ScalarV tV = SelectFT(IsEqual(vCtrlPt2TValOverride, ScalarV(V_ZERO)), vCtrlPt2TValOverride, pathSplineDistV);

	const Vec3V pt2V = AddScaled(vLast, vLastToCurrent, tV);
	const Vec3V pt1V = SubtractScaled(pt2V, vLastToCurrent, outerCtrlPtDistsV);
	const Vec3V pt3V = SubtractScaled(vNext, vCurrentToNext, pathSplineDistV);;
	const Vec3V pt4V = AddScaled(pt3V, vCurrentToNext, outerCtrlPtDistsV);;

	vCtrlPt1 = pt1V;
	vCtrlPt2 = pt2V;
	vCtrlPt3 = pt3V;
	vCtrlPt4 = pt4V;
}

inline Vector3 PathSpline(const Vector3 & vCtrlPt1, const Vector3 & vCtrlPt2, const Vector3 & vCtrlPt3, const Vector3 & vCtrlPt4, const float u)
{
	const Vector3 vTargetAfterSlide =
		vCtrlPt1 * (-0.5f*u*u*u + u*u - 0.5f*u) +
		vCtrlPt2 * (1.5f*u*u*u + -2.5f*u*u + 1.0f) +
		vCtrlPt3 * (-1.5f*u*u*u + 2.0f*u*u + 0.5f*u) +
		vCtrlPt4 * (0.5f*u*u*u - 0.5f*u*u);

	return vTargetAfterSlide;
}

//-----------------------------------------------------------------------------

class CBaseModelInfo;
class CFire;
class CCarDoor;

/*
PURPOSE
	CPathServerGta now holds some functions that depend on code that we may not
	want to move to the RAGE framework, such as CEntity and CFire.
TODO
	To continue moving code to RAGE, the CPathServer class (already mostly free
	from GTA dependencies) should probably be merged with fwPathServerBase, and
	renamed fwPathServer. Then, CPathServerGta here can be renamed back to
	CPathServer.
*/
class CPathServerGta : public CPathServer
{
	friend class CNavGenApp;

public:
	//*****************************************************************************************
	// CPopulation will call these functions when objects are converted to/from dummies.
	// Not all objects & entities are added, only those which pose significant obstructions.

	static bool MaybeAddDynamicObject(CEntity * pEntity);
	static void MaybeRemoveDynamicObject(CEntity * pEntity);

	static bool AddFireObject(CFire * pFire);
	static bool RemoveFireObject(const CFire * pFire);
	static bool RemoveVehicleDoorObject(const CCarDoor * pDoor);

	static s32 AddScriptedBlockingObject(const scrThreadId iThreadId, const Vector3 & vPosition, const Vector3 & vSize, const float fHeading, const s32 iBlockingFlags=TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
	static bool UpdateScriptedBlockingObject(const scrThreadId iThreadId, const s32 iObjHandle, const Vector3 & vPosition, const Vector3 & vSize, const float fHeading, const s32 iBlockingFlags=TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
	static bool RemoveScriptedBlockingObject(const scrThreadId iThreadId, s32 iObjIndex);
	static void RemoveAllBlockingObjectsForScript(const scrThreadId iThreadId);
	static bool DoesScriptedBlockingObjectExist(const scrThreadId iThreadId, const s32 iObjHandle);

	// Constructor/destructor for some entities will call these two functions (eg. boats)
	static bool MaybeRequestDynamicObjectNavMesh(CEntity * pEntity);
	static bool MaybeRemoveDynamicObjectNavMesh(CEntity * pEntity);

	static bool SetUpDynamicNavMeshInstance(CDynamicNavMeshEntry & newEntry, TNavMeshIndex iIndexOfNewNavmesh, CNavMesh * pBackedUpCopy, CVehicle * pVehicle);

	// Called every frame for moving objects.  We have to check whether the object has moved enough to need recalculating its bounds, etc
	static void UpdateDynamicObject(CEntity * pEntity, bool bForceUpdate=false, bool bForceActivate=false);
	static void UpdateFireObject(CFire * pFire);
	static void UpdateVehicleDoorsDynamicObjects(CVehicle * pVehicle, const bool bForceUpdate=false);
	static void UpdateVehicleDoorDynamicObject(CVehicle * pVehicle, CCarDoor * pDoor, const bool bForceUpdate=false);

	static void AddVehicleDoorDynamicObject(CVehicle * pVehicle, CCarDoor * pDoor);

	// Finds a safe position for a ped on a dynamic navmesh associated with the given entity.
	// Input & output positions are in worldspace, although internal calculations are done locally to the entity.
	static EGetClosestPosRet GetClosestPositionForPedOnDynamicNavmesh(CEntity * pEntity, const Vector3 & vNearThisPosition, const float fMaxSearchRadius, Vector3 & vOut);

	static bool ShouldAvoidObject(const CEntity* pEntity);
	static bool ShouldPedAvoidObject(const class CPed* pPed, const CEntity* pEntity);
	static bool IsObjectSignificant(const CEntity* pEntity);
	static bool SpecialCaseShouldAvoidFragmentObject(CObject * pObj);	// HACK_GTAV
	static bool SpecialCaseDontRemoveSmashedGlass(CObject * pObj);		// HACK_GTAV

protected:
	
	static bool AddDynamicObject(CEntity * pEntity, CBaseModelInfo * pModelInfo);

	static void AddVehicleDoorsDynamicObjects(CVehicle * pVehicle);
};



#ifdef GTA_ENGINE

// TODO: Move this elsewhere. /FF
/*
PURPOSE
	Interface to extend CPathServer with some features of the GTA engine.
*/
class CPathServerGameInterfaceGta : public fwPathServerGameInterface
{
public:
	virtual void OverridePathRequestParameters(const TRequestPathStruct& reqStruct, CPathRequest& pathRequest) const;

protected:
	// Helper function, used to be CPathFindMovementCosts::SetFromPed().
	static void SetPathFindMovementCostsFromPed(CPathFindMovementCosts& costsOut, const CPed * pPed);
};


#endif	// GTA_ENGINE

//-----------------------------------------------------------------------------

/*
PURPOSE
	Loose functions that used to be member functions of TDynObjBounds. They got moved out
	to facilitate TDynObjBounds moving down to the RAGE level, without having to bring
	CEntity/CFire/CVehicle dependencies with it.
*/
namespace TDynObjBoundsFuncs
{
	void CalcBoundsForEntity(TDynObjBounds &bnds, CEntity * pEntity, TDynamicObject * pDynObj);
	void CalcBoundsForFire(TDynObjBounds &bnds, CFire * pFire, TDynamicObject * pDynObj);
	bool CalcBoundsForVehicleDoor(TDynObjBounds& bnds, CCarDoor * pDoor, CVehicle * pParent, const Matrix34 & doorMat, TDynamicObject * pDynObj);
	void CalcBoundsForBlockingObject(TDynObjBounds& bnds, const Vector3 & vPos, const Vector3 & vSize, const float fRotation, TDynamicObject * pDynObj);

#ifdef GTA_ENGINE
	int GetDoorComponents(const CVehicle * pVehicle, int * iComponents, const s32 iMaxNum);
	int GetHeliIgnoreComponents(const CVehicle * pVehicle, int * iComponents, const s32 iMaxNum);
	s32 GetIgnoreComponentsForEntity(const CEntity * pEntity, s32 * pOut_ComponentsArray, const s32 iMaxNum);
#endif
}


#endif	// PATHSERVER_H





