////////////////////////////////////////////////////////////////////////////////////
// Title	:	PortalTracker.cpp
// Author	:	John
// Started	:	23/12/05
//			:	Used to track positions of objects and rendering through interiors
////////////////////////////////////////////////////////////////////////////////////


#include "scene/portals/portalTracker.h"


// Rage headers
#include "grcore/debugdraw.h"

// framework
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/replay.h"
#include "modelInfo/MLOmodelInfo.h"
#include "network/general/NetworkUtil.h"
#include "objects/object.h"
#include "objects/DummyObject.h"
#include "pathserver/ExportCollision.h"
#include "peds/ped.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/scene_channel.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/Portal.h"
#include "scene/portals/portalInst.h"
#include "scene/world/gameWorld.h"
#include "fwdebug/picker.h"
#include "fwscene/search/SearchVolumes.h"
#include "streaming/streaming.h"
#include "vehicles/vehicle.h"
#include "weapons/projectiles/Projectile.h"

#if __BANK
#include "fwscene/scan/ScanNodesDebug.h"
#include "peds/PedDebugVisualiser.h"
#include "system/stack_collector.h"
#include "renderer/Debug/EntitySelect.h"
//#include "Renderer/font.h"

extern bool bShowPortalTrackerSafeZoneSpheres;
extern bool bShowPortalTrackerSafeZoneSphereForSelected;
extern bool bBreakWhenSelectedPortalTrackerSafeSphereRadiusIsNearZero;
extern bool bVisualiseAllTrackers;
extern bool bVisualiseCutsceneTrackers;
extern bool bVisualiseActivatingTrackers;
extern bool bVisualiseObjectNames;
extern bool bCheckConsistency;
extern bool bDontPerformPtUpdate;
extern bool bPtUpdateDebugSpew;
#endif // __BANK

//OPTIMISATIONS_OFF();

PARAM(portaltrackerdebugmodel, "Debug spew for portal tracking of all entities with given modelname");
PARAM(portaltrackertracepeds, "Enable callstack trace tracking for peds");
PARAM(portaltrackertracevehicles, "Enable callstack trace tracking for vehicles");
PARAM(portaltrackertracecamera, "Enable callstack trace tracking for vehicles");
PARAM(portaltrackerPositionHistory, "Enable Position history for portal tracked Peds and Vehicle");

BANK_ONLY(fwPtrListSingleLink	CPortalTracker::m_globalTrackerList;)	// list of all trackers
fwPtrListSingleLink	CPortalTracker::m_activatingTrackerList;	// list of trackers which keep interiors active
bool CPortalTracker::ms_enableTraversalOfMulitpleLinkPortals = false;

CInteriorProxy*		CPortalVisTracker::ms_pPrimaryIntProxy = NULL;
s32					CPortalVisTracker::ms_primaryRoomIdx = -1;
CPortalInst*		CPortalTracker::ms_pPlayerProximityPortal = NULL;

CInteriorInst*		CPortalTracker::ms_failedPlayerProbeDestination = NULL;

bool		g_debugAmbulancePortalTracker = false;
bool CPortalTracker::ms_bEnableStadiumProbesThisFrame=false;

#if __BANK
bool g_breakOnSelectedEntityInPortalTrackerUpdate = false;
bool g_breakOnDebugPortalTrackerAddressInPortalTrackerUpdate = false;
CPortalTracker *pDebugTracker = NULL;
bool g_breakOnSelectedEntityHavingDifferentInteriorLoc = false;
bool g_breakOnSelectedMoveToNewLocation = false;
fwInteriorLocation gDebugBreakLocation;

bool g_bToggleSelectedFromActivatingTrackerList = false;
bool g_bToggleSelectedLoadingCollisions = false;
bool g_IgnoreSpeedTestForSelectedEntity = false;

extern bool g_enableRecursiveCheck;
extern bool g_breakOnRecursiveFail;
extern bool g_recursiveCheckAddToPicker;
extern bool g_ignoreSmallDeltaEarlyOut;
extern bool g_pauseGameOnRecursiveFail;

namespace rage
{
void Timer_TogglePauseMode(void);
}

bool g_recursedUpdate = false;
bool gRunUpdate = false;

const u32 traceTypeCamera = 0xff;
CPortalVisTracker *CPortalTracker::ms_pTraceVisTracker = NULL;

struct TrackProbeEntry
{
	TrackProbeEntry() :
		pos(0.0f, 0.0f, 0.0f),
		probePoint(0.0f, 0.0f, 0.0f),
		interiorHashName(),
		roomIdx(-1),
		probeLength(0.0f),
		frameCount(0),
		pTracker(NULL),
		probeType(CPortalTracker::PROBE_TYPE_DEFAULT),
		ownerEntityId(ENTITYSELECT_INVALID_ID),
		probeSucceeded(false)
	{
	}

	TrackProbeEntry(const Vector3 &_pos,
					const Vector3 &_probePoint,
					atNonFinalHashString _interiorHashName,
					s32 _roomIdx,
					float _probeLength,
					u32 _frameCount,
					CPortalTracker *_pTracker,
					bool _probeSucceeded,
					u32 _probeType,
					entitySelectID _ownerEntityID) :
		pos(_pos),
		probePoint(_probePoint),
		interiorHashName(_interiorHashName),
		roomIdx(_roomIdx),
		probeLength(_probeLength),
		frameCount(_frameCount),
		pTracker(_pTracker),
		probeType(_probeType),
		ownerEntityId(_ownerEntityID),
		probeSucceeded(_probeSucceeded)
	{
	}

	Vector3 pos;
	Vector3 probePoint;
	atNonFinalHashString interiorHashName;
	s32 roomIdx;
	float probeLength;
	u32 frameCount;
	CPortalTracker *pTracker;
	u32 probeType;
	entitySelectID ownerEntityId;
	bool probeSucceeded;
};
TrackProbeEntry gPTDebugEntry;

extern bool bPtEnableDrawingOfCollectedCallstack;
enum { PT_CALLSTACK_BUFFERS_SIZE = 8192 };

rage::sysStackCollector* CPortalTracker::ms_pStackCollector;
s32						 CPortalTracker::ms_entityTypeToCollectCallStacksFor = -1;
u16						 CPortalTracker::ms_lastRenderedCallstackCount = 0;
CPortalTracker *		 CPortalTracker::ms_lastRenderPortalTrackerForCallstack = NULL;
char *					 CPortalTracker::ms_callstackText = NULL;


enum { PT_MAX_MAP_SIZE = 128 };

typedef atMap<u32,TrackProbeEntry> TrackProbeMap;
TrackProbeMap *ms_ptTrackedProbes;

enum { POSITION_HISTORY_SIZE = 10 * 30 };
struct PositionHistory
{
	PositionHistory() : nextIndex(0), pTracker(NULL) { memset(position, 0, sizeof(position)); }
	float position[3 * POSITION_HISTORY_SIZE];
	int nextIndex;
	CPortalTracker *pTracker;
};
typedef atMap<u32,PositionHistory> PostionHistoryMap;
PostionHistoryMap *ms_ptPositionHistory;


const u32 INVALID_HASHTAG = (u32)-1;
#endif
//--------------------------- CPortal Tracker---------------------------


CPortalTrackerBase::~CPortalTrackerBase() { }

// tracker needs to track the movement from frame to frame of an entity. Need to init with correct position.
CPortalTracker::CPortalTracker(){

	m_pInteriorProxy = NULL;
	m_roomIdx = 0;

	m_bLoadsCollisions = false;
	m_bDontProcessUpdates = false;

	m_pOwner = NULL;

	m_bUpdated = false;

	BANK_ONLY(m_globalTrackerList.Add(this);)

	m_bActivatingTracker = false;		// most trackers won't cause activation of interiors in world

	m_bUsingProbeData = false;		// room data is is from probe result?

	m_bFlushedFromRetainList = false;
	m_bIgnoreHighSpeedDeltaTest = false;
	m_bRequestRescanNextUpdate = false;
	m_bScanUntilProbeTrue = false;
	m_bIsCutsceneControlled = false;
	m_bIsReplayControlled = false;
	m_bIsFallbackEnabled = false;
	m_bCaptureForInterior = false;
	m_bWaitForAllCollisionsBeforeProbe = false;
	m_bIsExteriorOnly = false;
	m_bForcedToBeActivating = false;

	m_padding = 0;

	m_bProbeType = PROBE_TYPE_DEFAULT;

	m_portalsSafeSphere.SetV4( Vec4V(V_ZERO) );
	m_safeInteriorPopulationScanCode = -1;

	m_currPos = m_oldPos = VEC3V_TO_VECTOR3( Vec3V(V_FLT_MAX) );
#if RSG_PC
	m_currPosObfuscated = m_currPos;
#endif

#if __BANK
	m_callstackTrackerTag = INVALID_HASHTAG;
	m_callstackCount = 0;
	m_setownerCalled = 0;
#endif
}

CPortalTracker::~CPortalTracker(){

	RemoveFromInterior();

	BANK_ONLY(m_globalTrackerList.Remove(this);)

	if (Unlikely(m_bActivatingTracker))
	{
		m_activatingTrackerList.Remove(this);
	} else
	{
		Assert(!m_activatingTrackerList.IsMemberOfList(this));
	}
#if __BANK
	if (m_callstackTrackerTag != INVALID_HASHTAG)
	{
		ms_pStackCollector->UnregisterTag(m_callstackTrackerTag);
		m_callstackTrackerTag = INVALID_HASHTAG;
	}
#endif
}

void CPortalTracker::ShutdownLevel(){

	CPortalVisTracker::ResetPrimaryData();
}

void CPortalTracker::SetLoadsCollisions(bool bLoadsCollisions)
{
	if (m_bLoadsCollisions != (u32)bLoadsCollisions){
		if (bLoadsCollisions == true){
			AddToActivatingTrackerList(this);
		} else {
			RemoveFromActivatingTrackerList(this);
		}

 		m_bLoadsCollisions = (u32)bLoadsCollisions;
	}
}

void CPortalTracker::SetInteriorAndRoom(CInteriorInst* pIntInst, s32 roomIdx)
{
    Assert(pIntInst);
	Assert(pIntInst->GetProxy());
    m_pInteriorProxy = pIntInst->GetProxy();
    m_roomIdx = roomIdx;
}

// probe from the portal tracker position down into the collision map & see if room data is there
bool CPortalTracker::DetermineInteriorLocationFromCoord(fwInteriorLocation& loc, Vector3* pProbePoint, bool useCustomProbeDistance, float probeDistance)
{
	CInteriorInst* pProbedIntInst = NULL;
	s32	probedRoomIdx = -1;

	if (m_pOwner == CGameWorld::FindLocalPlayer())
	{
		ms_failedPlayerProbeDestination = NULL;
	}

//	if (this->IsVisPortalTracker())
//	{
//		Displayf("camera");
//	}

	bool bRet;
	if (useCustomProbeDistance)
	{
		bRet = ProbeForInterior(m_currPos, pProbedIntInst, probedRoomIdx, pProbePoint, probeDistance);
	}
	else
	{
		// look for a collision with useful information at up to 4.0m, if that returns nothing then try again at 16.0m
		probeDistance = NEAR_PROBE_DIST;
		bRet = ProbeForInterior(m_currPos, pProbedIntInst, probedRoomIdx, pProbePoint, probeDistance);
		if (!bRet){

			probeDistance = IsVisPortalTracker() ? (float)FAR_PROBE_DIST_VIS : (float)FAR_PROBE_DIST;
			bRet = ProbeForInterior(m_currPos, pProbedIntInst, probedRoomIdx, pProbePoint, probeDistance);
		}
	}
#if __BANK
	if (!g_recursedUpdate && m_callstackTrackerTag != INVALID_HASHTAG)
	{
		TrackProbeEntry *pEntry = ms_ptTrackedProbes->Access(m_callstackTrackerTag);
		if (pEntry)
		{
			Assertf(pEntry->pTracker == this, "Hash collision for tracking probe");
			ms_ptTrackedProbes->Delete(m_callstackTrackerTag);
		}

		TrackProbeEntry entry(m_currPos, *pProbePoint, pProbedIntInst ? pProbedIntInst->GetBaseModelInfo()->GetModelNameHash() : 0, probedRoomIdx, probeDistance, TIME.GetFrameCount(),
							  this, bRet, m_bProbeType, m_pOwner ? CEntityIDHelper::ComputeEntityID(m_pOwner) : ENTITYSELECT_INVALID_ID);
		ms_ptTrackedProbes->Insert(m_callstackTrackerTag, entry);
	}
#endif


	loc.MakeInvalid();

	if (pProbedIntInst)
	{
		if ((probedRoomIdx > 0) && pProbedIntInst->CanReceiveObjects()){
			if (Verifyf(probedRoomIdx < pProbedIntInst->GetNumRooms(), "Probe at current position %f, %f, %f, returned an invalid room index for the interior inst,"
																	   "this will be from the collision data containing the incorrect room",
																		m_currPos.x, m_currPos.y, m_currPos.z))
			{
				loc = CInteriorProxy::CreateLocation(pProbedIntInst->GetProxy(), probedRoomIdx);
				return(true);
			}
		}
		else
		{

			if (m_pOwner == CGameWorld::FindLocalPlayer())
			{
				// really want to try and get into this interior... (e.g. respawn in metro section)
				CPed* pPlayer = static_cast<CPed*>(m_pOwner);
				if (pProbePoint && (m_currPos.Dist(*pProbePoint) < 2.0f))
				{
					ms_failedPlayerProbeDestination = pProbedIntInst;
					SetProbeType(PROBE_TYPE_SHORT);
					RequestRescanNextUpdate();

					if( pPlayer->GetIsInVehicle() )
					{
						CVehicle *pVehicle = pPlayer->GetVehiclePedInside();
						if(pVehicle)
						{
							pVehicle->GetPortalTracker()->SetProbeType(PROBE_TYPE_NEAR);
							pVehicle->GetPortalTracker()->RequestRescanNextUpdate();
						}
					}
				}
			}
		}
		return(false);		// interior is there, but not ready yet
	}

	return(bRet);
}

// for a given interior location & position, test for proximity to any of the portals (because the collisions may not match 100% against the
// portal positions. If we are close to a portal then use the portal to determine which side we are on).
fwInteriorLocation CPortalTracker::UpdateLocationFromPortalProximity(fwInteriorLocation loc, bool* goneOutOfMap){

	fwInteriorLocation ret = loc;
	s32 foundRoomIdx = loc.GetRoomIndex();
	CInteriorInst* pProbeIntInst = CInteriorInst::GetInteriorForLocation( loc );

	*goneOutOfMap = false;
	if (pProbeIntInst && foundRoomIdx > -1)
	{
		for(u32 i=0; i<pProbeIntInst->GetNumPortalsInRoom(foundRoomIdx);i++){

			if (pProbeIntInst->IsMirrorPortal(foundRoomIdx, i))
			{
				continue;
			}

			s32 portalIdx = pProbeIntInst->GetPortalIdxInRoom(foundRoomIdx, i);
			s32 portalTestRoomIdx = pProbeIntInst->TestPointAgainstPortal(portalIdx, m_currPos);

			if (pProbeIntInst->IsLinkPortal(foundRoomIdx, i) && (portalTestRoomIdx == 0)){
				// deal with the case where we are very close to a link portal - find correct room and interior
				CPortalInst* pPortalInst = pProbeIntInst->GetMatchingPortalInst(foundRoomIdx,i);
				//Assert(pPortalInst);
				if (pPortalInst){
					// update using info from the linked portal
					CInteriorInst* pDestIntInst = NULL;
					s32	destPortalIdx = -1;

					// get data about linked interior through this portal
					pPortalInst->GetDestinationData(pProbeIntInst, pDestIntInst, destPortalIdx);
					if ( pDestIntInst ) {
						// need to get the room index from the new interior and portal
						portalTestRoomIdx = pDestIntInst->GetDestThruPortalInRoom(0, destPortalIdx);		//entry/exit portal so we know it's room 0
						// update interior location too
						ret.SetInteriorProxyIndex(pDestIntInst->GetProxy()->GetInteriorProxyPoolIdx());
					} else {
// 						*goneOutOfMap = true;
 						ret.MakeInvalid();
 						return ret;
					}
				}
			}

			if (portalTestRoomIdx > -1){
				foundRoomIdx = portalTestRoomIdx;
				break;
			}
		}
	}

	if (foundRoomIdx > 0){
		ret.SetRoomIndex(foundRoomIdx);
	}
	else if (foundRoomIdx == 0) {
		ret.MakeInvalid();
	}

	return(ret);
}

s32 debugIntersectionRoomIdx = -1;

// probe for an interior at the given position
bool CPortalTracker::ProbeForInteriorProxy(const Vector3& pos, CInteriorProxy*& pIntProxy, s32& roomIdx, Vector3* pProbePoint, float probeLength){

	// now probe to find an interior at this position
	const int NUM_INTERSECTION_RESULTS = 32;

	Vector3 vecLineStart, vecLineEnd;
	roomIdx = INTLOC_INVALID_INDEX;

	vecLineStart = pos + Vector3(0.0f, 0.0f, 0.2f);
	vecLineEnd = pos + Vector3(0.0f, 0.0f, -probeLength);

	CInteriorProxy* pHighestIntersectionIntProxy = NULL;
	s32 intersectionRoomIdx = -1;
	s32 highestIntersectionRoomIdx = -1;
	float	highestOutsideCollisionT = 10.0f;		// point on probe line  of the highest outside collision
	float	highestInsideCollisionT = 10.0f;		// point on probe line of highest interior collision with valid room ID

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestHitPoint probeHits[NUM_INTERSECTION_RESULTS];
	WorldProbe::CShapeTestResults probeResults(probeHits, NUM_INTERSECTION_RESULTS);
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(vecLineStart, vecLineEnd);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probeDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED);

	probeDesc.SetOptions(WorldProbe::LOS_NO_RESULT_SORTING);

	bool bValidCollision = false;

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		WorldProbe::ResultIterator it;
		for(it = probeResults.begin(); it < probeResults.end(); ++it)
		{
			debugIntersectionRoomIdx = -1;

			if(it->GetHitDetected())
			{
				intersectionRoomIdx = PGTAMATERIALMGR->UnpackRoomId(it->GetHitMaterialId());
				debugIntersectionRoomIdx = intersectionRoomIdx;
				if (intersectionRoomIdx != 0)
				{
					// have found a room in the material, lets try and get the interior object
					phInst* pPhysInst = it->GetHitInst();
					if(pPhysInst)
					{
						if(it->GetHitTValue() < highestInsideCollisionT)
						{
							CEntity* pIntersectionEntity = CPhysics::GetEntityFromInst(pPhysInst);
							Assert(pIntersectionEntity);

							if (pIntersectionEntity->GetOwnedBy() != ENTITY_OWNEDBY_STATICBOUNDS)
							{
								//Assertf(pIntersectionEntity->GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS,  "%s : interior room idx is set on bounds data which shouldn't be.", g_StaticBoundsStore.GetName(strLocalIndex(pIntersectionEntity->GetIplIndex())));
								// if this isn't static geometry, then it's an instanced object, so ignore it as the room ID could be wrong and the following calculations are going to assert anyway
								continue;
							}
							
							// new style - the interior physics is part of the static bounds. Extract interior proxy from collision data if possible...
							strLocalIndex boundsStoreIndex = strLocalIndex(pIntersectionEntity->GetIplIndex());
							Assert(g_StaticBoundsStore.IsValidSlot(boundsStoreIndex));

							InteriorProxyIndex proxyId = -1;
							strLocalIndex depSlot;
							g_StaticBoundsStore.GetDummyBoundData(boundsStoreIndex, proxyId, depSlot);

							if (proxyId == -1)
							{
								Assertf(false, "bad probe from (%.2f,%.2f,%.2f) downwards was detected",pos.GetX(), pos.GetY(), pos.GetZ());

								Errorf("bounds at slot %d (%s) has room Id data, but no matching interior",boundsStoreIndex.Get(), g_StaticBoundsStore.GetName(boundsStoreIndex));
								Assertf(proxyId != -1,"bounds at slot %d (%s) has room Id data, but no matching interior",boundsStoreIndex.Get(), g_StaticBoundsStore.GetName(boundsStoreIndex));
							}
							else
							{
								CInteriorProxy* pInteriorProxy = CInteriorProxy::GetPool()->GetSlot(proxyId);
								Assert(pInteriorProxy);
								Assert(pInteriorProxy->GetStaticBoundsSlotIndex() == boundsStoreIndex);

								pHighestIntersectionIntProxy = pInteriorProxy;
								highestIntersectionRoomIdx = intersectionRoomIdx;
								highestInsideCollisionT = it->GetHitTValue();
								bValidCollision = true;
								if (pProbePoint)
								{
									*pProbePoint = it->GetHitPosition();
								}
							}
						}
					}
				}
				else
				{
					phInst* pPhysInst = it->GetHitInst();
					if(pPhysInst)
					{
						CEntity* pIntersectionEntity = CPhysics::GetEntityFromInst(pPhysInst);
						Assert(pIntersectionEntity);

						strLocalIndex boundsStoreIndex = strLocalIndex(pIntersectionEntity->GetIplIndex());
						Assert(g_StaticBoundsStore.IsValidSlot(boundsStoreIndex));

						InteriorProxyIndex proxyId = -1;
						strLocalIndex depSlot;
						g_StaticBoundsStore.GetDummyBoundData(boundsStoreIndex, proxyId, depSlot);
						fwInteriorLocation location = pIntersectionEntity->GetInteriorLocation();

						if (proxyId != -1 || location.IsValid())
						{
							continue;
						}
					}
					highestOutsideCollisionT = MIN(highestOutsideCollisionT, it->GetHitTValue());
					if (pProbePoint)
					{
						*pProbePoint = it->GetHitPosition();
					}
					bValidCollision = true;
				}
			}
		}
	}

	// the intersection closest to our start point determines interior location etc.
	// (there is a preference for inside collisions by bias of 5% - in the cases where we get coplanar inside / outside collisions)
	float interiorBias = 0.05f;

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && probeLength != 0.0f)
	{
		const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
		if(renderedCamera && renderedCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
		{
			// Don't use a bias of 5% as this will be larger for probes of greater length, instead always use a bias of 20cm.
			interiorBias = 20.0f / (probeLength * 100.0f);
		}
	}
#endif

	// GTA V hack - bug 2620688 - camera probing high enough above interior will probe to the interior, rather than the exterior
	if (camInterface::IsRenderingFirstPersonCamera() && probeLength >= (float)FAR_PROBE_DIST_VIS)
	{
		// Don't use a bias of 5% as this will be larger for probes of greater length, instead always use a bias of 50cm.
		interiorBias = 50.0f / (probeLength * 100.0f);
	}

	if ((highestOutsideCollisionT + interiorBias) < highestInsideCollisionT)
	{
		pHighestIntersectionIntProxy = NULL;
		highestIntersectionRoomIdx = 0;
	}

	roomIdx = highestIntersectionRoomIdx;
	pIntProxy = pHighestIntersectionIntProxy;
	return(bValidCollision);
}

// probe for an interior at the given position
bool CPortalTracker::ProbeForInterior(const Vector3& pos, CInteriorInst*& pIntInst, s32& roomIdx, Vector3* pProbePoint, float probeLength){

	CInteriorProxy* pProxy = NULL;
	bool retVal = false;
	retVal = ProbeForInteriorProxy(pos, pProxy, roomIdx, pProbePoint, probeLength);

	if (pProxy)
	{
		pIntInst = pProxy->GetInteriorInst();
	}

	if (pProxy!= NULL && pIntInst == NULL)
	{
		//interior not ready
		retVal = false;
	}

	return(retVal);

// 	// now probe to find an interior at this position
// 	const int NUM_INTERSECTION_RESULTS = 32;
//
// 	Vector3 vecLineStart, vecLineEnd;
// 	roomIdx = INTLOC_INVALID_INDEX;
//
// 	vecLineStart = pos + Vector3(0.0f, 0.0f, 0.2f);
// 	vecLineEnd = vecLineStart + Vector3(0.0f, 0.0f, -probeLength);
//
// 	CEntity* pIntersectionEntity = NULL;
// 	CInteriorInst* pHighestIntersectionIntInst = NULL;
// 	s32 intersectionRoomIdx = -1;
// 	s32 highestIntersectionRoomIdx = -1;
// 	float	highestOutsideCollisionT = 10.0f;		// point on probe line  of the highest outside collision
// 	float	highestInsideCollisionT = 10.0f;		// point on probe line of highest interior collision with valid room ID
//
// 	WorldProbe::CShapeTestProbeDesc probeDesc;
// 	WorldProbe::CShapeTestHitPoint probeHits[NUM_INTERSECTION_RESULTS];
// 	WorldProbe::CShapeTestResults probeResults(probeHits, NUM_INTERSECTION_RESULTS);
// 	probeDesc.SetResultsStructure(&probeResults);
// 	probeDesc.SetStartAndEnd(vecLineStart, vecLineEnd);
// 	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
// 	probeDesc.SetOptions(WorldProbe::LOS_NO_RESULT_SORTING);
//
// 	bool bValidCollision = false;
//
// 	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
// 	{
// 		WorldProbe::ResultIterator it;
// 		for(it = probeResults.begin(); it < probeResults.end(); ++it)
// 		{
// 			if(it->GetHitDetected())
// 			{
// 				intersectionRoomIdx = PGTAMATERIALMGR->UnpackRoomId(it->GetHitMaterialId());
// 				if (intersectionRoomIdx != 0)
// 				{
// 					// have found a room in the material, lets try and get the interior entity
// 					phInst* pPhysInst = it->GetHitInst();
// 					if(pPhysInst)
// 					{
// 						pIntersectionEntity = CPhysics::GetEntityFromInst(pPhysInst);
// 						Assert(pIntersectionEntity);
// 						CBaseModelInfo *pModelInfo = pIntersectionEntity->GetBaseModelInfo();
//
// 						// old style - the interior physics is pushed in by the MLO entity
// 						if(pModelInfo && pModelInfo->GetModelType() == MI_TYPE_MLO)
// 						{
// 							if(it->GetHitTValue() < highestInsideCollisionT)
// 							{
// 								pHighestIntersectionIntInst = static_cast<CInteriorInst*>(pIntersectionEntity);
// 								highestIntersectionRoomIdx = intersectionRoomIdx;
// 								highestInsideCollisionT = it->GetHitTValue();
// 								bValidCollision = true;
// 							}
// 						} else if (pIntersectionEntity->GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS)
// 						{
// 							// new style - the interior physics is part of the static bounds. Extract interior proxy from collision data if possible...
// 							s32 boundsStoreIndex = pIntersectionEntity->GetIplIndex();
// 							Assert(g_StaticBoundsStore.IsValidSlot(boundsStoreIndex));
//
// 							u32 fileNameHash = g_StaticBoundsStore.GetHash(boundsStoreIndex);
//
// 							u32 imapStoreIndex = g_MapDataStore.FindSlotFromHashKey(fileNameHash);
//
// 							CInteriorProxy* pInteriorProxy = CInteriorProxy::FindProxy(imapStoreIndex);
//
// 							CInteriorInst* pIntersectionInterior = pInteriorProxy->GetInteriorInst();
//
// 							if (pIntersectionEntity)
// 							{
// 								if(it->GetHitTValue() < highestInsideCollisionT)
// 								{
// 									pHighestIntersectionIntInst = pIntersectionInterior;
// 									highestIntersectionRoomIdx = intersectionRoomIdx;
// 									highestInsideCollisionT = it->GetHitTValue();
// 									bValidCollision = true;
// 								}
// 							}
//
// 						} else
// 						{
// 							Assertf(false, "%s : interior room idx is set on bounds data which shouldn't be.", g_StaticBoundsStore.GetName(pIntersectionEntity->GetIplIndex()));
// 						}
// 					}
// 				} else
// 				{
// 					// ignore any collision results with interior objects which have a room ID of 0
// 					phInst* pPhysInst = it->GetHitInst();
// 					if(pPhysInst)
// 					{
// 						pIntersectionEntity = CPhysics::GetEntityFromInst(pPhysInst);
// 						if(pIntersectionEntity && (pIntersectionEntity->GetIsInInterior() || pIntersectionEntity->GetIsTypeMLO()))
// 						{
// 							continue;
// 						}
// 					}
//
// 					highestOutsideCollisionT = MIN(highestOutsideCollisionT, it->GetHitTValue());
// 					bValidCollision = true;
// 				}
// 			}
// 		}
// 	}
//
// 	// the intersection closest to our start point determines interior location etc.
// 	// (there is a preference for inside collisions by bias of 5% - in the cases where we get coplanar inside / outside collisions)
// 	if ((highestOutsideCollisionT + 0.05f) < highestInsideCollisionT){
// 		pHighestIntersectionIntInst = NULL;
// 		highestIntersectionRoomIdx = 0;
// 	}
//
// 	roomIdx = highestIntersectionRoomIdx;
// 	pIntInst = pHighestIntersectionIntInst;
// 	return(bValidCollision);
}

// this version check for passing through a portal when a collision is detected with it
void CPortalTracker::EntryExitPortalCheck(fwPtrList& scanList, fwInteriorLocation& destLoc, bool &isStraddlingPortal, const Vector3& movementDelta)
{
#if __BANK
	bool enableStraddlingPortals = *fwScanNodesDebug::GetPortalStraddlingEnabled();
#else
	const bool enableStraddlingPortals = true;
#endif
	bool bProcessing = true;
	CPortalTracker* pTracker = this;
	fwPtrNode* pNode = scanList.GetHeadPtr();

	isStraddlingPortal = false;

	fwGeom::fwLine straddlingTestLine(Vector3::ZeroType, Vector3::ZeroType);
	ASSERT_ONLY(bool bStraddlingTestLineValid = false;)

	bool canStraddlePortals = m_pOwner && m_pOwner->GetIsDynamic() && static_cast<CDynamicEntity*>(m_pOwner)->CanStraddlePortals();
	if(canStraddlePortals)
	{
		Vector3 direction = movementDelta;
		direction.Normalize();
		if(m_pOwner->GetIsTypePed())
		{
			spdAABB aabb;
			static_cast<CPed*>(m_pOwner)->GetExtendedBoundBox(aabb);
			direction *= Mag(aabb.GetExtentFlat()).Getf();
		}
		else if (m_pOwner->GetIsTypeVehicle())
		{
			const spdAABB& aabb = m_pOwner->GetBaseModelInfo()->GetBoundingBox();
			direction *= aabb.GetExtent().GetYf();
		}
		else
		{
			const spdAABB& aabb = m_pOwner->GetBaseModelInfo()->GetBoundingBox();
			direction *= Mag(aabb.GetExtentFlat()).Getf();
		}

		straddlingTestLine.Set(m_currPos - direction, m_currPos + direction);
		ASSERT_ONLY(bStraddlingTestLineValid = true);
	}

	fwGeom::fwLine portalTraversalTestLine;
	fwPortalCorners::CreateExtendedLineForPortalTraversalCheck(pTracker->m_oldPos, pTracker->m_currPos, portalTraversalTestLine);

	while(bProcessing && pNode){

		CEntity* pEntity = static_cast<CEntity*>(pNode->GetPtr());
		if (pEntity){

			// collision with instanced portal (entry / exit portal)
			if (pEntity->GetModelIndex() == CPortal::GetPortalInstModelId().GetModelIndex())
			{
				CPortalInst* pPortalInst = (static_cast<CPortalInst*>(pEntity));
				CInteriorInst* pIntInst = pPortalInst->GetPrimaryInterior();	// track moving into vicinity of this interior
				Assert(pIntInst);

				s32 portalIdxRoom0 = -1;

				fwPortalCorners portal;
				pPortalInst->GetOwnerData(pIntInst, portalIdxRoom0);
				pIntInst->GetPortalInRoom(0,portalIdxRoom0, portal);		// instanced entry/exit portals are all room 0 of course.

				if (!pIntInst->IsMirrorPortal(0, portalIdxRoom0))
				{
					if (enableStraddlingPortals && canStraddlePortals && !isStraddlingPortal)
					{
						Assert(bStraddlingTestLineValid);
						// Fix this later to be just one test
						if (portal.HasPassedThrough(straddlingTestLine, false))
						{
							isStraddlingPortal = true;
#if __BANK
							if (*fwScanNodesDebug::GetDisplayPortalStraddlingDebugDraw())
							{
								rage::grcDebugDraw::Line(straddlingTestLine.m_start, straddlingTestLine.m_end, rage::Color32(0, 255, 255, 255), false);
							}
#endif
						}
					}

					// test for traversal
					if (portal.HasPassedThrough(portalTraversalTestLine))
					{
						// ensure the portal is enabled before traversing
						u32 portalIdx = pIntInst->GetPortalIdxInRoom(0, portalIdxRoom0);
						const fwPortalSceneGraphNode* pPortalSceneNode = pIntInst->GetPortalSceneGraphNode(portalIdx);
						if(pPortalSceneNode && pPortalSceneNode->IsEnabled())
						{
							s32 destRoomIdx = pIntInst->GetDestThruPortalInRoom(0,portalIdxRoom0);
							destLoc = CInteriorProxy::CreateLocation(pIntInst->GetProxy(), destRoomIdx);
							bProcessing = false;	//bail out of further tests - portal traversal should be unambiguous
						}
					}
				}

				if (pTracker->GetOwner() == CGameWorld::FindLocalPlayer()){
					if (!pPortalInst->IsLinkPortal() && !pPortalInst->GetIsOneWayPortal() && !(pIntInst->IsMirrorPortal(0,portalIdxRoom0))){
						ms_pPlayerProximityPortal = pPortalInst;
					}
				}
			}
		}

		pNode = pNode->GetNextPtr();
	}
}

u32 CPortalTracker::MoveToNewLocation(fwInteriorLocation destLoc)
{

#if __BANK
	if ( g_breakOnSelectedMoveToNewLocation &&
	    (m_pOwner == g_PickerManager.GetSelectedEntity() || pDebugTracker == this))
	{
		__debugbreak();
	}

	if (g_breakOnSelectedEntityHavingDifferentInteriorLoc &&
		((m_pOwner && m_pOwner == g_PickerManager.GetSelectedEntity()) || pDebugTracker == this))
	{
		if (destLoc.GetAsUint32() != gDebugBreakLocation.GetAsUint32())
			__debugbreak();
	}

#endif

	fwInteriorLocation	sourceLoc = CInteriorProxy::CreateLocation( m_pInteriorProxy, m_roomIdx );

	u32 retCode = eNetIntUpdateSameLocation;

	if ( ( m_pOwner && m_pOwner->GetIsRetainedByInteriorProxy() ) || !destLoc.IsSameLocation(sourceLoc) )
	{
		retCode = eNetIntUpdateNullEntity;

		//handy things...
//		if (IsVisPortalTracker())
//		{
//			Displayf("camera");
//		}

		CEntity* pEntity = GetOwner();
		if (pEntity)
		{
			//handy things...
//
// 			if (CPedFactory::GetFactory()->GetLocalPlayer() == pEntity)
// 			{
// 				Displayf("player");
// 			}

			retCode = eNetIntUpdateOk;

			// Need to cache the fwInteriorsSettings for audio because it's unsafe to get it from the CEntity on the northaudioupdate thread
			pEntity->SetAudioInteriorLocation(destLoc);

			if (!destLoc.IsValid()){
				retCode |= eNetIntUpdateAddToWorld;
				CGameWorld::RemoveAndAdd(pEntity, destLoc, true);
			} else {
				CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation( destLoc );
				if (AssertVerify(pInteriorProxy))
				{
					CInteriorInst* pIntInst = pInteriorProxy->GetInteriorInst();

					//Register with destination location (if available), otherwise retain it at the proxy for destination
					if (pIntInst && pIntInst->CanReceiveObjects())
					{
						retCode |= eNetIntUpdateAddToWorld;
						CGameWorld::RemoveAndAdd(pEntity, destLoc, true);
					}
					else
					{
						retCode |= eNetIntUpdateAddToRetainList;
						CGameWorld::Remove(pEntity, true);
						pInteriorProxy->MoveToRetainList( pEntity );
						return retCode;
					}
				}
			}
		}

		//Update this portal tracker
		if ( CInteriorInst::GetInteriorForLocation( sourceLoc ) )
		{
			retCode |= eNetIntUpdatePortalTracker;
			CInteriorInst::GetInteriorForLocation( sourceLoc )->RemoveFromActiveTrackerList( this );
		}

		CInteriorInst* pDestIntInst = CInteriorInst::GetInteriorForLocation( destLoc );
		m_pInteriorProxy = CInteriorProxy::GetFromLocation(destLoc);
		m_roomIdx = destLoc.GetRoomIndex();

		if ( pDestIntInst )
		{
			retCode |= eNetIntUpdateHasDestIntInst;
			pDestIntInst->AddToActiveTrackerList( this );
			CPortal::AddToActiveInteriorList( pDestIntInst );
			CPortal::ManageInterior( pDestIntInst );
		}
		else
		{
			retCode |= eNetIntUpdateNoDestIntInst;
		}
	}

	return retCode;
}

void CPortalTracker::MoveToNewLocation(CInteriorInst* pNewInterior, s32 roomIdx){
	fwInteriorLocation	destLoc = CInteriorInst::CreateLocation( pNewInterior, roomIdx );
	MoveToNewLocation( destLoc );
}

bool gbForceReScan = false;
bool gbForcePlayerRescan = false;
REPLAY_ONLY(bool gbForceReScanForReplay = false;)

// don't attempt to do proper portal tracking beyond this speed (m/frame) as it will
// cause huge amounts of interior activation!
#define SPEED_LIMIT	(25.0f)

#if __BANK

#define PtDebug(msg,...)		{ if (PtUpdateDebug::g_spewEnabled) visibilityDebugf1( "[pt update] " msg, ##__VA_ARGS__ ); }

namespace PtUpdateDebug
{
	enum {
		LOG_TYPE_GBUF_VIS_TRACKER,
		LOG_TYPE_LOCAL_PLAYER,
		LOG_TYPE_SELECTED_ENTITY,
		LOG_TYPE_PARAM_MODELNAME
	};

	bool		g_spewEnabled;

	int			g_jsonLogId;
	int			g_jsonLogType;
	CEntity*	g_jsonLogEntity;
	Vector3		g_jsonLogCurrPos;
	Vector3		g_jsonLogPos;
	bool		g_jsonLogRescanned;

	struct EndGuard
	{
		~EndGuard()
		{
			if ( g_spewEnabled )
			{
				static const char*	typeCaptions[] = { "gbuf", "localplayer", "selected", "modelname" };

				++g_jsonLogId;
				PtDebug( "json log entry: { \"id\" : %d, \"type\" : \"%s\", \"entity\" : %p, \"currPos\" : [%f,%f,%f], \"pos\" : [%f,%f,%f], \"rescanned\" : %s }",
					g_jsonLogId,
					typeCaptions[ g_jsonLogType ],
					g_jsonLogEntity,
					g_jsonLogCurrPos.x, g_jsonLogCurrPos.y, g_jsonLogCurrPos.z,
					g_jsonLogPos.x, g_jsonLogPos.y, g_jsonLogPos.z,
					g_jsonLogRescanned ? "true" : "false" );

				PtDebug( "---------- End portal tracker update" );
			}
		}
	};

	atString IntLocPrinter(const fwInteriorLocation intLoc)
	{
		char		buf[128];

		if ( !intLoc.IsValid() )
			formatf( buf, 128, "<exterior>" );
		else
		{
			const char*		interiorName = CInteriorProxy::GetFromLocation( intLoc )->GetModelName();

			if ( intLoc.IsAttachedToRoom() )
				formatf( buf, 128, "<interior %s, room %d>", interiorName, intLoc.GetRoomIndex() );
			else
				formatf( buf, 128, "<interior %s, portal %d>", interiorName, intLoc.GetPortalIndex() );
		}

		return atString(buf);
	}
}

void CPortalTracker::SprintfCallstack(size_t addr, const char *sym, size_t offset)
{
	sprintf(ms_callstackText, "%s%" SIZETFMT "x - %s+%" SIZETFMT "x\n", ms_callstackText, addr, sym, offset);
}

void CPortalTracker::CallstackPrintStackInfo(u32 /*tagCount*/, u32 /*stackCount*/, const size_t* stack, u32 stackSize)
{
	sysStack::PrintCapturedStackTrace( stack, stackSize , SprintfCallstack);
}

#else
#define PtDebug(msg,...)
#endif

#if __BANK
struct SafeSphereDisplay
{
	SafeSphereDisplay(CPortalTracker *pTracker) : m_pTracker(pTracker){}
	~SafeSphereDisplay()
	{
		if( bShowPortalTrackerSafeZoneSpheres ||
		   (bShowPortalTrackerSafeZoneSphereForSelected && ((m_pTracker->m_pOwner && m_pTracker->m_pOwner == g_PickerManager.GetSelectedEntity()) || m_pTracker == pDebugTracker)))

		{
			grcDebugDraw::Sphere(m_pTracker->m_portalsSafeSphere.GetCenter(), m_pTracker->m_portalsSafeSphere.GetRadiusf(), Color32(255,0,0,100));
			grcDebugDraw::Sphere(m_pTracker->m_portalsSafeSphere.GetCenter(), m_pTracker->m_portalsSafeSphere.GetRadiusf(), Color32(0,255,0,255), false);
		}
	}

	CPortalTracker *m_pTracker;
};
#endif

void CPortalTracker::TriggerForceAllActiveInteriorCapture()
{
	for (s32 i=0; i<CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy* pProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if (pProxy && !pProxy->GetGroupId() && pProxy->IsContainingImapActive() && pProxy->GetInteriorInst())
		{
			pProxy->GetInteriorInst()->CapturePedsAndVehicles();
		}
	}
}

bool CPortalTracker::IsPlayerInsideStiltApartment()
{
	return IsPedInsideStiltApartment(CGameWorld::FindLocalPlayer());
}

bool CPortalTracker::IsPedInsideStiltApartment(const CPed* pPed)
{
	if (pPed && pPed->GetIsInInterior())
	{
		fwInteriorLocation pedLoc = pPed->GetInteriorLocation();
		if (pedLoc.IsValid())
		{
			CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(pedLoc);
			if (pProxy)
			{
				u32 proxyNameHash = pProxy->GetNameHash();

				if ( (proxyNameHash == ATSTRINGHASH("apa_v_mp_stilts_a", 0xC9005949))
					|| (proxyNameHash == ATSTRINGHASH("apa_v_mp_stilts_b", 0xA6B714B7)) )
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool bVolumeDebug = false;
void SetDestLoc(u32 OUTPUT_ONLY(idx), fwInteriorLocation& target, fwInteriorLocation newVal, const Vector3& OUTPUT_ONLY(pos))
{
	if (target.GetAsUint32() != newVal.GetAsUint32())
	{
		if (bVolumeDebug)
		{
			worldDebugf1("<PT DEBUG> %d : Change to fwInteriorLoc : <%d,%d> : (%.2f,%.2f,%.2f)", idx, newVal.GetRoomIndex(), newVal.GetInteriorProxyIndex(), pos.GetX(), pos.GetY(), pos.GetZ());
		}
		target = newVal;
	}
}

// update the state of the portal tracking from the new position of the owning entity & update registration in world accordingly
// (ie. check for moving thru portals)
void CPortalTracker::Update(const Vector3& pos) {

	Assert(CheckConsistency());

#if __BANK

	// the MP carrier has a peculiar problem where the 2 hangars meet up
	//if ( pLocalPlayer && (pLocalPlayer != m_pOwner) && pLocalPlayer->GetIsInInterior() && NetworkInterface::IsGameInProgress())
	if (m_pOwner && m_pOwner->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pOwner);

		if (pPed->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT || 	pPed == CGameWorld::FindLocalPlayer())
		{
			static spdAABB carrierHangarVolumeDebugBox(Vec3V(3062.0f, -4686.0f,  7.0f), Vec3V(3073.0f, -4673.0f, 12.0f));
			Vec3V pos = VECTOR3_TO_VEC3V(m_currPos);
			if (carrierHangarVolumeDebugBox.ContainsPoint(pos))
			{
				bVolumeDebug = true;
			}
		}
	}
#endif //__BANK

#if __BANK
	SafeSphereDisplay(this);

	// trace selected entity
	if ((m_pOwner && g_breakOnSelectedEntityInPortalTrackerUpdate && m_pOwner == g_PickerManager.GetSelectedEntity()) ||
		(g_breakOnDebugPortalTrackerAddressInPortalTrackerUpdate  && pDebugTracker == this))
	{
		__debugbreak();
	}

	if (ms_entityTypeToCollectCallStacksFor != -1 &&
		((m_pOwner && m_pOwner == g_PickerManager.GetSelectedEntity()) ||
		(!m_pOwner && ms_entityTypeToCollectCallStacksFor == traceTypeCamera && ms_pTraceVisTracker == this)))
	{
		bool selectedNewEntity = ms_lastRenderPortalTrackerForCallstack != this;

		if (m_callstackTrackerTag != INVALID_HASHTAG)
		{
			const TrackProbeEntry *pEntry = ms_ptTrackedProbes->Access(m_callstackTrackerTag);
			if (pEntry && (selectedNewEntity || gPTDebugEntry.frameCount != pEntry->frameCount))
			{
				gPTDebugEntry = *pEntry;
			}
		}
		else
		{
			gPTDebugEntry = TrackProbeEntry();
		}

		if (ms_lastRenderedCallstackCount != m_callstackCount || selectedNewEntity)
		{
			memset(ms_callstackText, 0, PT_CALLSTACK_BUFFERS_SIZE);
			if (m_callstackTrackerTag != INVALID_HASHTAG)
			{
				// Capture call stack text
				ms_pStackCollector->PrintInfoForTag(m_callstackTrackerTag, CallstackPrintStackInfo);
				ms_pStackCollector->PrintInfoForTag(m_callstackTrackerTag);
			}
			else
			{
				strcpy(ms_callstackText, "No Call Stack Recorded");
				Displayf("%s",ms_callstackText);
				memset(&gPTDebugEntry, 0, sizeof(TrackProbeEntry));
			}
			ms_callstackText[PT_CALLSTACK_BUFFERS_SIZE - 1] = '\0';

			ms_lastRenderedCallstackCount			 = m_callstackCount;
			ms_lastRenderPortalTrackerForCallstack = this;
		}
	}
	if (m_setownerCalled == 1)
	{
		if (m_pOwner->GetType() == ms_entityTypeToCollectCallStacksFor)
		{
			USE_DEBUG_MEMORY();
			u32 uThis = u32((size_t)this);
			atVarString hashString("%s%d", m_pOwner->GetModelName(), uThis);
			m_callstackTrackerTag = atStringHash(hashString.c_str());
		}
		m_setownerCalled = 0;
	}

	if (m_pOwner && m_pOwner == g_PickerManager.GetSelectedEntity())
	{
		if (g_bToggleSelectedFromActivatingTrackerList)
		{
			if (m_bActivatingTracker)
			{
				RemoveFromActivatingTrackerList(this);
			}
			else
			{
				AddToActivatingTrackerList(this);
			}
			g_bToggleSelectedFromActivatingTrackerList = false;
		}

		if (g_bToggleSelectedLoadingCollisions)
		{
			SetLoadsCollisions(!m_bLoadsCollisions);
			g_bToggleSelectedLoadingCollisions = false;
		}

	}

	if (ms_ptPositionHistory && ms_entityTypeToCollectCallStacksFor != -1)
	{
		PositionHistory *pHistory = ms_ptPositionHistory->Access(m_callstackTrackerTag);
		if (!pHistory)
		{
			ms_ptPositionHistory->Insert(m_callstackTrackerTag);
			pHistory = ms_ptPositionHistory->Access(m_callstackTrackerTag);
			pHistory->pTracker = this;
		}
		if (pHistory)
		{
			int nextIndex = pHistory->nextIndex * 3;
			pHistory->position[nextIndex]	  = pos.x;
			pHistory->position[nextIndex + 1] = pos.y;
			pHistory->position[nextIndex + 2] = pos.z;

			++pHistory->nextIndex;
			if (pHistory->nextIndex >= POSITION_HISTORY_SIZE)
			{
				pHistory->nextIndex = 0;
			}
		}
	}
#endif // __BANK

#if __BANK
	if ( bDontPerformPtUpdate )
		return;

	bool enableStraddlingPortals = *fwScanNodesDebug::GetPortalStraddlingEnabled();
#else
	const bool enableStraddlingPortals = true;
#endif

#if __BANK
	const char*		debugModel;
	const bool		debugModelPresent = PARAM_portaltrackerdebugmodel.Get( debugModel );

	if ( debugModelPresent )
	bPtUpdateDebugSpew = true;

	PtUpdateDebug::g_spewEnabled = false;
	if ( bPtUpdateDebugSpew )
	{
		if ( this->IsVisPortalTracker() && this == g_SceneToGBufferPhaseNew->GetPortalVisTracker() )
		{
			PtUpdateDebug::g_spewEnabled = true;
			PtUpdateDebug::g_jsonLogType = PtUpdateDebug::LOG_TYPE_GBUF_VIS_TRACKER;
			PtUpdateDebug::g_jsonLogEntity = NULL;

			PtDebug( "++++++++++ Starting portal tracker update for Gbuf portal tracker" );
		}
		else if ( m_pOwner && m_pOwner == CGameWorld::FindLocalPlayer() )
		{
			PtUpdateDebug::g_spewEnabled = true;
			PtUpdateDebug::g_jsonLogType = PtUpdateDebug::LOG_TYPE_LOCAL_PLAYER;
			PtUpdateDebug::g_jsonLogEntity = m_pOwner;

			PtDebug( "++++++++++ Starting portal tracker update for entity %s (%p)", m_pOwner->GetModelName(), m_pOwner );
			}
		else if ( m_pOwner && m_pOwner == g_PickerManager.GetSelectedEntity() )
		{
			PtUpdateDebug::g_spewEnabled = true;
			PtUpdateDebug::g_jsonLogType = PtUpdateDebug::LOG_TYPE_SELECTED_ENTITY;
			PtUpdateDebug::g_jsonLogEntity = m_pOwner;

			PtDebug( "++++++++++ Starting portal tracker update for entity %s (%p)", m_pOwner->GetModelName(), m_pOwner );
		}
		else if ( m_pOwner && debugModelPresent && !stricmp( debugModel, m_pOwner->GetModelName() ) )
		{
			PtUpdateDebug::g_spewEnabled = true;
			PtUpdateDebug::g_jsonLogType = PtUpdateDebug::LOG_TYPE_PARAM_MODELNAME;
			PtUpdateDebug::g_jsonLogEntity = m_pOwner;

			PtDebug( "++++++++++ Starting portal tracker update for entity %s (%p)", m_pOwner->GetModelName(), m_pOwner );
		}
	}

	PtUpdateDebug::EndGuard		ptUpdateEndGuard;
#endif

	if (m_pOwner && m_pOwner->GetIsRetainedByInteriorProxy())
	{
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot( m_pOwner->GetRetainingInteriorProxy() );
		if (pIntProxy)
		{
			spdSphere boundingSphere;
			pIntProxy->GetBoundSphere(boundingSphere);
			if (!boundingSphere.ContainsPoint(m_pOwner->GetTransform().GetPosition()))
			{
				// retained entity is well outside the range of the interior, so perform emergency eviction
				Displayf("Emergency eviction of %s from %s retain list!", m_pOwner->GetModelName(), pIntProxy->GetName().GetCStr());
				pIntProxy->RemoveFromRetainList(m_pOwner);
				CGameWorld::Add(m_pOwner, CGameWorld::OUTSIDE, true);
			}
		}
	}

	// [HACK_GTAV]
	if (ms_bEnableStadiumProbesThisFrame || NetworkInterface::IsGameInProgress() REPLAY_ONLY(|| CReplayMgr::IsReplayInControlOfWorld()))
	{
		CProjectile* pProjectile=NULL;
		if (m_pOwner && m_pOwner->GetIsTypeObject())
		{
			pProjectile = (static_cast<CObject*>(m_pOwner))->GetAsProjectile();
		}

		if	(IsVisPortalTracker() || 
			(m_pOwner && (m_pOwner->GetIsTypePed() || m_pOwner->GetIsTypeVehicle() || m_pOwner->GetOwnedBy()==ENTITY_OWNEDBY_SCRIPT)) || 
			pProjectile!=NULL)
		{
			// bug 5206668 - stadium interior is super tall and has lots of stuff placed in it...
			{
				static spdAABB v_arena_volume(Vec3V(2608.0f, -3920.0f, 90.0f), Vec3V(2989.0f, -3682.0f, 250.0f));
				if (v_arena_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_STADIUM;
				}
			}
		}
	}

	if (m_pOwner && m_pOwner->GetIsRetainedByInteriorProxy())
	{
		// bug 5206668 - stadium interior is super tall and has lots of stuff placed in it...
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot( m_pOwner->GetRetainingInteriorProxy() );
		if (pIntProxy)
		{
			if ( (pIntProxy->GetNameHash() == ATSTRINGHASH("xs_arena_interior", 0x37e9f641)) )
			{
				m_bProbeType = PROBE_TYPE_STADIUM;
			}
		}
	}

	if (m_pOwner && m_pOwner->GetIsRetainedByInteriorProxy())
	{
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot( m_pOwner->GetRetainingInteriorProxy() );
		if (pIntProxy)
		{
			if ( (pIntProxy->GetNameHash() == ATSTRINGHASH("v_hanger", 0x6e23f273)) )
			{
				m_bProbeType = PROBE_TYPE_FAR;
			}
		}
	}

	if (m_pOwner && m_pOwner->GetIsRetainedByInteriorProxy())
	{
		CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetSlot( m_pOwner->GetRetainingInteriorProxy() );
		if (pIntProxy)
		{
			if ( (pIntProxy->GetNameHash() == ATSTRINGHASH("sm_smugdlc_int_01", 0x261589c6)))
			{
				m_bProbeType = PROBE_TYPE_SUPER_FAR;
			}
		}
	}

	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if (pLocalPlayer && m_pOwner == pLocalPlayer)
	{
		if (pLocalPlayer->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting))
		{
			// rescanning defeats portal traversal - don't do it whilst parachuting! Parachute retries usually teleport, setting the scan flag...
			m_bScanUntilProbeTrue = false;
			m_bRequestRescanNextUpdate = false;
		}

		if (pLocalPlayer->GetIsInInterior())
		{
			fwInteriorLocation playerLoc = pLocalPlayer->GetInteriorLocation();
			CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(playerLoc);
			if (pProxy)
			{
				if ( (pProxy->GetNameHash() == ATSTRINGHASH("v_coroner", 0xabfdc0b6)) )
				{
					if (playerLoc.GetRoomIndex() == 10)
					{
						// this room contains bad collision data, defeat probing whilst in it...
						m_bScanUntilProbeTrue = false;
						m_bRequestRescanNextUpdate = false;
					}
				}

				// bug  - molten metal pool has wrong collision IDs under it
				if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("v_foundry", 0x05314b05))
				{
					static spdAABB moltenPool(Vec3V(1084.3f, -2006.5f, 30.0f), Vec3V(1088.7f, -2001.4f, 32.5f));
					if (moltenPool.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
					{
						m_bRequestRescanNextUpdate = false;
						m_bScanUntilProbeTrue = false;
					}
				}


				// bug  - probes on metro track in station are a bit 'fail-ly'
				if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("metro_station_3_seoul", 0xa8c98507))
				{
					static spdAABB metroTrack(Vec3V(-305.3f, -350.3f, 8.0f), Vec3V(-300.0f, -314.4f, 9.3f));
					if (metroTrack.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
					{
						m_bRequestRescanNextUpdate = false;
						m_bScanUntilProbeTrue = false;
					}
				}

				// bug  - tunnel section with badly tagged collision IDs
				if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("metro_map_join_1", 0xfcbd0736))
				{
					static spdAABB badtunnel(Vec3V(-185.0f, -882.0f, 19.7f), Vec3V(-161.7f, -848.4f, 26.7f));
					if (badtunnel.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
					{
						m_bRequestRescanNextUpdate = false;
						m_bScanUntilProbeTrue = false;
					}
				}

				// bug 2275003 - heists ornate bank doorway with badly tagged collision IDs
				if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("hei_heist_ornate_bank", 0xe65ef99d))
				{
					static spdAABB badDoorway(Vec3V(264.9f, 217.0f, 109.0f), Vec3V(266.5f, 218.3f, 110.7f));
					if (badDoorway.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
					{
						m_bRequestRescanNextUpdate = false;
						m_bScanUntilProbeTrue = false;
					}
				}

				// bug 1951225 -water pool in lab causes drop out if player ragdolls over it.
				if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("v_lab", 0xa6091244))
				{
					if (playerLoc.GetRoomIndex() == 14)
					{
						static spdAABB noprobe(Vec3V(3529.2f, 3709.1f, 15.0f), Vec3V(3533.8f, 3716.7f, 21.7f));
						if (noprobe.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
						{
							m_bRequestRescanNextUpdate = false;
							m_bScanUntilProbeTrue = false;
						}
					}
				}


				// bug 2508585 -Local player can clip through the wall and become invisible to remote players
				if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("id1_11_tunnel3_int",0x9d190cc7))
				{
					if (playerLoc.GetRoomIndex() == 1)
					{
						static spdAABB tunnelSide(Vec3V(819.0f, -1824.2f, 19.9f), Vec3V(828.1f, -1805.1f, 23.8f));
						if (tunnelSide.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
						{
							m_bRequestRescanNextUpdate = false;
							m_bScanUntilProbeTrue = false;
						}
					}
				}

				// bug 2690743 -Popping out of cover on the edge of a portal then changing to 1st person cause the world to drop out.  
				if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("v_31_tun_01",0x96573810))
				{
					if (playerLoc.GetRoomIndex() == 1)
					{
						static spdAABB tunnelSide(Vec3V(-58.8f, -543.5f, 34.0f), Vec3V(-59.6f, -543.7f, 28.0f));
						if (tunnelSide.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
						{
							m_bRequestRescanNextUpdate = false;
							m_bScanUntilProbeTrue = false;
						}
					}
				}
			}
		}
		else
		{
			// bug 2639950 - Beast vs Slasher V - Map escape/world drops out after player ragdolls onto a path in Humane Labs.
			static spdAABB labArea(Vec3V(3590.0f,3722.62f,34.6f), Vec3V(3608.0f,3736.0f,40.0f));
			if (labArea.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
			{
				m_bRequestRescanNextUpdate = false;
				m_bScanUntilProbeTrue = false;
			}

			// bug 2761117, 2806204 - Carrier deck - falling on deck can place player into hangar under deck
			static spdAABB deckArea(Vec3V(3032.0f,-4818.0f,14.1f), Vec3V(3114.0f,-4724.0f,15.5f));
			if (deckArea.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
			{
				m_bRequestRescanNextUpdate = false;
				m_bScanUntilProbeTrue = false;
			}
		}
	}

	if( IsVisPortalTracker() )
	{
		CInteriorProxy *pProxy = CPortalVisTracker::GetPrimaryInteriorProxy();
		if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("v_lab", 0xa6091244))
		{
			if (CPortalVisTracker::GetPrimaryRoomIdx() == 14)
			{
				static spdAABB noprobe(Vec3V(3516.2f, 3709.1f, 15.0f), Vec3V(3533.8f, 3726.0f, 26.0f));
				if (noprobe.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bRequestRescanNextUpdate = false;
					m_bScanUntilProbeTrue = false;
				}
			}
		}

		// bug 5994002, 6145804 - normal probes in cutscene here aren't long enough to hit the ground
		//if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("ch_dlc_casino_shaft", 0x4a6709a7))
		{
			if (m_bIsCutsceneControlled || m_bRequestRescanNextUpdate || m_bScanUntilProbeTrue)
			{
				static spdAABB v_casinoLiftShaft_volume(Vec3V(2569.0, -258.0f, -152.0f), Vec3V(2578.0f, -252.0f, -58.0f));
				if (v_casinoLiftShaft_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_STADIUM;			// longest probe we've got...
				}
			}
		}
	}

	// special cases...
	if ( pLocalPlayer && pLocalPlayer->GetIsInInterior())
	{
		fwInteriorLocation playerLoc = pLocalPlayer->GetInteriorLocation();
		CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(playerLoc);

		// bug 1789662 - crappy collision data in this interior
		if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("v_apart_midspaz", 0xaa8789c1))
		{
			if (m_pOwner)
			{
				if (m_pOwner->GetArchetype()->GetModelNameHash() == ATSTRINGHASH("p_tumbler_cs2_s",0xa49287f8) ||
					(m_pOwner->GetArchetype()->GetModelNameHash() == ATSTRINGHASH("p_whiskey_bottle_s",0x1d18abd6)))
				{
					m_bRequestRescanNextUpdate = false;
					m_bScanUntilProbeTrue = false;
				}
			}
		}

		// bug 1832149 - normal probes in here aren't long enough to hit the ground
		if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("v_31_tun_swap", 0xb8d94a4f))
		{
			if (m_pOwner && m_bScanUntilProbeTrue)
			{
				static spdAABB v31Tun_platform(Vec3V(-10.0f, -678.9f, 13.1f), Vec3V(21.4f, -649.0f, 23.9f));
				if (v31Tun_platform.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_FAR;
				}

				// bug 2190367 - somewhere else in the same interior
				static spdAABB v31Tun_platform2(Vec3V(15.1f, -630.9f, 12.5f), Vec3V(27.6f, -622.0f, 15.1f));
				if (v31Tun_platform2.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_FAR;
				}
			}
		}

		// bug 1997229 - normal probes in here aren't long enough to hit the ground
		if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("v_hanger", 0x6e23f273))
		{
			if (m_pOwner && m_bScanUntilProbeTrue)
			{
				static spdAABB v_hanger_volume(Vec3V(-1037.0f, -3047.0f, 14.3f), Vec3V(-930.5f, -3001.0f, 23.7f));
				if (v_hanger_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_FAR;
				}
			}
		}

		if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("sm_smugdlc_int_01", 0x261589c6))
		{
			if (m_pOwner && m_bScanUntilProbeTrue)
			{
				static spdAABB v_smugHangar_volume(Vec3V(-1317.0f, -3070.0f, -50.0f), Vec3V(-1217.5f, -2956.0f, -10.0f));
				if (v_smugHangar_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_SUPER_FAR;
				}
			}
		}

		// bug 6103651 - normal probes in here aren't long enough to hit the ground
		if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("ch_dlc_casino_hotel", 0x8aa39877))
		{
			if (m_pOwner && (m_bIsCutsceneControlled || m_bRequestRescanNextUpdate || m_bScanUntilProbeTrue))
			{
				static spdAABB v_casinoStairwell_volume(Vec3V(2503.0, -265.0f, -57.0f), Vec3V(2508.0f, -261.3f, -33.0f));
				if (v_casinoStairwell_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_STADIUM;			// longest probe we've got...
				}
			}
		}
	}

	if (m_pOwner && m_pOwner->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pOwner);

		// metro entrances cause a bunch of problems with police trying to get to the player underground
		if (pPed->IsLawEnforcementPed() &&
			pLocalPlayer->GetPlayerWanted() &&
			(pLocalPlayer->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN))
			//			if (pPed->GetPedIntelligence() && (pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == pLocalPlayer))
		{
			static spdAABB metroTransitionBox1(Vec3V(-1364.0f, -528.0f, 22.0f), Vec3V(-1344.0f, -500.0f, 27.0f));
			static spdAABB metroTransitionBox2(Vec3V(-838.0f, -109.0f, 27.0f), Vec3V(-830.0f, -96.0f, 29.0f));
			static spdAABB metroTransitionBox3(Vec3V(-495.0f, -739.0f, 22.0f), Vec3V(-482.0f, -726.0f, 27.0f));
			Vec3V pos = VECTOR3_TO_VEC3V(m_currPos);
			if (metroTransitionBox1.ContainsPoint(pos) ||
				metroTransitionBox2.ContainsPoint(pos) ||
				metroTransitionBox3.ContainsPoint(pos))
			{
				m_bScanUntilProbeTrue = true;
			}
		}

		// the MP carrier has a peculiar problem where the 2 hangars meet up
		if ( pLocalPlayer && (pLocalPlayer != m_pOwner) && pLocalPlayer->GetIsInInterior() && NetworkInterface::IsGameInProgress())
		{
			static spdAABB carrierHangarTransitionBox(Vec3V(3064.2f, -4682.9f,  9.6f), Vec3V(3076.3f, -4670.0f, 12.0f));
			Vec3V pos = VECTOR3_TO_VEC3V(m_currPos);
			if (carrierHangarTransitionBox.ContainsPoint(pos))
			{
				m_bScanUntilProbeTrue = true;
			}
		}

		// bug 3996255 - normal probes in here aren't long enough to hit the ground
		{
			if (m_bScanUntilProbeTrue)
			{
				static spdAABB xm_x17dlc_volume(Vec3V(466.0f, 4754.0f, -53.0f), Vec3V(502.0f, 4858.0f, -40.0f));
				if (xm_x17dlc_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_FAR;
				}
			}
		}

		// bug 3930957 - normal probes in here aren't long enough to hit the ground
		{
			if (m_bScanUntilProbeTrue)
			{
				static spdAABB v_foundry_volume(Vec3V(1059.0f, -2031.0f, 28.0f), Vec3V(1132.5f, -1974.0f, 47.0f));
				if (v_foundry_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_FAR;
				}
			} else
			{
				// foundry seems *really* broken around here
				static spdAABB v_foundry_small_volume(Vec3V(1092.0f, -2026.0f, 35.0f), Vec3V(1128.0f, -2010.1f, 37.4f));
				if (v_foundry_small_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bScanUntilProbeTrue = true;
					m_bProbeType = PROBE_TYPE_FAR;
				}
			}
		}

		// bug 4203021 - normal probes around lift in here aren't long enough to hit the ground
		if (m_bScanUntilProbeTrue)
		{
			static spdAABB v_facility_lift_vol(Vec3V(2151.0f, 2918.0f, -85.0f), Vec3V(2159.0, 2925.0f, -53.0f));
			if (v_facility_lift_vol.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
			{
				m_bProbeType = PROBE_TYPE_FAR;
			}
		} 

		// bug 5514882 - normal probes in here aren't long enough to hit the ground
		if (m_bScanUntilProbeTrue)
		{
			static spdAABB xs_x18_int_mod_volume(Vec3V(193.0f, 5161.0f, -87.0f), Vec3V(211.3f, 5170.0f, -83.0f));
			if (xs_x18_int_mod_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
			{
				m_bProbeType = PROBE_TYPE_FAR;
			}
		}

		// bug 5994002, 6145804 - normal probes in here aren't long enough to hit the ground
		{
			if ((m_bIsCutsceneControlled || m_bRequestRescanNextUpdate || m_bScanUntilProbeTrue))
			{
				static spdAABB v_casinoLiftShaft_volume(Vec3V(2569.0, -258.0f, -152.0f), Vec3V(2578.0f, -252.0f, -58.0f));
				if (v_casinoLiftShaft_volume.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_STADIUM;			// longest probe we've got...
				}
			}
		}

		// bug 6194787 -water pool in v_lab can causing interior state de-syncing between players due to required probe length
		if ( NetworkInterface::IsGameInProgress() && m_pOwner && m_pOwner->GetIsTypePed() && (m_bRequestRescanNextUpdate || m_bScanUntilProbeTrue))
		{
				static spdAABB poolProbe(Vec3V(3528.0f, 3709.1f, 10.0f), Vec3V(3533.8f, 3721.7f, 21.7f));
				if (poolProbe.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
				{
					m_bProbeType = PROBE_TYPE_SUPER_FAR;
				}
		}

	}

	// end [HACK_GTAV]

	bool mustRescan = m_bScanUntilProbeTrue || m_bRequestRescanNextUpdate || gbForceReScan || m_bCaptureForInterior; // REPLAY_ONLY(|| gbForceReScanForReplay);
	mustRescan |= (gbForcePlayerRescan && (m_pOwner == pLocalPlayer));

	if (m_bWaitForAllCollisionsBeforeProbe)
	{
		const bool allCollisionsLoaded = g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(RCC_VEC3V(pos), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);

		if (allCollisionsLoaded)
		{
			m_bWaitForAllCollisionsBeforeProbe = false;
			mustRescan = true;
		}
	}

	// prevent these objects from enteringing interiors
	if (m_bIsExteriorOnly)
	{
		m_bScanUntilProbeTrue = false;
		m_bRequestRescanNextUpdate = false;
		mustRescan = false;
		m_bCaptureForInterior = false;
	}

	// Performing normal (i.e. incremental, movement-based) portal tracking is not meaningful for retained
	// entities, since they are not really surrounded by an interior with portals that they can traverse.
	// However, sometimes retained entities are teleported to somewhere else (usually requesting a probe to
	// see where they ended up) - in that case we have to process the portal tracking for them.
	// The following early-exit condition accomodates for these two cases. - Ale
	if (m_pOwner && m_pOwner->GetIsRetainedByInteriorProxy() && !mustRescan)
	{
		PtDebug( "Retained entity and no probe requested, skipping" );
		return;
	}

	PtDebug( "pos = (%.3f,%.3f,%.3f), m_oldPos = (%.3f,%.3f,%.3f), m_currPos = (%.3f,%.3f,%.3f)", pos.x, pos.y, pos.z, m_oldPos.x, m_oldPos.y, m_oldPos.z, m_currPos.x, m_currPos.y, m_currPos.z );

	// first call to update simply establishes initial position
	if (!m_bUpdated){
		PtDebug( "m_Updated is false, setting m_oldPos and m_currPos position to pos" );
		m_bUpdated = true;
		m_currPos = pos;
#if RSG_PC
		m_currPosObfuscated = pos;
#endif
		m_oldPos = pos;
	}

	BANK_ONLY( PtUpdateDebug::g_jsonLogCurrPos = m_currPos; PtUpdateDebug::g_jsonLogPos = pos; )

	// things like object attached to portals aren't going anywhere - don't let them update
	if (m_bDontProcessUpdates)
	{
		PtDebug( "Updates processing disabled, skipping" );
		return;
	}

	if (m_bLoadsCollisions){
		// trackers which load collisions need to keep the interior active, even if not moving
		if (m_pInteriorProxy && m_pInteriorProxy->GetInteriorInst()){
			m_pInteriorProxy->GetInteriorInst()->m_bInUse = true;
		}
	}

	// don't bother updating for very small movements
	Vector3 delta = pos-m_currPos;

	PtDebug( "delta = (%.3f,%.3f,%.3f)", delta.x, delta.y, delta.z );

	bool canStraddlePortals = false;
	bool wasStraddlingPortal = false;
	if (m_pOwner && m_pOwner->GetIsDynamic())
	{
		CDynamicEntity *pDE = static_cast<CDynamicEntity*>(m_pOwner);
		canStraddlePortals = pDE->CanStraddlePortals();
		wasStraddlingPortal = pDE->IsStraddlingPortal();
	}

	if (BANK_ONLY(!g_ignoreSmallDeltaEarlyOut &&) !mustRescan && delta.Mag2() < 0.0001f)
	{
		PtDebug( "Small delta and no rescan requested, skipping (current location of the tracker: %s )", PtUpdateDebug::IntLocPrinter(CInteriorProxy::CreateLocation(m_pInteriorProxy, m_roomIdx)).c_str() );
		return;
	}

	if (GetOwner() && !GetOwner()->GetIsReadyForInsertion()){
		PtDebug( "Not ready for insertion, skipping");
		return;
	}

	// update the stored position in portal tracker
	m_oldPos = m_currPos;
	SetCurrentPos(pos);

	PtDebug( "m_oldPos and m_currPos updated, now m_oldPos = (%.3f,%.3f,%.3f), m_currPos = (%.3f,%.3f,%.3f)", m_oldPos.x, m_oldPos.y, m_oldPos.z, m_currPos.x, m_currPos.y, m_currPos.z );

	/* *** establish what the destination location is for this tracker by either checking against collision mesh, or checking against portals to
		determine which room and interior we have moved into *** */

	// final destination of the tracker (the location where the update which tests for portal traversal ends)
	fwInteriorLocation destLoc = CInteriorProxy::CreateLocation(m_pInteriorProxy, m_roomIdx);		// start by assuming the destination location is the current location

	// source location of this tracker
	fwInteriorLocation sourceLoc = CInteriorProxy::CreateLocation(m_pInteriorProxy, m_roomIdx);

	// the location where the update which tests for portal traversal start from
	fwInteriorLocation updateStartLoc = sourceLoc;				// usually start traversal from the old position

	bool bSuccessfulProbe = false;
	bool bSkipPortalTraversal = false;
	bool goneOutOfMap = false;

	PtDebug( "Current location of the tracker: %s", PtUpdateDebug::IntLocPrinter(sourceLoc).c_str() );
	BANK_ONLY( PtUpdateDebug::g_jsonLogRescanned = false; )

#if 0 //__ASSERT
	if (!mustRescan && !m_bIgnoreHighSpeedDeltaTest && m_oldPos != Vector3(Vector3::ZeroType))
	{
		Assertf(delta.Mag() < SPEED_LIMIT, "Entity %s moved faster than the Speed limit of 25m a frame, %f", m_pOwner ? m_pOwner->GetModelName() : "No Owner", delta.Mag());
	}
#endif
#if __BANK
	if (m_pOwner && m_pOwner == g_PickerManager.GetSelectedEntity() && g_IgnoreSpeedTestForSelectedEntity)
	{
		m_bIgnoreHighSpeedDeltaTest = true;
	}
#endif
	// rescan update - don't check against portals but probe the collisions instead
	if (m_bIsCutsceneControlled || m_bIsReplayControlled || mustRescan || (!m_bIgnoreHighSpeedDeltaTest && (delta.Mag2() > (SPEED_LIMIT * SPEED_LIMIT)))
	    )
	{
		PtDebug( "Rescan requested: cutsceneControlled: %d, force: %d, scanUntilTrue: %d, requestRescan: %d, bigDelta: %d", m_bIsCutsceneControlled, gbForceReScan, m_bScanUntilProbeTrue, m_bRequestRescanNextUpdate, delta.Mag2() > (SPEED_LIMIT * SPEED_LIMIT) );
		BANK_ONLY( PtUpdateDebug::g_jsonLogRescanned = true; )

		Vector3 probePoint;
		if (m_bScanUntilProbeTrue && m_pOwner)
		{
			if(m_pOwner->GetIsTypeVehicle())
			{
				// rescanning defeats portal traversal - be very careful with it for aircraft
				CVehicle *pVehicle = static_cast<CVehicle*>(m_pOwner);
				if (pVehicle->GetIsAircraft() && pVehicle->IsInAir() && pVehicle->ContainsPlayer() && !pVehicle->IsRetainedFlagSet())
				{
					m_bScanUntilProbeTrue = false;
					m_bRequestRescanNextUpdate = true;
				}
			}
			else if(m_pOwner->GetIsTypePed())
			{
				CPed *pPed = static_cast<CPed*>(m_pOwner);
				if(pPed->IsPlayer())
				{
					CVehicle *pVehicle = pPed->GetMyVehicle();
					if(pVehicle && pVehicle->GetIsAircraft() && pVehicle->IsInAir() && !pVehicle->IsRetainedFlagSet())
					{
						m_bScanUntilProbeTrue = false;
						m_bRequestRescanNextUpdate = true;
					}
				}
			}
		}

		if (m_bCaptureForInterior)
		{
			float captureProbeDist = (float)CAPTURE_PROBE_DIST;

			// JW: heist placed object which needs capturing - but it doesn't set anywhere near the floor... (bug 2005374)
			// allow script placed objects a longer capture probe distance
			if (m_pOwner && m_pOwner->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(m_pOwner);
				if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
				{
					captureProbeDist = (float)SHORT_PROBE_DIST;

					if (NetworkInterface::IsGameInProgress())
					{
						captureProbeDist = 3.2f;			// just big enough for the fleeca bank - roof to floor
					}
				}
			}

			float probeDist = m_pOwner->GetBoundSphere().GetRadiusf() + captureProbeDist;
			bSuccessfulProbe = DetermineInteriorLocationFromCoord(updateStartLoc, &probePoint, true, probeDist);
			m_bCaptureForInterior = false;
			m_bProbeType = PROBE_TYPE_DEFAULT;
		}
		else if (m_bProbeType != PROBE_TYPE_DEFAULT)
		{
			float probeDist;
			if (m_bProbeType == PROBE_TYPE_SHORT)
			{
				probeDist = (float)SHORT_PROBE_DIST;
			}
			else if (m_bProbeType == PROBE_TYPE_NEAR)
			{
				probeDist = (float)NEAR_PROBE_DIST;
			}
			else  if (m_bProbeType == PROBE_TYPE_SUPER_FAR)
			{
				probeDist = (float)SUPER_FAR_PROBE_DIST;
			}
			else if (m_bProbeType == PROBE_TYPE_STADIUM)
			{
				probeDist = (float)STADIUM_PROBE_DIST;
			}
			else
			{
				probeDist = (float)FAR_PROBE_DIST;
			}

			// grrr - tops of trains & trucks are so far off of the ground...
			if (m_pOwner && m_pOwner->GetIsTypePed())
			{
				CPed *pPed = static_cast<CPed*>(m_pOwner);
				CPhysical *pGroundPhysical = pPed->GetGroundPhysical();
				if (pGroundPhysical && pGroundPhysical->GetIsTypeVehicle())
				{
					CVehicle *pVehicle = static_cast<CVehicle*>(pGroundPhysical);
					if (pVehicle->InheritsFromTrain() || pVehicle->InheritsFromAutomobile())
					{
						probeDist = 6.0f;
					}
				}
			}

			bSuccessfulProbe = DetermineInteriorLocationFromCoord(updateStartLoc, &probePoint, true, probeDist);
			if ((m_bRequestRescanNextUpdate && !m_bScanUntilProbeTrue)|| (m_bScanUntilProbeTrue && bSuccessfulProbe))
			{
				m_bProbeType = PROBE_TYPE_DEFAULT;
			}
		}
		else
		{
			bSuccessfulProbe = DetermineInteriorLocationFromCoord(updateStartLoc, &probePoint);
		}
#if __BANK
		if (bSuccessfulProbe && g_breakOnSelectedEntityHavingDifferentInteriorLoc &&
			(m_pOwner == g_PickerManager.GetSelectedEntity() || pDebugTracker == this))
		{
			if (updateStartLoc.GetAsUint32() != gDebugBreakLocation.GetAsUint32())
			{
				Displayf("Location Changed! - Succesful Probe %s, point %.2f, %.2f, %.2f", bSuccessfulProbe ? "true" : "false", probePoint.x, probePoint.y, probePoint.z);
				__debugbreak();

				Vector3 dummyProbePoint;
				fwInteriorLocation dummyUpdateStartLoc = sourceLoc;
				bool bDummySusccessfulProbe = DetermineInteriorLocationFromCoord(dummyUpdateStartLoc, &dummyProbePoint);
				Displayf("Subsequent probe - Succesful %s, point %.2f, %.2f, %.2f", bDummySusccessfulProbe ? "true" : "false", dummyProbePoint.x, dummyProbePoint.y, dummyProbePoint.z);
			}
		}
#endif
		updateStartLoc = UpdateLocationFromPortalProximity(updateStartLoc, &goneOutOfMap);	// because collisions can't identify rooms with 100% accuracy

		//---
		//destLoc = updateStartLoc;			// update destination from the intermediate point
		SetDestLoc(1, destLoc,updateStartLoc, pos);
		//---

		PtDebug( "Probe successful: %d, destLoc: %s, goneOutOfMap: %d", bSuccessfulProbe, PtUpdateDebug::IntLocPrinter(destLoc).c_str(), goneOutOfMap );

		if ( m_bIsFallbackEnabled )
		{
			if ( bSuccessfulProbe )
				m_bIsFallbackEnabled = false;
			else
			{
				PtDebug( "Fallback enabled and probe failed, falling back to %s", PtUpdateDebug::IntLocPrinter(m_fallbackInteriorLocation).c_str() );

				bSkipPortalTraversal = true;

				//---
				destLoc = m_fallbackInteriorLocation;
				SetDestLoc(2, destLoc, m_fallbackInteriorLocation, pos);
				//---
			}
		}

		if ( bSuccessfulProbe ) {
			m_bScanUntilProbeTrue = false;
			m_oldPos = probePoint;
		} else {
			// if entity is retained but probe at new location is inconclusive, then stay in the retain list
			if (m_pOwner && m_pOwner->GetIsRetainedByInteriorProxy()){
				PtDebug( "Entity is retained and probe was unsuccessful, skipping" );
				return;
			}
			m_oldPos = m_currPos;		//treat as teleport to new location
		}

#if __BANK
		if (m_pOwner && m_pOwner->GetIsRetainedByInteriorProxy() && m_bFlushedFromRetainList)
		{
			// verify that objects which are flushed out of retain lists pick up the correct interior destination
			netObject* pNetObj = m_pOwner->GetIsDynamic() ? static_cast<CDynamicEntity*>(m_pOwner)->GetNetworkObject() : NULL;

			if (!destLoc.IsValid()){
				Displayf("%s: probed location (outside!) doesn't match retained location (%s) at (%.3f,%.3f,%.3f)",
					pNetObj ? pNetObj->GetLogName() : m_pOwner->GetModelName(),
					CInteriorProxy::GetPool()->GetSlot(m_pOwner->GetRetainingInteriorProxy())->GetModelName(),
					m_currPos.x, m_currPos.y, m_currPos.z);
			} else {
				u32 retainedGroupId = CInteriorProxy::GetPool()->GetSlot(m_pOwner->GetRetainingInteriorProxy())->GetGroupId();
				u32 destinationGroupId = CInteriorProxy::GetFromLocation(destLoc)->GetGroupId();
				if (retainedGroupId != destinationGroupId)
				{
					if (destLoc.GetInteriorProxyIndex() != m_pOwner->GetRetainingInteriorProxy())
					{
						Displayf("%s: probed location (%s : %d) doesn't match retained location (%s : %d) at (%.3f,%.3f,%.3f)",
							pNetObj ? pNetObj->GetLogName() : m_pOwner->GetModelName(),
							CInteriorProxy::GetFromLocation(destLoc)->GetModelName(),
							destinationGroupId,
							CInteriorProxy::GetPool()->GetSlot(m_pOwner->GetRetainingInteriorProxy())->GetModelName(),
							retainedGroupId,
							m_currPos.x, m_currPos.y, m_currPos.z);
					}
				}
			}
		}
#endif //__BANK

		m_bFlushedFromRetainList = false;
		m_bRequestRescanNextUpdate = false;

		PtDebug( "Treating probe request as teleport, m_oldPos is now (%.3f,%.3f,%.3f)", m_oldPos.x, m_oldPos.y, m_oldPos.z );
	}

	m_bIgnoreHighSpeedDeltaTest = false;
	m_bIsReplayControlled = false;

	// if we didn't probe successfully then test for moving through any portal between our old and new positions
	// [SPHERE-OPTIMISE]
	delta = (m_currPos - m_oldPos);
	Vector3 midPoint = (delta / 2.0f) + m_oldPos;
	float	radius = delta.Mag() + 0.5f;		// make bounding sphere a little less tight
	radius = rage::Min(SPEED_LIMIT, radius);		// limit the size of the testing sphere

	spdAABB entityAABB;
	if (enableStraddlingPortals && canStraddlePortals)
	{
		m_pOwner->GetAABB(entityAABB);
		float deRadius = Mag(entityAABB.GetExtent()).Getf();
		radius = rage::Max(deRadius, radius);
	}
	spdSphere testSphere(RCC_VEC3V(midPoint), ScalarV(radius));

	if (GetOwner() == CGameWorld::FindLocalPlayer()){
		ms_pPlayerProximityPortal = NULL;
	}

	// if we have already probed successfully then don't check against portals for movement
	if (!bSkipPortalTraversal)
	{
		bool isStraddlingPortal = false;
		PtDebug( "Probe not done or unsuccessful, testing portal intersections, test sphere is (%.3f,%.3f,%.3f,%.3f)", midPoint.x, midPoint.y, midPoint.z, radius );

		// check against the portals in the current room for moving into a new room, a new interior or outside
		//if (sourceLoc.IsValid())
		if (updateStartLoc.IsValid())
		{
			PtDebug( "Source location is inside interior, testing intersection against portals of the interior" );

			// check for moving out of the interior (but not through a portal...
			spdSphere interiorBound;
		//	CInteriorInst* pIntInst = m_pInteriorProxy->GetInteriorInst();
			CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation(updateStartLoc);
			Assert(pIntInst);
			pIntInst->GetBoundSphere(interiorBound);


			if (!testSphere.IntersectsSphere(interiorBound) || m_bIsExteriorOnly){
				// fast exit out of interiors can be used sometimes...

				//---
				//destLoc.MakeInvalid();			// destination is outside
				fwInteriorLocation tempLoc;
				tempLoc.MakeInvalid();
				SetDestLoc(3,destLoc, tempLoc, pos);
				//---

			} else {
				/*- check for moving between rooms of an interior -*/

				static dev_bool		enableMultipleTraversal = true;
				int					currentRoom = updateStartLoc.GetRoomIndex();
				bool				continueTraversal;
				u32					traversalLimit = 10;
				do
				{
					fwGeom::fwLine traversalTestLine(Vector3::ZeroType, Vector3::ZeroType);
					fwPortalCorners::CreateExtendedLineForPortalTraversalCheck(m_oldPos, m_currPos, traversalTestLine);

					ASSERT_ONLY(bool bStraddlingTestLineValid = false;)
					fwGeom::fwLine straddlingTestLine(Vector3::ZeroType, Vector3::ZeroType);
					if(enableStraddlingPortals && canStraddlePortals)
					{
						Vector3 direction = delta;
						direction.Normalize();
						if(m_pOwner->GetIsTypePed())
						{
							spdAABB aabb;
							static_cast<CPed*>(m_pOwner)->GetExtendedBoundBox(aabb);
							direction *= Mag(aabb.GetExtentFlat()).Getf();
						}
						else if (m_pOwner->GetIsTypeVehicle())
						{
							const spdAABB& aabb = m_pOwner->GetBaseModelInfo()->GetBoundingBox();
							direction *= aabb.GetExtent().GetYf();
						}
						else
						{
							const spdAABB& aabb = m_pOwner->GetBaseModelInfo()->GetBoundingBox();
							direction *= Mag(aabb.GetExtentFlat()).Getf();
						}

						straddlingTestLine.Set(m_currPos - direction, m_currPos + direction);
						ASSERT_ONLY(bStraddlingTestLineValid = true;)
					}

					continueTraversal = false;

					// check against all of the portals in this room
					for(u32 i=0;i<pIntInst->GetNumPortalsInRoom(currentRoom); i++){
						// intersected with any portals?
						fwPortalCorners portal;
						pIntInst->GetPortalInRoom(currentRoom,i, portal);
						if (!pIntInst->IsMirrorPortal(currentRoom,i))
						{
							if (enableStraddlingPortals && canStraddlePortals && !isStraddlingPortal)
							{
								Assert(bStraddlingTestLineValid);
								// Fix this later to be just one test
								if (portal.HasPassedThrough(straddlingTestLine, false))
								{
									isStraddlingPortal = true;
#if __BANK
									if (*fwScanNodesDebug::GetDisplayPortalStraddlingDebugDraw())
									{
										rage::grcDebugDraw::Line(traversalTestLine.m_start, traversalTestLine.m_end, rage::Color32(255, 255, 0, 255), false);
									}
#endif
								}
							}
							if (portal.HasPassedThrough(traversalTestLine))
							{
								// ensure the portal is enabled before traversing
								u32 portalIdx = pIntInst->GetPortalIdxInRoom(currentRoom, i);
								const fwPortalSceneGraphNode* pPortalSceneNode = pIntInst->GetPortalSceneGraphNode(portalIdx);
								if(!pPortalSceneNode || !pPortalSceneNode->IsEnabled())
								{
									continue;
								}

								m_bUsingProbeData = false;		//invalidate probe data flag
								u32 newRoomIdx = pIntInst->GetDestThruPortalInRoom(currentRoom,i);

								// moving out of this MLO?
								if (newRoomIdx == 0) {

									// locally created, streamed archetype objects need to reset - not move out of their creation interior
									if (m_pOwner && m_pOwner->GetIsTypeObject() && m_pOwner->GetArchetype()->IsStreamedArchetype())
									{
										CObject* pObject = static_cast<CObject*>(m_pOwner);
										if (pObject->GetRelatedDummy() && pObject->GetRelatedDummy()->GetOwnedBy() == ENTITY_OWNEDBY_INTERIOR)
										{
											// ideally we'd want to set a flag to remove the real object and reset the dummy. Not for GTA V though.
											m_pOwner->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
											return;
										}
									}

									// if this is a link portal then we could be moving out from this MLO into a new MLO
									if (pIntInst->IsLinkPortal(currentRoom,i))
									{
										CPortalInst* pPortalInst = pIntInst->GetMatchingPortalInst(currentRoom,i);
										//Assert(pPortalInst);		// linked to nothing!

										if (pPortalInst){
											// update using info from the linked portal
											CInteriorInst* pDestIntInst = NULL;
											s32	destPortalIdx = -1;

											// get data about linked interior through this portal
											pPortalInst->GetDestinationData(pIntInst, pDestIntInst, destPortalIdx);
											if ( pDestIntInst )
											{
												Assert(pDestIntInst != pIntInst);		// shouldn't happen

												// destination is a new room in a new interior
												s32 dest = pDestIntInst->GetDestThruPortalInRoom(0, destPortalIdx);		// entry/exit portal so we know it's room 0

												//---
												//destLoc = CInteriorProxy::CreateLocation( pDestIntInst->GetProxy(), dest );
												fwInteriorLocation tempLoc;
												tempLoc = CInteriorProxy::CreateLocation( pDestIntInst->GetProxy(), dest );
												SetDestLoc(4,destLoc, tempLoc, pos);
												//---

												sceneAssert(destLoc.IsValid());

												if (ms_enableTraversalOfMulitpleLinkPortals)
												{
													pIntInst = pDestIntInst;
													currentRoom = newRoomIdx;
													continueTraversal = true;
												}

											}
											else
											{
												//goneOutOfMap = true;
											}
										} else
										{
											Warningf("tracker was unable to find an external link out of %s. Triggering scanning.", pIntInst->GetModelName());
											ScanUntilProbeTrue();
										}
									} else {
										// destination is the outside

										//---
										//destLoc.MakeInvalid();

										fwInteriorLocation tempLoc;
										tempLoc.MakeInvalid();
										SetDestLoc(5,destLoc, tempLoc, pos);
										//---
									}
								} else {

									// destination is a new room in the current MLO

									//---
									//destLoc.SetRoomIndex(newRoomIdx);
									fwInteriorLocation tempLoc;
									tempLoc = destLoc;
									tempLoc.SetRoomIndex(newRoomIdx);
									SetDestLoc(6,destLoc, tempLoc, pos);
									//---

									currentRoom = newRoomIdx;
									continueTraversal = true;
								}

								break;
							}
						}
					}

					// bug 2048191 : limit the number of traversals, to prevent infinite looping
					if (--traversalLimit == 0)
					{
						continueTraversal = false;
						Warningf("Traversal limit was reached, escaping from %s. Triggering scanning.", pIntInst->GetModelName());
						ScanUntilProbeTrue();
					}
				}
				while ( enableMultipleTraversal && continueTraversal);
			}

			PtDebug( "Intersection results: destLoc: %s, goneOutOfMap: %d", PtUpdateDebug::IntLocPrinter(destLoc).c_str(), goneOutOfMap );
		}
		else
		{
			if ( ( m_safeInteriorPopulationScanCode != CPortal::ms_interiorPopulationScanCode ) ||
				( m_portalsSafeSphere.ContainsPoint( VECTOR3_TO_VEC3V( m_currPos ) ) == false ) ||
				( m_portalsSafeSphere.GetRadiusf() < SMALL_FLOAT) )
			{
				PtDebug( "Source location is in exterior and test sphere outside safe area, searching for portals in the map" );

				/*- check for moving into an interior by testing against the portal objects in the world around this position -*/
				fwPtrListSingleLink	scanList;
				fwIsSphereIntersecting intersection(testSphere);

				m_portalsSafeSphere = CPortalInst::FindIntersectingPortals( testSphere.GetCenter(), intersection, scanList, false );
#if __BANK
				if(bBreakWhenSelectedPortalTrackerSafeSphereRadiusIsNearZero && m_pOwner && m_pOwner == g_PickerManager.GetSelectedEntity())
				{
					if (m_portalsSafeSphere.GetRadiusf() < 0.01f)
					{
						__debugbreak();
					}
				}
#endif
				m_safeInteriorPopulationScanCode = CPortal::ms_interiorPopulationScanCode;

				// ---
				//EntryExitPortalCheck(scanList, destLoc, isStraddlingPortal);			// if we moved through a portal then destination is a room inside an interior

				fwInteriorLocation tempLoc;
				tempLoc = destLoc;
				EntryExitPortalCheck(scanList, tempLoc, isStraddlingPortal, delta);			// if we moved through a portal then destination is a room inside an interior
				SetDestLoc(7, destLoc, tempLoc, pos);
				//---

			}
			else
			{
				PtDebug( "Source location is in exterior but test sphere inside safe area, not doing anything. SafeArea is (%.3f,%.3f,%.3f) radius = %.3f",
					m_portalsSafeSphere.GetCenter().GetXf(), m_portalsSafeSphere.GetCenter().GetYf(), m_portalsSafeSphere.GetCenter().GetZf(), m_portalsSafeSphere.GetRadiusf());
			}
		}
		if (canStraddlePortals)
		{
			Assert(m_pOwner && m_pOwner->GetIsDynamic());
			CDynamicEntity *pDE = static_cast<CDynamicEntity*>(m_pOwner);
			if (enableStraddlingPortals)
			{
				// If we weren't straddling a portal but we are now
				if (!wasStraddlingPortal && isStraddlingPortal)
				{
					pDE->AddToStraddlingPortalsContainer();
				}
				else if (wasStraddlingPortal && !isStraddlingPortal) // If we were straddling a portal but we aren't now
				{
					pDE->RemoveFromStraddlingPortalsContainer();
				}
			}
			else
			{
				if (wasStraddlingPortal)
				{
					pDE->RemoveFromStraddlingPortalsContainer();
				}
			}
		}
	}

	if ( goneOutOfMap )
	{
		PtDebug( "Gone out of map, marking entity (if any) and dest location accordingly" );

		// we have to handle gracefully the case where the entity traverse a portal with no destination
		// and falls out of the map
		if ( m_pOwner )
		{
			Assert( m_pOwner->GetIsDynamic() ); // only dynamic entities should have portal tracking
			static_cast< CDynamicEntity* >( m_pOwner )->m_nDEflags.bIsOutOfMap = true;
		}

		//---
		//destLoc.MakeInvalid();
		fwInteriorLocation tempLoc;
		tempLoc.MakeInvalid();
		SetDestLoc(8,destLoc, tempLoc, pos);
		//---
	}

	// *** update the registration of the entity in the node, depending on destination location ***
	PtDebug( "Moving to location: sourceLoc %s, destLoc %s", PtUpdateDebug::IntLocPrinter(sourceLoc).c_str(), PtUpdateDebug::IntLocPrinter(destLoc).c_str() );

	//CInteriorProxy::ValidateLocation(destLoc);
	MoveToNewLocation( destLoc );
#if __BANK
	if (g_enableRecursiveCheck && !bSuccessfulProbe)
	{
		if (!g_recursedUpdate)
		{
			if (m_pOwner &&
			   ((m_pOwner->GetIsTypePed()     && ms_entityTypeToCollectCallStacksFor == ENTITY_TYPE_PED) ||
			    (m_pOwner->GetIsTypeVehicle() && ms_entityTypeToCollectCallStacksFor == ENTITY_TYPE_VEHICLE)))
			{
				g_recursedUpdate = true;
				fwInteriorLocation oldLocation = destLoc;
				SetProbeType(PROBE_TYPE_NEAR);
				//RequestRescanNextUpdate();
				// manually setting these to avoid calling CollectStack()
				if (!m_bScanUntilProbeTrue)
				{
					m_bRequestRescanNextUpdate = true;
					m_bCaptureForInterior = false;
				}

				Vector3 tempPos = m_currPos;
				Update(tempPos);
				g_recursedUpdate = false;

				fwInteriorLocation	newLocation = CInteriorProxy::CreateLocation( m_pInteriorProxy, m_roomIdx );
				if (m_pOwner->GetOwnerEntityContainer() && oldLocation.GetAsUint32() != newLocation.GetAsUint32() &&
					!(oldLocation.GetInteriorProxyIndex() == -1 && newLocation.GetInteriorProxyIndex() == -1))
				{
					if (g_breakOnRecursiveFail)
					{
						__debugbreak();

						if (gRunUpdate)
						{
							MoveToNewLocation(oldLocation);
							g_recursedUpdate = true;
							//fwInteriorLocation oldLocation = destLoc;
							SetProbeType(PROBE_TYPE_NEAR);
							RequestRescanNextUpdate();

							Vector3 tempPos = m_currPos;
							Update(tempPos);
							g_recursedUpdate = false;
							gDebugBreakLocation = CInteriorProxy::CreateLocation( m_pInteriorProxy, m_roomIdx );
							gRunUpdate = false;
						}

					}

					if (g_pauseGameOnRecursiveFail)
					{
						Timer_TogglePauseMode();
					}
					if (g_PickerManager.IsEnabled() && g_recursiveCheckAddToPicker)
					{
						//g_PickerManager.ResetList(true);
						bool alreadyAdded = false;
						for (int i = 0; i < g_PickerManager.GetNumberOfEntities(); i++)
						{
							if (g_PickerManager.GetEntity(i) == m_pOwner)
							{
								alreadyAdded = true;
								break;

							}
						}

						if (!alreadyAdded)
						{
							g_PickerManager.AddEntityToPickerResults(m_pOwner, false, true);
						}
					}

					MoveToNewLocation(oldLocation);
				}
			}
		}
	}
#endif // __BANK
	Assert(CheckConsistency());
}


// return true if this entity should trigger activation of interiors
bool	CPortalTracker::IsActivatingCollisions(CEntity* pEntity){

	// don't want clones streaming in collisions when they are miles away from the player
	if (NetworkUtils::IsNetworkClone(pEntity))
	{
		if (pEntity == FindFollowPed())
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	if (pEntity->GetIsTypePed()){
		CPed* pPed = static_cast<CPed*>(pEntity);
		if (pPed == CGameWorld::FindLocalPlayer()){
			return(true);
		}
	}

	if (pEntity->GetIsTypeObject()){
		CObject* pObject = static_cast<CObject*>(pEntity);
		if ( pObject->GetPortalTracker()->GetIsForcedToBeActivating() == true){
			return(true);
		}
	}

	if (NetworkInterface::IsGameInProgress())
	{
		return(false);
	}

	if (pEntity->GetIsTypePed()){
		CPed* pPed = static_cast<CPed*>(pEntity);
		if ( (pPed->PopTypeIsMission()) && (pPed->ShouldLoadCollision())){
			return(true);
		}
	} else if (pEntity->GetIsTypeVehicle()){
		CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
		if ( (pVehicle->PopTypeIsMission()) && (pVehicle->ShouldLoadCollision())){
			return(true);
		}

	} else if (pEntity->GetIsTypeObject()){
		CObject* pObject = static_cast<CObject*>(pEntity);
		if ( (pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT) && (pObject->ShouldLoadCollision())){
			return(true);
		}
	}

	return(false);
}

// add this to the keep active list if it has correct properties: player, camera, POPTYPE_MISSION
bool	CPortalTracker::AddToActivatingTrackerList(CPortalTracker* pPT){

	if (pPT->m_bActivatingTracker==false)
	{
		if (pPT->IsVisPortalTracker()) {
			// if vis tracker then add
			m_activatingTrackerList.Add(pPT);
			pPT->m_bActivatingTracker = true;
			return(true);
		}

		CEntity* pEntity = pPT->GetOwner();
		if (!pEntity){
			return(false);
		}

		// if an entity tracker then only certain types are added
		if (pEntity->GetIsTypePed()){
			if ( (static_cast<CPed*>(pEntity))->PopTypeIsMission()){
				m_activatingTrackerList.Add(pPT);
				pPT->m_bActivatingTracker = true;
			}

		} else if (pEntity->GetIsTypeVehicle()){
			if ( (static_cast<CVehicle*>(pEntity))->PopTypeIsMission()){
				m_activatingTrackerList.Add(pPT);
				pPT->m_bActivatingTracker = true;
			}

		} else if (pEntity->GetIsTypeObject()){
			if ( (static_cast<CObject*>(pEntity))->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT){
				m_activatingTrackerList.Add(pPT);
				pPT->m_bActivatingTracker = true;
			}
		} else if (pEntity->GetIsDynamic()){
			if ( (static_cast<CDynamicEntity*>(pEntity)->GetStatus() == STATUS_PLAYER )){
				m_activatingTrackerList.Add(pPT);
				pPT->m_bActivatingTracker = true;
			}
		}
	}

	// anything else won't get added to this list
	return(false);
}

// remove from keep active list if on it
void	CPortalTracker::RemoveFromActivatingTrackerList(CPortalTracker* pPT){

//	if (CGameWorld::FindLocalPlayer() == pPT->m_pOwner)
//	{
//		return;
//	}

	if (pPT->m_bActivatingTracker)
	{
		m_activatingTrackerList.Remove(pPT);
		pPT->m_bActivatingTracker=false;
	}
}


//
// name:		CPortalTracker::RemoveFromInterior
// description:	Remove portal tracker from interior and setup its owners values
void CPortalTracker::RemoveFromInterior()
{
	Assert(CheckConsistency());

	CInteriorInst* pIntInst = m_pInteriorProxy ? m_pInteriorProxy->GetInteriorInst() : NULL;
	if(pIntInst)
	{
		// is actually inside a valid room inside an interior, so must be properly removed
		if (m_roomIdx > 0)
		{
			pIntInst->RemoveFromActiveTrackerList(this);
			m_roomIdx = 0;

			if ( this->GetOwner() )
				CGameWorld::Remove( this->GetOwner() );
		}
	}

	m_pInteriorProxy = NULL;

	Assert(CheckConsistency());
}


#if __BANK
bool CPortalTracker::CheckConsistency(){

#if __BANK
	if (!bCheckConsistency){
		return(true);
	}
#endif

	// for things like dynamic objects attached to portals...
	if (m_bDontProcessUpdates){
		return(true);
	}

	CEntity* pEntity = GetOwner();
	if(pEntity && !pEntity->GetIsRetainedByInteriorProxy())
	{
#if __ASSERT

		s32 intProxyId = pEntity->GetInteriorLocation().GetInteriorProxyIndex();

		CInteriorProxy* pIntProxy = (intProxyId == -1) ? NULL : CInteriorProxy::GetPool()->GetSlot(intProxyId);
		CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();

		Assert(pIntProxy == m_pInteriorProxy);
		Assert(pIntInst == m_pInteriorProxy->GetInteriorInst());

		s32	roomIdx = pEntity->GetInteriorLocation().GetRoomIndex();
		if (pIntInst != NULL)
		{
			Assert(roomIdx == m_roomIdx);
		}

		if (pEntity->m_nFlags.bInMloRoom)
		{
			Assert(pEntity->GetInteriorLocation().GetRoomIndex() > 0);
		}

#endif // __ASSERT
	}

#if __ASSERT
	if ( GetOwner() )
	{
		fwBaseEntityContainer*	container = GetOwner()->GetOwnerEntityContainer();
		if ( container )
		{
			Assert( container->GetEntityDescForEntity( GetOwner() ) != NULL );
		}
	}
#endif // __ASSERT

	return(true);
}
#endif // __BANK

// get the hashcode for the current room the portal tracker is in
u32	CPortalTracker::GetCurrRoomHash(void){
	// return name if tracker actually inside an interior
	CInteriorInst* pIntInst = GetInteriorInst();
	if (pIntInst && (m_roomIdx != 0))
	{
		return(pIntInst->GetRoomHashcode(m_roomIdx));
	} else
	{
		return(0);
	}
}

bool CPortalTracker::SetCurrRoomByHash(u32 hashcode){

	CInteriorInst* pIntInst = GetInteriorInst();
	if (pIntInst){
		s32	roomIdx =  pIntInst->FindRoomByHashcode(hashcode);

		if (roomIdx != -1){
			MoveToNewLocation( pIntInst, roomIdx );
#if __BANK
			CollectStack();
#endif
			return(true);
		}
	}

	return(false);			//unable to set tracker to this room
}

// room names only stored for development builds
#if __DEV
// get the name of the current room the portal tracker is in
const char*		CPortalTracker::GetCurrRoomName(void){

	CInteriorInst* pIntInst = GetInteriorInst();
	// return name if tracker actually inside an interior
	if (pIntInst && (m_roomIdx > 0)){
		return(pIntInst->GetRoomName(m_roomIdx));
	} else {
		return(NULL);
	}
}
#endif //__DEV

//--------------------------- CPortalVisTracker---------------------------

// tracker needs to track the movement from frame to frame of an entity. Need to init with correct position.
CPortalVisTracker::CPortalVisTracker() : CPortalTracker(){

	m_bExtClipActive = false;
	m_bDeterminesVisibility = true;

	m_pOwner = NULL;
	m_pGrcViewport = NULL;

	m_LastTargetEntity = 0;
}

CPortalVisTracker::~CPortalVisTracker(){
}

// update the state of the portal tracking from the new position of the owning entity
// (ie. check for moving thru portals)
void CPortalVisTracker::Update(const Vector3& pos){

	// For certain phases, whose portal tracker is not an "activating" one and are not active/updated
	// every frame (like the mirror reflection, that exists only if there is a mirror in view) the portal
	// tracker may be dangling, i.e. point to an interior that is now depopulated. In this case we just kick
	// the portal tracker to the outside. - AP
	if ( !m_pInteriorProxy || !m_pInteriorProxy->GetInteriorInst() || !m_pInteriorProxy->GetInteriorInst()->CanReceiveObjects() ) {
		MoveToNewLocation( CGameWorld::OUTSIDE );
	}

	m_bExtClipActive = false;

	Assert(m_pOwner == NULL);

	CPortalTracker::Update(pos);
}

// update the state of the portal tracking from the new position of the owning entity (the camera)
// but calculate interior data relative the camera current target entity (which should be more accurate)
void CPortalVisTracker::UpdateUsingTarget(const CEntity* pTarget, const Vector3& pos){

#if __BANK
	if (ms_entityTypeToCollectCallStacksFor == traceTypeCamera && m_callstackTrackerTag == INVALID_HASHTAG && this == ms_pTraceVisTracker)
	{
		USE_DEBUG_MEMORY();
		atVarString hashString("CPortalVisTracker %p", this);
		m_callstackTrackerTag = atStringHash(hashString.c_str());
	}
#endif
	if (pTarget && (pTarget->GetIsTypeObject() || pTarget->GetIsTypePed() || pTarget->GetIsTypeVehicle() )){

		// determine portal position by going from target back to current camera position
		const CDynamicEntity* pDE = static_cast<const CDynamicEntity*>(pTarget);
		CPortalTracker* pPT = const_cast<CPortalTracker*>(pDE ? pDE->GetPortalTracker() : NULL);

		CInteriorInst* pTargetIntInst = pPT->GetInteriorInst();
		s32			targetRoomIdx = pPT->m_roomIdx;

		// make sure that this tracker has same portal position as the target
		MoveToNewLocation(pTargetIntInst, targetRoomIdx);
		// tracker has same position as the target
		Teleport(pPT->GetCurrentPos());

		SetIgnoreHighSpeedDeltaTest(true);
		// perform an update from the targets position to the new position for this tracker (handling any
		// portals in the way correctly).

		ms_enableTraversalOfMulitpleLinkPortals = true;

		m_bForceUpdateUsingTarget = false;

		Update(pos);

		ms_enableTraversalOfMulitpleLinkPortals = false;
	}
}

void CPortalTracker::ClearInteriorStateForEntity(CEntity* pEntity)
{
	CDynamicEntity* pDE = static_cast<CDynamicEntity*>(pEntity);

	if (pDE)
	{
		CGameWorld::Remove(pDE, true);
		pDE->GetPortalTracker()->DontRequestScaneNextUpdate();
		pDE->GetPortalTracker()->StopScanUntilProbeTrue();
		CGameWorld::Add(pDE, CGameWorld::OUTSIDE, true);
	}
}

void CPortalTracker::RenderTrackerDebug(void){

#if __BANK
	if (bVisualiseAllTrackers || bVisualiseCutsceneTrackers || bVisualiseObjectNames){
		fwPtrNode* pNode = m_globalTrackerList.GetHeadPtr();

		while(pNode){

			CPortalTracker* pPT = (CPortalTracker*)pNode->GetPtr();
			Assert(pPT);
			CDynamicEntity* pOwner = static_cast<CDynamicEntity*>(pPT->m_pOwner);

			if (pOwner){
				if(	!bVisualiseCutsceneTrackers || (pOwner->GetIsTypeVehicle() && pOwner->GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE) ||
					(pOwner->GetIsTypeObject() && pOwner->GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE))
				{

					if (pPT){
						pPT->Visualise(80,80,255);
					}
				}
			}

			pNode = pNode->GetNextPtr();
		}
	}

	if (bVisualiseActivatingTrackers){
		fwPtrNode* pNode = m_activatingTrackerList.GetHeadPtr();

		while(pNode){
			CPortalTracker* pPT = (CPortalTracker*)pNode->GetPtr();

			if (pPT){
				if (pPT->m_pOwner && IsActivatingCollisions(pPT->m_pOwner)){
					pPT->Visualise(200, 80, 200);	// on list & activating collisions
				} else {
					pPT->Visualise(50, 20, 50);	// on list, but doesn't activate collsiions
				}
			}

			pNode = pNode->GetNextPtr();
		}
	}
#endif //__BANK
}

#define VISUALISE_RANGE	(20.0f)

bool CPortalTracker::Visualise(u8 DEV_ONLY(red), u8 DEV_ONLY(green), u8 DEV_ONLY(blue))
{
#if __DEV

	// only want to do text for peds close to camera
	// Set origin to be the debug cam, or the player..
#if __BANK
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
#else
	Vector3 vOrigin = FindPlayerPed()->GetPosition();
#endif // __BANK

	Vector3 vDiff = m_currPos - camInterface::GetPos();
	float fDist = vDiff.Mag();
	if(fDist >= VISUALISE_RANGE) return false;
	float fScale = 1.0f - (fDist / VISUALISE_RANGE);
	Vector3 WorldCoors = m_currPos + Vector3(0,0,0.1f);
	s32 iYOffset = 0;

	char	probeChar;

	if (m_bUsingProbeData){
		probeChar = '!';
	} else {
		probeChar = '.';
	}

	char debugText[50];
	sprintf(debugText, "%c[%s]", probeChar, GetCurrRoomName());

	if (bVisualiseObjectNames && m_pOwner) {
		sprintf(debugText, "%c[%s]", probeChar, m_pOwner->GetBaseModelInfo()->GetModelName());
		if (!m_pOwner->m_nFlags.bInMloRoom){
			red = 200;
			green = 200;
			blue = 200;
			iYOffset = -5;
		}
	}

	u8 iAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);

	Color32 colour = CRGBA(red,green,blue,iAlpha);

	grcDebugDraw::Text( WorldCoors, colour, 0, iYOffset, debugText);

	return true;

#else //  __DEV

	return true;

#endif //  __DEV

}

#if (__DEV || __BANK)
void CPortalTracker::DebugDraw()
{
	CInteriorInst* pIntInst = NULL;
	s32 roomIdx = -1;

	pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

	if (pIntInst && roomIdx>0 && roomIdx!=INTLOC_INVALID_INDEX)
	{
		// get the interior and room names
		CMloModelInfo *pModelInfo = reinterpret_cast<CMloModelInfo*>(pIntInst->GetBaseModelInfo());
		naAssertf(pModelInfo, "Failed to get model info from interior instance model index %d", pIntInst->GetModelIndex());

		const char* interiorName = "Unable to find Interior";
		if (pModelInfo)
		{
			interiorName = pModelInfo->GetModelName();
		}

		const char* roomName = "Unable To Find Room";
		if (pModelInfo)
		{
			sceneAssert (pModelInfo->GetIsStreamType());
			roomName = pModelInfo->GetRooms()[roomIdx].m_name;
		}

		char interiorInfoDebug[128] = "";
		CInteriorProxy* pInteriorProxy = pIntInst->GetProxy();
		formatf(interiorInfoDebug, 127, "Interior:<%s (%d)>; Room:<%s (%d)>; Group:<%d>",
										interiorName, CInteriorProxy::GetPool()->GetJustIndex(pInteriorProxy),
										roomName, roomIdx, pInteriorProxy->GetGroupId());
		grcDebugDraw::AddDebugOutput(interiorInfoDebug);

		char interiorInfoDebugFlags[384];
		strcpy(interiorInfoDebugFlags, "Flags set on this room:");
		u32 flags = pModelInfo->GetRooms()[roomIdx].m_flags;

		if (flags & ROOM_FREEZEVEHICLES)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s Freeze Vehicles,", interiorInfoDebugFlags);
		};

		if (flags & ROOM_FREEZEPEDS)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s Freeze Peds,", interiorInfoDebugFlags);
		};

		if (flags & ROOM_NO_DIR_LIGHT)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s No Directional Light,", interiorInfoDebugFlags);
		};

		if (flags & ROOM_NO_EXTERIOR_LIGHTS)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s No Exterior Lights,", interiorInfoDebugFlags);
		};

		if (flags & ROOM_FORCE_FREEZE)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s Force Freeze,", interiorInfoDebugFlags);
		};

		if (flags & ROOM_REDUCE_CARS)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s Reduces Cars,", interiorInfoDebugFlags);
		};

		if (flags & ROOM_REDUCE_PEDS)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s Reduce Peds,", interiorInfoDebugFlags);
		};

		if (flags & ROOM_FORCE_DIR_LIGHT_ON)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s Force Directional Light On,", interiorInfoDebugFlags);
		};

		if (flags & ROOM_DONT_RENDER_EXTERIOR)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s Don't Render Exterior", interiorInfoDebugFlags);
		};

		if (pModelInfo->GetRooms()[roomIdx].m_exteriorVisibiltyDepth != -1)
		{
			snprintf(interiorInfoDebugFlags, 255, "%s Exterior Visibility Depth %d", interiorInfoDebugFlags, pModelInfo->GetRooms()[roomIdx].m_exteriorVisibiltyDepth);
		}
		grcDebugDraw::AddDebugOutput(interiorInfoDebugFlags);

	}
	else
	{
		grcDebugDraw::AddDebugOutput("Exterior");
	}

	if (bPtEnableDrawingOfCollectedCallstack && ms_callstackText)
	{
		int yPos = 2;
		const size_t lineBufferSize = 256;
		const size_t maxLineStringLength = lineBufferSize - 1;

		char lineBuffer[lineBufferSize];
		bool endOfLines = false;
		char *pCurrent = ms_callstackText;
		while (!endOfLines)
		{
			char *pStart = pCurrent;
			while (*pCurrent != '\n')
			{
				if (*pCurrent == '\0')
				{
					endOfLines = true;
					break;
				}
				++pCurrent;
			}

			size_t size = size_t(pCurrent - pStart);

			if (!Verifyf(size_t(pCurrent - pStart) < maxLineStringLength, "line buffer size is too small"))
			{
				size = maxLineStringLength;
			}

			strncpy(lineBuffer, pStart, size);
			lineBuffer[size] = '\0';
			grcDebugDraw::PrintToScreenCoors(lineBuffer, NULL, 70, yPos);
			++yPos;
			++pCurrent;
		}

		if (ms_lastRenderPortalTrackerForCallstack && ms_lastRenderPortalTrackerForCallstack->m_callstackTrackerTag != INVALID_HASHTAG)
		{
			++yPos;
			snprintf(lineBuffer, maxLineStringLength, "Probe Start %.3f, %.3f, %.3f", gPTDebugEntry.pos.x, gPTDebugEntry.pos.y, gPTDebugEntry.pos.z);
			lineBuffer[maxLineStringLength]	= '\0';
			grcDebugDraw::PrintToScreenCoors(lineBuffer, NULL, 70, yPos);

			++yPos;
			snprintf(lineBuffer, maxLineStringLength, "Probe hit %.3f, %.3f, %.3f", gPTDebugEntry.probePoint.x, gPTDebugEntry.probePoint.y, gPTDebugEntry.probePoint.z);
			lineBuffer[maxLineStringLength]	= '\0';
			grcDebugDraw::PrintToScreenCoors(lineBuffer, NULL, 70, yPos);

			++yPos;
			CEntity *pOwner = NULL;
			if (gPTDebugEntry.pTracker && gPTDebugEntry.pTracker->GetOwner())
			{
				if (gPTDebugEntry.ownerEntityId != 0xFFFFFFFFU)
				{
					pOwner = (CEntity*)CEntityIDHelper::GetEntityFromID(gPTDebugEntry.ownerEntityId);
				}
			}

			if (gPTDebugEntry.probeSucceeded)
			{
				snprintf(lineBuffer, maxLineStringLength, "Interior %s, Room %d, Probe Length %.2f, Entity Name %s", gPTDebugEntry.interiorHashName.GetHash() != 0 ? gPTDebugEntry.interiorHashName.GetCStr() : "None",
																													 gPTDebugEntry.roomIdx,
																													 gPTDebugEntry.probeLength,
																													 pOwner ? pOwner->GetModelName(): "");
			}
			else
			{
				snprintf(lineBuffer, maxLineStringLength, "Probe Failed, Probe Length %.2f, Entity Name %s", gPTDebugEntry.probeLength, pOwner ? pOwner->GetModelName(): "");
			}
			const char *pProbeType;
			switch (gPTDebugEntry.probeType)
			{
				case CPortalTracker::PROBE_TYPE_DEFAULT:
				{
					pProbeType = "PROBE_TYPE_DEFAULT";
					break;
				}
				case CPortalTracker::PROBE_TYPE_SHORT:
				{
					pProbeType = "PROBE_TYPE_SHORT";
					break;
				}
				case CPortalTracker::PROBE_TYPE_NEAR:
				{
					pProbeType = "PROBE_TYPE_NEAR";
					break;
				}
				case CPortalTracker::PROBE_TYPE_FAR:
				{
					pProbeType = "PROBE_TYPE_FAR";
					break;
				}
				case CPortalTracker::PROBE_TYPE_SUPER_FAR:
				{
					pProbeType = "PROBE_TYPE_SUPER_FAR";
					break;
				}
				case CPortalTracker::PROBE_TYPE_STADIUM:
				{
					pProbeType = "PROBE_TYPE_STADIUM";
					break;
				}
				default:
				{
					pProbeType = "";
					break;
				}
			}

			snprintf(lineBuffer, maxLineStringLength, "%s, probeType %s", lineBuffer, pProbeType);

			lineBuffer[maxLineStringLength]	= '\0';
			grcDebugDraw::PrintToScreenCoors(lineBuffer, NULL, 70, yPos);

			grcDebugDraw::Sphere(gPTDebugEntry.pos, 0.2f, Color32(0xFF, 0, 0, 0xFF),  false);
			grcDebugDraw::Sphere(gPTDebugEntry.pos, 0.025f, Color32(0xFF, 0, 0, 0xFF),  false);
			grcDebugDraw::Sphere(gPTDebugEntry.probePoint, 0.2f, Color32(0, 0, 0xFF, 0xFF),  false);
			grcDebugDraw::Sphere(gPTDebugEntry.probePoint, 0.025f, Color32(0, 0, 0xFF, 0xFF),  false);

			grcDebugDraw::Line(gPTDebugEntry.pos, Vector3(gPTDebugEntry.pos.x, gPTDebugEntry.pos.y, gPTDebugEntry.pos.z - gPTDebugEntry.probeLength), Color32(0, 0xFF, 0, 0xFF));
		}
	}

	if (bPtEnableDrawingOfCollectedCallstack && ms_ptPositionHistory)
	{
		for (PostionHistoryMap::Iterator iter = ms_ptPositionHistory->CreateIterator(); !iter.AtEnd(); iter.Next())
		{
			CPortalTracker *pTracker = iter.GetDataPtr()->pTracker;
			if (pTracker && pTracker->GetOwner() && pTracker->GetOwner() == g_PickerManager.GetSelectedEntity())
			{
				PositionHistory *pHistory = iter.GetDataPtr();
				int firstIndex = pHistory->nextIndex;
				for (int i = 0; i < (POSITION_HISTORY_SIZE - 1); ++i)
				{
					int index = ((firstIndex + i) % POSITION_HISTORY_SIZE) * 3;
					Vector3 start(pHistory->position[index], pHistory->position[index + 1], pHistory->position[index + 2]);

					index = ((firstIndex + i + 1) % POSITION_HISTORY_SIZE) * 3;
					Vector3 end(pHistory->position[index], pHistory->position[index + 1], pHistory->position[index + 2]);
					grcDebugDraw::Line(start, end, Color32(0, 0xFF, 0, 0xFF), Color32(0xFF, 0, 0, 0xFF));
				}
			}
		}
	}

}
#endif	// __DEV || __BANK

void CPortalTracker::Init(u32)
{
#if __BANK
	if (PARAM_portaltrackertracepeds.Get())
	{
		ms_entityTypeToCollectCallStacksFor = ENTITY_TYPE_PED;
	}
	if (PARAM_portaltrackertracevehicles.Get())
	{
		ms_entityTypeToCollectCallStacksFor = ENTITY_TYPE_VEHICLE;
	}
	if (PARAM_portaltrackertracecamera.Get())
	{
		ms_entityTypeToCollectCallStacksFor = traceTypeCamera;
	}

	if (ms_entityTypeToCollectCallStacksFor != -1)
	{
		USE_DEBUG_MEMORY();
		Assertf(!ms_pStackCollector, "InitCallstackCollector has already been called");
		ms_pStackCollector = rage_new rage::sysStackCollector();

		ms_callstackText = rage_new char[PT_CALLSTACK_BUFFERS_SIZE];
		memset(ms_callstackText, 0, PT_CALLSTACK_BUFFERS_SIZE);

		ms_ptTrackedProbes = rage_new TrackProbeMap();
		ms_ptTrackedProbes->Create(PT_MAX_MAP_SIZE);
	}
	if (PARAM_portaltrackerPositionHistory.Get())
	{
		USE_DEBUG_MEMORY();
		ms_ptPositionHistory = rage_new PostionHistoryMap();
		ms_ptPositionHistory->Create(PT_MAX_MAP_SIZE);
	}
#endif // __BANK
}

void CPortalTracker::ShutDown(u32)
{
#if __BANK
	if (ms_pStackCollector)
	{
		USE_DEBUG_MEMORY();

		ms_ptTrackedProbes->Kill();
		delete ms_ptTrackedProbes;

		delete [] ms_callstackText;

		Assertf(ms_pStackCollector, "InitCallstackCollector hasn'tbeen called");
		delete ms_pStackCollector;
	}

	if (PARAM_portaltrackerPositionHistory.Get())
	{
		USE_DEBUG_MEMORY();

		ms_ptPositionHistory->Kill();
		delete ms_ptPositionHistory;
	}
#endif // __BANK
}


void CPortalVisTracker::ResetPrimaryData()
{
	ms_pPrimaryIntProxy = NULL;
	ms_primaryRoomIdx = -1;
}

void CPortalVisTracker::StorePrimaryData(CInteriorProxy* pIntProxy, s32 roomIdx)
{
	if (pIntProxy && pIntProxy->GetInteriorInst())
	{
		Assert(roomIdx < (s32)pIntProxy->GetInteriorInst()->GetNumRooms());
	}

	ms_pPrimaryIntProxy = pIntProxy;
	ms_primaryRoomIdx = roomIdx;
}

void CPortalVisTracker::CheckForResetPrimaryData(CInteriorInst* pTest)
{
	if (ms_pPrimaryIntProxy && (ms_pPrimaryIntProxy->GetInteriorInst()) == pTest)
	{
		ResetPrimaryData();
	}
}

fwInteriorLocation CPortalVisTracker::GetInteriorLocation(void)
{
	fwInteriorLocation loc;
	loc.MakeInvalid();

	if (ms_pPrimaryIntProxy)
	{
		loc = CInteriorProxy::CreateLocation(ms_pPrimaryIntProxy, ms_primaryRoomIdx);
	}
	return(loc);
}


// tag the portal trackers for all renderphases which have them so that they will do a
// rescan at next update to determine their state
void CPortalVisTracker::RequestResetRenderPhaseTrackers(const spdSphere *pSphere)
{
	// update the portal tracker for this cam as these animated cameras jump about a bit!
	int count = RENDERPHASEMGR.GetRenderPhaseCount();
	for(s32 i=0; i<count; i++)
	{
		CRenderPhase &renderPhase = (CRenderPhase &) RENDERPHASEMGR.GetRenderPhase(i);

		if (renderPhase.IsScanRenderPhase())
		{
			CRenderPhaseScanned &scanPhase = (CRenderPhaseScanned &) renderPhase;

			if (scanPhase.IsUpdatePortalVisTrackerPhase())
			{
				CPortalTracker* pPT = scanPhase.GetPortalVisTracker();

				if (pPT)
				{
					bool scan;
					if (pSphere)
					{
						scan = pSphere->ContainsPoint(VECTOR3_TO_VEC3V(pPT->GetCurrentPos()));
					}
					else
					{
						scan = true;
					}

					if (scan)
					{
						pPT->ScanUntilProbeTrue();
					}
				}
			}
		}
	}
}

s32 CPortalVisTracker::GetCameraFloorId(void)
{
	if (ms_pPrimaryIntProxy){
		CInteriorInst* pIntInst = ms_pPrimaryIntProxy->GetInteriorInst();
		if (pIntInst){
			return(pIntInst->GetFloorIdInRoom(ms_primaryRoomIdx) + pIntInst->GetFloorId());
		}
	}

	return(0);
}

void CPortalTracker::SetOwner(CEntity* pEntity)
{
	m_pOwner = pEntity;
	if (m_pOwner)
	{
		m_oldPos = VEC3V_TO_VECTOR3(m_pOwner->GetTransform().GetPosition());
		SetCurrentPos(m_oldPos);

#if __BANK
		if (ms_entityTypeToCollectCallStacksFor != -1)
		{
			if (!m_pOwner->GetBaseModelInfo())
			{
				m_setownerCalled = 1;
			}
			else if (m_pOwner->GetType() == ms_entityTypeToCollectCallStacksFor)
			{
				USE_DEBUG_MEMORY();
				atVarString hashString("%s%p", m_pOwner->GetModelName(), this);
				m_callstackTrackerTag = atStringHash(hashString.c_str());
			}
		}
#endif // __BANK
	}
#if __BANK
	else if (m_callstackTrackerTag != INVALID_HASHTAG)
	{
		USE_DEBUG_MEMORY();
		if (ms_pStackCollector->IsTagUsed(m_callstackTrackerTag))
		{
			ms_pStackCollector->UnregisterTag(m_callstackTrackerTag);
		}
		m_callstackTrackerTag = INVALID_HASHTAG;
	}
#endif
}


#if __BANK
void CPortalTracker::CollectStack()
{
	if (m_setownerCalled == 1 && m_pOwner->GetBaseModelInfo())
	{
		if (m_pOwner->GetType() == ms_entityTypeToCollectCallStacksFor)
		{
			USE_DEBUG_MEMORY();
			u32 uThis = u32((size_t)this);
			atVarString hashString("%s%d", m_pOwner->GetModelName(), uThis);
			m_callstackTrackerTag = atStringHash(hashString.c_str());
		}
		m_setownerCalled = 0;
	}

	if (m_callstackTrackerTag != INVALID_HASHTAG &&
	   ((m_pOwner && m_pOwner->GetType() == ms_entityTypeToCollectCallStacksFor) ||
	    (!m_pOwner && ms_entityTypeToCollectCallStacksFor == traceTypeCamera)))
	{
		USE_DEBUG_MEMORY();
		if (ms_pStackCollector->IsTagUsed(m_callstackTrackerTag))
		{
			ms_pStackCollector->UnregisterTag(m_callstackTrackerTag);
		}

		ms_pStackCollector->CollectStack(m_callstackTrackerTag, 1, 0);
		++m_callstackCount;
	}
}
#endif


bool CPortalTracker::ArePositionsWithinRoomDistance(fwInteriorLocation firstInteriorLoc, fwInteriorLocation secondInteriorLoc, int maxAllowedRoomDistance, bool bTraverseFromExteriorToInterior, bool bSkipDisabledPortals)
{
	if(firstInteriorLoc == secondInteriorLoc)
	{
		return true;
	}

	int roomDistance = 0;
	const int maxInteriorLocs = 100;
	fwInteriorLocation interiorLocsMem[maxInteriorLocs];
	atUserArray<fwInteriorLocation> aInteriorLocs(interiorLocsMem, maxInteriorLocs);
	aInteriorLocs.Push(firstInteriorLoc);

	int iLoc = 0;

	while(roomDistance < maxAllowedRoomDistance)
	{
		int numLocs = aInteriorLocs.GetCount();
		for(; iLoc < numLocs; iLoc++)
		{
			fwInteriorLocation& loc = aInteriorLocs[iLoc];
			if(loc.IsValid())
			{
				CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation(aInteriorLocs[iLoc]);
				Assert(pIntInst);
				if(!pIntInst->ArePortalsCreated())
				{
					continue;
				}
				int currentRoom = loc.GetRoomIndex();

				int numPortals = pIntInst->GetNumPortalsInRoom(currentRoom);
				for(u32 iPortal = 0; iPortal < numPortals; iPortal++)
				{
					fwPortalCorners portal;
					pIntInst->GetPortalInRoom(currentRoom, iPortal, portal);
					if(!pIntInst->IsMirrorPortal(currentRoom, iPortal))
					{
						if(bSkipDisabledPortals)
						{
							// ensure the portal is enabled before traversing
							u32 portalIdx = pIntInst->GetPortalIdxInRoom(currentRoom, iPortal);
							const fwPortalSceneGraphNode* pPortalSceneNode = pIntInst->GetPortalSceneGraphNode(portalIdx);
							if(!pPortalSceneNode || !pPortalSceneNode->IsEnabled())
							{
								continue;
							}
						}

						u32 newRoomIdx = pIntInst->GetDestThruPortalInRoom(currentRoom,iPortal);

						if (newRoomIdx != 0)
						{
							fwInteriorLocation newLoc = loc;
							newLoc.SetRoomIndex(newRoomIdx);

							if(newLoc == secondInteriorLoc)
							{
								return true;
							}
							else if(AssertVerify(AssertVerify(aInteriorLocs.GetCount() < aInteriorLocs.GetCapacity())) && aInteriorLocs.Find(newLoc) == -1)
							{
								aInteriorLocs.Push(newLoc);
							}
						}
						else if (pIntInst->IsLinkPortal(currentRoom,iPortal))
						{
							CPortalInst* pPortalInst = pIntInst->GetMatchingPortalInst(currentRoom,iPortal);

							if (pPortalInst)
							{
								// update using info from the linked portal
								CInteriorInst* pDestIntInst = NULL;
								s32	destPortalIdx = -1;

								// get data about linked interior through this portal
								pPortalInst->GetDestinationData(pIntInst, pDestIntInst, destPortalIdx);
								if ( pDestIntInst )
								{
									Assert(pDestIntInst != pIntInst);

									// destination is a new room in a new interior
									int dest = pDestIntInst->GetDestThruPortalInRoom(0, destPortalIdx);		// entry/exit portal so we know it's room 0

									fwInteriorLocation newLoc;
									newLoc = CInteriorProxy::CreateLocation(pDestIntInst->GetProxy(), dest);

									if(newLoc == secondInteriorLoc)
									{
										return true;
									}
									else if(AssertVerify(aInteriorLocs.GetCount() < aInteriorLocs.GetCapacity()) && aInteriorLocs.Find(newLoc) == -1)
									{
										aInteriorLocs.Push(newLoc);
									}
								}
							}
						}
						else
						{
							fwInteriorLocation newLoc;
							newLoc.MakeInvalid();

							if(newLoc == secondInteriorLoc)
							{
								return true;
							}
							else if(AssertVerify(aInteriorLocs.GetCount() < aInteriorLocs.GetCapacity()) && aInteriorLocs.Find(newLoc) == -1)
							{
								aInteriorLocs.Push(newLoc);
							}
						}
					}
				}
			}
			else if(bTraverseFromExteriorToInterior)
			{
				CPortalInst::Pool* pPortalInstPool = CPortalInst::GetPool();
				int poolSize = pPortalInstPool->GetSize();

				for (int i = 0; i < poolSize; i++)
				{
					CPortalInst *pPortalInst = pPortalInstPool->GetSlot(i);

					if(	!pPortalInst || pPortalInst->IsLinkPortal() )
					{
						continue;
					}

					CInteriorInst* pIntInst = pPortalInst->GetPrimaryInterior();	// track moving into vicinity of this interior
					if(!pIntInst || !pIntInst->ArePortalsCreated())
					{
						continue;
					}

					s32 portalIdxRoom0 = -1;

					fwPortalCorners portal;
					pPortalInst->GetOwnerData(pIntInst, portalIdxRoom0);
					pIntInst->GetPortalInRoom(0,portalIdxRoom0, portal);		// instanced entry/exit portals are all room 0 of course.

					if (!pIntInst->IsMirrorPortal(0, portalIdxRoom0))
					{
						if(bSkipDisabledPortals)
						{
							// ensure the portal is enabled before traversing
							u32 portalIdx = pIntInst->GetPortalIdxInRoom(0, portalIdxRoom0);
							const fwPortalSceneGraphNode* pPortalSceneNode = pIntInst->GetPortalSceneGraphNode(portalIdx);
							if(!pPortalSceneNode || !pPortalSceneNode->IsEnabled())
							{
								continue;
							}
						}

						s32 destRoomIdx = pIntInst->GetDestThruPortalInRoom(0,portalIdxRoom0);
						fwInteriorLocation newLoc = CInteriorProxy::CreateLocation(pIntInst->GetProxy(), destRoomIdx);
						if(newLoc == secondInteriorLoc)
						{
							return true;
						}
						else if(AssertVerify(aInteriorLocs.GetCount() < aInteriorLocs.GetCapacity()) && aInteriorLocs.Find(newLoc) == -1)
						{
							aInteriorLocs.Push(newLoc);
						}
					}
				}
			}
		}

		roomDistance++;
	}

	return false;
}

bool CPortalTracker::ArePositionsWithinRoomDistance(fwInteriorLocation firstInteriorLoc, const Vector3& secondPos, int maxAllowedRoomDistance, bool bTraverseFromExteriorToInterior, bool bSkipDisabledPortals)
{
	fwInteriorLocation secondInteriorLoc;

	CInteriorInst* pProbedIntInst = NULL;
	s32	probedRoomIdx = -1;
	Vector3 probePoint;

	// Probe for interior location of secondPos
	float probeDistance = NEAR_PROBE_DIST;
	if(!ProbeForInterior(secondPos, pProbedIntInst, probedRoomIdx, &probePoint, probeDistance))
	{
		probeDistance = (float)FAR_PROBE_DIST;
		ProbeForInterior(secondPos, pProbedIntInst, probedRoomIdx, &probePoint, probeDistance);
	}
	if(pProbedIntInst && probedRoomIdx > 0)
	{
		secondInteriorLoc = CInteriorProxy::CreateLocation(pProbedIntInst->GetProxy(), probedRoomIdx);
	}
	else
	{
		secondInteriorLoc.MakeInvalid();
	}

	return ArePositionsWithinRoomDistance(firstInteriorLoc, secondInteriorLoc, maxAllowedRoomDistance, bTraverseFromExteriorToInterior, bSkipDisabledPortals);
}

bool CPortalTracker::ArePositionsWithinRoomDistance(const Vector3& firstPos, const Vector3& secondPos, int maxAllowedRoomDistance, bool bTraverseFromExteriorToInterior, bool bSkipDisabledPortals)
{
	fwInteriorLocation firstInteriorLoc;
	fwInteriorLocation secondInteriorLoc;

	CInteriorInst* pProbedIntInst = NULL;
	s32	probedRoomIdx = -1;
	Vector3 probePoint;

	//Probe for interior location of first pos
	float probeDistance = NEAR_PROBE_DIST;
	if(!ProbeForInterior(firstPos, pProbedIntInst, probedRoomIdx, &probePoint, probeDistance))
	{
		probeDistance = (float)FAR_PROBE_DIST;
		ProbeForInterior(firstPos, pProbedIntInst, probedRoomIdx, &probePoint, probeDistance);
	}
	if(pProbedIntInst && probedRoomIdx > 0)
	{
		firstInteriorLoc = CInteriorProxy::CreateLocation(pProbedIntInst->GetProxy(), probedRoomIdx);
	}
	else
	{
		firstInteriorLoc.MakeInvalid();
	}

	// Probe for interior location of secondPos
	probeDistance = NEAR_PROBE_DIST;
	if(!ProbeForInterior(secondPos, pProbedIntInst, probedRoomIdx, &probePoint, probeDistance))
	{
		probeDistance = (float)FAR_PROBE_DIST;
		ProbeForInterior(secondPos, pProbedIntInst, probedRoomIdx, &probePoint, probeDistance);
	}
	if(pProbedIntInst && probedRoomIdx > 0)
	{
		secondInteriorLoc = CInteriorProxy::CreateLocation(pProbedIntInst->GetProxy(), probedRoomIdx);
	}
	else
	{
		secondInteriorLoc.MakeInvalid();
	}

	return ArePositionsWithinRoomDistance(firstInteriorLoc, secondInteriorLoc, maxAllowedRoomDistance, bTraverseFromExteriorToInterior, bSkipDisabledPortals);
}
