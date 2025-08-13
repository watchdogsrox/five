
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    gps.cpp
// PURPOSE : Everything having to do with gps. This code will store a list of nodes that
//			 would take the player to a particular destination.
//			 Also generates appropriates sound triggers; turn left after 400 yards.
// AUTHOR :  Matthew Smith, Obbe Vermeij.
// CREATED : 11/04/07
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _GPS_H_
	#include "control/gps.h"
#endif

// rage includes
#include "audioengine/curverepository.h"
#include "grcore/debugdraw.h"
#include "fwscene/world/WorldLimits.h"

// framework includes
#include "ai/aichannel.h"
#include "scaleform/channel.h"

// game includes
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "control/gamelogic.h"
#include "control/WaypointRecording.h"
#include "control/WaypointRecordingRoute.h"
#include "core/game.h"
#include "fwsys/timer.h"
#include "scene/portals/Portal.h"
#include "scene/RefMacros.h"
#include "scene/world/gameWorld.h"
#include "frontend/PauseMenu.h"
#include "frontend/CMapMenu.h"
#include "frontend/MiniMap.h"
#include "frontend/MiniMapRenderThread.h"
#include "vehicles/vehicle.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/scriptaudioentity.h"
#include "peds/AssistedMovementStore.h"
#include "peds/ped.h"
#include "script/script_hud.h"
#include "system/controlMgr.h"
#include "system/companion.h"
#include "text/messages.h"

AI_OPTIMISATIONS()

// Spoken GPS instructions - disabled for V
#define GPS_AUDIO 0

#define DEFAULT_GPS_FLASH_INTERVAL (500)  // default gps flash time in milliseconds

#define HIDE_GPS_UNDER_DISTANCE 50.0f
const float fHideGPSUnderDistance2 = HIDE_GPS_UNDER_DISTANCE*HIDE_GPS_UNDER_DISTANCE;

CGpsSlot CGps::m_Slots[GPS_NUM_SLOTS];
s32		CGps::ms_GpsNumNodesStored = 0;
s32	CGps::m_EnableAudio;
bool CGps::m_AudioCurrentlyDisabled;
bool CGps::m_bIsASyncSearchBlockedThisFrame;
bool CGps::m_bASyncStatusSearchSlot[CPathFind::GPS_ASYNC_SEARCH_NUM_SLOTS];

s32	CGps::m_iNumCustomAddedPoints;
Vector3	CGps::m_aCustomAddedPoints[MAX_CUSTOM_ADDED_POINTS];
CNodeAddress CGps::m_aCustomAddedNodes[MAX_CUSTOM_ADDED_POINTS];
int CGps::m_iMultiRouteStartNodeProgress[MAX_CUSTOM_ADDED_POINTS];
Color32	CGps::m_CustomRouteColour;
float CGps::m_iCustomRouteMiniMapWidth = 0.0f;
float CGps::m_iCustomRoutePauseMapWidth = 0.0f;
bool CGps::m_bMultiGpsTracksPlayer = true;
bool CGps::m_bIsFlashingGPS = false;
bool CGps::m_bShowGPSWhileFlashing = true;
u32 CGps::m_bFlashGPSTime = 0;
CDblBuf<Vector3> CGps::ms_relevantPlayerCoords;

bank_float CGps::ms_fNodeRequestMarginMin = 1250.0f;
bank_float CGps::ms_fNodeRequestMarginMax = 3000.0f;
bank_float CGps::ms_iRoadWidthMiniMap = 8.0f;
bank_float CGps::ms_iRoadWidthPauseMap = 30.0f;

#if __BANK
bool CGps::m_bDebug = false;
#endif

extern audFrontendAudioEntity g_FrontendAudioEntity;

u32 g_TurnPredelay = 200;
u32 g_TimeTilBong = 1000;
f32 g_DrivingFastTimeScalingFactor = 2.5f;
u32 g_TimeToPlayTurn = 6000;
u32 g_TimeToPlayTurnThenImmediately = 8000;
u32 g_TimeToPlayJustTurn = 4000;
u32 g_MinTimeBetweenAnnouncements = 1000;
u32 g_IdealTimeBetweenAnnouncements = 3000;
f32 g_DistanceReduction = 10.0f;
f32 g_ProceedToHighlightedDistance = 25.0f;
f32 g_ArrivedDistance = 30.0f;
f32 g_UTurnDotProduct = -0.3f;
bool g_SayDistanceFirst = true;
u8 g_GPSVoice = 0;
f32 g_ImmediateTurnDistance = 30.0f;


void CGpsSlot::Init(s32 slotIndex, unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		Assert(!m_NodeCoordinates);
		Assert(!m_NodeAdress);
		Assert(!m_NodeAdress);
		Assert(!m_NodeInfo);
		Assert(!m_NodeDistanceToTarget);
		m_NodeCoordinates = rage_new Vector3[CGps::ms_GpsNumNodesStored];
		m_NodeAdress = rage_new CNodeAddress[CGps::ms_GpsNumNodesStored];
		m_NodeInfo = rage_new u16[CGps::ms_GpsNumNodesStored];
		m_NodeDistanceToTarget = rage_new s16[CGps::ms_GpsNumNodesStored];
	}

	Clear(slotIndex, false);
}


void CGpsSlot::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		if(m_NodeCoordinates)
		{
			delete[] m_NodeCoordinates;
			m_NodeCoordinates = NULL;
		}
		if(m_NodeAdress)
		{
			delete[] m_NodeAdress;
			m_NodeAdress = NULL;
		}
		if(m_NodeInfo)
		{
			delete[] m_NodeInfo;
			m_NodeInfo = NULL;	
		}
		if(m_NodeDistanceToTarget)
		{
			delete[] m_NodeDistanceToTarget;
			m_NodeDistanceToTarget = NULL;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Clear
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

static const Vector3 g_vEntityPositionInvalid(64000.0f, 64000.0f, 64000.0f);

void CGpsSlot::Clear(s32 slotIndex, bool bClearGpsOnMiniMap)
{
	m_pTargetEntity = NULL;
	m_Destination = g_vEntityPositionInvalid;
	m_NumNodes = 0;
	SetStatus(STATUS_OFF);
	m_LastRecalcRouteTime = 0;
	m_LastShiftRouteTime = 0;
	m_iScanNodesTimer = 0;
	m_RedrawTimer = 0;
	m_Identifier = INVALID_BLIP_ID;
	m_Colour = rage::Color32(0xff, 0xff, 0xff, 0xff);
	m_iGpsFlags = 0;
	m_fastCalculatingPhase = false;
	m_targetEntitySpecified = false;
	m_bClearRender = false;
	m_uCustomRouteFlags = GPS_ROUTE_FLAG_NONE;

	m_AlreadyAnnouncedCalculatingRoute = false;
	m_AlreadyAnnouncedHighlighted = false;
	m_ConsecutiveUTurnRequests = 0;
	m_LastAnnouncedNode = -1;
	m_LastAnnouncedDistance = -1.0f;
	m_LastAnnouncedTime = 0;
	m_LastAnnouncedWasADouble = false;
	m_LastBongedNode = -1;
	m_DistanceFromRoute = -1.0f;
	m_DistanceOfRoute = 0.0f;
	m_iClosestRaceTrackSegment = 0;
	m_DontSayAnythingBeforeTime = 0;
	m_SlotIndex = slotIndex;
	m_InCarLastUpdate = false;
	m_fShowRouteProximity = 0.0f;
	m_LastRouteDistance = 0.f;

	m_bUsePartialDestination = false;
	m_PartialDestination = g_vEntityPositionInvalid;
	m_LastPartialRouteTime = 0;
	m_bLastRouteSearchWasIgnoreOneWay = false;

	if (bClearGpsOnMiniMap)
	{
		// ensure the route gets cleared
		CGps::ClearGpsOnMiniMap(slotIndex);
	}
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Start
// PURPOSE :  Initiates a gps search
/////////////////////////////////////////////////////////////////////////////////

void CGpsSlot::Start(Vector3 &newDestination, CEntity *pEntity, s32 identifier, bool bFastCalculatingPhase, s32 UNUSED_PARAM(slotIndex), Color32 colour)
{
//	Assertf(!IsOn(), "A Gps (route) was started even though one was already running (switch off first)");
	
	// If there is a race track active normal routes don't get rendered. Same for waypoints route.
	if (IsRaceTrackRoute() || IsMultiRoute())
	{
		return;
	}

	// If there is a custom route, then normal routes don't get rendered
	if (GetStatus() == STATUS_WORKING_CUSTOM_ROUTE)
	{
		return;
	}

	static const float MinClearCacheDistanceSqr = 10.f * 10.f;
	bool bClearGPSCache = false;
	if (!m_pTargetEntity && m_Destination.Dist2(newDestination) > MinClearCacheDistanceSqr)
		bClearGPSCache = true;

	m_Destination = newDestination;
	m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds() - m_iMinDurationBetweenRecalculatingRoute;
	m_LastShiftRouteTime = fwTimer::GetTimeInMilliseconds() - m_iMinDurationBeforeProgressingRoute;
	m_RedrawTimer = fwTimer::GetTimeInMilliseconds() - m_iMaxDurationBeforeRedrawingRoute;
	m_iScanNodesTimer = fwTimer::GetTimeInMilliseconds() - m_iScanNodesFreqMs;
	m_Identifier = identifier;
	m_Colour = colour;
	SetCurrentRenderStatus(false);

	m_pTargetEntity = pEntity;
	if (m_pTargetEntity)
	{
		m_targetEntitySpecified = true;
		Vector3 vEntityPos = VEC3V_TO_VECTOR3(m_pTargetEntity->GetTransform().GetPosition());
		if (m_Destination.Dist2(vEntityPos) > MinClearCacheDistanceSqr)
			bClearGPSCache = true;

		m_Destination = vEntityPos;
	}
	else
	{
		m_targetEntitySpecified = false;
	}


	m_DontSayAnythingBeforeTime = fwTimer::GetTimeInMilliseconds() + 200;
	m_fastCalculatingPhase = bFastCalculatingPhase;

	if (bClearGPSCache)
		m_NumNodes = 0;	// Bit hackish maybe

	SetStatus(STATUS_CALCULATING_ROUTE);
}

#if __BANK
void CGpsSlot::DebugRender()
{
	const Vector3 vOrigin = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	const float fDrawDistSqr = 250.0f*250.0f;
	const s32 iTextHeight = grcDebugDraw::GetScreenSpaceTextHeight();

	for(s32 n=1; n<m_NumNodes; n++)
	{
		const Vector3 & vLast = m_NodeCoordinates[n-1];
		const Vector3 & vCurr = m_NodeCoordinates[n];

		if( (vOrigin-vLast).Mag2() < fDrawDistSqr && (vOrigin-vCurr).Mag2() < fDrawDistSqr )
		{
			grcDebugDraw::Line(vLast + ZAXIS, vCurr + ZAXIS, Color_magenta, Color_magenta);

			//u32 iNodeLinks = m_NodeInfo[n] & NODEINFO_LINKS_MASK;
			u32 iNodeFlags = m_NodeInfo[n] & ~NODEINFO_LINKS_MASK;

			s32 iY=0;

			if(iNodeFlags & NODEINFO_HIGHWAY_BIT)
			{
				grcDebugDraw::Text(vCurr + ZAXIS, Color_white, 0, iY, "highway");
				iY += iTextHeight;
			}
			if(iNodeFlags & NODEINFO_JUNCTION_BIT)
			{
				grcDebugDraw::Text(vCurr + ZAXIS, Color_white, 0, iY, "junction");
				iY += iTextHeight;
			}
		}
	}

#if DEBUG_DRAW
	for(s32 w=0; w<CGps::m_iNumCustomAddedPoints; w++)
	{
		grcDebugDraw::Sphere( CGps::m_aCustomAddedPoints[w], 3.0f, Color_white, false );
	}
#endif
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Updates a gps search
/////////////////////////////////////////////////////////////////////////////////
void CGpsSlot::Update(s32 slotIndex, bool bTalk)
{
#if __BANK
	if(CGps::m_bDebug)
	{
		DebugRender();
	}
#endif

	if (IsOn() && m_NumNodes > 1)
		CalcDistanceToTarget();

	// No GPS when in an aircraft
	CPed *pPlayer = CGps::GetRelevantPlayer();
	CVehicle *pPlayerVehicle = CGps::GetRelevantPlayerVehicle();

	if (pPlayerVehicle && pPlayerVehicle->GetIsAircraft())
	{
#if COMPANION_APP
		CCompanionData::GetInstance()->ClearRouteNodeCount(slotIndex);
#endif	//	COMPANION_APP
		return;
	}

	bool bPlayerIsOnHorse = false;  // flag to know whether we are on a horse
	if (pPlayer && pPlayer->GetIsOnMount())
		bPlayerIsOnHorse = true;

	// No GPS on foot (except with map screen, or with certain settings)
	if(!pPlayerVehicle && !bPlayerIsOnHorse)
	{
#if COMPANION_APP
		CCompanionData::GetInstance()->ClearRouteNodeCount(slotIndex);
#endif	//	COMPANION_APP

		if (m_SlotIndex != GPS_SLOT_DISCRETE &&  
			!(slotIndex == GPS_SLOT_RADAR_BLIP && (m_uCustomRouteFlags & CGpsSlot::GPS_ROUTE_FLAG_ON_FOOT)) 
			&& !CPauseMenu::IsInMapScreen() && !CPauseMenu::IsInGalleryScreen() && !CScriptHud::bForceShowGPS)
		{
			if(m_NumNodes > 0)
				m_NodeCoordinates[0].w = 0.f;

			CheckForReachingDestination();
			return; // don't calculate gps now
		}
	}

	// If we were aiming for an entity but it has been deleted; free up the slot.
	// Likewise if our target coordinate is invalid.
	const bool bCurrentDestinationValid =
		(slotIndex == GPS_SLOT_USED_FOR_RACE_TRACK && CGps::m_iNumCustomAddedPoints != 0) ||
		(slotIndex == GPS_SLOT_USED_FOR_CUSTOM_ROUTE && CGps::m_iNumCustomAddedPoints != 0) ||
		(slotIndex == GPS_SLOT_USED_FOR_WAYPOINTS_GPS && CGps::m_iNumCustomAddedPoints != 0) ||
		!m_Destination.IsClose(g_vEntityPositionInvalid, 1.0f);

	if ( IsOn() && ((m_targetEntitySpecified && !m_pTargetEntity) || !bCurrentDestinationValid) )
	{
#if COMPANION_APP
		CCompanionData::GetInstance()->ClearRouteNodeCount(slotIndex);
#endif	//	COMPANION_APP
		Clear(slotIndex, true);
		return;
	}

	switch (GetStatus())
	{
		case STATUS_OFF:
			m_NumNodes = 0; // Prevent lingering stuff when GPs is switched on again
			break;
	
		case STATUS_CALCULATING_ROUTE:
		case STATUS_CALCULATING_ROUTE_QUIET:
			{
				Assert(!m_Destination.IsClose(g_vEntityPositionInvalid, 1.0f));

				m_DistanceFromRoute = -1.0f;
				Vector3	sourcePoint = CGps::GetRelevantPlayerCoords();
				// We make sure the required path nodes are streamed in before we calculate the route.

				if (m_pTargetEntity)
					m_Destination = VEC3V_TO_VECTOR3(m_pTargetEntity->GetTransform().GetPosition());

				Vector3& destination = m_bUsePartialDestination ? m_PartialDestination : m_Destination;

				float	fRequestMargin = CGps::CalcRequiredMarginX(sourcePoint, destination);
				float	requestMinX = rage::Max(WORLDLIMITS_XMIN, (rage::Min(destination.x, sourcePoint.x) - fRequestMargin) );
				float	requestMaxX = rage::Min(WORLDLIMITS_XMAX, (rage::Max(destination.x, sourcePoint.x) + fRequestMargin) );
				float	requestMinY = rage::Max(WORLDLIMITS_YMIN, (rage::Min(destination.y, sourcePoint.y) - PATHFINDREGIONSIZEY * 2) );
				float	requestMaxY = rage::Min(WORLDLIMITS_YMAX, (rage::Max(destination.y, sourcePoint.y) + PATHFINDREGIONSIZEY * 2) );

				// Make the request for the nodes to be loaded (as otherwise we can't make a route there).
				ThePaths.MakeRequestForNodesToBeLoaded(requestMinX, requestMaxX, requestMinY, requestMaxY, NODE_REQUEST_FIRST_GPS+m_SlotIndex);

				// Check if we should query to see if the request for nodes has been fulfilled.
				if(	(GetStatus() == STATUS_CALCULATING_ROUTE_QUIET) ||
					m_fastCalculatingPhase ||
					(fwTimer::GetTimeInMilliseconds() > m_LastRecalcRouteTime+m_iMinDurationBetweenRecalculatingRoute))
				{
					// Check if the request for nodes has been fulfilled.
					if(ThePaths.HaveRequestedNodesBeenLoaded(NODE_REQUEST_FIRST_GPS+m_SlotIndex) && !CGps::m_bIsASyncSearchBlockedThisFrame)
					{
						CGps::m_bIsASyncSearchBlockedThisFrame = true;
						CGps::m_bASyncStatusSearchSlot[CPathFind::GPS_ASYNC_SEARCH_START_NODE] = true;
						CGps::m_bASyncStatusSearchSlot[CPathFind::GPS_ASYNC_SEARCH_END_NODE] = true;

						const bool bIgnoreDestZ = (m_iGpsFlags & GPS_FLAG_IGNORE_DESTINATION_Z) != 0;
						const bool bReduceMaxSearch = (m_SlotIndex==GPS_SLOT_DISCRETE);
						const bool bIgnoreNoGps = ThePaths.bIgnoreNoGpsFlag && (m_SlotIndex==GPS_SLOT_RADAR_BLIP);// && NetworkInterface::IsGameInProgress();

						s16 iGpsFlags = m_iGpsFlags;
						if(m_bUsePartialDestination)
						{
							iGpsFlags |= GPS_FLAG_NO_PULL_PATH_TO_RIGHT_LANE;
							if(m_bLastRouteSearchWasIgnoreOneWay)
							{
								iGpsFlags |= GPS_FLAG_IGNORE_ONE_WAY;
							}
						}

						s32 iNumNodesBeforeRouteCalc = m_NumNodes;
						// Fire off an async search (the last parameter makes it async)
						PF_PUSH_TIMEBAR("Generate GPS Route");
						bool searchIgnoredOneWay = false;
						float fNewDistance = ThePaths.GenerateRoutePointsForGps(CNodeAddress(), destination, m_pTargetEntity, m_NodeCoordinates, m_NodeAdress, m_NodeInfo, m_NodeDistanceToTarget, CGps::ms_GpsNumNodesStored, m_NumNodes, bIgnoreDestZ, iGpsFlags, searchIgnoredOneWay, true, bReduceMaxSearch, bIgnoreNoGps);
						PF_POP_TIMEBAR();
						if(fNewDistance < 0.f) // just started the async search
						{							
							break;
						}
						
						m_bLastRouteSearchWasIgnoreOneWay = searchIgnoredOneWay;

						// Async search completed, release the nodes now
						ThePaths.ReleaseRequestedNodes(NODE_REQUEST_FIRST_GPS+m_SlotIndex);
						CGps::m_bIsASyncSearchBlockedThisFrame = false;
						CGps::m_bASyncStatusSearchSlot[CPathFind::GPS_ASYNC_SEARCH_START_NODE] = false;
						CGps::m_bASyncStatusSearchSlot[CPathFind::GPS_ASYNC_SEARCH_END_NODE] = false;

						// no need to re-render the route on the minimap if the distance is the same & num of nodes are the same (assume route hasnt changed)
						if (fNewDistance != m_DistanceOfRoute && iNumNodesBeforeRouteCalc != m_NumNodes)
						{
							SetCurrentRenderStatus(false);
							m_DistanceOfRoute = fNewDistance;
						}

				//		m_LastRouteDistance = fNewDistance;

						SetStatus(STATUS_WORKING);

						// New route - reset timers
						m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds();
						m_LastShiftRouteTime = 0;
						m_iScanNodesTimer = fwTimer::GetTimeInMilliseconds();
					}
				}
			}
			break;

		case STATUS_WORKING:
		case STATUS_WORKING_RACE_TRACK:
		case STATUS_WORKING_ROUTE_MULTI:
		case STATUS_WORKING_CUSTOM_ROUTE:
			break;

		case STATUS_CALCULATING_ROUTE_RACE_TRACK:
		{
			Assertf(CGps::m_iNumCustomAddedPoints >= 2,"Need at least 2 points in a race track");

			// Calculate the area we need the nodes for.
			float requestMinX = 999999.9f, requestMinY = 999999.9f;
			float requestMaxX = -999999.9f, requestMaxY = -999999.9f;
			for (s32 i = 0; i < CGps::m_iNumCustomAddedPoints; i++)
			{
				requestMinX = rage::Min(requestMinX, CGps::m_aCustomAddedPoints[i].x);
				requestMinY = rage::Min(requestMinY, CGps::m_aCustomAddedPoints[i].y);
				requestMaxX = rage::Max(requestMaxX, CGps::m_aCustomAddedPoints[i].x);
				requestMaxY = rage::Max(requestMaxY, CGps::m_aCustomAddedPoints[i].y);
			}

			ThePaths.MakeRequestForNodesToBeLoaded(requestMinX, requestMaxX, requestMinY, requestMaxY, NODE_REQUEST_FIRST_GPS+m_SlotIndex);

			if ( ThePaths.HaveRequestedNodesBeenLoaded(NODE_REQUEST_FIRST_GPS+m_SlotIndex) )
			{
				ThePaths.GenerateRoutePointsForGpsRaceTrack(m_NodeCoordinates, m_NodeInfo, m_NodeDistanceToTarget, CGps::ms_GpsNumNodesStored, m_NumNodes, m_iGpsFlags );
				ThePaths.ReleaseRequestedNodes(NODE_REQUEST_FIRST_GPS+m_SlotIndex);
				SetCurrentRenderStatus(false);
				SetStatus(STATUS_WORKING_RACE_TRACK);

				m_LastRouteDistance = 0.f;
				// New route - reset timers
				m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds();
				m_LastShiftRouteTime = 0;
				m_iScanNodesTimer = fwTimer::GetTimeInMilliseconds();
			}
			break;
		}
		case STATUS_CALCULATING_ROUTE_MULTI:
		{
			Assertf(CGps::m_bMultiGpsTracksPlayer || CGps::m_iNumCustomAddedPoints >= 2,"Need at least 2 points in a multi GPS route if not tracking the player");
			Assertf(!CGps::m_bMultiGpsTracksPlayer || CGps::m_iNumCustomAddedPoints >= 1,"Need at least 1 point in a multi GPS route if tracking the player");

			static dev_bool bMustHaveVehicle = true;
			CPed * pPlayer = CGps::GetRelevantPlayer();
			CVehicle * pVehicle = CGps::GetRelevantPlayerVehicle();
			Assertf(pPlayer, "We must have a player ped.");
			if(pPlayer && (bPlayerIsOnHorse || pVehicle || !bMustHaveVehicle || ((m_uCustomRouteFlags & CGpsSlot::GPS_ROUTE_FLAG_ON_FOOT) != 0)))
			{
				Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

				// Calculate the area we need the nodes for.
				float requestMinX = 999999.9f, requestMinY = 999999.9f;
				float requestMaxX = -999999.9f, requestMaxY = -999999.9f;
				for (s32 i = 0; i < CGps::m_iNumCustomAddedPoints; i++)
				{
					requestMinX = rage::Min(requestMinX, CGps::m_aCustomAddedPoints[i].x);
					requestMinY = rage::Min(requestMinY, CGps::m_aCustomAddedPoints[i].y);
					requestMaxX = rage::Max(requestMaxX, CGps::m_aCustomAddedPoints[i].x);
					requestMaxY = rage::Max(requestMaxY, CGps::m_aCustomAddedPoints[i].y);
				}
				if(CGps::m_bMultiGpsTracksPlayer)
				{
					requestMinX = rage::Min(requestMinX, vPlayerPos.x) - CGps::ms_fNodeRequestMarginMax;
					requestMinY = rage::Min(requestMinY, vPlayerPos.y) - PATHFINDREGIONSIZEY * 2;
					requestMaxX = rage::Max(requestMaxX, vPlayerPos.x) + CGps::ms_fNodeRequestMarginMax;
					requestMaxY = rage::Max(requestMaxY, vPlayerPos.y) + PATHFINDREGIONSIZEY * 2;
				}

				ThePaths.MakeRequestForNodesToBeLoaded(requestMinX, requestMaxX, requestMinY, requestMaxY, NODE_REQUEST_FIRST_GPS+m_SlotIndex);

				if ( ThePaths.HaveRequestedNodesBeenLoaded(NODE_REQUEST_FIRST_GPS+m_SlotIndex) )
				{
					//--------------------------------------------------------------
					// Calculate the route from the player to the first route point
					const bool bIgnoreNoGps = ThePaths.bIgnoreNoGpsFlag && (m_SlotIndex==GPS_SLOT_RADAR_BLIP);// && NetworkInterface::IsGameInProgress();

					for (int n = 0; n < CGps::m_iNumCustomAddedPoints; ++n)
					{
						if (CGps::m_aCustomAddedNodes[n].IsEmpty())
						{
							CPathNode* pSourceNode = ThePaths.FindRouteEndNodeForGpsSearch(CGps::m_aCustomAddedPoints[n], VEC3_ZERO, false, NULL, bIgnoreNoGps);
							Assertf(pSourceNode, "No node found for custom added point, performance warning!!!");
							if (pSourceNode)
								CGps::m_aCustomAddedNodes[n] = pSourceNode->m_address;
						}
					}

					m_NumNodes = 0;

					if(CGps::m_bMultiGpsTracksPlayer)
					{
						bool searchIgnoredOneWay = false;
						ThePaths.GenerateRoutePointsForGps(CGps::m_aCustomAddedNodes[0], CGps::m_aCustomAddedPoints[0], 
							NULL, 
							m_NodeCoordinates, m_NodeAdress, m_NodeInfo, m_NodeDistanceToTarget, 
							CGps::ms_GpsNumNodesStored, 
							m_NumNodes, 
							false, 
							m_iGpsFlags | GPS_FLAG_NO_PULL_PATH_TO_RIGHT_LANE,
							searchIgnoredOneWay,
							false, 
							false, 
							bIgnoreNoGps );
					}
					CGps::m_iMultiRouteStartNodeProgress[0] = m_NumNodes;

					for(int n = 1; n < CGps::m_iNumCustomAddedPoints; n++)
					{
						int iStorageRemaining = CGps::ms_GpsNumNodesStored - m_NumNodes;
						int iNumFound = 0;

						bool searchIgnoredOneWay = false;
						ThePaths.GenerateRoutePointsForGps(CGps::m_aCustomAddedNodes[n-1], CGps::m_aCustomAddedNodes[n], 
							CGps::m_aCustomAddedPoints[n-1], CGps::m_aCustomAddedPoints[n], 
							VEC3_ZERO, 
							VEC3_ZERO, 
							&m_NodeCoordinates[m_NumNodes], &m_NodeAdress[m_NumNodes], &m_NodeInfo[m_NumNodes], &m_NodeDistanceToTarget[m_NumNodes], 
							iStorageRemaining, 
							iNumFound, 
							false, 
							m_iGpsFlags | GPS_FLAG_NO_PULL_PATH_TO_RIGHT_LANE, 
							searchIgnoredOneWay,
							false, 
							false, 
							bIgnoreNoGps );

						m_NumNodes += iNumFound;

						CGps::m_iMultiRouteStartNodeProgress[n] = m_NumNodes;
					}

					// "Recalculate" route distance to target, it is not 100% accurate but a good enough estimation I recon
					if (m_NumNodes > 1)
					{
						s16 PrevRouteDist = 0;
						for (int i = m_NumNodes - 1; --i;)
						{
							if (m_NodeDistanceToTarget[i - 1] == 0)
								PrevRouteDist = m_NodeDistanceToTarget[i];

							m_NodeDistanceToTarget[i - 1] += PrevRouteDist;
						}
					}
					//

					m_LastRouteDistance = 0.f;
					ThePaths.ReleaseRequestedNodes(NODE_REQUEST_FIRST_GPS+m_SlotIndex);

					SetCurrentRenderStatus(false);
					SetStatus(STATUS_WORKING_ROUTE_MULTI);

					// New route - reset timers
					m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds();
					m_LastShiftRouteTime = 0;
					m_iScanNodesTimer = fwTimer::GetTimeInMilliseconds();
				}
			}
			break;
		}

		default:
			break;
	}

	//----------------------------------------------------------------------------------------------------
	// Shift nodes forward if we have progressed. Trigger 'Calculating route' if player strayed too far

	UpdateRoute();		

#if COMPANION_APP
	CCompanionData::GetInstance()->UpdateGpsRoute((s16)slotIndex, m_NumNodes, m_NodeCoordinates, m_Colour);
#endif	//	COMPANION_APP

	//-----------------------------------------------
	// Handle directions and/or reaching destination

	BANK_ONLY(const char *pString = "-");
	if (bTalk && IsOn())
	{
		//---------------------------------------------------------
		// checks for reaching destination in here if speech is on
#if GPS_AUDIO
		BANK_ONLY(pString = )ThinkOfSomethingToSay();
#endif
	}
	else
	{
		//------------------------------------------
		// otherwise check here if speech is off

		CheckForReachingDestination();  
	}

	//-------------------
	// Debug shit

#if __BANK
	if (CGps::m_bDebug)
	{
		char debugText[50];

		switch (GetStatus())
		{
			case STATUS_OFF:
				sprintf(debugText, "Gps slot:%d STATUS_OFF      %s", m_SlotIndex, pString);
				break;
			case STATUS_CALCULATING_ROUTE:
				sprintf(debugText, "Gps slot:%d STATUS_CALCULATING_ROUTE      %s", m_SlotIndex, pString);
				break;
			case STATUS_CALCULATING_ROUTE_QUIET:
				sprintf(debugText, "Gps slot:%d STATUS_CALCULATING_ROUTE_QUIET      %s", m_SlotIndex, pString);
				break;
			case STATUS_CALCULATING_ROUTE_RACE_TRACK:
				sprintf(debugText, "Gps slot:%d STATUS_CALCULATING_ROUTE_RACE_TRACK      %s", m_SlotIndex, pString);
				break;
			case STATUS_WORKING_RACE_TRACK:
				sprintf(debugText, "Gps slot:%d STATUS_WORKING_RACE_TRACK      %s", m_SlotIndex, pString);
				break;
			case STATUS_CALCULATING_ROUTE_MULTI:
				sprintf(debugText, "Gps slot:%d STATUS_CALCULATING_ROUTE_WAYPOINTS      %s", m_SlotIndex, pString);
				break;
			case STATUS_WORKING_ROUTE_MULTI:
				sprintf(debugText, "Gps slot:%d STATUS_WORKING_WAYPOINTS      %s", m_SlotIndex, pString);
				break;
			case STATUS_WORKING:
				sprintf(debugText, "Gps slot:%d STATUS_WORKING       %s", m_SlotIndex, pString);
				break;
			case STATUS_WORKING_CUSTOM_ROUTE:
				sprintf(debugText, "Gps slot:%d STATUS_WORKING_CUSTOM_ROUTE       %s", m_SlotIndex, pString);
				break;
				
			default:
				break;
		}
		grcDebugDraw::AddDebugOutput(debugText);

		if(m_SlotIndex == GPS_SLOT_USED_FOR_RACE_TRACK && IsRaceTrackRoute())
		{
			Vector3 vLastPos = CGps::m_aCustomAddedPoints[CGps::m_iNumCustomAddedPoints-1];
			for(int i=0; i<CGps::m_iNumCustomAddedPoints; i++)
			{
				Vector3 vPos = CGps::m_aCustomAddedPoints[i];
				grcDebugDraw::Line(vPos, vPos + (ZAXIS * 3.0f), Color_cyan2, Color_cyan);
				grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vPos + (ZAXIS * 3.0f)), 0.5f, Color_cyan);

				grcDebugDraw::Line(vLastPos + (ZAXIS * 2.9f), vPos + (ZAXIS * 2.9f), Color_orange2, Color_orange2);

				vLastPos = vPos;
			}
		}

		if(m_SlotIndex == GPS_SLOT_USED_FOR_WAYPOINTS_GPS && IsMultiRoute())
		{
			int i;
			for(i=1; i<m_NumNodes; i++)
			{
				grcDebugDraw::Line( m_NodeCoordinates[i-1], m_NodeCoordinates[i-1] + ZAXIS, Color_yellow, Color_yellow );
				grcDebugDraw::Line( m_NodeCoordinates[i-1] + ZAXIS, m_NodeCoordinates[i] + ZAXIS, Color_yellow3, Color_yellow3 );
			}
			grcDebugDraw::Line( m_NodeCoordinates[i-1], m_NodeCoordinates[i-1] + ZAXIS, Color_yellow, Color_yellow );


			for(i=0; i<CGps::m_iNumCustomAddedPoints; i++)
			{
				Vector3 vPos = CGps::m_aCustomAddedPoints[i];
				grcDebugDraw::Line(vPos, vPos + (ZAXIS * 3.0f), Color_white, Color_white);
				grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vPos + (ZAXIS * 3.0f)), 0.5f, Color_white);
			}
			Vector3 vLastPos = CGps::m_aCustomAddedPoints[0];
			for(i=1; i<CGps::m_iNumCustomAddedPoints; i++)
			{
				Vector3 vPos = CGps::m_aCustomAddedPoints[i];
				grcDebugDraw::Line(vLastPos + (ZAXIS * 2.9f), vPos + (ZAXIS * 2.9f), Color_black, Color_black);
				vLastPos = vPos;
			}
		}

		if(m_SlotIndex == GPS_SLOT_USED_FOR_CUSTOM_ROUTE && (GetStatus()==STATUS_WORKING_CUSTOM_ROUTE))
		{
			int i;
			for(i=0; i<CGps::m_iNumCustomAddedPoints; i++)
			{
				Vector3 vPos = CGps::m_aCustomAddedPoints[i];
				grcDebugDraw::Line(vPos, vPos + (ZAXIS * 3.0f), Color_cyan2, Color_cyan);
				grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vPos + (ZAXIS * 3.0f)), 0.5f, Color_cyan);
			}

			for(i=1; i<CGps::m_iNumCustomAddedPoints; i++)
			{
				Vector3 vLastPos = CGps::m_aCustomAddedPoints[i-1];
				Vector3 vPos = CGps::m_aCustomAddedPoints[i];
				grcDebugDraw::Line(vLastPos + (ZAXIS * 2.9f), vPos + (ZAXIS * 2.9f), Color_orange2, Color_orange2);
			}
		}

		Vector3 vTargetGps = g_vEntityPositionInvalid;
		Color32 col = Color_red;

		if(!m_Destination.IsClose(g_vEntityPositionInvalid, 1.0f))
		{
			vTargetGps = m_Destination;
			col = Color_green;
		}
		else if(m_pTargetEntity)
		{
			vTargetGps = VEC3V_TO_VECTOR3(m_pTargetEntity->GetTransform().GetPosition());
			col = Color_yellow;
		}

		const Vector3 vPlayerCoors = CGps::GetRelevantPlayerCoords();

        char tmp[16];
        sprintf(tmp, "GPS %i", m_SlotIndex);
        grcDebugDraw::Line( vPlayerCoors, vTargetGps, col, col );
        grcDebugDraw::Text( vTargetGps, col, tmp );
	}
#endif
}


// Callback for finding the closest node to a given point (insde the pData variable)
// Note that for the PS3 SPU Job, there is an Equivalent Callback in PathFindNodeSearchJob.cpp,  CPathFindGPSSpu::FindClosestNodeLinkCB
bool CGps::FindClosestNodeLinkCB(CPathNode * pNode, void * pData)
{
	CPathFind::TFindClosestNodeLinkGPS * pFindClosest = (CPathFind::TFindClosestNodeLinkGPS*)pData;

	if(pNode->IsPedNode())
		return false;
	if(pNode->m_2.m_noGps && !pFindClosest->bIgnoreNoGps)
		return false;
	if(pNode->m_2.m_waterNode && pFindClosest->bIgnoreWater)
		return false;
	if(pFindClosest->bIgnoreOneWay && pNode->NumLinks()==2 && ThePaths.GetNodesLink(pNode, 0).IsOneWay() && ThePaths.GetNodesLink(pNode, 1).IsOneWay())
		return false;
	if(pFindClosest->bIgnoreJunctions && pNode->IsJunctionNode())
		return false;

	Vector3 vNode, vOtherNode, vToOther, vClosest;
	pNode->GetCoors(vNode);

	if(pFindClosest->pBestNodeSoFar && (pFindClosest->fDistToClosestPointOnNearbyLinks*pFindClosest->fDistToClosestPointOnNearbyLinks*2) < (pFindClosest->vSearchOrigin - vNode).XYMag2())
	{
		// Skip this node, it's really far away - chances are it's links won't be good enough to justify testing it at all
		return true;
	}

	for(s32 l=0; l<pNode->NumLinks(); l++)
	{
		CPathNodeLink link = ThePaths.GetNodesLink(pNode, l);
		if(link.m_1.m_bDontUseForNavigation && pFindClosest->bIgnoreNoNav)
			continue;

		const CPathNode * pOtherNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);
		if(pOtherNode)
		{
			if(pFindClosest->bIgnoreJunctions && pOtherNode->IsJunctionNode())
				continue;

			pOtherNode->GetCoors(vOtherNode);
			vToOther = vOtherNode - vNode;
			if(vToOther.Mag2() > SMALL_FLOAT)
			{
				float fDot;
				if(pFindClosest->bIgnoreHeading)
				{
					fDot = 1.0f;
				}
				else
				{
					Vector3 vToOtherUnit = link.m_1.m_LanesToOtherNode == 0 ? -vToOther : vToOther;

					vToOtherUnit.NormalizeFast();
					fDot = DotProduct(pFindClosest->vSearchDir, vToOtherUnit);
				}

			//	if(fDot >= 0.0f)
				{
					const float T = geomTValues::FindTValueSegToPoint(vNode, vToOther, pFindClosest->vSearchOrigin);
					vClosest = vNode + (vToOther * T);
					Vector3 vDiff = (vClosest - pFindClosest->vSearchOrigin);
					float fOnLaneWeight = 1.f;
					float fMinDist = 1.25f;	// Can never be closer to a link than x meter unless... A min value makes the weights more sane for links crossing our path

					// We test here if we are on the actual lane for the link and if so we get a better fitnessvalue for this node
					if(fDot >= 0.25f)
					{
						fMinDist = 0.1f;	// 

						// Get the road widths.
						float drivableWidthOnLeft = 0.0f;
						float drivableWidthOnRight = 0.0f;
						ThePaths.FindRoadBoundaries(link, drivableWidthOnLeft, drivableWidthOnRight);

						Vector3 LinkRightVec = vToOther;
						LinkRightVec.Cross(ZAXIS);

						float fDistToLane = vDiff.XYMag();	// Yes sqrt here, think we save some cycles this way
						float fMinLaneDist = link.InitialLaneCenterOffset() - link.GetLaneWidth() * 0.5f;						
						float fMaxLaneDist = ((vDiff.Dot(LinkRightVec) > 0.f) ? drivableWidthOnRight : drivableWidthOnLeft);
						if (fMaxLaneDist > fDistToLane && fMinLaneDist < fDistToLane)
						{
							static dev_float DistEps = 0.6f;							
							fOnLaneWeight = DistEps;
						}
					}

					// Only consider this ZPenalty if the point is fairly close to us,
					dev_float fDistToConsider = 35.f;
					vDiff.z = Min(abs(vDiff.z), fDistToConsider);
					vDiff.z *= pFindClosest->fDistZPenalty;

					float fDist = Max(vDiff.Mag(), fMinDist);
					float fDistXY = Max(vDiff.XYMag(), fMinDist);

					// Factor in the distance from the actual node to the position on the link
					static dev_float fPenaltyAlongSeg = 0.25f;
					fDist += (vClosest - vNode).Mag() * fPenaltyAlongSeg;

					float fHeadingPenalty = (pNode->IsHighway() ? pFindClosest->fHeadingPenalty * 0.5f : pFindClosest->fHeadingPenalty);

					// Apply penalty for heading
					if(fDot >= 0.25f)
						fDist += (1.0f - fDot) * fHeadingPenalty;
					else
						fDist += fHeadingPenalty * 2.f;

					static dev_float fOffNodeWeight = 0.75f;	
					float fOffPenaltyParent = ((pNode->m_2.m_switchedOff || pOtherNode->m_2.m_switchedOff) ? fOffNodeWeight : 0.f);

					fDist *= fOnLaneWeight;
					fDist += fDistXY * fOffPenaltyParent;

					if(fDist < pFindClosest->fDistToClosestPointOnNearbyLinks)
					{
						pFindClosest->fDistToClosestPointOnNearbyLinks = fDist;
						pFindClosest->vClosestPointOnNearbyLinks = vClosest;
						pFindClosest->pBestNodeSoFar = pNode;
						pFindClosest->resultNodeAddress = pNode->m_address;
					}
				}
			}
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateRoute
// PURPOSE :  If the player is making progress along the route we update the nodes
//			  accordingly.
/////////////////////////////////////////////////////////////////////////////////

bank_s32 CGpsSlot::m_iMinDurationBeforeProgressingRoute						= 3000;
bank_s32 CGpsSlot::m_iMinDurationBetweenRecalculatingRoute					= 3000;
bank_s32 CGpsSlot::m_iMaxDurationBeforeRedrawingRoute						= 250;
bank_s32 CGpsSlot::m_iMaxZDistanceBeforeRecalculatingRoute					= 7;
bank_s32 CGpsSlot::m_iTargetEntityMaxDistMayMoveBeforeRecalculatingRoute	= 15;
bank_s32 CGpsSlot::m_iPlayerMaxDistMayMoveBeforeRecalculatingRoute			= 30;
bank_s32 CGpsSlot::m_iCheckNumSegments										= 22;	// 11;
bank_s32 CGpsSlot::m_iScanNodesFreqMs										= 1000;

void CGpsSlot::UpdateRoute()
{
	if (!IsOn())
	{
		return;
	}

	// see if we need to recalculate how far our route is
	if (m_LastShiftRouteTime == 0)
	{
		CalcDistanceToTarget();
		m_LastShiftRouteTime = 1;
	}

#if __BANK
	TUNE_GROUP_BOOL(GPS, bDebugClosestWayPoint, false);
	if(bDebugClosestWayPoint)
	{
		TUNE_GROUP_FLOAT(GPS, fDistanceAlongRouteOverride, 50.0f, -1.0f, 500.0f, 0.1f);
		TUNE_GROUP_BOOL(GPS, bStartAtPlayerPosition, true);
		Vector3 vGPSPos = GetNextNodePositionAlongRoute(bStartAtPlayerPosition, fDistanceAlongRouteOverride);
		vGPSPos.z += 1.25f;
		grcDebugDraw::Sphere(vGPSPos, 1.25f, Color_red);
	}
#endif

	// We need to shift the route even if we are calculating a new one or trailing away from it or it might leave long trails behind us
	bool bShiftedRoute = false;
	float fSmallestDistSqr = FLT_MAX;
	s32	iSmallestDistSegment = CalcClosestSegment(0, &fSmallestDistSqr, (m_LastShiftRouteTime == 1 && ThePaths.bIgnoreNoGpsFlagUntilFirstNode));
	if (m_NumNodes > 0 && iSmallestDistSegment > 0 && fwTimer::GetTimeInMilliseconds() > m_LastShiftRouteTime+m_iMinDurationBeforeProgressingRoute)
	{
		ShiftRoute(iSmallestDistSegment);
		CalcDistanceToTarget();
		bShiftedRoute = true;
	}

	// redraw the GPS route every N seconds (if not in frontend map)
	if (fwTimer::GetTimeInMilliseconds() > m_RedrawTimer+m_iMaxDurationBeforeRedrawingRoute)
	{
		// If we're going to redraw due to a time delta, then ensure that we shift the
		// route if we've progressed along it..
		if(!bShiftedRoute && iSmallestDistSegment > 0)
		{
			ShiftRoute(iSmallestDistSegment);
		}

		//if (GetStatus() == STATUS_WORKING || GetStatus() == STATUS_WORKING_CUSTOM_ROUTE)
		//{
		//	const CPed * pPlayer = CGameWorld::FindLocalPlayer();
		//	if (pPlayer && m_NumNodes > 1)
		//	{
		//		const CVehicle * pVeh = pPlayer->GetVehiclePedInside();
		//		const Vector3 vSrcCoors = VEC3V_TO_VECTOR3(pVeh ? pVeh->GetTransform().GetPosition() : pPlayer->GetTransform().GetPosition());
		//		Vector3 vNodeDir = m_NodeCoordinates[0] - m_NodeCoordinates[1];
		//		vNodeDir.z = 0;
		//		vNodeDir.Normalize();

		//		Vector3 vScrMoveDir = vSrcCoors - m_NodeCoordinates[0];
		//		vScrMoveDir.z = 0;
		//		vScrMoveDir.Normalize();

		//		if (vScrMoveDir.Dot(vNodeDir) > 0.707f && 
		//			vSrcCoors.Dist2(m_NodeCoordinates[0]) < 7.5f * 7.5f &&	// 
		//			ThePaths.FindTrailingLinkWithinDistance(m_NodeAdress[0], vSrcCoors))
		//		{
		//			(*(int*)&m_NodeCoordinates[0].w) = GNI_PLAYER_TRAIL_POS;
		//		}

		//		if (m_pTargetEntity && m_NumNodes < CGps::ms_GpsNumNodesStored - 1)
		//		{
		//			const Vector3 vDestCoors = VEC3V_TO_VECTOR3(m_pTargetEntity->GetTransform().GetPosition());
		//			if (ThePaths.FindTrailingLinkWithinDistance(m_NodeAdress[m_NumNodes - 1], vDestCoors))
		//			{
		//				(*(int*)&m_NodeCoordinates[m_NumNodes - 1].w) = GNI_PLAYER_TRAIL_POS;
		//				m_NodeCoordinates[m_NumNodes] = vDestCoors;	// Intentional, we write this to the next node
		//			}
		//		}
		//	}
		//}

		SetCurrentRenderStatus(false);
		m_RedrawTimer = fwTimer::GetTimeInMilliseconds();
	}
	//

	if (!IsWorking())
	{
		return;
	}

	// If this multi route never updates, then simply quit out here
	if(!CGps::m_bMultiGpsTracksPlayer && IsMultiRoute())
	{
		return;
	}

	// We never track the player along race-track routes
	if(IsRaceTrackRoute())
	{
		m_iClosestRaceTrackSegment = Min(Max(CalcClosestSegment(m_iClosestRaceTrackSegment), (s32)0), m_NumNodes - 1);
		return;
	}

	// If we have a pEntity and the entity has moved a certain amount we recalculate the route.
	if (m_pTargetEntity)
	{
		Assertf(GetStatus() != STATUS_WORKING_RACE_TRACK, "You can't have a target entity for a GPS race-track route.  Did you remember to switch off the race track route first?");
		Assertf(GetStatus() != STATUS_WORKING_ROUTE_MULTI, "You can't have a target entity for a GPS multi-route.  Did you remember to switch off the race multi-route first?");
		Assertf(GetStatus() != STATUS_WORKING_CUSTOM_ROUTE, "You can't have a target entity for a GPS custom route.  Did you remember to switch off the custom route first?");

		const bool bCurrentDestinationInvalid = m_Destination.IsClose(g_vEntityPositionInvalid, 1.0f);

		const Vector3 vTargetEntityPosition = VEC3V_TO_VECTOR3(m_pTargetEntity->GetTransform().GetPosition());
		float distMoved = (vTargetEntityPosition - m_Destination).XYMag2();

		if (bCurrentDestinationInvalid ||
			(distMoved > rage::square(m_iTargetEntityMaxDistMayMoveBeforeRecalculatingRoute) &&
			fwTimer::GetTimeInMilliseconds() > m_LastRecalcRouteTime+m_iMinDurationBetweenRecalculatingRoute) )
		{
			m_Destination = vTargetEntityPosition;

			SetStatus(STATUS_CALCULATING_ROUTE_QUIET);
			SetCurrentRenderStatus(false);
			m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds();
		}
	}

	const Vector3 vPlayerCoors = CGps::GetRelevantPlayerCoords();

	if(GetStatus() == STATUS_WORKING_CUSTOM_ROUTE)
	{
		CPed * pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer)
		{
			if(pPlayer->GetIsInVehicle() && ((m_uCustomRouteFlags & GPS_ROUTE_FLAG_IN_VEHICLE)==0))
				return;
			if(!pPlayer->GetIsInVehicle() && ((m_uCustomRouteFlags & GPS_ROUTE_FLAG_ON_FOOT)==0))
				return;
		}
	}

	if(GetStatus() == STATUS_WORKING_CUSTOM_ROUTE && iSmallestDistSegment == 0)
	{
		Vector3 & v1 = m_NodeCoordinates[iSmallestDistSegment];
		Vector3 & v2 = m_NodeCoordinates[iSmallestDistSegment+1];
		const Vector3 vDiff = v2-v1;
		const float T = (vDiff.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(v1, vDiff, vPlayerCoors) : 0.0f;
		const Vector3 vPos = v1 + ((v2-v1)*T);
		m_NodeCoordinates[iSmallestDistSegment] = vPos;
	}


	// if we only have a partial route
	if(GetStatus() == STATUS_WORKING && m_bUsePartialDestination)
	{
		if(fwTimer::GetTimeInMilliseconds() > m_LastPartialRouteTime+m_iMinDurationBetweenRecalculatingRoute || CPauseMenu::IsActive() || CMiniMap::IsInBigMap() ||
			( m_NodeDistanceToTarget[0] <= PARTIAL_DESTINATION_RECALCULATE_LENGTH))
		{
			// recalculate the full route
			m_bUsePartialDestination = false;
			SetStatus(STATUS_CALCULATING_ROUTE_QUIET);
			m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds();
			return;
		}
	}


	// If the player has strayed too far from the route we should recalculate it.
	if ( GetStatus() != STATUS_WORKING_CUSTOM_ROUTE &&
		fSmallestDistSqr > rage::square(m_iPlayerMaxDistMayMoveBeforeRecalculatingRoute) &&
			fwTimer::GetTimeInMilliseconds() > m_LastRecalcRouteTime+m_iMinDurationBetweenRecalculatingRoute )
	{
		if(GetStatus() == STATUS_WORKING_RACE_TRACK)
			SetStatus(STATUS_CALCULATING_ROUTE_RACE_TRACK);
		else if(GetStatus() == STATUS_WORKING_ROUTE_MULTI)
			SetStatus(STATUS_CALCULATING_ROUTE_MULTI);
		else
		{
			if(!m_bUsePartialDestination && m_NumNodes > 1)
			{
				SetCalculatePartialRouteAndDestination();
			}
			
			SetStatus(STATUS_CALCULATING_ROUTE_QUIET);
		}

		m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds();
		return;
	}

	const CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

	// If the player has just entered a vehicle recalculate the route
	if(pPlayerPed && pPlayerPed->GetVehiclePedInside() && GetStatus() != STATUS_WORKING_CUSTOM_ROUTE)
	{
		if (!m_InCarLastUpdate)
		{
			m_InCarLastUpdate = true;

			// If we just calculated a route we wouldn't want to calculate it again this very frame
			if (GetStatus() == STATUS_WORKING && m_LastRecalcRouteTime == fwTimer::GetTimeInMilliseconds())
				return;

			if(GetStatus() == STATUS_WORKING_RACE_TRACK)
				SetStatus(STATUS_CALCULATING_ROUTE_RACE_TRACK);
			else if(GetStatus() == STATUS_WORKING_ROUTE_MULTI)
				SetStatus(STATUS_CALCULATING_ROUTE_MULTI);
			else
				SetStatus(STATUS_CALCULATING_ROUTE);	// Don't if we just updated this very frame

			m_NumNodes = 0; // Don't render junc stuff from when you are on foot
			SetCurrentRenderStatus(false);
			m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds() - m_iMinDurationBetweenRecalculatingRoute;
			return;
		}
	}
	else
	{
		if (m_InCarLastUpdate)
		{
			// remove the route from the minimap as no longer in a car
			CGps::ClearGpsOnMiniMap(m_SlotIndex);
			m_InCarLastUpdate = false;
			return;
		}
	}

	//------------------------------------------------------------------------------------------------------
	// Periodically Scan nearby nodes using same logic as CPathFind uses to determine GPS start/end nodes
	// If the found nodes are not colinear with our current route, then we must have left the route onto
	// another road.  Force an immediate update in this case.

	CNodeAddress addr; // how about some fucking info on this variable, ProDG?

	if(fwTimer::GetTimeInMilliseconds() > m_iScanNodesTimer+m_iScanNodesFreqMs)
	{
		if(pPlayerPed && (GetStatus()==STATUS_WORKING || GetStatus()==STATUS_WORKING_ROUTE_MULTI))
		{
			const CVehicle * pVeh = pPlayerPed->GetVehiclePedInside();
			// const Vector3 srcCoors = VEC3V_TO_VECTOR3((pVeh) ? pVeh->GetTransform().GetPosition() : pPlayerPed->GetTransform().GetPosition());
			Vec3V vDirFlat = (pVeh)?pVeh->GetTransform().GetB() : pPlayerPed->GetTransform().GetB();
			Vector3	srcDirFlat = RC_VECTOR3(vDirFlat);
			srcDirFlat.z = 0.0f;
			srcDirFlat.Normalize();

			Vector3 vClosestPos;

			// Run an async search for the node closest to the player
			CPathNode * pSourceNode = NULL;
			if(ThePaths.AreNodesLoadedForRadius(vPlayerCoors, STREAMING_PATH_NODES_DIST_PLAYER))
			{
				const bool bIgnoreNoGps = ThePaths.bIgnoreNoGpsFlag && (m_SlotIndex==GPS_SLOT_RADAR_BLIP);
				// only do the search if the nodes are loaded - they should be because we're searching for the node closest to the player
				pSourceNode = ThePaths.FindRouteEndNodeForGpsSearchAsync(vPlayerCoors, srcDirFlat, false, &vClosestPos, CPathFind::GPS_ASYNC_SEARCH_PERIODIC, bIgnoreNoGps);
				CGps::m_bASyncStatusSearchSlot[CPathFind::GPS_ASYNC_SEARCH_PERIODIC] = true;
			}
			
			if(pSourceNode)
			{
				m_iScanNodesTimer = fwTimer::GetTimeInMilliseconds();	// Only reset timer once we got a node or this will "lag" too much
				CGps::m_bASyncStatusSearchSlot[CPathFind::GPS_ASYNC_SEARCH_PERIODIC] = false;

				// Search completed, clean up the search slot
				ThePaths.ClearGPSSearchSlot();

				Assert(!vClosestPos.IsClose(VEC3_ZERO, 0.01f));
#if __BANK
				if (CGps::m_bDebug)
				{
					grcDebugDraw::Sphere(vClosestPos + ZAXIS, 0.25, Color_azure, true);
				}
#endif
				// if the closest point is close to the first node of our path, we're still OK, don't force a recalc
				// Note the the closest point might not be on a node, but checking this can save us from doing a painful recalc
				if(vClosestPos.IsClose(m_NodeCoordinates[0],0.01f)) 
				{					
					return;
				}

				static dev_float fMaxDist = 4.0f;
				float fDistScale = 1.f;

				Vector3 vNode = vClosestPos;
				vNode.z = 0.0f;

				bool bBehindFirstNode = false;

				// If the closest position we have found is behind the first GPS route segment, then abandon
				// our test.. We don't want to force the GPS to update more frequently than normal when driving
				// away from our route.
				if(m_NumNodes >= 2)
				{
					Vector3 vNormal = m_NodeCoordinates[1] - m_NodeCoordinates[0];
					if(vNormal.Mag2() > SMALL_FLOAT)
					{
						vNormal.Normalize();
						const float fPlaneD = - DotProduct(vNormal, m_NodeCoordinates[0]);

						fDistScale = DotProduct(vNormal, srcDirFlat);
						if(DotProduct(vNode, vNormal) + fPlaneD < 0.0f && fDistScale > 0.25f) // ~ 75 degree
						{
							bBehindFirstNode = true;
						}
					}
				}

				if(!bBehindFirstNode)
				{
					fDistScale = Max(fDistScale, 0.5f);
					const s32 iNumNodes = Min(m_NumNodes,10);
					s32 n;

					for(n=1; n<iNumNodes; n++)
					{
						Vector3 vStart;
						if(ThePaths.IsRegionLoaded(m_NodeAdress[n-1].GetRegion()))
						{
							ThePaths.apRegions[m_NodeAdress[n-1].GetRegion()]->aNodes[m_NodeAdress[n-1].GetIndex()].GetCoors(vStart);
						}
						else
							continue;
						vStart.z = 0.0f;

						Vector3 vSeg;
						if(ThePaths.IsRegionLoaded(m_NodeAdress[n].GetRegion()))
						{
							ThePaths.apRegions[m_NodeAdress[n].GetRegion()]->aNodes[m_NodeAdress[n].GetIndex()].GetCoors(vSeg);
							vSeg = vSeg - vStart;
						}
						else
							continue;

						vSeg.z = 0.0f;

						if(vSeg.Mag2() > SMALL_FLOAT)
						{
							const float fDist = geomDistances::DistanceSegToPoint(vStart, vSeg, vNode);
							if(fDist < fMaxDist * fDistScale)
								break;
						}
					}

					if(n == iNumNodes)
					{
						if(GetStatus() == STATUS_WORKING_RACE_TRACK)
							SetStatus(STATUS_CALCULATING_ROUTE_RACE_TRACK);
						else if(GetStatus() == STATUS_WORKING_ROUTE_MULTI)
							SetStatus(STATUS_CALCULATING_ROUTE_MULTI);
						else
						{
							if(!m_bUsePartialDestination && m_NumNodes > 1)
							{
								SetCalculatePartialRouteAndDestination();
							}
							
							SetStatus(STATUS_CALCULATING_ROUTE_QUIET);					
						}
#if __BANK
						if (CGps::m_bDebug)
						{
							grcDebugDraw::Sphere(vClosestPos + ZAXIS, 0.35f, Color_tomato2, true);
							grcDebugDraw::Text(vPlayerCoors, Color_tomato2, 0, 0, "GPS : recalculating route", true, 90 );
						}
#endif // __BANK
						m_LastRecalcRouteTime = fwTimer::GetTimeInMilliseconds();
						return;
					}
				}
			}
		}
	}
}

Vector3 CGpsSlot::GetNextNodePositionAlongRoute(bool bStartAtPlayerPosition, float fDistanceAlongRoute)
{
	const Vector3 vPlayerCoors = CGps::GetRelevantPlayerCoords();

	if(m_NumNodes > 1)
	{	
		//! if we starting at current position, start at local player coords and work along to 1st point.
		if(bStartAtPlayerPosition)
		{
			//! get distance from here to next node.
			Vector3	vNextNodeXY = m_NodeCoordinates[1]; //next point is at index 1.
			Vector3 vDelta = vNextNodeXY-vPlayerCoors;
			float fDist = vDelta.Mag();
			if(fDist > fDistanceAlongRoute)
			{
				vDelta.NormalizeSafe();
				return (vPlayerCoors + (vDelta * fDistanceAlongRoute));
			}

			fDistanceAlongRoute -= fDist; //remove from dist.
		}

		s32 iNumSegments = m_NumNodes - 1;

		//! walk through node list to find position.
		for (s32 i = 1; i < iNumSegments && fDistanceAlongRoute > 0.0f; i++)
		{
#if __BANK
			TUNE_GROUP_BOOL(GPS, bRenderNodes, false);
			if(bRenderNodes)
			{
				Vector3 vDebugNodePos = m_NodeCoordinates[i];
				vDebugNodePos.z += 1.0f;
				grcDebugDraw::Sphere(vDebugNodePos, 1.0f, Color_green);
			}
#endif

			Vector3	vNode1 = m_NodeCoordinates[i];
			Vector3	vNode2 = m_NodeCoordinates[i+1];

			Vector3 vDelta = vNode2 - vNode1;
			float fDist = vDelta.Mag();
			if(fDist > fDistanceAlongRoute)
			{
				vDelta.NormalizeSafe();
				return (vNode1 + (vDelta * fDistanceAlongRoute));
			}

			fDistanceAlongRoute -= fDist;
		}

		//! return last node.
		return m_NodeCoordinates[m_NumNodes-1];
	}
	
	//! return destination node. Note: height can be invalid, so use player pos.
	Vector3 vDestination = m_Destination;
	vDestination.z = vPlayerCoors.z;
	return vDestination;
}

s32 CGpsSlot::CalcClosestSegment(s32 iStartFromSeg, float* pfOutSmallestDistSqr, bool bWholeRoute)
{
	float fSmallestDistSqr = FLT_MAX;
	float fSmallestDistSqrXY = FLT_MAX;
	s32	iSmallestDistSegment = -1;

	// Calculate the players distance to the next x line segments.
	// Pick the closest to it and fast forward to that one
	const Vector3 vPlayerCoors = CGps::GetRelevantPlayerCoords();
	//float fZDelta = (float)m_iMaxZDistanceBeforeRecalculatingRoute;

	//if(GetStatus() == STATUS_WORKING_CUSTOM_ROUTE)
	//{
	//	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	//	if(pPlayer)
	//		fZDelta = pPlayer && pPlayer->GetIsInVehicle() ? m_iMaxZDistanceBeforeRecalculatingRoute : 3.0f;
	//}

	iStartFromSeg = Min(Max(iStartFromSeg, 0), m_NumNodes - 1);	// Clamp, might want to handle wrapping later on
	s32	iNumSegments = Min(m_iCheckNumSegments + iStartFromSeg, m_NumNodes - 1);
	if (bWholeRoute)
		iNumSegments = m_NumNodes - 1;

	for (s32 i = iStartFromSeg; i < iNumSegments; i++)
	{
		Vector3	vNode1XY = Vector3(m_NodeCoordinates[i].x, m_NodeCoordinates[i].y, 0.0f);
		Vector3	vNode2XY = Vector3(m_NodeCoordinates[i+1].x, m_NodeCoordinates[i+1].y, 0.0f);


		const Vector3 vDeltaXY = vNode2XY-vNode1XY;
		const float T = (vDeltaXY.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vNode1XY, vDeltaXY, vPlayerCoors) : 0.0f;
		const Vector3 vClosestPosZ = m_NodeCoordinates[i] + ( (m_NodeCoordinates[i+1] - m_NodeCoordinates[i]) * T);
		float fDistZ = Abs(vClosestPosZ.z - vPlayerCoors.z);
		const float fZWeight = 5.0f;

		Vector3 vClosestPos = vNode1XY + (vDeltaXY*T);

		// Only consider this ZPenalty if the point is fairly close to us,
		dev_float fDistToConsider = 35.f;
		fDistZ = Min(fDistZ, fDistToConsider);
		fDistZ *= fZWeight;

		vClosestPos.z = vPlayerCoors.z + fDistZ;

		const float fDistSqr = vClosestPos.Dist2(vPlayerCoors);
		const float fDistSqrXY = (vClosestPos - vPlayerCoors).XYMag2();

		// Make sure this node is within Z distance and it's the smallest distance
		if ( fDistSqr < fSmallestDistSqr/* && fDistZ < fZDelta */)
		{
			if( i != m_iCheckNumSegments-1 )
			{
				if(fDistSqrXY > SMALL_FLOAT)
				{
					Assert(fDistSqrXY == fDistSqrXY);
					m_DistanceFromRoute = Sqrtf(fDistSqrXY);
				}
				else
				{
					m_DistanceFromRoute = 0.0f;
				}
			}
			fSmallestDistSqr = fDistSqr;
			fSmallestDistSqrXY = fDistSqrXY;
			iSmallestDistSegment = i;
		}
	}

	if (pfOutSmallestDistSqr)
		*pfOutSmallestDistSqr = fSmallestDistSqrXY;


	// If we are close to the destination and not near the route we shift to completion
	if (GetStatus() == CGpsSlot::STATUS_WORKING && m_NumNodes > 1)
	{
		if (m_LastRouteDistance > 1.f)
		{
			float fLastSegDistToDestSqr = (m_NodeCoordinates[m_NumNodes - 1] - m_Destination).XYMag2();
			if (fLastSegDistToDestSqr > m_LastRouteDistance * m_LastRouteDistance)
				iSmallestDistSegment = m_NumNodes - 1;
		}
	}

	return iSmallestDistSegment;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ShiftRoute
// PURPOSE :  Shift the route by a number of points.
/////////////////////////////////////////////////////////////////////////////////
void CGpsSlot::ShiftRoute(s32 number)
{
	Assert(number > 0);
	Assert(number <= m_NumNodes);

	if (m_iGpsFlags & GPS_FLAG_NO_ROUTE_SHIFT)
	{
		m_LastShiftRouteTime = fwTimer::GetTimeInMilliseconds();
		SetCurrentRenderStatus(false);  // redraw the route
		return;
	}

	s32 i;
	for (i = 0; i < m_NumNodes - number; i++)
	{
		m_NodeCoordinates[i] = m_NodeCoordinates[i+number];
		m_NodeAdress[i] = m_NodeAdress[i+number];
		m_NodeInfo[i] = m_NodeInfo[i+number];
		m_NodeDistanceToTarget[i] = m_NodeDistanceToTarget[i+number];
	}
	m_NumNodes -= number;
	m_LastAnnouncedNode -= number;
	m_LastBongedNode -= number;

	m_LastShiftRouteTime = fwTimer::GetTimeInMilliseconds();

	SetCurrentRenderStatus(false);  // redraw the route

	// Handle removing completed destination waypoints for multi-routes
	if(IsMultiRoute())
	{
		for(i=0; i<CGps::m_iNumCustomAddedPoints; i++)
		{
			CGps::m_iMultiRouteStartNodeProgress[i] -= number;
		}

		// So clear any possible points that don't really have a path between
		for(i=1; i<CGps::m_iNumCustomAddedPoints; )
		{
			if (CGps::m_iMultiRouteStartNodeProgress[i] - CGps::m_iMultiRouteStartNodeProgress[i-1] <= 0)
			{
				for(int p=i; p<CGps::m_iNumCustomAddedPoints-1; ++p)
				{
					CGps::m_aCustomAddedPoints[p] = CGps::m_aCustomAddedPoints[p+1];
					CGps::m_aCustomAddedNodes[p] = CGps::m_aCustomAddedNodes[p+1];
					CGps::m_iMultiRouteStartNodeProgress[p] = CGps::m_iMultiRouteStartNodeProgress[p+1];
				}
				CGps::m_iNumCustomAddedPoints--;
			}
			else
			{
				++i;
			}
		}

		while( CGps::m_iMultiRouteStartNodeProgress[0] <= 0 && CGps::m_iNumCustomAddedPoints > 0 )
		{
			for(int p=1; p<CGps::m_iNumCustomAddedPoints; p++)
			{
				CGps::m_aCustomAddedPoints[p-1] = CGps::m_aCustomAddedPoints[p];
				CGps::m_aCustomAddedNodes[p-1] = CGps::m_aCustomAddedNodes[p];
				CGps::m_iMultiRouteStartNodeProgress[p-1] = CGps::m_iMultiRouteStartNodeProgress[p];
			}
			CGps::m_iNumCustomAddedPoints--;
		}
	}
}

void CGpsSlot::SetCurrentRenderStatus(bool value)
{
	m_bCurrentRenderStatus = value;
	if(!value)
	{
		m_bRenderedLastFrame = false;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetDistanceToTarget
// PURPOSE :  Get the distance to the target
// RETURN : 0 if no route or the distance along the route
/////////////////////////////////////////////////////////////////////////////////
s16 CGpsSlot::GetDistanceToTarget()
{
	if(IsOn() && m_NumNodes > 1)//make sure we have some nodes left to check against
	{
		return static_cast<s16>(m_LastRouteDistance);
	}
	else if (GetStatus() == CGpsSlot::STATUS_WORKING)
	{
		Vector3	playerCoors = CGps::GetRelevantPlayerCoords();
		playerCoors.z = 0.f;
		return static_cast<s16>((m_Destination - playerCoors).XYMag());
	}

	return 0;
}

void CGpsSlot::CalcDistanceToTarget()
{
	if(IsOn() && m_NumNodes > 1)//make sure we have some nodes left to check against
	{
		if(m_bUsePartialDestination)
			return; // don't update the distance for partial routes

		Vector3	playerCoors = CGps::GetRelevantPlayerCoords();
		playerCoors.z = 0.f;

		//get distance to next point
		int iSegment = 0;
		int iNext = 1;
		int iEndNode = m_NumNodes - 1;
		if (GetStatus() == STATUS_WORKING_RACE_TRACK)
			iSegment = Min(m_iClosestRaceTrackSegment + 1, m_NumNodes - 1);

		// Get distance but make it more smooth
		TUNE_GROUP_FLOAT(GPS_DISTANCE_TWEAK, DistEps, 50.0f, 1.0f, 150.0f, 1.0f);
		TUNE_GROUP_FLOAT(GPS_DISTANCE_TWEAK, LerpRatio, 0.94f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(GPS_DISTANCE_TWEAK, SpeedThreshold, 10.0f, 1.0f, 100.0f, 1.0f);
		TUNE_GROUP_FLOAT(GPS_DISTANCE_TWEAK, LowSpeedLerpRatio, 0.87f, 0.0f, 1.0f, 0.1f);

		// So we check the seg between last node and destination against our current seg on route
		// and use that to determine which distance we should consider
		// There are probably better ways to do this
		const Vector3 vNode1XY = Vector3(m_Destination.x, m_Destination.y, 0.0f);
		const Vector3 vNode2XY = Vector3(m_NodeCoordinates[iEndNode].x, m_NodeCoordinates[iEndNode].y, 0.0f);
		const Vector3 vCurr1XY = Vector3(m_NodeCoordinates[0].x, m_NodeCoordinates[0].y, 0.0f);
		const Vector3 vCurr2XY = Vector3(m_NodeCoordinates[1].x, m_NodeCoordinates[1].y, 0.0f);

		const Vector3 vDeltaXY = vNode2XY - vNode1XY;
		const Vector3 vCurrDeltaXY = vCurr2XY - vCurr1XY;

		const float T = (vDeltaXY.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vNode1XY, vDeltaXY, playerCoors) : 0.0f;
		const float T2 = (vCurrDeltaXY.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vCurr1XY, vCurrDeltaXY, playerCoors) : 0.0f;
		const Vector3 vClosestPointOnLine = vNode1XY + (vDeltaXY * T);
		const Vector3 vCurrClosestPointOnLine = vCurr1XY + (vCurrDeltaXY * T2);

		Vector3 vToNextNode = m_NodeCoordinates[iNext] - vCurrClosestPointOnLine;
		Vector3 vToClosestPoint = vClosestPointOnLine - playerCoors;
		Vector3 vCurrToClosestPoint = vCurrClosestPointOnLine - playerCoors;

		float fDistance = 0.f;
		if (vCurrToClosestPoint.XYMag2() > vToClosestPoint.XYMag2() && (GetStatus() == CGpsSlot::STATUS_WORKING))
			fDistance = (vClosestPointOnLine - m_Destination).XYMag() + vToClosestPoint.XYMag();
		else
			fDistance = static_cast<float>(m_NodeDistanceToTarget[iNext]) +  vToNextNode.XYMag() + vCurrToClosestPoint.XYMag() + ((GetStatus() == CGpsSlot::STATUS_WORKING) ? (vDeltaXY).XYMag() : 0.f);

		float fLastRouteDist = m_LastRouteDistance;
		if (abs(fLastRouteDist - fDistance) > DistEps)
			fLastRouteDist = fDistance;
		
		float LerpRatioAdapted = LerpRatio;
		if (CGameWorld::FindLocalPlayerSpeed().Mag2() < SpeedThreshold * SpeedThreshold)
			LerpRatioAdapted = LowSpeedLerpRatio;	// Maybe we should have a linear interpolation here

		float fFinalRouteDist = fLastRouteDist * LerpRatioAdapted + fDistance * (1.f - LerpRatioAdapted);

		m_LastRouteDistance = fFinalRouteDist;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CheckForReachingDestination
// PURPOSE :  checks whether we need to remove the blip when we reach the
//				destination - only needed for the marker slot at the moment
/////////////////////////////////////////////////////////////////////////////////
bool CGpsSlot::CheckForReachingDestination()
{
	sWaypointStruct Waypoint;
	if (!CMiniMap::IsInPauseMap() &&
		!CMiniMap::IsInCustomMap() &&
		m_SlotIndex == GPS_SLOT_WAYPOINT &&
		!IsRaceTrackRoute() &&
		CMiniMap::GetActiveWaypoint(Waypoint))
	{
#define __DISTANCE_TO_ARRIVE_AT_DESTINATION (50.0f)

		switch (CMiniMap::GetWaypointClearOnArrivalMode())
		{
		case eWaypointClearOnArrivalMode::Enabled:
			break;
		case eWaypointClearOnArrivalMode::DisabledForObjects:
			if (Waypoint.ObjectId != NETWORK_INVALID_OBJECT_ID)
			{
				return false;
			}
			break;
		default:
			return false;
		}

		Vector3	playerCoors = CGps::GetRelevantPlayerCoords();
		bool bArrivedAtDestination = false;

		if (m_NumNodes > 0)
		{
			Vector3 targetCoords = m_NodeCoordinates[m_NumNodes-1];
			f32 distanceFromDestinationSq = (targetCoords - playerCoors).XYMag2();
			if (distanceFromDestinationSq<(g_ArrivedDistance*g_ArrivedDistance) && rage::Abs(targetCoords.z - playerCoors.z) < __DISTANCE_TO_ARRIVE_AT_DESTINATION)
				bArrivedAtDestination = true;
		}

		if (!bArrivedAtDestination)
		{
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(Waypoint.iBlipIndex);

			if (pBlip)
			{
				Vector3 targetCoords = CMiniMap::GetBlipPositionValue(pBlip);
				f32 distanceFromDestinationSq = (targetCoords - playerCoors).XYMag2();
				if (distanceFromDestinationSq<(g_ArrivedDistance*g_ArrivedDistance))	// A waypoint placed on the minimap has no good Z
					bArrivedAtDestination = true;
			}
		}

		if (bArrivedAtDestination)
		{
			//if (m_SlotIndex != GPS_SLOT_RADAR_BLIP)
			uiDisplayf("CGpsSlot::CheckForReachingDestination - Clearing Waypoint");
			CMiniMap::SwitchOffWaypoint();

			// remove the route from the minimap
			CGps::ClearGpsOnMiniMap(m_SlotIndex);
			SetStatus(STATUS_OFF);
			return true;
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalculatePartialRouteDestination
// PURPOSE :  Calculate the point on the current route that we want to use
//			as the partial destination for our next few recalculates
/////////////////////////////////////////////////////////////////////////////////
void CGpsSlot::SetCalculatePartialRouteAndDestination()
{
#if COMPANION_APP
	//	Partial route optimisation causes problems with second screen/smartglass apps
	//	So just drop out here
	return;
#else	//	COMPANION_APP

	if(CPauseMenu::IsActive())
		return; // no partial routes while in the pause menu

	if(CMiniMap::IsInBigMap())
		return; // or when the big map is showing

	Assert(m_NumNodes > 1);
	// store the partial route destination
	s32 partialNodeIndex = m_NumNodes;

	const Vector3 vPlayerCoors = CGps::GetRelevantPlayerCoords();
	s16 maxDistanceToTarget = m_NodeDistanceToTarget[0];
	for(int i = 1; i < m_NumNodes; i++)
	{
		if((maxDistanceToTarget - m_NodeDistanceToTarget[i]) > PARTIAL_DESTINATION_PATH_LENGTH &&
			vPlayerCoors.Dist2(m_NodeCoordinates[i]) > PARTIAL_DESTINATION_PATH_DIST_FROM_PLAYER_SQR)
		{
			// use this node
			partialNodeIndex = i;
			break;
		}
	}
	
	if(partialNodeIndex != m_NumNodes)
	{
		m_PartialDestination = m_NodeCoordinates[partialNodeIndex];

		m_bUsePartialDestination = true;
		m_LastPartialRouteTime = fwTimer::GetTimeInMilliseconds();
	}
	// else we're too close for a partial route
#endif
}

s32	CGpsSlot::ConvertPathDirectionToInstruction(s32 PathFindDirection)
{
	// I prefer having one enum for all but I am pretty sure we will use these differently anyway later on
	// Might aswell keep them splitted
	s32 ConversionTable[CPathFind::DIRECTIONS_MAX] =
	{
		GPS_INSTRUCTION_UNKNOWN,		//DIRECTIONS_UNKNOWN = 0,
		GPS_INSTRUCTION_UTURN,			//DIRECTIONS_WRONG_WAY,
		GPS_INSTRUCTION_STRAIGHT_AHEAD,	//DIRECTIONS_KEEP_DRIVING,
		GPS_INSTRUCTION_TURN_LEFT,		//DIRECTIONS_LEFT_AT_JUNCTION,
		GPS_INSTRUCTION_TURN_RIGHT,		//DIRECTIONS_RIGHT_AT_JUNCTION,
		GPS_INSTRUCTION_STRAIGHT_AHEAD,	//DIRECTIONS_STRAIGHT_THROUGH_JUNCTION,		
		GPS_INSTRUCTION_KEEP_LEFT,		//DIRECTIONS_KEEP_LEFT,
		GPS_INSTRUCTION_KEEP_RIGHT,		//DIRECTIONS_KEEP_RIGHT,
		GPS_INSTRUCTION_UTURN,			//DIRECTIONS_UTURN,
	};
	
	Assert(PathFindDirection >= 0 && PathFindDirection < CPathFind::DIRECTIONS_MAX);
	return ConversionTable[PathFindDirection];
}

s32 CGpsSlot::CalcRouteInstruction(float* pfOutDistance)
{
	float fDist = 0.f;
	s32 iDir = GPS_INSTRUCTION_UNKNOWN;
	if (GetStatus() == STATUS_WORKING)
	{
		u32 iHash = 0;
		Vector3 vOutPos;
		ThePaths.GenerateDirections(iDir, iHash, fDist, m_NodeAdress, m_NumNodes, vOutPos);
		iDir = ConvertPathDirectionToInstruction(iDir);
	}

	if (pfOutDistance)
		*pfOutDistance = fDist;

	// Add more logic here for distance and stuff to cover the rest of the instructions?


	return iDir;
}

#if GPS_AUDIO

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ThinkOfSomethingToSay
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

char *CGpsSlot::ThinkOfSomethingToSay()
{
	static	char	sayString[128];

	sprintf(sayString, "-");

	// Don't say anything for 5sec after mission dialogue finishes
	if (g_ScriptAudioEntity.IsScriptedConversationOngoing())
	{
		m_DontSayAnythingBeforeTime = Max(m_DontSayAnythingBeforeTime, fwTimer::GetTimeInMilliseconds() + 5000);
	}

	if (m_DontSayAnythingBeforeTime > fwTimer::GetTimeInMilliseconds())
	{
		return sayString;
	}

	// Don't say anything new if we're not in a car
	CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (!playerVehicle || playerVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT || playerVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN ||
						  playerVehicle->GetIsRotaryAircraft())
	{
		if (playerVehicle && playerVehicle->GetVehicleType() != VEHICLE_TYPE_TRAIN)
		{
			CheckForReachingDestination();
		}

		return sayString;
	}

	Vector3	playerCoors = CGps::GetRelevantPlayerCoords();
//	playerCoors.z = 0.0f;		// Everything in 2d plane.
//	Vector2 playerCoors2;
//	playerCoors.GetVector2XY(playerCoors2);

	if (CheckForReachingDestination())  // yes we have arrived
	{
		sprintf(sayString, "You have arrived.");
		Announce(GPS_INSTRUCTION_YOU_HAVE_ARRIVED);
		return sayString;
	}

	if (GetStatus() == STATUS_CALCULATING_ROUTE)
	{
		sprintf(sayString, "Calculating route...");
		Announce(GPS_INSTRUCTION_CALCULATING_ROUTE);
		return sayString;
	}

	if (m_DistanceFromRoute >= g_ProceedToHighlightedDistance)
	{
		sprintf(sayString, "Please proceed to the highlighted route.");
		Announce(GPS_INSTRUCTION_HIGHLIGHTED_ROUTE);
		return sayString;
	}

	float	runningDist = 0.0f;
	// Go through the nodes detecting what we can say
	bool oldHighway=false;
	for (s32 i = 0; i < m_NumNodes - 1 ; i++)
	{
			// Tets whether this is a junction and we are going left or right.
		s32 numRoadsAtJunction = m_NodeInfo[i] & NODEINFO_LINKS_MASK;
		bool  highway = (m_NodeInfo[i] & NODEINFO_HIGHWAY_BIT) != 0;

		if (i > 0)
		{
			if (!oldHighway && highway)
			{
				sprintf(sayString, "Join the highway after %s", FindDistanceStringWithDistance(runningDist));
//				return sayString;
			}
			if (oldHighway && !highway)
			{
				sprintf(sayString, "Leave the highway after %s", FindDistanceStringWithDistance(runningDist));
//				return sayString;
			}

			if (numRoadsAtJunction > 2)
			{		
				// We have a junction. Work out whether we turn.
				// Have we moved onto a new junction, when we'd announced the last one, but never bong'd it?
				if (i>m_LastAnnouncedNode && m_LastAnnouncedNode!=m_LastBongedNode)
				{
					Announce(GPS_INSTRUCTION_BONG, -1.0f, m_LastAnnouncedNode);
				}
				Vector3 oldDir = Vector3(m_NodeCoordinates[i].x - m_NodeCoordinates[i-1].x, m_NodeCoordinates[i].y - m_NodeCoordinates[i-1].y, 0.0f);
				Vector3 newDir = Vector3(m_NodeCoordinates[i+1].x - m_NodeCoordinates[i].x, m_NodeCoordinates[i+1].y - m_NodeCoordinates[i].y, 0.0f);
				oldDir.Normalize();
				newDir.Normalize();
				float crossZ = oldDir.CrossZ(newDir);
				float dotProd = oldDir.Dot(newDir);

				Vector3	playerSpeed = CGameWorld::FindLocalPlayerSpeed();
				playerSpeed.z = 0.0f;		// Everything in 2d plane.
				Vector3	nextJunctionNode = Vector3(m_NodeCoordinates[i].x, m_NodeCoordinates[i].y, 0.0f);
				Vector3 desiredJunctionHeading = nextJunctionNode - playerCoors;
				desiredJunctionHeading.NormalizeSafe();
				Vector3	nextNode = Vector3(m_NodeCoordinates[1].x, m_NodeCoordinates[1].y, 0.0f);
				Vector3 desiredNextHeading = nextNode - playerCoors;
				desiredNextHeading.NormalizeSafe();
				f32 speed = playerSpeed.Dot(desiredJunctionHeading);
				f32 actualDistance = Max(runningDist - g_DistanceReduction, 0.0f);
				CVehicle* vehicle = CGameWorld::FindLocalPlayerVehicle();
				if (vehicle)
				{
					Vector3 vehicleDirection = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetB());
					// makes sure we're either facing the very next node, or facing the next junction node
					f32 nextDir = vehicleDirection.Dot(desiredNextHeading);
					f32 junctionDir = vehicleDirection.Dot(desiredJunctionHeading);
					if (nextDir<g_UTurnDotProduct && junctionDir<g_UTurnDotProduct)
					{
						sprintf(sayString, "U-Turn");
						Announce(GPS_INSTRUCTION_UTURN);
						return sayString;
					}
				}
				// How long will it take to get there
				u32 timeToNode = 100000;
				if (Abs(speed) > 0.1f)
				{
					timeToNode = (u32)(1000.0f * actualDistance / speed);
				}

				s32 instruction = INVALID_GPS_INSTRUCTION;
				if (crossZ < -0.5f || (crossZ < 0.0f && dotProd < 0.0f))
				{
					instruction = GPS_INSTRUCTION_TURN_RIGHT;
				}
				else if (crossZ > 0.5f || (crossZ > 0.0f && dotProd < 0.0f))
				{
					instruction = GPS_INSTRUCTION_TURN_LEFT;
				}
				if (instruction!=INVALID_GPS_INSTRUCTION)
				{
					s32 immediateTurn = INVALID_GPS_INSTRUCTION;
					for (s32 j = i+1; j < m_NumNodes - 1 ; j++)
					{
						if (numRoadsAtJunction > 2)
						{		
							Vector3 nextOldDir = Vector3(m_NodeCoordinates[j].x - m_NodeCoordinates[j-1].x, m_NodeCoordinates[j].y - m_NodeCoordinates[j-1].y, 0.0f);
							Vector3 nextNewDir = Vector3(m_NodeCoordinates[j+1].x - m_NodeCoordinates[j].x, m_NodeCoordinates[j+1].y - m_NodeCoordinates[j].y, 0.0f);
							nextOldDir.Normalize();
							nextNewDir.Normalize();
							float nextCrossZ = nextOldDir.CrossZ(nextNewDir);
							float nextDotProd = nextOldDir.Dot(nextNewDir);
							f32 distanceToNextNode = (m_NodeCoordinates[j+1] - m_NodeCoordinates[i]).Mag();
							if(distanceToNextNode < g_ImmediateTurnDistance)
							{
								if (nextCrossZ < -0.5f || (nextCrossZ < 0.0f && nextDotProd < 0.0f))
								{
									immediateTurn = GPS_INSTRUCTION_TURN_RIGHT;
									break;
								}
								else if (crossZ > 0.5f || (crossZ > 0.0f && dotProd < 0.0f))
								{
									immediateTurn = GPS_INSTRUCTION_TURN_LEFT;
									break;
								}
							}
							else
							{
								break;
							}
						}
					}

					Announce(instruction, actualDistance, i, timeToNode, speed, immediateTurn);
					if (immediateTurn==-1)
					{
						sprintf(sayString, "Turn x after %s", FindDistanceStringWithDistance(actualDistance));
						return sayString;
					}
					else
					{
						sprintf(sayString, "Turn x after %s, then immediately x", FindDistanceStringWithDistance(actualDistance));
						return sayString;
					}
				}
			}
		}
		Vector3 oldNodeCoors = m_NodeCoordinates[i];
		if (i == 0)
		{
			Vector3 playerCrs = CGps::GetRelevantPlayerCoords();
			oldNodeCoors = playerCrs; //Vector2(playerCrs.x, playerCrs.y);
		}
		runningDist += (oldNodeCoors - m_NodeCoordinates[i+1]).XYMag();
		oldHighway = highway;
	}
	return sayString;
}

bool CGpsSlot::Announce(s32 instruction, f32 distance, s32 nodeId, u32 timeToDestination, f32 speed, s32 immediatelyInstruction)
{
	if(m_ConsecutiveUTurnRequests!=-1)
	{
		if (instruction==GPS_INSTRUCTION_UTURN)
		{
			m_ConsecutiveUTurnRequests++;
		}
		else
		{
			m_ConsecutiveUTurnRequests = 0;
		}
	}
	if (g_FrontendAudioEntity.IsGPSPlaying())
	{
		// If we're playing a line featuring distance, and this update refers to the same node, replace that distance
		if (nodeId == m_LastAnnouncedNode && distance > 0.0f)
		{
			s32 distancePhrase = -1;
			FindDistanceStringWithDistance(distance, &distancePhrase);
			bool replacedDistance = g_FrontendAudioEntity.ReplaceGPSDistance(distancePhrase);
			if (replacedDistance)
			{
				return true;
			}
		}
		return false;
	}

	// If we're less than 10 yards from it, bong, unless we've already done so.
	f32 minTime = (f32)g_TimeTilBong;
	f32 maxTime = minTime * g_DrivingFastTimeScalingFactor;
	u32 timeTilBong = (u32)audCurveRepository::GetLinearInterpolatedValue(minTime, maxTime, 0.0f, 15.0f, speed);
	if (instruction == GPS_INSTRUCTION_BONG || (timeToDestination>0 && timeToDestination<=timeTilBong && m_LastBongedNode!=nodeId && m_LastAnnouncedNode==nodeId))
	{
		m_LastBongedNode = nodeId;
		SaySingleLine(AUD_GPS_BONG);
		return true;
	}

	u32 timeLastStoppedPlaying = g_FrontendAudioEntity.GetTimeLastStoppedPlayingGPS();
	u32 currentTime = fwTimer::GetTimeInMilliseconds();

	// If it's a U-turn or Calculating route, have we already played one
	bool playedSomething = false;
	switch(instruction)
	{
		case GPS_INSTRUCTION_CALCULATING_ROUTE:
			if (!m_AlreadyAnnouncedCalculatingRoute || m_LastAnnouncedTime + 10000 < fwTimer::GetTimeInMilliseconds())
			{
				SaySingleLine(AUD_GPS_CALCULATINROUTE);
				playedSomething = true;
				m_AlreadyAnnouncedCalculatingRoute = true;
				m_LastAnnouncedNode = -1;
				m_LastAnnouncedTime = fwTimer::GetTimeInMilliseconds();
				m_ConsecutiveUTurnRequests = 0;
			}
			break;
		case GPS_INSTRUCTION_HIGHLIGHTED_ROUTE:
			if (!m_AlreadyAnnouncedHighlighted || m_LastAnnouncedTime + 10000 < fwTimer::GetTimeInMilliseconds())
			{
				SaySingleLine(AUD_GPS_HIGHLIGHTEDROUTE);
				playedSomething = true;
				m_AlreadyAnnouncedHighlighted = true;
				m_LastAnnouncedNode = -1;
				m_LastAnnouncedTime = fwTimer::GetTimeInMilliseconds();
				m_ConsecutiveUTurnRequests = 0;
			}
			break;
		case GPS_INSTRUCTION_YOU_HAVE_ARRIVED:
			SaySingleLine(AUD_GPS_UHAVEARRIVED);
			playedSomething = true;
			m_LastAnnouncedTime = fwTimer::GetTimeInMilliseconds();
			break;
		case GPS_INSTRUCTION_UTURN:
			if (m_ConsecutiveUTurnRequests > 30)
			{
				SaySingleLine(AUD_GPS_UTURN);
				playedSomething = true;
				m_ConsecutiveUTurnRequests = -1;
				m_LastAnnouncedNode = -1;
				m_LastAnnouncedTime = fwTimer::GetTimeInMilliseconds();
			}
			break;
		case GPS_INSTRUCTION_TURN_LEFT:
			// Only play if we've time, always leave a pause, and if we're not in a hurry, leave a big one.
			if (timeToDestination>g_TimeToPlayJustTurn && 
				((timeLastStoppedPlaying + g_MinTimeBetweenAnnouncements) < currentTime) &&
				(timeToDestination<g_TimeToPlayTurn || ((timeLastStoppedPlaying + g_IdealTimeBetweenAnnouncements) < currentTime)))
			{
				// Are we processing the same node as we last announced
				if (nodeId != m_LastAnnouncedNode)
				{
					SayTurn(false, distance, timeToDestination, immediatelyInstruction);
					playedSomething = true;
					m_LastAnnouncedNode = nodeId;
					m_ConsecutiveUTurnRequests = 0;
					m_LastAnnouncedTime = fwTimer::GetTimeInMilliseconds();
				}
			}
			break;
		case GPS_INSTRUCTION_TURN_RIGHT:
			// Only play if we've time, always leave a pause, and if we're not in a hurry, leave a big one.
			if (timeToDestination>g_TimeToPlayJustTurn && 
				((timeLastStoppedPlaying + g_MinTimeBetweenAnnouncements) < currentTime) &&
				(timeToDestination<g_TimeToPlayTurn || ((timeLastStoppedPlaying + g_IdealTimeBetweenAnnouncements) < currentTime)))
			{
				// Are we processing the same node as we last announced
				if (nodeId != m_LastAnnouncedNode)
				{
					SayTurn(true, distance, timeToDestination, immediatelyInstruction);
					playedSomething = true;
					m_LastAnnouncedNode = nodeId;
					m_ConsecutiveUTurnRequests = 0;
					m_LastAnnouncedTime = fwTimer::GetTimeInMilliseconds();
				}
			}
			break;
	}
	return playedSomething;
}

void CGpsSlot::SayTurn(bool rightTurn, f32 runningDist, u32 timeToDestination, s32 immediatelyInstruction)
{
	Assert(!g_FrontendAudioEntity.IsGPSPlaying());

	audGPSPhrase GPSPhrase[AUD_MAX_GPS_PHRASES];

	s32 distancePhrase = -1;
	FindDistanceStringWithDistance(runningDist, &distancePhrase);
	if (g_SayDistanceFirst)
	{
		u32 phraseNum = 0;
		if (timeToDestination>g_TimeToPlayTurn)
		{
			GPSPhrase[phraseNum++].PhraseId = AUD_GPS_IN;
			GPSPhrase[phraseNum].PhraseId = distancePhrase;
			GPSPhrase[phraseNum++].IsDistance = true;
			if (distancePhrase!=AUD_GPS_1MILE && distancePhrase!=AUD_GPS_2MILES)
			{
				GPSPhrase[phraseNum++].PhraseId = AUD_GPS_YARDS;
			}
			GPSPhrase[phraseNum].Predelay = g_TurnPredelay;
		}
		GPSPhrase[phraseNum++].PhraseId = AUD_GPS_TURN;
		if (rightTurn)
		{
			GPSPhrase[phraseNum++].PhraseId = AUD_GPS_RIGHT;
		}
		else
		{
			GPSPhrase[phraseNum++].PhraseId = AUD_GPS_LEFT;
		}
		if (immediatelyInstruction!=-1)
		{
			GPSPhrase[phraseNum++].PhraseId = AUD_GPS_THENIMMEDIATELY;
			if (immediatelyInstruction==GPS_INSTRUCTION_TURN_LEFT)
			{
				GPSPhrase[phraseNum++].PhraseId = AUD_GPS_LEFT;
			}
			else
			{
				GPSPhrase[phraseNum++].PhraseId = AUD_GPS_RIGHT;
			}
		}
	}
	else
	{
		u32 phraseNum = 0;
		GPSPhrase[phraseNum++].PhraseId = AUD_GPS_TURN;
		if (rightTurn)
		{
			GPSPhrase[phraseNum++].PhraseId = AUD_GPS_RIGHT;
		}
		else
		{
			GPSPhrase[phraseNum++].PhraseId = AUD_GPS_LEFT;
		}
		if (immediatelyInstruction!=INVALID_GPS_INSTRUCTION)
		{
			GPSPhrase[phraseNum++].PhraseId = AUD_GPS_THENIMMEDIATELY;
			if (immediatelyInstruction==GPS_INSTRUCTION_TURN_LEFT)
			{
				GPSPhrase[phraseNum++].PhraseId = AUD_GPS_LEFT;
			}
			else
			{
				GPSPhrase[phraseNum++].PhraseId = AUD_GPS_RIGHT;
			}
		}
		if (timeToDestination>g_TimeToPlayTurn)
		{
			GPSPhrase[phraseNum++].PhraseId = AUD_GPS_IN;
			GPSPhrase[phraseNum].PhraseId = distancePhrase;
			GPSPhrase[phraseNum++].IsDistance = true;
			if (distancePhrase!=AUD_GPS_1MILE && distancePhrase!=AUD_GPS_2MILES)
			{
				GPSPhrase[phraseNum++].PhraseId = AUD_GPS_YARDS;
			}
		}
	}

	g_FrontendAudioEntity.StartNewGPS(&GPSPhrase[0], g_GPSVoice);
}

void CGpsSlot::SaySingleLine(s32 phraseId)
{
	Assert(!g_FrontendAudioEntity.IsGPSPlaying());
	audGPSPhrase GPSPhrase[AUD_MAX_GPS_PHRASES];
	GPSPhrase[0].PhraseId = phraseId;
	g_FrontendAudioEntity.StartNewGPS(&GPSPhrase[0], g_GPSVoice);
}
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindDistanceStringWithDistance
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

char *CGpsSlot::FindDistanceStringWithDistance(float distInMeters, s32 *audioID)
{
	static	char	returnString[64];
	s32 returnAudioID = 0;

	if (distInMeters < 15.0f)
	{
		sprintf(returnString, "10 yards");
		returnAudioID = AUD_GPS_10;
	}
	else if (distInMeters < 40.0f)
	{
		sprintf(returnString, "30 yards");
		returnAudioID = AUD_GPS_20;
	}
	else if (distInMeters < 70.0f)
	{
		sprintf(returnString, "50 yards");
		returnAudioID = AUD_GPS_50;
	}
	else if (distInMeters < 150.0f)
	{
		sprintf(returnString, "100 yards");
		returnAudioID = AUD_GPS_100;
	}
	else if (distInMeters < 300.0f)
	{
		sprintf(returnString, "200 yards");
		returnAudioID = AUD_GPS_200;
	}
	else if (distInMeters < 500.0f)
	{
		sprintf(returnString, "400 yards");
		returnAudioID = AUD_GPS_400;
	}
	else if (distInMeters < 1000.0f)
	{
		sprintf(returnString, "750 yards");
		returnAudioID = AUD_GPS_800;
	}
	else if (distInMeters < 2000.0f)
	{
		sprintf(returnString, "1 mile");
		returnAudioID = AUD_GPS_1MILE;
	}
	else
	{
		sprintf(returnString, "2 miles");
		returnAudioID = AUD_GPS_2MILES;
	}
	if (audioID)
	{
		*audioID = returnAudioID;
	}
	return returnString;
}

#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Render
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
void CGpsSlot::Render(s32 iRouteNum, float offsetX, float offsetY, bool bClipFirstLine, float clipX, float clipY)
{
	m_RedrawTimer = fwTimer::GetTimeInMilliseconds();

	CMiniMap_RenderThread::RenderNavPath(iRouteNum, m_NumNodes, m_NodeCoordinates, GetColor(), offsetX, offsetY, bClipFirstLine, clipX, clipY, CGps::ms_iRoadWidthMiniMap, CGps::ms_iRoadWidthPauseMap);
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RenderRaceTrack
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGpsSlot::RenderRaceTrack(s32 iRouteNum, float offsetX, float offsetY)
{
	CMiniMap_RenderThread::RenderNavPath(iRouteNum, m_NumNodes, m_NodeCoordinates, GetColor(), offsetX, offsetY, false, 0.0f, 0.0f, CGps::ms_iRoadWidthMiniMap, CGps::ms_iRoadWidthPauseMap);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RenderMultiGPS
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGpsSlot::RenderMultiGPS(s32 iRouteNum, float offsetX, float offsetY)
{
	CMiniMap_RenderThread::RenderNavPath(iRouteNum, m_NumNodes, m_NodeCoordinates, GetColor(), offsetX, offsetY, false, 0.0f, 0.0f, CGps::ms_iRoadWidthMiniMap, CGps::ms_iRoadWidthPauseMap);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RenderCustomRoute
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGpsSlot::RenderCustomRoute(s32 iRouteNum, float offsetX, float offsetY)
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();

	if (pPlayer)
	{
		CMiniMap_RenderThread::RenderNavPath(iRouteNum, m_NumNodes, m_NodeCoordinates, GetColor(), offsetX, offsetY, false, 0.0f, 0.0f, CGps::m_iCustomRouteMiniMapWidth, CGps::m_iCustomRoutePauseMapWidth);
	}
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitLevel
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGps::Init(unsigned initMode)
{	
	if(initMode == INIT_CORE)
	{
		CGps::ms_GpsNumNodesStored = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("CGpsNumNodesStored", 0x08f25956d), CONFIGURED_FROM_FILE);
		for (s32 i = 0; i < GPS_NUM_SLOTS; i++)
		{
			m_Slots[i].Init(i, initMode); // init storage
		}

		ms_relevantPlayerCoords.Set(Vector3(Vector3::ZeroType));
	}
	else if(initMode == INIT_SESSION)
	{
		for (s32 i = 0; i < GPS_NUM_SLOTS; i++)
			m_Slots[i].Init(i,initMode);

		for (int j = 0; j < CPathFind::GPS_ASYNC_SEARCH_NUM_SLOTS; ++j)
			m_bASyncStatusSearchSlot[j] = false;

		m_EnableAudio = 0;
		m_AudioCurrentlyDisabled = false;
		m_bIsASyncSearchBlockedThisFrame = false;
		//	    bFlashing = false;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Shutdown
// PURPOSE :  Free up dynamic memory in CGpsSlot
/////////////////////////////////////////////////////////////////////////////////

void CGps::Shutdown(unsigned shutdownMode)
{	
	if(shutdownMode == SHUTDOWN_CORE)
	{
		for (s32 i = 0; i < GPS_NUM_SLOTS; i++)
		{
			m_Slots[i].Shutdown(shutdownMode); // shutdown storage
		}
	}	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Process
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGps::Update()
{
	bool	bGpsSpeaks = false;
	CVehicle *pVeh = CGameWorld::FindLocalPlayerVehicle();

	ms_relevantPlayerCoords.GetWriteBuf() = GetRelevantPlayerCoords();

//	if (pVeh && ((pVeh->GetVehicleModelInfo()->GetFlag(FLAG_HAS_GPS) )
	// do gps if it's on in selected cars, and this is one, or if we're on in all cars
	// also disable audio in taxis, and for a few seconds after we're in a taxi, so it doesn't suddenly start
#if GPS_AUDIO
	bool isPlayerAPassengerInTaxi = false;
	if (audNorthAudioEngine::GetGtaEnvironment()->GetLastTimeWeWereAPassengerInATaxi()+5000 > fwTimer::GetTimeInMilliseconds())
	{
		isPlayerAPassengerInTaxi = true;
	}
	if (m_EnableAudio==2 && !m_AudioCurrentlyDisabled && !isPlayerAPassengerInTaxi && CPauseMenu::GetMenuPreference(PREF_DISPLAY_GPS)) 
	{
		bGpsSpeaks = true;
	}
	else if (m_EnableAudio==1 && pVeh && pVeh->GetVehicleAudioEntity()->GetGPSType()!=GPS_TYPE_NONE && !m_AudioCurrentlyDisabled &&
					 !isPlayerAPassengerInTaxi && CPauseMenu::GetMenuPreference(PREF_DISPLAY_GPS))
	{
		bGpsSpeaks = true;
	}
#endif

	if (pVeh)
	{
		g_GPSVoice = pVeh->GetVehicleAudioEntity()->GetGPSVoice();
	}


	// gps flashes for prologue
	if(m_bIsFlashingGPS)
	{
		u32 currentTime = fwTimer::GetTimeInMilliseconds();

		if(m_bFlashGPSTime <= currentTime)
		{
			m_bShowGPSWhileFlashing = !m_bShowGPSWhileFlashing;
			m_bFlashGPSTime = fwTimer::GetTimeInMilliseconds() + DEFAULT_GPS_FLASH_INTERVAL;
		}
	}

	// Reset every frame
	m_bIsASyncSearchBlockedThisFrame = false;
	for (int i = 0; i < CPathFind::GPS_ASYNC_SEARCH_NUM_SLOTS; ++i)
		m_bASyncStatusSearchSlot[i] = false;
	
	for (s32 i = 0; i < GPS_NUM_SLOTS; i++)
	{
		bool thisSlotSpeaks = bGpsSpeaks;
		if (m_Slots[i].GetStatus()==CGpsSlot::STATUS_WORKING_RACE_TRACK ||
			m_Slots[i].GetStatus()==CGpsSlot::STATUS_WORKING_CUSTOM_ROUTE)
		{
			thisSlotSpeaks = false;
		}

		if (i == GPS_SLOT_DISCRETE)
			thisSlotSpeaks = false;

		ThePaths.bActiveRequestForRegions[i] = false;

		m_Slots[i].Update(i, thisSlotSpeaks && !(i!=GPS_SLOT_RADAR_BLIP && m_Slots[GPS_SLOT_RADAR_BLIP].IsOn()));
	}

	// So we didn't utilize the search slot, throw away to stop lingering searches staying around
	for (int i = 0; i < CPathFind::GPS_ASYNC_SEARCH_NUM_SLOTS; ++i)
	{
		if (!m_bASyncStatusSearchSlot[i])
			ThePaths.ClearGPSSearchSlot(i);

	}
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ClearGpsOnMiniMap
// PURPOSE : clears the gps route for this slot from the minimap
/////////////////////////////////////////////////////////////////////////////////
void CGps::ClearGpsOnMiniMap(s32 iSlot)
{
	m_Slots[iSlot].m_bClearRender = true;
}

void CGps::SetGpsFlagsForScript(const scrThreadId iScriptId, const u32 iFlags, const float fBlipRouteDisplayDistance)
{
	uiDebugf3("SetGpsFlagsForScript - called by script %s", scrThread::GetActiveThread()->GetScriptName());

	if(m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags == iScriptId || m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags==0)
	{
		m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags = iScriptId;
		m_Slots[GPS_SLOT_RADAR_BLIP].m_iGpsFlags = (u16)iFlags;
		m_Slots[GPS_SLOT_RADAR_BLIP].m_fShowRouteProximity = fBlipRouteDisplayDistance;
	}
#if !__NO_OUTPUT
	else
	{
		char debugText[512] = {};

		scrThread* pScriptThread = GtaThread::GetThread(m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags);
		const char* pScriptName = pScriptThread ? pScriptThread->GetScriptName() : NULL;
		if (pScriptName)
			sprintf(debugText, "...First Set by script name: %s...", pScriptName);
		else
			sprintf(debugText, "...Couldn't get Set by script name...");

		scrThread* pThisScriptThread = GtaThread::GetThread(iScriptId);
		const char* pThisScriptName = pThisScriptThread ? pThisScriptThread->GetScriptName() : NULL;
		if (pThisScriptName)
			sprintf(debugText, "%s This is script name: %s...", debugText, pThisScriptName);
		else
			sprintf(debugText, "%s Couldn't get this script name...", debugText);
		
		if ( ! uiVerifyf(m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags == iScriptId || m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags==0, "Script %u has already set GPS flags, this is script %u.  Only one script can do this at a time. Use ui_log=debugf3 for more log info. More info: %s", m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags, iScriptId, debugText) )
		{
			uiDebugf3("Script %u has already set GPS flags, this is script %u.  Only one script can do this at a time. More info: %s", m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags, iScriptId, debugText);
		}
	}
#endif

}

void CGps::ClearGpsFlagsOnScriptExit(const scrThreadId iScriptId)
{
#if !__NO_OUTPUT
	if(scrThread::GetActiveThread())
	{
		uiDebugf3("ClearGpsFlagsOnScriptExit - called by script %s", scrThread::GetActiveThread()->GetScriptName());
	}
	else
	{
		uiDebugf3("ClearGpsFlagsOnScriptExit - called when no script running (probably during restart?)");
	}
#endif 

	if(m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags == iScriptId)
	{
		m_Slots[GPS_SLOT_RADAR_BLIP].m_iScriptIdSetGpsFlags = THREAD_INVALID;
		m_Slots[GPS_SLOT_RADAR_BLIP].m_iGpsFlags = 0;
		m_Slots[GPS_SLOT_RADAR_BLIP].m_fShowRouteProximity = 0.0f;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ForceUpdateOfGpsOnMiniMap
// PURPOSE : forces the gps to be redrawn onto the minimap this frame
/////////////////////////////////////////////////////////////////////////////////
void CGps::ForceUpdateOfGpsOnMiniMap()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CGps: ForceUpdateOfGpsOnMiniMap can only be called on the RenderThread!");
		return;
	}

	for (s32 iCount = 0; iCount < GPS_NUM_SLOTS; iCount++)
	{
		if (ShouldGpsBeVisible(iCount))
		{
			m_Slots[iCount].SetCurrentRenderStatus(false);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateGpsOnMiniMapOnRT
// PURPOSE : draws the gps route onto the minimap
/////////////////////////////////////////////////////////////////////////////////
void CGps::UpdateGpsOnMiniMapOnRT()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CGps: UpdateGpsOnMiniMapOnRT can only be called on the RenderThread!");
		return;
	}

	Vector2 offSet(0.0f, 0.0f);
	Vector2 offSetIncrement(4.0f, 4.0f);
	bool	bRenderThisSlot[GPS_NUM_SLOTS] = { false, false, false };

	// decide if we need to render this frame:
	for (s32 iCount = 0; iCount < GPS_NUM_SLOTS; iCount++)
	{
		if (ShouldGpsBeVisible(iCount))
		{
			switch(m_Slots[iCount].GetStatus())
			{
				case CGpsSlot::STATUS_OFF:
				{
					bRenderThisSlot[iCount] = false;

					break;
				}
				case CGpsSlot::STATUS_WORKING:
				case CGpsSlot::STATUS_CALCULATING_ROUTE:
				case CGpsSlot::STATUS_CALCULATING_ROUTE_QUIET:
				{
					if (m_Slots[iCount].m_Identifier != INVALID_BLIP_ID)
					{
						CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_Slots[iCount].m_Identifier);
						if (pBlip)// we even display route on BLIP_DISPLAY_NEITHER as we create an invisible waypoint on other blips on the pausemap // && CMiniMap::GetBlipDisplayValue(pBlip) != BLIP_DISPLAY_NEITHER)
						{
							if ( (!CMiniMap::IsInPauseMap()) || (CMapMenu::ShouldBlipBeShownOnLegend(pBlip)) || CScriptHud::bForceShowGPS )
							{
								bRenderThisSlot[iCount] = true;
							}
						}
					}

					break;
				}
				case CGpsSlot::STATUS_CALCULATING_ROUTE_RACE_TRACK:
				case CGpsSlot::STATUS_WORKING_RACE_TRACK:
				{
					bRenderThisSlot[iCount] = true;

					break;
				}
				// Custom route & multi-route are always visible
				case CGpsSlot::STATUS_WORKING_ROUTE_MULTI:
				case CGpsSlot::STATUS_CALCULATING_ROUTE_MULTI:
				case CGpsSlot::STATUS_WORKING_CUSTOM_ROUTE:
				{
					bRenderThisSlot[iCount] = true;

					break;
				}
				default:
				{
					Assertf(false, "GPS slot %i, status %i, not handled", iCount, m_Slots[iCount].GetStatus());
					break;
				}
			}

			if(iCount == GPS_SLOT_RADAR_BLIP &&  m_Slots[iCount].m_fShowRouteProximity > 0.0f && m_Slots[iCount].GetTargetEntity())
			{
				CPed * pPlayer = CGameWorld::FindLocalPlayer();
				if(pPlayer)
				{
					const Vector3 vToEntity = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()) -
						VEC3V_TO_VECTOR3(m_Slots[iCount].GetTargetEntity()->GetTransform().GetPosition());

					// If within the minimum radius for displaying the route, then switch off rendering if it is enabled..
					if(bRenderThisSlot[iCount] && vToEntity.Mag2() < m_Slots[iCount].m_fShowRouteProximity*m_Slots[iCount].m_fShowRouteProximity)
						bRenderThisSlot[iCount] = false;
				}
			}
		}
	}

	//
	// render or clear the slots if needed:
	//
	for (s32 iCount = GPS_NUM_SLOTS-1; 0 <= iCount; --iCount)
	{
		//
		// slot not rendered this frame so switch it off:
		//
		if (m_Slots[iCount].m_bClearRender || !bRenderThisSlot[iCount])
		{
			if (m_Slots[iCount].m_bClearRender || m_Slots[iCount].GetCurrentRenderStatus() != bRenderThisSlot[iCount] || m_Slots[iCount].IsOn())
			{
				CMiniMap_RenderThread::ClearNavPath(iCount);
				m_Slots[iCount].m_bClearRender = false;
				m_Slots[iCount].SetCurrentRenderStatus(false);
			}
		}
		//
		// slot is to be rendered this frame so switch it on:
		//
		else
		{
			bool shouldRender = false;
			if(CPauseMenu::IsActive())
			{
				shouldRender = m_Slots[iCount].GetCurrentRenderStatus() != bRenderThisSlot[iCount];
			}
			else
			{
				shouldRender = m_Slots[iCount].GetCurrentRenderStatus() == bRenderThisSlot[iCount] &&
					!m_Slots[iCount].GetRenderedLastFrame();
			}

			if(CScriptHud::bForceShowGPS)
			{
				shouldRender = true;
			}

			if (shouldRender)
			{
				switch( m_Slots[iCount].GetStatus() )
				{
					case CGpsSlot::STATUS_WORKING_RACE_TRACK:
					{
						m_Slots[iCount].RenderRaceTrack(iCount, offSet.x, offSet.y);
						break;
					}
					case CGpsSlot::STATUS_WORKING_CUSTOM_ROUTE:
					{
						m_Slots[iCount].RenderCustomRoute(iCount, offSet.x, offSet.y);
						break;
					}
					default:
					{
						// at the moment as this isnt updated every frame (only when the route is updated),
						// we do not need to clip the 1st line.  This may have to change
						Vector3	vSourcePoint(0,0,0);
						bool bClipFirstLine = false;

						const CPed * pLocalPlayer = CGameWorld::FindLocalPlayer();
						if (pLocalPlayer)
						{
							vSourcePoint = ms_relevantPlayerCoords.GetReadBuf();
							bClipFirstLine = true;
						}

						m_Slots[iCount].Render(iCount, offSet.x, offSet.y, bClipFirstLine, vSourcePoint.x, vSourcePoint.y);
						break;
					}
				}

				m_Slots[iCount].SetRenderedLastFrame();
			}

			m_Slots[iCount].SetCurrentRenderStatus(bRenderThisSlot[iCount]);

			offSet.x += offSetIncrement.x;
			offSet.y += offSetIncrement.y;
		}
	}
}


void CGps::SetGpsFlashes(bool bFlash)
{
	m_bIsFlashingGPS = bFlash;

	if(m_bIsFlashingGPS)
	{
		m_bFlashGPSTime = fwTimer::GetTimeInMilliseconds() + DEFAULT_GPS_FLASH_INTERVAL;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ShouldGpsBeVisible
// PURPOSE : returns whether the gps routes should be displayed on the minimap
//			 this function now takes the slot number as different slots now have
//			 different visiblility settings
/////////////////////////////////////////////////////////////////////////////////
bool CGps::ShouldGpsBeVisible(s32 iSlot, bool checkMiniMap /*= true*/)
{
	if (iSlot == GPS_SLOT_DISCRETE)
		return false;

	if (m_Slots[iSlot].m_bClearRender)  // flagged to be cleared so return as should not be visible
	{
		return false;
	}
	else if(!m_Slots[iSlot].IsOn())
	{
		return false;
	}

	if(m_bIsFlashingGPS && !m_bShowGPSWhileFlashing)
	{
		return false;
	}

	if (!CPauseMenu::GetMenuPreference(PREF_DISPLAY_GPS))  // gps switched off in pause menu
	{
		return false;
	}

	//if(CMiniMap::IsInCustomMap())
	//{
	//	return false;
	//}

	if (!CPortal::IsGPSAvailable())
	{
		return false;
	}

	CPed *pPlayer = CGameWorld::FindLocalPlayer();
	CVehicle *pVeh = CGameWorld::FindLocalPlayerVehicle();

	bool isInterior = (CSystem::IsThisThreadId(SYS_THREAD_RENDER) ? CMiniMap_RenderThread::GetIsInsideInterior() : CMiniMap::GetIsInsideInterior());

	if (CMiniMap::IsInCustomMap())
	{
		return true;
	}

	if(CScriptHud::bForceShowGPS)
	{
		return true;
	}

	if (CMiniMap::IsInPauseMap() )  // all GPS should show whilst in frontend map (unless in aircraft/boat)
	{
		if (isInterior)
		{
			return false;
		}

		if(CMiniMap::IsInGolfMap())
		{
			return false;
		}

		if(checkMiniMap && !CMiniMap_RenderThread::CanDisplayGpsLine(true))
		{
			return false;
		}

		if (pVeh)
		{
			bool bInsideAmphibiousVehInWater = false;
			if (pVeh && pVeh->InheritsFromAmphibiousAutomobile())
			{
				// Check out of water timer to stop this flicking on and off when jumping across waves
				CAmphibiousAutomobile* pAmphiVeh = static_cast<CAmphibiousAutomobile*>(pVeh);
				bInsideAmphibiousVehInWater = pAmphiVeh->m_Buoyancy.GetStatus() != NOT_IN_WATER || pAmphiVeh->GetBoatHandling()->GetOutOfWaterTime() < 2.0f;
			}

			if (pVeh->GetIsAircraft() || pVeh->GetIsAquaticMode() || bInsideAmphibiousVehInWater)
			{
				if(m_Slots[iSlot].GetStatus() == CGpsSlot::STATUS_WORKING_CUSTOM_ROUTE || m_Slots[iSlot].GetStatus() == CGpsSlot::STATUS_WORKING_ROUTE_MULTI)
				{
					return true;  // only want to show GPS on a custom route when in these vehicle types
				}
				else
				{
					return false;
				}
			}
		}

		return true;
	}
	else
	{
		if (isInterior)
		{
			return false;  // do not show any GPS when we are on the minimap inside an interior
		}
	}

	if(pPlayer && pPlayer->GetIsOnFoot())
	{
		return ((m_Slots[iSlot].m_uCustomRouteFlags & CGpsSlot::GPS_ROUTE_FLAG_ON_FOOT)!=0) ?  true : false;
	}

	if (pPlayer && pVeh == NULL && !pPlayer->GetIsOnMount())  // no GPS when inside an interior as the minimap zooms in really far so any lines are too large
	{
		if (checkMiniMap && !CMiniMap_RenderThread::CanDisplayGpsLine(true))
		{
			return false;
		}
	}
	
	if (pVeh || CGameWorld::FindLocalPlayerMount() || (pPlayer && pPlayer->GetIsOnMount()))
	{
		bool bInsideAmphibiousVehInWater = false;
		if (pVeh && pVeh->InheritsFromAmphibiousAutomobile())
		{
			// Check out of water timer to stop this flicking on and off when jumping across waves
			CAmphibiousAutomobile* pAmphiVeh = static_cast<CAmphibiousAutomobile*>(pVeh);
			bInsideAmphibiousVehInWater = pAmphiVeh->m_Buoyancy.GetStatus() != NOT_IN_WATER || pAmphiVeh->GetBoatHandling()->GetOutOfWaterTime() < 2.0f;
		}

		if (pVeh && (pVeh->GetIsAircraft() || pVeh->GetIsAquaticMode() || bInsideAmphibiousVehInWater))
		{
			if(m_Slots[iSlot].GetStatus() == CGpsSlot::STATUS_WORKING_CUSTOM_ROUTE || m_Slots[iSlot].GetStatus() == CGpsSlot::STATUS_WORKING_ROUTE_MULTI)
			{
				return true;  // only want to show GPS on a custom route when in these vehicle types
			}
		}
		else
		{
			if(!checkMiniMap)
			{
				return true;
			}
			else if (CMiniMap_RenderThread::CanDisplayGpsLine(false))
			{
				// Only show the GPS line, if the player is far enough away from the target.
				CEntity* pTarget = m_Slots[iSlot].GetTargetEntity();
				float distance2 = fHideGPSUnderDistance2;

				if(pTarget)
				{
					// This function can be called on either UT or RT (for better or for worse)
					Vector3	playerCoors = sysThreadType::IsUpdateThread() ? ms_relevantPlayerCoords.GetUpdateBuf() : ms_relevantPlayerCoords.GetRenderBuf();
					distance2 = playerCoors.Dist2(pTarget->GetPreviousPosition());
				}

				if((m_Slots[iSlot].m_iGpsFlags & GPS_FLAG_CUSTOM_PROXIMITY) || distance2 >= fHideGPSUnderDistance2)
				{
					return true;  // in a vehicle and we want to show GPS
				}
			}
		}
	}

	return false;  // not in a vehicle
}

CPed* CGps::GetRelevantPlayer()
{
	CPed* pPed = FindPlayerPed();

	// Network object interface isn't RT safe!
	if(CNetwork::IsGameInProgress() && NetworkInterface::IsInSpectatorMode())
	{
		ObjectId objid = NetworkInterface::GetSpectatorObjectId();
		netObject* obj = NetworkInterface::GetNetworkObject(objid);
		if (obj && (gnetVerify(NET_OBJ_TYPE_PLAYER == obj->GetObjectType() || NET_OBJ_TYPE_PED == obj->GetObjectType())))
		{
			CPed* pSpectatedPed = static_cast<CPed*>(static_cast<CNetObjPed*>(obj)->GetPed());
			if (gnetVerify(pSpectatedPed))
			{
				pPed = pSpectatedPed;
			}
		}
	}

	return pPed;
}

CVehicle* CGps::GetRelevantPlayerVehicle()
{
	CPed* pPlayer = GetRelevantPlayer();
	CVehicle* pVehicle = NULL;

	if(pPlayer && pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
	{
		pVehicle = pPlayer->GetMyVehicle();
	}

	return pVehicle;
}

Vector3 CGps::GetRelevantPlayerCoords()
{
	CPed* pPlayer = GetRelevantPlayer();

	if(pPlayer)
	{
        Vector3 fakePlayerPosition;
        if( CScriptHud::GetFakedGPSPlayerPos( fakePlayerPosition ) )
        {
            return fakePlayerPosition;
        }
        else
        {
            return VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
        }
	}

	return Vector3();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StartRadarBlipGps
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGps::StartRadarBlipGps(Vector3 &newDestination, CEntity *pEntity, s32 identifier, Color32 colour)
{
	m_Slots[GPS_SLOT_RADAR_BLIP].Start(newDestination, pEntity, identifier, CMiniMap::IsInPauseMap() || CMiniMap::IsInCustomMap(), GPS_SLOT_RADAR_BLIP, colour);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StopRadarBlipGps
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
void CGps::StopRadarBlipGps(s32 identifier)
{
	if (m_Slots[GPS_SLOT_RADAR_BLIP].IsOn() &&
		!m_Slots[GPS_SLOT_RADAR_BLIP].IsRaceTrackRoute() &&
		m_Slots[GPS_SLOT_RADAR_BLIP].m_Identifier == identifier)
	{
		CGps::ClearGpsOnMiniMap(GPS_SLOT_RADAR_BLIP);

		m_Slots[GPS_SLOT_RADAR_BLIP].SetStatus(CGpsSlot::STATUS_OFF);
	}
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetRouteColour
// PURPOSE : returns the colour of the route
/////////////////////////////////////////////////////////////////////////////////
Color32 CGps::GetRouteColour(s32 iSlot)
{
	if (iSlot < GPS_NUM_SLOTS)
	{
		return m_Slots[iSlot].GetColor();
	}
	else
	{
		Assertf(0, "GPS Slot %d invalid", iSlot);
		return Color32(0,0,0,0);
	}
}

s32	CGps::CalcRouteInstruction(s32 iSlot, float* pfOutDistance)
{
	if(aiVerify(iSlot >= 0 && iSlot < GPS_NUM_SLOTS))
	{
		return m_Slots[iSlot].CalcRouteInstruction(pfOutDistance);
	}

	return INVALID_GPS_INSTRUCTION;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetRouteLength
// PURPOSE : returns the distance to the target
/////////////////////////////////////////////////////////////////////////////////
s16 CGps::GetRouteLength(s32 iSlot)
{
	if (iSlot < GPS_NUM_SLOTS)
	{
		return m_Slots[iSlot].GetDistanceToTarget();
	}
	else
	{
		Assertf(0, "GPS Slot %d invalid", iSlot);
		return 0;
	}
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetRouteFound
// PURPOSE : does a route exist and working for the slot passed
/////////////////////////////////////////////////////////////////////////////////
bool CGps::GetRouteFound(s32 iSlot)
{
	if (iSlot < GPS_NUM_SLOTS)
	{
		return (m_Slots[iSlot].IsOn());
	}
	else
	{
		Assertf(0, "GPS Slot %d invalid", iSlot);
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ChangeRadarBlipGpsColour
// PURPOSE : level guys seem to change the friendlyness of blips & the colour of
//			 blips once they have started a route, so the radar code ensures that
//			 the gps colour is re-set if they do this
/////////////////////////////////////////////////////////////////////////////////
void CGps::ChangeRadarBlipGpsColour(s32 identifier, Color32 colour)
{
	if (m_Slots[GPS_SLOT_RADAR_BLIP].m_Identifier == identifier)
	{
		m_Slots[GPS_SLOT_RADAR_BLIP].SetColor(colour);
	}

}

float CGps::CalcRequiredMarginX(const Vector3& vStart, const Vector3& vDestination)
{
	const float fCityBoundMinX = -2000.f;
	const float fCityBoundMinY = -3500.f;
	const float fCityBoundMaxX = 1250.f;
	const float fCityBoundMaxY = 100.f;	

	if (vStart.x > fCityBoundMinX && vStart.x < fCityBoundMaxX &&
		vStart.y > fCityBoundMinY && vStart.y < fCityBoundMaxY &&
		vDestination.x > fCityBoundMinX && vDestination.x < fCityBoundMaxX &&
		vDestination.y > fCityBoundMinY && vDestination.y < fCityBoundMaxY)
	{
		return 1000.f;	// In the city we settle for less regions
	}

	float fMargin = abs(vDestination.y - vStart.y) * 0.75f;
	return Clamp(fMargin, ms_fNodeRequestMarginMin, ms_fNodeRequestMarginMax);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StartRaceTrack
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGps::StartRaceTrack(s32 UNUSED_PARAM(colour))
{
	m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].Clear(GPS_SLOT_USED_FOR_WAYPOINTS_GPS, true);

	m_iNumCustomAddedPoints = 0;
	m_Slots[GPS_SLOT_USED_FOR_RACE_TRACK].SetColor(CHudColour::GetRGBA(HUD_COLOUR_OBJECTIVE_ROUTE));
	m_Slots[GPS_SLOT_USED_FOR_RACE_TRACK].SetStatus(CGpsSlot::STATUS_OFF);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SetPointOfRaceTrack
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGps::AddPointToRaceTrack(Vector3 &pnt)
{
	if(aiVerifyf(m_iNumCustomAddedPoints < MAX_CUSTOM_ADDED_POINTS, "Too many points in this race track. Call START_RACE_TRACK or talk to Obbe"))
	{
		m_aCustomAddedPoints[m_iNumCustomAddedPoints++] = pnt;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RenderRaceTrack
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CGps::RenderRaceTrack(bool bOn)
{
	if (!bOn)
	{	
		// If the racetrack is being rendered we switch it off here.
		if ( m_Slots[GPS_SLOT_USED_FOR_RACE_TRACK].IsRaceTrackRoute() )
		{
			m_Slots[GPS_SLOT_USED_FOR_RACE_TRACK].Clear(GPS_SLOT_USED_FOR_RACE_TRACK, true);
		}
	}
	else
	{
		Assertf( !m_Slots[GPS_SLOT_USED_FOR_RACE_TRACK].IsRaceTrackRoute(), "Render race track called when there alreary is one being rendered");

		m_Slots[GPS_SLOT_USED_FOR_RACE_TRACK].SetStatus(CGpsSlot::STATUS_CALCULATING_ROUTE_RACE_TRACK);
	}
}


// NAME : StartWaypointsGPS
// PURPOSE :

void CGps::StartMultiGPS(s32 colour, bool bTrackPlayer, bool bOnFoot)
{
	m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].Clear(GPS_SLOT_USED_FOR_WAYPOINTS_GPS, true);

	m_iNumCustomAddedPoints = 0;
	for (int i = 0; i < MAX_CUSTOM_ADDED_POINTS; ++i)
		m_aCustomAddedNodes[i].SetEmpty();

	m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].SetColor(CHudColour::GetRGBA((eHUD_COLOURS)colour));
	m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].SetStatus(CGpsSlot::STATUS_OFF);
	m_bMultiGpsTracksPlayer = bTrackPlayer;

	m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].m_uCustomRouteFlags = CGpsSlot::GPS_ROUTE_FLAG_NONE;
	if(bOnFoot)
	{
		m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].m_uCustomRouteFlags |= CGpsSlot::GPS_ROUTE_FLAG_ON_FOOT;
	}
}

// NAME : AddPointToWaypointsGPS
// PURPOSE :

void CGps::AddPointToMultiGPS(const Vector3 &pnt)
{

	if(aiVerifyf(m_iNumCustomAddedPoints<MAX_CUSTOM_ADDED_POINTS, "Too many points in this Multi-GPS route. Call START_WAYPOINTS_GPS or talk to James"))
	{
		m_aCustomAddedPoints[m_iNumCustomAddedPoints++] = pnt;
	}
}

// NAME : RenderWaypointsGPS
// PURPOSE :

void CGps::RenderMultiGPS(bool bOn)
{
	if (!bOn)
	{
		// If the waypoints GPS is being rendered we switch it off here.
		if ( m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].IsMultiRoute() )
		{
			m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].Clear(GPS_SLOT_USED_FOR_RACE_TRACK, true);
		}
	}
	else
	{
		Assertf( !m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].IsMultiRoute(), "Render multi GPS called when there already is one being rendered");

		m_Slots[GPS_SLOT_USED_FOR_WAYPOINTS_GPS].SetStatus(CGpsSlot::STATUS_CALCULATING_ROUTE_MULTI);
	}
}

// NAME : StartCustomRoute
// PURPOSE : Start a custom GPS route whose route points are specified by the designer
void CGps::StartCustomRoute(s32 colour, bool bOnFoot, bool bInVehicle)
{
	m_iNumCustomAddedPoints = 0;

	AssertMsg(bInVehicle||bOnFoot, "Custom route must be set as 'bInVehicle', 'bOnFoot' or BOTH!  If neither is set then there's nothing for me to do! :)");
	if(!(bInVehicle||bOnFoot))
		return;
	
	Assert(colour >= 0 && colour < HUD_COLOUR_MAX_COLOURS);

	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].SetColor(CHudColour::GetRGBA((eHUD_COLOURS)colour));
	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].SetStatus(CGpsSlot::STATUS_OFF);

	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags = CGpsSlot::GPS_ROUTE_FLAG_NONE;
	if(bOnFoot) m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags |= CGpsSlot::GPS_ROUTE_FLAG_ON_FOOT;
	if(bInVehicle) m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags |= CGpsSlot::GPS_ROUTE_FLAG_IN_VEHICLE;
}

// NAME : AddPointToCustomRoute
// PURPOSE : Extend the route by one element
void CGps::AddPointToCustomRoute(Vector3 &pnt)
{
	if(aiVerifyf(m_iNumCustomAddedPoints<MAX_CUSTOM_ADDED_POINTS, "Too many points in this race track. Call START_CUSTOM_GPS_ROUTE or talk to James"))
	{
		m_aCustomAddedPoints[m_iNumCustomAddedPoints++] = pnt;
	}
}

// NAME : InitCustomRouteFromWaypointRecordingRoute
// PURPOSE : Initialise the GPS route from the specified waypoint recording,
// with the ability to set up from a partial section of the given waypoint route.
// NOTES: The waypoint recording must already be loaded
void CGps::InitCustomRouteFromWaypointRecordingRoute(const char * pName, int iStartIndex, int iNumPts, s32 colour, bool bOnFoot, bool bInVehicle)
{
	m_iNumCustomAddedPoints = 0;

	Assert(colour >= 0 && colour < HUD_COLOUR_MAX_COLOURS);

	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].SetColor(CHudColour::GetRGBA((eHUD_COLOURS)colour));
	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].SetStatus(CGpsSlot::STATUS_OFF);

	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags = CGpsSlot::GPS_ROUTE_FLAG_NONE;
	if(bOnFoot) m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags |= CGpsSlot::GPS_ROUTE_FLAG_ON_FOOT;
	if(bInVehicle) m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags |= CGpsSlot::GPS_ROUTE_FLAG_IN_VEHICLE;


	CWaypointRecordingRoute * pRoute = CWaypointRecording::GetRecording(pName);
	Assertf(pRoute, "Waypoint recording \"%s\" isn't loaded.", pName);
	if(!pRoute)
		return;

	iStartIndex = Clamp(iStartIndex, 0, pRoute->GetSize()-1);
	if(iStartIndex < 0)
		return;

	if(iNumPts == -1)
		iNumPts = pRoute->GetSize();

	Assertf(iNumPts < MAX_CUSTOM_ADDED_POINTS, "Num points %i is greater than max of %i currently allowed for custom GPS routes. (GPS route will be truncated)", iNumPts, MAX_CUSTOM_ADDED_POINTS);
	iNumPts = Min(iNumPts, MAX_CUSTOM_ADDED_POINTS);

	if(iNumPts <= 0)
		return;

	while(iNumPts-- && iStartIndex < pRoute->GetSize())
	{
		Vector3 vPos = pRoute->GetWaypoint(iStartIndex).GetPosition();
		AddPointToCustomRoute(vPos);
		iStartIndex++;
	}
}

// NAME : InitCustomRouteFromAssistedMovementRoute
// PURPOSE : Initialise the GPS route from the specified assisted movement route,
// with the ability to set up from a partial section of the given route.
// NOTES: The assisted movement route must already be loaded
void CGps::InitCustomRouteFromAssistedMovementRoute(const char * pName, int iDirection, int iStartIndex, int iNumPts, s32 colour, bool bOnFoot, bool bInVehicle)
{
	m_iNumCustomAddedPoints = 0;

	Assert(colour >= 0 && colour < HUD_COLOUR_MAX_COLOURS);

	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].SetColor(CHudColour::GetRGBA((eHUD_COLOURS)colour));
	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].SetStatus(CGpsSlot::STATUS_OFF);

	m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags = CGpsSlot::GPS_ROUTE_FLAG_NONE;
	if(bOnFoot) m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags |= CGpsSlot::GPS_ROUTE_FLAG_ON_FOOT;
	if(bInVehicle) m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_uCustomRouteFlags |= CGpsSlot::GPS_ROUTE_FLAG_IN_VEHICLE;


	Assert(pName);
	if(!pName)
		return;

	AssertMsg(iDirection==-1 || iDirection==1, "Direction must be +1 or -1.  Assisted routes work in both ways, so you need to tell me which end you want the GPS to treat as the destination!");
	if(iDirection!=-1 && iDirection!=1)
		return;

	u32 iNameHash = atStringHash(pName);
	CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRouteByNameHash(iNameHash);
	Assertf(pRoute, "Assisted movment route \"%s\" doesn't exist, or is not loaded.", pName);
	if(!pRoute)
		return;

	Assertf(pRoute->GetRouteType()!=CAssistedMovementRoute::ROUTETYPE_WAYPOINTRECORDING, "No!! Don't initialise the GPS from an assisted-movement route which is *itself* being updated from a waypoint-recording.. Please just initialise the GPS route directly from the original waypoint recording! (route name was : \"%s\")", pName);
	if(pRoute->GetRouteType()==CAssistedMovementRoute::ROUTETYPE_WAYPOINTRECORDING)
		return;

	iStartIndex = Clamp(iStartIndex, 0, pRoute->GetSize()-1);
	if(iStartIndex < 0)
		return;

	if(iNumPts == -1)
		iNumPts = pRoute->GetSize();

	Assertf(iNumPts < MAX_CUSTOM_ADDED_POINTS, "Num points %i is greater than max of %i currently allowed for custom GPS routes. (GPS route will be truncated)", iNumPts, MAX_CUSTOM_ADDED_POINTS);
	iNumPts = Min(iNumPts, MAX_CUSTOM_ADDED_POINTS);

	if(iNumPts <= 0)
		return;

	while(iNumPts-- && iStartIndex < pRoute->GetSize() && iStartIndex >= 0)
	{
		Vector3 vPos = pRoute->GetPoint(iStartIndex);
		AddPointToCustomRoute(vPos);
		iStartIndex += iDirection;
	}
}
void CGps::RenderCustomRoute(bool bOn, int iMiniMapWidth, int iPauseMapWidth)
{
	if(iMiniMapWidth != -1)
		m_iCustomRouteMiniMapWidth = static_cast<float>(iMiniMapWidth);
	else
		m_iCustomRouteMiniMapWidth = CGps::ms_iRoadWidthMiniMap;

	if(iPauseMapWidth != -1)
		m_iCustomRoutePauseMapWidth = static_cast<float>(iPauseMapWidth);
	else
		m_iCustomRoutePauseMapWidth = CGps::ms_iRoadWidthPauseMap;

	if (!bOn)
	{	
		// If the custom route is being rendered we switch it off here.
		if ( m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].GetStatus() == CGpsSlot::STATUS_WORKING_CUSTOM_ROUTE)
		{
			m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].Clear(GPS_SLOT_USED_FOR_CUSTOM_ROUTE, true);
		}
	}
	else
	{
		AssertMsg( m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].GetStatus() != CGpsSlot::STATUS_WORKING_CUSTOM_ROUTE, "Render custom route called when there already is one being rendered");
		AssertMsg( m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].GetStatus() != CGpsSlot::STATUS_WORKING_RACE_TRACK, "Render custom route called when there already a racetrack route being rendered");

		m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].SetStatus(CGpsSlot::STATUS_WORKING_CUSTOM_ROUTE);

		
		// Set up the nodes
		int i=0;
		while(i < m_iNumCustomAddedPoints)
		{
			m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_NodeCoordinates[i] = m_aCustomAddedPoints[i];
			i++;
		}
		m_Slots[GPS_SLOT_USED_FOR_CUSTOM_ROUTE].m_NumNodes = i;
	}
}








#if __BANK

void SetUpBlipFocusEntityGPS()
{
	CEntity * pEntity = CDebugScene::FocusEntities_Get(0);
	if(pEntity)
	{
		Vector3 vec = VEC3_ZERO;
		CGps::StartRadarBlipGps(vec, pEntity, 0, Color_magenta);
	}
}
void SetUpWaypointFocusEntityGPS()
{
	CEntity * pEntity = CDebugScene::FocusEntities_Get(0);
	if(pEntity)
	{
		Vector3 vec = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		CGps::m_Slots[GPS_SLOT_WAYPOINT].Start(
			vec,
			pEntity,
			0,
			true,
			GPS_SLOT_WAYPOINT,
			CHudColour::GetRGB(HUD_COLOUR_WAYPOINT, 255));
	}
}



void SetUpTestRoute()
{
	Vector3 vPts[] =
	{
		Vector3(131.0f, -1320.0f, 28.5f),
		Vector3(84.0f, -1335.0f, 28.5f),
		Vector3(56.0f, -1314.0f, 28.5f),
		Vector3(46.0f, -1300.0f, 28.5f),
		Vector3(26.0f, -1297.0f, 28.5f)
	};

	CGps::StartCustomRoute(0xff00ff00, true, false);
	CGps::AddPointToCustomRoute(vPts[0]);
	CGps::AddPointToCustomRoute(vPts[1]);
	CGps::AddPointToCustomRoute(vPts[2]);
	CGps::AddPointToCustomRoute(vPts[3]);
	CGps::AddPointToCustomRoute(vPts[4]);
	CGps::RenderCustomRoute(true, 4, 4);
}

void SetUpTestRaceTrack()
{
	Vector3 vPts[] =
	{
		Vector3( -1118.0f, -928.0f, 2.0f ),
		Vector3( -951.0f, -1216.0f, 5.0f ),
		Vector3( -1149.5f, -1305.0f, 5.0f ),
		Vector3( -1284.0f, -918.0f, 12.0f ),
		Vector3( -1174.0f, -846.0f, 15.0f )
	};

	CGps::StartRaceTrack(15);
	CGps::AddPointToRaceTrack(vPts[0]);
	CGps::AddPointToRaceTrack(vPts[1]);
	CGps::AddPointToRaceTrack(vPts[2]);
	CGps::AddPointToRaceTrack(vPts[3]);
	CGps::AddPointToRaceTrack(vPts[4]);
	CGps::RenderRaceTrack(true);
}

void SetUpWaypointRoute()
{
	CGps::StartCustomRoute(0xff00ff00, true, false);
	CGps::InitCustomRouteFromWaypointRecordingRoute("FBI1", 0, -1, 0xff00ff00, true, false );
	CGps::RenderCustomRoute(true, 4, 4);
}

void SetUpAssistedRoute()
{
	CGps::StartCustomRoute(0xff00ff00, true, false);
	CGps::InitCustomRouteFromAssistedMovementRoute("dumbass", 1, 0, -1, 0xff00ff00, true, false );
	CGps::RenderCustomRoute(true, 4, 4);
}

void SetUpMultiGPSRoute(bool bTrackPlayer)
{
	const Vector3 vPts[] =
	{
		Vector3( 302.0f, 3422.0f, 38.0f ),
		Vector3( 2060.0f, 3721.0f, 35.0f ),
		Vector3( 1667.0f, 3860.0f, 38.0f )
	};
	CGps::StartMultiGPS(10, bTrackPlayer, false);
	CGps::AddPointToMultiGPS(vPts[0]);
	CGps::AddPointToMultiGPS(vPts[1]);
	CGps::AddPointToMultiGPS(vPts[2]);
	CGps::RenderMultiGPS(true);
}
void SetUpMultiGPSRoute_TrackPlayer()
{
	SetUpMultiGPSRoute(true);
}
void SetUpMultiGPSRoute_NoTrackPlayer()
{
	SetUpMultiGPSRoute(false);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitWidgets
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
void CGps::InitWidgets()
{
	m_bDebug = false;

	bkBank *bank = CGameLogic::GetGameLogicBank();

	bank->PushGroup("Gps");
	bank->AddToggle("Print debug", &CGps::m_bDebug);
	bank->AddButton("Test race-track", SetUpTestRaceTrack);
	bank->AddButton("Test custom route", SetUpTestRoute);
	bank->AddButton("Test waypoint route", SetUpWaypointRoute);
	bank->AddButton("Test assisted route", SetUpAssistedRoute);
	bank->AddButton("Test Multi-GPS route", SetUpMultiGPSRoute_TrackPlayer);
	bank->AddButton("Test Multi-GPS route (no track player)", SetUpMultiGPSRoute_NoTrackPlayer);
	bank->AddButton("Test GPS blip focus entity", SetUpBlipFocusEntityGPS);
	bank->AddButton("Test waypoint route to focus entity", SetUpWaypointFocusEntityGPS);
	
	
	//	bank->AddButton("Start debug route to (0, 0, 0)",datCallback(CFA1(StartDebugRoute)),"Start debug route");
	bank->AddSlider("Turn predelay:", &g_TurnPredelay, 0, 2000, 1);
	bank->AddSlider("Time til Bong:", &g_TimeTilBong, 0, 5000, 1);
	bank->AddSlider("Driving fast time scaling factor:", &g_DrivingFastTimeScalingFactor, 0.0f, 5.0f, 0.01f);
	bank->AddSlider("Time to play turn:", &g_TimeToPlayTurn, 0, 15000, 1);
	bank->AddSlider("Time to play turn-then-imm:", &g_TimeToPlayTurnThenImmediately, 0, 15000, 1);
	bank->AddSlider("Time to play just turn:", &g_TimeToPlayJustTurn, 0, 15000, 1);
	bank->AddSlider("Min time between announcements:", &g_MinTimeBetweenAnnouncements, 0, 5000, 1);
	bank->AddSlider("Ideal time between announcements:", &g_IdealTimeBetweenAnnouncements, 0, 5000, 1);
	bank->AddSlider("Distance reduction:", &g_DistanceReduction, 0.0f, 25.0f, 0.1f);
	bank->AddToggle("Say distance first", &g_SayDistanceFirst);
	bank->AddSlider("Highlighted route distance:", &g_ProceedToHighlightedDistance, 0.0f, 50.0f, 0.1f);
	bank->AddSlider("UTurn dot product:", &g_UTurnDotProduct, -1.0f, 1.0f, 0.01f);
	bank->AddSlider("Highlighted route distance:", &g_ProceedToHighlightedDistance, 0.0f, 50.0f, 0.1f);
	bank->AddSlider("GPS voice:", &g_GPSVoice, 0, 10, 1);
	bank->AddSlider("Immediate Turn Distance:", &g_ImmediateTurnDistance, 0.0f, 250.0f, 1.0f);
	bank->AddSlider("Route Width Minimap:", &ms_iRoadWidthMiniMap, 0.1f, 100.0f, 0.1f);
	bank->AddSlider("Route Width Pause Map:", &ms_iRoadWidthPauseMap, 0.1f, 100.0f, 0.1f);

	bank->AddSeparator();

	bank->AddSlider("Max duration before redrawing route", &CGpsSlot::m_iMaxDurationBeforeRedrawingRoute, 1, 10000, 1);
	bank->AddSlider("Min duration before progressing route", &CGpsSlot::m_iMinDurationBeforeProgressingRoute, 1, 10000, 1);
	bank->AddSlider("Min duration between recalculating route", &CGpsSlot::m_iMinDurationBetweenRecalculatingRoute, 1, 10000, 1);
	bank->AddSlider("Max Z dist before recalculating route", &CGpsSlot::m_iMaxZDistanceBeforeRecalculatingRoute, 1, 1000, 1);
	bank->AddSlider("Target entity max dist moved before recalculating", &CGpsSlot::m_iTargetEntityMaxDistMayMoveBeforeRecalculatingRoute, 1, 1000, 1);
	bank->AddSlider("Player max dist moved before recalculating", &CGpsSlot::m_iPlayerMaxDistMayMoveBeforeRecalculatingRoute, 1, 1000, 1);
	bank->AddSlider("Check num segments", &CGpsSlot::m_iCheckNumSegments, 1, 1000, 1);

	bank->PopGroup();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StartDebugRoute
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
/*
void CGps::StartDebugRoute()
{
	Vector3	testDest = Vector3(0.0f, 0.0f, 0.0f);
//	Vector3	source = CGameWorld::FindLocalPlayerCoors();
	m_Slots[GPS_SLOT_RADAR_BLIP].Start(testDest, NULL, 0, false, GPS_SLOT_RADAR_BLIP, CHudColour::GetRGB(HUD_COLOUR_RED, 255));

}
*/


#endif // __BANK
