////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MiniMap.cpp
// PURPOSE : manages the Scaleform radar code
// AUTHOR  : Derek Payne
// STARTED : 18/08/2010
//
/////////////////////////////////////////////////////////////////////////////////

// rage:
#include "parser/manager.h"
#include "grcore/viewport.h"

// fw:
#include "fwscript/scriptguid.h"

// game:
#include "audio/emitteraudioentity.h"
#include "audio/scriptaudioentity.h"
#include "frontend/ui_channel.h"
#include "MiniMap.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/debug/DebugDirector.h"
#include "Control/gamelogic.h"
#include "Control/gps.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/LightProbe.h"
#include "debug/TiledScreenCapture.h"
#include "frontend/FrontendStatsMgr.h"  // for checking whether we need metric measurements or not
#include "frontend/GameStreamMgr.h"
#include "frontend/CMapMenu.h"
#include "frontend/MiniMapRenderThread.h"
#include "frontend/MobilePhone.h"
#include "frontend/NewHud.h"
#include "frontend/PauseMenu.h"
#include "frontend/WarningScreen.h"
#include "frontend/SocialClubMenu.h"
#include "frontend/Map/BlipEnums.h"
#include "frontend/Scaleform/ScaleformMgr.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "modelinfo/PedModelInfo.h"
#include "vehicles/vehicle.h"
#include "vehicles/Submarine.h"
#include "vehicles/Heli.h"
#include "renderer/rendertargets.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "physics/WaterTestHelper.h"
#include "text/TextConversion.h"

#include "Game/user.h"
#include "game/weather.h"
#include "peds/Ped.h"
#include "peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "peds/PedIntelligence.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/PickupManager.h"
#include "scene/EntityIterator.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "streaming/streamingvisualize.h"
#include "system/Control.h"
#include "system/controlMgr.h"
#include "system/param.h"
#include "task/Combat/TaskInvestigate.h"
#include "task/movement/TaskParachute.h"
#include "vehicles/Planes.h"
#include "vehicles/Bike.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/vehiclepopulation.h"
#include "vfx/misc/markers.h"

#include "frontend/Metadata_parser.h"
//	Include and libs for SmartGlass stuff
#include "system/companion.h"

#if RSG_PC
#include "rline/rlpc.h"
#include "system/TamperActions.h"
#endif // RSG_PC

#if __BANK
#include "system/controlmgr.h"
#include "debug/VectorMap.h"
#endif  // __BANK

#if DEBUG_PAD_INPUT
#include "debug/debug.h"
#endif  // #if DEBUG_PAD_INPUT

FRONTEND_OPTIMISATIONS()

PF_PAGE(MiniMap, "MiniMap");
PF_GROUP(MiniMapSonar);
PF_LINK(MiniMap, MiniMapSonar);

PF_TIMER(AddSonarBlip, MiniMapSonar);

#define TIME_TO_ZOOM_OUT_MINIMAP (2000)

#if	!__FINAL
PARAM(nominimap, "[code] hide minimap");
PARAM(revealmap, "[code] reveal map on boot");
#endif

#if __DEV
u32 iTimeTaken = 0;
#endif // __DEV

#if __BANK

enum
{
	SONAR_STYLE_NONE = 0,
	SONAR_STYLE_RED_BLIPS_SONAR,
	SONAR_STYLE_RED_BLIPS_FLASH,
	SONAR_STYLE_SONAR_ONLY,
	SONAR_STYLE_RED_BLIPS_FLASH_5_SECONDS,
	MAX_SONAR_STYLES
};

extern CWeather g_weather;

bool bDisplayInteriorInfoToLog = false;
bool bDebug3DBlips = false;
float s_fDebugForceTilesAroundPlayer = -2.0f;
Vector2 s_vDebugForceTilesAroundPlayer(-1.0f, -1.0f);
float fDebugTiltValue = -1.0f;
float fDebugOffsetValue = -1.0f;
bool bDebugAlwaysDrawSonarBlips = false;
bool bDebugDrawSonarBlipRange = false;
bool bDebugVisibilityOfSonarBlips = false;
bool s_bRenderMapPositions = false;

bool noUIAA = false;
bool noUIAAOnBitMap = true;

static s32 iDebugSonarStyle = SONAR_STYLE_RED_BLIPS_SONAR;
static bool bAllPedBlipsAsStealth = false;

static float s_fRadarZoomDistanceThisFrameWidget = 0.0f;
static bool s_bZoomToWaypointBlip = false;

//static Vector2 vMax2dRadarDistanceSquareTest(0.43f, 0.461f);

#endif // __BANK

#define MAX_BLIP_REFERENCE_INDEX	(0xffff)
#define BLIP_REFERENCE_SHIFT		(16)
#define BLIP_REFERENCE_MASK			(0xffff0000)

#define DEFAULT_BLIP_FLASH_INTERVAL (500)  // default blip flash time in milliseconds
#define XML_CORE_NODE_NAME "minimap"
#define XML_VERSION			1.0

CMiniMap::Tunables CMiniMap::sm_Tunables;
IMPLEMENT_MINIMAP_TUNABLES(CMiniMap, 0xb3bbe4a1);

sMiniReappearanceState CMiniMap::ms_MiniMapReappearanceState;
bool CMiniMap::ms_bIsHiddenAfterTransition = false;

//bool CMiniMap::bSkipBlipUpdate = false;
sMiniMapInterior CMiniMap::ms_previousInterior;
CBlipCones CMiniMap::ms_blipCones;
atArray<sMinimapFakeCone> CMiniMap::m_FakeCones;
atMap<int, eHUD_COLOURS> CMiniMap::m_ConeColors;
eSETUP_STATE CMiniMap::MinimapModeState = MINIMAP_MODE_STATE_NONE;
eSETUP_STATE CMiniMap::ms_pendingMinimapModeState = MINIMAP_MODE_STATE_NONE;

float CMiniMap::ms_fBlipZoom = 40.0f;
float CMiniMap::ms_fMapZoom = 0.0014f;

bool CMiniMap::ms_bBigMapFullZoom = false;
bool CMiniMap::ms_bBitmapInitialized = false;
bool CMiniMap::ms_bEntityWaypointAllowed = false;
eWaypointClearOnArrivalMode CMiniMap::ms_bWaypointClearOnArrivalMode = eWaypointClearOnArrivalMode::Enabled;
u16 CMiniMap::ms_waypointGpsFlags = 0;
sWaypointStruct CMiniMap::ms_Waypoint[MAX_WAYPOINT_MARKERS];
atMultiArray3d<sBitmapStruct> CMiniMap::ms_BitmapBackground;
sBitmapStruct CMiniMap::ms_BitmapSuperLowRes;
atFixedArray<sPointOfInterestStruct, MAX_POI_STANDARD_USER> CMiniMap::ms_PointOfInterest;
atArray<sMiniMapInteriorStruct> CMiniMap::sm_InteriorInfo;
bool CMiniMap::ms_bFoundMatchingOverrideInterior = false;
bool CMiniMap::ms_bBlipVisibilityHighDetail = false;


#if __BANK
bool CMiniMap::ms_g_bDisplayGameMiniMap = true;
bool CMiniMap::ms_bRenderMiniMapBackground = true;
bool CMiniMap::ms_bRenderMiniMapForeground = true;
float CMiniMap::ms_fDebugRange = 2.0f;
s32 CMiniMap::ms_iDebugInteriorHash = 0;
s32 CMiniMap::ms_iDebugInteriorLevel = sMiniMapInterior::INVALID_LEVEL;
s32 CMiniMap::ms_iDebugGolfCourseMap = -1;
bool CMiniMap::ms_bCullDistantBlips = true;
bool CMiniMap::ms_bShowBlipVectorMap = false;

bool CMiniMap::ms_bDebugDrawTiles = false;

bool CMiniMap::ms_bOverrideConeColour = false;
s32 CMiniMap::ms_ConeColourRed = 47;
s32 CMiniMap::ms_ConeColourGreen = 92;
s32 CMiniMap::ms_ConeColourBlue = 115;
s32 CMiniMap::ms_ConeColourAlpha = CMiniMap_Common::ALPHA_OF_POLICE_PERCEPTION_CONES;

float CMiniMap::ms_fConeFocusRangeMultiplier = 1.05f;
bool CMiniMap::ms_bDrawActualRangeCone = false;

s32 CMiniMap::ms_iDebugBlip = INVALID_BLIP_ID;
bool ms_bUseNewAwesomeAltimeter = true;
#else
#define ms_bUseNewAwesomeAltimeter true
#endif

//	sMiniMapRenderStateStruct CMiniMap::MiniMapUpdateState;


#if __DEV
s32 CMiniMap::iNumCreatedTraces = 0;
u32 CMiniMap::iSizeOfBlipsCreated = 0;
#endif

atVector<sSonarBlipStruct> CMiniMap::ms_SonarBlips;

Vector3 CMiniMap::ms_vMapPosition;
float CMiniMap::ms_fMaxDistance = MINIMAP_DISTANCE_SCALER_2D;

bool CMiniMap::bVisible = true;

#if __BANK
bool CMiniMap::bDisplayAllBlipNames = false;
#endif // __BANK

#if __DEV
bool CMiniMap::ms_bRenderPedPerceptionCone = false;
#endif	//	__DEV

// private variables
bool CMiniMap::ms_bXmlDataHasBeenRead = false;
bool CMiniMap::ms_bCentreBlipPosChangedThisFrame = false;
s32 CMiniMap::ms_iCentreBlip = INVALID_BLIP_ID;
s32 CMiniMap::ms_iNorthBlip = INVALID_BLIP_ID;
s32 CMiniMap::ms_iMiniMapPedPoolIndex = 0;
//	s32 CMiniMap::ms_iBlipHovering = -1;
CMiniMap::MiniMapDesc CMiniMap::sm_miniMaps[MAX_MINIMAP_VALUES];
bool CMiniMap::ms_bSonarBlipsActive = true;

//char CMiniMap::ms_cMiniMapFileName[MAX_MINIMAP_VALUES][MAX_LENGTH_OF_MINIMAP_FILENAME];
//Vector2 CMiniMap::ms_vMiniMapPosition[MAX_MINIMAP_VALUES];
//Vector2 CMiniMap::ms_vMiniMapSize[MAX_MINIMAP_VALUES];

bool CMiniMap::ms_bInStealthMode = false;
bool CMiniMap::ms_bHideBackgroundMap = false;
bool CMiniMap::ms_bBlockWaypoint = false;
bool CMiniMap::ms_bInPrologue = false;
bool CMiniMap::ms_bOnIslandMap = false;
bool CMiniMap::ms_bInPauseMap = false;
bool CMiniMap::ms_bInCustomMap = false;
bool CMiniMap::ms_bInBigMap = false;
bool CMiniMap::ms_bInGolfMap = false;
bool CMiniMap::ms_bIsRendering = false;
//bool CMiniMap::ms_bShowingGPSDisplay = false;
bool CMiniMap::ms_bShowingYoke = false;
bool CMiniMap::ms_bShowSonarSweep = false;
bool CMiniMap::ms_bAllowFoWInMP = false;
bool CMiniMap::ms_bRequestRevealFoW = false;

bool CMiniMap::ms_bBigMapFullScreen = false;
bool CMiniMap::ms_bIsWaypointLocal = false;
unsigned CMiniMap::ms_WaypointLocalDirtyTimestamp = 0;
bool CMiniMap::ms_bFlashWantedOverlay = false;
bool CMiniMap::ms_bForceSonarBlipsOn = false;
bool CMiniMap::ms_bUseWeaponRangeForMPSonarDistance = true;

u32 CMiniMap::ms_uFlashStartTime = 0u;
eHUD_COLOURS CMiniMap::ms_eFlashColour = HUD_COLOUR_WHITE;

KEYBOARD_MOUSE_ONLY(eALTIMETER_MODES CMiniMap::ms_iPrevAltimeterMode = ALTIMETER_OFF;)
eGOLF_COURSE_HOLES CMiniMap::ms_iGolfCourseMap = GOLF_COURSE_OFF;
s32 CMiniMap::ms_iLockedMiniMapAngle = -1;
Vector2 CMiniMap::ms_vLockedMiniMapPosition = Vector2(-9999.0f,-9999.0f);

Vector2 CMiniMap::ms_vPauseMenuMapCursor;
float CMiniMap::ms_fPauseMenuMapScale;	

s32 CMiniMap::iSimpleBlip = -1;
u16 CMiniMap::iReferenceCount = 0;

bool CMiniMap::bActive = false;
s32 CMiniMap::iMovieId[MAX_MINIMAP_MOVIE_LAYERS];

CMiniMapBlip *CMiniMap::blip[MAX_NUM_BLIPS];

s32 CMiniMap::iMaxCreatedBlips = 0;

//	float CMiniMap::fPreviousWantedLevel = 0.0f;
bool CMiniMap::bPreviousMultiplayerStatus = false;	//	Not sure whether this should be false or true
bool CMiniMap::ms_bPreviousShowDepthGauge = false;
s32 CMiniMap::iPreviousGpsInstruction = -1;
s32 CMiniMap::ms_iPreviousMapRotation = -1;
float CMiniMap::fPreviousGPSDistance = -1.0f;
float CMiniMap::fPreviousMinimapFloor = -1.0f;
u32 CMiniMap::iMiniMapDpadZoomTimer = 0;

#if ENABLE_FOG_OF_WAR
bool CMiniMap::ms_bRequestFoWClear = true;
bool CMiniMap::ms_bSetFowIsValid = false;
bool CMiniMap::ms_bHideFoW = false;
bool CMiniMap::ms_bDoNotUpdateFoW = false;
bool CMiniMap::ms_bIgnoreFoWOnInit = false;
bool CMiniMap::ms_bUploadFoWTextureData = false;
bool CMiniMap::ms_bFoWUseCustomExtents = false;
Vector2 CMiniMap::ms_vecFoWCustomWorldPos = Vector2(0.0f,0.0f);
Vector2 CMiniMap::ms_vecFoWCustomWorldSize = Vector2(1.0f,1.0f);
Vector2 CMiniMap::ms_ScriptReveal[8];
int	CMiniMap::ms_numScriptRevealRequested = 0;
#endif // ENABLE_FOG_OF_WAR

bool CMiniMap::ms_bInsideReInit = false;
#if RSG_PC
u8 s_iDeviceLostResetCountdown = 0;
#endif

const float CMiniMap::fMinTimeBetweenBlipsMS = 250;
//
// tilt & offset values:
//
#if __BANK
#define MINIMAP_MIN_TILT (0.0f)
#define MINIMAP_MAX_TILT (200.0f)
#define MINIMAP_TILT_STEP (1.0f)

#define MINIMAP_MIN_OFFSET (0.0f)
#define MINIMAP_MAX_OFFSET (100.0f)
#define MINIMAP_OFFSET_STEP (0.1f)

#define MINIMAP_ZOOM_STEP (1.0f)

#define MINIMAP_MIN_ZOOM_SCALER (0.1f)
#define MINIMAP_MAX_ZOOM_SCALER (10.0f)
#define MINIMAP_ZOOM_SCALER_STEP (0.01f)
#endif	//	__BANK


float ALTIMETER_GROUND_LEVEL_FUDGE_FACTOR	= 0.0f;
float ALTIMETER_TICK_PIXEL_HEIGHT			= 91.0f;

BankFloat ALTIMETER_HEIGHTMAP_WIGGLE_ROOM	= 15.0f;

float MINIMAP_GPS_DISPLAY_LIMIT			= 100.0f;

float MAP_BLIP_ROUND_THRESHOLD			= 50.0f;

extern bool bMiniMap3D;
extern bool bMiniMapSquare;

#if __BANK  && ENABLE_FOG_OF_WAR
extern bool DebugDrawFOW;
extern float DebugFOWX1;
extern float DebugFOWY1;
extern float DebugFOWX2;
extern float DebugFOWY2;

bool displayRevealFoWRatio = false;
bool testRevealFoWCoord = false;
float testRevealX = 0.0f;
float testRevealY = 0.0f;

bool bIgnoreFogOfWarValueOf255 = false;
s32 fogOfWarMinX = FOG_OF_WAR_RT_SIZE;
s32 fogOfWarMinY = FOG_OF_WAR_RT_SIZE;
s32 fogOfWarMaxX = -1;
s32 fogOfWarMaxY = -1;
#endif // __BANK


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CEntityPoolIndexForBlip
/////////////////////////////////////////////////////////////////////////////////////

CEntityPoolIndexForBlip::CEntityPoolIndexForBlip(const CEntity *pEntity, eBLIP_TYPE blipType)
{
	m_iPoolIndex = sysMemPoolAllocator::s_invalidIndex;

	if (uiVerifyf(pEntity, "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - entity pointer is NULL"))
	{
		switch (blipType)
		{
		case BLIP_TYPE_CAR:
			if (uiVerifyf(pEntity->GetIsTypeVehicle(), "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - blip type is BLIP_TYPE_CAR so expected entity to be a vehicle"))
			{
				m_iPoolIndex = (s32) CVehicle::GetPool()->GetJustIndex(pEntity);
			}
			break;

		case BLIP_TYPE_CHAR:
			if (uiVerifyf(pEntity->GetIsTypePed(), "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - blip type is BLIP_TYPE_CHAR so expected entity to be a ped"))
			{
				m_iPoolIndex = CPed::GetPool()->GetJustIndex(pEntity);
			}
			break;

		case BLIP_TYPE_STEALTH:
			if (uiVerifyf(pEntity->GetIsTypePed(), "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - blip type is BLIP_TYPE_STEALTH so expected entity to be a ped"))
			{
				m_iPoolIndex = CPed::GetPool()->GetJustIndex(pEntity);
			}
			break;

		case BLIP_TYPE_OBJECT:
			{
				if (uiVerifyf(pEntity->GetIsTypeObject(), "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - blip type is BLIP_TYPE_OBJECT so expected entity to be an object"))
				{
					const CObject *pObject = static_cast<const CObject*>(pEntity);
					if (uiVerifyf(!pObject->m_nObjectFlags.bIsPickUp, "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - blip type is BLIP_TYPE_OBJECT so expected bIsPickUp to be FALSE"))
					{
						m_iPoolIndex = (s32) CObject::GetPool()->GetJustIndex(pEntity);
					}
				}
			}
			break;

		case BLIP_TYPE_PICKUP_OBJECT :
			{
				if (uiVerifyf(pEntity->GetIsTypeObject(), "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - blip type is BLIP_TYPE_PICKUP_OBJECT so expected entity to be an object"))
				{
					const CObject *pObject = static_cast<const CObject*>(pEntity);
					if (uiVerifyf(pObject->m_nObjectFlags.bIsPickUp, "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - blip type is BLIP_TYPE_PICKUP_OBJECT so expected bIsPickUp to be TRUE"))
					{
						m_iPoolIndex = CPickup::GetPool()->GetJustIndex(pEntity);
					}
				}
			}
			break;

		case BLIP_TYPE_COP:
			if (uiVerifyf(pEntity->GetIsTypePed(), "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - blip type is BLIP_TYPE_COP so expected entity to be a ped"))
			{
				m_iPoolIndex = CPed::GetPool()->GetJustIndex(pEntity);
			}
			break;

		default:
			uiAssertf(0, "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - unsupported blip type %d", blipType);
			break;
		}
	}
}

CEntityPoolIndexForBlip::CEntityPoolIndexForBlip(const CPickupPlacement *pPickupPlacement)
{
	m_iPoolIndex = sysMemPoolAllocator::s_invalidIndex;
	if (uiVerifyf(pPickupPlacement, "CEntityPoolIndexForBlip::CEntityPoolIndexForBlip - pickup placement pointer is NULL"))
	{
		m_iPoolIndex = CPickupPlacement::GetPool()->GetJustIndex(pPickupPlacement);
	}
}

eBLIP_TYPE CEntityPoolIndexForBlip::GetBlipTypeForEntity(const CEntity *pEntity)
{
	eBLIP_TYPE blipType = BLIP_TYPE_UNUSED;
	if (uiVerifyf(pEntity, "CEntityPoolIndexForBlip::GetBlipTypeForEntity - invalid entity"))
	{
		if (pEntity->GetIsTypeVehicle())
		{
			blipType = BLIP_TYPE_CAR;
		}
		else if (pEntity->GetIsTypePed())
		{
			blipType = BLIP_TYPE_CHAR;
		}
		else if (pEntity->GetIsTypeObject())
		{
			blipType = BLIP_TYPE_OBJECT;
		}
		else
		{
			uiAssertf(0, "CEntityPoolIndexForBlip::GetBlipTypeForEntity - failed to resolve blip type for: %s", pEntity->GetDebugName());
		}
	}
	return blipType;
}

CEntity *CEntityPoolIndexForBlip::GetEntity(eBLIP_TYPE blipType) const
{
	if (IsValid())
	{
		switch (blipType)
		{
		case BLIP_TYPE_CAR:
			return CVehicle::GetPool()->GetSlot(m_iPoolIndex);
		case BLIP_TYPE_CHAR:
		case BLIP_TYPE_STEALTH:
			return CPed::GetPool()->GetSlot(m_iPoolIndex);
		case BLIP_TYPE_OBJECT:
			return CObject::GetPool()->GetSlot(m_iPoolIndex);
		case BLIP_TYPE_PICKUP_OBJECT :
			return CPickup::GetPool()->GetSlot(m_iPoolIndex);
		case BLIP_TYPE_COP:
			return CPed::GetPool()->GetSlot(m_iPoolIndex);
		default:
			break;
		}
	}

	return NULL;
}


CPickupPlacement *CEntityPoolIndexForBlip::GetPickupPlacement()
{
	if (IsValid())
	{
		if (uiVerifyf(m_iPoolIndex >= 0 && m_iPoolIndex < CPickupPlacement::GetPool()->GetSize(), "CEntityPoolIndexForBlip::GetPickupPlacement - placement index %d is out of range", m_iPoolIndex))
		{
			return CPickupPlacement::GetPool()->GetSlot(m_iPoolIndex);
		}
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBlipComplex::CBlipComplex()
// PURPOSE: constructor for a complex blip
/////////////////////////////////////////////////////////////////////////////////////
CBlipComplex::CBlipComplex() : CMiniMapBlip(true), m_PoolIndex()
{
	m_flags.Reset();
	cLocName.Reset();

	vPosition.Set(0.0f, 0.0f, 0.0f);  // position

	fDirection = DEFAULT_BLIP_ROTATION;  // direction set to 360 degrees (instead of 0) so we can determine later if we have rotated it or its never been rotated without having to store a flag

	priority = BLIP_PRIORITY_LOW;  // priority of blip
	type = BLIP_TYPE_UNUSED;  // type of blip
	display = BLIP_DISPLAY_NEITHER;  // display type

	vScale.Set(1.0f, 1.0f);  // scale of the blip
	iColour = BLIP_COLOUR_DEFAULT;  // colour
	iAlpha = 0;  // alpha of the blip
	iNumberToDisplay = -1;
#if HASHED_BLIP_IDS
	iLinkageId.Clear();
#else
	iLinkageId = -1;
#endif
}


void CBlipCones::Init(unsigned initMode)
{
	if (INIT_SESSION == initMode)
	{
		m_MapOfBlipCones.Reset();
	}
}

void CBlipCones::ClearUsedFlagsForAllBlipCones()
{
	BlipConeMap::Iterator mapIter = m_MapOfBlipCones.CreateIterator();

	while (!mapIter.AtEnd())
	{
		mapIter.GetData().m_bUsedThisFrame = false;
		mapIter.Next();
	}
}

void CBlipCones::RemoveUnusedBlipCones()
{
	BlipConeMap::Iterator mapIter = m_MapOfBlipCones.CreateIterator();

	while (!mapIter.AtEnd())
	{
		if (mapIter.GetData().m_bUsedThisFrame == false)
		{
			m_MapOfBlipCones.Delete(mapIter.GetKey());
			mapIter.Start();
		}
		else
		{
			mapIter.Next();
		}
	}
}

//	Returns true if the cone should be redrawn either because it's a new cone or because the focus range has changed
bool CBlipCones::UpdateBlipConeParameters(s32 UniqueBlipId, float fFocusRange, bool bRedrawIfOtherParametersChange, float fPeripheralRange, float fGazeAngle, float fVisualFieldMinAzimuthAngle, float fVisualFieldMaxAzimuthAngle)
{
	bool bReturnValue = false;
	sConeDetails *pConeDetails = m_MapOfBlipCones.Access(UniqueBlipId);
	if (pConeDetails)
	{
		pConeDetails->m_bUsedThisFrame = true;
		if (pConeDetails->m_fFocusRange != fFocusRange)
		{
			pConeDetails->m_fFocusRange = fFocusRange;
			bReturnValue = true;
		}

		if (bRedrawIfOtherParametersChange)
		{
			// Update all other cone parameters and flag a redraw if needed.
			if (pConeDetails->m_fPeripheralRange != fPeripheralRange)
			{
				pConeDetails->m_fPeripheralRange = fPeripheralRange;
				bReturnValue = true;
			}
			if (pConeDetails->m_fGazeAngle != fGazeAngle)
			{
				pConeDetails->m_fFocusRange = fFocusRange;
				bReturnValue = true;
			}
			if (pConeDetails->m_fVisualFieldMinAzimuthAngle != fVisualFieldMinAzimuthAngle)
			{
				pConeDetails->m_fVisualFieldMinAzimuthAngle = fVisualFieldMinAzimuthAngle;
				bReturnValue = true;
			}
			if (pConeDetails->m_fVisualFieldMaxAzimuthAngle != fVisualFieldMaxAzimuthAngle)
			{
				pConeDetails->m_fVisualFieldMaxAzimuthAngle = fVisualFieldMaxAzimuthAngle;
				bReturnValue = true;
			}
		}
	}
	else
	{
		sConeDetails newConeDetails(fFocusRange, true, fPeripheralRange, fGazeAngle, fVisualFieldMinAzimuthAngle, fVisualFieldMaxAzimuthAngle);
		m_MapOfBlipCones.Insert(UniqueBlipId, newConeDetails);

		bReturnValue = true;
	}

	return bReturnValue;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::CMiniMap()
// PURPOSE: constructor for the minimap manager class
/////////////////////////////////////////////////////////////////////////////////////
//	CMiniMap::CMiniMap() // Init()
void CMiniMap::OldMiniMapConstructor()
{
	bActive = false;

	MinimapModeState = MINIMAP_MODE_STATE_NONE;

#if __DEV
	iNumCreatedTraces = 0;
	iSizeOfBlipsCreated = 0;
#endif

#if __BANK
	bDisplayAllBlipNames = false;
#endif

	for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
	{
		iMovieId[i] = -1;
	}

	iReferenceCount = 0;
	iSimpleBlip = -1;
	iMaxCreatedBlips = 0;

//	fPreviousWantedLevel = 0.0f;

	bVisible = true;

	iPreviousGpsInstruction = -1;
	fPreviousGPSDistance = -1.0f;
	fPreviousMinimapFloor = -1.0f;
	ms_bPreviousShowDepthGauge = false;

	if (CreateMiniMapMovie())
	{
		bPreviousMultiplayerStatus = !NetworkInterface::IsGameInProgress();  // this is initialised as this so the 1st update gets trigged and ActionScript is told
		iMiniMapDpadZoomTimer = 0;

		// initalise the global:
// 		MiniMapUpdateState.vPos = ORIGIN;
// 		MiniMapUpdateState.bDeadOrArrested = false;
// 		MiniMapUpdateState.bInPlaneOrHeli = false;
// 		MiniMapUpdateState.fRoll = 0.0f;
// 		MiniMapUpdateState.fSpeed = 0.0f;
// 		MiniMapUpdateState.bRunning = false;

		// initialise all blips:
		for (s32 iCount = 0; iCount < MAX_NUM_BLIPS; iCount++)
		{
			blip[iCount] = NULL;
		}

		CMiniMap_Common::EnumerateBlipLinkages(GetMovieId(MINIMAP_MOVIE_FOREGROUND));

		CMiniMap_RenderThread::SetupContainers();

		// turn off all components
		for (s32 iComponentId = 0; iComponentId < MAX_MINIMAP_COMPONENTS; iComponentId++)
		{
			ToggleComponent(iComponentId, false, true);
		}

		CNewHud::RepopulateHealthInfoDisplay();
	}
	else
	{
		uiAssertf(0, "MiniMap: Couldnt set up MiniMap Scaleform movie!");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetActive();
// PURPOSE:	signals that we are done with initialization and starts the movie
/////////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetActive()
{
	bActive = true;

	CScaleformMgr::UnlockMovie(GetMovieId(MINIMAP_MOVIE_BACKGROUND));
	CScaleformMgr::UnlockMovie(GetMovieId(MINIMAP_MOVIE_FOREGROUND));
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetSimpleBlipDefaults()
// PURPOSE:	this creates an invisible complex blip that all the simple blips get based on
/////////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetSimpleBlipDefaults()
{
	iSimpleBlip = CreateGenericBlip(true, BLIP_TYPE_WEAPON_PICKUP, CEntityPoolIndexForBlip(), Vector3(0.0f,0.0f,1.0f), BLIP_DISPLAY_BLIPONLY, "SimpleBlip");

	iSimpleBlip = ConvertUniqueBlipToActualBlip(iSimpleBlip);  // store the actual blip id

	CMiniMapBlip *pBlip = GetBlipFromActualId(iSimpleBlip);

	if (pBlip)
	{
		// set the default simple blips as shortrange, low priority & very small
		SetBlipPriorityValue(pBlip, BLIP_PRIORITY_LOW);
		SetBlipScaleValue(pBlip, 0.4f);
		SetBlipColourValue(pBlip, BLIP_COLOUR_SIMPLEBLIP_DEFAULT);
		SetBlipNameValue(pBlip, "BLIP_PICK", true);
		SetFlag(pBlip, BLIP_FLAG_SHORTRANGE);
	}
}






/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CreateSimpleBlip()
// PURPOSE:	creates a 'simple' blip which just holds position
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::CreateSimpleBlip(const Vector3& vPosition, const char *pScriptName)
{
	return (CreateGenericBlip(false, BLIP_TYPE_COORDS, CEntityPoolIndexForBlip(), vPosition, BLIP_DISPLAY_BOTH, pScriptName));
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CreateBlip()
// PURPOSE:	calls the generic blip creation function with relevant values for either
//			entity or coord blip values
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::CreateBlip(bool bComplex, eBLIP_TYPE iBlipType, const CEntityPoolIndexForBlip& iPoolIndex, eBLIP_DISPLAY_TYPE iDisplayFlag, const char *pScriptName)
{
	return (CreateGenericBlip(bComplex, iBlipType, iPoolIndex, Vector3(ORIGIN), iDisplayFlag, pScriptName));
}

s32 CMiniMap::CreateBlip(bool bComplex, eBLIP_TYPE iBlipType, const Vector3& vPosition, eBLIP_DISPLAY_TYPE iDisplayFlag, const char *pScriptName)
{
	s32 iReturnUniqueId = CreateGenericBlip(bComplex, iBlipType, CEntityPoolIndexForBlip(), vPosition, iDisplayFlag, pScriptName);

	if (vPosition == ORIGIN)
	{
		uiAssertf(0, "MiniMap: Blip %d was created at origin!  Suspect=%s", iReturnUniqueId, pScriptName);

		// DestroyBlipObject(iReturnUniqueId);  // eventually we will want to return invalid blip ID but we will give them a chance to fix stuff 1st
		// return INVALID_BLIP_ID;
	}

	return (iReturnUniqueId);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CreateGenericBlip()
// PURPOSE:	one function that deals with creating all the default settings for a new
//			blip
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::CreateGenericBlip(bool bComplex, eBLIP_TYPE iBlipType, const CEntityPoolIndexForBlip& iPoolIndex, const Vector3& vPosition, eBLIP_DISPLAY_TYPE iDisplayFlag, const char *DEV_ONLY(pScriptName) )
{
	s32 iIndex = FindAndCreateNextEmptyBlipSpace(bComplex);  // create rage_new blip & active it

	if (iIndex != -1)
	{
		CMiniMapBlip *pBlip = GetBlipFromActualId(iIndex);

		uiAssertf(pBlip, "MiniMap: Invalid blip at array index %d", iIndex);

		if (bComplex)
		{
			// set new blip settings:
			ResetFlag(pBlip);  // set no initial flags (0)
			SetBlipPriorityValue(pBlip, BLIP_PRIORITY_HIGH);
			SetBlipDirectionValue(pBlip, 360.0f, false);  // set to 360 degrees and do not clamp back to 0 (we later check this to see if the blip has been rotated out of its default 360 by using 0-359 degrees range
			SetBlipCategoryValue(pBlip, BLIP_CATEGORY_OTHER);  // none unless specified by script
			SetBlipTypeValue(pBlip, iBlipType);
			SetBlipScaleValue(pBlip, 1.0f);

			CRGBA secondaryBlipColour = CHudColour::GetRGBA(HUD_COLOUR_BLUE);  // default to BLUE so the existing FRIEND blips still work as they used to in script till this syncs up
			SetBlipSecondaryColourValue(pBlip, secondaryBlipColour);
			SetBlipAlphaValue(pBlip, 255);
			SetBlipFlashInterval(pBlip, DEFAULT_BLIP_FLASH_INTERVAL);
			SetBlipDisplayValue(pBlip, iDisplayFlag);
			SetBlipObjectName(pBlip, BLIP_LEVEL);  // default blip style
			SetBlipFlashDuration(pBlip, MAX_UINT16);
			SetFlag(pBlip, BLIP_FLAG_SHOW_HEIGHT);
			SetFlag(pBlip, BLIP_FLAG_HIGH_DETAIL);  // all blips are high detail as default

			SetBlipOn3dMap(pBlip, GetBlipTypeValue(pBlip) == BLIP_TYPE_RADIUS || GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA);

#if __BANK
			SetBlipDebugNumberValue(pBlip, -1);

			// if we currently want to have all standard ped blips as stealth then set them here
			if (bAllPedBlipsAsStealth)
			{
				if (iBlipType == BLIP_TYPE_CHAR)
				{
					SetBlipTypeValue(pBlip, BLIP_TYPE_STEALTH);
				}
			}
#endif // __BANK
		}

		// set the appropriate default name for the blip
		switch (iBlipType)
		{
			case BLIP_TYPE_COORDS:
				SetBlipNameValue(pBlip, "BLIP_DEST", true);
				break;
			case BLIP_TYPE_CONTACT:
				SetBlipNameValue(pBlip, "BLIP_CONT", true);
				break;
			case BLIP_TYPE_OBJECT:
				SetBlipNameValue(pBlip, "BLIP_OBJ", true);
				break;
			case BLIP_TYPE_PICKUP_OBJECT :
				SetBlipNameValue(pBlip, "BLIP_PICK_OBJ", true);
				break;
			case BLIP_TYPE_CAR:
				SetBlipNameValue(pBlip, "BLIP_CAR", true);
				break;
			case BLIP_TYPE_CHAR:
				SetBlipNameValue(pBlip, "BLIP_ENEMY", true);
				break;
			case BLIP_TYPE_CUSTOM:
				SetBlipNameValue(pBlip, "BLIP_GALLERY", true);
				break;
			case BLIP_TYPE_PICKUP:
				SetBlipNameValue(pBlip, "BLIP_PICK", true);
				break;
			case BLIP_TYPE_WEAPON_PICKUP:
				SetBlipNameValue(pBlip, "BLIP_PICK", true);
				break;
			case BLIP_TYPE_COP:
				SetBlipNameValue(pBlip, "BLIP_COP", true);
				break;
			case BLIP_TYPE_STEALTH:
				SetBlipNameValue(pBlip, "BLIP_ENEMY", true);
				break;
			default:
				SetBlipNameValue(pBlip, "BLIP_DEST", true);
				break;
		}

		if(    (iBlipType == BLIP_TYPE_CAR)
			|| (iBlipType == BLIP_TYPE_CHAR)
			|| (iBlipType == BLIP_TYPE_STEALTH)
			|| (iBlipType == BLIP_TYPE_OBJECT)
			|| (iBlipType == BLIP_TYPE_PICKUP_OBJECT)
			|| (iBlipType == BLIP_TYPE_COP))
		{
			CEntity* pEntity = iPoolIndex.GetEntity((eBLIP_TYPE)iBlipType);
			if(pEntity && pEntity->GetIsPhysical())
			{
				Assertf(!smart_cast<CPhysical*>(pEntity)->GetCreatedBlip(), "Creating duplicate %s blips for %s %s from script: %s.  Please call REMOVE_BLIP() first.", 
					GetBlipNameValue(pBlip), pEntity->GetDebugName(), pEntity->GetModelName(), pScriptName);
				smart_cast<CPhysical*>(pEntity)->SetCreatedBlip(true);
			}
		}

		// set specific details for certain types of blip:
		switch (iBlipType)
		{
			case BLIP_TYPE_WEAPON_PICKUP:
			{
				SetBlipPositionValue(pBlip, vPosition);

				if (bComplex)
				{
					SetBlipColourValue(pBlip, BLIP_COLOUR_COORD);
					SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
				}
				break;
			}

			case BLIP_TYPE_STEALTH:
			{
				SetBlipPositionValue(pBlip, vPosition);

#if __BANK
				if (iDebugSonarStyle == SONAR_STYLE_SONAR_ONLY)
				{
					if (GetBlipDisplayValue(pBlip) != BLIP_DISPLAY_NEITHER)
					{
						SetBlipDisplayValue(pBlip, BLIP_DISPLAY_NEITHER);
					}
				}
#endif // __BANK

				if (bComplex)
				{
					SetBlipPoolIndexValue(pBlip, iPoolIndex);
					SetBlipColourValue(pBlip, BLIP_COLOUR_RED);
					SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
					SetBlipScaleValue(pBlip, 0.4f);
				}
				break;
			}

			case BLIP_TYPE_COORDS:
			case BLIP_TYPE_CONTACT:
			{
				SetBlipPositionValue(pBlip, vPosition);

				if (bComplex)
				{
					SetBlipColourValue(pBlip, BLIP_COLOUR_COORD);
					SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
				}
				break;
			}

			case BLIP_TYPE_RADIUS:
			{
				SetBlipObjectName(pBlip, BLIP_RADIUS);  // radius blip style
				SetBlipPositionValue(pBlip, vPosition);
				SetBlipPriorityValue(pBlip, BLIP_PRIORITY_LOW);
				SetFlag(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND);
				break;
			}

			case BLIP_TYPE_AREA:
			{
				SetBlipObjectName(pBlip, BLIP_LEVEL); // default style, we use BLIP_AREA exclusively when we render. Let's us override it later for edge blips
				SetBlipPositionValue(pBlip, vPosition);
				SetBlipPriorityValue(pBlip, BLIP_PRIORITY_LOW);
				SetFlag(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND);
				break;
			}
			case BLIP_TYPE_CUSTOM:
			{
				SetBlipObjectName(pBlip, sMiniMapMenuComponent.GetBlipObject());
				if (sMiniMapMenuComponent.GetBlipObject() == 144)
				{
					SetBlipColourValue(pBlip, BLIP_COLOUR_RED);
				}
				else if ( sMiniMapMenuComponent.GetBlipObject() == 145)
				{
					SetBlipColourValue(pBlip, BLIP_COLOUR_GREEN);
				}

				SetBlipPositionValue(pBlip, vPosition);
				SetBlipPoolIndexValue(pBlip, iPoolIndex);
				SetFlag(pBlip, BLIP_FLAG_SHORTRANGE);
				
				break;
			}
			case BLIP_TYPE_OBJECT:
			case BLIP_TYPE_PICKUP_OBJECT :
			{
				uiAssert(pBlip->IsComplex());

				SetBlipColourValue(pBlip, BLIP_COLOUR_MISSION_OBJECT);
				SetBlipPoolIndexValue(pBlip, iPoolIndex);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
				break;
			}

			case BLIP_TYPE_PICKUP:
			{
				uiAssert(pBlip->IsComplex());

				SetBlipColourValue(pBlip, BLIP_COLOUR_GREEN);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);

				SetBlipPoolIndexValue(pBlip, iPoolIndex);
				break;
			}

			case BLIP_TYPE_COP:
			{
				SetFlag(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND);
				SetBlipPoolIndexValue(pBlip, iPoolIndex);
				break;
			}
			default:
			{
				uiAssert(pBlip->IsComplex());

				SetBlipColourValue(pBlip, BLIP_COLOUR_FRIENDLY);
				SetBlipPoolIndexValue(pBlip, iPoolIndex);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
				break;
			}
		}

#if __DEV
		if (pScriptName)
		{
			formatf(blip[iIndex]->m_cBlipCreatedByScriptName, pScriptName);
		}
#endif // __DEV

		blip[iIndex]->m_iActualId = iIndex;
		blip[iIndex]->m_iUniqueId = CreateNewUniqueBlipFromActualBlipId(iIndex);  // store the unique id that was used

//	Graeme - hopefully it's okay to skip this. The GfxValue is stored on the RenderThread so I can't set it here
//		SetBlipGfxValue(pBlip, NULL);  // set blip gfx value as null once we have a unique id to handle

		if (CPauseMenu::IsActive() && CPauseMenu::IsInMapScreen())
		{
#if __DEV
			uiDebugf1("Updating pausemap legend because '%s' blip created by '%s'", GetBlipNameValue(pBlip), blip[iIndex]->m_cBlipCreatedByScriptName);
#else
			uiDebugf1("Updating pausemap legend because '%s' blip created", GetBlipNameValue(pBlip));
#endif // __DEV
			CPauseMenu::UpdatePauseMapLegend();
		}

		uiDebugf1("Created '%s' blip as Actual id: %d and unique id %d", GetBlipNameValue(pBlip), blip[iIndex]->m_iActualId, blip[iIndex]->m_iUniqueId);

		return (blip[iIndex]->m_iUniqueId);
	}

	return INVALID_BLIP_ID;
}



///////////////////////////////////////////////////////////////////////////////////////
// NAME:	RemoveBlip
// PURPOSE:	sets a request to remove the blip from the stage and eventually destory it
///////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::RemoveBlip(CMiniMapBlip *pBlip)
{
	if (!pBlip)
		return false;

	SetFlag(pBlip, BLIP_FLAG_VALUE_REMOVE_FROM_STAGE);
#if COMPANION_APP
	CCompanionData::GetInstance()->RemoveBlip((CBlipComplex*)pBlip);
#endif
	return true;
}



///////////////////////////////////////////////////////////////////////////////////////
// NAME:	DestroyBlipObject
// PURPOSE:	actually destroys the blip object - should be removed from the stage 1st
///////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::DestroyBlipObject(CMiniMapBlip *pBlip)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap:: DestroyBlipObject can only be called on the UpdateThread!");
		return;
	}

	if (!pBlip)
		return;

	CGps::StopRadarBlipGps(GetUniqueBlipUsed(pBlip));  // remove any GPS that may be attached this blip

	if (CPauseMenu::IsActive() && CMiniMap::GetBlipTypeValue(pBlip) != BLIP_TYPE_CUSTOM )
	{
		uiDebugf1("Updating pausemap legend because '%s' blip was removed", GetBlipNameValue(pBlip));
		CMapMenu::OnBlipDelete(pBlip);
		CPauseMenu::UpdatePauseMapLegend();
	}
#if __DEV
	if (pBlip->IsComplex())
		iSizeOfBlipsCreated -= (u32)sizeof(CBlipComplex);
	else
		iSizeOfBlipsCreated -= (u32)sizeof(CBlipSimple);

	iNumCreatedTraces--;

	if (pBlip->IsComplex())
	{
		uiDebugf1("MiniMap: Blip Deleted (%s)   Num Blips Active: %d    Size: %d", GetBlipNameValue(pBlip), iNumCreatedTraces, iSizeOfBlipsCreated);
	}
#endif

	s32 iActualId = GetActualBlipUsed(pBlip);

	delete blip[iActualId];
	blip[iActualId] = NULL;

	// if this blip was the blip that is the maximum blip, then we can shorten the max list by 1
	if (iActualId+1 == iMaxCreatedBlips)
	{
		iMaxCreatedBlips--;

		uiDebugf1("MiniMap: Max blip count decreased to %d", iMaxCreatedBlips);
	}
	else
	{
		bool bFoundBlip = false;
		// if not, then run through between this blip and the current max, looking to see if there are any blips left, if not set the max to be this blip
		for (s32 i = iMaxCreatedBlips-1; ((!bFoundBlip) && i > iActualId); i--)
		{
			if (blip[i])
			{
				iMaxCreatedBlips = i + 1;
				uiDebugf1("MiniMap: Max blip count recalculated to %d", iMaxCreatedBlips);
				bFoundBlip = true;
			}
		}

		if (!bFoundBlip)
		{
			uiDebugf1("MiniMap: Max blip count stayed static at %d", iMaxCreatedBlips);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::CreateSonarBlip
// PURPOSE: creates a self-removing "sonar" stealth blip.   We do not need to keep
//			track of these as these blips self-remove via ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::CreateSonarBlip(const Vector3& vPos, float fSize, eHUD_COLOURS iHudColour, CPed* pPed /*= NULL*/, bool bScriptRequested, bool overrideBlipPos)
{
	PF_FUNC(AddSonarBlip);
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap:: CreateSonarBlip can only be called on the UpdateThread!");
		return;
	}

	if (!ms_bSonarBlipsActive)
	{
#if __BANK
		if (bDebugVisibilityOfSonarBlips)
		{
			uiDisplayf("MiniMap: SONAR_BLIP - Not adding sonar blip because ms_bSonarBlipsActive");
		}
#endif // __BANK
		return;
	}

	if ( (!bScriptRequested) && (pPed != GetMiniMapPed()) )  // fix for 1162373
	{
#if __BANK
		if (bDebugVisibilityOfSonarBlips)
		{
			uiDisplayf("MiniMap: SONAR_BLIP - Not adding sonar blip because ( (!bScriptRequested) && (pPed != GetMiniMapPed()) )");
		}
#endif // __BANK
		
		return;
	}

	if (!ShouldAddSonarBlips())
	{
		// debug is added in the ShouldAddSonarBlips function
		return;
	}
		
	if (fSize > 0.0f)
	{
		bool bFoundExistingSlot = false;
		bool bHasRedBlipActive = CHudColour::IsHudColourRed(iHudColour);

		for (s32 i = 0; i < ms_SonarBlips.GetCount(); i++)
		{
			if (ms_SonarBlips[i].pPed == pPed)
			{
				bHasRedBlipActive |= CHudColour::IsHudColourRed(ms_SonarBlips[i].iHudColour);
			}
		}

		if ((CHudColour::IsHudColourRed(iHudColour) || !bHasRedBlipActive))
		{
		for (s32 i = 0; ((!bFoundExistingSlot) && (i < ms_SonarBlips.GetCount())); i++)
		{
			if (ms_SonarBlips[i].pPed == pPed && ms_SonarBlips[i].iHudColour == iHudColour)  // reuse the existing slot so the blip can move with the ped but we dont need to store too much info about it
			{
#if __BANK
				if (bDebugVisibilityOfSonarBlips)
				{
						uiDisplayf("MiniMap: SONAR_BLIP - Reusing sonar blip %d", i);
				}
#endif
				s32 iFrameCheck = (s32)(floor)(ms_SonarBlips[i].fSize / 5.0f);  // work out how long to allow the existing sized blip to continue before re-sizing
				if ( (fSize * 2.0f >= ms_SonarBlips[i].fSize) || (ms_SonarBlips[i].iFramesAlive > iFrameCheck) )  // allow any existing anim a few frames before we restart it on a sound
				{
					ms_SonarBlips[i].fSize = fSize * 2.0f;
					ms_SonarBlips[i].iFramesAlive = 0;
				}

				ms_SonarBlips[i].vPos = Vector2(vPos.x, vPos.y);

				ms_SonarBlips[i].bShouldOverridePos = overrideBlipPos;

				bFoundExistingSlot = true;

				break;
			}
		}

		if (!bFoundExistingSlot)
		{
#define SONAR_BLIP_ID_START	(0)
#define SONAR_BLIP_ID_RANGE	(1000)

			static s32 iSonarBlipIdCounter = SONAR_BLIP_ID_START;

			sSonarBlipStruct newSonarItem;

			newSonarItem.iId = iSonarBlipIdCounter++;
			newSonarItem.pPed = pPed;
			newSonarItem.vPos = Vector2(vPos.x, vPos.y);
			newSonarItem.fSize = fSize * 2.0f;
			newSonarItem.iHudColour = iHudColour;
			newSonarItem.iFramesAlive = 0;
			newSonarItem.bShouldOverridePos = overrideBlipPos;

			ms_SonarBlips.PushAndGrow(newSonarItem);

#if __BANK
			if (bDebugVisibilityOfSonarBlips)
			{
				uiDisplayf("MiniMap: SONAR_BLIP - Creating new sonar blip %d", newSonarItem.iId);
			}
#endif

			if (iSonarBlipIdCounter > (SONAR_BLIP_ID_START+SONAR_BLIP_ID_RANGE))
				iSonarBlipIdCounter = SONAR_BLIP_ID_START;
		}
	}
	}
}

// Reduces the noise passed in for environmental factors, does ReportStealthNoise if pPed is a player,
//  then optionally creates a sonar blip for the noise.
float CMiniMap::CreateSonarBlipAndReportStealthNoise(CPed* pPed, const Vec3V& vPos, float fSize, eHUD_COLOURS iHudColor, bool bCreateSonarBlip, bool overrideBlipPos)
{
	// Current design is to not draw sonar blips for non-player characters [2/4/2013 mdawe]
	if (uiVerify(pPed) && fSize > 0.f && pPed->IsPlayer())
	{
		ReduceSoundSizeForEnvironment(fSize, vPos, pPed);
		CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
		if (pPlayerInfo)
		{
			fSize *= pPlayerInfo->GetStealthMultiplier();
			pPlayerInfo->ReportStealthNoise(fSize);
		}
		if (bCreateSonarBlip)
		{
			CreateSonarBlip(VEC3V_TO_VECTOR3(vPos), fSize, iHudColor, pPed, false, overrideBlipPos);
		}
	}
	return fSize;
}

void CMiniMap::ClearSonarBlips()
{
	ms_SonarBlips.resize(0);
}

// Under certain conditions, reduce the size of this sound event.
void CMiniMap::ReduceSoundSizeForEnvironment( float& fInOutSize, Vec3V_In vPos, const CPed* pPed )
{
#if __BANK && DEBUG_DRAW
	// We will use these variables below if (CPedDebugVisualiser::GetDebugDisplay() == CPedDebugVisualiser::eStealth)
	int iNoOfTexts = 3; // 3 lines of text in VisualizeStealth
	Vector3 vTextPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + ZAXIS;
#endif //__BANK && DEBUG_DRAW

	if (g_weather.IsRaining())
	{
		fInOutSize -= CMiniMap::sm_Tunables.Sonar.fRainSnowSoundReductionAmount;
#if	__BANK && DEBUG_DRAW
		if (CPedDebugVisualiser::GetDebugDisplay() == CPedDebugVisualiser::eStealth)
		{
			char debugText[50];
			sprintf(debugText, "Player sound reduced for RAIN from %f to %f", fInOutSize + CMiniMap::sm_Tunables.Sonar.fRainSnowSoundReductionAmount,fInOutSize);
			grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText);
			iNoOfTexts++;
		}
#endif //__BANK && DEBUG_DRAW
	}
	
	if (pPed && pPed->IsPlayer())
	{
		// If nearby a playing radio or other peds conversing, reduce the sound.
		if (g_EmitterAudioEntity.IsWithinRangeOfRadio(VEC3V_TO_VECTOR3(vPos), CMiniMap::sm_Tunables.Sonar.fRadioSoundReductionDistance))
		{
			fInOutSize -= CMiniMap::sm_Tunables.Sonar.fRadioSoundReductionAmount;
		}
		else if (pPed->GetPedIntelligence())
		{
			// Go through nearby vehicles to look for radios playing.
			CEntityScannerIterator vehicleList = pPed->GetPedIntelligence()->GetNearbyVehicles();
			ScalarV vSoundReductionDist2(rage::square(CMiniMap::sm_Tunables.Sonar.fRadioSoundReductionDistance));
			for( CEntity* pEnt = vehicleList.GetFirst(); pEnt; pEnt = vehicleList.GetNext() )
			{
				CVehicle* pThisVehicle = static_cast<CVehicle*>(pEnt);
				const audVehicleAudioEntity* pVehAudioEntity = pThisVehicle->GetVehicleAudioEntity();
				if (pVehAudioEntity)
				{
					if (pVehAudioEntity->GetRadioStation() && pVehAudioEntity->IsRadioSwitchedOn())
					{
						ScalarV positionDist2 = DistSquared(pThisVehicle->GetTransform().GetPosition(), vPos);
						if ( IsLessThanAll(positionDist2, vSoundReductionDist2) )
						{
							fInOutSize -= CMiniMap::sm_Tunables.Sonar.fRadioSoundReductionAmount;
#if	__BANK && DEBUG_DRAW
							if (CPedDebugVisualiser::GetDebugDisplay() == CPedDebugVisualiser::eStealth)
							{
								char debugText[50];
								sprintf(debugText, "Player sound reduced for RADIO from %f to %f", fInOutSize + CMiniMap::sm_Tunables.Sonar.fRadioSoundReductionAmount,fInOutSize);
								grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText, true, 33);
								grcDebugDraw::Line(vTextPos, VEC3V_TO_VECTOR3(pThisVehicle->GetTransform().GetPosition()), Color_BlanchedAlmond, 33);
								iNoOfTexts++;
							}
#endif // __BANK && DEBUG_DRAW
							return;
						}
					}
				}
			}

			// If we haven't already found a radio, check for nearby conversations.
			vSoundReductionDist2.Set(rage::square(CMiniMap::sm_Tunables.Sonar.fConversationSoundReductionDistance));
			CEntityScannerIterator pedList = pPed->GetPedIntelligence()->GetNearbyPeds();
			for( CEntity* pEnt = pedList.GetFirst(); pEnt; pEnt = pedList.GetNext() )
			{
				CPed* pPed = static_cast<CPed*>(pEnt);
				if (pPed)
				{
					if (pPed->GetSpeechAudioEntity() && pPed->GetSpeechAudioEntity()->GetIsAnySpeechPlaying())
					{
						ScalarV positionDist2 = DistSquared(pPed->GetTransform().GetPosition(), vPos);
						if ( IsLessThanAll(positionDist2, vSoundReductionDist2) )
						{
							fInOutSize -= CMiniMap::sm_Tunables.Sonar.fConversationSoundReductionAmount;
#if	DEBUG_DRAW
							if (CPedDebugVisualiser::GetDebugDisplay() == CPedDebugVisualiser::eStealth)
							{
								char debugText[50];
								sprintf(debugText, "Player sound reduced for CONVERSATION from %f to %f", fInOutSize + CMiniMap::sm_Tunables.Sonar.fConversationSoundReductionAmount,fInOutSize);
								grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText, true, 10);
								iNoOfTexts++;
							}
#endif
							return;
						}
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	FindAndCreateNextEmptyBlipSpace()
// PURPOSE:	returns the 1st empty/unused blip space we can find in the array
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::FindAndCreateNextEmptyBlipSpace(bool bComplex)
{
	// find a new slot in the blip array:
	for (s32 iIndex = 0; iIndex < MAX_NUM_BLIPS; iIndex++)
	{
		if (!blip[iIndex])
		{
			if (bComplex)
			{
				blip[iIndex] = rage_new CBlipComplex();  // lets point this to a complex blip
#if __DEV
				iSizeOfBlipsCreated += (u32)sizeof(CBlipComplex);
				iNumCreatedTraces++;
				uiDebugf1("MiniMap: New Complex Blip Created!   Num Blips Active: %d    Size: %d   Slot: %d\n", iNumCreatedTraces, iSizeOfBlipsCreated, iIndex);
#endif
			}
			else
			{
				blip[iIndex] = rage_new CBlipSimple();  // lets point this to a simple blip#
#if __DEV
				iSizeOfBlipsCreated += (u32)sizeof(CBlipSimple);
				iNumCreatedTraces++;
				uiDebugf1("MiniMap: New Simple Blip Created!   Num Blips Active: %d    Size: %d   Slot: %d\n", iNumCreatedTraces, iSizeOfBlipsCreated, iIndex);
#endif
			}

/*#define MAX_TRIAL_BUG_COUNT (150)  // I want to trial a smaller amount of blips - so this assert will catch it if 150 isnt enough
			if (iIndex == MAX_TRIAL_BUG_COUNT)
			{
				uiAssertf(0, "MiniMap: Trial max of %d blips has been hit (can be safely ignored but please bug to DerekP, continue playing and include logs at end of session)", MAX_TRIAL_BUG_COUNT);
			}*/

			if ( (iIndex + 1) > iMaxCreatedBlips )
			{
				iMaxCreatedBlips = (iIndex + 1);

				uiDebugf1("MiniMap: Max blip count increased to %d", iMaxCreatedBlips);
			}

			return (iIndex);
		}

	
	}

	uiAssertf(0, "MiniMap: Cannot find a free blip slot - maximum has been hit!");

	return -1;
}



////////////////////////////////////////////////////////////////////////////
// name:	CreateNewUniqueBlipFromActualBlipId
//
// purpose: Increases the Reference Index of the entry in the blip array and
//			adds this Reference Index to the Array Index
//			(Call this when first setting up a blip)
// params:  Index without Reference Index (i.e. Array Index)
// returns: Index including Reference Index
////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::CreateNewUniqueBlipFromActualBlipId(s32 iIndex)
{
	s32 iUniqueIndex = INVALID_BLIP_ID;

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap: CreateNewUniqueBlipFromActualBlipId can only be called on the UpdateThread!");
		return iUniqueIndex;
	}

	if (iIndex != -1 && blip[iIndex])
	{
		if (iReferenceCount < (MAX_BLIP_REFERENCE_INDEX - 1))
			iReferenceCount++;
		else
			iReferenceCount = 1;

		blip[iIndex]->m_iReferenceIndex = iReferenceCount;
		iUniqueIndex = iIndex | (blip[iIndex]->m_iReferenceIndex << BLIP_REFERENCE_SHIFT);
	}

	return (iUniqueIndex);
}



////////////////////////////////////////////////////////////////////////////
// name:	ConvertUniqueBlipToActualBlip
//
// purpose: Checks the Reference Index part of Index and then removes it so
//			that the remainder of Index can be treated as an index
//			into the blip array
// params:  Index including Reference Index
// returns: Index without Reference Index (i.e. Array Index)
//			or -1 if the Reference Index shows that Index refers to an old blip
////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::ConvertUniqueBlipToActualBlip(s32 iIndex)
{
	u32 iReferenceIndexOfThisBlip;
	s32 ArrayIndex;

	if (iIndex != INVALID_BLIP_ID)
	{
		ArrayIndex = (iIndex & ~BLIP_REFERENCE_MASK);

		iReferenceIndexOfThisBlip = (iIndex & BLIP_REFERENCE_MASK) >> BLIP_REFERENCE_SHIFT;

		uiAssertf(ArrayIndex >= 0, "MiniMap: ConvertUniqueBlipToActualBlip - Array Index is less than 0");
		uiAssertf(ArrayIndex < MAX_NUM_BLIPS, "MiniMap: ConvertUniqueBlipToActualBlip - Radar Blip Array Index is too big (%d)", ArrayIndex);

		if (blip[ArrayIndex] && iReferenceIndexOfThisBlip == blip[ArrayIndex]->m_iReferenceIndex)
		{
			return (ArrayIndex);
		}
	}

	return(-1);
}



////////////////////////////////////////////////////////////////////////////
// name:	UpdateBitmapOnUT
//
// purpose: deals with placing bitmap tiles onto the map as LOD
//			still need to make the 'active' tile selection a bit more
//			intellegent so we only render tiles we need to.
////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateBitmapOnUT(MM_BITMAP_VERSION mmOverride /* = MAX_MM_BITMAP_VERSIONS */)
{
	if (!ms_bBitmapInitialized) return;

	Vector3 vCentrePos;
	GetCentrePosition(vCentrePos);
	Vector2 vTestPos(vCentrePos.x - sm_Tunables.Bitmap.vBitmapStart.x, -(vCentrePos.y - sm_Tunables.Bitmap.vBitmapStart.y - sm_Tunables.Bitmap.vBitmapTileSize.y));
	Vector2 vBitmapTile((sm_Tunables.Bitmap.iBitmapTilesX / (sm_Tunables.Bitmap.iBitmapTilesX * sm_Tunables.Bitmap.vBitmapTileSize.x)) * vTestPos.x, (sm_Tunables.Bitmap.iBitmapTilesY / (sm_Tunables.Bitmap.iBitmapTilesY * sm_Tunables.Bitmap.vBitmapTileSize.y)) * vTestPos.y);

	float fCurrentTileX = vBitmapTile.x;
	float fCurrentTileY = vBitmapTile.y-1;

#define TILE_RANGE_CHECK (0.4f)
	//	get the max tiles we need around the player
	float CurrentTopTile = rage::Max((floorf(fCurrentTileY - TILE_RANGE_CHECK)), 0.0f);
	float CurrentBottomTile = rage::Min((ceilf(fCurrentTileY + TILE_RANGE_CHECK)), (float)(sm_Tunables.Bitmap.iBitmapTilesY-1));
	float CurrentLeftTile = rage::Max((floorf(fCurrentTileX - TILE_RANGE_CHECK)), 0.0f);
	float CurrentRightTile = rage::Min((ceilf(fCurrentTileX + TILE_RANGE_CHECK)), (float)(sm_Tunables.Bitmap.iBitmapTilesX-1));

	bool bInUseThisFrame = false;

	char cBitmapString[50];

	UpdateBitmapState(ms_BitmapSuperLowRes, true, !IsInPauseMap() && !IsInCustomMap(), "minimap_LOD_128");

	for (s32 i = 0; i < MM_BITMAP_VERSION_NUM_ENUMS; i++)
	{
		const char* pszPrefix = NULL;
		switch(i)
		{
		case MM_BITMAP_VERSION_MINIMAP: pszPrefix = "minimap"; break;
		case MM_BITMAP_VERSION_ALPHA: pszPrefix = "minimap_alpha"; break;
		case MM_BITMAP_VERSION_SEA: pszPrefix = "minimap_sea"; break;
		default: pszPrefix = NULL;	break;
		}

		for (s32 x = 0; x < sm_Tunables.Bitmap.iBitmapTilesX; x++)
		{
			for (s32 y = 0; y < sm_Tunables.Bitmap.iBitmapTilesY; y++)
			{
				if( mmOverride != MM_BITMAP_VERSION_NUM_ENUMS )
				{
					bInUseThisFrame = (i == mmOverride);
				}
				else if (IsInPauseMap() || IsInCustomMap())
				{
					if (i == sm_Tunables.Bitmap.eBitmapForPause)
					{
						bInUseThisFrame = true;
					}
					else
					{
						bInUseThisFrame = false;
					}
				}
				else
				{
					if (i == sm_Tunables.Bitmap.eBitmapForMinimap && 
						(IsInBigMap() || 
						GetRange() < sm_Tunables.Camera.fBitmapRequiredZoom || 
						fwTimer::GetTimeInMilliseconds() <= iMiniMapDpadZoomTimer+TIME_TO_ZOOM_OUT_MINIMAP+1000 || 
						sm_Tunables.Bitmap.bAlwaysDrawBitmap)
						)
					{
						bInUseThisFrame = true;
					}
					else
					{
						bInUseThisFrame = false;
					}

					if ( bInUseThisFrame && !IsInBigMap())  // fix for 1353935
					{
						if (x < CurrentLeftTile)
							bInUseThisFrame = false;

						if (x > CurrentRightTile)
							bInUseThisFrame = false;

						if (y < CurrentTopTile)
							bInUseThisFrame = false;

						if (y > CurrentBottomTile)
							bInUseThisFrame = false;
					}
				}
				
				formatf(cBitmapString, "%s_%d_%d", pszPrefix, y, x, NELEM(cBitmapString));

				sBitmapStruct& bitmap = ms_BitmapBackground(i,x,y);
				UpdateBitmapState(bitmap, bInUseThisFrame, bInUseThisFrame, cBitmapString);

			}
		}
	}
	
}

bool CMiniMap::AreAllTexturesActive(MM_BITMAP_VERSION mm)
{
	UpdateBitmapOnUT(mm);
	for (s32 x = 0; x < sm_Tunables.Bitmap.iBitmapTilesX; x++)
	{
		for (s32 y = 0; y < sm_Tunables.Bitmap.iBitmapTilesY; y++)
		{
			if( ms_BitmapBackground(mm,x,y).iState != MM_BITMAP_ACTIVE )
				return false;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::Update
// PURPOSE:	updates the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateMiniMap()
{
	if (!bActive)
		return;

	STRVIS_AUTO_CONTEXT(strStreamingVisualize::MINIMAP);

	UpdateDpadDown();

	UpdateMultiplayerInformation();

	UpdateStatesOnUT();

	UpdateBitmapOnUT();

	UpdateBlips();  // this needs to update on UT but call DLC commands to be run on RT later

//	UpdateBlipMarkers();  // added back in for 820689 but commented out pending decisions

	UpdateGPS();

	// Report the position & size to gamestream.
	CGameStream* GameStream = CGameStreamMgr::GetGameStream();
	if( GameStream != NULL )
	{
		Vector2 vPos = GetCurrentMiniMapPosition();
		Vector2 vSize = GetCurrentMiniMapSize();
		GameStream->ReportMinimapPosAndSize( vPos.x, vPos.y, vSize.x, vSize.y );
	}

#if __BANK
	if (!IsInPauseMap())
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_N, KEYBOARD_MODE_DEBUG, "show blip names"))
		{
			bDisplayAllBlipNames = !bDisplayAllBlipNames;

			ModifyLabelsToAllActiveBlips(bDisplayAllBlipNames, false);
		}
	}

	if( displayRevealFoWRatio )
	{
		float ratio = CMiniMap_Common::GetFogOfWarDiscoveryRatio();
		grcDebugDraw::AddDebugOutput("FoW Discovery Ratio %f",ratio);
	}
	
	if( testRevealFoWCoord )
	{
		bool revealed = CMiniMap_Common::IsCoordinateRevealed(Vector3(testRevealX,testRevealY,0.0f));
		grcDebugDraw::AddDebugOutput("%fx%f : %s",testRevealX,testRevealY,revealed ? "Been" : "Not Been");
	}
	
#endif // __BANK
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetMiniMapPed
// PURPOSE: returns the ped the minimap is running from (usually player ped unless
//			specified by script (ie spectator in MP games))
/////////////////////////////////////////////////////////////////////////////////////
CPed *CMiniMap::GetMiniMapPed()
{
	if (ms_iMiniMapPedPoolIndex != 0)
	{
		CPed *pPed = CPed::GetPool()->GetAt(ms_iMiniMapPedPoolIndex);

		if (pPed)
		{
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

			return (pPed);
		}
	}

	uiAssertf(CGameWorld::FindLocalPlayer(), "CMiniMap::GetMiniMapPed - failed to get a valid ped to base the minimap on");

	return CGameWorld::FindLocalPlayer();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetIsInsideInterior
// PURPOSE: whether the minimap thinks its in an interior or not
//			can be called from either thread
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::GetIsInsideInterior(bool bIgnorePauseMapSettings)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::GetIsInsideInterior can only be called on the UpdateThread!");
		return false;
	}

	if ( (!bIgnorePauseMapSettings) && (IsInPauseMap()) )
	{
		return (CMapMenu::GetIsInInteriorMode());
	}

	if (CScriptHud::ms_fMiniMapForcedZoomPercentage == 0.0f)
	{
		if (CScriptHud::ms_bFakeExteriorThisFrame)
		{
			return false;
		}
	}
	
	bool bInsideInterior = false;

	if (ms_bInPrologue || ms_bOnIslandMap)  // as prologue is now always an interior throughout we have a slightly different check to ensure it all still works as expected
	{
		const CPed *pLocalPlayer = GetMiniMapPed();
		CPortalTracker* pPT = const_cast<CPortalTracker*>(pLocalPlayer ? pLocalPlayer->GetPortalTracker() : NULL);

		bInsideInterior = ( (CScriptHud::FakeInterior.bActive) || ( (pPT) && (pPT->IsInsideInterior()) && (!pPT->IsAllowedToRunInInterior()) ) );
	}
	else  // general game checks movie being active on the stage:
	{
		if (!CScriptHud::ms_bFakeExteriorThisFrame) // fix for 1500837 - script are faking an interior but also faking an exterior, so they can have the map and zoom it out so I know not to zoom in here!
		{
			// Fix for 1623055 - return that we are not in an interior as soon as script stop faking an interior
			// if fake interior is active, then return true.  if its not anymore but was, return false.  if the interior was not set up
			// from a fake interior, then return based on the current movie settings as was done previously
			if (CScriptHud::FakeInterior.bActive)
			{
				bInsideInterior = true;
			}
			else if (!CMiniMap_RenderThread::IsInteriorSetupFromScript())
			{
				bInsideInterior = 	( (CMiniMap_RenderThread::IsInteriorMovieActiveAndSetupOnStage()) ||
									  ((CMiniMap_RenderThread::IsInteriorMovieActive()) && (!IsInPauseMap())) );
			}
		}
	}

	return bInsideInterior;
}

void CMiniMap::ExitPauseMapState()
{
	SetMinimapModeState(GetCurrentGolfMap() == GOLF_COURSE_OFF ? MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP : MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP);
}

void CMiniMap::SetMinimapModeState(eSETUP_STATE newState, bool bForceIt /*= false*/)
{
	if (MinimapModeState != MINIMAP_MODE_STATE_NONE)
	{
		// if we haven't finished current transition, then store up latest request and apply that once current
#if !__FINAL
		if(CMiniMap_Common::OutputDebugTransitions())
		{
			if (ms_pendingMinimapModeState == MINIMAP_MODE_STATE_NONE)
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap::SetMinimapModeState - Already in a transition (%d) so set new transition (%d) as pending (UT)", MinimapModeState, newState);
			else
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap::SetMinimapModeState - Already in a transition (%d) so set new transition (%d) as pending replacing old pending value (%d)(UT)", MinimapModeState, newState, ms_pendingMinimapModeState);
		}
#endif

		ms_pendingMinimapModeState = newState;

		return;
	}

	if (newState == MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP && (IsInPauseMap()||IsInCustomMap()) && GetCurrentGolfMap() != GOLF_COURSE_OFF)  // if we are coming out of pausemenu and setting minimap active, we need to switch to golfmap setup if we currently have a valid golf map active
	{
		newState = MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP;
	}

	if (newState == MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP && IsInPauseMap() && WasInBigMap())  // if we are coming out of pausemenu and setting minimap active, we need to switch to bigmap setup if thats what was previously set up
	{
		newState = MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP;
	}

	if ( !bForceIt && newState == MinimapModeState)  // if already in that state, ignore any idential states as the transition will already be set to go
	{
		return;
	}

	switch (newState)
	{
	case MINIMAP_MODE_STATE_NONE :
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP :
		{
			SetInPauseMap(false);
			SetInBigMap(false);  // we are now in "minimap" so set bigmap to false
			SetInGolfMap(false);
			UpdateBitmapOnUT();

#if __BANK
			// switch off any blip names when leave pausemap
			if (!bDisplayAllBlipNames)
			{
				ModifyLabelsToAllActiveBlips(false, false);
			}
#endif
		}
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP :
		{
			// do not check the state of the golf map as we need to set it up again regardless as we need to setup any new course maps

			SetInGolfMap(true);
			SetInPauseMap(false);
			SetInBigMap(false);  // we are now in "minimap" so set bigmap to false

			UpdateBitmapOnUT();

#if __BANK
			// switch off any blip names when leave pausemap
			if (!bDisplayAllBlipNames)
			{
				ModifyLabelsToAllActiveBlips(false, false);
			}
#endif
		}
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP :
		{
			if ( !bForceIt && IsInPauseMap())  // just return out if already in pausemap
				return;

			SetInPauseMap(true);

			UpdateBitmapOnUT();

#if __BANK
			ModifyLabelsToAllActiveBlips(true, true);
#endif // __BANK
		}
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP:
		{
			SetInCustomMap(true);

			UpdateBitmapOnUT();

			//setup starting positions:
			Vector2 vFakePlayerPos;
			if (CScriptHud::GetFakedPauseMapPlayerPos(vFakePlayerPos))
			{
				SetPauseMapCursor(vFakePlayerPos);
			}
			else
			{
				CPed *pLocalPlayer = GetMiniMapPed();
				if (uiVerifyf(pLocalPlayer, "CMiniMap::SetMinimapModeState - failed to get a pointer to the local player"))
				{
					Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());
					SetPauseMapCursor(Vector2(vPlayerPos.x, vPlayerPos.y));
				}
			}

			CMapMenu::SetMapZoom(ZOOM_LEVEL_GALLERY, true);
		}
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP :
		{
			if ( !bForceIt && IsInBigMap())  // just return out if already in bigmap
				return;

			SetInBigMap(true);  // we are now in "bigmap" so set bigmap to true here
			SetInPauseMap(false);
			SetInGolfMap(false);
			UpdateBitmapOnUT();

#if __BANK
			// switch off any blip names when leave pausemap
			if (!bDisplayAllBlipNames)
			{
				ModifyLabelsToAllActiveBlips(false, false);
			}
#endif
		}
		break;
		
	case MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD :
	case MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD :
	case MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD :
	case MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD :
	case MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD :
		break;
	}

#if !__FINAL
	if(CMiniMap_Common::OutputDebugTransitions())
		uiDisplayf("MINIMAP_TRANSITION: CMiniMap::SetMinimapModeState(%d) [old state=%d] (UT)", newState, MinimapModeState);
#endif

	MinimapModeState = newState;  // return this state which will be sent over to RT to process

#if DEBUG_PAD_INPUT
	CDebug::UpdateDebugPadMoviePositions();
#endif // #if DEBUG_PAD_INPUT
}


void CMiniMap::UpdateMapPositionAndScaleOnUT(float &fReturnAngle, float &fReturnOffset, Vector3 &vReturnPosition, float &fReturnRange, s32 &iReturnRotation)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap::UpdateMapPositionAndScaleOnUT can only be called on the UpdateThread!");
		return;
	}

	fReturnAngle = 0.0f;
	fReturnOffset = 0.0f;

	if (!IsInPauseMap() && !IsInCustomMap())
	{
		if ( (!IsInBigMap()) && (!IsInGolfMap()) )
		{
			if (!CScriptHud::bDontTiltMiniMapThisFrame)
			{
				if (GetIsInsideInterior())
				{
					if (bMiniMap3D)  // only tilt minimap if rag has set it
					{
						fReturnAngle = -sm_Tunables.Camera.fInteriorFootTilt;
						fReturnOffset = sm_Tunables.Camera.fInteriorFootOffset;
					}
				}
				else
				{
					if (bMiniMap3D)  // only tilt minimap if rag has set it
					{
						// centered and from above if swimmin' or in a plane
						if( ms_bUseNewAwesomeAltimeter && (IsPlayerInPlaneOrHelicopter() /*|| IsPlayerUnderwaterOrInSub()*/))
						{
							if( GetMiniMapPed() && !GetMiniMapPed()->GetVehiclePedInside() )
								fReturnAngle = -sm_Tunables.Camera.fInteriorFootTilt;
							else
								fReturnAngle = -sm_Tunables.Camera.fAltimeterTilt;
							fReturnOffset = sm_Tunables.Camera.fAltimeterOffset;
						}
						else if (CGameWorld::FindLocalPlayerVehicle() || IsPlayerOnHorse())
						{
							// in vehicle
							fReturnAngle = -sm_Tunables.Camera.fVehicleTilt;
							fReturnOffset = sm_Tunables.Camera.fVehicleOffset;
						}
						else
						{
							// on foot
							fReturnAngle = -sm_Tunables.Camera.fExteriorFootTilt;
							fReturnOffset = sm_Tunables.Camera.fExteriorFootOffset;
						}
					}
				}
			}
		}
	}

#if __BANK
	if (fDebugTiltValue != -1.0f)
		fReturnAngle = -fDebugTiltValue;
 	if (fDebugOffsetValue != -1.0f)
 		fReturnOffset = fDebugOffsetValue;
#endif // __BANK


	GetCentrePosition(vReturnPosition);

	if ( (IsInPauseMap()) || (IsInCustomMap() && CScriptHud::ms_fRadarZoomDistanceThisFrame == 0.0f) )
	{
		fReturnRange = GetPauseMapScale();
	}
	else
	{
		fReturnRange = GetRange();
	}

	if (!IsInPauseMap() && !IsInCustomMap())
	{
#if !__FINAL
		if (camInterface::GetDebugDirector().IsFreeCamActive() BANK_ONLY( DEBUG_DRAW_ONLY( && !CMiniMap_RenderThread::sm_bDrawCollisionVolumes))  )
		{
			vReturnPosition = camInterface::GetPos();
			fReturnAngle = 1.0f;  // no angle in debug cam - a quick and simple fix for 1314669
			fReturnOffset = 0.0f;
		}
#endif // __FINAL

		if (IsInBigMap())
		{
			if (ms_bBigMapFullZoom)
			{
				vReturnPosition.x = BIGMAP_OFFSET_X;
				vReturnPosition.y = BIGMAP_OFFSET_Y;
			}
			else
			{
				fReturnAngle = sm_Tunables.Camera.fBigmapTilt;
				fReturnOffset = sm_Tunables.Camera.fBigmapOffset;
			}
		}

		if ( (GetSpeedOfPlayersVehicle() == 0.0f) && (fReturnRange < MAP_BLIP_ROUND_THRESHOLD) )
		{
			vReturnPosition.x = floor(vReturnPosition.x+0.5f);
			vReturnPosition.y = floor(vReturnPosition.y+0.5f);
		}
	}

	if ( (!IsInPauseMap()) && (!ms_bBigMapFullZoom) && (!IsInCustomMap()) )  // bigmap (non full zoom) can rotate as per Les request 614280
	{
		if (GetLockedAngle() == -1)
		{			
			CPed* pLocalPed = GetMiniMapPed();
			CTask* pTask = pLocalPed ? pLocalPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE) : NULL;
			bool bIsSkydiving = pTask && pTask->GetState() == CTaskParachute::State_Skydiving;

			s32 iCamHeading = (s32)(camInterface::ComputeMiniMapHeading() * RtoD);

			s32 iRotDelta = abs(iCamHeading - ms_iPreviousMapRotation);
			if(iRotDelta > 180)
			{
				iRotDelta = 360 - iRotDelta;
			}
        //@@: range CMINIMAP_UPDATEMAPPOSITIONANDSCALEONUT {
			if(bIsSkydiving && camInterface::IsRenderingFirstPersonShooterCamera() && iRotDelta > 20)
			{
				iReturnRotation = ms_iPreviousMapRotation;
			}
			else
			{
#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_ROTATEMINIMAP
                if (TamperActions::ShouldRotateMiniMap())
                {
                   iReturnRotation = CMiniMap_RenderThread::GetMiniMapRenderRotation()-1;
					if(iReturnRotation <= -180)
						iReturnRotation = 180; 
                }
                else
#endif
                    iReturnRotation = iCamHeading;
			}
		}
		else
		{
			iReturnRotation = GetLockedAngle();
		}
        //@@: } CMINIMAP_UPDATEMAPPOSITIONANDSCALEONUT        
	}
	else
	{
		iReturnRotation = 0;	//	declare this as an s32
	}

	ms_iPreviousMapRotation = iReturnRotation;

	// set static variables for use inside UpdateBlips
	ms_vMapPosition = vReturnPosition;

	// This max distance calculation was pulled from MiniMapRenderThread
	// And is used to cull blips that would not have been rendered
	if (fReturnAngle != 0.0f)  // if we have an angle
	{
		// use stored range value
		ms_fMaxDistance = MINIMAP_DISTANCE_SCALER_3D * (MIN_MINIMAP_RANGE/fReturnRange);  // more zoomed out you are, the less we need to restrict the check			

		ms_fMaxDistance *= 2.f;  // take the max possible distance
	}
	else  // ... or a 2D view?
	{
		ms_fMaxDistance = MINIMAP_DISTANCE_SCALER_2D * (MIN_MINIMAP_RANGE/fReturnRange);

		if ((IsInPauseMap() || IsInCustomMap()) && CMapMenu::GetCurrentZoomLevel() == MAX_ZOOMED_IN)  // increase amount for pausemap when very zoomed in
		{
			ms_fMaxDistance *= 2.f;  // take the max possible distance
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateAltimeter
// PURPOSE:	updates the altimeter
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateAltimeter(sMiniMapRenderStateStruct& out_Results)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap::UpdateAltimeter can only be called on the UpdateThread!");
		return;
	}

	out_Results.m_AltimeterMode = ALTIMETER_OFF;

	
	bool bPlaneIsStalling = false;

	if (!IsInPauseMap() && !IsInBigMap() && !IsInCustomMap())
	{
#define __MAX_HEIGHT_OF_VEHICLE	(2500.0f)  // ie planes, closely matches a "tunable" in Planes.cpp, FLIGHT_CEILING_PLANE, but momentum carries you above it
#define __MAX_HEIGHT_OF_NON_VEHICLE	(2500.0f)  // ie parachute
#define __MIN_HEIGHT_OF_ABYSS (-250.0f);

		out_Results.m_fMaxAltimeterHeight = __MAX_HEIGHT_OF_VEHICLE;
		out_Results.m_fMinAltimeterHeight = 0.0f; // sea level

		

		CPed *pLocalPlayer = GetMiniMapPed();
		Vector3 vPlayerPos( out_Results.m_vCentrePosition );
		CVehicle* pVehicle = NULL;
		if (pLocalPlayer)
		{
			pVehicle = pLocalPlayer->GetVehiclePedInside();
			// prefer vehicle position if we can get it
			if( pVehicle )
			{
				vPlayerPos = pVehicle->GetBoundCentre();
			}
		}

		bool bMinNeedsSetting = true;

		out_Results.m_bColourAltimeter = false;
		if (CScriptHud::fFakeMinimapAltimeterHeight != 0.0f)
		{
			out_Results.m_fMaxAltimeterHeight = CScriptHud::fFakeMinimapAltimeterHeight;
			out_Results.m_AltimeterMode = CScriptHud::fFakeMinimapAltimeterHeight > 0 ? ALTIMETER_FLYING : ALTIMETER_SWIMMING;

			out_Results.m_bColourAltimeter = CScriptHud::ms_bColourMinimapAltimeter;
			out_Results.m_AltimeterColor = CScriptHud::ms_MinimapAltimeterColor;
		}
		else
		{
			if (IsPlayerInPlaneOrHelicopter())
			{
				if (pVehicle && pVehicle->InheritsFromPlane())
				{
					bPlaneIsStalling = verify_cast<CPlane*>(pVehicle)->IsStalling();
				}
						
				Vector2 minMaxHeight(0.0f, 0.0f);

				if (CVehiclePopulation::GetFlightHeightsFromFlightZones(vPlayerPos, minMaxHeight))
				{
					out_Results.m_fMinAltimeterHeight = minMaxHeight.x;
					out_Results.m_fMaxAltimeterHeight = minMaxHeight.y;
					bMinNeedsSetting = false;
				}
				else
				{
					out_Results.m_fMaxAltimeterHeight = __MAX_HEIGHT_OF_VEHICLE;
				}

				out_Results.m_AltimeterMode = ALTIMETER_FLYING;
			}
			else
			{
				if (IsPlayerUsingParachute())
				{
					out_Results.m_AltimeterMode = ALTIMETER_FLYING;

					out_Results.m_fMaxAltimeterHeight = __MAX_HEIGHT_OF_NON_VEHICLE;
				}
			}
		}

#if KEYBOARD_MOUSE_SUPPORT
		if (out_Results.m_AltimeterMode == ALTIMETER_FLYING || out_Results.m_AltimeterMode == ALTIMETER_SWIMMING)
		{
			bool bMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Vehicle;
			bool bCameraMouseFlySteering = CControl::GetMouseSteeringMode(PREF_MOUSE_FLY) == CControl::eMSM_Camera;

			if (IsPlayerUsingParachute())
			{
				// No mouse controls for parachuting; yoke indicator is not necessary
				if (ms_bShowingYoke)
				{
					CMiniMap::ShowYoke(false);
				}
			}
			else if((bMouseFlySteering && !CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn()) || (bCameraMouseFlySteering && CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn()))
			{
				// We've changed to flying
				CControl& playerControl = CControlMgr::GetMainPlayerControl();

				CVehicle *pVehicle = FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true);

				float fRoll = playerControl.GetValue(INPUT_VEH_FLY_ROLL_LR).GetNorm();
				float fPitch = playerControl.GetValue(INPUT_VEH_FLY_PITCH_UD).GetNorm();

				if(pVehicle)
				{
					if(pVehicle->InheritsFromHeli())
					{
						CHeli *pHeli = static_cast<CHeli*>(pVehicle);
						fRoll = -pHeli->GetRollControl();
						fPitch = pHeli->GetPitchControl();
					}
					else if(pVehicle->InheritsFromPlane())
					{
						CPlane *pPlane = static_cast<CPlane*>(pVehicle);
						fRoll = pPlane->GetRollControl();
						fPitch = pPlane->GetPitchControl();
					}
				}
				CMiniMap::ShowYoke(true, fRoll, fPitch, 100);
			}
			else
			{
				CMiniMap::ShowYoke(false);
			}
		}

		ms_iPrevAltimeterMode = out_Results.m_AltimeterMode;
#endif


		if( bMinNeedsSetting )
		{
			out_Results.m_fMinAltimeterHeight = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vPlayerPos.x-ALTIMETER_HEIGHTMAP_WIGGLE_ROOM, vPlayerPos.y-ALTIMETER_HEIGHTMAP_WIGGLE_ROOM
				, vPlayerPos.x+ALTIMETER_HEIGHTMAP_WIGGLE_ROOM, vPlayerPos.y+ALTIMETER_HEIGHTMAP_WIGGLE_ROOM );
		}
	}

	if (ms_bShowingYoke && out_Results.m_AltimeterMode != ALTIMETER_FLYING)
	{
		// Changed from flying mode to something else
		CMiniMap::ShowYoke(false);
	}

	// there's bugs with this being broken.
	// this loose static is probably why
	static bool bPreviousPlaneStalling = false;
	if (bPlaneIsStalling != bPreviousPlaneStalling)
	{
		if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SHOW_STALL_WARNING"))
		{
			CScaleformMgr::AddParamBool(bPlaneIsStalling);
			CScaleformMgr::EndMethod();
		}

		bPreviousPlaneStalling = bPlaneIsStalling;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::GetCurrentMainTile
// PURPOSE:	returns the tile the player is currently on
/////////////////////////////////////////////////////////////////////////////////////
Vector2 CMiniMap::GetCurrentMainTile()
{
	Vector2 vTile(0,0);

	Vector2 vPos;

	if (IsInPauseMap())
	{
		vPos.x = GetPauseMapCursor().x - sm_Tunables.Tiles.vMiniMapWorldStart.x;
		vPos.y = sm_Tunables.Tiles.vMiniMapWorldStart.y - GetPauseMapCursor().y;
	}
	else
	{
		Vector3 vPlayerPos;
		GetCentrePosition(vPlayerPos);

		vPos.x = vPlayerPos.x - sm_Tunables.Tiles.vMiniMapWorldStart.x;
		vPos.y = sm_Tunables.Tiles.vMiniMapWorldStart.y - vPlayerPos.y;
	}

	vTile.x = floor( (vPos.x / sm_Tunables.Tiles.vMiniMapWorldSize.x) * MINIMAP_WORLD_TILE_SIZE_X);
	vTile.y = floor( (vPos.y / sm_Tunables.Tiles.vMiniMapWorldSize.y) * MINIMAP_WORLD_TILE_SIZE_Y);

	//uiDisplayf("CURRENT TILE CHOSEN: %0.2f, %0.2f", vTile.x, vTile.y);

	return vTile;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateTiles
// PURPOSE:	switches on/off tiles based on current player position
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateTilesOnUT(bool &bReturnBitmapOnly, Vector2 &vReturnTilesAroundPlayer)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap::UpdateTilesOnUT can only be called on the Update Thread!");
		return;
	}

//	Vector2 vTilesAroundPlayer;
//	bool bBitmapOnly = false;
	vReturnTilesAroundPlayer = Vector2(2.0f, 2.0f);
	bReturnBitmapOnly = false;


	if ( (!IsInPauseMap()) && (!IsInCustomMap()) )
	{
		float fTilesAroundPlayer = 2.0f;

#if __BANK
		if (s_fDebugForceTilesAroundPlayer != -2.0f)
		{
			if (s_fDebugForceTilesAroundPlayer == -1.0f)
			{
				bReturnBitmapOnly = true;
			}

			fTilesAroundPlayer = s_fDebugForceTilesAroundPlayer;
		}
		else
#endif // __BANK

		if (IsInBigMap() && ms_bBigMapFullScreen)
		{
			bReturnBitmapOnly = true;
		}
		else if(IsInBigMap())
		{
			// calculate the number of tiles based on how much map we're showing
			// much more correct than guessing with tuning values, especially since script is involved
			Vector2 mapAreaWorld( GetCurrentMiniMapBlurSize() );
			Vector2 screenSize((float)VideoResManager::GetUIWidth(), (float)VideoResManager::GetUIHeight());
#if RSG_PC
			// handle multi-monitor fun-ness.
			const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
			screenSize.x = mon.getWidth() * screenSize.x / GRCDEVICE.GetWidth();
			screenSize.y = mon.getHeight() * screenSize.y / GRCDEVICE.GetHeight();
#endif
			mapAreaWorld.Multiply( screenSize ); // Convert to pixels
			mapAreaWorld.Scale( 100.0f / GetRange(true) ); // scale the amount up, adjusting for Scaleform scale (ie, 100 == 1x zoom. 50 == 2x zoom)

			Vector2 tileAreaWorld( sm_Tunables.Tiles.vMiniMapWorldSize );
			tileAreaWorld.Multiply( Vector2( 1.0f/(MINIMAP_WORLD_TILE_SIZE_X / WIDTH_OF_DWD_SUPERTILE), 1.0f/(MINIMAP_WORLD_TILE_SIZE_Y / HEIGHT_OF_DWD_SUPERTILE) ) ); // each supertile has a number of smaller tiles inside it

			Vector2 areaAroundPlayerWorld( mapAreaWorld.x / tileAreaWorld.x, mapAreaWorld.y / tileAreaWorld.y );
			areaAroundPlayerWorld.Scale(.5f); // we computed the 'diameter' of tiles, but we need radius
			fTilesAroundPlayer = Max(areaAroundPlayerWorld.x, areaAroundPlayerWorld.y); // whichever value is bigger, use that one.
		}
		else
		{
			float fRange = GetRange(true);
			if (fRange >= sm_Tunables.Camera.fInteriorFootZoom)
			{
				fTilesAroundPlayer = 1.0f;
			}
			else if (fRange >= sm_Tunables.Camera.fExteriorFootZoom)
			{
				fTilesAroundPlayer = 2.0f;
			}
			else if (fRange >= sm_Tunables.Camera.fVehicleStaticZoom)
			{
				fTilesAroundPlayer = 2.0f;
			}
			else if (fwTimer::GetTimeInMilliseconds() <= iMiniMapDpadZoomTimer+TIME_TO_ZOOM_OUT_MINIMAP)
			{
				fTilesAroundPlayer = 1.0f;
			}
			else
			{
				fTilesAroundPlayer = 2.0f;
			}
		}

		if (bReturnBitmapOnly)
		{
			fTilesAroundPlayer = 0.0f;
		}

		vReturnTilesAroundPlayer = Vector2(fTilesAroundPlayer, fTilesAroundPlayer);
	}
	else
	{	//	We're in the Pause Map  // dont stream out when we move off the panes
/*		if ( (!CPauseMenu::IsInMapScreen()) && (!CPauseMenu::IsInGalleryScreen()) )
		{
			bReturnBitmapOnly = true;
			vReturnTilesAroundPlayer = Vector2(0.0f, 0.0f);

			return;
		}*/

#if __BANK
		if (s_vDebugForceTilesAroundPlayer.x != -1.0f || s_vDebugForceTilesAroundPlayer.y != -1.0f)
		{
			bReturnBitmapOnly = false;
			vReturnTilesAroundPlayer = s_vDebugForceTilesAroundPlayer;
		}
		else
#endif // __BANK
		{
			if (IsInPauseMap() || CPauseMenu::IsInGalleryScreen())
			{
				vReturnTilesAroundPlayer = CMiniMap_Common::GetTilesFromZoomLevel(CMapMenu::GetCurrentZoomLevel());
				bReturnBitmapOnly = (vReturnTilesAroundPlayer.x == 0.0f && vReturnTilesAroundPlayer.y == 0.0f);
			}
			else  // this will be any other screen (ie custom screen).  Here we display a set number of tiles or none at all based on a zoom level
			{
				float fRange = GetRange(true);

#define RANGE_WHERE_MAP_NEEDS_TILES (5.0f)
				if (fRange >= RANGE_WHERE_MAP_NEEDS_TILES)
				{
					vReturnTilesAroundPlayer = Vector2(2.0f, 4.0);
					bReturnBitmapOnly = false;
				}
				else
				{
					vReturnTilesAroundPlayer = Vector2(0.0f, 0.0);
					bReturnBitmapOnly = true;
				}
			}
		}
	}
}



void CMiniMap::GetCentrePosition(Vector3 &vReturnVector)
{
#if !__FINAL
	if ((!IsInPauseMap()) && (!IsInCustomMap()) && 
		( camInterface::GetDebugDirector().IsFreeCamActive()  BANK_ONLY( DEBUG_DRAW_ONLY( && !CMiniMap_RenderThread::sm_bDrawCollisionVolumes))))
	{
		vReturnVector = camInterface::GetPos();
		return;
	}
#endif // __FINAL

	CPed *pLocalPlayer = GetMiniMapPed();

	if (uiVerifyf(pLocalPlayer, "CMiniMap::GetCentrePosition - failed to get pointer to local player"))
	{
		if ( (!CPauseMenu::IsInMapScreen()) && (!CPauseMenu::IsInGalleryScreen()) && (IsPositionLocked()) )
		{
			vReturnVector.x = GetLockedPosition().x;
			vReturnVector.y = GetLockedPosition().y;
			vReturnVector.z = 0.0f;
		}
		else if ( (IsInPauseMap()) || ( (IsInCustomMap()) && (CPauseMenu::IsActive()) ) )
		{
			vReturnVector.x = GetPauseMapCursor().x;
			vReturnVector.y = GetPauseMapCursor().y;
			vReturnVector.z = 0.0f;
		}
		else
		{
			vReturnVector = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());

			const CVehicle *pVehicle = pLocalPlayer->GetVehiclePedInside();
			if (pVehicle)
			{
				vReturnVector.z = pVehicle->GetBoundCentre().GetZ();
			}
		}
	}
	else
	{
		vReturnVector.Zero();
	}
}

bool CMiniMap::IsPlayerRunning()
{
	if (CScriptHud::bDontZoomMiniMapWhenRunningThisFrame)
	{
		return false;
	}

	CPed *pLocalPlayer = GetMiniMapPed();

	if (uiVerifyf(pLocalPlayer, "CMiniMap::IsPlayerRunning - failed to get pointer to local player"))
	{
		if (!pLocalPlayer->GetMotionData()->GetIsLessThanRun())
		{
			return true;
		}		
	}

	return false;
}


float CMiniMap::GetSpeedOfPlayersVehicle()
{
	float fReturnSpeed = 0.0f;

	CPed *pLocalPlayer = GetMiniMapPed();

	if (uiVerifyf(pLocalPlayer, "CMiniMap::GetSpeedOfPlayersVehicle - failed to get pointer to local player"))
	{
		const CVehicle *pVehicle = pLocalPlayer->GetVehiclePedInside();
		const CPed *pPedMount = pLocalPlayer->GetMyMount();  // ie horse
		if (pVehicle || pPedMount)
		{
			float fVehicleSpeed = 0.0f;
			bool bIsNetworkClone = false;

			if (pVehicle)
			{
				fVehicleSpeed = pVehicle->GetVelocity().Mag();
				bIsNetworkClone = pVehicle->IsNetworkClone();
			}
			else if (pPedMount)
			{
				fVehicleSpeed = pPedMount->GetVelocity().Mag();
				bIsNetworkClone = pPedMount->IsNetworkClone();
			}

			if (!bIsNetworkClone)  // don't scale on network clones as it causes issues like in bug 514578
			{
				fReturnSpeed = fVehicleSpeed;
			}
		}
	}

	return fReturnSpeed;
}


bool CMiniMap::IsPlayerUsingParachute()
{
	CPed *pLocalPlayer = GetMiniMapPed();

	if (uiVerifyf(pLocalPlayer, "CMiniMap::IsPlayerUsingParachute - failed to get pointer to local player"))
	{
		// set parachuting flag here in the main update which we can query for the rest of this frame
		if (!GetIsInsideInterior())
		{
			CQueriableInterface* pedQueriableInterface = pLocalPlayer->GetPedIntelligence()->GetQueriableInterface();

			if(pedQueriableInterface && pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_PARACHUTE))
			{
				s32 taskState = pedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_PARACHUTE);

				if(taskState == CTaskParachute::State_Deploying || 
					taskState == CTaskParachute::State_Skydiving ||
					taskState == CTaskParachute::State_Parachuting)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CMiniMap::IsPlayerInPlaneOrHelicopter()
{
	CPed *pLocalPlayer = GetMiniMapPed();

	if (uiVerifyf(pLocalPlayer, "CMiniMap::IsPlayerInPlaneOrHelicopter - failed to get pointer to local player"))
	{
		const CVehicle *pVehicle = pLocalPlayer->GetVehiclePedInside();
		if (pVehicle)
		{
			if (pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
			{
				return true;
			}
		}
	}

	return false;
}

bool CMiniMap::IsPlayerInBikeOrCar()
{
	CPed *pLocalPlayer = GetMiniMapPed();

	if (uiVerifyf(pLocalPlayer, "CMiniMap::IsPlayerInBikeOrCar - failed to get pointer to local player"))
	{
		const CVehicle *pVehicle = pLocalPlayer->GetVehiclePedInside();
		if (pVehicle)
		{
			if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType() ==VEHICLE_TYPE_BICYCLE || pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
			{
				return true;
			}
		}
	}

	return false;
}

bool CMiniMap::IsPlayerOnHorse()
{
	CPed *pLocalPlayer = GetMiniMapPed();

	if (uiVerifyf(pLocalPlayer, "CMiniMap::IsPlayerOnHorse - failed to get pointer to local player"))
	{
		return (pLocalPlayer->GetIsOnMount());
	}

	return false;
}

bool CMiniMap::IsPlayerUnderwaterOrInSub()
{
	CPed *pLocalPlayer = GetMiniMapPed();

	if (uiVerifyf(pLocalPlayer, "CMiniMap::IsPlayerUnderwaterOrInSub - failed to get pointer to local player"))
	{
		const CVehicle *pVehicle = pLocalPlayer->GetVehiclePedInside();
		if (pVehicle)
		{
			// Only render sub minimap details if subcar is transformed
			if (pVehicle->InheritsFromSubmarine() || (pVehicle->InheritsFromSubmarineCar() && pVehicle->IsInSubmarineMode()))
			{
				static bool bEverything = true;
				if( bEverything )
					return pVehicle->m_Buoyancy.GetStatus() != NOT_IN_WATER;
				return pVehicle->m_Buoyancy.GetStatus() == FULLY_IN_WATER;
			}
			return false;
		}

		// not in a vehicle? are you swimmin'?
		const CInWaterEventScanner& waterScanner = pLocalPlayer->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
		float fTimeUnderWater = waterScanner.GetTimeSubmerged();

		return fTimeUnderWater > 0.0f;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateStatesOnUT
// PURPOSE: updates global states on the UT so we can use them later in the RT call
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateStatesOnUT()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap:: UpdateStatesOnUT can only be called on the UpdateThread!");
		return;
	}

	if (!bActive)
		return;

	CPed *pLocalPlayer = GetMiniMapPed();

	if (!pLocalPlayer)
		return;

	sMiniMapRenderStateStruct MiniMapUpdateState;

	MiniMapUpdateState.bDeadOrArrested = (pLocalPlayer->ShouldBeDead() || pLocalPlayer->GetIsArrested());

	GetCentrePosition(MiniMapUpdateState.m_vCentrePosition);

	const CVehicle *pVehicle = pLocalPlayer->GetVehiclePedInside();

	if (pVehicle)
	{
		Matrix34 mat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());

		Vector3 eulers;
		mat.ToEulersYXZ(eulers);  // get the roll
		MiniMapUpdateState.fRoll = eulers.y;
	}
	else
	{
		Matrix34 mat = MAT34V_TO_MATRIX34(pLocalPlayer->GetMatrix());

		Vector3 eulers;
		mat.ToEulersYXZ(eulers);  // get the roll

		MiniMapUpdateState.fRoll = eulers.y;
	}

	// invert the roll if in first person
	{
		const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
		if(pDominantRenderedCamera && camInterface::GetDominantRenderedCamera()->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			const camCinematicMountedCamera* pMountedCamera	= static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
			if( pMountedCamera && pMountedCamera->IsFirstPersonCamera() )
				MiniMapUpdateState.fRoll *= -1.0;
		}
	}


	MiniMapUpdateState.bInPlaneOrHeli = IsPlayerInPlaneOrHelicopter();

	MiniMapUpdateState.m_fPlayerVehicleSpeed = GetSpeedOfPlayersVehicle();

	u32& iHashOfCurrentInterior = MiniMapUpdateState.m_Interior.uHash;

	CPortalTracker* pPT = const_cast<CPortalTracker*>(pLocalPlayer ? pLocalPlayer->GetPortalTracker() : NULL);
	if (pPT->IsInsideInterior())  // need to check very basic interior test here to know whether we need to stream in the interior movie
	{
		iHashOfCurrentInterior = pLocalPlayer->GetPortalTracker()->GetInteriorNameHash();
		u32 roomIndex = pLocalPlayer->GetInteriorLocation().GetRoomIndex();

		// get current interior rotation and position:
		CInteriorInst* pIntInst = pLocalPlayer->GetPortalTracker()->GetInteriorInst();
		if (pIntInst)
		{
			MiniMapUpdateState.m_Interior.iLevel = pIntInst->GetFloorIdInRoom(roomIndex) + pIntInst->GetFloorId();
			MiniMapUpdateState.m_Interior.vPos = VEC3V_TO_VECTOR3(pIntInst->GetTransform().GetPosition());
			MiniMapUpdateState.m_Interior.vBoundMin = pIntInst->GetBoundingBoxMin();
			MiniMapUpdateState.m_Interior.vBoundMax = pIntInst->GetBoundingBoxMax();

			MiniMapUpdateState.m_Interior.fRot = -(pIntInst->GetTransform().GetHeading() * RtoD);
		}
	}

	MiniMapUpdateState.bInsideInterior = GetIsInsideInterior();
	MiniMapUpdateState.bInCreator = CScriptHud::bUsingMissionCreator;

	if (ms_bInPrologue)  // fix for 986642
	{
		iHashOfCurrentInterior = ATSTRINGHASH("V_FakePrologueLand",0xa6410896);
 		MiniMapUpdateState.m_Interior.vPos.Set(4750.0f, -4500.0f, 0.0f);
 		MiniMapUpdateState.m_Interior.fRot = 0.0f;
 		MiniMapUpdateState.m_Interior.iLevel = 0;
	}
	else if(ms_bOnIslandMap)
	{
		u32 uIslandVaultHash = ATSTRINGHASH("h4_dlc_island_vault", 0x3D792750);
		bool bIsInsideIslandVault = MiniMapUpdateState.bInsideInterior && iHashOfCurrentInterior == uIslandVaultHash;

		iHashOfCurrentInterior = ATSTRINGHASH("h4_fake_islandx",0xC0A90510);
		MiniMapUpdateState.m_Interior.vPos.Set(4700.0f, -5150.0f, 0.0f);
		MiniMapUpdateState.m_Interior.fRot = 0.0f;

		// hack to allow the vault interior to display on the island map
		if(bIsInsideIslandVault)
		{
			MiniMapUpdateState.m_Interior.iLevel = 1;
		}
		else
		{
			MiniMapUpdateState.m_Interior.iLevel = 0;
		}
	}
	else
	{
		if (CScriptHud::FakeInterior.bActive && CScriptHud::FakeInterior.iHash != 0)
		{
			iHashOfCurrentInterior = CScriptHud::FakeInterior.iHash;
			if (CScriptHud::FakeInterior.iLevel == sMiniMapInterior::INVALID_LEVEL)
			{
				MiniMapUpdateState.m_Interior.iLevel = 0;
			}
			else
			{
				MiniMapUpdateState.m_Interior.iLevel = CScriptHud::FakeInterior.iLevel;
			}
			MiniMapUpdateState.m_Interior.vPos.Set(CScriptHud::FakeInterior.vPosition.x, CScriptHud::FakeInterior.vPosition.y, 0.0f);
			MiniMapUpdateState.m_Interior.fRot = -(float)CScriptHud::FakeInterior.iRotation;
			MiniMapUpdateState.m_Interior.vBoundMin.Zero();
			MiniMapUpdateState.m_Interior.vBoundMax.Zero();
		}
	}

#if __BANK
	if (ms_iDebugInteriorHash != 0)
	{
		iHashOfCurrentInterior = (u32)ms_iDebugInteriorHash;
		MiniMapUpdateState.m_bHasDebugInterior = true;
	}
	else
	{
		MiniMapUpdateState.m_bHasDebugInterior = false;
	}

	if (ms_iDebugInteriorLevel != sMiniMapInterior::INVALID_LEVEL)
	{
		MiniMapUpdateState.m_Interior.iLevel = ms_iDebugInteriorLevel;
	}
#endif // __BANK

	//
	// check whether this is an interior that we want to override with an alternative interior
	//
	if (iHashOfCurrentInterior != 0)
	{
		bool bFoundMatchingOverrideInterior = false;
		for (s32 i = 0; ( (!bFoundMatchingOverrideInterior) && (i < sm_InteriorInfo.GetCount()) ); i++)
		{
			if (iHashOfCurrentInterior != sm_InteriorInfo[i].iMainInteriorHash)
			{
				for (s32 j = 0; ( (!bFoundMatchingOverrideInterior) && (j < sm_InteriorInfo[i].iSubInteriorHash.GetCount()) ); j++)
				{
					if (iHashOfCurrentInterior == sm_InteriorInfo[i].iSubInteriorHash[j])
					{
						iHashOfCurrentInterior = sm_InteriorInfo[i].iMainInteriorHash;
						MiniMapUpdateState.m_Interior.vPos.Set(sm_InteriorInfo[i].vMainInteriorPos.x, sm_InteriorInfo[i].vMainInteriorPos.y, 0.0f);
						MiniMapUpdateState.m_Interior.fRot = sm_InteriorInfo[i].fMainInteriorRot;
						MiniMapUpdateState.m_Interior.vBoundMin.Zero();
						MiniMapUpdateState.m_Interior.vBoundMax.Zero();
						MiniMapUpdateState.m_Interior.iLevel = 0;

						bFoundMatchingOverrideInterior = true;
					}
				}
			}
		}
	}

	if ( ms_previousInterior.uHash != 0 &&
		 ms_previousInterior.ResolveDifferentHash(MiniMapUpdateState.m_Interior) )
	{
		// We're leaving an interior; stop caching the interior's fake position
		CScriptHud::ms_iCurrentFakedInteriorHash = 0;
		CScriptHud::vInteriorFakePauseMapPlayerPos.Set(INVALID_FAKE_PLAYER_POS);
	}
	else
	{
		// Make sure the previous interior is always cached
		ms_previousInterior.ResolveDifferentHash(MiniMapUpdateState.m_Interior);
	}

	MiniMapUpdateState.bIsInParachute = IsPlayerUsingParachute();

	UpdateMapPositionAndScaleOnUT(MiniMapUpdateState.m_fAngle, MiniMapUpdateState.m_fOffset, MiniMapUpdateState.m_vMiniMapPosition, MiniMapUpdateState.m_fMiniMapRange, MiniMapUpdateState.m_iRotation);

	MiniMapUpdateState.m_bIsInBigMap = IsInBigMap();
	MiniMapUpdateState.m_bBigMapFullZoom = ms_bBigMapFullZoom;
	MiniMapUpdateState.m_bIsInPauseMap = IsInPauseMap();
	MiniMapUpdateState.bDrawCrosshair = CPauseMenu::IsInMapScreen() && CPauseMenu::IsNavigatingContent();
	MiniMapUpdateState.m_bLockedToDistanceZoom = (CScriptHud::ms_fRadarZoomDistanceThisFrame != 0.0f);
	MiniMapUpdateState.m_bIsInCustomMap = sMiniMapMenuComponent.IsActive();
	MiniMapUpdateState.m_vCurrentMiniMapPosition = GetCurrentMiniMapPosition();
	MiniMapUpdateState.m_vCurrentMiniMapSize = GetCurrentMiniMapSize();
	MiniMapUpdateState.m_vCurrentMiniMapMaskPosition = GetCurrentMiniMapMaskPosition();
	MiniMapUpdateState.m_vCurrentMiniMapMaskSize = GetCurrentMiniMapMaskSize();
	MiniMapUpdateState.m_vCurrentMiniMapBlurPosition = GetCurrentMiniMapBlurPosition();
	MiniMapUpdateState.m_vCurrentMiniMapBlurSize = GetCurrentMiniMapBlurSize();
	MiniMapUpdateState.m_bIsActive = bActive;

#if __BANK
	if( s_bRenderMapPositions )
	{
		grcDebugDraw::RectAxisAligned(MiniMapUpdateState.m_vCurrentMiniMapPosition, MiniMapUpdateState.m_vCurrentMiniMapPosition+MiniMapUpdateState.m_vCurrentMiniMapSize, Color_red, false);
		grcDebugDraw::RectAxisAligned(MiniMapUpdateState.m_vCurrentMiniMapMaskPosition, MiniMapUpdateState.m_vCurrentMiniMapMaskPosition+MiniMapUpdateState.m_vCurrentMiniMapMaskSize, Color_orange, false);
		grcDebugDraw::RectAxisAligned(MiniMapUpdateState.m_vCurrentMiniMapBlurPosition, MiniMapUpdateState.m_vCurrentMiniMapBlurPosition+MiniMapUpdateState.m_vCurrentMiniMapBlurSize, Color_yellow, false);
	}
#endif

	// the MinimapModeState gets passed through to RT here, so we need the 'GetCurrentMiniMapPosition' style calls above to
	// return sizes based on this value as this if this is set to a transition, it will change the minimap properties, so we
	// need latest info to use
	MiniMapUpdateState.m_MinimapModeState = MinimapModeState;
#if RSG_PC
	// we need to resize after a device loss, but for some reason, if we did this immediately, it disappears
	// so instead we gotta wait a frame for the DLC call to make it through.
	if( s_iDeviceLostResetCountdown )
	{
		if( GRCDEVICE.IsReady() 
			&& MiniMapUpdateState.m_MinimapModeState == MINIMAP_MODE_STATE_NONE
			&& --s_iDeviceLostResetCountdown == 0)
		{
			MiniMapUpdateState.m_MinimapModeState = ms_MiniMapReappearanceState.MinimapModeStateWhenHidden;
		}
	}
#endif

	if (IsInBigMap() || IsInCustomMap())
	{
		MiniMapUpdateState.m_CurrentGolfMap = GOLF_COURSE_OFF;
	}
	else
	{
#if __BANK
		if (ms_iDebugGolfCourseMap != -1)
		{
			MiniMapUpdateState.m_CurrentGolfMap = (eGOLF_COURSE_HOLES) ms_iDebugGolfCourseMap;
		}
		else
#endif // __BANK
		{
			MiniMapUpdateState.m_CurrentGolfMap = GetCurrentGolfMap();
		}
	}

	if( MiniMapUpdateState.m_CurrentGolfMap == GOLF_COURSE_OFF )
	{
		MiniMapUpdateState.m_CurrentMiniMapFilename[0] = '\0';
	}
	else
	{
		safecpy(MiniMapUpdateState.m_CurrentMiniMapFilename, sm_miniMaps[MINIMAP_VALUE_GOLF_COURSE].m_fileName, MAX_LENGTH_OF_MINIMAP_FILENAME);
	}

#if __BANK
	MiniMapUpdateState.m_bDisplayAllBlipNames = bDisplayAllBlipNames;
//	MiniMapUpdateState.m_iDebugGolfCourseMap = ms_iDebugGolfCourseMap;
#endif

	UpdateAltimeter(MiniMapUpdateState);

	// We want to show a yoke to help with mouse control when driving. 
#if RSG_PC
	if(ms_iPrevAltimeterMode == ALTIMETER_OFF && ms_bShowingYoke)
	{
		CMiniMap::ShowYoke(false);
	}
#endif

	MiniMapUpdateState.m_bShowSonarSweep = ms_bShowSonarSweep;

	MiniMapUpdateState.m_bBackgroundMapShouldBeHidden = (CPauseMenu::GetMenuPreference(PREF_RADAR_MODE) == 2 || IsBackgroundMapHidden());

	if (IsInPauseMap() || IsInCustomMap())
	{
		MiniMapUpdateState.m_bBackgroundMapShouldBeHidden = false;
	}

	if (GetInPrologue())
	{
		MiniMapUpdateState.m_bShowPrologueMap = true;
		MiniMapUpdateState.m_bShowMainMap = false;
		MiniMapUpdateState.m_bShowIslandMap = false;
	}
	else
	{
		const bool c_bShouldShowMap = (!CScriptHud::ms_bHideMiniMapExteriorMapThisFrame || IsInCustomMap());

		MiniMapUpdateState.m_bShowPrologueMap = false;
		MiniMapUpdateState.m_bShowMainMap = c_bShouldShowMap && !GetIsOnIslandMap();
		MiniMapUpdateState.m_bShowIslandMap = c_bShouldShowMap && GetIsOnIslandMap();
	}

	MiniMapUpdateState.m_bHideInteriorMap = CScriptHud::ms_bHideMiniMapInteriorMapThisFrame;

#if ENABLE_FOG_OF_WAR
	MiniMapUpdateState.m_vPlayerPos = 	Vector2(pLocalPlayer->GetTransform().GetPosition().GetXf(),pLocalPlayer->GetTransform().GetPosition().GetYf());
	
	MiniMapUpdateState.m_bUpdateFoW = !IsInPauseMap() && !IsInCustomMap() && !MiniMapUpdateState.m_bIsInBigMap && !ms_bInPrologue && ((!NetworkInterface::IsGameInProgress()) || ms_bAllowFoWInMP);
	MiniMapUpdateState.m_bUpdateFoW &= !ms_bDoNotUpdateFoW;
	
	MiniMapUpdateState.m_bShowFow = IsInPauseMap() && ((!NetworkInterface::IsGameInProgress()) || ms_bAllowFoWInMP) && !IsInCustomMap() && !ms_bInPrologue && !ms_bHideFoW ;
	
	MiniMapUpdateState.m_numScriptRevealRequested = ms_numScriptRevealRequested;
	if( ms_numScriptRevealRequested )
	{
		sysMemCpy(MiniMapUpdateState.m_ScriptReveal,ms_ScriptReveal,sizeof(MiniMapUpdateState.m_ScriptReveal));
		ms_numScriptRevealRequested = 0;
	}

	MiniMapUpdateState.m_bUpdatePlayerFoW = !CGameWorld::GetMainPlayerInfo()->AreControlsDisabled();

	if( ShouldProcessMinimap() )
	{
		MiniMapUpdateState.m_bClearFoW = ms_bRequestFoWClear;
		ms_bRequestFoWClear = false;
		ms_bSetFowIsValid = true;
		
		MiniMapUpdateState.m_bRevealFoW = false;
#if !__FINAL
		MiniMapUpdateState.m_bRevealFoW |= PARAM_revealmap.Get();
#endif // !__FINAL
		MiniMapUpdateState.m_bRevealFoW |= ms_bRequestRevealFoW;
#if __BANK	
		MiniMapUpdateState.m_bRevealFoW |= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_M, KEYBOARD_MODE_DEBUG_SHIFT, "Reveal Map");
#endif // __BANK
		ms_bRequestRevealFoW = false;
#if __D3D11
		MiniMapUpdateState.m_bUploadFoWTextureData = ms_bUploadFoWTextureData;
		ms_bUploadFoWTextureData = false;
#endif // __D3D11
	}
#endif // ENABLE_FOG_OF_WAR

	MiniMapUpdateState.m_vCentreTileForPlayer = GetCurrentMainTile();
	UpdateTilesOnUT(MiniMapUpdateState.m_bBitmapOnly, MiniMapUpdateState.m_vTilesAroundPlayer);

	MiniMapUpdateState.m_bFlashWantedOverlay = ms_bFlashWantedOverlay;
	MiniMapUpdateState.m_uFlashStartTime = ms_uFlashStartTime;
	MiniMapUpdateState.m_eFlashColour = ms_eFlashColour;
	MiniMapUpdateState.m_fPauseMapScale = GetPauseMapScale();

	MiniMapUpdateState.m_bShouldProcessMiniMap = ShouldProcessMinimap();

	MiniMapUpdateState.m_bInsideReappearance = false;

	MiniMapUpdateState.m_bDisplayPlayerNames = CScriptHud::ms_bDisplayPlayerNameBlipTags;

	// if the minimap is turned off then the 1st render of it when it comes back has an instant zoom to fix bug 1013874 and similar issues
	// this lasts for 2 frames for a reason; to ensure all checks are catered for as we dont know what else may change the zoom and in what
	// order.  script may pop the player outside before turning on the minimap or they may turn it on beforehand, and we have to deal with
	// them doing that in a polished manner

	//
	// this bit of code is not pretty but at these stage we need to get it looking polished and there are so many things that can effect this stuff
	// we want it to appear nicely even if script hide it, show it, someone presses START, unpauses and goes to bigmap.... hence why this has a lot
	// of IF checks.
	//

	u32 uTimeForMiniMapToReappear = 500;  // fix for 1893333 without changing too much on how this works...

	if (sMiniMapMenuComponent.IsActive())  // quicker time in custom map
	{
		uTimeForMiniMapToReappear = 100;
	}

#if __BANK
	bool bOutputDebug = false;
#endif

	if (!MiniMapUpdateState.m_bShouldProcessMiniMap)
	{
		if ( (!ms_MiniMapReappearanceState.bBeenInPauseMenu) && (ms_MiniMapReappearanceState.iMiniMapRenderedTimer == 0) )  // if it was on previously
		{
			// store position of centre of minimap:
			ms_MiniMapReappearanceState.vMiniMapCentrePosWhenHidden = MiniMapUpdateState.m_vCentrePosition;

			// store whether inside an interior (note we need to check portal code here rather than the minimap movie, as that wont be setup till minimap is rendered)
			const CPed *pLocalPlayer = GetMiniMapPed();
			CPortalTracker* pPT = const_cast<CPortalTracker*>(pLocalPlayer ? pLocalPlayer->GetPortalTracker() : NULL);
			bool bInsideInteriorFromPortal = ( (CScriptHud::FakeInterior.bActive) || ( (pPT) && (pPT->IsInsideInterior()) && (!pPT->IsAllowedToRunInInterior()) ) );

			ms_MiniMapReappearanceState.bMiniMapInteriorWhenHidden = bInsideInteriorFromPortal || MiniMapUpdateState.bInsideInterior;

#if __BANK
			bOutputDebug = true;
#endif
		}

		if (CPauseMenu::IsActive() && !sMiniMapMenuComponent.IsActive())
		{
			ms_MiniMapReappearanceState.bBeenInPauseMenu = true;
		}

		if (!CScriptHud::bDisplayRadar)
		{
			u32 iNewTimer = fwTimer::GetSystemTimeInMilliseconds();

			if (iNewTimer > ms_MiniMapReappearanceState.iMiniMapRenderedTimer)
			{
				ms_MiniMapReappearanceState.iMiniMapRenderedTimer = iNewTimer;
			}
		}
		else
		{
			u32 iNewTimer = fwTimer::GetSystemTimeInMilliseconds()+uTimeForMiniMapToReappear;

			if (iNewTimer > ms_MiniMapReappearanceState.iMiniMapRenderedTimer)
			{
				ms_MiniMapReappearanceState.iMiniMapRenderedTimer = iNewTimer;
			}
		}

		MiniMapUpdateState.m_bInsideReappearance = true;

		ms_MiniMapReappearanceState.MinimapModeStateWhenHidden = MinimapModeState;
	}
	else if ( (!ms_MiniMapReappearanceState.bBeenInPauseMenu) && (ms_MiniMapReappearanceState.iMiniMapRenderedTimer != 0) )
	{
		if ( (ms_MiniMapReappearanceState.bMiniMapInteriorWhenHidden == MiniMapUpdateState.bInsideInterior) &&
			 (ms_MiniMapReappearanceState.MinimapModeStateWhenHidden == MINIMAP_MODE_STATE_NONE) )
		{
			ms_MiniMapReappearanceState.iMiniMapRenderedTimer = 0;  // turn it straight on again
		}
	}

	MiniMapUpdateState.m_bShouldRenderMiniMap = ShouldRenderMinimap();

	bool bIsInBigmapThisFrame = IsInBigMap();  // so we can render instantly when switching between minimap and bigmap
	static bool bIsInBigmapPreviousFrame = bIsInBigmapThisFrame;

	if (MiniMapUpdateState.m_bShouldRenderMiniMap)
	{
		MiniMapUpdateState.m_bShouldRenderMiniMap = (	(bIsInBigmapThisFrame || bIsInBigmapPreviousFrame) ||
														(ms_MiniMapReappearanceState.iMiniMapRenderedTimer == 0) || 
														( (IsPositionLocked() && !sMiniMapMenuComponent.IsActive()) && (!IsInGolfMap()) && (!ms_MiniMapReappearanceState.bBeenInPauseMenu) ) ||  // we are going to use IsPositionLocked to pre-stream the minimap
														(fwTimer::GetSystemTimeInMilliseconds() >= ms_MiniMapReappearanceState.iMiniMapRenderedTimer) || 
														(ms_MiniMapReappearanceState.iMiniMapRenderedTimer < (fwTimer::GetSystemTimeInMilliseconds() - uTimeForMiniMapToReappear)) );

		if (MiniMapUpdateState.m_bShouldRenderMiniMap)
		{
			if (!CPauseMenu::IsActive())
			{
				ms_MiniMapReappearanceState.bBeenInPauseMenu = false;
			}

			ms_MiniMapReappearanceState.iMiniMapRenderedTimer = 0;
		}
#if __BANK
		else if (bOutputDebug)  // only output debug if the timer hasnt been updated this frame (stops it spamming so much)
		{
			bool bCutsceneRunning = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());
			if ( (CPauseMenu::GetMenuPreference(PREF_RADAR_MODE) != 0) && (!CPauseMenu::IsActive()) && (!bCutsceneRunning) )  // only want this output when minimap is on and in game
			{
				uiDisplayf("Minimap not rendered as it is preparing its display (%d/%d)", fwTimer::GetSystemTimeInMilliseconds(), ms_MiniMapReappearanceState.iMiniMapRenderedTimer);
			}
		}
#endif // #if __BANK
	}

	if (ms_MiniMapReappearanceState.iMiniMapRenderedTimer == 0)
	{
		ms_bIsHiddenAfterTransition = false;
	}
	else
	{
		ms_bIsHiddenAfterTransition = (ms_MiniMapReappearanceState.iMiniMapRenderedTimer > (fwTimer::GetSystemTimeInMilliseconds() - uTimeForMiniMapToReappear));
	}

 	if (CPauseMenu::IsActive() && !sMiniMapMenuComponent.IsActive())  // if in pausemap, then render anyway, but keep the flags set as above
 	{
 		MiniMapUpdateState.m_bShouldRenderMiniMap = true;
 	}

 	if ( (IsInBigMap()) && (MiniMapUpdateState.m_bShouldRenderMiniMap) )  // fixes 1563911
 	{
 		MiniMapUpdateState.m_bShouldRenderMiniMap =	true;//AreAllTexturesActive(ms_eBitmapForMinimap);
 	}
	
	ms_bIsRendering = (MiniMapUpdateState.m_bShouldProcessMiniMap) && (MiniMapUpdateState.m_bShouldRenderMiniMap);

	MiniMapUpdateState.m_iScriptZoomValue = CScriptHud::ms_iRadarZoomValue;

	DLC(CMiniMap_RenderState_Setup, (&MiniMapUpdateState) );  // pass through to drawlist

//	Moved to CMiniMap::UpdateAtEndOfFrame() to ensure that it's only cleared once the RenderThread has had a chance to process it
//	MinimapModeState = MINIMAP_MODE_STATE_NONE;	//	Graeme - really not sure about this. I'm relying on MinimapModeState only being checked inside CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT()

	bIsInBigmapPreviousFrame = bIsInBigmapThisFrame;  // store for next frame

#if DEBUG_DRAW && __BANK
	CMiniMap_RenderThread::RenderDebugVolumesOnUT();
#endif
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateDpadDown()
// PURPOSE: certain things in the minimap are triggered when the player pressed DPAD DOWN
//			this function deals with that
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateDpadDown()
{
	// dont allow dpad down to work in golf course (but still process it if its been set previously)
	if ( !IsInGolfMap() && !CPhoneMgr::IsDisplayed() && 
		(!NetworkInterface::IsGameInProgress() || CNewHud::IsSniperSightActive()) && 
		!GetIsInsideInterior() )
	{
		CPed * pPlayerPed = FindPlayerPed();
		if (pPlayerPed)
		{
			const CControl* pControl = pPlayerPed->GetControlFromPlayer();
			if ( (pControl && pControl->GetHudSpecial().IsDown()) || (CNewHud::IsSniperSightActive() && !CScriptHud::bDontZoomMiniMapWhenSnipingThisFrame) )
			{
				iMiniMapDpadZoomTimer = fwTimer::GetTimeInMilliseconds();
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateGPS
// PURPOSE:	updates the GPS arrows on the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateGPS()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::UpdateGPS can only be called on the UpdateThread!");
		return;
	}

	if (GetMovieId(MINIMAP_MOVIE_FOREGROUND) == -1)
		return;

//	static bool bPreviousOffsetMiniMapForGPSBar = false;
//	bool bOffsetMiniMapForGPSBar = bPreviousOffsetMiniMapForGPSBar;

	s32 iLatestGpsInstruction = INVALID_GPS_INSTRUCTION;

	CPed *pPlayerPed = GetMiniMapPed();
	bool hideDistance = false;
	bool bDoSubCheck = false;

	if ( (pPlayerPed) && (pPlayerPed->GetIsInVehicle()) && (!IsInBigMap()) )
	{
		CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();

		bool bInsideSubCarOnRoad = false;
		bool bInsideAmphibiousCarOnRoad = false;
		
		if(pVehicle->InheritsFromSubmarineCar())  // fix for 1883240
		{
			bInsideSubCarOnRoad = (pVehicle->m_Buoyancy.GetStatus() == NOT_IN_WATER);  // only considered in the car and not sub if we are not in the water
		}
		else if (pVehicle->InheritsFromAmphibiousAutomobile())
		{
			// Check out of water timer to stop this flicking on and off when jumping across waves
			CAmphibiousAutomobile* pAmphiVeh = static_cast<CAmphibiousAutomobile*>(pVehicle);
			bInsideAmphibiousCarOnRoad = pAmphiVeh->m_Buoyancy.GetStatus() == NOT_IN_WATER && pAmphiVeh->GetBoatHandling()->GetOutOfWaterTime() > 2.0f;
		}

		bDoSubCheck = pVehicle && pVehicle->GetIsAquatic() && IsPlayerUnderwaterOrInSub();

		if(pVehicle && (pVehicle->GetIsAircraft() || (pVehicle->GetIsAquatic() && !bDoSubCheck && !bInsideSubCarOnRoad && !bInsideAmphibiousCarOnRoad)))
		{
			hideDistance = true;
		}
		else if (!IsInPauseMap() && !IsInBigMap())
		{
			s32 iCurrentGPSRouteId = -1;
			float fCurrentGPSDistance = -1.0f;

			if(bDoSubCheck)
			{
				Vector3 vSubPos;
				spdAABB bounds;
				bounds = pVehicle->GetLocalSpaceBoundBox(bounds);
				pVehicle->GetBoundCentre(vSubPos);
				const float fHalfSubHeight = (bounds.GetMax().GetZf() - bounds.GetMin().GetZf()) * 0.5f;

				float fWaterLevel = 0.0f;
				pVehicle->m_Buoyancy.GetWaterLevelIncludingRiversNoWaves(vSubPos, &fWaterLevel, POOL_DEPTH, REJECTIONABOVEWATER, NULL);

				// Ideally we would do this probe asynchronously, but this is already more efficient than using FindGroundZForCoord().
				WorldProbe::CShapeTestHitPoint probeHitPoint;
				WorldProbe::CShapeTestResults probeResult(probeHitPoint);
		
				WorldProbe::CShapeTestProbeDesc probeDesc;
				// If we hit nothing, assume THE ABYSS.
				float fGroundBelowSubLevel = __MIN_HEIGHT_OF_ABYSS;
				Vector3 vProbeEnd(vSubPos.x, vSubPos.y, fGroundBelowSubLevel);
				probeDesc.SetStartAndEnd(vSubPos, vProbeEnd);
				probeDesc.SetResultsStructure(&probeResult);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
				probeDesc.SetContext(WorldProbe::EHud);
				probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);

				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				{
					fGroundBelowSubLevel = probeHitPoint.GetHitPosition().z;
				}

				const float fDistanceToSurface = Max(fWaterLevel - (vSubPos.z + fHalfSubHeight),   0.0f);
				const float fDistanceToFloor   = Max((vSubPos.z - fHalfSubHeight) - fGroundBelowSubLevel, 0.0f);

				if( !ms_bPreviousShowDepthGauge )
				{
					CScaleformMgr::CallMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SHOW_DEPTH");
				}

				const float fDelta = 0.30f; // because metric, only send if we differ by ~a foot
				if( !ms_bPreviousShowDepthGauge || abs(fDistanceToSurface - fPreviousGPSDistance) >= fDelta || abs(fDistanceToFloor - fPreviousMinimapFloor) >= fDelta )
				{
					// this MAY need some leeway since at the highest level you start taking damage
					bool bWarnAboutCrushDepth = false;//pVehicle->GetSubHandling() && pVehicle->GetSubHandling()->IsUnderDesignDepth( pVehicle );

					if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_DEPTH"))
					{
						CScaleformMgr::AddParamFloat(fDistanceToSurface);
						CScaleformMgr::AddParamFloat(fDistanceToFloor);
						CScaleformMgr::AddParamBool(CFrontendStatsMgr::ShouldUseMetric());
						CScaleformMgr::AddParamBool(bWarnAboutCrushDepth);
						CScaleformMgr::EndMethod();
					}

					fPreviousGPSDistance = fDistanceToSurface;
					fPreviousMinimapFloor = fDistanceToFloor;
				}

				iLatestGpsInstruction = 0;
				ms_bPreviousShowDepthGauge = true;
			}

			else if (CGps::GetRouteFound(GPS_SLOT_RADAR_BLIP))
			{
				iCurrentGPSRouteId = GPS_SLOT_RADAR_BLIP;
			}
			else if (CGps::GetRouteFound(GPS_SLOT_WAYPOINT))
			{
				iCurrentGPSRouteId = GPS_SLOT_WAYPOINT;
			}

			if (iCurrentGPSRouteId != -1 && CGps::ShouldGpsBeVisible(iCurrentGPSRouteId, false))
			{
				iLatestGpsInstruction = CGps::CalcRouteInstruction(iCurrentGPSRouteId);

				if (CUserDisplay::AreaName.IsNewName() && CUserDisplay::AreaName.GetName()[0] != '\0')
				{
					if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_SATNAV_AREA"))
					{
						CScaleformMgr::AddParamString(CUserDisplay::AreaName.GetName());
						CScaleformMgr::EndMethod();
					}

					//bOffsetMiniMapForGPSBar = true;
				}

				if(CGps::GetSlot(iCurrentGPSRouteId).IsWorking())
				{
					fCurrentGPSDistance = float(CGps::GetRouteLength(iCurrentGPSRouteId));
				}
			}

			if( !bDoSubCheck )
			{
				if( fCurrentGPSDistance != -1.0f && abs(fCurrentGPSDistance - fPreviousGPSDistance) >= 1.0 ) // if a significant enough delta
				{
					if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_SATNAV_DISTANCE"))
					{
						CScaleformMgr::AddParamFloat(fCurrentGPSDistance);
						CScaleformMgr::AddParamBool(CFrontendStatsMgr::ShouldUseMetric());
						CScaleformMgr::EndMethod();
					}

					fPreviousGPSDistance = fCurrentGPSDistance;
				}
			}
		}
		else
		{
			// If you're in the pause menu, let's clear the distance so it'll refresh when we unpause.
			fPreviousGPSDistance = -1.0f;
		}
	}

	if( !bDoSubCheck && ms_bPreviousShowDepthGauge )
	{
		CScaleformMgr::CallMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "HIDE_DEPTH");
		ms_bPreviousShowDepthGauge = false;
	}

/*  // matches enum in Actionscript (bug 377270)
BLANK = 0
FORWARD = 1
BACK = 2
LEFT = 3
RIGHT = 4
SLIPROAD_LEFT = 5
SLIPROAD_RIGHT = 6
DIAGONAL_LEFT = 7
DIAGONAL_RIGHT = 8
MERGE_LEFT = 9
MERGE_RIGHT = 10 
U_TURN = 11
*/
	s32 iActionScriptValue = 0;

	if (!hideDistance && iLatestGpsInstruction != INVALID_GPS_INSTRUCTION)
	{
		switch (iLatestGpsInstruction)
		{
			case CGpsSlot::GPS_INSTRUCTION_TURN_LEFT:
			{
				iActionScriptValue = 3;
				break;
			}

			case CGpsSlot::GPS_INSTRUCTION_TURN_RIGHT:
			{
				iActionScriptValue = 4;
				break;
			}

			case CGpsSlot::GPS_INSTRUCTION_KEEP_LEFT:
			{
				iActionScriptValue = 7;
				break;
			}

			case CGpsSlot::GPS_INSTRUCTION_KEEP_RIGHT:
			{
				iActionScriptValue = 8;
				break;
			}

			case CGpsSlot::GPS_INSTRUCTION_STRAIGHT_AHEAD:
			{
				iActionScriptValue = 1;
				break;
			}

			case CGpsSlot::GPS_INSTRUCTION_UTURN:
			{
				iActionScriptValue = 11;
				break;
			}

			case CGpsSlot::GPS_INSTRUCTION_CALCULATING_ROUTE:
			case CGpsSlot::GPS_INSTRUCTION_HIGHLIGHTED_ROUTE:
			case CGpsSlot::GPS_INSTRUCTION_YOU_HAVE_ARRIVED:
			case CGpsSlot::GPS_INSTRUCTION_BONG:
			{
				iActionScriptValue = -1;
				break;
			}

			case CGpsSlot::GPS_INSTRUCTION_UNKNOWN:
			{
				iActionScriptValue = -1;
				break;
			}

			default:
			{
				iActionScriptValue = -1;
				break;
			}
		}
	}

	if (iActionScriptValue != iPreviousGpsInstruction)
	{
		if (iActionScriptValue == 0)
		{
			if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "HIDE_SATNAV"))
			{
				CScaleformMgr::EndMethod();
			}

			//bOffsetMiniMapForGPSBar = false;
		}
		else
		if (iActionScriptValue != -1)
		{
			if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_SATNAV_DIRECTION"))
			{
				CScaleformMgr::AddParamInt(iActionScriptValue);
				CScaleformMgr::EndMethod();
			}

			//bOffsetMiniMapForGPSBar = true;
		}

		iPreviousGpsInstruction = iActionScriptValue;
	}

	// the invokes to turn off the graphical elements of this will take a frame to get flushed, so we also need to wait the same time
	// before repositioning the movie and also skip a blip update at this point, so its a smooth transition
/*	if (bSkipBlipUpdate)
	{
		// readjust the position:
		Vector2 vPos = GetCurrentMiniMapPosition();
		Vector2 vSize = GetCurrentMiniMapSize();

		CScaleformMgr::ChangeMovieParams(GetMovieId(), vPos, vSize, GFxMovieView::SM_ExactFit);
		
		bSkipBlipUpdate = false;
	}*/

/*	if (bOffsetMiniMapForGPSBar != bPreviousOffsetMiniMapForGPSBar)
	{
		//SetShowingGPSDisplay(bOffsetMiniMapForGPSBar);

//		bSkipBlipUpdate = true;
		bPreviousOffsetMiniMapForGPSBar = bOffsetMiniMapForGPSBar;
	}*/
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ReInit
// PURPOSE:	reinits the minimap - shuts it down and restarts it
//			this is called between network sessions so the radar fully clears out its
//			memory and data
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::ReInit()
{
	uiDisplayf("MiniMap: REINIT() - Starting to reinitialise minimap...");

	gRenderThreadInterface.Flush();  // flush anything in the DL before we shutdown and re-init
	uiDisplayf("MiniMap: REINIT() - flushed renderthread OK...");

	// ensure queue is empty before we restart
	CBlipUpdateQueue::QueueLock lock(CMiniMap_Common::GetBlipUpdateQueue());
	CMiniMap_Common::EmptyBlipUpdateQueue();

	uiDisplayf("MiniMap: REINIT() - emptied queue OK...");

#if ENABLE_FOG_OF_WAR
	CMiniMap::SetIgnoreFowOnInit(true);
#endif

	ms_bInsideReInit = true;
	CMiniMap::Shutdown(SHUTDOWN_SESSION);
	ms_bInsideReInit = false;
	CMiniMap::Init(INIT_SESSION);

	// Reset FoW states to after reset
#if ENABLE_FOG_OF_WAR
	CMiniMap::SetIgnoreFowOnInit(false);
#endif

	uiDisplayf("MiniMap: REINIT() - Minimap is now fully reinitialised");
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateMultiplayerInformation
// PURPOSE:	updates the minimap with any information from MP
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateMultiplayerInformation()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::UpdateMultiplayerInformation can only be called on the UpdateThread!");
		return;
	}

	bool bDisableAbilityBar = NetworkInterface::IsGameInProgress() || CTheScripts::GetIsInDirectorMode();

	if (bPreviousMultiplayerStatus != bDisableAbilityBar )
	{
		bPreviousMultiplayerStatus = bDisableAbilityBar;

		if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "MULTIPLAYER_IS_ACTIVE"))
		{
			CScaleformMgr::AddParamBool(bPreviousMultiplayerStatus);
			CScaleformMgr::EndMethod();
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::IsFlagSet
// PURPOSE: returns whether a specific flag is set
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::IsFlagSet(const CMiniMapBlip *pBlip, u32 iQueryFlag)
{
	uiAssertf(pBlip, "CMiniMap::IsFlagSet - Blip not valid!");

	// optimisation - assume pBlip is valid if passed in, should speed things up
	if (pBlip->IsComplex())
	{
		return ((CBlipComplex*)pBlip)->m_flags.IsSet(iQueryFlag);
	}
	else
	{
		return ((CBlipComplex*)blip[iSimpleBlip])->m_flags.IsSet(iQueryFlag);
	}
}



void CMiniMap::UnsetFlag(CMiniMapBlip *pBlip, u32 iQueryFlag)
{
	if (pBlip->IsComplex())
	{
		((CBlipComplex*)pBlip)->m_flags.Clear(iQueryFlag);
	}
	else
	{
		((CBlipComplex*)blip[iSimpleBlip])->m_flags.Clear(iQueryFlag);
	}
}


void CMiniMap::SetFlag(CMiniMapBlip *pBlip, u32 iQueryFlag)
{
	if (pBlip->IsComplex())
	{
		((CBlipComplex*)pBlip)->m_flags.Set(iQueryFlag, true);
	}
	else
	{
		((CBlipComplex*)blip[iSimpleBlip])->m_flags.Set(iQueryFlag, true);
	}
}


void CMiniMap::ResetFlag(CMiniMapBlip *pBlip)
{
	if (pBlip->IsComplex())
		((CBlipComplex*)pBlip)->m_flags.Reset();
	else
		((CBlipComplex*)blip[iSimpleBlip])->m_flags.Reset();
}

void CMiniMap::GetAllFlags(BlipFlagBitset& outBits, CMiniMapBlip* pBlip)
{
	if (uiVerifyf(pBlip, "CMiniMap::IsFlagSet - Blip not valid!"))
	{
		if (pBlip->IsComplex())
		{
			outBits = ((CBlipComplex*)pBlip)->m_flags;
		}
		else
		{
			outBits = ((CBlipComplex*)blip[iSimpleBlip])->m_flags;
		}
	}
}

void CMiniMap::AddConeColorForBlip(s32 iBlipIndex, eHUD_COLOURS coneColor)
{
	m_ConeColors[iBlipIndex] = coneColor;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipPositionValue()
// PURPOSE:	return the position of the blip
/////////////////////////////////////////////////////////////////////////////////////
Vector3 CMiniMap::GetBlipPositionValue(CMiniMapBlip *pBlip)
{
	if (pBlip->IsComplex())
	{
		return ((CBlipComplex*)pBlip)->vPosition;
	}
	else
	{
		return Vector3(((CBlipSimple*)pBlip)->vPosition.x, ((CBlipSimple*)pBlip)->vPosition.y, 0.0f);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ShouldProcessMinimap
// PURPOSE:	in certain situations we dont want to update the minimap
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::ShouldProcessMinimap()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // as this method can be called on either thread, we need to check the relevant buffer here
	{
		uiAssertf(0, "CMiniMap::ShouldProcessMinimap - Graeme - didn't expect this to be called on the RenderThread");
		return false;
	}

#if __BANK
	static s32 iDebugUpdateState = -1;
#endif // __BANK

	if (MinimapModeState != MINIMAP_MODE_STATE_NONE)
	{
#if __BANK
		if (iDebugUpdateState != 1)
		{
			uiDebugf1("NOT UPDATING MINIMAP BECAUSE: MinimapModeState != MINIMAP_MODE_STATE_NONE (%d)", MinimapModeState);
			iDebugUpdateState = 1;
		}
#endif // __BANK

		return false;
	}

	if (!IsInPauseMap())
	{
#if __BANK
		if (TiledScreenCapture::IsEnabled() ||
			LightProbe::IsCapturingPanorama())
		{
#if __BANK
			if (iDebugUpdateState != 2)
			{
				uiDebugf1("NOT UPDATING MINIMAP BECAUSE: TiledScreenCapture::IsEnabled()");
				iDebugUpdateState = 2;
			}
#endif //__BANK
			
			return false;
		}

		if (!ms_g_bDisplayGameMiniMap)
		{
#if __BANK
			if (iDebugUpdateState != 3)
			{
				uiDebugf1("NOT UPDATING MINIMAP BECAUSE: !ms_g_bDisplayGameMiniMap");
				iDebugUpdateState = 3;
			}
#endif //__BANK

			return false;
		}
#endif // __BANK

#if COMPANION_APP
		if (!CCompanionData::GetInstance()->IsConnected())
		{
#endif	//	COMPANION_APP
			if (!CPauseMenu::GetMenuPreference(PREF_RADAR_MODE))
			{
#if __BANK
				if (iDebugUpdateState != 4)
				{
					uiDebugf1("NOT UPDATING MINIMAP BECAUSE: !CPauseMenu::GetMenuPreference(PREF_RADAR_MODE)");
					iDebugUpdateState = 4;
				}
#endif //__BANK
			
				return false;
			}
#if COMPANION_APP
		}
#endif	//	COMPANION_APP
	}
	else
	{
		if (!CPauseMenu::IsInMapScreen())  // if were are in the pausemenu map mode, then check we are actually on the map screen
			return false;
	}

#if __BANK
	if (iDebugUpdateState != 0)
	{
		uiDebugf1("NOW UPDATING MINIMAP");

		iDebugUpdateState = 0;
	}
#endif // __BANK

	return true;  // we should render/update the minimap
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ShouldRenderMinimap
// PURPOSE:	in certain situations we dont want to render the minimap
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::ShouldRenderMinimap()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // as this method can be called on either thread, we need to check the relevant buffer here
	{
		uiAssertf(0, "CMiniMap::ShouldRenderMinimap - Graeme - didn't expect this to be called on the RenderThread");
		return false;
	}

#if RSG_PC
	if (g_rlPc.IsUiShowing())
	{
		return false;
	}
#endif

#if __BANK
	static s32 iDebugRenderingState = -1;
#endif // __BANK

	if (sMiniMapMenuComponent.IsActive())  // always show minimap if in custom screen so it appears in the menu, regardless of anything else
	{
		return true;
	}

 	if (!IsInPauseMap())
	{
#if __BANK
		if (TiledScreenCapture::IsEnabled() ||
			LightProbe::IsCapturingPanorama())
		{
#if __BANK
			if (iDebugRenderingState != 2)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: TiledScreenCapture::IsEnabled() or LightProbe::IsCapturingPanorama()");
				iDebugRenderingState = 2;
			}
#endif //__BANK
			
			return false;
		}

		if (!ms_g_bDisplayGameMiniMap)
		{
#if __BANK
			if (iDebugRenderingState != 3)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: !ms_g_bDisplayGameMiniMap");
				iDebugRenderingState = 3;
			}
#endif //__BANK

			return false;
		}
#endif // __BANK

		if (!CScriptHud::bDisplayRadar)
		{
#if __BANK
			if (iDebugRenderingState != 4)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: !CScriptHud::bDisplayRadar");
				iDebugRenderingState = 4;
			}
#endif //__BANK

			return false;
		}

		if (!bVisible)
		{
#if __BANK
			if (iDebugRenderingState != 5)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: !bVisible");
				iDebugRenderingState = 5;
			}
#endif //__BANK
			
			return false;
		}

		if (CWarningScreen::IsActive())
		{
#if __BANK
			if (iDebugRenderingState != 6)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: CWarningScreen::IsActive()");
				iDebugRenderingState = 6;
			}
#endif //__BANK
			
			return false;
		}

		if (gVpMan.AreWidescreenBordersActive())
		{
#if __BANK
			if (iDebugRenderingState != 7)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: gVpMan.AreWidescreenBordersActive()");
				iDebugRenderingState = 7;
			}
#endif //__BANK
			
			return false;
		}

		if (CScriptHud::bDontDisplayHudOrRadarThisFrame)
		{
#if __BANK
			if (iDebugRenderingState != 8)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: CScriptHud::bDontDisplayHudOrRadarThisFrame");
				iDebugRenderingState = 8;
			}
#endif //__BANK
			
			return false;
		}

		if (!CPauseMenu::GetMenuPreference(PREF_RADAR_MODE))
		{
#if __BANK
			if (iDebugRenderingState != 9)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: !CPauseMenu::GetMenuPreference(PREF_RADAR_MODE)");
				iDebugRenderingState = 9;
			}
#endif //__BANK
			
			return false;
		}

		if (!NetworkInterface::IsGameInProgress())  // keep minimap on in these cases in MP
		{
			if (!CGameLogic::IsGameStateInPlay())
			{
#if __BANK
				if (iDebugRenderingState != 10)
				{
					uiDebugf1("NOT RENDERING MINIMAP BECAUSE: !CGameLogic::IsGameStateInPlay()");
					iDebugRenderingState = 10;
				}
#endif //__BANK
				
				return false;
			}

			CPed *pLocalPlayer = GetMiniMapPed();
			if (pLocalPlayer && (pLocalPlayer->ShouldBeDead() || pLocalPlayer->GetIsArrested()))
			{
#if __BANK
				if (iDebugRenderingState != 11)
				{
					uiDebugf1("NOT RENDERING MINIMAP BECAUSE: (pLocalPlayer && (pLocalPlayer->ShouldBeDead() || pLocalPlayer->GetIsArrested()))");
					iDebugRenderingState = 11;
				}
#endif //__BANK
				
				return false;
			}
		}

		if (camInterface::IsFadedOut())
		{
#if __BANK
			if (iDebugRenderingState != 12)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: camInterface::IsFadedOut()");
				iDebugRenderingState = 12;
			}
#endif //__BANK
			
			return false;
		}

		if ( (CutSceneManager::GetInstance()) && (CutSceneManager::GetInstance()->IsRunning()) && (!CutSceneManager::GetInstance()->GetDisplayMiniMapThisUpdate()) )
		{
#if __BANK
			if (iDebugRenderingState != 13)
			{
				uiDebugf1("NOT RENDERING MINIMAP BECAUSE: cutscene running and it doesnt need to show");
				iDebugRenderingState = 13;
			}
#endif //__BANK

			return( false );
		}
	}
	else
	{
		if (!CPauseMenu::IsInMapScreen())  // if were are in the pausemenu map mode, then check we are actually on the map screen
			return false;
	}

#if __BANK
	if (iDebugRenderingState != 0)
	{
		uiDebugf1("NOW RENDERING MINIMAP");

		iDebugRenderingState = 0;
	}
#endif // __BANK

	return true;  // we should render/update the minimap
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ShouldAddSonarBlips
// PURPOSE:	Determines whether sonar blips should be added
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::ShouldAddSonarBlips()
{
#if __BANK
	if (bDebugAlwaysDrawSonarBlips)
	{
		return true;
	}
#endif //__BANK

	if (ms_bForceSonarBlipsOn)
	{
		ResetFlags();
		return true;
	}

	CPed* pLocalPlayer = GetMiniMapPed();
	if (!pLocalPlayer)
	{
#if __BANK
		if (bDebugVisibilityOfSonarBlips)
		{
			uiDisplayf("MiniMap: SONAR_BLIP - Not adding sonar blip because No Local Player");
		}
#endif

		return false;
	}

#if DEBUG_DRAW
	if (bDebugDrawSonarBlipRange)
	{
		grcDebugDraw::Circle(VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()), CMiniMap::sm_Tunables.Sonar.fMinListenerRangeToDrawSonarBlips, Color_bisque3, Vector3(1,0,0), Vector3(0,1,0));
	}
#endif // DEBUG_DRAW

	if (pLocalPlayer->IsUsingStealthMode())
	{
		return true;
	}

	CEntityScannerIterator entityList = pLocalPlayer->GetPedIntelligence()->GetNearbyPeds();
	for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		CPed* pNearbyPed = static_cast<CPed*>(pEnt);
		if (pNearbyPed)
		{
			const bool bIncludeDislike = false;

			bool bIsThreatenedBy = pNearbyPed->GetPedIntelligence()->IsThreatenedBy(*pLocalPlayer, bIncludeDislike);
			bool bIsInCombatWith = false;
			if (!bIsThreatenedBy)
			{
				//Check if the target is the player.
				const CEntity* pTarget = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
				bIsInCombatWith = (pTarget && pTarget == pLocalPlayer);
			}

			if (bIsThreatenedBy || bIsInCombatWith)
			{
				// If this is a network game, skip this ped if it isn't another player
				if (NetworkInterface::IsGameInProgress() && !pNearbyPed->IsPlayer())
				{
					continue;
				}

				Vec3V vNearbyPedPos = pNearbyPed->GetTransform().GetPosition();
				ScalarV vDist2 = DistSquared(vNearbyPedPos, pLocalPlayer->GetTransform().GetPosition());
				if (IsLessThanAll(vDist2, ScalarV(square(CMiniMap::sm_Tunables.Sonar.fMinListenerRangeToDrawSonarBlips))))
				{
#if DEBUG_DRAW
						if (bDebugDrawSonarBlipRange)
						{
							grcDebugDraw::Line(VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(vNearbyPedPos), Color_bisque3);
						}
#endif // DEBUG_DRAW
					return true;
				}
			}
		}
	}

#if __BANK
		if (bDebugVisibilityOfSonarBlips)
		{
			uiDisplayf("MiniMap: SONAR_BLIP - Not adding sonar blip because of nearby ped checks failed");
		}
#endif
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateBlips
// PURPOSE:	updates all the blips
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateBlips()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap:: UpdateBlips can only be called on the UpdateThread!");
		return;
	}

	CBlipUpdateQueue::QueueLock lock(CMiniMap_Common::GetBlipUpdateQueue());

	CMiniMap_Common::EmptyBlipUpdateQueue();

	//
	// self-destructing sonar blips
	//

	bool bHasRedBlipActive = false;

	// B*1563553 - Don't draw other sonar blips when a red sonar blip is active  [7/19/2013 mdawe]
	for (s32 i = 0; i < ms_SonarBlips.GetCount(); i++)
	{
		bHasRedBlipActive |= CHudColour::IsHudColourRed(ms_SonarBlips[i].iHudColour);
	}

	for (s32 i = 0; i < ms_SonarBlips.GetCount(); i++)
	{
		// if still set to rendered from the previous frame then it hasnt been re-used, so delete it here. Also check if a red blip is active, as red blips should kill other ones.
		if (ms_SonarBlips[i].iFramesAlive > 0 || bHasRedBlipActive)  
		{
			ms_SonarBlips[i].iFramesAlive++;

			bool bDeleteSonarBlip = false;

			static const s32 iNumFramesToKeepSonarBlipsAlive = 70;  // 70 frames should be enough to keep them alive for
			bDeleteSonarBlip |= (ms_SonarBlips[i].iFramesAlive > iNumFramesToKeepSonarBlipsAlive);
			bDeleteSonarBlip |= (ms_SonarBlips[i].pPed && ms_SonarBlips[i].pPed->IsDead());
			bDeleteSonarBlip |= (bHasRedBlipActive && !CHudColour::IsHudColourRed(ms_SonarBlips[i].iHudColour));

			if (bDeleteSonarBlip)
			{
				ms_SonarBlips.DeleteFast(i);
				i = 0;
				continue;
			}
		}
	}

	if (ShouldRenderMinimap() && GetInStealthMode())
	{
		for (s32 i = 0; i < ms_SonarBlips.GetCount(); i++)
		{
			if (uiVerifyf(ms_SonarBlips[i].fSize > 0.0f, "Invalid size %f for sonar blip.", ms_SonarBlips[i].fSize))
			{
				s32 iFrameCheck = (s32)(floor)(ms_SonarBlips[i].fSize / 5.0f);  // work out how long to allow the existing sized blip to continue before re-sizing
				if (ms_SonarBlips[i].iFramesAlive <= (iFrameCheck-1))
				{
					// update position based on ped if we have one
					if (ms_SonarBlips[i].pPed && ms_SonarBlips[i].bShouldOverridePos)
					{
						Vector3 vPedPos = (VEC3V_TO_VECTOR3(ms_SonarBlips[i].pPed->GetTransform().GetPosition()));
						ms_SonarBlips[i].vPos = Vector2(vPedPos.x, vPedPos.y);
					}

					DLC(CMiniMap_AddSonarBlipToStage, (ms_SonarBlips[i]));
				}
			}

			if (ms_SonarBlips[i].iFramesAlive == 0)  // only send thru if not already rendered
			{
				ms_SonarBlips[i].iFramesAlive = 1;
			}
		}
	}




	//
	// standard blips:
	//

	s32 iRotation = 0;

	if (!IsInPauseMap() && !ms_bBigMapFullScreen)
	{
		if (GetLockedAngle() == -1)
		{
			iRotation = (s32)(camInterface::ComputeMiniMapHeading() * RtoD);
		}
		else
		{
			iRotation = GetLockedAngle();
		}
	}

	DLC(CMiniMap_ResetBlipConeFlags, () );

	ms_blipCones.ClearUsedFlagsForAllBlipCones();

#if __DEV
	u32 culledBlips = 0;
	u32 totalBlips = 0;
#endif

	for (s32 iIndex = 0; iIndex < iMaxCreatedBlips; iIndex++)
	{
		if (iIndex == iSimpleBlip)
			continue;

		CMiniMapBlip *pBlip = GetBlipFromActualId(iIndex);

		if (!pBlip)
			continue;

		// Prefetch next blip
		if(iIndex < (iMaxCreatedBlips-1))
		{
			int nextIndex = iIndex+1;
			if (nextIndex != iSimpleBlip)
			{
				CMiniMapBlip *pBlip = GetBlipFromActualId(nextIndex);

				if (pBlip)
					PrefetchObject(pBlip);
			}
		}

		DEV_ONLY(totalBlips++);

		//
		// update specific blip info based on entity details:
		//
		CEntity *pEntity = NULL;		

		bool bBLIP_FLAG_VALUE_REMOVE_FROM_STAGE = IsFlagSet(pBlip, BLIP_FLAG_VALUE_REMOVE_FROM_STAGE);
		bool bBLIP_FLAG_VALUE_CHANGED_FLASH = CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);
		if (!bBLIP_FLAG_VALUE_REMOVE_FROM_STAGE)
		{   
			// The blip may have been marked for removal when its entity was deleted so don't try to use the entity
			eBLIP_TYPE iBlipType = GetBlipTypeValue(pBlip);
			if (iBlipType == BLIP_TYPE_CAR || iBlipType == BLIP_TYPE_CHAR || iBlipType == BLIP_TYPE_OBJECT || iBlipType == BLIP_TYPE_PICKUP_OBJECT || iBlipType == BLIP_TYPE_COP || iBlipType == BLIP_TYPE_STEALTH)
			{
				pEntity = FindBlipEntity(pBlip);

				//
				// update the heading indicator if it is attached to an entity
				//
				if (IsFlagSet(pBlip, BLIP_FLAG_SHOW_HEADING_INDICATOR))
				{
					if (pEntity)
					{
						SetBlipDirectionValue(pBlip, (pEntity->GetTransform().GetHeading() * RtoD));
					}
				}
			}
			else if (iBlipType == BLIP_TYPE_PICKUP)
			{
				pEntity = NULL;

				CPickupPlacement* pPlacement = GetPickupPlacementFromBlip(pBlip);
				Assertf(pPlacement, "CMiniMap: pickup doesn't exist");

				if (pPlacement)
				{
					// Use the pickup position if a pickup has created. So the blip shows its real position.
					CPickup* pPickup = pPlacement->GetPickup();
					if(pPickup)
					{
						SetBlipPositionValue(pBlip, VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()));
					}
					else
					{
						SetBlipPositionValue(pBlip, pPlacement->GetPickupPosition());
					}
				}
			}

			if (CPauseMenu::IsInMapScreen() && GetBlipCategoryValue(pBlip) == BLIP_CATEGORY_WAYPOINT &&
				ms_bCentreBlipPosChangedThisFrame)
			{
				CMapMenu::UpdateLegendDisplayItem(pBlip);
			}
		}

		if (pEntity)
		{
			Vector3 vEntityPos = (VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));

			if (pEntity->GetIsTypePed())
			{
				CPed *pPed = static_cast<CPed*>(pEntity);
				if (pPed)
				{
					if (IsFlagSet(pBlip, BLIP_FLAG_SHOW_CONE))
					{
						SetupConeDetail(pBlip, pPed);  // we need to setup the cone data on the UT
					}
					
					// Only update BLIP_CATEGORY_PLAYER legend item if local/remote player's positions have changed
					if(CPauseMenu::IsInMapScreen() && GetBlipCategoryValue(pBlip) == BLIP_CATEGORY_PLAYER &&
						(ms_bCentreBlipPosChangedThisFrame || GetBlipPositionValue(pBlip) != vEntityPos))
					{
						CMapMenu::UpdateLegendDisplayItem(pBlip);
					}

					// In MP override the player position via the network position getter.  That will provide us with the same position for player blips if two remote players are in the same vehicle
					// This runs through network code because vehicle entities don't exist if the local player is viewing two remote players in the same vehicle from across the world
					if(pPed->IsAPlayerPed() && NetworkInterface::IsGameInProgress())
					{
						NetworkInterface::GetPlayerPositionForBlip(*pPed, vEntityPos);
					}
				}
			}
			else
			{
				if (IsFlagSet(pBlip, BLIP_FLAG_SHOW_CONE))
				{
					SetupFakeConeDetails(pBlip);
				}
			}

			SetBlipPositionValue(pBlip, vEntityPos);
		}
		else
		{
			// Update BLIP_CATEGORY_PLAYER legend item if remote player is in a building and local player's position has changed
			if (CPauseMenu::IsInMapScreen() && GetBlipCategoryValue(pBlip) == BLIP_CATEGORY_PLAYER && ms_bCentreBlipPosChangedThisFrame)
			{
				CMapMenu::UpdateLegendDisplayItem(pBlip);
			}
		}

		// test stealth blips
#if __BANK
		if (GetBlipTypeValue(pBlip) == BLIP_TYPE_STEALTH && iDebugSonarStyle != SONAR_STYLE_NONE)
		{
			if (pEntity && pEntity->GetIsTypePed())
			{
				CPed *pPed = static_cast<CPed*>(pEntity);

				if (pPed)
				{
					Vector3 vCurrentBlipPos(GetBlipPositionValue(pBlip));	

#define __MOVEMENT_PRECISION	(2.5f)

					Vector3 vStartingBlipPos(GetBlipPositionValueOnStage(pBlip));

					vStartingBlipPos.x = ceilf(vStartingBlipPos.x * __MOVEMENT_PRECISION) / __MOVEMENT_PRECISION;
					vStartingBlipPos.y = ceilf(vStartingBlipPos.y * __MOVEMENT_PRECISION) / __MOVEMENT_PRECISION;
					vStartingBlipPos.z = ceilf(vStartingBlipPos.z * __MOVEMENT_PRECISION) / __MOVEMENT_PRECISION;
					vCurrentBlipPos.x = ceilf(vCurrentBlipPos.x * __MOVEMENT_PRECISION) / __MOVEMENT_PRECISION;
					vCurrentBlipPos.y = ceilf(vCurrentBlipPos.y * __MOVEMENT_PRECISION) / __MOVEMENT_PRECISION;
					vCurrentBlipPos.z = ceilf(vCurrentBlipPos.z * __MOVEMENT_PRECISION) / __MOVEMENT_PRECISION;

					bool bFiring = false;
					const CTaskGun* gunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
					if(gunTask)
					{
						bFiring = (gunTask->GetIsFiring());
					}

					bool bActivateVisualStealthDisplay = false;

					bActivateVisualStealthDisplay = (vStartingBlipPos != vCurrentBlipPos || bFiring);

					if (iDebugSonarStyle == SONAR_STYLE_RED_BLIPS_FLASH_5_SECONDS)
					{
						if (pPed->IsDead())
						{
							bActivateVisualStealthDisplay = true;
						}
					}

					if (bActivateVisualStealthDisplay)
					{
						if (iDebugSonarStyle == SONAR_STYLE_SONAR_ONLY)
						{
							if (GetBlipDisplayValue(pBlip) != BLIP_DISPLAY_NEITHER)
							{
								SetBlipDisplayValue(pBlip, BLIP_DISPLAY_NEITHER);
							}
						}
						else
						{
							if (GetBlipDisplayValue(pBlip) != BLIP_DISPLAY_BOTH)
							{
								SetBlipDisplayValue(pBlip, BLIP_DISPLAY_BOTH);
							}
						}

						if (iDebugSonarStyle == SONAR_STYLE_RED_BLIPS_SONAR || iDebugSonarStyle == SONAR_STYLE_SONAR_ONLY)
						{
							CreateSonarBlip(GetBlipPositionValue(pBlip), CMiniMap::sm_Tunables.Sonar.fSoundRange_MostlyAudible, HUD_COLOUR_BLACK);
						}

						if (iDebugSonarStyle == SONAR_STYLE_RED_BLIPS_FLASH)  // flash for 1 second
						{
							if (!IsFlagSet(pBlip, BLIP_FLAG_FLASHING))
							{
								SetBlipFlashDuration(pBlip, 1000);
								SetBlipFlashInterval(pBlip, 200);
								SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);
								bBLIP_FLAG_VALUE_CHANGED_FLASH = true;
							}
						}

						if (iDebugSonarStyle == SONAR_STYLE_RED_BLIPS_FLASH_5_SECONDS)  // flash for 1 second
						{
							if (!IsFlagSet(pBlip, BLIP_FLAG_FLASHING))
							{
								SetBlipFlashDuration(pBlip, 5000);
								SetBlipFlashInterval(pBlip, 200);
								SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);
								bBLIP_FLAG_VALUE_CHANGED_FLASH = true;
							}

							if (pPed->IsDead())
							{
								SetBlipFlashDuration(pBlip, MAX_UINT16);
							}
						}

						if (iDebugSonarStyle != SONAR_STYLE_RED_BLIPS_FLASH_5_SECONDS && iDebugSonarStyle != SONAR_STYLE_RED_BLIPS_FLASH)
						{
							if (IsFlagSet(pBlip, BLIP_FLAG_FLASHING))
							{
								SetBlipFlashInterval(pBlip, DEFAULT_BLIP_FLASH_INTERVAL);
								SetBlipFlashDuration(pBlip, MAX_UINT16);
								UnsetFlag(pBlip, BLIP_FLAG_FLASHING);
							}
						}
					}

					if (iDebugSonarStyle != SONAR_STYLE_RED_BLIPS_FLASH_5_SECONDS && iDebugSonarStyle != SONAR_STYLE_RED_BLIPS_FLASH)
					{
						if (IsFlagSet(pBlip, BLIP_FLAG_FLASHING))
						{
							SetBlipFlashInterval(pBlip, DEFAULT_BLIP_FLASH_INTERVAL);
							SetBlipFlashDuration(pBlip, MAX_UINT16);
							UnsetFlag(pBlip, BLIP_FLAG_FLASHING);
						}
					}
				}
			}
		}
#endif // #if __BANK

		// UpdateIndividualBlip
		if ( (GetUniqueBlipUsed(pBlip) != INVALID_BLIP_ID) && (!IsFlagSet(pBlip, BLIP_FLAG_VALUE_DESTROY_BLIP_OBJECT)) )
		{			
			bool dlcBlip = true; // do we want to send the blip to the render thread for additional processing
			bool setExecutedLastFrame = true; // do we want to set the flag that we sent the blip to the render thread
			bool cullRemoveFromStage = false; // are we removing the blip from the stage because it's being culled, but was sent to the render thread last frame

			BANK_ONLY(if(ms_bCullDistantBlips))
			{
				if(bBLIP_FLAG_VALUE_REMOVE_FROM_STAGE)
				{
					// do nothing, these blips need to be removed from the stage on the renderthread
				}
				else
				{
					bool cullBlip = false;
					// use stored map position
					bool bRadiusBlip = (GetBlipTypeValue(pBlip) == BLIP_TYPE_RADIUS || GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA);
					Vector3 vBlipPos = GetBlipPositionValue(pBlip);
					Vector3 ms_vMapPosition2 = ms_vMapPosition;

					vBlipPos.z = 0.0f;
					ms_vMapPosition2.z = 0.0f;

					if( (!bRadiusBlip) && (vBlipPos.Dist2(ms_vMapPosition2) > rage::square(ms_fMaxDistance)) )
					{
						if (IsInPauseMap())
						{
							if (GetIsInsideInterior())
							{
								CPed *pLocalPlayer = CMiniMap::GetMiniMapPed();
								CPortalTracker* pPT = const_cast<CPortalTracker*>(pLocalPlayer ? pLocalPlayer->GetPortalTracker() : NULL);

								Vector3 vInteriorBoundMin;
								Vector3 vInteriorBoundMax;

								if (pPT && pPT->IsInsideInterior())
								{
									CInteriorInst* pIntInst = pLocalPlayer->GetPortalTracker()->GetInteriorInst();
									if (pIntInst)
									{
										vInteriorBoundMin = pIntInst->GetBoundingBoxMin();
										vInteriorBoundMax = pIntInst->GetBoundingBoxMax();
									}
								}

								if (GetBlipTypeValue(pBlip) == BLIP_TYPE_COORDS)
								{
									if (vBlipPos.GetX() < vInteriorBoundMin.x ||
										vBlipPos.GetX() > vInteriorBoundMax.x ||
										vBlipPos.GetY() < vInteriorBoundMin.y ||
										vBlipPos.GetY() > vInteriorBoundMax.y)
									{
										cullBlip = true;
									}
								}
							}
							else
							{
								cullBlip = true;
							}
						}
						else if (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_SHORTRANGE) && (!CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_FLASHING)) && (!bBLIP_FLAG_VALUE_CHANGED_FLASH))
						{
							cullBlip = true;
						}

						if(cullBlip)
						{
							if(IsFlagSet(pBlip, BLIP_FLAG_EXECUTED_VIA_DLC_LAST_FRAME))
							{
								// blip might be on the stage - remove it temporarily so it doesn't become a zombie blip
								UnsetFlag(pBlip, BLIP_FLAG_EXECUTED_VIA_DLC_LAST_FRAME);
								SetFlag(pBlip, BLIP_FLAG_VALUE_REMOVE_FROM_STAGE);
								bBLIP_FLAG_VALUE_REMOVE_FROM_STAGE = true;
								cullRemoveFromStage = true;
								setExecutedLastFrame = false;
							}
							else
							{
								// we don't need this blip
								dlcBlip = false;
							}
						}
					}
				}				
			}

			if(dlcBlip)
			{
				CMiniMap_Common::AddBlipToUpdate(*((CBlipComplex*)pBlip));
				//DLC(CMiniMap_Blip_Update, ((CBlipComplex*)pBlip, -iRotation) );
				if(setExecutedLastFrame)
				{
					// we sent this blip to the RT this frame, so if we cull it next frame we'll need to remove it from the stage
					SetFlag(pBlip, BLIP_FLAG_EXECUTED_VIA_DLC_LAST_FRAME);
				}

				if(cullRemoveFromStage)
				{
					// clear our remove flag if we just set it above, to keep from destroying our blips
					UnsetFlag(pBlip, BLIP_FLAG_VALUE_REMOVE_FROM_STAGE);
					bBLIP_FLAG_VALUE_REMOVE_FROM_STAGE = false;
				}
					
			}
#if __DEV
			else
			{
				culledBlips++;
			}
#endif
#if __BANK
			if(ms_bShowBlipVectorMap)
			{				
				CVectorMap::DrawMarker(GetBlipPositionValue(pBlip), dlcBlip? Color_green : Color_purple, 10.f);
			}
#endif
		}

		if (bBLIP_FLAG_VALUE_REMOVE_FROM_STAGE)
		{
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_REMOVE_FROM_STAGE);

			UnsetFlag(pBlip, BLIP_FLAG_FLASHING);  // set as no longer flashing as its been removed from the stage
			SetFlag(pBlip, BLIP_FLAG_VALUE_DESTROY_BLIP_OBJECT);
		}
		if (bBLIP_FLAG_VALUE_CHANGED_FLASH)
		{
			if (!IsFlagSet(pBlip,BLIP_FLAG_FLASHING))
				SetFlag(pBlip, BLIP_FLAG_FLASHING);
			else
				UnsetFlag(pBlip, BLIP_FLAG_FLASHING);

			UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);
		}

		UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);

		// silly to check for set then force it off anyway
		UnsetFlag(pBlip, BLIP_FLAG_UPDATE_STAGE_DEPTH);

		//optimization: gate all these low freq checks with one
		if (IsFlagSet(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS))
		{
			UnsetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_REINIT_STAGE);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_TICK);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_GOLD_TICK);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FOR_SALE);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_PULSE);
			UnsetFlag(pBlip, BLIP_FLAG_VALUE_SET_NUMBER);
		}

#if __BANK
		UnsetFlag(pBlip, BLIP_FLAG_VALUE_SET_LABEL);
		UnsetFlag(pBlip, BLIP_FLAG_VALUE_REMOVE_LABEL);
#endif // __BANK

		// update any flash durations set on this blip:
		UpdateAndAutoSwitchOffFlash(pBlip);
	}

	ms_blipCones.RemoveUnusedBlipCones();
#if COMPANION_APP
	// Call Companion to listen for messages and notifications
	CCompanionData::GetInstance()->Update();
	//	Update blips on Companion App here
	SendBlipsToCompanionApp();
#endif	//	COMPANION_APP

	DLC(CMiniMap_UpdateBlips, () );
	DLC(CMiniMap_RemoveUnusedBlipConesFromStage, () );

#if __DEV
	if(ms_bCullDistantBlips)
		grcDebugDraw::AddDebugOutput("Culling %u of %u blips", culledBlips, totalBlips);
#endif

#if __BANK
	if(ms_bShowBlipVectorMap)
	{
		// draw our vector map
		CVectorMap::DrawMarker(ms_vMapPosition, Color_green, 10.f);		

		CVectorMap::DrawCircle(ms_vMapPosition, ms_fMaxDistance, Color_red, false );		
	}
#endif
}

// #include "profile/element.h"
// #include "profile/group.h"
// #include "profile/page.h"

//	Timing stuff
// PF_PAGE(RobM, "AA RobM Stuff");
// PF_GROUP(RobMSend);
// PF_LINK(RobM, RobMSend);
// PF_TIMER(RobMSendCost, RobMSend);

void CMiniMap::SendBlipsToCompanionApp()
{
#if COMPANION_APP
// 	PF_FUNC(RobMSendCost);
	if (CCompanionData::GetInstance()->IsConnected())
	{
		for (s32 iIndex = 0; iIndex < iMaxCreatedBlips; iIndex++)
		{
			if (iIndex == iSimpleBlip)
				continue;

			CMiniMapBlip *pBlip = GetBlipFromActualId(iIndex);

			if (!pBlip)
				continue;

			if (pBlip->IsComplex())
			{
				//	Add the new blip
				if (!IsFlagSet(pBlip, BLIP_FLAG_VALUE_REMOVE_FROM_STAGE) && !IsFlagSet(pBlip, BLIP_FLAG_VALUE_DESTROY_BLIP_OBJECT))
				{
					Color32 colour;

					if (GetBlipColourValue(pBlip) != BLIP_COLOUR_USE_COLOUR32)
					{
						colour = CMiniMap_Common::GetColourFromBlipSettings(GetBlipColourValue(pBlip), IsFlagSet(pBlip, BLIP_FLAG_BRIGHTNESS));
					}
					else
					{
						colour = GetBlipColour32Value(pBlip);  // use secondary colour
					}

					CCompanionData::GetInstance()->AddBlip(1, (CBlipComplex*)pBlip, colour);
				}
			}
		}
	}
#endif	//	COMPANION_APP
}


void CMiniMap::ResetFlags()
{
	ms_bForceSonarBlipsOn = false;
}


void CMiniMap::SetupConeDetail(CMiniMapBlip *pBlip, CPed *pPed)
{
	if (!uiVerifyf(pPed,"[PEDCONES] Blip Actual: %i Unique: %i couldn't update a visibility cone because there was no ped for me", pBlip->m_iActualId, pBlip->m_iUniqueId))
		return;

	CPedPerception *pPedPerception = &pPed->GetPedIntelligence()->GetPedPerception();

	if (!uiVerifyf(pPedPerception, "[PEDCONES] Blip Actual: %i Unique: %i couldn't update a visibility cone because ped %s had no CPedPerception", pBlip->m_iActualId, pBlip->m_iUniqueId, pPed->GetDebugName()))
		return;

	// work out the coords for the cone:
	const float fVisualFieldMinAzimuthAngle	= pPedPerception->GetIsHighlyPerceptive() ? -PI : pPedPerception->GetVisualFieldMinAzimuthAngle() * DtoR;
	const float fVisualFieldMaxAzimuthAngle	= pPedPerception->GetIsHighlyPerceptive() ? PI : pPedPerception->GetVisualFieldMaxAzimuthAngle() * DtoR;
	const float fCentreOfGazeMaxAngle = pPedPerception->GetCentreOfGazeMaxAngle() * DtoR;

	// Calculate the peripheral and focus ranges to render the debug lines
	float fPeripheralRange = pPedPerception->GetSeeingRangePeripheral();
	float fFocusRange = pPedPerception->GetSeeingRange();
	pPedPerception->CalculatePerceptionDistances(fPeripheralRange, fFocusRange);

	// B*2343564: If any of the overriden cop perception parameters have changed, force re-draw the cone (not just the range value).
	bool bRedrawIfAnyConeParametersChange = CPedPerception::ms_bCopOverridesSet && pPed->IsCopPed();
	bool bForceRedraw = false;
	if (ms_blipCones.UpdateBlipConeParameters(GetUniqueBlipUsed(pBlip), fFocusRange, bRedrawIfAnyConeParametersChange, fPeripheralRange, fCentreOfGazeMaxAngle, fVisualFieldMinAzimuthAngle, fVisualFieldMaxAzimuthAngle))
	{
		bForceRedraw = true;
	}

//	To calculate rotation of cone
	Matrix34 viewMatrix;
	pPedPerception->CalculateViewMatrix(viewMatrix);
	Vector3 localEuler = viewMatrix.GetEulers();

//	To position the cone
	Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);

	eHUD_COLOURS color = HUD_COLOUR_BLUEDARK;
	eHUD_COLOURS* pColor = m_ConeColors.Access(pBlip->m_iUniqueId);
	if(pColor)
	{
		color = *pColor;
		m_ConeColors.Delete(pBlip->m_iUniqueId);
	}

	CBlipCone blipCone(GetActualBlipUsed(pBlip),
		fVisualFieldMinAzimuthAngle, fVisualFieldMaxAzimuthAngle, fCentreOfGazeMaxAngle, 
		fFocusRange, fPeripheralRange, 
		vBlipPos.x, vBlipPos.y, localEuler.z, (u8)CMiniMap::GetBlipAlphaValue(pBlip), bForceRedraw, color);

	DLC(CMiniMap_RenderBlipCone, (&blipCone) );


#if __DEV
	if (ms_bRenderPedPerceptionCone)
	{
		// Render just the exterior perception ranges and interior focus boundaries so its simpler to understand
		float aAsimuthAngles[MAX_CONE_ANGLES] = {fVisualFieldMinAzimuthAngle, -fCentreOfGazeMaxAngle, fCentreOfGazeMaxAngle, fVisualFieldMaxAzimuthAngle};

		for(u32 azimuthIndex=0; azimuthIndex<MAX_CONE_ANGLES; azimuthIndex++)
		{
			float azimuthAngle = aAsimuthAngles[azimuthIndex];

			bool isCentreOfGaze = azimuthIndex == 1 || azimuthIndex == 2;
			Vector3 line(0.0f, isCentreOfGaze?fFocusRange:fPeripheralRange, 0.0f);

			float elevationAngle = 0.0f;
			line.RotateX(elevationAngle);
			line.RotateZ(azimuthAngle);

			float fNextAzimuthAngle = (azimuthIndex < (MAX_CONE_ANGLES-1))?aAsimuthAngles[azimuthIndex+1]:azimuthAngle;
			bool isArcCentreOfGaze = azimuthIndex == 1;
			Color32 focusColour = Color_cyan1;
			Color32 colour = isCentreOfGaze ? focusColour : Color_cyan2;
			Color32 arcColour = (isArcCentreOfGaze) ? focusColour : Color_cyan2;

			viewMatrix.Transform(line);
			// draw the line in the 3d world
			Vector3 startPosition(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
			grcDebugDraw::Line(startPosition, line, colour, colour);

			// Draw an arc towards the next angle
			float fAngle = azimuthAngle;
			static float ANGLE_RENDER_DIFF = PI/32.0f;
			while( fAngle < fNextAzimuthAngle )
			{
				float fNextAngle = Min(fAngle + ANGLE_RENDER_DIFF, fNextAzimuthAngle);
				Vector3 line1(0.0f, isArcCentreOfGaze?fFocusRange:fPeripheralRange, 0.0f);
				line1.RotateX(elevationAngle);
				line1.RotateZ(fAngle);
				viewMatrix.Transform(line1);
				Vector3 line2(0.0f, isArcCentreOfGaze?fFocusRange:fPeripheralRange, 0.0f);
				line2.RotateX(elevationAngle);
				line2.RotateZ(fNextAngle);
				viewMatrix.Transform(line2);
				grcDebugDraw::Line(line1, line2, arcColour, arcColour);
				fAngle = fNextAngle;
			}
		}
	}
#endif // __DEV
}

void CMiniMap::SetupFakeConeDetails(CMiniMapBlip* pBlip)
{
	const int INVALID_INDEX = -1;
	int iFakeConeInfoIndex = INVALID_INDEX;

	for(int i = 0; i < m_FakeCones.GetCount(); ++i)
	{
		if(m_FakeCones[i].pBlip == pBlip)
		{
			iFakeConeInfoIndex = i;
			break;
		}
	}

	if(iFakeConeInfoIndex == INVALID_INDEX)
	{
		uiAssertf(0, "CMiniMap::SetupFakeConeDetails - Blip Actual: %i Unique: %i couldn't update a visibility cone because there is no fake cone info.", pBlip->m_iActualId, pBlip->m_iUniqueId);
		return;
	}

	Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);

	bool bForceRedraw = false;
	if (ms_blipCones.UpdateBlipConeParameters(GetUniqueBlipUsed(pBlip), 
		m_FakeCones[iFakeConeInfoIndex].fFocusRange, 
		m_FakeCones[iFakeConeInfoIndex].bContinuousUpdate, 
		m_FakeCones[iFakeConeInfoIndex].fPeripheralRange, 
		m_FakeCones[iFakeConeInfoIndex].fCentreOfGazeMaxAngle, 
		m_FakeCones[iFakeConeInfoIndex].fVisualFieldMinAzimuthAngle, 
		m_FakeCones[iFakeConeInfoIndex].fVisualFieldMaxAzimuthAngle))
	{
		bForceRedraw = true;
	}

	CBlipCone blipCone(
		GetActualBlipUsed(pBlip), 
		m_FakeCones[iFakeConeInfoIndex].fVisualFieldMinAzimuthAngle,
		m_FakeCones[iFakeConeInfoIndex].fVisualFieldMaxAzimuthAngle,
		m_FakeCones[iFakeConeInfoIndex].fCentreOfGazeMaxAngle, 
		m_FakeCones[iFakeConeInfoIndex].fFocusRange,
		m_FakeCones[iFakeConeInfoIndex].fPeripheralRange, 
		vBlipPos.x, vBlipPos.y,
		m_FakeCones[iFakeConeInfoIndex].fRotation,
		(u8)CMiniMap::GetBlipAlphaValue(pBlip),
		bForceRedraw,
		m_FakeCones[iFakeConeInfoIndex].color);

	DLC(CMiniMap_RenderBlipCone, (&blipCone) );

	// TODO: Debug drawing if wanted
}

void CMiniMap::AddFakeConeData(int iBlipIndex, float fVisualFieldMinAzimuthAngle, float fVisualFieldMaxAzimuthAngle, float fCentreOfGazeMaxAngle, float fPeripheralRange, float fFocusRange, float fRotation, bool bContinuousUpdate, eHUD_COLOURS color)
{
	s32 iActualIndex = ConvertUniqueBlipToActualBlip(iBlipIndex);

	if (iActualIndex >= MAX_NUM_BLIPS)
	{
		uiAssertf(0, "CMiniMap::AddFakeConeData - Attempted to make cone data for blip %d (%d) over the max array (%d)!", iBlipIndex, iActualIndex, MAX_NUM_BLIPS);
		return;
	}

	if (iActualIndex < 0)
	{
		uiAssertf(0, "CMiniMap::AddFakeConeData - Invalid actual index.");
		return;
	}

	CMiniMapBlip *pBlip = GetBlipFromActualId(iActualIndex);

	if (!pBlip)
	{
		uiAssertf(0, "CMiniMap::AddFakeConeData - Invalid blip index used to get blip.");
		return;
	}

	const int INVALID_INDEX = -1;
	int iFakeConeInfoIndex = INVALID_INDEX;

	for(int i = 0; i < m_FakeCones.GetCount(); ++i)
	{
		if(m_FakeCones[i].pBlip == pBlip)
		{
			iFakeConeInfoIndex = i;
			break;
		}
	}

	if(iFakeConeInfoIndex != INVALID_INDEX)
	{
		m_FakeCones[iFakeConeInfoIndex].fVisualFieldMinAzimuthAngle = fVisualFieldMinAzimuthAngle;
		m_FakeCones[iFakeConeInfoIndex].fVisualFieldMaxAzimuthAngle = fVisualFieldMaxAzimuthAngle;
		m_FakeCones[iFakeConeInfoIndex].fCentreOfGazeMaxAngle = fCentreOfGazeMaxAngle;

		m_FakeCones[iFakeConeInfoIndex].fPeripheralRange = fPeripheralRange;
		m_FakeCones[iFakeConeInfoIndex].fFocusRange = fFocusRange;

		m_FakeCones[iFakeConeInfoIndex].fRotation = fRotation;

		m_FakeCones[iFakeConeInfoIndex].bContinuousUpdate = bContinuousUpdate;

		m_FakeCones[iFakeConeInfoIndex].color = color;
	}
	else
	{
		sMinimapFakeCone fakeCone;

		fakeCone.pBlip = pBlip;

		fakeCone.fVisualFieldMinAzimuthAngle = fVisualFieldMinAzimuthAngle;
		fakeCone.fVisualFieldMaxAzimuthAngle = fVisualFieldMaxAzimuthAngle;
		fakeCone.fCentreOfGazeMaxAngle = fCentreOfGazeMaxAngle;

		fakeCone.fPeripheralRange = fPeripheralRange;
		fakeCone.fFocusRange = fFocusRange;

		fakeCone.fRotation = fRotation;

		fakeCone.bContinuousUpdate = bContinuousUpdate;

		fakeCone.color = color;

		m_FakeCones.PushAndGrow(fakeCone);
	}
}

void CMiniMap::DestroyFakeConeData(int iBlipIndex)
{
	s32 iActualIndex = ConvertUniqueBlipToActualBlip(iBlipIndex);
	if (iActualIndex >= MAX_NUM_BLIPS)
	{
		uiAssertf(0, "CMiniMap::DestroyFakeConeData - Attempted to make cone data for blip %d (%d) over the max array (%d)!", iBlipIndex, iActualIndex, MAX_NUM_BLIPS);
		return;
	}

	if (iActualIndex < 0)
	{
		uiAssertf(0, "CMiniMap::DestroyFakeConeData - Invalid actual index.");
		return;
	}

	CMiniMapBlip *pBlip = GetBlipFromActualId(iActualIndex);

	if (!pBlip)
	{
		uiAssertf(0, "CMiniMap::DestroyFakeConeData - Invalid blip index used to get blip.");
		return;
	}

	int iFakeConeInfoIndex = -1;
	for(int i = 0; i < m_FakeCones.GetCount(); ++i)
	{
		if(m_FakeCones[i].pBlip == pBlip)
		{
			iFakeConeInfoIndex = i;
			break;
		}
	}

	if (uiVerifyf(iFakeConeInfoIndex != -1, "[PEDCONES] Blip Actual: %i Unique: %i couldn't update a visibility cone because there is no fake cone info.", pBlip->m_iActualId, pBlip->m_iUniqueId))
	{
		m_FakeCones.Delete(iFakeConeInfoIndex);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateBlipMarkers()
// PURPOSE:	updates the blip markers - adds to a list (DOES NOT RENDER THEM HERE!)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateBlipMarkers()
{
	if (IsInPauseMap())
		return;

#if __BANK
	if (!CMarkers::g_bDisplayMarkers)
		return;
#endif

	if (gVpMan.AreWidescreenBordersActive())  // dont render world markers if widescreen borders are active
		return;

	CEntity *pEntity = NULL;
	CVehicle* pVehicle = NULL;
	CPed* pPed = NULL;
	CPed * pLocalPlayer = GetMiniMapPed();
	const Vector3 &vPlayerPos = CGameWorld::FindLocalPlayerCentreOfWorld();
	float fEntityTop;
	Vector3 vecPos;

#define ARROW_OFFSET					(0.4f)
#define SIZE_OF_ARROW					(0.12f)
#define INTERIOR_HEIGHT_ABOVE_ENTITY	(0.1f)
#define MIN_HEIGHT_ADJUST				(0.7f)
#define MAX_HEIGHT_OF_MARKER_ABOVE_CAR	(0.9f)
#define MAX_HEIGHT_OF_MARKER_ABOVE_OBJ	(1.3f)
#define MIN_DISTANCE_TO_ADJUST_FROM 	(2.0f)
#define MAX_DISTANCE_TO_ADJUST_FROM 	(6.5f)

#define MAX_DISTANCE_TO_DISPLAY			(500.0f)  // only display markers on blips X metres around the player - needs to be high as we may be looking thru the sniper rifle

#define DISTANCE_TO_DISPLAY_FROM				(40.0f)
#define DISTANCE_TO_DISPLAY_FROM_MULTIPLAYER	(60.0f)
#define DISTANCE_TO_DISPLAY_FROM_LONG_DIST		(120.0f)

	float fDistFromMarker = (MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM);

	for (s32 nTrace = 0; nTrace < iMaxCreatedBlips; nTrace++)
	{
		if (nTrace == iSimpleBlip)  // dont process the simple blip
			continue;

		CMiniMapBlip *pBlip = GetBlipFromActualId(nTrace);

		if (!pBlip)
			continue;

		eBLIP_DISPLAY_TYPE displayValue = GetBlipDisplayValue(pBlip);

		if (displayValue != BLIP_DISPLAY_NEITHER)
		{
			Color32 BlipColour;

			if (GetBlipColourValue(pBlip) != BLIP_COLOUR_USE_COLOUR32)
			{
				BlipColour = CMiniMap_Common::GetColourFromBlipSettings(GetBlipColourValue(pBlip), IsFlagSet(pBlip, BLIP_FLAG_BRIGHTNESS));
			}
			else
			{
				BlipColour = GetBlipColour32Value(pBlip);
			}

			bool bPlayerInsideInterior = GetIsInsideInterior();

			switch (GetBlipTypeValue(pBlip))
			{
				case BLIP_TYPE_CONTACT:
				{
					// Coordinates get a marker on the ground
					if(!CTheScripts::GetPlayerIsOnAMission())
					{
						if ((displayValue == BLIP_DISPLAY_BOTH)
							|| (displayValue == BLIP_DISPLAY_MARKERONLY))
						{
							if ( pLocalPlayer )
							{
								// all locates that have default colour should be yellow:
								if (GetBlipColourValue(pBlip) == BLIP_COLOUR_DEFAULT)
									BlipColour = CHudColour::GetRGBA(HUD_COLOUR_YELLOWLIGHT);

								Vector3 vBlipPos(GetBlipPositionValue(pBlip));

								Vector3 deltaFromTargetXY = vPlayerPos-vBlipPos;
								deltaFromTargetXY.z = 0;
								fDistFromMarker = deltaFromTargetXY.Mag2();

								if (fDistFromMarker < MAX_DISTANCE_TO_DISPLAY*MAX_DISTANCE_TO_DISPLAY)
								{
									fDistFromMarker = rage::Min(MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM, fDistFromMarker);
									s32 iAlpha = (u32)MIN(220,rage::Floorf((rage::Max((fDistFromMarker-(MIN_DISTANCE_TO_ADJUST_FROM*MIN_DISTANCE_TO_ADJUST_FROM)),fDistFromMarker)/(MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM))*220.0f));

									MarkerInfo_t markerInfo;
									markerInfo.type = MARKERTYPE_ARROW;
									markerInfo.vPos = RCC_VEC3V(vBlipPos);
									markerInfo.vScale = Vec3V(V_TWO);
									markerInfo.col = BlipColour; 
									markerInfo.col.SetAlpha(iAlpha);
									markerInfo.faceCam = true;
									g_markers.Register(markerInfo);
								}
							}
						}
					}
					break;
				}

				case BLIP_TYPE_COORDS:
				case BLIP_TYPE_RADIUS:
				case BLIP_TYPE_AREA:
				{
					// no 3d markers for this radar blip type
					break;
				}

				case BLIP_TYPE_CAR :
				{
					pVehicle = static_cast<CVehicle*>(FindBlipEntity(pBlip));
					Assert(pVehicle);

					if (pVehicle)
					{
						if ( (displayValue == BLIP_DISPLAY_BOTH) || (displayValue == BLIP_DISPLAY_MARKERONLY) )
						{
							fEntityTop = pVehicle->GetBaseModelInfo()->GetBoundingBoxMax().z;

							if (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
							{
								fEntityTop -= 1.0f;  // little lower down for helicoptors
							}

							vecPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

							float fHeightAboveEntity = MIN_HEIGHT_ADJUST;

							vecPos.z += fEntityTop + fHeightAboveEntity + SIZE_OF_ARROW + ARROW_OFFSET;

							Vector3 deltaFromTargetXY = vPlayerPos-vecPos;
							deltaFromTargetXY.z = 0;
							fDistFromMarker = deltaFromTargetXY.Mag2();

							if (fDistFromMarker < MAX_DISTANCE_TO_DISPLAY*MAX_DISTANCE_TO_DISPLAY)
							{
								fDistFromMarker = rage::Min(MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM, fDistFromMarker);
								s32 iAlpha = (u32)MIN(220,rage::Floorf((rage::Max((fDistFromMarker-(MIN_DISTANCE_TO_ADJUST_FROM*MIN_DISTANCE_TO_ADJUST_FROM)),fDistFromMarker)/(MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM))*220.0f));

								MarkerInfo_t markerInfo;
								markerInfo.type = MARKERTYPE_ARROW;
								markerInfo.vPos = RCC_VEC3V(vecPos);
								markerInfo.vScale = Vec3V(SIZE_OF_ARROW, SIZE_OF_ARROW, SIZE_OF_ARROW);
								markerInfo.col = Color32(BlipColour.GetRed(), BlipColour.GetGreen(), BlipColour.GetBlue(), iAlpha);
								markerInfo.faceCam = true;

								g_markers.Register(markerInfo);
							}
						}
					}
					break;
				}

				case BLIP_TYPE_CHAR :
				{
					pPed = static_cast<CPed*>(FindBlipEntity(pBlip));
					fEntityTop = 0.0f;

					if (pPed)
					{
						if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
						{
							if (pPed->GetVehiclePedInside())
							{
								pEntity = (CEntity*) pPed->GetVehiclePedInside();
								fEntityTop = pEntity->GetBaseModelInfo()->GetBoundingBoxMax().z;

								if ((pPed->GetVehiclePedInside())->GetVehicleType() == VEHICLE_TYPE_HELI)
								{
									fEntityTop -= 1.0f;  // little lower down for helicoptors
								}
							}
							else
							{
								pEntity = NULL;
							}
						}
						else
						{
							pEntity = (CEntity*) pPed;
							fEntityTop = pEntity->GetBaseModelInfo()->GetBoundingBoxMax().z * 0.9f;
						}
					}
					else
					{
						pEntity = NULL;
					}

					if (pEntity != NULL && ((displayValue == BLIP_DISPLAY_BOTH)
						|| (displayValue == BLIP_DISPLAY_MARKERONLY)))
					{
						bool bEntityInsideInterior = (pPed && pPed->GetPortalTracker()->IsInsideInterior());

						// dont display if player is outside and entity is inside
						// this may cause other issues but something is needed to stop the markers
						// appearing outside celings when the player is standing on a roof
						if (!bPlayerInsideInterior && bEntityInsideInterior)
							break;

						vecPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());  // get the position of the entity

						// if entity is dynamic then check that the global matrices have been updated, if so get the position from them
						if (pEntity->GetIsDynamic())
						{
							CDynamicEntity *pDynamicEntity = static_cast<CDynamicEntity*>(pEntity);
							Assert(pDynamicEntity);

							s32 boneIndex = pDynamicEntity->GetBoneIndexFromBoneTag(BONETAG_ROOT);
							Assert(boneIndex != BONETAG_INVALID);
							if(boneIndex == BONETAG_INVALID)
								boneIndex = 0;

							Matrix34 boneMat;
							pPed->GetGlobalMtx(boneIndex, boneMat);

							vecPos = boneMat.d;
						}

						float fHeightAboveEntity = 0.0f;  // for a ped in an interior, this should be enough for it not to go thru the roof

						bool bRender = true;

						if (!bPlayerInsideInterior)
						{
							Vector3 deltaFromTargetXY = vPlayerPos-vecPos;
							deltaFromTargetXY.z = 0;
							fDistFromMarker = deltaFromTargetXY.Mag2();

							if (fDistFromMarker > MAX_DISTANCE_TO_DISPLAY*MAX_DISTANCE_TO_DISPLAY)
							{
								bRender = false;
							}
							fDistFromMarker = rage::Min(MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM, fDistFromMarker);

							fHeightAboveEntity = rage::Max(((((1.0f / (MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM))*fDistFromMarker)*(MAX_HEIGHT_OF_MARKER_ABOVE_CAR*MAX_HEIGHT_OF_MARKER_ABOVE_CAR))-MIN_HEIGHT_ADJUST), 0.0f);
						}

						vecPos.z += fEntityTop + fHeightAboveEntity + SIZE_OF_ARROW + ARROW_OFFSET;

						// lets ensure that these markers dont go halfway through buildings etc
						const Vector3 vEndPos(vecPos.x, vecPos.y, vecPos.z - SIZE_OF_ARROW);

						if (bEntityInsideInterior)
						{
							WorldProbe::CShapeTestProbeDesc probeDesc;
							probeDesc.SetStartAndEnd(vecPos, vEndPos);
							probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES); 
							bRender = (!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc));
						}

						if (bRender)
						{
							s32 iAlpha = (u32)MIN(220,rage::Floorf((rage::Max((fDistFromMarker-(MIN_DISTANCE_TO_ADJUST_FROM*MIN_DISTANCE_TO_ADJUST_FROM)),fDistFromMarker)/(MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM))*220.0f));

							MarkerInfo_t markerInfo;
							markerInfo.type = MARKERTYPE_ARROW;
							markerInfo.vPos = RCC_VEC3V(vecPos);
							markerInfo.vScale = Vec3V(SIZE_OF_ARROW, SIZE_OF_ARROW, SIZE_OF_ARROW);
							markerInfo.col = Color32(BlipColour.GetRed(), BlipColour.GetGreen(), BlipColour.GetBlue(), iAlpha);
							markerInfo.faceCam = true;

							g_markers.Register(markerInfo);
						}
					}
					break;
				}

				case BLIP_TYPE_OBJECT :
				case BLIP_TYPE_PICKUP_OBJECT :
				case BLIP_TYPE_PICKUP :
				case BLIP_TYPE_WEAPON_PICKUP:
				{
					if ((displayValue == BLIP_DISPLAY_BOTH)
						|| (displayValue == BLIP_DISPLAY_MARKERONLY))
					{
						bool bRenderMarker = true;
						float fSizeOfArrow = SIZE_OF_ARROW;

						vecPos.Set(0.0f, 0.0f, 0.0f);

						if (GetBlipTypeValue(pBlip) == BLIP_TYPE_OBJECT)
						{
							pEntity = FindBlipEntity(pBlip);
							Assertf(pEntity, "CRadar::Draw3dMarkers - object doesn't exist");

							if(pEntity)
							{
								fEntityTop = pEntity->GetBaseModelInfo()->GetBoundingBoxMax().z;
								vecPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
							}
							else
							{
								fEntityTop = 0.5f;
							}
						}
						else
						{
							Assert(GetBlipTypeValue(pBlip) == BLIP_TYPE_PICKUP || GetBlipTypeValue(pBlip) == BLIP_TYPE_WEAPON_PICKUP);

							CPickupPlacement* pPlacement = GetPickupPlacementFromBlip(pBlip);
							Assertf(pPlacement, "Blip exists for non-existing pickup placement");

							fEntityTop = 0.5f;  // default pickup height

							if (pPlacement)
							{
								vecPos = pPlacement->GetPickupPosition();
							}

							pEntity = NULL;
						}							

						if (pEntity)  // objects
						{
							Vector3 deltaFromTargetXY = vPlayerPos-vecPos;
							deltaFromTargetXY.z = 0;
							fDistFromMarker = deltaFromTargetXY.Mag2();

							if (fDistFromMarker > MAX_DISTANCE_TO_DISPLAY*MAX_DISTANCE_TO_DISPLAY)
							{
								bRenderMarker = false;
							}

							fDistFromMarker = rage::Min(MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM, fDistFromMarker);
						}
						else  // pickups
						{
							Vector3 deltaFromTargetXY = vPlayerPos-vecPos;
							deltaFromTargetXY.z = 0;
							fDistFromMarker = deltaFromTargetXY.Mag2();

							if (fDistFromMarker > MAX_DISTANCE_TO_DISPLAY*MAX_DISTANCE_TO_DISPLAY)
							{
								bRenderMarker = false;
							}

							CPickupPlacement* pPlacement = GetPickupPlacementFromBlip(pBlip);
							bool bScaled = false;
							if (pPlacement)
							{
								const CPickupData* pPickupData = pPlacement->GetPickupData();
								if (pPickupData)
								{
									bScaled = (pPickupData->GetScale() > 1.0f);
								}
							}

							if (!NetworkInterface::IsGameInProgress())
							{
								if (((!IsFlagSet(pBlip,BLIP_FLAG_MARKER_LONG_DIST)) && !bScaled) && fDistFromMarker > DISTANCE_TO_DISPLAY_FROM*DISTANCE_TO_DISPLAY_FROM)   // skip any pickups that are not close by otherwise the 3dmarkers array will just fill up (shorter distance)
									bRenderMarker = false;
							}
							else
							{
								if (((!IsFlagSet(pBlip,BLIP_FLAG_MARKER_LONG_DIST)) && !bScaled) && fDistFromMarker > DISTANCE_TO_DISPLAY_FROM_MULTIPLAYER*DISTANCE_TO_DISPLAY_FROM_MULTIPLAYER)   // skip any pickups that are not close by otherwise the 3dmarkers array will just fill up (shorter distance)
									bRenderMarker = false;
							}
							if ((bScaled || (IsFlagSet(pBlip,BLIP_FLAG_MARKER_LONG_DIST))) && fDistFromMarker > DISTANCE_TO_DISPLAY_FROM_LONG_DIST*DISTANCE_TO_DISPLAY_FROM_LONG_DIST)   // check a longer distance
								bRenderMarker = false;

							fDistFromMarker = rage::Min(MAX_DISTANCE_TO_ADJUST_FROM*MAX_DISTANCE_TO_ADJUST_FROM, fDistFromMarker);

							fSizeOfArrow *= 1.7f;  // Les wants this 3 times as big
						}

						float fHeightAboveEntity = INTERIOR_HEIGHT_ABOVE_ENTITY;

						if (!bPlayerInsideInterior)
						{   
							if (GetBlipTypeValue(pBlip) == BLIP_TYPE_WEAPON_PICKUP)
								fHeightAboveEntity = 0.2f;  // dont scale for weapon pickups
							else
								fHeightAboveEntity = MIN_HEIGHT_ADJUST;
						}

						vecPos.z += fEntityTop + fHeightAboveEntity + fSizeOfArrow + ARROW_OFFSET;

						/// const Vector3 vEndPos = Vector3(vecPos.x, vecPos.y, vecPos.z - fSizeOfArrow);

						if (bRenderMarker)
						{
							MarkerInfo_t markerInfo;
							markerInfo.type = MARKERTYPE_ARROW;
							markerInfo.vPos = RCC_VEC3V(vecPos);
							markerInfo.vScale = Vec3V(fSizeOfArrow, fSizeOfArrow, fSizeOfArrow);
							markerInfo.col = BlipColour;
							markerInfo.col.SetAlpha(220);
							markerInfo.bounce = GetBlipTypeValue(pBlip) == BLIP_TYPE_WEAPON_PICKUP;
							markerInfo.faceCam = true;

							g_markers.Register(markerInfo);
						}
					}
					break;
				}

				case BLIP_TYPE_UNUSED :
				{
					break;
				}

				default :
				{
					Assertf(0, "Blip %d has an invalid type: %d\n", nTrace, (s32)GetBlipTypeValue(pBlip)); 
					break;
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::CreateMiniMapMovie
// PURPOSE:	creates and loads the scaleform radar movie
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::CreateMiniMapMovie()
{
	CScaleformMgr::CreateMovieParams params("minimap_main_map");
	params.vPos.x = GetCurrentMiniMapPosition().x;
	params.vPos.y = GetCurrentMiniMapPosition().y;
	params.vPos.z = 0.0f;
	params.vSize = GetCurrentMiniMapSize();
	params.bRemovable = false;  // not removeable at shutdown session in scaleform automatically
	params.bInitiallyLocked = true; // don't update until we give these movies the go-ahead
	params.bIgnoreSuperWidescreenScaling = true;

	iMovieId[MINIMAP_MOVIE_BACKGROUND] = CScaleformMgr::CreateMovieAndWaitForLoad(params);

	params.cFilename = GetCurrentMiniMapFilename();

	iMovieId[MINIMAP_MOVIE_FOREGROUND] = CScaleformMgr::CreateMovieAndWaitForLoad(params); 

	if (GetMovieId(MINIMAP_MOVIE_BACKGROUND) != -1 && GetMovieId(MINIMAP_MOVIE_FOREGROUND) != -1)
	{
		CScaleformMgr::ForceMovieUpdateInstantly(GetMovieId(MINIMAP_MOVIE_BACKGROUND), false);
		CScaleformMgr::ForceMovieUpdateInstantly(GetMovieId(MINIMAP_MOVIE_FOREGROUND), false);

		CMiniMap_RenderThread::SetMovieId(MINIMAP_MOVIE_BACKGROUND, GetMovieId(MINIMAP_MOVIE_BACKGROUND));
		CMiniMap_RenderThread::SetMovieId(MINIMAP_MOVIE_FOREGROUND, GetMovieId(MINIMAP_MOVIE_FOREGROUND));
		return true;
	}
	else
	{
		return false;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::RemoveMiniMapMovie
// PURPOSE:	deletes the mini map movie
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::RemoveMiniMapMovie()
{
	for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
	{
		if (GetMovieId(i) >= 0)
		{
			CScaleformMgr::RequestRemoveMovie(GetMovieId(i));
			iMovieId[i] = -1;
			CMiniMap_RenderThread::SetMovieId(i, -1);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetBlipObjectName
// PURPOSE: sets the actionscript object name to be used for this blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetBlipObjectName(CMiniMapBlip *pBlip, const BlipLinkage& iNameId )
{
	if (pBlip && pBlip->IsComplex())
	{
		((CBlipComplex*)pBlip)->iLinkageId = iNameId;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetBlipNameValue
// PURPOSE: sets the text name value to be shown on screen
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetBlipNameValue(CMiniMapBlip *pBlip, const char *cParam, bool bFromTextFile)
{
	if (pBlip && pBlip->IsComplex())
	{
		CBlipComplex* pBlipCmplx = static_cast<CBlipComplex*>(pBlip);

		if (bFromTextFile)
		{
			pBlipCmplx->cLocName.SetKey(cParam);
		}
		else
		{
			pBlipCmplx->cLocName.SetLoc(cParam);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetBlipNameValue
// PURPOSE: sets the text name value to be shown on screen
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetBlipNameValue(CMiniMapBlip *pBlip, const atHashWithStringBank& hash)
{
	if (pBlip && pBlip->IsComplex())
	{
		CBlipComplex* pBlipCmplx = static_cast<CBlipComplex*>(pBlip);
		pBlipCmplx->cLocName.SetKey(hash);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetBlipCategoryValue
// PURPOSE: sets the category the blip is in
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetBlipCategoryValue(CMiniMapBlip *pBlip, u8 iNewBlipCategory)
{
	if (pBlip && pBlip->IsComplex())
	{
		((CBlipComplex*)pBlip)->iCategory = iNewBlipCategory;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetBlipDirectionValue
// PURPOSE: sets rotation of a blip in DEGREES
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetBlipDirectionValue(CMiniMapBlip *pBlip, float fParam, bool bClamp /*= true*/)
{
	if (pBlip && pBlip->IsComplex())
	{
		if (bClamp)
		{
			// ensure the value is between 0 and 359 degrees
			fParam = CMiniMap_Common::WrapBlipRotation(fParam);
		}

		((CBlipComplex*)pBlip)->fDirection = fParam; 
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipPoolIndexValue()
// PURPOSE:	return the pool index value
/////////////////////////////////////////////////////////////////////////////////////
CEntityPoolIndexForBlip &CMiniMap::GetBlipPoolIndexValue(CMiniMapBlip *pBlip)
{
	static CEntityPoolIndexForBlip invalidPoolIndex;

	if (pBlip && pBlip->IsComplex())
		return ((CBlipComplex*)pBlip)->m_PoolIndex;
	else
		return invalidPoolIndex;
}


CPickupPlacement *CMiniMap::GetPickupPlacementFromBlip(CMiniMapBlip *pBlip)
{
	if (pBlip && pBlip->IsComplex())
	{
		CBlipComplex *pComplexBlip = (CBlipComplex*) pBlip;
		if (uiVerifyf(pComplexBlip->type == BLIP_TYPE_PICKUP, "CMiniMap::GetPickupPlacementFromBlip - expected this blip to have type BLIP_TYPE_PICKUP"))
		{
			return pComplexBlip->m_PoolIndex.GetPickupPlacement();
		}
	}

	return NULL;
}


////////////////////////////////////////////////////////////////////////////
// name:	GetActualBlipUsed
//
// purpose: gets the actual blip used for this blip
////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::GetActualBlipUsed(const CMiniMapBlip *pBlip)
{
	if (pBlip)
	{
		return (pBlip->m_iActualId);
	}

	return -1;
}


////////////////////////////////////////////////////////////////////////////
// name:	GetUniqueBlipUsed
//
// purpose: gets the unique blip used for this blip
////////////////////////////////////////////////////////////////////////////
//	Only used for debug output in MiniMapCommon and MiniMapRenderThread
s32 CMiniMap::GetUniqueBlipUsed(const CMiniMapBlip *pBlip)
{
	if (pBlip)
	{
		return (pBlip->m_iUniqueId);
	}

	return INVALID_BLIP_ID;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipPositionValue()
// PURPOSE:	return the position of the blip
/////////////////////////////////////////////////////////////////////////////////////
Vector3 CMiniMap::GetBlipPositionValueOnStage(CMiniMapBlip *pBlip)
{
	if (uiVerifyf(pBlip, "Blip doesnt exist!"))
	{
		if (pBlip->IsComplex())
		{
			return ((CBlipComplex*)pBlip)->vPosition;
		}
		else
		{
			return Vector3(((CBlipSimple*)pBlip)->vPosition.x, ((CBlipSimple*)pBlip)->vPosition.y, 0.0f);
		}
	}

	return ORIGIN;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipObjectNameId
// PURPOSE: gets id of the object
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::GetBlipObjectNameId(CMiniMapBlip *pBlip)
{
	CBlipComplex* pCmplxBlip = static_cast<CBlipComplex*>( pBlip && pBlip->IsComplex() ? pBlip : blip[iSimpleBlip] );
#if HASHED_BLIP_IDS
	return static_cast<s32>(pCmplxBlip->iLinkageId.GetHash());
#else
	return pCmplxBlip->iLinkageId;
#endif
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipNameValue
// PURPOSE: gets the text name value to be shown on screen
/////////////////////////////////////////////////////////////////////////////////////
const char* CMiniMap::GetBlipNameValue(CMiniMapBlip *pBlip)
{
	const CBlipComplex* pBlipCmplex = static_cast<CBlipComplex*>(pBlip->IsComplex() ? pBlip : blip[iSimpleBlip] );

	return pBlipCmplex->cLocName.GetStr();
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipColourValue()
// PURPOSE:	return the colour info for this blip
/////////////////////////////////////////////////////////////////////////////////////
eBLIP_COLOURS CMiniMap::GetBlipColourValue(CMiniMapBlip *pBlip)
{
	if (pBlip && pBlip->IsComplex())
		return (eBLIP_COLOURS)((CBlipComplex*)pBlip)->iColour;
	else
		return (eBLIP_COLOURS)((CBlipComplex*)blip[iSimpleBlip])->iColour;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipAlphaValue()
// PURPOSE:	return the alpha for this blip
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::GetBlipAlphaValue(CMiniMapBlip *pBlip)
{
	if (pBlip && pBlip->IsComplex())
		return ((CBlipComplex*)pBlip)->iAlpha;
	else
		return ((CBlipComplex*)blip[iSimpleBlip])->iAlpha;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipSecondaryColourValue()
// PURPOSE:	return the rgba colour info for this blip
/////////////////////////////////////////////////////////////////////////////////////
Color32 CMiniMap::GetBlipColour32Value(CMiniMapBlip *pBlip)
{
	if (pBlip && pBlip->IsComplex())
		return ((CBlipComplex*)pBlip)->iColourSecondary;
	else
		return ((CBlipComplex*)blip[iSimpleBlip])->iColourSecondary;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipTypeValue()
// PURPOSE:	return the blip type
/////////////////////////////////////////////////////////////////////////////////////
eBLIP_TYPE CMiniMap::GetBlipTypeValue(CMiniMapBlip *pBlip)
{
	if (uiVerifyf(pBlip, "Blip doesnt exist!"))
	{
		if (pBlip->IsComplex())
			return (eBLIP_TYPE)((CBlipComplex*)pBlip)->type;
		else
			return (eBLIP_TYPE)((CBlipComplex*)blip[iSimpleBlip])->type;
	}

	return BLIP_TYPE_UNUSED;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipNumberValue()
// PURPOSE:	return the blip display type
/////////////////////////////////////////////////////////////////////////////////////
s8 CMiniMap::GetBlipNumberValue(CMiniMapBlip *pBlip)
{
	if (pBlip)
	{
		if (pBlip->IsComplex())
			return ((CBlipComplex*)pBlip)->iNumberToDisplay;
		else
			return ((CBlipComplex*)blip[iSimpleBlip])->iNumberToDisplay;
	}

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipDisplayValue()
// PURPOSE:	return the blip display type
/////////////////////////////////////////////////////////////////////////////////////
eBLIP_DISPLAY_TYPE CMiniMap::GetBlipDisplayValue(CMiniMapBlip *pBlip)
{
	if (pBlip)
	{
		if (pBlip->IsComplex())
			return (eBLIP_DISPLAY_TYPE)((CBlipComplex*)pBlip)->display;
		else
			return (eBLIP_DISPLAY_TYPE)((CBlipComplex*)blip[iSimpleBlip])->display;
	}

	return BLIP_DISPLAY_NEITHER;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipDirectionValue()
// PURPOSE:	return rotation info of this blip
/////////////////////////////////////////////////////////////////////////////////////
float CMiniMap::GetBlipDirectionValue(CMiniMapBlip *pBlip)
{
	if (pBlip && pBlip->IsComplex())
		return ((CBlipComplex*)pBlip)->fDirection;
	else
		return ((CBlipComplex*)blip[iSimpleBlip])->fDirection;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::FindBlipEntity()
// PURPOSE:	For a radar trace we identify the entity pointer (if there is one)
/////////////////////////////////////////////////////////////////////////////////////
CEntity *CMiniMap::FindBlipEntity(CMiniMapBlip *pBlip)
{
	if (uiVerifyf(pBlip, "CMiniMap::FindBlipEntity - blip doesn't exist"))
	{
		if (uiVerifyf(pBlip->IsComplex(), "CMiniMap::FindBlipEntity - blip isn't complex"))
		{
			const eBLIP_TYPE blipType = GetBlipTypeValue(pBlip);
			switch(blipType)
			{
				case BLIP_TYPE_CAR:
				case BLIP_TYPE_CHAR:
				case BLIP_TYPE_STEALTH:
				case BLIP_TYPE_OBJECT:
				case BLIP_TYPE_PICKUP_OBJECT :
				case BLIP_TYPE_COP:
					return ((CBlipComplex*)pBlip)->m_PoolIndex.GetEntity(blipType);
				default:
					break;
			}
		}
	}
	return NULL;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	SetBlipParameter()
// PURPOSE:	updates a param in the blip structure
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, s32 iParam)
{
	s32 iActualIndex = ConvertUniqueBlipToActualBlip(iIndex);

	if (iActualIndex >= MAX_NUM_BLIPS)
	{
		uiAssertf(0, "MiniMap: Attempted to update blip %d (%d) over the max array (%d)!", iIndex, iActualIndex, MAX_NUM_BLIPS);
		return;
	}

	if (iActualIndex < 0)
		return;

	CMiniMapBlip *pBlip = GetBlipFromActualId(iActualIndex);

	if (!pBlip)
		return;

	if (!pBlip->IsComplex())
	{
		uiAssertf(0, "MiniMap: Attempted to modify a Simple Blip - %d (%d)", iIndex, iActualIndex);
	}
		
	switch (iTodo)
	{
		case BLIP_CHANGE_DEBUG_NUMBER:
		{
#if __BANK
			SetBlipDebugNumberValue(pBlip, (s8)iParam);

			if (iParam != -1)
			{
				SetFlag(pBlip, BLIP_FLAG_VALUE_SET_LABEL);  // on
			}
			else
			{
				SetFlag(pBlip, BLIP_FLAG_VALUE_REMOVE_LABEL);  // off
			}
#endif // __BANK
			break;
		}

		case BLIP_CHANGE_CATEGORY:
		{
			SetBlipCategoryValue(pBlip, (u8)iParam);
			break;
		}

		case BLIP_CHANGE_PRIORITY:
		{
			SetBlipPriorityValue(pBlip, (eBLIP_PRIORITY)iParam);
			SetFlag(pBlip, BLIP_FLAG_UPDATE_STAGE_DEPTH);
			break;
		}

		case BLIP_CHANGE_DISPLAY:
		{
			SetBlipDisplayValue(pBlip, (eBLIP_DISPLAY_TYPE)iParam);

			if (CPauseMenu::IsActive())
			{
				if ((eBLIP_DISPLAY_TYPE)iParam != GetBlipDisplayValue(pBlip))
				{
					uiDebugf1("Updating pausemap legend because '%s' blip changed display value", GetBlipNameValue(pBlip));
					CPauseMenu::UpdatePauseMapLegend();
				}
			}
			break;
		}

		case BLIP_CHANGE_SHOW_NUMBER:
		{
			SetBlipNumberValue(pBlip, (s8)iParam);
			SetFlag(pBlip, BLIP_FLAG_VALUE_SET_NUMBER);
			SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			break;
		}

		case BLIP_CHANGE_COLOUR:
		{
			if (GetBlipColourValue(pBlip) != (eBLIP_COLOURS)iParam)
			{
				if (iIndex == GetUniqueCentreBlipId())
				{
					uiDebugf1("MiniMap: Centre blip colour changed to BLIP COLOUR=%d", iParam);
				}

				SetBlipColourValue(pBlip, (eBLIP_COLOURS)iParam);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);

				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR);  // incase alpha changes
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR);  // incase alpha changes
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR);  // incase alpha changes
				SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			}

			// if its a ped and we change the colour to BLUE ensure we change the blip name to be friend instead of enemy
			if (GetBlipTypeValue(pBlip) == BLIP_TYPE_CHAR)
			{
				if ((eBLIP_COLOURS)iParam == BLIP_COLOUR_BLUE)
					SetBlipNameValue(pBlip, "BLIP_FRIEND", true);
			}

			break;
		}

		case BLIP_CHANGE_COLOUR_ROUTE:
		{
			// update the colour of the gps route:
			if (IsFlagSet(pBlip,BLIP_FLAG_ROUTE))
			{
				Color32 colour;

				if (iParam != BLIP_COLOUR_USE_COLOUR32)
				{
					colour = CMiniMap_Common::GetColourFromBlipSettings((eBLIP_COLOURS)iParam, IsFlagSet(pBlip,BLIP_FLAG_BRIGHTNESS));
				}
				else
				{
					colour = GetBlipColour32Value(pBlip);  // use secondary colour for route
				}

				CGps::ChangeRadarBlipGpsColour(iIndex, colour);
			}

			break;
		}

		case BLIP_CHANGE_ALPHA:
		{
			if (iParam > 255)
			{
				uiAssertf(0, "Invalid alpha value for blip %d (%d)", iIndex, iActualIndex);

				iParam = 255;  // ensure it caps it at 255
			}

			if (GetBlipAlphaValue(pBlip) != (u8)iParam)
			{
				if (GetBlipAlphaValue(pBlip) == 0)  // if it was 0 then it will need to be recoloured when it gets re-added to the stage
				{
					if (iIndex == GetUniqueCentreBlipId())
					{
						uiDebugf1("MiniMap: Centre blip alpha changed to %d", (u8)iParam);
					}

					SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
				}

				SetBlipAlphaValue(pBlip, (u8)iParam);  // alpha is stored as a u8
				SetFlag(pBlip, BLIP_FLAG_UPDATE_ALPHA_ONLY);

				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR);  // if alpha changes, we need to change alpha in friend indicator
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR);  // if alpha changes, we need to change alpha in friend indicator
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR);  // if alpha changes, we need to change alpha in friend indicator
				SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			}
			break;
		}

		case BLIP_CHANGE_FLASH_DURATION:
		{
			if (GetBlipFlashDuration(pBlip) != (u16)iParam)  // only set the interval if it is different from the currently set flash interval
			{
				SetBlipFlashDuration(pBlip, (u16)iParam);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);
			}
			break;
		}

		case BLIP_CHANGE_FLASH_INTERVAL:
		{
			if (GetBlipFlashInterval(pBlip) != (u16)iParam)  // only set the interval if it is different from the currently set flash interval
			{
				SetBlipFlashInterval(pBlip, (u16)iParam);

				// go through all blips that have been set to flash (inc this one) at the same rate and re-init the flash:
				for (s32 i = 0; i < iMaxCreatedBlips; i++)
				{
					if (i == iSimpleBlip)
						continue;

					CMiniMapBlip *pFlashingBlip = GetBlipFromActualId(i);
					if (!pFlashingBlip)
						continue;

					if (IsFlagSet(pFlashingBlip,BLIP_FLAG_FLASHING))
					{
						UnsetFlag(pFlashingBlip, BLIP_FLAG_FLASHING);  // unset so it resets the flag on the change
						SetFlag(pFlashingBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);  // flag so we know its changed
					}
				}
			}

			break;
		}

		case BLIP_CHANGE_BRIGHTNESS:
		{
			if (IsFlagSet(pBlip, BLIP_FLAG_BRIGHTNESS) != (iParam != 0))
			{
				if (iParam)
					SetFlag(pBlip, BLIP_FLAG_BRIGHTNESS);
				else
					UnsetFlag(pBlip, BLIP_FLAG_BRIGHTNESS);

				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
			}

			break;
		}

		case BLIP_CHANGE_SHOW_HEIGHT:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_SHOW_HEIGHT);
			else
			{
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_HEIGHT);
				UnsetFlag(pBlip, BLIP_FLAG_USE_EXTENDED_HEIGHT_THRESHOLD);
				UnsetFlag(pBlip, BLIP_FLAG_USE_HEIGHT_ON_EDGE);
				SetFlag(pBlip, BLIP_FLAG_VALUE_TURNED_OFF_SHOW_HEIGHT);
			}
			break;
		}

		case BLIP_CHANGE_HIGH_DETAIL:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_HIGH_DETAIL);
			else
				UnsetFlag(pBlip, BLIP_FLAG_HIGH_DETAIL);

			break;
		}

		case BLIP_CHANGE_SHOW_TICK:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_SHOW_TICK);
			else
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_TICK);

			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_TICK);
			SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			break;
		}

		case BLIP_CHANGE_SHOW_TICK_GOLD:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_SHOW_GOLD_TICK);
			else
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_GOLD_TICK);

			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_GOLD_TICK);
			SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			break;
		}

		case BLIP_CHANGE_SHOW_FOR_SALE:
		{
			if(iParam)
				SetFlag(pBlip, BLIP_FLAG_SHOW_FOR_SALE);
			else
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_FOR_SALE);

			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FOR_SALE);
			SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			break;
		}

		case BLIP_CHANGE_SHOW_HEADING_INDICATOR:
		{
			if (iParam)
			{
				SetFlag(pBlip, BLIP_FLAG_SHOW_HEADING_INDICATOR);
			}
			else
			{
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_HEADING_INDICATOR);
				SetBlipDirectionValue(pBlip, DEFAULT_BLIP_ROTATION, false);  // reset back to default (no) rotation (fixes bug 1168334)
			}

			break;
		}

		case BLIP_CHANGE_SHOW_OUTLINE_INDICATOR:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_SHOW_OUTLINE_INDICATOR);
			else
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_OUTLINE_INDICATOR);

			UnsetFlag(pBlip, BLIP_FLAG_SHOW_FRIEND_INDICATOR);
			UnsetFlag(pBlip, BLIP_FLAG_SHOW_CREW_INDICATOR);

			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR);
			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR);
			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR);
			SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);

			break;
		}

		case BLIP_CHANGE_SHOW_FRIEND_INDICATOR:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_SHOW_FRIEND_INDICATOR);
			else
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_FRIEND_INDICATOR);

			UnsetFlag(pBlip, BLIP_FLAG_SHOW_OUTLINE_INDICATOR);

			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR);
			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR);
			SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);

			break;
		}

		case BLIP_CHANGE_SHOW_CREW_INDICATOR:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_SHOW_CREW_INDICATOR);
			else
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_CREW_INDICATOR);

			UnsetFlag(pBlip, BLIP_FLAG_SHOW_OUTLINE_INDICATOR);

			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR);
			SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR);
			SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);

			break;
		}

		case BLIP_CHANGE_SHOW_CONE:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_SHOW_CONE);
			else
			{
				UnsetFlag(pBlip, BLIP_FLAG_SHOW_CONE);
			}
			break;
		}

		case BLIP_CHANGE_DELETE:
		{
			CEntity* pEntity = CMiniMap::FindBlipEntity(pBlip);
			if(pEntity && pEntity->GetIsPhysical())
			{
#if __BANK
				//Displayf("BLIP_CHANGE_DELETE RemoveBlip %s for %s:%x i:%d %d from script.", GetBlipNameValue(pBlip), pEntity->GetModelName(), pEntity, iIndex, iActualIndex);
#endif 
				smart_cast<CPhysical*>(pEntity)->SetCreatedBlip(false);
			}
			RemoveBlip(pBlip);
			break;
		}

		case BLIP_CHANGE_PULSE:
		{
			const BlipLinkage& iCurrentBlipId = GetBlipObjectNameId(pBlip);

			// only BLIP_LEVEL can pulse at the moment (as this is hard coded into the actionscript)
			if (uiVerifyf(iCurrentBlipId == BLIP_LEVEL, "Minimap - PULSE_BLIP called on a blip that is unsupported (%d)", iCurrentBlipId))
			{
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_PULSE);
				SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			}
			break;
		}
		
		case BLIP_CHANGE_MINIMISE_ON_EDGE:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_MINIMISE_ON_EDGE);
			else
				UnsetFlag(pBlip, BLIP_FLAG_MINIMISE_ON_EDGE);
			break;
		}

		case BLIP_CHANGE_MISSION_CREATOR:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_MISSION_CREATOR);
			else
				UnsetFlag(pBlip, BLIP_FLAG_MISSION_CREATOR);
			break;
		}

		case BLIP_CHANGE_FRIENDLY:
		{
			const BlipLinkage& iCurrentBlipId = GetBlipObjectNameId(pBlip);

			if (iCurrentBlipId == BLIP_LEVEL)
			{
				SetBlipColourValue(pBlip, BLIP_COLOUR_FRIENDLY);
				if (iParam)
				{
					SetFlag(pBlip, BLIP_FLAG_BRIGHTNESS);

					if (GetBlipTypeValue(pBlip) == BLIP_TYPE_CHAR)
						SetBlipNameValue(pBlip, "BLIP_FRIEND", true);
				}
				else
				{
					UnsetFlag(pBlip, BLIP_FLAG_BRIGHTNESS);

					if (GetBlipTypeValue(pBlip) == BLIP_TYPE_CHAR)
						SetBlipNameValue(pBlip, "BLIP_ENEMY", true);
				}

				// update the colour of the gps route:
				if (IsFlagSet(pBlip, BLIP_FLAG_ROUTE))
				{
					Color32 colour;

					if (GetBlipColourValue(pBlip) != BLIP_COLOUR_USE_COLOUR32)
					{
						eBLIP_COLOURS blipColour = GetBlipColourValue(pBlip);

						if (blipColour == BLIP_COLOUR_DEFAULT)  // any "default" coloured sprite blips always have a yellow gps route
						{
							blipColour = BLIP_COLOUR_YELLOW;
						}

						colour = CMiniMap_Common::GetColourFromBlipSettings(blipColour, IsFlagSet(pBlip,BLIP_FLAG_BRIGHTNESS));
					}
					else
					{
						colour = GetBlipColour32Value(pBlip);  // use secondary colour for route
					}

					CGps::ChangeRadarBlipGpsColour(iIndex, colour);
				}

				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
			}
			break;
		}

		case BLIP_CHANGE_RADIUS_EDGE:
		{
			if (uiVerifyf(GetBlipTypeValue(pBlip) == BLIP_TYPE_RADIUS, "Blip %d (%d) is not a radius blip!", iIndex, iActualIndex))
			{
				if (iParam)
					SetBlipObjectName(pBlip, BLIP_RADIUS_OUTLINE);
				else
					SetBlipObjectName(pBlip, BLIP_RADIUS);

				SetFlag(pBlip, BLIP_FLAG_VALUE_REINIT_STAGE);  // as the sprite itself changes, it needs to be removed and re-added
				SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			}
			break;
		}
		case BLIP_CHANGE_AREA_EDGE:
		{
			if (uiVerifyf(GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA, "Blip %d (%d) is not an area blip!", iIndex, iActualIndex))
			{
				if (iParam)
					SetBlipObjectName(pBlip, BLIP_AREA_OUTLINE);
				else
					SetBlipObjectName(pBlip, BLIP_AREA);

				SetFlag(pBlip, BLIP_FLAG_VALUE_REINIT_STAGE);  // as the sprite itself changes, it needs to be removed and re-added
				SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			}

			break;
		}
		case BLIP_CHANGE_ROUTE:
		{
			//
			// only 1 route at any one time at the moment:
			//
			if (iParam)  // clear all previous route blips
			{
				ClearAllBlipRoutes();
			}

			// set this blip to use a route
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_ROUTE);
			else
				UnsetFlag(pBlip, BLIP_FLAG_ROUTE);

			if (IsFlagSet(pBlip,BLIP_FLAG_ROUTE))
			{
				Color32 colour;

				if (GetBlipColourValue(pBlip) != BLIP_COLOUR_USE_COLOUR32)
				{
					eBLIP_COLOURS blipColour = GetBlipColourValue(pBlip);

					if (blipColour == BLIP_COLOUR_DEFAULT)  // any "default" coloured sprite blips always have a yellow gps route
					{
						blipColour = BLIP_COLOUR_YELLOW;
					}

					colour = CMiniMap_Common::GetColourFromBlipSettings(blipColour, IsFlagSet(pBlip,BLIP_FLAG_BRIGHTNESS));
				}
				else
				{
					colour = GetBlipColour32Value(pBlip);  // use secondary colour for route
				}

				Vector3 vBlipPos(GetBlipPositionValue(pBlip));

				CGps::StartRadarBlipGps(vBlipPos, FindBlipEntity(pBlip), iIndex, colour);
			}
			else
			{
				CGps::StopRadarBlipGps(GetUniqueBlipUsed(pBlip));
			}
			break;
		}

		case BLIP_CHANGE_SHORTRANGE:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_SHORTRANGE);
			else
				UnsetFlag(pBlip, BLIP_FLAG_SHORTRANGE);

			break;
		}

		case BLIP_CHANGE_HIDDEN_ON_LEGEND:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND);
			else
				UnsetFlag(pBlip, BLIP_FLAG_HIDDEN_ON_LEGEND);
			break;
		}

		case BLIP_CHANGE_MARKER_LONG_DISTANCE:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_MARKER_LONG_DIST);
			else
				UnsetFlag(pBlip, BLIP_FLAG_MARKER_LONG_DIST);

			break;
			
		}

		case BLIP_CHANGE_FLASH:
		{
			if (IsFlagSet(pBlip, BLIP_FLAG_FLASHING) != (iParam != 0))
			{
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);
				
				SetBlipFlashInterval(pBlip, DEFAULT_BLIP_FLASH_INTERVAL);
				SetBlipFlashDuration(pBlip, MAX_UINT16);
			}
			
			break;
		}

		case BLIP_CHANGE_FLASH_ALT:
		{
			if (IsFlagSet(pBlip, BLIP_FLAG_FLASHING) != (iParam != 0))
			{
				SetBlipFlashInterval(pBlip, DEFAULT_BLIP_FLASH_INTERVAL * 2);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);

				SetBlipFlashInterval(pBlip, DEFAULT_BLIP_FLASH_INTERVAL);
				SetBlipFlashDuration(pBlip, MAX_UINT16);
			}

			break;
		}

		case BLIP_CHANGE_OBJECT_NAME:
		case BLIP_CHANGE_SPRITE:  // need to support this until old radar is removed
		{
			const char* pBlipSpriteName = CMiniMap_Common::GetBlipName(iParam);
			if (pBlipSpriteName)
			{
				// some blips have height indication as default:
				if ( (iParam == BLIP_LEVEL) || (iParam >= RADAR_TRACE_NUMBERED_1 && iParam <= RADAR_TRACE_NUMBERED_5) )
				{
					SetFlag(pBlip, BLIP_FLAG_SHOW_HEIGHT);
				}
				else
				{
					UnsetFlag(pBlip, BLIP_FLAG_SHOW_HEIGHT);
				}

				SetBlipObjectName(pBlip, iParam);

				// set the blip name based on the object id:
				char cBlipTextLabel[TEXT_KEY_SIZE];
				formatf(cBlipTextLabel, "BLIP_%d", iParam, NELEM(cBlipTextLabel));
				SetBlipNameValue(pBlip, cBlipTextLabel, true);

				SetBlipColourValue(pBlip, BLIP_COLOUR_DEFAULT);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
				SetFlag(pBlip, BLIP_FLAG_VALUE_REINIT_STAGE);  // as the sprite itself changes, it needs to be removed and re-added
				SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);

				if (CPauseMenu::IsActive())
				{
					uiDebugf1("Updating pausemap legend because '%s' blip changed sprite", GetBlipNameValue(pBlip));
					CPauseMenu::UpdatePauseMapLegend();
				}
			}
			else
			{
				uiAssertf(0, "MiniMap: Couldn't change blip object name to %d as blip %d (%d) doesnt exist", iParam, iIndex, iActualIndex);
			}
			break;
		}

		case BLIP_CHANGE_EXTENDED_HEIGHT_THRESHOLD :
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_USE_EXTENDED_HEIGHT_THRESHOLD);
			else
				UnsetFlag(pBlip, BLIP_FLAG_USE_EXTENDED_HEIGHT_THRESHOLD);
			break;
		}

		case BLIP_CHANGE_SHORT_HEIGHT_THRESHOLD :
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_USE_SHORT_HEIGHT_THRESHOLD);
			else
				UnsetFlag(pBlip, BLIP_FLAG_USE_SHORT_HEIGHT_THRESHOLD);
			break;
		}

		case BLIP_CHANGE_HEIGHT_ON_EDGE :
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_USE_HEIGHT_ON_EDGE);
			else
				UnsetFlag(pBlip, BLIP_FLAG_USE_HEIGHT_ON_EDGE);
			break;
		}

		case BLIP_CHANGE_HOVERED_ON_PAUSEMAP:
		{
			if (iParam)
				SetFlag(pBlip, BLIP_FLAG_HOVERED_ON_PAUSEMAP);
			else
				UnsetFlag(pBlip, BLIP_FLAG_HOVERED_ON_PAUSEMAP);

			SetFlag(pBlip, BLIP_FLAG_UPDATE_STAGE_DEPTH);
			break;
		}

		default:
		{
			uiAssertf(0, "MiniMap: Trying to update blip %d (%d) with invalid task (%d) - Int %d", iIndex, iActualIndex, iTodo, iParam);
		}
	}
}

void CMiniMap::ClearAllBlipRoutes()
{
	for (s32 i = 0; i < iMaxCreatedBlips; i++)
	{
		if (i == iSimpleBlip)
			continue;

		CMiniMapBlip *pRouteBlip = blip[i];
		if (!pRouteBlip)
			continue;

		if (IsFlagSet(pRouteBlip,BLIP_FLAG_ROUTE))
		{
			CGps::StopRadarBlipGps(GetUniqueBlipUsed(pRouteBlip));  // remove identifier to route in gps code
			UnsetFlag(pRouteBlip, BLIP_FLAG_ROUTE);  // remove route
		}
	}
}

void CMiniMap::SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, Color32 iParam)
{
	s32 iActualIndex = ConvertUniqueBlipToActualBlip(iIndex);

	if (iActualIndex >= MAX_NUM_BLIPS)
	{
		uiAssertf(0, "MiniMap: Attempted to update blip %d (%d) over the max array (%d)!", iIndex, iActualIndex, MAX_NUM_BLIPS);
		return;
	}

	if (iActualIndex < 0)
		return;

	CMiniMapBlip *pBlip = GetBlipFromActualId(iActualIndex);

	if (!pBlip)
		return;

	if (!pBlip->IsComplex())
	{
		uiAssertf(0, "MiniMap: Attempted to modify a Simple Blip %d (%d)", iIndex, iActualIndex);
	}

	switch (iTodo)
	{
		case BLIP_CHANGE_COLOUR32:
		{
			if (GetBlipColour32Value(pBlip) != iParam)
			{
				SetBlipSecondaryColourValue(pBlip, iParam);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);

				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR);
				SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR);
				SetFlag(pBlip, BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS);
			}

			break;
		}

		case BLIP_CHANGE_COLOUR_ROUTE:
		{
			// update the colour of the gps route:
			if (IsFlagSet(pBlip,BLIP_FLAG_ROUTE))
			{
				CGps::ChangeRadarBlipGpsColour(iIndex, iParam);
			}

			break;
		}

		default:
		{
			uiAssertf(0, "MiniMap: Trying to update blip %d (%d) with invalid task (%d) - Colour %d", iIndex, iActualIndex, iTodo, iParam.GetColor());
		}
	}
}

void CMiniMap::SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, float fParam)
{
	s32 iActualIndex = ConvertUniqueBlipToActualBlip(iIndex);

	if (iActualIndex < 0)
		return;

	CMiniMapBlip *pBlip = GetBlipFromActualId(iActualIndex);

	if (!pBlip)
		return;

	if (!pBlip->IsComplex())
	{
		uiAssertf(0, "MiniMap: Attempted to modify a Simple Blip %d (%d)", iIndex, iActualIndex);
	}

	switch (iTodo)
	{
		case BLIP_CHANGE_SCALE:
		{
			SetBlipScaleValue(pBlip, fParam);
			break;
		}
		case BLIP_CHANGE_DIRECTION:
		{
			SetBlipDirectionValue(pBlip, fParam);
			break;
		}

		default:
		{
			uiAssertf(0, "MiniMap: Trying to update blip %d (%d) with invalid task (%d) - Float %0.2f", iIndex, iActualIndex, iTodo, fParam);
		}
	}
}

void CMiniMap::SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, const Vector3& vParam)
{
	s32 iActualIndex = ConvertUniqueBlipToActualBlip(iIndex);

	uiAssertf(iActualIndex >= 0, "SetBlipParameter - blip %d (%d) doesn't exist", iIndex, iActualIndex);

	if (iActualIndex < 0)
		return;

	CMiniMapBlip *pBlip = GetBlipFromActualId(iActualIndex);

	if (!pBlip)
		return;

	if (!pBlip->IsComplex())
	{
		uiAssertf(0, "MiniMap: Attempted to modify a Simple Blip %d (%d)", iIndex, iActualIndex);
	}

	switch (iTodo)
	{
	case BLIP_CHANGE_SCALE:
		{
			Vector2 vec2;
			vParam.GetVector2XY(vec2);
			SetBlipScaleValue(pBlip, vec2);
			break;
		}
		case BLIP_CHANGE_POSITION:
		{
			SetBlipPositionValue(pBlip, vParam);
			break;
		}

		default:
		{
			uiAssertf(0, "MiniMap: Trying to update blip %d (%d) with invalid task (%d) - Vector %0.2f,%0.2f,%0.2f", iIndex, iActualIndex, iTodo, vParam.x, vParam.y, vParam.z);
		}
	}
}

void CMiniMap::SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, const char *cParam)
{
	s32 iActualIndex = ConvertUniqueBlipToActualBlip(iIndex);

	if (iActualIndex < 0)
		return;

	CMiniMapBlip *pBlip = GetBlipFromActualId(iActualIndex);

	if (!pBlip)
		return;

	if (!pBlip->IsComplex())
	{
		uiAssertf(0, "MiniMap: Attempted to modify a Simple Blip %d (%d)", iIndex, iActualIndex);
	}

	switch (iTodo)
	{
	case BLIP_CHANGE_NAME:
		{
			if ( static_cast<CBlipComplex*>(pBlip)->cLocName != cParam )
			{
				SetBlipNameValue(pBlip, cParam, true);

				if (CPauseMenu::IsActive())
				{
					uiDebugf1("Updating pausemap legend because '%s' blip changed name", GetBlipNameValue(pBlip));
					CPauseMenu::UpdatePauseMapLegend();
				}
			}
			else
			{
				uiDebugf1("Blip %d (%s, %d) set with name '%s' when it was already set with it (BLIP_CHANGE_NAME)", GetUniqueBlipUsed(pBlip), GetBlipObjectName(pBlip), GetBlipObjectNameId(pBlip), GetBlipNameValue(pBlip));
			}

			break;
		}

		case BLIP_CHANGE_NAME_FROM_ASCII:
		{
			if ( static_cast<CBlipComplex*>(pBlip)->cLocName != cParam )
			{
				SetBlipNameValue(pBlip, cParam, false);

				if (CPauseMenu::IsActive())
				{
					uiDebugf1("Updating pausemap legend because '%s' blip changed name", GetBlipNameValue(pBlip));
					CPauseMenu::UpdatePauseMapLegend();
				}
			}
			else
			{
				uiDebugf1("Blip %d (%s, %d) set with name '%s' when it was already set with it (BLIP_CHANGE_NAME_FROM_ASCII)", GetUniqueBlipUsed(pBlip), GetBlipObjectName(pBlip), GetBlipObjectNameId(pBlip), GetBlipNameValue(pBlip));
			}

			break;
		}

		default:
		{
			uiAssertf(0, "MiniMap: Trying to update blip %d (%d) with invalid task (%d) with String %s", iIndex, iActualIndex, iTodo, cParam);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetBlipNumberValue
// PURPOSE:
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetBlipNumberValue(CMiniMapBlip *pBlip, s8 iParam)
{
	if (pBlip && pBlip->IsComplex())
		((CBlipComplex*)pBlip)->iNumberToDisplay = iParam;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetBlipDisplayValue
// PURPOSE:
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetBlipDisplayValue(CMiniMapBlip *pBlip, eBLIP_DISPLAY_TYPE iParam)
{
	if (pBlip && pBlip->IsComplex())
		((CBlipComplex*)pBlip)->display = (u8)iParam;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetActualSimpleBlipId
// PURPOSE:	returns the id used for the simple blip
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::GetActualSimpleBlipId()
{
	return iSimpleBlip;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::IsBlipIdInUse
// PURPOSE:	returns whether this blip is in use
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::IsBlipIdInUse(s32 iUniqueIndex)
{
	if (iUniqueIndex == INVALID_BLIP_ID)
	{
		return false;
	}

	CMiniMapBlip *pBlip = GetBlip(iUniqueIndex);
	
	if (pBlip)
	{
		return ((GetUniqueBlipUsed(pBlip) != INVALID_BLIP_ID) &&
				(!IsFlagSet(pBlip, BLIP_FLAG_VALUE_REMOVE_FROM_STAGE)) &&
				(!IsFlagSet(pBlip, BLIP_FLAG_VALUE_DESTROY_BLIP_OBJECT)));
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::IsBlipOnStage
// PURPOSE:	returns whether this blip is currently on the stage
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::IsBlipOnStage(CMiniMapBlip *pBlip)
{
	if (pBlip)
	{
//		if (GetBlipGfxValue(pBlip))
		if (IsFlagSet(pBlip, BLIP_FLAG_ON_STAGE))
		{
			return true;
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetVisible
// PURPOSE: sets the minimap to be visible or hidden
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetVisible(bool bSetVisible)
{
	bVisible = bSetVisible;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlipFromActualId
// PURPOSE: returns the base blip from the actual id
/////////////////////////////////////////////////////////////////////////////////////
CMiniMapBlip *CMiniMap::GetBlipFromActualId(s32 iId)
{
	if (iId >= 0 && iId < MAX_NUM_BLIPS)
	{
		return (blip[iId]);
	}

	uiAssertf(0, "CMiniMap: Cannot get Blip from actual blip id %d", iId);

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetBlip
// PURPOSE: returns the base blip from the unique id
//			this function needs to fail silently
/////////////////////////////////////////////////////////////////////////////////////
CMiniMapBlip *CMiniMap::GetBlip(s32 iUniqueBlipId)
{
	s32 iActualBlipId = ConvertUniqueBlipToActualBlip(iUniqueBlipId);

	if (iActualBlipId >= 0 && iActualBlipId < MAX_NUM_BLIPS)  // this function needs to fail silently
	{
		return (GetBlipFromActualId(iActualBlipId));
	}

	return NULL;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::RemoveBlipAttachedToEntity()
// PURPOSE:	removes any blips that are attached to the passed entity index
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::RemoveBlipAttachedToEntity(const CEntity *pEntity, eBLIP_TYPE blipType)
{
	CEntityPoolIndexForBlip entityIndex(pEntity, blipType);

	for (s32 iIndex = 0; iIndex < iMaxCreatedBlips; iIndex++)
	{
		if (iIndex == iSimpleBlip)
			continue;

		CMiniMapBlip *pBlip = GetBlipFromActualId(iIndex);

		if (!pBlip)
			continue;

		if ( (GetBlipTypeValue(pBlip) == blipType) && (GetBlipPoolIndexValue(pBlip) == entityIndex) )
		{
			s32 iUniqueBlipId = GetUniqueBlipUsed(pBlip);

			RemoveBlip(pBlip);

			// remove any script resources which reference this blip
			CTheScripts::GetScriptHandlerMgr().RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_RADAR_BLIP, iUniqueBlipId);

			return;  // if we removed one we can now return out (dont carry on through the array checking - fixes bug 649817)
		}
	}
}


CMiniMapBlip *CMiniMap::GetBlipAttachedToEntity(const CEntity *pEntity, eBLIP_TYPE blipType, u32 FlagToCheck, bool bInUseOnly)
{
	bool bCheckForRelationshipGroup = false;
	if ((FlagToCheck & BLIP_FLAG_CREATED_FOR_RELATIONSHIP_GROUP_PED) != 0)
	{
		bCheckForRelationshipGroup = true;
	}

	bool bCheckFlags = bInUseOnly || bCheckForRelationshipGroup;

	CEntityPoolIndexForBlip entityIndex(pEntity, blipType);

	for (s32 iIndex = 0; iIndex < iMaxCreatedBlips; iIndex++)
	{
		if (iIndex == iSimpleBlip)
			continue;

		CMiniMapBlip *pBlip = blip[iIndex];

		if (!pBlip)
			continue;

		if (!bCheckFlags
			|| (bCheckForRelationshipGroup && IsFlagSet(pBlip, BLIP_FLAG_CREATED_FOR_RELATIONSHIP_GROUP_PED)) 
			|| (bInUseOnly && IsBlipIdInUse(pBlip->m_iUniqueId)))
		{
			if ( (GetBlipTypeValue(pBlip) == blipType) && (GetBlipPoolIndexValue(pBlip) == entityIndex) )
			{
				return pBlip;
			}
		}
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateAndAutoSwitchOffFlash()
// PURPOSE:	updates and decides whether to switch off a flash on a blip
//			based on the set duration
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateAndAutoSwitchOffFlash(CMiniMapBlip *pBlip)
{
	//
	// Check whether we need to auto-switch off flashing:
	//

	if (uiVerifyf(pBlip, "Blip doesnt exist!"))
	{
		if (IsFlagSet(pBlip,BLIP_FLAG_FLASHING))
		{
			u16 iFlashValue = GetBlipFlashDuration(pBlip);

			if (iFlashValue != MAX_UINT16)  // dont switch off the flash on blip value MAX_UINT8 ever
			{
				s32 iNewFlashValue = iFlashValue - fwTimer::GetTimeStepInMilliseconds();

				if (iNewFlashValue > 0) // decrement the timer
				{
					SetBlipFlashDuration(pBlip, u16(iNewFlashValue));
				}

				if (iFlashValue == 0 || iNewFlashValue <= 0)
				{
					// switch off the flash completely
					SetBlipFlashDuration(pBlip, MAX_UINT16);
					SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);
				}
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ToggleComponent()
// PURPOSE:	turns each minimap component on/off
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::ToggleComponent(s32 const c_componentId, bool const c_visible, bool const c_immediate, eHUD_COLOURS const c_hudColor)
{
	if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_BACKGROUND), SF_BASE_CLASS_MINIMAP, "TOGGLE_COMPONENT"))
	{
		CScaleformMgr::AddParamInt(c_componentId);
		CScaleformMgr::AddParamBool(c_visible);

		if (c_hudColor != HUD_COLOUR_INVALID)
		{
			CScaleformMgr::AddParamInt((s32)c_hudColor);
		}

		CScaleformMgr::EndMethod(c_immediate);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ToggleTerritory()
// PURPOSE:	sets up each minimap territory
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::ToggleTerritory(s32 iPropertyId, s32 iOwner1, s32 iOwner2, s32 iOwner3, s32 iOwner4)
{
	if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_MP_PROPERTY_OWNER"))
	{
		CScaleformMgr::AddParamInt(iPropertyId);
		CScaleformMgr::AddParamInt(iOwner1);

		if (iOwner2 != -1)
		{
			CScaleformMgr::AddParamInt(iOwner2);
		}

		if (iOwner3 != -1)
		{
			CScaleformMgr::AddParamInt(iOwner3);
		}

		if (iOwner4 != -1)
		{
			CScaleformMgr::AddParamInt(iOwner4);
		}

		CScaleformMgr::EndMethod();
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ShowYoke()
// PURPOSE:	calls "show_yoke" on the foreground movie with the passed params
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::ShowYoke(const bool bVisible, const float fPosX, const float fPosY, const s32 iAlpha)
{
	// Return early if the yoke is continuing to be invisible. If we return early when the yoke is visible, its position won't be updated.
	if (!ms_bShowingYoke && !bVisible)
	{
		return;
	}

	CMiniMap::ms_bShowingYoke = bVisible;
	if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SHOW_YOKE"))
	{
		CScaleformMgr::AddParamFloat(fPosX);
		CScaleformMgr::AddParamFloat(fPosY);
		CScaleformMgr::AddParamBool(bVisible);
		CScaleformMgr::AddParamInt(iAlpha);

		CScaleformMgr::EndMethod();
	}
}

void CMiniMap::ShowSonarSweep(const bool bVisible)
{
	if(!bVisible)
	{
		if (CScaleformMgr::BeginMethod(GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SHOW_SONAR_SWEEP"))
		{
			CScaleformMgr::AddParamBool(bVisible);
			CScaleformMgr::EndMethod();
		}
	}

	ms_bShowSonarSweep = bVisible;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetCurrentMiniMap
// PURPOSE: returns the enum for the current minimap
/////////////////////////////////////////////////////////////////////////////////////
eMINIMAP_VALUES CMiniMap::GetCurrentMiniMap()
{
	//
	// reworked this function to fix 1797848.   Before we could have 1 state pending to be set up but the values used would be the current state, causing a glitch
	//
	eMINIMAP_VALUES returnValue = MINIMAP_VALUE_MINIMAP;

#if !__FINAL
	static eMINIMAP_VALUES prevMiniMapValue = returnValue;
#endif

	switch (MinimapModeState)  // if we are currently set to move to another state, ensure we use that value
	{
		case MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP:
		case MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD:
		{
			returnValue = MINIMAP_VALUE_MINIMAP;
			break;
		}

		case MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP:
		case MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD:
		{
			returnValue = MINIMAP_VALUE_GOLF_COURSE;
			break;
		}

		case MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP:
		case MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD:
		{
			returnValue = MINIMAP_VALUE_PAUSEMAP;
			break;
		}

		case MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP:
		case MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD:
		{
			if (!ms_bBigMapFullScreen)
			{
				returnValue = MINIMAP_VALUE_BIGMAP;
			}
			else
			{
				returnValue = MINIMAP_VALUE_PAUSEMAP;
			}
			break;
		}

		case MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP:
		case MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD:
		{
			returnValue = MINIMAP_VALUE_CUSTOM;
			break;
		}
		
		// otherwise use the standard values:
		default:
		{
			if (IsInPauseMap())
			{
				returnValue = MINIMAP_VALUE_PAUSEMAP;
			}
			else
			{
				if (IsInBigMap())
				{
					if (!ms_bBigMapFullScreen)
					{
						returnValue = MINIMAP_VALUE_BIGMAP;
					}
					else
					{
						returnValue = MINIMAP_VALUE_PAUSEMAP;
					}
				}
				else if (IsInCustomMap())
				{
					returnValue = MINIMAP_VALUE_CUSTOM;
				}
				else
				{
					if (IsInGolfMap())
					{
						returnValue = MINIMAP_VALUE_GOLF_COURSE;
					}
					else
					{
						returnValue = MINIMAP_VALUE_MINIMAP;
					}
				}
			}
		}
	}



#if !__FINAL
	if (prevMiniMapValue != returnValue)
	{
		if(CMiniMap_Common::OutputDebugTransitions())
		{
			uiDisplayf("MINIMAP_TRANSITION: CMiniMap::GetCurrentMiniMap changed from %d to %d (UT)", (s32)prevMiniMapValue, (s32)returnValue);
		}
		
		prevMiniMapValue = returnValue;
	}
#endif  // !__FINAL

	return returnValue;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetCurrentMiniMapFilename
// PURPOSE: returns the current minimap filename
/////////////////////////////////////////////////////////////////////////////////////
char *CMiniMap::GetCurrentMiniMapFilename()
{
	if (IsInPauseMap())
	{
		return &sm_miniMaps[MINIMAP_VALUE_MINIMAP].m_fileName[0];  // uses MINIMAP_VALUE_STANDARD for pausemap as it shares the same map
	}
	else
	{
		if (IsInBigMap())
		{
			return &sm_miniMaps[MINIMAP_VALUE_MINIMAP].m_fileName[0];  // uses MINIMAP_VALUE_STANDARD for bigmap as it shares the same map
		}
		else
		{
			// never return golf course, for it will mess up everything if we attempt to load
			//if (IsInGolfMap())
			//{
			//	return &sm_miniMaps[MINIMAP_VALUE_GOLF_COURSE].m_fileName[0];
			//}

			//else
			{
				return &sm_miniMaps[MINIMAP_VALUE_MINIMAP].m_fileName[0];
			}
		}
	}
}

eMINIMAP_VALUES GetMaskValue(eMINIMAP_VALUES miniMap)
{
	if( miniMap == MINIMAP_VALUE_PAUSEMAP)
	{
		miniMap = MINIMAP_VALUE_PAUSEMAP_MASK;
	}
	else if (miniMap == MINIMAP_VALUE_BIGMAP)
	{
		miniMap = MINIMAP_VALUE_BIGMAP_MASK;
	}
	else if( miniMap == MINIMAP_VALUE_MINIMAP )
	{
		miniMap = MINIMAP_VALUE_MINIMAP_MASK;
	}
	else if( miniMap == MINIMAP_VALUE_CUSTOM )
	{
		miniMap = MINIMAP_VALUE_CUSTOM_MASK;
	}

	return miniMap;
}

eMINIMAP_VALUES GetBlurValue(eMINIMAP_VALUES miniMap)
{
	if (miniMap == MINIMAP_VALUE_BIGMAP)
	{
		miniMap = MINIMAP_VALUE_BIGMAP_BLUR;
	}
	else
	{
		miniMap = MINIMAP_VALUE_MINIMAP_BLUR;
	}

	return miniMap;
}


Vector2 CMiniMap::GetCurrentMiniMapMaskPosition()
{
	eMINIMAP_VALUES miniMap = GetMaskValue(GetCurrentMiniMap());
	Vector2 vPos(sm_miniMaps[miniMap].m_position);
	Vector2 vSize(sm_miniMaps[miniMap].m_size);

#if RSG_PC
	if(miniMap != MINIMAP_VALUE_PAUSEMAP_MASK)
	{
		// we need to adjust the map twice to compensate for the extra divisions that happen on the render thread, which work for the pause map
		// here we're compensating for rather rubbish tuning values that we don't (?) want to adjust for the consoles...?
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(sm_miniMaps[miniMap].m_alignX), nullptr, &vSize);
	}

	CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(sm_miniMaps[miniMap].m_alignX), &vPos, &vSize);
#endif

	return CHudTools::CalculateHudPosition(vPos, vSize, sm_miniMaps[miniMap].m_alignX, sm_miniMaps[miniMap].m_alignY);
}


Vector2 CMiniMap::GetCurrentMiniMapMaskSize()
{
	eMINIMAP_VALUES miniMap = GetMaskValue(GetCurrentMiniMap());
	Vector2 vSize(sm_miniMaps[miniMap].m_size);

#if RSG_PC
	if(miniMap != MINIMAP_VALUE_PAUSEMAP_MASK)
	{
		// we need to adjust the map twice to compensate for the extra divisions that happen on the render thread, which work for the pause map
		// here we're compensating for rather rubbish tuning values that we don't (?) want to adjust for the consoles...?
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(sm_miniMaps[miniMap].m_alignX), nullptr, &vSize);
	}

	CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(sm_miniMaps[miniMap].m_alignX), nullptr, &vSize);
#endif

	return vSize;
}


Vector2 CMiniMap::GetCurrentMiniMapBlurPosition()
{
	eMINIMAP_VALUES miniMap = GetBlurValue( GetCurrentMiniMap() );
	Vector2 vPos(sm_miniMaps[miniMap].m_position);
	Vector2 vSize(sm_miniMaps[miniMap].m_size);

#if RSG_PC
	CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(sm_miniMaps[miniMap].m_alignX), &vPos, &vSize);
#endif

	return CHudTools::CalculateHudPosition(vPos, vSize, sm_miniMaps[miniMap].m_alignX, sm_miniMaps[miniMap].m_alignY);
}


Vector2 CMiniMap::GetCurrentMiniMapBlurSize()
{
	eMINIMAP_VALUES miniMap = GetBlurValue( GetCurrentMiniMap() );
	Vector2 vSize(sm_miniMaps[miniMap].m_size);


#if RSG_PC
	CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(sm_miniMaps[miniMap].m_alignX), nullptr, &vSize);
#endif

	return vSize;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetCurrentMiniMapPosition
// PURPOSE: returns the current minimap position
/////////////////////////////////////////////////////////////////////////////////////
//	Is it safe for this to be called on the RenderThread?
Vector2 CMiniMap::GetCurrentMiniMapPosition()
{
	eMINIMAP_VALUES miniMap = GetCurrentMiniMap();
	Vector2 vPos(sm_miniMaps[miniMap].m_position);
	Vector2 vSize(sm_miniMaps[miniMap].m_size);

#if RSG_PC
	if( miniMap!= MINIMAP_VALUE_PAUSEMAP )
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(sm_miniMaps[miniMap].m_alignX), &vPos, &vSize);
#endif

	return CHudTools::CalculateHudPosition(vPos, vSize, sm_miniMaps[miniMap].m_alignX, sm_miniMaps[miniMap].m_alignY);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetCurrentMiniMapSize
// PURPOSE: returns the current minimap size
/////////////////////////////////////////////////////////////////////////////////////
//	Is it safe for this to be called on the RenderThread?
Vector2 CMiniMap::GetCurrentMiniMapSize()
{
	eMINIMAP_VALUES miniMap = GetCurrentMiniMap();
	Vector2 vSize(sm_miniMaps[miniMap].m_size);

#if RSG_PC
	if( miniMap!= MINIMAP_VALUE_PAUSEMAP )
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(sm_miniMaps[miniMap].m_alignX), nullptr, &vSize);
#endif

	return vSize;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetInPrologue
// PURPOSE: Sets if we should display the prologue map, and clears out any custom
//			waypoints if we change maps.
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetInPrologue(bool bValue)
{
	if(ms_bInPrologue != bValue)
	{
		uiDisplayf("CMiniMap::SetInPrologue - Prologue Clear");
		SwitchOffWaypoint();

		if (ms_iNorthBlip != INVALID_BLIP_ID)
		{
			float fNorthPos = CMiniMap::sm_Tunables.Tiles.vMiniMapWorldSize.y*2.0f;
			if (bValue)
			{
				fNorthPos = -fNorthPos;  // invert the "north" in prologue - todo 928263
			}

			Vector3 vNorthPos(0.0f,fNorthPos, 0.0f);
			SetBlipParameter(BLIP_CHANGE_POSITION, ms_iNorthBlip, vNorthPos);
		}

		ms_bInPrologue = bValue;
	}
}

void CMiniMap::SetOnIslandMap(bool bValue)
{
	if(ms_bOnIslandMap != bValue)
	{
		uiDisplayf("CMiniMap::SetOnIslandMap - Island X Map Change.");
		SwitchOffWaypoint();

		// TODO_UI: North blip hack goes here too?

		ms_bOnIslandMap = bValue;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::FlashMiniMapDisplay
// PURPOSE: a flash for the radar when stuff appears e.g. random events
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::FlashMiniMapDisplay(eHUD_COLOURS hudColour)
{
	ms_eFlashColour = hudColour;
	ms_uFlashStartTime = fwTimer::GetTimeInMilliseconds();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::CreateWaypointBlipAndRoute
// PURPOSE: creates a blip and gps route for the given waypoint
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::CreateWaypointBlipAndRoute(sWaypointStruct& Waypoint, eBLIP_DISPLAY_TYPE iDisplayType, eHUD_COLOURS iHudColor)
{
	ClearWaypointBlipAndRoute(Waypoint);

	CEntity* pEntity = nullptr;
	Vector3 vTarget(Waypoint.vPos.x, Waypoint.vPos.y, WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, Waypoint.vPos.x, Waypoint.vPos.y));

	//Entity based waypoint

	if (ms_bEntityWaypointAllowed && NetworkInterface::IsGameInProgress())
	{
		if (netObject* pObj = NetworkInterface::GetObjectManager().GetNetworkObject(Waypoint.ObjectId))
		{
			pEntity = pObj->GetEntity();
			eBLIP_TYPE iBlipType = CEntityPoolIndexForBlip::GetBlipTypeForEntity(pEntity);
			CEntityPoolIndexForBlip PoolIndex(pEntity, iBlipType);
			Waypoint.iBlipIndex = CreateBlip(true, iBlipType, PoolIndex, iDisplayType, "waypoint");
		}
	}
	//Fall back to coordinate waypoint
	if(Waypoint.iBlipIndex == INVALID_BLIP_ID)
	{
		Waypoint.iBlipIndex = CreateBlip(true, BLIP_TYPE_COORDS, vTarget, iDisplayType, "waypoint");
	}

	CGpsSlot& GpsSlot = CGps::GetSlot(GPS_SLOT_WAYPOINT);
	GpsSlot.m_iGpsFlags = ms_waypointGpsFlags;
	if (pEntity == nullptr) //Ignore Z for coordinate only waypoints
	{
		GpsSlot.m_iGpsFlags |= GPS_FLAG_IGNORE_DESTINATION_Z;
	}
	GpsSlot.Start(vTarget, pEntity, Waypoint.iBlipIndex, true, GPS_SLOT_WAYPOINT, CHudColour::GetRGB(iHudColor, 255));

	if (Waypoint.iBlipIndex != INVALID_BLIP_ID)
	{
		SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, Waypoint.iBlipIndex, BLIP_WAYPOINT);
		SetBlipParameter(BLIP_CHANGE_PRIORITY, Waypoint.iBlipIndex, BLIP_PRIORITY_HIGHEST_SPECIAL);
		SetBlipParameter(BLIP_CHANGE_COLOUR, Waypoint.iBlipIndex, BLIP_COLOUR_USE_COLOUR32);
		SetBlipParameter(BLIP_CHANGE_COLOUR32, Waypoint.iBlipIndex, CHudColour::GetRGBA(iHudColor));
		SetBlipParameter(BLIP_CHANGE_CATEGORY, Waypoint.iBlipIndex, BLIP_CATEGORY_WAYPOINT);
	}
}

void CMiniMap::ClearWaypointBlipAndRoute(sWaypointStruct& Waypoint)
{
	if (Waypoint.iBlipIndex != INVALID_BLIP_ID)
	{
		if (IsBlipIdInUse(Waypoint.iBlipIndex))
		{
			if (CMiniMapBlip *pBlip = GetBlip(Waypoint.iBlipIndex))
			{
				RemoveBlip(pBlip);
			}
		}
		Waypoint.iBlipIndex = INVALID_BLIP_ID;
		Waypoint.ObjectId = NETWORK_INVALID_OBJECT_ID;
		Waypoint.iAssociatedBlipId = INVALID_BLIP_ID;
		CGps::m_Slots[GPS_SLOT_WAYPOINT].Clear(GPS_SLOT_WAYPOINT, true);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetInBigMap
// PURPOSE: sets up any params when we enter "big map" mode
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetInBigMap(bool bBigMap)
{
#if !__FINAL
	if(CMiniMap_Common::OutputDebugTransitions())
	{
		if (ms_bInBigMap != bBigMap)  // because script like to spam these things
		{
			if (bBigMap)
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap::SetInBigMap(TRUE) (UT)");
			else
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap::SetInBigMap(FALSE) (UT)");
		}
	}
#endif

	if (ms_bInBigMap != bBigMap)
	{
		CScriptHud::ms_iRadarZoomValue = 0;  // only alter these zoom values if the bigmap actually is changing state
		CScriptHud::ms_fMiniMapForcedZoomPercentage = 0.0f;

		ms_bInBigMap = bBigMap;

		if (!bBigMap)
		{
			ms_bBigMapFullZoom = ms_bBigMapFullScreen = false;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetInGolfMap
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetInGolfMap(bool bGolfMap)
{
#if !__FINAL
	if(CMiniMap_Common::OutputDebugTransitions())
	{
		if (ms_bInGolfMap != bGolfMap)  // because script like to spam these things
		{
			if (bGolfMap)
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap::SetInGolfMap(TRUE) (UT)");
			else
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap::SetInGolfMap(FALSE) (UT)");
		}
	}
#endif

	ms_bInGolfMap = bGolfMap;

	if (!bGolfMap)
	{
		ms_iGolfCourseMap = GOLF_COURSE_OFF;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::Init
// PURPOSE:	initialises the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		ms_bXmlDataHasBeenRead = false;

		for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
		{
			ms_Waypoint[i].iPlayerModelHash = 0;
			ms_Waypoint[i].iBlipIndex = INVALID_BLIP_ID;
			ms_Waypoint[i].vPos = Vector2(0,0);
		}

		ms_PointOfInterest.Reset();

		REGISTER_FRONTEND_XML(CMiniMap::HandleCoreXML, XML_CORE_NODE_NAME);
		REGISTER_FRONTEND_XML(CMiniMap::HandleInteriorXML, "minimap_interiors");
#if COMPANION_APP
		//	Initialise the Companion App stuff here
		CCompanionData::GetInstance()->Initialise();
#endif	//	COMPANION_APP
	}
	else if(initMode == INIT_SESSION)
	{
		ms_BitmapBackground.Reset();
		//@@: location CMINIMAP_INIT_RESIZE_MAP
		ms_BitmapBackground.Resize(MM_BITMAP_VERSION_NUM_ENUMS, sm_Tunables.Bitmap.iBitmapTilesX, sm_Tunables.Bitmap.iBitmapTilesY);

		for (s32 a = 0; a < MM_BITMAP_VERSION_NUM_ENUMS; a++)
		{
			for (s32 i = 0; i < sm_Tunables.Bitmap.iBitmapTilesX; i++)
			{
				for (s32 j = 0; j < sm_Tunables.Bitmap.iBitmapTilesY; j++)
				{
					ms_BitmapBackground(a,i,j).iTxdId = -1;
					ms_BitmapBackground(a,i,j).iState = MM_BITMAP_NONE;
				}
			}
		}

		//Clear waypoints state
		ms_bEntityWaypointAllowed = false;
		ms_waypointGpsFlags = 0;
		ms_bWaypointClearOnArrivalMode = eWaypointClearOnArrivalMode::Enabled;
		for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
		{
			ClearWaypointBlipAndRoute(ms_Waypoint[i]);
			ms_Waypoint[i].iPlayerModelHash = 0;
		}

		ms_bBitmapInitialized = true;

		ms_bFlashWantedOverlay = false;

		ms_iLockedMiniMapAngle = -1;
		ms_vLockedMiniMapPosition = Vector2(-9999.0f,-9999.0f);

		ms_iCentreBlip = INVALID_BLIP_ID;
		ms_iNorthBlip = INVALID_BLIP_ID;
//		ms_iBlipHovering = -1;
		ms_bInStealthMode = true;  // PhilH wants this ON as default
		ms_bHideBackgroundMap = false;
		ms_bBlockWaypoint = false;
		ms_bInPrologue = false;
		ms_bOnIslandMap = false;
		ms_bInPauseMap = false;
		ms_bInCustomMap = false;
		ms_bInBigMap = false;
		ms_iGolfCourseMap = GOLF_COURSE_OFF;
		ms_iLockedMiniMapAngle = -1;

#if __BANK
		ms_g_bDisplayGameMiniMap = true;
		ms_iDebugGolfCourseMap = -1;

		ms_bOverrideConeColour = false;
		ms_ConeColourRed = 47;
		ms_ConeColourGreen = 92;
		ms_ConeColourBlue = 115;
		ms_ConeColourAlpha = CMiniMap_Common::ALPHA_OF_POLICE_PERCEPTION_CONES;

		ms_fConeFocusRangeMultiplier = 1.05f;
		ms_bDrawActualRangeCone = false;
#endif
		if (!ms_bXmlDataHasBeenRead)  // only create a new minimap movie once - not between sessions (its a waste of time as it removes and re-adds shortly after)
		{
			LoadXmlData();
			ms_bXmlDataHasBeenRead = true;

			OldMiniMapConstructor();
		}
		else
		{
			// Make sure the movies are locked so the SetActive will unlock them
			CScaleformMgr::LockMovie(GetMovieId(MINIMAP_MOVIE_BACKGROUND));
			CScaleformMgr::LockMovie(GetMovieId(MINIMAP_MOVIE_FOREGROUND));
		}

		MinimapModeState = MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP;  // fix for 1390231

		SetSimpleBlipDefaults();
		CreateCentreAndNorthBlips();

		SetInPrologue(false);
		SetOnIslandMap(false);

		// ensure the minimap is set up at init session in order to continue safely:
		SetActive();
		
#if ENABLE_FOG_OF_WAR
		if( GetIgnoreFowOnInit() == false )
		{
			SetRequestFoWClear(true);
			SetFoWMapValid(false);
			SetHideFow(false);
		}
#endif
		uiDisplayf("Minimap is now active");
	}


	ms_MiniMapReappearanceState.iMiniMapRenderedTimer = 0;
	ms_MiniMapReappearanceState.MinimapModeStateWhenHidden = MINIMAP_MODE_STATE_NONE;
	ms_MiniMapReappearanceState.vMiniMapCentrePosWhenHidden.Zero();
	ms_MiniMapReappearanceState.bMiniMapInteriorWhenHidden = false;
	ms_MiniMapReappearanceState.bBeenInPauseMenu = false;

	ms_previousInterior.Clear();
	ms_blipCones.Init(initMode);
	m_FakeCones.clear();
	m_ConeColors.Reset();

	CMiniMap_Common::Init(initMode);
	CMiniMap_RenderThread::Init(initMode);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::Shutdown
// PURPOSE:	shuts down the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::Shutdown(unsigned shutdownMode)
{
	{ // scope for the AutoLocks
	CScaleformMgr::AutoLock lockFore(GetMovieId(MINIMAP_MOVIE_FOREGROUND));
	CScaleformMgr::AutoLock lockBack(GetMovieId(MINIMAP_MOVIE_BACKGROUND));

	if (shutdownMode == SHUTDOWN_CORE)
	{
#if __BANK
		ShutdownWidgets();
#endif // __BANK

		CMiniMap_Common::Shutdown(shutdownMode);
		CMiniMap_RenderThread::Shutdown(shutdownMode);
		CMiniMap_RenderThread::RemoveContainers();

		RemoveMiniMapMovie();
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{	
		CScaleformMgr::ForceMovieUpdateInstantly(GetMovieId(MINIMAP_MOVIE_BACKGROUND), false);
		CScaleformMgr::ForceMovieUpdateInstantly(GetMovieId(MINIMAP_MOVIE_FOREGROUND), false);

		//	ms_bInsideReInit - we don't want to remove all blips when transitioning between MP and SP.
		//	We should leave it to script to remove their own blips so that the number of SCRIPT_RESOURCE_RADAR_BLIP doesn't report more blips than actually exist

		const s32 actualSimpleBlipId = GetActualSimpleBlipId();
		const s32 actualNorthBlipId = GetActualNorthBlipId();
		const s32 actualCentreBlipId = GetActualCentreBlipId();

		for (s32 iIndex = iMaxCreatedBlips-1; iIndex >= 0; iIndex--)  // remove them in reverse order so that the iMaxCreatedBlips should also get adjusted
		{
			CMiniMapBlip *pBlip = GetBlipFromActualId(iIndex);

			if (!pBlip)
				continue;

			if (pBlip->IsComplex())
			{	//	Not sure what to do if the blip is simple. Ask Derek if it can ever be simple and be on the stage
				CMiniMap_RenderThread::RemoveBlipFromStage((CBlipComplex*)pBlip, true);
			}
			else
			{
				uiAssertf(0, "CMiniMap::Shutdown - Graeme - just checking if this ever happens for simple blips");
			}

			if (!ms_bInsideReInit
				|| (iIndex == actualSimpleBlipId) 
				|| (iIndex == actualNorthBlipId) 
				|| (iIndex == actualCentreBlipId) )
			{
				RemoveBlip(pBlip);
				DestroyBlipObject(pBlip); 
			}
		}

		if (!ms_bInsideReInit)
		{
			iMaxCreatedBlips = 0;
			ms_PointOfInterest.Reset();
		}

		bActive = false;

		// at end of each session, we flag to perform garbage collection on the minimap
		CScaleformMgr::ForceCollectGarbage(GetMovieId(MINIMAP_MOVIE_BACKGROUND));
		CScaleformMgr::ForceCollectGarbage(GetMovieId(MINIMAP_MOVIE_FOREGROUND));

		CMiniMap_Common::Shutdown(shutdownMode);
		CMiniMap_RenderThread::Shutdown(shutdownMode);

		RestartMiniMapMovie();
	}

	} // scope for the AutoLocks - not sure if gRenderThreadInterface.Flush() is safe to do while the movies are locked. better to be careful.
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::RestartMiniMapMovie
// PURPOSE:	restarts the minimap movies - removes all stuff thats been setup,
//			restarts the movies (removes & re-adds movieview) and re-sets up movies
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::RestartMiniMapMovie()
{
	CMiniMap_RenderThread::RemoveContainers();

	// restart the background movie
	if (!CScaleformMgr::RestartMovie(GetMovieId(MINIMAP_MOVIE_BACKGROUND), true, false))
	{
		uiAssertf(0, "CMiniMap::Init - Couldnt restart the foreground movie");
	}

	// restart the blips movie
	if (!CScaleformMgr::RestartMovie(GetMovieId(MINIMAP_MOVIE_FOREGROUND), true, false))
	{
		uiAssertf(0, "CMiniMap::Init - Couldnt restart the foreground movie");
	}

	CMiniMap_RenderThread::SetupContainers();

	// turn off all components
	for (s32 iComponentId = 0; iComponentId < MAX_MINIMAP_COMPONENTS; iComponentId++)
	{
		ToggleComponent(iComponentId, false, true);
	}

	CNewHud::RepopulateHealthInfoDisplay();

	CMiniMap_RenderThread::SetupHealthArmourOnStage(false);

	CMiniMap_RenderThread::UpdateMapVisibility();
}



void CMiniMap::HandleCoreXML(parTreeNode* pMiniMapNode)
{
#if __ASSERT
	parAttribute* pVersion = pMiniMapNode->GetElement().FindAttribute("v");
	if( uiVerifyf(pVersion, "CMinimap is expecting a versioned node \"" XML_CORE_NODE_NAME "\" in frontend.xml. Please get latest on it." ))
	{
		float xmlVersion = static_cast<float>(atof(pVersion->GetStringValue()));
		uiAssertf(xmlVersion == XML_VERSION, "CMiniMap expected a version of %f in \"" XML_CORE_NODE_NAME "\" but found %f instead!", XML_VERSION, xmlVersion);
	}
#endif // __ASSERT
	s32 iCount = MINIMAP_VALUE_MINIMAP;  // start at standard style
	parTreeNode* pNode = NULL;
	while ((pNode = pMiniMapNode->FindChildWithName("data", pNode)) != NULL)
	{
		safecpy(&sm_miniMaps[iCount].m_fileName[0], pNode->GetElement().FindAttributeAnyCase("name")->GetStringValue(), MAX_LENGTH_OF_MINIMAP_FILENAME);

		sm_miniMaps[iCount].m_position.x = (float)atof(pNode->GetElement().FindAttributeAnyCase("posX")->GetStringValue());
		sm_miniMaps[iCount].m_position.y = (float)atof(pNode->GetElement().FindAttributeAnyCase("posY")->GetStringValue());

		sm_miniMaps[iCount].m_size.x = (float)atof(pNode->GetElement().FindAttributeAnyCase("sizeX")->GetStringValue());
		sm_miniMaps[iCount].m_size.y = (float)atof(pNode->GetElement().FindAttributeAnyCase("sizeY")->GetStringValue());

		sm_miniMaps[iCount].m_alignX = (pNode->GetElement().FindAttributeAnyCase("alignX")->GetStringValue())[0];
		sm_miniMaps[iCount].m_alignY = (pNode->GetElement().FindAttributeAnyCase("alignY")->GetStringValue())[0];

		/*
		// need to adjust the mask from 16:9 to current aspect ratio in use by the scaleform movies
 		if (iCount == MINIMAP_VALUE_MINIMAP_MASK_STANDARD_16_9 || iCount == MINIMAP_VALUE_MINIMAP_MASK_BIGMAP_16_9)
 		{
 			sm_miniMaps[iCount].m_size.x += CHudTools::GetDifferenceFrom_16_9_ToCurrentAspectRatio() * 0.3f;  // use the difference we currently have and scale it for now, needs revising!
 		}

		// only a certain few want to be adjusted for widescreen at load time (probably NONE of these on PC... but that's for later)
		if( iCount == MINIMAP_VALUE_STANDARD ||
			iCount == MINIMAP_VALUE_GOLF_COURSE ||
			iCount == MINIMAP_VALUE_BIGMAP ||
			iCount == MINIMAP_VALUE_MINIMAP_BLUR ||
			iCount == MINIMAP_VALUE_BIGMAP_BLUR )
		{
			CHudTools::AdjustForWidescreen(WIDESCREEN_FORMAT_LEFT, &sm_miniMaps[iCount].m_position, &sm_miniMaps[iCount].m_size, NULL);
		}
		*/

		iCount++;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::LoadXmlData()
// PURPOSE:	reads in the xml data and stores it
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::LoadXmlData(bool bForceReload)
{
	CFrontendXMLMgr::LoadXML(bForceReload);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::HandleInteriorXML()
// PURPOSE:	reads in the xml data and stores it
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::HandleInteriorXML(parTreeNode* pMiniMapNode)
{
	sm_InteriorInfo.Reset();

	parTreeNode* pNode = NULL;
	parTreeNode* pNodeContent = NULL;

	while ((pNode = pMiniMapNode->FindChildWithName("interior", pNode)) != NULL)
	{
		sMiniMapInteriorStruct newContentInfo;

		newContentInfo.iMainInteriorHash = atStringHash(pNode->GetElement().FindAttributeAnyCase("name")->GetStringValue());
		newContentInfo.vMainInteriorPos.x = (float)atof(pNode->GetElement().FindAttributeAnyCase("posX")->GetStringValue());
		newContentInfo.vMainInteriorPos.y = (float)atof(pNode->GetElement().FindAttributeAnyCase("posY")->GetStringValue());
		newContentInfo.fMainInteriorRot = (float)atof(pNode->GetElement().FindAttributeAnyCase("rot")->GetStringValue());

		while ((pNodeContent = pNode->FindChildWithName("content", pNodeContent)) != NULL)
		{
			u32 iNewSubInteriorHash = atStringHash(pNodeContent->GetElement().FindAttributeAnyCase("name")->GetStringValue());
			newContentInfo.iSubInteriorHash.PushAndGrow(iNewSubInteriorHash);
		}

		sm_InteriorInfo.PushAndGrow(newContentInfo);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateAtEndOfFrame
// PURPOSE:	various stuff that needs to be done in the 'safe area' at the end of the
//			frame, like removing blips, requesting stream or removal of other movies
//			removed any unused blips that have been marked for deletion and already
//			removed from the stage in a safe manner
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateAtEndOfFrame()
{
	if (!bActive)  // if not active yet, then dont update at end of frame
		return;

	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::UpdateAtEndOfFrame has been called before MiniMap has been fully initialised");
		return;
	}


//	if (CMiniMap_RenderThread::HasRenderStateStructBeenReceivedByRenderThread())
	if (CMiniMap_RenderThread::HasRenderThreadProcessedThisMinimapModeState(MinimapModeState))
	{
		if (MinimapModeState == MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP)  // ensure health and armour are repopulated after the state has been applied on the RT
		{
			CNewHud::RepopulateHealthInfoDisplay();
		}

		MinimapModeState = MINIMAP_MODE_STATE_NONE;

		// apply any pending transition that has previously been requested here:
		if (ms_pendingMinimapModeState != MINIMAP_MODE_STATE_NONE)
		{
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
			{
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap::UpdateAtEndOfFrame -- Setting pending minimap mode state %d", ms_pendingMinimapModeState);
			}
#endif //!__FINAL

			SetMinimapModeState(ms_pendingMinimapModeState);

			ms_pendingMinimapModeState = MINIMAP_MODE_STATE_NONE;
		}
	}

	//
	// removal of any unused blips:
	//
	for (s32 iIndex = 0; iIndex < iMaxCreatedBlips; iIndex++)
	{
		if (iIndex == GetActualSimpleBlipId())
			continue;

		CMiniMapBlip *pBlip = GetBlipFromActualId(iIndex);
		
		if (!pBlip)
			continue;

		if (IsFlagSet(pBlip, BLIP_FLAG_VALUE_DESTROY_BLIP_OBJECT))  // try and remove the blip if this flag is set but it will only remove it if its no longer on the stage
		{
			DestroyBlipObject(pBlip);  // we may remove this blip here
		}
		else
		{
			if (CMiniMap_RenderThread::DoesRenderedBlipExist(iIndex))
			{
				SetFlag(pBlip, BLIP_FLAG_ON_STAGE);
			}
			else
			{
				UnsetFlag(pBlip, BLIP_FLAG_ON_STAGE);
			}
		}
	}

	CMiniMap_RenderThread::ProcessAtEndOfFrame();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::Update
// PURPOSE:	updates the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::Update()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap:: Update can only be called on the UpdateThread!");
		return;
	}

	//
	// debug code so we can test the zoom to distance and zoom to blip code
	//
#if __BANK
	if (s_bZoomToWaypointBlip)
	{
		s32 iBlipIndex = CMiniMap::GetActiveWaypointId();

		if (iBlipIndex != INVALID_BLIP_ID)
		{
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);

			if (pBlip)
			{
				Vector3 vCentrePos;
				Vector3 vBlipPos = CMiniMap::GetBlipPositionValue(pBlip);
				CMiniMap::GetCentrePosition(vCentrePos);

				Vector3 deltaFromTargetXY = vCentrePos-vBlipPos;
				deltaFromTargetXY.z = 0.0f;

				CScriptHud::ms_fRadarZoomDistanceThisFrame = deltaFromTargetXY.Mag() + s_fRadarZoomDistanceThisFrameWidget;
			}
		}
	}
	else
	{
		if (s_fRadarZoomDistanceThisFrameWidget != 0.0f)  // set this every frame if its been set in rag
		{
			CScriptHud::ms_fRadarZoomDistanceThisFrame = s_fRadarZoomDistanceThisFrameWidget;
		}
	}
#endif
	//
	// end of debug code so we can test the zoom to distance and zoom to blip code
	//

	if ( !ShouldProcessMinimap() || 
		( IsInPauseMap() && !CPauseMenu::IsInMapScreen() && !CNetwork::IsGameInProgress()) )
	{
		//  		if(cStoreScreenMgr::IsStoreMenuOpen())  // valid "updatestate" needs to be sent all the time to keep things transitioning correctly so flags etc set match what we have sent to AS
		//  		{
		//  			DLC(CMiniMap_RenderState_Setup, ());
		//  		}
		//  		else
		// 		{
		UpdateStatesOnUT();	//	Graeme - The render thread uses ms_MiniMapRenderState.m_bShouldProcessMiniMap to know whether to render or not. So I'll try calling this in here
		//	so that the render thread knows not to render. Without this, I was seeing the minimap even when the scripts were calling HIDE_HUD_AND_RADAR_THIS_FRAME
		//	}

		UpdateBlips();			//	Graeme - B*1320771 - we were getting a "Cannot find a free blip slot - maximum has been hit" assert if script removed lots of blips and added lots of new ones
		//	while ShouldProcessMinimap() was returning false. The Update Thread can only remove a blip properly once it has told the Render Thread to remove the blip from the stage.
		//	For that to happen, we need to call UpdateBlips() even if the rest of UpdateMiniMap() isn't going to get called.
		//	Maybe as an alternative we could have always processed the minimap fully and just chosen not to render it where we're currently saying not to process it.

		return;
	}

#if	!__FINAL
	if (PARAM_nominimap.Get())
	{
		if (!IsInPauseMap())
			return;
	}
#endif // !__FINAL

	PF_PUSH_TIMEBAR_DETAIL("Scaleform Movie Update (MiniMap)");

	UpdateCentreAndNorthBlips();
	UpdateWaypoint();  // ensure we always have the correct waypoint in use for the current player in use

	UpdateMiniMap();

	PF_POP_TIMEBAR_DETAIL();
}

#if RSG_PC
void CMiniMap::DeviceLost()
{
	uiDebugf1("Device got lost, but we don't do anything about it for the minimap");
}

void CMiniMap::DeviceReset()
{
	if( ms_MiniMapReappearanceState.MinimapModeStateWhenHidden != MINIMAP_MODE_STATE_NONE )
		s_iDeviceLostResetCountdown = 2; // we need to wait a bit, otherwise the DLC call gets lost and we don't resize.
}

#endif // RSG_PC

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetCurrentGolfMap
// PURPOSE: sets the golf course map (and also turns on/off the sea background)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetCurrentGolfMap(eGOLF_COURSE_HOLES iValue)
{
	if (ms_iGolfCourseMap != iValue)
	{
		ms_iGolfCourseMap = iValue;  // set the new course map after we setup the state

		if (iValue != GOLF_COURSE_OFF)
		{
			// turn on golf map (or set a new golf map)
			if (!CPauseMenu::IsActive())  // dont set new golf map if in pausemenu (pausemap or custom maps)
			{
				SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP);  // setup whatever golf map we may need
			}
		}
		else
		{
			// turn off golf map
			SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP);  // back to standard minimap if we turn it off
		}
	}
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetActualCentreBlipId
// PURPOSE: returns the actual blip array id for the centre blip
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::GetActualCentreBlipId()
{
	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::GetActualCentreBlipId has been called before MiniMap has been fully initialised");
		return -1;
	}

	return (ConvertUniqueBlipToActualBlip(ms_iCentreBlip));
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetActualNorthBlipId
// PURPOSE: returns the actual blip array id for the north blip
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::GetActualNorthBlipId()
{
	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::GetActualNorthBlipId has been called before MiniMap has been fully initialised");
		return -1;
	}

	return (ConvertUniqueBlipToActualBlip(ms_iNorthBlip));
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::RevealCoordinate
// PURPOSE: returns the actual blip array id for the north blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::RevealCoordinate(const Vector2& coordinates)
{
	uiAssertf(ms_numScriptRevealRequested<=8,"Trying to reveal more than 8 coordinates in the FoW map");
	if( ms_numScriptRevealRequested < 8 )
	{
		ms_ScriptReveal[ms_numScriptRevealRequested] = coordinates;
		ms_numScriptRevealRequested++;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::CreateCentreAndNorthBlips
// PURPOSE: creates the player position and north blips
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::CreateCentreAndNorthBlips()
{
	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::CreateCentreAndNorthBlips has been called before MiniMap has been fully initialised");
		return;
	}

	ms_iCentreBlip = CreateBlip(true, BLIP_TYPE_COORDS, Vector3(0,0,1.0f), BLIP_DISPLAY_BLIPONLY, "centre_blip");
	SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, ms_iCentreBlip, BLIP_CENTRE);
	SetBlipParameter(BLIP_CHANGE_SCALE, ms_iCentreBlip, 0.7f);  // slightly smaller
	SetBlipParameter(BLIP_CHANGE_PRIORITY, ms_iCentreBlip, BLIP_PRIORITY_ONTOP_OF_EVERYTHING);
	SetBlipParameter(BLIP_CHANGE_BRIGHTNESS, ms_iCentreBlip, true);
	SetBlipParameter(BLIP_CHANGE_CATEGORY, ms_iCentreBlip, BLIP_CATEGORY_LOCAL_PLAYER);

	Vector3 vNorthPos(0.0f,CMiniMap::sm_Tunables.Tiles.vMiniMapWorldSize.y*2.0f, 0.0f);
 	ms_iNorthBlip = CreateBlip(true, BLIP_TYPE_COORDS, vNorthPos, BLIP_DISPLAY_MINIMAP_OR_BIGMAP, "North_blip");
	SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, ms_iNorthBlip, BLIP_NORTH);
	SetBlipParameter(BLIP_CHANGE_PRIORITY, ms_iNorthBlip, BLIP_PRIORITY_LOW_SPECIAL);  // bug 623249 requests NORTH blip is always ontop of everything - thgen 783144 says otherwise
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateCentreAndNorthBlips
// PURPOSE: updates the player position
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateCentreAndNorthBlips()
{
	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::UpdateCentreAndNorthBlips has been called before MiniMap has been fully initialised");
		return;
	}

	ms_bCentreBlipPosChangedThisFrame = false;

	if ((!IsInPauseMap()) && ms_iNorthBlip != INVALID_BLIP_ID)
	{
		if (!IsInGolfMap())
		{
			SetBlipParameter(BLIP_CHANGE_DISPLAY, ms_iNorthBlip, BLIP_DISPLAY_MINIMAP_OR_BIGMAP);
		}
		else
		{
			SetBlipParameter(BLIP_CHANGE_DISPLAY, ms_iNorthBlip, BLIP_DISPLAY_NEITHER);
		}
	}

	const CPed * pLocalPlayer = GetMiniMapPed();

	if (pLocalPlayer && ms_iCentreBlip != INVALID_BLIP_ID)
	{
		CMiniMapBlip *pBlip = GetBlip(ms_iCentreBlip);

		if (!pBlip)
			return;

		float fHeading = pLocalPlayer->GetTransform().GetHeading() * RtoD;
		Vector3 vPosition = GetPlayerBlipPosition();

		// Cache whether the player position has changed this frame
		ms_bCentreBlipPosChangedThisFrame = (vPosition != GetBlipPositionValue(pBlip));

		const CVehicle *pVehicle = pLocalPlayer->GetVehiclePedInside();
		if (pVehicle && (pLocalPlayer->GetIsDrivingVehicle() || CNetwork::IsGameInProgress()) )
		{
			// fix for 1493320 - using this instead of heading means we get the dir regardless of whether upside down or not, works in all vehicles really well
			Matrix34 mat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());

			Vector3 eulers;
			mat.ToEulersYXZ(eulers);

			fHeading = eulers.z * RtoD;
		}

		if (IsInPauseMap())
		{
			SetBlipParameter(BLIP_CHANGE_FLASH, ms_iCentreBlip, true);
		}
		else
		{
			// use debugcam position/rotation and flash if debug cam is active
#if !__FINAL
			if (camInterface::GetDebugDirector().IsFreeCamActive() BANK_ONLY( DEBUG_DRAW_ONLY( && !CMiniMap_RenderThread::sm_bDrawCollisionVolumes)))
			{
				vPosition = camInterface::GetPos();
				fHeading =  camInterface::GetHeading() * RtoD;
				SetBlipParameter(BLIP_CHANGE_FLASH, ms_iCentreBlip, true);
				SetBlipParameter(BLIP_CHANGE_FLASH_INTERVAL, ms_iCentreBlip, 200);
			}
			else
#endif // __FINAL
			{
				SetBlipParameter(BLIP_CHANGE_FLASH, ms_iCentreBlip, false);
			}
		}
		
		SetBlipParameter(BLIP_CHANGE_DIRECTION, ms_iCentreBlip, fHeading);
		SetBlipParameter(BLIP_CHANGE_POSITION, ms_iCentreBlip, vPosition);

		//
		// set the centre blip to the network team colour if we are in a network game:
		//
		if (NetworkInterface::IsGameInProgress())
		{
/*			s32 iColour;

			if (!NetworkInterface::GetLocalPlayer()->HasTeam() || CScriptHud::bUsePlayerColourInsteadOfTeamColour)
			{
				iColour = NetworkColours::MapNetworkColourToRadarBlipColour(NetworkInterface::GetLocalPlayer()->GetColour());
			}
			else
			{
				iColour = NetworkColours::MapNetworkColourToRadarBlipColour(NetworkColours::GetTeamColour(NetworkInterface::GetLocalPlayer()->GetTeam()));
			}

			if (GetBlipColourValue(pBlip) != iColour)
			{
				SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, iColour);
			}*/
		}
		else
		{
			if (CScriptHud::m_playerBlipColourOverride != BLIP_COLOUR_DEFAULT)
			{
 				if (GetBlipColourValue(pBlip) != CScriptHud::m_playerBlipColourOverride)
				{
 					SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, CScriptHud::m_playerBlipColourOverride);
 				}
			}
			else
			{
				if (!IsInPauseMap())
				{

					/*CWanted *pWanted = CGameWorld::FindLocalPlayerWanted();
					if(pWanted)
					{
						int wantedLevel = CNewHud::GetHudWantedLevel(pWanted);
						if (CWanted::GetWantedBlips().GetIsBeingAttacked())
						{
							// light red when wanted and "hidden to cops"
							if (GetBlipColourValue(pBlip) != BLIP_COLOUR_WANTED)
							{
								SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, BLIP_COLOUR_WANTED);
							}
						}
						else if(wantedLevel > 0)
						{
							if(pWanted->CopsAreSearching())
							{
								// grey when wanted and "hidden to cops"
								if (GetBlipColourValue(pBlip) != BLIP_COLOUR_STEALTH_GREY)
								{
									SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, BLIP_COLOUR_STEALTH_GREY);
								}
							}
							else
							{
								// light red when wanted and "hidden to cops"
								if (GetBlipColourValue(pBlip) != BLIP_COLOUR_WANTED)
								{
									SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, BLIP_COLOUR_WANTED);
								}
							}
						}
						else
						{
							// white at all other times
							if (GetBlipColourValue(pBlip) != BLIP_COLOUR_WHITE)
							{
								SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, BLIP_COLOUR_WHITE);
							}
						}
					}
					else*/
						CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
					CWanted* pWanted = NULL;
					if(pPlayerPed)
						pWanted = pPlayerPed->GetPlayerWanted();
					bool bStealth = false;
					if(pWanted)
					{
						bool bTemp;
						int wantedLevel = CNewHud::GetHudWantedLevel(pWanted, bTemp);
						if(wantedLevel > 0 && pWanted->CopsAreSearching())
						{
							if(CPedGeometryAnalyser::IsPedInBush(*pPlayerPed) ||
								CPedGeometryAnalyser::IsPedInUnknownCar(*pPlayerPed) ||
								CPedGeometryAnalyser::IsPedInWaterAtDepth(*pPlayerPed, CPedGeometryAnalyser::ms_MaxPedWaterDepthForVisibility))
							{
								bStealth = true;
							}
						}
					}

					if(bStealth)
					{
						// grey when hidden
						if (GetBlipColourValue(pBlip) != BLIP_COLOUR_STEALTH_GREY)
						{
							SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, BLIP_COLOUR_STEALTH_GREY);
						}
					}
					else 
					{
						// white at all other times
						if (GetBlipColourValue(pBlip) != BLIP_COLOUR_WHITE)
						{
							SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, BLIP_COLOUR_WHITE);
						}

						// keep alpha at 255 at all times in SP
						if (GetBlipAlphaValue(pBlip) != 255)
						{
							SetBlipParameter(BLIP_CHANGE_ALPHA, ms_iCentreBlip, 255);
						}
					}
				}
				else
				{
					// player colour in SP:  (1548193)
					eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

					eBLIP_COLOURS blipColour;

					switch (hudColour)
					{
						case HUD_COLOUR_MICHAEL:
						{
							blipColour = BLIP_COLOUR_MICHAEL;
							break;
						}

						case HUD_COLOUR_FRANKLIN:
						{
							blipColour = BLIP_COLOUR_FRANKLIN;
							break;
						}

						case HUD_COLOUR_TREVOR:
						default:
						{
							blipColour = BLIP_COLOUR_TREVOR;
							break;
						}
					}

					if (GetBlipColourValue(pBlip) != blipColour)
					{
						SetBlipParameter(BLIP_CHANGE_COLOUR, ms_iCentreBlip, blipColour);
					}
				}
			}
		}
	}
}

Vector3 CMiniMap::GetPlayerBlipPosition()
{
	Vector3 vPosition;
	Vector2 vFakePlayerPosition;

	if (CScriptHud::GetFakedPauseMapPlayerPos(vFakePlayerPosition))
	{
		vPosition.Set(vFakePlayerPosition.x, vFakePlayerPosition.y, 0.0f);
	}
	else
	{
		const CPed* pLocalPlayer = GetMiniMapPed();
		if(pLocalPlayer)
		{
			vPosition = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());
		}
	}

	return vPosition;
}

/////////////////////////////////////////////////////////////////////////////////////

void sPointOfInterestStruct::Delete()
{
	CMiniMap::RemoveBlip(GetBlip());
}

CMiniMapBlip* sPointOfInterestStruct::GetBlip() const
{
	if(iUniqueBlipId != INVALID_BLIP_ID)
	{
		return CMiniMap::GetBlip(iUniqueBlipId);
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateDisplayConfig()
// PURPOSE:	updates safezone info on scaleform
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateDisplayConfig()
{
	for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
	{
		CScaleformMgr::ChangeMovieParams(iMovieId[i], GetCurrentMiniMapPosition(), GetCurrentMiniMapSize(), GFxMovieView::SM_ExactFit);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::AddOrRemovePointOfInterest()
// PURPOSE:	turns on or off a POI blip - up to 10 (bug 989055)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::AddOrRemovePointOfInterest(float fPosX, float fPosY)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::SwitchOnWaypoint can only be called on the UpdateThread!");
		return;
	}

	#define POI_BLIP_OVERLAY_RADIUS (400.0f)
	float fBlipOverlayRadius2 = rage::square(POI_BLIP_OVERLAY_RADIUS / CMiniMap::GetPauseMapScale());
	float fClosestMatch = POI_BLIP_OVERLAY_RADIUS*POI_BLIP_OVERLAY_RADIUS; // plenty big size
	int iClosestIndex = -1;

	Vector3 vPos(fPosX, fPosY, 1.0f);

	for (s32 iId = 0; iId < ms_PointOfInterest.size(); ++iId)
	{
		CMiniMapBlip *pBlip = ms_PointOfInterest[iId].GetBlip();

		if (pBlip)
		{
			Vector3 vBlipPos(CMiniMap::GetBlipPositionValue(pBlip));
			float fCurDist = vBlipPos.Dist2(vPos);
			if ( fCurDist < fBlipOverlayRadius2 && fCurDist < fClosestMatch )
			{
				fCurDist = fClosestMatch;
				iClosestIndex = iId;
			}
		}
	}

	if( iClosestIndex != -1 )
	{
		// turn off this blip
		ms_PointOfInterest[iClosestIndex].Delete();
		ms_PointOfInterest.Delete(iClosestIndex);
		return;
	}

	if( ms_PointOfInterest.IsFull() )
	{
		// if full, shift contents and recycle the front's index
		// this COULD be implemented as a circular/round-robin/ring buffer, but those add complexity and apparently we don't have any (good) templates for them?

		sPointOfInterestStruct temp = ms_PointOfInterest[0];
		ms_PointOfInterest.Delete(0);
		ms_PointOfInterest.Push( temp );

		SetBlipParameter(BLIP_CHANGE_POSITION, temp.iUniqueBlipId, vPos);
		return;
	}

	s32 iNewBlipId = CreateBlip(true, BLIP_TYPE_COORDS, vPos, BLIP_DISPLAY_PAUSEMAP, "poi");
	if (iNewBlipId != INVALID_BLIP_ID) // rather rare, but if no room, don't add entries to the array
	{
		sPointOfInterestStruct& newPoi = ms_PointOfInterest.Append();
		newPoi.iUniqueBlipId = iNewBlipId;
		SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, newPoi.iUniqueBlipId, RADAR_TRACE_POI);
		SetBlipParameter(BLIP_CHANGE_PRIORITY, newPoi.iUniqueBlipId, BLIP_PRIORITY_LOWEST);
	}
}


bool CMiniMap::GetPointOfInterestDetails(s32 index, Vector2& vPosition)
{
	vPosition.Set(0.0f, 0.0f);

	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::GetPointOfInterestDetails can only be called on the UpdateThread!");
		return false;
	}
	// because the special POI was unused, it's been optimized away. HOWEVER, the save data can't really be touched
	// so we'll do this and let it lie fallow in there
	if( index == POI_SPECIAL_ID)
		return false;

	if( index >= 0 && index < ms_PointOfInterest.GetCount() )
	{
		CMiniMapBlip *pBlip = ms_PointOfInterest[index].GetBlip();

		if (pBlip)
		{
			Vector3 vBlipPos(CMiniMap::GetBlipPositionValue(pBlip));

			vPosition.x = vBlipPos.x;
			vPosition.y = vBlipPos.y;
			return true;
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SwitchOnWaypoint()
// PURPOSE:	switches on the waypoint blip - pass in the blip sprite we want to use
//			as the waypoint blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SwitchOnWaypoint(float fPosX, float fPosY, ObjectId ObjectId, bool bHideWaypointIcon, bool bSetLocalDirty, eHUD_COLOURS color /* = eHUD_COLOURS::HUD_COLOUR_WAYPOINT*/)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::SwitchOnWaypoint can only be called on the UpdateThread!");
		return;
	}

	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::SwitchOnWaypoint has been called before MiniMap has been fully initialised");
		return;
	}

	s32 iWaypointToUse = -1;

	if (NetworkInterface::IsGameInProgress())
	{
		iWaypointToUse = MULTIPLAYER_WAYPOINT_MARKER_ID;
	}
	else
	{
		// find a slot that has no blip used:
		for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
		{
			if (ms_Waypoint[i].iBlipIndex == INVALID_BLIP_ID && ms_Waypoint[i].iPlayerModelHash == 0)
			{
				iWaypointToUse = i;
				break;
			}
		}
	}

	if (iWaypointToUse == -1)
	{
		uiAssertf(0, "MiniMap: Not enough waypoint slots to create new marker, replacing slot 0");
		iWaypointToUse = 0;  // if not enough free slots, then use the 1st to ensure we can always create a new route.
	}

	sWaypointStruct& Waypoint = ms_Waypoint[iWaypointToUse];

	ClearWaypointBlipAndRoute(Waypoint);

	// if we have a free slot, create the blip and store the info:
	Waypoint.iPlayerModelHash = 0;
	CPed *pPlayerPed = FindPlayerPed();
	if (pPlayerPed)
	{
		Waypoint.iPlayerModelHash = pPlayerPed->GetPedModelInfo()->GetHashKey();  // store player model index used
	}

	//Set waypoint location
	Waypoint.vPos.Set(fPosX, fPosY);
	
	//Search for best Blip
	float MinBlipDistanceSq = 0.1f;
	s32 ClosestBlip = INVALID_BLIP_ID;
	if (NetworkInterface::IsGameInProgress())
	{
		if (netObject* pObj = NetworkInterface::GetObjectManager().GetNetworkObject(ObjectId))
		{
			if (CEntity* pWaypointEntity = pObj->GetEntity())
			{
				const Vector3 WaypointLocation(Waypoint.vPos.x, Waypoint.vPos.y, 0.0f);
				for (s32 iIndex = 0; iIndex < iMaxCreatedBlips; iIndex++)
				{
					CMiniMapBlip *pBlip = blip[iIndex];
					if (pBlip && pBlip->IsComplex() && IsBlipIdInUse(pBlip->m_iUniqueId))
					{
						//Find blip associated with this entity
						if (pWaypointEntity == FindBlipEntity(pBlip))
						{
							//Entity found! Assign ObjectID and cache the blip ID so we can validate it later
							Waypoint.ObjectId = ObjectId;
							Waypoint.iAssociatedBlipId = pBlip->m_iUniqueId;
							break;
						}
						//Calculate closest blip
						else
						{
							const float BlipDistanceSq = (WaypointLocation - GetBlipPositionValue(pBlip)).Mag2();
							if (BlipDistanceSq < MinBlipDistanceSq)
							{
								MinBlipDistanceSq = BlipDistanceSq;
								ClosestBlip = pBlip->m_iUniqueId;
							}
						}
					}
				}
			}
		}
	}

	//Fall back to closest coordinate lookup
	if (Waypoint.iAssociatedBlipId == INVALID_BLIP_ID && ClosestBlip != INVALID_BLIP_ID)
	{
		Waypoint.iAssociatedBlipId = ClosestBlip;
	}

	eBLIP_DISPLAY_TYPE iDisplayType;
	if (bHideWaypointIcon)
	{
		iDisplayType = BLIP_DISPLAY_NEITHER;
	}
	else
	{
		iDisplayType = BLIP_DISPLAY_BLIPONLY;
	}

	CreateWaypointBlipAndRoute(Waypoint, iDisplayType, color);

	uiDebugf1("MiniMap: Waypoint added in slot %d for player %d", iWaypointToUse, Waypoint.iPlayerModelHash);

	if(bSetLocalDirty)
	{
		// update local dirty time
		ms_WaypointLocalDirtyTimestamp = NetworkInterface::IsNetworkOpen() ? NetworkInterface::GetNetworkTime() : sysTimer::GetSystemMsTime();
		MarkLocallyOwned(true);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SwitchOffWaypoint()
// PURPOSE:	switches off the waypoint blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SwitchOffWaypoint(bool bSetLocalDirty)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::SwitchOffWaypoint can only be called on the UpdateThread!");
		return;
	}

	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::SwitchOffWaypoint has been called before MiniMap has been fully initialised");
		return;
	}

#if __BANK
	sysStack::PrintStackTrace();
#endif // #if __BANK

	s32 iWaypointToUse = -1;

	if (NetworkInterface::IsGameInProgress())
	{
		iWaypointToUse = MULTIPLAYER_WAYPOINT_MARKER_ID;
	}
	else
	{
		u32 iCurrentPlayerModelHash = 0;
		CPed *pPlayerPed = FindPlayerPed();
		if (pPlayerPed)
		{
			iCurrentPlayerModelHash = pPlayerPed->GetPedModelInfo()->GetHashKey();
		}

		for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
		{
			if (ms_Waypoint[i].iPlayerModelHash == iCurrentPlayerModelHash && ms_Waypoint[i].iBlipIndex != INVALID_BLIP_ID)
			{
				iWaypointToUse = i;
				break;
			}
		}
	}

	if (iWaypointToUse != -1)
	{
		sWaypointStruct& Waypoint = ms_Waypoint[iWaypointToUse];
		ClearWaypointBlipAndRoute(Waypoint);

		Waypoint.iPlayerModelHash = 0;
		Waypoint.vPos = Vector2(0,0);

		uiDebugf1("MiniMap: Waypoint removed from slot %d for player %d", iWaypointToUse, Waypoint.iPlayerModelHash);
	}

	MarkLocallyOwned(false);

	if(bSetLocalDirty)
	{
		// update local dirty time
		ms_WaypointLocalDirtyTimestamp = NetworkInterface::IsNetworkOpen() ? NetworkInterface::GetNetworkTime() : sysTimer::GetSystemMsTime();
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::UpdateWaypoint()
// PURPOSE:	replaces the waypoint - removes any old waypoint and uses the one
//			current to the local player and creates a gps route for it
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::UpdateWaypoint()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::UpdateWaypoint can only be called on the UpdateThread!");
		return;
	}

	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::UpdateWaypoint has been called before MiniMap has been fully initialised");
		return;
	}

	s32 iWaypointToUse = -1;

	u32 iCurrentPlayerModelHash = 0;
	CPed *pPlayerPed = FindPlayerPed();
	if (pPlayerPed)
	{
		iCurrentPlayerModelHash = pPlayerPed->GetPedModelInfo()->GetHashKey();
	}

	//
	// get current waypoint in use
	//
	for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
	{
		if (ms_Waypoint[i].iBlipIndex != INVALID_BLIP_ID)
		{
			iWaypointToUse = i;
			break;
		}
	}

	//
	// remove any current waypoint blip and GPS:  (but leave any other info in place to reuse later)
	//
	if (iWaypointToUse != -1)
	{
		sWaypointStruct& Waypoint = ms_Waypoint[iWaypointToUse];

		// if the waypoint that is already valid is the correct one for the current player, then it doesnt need to
		// be refreshed (note if we refresh anyway this causes the blip tracking for taxi missions to get out of sync and end)
		// fixes bug 273305
		bool ClearWaypoint = Waypoint.iPlayerModelHash != iCurrentPlayerModelHash;

		//Validate waypoint object blip still exists
		if (Waypoint.ObjectId != NETWORK_INVALID_OBJECT_ID && !IsBlipIdInUse(Waypoint.iAssociatedBlipId))
		{
			uiDebugf1("MiniMap: Waypoint ObjectID has no associated blip");
			ClearWaypoint = true;
		}
		
		if (ClearWaypoint)
		{
			ClearWaypointBlipAndRoute(Waypoint);
			uiDebugf1("MiniMap: Waypoint cleared from slot %d for player %d", iWaypointToUse, Waypoint.iPlayerModelHash);
		}
		else //No refresh required
		{
			return;
		}
	}

	if (NetworkInterface::IsGameInProgress())  // we dont want to re-create the waypoint in MP, as it always uses the same slot (3).  This is the cause of 364289, which caused another bug (-1/INVALID in blip code) to happen in the minimap code.
	{
		return;
	}

	iWaypointToUse = -1;

	for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
	{
		if (ms_Waypoint[i].iPlayerModelHash == iCurrentPlayerModelHash)
		{
			iWaypointToUse = i;
			break;
		}
	}
	
	//
	// if a blip exists still which is valid for this player, then create it:
	//
	if (iWaypointToUse != -1)
	{
		if (ms_Waypoint[iWaypointToUse].iPlayerModelHash != 0)
		{
			CreateWaypointBlipAndRoute(ms_Waypoint[iWaypointToUse], BLIP_DISPLAY_BLIPONLY);

			uiDebugf1("MiniMap: Waypoint refreshed in slot %d for player %d", iWaypointToUse, ms_Waypoint[iWaypointToUse].iPlayerModelHash);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetActiveWaypointId()
// PURPOSE:	returns the ID of the active waypoint blip
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::IsAnyWaypointIdActive()
{
	for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
	{
		if (ms_Waypoint[i].iPlayerModelHash != 0)
		{
			return true;
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::IsPlayerWaypointActive()
// PURPOSE:	returns the ID of the active waypoint blip
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::IsPlayerWaypointActive(u32 iPlayerHash)
{
	for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
	{
		if (ms_Waypoint[i].iPlayerModelHash == iPlayerHash)
		{
			return true;
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetActiveWaypointId()
// PURPOSE:	returns the ID of the active waypoint blip
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::GetActiveWaypointId()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::GetActiveWaypointId can only be called on the UpdateThread!");
		return INVALID_BLIP_ID;
	}

	sWaypointStruct Waypoint;
	if (GetActiveWaypoint(Waypoint))
	{
		return Waypoint.iBlipIndex;
	}
	return INVALID_BLIP_ID;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetActiveWaypointPosition
// PURPOSE:	retrieves waypoint position
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::GetActiveWaypoint(sWaypointStruct& Waypoint)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CMiniMap::GetActiveWaypointPosition can only be called on the UpdateThread!");
		return false;
	}

	s32 iWaypointToUse = -1;

	u32 iCurrentPlayerModelHash = 0;
	CPed *pPlayerPed = FindPlayerPed();
	if (pPlayerPed)
	{
		iCurrentPlayerModelHash = pPlayerPed->GetPedModelInfo()->GetHashKey();
	}

	if (NetworkInterface::IsGameInProgress())
	{
		iWaypointToUse = MULTIPLAYER_WAYPOINT_MARKER_ID;
	}
	else
	{
		for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
		{
			if (ms_Waypoint[i].iPlayerModelHash == iCurrentPlayerModelHash)
			{
				iWaypointToUse = i;
				break;
			}
		}
	}

	if (iWaypointToUse != -1 && ms_Waypoint[iWaypointToUse].iBlipIndex != INVALID_BLIP_ID)
	{
		Waypoint = ms_Waypoint[iWaypointToUse];
		return true;
	}

	// no active waypoint
	return false;
}

bool CMiniMap::SetActiveWaypoint(const Vector2& vPosition, ObjectId ObjectId)
{
	s32 iWaypointToUse = -1;
	
	if (NetworkInterface::IsGameInProgress())
	{
		iWaypointToUse = MULTIPLAYER_WAYPOINT_MARKER_ID;
	}
	else
	{
		u32 iCurrentPlayerModelHash = 0;
		CPed *pPlayerPed = FindPlayerPed();
		if (pPlayerPed)
		{
			iCurrentPlayerModelHash = pPlayerPed->GetPedModelInfo()->GetHashKey();

			for (s32 i = 0; i < MAX_WAYPOINT_MARKERS; i++)
			{
				if (ms_Waypoint[i].iPlayerModelHash == iCurrentPlayerModelHash)
				{
					iWaypointToUse = i;
					break;
				}
			}
		}
	}
	if (iWaypointToUse != -1 && ms_Waypoint[iWaypointToUse].iBlipIndex != INVALID_BLIP_ID)
	{
		ms_Waypoint[iWaypointToUse].vPos = vPosition;
		ms_Waypoint[iWaypointToUse].ObjectId = ObjectId;

		CreateWaypointBlipAndRoute(ms_Waypoint[iWaypointToUse]);

		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetActiveWaypointPosition
// PURPOSE:	retrieves waypoint position
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::IsActiveWaypoint(Vector2 vPosition, ObjectId ObjectId)
{
	if(IsWaypointActive())
	{
		sWaypointStruct vCurrentWaypoint;
		GetActiveWaypoint(vCurrentWaypoint);

		bool bMatchingPosition = ABS(vPosition.x - vCurrentWaypoint.vPos.x) < VERY_SMALL_FLOAT && ABS(vPosition.y - vCurrentWaypoint.vPos.y) < VERY_SMALL_FLOAT;
		bool bMatchingObjectId = ObjectId == vCurrentWaypoint.ObjectId;
		if(bMatchingPosition && bMatchingObjectId)
		{
			return true; 
		}
	}

	return false;
}

void CMiniMap::SetEntityWaypointAllowed(bool Value)
{
	if (Value != ms_bEntityWaypointAllowed)
	{
		uiDisplayf("CMiniMap::SetEntityWaypointAllowed(%s)", Value? "true":"false");
		ms_bEntityWaypointAllowed = Value;
	}
}

void CMiniMap::SetWaypointClearOnArrivalMode(eWaypointClearOnArrivalMode Value)
{
	if (Value != ms_bWaypointClearOnArrivalMode)
	{
		uiDisplayf("CMiniMap::SetWaypointClearOnArrivalMode(%d)", (int)Value);
		ms_bWaypointClearOnArrivalMode = Value;
	}
}

void CMiniMap::SetWaypointGpsFlags(u16 Value)
{
	if (Value != ms_waypointGpsFlags)
	{
		uiDisplayf("CMiniMap::SetWaypointGPSFlags(%d)", Value);
		ms_waypointGpsFlags = Value;
	}
}

void CMiniMap::GetWaypointData(u32 WaypointIndex, u32 &PlayerHash, Vector2 &vPosition)
{
	if (uiVerifyf(WaypointIndex < MAX_WAYPOINT_MARKERS, "CMiniMap::GetWaypointData - waypoint index is too large"))
	{
		PlayerHash = ms_Waypoint[WaypointIndex].iPlayerModelHash;
		vPosition = ms_Waypoint[WaypointIndex].vPos;
	}
}

void CMiniMap::SetWaypointData(u32 WaypointIndex, u32 PlayerHash, const Vector2 &vPosition)
{
	if (uiVerifyf(WaypointIndex < MAX_WAYPOINT_MARKERS, "CMiniMap::SetWaypointData - waypoint index is too large"))
	{
		ms_Waypoint[WaypointIndex].iPlayerModelHash = PlayerHash;
		ms_Waypoint[WaypointIndex].vPos = vPosition;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::MarkLocallyOwned
// PURPOSE:	indicates whether the custom waypoint is locally owned or not
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::MarkLocallyOwned(bool isLocallyOwned)
{
	ms_bIsWaypointLocal = isLocallyOwned;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::IsWaypointLocallyOwned
// PURPOSE:	retrieves locally owned status
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap::IsWaypointLocallyOwned()
{
	return ms_bIsWaypointLocal;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetWaypointLocalDirtyTimestamp
// PURPOSE:	retrieves local dirty timestamp
/////////////////////////////////////////////////////////////////////////////////////
unsigned CMiniMap::GetWaypointLocalDirtyTimestamp()
{
	return ms_WaypointLocalDirtyTimestamp;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::SetFlashWantedOverlay
// PURPOSE:	because minimap now has faded out sides, we need to do the wanted flash
//			in code so we can colour the whole map instead of being limited to the
//			edge of the movie itself
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::SetFlashWantedOverlay(const bool bFlashOn)
{
	if(CScriptHud::bWantsToBlockWantedFlash)
	{
		ms_bFlashWantedOverlay = false;
	}
	else
	{
		ms_bFlashWantedOverlay = bFlashOn;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetNearestWantedCriminalActualBlipId
// PURPOSE:	goes through all wanted criminal blips and returns the one that is the
//			nearest to the player blip
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap::GetNearestWantedCriminalActualBlipId()
{
	s32 iNearestBlipId = -1;

	if (!CNetwork::IsGameInProgress())
		return iNearestBlipId;

	float fShortestDistance = (CMiniMap::sm_Tunables.Tiles.vMiniMapWorldSize.x + CMiniMap::sm_Tunables.Tiles.vMiniMapWorldSize.y);  // seems a good starting max value

	CMiniMapBlip *pPlayerBlip = GetBlipFromActualId(GetActualCentreBlipId());

	if (!pPlayerBlip)
		return iNearestBlipId;

	Vector3 vPlayerPos = GetBlipPositionValue(pPlayerBlip);

	for (s32 iIndex = 0; iIndex < iMaxCreatedBlips; iIndex++)
	{
		if (iIndex == iSimpleBlip)
			continue;

		CMiniMapBlip *pBlip = GetBlipFromActualId(iIndex);

		if (!pBlip)
			continue;

#define CRIM_BLIP_NAME ("radar_gang_wanted_")  // i need to make these exposed to code really instead of this hack, but it will do for now since the bug is well overdue!

		if (!strncmp(CRIM_BLIP_NAME, GetBlipObjectName(pBlip), NELEM(CRIM_BLIP_NAME)-1))
		{
			Vector3 vCriminalPos = GetBlipPositionValue(pBlip);

			Vector2 vTestForDistance(Vector2(vCriminalPos.x, vCriminalPos.y)-Vector2(vPlayerPos.x, vPlayerPos.y));
			float fDistance = vTestForDistance.Mag(); 

			//Displayf("DISTANCE TO CRIM BLIP IS %0.2f    (%0.2f)", fDistance, fShortestDistance);

			if (fDistance < fShortestDistance)
			{
				fShortestDistance = fDistance;
				iNearestBlipId = iIndex;
			}
		}
	}

	return iNearestBlipId;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::CheckIncomingFunctions
// PURPOSE:	checks for incoming functions from ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::CheckIncomingFunctions(atHashWithStringBank /*methodName*/, const GFxValue* args)
{
	if ((eSCALEFORM_BASE_CLASS)(s32)args[0].GetNumber() != SF_BASE_CLASS_MINIMAP)
		return;

	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::CheckIncomingFunctions has been called before MiniMap has been fully initialised");
		return;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetRevisedMiniMapRangeFromPercentage
// PURPOSE: returns the range of the radar based on the current minimap setting and
//			the value passed from script
/////////////////////////////////////////////////////////////////////////////////////
float CMiniMap::GetRevisedMiniMapRangeFromPercentage(float fPercentage)
{
#define ZOOM_RANGE (500.0f)

	float fNewRange = (ZOOM_RANGE / 100.0f) * fPercentage;

	if (fNewRange > ZOOM_RANGE)
		fNewRange = ZOOM_RANGE;

	if (fNewRange < 0.0f)
		fNewRange = 0.0f;

	fNewRange = ((ZOOM_RANGE+1)-fNewRange);

	if (IsInBigMap())
	{
		fNewRange *= __EXTRA_BIGMAP_SCALER;  // scale for bug 1624415
	}

	return fNewRange;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetRevisedMiniMapRangeFromScript
// PURPOSE: returns the range of the radar based on the current minimap setting and
//			the value passed from script
/////////////////////////////////////////////////////////////////////////////////////
float CMiniMap::GetRevisedMiniMapRangeFromScript(float fCurrentRange, s32 iScriptZoomValue)
{
	float fRange = fCurrentRange;

	if (iScriptZoomValue < 100)
	{
		return fRange;
	}

	if (IsInBigMap())
	{
		if (iScriptZoomValue <= MAX_SCRIPT_SCALE_AMOUNT_BIGMAP)
		{
			float fNewScriptRange = (float)MAX_SCRIPT_SCALE_AMOUNT_BIGMAP - (float)iScriptZoomValue;
			float fMinAmount = 1.0f;

			fNewScriptRange = ( (fNewScriptRange / ((float)MAX_SCRIPT_SCALE_AMOUNT_BIGMAP * 5.0f)) * (40.0f-fMinAmount) ) + fMinAmount;
			fRange = (fNewScriptRange * 0.64f);  // minimap has changed size so instead of script re-adjusting their values, we rescale here

			fRange *= __EXTRA_BIGMAP_SCALER;  // scale for bug 1624415
		}
	}
	else
	{
		if (iScriptZoomValue <= MAX_SCRIPT_SCALE_AMOUNT_STANDARD)
		{
			float fNewScriptRange = (float)MAX_SCRIPT_SCALE_AMOUNT_STANDARD - (float)iScriptZoomValue;
			float fMinAmount = 7.0f;

			fNewScriptRange = ( (fNewScriptRange / (float)MAX_SCRIPT_SCALE_AMOUNT_STANDARD) * (200.0f-fMinAmount) ) + fMinAmount;
			fRange = fNewScriptRange;
		}

		if (IsInGolfMap())
		{
			fRange *= 0.62f;  // minimap has changed size so instead of script re-adjusting their values, we rescale here
		}
	}

	return fRange;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::GetRange
// PURPOSE: returns the range of the radar as it should be based on player speed etc
/////////////////////////////////////////////////////////////////////////////////////
float CMiniMap::GetRange(bool bGetTrueRange)
{
	// Use adjusted values when the player is wanted
	CPed *pLocalPlayer = GetMiniMapPed();
	const bool bWanted = ( pLocalPlayer && pLocalPlayer->GetPlayerWanted() && pLocalPlayer->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN );
	const float fVehicleStaticZoom = bWanted ? sm_Tunables.Camera.fVehicleStaticWantedZoom: sm_Tunables.Camera.fVehicleStaticZoom;
	const float fVehicleMovingZoom = bWanted ? sm_Tunables.Camera.fVehicleMovingWantedZoom: sm_Tunables.Camera.fVehicleMovingZoom;
	const float fExteriorZoom = bWanted ? sm_Tunables.Camera.fExteriorFootZoomWanted:  sm_Tunables.Camera.fExteriorFootZoom;
//	const float fExteriorZoomRunning = bWanted ? sm_Tunables.Camera.fExteriorFootZoomWantedRunning:  sm_Tunables.Camera.fExteriorFootZoomRunning;

	float fRange = fExteriorZoom;

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMiniMap::GetRange can only be called on the UpdateThread!");
		return fRange;
	}

	if (!IsInBigMap())
	{
		if ( (GetIsInsideInterior()) && (pLocalPlayer && !pLocalPlayer->IsInOrAttachedToCar()) )
		{
			if(CScriptHud::ms_bUseVerySmallInteriorZoom)
			{
				fRange = sm_Tunables.Camera.fInteriorVerySmall;
			}
			else if(CScriptHud::ms_bUseVeryLargeInteriorZoom)
			{
				fRange = sm_Tunables.Camera.fInteriorVeryLarge;
			}
			else
			{
				fRange = sm_Tunables.Camera.fInteriorFootZoom;
			}
		}
		else
		{
			float fVehicleSpeed = GetSpeedOfPlayersVehicle();

			if (fVehicleSpeed != 0.0f)
			{
				fRange = fVehicleStaticZoom - (fVehicleSpeed * sm_Tunables.Camera.fVehicleSpeedZoomScalar);
				fRange = rage::Max(fRange, fVehicleMovingZoom); 
			}
			else
			{
				const CVehicle *pVehicle = CGameWorld::FindLocalPlayerVehicle();

				if(pVehicle)
				{
					// B* 526555: When in a car that someone else is driving, the speed is assumed to be 0, but lets force the minimap to be bigger,
					// so that the player can actually see stuff.
					if(pVehicle->IsNetworkClone())
					{
						fRange = fVehicleMovingZoom;
					}
					else // If not a network clone, then we're actually idle.
					{
						fRange = fVehicleStaticZoom;
					}
				}
				else if(CGameWorld::FindLocalPlayerMount())
				{
					fRange = fVehicleStaticZoom;
				}
				else if (IsPlayerUsingParachute())
				{
					fRange = sm_Tunables.Camera.fParachutingZoom;
				}
				else
				{
					fRange = fExteriorZoom;
				}
			}
/*			// not wanted now 1270882
			if (fRange == fExteriorZoom)  // no scaling when in an interior for 784769
			{
				if (IsPlayerRunning())
				{
					fRange = fExteriorZoomRunning;
				}
				}*/
		}

		if (pLocalPlayer && pLocalPlayer->IsUsingActionMode())
		{
			fRange *= 1.2f;  // for 1130713
		}
	}
	else
	{
		if (ms_bBigMapFullZoom)
		{
			fRange = BIGMAP_SCALE_FULL;
		}
		else
		{
			fRange = BIGMAP_SCALE;
		}
	}

	bool bZoomForced = false;

	if (CScriptHud::ms_fRadarZoomDistanceThisFrame != 0.0f)
	{
		float fScaler = ( 100.0f / (CScriptHud::ms_fRadarZoomDistanceThisFrame * 2.0f) );

		fRange = 100.0f * fScaler;

		return fRange;  // return here so we use whatever distance script has set, regardless of any other options - its capped earlier
	}
	else
	{
	//
	// script can set the value of the zoom between 100 and MAX_SCRIPT_SCALE_AMOUNT.
	//
		if (CScriptHud::ms_fMiniMapForcedZoomPercentage != 0.0f)
		{
			fRange = GetRevisedMiniMapRangeFromPercentage(CScriptHud::ms_fMiniMapForcedZoomPercentage);
			bZoomForced = true;
		}
		else if ( (CScriptHud::ms_iRadarZoomValue != 0) && (!ms_bBigMapFullZoom) )  // fix for 510690
		{
			fRange = GetRevisedMiniMapRangeFromScript(fRange, CScriptHud::ms_iRadarZoomValue);
			bZoomForced = true;
		}
	}

	// apply debug widget range
#if __BANK
	if (ms_fDebugRange != 2.0f)
		fRange = ms_fDebugRange;
#endif // __BANK

	if (!IsInBigMap())
	{
		if (!bZoomForced)
		{
			// cap:
			fRange = rage::Max(fRange, MIN_MINIMAP_RANGE);
			fRange = rage::Min(fRange, MAX_MINIMAP_RANGE);
		}

		if (IsPlayerInPlaneOrHelicopter())
		{
			fRange /= 1.8f;  // zoom out a little extra when in a plane (bug 1121704)
		}

		if ( (!bGetTrueRange) && (fwTimer::GetTimeInMilliseconds() <= iMiniMapDpadZoomTimer+TIME_TO_ZOOM_OUT_MINIMAP) )
		{	
			if (IsPlayerInPlaneOrHelicopter())
			{
				fRange /= sm_Tunables.Camera.fRangeZoomedScalarPlane;
			}
			else
			{
				fRange /= sm_Tunables.Camera.fRangeZoomedScalarStandard;
			}
		}

#if !__FINAL
		if (camInterface::GetDebugDirector().IsFreeCamActive() BANK_ONLY( DEBUG_DRAW_ONLY( && !CMiniMap_RenderThread::sm_bDrawCollisionVolumes)))
		{
			if (!GetIsInsideInterior())
			{
				fRange /= 1.8f;
			}
		}
#endif // __FINAL
	}
	else
	{
		if ( (ms_bBigMapFullZoom) && (!bZoomForced) )
		{
			fRange = rage::Max(fRange, BIGMAP_SCALE_FULL);
		}
	}

	return fRange;
}


void CMiniMap::UpdateBitmapState( sBitmapStruct& bitmap, bool bStreamThisFrame, bool bDrawThisFrame, const char * cBitmapString)
{
	switch (bitmap.iState)
	{
	case MM_BITMAP_NONE:
		{
			if (bStreamThisFrame)
			{
				strLocalIndex iTxdId = g_TxdStore.FindSlot(cBitmapString);

				if (iTxdId != -1)
				{
					g_TxdStore.StreamingRequest(iTxdId, (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE));

					bitmap.iTxdId = iTxdId;
					bitmap.iState = MM_BITMAP_LOADING;
				}
			}
			break;
		}

	case MM_BITMAP_LOADING:
		{
			if (uiVerifyf(bitmap.iTxdId != -1, "txd id is -1"))
			{
				if (g_TxdStore.HasObjectLoaded(bitmap.iTxdId))
				{
					g_TxdStore.AddRef(bitmap.iTxdId, REF_OTHER);
					g_TxdStore.ClearRequiredFlag(bitmap.iTxdId.Get(), STRFLAG_DONTDELETE);

					bitmap.iState = MM_BITMAP_ATTACH;
				}
			}
			break;
		}

	case MM_BITMAP_ACTIVE:
		{
			if (!bStreamThisFrame)
			{
				bitmap.iState = MM_BITMAP_DETACH;
			}
			break;
		}

	case MM_BITMAP_SHUTDOWN:
		{
			strLocalIndex iTxdId = bitmap.iTxdId;
			if ( iTxdId != -1)
			{
				g_TxdStore.AddRef(iTxdId, REF_RENDER); // Make sure the reference sticks around for a few more frames
				gDrawListMgr->AddRefCountedModuleIndex(iTxdId.Get(), &g_TxdStore); 
				g_TxdStore.RemoveRef(iTxdId, REF_OTHER);
			}

			bitmap.iTxdId = -1;
			bitmap.iState = MM_BITMAP_NONE;

			break;
		}

	case MM_BITMAP_ATTACH:
		{
			bitmap.iState = MM_BITMAP_ACTIVE;

			break;
		}

	case MM_BITMAP_DETACH:
		{
			bitmap.iState = MM_BITMAP_SHUTDOWN;
			break;
		}

	default:
		{
			// sweet F.A.
			break;
		}
	}

	if (bDrawThisFrame && (bitmap.iState == MM_BITMAP_ACTIVE ||
		bitmap.iState == MM_BITMAP_ATTACH))
	{
		bitmap.iDrawIdx.GetWriteBuf() = bitmap.iTxdId.Get();
	}
	else
	{
		bitmap.iDrawIdx.GetWriteBuf() = -1;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// __BANK - all debug stuff shown appear under here
/////////////////////////////////////////////////////////////////////////////////////
#if __BANK

void CalculateFogOfWarMinMaxXAndY()
{
	u8 ignoreValue = 0;
	if (bIgnoreFogOfWarValueOf255)
	{
		ignoreValue = 255;
	}

	fogOfWarMinX = FOG_OF_WAR_RT_SIZE;
	fogOfWarMinY = FOG_OF_WAR_RT_SIZE;
	fogOfWarMaxX = -1;
	fogOfWarMaxY = -1;

	u8 *FoW = CMiniMap_Common::GetFogOfWarLockPtr();
	if(FoW)
	{
		for (s32 row = 0; row < FOG_OF_WAR_RT_SIZE; row++)
		{
			for (s32 column = 0; column < FOG_OF_WAR_RT_SIZE; column++)
			{
				if (FoW[(row*FOG_OF_WAR_RT_SIZE)+column] != ignoreValue)
				{
					if (column < fogOfWarMinX) { fogOfWarMinX = column; }
					if (column > fogOfWarMaxX) { fogOfWarMaxX = column; }
					if (row < fogOfWarMinY) { fogOfWarMinY = row; }
					if (row > fogOfWarMaxY) { fogOfWarMaxY = row; }
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::InitWidgets()
// PURPOSE: inits the ui bank widget and "Create" button
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (!pBank)  // create the bank if not found
	{
		pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
	}

	if (pBank)
	{
		pBank->AddButton("Create MiniMap widgets", &CreateBankWidgets);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::CreateBankWidgets()
// PURPOSE: creates the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::CreateBankWidgets()
{
	static bool bBankCreated = false;

	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if ((!bBankCreated) && (bank))
	{
		bank->PushGroup("MiniMap");

			bank->AddButton("Set Bigmap Active", &SetBigMapActiveDebug);
			//bank->AddToggle("Bigmap Full Zoom", &ms_bBigMapFullZoom);
			bank->AddButton("Reload XML", &ReloadXmlData);
			bank->AddButton("Flash Minimap", datCallback(CFA1(CMiniMap::FlashMiniMapDisplay), reinterpret_cast<CallbackData>(HUD_COLOUR_WHITE) ) );
			bank->AddToggle("Display Masks (requires toggling map view)", &CMiniMap_RenderThread::sm_bDebugMaskDisplay);
			bank->AddSlider("Mask Display Alpha", &CMiniMap_RenderThread::sm_uDebugMaskAlpha, 0, 100, 5);
			bank->AddSeparator();
			bank->AddSlider("Multihead Feather Pct", &CMiniMap_RenderThread::sm_fMultiheadEdgeFeatherPct, 0.f, 1.0f, 0.001f);
			bank->AddToggle("Draw map sizes (not all drawn are used)", &s_bRenderMapPositions);

			for (s32 i = 0; i < MAX_MINIMAP_VALUES; i++)
			{
				bank->PushGroup(strupr(sm_miniMaps[i].m_fileName));
					bank->AddTitle("'L'=left, 'R'=right, 'C'=center or 'I'=ignore");
					bank->AddText("AlignX", (char*)&sm_miniMaps[i].m_alignX, 2);
					bank->AddTitle("'T'=top, 'B'=bottom, 'C'=center or 'I'=ignore");
					bank->AddText("AlignY", (char*)&sm_miniMaps[i].m_alignY, 2);
					bank->AddSlider("Position", &sm_miniMaps[i].m_position, -1.0f, 1.0f, 0.001f, &UpdateMiniMapDebug);
					bank->AddSlider("Size", &sm_miniMaps[i].m_size, 0.0f, 1.0f, 0.001f, &UpdateMiniMapDebug);
				bank->PopGroup();
			}

			bank->AddSeparator();
			bank->PushGroup("Waypoints");
				bank->AddToggle("Entity Waypoint Allowed", &ms_bEntityWaypointAllowed);
				bank->AddToggle("GPS_FLAG_IGNORE_ONE_WAY", &ms_waypointGpsFlags, GPS_FLAG_IGNORE_ONE_WAY);
				bank->AddToggle("GPS_FLAG_FOLLOW_RULES", &ms_waypointGpsFlags, GPS_FLAG_FOLLOW_RULES);
				bank->AddToggle("GPS_FLAG_AVOID_HIGHWAY", &ms_waypointGpsFlags, GPS_FLAG_AVOID_HIGHWAY);
				bank->AddToggle("GPS_FLAG_NO_ROUTE_SHIFT", &ms_waypointGpsFlags, GPS_FLAG_NO_ROUTE_SHIFT);
				bank->AddToggle("GPS_FLAG_NO_PULL_PATH_TO_RIGHT_LANE", &ms_waypointGpsFlags, GPS_FLAG_NO_PULL_PATH_TO_RIGHT_LANE);
				bank->AddToggle("GPS_FLAG_AVOID_OFF_ROAD", &ms_waypointGpsFlags, GPS_FLAG_AVOID_OFF_ROAD);
				bank->AddToggle("GPS_FLAG_IGNORE_DESTINATION_Z", &ms_waypointGpsFlags, GPS_FLAG_IGNORE_DESTINATION_Z);
			bank->PopGroup();

			bank->AddSeparator();
			bank->PushGroup("Pause Map");
			{
				bank->AddSlider("Pause Menu Map Scale", &ms_fPauseMenuMapScale, 0, 10000.0, 2.0f);
				bank->AddSlider("Pause Menu Cursor", &ms_vPauseMenuMapCursor, -10000.0f, 10000.0, 50.0f);
			}
			bank->PopGroup();


			bank->AddButton("Create/Remove Debug Blip", &CreateRemoveDebugBlip);
			bank->AddToggle("CullDistant Blips", &ms_bCullDistantBlips);
			bank->AddToggle("Display Blip Vector Map", &ms_bShowBlipVectorMap);	

			bank->AddSlider("MiniMap Zoom Forced Percentage", &CScriptHud::ms_fMiniMapForcedZoomPercentage, -100.0f, 100.0f, 0.01f);
			
			bank->AddSlider("MiniMap Distance Zoom", &s_fRadarZoomDistanceThisFrameWidget, 0.0f, 5000.0f, 1.0f);
			bank->AddToggle("Zoom To Active Waypoint Blip", &s_bZoomToWaypointBlip);

 			bank->AddSlider("Tilt adjuster", &fDebugTiltValue, -1.0f, 140.0f, 1.0f);
			bank->AddSlider("Offset adjuster", &fDebugOffsetValue, -1.0f, 300.0f, 5.0f);
			bank->AddSlider("Debug Minimap Zoom", &ms_fDebugRange, 2.0f, 500.0f, 0.001f);
			bank->AddSlider("Script Minimap Zoom", &CScriptHud::ms_iRadarZoomValue, 0, MAX_SCRIPT_SCALE_AMOUNT_BIGMAP, 1);
			
			bank->AddToggle("Output current interior info to log", &bDisplayInteriorInfoToLog);
 			bank->AddSlider("Select Interior", &ms_iDebugInteriorHash, -MAX_INT32, MAX_INT32, 1);
			bank->AddSlider("Select Interior Level", &ms_iDebugInteriorLevel, sMiniMapInterior::INVALID_LEVEL, 50, 1);
			bank->AddSlider("Debug Golf Course", &ms_iDebugGolfCourseMap, -1, 10, 1);
			bank->AddToggle("Always Draw Sonar Blips", &bDebugAlwaysDrawSonarBlips);
			bank->AddToggle("Debug visibility of Sonar Blips", &bDebugVisibilityOfSonarBlips);
			bank->AddToggle("Debug draw ped range for sonar blips", &bDebugDrawSonarBlipRange);
			bank->AddToggle("Use weapon range for MP weapon sonar", &CMiniMap::ms_bUseWeaponRangeForMPSonarDistance);
			bank->AddSeparator();

			bank->AddToggle("Render Background Movie", &ms_bRenderMiniMapBackground);
			bank->AddToggle("Render Foreground Movie", &ms_bRenderMiniMapForeground);

			bank->AddToggle("Use Textured Alpha On All Movies", &CMiniMap_RenderThread::ms_bUseTexturedAlphaAllMovies);
			bank->AddToggle("Use Textured Alpha On Background Movie", &CMiniMap_RenderThread::ms_bUseTextureAlphaBaseMovie);
	
			bank->AddSeparator();

			bank->AddSlider("Locked MiniMap Pos", &ms_vLockedMiniMapPosition, -9999.0f, 9999.0f, 1.0f);
			bank->AddSlider("Locked MiniMap Angle", &ms_iLockedMiniMapAngle, -1, 360, 1);

			bank->AddSeparator();

			bank->PushGroup("Altimeter");
			{
				bank->AddToggle("New Awesome Mode", &ms_bUseNewAwesomeAltimeter);
				bank->AddSlider("Script: Fake Ceiling", &CScriptHud::fFakeMinimapAltimeterHeight, -1000.0f, 2000.0f, 5.0f);
				bank->AddToggle("Script: Use Colour", &CScriptHud::ms_bColourMinimapAltimeter);
				bank->AddSlider("Altimeter Tick Movie Height", &ALTIMETER_TICK_PIXEL_HEIGHT, -1000.0f, 1000.0f, 1.0f);
				bank->AddSlider("Altimeter Ground Level", &ALTIMETER_GROUND_LEVEL_FUDGE_FACTOR, -1000.0f, 1000.0f, 0.5f);
				bank->AddSlider("Height Map Wiggle Room", &ALTIMETER_HEIGHTMAP_WIGGLE_ROOM, -100.0f, 100.0f, 5.0f);
				bank->PopGroup();
			}

			bank->AddSeparator();
			
			bank->AddToggle("Hide Background Map", &ms_bHideBackgroundMap);
			bank->AddToggle("Block waypoint", &ms_bBlockWaypoint);
			bank->AddToggle("Prologue Map", &ms_bInPrologue);
			bank->AddToggle("Island X Map", &ms_bOnIslandMap);
//			bank->AddToggle("Debug 3D Blips", &bDebug3DBlips);

			bank->AddSeparator();
			bank->AddToggle("No UI AA on Minimap", &noUIAA);
			bank->AddToggle("No UI AA on Bitmap Minimap", &noUIAAOnBitMap);

			bank->AddSeparator();

			bank->AddSlider("Tiles MiniMap", &s_fDebugForceTilesAroundPlayer, -2.0f, 1000.0f, 1.0f);
			bank->AddSlider("Tiles PauseMap", &s_vDebugForceTilesAroundPlayer, -1.0f, 1000.0f, 1.0f);

			bank->AddToggle("Debug draw tiles", &ms_bDebugDrawTiles);
			bank->AddToggle("Debug draw area/radius blip debug volumes", &CMiniMap_RenderThread::sm_bDrawCollisionVolumes);

			bank->AddSeparator();

//			bank->AddSlider("Square Limit Range", &vMax2dRadarDistanceSquareTest, 0.0f, 1.0f, 0.001f);

			bank->AddToggle("Old Stealth Mode", &ms_bInStealthMode);
			bank->AddSlider("Sonar Style", &iDebugSonarStyle, 0, MAX_SONAR_STYLES, 1);
			bank->AddToggle("All ped blips as stealth blips", &bAllPedBlipsAsStealth, &ToggleAllPedToStealth);


			bank->AddToggle("Override Cone Colour", &ms_bOverrideConeColour);
			bank->AddSlider("Cone Colour Red", &ms_ConeColourRed, 0, 255, 1);
			bank->AddSlider("Cone Colour Green", &ms_ConeColourGreen, 0, 255, 1);
			bank->AddSlider("Cone Colour Blue", &ms_ConeColourBlue, 0, 255, 1);
			bank->AddSlider("Cone Colour Alpha", &ms_ConeColourAlpha, 0, 100, 1);

			bank->AddSlider("Cone Focus Range Multiplier", &ms_fConeFocusRangeMultiplier, 0.5f, 2.0f, 0.01f);
			bank->AddToggle("Draw Actual Range Cone", &ms_bDrawActualRangeCone);

#if ENABLE_FOG_OF_WAR
			bank->PushGroup("FoW");
			{
				bank->AddToggle("Display reveal ratio",&displayRevealFoWRatio);
				bank->AddToggle("Test reveal Coord",&testRevealFoWCoord);
				bank->AddSlider("Test reveal Coord X",&testRevealX, -10000.0f, 10000.0f, 1.0f);
				bank->AddSlider("Test reveal Coord X",&testRevealY, -10000.0f, 10000.0f, 1.0f);
				bank->AddToggle("Reveal Map",&ms_bRequestRevealFoW);
				bank->AddToggle("Debug Draw Fog Of War Map", &DebugDrawFOW);
				bank->AddSlider("Debug Draw X1", &DebugFOWX1, 0.0f, 1.0f, 0.1f);
				bank->AddSlider("Debug Draw Y1", &DebugFOWY1, 0.0f, 1.0f, 0.1f);
				bank->AddSlider("Debug Draw X2", &DebugFOWX2, 0.0f, 1.0f, 0.1f);
				bank->AddSlider("Debug Draw Y2", &DebugFOWY2, 0.0f, 1.0f, 0.1f);

				bank->AddButton("Calculate Fog of War Min/Max X/Y", CalculateFogOfWarMinMaxXAndY);
				bank->AddToggle("Ignore FoW value of 255", &bIgnoreFogOfWarValueOf255);

				bank->AddText("fogOfWarMinX",&fogOfWarMinX,true);
				bank->AddText("fogOfWarMinY",&fogOfWarMinY,true);
				bank->AddText("fogOfWarMaxX",&fogOfWarMaxX,true);
				bank->AddText("fogOfWarMaxY",&fogOfWarMaxY,true);
			}

			CMiniMap_Common::CreateWidgets(bank);

			bank->PopGroup();
#endif // ENABLE_FOG_OF_WAR
		bank->PopGroup();

		bBankCreated = true;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ShutdownWidgets()
// PURPOSE: removes the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::ShutdownWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (pBank)
	{
		pBank->Destroy();
	}
}



void CMiniMap::SetBigMapActiveDebug()
{
	// revised this debug to match script command (CommandSetBigMapActive)
	bool bActive = !CMiniMap::WasInBigMap();

	if (sMiniMapMenuComponent.IsActive() ||
		SocialClubMenu::IsActive() ||
		cStoreScreenMgr::IsStoreMenuOpen())
	{
		return;
	}

	if (bActive)
	{
		if (!CMiniMap::IsInBigMap())
		{
			if (!CMiniMap::IsInPauseMap())
			{
				CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP);
			}
		}

		CMiniMap::SetBigMapFullZoom(false);
	}
	else
	{
		if (CMiniMap::WasInBigMap())  // want to check it regardless of whether in the pausemenu or not
		{
			if (!CPauseMenu::IsActive())
			{
				CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP);
			}
		}
	}
};



void CMiniMap::CreateRemoveDebugBlip()
{
	if (ms_iDebugBlip == INVALID_BLIP_ID)
	{
		ms_iDebugBlip = CMiniMap::CreateBlip(true, BLIP_TYPE_COORDS, Vector3(10,10, 10), BLIP_DISPLAY_BOTH, "test debug blip");
		SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, ms_iDebugBlip, BLIP_POLICE_PLANE_MOVE);
		SetBlipParameter(BLIP_CHANGE_NAME_FROM_ASCII, ms_iDebugBlip, "FKNG_DEBUG_BLIP");
	}
	else
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(ms_iDebugBlip);

		if (pBlip)
		{
			RemoveBlip(pBlip);
			ms_iDebugBlip = INVALID_BLIP_ID;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ReloadXmlData()
// PURPOSE:	reloads the xml and updates the values so we see the changes instantly
//			in game
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::ReloadXmlData()
{
	LoadXmlData(true);

	UpdateMiniMapDebug();
}



void CMiniMap::UpdateMiniMapDebug()
{
	// decide in what we set the new changes:
	if (IsInPauseMap())
	{
		SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP);
	}
	else if (IsInCustomMap())
	{
		SetMinimapModeState( MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP);
	}
	else if (IsInBigMap())
	{
		SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP);
	}
	else if (IsInGolfMap())
	{
		SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP);
	}
	else
	{
		SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ToggleAllPedToStealth()
// PURPOSE: debug function to toggle all normal ped blips into stealth blips
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::ToggleAllPedToStealth()
{
	if (!ms_bXmlDataHasBeenRead)
	{
		uiAssertf(0, "Graeme - CMiniMap::ToggleAllPedToStealth has been called before MiniMap has been fully initialised");
		return;
	}

	for (s32 iIndex = 0; iIndex < iMaxCreatedBlips; iIndex++)
	{
		if (iIndex == iSimpleBlip)
			continue;

		CMiniMapBlip *pBlip = GetBlipFromActualId(iIndex);

		if (!pBlip)
			continue;

		if (bAllPedBlipsAsStealth)
		{
			if (GetBlipTypeValue(pBlip) == BLIP_TYPE_CHAR)
			{
				SetBlipTypeValue(pBlip, BLIP_TYPE_STEALTH);
			}
		}
		else
		{
			if (GetBlipTypeValue(pBlip) == BLIP_TYPE_STEALTH)
			{
				SetBlipTypeValue(pBlip, BLIP_TYPE_CHAR);
			}
		}
	}

	if (bAllPedBlipsAsStealth)
	{
		uiDisplayf("MiniMap: ALL PED BLIPS ARE NOW STEALTH BLIPS");
	}
	else
	{
		uiDisplayf("MiniMap: ALL STEALTH BLIPS ARE NOW PED BLIPS");
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap::ModifyLabelsToAllActiveBlips()
// PURPOSE:	switches on/off labels for all active blips
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap::ModifyLabelsToAllActiveBlips(bool bSwitchOn, bool bDebugNumbersOnly)
{
	for (s32 iId = 0; iId < iMaxCreatedBlips; iId++)
	{
		if (iId == iSimpleBlip)
			continue;

		CMiniMapBlip *pBlip = GetBlipFromActualId(iId);

		if (!pBlip)
			continue;

		if ( (bDebugNumbersOnly) && (CMiniMap::GetBlipDebugNumberValue(pBlip) == -1) )
		{
			continue;
		}

		if (bSwitchOn)
			SetFlag(pBlip, BLIP_FLAG_VALUE_SET_LABEL);  // on
		else
			SetFlag(pBlip, BLIP_FLAG_VALUE_REMOVE_LABEL);  // off
	}
}


#endif  // __BANK

// eof
