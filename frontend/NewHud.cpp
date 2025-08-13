/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : NewHud.cpp
// PURPOSE : manages the new Scaleform-based HUD code
// AUTHOR  : Derek Payne
// STARTED : 17/11/2009
//
/////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "grcore/allocscope.h"
#include "grcore/debugdraw.h"
#include "parser/manager.h"
#include "script/thread.h"

// Game headers
#include "frontend/NewHud.h"


#include "audio/frontendaudioentity.h"
#include "audioengine/engine.h"
#include "frontend/BusySpinner.h"
#include "frontend/PauseMenu.h"
#include "frontend/FrontendStatsMgr.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/MobilePhone.h"
#include "frontend/MiniMap.h"
#include "frontend/MultiplayerChat.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/SocialClubMenu.h"
#include "frontend/WarningScreen.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/ui_channel.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonPedAimCamera.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "Control/gamelogic.h"
#include "Control/gps.h"
#include "control/replay/replay.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/MarketingTools.h"
#include "Game/Clock.h"
#include "Game/user.h"
#include "Game/Dispatch/DispatchData.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/ped_channel.h"
#include "peds/PedEventScanner.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PlayerInfo.h"
#include "renderer/rendertargets.h"
#include "renderer/sprite2d.h"
#include "SaveLoad/savegame_channel.h"
#include "Script/script.h"
#include "Script/script_hud.h"
#include "scene/world/gameworld.h"
#include "Stats/StatsInterface.h"
#include "Stats/MoneyInterface.h"
#include "system/Control.h"
#include "system/controlMgr.h"
#include "system/param.h"
#include "system/system.h"
#include "system/tamperactions.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Text/Messages.h"
#include "Text/Text.h"
#include "Text/TextConversion.h"
#include "Text/TextFormat.h"
#include "Vehicles/VehicleGadgets.h"
#include "Weapons/Components/WeaponComponent.h"
#include "Weapons/WeaponTypes.h"

#if RSG_PC
#include "rline/rlpc.h"
#include "replaycoordinator/ReplayCoordinator.h"
#endif // RSG_PC

#if __WIN32PC
#include "core/watermark.h"
#endif

#include "Text/GxtString.h"

//OPTIMISATIONS_OFF()
FRONTEND_OPTIMISATIONS()

#define FAKE_CASH_NOT_IN_USE -1

#if	!__FINAL
PARAM(nohud, "[code] hide hud");
PARAM(noweaponwheel, "[code] dont use weapon wheel");
PARAM(nohelptextpause, "[code] update help text when paused");
PARAM(noautorestarthud, "[code] do not restart the hud");
PARAM(logPlayerHealth, "[code] Logs player health to log files");

#endif

const bool s_useNewBlipTextHud = true;

#if __BANK
s32 CNewHud::ms_iHudComponentId = NEW_HUD;
char CNewHud::ms_cDebugMethod[256];
bool CNewHud::bDisplayDebugOutput = false;
bool CNewHud::bForceScriptWidescreen = false;
bool CNewHud::bForceScriptHD = false;
bool CNewHud::bDebugScriptGfxDrawOrder = false;
bool CNewHud::bUpdateHelpTextWhenPaused = false;
bool CNewHud::ms_bDontRemoveDebugWidgets = false;
bool CNewHud::ms_bForceAreaDistrictStreetVehicle = false;
s32 CNewHud::iDebugComponentVisibilityDisplay = 0;
bool CNewHud::ms_bDebugHelpText = false;
bool CNewHud::ms_bLogPlayerHealth = false;
bool CNewHud::ms_bShowEverything = false;
s32 iDebugHudEnduranceValue = 0;
s32 iDebugHudMaxEnduranceValue = 0;
s32 iDebugHudArmourValue = 0;
s32 iDebugHudMaxArmourValue = 0;
s32 iDebugHudHealthValue = 0;
s32 iDebugHudMaxHealthValue = 0;
static bool s_bPrintScriptHiddenHud = false;
static int s_iQueuedShutdown = 0;
static s32 iDebugMultiplayerWalletCash = -1;
static s32 iDebugMultiplayerBankCash = -1;
static s32 iDebugMPWalletCashOverride = -1;
static s32 iDebugMPBankCashOverride = -1;
#endif // _BANK

#if !__FINAL
bool CNewHud::m_bDebugFlashWantedStars = false;
#endif

atRangeArray<CNewHud::sNewHudDataInfo, TOTAL_HUD_COMPONENT_LIST_SIZE> CNewHud::HudComponent;

CNewHud::sPreviousFrameHudValues CNewHud::PreviousHudValue;
CNewHud::eDISPLAY_MODE CNewHud::s_previousMultiplayerGameMode = CNewHud::GetInvalidDisplayMode();
CNewHud::eDISPLAY_MODE CNewHud::ms_eDisplayMode = CNewHud::DM_DEFAULT;

bool CNewHud::ms_bRestartHud = false;
s32 CNewHud::ms_iNumHudComponentsRemovedSinceLastRestartHud = 0;
bool CNewHud::ms_bIsShowingCharacterSwitch = false;
bool CNewHud::ms_bRestartHudWhenCannotRender = false;
u32 CNewHud::ms_iGpsDpadZoomTimer = 0;
bool CNewHud::ms_bHudIsHidden = false;
float CNewHud::ms_fLowestTopRightY = 0.0f;
u32 CNewHud::iHelpTextLockedWaitingForActionscriptResponse[MAX_HELP_TEXT_SLOTS];
bool CNewHud::bHelpTextLockedWaitingForActionscriptResponse[MAX_HELP_TEXT_SLOTS];
bool CNewHud::ms_bHideStreetNamesThisFrame = false;
bool CNewHud::ms_bShowHelperTextAfterPauseMenuExit = false;
s64 CNewHud::m_iMPWalletCashOverride = FAKE_CASH_NOT_IN_USE;
s64 CNewHud::m_iMPBankCashOverride = FAKE_CASH_NOT_IN_USE;
bool CNewHud::m_bSuppressCashUpdates = false;
bool CNewHud::ms_bDeviceReset = false;

bool CNewHud::ms_Initialized = false;
bool CNewHud::ms_InitializedEndurance = false;

#if SUPPORT_MULTI_MONITOR
int CNewHud::ms_iDrawBlinders = 0;
#endif // SUPPORT_MULTI_MONITOR

CWeaponWheel				CNewHud::ms_WeaponWheel;
CRadioWheel					CNewHud::ms_RadioWheel;
CNewHud::sHealthInfo		CNewHud::ms_HealthInfo;
CNewHud::sSpecialAbility	CNewHud::ms_SpecialAbility;

eHUD_COLOURS CNewHud::ms_eSPCharacterColours[MAX_SP_CHARACTER_COLOURS] = {HUD_COLOUR_MICHAEL, HUD_COLOUR_FRANKLIN, HUD_COLOUR_TREVOR};
eHUD_COLOURS CNewHud::ms_eCurrentCharacterColour = HUD_COLOUR_WHITE;

bool CNewHud::ms_bSuppressHealthFetch = false;
bool CNewHud::ms_bOverrideSpecialAbility = false;
bool CNewHud::ms_bAllowAbilityBar = true;

CHudTunablesListener* CNewHud::m_pTunablesListener = NULL;
CUIReticule CNewHud::m_reticule;

#if GTA_REPLAY
CUIReplayScaleformController CNewHud::m_replaySFController;
#endif // GTA_REPLAY

const s32 INVALID_CASH_VALUE = -1;
#define __INVALID_REPOSITION_VALUE (-999.0f)

#ifndef __SPECIAL_FLAG_DONT_DISPLAY_AMMO
#define	__SPECIAL_FLAG_DONT_DISPLAY_AMMO	(-1)
#define __SPECIAL_FLAG_USING_INFINITE_AMMO	(-2)
#endif

#define FULL_AIR_BAR (100.0f)



#define PACK_START_AND_END(start, end) (start | (end<<16))
#define UNPACK_START(input) (input & 0x0000FFFF)
#define UNPACK_END(input)   ((input >> 16) & 0x0000FFFF)


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHudTunablesListener::OnTunablesRead
// PURPOSE: Used to overload HUD variables from the cloud
/////////////////////////////////////////////////////////////////////////////////////
void CHudTunablesListener::OnTunablesRead()
{
	// the freemode colours can be updated from the cloud
	int nFreemodeColour = 0;

    // regular freemode colour
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("HUD_COLOUR_FREEMODE", 0x9c6448a0), nFreemodeColour))
		CHudColour::SetIntColour(HUD_COLOUR_FREEMODE, static_cast<u32>(nFreemodeColour));

    // freemode dark colour
    if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("HUD_COLOUR_FREEMODE_DARK", 0x75b23d78), nFreemodeColour))
        CHudColour::SetIntColour(HUD_COLOUR_FREEMODE_DARK, static_cast<u32>(nFreemodeColour));
}

//////////////////////////////////////////////////////////////////////////
CFrontendXMLMgr::XMLLoadTypeMap CFrontendXMLMgr::sm_xmlLoadMap;
bool							CFrontendXMLMgr::sm_bLoadedOnce = false;

void CFrontendXMLMgr::RegisterXMLReader(atHashWithStringBank objName, datCallback handleFunc)
{
	sm_xmlLoadMap[objName] = handleFunc;
}

void CFrontendXMLMgr::LoadXML(bool bForceReload)
{
	if(sm_bLoadedOnce && !bForceReload )
		return;

	sfDebugf3("==================================");
	sfDebugf3("==== HUD COMPONENT DATA SETUP ====");
	sfDebugf3("==================================");

#define FRONTEND_XML_FILENAME	"common:/data/ui/frontend.xml"

	INIT_PARSER;

	parTree* pTree = PARSER.LoadTree(FRONTEND_XML_FILENAME, "xml");

	if (pTree)
	{
		parTreeNode* pRootNode = pTree->GetRoot();

		parTreeNode::ChildNodeIterator pChildrenStart = pRootNode->BeginChildren();
		parTreeNode::ChildNodeIterator pChildrenEnd = pRootNode->EndChildren();

		for(parTreeNode::ChildNodeIterator ci = pChildrenStart; ci != pChildrenEnd; ++ci)
		{
			parTreeNode* curNode = *ci;
			atHashWithStringBank curNodeName(curNode->GetElement().GetName());

			#if !RSG_PC
			if (curNodeName == ATSTRINGHASH("TextEntryBox", 0x9150E275))
				continue;
			#endif

			datCallback* pReader = sm_xmlLoadMap.Access(curNodeName);
			if( uiVerifyf(pReader, "Encountered a node in frontend.xml named '%s' without a valid reader attached to it!", curNodeName.GetCStr() ) )
			{
				pReader->Call( CallbackData(curNode) );
			}
		}

		delete pTree;
	}
	else
	{
		Assertf(0, "Missing FRONTEND.XML file!");
	}


	sm_bLoadedOnce = true;
	SHUTDOWN_PARSER;

#if !__BANK
	// if there's no chance of forcing a reload, wipe out the mapping table
	sm_xmlLoadMap.Kill();
#endif

	sfDebugf3("==================================");
	sfDebugf3("== END HUD COMPONENT DATA SETUP ==");
	sfDebugf3("==================================");
}

// A lighter version of CFrontendXMLMgr::LoadXML, which allows for only specific nodes to be read from the frontend xml
// THIS IS ONLY USED FOR LOADING GAME STREAM RELEVANT DATA DURING THE INSTALL SCREEN
void CFrontendXMLMgr::LoadXMLNode(atHashWithStringBank nodeName)
{
	sfDebugf3("=========================================");
	BANK_ONLY(sfDebugf3("==== HUD COMPONENT DATA SETUP FOR %s ====", nodeName.GetCStr());)
	sfDebugf3("=========================================");

	INIT_PARSER;

	parTree* pTree = PARSER.LoadTree(FRONTEND_XML_FILENAME, "xml");

	if (pTree)
	{
		parTreeNode* pRootNode = pTree->GetRoot();

		parTreeNode::ChildNodeIterator pChildrenStart = pRootNode->BeginChildren();
		parTreeNode::ChildNodeIterator pChildrenEnd = pRootNode->EndChildren();

		for(parTreeNode::ChildNodeIterator ci = pChildrenStart; ci != pChildrenEnd; ++ci)
		{
			parTreeNode* curNode = *ci;
			atHashWithStringBank curNodeName(curNode->GetElement().GetName());

			if(curNodeName == nodeName)
			{
				datCallback* pReader = sm_xmlLoadMap.Access(curNodeName);
				if( uiVerifyf(pReader, "Encountered a node in frontend.xml named '%s' without a valid reader attached to it!", curNodeName.GetCStr() ) )
				{
					pReader->Call( CallbackData(curNode) );
				}
			}
		}

		delete pTree;
	}
	else
	{
		Assertf(0, "Missing FRONTEND.XML file!");
	}

	SHUTDOWN_PARSER;

	sfDebugf3("==================================");
	BANK_ONLY(sfDebugf3("== END HUD COMPONENT DATA SETUP FOR %s==", nodeName.GetCStr());)
	sfDebugf3("==================================");}


//////////////////////////////////////////////////////////////////////////

void CNewHud::sNewHudDataInfo::Init()
{
	bActive = false;
	bWantToResetPositionOnNextSetActive = false;
	vOverriddenPositionToUseNextSetActive = Vector2(__INVALID_REPOSITION_VALUE, __INVALID_REPOSITION_VALUE);
	bCodeThinksVisible = true;
	bInUseByActionScript = false;
	bIsPositionOverridden = false;
	bShouldShowComponentOnMpTextChatClose = false;
	iScriptRequestVisibility = VSS_NOT_CHANGING;
	iScriptRequestVisibilityPreviousFrame = VSS_NOT_CHANGING;
	iId = -1;
	iLoadingState = NOT_LOADING;
	fNoOffsetPosX = 0.0f;
	fNoOffsetRevisedPosX = 0.0f;
	DEV_ONLY( cDebugLog[0] ='\0'; )
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::Init()
// PURPOSE:	Initialises the hud at start of game.
//			Note Init() is called per level.
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		s32 iCount;

	    for (iCount = 0; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	    {
			HudComponent[iCount].Init();
	    }

		for (iCount = 0; iCount < MAX_HELP_TEXT_SLOTS; iCount++)
		{
			iHelpTextLockedWaitingForActionscriptResponse[iCount] = 0;
			bHelpTextLockedWaitingForActionscriptResponse[iCount] = false;
		}

		m_pTunablesListener = rage_new CHudTunablesListener();

		REGISTER_FRONTEND_XML_CB(CNewHud::HandleXMLComponent, "Hud",		PACK_START_AND_END(NEW_HUD,						MAX_NEW_HUD_COMPONENTS) );
		REGISTER_FRONTEND_XML_CB(CNewHud::HandleXMLComponent, "WeaponHud",	PACK_START_AND_END(WEAPON_HUD_COMPONENTS_START,	WEAPON_HUD_COMPONENTS_END) );
		REGISTER_FRONTEND_XML_CB(CNewHud::HandleXMLComponent, "ScriptHud",	PACK_START_AND_END(SCRIPT_HUD_COMPONENTS_START,	SCRIPT_HUD_COMPONENTS_END) );

#if GTA_REPLAY
		REGISTER_FRONTEND_XML_CB(CNewHud::HandleXMLComponent, "ReplayHud",	PACK_START_AND_END(REPLAY_HUD_COMPONENTS_START,	REPLAY_HUD_COMPONENTS_END) );
#else
		REGISTER_FRONTEND_XML(CFrontendXMLMgr::DoNothing, "ReplayHud");
#endif
	}
	else if(initMode == INIT_SESSION)
	{
#if __BANK
		bForceScriptWidescreen = CHudTools::GetWideScreen();
		bForceScriptHD = GRCDEVICE.GetHiDef();
		bUpdateHelpTextWhenPaused = PARAM_nohelptextpause.Get();
#endif

		LoadXmlData();  // read in the component placement xml file

		// load & activate any hud components that are independent from the hud:
		for (s32 iCount = NEW_HUD; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
		{
			if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
				continue;

			if (!IsHudComponentMovieActive(iCount) || ms_bDeviceReset)  // dont want to attempt to reactivate something that is already active on init
			{
				if (HudComponent[iCount].bStandaloneMovie)
				{
					ActivateHudComponent(true, iCount, true, (iCount != NEW_HUD));
				}
			}
		}

		ResetAllPreviousValues();

		ms_iGpsDpadZoomTimer = 0;

		InitStartingValues();

		// No need to clear the weapon wheel on a device reset as that clears game data not relevant to the display device
		if(!ms_bDeviceReset)
		{
			ms_WeaponWheel.Clear(true);
		}

		ms_bIsShowingCharacterSwitch = false;
		ms_iNumHudComponentsRemovedSinceLastRestartHud = 0;

		AllowRestartHud();  // restart at next opportunity

		if (ms_bDeviceReset)
		{
			// Restore script overrides
			for (int iComponentIndex = NEW_HUD + 1; iComponentIndex < MAX_NEW_HUD_COMPONENTS; ++iComponentIndex)
			{
				if (HudComponent[iComponentIndex].bIsPositionOverridden)
				{
					if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "OVERRIDE_COMPONENT_POSITION"))
					{
						CScaleformMgr::AddParamInt(iComponentIndex);
						CScaleformMgr::AddParamFloat(HudComponent[iComponentIndex].vOverriddenPositionToUseNextSetActive.x);
						CScaleformMgr::AddParamFloat(HudComponent[iComponentIndex].vOverriddenPositionToUseNextSetActive.y);
						CScaleformMgr::EndMethod(!HudComponent[iComponentIndex].bIsPositionOverridden);  // fix for 1880459
					}
				}
			}
		}

		ms_bDeviceReset = false;
		ms_Initialized = true;
		
#if __BANK
		ms_bLogPlayerHealth = PARAM_logPlayerHealth.Get();

		// NOTE: we must initialise the HUD component widgets separately to the other marketing widgets, as we require access to component filenames
		// that only become available once the HUD has been initialised.
		CMarketingTools::InitHudComponentWidgets();
#endif // __BANK
	}
}


void CNewHud::InitStartingValues()
{
	m_reticule.Reset();  // we should probably reset the reticle states upon initstartingvalues (on init, and on hud restarts)

	if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_WEAPON_WHEEL_ACTIVE"))
	{
		CScaleformMgr::AddParamInt(0);
		CScaleformMgr::EndMethod();  // this is instant as we are initialising the hud here
	}

	if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_MAX_HUD_COMPONENTS"))
	{
		CScaleformMgr::AddParamInt(MAX_NEW_HUD_COMPONENTS);
		CScaleformMgr::EndMethod();  // this is instant as we are initialising the hud here
	}

	if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_HUD_COMPONENT_RANGE"))
	{
		CScaleformMgr::AddParamInt(WEAPON_HUD_COMPONENTS_START);
		CScaleformMgr::AddParamInt(SCRIPT_HUD_COMPONENTS_START);
		CScaleformMgr::EndMethod();  // this is instant as we are initialising the hud here
	}
	
	CNewHud::HandleCharacterChange();

	UpdateDisplayConfig(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::Shutdown()
// PURPOSE:	Shuts down the whole of the hud
//			Note ShutdownLevel() is called per level finish.
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		ms_WeaponWheel.Clear(true);

		CHelpMessage::ClearAll();
		ClearSubtitleText();
	}

	// remove all hud components (not the base component)
	for (s32 iCount = BASE_HUD_COMPONENT; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		if (IsHudComponentActive(iCount))
		{
			if (shutdownMode == SHUTDOWN_CORE)
			{
				HudComponent[iCount].bInUseByActionScript = false;
				HudComponent[iCount].iLoadingState = REQUEST_REMOVAL_OF_MOVIE;  // we can now remove the hud component
				HudComponent[iCount].bStandaloneMovie = false;  // no longer independent from hud so it can be removed
				RemoveHudComponent(iCount);
			}
			else
			{
				RemoveHudComponentFromActionScript(iCount);
			}
		}
	}

	if (shutdownMode == SHUTDOWN_CORE)
	{
		// force the shutdown of these two components
		HudComponent[NEW_HUD_RETICLE].bInUseByActionScript = false;
		HudComponent[NEW_HUD_RETICLE].iLoadingState = REQUEST_REMOVAL_OF_MOVIE;  // we can now remove the hud component

		HudComponent[NEW_HUD].bInUseByActionScript = false;
		HudComponent[NEW_HUD].iLoadingState = REQUEST_REMOVAL_OF_MOVIE;  // we can now remove the hud component

		RemoveHudComponent(NEW_HUD_RETICLE);  // finally, remove the base component
		RemoveHudComponent(NEW_HUD);  // finally, remove the base component

		delete m_pTunablesListener;
		m_pTunablesListener = NULL;

#if __BANK
		if( !ms_bDontRemoveDebugWidgets )
			ShutdownWidgets();
#endif // __BANK
	}
	else
	{
		RemoveHudComponentFromActionScript(NEW_HUD);
	}

	if( shutdownMode == SHUTDOWN_SESSION )
	{
		Wanted_SetCopLOS(false);
	}
}

void CNewHud::Restart()
{
	// remove all hud components (not the base component)
	for (s32 iCount = BASE_HUD_COMPONENT; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		if (IsHudComponentActive(iCount))
		{
			HudComponent[iCount].bInUseByActionScript = false;
			HudComponent[iCount].iLoadingState = REQUEST_REMOVAL_OF_MOVIE;  // we can now remove the hud component
			HudComponent[iCount].bStandaloneMovie = false;  // no longer independent from hud so it can be removed
			RemoveHudComponent(iCount);
		}
	}

	// force the shutdown of these two components
	HudComponent[NEW_HUD_RETICLE].bInUseByActionScript = false;
	HudComponent[NEW_HUD_RETICLE].iLoadingState = REQUEST_REMOVAL_OF_MOVIE;  // we can now remove the hud component

	HudComponent[NEW_HUD].bInUseByActionScript = false;
	HudComponent[NEW_HUD].iLoadingState = REQUEST_REMOVAL_OF_MOVIE;  // we can now remove the hud component

	RemoveHudComponent(NEW_HUD_RETICLE);  // finally, remove the base component
	RemoveHudComponent(NEW_HUD);  // finally, remove the base component
	Wanted_SetCopLOS(false);

	// load & activate any hud components that are independent from the hud:
	for (s32 iCount = NEW_HUD; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		if (!IsHudComponentMovieActive(iCount))  // dont want to attempt to reactivate something that is already active on init
		{
			if (HudComponent[iCount].bStandaloneMovie)
			{
				ActivateHudComponent(true, iCount, true, (iCount != NEW_HUD));
			}
		}
	}

	ResetAllPreviousValues();

	ms_iGpsDpadZoomTimer = 0;

	InitStartingValues();

	ms_WeaponWheel.Clear(true);
	ms_bIsShowingCharacterSwitch = false;
	ms_iNumHudComponentsRemovedSinceLastRestartHud = 0;

	AllowRestartHud();  // restart at next opportunity

}

void CNewHud::HandleXMLComponent(s32 packedPoint, parTreeNode* pNodeRead)
{
	s32 iComponentId = UNPACK_START(packedPoint);
	s32 endingPoint = UNPACK_END(packedPoint);
	parTreeNode* pNode = NULL;

	while(((pNode = pNodeRead->FindChildWithName("component", pNode)) != NULL) &&
		(iComponentId < endingPoint))
	{

		RetrieveComponentInfo(pNode, iComponentId++);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::LoadXmlData()
// PURPOSE:	reads in the xml data and stores it
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::LoadXmlData(bool bForceReload)
{
	CFrontendXMLMgr::LoadXML(bForceReload);
}


void CNewHud::RetrieveComponentInfo(parTreeNode* pNode, s32 iComponentId)
{
	if (!pNode)
		return;

	if (HudComponent[iComponentId].bActive)  // cannot reinitialise moves that are already active - they must be inactive 1st
	{
		sfDebugf3("   Component %s (%d) NOT re-initialised as it is active", HudComponent[iComponentId].cFilename, iComponentId);
		return;
	}

	if(	pNode->GetElement().FindAttribute("name") &&
		pNode->GetElement().FindAttribute("sizeX") &&
		pNode->GetElement().FindAttribute("sizeY") &&
		pNode->GetElement().FindAttribute("hudcolor") &&
		pNode->GetElement().FindAttribute("alpha") &&
		pNode->GetElement().FindAttribute("depth") &&
		pNode->GetElement().FindAttribute("listId") &&
		pNode->GetElement().FindAttribute("listPriority") &&
		pNode->GetElement().FindAttribute("hasGfx") &&
		((pNode->GetElement().FindAttribute("posX") || pNode->GetElement().FindAttribute("alignX")) &&
		 (pNode->GetElement().FindAttribute("posY") || pNode->GetElement().FindAttribute("alignY"))))
	{
		HudComponent[iComponentId].vPos.x = 0.0f;
		HudComponent[iComponentId].vPos.y = 0.0f;
		HudComponent[iComponentId].cAlignX[0] = HudComponent[iComponentId].cAlignY[0] = 'I';  // invalid = 'I'  // set align to be invalid until we find it so we can check against this later
		HudComponent[iComponentId].cAlignX[1] = HudComponent[iComponentId].cAlignY[1] = '\0';

		safecpy(&HudComponent[iComponentId].cFilename[0], pNode->GetElement().FindAttributeAnyCase("name")->GetStringValue(), NELEM(HudComponent[iComponentId].cFilename));

		if (pNode->GetElement().FindAttribute("posX"))
			HudComponent[iComponentId].vPos.x = (float)atof(pNode->GetElement().FindAttributeAnyCase("posX")->GetStringValue());
		if (pNode->GetElement().FindAttribute("posY"))
			HudComponent[iComponentId].vPos.y = (float)atof(pNode->GetElement().FindAttributeAnyCase("posY")->GetStringValue());
		if (pNode->GetElement().FindAttribute("alignX"))
			HudComponent[iComponentId].cAlignX[0] = (pNode->GetElement().FindAttributeAnyCase("alignX")->GetStringValue())[0];
		if (pNode->GetElement().FindAttribute("alignY"))
			HudComponent[iComponentId].cAlignY[0] = (pNode->GetElement().FindAttributeAnyCase("alignY")->GetStringValue())[0];

		HudComponent[iComponentId].vSize.x = (float)atof(pNode->GetElement().FindAttributeAnyCase("sizeX")->GetStringValue());
		HudComponent[iComponentId].vSize.y = (float)atof(pNode->GetElement().FindAttributeAnyCase("sizeY")->GetStringValue());

		eHUD_COLOURS iHudColour = CHudColour::GetColourFromKey(pNode->GetElement().FindAttributeAnyCase("hudcolor")->GetStringValue());
		HudComponent[iComponentId].colour = CHudColour::GetRGB(iHudColour, (u8)(atoi(pNode->GetElement().FindAttributeAnyCase("alpha")->GetStringValue())));

		HudComponent[iComponentId].vRevisedPos = HudComponent[iComponentId].vPos;
		HudComponent[iComponentId].vRevisedSize = HudComponent[iComponentId].vSize;

		HudComponent[iComponentId].iDepth = (s32)atoi(pNode->GetElement().FindAttributeAnyCase("depth")->GetStringValue());
		HudComponent[iComponentId].iListId = (s32)atoi(pNode->GetElement().FindAttributeAnyCase("listId")->GetStringValue());
		HudComponent[iComponentId].iListPriority = (s32)atoi(pNode->GetElement().FindAttributeAnyCase("listPriority")->GetStringValue());
		HudComponent[iComponentId].bHasGfx = ((s32)atoi(pNode->GetElement().FindAttributeAnyCase("hasGfx")->GetStringValue()) == 1);
		HudComponent[iComponentId].bStandaloneMovie = ((s32)atoi(pNode->GetElement().FindAttributeAnyCase("standaloneMovie")->GetStringValue()) == 1);
		
		HudComponent[iComponentId].fNoOffsetPosX = HudComponent[iComponentId].vPos.x;
		HudComponent[iComponentId].fNoOffsetRevisedPosX = HudComponent[iComponentId].vRevisedPos.x;

		sfDebugf3("   Component %s (%d) initialised", HudComponent[iComponentId].cFilename, iComponentId);
	}
	else
	{
		Assertf(0, "[FRONTEND.XML] Hud component is missing correct attributes");
	}
}

void CNewHud::UpdateDisplayConfig(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass)
{
	if( CScaleformMgr::IsMovieIdInRange( iIndex ) )
	{
		float fSafeZoneX[2], fSafeZoneY[2];
		CHudTools::GetMinSafeZoneForScaleformMovies(fSafeZoneX[0], fSafeZoneY[0], fSafeZoneX[1], fSafeZoneY[1]);

		ms_fLowestTopRightY = fSafeZoneY[0];

		CScaleformMgr::SDC flags = CScaleformMgr::SDC::Default;
		if (CScaleformMgr::ShouldUseWidescreen(iIndex))
			flags |= CScaleformMgr::SDC::ForceWidescreen;

		CScaleformMgr::SetMovieDisplayConfig(iIndex, iBaseClass, flags);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ActivateHudComponent
// PURPOSE:	loads a hud component
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ActivateHudComponent(bool bActivatedByActionScript, s32 iComponentId, bool bLoadInstantly, bool bChildMovie)
{
	// we need to ignore calls to activate it if its already in a state of being activated since script want to spam it as per bug 1092509
	if ( (!bActivatedByActionScript) && (HudComponent[iComponentId].iLoadingState == WAITING_ON_SETUP_BY_ACTIONSCRIPT) )
	{
		return;
	}

	HudComponent[iComponentId].iLoadingState = NOT_LOADING;

	//OffsetHudComponentForCurrentResolution(iComponentId);

	if (HudComponent[iComponentId].bHasGfx)
	{
		s32 iParentId = -1;
		if (bChildMovie)
		{
			iParentId = HudComponent[NEW_HUD].iId;
		}

		CScaleformMgr::CreateMovieParams createMovieParams(HudComponent[iComponentId].cFilename);
		createMovieParams.vPos			= Vector3(HudComponent[iComponentId].vPos.x, HudComponent[iComponentId].vPos.y, 0.0f);
		createMovieParams.vSize			= HudComponent[iComponentId].vSize;
		createMovieParams.bRemovable	= false;
		createMovieParams.iParentMovie	= iParentId;
		createMovieParams.bRequiresMovieView = HudComponent[iComponentId].bStandaloneMovie;
		createMovieParams.eScaleMode	= GetHudScaleMode(iComponentId);

		if (HudComponent[iComponentId].bStandaloneMovie)
		{
			 const Vector2 vStandaloneMoviePos = CHudTools::CalculateHudPosition(Vector2(createMovieParams.vPos.x, createMovieParams.vPos.y), createMovieParams.vSize, HudComponent[iComponentId].cAlignX[0], HudComponent[iComponentId].cAlignY[0]);
			 createMovieParams.vPos = Vector3(vStandaloneMoviePos.x, vStandaloneMoviePos.y, 0.0f);
		}

		if (bLoadInstantly)
		{
			HudComponent[iComponentId].iId = CScaleformMgr::CreateMovieAndWaitForLoad(createMovieParams);  // for the moment, stream these but load instantly
		}
		else
		{
			HudComponent[iComponentId].iId = CScaleformMgr::CreateMovie(createMovieParams);  // for the moment, stream these but load instantly
		}
		// bail if the movie can't/didn't load
		if(HudComponent[iComponentId].iId == -1)
			return;
	}

	if ( (iComponentId == NEW_HUD) || ((!HudComponent[iComponentId].bStandaloneMovie) || (!bLoadInstantly)) )
	{
		if (iComponentId != NEW_HUD )
		{
			if (bActivatedByActionScript)
			{
				HudComponent[iComponentId].iLoadingState = LOADING_REQUEST_BY_ACTIONSCRIPT;
			}
			else
			{
				HudComponent[iComponentId].iLoadingState = LOADING_REQUEST_NOT_BY_ACTIONSCRIPT;
			}
		}

		HudComponent[iComponentId].bActive = bActivatedByActionScript;

		AllowRestartHud();  // a hud component has been made active, so we will want to restart at next opportunity

#if __DEV
		sfDebugf1("SF HUD: Hud component %s %d now active", HudComponent[iComponentId].cFilename, iComponentId);
#endif
	}
}



void CNewHud::ResetVisibilityPerFrame(s32 iComponentId)
{
	switch( HudComponent[iComponentId].iScriptRequestVisibility )
	{
	case VSS_SHOW_THIS_FRAME:
	case VSS_SHOW_THIS_FRAME_FIRST_TIME:
		HudComponent[iComponentId].iScriptRequestVisibility = VSS_SHOWN_LAST_FRAME;
		break;

	case VSS_HIDE_THIS_FRAME:
	case VSS_HIDE_THIS_FRAME_FIRST_TIME:
		HudComponent[iComponentId].iScriptRequestVisibility = VSS_HIDDEN_LAST_FRAME;
		break;

	default:
		HudComponent[iComponentId].iScriptRequestVisibility = VSS_NOT_CHANGING;
	}

	// store the state of this frame so we can compare it next frame
	HudComponent[iComponentId].iScriptRequestVisibilityPreviousFrame = HudComponent[iComponentId].iScriptRequestVisibility;
}

void CNewHud::SetHudComponentToBeShown(s32 iId)
{
	switch( HudComponent[iId].iScriptRequestVisibility )
	{
		case VSS_SHOW_THIS_FRAME:
		case VSS_SHOWN_LAST_FRAME:
			HudComponent[iId].iScriptRequestVisibility = VSS_SHOW_THIS_FRAME;
			break;
		
		default:
			HudComponent[iId].iScriptRequestVisibility = VSS_SHOW_THIS_FRAME_FIRST_TIME;
	}
#if __BANK
	if(scrThread::GetActiveThread() && s_bPrintScriptHiddenHud)
		grcDebugDraw::AddDebugOutput("%s wants to show hud component %s", scrThread::GetActiveThread()->GetScriptName(), HudComponent[iId].cFilename);
#endif
}

bool CNewHud::IsHelpTextHiddenByScriptThisFrame(s32 iIndex)
{
	if (iIndex < MAX_HELP_TEXT_SLOTS)
	{
#if RSG_PC
		if(ioKeyboard::ImeIsInProgress() && iIndex == 0)
		{
			return false;
		}
#endif // RSG_PC

		return (HudComponent[NEW_HUD_HELP_TEXT+iIndex].iScriptRequestVisibility == VSS_HIDE_THIS_FRAME ||
				HudComponent[NEW_HUD_HELP_TEXT+iIndex].iScriptRequestVisibility == VSS_HIDE_THIS_FRAME_FIRST_TIME);
	}
	else
	{
		return false;
	}
}

void CNewHud::SetHudComponentToBeHidden(s32 iId)
{
#if RSG_PC
	if(ioKeyboard::ImeIsInProgress())
	{
		if(iId == NEW_HUD_HELP_TEXT)
			return;
	}
#endif // RSG_PC
	switch( HudComponent[iId].iScriptRequestVisibility )
	{
		case VSS_HIDE_THIS_FRAME:
		case VSS_HIDDEN_LAST_FRAME:
			HudComponent[iId].iScriptRequestVisibility = VSS_HIDE_THIS_FRAME;
			break;

		default:
			HudComponent[iId].iScriptRequestVisibility = VSS_HIDE_THIS_FRAME_FIRST_TIME;
	}
#if __BANK
	if(scrThread::GetActiveThread() && s_bPrintScriptHiddenHud)
		grcDebugDraw::AddDebugOutput("%s has hidden hud component %s", scrThread::GetActiveThread()->GetScriptName(), HudComponent[iId].cFilename);
#endif
}

bool CNewHud::DoesScriptWantToForceShowThisFrame(s32 iComponentId, bool bFirstOnly)
{
#if RSG_PC
	if(ioKeyboard::ImeIsInProgress())
	{
		if(iComponentId == NEW_HUD_HELP_TEXT)
			return true;
	}
#endif // RSG_PC
	return ( HudComponent[iComponentId].iScriptRequestVisibility == VSS_SHOW_THIS_FRAME && !bFirstOnly )
		  || HudComponent[iComponentId].iScriptRequestVisibility == VSS_SHOW_THIS_FRAME_FIRST_TIME;
}

bool CNewHud::DoesScriptWantToForceHideThisFrame(s32 iComponentId, bool bFirstOnly)
{
#if RSG_PC
	if(ioKeyboard::ImeIsInProgress())
	{
		if(iComponentId == NEW_HUD_HELP_TEXT)
			return false;
	}
#endif // RSG_PC

	return ( HudComponent[iComponentId].iScriptRequestVisibility == VSS_HIDE_THIS_FRAME && !bFirstOnly)
		  || HudComponent[iComponentId].iScriptRequestVisibility == VSS_HIDE_THIS_FRAME_FIRST_TIME;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::RemoveHudComponentFromActionScript
// PURPOSE:	removes a hud component
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::RemoveHudComponentFromActionScript(s32 iComponentId)
{
	if (iComponentId != NEW_HUD && iComponentId != NEW_HUD_RETICLE)  // we never remove the hud movie during play and also now, the reticle
	{
		HudComponent[iComponentId].iLoadingState = WAITING_ON_REMOVAL_BY_ACTIONSCRIPT;  // we now wait on a response from Actionscript

		if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "REMOVE_HUD_ITEM"))
		{
			CScaleformMgr::AddParamInt(iComponentId);
			CScaleformMgr::AddParamBool(true);
			CScaleformMgr::EndMethod();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::RemoveHudComponent
// PURPOSE:	removes a hud component
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::RemoveHudComponent(s32 iComponentId)
{
#if __DEV
	//copy these as they will be removed before we output the debug log
	char cTempFilename[40];
	safecpy(cTempFilename, HudComponent[iComponentId].cFilename, 40);
	s32 iTempId = HudComponent[iComponentId].iId;
#endif  // DEV

	if ( (HudComponent[iComponentId].bHasGfx) 
		&& (IsHudComponentMovieActive(iComponentId)) 
		&& (!HudComponent[iComponentId].bInUseByActionScript) 
		&& (   HudComponent[iComponentId].iLoadingState == REQUEST_REMOVAL_OF_MOVIE 
			|| HudComponent[iComponentId].iLoadingState == NOT_LOADING) )
	{

		if (HudComponent[iComponentId].iId != -1)  // ensure we have a movie set up to remove
		{
			if (!HudComponent[iComponentId].bStandaloneMovie BANK_ONLY(|| ms_bDontRemoveDebugWidgets))
			{
				CScaleformMgr::RequestRemoveMovie(HudComponent[iComponentId].iId);
				HudComponent[iComponentId].iId = -1;
			}
		}
	}

	HudComponent[iComponentId].bInUseByActionScript = false;

	HudComponent[iComponentId].iLoadingState = NOT_LOADING;

#if __DEV
	HudComponent[iComponentId].cDebugLog[0] = '\0';
#endif // __DEV

	HudComponent[iComponentId].bActive = false;

	if (iComponentId != NEW_HUD)
	{
//  		if (!HudComponent[iComponentId].bHasGfx)  // force garbage collection here if we dont have a gfx to remove
//  		{
//  			CScaleformMgr::ForceCollectGarbage(HudComponent[NEW_HUD].iId);  // set to force garbage collection on RT
//  		}

#if __DEV
		if (iTempId != -1)
		{
			sfDebugf3("SF HUD: Hud has deleted component %s with the movie id %d requested to be removed", cTempFilename, iTempId);
		}
		else
		{
			sfDebugf3("SF HUD: Hud has deleted component %s", cTempFilename);
		}
#endif  // DEV
	}

	ms_iNumHudComponentsRemovedSinceLastRestartHud++;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::IsHudComponentMovieActive
// PURPOSE:	returns whether the individual hud component is currently loaded & active
/////////////////////////////////////////////////////////////////////////////////////
bool CNewHud::IsHudComponentMovieActive(s32 iComponentId)
{
	if (HudComponent[iComponentId].bHasGfx)
	{
		return ( (CScaleformMgr::IsMovieActive(HudComponent[iComponentId].iId)) || (HudComponent[iComponentId].iLoadingState != NOT_LOADING) );
	}
	else
	{
		return IsHudComponentActive(iComponentId);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::IsHudComponentActive
// PURPOSE:	returns whether the individual hud component is currently loaded & active
/////////////////////////////////////////////////////////////////////////////////////
bool CNewHud::IsHudComponentActive(s32 iComponentId)
{
	return (HudComponent[iComponentId].bActive);
}


void CNewHud::SendComponentValuesToActionScript(s32 iComponentId)
{
	// send the info back to ActionScript:
	if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_COMPONENT_VALUES"))
	{
		CScaleformMgr::AddParamInt(iComponentId);
		CScaleformMgr::AddParamString(HudComponent[iComponentId].cFilename);
		
		CScaleformMgr::AddParamString(HudComponent[iComponentId].cAlignX);
		CScaleformMgr::AddParamString(HudComponent[iComponentId].cAlignY);
		CScaleformMgr::AddParamFloat(HudComponent[iComponentId].vSize.x);
		CScaleformMgr::AddParamFloat(HudComponent[iComponentId].vSize.y);
		CScaleformMgr::AddParamInt(HudComponent[iComponentId].colour.GetRed());
		CScaleformMgr::AddParamInt(HudComponent[iComponentId].colour.GetGreen());
		CScaleformMgr::AddParamInt(HudComponent[iComponentId].colour.GetBlue());
		CScaleformMgr::AddParamFloat((float)HudComponent[iComponentId].colour.GetAlphaf() * 100.0f);
		CScaleformMgr::AddParamInt(HudComponent[iComponentId].iDepth);
		CScaleformMgr::AddParamInt(HudComponent[iComponentId].iListId);
		CScaleformMgr::AddParamInt(HudComponent[iComponentId].iListPriority);
		CScaleformMgr::AddParamBool(HudComponent[iComponentId].bHasGfx);
		CScaleformMgr::AddParamFloat(HudComponent[iComponentId].vPos.x);
		CScaleformMgr::AddParamFloat(HudComponent[iComponentId].vPos.y);

		CScaleformMgr::EndMethod();
	}

	// if its being overriden, then call this straight after set component values to make sure AS has latest values as this is an offset of the above sent in positions
	if (HudComponent[iComponentId].vOverriddenPositionToUseNextSetActive.x != __INVALID_REPOSITION_VALUE && HudComponent[iComponentId].vOverriddenPositionToUseNextSetActive.y != __INVALID_REPOSITION_VALUE)  // if we want to reset the position but it wasnt active when the command was called, do it now on re-activate
	{
		if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "OVERRIDE_COMPONENT_POSITION"))
		{
			CScaleformMgr::AddParamInt(iComponentId);
			CScaleformMgr::AddParamFloat(HudComponent[iComponentId].vOverriddenPositionToUseNextSetActive.x);
			CScaleformMgr::AddParamFloat(HudComponent[iComponentId].vOverriddenPositionToUseNextSetActive.y);
			CScaleformMgr::EndMethod();
		}
	}

}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateAtEndOfFrame
// PURPOSE:	various stuff that needs to be done in the 'safe area' at the end of the
//			frame, like removing hud items
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateAtEndOfFrame()
{
////#if GTA_REPLAY
////	if (CReplayMgr::IsEditModeActive() && !CUIReplayMgr::IsInitialized())
////	{
////		CUIReplayMgr::InitSession(true);
////	}
////	else if (!CReplayMgr::IsEditModeActive() && CUIReplayMgr::IsInitialized())
////	{
////		CUIReplayMgr::ShutdownSession();
////	}
////#endif // GTA_REPLAY

#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		if (m_replaySFController.IsRunningClip() && m_replaySFController.ChangedStateThisFrame())
		{
			for (s32 hudComponentIndex = 1; hudComponentIndex < MAX_NEW_HUD_COMPONENTS; ++hudComponentIndex)
			{
				if (hudComponentIndex == NEW_HUD_HELP_TEXT || hudComponentIndex == NEW_HUD_RETICLE)
					continue;

				RemoveHudComponentFromActionScript(hudComponentIndex);
			}
		}
	}
#endif

	for (s32 iCount = BASE_HUD_COMPONENT; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		switch (HudComponent[iCount].iLoadingState)
		{
			case REQUEST_REMOVAL_OF_MOVIE:
			{
				RemoveHudComponent(iCount);  // remove the hud component and movie completely

				break;
			}

			case LOADING_REQUEST_TO_CREATE_MOVIE:
			{
				ActivateHudComponent(true, iCount, false, true);  // streaming has started
				break;
			}
		
			case LOADING_REQUEST_BY_ACTIONSCRIPT:
			case LOADING_REQUEST_NOT_BY_ACTIONSCRIPT:
			{
				// check to see if its loaded yet:
				if ( (!HudComponent[iCount].bHasGfx) || (CScaleformMgr::IsMovieActive(HudComponent[iCount].iId)) )
				{
	#if __DEV
					sfDebugf1("SF HUD: Component values sent to movie '%s' which is now activated", HudComponent[iCount].cFilename);
	#endif
					SendComponentValuesToActionScript(iCount);

					if (HudComponent[iCount].iLoadingState == LOADING_REQUEST_BY_ACTIONSCRIPT)
					{
						HudComponent[iCount].iLoadingState =  NOT_LOADING;
					}
					else
					{
						HudComponent[iCount].iLoadingState = WAITING_ON_SETUP_BY_ACTIONSCRIPT;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}

#if __DEV
		if (bDisplayDebugOutput)
		{
			if (/*HudComponent[iCount].iId != -1 && */iCount != NEW_HUD)
			{
				GFxValue retValue;

				if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "GET_COMPONENT_STATUS"))
				{
					CScaleformMgr::AddParamInt(iCount);
					retValue = CScaleformMgr::EndMethodGfxValue(true);
				}

				if (retValue.IsString())
				{
					if (strcmpi(HudComponent[iCount].cDebugLog, retValue.GetString()))
					{
						safecpy(HudComponent[iCount].cDebugLog, retValue.GetString(), NELEM(HudComponent[iCount].cDebugLog));
						sfDisplayf("SF HUD: Movie status: %s (%d) = %s", HudComponent[iCount].cFilename, iCount, HudComponent[iCount].cDebugLog);
					}
				}
			}
		}
#endif
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateMultiplayerInformation
// PURPOSE:	updates the hud with any information from MP
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateMultiplayerInformation()
{
	if (PreviousHudValue.bMultiplayerStatus != NetworkInterface::IsGameInProgress())
	{
		PreviousHudValue.bMultiplayerStatus = NetworkInterface::IsGameInProgress();

		// hud:
		if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "MULTIPLAYER_IS_ACTIVE"))
		{
			CScaleformMgr::AddParamBool(PreviousHudValue.bMultiplayerStatus);
			CScaleformMgr::EndMethod();
		}		

		// also minimap:
		if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "MULTIPLAYER_IS_ACTIVE"))
		{
			CScaleformMgr::AddParamBool(PreviousHudValue.bMultiplayerStatus);
			CScaleformMgr::EndMethod();
		}

		if ( ms_bOverrideSpecialAbility )
		{
			SetToggleAbilityBar(ms_bOverrideSpecialAbility);
		}
	}

	
}


bool CNewHud::ShouldHudComponentBeShownWhenHudIsHidden(s32 iHudComponentIndex)
{
	//
	// BEFORE CHANGING WHAT THIS RETURNS - we must not have any state checks in here - the visibility should be done elsewhere
	//

	// always show help text when hud is hidden:
	for (s32 iCount = 0; iCount < MAX_HELP_TEXT_SLOTS; iCount++)
	{
		if ( (NEW_HUD_HELP_TEXT+iCount) == iHudComponentIndex)
		{
			return true;
		}
	}

	// always show subtitle text when hud is hidden:
	if (NEW_HUD_SUBTITLE_TEXT == iHudComponentIndex)
	{
		return true;
	}

	// always show saving spinner when hud is hidden:
	if (NEW_HUD_SAVING_GAME == iHudComponentIndex)
	{
		return true;
	}

	if (NEW_HUD_RETICLE == iHudComponentIndex)
	{
		return true;
	}

	// B* 1022838: Show even if the HUD is turned off
	if( NEW_HUD_RADIO_STATIONS == iHudComponentIndex
		|| NEW_HUD_WEAPON_WHEEL == iHudComponentIndex
		|| NEW_HUD_WEAPON_WHEEL_STATS == iHudComponentIndex )
		return true;

	if (iHudComponentIndex >= MAX_NEW_HUD_COMPONENTS)
	{
		return true;
	}

	return false;
}


void CNewHud::ShowHudComponent(s32 iHudComponentIndex)
{
	if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SHOW"))
	{
		CScaleformMgr::AddParamInt(iHudComponentIndex);
		CScaleformMgr::EndMethod();
	}

	HudComponent[iHudComponentIndex].bCodeThinksVisible = true;
	HudComponent[iHudComponentIndex].bShouldShowComponentOnMpTextChatClose = true;
}


void CNewHud::HideHudComponent(s32 iHudComponentIndex)
{
	if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "HIDE"))
	{
		CScaleformMgr::AddParamInt(iHudComponentIndex);
		CScaleformMgr::EndMethod();
	}

	HudComponent[iHudComponentIndex].bCodeThinksVisible = false;
	HudComponent[iHudComponentIndex].bShouldShowComponentOnMpTextChatClose = false;
}

bool CNewHud::ShouldHideHud(bool checkSettings)
{
	return (!CScriptHud::bDisplayHud) ||
		(gVpMan.AreWidescreenBordersActive()) ||
		(CScriptHud::bDontDisplayHudOrRadarThisFrame) ||
		(checkSettings && !CPauseMenu::GetMenuPreference(PREF_DISPLAY_HUD));

}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateVisibility
// PURPOSE:	turns on/off hud components based on script/game flags
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateVisibility()
{
	// hide the area name if the busy spinner is on - fixes 1567346, 1568609 - reuse the script flag here if its not already being hidden by script
	if ( (CBusySpinner::IsOn()) && (!DoesScriptWantToForceHideThisFrame(NEW_HUD_AREA_NAME)) )
	{
		CNewHud::SetHudComponentToBeHidden(NEW_HUD_AREA_NAME);
	}



	if ( ShouldHideHud(true))
	{
		if (!ms_bHudIsHidden)
		{
			CScaleformMgr::CallMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "HIDE_ALL");

			for (s32 iCount = 0; iCount < MAX_NEW_HUD_COMPONENTS; iCount++)  // HIDE_ALL in as only deals with code hud components
			{
				if (ShouldHudComponentBeShownWhenHudIsHidden(iCount))
				{
					ShowHudComponent(iCount);
				}
				else
				{
					HudComponent[iCount].bCodeThinksVisible = false;
					HudComponent[iCount].bShouldShowComponentOnMpTextChatClose = false;
				}
			}

			ms_bHudIsHidden = true;
		}
	}
	else
	{
		if (ms_bHudIsHidden)
		{
			CScaleformMgr::CallMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SHOW_ALL");

			for (s32 iCount = 0; iCount < MAX_NEW_HUD_COMPONENTS; iCount++)  // SHOW_ALL in as only deals with code hud components
			{
				HudComponent[iCount].bCodeThinksVisible = true;
				HudComponent[iCount].bShouldShowComponentOnMpTextChatClose = true;
			}

			ms_bHudIsHidden = false;
		}
	}

	// this individual component on/off must be done after the whole hud is added/removed above as it overwrites it
	// (ie the hud can be removed, but 1 hud component forced on)
	for (s32 iCount = 0; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		switch( HudComponent[iCount].iScriptRequestVisibility )
		{
			//////////////////////////////////////////////////////////////////////////
			case VSS_HIDE_THIS_FRAME_FIRST_TIME:
			case VSS_HIDE_THIS_FRAME:
			{
				if ( (HudComponent[iCount].iScriptRequestVisibilityPreviousFrame != VSS_HIDDEN_LAST_FRAME) || (HudComponent[iCount].bCodeThinksVisible) )  // hide it
				{
	#if __BANK
					if (iDebugComponentVisibilityDisplay == iCount)
					{
						sfDisplayf("calling HIDE on %d %s as script want to force it hidden", iDebugComponentVisibilityDisplay, HudComponent[iDebugComponentVisibilityDisplay].cFilename);
					}
	#endif // __BANK

					HideHudComponent(iCount);
				}
			}
			break;

			//////////////////////////////////////////////////////////////////////////
			case VSS_HIDDEN_LAST_FRAME:   // show it
			{
				if ( (!ms_bHudIsHidden) || (ShouldHudComponentBeShownWhenHudIsHidden(iCount)) )
				{	// only display again after turning off per frame if the hud isn't hidden
					//	components which should be shown even when the hud is hidden (such as help text) are always switched back on here

					if ( (HudComponent[iCount].iScriptRequestVisibilityPreviousFrame != HudComponent[iCount].iScriptRequestVisibility) || (!HudComponent[iCount].bCodeThinksVisible) )
					{
	#if __BANK
						if (iDebugComponentVisibilityDisplay == iCount)
						{
							sfDisplayf("calling SHOW on %d %s as script no longer want to force it hidden", iDebugComponentVisibilityDisplay, HudComponent[iDebugComponentVisibilityDisplay].cFilename);
						}
	#endif // __BANK

						ShowHudComponent(iCount);
					}
				}
			}
			break;

			//////////////////////////////////////////////////////////////////////////
			case VSS_SHOW_THIS_FRAME_FIRST_TIME:
			case VSS_SHOW_THIS_FRAME:
			{
				if ( (HudComponent[iCount].iScriptRequestVisibilityPreviousFrame != VSS_SHOWN_LAST_FRAME) || (!HudComponent[iCount].bCodeThinksVisible) )
				{
		#if __BANK
					if (iDebugComponentVisibilityDisplay == iCount)
					{
						sfDisplayf("calling SHOW on %d %s as script want to force it displayed", iDebugComponentVisibilityDisplay, HudComponent[iDebugComponentVisibilityDisplay].cFilename);
					}
		#endif // __BANK

					ShowHudComponent(iCount);
				}
			}
			break;

			//////////////////////////////////////////////////////////////////////////
			case VSS_SHOWN_LAST_FRAME:   // hide it
			{
				if ( (HudComponent[iCount].iScriptRequestVisibilityPreviousFrame != HudComponent[iCount].iScriptRequestVisibility) || (HudComponent[iCount].bCodeThinksVisible) )
				{
					if (ms_bHudIsHidden)  // only hide again after turning off per frame if the hud IS hidden
					{
		#if __BANK
						if (iDebugComponentVisibilityDisplay == iCount)
						{
							sfDisplayf("calling HIDE on %d %s as script want to stop forcing it to be shown", iDebugComponentVisibilityDisplay, HudComponent[iDebugComponentVisibilityDisplay].cFilename);
						}
		#endif // __BANK

						if(iCount != NEW_HUD_RETICLE)
							HideHudComponent(iCount);
					}
				}
			}
			default:
			break;
		} // end of switch
	} // end of for

#if __BANK
	if (iDebugComponentVisibilityDisplay != 0)
	{
		char cDebugString[300];
		
		if (HudComponent[iDebugComponentVisibilityDisplay].bCodeThinksVisible)
		{
			sprintf(cDebugString, "HUD Component %s (%d) SHOULD DISPLAY - HudVisible:%s  CodeThinksVisible:%s  ScriptWantsToShow:%s  ScriptWantsToHide:%s",	HudComponent[iDebugComponentVisibilityDisplay].cFilename,
						iDebugComponentVisibilityDisplay, 
						(!ms_bHudIsHidden)?"YES":"NO",
						(HudComponent[iDebugComponentVisibilityDisplay].bCodeThinksVisible)?"YES":"NO",
						(DoesScriptWantToForceShowThisFrame(iDebugComponentVisibilityDisplay))?"YES":"NO",
						(DoesScriptWantToForceHideThisFrame(iDebugComponentVisibilityDisplay))?"YES":"NO");
		}
		else
		{
			sprintf(cDebugString, "HUD Component %s (%d) SHOULD NOT DISPLAY - HudVisible:%s  CodeThinksVisible:%s  ScriptWantsToShow:%s  ScriptWantsToHide:%s",	HudComponent[iDebugComponentVisibilityDisplay].cFilename,
						iDebugComponentVisibilityDisplay, 
						(!ms_bHudIsHidden)?"YES":"NO",
						(HudComponent[iDebugComponentVisibilityDisplay].bCodeThinksVisible)?"YES":"NO",
						(DoesScriptWantToForceShowThisFrame(iDebugComponentVisibilityDisplay))?"YES":"NO",
						(DoesScriptWantToForceHideThisFrame(iDebugComponentVisibilityDisplay))?"YES":"NO");
		}
		sfDisplayf("%s", cDebugString);
	}
#endif // __BANK
}

void CNewHud::ResetAllPerFrameVisibilities()
{
	// reset any "per frame" flags:
	for (s32 iCount = 0; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		ResetVisibilityPerFrame(iCount);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::Update()
// PURPOSE:	main update for the hud
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::Update()
{
#if __BANK
	if( s_iQueuedShutdown != 0 )
	{
		ms_bDontRemoveDebugWidgets = true;
		if( s_iQueuedShutdown == 1 )
		{
			Shutdown(SHUTDOWN_SESSION);
			s_iQueuedShutdown = 2;
		}
		else
		{
			Shutdown(SHUTDOWN_CORE);
			s_iQueuedShutdown = 0;
		}
		ms_bDontRemoveDebugWidgets = false;

		return;
	}
#endif

#if SUPPORT_MULTI_MONITOR
	if(ShouldDrawBlinders())
		ms_iDrawBlinders--;
#endif	//SUPPORT_MULTI_MONITOR

	if (!CanRenderHud())
	{
		if (!ms_bRestartHudWhenCannotRender)
		{
			if (ms_bRestartHud)
			{
				if (camInterface::IsFadedOut() || (CPauseMenu::IsActive() && (!CNetwork::IsGameInProgress())))  // only restart HUD when game fades to black or is in Single Player pause
				{
					sfDebugf3("HUD: RestartHud: Attempting to restart full hud when hud is not being rendered");
					RestartHud(true);

					ms_bRestartHudWhenCannotRender = true;
				}
			}
		}
	}
	else
	{
		ms_bRestartHudWhenCannotRender = false;
	}

	// do not UPDATE the hud when game is paused as the scaleform movies will advance - but we do need to update if in MP.
	// But also if script is calling DISPLAY_HUD_WHEN_PAUSED_THIS_FRAME. Need that part too.
	bool doUpdateWhenPaused = CPauseMenu::IsActive() && !NetworkInterface::IsGameInProgress() && !CScriptHud::ms_bDisplayHudWhenPausedThisFrame.GetUpdateBuf();

#if GTA_REPLAY
	if ( CVideoEditorInterface::IsActive() )// && fwTimer::IsGamePaused())
	{
		// mini update for HUD when in replay ...just for components that can run in replay
		if (!doUpdateWhenPaused)
		{
			m_reticule.Update(DoesScriptWantToForceShowThisFrame(NEW_HUD_RETICLE), false);
			m_replaySFController.Update();
			UpdateVisibility();
		}
		ResetAllPerFrameVisibilities();

		if( CVideoEditorInterface::CanUpdateHelpText() )
		{
			UpdateHelpText();
		}
	}
#endif // GTA_REPLAY

	UpdateIMEVisibility();

	if (doUpdateWhenPaused)
	{
		UpdateWhenPaused();
		return;
	}

	if (fwTimer::IsGamePaused())
	{
#if GTA_REPLAY
		if (CVideoEditorInterface::IsActive())
		{
			UpdateHelpText();
		}
#elif __BANK
		// special support for updating help text when paused. Initially for marketing use
		if (bUpdateHelpTextWhenPaused)
		{
			UpdateHelpText();
		}
#endif // GTA_REPLAY

		return;
	}

#if GTA_REPLAY
	// now HUD plays on replay, early out now ...we have a smaller update already run for replay
	if (CVideoEditorInterface::IsActive())
	{
		return;
	}
#endif // GTA_REPLAY

	if (!HudComponent[NEW_HUD].bActive)
		return;

	UpdateDpadDown();

	UpdateVisibility();

	UpdateMultiplayerInformation();

	// update the movies and tell ActionScript the new values:
	UpdateHealthInfo(false);

	UpdateMultiplayerCash();  // let this update with the "off" value in SP to make sure stuff is turned off (fixes 1566273)
	UpdateSinglePlayerCash();
	UpdateGameStream();
	UpdateWantedStars();
	UpdateMiniMapOverlay();

	bool bShouldHideHud = ShouldHideHud(false);
	ms_WeaponWheel.Update(DoesScriptWantToForceShowThisFrame(NEW_HUD_WEAPON_WHEEL), bShouldHideHud);
	UpdateWeaponAndAmmo();

	ms_RadioWheel.Update(DoesScriptWantToForceShowThisFrame(NEW_HUD_RADIO_STATIONS), bShouldHideHud);
	UpdateAbility();
	UpdateAirBar();
	
	m_reticule.Update(DoesScriptWantToForceShowThisFrame(NEW_HUD_RETICLE), bShouldHideHud);
#if GTA_REPLAY
	m_replaySFController.Update();
#endif // GTA_REPLAY

	if(!ShouldHideHud(true))
	{
		UpdateAreaDistrictStreetVehicleNames();
	}

	UpdateHelpText();
	UpdateForMpChat();

	// Should be last so we know the frames have been touched
	ResetAllPerFrameVisibilities();
}

void CNewHud::UpdateWhenPaused()
{
	Wanted_SetCopLOS(false);

	m_reticule.Update(DoesScriptWantToForceShowThisFrame(NEW_HUD_RETICLE), ShouldHideHud(false));
#if GTA_REPLAY
	m_replaySFController.Update();
#endif // GTA_REPLAY
	ms_WeaponWheel.Update(false,true);
	ms_RadioWheel.Update(false,true);

	UpdateVisibility();  // still want to update visibility; script may want to hide stuff  - fix for 1493829

#if RSG_PC
	if(ioKeyboard::ImeIsInProgress())
	{
		UpdateHelpText();
	}
#endif // RSG_PC

	CScaleformMgr::ForceMovieUpdateInstantly(HudComponent[NEW_HUD].iId, false);  // ensure the movie stops updating
}

void CNewHud::GetHealthInfo(const CPhysical* pEntity, sHealthInfo& healthInfo)
{
	if (!pEntity)
		return;

	s32 iHealth = (s32)ceil(pEntity->GetHealth());
	s32 iMaxHealth = (s32)ceil(pEntity->GetMaxHealth());

	s32 iArmour = 0;
	s32 iMaxArmour = 100;

	s32 iEndurance = 0;
	s32 iMaxEndurance = 100;
	
	s32 iTeam = 0;

	s32 iHudMaxHealthDisplay = CScriptHud::iHudMaxHealthDisplay;
	s32 iHudMaxArmourDisplay = CScriptHud::iHudMaxArmourDisplay;
	s32 iHudMaxEnduranceDisplay = CScriptHud::iHudMaxEnduranceDisplay;

	const CPed* pPed = NULL;

	if(pEntity->GetIsTypePed())
	{
		iHealth -= 100;
		iMaxHealth -= 100;
		iHudMaxHealthDisplay -= 100;

		pPed = reinterpret_cast<const CPed*>(pEntity);		
		
		const CPlayerInfo * pPlayerInfo = pPed->GetPlayerInfo();

		if(pPlayerInfo)
		{
			iTeam = (s32)pPlayerInfo->GetArcadeInformation().GetTeam();
			iArmour = (s32)ceil(pPed->GetArmour());
			iMaxArmour = (s32)pPlayerInfo->MaxArmour;

			iEndurance = (s32)pPed->GetEndurance();
			iMaxEndurance = (s32)pPed->GetMaxEndurance();
		}
	}

#if __BANK
	if (iDebugHudHealthValue != 0)
		iHealth = iDebugHudHealthValue;

	if (iDebugHudMaxHealthValue != 0)
		iMaxHealth = iDebugHudMaxHealthValue;

	if (iDebugHudArmourValue != 0)
		iArmour = iDebugHudArmourValue;

	if (iDebugHudMaxArmourValue != 0)
		iMaxArmour = iDebugHudMaxArmourValue;

	if (iDebugHudEnduranceValue != 0)
		iEndurance = iDebugHudEnduranceValue;

	if (iDebugHudMaxEnduranceValue != 0)
		iMaxEndurance = iDebugHudMaxEnduranceValue;
#endif // __BANK

#if __BANK
	if (iDebugHudHealthValue == 0 && iDebugHudArmourValue == 0 && iDebugHudEnduranceValue == 0)
#endif // __BANK
	{
		// ensure the max display capacity is never less than the capacity
		if ((!NetworkInterface::IsGameInProgress()) || iHudMaxHealthDisplay <= 0 || iHudMaxHealthDisplay < iMaxHealth)
		{
			iHudMaxHealthDisplay = iMaxHealth;
		}

		if ((!NetworkInterface::IsGameInProgress()) || iHudMaxArmourDisplay <= 0 || iHudMaxArmourDisplay < iMaxArmour)
		{
			iHudMaxArmourDisplay = iMaxArmour;
		}

		// check endurance display capacity is never less than the capacity
		if ((!NetworkInterface::IsGameInProgress()) || iHudMaxEnduranceDisplay <= 0 || iHudMaxEnduranceDisplay < iMaxEndurance)
		{
			iHudMaxEnduranceDisplay = iMaxEndurance;
		}
	}

#if __BANK
	if (iDebugHudHealthValue != 0 || iDebugHudArmourValue != 0 || iDebugHudEnduranceValue != 0)
	{
		sfDisplayf("HEALTH: %d - %d - %d", iHealth, iMaxHealth, iHudMaxHealthDisplay);
		sfDisplayf("ARMOUR: %d - %d - %d", iArmour, iMaxArmour, iHudMaxArmourDisplay);
		sfDisplayf("ENDURANCE: %d - %d - %d", iEndurance, iMaxEndurance, iHudMaxEnduranceDisplay);

	}

	if ( ms_bLogPlayerHealth )
	{
		uiDebugf3("HEALTH: %d - %d - %d", iHealth, iMaxHealth, iHudMaxHealthDisplay);
		uiDebugf3("ARMOUR: %d - %d - %d", iArmour, iMaxArmour, iHudMaxArmourDisplay);
		uiDebugf3("ENDURANCE: %d - %d - %d", iEndurance, iMaxEndurance, iHudMaxEnduranceDisplay);
	}
#endif // __BANK

	healthInfo.Set(iHealth, iMaxHealth, iArmour, iMaxArmour, iEndurance, iMaxEndurance, iHudMaxHealthDisplay, iHudMaxArmourDisplay, iHudMaxEnduranceDisplay, iTeam, pPed);
}

void CNewHud::sHealthInfo::Set(int iHealth, int iMaxHealth, int iArmour, int iMaxArmour, int iEndurance, int iMaxEndurance, int iHudMaxHealthDisplay, int iHudMaxArmourDisplay, int iHudMaxEnduranceDisplay, int iTeam, const CPed* pPedFetched, bool bShowDamage /* = true */)
{
	m_iHealthPercentageCapacity =      s32( 100.0f * rage::ClampRange(iMaxHealth, 0, iHudMaxHealthDisplay));
	m_iArmourPercentageCapacity =      s32( 100.0f * rage::ClampRange(iMaxArmour    , 0, iHudMaxArmourDisplay    ));
	m_iEndurancePercentageCapacity =   s32( 100.0f * rage::ClampRange(iMaxEndurance, 0, iHudMaxEnduranceDisplay));

	m_iHealthPercentageValue =	  Min( s32( 100.0f * rage::ClampRange(iHealth,    0, iHudMaxHealthDisplay)), m_iHealthPercentageCapacity);  // < 100 means dead as this is injured on non-player peds?
	m_iArmourPercentageValue =    Min( s32( 100.0f * rage::ClampRange(iArmour    ,    0, iHudMaxArmourDisplay    )), m_iArmourPercentageCapacity);
	m_iEndurancePercentageValue = Min( s32( 100.0f * rage::ClampRange(iEndurance, 0, iHudMaxEnduranceDisplay)), m_iEndurancePercentageCapacity);

	m_iTeam = iTeam;

	m_bShowDamage = bShowDamage;
	m_pPedFetched = pPedFetched;
}

void CNewHud::SetHealthOverride(int iHealth, int iArmour, bool bShowDmg, int iEndurance)
{
	ms_bSuppressHealthFetch = (iHealth != -1);
	ms_HealthInfo.Set(iHealth-100, CScriptHud::iHudMaxHealthDisplay-100, iArmour, CScriptHud::iHudMaxArmourDisplay, iEndurance, CScriptHud::iHudMaxEnduranceDisplay, CScriptHud::iHudMaxHealthDisplay-100, CScriptHud::iHudMaxArmourDisplay, CScriptHud::iHudMaxEnduranceDisplay,-1, NULL, bShowDmg);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ForceUpdateHealthInfoWithExistingValuesOnRT()
// PURPOSE: updates the radar with existing health, endurance & armour values to fix 1829470
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ForceUpdateHealthInfoWithExistingValuesOnRT()
{
	UpdateHealthInfo(true);  // do the update, health, endurance & armour only, instantly on RT
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateHealthAndArmour()
// PURPOSE: updates the current player health, endurance & armour values and passes them onto
//			the radar
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateHealthInfo(const bool bInstant)
{
	//
	// update the health:
	//
	if ((!ms_bSuppressHealthFetch) && (!bInstant))
		GetHealthInfo(CMiniMap::GetMiniMapPed(), ms_HealthInfo);

	if ((bInstant) ||
		(((ms_HealthInfo.m_iHealthPercentageCapacity != PreviousHudValue.healthInfo.m_iHealthPercentageCapacity) ||
		(ms_HealthInfo.m_iHealthPercentageValue != PreviousHudValue.healthInfo.m_iHealthPercentageValue)) &&
			(ms_HealthInfo.m_iHealthPercentageValue >= 0 && ms_HealthInfo.m_iHealthPercentageValue <= 100)))
	{
		if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_PLAYER_HEALTH"))
		{
			CScaleformMgr::AddParamInt(ms_HealthInfo.m_iHealthPercentageValue);
			CScaleformMgr::AddParamBool(ms_HealthInfo.m_iHealthPercentageValue > PreviousHudValue.healthInfo.m_iHealthPercentageValue);
			CScaleformMgr::AddParamInt(ms_HealthInfo.m_iHealthPercentageCapacity);
			CScaleformMgr::AddParamBool(ms_HealthInfo.m_bShowDamage && ms_HealthInfo.m_pPedFetched && PreviousHudValue.healthInfo.m_pPedFetched == ms_HealthInfo.m_pPedFetched);
			CScaleformMgr::EndMethod(bInstant);
		}
	}

	if ((bInstant) ||
		(((ms_HealthInfo.m_iArmourPercentageCapacity != PreviousHudValue.healthInfo.m_iArmourPercentageCapacity) || (ms_HealthInfo.m_iArmourPercentageValue != PreviousHudValue.healthInfo.m_iArmourPercentageValue)) &&
		(ms_HealthInfo.m_iArmourPercentageValue >= 0 && ms_HealthInfo.m_iArmourPercentageValue <= 100))) // only update if armour has changed
	{
		if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_PLAYER_ARMOUR"))
		{
			CScaleformMgr::AddParamInt(ms_HealthInfo.m_iArmourPercentageValue);
			CScaleformMgr::AddParamBool(ms_HealthInfo.m_iArmourPercentageValue > PreviousHudValue.healthInfo.m_iArmourPercentageValue);
			CScaleformMgr::AddParamInt(ms_HealthInfo.m_iArmourPercentageCapacity);
			CScaleformMgr::EndMethod(bInstant);
		}
	}

	if (CNewHud::GetDisplayMode() == CNewHud::DM_ARCADE_CNC)
	{
		if (ms_HealthInfo.m_iTeam == (int)eArcadeTeam::AT_CNC_CROOK)
		{
			bool HasEndurancePercentageValueChanged = ms_HealthInfo.m_iEndurancePercentageValue != PreviousHudValue.healthInfo.m_iEndurancePercentageValue;
			int iClampedEndurancePercent = Clamp(ms_HealthInfo.m_iEndurancePercentageValue, 0, 100);

			if (!ms_InitializedEndurance || bInstant || HasEndurancePercentageValueChanged)
			{
				if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_ENDURANCE_BAR"))
				{
					ms_InitializedEndurance = true;
					CScaleformMgr::AddParamInt(iClampedEndurancePercent);
					CScaleformMgr::EndMethod(bInstant);
				}
			}
		}
		// Remove the endurance bar for cop players
		else if (ms_HealthInfo.m_iTeam != PreviousHudValue.healthInfo.m_iTeam)
		{
			if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "HIDE_ENDURANCE_BAR"))
			{
				ms_InitializedEndurance = false;
				CScaleformMgr::EndMethod(bInstant);
			}
		}
	}
	else
	{
		// In case we aren't in C&C anymore and the endurance bar is hide it.
		if (ms_InitializedEndurance)
		{
			if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "HIDE_ENDURANCE_BAR"))
			{
				ms_InitializedEndurance = false;
				CScaleformMgr::EndMethod(bInstant);
			}
		}	

	}

	CNewHud::eDISPLAY_MODE currentMultiplayerGameMode = CNewHud::GetDisplayMode();

	if ((currentMultiplayerGameMode != CNewHud::s_previousMultiplayerGameMode)||(ms_HealthInfo.m_iTeam != PreviousHudValue.healthInfo.m_iTeam))
	{
		s_previousMultiplayerGameMode = currentMultiplayerGameMode;

		if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_GAME_MODE"))
		{
			CScaleformMgr::AddParamInt(int(currentMultiplayerGameMode));
			CScaleformMgr::EndMethod();
		}

		if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_GAME_MODE_AND_TEAM"))
		{
			CScaleformMgr::AddParamInt(currentMultiplayerGameMode);
			CScaleformMgr::AddParamInt(ms_HealthInfo.m_iTeam);
			CScaleformMgr::EndMethod();
		}
	}

	PreviousHudValue.healthInfo = ms_HealthInfo;


	if (!bInstant)
	{
		// update Stamina display

		bool bFlashHealthMeter = (ms_HealthInfo.m_iHealthPercentageValue <= CMiniMap::sm_Tunables.HealthBar.iHealthDepletionBlinkPercentage) &&
			ms_HealthInfo.m_iHealthPercentageValue > 0;
		if( const CPed* pPed = CMiniMap::GetMiniMapPed() )
		{
			if( const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo() )
			{
				if( !pPlayerInfo->GetPlayerDataPlayerSprintDisabled() )
				{
					float fStamina = pPlayerInfo->GetPlayerDataSprintEnergy();
					float fMaxStamina = pPlayerInfo->GetPlayerDataMaxSprintEnergy();

				bFlashHealthMeter = bFlashHealthMeter || ((fStamina / fMaxStamina) <= CMiniMap::sm_Tunables.HealthBar.fStaminaDepletionBlinkPercentage);
				}
			}

			if( PreviousHudValue.bFlashHealthMeter != bFlashHealthMeter )
			{
				PreviousHudValue.bFlashHealthMeter = bFlashHealthMeter;
				if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "FLASH_HEALTH_BAR"))
				{
					CScaleformMgr::AddParamBool(bFlashHealthMeter);
					CScaleformMgr::EndMethod();
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::GetHudWantedLevel()
// PURPOSE: return what the hud thinks is the wanted level
/////////////////////////////////////////////////////////////////////////////////////
int CNewHud::GetHudWantedLevel(CWanted* pWanted, bool& bFlashOverride)
{
	int iWantedLevel = pWanted->GetWantedLevel();
	if (pWanted->m_WantedLevelBeforeParole != WANTED_CLEAN)
	{
		iWantedLevel = pWanted->m_WantedLevelBeforeParole;
	}

	// Fake wanted level will override real wanted level only if it is greater (B* 1725827)
	if (iWantedLevel < CScriptHud::iFakeWantedLevel)
	{
		iWantedLevel = CScriptHud::iFakeWantedLevel;
	}
	// If we are forcing the stars to flash skip the Fake wanted level check
	if (!CScriptHud::bForceOnWantedStarFlash)
	{
		if (CScriptHud::iFakeWantedLevel != WANTED_CLEAN)
		{
			bFlashOverride = false; // don't blink if we're faking it
		}
	}
	return iWantedLevel;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::GetHudWantedCopsAreSearching()
// PURPOSE: return if hud thinks cops are searching
/////////////////////////////////////////////////////////////////////////////////////
bool CNewHud::GetHudWantedCopsAreSearching(CWanted* pWanted)
{
	bool bCopsAreSearching = pWanted->CopsAreSearching();
	bCopsAreSearching &= (!pWanted->m_SuppressLosingWantedLevelIfHiddenThisFrame || pWanted->m_AllowEvasionHUDIfDisablingHiddenEvasionThisFrame) && !CScriptHud::bForceOffWantedStarFlash && (CScriptHud::iFakeWantedLevel == WANTED_CLEAN);
	return bCopsAreSearching;
}


void CNewHud::ForceSpecialVehicleStatusUpdate()
{
	ms_WeaponWheel.ForceSpecialVehicleStatusUpdate();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateWantedStars_ArcadeCNC()
// PURPOSE: Simplified Wanted Stars management for CNC
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateWantedStars_ArcadeCNC()
{
	CPed* pFollowPlayer = CGameWorld::FindFollowPlayer();
	if (!pFollowPlayer)
		return;

	CWanted *pWanted = pFollowPlayer->GetPlayerWanted();
	if (!pWanted)
		return;

	// RUFFIAN RM: Determine what needs to happen with wanted stars this frame

	bool bScaleformSetWantedLevel = false;
	bool bScaleformStartFlashingWantedStars = false;
	bool bScaleformStopFlashingWantedStars = false;
	bool bRemoveWantedStarsHudComponent = false;
	
	int iWantedLevel = pWanted->GetWantedLevel();
	if (iWantedLevel > 0)
	{
		// if bFlashWantedStarDisplay has changed 
		if (CScriptHud::bFlashWantedStarDisplay != PreviousHudValue.bFlashWantedStars)
		{
			bScaleformStartFlashingWantedStars = CScriptHud::bFlashWantedStarDisplay;
			bScaleformStopFlashingWantedStars = !CScriptHud::bFlashWantedStarDisplay;
			
			bScaleformSetWantedLevel = true;
		}

		// wanted level has changed
		if (iWantedLevel != PreviousHudValue.iWantedLevel)
		{
			bScaleformSetWantedLevel = true;			
		}
	}
	else if (PreviousHudValue.iWantedLevel > 0) // wanted level is currently 0, and wasn't previously
	{
		bScaleformStopFlashingWantedStars = true;
		bRemoveWantedStarsHudComponent = true;
	}

	// RUFFIAN RM: check for scaleform operations

	if (bScaleformStartFlashingWantedStars)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "FLASH_WANTED_STARS"))
		{
			CScaleformMgr::AddParamBool(true); // will flash until stopped
			CScaleformMgr::EndMethod();
		}
	}
	
	if (bScaleformStopFlashingWantedStars)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "STOP_FLASHING_WANTED_STARS"))
		{
			CScaleformMgr::EndMethod();
		}
	}

	if (bScaleformSetWantedLevel)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "SET_PLAYER_WANTED_LEVEL"))
		{
			CScaleformMgr::AddParamInt(iWantedLevel); // number of stars to show
			CScaleformMgr::AddParamBool(false); // searching mode is disabled
			CScaleformMgr::EndMethod();
		}
	}

	if (CScriptHud::bUpdateWantedThreatVisibility)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "WANTED_THREAT_IS_VISIBLE"))
		{
			CScaleformMgr::AddParamBool(CScriptHud::bIsWantedThreatVisible); // show the threat indicator skulls instead of stars
			CScaleformMgr::EndMethod();
		}
		CScriptHud::bUpdateWantedThreatVisibility = false;
	}

	if (CScriptHud::bUpdateWantedDrainLevel)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "SET_WANTED_DRAIN_LEVEL"))
		{
			CScaleformMgr::AddParamInt(CScriptHud::iWantedDrainLevelPercentage); // percentage to show the wanted meter is 
			CScaleformMgr::EndMethod();
		}
		CScriptHud::bUpdateWantedDrainLevel = false;
	}

	if (bRemoveWantedStarsHudComponent)
	{
		if (IsHudComponentActive(NEW_HUD_WANTED_STARS) && HudComponent[NEW_HUD_WANTED_STARS].iLoadingState == NOT_LOADING)
		{
			RemoveHudComponentFromActionScript(NEW_HUD_WANTED_STARS);
		}
	}	

	// RUFFIAN RM: cache this frame's data as previous HUD values
	PreviousHudValue.bFlashWantedStars = CScriptHud::bFlashWantedStarDisplay;
	PreviousHudValue.iWantedLevel = iWantedLevel;

	return;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateWantedStars()
// PURPOSE: updates the wanted stars based on the current player wanted level
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateWantedStars()
{
	// CNC path
	switch (CNewHud::ms_eDisplayMode)
	{
	case DM_DEFAULT:
		// follow logic below
		break;
	case DM_ARCADE_CNC:
		UpdateWantedStars_ArcadeCNC();
		return;
	default:
		break;
	}

	CPed* pFollowPlayer = CGameWorld::FindFollowPlayer();
	if (!pFollowPlayer)
		return;

	CWanted *pWanted = pFollowPlayer->GetPlayerWanted();
	if (pWanted)
	{
		//
		// update the wanted level:
		//

	
		const bool bFlashWholeMovie =  CScriptHud::bFlashWantedStarDisplay;
		const bool bReasonsToFlashForever = GetHudWantedCopsAreSearching(pWanted) || bFlashWholeMovie;
		const bool bReasonsToFlashTemporarily = pWanted->m_WantedLevelBeforeParole != WANTED_CLEAN;
		bool bFlashWantedStars = (bReasonsToFlashForever || bReasonsToFlashTemporarily);

		int iWantedLevel = GetHudWantedLevel(pWanted, bFlashWantedStars);

#if !__FINAL
		if (bFlashWantedStars != m_bDebugFlashWantedStars)
			wantedDebugf3("CNewHud::UpdateWantedStars bFlashWantedStars[%d] fc[%d] iWantedLevel[%d] bFlashWholeMovie[%d] CopsAreSearching[%d] bReasonsToFlashNotForever[%d]", bFlashWantedStars, fwTimer::GetFrameCount(), iWantedLevel, bFlashWholeMovie, GetHudWantedCopsAreSearching(pWanted), bReasonsToFlashTemporarily);

		m_bDebugFlashWantedStars = bFlashWantedStars;
#endif

		if (IsHudComponentMovieActive(NEW_HUD_WANTED_STARS))
		{

			if (bFlashWantedStars != PreviousHudValue.bFlashWantedStars 
				|| bReasonsToFlashForever != PreviousHudValue.bReasonsToFlashForever 
				|| iWantedLevel != PreviousHudValue.iWantedLevel
				|| bFlashWholeMovie != PreviousHudValue.bFlashWholeWantedMovie)
			{
				if( iWantedLevel > 0 )
				{
					if (bFlashWantedStars)
					{
						//@@: location CNEWHUD_UPDATEWANTEDSTARS_FLASH_STARS
						if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "FLASH_WANTED_STARS"))
						{
							CScaleformMgr::AddParamBool(bReasonsToFlashForever); // flash forever?
							CScaleformMgr::AddParamInt(CDispatchData::GetHiddenEvasionTime(iWantedLevel)); // length of time wanted stuff
							CScaleformMgr::AddParamBool(bFlashWholeMovie); // flash everything?
							CScaleformMgr::EndMethod();
						}
					}
					else if( PreviousHudValue.bFlashWantedStars != bFlashWantedStars)
					{
						if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "STOP_FLASHING_WANTED_STARS"))
						{
							CScaleformMgr::EndMethod();
						}
					}

					PreviousHudValue.bFlashWantedStars = bFlashWantedStars;
					PreviousHudValue.bReasonsToFlashForever = bReasonsToFlashForever;
					PreviousHudValue.bFlashWholeWantedMovie = bFlashWholeMovie;
				}
			}
		}

		if ( (iWantedLevel != PreviousHudValue.iWantedLevel) || (IsHudComponentActive(NEW_HUD_WANTED_STARS) && HudComponent[NEW_HUD_WANTED_STARS].iLoadingState == NOT_LOADING && iWantedLevel == 0) || (DoesScriptWantToForceShowThisFrame(NEW_HUD_WANTED_STARS,true)) )  // only update if wanted level has changed
		{
			if (iWantedLevel == 0)
			{
				// we must ensure that we dont try to remove wanted stars that are not there if forced by script
				if ( (!DoesScriptWantToForceShowThisFrame(NEW_HUD_WANTED_STARS, true)) ||
					 (DoesScriptWantToForceShowThisFrame(NEW_HUD_WANTED_STARS, true) && iWantedLevel != PreviousHudValue.iWantedLevel))
				{
					RemoveHudComponentFromActionScript(NEW_HUD_WANTED_STARS);
					PreviousHudValue.iWantedLevel = iWantedLevel;
				}
			}
			else
			{
                //@@: range CNEWHUD_UPDATEWANTEDSTARS {
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "SET_PLAYER_WANTED_LEVEL"))
				{
#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_SIXSTARWANTEDLEVEL
					if(iWantedLevel==5 && TamperActions::IsSixStarWantedLevel())
						iWantedLevel = 6;
#endif
					CScaleformMgr::AddParamInt(iWantedLevel);
					CScaleformMgr::AddParamBool(false); // searching mode is disabled
					CScaleformMgr::EndMethod();
				}
                //@@: } CNEWHUD_UPDATEWANTEDSTARS

				PreviousHudValue.iWantedLevel = iWantedLevel;
			}
		}
	}

#if __BANK
	if(ms_bShowEverything)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WANTED_STARS, "SET_PLAYER_WANTED_LEVEL"))
		{
			CScaleformMgr::AddParamInt(3);
			CScaleformMgr::AddParamBool(false); // searching mode is disabled
			CScaleformMgr::EndMethod();
		}
	}
#endif // __BANK
}

void CNewHud::UpdateMiniMapOverlay()
{
	CPed* pFollowPlayer = CGameWorld::FindFollowPlayer();
	if (!pFollowPlayer)
		return;
	CWanted *pWanted = pFollowPlayer->GetPlayerWanted();
	if (!pWanted)
		return;
	int iWantedLevel = GetHudWantedLevel(pWanted, CScriptHud::bFlashWantedStarDisplay);
	const bool bCopsHaveLOS = !CPauseMenu::IsActive() && iWantedLevel > WANTED_CLEAN && !GetHudWantedCopsAreSearching(pWanted);
	Wanted_SetCopLOS(bCopsHaveLOS);

	//Force flash wanted on for CnC
	if (CNewHud::ms_eDisplayMode == DM_ARCADE_CNC)
	{
		CMiniMap::SetFlashWantedOverlay(true);
	}
}

void CNewHud::Wanted_SetCopLOS(const bool bCopsHaveLOS)
{
	if (bCopsHaveLOS != PreviousHudValue.bCopsHaveLOS)
	{
		if (bCopsHaveLOS)
		{
			//Immediately invoke this to ensure the wanted cones are removed when we are going into LOS for bugstar:1391284 - lavalley
			NOTFINAL_ONLY(wantedDebugf3("CNewHud::Wanted_SetCopLOS bCopsHaveLOS changed --> invoke DisableCones");)
				CWanted::GetWantedBlips().DisableCones();
		}
		PreviousHudValue.bCopsHaveLOS = bCopsHaveLOS;
	}
	CMiniMap::SetFlashWantedOverlay(bCopsHaveLOS);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateGameStream()
// PURPOSE: updates the game stream
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateGameStream()
{
	CGameStreamMgr::GetGameStream()->ReportHelptextStatus(GetHudComponentPosition(NEW_HUD_HELP_TEXT),GetHudComponentSize(NEW_HUD_HELP_TEXT),IsHudComponentMovieActive(NEW_HUD_HELP_TEXT));
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateSinglePlayerCash()
// PURPOSE: updates the main player cash
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateSinglePlayerCash()
{
	if (NetworkInterface::IsGameInProgress())  // no SP cash in MP
	{
		return;
	}

#if __BANK
	if (iDebugMultiplayerBankCash != -1 || iDebugMultiplayerWalletCash != -1)  // dont process if debugging MP cash here
	{
		return;
	}
#endif // __BANK
	
	//
	// update the cash:
	//
	s64 iCurrentCash = 0;
	u32 PlayerPedModelHash = 0;

	// need to set fCash to the real cash amount here per frame
	const CPlayerInfo* pi = CGameWorld::GetMainPlayerInfo();
	if (pi && pi->GetPlayerPed())
	{
		iCurrentCash = StatsInterface::GetIntStat(STAT_TOTAL_CASH.GetStatId());

		CBaseModelInfo* pPlayerPedModel = pi->GetPlayerPed()->GetBaseModelInfo();
		if (pPlayerPedModel)
		{
			PlayerPedModelHash = pPlayerPedModel->GetHashKey();
		}
	}

	if( !DoesScriptWantToForceHideThisFrame(NEW_HUD_CASH) )
	{
		if (!CGame::IsChangingSessionState())
		{
			if( iCurrentCash != PreviousHudValue.iCash || DoesScriptWantToForceShowThisFrame(NEW_HUD_CASH, true) )
			{
				PreviousHudValue.bCashForcedOn = DoesScriptWantToForceShowThisFrame(NEW_HUD_CASH);
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH, "SET_PLAYER_CASH"))
				{
					CScaleformMgr::AddParamInt64(iCurrentCash);
					CScaleformMgr::AddParamBool(PreviousHudValue.bCashForcedOn);
					CScaleformMgr::EndMethod();
				}
				
			}
			// if we previously had it on, but don't want it anymore, hide it.
			else if( PreviousHudValue.bCashForcedOn && !DoesScriptWantToForceShowThisFrame(NEW_HUD_CASH ) )
			{
				// explicitly call HIDE on component to clear forever flag
				HideHudComponent(NEW_HUD_CASH);
				CHudTools::CallHudScaleformMethod(NEW_HUD_CASH, "HIDE");
				PreviousHudValue.bCashForcedOn = false;
			}
		}

		//	Don't display the change in cash when a player swap has just occurred
		if ( iCurrentCash != PreviousHudValue.iCash && PlayerPedModelHash == PreviousHudValue.iPlayerPedModelHash )
		{
			if (PreviousHudValue.iCash != INVALID_CASH_VALUE)	//	Don't display the initial change at the very start of the session
			{
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH_CHANGE, "SET_PLAYER_CASH_CHANGE"))
				{
					s64 iDiff = 0;
					bool bIncrease = false;

					if (iCurrentCash > PreviousHudValue.iCash)
					{
						iDiff = iCurrentCash - PreviousHudValue.iCash;
						bIncrease = true;
					}
					else
					{
						iDiff = PreviousHudValue.iCash - iCurrentCash;
					}

					CScaleformMgr::AddParamInt64(iDiff);
					CScaleformMgr::AddParamBool(bIncrease);
					CScaleformMgr::EndMethod();
				}
			}
		}
	}
	// if we're force-hiding it, and only just for this frame, force-hide
	else if( PreviousHudValue.bCashForcedOn || DoesScriptWantToForceHideThisFrame(NEW_HUD_CASH, true ) )
	{
		// explicitly call HIDE on component to clear forever flag
		CHudTools::CallHudScaleformMethod(NEW_HUD_CASH, "HIDE");
		HideHudComponent(NEW_HUD_CASH);
		PreviousHudValue.bCashForcedOn = false;
	}

#if __BANK
	if(ms_bShowEverything)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH, "SET_PLAYER_CASH"))
		{
			CScaleformMgr::AddParamInt64(1111);
			CScaleformMgr::AddParamBool(true);
			CScaleformMgr::EndMethod();
		}
	}
#endif // __BANK

	PreviousHudValue.iCash = iCurrentCash;
	PreviousHudValue.iPlayerPedModelHash = PlayerPedModelHash;
}

void CNewHud::DisplayFakeSPCash(int iAmountChanged)
{
	int iCurrentCash = StatsInterface::GetIntStat(STAT_TOTAL_CASH.GetStatId());
	iCurrentCash += iAmountChanged;

	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH, "SET_PLAYER_CASH"))
	{
		CScaleformMgr::AddParamInt64(iCurrentCash);
		CScaleformMgr::AddParamBool(true);
		CScaleformMgr::EndMethod();
	}

	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH_CHANGE, "SET_PLAYER_CASH_CHANGE"))
	{
		CScaleformMgr::AddParamInt64(abs(iAmountChanged));
		CScaleformMgr::AddParamBool(iAmountChanged > 0);
		CScaleformMgr::EndMethod();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::AreInstructionalButtonsActive()
// PURPOSE: Checks whether the instructional_buttons movie is active
/////////////////////////////////////////////////////////////////////////////////////
bool CNewHud::AreInstructionalButtonsActive()
{
	return CScaleformMgr::FindMovieByFilename("INSTRUCTIONAL_BUTTONS") != INVALID_MOVIE_ID;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ForceMultiplayerCashToReshowIfActive()
// PURPOSE: forces MP cash to reshow - if its active already
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ForceMultiplayerCashToReshowIfActive()
{
	if (IsHudComponentActive(NEW_HUD_MP_CASH))  // wallet
	{
		PreviousHudValue.iMultiplayerWalletCash = INVALID_CASH_VALUE;
	}

	if (IsHudComponentActive(NEW_HUD_CASH))  // bank
	{
		PreviousHudValue.iMultiplayerBankCash = INVALID_CASH_VALUE;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateMultiplayerCash()
// PURPOSE: updates the multiplayer cash
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateMultiplayerCash()
{
	s64 iCurrentWalletCash = -1;
	s64 iCurrentBankCash = -1;
	GetCash(iCurrentWalletCash, iCurrentBankCash);

	if(m_bSuppressCashUpdates)
	{
		PreviousHudValue.iMultiplayerWalletCash = iCurrentWalletCash;
		PreviousHudValue.iMultiplayerBankCash = iCurrentBankCash;

		if (CScriptHud::ms_bTurnOffMultiplayerWalletCashThisFrame)
		{
			CHudTools::CallHudScaleformMethod(NEW_HUD_MP_CASH, "REMOVE_PLAYER_MP_CASH");
			CHudTools::CallHudScaleformMethod(NEW_HUD_MP_MESSAGE, "REMOVE");
		}

		if (CScriptHud::ms_bTurnOffMultiplayerBankCashThisFrame)
		{
			CHudTools::CallHudScaleformMethod(NEW_HUD_CASH, "REMOVE_PLAYER_MP_CASH");
			CHudTools::CallHudScaleformMethod(NEW_HUD_CASH_CHANGE, "REMOVE");
		}

		return;
	}

	//
	// update the multiplayer wallet:
	//

	// actual value:
	if (iCurrentWalletCash != -1)
	{
		if (iCurrentWalletCash != PreviousHudValue.iMultiplayerWalletCash || CScriptHud::ms_bTurnOnMultiplayerWalletCashThisFrame || DoesScriptWantToForceShowThisFrame(NEW_HUD_MP_CASH, true))
		{
			char gxtCashString[128];
			char bankBuff[64];

			CFrontendStatsMgr::FormatInt64ToCash(iCurrentWalletCash, bankBuff, NELEM(bankBuff), false);  // removing commas as per SteveW
			
			// build the string out of text from the text file & also the converted cash and bank cash
			safecpy(gxtCashString, "~HC_GREEN~ <font size='20'>" );

			if ( (!CPauseMenu::IsActive()) && (CScriptHud::ms_bAllowDisplayOfMultiplayerCashText) )  // fix for 1633959 & 1635472
			{
				safecat(gxtCashString, TheText.Get("MENU_PLYR_CASH") );  // "CASH"
			}
			safecat(gxtCashString, "<font size='26'> ");
			safecat(gxtCashString, (char*)bankBuff);

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_MP_CASH, "SET_PLAYER_MP_CASH_WITH_STRING"))
			{
				CScaleformMgr::AddParamString(gxtCashString);
				CScaleformMgr::AddParamBool(CScriptHud::ms_bTurnOnMultiplayerWalletCashThisFrame);
				CScaleformMgr::EndMethod();
			}

			// changed:
			if (iCurrentWalletCash != PreviousHudValue.iMultiplayerWalletCash || 
				CScriptHud::ms_bTurnOnMultiplayerWalletCashThisFrame)
			{
				if (PreviousHudValue.iMultiplayerWalletCash != INVALID_CASH_VALUE)	//	Don't display the initial change at the very start of the session
				{
					s64 iDiff = 0;
					bool bIncrease = false;

					if (iCurrentWalletCash > PreviousHudValue.iMultiplayerWalletCash)
					{
						iDiff = iCurrentWalletCash - PreviousHudValue.iMultiplayerWalletCash;
						bIncrease = true;
					}
					else
					{
						iDiff = PreviousHudValue.iMultiplayerWalletCash - iCurrentWalletCash;
					}

					if (iDiff != 0)
					{
						char gxtCashString[60];
						char bankBuff[64];

						if (bIncrease)
						{
							safecpy(gxtCashString, "~HC_WHITE~" );
							safecat(gxtCashString, "+" );
						}
						else
						{
							safecpy(gxtCashString, "~HC_RED~");
							safecat(gxtCashString, "-");
						}

						formatf(bankBuff, "$%d", iDiff, NELEM(bankBuff));

						safecat(gxtCashString, (char*)bankBuff );

						if (CHudTools::BeginHudScaleformMethod(NEW_HUD_MP_MESSAGE, "SET_MP_MESSAGE"))
						{
							CScaleformMgr::AddParamString(gxtCashString);
							CScaleformMgr::EndMethod();
						}
					}
				}
			}
		}

		if (CScriptHud::ms_bTurnOffMultiplayerWalletCashThisFrame)
		{
			CHudTools::CallHudScaleformMethod(NEW_HUD_MP_CASH, "REMOVE_PLAYER_MP_CASH");
			CHudTools::CallHudScaleformMethod(NEW_HUD_MP_MESSAGE, "REMOVE");
		}
	}
	else
	{
		if (PreviousHudValue.iMultiplayerWalletCash != iCurrentWalletCash || CScriptHud::ms_bTurnOffMultiplayerWalletCashThisFrame)
		{
			CHudTools::CallHudScaleformMethod(NEW_HUD_MP_CASH, "REMOVE");  // REMOVE_PLAYER_MP_CASH calls hide which we dont want here
			CHudTools::CallHudScaleformMethod(NEW_HUD_MP_MESSAGE, "REMOVE");
		}
	}

	PreviousHudValue.iMultiplayerWalletCash = iCurrentWalletCash;



	//
	// update the multiplayer bank cash:
	//

	// actual value:
	if (iCurrentBankCash != -1)
	{
		if (iCurrentBankCash != PreviousHudValue.iMultiplayerBankCash || CScriptHud::ms_bTurnOnMultiplayerBankCashThisFrame || DoesScriptWantToForceShowThisFrame(NEW_HUD_CASH, true))
		{
			char gxtCashString[128];
			char bankBuff[64];

			CFrontendStatsMgr::FormatInt64ToCash(iCurrentBankCash, bankBuff, NELEM(bankBuff), false);  // removing commas as per SteveW
			
			// build the string out of text from the text file & also the converted cash and bank cash
			safecpy(gxtCashString, "~HC_GREENLIGHT~ <font size='20'>");

			if ( (!CPauseMenu::IsActive()) && (CScriptHud::ms_bAllowDisplayOfMultiplayerCashText) )  // fix for 1633959 & 1635472
			{
				safecat(gxtCashString, TheText.Get("MENU_PLYR_BANK"), NELEM(gxtCashString));  // "BANK"
			}

			safecat(gxtCashString, "<font size='26'> ");
			safecat(gxtCashString, (char*)bankBuff );

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH, "SET_PLAYER_MP_CASH_WITH_STRING"))
			{
				CScaleformMgr::AddParamString(gxtCashString);
				CScaleformMgr::AddParamBool(CScriptHud::ms_bTurnOnMultiplayerBankCashThisFrame);
				CScaleformMgr::EndMethod();
			}

			// changed:
			if (iCurrentBankCash != PreviousHudValue.iMultiplayerBankCash || CScriptHud::ms_bTurnOnMultiplayerBankCashThisFrame)
			{
				if (PreviousHudValue.iMultiplayerBankCash != INVALID_CASH_VALUE)	//	Don't display the initial change at the very start of the session
				{
					s64 iDiff = 0;
					bool bIncrease = false;

					if (iCurrentBankCash > PreviousHudValue.iMultiplayerBankCash)
					{
						iDiff = iCurrentBankCash - PreviousHudValue.iMultiplayerBankCash;
						bIncrease = true;
					}
					else
					{
						iDiff = PreviousHudValue.iMultiplayerBankCash - iCurrentBankCash;
					}

					if (iDiff != 0)
					{
						if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH_CHANGE, "SET_PLAYER_CASH_CHANGE"))
						{
							CScaleformMgr::AddParamInt64(iDiff);
							CScaleformMgr::AddParamBool(bIncrease);
							CScaleformMgr::EndMethod();
						}
					}
				}
			}
		}

		if (CScriptHud::ms_bTurnOffMultiplayerBankCashThisFrame)
		{
			CHudTools::CallHudScaleformMethod(NEW_HUD_CASH, "REMOVE_PLAYER_MP_CASH");
			CHudTools::CallHudScaleformMethod(NEW_HUD_CASH_CHANGE, "REMOVE");
		}
	}
	else
	{
		if (PreviousHudValue.iMultiplayerBankCash != iCurrentBankCash || CScriptHud::ms_bTurnOffMultiplayerBankCashThisFrame)
		{
			CHudTools::CallHudScaleformMethod(NEW_HUD_CASH, "REMOVE");
			CHudTools::CallHudScaleformMethod(NEW_HUD_CASH_CHANGE, "REMOVE");
		}
	}

	PreviousHudValue.iMultiplayerBankCash = iCurrentBankCash;
}

void CNewHud::UseFakeMPCash(bool bUseFakeMPCash)
{
	if(bUseFakeMPCash)
	{
		GetCash(m_iMPWalletCashOverride, m_iMPBankCashOverride);
		m_bSuppressCashUpdates = true;
	}
	else
	{
		m_iMPWalletCashOverride = FAKE_CASH_NOT_IN_USE;
		m_iMPBankCashOverride = FAKE_CASH_NOT_IN_USE;
		m_bSuppressCashUpdates = false;
	}
}

void CNewHud::ChangeFakeMPCash(int iMPWalletDifference, int iMPBankDifference)
{
	s64 iCurrentWalletCash = m_iMPWalletCashOverride;
	s64 iCurrentBankCash = m_iMPBankCashOverride;

	iCurrentWalletCash += iMPWalletDifference;
	iCurrentBankCash += iMPBankDifference;

	OverrideMPCash(iCurrentWalletCash, iCurrentBankCash);
}

void CNewHud::OverrideMPCash(s64 iMPWalletCashOverride, s64 iMPBankCashOverride)
{
	s64 iCurrentWalletCash = -1;
	s64 iCurrentBankCash = -1;

	iCurrentWalletCash = m_iMPWalletCashOverride;
	iCurrentBankCash = m_iMPBankCashOverride;
	m_iMPWalletCashOverride = iMPWalletCashOverride;
	m_iMPBankCashOverride = iMPBankCashOverride;

	// Wallet Override
	if(m_iMPWalletCashOverride != iCurrentWalletCash)
	{
		char gxtCashString[128];
		char bankBuff[64];

		CFrontendStatsMgr::FormatInt64ToCash(m_iMPWalletCashOverride, bankBuff, NELEM(bankBuff), false);
		safecpy(gxtCashString, "~HC_GREEN~ <font size='20'>" );

		if ( (!CPauseMenu::IsActive()) && (CScriptHud::ms_bAllowDisplayOfMultiplayerCashText) )
		{
			safecat(gxtCashString, TheText.Get("MENU_PLYR_CASH") );
		}
		safecat(gxtCashString, "<font size='26'> ");
		safecat(gxtCashString, (char*)bankBuff);

		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_MP_CASH, "SET_PLAYER_MP_CASH_WITH_STRING"))
		{
			CScaleformMgr::AddParamString(gxtCashString);
			CScaleformMgr::AddParamBool(CScriptHud::ms_bTurnOnMultiplayerWalletCashThisFrame);
			CScaleformMgr::EndMethod();
		}

		s64 iDiff = 0;
		bool bIncrease = false;

		if (m_iMPWalletCashOverride > iCurrentWalletCash)
		{
			iDiff = m_iMPWalletCashOverride - iCurrentWalletCash;
			bIncrease = true;
		}
		else
		{
			iDiff = iCurrentWalletCash - m_iMPWalletCashOverride;
		}

		if (iDiff != 0)
		{
			char gxtCashString[60];
			char bankBuff[64];

			if (bIncrease)
			{
				safecpy(gxtCashString, "~HC_WHITE~" );
				safecat(gxtCashString, "+" );
			}
			else
			{
				safecpy(gxtCashString, "~HC_RED~");
				safecat(gxtCashString, "-");
			}

			formatf(bankBuff, "$%d", iDiff, NELEM(bankBuff));

			safecat(gxtCashString, (char*)bankBuff );

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_MP_MESSAGE, "SET_MP_MESSAGE"))
			{
				CScaleformMgr::AddParamString(gxtCashString);
				CScaleformMgr::EndMethod();
			}
		}
	}

	// Bank Override
	if(m_iMPBankCashOverride != iCurrentBankCash)
	{
		char gxtCashString[128];
		char bankBuff[64];

		CFrontendStatsMgr::FormatInt64ToCash(m_iMPBankCashOverride, bankBuff, NELEM(bankBuff), false);
		safecpy(gxtCashString, "~HC_GREENLIGHT~ <font size='20'>");

		if ( (!CPauseMenu::IsActive()) && (CScriptHud::ms_bAllowDisplayOfMultiplayerCashText) )
		{
			safecat(gxtCashString, TheText.Get("MENU_PLYR_BANK"), NELEM(gxtCashString));
		}

		safecat(gxtCashString, "<font size='26'> ");
		safecat(gxtCashString, (char*)bankBuff );

		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH, "SET_PLAYER_MP_CASH_WITH_STRING"))
		{
			CScaleformMgr::AddParamString(gxtCashString);
			CScaleformMgr::AddParamBool(CScriptHud::ms_bTurnOnMultiplayerBankCashThisFrame);
			CScaleformMgr::EndMethod();
		}

		s64 iDiff = 0;
		bool bIncrease = false;

		if (m_iMPBankCashOverride > iCurrentBankCash)
		{
			iDiff = m_iMPBankCashOverride - iCurrentBankCash;
			bIncrease = true;
		}
		else
		{
			iDiff = iCurrentBankCash - m_iMPBankCashOverride;
		}

		if (iDiff != 0)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_CASH_CHANGE, "SET_PLAYER_CASH_CHANGE"))
			{
				CScaleformMgr::AddParamInt64(iDiff);
				CScaleformMgr::AddParamBool(bIncrease);
				CScaleformMgr::EndMethod();
			}
		}
	}
}

void CNewHud::GetCash(s64 &iCurrentWalletCash, s64 &iCurrentBankCash)
{
	#if __BANK
	if (iDebugMultiplayerWalletCash != -1)
	{
		iCurrentWalletCash = iDebugMultiplayerWalletCash;
	}

	if (iDebugMultiplayerBankCash != -1)
	{
		iCurrentBankCash = iDebugMultiplayerBankCash;
	}
#endif // __BANK

	if (NetworkInterface::IsGameInProgress() && (NetworkInterface::IsTransitioning() || !NetworkInterface::IsSessionLeaving()))  // no MP cash in SP
	{
	#if __BANK
		if (iDebugMultiplayerWalletCash == -1)
	#endif
		{
			if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			{
				iCurrentWalletCash = MoneyInterface::GetVCWalletBalance();
			}
		}

	#if __BANK
		if (iDebugMultiplayerBankCash == -1)
	#endif
		{
			if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			{
				iCurrentBankCash = MoneyInterface::GetVCBankBalance();
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::AllowRestartHud()
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::AllowRestartHud()
{
#define NUM_OF_HUD_COMPONENTS_REMOVED_BEFORE_RESTART_ATTEMPT (5)  // 5 hud components must be removed before we attempt to restart the hud

	if (!ms_bRestartHud)
	{
		if (ms_iNumHudComponentsRemovedSinceLastRestartHud >= NUM_OF_HUD_COMPONENTS_REMOVED_BEFORE_RESTART_ATTEMPT)
		{
			sfDebugf3("HUD: RestartHud: Hud will be restarted on next opportunity as %d hud components have been removed since last restart", ms_iNumHudComponentsRemovedSinceLastRestartHud);
			ms_bRestartHud = true;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::DoesHudComponentBlockRestart()
// PURPOSE: returns true if iComponent is any of the listed items - we cannot
//			restart when these are active
//			these are generally components that, if active, set up on another system
//			for example, subtitle text will be set to active, triggered from an
//			external system and script can check if there is subtitle text active,
//			which returns the flag in the external system, but as the hud will be
//			restarted the text is not on screen.   Put simply, we can only restart
//			if a component is active when that component is soley used and managed
//			by CNewHud.   This function needs to list any hud components that are
//			not just used by CNewHud.
//			NEW_HUD_SUBTITLE_TEXT - added to list to fix 1811794
//			NEW_HUD_WEAPON_WHEEL - added to list to fix 1811311
/////////////////////////////////////////////////////////////////////////////////////
bool CNewHud::DoesHudComponentBlockRestart(const s32 iComponent)
{
	if (iComponent == NEW_HUD_SUBTITLE_TEXT ||
		iComponent == NEW_HUD_WANTED_STARS ||
		iComponent == NEW_HUD_WEAPON_WHEEL ||
		iComponent == NEW_HUD_RADIO_STATIONS)
	{
		return true;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::RestartHud()
// PURPOSE: restarts the hud from scratch - only to be called under specific
//			circumstances - force restart will restart it even if movies are active
//			but this causes a flicker so would only want that called on screen fades
//			etc
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::RestartHud(bool bForceRestart)
{
	if (ms_bRestartHud && HudComponent[NEW_HUD].iId != -1 && CScaleformMgr::IsMovieActive(HudComponent[NEW_HUD].iId))
	{
		bool bFoundActiveComponent = false;

		for (s32 iCount = BASE_HUD_COMPONENT; ( (!bFoundActiveComponent) && (iCount < TOTAL_HUD_COMPONENT_LIST_SIZE) ); iCount++)
		{
			if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
				continue;

			if (HudComponent[iCount].iLoadingState != NOT_LOADING)
			{
				sfDebugf3("HUD: RestartHud: Attempting to restart hud but cant as '%s' (%d) component is in the middle of a state change (%d)", HudComponent[iCount].cFilename, iCount, HudComponent[iCount].iLoadingState);
				bFoundActiveComponent = true;

				break;
			}
			else
			if ( (!HudComponent[iCount].bCodeThinksVisible) || (DoesScriptWantToForceHideThisFrame(iCount)) )  // fix for restarting hud when component is hidden
			{
				sfDebugf3("HUD: RestartHud: Attempting to restart hud but cant as '%s' (%d) component is currently hidden", HudComponent[iCount].cFilename, iCount);
				bFoundActiveComponent = true;

				break;
			}
			else
			if ( (DoesScriptWantToForceShowThisFrame(iCount)) )  // fix for restarting hud when component is forced on
			{
				sfDebugf3("HUD: RestartHud: Attempting to restart hud but cant as '%s' (%d) component is currently forced on by script", HudComponent[iCount].cFilename, iCount);
				bFoundActiveComponent = true;

				break;
			}
			else
			{
				if (CNewHud::IsHudComponentActive(iCount))
				{
					if (!bForceRestart)
					{
						// some hud components are allowed to be active and restart, aslong as we flag to reshow them afterwards:
						switch (iCount)
						{
							case NEW_HUD_RETICLE:  // we can restart with reticle 'active' (since it always is)... but we reset its state after correctly now
							{
								break;
							}

							default:
							{
								sfDebugf3("HUD: RestartHud: Attempting to restart hud but cant as '%s' (%d) component is active", HudComponent[iCount].cFilename, iCount);
								bFoundActiveComponent = true;
							}
						}
				
						break;
					}
					else
					{
						if (DoesHudComponentBlockRestart(iCount))
						{
							sfDebugf3("HUD: RestartHud: Attempting to force restart hud but cant as '%s' (%d) component is active", HudComponent[iCount].cFilename, iCount);
							bFoundActiveComponent = true;
							break;
						}
						else
						if (iCount >= SCRIPT_HUD_COMPONENTS_START)  // even if we force a restart hud we cant restart if a scripted hud component is active
						{
							sfDebugf3("HUD: RestartHud: Attempting to force restart hud but cant as scripted '%s' (%d) component is active", HudComponent[iCount].cFilename, iCount);
							bFoundActiveComponent = true;
							break;
						}
					}
				}
			}
		}

		if (!bFoundActiveComponent)
		{
#if	!__FINAL
			if (PARAM_noautorestarthud.Get())
			{
				sfDebugf3("HUD: RestartHud: Hud has NOT been restarted by code as it is turned off");
			}
			else
#endif  // !__FINAL

			{
				if (CScaleformMgr::RestartMovie(HudComponent[NEW_HUD].iId, false, false))
				{
					InitStartingValues();

					// send Actionscript any component values for any hud components currently active:
					for (s32 iCount = BASE_HUD_COMPONENT; (iCount < MAX_NEW_HUD_COMPONENTS); iCount++)
					{
						if (IsHudComponentActive(iCount))
						{
							ResetPreviousValueOfHudComponent(iCount);
						}
					}

					sfDebugf3("HUD: RestartHud: Hud has been restarted by code");
				}
				else
				{
					sfDebugf3("HUD: RestartHud: Hud tried to restart but it failed");
				}
			}

			ms_bRestartHud = false;
			ms_iNumHudComponentsRemovedSinceLastRestartHud = 0;
		}

		if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_AIR_BAR"))
		{
			CScaleformMgr::AddParamFloat(-1.0f);
			CScaleformMgr::EndMethod();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::SetSubtitleText()
// PURPOSE: sets up the HUD with the subtitle text
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::SetSubtitleText(char *pMessageText)
{
	if (pMessageText && pMessageText[0] != 0)
	{
		if (s_useNewBlipTextHud)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_SUBTITLE_TEXT, "SET_SUBTITLE_TEXT_RAW"))
			{
				bool bInCutscene = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());
				CScaleformMgr::AddParamString(pMessageText, false);
				CScaleformMgr::AddParamBool(bInCutscene);
				CScaleformMgr::EndMethod();
			}

			return;
		}

		char cHtmlFormattedHelpTextString[MAX_CHARS_FOR_TEXT_STRING];

		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_SUBTITLE_TEXT, "SET_SUBTITLE_TEXT"))
		{
			CScaleformMgr::AddParamString(CTextConversion::TextToHtml(pMessageText, cHtmlFormattedHelpTextString, MAX_CHARS_FOR_TEXT_STRING));
			CScaleformMgr::EndMethod();
		}
	}																																	
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ClearSubtitleText()
// PURPOSE: clears the subtitle text movie content
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ClearSubtitleText()
{
	CHudTools::CallHudScaleformMethod(NEW_HUD_SUBTITLE_TEXT, "CLEAR_SUBTITLE_TEXT");
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ClearHelpText()
// PURPOSE: clears the help text movie content
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ClearHelpText(s32 iHelpTextSlot, bool bClearNow)
{
	SetHelpTextWaitingForActionScriptResponse(iHelpTextSlot, true);

	if (bClearNow)
	{
		CHelpMessage::ClearMessage(iHelpTextSlot);

		// doesnt fade out, clears instanty (but still cleans up, requests removal in same way)
		if (CHudTools::BeginHudScaleformMethod(static_cast<eNewHudComponents>(NEW_HUD_HELP_TEXT+iHelpTextSlot), "CLEAR_HELP_TEXT_NOW"))
		{
			CScaleformMgr::AddParamInt((s32)GetHelpTextToken(iHelpTextSlot));
			CScaleformMgr::EndMethod();
		}
	}
	else
	{
		if (CHudTools::BeginHudScaleformMethod(static_cast<eNewHudComponents>(NEW_HUD_HELP_TEXT+iHelpTextSlot), "CLEAR_HELP_TEXT"))
		{
			CScaleformMgr::AddParamInt((s32)GetHelpTextToken(iHelpTextSlot));
			CScaleformMgr::EndMethod();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateHelpText()
// PURPOSE: updates help message text
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateHelpText()
{
#define ACTION_SCRIPT_DISPLAY_PER_FRAME	(1)
#define ACTION_SCRIPT_DISPLAY_FOREVER	(0)

	for (s32 iCount = 0; iCount < MAX_HELP_TEXT_SLOTS; iCount++)
	{
		char *pMessageText = CHelpMessage::GetMessageText(iCount);

		if (pMessageText[0] != 0)
		{
			const eNewHudComponents helpTextComponent = static_cast<eNewHudComponents>(NEW_HUD_HELP_TEXT+iCount);
			//
			// see if we need to reposition the help text:
			//
			if ( (iCount != HELP_TEXT_SLOT_STANDARD) && (CHelpMessage::GetScreenPosition(iCount) != PreviousHudValue.vHelpMessagePosition[iCount]) )
			{
				Vector2 vNewPosition;
				eTEXT_ARROW_ORIENTATION iDirectionOffScreen = HELP_TEXT_ARROW_NORMAL;  // 0 = not off screen

				if (CHelpMessage::GetScreenPosition(iCount).x == -9999.0f && CHelpMessage::GetScreenPosition(iCount).y == -9999.0f)
				{
					vNewPosition = HudComponent[helpTextComponent].vPos;
				}
				else
				{
					vNewPosition = CHelpMessage::GetScreenPosition(iCount);
					iDirectionOffScreen = CHelpMessage::GetDirectionHelpText(iCount, vNewPosition);
				}

				// pass the onscreen/offscreen info to ActionScript if it differs from previous time:
				if (iDirectionOffScreen != PreviousHudValue.iDirectionOffScreen[iCount])
				{
					if (CHudTools::BeginHudScaleformMethod(helpTextComponent, "SET_HELP_TEXT_OFFSCREEN"))
					{
						CScaleformMgr::AddParamInt((s32)iDirectionOffScreen);

						CScaleformMgr::EndMethod();
					}

					PreviousHudValue.iDirectionOffScreen[iCount] = iDirectionOffScreen;
				}

				// invoke ActionScript with the new position (only if its different when its floating help text)
				if (vNewPosition != PreviousHudValue.vHelpMessagePosition[iCount])
				{
					if (CHudTools::BeginHudScaleformMethod(helpTextComponent, "SET_HELP_TEXT_POSITION"))
					{
						CScaleformMgr::AddParamFloat(vNewPosition.x);
						CScaleformMgr::AddParamFloat(vNewPosition.y);
						CScaleformMgr::EndMethod();
					}
				}

				PreviousHudValue.vHelpMessagePosition[iCount] = CHelpMessage::GetScreenPosition(iCount);
			}

			Color32 colour = CHelpMessage::GetColour(iCount);
			
			// if not in an interior and game clock is between 20:00 and 06:00, we display no black background on standard help text:
			if (iCount == HELP_TEXT_SLOT_STANDARD && CHelpMessage::GetStyle(iCount) == HELP_TEXT_STYLE_NORMAL)  // only on standard help text (not floating)
			{
				CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

 				if ((!pPlayerPed->GetPortalTracker()->IsInsideInterior()) && (CClock::GetHour() < 6 || CClock::GetHour() > 20))
 					colour.SetAlpha(0);
			}
			
			if (CHelpMessage::IsHelpTextNew(iCount) ||
				CHelpMessage::IsHelpTextForever(iCount) ||
				CHelpMessage::IsHelpTextSame(iCount) ||
				ms_bShowHelperTextAfterPauseMenuExit)
			{
				ms_bShowHelperTextAfterPauseMenuExit = false;
				if (CHelpMessage::IsHelpTextNew(iCount) || CHelpMessage::IsHelpTextForever(iCount))  // start with the standard time countdown
				{
					if (CHelpMessage::ShouldPlaySound(iCount))
					{
						if (!DoesScriptWantToForceHideThisFrame(helpTextComponent))  // fixes 1477359, later change fixes 1534814
						{
							// play a sound
							Displayf("NewHud::UpdateHelpText Playing sound INFO ");
							g_FrontendAudioEntity.PlaySound("INFO","HUD_FRONTEND_DEFAULT_SOUNDSET");
						}

						if (!cStoreScreenMgr::IsTransitioningToPause())
							CHelpMessage::SetShouldPlaySound(iCount, false);
					}
				}

				if (CHudTools::BeginHudScaleformMethod(helpTextComponent, "SET_HELP_TEXT_STYLE"))
				{
					CScaleformMgr::AddParamInt((s32)CHelpMessage::GetStyle(iCount));
					CScaleformMgr::AddParamInt((s32)CHelpMessage::GetArrowDirection(iCount));
					CScaleformMgr::AddParamInt((s32)CHelpMessage::GetFloatingTextOffset(iCount));
					CScaleformMgr::AddParamInt((s32)colour.GetRed());
					CScaleformMgr::AddParamInt((s32)colour.GetGreen());
					CScaleformMgr::AddParamInt((s32)colour.GetBlue());
					CScaleformMgr::AddParamInt((s32)(colour.GetAlphaf() * 100.0f));
					CScaleformMgr::EndMethod();
				}
#if __DEV && 0 // this has been broken for a while, but now it works, so... commenting out
				if (DoesScriptWantToForceHideThisFrame(helpTextComponent))
				{
					uiDisplayf("Help text getting called but HUD is hidden because HIDE_HUD_COMPONENT_THIS_FRAME(%s) or HIDE_HELP_TEXT_THIS_FRAME is being called", HudComponent[helpTextComponent].cFilename);
				}
#endif // __DEV

				if (CHudTools::BeginHudScaleformMethod(helpTextComponent, "SET_OVERRIDE_DURATION"))
				{
					CScaleformMgr::AddParamInt(CHelpMessage::GetOverrideDuration(iCount));
					CScaleformMgr::EndMethod();
				}

				IncrementHelpTextToken(iCount);
				SetHelpTextWaitingForActionScriptResponse(iCount, false);

				if (s_useNewBlipTextHud)
				{
					if (CHudTools::BeginHudScaleformMethod(helpTextComponent, "SET_HELP_TEXT_RAW"))
					{
						CScaleformMgr::AddParamString(pMessageText, false);

						if (CHelpMessage::IsHelpTextForever(iCount) || CHelpMessage::WasHelpTextForever(iCount))
						{
							CScaleformMgr::AddParamInt(ACTION_SCRIPT_DISPLAY_FOREVER);
						}
						else if (CHelpMessage::IsHelpTextSame(iCount))
						{
							CScaleformMgr::AddParamInt(ACTION_SCRIPT_DISPLAY_PER_FRAME);
						}
						else
						{
							CScaleformMgr::AddParamInt(-1);
						}

						CScaleformMgr::AddParamInt((s32)GetHelpTextToken(iCount));

						CScaleformMgr::EndMethod();
					}
				}
				else
				{
					if (CHudTools::BeginHudScaleformMethod(helpTextComponent, "SET_HELP_TEXT"))
					{
						CScaleformMgr::AddParamString(pMessageText);

						if (CHelpMessage::IsHelpTextForever(iCount) || CHelpMessage::WasHelpTextForever(iCount))
						{
							CScaleformMgr::AddParamInt(ACTION_SCRIPT_DISPLAY_FOREVER);
						}
						else if (CHelpMessage::IsHelpTextSame(iCount))
						{
							CScaleformMgr::AddParamInt(ACTION_SCRIPT_DISPLAY_PER_FRAME);
						}
						else
						{
							CScaleformMgr::AddParamInt(-1);
						}

						CScaleformMgr::AddParamInt((s32)GetHelpTextToken(iCount));

						CScaleformMgr::EndMethod();
					}
				}

				// Only clear the help text if it's not meant to display forever
				CHelpMessage::SetHelpMessageAsProcessed(iCount);  // set it as processed this frame
			}
		}
	}

#if __BANK
	if(ms_bShowEverything)
	{
		char buff[1000];
		sprintf(buff, "%d", fwTimer::GetTimeInMilliseconds());
		CHelpMessage::SetMessageText(0, buff);
	}
#endif // __BANK
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateForMpChat()
// PURPOSE: Hides/shows overlapping HUD components based on MP Chat status
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateForMpChat()
{
#if RSG_PC
	if(SMultiplayerChat::GetInstance().IsChatTyping())
	{
		if(HudComponent[NEW_HUD_VEHICLE_NAME].bCodeThinksVisible)
		{
			HideHudComponent(NEW_HUD_VEHICLE_NAME);
			HudComponent[NEW_HUD_VEHICLE_NAME].bShouldShowComponentOnMpTextChatClose = true;
		}

		if(HudComponent[NEW_HUD_AREA_NAME].bCodeThinksVisible)
		{
			HideHudComponent(NEW_HUD_AREA_NAME);
			HudComponent[NEW_HUD_AREA_NAME].bShouldShowComponentOnMpTextChatClose = true;
		}

		if(HudComponent[NEW_HUD_STREET_NAME].bCodeThinksVisible)
		{
			HideHudComponent(NEW_HUD_STREET_NAME);
			HudComponent[NEW_HUD_STREET_NAME].bShouldShowComponentOnMpTextChatClose = true;
		}

		if(HudComponent[NEW_HUD_SUBTITLE_TEXT].bCodeThinksVisible)
		{
			HideHudComponent(NEW_HUD_SUBTITLE_TEXT);
			HudComponent[NEW_HUD_SUBTITLE_TEXT].bShouldShowComponentOnMpTextChatClose = true;
		}
	}
	else
	{
		if(HudComponent[NEW_HUD_VEHICLE_NAME].bShouldShowComponentOnMpTextChatClose)
		{
			ShowHudComponent(NEW_HUD_VEHICLE_NAME);
			HudComponent[NEW_HUD_VEHICLE_NAME].bShouldShowComponentOnMpTextChatClose = false;
		}

		if(HudComponent[NEW_HUD_AREA_NAME].bShouldShowComponentOnMpTextChatClose)
		{
			ShowHudComponent(NEW_HUD_AREA_NAME);
			HudComponent[NEW_HUD_AREA_NAME].bShouldShowComponentOnMpTextChatClose = false;
		}

		if(HudComponent[NEW_HUD_STREET_NAME].bShouldShowComponentOnMpTextChatClose)
		{
			ShowHudComponent(NEW_HUD_STREET_NAME);
			HudComponent[NEW_HUD_STREET_NAME].bShouldShowComponentOnMpTextChatClose = false;
		}

		if(HudComponent[NEW_HUD_SUBTITLE_TEXT].bShouldShowComponentOnMpTextChatClose)
		{
			ShowHudComponent(NEW_HUD_SUBTITLE_TEXT);
			HudComponent[NEW_HUD_SUBTITLE_TEXT].bShouldShowComponentOnMpTextChatClose = false;
		}
	}
#endif // RSG_PC
}

void CNewHud::UpdateIMEVisibility()
{
#if RSG_PC
	if(ioKeyboard::ImeIsInProgress() && CPauseMenu::IsActive())
	{
		if(HudComponent[NEW_HUD_SUBTITLE_TEXT].bCodeThinksVisible)
		{
			HideHudComponent(NEW_HUD_SUBTITLE_TEXT);
			HudComponent[NEW_HUD_SUBTITLE_TEXT].bShouldShowComponentOnIMEClose = true;
		}

		if(HudComponent[NEW_HUD_WANTED_STARS].bCodeThinksVisible)
		{
			HideHudComponent(NEW_HUD_WANTED_STARS);
			HudComponent[NEW_HUD_WANTED_STARS].bShouldShowComponentOnIMEClose = true;
		}
	}
	else
	{
		if(HudComponent[NEW_HUD_SUBTITLE_TEXT].bShouldShowComponentOnIMEClose)
		{
			ShowHudComponent(NEW_HUD_SUBTITLE_TEXT);
			HudComponent[NEW_HUD_SUBTITLE_TEXT].bShouldShowComponentOnIMEClose = false;
		}

		if(HudComponent[NEW_HUD_WANTED_STARS].bShouldShowComponentOnIMEClose)
		{
			ShowHudComponent(NEW_HUD_WANTED_STARS);
			HudComponent[NEW_HUD_WANTED_STARS].bShouldShowComponentOnIMEClose = false;
		}
	}
#endif // RSG_PC
}

#if RSG_PC
bool CNewHud::UpdateImeText()
{
	bool bIMEActive = false;
	if(ioKeyboard::ImeIsInProgress())
	{
		SetHudComponentToBeShown(NEW_HUD_HELP_TEXT);

		// If IME is in progress we do not have input focus. We wait for the two exit cases to be released so we do not close
		// the input box when we stop IME text input!
		bIMEActive = true;

		// TODO: This is just a sample of how to interact with the IME text input! Replace with real implementation.
		const u32 bufferLen = 255;
		char tmpBuffer[bufferLen] = {0};

		// Slow to use atString and it uses the heap but this is just an example!
		atString buffer(WideToUtf8(tmpBuffer, ioKeyboard::ImeGetCompositionText(), bufferLen));
		buffer += "\n";

		for(u32 i = 0; i < ioKeyboard::ImeGetNumberOfCandidates(); ++i)
		{
			if(!CTextFormat::DoAllCharactersExistInFont(FONT_STYLE_STANDARD, ioKeyboard::ImeGetCandidateText(i), bufferLen))
			{
				// TODO: If possible we should remove invalid characters from the list here.
			}

			buffer += "\n";

			if(i == ioKeyboard::ImeGetSelectedCandidateIndex())
				buffer += ">";
			else
				buffer += " ";

			char indexStr[8] = {0};
			buffer += formatf(indexStr, "%d", i +1);
			buffer += "\t";
			buffer += WideToUtf8(tmpBuffer, ioKeyboard::ImeGetCandidateText(i), bufferLen);
		}

		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, buffer, NULL, 0, NULL, 0, true, false, -1, true);
	}

	return bIMEActive;
}
#endif // RSG_PC

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::HandleCharacterChange(...)
// PURPOSE: Handles any character-specific HUD changes 
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::HandleCharacterChange()
{
	SetCurrentCharacterColourFromStat();

	if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_CHARACTER_COLOUR"))
	{
		CScaleformMgr::AddParamInt(ms_eCurrentCharacterColour);
		CScaleformMgr::EndMethod();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::SetCharacterColour(...)
// PURPOSE: Retrieves the character colour for the current player
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::SetCurrentCharacterColour(eHUD_COLOURS colour)
{
	ms_eCurrentCharacterColour = colour;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::SetCharacterColour(...)
// PURPOSE: Retrieves the character colour for the current player
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::SetCurrentCharacterColourFromStat()
{
	SetCurrentCharacterColour( GetCharacterColour(StatsInterface::GetStatsModelPrefix()) );
}

eHUD_COLOURS CNewHud::GetCurrentCharacterColour()
{
#if GEN9_LANDING_PAGE_ENABLED
    // url:bugstar:7098463
    // Always force freemode colours when the landing page is active
    if( CLandingPage::IsActive() )
    {
        return HUD_COLOUR_FREEMODE;
    }
#endif // GEN9_LANDING_PAGE_ENABLED

    return ms_eCurrentCharacterColour;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::GetCharacterColour(...)
// PURPOSE: Retrieves the character colour for specified character index (taken from StatsInterface Model Prefix)
/////////////////////////////////////////////////////////////////////////////////////
eHUD_COLOURS CNewHud::GetCharacterColour(int iCharacter)
{
	eHUD_COLOURS eCharacterColour = HUD_COLOUR_BLACK;

	if(iCharacter >= 0)
	{
		if(iCharacter < MAX_SP_CHARACTER_COLOURS)
		{
			eCharacterColour = ms_eSPCharacterColours[ iCharacter ];
		}
		else
		{
			if(NetworkInterface::IsGameInProgress() && GetDisplayMode() == CNewHud::DM_ARCADE_CNC)
			{
				const CPlayerInfo *pPlayer = CGameWorld::FindLocalPlayer()->GetPlayerInfo();
				if(pPlayer)
				{
					eArcadeTeam ePlayerTeam = pPlayer->GetArcadeInformation().GetTeam();
					if(ePlayerTeam == eArcadeTeam::AT_CNC_COP)
					{
						eCharacterColour = HUD_COLOUR_FREEMODE;
					}
					else if (ePlayerTeam == eArcadeTeam::AT_CNC_CROOK)
					{
						eCharacterColour = HUD_COLOUR_FREEMODE;
					}
				}
			}
			else
			{
				if(CTheScripts::GetCustomMPHudColor() != HUD_COLOUR_INVALID)
				{
					eCharacterColour = eHUD_COLOURS(CTheScripts::GetCustomMPHudColor());
				}
				else
				{
					eCharacterColour = HUD_COLOUR_FREEMODE;
				}
			}
		}
	}

	return eCharacterColour;
}

/*
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::SetSavingText(...)
// PURPOSE: tells scaleform to display the saving spinner.
/////////////////////////////////////////////////////////////////////////////////////
bool CNewHud::SetSavingText(const char* pMessageText, s32 iIconStyle)
{
	if(pMessageText != NULL)
	{
		// setup the hud component
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_SAVING_GAME, "SET_SAVING_TEXT"))
		{
			CScaleformMgr::AddParamInt(iIconStyle);
			CScaleformMgr::AddParamString(pMessageText);
			CScaleformMgr::EndMethod();
		}

		// also set up the standalone version:
		if (CNewHud::IsHudComponentMovieActive(NEW_HUD_SAVING_GAME))
		{
			if (CScaleformMgr::BeginMethod(CNewHud::GetHudComponentMovieId(NEW_HUD_SAVING_GAME), SF_BASE_CLASS_GENERIC, "SET_SAVING_TEXT_STANDALONE"))
			{
				CScaleformMgr::AddParamInt(iIconStyle);
				CScaleformMgr::AddParamString(pMessageText);
				CScaleformMgr::EndMethod();
			}
		}

		PreviousHudValue.bDisplayingSavingMessage = true;
		savegameDisplayf("CNewHud::SetSavingText - setting PreviousHudValue.bDisplayingSavingMessage");

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::HideSavingText()
// PURPOSE: tells scaleform to remove the saving spinner.
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::HideSavingText()
{
	if (PreviousHudValue.bDisplayingSavingMessage)
	{
		// also hide the standalone version:
		if (CNewHud::IsHudComponentMovieActive(NEW_HUD_SAVING_GAME))
		{
			if (CScaleformMgr::BeginMethod(CNewHud::GetHudComponentMovieId(NEW_HUD_SAVING_GAME), SF_BASE_CLASS_GENERIC, "HIDE_SAVING"))
			{
				CScaleformMgr::EndMethod();
			}
		}

		CHudTools::CallHudScaleformMethod(NEW_HUD_SAVING_GAME, "HIDE_SAVING");

		PreviousHudValue.bDisplayingSavingMessage = false;
		savegameDisplayf("CNewHud::HideSavingText - clearing PreviousHudValue.bDisplayingSavingMessage");
	}
}
*/


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateAreaDistrictStreetVehicleNames()
// PURPOSE: updates the area, street and vehicle names
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateAreaDistrictStreetVehicleNames()
{
	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())  // no area names during cutscenes (fixes 1934238)
	{
		return;
	}

	if(IsSniperSightActive())
	{
		return;
	}

	if (CPauseMenu::IsActive() || ms_bHideStreetNamesThisFrame)  // do not update these when in pausemap
	{
		ms_bHideStreetNamesThisFrame = false;
		return;
	}

	bool forceShowButtonPressed = CControlMgr::GetMainFrontendControl().GetHudSpecial().IsPressed();

	if(!CMiniMap::GetIsInsideInterior())
	{
		if ((forceShowButtonPressed || CUserDisplay::AreaName.IsNewName() || DoesScriptWantToForceShowThisFrame(NEW_HUD_AREA_NAME)) && CUserDisplay::AreaName.GetName()[0] != '\0')
		{
			const char *pAreaName = CUserDisplay::AreaName.GetName();

			if (pAreaName)
			{
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_AREA_NAME, "SET_NAME"))
				{
					CScaleformMgr::AddParamString(pAreaName);

					if (CScriptHud::ms_iOverrideTimeForAreaVehicleNames != -1)
					{
						CScaleformMgr::AddParamInt(CScriptHud::ms_iOverrideTimeForAreaVehicleNames);
					}
				
					CScaleformMgr::EndMethod();
				}
			}
		}

		if ((forceShowButtonPressed || CUserDisplay::StreetName.IsNewName() || DoesScriptWantToForceShowThisFrame(NEW_HUD_STREET_NAME)) && CUserDisplay::StreetName.GetName()[0] != '\0')
		{
			const char *pStreetName = CUserDisplay::StreetName.GetName();

			if (pStreetName)
			{
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_STREET_NAME, "SET_NAME"))
				{
					CScaleformMgr::AddParamString(pStreetName);

					if (CScriptHud::ms_iOverrideTimeForAreaVehicleNames != -1)
					{
						CScaleformMgr::AddParamInt(CScriptHud::ms_iOverrideTimeForAreaVehicleNames);
					}

					CScaleformMgr::EndMethod();
				}
			}
		}
	}

	if ((forceShowButtonPressed || CUserDisplay::CurrentVehicle.IsNewName() || DoesScriptWantToForceShowThisFrame(NEW_HUD_VEHICLE_NAME)) && CUserDisplay::CurrentVehicle.GetName()[0] != '\0')
	{
		ActuallyUpdateVehicleName();
	}
}

void CNewHud::ActuallyUpdateVehicleName()
{
	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		CVehicle *pPlayerVehicle = pPlayerPed->GetVehiclePedInside();
		if (pPlayerVehicle)
		{			
			const int cMaxDisplayNameSize = 256;
			char vehicleNameBuffer[cMaxDisplayNameSize] = {0};
			char cVehClassString[cMaxDisplayNameSize] = {0};
			u32 uVehHash = pPlayerVehicle->GetModelNameHash();
			s32 iVehClass = (s32)pPlayerVehicle->GetVehicleModelInfo()->GetVehicleClass();

			// Hack for the Thruster jetpack, classed as a heli but should be military.
			const u32 THRUSTER_JETPACK_HASH = 1489874736;
			if(uVehHash == THRUSTER_JETPACK_HASH)
			{
				const int MILITARY_CLASS = 19;
				iVehClass = MILITARY_CLASS;
			}

			safecpy( vehicleNameBuffer, CUserDisplay::CurrentVehicle.GetName() );
			formatf(cVehClassString, NELEM(cVehClassString), "VEH_CLASS_%d", iVehClass);

			char msgBuffer[cMaxDisplayNameSize] = {0};
			CSubStringWithinMessage sub[2];
			sub[0].SetLiteralString(vehicleNameBuffer, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE, false);
			sub[1].SetLiteralString(TheText.Get(cVehClassString), CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE, false);
			CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("VEH_NAME_CLASS"), NULL, 0, sub, NELEM(sub), msgBuffer, NELEM(msgBuffer));

			if (msgBuffer[0] != '\0')  // only pass in a name if its a valid string
			{
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_VEHICLE_NAME, "SET_NAME"))
				{
					CScaleformMgr::AddParamString(msgBuffer, false);

					if (CScriptHud::ms_iOverrideTimeForAreaVehicleNames != -1)
					{
						CScaleformMgr::AddParamInt(CScriptHud::ms_iOverrideTimeForAreaVehicleNames);
					}

					CScaleformMgr::EndMethod();
				}
			}
		}
	}
}

void CNewHud::FlushAreaDistrictStreetVehicleNames()
{
	CUserDisplay::AreaName.Display();
	CUserDisplay::AreaName.IsNewName();
	CUserDisplay::DistrictName.Display();
	CUserDisplay::DistrictName.IsNewName();
	CUserDisplay::StreetName.Display();
	CUserDisplay::StreetName.IsNewName();
	CUserDisplay::CurrentVehicle.Display();
	CUserDisplay::CurrentVehicle.IsNewName();
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateAbility()
// PURPOSE: updates the special ability on the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateAbility()
{
	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

	if (!pPlayerPed)
		return;

	CPlayerSpecialAbility* pSpecialAbility = pPlayerPed->GetSpecialAbility();

	bool bSpecialAbilityActive = (pSpecialAbility && pSpecialAbility->IsActive());

	if (bSpecialAbilityActive != PreviousHudValue.bAbilityBarActive)
	{
		if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "FLASH_ABILITY_BAR"))
		{
			CScaleformMgr::AddParamInt(500);
			CScaleformMgr::AddParamInt(5);
			CScaleformMgr::EndMethod();
		}

		PreviousHudValue.bAbilityBarActive = bSpecialAbilityActive;
	}

	if( !ms_bOverrideSpecialAbility )
	{
		if (pSpecialAbility)
		{
			ms_SpecialAbility.fPercentage = pSpecialAbility->GetRemainingPercentageForDisplay();
			ms_SpecialAbility.fPercentageCapacity = pSpecialAbility->GetCapacityPercentageForDisplay();
		}
		else
		{
			ms_SpecialAbility.fPercentage = 0.0f;
			ms_SpecialAbility.fPercentageCapacity = 100.0f;
		}
	}
	

	Assertf(ms_SpecialAbility.fPercentage <= 100.0f, "Special ability is over 100.0f when its suppose to be 0.0f-100.0f (%f)", ms_SpecialAbility.fPercentage);

	// You still need to set the ability if it gets to zero (eg when swapping between one char who has complete special ability and one who has none)
	//if (ms_SpecialAbility.fPercentage >= 0.0f)
	if( PreviousHudValue.m_fAirPercentage == FULL_AIR_BAR ) // if we're not underwater at this time, because the meters will fight
	{
		if (ms_SpecialAbility != PreviousHudValue.SpecialAbility )
		{
			if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_ABILITY_BAR"))
			{
				CScaleformMgr::AddParamFloat(ms_SpecialAbility.fPercentage);
				CScaleformMgr::AddParamBool(ms_SpecialAbility.fPercentage > PreviousHudValue.SpecialAbility.fPercentage);
				CScaleformMgr::AddParamFloat(ms_SpecialAbility.fPercentageCapacity);
				CScaleformMgr::EndMethod();
			}

			PreviousHudValue.SpecialAbility = ms_SpecialAbility;
		}
	}
}

void CNewHud::BlinkAbilityBar(int millisecondsToFlash)
{
	if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "BLINK_ABILITY_BAR"))
	{
		CScaleformMgr::AddParamInt(millisecondsToFlash);
		CScaleformMgr::EndMethod();
	}
}

void CNewHud::SetToggleAbilityBar(bool bTurnOn)
{
	if(ms_bAllowAbilityBar)
	{
		ms_bOverrideSpecialAbility = bTurnOn;
		if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_ABILITY_BAR_VISIBILITY_IN_MULTIPLAYER")) 
		{ 
			CScaleformMgr::AddParamBool(ms_bOverrideSpecialAbility); 
			CScaleformMgr::EndMethod(); 
		}
	}
}

void CNewHud::SetAbilityBarGlow(bool bTurnOn)
{
	
	if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_ABILITY_BAR_GLOW")) 
	{ 
		CScaleformMgr::AddParamBool(bTurnOn); 
		CScaleformMgr::EndMethod(); 
	}
}

void CNewHud::SetAbilityOverride(float fPercentage, float fMaxPercentage)
{
	ms_SpecialAbility.fPercentage = fPercentage;
	ms_SpecialAbility.fPercentageCapacity = fMaxPercentage;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateAirBar()
// PURPOSE: updates the air bar on the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateAirBar()
{
	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

	if (!pPlayerPed || (pPlayerPed && pPlayerPed->IsDead()))
	{
		PreviousHudValue.fAirPercentageWhileSubmerged = FULL_AIR_BAR;
		return;
	}

	float fAirPercentage = 0.0f;

	const CInWaterEventScanner& waterScanner = pPlayerPed->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
	float fTimeUnderWater = waterScanner.GetTimeSubmerged();
	float fTimeAboveWater = waterScanner.GetTimeOnSurface();

	static const float f_REBREATH_TIME = 100.f;
	static const float f_SURFACING_DELAY = 0.5f;

	if (fTimeUnderWater != 0.0f && fTimeAboveWater <= f_SURFACING_DELAY)
	{
		float fMaxTimeUnderWater = pPlayerPed->GetMaxTimeUnderwater();

		if (fMaxTimeUnderWater >= fTimeUnderWater)
		{
			fAirPercentage = (FULL_AIR_BAR - (fTimeUnderWater / fMaxTimeUnderWater) * FULL_AIR_BAR);
		}
		PreviousHudValue.fAirPercentageWhileSubmerged = fAirPercentage;
	}
	// for url:bugstar:1905314, we want to have a bit of a 'breathing time'
	// except that the WaterScanner doesn't track that (you're instantly back to full breath)
	// so we fake it this way.
	else if( !Approach(PreviousHudValue.fAirPercentageWhileSubmerged, 100.0f, f_REBREATH_TIME, fwTimer::GetTimeStep()) )
	{
		fAirPercentage = PreviousHudValue.fAirPercentageWhileSubmerged;
	}
	else
	{
		fAirPercentage = FULL_AIR_BAR;
	}

	Assertf(fAirPercentage <= FULL_AIR_BAR, "Air is over 100.0f when its suppose to be 0.0f-100.0f (%f)", fAirPercentage);

	//if (fAirPercentage >= 0.0f)
	{
		if (fAirPercentage != PreviousHudValue.m_fAirPercentage)
		{
			if (fAirPercentage!=FULL_AIR_BAR) 
			{
				if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_AIR_BAR"))
				{	
					CScaleformMgr::AddParamFloat(fAirPercentage);
					CScaleformMgr::EndMethod();
				}
			}
			else
			{
				if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "SET_AIR_BAR"))
				{	
					CScaleformMgr::AddParamFloat(-1.0f);
					CScaleformMgr::EndMethod();
				}
			}

			PreviousHudValue.m_fAirPercentage = fAirPercentage;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateWeaponAndAmmo()
// PURPOSE: updates the weapon and ammo component
// NOTE:    Before changing anything in this method you must ensure what you are
//			changing wont cause the scaleform methods to be called every frame!
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateWeaponAndAmmo()
{
	// We do want to show this after all.  In fact, we want to FORCE show it
	if(ms_WeaponWheel.IsVisible())
		SetHudComponentToBeShown(NEW_HUD_WEAPON_ICON);

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif // GTA_REPLAY

	const CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
	if (!pPlayerPed)
		return;

	const CPedWeaponManager* pPlayerWeaponManager = pPlayerPed->GetWeaponManager();
	if (!pPlayerWeaponManager)
		return;

	//
	// get the current weapon:
	//
	weaponAssert(pPlayerWeaponManager);
	u32 iCurrentWeaponHash = pPlayerWeaponManager->GetSelectedWeaponHash();

	//
	// get the current ammo:
	//
	s32 iAmmoClipSize = 0;
	s32 iDisplayClipAmmo = 0;
	s32 iDisplayAmmo = 0;
	s32 iAmmoSpecialType = 0;

	bool bUpdateWeaponAndAmmo = false;
	bool bForceTimerUpdate = false;
	f32 fWeaponTimerPercentage = -1.0f;
	bool bUsingChernobog = false;

	const CWeapon *pWeapon = pPlayerWeaponManager->GetEquippedWeapon();
	const bool bPedInVehicle = pPlayerPed->GetIsInVehicle();
	bool bUsingRestrictedVehicleAmmo = false;
	bool bInBlazer = false;

	if (iCurrentWeaponHash == 0 && bPedInVehicle && pWeapon)
	{
		const CVehicle* pVehicle = pPlayerPed->GetMyVehicle();
		u32 uVehicleHash = pVehicle->GetVehicleModelInfo()->GetModelNameHash();
		if (uVehicleHash == ATSTRINGHASH("BLAZER5", 0xA1355F67))
		{
			iCurrentWeaponHash = pWeapon->GetWeaponHash();
			bInBlazer = true;
		}

		// B*3650649: Show ammo count for vehicle weapons with a restricted ammo count (set by script; default is infinite).
		if (iCurrentWeaponHash == 0)
		{
			const CVehicleWeapon* pEquippedVehicleWeapon = pPlayerWeaponManager->GetEquippedVehicleWeapon();
			if (pEquippedVehicleWeapon)
			{
				const CVehicleWeaponMgr* pVehicleWeaponManager = pVehicle->GetVehicleWeaponMgr();
				const s32 iVehicleWeaponMgrIndex = pVehicleWeaponManager ? pVehicleWeaponManager->GetIndexOfVehicleWeapon(pEquippedVehicleWeapon) : -1;
				if (iVehicleWeaponMgrIndex != -1)
				{
					const s32 iRestrictedAmmoCount = pVehicle->GetRestrictedAmmoCount(iVehicleWeaponMgrIndex);
					if (iRestrictedAmmoCount != -1)
					{
						bUsingRestrictedVehicleAmmo = true;
						iCurrentWeaponHash = pWeapon->GetWeaponHash();
						iAmmoClipSize = 1;
						iDisplayAmmo = iRestrictedAmmoCount;
						iDisplayClipAmmo = 0;
					}
				}
			}
		}

		// B*4200907
		if(uVehicleHash == ATSTRINGHASH("chernobog", 0xD6BC7523))
		{
			if(pPlayerPed->GetPedIntelligence())
			{
				bUsingChernobog = true;
				f32 fTimeToFire = pPlayerPed->GetPedIntelligence()->GetFiringPattern().GetTimeTillNextBurst();
				bForceTimerUpdate = true;
				if(fTimeToFire > 0.0f)
				{
					const float TIME_TO_RECHARGE = 4.0f;
					fWeaponTimerPercentage = 100.0f - ((abs(fTimeToFire - TIME_TO_RECHARGE) / TIME_TO_RECHARGE) * 100.0f);
				}
				else
				{
					fWeaponTimerPercentage = 0.0f;
				}

				// TODO: Currently using the stungun hash to avoid having to update and rebuild all the HUD actionscript.
				// After Christmas2017 DLC update it so that this gets it's own special condition.
				iCurrentWeaponHash = 911657153;
				bUpdateWeaponAndAmmo = true;
			}
		}
	}

	if (!bUsingRestrictedVehicleAmmo)
	{
		if (pWeapon && pWeapon->GetWeaponHash() == iCurrentWeaponHash)
		{
			// Attempt to get the ammo from the CWeapon, as this maintains the current ammo in the clip
			const CWeaponInfo *pWeaponInfo = pWeapon->GetWeaponInfo();

			if (pWeaponInfo)
			{
				iAmmoClipSize = pWeapon->GetClipSize();
				iDisplayAmmo = pWeapon->GetAmmoTotal();
				iDisplayClipAmmo = pWeapon->GetAmmoInClip();
			}
		}
		else
		{
			// Get the ammo for the selected weapon from the inventory
			const CWeaponItem* pWeaponItem = iCurrentWeaponHash != 0 ? pPlayerPed->GetInventory()->GetWeapon(iCurrentWeaponHash) : NULL;

			if (pWeaponItem)
			{
				weaponAssert(pWeaponItem->GetInfo());
				iAmmoClipSize = pWeaponItem->GetInfo()->GetClipSize();

				if (pWeaponItem->GetComponents())
				{
					const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
					for (s32 i = components.GetCount()-1; i >= 0; i--)
					{
						if (components[i].pComponentInfo->GetIsClass<CWeaponComponentClipInfo>())
						{
							iAmmoClipSize = static_cast<const CWeaponComponentClipInfo*>(components[i].pComponentInfo)->GetClipSize();
							break;
						}
					}
				}

				iDisplayAmmo = pPlayerPed->GetInventory()->GetWeaponAmmo(iCurrentWeaponHash);
				iDisplayClipAmmo = Min(iDisplayAmmo, iAmmoClipSize);

				// B*2295321: Display cached ammo in clip value if we've stored the previous clip info.
				if (pWeaponItem->GetLastAmmoInClip() != -1)
				{
					iDisplayClipAmmo = pWeaponItem->GetLastAmmoInClip();
				}

				if(CNewHud::GetWheelDisableReload())
				{
					iDisplayClipAmmo = CNewHud::GetWheelHolsterAmmoInClip();
				}
			}
		}

	}

	// Always update the ammo special type from the weapon hash
	if (pWeapon && pWeapon->GetWeaponHash() == iCurrentWeaponHash)
	{
		const CWeaponInfo *pWeaponInfo = pWeapon->GetWeaponInfo();
		if (pWeaponInfo)
		{
			const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(pPlayerPed);
			iAmmoSpecialType = pAmmoInfo ? (s32)pAmmoInfo->GetAmmoSpecialType() : 0;
		}
	}

	if (pPlayerPed->GetInventory()->GetAmmoRepository().GetIsUsingInfiniteAmmo() || (iCurrentWeaponHash&&pPlayerPed->GetInventory()->GetIsWeaponUsingInfiniteAmmo(iCurrentWeaponHash)) )  // if we have infinite ammo pass in -2
	{
		static const s32 useThisFlag = __SPECIAL_FLAG_USING_INFINITE_AMMO;
		iDisplayAmmo = useThisFlag;
		iDisplayClipAmmo = useThisFlag;
		iAmmoClipSize = useThisFlag;
	}
	else
	{
		// we have some sort of ammo:
		if (iAmmoClipSize > 1 && iAmmoClipSize < 1000)  // yes this weapon uses a clip
		{
			iDisplayAmmo = iDisplayAmmo - iDisplayClipAmmo;
		}
		iDisplayAmmo = rage::Max(iDisplayAmmo, 0);
		iDisplayClipAmmo = rage::Max(iDisplayClipAmmo, 0);
	}
	
	//
	// these checks decide whether we re-trigger the display of the weapon and ammo.
	// IMPORTANT: We need to only call the code wrapped in bUpdateWeaponAndAmmo once when something changes. 
	//
	bool bReloading = false;
	bool bFiring = false;
	bool bAiming = false;
	bool bAimingOrReloading = DoesScriptWantToForceShowThisFrame(NEW_HUD_WEAPON_ICON);

	const CTaskGun* gunTask = static_cast<CTaskGun*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
	if(gunTask)
	{
		bFiring = (gunTask->GetIsFiring());
		bAiming = (gunTask->GetIsAiming()) && (!bFiring) && (!camInterface::GetGameplayDirector().IsCameraInterpolating());
		bReloading = (gunTask->GetIsReloading()) && (!bAiming) && (!bFiring);
		bAimingOrReloading = (bAiming || bReloading || bFiring);
	}

	//
	// grenade/projectile:
	//
	const CTaskAimAndThrowProjectile* pAimTask = static_cast<CTaskAimAndThrowProjectile*>( pPlayerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE ) );
	if (pWeapon && pWeapon->GetWeaponInfo())
	{
		if (pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f)
		{
			fWeaponTimerPercentage = (((pWeapon->GetIsCooking() ? (float)(fwTimer::GetTimeInMilliseconds() - pWeapon->GetCookTime())/1000.0f : 0.0f) / pWeapon->GetWeaponInfo()->GetProjectileFuseTime(pPlayerPed->GetIsInVehicle())) * 100.0f);  // % before grenade blows

			if (fWeaponTimerPercentage > 0.0f)  // if we have a percentage value then we need to make it from 100 to 0
			{
				fWeaponTimerPercentage = (100.0f-fWeaponTimerPercentage);  // convert it to count down from 100 to 0.
			}
		}
		else
		{
			if(!bUsingChernobog)
			{
				fWeaponTimerPercentage = 0.0f;
			}
		}
	}
	else
	{
		if(PreviousHudValue.fWeaponTimerPercentage > 0.0f)
		{
			fWeaponTimerPercentage = 0.0f;
			bForceTimerUpdate = true;
		}
	}

	if (pAimTask)
	{
		bAimingOrReloading = true;

		bool bDroppingProjectile = pAimTask->IsDroppingProjectile();

		if (bDroppingProjectile != PreviousHudValue.bDroppingProjectile)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_ICON, "SHOW_GRENADE_DROP"))
			{
				CScaleformMgr::AddParamBool(bDroppingProjectile);
				CScaleformMgr::EndMethod();
			}

			PreviousHudValue.bDroppingProjectile = bDroppingProjectile;
		}
	}
	else
	{
		PreviousHudValue.bDroppingProjectile = false;

		// grenades and stun guns use the same variable. So blanking this out effs up our shee.
		if( !pWeapon || !pWeapon->GetWeaponInfo() || !(pWeapon->GetWeaponInfo()->GetDisplayRechargeTimeHUD() || pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f ) )
		{
			if(!bUsingChernobog)
			{
				PreviousHudValue.fWeaponTimerPercentage = -1.0f;
			}
		}
	}

	//
	// recharge (stun gun):
	//
	static f32 fMaxTimeBeforeNextShot = 0.0f;

	if (pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetDisplayRechargeTimeHUD() )
	{
		// Any time we're using 'DisplayRechargeTimeHUD' flag, force the weapon hash to WEAPON_STUNGUN in order to display the scaleform bar correctly...
		iCurrentWeaponHash = 911657153;

		f32 fTimeBetweenShots = pWeapon->GetWeaponInfo()->GetTimeBetweenShots();
		f32 fTimeBeforeNextShot = pWeapon->GetTimeBeforeNextShot();
		if( fTimeBeforeNextShot == 0.0f)
			fWeaponTimerPercentage = 0.0f;

		else if( fTimeBeforeNextShot == -1.0f ) // out of ammo/reloading
			fWeaponTimerPercentage = 100.0f;
		else
		{
			if(pWeapon->GetWeaponInfo()->GetHash() == ATSTRINGHASH("WEAPON_STUNGUN_MP", 0x45CD9CF3))
			{
				fWeaponTimerPercentage = ((fTimeBeforeNextShot / fTimeBetweenShots) * 100.0f); 
			}
			else
			{
				fMaxTimeBeforeNextShot = Max(fTimeBeforeNextShot, fMaxTimeBeforeNextShot);  // we always store the max we recieve so we can work out percentage

				fWeaponTimerPercentage = ((fTimeBeforeNextShot / fMaxTimeBeforeNextShot) * 100.0f);  // % before weapon is recharged
			}

		}		
	}
	else
	{
		// Keep recharging if the ped still has a stun gun in the inventory when the ped is entering/exiting a vehicle. 
		// Fix the stun gun stopping charging after the ped enters/exits a vehicle. The weapon object is destroyed and recreated in this case. 
		if(!pWeapon)
		{
			const CPedInventory* pInventory = pPlayerPed->GetInventory();
			const CWeaponItem* pWeaponItem = (pInventory && iCurrentWeaponHash) ? pInventory->GetWeapon(iCurrentWeaponHash) : NULL;
			if(pWeaponItem)
			{
				if(pInventory && pWeaponItem->GetInfo()->GetDisplayRechargeTimeHUD())
				{
					f32 fTimeBeforeNextShot = (static_cast<f32>(pWeaponItem->GetTimer()) - static_cast<f32>(fwTimer::GetTimeInMilliseconds())) / 1000.0f;
					if( fTimeBeforeNextShot <= 0.0f)
					{
						fWeaponTimerPercentage = 0.0f;
						fMaxTimeBeforeNextShot = 0.0f;
					}
					else
					{
						fMaxTimeBeforeNextShot = Max(fTimeBeforeNextShot, fMaxTimeBeforeNextShot);  // we always store the max we recieve so we can work out percentage
						fWeaponTimerPercentage = ((fTimeBeforeNextShot / fMaxTimeBeforeNextShot) * 100.0f);  // % before weapon is recharged
					}		
				}
				else
				{
					fMaxTimeBeforeNextShot = 0.0f;
				}
			}
			else
			{
				fMaxTimeBeforeNextShot = 0.0f;
			}
		}
		else
		{
			fMaxTimeBeforeNextShot = 0.0f;
		}
	}

	if (fWeaponTimerPercentage >= 0.0f && fWeaponTimerPercentage != PreviousHudValue.fWeaponTimerPercentage)
	{
		if (fWeaponTimerPercentage != 0.0f || PreviousHudValue.fWeaponTimerPercentage != -1.0f || bForceTimerUpdate)
		{
			bAimingOrReloading = true;

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_ICON, "SET_WEAPON_TIMER"))
			{
				CScaleformMgr::AddParamFloat(fWeaponTimerPercentage);
				CScaleformMgr::EndMethod();
			}

			if(iDisplayAmmo == __SPECIAL_FLAG_USING_INFINITE_AMMO)
			{
				m_reticule.SetWeaponTimer(fWeaponTimerPercentage);
			}

			PreviousHudValue.fWeaponTimerPercentage = fWeaponTimerPercentage;
		}
	}

	if (bFiring)
	{
		bUpdateWeaponAndAmmo = true;
	}

	const u32 uUnarmedWeaponHash = 0xa2719263;
	bool bIsValidWeapon = (pWeapon && pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun());
	bool bNoAmmoDisplay = (pWeapon && pWeapon->GetWeaponInfo()->GetFlag(CWeaponInfoFlags::NoAmmoDisplay));
	bool bIsBlazerWeaponEquipped = iCurrentWeaponHash == uUnarmedWeaponHash && bInBlazer;

	if ( ( bAimingOrReloading && !PreviousHudValue.bAimingOrReloading && bIsValidWeapon && !bNoAmmoDisplay ) || bIsBlazerWeaponEquipped )
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_ICON, "SET_WEAPONS_AND_AMMO_FOREVER"))
		{
			CScaleformMgr::AddParamBool(true);
			CScaleformMgr::EndMethod();
		}
		bUpdateWeaponAndAmmo = true;
	}
	else if (!bAimingOrReloading && PreviousHudValue.bAimingOrReloading && bIsValidWeapon)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_ICON, "SET_WEAPONS_AND_AMMO_FOREVER"))
		{
			CScaleformMgr::AddParamBool(false);
			CScaleformMgr::EndMethod();
		}
	}


	// now we can see if we need to show the weapon icon anyway (due to weapon icon changing etc)
	if (!bUpdateWeaponAndAmmo)
	{
		bUpdateWeaponAndAmmo = (	((iCurrentWeaponHash != 0) && ((pPlayerWeaponManager->GetSelectedVehicleWeaponIndex() == -1) || bUsingRestrictedVehicleAmmo)) &&  // no vehicle weapon selected
									(iCurrentWeaponHash != PreviousHudValue.iWeaponHash ||  // weapon has changed
									iDisplayAmmo != PreviousHudValue.iAmmo ||  // ammo has changed
									iDisplayClipAmmo != PreviousHudValue.iClipAmmo));
	}

	if (bUpdateWeaponAndAmmo)
	{
		// if weapon has changed then remove the current reticle:
		if (iCurrentWeaponHash == 0 && PreviousHudValue.iWeaponHash != 0)
		{
			PreviousHudValue.iAmmo = -1;  // when weapon changes, reset the ammo counts so we always ensure they are updated
			PreviousHudValue.iClipAmmo = -1;

			CHudTools::CallHudScaleformMethod(NEW_HUD_RETICLE, "REMOVE");
		}

		if (iCurrentWeaponHash != 0 && PreviousHudValue.iWeaponHash != iCurrentWeaponHash)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "LOAD_RETICLE"))
			{
				CScaleformMgr::AddParamInt(iCurrentWeaponHash);
				CScaleformMgr::EndMethod();
			}

			m_reticule.GetPreviousValues().ResetDisplay();
		}

		// find out whether we are going for the next or previous weapon
		s32 iMovementDirection = 0;

		const CControl* pControl = pPlayerPed->GetControlFromPlayer();
		if(pControl)
		{
			if(pControl->GetWeaponWheelNext().IsPressed())
			{
				iMovementDirection = 1;
			}
			else if(pControl->GetWeaponWheelPrev().IsPressed())
			{
				iMovementDirection = -1;
			}
		}

		bool bInCutscene = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());
		bool bForceShowAmmo = DoesScriptWantToForceShowThisFrame(NEW_HUD_WEAPON_ICON) || 
			bIsValidWeapon ||
			pPlayerPed->IsUsingActionMode();

		// always show ammo when you change weapon
		bool bValidInventory = pPlayerPed && pPlayerPed->GetInventory() && iCurrentWeaponHash != 0;
		const CWeaponItem* pWeaponItem = bValidInventory ? pPlayerPed->GetInventory()->GetWeapon(iCurrentWeaponHash) : NULL;
		bool bUsesAmmo = (pWeaponItem && pWeaponItem->GetInfo()->GetUsesAmmo()) || bUsingRestrictedVehicleAmmo;
		bool bChangedWeapon = iCurrentWeaponHash != PreviousHudValue.iWeaponHash;
		bool bNoAmmoDisplay = pWeaponItem && pWeaponItem->GetInfo()->GetFlag(CWeaponInfoFlags::NoAmmoDisplay);

		// Bomber planes
		if(bPedInVehicle)
		{
			bool bBombBayDoorsOpen = false;
			CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();
			if(pVehicle)
			{
				for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
				{
					CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);
					if(pVehicleGadget->GetType() == VGT_BOMBBAY)
					{
						CVehicleGadgetBombBay *pBombBay = static_cast<CVehicleGadgetBombBay*>(pVehicleGadget);
						bBombBayDoorsOpen = pBombBay->GetOpen();

						if(bBombBayDoorsOpen)
						{
#define BOMB_BAY 1059723823
							iCurrentWeaponHash = BOMB_BAY;
							iDisplayAmmo = pVehicle->GetBombAmmoCount();
							iDisplayClipAmmo = iDisplayAmmo;
							bUsesAmmo = true;
						}

						break;
					}
				}
			}
		}

		if (((iCurrentWeaponHash != PreviousHudValue.iWeaponHash) ||  // only call SET_PLAYER_WEAPON if the weapon actually changes
			(bAimingOrReloading && !PreviousHudValue.bAimingOrReloading)) &&
			iCurrentWeaponHash != 0)
		{
			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_ICON, "SET_PLAYER_WEAPON"))
			{
				CScaleformMgr::AddParamInt(iCurrentWeaponHash);
				CScaleformMgr::AddParamInt(iMovementDirection);
				CScaleformMgr::AddParamBool(!bInCutscene && !bNoAmmoDisplay);  // flag to say whether we should show the icon or not
				CScaleformMgr::EndMethod();

				PreviousHudValue.iWeaponHash = iCurrentWeaponHash;
			}

			if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICULE_BOOST"))
			{
				CScaleformMgr::AddParamFloat(1.0f + ((float)CPauseMenu::GetMenuPreference(PREF_RETICULE_SIZE) / 10.0f));
				CScaleformMgr::EndMethod();
			}
		}

		bool bAmmoChanged = ((iDisplayAmmo != PreviousHudValue.iAmmo) ||
			(iDisplayClipAmmo != PreviousHudValue.iClipAmmo));
		
		if ((bForceShowAmmo || (bAmmoChanged && bUsesAmmo) || (bChangedWeapon)) && iCurrentWeaponHash != 0 && !bNoAmmoDisplay)
		{
			if (!bInCutscene)  // dont invoke this if a cutscene is running
			{
				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_ICON, "SET_AMMO_COUNT"))
				{
					CScaleformMgr::AddParamInt(iDisplayAmmo);
					CScaleformMgr::AddParamInt(iDisplayClipAmmo);
					CScaleformMgr::AddParamInt(iAmmoClipSize);
					CScaleformMgr::AddParamInt(iAmmoSpecialType);
					CScaleformMgr::EndMethod();
					
					PreviousHudValue.iAmmo = iDisplayAmmo;
					PreviousHudValue.iClipAmmo = iDisplayClipAmmo;
				}
			}
		}
	}

#if __BANK
	if(ms_bShowEverything)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_ICON, "SET_AMMO_COUNT"))
		{
			CScaleformMgr::AddParamInt(1111);
			CScaleformMgr::AddParamInt(2222);
			CScaleformMgr::AddParamInt(3333);
			CScaleformMgr::EndMethod();
		}
	}
#endif // __BANK

	PreviousHudValue.bAimingOrReloading = bAimingOrReloading;
}

bool CNewHud::IsSniperSightActive()
{
	const CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

	if (!pPlayerPed)
		return false;

	//
	// get the current weapon:
	//
	u32 iVehicleWeaponHash = 0;

	const CPedWeaponManager* pPlayerWeaponManager = pPlayerPed->GetWeaponManager();
	if (!pPlayerWeaponManager) //AI debug switch to focus entity, could be an animal
		return false;

	const CVehicleWeapon* pEquippedVehicleWeapon = pPlayerWeaponManager->GetEquippedVehicleWeapon();
	const CWeaponInfo *pWeaponInfo  = NULL;
	const CWeapon* pPlayerEquippedWeapon = pPlayerWeaponManager->GetEquippedWeapon();

	// If we're paused, then we only want to show the reticle if we're forced into sniper mode.
	if(CPauseMenu::IsActive() && (!pPlayerEquippedWeapon || !pPlayerEquippedWeapon->GetHasFirstPersonScope()))
	{
		return false;
	}

	if (pEquippedVehicleWeapon)
	{
		pWeaponInfo = pEquippedVehicleWeapon->GetWeaponInfo();
		iVehicleWeaponHash = pEquippedVehicleWeapon->GetHash();
	}


	if ((!pEquippedVehicleWeapon) || iVehicleWeaponHash == 0)
	{
		if (pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope())
		{
			const camBaseCamera* pDominantCam = camInterface::GetDominantRenderedCamera(); 
			const bool bFirstPersonPedAimCamera = (pDominantCam && pDominantCam->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()));

			//If the gameplay director isn't rendering a first-person aim camera, fall-back to using a third-person reticule.
			if(bFirstPersonPedAimCamera && !ShouldHideHud(false)) 
			{
				return true;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::Render()
// PURPOSE: renders each hud component
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::Render()
{
#if	!__FINAL
	if (PARAM_nohud.Get())
		return;
#endif // !__FINAL

#if __BANK
	if (!CHudTools::bDisplayHud)
		return;
#endif // __BANK

	AssertMsg(!gDrawListMgr->IsBuildingDrawList(), "CNewHud::Render can only be called on the RenderThread!");

	if (gDrawListMgr->IsBuildingDrawList())
		return;

	if (!HudComponent[NEW_HUD].bActive)
		return;

////#if GTA_REPLAY
////	if (CUIReplayMgr::IsInitialized())
////	{
////		CUIReplayMgr::Render();
////	}
////#endif // GTA_REPLAY

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{
		CShaderLib::SetReticuleDistTexture(CNewHud::IsReticuleVisible());

		// manual depth adjustment for w.wheel
		CShaderLib::SetStereoParams1(Vector4(0.8f,1.0f,1.0f,1.0f));
	}
#endif

	PF_PUSH_TIMEBAR("Scaleform Movie Render (Hud)");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	if (IsHudComponentMovieActive(NEW_HUD))  // do not update during load
	{
		bool bUpdateWhenPaused = false;
#if __BANK
		// special support for updating help text when paused. Initially for marketing use
		if (fwTimer::IsGamePaused() && bUpdateHelpTextWhenPaused)
		{
			// NOTE: we use the system time step here to allow the text to time-out and fade correctly when paused
			bUpdateWhenPaused = true;
		}
#endif // __BANK

#if GTA_REPLAY
		if (CVideoEditorInterface::IsActive())
		{
			bUpdateWhenPaused = true;
		}
#endif //GTA_REPLAY

		static bool bRepositionSavingComponent = false;

		if (CanRenderHud())
		{	
			CScaleformMgr::RenderMovie(HudComponent[NEW_HUD].iId, fwTimer::GetSystemTimeStep(), bUpdateWhenPaused);
			bRepositionSavingComponent = false;
		}
		else
		{
			bool bFoundOneStandaloneActive = false;

			// render any hud components (other than NEW_HUD) that are independent from the hud:
			for (s32 iCount = BASE_HUD_COMPONENT; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
			{
				if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
					continue;

				if (HudComponent[iCount].bStandaloneMovie)
				{
					if (iCount == NEW_HUD_SAVING_GAME && IsHudComponentActive(iCount) && IsHudComponentMovieActive(iCount))
					{
						//
						// This is not the nicest way to do this but I cant see any other effective way of doing this at this stage without major implications
						// or changes we dont have time for.
						// saving spinner needs to move up a bit when we are paused to avoid it overlapping pausemenu stuff, this does it...
						//
						if (PreviousHudValue.bDisplayingSavingMessage)
						{
							bool bChangedValuesThisFrame = false;

							if (CPauseMenu::IsActive() || CWarningScreen::IsActive() || cStoreScreenMgr::IsStoreMenuOpen())
							{
								if (!bRepositionSavingComponent)
								{
#define __AMOUNT_TO_SHIFT_SAVING_SPINNER_UP_WHEN_PAUSED (0.04f)
									GFxMovieView::ScaleModeType eScaleMode = GetHudScaleMode(iCount);

									Vector2 vNewPos = CHudTools::CalculateHudPosition(Vector2(HudComponent[iCount].vPos.x, HudComponent[iCount].vPos.y-__AMOUNT_TO_SHIFT_SAVING_SPINNER_UP_WHEN_PAUSED), HudComponent[iCount].vSize, HudComponent[iCount].cAlignX[0], HudComponent[iCount].cAlignY[0]);

									CScaleformMgr::ChangeMovieParams(HudComponent[iCount].iId, Vector2(vNewPos.x, vNewPos.y), HudComponent[iCount].vSize, eScaleMode);

									bRepositionSavingComponent = true;
								}
			
								bChangedValuesThisFrame = true;
							}

							if ( (!bChangedValuesThisFrame) && (bRepositionSavingComponent) )
							{
								Vector2 vNewPos = CHudTools::CalculateHudPosition(Vector2(HudComponent[iCount].vPos.x, HudComponent[iCount].vPos.y), HudComponent[iCount].vSize, HudComponent[iCount].cAlignX[0], HudComponent[iCount].cAlignY[0]);

								GFxMovieView::ScaleModeType eScaleMode = GetHudScaleMode(iCount);
								CScaleformMgr::ChangeMovieParams(HudComponent[iCount].iId, vNewPos, HudComponent[iCount].vSize, eScaleMode);

								bRepositionSavingComponent = false;
							}

							CScaleformMgr::RenderMovie(HudComponent[iCount].iId, fwTimer::GetSystemTimeStep(), true);
							bFoundOneStandaloneActive = true;
						}
					}
					else
					{
						bRepositionSavingComponent = false;
					}
				}
			}
			
			if (bFoundOneStandaloneActive)
			{
				CScaleformMgr::UpdateMovieOnly(HudComponent[NEW_HUD].iId, fwTimer::GetSystemTimeStep());
			}
		}
	}

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
		CShaderLib::SetReticuleDistTexture(false);
#endif

	PF_POP_TIMEBAR();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::DeviceLost() and CNewHud::DeviceReset()
// PURPOSE: Callback functions for device lost and reset for PC
/////////////////////////////////////////////////////////////////////////////////////
#if RSG_PC
void CNewHud::DeviceLost()
{

}

void CNewHud::DeviceReset()
{
	if (ms_Initialized)
	{
		ms_bDeviceReset = true;
		Init(INIT_SESSION);
	}
}
#endif //RSG_PC




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::CanRenderHud()
// PURPOSE: returns whether we can/should render the hud this frame.  This allows
//			us to know if we can render but also if we are safe to restart the hud
//			because it is not onscreen
/////////////////////////////////////////////////////////////////////////////////////
bool CNewHud::CanRenderHud()
{
#if RSG_PC & GTA_REPLAY
	if (g_rlPc.IsUiShowing())
	{
		if (!CReplayCoordinator::IsExportingToVideoFile())
			return false;
	}
#endif

	bool bDisplayWhenPaused = false;
	bool bDisplayWhenNotInStateOfPlay = false;

	if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		bDisplayWhenPaused = CScriptHud::ms_bDisplayHudWhenPausedThisFrame.GetWriteBuf();
		bDisplayWhenNotInStateOfPlay = CScriptHud::ms_bDisplayHudWhenNotInStateOfPlayThisFrame.GetWriteBuf();
	}
	else
	{
		bDisplayWhenPaused = CScriptHud::ms_bDisplayHudWhenPausedThisFrame.GetReadBuf();
		bDisplayWhenNotInStateOfPlay = CScriptHud::ms_bDisplayHudWhenNotInStateOfPlayThisFrame.GetReadBuf();
	}

#if RSG_PC
	bDisplayWhenPaused |= ioKeyboard::ImeIsInProgress();
	bDisplayWhenNotInStateOfPlay |= ioKeyboard::ImeIsInProgress();
#endif // RSG_PC

	bool bAreAnyHudComponentsActive = false;

	for (s32 iCount = BASE_HUD_COMPONENT; (iCount < TOTAL_HUD_COMPONENT_LIST_SIZE); iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		if (IsHudComponentActive(iCount))
		{
			if ((iCount == NEW_HUD_RETICLE) && !IsReticuleVisible())
			{
				continue;
			}

			bAreAnyHudComponentsActive = true;
			break;
		}
	}

	bool isReplayEditModeActive = false;
#if GTA_REPLAY
	isReplayEditModeActive = CReplayMgr::IsEditModeActive();
#endif

	return ( (((CGameLogic::IsGameStateInPlay() || bDisplayWhenNotInStateOfPlay) &&
			   ( !CPauseMenu::IsActive(PM_SkipSocialClub) || bDisplayWhenPaused || isReplayEditModeActive) ) &&
			   !SocialClubMenu::IsActive() &&
			   (!camInterface::IsFadedOut()) ) &&
			   (bAreAnyHudComponentsActive))
			   ;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateDpadDown()
// PURPOSE: certain things in the hud are triggered when the player pressed DPAD DOWN
//			this function deals with that
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateDpadDown()
{
	if (ms_bHudIsHidden)  // do not update any dpad down logic when hud is hidden
		return;

	CPed * pPlayerPed = FindPlayerPed();

	if (!pPlayerPed)
		return;

	const CControl* pControl = pPlayerPed->GetControlFromPlayer();
	if (pControl && pControl->GetHudSpecial().IsDown() && (!CPhoneMgr::IsDisplayed()))
	{
		ms_iGpsDpadZoomTimer = fwTimer::GetTimeInMilliseconds();
		
		const CTaskGun* gunTask = static_cast<CTaskGun*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
		if (!(gunTask && gunTask->GetIsAiming()))
		{
			if (!pPlayerPed || !pPlayerPed->GetWeaponManager())
				return;

			PreviousHudValue.iWeaponHash = (pPlayerPed->GetWeaponManager()->GetSelectedWeaponHash());
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateWhenBustedOrWasted()
// PURPOSE:	fades in and out hud elements when we are busted or wasted
/////////////////////////////////////////////////////////////////////////////////////
bool CNewHud::UpdateWhenBustedOrWasted()
{
	// fade on (and off) Hud elements when we die or get busted:
	if (CHudTools::HasJustRestarted())
	{
		CHudTools::SetAsRestarted(false);

		// fade in current weapon icon and ammo

		// fade in area and street name
		return true;
	}

	if (!CGameLogic::IsGameStateInPlay())
	{
		// hide hud items when you die/busted (i think)

		return false;
	}

	return true;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::SetHudComponentPosition()
// PURPOSE: sets the hud component position
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::SetHudComponentPosition(s32 iId, Vector2 vPos)
{
	if (uiVerifyf(iId < TOTAL_HUD_COMPONENT_LIST_SIZE, "HUD: invalid hud component passed to SetHudComponentPosition (%d)", iId))
	{
		if (HudComponent[iId].vOverriddenPositionToUseNextSetActive != vPos)
		{
			if ( (!HudComponent[iId].bHasGfx) || (CNewHud::IsHudComponentActive(iId)) )
			{
				if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "OVERRIDE_COMPONENT_POSITION"))
				{
					CScaleformMgr::AddParamInt(iId);
					CScaleformMgr::AddParamFloat(vPos.x);
					CScaleformMgr::AddParamFloat(vPos.y);
					CScaleformMgr::EndMethod(!HudComponent[iId].bIsPositionOverridden);  // fix for 1880459
					HudComponent[iId].bIsPositionOverridden = true;
				}
			}

			HudComponent[iId].bWantToResetPositionOnNextSetActive = false;  // if we are setting the value, we no longer want to reset so clear the reset flag here

			// want to store this up so if it fades out and gets set back active later on, we still use this new override position
			HudComponent[iId].vOverriddenPositionToUseNextSetActive = vPos;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::SetHudComponentPosition()
// PURPOSE: sets the hud component position
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ResetHudComponentPositionFromOriginalXmlValues(s32 iId)
{
	if (uiVerifyf(iId < TOTAL_HUD_COMPONENT_LIST_SIZE, "HUD: invalid hud component passed to ResetHudComponentPositionFromOriginalXmlValues (%d)", iId))
	{
		if ( (!HudComponent[iId].bHasGfx) || (CNewHud::IsHudComponentActive(iId)) )
		{
			if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_COMPONENT_POSITION"))
			{
				CScaleformMgr::AddParamInt(iId);
				CScaleformMgr::AddParamFloat(HudComponent[iId].vSize.x);
				CScaleformMgr::AddParamFloat(HudComponent[iId].vSize.y);
				CScaleformMgr::AddParamString(HudComponent[iId].cAlignX);
				CScaleformMgr::AddParamString(HudComponent[iId].cAlignY);
				CScaleformMgr::AddParamFloat(HudComponent[iId].vPos.x);
				CScaleformMgr::AddParamFloat(HudComponent[iId].vPos.y);
				CScaleformMgr::EndMethod(HudComponent[iId].bIsPositionOverridden);  // fix for 1973301 - only instant if we are currently overriding
			}

			HudComponent[iId].bIsPositionOverridden = false;
			HudComponent[iId].vOverriddenPositionToUseNextSetActive = Vector2(__INVALID_REPOSITION_VALUE, __INVALID_REPOSITION_VALUE);
		}
		else
		{
			HudComponent[iId].bWantToResetPositionOnNextSetActive = true;
		}
	}
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::SetHudComponentColour()
// PURPOSE: sets the hud component colour
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::SetHudComponentColour(s32 iComponentId, eHUD_COLOURS iHudColor, s32 iAlpha)
{
	Color32 newColour;
	
	if (iAlpha != -1)
	{
		newColour = CHudColour::GetRGB(iHudColor, (u8)(floor((iAlpha / 255.0f) * 100.0f)));  // 0-100 alpha
	}
	else
	{
		newColour = CHudColour::GetRGBA(iHudColor);
		newColour.SetAlpha((s32)(newColour.GetAlphaf() * 100.0f));  // 0-100 alpha
	}

	if (HudComponent[iComponentId].colour != newColour)
	{
		if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_COMPONENT_COLOUR"))
		{
			CScaleformMgr::AddParamInt(iComponentId);
			CScaleformMgr::AddParamInt((s32)newColour.GetRed());
			CScaleformMgr::AddParamInt((s32)newColour.GetGreen());
			CScaleformMgr::AddParamInt((s32)newColour.GetBlue());
			CScaleformMgr::AddParamInt((s32)newColour.GetAlpha());  // already is 0-100
			CScaleformMgr::EndMethod();
		}

		HudComponent[iComponentId].colour = CHudColour::GetRGB(iHudColor, (u8)iAlpha);  // store the hud colour with 0-255 alpha
	}
};



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::SetHudComponentAlpha()
// PURPOSE: sets the hud component alpha
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::SetHudComponentAlpha(s32 iComponentId, s32 iAlpha)
{
	s32 iNewAlpha100 = (u8)(floor((iAlpha / 255.0f) * 100.0f));

	Color32 newColour = HudComponent[iComponentId].colour;
	newColour.SetAlpha(iNewAlpha100);  // 0-100 alpha

	if (HudComponent[iComponentId].colour != newColour)
	{
		if (CScaleformMgr::BeginMethod(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD, "SET_COMPONENT_COLOUR"))
		{
			CScaleformMgr::AddParamInt(iComponentId);
			CScaleformMgr::AddParamInt((s32)newColour.GetRed());
			CScaleformMgr::AddParamInt((s32)newColour.GetGreen());
			CScaleformMgr::AddParamInt((s32)newColour.GetBlue());
			CScaleformMgr::AddParamInt((s32)newColour.GetAlpha());  // already is 0-100
			CScaleformMgr::EndMethod();
		}

		HudComponent[iComponentId].colour.SetAlpha(iAlpha);  // 0-255 alpha
	}
};



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::RepopulateHealthInfoDisplay()
// PURPOSE: sets previous values as -1 so the hud will repopulate the minimap with
//			the health and armour regardless of current values
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::RepopulateHealthInfoDisplay()
{
	PreviousHudValue.healthInfo.Reset();
	PreviousHudValue.SpecialAbility.fPercentage = -1.0f;
	PreviousHudValue.m_fAirPercentage = 0.0f;
	PreviousHudValue.fAirPercentageWhileSubmerged = FULL_AIR_BAR;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ResetAllPreviousValues()
// PURPOSE: resets all previous hud values so game will consider them changed next
//			pass
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ResetAllPreviousValues()
{
	for (s32 iCount = BASE_HUD_COMPONENT; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		ResetPreviousValueOfHudComponent(iCount);
	}

	// generic values:
	PreviousHudValue.healthInfo.Reset();
	PreviousHudValue.bAbilityBarActive = false;
	PreviousHudValue.SpecialAbility.fPercentage = -1.0f;
	PreviousHudValue.SpecialAbility.fPercentageCapacity = -1.0f;
	PreviousHudValue.m_fAirPercentage = 0.0f;
	PreviousHudValue.fAirPercentageWhileSubmerged = FULL_AIR_BAR;
	PreviousHudValue.bMultiplayerStatus = !NetworkInterface::IsGameInProgress();

	// Only want to reset this between sessions, not upon a restart
	PreviousHudValue.iMultiplayerBankCash = -1;
	PreviousHudValue.iCash = INVALID_CASH_VALUE;
	PreviousHudValue.iMultiplayerWalletCash = -1;

	if(PreviousHudValue.bFlashHealthMeter)
	{
		if (CScaleformMgr::BeginMethod(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND), SF_BASE_CLASS_MINIMAP, "FLASH_HEALTH_BAR"))
		{
			CScaleformMgr::AddParamBool(false);
			CScaleformMgr::EndMethod();
		}
	}

	PreviousHudValue.bFlashHealthMeter = false;
	
#if __DEV
	sfDebugf1("SF HUD: Reset all previous values");
#endif // __DEV
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ResetPreviousValueOfHudComponent()
// PURPOSE: resets any previous values associated with this component
//			duplications are allowed and they must be in the right place otherwise
//			things may break when this is called via RestartHud()
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ResetPreviousValueOfHudComponent(s32 iHudComponent)
{
	switch (iHudComponent)
	{
		case NEW_HUD_WANTED_STARS:
		{
			PreviousHudValue.iWantedLevel = 0;
			PreviousHudValue.bCopsHaveLOS = false;			
//			PreviousHudValue.bEnemiesHaveLOS = false;
			PreviousHudValue.bFlashWantedStars = false;
			PreviousHudValue.bReasonsToFlashForever = false;
			PreviousHudValue.bFlashWholeWantedMovie = false;
			break;
		}

		case NEW_HUD_WEAPON_ICON:
		{
			PreviousHudValue.iWeaponHash = 0;
			PreviousHudValue.iAmmo = -1;
			PreviousHudValue.iClipAmmo = -1;
			PreviousHudValue.bAimingOrReloading = true;
			PreviousHudValue.fWeaponTimerPercentage = -1.0f;
			PreviousHudValue.bDroppingProjectile = false;
			break;
		}

		case NEW_HUD_CASH:
		{
			PreviousHudValue.bCashForcedOn = false;
			break;
		}

		case NEW_HUD_HELP_TEXT:
		case NEW_HUD_FLOATING_HELP_TEXT_1:
		case NEW_HUD_FLOATING_HELP_TEXT_2:
		{
			PreviousHudValue.vHelpMessagePosition[iHudComponent-NEW_HUD_HELP_TEXT] = -CHelpMessage::GetScreenPosition(iHudComponent-NEW_HUD_HELP_TEXT);
			PreviousHudValue.iDirectionOffScreen[iHudComponent-NEW_HUD_HELP_TEXT] = HELP_TEXT_ARROW_FORCE_RESET;
			break;
		}

		case NEW_HUD_CASH_CHANGE:
		{
			PreviousHudValue.iCash =  INVALID_CASH_VALUE;
			PreviousHudValue.iPlayerPedModelHash = 0;
			break;
		}

		case NEW_HUD_RETICLE:
		{
			m_reticule.GetPreviousValues().Reset();
			break;
		}

		case NEW_HUD_WEAPON_WHEEL:
		{
			ms_WeaponWheel.ClearPreviouslyActive();
			break;
		}

		case NEW_HUD_SAVING_GAME:
		{
			PreviousHudValue.bDisplayingSavingMessage = false;
			savegameDisplayf("CNewHud::ResetPreviousValueOfHudComponent - clearing PreviousHudValue.bDisplayingSavingMessage");
			break;
		}
	}

#if __DEV
	sfDebugf1("SF HUD: Reset previous values for hud component '%s' %d", HudComponent[iHudComponent].cFilename, iHudComponent);
#endif // __DEV
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::GetHudScaleMode()
// PURPOSE: Returns the scale mode based off the current aspect ratio
/////////////////////////////////////////////////////////////////////////////////////
GFxMovieView::ScaleModeType CNewHud::GetHudScaleMode(s32 iComponentId)
{
	if (!HudComponent[iComponentId].bHasGfx)  // any movies that do not have their own gfx must use the hud container movie
	{
		iComponentId = NEW_HUD;
	}

	return ( CScaleformMgr::GetRequiredScaleMode(HudComponent[iComponentId].vPos, HudComponent[iComponentId].vSize) );
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::OffsetHudComponentForCurrentResolution()
// PURPOSE: Adjusts the offsets of components aligned to the left and right edges
//           when the aspect ratio is between 5:4 and 16:9
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::OffsetHudComponentForCurrentResolution(s32 iComponentId)
{
	if (!CHudTools::IsAspectRatio_4_3_or_16_9() && GetHudScaleMode(iComponentId) == GFxMovieView::SM_NoBorder)
	{	
		const float ASPECT_RATIO_16_9 = 16.0f / 9.0f;

		const float fAspectRatio = CHudTools::GetAspectRatio();
		const float fMarginRatio = (1.0f - CHudTools::GetSafeZoneSize()) * 0.5f;
		const float fDifferenceInMarginSize = fMarginRatio * (ASPECT_RATIO_16_9 - fAspectRatio);
		const float fOffsetValue = (1.0f - (fAspectRatio/ASPECT_RATIO_16_9) - fDifferenceInMarginSize) * 0.5f;

		if (strnicmp(HudComponent[iComponentId].cAlignX, "L", 1) == 0)
		{
			HudComponent[iComponentId].vPos.x = HudComponent[iComponentId].fNoOffsetPosX + fOffsetValue;
			HudComponent[iComponentId].vRevisedPos.x = HudComponent[iComponentId].fNoOffsetRevisedPosX + fOffsetValue;
		}
		else if (strnicmp(HudComponent[iComponentId].cAlignX, "R", 1) == 0)
		{
			HudComponent[iComponentId].vPos.x = HudComponent[iComponentId].fNoOffsetPosX - fOffsetValue;
			HudComponent[iComponentId].vRevisedPos.x = HudComponent[iComponentId].fNoOffsetRevisedPosX - fOffsetValue;
		}
	}
}


bool CNewHud::ShouldWheelBlockCamera()
{
	if( CNewHud::IsHudComponentActive(NEW_HUD_WEAPON_WHEEL) 
		&& ms_WeaponWheel.IsBeingForcedOpen() )
		return true;

	return CNewHud::IsHudComponentActive(NEW_HUD_RADIO_STATIONS) && ms_RadioWheel.IsBeingForcedOpen();
}

bool CNewHud::IsWeaponWheelActive()
{	
	if (IsHudComponentActive(NEW_HUD_WEAPON_WHEEL))
	{
		return true;
	}

	// This catches the frame where we've just deactivated the weapon wheel but haven't changed the equipped weapon
	return ms_WeaponWheel.HaventChangedWeaponYet();
}

bool CNewHud::IsRadioWheelActive()
{	
	return IsHudComponentActive(NEW_HUD_RADIO_STATIONS);
}

#if KEYBOARD_MOUSE_SUPPORT
void CNewHud::PopulateWeaponsFromWheelMemory(atRangeArray<atHashWithStringNotFinal, MAX_WHEEL_SLOTS>& memoryList)
{
	for(int i = 0; i < MAX_WHEEL_SLOTS; ++i)
	{
		memoryList[i] = ms_WeaponWheel.GetWeaponHashFromMemory((u8)i);
	}
}
#endif //#if KEYBOARD_MOUSE_SUPPORT


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::CreateBankWidgets()
// PURPOSE: creates the hud bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::UpdateHudDisplayConfig()
{
	if(ms_Initialized)
	{
		UpdateDisplayConfig(HudComponent[NEW_HUD].iId, SF_BASE_CLASS_HUD);
	}
}

static u32 g_SafezoneRenderStartTime = 0;
//PURPOSE: Enable the hud safe zone rendering for a specified time period
//PARAMS: milliseconds - time safezone is rendered
void CNewHud::EnableSafeZoneRender(int milliseconds)
{
	if(milliseconds != 0)
	{
		g_SafezoneRenderStartTime = fwTimer::GetSystemTimeInMilliseconds() + milliseconds;
	}
	else
	{
		g_SafezoneRenderStartTime = 0;
	}
}

//PURPOSE: Render safezone
bool CNewHud::ShouldDrawSafeZone()
{
	bool bShouldDraw = false;

	if(g_SafezoneRenderStartTime != 0 && CPauseMenu::IsActive())
	{
		if(fwTimer::GetSystemTimeInMilliseconds() <= g_SafezoneRenderStartTime)
		{
			bShouldDraw = true;
	}
		else
		{
			g_SafezoneRenderStartTime = 0;
		}
	}

	return bShouldDraw;
}

void CNewHud::DrawSafeZone()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP();
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState(grcStateBlock::BS_CompositeAlpha);

	float x0, x1, y0, y1;

#if RSG_PC
	float fSafezoneSize = (1.0f - CHudTools::GetSafeZoneSize()) * 0.5f;
	float fOffset = 1.0f;
	const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();

#if SUPPORT_MULTI_MONITOR
	if(GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		fOffset = 0.3333f;
	}
#endif // SUPPORT_MULTI_MONITOR

	x0 = mon.getArea().GetLeft() + (fSafezoneSize * fOffset);
	x1 = mon.getArea().GetRight() - (fSafezoneSize * fOffset);
	y0 = mon.getArea().GetBottom() + fSafezoneSize;
	y1 = mon.getArea().GetTop() - fSafezoneSize;
#else
	CHudTools::GetMinSafeZone(x0, y0, x1, y1);
#endif // RSG_PC

	const int SMALL_OFFSET = 14;
	const int LARGE_OFFSET = 50;
	float fDifference = CHudTools::GetAspectRatio() / (16.0f/9.0f);
	const float fSmallOffsetWidth = (SMALL_OFFSET * fDifference) / GRCDEVICE.GetWidth();
	const float fLargeOffsetWidth = (LARGE_OFFSET * fDifference) / GRCDEVICE.GetWidth();
	const float fSmallOffsetHeight = (SMALL_OFFSET * fDifference) / GRCDEVICE.GetHeight();
	const float fLargeOffsetHeight = (LARGE_OFFSET * fDifference) / GRCDEVICE.GetHeight();

	// top left
	grcBindTexture(NULL);
	grcBegin(drawTriStrip, 6);
	grcColor4f(0.4f, 0.4f, 0.4f, 0.1f);
	grcVertex2f(x0+fSmallOffsetWidth	, y0+fLargeOffsetHeight);
	grcVertex2f(x0				, y0+fLargeOffsetHeight);
	grcVertex2f(x0+fSmallOffsetWidth	, y0+fSmallOffsetHeight);
	grcVertex2f(x0				, y0);
	grcVertex2f(x0+fLargeOffsetWidth, y0+fSmallOffsetHeight);
	grcVertex2f(x0+fLargeOffsetWidth, y0);
	grcEnd();

	// top right
	grcBindTexture(NULL);
	grcBegin(drawTriStrip, 6);
	grcColor4f(0.4f, 0.4f, 0.4f, 0.1f);
	grcVertex2f(x1-fSmallOffsetWidth	, y0+fLargeOffsetHeight);
	grcVertex2f(x1				, y0+fLargeOffsetHeight);
	grcVertex2f(x1-fSmallOffsetWidth	, y0+fSmallOffsetHeight);
	grcVertex2f(x1				, y0);
	grcVertex2f(x1-fLargeOffsetWidth	, y0+fSmallOffsetHeight);
	grcVertex2f(x1-fLargeOffsetWidth	, y0);
	grcEnd();

	// bottom right
	grcBindTexture(NULL);
	grcBegin(drawTriStrip, 6);
	grcColor4f(0.4f, 0.4f, 0.4f, 0.1f);
	grcVertex2f(x0+fSmallOffsetWidth	, y1-fLargeOffsetHeight);
	grcVertex2f(x0				, y1-fLargeOffsetHeight);
	grcVertex2f(x0+fSmallOffsetWidth	, y1-fSmallOffsetHeight);
	grcVertex2f(x0				, y1);
	grcVertex2f(x0+fLargeOffsetWidth	, y1-fSmallOffsetHeight);
	grcVertex2f(x0+fLargeOffsetWidth	, y1);
	grcEnd();

	// bottom left
	grcBindTexture(NULL);
	grcBegin(drawTriStrip, 6);
	grcColor4f(0.4f, 0.4f, 0.4f, 0.1f);
	grcVertex2f(x1-fSmallOffsetWidth	, y1-fLargeOffsetHeight);
	grcVertex2f(x1				, y1-fLargeOffsetHeight);
	grcVertex2f(x1-fSmallOffsetWidth	, y1-fSmallOffsetHeight);
	grcVertex2f(x1				, y1);
	grcVertex2f(x1-fLargeOffsetWidth	, y1-fSmallOffsetHeight);
	grcVertex2f(x1-fLargeOffsetWidth	, y1);
	grcEnd();

	// border
	grcBindTexture(NULL);
	grcBegin(drawTriStrip, 10);
	grcColor4f(0.0f, 0.0f, 0.0f, 0.3f);
	grcVertex2f(0.0f	, 0.0f);
	grcVertex2f(x0		, y0);
	grcVertex2f(1.0f	, 0.0f);
	grcVertex2f(x1		, y0);
	grcVertex2f(1.0f	, 1.0f);
	grcVertex2f(x1		, y1);
	grcVertex2f(0.0f	, 1.0f);
	grcVertex2f(x0		, y1);
	grcVertex2f(0.0f	, 0.0f);
	grcVertex2f(x0		, y0);
	grcEnd();

	// Reset stateblocks after drawing 
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
}

#if SUPPORT_MULTI_MONITOR
void CNewHud::SetDrawBlinder(int iDrawBlindersFrames)
{
	bool bInCutscene = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());
	if(ms_iDrawBlinders <= 0 && !bInCutscene)
	{
		if (GRCDEVICE.GetMonitorConfig().isMultihead())
		{
			const fwRect area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
			CSprite2d::SetMultiFadeParams(area, true, 0, 1.0f, 0.0f, 0.5f);
		}
		else
		{
			const float SIXTEEN_BY_NINE = 16.0f/9.0f;
			float fAspect = CHudTools::GetAspectRatio();
			if(CHudTools::IsSuperWideScreen())
			{
				const float fEpsilon = 0.001f;
				float fDifference = ((SIXTEEN_BY_NINE / fAspect) * 0.5f) - fEpsilon;
				fwRect area(0.5f - fDifference, 0, 0.5f + fDifference, 1.0f);
				CSprite2d::SetMultiFadeParams(area, true, 0, 1.0f, 0.0f, 0.5f);
			}
			else
			{
				ms_iDrawBlinders = 0;
				return;
			}
		}
	}

	if(!bInCutscene)
		ms_iDrawBlinders = iDrawBlindersFrames;
}

bool CNewHud::ShouldDrawBlinders()
{
	bool bInCutscene = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());
	return (ms_iDrawBlinders > 0 || CWarningScreen::IsActive()) && !bInCutscene;
}
#endif // SUPPORT_MULTI_MONITOR


/////////////////////////////////////////////////////////////////////////////////////
// __BANK - all debug stuff shown appear under here
/////////////////////////////////////////////////////////////////////////////////////
#if __BANK
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::InitWidgets()
// PURPOSE: inits the ui bank widget and "Create" button
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (!pBank)  // create the bank if not found
	{
		pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
	}

	if (pBank)
	{
		pBank->AddButton("Create HUD widgets", &CNewHud::CreateBankWidgets);
	}
}

char s_TempTextBlock[32];
char s_TempHelpText[32];
char s_TempSubtitleText[32];

void LoadTextBlock()
{
	TheText.LoadAdditionalText(s_TempTextBlock, MISSION_TEXT_SLOT);
}

void PostHelpText()
{
	char *pMainString = TheText.Get(s_TempHelpText);
	s32 MainTextBlock = TheText.GetBlockContainingLastReturnedString();

	CHelpMessage::SetMessageText(0, pMainString);

	CSubtitleMessage PreviousBrief;
	PreviousBrief.Set(pMainString, MainTextBlock, 
		NULL, 0, 
		NULL, 0, 
		false, true);
	CMessages::AddToPreviousBriefArray(PreviousBrief, CScriptHud::GetNextMessagePreviousBriefOverride());
}

void PostSubtitleText()
{
	char *pMainString = TheText.Get(s_TempSubtitleText);
	s32 MainTextBlock = TheText.GetBlockContainingLastReturnedString();

	CMessages::AddMessage(pMainString, MainTextBlock, 
		5000, true, CScriptHud::GetAddNextMessageToPreviousBriefs(), CScriptHud::GetNextMessagePreviousBriefOverride(), 
		NULL, 0, 
		NULL, 0, 
		false);
}

void CNewHud::CreateBankWidgets()
{
	static bool bBankCreated = false;

	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if ((!bBankCreated) && (bank))
	{
		bank->PushGroup("HUD");
		m_reticule.CreateBankWidgets(bank);

		bank->AddToggle("Force Show Some HUD", &CNewHud::ms_bShowEverything);
		bank->AddToggle("Log player health/armor/endurance", &CNewHud::ms_bLogPlayerHealth);
		bank->AddToggle("Debug Help Text", &CNewHud::ms_bDebugHelpText);
		bank->AddToggle("Return Widescreen To Script", &CNewHud::bForceScriptWidescreen);
		bank->AddToggle("Return HD To Script", &CNewHud::bForceScriptHD);
		bank->AddSeparator();
		bank->AddButton("Unload NEW_HUD movie (for Preview)", &CNewHud::DebugUnloadMovie);
		bank->AddButton("Reload NEW_HUD movie (for Preview)", &CNewHud::DebugLoadMovie);
		bank->AddSeparator();
		bank->AddToggle("Show debug output", &CNewHud::bDisplayDebugOutput, &CNewHud::RetriggerDebugOutput);
		bank->AddToggle("Debug script gfx draw order", &CNewHud::bDebugScriptGfxDrawOrder);
		bank->AddToggle("Display HUD (Scripted)", &CScriptHud::bDisplayHud);
		bank->AddToggle("Display Script Scaleform Movies", &CScriptHud::bDisplayScriptScaleformMovieDebug);
		bank->AddToggle("Display Script Hidden Hud Components", &s_bPrintScriptHiddenHud);
		bank->AddButton("Reload XML", datCallback(CFA1(CNewHud::LoadXmlData), CallbackData(true)));
		bank->AddButton("Reload HUD Colours", datCallback(CFA(CHudColour::SetValuesFromDataFile)));

		bank->AddSeparator();

		bank->AddSlider("MAX_HEALTH_DISPLAY", &CScriptHud::iHudMaxHealthDisplay, 0, 500, 1);
		bank->AddSlider("Actual Health value", &iDebugHudHealthValue, 0, 500, 1);
		bank->AddSlider("Max Health value", &iDebugHudMaxHealthValue, 0, 500, 1);
		bank->AddSlider("MAX_ARMOUR_DISPLAY", &CScriptHud::iHudMaxArmourDisplay, 0, 500, 1);
		bank->AddSlider("Actual Armour value", &iDebugHudArmourValue, 0, 500, 1);
		bank->AddSlider("Max Armour value", &iDebugHudMaxArmourValue, 0, 500, 1);
		bank->AddSlider("MAX_ENDURANCE_DISPLAY", &CScriptHud::iHudMaxEnduranceDisplay, 0, 500, 1);
		bank->AddSlider("Actual Endurance value", &iDebugHudEnduranceValue, 0, 500, 1);
		bank->AddSlider("Max Endurance value", &iDebugHudMaxEnduranceValue, 0, 500, 1);

		bank->AddSeparator();

		bank->AddToggle("Suppress MP Cash Updates", &CNewHud::m_bSuppressCashUpdates);
		bank->AddSlider("MP Wallet Cash", &iDebugMultiplayerWalletCash, -5000, 5000, 1);
		bank->AddSlider("MP Bank Cash", &iDebugMultiplayerBankCash, -5000, 5000, 1);
		bank->AddButton("Begin Using MP Cash Override", &CNewHud::dbgBeginOverride);
		bank->AddButton("End Using MP Cash Override", &CNewHud::dbgEndOverride);
		bank->AddSlider("Change Fake MP Wallet Cash By", &iDebugMPWalletCashOverride, -5000, 5000, 1);
		bank->AddSlider("Change Fake MP Bank Cash By", &iDebugMPBankCashOverride, -5000, 5000, 1);
		bank->AddButton("Apply Override", &CNewHud::dbgApplyOverride);

// 		bank->AddToggle("Turn On MP Wallet Cash", &CScriptHud::ms_bTurnOnMultiplayerWalletCashThisFrame);
// 		bank->AddToggle("Turn On MP Bank Cash", &CScriptHud::ms_bTurnOnMultiplayerBankCashThisFrame);
// 		bank->AddToggle("Turn Off MP Wallet Cash", &CScriptHud::ms_bTurnOffMultiplayerWalletCashThisFrame);
// 		bank->AddToggle("Turn Off MP Bank Cash", &CScriptHud::ms_bTurnOffMultiplayerBankCashThisFrame);
		
		bank->AddSeparator();

		bank->AddSlider("Override Id (ComponentId)", &ms_iHudComponentId, 0, TOTAL_HUD_COMPONENT_LIST_SIZE, 1);
		bank->AddButton("Override Component", &CNewHud::DebugOverrideComponent);
		bank->AddButton("Reset Component", &CNewHud::DebugResetComponent);

		bank->AddSeparator();

		bank->AddSlider("DEBUG SCALEFORM METHOD (ComponentId)", &ms_iHudComponentId, 0, TOTAL_HUD_COMPONENT_LIST_SIZE, 1);
		bank->AddText("Call Method With Params", ms_cDebugMethod, sizeof(ms_cDebugMethod), false, &CallDebugMethod);

		bank->AddSeparator();

		bank->AddSlider("Show HUD visiblity (ComponentId)", &iDebugComponentVisibilityDisplay, 0, TOTAL_HUD_COMPONENT_LIST_SIZE, 1);
		bank->AddToggle("Toggle Force Show Area/Street/Vehicle Names", &CNewHud::ms_bForceAreaDistrictStreetVehicle, &CNewHud::ToggleForceShowDistrictVehicleArea);

		bank->AddText("Load text block", s_TempTextBlock, sizeof(s_TempTextBlock), datCallback(LoadTextBlock));
		bank->AddText("Post help message", s_TempHelpText, sizeof(s_TempHelpText), datCallback(PostHelpText));
		bank->AddText("Post subtitle", s_TempSubtitleText, sizeof(s_TempSubtitleText), datCallback(PostSubtitleText));

#if __DEV
		bank->AddToggle("Ignore Widescreen Adjustment", &CHudTools::bForceWidescreenStretch);
#endif

		ms_RadioWheel.CreateBankWidgets(bank);
		ms_WeaponWheel.CreateBankWidgets(bank);
		
		for (s32 i = 0; i < TOTAL_HUD_COMPONENT_LIST_SIZE; i++)  // add each component to the bank
		{
			if (i >= MAX_NEW_HUD_COMPONENTS && i < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
				continue;

			if (HudComponent[i].cFilename[0] != '\0')  // only add components to widgets if it has a valid value (based on filename)
			{
				AddComponentToWidget(i);
			}
		}

		bank->PopGroup();

		bBankCreated = true;
	}
}

void CNewHud::dbgApplyOverride()
{
	CNewHud::ChangeFakeMPCash(iDebugMPWalletCashOverride, iDebugMPBankCashOverride);
}

void CNewHud::dbgBeginOverride()
{
	CNewHud::UseFakeMPCash(true);
}

void CNewHud::dbgEndOverride()
{
	CNewHud::UseFakeMPCash(false);
}

void CNewHud::ToggleForceShowDistrictVehicleArea()
{
	const char *pAreaName = CUserDisplay::AreaName.GetName();
	const char *pStreetName = CUserDisplay::StreetName.GetName();

	// HUD Area Name
	ForceShowHudComponent(NEW_HUD_AREA_NAME, ms_bForceAreaDistrictStreetVehicle);

	if (pAreaName)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_AREA_NAME, "SET_NAME"))
		{
			CScaleformMgr::AddParamString(pAreaName);

			if (CScriptHud::ms_iOverrideTimeForAreaVehicleNames != -1)
			{
				CScaleformMgr::AddParamInt(CScriptHud::ms_iOverrideTimeForAreaVehicleNames);
			}

			CScaleformMgr::EndMethod();
		}
	}

	// HUD Street Name
	ForceShowHudComponent(NEW_HUD_STREET_NAME, ms_bForceAreaDistrictStreetVehicle);

	if (pStreetName)
	{
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_STREET_NAME, "SET_NAME"))
		{
			CScaleformMgr::AddParamString(pStreetName);

			if (CScriptHud::ms_iOverrideTimeForAreaVehicleNames != -1)
			{
				CScaleformMgr::AddParamInt(CScriptHud::ms_iOverrideTimeForAreaVehicleNames);
			}

			CScaleformMgr::EndMethod();
		}
	}

	//HUD Vehicle Name
	ForceShowHudComponent(NEW_HUD_VEHICLE_NAME, ms_bForceAreaDistrictStreetVehicle);
	ActuallyUpdateVehicleName();
}

void CNewHud::ForceShowHudComponent(eNewHudComponents eHudComponentId, const bool bForceShow)
{
	if (CHudTools::BeginHudScaleformMethod(eHudComponentId, "FORCE_SHOW"))
	{
		CScaleformMgr::AddParamBool(bForceShow);
		CScaleformMgr::EndMethod();
	}
}

void CNewHud::DebugLoadMovie()
{
	Init(INIT_CORE);
	Init(INIT_SESSION);
}

void CNewHud::DebugUnloadMovie()
{
	s_iQueuedShutdown = 1;
}

void CNewHud::DebugOverrideComponent()
{
	SetHudComponentPosition((eNewHudComponents)ms_iHudComponentId, Vector2(0.0f, -0.04f));
}

void CNewHud::DebugResetComponent()
{
	ResetHudComponentPositionFromOriginalXmlValues((eNewHudComponents)ms_iHudComponentId);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ShutdownWidgets()
// PURPOSE: removes the hud bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ShutdownWidgets()
{
	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (bank)
	{
		bank->Destroy();
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::CallDebugMethod()
// PURPOSE: calls a debug method on the movie
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::CallDebugMethod()
{
	if (GetContainerMovieId() != -1)
	{
		char cMethodName[256];
		sscanf(ms_cDebugMethod, "%s", &cMethodName[0]);
		if (CHudTools::BeginHudScaleformMethod(static_cast<eNewHudComponents>(ms_iHudComponentId), cMethodName))
		{
			CScaleformMgr::PassDebugMethodParams(ms_cDebugMethod);
			CScaleformMgr::EndMethod();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::AddComponentToWidget()
// PURPOSE: add one component to a widget
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::AddComponentToWidget(s32 iIndex)
{
	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);  // get the bank

	bank->PushGroup(strupr(HudComponent[iIndex].cFilename));
		bank->AddSlider("Position", &HudComponent[iIndex].vPos, -1.0f, 2.0f, 0.001f, CNewHud::ReadjustAllComponents);
		bank->AddSlider("Size", &HudComponent[iIndex].vSize, 0.0f, 2.0f, 0.001f, CNewHud::ReadjustAllComponents);
	bank->PopGroup();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::ReadjustAllComponents()
// PURPOSE: sets all the params again for each movie so they are updated from the new
//			data provided by the bank widgets
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::ReadjustAllComponents()
{
	for (s32 iCount = 0; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		GFxMovieView::ScaleModeType eScaleMode = GetHudScaleMode(iCount);

		CScaleformMgr::ChangeMovieParams(HudComponent[iCount].iId, HudComponent[iCount].vPos, HudComponent[iCount].vSize, eScaleMode);
	}
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::RetriggerDebugOutput()
// PURPOSE: resets all the previous logs for this component so we force the output
//			to be displayed on the log again
/////////////////////////////////////////////////////////////////////////////////////
void CNewHud::RetriggerDebugOutput()
{
#if __DEV
	for (s32 iCount = 0; iCount < TOTAL_HUD_COMPONENT_LIST_SIZE; iCount++)
	{
		if (iCount >= MAX_NEW_HUD_COMPONENTS && iCount < SCRIPT_HUD_COMPONENTS_START)  // skip the stuff we dont actually support if we are looping through the whole array
			continue;

		HudComponent[iCount].cDebugLog[0] = '\0';
	}
#endif
}

#endif // __BANK



void CNewHud::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	if (methodName == ATSTRINGHASH("STREAM_ITEM_FAILED",0x1a888d79))
	{
		if (uiVerifyf(args[1].IsNumber(), "STREAM_ITEM_FAILED params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			CGameStream* GameStream = CGameStreamMgr::GetGameStream();
			if( GameStream != NULL )
			{
				GameStream->StreamItemFailed( (s32)args[1].GetNumber() );
			}
		}
		return;
	}

	// public function CLEAR_HELP_TEXTfilename:String):Void
	if (methodName == ATSTRINGHASH("CLEAR_HELP_TEXT",0xc01c7d4f))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber(), "CLEAR_HELP_TEXT params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			// filename is the 1st param
			s32 iId = (s32)args[1].GetNumber();
			u32 iUniqueToken = (u32)args[2].GetNumber();

			if (GetHelpTextToken(iId) == iUniqueToken)
			{
				sfDebugf1("Actionscript has called CLEAR_HELP_TEXT with id %d, token %d", iId, iUniqueToken);

				CNewHud::SetHelpTextWaitingForActionScriptResponse(iId, false);

				CHelpMessage::ClearMessage(iId);

				PreviousHudValue.vHelpMessagePosition[iId] = -CHelpMessage::GetScreenPosition(iId);  // always different
				PreviousHudValue.iDirectionOffScreen[iId] = HELP_TEXT_ARROW_FORCE_RESET;

				RemoveHudComponent(NEW_HUD_HELP_TEXT+iId);
			}
			else
			{
				sfDebugf1("CLEAR_HELP_TEXT: Code Token = %d,   Actionscript Token = %d", GetHelpTextToken(iId), iUniqueToken);
			}
		}

		return;
	}

	// public function SET_ACTIVE_STATE
	if (methodName == ATSTRINGHASH("SET_ACTIVE_STATE",0x4f8cee56))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsBool(), "SET_ACTIVE_STATE params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			s32 iComponentId = (s32)args[1].GetNumber();
			bool bToBeActive = args[2].GetBool();

			// request that the movie is removed:
#if __DEV
			if (bToBeActive)
				sfDebugf1("SF HUD: ActionScript wants to to set %s active", HudComponent[iComponentId].cFilename);
			else
				sfDebugf1("SF HUD: ActionScript wants to to set %s inactive", HudComponent[iComponentId].cFilename);
#endif
			HudComponent[iComponentId].bActive = bToBeActive;

			if (bToBeActive)
			{
				if (HudComponent[iComponentId].bWantToResetPositionOnNextSetActive)  // if we want to reset the position but it wasnt active when the command was called, do it now on re-activate
				{
					ResetHudComponentPositionFromOriginalXmlValues(iComponentId);
					HudComponent[iComponentId].bWantToResetPositionOnNextSetActive = false;
				}

				if (HudComponent[iComponentId].vOverriddenPositionToUseNextSetActive.x != __INVALID_REPOSITION_VALUE && HudComponent[iComponentId].vOverriddenPositionToUseNextSetActive.y != __INVALID_REPOSITION_VALUE)  // if we want to reset the position but it wasnt active when the command was called, do it now on re-activate
				{
					SetHudComponentPosition(iComponentId, HudComponent[iComponentId].vOverriddenPositionToUseNextSetActive);
				}
			}

			if (HudComponent[iComponentId].iLoadingState == WAITING_ON_SETUP_BY_ACTIONSCRIPT)
			{
				HudComponent[iComponentId].iLoadingState = NOT_LOADING;
			}

			sfDisplayf("Component: %s Active: %i, bCodeThinksVisible: %i", HudComponent[iComponentId].cFilename, HudComponent[iComponentId].bActive, HudComponent[iComponentId].bCodeThinksVisible);
		}

		return;
	}

	// public function SET_COMPONENT_LOADED
	if (methodName == ATSTRINGHASH("SET_COMPONENT_LOADED",0xc3da1ea4))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsBool(), "SET_COMPONENT_LOADED params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			s32 iComponentId = (s32)args[1].GetNumber();
			bool bLoaded = args[2].GetBool();

			s32 iNewComponentMovieId = CScaleformMgr::FindMovieByFilename(HudComponent[iComponentId].cFilename);

			if (iNewComponentMovieId != -1)
			{
				// request that the movie is removed:
#if __DEV
				if (bLoaded)
					sfDebugf1("SF HUD: ActionScript has set %s as loaded", HudComponent[iComponentId].cFilename);
				else
					sfDebugf1("SF HUD: ActionScript has set %s as unloaded", HudComponent[iComponentId].cFilename);
#endif

				if (CScaleformMgr::IsMovieActive(iNewComponentMovieId))
				{
					HudComponent[iComponentId].bInUseByActionScript = bLoaded;
				}
			}
		}

		return;
	}

	// public function SET_LOWEST_TOP_RIGHT_Y
	if (methodName == ATSTRINGHASH("SET_LOWEST_TOP_RIGHT_Y",0xd954fb86))
	{
		if (uiVerifyf(args[1].IsNumber(), "SET_LOWEST_TOP_RIGHT_Y params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			float fPositionY = (float)args[1].GetNumber();

			ms_fLowestTopRightY = fPositionY / GRCDEVICE.GetHeight();
		}

		return;
	}

	// public function SET_HUD_COMPONENT_AS_NO_LONGER_NEEDED filename:String):Void
	if (methodName == ATSTRINGHASH("SET_HUD_COMPONENT_AS_NO_LONGER_NEEDED",0xc36cc66b))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsBool(), "SET_HUD_COMPONENT_AS_NO_LONGER_NEEDED params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			s32 iComponentId = (s32)args[1].GetNumber();
			bool bRequestWasTriggeredByCode = args[2].GetBool();

			if (CNewHud::IsHudComponentActive(iComponentId) && ( (!bRequestWasTriggeredByCode) || (bRequestWasTriggeredByCode && HudComponent[iComponentId].iLoadingState == WAITING_ON_REMOVAL_BY_ACTIONSCRIPT) ) )
			{
				// request that the movie is removed:
	#if __DEV
				sfDebugf1("SF HUD: ActionScript has cleared its references to movie %s id:%d.", HudComponent[iComponentId].cFilename, HudComponent[iComponentId].iId);
	#endif
				HudComponent[iComponentId].bInUseByActionScript = false;
				HudComponent[iComponentId].iLoadingState = REQUEST_REMOVAL_OF_MOVIE;  // we can now remove the hud component

				CScaleformMgr::ForceCollectGarbage(HudComponent[NEW_HUD].iId); 
			}
		}

		return;
	}

	// public function REQUEST_GFX_STREAM(uid:Number, filename:String):Void
	if (methodName == ATSTRINGHASH("REQUEST_GFX_STREAM",0x9b2dc57c) ||  // to be removed and replaced with...
		methodName== ATSTRINGHASH("ACTIVATE_COMPONENT",0x3897042) )    // ... this (once Gareths stuff has sync'd)
	{
		if (methodName == ATSTRINGHASH("REQUEST_GFX_STREAM",0x9b2dc57c))
		{
			uiAssertf(0, "REQUEST_GFX_STREAM called by ActionScript - please replace with ACTIVATE_COMPONENT");
		}

		if (uiVerifyf(args[1].IsNumber(), "ACTIVATE_COMPONENT params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			// we have a request for a stream.
			s32 iComponentId = (s32)args[1].GetNumber();

	#if __DEV
			sfDebugf1("SF HUD: ActionScript wants to load in movie '%s'", HudComponent[iComponentId].cFilename);
	#endif

			//sfAssertf(HudComponent[iComponentId].iLoadingState == NOT_LOADING, "ACTIVATE_COMPONENT: hud component %s %d in a handshake state %d) when asked to be active", HudComponent[iComponentId].cFilename, iComponentId, HudComponent[iComponentId].iLoadingState);

			// request that the movie is streamed in:
			if (HudComponent[iComponentId].iLoadingState == NOT_LOADING || HudComponent[iComponentId].iLoadingState == WAITING_ON_REMOVAL_BY_ACTIONSCRIPT)
			{
				// found the requested filename in the hud - stream it in
				HudComponent[iComponentId].iLoadingState = LOADING_REQUEST_TO_CREATE_MOVIE;
			}
		}
		return;
	}

	// public function SET_REVISED_COMPONENT_VALUES
	if (methodName == ATSTRINGHASH("SET_REVISED_COMPONENT_VALUES",0xd258d8c7))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber() && args[3].IsNumber() && args[4].IsNumber() && args[5].IsNumber()
			, "SET_REVISED_COMPONENT_VALUES params not compatible: %s %s %s %s %s"
			, sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])
			, sfScaleformManager::GetTypeName(args[3]), sfScaleformManager::GetTypeName(args[4])
			, sfScaleformManager::GetTypeName(args[5])))
		{
			// we have a request for a stream.
			s32 iComponentId = (s32)args[1].GetNumber();

			HudComponent[iComponentId].vRevisedPos = Vector2((float)args[2].GetNumber(), (float)args[3].GetNumber());
			HudComponent[iComponentId].vRevisedSize = Vector2((float)args[4].GetNumber(), (float)args[5].GetNumber());

			HudComponent[iComponentId].fNoOffsetRevisedPosX = HudComponent[iComponentId].vRevisedPos.x;
		}

		return;
	}

#if RSG_PC
	if (methodName == ATSTRINGHASH("SET_LANDING_SCREEN_CHARACTER_COLOUR",0xafff6d5d))
	{
		if (uiVerifyf(CLoadingScreens::IsDisplayingLandingScreen(), "Trying to override character colour when not on landing page!  This is not allowed") && 
			uiVerifyf(args[1].IsNumber(), "SET_LANDING_SCREEN_CHARACTER_COLOUR params not compatible: %s"))
		{
			CNewHud::SetCurrentCharacterColour(static_cast<eHUD_COLOURS>((int)args[1].GetNumber()));
		}
	}
#endif

	// TOSS TO ANY CHILD COMPONENTS
	if( ms_WeaponWheel.CheckIncomingFunctions(methodName, args) )
		return;
}

// eof

