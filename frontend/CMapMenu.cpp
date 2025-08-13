/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CMapMenu.cpp
// PURPOSE : code to control the pausemap in the pausemenu
// AUTHOR  : Derek Payne
// STARTED : 24/10/2012
//
/////////////////////////////////////////////////////////////////////////////////

#include "Frontend/CMapMenu.h"
#include "audio/frontendaudioentity.h"
#include "camera/viewports/ViewportManager.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "frontend/ButtonEnum.h"
#include "frontend/FrontendStatsMgr.h"  // for checking whether we need metric measurements or not
#include "frontend/MapZoomData_parser.h"
#include "frontend/MiniMap.h"
#include "frontend/MiniMapRenderThread.h"
#include "frontend/PauseMenu.h"
#include "frontend/ScaleformMenuHelper.h"
#include "frontend/ui_channel.h"
#include "frontend/Map/BlipEnums.h"
#include "frontend/MousePointer.h"
#include "Game/User.h"
#include "Game/Wanted.h"
#include "Renderer/sprite2d.h"
#include "scene/streamer/SceneStreamer.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/WarpManager.h"
#include "script/script_hud.h"
#include "Stats/StatsTypes.h"
#include "text/TextConversion.h"
#include "text/TextFile.h"
#include "System/controlMgr.h"
#include "Scaleform/tweenstar.h"
#include "control/gps.h"


// dev
#if !__FINAL
#include "audio/northaudioengine.h"
#include "physics/physics.h"
#include "system/bootmgr.h"
#endif

FRONTEND_OPTIMISATIONS()

#define INVALID_ACTUAL_ID -1

//
// extremes of the pausemap movement
//
#define MAP_LIMIT_WEST (-3747.0f)  // added an extra 300m onto this for 1443279
#define MAP_LIMIT_EAST (4500.0f)
#define MAP_LIMIT_NORTH (8022.0f)
#define MAP_LIMIT_SOUTH (-4400.0f)

#define MAP_LIMIT_WEST_PROLOGUE (1935.0f)
#define MAP_LIMIT_EAST_PROLOGUE (7850.0f)
#define MAP_LIMIT_NORTH_PROLOGUE (-3561.0f)
#define MAP_LIMIT_SOUTH_PROLOGUE (-5295.0f)

#define MAP_LIMIT_WEST_ISLAND (3400.0f)
#define MAP_LIMIT_EAST_ISLAND (5750.0f)
#define MAP_LIMIT_NORTH_ISLAND (-4100.0f)
#define MAP_LIMIT_SOUTH_ISLAND (-6200.0f)

#define MAX_NUMBER_OF_LEGEND_ITEMS	(100)

atArray<sMapLegendList> CMapMenu::sm_LegendList;
s32 CMapMenu::sm_iPreviousHoverActualId = INVALID_ACTUAL_ID;
s32 CMapMenu::sm_iCurrentSelectedMissionCreatorUniqueId = INVALID_BLIP_ID;
bool CMapMenu::sm_bCanStartMission = false;
bool CMapMenu::sm_bShowStartMissionButton = false;
bool CMapMenu::sm_bCanContact = false;
bool CMapMenu::sm_bShowContactButton = false;
bool CMapMenu::sm_bUpdateLegend = false;
bool CMapMenu::sm_bHideLegend = false;
bool CMapMenu::sm_bPauseMapInInterior = false;
bool CMapMenu::sm_bPauseMapInGolf = false;
Vector2 CMapMenu::sm_vPauseMapLastZoomedPos;
Vector2 CMapMenu::sm_vPauseMapOriginalPos;
s32 CMapMenu::sm_iCurrentZoomLevel;
bool CMapMenu::sm_bCurrentlyZooming;
float CMapMenu::sm_fPrevZoomLevel = -1;
bool CMapMenu::sm_bIsRenderingMask = false;
bool CMapMenu::sm_bWasReloaded = false;

#define __INTERVAL_BETWEEN_LEGEND_UPDATES (1000)
#define __TIME_TO_WAIT_FOR_RESPONSE_TO_SCRIPT_FOR_BLIP_HOVER (1000)
u32 CMapMenu::sm_iUpdateLegendTimer;
u32 CMapMenu::sm_iMissionCreatorHoverQueryTimer;
bool CMapMenu::sm_bHasQueried = false;
Vector2 CMapMenu::sm_vPreviousCursorPos = Vector2(0,0);

#if KEYBOARD_MOUSE_SUPPORT
BankFloat CMapMenu::MOUSE_WHEEL_DECAY_RATE = 2.5f;
BankFloat CMapMenu::MOUSE_WHEEL_SCALAR = 0.75f;
BankFloat CMapMenu::MOUSE_FLICK_DECAY_RATE = 7.f;
BankFloat CMapMenu::MOUSE_ZOOM_SPEED_FAR = 10.f;
BankFloat CMapMenu::MOUSE_ZOOM_SPEED_NEAR = 100.0f;
BankFloat CMapMenu::MOUSE_DRAG_DIST_SQRD = 25.0f;
BankFloat CMapMenu::MOUSE_DRAG_SOUND_SCREEN_PIXELS_SQRD = 25.f;
BankFloat CMapMenu::MAX_SOUNDVAR_VALUE = 5.0f;
BankUInt32 CMapMenu::MOUSE_DBL_CLICK_TIME = 400;
BankInt32 CMapMenu::MAP_SNAP_PAN_SPEED = 350;
BankUInt32 MAP_FIRST_PAN_DELAY = 150;
BankUInt32 MAP_SPAM_PAN_DELAY = 250;
BankFloat MOUSE_PUSH_GUTTER_PCT = .05f;
int s_LERP_METHOD = rage::TweenStar::EASE_quadratic_EI;


bool s_bMapMovesOnWaypoint = true;
bool s_bCenterOnSingleClick = true;

PARAM(mapStaysOnWaypoints, "Whether the Pause Map moves on waypoints");
PARAM(mapCentersOnClick, "If the map centers on a single click and waypoints on a double");

#endif // KEYBOARD_MOUSE_SUPPORT

atArray<HoveredBlipUniqueId*> HoveredBlipUniqueId::sm_pAllElements;

HoveredBlipUniqueId::HoveredBlipUniqueId()
	: m_iUniqueId(INVALID_BLIP_ID)
{
	sm_pAllElements.PushAndGrow(this);
};

HoveredBlipUniqueId::~HoveredBlipUniqueId()
{
	Reset();
	sm_pAllElements.DeleteMatches(this);
}

void HoveredBlipUniqueId::Set(s32 newIndex)
{
	// do nothing if it's already the same
	if( newIndex == m_iUniqueId )
		return;

	// because multiple things may point to the same blip, check if it's safe to turn it off
	if( m_iUniqueId != INVALID_BLIP_ID && IsMyBlipUniqueToMe())
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_HOVERED_ON_PAUSEMAP, m_iUniqueId, false);
	}

	m_iUniqueId = newIndex;

	if( m_iUniqueId != INVALID_BLIP_ID )
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_HOVERED_ON_PAUSEMAP, m_iUniqueId, true);
	}
}

void HoveredBlipUniqueId::Reset()
{ 
	Set( INVALID_BLIP_ID ); 
}

bool HoveredBlipUniqueId::IsValid() const
{
	return m_iUniqueId != INVALID_BLIP_ID;
}

bool HoveredBlipUniqueId::IsMyBlipUniqueToMe() const
{
	for(const auto& p : sm_pAllElements)
	{
		if(p!=this && p->m_iUniqueId == m_iUniqueId )
			return false;
	}

	return true;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMapMenu::CMapMenu
// PURPOSE:	init mapscreen
/////////////////////////////////////////////////////////////////////////////////////
CMapMenu::CMapMenu(CMenuScreen& owner) : CScriptMenu(owner)
	, m_vBGPos(0.162f, 0.221f)
	, m_vBGSize(0.677f, 0.597f)
	, m_bLastClearedTitle(false)
	, m_bAtLeastOneLowDetailBlip(false)
	, m_bCheckFakedPauseMapPosForCursor(false)
	, m_bCheckGPSPauseMapPosForCursor(false)
	, m_iCurrentLegendItem(0)
	, m_iCurrentLegendItemIndex(0)
#if KEYBOARD_MOUSE_SUPPORT
	, m_vwrldMouseMovement(0.f, 0.f)
	, m_vscrLastMousePos(0.f, 0.f)
	, m_vscrMousePosOnClick(0.f, 0.f)
	, m_uFirstClickTime(0u)
	, m_fMouseWheelMovement(0.0f)
	, m_bClickedWithoutDragging(false)
	, m_bWasBlipDragging(false)
	, m_iBlipClickedOnUniqueId(INVALID_BLIP_ID)
#endif
#if RSG_PC
	, m_iRenderDelay(0)
#endif
#if __BANK
, m_pMyGroup(NULL)
#endif
{
#if KEYBOARD_MOUSE_SUPPORT && !__FINAL
	// only set this bool once
	if( PARAM_mapStaysOnWaypoints.Get() )
	{
		s_bMapMovesOnWaypoint = !PARAM_mapStaysOnWaypoints.Get();
		PARAM_mapStaysOnWaypoints.Set(NULL);
	}

	// only set this bool once
	if( PARAM_mapCentersOnClick.Get() )
	{
		s_bCenterOnSingleClick = !PARAM_mapCentersOnClick.Get();
		PARAM_mapCentersOnClick.Set(NULL);
	}
#endif

	owner.FindDynamicData("sp_path", m_SPPath);

	sm_iPreviousHoverActualId = INVALID_ACTUAL_ID;
	sm_iCurrentSelectedMissionCreatorUniqueId = INVALID_BLIP_ID;

	InitStartingValues();

	sm_vPauseMapOriginalPos = CMiniMap::GetPauseMapCursor();
	SetCursorActual(CMiniMap::GetPauseMapCursor());

	if (CMiniMap::GetIsInsideInterior(true))
	{
		sm_bPauseMapInInterior = true;
		sm_vPauseMapLastZoomedPos = CMiniMap::GetPauseMapCursor();
	}
	else
	{
		sm_bPauseMapInInterior = false;
		sm_vPauseMapLastZoomedPos.Set(0,0);
	}

	if (CMiniMap::IsInGolfMap())
	{
		sm_bPauseMapInGolf = true;
		sm_vPauseMapLastZoomedPos = CMiniMap::GetPauseMapCursor();
	}
	else
	{
		sm_bPauseMapInGolf = false;
		sm_vPauseMapLastZoomedPos.Set(0,0);
	}

	InitStartingValues();

	// we need the centre and north blips to be set up for pausemap as soon as the pausemap starts updating, so force through and update of
	// these particular blips here
	CMiniMap::UpdateCentreAndNorthBlips();

	sm_iUpdateLegendTimer = fwTimer::GetSystemTimeInMilliseconds() - __INTERVAL_BETWEEN_LEGEND_UPDATES;
	sm_iMissionCreatorHoverQueryTimer = fwTimer::GetSystemTimeInMilliseconds();

	sm_bCurrentlyZooming = true;
}

atString& CMapMenu::GetScriptPath()
{
	if(NetworkInterface::IsNetworkOpen())
		return m_ScriptPath;
	return m_SPPath;
}

void CMapMenu::Init()
{
	CScriptMenu::Init();

	CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP);

	if(const SGeneralPauseDataConfig* pData = CPauseMenu::GetMenuArray().GeneralData.MovieSettings.Access("MinimapBG"))
	{
		m_vBGPos =  pData->vPos;
		m_vBGSize = pData->vSize;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMapMenu::~CMapMenu()
// PURPOSE:	clears the legend
/////////////////////////////////////////////////////////////////////////////////////
CMapMenu::~CMapMenu()
{
	if (sm_bCurrentlyZooming)
	{
		g_FrontendAudioEntity.StopSoundMapZoom();
		sm_bCurrentlyZooming = false;
	}

#if __BANK
	if( m_pMyGroup )
		m_pMyGroup->Destroy();
#endif

	CMiniMap::ExitPauseMapState();

	m_bCheckFakedPauseMapPosForCursor = false;
	m_bCheckGPSPauseMapPosForCursor = false;
}

bool CMapMenu::IsDoneLoading() const
{
	return CMiniMap::AreAllTexturesActive(CMiniMap::GetBitmapForMinimap());
}


void CMapMenu::LoseFocus()
{
	uiDebugf3("CMapMenu::LoseFocus");

	//@@: location AHOOK_0067_CHK_LOCATION_D
	if (sm_bCurrentlyZooming)
	{
		g_FrontendAudioEntity.StopSoundMapZoom();
		sm_bCurrentlyZooming = false;
	}
	EmptyLegend(false);

	g_FrontendAudioEntity.StopSoundMapMovement();
	g_FrontendAudioEntity.StopSoundMapZoom();


	CScriptMenu::LoseFocus();

#if KEYBOARD_MOUSE_SUPPORT
	CMousePointer::SetMouseCursorVisible(true);

	m_fMouseWheelMovement = 0.0f;
	m_vwrldMouseMovement.Zero();
	m_iBlipMouseCurrentlyHoverOverUniqueId.Reset();
#endif // KEYBOARD_MOUSE_SUPPORT

	CPauseMenu::ScaleContentMovie(false);

	// prevent leaky blipping
	m_iBlipCurrentlyHoveredOverUniqueId.Reset();
}


void CMapMenu::InitStartingValues()
{
	sm_bCanStartMission = false;
	sm_bShowStartMissionButton = false;
	
	sm_bCanContact = false;
	sm_bShowContactButton = false;

	//
	// initial positions:
	//
	//setup starting positions:
	// But not if we're in an interior
	Vector2 vFakePlayerPos;
	if (!sm_bPauseMapInInterior && CScriptHud::GetFakedPauseMapPlayerPos(vFakePlayerPos))
	{
		CMiniMap::SetPauseMapCursor(vFakePlayerPos);
	}
	else
	{
		CPed *pLocalPlayer = CMiniMap::GetMiniMapPed();
		if (uiVerifyf(pLocalPlayer, "CMiniMap::SetMinimapModeState - failed to get a pointer to the local player"))
		{
			Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());
			CMiniMap::SetPauseMapCursor(Vector2(vPlayerPos.x, vPlayerPos.y));
		}
	}

	//
	// initial zoom levels as we go into the map menu
	//
	if (sm_bPauseMapInInterior)
	{
		SetMapZoom(ZOOM_LEVEL_INTERIOR, true);
	}
	else if (sm_bPauseMapInGolf)
	{
		SetMapZoom(ZOOM_LEVEL_GOLF_COURSE, true);				
	}
	else if (CMiniMap::GetInPrologue())
	{
		SetMapZoom(ZOOM_LEVEL_START_PROLOGUE, true);
	}
	else if (!NetworkInterface::IsGameInProgress())
	{
		SetMapZoom(ZOOM_LEVEL_START, true);
	}
	else
	{
		SetMapZoom(ZOOM_LEVEL_START_MP, true);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateInput
// PURPOSE:	updates the map screen input if navigating content
/////////////////////////////////////////////////////////////////////////////////////
bool CMapMenu::UpdateInput(s32 eInput)
{
	bool bResult = false;
	if (CPauseMenu::IsNavigatingContent())
	{
		if ( eInput == PAD_CIRCLE &&
			(SUIContexts::IsActive("CORONA_BIGMAP_CLOSE") || SUIContexts::IsActive("CORONA_BIGMAP_CLOSE_KM")) )
		{
			// Override the map exit input while we're in full-screen race corona map
			return true;
		}

		bResult = UpdateMapScreenInput(false, eInput);
	}
	else
	{
		UpdateZooming(0.0f, 0.0f, false);
#if KEYBOARD_MOUSE_SUPPORT
		CMousePointer::SetMouseCursorVisible(true);
#endif // KEYBOARD_MOUSE_SUPPORT
	}

	bResult = CScriptMenu::UpdateInput(eInput) || bResult;

	return bResult;
}


void CMapMenu::SetMapZoom(s32 iNewZoomLevel, bool bInstant)
{
	sm_iCurrentZoomLevel = iNewZoomLevel;

	if (bInstant)
	{
		CMiniMap::SetPauseMapScale(CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel));
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::GetCurrentSelectedMissionCreatorBlip
// PURPOSE:	returns and resets the script flag
/////////////////////////////////////////////////////////////////////////////////////
s32 CMapMenu::GetCurrentSelectedMissionCreatorBlip()
{
	s32 iReturnValue = INVALID_BLIP_ID;

	if (sm_iCurrentSelectedMissionCreatorUniqueId != INVALID_BLIP_ID)
	{
		if (sm_bHasQueried)
		{
			if (fwTimer::GetSystemTimeInMilliseconds() > sm_iMissionCreatorHoverQueryTimer + __TIME_TO_WAIT_FOR_RESPONSE_TO_SCRIPT_FOR_BLIP_HOVER)
			{
				sm_iMissionCreatorHoverQueryTimer = fwTimer::GetSystemTimeInMilliseconds();
				iReturnValue = sm_iCurrentSelectedMissionCreatorUniqueId;
				sm_iCurrentSelectedMissionCreatorUniqueId = INVALID_BLIP_ID;
				
				//ResetCornerBlipInfo(true);
				sm_bHasQueried = false;
			}
		}
		else
		{
			if (sm_iMissionCreatorHoverQueryTimer != 0)
			{
				sm_iMissionCreatorHoverQueryTimer = fwTimer::GetSystemTimeInMilliseconds();  // start timer on 1st check
			}
			else
			{
				sm_iMissionCreatorHoverQueryTimer = (fwTimer::GetSystemTimeInMilliseconds() - (s32)(floor(__TIME_TO_WAIT_FOR_RESPONSE_TO_SCRIPT_FOR_BLIP_HOVER * 0.5f)));
			}

			sm_bHasQueried = true;
		}
	}

	return iReturnValue;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateZooming
// PURPOSE:	updates the zoom to the required level.  Needs to be called outside
//			navigating content to allow the map to finish its zoom if the user
//			backs out mid-zoom
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::UpdateZooming(float fLeftShoulder, float fRightShoulder, bool bInitialEntry)
{
	bool bUpdateZoomDisplay = false;

	if (!(fRightShoulder > 0.0f && fLeftShoulder > 0.0f) && (sm_bCurrentlyZooming || fLeftShoulder > 0.0f || fRightShoulder > 0.0f))
	{
		if ( (!sm_bCurrentlyZooming) && (!sm_bPauseMapInInterior) && (!sm_bPauseMapInGolf) )
		{
			g_FrontendAudioEntity.PlaySoundMapZoom();

			sm_bCurrentlyZooming = true;
		}

		bUpdateZoomDisplay = (fLeftShoulder > 0.0f || fRightShoulder > 0.0f);

		if (fRightShoulder > 0.0f && CMiniMap::GetPauseMapScale() < CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel))
		{
			s32 iZoomLevelToUse = sm_iCurrentZoomLevel-1;

			s32 iMaxZoomLevel = MAX_ZOOMED_OUT;

			if (CMiniMap::GetInPrologue())
			{
				iMaxZoomLevel = MAX_ZOOMED_OUT_PROLOGUE;
			}

			if (iZoomLevelToUse < iMaxZoomLevel)
			{
				iZoomLevelToUse = iMaxZoomLevel;
			}
#if RSG_PC || RDR_VERSION
			// on PC, we'll just use these more properly tuned, better-feeling values
			float fPercentZoomed = Range(CMiniMap::GetPauseMapScale(), CMiniMap_Common::GetScaleFromZoomLevel(MAX_ZOOMED_OUT), CMiniMap_Common::GetScaleFromZoomLevel(MAX_ZOOMED_IN) );
			float fScaleIncrease = fRightShoulder * Lerp(SlowOut(fPercentZoomed), MOUSE_ZOOM_SPEED_FAR, MOUSE_ZOOM_SPEED_NEAR) * fwTimer::GetTimeStep_NonPausedNonScaledClipped();
#else
			float fScaleIncrease = CMiniMap_Common::GetZoomSpeedFromZoomLevel(iZoomLevelToUse, fRightShoulder);
#endif

			CMiniMap::SetPauseMapScale(CMiniMap::GetPauseMapScale() + fScaleIncrease);

			if ((sm_iCurrentZoomLevel == MAX_ZOOMED_IN && CMiniMap::GetPauseMapScale() > CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel)) || fRightShoulder > 1.0f)
			{
				CMiniMap::SetPauseMapScale(CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel));
				if(fRightShoulder > 1.0f)
				{
					bUpdateZoomDisplay = false;
				}
			}
		}
		else if (fLeftShoulder > 0.0f && CMiniMap::GetPauseMapScale() > CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel))
		{
			s32 iZoomLevelToUse = sm_iCurrentZoomLevel+1;

			if (iZoomLevelToUse > MAX_ZOOMED_IN)
			{
				iZoomLevelToUse = MAX_ZOOMED_IN;
			}

#if RSG_PC || RDR_VERSION
			// on PC, we'll just use these more properly tuned, better-feeling values
			float fPercentZoomed = Range(CMiniMap::GetPauseMapScale(), CMiniMap_Common::GetScaleFromZoomLevel(MAX_ZOOMED_OUT), CMiniMap_Common::GetScaleFromZoomLevel(MAX_ZOOMED_IN) );
			float fScaleDecrease = fLeftShoulder * Lerp(SlowOut(fPercentZoomed), MOUSE_ZOOM_SPEED_FAR, MOUSE_ZOOM_SPEED_NEAR) * fwTimer::GetTimeStep_NonPausedNonScaledClipped();
#else
			float fScaleDecrease = CMiniMap_Common::GetZoomSpeedFromZoomLevel(iZoomLevelToUse, fLeftShoulder);
#endif

			CMiniMap::SetPauseMapScale(CMiniMap::GetPauseMapScale() - fScaleDecrease);

			if ((sm_iCurrentZoomLevel == MAX_ZOOMED_OUT && CMiniMap::GetPauseMapScale() < CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel)) || fLeftShoulder > 1.0f)
			{
				CMiniMap::SetPauseMapScale(CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel));
				if(fLeftShoulder > 1.0f)
				{
					bUpdateZoomDisplay = false;
				}
			}
		}
		else
		{
			sm_bCurrentlyZooming = false;
		}
	}
		
	if (bUpdateZoomDisplay || bInitialEntry)
	{
		if (CPauseMenu::IsNavigatingContent())
		{
			const float scaleMeterSize = 2750.0f;
			float fFinalZoomValue = scaleMeterSize/CMiniMap::GetPauseMapScale();
			if(fFinalZoomValue != sm_fPrevZoomLevel || bInitialEntry)
			{
				char cZoomString[32];

				if (CFrontendStatsMgr::ShouldUseMetric())  // the conversion is already done at entry of menu
				{
					formatf(cZoomString, "%0.0fm", fFinalZoomValue);
				}
				else
				{
					formatf(cZoomString, "%0.0fft", METERS_TO_FEET(fFinalZoomValue));
				}

				// comment out the line below to debug the scale values!
	//				formatf(cZoomString, "%0.0fm (%0.2f  %0.2f)", fFinalZoomValue, fCurrentScaleValue, CMiniMap::GetPauseMapScale(), NELEM(cZoomString));

				CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_DESCRIPTION", 0,0, cZoomString);

				sm_fPrevZoomLevel = fFinalZoomValue;
			}
		}
	}

	if (!sm_bCurrentlyZooming)
	{
		g_FrontendAudioEntity.StopSoundMapZoom();
	}
}


void CMapMenu::DisplayAreaName(bool bForceUpdate)
{
	if ((sm_bPauseMapInInterior) || (sm_bPauseMapInGolf) || CMiniMap::GetInPrologue())
	{
		if( !m_bLastClearedTitle )
			CScaleformMenuHelper::SET_COLUMN_TITLE(PM_COLUMN_LEFT, "");
		m_bLastClearedTitle = true;
	}
	else
	{
		m_bLastClearedTitle = false;
		if (bForceUpdate || CUserDisplay::AreaName.IsNewName() || CUserDisplay::StreetName.IsNewName())
		{
			char GxtLocationName[MAX_ZONE_NAME_CHARS * 3];

			char const *pStreetName = &CUserDisplay::StreetName.GetForMap(CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y)[0];

			if (pStreetName[0] != '\0')
			{
				formatf(GxtLocationName, "%s / %s", &CUserDisplay::AreaName.GetForMap(CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y)[0], pStreetName, NELEM(GxtLocationName));
			}
			else
			{
				const Vector2 c_vMapCursor(CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y);
				const bool c_bCursorInIslandLocation = c_vMapCursor.x >= MAP_LIMIT_WEST_ISLAND || c_vMapCursor.y <= MAP_LIMIT_NORTH_ISLAND;
				const bool c_bIsOnIsland = CMiniMap::GetIsOnIslandMap();

				if(!c_bCursorInIslandLocation || c_bIsOnIsland)
				{
					formatf(GxtLocationName, "%s", &CUserDisplay::AreaName.GetForMap(CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y)[0], NELEM(GxtLocationName));
				}
				else
				{
					formatf(GxtLocationName, "%s", TheText.Get("Oceana"));
				}
			}

			CScaleformMenuHelper::SET_COLUMN_TITLE(PM_COLUMN_LEFT, GxtLocationName);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateMapScreenInput
// PURPOSE:	updates the map screen input
/////////////////////////////////////////////////////////////////////////////////////
bool CMapMenu::UpdateMapScreenInput(bool bInitialEntry, s32 eInput /* = PAD_NO_BUTTON_PRESSED */)
{
#define __STICK_THRESHOLD (0.1f)

	bool bScaleChanged = bInitialEntry;
	float fMapCrosshairMultiplier = 0.0f;
	bool bChangeZoomStyle = false;


	if( eInput == PAD_R3 )
	{
		if( SUIContexts::IsActive("MAP_VisHigh") || SUIContexts::IsActive("MAP_VisLow") )
		{
			// toggle it as we press the button:
			CMiniMap::SetBlipVisibilityHighDetail(!CMiniMap::GetBlipVisibilityHighDetail());

			g_FrontendAudioEntity.PlaySound("SELECT","HUD_FRONTEND_DEFAULT_SOUNDSET");  // fix for 1877020

			uiDebugf1("Updating pausemap legend because player toggled HIGH/LOW detail");

			CPauseMenu::UpdatePauseMapLegend();

			CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);
			return true;
		}
	}

	if (!sm_bPauseMapInInterior && !sm_bPauseMapInGolf)
	{
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_SELECT, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE))  // RB / R1
		{
			bChangeZoomStyle = true;

			if (CMiniMap::GetIsInsideInterior(true))
			{
				SetCursorActual(sm_vPauseMapLastZoomedPos);
				ResetCornerBlipInfo(false, false);
				SetMapZoom(ZOOM_LEVEL_INTERIOR, true);

				sm_bPauseMapInInterior = true;
				bScaleChanged = true;

				CPauseMenu::PlaySound("TOGGLE_ON");
			}

			if (CMiniMap::IsInGolfMap())
			{
				SetCursorActual(sm_vPauseMapLastZoomedPos);
				ResetCornerBlipInfo(false, false);
				SetMapZoom(ZOOM_LEVEL_GOLF_COURSE, true);

				sm_bPauseMapInGolf = true;
				bScaleChanged = true;

				CPauseMenu::PlaySound("TOGGLE_ON");
			}

			CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);  // repopulate any instructional buttons
			UpdatePauseMapLegend(true);

			// If you've toggled back to interior, ensure this flag is cleared
			m_bCheckFakedPauseMapPosForCursor = false; 
			m_bCheckGPSPauseMapPosForCursor = false;
		}
	}

	const CControl& controls = CControlMgr::GetMainFrontendControl();

	float fLeftShoulder = controls.GetFrontendLT().GetNorm();
	float fRightShoulder = controls.GetFrontendRT().GetNorm();

#if KEYBOARD_MOUSE_SUPPORT
	UpdateMouseWheelZoom(fLeftShoulder, fRightShoulder);
#endif // KEYBOARD_MOUSE_SUPPORT

	if (!bScaleChanged)
	{
		if ( (!bChangeZoomStyle) && (sm_bPauseMapInInterior || sm_bPauseMapInGolf) )
		{
			fMapCrosshairMultiplier = CMiniMap_Common::GetScrollSpeedFromZoomLevel(sm_iCurrentZoomLevel);

			if (CPauseMenu::CheckInput(FRONTEND_INPUT_SELECT, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE))  // RT / R1
			{
				CPauseMenu::PlaySound("TOGGLE_ON");

				if (CMiniMap::GetInPrologue())
				{
					SetMapZoom(ZOOM_LEVEL_START_PROLOGUE, true);
				}
				else
				{
					SetMapZoom(ZOOM_LEVEL_START, true);
				}

				sm_bPauseMapInInterior = false;
				sm_bPauseMapInGolf = false;
				bScaleChanged = true;

				sm_vPauseMapLastZoomedPos = CMiniMap::GetPauseMapCursor();

				Vector2 vFakePlayerPos;
				if (CScriptHud::GetFakedPauseMapPlayerPos(vFakePlayerPos))
				{
					SetCursorActual(vFakePlayerPos);
				}
			
				// If the player is in an interior where the real player position should be used but the location of the interior is in a faked area of the game-world (e.g. garage),
				// we need to toggle whether we're faking the player position depending on if we're viewing the interior or exterior.  This requires script to query when we've switched
				// back to the exterior map and causes a delay before we know to set the cursor position to the newly faked player position.  This flag allows us to query the faked position 
				// and set the map cursor the first frame script begins faking the player position
				m_bCheckFakedPauseMapPosForCursor = true;
				m_bCheckGPSPauseMapPosForCursor = true;

				CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);  // repopulate any instructional buttons
				UpdatePauseMapLegend(true);
			}
		}
		else
		{
			fMapCrosshairMultiplier = CMiniMap_Common::GetScrollSpeedFromZoomLevel(sm_iCurrentZoomLevel);

			if (fLeftShoulder > __STICK_THRESHOLD && fRightShoulder == 0.0f)
			{
				s32 iMaxZoomLevel = MAX_ZOOMED_OUT;

				if (CMiniMap::GetInPrologue())
				{
					iMaxZoomLevel = MAX_ZOOMED_OUT_PROLOGUE;
				}

				if (sm_iCurrentZoomLevel > iMaxZoomLevel)
				{
					if (CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel) >= CMiniMap::GetPauseMapScale())
					{
						SetMapZoom(sm_iCurrentZoomLevel - 1, false);
					}
				}
			}
			if (fRightShoulder > __STICK_THRESHOLD && fLeftShoulder == 0.0f)
			{
				if (sm_iCurrentZoomLevel < MAX_ZOOMED_IN)
				{
					if (CMiniMap_Common::GetScaleFromZoomLevel(sm_iCurrentZoomLevel) <= CMiniMap::GetPauseMapScale())
					{
						SetMapZoom(sm_iCurrentZoomLevel + 1, false);

						if (sm_iPreviousHoverActualId != INVALID_ACTUAL_ID)
						{
							CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(sm_iPreviousHoverActualId);
							if (pBlip)
							{
								Vector3 vBlipPos(CMiniMap::GetBlipPositionValue(pBlip));
								SetCursorActual(vBlipPos.x, vBlipPos.y);
								sm_vPreviousCursorPos = CMiniMap::GetPauseMapCursor();
							}
						}
					}
				}
			}

// #if __DEV
// 			uiDebugf1("Current Map Scale: Level %d   %0.2f   Pos: (%0.2f, %0.2f)   LR: %0.2f %0.2f", sm_iCurrentZoomLevel, CMiniMap::GetPauseMapScale(), CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y, iLeftShoulder, iRightShoulder);
// #endif // __DEV
		}
	}

	UpdateZooming(fLeftShoulder, fRightShoulder, bInitialEntry);
	
	Vector2 vOldPos = CMiniMap::GetPauseMapCursor();
	Vector2 vNewPos = vOldPos;
	
#if KEYBOARD_MOUSE_SUPPORT
	bool bUsedMouse = true;
	if( UpdateMouseClicksAndPosition(vNewPos) )
#endif // KEYBOARD_MOUSE_SUPPORT
	{
		KEYBOARD_MOUSE_ONLY( bUsedMouse = false; )

		Vector2 movement( controls.GetFrontendLeftRight().GetUnboundNorm()
						, controls.GetFrontendUpDown().GetUnboundNorm() );
		
		if( movement.Mag2() >= (__STICK_THRESHOLD * __STICK_THRESHOLD) )
		{
			vNewPos.x += (fMapCrosshairMultiplier) * movement.x;
			vNewPos.y -= (fMapCrosshairMultiplier) * movement.y;
		}
	}

	static Vector2 vInteriorBoundMin(0,0);
	static Vector2 vInteriorBoundMax(0,0);

	if ( (bScaleChanged) || (vInteriorBoundMin.IsZero() || vInteriorBoundMax.IsZero()) || (GetCurrentZoomLevel() > MAX_ZOOMED_IN) )
	{
		GetMapExtents(vInteriorBoundMin, vInteriorBoundMax, !bInitialEntry);
	}

	if(sm_bWasReloaded)
	{
		GetMapExtents(vInteriorBoundMin, vInteriorBoundMax, !bInitialEntry);
		sm_bWasReloaded = false;
	}

	vNewPos.x = Clamp( vNewPos.x, vInteriorBoundMin.x, vInteriorBoundMax.x );
	vNewPos.y = Clamp( vNewPos.y, vInteriorBoundMax.y, vInteriorBoundMin.y ); // min/max reversed because y is reversed

	if (vOldPos != vNewPos)
	{
#if KEYBOARD_MOUSE_SUPPORT
		if( bUsedMouse )
		{
			Vector2 vScreenSpace( GetScreenPosFromWorld(vOldPos.x - vNewPos.x, vOldPos.y - vNewPos.y, false) );
			float fSoundScalar = rage::RampValue(vScreenSpace.Mag2(), 0.0f, MOUSE_DRAG_SOUND_SCREEN_PIXELS_SQRD, 0.0f, MAX_SOUNDVAR_VALUE);
			g_FrontendAudioEntity.PlaySoundMapMovement(fSoundScalar);
			uiWarningf("MOVE: %f (%f)", fSoundScalar, vScreenSpace.Mag2());
		}
		else
#endif
		{
			g_FrontendAudioEntity.PlaySoundMapMovement(-1.0f);
		}
		SetCursorActual(vNewPos);
	}
	else
	{
		g_FrontendAudioEntity.StopSoundMapMovement();
	}

	if (!bInitialEntry)
	{
#define __STREET_NAME_BOUNDING_BOX_RANGE (20.0f)
		ThePaths.AddNodesRequiredRegionThisFrame( CPathFind::CPathNodeRequiredArea::CONTEXT_MAP, Vector3(vNewPos.x-__STREET_NAME_BOUNDING_BOX_RANGE, vNewPos.y-__STREET_NAME_BOUNDING_BOX_RANGE, -__STREET_NAME_BOUNDING_BOX_RANGE), Vector3(vNewPos.x+__STREET_NAME_BOUNDING_BOX_RANGE, vNewPos.y+__STREET_NAME_BOUNDING_BOX_RANGE, __STREET_NAME_BOUNDING_BOX_RANGE), "CMapMenu" );
	}

	//
	// placing a waypoint
	//
	if (!CScriptHud::bHideFrontendMapBlips)
	{
		if ( SUIContexts::IsActive("MAP_CanDropPOI") )
		{
			
			const ioValue& poiInput = CControlMgr::GetMainFrontendControl().GetValue(INPUT_MAP_POI);
			if(poiInput.IsReleased() || CPauseMenu::CheckInput(FRONTEND_INPUT_Y, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) )// Y/TRIANGLE button
			{
				bool bPlaySound = true;
#if KEYBOARD_MOUSE_SUPPORT
				// if you click the mouse button, place it where the mouse is
				if(poiInput.IsReleased() && poiInput.GetLastSource().m_Device == IOMS_MOUSE_BUTTON )
				{
					// only do the thing if you're over the map. Otherwise... don't
					if( IsMouseOverMap() )
					{
						Vector2 vWorldPos( GetWorldPosFromScreen((float)ioMouse::GetX(), (float)ioMouse::GetY()) );
						CMiniMap::AddOrRemovePointOfInterest(vWorldPos.x, vWorldPos.y);
					}
					else
					{
						bPlaySound = false;
					}
				}
				else
#endif
				{
					CMiniMap::AddOrRemovePointOfInterest(CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y);
					CheckCursorOverBlip(CMiniMap::GetPauseMapCursor(), true, false, true);
					sm_vPreviousCursorPos = -CMiniMap::GetPauseMapCursor();
				}

				if( bPlaySound )
				{
					g_FrontendAudioEntity.PlaySound("WAYPOINT_SET", "HUD_FRONTEND_DEFAULT_SOUNDSET");  // 1503220
					ResetCornerBlipInfo(true, false);
				}
			}
		}

		// place a waypoint GPS blip on the radar or warp if in mission creator:
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE)) // SQUARE/X button
		{
			PlaceWaypoint();
		}
	}

	static bool bLastInteriorFlagSentToActionscript = !(sm_bPauseMapInInterior || sm_bPauseMapInGolf);

	if (bLastInteriorFlagSentToActionscript != (sm_bPauseMapInInterior || sm_bPauseMapInGolf))
	{
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_FULL_MAP_SCREEN_CONTROL", !(sm_bPauseMapInInterior || sm_bPauseMapInGolf) );

		bLastInteriorFlagSentToActionscript = (sm_bPauseMapInInterior || sm_bPauseMapInGolf);
	}

	// debug stuff:
#if !__FINAL
#define DEBUG_WARP_BUTTON_TIME_TO_HOLD (1000)

	static u32 sDebugWarpButtonPressedTimer = fwTimer::GetSystemTimeInMilliseconds();
	static bool bDebugWarpButtonPressed = false;

	// useful debug stuff to drop the player in the current cursor position:
	if (CControlMgr::GetPlayerPad() && (CControlMgr::GetPlayerPad()->GetRightShoulder2() && CControlMgr::GetPlayerPad()->GetLeftShoulder2()))
	{	
		if (!bDebugWarpButtonPressed)
		{
			CPed* pPed = CGameWorld::FindLocalPlayer();
			if (pPed && (!pPed->GetIsInVehicle() || pPed->GetIsDrivingVehicle()))		// don't warp if player is a passenger
			{
				sDebugWarpButtonPressedTimer = fwTimer::GetSystemTimeInMilliseconds();
				bDebugWarpButtonPressed = true;
			}
		}
	}
	else
	{
		bDebugWarpButtonPressed = false;
	}

	XPARAM(nocheats);
	if (bDebugWarpButtonPressed && !PARAM_nocheats.Get() && fwTimer::GetSystemTimeInMilliseconds() > sDebugWarpButtonPressedTimer + DEBUG_WARP_BUTTON_TIME_TO_HOLD)
	{
		g_FrontendAudioEntity.PlaySound("WAYPOINT_SET", "HUD_FRONTEND_DEFAULT_SOUNDSET");

		if(!NetworkInterface::IsGameInProgress())
		{
			audNorthAudioEngine::MinimalUpdate();
		}
		CHelpMessage::ClearAll();

		sDebugWarpButtonPressedTimer = fwTimer::GetSystemTimeInMilliseconds();
		bDebugWarpButtonPressed = false;

		HANG_DETECT_SAVEZONE_ENTER();

		float fX = CMiniMap::GetPauseMapCursor().x;
		float fY = CMiniMap::GetPauseMapCursor().y;
		float fZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(fX, fY);
		Vector3 NewPlayerPos(fX, fY, fZ); 

		// Warp there
		gRenderThreadInterface.Flush();
		CWarpManager::SetWarp( NewPlayerPos, Vector3(0,0,0), 0.0f, true, true, -1.0f, true );

		if (NetworkInterface::IsGameInProgress())
		{
			GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_WARP));
		}

		HANG_DETECT_SAVEZONE_EXIT("CMapMenu::UpdateMapScreenInput");

		CPauseMenu::Close();
	}
#endif //  !__FINAL

	return false;
}

void CMapMenu::ReloadMap()
{
#if KEYBOARD_MOUSE_SUPPORT && !__FINAL
	// only set this bool once
	if( PARAM_mapStaysOnWaypoints.Get() )
	{
		s_bMapMovesOnWaypoint = !PARAM_mapStaysOnWaypoints.Get();
		PARAM_mapStaysOnWaypoints.Set(NULL);
	}

	// only set this bool once
	if( PARAM_mapCentersOnClick.Get() )
	{
		s_bCenterOnSingleClick = !PARAM_mapCentersOnClick.Get();
		PARAM_mapCentersOnClick.Set(NULL);
	}
#endif

	sm_iPreviousHoverActualId = INVALID_ACTUAL_ID;
	sm_iCurrentSelectedMissionCreatorUniqueId = INVALID_BLIP_ID;

	InitStartingValues();
	SetMapZoom(ZOOM_LEVEL_0, true);
	sm_bWasReloaded = true;

	if (CMiniMap::GetIsInsideInterior(true))
	{
		sm_bPauseMapInInterior = true;
		sm_vPauseMapLastZoomedPos = CMiniMap::GetPauseMapCursor();
	}
	else
	{
		sm_bPauseMapInInterior = false;
		sm_vPauseMapLastZoomedPos.Set(0,0);
	}

	if (CMiniMap::IsInGolfMap())
	{
		sm_bPauseMapInGolf = true;
		sm_vPauseMapLastZoomedPos = CMiniMap::GetPauseMapCursor();
	}
	else
	{
		sm_bPauseMapInGolf = false;
		sm_vPauseMapLastZoomedPos.Set(0,0);
	}

	InitStartingValues();
	CMiniMap::UpdateCentreAndNorthBlips();

	sm_iUpdateLegendTimer = fwTimer::GetSystemTimeInMilliseconds() - __INTERVAL_BETWEEN_LEGEND_UPDATES;
	sm_iMissionCreatorHoverQueryTimer = fwTimer::GetSystemTimeInMilliseconds();

	sm_bCurrentlyZooming = true;

	CPauseMenu::GenerateMenuData(MENU_UNIQUE_ID_MAP);
}

#if RSG_PC
void CMapMenu::DeviceReset()
{
	m_iRenderDelay = 3; // can't simply reset sm_bIsRenderingMask because the map itself has a delay it's going to smack us with
}
#endif

void CMapMenu::Render(const PauseMenuRenderDataExtra* renderData)
{
	if (CPauseMenu::IsNavigatingContent())
	{
		if (sm_bIsRenderingMask WIN32PC_ONLY( || (m_iRenderDelay && --m_iRenderDelay==0)) )
		{
			CMiniMap_RenderThread::SetMask(false);

			sm_bIsRenderingMask = false;
		}
	}
	else
	{
		if (!sm_bIsRenderingMask WIN32PC_ONLY( || (m_iRenderDelay && --m_iRenderDelay==0)) )
		{
			CMiniMap_RenderThread::SetMask(true);

			sm_bIsRenderingMask = true;
		}
	}

	//
	// render actual map:
	//
	Vector2 vBackgroundPos = m_vBGPos;
	Vector2 vBackgroundSize = m_vBGSize;
	if (CPauseMenu::IsNavigatingContent())
	{
		vBackgroundPos.Zero();
		vBackgroundSize.Set(1.0f, 1.0f);
	}
	else
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_CENTRE, &vBackgroundPos, &vBackgroundSize);
	
	CMiniMap_RenderThread::RenderPauseMap(vBackgroundPos, vBackgroundSize, renderData->fAlphaFader, renderData->fBlipAlphaFader, renderData->fSizeScalar);
	
	CScriptMenu::Render(renderData);
}



void CMapMenu::RenderPauseMapTint(const Vector2& vBackgroundPos, const Vector2& vBackgroundSize, float fAlpha, bool bBrightTint)  // 1354721
{
	eHUD_COLOURS hudColour = HUD_COLOUR_PAUSEMAP_TINT;

	// alternative shade of tint if navigating content (fixes 1481452)
	if (bBrightTint)
	{
		hudColour = HUD_COLOUR_PAUSEMAP_TINT_HALF; 
	}

	Color32 adjColor(CHudColour::GetRGBA(hudColour));

	// the color is stored 'inverted', so reverse it
	adjColor.SetAlpha( Clamp( int(float(255 - adjColor.GetAlpha()) * fAlpha), 0, 255) );

	CSprite2d::DrawRectGUI(fwRect(	vBackgroundPos.x,	vBackgroundPos.y,
		vBackgroundPos.x+vBackgroundSize.x,	vBackgroundPos.y+vBackgroundSize.y),
		adjColor);
}

bool CMapMenu::Populate(MenuScreenId newScreenId)
{
	CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP WIN32PC_ONLY(, true)); // on PC, just always force this because of resolution changes

	m_iCurrentLegendItem = 0;
	m_iCurrentLegendItemIndex = 0;

	CScriptMenu::Populate(newScreenId);

	return true;
}



/*void CMapMenu::ShowNextMissionObjective(bool bShow)
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);  // 948328

	if (bShow)
	{
		for (s32 iMessageNum = CMessages::GetMaxNumberOfPreviousMessages(CMessages::PREV_MESSAGE_TYPE_MISSION)-1; iMessageNum >= 0; iMessageNum--)
		{
			char GxtBriefMessage[MAX_CHARS_IN_MESSAGE];
			if (iMessageNum >= 0 && CMessages::FillStringWithPreviousMessage(CMessages::PREV_MESSAGE_TYPE_MISSION, iMessageNum, GxtBriefMessage, NELEM(GxtBriefMessage)))
			{
				if( pauseContent.BeginMethod("SET_COLUMN_SCROLL") )
				{
					pauseContent.AddParam(0);
					pauseContent.AddParam(0);
					pauseContent.AddParam(0);
					pauseContent.AddParam(0);
					pauseContent.AddParam(GxtBriefMessage);
					pauseContent.EndMethod();
				}

				return;
			}
		}
	}

	if( pauseContent.BeginMethod("SET_COLUMN_SCROLL") )
	{
		pauseContent.AddParam(0);
		pauseContent.AddParam(-1);
		pauseContent.AddParam(-1);
		pauseContent.AddParam(-1);
		pauseContent.EndMethod();
	}
}*/



void CMapMenu::OnNavigatingContent(bool bAreWe)
{
#define __ZOOM_WHEN_ACTIVE_INACTIVE (0.975f)  // 2.5% // 1496410

	// if on map screen and going out of navigating content, then clear the legend in the actionscript
	CMenuScreen& LegendData = CPauseMenu::GetScreenData(MENU_UNIQUE_ID_MAP_LEGEND);
	if (bAreWe)
	{
		CUserDisplay::AreaName.ForceSetToDisplay();
		CUserDisplay::DistrictName.ForceSetToDisplay();
		UpdateMapScreenInput(true);
		UpdatePauseMapLegend(false, true);  // want to restart the timer

		CMiniMap::SetPauseMapScale(CMiniMap::GetPauseMapScale() / __ZOOM_WHEN_ACTIVE_INACTIVE);
		CheckCursorOverBlip(CMiniMap::GetPauseMapCursor(), false, true, true);
		
#if KEYBOARD_MOUSE_SUPPORT
		m_vscrLastMousePos.Set(-1.0f, -1.0f); // adjust the mouse position so we'll refresh on enter
		Vector2 vNewPos( CMiniMap::GetPauseMapCursor() );
		UpdateMouseClicksAndPosition(vNewPos);
#endif
	}
	else
	{
		g_FrontendAudioEntity.StopSoundMapMovement();
		g_FrontendAudioEntity.StopSoundMapZoom();

		CScaleformMenuHelper::SET_COLUMN_TITLE(PM_COLUMN_LEFT, "");
		m_bLastClearedTitle = true;

		CMiniMap::SetPauseMapScale(CMiniMap::GetPauseMapScale() * __ZOOM_WHEN_ACTIVE_INACTIVE);

		CScaleformMenuHelper::HIDE_COLUMN_SCROLL( static_cast<PM_COLUMNS>(LegendData.depth-1));
		ClearMapOverlays();

#if KEYBOARD_MOUSE_SUPPORT
		m_fMouseWheelMovement = 0.0f;
		m_vwrldMouseMovement.Zero();
		CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);
		m_uFirstClickTime = 0u;
		m_bClickedWithoutDragging = false;
		m_bWasBlipDragging = false;
		m_iBlipMouseCurrentlyHoverOverUniqueId.Reset();
#endif
		m_iBlipCurrentlyHoveredOverUniqueId.Reset();
		sm_iPreviousHoverActualId = -1;
	}
	CPauseMenu::ScaleContentMovie(bAreWe);
}

bool CMapMenu::CanSetWaypoint()
{
	return (NetworkInterface::CanSetWaypoint(CGameWorld::FindLocalPlayer()) && (!CScriptHud::bUsingMissionCreator) && (!CMiniMap::IsInGolfMap()) && (!sm_bPauseMapInInterior) && !CMiniMap::GetInPrologue() && !CMiniMap::GetBlockWaypoint());
}

void CMapMenu::PlaceWaypoint(bool bAtMousePosition)
{
	if (!CScriptHud::bUsingMissionCreator)  // standard waypoint
	{
		SetWaypoint( bAtMousePosition );
	}
	else
	{
		if (CScriptHud::bAllowMissionCreatorWarp)
		{
			CPed* pPed = CGameWorld::FindLocalPlayer();
			if (pPed && (!pPed->GetIsInVehicle() || pPed->GetIsDrivingVehicle()))		// don't warp if player is a passenger
			{
				CPauseMenu::CloseAndWarp();
			}
		}
	}
}



void CMapMenu::PrepareInstructionalButtons( MenuScreenId UNUSED_PARAM(MenuId), s32 UNUSED_PARAM(iUniqueId) )
{
	// hide instructional buttons when certain categories in the legend are highlighted:
	bool bOnOffDisabled = false;
	if (m_iCurrentLegendItem >= 0 && m_iCurrentLegendItem < sm_LegendList.GetCount())
	{
		if (sm_LegendList[m_iCurrentLegendItem].iBlipCategory == BLIP_CATEGORY_WAYPOINT || sm_LegendList[m_iCurrentLegendItem].iBlipCategory == BLIP_CATEGORY_LOCAL_PLAYER)
		{
			bOnOffDisabled = true;
		}
	}

	bool bShowBlipDetailButtons = m_bAtLeastOneLowDetailBlip && 
		!CScriptHud::bUsingMissionCreator && 
		!CMapMenu::GetIsInInteriorMode() && 
		!CMiniMap::GetIsOnIslandMap() &&
		!CMapMenu::GetIsInGolfMode();

	SUIContexts::SetActive("MAP_CanWarp",  CScriptHud::bUsingMissionCreator && CScriptHud::bAllowMissionCreatorWarp );

	SUIContexts::SetActive("MAP_Waypoint", CanSetWaypoint());
	SUIContexts::SetActive("MAP_Destination", CScriptHud::bSetDestinationInMapMenu);
	SUIContexts::SetActive("MAP_CanZoom",  !sm_bPauseMapInInterior && !sm_bPauseMapInGolf);  // fix for 1809749
	SUIContexts::SetActive("MAP_CanStartMiss", sm_bShowStartMissionButton);
	SUIContexts::SetActive("MAP_CanContact", sm_bShowContactButton);
//	SUIContexts::SetActive("MAP_Criminal", NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInFreeMode() );
	SUIContexts::SetActive("MAP_Interior", CMiniMap::GetIsInsideInterior(true));
	SUIContexts::SetActive("MAP_Golf",	   CMiniMap::IsInGolfMap());
	SUIContexts::SetActive("MAP_VisHigh", bShowBlipDetailButtons && !CMiniMap::GetBlipVisibilityHighDetail() );
	SUIContexts::SetActive("MAP_VisLow",  bShowBlipDetailButtons &&  CMiniMap::GetBlipVisibilityHighDetail() );

	const bool bCanDropPOI = !CScriptHud::bUsingMissionCreator && 
		!sm_bPauseMapInInterior && 
		!sm_bPauseMapInGolf && 
		!CMiniMap::GetInPrologue() && 
		!CMiniMap::IsInGolfMap() && 
		!CMiniMap::GetIsOnIslandMap() &&
		!SUIContexts::IsActive("CORONA_BIGMAP_CLOSE") && 
		!SUIContexts::IsActive("CORONA_BIGMAP_CLOSE_KM");
	SUIContexts::SetActive("MAP_CanDropPOI",  bCanDropPOI);
}

bool CMapMenu::TriggerEvent(MenuScreenId MenuId, s32 iUniqueId)
{
	if (MenuId == MENU_UNIQUE_ID_MAP_LEGEND)
	{
//		CMapMenu::ProcessLegend(iUniqueId);
		return true;
	}

	return CScriptMenu::TriggerEvent(MenuId,iUniqueId);
}

void CMapMenu::LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir )
{
	if (CPauseMenu::IsNavigatingContent())
	{
		if (iNewLayout == MENU_UNIQUE_ID_MAP_LEGEND)
		{
			if (iUniqueId != -1)
			{
				ProcessLegend(iUniqueId, -1);
			}
		}

		CScriptMenu::LayoutChanged(iPreviousLayout, iNewLayout, iUniqueId, eDir);
	}

	CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);  // repopulate any instructional buttons as we need to check against legend items
}

bool CMapMenu::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	if (methodName == ATSTRINGHASH("SET_MAP_LOCATION",0x3800d093))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber(), "SET_MAP_LOCATION params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			s32 iUniqueId = (s32)args[1].GetNumber();
			s32 iSelectedValue = (s32)args[2].GetNumber();

			uiDebugf3("PauseMenu: Actionscript SET_MAP_LOCATION iUniqueId=%d iSelectedValue=%d", iUniqueId, iSelectedValue);

			ProcessLegend(iUniqueId, iSelectedValue);

			CPauseMenu::PlaySound("NAV_LEFT_RIGHT");
		}

		return true;
	}
	// normally occurs RIGHT after a layout changed, so we can use this to know if they've clicked the mouse on an item and can skip the delay
	else if(methodName == ATSTRINGHASH("SET_MOUSE_OVER_PARAMS",0x32e8715f))
	{
		SetCursorTarget(m_vCursorTarget, true);
	}

	return false;
}

void CMapMenu::Update()
{
	CWanted *pWanted = CGameWorld::FindLocalPlayerWanted();  // we want to flash the wanted blips even if we are paused
	if (pWanted)
	{
		CWanted::GetWantedBlips().CreateAndUpdateRadarBlips(pWanted);
	}

	// For now, we only care about catching the case where we go from non-faked player position interior, to faked-player position exterior
	if (!sm_bPauseMapInInterior)
	{
		if(m_bCheckFakedPauseMapPosForCursor)
		{
			Vector2 vFakePlayerPos;
			if(CScriptHud::GetFakedPauseMapPlayerPos(vFakePlayerPos))
			{
				CMiniMap::SetPauseMapCursor(vFakePlayerPos);
				m_bCheckFakedPauseMapPosForCursor = false;
			}
		}
		
		if(m_bCheckGPSPauseMapPosForCursor)
		{
			Vector3 vFakeGPSPos;
			if(CScriptHud::GetFakedGPSPlayerPos(vFakeGPSPos))
			{
				for(int i = 0; i < GPS_NUM_SLOTS; ++i)
				{
					if(CGps::m_Slots[i].IsOn())
					{
						CGps::m_Slots[i].SetStatus(CGpsSlot::eStatus::STATUS_CALCULATING_ROUTE);
					}
				}
				m_bCheckGPSPauseMapPosForCursor = false;
			}
		}
	}
	else
	{
		m_bCheckFakedPauseMapPosForCursor = false;
		m_bCheckGPSPauseMapPosForCursor = false;
	}

	if (sm_bUpdateLegend)
	{
		// we ensure the legend doesnt update every frame if blips are added/removed as it will jump about and look
		// shit and also slow down the map because of all the invokes
		if (fwTimer::GetSystemTimeInMilliseconds() > sm_iUpdateLegendTimer + __INTERVAL_BETWEEN_LEGEND_UPDATES)
		{
			UpdateLegend(false, false);
			sm_bUpdateLegend = false;

			sm_iUpdateLegendTimer = fwTimer::GetSystemTimeInMilliseconds();
		}
	}
#if KEYBOARD_MOUSE_SUPPORT
	// Handle Lerping pan position
	if( m_uCursorTargetTime )
	{
		Vector2 curPos = CMiniMap::GetPauseMapCursor();
		s32 timePassed = s32(fwTimer::GetSystemTimeInMilliseconds() - m_uCursorTargetTime);
		if( timePassed < MAP_SNAP_PAN_SPEED )
		{
			// timePassed may be negative, because the time is in a delay, so just clamp to 0 and hang out there for a bit
			float percentComplete = rage::Clamp(float(timePassed)/MAP_SNAP_PAN_SPEED, 0.0f, 1.0f);
			float percentThere = rage::TweenStar::ComputeEasedValue(percentComplete, static_cast<rage::TweenStar::EaseType>(s_LERP_METHOD) );
			CMiniMap::SetPauseMapCursor( Lerp(percentThere, curPos, m_vCursorTarget) );
		}
		else
		{
			SetCursorActual(m_vCursorTarget);
			DisplayAreaName(true);
		}
	}
	else
#endif
	if(CPauseMenu::IsNavigatingContent())
	{
		//
		// go through all the blips and see if we are hovering ontop of one
		//
		if ( WIN32PC_ONLY( CControlMgr::GetMainFrontendControl().GetCursorAccept().IsUp() && ) !CheckCursorOverBlip(CMiniMap::GetPauseMapCursor(), false, false, true))
		{
			DisplayAreaName(false);
		}
	}

	CScriptMenu::Update();
}

void CMapMenu::SetCursorTarget(const Vector2& vPosToLerpTo, bool KEYBOARD_MOUSE_ONLY(bSkipDelay))
{
#if KEYBOARD_MOUSE_SUPPORT
	m_vCursorTarget = vPosToLerpTo;

	// if the time is already set, dial it back a bit so we don't go anywhere for a bit
	// prevent bad-looking spamming
	int iDelay = bSkipDelay ? 0 : (m_uCursorTargetTime == 0 ? MAP_FIRST_PAN_DELAY : MAP_SPAM_PAN_DELAY);

	m_uCursorTargetTime = fwTimer::GetSystemTimeInMilliseconds() + iDelay;
#else
	// no lerping on console (for now)
	SetCursorActual(vPosToLerpTo);
#endif
}

void CMapMenu::SetCursorActual(const Vector2& vPosToSnapTo)
{
	CMiniMap::SetPauseMapCursor(vPosToSnapTo);
	m_vCursorTarget = vPosToSnapTo;
	m_uCursorTargetTime = 0u;
}

bool CMapMenu::ShouldBlipBeShownOnLegend(CMiniMapBlip *pBlip)
{
	if (!pBlip)
		return false;

	if(CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND))
	{
		return false;
	}

	if (CMiniMap::GetBlipVisibilityHighDetail() && !CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_HIGH_DETAIL))
	{
		return false;
	}

	return true;
}

Vector2 CMapMenu::GetWorldPosFromScreen(float x, float y, bool bIncludeTranslation) const
{
	GPointF screenSpace( x, y );
	GPointF worldSpace2D;
	if(bIncludeTranslation)
	{
		worldSpace2D = CMiniMap_RenderThread::GetLocalToScreenMatrix_UT().TransformByInverse(screenSpace);
	}
	else
	{
		GMatrix3D cloneMatrix;
		cloneMatrix.SetInverse( CMiniMap_RenderThread::GetLocalToScreenMatrix_UT() );
		// odd there's no SetTranslation
		cloneMatrix.SetX(0.f);
		cloneMatrix.SetY(0.f);
		cloneMatrix.SetZ(0.f);
		worldSpace2D = cloneMatrix.Transform(screenSpace);
	}

	return Vector2(worldSpace2D.x, -worldSpace2D.y); // -y because screen space and world space have different handedness
}

Vector2 CMapMenu::GetScreenPosFromWorld(float x, float y, bool bIncludeTranslation) const
{
	GPointF screenSpace( x, y );
	GPointF worldSpace2D;
	if(bIncludeTranslation)
	{
		worldSpace2D = CMiniMap_RenderThread::GetLocalToScreenMatrix_UT().Transform(screenSpace);
	}
	else
	{
		GMatrix3D cloneMatrix( CMiniMap_RenderThread::GetLocalToScreenMatrix_UT() );
		// odd there's no SetTranslation
		cloneMatrix.SetX(0.f);
		cloneMatrix.SetY(0.f);
		cloneMatrix.SetZ(0.f);
		worldSpace2D = cloneMatrix.Transform(screenSpace);
	}

	return Vector2(worldSpace2D.x, -worldSpace2D.y); // -y because screen space and world space have different handedness
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::CheckCursorOverBlip()
// PURPOSE:	checks whether we are hovering over a blip
/////////////////////////////////////////////////////////////////////////////////////
bool CMapMenu::CheckCursorOverBlip(Vector2 vCursorPos, bool bSnapToBlip, bool bForce, bool bHighlightOnLegend)
{
	s32 iCurrentBlipHoverActualId = INVALID_ACTUAL_ID;
	if (vCursorPos != sm_vPreviousCursorPos || bForce)
	{
		iCurrentBlipHoverActualId = FindActualBlipIdNearPos(vCursorPos);

		SetCurrentBlipHover(iCurrentBlipHoverActualId, bSnapToBlip, bHighlightOnLegend);

		sm_vPreviousCursorPos = vCursorPos;

		return (iCurrentBlipHoverActualId != INVALID_ACTUAL_ID);
	}

	return (sm_iPreviousHoverActualId != INVALID_ACTUAL_ID);
}

void CMapMenu::SetCurrentBlipHover(s32 iCurrentBlipHoverActualId, bool bSnapToBlip, bool bHighlightOnLegend)
{
	if (iCurrentBlipHoverActualId == INVALID_ACTUAL_ID)  // we didnt find any blip this time
	{
		if (sm_iPreviousHoverActualId != INVALID_ACTUAL_ID)
		{
			ResetCornerBlipInfo(sm_bCanStartMission || sm_bCanContact, false);
			sm_iPreviousHoverActualId = INVALID_ACTUAL_ID;
		}

		m_iBlipCurrentlyHoveredOverUniqueId.Reset();
	}
	else
	{
		if (iCurrentBlipHoverActualId != sm_iPreviousHoverActualId)  // we have a valid new current blip
		{
			CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(iCurrentBlipHoverActualId);
			if (pBlip)
			{
				m_iBlipCurrentlyHoveredOverUniqueId = CMiniMap::GetUniqueBlipUsed(pBlip);

				if (bSnapToBlip)
				{
					// if a new blip then snap to the centre of the blip:  This aids zooming into blips and general positioning of the cursor
					Vector3 vBlipPos(CMiniMap::GetBlipPositionValue(pBlip));
					SetCursorTarget(vBlipPos.x, vBlipPos.y, false);
				}

				if ( (CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_MISSION_CREATOR)) && (ShouldBlipBeShownOnLegend(pBlip)) )
				{
					if (sm_iPreviousHoverActualId == INVALID_ACTUAL_ID)
					{
						sm_iMissionCreatorHoverQueryTimer = fwTimer::GetSystemTimeInMilliseconds();
					}
					else
					{
						sm_iMissionCreatorHoverQueryTimer = 0;
					}
					
					sm_iCurrentSelectedMissionCreatorUniqueId = CMiniMap::GetUniqueBlipUsed(pBlip);
					
					if(NetworkInterface::IsGameInProgress())
					{
						sm_bCanStartMission = true;
						sm_bCanContact = true;
					}

					CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);

					sm_bHasQueried = false;
					// column 1 will now get populated by scripted based on the content of sm_iCurrentSelectedMissionCreatorBlipIndex;
				}
				else
				{
					sm_iCurrentSelectedMissionCreatorUniqueId = INVALID_BLIP_ID;
					sm_bCanStartMission = false;
					sm_bShowStartMissionButton = false;

					sm_bCanContact = false;
					sm_bShowContactButton = false;

					CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);

					ResetCornerBlipInfo(false, false);
				}

				if ( bHighlightOnLegend )
				{
					HighlightLegendItem(pBlip);
				}
			}

#if __BANK
			if (sm_iPreviousHoverActualId != INVALID_ACTUAL_ID)
			{
				CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(sm_iPreviousHoverActualId);
				if (pBlip)
				{
					if (CMiniMap::GetBlipDebugNumberValue(pBlip) == -1)
					{
						CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_REMOVE_LABEL);  // toggle it off
					}
				}
			}
#endif // __BANK

			sm_iPreviousHoverActualId = iCurrentBlipHoverActualId;
		}
	}
}

s32 CMapMenu::FindActualBlipIdNearPos(const Vector2& vCursorPos, bool bTightenRadius) const
{
	#define BLIP_OVERLAY_RADIUS (250.0f)
	float fBlipOverlayRadius = (BLIP_OVERLAY_RADIUS / CMiniMap::GetPauseMapScale());

	if (bTightenRadius)
	{
		fBlipOverlayRadius = 1.0f;
	}

	float fClosestMatch = (fBlipOverlayRadius*fBlipOverlayRadius); // plenty big size
	int iClosestIndex = INVALID_ACTUAL_ID;
	eBLIP_PRIORITY closestPriority = static_cast<eBLIP_PRIORITY>(BLIP_PRIORITY_LOW_SPECIAL-1);

	for (s32 iId = 0; iId < CMiniMap::GetMaxCreatedBlips(); ++iId)
	{
		if (iId == CMiniMap::GetActualSimpleBlipId())
			continue;

		CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(iId);
		if (!pBlip)
			continue;

		switch(CMiniMap::GetBlipDisplayValue(pBlip))
		{
		case BLIP_DISPLAY_PAUSEMAP:
		case BLIP_DISPLAY_PAUSEMAP_ZOOMED:
		case BLIP_DISPLAY_BOTH:
		case BLIP_DISPLAY_BLIPONLY:
		{
			if (ShouldBlipBeShownOnLegend(pBlip))
			{
				Vector2 vBlipPos;
				CMiniMap::GetBlipPositionValue(pBlip).GetVector2XY(vBlipPos);
				float fCurDist2 = vCursorPos.Dist2(vBlipPos);
				eBLIP_PRIORITY curPriority = CMiniMap::GetBlipPriorityValue(pBlip);
				// prefer higher priority blips if we can
				// equals so that later added blips of closer distance will win
				if( fCurDist2 < fClosestMatch || (fCurDist2 == fClosestMatch && curPriority >= closestPriority))
				{
					fClosestMatch = fCurDist2;
					iClosestIndex = iId;
					closestPriority = curPriority;
					// if the blip is the hovered blip, it ALWAYS wins ties (this fixes issues with scrolling along the list and losing highlight)
					if( pBlip->m_iUniqueId == m_iBlipCurrentlyHoveredOverUniqueId.Get())
						closestPriority = BLIP_PRIORITY_MAX;
				}
			}
			break;
		}
		default:
			// nothing happens
			break;
		}
	}

	return iClosestIndex;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SnapToBlip
// PURPOSE: snaps to any passed in blip
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::SnapToBlip(CMiniMapBlip *pBlip, bool bAllowedToLerp, bool bSetLegend)
{
	if (!pBlip)
		return;

	Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);
//	s32 iCategory = CMiniMap::GetBlipCategoryValue(pBlip);

	// work for 1595527...
	// work out how far this new blip is from the current map cursor position, as if over a certain distance we have to zoom out to avoid LOD being shown
	Vector2 deltaFromTargetXY = CMiniMap::GetPauseMapCursor()-Vector2(vBlipPos.x, vBlipPos.y);
	float fDistanceToNewBlip = deltaFromTargetXY.Mag2();

	//CMiniMap::SetPauseMapCursor(Vector2(vBlipPos.x, vBlipPos.y));
	if ( !CMiniMap::GetInPrologue() && !CMiniMap::GetIsOnIslandMap()
		&& !sm_bPauseMapInInterior && !sm_bPauseMapInGolf )
	{
#define __DISTANCE_TO_AUTO_ZOOM_OUT (800)

		// fix for 1720660 - even non-player blips will not auto-zoom out if they are close by, they only zoom the map out if far away
		if (fDistanceToNewBlip > __DISTANCE_TO_AUTO_ZOOM_OUT*__DISTANCE_TO_AUTO_ZOOM_OUT)
		{
			bAllowedToLerp = bAllowedToLerp && (sm_iCurrentZoomLevel == ZOOM_LEVEL_START);
			SetMapZoom(ZOOM_LEVEL_START, true);
		}
		sm_bPauseMapInInterior = false;
		sm_bPauseMapInGolf = false;
	}

	ResetCornerBlipInfo(false, true);

	if( bAllowedToLerp )
	{
		SetCursorTarget(vBlipPos.x, vBlipPos.y, false);
		SetCurrentBlipHover(CMiniMap::GetActualBlipUsed(pBlip), false, bSetLegend);
	}
	else
	{
		SetCurrentBlipHover(CMiniMap::GetActualBlipUsed(pBlip), true, bSetLegend);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdatePauseMapLegend
// PURPOSE: requests an update of the pausemap legend, either in the next update
//			based on a timer, or instantly
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::UpdatePauseMapLegend(bool bInstant, bool bRestartTimer)
{
	if (!CPauseMenu::IsActive())
		return;

	if (!CPauseMenu::IsNavigatingContent() || sm_bHideLegend)
		return;

	sm_bUpdateLegend = true;

	if (bInstant)
	{
		// reduce the timer by half to speed up the update, but not instant as otherwise we wont catch changes in the next
		// couple of frames, which we will want to catch
 		s32 iAlmostCompleteLegendUpdateTime = (s32)floor(__INTERVAL_BETWEEN_LEGEND_UPDATES * 0.5f);
 		sm_iUpdateLegendTimer = fwTimer::GetSystemTimeInMilliseconds() - iAlmostCompleteLegendUpdateTime;
	}

	if (bRestartTimer)
	{
		sm_iUpdateLegendTimer = fwTimer::GetSystemTimeInMilliseconds() - __INTERVAL_BETWEEN_LEGEND_UPDATES;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateLegendDisplayItem
// PURPOSE: Updates the SF display item associated with the current blip
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::UpdateLegendDisplayItem(CMiniMapBlip* pBlip)
{
	s32 iLegendItem, iBlipIndex;

	if(GetLegendItemData(pBlip, iLegendItem, iBlipIndex))
	{
		// Only update the legend display item if the currently showing item is the blip index that has changed
		if(sm_LegendList[iLegendItem].iCurrentSelected == iBlipIndex)
		{
			UpdateLegendDisplayItem(iLegendItem, iBlipIndex, false);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetCurrentLegendItem
// PURPOSE: Sets the current legend item and updates the SF display item at the specified legend and blip index
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::SetCurrentLegendItem(s32 iLegendItem, s32 iBlipIndex, bool bUpdateCurrent)
{
	if (sm_bHideLegend)
	{
		return;
	}

	m_iCurrentLegendItem = iLegendItem;
	m_iCurrentLegendItemIndex = iBlipIndex;
	UpdateLegendDisplayItem(iLegendItem, iBlipIndex, bUpdateCurrent);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateLegendDisplayItem
// PURPOSE: Updates the SF display item at the specified legend and blip index
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::UpdateLegendDisplayItem(s32 iLegendItem, s32 iActualId, bool bUpdateCurrent)
{
	if (iActualId != INVALID_ACTUAL_ID)
	{
		if (iLegendItem >= 0 && iLegendItem < sm_LegendList.GetCount())
		{
			if (uiVerifyf(iActualId < sm_LegendList[iLegendItem].blip.GetCount(), "PauseMap Legend: UpdateLegendDisplayItem tried to access a blip that is not in the list inside item %d (%s)", iLegendItem, sm_LegendList[iLegendItem].cBlipName))
			{
				s32 iUniqueBlipId = sm_LegendList[iLegendItem].blip[iActualId].iUniqueBlipId;

				CMiniMapBlip *pBlip = CMiniMap::GetBlip(iUniqueBlipId);

				if (pBlip)
				{
					uiDebugf1("PAUSEMAP: Updating slot in legend to Actionscript...");

					eOPTION_DISPLAY_STYLE blipStyle = static_cast<eOPTION_DISPLAY_STYLE>(sm_LegendList[iLegendItem].iBlipObjectId);

					if (sm_LegendList[iLegendItem].blip.GetCount() > 0)
					{
						if (pBlip)
						{
							if (bUpdateCurrent)
							{
								sm_LegendList[iLegendItem].iCurrentSelected = iActualId;
							}

							UpdateLegendSlotWithNameAndColour(&sm_LegendList[iLegendItem], pBlip, CMiniMap::GetBlipCategoryValue(pBlip));

							if( CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_LEFT, iLegendItem, MENU_UNIQUE_ID_MAP_LEGEND + PREF_OPTIONS_THRESHOLD, iLegendItem, blipStyle, sm_LegendList[iLegendItem].iCurrentSelected, true, sm_LegendList[iLegendItem].cBlipName, false, true))
							{
								CScaleformMgr::AddParamInt(sm_LegendList[iLegendItem].blipColour.GetRed());
								CScaleformMgr::AddParamInt(sm_LegendList[iLegendItem].blipColour.GetGreen());
								CScaleformMgr::AddParamInt(sm_LegendList[iLegendItem].blipColour.GetBlue());

								CScaleformMgr::AddParamString(sm_LegendList[iLegendItem].cBlipObjectName, false);
								CScaleformMgr::AddParamInt(sm_LegendList[iLegendItem].blip.GetCount());
								CScaleformMgr::AddParamBool(sm_LegendList[iLegendItem].bActive);
								CScaleformMgr::EndMethod();
							}
						}
					}
				}
			}
		}
	}
}

bool CMapMenu::GetLegendItemData(CMiniMapBlip *pBlip, int& iLegendItem, int& iBlipIndex)
{
	if(uiVerify(pBlip))
	{
		s32 iUniqueBlipId = CMiniMap::GetUniqueBlipUsed(pBlip);

		if (iUniqueBlipId != INVALID_BLIP_ID)
		{
			u8 iBlipCategory = CMiniMap::GetBlipCategoryValue(pBlip);

			// find blip in legend
			for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
			{
				if (sm_LegendList[iItemCount].iBlipCategory == iBlipCategory)  // only bother to check blips that are in the same category
				{
					for (s32 iBlipCount = 0; iBlipCount < sm_LegendList[iItemCount].blip.GetCount(); iBlipCount++)
					{
						if (sm_LegendList[iItemCount].blip[iBlipCount].iUniqueBlipId == iUniqueBlipId)
						{
							iLegendItem = iItemCount;
							iBlipIndex = iBlipCount;

							return true;
						}
					}
				}
			}
		}
	}
	
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::HighlightLegendItem
// PURPOSE: highlights a blip in the legend list
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::HighlightLegendItem(CMiniMapBlip *pBlip)
{
	if (!pBlip)
		return;

	s32 iLegendItem, iBlipIndex;

	if(GetLegendItemData(pBlip, iLegendItem, iBlipIndex))
	{
		SetCurrentLegendItem(iLegendItem, iBlipIndex, true);
		CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, iLegendItem);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::ResetCornerBlipInfo(bool bClear, bool bForceUpdateName)
{
	if (sm_iCurrentSelectedMissionCreatorUniqueId != INVALID_BLIP_ID || !CPauseMenu::IsNavigatingContent() || sm_bHideLegend)
	{
		bClear = true;
	}

	sm_iCurrentSelectedMissionCreatorUniqueId = INVALID_BLIP_ID;

	if (sm_bCanStartMission)  // 1587734 - only redraw the buttons if we were on a mission blip
	{
		sm_bCanStartMission = false;
		sm_bShowStartMissionButton = false;

		CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);
	}

	if(sm_bCanContact)
	{
		sm_bCanContact = false;
		sm_bShowContactButton = false;

		CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_MAP);
	}

	if ( (bClear) || (sm_bPauseMapInInterior) || (sm_bPauseMapInGolf) || CMiniMap::GetInPrologue())
	{
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SHOW_COLUMN", 1, false );
	}

	if (bForceUpdateName || !bClear)
	{
		DisplayAreaName(bForceUpdateName);
		UpdateZooming(0.0f, 0.0f, true);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::ProcessLegend
// PURPOSE: processes what blip to highlight from clicking on the legend
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::ProcessLegend(s32 iLegendItem, s32 iSelectedValue)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		sfAssertf(0, "CMapMenu::ProcessLegend can only be called on the UpdateThread!");
		return;
	}

	if (sm_bHideLegend)
	{
		return;		// Don't bother if we're set to not show the legend
	}

	// using the -1 to know when we change categories, and thus it's fine to lerp there
	bool bAllowedToLerp = (iSelectedValue == -1);

	if (sm_LegendList.GetCount() > 1 && iLegendItem >= 0 && iLegendItem < sm_LegendList.GetCount())  // check we have more than 1 item in the legend to move to (fixes) 1450856
	{
		if (sm_LegendList[iLegendItem].bActive)  // snap to the blip if its active
		{
			if (iSelectedValue >= 0 && iSelectedValue < sm_LegendList[iLegendItem].blip.GetCount())
			{
				sm_LegendList[iLegendItem].iCurrentSelected = iSelectedValue;
			}
			else
			{
				if (sm_LegendList[iLegendItem].iCurrentSelected >= 0 && sm_LegendList[iLegendItem].iCurrentSelected < sm_LegendList[iLegendItem].blip.GetCount())
				{
					iSelectedValue = sm_LegendList[iLegendItem].iCurrentSelected;
				}
				else
				{
					iSelectedValue = 0;  // this have gone bad so set it to 0
				}
			}

			if (uiVerifyf(iSelectedValue >= 0 && iSelectedValue < sm_LegendList[iLegendItem].blip.GetCount(), "PauseMap Legend: SET_MAP_LOCATION tried to access a blip that is not in the list inside item %d (%s)", iLegendItem, sm_LegendList[iLegendItem].cBlipName))
			{
				s32 iUniqueBlipId = sm_LegendList[iLegendItem].blip[iSelectedValue].iUniqueBlipId;

				CMiniMapBlip *pBlip = CMiniMap::GetBlip(iUniqueBlipId);

				if (pBlip)  // reposition and zoom out
				{
					SetCurrentLegendItem(iLegendItem, iSelectedValue, false);

					SnapToBlip(pBlip, bAllowedToLerp, false);
				}
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RemoveLegend
// PURPOSE: clears and inits the legend ready for new data or removing old data
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::RemoveLegend()
{
	EmptyLegend(true);

	sm_LegendList.Reset();
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::EmptyLegend
// PURPOSE: empties the legend of all blips but keeps its basic values there to
//			refill with updated blip info
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::EmptyLegend(bool bForceRemove)
{
	for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
	{
		if (sm_LegendList[iItemCount].bActive || bForceRemove)
		{
			sm_LegendList[iItemCount].blip.Reset();  // empty all stored blips from this item
		}
	}
}



int CompareBlipLegendSortFunc(const sMapLegendList* candidateA, const sMapLegendList* candidateB)
{
	// first compare categories
	if( candidateA->iBlipCategory != candidateB->iBlipCategory )
	{
		return ( (candidateA->iBlipCategory < candidateB->iBlipCategory) ? -1 : 1 );
	}

	// then label

	// the cast works here since we are just comparing strings and will work with Japanese type languages
	// since all we are interested in here is keeping the order correct rather than a diff check itself
	return strcmpi((char*)candidateA->cBlipName, (char*)candidateB->cBlipName) < 0 ? -1 : 1;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::ClearMapOverlays
// PURPOSE: clears the legend from actionscript
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::ClearMapOverlays()
{
	EmptyLegend(false);
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_LEFT);
	ResetCornerBlipInfo(true, false);
}



void CMapMenu::GetMapExtents(Vector2 &vMin, Vector2 &vMax, bool bUseLegendList)
{
#define __AMOUNT_AROUND_EXTENTS (20.0f)
#define __AMOUNT_AROUND_EXTENTS_GOLF (300.0f)

	bool bOverridingInterior = CMiniMap::IsOverridingInterior();

	if (!bOverridingInterior)
	{
		CPed *pLocalPlayer = CMiniMap::GetMiniMapPed();
		CPortalTracker* pPT = const_cast<CPortalTracker*>(pLocalPlayer ? pLocalPlayer->GetPortalTracker() : NULL);

		Vector3 vInteriorMin(0,0,0);
		Vector3 vInteriorMax(0,0,0);

		if (pPT && pPT->IsInsideInterior())  // need to check very basic interior test here to know whether we need to stream in the interior movie
		{
			CInteriorInst* pIntInst = pLocalPlayer->GetPortalTracker()->GetInteriorInst();
			if (pIntInst)
			{
				vInteriorMin = pIntInst->GetBoundingBoxMin();
				vInteriorMax = pIntInst->GetBoundingBoxMax();
			}
		}

		if (sm_bPauseMapInInterior)
		{
			if (vInteriorMin.IsNonZero() && vInteriorMax.IsNonZero())
			{
				vMin.y = vInteriorMax.y;
				vMax.y = vInteriorMin.y;
				vMax.x = vInteriorMax.x;
				vMin.x = vInteriorMin.x;
			}
			else
			{
				vMin.y = sm_vPauseMapOriginalPos.y + __AMOUNT_AROUND_EXTENTS;
				vMax.y = sm_vPauseMapOriginalPos.y - __AMOUNT_AROUND_EXTENTS;
				vMax.x = sm_vPauseMapOriginalPos.x + __AMOUNT_AROUND_EXTENTS;
				vMin.x = sm_vPauseMapOriginalPos.x - __AMOUNT_AROUND_EXTENTS;
			}
		}
		else if (sm_bPauseMapInGolf)
		{
			vMin.y = sm_vPauseMapOriginalPos.y + __AMOUNT_AROUND_EXTENTS_GOLF;
			vMax.y = sm_vPauseMapOriginalPos.y - __AMOUNT_AROUND_EXTENTS_GOLF;
			vMax.x = sm_vPauseMapOriginalPos.x + __AMOUNT_AROUND_EXTENTS_GOLF;
			vMin.x = sm_vPauseMapOriginalPos.x - __AMOUNT_AROUND_EXTENTS_GOLF;
		}
		else
		{
			if (CMiniMap::GetInPrologue())
			{
				vMin.y = MAP_LIMIT_NORTH_PROLOGUE;
				vMax.y = MAP_LIMIT_SOUTH_PROLOGUE;
				vMax.x = MAP_LIMIT_EAST_PROLOGUE;
				vMin.x = MAP_LIMIT_WEST_PROLOGUE;
			}
			else
			{
				if(CMiniMap::GetIsOnIslandMap())
				{
					vMin.y = MAP_LIMIT_NORTH_ISLAND;
					vMax.y = MAP_LIMIT_SOUTH_ISLAND;
					vMax.x = MAP_LIMIT_EAST_ISLAND;
					vMin.x = MAP_LIMIT_WEST_ISLAND;
				}
				else
				{
					vMin.y = MAP_LIMIT_NORTH;
					vMax.y = MAP_LIMIT_SOUTH;
					vMax.x = MAP_LIMIT_EAST;
					vMin.x = MAP_LIMIT_WEST;
				}
			}
		}
	}
	else
	{
		vMin.y = sm_vPauseMapOriginalPos.y + __AMOUNT_AROUND_EXTENTS;
		vMax.y = sm_vPauseMapOriginalPos.y - __AMOUNT_AROUND_EXTENTS;
		vMax.x = sm_vPauseMapOriginalPos.x + __AMOUNT_AROUND_EXTENTS;
		vMin.x = sm_vPauseMapOriginalPos.x - __AMOUNT_AROUND_EXTENTS;
	}

	if (bUseLegendList)  // use items in the legend list:
	{
		for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
		{
			CMiniMapBlip *pBlip = NULL;
			for (s32 iBlipCount = 0; iBlipCount < sm_LegendList[iItemCount].blip.GetCount(); iBlipCount++)
			{
				pBlip = CMiniMap::GetBlip(sm_LegendList[iItemCount].blip[iBlipCount].iUniqueBlipId);

				if (!pBlip)
					continue;

				Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);

				if (vBlipPos.x > vMax.x)
				{
					vMax.x = vBlipPos.x;
				}

				if (vBlipPos.x < vMin.x)
				{
					vMin.x = vBlipPos.x;
				}

				if (vBlipPos.y > vMin.y)
				{
					vMin.y = vBlipPos.y;
				}

				if (vBlipPos.y < vMax.y)
				{
					vMax.y = vBlipPos.y;
				}
			}
		}
	}
	else  // use blips that are 
	{
		if ( ( (!sm_bPauseMapInInterior) && (!sm_bPauseMapInGolf) ) || (bOverridingInterior) )
		{
			// go through all active blips and extend the limits if a blip is outwith the standard limits:
			for (s32 iId = 0; iId < CMiniMap::GetMaxCreatedBlips(); iId++)
			{
				if (iId == CMiniMap::GetActualSimpleBlipId())
					continue;

				CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(iId);

				if (!pBlip)
					continue;

				if ((CMiniMap::GetBlipDisplayValue(pBlip) == BLIP_DISPLAY_PAUSEMAP || CMiniMap::GetBlipDisplayValue(pBlip) == BLIP_DISPLAY_PAUSEMAP_ZOOMED || CMiniMap::GetBlipDisplayValue(pBlip) == BLIP_DISPLAY_BOTH || CMiniMap::GetBlipDisplayValue(pBlip) == BLIP_DISPLAY_BLIPONLY))
				{
					if (ShouldBlipBeShownOnLegend(pBlip))
					{
						Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);

						if (vBlipPos.x > vMax.x)
						{
							vMax.x = vBlipPos.x;
						}

						if (vBlipPos.x < vMin.x)
						{
							vMin.x = vBlipPos.x;
						}

						if (vBlipPos.y > vMin.y)
						{
							vMin.y = vBlipPos.y;
						}

						if (vBlipPos.y < vMax.y)
						{
							vMax.y = vBlipPos.y;
						}
					}
				}
			}
		}
	}
}

void CMapMenu::OnBlipDelete(CMiniMapBlip* pBlip)
{
	// clear all static things if it's a blip we care about.
	if( pBlip->m_iActualId == sm_iPreviousHoverActualId )
		sm_iPreviousHoverActualId = INVALID_ACTUAL_ID;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateLegend
// PURPOSE: updates the legend list in code with latest blip info
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::UpdateLegend(bool bClear, bool bForceSendToActionScript)
{
	if (!CPauseMenu::IsActive() || sm_bHideLegend)
		return;

	if ( (!bForceSendToActionScript) && (!CPauseMenu::IsNavigatingContent()) )
		return;

	if (!CPauseMenu::IsInMapScreen())
	{
		return;
	}

	if (!CPauseMenu::HasProcessedContent())
		return;

	atArray<sMapLegendList> PrevLegendList;

	if (!bForceSendToActionScript)
	{
		// make a copy of the legend array:
		for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
		{
			sMapLegendList newLegendItem;
			newLegendItem = sm_LegendList[iItemCount];
			PrevLegendList.PushAndGrow(newLegendItem);
		}
	}

	if (bClear)
	{
		RemoveLegend();
		uiDebugf1("PAUSEMAP: Legend fully cleared - ready to populate");
	}
	else
	{
		EmptyLegend(false);
		uiDebugf1("PAUSEMAP: Legend emptied of its blips - ready to populate");
	}

	m_bAtLeastOneLowDetailBlip = false;

	Vector2 vInteriorBoundMin(0,0);
	Vector2 vInteriorBoundMax(0,0);

	GetMapExtents(vInteriorBoundMin, vInteriorBoundMax, false);

	for (s32 iBlipIndex = 0; iBlipIndex < CMiniMap::GetMaxCreatedBlips(); iBlipIndex++)
	{
		if (iBlipIndex == CMiniMap::GetActualSimpleBlipId())
			continue;

		CMiniMapBlip *pBlip = CMiniMap::GetBlipFromActualId(iBlipIndex);

		if (pBlip)
		{
			// because not all blips may be shown on the legend, but may be toggled on/off, query this way up here.

			m_bAtLeastOneLowDetailBlip = m_bAtLeastOneLowDetailBlip || !CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_HIGH_DETAIL);

			s32 iThisBlipObjectId = CMiniMap::GetBlipObjectNameId(pBlip);
			s32 iThisBlipCategory = CMiniMap::GetBlipCategoryValue(pBlip);
			eBLIP_DISPLAY_TYPE iDisplayValue = CMiniMap::GetBlipDisplayValue(pBlip);

			// no POI when in golf (bug 1319054) or creator (1702753)
			if (CMiniMap::GetBlipObjectNameId(pBlip) == RADAR_TRACE_POI)
			{
				if ( (sm_bPauseMapInGolf) ||
					 (CScriptHud::bUsingMissionCreator) )
				{
					continue;  // skip this blip
				}
			}

			if (sm_bPauseMapInInterior || sm_bPauseMapInGolf)
			{
				bool bCheckBounds = (!vInteriorBoundMin.IsZero()) && (!vInteriorBoundMax.IsZero());

				if (bCheckBounds)
				{
					if (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_MISSION_CREATOR))  // fix for 1690613 - do not show any mission creator blips when inside interiors
					{
						continue;  // skip this blip
					}

					switch (CMiniMap::GetBlipTypeValue(pBlip))
					{
						case BLIP_TYPE_COORDS:  // FIX for 1463558 - want to show coord blips when fake interiors are active as these are probably "destinations" we still want to see
						{
							if (CScriptHud::FakeInterior.bActive && CMiniMap::GetBlipObjectNameId(pBlip) == BLIP_LEVEL)
							{
								bCheckBounds = false;
							}
							break;
						}

						case BLIP_TYPE_CHAR:  // allow any chars so we see them outside the interior
						{
							bCheckBounds = false;
							break;
						}

						default:
						{
							bCheckBounds = true;
						}
					}

					if (bCheckBounds)
					{
						float fAdditionalRangeForCertainBlipTypes = 0.0f;  // cull all other blips (apart from some below which have larger range)

						if (CMiniMap::GetBlipTypeValue(pBlip) == BLIP_TYPE_CAR)  // extend the range of where we show car blips
						{
							fAdditionalRangeForCertainBlipTypes = 30.0f;
						}

						Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip); // Stay out of the vector registers
						if (vBlipPos.GetX() < (vInteriorBoundMin.x-fAdditionalRangeForCertainBlipTypes) ||
							vBlipPos.GetX() > (vInteriorBoundMax.x+fAdditionalRangeForCertainBlipTypes) ||
							vBlipPos.GetY() < (vInteriorBoundMax.y-fAdditionalRangeForCertainBlipTypes) ||
							vBlipPos.GetY() > (vInteriorBoundMin.y+fAdditionalRangeForCertainBlipTypes))
						{
							continue;
						}
					}
				}
			}

			if ( (ShouldBlipBeShownOnLegend(pBlip) && (CMiniMap::GetBlipAlphaValue(pBlip) != 0) && (iDisplayValue != BLIP_DISPLAY_NEITHER) ) )
			{
				if (iDisplayValue == BLIP_DISPLAY_BLIPONLY || iDisplayValue == BLIP_DISPLAY_PAUSEMAP || iDisplayValue == BLIP_DISPLAY_BOTH || iDisplayValue == BLIP_DISPLAY_PAUSEMAP_ZOOMED)
				{
					bool bFound = false;

					for (s32 iItemCount = 0; ((!bFound) && (iItemCount < sm_LegendList.GetCount())); iItemCount++)
					{
						Color32 newBlipColour;
						if (CMiniMap::GetBlipColourValue(pBlip) != BLIP_COLOUR_USE_COLOUR32)
						{
							newBlipColour = CMiniMap_Common::GetColourFromBlipSettings(CMiniMap::GetBlipColourValue(pBlip), CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_BRIGHTNESS));
						}
						else
						{
							newBlipColour = CMiniMap::GetBlipColour32Value(pBlip);
						}

						// if same blip id AND same colour AND same name then use in same place (treat different coloured blips as different items, but only check RGB - ignore different alphas)
						if ( (((!bFound) && (((IsBlipInGroupedCategory(iThisBlipCategory)) && sm_LegendList[iItemCount].iBlipCategory == iThisBlipCategory))) || ((!strcmp((char*)sm_LegendList[iItemCount].cBlipName, (char*)CMiniMap::GetBlipNameValue(pBlip))) && (sm_LegendList[iItemCount].iBlipObjectId == iThisBlipObjectId) && (VEC3V_TO_VECTOR3(sm_LegendList[iItemCount].blipColour.GetRGB()) == VEC3V_TO_VECTOR3(newBlipColour.GetRGB())))) )
						{
							// remove any dead blips in this list:
							for (s32 i = 0; i < sm_LegendList[iItemCount].blip.GetCount(); i++)
							{
								if (!CMiniMap::GetBlip(sm_LegendList[iItemCount].blip[i].iUniqueBlipId))
								{
									sm_LegendList[iItemCount].blip.DeleteFast(i);
								}
							}
							
							if (sm_LegendList[iItemCount].bActive)  // only actually add it if its an active item - if its inactive, we skip it and when its flagged as active again the legend is re-populated
							{
								sMapLegendItem newLegendBlipItem;
								newLegendBlipItem.iUniqueBlipId = CMiniMap::GetUniqueBlipUsed(pBlip);

								if (sm_LegendList[iItemCount].iCurrentSelected != 0 || sm_LegendList[iItemCount].blip.GetCount() == 0) // fix for 1476383 - only re-update this legend slot if the item is not 0 - also fix for 1693594
								{
									UpdateLegendSlotWithNameAndColour(&sm_LegendList[iItemCount], pBlip, iThisBlipCategory);
								}

								uiDebugf1("PAUSEMAP: Adding blip %d (%d) to %d %s (colour %d,%d,%d) on legend (x%d)", iBlipIndex, newLegendBlipItem.iUniqueBlipId, iItemCount, sm_LegendList[iItemCount].cBlipName, sm_LegendList[iItemCount].blipColour.GetRed(), sm_LegendList[iItemCount].blipColour.GetGreen(), sm_LegendList[iItemCount].blipColour.GetBlue(), sm_LegendList[iItemCount].blip.GetCount()+1);

								sm_LegendList[iItemCount].blip.PushAndGrow(newLegendBlipItem);
							}

							bFound = true;
						}
					}

					if (!bFound)  // not added to a previous item, so create a new one
					{
						sMapLegendList *newLegendItem = &sm_LegendList.Grow();

						newLegendItem->bActive = true;

						sMapLegendItem newLegendBlipItem;
						newLegendBlipItem.iUniqueBlipId = CMiniMap::GetUniqueBlipUsed(pBlip);

						newLegendItem->blip.Reset();
						newLegendItem->blip.PushAndGrow(newLegendBlipItem);

						UpdateLegendSlotWithNameAndColour(newLegendItem, pBlip, iThisBlipCategory);

						newLegendItem->iBlipCategory = CMiniMap::GetBlipCategoryValue(pBlip);
						newLegendItem->iCurrentSelected = 0;

						uiDebugf1("PAUSEMAP: Creating blip %d (%d) to %d %s (colour %d,%d,%d) on legend in category %u", iBlipIndex, newLegendBlipItem.iUniqueBlipId, sm_LegendList.GetCount()-1, newLegendItem->cBlipName, newLegendItem->blipColour.GetRed(), newLegendItem->blipColour.GetGreen(), newLegendItem->blipColour.GetBlue(), newLegendItem->iBlipCategory);
					}

					if (sm_LegendList.GetCount() >= MAX_NUMBER_OF_LEGEND_ITEMS)  // ensure we cap this list at a reasonable amount
					{
						uiAssertf(0, "Max number of unique legend items added (%d)", MAX_NUMBER_OF_LEGEND_ITEMS);

						break;
					}
				}
			}
		}
	}

	// remove any dupes
	for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
	{
		if (sm_LegendList[iItemCount].blip.GetCount() > 0)
		{
			for (s32 iItemCount2 = 0; iItemCount2 < sm_LegendList.GetCount(); iItemCount2++)
			{
				if (iItemCount == iItemCount2)
					continue;

				if (sm_LegendList[iItemCount2].blip.GetCount() > 0)
				{
					if (sm_LegendList[iItemCount].blip[0].iUniqueBlipId == sm_LegendList[iItemCount2].blip[0].iUniqueBlipId)
					{
						uiDebugf1("PAUSEMAP: Removing item %d %s - unique %d as it is a dupe of item %d %s - unique %d", iItemCount2, sm_LegendList[iItemCount2].cBlipName, sm_LegendList[iItemCount2].blip[0].iUniqueBlipId, iItemCount, sm_LegendList[iItemCount].cBlipName, sm_LegendList[iItemCount].blip[0].iUniqueBlipId);
						sm_LegendList.DeleteFast(iItemCount2);
						iItemCount2 = 0;

						// update the name of the 1st item again
						s32 iUniqueBlipId = sm_LegendList[iItemCount].blip[0].iUniqueBlipId;
						CMiniMapBlip *pBlip = CMiniMap::GetBlip(iUniqueBlipId);
						UpdateLegendSlotWithNameAndColour(&sm_LegendList[iItemCount], pBlip, CMiniMap::GetBlipCategoryValue(pBlip));
					}
				}
			}
		}
	}

	// finally remove any blips that are no longer there 
	for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
	{
// 		bool bHideInactiveBlips = false;
 		CMiniMapBlip *pBlip = NULL;
 		if (sm_LegendList[iItemCount].blip.GetCount() > 0)
 		{
 			pBlip = CMiniMap::GetBlip(sm_LegendList[iItemCount].blip[0].iUniqueBlipId);
// 			bHideInactiveBlips = ( (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_HIDDEN_BY_LEGEND)) && (sm_bPauseMapInGolf || sm_bPauseMapInInterior) );
 		}

		if ( (sm_LegendList[iItemCount].blip.GetCount() <= 0) || (!pBlip)/* || bHideInactiveBlips*/)
		{
#if !__NO_OUTPUT
			s32 iCurrentArraySize = sm_LegendList.GetCount();
#endif  // #if !__NO_OUTPUT

			sm_LegendList[iItemCount].blip.Reset();
			sm_LegendList.DeleteFast(iItemCount);

#if !__NO_OUTPUT
			s32 iNewArraySize = sm_LegendList.GetCount();
			uiDebugf1("PAUSEMAP: Deleting item %d from legend as it no longer has any blips in the slot (size %d now %d)", iItemCount, iCurrentArraySize, iNewArraySize);
#endif // #if !__NO_OUTPUT

			iItemCount = 0;
		}
	}

	// sort the list in alphabetical order:  - this is important as we need to keep the correct
	if (sm_LegendList.GetCount() > 1)
	{
		sm_LegendList.QSort(0, -1, CompareBlipLegendSortFunc);
	}

	// ensure current selected is not higher than the count
	for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
	{
		if (sm_LegendList[iItemCount].iCurrentSelected >= sm_LegendList[iItemCount].blip.GetCount())
			sm_LegendList[iItemCount].iCurrentSelected = sm_LegendList[iItemCount].blip.GetCount()-1;
	}

	uiDebugf1("PAUSEMAP: FINAL LEGEND ITEMS...");
	for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
	{
		uiDebugf1("PAUSEMAP:    Legend item: %d - %d %s (number of blips %d) Selected: %d   Category: %d", iItemCount, sm_LegendList[iItemCount].iBlipObjectId, sm_LegendList[iItemCount].cBlipName, sm_LegendList[iItemCount].blip.GetCount(), sm_LegendList[iItemCount].iCurrentSelected, sm_LegendList[iItemCount].iBlipCategory);
	}	
	uiDebugf1("PAUSEMAP: ...END FINAL LEGEND ITEMS");

	uiDebugf1("PAUSEMAP: legend fully populated");

	
	// attempt to find new index close to where we were
	while(m_iCurrentLegendItem > 0) // no point checking if we're at the 0th element already
	{
		const sMapLegendList& prevLegendList = PrevLegendList[m_iCurrentLegendItem];
		for (s32 iPrevItemIndex = 0; iPrevItemIndex < prevLegendList.blip.GetCount(); iPrevItemIndex++)
		{
			// doing this the first time so we can use the same loop and check if the exact same blip still exists
			// failing that, we'll just check the rest in that same blip legend
			s32 iCheckItemIndex = (iPrevItemIndex + m_iCurrentLegendItemIndex) % prevLegendList.blip.GetCount();

			if( CMiniMapBlip* pBlip = CMiniMap::GetBlip(prevLegendList.blip[iCheckItemIndex].iUniqueBlipId) )
			{
				// see if that blip is in the new legend somewhere
				if( GetLegendItemData( pBlip, m_iCurrentLegendItem, m_iCurrentLegendItemIndex) )
				{
					uiDebugf1("PAUSEMAP: New current legend item changed to %d[%d]", m_iCurrentLegendItem, m_iCurrentLegendItemIndex);
					goto OutOfTheNestedLoop;
				}
			}
			// failed the first iteration--this is invalid now.
			sm_iPreviousHoverActualId = INVALID_ACTUAL_ID;
		}
		--m_iCurrentLegendItem;
		m_iCurrentLegendItemIndex = 0;
	}

	uiAssertf(m_iCurrentLegendItem==0, "Not sure how the loop failed... we should've iterated all the way back to 0");
	OutOfTheNestedLoop:

	// if current legend item is now over the total amount of items in the legend after any deletion, then reduce by 1
	// Pretty sure with the new loops above, we'll never encounter this
	if (m_iCurrentLegendItem > 0 && m_iCurrentLegendItem >= sm_LegendList.GetCount())
	{
		m_iCurrentLegendItem = sm_LegendList.GetCount()-1;
		m_iCurrentLegendItemIndex = 0;
	}

	bool bSendLegendContentsToActionScript = true;

	if (!bForceSendToActionScript)
	{
		if ( (sm_LegendList.GetCount() == PrevLegendList.GetCount()) && (PrevLegendList.GetCount() > 0) )
		{
			bool bFoundDiff = false;

			for (s32 iItemCount = 0; (!bFoundDiff) && iItemCount < sm_LegendList.GetCount() && iItemCount < PrevLegendList.GetCount(); iItemCount++)
			{
				if ( (sm_LegendList[iItemCount].blip.GetCount() != PrevLegendList[iItemCount].blip.GetCount()) ||
					 (sm_LegendList[iItemCount].iBlipObjectId != PrevLegendList[iItemCount].iBlipObjectId) )
				{
					bFoundDiff = true;
				}
			}

			if (!bFoundDiff)
			{
				uiDebugf1("PAUSEMAP: Legend repopulated but detected no change, so will not update actionscript this time");
				bSendLegendContentsToActionScript = false;
			}
		}
	}

	if (bSendLegendContentsToActionScript)
	{
		SendLegendToActionscript();

		if (bForceSendToActionScript)
		{
			CheckCursorOverBlip(CMiniMap::GetPauseMapCursor(), true, true, true);
		}
	}

	if (sm_LegendList.GetCount() == 0)  // reset it to 0 so we start afresh if/when new blips appear
	{
		m_iCurrentLegendItem = 0;
		m_iCurrentLegendItemIndex = 0;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsBlipInGroupedCategory
// PURPOSE: returns whether this is a category where all blips are grouped together
//			in the legend
/////////////////////////////////////////////////////////////////////////////////////
bool CMapMenu::IsBlipInGroupedCategory(s32 iThisBlipCategory)
{
	return (iThisBlipCategory == BLIP_CATEGORY_PLAYER || iThisBlipCategory == BLIP_CATEGORY_PROPERTY || iThisBlipCategory == BLIP_CATEGORY_APARTMENT);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SendLegendToActionscript
// PURPOSE: sends the legend to actionscript
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::SendLegendToActionscript()
{
	uiDebugf1("PAUSEMAP: Sending Legend to Actionscript...");

	CMiniMapBlip *pBlip = NULL;

	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_LEFT);

	if (sm_LegendList.GetCount() > 0)
	{
		s32 iItemIndex = 0;  // ensures index remains intact even if we assert and ignore (so assert becomes ignorable)
		for (s32 iItemCount = 0; iItemCount < sm_LegendList.GetCount(); iItemCount++)
		{
	/*#define __SHOW_CATEGORY (0)
	#if __SHOW_CATEGORY
			char cCategoryTemp[6];

			formatf(cCategoryTemp, " [%d]", sm_LegendList[iItemCount].iBlipCategory, NELEM(cCategoryTemp));
			cGxtLegendName = CTextConversion::charStrcat(cGxtLegendName, (char*)cCategoryTemp);
	#endif // __SHOW_CATEGORY*/

			eOPTION_DISPLAY_STYLE blipStyle = static_cast<eOPTION_DISPLAY_STYLE>(sm_LegendList[iItemCount].iBlipObjectId);

			pBlip = NULL;

			if (sm_LegendList[iItemCount].blip.GetCount() > 0)
			{
				int iUniqueBlipID = sm_LegendList[iItemCount].blip[0].iUniqueBlipId;
				pBlip = CMiniMap::GetBlip(iUniqueBlipID);
			
				if (uiVerifyf(pBlip, "sm_LegendList has invalid blip entry. Index: %d, ID: %d", iItemCount, iUniqueBlipID))
				{
					if( CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_LEFT, iItemIndex, MENU_UNIQUE_ID_MAP_LEGEND + PREF_OPTIONS_THRESHOLD, iItemIndex, blipStyle, sm_LegendList[iItemCount].iCurrentSelected, true, sm_LegendList[iItemCount].cBlipName, false))
					{
						CScaleformMgr::AddParamInt(sm_LegendList[iItemCount].blipColour.GetRed());
						CScaleformMgr::AddParamInt(sm_LegendList[iItemCount].blipColour.GetGreen());
						CScaleformMgr::AddParamInt(sm_LegendList[iItemCount].blipColour.GetBlue());
						CScaleformMgr::AddParamString(sm_LegendList[iItemCount].cBlipObjectName, false);
						CScaleformMgr::AddParamInt(sm_LegendList[iItemCount].blip.GetCount());
						CScaleformMgr::AddParamBool(sm_LegendList[iItemCount].bActive);
						CScaleformMgr::EndMethod();

						iItemIndex++;
					}
				}
			}
		}


		if (uiVerifyf(m_iCurrentLegendItem < sm_LegendList.GetCount(), "CMiniMap::SendLegendToActionscript - sm_iCurrentLegendItem is higher than sm_LegendList.GetCount()"))
		{
			if (sm_LegendList[m_iCurrentLegendItem].blip.GetCount() > 0 
				&& uiVerifyf(m_iCurrentLegendItemIndex < sm_LegendList[m_iCurrentLegendItem].blip.GetCount(), "CMiniMap::SendLegendToActionscript - m_iCurrentLegendItemIndex is higher than sm_LegendList[].blip.GetCount()" )
				)
			{
				pBlip = CMiniMap::GetBlip(sm_LegendList[m_iCurrentLegendItem].blip[m_iCurrentLegendItemIndex].iUniqueBlipId);
			}
		}
	}

	CScaleformMenuHelper::DISPLAY_DATA_SLOT(PM_COLUMN_LEFT);

	if (sm_LegendList.GetCount() > 0 && pBlip)
	{
		HighlightLegendItem(pBlip);
	}

	uiDebugf1("PAUSEMAP: ...Finished sending Legend to Actionscript");
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateLegendSlotWithNameAndColour
// PURPOSE: sets up colour and blip name to the legend item passed in
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::UpdateLegendSlotWithNameAndColour(sMapLegendList *pLegendItem, CMiniMapBlip *pBlip, s32 iThisBlipCategory)
{
	if (!pLegendItem)
		return;

	if (!pBlip)
		return;

	bool bCentreBlip = (CMiniMap::GetUniqueBlipUsed(pBlip) == CMiniMap::GetUniqueCentreBlipId());

	if (pLegendItem->bActive)  // only actually add it if its an active item - if its inactive, we skip it and when its flagged as active again the legend is re-populated
	{
		char cUpdatedString[MAX_BLIP_NAME_SIZE * 2];

		switch (iThisBlipCategory)
		{
			case BLIP_CATEGORY_PLAYER:
			{
				s32 iDistToOtherPlayer = 0;

				CEntity* pEntity = CMiniMap::FindBlipEntity(pBlip);
				CVehicle* pLocalPlayerVehicle = CGameWorld::FindLocalPlayerVehicle();
			
				// If we're in the same vehicle, keep distance 0
				bool bInSameVehicle = pLocalPlayerVehicle != NULL && pEntity && pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->GetVehiclePedInside() == pLocalPlayerVehicle;
				if (!bInSameVehicle)
				{
					

					Vector3 vPlayerBlipPos = CMiniMap::GetPlayerBlipPosition();
					Vector3 vOtherBlipPos = CMiniMap::GetBlipPositionValue(pBlip);

					iDistToOtherPlayer = (s32)rage::round(vPlayerBlipPos.Dist(vOtherBlipPos));
				}

				bool bBlipNameContainsHtml = false;
				if (strstr(CMiniMap::GetBlipNameValue(pBlip), "<FONT")
					&& strstr(CMiniMap::GetBlipNameValue(pBlip), "</FONT>"))
				{	//	Lowrider Bug 2534692 - the Beast blip name passed from script looks like this
					//	<FONT FACE='$Font2_cond_NOT_GAMERNAME' SIZE='15'>~a~</FONT> with the translated word Beast inserted where the ~a~ is.
					//	The ActionScript doesn't try to parse HTML within <C>...</C> so don't add those tags if the string already contains the FONT tags.
					bBlipNameContainsHtml = true;
				}

				if (CFrontendStatsMgr::ShouldUseMetric())
				{
					formatf(cUpdatedString, "%s: %s%s%s (%dm)", TheText.Get("BLIP_OTHPLYR"), bBlipNameContainsHtml?"":"<C>", CMiniMap::GetBlipNameValue(pBlip), bBlipNameContainsHtml?"":"</C>", iDistToOtherPlayer);
				}
				else
				{
					formatf(cUpdatedString, "%s: %s%s%s (%.0fft)", TheText.Get("BLIP_OTHPLYR"), bBlipNameContainsHtml?"":"<C>", CMiniMap::GetBlipNameValue(pBlip), bBlipNameContainsHtml?"":"</C>", METERS_TO_FEET(iDistToOtherPlayer));
				}
				
				break;
			}

			case BLIP_CATEGORY_PROPERTY:
			{
				formatf(cUpdatedString, "%s: %s", TheText.Get("BLIP_PROPCAT"), CMiniMap::GetBlipNameValue(pBlip));
				break;
			}

			case BLIP_CATEGORY_APARTMENT:
			{
				formatf(cUpdatedString, "%s: %s", TheText.Get("BLIP_APARTCAT"), CMiniMap::GetBlipNameValue(pBlip));
				break;
			}

			case BLIP_CATEGORY_WAYPOINT:
			{
				Vector3 vPlayerBlipPos = CMiniMap::GetPlayerBlipPosition();
				Vector3 vWaypointBlipPos = CMiniMap::GetBlipPositionValue(pBlip);
				s32 iDistToWaypoint = (s32)rage::round(vPlayerBlipPos.Dist(vWaypointBlipPos));
				if (CFrontendStatsMgr::ShouldUseMetric())
				{
					formatf(cUpdatedString, "%s (%dm)", CMiniMap::GetBlipNameValue(pBlip), iDistToWaypoint);
				}
				else
				{
					formatf(cUpdatedString, "%s (%.0fft)", CMiniMap::GetBlipNameValue(pBlip), METERS_TO_FEET(iDistToWaypoint));
				}
				break;
			}

			default:
			{
				if (bCentreBlip)
				{
					if (!NetworkInterface::IsGameInProgress())
					{
						formatf(cUpdatedString, "%s", TheText.Get("BLIP_PLAYER"));
					}
					else
					{
						formatf(cUpdatedString, "<C>%s</C>", CMiniMap::GetBlipNameValue(pBlip), NELEM(cUpdatedString));
					}
				}
				else
				{
					formatf(cUpdatedString, "%s", CMiniMap::GetBlipNameValue(pBlip));
				}
				break;
			}
		}

		CTextConversion::TextToHtml(cUpdatedString, pLegendItem->cBlipName, NELEM(pLegendItem->cBlipName));

		if (CMiniMap::GetBlipColourValue(pBlip) != BLIP_COLOUR_USE_COLOUR32)
		{
			pLegendItem->blipColour = CMiniMap_Common::GetColourFromBlipSettings(CMiniMap::GetBlipColourValue(pBlip), CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_BRIGHTNESS));
		}
		else
		{
			pLegendItem->blipColour = CMiniMap::GetBlipColour32Value(pBlip);
		}

		if (CMiniMap::GetBlipTypeValue(pBlip) == BLIP_TYPE_RADIUS || CMiniMap::GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA)  // 1531781 - if it is a radius blip then send actionscript the radar_level blip instead
		{
			safecpy(pLegendItem->cBlipObjectName, "radar_level", NELEM(pLegendItem->cBlipObjectName));
		}
		else
		{
			safecpy(pLegendItem->cBlipObjectName, CMiniMap::GetBlipObjectName(pBlip), NELEM(pLegendItem->cBlipObjectName));
		}

		pLegendItem->iBlipObjectId = CMiniMap::GetBlipObjectNameId(pBlip);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetWaypoint
// PURPOSE:	sets a waypoint
/////////////////////////////////////////////////////////////////////////////////////
void CMapMenu::SetWaypoint(bool KEYBOARD_MOUSE_ONLY(bUseMousePos) /* = false */)
{
	if (CanSetWaypoint())
	{	
		Vector2 CursorPos( CMiniMap::GetPauseMapCursor() );
#if KEYBOARD_MOUSE_SUPPORT
		if( bUseMousePos )
		{
			CursorPos = GetWorldPosFromScreen(m_vscrMousePosOnClick.x, m_vscrMousePosOnClick.y);
		}
#endif

		ObjectId BlipObjectId = NETWORK_INVALID_OBJECT_ID;

		s32 IdToUse = KEYBOARD_MOUSE_ONLY(bUseMousePos ? m_iBlipMouseCurrentlyHoverOverUniqueId.Get() : ) m_iBlipCurrentlyHoveredOverUniqueId.Get();
		if (IdToUse!=INVALID_BLIP_ID)  // if we are already locked to a selected blip, then do not toggle on/off, set the new waypoint direct to this blip, 
		{
			// prefer the blip's actual locked position if it's currently hovered over.
			if( CMiniMapBlip* pHoverBlip = CMiniMap::GetBlip(IdToUse) )
			{
				Vector3 hoverBlipPos(CMiniMap::GetBlipPositionValue(pHoverBlip));
				CursorPos.Set(hoverBlipPos.x, hoverBlipPos.y);

				if (netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(CMiniMap::FindBlipEntity(pHoverBlip)))
				{
					BlipObjectId = pNetObj->GetObjectID();
				}
			}

			if (CMiniMap::IsWaypointActive())
			{
				CMiniMapBlip *pWaypointBlip = CMiniMap::GetBlip(CMiniMap::GetActiveWaypointId());
				Vector3 vCurrentWaypointPos(CMiniMap::GetBlipPositionValue(pWaypointBlip));

				if (CursorPos.x != vCurrentWaypointPos.x || CursorPos.y != vCurrentWaypointPos.y) // unless its the same blip as the waypoint is currently set on
				{
					uiDisplayf("CMapMenu::SetWaypoint - Clearing Waypoint from the place I can never make it hit.");
					CMiniMap::SwitchOffWaypoint();
				}
			}
		}

		if (!CMiniMap::IsWaypointActive())
		{
			Vector3 vCentreBlipPos(CMiniMap::GetBlipPositionValue(CMiniMap::GetBlipFromActualId(CMiniMap::GetActualCentreBlipId())));
			if (CursorPos.x != vCentreBlipPos.x || CursorPos.y != vCentreBlipPos.y)  // ensure a waypoint is never placed ontop of the player icon
			{
				g_FrontendAudioEntity.PlaySound("WAYPOINT_SET", "HUD_FRONTEND_DEFAULT_SOUNDSET");

				CMiniMap::SwitchOnWaypoint(CursorPos.x, CursorPos.y, BlipObjectId, false);  // AdamF wants the waypoint to be visible
				ResetCornerBlipInfo(true, false);
				KEYBOARD_MOUSE_ONLY( if(!bUseMousePos || s_bMapMovesOnWaypoint ) )
				{
					CheckCursorOverBlip(CursorPos, true, false, true);
				}
				sm_vPreviousCursorPos = -CursorPos;
			}
		}
		else
		{
			uiDisplayf("CMapMenu::SetWaypoint - Clearing Waypoint normally");
			CMiniMap::SwitchOffWaypoint();
		}
	}
	else
	{
		uiDisplayf("PAUSEMAP: Tried to set a waypoint but couldn't due to CMapMenu::CanSetWaypoint() returning false");
	}
}

#if KEYBOARD_MOUSE_SUPPORT
bool CMapMenu::IsMouseOverMap() const
{
	if( CPauseMenu::GetMouseHoverIndex() != -1 || CMousePointer::IsMouseRolledOverInstructionalButtons() )
		return false;

	Vector2 mousePosNorm( m_vscrLastMousePos.x / GRCDEVICE.GetWidth(), m_vscrLastMousePos.y / GRCDEVICE.GetHeight() );
	fwRect mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
	return mon.IsInside(mousePosNorm);
}

bool CMapMenu::UpdateMouseClicksAndPosition(Vector2& vNewMapPos)
{
	const CControl& controls = CControlMgr::GetMainFrontendControl();

	bool bAllowControllerPanning = true;

	Vector2 vscrMousePos((float)ioMouse::GetX(), (float)ioMouse::GetY());
	
	// check for hovering state
	if( m_vscrLastMousePos != vscrMousePos  )
	{
		s32 iUnderMouseActualId = FindActualBlipIdNearPos(GetWorldPosFromScreen(vscrMousePos.x, vscrMousePos.y));
		if( iUnderMouseActualId != INVALID_ACTUAL_ID )
		{
			if( CMiniMapBlip* pBlip = CMiniMap::GetBlipFromActualId(iUnderMouseActualId) )
			{
				m_iBlipMouseCurrentlyHoverOverUniqueId = CMiniMap::GetUniqueBlipUsed(pBlip);
			}
			else
			{
				m_iBlipMouseCurrentlyHoverOverUniqueId.Reset();
			}
		}
		else
		{
			m_iBlipMouseCurrentlyHoverOverUniqueId.Reset();
		}
	}


	if( controls.GetCursorAccept().IsDown() && IsMouseOverMap()  )
	{
		bAllowControllerPanning = false;

		if( controls.GetCursorAccept().WasUp() )
		{
			m_bClickedWithoutDragging = true;
			m_iBlipClickedOnUniqueId = m_iBlipMouseCurrentlyHoverOverUniqueId.Get();

			// first click or we've clicked further away from the last time
			if( m_uFirstClickTime == 0 || vscrMousePos.Dist2(m_vscrMousePosOnClick) >= MOUSE_DRAG_DIST_SQRD )
			{
				m_uFirstClickTime = fwTimer::GetSystemTimeInMilliseconds();
				m_vscrMousePosOnClick = vscrMousePos;
			}
			// second click and we're within the window for a proper double click
			else if( (fwTimer::GetSystemTimeInMilliseconds() - m_uFirstClickTime) <= MOUSE_DBL_CLICK_TIME)
			{
				// double click time!
				HandleClickAction(true);

			}
			m_vwrldMouseMovement.Zero();
		}
		
		// check if they've moved the mouse outside of our wiggle room for clicks
		if( !m_bClickedWithoutDragging || vscrMousePos.Dist2(m_vscrMousePosOnClick) >= MOUSE_DRAG_DIST_SQRD )
		{
			m_bClickedWithoutDragging = false;
			// once they've moved the mouse, we can switch to grab
			CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_GRAB);
			// we can't trust ioMouse's GetDX() for this because it ends up not matching the cursor's live display
			

			// check for ability to drag a blip

			bool bBlipDragged = false;
			if( m_iBlipClickedOnUniqueId != INVALID_BLIP_ID )
			{	
				if( CMiniMapBlip* pHoveredBlip = CMiniMap::GetBlip(m_iBlipClickedOnUniqueId) )
				{
					Vector2 vWorldPos( GetWorldPosFromScreen((float)ioMouse::GetX(), (float)ioMouse::GetY() ));
					// dragging waypoint
					if( m_iBlipClickedOnUniqueId == CMiniMap::GetActiveWaypointId() )
					{
						bBlipDragged = CMiniMap::SetActiveWaypoint(vWorldPos);
					}
					// dragging POI
					else if(CMiniMap::GetBlipObjectNameId(pHoveredBlip) == RADAR_TRACE_POI)
					{
						bBlipDragged = true;
						CMiniMap::SetBlipParameter(BLIP_CHANGE_POSITION, m_iBlipClickedOnUniqueId, Vector3(vWorldPos.x, vWorldPos.y, 1.0f));
					}
				}

				if( bBlipDragged && !m_bWasBlipDragging )
				{
					g_FrontendAudioEntity.PlaySound("WAYPOINT_MOUSE_CLICK", "HUD_FRONTEND_DEFAULT_SOUNDSET");
					m_bWasBlipDragging = true;
				}
			}

			// if dragging a blip, check for push scrolling
			if( bBlipDragged )
			{
				bAllowControllerPanning = true;
				Vector2 vpctMousePos( vscrMousePos.x / SCREEN_WIDTH, vscrMousePos.y / SCREEN_HEIGHT);
				float fSafeZone = 1.0f-CHudTools::GetSafeZoneSize();
				// divide by the aspect ratio so that it's the same pixel size for the edges of the screen
				if(      vpctMousePos.x <=       fSafeZone + (MOUSE_PUSH_GUTTER_PCT / CHudTools::GetAspectRatio() ))	vNewMapPos.x -= CMiniMap_Common::GetScrollSpeedFromZoomLevel(sm_iCurrentZoomLevel);
				else if( vpctMousePos.x >= 1.0f-(fSafeZone + (MOUSE_PUSH_GUTTER_PCT / CHudTools::GetAspectRatio() )))	vNewMapPos.x += CMiniMap_Common::GetScrollSpeedFromZoomLevel(sm_iCurrentZoomLevel);

				if(      vpctMousePos.y <=       fSafeZone + MOUSE_PUSH_GUTTER_PCT )	vNewMapPos.y += CMiniMap_Common::GetScrollSpeedFromZoomLevel(sm_iCurrentZoomLevel);
				else if( vpctMousePos.y >= 1.0f-(fSafeZone + MOUSE_PUSH_GUTTER_PCT ))	vNewMapPos.y -= CMiniMap_Common::GetScrollSpeedFromZoomLevel(sm_iCurrentZoomLevel);
			}
			else
			{
				GPointF mouseDelta( vscrMousePos.x - m_vscrLastMousePos.x, vscrMousePos.y - m_vscrLastMousePos.y);
				m_vwrldMouseMovement = GetWorldPosFromScreen(mouseDelta.x, mouseDelta.y, false);

				vNewMapPos -= m_vwrldMouseMovement;
			}
		}
	}
	else // not holding or over buttons
	{
		CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);

		if( m_bWasBlipDragging )
		{
			g_FrontendAudioEntity.PlaySound("WAYPOINT_MOUSE_DROP", "HUD_FRONTEND_DEFAULT_SOUNDSET");
			m_bWasBlipDragging = false;
		}

		// time out a single click
		if( m_bClickedWithoutDragging && 
			m_uFirstClickTime && (fwTimer::GetSystemTimeInMilliseconds() - m_uFirstClickTime) > MOUSE_DBL_CLICK_TIME)
		{
			HandleClickAction(false);
		}


		// not dragging, check if we have any momentum
		if(m_vwrldMouseMovement.Mag2() > (__STICK_THRESHOLD*__STICK_THRESHOLD))
		{
			bAllowControllerPanning = false;

			m_vwrldMouseMovement = Lerp( MOUSE_FLICK_DECAY_RATE * fwTimer::GetTimeStep_NonPausedNonScaledClipped(), m_vwrldMouseMovement, Vector2(0.0f, 0.0f)); 
			vNewMapPos -= m_vwrldMouseMovement;
		}
		else
		{
			m_vwrldMouseMovement.Zero();
		}
	}

	m_vscrLastMousePos = vscrMousePos;

	return bAllowControllerPanning;
}

void CMapMenu::HandleClickAction(bool bIsDoubleClick)
{
	m_uFirstClickTime = 0u;

	if( bIsDoubleClick == s_bCenterOnSingleClick )
	{
		PlaceWaypoint(true);
	}
	else if( m_iBlipClickedOnUniqueId != INVALID_BLIP_ID )
	{	
		if( CMiniMapBlip* pBlip = CMiniMap::GetBlip(m_iBlipClickedOnUniqueId) )
		{
			SetCurrentBlipHover( CMiniMap::GetActualBlipUsed(pBlip), true, true);
			g_FrontendAudioEntity.PlaySound("TOGGLE_ON","HUD_FRONTEND_DEFAULT_SOUNDSET");
		}
	}
}

void CMapMenu::UpdateMouseWheelZoom(float& fLeftShoulder, float& fRightShoulder)
{
	const s32 sMouseWheel = ioMouse::GetDZ();
	m_fMouseWheelMovement = Clamp( m_fMouseWheelMovement + sMouseWheel * MOUSE_WHEEL_SCALAR, -1.0f, 1.0f);

	if( IsMouseOverMap()
		&& !Approach(m_fMouseWheelMovement, 0.0f, MOUSE_WHEEL_DECAY_RATE, fwTimer::GetTimeStep_NonPausedNonScaledClipped()) 
		&& fabsf(m_fMouseWheelMovement) >= __STICK_THRESHOLD ) // this is here because the zoom function gates on it
	{
		// mouse wheel zooming in
		if(m_fMouseWheelMovement > 0.0f)
		{
			fLeftShoulder = 0.0f;
			fRightShoulder = QuarticEaseInOut(m_fMouseWheelMovement); // easing to make it feel good
		}
		else
		{
			// zooming out
			fLeftShoulder = QuarticEaseInOut(-m_fMouseWheelMovement);
			fRightShoulder = 0.0f;
		}
	}
	else
	{
		m_fMouseWheelMovement = 0.0f;
		// if the mouse is hovering over the legend, allow mouse scrolling
		if( CPauseMenu::GetMouseHoverIndex() != -1 )
		{
			if( CMousePointer::IsMouseWheelUp() )
				CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_INPUT_EVENT", PAD_DPADUP );

			else if( CMousePointer::IsMouseWheelDown() )
				CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_INPUT_EVENT", PAD_DPADDOWN );
		}
	}

}

#endif // KEYBOARD_MOUSE_SUPPORT

#if __BANK
void CMapMenu::AddWidgets(bkBank* pBank)
{
	atString name("PauseMap Bank: ");
	name += m_Owner.MenuScreen.GetParserName();
	m_pMyGroup = pBank->PushGroup(name);
	{
		
		pBank->AddSlider("CURRENT ZOOM LEVEL", &sm_iCurrentZoomLevel, ZOOM_LEVEL_START, MAX_ZOOM_LEVELS, 0);
		pBank->AddText("Blip Previous Hovering", &sm_iPreviousHoverActualId);
		pBank->AddText("Blip For Mission Creator", &sm_iCurrentSelectedMissionCreatorUniqueId);
		pBank->AddButton("Reinit Menu", datCallback(CFA(CMapMenu::ReloadMap)));
#if KEYBOARD_MOUSE_SUPPORT
		pBank->AddText("Blip Clicked On", &m_iBlipClickedOnUniqueId);
		pBank->AddText("Blip Hovering", m_iBlipMouseCurrentlyHoverOverUniqueId.GetPtr());
		pBank->PushGroup("PC Tune");
		{
			const char* pszaEaseNames[] = {
				"Linear",
				"Quadratic In",
				"Quadratic Out",
				"Quadratic InOut",
				"Cubic In",
				"Cubic Out",
				"Cubic InOut",
				"Quartic In",
				"Quartic Out",
				"Quartic InOut",
				"Sine In",
				"Sine Out",
				"Sine InOut",
				"Back In",
				"Back Out",
				"Back InOut",
				"Circular In",
				"Circular Out",
				"Circular InOut"
			};
		
			pBank->AddToggle("Map Moves on Waypoint Creation", &s_bMapMovesOnWaypoint);
			pBank->AddToggle("Switch Single Click (Focus) and Double Click (Waypoint) actions", &s_bCenterOnSingleClick);
			pBank->AddSlider("MOUSE_WHEEL_DECAY_RATE",	&MOUSE_WHEEL_DECAY_RATE, 0.f, 10.f, 0.1f);
			pBank->AddSlider("MOUSE_WHEEL_SCALAR",		&MOUSE_WHEEL_SCALAR,	 0.f, 10.f, 0.1f);
			pBank->AddSlider("MOUSE_FLICK_DECAY_RATE",	&MOUSE_FLICK_DECAY_RATE, 0.f, 1000.f, 0.1f);
			pBank->AddSlider("MOUSE_ZOOM_SPEED_FAR",	&MOUSE_ZOOM_SPEED_FAR,		 0.f, 1000.f, 0.1f);
			pBank->AddSlider("MOUSE_ZOOM_SPEED_NEAR",	&MOUSE_ZOOM_SPEED_NEAR,		 0.f, 1000.f, 0.1f);
			pBank->AddSlider("MOUSE_DRAG_DIST_SQRD (Pixels^2)",	&MOUSE_DRAG_DIST_SQRD,		 0.f, 5000.f, 5.0f);
			pBank->AddSlider("MOUSE_DRAG_SOUND_SCREEN_PIXELS_SQRD (Pixels^2)",	&MOUSE_DRAG_SOUND_SCREEN_PIXELS_SQRD,		 0.f, 5000.f, 5.0f);
			pBank->AddSlider("MAX_SOUNDVAR_VALUE",	&MAX_SOUNDVAR_VALUE,		 0.f, 500.f, 5.0f);

			pBank->AddSlider("MOUSE_DBL_CLICK_TIME (ms)",	&MOUSE_DBL_CLICK_TIME,		 0, 1000, 25);
			pBank->AddSlider("MAP_SNAP_PAN_SPEED (ms)",		&MAP_SNAP_PAN_SPEED,		 0, 5000, 25);
			pBank->AddSlider("MAP_FIRST_PAN_DELAY (ms)",&MAP_FIRST_PAN_DELAY,		 0, 5000, 25);
			pBank->AddSlider("MAP_SPAM_PAN_DELAY (ms)",	&MAP_SPAM_PAN_DELAY,		 0, 5000, 25);
			pBank->AddSlider("MOUSE_PUSH_GUTTER_PCT",	&MOUSE_PUSH_GUTTER_PCT, 0.0f, 1.0f, 0.01f);
			pBank->AddCombo("LERP TYPE",				&s_LERP_METHOD,		COUNTOF(pszaEaseNames), pszaEaseNames);
		}
		pBank->PopGroup();
#endif
	}
	pBank->PopGroup();
}

#endif


