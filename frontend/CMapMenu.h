/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CMapMenu.h
// PURPOSE : code to control the pausemap in the pausemenu
// AUTHOR  : Derek Payne
// STARTED : 24/10/2012
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef __CMAPMENU_H__
#define __CMAPMENU_H__

#include "atl/string.h"
#include "parser/macros.h"
#include "Text/TextFile.h"
#include "CScriptMenu.h"
#include "system/control.h"
#include "frontend/minimapcommon.h"
#include "frontend/ButtonEnum.h"

#define ZOOM_LEVEL_START_MP			(ZOOM_LEVEL_2)
#define ZOOM_LEVEL_START_PROLOGUE	(ZOOM_LEVEL_2)  // basic starting point, for prologue map
#define ZOOM_LEVEL_START			(ZOOM_LEVEL_0)  // basic starting point, zoomed in as far as we can without any tiles
#define MAX_ZOOMED_OUT_PROLOGUE		(ZOOM_LEVEL_2)
#define MAX_ZOOMED_OUT				(ZOOM_LEVEL_1)
#define MAX_ZOOMED_IN				(ZOOM_LEVEL_4)

struct sMapLegendItem
{
	s32 iUniqueBlipId;
};

struct sMapLegendList
{
	atArray<sMapLegendItem> blip;
	char cBlipName[MAX_BLIP_NAME_SIZE + 32];	//	Lowrider Bug 2534692 requires a longer string for the Beast blip name - <FONT FACE='$Font2_cond_NOT_GAMERNAME' SIZE='15'>~a~</FONT> with the translated word Beast inserted where the ~a~ is.
	char cBlipObjectName[MAX_BLIP_NAME_SIZE];
	s32 iBlipObjectId;
	s32 iCurrentSelected;
	Color32 blipColour;
	u8 iBlipCategory;
	bool bActive;
};

class HoveredBlipUniqueId
{
public:
	HoveredBlipUniqueId();
	~HoveredBlipUniqueId();

	HoveredBlipUniqueId& operator=(const HoveredBlipUniqueId& rhs ) { Set(rhs.m_iUniqueId); return *this; }
	HoveredBlipUniqueId& operator=(const s32& rhs )					{ Set(rhs);				return *this; }

	bool IsValid() const;

	s32 Get() const { return m_iUniqueId; }
	void Set(s32 newIndex);
	void Reset();

#if __BANK
	s32* GetPtr() { return &m_iUniqueId; }
#endif
private:
	s32 m_iUniqueId;
	
	bool IsMyBlipUniqueToMe() const;

	// because there's multiple things that can turn on/off the same lightswitch, we gotta track all ones to ensure it's safe
	// this is in lieu of adding ref counting to each and every blip
	static atArray<HoveredBlipUniqueId*> sm_pAllElements; // could be a list, but for now I only need one other one
};

//////////////////////////////////////////////////////////////////////////
class CMapMenu : public CScriptMenu
{
public:
	CMapMenu(CMenuScreen& owner);
	~CMapMenu();

	virtual void Init();
	virtual void Render(const PauseMenuRenderDataExtra* renderData);
	virtual void Update();
	virtual bool UpdateInput(s32 eInput);

	virtual bool IsDoneLoading() const;

	virtual void LoseFocus();

	virtual bool Populate(MenuScreenId newScreenId);

	virtual bool TriggerEvent(MenuScreenId MenuId, s32 iUniqueId);
	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir );
	virtual void PrepareInstructionalButtons( MenuScreenId MenuId, s32 iUniqueId);
	virtual void OnNavigatingContent(bool bAreWe);

	virtual bool CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

	virtual atString& GetScriptPath();

	static void SetHideLegend(bool bHide) { sm_bHideLegend = bHide; }
	static void UpdatePauseMapLegend(bool bInstant = false, bool bRestartTimer = false);
	static void OnBlipDelete(CMiniMapBlip* pBlip);

	static void SetMapZoom(s32 iNewZoomLevel, bool bInstant);
	static void SetIsMaskRendering(bool bIsRendering) { sm_bIsRenderingMask = bIsRendering; }
	static void RenderPauseMapTint(const Vector2& vBackgroundPos, const Vector2& vBackgroundSize, float fAlpha, bool bBrightTint);

	static bool ShouldBlipBeShownOnLegend(CMiniMapBlip *pBlip);
	bool CheckCursorOverBlip(Vector2 vCursorPos, bool bSnapToBlip, bool bForce, bool bHighlightOnLegend);

	void SnapToBlip(CMiniMapBlip *pBlip, bool bAllowedToLerp = true, bool bSetLegend = true);
	void ProcessLegend(s32 iLegendItem, s32 iSelectedValue = -1);
	bool UpdateMapScreenInput(bool bInitialEntry, s32 eInput = PAD_NO_BUTTON_PRESSED);

	static s32 GetCurrentSelectedMissionCreatorBlip();
	static bool IsHoveringOnMissionCreatorBlip() { return sm_bCanStartMission || sm_bCanContact; }
	static void ShowStartMissionInstructionalButton(bool bShow) { sm_bShowStartMissionButton = bShow; }
	static void ShowContactInstructionalButton(bool bShow) { sm_bShowContactButton = bShow; }

	static bool GetLegendItemData(CMiniMapBlip* pBlip, s32& iLegendItem, s32& iBlipIndex);
	static void UpdateLegendDisplayItem(CMiniMapBlip* pBlip);
	static void UpdateLegendDisplayItem(s32 iLegendItem, s32 iActualId, bool bUpdateCurrent);
	void SetCurrentLegendItem(s32 iLegendItem, s32 iBlipIndex = -1, bool bUpdateCurrent = false);
	void ClearMapOverlays();
	void UpdateLegend(bool bClear, bool bForceSendToActionScript);

	static void RemoveLegend();

	static s32 GetCurrentZoomLevel() { return sm_iCurrentZoomLevel; }

	static bool GetIsInInteriorMode() { return sm_bPauseMapInInterior; }
	static bool GetIsInGolfMode() { return sm_bPauseMapInGolf; }
	static void InitZoomLevels();
	static void InitStartingValues();
	static void ReloadMap();
//	static void ShowNextMissionObjective(bool bShow);

#if RSG_PC
	virtual void DeviceReset();
#endif

#if __BANK
	virtual void AddWidgets(bkBank* pBank);
#endif

private:
	void SetCursorTarget(const Vector2& vPosToLerpTo, bool bSkipDelay);
	void SetCursorActual(const Vector2& vPosToSnapTo);

	void SetCursorTarget(float x, float y, bool bSkipDelay) { SetCursorTarget(Vector2(x,y), bSkipDelay); }
	void SetCursorActual(float x, float y)					{ SetCursorActual(Vector2(x,y)); }

	void SetCurrentBlipHover(s32 iCurrentBlipHoverActualId, bool bSnapToBlip, bool bHighlightOnLegend);

	bool CanSetWaypoint();
	void PlaceWaypoint(bool bAtMousePosition = false);

	void DisplayAreaName(bool bForceUpdate);
	void UpdateZooming(float fLeftShoulder, float fRightShoulder, bool bInitialEntry);  // over 1.0f and it will snap instantly
	void HighlightLegendItem(CMiniMapBlip *pBlip);
	void ResetCornerBlipInfo(bool bClear, bool bForceUpdateName);
	void SetWaypoint(bool KEYBOARD_MOUSE_ONLY(bUseMousePos) = false);
	bool IsBlipInGroupedCategory(s32 iThisBlipCategory);
	static void UpdateLegendSlotWithNameAndColour(sMapLegendList *pLegendItem, CMiniMapBlip *pBlip, s32 iThisBlipCategory);
	void SendLegendToActionscript();
	static void EmptyLegend(bool bForceRemove);
	void GetMapExtents(Vector2 &vMin, Vector2 &vMax, bool bUseLegendList);
	
	Vector2 GetWorldPosFromScreen(float x, float y, bool bIncludeTranslation = true) const;
	Vector2 GetScreenPosFromWorld(float x, float y, bool bIncludeTranslation = true) const;
	s32 FindActualBlipIdNearPos(const Vector2& vCursorPos, bool bTightenRadius = false) const;

	static s32 sm_iCurrentZoomLevel;
	static bool sm_bCurrentlyZooming;
	static u32 sm_iMissionCreatorHoverQueryTimer;
	static u32 sm_iUpdateLegendTimer;
	
	static bool sm_bHasQueried;
	static Vector2 sm_vPreviousCursorPos;

	static atArray<sMapLegendList> sm_LegendList;
	static bool sm_bCanStartMission;
	static bool sm_bShowStartMissionButton;
	static bool sm_bCanContact;
	static bool sm_bShowContactButton;
	static bool sm_bUpdateLegend;
	static bool sm_bHideLegend;
	static s32 sm_iPreviousHoverActualId;
	static s32 sm_iCurrentSelectedMissionCreatorUniqueId;
	static bool sm_bPauseMapInInterior;
	static bool sm_bPauseMapInGolf;
	static Vector2 sm_vPauseMapLastZoomedPos;
	static Vector2 sm_vPauseMapOriginalPos;
	static float sm_fPrevZoomLevel;

	static bool sm_bIsRenderingMask;
	static bool sm_bWasReloaded;

#if KEYBOARD_MOUSE_SUPPORT
	static BankFloat MOUSE_WHEEL_DECAY_RATE;
	static BankFloat MOUSE_WHEEL_SCALAR;
	static BankFloat MOUSE_FLICK_DECAY_RATE;
	static BankFloat MOUSE_ZOOM_SPEED_FAR;
	static BankFloat MOUSE_ZOOM_SPEED_NEAR;
	static BankFloat MOUSE_DRAG_DIST_SQRD;
	static BankFloat MOUSE_DRAG_SOUND_SCREEN_PIXELS_SQRD;
	static BankFloat MAX_SOUNDVAR_VALUE;
	static BankUInt32 MOUSE_DBL_CLICK_TIME;
	static BankInt32 MAP_SNAP_PAN_SPEED;
#endif

	HoveredBlipUniqueId m_iBlipCurrentlyHoveredOverUniqueId;
	s32 m_iCurrentLegendItem;
	s32 m_iCurrentLegendItemIndex;

#if RSG_PC
	u8 m_iRenderDelay;
#endif

protected:
	Vector2 m_vBGSize;
	Vector2 m_vBGPos;
	Vector2 m_vCursorTarget;
	u32		m_uCursorTargetTime;
	atString m_SPPath;
	bool	m_bLastClearedTitle;
	bool	m_bAtLeastOneLowDetailBlip;
	bool	m_bCheckFakedPauseMapPosForCursor;
	bool	m_bCheckGPSPauseMapPosForCursor;

#if KEYBOARD_MOUSE_SUPPORT

	bool IsMouseOverMap() const;
	bool UpdateMouseClicksAndPosition(Vector2& vNewPos);

	void HandleClickAction(bool bIsDoubleClick);

	void UpdateMouseWheelZoom(float& fLeftShoulder, float& fRightShoulder);

	Vector2 m_vwrldMouseMovement;
	Vector2 m_vscrLastMousePos;
	Vector2 m_vscrMousePosOnClick;
	u32		m_uFirstClickTime;
	HoveredBlipUniqueId m_iBlipMouseCurrentlyHoverOverUniqueId;
	s32		m_iBlipClickedOnUniqueId;
	float   m_fMouseWheelMovement;
	bool	m_bClickedWithoutDragging;
	bool	m_bWasBlipDragging;
#endif

#if __BANK
	bkGroup*		m_pMyGroup;
#endif

};

#endif // __CMAPMENU_H__

