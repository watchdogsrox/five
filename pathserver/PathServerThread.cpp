
//Rage headers
#include "atl/binheap.h"
#include "profile/telemetry.h"
#include "profile/timebars.h"
#include "profile/cputrace.h"
#include "System/ipc.h"
#include "System/Xtl.h"
#include "Vector/Colors.h"
#include "Vector/Geometry.h"
#include "system/memory.h"

// Framework headers
#include "ai/navmesh/priqueue.h"
#include "fwmaths/Vector.h"

#include "PathServer/PathServer.h"
#include "PathServer/PathServerThread.h"

#include "ai/navmesh/pathserverthread.h"

//Game headers
#ifdef GTA_ENGINE
#include "game/config.h"				// Used for some thread priority stuff now, will need to be dealt with when moving more code to RAGE.
#include "system/threadpriorities.h"
#include "task/Movement/TaskNavMesh.h"	// Used for tracking cases where an AI task has failed to retrieve a requested path
#include "task/Movement/TaskMoveWander.h"
#include "peds/pedpopulation.h"
#include "system/InfoState.h"
#endif

#include "security/ragesecgameinterface.h"
#include "Network/Sessions/NetworkSession.h"

#if __XENON && !__FINAL
#define __TRACE_PATH_REQUESTS_ON_XENON		1
#else
#define __TRACE_PATH_REQUESTS_ON_XENON		0
#endif

#define TRACK_LEAKED_PATHS		0	//NAVMESH_OPTIMISATIONS_OFF

#if __TRACE_PATH_REQUESTS_ON_XENON
#include "xtl.h"
#include "tracerecording.h"
#endif	// __TRACE_PATH_REQUESTS_ON_XENON

NAVMESH_OPTIMISATIONS()

float CPathServerThread::ms_fMaxMassOfPushableObject = 500.0f;
float CPathServerThread::ms_fMinMassOfSmallSignificant = 125.f;
float CPathServerThread::ms_fMinVolumeOfSignificant = 0.33f;
float CPathServerThread::ms_fBigVolumeOfSignificant = 1.5f;
float CPathServerThread::ms_fHeightAboveNavMeshForDynObjLOS = 0.4f;	//0.1f;	// was 0.5f;
float CPathServerThread::ms_fHeightAboveFirstTestFor2ndDynObjLOS = 0.4f;	// This is the height above ms_fHeightAboveNavMeshForDynObjLOS value (and not above the navmesh itself)
bank_float CPathServerThread::ms_fSmallPolySizeWhenTessellating = 2.0f;
const float CPathServerThread::ms_fNormalDistBelowToLookForPoly = 4.0f;
const float CPathServerThread::ms_fNormalDistAboveToLookForPoly = 0.0f;
bank_float CPathServerThread::ms_fPreferDownhillScaleFactor = 50.0f;
bank_float CPathServerThread::ms_fWanderTurnWeight = 200.0f;
bank_float CPathServerThread::ms_fWanderTurnPlaneWeight = 15.0f;
bank_float CPathServerThread::ms_fPullOutFromEdgesDistance = 0.25f;
bank_float CPathServerThread::ms_fPullOutFromEdgesDistanceExtra = 0.4f;

Vector3 CPathServerThread::ms_vDynamicObjectIntersectionPos(0.0f,0.0f,0.0f);
float CPathServerThread::ms_fDynObjPlaneEpsilon[NUM_DYNAMIC_OBJECT_PLANES] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
float CPathServerThread::ms_fDynObjPlaneEpsilonForceReduced[NUM_DYNAMIC_OBJECT_PLANES] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
float CPathServerThread::ms_fDynObjPlaneEpsilonNotReduced[NUM_DYNAMIC_OBJECT_PLANES] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

// The list of all objects intersecting the "bothPolysMinMax" min/max region
atArray<TDynamicObject*> CPathServerThread::ms_dynamicObjectsIntersectingPolygons(0,256);

float CPathServerThread::ms_fNonPavementPenalty_ShortestPath = 50.0f;
float CPathServerThread::ms_fNonPavementPenalty_Flee = 55.0f;
float CPathServerThread::ms_fNonPavementPenalty_FleeSofter = 75.0f;
float CPathServerThread::ms_fNonPavementPenalty_Wander = 1000.0f;	//250.0f;   //500.0f;	// 200.0f	// 16000.0f

float CPathServerThread::ms_fClimbMultiplier_ShortestPath = 0.65f;
float CPathServerThread::ms_fClimbMultiplier_Flee = 3.5f;
float CPathServerThread::ms_fClimbMultiplier_Wander = 1.f;

float CPathServerThread::ms_fWaterMultiplier_Flee = 10.f;

#if __DEV
s32 CPathServerThread::m_eVisitedPolyMarkingMode = CPathServerThread::EUseAStarTimeStamp;
#endif

#if !__FINAL
bool CPathServerThread::ms_bMarkVisitedPolys = false;
bank_bool CPathServerThread::ms_bUsePrefetching = false;
#endif

s32 CPathServerThread::ms_iNumVisitedPolysInThisBatchOfPathRequests = 0;
s32 CPathServerThread::ms_iNumTessellationsInThisBatchOfPathRequests = 0;
s32 CPathServerThread::ms_iMaxNumVisitedPolysInEachBatchOfPathRequests = 256;
s32 CPathServerThread::ms_iMaxNumTessellationsInEachBatchOfPathRequests = 128;

TPathFindVars CPathServerThread::m_Vars;
TVisitPolyStruct CPathServerThread::m_VisitPolyVars;
CTestNavMeshLosVars CPathServerThread::m_LosVars;
CBlockedLadders CPathServerThread::m_BlockedLadders;

bool CPathServerThread::m_bHandleTimestampOverflow = false;
bool CPathServerThread::m_bHandleAStarTimestampOverflow = false;

bank_u32 CPathServerThread::ms_iPathRequestNumMillisecsBeforeSleepingThread = 8;
sysPerformanceTimer	* CPathServerThread::m_PathRequestSleepTimer = NULL;
bool CPathServerThread::m_bSleepingDuringPathSearch = false;

#if !__FINAL && !__PPU && SANITY_CHECK_TESSELLATION

void
CPathServerThread::SanityCheckPolyConnections(CNavMesh * pNavMesh, TNavMeshPoly * pPoly)
{
	Assert(pNavMesh->m_iIndexOfMesh == pPoly->GetNavMeshIndex());
	u32 iPolyIndex = pNavMesh->GetPolyIndex(pPoly);

	u32 v,v2;

	for(v=0; v<pPoly->GetNumVertices(); v++)
	{
		TAdjPoly adjPoly;
		pNavMesh->GetAdjacentPoly(pPoly, v, adjPoly);

		if(adjPoly.GetNavMeshIndex() == NAVMESH_NAVMESH_INDEX_NONE)
			continue;

		// non-standard adjacency types don't necessarily link back to this poly
		if(adjPoly.GetAdjacencyType() != ADJACENCY_TYPE_NORMAL)
			continue;

		CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(adjPoly.GetNavMeshIndex());
		if(pAdjNavMesh)
		{
			TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetPolyIndex());
			Assert(pAdjPoly->GetNavMeshIndex() == pAdjNavMesh->m_iIndexOfMesh);

			for(v2=0; v2<pAdjPoly->GetNumVertices(); v2++)
			{
				TAdjPoly adjAdjPoly;
				pAdjNavMesh->GetAdjacentPoly(pAdjPoly, v2, adjAdjPoly);

				if(adjAdjPoly.GetNavMeshIndex(pAdjNavMesh->GetAdjacentMeshes()) == pNavMesh->m_iIndexOfMesh && adjAdjPoly.GetPolyIndex() == iPolyIndex)
					break;
			}

			if(v2 == pAdjPoly->GetNumVertices())
			{
				Assert(0 && "adjacent poly doesn't link back");

				Displayf("ORIGINAL POLY\n");
				CPathServer::DebugPolyText(pNavMesh, pNavMesh->GetPolyIndex(pPoly));

				Displayf("ADJACENT POLY\n");
				CPathServer::DebugPolyText(pAdjNavMesh, pAdjNavMesh->GetPolyIndex(pAdjPoly));
			}

		}
	}
}

#endif

#if __XENON

// See the XDK "SetThreadName" for info on this.
typedef struct tagTHREADNAME_INFO {
	DWORD dwType;     // Must be 0x1000
	LPCSTR szName;    // Pointer to name (in user address space)
	DWORD dwThreadID; // Thread ID (-1 for caller thread)
	DWORD dwFlags;    // Reserved for future use; must be zero
} THREADNAME_INFO;

void SetThreadName( DWORD dwThreadID, LPCSTR szThreadName )
{
	THREADNAME_INFO info;

	info.dwType = 0x1000;
	info.szName = szThreadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD), (DWORD *)&info );
	}
	__except( EXCEPTION_CONTINUE_EXECUTION )
	{
	}
}
#endif

//-----------------------------------------------------------------------------------------------

#ifdef GTA_ENGINE

dev_s32 CBlockedLadders::ms_iDisableInterval = 5000;
dev_s32 CBlockedLadders::ms_iMaxNumClimbers = 3;

void CBlockedLadders::Reset()
{
	for(s32 i=0; i<ms_iMaxNumLadders; i++)
	{
		m_Ladders[i].m_iNumClimbers = 0;
		m_Ladders[i].m_iDisableTimer = 0;
		m_Ladders[i].m_iLastPathfindTime = 0;
		m_Ladders[i].m_vBasePosition.Zero();
	}
}

bool CBlockedLadders::IsLadderBlocked(const Vector3 & vBasePosition) const
{
	for(s32 i=0; i<ms_iMaxNumLadders; i++)
	{
		const s32 iDelta = fwTimer::GetTimeInMilliseconds() - m_Ladders[i].m_iDisableTimer;
		if(iDelta < ms_iDisableInterval && vBasePosition.IsClose(m_Ladders[i].m_vBasePosition, 0.1f))
		{
			return true;
		}
	}
	return false;
}

void CBlockedLadders::UpdateUsage(const Vector3 & vBasePosition)
{
	s32 iFirstFree = -1;
	for(s32 i=0; i<ms_iMaxNumLadders; i++)
	{
		if(vBasePosition.IsClose(m_Ladders[i].m_vBasePosition, 0.1f))
		{
			const s32 iDelta = fwTimer::GetTimeInMilliseconds() - m_Ladders[i].m_iLastPathfindTime;
			if(iDelta >= ms_iDisableInterval)
			{
				m_Ladders[i].m_iNumClimbers = 1;
			}
			else
			{
				m_Ladders[i].m_iNumClimbers++;
			}

			if(m_Ladders[i].m_iNumClimbers >= ms_iMaxNumClimbers)
			{
				m_Ladders[i].m_iDisableTimer = fwTimer::GetTimeInMilliseconds();
			}

			m_Ladders[i].m_iLastPathfindTime = fwTimer::GetTimeInMilliseconds();

			return;
		}
		if(iFirstFree < 0 && fwTimer::GetTimeInMilliseconds() - m_Ladders[i].m_iDisableTimer >= ms_iDisableInterval)
		{
			iFirstFree = i;
		}
	}
	if(iFirstFree >= 0)
	{
		m_Ladders[iFirstFree].m_vBasePosition = vBasePosition;
		m_Ladders[iFirstFree].m_iDisableTimer = fwTimer::GetTimeInMilliseconds();
		m_Ladders[iFirstFree].m_iLastPathfindTime = fwTimer::GetTimeInMilliseconds();
		m_Ladders[iFirstFree].m_iNumClimbers = 1;
	}
}

#endif

//-----------------------------------------------------------------------------------------------

//*******************************************************
//	CPathServerThread
//	This class encapsulates the path-finding thread, and
//	provides the methods which it uses to find paths.
//*******************************************************

const u32 CPathServerThread::ms_iNumFindPathIterationsBeforeGivingTime			= 256;	// was 1024
const u32 CPathServerThread::ms_iNumRefinePathIterationsBeforeGivingTime			= 256;	// was 1024
const u32 CPathServerThread::ms_iNumMinimisePathLengthIterationsBeforeGivingTime = 256;	// was 1024


CPathServerThread::CPathServerThread()
{
	sysMemUseMemoryBucket b(MEMBUCKET_GAMEPLAY);

	m_iTotalMemoryUsed = sizeof(CPathServerThread);

#ifndef GTA_ENGINE
	m_hThreadHandle = 0;
#endif

	m_iThreadID = 0;

	m_DynamicObjectsStore = rage_new TDynamicObject[PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS];
	CDynamicObjectsContainer::Init();

	//m_PolyObjectsCache = new fwPolyObjectsCache(this);

	Reset();

	Printf("sizeof(CPathServer) : %" SIZETFMT "ik\n", sizeof(CPathServer) / 1024);
	Printf("sizeof(CPathServerThread) : %" SIZETFMT "ik\n", sizeof(CPathServerThread) / 1024);
	Printf("CPathServerThread allocated %" SIZETFMT "ik for m_DynamicObjectsStore[%i]\n", (sizeof(TDynamicObject) * PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)/1024, PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);

#if !__FINAL
	m_PerfTimer = rage_new sysPerformanceTimer("PathServerThread");
	m_RequestProcessingOverallTimer = rage_new sysPerformanceTimer("Request Overall Timer");
	m_PolyTessellationTimer = rage_new sysPerformanceTimer("Poly Tessellation Timer");
#endif

	Reset();
}

CPathServerThread::~CPathServerThread(void)
{
	Close();
}

bool
CPathServerThread::Init(u32 iProcessorIndex)
{
#ifndef GTA_ENGINE
	if(m_hThreadHandle)
	{
		return false;
	}
#endif
	if(m_iThreadID)
	{
		return false;
	}

	m_PathServerThreadParams.m_pPathServerThread = this;

	m_PathRequestSleepTimer = rage_new sysPerformanceTimer("PathServerThreadSleepTimer");

#if RSG_DURANGO
	iProcessorIndex = 5;
#endif

	m_iProcessorIndex = iProcessorIndex;

	//******************************************************************************************************

#ifdef GTA_ENGINE

	// Create the event which is used to signal that a path has been requested
	fwPathServer::m_PathRequestSema = sysIpcCreateSema(false);

	sysIpcPriority iPathfindThreadPriority = static_cast<sysIpcPriority>(PRIO_PATHSERVER_THREAD);

	//*************************************************************************************************
	// Create the worker thread for the pathserver.  We'll create this with the base (normal) priority
	// since the RAGE thread-priorities system isn't very usable - for example it supports -8 to +8
	// priority values, but these don't map properly across platforms.

	m_iThreadID = sysIpcCreateThread(
		PathServerThreadFunc,
		&m_PathServerThreadParams,
		PATH_SERVER_THREAD_STACKSIZE,
		iPathfindThreadPriority,
		"PathServerThread",
		m_iProcessorIndex,
		"PathServer"
	);

#else	/////////////////////////////////////////////// NON GTA_ENGINE //////////////////////////////////////////

	// Initialize the critical section objects for synchronizing access to dynamic objects & navmesh data
	InitializeCriticalSection(&m_DynamicObjectsCriticalSection);
	InitializeCriticalSection(&m_NavMeshDataCriticalSection);
	InitializeCriticalSection(&m_NavMeshImmediateAccessCriticalSection);	// this should be done in the main thread?


	m_hThreadHandle = CreateThread(
		NULL,
		PATH_SERVER_THREAD_STACKSIZE,
		PathServerThreadFunc,
		&m_PathServerThreadParams,
		CREATE_SUSPENDED,
		&m_iThreadID
		);
	if(!m_hThreadHandle)
	{
		return false;
	}

#endif	// !GTA_ENGINE

	Printf("sizeof(CPathServerBinHeap::Node) = %" SIZETFMT "i\n", sizeof(CPathServerBinHeap::Node));
	m_PathSearchPriorityQueue = rage_new CPathSearchPriorityQueue(MAX_PATH_STACK_SIZE);
	m_iTotalMemoryUsed += PATH_SERVER_THREAD_STACKSIZE;
	m_iTotalMemoryUsed += m_PathSearchPriorityQueue->GetMemoryUsed();

	if(!m_iThreadID)
	{
		return false;
	}

#if __WIN32
	iProcessorIndex;	// Shut the compiler up
#endif

	// On the Xenon, we may assign this thread to a specified processor
#if __XENON

	// Index out of range ?  Then assign to CPU zero
	if(iProcessorIndex >= MAXIMUM_PROCESSORS)
	{
		iProcessorIndex = 0;
	}

#endif	// __XENON

	// On the PS3 this will probably be more complicated

#ifdef GTA_ENGINE
#else
	// Start it running
	ResumeThread(m_hThreadHandle);
#endif

	return true;
}

void
CPathServerThread::Reset()
{
	m_pCurrentActiveRequest = NULL;
	m_pCurrentActivePathRequest = NULL;
	m_pCurrentActiveGridRequest = NULL;
	m_pCurrentActiveLosRequest = NULL;
	m_pCurrentActiveAudioRequest = NULL;
	m_pCurrentActiveFloodFillRequest = NULL;
	m_pCurrentActiveClearAreaRequest = NULL;
	m_pCurrentActiveClosestPositionRequest = NULL;

	m_bQuitNow = false;
	m_bHasQuit = false;

	memset(&m_Vars, 0, sizeof(m_Vars));
	m_Vars.Init();
	m_Vars.m_iNumVisitedPolys = 0;
	m_iLastNumVisitedPolys = 0;
	m_iLastRequestType = ENoRequest;
	m_iNavMeshPolyTimeStamp = 0;

	m_iNumDeferredClimbDeactivations = 0;

	m_pFirstDynamicObject = NULL;
	memset(m_DynamicObjectsStore, 0, sizeof(TDynamicObject) * PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);

	CDynamicObjectsContainer::Clear();
#ifdef GTA_ENGINE
	m_BlockedLadders.Reset();
#endif
	m_LosVars.m_iTestLosStackNumEntries = 0;
}

void
CPathServerThread::Close(void)
{
	// We should do something here to test that the
	// thread is still alive.

	// If the thread is still active, ask it to stop
	// Keep this thread spinning until "m_bHasQuit" is set
	// NB : We should have some maximum timeout (2secs?) before
	// which we kill the thread & quit anyway
#ifdef GTA_ENGINE
	if(m_iThreadID)
	{
		if(!m_bHasQuit)
		{
			SignalQuit();

			if(CPathServer::ms_bUseEventsForRequests)
			{
				// Be sure to signal the path-request event, so that the thread wakes up long enough to realise it must quit!
				sysIpcSignalSema(CPathServer::m_PathRequestSema);
			}

			sysIpcWaitThreadExit(m_iThreadID);
		}
		m_iThreadID = 0;
	}

	if(fwPathServer::m_PathRequestSema)
	{
		sysIpcDeleteSema(fwPathServer::m_PathRequestSema);
		fwPathServer::m_PathRequestSema = 0;
	}

#else
	if(m_hThreadHandle)
	{
		if(!m_bHasQuit)
		{
			SignalQuit();

			while(!m_bHasQuit)
			{
				Sleep(0);
			}
		}

		DeleteCriticalSection(&m_DynamicObjectsCriticalSection);
		DeleteCriticalSection(&m_NavMeshDataCriticalSection);
		DeleteCriticalSection(&m_NavMeshImmediateAccessCriticalSection);
	}
#endif

	if(m_PathSearchPriorityQueue)
	{
		delete m_PathSearchPriorityQueue;
		m_PathSearchPriorityQueue = NULL;
	}

	if(m_DynamicObjectsStore)
	{
		delete[] m_DynamicObjectsStore;
		m_DynamicObjectsStore = NULL;
	}

#if !__FINAL
	if(m_PerfTimer)
	{
		delete m_PerfTimer;
		m_PerfTimer = NULL;
	}
	if(m_RequestProcessingOverallTimer)
	{
		delete m_RequestProcessingOverallTimer;
		m_RequestProcessingOverallTimer = NULL;
	}
	if(m_PolyTessellationTimer)
	{
		delete m_PolyTessellationTimer;
		m_PolyTessellationTimer = NULL;
	}
#endif
}

//********************************************************************************
//	WaitForNextPathRequest
//	Waits for the next path request to appear.  Depending on the implementation
//	and platform (eg. PS3 has no thread-event functions) we may either block
//	this thread until an event is signalled, or just sleep for a while.
//	Every so often this function will return a NULL, which lets the worker thread
//	perform various housekeeping duties even if it has no pending path requests
//********************************************************************************

CPathServerRequestBase *
CPathServerThread::WaitForNextRequest()
{
	// Reset our counters which total up how much work we've done since we last waited.
	// If we're processing as many requests as are available every time this thread
	// wakes up, then we can use these counters to throttle off the amount of work done.
	ms_iNumVisitedPolysInThisBatchOfPathRequests = 0;
	ms_iNumTessellationsInThisBatchOfPathRequests = 0;

	// Awake 20 times a second
	// If there are not requests pending, and it's not time to perform housekeeping - then the thread
	// will go back to sleep.
	static dev_s32 iThreadWakeTimeMS = 50; //CPathServer::m_iThreadHousekeepingFrequencyInMillisecs;

	while(1)
	{
#ifdef GTA_ENGINE
		// Don't service any requests when the game is paused
		if(fwTimer::IsGamePaused())
		{
			do
			{
				ResetRequestTimers();

				// Sleep for 2 secs. 
				sysIpcSleep(2000);

			} while (fwTimer::IsGamePaused() && !m_bQuitNow);

			ResetRequestTimers();
		}

		if(m_bQuitNow)
			return NULL;
#endif

#ifdef GTA_ENGINE
		if(CPathServer::ms_bUseEventsForRequests)
		{
			sysIpcWaitSemaTimed(CPathServer::m_PathRequestSema, iThreadWakeTimeMS);

			CPathServerRequestBase * pRequest = GetNextRequest();
			
			// Always return the request, even if NULL.  This is because if we recieve a NULL event, we will always
			// be ready to do a housekeeping task since the sysIpcCheckEvent func will quit out every multiple
			// of 'm_iThreadHousekeepingFrequencyInMillisecs'.
			return pRequest;
		}
#else // GTA_ENGINE
		{
			Sleep(CPathServer::m_iTimeToSleepWaitingForRequestsMs);

			CPathServerRequestBase * pRequest = GetNextRequest();
			if(pRequest)
				return pRequest;
			return NULL;
		}
#endif

	}	// Keep on looping until we get a path request, or it is time to do some thread housekeeping stuff
}

void CPathServerThread::DoHousekeeping()
{
#ifdef GTA_ENGINE
	if(!CPathServer::m_bGameRunning)
		return;

	if(m_bForcePerformThreadHousekeeping ||
		fwTimer::GetTimeInMilliseconds() - CPathServer::m_iLastTimeThreadDidHousekeeping >= CPathServer::m_iThreadHousekeepingFrequencyInMillisecs)
	{
		CPathServer::m_iLastTimeThreadDidHousekeeping = fwTimer::GetTimeInMilliseconds();

#if !__FINAL
		m_PerfTimer->Reset();
		m_PerfTimer->Start();
#endif

//		PF_START_TIMEBAR_DETAIL("Remove & Add objects");

		// Remove objects which are flagged for deletion, and activate those which are newly added
		RemoveAndAddObjects();

		// Climbs must be removed for climbable objects which have been uprooted; we cannot do this
		// at the time of the uproot for it may cause a long stall on the main thread whilst we wait
		// for a safe time to access the loaded navmeshes
		ProcessDeferredDeactivationOfClimbs();

		UpdateAllDynamicObjectsPositions();

#if !__FINAL
		m_PerfTimer->Stop();
		CPathServer::m_fTimeTakenToAddRemoveUpdateObjects = (float) m_PerfTimer->GetTimeMS();
#endif
	}

//	PF_START_TIMEBAR_DETAIL("Extract coverpoints");

	// We will periodically run a timesliced algorithm which extracts coverpoints from the loaded navmeshes,
	// and places them into a buffer (m_CoverPointsBuffer).  The CCover class can then query this buffer
	// to create CCoverPoint's without incurring a stall on the main game thread.
	CPathServer::MaybeExtractCoverPointsFromNavMeshes();

//	PF_START_TIMEBAR_DETAIL("Find ped-generation coords");

	// We periodically search through navmeshes to find potential positions to create new peds.  This can be
	// quite an expensive process & difficult to timeslice, so we opt to do this as a background task in this
	// pathfinding thread.

	const bool bPedGenTimerExpired = m_bForcePerformThreadHousekeeping || (fwTimer::GetTimeInMilliseconds() - CPathServer::m_iLastTimeProcessedPedGeneration) >= CPathServer::m_iProcessPedGenerationFreq;

	if(bPedGenTimerExpired)
	{
		CPathServerAmbientPedGen & ambientPedGen = CPathServerGta::GetAmbientPedGen();
		CPathServerOpponentPedGen & opponentPedGen = CPathServerGta::GetOpponentPedGen();

		static CPathServer::EPedGenProcessing iLastPedGenMode = CPathServer::EPedGen_OpponentSpawning;

		// Choose the next pedgen processing; initially this will be the one which we didn't process last time
		CPathServer::EPedGenProcessing iNextPedGenMode = (iLastPedGenMode==CPathServer::EPedGen_AmbientPeds) ?
				CPathServer::EPedGen_OpponentSpawning : CPathServer::EPedGen_AmbientPeds;
			
		// Now adjust this if we have nothing to do in the current mode?
		// Or if it is more important that we be processing ambient ped generation (eg. when attempting to instantly fill the ambient population)
		if(iNextPedGenMode == CPathServer::EPedGen_OpponentSpawning && ( /*!NetworkInterface::IsGameInProgress() ||*/ CPedPopulation::GetInstantFillPopulation() || (opponentPedGen.GetState()!=CPathServerOpponentPedGen::STATE_ACTIVE && ambientPedGen.m_bPedGenNeedsProcessing)))
		{
			// Nothing to do for multiplayer ped spawning, but work still to do on ambient peds?  Fine, choose to process ambient peds this time
			iNextPedGenMode = CPathServer::EPedGen_AmbientPeds;
		}
		else if(iNextPedGenMode == CPathServer::EPedGen_AmbientPeds && ambientPedGen.m_bPedGenNeedsProcessing == false && opponentPedGen.GetState()==CPathServerOpponentPedGen::STATE_ACTIVE)
		{
			// Nothing to do for ambient ped creation, but we have multiplayer spawning in need of attention?
			iNextPedGenMode = CPathServer::EPedGen_OpponentSpawning;
		}
		
		if(iNextPedGenMode == CPathServer::EPedGen_AmbientPeds && ambientPedGen.m_bPedGenNeedsProcessing)
		{
			CPathServer::m_iLastTimeProcessedPedGeneration = fwTimer::GetTimeInMilliseconds();

#if !__FINAL
			m_PerfTimer->Reset();
			m_PerfTimer->Start();
#endif

			ambientPedGen.FindPedGenCoordsBackgroundTask();

#if !__FINAL
			m_PerfTimer->Stop();
			CPathServer::m_fTimeTakenToDoPedGenInMSecs = (float) m_PerfTimer->GetTimeMS();
#endif
		}

		else if(iNextPedGenMode == CPathServer::EPedGen_OpponentSpawning && opponentPedGen.GetState()==CPathServerOpponentPedGen::STATE_ACTIVE)
		{
			CPathServer::m_iLastTimeProcessedPedGeneration = fwTimer::GetTimeInMilliseconds();

#if !__FINAL
			m_PerfTimer->Reset();
			m_PerfTimer->Start();
#endif

			opponentPedGen.FindSpawnCoordsBackgroundTask();

#if !__FINAL
			m_PerfTimer->Stop();
			CPathServer::m_fTimeTakenToDoPedGenInMSecs = (float) m_PerfTimer->GetTimeMS();
#endif
		}

		iLastPedGenMode = iNextPedGenMode;
	}

//	PF_POP_TIMEBAR_DETAIL();

	// For some reason the Timerbars code does not properly end PIX events when popping a timer,
	// so do it manually - I don't want my main path-requests section to be nested within the
	// 'housekeeping' section when I do a profile!
#if __XENON && !__FINAL
//	PIXEnd(); 
#endif

#endif // GTA_ENGINE

	m_bForcePerformThreadHousekeeping = false;
}

void CPathServerThread::AddDeferredDisableClimbAtPosition(const Vector3 & vPos, const bool bNavmeshClimb, const bool bClimbableObject)
{
	s32 iType = 0;
	if(bNavmeshClimb && bClimbableObject)
		iType = TDeferredClimbDeactivation::EBoth;
	else if(bNavmeshClimb)
		iType = TDeferredClimbDeactivation::ENavMeshClimb;
	else if(bClimbableObject)
		iType = TDeferredClimbDeactivation::EClimbableObject;
	else
	{
		Assertf(false, "At least one type of climb must be specified");
		return;
	}

	Assertf(m_iNumDeferredClimbDeactivations < MAX_DEFERRED_CLIMB_DEACTIVATIONS, "Too many climbs marked for deactivation");
	if(m_iNumDeferredClimbDeactivations < MAX_DEFERRED_CLIMB_DEACTIVATIONS)
	{
		m_DeferredClimbDeactivations[m_iNumDeferredClimbDeactivations].type = iType;
		m_DeferredClimbDeactivations[m_iNumDeferredClimbDeactivations].xyz[0] = vPos.x;
		m_DeferredClimbDeactivations[m_iNumDeferredClimbDeactivations].xyz[1] = vPos.y;
		m_DeferredClimbDeactivations[m_iNumDeferredClimbDeactivations].xyz[2] = vPos.z;

		m_iNumDeferredClimbDeactivations++;
	}
}

void CPathServerThread::ProcessDeferredDeactivationOfClimbs()
{
#ifdef GTA_ENGINE
	for(s32 c=0; c<m_iNumDeferredClimbDeactivations; c++)
	{
		TDeferredClimbDeactivation & cd = m_DeferredClimbDeactivations[c];
		Vector3 vPos(cd.xyz[0], cd.xyz[1], cd.xyz[2]);
		switch(cd.type)
		{
			case TDeferredClimbDeactivation::ENavMeshClimb:
				CPathServer::DisableClimbAtPosition(vPos, false);
				break;
			case TDeferredClimbDeactivation::EClimbableObject:
				CPathServer::DisableClimbAtPosition(vPos, true);
				break;
			case TDeferredClimbDeactivation::EBoth:
				CPathServer::DisableClimbAtPosition(vPos, false);
				CPathServer::DisableClimbAtPosition(vPos, true);
				break;
		}		
	}

	m_iNumDeferredClimbDeactivations = 0;
#endif
}

// If the game has been paused, then we need to increase the "start-time" of all the path requests (etc)
// by this amount of time.. Otherwise all pending requests will be canceled the instant we unpause!
void
CPathServerThread::ResetRequestTimers()
{
#ifdef GTA_ENGINE

	u32 iTime = fwTimer::GetTimeInMilliseconds();

	LOCK_REQUESTS

	s32 i;
	for(i=0; i<MAX_NUM_PATH_REQUESTS; i++)
	{
		if(CPathServer::m_PathRequests[i].m_hHandle)
			CPathServer::m_PathRequests[i].m_iTimeRequestIssued = iTime;
	}
	for(i=0; i<MAX_NUM_GRID_REQUESTS; i++)
	{
		if(CPathServer::m_GridRequests[i].m_hHandle)
			CPathServer::m_GridRequests[i].m_iTimeRequestIssued = iTime;
	}
	for(i=0; i<MAX_NUM_LOS_REQUESTS; i++)
	{
		if(CPathServer::m_LineOfSightRequests[i].m_hHandle)
			CPathServer::m_LineOfSightRequests[i].m_iTimeRequestIssued = iTime;
	}
	for(i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
	{
		if(CPathServer::m_AudioRequests[i].m_hHandle)
			CPathServer::m_AudioRequests[i].m_iTimeRequestIssued = iTime;
	}
	for(i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
	{
		if(CPathServer::m_FloodFillRequests[i].m_hHandle)
			CPathServer::m_FloodFillRequests[i].m_iTimeRequestIssued = iTime;
	}
	for(i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
	{
		if(CPathServer::m_ClearAreaRequests[i].m_hHandle)
			CPathServer::m_ClearAreaRequests[i].m_iTimeRequestIssued = iTime;
	}
	for(i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
	{
		if(CPathServer::m_ClosestPositionRequests[i].m_hHandle)
			CPathServer::m_ClosestPositionRequests[i].m_iTimeRequestIssued = iTime;
	}

	UNLOCK_REQUESTS

#endif
}

#define __CHECK_FOR_STALE_PATH_REQUESTS		1

void
CPathServerThread::CheckForStaleRequests()
{
#ifdef GTA_ENGINE
#if __CHECK_FOR_STALE_PATH_REQUESTS

	LOCK_REQUESTS

	int i;

	// When the game is paused, we reset all the request timers to the current time to avoid annoying asserts.
	if(fwTimer::IsGamePaused())
	{
		for(i=0; i<MAX_NUM_PATH_REQUESTS; i++)
		{
			CPathServer::m_PathRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
		}
		for(i=0; i<MAX_NUM_GRID_REQUESTS; i++)
		{
			CPathServer::m_GridRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
		}
		for(i=0; i<MAX_NUM_LOS_REQUESTS; i++)
		{
			CPathServer::m_LineOfSightRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
		}
		for(i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
		{
			CPathServer::m_AudioRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
		}
		for(i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
		{
			CPathServer::m_FloodFillRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
		}
		for(i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
		{
			CPathServer::m_ClearAreaRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
		}
		for(i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
		{
			CPathServer::m_ClosestPositionRequests[i].m_iTimeRequestIssued = fwTimer::GetTimeInMilliseconds();
		}

		return;
	}


	for(i=0; i<MAX_NUM_PATH_REQUESTS; i++)
	{
		CPathRequest * pReq = &CPathServer::m_PathRequests[i];
		const bool bSlotEmpty = pReq->m_bSlotEmpty;
		const u32 iDeltaTime = fwTimer::GetTimeInMilliseconds() - pReq->m_iTimeRequestIssued;

		if(!bSlotEmpty && pReq->m_bComplete && iDeltaTime > CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs)
		{
			#if AI_OPTIMISATIONS_OFF
			Assertf(false, "NOTE - path request 0x%p (context 0x%p) has been waiting for %i ms to be retrieved - it will now be cancelled.\n", pReq, pReq->m_pContext, CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs);
			Assert(pReq->m_hHandle != PATH_HANDLE_NULL);

			#if TRACK_LEAKED_PATHS
			for(int t=0; t<CTask::_ms_pPool->GetSize(); t++)
			{
				CTask * pTask = CTask::_ms_pPool->GetSlot(t);
				if(pTask)
				{
					if(pTask->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
					{
						CTaskMoveFollowNavMesh * pNavTask = (CTaskMoveFollowNavMesh*)pTask;
						if(pReq->m_hHandle == pNavTask->GetPathHandle())
						{
							AssertMsg(false, "Found leaked path task (follow navmesh).");
						}
					}
					if(pTask->GetTaskType()==CTaskTypes::TASK_MOVE_WANDER)
					{
						CTaskMoveWander * pWanderTask = (CTaskMoveWander*)pTask;
						if(pReq->m_hHandle == pWanderTask->GetPathHandle())
						{
							AssertMsg(false, "Found leaked path task (wander).");
						}
					}
				}
			}
			#endif // TRACK_LEAKED_PATHS
			#endif // AI_OPTIMISATIONS_OFF

			if(pReq->m_hHandle != PATH_HANDLE_NULL)
				CPathServer::CancelRequest(pReq->m_hHandle);
			pReq->m_bSlotEmpty = true;
		}
	}

	for(i=0; i<MAX_NUM_GRID_REQUESTS; i++)
	{
		CGridRequest * pReq = &CPathServer::m_GridRequests[i];
		const bool bSlotEmpty = pReq->m_bSlotEmpty;
		const u32 iDeltaTime = fwTimer::GetTimeInMilliseconds() - pReq->m_iTimeRequestIssued;

		if(!bSlotEmpty && pReq->m_bComplete && iDeltaTime > CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs)
		{
			#if AI_OPTIMISATIONS_OFF
			Assertf(false, "NOTE - grid request 0x%p (context 0x%p) has been waiting for %i ms to be retrieved - it will now be cancelled.\n", pReq, pReq->m_pContext, CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs);
			#endif

			if(pReq->m_hHandle != PATH_HANDLE_NULL)
				CPathServer::CancelRequest(pReq->m_hHandle);
			pReq->m_bSlotEmpty = true;
		}
	}

	for(i=0; i<MAX_NUM_LOS_REQUESTS; i++)
	{
		CLineOfSightRequest * pReq = &CPathServer::m_LineOfSightRequests[i];
		const bool bSlotEmpty = pReq->m_bSlotEmpty;
		const u32 iDeltaTime = fwTimer::GetTimeInMilliseconds() - pReq->m_iTimeRequestIssued;

		if(!bSlotEmpty && pReq->m_bComplete && iDeltaTime > CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs)
		{
			#if AI_OPTIMISATIONS_OFF
			Assertf(false, "NOTE - LOS request 0x%p (context 0x%p) has been waiting for %i ms to be retrieved - it will now be cancelled.\n", pReq, pReq->m_pContext, CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs);
			#endif

			if(pReq->m_hHandle != PATH_HANDLE_NULL)
				CPathServer::CancelRequest(pReq->m_hHandle);
			pReq->m_bSlotEmpty = true;
		}
	}

	for(i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
	{
		CAudioRequest * pReq = &CPathServer::m_AudioRequests[i];
		const bool bSlotEmpty = pReq->m_bSlotEmpty;
		const u32 iDeltaTime = fwTimer::GetTimeInMilliseconds() - pReq->m_iTimeRequestIssued;

		if(!bSlotEmpty && pReq->m_bComplete && iDeltaTime > CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs)
		{
			#if NAVMESH_OPTIMISATIONS_OFF
			Assertf(false, "NOTE - audio request 0x%p (context 0x%p) has been waiting for %i ms to be retrieved - it will now be cancelled.\n", pReq, pReq->m_pContext, CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs);
			#endif

			if(pReq->m_hHandle != PATH_HANDLE_NULL)
				CPathServer::CancelRequest(pReq->m_hHandle);
			pReq->m_bSlotEmpty = true;
		}
	}

	for(i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
	{
		CFloodFillRequest * pReq = &CPathServer::m_FloodFillRequests[i];
		const bool bSlotEmpty = pReq->m_bSlotEmpty;
		const u32 iDeltaTime = fwTimer::GetTimeInMilliseconds() - pReq->m_iTimeRequestIssued;

		if(!bSlotEmpty && pReq->m_bComplete && iDeltaTime > CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs)
		{
			#if AI_OPTIMISATIONS_OFF
			Assertf(false, "NOTE - floodfill request 0x%p (context 0x%p) has been waiting for %i ms to be retrieved - it will now be cancelled.\n", pReq, pReq->m_pContext, CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs);
			#endif

			if(pReq->m_hHandle != PATH_HANDLE_NULL)
				CPathServer::CancelRequest(pReq->m_hHandle);
			pReq->m_bSlotEmpty = true;
		}
	}

	for(i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
	{
		CClearAreaRequest * pReq = &CPathServer::m_ClearAreaRequests[i];
		const bool bSlotEmpty = pReq->m_bSlotEmpty;
		const u32 iDeltaTime = fwTimer::GetTimeInMilliseconds() - pReq->m_iTimeRequestIssued;

		if(!bSlotEmpty && pReq->m_bComplete && iDeltaTime > CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs)
		{
#if AI_OPTIMISATIONS_OFF
			Assertf(false, "NOTE - ClearArea request 0x%p (context 0x%p) has been waiting for %i ms to be retrieved - it will now be cancelled.\n", pReq, pReq->m_pContext, CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs);
#endif

			if(pReq->m_hHandle != PATH_HANDLE_NULL)
				CPathServer::CancelRequest(pReq->m_hHandle);
			pReq->m_bSlotEmpty = true;
		}
	}

	for(i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
	{
		CClosestPositionRequest * pReq = &CPathServer::m_ClosestPositionRequests[i];
		const bool bSlotEmpty = pReq->m_bSlotEmpty;
		const u32 iDeltaTime = fwTimer::GetTimeInMilliseconds() - pReq->m_iTimeRequestIssued;

		if(!bSlotEmpty && pReq->m_bComplete && iDeltaTime > CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs)
		{
#if AI_OPTIMISATIONS_OFF
			Assertf(false, "NOTE - ClosestPosition request 0x%p (context 0x%p) has been waiting for %i ms to be retrieved - it will now be cancelled.\n", pReq, pReq->m_pContext, CPathServerRequestBase::ms_iRequestTimeoutValInMillisecs);
#endif

			if(pReq->m_hHandle != PATH_HANDLE_NULL)
				CPathServer::CancelRequest(pReq->m_hHandle);
			pReq->m_bSlotEmpty = true;
		}
	}

	UNLOCK_REQUESTS

#endif	// __CHECK_FOR_STALE_PATH_REQUESTS
#endif
}

void CPathServerThread::Run()
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);
	while(!m_bQuitNow)
	{
		TELEMETRY_SET_THREAD_NAME(m_iThreadID, "PathServer");

#ifdef GTA_ENGINE
// 		PF_FRAMEINIT_TIMEBARS();
// 		PF_START_TIMEBAR_IDLE("Idle");
#endif

#ifdef GTA_ENGINE
		// If the game is loading/restarting then don't run this thread
		while( (!CPathServer::m_bGameRunning || !CPathServer::ms_bGameInSession) && !m_bQuitNow)
		{
			sysIpcSleep(1000);
		}
#endif
		CPathServerRequestBase * pRequest = NULL;

		if(!m_bForcePerformThreadHousekeeping)
		{
			if(CPathServer::ms_bProcessAllPendingPathsAtOnce)
			{
				pRequest = GetNextRequest();
				if(!pRequest)
					pRequest = WaitForNextRequest();
			}
			else
			{
				pRequest = WaitForNextRequest();
			}
		}

#ifdef GTA_ENGINE

		// Disable defragging whilst pathserver thread is active
		if(CPathServer::GetNavMeshStore(kNavDomainRegular))
			CPathServer::GetNavMeshStore(kNavDomainRegular)->SetDefragmentCopyBlocked(true);
		if(CPathServer::GetNavMeshStore(kNavDomainHeightMeshes))
			CPathServer::GetNavMeshStore(kNavDomainHeightMeshes)->SetDefragmentCopyBlocked(true);


		//-------------------------------------------------------------------------------------------
		// Under certain conditions we will wish to stall the pathserver thread in a safe condition:
		// 1) If the pathserver is waiting to perform RequestAndEvict from the main thread
		// 2) If we have scripted blocking objects waiting to be added

		while( sysInterlockedAnd(&CPathServer::ms_iBlockRequestsFlags, 0xffffffff) !=0 )
		{
			sysIpcSleep(1);
		}
#endif

		{	// Enter scope, so that navmeshes critical section will be automatically left later

#ifdef GTA_ENGINE
			LOCK_NAVMESH_DATA;
#else
			EnterCriticalSection(&m_NavMeshDataCriticalSection);
#endif

			fwPathServer::m_bPathServerThreadIsActive = true;

			//----------------------------------------------------------------------------------
			// Periodically do housekeeping tasks - switching cpu, extracting coverpoints, etc

			DoHousekeeping();

			//----------------------
			// Service requests

			if(pRequest && CPathServer::m_bGameRunning)
			{

				m_pCurrentActiveRequest = NULL;
				m_pCurrentActivePathRequest = NULL;
				m_pCurrentActiveGridRequest = NULL;
				m_pCurrentActiveLosRequest = NULL;
				m_pCurrentActiveAudioRequest = NULL;
				m_pCurrentActiveFloodFillRequest = NULL;
				m_pCurrentActiveClearAreaRequest = NULL;
				m_pCurrentActiveClosestPositionRequest = NULL;

#if !__FINAL
				m_RequestProcessingOverallTimer->Reset();
				m_RequestProcessingOverallTimer->Start();
				pRequest->m_iNumTimesSlept = 0;
#endif

				//*******************************************************************
				//	See what type of path request this is.  Each request should be
				//	handled in it's own Process...() function..
				//*******************************************************************

				pRequest->m_iCompletionCode = PATH_NOT_FOUND;

				// Grid request
				switch(pRequest->m_iType)
				{
					//---------------------------------------------------------
					// Process 'CGridRequest' type requests
					// UNUSED

					case EGrid:
					{
						Assertf(false, "request type not in use");

						CGridRequest * pGridRequest = (CGridRequest*) pRequest;

						m_pCurrentActiveRequest = pGridRequest;
						m_pCurrentActiveGridRequest = pGridRequest;

						SampleWalkableGrid(pGridRequest);

						break;
					}

					//---------------------------------------------------------
					// Process 'CPathRequest' type requests

					case EPath:
					{
						CPathRequest * pPathRequest = (CPathRequest*) pRequest;
#if !__FINAL
#ifdef GTA_ENGINE
						pPathRequest->m_iFrameRequestStarted = fwTimer::GetFrameCount();
#endif
#endif
						Assert(fwPathServer::GetCurrentNumTessellationPolys() == 0);

						PIXBeginCN(0, Color_purple.GetColor(), "ProcessPathRequest 0x%x", pRequest->m_hHandle);

#if __TRACE_PATH_REQUESTS_ON_XENON
						if(CPathServer::ms_bUseXTraceOnTheNextPathRequest)
						{
							XTraceStartRecording( "e:\\PathRequestTrace.bin" );
						}
#endif

						m_pCurrentActiveRequest = pPathRequest;
						m_pCurrentActivePathRequest = pPathRequest;

						bool bOk = true;

						if(pPathRequest->m_iCompletionCode == PATH_CANCELLED)
						{
							bOk = false;
						}
						else
						{
							bOk = ProcessPathRequest(pPathRequest);
						}

						m_pCurrentActivePathRequest = NULL;

						// If not ok, then the error code will have been set within ProcessPathRequest()
						if(!bOk)
						{

						}
						else
						{
							if(!pPathRequest->m_iNumPoints)
							{
								// If no points, then set a special completion code
								pPathRequest->m_iCompletionCode = PATH_NO_POINTS;
							}
							else
							{
								// Path was found, so fill in the completion data
								// Note that the "RefinePathAndCreateNodes()" function will have
								// already created the waypoints, etc..
								pPathRequest->m_iCompletionCode = PATH_FOUND;
							}
						}

						{
							sysCriticalSection critSec(pPathRequest->m_CriticalSectionToken);

							pPathRequest->m_bRequestActive = false;

							// If the request was interrupted then don't mark it as complete.
							if(pPathRequest->m_iCompletionCode == PATH_ABORTED_DUE_TO_ANOTHER_THREAD)
							{
								pPathRequest->m_bRequestPending = true;
							}
							else
							{
								pPathRequest->m_bComplete = true;
	#if !__FINAL
	#ifdef GTA_ENGINE
								pPathRequest->m_iFrameRequestCompleted = fwTimer::GetFrameCount();
	#endif
	#endif
							}

							pPathRequest->m_bWaitingToAbort = false;
						}

						// Detessellate polys
						if(CPathServer::ms_bDoPolyUnTessellation)
						{
#if !__FINAL
							m_PerfTimer->Reset();
							m_PerfTimer->Start();
#endif
							//--------------------------------------------------------------------------
							// Remove all polygon tessellations, and return navmeshes to original state	

							fwPathServer::DetessellateAllPolys();

#if !__FINAL
							m_PerfTimer->Stop();
							pPathRequest->m_fMillisecsSpentInDetessellation = (float) m_PerfTimer->GetTimeMS();
#endif
							
						}

						if(pPathRequest->m_bHasAdjustedDynamicObjectsMinMaxForWidth)
						{
							Assert(pPathRequest->m_bUseVariableEntityRadius);
							const float fRadiusDelta = PATHSERVER_PED_RADIUS - pPathRequest->m_fEntityRadius;
							Assert(fRadiusDelta < 0.0f);
							CDynamicObjectsContainer::AdjustAllBoundsByAmount(m_Vars.m_PathSearchDistanceMinMax, fRadiusDelta, false);
							pPathRequest->m_bHasAdjustedDynamicObjectsMinMaxForWidth = false;
						}

	#if __TRACE_PATH_REQUESTS_ON_XENON
						if(CPathServer::ms_bUseXTraceOnTheNextPathRequest)
						{
							XTraceStopRecording();
							CPathServer::ms_bUseXTraceOnTheNextPathRequest = false;
						}
	#endif

						PIXEnd();

						break;
					}

					//---------------------------------------------------------
					// Process 'CLineOfSightRequest' type requests

					case ELineOfSight:
					{
						CLineOfSightRequest * pLosRequest = (CLineOfSightRequest*) pRequest;

						PIXBeginCN(0, Color_purple.GetColor(), "ProcessLosRequest 0x%x", pRequest->m_hHandle);

						m_pCurrentActiveRequest = pLosRequest;
						m_pCurrentActiveLosRequest = pLosRequest;

						if(ProcessLineOfSightRequest(pLosRequest))
						{
							// Set completion status to PATH_FOUND.  If the request failed, then the status
							// will have already been set in the Process...() function
							pLosRequest->m_iCompletionCode = PATH_FOUND;
						}

						{
							sysCriticalSection critSec(pLosRequest->m_CriticalSectionToken);

							pLosRequest->m_bRequestActive = false;
							pLosRequest->m_bComplete = true;
							pLosRequest->m_bWaitingToAbort = false;
						}
#if !__FINAL
#ifdef GTA_ENGINE
						pLosRequest->m_iFrameRequestCompleted = fwTimer::GetFrameCount();
#endif
#endif

						PIXEnd();

						break;
					}

					//---------------------------------------------------------
					// Process 'CAudioRequest' type requests

					case EAudioProperties:
					{
						CAudioRequest * pAudioRequest = (CAudioRequest*) pRequest;

						PIXBeginCN(0, Color_purple.GetColor(), "ProcessAudioRequest 0x%x", pRequest->m_hHandle);

						m_pCurrentActiveRequest = pAudioRequest;
						m_pCurrentActiveAudioRequest = pAudioRequest;

						if(ProcessAudioPropertiesRequest(pAudioRequest))
						{
							// Set completion status to PATH_FOUND.  If the request failed, then the status
							// will have already been set in the Process...() function
							pAudioRequest->m_iCompletionCode = PATH_FOUND;
						}

						{
							sysCriticalSection critSec(pAudioRequest->m_CriticalSectionToken);

							pAudioRequest->m_bRequestActive = false;
							pAudioRequest->m_bComplete = true;
							pAudioRequest->m_bWaitingToAbort = false;
						}
#if !__FINAL
#ifdef GTA_ENGINE
						pAudioRequest->m_iFrameRequestCompleted = fwTimer::GetFrameCount();
#endif
#endif
						PIXEnd();

						break;
					}

					//---------------------------------------------------------
					// Process 'CFloodFillRequest' type requests

					case EFloodFill:
					{
						CFloodFillRequest * pFloodFillRequest = (CFloodFillRequest*) pRequest;

						PIXBeginCN(0, Color_purple.GetColor(), "ProcessFloodFillRequest 0x%x", pRequest->m_hHandle);

						m_pCurrentActiveRequest = pFloodFillRequest;
						m_pCurrentActiveFloodFillRequest = pFloodFillRequest;

						if(ProcessFloodFillRequest(pFloodFillRequest))
						{
							// Set completion status to PATH_FOUND.  If the request failed, then the status
							// will have already been set in the Process...() function
							pFloodFillRequest->m_iCompletionCode = PATH_FOUND;
						}

						{
							sysCriticalSection critSec(pFloodFillRequest->m_CriticalSectionToken);

							pFloodFillRequest->m_bRequestActive = false;

							// If the request was interrupted then don't mark it as complete.
							if(pFloodFillRequest->m_iCompletionCode == PATH_ABORTED_DUE_TO_ANOTHER_THREAD)
							{
								pFloodFillRequest->m_bRequestPending = true;
							}
							else
							{
								pFloodFillRequest->m_bComplete = true;
							}
							pFloodFillRequest->m_bWaitingToAbort = false;

						}
#if !__FINAL
#ifdef GTA_ENGINE
						pFloodFillRequest->m_iFrameRequestCompleted = fwTimer::GetFrameCount();
#endif
#endif

						PIXEnd();

						break;
					}

					//---------------------------------------------------------
					// Process 'CClearAreaRequest' type requests


					case EClearArea:
					{
						CClearAreaRequest * pClearAreaRequest = (CClearAreaRequest*) pRequest;

						PIXBeginCN(0, Color_purple.GetColor(), "ProcessClearAreaRequest 0x%x", pRequest->m_hHandle);

						m_pCurrentActiveRequest = pClearAreaRequest;
						m_pCurrentActiveClearAreaRequest = pClearAreaRequest;

						if(ProcessClearAreaRequest(pClearAreaRequest))
						{
							// Set completion status to PATH_FOUND.  If the request failed, then the status
							// will have already been set in the Process...() function
							pClearAreaRequest->m_iCompletionCode = PATH_FOUND;
						}

						{
							sysCriticalSection critSec(pClearAreaRequest->m_CriticalSectionToken);

							pClearAreaRequest->m_bRequestActive = false;

							// If the request was interrupted then don't mark it as complete.
							if(pClearAreaRequest->m_iCompletionCode == PATH_ABORTED_DUE_TO_ANOTHER_THREAD)
							{
								pClearAreaRequest->m_bRequestPending = true;
							}
							else
							{
								pClearAreaRequest->m_bComplete = true;
							}
							pClearAreaRequest->m_bWaitingToAbort = false;
						}
#if !__FINAL
#ifdef GTA_ENGINE
						pClearAreaRequest->m_iFrameRequestCompleted = fwTimer::GetFrameCount();
#endif
#endif

						PIXEnd();

						break;
					}

					//---------------------------------------------------------
					// Process 'CClosestPositionRequest' type requests

					case EClosestPosition:
					{
						CClosestPositionRequest * pClosestPosRequest = (CClosestPositionRequest*) pRequest;

						PIXBeginCN(0, Color_purple.GetColor(), "CClosestPositionRequest 0x%x", pRequest->m_hHandle);

						m_pCurrentActiveRequest = pClosestPosRequest;
						m_pCurrentActiveClosestPositionRequest = pClosestPosRequest;

						if(ProcessClosestPositionRequest(pClosestPosRequest))
						{
							// Set completion status to PATH_FOUND.  If the request failed, then the status
							// will have already been set in the Process...() function
							pClosestPosRequest->m_iCompletionCode = PATH_FOUND;
						}

						{
							sysCriticalSection critSec(pClosestPosRequest->m_CriticalSectionToken);

							pClosestPosRequest->m_bRequestActive = false;

							// If the request was interrupted then don't mark it as complete.
							if(pClosestPosRequest->m_iCompletionCode == PATH_ABORTED_DUE_TO_ANOTHER_THREAD)
							{
								pClosestPosRequest->m_bRequestPending = true;
							}
							else
							{
								pClosestPosRequest->m_bComplete = true;
							}

							pClosestPosRequest->m_bWaitingToAbort = false;

						}
#if !__FINAL
#ifdef GTA_ENGINE
						pClosestPosRequest->m_iFrameRequestCompleted = fwTimer::GetFrameCount();
#endif
#endif

						PIXEnd();

						break;
					}

					default:
					{
						break;
					}
				}
	#ifdef GTA_ENGINE
//				PF_START_TIMEBAR_DETAIL("End Process Requests");
	#endif

	#if !__FINAL
				m_RequestProcessingOverallTimer->Stop();
				pRequest->m_fTotalProcessingTimeInMillisecs = (float) m_RequestProcessingOverallTimer->GetTimeMS();
				CPathServer::m_fRunningTotalOfTimeOnPathRequests += pRequest->m_fTotalProcessingTimeInMillisecs;
	#endif

				m_pCurrentActiveRequest = NULL;
				m_pCurrentActivePathRequest = NULL;
				m_pCurrentActiveGridRequest = NULL;
				m_pCurrentActiveLosRequest = NULL;
				m_pCurrentActiveAudioRequest = NULL;
				m_pCurrentActiveFloodFillRequest = NULL;
				m_pCurrentActiveClearAreaRequest = NULL;
				m_pCurrentActiveClosestPositionRequest = NULL;

				// Leave the navmeshes critical section
#ifndef GTA_ENGINE
				LeaveCriticalSection(&m_NavMeshDataCriticalSection);
#endif
			}

			//-------------------------------------------------------------------------------------
			// If the processing of a request has been forced to abort by the streaming system,
			// then we must restore this thread's priority to its normal level, as it might have
			// been boosted so that it took immediate control.
			// We then need to yield, since now that we have released the critical section on the
			// navmeshes - the main thread can continue to process the streaming.

			if(CPathServer::m_bForceAbortCurrentRequest)
			{
				if(pRequest)
					pRequest->m_bWasAborted = true;
				CPathServer::m_bForceAbortCurrentRequest = false;
				fwPathServer::m_bPathServerThreadIsActive = false;

				sysIpcYield(0);
			}

			//-------------------------------------------------------------------------------------
			// If the previous request caused either of the the internal timestamps to overflow
			// then ensure all timestamps in all navmesh polygons are reset.
			// This process was moved out of IncTimeStamp() to avoid thread locks when this this
			// thread and another are trying to lock the navmesh stores and the navmesh data in
			// reverse order.  This couldn't be solved without locking the navmesh data from the
			// main thread during RequestAndEvict, which would have caused a stall.
			// I've safeguarded against an overflow causing rare pathfinding errors when touching
			// polygons with an old timestamp, by lowering the max timestamp value to 

			if(m_bHandleTimestampOverflow)
			{
				HandleTimeStampOverFlow();
				m_bHandleTimestampOverflow = false;
			}
			if(m_bHandleAStarTimestampOverflow)
			{
				HandleAStarTimeStampOverFlow();
				m_bHandleAStarTimestampOverflow = false;
			}
		}

		fwPathServer::m_bPathServerThreadIsActive = false;

#ifdef GTA_ENGINE
		// Enable defragging again
		if(CPathServer::GetNavMeshStore(kNavDomainRegular))
			CPathServer::GetNavMeshStore(kNavDomainRegular)->SetDefragmentCopyBlocked(false);
		if(CPathServer::GetNavMeshStore(kNavDomainHeightMeshes))
			CPathServer::GetNavMeshStore(kNavDomainHeightMeshes)->SetDefragmentCopyBlocked(false);
#endif
	}

#ifndef GTA_ENGINE
	LeaveCriticalSection(&m_NavMeshDataCriticalSection);
	m_hThreadHandle = 0;
#endif

	m_iThreadID = 0;
	m_bHasQuit = true;
}


s32 CPathServerThread::GetNumPendingRequests() const
{
	s32 iNum=0;
	int r;

	for(r=0; r<MAX_NUM_PATH_REQUESTS; r++)
	{
		if(CPathServer::m_PathRequests[r].m_bRequestPending) iNum++;
	}
	for(r=0; r<MAX_NUM_GRID_REQUESTS; r++)
	{
		if(CPathServer::m_GridRequests[r].m_bRequestPending) iNum++;
	}
	for(r=0; r<MAX_NUM_LOS_REQUESTS; r++)
	{
		if(CPathServer::m_LineOfSightRequests[r].m_bRequestPending) iNum++;
	}
	for(r=0; r<MAX_NUM_AUDIO_REQUESTS; r++)
	{
		if(CPathServer::m_AudioRequests[r].m_bRequestPending) iNum++;
	}
	for(r=0; r<MAX_NUM_FLOODFILL_REQUESTS; r++)
	{
		if(CPathServer::m_FloodFillRequests[r].m_bRequestPending) iNum++;
	}
	for(r=0; r<MAX_NUM_CLEARAREA_REQUESTS; r++)
	{
		if(CPathServer::m_ClearAreaRequests[r].m_bRequestPending) iNum++;
	}
	for(r=0; r<MAX_NUM_CLOSESTPOSITION_REQUESTS; r++)
	{
		if(CPathServer::m_ClosestPositionRequests[r].m_bRequestPending) iNum++;
	}
	return iNum;
}

//*******************************************************
//	GetNextRequest
//	Gets the next request.  This can be a path, grid
//	or line-of-sight request.  Depending upon which
//	type we processed last, we will choose a different
//	type this time.
//*******************************************************

CPathServerRequestBase * CPathServerThread::GetNextRequest(void)
{
	// If there are any scripted paths pending in the request list, then treat them as highest priority -
	// force the next request to be processed to be a path request so that they will be serviced immediately.
	for(int r=0; r<MAX_NUM_PATH_REQUESTS; r++)
	{
		if( CPathServer::m_PathRequests[r].m_bRequestPending && (CPathServer::m_PathRequests[r].m_bScriptedRoute || CPathServer::m_PathRequests[r].m_bMissionPed || CPathServer::m_PathRequests[r].m_bHighPrioRoute))
		{
			m_iLastRequestType = ENoRequest;
			break;
		}
	}

	const s32 iStartType = (EPathServerRequestType)((m_iLastRequestType+1)%ENumRequestTypes);
	s32 iCurrentType = iStartType;
	CPathServerRequestBase * pRequest = NULL;

	do 
	{
		switch(iCurrentType)
		{
			case EPath:
				pRequest = GetNextPathRequest();
				break;
			case EGrid:
				pRequest = GetNextGridRequest();
				break;
			case ELineOfSight:
				pRequest = GetNextLosRequest();
				break;
			case EAudioProperties:
				pRequest = GetNextAudioPropertiesRequest();
				break;
			case EFloodFill:
				pRequest = GetNextFloodFillRequest();
				break;
			case EClearArea:
				pRequest = GetNextClearAreaRequest();
				break;
			case EClosestPosition:
				pRequest = GetNextClosestPositionRequest();
				break;
			default:
				Assert(false);
				break;
		}
		if(pRequest)
		{
			m_iLastRequestType = iCurrentType;
			return pRequest;
		}
		iCurrentType = (iCurrentType+1)%(int)ENumRequestTypes;

	} while(iCurrentType != iStartType);

	return NULL;
}


//****************************************************
// Get the next path request to process.
// Choose the one with the lowest non-zero handle
//****************************************************
CPathRequest * CPathServerThread::GetNextPathRequest(void)
{
	return GetNextPathRequest_New();
}

//****************************************************
// Get the next path request to process.
// Choose the one with the lowest non-zero handle
//****************************************************
CPathRequest * CPathServerThread::GetNextPathRequest_New()
{
	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	s32 iIndex = -1;
	u32 iHighestHandle = 0;

	int i;
	for(i=0; i<MAX_NUM_PATH_REQUESTS; i++)
	{
		CPathRequest * pPathRequest = &CPathServer::m_PathRequests[i];

		if(pPathRequest->m_bRequestPending && pPathRequest->m_hHandle != 0)
		{
			iHighestHandle = Max(iHighestHandle, pPathRequest->m_hHandle);
		}
	}

	float fHighestPriority = -1.0f;

	for(i=0; i<MAX_NUM_PATH_REQUESTS; i++)
	{
		CPathRequest * pPathRequest = &CPathServer::m_PathRequests[i];

		if(pPathRequest->m_bRequestPending && pPathRequest->m_hHandle != 0)
		{
			float fPriority = (float)(iHighestHandle - pPathRequest->m_hHandle);

			if(pPathRequest->m_bHighPrioRoute)
				fPriority *= 2.0f;
			if(pPathRequest->m_bMissionPed || pPathRequest->m_bScriptedRoute)
				fPriority *= 2.0f;

			if(fPriority > fHighestPriority)
			{
				iIndex = i;
				fHighestPriority = fPriority;
			}
		}
	}

	if(iIndex == -1)
	{
		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
		return NULL;
	}

	CPathServer::m_PathRequests[iIndex].m_bRequestPending = false;
	CPathServer::m_PathRequests[iIndex].m_bRequestActive = true;

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return &CPathServer::m_PathRequests[iIndex];
}

//****************************************************
// Get the next grid request to process.
// Choose the one with the lowest non-zero handle
//****************************************************
CGridRequest * CPathServerThread::GetNextGridRequest(void)
{
	u32 iLowestHandle = 0xFFFFFFFF;
	s32 iIndex = -1;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i;
	for(i=0; i<MAX_NUM_GRID_REQUESTS; i++)
	{
		CGridRequest * pGridRequest = &CPathServer::m_GridRequests[i];

		if(pGridRequest->m_bRequestPending &&
			pGridRequest->m_hHandle != 0 &&
			pGridRequest->m_hHandle < iLowestHandle)
		{
			iLowestHandle = pGridRequest->m_hHandle;
			iIndex = i;
		}
	}

	if(iIndex == -1)
	{
		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
		return NULL;
	}

	CPathServer::m_GridRequests[iIndex].m_bRequestPending = false;
	CPathServer::m_GridRequests[iIndex].m_bRequestActive = true;

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
	return &CPathServer::m_GridRequests[iIndex];
}

//****************************************************
// Get the next line-of-sight request to process.
// Choose the one with the lowest non-zero handle
//****************************************************
CLineOfSightRequest * CPathServerThread::GetNextLosRequest(void)
{
	u32 iLowestHandle = 0xFFFFFFFF;
	s32 iIndex = -1;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i;
	for(i=0; i<MAX_NUM_LOS_REQUESTS; i++)
	{
		CLineOfSightRequest * pLosRequest = &CPathServer::m_LineOfSightRequests[i];

		if(pLosRequest->m_bRequestPending &&
			pLosRequest->m_hHandle != 0 &&
			pLosRequest->m_hHandle < iLowestHandle)
		{
			iLowestHandle = pLosRequest->m_hHandle;
			iIndex = i;
		}
	}

	if(iIndex == -1)
	{
		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
		return NULL;
	}

	CPathServer::m_LineOfSightRequests[iIndex].m_bRequestPending = false;
	CPathServer::m_LineOfSightRequests[iIndex].m_bRequestActive = true;

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return &CPathServer::m_LineOfSightRequests[iIndex];
}

//****************************************************
// Get the next audio-properties request to process.
// Choose the one with the lowest non-zero handle
//****************************************************
CAudioRequest * CPathServerThread::GetNextAudioPropertiesRequest()
{
	u32 iLowestHandle = 0xFFFFFFFF;
	u32 iLowestHandlePriorityRequest = 0xFFFFFFFF;
	s32 iIndex = -1;
	s32 iIndexPriorityRequest = -1;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i;
	for(i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
	{
		CAudioRequest * pAudioRequest = &CPathServer::m_AudioRequests[i];

		if(pAudioRequest->m_bRequestPending && pAudioRequest->m_hHandle != 0)
		{
			// Find the lowest request
			if(pAudioRequest->m_hHandle < iLowestHandle)
			{
				iLowestHandle = pAudioRequest->m_hHandle;
				iIndex = i;
			}
			// Find the lowest priority request
			if(pAudioRequest->m_bPriorityRequest && pAudioRequest->m_hHandle < iLowestHandlePriorityRequest)
			{
				iLowestHandlePriorityRequest = pAudioRequest->m_hHandle;
				iIndexPriorityRequest = i;
			}
		}
	}

	// No pending requests found?
	if(iIndex == -1 && iIndexPriorityRequest == -1)
	{
		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
		return NULL;
	}

	// Deal with any priority request we may have found
	if(iIndexPriorityRequest != -1)
	{
		CPathServer::m_AudioRequests[iIndexPriorityRequest].m_bRequestPending = false;
		CPathServer::m_AudioRequests[iIndexPriorityRequest].m_bRequestActive = true;

		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

		return &CPathServer::m_AudioRequests[iIndexPriorityRequest];
	}
	// Otherwise it was a regular request
	else
	{
		CPathServer::m_AudioRequests[iIndex].m_bRequestPending = false;
		CPathServer::m_AudioRequests[iIndex].m_bRequestActive = true;

		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

		return &CPathServer::m_AudioRequests[iIndex];
	}
}

//****************************************************
// Get the next flood-fill request to process.
// Choose the one with the lowest non-zero handle
//****************************************************
CFloodFillRequest * CPathServerThread::GetNextFloodFillRequest(void)
{
	u32 iLowestHandle = 0xFFFFFFFF;
	s32 iIndex = -1;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i;
	for(i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
	{
		CFloodFillRequest * pFloodFillRequest = &CPathServer::m_FloodFillRequests[i];

		if(pFloodFillRequest->m_bRequestPending &&
			pFloodFillRequest->m_hHandle != 0 &&
			pFloodFillRequest->m_hHandle < iLowestHandle)
		{
			iLowestHandle = pFloodFillRequest->m_hHandle;
			iIndex = i;
		}
	}

	if(iIndex == -1)
	{
		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
		return NULL;
	}

	CPathServer::m_FloodFillRequests[iIndex].m_bRequestPending = false;
	CPathServer::m_FloodFillRequests[iIndex].m_bRequestActive = true;

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return &CPathServer::m_FloodFillRequests[iIndex];
}

//****************************************************
// Get the next clear-area request to process.
// Choose the one with the lowest non-zero handle
//****************************************************
CClearAreaRequest * CPathServerThread::GetNextClearAreaRequest(void)
{
	u32 iLowestHandle = 0xFFFFFFFF;
	s32 iIndex = -1;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i;
	for(i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
	{
		CClearAreaRequest * pClearAreaRequest = &CPathServer::m_ClearAreaRequests[i];

		if(pClearAreaRequest->m_bRequestPending &&
			pClearAreaRequest->m_hHandle != 0 &&
			pClearAreaRequest->m_hHandle < iLowestHandle)
		{
			iLowestHandle = pClearAreaRequest->m_hHandle;
			iIndex = i;
		}
	}

	if(iIndex == -1)
	{
		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
		return NULL;
	}

	CPathServer::m_ClearAreaRequests[iIndex].m_bRequestPending = false;
	CPathServer::m_ClearAreaRequests[iIndex].m_bRequestActive = true;

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return &CPathServer::m_ClearAreaRequests[iIndex];
}

//****************************************************
// Get the next closest-position request to process.
// Choose the one with the lowest non-zero handle
//****************************************************
CClosestPositionRequest * CPathServerThread::GetNextClosestPositionRequest(void)
{
	u32 iLowestHandle = 0xFFFFFFFF;
	s32 iIndex = -1;

	GTA_ENGINE_ONLY(LOCK_REQUESTS)

	int i;
	for(i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
	{
		CClosestPositionRequest * pClosestPositionRequest = &CPathServer::m_ClosestPositionRequests[i];

		if(pClosestPositionRequest->m_bRequestPending &&
			pClosestPositionRequest->m_hHandle != 0 &&
			pClosestPositionRequest->m_hHandle < iLowestHandle)
		{
			iLowestHandle = pClosestPositionRequest->m_hHandle;
			iIndex = i;
		}
	}

	if(iIndex == -1)
	{
		GTA_ENGINE_ONLY(UNLOCK_REQUESTS)
		return NULL;
	}

	CPathServer::m_ClosestPositionRequests[iIndex].m_bRequestPending = false;
	CPathServer::m_ClosestPositionRequests[iIndex].m_bRequestActive = true;

	GTA_ENGINE_ONLY(UNLOCK_REQUESTS)

	return &CPathServer::m_ClosestPositionRequests[iIndex];
}


TNavMeshAndPoly
CPathServerThread::GetClosestPolyBelowPoint(aiNavDomain domain, const Vector3 & vPos, float fMaxRadius, float fHeightAbove, float fHeightBelow, bool bOnlyOnPavement, bool bClosestToCentre)
{
	TNavMeshAndPoly meshAndPoly;
	meshAndPoly.Reset();
//	adjPoly.SetNavMeshIndex(NAVMESH_NAVMESH_INDEX_NONE);
//	adjPoly.SetPolyIndex(NAVMESH_POLY_INDEX_NONE);

	Vector3 vMin = vPos - Vector3(fMaxRadius, fMaxRadius, fHeightAbove);	// below (this needs to account for when standing on walls, etc)
	Vector3 vMax = vPos + Vector3(fMaxRadius, fMaxRadius, fHeightBelow);	// above

	// There may be up to 4 CNavMeshes which we need to examine, in the case
	// that we are straddling the corner of four meshes.
	// For now however, we'll just use the mesh directly below vPos.
	// NB : Improve this to query adjacent nav-meshes if needed.
	u32 iMeshIndex = CPathServer::GetNavMeshIndexFromPosition(vPos, domain);

	if(iMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
	{
		return meshAndPoly;
	}

	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iMeshIndex, domain);
	if(!pNavMesh)
	{
		return meshAndPoly;
	}

	u32 iPolyIndex = pNavMesh->GetPolyMostlyWithinVolume(vMin, vMax, bOnlyOnPavement, bClosestToCentre);
	if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
	{
		return meshAndPoly;
	}

	meshAndPoly.m_iNavMeshIndex = iMeshIndex;
	meshAndPoly.m_iPolyIndex = iPolyIndex;
	return meshAndPoly;
}

TNavMeshAndPoly
CPathServerThread::GetClosestPolyBelowPointInMultipleNavMeshes(aiNavDomain domain, const Vector3 & vPos, float fMaxRadius, float fHeightAbove, float fHeightBelow, bool bOnlyOnPavement, bool bClosestToCentre)
{
	Vector3 vMin = vPos - Vector3(fMaxRadius, fMaxRadius, fHeightAbove);	// below (this needs to account for when standing on walls, etc)
	Vector3 vMax = vPos + Vector3(fMaxRadius, fMaxRadius, fHeightBelow);	// above

	float fNavMeshSize = CPathServer::m_pNavMeshStores[domain]->GetMeshSize();

	u32 iNavMeshIndices[5];
	iNavMeshIndices[0] = CPathServer::GetNavMeshIndexFromPosition(vPos, domain);
	iNavMeshIndices[1] = CPathServer::GetNavMeshIndexFromPosition(vPos - Vector3(fNavMeshSize, 0.0f, 0.0f), domain);
	iNavMeshIndices[2] = CPathServer::GetNavMeshIndexFromPosition(vPos + Vector3(fNavMeshSize, 0.0f, 0.0f), domain);
	iNavMeshIndices[3] = CPathServer::GetNavMeshIndexFromPosition(vPos - Vector3(0.0f, fNavMeshSize, 0.0f), domain);
	iNavMeshIndices[4] = CPathServer::GetNavMeshIndexFromPosition(vPos + Vector3(0.0f, fNavMeshSize, 0.0f), domain);

	CNavMesh::ms_fGetPolyBestOverlap = 0.0f;
	CNavMesh::ms_iGetPolyBestIndex = NAVMESH_POLY_INDEX_NONE;
	CNavMesh::ms_bGetPolyOnlyOnPavement = bOnlyOnPavement;
	CNavMesh::ms_bGetPolyClosestToCentre = bClosestToCentre;
	CNavMesh::ms_vGetPolyVolumeCentre = (vMin + vMax) * 0.5f;
	CNavMesh::ms_fGetPolyLeastDistSqr = FLT_MAX;

	u32 iBestNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;

	for(s32 i=0; i<5; i++)
	{
		if(iNavMeshIndices[i] != NAVMESH_NAVMESH_INDEX_NONE)
		{
			CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iNavMeshIndices[i], domain);

			if(pNavMesh)
			{
				CNavMesh::ms_bGetPolyFoundAnyPoly = false;

				pNavMesh->GetPolyMostlyWithinVolumeR(pNavMesh->GetQuadTree(), vMin, vMax);

				if(CNavMesh::ms_bGetPolyFoundAnyPoly)
				{
					iBestNavMeshIndex = iNavMeshIndices[i];
				}
			}
		}
	}

	TNavMeshAndPoly meshAndPoly;
	meshAndPoly.m_iNavMeshIndex = iBestNavMeshIndex;
	meshAndPoly.m_iPolyIndex = CNavMesh::ms_iGetPolyBestIndex;
	return meshAndPoly;
}


EPathServerErrorCode
CPathServerThread::GetMultipleNavMeshPolysForRouteEndPoints(CPathRequest * pPathRequest, TNavMeshPoly * pStartingPoly, CNavMesh * pNavMesh, const Vector3 & vPos, float fRadius, atArray<TNavMeshPoly*> & navMeshPolys, aiNavDomain domain)
{
	Assert(pStartingPoly);

	IncTimeStamp();
	pStartingPoly->m_TimeStamp = m_iNavMeshPolyTimeStamp;

	static const float fVerticalDist = 3.0f;
	Vector3 vMin = vPos - Vector3(fRadius,fRadius,fVerticalDist);
	Vector3 vMax = vPos + Vector3(fRadius,fRadius,fVerticalDist);
	m_Vars.m_MultipleNavMeshPolysMinMax.SetFloat(vMin.x, vMin.y, vMin.z, vMax.x, vMax.y, vMax.z);

	FloodFillToGetRouteEndPolys(pPathRequest, pStartingPoly, pNavMesh, m_Vars.m_MultipleNavMeshPolysMinMax, navMeshPolys, domain);

	return PATH_NO_ERROR;
}

void CPathServerThread::FloodFillToGetRouteEndPolys(CPathRequest * pPathRequest, TNavMeshPoly * pStartingPoly, CNavMesh * pNavMesh, const TShortMinMax & regionMinMax, atArray<TNavMeshPoly*> & navMeshPolys, aiNavDomain domain)
{
	static const s32 MAX_NODES = 128;
	atBinHeap<float, TNavMeshPoly*> binHeap(MAX_NODES);

	pStartingPoly->m_TimeStamp = m_iNavMeshPolyTimeStamp;
	binHeap.Insert(0.0f, pStartingPoly);

	float fCurrentKey;
	TNavMeshPoly * pPoly;
	while( binHeap.ExtractMin(fCurrentKey, pPoly) )
	{
		pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);
		if(!pNavMesh)
			continue;

		for(u32 p=0; p<pPoly->GetNumVertices(); p++)
		{
			const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex() + p );
			const u32 iAdjNavMesh = adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes());
			if(iAdjNavMesh != NAVMESH_NAVMESH_INDEX_NONE &&
				adjPoly.GetPolyIndex() != NAVMESH_POLY_INDEX_NONE &&
				adjPoly.GetAdjacencyType() == ADJACENCY_TYPE_NORMAL)
			{
				CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(iAdjNavMesh, domain);
				if(pAdjNavMesh)
				{
					TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetPolyIndex());

					if(pAdjPoly->m_TimeStamp != m_iNavMeshPolyTimeStamp)
					{
						pAdjPoly->m_TimeStamp = m_iNavMeshPolyTimeStamp;

						if(regionMinMax.Intersects(pAdjPoly->m_MinMax))
						{
							if(!pAdjPoly->GetIsDegenerateConnectionPoly() && navMeshPolys.GetCount() < navMeshPolys.GetCapacity())
							{
								// If this poly is suitable as a starting polygon for this route, then add to our list

								if((pPathRequest->m_bNeverLeavePavements || pPathRequest->m_bPreferPavements) && !pAdjPoly->GetIsPavement())
								{

								}
								else if(pPathRequest->m_bNeverEnterWater && pAdjPoly->GetIsWater())
								{

								}
								else if(pPathRequest->m_bNeverLeaveWater && !pAdjPoly->GetIsWater())
								{

								}
								else
								{
									navMeshPolys.Append() = pAdjPoly;
								}

								if( binHeap.GetNodeCount() < binHeap.GetMaxSize() )
								{
									binHeap.Insert(fCurrentKey + 1.0f, pAdjPoly);
								}
							}
						}
					}
				}
			}
		}
	}
}

//*************************************************************************************************************************************
// fDistanceBelowPointToLook is a value by which to shift vPos downwards if no navmesh poly is found directly beneath the input point.
// This is because typically the positions passed into the pathfinder are at waist height.  If for example the ped was standing next
// to a table which had some navmesh polys on it, then if we didn't adjust the position downwards then we'd pick up the polys on the
// table instead of the ones nearby on the floor.
// If the fDistanceAbovePointToLook value is not 0.0f, then it will look up FIRST.  (for underwater pathfinding).

EPathServerErrorCode
CPathServerThread::GetNavMeshPolyForRouteEndPoints(
	const Vector3 & vPos,
	const TNavMeshAndPoly * pExistingNavMeshAndPoly,
	CNavMesh *& pNavMesh,
	TNavMeshPoly *& pPoly,
	Vector3 * pNewPos,
	const float fDistanceBelowPointToLook,
	const float fDistanceAbovePointToLook,
	const bool bCheckDynamicNavMeshes,
	const bool bSnapPointToNavMesh,	
	aiNavDomain domain,
	const float fMaxDistanceToAdjustEndPoint,
	const Vector3* pvPolySearchDir)
{
	// assuming that the input point is at a ped's origin, then foot position will be a metre below
	static const float fFootOffset = 1.0f;
	static const float fRaycastStartOffset = 1.0f;
	static const Vector3 vRaycastStartOffset = ZAXIS;

	if(pNewPos)
		*pNewPos = vPos;

	u32 iNavMeshIndex;

	if(pExistingNavMeshAndPoly && pExistingNavMeshAndPoly->m_iNavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE)
	{
		iNavMeshIndex = pExistingNavMeshAndPoly->m_iNavMeshIndex;
		pNavMesh = CPathServer::GetNavMeshFromIndex(iNavMeshIndex, domain);
	}
	else
	{
		iNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;
		
		if(bCheckDynamicNavMeshes)
		{
			Assert(pNavMesh->GetIsDynamic());
			iNavMeshIndex = pNavMesh->GetIndexOfMesh();
		}
		if(iNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
		{
			iNavMeshIndex = CPathServer::GetNavMeshIndexFromPosition(vPos, domain);
		}
		// Are coordinates outside of the map area ?
		if(iNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE)
		{
			return PATH_INVALID_COORDINATES;
		}

		pNavMesh = CPathServer::GetNavMeshFromIndex(iNavMeshIndex, domain);
	}

	// Coordinates are valid, but there's no navmesh loaded here
	if(!pNavMesh)
	{
		return PATH_NAVMESH_NOT_LOADED;
	}

	u32 iPolyIndex;
	Vector3 vIntersectPos;

	if(pExistingNavMeshAndPoly && pExistingNavMeshAndPoly->m_iPolyIndex != NAVMESH_POLY_INDEX_NONE &&
		pNavMesh->RayIntersectsPoly(
			Vector3(vPos.x, vPos.y, vPos.z + fRaycastStartOffset), Vector3(vPos.x, vPos.y, vPos.z - fDistanceBelowPointToLook),
			pNavMesh->GetPoly(pExistingNavMeshAndPoly->m_iPolyIndex), vIntersectPos) )
	{
		iPolyIndex = pExistingNavMeshAndPoly->m_iPolyIndex;

		if(pNewPos && bSnapPointToNavMesh)
			pNewPos->z = vIntersectPos.z;
	}
	else
	{
		iPolyIndex = NAVMESH_POLY_INDEX_NONE;

		// Optionally look upwards first
		if(fDistanceAbovePointToLook != 0.0f)
		{
			iPolyIndex = pNavMesh->GetPolyAbovePoint(vPos + vRaycastStartOffset, vIntersectPos, fDistanceAbovePointToLook);
		}
		// Can we get a polygon precisely underneath this location
		if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
		{
			iPolyIndex = pNavMesh->GetPolyBelowPoint(vPos + vRaycastStartOffset, vIntersectPos, fDistanceBelowPointToLook);
		}

		if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
		{
			// If we didn't find one exactly below, then try again with an approximation.
			// We'll try to find the closest polygon.  Use a position 1m below the input point, to favour the floor polys
			// over any polys which may be up on low walls, etc.
			if (fMaxDistanceToAdjustEndPoint > 0.0f) 
			{			
				const TNavMeshIndex iDynamicNavMeshIndex = pNavMesh->GetIsDynamic() ? pNavMesh->GetIndexOfMesh() : NAVMESH_NAVMESH_INDEX_NONE;
				TNavMeshAndPoly navMeshAndPoly;

				if(pNewPos)
				{
					FindApproxNavMeshPolyForRouteEndPointsAndAdjustPosition(vPos - Vector3(0, 0, fFootOffset), fMaxDistanceToAdjustEndPoint, navMeshAndPoly, false, *pNewPos, iDynamicNavMeshIndex, domain, pvPolySearchDir);
				}
				else
				{
					FindApproxNavMeshPolyForRouteEndPoints(vPos - Vector3(0, 0, fFootOffset), fMaxDistanceToAdjustEndPoint, navMeshAndPoly, false, iDynamicNavMeshIndex, domain, pvPolySearchDir);
				}

				if(navMeshAndPoly.m_iNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE || navMeshAndPoly.m_iPolyIndex == NAVMESH_POLY_INDEX_NONE)
				{
					return PATH_NO_SURFACE_AT_COORDINATES;
				}

				pNavMesh = CPathServer::GetNavMeshFromIndex(navMeshAndPoly.m_iNavMeshIndex, domain);
				iPolyIndex = navMeshAndPoly.m_iPolyIndex;
			}
			else 
			{
				return PATH_NO_SURFACE_AT_COORDINATES;
			}
		}
		else if(pNewPos && bSnapPointToNavMesh)
		{
			pNewPos->z = vIntersectPos.z;
		}
	}

	if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
		return PATH_NO_SURFACE_AT_COORDINATES;

	pPoly = pNavMesh->GetPoly(iPolyIndex);

	if(pPoly->GetIsDisabled())
		return PATH_NO_SURFACE_AT_COORDINATES;

	CPathServerThread::OnFirstVisitingPoly(pPoly);

	// If this navmesh poly has been tessellated & replaced by a polygon in the
	// tessellation navmesh, then we should check that navmesh for a candidate poly
	u32 iLastPolyIndex = NAVMESH_POLY_INDEX_NONE;
	while(pPoly->TestFlags(NAVMESHPOLY_REPLACED_BY_TESSELLATION))
	{
		pNavMesh = CPathServer::m_pTessellationNavMesh;
		iPolyIndex = pNavMesh->GetPolyBelowPoint(vPos + Vector3(0, 0, 1.0f), vIntersectPos, fDistanceBelowPointToLook);

		if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
		{
			if(fMaxDistanceToAdjustEndPoint == 0.0f)
			{
				return PATH_NO_SURFACE_AT_COORDINATES;
			}
			else
			{
				// If we didn't find one exactly below, then try again with an approximation.
				// We'll try to find the closest polygon.  Use a position 1m below the input point, to favour the floor polys
				// over any polys which may be up on low walls, etc.
				const TNavMeshIndex iDynamicNavMeshIndex = pNavMesh->GetIsDynamic() ? pNavMesh->GetIndexOfMesh() : NAVMESH_NAVMESH_INDEX_NONE;
				TNavMeshAndPoly meshAndPoly;

				if(pNewPos)
				{
					FindApproxNavMeshPolyForRouteEndPointsAndAdjustPosition(vPos - Vector3(0, 0, fFootOffset), fMaxDistanceToAdjustEndPoint, meshAndPoly, true, *pNewPos, iDynamicNavMeshIndex, domain);
				}
				else
				{
					FindApproxNavMeshPolyForRouteEndPoints(vPos - Vector3(0, 0, fFootOffset), fMaxDistanceToAdjustEndPoint, meshAndPoly, true, iDynamicNavMeshIndex, domain);
				}

				if(meshAndPoly.m_iNavMeshIndex == NAVMESH_NAVMESH_INDEX_NONE || meshAndPoly.m_iPolyIndex == NAVMESH_POLY_INDEX_NONE)
				{
					return PATH_NO_SURFACE_AT_COORDINATES;
				}

				pNavMesh = CPathServer::GetNavMeshFromIndex(meshAndPoly.m_iNavMeshIndex, domain);
				iPolyIndex = meshAndPoly.m_iPolyIndex;

				// Has the algorithm got stuck finding the same poly over & over ?  If so we'll return false..
				if(iPolyIndex == iLastPolyIndex && iLastPolyIndex != NAVMESH_POLY_INDEX_NONE)
				{
					Assertf(0, "CPathServer : Found an infinite loop in GetNavMeshPolyForRouteEndPoints()...");
					return PATH_NO_SURFACE_AT_COORDINATES;
				}

				iLastPolyIndex = iPolyIndex;
			}
		}
		else if(pNewPos && bSnapPointToNavMesh)
		{
			pNewPos->z = vIntersectPos.z;
		}

		pPoly = pNavMesh->GetPoly(iPolyIndex);

		if(pPoly->GetIsDisabled())
			return PATH_NO_SURFACE_AT_COORDINATES;
	}

	return PATH_NO_ERROR;
}


#define MAX_NUM_ENDPOINT_NAVMESHES		8

bool
CPathServerThread::FindApproxNavMeshPolyForRouteEndPoints(const Vector3 & vPos, float fRadius, TNavMeshAndPoly & closestPoly, bool bTessellationNavMesh, const TNavMeshIndex iThisDynamicNavMeshOnly, aiNavDomain domain, const Vector3* pvPolySearchDir)
{
	//***********************************************************************************************
	// We can have a number of potential navmeshes, if we are situated in the corner of a navmesh
	// Also we need room to be able to add intersecting dynamic navmeshes.
	// However, if a valid "iThisDynamicNavMeshOnly" is passed in we will ONLY look within this.

	u32 iNavMeshes[MAX_NUM_ENDPOINT_NAVMESHES];
	s32 iNumNavMeshes = 0;

	if(iThisDynamicNavMeshOnly != NAVMESH_NAVMESH_INDEX_NONE)
	{
		iNavMeshes[0] = iThisDynamicNavMeshOnly;
		iNumNavMeshes = 1;
	}
	else if(bTessellationNavMesh)
	{
		iNavMeshes[0] = NAVMESH_INDEX_TESSELLATION;
		iNumNavMeshes = 1;
	}
	else
	{
		iNavMeshes[0] = CPathServer::GetNavMeshIndexFromPosition(vPos, domain);
		iNumNavMeshes = 1;
	}

	Vector3 vPolyPts[NAVMESHPOLY_MAX_NUM_VERTICES];

	TShortMinMax minMax;
	minMax.SetFloat(vPos.x - fRadius, vPos.y - fRadius, vPos.z - fRadius, vPos.x + fRadius, vPos.y + fRadius, vPos.z + fRadius);

	s32 n;
	u32 p,v;
	float fDistSqr;
	float fMinDistSqr = FLT_MAX;
	float fRadiusSqr = fRadius * fRadius;
	Vector3 vDiff;
	closestPoly.Reset();

	for(n=0; n<iNumNavMeshes; n++)
	{
		CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iNavMeshes[n], domain);
		if(!pNavMesh)
			continue;

		if(pNavMesh->GetQuadTree())
		{
			Assert(!(pNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC));

			Vector3 vPosOnEdge;
			TNavMeshPoly * pPoly = pNavMesh->GetClosestNavMeshPolyEdge(vPos, fRadius, vPosOnEdge, NULL, NULL, false, 0, 0, NULL, pvPolySearchDir);
			if(pPoly)
			{			
				fDistSqr = (vPosOnEdge - vPos).Mag2();
				if(fDistSqr < fMinDistSqr)
				{
					fMinDistSqr = fDistSqr;
					closestPoly.m_iNavMeshIndex = iNavMeshes[n];
					closestPoly.m_iPolyIndex = pNavMesh->GetPolyIndex(pPoly);
				}
			}
		}
		else
		{
			for(p=0; p<pNavMesh->GetNumPolys(); p++)
			{
				TNavMeshPoly * pPoly = pNavMesh->GetPoly(p);

				CPathServerThread::OnFirstVisitingPoly(pPoly);

				// If this is the tessellation navmesh then we don't want to consider polys which
				// have been replaced by tessellation.  For other navmeshes this is ok, since we will
				// subsequently proceed to look in the tessellation navmesh for the nearest fragment
				if(bTessellationNavMesh && pPoly->GetReplacedByTessellation())
					continue;

				if(!minMax.Intersects(pPoly->m_MinMax))
					continue;

				for(v=0; v<pPoly->GetNumVertices(); v++)
				{
					pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), vPolyPts[v] );
				}

				s32 lastv = pPoly->GetNumVertices()-1;
				for(v=0; v<pPoly->GetNumVertices(); v++)
				{
					fDistSqr = geomDistances::Distance2SegToPoint(vPolyPts[lastv], vPolyPts[v] - vPolyPts[lastv], vPos);

					if(fDistSqr < fRadiusSqr && fDistSqr < fMinDistSqr)
					{
						fMinDistSqr = fDistSqr;
						closestPoly.m_iNavMeshIndex = iNavMeshes[n];
						closestPoly.m_iPolyIndex = p;
					}

					lastv = v;
				}
			}
		}
	}

	if(closestPoly.m_iNavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE && closestPoly.m_iPolyIndex != NAVMESH_POLY_INDEX_NONE)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool CPathServerThread::FindApproxNavMeshPolyForRouteEndPointsAndAdjustPosition(const Vector3 & vPos, float fRadius, TNavMeshAndPoly & closestPoly, bool bTessellationNavMesh, Vector3 & vNewPos, const TNavMeshIndex iThisDynamicNavMeshOnly, aiNavDomain domain, const Vector3* pvPolySearchDir)
{
	//***********************************************************************************************
	// We can have a number of potential navmeshes, if we are situated in the corner of a navmesh
	// Also we need room to be able to add intersecting dynamic navmeshes.
	// However, if a valid "iThisDynamicNavMeshOnly" is passed in we will ONLY look within this.

	u32 iNavMeshes[MAX_NUM_ENDPOINT_NAVMESHES];
	s32 iNumNavMeshes = 0;

	if(iThisDynamicNavMeshOnly != NAVMESH_NAVMESH_INDEX_NONE)
	{
		iNavMeshes[0] = iThisDynamicNavMeshOnly;
		iNumNavMeshes = 1;
	}
	else if(bTessellationNavMesh)
	{
		iNavMeshes[0] = NAVMESH_INDEX_TESSELLATION;
		iNumNavMeshes = 1;
	}
	else
	{
		iNumNavMeshes = CPathServer::GetNavMeshesIntersectingPosition(vPos, fRadius, &iNavMeshes[0], domain);
	}

	Vector3 vPolyPts[NAVMESHPOLY_MAX_NUM_VERTICES];

	TShortMinMax minMax;
	minMax.SetFloat(vPos.x - fRadius, vPos.y - fRadius, vPos.z - fRadius, vPos.x + fRadius, vPos.y + fRadius, vPos.z + fRadius);

	s32 n;
	u32 p,v;
	float fDistSqr;
	float fMinDistSqr = FLT_MAX;
	float fRadiusSqr = fRadius * fRadius;
	Vector3 vDiff;
	closestPoly.Reset();

	for(n=0; n<iNumNavMeshes; n++)
	{
		CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iNavMeshes[n], domain);
		if(!pNavMesh)
			continue;

		if(pNavMesh->GetQuadTree())
		{
			Assert(!(pNavMesh->GetFlags() & NAVMESH_IS_DYNAMIC));

			Vector3 vPosOnEdge;
			TNavMeshPoly * pPoly = pNavMesh->GetClosestNavMeshPolyEdge(vPos, fRadius, vPosOnEdge, NULL, NULL, false, 0, 0, NULL, pvPolySearchDir);
			if(pPoly)
			{
				fDistSqr = (vPosOnEdge - vPos).Mag2();
				if(fDistSqr < fMinDistSqr)
				{
					fMinDistSqr = fDistSqr;
					closestPoly.m_iNavMeshIndex = iNavMeshes[n];
					closestPoly.m_iPolyIndex = pNavMesh->GetPolyIndex(pPoly);

					vNewPos = vPosOnEdge;

#if !__FINAL && __BANK
					if (CPathServer::m_bVisualisePolygonRequests)
					{
						grcDebugDraw::Line(vNewPos, vNewPos + ZAXIS * 5.f, Color_green, 10);

						//Vector3 vPosCenter;
						//pNavMesh->GetPolyCentroidQuick(pPoly, vPosCenter);
						//grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_red, 10);
					}
#endif
				}
			}
		}
		else
		{
			for(p=0; p<pNavMesh->GetNumPolys(); p++)
			{
				TNavMeshPoly * pPoly = pNavMesh->GetPoly(p);
				if(pPoly->GetIsDisabled())
					continue;

				CPathServerThread::OnFirstVisitingPoly(pPoly);

				// If this is the tessellation navmesh then we don't want to consider polys which
				// have been replaced by tessellation.  For other navmeshes this is ok, since we will
				// subsequently proceed to look in the tessellation navmesh for the nearest fragment
				if(bTessellationNavMesh && pPoly->GetReplacedByTessellation())
					continue;

				if(!minMax.Intersects(pPoly->m_MinMax))
					continue;

				for(v=0; v<pPoly->GetNumVertices(); v++)
				{
					pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), vPolyPts[v] );
				}

				s32 lastv = pPoly->GetNumVertices()-1;
				for(v=0; v<pPoly->GetNumVertices(); v++)
				{
					fDistSqr = geomDistances::Distance2SegToPoint(vPolyPts[lastv], vPolyPts[v] - vPolyPts[lastv], vPos);

					if(fDistSqr < fRadiusSqr && fDistSqr < fMinDistSqr)
					{
						fMinDistSqr = fDistSqr;
						closestPoly.m_iNavMeshIndex = iNavMeshes[n];
						closestPoly.m_iPolyIndex = p;

						const Vector3 vDiff = vPolyPts[v] - vPolyPts[lastv];
						const float fTVal = (vDiff.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vPolyPts[lastv], vDiff, vPos) : 0.0f;
						vNewPos = vPolyPts[lastv] + ((vPolyPts[v] - vPolyPts[lastv]) * fTVal);
					}

					lastv = v;
				}
			}
		}
	}

	if(closestPoly.m_iNavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE && closestPoly.m_iPolyIndex != NAVMESH_POLY_INDEX_NONE)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CPathServerThread::ProcessLineOfSightRequest(CLineOfSightRequest * pLosRequest)
{
	Assert(pLosRequest->m_iNumPts >= 2);

	// Have to call this before we process any request which may use dynamic objects
	UpdateAllDynamicObjectsPositions();

	pLosRequest->m_bLineOfSightExists = true;

	u32 iLosFlags = 0;

	if(pLosRequest->m_fMaxAngle > 0.0f)
	{
		m_Vars.m_fMinSlopeDotProductWithUpVector = rage::Cosf(pLosRequest->m_fMaxAngle);
		iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_MAXSLOPE_BOUNDARY;
	}

	if ( pLosRequest->m_bNoLosAcrossWaterBoundary )
	{
		iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_WATER_BOUNDARY;
	}

	const aiNavDomain domain = pLosRequest->GetMeshDataSet();

	CNavMesh * pStartNavMesh, * pEndNavMesh;
	TNavMeshPoly * pStartPoly, * pEndPoly;

	const EPathServerErrorCode eStartRet = GetNavMeshPolyForRouteEndPoints(
		pLosRequest->m_vPoints[0],
		NULL,
		pStartNavMesh,
		pStartPoly,
		NULL,
		ms_fNormalDistBelowToLookForPoly, 0.0f,
		false,
		false,
		domain,
		0.0f
	);

	if(eStartRet != PATH_NO_ERROR)
	{
		pLosRequest->m_bLineOfSightExists = false;
		pLosRequest->m_bRequestActive = false;
		pLosRequest->m_bComplete = true;
		pLosRequest->m_iCompletionCode = eStartRet;
		return false;
	}

	// store if we are starting in water or not
	if ( pLosRequest->m_bStartsInWater )
	{
		if ( !pStartPoly || !pStartPoly->GetIsWater() )
		{
			pLosRequest->m_bLineOfSightExists = false;
			pLosRequest->m_bRequestActive = false;
			pLosRequest->m_bComplete = true;
			pLosRequest->m_iCompletionCode = eStartRet;
			return false;
		}
	}


	s32 iProgress;
	for(iProgress=1; iProgress<pLosRequest->m_iNumPts; iProgress++)
	{
		const EPathServerErrorCode eEndRet = GetNavMeshPolyForRouteEndPoints(
			pLosRequest->m_vPoints[iProgress],
			NULL,
			pEndNavMesh,
			pEndPoly,
			NULL,
			ms_fNormalDistBelowToLookForPoly, 0.0f,
			false, false, domain, 0.0f
		);

		if(eEndRet != PATH_NO_ERROR)
		{
			pLosRequest->m_bLineOfSightExists = false;
			pLosRequest->m_bRequestActive = false;
			pLosRequest->m_bComplete = true;
			pLosRequest->m_iCompletionCode = eEndRet;
			return false;
		}

		// NB : ?? Is this really needed ??
		pStartPoly->m_PathParentPoly = NULL;
		pEndPoly->m_PathParentPoly = NULL;

		const Vector3 & vStartPos = pLosRequest->m_vPoints[iProgress-1];
		const Vector3 & vEndPos = pLosRequest->m_vPoints[iProgress];

		// Increment timestamp before LOS.
		IncTimeStamp();

		bool bLosExists = TestNavMeshLOS(
			vStartPos,
			vEndPos,
			pEndPoly,
			pStartPoly,
			NULL,
			iLosFlags,
			domain
		);

		if(pLosRequest->m_bDynamicObjects && bLosExists)
		{
			bLosExists = TestDynamicObjectLOS(vStartPos, vEndPos);
		}

		pLosRequest->m_bLosResults[iProgress-1] = bLosExists;

		// This test can be made to quit out the first time that a LOS fails.  This will save us from processing the later segments.
		if(!bLosExists)
		{
			pLosRequest->m_bLineOfSightExists = false;

			if(pLosRequest->m_bQuitAtFirstLosFail)
			{
				while(iProgress < pLosRequest->m_iNumPts)
				{
					pLosRequest->m_bLosResults[iProgress] = false;
					iProgress++;
				}
				break;
			}
		}

		pStartPoly = pEndPoly; 
		pStartNavMesh = pEndNavMesh;
	}

	return true;
}

// Added these to help avoid possible recursion stack overflow
TShortMinMax g_AudioPropertiesMinMax;
CAudioRequest * g_pCurrentAudioRequest = NULL;

bool CPathServerThread::ProcessAudioPropertiesRequest(CAudioRequest * pAudioRequest)
{
#if !__FINAL
	if(CPathServer::m_bDisableAudioRequests)
	{
		pAudioRequest->m_iAudioProperties[0] = 0;
		pAudioRequest->m_fPolyAreas[pAudioRequest->m_iNumAudioProperties] = 1.0f;
		pAudioRequest->m_vPolyCentroids[0] = pAudioRequest->m_vPosition;

		pAudioRequest->m_iNumAudioProperties = 1;

		return true;
	}
#endif

	CNavMesh * pNavMesh = NULL;
	TNavMeshPoly * pPoly = NULL;
	EPathServerErrorCode eRet = PATH_NO_ERROR;

	const aiNavDomain domain = pAudioRequest->GetMeshDataSet();

	// If the issuing entity is being tracked in the navmesh, then its possible that we already know its
	// position to good degree of accuracy.  By using this we can avoid a costly search through navmesh polys.
	if(pAudioRequest->m_KnownNavmeshPosition.m_iNavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE &&
		pAudioRequest->m_KnownNavmeshPosition.m_iPolyIndex != NAVMESH_POLY_INDEX_NONE)
	{
		pNavMesh = CPathServer::GetNavMeshFromIndex(pAudioRequest->m_KnownNavmeshPosition.m_iNavMeshIndex, domain);
		if(pNavMesh && pAudioRequest->m_KnownNavmeshPosition.m_iPolyIndex < pNavMesh->GetNumPolys())
		{
			pPoly = pNavMesh->GetPoly(pAudioRequest->m_KnownNavmeshPosition.m_iPolyIndex);
		}
	}
	if(!pPoly)
	{
		eRet = GetNavMeshPolyForRouteEndPoints(
			pAudioRequest->m_vPosition,
			NULL,
			pNavMesh,
			pPoly,
			NULL,
			ms_fNormalDistBelowToLookForPoly, 0.0f,
			false, false, domain
		);
	}

	if( (eRet != PATH_NO_ERROR) || (!pNavMesh) || (!pPoly) || (pPoly->GetNavMeshIndex() != pNavMesh->GetIndexOfMesh()))
	{
		pAudioRequest->m_iNumAudioProperties = 0;
		pAudioRequest->m_bRequestActive = false;
		pAudioRequest->m_bComplete = true;
		pAudioRequest->m_iCompletionCode = eRet;
		return false;
	}

	IncAStarTimeStamp();
	pPoly->SetTimeStamp(m_iAStarTimeStamp);

	u8 iAudioFlags = 0;
	if(pPoly->GetIsSheltered())
		iAudioFlags |= CAudioRequest::FLAG_POLYGON_IS_SHELTERED;

	pAudioRequest->m_iNumAudioProperties = 1;
	pAudioRequest->m_iAudioProperties[0] = pPoly->GetAudioProperties();
	pAudioRequest->m_fPolyAreas[0] = pNavMesh->GetPolyArea(pNavMesh->GetPolyIndex(pPoly));
	pAudioRequest->m_iAdditionalFlags[0] = iAudioFlags;
	pNavMesh->GetPolyCentroidQuick(pPoly, pAudioRequest->m_vPolyCentroids[0]);

	TShortMinMax minMax;
	f32 radius = pAudioRequest->m_fRadius;
	// We behave differently if we have a direction - we do an angle-based search, but with an extra box around the centre, 
	// to catch nearby poly's who's centre's not in line, but who cover the segment we're interested in.
	if (pAudioRequest->m_vDirection.Mag2() > 0.25f)
	{
		radius = CPathServer::m_fAudioVelocityCubeSize;
	}
	radius += TShortMinMax::ms_fResolution;
	minMax.SetFloat(
		pAudioRequest->m_vPosition.x - radius,
		pAudioRequest->m_vPosition.y - radius,
		pAudioRequest->m_vPosition.z - radius,
		pAudioRequest->m_vPosition.x + radius,
		pAudioRequest->m_vPosition.y + radius,
		pAudioRequest->m_vPosition.z + radius
	);

	// See what sort of search we're doing.
	// No direction and -ve radius means return the one we're on, and all its neighbours - used for static things. TYPE 1.
	// No direction and a +ve radius means return anything within that radius - used for listener metrics. TYPE 2.
	// A direction and a radius means return anything within an angle of that direction, up to a distance of that radius. TYPE 3.

	bool directionPresent = (pAudioRequest->m_vDirection.Mag2() > 0.25f);
	bool useRadius = (pAudioRequest->m_fRadius > 0.0f);
	
	if(directionPresent && !useRadius)
	{
		pAudioRequest->m_iAudioReqType = CAudioRequest::DIRECTION_NO_RADIUS;
	}
	else if(!directionPresent && useRadius)
	{
		pAudioRequest->m_iAudioReqType = CAudioRequest::RADIUS_NO_DIRECTION;
	}
	else if(directionPresent && useRadius)
	{
		pAudioRequest->m_iAudioReqType = CAudioRequest::DIRECTION_AND_RADIUS;
	}
	else
	{
		pAudioRequest->m_iAudioReqType = CAudioRequest::NO_DIRECTION_NO_RADIUS;

		// No more work to do for this audio query type
		return true;
	}

	g_AudioPropertiesMinMax = minMax;
	g_pCurrentAudioRequest = pAudioRequest;

	ProcessAudioPropertiesRequestR(pNavMesh, pPoly, 1);

	g_pCurrentAudioRequest = NULL;

	return true;
}


const s32 sMaxNumRecursions = 24;

bool CPathServerThread::ProcessAudioPropertiesRequestR(CNavMesh * pNavMesh, TNavMeshPoly * pPoly, const s32 iNumRecursions)
{
	CAudioRequest * pAudioRequest = g_pCurrentAudioRequest;
	const aiNavDomain domain = pAudioRequest->GetMeshDataSet();

	u32 v;
	for(v=0; v<pPoly->GetNumVertices(); v++)
	{
		const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex() +  v);
		const u32 iAdjNavMesh = adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes());

		if(iAdjNavMesh==NAVMESH_NAVMESH_INDEX_NONE ||
			adjPoly.GetPolyIndex()==NAVMESH_POLY_INDEX_NONE ||
			adjPoly.GetAdjacencyType()!=ADJACENCY_TYPE_NORMAL)
			continue;

		CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(iAdjNavMesh, domain);

		Vector3 vCentroid;

		if(pAdjNavMesh)
		{
			Assert(pAdjNavMesh->GetIndexOfMesh() == iAdjNavMesh);

			TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetPolyIndex());

			if(pAdjPoly->GetTimeStamp() != m_iAStarTimeStamp)
			{
				pAdjPoly->SetTimeStamp(m_iAStarTimeStamp);

				u8 iAudioFlags = 0;
				if(pAdjPoly->GetIsSheltered())
					iAudioFlags |= CAudioRequest::FLAG_POLYGON_IS_SHELTERED;

				// TODO : If pAdjNavMesh is a *dynamic* navmesh, then we need to calculate this poly's m_MinMax now..

				switch(pAudioRequest->m_iAudioReqType)
				{
					case CAudioRequest::DIRECTION_NO_RADIUS:
					{
						pAdjNavMesh->GetPolyCentroidQuick(pAdjPoly, vCentroid);
						pAudioRequest->InsertAudioProperty((vCentroid-pAudioRequest->m_vPosition).Mag2(), pAdjPoly->GetAudioProperties(), vCentroid, pAdjNavMesh->GetPolyArea(pAdjNavMesh->GetPolyIndex(pAdjPoly)), iAudioFlags);

						break;
					}
					case CAudioRequest::RADIUS_NO_DIRECTION:
					{
						if (g_AudioPropertiesMinMax.Intersects(pAdjPoly->m_MinMax))
						{
							// We don't store results for wee polys - the worry is we want a decent size radius, but not too many results,
							// so try ignoring small ones.

							const f32 polyArea = pAdjNavMesh->GetPolyArea(pAdjNavMesh->GetPolyIndex(pAdjPoly));
							if (polyArea >= CPathServer::m_fAudioMinPolySizeForRadiusSearch)
							{
								Vector3 vEdgePoint;
								pAdjNavMesh->GetPolyClosestEdgePointToPoint(pAudioRequest->m_vPosition, pAdjPoly, vEdgePoint);
								if( (vEdgePoint - pAudioRequest->m_vPosition).Mag2() < pAudioRequest->m_fRadius*pAudioRequest->m_fRadius)
								{
									pAdjNavMesh->GetPolyCentroidQuick(pAdjPoly, vCentroid);
									pAudioRequest->InsertAudioProperty((vEdgePoint-pAudioRequest->m_vPosition).Mag2(), pAdjPoly->GetAudioProperties(), vCentroid, pAdjNavMesh->GetPolyArea(pAdjNavMesh->GetPolyIndex(pAdjPoly)), iAudioFlags);
								}
							}
							bool bContinue =
								(iNumRecursions >= sMaxNumRecursions) ?
									false : ProcessAudioPropertiesRequestR(pAdjNavMesh, pAdjPoly, iNumRecursions+1);
							if(!bContinue)
								return false;
						}
						break;
					}
					case CAudioRequest::DIRECTION_AND_RADIUS:
					{
						Vector3 polyCentroid;
						pAdjNavMesh->GetPolyCentroidQuick(pAdjPoly, polyCentroid);
						Vector3 startPositionToPoly = pAudioRequest->m_vPosition - polyCentroid;
						startPositionToPoly.NormalizeSafe();
						f32 dotProd = startPositionToPoly.Dot(pAudioRequest->m_vDirection);
						if ((dotProd > CPathServer::m_fAudioVelocityDotProduct) ||
							(dotProd > 0.0f && g_AudioPropertiesMinMax.Intersects(pAdjPoly->m_MinMax)))
						{
							pAdjNavMesh->GetPolyCentroidQuick(pAdjPoly, vCentroid);
							pAudioRequest->InsertAudioProperty((vCentroid-pAudioRequest->m_vPosition).Mag2(), pAdjPoly->GetAudioProperties(), vCentroid, pAdjNavMesh->GetPolyArea(pAdjNavMesh->GetPolyIndex(pAdjPoly)), iAudioFlags);

							bool bContinue =
								(iNumRecursions >= sMaxNumRecursions) ?
									false : ProcessAudioPropertiesRequestR(pAdjNavMesh, pAdjPoly, iNumRecursions+1);
							if(!bContinue)
								return false;
						}
						break;
					}
					default:
						Assert(false);
						break;
				}
			}
		}
	}

	return true;
}



bool CPathServerThread::ProcessFloodFillRequest(CFloodFillRequest * pFloodFillRequest)
{
	CNavMesh * pStartNavMesh = NULL;
	TNavMeshPoly * pStartPoly = NULL;

	const aiNavDomain domain = pFloodFillRequest->GetMeshDataSet();

	const EPathServerErrorCode eRet = GetNavMeshPolyForRouteEndPoints(
		pFloodFillRequest->m_vStartPos,
		NULL,
		pStartNavMesh,
		pStartPoly,
		NULL,
		ms_fNormalDistBelowToLookForPoly, 0.0f,
		false, false,
		domain
	);

	if(eRet != PATH_NO_ERROR || !pStartNavMesh || !pStartPoly)
	{
		pFloodFillRequest->m_bRequestActive = false;
		pFloodFillRequest->m_bComplete = true;
		pFloodFillRequest->m_iCompletionCode = eRet;
		return false;
	}

	Assertf(pStartNavMesh->GetIndexOfMesh() != NAVMESH_INDEX_TESSELLATION, "FloodFill - chose tessellation mesh as start!");

	IncAStarTimeStamp();

	TShortMinMax minMax;
	minMax.SetFloat(
		pFloodFillRequest->m_vStartPos.x - pFloodFillRequest->m_fMaxRadius,
		pFloodFillRequest->m_vStartPos.y - pFloodFillRequest->m_fMaxRadius,
		pFloodFillRequest->m_vStartPos.z - pFloodFillRequest->m_fMaxRadius,
		pFloodFillRequest->m_vStartPos.x + pFloodFillRequest->m_fMaxRadius,
		pFloodFillRequest->m_vStartPos.y + pFloodFillRequest->m_fMaxRadius,
		pFloodFillRequest->m_vStartPos.z + pFloodFillRequest->m_fMaxRadius
	);

	switch(pFloodFillRequest->m_FloodFillType)
	{
		case CFloodFillRequest::EAudioPropertiesFloodFill:
		{
			Assertf(0, "Not yet implemented!");
			return false;
		}
		case CFloodFillRequest::EFindClosestCarNodeFloodFill:
		{
			pFloodFillRequest->m_pShouldVisitPolyFn = FloodFill_FindClosestCarNode_ShouldVisitPolyFn;
			pFloodFillRequest->m_pShouldEndOnPolyFn = FloodFill_FindClosestCarNode_ShouldEndOnThisPolyFn;
			pFloodFillRequest->GetFindClosestCarNodeData()->m_bFoundCarNode = false;
			pFloodFillRequest->GetFindClosestCarNodeData()->m_fClosestCarNodeDistSqr = FLT_MAX;
			break;
		}
		case CFloodFillRequest::EFindClosestShelteredPolyFloodFill:
		{
			pFloodFillRequest->m_pShouldVisitPolyFn = FloodFill_FindClosestShelteredPoly_ShouldVisitPolyFn;
			pFloodFillRequest->m_pShouldEndOnPolyFn = FloodFill_FindClosestShelteredPoly_ShouldEndOnThisPolyFn;
			pFloodFillRequest->GetFindClosestShelteredPolyData()->m_bFound = false;
			pFloodFillRequest->GetFindClosestShelteredPolyData()->m_fClosestDistSqr = FLT_MAX;
			break;
		}
		case CFloodFillRequest::EFindClosestUnshelteredPolyFloodFill:
		{
			pFloodFillRequest->m_pShouldVisitPolyFn = FloodFill_FindClosestUnshelteredPoly_ShouldVisitPolyFn;
			pFloodFillRequest->m_pShouldEndOnPolyFn = FloodFill_FindClosestUnshelteredPoly_ShouldEndOnThisPolyFn;
			pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_startPolyIndex = pStartNavMesh->GetPolyIndex(pStartPoly);
			pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_startNavMeshIndex = pStartPoly->GetNavMeshIndex();
			pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_fClosestDistSqr = FLT_MAX;
			break;
		}
		case CFloodFillRequest::ECalcAreaUnderfoot:
		{
			pFloodFillRequest->m_pShouldVisitPolyFn = FloodFill_CalcAreaUnderfoot_ShouldVisitPolyFn;
			pFloodFillRequest->m_pShouldEndOnPolyFn = FloodFill_CalcAreaUnderfoot_ShouldEndOnThisPolyFn;
			pFloodFillRequest->GetCalcAreaUnderfootData()->m_fArea = 0.0f;
			break;
		}
		case CFloodFillRequest::EFindClosestPositionOnLand:
		{
			pFloodFillRequest->m_pShouldVisitPolyFn = FloodFill_FindClosestPositionOnLand_ShouldVisitPolyFn;
			pFloodFillRequest->m_pShouldEndOnPolyFn = FloodFill_FindClosestPositionOnLand_ShouldEndOnThisPolyFn;
			pFloodFillRequest->GetFindClosestPositionOnLandData()->m_bFound = false;
			pFloodFillRequest->GetFindClosestPositionOnLandData()->m_fClosestDistSqr = FLT_MAX;
			break;
		}
		case CFloodFillRequest::EFindNearbyPavementFloodFill:
		{
			pFloodFillRequest->m_pShouldVisitPolyFn = NULL;	// FloodFill_FindNearbyPavement_ShouldVisitPolyFn;
			pFloodFillRequest->m_pShouldEndOnPolyFn = FloodFill_FindNearbyPavementPoly_ShouldEndOnThisPolyFn;
			pFloodFillRequest->GetFindNearbyPavementPolyData()->m_bFound = false;
			break;
		}
		default:
		{
			Assert(0);
			return false;
		}
	}

	//****************************************************************************

	Vector3 vDirFromLast(0.0f,0.0f,0.0f);
	Vector3 vPolyCentroid;
	pStartNavMesh->GetPolyCentroidQuick(pStartPoly, vPolyCentroid);
	float fDistSqr = (vPolyCentroid - pFloodFillRequest->m_vStartPos).Mag2();

	m_PathSearchPriorityQueue->Clear();
	pStartPoly->m_AStarTimeStamp = m_iAStarTimeStamp;

	m_PathSearchPriorityQueue->Insert(fDistSqr, 0.0f, pStartPoly, vPolyCentroid, vDirFromLast, 0, NAVMESH_POLY_INDEX_NONE);

	//****************************************************************************

	float fParentCost, fDistanceTravelled;
	TNavMeshPoly * pPoly;
	CNavMesh * pNavMesh;

	u32 iBinHeapFlags = 0;
	u32 iTmpIndex;

	while(m_PathSearchPriorityQueue->RemoveTop(fParentCost, fDistanceTravelled, pPoly, m_Vars.m_vLastClosestPoint, m_Vars.m_vDirFromPrevious, iBinHeapFlags, iTmpIndex))
	{
		if(!pPoly)
			continue;

		Assertf(pPoly->GetNavMeshIndex() != NAVMESH_INDEX_TESSELLATION, "FloodFill - visiting poly in tessellation mesh");
		Assert(!pPoly->GetIsTessellatedFragment());

		if(CPathServer::m_bForceAbortCurrentRequest)
		{
			pFloodFillRequest->m_iCompletionCode = PATH_ABORTED_DUE_TO_ANOTHER_THREAD;
			return false;
		}
#if NAVMESH_OPTIMISATIONS_OFF
		Assert(!pPoly->GetReplacedByTessellation());
#endif
		if(pPoly->GetReplacedByTessellation())
			continue;

		pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);
		if(!pNavMesh)
			continue;

		Assertf(pNavMesh->GetIndexOfMesh()!=NAVMESH_INDEX_TESSELLATION, "FloodFill - poly belongs to tessellation mesh");

		if(!pFloodFillRequest->m_pShouldEndOnPolyFn || pFloodFillRequest->m_pShouldEndOnPolyFn(pPoly, pNavMesh, pFloodFillRequest, fDistanceTravelled))
		{
			return true;
		}

		for(u32 a=0; a<pPoly->GetNumVertices(); a++)
		{
			const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex() + a);
			const u32 iAdjNavMesh = adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes());

			Assertf(adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes())!=NAVMESH_INDEX_TESSELLATION, "FloodFill - TAdjPoly adjacency is tessellation mesh!");

			if(iAdjNavMesh!=NAVMESH_NAVMESH_INDEX_NONE && adjPoly.GetOriginalPolyIndex()!=NAVMESH_POLY_INDEX_NONE)
			{
				// Usage of climbs & drops can be enabled/disabled in the floodfill-request itself, or can be
				// handled specifically in the callback fn.
				if(!pFloodFillRequest->m_bUseClimbsAndDrops && (adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_CLIMB_LOW || adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_CLIMB_HIGH || adjPoly.GetAdjacencyType()==ADJACENCY_TYPE_DROPDOWN))
				{
					continue;
				}

				CNavMesh * pAdjNavMesh = CPathServer::GetNavMeshFromIndex(iAdjNavMesh, domain);
				if(pAdjNavMesh)
				{
					Assertf(pAdjNavMesh->GetIndexOfMesh()!=NAVMESH_INDEX_TESSELLATION, "adjacent navmesh is tessellation mesh");

					TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetOriginalPolyIndex());
					Assert(pAdjPoly);

					if(pAdjPoly && minMax.Intersects(pAdjPoly->m_MinMax) && pAdjPoly->m_AStarTimeStamp!=m_iAStarTimeStamp &&
						(!pFloodFillRequest->m_pShouldVisitPolyFn || pFloodFillRequest->m_pShouldVisitPolyFn(pAdjPoly, pAdjNavMesh, pPoly, pNavMesh, &adjPoly)))
					{
						pAdjNavMesh->GetPolyCentroidQuick(pAdjPoly, vPolyCentroid);

						if(!pFloodFillRequest->m_bConsiderDynamicObjects || TestDynamicObjectLOS(m_Vars.m_vLastClosestPoint, vPolyCentroid))
						{
							pAdjPoly->m_AStarTimeStamp = m_iAStarTimeStamp;
							fDistSqr = (vPolyCentroid - pFloodFillRequest->m_vStartPos).Mag2();
							fDistanceTravelled += (m_Vars.m_vLastClosestPoint - vPolyCentroid).Mag();
							m_PathSearchPriorityQueue->Insert(fDistSqr, fDistanceTravelled, pAdjPoly, vPolyCentroid, vDirFromLast, 0, NAVMESH_POLY_INDEX_NONE);
						}
					}
				}
			}
		}

		// Do ladders & other special-links here..
		if(pPoly->GetNumSpecialLinks()!=0)
		{
			const u32 iPolyIndex = pNavMesh->GetPolyIndex(pPoly);
			s32 iLinkLookupIndex = pPoly->GetSpecialLinksStartIndex();
			for(s32 s=0; s<pPoly->GetNumSpecialLinks(); s++)
			{
				const u16 iLinkIndex = pNavMesh->GetSpecialLinkIndex(iLinkLookupIndex++);
				Assert(iLinkIndex < pNavMesh->GetNumSpecialLinks());
				const CSpecialLinkInfo & linkInfo = pNavMesh->GetSpecialLinks()[iLinkIndex];

				if(linkInfo.GetOriginalLinkFromPoly() == iPolyIndex)
				{
					switch(linkInfo.GetLinkType())
					{
						default:
							Assert(false);
							break;
						case NAVMESH_LINKTYPE_CLIMB_LADDER:
						case NAVMESH_LINKTYPE_DESCEND_LADDER:
						{
							if(pFloodFillRequest->m_bUseLadders)
							{
								CNavMesh * pLinkToNavMesh = CPathServer::GetNavMeshFromIndex(linkInfo.GetOriginalLinkToNavMesh(), domain);
								if(pLinkToNavMesh)
								{
									TNavMeshPoly * pLinkToPoly = pLinkToNavMesh->GetPoly(linkInfo.GetOriginalLinkToPoly());

									if(minMax.Intersects(pLinkToPoly->m_MinMax) && pLinkToPoly->m_AStarTimeStamp!=m_iAStarTimeStamp &&
										(!pFloodFillRequest->m_pShouldVisitPolyFn || pFloodFillRequest->m_pShouldVisitPolyFn(pLinkToPoly, pLinkToNavMesh, pPoly, pNavMesh, NULL)))
									{
										pLinkToPoly->m_AStarTimeStamp = m_iAStarTimeStamp;

										pLinkToNavMesh->GetPolyCentroidQuick(pLinkToPoly, vPolyCentroid);
										fDistSqr = (vPolyCentroid - pFloodFillRequest->m_vStartPos).Mag2();
										fDistanceTravelled += (m_Vars.m_vLastClosestPoint - vPolyCentroid).Mag();
										m_PathSearchPriorityQueue->Insert(fDistSqr, fDistanceTravelled, pLinkToPoly, vPolyCentroid, vDirFromLast, 0, NAVMESH_POLY_INDEX_NONE);
									}
								}
							}
							break;
						}
						case NAVMESH_LINKTYPE_CLIMB_OBJECT:
						{
							if(pFloodFillRequest->m_bUseClimbsAndDrops)
							{
								CNavMesh * pLinkToNavMesh = CPathServer::GetNavMeshFromIndex(linkInfo.GetOriginalLinkToNavMesh(), domain);
								if(pLinkToNavMesh)
								{
									TNavMeshPoly * pLinkToPoly = pLinkToNavMesh->GetPoly(linkInfo.GetOriginalLinkToPoly());

									if(minMax.Intersects(pLinkToPoly->m_MinMax) && pLinkToPoly->m_AStarTimeStamp!=m_iAStarTimeStamp &&
										(!pFloodFillRequest->m_pShouldVisitPolyFn || pFloodFillRequest->m_pShouldVisitPolyFn(pLinkToPoly, pLinkToNavMesh, pPoly, pNavMesh, NULL)))
									{
										pLinkToPoly->m_AStarTimeStamp = m_iAStarTimeStamp;

										pLinkToNavMesh->GetPolyCentroidQuick(pLinkToPoly, vPolyCentroid);
										fDistSqr = (vPolyCentroid - pFloodFillRequest->m_vStartPos).Mag2();
										fDistanceTravelled += (m_Vars.m_vLastClosestPoint - vPolyCentroid).Mag();
										m_PathSearchPriorityQueue->Insert(fDistSqr, fDistanceTravelled, pLinkToPoly, vPolyCentroid, vDirFromLast, 0, NAVMESH_POLY_INDEX_NONE);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return true;
}

bool
CPathServerThread::FloodFill_FindClosestCarNode_ShouldVisitPolyFn(const TNavMeshPoly * UNUSED_PARAM(pAdjPoly), const CNavMesh * UNUSED_PARAM(pAdjNavMesh), const TNavMeshPoly * UNUSED_PARAM(pFromPoly), const CNavMesh * UNUSED_PARAM(pFromNavMesh), const TAdjPoly * UNUSED_PARAM(pAdjacency))
{
	return true;
}

bool
CPathServerThread::FloodFill_FindClosestCarNode_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float fDistanceTravelled)
{
	if(pAdjPoly->GetIsNearCarNode())
	{
		Vector3 vPolyCentroid;
		pAdjNavMesh->GetPolyCentroid(pAdjNavMesh->GetPolyIndex(pAdjPoly), vPolyCentroid);

		//float fDistSqr = (pFloodFillRequest->GetFindClosestCarNodeData()->GetCarNodePos() - pFloodFillRequest->m_vStartPos).Mag2();
		const float fDistSqr = fDistanceTravelled*fDistanceTravelled;
		if(fDistSqr < pFloodFillRequest->GetFindClosestCarNodeData()->m_fClosestCarNodeDistSqr)
		{
			pFloodFillRequest->GetFindClosestCarNodeData()->m_fClosestCarNodeDistSqr = fDistSqr;
			pFloodFillRequest->GetFindClosestCarNodeData()->SetCarNodePos(vPolyCentroid);
			pFloodFillRequest->GetFindClosestCarNodeData()->m_bFoundCarNode = true;
		}
	}

	// Never terminate this search early - we want to ensure we find the closest node by an exhaustive search
	return false;
}


bool
CPathServerThread::FloodFill_FindClosestShelteredPoly_ShouldVisitPolyFn(const TNavMeshPoly * UNUSED_PARAM(pAdjPoly), const CNavMesh * UNUSED_PARAM(pAdjNavMesh), const TNavMeshPoly * UNUSED_PARAM(pFromPoly), const CNavMesh * UNUSED_PARAM(pFromNavMesh), const TAdjPoly * pAdjacency)
{
	// When looking for sheltered polys don't use climbs/drops
	if(pAdjacency->GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
		return true;
	return false;
}

bool
CPathServerThread::FloodFill_FindClosestShelteredPoly_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float UNUSED_PARAM(fDistanceTravelled))
{
	if(pAdjPoly->GetIsSheltered())
	{
		Vector3 vPolyCentroid;
		pAdjNavMesh->GetPolyCentroid(pAdjNavMesh->GetPolyIndex(pAdjPoly), vPolyCentroid);

		float fDistSqr = (pFloodFillRequest->GetFindClosestShelteredPolyData()->GetPos() - pFloodFillRequest->m_vStartPos).Mag2();
		if(fDistSqr < pFloodFillRequest->GetFindClosestShelteredPolyData()->m_fClosestDistSqr)
		{
			pFloodFillRequest->GetFindClosestShelteredPolyData()->m_fClosestDistSqr = fDistSqr;
			pFloodFillRequest->GetFindClosestShelteredPolyData()->SetPos(vPolyCentroid);
			pFloodFillRequest->GetFindClosestShelteredPolyData()->m_bFound = true;
		}
	}

	// Never terminate this search early - we want to ensure we find the closest node by an exhaustive search
	return false;
}

bool
CPathServerThread::FloodFill_FindClosestUnshelteredPoly_ShouldVisitPolyFn(const TNavMeshPoly * UNUSED_PARAM(pAdjPoly), const CNavMesh * UNUSED_PARAM(pAdjNavMesh), const TNavMeshPoly * UNUSED_PARAM(pFromPoly), const CNavMesh * UNUSED_PARAM(pFromNavMesh), const TAdjPoly * pAdjacency)
{
	// When looking for sheltered polys don't use climbs/drops
	if(pAdjacency->GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
		return true;
	return false;
}

bool
CPathServerThread::FloodFill_FindClosestUnshelteredPoly_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float UNUSED_PARAM(fDistanceTravelled))
{
	if(!pAdjPoly->GetIsSheltered())
	{
		// special case - if the start poly is unsheltered then we're good to end immediately
		if(pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_startNavMeshIndex == pAdjPoly->GetNavMeshIndex() && pAdjNavMesh->GetPolyIndex(pAdjPoly) == pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_startPolyIndex)
		{
			pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_fClosestDistSqr = 0.f;
			return true;
		}
		else
		{
			Vector3 vPolyCentroid;
			pAdjNavMesh->GetPolyCentroid(pAdjNavMesh->GetPolyIndex(pAdjPoly), vPolyCentroid);
			float fDistSqr = (vPolyCentroid - pFloodFillRequest->m_vStartPos).Mag2();
			pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_fClosestDistSqr = Min(fDistSqr, pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_fClosestDistSqr);
			if(pFloodFillRequest->GetFindClosestUnshelteredPolyData()->m_fClosestDistSqr <= 9.0f)
			{
				// 3m is close enough...
				return true;
			}
		}
	}

	return false;
}


bool
CPathServerThread::FloodFill_CalcAreaUnderfoot_ShouldVisitPolyFn(const TNavMeshPoly * UNUSED_PARAM(pAdjPoly), const CNavMesh * UNUSED_PARAM(pAdjNavMesh), const TNavMeshPoly * UNUSED_PARAM(pFromPoly), const CNavMesh * UNUSED_PARAM(pFromNavMesh), const TAdjPoly * pAdjacency)
{
	// When looking for sheltered polys don't use climbs/drops
	if(pAdjacency->GetAdjacencyType()==ADJACENCY_TYPE_NORMAL)
		return true;
	return false;
}

bool
CPathServerThread::FloodFill_CalcAreaUnderfoot_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float UNUSED_PARAM(fDistanceTravelled))
{
	const float fPolyArea = pAdjNavMesh->GetPolyArea( pAdjNavMesh->GetPolyIndex(pAdjPoly) );
#ifdef GTA_ENGINE
	Assert(rage::FPIsFinite(fPolyArea));
#endif
	pFloodFillRequest->GetCalcAreaUnderfootData()->m_fArea += fPolyArea;
	return false;
}

bool
CPathServerThread::FloodFill_FindClosestPositionOnLand_ShouldVisitPolyFn(const TNavMeshPoly * UNUSED_PARAM(pAdjPoly), const CNavMesh * UNUSED_PARAM(pAdjNavMesh), const TNavMeshPoly * UNUSED_PARAM(pFromPoly), const CNavMesh * UNUSED_PARAM(pFromNavMesh), const TAdjPoly * pAdjacency)
{
	if(pAdjacency && pAdjacency->GetAdjacencyType()!=ADJACENCY_TYPE_NORMAL && pAdjacency->GetAdjacencyDisabled())
		return false;
	return true;
}

bool
CPathServerThread::FloodFill_FindClosestPositionOnLand_ShouldEndOnThisPolyFn(TNavMeshPoly * pAdjPoly, CNavMesh * pAdjNavMesh, CFloodFillRequest * pFloodFillRequest, const float UNUSED_PARAM(fDistanceTravelled))
{
	if(!pAdjPoly->GetIsWater() && !pAdjPoly->GetIsIsolated())
	{
		Vector3 vPolyCentroid;
		pAdjNavMesh->GetPolyCentroid(pAdjNavMesh->GetPolyIndex(pAdjPoly), vPolyCentroid);

		const Vector3 vDiff = pFloodFillRequest->GetFindClosestPositionOnLandData()->GetPos() - pFloodFillRequest->m_vStartPos;
		const float fDistSqr = vDiff.Mag2();

		if(fDistSqr < pFloodFillRequest->GetFindClosestPositionOnLandData()->m_fClosestDistSqr)
		{
			pFloodFillRequest->GetFindClosestPositionOnLandData()->m_fClosestDistSqr = fDistSqr;
			pFloodFillRequest->GetFindClosestPositionOnLandData()->SetPos(vPolyCentroid);
			pFloodFillRequest->GetFindClosestPositionOnLandData()->m_bFound = true;
		}
	}
	// Never end the search early, we want to make sure we find the closest place to get out
	return false;
}

bool 
CPathServerThread::FloodFill_FindNearbyPavement_ShouldVisitPolyFn (const TNavMeshPoly * UNUSED_PARAM(pAdjPoly), const CNavMesh * UNUSED_PARAM(pAdjNavMesh), const TNavMeshPoly * UNUSED_PARAM(pFromPoly), const CNavMesh * UNUSED_PARAM(pFromNavMesh), const TAdjPoly * UNUSED_PARAM(pAdjacency))
{
	return true;
}

bool 
CPathServerThread::FloodFill_FindNearbyPavementPoly_ShouldEndOnThisPolyFn (TNavMeshPoly * pAdjPoly, CNavMesh * UNUSED_PARAM(pAdjNavMesh), CFloodFillRequest * pFloodFillRequest, const float UNUSED_PARAM(fDistanceTravelled))
{
	if (pAdjPoly->GetIsPavement())
	{
		pFloodFillRequest->GetFindNearbyPavementPolyData()->m_bFound = true;
		return true;
	}
	return false;
}


int ClearArea_SortPolyListCB(const TNavProcessClearAreaEntry * p1, const TNavProcessClearAreaEntry * p2)
{
	return (p1->m_fDistSqr < p2->m_fDistSqr) ? -1 : 1;
}

bool CPathServerThread::ProcessClearAreaRequest(CClearAreaRequest * pClearAreaRequest)
{
#ifdef GTA_ENGINE
	const Vector3 vAreaMin = pClearAreaRequest->m_vSearchOrigin - Vector3(pClearAreaRequest->m_fSearchRadiusXY, pClearAreaRequest->m_fSearchRadiusXY, pClearAreaRequest->m_fSearchDistZ);
	const Vector3 vAreaMax = pClearAreaRequest->m_vSearchOrigin + Vector3(pClearAreaRequest->m_fSearchRadiusXY, pClearAreaRequest->m_fSearchRadiusXY, pClearAreaRequest->m_fSearchDistZ);
	TShortMinMax areaMinMax;
	areaMinMax.Set(vAreaMin, vAreaMax);

	const aiNavDomain domain = pClearAreaRequest->GetMeshDataSet();
	aiNavMeshStore * pStore = CPathServer::GetNavMeshStore(domain);
	atArray<s32> loadedMeshes;

	LOCK_STORE_LOADED_MESHES
	{
		const atArray<s32> & storeLoadedMeshes = pStore->GetLoadedMeshes();
		loadedMeshes.CopyFrom( storeLoadedMeshes.GetElements(), (u16) storeLoadedMeshes.GetCount() );
	}
	UNLOCK_STORE_LOADED_MESHES

	static const u32 iMaxPolys = 4096;
	atArray<TNavProcessClearAreaEntry> sortedPolys;
	sortedPolys.Reserve(iMaxPolys);

	Vector3 vPolyCentroid;
	Vector2 vMeshMin, vMeshMax;
	s32 iNavMesh;

	const float fMinDistSqr = pClearAreaRequest->m_fMinimumDistanceFromOrigin * pClearAreaRequest->m_fMinimumDistanceFromOrigin;

	for(int m=0; m<loadedMeshes.GetCount() && sortedPolys.GetCount()<sortedPolys.GetCapacity(); m++)
	{
		iNavMesh = loadedMeshes[m];
		if(iNavMesh == NAVMESH_NAVMESH_INDEX_NONE)
			continue;

		CPathServer::GetNavMeshExtentsFromIndex(iNavMesh, vMeshMin, vMeshMax, domain);
		if(vMeshMax.x < vAreaMin.x || vMeshMax.y < vAreaMin.y || vAreaMax.x < vMeshMin.x || vAreaMax.y < vAreaMin.y)
			continue;

		CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iNavMesh, domain);
		if(!pNavMesh)
			continue;

		for(u32 p=0; p<pNavMesh->GetNumPolys(); p++)
		{
			TNavMeshPoly * pPoly = pNavMesh->GetPoly(p);
			if(!pPoly->m_MinMax.Intersects(areaMinMax))
				continue;

			if(pPoly->GetIsWater() && !pClearAreaRequest->m_bConsiderWater)
				continue;
			if(pPoly->GetIsInterior() && !pClearAreaRequest->m_bConsiderInterior)
				continue;
			if(!pPoly->GetIsInterior() && !pClearAreaRequest->m_bConsiderExterior)
				continue;
			if(pPoly->GetIsSheltered() && !pClearAreaRequest->m_bConsiderSheltered)
				continue;

			pNavMesh->GetPolyCentroidQuick(pPoly, vPolyCentroid);

			// Test optional minimum distance which center of area must be from the supplied origin.  (url:bugstar:738465)
			if(fMinDistSqr > 0.0f)
			{
				if( (vPolyCentroid - pClearAreaRequest->m_vSearchOrigin).XYMag2() < fMinDistSqr )
					continue;
			}

			sortedPolys.PushAndGrow( TNavProcessClearAreaEntry(pPoly, (vPolyCentroid-pClearAreaRequest->m_vSearchOrigin).Mag2()) );
			if(sortedPolys.GetCount()==sortedPolys.GetCapacity())
				break;
		}
	}

	// Sort the poly list by distance from the search origin

	sortedPolys.QSort(0, -1, ClearArea_SortPolyListCB);

	//----------------------------------
	// Now process each polygon

	// Calculate the test directions
	// TODO: increase the number of tests based upon the desired clear area
	int iNumTestDirections = 8;
	Vector2 vTestDirections[8];
	Vector3 vIsectPositions[8];
	TNavMeshPoly * pEndPolys[8];

	const float fInc = (360.0f / ((float)iNumTestDirections)) * DtoR;
	const Vector2 vOriginal(pClearAreaRequest->m_fDesiredClearAreaRadius, 0.0f);
	float fRot = 0.0f;
	for(int i=0; i<iNumTestDirections; i++)
	{
		vTestDirections[i] = vOriginal;
		vTestDirections[i].Rotate(fRot);
		fRot += fInc;
	}

	u32 iLosFlags = 0;
	if(pClearAreaRequest->m_bConsiderInterior != pClearAreaRequest->m_bConsiderExterior)
		iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_ACROSS_INTERIOR_EXTERIOR_BOUNDARY;
	if(!pClearAreaRequest->m_bConsiderSheltered)
		iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_ACROSS_SHELTERED_BOUNDARY;
	if(!pClearAreaRequest->m_bConsiderWater)
		iLosFlags |= CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_WATER_BOUNDARY;

	Vector3 vIsectPos;
	static dev_u32 iNumPolysToProcessBeforeYield = 256;
	u32 iYieldCounter = iNumPolysToProcessBeforeYield;

	for(u32 p=0; p<sortedPolys.GetCount(); p++, iYieldCounter--)
	{
		// Yield regularly, since this can be a pretty intensive search
		if(CPathServer::m_bSleepPathServerThreadOnLongPathRequests && iYieldCounter==0)
		{
#if !__FINAL
			m_RequestProcessingOverallTimer->Stop();
#endif
			// give up the rest of our timeslice
			sysIpcYield(0);
			iYieldCounter = iNumPolysToProcessBeforeYield;

#if !__FINAL
			m_RequestProcessingOverallTimer->Start();
			pClearAreaRequest->m_iNumTimesSlept++;
#endif
		}

		// Enable streaming to abort this request instantly or we may incur a stall
		if(CPathServer::m_bForceAbortCurrentRequest)
		{
			pClearAreaRequest->m_iCompletionCode = PATH_ABORTED_DUE_TO_ANOTHER_THREAD;
			return false;
		}

		TNavMeshPoly * pPoly = sortedPolys[p].m_pPoly;
		CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);

		// NB: Do we want to calculate this again, or should we have cached it in TNavProcessClearAreaEntry?
		pNavMesh->GetPolyCentroidQuick(pPoly, vPolyCentroid);

		// TODO: Opportunity to early out on this polygon if the space around it's vertices/edges violates our desired radius constraint?
		// Test line-of-sight from polygon centroid out to the maximum radius, in a number of directions
		int t;
		for(t=0; t<iNumTestDirections; t++)
		{
			IncTimeStamp();

			TNavMeshPoly * pEndPoly = TestNavMeshLOS(vPolyCentroid, vTestDirections[t], &vIsectPositions[t], pPoly, iLosFlags, domain);

			if(pEndPoly && pClearAreaRequest->m_bConsiderDynamicObjects)
				if(!TestDynamicObjectLOS(vPolyCentroid, vIsectPositions[t]))
					pEndPoly = NULL;

#if __BANK && DEBUG_DRAW
			if(CPathServer::m_iVisualisePathServerInfo == CPathServer::PathServerVis_ClearAreaRequests)
				grcDebugDraw::Line(vPolyCentroid, vPolyCentroid+Vector3(vTestDirections[t].x,vTestDirections[t].y,0.0f), pEndPoly ? Color_green:Color_red, 10);
#endif
			if(!pEndPoly)
				break;

			pEndPolys[t] = pEndPoly;
		}
		// If any LOS failed then proceed to next polygon
		if(t != iNumTestDirections)
			continue;

		// Test line-of-sight in a circle at the maximum radius
		for(t=0; t<iNumTestDirections; t++)
		{
			IncTimeStamp();

			const Vector2 vTestDir( vIsectPositions[(t+1)%iNumTestDirections] - vIsectPositions[t], Vector2::kXY );
			TNavMeshPoly * pEndPoly = TestNavMeshLOS(vIsectPositions[t], vTestDir, &vIsectPos, pEndPolys[t], iLosFlags, domain);

			if(pEndPoly && pClearAreaRequest->m_bConsiderDynamicObjects)
				if(!TestDynamicObjectLOS(vIsectPositions[t], vIsectPos))
					pEndPoly = NULL;

#if __BANK && DEBUG_DRAW
			if(CPathServer::m_iVisualisePathServerInfo == CPathServer::PathServerVis_ClearAreaRequests)
				grcDebugDraw::Line(vIsectPositions[t], vIsectPositions[t]+Vector3(vTestDir.x,vTestDir.y,0.0f), pEndPoly ? Color_green:Color_red, 10);
#endif
			if(!pEndPoly)
				break;
		}
		// If any LOS failed then proceed to next polygon
		if(t != iNumTestDirections)
			continue;

		// This polygon has passed the area test, so we can use its centroid as the result of our clear-area search
		pClearAreaRequest->m_vResultOrigin = vPolyCentroid;
		pClearAreaRequest->m_iCompletionCode = PATH_FOUND;
		return true;
	}

#endif // GTA_ENGINE

	pClearAreaRequest->m_iCompletionCode = PATH_NOT_FOUND;
	return false;
}




// Called prior to visiting any polygon during ProcessClosestPositionRequest()
bool CPathServerThread::ClosestPositionRequest_PerPolyCallback(const CNavMesh * UNUSED_PARAM(pNavMesh), const TNavMeshPoly * pPoly)
{
	CClosestPositionRequest * pRequest = CPathServer::GetPathServerThread()->m_pCurrentActiveClosestPositionRequest;
	Assert(pRequest);

	if(!pRequest->m_bConsiderInterior && pPoly->GetIsInterior()) 
		return false;
	if(!pRequest->m_bConsiderExterior && !pPoly->GetIsInterior()) 
		return false;
	if(pRequest->m_bConsiderOnlyLand && pPoly->GetIsWater())
		return false;
	if(pRequest->m_bConsiderOnlyWater && !pPoly->GetIsWater())
		return false;
	if(pRequest->m_bConsiderOnlyPavement && !pPoly->GetIsPavement())
		return false;
	if(pRequest->m_bConsiderOnlyNonIsolated && pPoly->GetIsIsolated())
		return false;
	if(pRequest->m_bConsiderOnlySheltered && !pPoly->GetIsSheltered())
		return false;
	if(pRequest->m_bConsiderOnlyNetworkSpawn && !pPoly->GetIsNetworkSpawnCandidate())
		return false;

	if(pRequest->m_bConsiderDynamicObjects)
	{
		CPathServerThread::ms_dynamicObjectsIntersectingPolygons.clear();
		CDynamicObjectsContainer::GetObjectsIntersectingRegion(pPoly->m_MinMax, CPathServerThread::ms_dynamicObjectsIntersectingPolygons);
	}

	return true;
}
// Called prior to visiting each point-in-polygon during ProcessClosestPositionRequest()
float CPathServerThread::ClosestPositionRequest_PerPointCallback(const CNavMesh * UNUSED_PARAM(pNavMesh), const TNavMeshPoly * UNUSED_PARAM(pPoly), const Vector3 & vPos)
{
	CClosestPositionRequest * pRequest = CPathServer::GetPathServerThread()->m_pCurrentActiveClosestPositionRequest;
	Assert(pRequest);

	// Calculate sqr distance from sphere origin
	Vector3 vDiff = pRequest->m_vSearchOrigin - vPos;
	float fScore = vDiff.Mag2();
	// If we're outside of the search range, then reject
	if(fScore > pRequest->m_fSearchRadius*pRequest->m_fSearchRadius)
	{
		return FLT_MAX;
	}

	// Check avoid apheres
	for(s32 s=0; s<pRequest->m_iNumAvoidSpheres; s++)
	{
		if(pRequest->m_AvoidSpheres[s].ContainsPoint( VECTOR3_TO_VEC3V(vPos) ) )
		{
			return FLT_MAX;
		}
	}

	// Check dynamic objects
	if(pRequest->m_bConsiderDynamicObjects)
	{
		for(s32 o=0; o<ms_dynamicObjectsIntersectingPolygons.GetCount(); o++)
		{
			TDynamicObject * pObj = ms_dynamicObjectsIntersectingPolygons[o];

			// See if we are inside this object at all

			const float* fDynObjPlaneEpsilon = GetDynObjPlaneEpsilon(pObj);

			s32 p;
			for(p=0; p<6; p++)
			{
				const float fEdgePlaneDist = (pObj->m_Bounds.m_vEdgePlaneNormals[p].Dot3(vPos) + pObj->m_Bounds.m_vEdgePlaneNormals[p].w) + fDynObjPlaneEpsilon[p];
				// If we're outside of any edge plane, then we cannot be within the object at all
				if(fEdgePlaneDist > PATHSERVER_PED_RADIUS)
				{
					break;
				}
			}
			if(p == 6)
			{
				return FLT_MAX;
			}
		}
	}

	// Check spacing constraints
	if(pRequest->m_fMinimumSpacing > 0.0f)
	{
		for(s32 s=0; s<pRequest->m_Results.m_iNumResults; s++)
		{
			if((pRequest->m_Results.m_vResults[s] - vPos).Mag2() < pRequest->m_fMinimumSpacing*pRequest->m_fMinimumSpacing)
				return FLT_MAX;
		}
	}

	// Handle optionally Z-weighting above/below the search origin
	if(vDiff.z < 0.0f)		// point is above search origin ?
	{
		if(pRequest->m_fZWeightingAbove != 1.0f)
		{
			vDiff.z *= pRequest->m_fZWeightingAbove;
			fScore = vDiff.Mag2();
		}
	}
	else	// point is at or below search origin ?
	{
		if(pRequest->m_fZWeightingAtOrBelow != 1.0f)
		{
			vDiff.z *= pRequest->m_fZWeightingAtOrBelow;
			fScore = vDiff.Mag2();
		}
	}

	// NB: Do we want to take the Sqrt() of fScore now, or are we happy using squared distances?

	//---------------------
	// Insert result

	if(!pRequest->m_Results.m_iNumResults)
	{
		// If we have no results yet, then add as first member
		pRequest->m_Results.m_vResults[0] = vPos;
		pRequest->m_Results.m_vResults[0].w = fScore;
		pRequest->m_Results.m_iNumResults++;
	}
	else
	{
		// If results buffer is full, and score is greater than last item's score - just return
		if(pRequest->m_Results.m_iNumResults == pRequest->m_iMaxResults &&
			fScore >= pRequest->m_Results.m_vResults[pRequest->m_Results.m_iNumResults-1].w)
			return fScore;

		// Otherwise we insert the result somewhere along the list
		s32 r;
		for(r=0; r<pRequest->m_Results.m_iNumResults; r++)
		{
			if(fScore < pRequest->m_Results.m_vResults[r].w)
			{
				s32 end = Min(pRequest->m_Results.m_iNumResults, pRequest->m_iMaxResults-1);
				for(s32 j=end; j>r; j--)
				{
					pRequest->m_Results.m_vResults[j] = pRequest->m_Results.m_vResults[j-1];
				}

				pRequest->m_Results.m_vResults[r] = vPos;
				pRequest->m_Results.m_vResults[r].w = fScore;

				pRequest->m_Results.m_iNumResults++;
				pRequest->m_Results.m_iNumResults = Min(pRequest->m_Results.m_iNumResults, pRequest->m_iMaxResults);
				return fScore;
			}
		}
		if(r < pRequest->m_iMaxResults)
		{
			// Add result at the end of the list
			pRequest->m_Results.m_vResults[pRequest->m_Results.m_iNumResults] = vPos;
			pRequest->m_Results.m_vResults[pRequest->m_Results.m_iNumResults].w = fScore;
			pRequest->m_Results.m_iNumResults++;
			return fScore;
		}
	}

	return fScore;
}

bool CPathServerThread::ProcessClosestPositionRequest(CClosestPositionRequest * pClosestPosRequest)
{
	Assertf(pClosestPosRequest->m_fSearchRadius < CPathServerExtents::GetSizeOfNavMesh(), "Search radius %.1f is too great, must be under %.1f", pClosestPosRequest->m_fSearchRadius, CPathServerExtents::GetSizeOfNavMesh());
	pClosestPosRequest->m_fSearchRadius = Min(pClosestPosRequest->m_fSearchRadius, CPathServerExtents::GetSizeOfNavMesh());

	//---------------------------------------------------
	// Determine which navmeshes we might need to visit

	const Vector3 vMeshOffsets[8] =
	{
		Vector3(-pClosestPosRequest->m_fSearchRadius, 0.0f, 0.0f),
		Vector3(-pClosestPosRequest->m_fSearchRadius * 0.707f, pClosestPosRequest->m_fSearchRadius * 0.707f, 0.0f),
		Vector3(0.0f, pClosestPosRequest->m_fSearchRadius, 0.0f),
		Vector3(pClosestPosRequest->m_fSearchRadius * 0.707f, pClosestPosRequest->m_fSearchRadius * 0.707f, 0.0f),
		Vector3(pClosestPosRequest->m_fSearchRadius, 0.0f, 0.0f),
		Vector3(pClosestPosRequest->m_fSearchRadius * 0.707f, -pClosestPosRequest->m_fSearchRadius * 0.707f, 0.0f),
		Vector3(0.0f, -pClosestPosRequest->m_fSearchRadius, 0.0f),
		Vector3(-pClosestPosRequest->m_fSearchRadius * 0.707f, -pClosestPosRequest->m_fSearchRadius * 0.707f, 0.0f)
	};

	TNavMeshIndex indices[9];
	indices[0] = CPathServer::GetNavMeshIndexFromPosition(pClosestPosRequest->m_vSearchOrigin, pClosestPosRequest->m_NavDomain);
	int iNumIndices = 1;

	//-----------------------------------------------------------------------------------
	// Add indices of surrounding navmeshes.  Ensure that we don't store any duplicates

	int o,o2;

	for(o=0; o<8; o++)
	{
		TNavMeshIndex index = CPathServer::GetNavMeshIndexFromPosition(pClosestPosRequest->m_vSearchOrigin + vMeshOffsets[o], pClosestPosRequest->m_NavDomain);
		for(o2=0; o2<iNumIndices; o2++)
		{
			if(indices[o2]==index)
				break;
		}
		if(o2==iNumIndices)
		{
			indices[iNumIndices++] = index;
		}
	}

	pClosestPosRequest->m_Results.m_iNumResults = 0;

	//-----------------------------------------------------------------------
	// Initially add a point directly under the input position, if one exists

	CNavMesh * pMeshUnderInputPos = CPathServer::GetNavMeshFromIndex( CPathServer::GetNavMeshIndexFromPosition(pClosestPosRequest->m_vSearchOrigin, pClosestPosRequest->m_NavDomain), pClosestPosRequest->m_NavDomain );
	if(pMeshUnderInputPos)
	{
		Vector3 vPointUnderInputPos;
		u32 iPoly = pMeshUnderInputPos->GetPolyBelowPoint( pClosestPosRequest->m_vSearchOrigin, vPointUnderInputPos, pClosestPosRequest->m_fSearchRadius );
		if(iPoly != NAVMESH_POLY_INDEX_NONE)
		{
			TNavMeshPoly * pPolyUnderInputPos = pMeshUnderInputPos->GetPoly(iPoly);
			if(ClosestPositionRequest_PerPolyCallback(pMeshUnderInputPos, pPolyUnderInputPos))
			{
				ClosestPositionRequest_PerPointCallback(pMeshUnderInputPos, pPolyUnderInputPos, vPointUnderInputPos);
			}
		}
	}

	//-------------------------
	// Visit each navmesh

	for(o=0; o<iNumIndices; o++)
	{
		CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(indices[o], pClosestPosRequest->m_NavDomain);
		if(pNavMesh)
		{
			pNavMesh->ForAllPolys(
				pClosestPosRequest->m_vSearchOrigin,
				pClosestPosRequest->m_fSearchRadius, 
				ClosestPositionRequest_PerPolyCallback,
				ClosestPositionRequest_PerPointCallback,
				TForAllPolysStruct::FLAG_MULTIPLE_POINTS_PER_POLYGON);
		}
	}

	if(pClosestPosRequest->m_Results.m_iNumResults != 0)
	{
		pClosestPosRequest->m_iCompletionCode = PATH_FOUND;
	}
	else
	{
		pClosestPosRequest->m_iCompletionCode = PATH_NOT_FOUND;
	}

	return true;
}

void CPathServerThread::IncTimeStamp(void)
{
	m_iNavMeshPolyTimeStamp++;

	// If our timestamp has reached whatever maximum value we have for it, then we must reset all the polys' timestamps to zero
	if(m_iNavMeshPolyTimeStamp == TIMESTAMP_MAX_VALUE)
	{
#ifdef NAVGEN_TOOL
		HandleTimeStampOverFlow();
#else

#if AI_OPTIMISATIONS_OFF || NAVMESH_OPTIMISATIONS_OFF
		Assertf(!m_bHandleTimestampOverflow, "Timestamp has overflowed twice during a single request - there will have been errors.");
#endif
		// Flag the system to handle the overflow directly after this request
		m_bHandleTimestampOverflow = true;
#endif
	}
}

void
CPathServerThread::IncAStarTimeStamp()
{
	m_iAStarTimeStamp++;

	// If our timestamp has reached whatever maximum value we have for it, then we must reset all the polys' timestamps to zero
	if(m_iAStarTimeStamp == TIMESTAMP_MAX_VALUE)
	{
#ifdef NAVGEN_TOOL
		HandleAStarTimeStampOverFlow();
#else

#if AI_OPTIMISATIONS_OFF || NAVMESH_OPTIMISATIONS_OFF
		Assertf(!m_bHandleAStarTimestampOverflow, "A* Timestamp has overflowed twice during a single request - there will have been errors.");
#endif
		// Flag the system to handle the overflow directly after this request
		m_bHandleAStarTimestampOverflow = true;
#endif
	}
}

// This function needs to visit all loaded polys in all nav-meshes, and has
// to reset their timestamps to zero..

void CPathServerThread::HandleTimeStampOverFlow(void)
{
	m_iNavMeshPolyTimeStamp = 1;

#ifdef GTA_ENGINE

#if __DEV
	printf("!!! CPathServerThread::HandleTimeStampOverFlow() !!!\n");
#endif

	LOCK_NAVMESH_DATA;
	LOCK_IMMEDIATE_DATA;
	LOCK_STORE_LOADED_MESHES;

#endif	// GTA_ENGINE

	for(int domain=0; domain<kNumNavDomains; domain++)
	{
		if(fwPathServer::GetIsNavDomainEnabled((aiNavDomain)domain))
		{
			const atArray<s32> & loadedMeshes = fwPathServer::GetNavMeshStore((aiNavDomain)domain)->GetLoadedMeshes();

			int n;
			for(n=0; n<loadedMeshes.GetCount(); n++)
			{
				TNavMeshIndex iMeshIndex = loadedMeshes[n];
				CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iMeshIndex, (aiNavDomain)domain);
				if(!pNavMesh)
					continue;

				for(u32 p=0; p<pNavMesh->GetNumPolys(); p++)
				{
					TNavMeshPoly & poly = *pNavMesh->GetPoly(p);
					poly.m_TimeStamp = 0;
				}
			}
		}
	}

	for(s32 n=0; n<CPathServer::m_DynamicNavMeshStore.GetNum(); n++)
	{
		CModelInfoNavMeshRef & ref = CPathServer::m_DynamicNavMeshStore.Get(n);
		CNavMesh * pNavMesh = ref.m_pBackUpNavmeshCopy;
		if(pNavMesh)
		{
			for(u32 p=0; p<pNavMesh->GetNumPolys(); p++)
			{
				TNavMeshPoly & poly = *pNavMesh->GetPoly(p);
				poly.m_TimeStamp = 0;
			}
		}
	}
}

void
CPathServerThread::HandleAStarTimeStampOverFlow(void)
{
	m_iAStarTimeStamp = 1;

#if __DEV
	printf("!!! CPathServerThread::HandleAStarTimeStampOverFlow() !!!\n");
#endif

#ifdef GTA_ENGINE

	LOCK_NAVMESH_DATA;
	LOCK_IMMEDIATE_DATA;
	LOCK_STORE_LOADED_MESHES;

#endif	// GTA_ENGINE

	for(int domain=0; domain<kNumNavDomains; domain++)
	{
		if(fwPathServer::GetIsNavDomainEnabled((aiNavDomain)domain))
		{
			const atArray<s32> & loadedMeshes = fwPathServer::GetNavMeshStore((aiNavDomain)domain)->GetLoadedMeshes();

			int n;
			for(n=0; n<loadedMeshes.GetCount(); n++)
			{
				TNavMeshIndex iMeshIndex = loadedMeshes[n];
				CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iMeshIndex, (aiNavDomain)domain);
				if(!pNavMesh)
					continue;

				for(u32 p=0; p<pNavMesh->GetNumPolys(); p++)
				{
					TNavMeshPoly & poly = *pNavMesh->GetPoly(p);
					poly.m_AStarTimeStamp = 0;
				}

				for(u32 s=0; s<pNavMesh->GetNumSpecialLinks(); s++)
				{
					pNavMesh->GetSpecialLinks()[s].SetAStarTimeStamp(0);
				}
			}
		}
	}

	for(s32 n=0; n<CPathServer::m_DynamicNavMeshStore.GetNum(); n++)
	{
		CModelInfoNavMeshRef & ref = CPathServer::m_DynamicNavMeshStore.Get(n);
		CNavMesh * pNavMesh = ref.m_pBackUpNavmeshCopy;
		if(pNavMesh)
		{
			for(u32 p=0; p<pNavMesh->GetNumPolys(); p++)
			{
				TNavMeshPoly & poly = *pNavMesh->GetPoly(p);
				poly.m_AStarTimeStamp = 0;
			}
		}
	}
}

void
CPathServerThread::CalculatePolyMinMaxForDynamicNavMesh(TNavMeshPoly * pPoly, aiNavDomain domain)
{
	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(pPoly->GetNavMeshIndex(), domain);
	Assert(pNavMesh);
	pNavMesh->CalculatePolyMinMaxForDynamicNavMesh(pPoly);
}


bool
CPathServerThread::TestNavMeshLOS(const Vector3 & vStartPos, const Vector3 & vEndPos, const u32 iLosFlags, aiNavDomain domain)
{
	m_LosVars.Init(vStartPos, vEndPos, iLosFlags);

	CNavMesh * pStartNavMesh, * pEndNavMesh;
	TNavMeshPoly * pStartPoly, * pEndPoly;

	const EPathServerErrorCode eStartRet = GetNavMeshPolyForRouteEndPoints(
		vStartPos,
		NULL,
		pStartNavMesh,
		pStartPoly,
		NULL,
		ms_fNormalDistBelowToLookForPoly, 0.0f,
		false, false,
		domain
	);

	if(eStartRet != PATH_NO_ERROR)
	{
		return false;
	}

	const EPathServerErrorCode eEndRet = GetNavMeshPolyForRouteEndPoints(
		vEndPos,
		NULL,
		pEndNavMesh,
		pEndPoly,
		NULL,
		ms_fNormalDistBelowToLookForPoly, 0.0f,
		false, false, domain
	);

	if(eEndRet != PATH_NO_ERROR)
	{
		return false;
	}

	Assert(!pStartPoly || pStartPoly->GetNavMeshIndex() != NAVMESH_INDEX_TESSELLATION);
	Assert(!pEndPoly || pEndPoly->GetNavMeshIndex() != NAVMESH_INDEX_TESSELLATION);

	// Increment timestamp before LOS.
	IncTimeStamp();

	const bool bLosExists = TestNavMeshLOS(
		vStartPos,
		vEndPos,
		pEndPoly,
		pStartPoly,
		NULL,
		iLosFlags, domain
	);

	return bLosExists;
}

//************************************************************************************
// pToPoly is the poly upon which the vEndPos is situated
// when first called pTestPoly will be the poly upon which vStartPos is situated

bool CPathServerThread::TestNavMeshLOS(const Vector3 & vStartPos, const Vector3 & vEndPos, TNavMeshPoly * pToPoly, TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, const u32 iLosFlags, aiNavDomain domain)
{
	if(pToPoly && pToPoly->GetNavMeshIndex()==NAVMESH_INDEX_TESSELLATION)
	{
		const u32 iPoly = CPathServer::GetTessellationNavMesh()->GetPolyIndex(pToPoly);
		TTessInfo * pTess = CPathServer::GetTessInfo(iPoly);
		Assert(pTess);
		CNavMesh * pOriginalMesh = CPathServer::GetNavMeshFromIndex(pTess->m_iNavMeshIndex, kNavDomainRegular);
		if(!pOriginalMesh)
			return false;
		pToPoly = pOriginalMesh->GetPoly(pTess->m_iPolyIndex);
	}
	if(pTestPoly && pTestPoly->GetNavMeshIndex()==NAVMESH_INDEX_TESSELLATION)
	{
		const u32 iPoly = CPathServer::GetTessellationNavMesh()->GetPolyIndex(pTestPoly);
		TTessInfo * pTess = CPathServer::GetTessInfo(iPoly);
		Assert(pTess);
		CNavMesh * pOriginalMesh = CPathServer::GetNavMeshFromIndex(pTess->m_iNavMeshIndex, kNavDomainRegular);
		if(!pOriginalMesh)
			return false;
		pTestPoly = pOriginalMesh->GetPoly(pTess->m_iPolyIndex);
	}
	if(pLastPoly && pLastPoly->GetNavMeshIndex()==NAVMESH_INDEX_TESSELLATION)
	{
		const u32 iPoly = CPathServer::GetTessellationNavMesh()->GetPolyIndex(pLastPoly);
		TTessInfo * pTess = CPathServer::GetTessInfo(iPoly);
		Assert(pTess);
		CNavMesh * pOriginalMesh = CPathServer::GetNavMeshFromIndex(pTess->m_iNavMeshIndex, kNavDomainRegular);
		if(!pOriginalMesh)
			return false;
		pLastPoly = pOriginalMesh->GetPoly(pTess->m_iPolyIndex);
	}

	Assert(!pToPoly || pToPoly->GetNavMeshIndex() != NAVMESH_INDEX_TESSELLATION);
	Assert(!pTestPoly || pTestPoly->GetNavMeshIndex() != NAVMESH_INDEX_TESSELLATION);
	Assert(!pLastPoly || pLastPoly->GetNavMeshIndex() != NAVMESH_INDEX_TESSELLATION);
	
	if(pToPoly && pToPoly->GetIsDisabled())
		return false;
	if(pTestPoly && pTestPoly->GetIsDisabled())
		return false;

	m_LosVars.Init(vStartPos, vEndPos, pToPoly, iLosFlags);

	return TestNavMeshLOS_R(pTestPoly, pLastPoly, domain);
}

TNavMeshPoly * CPathServerThread::TestNavMeshLOS(const Vector3 & vStartPos, const Vector2 & vLosDir, Vector3 * pvOut_IntersectionPos, TNavMeshPoly * pStartPoly, const u32 iLosFlags, aiNavDomain domain)
{
	if(!pStartPoly)
	{
		Vector3 vTmp;

		const u32 iStartNavMesh = CPathServer::GetNavMeshIndexFromPosition(vStartPos, domain);
		if(iStartNavMesh == NAVMESH_NAVMESH_INDEX_NONE)
			return NULL;
		CNavMesh * pStartNavMesh = CPathServer::GetNavMeshFromIndex(iStartNavMesh, domain);
		if(!pStartNavMesh)
			return NULL;
		const u32 iStartPolyIndex = pStartNavMesh->GetPolyBelowPoint(vStartPos + Vector3(0, 0, 1.0f), vTmp, 5.0f);
		if(iStartPolyIndex == NAVMESH_POLY_INDEX_NONE)
			return NULL;
		pStartPoly = pStartNavMesh->GetPoly(iStartPolyIndex);
		Assert(pStartPoly);
		Assert(pStartPoly->GetNavMeshIndex() != NAVMESH_INDEX_TESSELLATION);
	}

	if(pStartPoly && pStartPoly->GetIsDisabled())
		return NULL;

	if(pStartPoly && pStartPoly->GetNavMeshIndex()==NAVMESH_INDEX_TESSELLATION)
	{
		const u32 iPoly = CPathServer::GetTessellationNavMesh()->GetPolyIndex(pStartPoly);
		TTessInfo * pTess = CPathServer::GetTessInfo(iPoly);
		Assert(pTess);
		CNavMesh * pOriginalMesh = CPathServer::GetNavMeshFromIndex(pTess->m_iNavMeshIndex, kNavDomainRegular);
		if(!pOriginalMesh)
			return NULL;
		pStartPoly = pOriginalMesh->GetPoly(pTess->m_iPolyIndex);
	}

	m_LosVars.Init(vStartPos, vLosDir, iLosFlags);

	const bool bIntersection = TestNavMeshLOS_R(pStartPoly, NULL, domain);

	if(bIntersection)
	{
		if(pvOut_IntersectionPos)
			*pvOut_IntersectionPos = m_LosVars.m_vIntersectionPosition;
		return m_LosVars.m_pPolyEndedUpon;
	}
	return NULL;
}


bool CPathServerThread::TestNavMeshLOS_R(TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, aiNavDomain domain)
{
#if !__FINAL
	CPathServer::ms_iNumTestNavMeshLOS++;
#endif

	m_LosVars.m_iTestLosStackNumEntries = 0;

	Vector3 vEdgeVert1, vEdgeVert2;
	u32 v;
	s32 lastv;
	u32 iTestAdjNavMesh;
	TAdjPoly * pAdjacency;

RESTART_FOR_RECURSION:

	Assert(pTestPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);
	Assert(!pLastPoly || pLastPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);

	CNavMesh * pTestPolyNavMesh = CPathServer::GetNavMeshFromIndex(pTestPoly->GetNavMeshIndex(), domain);
	if(!pTestPolyNavMesh)
		return false;

	if(m_LosVars.m_pToPoly)
	{
#if NAVMESH_OPTIMISATIONS_OFF
		Assert(!m_LosVars.m_pToPoly->GetIsDisabled());
		Assert(!m_LosVars.m_pToPoly->GetIsDegenerateConnectionPoly());
		Assert(m_LosVars.m_pToPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);
#endif
		// In the case where we know the destination poly, then see if we've arrived there
		if(pTestPoly == m_LosVars.m_pToPoly && !pTestPoly->GetIsDegenerateConnectionPoly())
		{
			// LINE OF SIGHT EXISTS
			m_LosVars.m_iTestLosStackNumEntries = 0;
			return true;
		}
	}
	else
	{
		// In cases where we have no destination poly, then check for an minmax intersection with the known endpos
		// versus this polygon.  If this succeeds, then try a full polygon containment test.

		if(!pTestPoly->GetIsDegenerateConnectionPoly() &&
			m_LosVars.m_EndPosMinMax.IntersectsXY(pTestPoly->m_MinMax))
		{
			m_LosVars.m_vAboveEndPos.z = pTestPoly->m_MinMax.GetMaxZAsFloat() + 1.0f;
			m_LosVars.m_vBelowEndPos.z = pTestPoly->m_MinMax.GetMinZAsFloat() - 1.0f;

			const bool bIntersects = pTestPolyNavMesh->RayIntersectsPoly(m_LosVars.m_vAboveEndPos, m_LosVars.m_vBelowEndPos, pTestPoly, m_LosVars.m_vIntersectionPosition);
			if(bIntersects)
			{
				m_LosVars.m_iTestLosStackNumEntries = 0;
				m_LosVars.m_pPolyEndedUpon = pTestPoly;
				return true;
			}
		}
	}

	lastv = pTestPoly->GetNumVertices()-1;
	for(v=0; v<pTestPoly->GetNumVertices(); v++)
	{
		pAdjacency = pTestPolyNavMesh->GetAdjacentPolysArray().Get(pTestPoly->GetFirstVertexIndex() + lastv);
		iTestAdjNavMesh = pAdjacency->GetOriginalNavMeshIndex(pTestPolyNavMesh->GetAdjacentMeshes());

#if !__FINAL && __BANK
	//	if (CPathServer::m_bVisualisePolygonRequests)
	//	{
	//		Vector3 vPosCenter;
	//		pTestPolyNavMesh->GetPolyCentroidQuick(pTestPoly, vPosCenter);
	//		grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_VioletRed, 10);
	//	}
#endif

		// We cannot complete a LOS over an adjacency which is a climb-up/drop-down etc.
		if(pAdjacency->GetOriginalPolyIndex() != NAVMESH_POLY_INDEX_NONE && pAdjacency->GetAdjacencyType() == ADJACENCY_TYPE_NORMAL)
		{
			// Check that this NavMesh exists & is loaded. (It's quite possible for the refinement algorithm to
			// visit polys which the poly-pathfinder didn't..)
			CNavMesh * pAdjMesh = CPathServer::GetNavMeshFromIndex(iTestAdjNavMesh, domain);
			if(!pAdjMesh)
			{
				lastv = v;
				continue;
			}

			TNavMeshPoly * pAdjPoly = pAdjMesh->GetPoly(pAdjacency->GetOriginalPolyIndex());
			if(pAdjPoly->GetIsDisabled())
			{

#if !__FINAL && __BANK
				if (CPathServer::m_bVisualisePolygonRequests)
				{
					Vector3 vPosCenter;
					pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
					grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_red, 10);
				}
#endif

				lastv = v;
				continue;
			}

			Assert(pAdjPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);

#if __BREAK_UPON_PROCESSING_CONNECTION_POLY
			bool bPause = false;
			if(pAdjPoly->GetIsDegenerateConnectionPoly())
				bPause = true;
#endif

			// If this path is requested to keep to pavements, then don't allow a line-of-sight to
			// be clear when leaving a pavement poly onto a non-pavement poly - otherwise the
			// path may diverge from the pavement polygons
			if(m_pCurrentActiveRequest && !pAdjPoly->GetIsDegenerateConnectionPoly())
			{
				if(m_LosVars.m_iLosFlags & CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_PAVEMENT_BOUNDARY)
				{
					if(pTestPoly->GetIsPavement() && !pAdjPoly->GetIsPavement())
					{
#if !__FINAL && __BANK
						if (CPathServer::m_bVisualisePolygonRequests)
						{
							Vector3 vPosCenter;
							pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
							grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_yellow, 10);
						}
#endif

						lastv = v;
						continue;
					}
				}
				// Likewise for entering/exiting water
				if(m_LosVars.m_iLosFlags & CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_WATER_BOUNDARY)
				{
					if(pTestPoly->GetIsWater() != pAdjPoly->GetIsWater())
					{

#if !__FINAL && __BANK
						if (CPathServer::m_bVisualisePolygonRequests)
						{
							Vector3 vPosCenter;
							pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
							grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_copper, 10);
						}
#endif

						lastv = v;
						continue;
					}
				}

				// And for transitioning to a steep surface from a non-steep surface
				if(m_LosVars.m_iLosFlags & CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_TOOSTEEP_BOUNDARY)
				{
					if(pAdjPoly->TestFlags(NAVMESHPOLY_TOO_STEEP_TO_WALK_ON) && !pTestPoly->TestFlags(NAVMESHPOLY_TOO_STEEP_TO_WALK_ON))
					{

#if !__FINAL && __BANK
						if (CPathServer::m_bVisualisePolygonRequests)
						{
							Vector3 vPosCenter;
							pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
							grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_orange, 10);
						}
#endif

						lastv = v;
						continue;
					}
				}

				if(m_LosVars.m_iLosFlags & CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_MAXSLOPE_BOUNDARY)
				{
					if(pAdjPoly->m_MinMax.m_iMaxZ > pTestPoly->m_MinMax.m_iMaxZ)
					{
						// Get the normal of this pAdjPoly
						// NB: Hugely inefficient.  This needs to be cached per-poly.  Quake had an lookup-table of
						// normals, indexed by a single byte - we could do something similar, and quite possibly
						// at a lower resolution.  Our normals' Z component will always be > 0.

						Vector3 vAdjPolyNormal;
						if(pAdjMesh->GetPolyNormal(vAdjPolyNormal, pAdjPoly))
						{
							if(DotProduct(vAdjPolyNormal, ZAXIS) < m_Vars.m_fMinSlopeDotProductWithUpVector)
							{
#if !__FINAL && __BANK
								if (CPathServer::m_bVisualisePolygonRequests)
								{
									Vector3 vPosCenter;
									pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
									grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_blue, 10);
								}
#endif

								lastv = v;
								continue;
							}
						}
					}
				}
			}

			// Make sure we don't recurse back & forwards indefinitely
			// However, disregard the timestamp for the "pToPoly" because we don't want to screw our chances
			// of getting a decent LOS via another polygon if the "pToPoly" is adjacent to the "pFromPoly"
			// but happens to be not directly visible by the adjoining edge..
			if(pAdjPoly != pLastPoly && (pAdjPoly->m_TimeStamp != m_iNavMeshPolyTimeStamp || pAdjPoly == m_LosVars.m_pToPoly))
			{
				pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, lastv), vEdgeVert1);
				pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, v), vEdgeVert2);

				if(m_LosVars.m_iLosFlags & CTestNavMeshLosVars::FLAG_USE_ENTITY_RADIUS)
				{
					const float fDistToVert1 = geomDistances::DistanceSegToPoint( m_LosVars.m_vStartPos, m_LosVars.m_vLosVec, vEdgeVert1 );
					const float fVertexSpace1 = pTestPolyNavMesh->GetAdjacentPoly( pTestPoly->GetFirstVertexIndex()+lastv ).GetFreeSpaceAroundVertex();
					if(fDistToVert1 - m_LosVars.m_fEntityRadius + fVertexSpace1 < 0.0f)
					{

#if !__FINAL && __BANK
						if (CPathServer::m_bVisualisePolygonRequests)
						{
							Vector3 vPosCenter;
							pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
							grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_white, 10);
						}
#endif

						lastv = v;
						continue;
					}
					const float fDistToVert2 = geomDistances::DistanceSegToPoint( m_LosVars.m_vStartPos, m_LosVars.m_vLosVec, vEdgeVert2 );
					const float fVertexSpace2 = pTestPolyNavMesh->GetAdjacentPoly(pTestPoly->GetFirstVertexIndex()+v).GetFreeSpaceAroundVertex();					
					if(fDistToVert2 - m_LosVars.m_fEntityRadius + fVertexSpace2 < 0.0f)
					{

#if !__FINAL && __BANK
						if (CPathServer::m_bVisualisePolygonRequests)
						{
							Vector3 vPosCenter;
							pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
							grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_white, 10);
						}
#endif

						lastv = v;
						continue;
					}
				}

				if(CNavMesh::LineSegsIntersect2D_LOS(m_LosVars.m_vStartPos, m_LosVars.m_vEndPos, vEdgeVert1, vEdgeVert2))
				{
					pAdjPoly->m_TimeStamp = m_iNavMeshPolyTimeStamp;

					if(m_pCurrentActiveRequest && !pAdjPoly->GetIsDegenerateConnectionPoly())
					{
						//*************************************************************************************
						// If this path is using influence spheres, then don't allow a line-of-sight to
						// cross a sphere's boundary (defined by its radius).  We set a flag if we've
						// crossed a boundary, which we'll need to examine in the refinement code.
						// NB : It might not be sufficient to rely on this flag, since we might at this point
						// be visiting polys which we didn't visit during the path-search.  So, we may need
						// to actually do a sphere/point containment test ):

						if(m_LosVars.m_iLosFlags & CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_INFLUENCESPHERES_BOUNDARY)
						{
							const bool bStartIsWithinSphere = (pTestPoly->TestFlags(NAVMESHPOLY_IS_WITHIN_INFLUENCE_SPHERE));
							const bool bAdjacentIsWithinSphere = (pAdjPoly->TestFlags(NAVMESHPOLY_IS_WITHIN_INFLUENCE_SPHERE));

							if(bStartIsWithinSphere != bAdjacentIsWithinSphere)
							{
								m_bLosHitInfluenceBoundary = true;
							}
						}
						// And for transitioning between interior/exterior navmesh
						if(m_LosVars.m_iLosFlags & CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_ACROSS_INTERIOR_EXTERIOR_BOUNDARY)
						{
							if(pAdjPoly->GetIsInterior() != pTestPoly->GetIsInterior())
							{

#if !__FINAL && __BANK
								if (CPathServer::m_bVisualisePolygonRequests)
								{
									Vector3 vPosCenter;
									pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
									grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_pink, 10);
								}
#endif

								lastv = v;
								continue;
							}
						}
						// And for transitioning across sheltered/unsheltered boundaries
						if(m_LosVars.m_iLosFlags & CTestNavMeshLosVars::FLAG_NO_LOS_ACROSS_ACROSS_SHELTERED_BOUNDARY)
						{
							if(pAdjPoly->GetIsSheltered() != pTestPoly->GetIsSheltered())
							{
#if !__FINAL && __BANK
								if (CPathServer::m_bVisualisePolygonRequests)
								{
									Vector3 vPosCenter;
									pAdjMesh->GetPolyCentroidQuick(pAdjPoly, vPosCenter);
									grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_brown, 10);
								}
#endif

								lastv = v;
								continue;
							}
						}
					}

					//****************************************************************
					// This is the point of recursion in the original algorithm.
					// Instead of recursing, we'll store off the algorithm state in a
					// stack entry in "m_LosVars.m_TestLosStack", and will jump to the label
					// "RESTART_FOR_RECURSION".

					if(m_LosVars.m_iTestLosStackNumEntries >= SIZE_TEST_LOS_STACK)
					{
#if NAVMESH_OPTIMISATIONS_OFF
						Assertf(m_LosVars.m_iTestLosStackNumEntries < SIZE_TEST_LOS_STACK, "TestNavMeshLOS recursion stack reached its limit of %i.", SIZE_TEST_LOS_STACK);
#endif
						m_LosVars.m_iTestLosStackNumEntries = 0;
						return false;
					}

					{
						TTestLosStack & stackEntry = m_LosVars.m_TestLosStack[m_LosVars.m_iTestLosStackNumEntries];
						m_LosVars.m_iTestLosStackNumEntries++;

						stackEntry.pTestPoly = pTestPoly;
						stackEntry.pLastPoly = pLastPoly;
						stackEntry.iVertex = v;

						// Set pTestPoly & pLastPoly to their new values, as would occur in recursive call.
						pTestPoly = pAdjPoly;
						pLastPoly = pTestPoly;
					}
					
					goto RESTART_FOR_RECURSION;


				}
			}
		}
DROP_OUT_OF_RECURSION:	;

#if !__FINAL && __BANK
		if (CPathServer::m_bVisualisePolygonRequests)
		{
			Vector3 vPosCenter;
			pTestPolyNavMesh->GetPolyCentroidQuick(pTestPoly, vPosCenter);
			grcDebugDraw::Line(vPosCenter, vPosCenter + ZAXIS * 5.f, Color_green, 10);
		}
#endif

		lastv = v;
	}

	//********************************************************************
	// This is the point at which we would drop out of recursion.  If we
	// have any points of recursion stored in out stack, then re-enter
	// them now.

	if(m_LosVars.m_iTestLosStackNumEntries > 0)
	{
		m_LosVars.m_iTestLosStackNumEntries--;

		TTestLosStack & stackEntry = m_LosVars.m_TestLosStack[m_LosVars.m_iTestLosStackNumEntries];

		pTestPoly = stackEntry.pTestPoly;
		pLastPoly = stackEntry.pLastPoly;
		v = stackEntry.iVertex;
		lastv = stackEntry.iVertex-1;
		if(lastv < 0)
			lastv = pTestPoly->GetNumVertices()-1;
		
		pTestPolyNavMesh = CPathServer::GetNavMeshFromIndex(pTestPoly->GetNavMeshIndex(), domain);

		goto DROP_OUT_OF_RECURSION;
	}

	m_LosVars.m_iTestLosStackNumEntries = 0;
	return false;
}






//******************************************************************************************
// Similar to TestNavMeshLOS except finds the first change in slope of the navmesh
// underneath the line vStartPos to vEndPos.
// pToPoly is the poly upon which the vEndPos is situated
// when first called pTestPoly will be the poly upon which vStartPos is situated

// These variables are global to reduce stack overhead on recursion.
// We may need to do the "non re-entrant" treatment on this function..
Vector3 g_vFindHeightChange_StartPos(0.0f,0.0f,0.0f);
Vector3 g_vFindHeightChange_EndPos(0.0f,0.0f,0.0f);
Vector3 g_vFindHeightChange_Vec(0.0f,0.0f,0.0f);
const TNavMeshPoly * g_pFindHeightChange_ToPoly = NULL;
TNavMeshPoly * g_pFindHeightChange_OutHeightChangePoly = NULL;
Vector3 g_vFindHeightChange_HeightChangePos(0.0f,0.0f,0.0f);
int g_iFindHeightChange_NumRecursions = 0;
bool g_bFindHeightChange_OnlyUphill = false;	// set to true when underwater
static const int g_iFindHeightChange_MaxNumRecursions = 20;

bool CPathServerThread::FindHeightChangePosAlongLine(const Vector3 & vStartPos, const Vector3 & vEndPos, const TNavMeshPoly * pToPoly, TNavMeshPoly * pTestPoly, TNavMeshPoly *& pHeightChangePoly, Vector3 & vHeightChangePos, bool bOnlyUphill, aiNavDomain domain)
{
	Assert(pToPoly && pToPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);
	Assert(pTestPoly && pTestPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);
	Assert(pHeightChangePoly==NULL || pHeightChangePoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);

	g_vFindHeightChange_StartPos = vStartPos;
	g_vFindHeightChange_EndPos = vEndPos;
	g_pFindHeightChange_ToPoly = pToPoly;
	g_vFindHeightChange_Vec = vEndPos - vStartPos;
	
	g_bFindHeightChange_OnlyUphill = bOnlyUphill;
	g_iFindHeightChange_NumRecursions = 0;

	if(!g_vFindHeightChange_Vec.Mag2())
		return false;
	g_vFindHeightChange_Vec.Normalize();

	g_pFindHeightChange_OutHeightChangePoly = NULL;
	const bool bRet = FindHeightChangePosAlongLineR(pTestPoly, NULL, domain);

	pHeightChangePoly = g_pFindHeightChange_OutHeightChangePoly;
	vHeightChangePos = g_vFindHeightChange_HeightChangePos;

	return bRet;
}

bool CPathServerThread::FindHeightChangePosAlongLineR(TNavMeshPoly * pTestPoly, TNavMeshPoly * pLastPoly, aiNavDomain domain)
{
	Assert(!pTestPoly || pTestPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);
	Assert(!pLastPoly || pLastPoly->GetNavMeshIndex()!=NAVMESH_INDEX_TESSELLATION);

	g_iFindHeightChange_NumRecursions++;

	if(pTestPoly == g_pFindHeightChange_ToPoly || !pTestPoly || !g_pFindHeightChange_ToPoly)
	{
		return false;
	}

	static dev_float fMaxHeightChange = 0.5f;

	u32 v, lastv;
	int bIsectRet;
	Vector3 vEdgeVert1, vEdgeVert2, vIsectPos;

	const CNavMesh * pTestPolyNavMesh = CPathServer::GetNavMeshFromIndex(pTestPoly->GetNavMeshIndex(), domain);

	if(pTestPolyNavMesh)
	{
		lastv = pTestPoly->GetNumVertices()-1;
		for(v=0; v<pTestPoly->GetNumVertices(); v++)
		{
			const TAdjPoly & adjPoly = pTestPolyNavMesh->GetAdjacentPoly(pTestPoly->GetFirstVertexIndex() + lastv);
			const u32 iTestAdjNavMesh = adjPoly.GetOriginalNavMeshIndex(pTestPolyNavMesh->GetAdjacentMeshes());

			// We cannot complete a LOS over an adjacency which is a climb-up/drop-down etc.
			if(adjPoly.GetOriginalPolyIndex() != NAVMESH_POLY_INDEX_NONE && adjPoly.GetAdjacencyType() == ADJACENCY_TYPE_NORMAL)
			{
				// Check that this NavMesh exists & is loaded. (It's quite possible for the refinement algorithm to
				// visit polys which the poly-pathfinder didn't..)
				//const CNavMesh * pAdjMesh = CPathServer::GetNavMeshFromIndex(adjPoly.GetNavMeshIndex());
				const CNavMesh * pAdjMesh = CPathServer::GetNavMeshFromIndex(iTestAdjNavMesh, domain);
				if(!pAdjMesh)
				{
					lastv = v;
					continue;
				}

				Assert(pAdjMesh->GetIndexOfMesh() != NAVMESH_INDEX_TESSELLATION);

				TNavMeshPoly * pAdjPoly = pAdjMesh->GetPoly(adjPoly.GetOriginalPolyIndex());
				if(pAdjPoly->GetIsDisabled())
				{
					lastv = v;
					continue;
				}

				// Make sure we don't recurse back & forwards indefinitely.
				// However, disregard the timestamp for the "pToPoly" because we don't want to screw our chances
				// of getting a decent LOS via another polygon if the "pToPoly" is adjacent to the "pFromPoly"
				// but happens to be not directly visible by the adjoining edge..
				if(pAdjPoly != pLastPoly && (pAdjPoly->m_TimeStamp != m_iNavMeshPolyTimeStamp || pAdjPoly == g_pFindHeightChange_ToPoly))
				{
					pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, lastv), vEdgeVert1);
					pTestPolyNavMesh->GetVertex( pTestPolyNavMesh->GetPolyVertexIndex(pTestPoly, v), vEdgeVert2);

					//**********************************************************************
					// Get the intersection between this polygon edge and the path segment

					bIsectRet = CNavMesh::LineSegsIntersect2D(g_vFindHeightChange_StartPos, g_vFindHeightChange_EndPos, vEdgeVert1, vEdgeVert2, &vIsectPos);

					if(bIsectRet == SEGMENTS_INTERSECT)
					{
						pAdjPoly->m_TimeStamp = m_iNavMeshPolyTimeStamp;

						// Get the distance of this intersection point along the line.
						// Find the height of the line at this point

						Vector3 vFromStart = vIsectPos - g_vFindHeightChange_StartPos;
						vFromStart.z = 0.0f;
						float fDistAlong = vFromStart.Mag();

						const Vector3 vLinePosAtIntersection = g_vFindHeightChange_StartPos + (g_vFindHeightChange_Vec * fDistAlong);
						const float fZDiff = vIsectPos.z - vLinePosAtIntersection.z;

						if(fZDiff > fMaxHeightChange)
						{
							g_pFindHeightChange_OutHeightChangePoly = pAdjPoly;
							g_vFindHeightChange_HeightChangePos = vIsectPos;
							return true;
						}
						else if (!g_bFindHeightChange_OnlyUphill && fZDiff < -fMaxHeightChange)
						{
							g_pFindHeightChange_OutHeightChangePoly = pAdjPoly;
							g_vFindHeightChange_HeightChangePos = vIsectPos;
							return true;
						}
						else if(g_iFindHeightChange_NumRecursions > g_iFindHeightChange_MaxNumRecursions)
						{
							g_pFindHeightChange_OutHeightChangePoly = pAdjPoly;
							g_vFindHeightChange_HeightChangePos = vIsectPos;
							return true;
						}

						if(FindHeightChangePosAlongLineR(pAdjPoly, pTestPoly, domain))
						{
							return true;
						}
						g_iFindHeightChange_NumRecursions--;
					}
				}
			}

			lastv = v;
		}
	}

	return false;
}







