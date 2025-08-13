#include "MinimapMenuComponent.h"

#include "frontend/MiniMap.h"
#include "frontend/MiniMapRenderThread.h"
#include "frontend/PauseMenu.h"
#include "audio/frontendaudioentity.h"
#include "script/commands_hud.h"
#include "frontend/HudTools.h"
#include "Frontend/CMapMenu.h"

CMiniMapMenuComponent sMiniMapMenuComponent;

void CMiniMapMenuComponent::InitClass()
{
	ResetClass();
	m_pGalleryBlips.Reset();
}

void CMiniMapMenuComponent::ShutdownClass()
{
	ResetClass();
	ResetBlips();

	if (CPauseMenu::IsActive())
	{
		CMapMenu::InitStartingValues();  // set the starting values again of the pausemap
	}
}

void CMiniMapMenuComponent::ResetClass()
{
	SetActive(false);
	//m_iBlipObject = -1;
}

void CMiniMapMenuComponent::RenderMap(const PauseMenuRenderDataExtra& renderData)
{
	if (CMiniMap_RenderThread::ShouldRenderMiniMap())
	{
		if(const SGeneralPauseDataConfig* pData = CPauseMenu::GetMenuArray().GeneralData.MovieSettings.Access("CoronaMapBG"))
		{
			Vector2 vPos ( pData->vPos  );
			Vector2 vSize( pData->vSize );

#if SUPPORT_MULTI_MONITOR
			CSprite2d::MovePosVectorToScreenGUI(vPos, vSize);
#endif //SUPPORT_MULTI_MONITOR

			CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_CENTRE, &vPos, &vSize);

			CMiniMap_RenderThread::RenderPauseMap(vPos, vSize, renderData.fAlphaFader, renderData.fBlipAlphaFader, renderData.fSizeScalar);
		}
	}
}

void CMiniMapMenuComponent::SetActive(bool bActive)
{
	if (bActive != m_bIsActive)
	{
		// need to set up the new minimap state before we change the state of the minimap component as its checked inside SetMinimapModeState
		if( bActive )
		{
			CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP);
		}
		else
		{
			CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP);
		}

		m_bIsActive = bActive;
	}
}
	
//===============================================================
// CMiniMapMenuComponent - SnapToBlip(s32 iBlipIndex)
// Purpose: Centers the minimap on a particular blip
// Param: iBlipIndex - the index of the blip as added.
//===============================================================
bool CMiniMapMenuComponent::SnapToBlip(s32 iKey)
{

	bool bResult = false;
	s32* pBlipId = m_pGalleryBlips.Access(iKey);

	if (!pBlipId)
	{
		Assert(0);
		return false;
	}

	s32 iUniqueId = *pBlipId;
	CMiniMapBlip* pBlip = CMiniMap::GetBlip(iUniqueId);

	if (pBlip)
	{
		Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);
		CMiniMap::SetPauseMapCursor(Vector2(vBlipPos.x, vBlipPos.y));
		bResult = true;
	}

	return bResult;
}

//===============================================================
// CMiniMapMenuComponent - SnapToBlip(s32 iBlipIndex)
// Purpose: Centers the minimap on a particular blip
// Param: iBlipIndex - the index of the blip as added.
//===============================================================
bool CMiniMapMenuComponent::SnapToBlipWithDistanceCheck(s32 iKey, float fDistanceRequirement)
{

	bool bResult = false;

	s32* pBlipId =  m_pGalleryBlips.Access(iKey);

	if (!pBlipId)
	{
		char szMsg[64];
		formatf(szMsg,64,"Invalid key for blip. Blip key=%d",iKey);
		AssertMsg(false, szMsg);

		return false;
	}

	s32 iUniqueId = *pBlipId;
	CMiniMapBlip* pBlip = CMiniMap::GetBlip(iUniqueId);

	if (pBlip)
	{
		Vector2 vCurrentPos = CMiniMap::GetPauseMapCursor();
		Vector2 vBlipPos = Vector2(CMiniMap::GetBlipPositionValue(pBlip).x, CMiniMap::GetBlipPositionValue(pBlip).y);
		
		float fDistance = vBlipPos.Dist(vCurrentPos);
		//photoDisplayf("CGalleryMenu::Distance%f", fDistance);

		if (fDistance > fDistanceRequirement)
		{
			CMiniMap::SetPauseMapCursor(vBlipPos);
			bResult = true;
		}
	}

	return bResult;
}


bool CMiniMapMenuComponent::SnapToBlipUsingUniqueId(s32 iUniqueId)
{
	bool bResult = false;

	if (iUniqueId >= 0)
	{
		int iFoundIndex = -1;

		for (int i = 0; i < m_pGalleryBlips.GetNumUsed(); i++)
		{
			if (iUniqueId == m_pGalleryBlips[i])
			{
				iFoundIndex = i;
				break;
			}
		}

		CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_pGalleryBlips[iFoundIndex]);

		if (pBlip)
		{
			Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);
			CMiniMap::SetPauseMapCursor(Vector2(vBlipPos.x, vBlipPos.y));
			bResult = true;
		}
	}

	return bResult;
}
//===============================================================
// CMiniMapMenuComponent - SnapToBlip(s32 iBlipIndex)
// Purpose: Centers the minimap on a particular blip
// Param: pBlip - pointer to the blip to be centered on
//===============================================================
bool CMiniMapMenuComponent::SnapToBlip(CMiniMapBlip *pBlip)
{
	bool bResult = false;

	if (pBlip)
	{
		Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);
		CMiniMap::SetPauseMapCursor(Vector2(vBlipPos.x, vBlipPos.y));
		bResult = true;
	}

	return bResult;
}

bool CMiniMapMenuComponent::DoesBlipExist(int iIndex)
{
	if (m_pGalleryBlips.Access(iIndex))
	{
		return true;
	}

	return false;
}

//===============================================================
// CMiniMapMenuComponent - CreateBlip(Vector3& vPosition,u32 uKey)
// Purpose: Create a new blip that is localized to this particular component
// Param: vPosition - position of the blip to be centered on
//===============================================================
s32 CMiniMapMenuComponent::CreateBlip(Vector3& vPosition, u32 uKey)
{
	// Zero out the z portion so that blips are always on level.
	vPosition.z = 0.0f;
	s32 sBlipId = CMiniMap::CreateBlip(true, BLIP_TYPE_CUSTOM, vPosition, BLIP_DISPLAY_CUSTOM_MAP_ONLY, NULL);

	m_pGalleryBlips.Insert(uKey,sBlipId);

	return sBlipId;
}

//===============================================================
// CMiniMapMenuComponent - CreateBlip(Vector3& vPosition)
// Purpose: Create a new blip that is localized to this particular component
// Param: vPosition - position of the blip to be centered on
//===============================================================
s32 CMiniMapMenuComponent::CreateBlip(Vector3& vPosition)
{
	// Zero out the z portion so that blips are always on level.
	vPosition.z = 0.0f;
	s32 sBlipId = CMiniMap::CreateBlip(true, BLIP_TYPE_CUSTOM, vPosition, BLIP_DISPLAY_CUSTOM_MAP_ONLY, NULL);
	
	m_pGalleryBlips.Insert(m_pGalleryBlips.GetNumUsed(),sBlipId);
	
	return sBlipId;
}

//===============================================================
// CMiniMapMenuComponent - RemoveBlip(s32 iIndex)
// Purpose: Remove a blip from the minimap via the index.
//===============================================================
bool CMiniMapMenuComponent::RemoveBlip(s32 iKey)
{
	bool bResult = false;

	s32* pBlipId = m_pGalleryBlips.Access(iKey);
	
	if (!pBlipId)
	{
		char szMsg[64];
		formatf(szMsg,64,"Invalid key for blip. Blip key=%d",iKey);
		AssertMsg(false, szMsg);

		return false;
	}

	s32 iUniqueId = *pBlipId;
	CMiniMapBlip* pBlip = CMiniMap::GetBlip(iUniqueId);

	if (pBlip)
		bResult = CMiniMap::RemoveBlip(pBlip);

	if (bResult)
		m_pGalleryBlips.Delete(iKey);

	return bResult;
}

//===============================================================
// CMiniMapMenuComponent - RemoveBlip(s32 iUniqueId)
// Purpose: Remove a blip from the minimap via the index.
//===============================================================
bool CMiniMapMenuComponent::RemoveBlipByUniqueId(s32 iUniqueId)
{
	bool bResult = false;
	int iFoundIndex = -1;

	for (int i = 0; i < m_pGalleryBlips.GetNumUsed(); i++)
	{
		if (iUniqueId == m_pGalleryBlips[i])
		{
			iFoundIndex = i;
			break;
		}
	}

	if (iFoundIndex > -1)
	{
		CMiniMapBlip* pBlip = CMiniMap::GetBlip(m_pGalleryBlips[iFoundIndex]);

		if (pBlip)
			bResult = CMiniMap::RemoveBlip(pBlip);

		if (bResult)
			m_pGalleryBlips.Delete(iFoundIndex);
	}

	return bResult;
}

//===============================================================
// CMiniMapMenuComponent - SetWaypoint()
// Purpose: Set a waypoint for the current blip
//===============================================================
void CMiniMapMenuComponent::SetWaypoint()
{
	ObjectId BlipObjectId = NETWORK_INVALID_OBJECT_ID;

	if (CMiniMap::IsWaypointActive())
	{
		// Use the following ruleset:
		// 1) If SetWaypoint called on the same blip that is currently active and has a waypoint already, just disable it.
		// 2) If its a different blip, disable and enable on the new blip.
		CMiniMapBlip *pWaypointBlip = CMiniMap::GetBlip(CMiniMap::GetActiveWaypointId());
		Vector3 vCurrentWaypointPos(CMiniMap::GetBlipPositionValue(pWaypointBlip));

		uiDisplayf("CMiniMapMenuComponent::SetWaypoint - Clearing waypoint");
		CMiniMap::SwitchOffWaypoint();

		if (CMiniMap::GetPauseMapCursor().x == vCurrentWaypointPos.x && CMiniMap::GetPauseMapCursor().y == vCurrentWaypointPos.y)  // ensure a waypoint is never placed ontop of the player icon
		{
			return;
		}
		else if (netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(CMiniMap::FindBlipEntity(pWaypointBlip)))
		{
			BlipObjectId = pNetObj->GetObjectID();
		}
	}

	if (CPauseMenu::IsActive())
	{
		g_FrontendAudioEntity.PlaySound("WAYPOINT_SET", "HUD_FRONTEND_DEFAULT_SOUNDSET");
	}

	CMiniMap::SwitchOnWaypoint(CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y, BlipObjectId);
}

//===============================================================
// CMiniMapMenuComponent - ResetBlips()
// Purpose: Reset the blip container
//===============================================================
void CMiniMapMenuComponent::ResetBlips()
{
	for(atMap<s32,s32>::Iterator it = m_pGalleryBlips.CreateIterator(); !it.AtEnd(); it.Next())
	{
		CMiniMap::RemoveBlip(CMiniMap::GetBlip(it.GetData()));
	}
	m_pGalleryBlips.Reset();
}

