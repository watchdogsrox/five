/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : NewHud.cpp
// PURPOSE : manages the new Scaleform-based HUD code
// AUTHOR  : Derek Payne
// STARTED : 17/11/2009
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_NEW_HUD_H_
#define INC_NEW_HUD_H_

//rage
#include "parser/manager.h"

#include "vector/color32.h"
#include "vector/vector2.h"

//game
#include "Scaleform/scaleform.h"
#include "Scaleform/scaleformmgr.h"
#include "Text/Messages.h"

#include "Weapons/info/WeaponInfo.h"

#include "control/replay/ReplaySettings.h"
#include "atl/hashstring.h"

// really do wish this were an interface class...
#include "Frontend/CWeaponWheel.h"
#include "Frontend/CRadioWheel.h"
#include "Frontend/HudTools.h"
#include "Frontend/hud_colour.h"
#include "Frontend/UIReticule.h"
#include "Frontend/UIReplayScaleformController.h"
#include "Network/Cloud/Tunables.h"


// FORWARD DECLARATIONS
class CPed;

namespace rage{
#if __BANK
class bkBank;
class bkGroup;
#endif
};



#if GTA_REPLAY
enum eReplayHudComponents
{
	HUD_REPLAY_CONTROLLER = 0,
	HUD_REPLAY_MOUSE,
	HUD_REPLAY_HEADER,
	HUD_REPLAY_OPTIONS,
	HUD_REPLAY_HELP_TEXT,
	HUD_REPLAY_MISC_TEXT,
	HUD_REPLAY_TOP_LINE,
	HUD_REPLAY_BOTTOM_LINE,
	HUD_REPLAY_LEFT_BAR,
	HUD_REPLAY_TIMER,

	MAX_REPLAY_HUD_COMPONENTS
};
#else
#define MAX_REPLAY_HUD_COMPONENTS 0
#endif // GTA_REPLAY

#define MAX_SP_CHARACTER_COLOURS (3)

#define MAX_WEAPON_HUD_COMPONENTS (100)
#define MAX_SCRIPT_HUD_COMPONENTS (30)

#define WEAPON_HUD_COMPONENTS_START (MAX_NEW_HUD_COMPONENTS)
#define WEAPON_HUD_COMPONENTS_END (WEAPON_HUD_COMPONENTS_START+MAX_WEAPON_HUD_COMPONENTS)

#define SCRIPT_HUD_COMPONENTS_START (WEAPON_HUD_COMPONENTS_END)
#define SCRIPT_HUD_COMPONENTS_END (SCRIPT_HUD_COMPONENTS_START+MAX_SCRIPT_HUD_COMPONENTS)

#if GTA_REPLAY
#define REPLAY_HUD_COMPONENTS_START (SCRIPT_HUD_COMPONENTS_END)
#define REPLAY_HUD_COMPONENTS_END (REPLAY_HUD_COMPONENTS_START+MAX_REPLAY_HUD_COMPONENTS)
#endif // GTA_REPLAY

#define TOTAL_HUD_COMPONENT_LIST_SIZE (MAX_NEW_HUD_COMPONENTS+MAX_WEAPON_HUD_COMPONENTS+MAX_SCRIPT_HUD_COMPONENTS+MAX_REPLAY_HUD_COMPONENTS)

#define OFFSET_FROM_EDGE_OF_SCREEN (0.1f)  // offset from edge of screen when considered "off screen".  Probably should be atleast the size of safezone

#define BASE_HUD_COMPONENT (NEW_HUD + 1)

class CFrontendXMLMgr
{
public:
	static void RegisterXMLReader(atHashWithStringBank objName, datCallback handleFunc);
	static void LoadXML( bool bForceReload = false);
	static void LoadXMLNode(atHashWithStringBank nodeName);

	static void DoNothing(s32, parTreeNode*) {};
	static bool IsLoaded() { return sm_bLoadedOnce; }

private:
	typedef atMap<atHashWithStringBank, datCallback> XMLLoadTypeMap;
	static XMLLoadTypeMap sm_xmlLoadMap;
	static bool sm_bLoadedOnce;

};

#define REGISTER_FRONTEND_XML(func, name)			CFrontendXMLMgr::RegisterXMLReader(name, datCallback(CFA1(func), 0, true))
#define REGISTER_FRONTEND_XML_CB(func, name, data)	CFrontendXMLMgr::RegisterXMLReader(name, datCallback(CFA2(func), CallbackData(data)))


class CHudTunablesListener : public TunablesListener
{
public:
#if !__NO_OUTPUT
	CHudTunablesListener() : TunablesListener("CHudTunablesListener") {}
#endif
	virtual void OnTunablesRead();
};

class CNewHud
{

	enum eLOAD_STATE
	{
		NOT_LOADING = 0,
		LOADING_REQUEST_TO_CREATE_MOVIE,
		LOADING_REQUEST_BY_ACTIONSCRIPT,
		LOADING_REQUEST_NOT_BY_ACTIONSCRIPT,
		WAITING_ON_SETUP_BY_ACTIONSCRIPT,
		WAITING_ON_REMOVAL_BY_ACTIONSCRIPT,
		REQUEST_REMOVAL_OF_MOVIE
	};

	enum eVISIBILITY_STATE
	{
		VSS_NOT_CHANGING = 0,

		// force-hide
		VSS_HIDE_THIS_FRAME_FIRST_TIME,
		VSS_HIDE_THIS_FRAME,
		VSS_HIDDEN_LAST_FRAME,

		// force-show
		VSS_SHOW_THIS_FRAME_FIRST_TIME,
		VSS_SHOW_THIS_FRAME,		
		VSS_SHOWN_LAST_FRAME
	};

	//
	// sNewHudDataInfo - structure to hold data for each hud component
	//
	struct sNewHudDataInfo
	{
		bool bActive : 1;
		bool bCodeThinksVisible : 1;  // this means IF ITS ACTIVE AND ONSCREEN code thinks it should display.  It doesnt mean its loaded or onscreen
		bool bHasGfx : 1;
		bool bStandaloneMovie : 1;
		bool bInUseByActionScript : 1;
		bool bWantToResetPositionOnNextSetActive : 1;
		bool bIsPositionOverridden : 1;
		bool bShouldShowComponentOnMpTextChatClose : 1;
		bool bShouldShowComponentOnIMEClose : 1;
		eLOAD_STATE iLoadingState : 4;
		eVISIBILITY_STATE iScriptRequestVisibility : 4;
		eVISIBILITY_STATE iScriptRequestVisibilityPreviousFrame : 4;
		// some padding sneaks in here
		s32 iId;
		s32 iDepth;
		s32 iListId;
		s32 iListPriority;
		char cFilename[40];
		char cAlignX[2];
		char cAlignY[2];
		Vector2 vPos;
		Vector2 vSize;
		Vector2 vRevisedPos;
		Vector2 vRevisedSize;
		Vector2 vOverriddenPositionToUseNextSetActive;
		float fNoOffsetPosX;
		float fNoOffsetRevisedPosX;
		
		Color32 colour;

		

#if __DEV
		char cDebugLog[200];
#endif // __DEV

		sNewHudDataInfo() 
		{
			Init();
		}

		void Init();
	};


public:

	enum eDISPLAY_MODE
	{
		DM_INVALID = -1,
		DM_DEFAULT = 0,
		DM_ARCADE_CNC,
		DM_NUM_DISPLAY_MODES
	};

	struct sHealthInfo
	{
		int m_iHealthPercentageValue;
		int m_iHealthPercentageCapacity;

		int m_iArmourPercentageValue;
		int m_iArmourPercentageCapacity;
	
		int m_iEndurancePercentageValue;
		int m_iEndurancePercentageCapacity;

		int m_iTeam;

		bool m_bShowDamage;
		const CPed* m_pPedFetched;

		sHealthInfo()
		{
			Reset();
		}

		void Reset()
		{
			m_iHealthPercentageValue = -1;
			m_iHealthPercentageCapacity = -1;
			m_iArmourPercentageValue = -1;
			m_iArmourPercentageCapacity = -1;
			m_iEndurancePercentageValue = -1;
			m_iEndurancePercentageCapacity = -1;		
			m_iTeam = 0;
			m_bShowDamage = true;
			m_pPedFetched = NULL;
		}

		void Set(int iHealth, int iMaxHealth, int iArmour, int iMaxArmour, int iEndurance, int iMaxEndurance, int iHudMaxHealthDisplay, int iHudMaxArmourDisplay, int iHudMaxEnduranceDisplay, int iTeam ,const CPed* pPedFetched, bool bShowDamage = true);
	};

	struct sSpecialAbility
	{
		float fPercentage;
		float fPercentageCapacity;

		sSpecialAbility() : fPercentage(0.0f), fPercentageCapacity(-1.0f) {}

		bool operator==(const sSpecialAbility& rhs) const
		{
			return fPercentage == rhs.fPercentage && fPercentageCapacity == rhs.fPercentageCapacity;
		}

		bool operator!=(const sSpecialAbility& rhs) const { return ! operator==(rhs); }
	};


private:
	//
	// sHudChangeValues - structure to hold data so we can check changes
	//
	struct sPreviousFrameHudValues
	{
		sSpecialAbility	SpecialAbility;
		sHealthInfo healthInfo;
		eTEXT_ARROW_ORIENTATION iDirectionOffScreen[MAX_HELP_TEXT_SLOTS];
		Vector2 vHelpMessagePosition[MAX_HELP_TEXT_SLOTS];

		s32 iWantedLevel;
		s64 iCash;
		s64 iMultiplayerWalletCash;
		s64 iMultiplayerBankCash;
		s32 iAmmo;
		s32 iClipAmmo;
		u32 iPlayerPedModelHash;	//	Used to decide whether the cash change should be shown on the hud
		u32 iWeaponHash;
		f32 fWeaponTimerPercentage;
		float m_fAirPercentage;
		float fAirPercentageWhileSubmerged;

		bool bDroppingProjectile;
		bool bCashForcedOn;
		bool bAimingOrReloading;
		bool bCopsHaveLOS;
//		bool bEnemiesHaveLOS;
		bool bReasonsToFlashForever;
		bool bFlashWholeWantedMovie;
		bool bFlashWantedStars;
		bool bFlashHealthMeter;
		bool bDisplayingSavingMessage;
		bool bAbilityBarActive;
		bool bMultiplayerStatus;
	};

	

public:


	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Restart();

	static void Update();
	static void Render();

#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif //RSG_PC

	static void UpdateAtEndOfFrame();
	static void UpdateHudDisplayConfig();
	static void UpdateDisplayConfig(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass);

	static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);
	static bool IsHelpTextHiddenByScriptThisFrame(s32 iIndex);
	static void SetHudComponentToBeShown(s32 iId);
	static void SetHudComponentToBeHidden(s32 iId);
	static void SetHudComponentPosition(s32 iId, Vector2 vPos);
	static void ResetHudComponentPositionFromOriginalXmlValues(s32 iId);
	static void SetHudComponentColour(s32 iComponentId, eHUD_COLOURS iHudColor, s32 iAlpha);
	static void SetHudComponentAlpha(s32 iComponentId, s32 iAlpha);
	static char *GetHudComponentFileName(s32 iId) { return HudComponent[iId].cFilename; }
	static Vector2 GetHudComponentPosition(s32 iId) { return HudComponent[iId].vRevisedPos; }
	static Vector2 GetHudComponentSize(s32 iId) { return HudComponent[iId].vRevisedSize; }
	//static void SetHudComponentColour(s32 iComponentId, eHUD_COLOURS iHudColor, s32 iAlpha);
	static Color32 GetHudComponentColour(s32 iId) { return HudComponent[iId].colour; }
	//static void SetHudComponentAlpha(s32 iComponentId, s32 iAlpha);
	static s32 GetHudComponentMovieId(s32 iId) { return HudComponent[iId].iId; }
	static bool IsHudComponentActive(s32 iComponentId);
	static bool IsHudComponentMovieActive(s32 iComponentId);

#if RSG_PC
	static bool UpdateImeText();
#endif // RSG_PC

	static void UseFakeMPCash(bool bUseFakeMPCash);
	static void ChangeFakeMPCash(int iMPWalletAmount, int iMPBankAmount);
	static void DisplayFakeSPCash(int iAmountChanged);

	static bool GetShowHelperTextAfterPauseMenuExit() { return ms_bShowHelperTextAfterPauseMenuExit; }
	static void SetShowHelperTextAfterPauseMenuExit(bool justExit) { ms_bShowHelperTextAfterPauseMenuExit = justExit; }
	
	static s32 GetContainerMovieId() { return HudComponent[NEW_HUD].iId; }

	static float GetLowestTopRightYPos() { return ms_fLowestTopRightY; }

	static void ActivateHudComponent(bool bActivatedByActionScript, s32 iComponentId, bool bLoadInstantly = false, bool bChildMovie = false);

	static void RepopulateHealthInfoDisplay();
	static void RemoveHudComponentFromActionScript(s32 iComponentId);
	static void RemoveHudComponent(s32 iComponentId);

	static void IncrementHelpTextToken(s32 iId) { iHelpTextLockedWaitingForActionscriptResponse[iId]++; }
	static u32 GetHelpTextToken(s32 iId) { return iHelpTextLockedWaitingForActionscriptResponse[iId]; }

	static void SetHelpTextWaitingForActionScriptResponse(s32 iId, bool bValue) { bHelpTextLockedWaitingForActionscriptResponse[iId] = bValue; }
	static bool GetHelpTextWaitingForActionScriptResponse(s32 iId) { return bHelpTextLockedWaitingForActionscriptResponse[iId]; }

	static void GetHealthInfo(const CPhysical* pEnt, sHealthInfo& setMe);
	static void SetHealthOverride(int iHealth, int iArmour, bool bShowDmg = true, int iEndurance = 0); // -1 for health to turn override off
	static int GetHudWantedLevel(class CWanted* pWanted, bool& bFlashOverride);
	static bool GetHudWantedCopsAreSearching(class CWanted* pWanted);

	static void HideStreetNamesThisFrame() {ms_bHideStreetNamesThisFrame = true;}

	static void HandleCharacterChange();
	static void SetCurrentCharacterColour(eHUD_COLOURS colour);
	static void SetCurrentCharacterColourFromStat();
	static eHUD_COLOURS GetCurrentCharacterColour();
	static eHUD_COLOURS GetCharacterColour(int iCharacter);

	static void SetDisplayMode(eDISPLAY_MODE newMode) { ms_eDisplayMode = newMode; }
	static eDISPLAY_MODE GetDisplayMode() { return ms_eDisplayMode; }
	static eDISPLAY_MODE GetInvalidDisplayMode() { return CNewHud::DM_INVALID; }

//	static bool SetSavingText(const char* pMessageText, s32 iIconStyle);
//	static void HideSavingText();

#if SUPPORT_MULTI_MONITOR
	static void SetDrawBlinder(int iDrawBlindersFrames);
	static bool ShouldDrawBlinders();
#endif	// SUPPORT_MULTI_MONITOR

	// safezone drawing
	static void EnableSafeZoneRender(int milliseconds);
	static bool ShouldDrawSafeZone();
	static void DrawSafeZone();

	static void FlushAreaDistrictStreetVehicleNames();
	static bool CanRenderHud();

	static bool DoesScriptWantToForceHideThisFrame(s32 iComponentId, bool bFirstTimeOnly = false);

	static bool IsWeaponWheelVisible() { return ms_WeaponWheel.IsVisible(); }
	static bool AreInstructionalButtonsActive();

	static bool IsFullHudHidden() { return ms_bHudIsHidden; }

	static void ForceMultiplayerCashToReshowIfActive();

	static void ForceUpdateHealthInfoWithExistingValuesOnRT();

	static void ForceSpecialVehicleStatusUpdate();

	static bool GetIsOverrideSpecialAbilitySet() { return ms_bOverrideSpecialAbility; }
private:
	static void RestartHud(bool bForceRestart);
	static bool DoesHudComponentBlockRestart(const s32 iComponent);

	static void InitStartingValues();
	static void LoadXmlData(bool bForceReload = false);
	static void HandleXMLComponent(s32 packedPoint, parTreeNode* pNodeRead);
	static void RetrieveComponentInfo(parTreeNode* pNode, s32 iComponentId);

	static void ResetVisibilityPerFrame(s32 iComponentId);
	static bool DoesScriptWantToForceShowThisFrame(s32 iComponentId, bool bFirstTimeOnly = false);

	static void SendComponentValuesToActionScript(s32 iComponentId);
	static void UpdateMultiplayerInformation();

	static bool ShouldHudComponentBeShownWhenHudIsHidden(s32 iHudComponentIndex);
	static void ShowHudComponent(s32 iHudComponentIndex);
	static void HideHudComponent(s32 iHudComponentIndex);
	static void UpdateVisibility();
	static void ResetAllPerFrameVisibilities();
	static void ResetAllPreviousValues();
	static void ResetPreviousValueOfHudComponent(s32 iHudComponent);

	static GFxMovieView::ScaleModeType GetHudScaleMode(s32 iComponentId);
	static void OffsetHudComponentForCurrentResolution(s32 iComponentId);

	static void UpdateWhenPaused();

	static void OverrideMPCash(s64 iMPWalletCashOverride, s64 iMPBankCashOverride);

	static void UpdateHealthInfo(const bool bInstant);
	static void UpdateWantedStars();
	static void UpdateWantedStars_ArcadeCNC();

	static void UpdateMiniMapOverlay();

	static void Wanted_SetCopLOS( const bool bCopsHaveLOS );	
	static void UpdateSinglePlayerCash();
	static void UpdateGameStream();
	static void UpdateMultiplayerCash();
	static void UpdateHelpText();
	static void UpdateAreaDistrictStreetVehicleNames();
	static void UpdateAbility();
	static void UpdateAirBar();
	static void UpdateWeaponAndAmmo();
	static void UpdateForMpChat();
	static void UpdateIMEVisibility();

	static void GetCash(s64 &iWalletCash, s64 &iBankCash);

	static void ActuallyUpdateVehicleName();

	static bool UpdateWhenBustedOrWasted();
	static void UpdateDpadDown();

	static bool ShouldHideHud(bool checkSettings);

	static eDISPLAY_MODE ms_eDisplayMode;

	static atRangeArray<sNewHudDataInfo, TOTAL_HUD_COMPONENT_LIST_SIZE>  HudComponent;
	static CWeaponWheel ms_WeaponWheel;
	static CRadioWheel	ms_RadioWheel;

	static eHUD_COLOURS ms_eSPCharacterColours[MAX_SP_CHARACTER_COLOURS];
	static eHUD_COLOURS ms_eCurrentCharacterColour;

	static sPreviousFrameHudValues PreviousHudValue;
	static CNewHud::eDISPLAY_MODE s_previousMultiplayerGameMode;
	static sHealthInfo ms_HealthInfo;
	static sSpecialAbility ms_SpecialAbility;
	static bool ms_bOverrideSpecialAbility;
	static bool ms_bAllowAbilityBar;

	static s64 m_iMPWalletCashOverride;
	static s64 m_iMPBankCashOverride;
	static bool m_bSuppressCashUpdates;


#if SUPPORT_MULTI_MONITOR
	static int ms_iDrawBlinders;
#endif	//SUPPORT_MULTI_MONITOR

	static u32 ms_iGpsDpadZoomTimer;
	static bool ms_bHudIsHidden;
	static bool ms_bSuppressHealthFetch;
	static float ms_fLowestTopRightY;
	static BankBool ms_bHideReticleWithLaserSight;

	static u32 iHelpTextLockedWaitingForActionscriptResponse[MAX_HELP_TEXT_SLOTS];
	static bool bHelpTextLockedWaitingForActionscriptResponse[MAX_HELP_TEXT_SLOTS];

	static bool ms_bRestartHudWhenCannotRender;
	static bool ms_bRestartHud;
	static s32 ms_iNumHudComponentsRemovedSinceLastRestartHud;
	static bool ms_bIsShowingCharacterSwitch;
	static bool ms_bHideStreetNamesThisFrame;
	static bool ms_bShowHelperTextAfterPauseMenuExit;
	static bool ms_Initialized;
	static bool ms_InitializedEndurance;

	static bool ms_bDeviceReset;

	static CHudTunablesListener* m_pTunablesListener;

	static CUIReticule m_reticule;

#if GTA_REPLAY
	static CUIReplayScaleformController m_replaySFController;
#endif // GTA_REPLAY

public:
	static void AllowRestartHud();
	static void SetSubtitleText(char *pMessageText);
	static void ClearSubtitleText();
	static void ClearHelpText(s32 iHelpTextSlot, bool bClearNow);

	// radio wheel
	static bool IsRadioWheelActive();
#if __DEV
	static const char* GetRadioWheelDisabledReason() { return ms_RadioWheel.GetDisabledReason(); }
	static const char* GetWeaponWheelDisabledReason() { return ms_WeaponWheel.GetDisabledReason(); }
#endif

	// weapon wheel
	static bool ShouldWheelBlockCamera();
	static bool IsWeaponWheelActive();
	static bool IsUsingWeaponWheel() { return ms_WeaponWheel.IsActive(); }
	static void SetWeaponWheelForcedByScript( bool bOnOrOff ) { ms_WeaponWheel.SetForcedByScript(bOnOrOff); }
	static void SetWeaponWheelSpecialVehicleForcedByScript()	{ ms_WeaponWheel.SetSpecialVehicleAbilityFromScript(); }
	static void SetWeaponWheelSuppressResultsThisFrame() { ms_WeaponWheel.SetSuppressResults(true); }
	static void SetWeaponWheelContents(SWeaponWheelScriptNode paScriptNodes[], int nodeCount) { ms_WeaponWheel.SetContentsFromScript(paScriptNodes, nodeCount); }
	static void SetWheelWeaponInHand(u32 uWeaponInHand) { ms_WeaponWheel.SetWeaponInHand(uWeaponInHand); }
	static u32  GetWheelWeaponInHand() { return ms_WeaponWheel.GetWeaponInHand(); }
	static bool GetWheelDisableReload() { return ms_WeaponWheel.GetDisabledReload(); }
	static void SetWheelDisableReload(bool bDisable) { ms_WeaponWheel.SetDisabledReload(bDisable); }
	static s32  GetWheelHolsterAmmoInClip() { return ms_WeaponWheel.GetHolsterAmmoInClip(); }
	static void SetWheelHolsterAmmoInClip(s32 sAmmo) { ms_WeaponWheel.SetHolsterAmmoInClip(sAmmo); }
	static atHashWithStringNotFinal	GetWheelCurrentlyHighlightedWeapon() { return ms_WeaponWheel.GetCurrentlyHighlightedWeapon(); }
	static bool GetWheelHasASlotWithMultipleWeapons() { return ms_WeaponWheel.HasASlotWithMultipleWeapons(); }
	static void SetWheelSelectedWeapon(atHashWithStringNotFinal uWeaponHash) {ms_WeaponWheel.SetSelected(uWeaponHash);}
	static void SetWheelMemoryWeapon(atHashWithStringNotFinal uWeaponHash) { ms_WeaponWheel.SetWeaponInMemory(uWeaponHash);}
	static atHashWithStringNotFinal GetWheelMemoryWeaponAtSlot(u8 uSlotIndex) { return ms_WeaponWheel.GetWeaponInMemory(uSlotIndex); }

	static atHashWithStringNotFinal GetWeaponHashFromMemory(u8 uSlotIndex) { return ms_WeaponWheel.GetWeaponHashFromMemory(uSlotIndex); }
#if KEYBOARD_MOUSE_SUPPORT
	static void PopulateWeaponsFromWheelMemory(atRangeArray<atHashWithStringNotFinal, MAX_WHEEL_SLOTS>& memoryList);
#endif // KEYBAORD_MOUSE_SUPPORT
	
	// character switch
	static void  SetShowingCharacterSwitch(bool bShowing) { ms_bIsShowingCharacterSwitch = bShowing; }
	static bool  IsShowingCharacterSwitch() { return ms_bIsShowingCharacterSwitch; }

	// radial menus
	static bool IsShowingHUDMenu() {return IsWeaponWheelActive() || IsRadioWheelActive() || IsShowingCharacterSwitch();}


	static bool IsReticuleVisible() {return m_reticule.GetPreviousValues().IsDisplaying();}
	static CUIReticule& GetReticule() {return m_reticule;}

#if GTA_REPLAY
	static CUIReplayScaleformController& GetReplaySFController() {return m_replaySFController;}
#endif // GTA_REPLAY

	static bool IsSniperSightActive();

	static void BlinkAbilityBar(int millisecondsToFlash);
	static void SetToggleAbilityBar(bool bTurnOn);
	static void SetAllowAbilityBar(bool bAllow) { ms_bAllowAbilityBar = bAllow; }
	static void SetAbilityBarGlow(bool bTurnOn);
	static void SetAbilityOverride(float fPercentage, float fMaxPercentage);

#if __BANK
	static s32 ms_iHudComponentId;
	static char ms_cDebugMethod[256];
	static bool bForceScriptWidescreen;
	static bool bForceScriptHD;
	static bool bDisplayDebugOutput;
	static bool bDebugScriptGfxDrawOrder;
	static bool bUpdateHelpTextWhenPaused;
	static bool ms_bDontRemoveDebugWidgets;
	static bool ms_bForceAreaDistrictStreetVehicle;
	static s32 iDebugComponentVisibilityDisplay;
	static bool ms_bDebugHelpText;
	static bool ms_bShowEverything;
	static bool ms_bLogPlayerHealth;

	static void CallDebugMethod();
	static void CreateBankWidgets();
	static void InitWidgets();
	static void ShutdownWidgets();
	static void AddComponentToWidget(s32 iIndex);
	static void ReadjustAllComponents();
	static void RetriggerDebugOutput();
	static void ToggleForceShowDistrictVehicleArea();
	static void ForceShowHudComponent(eNewHudComponents eHudComponentId, bool bForceShow);
	static void DebugUnloadMovie();
	static void DebugLoadMovie();
	static void DebugOverrideComponent();
	static void DebugResetComponent();
	static void dbgApplyOverride();
	static void dbgBeginOverride();
	static void dbgEndOverride();
#endif  // __BANK

#if !__FINAL
	static bool m_bDebugFlashWantedStars;
#endif
};


#endif  // INC_NEW_HUD_H_

// eof

