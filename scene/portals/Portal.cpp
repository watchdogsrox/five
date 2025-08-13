////////////////////////////////////////////////////////////////////////////////////
// Title	:	Portal.cpp
// Author	:	John
// Started	:	28/10/05
//			:	Portal rendering handled here (using interior data defined in scene
////////////////////////////////////////////////////////////////////////////////////

#include "Portal.h"

// Framework headers
#include "fwmaths/PortalCorners.h"
#include "fwmaths/geomutil.h"
#include "fwscene/search/SearchVolumes.h"

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "grcore/debugdraw.h"
#include "math/math.h"
#include "spatialdata/transposedplaneset.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "control/replay/replay.h"
#include "core/game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "modelInfo/MLOmodelInfo.h"
#include "objects/door.h"
#include "objects/objectpopulation.h"
#include "pathserver/ExportCollision.h"
#include "peds/ped.h"
#include "renderer/lights/lights.h"
#include "renderer/PostScan.h"
#include "renderer/renderer.h"
#include "renderer/renderListGroup.h"
#include "renderer/Mirrors.h"
#include "scene/lod/LodMgr.h"
#include "scene/scene_channel.h"
#include "scene/portals/portalDebugHelper.h"
#include "scene/portals/InteriorInst.h"
#include "scene/loader/MapData_Interiors.h"
#include "scene/portals/portalInst.h"
#include "scene/portals/portalTracker.h"
#include "scene/portals/InteriorGroupManager.h"
#include "scene/world/gameWorld.h"
#include "streaming/IslandHopper.h"
#include "streaming/streaming.h"
#include "TimeCycle/TimeCycle.h"
#include "timecycle/optimisations.h"
#include "vehicles/vehicle.h"
#include "Vfx/VfxHelper.h"

RENDER_OPTIMISATIONS()
TIMECYCLE_OPTIMISATIONS()


#if __BANK
// debug widgetty stuff
extern roomDebugData	roomsData[32];

extern bool bDrawOnlyCurrentInterior;
extern bool	bShowExtFrustum;

extern bool	bVisualiseCutsceneTrackers;
extern bool	bVisualiseActivatingTrackers;
extern bool	bVisualiseObjectNames;
extern bool bObjPointClip;
extern bool bSkipSectorRender;
extern bool bSkipActiveObjRender;

extern bool bShowPortalRenderStats;

// portal rendering stats
extern s32 numMLOsInScene;
extern s32 numLowLODRenders;
extern s32 numHighLODRenders;
extern s32 numIntModelsRendered;

extern s32 numCurrIntModelsRendered;
extern s32 numCurrIntTrackers;
extern s32 numCurrIntRooms;
extern s32 numCurrIntModels;

extern CInteriorInst* pCurrInterior;
#endif //__BANK


fwPtrListSingleLink CPortal::ms_activeInteriorList;		// list of interiors which are currently active

fwPtrListSingleLink CPortal::ms_activePortalObjList;		// list of interiors with active portal objects
fwPtrListSingleLink CPortal::ms_intersectionList;		// working list of entities built from results of intersection tests

fwModelId	CPortal::m_portalInstModelId;

bool			CPortal::ms_InScanPhase			= false;
bool			CPortal::ms_bIsInteriorScene	= false;
bool			CPortal::ms_bPlayerIsInTunnel	= false;
bool			CPortal::ms_bGPSAvailable		= false;
bool			CPortal::ms_bActivateInteriorGroupsUsingCamera = false;
float			CPortal::ms_interiorFxLevel		= 0.0f;

bool			CPortal::ms_bIsWaterReflectionDisabled = false;

Vector3			CPortal::ms_lastPlayerVelocityOrigin;
Vector3			CPortal::ms_lastPlayerVelocityDirection;

int				CPortal::ms_interiorPopulationScanCode = 0;

u32				CPortal::ms_activeGroups;
u32				CPortal::ms_newlyActiveGroups;

sysTimer		CPortal::ms_metroRespawnTimer;

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitLevel
// PURPOSE :  Initialises everything at the start of a level.
/////////////////////////////////////////////////////////////////////////////////
void CPortal::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		m_portalInstModelId.Invalidate();
		CInteriorProxy::InitCacheLoaderModule();
	}
    else if(initMode == INIT_BEFORE_MAP_LOADED)
    {
	    // store the idx for the portal instance model info, or add it if it doesn't already exist
		CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("PortalInst",0x272E7F43),&m_portalInstModelId);

		if (!m_portalInstModelId.IsValid()) {
			CModelInfo::AddBaseModel("PortalInst");
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("PortalInst",0x272E7F43),&m_portalInstModelId);

			pModelInfo->SetHasLoaded(true);
			pModelInfo->SetLodDistance(PORTAL_INSTANCE_DIST);
			pModelInfo->SetDrawableTypeAsAssetless();
		}
    }

	ms_activeGroups = 0;
	ms_newlyActiveGroups = 0;
}

// do necessary cleaning up for end of level
void CPortal::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		CPortalDebugHelper::Shutdown(SHUTDOWN_CORE);

		ms_activeInteriorList.Flush();
		ms_activePortalObjList.Flush();
	}
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	    // remove portals from world & remove physics from objects held in MLO from world
	    CInteriorInst::ShutdownLevel();

		CPortalTracker::ShutdownLevel();

	    CInteriorInst* pIntInst = NULL;
	    s32 poolSize=CInteriorInst::GetPool()->GetSize();
	    s32 offset = 0;

	    while(offset < poolSize)
	    {
		    pIntInst = CInteriorInst::GetPool()->GetSlot(offset);
		    if (pIntInst){
			    pIntInst->m_bInUse = false;		// reset in use flag. will be set by tracker if active.

                pIntInst->RemovePhysics();
				pIntInst->DeleteInst();
		    }
		    offset++;
	    }
    }
}

//handles conversion of dummies in MLO to real objects
// want to make sure collisions setup for current room & room we may be about to enter
void	CPortal::ManageInterior(CInteriorInst *pIntInst){

	Assert(pIntInst);
	pIntInst->m_bInUse = true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  handle conversion of MLO objects back & forth between real and dummy objects in world
/////////////////////////////////////////////////////////////////////////////////

void CPortal::UpdatePlayerVelocity()
{
	CPhysical*			trackedEntity = static_cast< CPhysical* >( CGameWorld::FindLocalPlayer() );
	if(trackedEntity)
	{
		while ( trackedEntity->GetAttachParent() )
			trackedEntity = (CPhysical *) trackedEntity->GetAttachParent();
	}
	else
	{
		// This function used to crash if the player doesn't exist, but we now do this instead,
		// when running the nav data exporter. Not sure if it's really necessary to set these variables,
		// but it seems dangerous to leave them uninitialized, as they have no default value. /FF
		ms_lastPlayerVelocityOrigin = CGameWorld::GetPlayerFallbackPos();
		ms_lastPlayerVelocityDirection.Set(0.001f, 0.0f, 0.0f);		// Is this allowed to be 0, 0, 0?
		return;
	}

	CPortalTracker*		portalTracker = trackedEntity->GetPortalTracker();
	Vector3				rayStart = portalTracker->GetCurrentPos();
	Vector3				rayDirection = portalTracker->GetCurrentPos() - portalTracker->m_oldPos;

	if ( rayDirection.Mag2() > 0.001f )
	{
		ms_lastPlayerVelocityOrigin = rayStart;
		ms_lastPlayerVelocityDirection = rayDirection;
	}
}

spdSphere proxySphere;

bool CPortal::InteriorFrustumCheck(const CInteriorProxy* pIntProxy, CPortalTracker* pPT)
{
	// the duty to populate grouped interiors is left to the group check
	if ( pIntProxy->GetGroupId() != 0 )
		return false;

	// instance portals if the parent MLO is in the frustum (of a vis tracker)
	if (pPT->IsVisPortalTracker()){
		CPortalVisTracker *pPVT = static_cast<CPortalVisTracker*>(pPT);

		const grcViewport *viewport = pPVT->GetGrcViewport();

		Assert(viewport);

		pIntProxy->GetBoundSphere(proxySphere);	
		Vec3V	proxySphereCentre = proxySphere.GetCenter();
		Vector3 boundCentre = RCC_VECTOR3(proxySphereCentre);
		float	boundRadius = proxySphere.GetRadiusf();

		float		instDist = pIntProxy->GetPopulationDistance(true) + 15.0f;		// interior inst and it's LOD might not have same pivot so boost by 15m

		const float		dist2 = (pPT->GetCurrentPos() - boundCentre).Mag2();

		float			instanceDist2 = instDist*instDist;

		// check in the frustum out to the portal instance distance if interior has exterior portals
		spdTransposedPlaneSet8 cullPlanes;
#if NV_SUPPORT
		cullPlanes.SetStereo(*viewport);
#else
		cullPlanes.Set(*viewport);
#endif
		if ((dist2 < instanceDist2) && 
			(cullPlanes.IntersectsSphere(proxySphere)))
		{
			return true;
		}

		// check in a much closer volume around the camera

		instDist = boundRadius + (PORTAL_INSTANCE_DIST/3);
		instanceDist2 = instDist*instDist;
		if (dist2 < instanceDist2){
			return true;
		}
	}

	return false;
}

bool CPortal::InteriorProximityCheck(const CInteriorProxy* pIntProxy, CPortalTracker* pPT)
{
	// the duty to populate grouped interiors is left to the group check
	if ( pIntProxy->GetGroupId() != 0 )
		return false;

	// check dist from tracker to interior & take appropriate action
	spdSphere proxySphere;
	pIntProxy->GetBoundSphere(proxySphere);	

	spdSphere activationSphere( VECTOR3_TO_VEC3V(pPT->GetCurrentPos()), ScalarV(5.0f) );
	return activationSphere.IntersectsSphere(proxySphere);
}

bool CPortal::InteriorGroupCheck(const CInteriorProxy* pIntProxy, CPortalTracker* pPT, s32 REPLAY_ONLY(proxyID))
{
	// only use this for the major tunnel network (metro and sewer)
	if ( pIntProxy->GetGroupId() != 1 )
		return false;

	// check that the interior is near enough
	spdSphere proxySphere;
	pIntProxy->GetBoundSphere(proxySphere);	
	Vec3V	proxySphereCentre = proxySphere.GetCenter();
	Vector3 boundCentre = RCC_VECTOR3(proxySphereCentre);
	float	boundRadius = proxySphere.GetRadiusf();

	const float		dist2 = ( pPT->GetCurrentPos() - boundCentre ).Mag2();
	const float		extInstDist = boundRadius + GROUP_PORTAL_EXTERNAL_INST_DIST;
	const float		intInstDist = boundRadius + GROUP_PORTAL_INTERNAL_INST_DIST;

	const float		extInstanceDist2 = extInstDist * extInstDist;		// external group MLOs which connect to outside
	const float		intInstanceDist2 = intInstDist * intInstDist;		// internal group MLOs which connect to each other

	if (pIntProxy->IsRequiredByLoadScene())
	{
		return true;
	}
	
	CompileTimeAssert(GROUP_PORTAL_INTERNAL_INST_DIST > GROUP_PORTAL_EXTERNAL_INST_DIST);
	// ditch everything beyong the internal activation distance (which is larger, yes?)
	if ( dist2 > intInstanceDist2 )
		return false;

	// check whether this is an entrance interior - and within the external activation distance
	if ( (pIntProxy->GetNumExitPortals() > 0) && (dist2 < extInstanceDist2))
		return true;

	// check whether this tracker is inside the group
	if ( pPT->GetInteriorInst() && (pPT->GetInteriorInst()->GetGroupId() == pIntProxy->GetGroupId()) )
		return true;

	if (NetworkInterface::IsGameInProgress())
	{
		// check if the player tried to probe into this interior, but failed
		if (pIntProxy &&  pIntProxy->GetInteriorInst() && pIntProxy->GetInteriorInst() == CPortalTracker::GetFailedPlayerProbeDestination())
			return true;

		if ((ms_metroRespawnTimer.GetTime() < 15.0f) && (dist2 < extInstanceDist2))
			return true;

		if (NetworkInterface::IsInSpectatorMode() && pPT->IsVisPortalTracker() && (dist2 < (boundRadius * boundRadius)))
		{
			// handle spectator cams intersecting inactive metro tunnel interiors : bug 1669313
			return true;
		}

		if (ms_bActivateInteriorGroupsUsingCamera && pPT->IsVisPortalTracker() && (dist2 < (boundRadius * boundRadius)))
		{
			// handle camera being detached from player location by script : bug 3614533
			return true;
		}
	}

#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		if( proxyID == CReplayMgr::GetReplayPlayerInteriorProxyIndex() )
		{
			CReplayMgr::SetReplayPlayerInteriorProxyIndex(-1);
			return true;
		}
	}
#endif	//GTA_REPLAY

	// ok, we're outside, looking to a (non-entrance) tunnel section - let's do the velocity testing
	if ( CInteriorGroupManager::HasGroupExits( pIntProxy->GetGroupId() ) )
		return CInteriorGroupManager::RayIntersectsExitPortals( pIntProxy->GetGroupId(), ms_lastPlayerVelocityOrigin, ms_lastPlayerVelocityDirection );

	// no population condition applied
	return false;
}

bool CPortal::InteriorShortTunnelCheck(const CInteriorProxy* pIntProxy, CPortalTracker* pPT)
{
	// we don't deal with non grouped interiors, sorry
	if ( pIntProxy->GetGroupId() <= 1 )
		return false;

	// TODO : the bound radius isn't a good match for this. Want to store the visible sphere (pos + lod distance) in proxy instead : much better!
	// check that the interior is near enough
	spdSphere proxySphere;
	pIntProxy->GetBoundSphere(proxySphere);	
	Vec3V	proxySphereCentre = proxySphere.GetCenter();
	Vector3 boundCentre = RCC_VECTOR3(proxySphereCentre);
	float	boundRadius = proxySphere.GetRadiusf();

	const float		dist2 = ( pPT->GetCurrentPos() - boundCentre ).Mag2();
	const float		extInstDist = pIntProxy->GetPopulationDistance(false) + 20.0f;		// need 20m leeway because interior & LOD pivots can mismatch...
	const float		extInstanceDist2 = extInstDist * extInstDist;		// external group MLOs which connect to outside

	bool bResult = false;

	// these tests determine if an interior is going to activate it's group
	// check whether this is an entrance interior - and within the external activation distance (don't let cameras activate groups!)
	if ( (pIntProxy->GetNumExitPortals() > 0) && (dist2 < extInstanceDist2) && (!pPT->IsVisPortalTracker()))
	{
		bResult = true;
		ms_newlyActiveGroups |= (1<<pIntProxy->GetGroupId());
	}

	// check whether this tracker is inside the group
	if ( pPT->GetInteriorInst() && (pPT->GetInteriorInst()->GetGroupId() == pIntProxy->GetGroupId()) )
	{
		bResult = true;
		ms_newlyActiveGroups |= (1<<pIntProxy->GetGroupId());
	}

	// if the interior isn't causing activation, it may still be in an activated group
	if ((ms_activeGroups & ( 1<< pIntProxy->GetGroupId())) != 0) 
	{
		bResult = true;
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if (ms_bActivateInteriorGroupsUsingCamera && pPT->IsVisPortalTracker() && (dist2 < (boundRadius * boundRadius)))
		{
			// handle camera being detached from player location by script : bug 3614533
			return true;
		}
	}

	// no population condition applied
	return bResult;
}

void CPortal::EnsurePlayerActivatesInteriors()
{
	// this is a patch for bugs related to script clearing the load collision flag
	// on a player ped, removing his tracker from the activating list
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (pPlayer && pPlayer->GetPortalTracker())
	{
		CPortalTracker* pPT = pPlayer->GetPortalTracker();
		if (!pPT->IsActivatingTracker())
		{
			Warningf("Local player %s does not have an activating portal tracker - has script cleared the load collision flag?", pPlayer->GetModelName());
			CPortalTracker::AddToActivatingTrackerList(pPT);
		}
	}
}

void CPortal::Update()
{
	PF_AUTO_PUSH_TIMEBAR("CPortal::Update");

	extern bool g_render_lock;
	if(g_render_lock)
	{
		return;
	}

	EnsurePlayerActivatesInteriors();

	// handle interiors which need to change state
	CInteriorProxy::ImmediateStateUpdate();

#if NAVMESH_EXPORT // BuildActiveBuildingsList function in CNavMeshDataExporter already instances/cleans-up interiors.
	if(CNavMeshDataExporter::IsExportingCollision())
	{
		return;
	}
#endif

	// scan through the interior instance list over a number of frames & dummify entries
	CInteriorProxy* pIntProxy = NULL;
	CInteriorInst* pIntInst = NULL;
	s32 poolSize=CInteriorProxy::GetPool()->GetSize();
	static s32 offset = 0;
	s32 base = 0;
	u32 skip = PORTAL_UPDATE_NUM_FRAMES_TO_PROCESS;

	UpdateInteriorScene();
	UpdatePlayerVelocity();

	// update active groups after every cycle through proxies
	if(offset == 0)
	{
		ms_activeGroups = ms_newlyActiveGroups;
		ms_newlyActiveGroups = 0;
	}

	bool bFrustumCheck;
	bool bProximityCheck;
	bool bGroupCheck;
	bool bShortTunnelCheck;
	CPortalTracker* pPT;
	u32 groupId;

	fwPtrNode* pNode = CPortalTracker::GetActivatingTrackerList().GetHeadPtr();

	atFixedArray<CPortalTracker*, 1024> activeTrackers;
	while(pNode != NULL)
	{
		pPT = reinterpret_cast<CPortalTracker*>(pNode->GetPtr());
		pNode = pNode->GetNextPtr();

		if(pPT->m_pOwner != NULL && !CPortalTracker::IsActivatingCollisions(pPT->m_pOwner))
		{
			continue;
		}

		if(AssertVerify(!activeTrackers.IsFull()))
		{
			activeTrackers.Push(pPT);
		}
	}

	u32 i;

	while((base+offset) < poolSize)
	{
		pIntProxy = CInteriorProxy::GetPool()->GetSlot(base + offset);

		// do any required processing on each interior
		if(pIntProxy)
		{
			// HACK for url:bugstar:6834239 - The Carrier interiors turn out to have dodgy group ID's of 100. 
			// We use the group id to bitshift so this should never have been set that high. Unfortunately due 
			// to this the group id for some tunnel interiors on the main island get activated (and pinned) which 
			// in turn brings in the buildings (even though they're culled)
			if(CIslandHopper::GetInstance().GetCurrentActiveIsland().GetHash() == 0x114611d6/*"HeistIsland"*/)
			{
				u32 dontActivateInteriors[] = {
					0xb49288bd, /*"hei_int_mph_carrierhang3"*/
					0xde34dc01, /*"hei_int_mph_carrierhang1"*/
					0xab7fc6f7, /*"hei_int_mph_carrierupper"*/
					0x3af189b3, /*"hei_int_mph_carriercontrol1"*/
					0x6933e637, /*"hei_int_mph_carriercontrol2"*/
				};
				bool dontActivate = false;
				for(int i = 0; i < NELEM(dontActivateInteriors); ++i)
				{
					if(pIntProxy->GetNameHash() == dontActivateInteriors[i])
					{
						dontActivate = true;
					}
				}

				if(dontActivate)
				{
					base += skip;
					continue;
				}
			}

			bFrustumCheck = false;
			bProximityCheck = false;
			bGroupCheck = false;
			bShortTunnelCheck = false;

			pIntInst = pIntProxy->GetInteriorInst();

			groupId = pIntProxy->GetGroupId();

			if(groupId == 0)
			{
				for(i = 0; i < activeTrackers.size() && !bProximityCheck; ++i)
				{
					bFrustumCheck = bFrustumCheck || CPortal::InteriorFrustumCheck(pIntProxy, activeTrackers[i]);
					bProximityCheck = CPortal::InteriorProximityCheck(pIntProxy, activeTrackers[i]);
				}

// 				if (!(bFrustumCheck || bProximityCheck))
// 				{
// 					if (pIntProxy->GetCurrentState() == CInteriorProxy::PS_FULL_WITH_COLLISIONS)
// 					{
// 						Displayf("shutting down");
// 					}
// 				}

				if(bFrustumCheck)
				{
					pIntProxy->SetRequestedState(CInteriorProxy::RM_VISIBLE, CInteriorProxy::PS_FULL);
				}
				else
				{
					pIntProxy->SetRequestedState(CInteriorProxy::RM_VISIBLE, CInteriorProxy::PS_NONE);
				}

				if(bProximityCheck)
				{
					pIntProxy->SetRequestedState(CInteriorProxy::RM_PT_PROXIMITY, CInteriorProxy::PS_FULL_WITH_COLLISIONS);
				}
				else
				{
					pIntProxy->SetRequestedState(CInteriorProxy::RM_PT_PROXIMITY, CInteriorProxy::PS_NONE);
				}
			}
			else if(groupId == 1)
			{
				for(i = 0; i < activeTrackers.size() && !bGroupCheck; ++i)
				{
					bGroupCheck = CPortal::InteriorGroupCheck(pIntProxy, activeTrackers[i], base+offset);
				}

				if(bGroupCheck)
				{
					pIntProxy->SetRequestedState(CInteriorProxy::RM_GROUP, CInteriorProxy::PS_FULL);
				}
				else
				{
					pIntProxy->SetRequestedState(CInteriorProxy::RM_GROUP, CInteriorProxy::PS_NONE, true);
				}
			}
			else
			{
				const bool bIsCurrentlyActive = (ms_activeGroups & ( 1<< pIntProxy->GetGroupId())) != 0;
				for(i = 0; i < activeTrackers.size() && (!bShortTunnelCheck || bIsCurrentlyActive); ++i)
				{
					bShortTunnelCheck = CPortal::InteriorShortTunnelCheck(pIntProxy, activeTrackers[i]);
				}

				if(bShortTunnelCheck)
				{
					pIntProxy->SetRequestedState(CInteriorProxy::RM_SHORT_TUNNEL, CInteriorProxy::PS_FULL);
				}
				else
				{
					pIntProxy->SetRequestedState(CInteriorProxy::RM_SHORT_TUNNEL, CInteriorProxy::PS_NONE, true);
				}
			}

			if(pIntInst)
			{
				pIntInst->m_bInUse &= bFrustumCheck || bProximityCheck || bGroupCheck || bShortTunnelCheck;
			}
		}

		base += skip;
	}

	offset = (offset + 1) % PORTAL_UPDATE_NUM_FRAMES_TO_PROCESS;

	u32 interiorPoolIdx = 0;
	poolSize = CInteriorInst::GetPool()->GetSize();
	while(interiorPoolIdx < poolSize)
	{
		CInteriorInst* pIntInst = CInteriorInst::GetPool()->GetSlot(interiorPoolIdx);

		// do any required processing on each interior
		if(pIntInst)
		{
			if (pIntInst->m_bInUse)
			{
				pIntInst->Update();
			}

			// store distance to camera
			Vector3 delta = camInterface::GetPos() - VEC3V_TO_VECTOR3(pIntInst->GetTransform().GetPosition());
			pIntInst->SetMLODistance(delta.Mag());
		}

		interiorPoolIdx++;
	}

	CalcInteriorFxLevel();

#if __BANK
	CPortalDebugHelper::DebugOutput();
	CPortalDebugHelper::Update();
#endif

	// reset this flag after every update
	ms_bActivateInteriorGroupsUsingCamera = false;
}


// build list of objects in interiors which need to have physics loaded
void CPortal::AppendToActiveBuildingsArray(atArray<CEntity*>& entityArray)
{
	CInteriorInst* pIntInst = NULL;

	fwPtrNode* pPortalObjNode = ms_activePortalObjList.GetHeadPtr();
	fwPtrNode* pActiveInteriorNode = ms_activeInteriorList.GetHeadPtr();

	if (pPortalObjNode == NULL && pActiveInteriorNode == NULL){
		return;
	}

	Assert(entityArray.GetCount() == 0);

	// add the collisions for portal attached objects
	fwPtrNode* pNode = pPortalObjNode;
	while(pNode != NULL){
		pIntInst = reinterpret_cast<CInteriorInst*>(pNode->GetPtr());
		pNode = pNode->GetNextPtr();

		if (pIntInst && !ms_activePortalObjList.IsMemberOfList(pIntInst)){
			pIntInst->AppendPortalActiveObjects(entityArray);
		}
	}

	// add the collisions for the interiors with active trackers
	//	pIntInst = FindPlayerPed()->m_pPortalTracker->m_pIntInst;
	pNode = pActiveInteriorNode;
	while(pNode != NULL){
		pIntInst = reinterpret_cast<CInteriorInst*>(pNode->GetPtr());
		pNode = pNode->GetNextPtr();

		if (pIntInst){
			entityArray.Push(pIntInst);
			pIntInst->AppendActiveObjects(entityArray);
		}
	}

	return;
}

// return if the current scene for the game is an interior one or not, use it to do cheaper stuff outside than usual.
void CPortal::UpdateInteriorScene(){

	CPed * pPlayer = FindPlayerPed();

	bool	bPlayerInside = pPlayer ? pPlayer->m_nFlags.bInMloRoom : false;

	bool	bPrimaryCameraInside = true;

	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	s32 roomId = CPortalVisTracker::GetPrimaryRoomIdx();

	if ((roomId ==-1 ) || (roomId == 0)) {
		bPrimaryCameraInside = false;
	}

	if (pIntInst == NULL){
		bPrimaryCameraInside = false;
	}

	ms_bIsInteriorScene = (bPlayerInside && bPrimaryCameraInside);

	// recalculate this
	ms_bIsWaterReflectionDisabled = false;

	if (bPrimaryCameraInside){
		ms_bIsWaterReflectionDisabled = pIntInst->IsNoWaterReflection();
	}

	CInteriorInst* pPlayerInterior = ( bPlayerInside ? pPlayer->GetPortalTracker()->GetInteriorInst() : NULL);
	CInteriorInst::HoldRoomZeroModels( pPlayerInterior );

	ms_bPlayerIsInTunnel = (pPlayerInterior && pPlayerInterior->GetProxy()->GetGroupId() > 0);

	ms_bGPSAvailable = !ms_bPlayerIsInTunnel || ((pPlayerInterior->GetMLOFlags() & INSTANCE_MLO_GPS_ON) != 0);
}

// ----------------- Portal rendering functions ---------------------

float CPortal::GetDoorOcclusion(const CInteriorInst* pIntInst, u32 globalPortalIdx)
{
	bool doorFound=false;
	
	float doorWeight=0.0f;
	float doorOcclusion=0.0f;

	const bool cutScenePlaying = CutSceneManager::GetInstance()->IsCutscenePlayingBack();

	if( pIntInst->CanReceiveObjects() )
	{
		const fwPortalSceneGraphNode* node = pIntInst->GetPortalSceneGraphNode(globalPortalIdx);

		for(u32 d=0; d<node->GetEntityCount(); d++)
		{
			const CEntity *pEntity = (const CEntity*)node->GetEntity(d);

			if (pEntity)
			{
				if (pEntity->GetIsTypeObject())
				{
					CObject* pObject = (CObject*)pEntity;
					if(pObject->IsADoor())
					{
						CDoor * pDoor = static_cast<CDoor*>(pObject);

						if (!cutScenePlaying || pDoor->IsRegisteredWithCutscene())
						{
							static dev_float debugTransThreshold=0.25f;
							static dev_float debugOrientThreshold=0.5f;

							doorOcclusion+=pDoor->GetDoorOcclusion(0.01f, 0.03f, debugTransThreshold, debugOrientThreshold);
							doorWeight+=1.0f;
							doorFound=true;
						}
					}
				}
			}
		}
	}

	if (doorFound==true)
	{
		doorOcclusion*=1.0f/doorWeight;
	}
	else
	{
		doorOcclusion=1.0f;
	}
			
	return doorOcclusion;
}

void CPortal::CalcAmbientScale(Vec3V_In coords, Vec3V_InOut baseAmbientScales, Vec3V_InOut bleedingAmbientScales, fwInteriorLocation intLoc)
{
	float natAmbScale[2]= {1.0f,1.0f};
	float artAmbScale[2]= {0.0f,0.0f};
	float blendOut[2]= {0.0f,0.0f};
	bool foundHemorrhage = false;
	
	if (!intLoc.IsValid() )
	{
		baseAmbientScales = Vec3V(ScalarV(natAmbScale[0]),ScalarV(artAmbScale[0]),ScalarV(blendOut[0]));
		bleedingAmbientScales = Vec3V(ScalarV(natAmbScale[1]),ScalarV(artAmbScale[1]),ScalarV(blendOut[1]));
		return;
	}
	
	
	CInteriorProxy* pIntProxy = CInteriorProxy::GetFromLocation(intLoc);
	CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();

	if (pIntInst && pIntInst->m_bArePortalsCreated)
	{
		CMloModelInfo *pModelInfo = pIntInst->GetMloModelInfo();
		sceneAssert(pModelInfo);
		sceneAssert(pModelInfo->GetIsStreamType());

		u32 roomId;

		if( intLoc.IsAttachedToPortal() ) 
		{	
			u32 roomId1,roomId2;
			sceneAssert(pModelInfo->GetIsStreamType());

			CPortalFlags dontCare;
			pModelInfo->GetPortalData(intLoc.GetPortalIndex(), roomId1, roomId2, dontCare);
			
			roomId = Max(roomId1,roomId2);
		}
		else
		{
			roomId = intLoc.GetRoomIndex();
		}

		int timeCycleIndex;
		int secondaryTimeCycleIndex;
		float blend;

		sceneAssert(pModelInfo->GetIsStreamType());

		pIntInst->GetTimeCycleIndicesForRoom(roomId, timeCycleIndex, secondaryTimeCycleIndex);
		blend = pModelInfo->GetRoomBlend(roomId);
#if __ASSERT
		atHashWithStringNotFinal timeCycleName;
		atHashWithStringNotFinal secondaryTimeCycleName;
		pModelInfo->GetRoomTimeCycleNames(roomId, timeCycleName, secondaryTimeCycleName);
#endif

#if TC_DEBUG
		if( g_timeCycle.GetGameDebug().GetDisableSecondaryInteriorModifiers() )
		{
			secondaryTimeCycleIndex = -1;
			ASSERT_ONLY(secondaryTimeCycleName.Clear());
		}
#endif

		if( timeCycleIndex>=0 && blend>0.0f)
		{
			g_timeCycle.CalcModifierAmbientScale(timeCycleIndex, secondaryTimeCycleIndex, blend, natAmbScale[0], artAmbScale[0] ASSERT_ONLY(, timeCycleName, secondaryTimeCycleName));
			
			// Both bleeding on non bleeding starts with the same bases.
			natAmbScale[1] = natAmbScale[0];
			artAmbScale[1] = artAmbScale[0];
		}

		u32 numPortalInRoom = pIntInst->GetNumPortalsInRoom(roomId);

		for (int i=0;i<numPortalInRoom;i++)
		{
			float doorOcclusion=1.0f;

			// Get Portal flags
			u32	interiorPortalIdx = pIntInst->GetPortalIdxInRoom(roomId,i);
			const CMloPortalDef &portal = pModelInfo->GetPortals()[interiorPortalIdx];
			CPortalFlags portalFlags = CPortalFlags(portal.m_flags); 

			if (portalFlags.GetIsMirrorPortal() || portalFlags.GetIgnoreModifier() || (pIntInst->TestPortalState(true, false, roomId,i)==true))
			{
				//this is a mirror or is flagged to ignore or is closed 
				doorOcclusion=0.0f;
			}
			else if ( portalFlags.GetAllowClosing() == true )
			{		
				doorOcclusion = GetDoorOcclusion(pIntInst, interiorPortalIdx);
			}
			
			if (doorOcclusion>0.01f)
			{		
				if (roomId==0) //we're actually moving into an interior
				{
					// skip case, TODO : Higher, unless we need that stuff for portal bleeding ? we shall see...
				}
				else if (portalFlags.GetIsLinkPortal()) // handle a portal linking this MLO to another MLO
				{
					CPortalInst* pPortalInst = pIntInst->GetMatchingPortalInst(roomId, i);
					if (pPortalInst && pPortalInst->IsLinkPortal())
					{
						CInteriorInst* pDestIntInst = NULL;
						s32 destPortalIdx = -1;
						pPortalInst->GetDestinationData(pIntInst, pDestIntInst, destPortalIdx);

						if (pDestIntInst)
						{
							int portalId = pDestIntInst->GetPortalIdxInRoom(INTLOC_ROOMINDEX_LIMBO, destPortalIdx);
							fwPortalCorners& portalCorners=pDestIntInst->GetPortal(portalId);

							CMloModelInfo* pDestModelInfo = pDestIntInst->GetMloModelInfo();
							sceneAssert (pDestModelInfo->GetIsStreamType());
							
							s32 destRoomIdx = pDestIntInst->GetDestThruPortalInRoom(INTLOC_ROOMINDEX_LIMBO, destPortalIdx);		//entry/exit portal so we know it's room 0
							
							pDestIntInst->GetTimeCycleIndicesForRoom(destRoomIdx, timeCycleIndex, secondaryTimeCycleIndex);
#if __ASSERT
							pDestModelInfo->GetRoomTimeCycleNames(destRoomIdx, timeCycleName, secondaryTimeCycleName);
#endif
							
							blend = pDestModelInfo->GetRoomBlend(destRoomIdx);

							float roomNatAmbScale=1.0f;
							float roomArtAmbScale=0.0f;

#if TC_DEBUG
							if( g_timeCycle.GetGameDebug().GetDisableSecondaryInteriorModifiers() )
							{
								secondaryTimeCycleIndex = -1;
								ASSERT_ONLY(secondaryTimeCycleName.Clear());
							}
#endif

							if (timeCycleIndex>=0 && blend>0.0f)
							{
								g_timeCycle.CalcModifierAmbientScale(timeCycleIndex, secondaryTimeCycleIndex,blend,roomNatAmbScale,roomArtAmbScale ASSERT_ONLY(, timeCycleName, secondaryTimeCycleName));
							}

							float feather = portalCorners.CalcAmbientScaleFeather(coords);
						
							int scalesIdx = portalFlags.GetUseLightBleed() == false ? 0 : 1;

							if (roomNatAmbScale>natAmbScale[scalesIdx])
							{
								foundHemorrhage |= portalFlags.GetUseLightBleed();
								natAmbScale[scalesIdx]+=(roomNatAmbScale-natAmbScale[scalesIdx])*feather;
							}

							if (roomArtAmbScale>artAmbScale[scalesIdx])
							{
								foundHemorrhage |= portalFlags.GetUseLightBleed();
								artAmbScale[scalesIdx]+=(roomArtAmbScale-artAmbScale[scalesIdx])*feather;
							}							
						}
					} 
				}
				else 
				{
					// handle the case where we move outside, or move to another room inside this MLO
					fwPortalCorners portalCorners;
					pIntInst->GetPortalInRoom(roomId, i, portalCorners);

					u32 roomId0 = (portal.m_roomTo == roomId) ? portal.m_roomFrom : portal.m_roomTo;
					
					if (roomId0==0)
					{
						// Moving outside
						float feather = portalCorners.CalcAmbientScaleFeather(coords) * doorOcclusion;
						if( feather > 0.0f )
						{
							foundHemorrhage |= portalFlags.GetUseLightBleed();
							int blendOutIdx = portalFlags.GetUseLightBleed() == false ? 0 : 1;
							blendOut[blendOutIdx] += feather;
						}
					}
					else
					{
						// another room in this MLO
						pIntInst->GetTimeCycleIndicesForRoom(roomId0, timeCycleIndex, secondaryTimeCycleIndex);
#if __ASSERT
						pModelInfo->GetRoomTimeCycleNames(roomId0, timeCycleName, secondaryTimeCycleName);
#endif
						blend = pModelInfo->GetRoomBlend(roomId0);

						float roomNatAmbScale=1.0f;
						float roomArtAmbScale=0.0f;

#if TC_DEBUG
						if( g_timeCycle.GetGameDebug().GetDisableSecondaryInteriorModifiers() )
						{
							secondaryTimeCycleIndex = -1;
							ASSERT_ONLY(secondaryTimeCycleName.Clear());
						}
#endif

						if (timeCycleIndex>=0 && blend>0.0f)
						{
							g_timeCycle.CalcModifierAmbientScale(timeCycleIndex, secondaryTimeCycleIndex,blend,roomNatAmbScale,roomArtAmbScale ASSERT_ONLY(, timeCycleName, secondaryTimeCycleName));
						}

						float feather = portalCorners.CalcAmbientScaleFeather(coords) * doorOcclusion;
					
						int scalesIdx = portalFlags.GetUseLightBleed() == false ? 0 : 1;

						if (roomNatAmbScale>natAmbScale[scalesIdx])
						{
							foundHemorrhage |= portalFlags.GetUseLightBleed();
							natAmbScale[scalesIdx]+=(roomNatAmbScale-natAmbScale[scalesIdx])*feather;
						}

						if (roomArtAmbScale>artAmbScale[scalesIdx])
						{
							foundHemorrhage |= portalFlags.GetUseLightBleed();
							artAmbScale[scalesIdx]+=(roomArtAmbScale-artAmbScale[scalesIdx])*feather;
						}
					}
				}
			}
		}
	}
	
	baseAmbientScales = Vec3V(ScalarV(natAmbScale[0]),ScalarV(artAmbScale[0]),ScalarV(blendOut[0]));
	
	bleedingAmbientScales = Vec3V(ScalarV(natAmbScale[1]),ScalarV(artAmbScale[1]),ScalarV(foundHemorrhage ? blendOut[1] : -1.0f));
}

bool CPortal::GetNoDirectionalLight()
{
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	s32 roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

	bool ret;

	if ((roomIdx ==-1 ) || (roomIdx == 0) ||(pIntInst == NULL))
	{
		ret=false;
	}
	else
	{
		u32 roomFlags;

		// find the info for this interior
		CMloModelInfo *pModelInfo = pIntInst->GetMloModelInfo();
		Assert(pModelInfo);

		sceneAssert(pModelInfo->GetIsStreamType());
		roomFlags = pModelInfo->GetRoomFlags(roomIdx);

		if (roomFlags&ROOM_NO_DIR_LIGHT)
			ret=true;
		else
			ret=false;
	}

	return ret;	
}

void CPortal::FindModifiersForInteriors(const grcViewport& viewPort, CInteriorInst* pIntInst, u32 roomId, modifierQueryResults *results)
{
	if (roomId<=0) roomId=0;

	CMloModelInfo *pModelInfo = pIntInst->GetMloModelInfo();
	Assert(pModelInfo);

	float blend;
	int timeCycleIndex;
	int secondaryTimeCycleIndex;
	
	if (roomId!=0)
	{		
		//new code path
		sceneAssert(pModelInfo->GetIsStreamType());

#if __ASSERT
		atHashWithStringNotFinal timeCycleName;
		atHashWithStringNotFinal secondaryTimeCycleName;
		pModelInfo->GetRoomTimeCycleNames(roomId, timeCycleName, secondaryTimeCycleName);
#endif
		blend = pModelInfo->GetRoomBlend(roomId);
		pIntInst->GetTimeCycleIndicesForRoom(roomId, timeCycleIndex, secondaryTimeCycleIndex);
		

#if TC_DEBUG
		if( g_timeCycle.GetGameDebug().GetDisableSecondaryInteriorModifiers() )
		{
			secondaryTimeCycleIndex = -1;
			ASSERT_ONLY(secondaryTimeCycleName.Clear());
		}
#endif

		modifierData &data = results->Append();
		data.strength = blend;
#if __ASSERT
		data.mainModifierName = timeCycleName;
		data.secondaryModifierName = secondaryTimeCycleName;
#endif
		data.mainModifier = timeCycleIndex;
		data.secondaryModifier = secondaryTimeCycleIndex;
		data.blendWeight = 1.0f;
	}

	CPortal::FindModifiersForInteriorsR(viewPort, pIntInst, roomId, results, 0, 1.0f, NULL);
}


// look up the time cycle modifier data store in the info for this interior
void CPortal::FindModifiersForInteriorsR(const grcViewport& viewPort, CInteriorInst* pIntInst, u32 roomId, modifierQueryResults *results, int recursionLevel, float parentfactor, const Vector3* parentPortalSphereCentre)
{
	if (recursionLevel>0) 
		return;

	Assert(recursionLevel<=1);

	// find the info for this interior
	CMloModelInfo *pModelInfo = pIntInst->GetMloModelInfo();
	Assert(pModelInfo);

	const float devHeight = (float)GRCDEVICE.GetHeight();
	const float devWidth = (float)GRCDEVICE.GetWidth();

	u32 numPortalInRoom = pIntInst->GetNumPortalsInRoom(roomId);

	for (int i=0;i<numPortalInRoom;i++)
	{
		float doorOcclusion=1.0f;

		// Get Portal flags
		u32	interiorPortalIdx = pIntInst->GetPortalIdxInRoom(roomId,i);
		const CMloPortalDef &portal = pModelInfo->GetPortals()[interiorPortalIdx];
		CPortalFlags portalFlags = CPortalFlags(portal.m_flags); 

		if (portalFlags.GetIsMirrorPortal() || portalFlags.GetIgnoreModifier() || (pIntInst->TestPortalState(true, false, roomId,i)==true))
		{
			//this is a mirror or is flagged to ignore or is closed 
			doorOcclusion=0.0f;
		}
		else if ( portalFlags.GetAllowClosing() == true )
		{		
			doorOcclusion = GetDoorOcclusion(pIntInst, interiorPortalIdx);
		}

#if GTA_REPLAY
		//During replay allow all doors to be open, stops far clip being reduced for replay free camera
		if(CReplayMgr::IsEditModeActive() && !portalFlags.GetIsMirrorPortal() && !portalFlags.GetIgnoreModifier())
		{
			doorOcclusion = 1.0f;
		}
#endif

#if 0
// Don't delete me, I'm cool debuging code.
		{
			fwPortalCorners portalCorners;
			pIntInst->GetPortalInRoom(roomId, i, portalCorners);

			float area = portalCorners.CalcScreenSurface(viewPort);
			float factor = rage::Clamp(area/(devWidth*devHeight),0.0f,1.0f)*parentfactor*doorOcclusion;

			char text[64] = "";
			sprintf( text, "GFX %f %f %s %s", 
								doorOcclusion, 
								factor, 
								portalFlags.GetIsLinkPortal() ? "LINK" : "",
								portalFlags.GetUseLightBleed() ? "BLEED" : "" );
			grcDebugDraw::Text( portalCorners.GetCorner(1), Color32(255,255,0,255), text );
		
			for( int i=0; i<4;i++)
			{
				sprintf(text,"%d",i);
				grcDebugDraw::Text( portalCorners.GetCorner(i), Color32(255,255,255,255), text );
			}
		}
#endif // 0
		if (doorOcclusion>0.01f)
		{		
			if (roomId==0) //we're actually moving into an interior
			{
				fwPortalCorners portalCorners;
				pIntInst->GetPortalInRoom(roomId, i, portalCorners);

				u32 roomId0 = (portal.m_roomTo == roomId) ? portal.m_roomFrom : portal.m_roomTo;
				if (roomId0==0)
				{
					//shouldn't be possible...
					Assert(0);
				}
				else if( !portalFlags.GetIsOneWayPortal() )
				{
					float area = portalCorners.CalcScreenSurface(viewPort);

					float factor = rage::Clamp(area/(devWidth*devHeight),0.0f,1.0f)*parentfactor*doorOcclusion;

					spdSphere sphere;
					portalCorners.CalcBoundingSphere(sphere);

					if (parentPortalSphereCentre)
					{
						const ScalarV d2 = MagSquared(RCC_VEC3V(*parentPortalSphereCentre) - sphere.GetCenter());
						if (IsLessThanAll(d2, ScalarV(V_FLT_SMALL_4))) // d2 < 0.01f*0.01f
						{
							factor=0.0f;
						}
					}

					if (factor>0.01f)
					{
						float blend;
						int timeCycleIndex;
						int secondaryTimeCycleIndex;
						
						sceneAssert(pModelInfo->GetIsStreamType());
						
						pIntInst->GetTimeCycleIndicesForRoom(roomId0, timeCycleIndex, secondaryTimeCycleIndex);
#if __ASSERT
						atHashWithStringNotFinal timeCycleName;
						atHashWithStringNotFinal secondaryTimeCycleName;
						pModelInfo->GetRoomTimeCycleNames(roomId0, timeCycleName, secondaryTimeCycleName);
#endif
						blend = pModelInfo->GetRoomBlend(roomId0);

#if TC_DEBUG
						if( g_timeCycle.GetGameDebug().GetDisableSecondaryInteriorModifiers() )
						{
							secondaryTimeCycleIndex = -1;
							ASSERT_ONLY(secondaryTimeCycleName.Clear());
						}
#endif
						
						if (results->IsFull() == false)
						{						
							modifierData &data = results->Append();
							data.strength = blend;
#if __ASSERT
							data.mainModifierName = timeCycleName;
							data.secondaryModifierName = secondaryTimeCycleName;
#endif
							data.mainModifier = timeCycleIndex;
							data.secondaryModifier = secondaryTimeCycleIndex;
							data.blendWeight = factor;
							data.portalBoundSphere = sphere;

							if (recursionLevel==0)
							{
								const Vector3 sphereCentre = VEC3V_TO_VECTOR3(sphere.GetCenter());
								CPortal::FindModifiersForInteriorsR(viewPort, pIntInst, roomId0, results, recursionLevel+1, factor, &sphereCentre);
							}
						}
						else
						{
							//Assert(0); 
						}
					}
				}				
			}
			else if (portalFlags.GetIsLinkPortal()) // handle a portal linking this MLO to another MLO
			{
				CPortalInst* pPortalInst = pIntInst->GetMatchingPortalInst(roomId, i);
				if (pPortalInst && pPortalInst->IsLinkPortal() )
				{
					CInteriorInst* pDestIntInst = NULL;
					s32 destPortalIdx = -1;
					pPortalInst->GetDestinationData(pIntInst, pDestIntInst, destPortalIdx);

					if (pDestIntInst)
					{
						int portalId = pDestIntInst->GetPortalIdxInRoom(INTLOC_ROOMINDEX_LIMBO, destPortalIdx);
						fwPortalCorners& portalCorners=pDestIntInst->GetPortal(portalId);

						float area = portalCorners.CalcScreenSurface(viewPort);

						float factor = rage::Clamp(area/(devWidth*devHeight),0.0f,1.0f)*parentfactor*doorOcclusion;

						spdSphere sphere;
						portalCorners.CalcBoundingSphere(sphere);

						if (parentPortalSphereCentre)
						{
							const ScalarV d2 = MagSquared(RCC_VEC3V(*parentPortalSphereCentre) - sphere.GetCenter());
							if (IsLessThanAll(d2, ScalarV(V_FLT_SMALL_4))) // d2 < 0.01f*0.01f
							{
								factor=0.0f;
							}
						}

						if (factor>0.01f)
						{
							float blend;
							int timeCycleIndex;
							int secondaryTimeCycleIndex;
							
							CMloModelInfo* pDestModelInfo = pDestIntInst->GetMloModelInfo();
							sceneAssert (pDestModelInfo->GetIsStreamType());
							
							s32 destRoomIdx = pDestIntInst->GetDestThruPortalInRoom(INTLOC_ROOMINDEX_LIMBO, destPortalIdx);		//entry/exit portal so we know it's room 0
							
#if __ASSERT
							atHashWithStringNotFinal timeCycleName;
							atHashWithStringNotFinal secondaryTimeCycleName;
							pDestModelInfo->GetRoomTimeCycleNames(destRoomIdx, timeCycleName, secondaryTimeCycleName);
#endif
							blend = pDestModelInfo->GetRoomBlend(destRoomIdx);
							pDestIntInst->GetTimeCycleIndicesForRoom(destRoomIdx, timeCycleIndex, secondaryTimeCycleIndex);


#if TC_DEBUG
							if( g_timeCycle.GetGameDebug().GetDisableSecondaryInteriorModifiers() )
							{
								secondaryTimeCycleIndex = -1;
								ASSERT_ONLY(secondaryTimeCycleName.Clear());
							}
#endif

							if (results->IsFull() == false)
							{	
								modifierData &data = results->Append();
								data.strength = blend;
#if __ASSERT
								data.mainModifierName = timeCycleName;
								data.secondaryModifierName = secondaryTimeCycleName;
#endif
								data.mainModifier = timeCycleIndex;
								data.secondaryModifier = secondaryTimeCycleIndex;
								data.blendWeight = factor;
								data.portalBoundSphere = sphere;

								if (recursionLevel==0)
								{
									const Vector3 sphereCentre = VEC3V_TO_VECTOR3(sphere.GetCenter());
									CPortal::FindModifiersForInteriorsR(viewPort, pDestIntInst, destRoomIdx, results, recursionLevel+1, factor, &sphereCentre);
								}
							}
							else
							{
								//Assert(0); 
							}
						}
					}
				} 
				else 
				{
					//pIntInst->InstancePortals();	// no portals for this interior yet so instance them
				}
			}
			else 
			{
				// handle the case where we move outside, or move to another room inside this MLO
				fwPortalCorners portalCorners;
				pIntInst->GetPortalInRoom(roomId, i, portalCorners);

				u32 roomId0 = (portal.m_roomTo == roomId) ? portal.m_roomFrom : portal.m_roomTo;
				
				if (roomId0==0)
				{
					float area = portalCorners.CalcScreenSurface(viewPort);

					float factor = rage::Clamp(area/(devWidth*devHeight),0.0f,1.0f)*parentfactor*doorOcclusion;

					spdSphere sphere;
					portalCorners.CalcBoundingSphere(sphere);

					if (parentPortalSphereCentre)
					{
						const ScalarV d2 = MagSquared(RCC_VEC3V(*parentPortalSphereCentre) - sphere.GetCenter());
						if (IsLessThanAll(d2, ScalarV(V_FLT_SMALL_4))) // d2 < 0.01f*0.01f
						{
							factor=0.0f;
						}
					}

					if (factor>0.01f)
					{
						if (results->IsFull() == false)
						{	
							modifierData &data = results->Append();
							data.strength = 1.0f;
#if __ASSERT
							data.mainModifierName.SetHash(0xFFFFFFFF);
							data.secondaryModifierName.SetHash(0xFFFFFFFF);
#endif
							data.mainModifier = -1;
							data.secondaryModifier = -1;
							data.blendWeight = factor;
							data.portalBoundSphere = sphere;
						}
					}
				}
				else
				{
					// another room in this MLO
					float area = portalCorners.CalcScreenSurface(viewPort);
					float factor = rage::Clamp(area/(devWidth*devHeight),0.0f,1.0f)*parentfactor*doorOcclusion;

					spdSphere sphere;
					portalCorners.CalcBoundingSphere(sphere);

					if (parentPortalSphereCentre)
					{
						const ScalarV d2 = MagSquared(RCC_VEC3V(*parentPortalSphereCentre) - sphere.GetCenter());
						if (IsLessThanAll(d2, ScalarV(V_FLT_SMALL_4))) // d2 < 0.01f*0.01f
						{
							factor=0.0f;
						}
					}

					if (factor>0.01f)
					{

						float blend;
						int timeCycleIndex;
						int secondaryTimeCycleIndex;

						sceneAssert (pModelInfo->GetIsStreamType());
						
#if __ASSERT
						atHashWithStringNotFinal timeCycleName;
						atHashWithStringNotFinal secondaryTimeCycleName;
						pModelInfo->GetRoomTimeCycleNames(roomId0, timeCycleName, secondaryTimeCycleName);
#endif
						blend = pModelInfo->GetRoomBlend(roomId0);
						pIntInst->GetTimeCycleIndicesForRoom(roomId0, timeCycleIndex, secondaryTimeCycleIndex);

#if TC_DEBUG
						if( g_timeCycle.GetGameDebug().GetDisableSecondaryInteriorModifiers() )
						{
							secondaryTimeCycleIndex = -1;
							ASSERT_ONLY(secondaryTimeCycleName.Clear());
						}
#endif

						if (results->IsFull() == false)
						{	
							modifierData &data = results->Append();
							data.strength = blend;
#if __ASSERT
							data.mainModifierName = timeCycleName;
							data.secondaryModifierName = secondaryTimeCycleName;
#endif
							data.mainModifier = timeCycleIndex;
							data.secondaryModifier = secondaryTimeCycleIndex;
							data.blendWeight = factor;
							data.portalBoundSphere = sphere;

							if (recursionLevel==0)
							{
								const Vector3 sphereCentre = VEC3V_TO_VECTOR3(sphere.GetCenter());
								CPortal::FindModifiersForInteriorsR(viewPort, pIntInst, roomId0, results, recursionLevel+1, factor, &sphereCentre);
							}
						}
						else
						{
							//Assert(0);
						}
					}
				}
			}
		}
	}
}

int CPortal::FindCrossedPortalRoom(const CInteriorInst* pIntInst, u32 roomId, Vec3V_In pos, Vec3V_In posDelta)
{
	// find the info for this interior
	CMloModelInfo *pModelInfo = pIntInst->GetMloModelInfo();
	Assert(pModelInfo);
	u32 numPortalInRoom = pIntInst->GetNumPortalsInRoom(roomId);
	
	for (int i=0;i<numPortalInRoom;i++)
	{
		float doorOcclusion=1.0f;

		// Get Portal flags
		u32	interiorPortalIdx = pIntInst->GetPortalIdxInRoom(roomId,i);
		const CMloPortalDef &portal = pModelInfo->GetPortals()[interiorPortalIdx];
		CPortalFlags portalFlags = CPortalFlags(portal.m_flags); 

		if (portalFlags.GetIsMirrorPortal() || portalFlags.GetIgnoreModifier() || (pIntInst->TestPortalState(true, false, roomId,i)==true))
		{
			//this is a mirror or is flagged to ignore or is closed 
			doorOcclusion=0.0f;
		}
		else if ( portalFlags.GetAllowClosing() == true )
		{		
			doorOcclusion = GetDoorOcclusion(pIntInst, interiorPortalIdx);
		}
		
		if (doorOcclusion>0.01f)
		{		
			fwPortalCorners portalCorners;
			pIntInst->GetPortalInRoom(roomId, i, portalCorners);

			u32 nextRoomId = (portal.m_roomTo == roomId) ? portal.m_roomFrom : portal.m_roomTo;

			const Vec3V start = pos;
			const Vec3V end = pos - posDelta;
			
			if( portalCorners.HasPassedThrough(start, end) )
			{
				return nextRoomId;
			}
		}
	}
	
	return 0;
}

void CPortal::ActivateIntersectionList(void)
{
	fwPtrNode* pLinkNode = ms_intersectionList.GetHeadPtr();

	while(pLinkNode)
	{
		CInteriorInst*	pIntInst = static_cast<CInteriorInst*>(pLinkNode->GetPtr());
		pLinkNode = pLinkNode->GetNextPtr();

		if (pIntInst)
		{
			CPortal::AddToActiveInteriorList(pIntInst);			// add collisions
			CPortal::ManageInterior(pIntInst);					// convert dummies
		}
	}
}

bool CPortal::AddToIntersectionListCB(CEntity* pEntity, void* data){

	fwGeom::fwLine* pTestLine = static_cast<fwGeom::fwLine*>(data);
	CInteriorInst* pIntInst = static_cast<CInteriorInst*>(pEntity);

	Vector3 centre = pIntInst->GetBoundCentre();
	Vector3 closestPt;

	pTestLine->FindClosestPointOnLine(centre, closestPt);
	Vector3 closestPtToMLO = centre - closestPt;

	if (closestPtToMLO.Mag() < pIntInst->GetBoundRadius()){
		if (pIntInst->GetNumPortalsInRoom(0) > 0){
			ms_intersectionList.Add(pIntInst);
		}
	}

	return(true);		
}

void CPortal::ActivateCollisions(Vector3& mloActivateStart, Vector3& mloActivateVec){

	// test all MLOs within possible collision distance of the activation line
	Vec3V p = RCC_VEC3V(mloActivateStart);
	Vec3V v = RCC_VEC3V(mloActivateVec);
	ScalarV radius = Mag(v)*ScalarV(V_HALF);
	Vec3V centre = AddScaled(p, v, ScalarV(V_HALF));

	fwIsSphereIntersecting testSphere(spdSphere(centre, radius));
	fwGeom::fwLine	testLine(mloActivateStart, (mloActivateStart+mloActivateVec));

	ms_intersectionList.Flush();

	CGameWorld::ForAllEntitiesIntersecting(&testSphere, CPortal::AddToIntersectionListCB, &testLine, ENTITY_TYPE_MASK_MLO, SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_PORTALS);
	ActivateIntersectionList();

	ms_intersectionList.Flush();
}



struct FXLevelData 
{
	float*		m_pInteriorFxLevel;
	fwIsSphereIntersecting*		m_testSphere;
};

bool CPortal::MloFxLevelCB(void* pItem, void* data)
{
	// do any required processing on each interior
	if (pItem)
	{
		FXLevelData callBackData = *static_cast<FXLevelData*>(data);
		fwIsSphereIntersecting testSphere = *(callBackData.m_testSphere);
		float* pFxLevel = callBackData.m_pInteriorFxLevel;

		spdAABB tempBox;
		spdSphere sphere;
		CInteriorInst* pIntInst = static_cast<CInteriorInst*>(pItem);

		if (!pIntInst->GetProxy() || pIntInst->GetProxy()->GetCurrentState() < CInteriorProxy::PS_FULL)
		{
			return(true);
		}

		const spdAABB &box = pIntInst->GetBoundBox( tempBox );
		pIntInst->GetBoundSphere( sphere );

		if (testSphere( sphere, box ))
		{
			for (u32 i=0; i<pIntInst->GetNumPortalsInRoom(0); ++i)
			{
				s32 portalIdx = pIntInst->GetPortalIdxInRoom(0, i);
 				const fwPortalCorners& portalCorners = pIntInst->GetPortal(portalIdx);
				ScalarV vDistSqr = portalCorners.CalcSimpleDistanceToPointSquared(testSphere.m_sphere.GetCenter());
				if (vDistSqr.Getf()<25.0f)
				{
					float thisFxLevel = vDistSqr.Getf()/25.0f;
					*pFxLevel = rage::Min(thisFxLevel, *pFxLevel);
				}
			}
		}
	}

	return(true);			
}

// go through pool and calc fx level for interiors nearby
void CPortal::CalcInteriorFxLevel()
{
	if (CVfxHelper::IsInInterior(NULL))
	{
		ms_interiorFxLevel = 0.0f;
	}
	else
	{
		FXLevelData	callBackData;
		callBackData.m_pInteriorFxLevel = &ms_interiorFxLevel;
		fwIsSphereIntersecting testSphere(spdSphere(RCC_VEC3V(camInterface::GetPos()), ScalarV(V_FOUR)));
		callBackData.m_testSphere = &testSphere;

		ms_interiorFxLevel = 1.0f;
		CInteriorInst::GetPool()->ForAll(MloFxLevelCB, &callBackData);
	}
}



float CPortal::GetInteriorFxLevel()
{
	return ms_interiorFxLevel;
}


