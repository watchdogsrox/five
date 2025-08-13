//--------------------------------------------------------------------------------------
// companion.cpp
//--------------------------------------------------------------------------------------

#include "Companion.h"

#if COMPANION_APP
#if RSG_ORBIS
#include <app_content.h>
#endif	//	RSG_ORBIS
#include "data/base64.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/MiniMap.h"
#include "script/script_channel.h"
#include "system/criticalsection.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Stats/StatsMgr.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Peds/PlayerInfo.h"
#include "control/GameLogic.h"
#include "control/gps.h"
#include "camera/CamInterface.h"

#include "script/script_hud.h"

RAGE_DEFINE_CHANNEL(Companion)

static sysCriticalSectionToken s_critSecToken;
CCompanionData* CCompanionData::sm_instance = NULL;

//--------------------------------------------------------------------------------------
// Companion Data class
//--------------------------------------------------------------------------------------
CCompanionData::CCompanionData()
{
	m_isConnected = false;
	m_createWaypoint = false;
	m_removeWaypoint = false;
	m_resendAll = false;
	m_togglePause = false;
	m_newWaypointX = 0;
	m_newWaypointY = 0;
	m_routeNodeCount[0] = -1;
	m_routeNodeCount[1] = -1;
	m_routeColour = Color32(240, 200, 80, 255);
	m_mapDisplay = eMapDisplay::MAP_DISPLAY_NORMAL;
	m_isInPlay = false;
}

CCompanionData::~CCompanionData()
{
}

// #include "profile/element.h"
// #include "profile/group.h"
// #include "profile/page.h"

// PF_PAGE(RobMB, "AAB RobM Stuff");
// PF_GROUP(RobMBSend);
// PF_LINK(RobMB, RobMBSend);
// PF_TIMER(CreateWriterCost, RobMBSend);
// PF_TIMER(CloseWriterCost, RobMBSend);
// PF_TIMER(CreateArrayCost, RobMBSend);
// PF_TIMER(CloseArrayCost, RobMBSend);
// PF_TIMER(AddField1Cost, RobMBSend);
// PF_TIMER(AddField2Cost, RobMBSend);
// PF_TIMER(AddBlipCost, RobMBSend);
// PF_TIMER(SendPacketCost, RobMBSend);
// PF_TIMER(Base64Cost, RobMBSend);

void CCompanionData::Update()
{
	//	Reset everything if we need to send everything from clean
	if (m_resendAll)
	{
		//	Set the number of route nodes to 0
		ClearRouteNodeCount(0);
		ClearRouteNodeCount(1);
		
		//	Erase all blips
		RemoveOldBlipMessages(false);
		m_blipLinkedList.clear();
		
		//	Erase all routes
		RemoveOldRouteMessages(false);
		m_routeNodeLinkedList.clear();

		m_resendAll = false;
	}
	else
	{
		//	Create a waypoint if one is requested from the companion app
		if (m_createWaypoint)
		{
			//	Switch off any current waypoint
			if (CMiniMap::IsWaypointActive())
			{
				CMiniMap::SwitchOffWaypoint();
				//	Reset the routes so the companion drawing doesn't go mental
				RemoveOldRouteMessages(false);
				m_routeNodeLinkedList.clear();
			}
			//	Create a new waypoint
			CMiniMap::SwitchOnWaypoint((float)m_newWaypointX, (float)m_newWaypointY, NETWORK_INVALID_OBJECT_ID, false/*CMapMenu::sm_bLockedToSelectedBlip*/);  // AdamF wants the waypoint to be visible

			m_createWaypoint = false;
		}
		//	Remove a waypoint if requested from the companion app
		else if (m_removeWaypoint)
		{
			//	Switch off any current waypoint
			if (CMiniMap::IsWaypointActive())
			{
				CMiniMap::SwitchOffWaypoint();
				//	Reset the routes so the companion drawing doesn't go mental
				RemoveOldRouteMessages(false);
				m_routeNodeLinkedList.clear();
			}

			m_removeWaypoint = false;
		}
	}

	if (m_togglePause)
	{
		// [B* 1892361]: we received a request to toggle the pause menu, first off turn the flag off
		m_togglePause = false;

		// check the status of the pause menu
		bool pauseMenuActive = CPauseMenu::IsActive();
		if (pauseMenuActive == false)
		{
			// The menu is no active - do that now
			CPauseMenu::OpenCorrectMenu();
		}
		else
		{
			CPauseMenu::Close();
		}
	}

	//	Set is in play switch
	m_isInPlay = !CutSceneManager::GetInstance()->IsRunning() && IsPlayerActive();
}

HRESULT CCompanionData::AddBlip( DWORD /*nClientId*/, CBlipComplex* pBlip, Color32 colour )
{
// 	PF_FUNC(AddBlipCost);
	HRESULT hr = S_OK;

	//	We don't want to add the north blip
	if (pBlip->iLinkageId == BLIP_NORTH)
	{
		return S_OK;
	}

	sCompanionBlip newBlip;

	//	Populate the blip data structure
	newBlip.basicInfo.x = pBlip->vPosition.GetX();
	newBlip.basicInfo.y = pBlip->vPosition.GetY();
	newBlip.basicInfo.rotation = static_cast<s16>(pBlip->fDirection);
	newBlip.basicInfo.iconId = pBlip->iLinkageId;
	newBlip.basicInfo.id = pBlip->m_iUniqueId;

	newBlip.basicInfo.r = colour.GetRed();
	newBlip.basicInfo.g = colour.GetGreen();
	newBlip.basicInfo.b = colour.GetBlue();
	newBlip.basicInfo.a = pBlip->iAlpha;

	newBlip.basicInfo.labelSize = (u8)strlen(CMiniMap::GetBlipNameValue(pBlip));

	strcpy(newBlip.label, CMiniMap::GetBlipNameValue(pBlip));

	// Get and set z Offset for the blip
	newBlip.basicInfo.priority = pBlip->priority;
	newBlip.basicInfo.category = pBlip->iCategory;
	newBlip.scale = pBlip->vScale.x; // this code's not used, but let's just pick this one so it compiles

	// zero extra values
	newBlip.basicInfo.flags = 0;
	newBlip.secondaryColour = 0;

	// check for crew indicator
	if (pBlip->m_flags.IsSet(BLIP_FLAG_SHOW_CREW_INDICATOR))
	{
		newBlip.basicInfo.flags |= COMPANIONBLIP_CREW_INDICATOR;
		newBlip.secondaryColour |= pBlip->iColourSecondary.GetRed() << 24;
		newBlip.secondaryColour |= pBlip->iColourSecondary.GetGreen() << 16;
		newBlip.secondaryColour |= pBlip->iColourSecondary.GetBlue() << 8;
	}
	else
	if (pBlip->m_flags.IsSet(BLIP_FLAG_SHOW_TICK))
	{
		newBlip.basicInfo.flags |= COMPANIONBLIP_TICK_INDICATOR;
	}
	else
	if (pBlip->m_flags.IsSet(BLIP_FLAG_SHOW_GOLD_TICK))
	{
		newBlip.basicInfo.flags |= COMPANIONBLIP_GOLD_TICK_INDICATOR;
	}
	else
	if (pBlip->m_flags.IsSet(BLIP_FLAG_FLASHING))
	{
		newBlip.basicInfo.flags |= COMPANIONBLIP_FLASH_INDICATOR;
	}
	else
	if(pBlip->type == BLIP_TYPE_RADIUS)
	{
		// we need the scale value to render the radius propery
		newBlip.basicInfo.flags |= COMPANIONBLIP_TYPE_RADIUS;
	}

	if (pBlip->m_flags.IsSet(BLIP_FLAG_SHOW_FRIEND_INDICATOR))
	{
		newBlip.basicInfo.flags |= COMPANIONBLIP_FRIEND_INDICATOR;
	}

	if (pBlip->m_flags.IsSet(BLIP_FLAG_HIDDEN_ON_LEGEND))
	{
		newBlip.basicInfo.flags |= COMPANIONBLIP_NO_LEGEND;
	}

	bool found = false;

	for (BlipLinkedListIterator it = m_blipLinkedList.begin(); it != m_blipLinkedList.end(); it++)
	{
		CCompanionBlipNode* currentBlip = *it;

		if (newBlip.basicInfo.id == currentBlip->m_blip.basicInfo.id)
		{
			//	Check display, incase it needs hiding
			if (pBlip->display == BLIP_DISPLAY_NEITHER)
			{
#if HASHED_BLIP_IDS
				newBlip.basicInfo.iconId.Clear();
#else
				newBlip.basicInfo.iconId = -1;	//	Set to -1 to remove from companion app
#endif
			}

			currentBlip->Update(newBlip);
			found = true;
			break;
		}
	}

	if (!found)
	{
		if (pBlip->display != BLIP_DISPLAY_NEITHER)
		{
			//	Add it to the blip list
			m_blipLinkedList.push_back(rage_new CCompanionBlipNode(newBlip));
		}
	}

	return hr;
}

void CCompanionData::UpdateGpsRoute(s16 routeId, s32 numNodes, Vector3* pNodes, Color32 colour)
{
	int lastValidNode = 0;
	s16 nodeId = 0;
	bool bClipFirstLine = false;
	Vector3	vSourcePoint(0,0,0);
	const CPed * pLocalPlayer = CGameWorld::FindLocalPlayer();

	//	Only bother setting colour if it's the mission route, not user waypoint
	if (routeId == 1)
	{
		SetRouteColour(colour);
	}

	if (pLocalPlayer)
	{
		vSourcePoint = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());
		bClipFirstLine = true;
	}

	if (numNodes > 0)
	{
		for (s32 i = 1; i < numNodes; i++)
		{
			if ((*(int*)&pNodes[i].w) == GNI_IGNORE_FOR_NAV)
				continue;

			Vector2 vMain = Vector2(pNodes[i].x, pNodes[i].y);

			if (lastValidNode == 0)
			{
				lastValidNode = i;
				Vector2 vLink(pNodes[0].x, pNodes[0].y);

				if (bClipFirstLine)
				{
					Vector2 diff = vLink - vMain;
					float	length = diff.Mag();
					Vector2 diffClip = Vector2(vSourcePoint.x, vSourcePoint.y) - vMain;
					float	lengthAlongLine = (diffClip.x * diff.x + diffClip.y * diff.y) / length;
					lengthAlongLine = rage::Max(0.0f, lengthAlongLine);

					if (lengthAlongLine < length)
					{	
						// reduced line segment.
						vLink = vMain + diff * (lengthAlongLine / length);
					}
				}

				if ((*(int*)&pNodes[0].w) == GNI_PLAYER_TRAIL_POS)	// Adding extra point
				{
					AddGpsPoint(0, routeId, nodeId++, vSourcePoint.x, vSourcePoint.y);
					AddGpsPoint(0, routeId, nodeId++, vLink.x, vLink.y);
				}
				else
				{
					AddGpsPoint(0, routeId, nodeId++, vLink.x, vLink.y);
				}

				AddGpsPoint(0, routeId, nodeId++, vMain.x, vMain.y);
			}
			else
			{
				AddGpsPoint(0, routeId, nodeId++, vMain.x, vMain.y);

				if ((*(int*)&pNodes[i].w) == GNI_PLAYER_TRAIL_POS)	// Adding extra point
				{
					AddGpsPoint(0, routeId, nodeId++, pNodes[i+1].x, pNodes[i+1].y);
				}
			}
		}

		SetRouteNodeCount(routeId, nodeId);
	}
	else
	{
		ClearRouteNodeCount(routeId);
	}
}

HRESULT CCompanionData::AddGpsPoint( DWORD /*nClientId*/, s16 routeId, s16 id, double x, double y )
{
	// 	PF_FUNC(AddBlipCost);
	HRESULT hr = S_OK;

	if (routeId > 1)
	{
		return S_OK;
	}

	sCompanionRouteNode newNode;

	//	Populate the route node data structure
	newNode.x = x;
	newNode.y = y;

	newNode.id = id;
	newNode.routeId = routeId;

	bool found = false;

	for (RouteNodeLinkedListIterator it = m_routeNodeLinkedList.begin(); it != m_routeNodeLinkedList.end(); it++)
	{
		CCompanionRouteNode* currentNode = *it;

		if (newNode.id == currentNode->m_routeNode.id && 
			newNode.routeId == currentNode->m_routeNode.routeId)
		{
			currentNode->Update(newNode);
			found = true;
			break;
		}
	}

	if (!found)
	{
		//	Add it to the route node list
		m_routeNodeLinkedList.push_back(rage_new CCompanionRouteNode(newNode));
	}

	return hr;
}

void CCompanionData::RemoveBlip(CBlipComplex* pBlip)
{
	sCompanionBlip newBlip;

	//	Populate the blip data structure
	newBlip.basicInfo.x = 0;
	newBlip.basicInfo.y = 0;
	newBlip.basicInfo.rotation = 0;
#if HASHED_BLIP_IDS
	newBlip.basicInfo.iconId.Clear();
#else
	newBlip.basicInfo.iconId = -1;	//	Set to -1 to remove from companion app
#endif
	newBlip.basicInfo.id = pBlip->m_iUniqueId;

	newBlip.basicInfo.r = 0;
	newBlip.basicInfo.g = 0;
	newBlip.basicInfo.b = 0;
	newBlip.basicInfo.a = 0;

	newBlip.basicInfo.labelSize = (u8)strlen(CMiniMap::GetBlipNameValue(pBlip));

	strcpy(newBlip.label, CMiniMap::GetBlipNameValue(pBlip));

	for (BlipLinkedListIterator it = m_blipLinkedList.begin(); it != m_blipLinkedList.end(); it++)
	{
		CCompanionBlipNode* currentBlip = *it;

		if (newBlip.basicInfo.id == currentBlip->m_blip.basicInfo.id)
		{
			currentBlip->Update(newBlip);
			break;
		}
	}
}

bool CCompanionData::IsFlagSet(const CMiniMapBlip *pBlip, u32 iQueryFlag)
{
	// optimisation - assume pBlip is valid if passed in, should speed things up
	if (pBlip->IsComplex())
	{
		return ((CBlipComplex*)pBlip)->m_flags.IsSet(iQueryFlag);
	}
	else
	{
		return false;//((CBlipComplex*)blip[iSimpleBlip])->m_flags.IsSet(iQueryFlag);
	}
}

CCompanionBlipMessage* CCompanionData::GetBlipMessage()
{
	//	Get the message on the top of the stack
	CCompanionBlipMessage* blipMessage = rage_new CCompanionBlipMessage();

	if (m_blipLinkedList.size() > 0)
	{
		for (BlipLinkedListIterator it = m_blipLinkedList.begin(); it != m_blipLinkedList.end(); it++)
		{
			CCompanionBlipNode* newBlip = *it;

			if (newBlip->HasBeenUpdated())
			{
				if (blipMessage->AddBlip(newBlip->m_blip))
				{
					newBlip->SetUpdated(false);
				}
				else
				{
					break;
				}
			}
		}
	}

	return blipMessage;
}

CCompanionRouteMessage* CCompanionData::GetRouteMessage()
{
	//	Get the message on the top of the stack
	CCompanionRouteMessage* routeMessage = rage_new CCompanionRouteMessage();

	if (m_routeNodeLinkedList.size() > 0)
	{
		for (RouteNodeLinkedListIterator it = m_routeNodeLinkedList.begin(); it != m_routeNodeLinkedList.end(); it++)
		{
			CCompanionRouteNode* newNode = *it;

			if (newNode->HasBeenUpdated())
			{
				if (routeMessage->AddNode(newNode->m_routeNode))
				{
					newNode->SetUpdated(false);
				}
				else
				{
					break;
				}
			}
		}
	}

	return routeMessage;
}

void CCompanionData::RemoveOldBlipMessages(bool whenReady)
{
	BlipLinkedListIterator it = m_blipLinkedList.begin();

	while (it != m_blipLinkedList.end())
	{
		CCompanionBlipNode* currentNode = *it;

		if (currentNode && (!whenReady || currentNode->ReadyToDelete()))
		{
			it = m_blipLinkedList.erase(it);
			delete currentNode;
		}
		else
		{
			++it;
		}
	}
}

void CCompanionData::RemoveOldRouteMessages(bool whenReady)
{
	RouteNodeLinkedListIterator it = m_routeNodeLinkedList.begin();

	while (it != m_routeNodeLinkedList.end())
	{
		CCompanionRouteNode* currentNode = *it;

		if (currentNode && (!whenReady || currentNode->ReadyToDelete()))
		{
			it = m_routeNodeLinkedList.erase(it);
			delete currentNode;
		}
		else
		{
			++it;
		}
	}
}

void CCompanionData::ClearRouteNodeCount(int routeId)
{
	if (routeId > 1)
	{
		return;
	}

	m_routeNodeCount[routeId] = -1;

	RouteNodeLinkedListIterator it = m_routeNodeLinkedList.begin();

	// clear the actual nodes belonging to the route
	while (it != m_routeNodeLinkedList.end())
	{
		CCompanionRouteNode* currentNode = *it;

		if (currentNode && currentNode->m_routeNode.routeId == routeId)
		{
			it = m_routeNodeLinkedList.erase(it);
			delete currentNode;
		}
		else
		{
			++it;
		}
	}
}

s16 CCompanionData::GetRouteNodeCount(int routeId)
{
	if (routeId > 1)
	{
		return 0;
	}

	return m_routeNodeCount[routeId];
}

void CCompanionData::SetRouteNodeCount(int routeId, s16 nodeCount)
{
	if (routeId > 1)
	{
		return;
	}

	m_routeNodeCount[routeId] = nodeCount;
}

void CCompanionData::CreateWaypoint(double x, double y)
{
	m_createWaypoint = true;
	m_newWaypointX = x;
	m_newWaypointY = y;
}

void CCompanionData::RemoveWaypoint()
{
	m_removeWaypoint = true;
}

void CCompanionData::ResendAll()
{
	m_resendAll = true;
}

void CCompanionData::TogglePause()
{
	m_togglePause = true;
}

void CCompanionData::SetRouteColour(Color32 colour)
{
	m_routeColour = colour;
}

void CCompanionData::SetMapDisplay(eMapDisplay display)
{
	m_mapDisplay = display;
}

void CCompanionData::OnClientConnected()
{
	m_isConnected = true;
}

void CCompanionData::OnClientDisconnected()
{
	m_isConnected = false;
}

// This is based on CStatsMgr::IsPlayerActive() - slighty modified to accomodate the needs of the companion app
bool CCompanionData::IsPlayerActive()
{
	static bool s_IsActive = false;

	CPlayerInfo* pi = CGameWorld::GetMainPlayerInfo();
	bool isSpectating = (NetworkInterface::IsGameInProgress() && NetworkInterface::IsInSpectatorMode());

	if (isSpectating)
	{
		s_IsActive = true;
	}
	else if (!pi)
	{
		s_IsActive = false;
	}
	else if (NetworkInterface::IsInSinglePlayerPrivateSession())
	{
		s_IsActive = false;
	}
	else if (g_PlayerSwitch.IsActive())
	{
		s_IsActive = false;
	}
	else if (pi->GetPlayerState() != CPlayerInfo::PLAYERSTATE_PLAYING)
	{
		s_IsActive = false;
	}
	else if (!CGameLogic::IsGameStateInPlay())
	{
		s_IsActive = false;
	}
	else if (!camInterface::IsFadedIn())
	{
		s_IsActive = false;
	}
	else if (NetworkInterface::IsGameInProgress() && pi->AreAnyControlsOtherThanFrontendDisabled())
	{
		s_IsActive = false;
	}
	else if (CScriptHud::bDontDisplayHudOrRadarThisFrame || !CScriptHud::bDisplayRadar)
	{
		s_IsActive = false;
	}
	else
	{
		s_IsActive = true;

		// If a menu is active, we need to check if we are in pause or otherwise.
		if (CPauseMenu::IsActive())
		{
			int currentMenu = CPauseMenu::GetCurrentMenuVersion();
			if (!(currentMenu == FE_MENU_VERSION_SP_PAUSE || currentMenu == FE_MENU_VERSION_MP_PAUSE))
			{
				s_IsActive = false;
			}
		}

		// Some interior rooms (like garages) are not in the right geographic location, 
		// but there is no way to detect it; so we check for being deep underground and
		// inside an interior.
		CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
		bool bIsInside = pPlayerPed->GetPortalTracker()->IsInsideInterior();

		if (pi && pPlayerPed && bIsInside)
		{
			float hPos = pi->ms_cachedMainPlayerPos.z;
			if (hPos < -80)
			{
				s_IsActive = false;
			}
		}
	}

	return s_IsActive;
}

#endif	//	COMPANION_APP