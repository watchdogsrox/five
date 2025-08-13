/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MiniMap.h
// PURPOSE : manages the Scaleform radar code
// AUTHOR  : Derek Payne
// STARTED : 18/08/2010
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef __MINIMAP_H__
#define __MINIMAP_H__

// rage
#include "grcore/stateblock.h"
#include "grcore/viewport.h"
#include "Scaleform/scaleform.h"

// game
#include "Frontend/MiniMapCommon.h"
#include "Frontend/MinimapMenuComponent.h"
#include "Frontend/MinimapTunables.h"
#include "frontend/PauseMenu.h"
#include "Task/System/Tuning.h"
#include "text/TextFile.h"
#include "vehicleAi/pathfind.h"


enum eMINIMAP_VALUES
{
	MINIMAP_VALUE_MINIMAP = 0,
	MINIMAP_VALUE_MINIMAP_MASK,
	MINIMAP_VALUE_MINIMAP_BLUR,
	MINIMAP_VALUE_BIGMAP,
	MINIMAP_VALUE_BIGMAP_MASK,
	MINIMAP_VALUE_BIGMAP_BLUR,
	MINIMAP_VALUE_PAUSEMAP,
	MINIMAP_VALUE_PAUSEMAP_MASK,
	MINIMAP_VALUE_CUSTOM,
	MINIMAP_VALUE_CUSTOM_MASK,
	MINIMAP_VALUE_GOLF_COURSE,

	MAX_MINIMAP_VALUES
};



enum
{
	MINIMAP_COMPONENT_RUNWAY_1 = 0,		//city airport
	MINIMAP_COMPONENT_RUNWAY_2,       //countryside
	MINIMAP_COMPONENT_RUNWAY_3,       //countryside small
	MINIMAP_COMPONENT_RUNWAY_4,       //military
	MINIMAP_COMPONENT_PRISON_GROUNDS, //prison interior	
	MINIMAP_COMPONENT_DEBUG_MAP_AREAS, //debug coloured map areas
	MINIMAP_COMPONENT_KING_HILL_1,
	MINIMAP_COMPONENT_KING_HILL_2,
	MINIMAP_COMPONENT_KING_HILL_3,
	MINIMAP_COMPONENT_KING_HILL_4,
	MINIMAP_COMPONENT_KING_HILL_5,
	MINIMAP_COMPONENT_KING_HILL_6,
	MINIMAP_COMPONENT_KING_HILL_7,
	MINIMAP_COMPONENT_KING_HILL_8,
	MINIMAP_COMPONENT_KING_HILL_9,
	MINIMAP_COMPONENT_KING_HILL_10,
	MINIMAP_COMPONENT_KING_HILL_11,
	MINIMAP_COMPONENT_KING_HILL_12,
	MINIMAP_COMPONENT_KING_HILL_13,
	MAX_MINIMAP_COMPONENTS
};



// flags so we know what param to change:
enum eBLIP_PARAMS
{
	BLIP_CHANGE_SCALE = 0,
	BLIP_CHANGE_DISPLAY,
	BLIP_CHANGE_POSITION,
	BLIP_CHANGE_OBJECT_NAME, // internally now referred to by the Linkage, but left this way for the interface so we don't have to change all the files
	BLIP_CHANGE_SPRITE,   // need to support this until old radar is removed
	BLIP_CHANGE_DELETE,
	BLIP_CHANGE_COLOUR,
	BLIP_CHANGE_COLOUR32,
	BLIP_CHANGE_ALPHA,
	BLIP_CHANGE_FLASH_DURATION,
	BLIP_CHANGE_FLASH_INTERVAL,
	BLIP_CHANGE_FRIENDLY,
	BLIP_CHANGE_ROUTE,
	BLIP_CHANGE_BRIGHTNESS,
	BLIP_CHANGE_SHOW_HEIGHT,
	BLIP_CHANGE_SHOW_NUMBER,
	BLIP_CHANGE_SHOW_CONE,
	BLIP_CHANGE_FLASH,
	BLIP_CHANGE_FLASH_ALT,
	BLIP_CHANGE_SHORTRANGE,
	BLIP_CHANGE_MARKER_LONG_DISTANCE,
	BLIP_CHANGE_PRIORITY,
	BLIP_CHANGE_DIRECTION,
	BLIP_CHANGE_NAME,
	BLIP_CHANGE_NAME_FROM_ASCII,
	BLIP_CHANGE_RADIUS_EDGE,
	BLIP_CHANGE_AREA_EDGE,
	BLIP_CHANGE_COLOUR_ROUTE,
	BLIP_CHANGE_MINIMISE_ON_EDGE,
	BLIP_CHANGE_EXTENDED_HEIGHT_THRESHOLD,
	BLIP_CHANGE_PULSE,
	BLIP_CHANGE_MISSION_CREATOR,
	BLIP_CHANGE_SHOW_TICK,
	BLIP_CHANGE_SHOW_TICK_GOLD,
	BLIP_CHANGE_SHOW_FOR_SALE,
	BLIP_CHANGE_SHOW_HEADING_INDICATOR,
	BLIP_CHANGE_SHOW_OUTLINE_INDICATOR,
	BLIP_CHANGE_SHOW_FRIEND_INDICATOR,
	BLIP_CHANGE_SHOW_CREW_INDICATOR,
	BLIP_CHANGE_CATEGORY,
	BLIP_CHANGE_DEBUG_NUMBER,
	BLIP_CHANGE_HIDDEN_ON_LEGEND,
	BLIP_CHANGE_HIGH_DETAIL,
	BLIP_CHANGE_HEIGHT_ON_EDGE,
	BLIP_CHANGE_HOVERED_ON_PAUSEMAP,
	BLIP_CHANGE_SHORT_HEIGHT_THRESHOLD,
	MAX_BLIP_PARAMS
};

enum
{
	MINIMAP_MOVIE_BACKGROUND = 0,
	MINIMAP_MOVIE_FOREGROUND,
	MAX_MINIMAP_MOVIE_LAYERS
};


#define __EXTRA_BIGMAP_SCALER		(1.1f)  // for bug 1624415 - 10%
#define BIGMAP_SCALE_FULL			(1.056f)
#define BIGMAP_SCALE				(7.04f * __EXTRA_BIGMAP_SCALER)  // we scale for bug 1624415

#define BIGMAP_OFFSET_X				(295.94f)
#define BIGMAP_OFFSET_Y				(1659.84f)

#define MAX_POI							(11)  // max number of waypoint markers in total
#define MAX_POI_STANDARD_USER			(10)  // max number of waypoint markers by standard input (pausemap, script etc)
#define POI_SPECIAL_ID					(10)  // id of the special POI

#define MAX_WAYPOINT_MARKERS			(4)  // max number of waypoint markers
#define MULTIPLAYER_WAYPOINT_MARKER_ID	(3)  // id of the waypoint marker to always use for MP

#define INVALID_BLIP_ID	(0)  // this needs to match script and script resource invalid ids

#define MAX_SCRIPT_SCALE_AMOUNT_BIGMAP		(7500)
#define MAX_SCRIPT_SCALE_AMOUNT_STANDARD	(1500)

struct sPointOfInterestStruct
{
	sPointOfInterestStruct() : iUniqueBlipId(INVALID_BLIP_ID) {};

	s32 iUniqueBlipId;

	CMiniMapBlip* GetBlip() const;
	void Delete();
} ;

struct sWaypointStruct
{
	sWaypointStruct()
		: iPlayerModelHash(0)
		, iBlipIndex(INVALID_BLIP_ID)
		, ObjectId(NETWORK_INVALID_OBJECT_ID)
		, iAssociatedBlipId(INVALID_BLIP_ID)
	{}
	u32	iPlayerModelHash;
	s32 iBlipIndex;
	Vector2 vPos;
	ObjectId ObjectId;
	s32 iAssociatedBlipId;
};

typedef struct
{
	u32 iMiniMapRenderedTimer;
	eSETUP_STATE MinimapModeStateWhenHidden;
	Vector3 vMiniMapCentrePosWhenHidden;
	bool bMiniMapInteriorWhenHidden;
	bool bBeenInPauseMenu;
} sMiniReappearanceState;

struct sMiniMapInteriorStruct
{
	u32 iMainInteriorHash;
	Vector2 vMainInteriorPos;
	float fMainInteriorRot;
	atArray<u32> iSubInteriorHash;
};

struct sMinimapFakeCone
{
	CMiniMapBlip* pBlip;

	float fVisualFieldMinAzimuthAngle;
	float fVisualFieldMaxAzimuthAngle;
	float fCentreOfGazeMaxAngle;

	float fPeripheralRange;
	float fFocusRange;

	float fRotation;

	bool bContinuousUpdate;

	eHUD_COLOURS color;
};

enum eBITMAP_STATE
{
	MM_BITMAP_NONE = 0,
	MM_BITMAP_LOADING,
	MM_BITMAP_ATTACH,
	MM_BITMAP_ACTIVE,
	MM_BITMAP_DETACH,
	MM_BITMAP_SHUTDOWN
};

struct sBitmapStruct
{
	strLocalIndex iTxdId;
	eBITMAP_STATE iState;
	CDblBuf<int> iDrawIdx; // The index of the txd to draw, or -1 for no txd
};

class CBlipCones
{
	struct sConeDetails
	{
		sConeDetails(float fFocusRange, bool bUsedThisFrame, float fPeripheralRange, float fGazeAngle, float fVisualFieldMinAzimuthAngle, float fVisualFieldMaxAzimuthAngle) 
			: m_fFocusRange(fFocusRange), m_bUsedThisFrame(bUsedThisFrame), m_fPeripheralRange(fPeripheralRange), m_fGazeAngle(fGazeAngle), 
			m_fVisualFieldMinAzimuthAngle(fVisualFieldMinAzimuthAngle), m_fVisualFieldMaxAzimuthAngle(fVisualFieldMaxAzimuthAngle)
		{
		}

		float m_fFocusRange;
		float m_fPeripheralRange;
		float m_fGazeAngle;
		float m_fVisualFieldMinAzimuthAngle;
		float m_fVisualFieldMaxAzimuthAngle;
		bool m_bUsedThisFrame;
	};

public:
	void Init(unsigned initMode);

	void ClearUsedFlagsForAllBlipCones();

	void RemoveUnusedBlipCones();

	//	Returns true if the cone should be redrawn either because it's a new cone or because the focus range has changed
	//	B*2343564: Added additional parameters to update blip and redraw if any other parameters have changed (currently only used for cop peds if perception values have been overriden by script using SET_COP_PERCEPTION_OVERRIDES).
	bool UpdateBlipConeParameters(s32 UniqueBlipId, float fFocusRange, bool bRedrawIfAnyConeParametersChange, float fPeripheralRange, float fGazeAngle, float fVisualFieldMinAzimuthAngle, float fVisualFieldMaxAzimuthAngle);

private:
	typedef atMap< s32, sConeDetails > BlipConeMap;
	BlipConeMap m_MapOfBlipCones;
};

enum class eWaypointClearOnArrivalMode : u8
{
	Enabled,
	DisabledForObjects,
	Disabled,
	MAX
};

class CMiniMap
{
public:
	struct MiniMapDesc {
		// in order to use the RAG widgets, we need to terminate these strings with spacers we set to zero.
		BANK_ONLY( MiniMapDesc() : xSpacer(0), ySpacer(0) {}; )

		char m_fileName[MAX_LENGTH_OF_MINIMAP_FILENAME];
		Vector2 m_position;
		Vector2 m_size;
		u8 m_alignX;
		BANK_ONLY( u8 xSpacer; )
		u8 m_alignY;
		BANK_ONLY( u8 ySpacer; )
	};

	struct Tunables : public CTuning
	{
		Tunables();

		CMiniMap_SonarTunables Sonar;
		CMiniMap_HealthBarTunables HealthBar;
		CMiniMap_BitmapTunables Bitmap;
		CMiniMap_FogOfWarTunables FogOfWar;
		CMiniMap_CameraTunables Camera;
		CMiniMap_OverlayTunables Overlay;
		CMiniMap_TileTunables Tiles;
		CMiniMap_DisplayTunables Display;

		PAR_PARSABLE;
	};
	static Tunables sm_Tunables;

	static void Init(unsigned initMode);
	static void HandleCoreXML(parTreeNode* pMinimapNode);
	static void HandleInteriorXML(parTreeNode* pMinimapNode);	
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void ReInit();
#if RSG_PC
	static void DeviceReset();
	static void DeviceLost();
#endif

	static void UpdateAtEndOfFrame();

	static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

	static void AddOrRemovePointOfInterest(float fPosX, float fPosY);
	static bool GetPointOfInterestDetails(s32 index, Vector2& vPosition); // note that indices are volatile--deleting a POI will adjust all indexes

	static void SwitchOnWaypoint(float fPosX, float fPosY, ObjectId ObjectId = NETWORK_INVALID_OBJECT_ID, bool bHideWaypointIcon = false, bool bSetLocalDirty = true, eHUD_COLOURS colorOverride = eHUD_COLOURS::HUD_COLOUR_WAYPOINT);
	static void	SwitchOffWaypoint(bool bSetLocalDirty = true);
	static void UpdateWaypoint();
	static bool GetActiveWaypoint(sWaypointStruct& Waypoint);
	static bool SetActiveWaypoint(const Vector2& vPosition, ObjectId ObjectId = NETWORK_INVALID_OBJECT_ID);
	static bool IsActiveWaypoint(Vector2 vPosition, ObjectId ObjectId = NETWORK_INVALID_OBJECT_ID);
	static bool IsAnyWaypointIdActive();
	static bool IsPlayerWaypointActive(u32 iPlayerHash);
	static s32 GetActiveWaypointId();
	static bool IsWaypointActive() { return (GetActiveWaypointId() != INVALID_BLIP_ID); }

	static bool IsEntityWaypointAllowed() { return ms_bEntityWaypointAllowed; }
	static void SetEntityWaypointAllowed(bool Value);

	static eWaypointClearOnArrivalMode GetWaypointClearOnArrivalMode() { return ms_bWaypointClearOnArrivalMode; }
	static void SetWaypointClearOnArrivalMode(eWaypointClearOnArrivalMode Value);

	static u16 GetWaypointGpsFlags() { return ms_waypointGpsFlags; }
	static void SetWaypointGpsFlags(u16 Value);

	static void GetWaypointData(u32 WaypointIndex, u32 &PlayerHash, Vector2 &vPosition);
	static void SetWaypointData(u32 WaypointIndex, u32 PlayerHash, const Vector2 &vPosition);

	static void MarkLocallyOwned(bool isLocallyOwned);
	static bool IsWaypointLocallyOwned();
	static unsigned GetWaypointLocalDirtyTimestamp();

	static void SetFlashWantedOverlay(const bool bFlashOn);
	static s32 GetNearestWantedCriminalActualBlipId();

	static s32 GetActualCentreBlipId();
	static s32 GetActualNorthBlipId();

	static bool GetIsHiddenAfterTransition() { return ms_bIsHiddenAfterTransition; }

	static float GetPauseMapScale() { return ms_fPauseMenuMapScale; }
	static void SetPauseMapScale(float fScale) { ms_fPauseMenuMapScale = fScale; }
	static Vector2 GetPauseMapCursor() { return ms_vPauseMenuMapCursor; }
	static void SetPauseMapCursor(Vector2 vNewPos) { ms_vPauseMenuMapCursor = vNewPos; }

	static void SetBackgroundMapAsHidden(bool bValue) { ms_bHideBackgroundMap = bValue; }

	static void SetBlockWaypoint(bool bValue) { ms_bBlockWaypoint = bValue; }
	static bool GetBlockWaypoint() { return ms_bBlockWaypoint; }

	static void SetInPrologue(bool bValue);
	static bool GetInPrologue() { return ms_bInPrologue; }

	static void SetOnIslandMap(bool bValue);
	static bool GetIsOnIslandMap() { return ms_bOnIslandMap; }

	static void SetInStealthMode(bool bValue) { ms_bInStealthMode = bValue; }

	static void FlashMiniMapDisplay(eHUD_COLOURS hudColour);

	static void SetCurrentGolfMap(eGOLF_COURSE_HOLES iValue);
	static eGOLF_COURSE_HOLES GetCurrentGolfMap() { return ms_iGolfCourseMap; }
	static bool IsInGolfMap() { return (ms_bInGolfMap && GetCurrentGolfMap() != GOLF_COURSE_OFF); }

	static void LockMiniMapAngle(s32 iAngle) { ms_iLockedMiniMapAngle = iAngle; }
	static void LockMiniMapPosition(Vector2 vNewPos) { ms_vLockedMiniMapPosition = vNewPos; }

	static bool IsInPauseMap() { return CPauseMenu::IsActive() && ms_bInPauseMap; }
	static bool IsInCustomMap() { return CPauseMenu::IsActive() && sMiniMapMenuComponent.IsActive(); }

	static bool IsInBigMap() { return ((!IsInPauseMap() && !IsInCustomMap()) && ms_bInBigMap); }
	static bool WasInBigMap() { return ms_bInBigMap; }

	static void SetBigMapAsFullScreen(bool bFullScreen) { ms_bBigMapFullScreen = bFullScreen; }

	static void SetForceSonarBlipsThisFrame(bool bForce) { ms_bForceSonarBlipsOn = bForce; }

#if __BANK
	static void InitWidgets();

	static bool GetOverrideConeColour() { return ms_bOverrideConeColour; }
	static s32 GetConeColourRed() { return ms_ConeColourRed; }
	static s32 GetConeColourGreen() { return ms_ConeColourGreen; }
	static s32 GetConeColourBlue() { return ms_ConeColourBlue; }
	static s32 GetConeColourAlpha() { return ms_ConeColourAlpha; }

	static float GetConeFocusRangeMultiplier() { return ms_fConeFocusRangeMultiplier; }
	static bool GetDrawActualRangeCone() { return ms_bDrawActualRangeCone; }
#endif // __BANK

	static s32 GetMovieId(s32 iId) { return iMovieId[iId]; }

	static s32 CreateBlip(bool bComplex, eBLIP_TYPE iBlipType, const CEntityPoolIndexForBlip& iPoolIndex, eBLIP_DISPLAY_TYPE iDisplayFlag, const char *pScriptName);
	static s32 CreateBlip(bool bComplex, eBLIP_TYPE iBlipType, const Vector3& vPosition, eBLIP_DISPLAY_TYPE iDisplayFlag, const char *pScriptName);
	static s32 CreateSimpleBlip(const Vector3& vPosition, const char *pScriptName);

	static void AllowSonarBlips(bool bSet) { ms_bSonarBlipsActive = bSet; }
	static void CreateSonarBlip(const Vector3& vPos, float fSize, eHUD_COLOURS iHudColour, CPed* ped = NULL, bool bScriptRequested = false, bool shouldOverridePos = true);
	static float CreateSonarBlipAndReportStealthNoise(CPed* pPed, const Vec3V& vPos, float fSize, eHUD_COLOURS iHudColor, bool bCreateSonarBlip = true, bool shouldOverridePos = true);
	static void ClearSonarBlips();
	static void ClearAllBlipRoutes();

	static bool RemoveBlip(CMiniMapBlip *pBlip);

	// get:
	static Vector3 GetPlayerBlipPosition();
	static Vector3 GetBlipPositionValue(CMiniMapBlip *pBlip);
	static eBLIP_TYPE GetBlipTypeValue(CMiniMapBlip *pBlip);
	static eBLIP_DISPLAY_TYPE GetBlipDisplayValue(CMiniMapBlip *pBlip);
	static s8 GetBlipNumberValue(CMiniMapBlip *pBlip);
	static eBLIP_COLOURS GetBlipColourValue(CMiniMapBlip *pBlip);
	static s32 GetBlipAlphaValue(CMiniMapBlip *pBlip);
	static const char* GetBlipNameValue(CMiniMapBlip *pBlip);
	static s32 GetBlipObjectNameId(CMiniMapBlip *pBlip);
	static float GetBlipDirectionValue(CMiniMapBlip *pBlip);
	static Color32 GetBlipColour32Value(CMiniMapBlip *pBlip);
	static const char*	GetBlipObjectName(CMiniMapBlip *pBlip)		{ return CMiniMap_Common::GetBlipName( static_cast<CBlipComplex*>( (pBlip&&pBlip->IsComplex()) ? pBlip : blip[iSimpleBlip])->iLinkageId ); }
	static u8 GetBlipCategoryValue(CMiniMapBlip *pBlip)  { if (pBlip && pBlip->IsComplex()) return ((CBlipComplex*)pBlip)->iCategory; else return ((CBlipComplex*)blip[iSimpleBlip])->iCategory; }

	// set:
	static void SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, s32 iParam = 0);
	static void SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, Color32 iParam);
	static void SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, float fParam);
	static void SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, const Vector3& vParam);
	static void SetBlipParameter(eBLIP_PARAMS iTodo, s32 iIndex, const char *cParam);

	static void SetBlipCategoryValue(CMiniMapBlip *pBlip, u8 iNewBlipCategory);
	static void SetBlipNameValue(CMiniMapBlip *pBlip, const char *cParam, bool bFromTextFile);
	static void SetBlipNameValue(CMiniMapBlip *pBlip, const atHashWithStringBank& hash);
	static void SetBlipDisplayValue(CMiniMapBlip *pBlip, eBLIP_DISPLAY_TYPE iParam);
	static void SetBlipNumberValue(CMiniMapBlip *pBlip, s8 iParam);

	static void AddFakeConeData(int iBlipIndex, float fVisualFieldMinAzimuthAngle, float fVisualFieldMaxAzimuthAngle, float fCentreOfGazeMaxAngle, float fPeripheralRange, float fFocusRange, float fRotation, bool bContinuousUpdate, eHUD_COLOURS color);
	static void DestroyFakeConeData(int iBlipIndex);
	static void ClearFakeConesArray() { m_FakeCones.clear();}

	static void SetInteriorMovieActive(bool bActive) { ms_bInteriorMovieIsActive = bActive; }  // this is set up in safe area
	static bool IsInteriorMovieActive() { return ms_bInteriorMovieIsActive; }

	static s32 GetActualBlipUsed(const CMiniMapBlip *pBlip);
	static s32 GetUniqueBlipUsed(const CMiniMapBlip *pBlip);
	static bool IsBlipIdInUse(s32 iUniqueIndex);

	static s32 GetUniqueCentreBlipId() { return ms_iCentreBlip; }

	static void SetVisible(bool bVisible);
	static bool GetVisible() { return bVisible; }
	static bool AreAllTexturesActive(MM_BITMAP_VERSION mm);
	static MM_BITMAP_VERSION GetBitmapForMinimap() { return CMiniMap::sm_Tunables.Bitmap.eBitmapForMinimap; }

	static bool IsBlipOnStage(CMiniMapBlip *pBlip);

	static CMiniMapBlip *GetBlipFromActualId(s32 iId);
	static CMiniMapBlip *GetBlip(s32 iUniqueBlipId);

	static void RemoveBlipAttachedToEntity(const CEntity *pEntity, eBLIP_TYPE blipType);
	static CMiniMapBlip *GetBlipAttachedToEntity(const CEntity *pEntity, eBLIP_TYPE blipType, u32 FlagToCheck, bool bInUseOnly = false);

	static s32 GetActualSimpleBlipId();

	static CEntityPoolIndexForBlip &GetBlipPoolIndexValue(CMiniMapBlip *pBlip);
	static CPickupPlacement *GetPickupPlacementFromBlip(CMiniMapBlip *pBlip);

	static bool IsFlagSet(const CMiniMapBlip *pBlip, u32 flag);
	static void UnsetFlag(CMiniMapBlip *pBlip, u32 flag);
	static void SetFlag(CMiniMapBlip *pBlip, u32 flag);
	static void ResetFlag(CMiniMapBlip *pBlip);

	typedef atFixedBitSet<MAX_BLIP_FLAGS, u32> BlipFlagBitset;
	static void GetAllFlags(BlipFlagBitset& outFlags, CMiniMapBlip* pBlip);

	static void AddConeColorForBlip(s32 iBlipIndex, eHUD_COLOURS coneColor);

	static void ToggleComponent(s32 const c_componentId, bool const c_visible, bool const c_immediate, eHUD_COLOURS const c_hudColor = HUD_COLOUR_INVALID);
	static void ToggleTerritory(s32 iPropertyId, s32 iOwner1, s32 iOwner2, s32 iOwner3, s32 iOwner4);
	static void ShowYoke(const bool bVisible, const float fPosX = 0.0f, const float fPosY = 0.0f, const s32 iAlpha = 0);
	static void ShowSonarSweep(const bool bVisible);

	static CEntity *FindBlipEntity(CMiniMapBlip *pBlip);

	static s32 GetMaxCreatedBlips() { return iMaxCreatedBlips; }

	static bool GetIsInsideInterior(bool bIgnorePauseMapSettings = false);

	static void ExitPauseMapState();
	static void SetMinimapModeState(eSETUP_STATE newState, bool bForceIt = false);	//	 { MinimapModeState = newState; }

	static void SetBigMapFullZoom(bool bFullZoom) { ms_bBigMapFullZoom = bFullZoom; }

#if __BANK
	static void SetDisplayGameMiniMap(bool bDisplayGameMiniMap) { ms_g_bDisplayGameMiniMap = bDisplayGameMiniMap; }
	static bool GetDisplayAllBlipNames() { return bDisplayAllBlipNames; }
	static inline s8 GetBlipDebugNumberValue(CMiniMapBlip *pBlip) {if (pBlip && pBlip->IsComplex()) return ((CBlipComplex*)pBlip)->iDebugBlipNumber; else return ((CBlipComplex*)blip[iSimpleBlip])->iDebugBlipNumber; }
	static inline void SetBlipDebugNumberValue(CMiniMapBlip *pBlip, s8 iParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->iDebugBlipNumber = iParam; }
	static bool DebugDrawTiles() { return ms_bDebugDrawTiles;}
	static bool DrawVectorSeaPaused() { return CMiniMap::sm_Tunables.Tiles.bDrawVectorSeaPaused;}
	static bool DrawVectorSeaMinimap() { return CMiniMap::sm_Tunables.Tiles.bDrawVectorSeaMinimap;}
#endif // __BANK

	static void UpdateCentreAndNorthBlips();

	static void UpdateDisplayConfig();

	static CPed *GetMiniMapPed();
	static void SetMiniMapPedPoolIndex(s32 iPoolIndex) { ms_iMiniMapPedPoolIndex = iPoolIndex; }

	static bool IsRendering(){ return(ms_bIsRendering); }
	static eBLIP_PRIORITY GetBlipPriorityValue(CMiniMapBlip *pBlip) { if (pBlip->IsComplex()) return (eBLIP_PRIORITY)((CBlipComplex*)pBlip)->priority; else return (eBLIP_PRIORITY)((CBlipComplex*)blip[iSimpleBlip])->priority; }

	static bool ms_bUseWeaponRangeForMPSonarDistance;
	static bool ms_bBitmapInitialized;
	static atMultiArray3d<sBitmapStruct> ms_BitmapBackground;
	static sBitmapStruct ms_BitmapSuperLowRes;

	static bool IsOverridingInterior() { return (ms_bFoundMatchingOverrideInterior) && (GetIsInsideInterior(true)); }

	static void SetBlipVisibilityHighDetail(bool bHigh) { ms_bBlipVisibilityHighDetail = bHigh; }
	static bool GetBlipVisibilityHighDetail() { return ms_bBlipVisibilityHighDetail; }

#if ENABLE_FOG_OF_WAR
	static void SetRequestFoWClear(bool bValue) { ms_bRequestFoWClear = bValue; }
	static void SetFoWMapValid(bool bValue) { ms_bSetFowIsValid = bValue; }
	static bool GetFowMapValid() { return ms_bSetFowIsValid; }
	static void SetHideFow(bool bValue) { ms_bHideFoW = bValue; }
	static void SetDoNotUpdateFoW(bool bValue) { ms_bDoNotUpdateFoW = bValue; }
	static void RevealCoordinate(const Vector2& coordinates);
	static void SetIgnoreFowOnInit(bool bValue) { ms_bIgnoreFoWOnInit = bValue; }
	static bool GetIgnoreFowOnInit() { return ms_bIgnoreFoWOnInit; }
#if __D3D11
	static void SetUploadFoWTextureData(bool bValue) { ms_bUploadFoWTextureData = bValue;}
#endif // __D3D11

	static void	EnableFowCustomWorldExtents(bool bValue)	{ ms_bFoWUseCustomExtents=bValue; }
	static bool	AreFowCustomWorldExtentsEnabled()			{ return(ms_bFoWUseCustomExtents); }
	static void SetFowCustomWorldPos(float x, float y)		{ ms_vecFoWCustomWorldPos.x=x;  ms_vecFoWCustomWorldPos.y=y;  }
	static float GetFowCustomWorldX()						{ return ms_vecFoWCustomWorldPos.x; }
	static float GetFowCustomWorldY()						{ return ms_vecFoWCustomWorldPos.y; }
	static void SetFowCustomWorldSize(float w, float h)		{ ms_vecFoWCustomWorldSize.x=w;	ms_vecFoWCustomWorldSize.y=h; }
	static float GetFowCustomWorldW()						{ return ms_vecFoWCustomWorldSize.x; }
	static float GetFowCustomWorldH()						{ return ms_vecFoWCustomWorldSize.y; }
#endif // ENABLE_FOG_OF_WAR

	static bool ShouldRenderMinimap();  // so feed can use it
	static void GetCentrePosition(Vector3 &vReturnVector);

	static int GetNorthBlipIndex()				{ return ms_iNorthBlip; }
	static Vector2 GetCurrentMiniMapMaskSize();

	static void SetAllowFoWInMP(bool b)			{ ms_bAllowFoWInMP = b; }
	static bool GetAllowFoWInMP()				{ return ms_bAllowFoWInMP;  }
	static void SetRequestRevealFoW(bool b)		{ ms_bRequestRevealFoW = b; }

private:
	static void CreateWaypointBlipAndRoute(sWaypointStruct& Waypoint, eBLIP_DISPLAY_TYPE iDisplayType = BLIP_DISPLAY_BLIPONLY, eHUD_COLOURS iHudColor = HUD_COLOUR_WAYPOINT);
	static void ClearWaypointBlipAndRoute(sWaypointStruct& Waypoint);

	static void SetInBigMap(bool bBigMap);  // this should only ever be called from SetMinimapModeState
	static void SetInGolfMap(bool bGolfMap);  // this should only ever be called from SetMinimapModeState

	static void RestartMiniMapMovie();
	static void CreateCentreAndNorthBlips();

	static bool ShouldProcessMinimap();

	static bool ShouldAddSonarBlips();

	static void LoadXmlData(bool bForceReload = false);

	
	static eMINIMAP_VALUES GetCurrentMiniMap();
	static char *GetCurrentMiniMapFilename();
	
	static Vector2 GetCurrentMiniMapMaskPosition();
	static Vector2 GetCurrentMiniMapBlurPosition();
	static Vector2 GetCurrentMiniMapBlurSize();	

	static Vector2 GetCurrentMiniMapPosition();
	static Vector2 GetCurrentMiniMapSize();

	static bool IsBackgroundMapHidden() { return ms_bHideBackgroundMap; }

	static bool GetInStealthMode() { return ms_bInStealthMode; }

	static s32 GetLockedAngle() { return ms_iLockedMiniMapAngle; }
	static Vector2 GetLockedPosition() { return ms_vLockedMiniMapPosition; }
	static bool IsPositionLocked() { return ( (GetLockedPosition().x != -9999.0f) && (GetLockedPosition().y != -9999.0f) ); }

	static void SetInPauseMap(bool bPauseMap) { ms_bInPauseMap = bPauseMap; }
	static void SetInCustomMap(bool bCustomMap) {  ms_bInPauseMap = !bCustomMap;  }

//	static bool IsInBigMapFull() { return ((!IsInPauseMap()) && ms_bInBigMap && ms_bBigMapFullZoom); }

// 	static bool IsShowingGPSDisplay() { return ms_bShowingGPSDisplay; }
// 	static void SetShowingGPSDisplay(bool bShow) { ms_bShowingGPSDisplay = bShow; }

#if __BANK
	static void CreateBankWidgets();
	static void ShutdownWidgets();
	static void SetBigMapActiveDebug();
	static void UpdateMiniMapDebug();
	static void CreateRemoveDebugBlip();
	static void ToggleAllPedToStealth();
	static void ReloadXmlData();
	static void ModifyLabelsToAllActiveBlips(bool bSwitchOn, bool bDebugNumbersOnly);
#endif // __BANK

	static void OldMiniMapConstructor();

	static void RemoveMiniMapMovie();
	static void SetSimpleBlipDefaults();
	static void SetActive();
	static void UpdateMiniMap();
	static void UpdateBitmapOnUT(MM_BITMAP_VERSION mmOverride = (MM_BITMAP_VERSION)MM_BITMAP_VERSION_NUM_ENUMS);

	static void UpdateBitmapState( sBitmapStruct& bitmap, bool bStreamThisFrame, bool bDrawThisFrame, const char * cBitmapString);

	// get:
	static Vector3 GetBlipPositionValueOnStage(CMiniMapBlip *pBlip);

	static bool ms_bBlipVisibilityHighDetail;

	static inline void SetBlipTypeValue(CMiniMapBlip *pBlip, eBLIP_TYPE iParam) { if (pBlip->IsComplex()) ((CBlipComplex*)pBlip)->type = (u8)iParam; }

	static void DestroyBlipObject(CMiniMapBlip *pBlip);

	static s32 ConvertUniqueBlipToActualBlip(s32 iIndex);


	// get:
	static inline const Vector2& GetBlipScaleValue(CMiniMapBlip *pBlip) { if (pBlip->IsComplex()) return ((CBlipComplex*)pBlip)->vScale; else return ((CBlipComplex*)blip[iSimpleBlip])->vScale; }
	static inline bool GetBlipOn3dMap(CMiniMapBlip *pBlip) { if (pBlip->IsComplex()) return ((CBlipComplex*)pBlip)->bBlipIsOn3dMap; else return ((CBlipComplex*)blip[iSimpleBlip])->bBlipIsOn3dMap; }
	static inline u16 GetBlipFlashInterval(CMiniMapBlip *pBlip) {if (pBlip && pBlip->IsComplex()) return ((CBlipComplex*)pBlip)->iFlashInterval; else return ((CBlipComplex*)blip[iSimpleBlip])->iFlashInterval; }
	static inline u16 GetBlipFlashDuration(CMiniMapBlip *pBlip) {if (pBlip && pBlip->IsComplex()) return ((CBlipComplex*)pBlip)->iFlashDuration; else return ((CBlipComplex*)blip[iSimpleBlip])->iFlashDuration; }

	// set:
	static void SetBlipObjectName(CMiniMapBlip *pBlip, const BlipLinkage& iNameId );
	static void SetBlipDirectionValue(CMiniMapBlip *pBlip, float iParam, bool bClamp = true);
	static inline void SetBlipScaleValue(CMiniMapBlip *pBlip, float fParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->vScale.Set(fParam, fParam); }
	static inline void SetBlipScaleValue(CMiniMapBlip *pBlip, Vector2 vParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->vScale = vParam; }
	static inline void SetBlipColourValue(CMiniMapBlip *pBlip, eBLIP_COLOURS iParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->iColour = (u32)iParam; }
	static inline void SetBlipSecondaryColourValue(CMiniMapBlip *pBlip, Color32 iParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->iColourSecondary = iParam; }
	static inline void SetBlipOn3dMap(CMiniMapBlip *pBlip, bool bParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->bBlipIsOn3dMap = bParam; }
	static inline void SetBlipPositionValue(CMiniMapBlip *pBlip, Vector3::Vector3Param vParam) { if (pBlip->IsComplex()) ((CBlipComplex*)pBlip)->vPosition = vParam; else { Vector3 vp(vParam); ((CBlipSimple*)pBlip)->vPosition.x = vp.x; ((CBlipSimple*)pBlip)->vPosition.y = vp.y; }     }
	static inline void SetBlipPriorityValue(CMiniMapBlip *pBlip, eBLIP_PRIORITY iParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->priority = (u8)iParam; }
	static inline void SetBlipAlphaValue(CMiniMapBlip *pBlip, u8 iParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->iAlpha = iParam; }
	static inline void SetBlipFlashInterval(CMiniMapBlip *pBlip, u16 iParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->iFlashInterval = iParam; }
	static inline void SetBlipFlashDuration(CMiniMapBlip *pBlip, u16 iParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->iFlashDuration = iParam; }
	static inline void SetBlipPoolIndexValue(CMiniMapBlip *pBlip, const CEntityPoolIndexForBlip& iParam) { if (pBlip && pBlip->IsComplex()) ((CBlipComplex*)pBlip)->m_PoolIndex = iParam; }

	static s32 CreateGenericBlip(bool bComplex, eBLIP_TYPE iBlipType, const CEntityPoolIndexForBlip& iPoolIndex, const Vector3& vPosition, eBLIP_DISPLAY_TYPE iDisplayFlag, const char *pScriptName );

	static void UpdateMapPositionAndScaleOnUT(float &fReturnAngle, float &fReturnOffset, Vector3 &vReturnPosition, float &fReturnRange, s32 &iReturnRotation);
	static void UpdateAltimeter(sMiniMapRenderStateStruct& out_Results);

	static bool CreateMiniMapMovie();
	static void UpdateMultiplayerInformation();
	static void UpdateTilesOnUT(bool &bReturnBitmapOnly, Vector2 &vReturnTilesAroundPlayer);

	static void UpdateStatesOnUT();
	static void UpdateGPS();

	static void UpdateDpadDown();

	static void SetupConeDetail(CMiniMapBlip *pBlip, CPed *pPed);
	static void SetupFakeConeDetails(CMiniMapBlip* pBlip);
	static void UpdateBlips();
	static void UpdateBlipMarkers();

	static void UpdateAndAutoSwitchOffFlash(CMiniMapBlip *pBlip);

//	static bool RemoveBlipFromStage(CMiniMapBlip *pBlip);

	static s32 FindAndCreateNextEmptyBlipSpace(bool bComplex);

	static s32 CreateNewUniqueBlipFromActualBlipId(s32 iIndex);

	static float GetRevisedMiniMapRangeFromPercentage(float fPercentage);
	static float GetRevisedMiniMapRangeFromScript(float fCurrentRange, s32 iScriptZoomValue);

	static float GetRange(bool bGetTrueRange = false);

	static Vector2 GetCurrentMainTile();

	static bool IsPlayerRunning();
	static float GetSpeedOfPlayersVehicle();
	static bool IsPlayerUsingParachute();
	static bool IsPlayerInPlaneOrHelicopter();
	static bool IsPlayerInBikeOrCar();
	static bool IsPlayerOnHorse();
	static bool IsPlayerUnderwaterOrInSub();

	static void ReduceSoundSizeForEnvironment( float& fOutSize, Vec3V_In vPos, const CPed* pPed );
	static void SendBlipsToCompanionApp();

	static void ResetFlags();

// Private variables
//	static bool bSkipBlipUpdate;
	static sMiniMapInterior ms_previousInterior;
	static CBlipCones ms_blipCones;
	static s32 ms_iMiniMapPedPoolIndex;
	static eSETUP_STATE MinimapModeState;
	static eSETUP_STATE ms_pendingMinimapModeState;
	static bool ms_bInteriorMovieIsActive;
	static bool ms_bFoundMatchingOverrideInterior;
	static float ms_fBlipZoom;
	static float ms_fMapZoom;
	static bool ms_bBigMapFullZoom;
	static bool ms_bEntityWaypointAllowed;
	static eWaypointClearOnArrivalMode ms_bWaypointClearOnArrivalMode;
	static u16 ms_waypointGpsFlags;
	static sWaypointStruct ms_Waypoint[MAX_WAYPOINT_MARKERS];
	static atFixedArray<sPointOfInterestStruct, MAX_POI_STANDARD_USER>  ms_PointOfInterest;
	static bool ms_bSonarBlipsActive;

#if __BANK
	static bool ms_g_bDisplayGameMiniMap;
	static float ms_fDebugRange;
	static s32 ms_iDebugInteriorHash;
	static s32 ms_iDebugInteriorLevel;
	static s32 ms_iDebugGolfCourseMap;
	static bool ms_bCullDistantBlips;
	static bool ms_bShowBlipVectorMap;

	static bool ms_bDebugDrawTiles;
#endif // __BANK

//	static sMiniMapRenderStateStruct MiniMapUpdateState;

#if __DEV
	static s32 iNumCreatedTraces;
	static u32 iSizeOfBlipsCreated;
#endif

#if __DEV
	static bool ms_bRenderPedPerceptionCone;
#endif	//	__DEV

	static atVector<sSonarBlipStruct> ms_SonarBlips;
	
	static Vector3 ms_vMapPosition;
	static float ms_fMaxDistance;

	static bool bVisible;

#if __BANK
	static bool bDisplayAllBlipNames;

	static bool ms_bOverrideConeColour;
	static s32 ms_ConeColourRed;
	static s32 ms_ConeColourGreen;
	static s32 ms_ConeColourBlue;
	static s32 ms_ConeColourAlpha;
	static float ms_fConeFocusRangeMultiplier;
	static bool ms_bDrawActualRangeCone;
	static s32 ms_iDebugBlip;
#endif // __BANK

	static bool ms_bXmlDataHasBeenRead;
	static bool ms_bCentreBlipPosChangedThisFrame;
	static s32 ms_iCentreBlip;
	static s32 ms_iNorthBlip;

//	static s32 ms_iBlipHovering;
	static MiniMapDesc sm_miniMaps[MAX_MINIMAP_VALUES];
//	static char ms_cMiniMapFileName[MAX_MINIMAP_VALUES][MAX_LENGTH_OF_MINIMAP_FILENAME];
//	static Vector2 ms_vMiniMapPosition[MAX_MINIMAP_VALUES];
//	static Vector2 ms_vMiniMapSize[MAX_MINIMAP_VALUES];

	static bool ms_bInStealthMode;
	static bool ms_bHideBackgroundMap;
	static bool ms_bBlockWaypoint;
	static bool ms_bInPrologue;
	static bool ms_bOnIslandMap;
	static bool ms_bInPauseMap;
	static bool ms_bInCustomMap;
	static bool ms_bInBigMap;
	static bool ms_bInGolfMap;
//	static bool ms_bShowingGPSDisplay;
	static bool ms_bIsRendering;
	static bool ms_bShowingYoke;
	static bool ms_bShowSonarSweep;
	static bool ms_bAllowFoWInMP;
	static bool ms_bRequestRevealFoW;

	static bool ms_bBigMapFullScreen;
	static bool ms_bIsWaypointLocal;
	static unsigned ms_WaypointLocalDirtyTimestamp;
	static bool ms_bFlashWantedOverlay;
	static eHUD_COLOURS ms_eFlashColour;
	static u32 ms_uFlashStartTime;

	static bool ms_bForceSonarBlipsOn;
	
	KEYBOARD_MOUSE_ONLY(static eALTIMETER_MODES ms_iPrevAltimeterMode;)
	static eGOLF_COURSE_HOLES ms_iGolfCourseMap;
	static s32 ms_iLockedMiniMapAngle;
	static Vector2 ms_vLockedMiniMapPosition;

	static Vector2 ms_vPauseMenuMapCursor;
	static float ms_fPauseMenuMapScale;	

	static s32 iSimpleBlip;
	static u16 iReferenceCount;

	static bool bActive;
	static s32 iMovieId[MAX_MINIMAP_MOVIE_LAYERS];

	static CMiniMapBlip *blip[MAX_NUM_BLIPS];
	static s32 iMaxCreatedBlips;

	static atMap<int, eHUD_COLOURS> m_ConeColors;
	static atArray<sMinimapFakeCone> m_FakeCones;

//	static float fPreviousWantedLevel;
	static bool bPreviousMultiplayerStatus;
	static bool ms_bPreviousShowDepthGauge;
	static s32 ms_iPreviousMapRotation;
	static s32 iPreviousGpsInstruction;
	static float fPreviousGPSDistance;
	static float fPreviousMinimapFloor;
	static u32 iMiniMapDpadZoomTimer;

	static atArray<sMiniMapInteriorStruct> sm_InteriorInfo;
	static sMiniReappearanceState ms_MiniMapReappearanceState;
	static bool ms_bIsHiddenAfterTransition;
#if ENABLE_FOG_OF_WAR
	static bool ms_bHideFoW;
	static bool ms_bRequestFoWClear;
	static bool ms_bSetFowIsValid;
	static bool ms_bDoNotUpdateFoW;
	static bool ms_bIgnoreFoWOnInit;
	static bool ms_bUploadFoWTextureData;
	static bool ms_bFoWUseCustomExtents;
	static Vector2 ms_vecFoWCustomWorldPos;		// fWorldX/Y
	static Vector2 ms_vecFoWCustomWorldSize;	// fWorldW/H
	static Vector2 ms_ScriptReveal[8];
	static int	ms_numScriptRevealRequested;
#endif // ENABLE_FOG_OF_WAR

	static bool ms_bInsideReInit;
public:
	static const float fMinTimeBetweenBlipsMS;

#if __BANK
	static bool ms_bRenderMiniMapBackground;
	static bool ms_bRenderMiniMapForeground;
#endif  // __BANK

};

#endif // #ifndef __MINIMAP_H__

// eof
