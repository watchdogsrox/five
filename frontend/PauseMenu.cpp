/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PauseMenu.cpp
// PURPOSE : code to control the Scaleform pause menu in game
// AUTHOR  : Derek Payne
// STARTED : 03/02/2011
//
/////////////////////////////////////////////////////////////////////////////////

//#define ENABLE_TRAPS_IN_THIS_FILE

#if RSG_PC
#include <dxgi.h>
#endif // RSG_PC

#if __PPU
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_sysparam.h>
#endif
 
// rage headers:
#include "grcore/allocscope.h"
#if __BANK
#include "bank/bkmgr.h"
#include "game/localisation.h"
#endif // __BANK

#include "Scaleform/tweenstar.h"
#include "system/service.h"

// Framework headers
#include "scaleform/channel.h"
#include "audiohardware/driver.h"
#include "fwrenderer/renderthread.h"
#include "fwsys/gameskeleton.h"
#include "parser/manager.h"
#include "parser/tree.h"
#include "parsercore/settings.h"
#include "parsercore/utils.h"
#include "system/appcontent.h"
#include "system/platform.h"

// game headers:
#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/radioaudioentity.h"
#include "audio/radiostation.h"
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "control/gamelogic.h"
#include "control/gps.h"
#include "core/game.h"
#include "Cutscene/CutsceneManagerNew.h"
#include "frontend/Credits.h"
#include "frontend/DisplayCalibration.h"
#include "frontend/GameStream.h"
#include "frontend/GameStreamMgr.h"
#include "Frontend/PauseMenu.h"
#include "frontend/PauseMenu_parser.h"
#include "frontend/LanguageSelect.h"
#include "Frontend/loadingscreens.h"

#include "Peds/pedplacement.h"

#if RSG_PC
#include "frontend/MultiplayerChat.h"
#include "frontend/PCGamepadCalibration.h"
#include "frontend/MousePointer.h"
#endif // RSG_PC

#include "Frontend/CMapMenu.h"
#include "Frontend/MiniMap.h"
#include "Frontend/MiniMapCommon.h"
#include "Frontend/MiniMapRenderThread.h"
#include "Frontend/NewHud.h"
#include "Frontend/HudMarkerManager.h"
#include "Frontend/HudTools.h"
#include "Frontend/GameStreamMgr.h"
#include "Frontend/BusySpinner.h"
#include "Frontend/SocialClubMenu.h"
#include "Frontend/StatsMenu.h"
#include "Frontend/ControllerLabelMgr.h"
#include "Frontend/InfoMenu.h"
#include "Frontend/ScaleformMenuHelper.h"
#include "Frontend/Scaleform/ScaleformMgr.h"
#include "Frontend/Store/PauseStoreMenu.h"
#include "Frontend/Store/StoreScreenMgr.h"
#include "Frontend/FrontendStatsMgr.h"
#include "frontend/KeyMappingMenu.h"
#include "Frontend/ReportMenu.h"
#include "Frontend/WarningScreen.h"
#include "Frontend/UIMenuPed.h"
#include "Frontend/ui_channel.h"
#if GTA_REPLAY
#include "control/replay/replaycontrol.h"
#include "Network/Cloud/VideoUploadManager.h"
#include "frontend/VideoEditor/ui/Menu.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/VideoEditor/ui/Playback.h"
#include "rline/rlsystemui.h"
#endif // GTA_REPLAY
#include "Game/Clock.h"
#include "Game/Wanted.h"
#include "Game/User.h"
#include "Game/ModelIndices.h"
#include "Network/Network.h"
#include "Network/Commerce/CommerceManager.h"
#include "Network/events/NetworkEventTypes.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "Network/NetworkInterface.h"
#include "Network/Sessions/NetworkSession.h"
#if RSG_PC
#include "network/Voice/NetworkVoice.h"
#endif // RSG_PC
#if RSG_PC
#include "frontend/landing_page/LegacyLandingScreen.h"
#include "frontend/TextInputBox.h"
#endif // RSG_PC
#include "Peds/Ped.h"
#include "Peds/Rendering/PedHeadshotManager.h"
#include "Profile/timebars.h"
#include "renderer/PostProcessFX.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/RenderPhases/RenderPhaseDebugNY.h"
#include "renderer/RenderPhases/RenderPhaseStd.h"
#include "renderer/RenderPhases/RenderPhaseStdNY.h"
#include "renderer/RenderThread.h"
#include "renderer/Water.h"
#include "saveload/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_frontend.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "saveload/savegame_queue.h"
#include "scene/world/GameWorld.h"
#include "scene/streamer/SceneStreamer.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "script/script_hud.h"
#include "Stats/StatsMgr.h"
#include "stats/StatsInterface.h"
#include "stats/MoneyInterface.h"
#include "streaming/defragmentation.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingvisualize.h"
#include "Streaming/streamingrequestlist.h"
#include "System/controlMgr.h"
#include "System/pad.h"
#include "system/service.h"
#include "system/SettingsDefaults.h"
#include "text/messages.h"
#include "text/textfile.h"
#include "Vfx/Misc/MovieManager.h"
#include "event/EventNetwork.h"
#include "event/EventGroup.h"

#include "Frontend/CCrewMenu.h"
#include "Frontend/CFriendsMenu.h"
#include "Frontend/CMapMenu.h"
#include "Frontend/CScriptMenu.h"
#include "Frontend/CSettingsMenu.h"
#include "Frontend/GalleryMenu.h"
#include "Frontend/ButtonEnum.h"
#include "Frontend/MultiplayerGamerTagHud.h"
#include "Frontend/UIContexts.h"
#include "frontend/VideoEditorPauseMenu.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"

#include "audiohardware/driver.h"
#include "audiohardware/device_orbis.h"

#if GEN9_LANDING_PAGE_ENABLED
#include "frontend/landing_page/LandingPage.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#endif

#if __DEV
#include "text/text.h" // for TextLayout's debug draw
#endif

#if __ASSERT
#include "atl/map.h"

extern const char*				parser_eMenuAction_Strings[];
extern rage::parEnumListEntry	parser_eMenuPref_Values[];
extern rage::parEnumData		parser_eInstructionButtons_Data;
#endif

#if RSG_PC
#include "avchat/voicechat.h"
#include "avchat/voice/rv.h"
#include "rline/rlpc.h"
#include "string/stringhash.h"
#define MAX_VOICE_DEVICE_LENGTH 128
#define MAX_VOICE_DEVICE_FRONTEND_LENGTH 32
#define EXIT_TO_WINDOWS_DELAY 100
#define DX_VERSION_11_INDEX 2
#endif // RSG_PC

#if RSG_PC
#include "grcore/adapter_d3d11.h"
#endif // RSG_PC

#if __PPU
#include <sysutil/sysutil_sysparam.h>
#endif

#if RSG_DURANGO
#include "Network/Live/Events_durango.h"
#endif

#if RSG_ORBIS
#include "rline/rlnp.h"
#include <system_service.h>
#pragma comment(lib,"SceSystemService_stub_weak")
#endif

#if RSG_PC
#define DEFAULT_TARGETING_MODE CPedTargetEvaluator::TARGETING_OPTION_FREEAIM
#else
#define DEFAULT_TARGETING_MODE CPedTargetEvaluator::TARGETING_OPTION_ASSISTED_AIM 
#endif


#include <time.h>

#include "data/aes_init.h"
AES_INIT_3;

FRONTEND_MENU_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

// not really ready yet
#define OUTRO_EFFECT 0
#define FANCY_TIME_WARP_TECH 0

#if	!__FINAL
PARAM(autoaddPausemenuWidgets, "Automatically create the pausemenu widgets on startup");
#endif
#if __BANK
PARAM(alwaysReloadPauseMenuXML, "Always Reload the Pause Menu XML when opening the menu");
#if __PRELOAD_PAUSE_MENU
PARAM(noResidentPauseMenu, "Pause menu will not be kept around after unloading");
#endif
#endif // __BANK

#if !RSG_FINAL
PARAM(overrideCreditsDisplay, "Override the credits menu item being displayed (-1 : none, 0 : credits / legal, 1 : credits, 2 : legal");
#endif

NOSTRIP_PARAM(uilanguage,"Set language game uses");
#if RSG_PC
PARAM(ignoreGfxLimit, "Allows graphics/video settings to be applied even if they go over the card memory limits");
NOSTRIP_PC_PARAM(streamingbuild, "Run as if the game was streaming via big picture or NVidia Shield");
#elif RSG_ORBIS
#if __BANK
	PARAM(enableJPNButtonSwap, "Enable button swap when language is set to japanese");
#endif // __BANK
#endif // RSG_PC / RSG_ORBIS

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	PARAM(useManualLoadMenuToExportSavegame, "The Manual Load menu in the pause menu will export the savegame (for gen9 migration) instead of loading it and starting a new session");
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	PARAM(useManualSaveMenuToImportSavegame, "The Manual Save menu in the pause menu will import the savegame (for gen9 migration) instead of saving the state of the current single player session");
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

NOSTRIP_XPARAM(setGamma);

namespace rage {
	XPARAM(forceResolution);
#if !__FINAL
	XPARAM(BlockOnLostFocus);
#endif
}

RAGE_DEFINE_CHANNEL(ui)
RAGE_DEFINE_SUBCHANNEL(ui,report)
RAGE_DEFINE_SUBCHANNEL(ui,playercard)
RAGE_DEFINE_SUBCHANNEL(ui,netplayercard)
RAGE_DEFINE_SUBCHANNEL(ui,playerlist)

#if RSG_PC
RAGE_DEFINE_SUBCHANNEL(ui,mpchat)
#endif

RAGE_DEFINE_CHANNEL(uiSpew, DIAG_SEVERITY_ERROR, DIAG_SEVERITY_ERROR, DIAG_SEVERITY_ERROR )
#define uiSpew(fmt,...)					RAGE_DISPLAYF(uiSpew,fmt,##__VA_ARGS__)
#define uiSpew1(fmt,...)				RAGE_DEBUGF1(uiSpew,fmt,##__VA_ARGS__)

#if !__NO_OUTPUT
MenuScreenId s_UiSpewLastScreen;
#endif

#define OUTPUT_DIRTY_GFX_COMP 0

#define PERSISTENT_DATA "CPauseMenuPersistentData"
#define ARBITRARY_OFFSET_TO_FAKE_HISTORY 10000
#define NO_STREAMING_MOVIE (-1)
#define MENU_STATE_PUSH_SIZE 2
void CMenuScreen::RegisterTypes()
{
	REGISTER_MENU_TYPE("CREW", CCrewMenu);
	REGISTER_MENU_TYPE("CREWDETAILS", CCrewDetailMenu);
	REGISTER_MENU_TYPE("SCRIPT", CScriptMenu);
	REGISTER_MENU_TYPE("FRIENDS", CFriendsMenuSP);
	REGISTER_MENU_TYPE("MPFRIENDS", CFriendsMenuMP);
	REGISTER_MENU_TYPE("PLAYERS", CPlayersMenu);
	REGISTER_MENU_TYPE("LOBBY", CLobbyMenu);
	REGISTER_MENU_TYPE("PARTY", CPartyMenu);
	REGISTER_MENU_TYPE("CORONA_INVITE_CREWS", CCoronaCrewsInviteMenu);
	REGISTER_MENU_TYPE("CORONA_INVITE_FRIENDS", CCoronaFriendsInviteMenu);
	REGISTER_MENU_TYPE("CORONA_INVITE_PLAYERS", CCoronaPlayersInviteMenu);
	REGISTER_MENU_TYPE("CORONA_INVITE_JOINED", CCoronaJoinedPlayersMenu);
	REGISTER_MENU_TYPE("CORONA_INVITE_MATCHED_PLAYERS", CCoronaMatchedPlayersInviteMenu);
	REGISTER_MENU_TYPE("CORONA_INVITE_LAST_JOB_PLAYERS", CCoronaLastJobInviteMenu);
	REGISTER_MENU_TYPE("MAP", CMapMenu);
	REGISTER_MENU_TYPE("STATS", CStatsMenu);
	REGISTER_MENU_TYPE("INFO", CInfoMenu);
	REGISTER_MENU_TYPE("GALLERY", CGalleryMenu);
	REGISTER_MENU_TYPE("SETTINGS", CSettingsMenu);
    REGISTER_MENU_TYPE("STORETAB", CPauseStoreMenu);
	REGISTER_MENU_TYPE("CUTSCENE", CCutsceneMenu);
	REGISTER_MENU_TYPE("MOVIETAB", CPauseStoreMenu);
#if GTA_REPLAY
	REGISTER_MENU_TYPE("VIDEOEDITORTAB", CPauseVideoEditorMenu);
#endif
	KEYBOARD_MOUSE_ONLY( REGISTER_MENU_TYPE("KEYMAP", CKeyMappingMenu); )
	REGISTER_MENU_TYPE("SAVEGAME", CSavegameFrontEnd);
}

#if __PRELOAD_PAUSE_MENU
// on some games, this may change
// this COULD be more dynamic, but currently on every possible prompted pause menu the map comes first
#define DEFAULT_PRELOAD_PAGE "PAUSE_MENU_PAGES_MAP"
#define DEFAULT_PRELOAD_COMPONENTS "PAUSE_MENU_SHARED_COMPONENTS_03", "PAUSE_MENU_SHARED_COMPONENTS_02", "PAUSE_MENU_SHARED_COMPONENTS_MP_01"
#endif

#define PAUSEMENU_FILENAME_HEADER	"PAUSE_MENU_HEADER"  // PAUSE_MENU.GFX
#define PAUSEMENU_FILENAME_CONTENT	"PAUSE_MENU_SP_CONTENT"  // PAUSE_MENU.GFX
#define PAUSEMENU_FILENAME_INSTRUCTIONAL_BUTTONS	"PAUSE_MENU_INSTRUCTIONAL_BUTTONS"  // INSTRUCTIONAL_BUTTONS.GFX
#define PAUSEMENU_FILENAME_SHARED_COMPONENTS	"PAUSE_MENU_SHARED_COMPONENTS"  // PAUSE_MENU_SHARED_COMPONENTS.GFX

#define PAUSEMENU_DATA_XML_FILENAME	"common:/data/ui/pausemenu.xml"

#define SC_CONTEXT_BUTTON					UIATSTRINGHASH("NotInSocialClub",0xb5f0f8cf)
#define SC_UPDATE_CONTEXT_BUTTON			UIATSTRINGHASH("UpdateSocialClub",0x8b9cd5a)
#define SELECT_STORAGE_DEVICE_CONTEXT		UIATSTRINGHASH("SELECT_STORAGE_DEVICE",0x58cb3d5b)
#define NO_SAVEGAMES_CONTEXT				ATSTRINGHASH("NO_SAVEGAMES",0x7f80da90)
#define CAN_PLAY_CREDITS					ATSTRINGHASH("CAN_PLAY_CREDITS", 0x75E65184)

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
#define ALLOW_PROCESS_SAVEGAME_CONTEXT		ATSTRINGHASH("ALLOW_PROCESS_SAVEGAME",0xca0276e4)
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
#define ALLOW_IMPORT_SAVEGAME_CONTEXT		ATSTRINGHASH("ALLOW_IMPORT_SAVEGAME",0xB3D98636)
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
#if __ALLOW_SP_CREDITS
#define CREDITS_ENABLED_CONTEXT				ATSTRINGHASH("CREDITS_ENABLED",0x5DF9320)
#define LEGAL_ENABLED_CONTEXT				ATSTRINGHASH("LEGAL_ENABLED",0x7602AAA6)
#endif // __ALLOW_SP_CREDITS


#define PM_DELAY_UPDATE 3
#define STREAMING_FRAMES 2

static const strStreamingObjectName PAUSEMENU_TXD_PATH("platform:/textures/pausemenu_gfx",0x1A311E0B);

static bool bActionScriptPopulated = false;
static bool bActionScriptPopulatedWithHeadshot = false;

static const char* s_HudFrontendSoundset = "HUD_FRONTEND_DEFAULT_SOUNDSET";
static CSystemTimer s_iSpinnerTimer;

#if RSG_PS3
static const int CONTROLLER_RECONNECT_WARNING_THRESHOLD = 10;
static int iNumFramesControllerDisconnected = 0;
#endif // RSG_PS3

PauseMenuRenderData CPauseMenu::sm_RenderData;

bool CPauseMenu::sm_PauseMenuWarningScreen = false;
bool CPauseMenu::sm_EnableTogglePauseRenderPhases = true;
int CPauseMenu::sm_TogglePauseRPOwner = OWNER_OVERRIDE;
bool CPauseMenu::sm_PauseRenderPhasesStatus = false;
u8 CPauseMenu::sm_updateDelay = PM_DELAY_UPDATE;

bool CPauseMenu::sm_waitingForForceDropIntoMenu = false;
bool CPauseMenu::sm_forceDropIntoMenu = false;
bool CPauseMenu::sm_dropIntoMenuWhenStreamed = false;

#if RSG_ORBIS
int CPauseMenu::sm_iFriendPaneMovementInterval = 2000;
int CPauseMenu::sm_iFriendPaneMovementTunable = 2000;
#endif 

#if RSG_PC || RSG_DURANGO
int CPauseMenu::sm_GPUCountdownToPause = -1;
#endif

CMenuScreen::CollectionTypeMap CMenuScreen::sm_MenuTypeMap;

bool CPauseMenu::sm_bMaxPayneMode = false;
bool CPauseMenu::sm_bRestarting = false;
bool CPauseMenu::sm_bClosingDown = false;
bool CPauseMenu::sm_bClosingDownPart2 = false;
char CPauseMenu::sm_PauseMenuCloseDelay = 0;
bool CPauseMenu::sm_bCurrentMenuNeedsFading = false;
//s32 CPauseMenu::sm_iBackgroundFader = 40;
s32 CPauseMenu::sm_inputCooldownTimer = 0;
CSystemTimer CPauseMenu::sm_iMenuSelectTimer;
bool CPauseMenu::sm_bRenderContent = false;
sysTimer CPauseMenu::sm_PauseMenuCloseTimer;

bool CPauseMenu::sm_bWaitingOnPulseWarning = false;
bool CPauseMenu::sm_bWaitingOnAimWarning = false;
bool CPauseMenu::sm_bWaitingOnGfxOverrideWarning = false;

//bool	CPauseMenu::sm_bInGamePreview = false;
bool	CPauseMenu::sm_bNoValidSaveGameFiles = false;

bool CPauseMenu::sm_bSaveGameListSync = false;
bool CPauseMenu::sm_bQueueManualLoadASAP = false;
bool CPauseMenu::sm_bQueueManualSaveASAP = false;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CPauseMenu::sm_bQueueUploadSavegameASAP = false;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

bool CPauseMenu::sm_bAwaitingMenuShiftDepthResponse = false;

CPauseMenu::eSavegameMenuChoice CPauseMenu::sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_NONE;
bool CPauseMenu::sm_bSaveMenuIsBeingRedrawnAfterDeletingASavegame = false;

CSavegameQueuedOperation_ManualLoad CPauseMenu::sm_ManualLoadStructure;
CSavegameQueuedOperation_ManualSave CPauseMenu::sm_ManualSaveStructure;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
CSavegameQueuedOperation_ExportSinglePlayerSavegame CPauseMenu::sm_ExportSPSave;
bool CPauseMenu::sm_bUseManualLoadMenuToExportSavegame = false;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
CSavegameQueuedOperation_ImportSinglePlayerSavegame CPauseMenu::sm_ImportSPSave;
bool CPauseMenu::sm_bUseManualSaveMenuToImportSavegame = false;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

s32 CPauseMenu::iLoadNewGameTrigger = -1;
eMenuPref CPauseMenu::sm_iMenuPrefSelected = PREF_INVALID;
eMenuPref CPauseMenu::sm_iLastValidPref = PREF_INVALID;
bool CPauseMenu::sm_bHasFocusedMenu = false;
bool CPauseMenu::sm_bCloseMenus = false;
bool CPauseMenu::sm_bUnpauseGameOnMenuClose = true;
CPauseMenu::ClosingAction  CPauseMenu::sm_eClosingAction = CPauseMenu::CA_None;
// bool CPauseMenu::bCloseAndStartCommerce = false;
// bool CPauseMenu::bCloseAndStartSC = false;
// bool CPauseMenu::bCloseAndStartNewGame = false;
// bool CPauseMenu::bCloseAndWarp = false;
// bool CPauseMenu::bCloseAndLeaveGame = false;
// bool CPauseMenu::bCloseAndStartSavedGame = false;
#if RSG_PC
bool CPauseMenu::sm_bWaitOnExitToWindowsConfirmationScreen = false;
bool CPauseMenu::sm_bWantsToExitGame = false;
bool CPauseMenu::sm_bWantsToRestartGame = false;
bool CPauseMenu::m_bSetStereo = false;
bool CPauseMenu::m_bStereoOverride = false;
bool CPauseMenu::m_bIsStereoPossible = false;
bool CPauseMenu::sm_bWaitOnPCGamepadCalibrationScreen = false;
bool CPauseMenu::sm_bBackedWithChanges = false;
u32  CPauseMenu::sm_iExitTimer = 0;
#endif

bool CPauseMenu::sm_bDisableSpinner = false;
sysTimer CPauseMenu::sm_time;
bool CPauseMenu::sm_bWaitOnNewGameConfirmationScreen = false;
bool CPauseMenu::sm_bWaitOnDisplayCalibrationScreen = false;
bool CPauseMenu::sm_bDisplayCreditsScreenNextFrame = false;
bool CPauseMenu::sm_bWaitOnCreditsScreen = false;
MenuScreenId CPauseMenu::sm_eCreditsLaunchedFrom = MENU_UNIQUE_ID_INVALID;
atString CPauseMenu::sm_currentMissionTextStem;
bool CPauseMenu::sm_bActive = false;
bool CPauseMenu::sm_bGalleryMaximizeEnabled = false;
bool CPauseMenu::sm_bGalleryLoadingImage = false;
bool CPauseMenu::sm_bGalleryDisplayInstructionalButtons = false;
s32 CPauseMenu::sm_iCurrentHighlightedTabIndex = 0;
s32 CPauseMenu::sm_iStreamingMovie = NO_STREAMING_MOVIE;
s32 CPauseMenu::sm_iStreamedMovie = NO_STREAMING_MOVIE;
s32 CPauseMenu::sm_MenuPref[MAX_MENU_PREFERENCES];
char CPauseMenu::sm_displayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
bool CPauseMenu::sm_bWaitOnImportConfirmationScreen;
#endif
#if RSG_ORBIS
float CPauseMenu::sm_fOrbisSafeZone = 0.9f;
bool CPauseMenu::sm_bOrbisSafeZoneInError = false;
ServiceDelegate CPauseMenu::sm_SafeZoneDelegate;
#elif RSG_DURANGO
int CPauseMenu::sm_displayNameReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
bool CPauseMenu::sm_bHelpLoginPromptActive = false;
#endif
eFRONTEND_MENU_VERSION CPauseMenu::sm_iCurrentMenuVersion = FE_MENU_VERSION_SP_PAUSE;
CMenuVersion* CPauseMenu::sm_pCurrentMenuVersion = NULL;
eFRONTEND_MENU_VERSION CPauseMenu::sm_iRestartingMenuVersion;
MenuScreenId CPauseMenu::sm_iRestartingHighlightTab;
s32 CPauseMenu::sm_PedShotHandle = 0;//(s32)PedHeadshotManager::INVALID_HANDLE;
bool CPauseMenu::sm_bClanTextureRequested;
bool CPauseMenu::sm_bAllowRumble = false;
u32 CPauseMenu::sm_EndAllowRumbleTime = 0;
bank_u32 CPauseMenu::sm_RumbleDuration = 300; // milliseconds.
s32 CPauseMenu::sm_LastTpsControlsMode = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastFpsControlsMode = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastControlConfigChanged = PREF_CONTROL_CONFIG;
s32 CPauseMenu::sm_LastDuckHandBrakeMode = -1; // Initial invalid value.
CPauseMenu::QueuedOpenParams CPauseMenu::sm_QueuedOpenParams;
audScene* CPauseMenu::sm_frontendMenuScene = NULL;

#if GTA_REPLAY
eReplayMemoryLimit	CPauseMenu::sm_LastReplayMemoryLimit;
#endif // GTA_REPLAY

#if KEYBOARD_MOUSE_SUPPORT
s32 CPauseMenu::sm_LastLookInvertMode  = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseInvertMode = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseInvertFlyingMode = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseSwapRollYawFlyingMode = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseInvertSubMode = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseOnFootSensitivity  = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseDrivingSensitivity = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMousePlaneSensitivity = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseHeliSensitivity = -1; // Initial invalid value.

s32 CPauseMenu::sm_LastMouseDriveMode = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseFlyMode = -1; // Initial invalid value.
s32 CPauseMenu::sm_LastMouseSubMode = -1; // Initial invalid value.
#endif // KEYBOARD_MOUSE_SUPPORT

#if KEYBOARD_MOUSE_SUPPORT && FPS_MODE_SUPPORTED
s32 CPauseMenu::sm_LastFPSScopeState = -1;
#endif // KEYBOARD_MOUSE_SUPPORT && FPS_MODE_SUPPORTED

MenuScreenId CPauseMenu::sm_DefaultHighlightTab(MENU_UNIQUE_ID_INVALID);

// the number we need to count as 'complete'
bool CPauseMenu::sm_bRenderMenus;
bool CPauseMenu::sm_bScriptWasPaused;
u8   CPauseMenu::sm_iCallbacksPending;
const char* CPauseMenu::sm_pMsgToWarnOnTabChange = NULL;
bool CPauseMenu::sm_bStartedUserPause;
bool CPauseMenu::sm_bWaitingForFirstLayoutChanged = false;

#if KEYBOARD_MOUSE_SUPPORT
int CPauseMenu::sm_iMouseClickDirection = 0;
int CPauseMenu::sm_iMouseScrollDirection = SCROLL_CLICK_NONE;
const CPauseMenu::PMMouseEvent CPauseMenu::PMMouseEvent::INVALID_MOUSE_EVENT = { -1, -1, -1 };
CPauseMenu::PMMouseEvent CPauseMenu::sm_MouseHoverEvent = PMMouseEvent::INVALID_MOUSE_EVENT;
CPauseMenu::PMMouseEvent CPauseMenu::sm_MouseClickEvent = PMMouseEvent::INVALID_MOUSE_EVENT;
int CPauseMenu::sm_iHairColourSelected = -1;
#endif // KEYBOARD_MOUSE_SUPPORT

bool CPauseMenu::sm_bMenuTriggerEventOccurred;
bool CPauseMenu::sm_bMenuLayoutChangedEventOccurred;
s32 CPauseMenu::sm_iMenuEventOccurredUniqueId[3];

MenuScreenId CPauseMenu::sm_iCodeWantsScriptToControlScreen(MENU_UNIQUE_ID_INVALID);
bool CPauseMenu::sm_bMenuControlChangedThisFrame = false;
bool CPauseMenu::sm_bMenuControlIsLocked = false;
bool CPauseMenu::sm_bLockTheAcceptButton = false;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CPauseMenu::sm_bDelayedEntryToExportSavegameMenu = false;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
s8 CPauseMenu::sm_SpinnerColumnForFlaggedAsBusy = PauseMenuRenderData::NO_SPINNER;
bool CPauseMenu::sm_bCurrentScreenSelected;
atRangeArray<CScaleformMovieWrapper,MAX_PAUSE_MENU_BASE_MOVIES>		CPauseMenu::sm_iBaseMovieId;
atRangeArray<CStreamMovieHelper, MAX_PAUSE_MENU_CHILD_MOVIES>		CPauseMenu::sm_iChildrenMovies;

atArray<SPauseMenuState> CPauseMenu::sm_aMenuState;

bool CPauseMenu::sm_bSetupStartingPane;
int CPauseMenu::sm_iLoadingAssetsPhase;
#if __PRELOAD_PAUSE_MENU
int CPauseMenu::sm_iPreloadingAssetsPhase;
#endif
bool CPauseMenu::sm_bNavigatingContent;
bool CPauseMenu::sm_bStoreAvailable;

CSprite2d CPauseMenu::sm_MenuSprite[MAX_MENU_SPRITES];

bool CPauseMenu::sm_bProcessedContent = false;
bool CPauseMenu::sm_bCurrentlyOnAMission = false;
char CPauseMenu::sm_MissionNameLabel[MAX_LENGTH_OF_MISSION_TITLE];
bool CPauseMenu::sm_bMissionLabelIsALiteralString = false;

char CPauseMenu::sm_MissionDescriptionString[MAX_LENGTH_OF_MISSION_DESCRIPTION];
bool CPauseMenu::sm_bMissionDescriptionIsActive = false;

#if RSG_PC
bool CPauseMenu::sm_bCanApply = false;
#endif // RSG_PC

DynamicPauseMenu*			CPauseMenu::sm_pDynamicPause = NULL;
CPauseMenuPersistentData	CPauseMenu::sm_PersistentData;
CMenuArray					CPauseMenu::sm_MenuArray;
atArray<bool>				CPauseMenu::sm_TabLocked;

// TUNABLES
struct ScaleEffectTunable
{
	s16 EffectDuration;
	float fScalarStart;
	float fScalarEnd;
	float fAlphaStart;
	float fAlphaEnd;
	float fBlipAlphaStart;
	float fBlipAlphaEnd;
	int	  eLerpMethod;
};

#if __BANK
int CMenuItem::sm_Allocs = 0;
int CMenuItem::sm_Count  = 0;

int CMenuVersion::sm_Allocs = 0;
int CMenuVersion::sm_Count  = 0;

int CMenuScreen::sm_Allocs = 0;
int CMenuScreen::sm_Count  = 0;

      ScaleEffectTunable s_EffectSettings[DynamicPauseMenu::SE_Count] = 
#else
const ScaleEffectTunable s_EffectSettings[DynamicPauseMenu::SE_Count] = 
#endif
	// time,	scale		  alpha		  blips
	{ {350, 1.0f,	1.0f,	0.0f, 1.0f, 0.0f, 0.0f, rage::TweenStar::EASE_cubic_EIEO}	// intro
	, {125, 1.0f,   1.0f,	1.0f, 1.0f, 0.0f, 1.0f, rage::TweenStar::EASE_cubic_EI}		// intro (blips)
	, {350, 1.0f,   1.0f,	1.0f, 0.0f, 1.0f, 0.0f,rage::TweenStar::EASE_cubic_EO}		// outro
	};

bank_u32 CPauseMenu::sm_uDisableInputDuration = 250; // milliseconds.
BankInt32 TIME_TO_WAIT_UNTIL_SPINNER_APPEARS  = 350;
BankInt32 TIME_TO_WAIT_UNTIL_INITIAL_LOADING_SPINNER_APPEARS  = 1000;
BankInt32 MENU_PANE_MOVEMENT_INTERVAL		= 400;
BankInt32 MENU_PANE_MOVEMENT_INTERVAL_SHORT = -133;
BankInt32 SPIN_TIME	= 25;
BankInt32 CPauseMenu::FRONTEND_ANALOGUE_THRESHOLD = 80;  // out of 128
BankInt32 BUTTON_PRESSED_DOWN_INTERVAL = 250;
BankInt32 BUTTON_PRESSED_REFIRE_ATTRITION = 45;
BankInt32 BUTTON_PRESSED_REFIRE_MINIMUM = 100;
BankFloat TIME_WARP_LERP_SPEED = 500;

bool CPauseMenu::sm_languagePreferenceSet = false;
u32 CPauseMenu::sm_languageFromSystemLanguage=0;

s32 s_iLastRefireTimeUp = BUTTON_PRESSED_DOWN_INTERVAL;
s32 s_iLastRefireTimeDn = BUTTON_PRESSED_DOWN_INTERVAL;
u32 CPauseMenu::sm_uLastResumeTime = 0;
u32 CPauseMenu::sm_uTimeInputsDisabledFromClose = 0;

// ENUMS
enum PM_LOADING_PHASE
{
 	 PMLP_DONE
	, PMLP_FIRST
	, PMLP_LOAD_CUSTOM_COMPONENTS
	, PMLP_LOAD_FIRST_PANEL
	, PMLP_CHECK_ASSETS_LOADED
};


#if __DEV
bool sm_bDebugFileFail = false;
#endif

#if __BANK
bool s_bAlwaysFailTimeOut = false;
bool s_bDebugAlwaysReloadXML = false;
bool s_bDebugPauseRenderPhases = false;
#if __PRELOAD_PAUSE_MENU
bool s_bDebugResidentPauseMenu = true;
#endif
bool s_bShowLockedColumns = false;
bool s_bPauseMenuBankCreated = false;
CWarningMessage::Data* s_pDbgWarningData = NULL;
::rage::bkGroup* s_pTunablesGroup = NULL;
::rage::bkGroup* s_pXMLTunablesGroup = NULL;

bool s_bRaceFullscreenMapOpen = false;


atHashString s_pszCurrentMenuVersion("-undefined-",0xBD00438C);
atHashString s_pszCurrentMenu("-undefined-",0xBD00438C);
atHashString s_pszCurrentPanel("-undefined-",0xBD00438C);

atHashString s_pszDebugMenuVersionToOpen("FE_MENU_VERSION_SP_PAUSE",0xD528C7E2);
atHashString s_pszDebugMenuTabToOpen("MENU_UNIQUE_ID_INVALID",0xCEC507D);


atHashString s_pszDebugMenuOscillator[2] = { atHashString("FE_MENU_VERSION_CORONA",0x34fde059), atHashString("FE_MENU_VERSION_CORONA_LOBBY",0xd5cbc440) };
int			 s_iDbgFramesToWait = 100;
int			 s_iDbgFramesRemaining = 0;
bool		 s_bDbgOscillate = false;
int			 s_iOscillateField = 0;
bool		 s_bDebugPauseGame = true;

SPauseMenuState	s_CurrentStateDebug;

atHashString	s_PauseMenuState("PM_INACTIVE",0x20e1ebf5);
#endif //__BANK

//////////////////////////////////////////////////////////////////////////

CStreamMovieHelper::CStreamMovieHelper()
	: m_requestingScreen(MENU_UNIQUE_ID_INVALID)
	, m_iMenuceptionDir(kMENUCEPT_LIMBO)
{

}

CStreamMovieHelper::~CStreamMovieHelper()
{
	Remove();
}

void CStreamMovieHelper::Remove()
{
	m_movieId.RemoveMovie();
	m_requestingScreen = MENU_UNIQUE_ID_INVALID;
	m_iMenuceptionDir = kMENUCEPT_LIMBO;
	m_sGfxFilename.Clear();

	BANK_ONLY(m_dbgScreenName = m_requestingScreen.GetParserName());
}

void CStreamMovieHelper::Set(const char* cGfxFilename, MenuScreenId _newId, eMenuceptionDir _iMenuceptionDir /* = kMENUCEPT_LIMBO */, bool bWaitForLoad/*=false*/)
{
	uiAssertf(_newId != MENU_UNIQUE_ID_INVALID, "Oh HELL no you didn't request an invalid movie to load '%s'!", cGfxFilename);

	if( m_movieId.IsFree() && cGfxFilename && cGfxFilename[0] != '\0')
	{
		// need to find latest SC movie id to use here, not the base as it fucks with the dependencies
		int dependentSCMovieId = INVALID_MOVIE_ID;
		int i=PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_END;
		while( i >= PAUSE_MENU_MOVIE_SHARED_COMPONENTS && dependentSCMovieId == INVALID_MOVIE_ID )
		{
			dependentSCMovieId = CPauseMenu::GetMovieWrapper(eNUM_PAUSE_MENU_MOVIES(i)).GetMovieID();
			--i;
		}

		if(bWaitForLoad)
		{
			m_movieId.CreateMovieAndWaitForLoad(SF_BASE_CLASS_PAUSEMENU
				, cGfxFilename
				, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f)
				, true
				, CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).GetMovieID()
				, dependentSCMovieId
				, false
				);
		}
		else
		{
			m_movieId.CreateMovie(SF_BASE_CLASS_PAUSEMENU
				, cGfxFilename
				, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f)
				, true
				, CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).GetMovieID()
				, dependentSCMovieId
				, false
				);
		}
	}
	m_requestingScreen = _newId;
	m_iMenuceptionDir = _iMenuceptionDir;
	m_sGfxFilename = cGfxFilename;

	BANK_ONLY(m_dbgScreenName = m_requestingScreen.GetParserName());
}

#if __BANK

void CStreamMovieHelper::AddWidgets(bkBank* bank, int iIndex)
{
	char baseName[32];
	formatf(baseName, 32, "Child MovieIndex #%02i", iIndex);
	bank->AddText(baseName, m_movieId.GetMovieIDPtr());
	bank->AddText("Requesting Screen", &m_dbgScreenName);
	bank->AddText("GFXFilename", &m_sGfxFilename);
	bank->AddText("MenuceptionDir",	reinterpret_cast<int*>(&m_iMenuceptionDir));


}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::InitWidgets()
// PURPOSE: inits the ui bank widget and "Create" button
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (!pBank)  // create the bank if not found
	{
		pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
	}

	if (pBank)
	{
		pBank->AddButton("Create Pause Menu widgets", &CPauseMenu::CreateBankWidgets);
#if !__FINAL
		if( PARAM_autoaddPausemenuWidgets.Get() )
			CPauseMenu::CreateBankWidgets();
#endif
	}
}

void CPauseMenu::DebugRescalePauseMenu()
{
	CPauseMenu::ScaleContentMovie(false);
}

void CPauseMenu::DebugOpenMenu()
{
	// can't use the hash since eMenuScreen_Data uses LITERAL hash, but the bank widgets don't support literal
	eMenuScreen tabToHighlight = s_pszDebugMenuTabToOpen.IsNotNull() ? static_cast<eMenuScreen>(parser_eMenuScreen_Data.ValueFromName(s_pszDebugMenuTabToOpen.GetCStr())) : MENU_UNIQUE_ID_INVALID;
	CPauseMenu::Open(s_pszDebugMenuVersionToOpen, s_bDebugPauseGame, tabToHighlight, true);
}

void CPauseMenu::DebugRestartMenu()
{
	// can't use the hash since eMenuScreen_Data uses LITERAL hash, but the bank widgets don't support literal
	eMenuScreen tabToHighlight = s_pszDebugMenuTabToOpen.IsNotNull() ? static_cast<eMenuScreen>(parser_eMenuScreen_Data.ValueFromName(s_pszDebugMenuTabToOpen.GetCStr())) : MENU_UNIQUE_ID_INVALID;
	CPauseMenu::Restart(s_pszDebugMenuVersionToOpen, tabToHighlight);
}

void DbgAdjustSafeZone()
{
	CPauseMenu::SetValueBasedOnPreference(PREF_SAFEZONE_SIZE, CPauseMenu::UPDATE_PREFS_FROM_MENU);
}

void DbgToggleRenderPhase()
{
	bool newState = CPauseMenu::GetPauseRenderPhasesStatus();
	CPauseMenu::TogglePauseRenderPhases(newState,OWNER_OVERRIDE, OUTPUT_ONLY( __FUNCTION__ ) );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::InitWidgets()
// PURPOSE: creates the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::CreateBankWidgets()
{
	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (!s_bPauseMenuBankCreated && bank)
	{
		bank->PushGroup("Pause Menu");
			bank->AddSlider("Safe Zone", &sm_MenuPref[PREF_SAFEZONE_SIZE], 0, SAFEZONE_SLIDER_MAX, 1, datCallback(DbgAdjustSafeZone));
			bank->AddSeparator();
			bank->AddButton("Rescale Pause Menu", &DebugRescalePauseMenu);
			bank->AddToggle("Always Reload the XML", &s_bDebugAlwaysReloadXML);
#if __PRELOAD_PAUSE_MENU
			bank->AddToggle("Keep Pause Menu Resident", &s_bDebugResidentPauseMenu);
#endif
			bank->AddToggle("Always Fail Time out", &s_bAlwaysFailTimeOut);
			bank->AddToggle("Render Phases Toggle Enabled", &sm_EnableTogglePauseRenderPhases);
			bank->AddButton("Toggle Render Phase", datCallback(DbgToggleRenderPhase));
			bank->AddToggle("SHOW_LOCKED_COLUMNS", &s_bShowLockedColumns, &CPauseMenu::SendDebugFunctionToActionScript);
			bank->AddToggle("Pause render phases", &s_bDebugPauseRenderPhases, &CPauseMenu::DebugPauseRenderPhases);
			bank->AddSeparator();
			bank->AddButton("Debug open menu", &CPauseMenu::DebugOpenMenu);
			bank->AddButton("Debug restart menu", &CPauseMenu::DebugRestartMenu);
			bank->AddButton("Close menu", &CPauseMenu::Close);
			bank->AddButton("Redraw Instruction Buttons", datCallback(CFA1(CPauseMenu::RedrawInstructionalButtons),0));
			bank->AddText("Menu Version to Open", &s_pszDebugMenuVersionToOpen);
			bank->AddToggle("Pause Game on open", &s_bDebugPauseGame);
			bank->AddText("Menu Tab to Open", &s_pszDebugMenuTabToOpen);

			bank->PushGroup("Corona Race");
			{
				bank->AddToggle("Showing fullscreen map", &s_bRaceFullscreenMapOpen, &CPauseMenu::DebugToggleCoronaFullscreenMap);
			}
			bank->PopGroup();

			bank->PushGroup("Spammer");
			{
				bank->AddToggle("Spam Restarts", &s_bDbgOscillate);
				bank->AddSlider("Frame Delay", &s_iDbgFramesToWait, 0, 10000, 1);
				bank->AddText("Menu Version to Open 1", &s_pszDebugMenuOscillator[0]);
				bank->AddText("Menu Version to Open 2", &s_pszDebugMenuOscillator[1]);
			}
			bank->PopGroup();
#if KEYBOARD_MOUSE_SUPPORT
			bank->PushGroup("Mouse");
			{
				bank->AddText("Mouse Hover Index", &sm_MouseHoverEvent.iIndex);
				bank->AddText("Mouse Hover MenuItemId", &sm_MouseHoverEvent.iMenuItemId);
				bank->AddText("Mouse Hover UniqueId", &sm_MouseHoverEvent.iUniqueId);
				bank->AddSeparator();
				bank->AddText("Mouse Click Index", &sm_MouseClickEvent.iIndex);
				bank->AddText("Mouse Click MenuItemId", &sm_MouseClickEvent.iMenuItemId);
				bank->AddText("Mouse Click UniqueId", &sm_MouseClickEvent.iUniqueId);
				bank->AddSeparator();
				bank->AddText("Mouse Click Direction", &sm_iMouseClickDirection);
				bank->AddText("Mouse Scroll Direction", &sm_iMouseScrollDirection);
			}
			bank->PopGroup();
#endif
			bank->AddSeparator();
			bank->AddText("State", &s_PauseMenuState);
			bank->AddText("IsActive", &sm_bActive);
			bank->AddText("IsRestarting", &sm_bRestarting);
			bank->AddText("IsClosingDown", &sm_bClosingDown);
			bank->AddText("IsClosingDownPart2", &sm_bClosingDownPart2);
			bank->AddText("IsNavigatingContent", &sm_bNavigatingContent);
			bank->AddText("HasFocusedMenu", &sm_bHasFocusedMenu);
			bank->AddText("RenderContent", &sm_bRenderContent);
			bank->AddText("RenderMenus", &sm_bRenderMenus);
			bank->AddText("Current Menu Version", (int*)&sm_iCurrentMenuVersion);
			bank->AddText("Current Menu Version", &s_pszCurrentMenuVersion);
			bank->AddText("Current Menu", s_CurrentStateDebug.currentScreen.GetValuePtr());
			bank->AddText("Current Menu", &s_pszCurrentMenu);
			bank->AddText("Current Panel", s_CurrentStateDebug.currentActivePanel.GetValuePtr());
			bank->AddText("Current Panel", &s_pszCurrentPanel);
			bank->AddText("Current Menu Pref", (int*)&sm_iMenuPrefSelected);
			bank->AddText("Last Valid Pref", (int*)&sm_iLastValidPref);
			bank->AddText("Current Highlighted Tab Index", &sm_iCurrentHighlightedTabIndex);
			bank->AddSlider("Current Menuception Depth", sm_aMenuState.GetCountPointer(),0,16,0);

			bank->PushGroup("Busy Spinners");
			{
				char temp[32];
				for(int i=0; i < PauseMenuRenderData::MAX_SPINNERS; ++i )
				{
					formatf(temp, NELEM(temp), "Spinner #%i", i);
					bank->AddSlider(temp, &sm_RenderData.iLoadingSpinnerDesired[i], s8(PauseMenuRenderData::NO_SPINNER), s8(sm_PersistentData.Spinner.Offsets.GetCount()-1),1);
				}

				for(int i=0; i<sm_PersistentData.Spinner.Offsets.GetCount(); ++i)
				{
					formatf(temp, NELEM(temp), "Spinner loc #%i x", i);
					bank->AddSlider(temp, &sm_PersistentData.Spinner.Offsets[i].x, 0.0f, 1.0f, 0.001f);

					formatf(temp, NELEM(temp), "Spinner loc #%i y", i);
					bank->AddSlider(temp, &sm_PersistentData.Spinner.Offsets[i].y, 0.0f, 1.0f, 0.001f);
				}
			}
			bank->PopGroup();

			bank->PushGroup("Stream Requests");
			{
				bank->AddText("Instructional Buttons MovieID", sm_iBaseMovieId[PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS].GetMovieIDPtr());
				bank->AddText("Shared Components MovieID", sm_iBaseMovieId[PAUSE_MENU_MOVIE_SHARED_COMPONENTS].GetMovieIDPtr());
				bank->AddText("Content MovieID", sm_iBaseMovieId[PAUSE_MENU_MOVIE_CONTENT].GetMovieIDPtr());
				bank->AddText("Header MovieID", sm_iBaseMovieId[PAUSE_MENU_MOVIE_HEADER].GetMovieIDPtr());
				bank->AddSeparator();

				bank->AddText("Streaming Movie Index", &sm_iStreamingMovie);
				bank->AddText("Streamed Movie Index", &sm_iStreamedMovie);

				for(int i=0; i < sm_iChildrenMovies.GetMaxCount(); ++i)
				{
					bank->AddSeparator();
					sm_iChildrenMovies[i].AddWidgets(bank,i);
				}
				
			}

			bank->PopGroup();
			if( sm_pDynamicPause )
			{
				sm_pDynamicPause->AddWidgets(bank);

				atArray<CMenuScreen>& menuScreens = GetMenuArray().MenuScreens;
				for(int i=0; i < menuScreens.GetCount(); ++i)
				{
					if( menuScreens[i].HasDynamicMenu() )
						 menuScreens[i].GetDynamicMenu()->AddWidgets(bank);
				}
			}

			s_pTunablesGroup = bank->PushGroup("Tunables");
			{
				bank->AddSlider("Control Disable Time", &sm_uDisableInputDuration, 0, 2000, 1);
				bank->AddSlider("TIME_TO_WAIT_UNTIL_SPINNER_APPEARS",	&TIME_TO_WAIT_UNTIL_SPINNER_APPEARS, 0, 2000, 125);
				bank->AddSlider("TIME_TO_WAIT_UNTIL_INITIAL_LOADING_SPINNER_APPEARS",	&TIME_TO_WAIT_UNTIL_INITIAL_LOADING_SPINNER_APPEARS, 0, 2000, 125);
				
				bank->AddSlider("SPIN_TIME",							&SPIN_TIME, 0, 1000, 5);

				bank->AddSlider("MENU_PANE_MOVEMENT_INTERVAL",			&MENU_PANE_MOVEMENT_INTERVAL, 0, 2000, 125);
				bank->AddSlider("MENU_PANE_MOVEMENT_INTERVAL_SHORTENER", &MENU_PANE_MOVEMENT_INTERVAL_SHORT, -2000, 2000, 125);
				bank->AddSlider("FRONTEND_ANALOGUE_THRESHOLD",			&FRONTEND_ANALOGUE_THRESHOLD, 0, 128, 5);
				bank->AddSlider("BUTTON_PRESSED_DOWN_INTERVAL",			&BUTTON_PRESSED_DOWN_INTERVAL, 0, 2000, 125);
				bank->AddSlider("BUTTON_PRESSED_REFIRE_ATTRITION",		&BUTTON_PRESSED_REFIRE_ATTRITION, 0, 2000, 125);
				bank->AddSlider("BUTTON_PRESSED_REFIRE_MINIMUM",		&BUTTON_PRESSED_REFIRE_MINIMUM, 0, 2000, 125);
				
#if RSG_ORBIS
				bank->AddSlider("sm_iFriendPaneMovementTunable", &sm_iFriendPaneMovementTunable, 0, 4000, 100);
#endif
				bank->AddSlider("s_iLastRefireTimeUp",		&s_iLastRefireTimeUp, 0, 2000, 125);
				bank->AddSlider("s_iLastRefireTimeDn",		&s_iLastRefireTimeDn, 0, 2000, 125);
			}

			bank->PopGroup();

			CreateXMLDrivenWidgets();

			bank->PushGroup("Memory");
			{
#define ADD_ALLOC_SLIDER(class)\
	bank->AddSlider(#class " Count",	 &class::sm_Count,  0, 10000, 0);\
	bank->AddSlider(#class " Footprint", &class::sm_Allocs, 0, 10000, 0);
				ADD_ALLOC_SLIDER(CMenuItem);
				ADD_ALLOC_SLIDER(CMenuVersion);
				ADD_ALLOC_SLIDER(CMenuScreen);
				ADD_ALLOC_SLIDER(CContextMenuOption);
				ADD_ALLOC_SLIDER(CContextMenu);
				ADD_ALLOC_SLIDER(CMenuButton);
				ADD_ALLOC_SLIDER(CMenuButtonList);
				ADD_ALLOC_SLIDER(CMenuBase);
			}
			bank->PopGroup();

		bank->PopGroup();

		SUIContexts::CreateBankWidgets(bank);
		CUIMenuPed::CreateBankWidgets(bank);

		PAUSEMENUPOSTFXMGR.AddWidgets(*bank);

		s_bPauseMenuBankCreated = true;
	}
}

void CPauseMenu::CreateXMLDrivenWidgets()
{
	if(!s_pTunablesGroup)
		return;

	if(s_pXMLTunablesGroup)
		s_pXMLTunablesGroup->Destroy();

	s_pXMLTunablesGroup = s_pTunablesGroup->AddGroup("From PauseMenu.xml");

	::rage::atBinaryMap<SGeneralPauseDataConfig,::rage::atHashWithStringBank >& rMovieSettings = CPauseMenu::GetMenuArray().GeneralData.MovieSettings;
	
	for(::rage::atBinaryMap<SGeneralPauseDataConfig,::rage::atHashWithStringBank >::Iterator iter(rMovieSettings.Begin());
		iter!=rMovieSettings.End();
		++iter)
	{
		SGeneralPauseDataConfig* pConfig = &(*iter);
		bkGroup* pGroup = s_pXMLTunablesGroup->AddGroup(iter.GetKey().TryGetCStr(), false);
		pGroup->AddVector("vPos",	&pConfig->vPos,		-1.0f, 1.0f, 0.001f);
		pGroup->AddVector("vSize",	&pConfig->vSize,	-2.0f, 2.0f, 0.001f);
		pGroup->AddToggle("bDependsOnSharedComponents",	&pConfig->bDependsOnSharedComponents);
		pGroup->AddToggle("bRequiresMovieView",			&pConfig->bRequiresMovieView);
		pGroup->AddSlider("Loading Order",				&pConfig->LoadingOrder, 0, 255, 1);
	}
}

void DynamicPauseMenu::AddWidgets(bkBank* pToThis)
{
	if(!pToThis || s_pDbgWarningData)
		return;

	s_pDbgWarningData = rage_new CWarningMessage::Data;
	m_dbgTitle[0] = m_dbgBody[0] = m_dbgSubstring[0] = m_dbgSubstring1[0] = m_dbgSubstring2[0] = '\0';

	m_pGroup = pToThis->PushGroup("Dynamic Stuff");
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

		m_TimeWarper.AddWidgets(pToThis->PushGroup("Time Warp") );
			pToThis->AddSlider("LERP TIME", &TIME_WARP_LERP_SPEED, 0, 10000,50);

			for(int i=0; i < SE_Count; ++i)
			{
				const char* pszName = (i==SE_Intro) ? "Intro" : (i==SE_IntroBlips) ? "Intro Blips" : "Outro";
				pToThis->PushGroup( pszName );
				{
					ScaleEffectTunable& t = s_EffectSettings[i];
					pToThis->AddCombo("LERP TYPE",		&t.eLerpMethod,		COUNTOF(pszaEaseNames), pszaEaseNames);
					pToThis->AddSlider("Scale Effect Time",	&t.EffectDuration,  0, 10000, 33);
					pToThis->AddSlider("Scalar Start",	&t.fScalarStart,	0, 5.0, 0.25);
					pToThis->AddSlider("Scalar End",	&t.fScalarEnd,		0, 5.0, 0.25);
					pToThis->AddSlider("Alpha Start",	&t.fAlphaStart,		0, 1.0, 0.125);
					pToThis->AddSlider("Alpha End",		&t.fAlphaEnd,		0, 1.0, 0.125);
					pToThis->AddSlider("Blip Start",	&t.fBlipAlphaStart,	0, 1.0, 0.125);
					pToThis->AddSlider("Blip End",		&t.fBlipAlphaEnd,	0, 1.0, 0.125);
					pToThis->AddButton("Trigger Effect", datCallback( MFA1( DynamicPauseMenu::StartScaleEffectDBG ), this, CallbackData( static_cast<SE_Dir>(i)), false) );

				}
				pToThis->PopGroup();
			}
		pToThis->PopGroup();

		pToThis->PushGroup("Warning Screen");
		{
			pToThis->AddText("Title",		m_dbgTitle, 64);
			pToThis->AddText("Body",		m_dbgBody, 64);
			pToThis->AddText("Subtext",		m_dbgSubstring, 64);
			pToThis->AddText("Substring 1",	m_dbgSubstring1, 64);
			pToThis->AddText("Substring 2",	m_dbgSubstring2, 64);
			pToThis->AddText("ButtonBits",	reinterpret_cast<int*>(&s_pDbgWarningData->m_iFlags));

			pToThis->AddButton("Add Message",     datCallback(MFA(DynamicPauseMenu::DbgAddMessage), this));
			pToThis->AddButton("Clear Message",   datCallback(MFA(DynamicPauseMenu::DbgClearMessage), this));
			s_pDbgWarningData->m_acceptPressed	= datCallback(MFA(DynamicPauseMenu::DbgClearMessage), this);
			s_pDbgWarningData->m_declinePressed	= datCallback(MFA(DynamicPauseMenu::DbgClearMessage), this);
			s_pDbgWarningData->m_xPressed		= datCallback(MFA(DynamicPauseMenu::DbgClearMessage), this);
			s_pDbgWarningData->m_yPressed		= datCallback(MFA(DynamicPauseMenu::DbgClearMessage), this);
		}
		pToThis->PopGroup();
	}
	pToThis->PopGroup();
}

void DynamicPauseMenu::StartScaleEffectDBG(SE_Dir whichWay)
{ 
	if( whichWay == SE_Intro )  
		StartScaleEffect( whichWay, datCallback( MFA1( DynamicPauseMenu::StartScaleEffectDBG ), this, CallbackData( SE_IntroBlips), false) );
	else
		StartScaleEffect(whichWay);
}

void DynamicPauseMenu::DbgAddMessage()
{
	if( s_pDbgWarningData )
	{
		s_pDbgWarningData->m_TextLabelHeading = m_dbgTitle;
		s_pDbgWarningData->m_TextLabelBody	  = m_dbgBody;
		s_pDbgWarningData->m_TextLabelSubtext = m_dbgSubstring;
		s_pDbgWarningData->m_FirstSubString.Set( m_dbgSubstring1 );
		s_pDbgWarningData->m_SecondSubString.Set( m_dbgSubstring2 );
		GetErrorMessage().SetMessage(*s_pDbgWarningData);
	}
}

void DynamicPauseMenu::DbgClearMessage()
{
	GetErrorMessage().Clear();
}
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SendDebugFunctionToActionScript()
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SendDebugFunctionToActionScript()
{
	GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SHOW_LOCKED_COLUMNS", s_bShowLockedColumns);
}

void CPauseMenu::DebugPauseRenderPhases()
{
	TogglePauseRenderPhases(!s_bDebugPauseRenderPhases, OWNER_OVERRIDE, __FUNCTION__ );
}

void CPauseMenu::DebugToggleCoronaFullscreenMap()
{
	bool bKeyboard = false;
	WIN32PC_ONLY(bKeyboard = CControlMgr::GetMainFrontendControl().WasKeyboardMouseLastKnownSource();)
	if (s_bRaceFullscreenMapOpen)
	{
		if (bKeyboard)
		{
			SUIContexts::Deactivate("CORONA_BIGMAP_AVAIL_KM");
			SUIContexts::Activate("CORONA_BIGMAP_CLOSE_KM");
			SUIContexts::Deactivate("CORONA_BIGMAP_AVAIL");
			SUIContexts::Deactivate("CORONA_BIGMAP_CLOSE");
		}
		else
		{
			SUIContexts::Deactivate("CORONA_BIGMAP_AVAIL");
			SUIContexts::Activate("CORONA_BIGMAP_CLOSE");
			SUIContexts::Deactivate("CORONA_BIGMAP_AVAIL_KM");
			SUIContexts::Deactivate("CORONA_BIGMAP_CLOSE_KM");
		}
	} 
	else
	{
		if (bKeyboard)
		{
			SUIContexts::Activate("CORONA_BIGMAP_AVAIL_KM");
			SUIContexts::Deactivate("CORONA_BIGMAP_CLOSE_KM");
			SUIContexts::Deactivate("CORONA_BIGMAP_AVAIL");
			SUIContexts::Deactivate("CORONA_BIGMAP_CLOSE");
		}
		else
		{
			SUIContexts::Activate("CORONA_BIGMAP_AVAIL");
			SUIContexts::Deactivate("CORONA_BIGMAP_CLOSE");
			SUIContexts::Deactivate("CORONA_BIGMAP_AVAIL_KM");
			SUIContexts::Deactivate("CORONA_BIGMAP_CLOSE_KM");
		}
	}

	ToggleFullscreenMap(s_bRaceFullscreenMapOpen);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::ShutdownWidgets()
// PURPOSE: removes the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::ShutdownWidgets()
{
	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (bank)
	{
		SUIContexts::DestroyBankWidgets();

		bank->Destroy();
	}
}
#endif //__BANK



void CMenuVersion::preLoad(parTreeNode* pTreeNode)
{
	parTreeNode* pContexts = pTreeNode->FindChildWithName("InitialContexts");
	if( pContexts )
	{
		m_InitialContexts.ParseFromString(pContexts->GetData(),false);
	}
}

void CMenuItem::preLoad(parTreeNode* pTreeNode)
{
	parTreeNode* pMenus = pTreeNode->FindChildWithName("Contexts");
	if( pMenus )
	{
		m_ContextHashes.ParseFromString(pMenus->GetData());
	}

	parTreeNode* pHashed = pTreeNode->FindChildWithName("MenuUniqueIdHash");
	if( pHashed )
	{
		MenuUniqueId.SetFromXML(pHashed->GetData());
	}
}

void CMenuScreen::CreateDynamicMenu()
{
	if( m_pDynamicMenu != NULL )
	{
		uiWarningf("Menu %s already had a dynamic menu when re-initialized. If you see weirdness, this could be why.", MenuScreen.GetParserName());
		return;
	}
	
	if( m_pDynamicData && uiVerifyf(m_pDynamicData->type.IsNotNull(),"Runtime object specified but with no valid type!") )
	{
		UIMenuCollectionFunc* func = sm_MenuTypeMap.Access(m_pDynamicData->type);

		if(uiVerifyf(func, "MenuScreen::CreateDynamicMenu - don't know (%s)", m_pDynamicData->type.TryGetCStr()))
		{
			uiDebugf1("Creating dynamic menu type: %s for Menu %s", atHashString(m_pDynamicData->type).TryGetCStr(), MenuScreen.GetParserName());
			m_pDynamicMenu = (*func)(*this);
		}
	}
}

int SortMenuScreens(const CMenuScreen* a, const CMenuScreen* b)
{
	if( a->MenuScreen.GetValue() < b->MenuScreen.GetValue() )
		return -1;
	if( a->MenuScreen.GetValue() > b->MenuScreen.GetValue() )
		return 1;

	return 0;
}

void CMenuArray::postLoad()
{
#if __ASSERT
	// check for dupes using a 'set' (the closest thing we got)
	atMap<s32,u8> usedMenuIds;
	for(int i=0; i < MenuScreens.GetCount(); ++i)
	{
		CMenuScreen& curScreen = MenuScreens[i];
		bool bDoesntExist = !usedMenuIds.Access(curScreen.MenuScreen.GetValue());
		if( uiVerifyf(bDoesntExist, "There are two elements with the same ID of %s in PauseMenu.XML!", curScreen.MenuScreen.GetParserName()) )
			usedMenuIds[curScreen.MenuScreen.GetValue()] = 1; // this part is unnecessary.

		// check headers for sanity of contexts (only the last one may have any)
		if( curScreen.depth == MENU_DEPTH_HEADER )
		{
			for(int j=0; j < curScreen.MenuItems.GetCount()-1; ++j)
			{
				CMenuItem& curItem = curScreen.MenuItems[j];
				uiAssertf( !curItem.m_ContextHashes.IsSet(), "In Menu %s, item %s has <Contexts> set--Only the last item in a header may use contexts!", curScreen.MenuScreen.GetParserName(), curItem.MenuUniqueId.GetParserName() );
			}
		}
	}
#endif

	// sort the IDs so we can do a binary search later
	MenuScreens.QSort(0,-1, SortMenuScreens);
}

bool IsOfOnScreenType( const CMenuItem& checkThisGuy )
{
	return checkThisGuy.MenuAction == MENU_OPTION_ACTION_LINK 
		|| checkThisGuy.MenuAction == MENU_OPTION_ACTION_PREF_CHANGE 
		|| checkThisGuy.MenuAction == MENU_OPTION_ACTION_TRIGGER
		|| checkThisGuy.MenuAction == MENU_OPTION_ACTION_INCEPT
		|| checkThisGuy.MenuAction == MENU_OPTION_ACTION_SEPARATOR;
}

int CMenuScreen::FindItemIndex(MenuScreenId findMatch, int* onScreenIndex /* = NULL */, bool bFilterForOnscreen /* = false */) const
{
	if( onScreenIndex )
		(*onScreenIndex) = 0;

	for(int i=0; i < MenuItems.GetCount(); ++i)
	{
		const CMenuItem& curItem = MenuItems[i];
		bool bWouldveShown = IsOfOnScreenType(curItem) && curItem.CanShow();

		if( curItem.MenuUniqueId == findMatch && ( !bFilterForOnscreen || bWouldveShown ) )
		{
			uiAssertf( IsOfOnScreenType(curItem)
				, "FindOnScreenItemIndex: Item matching '%s' is of wrong type '%s!'"
				, findMatch.GetParserName()
				, parser_eMenuAction_Strings[curItem.MenuAction] );

			return i;
		}
		else if( onScreenIndex && bWouldveShown )
		{
			++(*onScreenIndex);
		}
	}

	// not found. -1 for you!
	return -1;
}

int CMenuScreen::FindItemIndex(eMenuPref findMatch, int* onScreenIndex /* = NULL */) const
{
	if( onScreenIndex )
		(*onScreenIndex) = 0;

	for(int i=0; i < MenuItems.GetCount(); ++i)
	{
		const CMenuItem& curItem = MenuItems[i];
		if( curItem.MenuPref == findMatch && curItem.MenuAction == MENU_OPTION_ACTION_PREF_CHANGE)
		{
			uiAssertf( IsOfOnScreenType(curItem)
				, "FindOnScreenItemIndex: Item matching '%d' is of wrong type '%s!'"
				, parser_eMenuPref_Values[findMatch].m_Value
				, parser_eMenuAction_Strings[curItem.MenuAction] );

			return i;
		}
		else if( onScreenIndex && IsOfOnScreenType(curItem) && curItem.CanShow() )
		{
			++(*onScreenIndex);
		}
	}

	// not found. -1 for you!
	return -1;
}

int CMenuScreen::FindOnscreenItemIndex(int onScreenIndex, eMenuAction kActionFilter ) const
{
	for(int i=0; i < MenuItems.GetCount(); ++i)
	{
		const CMenuItem& curItem = MenuItems[i];
		if( IsOfOnScreenType(curItem) && curItem.CanShow() )
		{
			if( kActionFilter == MENU_OPTION_ACTION_NONE || kActionFilter == curItem.MenuAction)
			{
				if( onScreenIndex == 0 )
					return i;
				--onScreenIndex;
			}
		}
	}

	// not found. -1 for you!
	return -1;
}

bool CMenuScreen::WouldShowAtLeastTwoItems() const
{
	bool bHitOneAlready = false;
	for(int i=0; i < MenuItems.GetCount(); ++i)
	{
		const CMenuItem& curItem = MenuItems[i];
		if( IsOfOnScreenType(curItem) && curItem.CanShow() )
		{
			if( bHitOneAlready )
				return true;
			bHitOneAlready = true;
		}
	}

	return false;
}

const atString* CMenuScreen::FindDatum( const atHashWithStringDev& key) const
{
	if( !m_pDynamicData )
		return NULL;

	return m_pDynamicData->params.SafeGet(key);
}

bool CMenuScreen::FindDynamicData(atHashWithStringDev key, int& out_Value, int DefaultValue /* = 0 */) const
{
	if( const atString* pData = FindDatum(key) )
	{
		out_Value = parUtils::StringToInt(pData->c_str());
		return true;
	}
	out_Value = DefaultValue;
	return false;
}

bool CMenuScreen::FindDynamicData(atHashWithStringDev key, bool& out_Value, bool DefaultValue /* = false */) const
{
	if( const atString* pData = FindDatum(key) )
	{
		out_Value = parUtils::StringToBool(pData->c_str());
		return true;
	}
	out_Value = DefaultValue;
	return false;
}

bool CMenuScreen::FindDynamicData(atHashWithStringDev key, atString& out_Value, const char* DefaultValue /* = NULL */) const
{
	if( const atString* pData = FindDatum(key) )
	{
		out_Value = *pData;
		return true;
	}
	out_Value = DefaultValue;
	return false;
}

bool CMenuScreen::FindDynamicData(atHashWithStringDev key, const char*& out_Value, const char* DefaultValue /* = NULL */) const
{
	if( const atString* pData = FindDatum(key) )
	{
		out_Value = pData->c_str();
		return true;
	}
	out_Value = DefaultValue;
	return false;
}

//////////////////////////////////////////////////////////////////////////

void CMenuScreen::DrawInstructionalButtons(MenuScreenId iMenuScreen, s32 iUniqueId)
{
	// If no dynamic or the override passes, use the general defaults
	if( IsContextMenuOpen() || (!HasFlag(AlwaysUseButtons) && (!CPauseMenu::IsNavigatingContent() || !m_pButtonList || m_pButtonList->empty()) ))
	{
		CPauseMenu::GetMenuArray().DefaultButtonList.Draw();
	}
	else
	{
		if( HasDynamicMenu() )
			GetDynamicMenu()->PrepareInstructionalButtons(iMenuScreen, iUniqueId);
		if( m_pButtonList )
			m_pButtonList->Draw();
	}
}

void CMenuScreen::ClearInstructionalButtons()
{
	// If no dynamic or the override passes, use the general defaults
	if( IsContextMenuOpen() || (!HasFlag(AlwaysUseButtons) && (!CPauseMenu::IsNavigatingContent() || !m_pButtonList || m_pButtonList->empty()) ))
	{
		CPauseMenu::GetMenuArray().DefaultButtonList.Clear();
	}
	else
	{
		if( m_pButtonList )
		{
			m_pButtonList->Clear();
		}
	}
}

void CMenuScreen::INIT_SCROLL_BAR() const
{
	INIT_SCROLL_BAR_Generic(m_ScrollBarFlags, depth);
}

void CMenuScreen::INIT_SCROLL_BAR_Generic(const atFixedBitSet16& checkThis, eDepth depth)
{
	//wrapping ternaries. A bit stupid? yes.

#define TWO_WAY_SPLIT(FlagA, ResultA, ResultB)\
	checkThis.IsSet(FlagA) ? ResultA : ResultB

#define THREE_WAY_SPLIT(FlagA, ResultA, FlagB, ResultB, ResultC)\
	TWO_WAY_SPLIT(FlagA, ResultA, TWO_WAY_SPLIT(FlagB, ResultB, ResultC))

#define FOUR_WAY_SPLIT(FlagA, ResultA, FlagB, ResultB, FlagC, ResultC, ResultD)\
	THREE_WAY_SPLIT(FlagA, ResultA, FlagB, ResultB, TWO_WAY_SPLIT(FlagC, ResultC, ResultD))

#define FIVE_WAY_SPLIT(FlagA, ResultA, FlagB, ResultB, FlagC, ResultC, FlagD, ResultD, ResultE)\
	FOUR_WAY_SPLIT(FlagA, ResultA, FlagB, ResultB, FlagC, ResultC, TWO_WAY_SPLIT(FlagD, ResultD, ResultE))

	const int iColumnWidth = FOUR_WAY_SPLIT(Width_1, 1, Width_2, 2, Width_3, 3, 0);

	// determine various bits
	int iDepth = static_cast<int>(depth-1);

	// no width specified, just bail
	if( iColumnWidth == 0 )
	{
		CScaleformMenuHelper::HIDE_COLUMN_SCROLL(static_cast<PM_COLUMNS>(iDepth));
		return;
	}

	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if( !pauseContent.BeginMethod( "INIT_COLUMN_SCROLL" ) )
		return;

	// borrowed from ActionScript
	#define SCROLL_TYPE_ALL			0
	#define SCROLL_TYPE_UP_DOWN		1
	#define SCROLL_TYPE_LEFT_RIGHT	2

	#define POSITION_ARROW_LEFT		0
	#define POSITION_ARROW_CENTER	1
	#define POSITION_ARROW_RIGHT	2


	int iArrowPos = THREE_WAY_SPLIT(Align_Left, POSITION_ARROW_LEFT, Align_Right, POSITION_ARROW_RIGHT, POSITION_ARROW_CENTER );
	int iXOffset = FIVE_WAY_SPLIT(Offset_2Left, -2, Offset_1Left, -1, Offset_1Right, 1, Offset_2Right, 2, 0);

	// determine scroll type (up/down by default, so we don't actually need to check that flag,
	// except to determine if we want all directions)
	int iScrollType = SCROLL_TYPE_UP_DOWN;
	if( checkThis.IsSet(Arrows_LeftRight) )
	{
		iScrollType = SCROLL_TYPE_LEFT_RIGHT;
		if( checkThis.IsSet(Arrows_UpDown) )	
			iScrollType = SCROLL_TYPE_ALL;
	}

	pauseContent.AddParam(iDepth); // base column
	pauseContent.AddParam(checkThis.IsSet(InitiallyVisible)); // displays on focus
	pauseContent.AddParam(iColumnWidth); // width
	pauseContent.AddParam(iScrollType); // scroll type
	pauseContent.AddParam(iArrowPos); // arrow position
	pauseContent.AddParam(checkThis.IsSet(ManualUpdate)); // override
	pauseContent.AddParam(iXOffset); // xOffset

	pauseContent.EndMethod();

	if(checkThis.IsSet(InitiallyInvisible))
		CScaleformMenuHelper::HIDE_COLUMN_SCROLL(static_cast<PM_COLUMNS>(iDepth));
}

//////////////////////////////////////////////////////////////////////////

CContextMenu* CMenuScreen::GetContextMenu()
{
	// our dynamic menu might have something to say about this
	if( HasDynamicMenu() )
		return GetDynamicMenu()->GetContextMenu();

	return m_pContextMenu;
}

bool CMenuScreen::HandleContextMenuTriggers(MenuScreenId MenuTriggered, s32 iUniqueId)
{
	CContextMenu* pContextMenu = GetContextMenu();

	// no context menu, don't bother doing more work
	if( !pContextMenu || !pContextMenu->HasOptions() )
		return false;

	CContextMenu& contextMenu = *pContextMenu;

	if( uiVerifyf(HasDynamicMenu(), "If you want your context menu to work, you'll need a custom type!") )
	{
		// event triggered on CONTEXT menu == Action selected!
		if( MenuTriggered == contextMenu.GetContextMenuId() )
		{
			if( contextMenu.IsOpen() )
			{
				atHashWithStringBank selectedItem(iUniqueId);
				bool bWasAccepted = GetDynamicMenu()->HandleContextOption(selectedItem);

				//const CContextMenuOption* pOption = CPauseMenu::GetMenuArray().GetContextMenuOption(selectedItem);
				if( bWasAccepted )//&& (!uiVerify(pOption) || pOption->bClosesOnAccept) ) // this was always set as true, so...
					contextMenu.CloseMenu();
				return true;
			}
		}

		// event triggered on TRIGGER menu == SHOW menu
		if( MenuTriggered == contextMenu.GetTriggerMenuId() )
		{
			if( PromptContextMenu() )
			{
				SUIContexts::Deactivate(UIATSTRINGHASH("HasContextMenu",0x52536cbf));
				return true;
			}
		}
	}

	return false;
}

bool CMenuScreen::HandleContextMenuInput(s32 eInput)
{
	if( eInput == PAD_CIRCLE 
		|| CMousePointer::IsMouseBack() )
	{
		CContextMenu* contextMenu = GetContextMenu();
		if( contextMenu && contextMenu->IsOpen() )
		{
			contextMenu->CloseMenu();
			SUIContexts::Activate(UIATSTRINGHASH("HasContextMenu",0x52536cbf));
			CPauseMenu::RedrawInstructionalButtons();
			CPauseMenu::PlayInputSound(FRONTEND_INPUT_BACK);

			CControlMgr::GetMainFrontendControl(false).DisableInputsOnBackButton(100);
			return true;
		}
	}

	return false;
}


bool CMenuScreen::PromptContextMenu()
{
	CContextMenu* pContextMenu = GetContextMenu();
	if(!pContextMenu)
		return false;

	SUICONTEXT_AUTO_PUSH();
	GetDynamicMenu()->BuildContexts();
	if( pContextMenu->ShowMenu() )
		return true;
	return false;
}

void CMenuScreen::BackOutOfWarningMessageToContextMenu()
{
	// remove the warning message
	CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();

	// bring the context menu back up.
	//PromptContextMenu();
}


//////////////////////////////////////////////////////////////////////////
const CContextMenuOption* CMenuArray::GetContextMenuOption(atHashWithStringBank contextOption) const
{
	for(int i=0; i<ContextMenuOptions.GetCount(); ++i)
	{
		if(ContextMenuOptions[i].contextOption == contextOption)
		{
			return &ContextMenuOptions[i];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
DynamicPauseMenu::DynamicPauseMenu()
	: m_iLastNumberOfInstructionButtonsDrawn(0)
	, m_uLastButtonHash(0)
	, m_useMiddleAvatarBG(false)
	, m_showAvatarBG(false)
	, m_avatarBGAlpha(255)
	, m_TimeDir(TW_Normal)
	, m_TimeWarper(TIME_WARP_LERP_SPEED)
	, m_EffectDir(SE_Intro)
	, m_fAlphaFaderForMenuPed(1.0f)
	, m_showLocalAvatar(false)
	, m_eAudioTransitionStage(ATS_Done)
	, m_uTimeStartedScaleEffect(0u)
#if __BANK
	, m_pGroup(NULL)
#endif
#if RSG_PC
	, m_LastInputKeyboard(false)
#endif
{
	m_pErrorMessage = rage_new CWarningMessage;
	// script uses this mostly all the time by default, so we'll set it up this way.
	formatf(m_szTimerMessage, GetTimerMessageSize(), TheText.Get("FM_COR_AUTOPM"));

	m_localPlayerMenuPed.SetSlot(1);
	m_localPlayerMenuPed.SetControlsBG(false);
}

DynamicPauseMenu::~DynamicPauseMenu()
{
	m_NetAction.Cancel();

	delete m_pErrorMessage;
	m_TimeWarper.Reset(TW_Normal);

#if __BANK
	if( m_pGroup )
		m_pGroup->Destroy();
	m_pGroup = NULL;
	if( s_pDbgWarningData )
		delete s_pDbgWarningData;
	s_pDbgWarningData = NULL;
#endif
}

void DynamicPauseMenu::Restart()
{
	m_iLastNumberOfInstructionButtonsDrawn = 0;
	m_uLastButtonHash = 0;
	m_useMiddleAvatarBG = false;
	m_showAvatarBG = false;
	m_avatarBGAlpha = 255;
}

void DynamicPauseMenu::Update()
{
	m_pErrorMessage->Update();

	// update after the above to buffer out frame blips
	m_NetAction.Update(m_pErrorMessage);

	float fAlphaFader, fIgnored;
	if( GetScaleEffectPercent(fAlphaFader, fIgnored, fIgnored) )
	{
		// we've reached full effect
		datCallback oldCB = m_EffectCompleteCB;
		m_EffectCompleteCB.Call();

		// callback may change this, so check if that's still the case
		if( oldCB == m_EffectCompleteCB )
			m_EffectCompleteCB = NullCB;
	}
	m_fAlphaFaderForMenuPed = fAlphaFader;

	m_TimeWarper.SetTargetTimeWarp(m_TimeDir);

	UpdateAudioTransition();

	#if RSG_PC
	// detect if we need to cycle instructional buttons
	if( m_LastInputKeyboard != CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource() )
	{
		m_LastInputKeyboard = CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource();
		CPauseMenu::RedrawInstructionalButtons();
	}
	#endif
}

void DynamicPauseMenu::SetAvatarBGAlpha(float alpha)
{
	m_avatarBGAlpha = (u8)(alpha * 255);
}

void DynamicPauseMenu::StartAudioTransition(bool bShouldBeQueued)
{
	m_eAudioTransitionStage = bShouldBeQueued ? ATS_WaitingToBeSuppressed : ATS_ReadyToGo;
}

void DynamicPauseMenu::UpdateAudioTransition()
{
	static int scriptTimeout = 0;
	switch( m_eAudioTransitionStage )
	{
		case ATS_WaitingToBeSuppressed:
			if( !CScriptHud::bSuppressPauseMenuRenderThisFrame )
			{
				uiDisplayf("PAUSEMENU: Waiting for SUPPRESS_FRONTEND_RENDERING_THIS_FRAME to be called");
				if( ++scriptTimeout >= 2 ) // typical wait times have been ZERO frames, so 2 is pretty generous
				{
					m_eAudioTransitionStage = ATS_ReadyToGo;
					uiDisplayf("PAUSEMENU: Bailing because it seems that script will NEVER call it and we don't wanna be stuck");
				}
			}
			else
			{
				m_eAudioTransitionStage = ATS_WaitingToStopBeingSuppressed;
			}
			break;

		case ATS_WaitingToStopBeingSuppressed:
			if( CScriptHud::bSuppressPauseMenuRenderThisFrame )
			{
				uiDisplayf("PAUSEMENU: Waiting for SUPPRESS_FRONTEND_RENDERING_THIS_FRAME to stop being called");
				break;
			}
			// fallthru on purpose, so we can reuse the state below once we're not suppressed

		case ATS_ReadyToGo:
			g_FrontendAudioEntity.StopPauseMenuFirstHalf();
			g_FrontendAudioEntity.StartPauseMenuSecondHalf();
			StartScaleEffect(SE_Intro, datCallback( MFA( DynamicPauseMenu::IntroComplete), this));

			scriptTimeout = 0;
			m_eAudioTransitionStage = ATS_Done;
			// fallthru on purpose
		case ATS_Done:
			break;
	}
}

void DynamicPauseMenu::IntroComplete()
{
	StartScaleEffect(DynamicPauseMenu::SE_IntroBlips);
}

void DynamicPauseMenu::UpdateMenuPed()
{
	m_menuPed.Update(m_fAlphaFaderForMenuPed);

	m_localPlayerMenuPed.SetVisible(m_menuPed.IsVisible() && m_menuPed.HasPed() && m_showLocalAvatar);
	m_localPlayerMenuPed.Update(m_fAlphaFaderForMenuPed);
}

void DynamicPauseMenu::StartScaleEffect(SE_Dir whichWay, datCallback CB /* = NullCB */)
{
	m_EffectDir = whichWay;
	m_EffectCompleteCB = CB;
	m_uTimeStartedScaleEffect = fwTimer::GetSystemTimeInMilliseconds();
}

bool DynamicPauseMenu::GetScaleEffectPercent( float& fAlphaFader, float& fBlipAlphaFader, float& fSizeScalar )
{
	const u32 now = fwTimer::GetSystemTimeInMilliseconds();

	const ScaleEffectTunable& tuneValue = s_EffectSettings[m_EffectDir];
	const u32 fAdjTime = m_eAudioTransitionStage == ATS_Done ? m_uTimeStartedScaleEffect : now; // never do a transition if we're not done

	const float fTime = Clamp(float(now - fAdjTime)/float(tuneValue.EffectDuration),0.0f, 1.0f);
	const float AdjT =  Clamp(rage::TweenStar::ComputeEasedValue(fTime, static_cast<rage::TweenStar::EaseType>(tuneValue.eLerpMethod) ), 0.0f, 1.0f);

	fAlphaFader		= Lerp(AdjT, tuneValue.fAlphaStart,		tuneValue.fAlphaEnd );
	fBlipAlphaFader = Lerp(AdjT, tuneValue.fBlipAlphaStart, tuneValue.fBlipAlphaEnd );
	fSizeScalar		= Lerp(AdjT, tuneValue.fScalarStart,	tuneValue.fScalarEnd);

	return AdjT >= 1.0f;
}

void DynamicPauseMenu::Render()
{

}

void DynamicPauseMenu::RenderBGLayer(const PauseMenuRenderDataExtra& renderData)
{
	if(m_showAvatarBG && !renderData.bHideMenu)
	{
		CRGBA color = CHudColour::GetRGBF(HUD_COLOUR_PAUSE_BG, renderData.fAlphaFader);
		if(!CPauseMenu::IsNavigatingContent())
		{
			color = color.MultiplyAlpha( CHudColour::GetRGBA(HUD_COLOUR_PAUSE_DESELECT).GetAlpha() );
		}

		color.SetAlpha((int)(color.GetAlphaf() * m_avatarBGAlpha));

		const SGeneralPauseDataConfig* pData = NULL;

		if(m_useMiddleAvatarBG)
		{
			pData = CPauseMenu::GetMenuArray().GeneralData.MovieSettings.Access("MIDDLE_AVATAR_BG");
		}
		else
		{
			pData = CPauseMenu::GetMenuArray().GeneralData.MovieSettings.Access("RIGHT_AVATAR_BG");
		}

		if(pData)
		{
			Vector2 vPos ( pData->vPos  );
			Vector2 vSize( pData->vSize );

#if SUPPORT_MULTI_MONITOR
			if(GRCDEVICE.GetMonitorConfig().isMultihead())
			{
				float fOneThird = (1.0f/3.0f);
				vSize.x *= fOneThird;
				vPos.x = (vPos.x * fOneThird) + fOneThird;
			}
#endif // SUPPORT_MULTI_MONITOR

			CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(pData->HAlign), &vPos, &vSize WIN32PC_ONLY(, true));
			CScaleformMgr::ScalePosAndSize(vPos,vSize, renderData.fSizeScalar);

			CSprite2d::DrawRect(fwRect(vPos.x, vPos.y, vPos.x+vSize.x, vPos.y+vSize.y), color);
		}

	}
}

//////////////////////////////////////////////////////////////////////////
NetAction::NetAction()
	: m_MessagePartialHash(0)
	, m_Op(NetworkClan::OP_NONE)
{
	m_NetworkClanDelegate.Bind(this, &NetAction::OnEvent_NetworkClan);
	NETWORK_CLAN_INST.AddDelegate(&m_NetworkClanDelegate);
}
NetAction::~NetAction()
{
	NETWORK_CLAN_INST.RemoveDelegate(&m_NetworkClanDelegate);
}


void NetAction::Cancel(bool bResetToo)
{
	if( m_Status.NowStatus.Pending() && m_Op==NetworkClan::OP_NONE )
	{
		uiDisplayf("Had to cancel our last Net Action because you pressed Leave!");
		rlClan::Cancel( &m_Status.NowStatus );
	}

	m_Op = NetworkClan::OP_NONE;

	if( bResetToo )
	{
		m_Status.Reset();
	}
}

bool NetAction::IsDirty()
{
	return m_Status.CheckDirty();
}

netStatus* NetAction::Set(const char* primaryMessage, const char* dataOne /* = NULL */, const char* dataTwo /* = NULL */, datCallback onCompleteCB /* = NullCB */, rlClanErrorCodes acceptableError /* = RL_CLAN_SUCCESS*/)
{
	Cancel();

	m_MessagePartialHash = atPartialStringHash(primaryMessage);
	if( dataOne )
		m_First = dataOne;
	else
		m_First.Reset();

	if( dataTwo )
		m_Second = dataTwo;
	else
		m_Second.Reset();

	m_CompleteCB = onCompleteCB;
	m_acceptableError = acceptableError;

	return GetStatus();
}

void NetAction::SetExternal(NetworkClan::eNetworkClanOps externalOp, const char* primaryMessage, const char* dataOne /* = NULL */, const char* dataTwo /* = NULL */, datCallback onCompleteCB /* = NullCB */, rlClanErrorCodes acceptableError /* = RL_CLAN_SUCCESS*/)
{
	Cancel();
	m_Op = externalOp;
	m_Status.NowStatus.SetPending(); // lie, since I don't think we get a confirmation of starting

	m_MessagePartialHash = atPartialStringHash(primaryMessage);
	if( dataOne )
		m_First = dataOne;
	else
		m_First.Reset();

	if( dataTwo )
		m_Second = dataTwo;
	else
		m_Second.Reset();

	m_CompleteCB = onCompleteCB;
	m_acceptableError = acceptableError;
}

void NetAction::Update(CWarningMessage* pErrorMessage)
{

	if( !IsDirty() )
		return;

	CWarningMessage::Data messageData;
	//messageData.m_TextLabelBody = m_Message;
	messageData.m_FirstSubString.Set( m_First );
	messageData.m_SecondSubString.Set( m_Second );
	switch( m_Status.GetStatus()->GetStatus() )
	{
	case netStatus::NET_STATUS_SUCCEEDED:
		OnSuccess(pErrorMessage);
		break;

	case netStatus::NET_STATUS_PENDING:
		messageData.m_TextLabelBody.SetHash( atStringHash("PROGRESS", m_MessagePartialHash));
		messageData.m_iFlags = FE_WARNING_SPINNER;
//		messageData.m_declinePressed = datCallback(MFA2(NetAction::Cancel), this, NULL);
//		messageData.m_iFlags = FE_WARNING_CANCEL;
		pErrorMessage->SetMessage(messageData);
		break;

	case netStatus::NET_STATUS_FAILED:
		{
			if(m_acceptableError == m_Status.GetStatus()->GetResultCode())
			{
				OnSuccess(pErrorMessage);
			}
			else
			{
				messageData.m_TextLabelBody = NetworkClan::GetErrorName(*m_Status.GetStatus());

				// tiny kludge. Promote it to more informative message if possible.
				if( messageData.m_TextLabelBody.GetHash() == ATSTRINGHASH("RL_CLAN_UNKNOWN",0x7769bd50) )
				{
					messageData.m_TextLabelBody = "RL_CLAN_UNKNOWN_EXT";
					messageData.m_NumberToInsert = m_Status.GetStatus()->GetResultCode();
					messageData.m_bInsertNumber = true;
				}

				messageData.m_TextLabelHeading = "MO_SC_ERR_HEAD";

				messageData.m_bCloseAfterPress = true;
				messageData.m_iFlags = FE_WARNING_OK;
				pErrorMessage->SetMessage(messageData);
			}
		}
		break;

		//case netStatus::NET_STATUS_NONE:
	default:
		// not doing anything, I just want the compiler to shut up
	case netStatus::NET_STATUS_CANCELED:
		pErrorMessage->Clear();
		return;
	}
}

void NetAction::OnSuccess(CWarningMessage* pErrorMessage)
{
	CWarningMessage::Data messageData;
	//messageData.m_TextLabelBody = m_Message;
	messageData.m_FirstSubString.Set( m_First );
	messageData.m_SecondSubString.Set( m_Second );

	messageData.m_TextLabelBody.SetHash( atStringHash("SUCCESS", m_MessagePartialHash));
	messageData.m_bCloseAfterPress = true;
	messageData.m_iFlags = FE_WARNING_OK;

	pErrorMessage->SetMessage(messageData);

	m_CompleteCB.Call();
	m_CompleteCB = NullCB;
}

void NetAction::OnEvent_NetworkClan(const NetworkClan::NetworkClanEvent& evt)
{
	if(evt.m_opType == m_Op)
	{
		m_Status.NowStatus = evt.m_statusRef;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::Init
// PURPOSE:	initialises the pause menu
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		uiAssertf(!sm_bActive, "PauseMenu: Already active!");

		CMenuScreen::RegisterTypes();

		sm_bActive = false;
		sm_RenderData.Reset();

		InitGlobalFrontendSprites();

#if __BANK
		s_bDebugAlwaysReloadXML = PARAM_alwaysReloadPauseMenuXML.Get();
#if __PRELOAD_PAUSE_MENU
		s_bDebugResidentPauseMenu = !PARAM_noResidentPauseMenu.Get();
#endif
#endif


		REGISTER_FRONTEND_XML(CPauseMenu::HandlePersistentXML, "PauseMenu");
		CProfileSettings::GetInstance().SetReadCallback(MakeFunctor(&CPauseMenu::UpdateMenuOptionsFromProfile));

#if RSG_ORBIS
		InitOrbisSafeZoneDelegate();
#endif
		SetDefaultValues();

		// update options based on profile and then do the same in the other direction to make sure they are consistent
		UpdateMenuOptionsFromProfile();

		CMapMenu::RemoveLegend();

		PauseMenuPostFXManager::ClassInit();

		SetupMenuStructureFromXML(INIT_CORE);

		sm_QueuedOpenParams.Reset();

		SReportMenu::Instantiate();

		sm_languageFromSystemLanguage = GetLanguageFromSystemLanguage();

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		if (PARAM_useManualLoadMenuToExportSavegame.Get())
		{
			sm_bUseManualLoadMenuToExportSavegame = true;
		}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		if (PARAM_useManualSaveMenuToImportSavegame.Get())
		{
			sm_bUseManualSaveMenuToImportSavegame = true;
		}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	}
	else if(initMode == INIT_BEFORE_MAP_LOADED)
	{
		CMenuScreen::RegisterTypes();
		UpdateProfileFromMenuOptions();
	}
	else if(initMode == INIT_SESSION)
	{
		sm_uLastResumeTime = 0;
		sm_uTimeInputsDisabledFromClose = 0;
#if __PRELOAD_PAUSE_MENU
		sm_iPreloadingAssetsPhase = PMLP_FIRST;
#endif
	}
	sm_time.Reset();
}

void CPauseMenu::HandlePersistentXML(parTreeNode* pNodeRead)
{
	parTreeNode* pPersistentNode = pNodeRead->FindChildWithName(PERSISTENT_DATA);

	sysMemUseMemoryBucket b(MEMBUCKET_UI);

	if( uiVerifyf( pPersistentNode, "Unable to find expected data! Do you have PauseMenu/" PERSISTENT_DATA " in your Frontend.xml?" ) )
	{
		uiVerifyf( PARSER.LoadObject(pPersistentNode, sm_PersistentData), "Unable to parse expected data! Do you have " PERSISTENT_DATA " in your Frontend.xml?");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::Shutdown
// PURPOSE:	shuts down the pause menu
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		SReportMenu::Destroy();

		CMapMenu::RemoveLegend();

		ShutdownGlobalFrontendSprites();

		PauseMenuPostFXManager::ClassShutdown();

#if __BANK
		ShutdownWidgets();
#endif // __BANK

		CProfileSettings::ShutdownClass();
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
		if(IsActive())
		{
			// Force close Pause Menu immediately
			Close();
			CloseInternal();
			CloseInternal2();
		}

		// Clear all movie wrappers immediately
		RemoveAllMovies();

		SetPauseRenderPhases(false);

		sm_TogglePauseRPOwner = OWNER_OVERRIDE;
		sm_EnableTogglePauseRenderPhases = true;
		sm_PauseRenderPhasesStatus = false;
#if RSG_PC || RSG_DURANGO
		sm_GPUCountdownToPause = -1;
#endif

	}
}

void DynamicPauseMenu::AddRequiredMovie(const CMenuScreen& menuData)
{
	if( menuData.cGfxFilename.length() == 0 )
		return;

	// do wish this was typedef'd
	::rage::atBinaryMap<SGeneralPauseDataConfig,::rage::atHashWithStringBank >& rMovieSettings = CPauseMenu::GetMenuArray().GeneralData.MovieSettings;

	SGeneralPauseDataConfig* pConfigData = rMovieSettings.Access(menuData.cGfxFilename.c_str());
	if( !pConfigData )
		return;

	for(int iComp = 0; iComp < pConfigData->RequiredMovies.GetCount(); ++iComp)
	{
		const atString* const pCurFilename = &pConfigData->RequiredMovies[iComp];
		{
			const SGeneralPauseDataConfig* pNewMoviedata = rMovieSettings.Access( pCurFilename->c_str() );
			u8 sortValue = pNewMoviedata ? pNewMoviedata->LoadingOrder : 255;

			atHashString stringHash(*pCurFilename);

			// insertion sort based on priority of movies
			int iInsertionCandidate = m_OptionalSharedComponents.GetCount();

			for(int iIndex=0; iIndex < m_OptionalSharedComponents.GetCount(); ++iIndex)
			{
				atHashString curEltHash( *(m_OptionalSharedComponents[iIndex]) );
				if( stringHash == curEltHash )
				{
					uiSpew1( "SHAREDCOMPONENT: \"%s\" already exists, not adding it!", pCurFilename->c_str());
					iInsertionCandidate = -1;
					break;
				}

				const SGeneralPauseDataConfig* pCheckData = rMovieSettings.Access( curEltHash );
				if( pCheckData && sortValue < pCheckData->LoadingOrder )
				{
					uiSpew1( "SHAREDCOMPONENT: \"%s\" wants to be inserted at %i", pCurFilename->c_str(), iIndex);
					iInsertionCandidate = iIndex;
					break;
				}
			}

			if( iInsertionCandidate != -1 )
			{
				if( iInsertionCandidate == m_OptionalSharedComponents.GetCount())
				{
					uiSpew1( "SHAREDCOMPONENT: Array was empty or adding at the end! Makin' it bigger to accomodate %s!", pCurFilename->c_str());
					m_OptionalSharedComponents.PushAndGrow(pCurFilename);
				}
				else
				{
					uiSpew1( "SHAREDCOMPONENT: \"%s\" inserted at %i", pCurFilename->c_str(), iInsertionCandidate);
					m_OptionalSharedComponents.insert( &m_OptionalSharedComponents[iInsertionCandidate], 1, pCurFilename);
				}
			}
		}
	}

#if !__NO_OUTPUT
	for(int i=0; i < m_OptionalSharedComponents.GetCount() ;++i)
		uiSpew1("SHAREDCOMPONENT: #%i: %s", i, m_OptionalSharedComponents[i]->c_str());
#endif
}

void CPauseMenu::RecurseRequiredMovies(CMenuScreen& screenData, atArray<MenuScreenId>* aCycleCheck)
{
	// create any dynamic menus as necessary
	screenData.CreateDynamicMenu();


	if(screenData.HasDynamicMenu())
	{
		CMenuBase* pMenu = screenData.GetDynamicMenu();
#if __BANK
		// add/re-add any bank widgets
		bkBank* bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);
		if( s_bPauseMenuBankCreated && bank )
		{
			pMenu->AddWidgets(bank);
		}
#endif

		pMenu->Init();
		// if a menu isn't done loading, add it to our list to query later.
		// this is probably EXCEPTIONALLY rare
		if( !pMenu->IsDoneLoading() )
			GetDynamicPauseMenu()->GetNotReadyList().PushAndGrow(pMenu,4);
	}

#if __BANK
	// cycle check
	if( aCycleCheck->Find(screenData.MenuScreen) != -1)
	{
		atString path;
		for(int i=0;i<aCycleCheck->GetCount();++i)
		{
			path += (*aCycleCheck)[i].GetParserName();
			path += " -> ";
		}
		path += screenData.MenuScreen.GetParserName();
		uiAssertf(0, "Link/Incept/Reference Loop detected in %s! Gotta bail or we'll loop forever!", path.c_str() );
		return;
	}

	aCycleCheck->PushAndGrow(screenData.MenuScreen);
#endif

	// establish its movie
	GetDynamicPauseMenu()->AddRequiredMovie(screenData);

	// now spider this items' menuceptions and references
	for(int i=0; i < screenData.MenuItems.GetCount();++i)
	{
		CMenuItem& rCurItem = screenData.MenuItems[i];
		// no ID? bail
		if( rCurItem.MenuUniqueId == MENU_UNIQUE_ID_INVALID )
			continue;

		// improper type? bail
		if(    rCurItem.MenuAction != MENU_OPTION_ACTION_LINK
			&& rCurItem.MenuAction != MENU_OPTION_ACTION_INCEPT
			&& rCurItem.MenuAction != MENU_OPTION_ACTION_REFERENCE )
			continue;

		// invalid for this mode?
		if( !rCurItem.CanShow() )
			continue;

		// no valid data?
		MenuArrayIndex iItem = GetActualScreen(rCurItem.MenuUniqueId);
		if( iItem == MENU_UNIQUE_ID_INVALID)
			continue;

		CMenuScreen& screenData = GetScreenDataByIndex(iItem);

		RecurseRequiredMovies(screenData, aCycleCheck);
	}

#if __BANK
	aCycleCheck->Pop();
#endif
}

void CPauseMenu::SetupMenuContexts()
{
	sysMemUseMemoryBucket b(MEMBUCKET_UI);

	SUIContexts::ClearAll();
	SUIContexts::Activate(PM_PLATFORM_ID);
	SUIContexts::Activate(RSG_CONFIGURATION);
	SUIContexts::SetActive(UIATSTRINGHASH("HEADPHONE_ENABLED", 0x82B920A8), AreHeadphonesEnabled());
#if RSG_PC
	if(!m_bSetStereo)
	{
		m_bIsStereoPossible = GRCDEVICE.StereoIsPossible();
		m_bSetStereo = true;
	}
	SUIContexts::SetActive("VID_STEREO", m_bIsStereoPossible);	
	
	SUIContexts::SetActive("StreamingBuild", PARAM_streamingbuild.Get());

	bool bNvidiaCard = GRCDEVICE.GetManufacturer() == NVIDIA;
	bool bSupportTXAA = false;
#if USE_NV_TXAA
	bSupportTXAA = GRCDEVICE.GetTXAASupported();
#endif // USE_NV_TXAA
	SUIContexts::SetActive(ATSTRINGHASH("GFX_SUPPORT_TXAA", 0x210E0739), bNvidiaCard && bSupportTXAA);
#endif // RSG_PC

	ORBIS_ONLY(SUIContexts::SetActive(ATSTRINGHASH("PADSPEAKER_ENABLED", 0x1C37A4AC), ((audMixerDeviceOrbis*)audDriver::GetMixer())->IsPadSpeakerEnabled()));

	SUIContexts::SetActive(UIATSTRINGHASH("SS_SUPPORTED",0xdbb2ce09), audDriver::GetHardwareOutputMode() >= AUD_OUTPUT_5_1 PS3_ONLY(&& !audNorthAudioEngine::IsWirelessHeadsetConnected()));
	if(CControlMgr::GetPlayerMappingControl().IsUsingRemotePlay())
		SUIContexts::Activate("RemotePlay");
	if(CScriptHud::bUsingMissionCreator)
		SUIContexts::Activate("Creator");
#if GTA_REPLAY
	SUIContexts::Activate("ReplayEditor");
#endif

#if LIGHT_EFFECTS_SUPPORT && RSG_PC
	if(CControlMgr::GetPlayerMappingControl().HasLightEffectDevices())
	{
		SUIContexts::Activate("LightEffectHardware");
	}
#endif // LIGHT_EFFECTS_SUPPORT && RSG_PC

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (AllowSavegamesToBeExported())
	{
		SUIContexts::Activate(ALLOW_PROCESS_SAVEGAME_CONTEXT);
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	if (AllowSavegamesToBeImported())
	{
		SUIContexts::Activate(ALLOW_IMPORT_SAVEGAME_CONTEXT);
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_SP_CREDITS	
	int cl_display = Tunables::GetInstance().TryAccess(BASE_GLOBALS_HASH, ATSTRINGHASH("CREDITS_AND_LEGAL_DISPLAY_STATE", 0xAFD69144), CREDITS_AND_LEGAL_SHOW_BOTH);

#if	!RSG_FINAL
	PARAM_overrideCreditsDisplay.Get(cl_display);
#endif // !__FINAL

	switch (cl_display)
	{
	case CREDITS_AND_LEGAL_SHOW_BOTH:
		SUIContexts::Activate(CREDITS_ENABLED_CONTEXT);
		SUIContexts::Activate(LEGAL_ENABLED_CONTEXT);
		break;
	case CREDITS_AND_LEGAL_SHOW_CREDITS:
		SUIContexts::Activate(CREDITS_ENABLED_CONTEXT);
		SUIContexts::Deactivate(LEGAL_ENABLED_CONTEXT);
		break;
	case CREDITS_AND_LEGAL_SHOW_LEGAL:
		SUIContexts::Deactivate(CREDITS_ENABLED_CONTEXT);
		SUIContexts::Activate(LEGAL_ENABLED_CONTEXT);
		break;
	default:
		SUIContexts::Deactivate(CREDITS_ENABLED_CONTEXT);
		SUIContexts::Deactivate(LEGAL_ENABLED_CONTEXT);
		break;
	}
#endif // __ALLOW_SP_CREDITS


}

bool CPauseMenu::SetCurrentMenuVersion(eFRONTEND_MENU_VERSION newVersion, bool bFailGently)
{
	SetupMenuContexts();

	// Set up wireless headset context
	PS3_ONLY(CPauseMenu::HandleWirelessHeadsetContextChange());

	// destroy all the dynamic menus we may have created previously
	DestroyAllDynamicMenus();

#if __BANK
	sm_pDynamicPause->AddWidgets(BANKMGR.FindBank(UI_DEBUG_BANK_NAME));
#endif
	ActivateSocialClubContext();

	sm_pCurrentMenuVersion = NULL;
	atArray<CMenuVersion>& menuVersions = GetMenuArray().MenuVersions;
	for(int i=0; i < menuVersions.GetCount(); ++i)
	{
		if(menuVersions[i].GetId().GetHash() == newVersion)
		{
			sm_pCurrentMenuVersion = &menuVersions[i];
			sm_iCurrentMenuVersion = newVersion;
#if __BANK
			s_pszCurrentMenuVersion.SetHash(sm_iCurrentMenuVersion);
#endif

			SUIContexts::PushThisList( sm_pCurrentMenuVersion->GetInitialContexts() );

			Displayf("=============================================================================================");
			Displayf("         Pause Menu current version is: %s, 0x%08x", atHashString(sm_pCurrentMenuVersion->GetId()).TryGetCStr(), sm_iCurrentMenuVersion);
			Displayf("=============================================================================================");

			// set up any extra necessary streamed movies
			const MenuScreenId& iUniqueInitialScreenId = GetInitialScreen();
			CMenuScreen& menuData = GetScreenData(iUniqueInitialScreenId);
			
			atArray<MenuScreenId>* pCycleCheck = NULL;
#if __BANK
			atArray<MenuScreenId> aCycleCheck;
			pCycleCheck = &aCycleCheck;
#endif

			RecurseRequiredMovies(menuData, pCycleCheck);

#if __BANK
			uiAssertf(aCycleCheck.GetCount() == 0, "Not sure how, but we came out of making %s with an unbalanced movie list!", sm_pCurrentMenuVersion->GetId().TryGetCStr());
#endif
			if( GetCurrentMenuVersionHasFlag(kCanHide) )
				SUIContexts::Activate(UIATSTRINGHASH("CAN_TOGGLE_MENU",0x7712035c));
			return true;
		}
	}

	if( bFailGently )
	{
		uiErrorf("Unable to find a MenuVersion with an ID of %i, 0x%08x. Going to let you off with a warning this time.", newVersion, newVersion);
	}
	else
	{
		Quitf(ERR_GUI_MENU_VER,"Unable to find a MenuVersion with an ID of %i, 0x%08x. It is not safe to continue. Please check PauseMenu.XML and your ID to verify it exists.", newVersion, newVersion);
	}
	
	return false;
}

MenuScreenId CPauseMenu::GetCurrentScreen()
{
	if( sm_aMenuState.empty() )
		return MENU_UNIQUE_ID_INVALID;

	return sm_aMenuState.Top().currentScreen;
}

MenuScreenId CPauseMenu::GetCurrentActivePanel()
{
	if( sm_aMenuState.empty() )
		return MENU_UNIQUE_ID_INVALID;

	return sm_aMenuState.Top().currentActivePanel;
}

bool CPauseMenu::IsCurrentScreenValid()
{
	if( sm_aMenuState.empty() )
		return false;

	return GetCurrentScreen() != MENU_UNIQUE_ID_INVALID;
}


MenuScreenId CPauseMenu::GetMenuceptionForceFocus()
{
	if( sm_aMenuState.empty() )
		return MENU_UNIQUE_ID_INVALID;

	return sm_aMenuState.Top().forceFocus;
}


void CPauseMenu::SetCurrentScreen(MenuScreenId iUniqueScreen)
{
	// if we're resetting the menu depth, don't push any new ones
	if( sm_aMenuState.empty() )
	{
		if( iUniqueScreen == MENU_UNIQUE_ID_INVALID )
			return;

		sm_aMenuState.Grow(MENU_STATE_PUSH_SIZE);
	}

	// teensy kludge to ensure that losefocus is called when going back to invalid states (aka shutting down)
	if( iUniqueScreen == MENU_UNIQUE_ID_INVALID 
		&& IsCurrentScreenValid()
		&& GetCurrentScreenData().HasDynamicMenu() )
	{
		uiSpew("@@@@@ LOSEF: %s", GetCurrentScreenData().MenuScreen.GetParserName());
		GetCurrentScreenData().GetDynamicMenu()->LoseFocus();
	}

	uiSpew("Current SCREEN is now %s", iUniqueScreen.GetParserName());

	sm_aMenuState.Top().currentScreen = iUniqueScreen;
#if __BANK
	s_pszCurrentMenu = GetCurrentScreen().GetParserName();
	s_CurrentStateDebug = sm_aMenuState.Top();
#endif
}

void CPauseMenu::SetCurrentPane(MenuScreenId iUniqueScreen)
{
	if( sm_aMenuState.empty() )
	{
		if( iUniqueScreen == MENU_UNIQUE_ID_INVALID )
			return;

		sm_aMenuState.Grow(MENU_STATE_PUSH_SIZE);
	}

	uiSpew("Current PANE is now %s", iUniqueScreen.GetParserName());

	sm_aMenuState.Top().currentActivePanel = iUniqueScreen;
#if __BANK
	s_pszCurrentPanel = GetCurrentActivePanel().GetParserName();
	s_CurrentStateDebug = sm_aMenuState.Top();
#endif
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsGameInStateWhereItCanBePaused
// PURPOSE:	returns true if the game is in a state where it can be paused but
//			returns false if it cannot be paused for various reasons
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsGameInStateWhereItCanBePaused()
{
	bool bCanPause = true;

	if (CSavegameDialogScreens::IsMessageProcessing() || ( CWarningScreen::IsActive() && !CControlMgr::IsShowingControllerDisconnectMessage()) )
	{
		uiDebugf3("PauseMenu: Activation stopped.  Reason: A Message screen is active");
		bCanPause = false;
	}

#if RSG_PC
	if(STextInputBox::GetInstance().IsActive())
	{
		bCanPause = false;
	}

	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		bCanPause = false;
	}
#endif // RSG_PC

	// cannot pause when screen is faded or fading:
	if (camInterface::IsFadedOut() || camInterface::IsFading())
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: Screen is fading or faded");
		bCanPause = false;
	}

	if( g_PlayerSwitch.IsActive() )
	{
		uiDisplayf("PauseMenu: Activation stopped. Reason: Player switch is happening");
		bCanPause = false;
	}

	CPed * pPlayerPed = FindPlayerPed();
	if (pPlayerPed && (!CScriptHud::bAllowPauseWhenInNotInStateOfPlayThisFrame) && (pPlayerPed->ShouldBeDead() || pPlayerPed->GetIsArrested() || pPlayerPed->GetPlayerInfo()->IsControlsLoadingScreenDisabled()))
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: Ped is not in a state where pause is allowed");
		bCanPause = false;
	}

	if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning() && !CutSceneManager::GetInstance()->IsPlaying()))
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: Cutscene is not in a state where pause is allowed");
		bCanPause = false;
	}

	if (CCredits::IsRunning())
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: Credits are rolling");
		bCanPause = false;
	}

	if (CScriptHud::bDisablePauseMenuThisFrame)
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: Script have disabled pausemenu this frame");
		bCanPause = false;
	}
	
	if ( (!CGameLogic::IsGameStateInPlay()) && (!CScriptHud::bAllowPauseWhenInNotInStateOfPlayThisFrame) )
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: Game is not in a state of play");
		bCanPause = false;
	}

	if (cStoreScreenMgr::IsPendingNetworkShutdownToOpen())
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: Store Menu is in the process of opening");
		bCanPause = false;
	}

	if (cStoreScreenMgr::IsStoreMenuOpen())
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: Store Menu is Open");
		bCanPause = false;
	}

	if( sm_bClosingDownPart2 )
	{
		uiDisplayf("PauseMenu: Activation stopped.  Reason: We're currently in the process of shutting down");
		bCanPause = false;
	}

	if (bCanPause)
	{
		uiDisplayf("PauseMenu: Activation requested - menus will now stream");
	}

	return (bCanPause);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::Restart
// PURPOSE: restarts the pausemenu with new header tabs
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::Restart(eFRONTEND_MENU_VERSION iMenuVersion, eMenuScreen HighlightTab)
{
	if (!sm_bActive)
	{
		uiAssertf(0, "RESTART_FRONTEND_MENU - trying to restart the frontend menus when they are not active");
		return;
	}

	if (sm_bRestarting)
	{
		uiAssertf(0, "RESTART_FRONTEND_MENU - trying to restart the frontend menus when it is already restarting.  Only call restart once!");
		return;
	}

	if (sm_iLoadingAssetsPhase != PMLP_DONE)
	{
		uiAssertf(0, "RESTART_FRONTEND_MENU - trying to restart the frontend menus when assets are loading");
		return;
	}

	if (IsMenuControlLocked())
	{
		uiAssertf(0, "RESTART_FRONTEND_MENU - trying to restart the frontend menus when it is in a busy state");
		return;
	}

	if( DynamicMenuExists() )
	{
		atArray<CMenuVersion>& menuVersions = GetMenuArray().MenuVersions;
		bool bFoundAVersion = false;
		for(int i=0; i < menuVersions.GetCount() && !bFoundAVersion; ++i)
		{
			bFoundAVersion = (menuVersions[i].GetId().GetHash() == iMenuVersion);
		}

		if( !uiVerifyf(bFoundAVersion, "Couldn't find a restartable version for 0x%08x, so just bailing!", iMenuVersion))
			return;
	}

	if (uiVerifyf(iMenuVersion != sm_iCurrentMenuVersion, "PauseMenu: Trying to restart to a menu that is already active (%d, %s)", (s32)iMenuVersion, atHashString::TryGetString(iMenuVersion)))
	{
		if(	IsCurrentScreenValid() && GetCurrentScreenData().HasDynamicMenu() )
		{
			GetCurrentScreenData().GetDynamicMenu()->LoseFocus();
		}

		LockMenuControl(false);

		sm_bRestarting = true;
		sm_iRestartingMenuVersion = iMenuVersion;
		sm_iRestartingHighlightTab = HighlightTab;

		GetDynamicPauseMenu()->Restart();

		GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("BEGIN_RESTART_PAUSE_MENU");
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::ResetPauseRenderPhases
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::ResetPauseRenderPhases(WIN32PC_ONLY(eNUM_PAUSE_MENU_PAUSEDRP_OWNERS owner))
{
#if RSG_PC
	TogglePauseRenderPhases(true,owner, __FUNCTION__ );
#else
	TogglePauseRenderPhases(true,OWNER_OVERRIDE, __FUNCTION__ );
#endif
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::TogglePauseRenderPhases
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::TogglePauseRenderPhases(bool setRenderToUpdate, eNUM_PAUSE_MENU_PAUSEDRP_OWNERS owner, const char* OUTPUT_ONLY(caller) )
{
	//RossC: We should burn this function and salt the ground it stands on ASAP.
	//I have tried to rename the params to make it more obvious what is going on, but I think the core premise is a little off.

	if(!sm_EnableTogglePauseRenderPhases)
		return;
		
	if(!g_RenderPhaseManager::IsInstantiated())//player can attempt to system pause during startup when renderphases has not been initialized
		return;

	bool setRenderPhases = false;
	
#if RSG_PC
	eNUM_PAUSE_MENU_PAUSEDRP_OWNERS multiGPUowner = owner;
	if (owner == OWNER_SCRIPT_OVERRIDE)
	{
		// tracking down reset pause from script flro multi gpu
		multiGPUowner = OWNER_SCRIPT;
		owner = OWNER_OVERRIDE;
	}
#endif

	//Store screen and pause menu are a special case. If one has the render update disabled, the other can add to the lock.
	//When both are freed, rendering resumes. We go in here when the requesting owner is either store or pause, and the owner mask has either bit set.
	if  ( ( owner == OWNER_STORE || owner == OWNER_PAUSEMENU ) && ( sm_TogglePauseRPOwner & (OWNER_STORE | OWNER_PAUSEMENU) ) )
	{
		if ( setRenderToUpdate )
		{
			//Clear our calling owners bit
			sm_TogglePauseRPOwner &= ~owner;

			//Check if the flags both been cleared.
			if ( sm_TogglePauseRPOwner == 0 )
			{
				//Since we cleared the bits, we can toggle
				setRenderPhases = true;
			}
		}
		else
		{
			sm_TogglePauseRPOwner |= owner;
			setRenderPhases = true;
		}

	}
	else if( owner == OWNER_OVERRIDE 
		||  sm_TogglePauseRPOwner == OWNER_OVERRIDE 
		||  owner == sm_TogglePauseRPOwner )
	{
		//This is the normal way of doing things. If we are the owner, we can unpause. If there is no owner (OWNER_OVERRIDE), then anyone can pause and become the owner.
		if( setRenderToUpdate )
		{
			//This is clearing the pause, and setting the neutral state
			sm_TogglePauseRPOwner = OWNER_OVERRIDE;
		}
		else
		{
			sm_TogglePauseRPOwner = owner;
		}

		setRenderPhases = true;
	}
	
	if(!setRenderPhases)
	{

		Displayf("Failed: Function: %s called TOGGLE_PAUSED_RENDERPHASES(%s) - %d", caller ? caller : "", setRenderToUpdate ? "TRUE" : "FALSE", owner);


		return;
	}


	Displayf("Succeeded: Function: %s called TOGGLE_PAUSED_RENDERPHASES(%s) - %d", caller ? caller : "", setRenderToUpdate ? "TRUE" : "FALSE", owner);



	SetPauseRenderPhases(!setRenderToUpdate WIN32PC_ONLY(,multiGPUowner));
}

void CPauseMenu::SetPauseRenderPhases(bool bPause WIN32PC_ONLY(,eNUM_PAUSE_MENU_PAUSEDRP_OWNERS owner))
{
#if RSG_PC || RSG_DURANGO
	if (!bPause)
	{
		sm_GPUCountdownToPause = -1;
#if RSG_PC
		PostFX::DoPauseResolve(PostFX::UNRESOLVE_PAUSE);
		if(GRCDEVICE.UsingMultipleGPUs())
			PostFX::SetRequireResetDOFRenderTargets(true);
#endif
	}
	else
	{
		if ((sm_GPUCountdownToPause == -1) WIN32PC_ONLY(|| owner == OWNER_SCRIPT))
		{
			// Need to copy BackBuffer out of ESRAM before pause
#if RSG_PC
			// we will let the current frame to be rendered & saved in pause backbuffer texture
			// render thread pause will start next frame
			sm_GPUCountdownToPause = 2;
			PostFX::DoPauseResolve(PostFX::RESOLVE_PAUSE);
#else
			sm_GPUCountdownToPause = 1;
#endif
			return;
		}
#if !RSG_DURANGO
		sm_GPUCountdownToPause--;
#endif
		if (sm_GPUCountdownToPause != 0)
		{
			return;
		}
#if RSG_DURANGO
		sm_GPUCountdownToPause = -1;
#endif
	}
#endif

	sm_PauseRenderPhasesStatus = bPause;

	fwRenderPhaseManager& renderPhaseManager = g_RenderPhaseManager::GetInstance();
	s32 count = renderPhaseManager.GetRenderPhaseCount();
	for(int i = 0; i < count; i++)
	{
		const CRenderPhase* renderPhase = (const CRenderPhase*)&renderPhaseManager.GetRenderPhase(i); // All our renderphases are CRenderPhase, currently.
		bool canBeDisabled	= renderPhase->GetDisableOnPause();

		if(canBeDisabled)
		{
			renderPhaseManager.GetRenderPhase(i).SetBuildDrawListEnabled(!bPause);
		}
	}
}

#if RSG_PC
void CPauseMenu::RePauseRenderPhases()
{
	sm_GPUCountdownToPause = 2 * GRCDEVICE.GetGPUCount(true);
	PostFX::DoPauseResolve(PostFX::UNRESOLVE_PAUSE);
	SetPauseRenderPhases(true);
}
#endif

#if RSG_PC
void CPauseMenu::DecrementGPUCountDown() {
	if (sm_GPUCountdownToPause != -1 && !sm_PauseRenderPhasesStatus)
	{
		CPauseMenu::SetPauseRenderPhases(true);
	}
}
#elif RSG_DURANGO
void CPauseMenu::DecrementGPUCountDown()
{
	if (sm_GPUCountdownToPause == 0 && !sm_PauseRenderPhasesStatus)
	{
		CPauseMenu::SetPauseRenderPhases(true);
	}
}
#endif

int CPauseMenu::GetCurrentCharacter()
{
	const int rangeStart = NetworkInterface::IsInFreeMode() ? CHAR_MP_START : CHAR_SP_START;
	const int rangeEnd = NetworkInterface::IsInFreeMode() ? CHAR_MP_END : CHAR_SP_END;
	const int rangeSize = rangeEnd - rangeStart + 1;
	int iCharacter = -1;

	if(iCharacter == -1)
	{
		iCharacter = (int)StatsInterface::GetCharacterIndex() - rangeStart;
	}

	if(iCharacter < 0 || rangeSize <= iCharacter)
	{
		iCharacter = 0;
	}

	return iCharacter;
}
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::ResetAllGlobalMenuVariables
// PURPOSE: all global variables get reset to their default states here and can be
//			set to new values in whatever function calls this.  This is important
//			so when the menu is restarted, opened, closed etc, all the variables get
//			set to the correct state
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::ResetAllGlobalMenuVariables(bool bResetPed)
{
	sm_DefaultHighlightTab = MENU_UNIQUE_ID_INVALID;
	sm_bClosingDown = false;
	sm_bRenderContent = false;
	sm_bRenderMenus = false;
	sm_bMaxPayneMode = false;
	bActionScriptPopulated = false;
	bActionScriptPopulatedWithHeadshot = false;

	sm_iCurrentHighlightedTabIndex = 0;

	sm_bStoreAvailable = !IsStoreAvailable();

	sm_RenderData.Reset();

	sm_bWaitOnDisplayCalibrationScreen = false;
	sm_bDisplayCreditsScreenNextFrame = false;
	sm_bWaitOnCreditsScreen = false;

	sm_bClanTextureRequested = false;

	if (sm_PedShotHandle != 0)//(s32)PedHeadshotManager::INVALID_HANDLE)
	{
		PEDHEADSHOTMANAGER.UnregisterPed(sm_PedShotHandle);
	}
	sm_PedShotHandle = 0;//(s32)PedHeadshotManager::INVALID_HANDLE;

	sm_iStreamedMovie = NO_STREAMING_MOVIE;
	sm_iStreamingMovie = NO_STREAMING_MOVIE;

	sm_iMenuPrefSelected = PREF_INVALID;
	sm_iLastValidPref = PREF_INVALID;
	sm_bProcessedContent = false;
	sm_bRenderContent = false;
	sm_bRestarting = false;
	sm_pMsgToWarnOnTabChange = NULL;

	sm_iCodeWantsScriptToControlScreen = MenuScreenId(MENU_UNIQUE_ID_INVALID);

	sm_aMenuState.Reset();
	SetCurrentPane(MENU_UNIQUE_ID_INVALID);
	SetCurrentScreen(MENU_UNIQUE_ID_INVALID);

	sm_iLoadingAssetsPhase = PMLP_FIRST;
	sm_bRenderMenus = GetCurrentMenuVersionHasFlag(kRenderMenusInitially);
	sm_bSetupStartingPane = false;
	sm_bWaitingForFirstLayoutChanged = true;
	sm_bAwaitingMenuShiftDepthResponse = false;

	sm_bMenuTriggerEventOccurred = false;
	sm_bMenuLayoutChangedEventOccurred = false;
	sm_iMenuEventOccurredUniqueId[0] = -1;
	sm_iMenuEventOccurredUniqueId[1] = -1;
	sm_iMenuEventOccurredUniqueId[2] = 0;
#if KEYBOARD_MOUSE_SUPPORT
	sm_MouseHoverEvent = PMMouseEvent::INVALID_MOUSE_EVENT;
#endif

	if(sm_pDynamicPause && bResetPed)
	{
		if(GetDisplayPed())
		{
			GetDisplayPed()->Shutdown();
			GetDisplayPed()->Init();
		}

		if(GetLocalPlayerDisplayPed())
		{
			GetLocalPlayerDisplayPed()->Shutdown();
			GetLocalPlayerDisplayPed()->Init();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::Open
// PURPOSE:	opens the pause menu
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::Open(eFRONTEND_MENU_VERSION iMenuVersion, bool bPauseGame/*=true*/, eMenuScreen HighlightTab/*=MENU_UNIQUE_ID_INVALID*/, bool bFailGently/*=false*/)
{
	if( sm_bActive )  // do not try to open twice!
	{
		uiAssertf(iMenuVersion==sm_iCurrentMenuVersion, "Attempted to OPEN a menu of type '%s' (0x%08x) when we're already at type '%s' (0x%08x)! You'll need to call RESTART instead!", atHashString::TryGetString(iMenuVersion), iMenuVersion, atHashString::TryGetString(sm_iCurrentMenuVersion), sm_iCurrentMenuVersion);
		return;
	}

#if RSG_DURANGO
	events_durango::WriteEvent_PlayerSessionPause();
#endif

#if ALLOW_MANUAL_FRIEND_REFRESH
	// Soft refresh: does an async request for presence to load the online/session status inline
	if (NetworkInterface::HasValidRosCredentials() && rlFriendsManager::CanQueueFriendsRefresh(NetworkInterface::GetLocalGamerIndex()))
	{
		rlFriendsManager::RequestRefreshFriendsPage(NetworkInterface::GetLocalGamerIndex(), CLiveManager::GetFriendsPage(), 0, CPlayerListMenuDataPaginator::FRIEND_UI_PAGE_SIZE, true);
	}
#endif // #if ALLOW_MANUAL_FRIEND_REFRESH

	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().SetDeletePending();
	}

#if RSG_PC
	if (SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsChatTyping())
	{
		SMultiplayerChat::GetInstance().ForceAbortTextChat();
	}
#endif

	// Reset rumble timer.
	sm_EndAllowRumbleTime = 0;
	sm_iMenuSelectTimer.Start();  // start the select timer as we open the menu
	s_iSpinnerTimer.Start();

	sm_bNavigatingContent = false;

	CGameStreamMgr::GetGameStream()->Pause();
	sm_updateDelay = PM_DELAY_UPDATE;

#if RSG_PC
	// mouse pointer not visible until it is moved:
	CMousePointer::SetMouseCursorVisible(true);
	CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);
#endif // __RSG_PC

	DeleteDynamicPause();

	sm_pDynamicPause = rage_new DynamicPauseMenu();

	// handle set up, catching any failures
	if( !SetupMenuStructureFromXML(rage::INIT_SESSION) || !SetCurrentMenuVersion( iMenuVersion, bFailGently ) )
	{
		// failed to set up! sadness.
		CGameStreamMgr::GetGameStream()->HideForUpdates( PAUSE_MENU_FEED_INVISIBLE_FRAMES );
		CGameStreamMgr::GetGameStream()->Resume();
		DeleteDynamicPause();


		return;
	}
	
	if(bPauseGame)
	{
		SetupCodeForPause();
	}

	RemoveAllMovies();

	OpenLite(HighlightTab);

	SetupControllerPref();

// 	if (GetCurrentMenuVersionHasFlag(kFadesIn))
// 	{
// 		sm_iBackgroundFader = sm_PersistentData.Gradient.minAlpha;
// 	}
// 	else
// 	{
// 		sm_iBackgroundFader = sm_PersistentData.Gradient.maxAlpha;  // instant fade if scripted menus - hack for now until I can speak to Brenda
// 	}
	
	sm_MenuSprite[MENU_SPRITE_ARROW].Delete();

	RequestSprites();

	LockMenuControl(false);

	ResetMenuItemTriggered();

	sm_bActive = true;

#if RSG_ORBIS
	sm_iFriendPaneMovementInterval = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FRIEND_PANE_MOVEMENT_INTERVAL", 0x82681B4D), sm_iFriendPaneMovementTunable);
	sm_iFriendPaneMovementTunable = sm_iFriendPaneMovementInterval;
#endif

	CNewHud::ForceMultiplayerCashToReshowIfActive();

	sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_NONE;
	sm_bSaveMenuIsBeingRedrawnAfterDeletingASavegame = false;

	sm_bQueueManualLoadASAP = false;
	sm_bQueueManualSaveASAP = false;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	sm_bQueueUploadSavegameASAP = false;
	uiDisplayf("CPauseMenu::Open - setting sm_bQueueManualLoadASAP, sm_bQueueManualSaveASAP and sm_bQueueUploadSavegameASAP to false");
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	uiDisplayf("CPauseMenu::Open - setting sm_bQueueManualLoadASAP and sm_bQueueManualSaveASAP to false");
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	if (!SPlayerCardManager::IsInstantiated())
		SPlayerCardManager::Instantiate();
	SClanInviteManager::Instantiate();

	if (GetCurrentMenuVersionHasFlag(kFadesIn))
	{
		PAUSEMENUPOSTFXMGR.StartFadeIn();
	}

	// Ensure the radio station is in sync with the game
	if(g_RadioAudioEntity.GetPlayerRadioStation())
	{
		s32 stationIndex = (s32)g_RadioAudioEntity.GetPlayerRadioStation()->GetStationIndex();
		const audRadioStation *station = audRadioStation::GetStation(stationIndex);

		// If the player has somehow selected a hidden station, just default to 'off'
		if(station && (station->IsLocked() || station->IsHidden()))
		{
			stationIndex = g_OffRadioStation;
		}

#if RSG_PC		
		if(station && station->IsUserStation() && stationIndex != (s32)g_OffRadioStation && !audRadioStation::HasUserTracks())
		{
			SetMenuPreference(PREF_RADIO_STATION, -1); // make sure that if Self Radio is selected but we have no tracks we shut the radio off
		}
		else
#endif
		{
			SetMenuPreference(PREF_RADIO_STATION, stationIndex);
		}
	}
	
	bool isSpectating	= false;

	if(NetworkInterface::IsGameInProgress())
	{
		isSpectating = NetworkInterface::IsInSpectatorMode();

		if(sm_frontendMenuScene)
		{
			sm_frontendMenuScene->Stop();
			sm_frontendMenuScene = NULL;
		}

		// Only trigger MP_FRONTEND_MENU_SCENE when displaying online pause menu
		if(iMenuVersion == FE_MENU_VERSION_MP_PAUSE)
			DYNAMICMIXER.StartScene(ATSTRINGHASH("MP_FRONTEND_MENU_SCENE", 0x10d4f315), &sm_frontendMenuScene);

#if RSG_DURANGO
		// For Durango we need to fetch the display name
		if (sm_displayNameReqID != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
		{
			if (CLiveManager::GetFindDisplayName().GetRequestIndexByRequestId(sm_displayNameReqID) != CDisplayNamesFromHandles::INVALID_REQUEST_ID )
			{
				CLiveManager::GetFindDisplayName().CancelRequest(sm_displayNameReqID);
			}

			sm_displayNameReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
		}
		sm_displayNameReqID = CLiveManager::GetFindDisplayName().RequestDisplayNames(NetworkInterface::GetLocalGamerIndex(), &NetworkInterface::GetLocalGamerHandle(), 1);
#else
		// For everything else, let's immediately set our cached player name
		CNetGamePlayer* pNetPlayer = NetworkInterface::GetLocalPlayer();

		if (pNetPlayer)
		{
			safecpy(sm_displayName, pNetPlayer->GetGamerInfo().GetName(), NELEM(sm_displayName));
			SetPlayerBlipName(sm_displayName);
		}
#endif
	}

	if (isSpectating)
		SUIContexts::Activate("IsSpectating");
	else
		SUIContexts::Deactivate("IsSpectating");

#if RSG_PC
	if(audRadioStation::GetUserRadioTrackManager()->IsScanning())
	{
		if(audRadioStation::GetUserRadioTrackManager()->GetIsCompleteScanRunning())
		{
			SUIContexts::SetActive(UIATSTRINGHASH("AUD_QSCANNING",0x54348A17), false);
			SUIContexts::SetActive(UIATSTRINGHASH("AUD_FSCANNING",0x64E3F10A), true);
		}
		else
		{
			SUIContexts::SetActive(UIATSTRINGHASH("AUD_QSCANNING",0x54348A17), true);
			SUIContexts::SetActive(UIATSTRINGHASH("AUD_FSCANNING",0x64E3F10A), false);
		}
	}
	else
	{
		SUIContexts::SetActive(UIATSTRINGHASH("AUD_QSCANNING",0x54348A17), false);
		SUIContexts::SetActive(UIATSTRINGHASH("AUD_FSCANNING",0x64E3F10A), false);
	}
#endif
}

void CPauseMenu::OpenLite(MenuScreenId defaultHighlight)
{
	ResetAllGlobalMenuVariables(!GetCurrentMenuVersionHasFlag(kNoPeds));

	sm_DefaultHighlightTab = defaultHighlight;

	const MenuScreenId& iUniqueInitialScreenId = GetInitialScreen();
	const CMenuScreen& thisScreen = GetScreenData(iUniqueInitialScreenId);

	SUIContexts::SetActive("IN_CORONA_MENU", iUniqueInitialScreenId == MENU_UNIQUE_ID_HEADER_CORONA);

	for (s32 i = 0; i < thisScreen.MenuItems.GetCount(); i++)
	{
		if (thisScreen.MenuItems[i].MenuUniqueId == sm_DefaultHighlightTab)
		{
			sm_iCurrentHighlightedTabIndex = i;
			break;
		}
	}

	sm_bProcessedContent = thisScreen.MenuItems.empty();
}

void CPauseMenu::QueueOpen(eFRONTEND_MENU_VERSION iMenuVersion, bool bPauseGame, eMenuScreen HighlightTab, bool bFailGently)
{
	sm_QueuedOpenParams.Set(iMenuVersion, bPauseGame, HighlightTab, bFailGently);
}

void CPauseMenu::ToggleFullscreenMap(bool bIsOpen)
{
	static MenuScreenId prevScreen = MENU_UNIQUE_ID_INVALID;

	if (uiVerify(CPauseMenu::IsActive()))
	{
		if (bIsOpen)
		{
			if (uiVerify(GetCurrentScreen() != MENU_UNIQUE_ID_MAP))
			{
				prevScreen = GetCurrentScreen();
				CMapMenu::SetIsMaskRendering(true);		// There will always be a mask set when this is opened; make sure the map menu knows this, so it can be turned off since we're going directly into fullscreen
				MenuceptionGoDeeper(MENU_UNIQUE_ID_MAP);
				CMapMenu::SetMapZoom(ZOOM_LEVEL_START_MP, true);
			}
		}
		else
		{
			MenuceptionTheKick();
			SetCurrentScreen(prevScreen);
		}

		CMapMenu::SetHideLegend(bIsOpen);
	}
}

void CPauseMenu::DestroyAllDynamicMenus()
{
	// the actual contents of this array are pretty sparse, but it'll get us through the night
	int i = GetMenuArray().MenuScreens.GetCount();
	while( i-- > 0 ) // just in case count is negative..?
	{
		GetMenuArray().MenuScreens[i].DeleteDynamicMenu();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::Close
// PURPOSE:	closes the pause menu
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::CloseInternal()
{
	if (!sm_bActive)
	{
		return; 
	}

	if(sm_frontendMenuScene)
	{
		sm_frontendMenuScene->Stop();
		sm_frontendMenuScene = NULL;
	}


#if RSG_DURANGO
	events_durango::WriteEvent_PlayerSessionResume();
	if (sm_displayNameReqID != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
	{
		if (CLiveManager::GetFindDisplayName().GetRequestIndexByRequestId(sm_displayNameReqID) != CDisplayNamesFromHandles::INVALID_REQUEST_ID )
		{
			CLiveManager::GetFindDisplayName().CancelRequest(sm_displayNameReqID);
		}

		sm_displayNameReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
	}
#endif

	// Reset the friends to the first array
	CLiveManager::RequestNewFriendsPage(0, CLiveManager::GetFriendsPage()->m_MaxFriends);

	if(CNetwork::IsNetworkOpen() && !SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::Instantiate();
	}

	sm_bCurrentMenuNeedsFading = GetCurrentMenuVersionHasFlag(kFadesIn);// && !sm_bMaxPayneMode;
	
	CGameStreamMgr::GetGameStream()->HideForUpdates( PAUSE_MENU_FEED_INVISIBLE_FRAMES );
	CGameStreamMgr::GetGameStream()->Resume();
	CGameStreamMgr::GetGameStream()->ResetPauseMenuCounter();

	SetCurrentScreen( MENU_UNIQUE_ID_INVALID );

	SClanInviteManager::Destroy();

	//Don't mess with the transition player card.
	if (!CNetwork::GetNetworkSession().IsTransitionActive())
	{
		SPlayerCardManager::Destroy();
	}

	if (IsLoadGameOptionHighlighted())
	{
		if(!IsInLoadGamePanel())
		{
			BackOutOfSaveGameList();
		}
	}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (IsUploadSavegameOptionHighlighted())
	{
		if (!IsInUploadSavegamePanel())
		{
			BackOutOfSaveGameList();
		}
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	sm_bClosingDown = true;  // we'll want this to be true for some of the functions that are about to happen
	sm_bActive = false;

	CNewHud::ForceMultiplayerCashToReshowIfActive();

	sm_bHasFocusedMenu = false;

	//	Graeme - When a Manual Load or Save is started, we need to ensure that sm_bSaveGameListSync is reset. BackOutOfSaveGameList() already reset the flag when the player chose not to proceed with the load/save, 
	//	but until I added this, the flag was not being reset when a load or save was started.
	sm_bSaveGameListSync = false;

	sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_NONE;
	sm_bSaveMenuIsBeingRedrawnAfterDeletingASavegame = false;

	sm_bQueueManualLoadASAP = false;
	sm_bQueueManualSaveASAP = false;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	sm_bQueueUploadSavegameASAP = false;
	uiDisplayf("CPauseMenu::CloseInternal - setting sm_bSaveGameListSync, sm_bQueueManualLoadASAP, sm_bQueueManualSaveASAP and sm_bQueueUploadSavegameASAP to false");
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	uiDisplayf("CPauseMenu::CloseInternal - setting sm_bSaveGameListSync, sm_bQueueManualLoadASAP and sm_bQueueManualSaveASAP to false");
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	CBusySpinner::UnregisterInstructionalButtonMovie(GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).GetMovieID());

	RemoveAllMovies();

	if( sm_iLoadingAssetsPhase == PMLP_DONE )  // if still loading, then they haven't been set up, so don't bother removing them
	{
		RemoveSprites();
	}

	if (sm_bClanTextureRequested)
	{
		NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
		if (clanMgr.HasPrimaryClan())
		{
			const rlClanDesc* pMyClan = clanMgr.GetPrimaryClan();

			rlClanId myClanId = pMyClan->m_Id;

			if (myClanId != RL_INVALID_CLAN_ID)
			{
				clanMgr.ReleaseEmblemReference(myClanId  ASSERT_ONLY(, "CPauseMenu"));
			}
		}

		sm_bClanTextureRequested = false;
	}

	if (sm_PedShotHandle != 0)//(s32)PedHeadshotManager::INVALID_HANDLE)
	{
		PEDHEADSHOTMANAGER.UnregisterPed(sm_PedShotHandle);
		sm_PedShotHandle = 0;//(s32)PedHeadshotManager::INVALID_HANDLE;
	}

	bool bResetPed = !GetCurrentMenuVersionHasFlag(kNoPeds);

	sm_pCurrentMenuVersion = NULL;

	// update the player profile
	UpdateProfileFromMenuOptions();

#if RSG_PC
	if (DirtyGfxSettings() || DirtyAdvGfxSettings())
	{
		BackoutGraphicalChanges(false, false);
	}
#endif // RSG_PC

	if(!sm_bStartedUserPause)
	{
		DeleteDynamicPause();
	}

	SUIContexts::Deactivate("IN_CORONA_MENU");

	SUIContexts::ClearAll();

	if(sm_bWaitOnCreditsScreen)
	{
		CloseCreditsScreen();
	}

	//Reenable render phases

	TogglePauseRenderPhases(true, OWNER_PAUSEMENU, __FUNCTION__ );

	CNewHud::SetShowHelperTextAfterPauseMenuExit(true);
	CNewHud::FlushAreaDistrictStreetVehicleNames();
	CNewHud::GetReticule().Reset();
	if (CHudMarkerManager::Get())
	{
		CHudMarkerManager::Get()->UpdateMovieConfig();
	}

	ResetAllGlobalMenuVariables(bResetPed);  // ensure everything is reset again for next time
	
	sm_uLastResumeTime = fwTimer::GetSystemTimeInMilliseconds();
	sm_bClosingDown = true; // gets reset by ResetAll above
	sm_bClosingDownPart2 = true;
	sm_PauseMenuCloseDelay = 0;
	sm_PauseMenuCloseTimer.Reset();
}

void CPauseMenu::CloseInternal2(bool /*bForceIt*/)
{
#if 0
	// Wait here until everything is loaded - unless we're in MP, or we're timing out.
	if (!bForceIt 
		&& !NetworkInterface::IsGameInProgress()
		&& !gStreamingRequestList.IsPlaybackActive()
		&& sm_eClosingAction == CA_None
		&& sm_PauseMenuCloseTimer.GetTime() < 3.0f)
	{
		// We need to give the scene streamer a frame to reevaluate the scene with the render phases reenabled.
		if (++sm_PauseMenuCloseDelay < STREAMING_FRAMES)
		{
			uiDisplayf("Pause Menu needs to delay a few frames before shutting down");
			return;
		}
#if __BANK
		if( s_bAlwaysFailTimeOut )
			return;
#endif

		CStreamingBucketSet &neededBucketSet = g_SceneStreamerMgr.GetStreamingLists().GetNeededEntityList().GetBucketSet();
		BucketEntry::BucketEntryArray &neededList = neededBucketSet.GetBucketEntries();
		neededBucketSet.WaitForSorting();

		int neededListCount = neededList.GetCount();

		if (neededListCount > 0 && neededList[neededListCount-1].m_Score > 1.001f)
		{
			// We're still waiting for some important assets.
			uiDisplayf("Pause Menu is waiting for streaming to resume, score is: %0.2f", neededList[neededListCount-1].m_Score);
			return;
		}
	}
#endif

	if (sm_bCurrentMenuNeedsFading)
	{
		PAUSEMENUPOSTFXMGR.StartFadeOut();
	}


	CControlMgr::GetMainPlayerControl(false).DisableAllInputs(sm_uDisableInputDuration, ioValue::DisableOptions(ioValue::DisableOptions::F_ALLOW_SUSTAINED));
	CControlMgr::GetMainPlayerControl(false).DisableInputsOnBackButton();
	sm_uTimeInputsDisabledFromClose = fwTimer::GetTimeInMilliseconds();

	CheckWhatToDoWhenClosed();
#if __PRELOAD_PAUSE_MENU
	sm_iPreloadingAssetsPhase = PMLP_FIRST;
#endif

	// this should be the final flag that is cleared
	sm_bClosingDownPart2 = false;
	sm_bClosingDown = false;
}

void CPauseMenu::DeleteDynamicPause()
{
	delete sm_pDynamicPause;
	sm_pDynamicPause = NULL;
	DestroyAllDynamicMenus();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::CheckWhatToDoWhenClosed
// PURPOSE:	once the pause menu has closed, we may of flagged to then do something
//			else (like start new game, load a save etc), so we trigger that here
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::CheckWhatToDoWhenClosed()
{
	switch(sm_eClosingAction)
	{
		case CA_StartNewGame:
		{
			audNorthAudioEngine::NotifyStartNewGame();

			camInterface::FadeOut(0);  // fade out the screen
			CGameStreamMgr::GetGameStream()->ForceFlushQueue();

			CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());
			break;
		}

		case CA_Warp:
		//if (!camInterface::IsFading())
		{	
			gVpMan.PushViewport(gVpMan.GetGameViewport()); // so that tyhe streaming knows what viepwort we are using when it needs to evict stuff... ( based on visibility tests ) 
			CVehicle *pVehicle = FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true);

			float fX = CMiniMap::GetPauseMapCursor().x;
			float fY = CMiniMap::GetPauseMapCursor().y;
			float fZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(fX, fY);

			Vector3 NewPlayerPos(fX, fY, fZ);

			Vector3	waterCheckStartPos = NewPlayerPos;

			// move player to vicinity of destination first - before pulling in the scene.
			if (pVehicle)
			{
				gRenderListGroup.Clear();  // clear any renderlists as this stuff probably has deleted entities that they are pointing to!
				pVehicle->Teleport(NewPlayerPos, 0.0f);
			}
			else ////will change this to teleport once the ped stuff is okay
			{
				gRenderListGroup.Clear();  // clear any renderlists as this stuff probably has deleted entities that they are pointing to!
				FindPlayerPed()->Teleport(NewPlayerPos, 0.0f);
			}


			CPhysics::LoadAboutPosition(RCC_VEC3V(NewPlayerPos));
			g_SceneStreamerMgr.LoadScene(NewPlayerPos);


			if (CScriptHud::bUsingMissionCreator)
			{
				// Place the ped on the ground
				u32 flags = ArchetypeFlags::GTA_MAP_TYPE_MOVER |
					ArchetypeFlags::GTA_VEHICLE_TYPE |
					ArchetypeFlags::GTA_OBJECT_TYPE |
					ArchetypeFlags::GTA_PICKUP_TYPE |
					ArchetypeFlags::GTA_GLASS_TYPE |
					ArchetypeFlags::GTA_RIVER_TYPE;

				CPedPlacement::FindZCoorForPed(TOP_SURFACE, &NewPlayerPos, NULL, NULL, NULL, 0.5f, 99.5f, flags);
			}
			else
			{
				NewPlayerPos.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, NewPlayerPos.x, NewPlayerPos.y);
			}


			// Check if we're above water
			waterCheckStartPos.z += 10.0f;
			Vector3 waterCheckEndPos = NewPlayerPos;
			Vector3 resPos;
			if( Water::TestLineAgainstWater(waterCheckStartPos, waterCheckEndPos, &resPos) )
			{
				if( resPos.z > NewPlayerPos.z )
				{
					NewPlayerPos = resPos;
				}
			}

			NewPlayerPos.z += 1.0f;

			if (pVehicle)
			{
				gRenderListGroup.Clear();  // clear any renderlists as this stuff probably has deleted entities that they are pointing to!
				pVehicle->Teleport(NewPlayerPos, 0.0f);
			}
			else ////will change this to teleport once the ped stuff is okay
			{
				gRenderListGroup.Clear();  // clear any renderlists as this stuff probably has deleted entities that they are pointing to!
				FindPlayerPed()->Teleport(NewPlayerPos, 0.0f);
			}
			gVpMan.PopViewport(); // so that tyhe streaming knows what viepwort we are using when it needs to evict stuff... ( based on visibility tests ) 

			if (NetworkInterface::IsGameInProgress())
			{
				GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_WARP));
			}

			camInterface::FadeIn(0);

			CGtaOldLoadingScreen::ForceLoadingRenderFunction(false);

			break;
		}

		case CA_LeaveGame:
		{
			camInterface::FadeOut(0);  // fade out the screen

			NetworkInterface::LeaveSession(false, false);
			break;
		}
		case CA_StartSavedGame:
		{
			audNorthAudioEngine::NotifyStartNewGame();
			camInterface::FadeOut(0);  // fade out the screen

			if (NetworkInterface::IsGameInProgress())
			{
				uiAssertf(0, "FRONTEND_STATE_RESTART_SAVED_GAME_CHECK - should never get here in a network game - Graeme");
				CNetwork::Shutdown(SHUTDOWN_SESSION);
			}

			CGame::LoadSaveGame();
			break;
		}

		case CA_StartCommerce:
			CNetworkTelemetry::GetPurchaseMetricInformation().SaveCurrentCash();
			CNetworkTelemetry::GetPurchaseMetricInformation().SetFromLocation(SPL_INGAME);
			cStoreScreenMgr::Open();
			break;
		case CA_StartCommerceMP:
			if(!cStoreScreenMgr::RequestMPStore())
            {
                CBusySpinner::Off( SPINNER_SOURCE_STORE_LOADING );
            }
			break;
		case CA_StartSocialClub:
			SocialClubMenu::Open();
			break;

		case CA_None:
			// nothing on purpose
			break;
	}

	SetClosingAction(CA_None);

	if(sm_bUnpauseGameOnMenuClose)
	{
		SetupCodeForUnPause();
	}
	else
	{
		sm_bUnpauseGameOnMenuClose = true;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::Close()
// PURPOSE:	sends a request to Actionscript to remove its refs and then tell code
//			it's ready so the pause menu system can be shut down
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::Close()
{
	if(sm_pDynamicPause)
	{
		sm_pDynamicPause->SetAvatarBGOn(false);

		if(GetDisplayPed())
		{
			GetDisplayPed()->ClearPed();
		}

		if(GetLocalPlayerDisplayPed())
		{
			GetLocalPlayerDisplayPed()->ClearPed();
		}
	}
	//@@: location CPAUSEMENU_CLOSE
	if ( (sm_bActive) && (!sm_bClosingDown) )
	{
		if (sm_iLoadingAssetsPhase != PMLP_DONE)
		{
			CloseInternal(); // Don't hang the game if we don't have enough memory to allocate the pause menu.
			CloseInternal2(true);
			return;
		}

		sm_bClosingDown = true;
		sm_PauseMenuCloseDelay = 0;

#if OUTRO_EFFECT
		if( GetCurrentMenuVersionHasFlag(kFadesIn) )
			TriggerMenuClosure();
		else
#endif
			GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("BEGIN_EXIT_PAUSE_MENU");
	}
	else
	{
		TogglePauseRenderPhases(true, OWNER_PAUSEMENU, __FUNCTION__ );
	}

	// B*1922505 - sub_2 - Stuck on alert screen after idling out while in XB store and constrained.
	// no nicer way to do this without exposing to the script that does the bulk of this
	// otherwise it's a catch 22 of the UI waiting on network waiting on UI
	if (cStoreScreenMgr::IsStoreMenuOpen())
	{
#if GEN9_STANDALONE_ENABLED
		// Skip the below behaviour if landing page will launch on close
		if (!cStoreScreenMgr::GetLaunchLandingPageOnClose())
#endif // GEN9_STANDALONE_ENABLED
		{
			cStoreScreenMgr::RequestExit();
			cStoreScreenMgr::SetReturnToPauseMenu(false);
		}
	}
}

// up here if only for proximity to the CreateMovie Wrappers
void CPauseMenu::KickoffStreamingChildMovie(const char* cGfxFilename, MenuScreenId newScreen, eMenuceptionDir menuceptionDir)
{
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
	STRVIS_SET_MARKER_TEXT("Pause menu - Kick off streaming child movie");

	uiAssertf(newScreen != MENU_UNIQUE_ID_INVALID, "Can't kick off a streaming request for %s with an invalid menu ID!", cGfxFilename);

	ClearInstructionalButtons();

	atHashWithStringBank hGfxFilename(cGfxFilename);

	// gotta check this in two passes, because the array is sparse and we might find an opening BEFORE we find a match
	for(int iPass = 0; iPass < 2;++iPass)
	{
		for(int i=0; i < MAX_PAUSE_MENU_CHILD_MOVIES;++i)
		{
			CStreamMovieHelper& rMovieHelper = GetChildMovieHelper(i);
			bool bPassesCriteria = false;
			if( iPass == 0 )
				bPassesCriteria = (rMovieHelper.GetRequestingScreen() == newScreen || rMovieHelper.GetGfxFilename() == hGfxFilename);
			else
				bPassesCriteria = rMovieHelper.IsFree();

			if(bPassesCriteria )
			{
				sfDisplayf("STREAMED_PANE Started streaming '%s' for '%s'", cGfxFilename?cGfxFilename:"-nothing-", newScreen.GetParserName());
				sm_iStreamingMovie = i;
				rMovieHelper.Set(cGfxFilename, newScreen, menuceptionDir);
				return;
			}
		}
	}

	Quitf(ERR_GUI_CHILD,"You've run out of Child movies you can create! This is pretty bad!");
}

bool CPauseMenu::LoadBaseMovie(eNUM_PAUSE_MENU_MOVIES kMovieIndex, const char* fileName, const char* dataOverride /*= NULL*/)
{
	CScaleformMovieWrapper& wrapper = GetMovieWrapper(kMovieIndex);
	if( wrapper.IsFree() )
	{
		SGeneralPauseDataConfig kConfig;
		SGeneralPauseDataConfig* pConfigData = GetMenuArray().GeneralData.MovieSettings.Access(dataOverride?dataOverride:fileName);
		if( !pConfigData )
			pConfigData = &kConfig;

		int parentMovieId = INVALID_MOVIE_ID;
		if( pConfigData->bDependsOnSharedComponents )
		{
			int i=PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_END;
			while( i >= PAUSE_MENU_MOVIE_SHARED_COMPONENTS && parentMovieId == INVALID_MOVIE_ID )
			{
				parentMovieId = GetMovieWrapper(eNUM_PAUSE_MENU_MOVIES(i)).GetMovieID();
				--i;
			}
		}
		Vector2 vMoviePos ( pConfigData->vPos  );
		Vector2 vMovieSize( pConfigData->vSize );

		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(pConfigData->HAlign), &vMoviePos, &vMovieSize);

		wrapper.CreateMovie(SF_BASE_CLASS_PAUSEMENU, fileName, vMoviePos, vMovieSize, true, INVALID_MOVIE_ID, parentMovieId, pConfigData->bRequiresMovieView, SF_MOVIE_TAGGED_BY_CODE, true);
		if(kMovieIndex == PAUSE_MENU_MOVIE_CONTENT)
		{
			CScaleformMgr::OverrideOriginalMoviePosition(wrapper.GetMovieID(), vMoviePos);
		}

		return false;
	}
	
	return wrapper.IsActive();
}
//////////////////////////////////////////////////////////////////////////

void CPauseMenu::OpenCorrectMenu(bool bSkipCutscene /* = false */)
{
	
	if (!sm_bClosingDown)
	{
		if (IsGameInStateWhereItCanBePaused())
		{
			//@@: location CPAUSEMENU_OPENCORRECTMENU_OPENMP
			Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			Displayf("    OPENING PAUSE MENU DUE TO USER INPUT");
			Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

			STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
			STRVIS_SET_MARKER_TEXT("Begin show pause menu");
			g_FrontendAudioEntity.StartPauseMenuFirstHalf();
			
			//@@: range CPAUSEMENU_OPENCORRECTMENU {
			if(CScriptHud::bUsingMissionCreator)
			{
				if( bSkipCutscene && sm_bActive )
					Restart(FE_MENU_VERSION_CREATOR_PAUSE);
				else
					Open(FE_MENU_VERSION_CREATOR_PAUSE);
			}
			else if (NetworkInterface::IsGameInProgress())
			{
				
				if( bSkipCutscene && sm_bActive )
					Restart(FE_MENU_VERSION_MP_PAUSE);
				else
				{

					Open(FE_MENU_VERSION_MP_PAUSE);
				}
			}
			// there are cutscenes in MP, but those should always just straight-up pause the game
			else if( !bSkipCutscene && CutSceneManager::GetInstance()->IsRunning() )
			{
				if( sm_bActive )
					Restart(FE_MENU_VERSION_CUTSCENE_PAUSE);
				else
					Open(FE_MENU_VERSION_CUTSCENE_PAUSE);
			}
			else
			{
				if( bSkipCutscene && sm_bActive )
					Restart(FE_MENU_VERSION_SP_PAUSE);
				else
					Open(FE_MENU_VERSION_SP_PAUSE);
			}
			//@@: } CPAUSEMENU_OPENCORRECTMENU

		}
	}
}

#if __BANK
void UpdateState()
{
	// early breakpoint for if you want to track when GetStateForScript is called without a bunch of noise
	static bool bSkipThisForDebuggerSanity = false;
	if(bSkipThisForDebuggerSanity)
		return;


	CPauseMenu::scrPauseMenuScriptState iState = CPauseMenu::GetStateForScript();
	switch(iState)
	{
			case CPauseMenu::PM_INACTIVE:		s_PauseMenuState.SetHashWithString(0x20e1ebf5,	"PM_INACTIVE");		break;
			case CPauseMenu::PM_STARTING_UP:	s_PauseMenuState.SetHashWithString(0x63e334,	"PM_STARTING_UP");	break;
			case CPauseMenu::PM_RESTARTING:		s_PauseMenuState.SetHashWithString(0x2fecb9b0,	"PM_RESTARTING");	break;
			case CPauseMenu::PM_READY:			s_PauseMenuState.SetHashWithString(0x7323a5f4,	"PM_READY");		break;
			case CPauseMenu::PM_IN_STORE:		s_PauseMenuState.SetHashWithString(0x545ca67a,	"PM_IN_STORE");		break;
			case CPauseMenu::PM_IN_SC_MENU:		s_PauseMenuState.SetHashWithString(0x848cf288,	"PM_IN_SC_MENU");	break;
			case CPauseMenu::PM_SHUTTING_DOWN:	s_PauseMenuState.SetHashWithString(0xf93ea8cc,	"PM_SHUTTING_DOWN"); break;
			default:							s_PauseMenuState.SetHashWithString(0x2e8ffceb,	"UNKNOWN");			break;
	}
}
#define BANK_UPDATE_STATE() UpdateState()
#else
#define BANK_UPDATE_STATE() do {}while(0)
#endif



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::Update
// PURPOSE:	updates the pause menu
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::Update()
{
	PF_AUTO_PUSH_TIMEBAR("CPauseMenu Update");

	const bool bLockInputThisFrame = (IsMenuControlLocked() || sm_bMenuControlChangedThisFrame) || CScriptHud::bSuppressPauseMenuRenderThisFrame || IsInInputCooldown();

	sm_bMenuControlChangedThisFrame = false;  // reset this every frame

	if(sm_QueuedOpenParams.m_bActive)
	{
		Open(sm_QueuedOpenParams.m_iMenuVersion, sm_QueuedOpenParams.m_bPauseGame, sm_QueuedOpenParams.m_HighlightTab,
			sm_QueuedOpenParams.m_bFailGently);
		sm_QueuedOpenParams.Reset();
	}
#if RSG_ORBIS
	UpdateOrbisSafeZone();
#endif

#if GTA_REPLAY
	if ( (CVideoEditorUi::IsActive() || CVideoEditorInterface::IsActive()) && !IsClosingDown() )  // need to allow it to shutdown if in that state
	{
		BANK_UPDATE_STATE();
		return;
	}
#endif

	if(SocialClubMenu::IsActive())
	{
		BANK_UPDATE_STATE();
		return;
	}

	if (sm_bActive && sm_bWaitOnDisplayCalibrationScreen)
	{
		if (CDisplayCalibration::UpdateInput())
		{
			CloseDisplayCalibrationScreen();
		}
		BANK_UPDATE_STATE();
		return;
	}

	if (sm_bActive && sm_bDisplayCreditsScreenNextFrame &&
		(GetCurrentActivePanel() == MENU_UNIQUE_ID_CREDITS || 
		GetCurrentActivePanel() == MENU_UNIQUE_ID_LEGAL ||
		GetCurrentActivePanel() == MENU_UNIQUE_ID_CREDITS_LEGAL))
	{
		if(!TheText.HasAdditionalTextLoaded(CREDITS_TEXT_SLOT))
		{
			TheText.RequestAdditionalText("CREDIT", CREDITS_TEXT_SLOT);
		}

		CCredits::SetLaunchedFromPauseMenu(true); // must be set before init
		CCredits::Init(INIT_AFTER_MAP_LOADED);
		CCredits::SetToRenderBeforeFade(false);
		sm_eCreditsLaunchedFrom = GetCurrentScreen();
		sm_bWaitOnCreditsScreen = true;
		sm_bDisplayCreditsScreenNextFrame = false;
	}

	if (sm_bActive && sm_bWaitOnCreditsScreen)
	{
		CCredits::UpdateInput();
		if (CCredits::HaveReachedEnd() ||
			!CCredits::IsRunning())
		{
			CloseCreditsScreen();
		}

		BANK_UPDATE_STATE();
		return;
	}

#if RSG_PC
	if (sm_bActive && sm_bWaitOnPCGamepadCalibrationScreen)
	{
		CPCGamepadCalibration::Update();
		if (CPCGamepadCalibration::HasCompleted())
		{
			ClosePCGamepadCalibrationScreen();
		}
		BANK_UPDATE_STATE();
		return;
	}

	if((SUIContexts::IsActive(UIATSTRINGHASH("AUD_QSCANNING",0x54348A17)) || SUIContexts::IsActive(UIATSTRINGHASH("AUD_FSCANNING",0x64E3F10A))) && 
		!audRadioStation::GetUserRadioTrackManager()->IsScanning() && audRadioStation::GetUserRadioTrackManager()->IsScanningIdle())
	{		
		SUIContexts::SetActive(UIATSTRINGHASH("AUD_QSCANNING",0x54348A17), false);
		SUIContexts::SetActive(UIATSTRINGHASH("AUD_FSCANNING",0x64E3F10A), false);

		if(GetCurrentActivePanel() == MENU_UNIQUE_ID_SETTINGS_AUDIO)
		{
			const s32 stationIndex = CPauseMenu::GetMenuPreference(PREF_RADIO_STATION);
			const audRadioStation *station = audRadioStation::GetStation(stationIndex);
			if(station && station->IsUserStation() && stationIndex != (s32)g_OffRadioStation && !audRadioStation::HasUserTracks())
			{
				SetMenuPreference(PREF_RADIO_STATION, -1);	// set to Radio Off
				HandleRadioStation(0, true);				// update the menu
			}
			GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_AUDIO, true);
			RedrawInstructionalButtons();
			PlaySound("BACK");
		}
	}
	
#endif

	// Stop rumbles except when the user is changing the rumble option.
	if(sm_bActive && sm_EndAllowRumbleTime < fwTimer::GetSystemTimeInMilliseconds() && !sm_bAllowRumble)
	{
		CControlMgr::StopPadsShaking();
	}

#if __PRELOAD_PAUSE_MENU
	// in order to keep the pause menu loading somewhat fast, we want to keep the pause menu assets around
	// so as soon as we close the menu, we go ahead and reload the assets for the base set
	// then when we go ahead and start the menu up, we wipe out the stuff, but they're still resident so they load faster
	if(!IsActive())
		HandlePreloadingState();
	else
		sm_iPreloadingAssetsPhase = PMLP_FIRST;
#endif

	//
	// enter pause menu:
	//
	// (though not if the warning messages are up)
	// don't process this AT ALL if script has disabled this.
	if( !SReportMenu::GetInstance().IsActive() && !CWarningScreen::IsActive() && !CScriptHud::bDisablePauseMenuThisFrame )
	{
		bool bPressedBWhileLoading = false;
		if( sm_bActive 
			&& (!sm_bRenderMenus || sm_bRestarting || sm_iLoadingAssetsPhase != PMLP_DONE) 
			&& GetCurrentMenuVersionHasFlag(kAllowBackingOut) )
		{
			bPressedBWhileLoading = CheckInput(FRONTEND_INPUT_BACK, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE);
		}

#if RSG_PC
		bool systemPauseRequest = GRCDEVICE.GetRequestPauseMenu() && GetMenuPreference(PREF_VID_PAUSE_ON_FOCUS_LOSS) && NetworkInterface::CanPause()
#if DEBUG_PAD_INPUT
			&& !CControlMgr::IsDebugPadOn()
#endif
			;
#endif

		if(bPressedBWhileLoading || CheckInput(FRONTEND_INPUT_START, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) WIN32PC_ONLY(|| (systemPauseRequest && !sm_bActive)))
		{
			if (!sm_bActive)
			{
				UpdatePlayerInfoAtTopOfScreen(true, false);  // send 1 request through to get pedheadshot and other info as soon as we can so its ready by the time pause menu appears

				OpenCorrectMenu();
			}
			else
			{
				// drop out of max payne mode via the start button
				if( sm_bMaxPayneMode )
				{
					SetMaxPayneMode(false);
				}
				else
				if( GetCurrentMenuVersionHasFlag(kAllowStartButton) 
					&& !NetworkInterface::IsGameInProgress() 
					&& CutSceneManager::GetInstance()->IsRunning() )
				{
					if( !sm_bRestarting && sm_iLoadingAssetsPhase == PMLP_DONE )
					{
						OpenCorrectMenu();
					}
					else
						Displayf("USER WANTED TO PAUSE, BUT WE DENIED IT BECAUSE THEY ARE SPAMMY");
				}
				else
				if( !sm_bRenderMenus || GetCurrentMenuVersionHasFlag(kAllowStartButton) )
				{
					if (!IsInSaveGameMenus()
#if KEYBOARD_MOUSE_SUPPORT
						// When using the keyboard we do not want the escape key (mapped to frontend back AND frontend start) to exit the 
						// pause menu when in a sub menu, however, if backing out is not supported we need to allow the exit. If FRONTEND_INPUT_BACK
						// is pressed at the same time as FRONTEND_INPUT_START then we assume this is the conflict in question. This is for
						// url:bugstar:764252.
						&& (!GetCurrentMenuVersionHasFlag(kAllowBackingOut) || !CheckInput(FRONTEND_INPUT_BACK, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE))
#endif // KEYBOARD_MOUSE_SUPPORT
						)
					{
						const char* pWarningMessageToShowIfQuit = NULL;
						bool bCanContinue = false;
						if( IsCurrentScreenValid() && GetCurrentScreenData().HasDynamicMenu()
							&& GetCurrentScreenData().GetDynamicMenu()->ShouldBlockCloseAttempt(pWarningMessageToShowIfQuit, bCanContinue) )
						{
							if( bCanContinue )
							{
								ShowConfirmationAlert(pWarningMessageToShowIfQuit, datCallback(CFA(CPauseMenu::CloseAndPlayProperSoundCB)));
							}
							else
							{
								CWarningMessage::Data newData;
								newData.m_TextLabelHeading = "GLOBAL_ALERT_DEFAULT";
								newData.m_TextLabelBody = pWarningMessageToShowIfQuit;
								newData.m_iFlags = FE_WARNING_OK;
								newData.m_bCloseAfterPress = true;
								GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newData);
							}

							PlaySound("BACK");
						}
						else
						{
							CloseAndPlayProperSoundCB();
						}
					}
				}
			}
		}
	}

	// unallowable conditions triggered while pausing
	// handle them if we haven't paused yet
	if( sm_bActive  // we've activated
		&& sm_bStartedUserPause && sm_iCallbacksPending  // we haven't fully paused yet
		&& (!IsGameInStateWhereItCanBePaused() || CutSceneManager::GetInstance()->IsRunning()) ) // then something happened
	{
		Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		Displayf("          CLOSING PAUSE MENU DUE TO CONDITIONS GOING BAD");
		Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		Close();
		if (NetworkInterface::IsGameInProgress())
			g_FrontendAudioEntity.PlaySound("RESUME","HUD_FRONTEND_MP_SOUNDSET");
		else
			g_FrontendAudioEntity.PlaySound("RESUME",s_HudFrontendSoundset);
	}

#if __BANK
	if( s_bDbgOscillate )
	{
		if( --s_iDbgFramesRemaining <= 0 )
		{
			s_iDbgFramesRemaining = s_iDbgFramesToWait;
			eMenuScreen tabToHighlight = s_pszDebugMenuTabToOpen.IsNotNull() ? static_cast<eMenuScreen>(parser_eMenuScreen_Data.ValueFromName(s_pszDebugMenuTabToOpen.GetCStr())) : MENU_UNIQUE_ID_INVALID;
			if( s_pszDebugMenuOscillator[s_iOscillateField].IsNull() )
				Close();
			else if( !sm_bActive )
				Open(s_pszDebugMenuOscillator[s_iOscillateField], s_bDebugPauseGame, tabToHighlight, true);
			else
				Restart(s_pszDebugMenuOscillator[s_iOscillateField], tabToHighlight);

			s_iOscillateField = (s_iOscillateField+1)%2;
		}
	}
#endif

	if(sm_bWaitingOnPulseWarning)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "CWS_WARNING", "WRN_PULSE", FE_WARNING_OK);
		eWarningButtonFlags eButton = CWarningScreen::CheckAllInput(false);
		if(eButton != FE_WARNING_NONE)
		{
			if(eButton == FE_WARNING_OK)
			{
				sm_bWaitingOnPulseWarning = false;
			}
		}
	}

	if(sm_bWaitingOnAimWarning)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "CWS_WARNING", "WRN_AIMING", FE_WARNING_OK);
		eWarningButtonFlags eButton = CWarningScreen::CheckAllInput(false);
		if(eButton != FE_WARNING_NONE)
		{
			if(eButton == FE_WARNING_OK)
			{
				sm_bWaitingOnAimWarning = false;
			}
		}
	}

#if RSG_PC
	if(sm_bWaitingOnGfxOverrideWarning)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "CWS_WARNING", "WRN_GFX_OVERRIDE", FE_WARNING_OK);
		eWarningButtonFlags eButton = CWarningScreen::CheckAllInput(false);
		if(eButton != FE_WARNING_NONE)
		{
			if(eButton == FE_WARNING_OK)
			{
				sm_bWaitingOnGfxOverrideWarning = false;
			}
		}
	}
#endif // RSG_PC

#if !RSG_PC
#if RSG_DURANGO
	if(sm_bHelpLoginPromptActive)
	{
		CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, "HELP_SIGNIN", FE_WARNING_YES_NO);

		eWarningButtonFlags eButton = CWarningScreen::CheckAllInput(false);
		if(eButton != FE_WARNING_NONE)
		{
			sm_bHelpLoginPromptActive = false;
			if(eButton == FE_WARNING_YES)
			{
				CLiveManager::ShowSigninUi();
			}
		}
	}
#endif // RSG_DURANGO

	CControlMgr::CheckForControllerDisconnect();

#endif // !RSG_PC

#if RSG_PC
	GRCDEVICE.SetRequestPauseMenu(false);
#endif


	if( sm_pDynamicPause )
	{
		// delayed deletion. Delete it here instead of via callback
		if(!sm_bActive && !sm_iCallbacksPending)
			DeleteDynamicPause();
		else
			sm_pDynamicPause->Update();
	}

	if (sm_bClosingDownPart2)
	{
		CloseInternal2();
	}

	if( !sm_bActive || sm_bRestarting )
	{
		BANK_UPDATE_STATE();
		return;
	}

	SClanInviteManager::GetInstance().Update();
	SPlayerCardManager::GetInstance().Update();
	
	PF_PUSH_TIMEBAR("Scaleform Movie Update (PauseMenu)");

	if( HandleCreationAndLoadingState() )
	{
		BANK_UPDATE_STATE();
		PF_POP_TIMEBAR();
		return;
	}

	if (sm_iLoadingAssetsPhase != PMLP_DONE)
	{
		BANK_UPDATE_STATE();
		PF_POP_TIMEBAR();
		return;
	}


	QueueSavegameOperationIfNecessary();

	UpdatePlayerInfoAtTopOfScreen(false, true);

	if (MenuVersionHasHeadings())
	{
		SetStoreAvailable(IsStoreAvailable());

		UpdateInput(bLockInputThisFrame);

		UpdateSwitchPane();

		if(sm_updateDelay == 0)
		{
			// now that we're actually in the menu and everything's set, go.
			if( sm_bHasFocusedMenu &&
				!sm_bCloseMenus &&
				IsCurrentScreenValid() && GetCurrentScreenData().HasDynamicMenu() )
			{
				CMenuScreen& curScreen = GetCurrentScreenData();
				CMenuScreen& cScreen = GetCurrentHighlightedTabData();

				// note that this is the cause of some off behavior maybe sometimes?
				//if( uiVerifyf(curScreen.MenuScreen == cScreen.MenuScreen || sm_aMenuState.GetCount() > 1, "Something messed up, and now you'll have an empty screen probably. But look it you, bugging the assert like it's the real issue."))
				if( curScreen.MenuScreen == cScreen.MenuScreen || sm_aMenuState.GetCount() > 1 )
				{
#if !__NO_OUTPUT
					if( s_UiSpewLastScreen != curScreen.MenuScreen )
					{
						uiSpew("      UPDATE: %s == %s, %d)", curScreen.MenuScreen.GetParserName(), cScreen.MenuScreen.GetParserName(), sm_aMenuState.GetCount() );
						s_UiSpewLastScreen = curScreen.MenuScreen;
					}
					else if( fwTimer::GetSystemFrameCount() % 60 == 0)
					{
						uiSpew1("      UPDATE: %s == %s, %d)", curScreen.MenuScreen.GetParserName(), cScreen.MenuScreen.GetParserName(), sm_aMenuState.GetCount() );
					}
#endif
					GetCurrentScreenData().GetDynamicMenu()->Update();
				}
				else
					uiSpew1("????? UPDATE: %s != %s)", curScreen.MenuScreen.GetParserName(), cScreen.MenuScreen.GetParserName());
			}
		}
		else
		{
			--sm_updateDelay;
		}
	} 
	else if( GetCurrentMenuVersionHasFlag( kForceProcessInput ) )
	{
		UpdateInput(bLockInputThisFrame);
	}

	if( sm_pDynamicPause )
	{
		sm_pDynamicPause->UpdateMenuPed();
	}

#if RSG_PC
	// Disable sending of outgoing voice data while in the voice chat settings menu B*2168376
	// WARNING: As of December 19, 2014, players in the voice chat settings menu will still
	//			be able to hear other players but other players can't hear them. There is no indication
	//			that other players can't hear the local player so this may appear to be a bug but it is
	//			implemented as requested. Ideally, the player would have to manually trigger a voice test
	//			mode (similar to other PC games) where they are clearly alerted to the fact that they are
	//			testing voice chat and other players can't hear them. Note that voice data can still be
	//			sent from anywhere else in the pause menu, just not the voice settings menu.
	if(GetCurrentActivePanel() == MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT)
	{
		CNetwork::GetVoice().SetVoiceTestMode(true);
	}
	else
	{
		CNetwork::GetVoice().SetVoiceTestMode(false);
	}

	sm_MouseClickEvent = PMMouseEvent::INVALID_MOUSE_EVENT;
#endif // RSG_PC

	if (sm_bCloseMenus)
	{
		sm_bCloseMenus = false;

		gRenderThreadInterface.Flush();

#if RSG_PC
		CNetwork::GetVoice().SetVoiceTestMode(false);
#endif // RSG_PC

		CloseInternal();
	}

	BANK_UPDATE_STATE();

	PF_POP_TIMEBAR();
}

void CPauseMenu::CloseAndPlayProperSoundCB()
{
	Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	Displayf("          CLOSING PAUSE MENU DUE TO USER INPUT");
	Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	BackOutOfSaveGameList();
	Close();
	if (NetworkInterface::IsGameInProgress())
		g_FrontendAudioEntity.PlaySound("RESUME","HUD_FRONTEND_MP_SOUNDSET");
	else
		g_FrontendAudioEntity.PlaySound("RESUME",s_HudFrontendSoundset);
}



#define LOAD_MOVIE(movieIndex, fileName)\
	if( LoadBaseMovie(movieIndex, fileName)){\
	uiDisplayf("PauseMenu: Asset is already loaded... %s", fileName); }\
	else {\
	uiDisplayf("PauseMenu: Assets are loading... %s", fileName); }

#define LOAD_MOVIE_ALT(movieIndex, fileName, pszOverride)\
	if( LoadBaseMovie(movieIndex, fileName, pszOverride)){\
	uiDisplayf("PauseMenu: Asset is already loaded... %s", fileName); }\
	else {\
	uiDisplayf("PauseMenu: Assets are loading... %s", fileName); }


#if __PRELOAD_PAUSE_MENU
bool CPauseMenu::HandlePreloadingState()
{
	static bool bCareAboutDependencies = true;
#if __BANK
	if( !s_bDebugResidentPauseMenu )
		return true;
#endif

	if( CLoadingScreens::AreActive() )
	{
		sm_iPreloadingAssetsPhase = PMLP_FIRST;
		return true;
	}

	switch(sm_iPreloadingAssetsPhase)
	{
		// HACK: Shittily implemented few frame delay to allow the pause menu some time to release its assets
		// instead of quickly restarting these movies after we reload them
		case PMLP_FIRST:
			RemoveAllMovies();

			sm_iPreloadingAssetsPhase = 10;
			return true;

		case 10:
		case 11:
		case 12:
			sm_iPreloadingAssetsPhase++;
			return true;
		case 13:

			LOAD_MOVIE( PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS,	PAUSEMENU_FILENAME_INSTRUCTIONAL_BUTTONS);
			LOAD_MOVIE( PAUSE_MENU_MOVIE_HEADER,				PAUSEMENU_FILENAME_HEADER);
			LOAD_MOVIE( PAUSE_MENU_MOVIE_SHARED_COMPONENTS,		PAUSEMENU_FILENAME_SHARED_COMPONENTS);

			// fallthrough on purpose

		case PMLP_LOAD_CUSTOM_COMPONENTS:
			// load dependent movies
			sm_iPreloadingAssetsPhase = PMLP_LOAD_CUSTOM_COMPONENTS;
			{
				const char* preLoadList[] = {DEFAULT_PRELOAD_COMPONENTS};

				bool bAllLoaded = true;
				for(int iComp = 0; iComp < NELEM(preLoadList); ++iComp)
					bAllLoaded = LoadBaseMovie( static_cast<eNUM_PAUSE_MENU_MOVIES>(iComp+PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_START), preLoadList[iComp] ) && bAllLoaded;
				
				if( bCareAboutDependencies && !bAllLoaded )
				{
					uiDebugf2("PRELOAD: Something wasn't ready yet so waiting a bit");
					return false;
				}
			}

			// fallthrough on purpose
		case PMLP_LOAD_FIRST_PANEL:
			sm_iPreloadingAssetsPhase = PMLP_LOAD_FIRST_PANEL;

			// can't do content until shared components are ready
			if(bCareAboutDependencies && !GetMovieWrapper(PAUSE_MENU_MOVIE_SHARED_COMPONENTS).IsActive())
			{
				uiDisplayf("PRELOAD: Shared Components not ready");
				return false;
			}

			// load content and the default menu
			LOAD_MOVIE( PAUSE_MENU_MOVIE_CONTENT, PAUSEMENU_FILENAME_CONTENT);
			GetChildMovieHelper(0).Set(DEFAULT_PRELOAD_PAGE, MENU_UNIQUE_ID_MAP, kMENUCEPT_LIMBO, false); // requesting screen doesn't REALLY matter here
		
			// fallthrough on purpose!
		case PMLP_CHECK_ASSETS_LOADED:
			sm_iPreloadingAssetsPhase = PMLP_CHECK_ASSETS_LOADED;
			if( !GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).IsActive() 
				|| !GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).IsActive() 
				|| !GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER).IsActive() )
			{
				uiDisplayf("Waiting on Content, Instructional Buttons, or Header to be active");
				return false;
			}



			uiDisplayf("PRELOAD: All done!");

			// fallthrough on purpose!
		case PMLP_DONE:
			// we don't want to bother to update these movies, so disable them
			CScaleformMgr::ForceMovieUpdateInstantly( GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).GetMovieID(), false);
			CScaleformMgr::ForceMovieUpdateInstantly( GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).GetMovieID(), false);
			CScaleformMgr::ForceMovieUpdateInstantly( GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER).GetMovieID(), false);
			sm_iPreloadingAssetsPhase = PMLP_DONE;
			return true;
	}

	return true;
}
#endif // __PRELOAD_PAUSE_MENU

bool CPauseMenu::HandleLoadingState()
{
	if (sm_iLoadingAssetsPhase == PMLP_DONE)
		return false;


	switch(sm_iLoadingAssetsPhase)
	{
	case PMLP_FIRST:
		uiAssert(sm_PedShotHandle == 0);
		sm_PedShotHandle = (s32)PEDHEADSHOTMANAGER.RegisterPed(CGameWorld::FindLocalPlayer());

		LOAD_MOVIE( PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS, PAUSEMENU_FILENAME_INSTRUCTIONAL_BUTTONS);
		LOAD_MOVIE( PAUSE_MENU_MOVIE_HEADER,				PAUSEMENU_FILENAME_HEADER);
		LOAD_MOVIE( PAUSE_MENU_MOVIE_SHARED_COMPONENTS,		PAUSEMENU_FILENAME_SHARED_COMPONENTS);
		
		CBusySpinner::RegisterInstructionalButtonMovie(GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).GetMovieID());  // register this "instructional button" movie with the spinner system

		// fallthrough on purpose

	case PMLP_LOAD_CUSTOM_COMPONENTS:
		{
			// load dependent movies
			sm_iLoadingAssetsPhase = PMLP_LOAD_CUSTOM_COMPONENTS;

			const SharedComponentList& optionalComponents = GetDynamicPauseMenu()->GetOptionalSharedComponents();
			bool bContinue = true;
			int i=0; 
			while( i < MAX_EXTRA_SHARED_MOVIES && i < optionalComponents.GetCount() )
			{
                char const * const c_assetName = optionalComponents[i]->c_str();
				uiDebugf3("PauseMenu: Assets are loading... %s", c_assetName );
				bContinue = bContinue && LoadBaseMovie( static_cast<eNUM_PAUSE_MENU_MOVIES>(i+PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_START), c_assetName );
				++i;
			}

			if(!bContinue || !GetMovieWrapper(PAUSE_MENU_MOVIE_SHARED_COMPONENTS).IsActive())
				break;

			LOAD_MOVIE_ALT( PAUSE_MENU_MOVIE_CONTENT, PAUSEMENU_FILENAME_CONTENT, GetCurrentMenuVersionHasFlag(kUseAlternateContentPos) ? "PAUSE_MENU_SP_CONTENT_ALT" : NULL);
		}
		// fallthrough on purpose
	

	case PMLP_LOAD_FIRST_PANEL:
		sm_iLoadingAssetsPhase = PMLP_LOAD_FIRST_PANEL;
		if( MenuVersionHasHeadings() && sm_iStreamingMovie == NO_STREAMING_MOVIE)
		{
			const CMenuScreen& cScreen = GetCurrentHighlightedTabData();
			const char* cGfxFilename = cScreen.GetGfxFilename();

			KickoffStreamingChildMovie(cGfxFilename, cScreen.MenuScreen);
			uiDisplayf("PauseMenu: Assets are loading... %s", cGfxFilename);
		}
		// fallthrough on purpose

	case PMLP_CHECK_ASSETS_LOADED:
		sm_iLoadingAssetsPhase = PMLP_CHECK_ASSETS_LOADED;
		if (WIN32PC_ONLY(!GRCDEVICE.IsMinimized() &&) AreAllAssetsActive())
		{
			SetupSprites();
			sm_bSetupStartingPane = true;
			sm_iLoadingAssetsPhase = PMLP_DONE;
			uiDebugf3("PauseMenu: Assets are now loaded!");

			GetDynamicPauseMenu()->StartAudioTransition(GetCurrentMenuVersionHasFlag(kDelayAudioUntilUnsuppressed));
			return true;
		}
		break;
	} // endswitch

	return false;
}

bool CPauseMenu::HandleCreationAndLoadingState()
{
	bool bInitialisationThisFrame = HandleLoadingState();

	if (!sm_bSetupStartingPane)
		return false;

	MenuScreenId iUniqueInitialScreenId = GetInitialScreen();
	MenuArrayIndex iHeaderMenuId = GetActualScreen(iUniqueInitialScreenId);


	if ( !bInitialisationThisFrame && sm_iStreamingMovie == NO_STREAMING_MOVIE )
	{
		if (iHeaderMenuId >= 0 && MenuVersionHasHeadings())
		{
			const CMenuScreen& cScreen = GetCurrentHighlightedTabData();
			const char* cGfxFilename = cScreen.GetGfxFilename();
			KickoffStreamingChildMovie(cGfxFilename, cScreen.MenuScreen);
		}
	}
	else if( bInitialisationThisFrame || AreAllAssetsActive())
	{
		STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
		STRVIS_SET_MARKER_TEXT("Pause menu - set up starting pane");
		
		sm_bSetupStartingPane = false;
		const CMenuScreen& curHeaderScreen = GetScreenDataByIndex(iHeaderMenuId);

		// push the initial screen's context
		SUIContexts::Activate(curHeaderScreen.MenuScreen.GetParserName());
		SetNavigatingContent(false);

		CScaleformMovieWrapper& pauseContent = GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
		pauseContent.CallMethod("SET_TEXT_SIZE", CText::IsAsianLanguage()); // first param is if it's asian language. Second size (unused) is a size override.

		if (MenuVersionHasHeadings())
		{
			SetupMenuHeadings();

			if (curHeaderScreen.MenuItems.GetCount() == 1)
			{
				SetNavigatingContent(true);
				SUIContexts::Activate( UIATSTRINGHASH("SINGLE_SCREEN",0xe0250d7a) );
			}

			if( GetCurrentMenuVersionHasFlag(kGeneratesMenuData))  // do not want to generate menu data on 1st run if its a scripted screen
			{
				GenerateMenuData(curHeaderScreen.MenuItems[0].MenuUniqueId);
			}
		}
		else
		{
			CScaleformMovieWrapper& pauseHeader = GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER);

			pauseHeader.CallMethod( "SHOW_HEADING_DETAILS", false);
			pauseHeader.CallMethod( "SET_HEADER_TITLE", "", false, "");

			sm_TabLocked.Reset();
			if(GetCurrentMenuVersionHasFlag(kForceButtonRender) )
			{
				SetCurrentScreen(GetInitialScreen());
				RedrawInstructionalButtons(0);
			}
		}

		UnlockMenuControl();

		GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_SC_LOGGED", CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub());
#if RSG_PC
		pauseContent.CallMethod("INIT_MOUSE_EVENTS"); // 1881957
#endif

		uiDebugf3("PauseMenu: Starting pane is now set up");
	}

	return true;
}

void CPauseMenu::TriggerMenuClosure()
{
	if(DynamicMenuExists())
	{
		if(GetDisplayPed())
		{
			GetDisplayPed()->ClearPed();
		}

		if(GetLocalPlayerDisplayPed())
		{
			GetLocalPlayerDisplayPed()->ClearPed();
		}
	}

	sm_bSetupStartingPane = false;
	sm_bCloseMenus = true;
}

void CPauseMenu::CloseDisplayCalibrationScreen()
{
	if(sm_bWaitOnDisplayCalibrationScreen)
	{
		CDisplayCalibration::RemoveCalibrationMovie();
		CDisplayCalibration::SetActive(false);
		sm_bWaitOnDisplayCalibrationScreen = false;

		CPauseMenu::GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_DISPLAY, true);
	}
}

void CPauseMenu::CloseCreditsScreen()
{
	if (sm_bWaitOnCreditsScreen)
	{
		CCredits::Shutdown(SHUTDOWN_WITH_MAP_LOADED);
		CCredits::SetLaunchedFromPauseMenu(false); // must be set after shutdown
		sm_bDisplayCreditsScreenNextFrame = false;
		sm_bWaitOnCreditsScreen = false;

		if(!sm_bClosingDown)
		{
			CPauseMenu::GenerateMenuData(sm_eCreditsLaunchedFrom, true);
		}

		sm_eCreditsLaunchedFrom = MENU_UNIQUE_ID_INVALID;

		// clear credit text
		if( TheText.HasAdditionalTextLoaded(CREDITS_TEXT_SLOT) || TheText.IsRequestingAdditionalText(CREDITS_TEXT_SLOT))
		{
			TheText.FreeTextSlot(CREDITS_TEXT_SLOT,false);
		}
	}
}

bool CPauseMenu::IsPlayCreditsSupportedOnThisScreen( MenuScreenId const menuLayoutId )
{
	bool isSupported = false;

    if( menuLayoutId == MENU_UNIQUE_ID_CREDITS ||
		menuLayoutId == MENU_UNIQUE_ID_LEGAL ||
		menuLayoutId == MENU_UNIQUE_ID_CREDITS_LEGAL )
    {
        if( Tunables::IsInstantiated() )
        {
			isSupported = Tunables::GetInstance().TryAccess( CD_GLOBAL_HASH, ATSTRINGHASH( "UI_SHOULD_SHOW_CREDITS_BUTTON", 0x1a9a97e3 ), false );
        }
    }

	return isSupported;
}

#if RSG_PC
void CPauseMenu::ClosePCGamepadCalibrationScreen()
{
	if(sm_bWaitOnPCGamepadCalibrationScreen)
	{
		CPCGamepadCalibration::RemoveCalibrationMovie();
		sm_bWaitOnPCGamepadCalibrationScreen = false;

		CPauseMenu::GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_CONTROLS, true);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
CMenuScreen& CPauseMenu::GetCurrentActivePanelData()
{
	MenuArrayIndex index = GetActualScreen(GetCurrentActivePanel());
	if( index == MENU_UNIQUE_ID_INVALID )
	{
		Quitf(ERR_GUI_ACTIVE_PANEL,"Unfortunately for you, the currently active panel '%s', does not have a corresponding XML entry. Either add one, or re-evaluate the use of GetCurrentActivePanelData(). This is unrecoverable, so we'll crash now to embarrass you.", GetCurrentActivePanel().GetParserName());
	}
	return GetScreenDataByIndex( index );
}

bool CPauseMenu::GetCurrentActivePanelHasFlag(eMenuScreenBits kFlag)
{
	MenuArrayIndex index = GetActualScreen(GetCurrentActivePanel());
	if( index != MENU_UNIQUE_ID_INVALID )
	{
		return GetScreenDataByIndex( index ).HasFlag(kFlag);
	}

	return false;
}

bool CPauseMenu::GetCurrentScreenHasFlag(eMenuScreenBits kFlag)
{
	if( IsCurrentScreenValid() )
	{
		return GetCurrentScreenData().HasFlag(kFlag);
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetRenderData(PauseMenuRenderDataExtra& renderData)
{
	// copy over the base values
	renderData.Set( sm_RenderData );

	renderData.bIsActive					= sm_bActive;
	renderData.bClosingDown					= sm_bClosingDown;
	renderData.bWaitOnCalibrationScreen		= sm_bWaitOnDisplayCalibrationScreen;
	renderData.bWaitOnCreditsScreen			= sm_bWaitOnCreditsScreen;
#if RSG_PC
	renderData.bWaitOnPCGamepadScreen		= sm_bWaitOnPCGamepadCalibrationScreen;
#endif
	renderData.bDisableSpinner			    = sm_bDisableSpinner;
	renderData.bHideMenu					= sm_bMaxPayneMode;
	renderData.bRenderContent				= sm_bRenderContent;
	renderData.bRenderHeader				= (!sm_bNavigatingContent || GetCurrentScreen() != MENU_UNIQUE_ID_MAP);
	renderData.bGalleryLoadingImage			= sm_bGalleryLoadingImage;
	renderData.bGalleryMaximizeEnabled		= sm_bGalleryMaximizeEnabled;
	renderData.bRenderCustomMap				= sMiniMapMenuComponent.IsActive() && (CPauseMenu::GetCurrentScreen() == MENU_UNIQUE_ID_CORONA_SETTINGS || CPauseMenu::GetCurrentScreen() == MENU_UNIQUE_ID_RACE_INFO );

	renderData.bRenderBackgroundThisFrame	= CScriptHud::bRenderFrontendBackgroundThisFrame;
	renderData.bWarningActive				= CWarningScreen::IsActive();
	renderData.bCustomWarningActive			= SReportMenu::GetInstance().IsActive();
	renderData.bSocialClubActive			= SocialClubMenu::IsActive();
	renderData.bStoreActive					= cStoreScreenMgr::IsStoreMenuOpen();
	renderData.bRestartingConditions		= sm_bRestarting || sm_bSetupStartingPane || (sm_bClosingDown && !sm_iCallbacksPending) || sm_iLoadingAssetsPhase!=PMLP_DONE || !sm_bRenderMenus;
	renderData.bForceSpinner				= sm_bClosingDown && sm_PauseMenuCloseDelay > (STREAMING_FRAMES + 1);  // give it a few more frames before it shows up
	#if RSG_PC
	renderData.bRenderBlackBackground		= sm_iExitTimer != 0 || sm_bWantsToExitGame;
	#endif
	renderData.bCurrentScreenValid			= false;
	renderData.bVersionHasHeadings			= false;
	renderData.bRenderInstructionalButtons	= false;
	renderData.bVersionHasNoBackground		= false;
	renderData.fAlphaFader = 1.0f;
	renderData.fSizeScalar = 1.0f;
	if( renderData.bIsActive && DynamicMenuExists() )
	{
		GetDynamicPauseMenu()->GetScaleEffectPercent(renderData.fAlphaFader, renderData.fBlipAlphaFader, renderData.fSizeScalar);

		renderData.bVersionHasHeadings = MenuVersionHasHeadings();
		renderData.bVersionHasNoBackground = GetCurrentMenuVersionHasFlag(kNoBackground);
		if( !WaitingForForceDropIntoMenu() && GetDynamicPauseMenu()->GetLastNumberOfInstructionButtonsDrawn() > 0 
			&& !sm_aMenuState.empty() && sm_aMenuState.Top().iMenuceptionDir == kMENUCEPT_LIMBO )
		{
			if( (!IsNavigatingContent() || sm_bRenderContent) 
				|| (IsInGalleryScreen() && sm_bGalleryLoadingImage && sm_bGalleryDisplayInstructionalButtons)
				|| GetCurrentMenuVersionHasFlag(kForceButtonRender) )
				renderData.bRenderInstructionalButtons = true;
		}

		if( IsCurrentScreenValid() )
		{
			renderData.bCurrentScreenValid = true;

			// through the power of virtualization, we'll end up calling the proper Render function
			// yay overhead!
			if( GetCurrentScreenData().HasDynamicMenu() )
				renderData.DynamicMenuRender = datCallback(MFA1(CMenuBase::Render), GetCurrentScreenData().GetDynamicMenu(), 0, true);
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RenderBackgroundContent
// PURPOSE: renders the pause menu background
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RenderBackgroundContent(const PauseMenuRenderDataExtra& renderData)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	bool bRenderBackgroundOnly = renderData.bRenderBackgroundThisFrame;

	if (!renderData.bRenderBackgroundThisFrame)
	{
		if (!renderData.bIsActive ||
			renderData.bSocialClubActive ||
			renderData.bWaitOnCalibrationScreen ||
			renderData.bWaitOnCreditsScreen WIN32PC_ONLY(|| renderData.bWaitOnPCGamepadScreen))
		{
			return;
		}
	}

	PF_AUTO_PUSH_TIMEBAR("Scaleform Movie Render (PauseMenu Background)");

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState(CHudTools::GetStandardSpriteBlendState());

	if (!bRenderBackgroundOnly)
	{
		if ( renderData.bRestartingConditions || renderData.bCustomWarningActive )
		{
			bRenderBackgroundOnly = true;
		}

		for (s32 iMovieCount = 0; iMovieCount < MAX_PAUSE_MENU_BASE_MOVIES; iMovieCount++)
		{
			if( !sm_iBaseMovieId[iMovieCount].IsFree() && !sm_iBaseMovieId[iMovieCount].IsActive() )
			{
				bRenderBackgroundOnly = true;  // no render if any are not available
			}
		}
	}

	
	RenderBackground(renderData);  // not until actionscript has told us it is ready for stuff to be rendered

	if (bRenderBackgroundOnly)
		return;

	if (renderData.bRenderContent)
	{
		RenderSpecificScreenBackground(renderData);
	}

	if(sm_pDynamicPause)
		sm_pDynamicPause->RenderBGLayer(renderData);

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	CSprite2d::ClearRenderState();
	CShaderLib::SetGlobalEmissiveScale(1.0f);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RenderForegroundContent
// PURPOSE: renders the pause menu foreground content
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RenderForegroundContent(const PauseMenuRenderDataExtra& renderData)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __DEV
	if( sm_bDebugFileFail )
	{
		CTextLayout StreamingVisualiseDebugText;
		StreamingVisualiseDebugText.SetScale(Vector2(0.5f, 1.0f));
		StreamingVisualiseDebugText.SetColor( Color32(0xFFE15050) );
		StreamingVisualiseDebugText.Render( Vector2(0.27f, 0.35f), "COULDN'T PARSE PAUSEMENU.XML");
		StreamingVisualiseDebugText.SetScale(Vector2(0.35f, 0.7f));
		StreamingVisualiseDebugText.Render( Vector2(0.41f, 0.40f), "(reload the file)");
		CText::Flush();
	}
#endif

	bool bRenderBackgroundOnly = renderData.bRenderBackgroundThisFrame;
	bool bRenderSpinner = !bRenderBackgroundOnly;

	if( !renderData.bForceSpinner )
	{
		if( (!renderData.bIsActive && !bRenderBackgroundOnly) || renderData.bCustomWarningActive || renderData.bSocialClubActive || renderData.bStoreActive || renderData.bClosingDown)
			return;
	}

	PF_AUTO_PUSH_TIMEBAR("Scaleform Movie Render (PauseMenu Foreground)");

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState(CHudTools::GetStandardSpriteBlendState());
	CShaderLib::SetGlobalEmissiveScale(1.0f,true);
	grcLightState::SetEnabled(false);

	if (renderData.bWaitOnCalibrationScreen WIN32PC_ONLY(|| renderData.bWaitOnPCGamepadScreen))
	{
#if RSG_PC
		// it'll only be one or the other ...can't have both on at same time
		if (renderData.bWaitOnPCGamepadScreen)
		{
			CPCGamepadCalibration::Render();
		}
		else
#endif
		{
			CDisplayCalibration::Render();
		}

		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		CSprite2d::ClearRenderState();

		return;
	}

	if (renderData.bWaitOnCreditsScreen)
	{
		CSprite2d::DrawRectGUI(fwRect(0.0f, 0.0f, 1.0f, 1.0f), Color32(0, 0, 0, 255));
		return;
	}

	if (!bRenderBackgroundOnly)
	{
		if ( renderData.bRestartingConditions )
		{
			bRenderBackgroundOnly = true;
		}

		for (s32 iMovieCount = 0; iMovieCount < MAX_PAUSE_MENU_BASE_MOVIES; iMovieCount++)
		{
			if( !sm_iBaseMovieId[iMovieCount].IsFree() && !sm_iBaseMovieId[iMovieCount].IsActive() )
			{
				bRenderBackgroundOnly = true;  // no render if any are not available
			}
		}
	}

	if( renderData.bForceSpinner )
	{
		RenderAnimatedSpinner();
		
	}
	else if (bRenderBackgroundOnly)
	{
		if ( bRenderSpinner && !renderData.bClosingDown )  // if faded background in and still not streamed, then render the spinner
		{
			if (renderData.bVersionHasHeadings && s_iSpinnerTimer.IsStarted() && s_iSpinnerTimer.IsComplete(TIME_TO_WAIT_UNTIL_INITIAL_LOADING_SPINNER_APPEARS,false))
			{
				RenderAnimatedSpinner();
			}
		}

		return;
	}
	else
	{
		if( !renderData.bHideMenu )
		{

			if( (renderData.bRenderContent || renderData.bGalleryMaximizeEnabled ) )
			{
				RenderSpecificScreenOverlays(renderData);
			}

			if (renderData.bRenderContent && renderData.bRenderCustomMap)
			{
				sMiniMapMenuComponent.RenderMap(renderData);
			}

			if( renderData.bRenderHeader )
			{
				GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER).AffectRenderSize(renderData.fSizeScalar);
				GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER).Render(false, renderData.fAlphaFader);
			}

			bool bSpinnerRendered = false;

			if( renderData.bRenderContent )
			{
				GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).AffectRenderSize(renderData.fSizeScalar);
				GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).Render(false, renderData.fAlphaFader);

				s_iSpinnerTimer.Start();
			}
			else if( !renderData.bDisableSpinner && (!renderData.bGalleryMaximizeEnabled || renderData.bGalleryLoadingImage) )
			{
				if ( s_iSpinnerTimer.IsComplete(TIME_TO_WAIT_UNTIL_SPINNER_APPEARS,false))
				{
					if( renderData.bVersionHasHeadings )
					{
						RenderAnimatedSpinner();
						bSpinnerRendered = true;
					}
				}
			}


			if( !bSpinnerRendered )
			{
				const char* pszTuningToUse = GetCurrentMenuVersionHasFlag(kUseAlternateContentPos) ? "PAUSE_MENU_SP_CONTENT_ALT" : PAUSEMENU_FILENAME_CONTENT;
				const SGeneralPauseDataConfig& rPauseMenuContent = GetMenuArray().GeneralData.MovieSettings[pszTuningToUse];
				Vector2 movieSize = rPauseMenuContent.vSize;
				Vector2 moviePos =  rPauseMenuContent.vPos ;
				
				CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(rPauseMenuContent.HAlign), &moviePos, &movieSize);

				for(int i=0; i<PauseMenuRenderData::MAX_SPINNERS;++i)
				{
					if( renderData.iLoadingSpinnerDesired[i] != PauseMenuRenderData::NO_SPINNER )
					{
						Vector2 vPos( sm_PersistentData.Spinner.Offsets[ renderData.iLoadingSpinnerDesired[i] ] * movieSize);
						vPos += moviePos;
						RenderAnimatedSpinner(vPos);
					}
				}
			}
		
			if( renderData.bRenderHeader )
			{
				RenderGeneralOverlays(renderData);
			}
		}

		if( renderData.bRenderInstructionalButtons )
		{
			GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).Render(false, renderData.fAlphaFader);
		}
	}

	if(!renderData.bForceSpinner && uiVerify(sm_pDynamicPause) && !renderData.bHideMenu)
		sm_pDynamicPause->Render();

#if RSG_PC
	if (renderData.bRenderBlackBackground)
	{
		CSprite2d::DrawRectGUI(fwRect(0.0f, 0.0f, 1.0f, 1.0f), Color32(0, 0, 0, 255));
	}
#endif

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	CSprite2d::ClearRenderState();
	CShaderLib::SetGlobalEmissiveScale(1.0f);
}

void CPauseMenu::SetGalleryLoadingTexture(bool bLoading, bool bDisplayInstructionalButtons)
{ 
	sm_bGalleryLoadingImage = bLoading; 
	sm_bRenderContent = !bLoading;
	sm_bGalleryDisplayInstructionalButtons = bDisplayInstructionalButtons;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::GetContentMovieId
// PURPOSE:	returns the content movie id
/////////////////////////////////////////////////////////////////////////////////////
s32 CPauseMenu::GetContentMovieId()
{
	return (GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).GetMovieID());
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::GetHeaderMovieId
// PURPOSE:	returns the header movie id
/////////////////////////////////////////////////////////////////////////////////////
s32 CPauseMenu::GetHeaderMovieId()
{
	return (GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER).GetMovieID());
}

Vector3 CPauseMenu::GetPosition()
{
	return CScaleformMgr::GetMoviePos(GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).GetMovieID());
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::GetScreenCodeWantsScriptToControl
// PURPOSE:	returns the screen that code wants script to control
/////////////////////////////////////////////////////////////////////////////////////
s32 CPauseMenu::GetScreenCodeWantsScriptToControl()
{
	s32 iReturnValue = sm_iCodeWantsScriptToControlScreen.GetValue();
	sm_iCodeWantsScriptToControlScreen = MENU_UNIQUE_ID_INVALID;

	return iReturnValue;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::GetEventOccurredUniqueId
// PURPOSE:	returns the event occurred details
/////////////////////////////////////////////////////////////////////////////////////
s32 CPauseMenu::GetEventOccurredUniqueId(s32 iNum)
{
	sm_bMenuTriggerEventOccurred = false;
	sm_bMenuLayoutChangedEventOccurred = false;

	if (Verifyf(iNum < 3, "PauseMenu: Invalid EventOccurredUniqueId value %d", iNum))
	{
		return sm_iMenuEventOccurredUniqueId[iNum];
	}

	return -1;  // invalid
}


s8 CPauseMenu::GetColumnForSpinner()
{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (IsLoadGameOptionHighlighted() || IsInLoadGamePanel()
		|| IsUploadSavegameOptionHighlighted() || IsInUploadSavegamePanel())
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (IsLoadGameOptionHighlighted() || IsInLoadGamePanel())
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	{
		return PM_COLUMN_MIDDLE_RIGHT;
	}

	return PM_COLUMN_MIDDLE;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetFlaggedAsBusy
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetFlaggedAsBusy(bool bBusy)
{
	const bool bAlreadyDisplayingASpinner = (sm_SpinnerColumnForFlaggedAsBusy != PauseMenuRenderData::NO_SPINNER);
	if (bAlreadyDisplayingASpinner != bBusy)
	{
		if (GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).IsActive())
		{
			if (bBusy)
			{
				uiAssertf(sm_SpinnerColumnForFlaggedAsBusy == PauseMenuRenderData::NO_SPINNER, "CPauseMenu::SetFlaggedAsBusy - Before displaying a spinner, expected sm_SpinnerColumnForFlaggedAsBusy to be NO_SPINNER but it's %d", sm_SpinnerColumnForFlaggedAsBusy);

				sm_SpinnerColumnForFlaggedAsBusy = GetColumnForSpinner();
				SetBusySpinner(true, sm_SpinnerColumnForFlaggedAsBusy);
			}
			else
			{
				if (uiVerifyf(sm_SpinnerColumnForFlaggedAsBusy != PauseMenuRenderData::NO_SPINNER, "CPauseMenu::SetFlaggedAsBusy - about to clear the spinner so didn't expect it to be NO_SPINNER"))
				{
					SetBusySpinner(false, sm_SpinnerColumnForFlaggedAsBusy);
					sm_SpinnerColumnForFlaggedAsBusy = PauseMenuRenderData::NO_SPINNER;
				}
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::LockMenuControl
// PURPOSE:	Locks all menu control
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::LockMenuControl(const bool bFlagAsBusy)
{
	uiDebugf3("CODE DOES NOT HAVE CONTROL OF FRONTEND");

	if (IsNavigatingContent() && !sm_bMenuControlIsLocked)
	{
		sm_bMenuControlChangedThisFrame = !sm_bMenuControlChangedThisFrame;
	}

	sm_bMenuControlIsLocked = true;

	if (bFlagAsBusy)
	{
		SetFlaggedAsBusy(true);
	}


}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UnlockMenuControl
// PURPOSE:	Unlocks all menu control
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::UnlockMenuControl()
{
	uiDebugf3("CODE HAS CONTROL OF FRONTEND");

	if (IsNavigatingContent() && sm_bMenuControlIsLocked)
	{
		sm_bMenuControlChangedThisFrame = !sm_bMenuControlChangedThisFrame;
	}

	sm_bMenuControlIsLocked = false;
	sm_bLockTheAcceptButton = false;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	sm_bDelayedEntryToExportSavegameMenu = false;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	SetFlaggedAsBusy(false);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::GetActualScreen
// PURPOSE:	Gets the actual screen array id from a unique screen id
/////////////////////////////////////////////////////////////////////////////////////
MenuArrayIndex CPauseMenu::GetActualScreen(MenuScreenId iUniqueScreen)
{
	atArray<CMenuScreen>& screens = GetMenuArray().MenuScreens;

	// implementing atArray's BinarySearch but with a different search criteria
	int low = 0;
	int high = screens.GetCount()-1;
	while (low <= high) {
		int mid = (low + high) >> 1;
		s32 curValue = screens[mid].MenuScreen.GetValue();

		if( curValue == iUniqueScreen.GetValue() )
			return mid;

		else if ( curValue > iUniqueScreen.GetValue() )
			high = mid-1;

		else
			low = mid+1;
	}

	return MENU_UNIQUE_ID_INVALID;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::FillContent
// PURPOSE:	Fill content from "special/custom" screens
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::FillContent(MenuScreenId iPaneId)
{
	ASSERT_ONLY( bool bTossedToDynamic = false; )
	
	if( IsCurrentScreenValid() )
	{
		if( GetCurrentScreenData().HasDynamicMenu() )
		{
			uiSpew("@@@@@ POPULA: %s", GetCurrentScreenData().MenuScreen.GetParserName());
			ASSERT_ONLY( bTossedToDynamic = true; )
			if( GetCurrentScreenData().GetDynamicMenu()->Populate(iPaneId) )
				return;
		}
		else
			uiSpew("????? POPULA: %s", GetCurrentScreenData().MenuScreen.GetParserName());
	}
	else
		uiSpew("????? POPULA: -invalid menu-");


	switch (iPaneId.GetValue())
	{
		case MENU_UNIQUE_ID_LOAD_GAME:
		{
			PopulateLoadGameMenu(false);
			break;
		}

		case MENU_UNIQUE_ID_SAVE_GAME:
		{
			PopulateSaveGameMenu();
			break;
		}

		case MENU_UNIQUE_ID_PROCESS_SAVEGAME:
		{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
			PopulateUploadSavegameMenu(false);
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
			uiErrorf("CPauseMenu::FillContent - __ALLOW_EXPORT_OF_SP_SAVEGAMES is 0 so didn't expect MENU_UNIQUE_ID_PROCESS_SAVEGAME - do nothing");
			uiAssertf(0, "CPauseMenu::FillContent - __ALLOW_EXPORT_OF_SP_SAVEGAMES is 0 so didn't expect MENU_UNIQUE_ID_PROCESS_SAVEGAME - do nothing");
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
			break;
		}

		case MENU_UNIQUE_ID_GALLERY:
		{
			// call a function here to populate all the gallery items
			break;
		}

		case MENU_UNIQUE_ID_HOME_MISSION:
		case MENU_UNIQUE_ID_HOME_HELP:
		case MENU_UNIQUE_ID_HOME_BRIEF:
		case MENU_UNIQUE_ID_HOME_FEED:
		case MENU_UNIQUE_ID_HOME_NEWSWIRE:
		{
			// This happens sometimes. It's fine. The menu will populate correctly on the next frame.
			break;
		}

		default:
		{
			uiAssertf(bTossedToDynamic, "Unsupported menu for fillcontent %d!", iPaneId.GetValue());
		}
	}
}

PM_COLUMNS CPauseMenu::GetColumnForSavegameMenu()
{
	if (IsInSaveGameMenus())
	{
		return PM_COLUMN_LEFT;
	}

	return PM_COLUMN_MIDDLE;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::InsertSaveGameIntoMenu
// PURPOSE:	Inserts savegame data into a menu
//			(called from savegame code)
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::InsertSaveGameIntoMenu(s32 iIndex, char *pSaveGameNameGxt, char *pSaveGameDateGxt, bool bActive)
{
	if (!sm_bActive)
		return;

	PM_COLUMNS column = GetColumnForSavegameMenu();

	eMenuScreen menuScreen = MENU_UNIQUE_ID_SAVE_GAME_LIST;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (IsUploadSavegameOptionHighlighted() || IsInUploadSavegamePanel())
	{
		menuScreen = MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST;
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	if( CScaleformMenuHelper::SET_DATA_SLOT(column, iIndex, menuScreen + PREF_OPTIONS_THRESHOLD, iIndex, OPTION_DISPLAY_NONE, 0, bActive, pSaveGameNameGxt, false) )
	{
		if( pSaveGameDateGxt )
			CScaleformMgr::AddParamString(pSaveGameDateGxt);
		CScaleformMgr::EndMethod();
	}
}

void CPauseMenu::ClearSaveGameMenu()
{
	PM_COLUMNS column = GetColumnForSavegameMenu();

	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(column);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::FlagSaveGameMenuAsFinished
// PURPOSE: Flags the menu system as finished with compiling savegame menus
//			(called from savegame code)
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::FlagSaveGameMenuAsFinished(s32 iMenuItemToHighlight/* = -1*/)
{
	if (!sm_bActive)
		return;

	if (IsInSaveGameMenus())
	{
		// start inside the menu on the savegame screen
		CScaleformMenuHelper::DISPLAY_DATA_SLOT( PM_COLUMN_LEFT );

		if (sm_bSaveMenuIsBeingRedrawnAfterDeletingASavegame)
		{
			CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_LEFT, true, false);

			if (iMenuItemToHighlight != -1)
			{
				CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, iMenuItemToHighlight);
			}
		}
		else
		{	//	Graeme - this seems to be what is causing a second "delete savegame" to be triggered immediately
			MENU_SHIFT_DEPTH(kMENUCEPT_DEEPER);
		}

	}
	else
	{
		CScaleformMenuHelper::DISPLAY_DATA_SLOT( PM_COLUMN_MIDDLE );

		if (sm_bSaveMenuIsBeingRedrawnAfterDeletingASavegame && !GetNoValidSaveGameFiles())
		{
			// Maintain focus on the save game list after deleting a save if there are still valid saves
			CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_MIDDLE, true, false);

			if (iMenuItemToHighlight != -1)
			{
				// Keep the correct save highlighted - this stops the highlight from resetting to the top item after deleting a save
				CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_MIDDLE, iMenuItemToHighlight);
			}
		}
	}

	sm_bSaveMenuIsBeingRedrawnAfterDeletingASavegame = false;	// Reset this to prevent a save from highlighting in the load game while in MENU_UNIQUE_ID_LOAD_GAME after deleting a save

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (sm_bDelayedEntryToExportSavegameMenu)
	{
		if (uiVerifyf(GetCurrentActivePanel() == MENU_UNIQUE_ID_PROCESS_SAVEGAME, "CPauseMenu::FlagSaveGameMenuAsFinished - sm_bDelayedEntryToExportSavegameMenu is set so expected GetCurrentActivePanel() to return MENU_UNIQUE_ID_PROCESS_SAVEGAME, but it returned %d", GetCurrentActivePanel().GetValue()))
		{
			MENU_SHIFT_DEPTH(kMENUCEPT_DEEPER);
		}

		sm_bDelayedEntryToExportSavegameMenu = false;
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	UnlockMenuControl();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::PopulateLoadGameMenu
// PURPOSE: populates the load game menu (called from savegame code)
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::PopulateLoadGameMenu(bool bRepopulate)
{
	uiDisplayf("CPauseMenu::PopulateLoadGameMenu has been called");
	if (bRepopulate || (!sm_bSaveGameListSync) )
	{
		uiDisplayf("CPauseMenu::PopulateLoadGameMenu - sm_bSaveGameListSync is %s. bRepopulate is %s", sm_bSaveGameListSync?"true":"false", bRepopulate?"true":"false");

		ResetMenuItemTriggered();

		if (CScriptHud::bUsingMissionCreator)
		{
			CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("CANT_LOAD_CRTR"), TheText.Get("PM_PANE_LOA"));
			uiDisplayf("CPauseMenu::PopulateLoadGameMenu - bUsingMissionCreator is true so don't list savegames");
		}
		else
		{
			sm_bQueueManualLoadASAP = true;
			uiDisplayf("CPauseMenu::PopulateLoadGameMenu - setting sm_bQueueManualLoadASAP to true");

			SetFlaggedAsBusy(true);
		}

//	Graeme - only lock the button that selects the child menu (X on PS3, A on 360)
//		LockMenuControl(true);
		sm_bLockTheAcceptButton = true;
		sm_bSaveGameListSync = true;

		uiDisplayf("CPauseMenu::PopulateLoadGameMenu - sm_bSaveGameListSync has been set to true");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::PopulateSaveGameMenu
// PURPOSE: populates the save game menu (called from savegame code)
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::PopulateSaveGameMenu()
{
	if (!sm_bSaveGameListSync)
	{
//		ResetMenuItemTriggered();

		sm_bQueueManualSaveASAP = true;
		uiDisplayf("CPauseMenu::PopulateSaveGameMenu - setting sm_bQueueManualSaveASAP to true");

//		SUIContexts::Activate(SELECT_STORAGE_DEVICE_CONTEXT);
//		SUIContexts::Activate(DELETE_SAVEGAME_CONTEXT);
//		RedrawInstructionalButtons();

		//		LockMenuControl(true);	//	Graeme commented this out. CPauseMenu::Update() calls UnlockMenuControl() immediately afterwards so it doesn't really have much effect.
		sm_bSaveGameListSync = true;
	}
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CPauseMenu::PopulateUploadSavegameMenu(bool bRepopulate)
{
	uiDisplayf("CPauseMenu::PopulateUploadSavegameMenu has been called");
	if (bRepopulate || (!sm_bSaveGameListSync) )
	{
		uiDisplayf("CPauseMenu::PopulateUploadSavegameMenu - sm_bSaveGameListSync is %s. bRepopulate is %s", sm_bSaveGameListSync?"true":"false", bRepopulate?"true":"false");

		ResetMenuItemTriggered();

		if (CScriptHud::bUsingMissionCreator)
		{
			CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("CANT_PROC_CRTR"), TheText.Get("PM_PANE_PROC"));
			uiDisplayf("CPauseMenu::PopulateUploadSavegameMenu - bUsingMissionCreator is true so don't list savegames");
		}
		else
		{
			sm_bQueueUploadSavegameASAP = true;
			uiDisplayf("CPauseMenu::PopulateUploadSavegameMenu - setting sm_bQueueUploadSavegameASAP to true");

			SetFlaggedAsBusy(true);
		}

		sm_bLockTheAcceptButton = true;
		sm_bSaveGameListSync = true;

		uiDisplayf("CPauseMenu::PopulateUploadSavegameMenu - sm_bSaveGameListSync has been set to true");
	}
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

void CPauseMenu::QueueSavegameOperationIfNecessary()
{
	if (sm_bQueueManualLoadASAP)
	{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		if (sm_bUseManualLoadMenuToExportSavegame)
		{
			if (sm_ExportSPSave.GetStatus() != MEM_CARD_BUSY)
			{
				uiDisplayf("CPauseMenu::QueueSavegameOperationIfNecessary - sm_bQueueManualLoadASAP and sm_bUseManualLoadMenuToExportSavegame are set. About to queue an ExportSPSave");
				sm_ExportSPSave.Init();
				if (!CGenericGameStorage::PushOnToSavegameQueue(&sm_ExportSPSave))
				{
					uiAssertf(0, "CPauseMenu::QueueSavegameOperationIfNecessary - CGenericGameStorage::PushOnToSavegameQueue failed (ExportSPSave)");
					return;
				}
				sm_bQueueManualLoadASAP = false;
			}
		}
		else
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

		{
			if (CPauseMenu::sm_ManualLoadStructure.GetStatus() != MEM_CARD_BUSY)
			{
				uiDisplayf("CPauseMenu::QueueSavegameOperationIfNecessary - sm_bQueueManualLoadASAP is set. About to queue a manual load");
				CPauseMenu::sm_ManualLoadStructure.Init();
				if (!CGenericGameStorage::PushOnToSavegameQueue(&CPauseMenu::sm_ManualLoadStructure))
				{
					uiAssertf(0, "CPauseMenu::QueueSavegameOperationIfNecessary - CGenericGameStorage::PushOnToSavegameQueue failed (Manual Load)");
					return;
				}
				sm_bQueueManualLoadASAP = false;
			}
		}
	}

	if (sm_bQueueManualSaveASAP)
	{
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		if (sm_bUseManualSaveMenuToImportSavegame)
		{
			if (sm_ImportSPSave.GetStatus() != MEM_CARD_BUSY)
			{
				uiDisplayf("CPauseMenu::QueueSavegameOperationIfNecessary - sm_bQueueManualSaveASAP and sm_bUseManualSaveMenuToImportSavegame are set. About to queue an ImportSPSave");
				sm_ImportSPSave.Init();
				if (!CGenericGameStorage::PushOnToSavegameQueue(&sm_ImportSPSave))
				{
					uiAssertf(0, "CPauseMenu::QueueSavegameOperationIfNecessary - CGenericGameStorage::PushOnToSavegameQueue failed (ImportSPSave)");
					return;
				}
				sm_bQueueManualSaveASAP = false;
			}
		}
		else
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

		{
			if (CPauseMenu::sm_ManualSaveStructure.GetStatus() != MEM_CARD_BUSY)
			{
				uiDisplayf("CPauseMenu::QueueSavegameOperationIfNecessary - sm_bQueueManualSaveASAP is set. About to queue a manual save");
				CPauseMenu::sm_ManualSaveStructure.Init();
				if (!CGenericGameStorage::PushOnToSavegameQueue(&CPauseMenu::sm_ManualSaveStructure))
				{
					uiAssertf(0, "CPauseMenu::QueueSavegameOperationIfNecessary - CGenericGameStorage::PushOnToSavegameQueue failed (Manual Save)");
					return;
				}
				sm_bQueueManualSaveASAP = false;
			}
		}
	}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (sm_bQueueUploadSavegameASAP)
	{
		if (sm_ExportSPSave.GetStatus() != MEM_CARD_BUSY)
		{
			uiDisplayf("CPauseMenu::QueueSavegameOperationIfNecessary - sm_bQueueUploadSavegameASAP is set. About to queue an ExportSPSave");
			sm_ExportSPSave.Init();
			if (!CGenericGameStorage::PushOnToSavegameQueue(&sm_ExportSPSave))
			{
				uiAssertf(0, "CPauseMenu::QueueSavegameOperationIfNecessary - CGenericGameStorage::PushOnToSavegameQueue failed (ExportSPSave)");
				return;
			}
			sm_bQueueUploadSavegameASAP = false;
		}
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
}

void CPauseMenu::GetFullStatName(char* szStatName, const sStatData* pActualStat, const sUIStatData* UNUSED_PARAM(pStat) )
{
	// Test to see if its a unique stat.
	if (!pActualStat)
	{
		// Convert the name of the stat to the proper character.
		CPlayerInfo *pPlayer = CGameWorld::FindLocalPlayer()->GetPlayerInfo();

		//if (!NetworkInterface::IsGameInProgress())
		{
			if (pPlayer->GetPlayerPed()->GetPedModelInfo()->GetModelNameHash() == MI_PLAYERPED_PLAYER_ZERO.GetName().GetHash()) 
			{
				::snprintf(szStatName,64,"SP0_%s","tempToFixBuild");
			}
			else if (pPlayer->GetPlayerPed()->GetPedModelInfo()->GetModelNameHash() == MI_PLAYERPED_PLAYER_ONE.GetName().GetHash()) 
			{
				::snprintf(szStatName,64,"SP1_%s","tempToFixBuild");
			}
			else
			{
				::snprintf(szStatName,64,"SP2_%s","tempToFixBuild");
			}
		}

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		pActualStat = statsDataMgr.GetStat(HASH_STAT_ID(szStatName));
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::TriggerEvent
// PURPOSE: triggers off an event in code based on a unique id
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::TriggerEvent(MenuScreenId iUniqueId, s32 iMenuIndex, MenuScreenId newMenuIndex)
{
	CPauseMenu::iLoadNewGameTrigger = -1;
	
	if( IsCurrentScreenValid() ) 
	{
		CMenuScreen& curScreen = GetCurrentScreenData();
		// context menus get first crack
		if( curScreen.HandleContextMenuTriggers(iUniqueId, iMenuIndex) )
			return;

		// check for menuception
		if( iUniqueId == MENU_UNIQUE_ID_INCEPT_TRIGGER )
		{
			MenuceptionGoDeeper( MenuScreenId(iMenuIndex) );
			return;
		}

		// then finally dynamic menus
		if( curScreen.HasDynamicMenu() )
			curScreen.GetDynamicMenu()->TriggerEvent(iUniqueId, iMenuIndex);
	}

	sm_bMenuTriggerEventOccurred = true;
	sm_iMenuEventOccurredUniqueId[0] = iUniqueId.GetValue();
	sm_iMenuEventOccurredUniqueId[1] = -1;
	sm_iMenuEventOccurredUniqueId[2] = iMenuIndex;

	if (iUniqueId == MENU_UNIQUE_ID_NEW_GAME)
	{
		sm_bWaitOnNewGameConfirmationScreen = true;
		return;
	}

	#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	if (iUniqueId == MENU_UNIQUE_ID_IMPORT_SAVEGAME)
	{
		sm_bWaitOnImportConfirmationScreen = true;
		return;
	}
	#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	#if RSG_PC
	if (iUniqueId == MENU_UNIQUE_ID_EXIT_TO_WINDOWS)
	{
		sm_bWaitOnExitToWindowsConfirmationScreen = true;
		return;
	}
	#endif

	if (iUniqueId == MENU_UNIQUE_ID_LEAVE_GAME)
	{
		uiAssertf(0, "CPauseMenu::TriggerEvent - just checking if MENU_UNIQUE_ID_LEAVE_GAME is ever used now - Graeme");
		SetClosingAction(CA_LeaveGame);
		Close();
		return;
	}

	if (iUniqueId == MENU_UNIQUE_ID_SAVE_GAME_LIST)
	{
		iLoadNewGameTrigger = iMenuIndex;
		return;
	}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (iUniqueId == MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST)
	{
		iLoadNewGameTrigger = iMenuIndex;
		return;
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	if (iUniqueId == MENU_UNIQUE_ID_LOAD_GAME)
	{	//	For now, I'm going to assume that if we're going forward from MENU_UNIQUE_ID_LOAD_GAME then we're going to MENU_UNIQUE_ID_SAVE_GAME_LIST
		//	Jeff is going to modify TRIGGER_PAUSE_MENU_EVENT so that it passes the unique Id of the new menu (B*983706)
		uiAssertf(newMenuIndex == MENU_UNIQUE_ID_SAVE_GAME_LIST, "CPauseMenu::TriggerEvent - moving forward from MENU_UNIQUE_ID_LOAD_GAME so expected newMenuIndex to be MENU_UNIQUE_ID_SAVE_GAME_LIST, but it's %d", newMenuIndex.GetValue());
		SetCurrentPane(MENU_UNIQUE_ID_SAVE_GAME_LIST);

		SUIContexts::Activate(DELETE_SAVEGAME_CONTEXT);
	}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (iUniqueId == MENU_UNIQUE_ID_PROCESS_SAVEGAME)
	{
		uiAssertf(newMenuIndex == MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST, "CPauseMenu::TriggerEvent - moving forward from MENU_UNIQUE_ID_PROCESS_SAVEGAME so expected newMenuIndex to be MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST, but it's %d", newMenuIndex.GetValue());
		SetCurrentPane(MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST);

//		SUIContexts::Activate(DELETE_SAVEGAME_CONTEXT);
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	if ( (iUniqueId == MENU_UNIQUE_ID_GAME) && (newMenuIndex == MENU_UNIQUE_ID_LOAD_GAME) )
	{
		SetCurrentPane(newMenuIndex);
		GenerateMenuData(newMenuIndex);
	}

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	if ( (iUniqueId == MENU_UNIQUE_ID_IMPORT_SAVEGAME) && (newMenuIndex == MENU_UNIQUE_ID_IMPORT_SAVEGAME) )
	{
		SetCurrentPane(newMenuIndex);
		DisplayImportAlertMessage();
	}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if ( (iUniqueId == MENU_UNIQUE_ID_GAME) && (newMenuIndex == MENU_UNIQUE_ID_PROCESS_SAVEGAME) )
	{
		SetCurrentPane(newMenuIndex);
		GenerateMenuData(newMenuIndex);
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_SP_CREDITS
	if (((iUniqueId == MENU_UNIQUE_ID_CREDITS) && (newMenuIndex == MENU_UNIQUE_ID_LEGAL)) ||
		((iUniqueId == MENU_UNIQUE_ID_LEGAL) && (newMenuIndex == MENU_UNIQUE_ID_CREDITS)) ||
		((iUniqueId == MENU_UNIQUE_ID_CREDITS_LEGAL) && (newMenuIndex == MENU_UNIQUE_ID_CREDITS_LEGAL)))
	{
		SetCurrentPane(newMenuIndex);
		DisplayCreditsMessage(newMenuIndex);
	}
#endif // __ALLOW_SP_CREDITS

	if ( (iUniqueId == MENU_UNIQUE_ID_GAME) && (newMenuIndex == MENU_UNIQUE_ID_NEW_GAME) )
	{
		SetCurrentPane(newMenuIndex);
		DisplayNewGameAlertMessage();
	}

	if ( (iUniqueId == MENU_UNIQUE_ID_EXIT_TO_WINDOWS) )
	{
		SetCurrentPane (newMenuIndex);

		#if RSG_PC
		DisplayQuitGameAlertMessage();
		#endif
	}

	if (iUniqueId == MENU_UNIQUE_ID_SAVE_GAME)
	{
		iLoadNewGameTrigger = iMenuIndex;
		return;
	}

	if (iUniqueId == MENU_UNIQUE_ID_HELP)
	{
#if RSG_DURANGO
		if(CLiveManager::IsSignedIn() == false)
		{
			sm_bHelpLoginPromptActive = true;
		}
		else
#endif // RSG_DURANGO
		{
			CLiveManager::ShowAppHelpUI();
		}
	}
	else if (iUniqueId == MENU_UNIQUE_ID_SHOW_ACCOUNT_PICKER)
	{
		CLiveManager::ShowSigninUi();
	}
}

bool CPauseMenu::IsMenucepting()
{
	return sm_bAwaitingMenuShiftDepthResponse || (!sm_aMenuState.empty() && sm_aMenuState.Top().iMenuceptionDir != kMENUCEPT_LIMBO);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CPauseMenu::MenuceptionGoDeeper(MenuScreenId newMenu)
{
	if( IsCurrentScreenValid() )
	{
		if( GetCurrentScreenData().HasDynamicMenu() )
		{
			uiSpew("@@@@@ LOSEF: %s", GetCurrentScreenData().MenuScreen.GetParserName());
			GetCurrentScreenData().GetDynamicMenu()->LoseFocus();
		}
		else
			uiSpew("????? LOSEF: %s", GetCurrentScreenData().MenuScreen.GetParserName());
	}
	else
		uiSpew("????? LOSEF: -invalid menu-");

	// super-duper error case. why the CRAP is script calling this!?
	if( !uiVerifyf(!sm_aMenuState.empty(), "Trying to GoDeeper to %s when we've never gone anywhere before!? Ignoring your request until you figure out what you want.", newMenu.GetParserName() ))
	{
		uiErrorf("Trying to GoDeeper to %s when we've never gone anywhere before!? Ignoring your request until you figure out what you want.", newMenu.GetParserName() );
		return;
	}

	// gotta set this up first before we monkey with it below
	SPauseMenuState newState(sm_aMenuState.Top());
	newState.iMenuceptionDir = kMENUCEPT_DEEPER;

	if( IsScreenDataValid(newMenu) )
	{
		CMenuScreen& data = GetScreenData(newMenu);
		if( data.depth == MENU_DEPTH_HEADER )
		{
			// we need to push the titles back on once we're done
			SPauseMenuState newMenuVersion(sm_aMenuState.Top());
			newMenuVersion.iMenuceptionDir = kMENUCEPT_SHALLOWER;
			newMenuVersion.currentActivePanel = newMenu;		// active Panel contains menu we're going to
			newMenuVersion.currentScreen = GetInitialScreen(); // screen contains menu we're at now (for history)

			// because we may have pushed multiple times, need to check past pushes for our 'current' menu 'version'
			int iDepth = sm_aMenuState.GetCount()-1; // -1 because we don't need to check the current top
			while(iDepth--)
			{
				const SPauseMenuState& pastPush = sm_aMenuState[iDepth];
				if( pastPush.iMenuceptionDir == kMENUCEPT_SHALLOWER )
				{
					CMenuScreen& data = GetScreenData(pastPush.currentActivePanel);
					if( uiVerifyf(data.depth == MENU_DEPTH_HEADER, "Expected a pre-cepted menu on our stack to be a header, but it's not!") )
					{
						newMenuVersion.currentScreen = pastPush.currentActivePanel;
						break;
					}
				}
			}

			sm_aMenuState.PushAndGrow(newMenuVersion, MENU_STATE_PUSH_SIZE);

			// set up the titles now
			SetupHeaderTextAndHighlights(data);

			// and then 'cept the first item
			newMenu = data.MenuItems[0].MenuUniqueId;
		}
	}

	const char* cGfxFilename = NULL;
	if( IsScreenDataValid(newMenu) )
	{
		CMenuScreen& data = GetScreenData(newMenu);
		cGfxFilename = data.GetGfxFilename();
	}
	
	sm_aMenuState.PushAndGrow(newState, MENU_STATE_PUSH_SIZE);

	sfDisplayf("MENUCEPTION Started streaming '%s' for '%s'", cGfxFilename?cGfxFilename:"-nothing-", newMenu.GetParserName());

	if(uiVerifyf(cGfxFilename, "Expected a movie to start loading. If you get this assert, but everything SEEMS fine, tell Jeremy. Heck, tell him anyway."))
		KickoffStreamingChildMovie(cGfxFilename, newMenu, kMENUCEPT_DEEPER);

	sm_bHasFocusedMenu = false;
	sm_bRenderContent = false;
	s_iSpinnerTimer.Zero();
	GetDynamicPauseMenu()->SetLastIBData(0, 0);
	PlaySound("SELECT");
}

void CPauseMenu::MenuceptionTheKick()
{
	if(sm_aMenuState.GetCount() > 1) // Can't menucept backwards with only 1 or less depth!  -- Removed verifyf for url:bugstar:1714020
	{
		SPauseMenuState& oldState = sm_aMenuState.Pop();

		if( sm_aMenuState.Top().iMenuceptionDir == kMENUCEPT_SHALLOWER )
		{
			CMenuScreen& data = GetScreenData(sm_aMenuState.Top().currentScreen);
			if( uiVerifyf(data.depth == MENU_DEPTH_HEADER, "Expected a pre-cepted menu on our stack to be a header, but it's not!") )
			{
				SetupHeaderTextAndHighlights(data);
				sm_aMenuState.Pop();
			}

		}

		SPauseMenuState& newState = sm_aMenuState.Top();
		CMenuScreen& oldData = GetScreenData(oldState.currentScreen);
		newState.iMenuceptionDir = kMENUCEPT_SHALLOWER;
		newState.forceFocus = oldState.currentScreen;
		
		uiDisplayf("MENUCEPTION Leaving '%s' behind, heading for '%s'", oldState.currentScreen.GetParserName(), newState.currentScreen.GetParserName());

		uiSpew("@@@@@ LOSEF: %s Kick", GetCurrentScreenData().MenuScreen.GetParserName());

		if( oldData.HasDynamicMenu() )
		{
			oldData.GetDynamicMenu()->LoseFocus();
		}

		const char* cGfxFilename = GetScreenData(newState.currentScreen).GetGfxFilename();
		sfDisplayf("MENUCEPTION Kick started streaming '%s' for '%s'", cGfxFilename?cGfxFilename:"-nothing-", newState.currentScreen.GetParserName());

		KickoffStreamingChildMovie(cGfxFilename, newState.currentScreen, kMENUCEPT_SHALLOWER);
		sm_bHasFocusedMenu = false;
		sm_bRenderContent = false;
		s_iSpinnerTimer.Zero();
		GetDynamicPauseMenu()->SetLastIBData(0, 0);
	}
}

int FindNextRadioIndex(int iStartingIndex, int iDirection)
{
	int iIndex = iStartingIndex;
	// one to adjust for OFF
	int iMaxStations = audRadioStation::GetNumUnlockedStations()+1;
	audRadioStation* pStation = NULL;
	int iSanityCheck = 0;

	do
	{
		iIndex = ((iIndex+1+iMaxStations+iDirection) % iMaxStations)-1;
		if(iIndex == -1)
			return iIndex;

		pStation = audRadioStation::GetStation(iIndex);

		// allow an initially invalid direction of 0 to check for validity
		if(iDirection==0)
			iDirection = 1;
		++iSanityCheck;
	} while ( (!pStation || pStation->IsLocked() || pStation->IsHidden()) && uiVerifyf(iSanityCheck <= iMaxStations, "Loop to find next radio station had to bail out due to too many invalid stations!"));
	
	return iIndex;
}

void CPauseMenu::HandleRadioStation( int iDirection, bool bForceUpdate )
{
	if(iDirection != 0 && !bForceUpdate && g_RadioAudioEntity.ShouldBlockPauseMenuRetunes())
	{
		return;
	}

	CMenuScreen& audioData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_AUDIO );
	int onScreenIndex = 0;
	int dataIndex = audioData.FindItemIndex(PREF_RADIO_STATION, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_RADIO_STATION, but totally expected it!" ) )
	{	
		CMenuItem& curItem = audioData.MenuItems[dataIndex];

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			int iNewStation = FindNextRadioIndex( GetMenuPreference(PREF_RADIO_STATION), iDirection );

			// don't play the tick sound on initial creation if we had an invalid station
			if( iDirection != 0 && iNewStation != GetMenuPreference(PREF_RADIO_STATION) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_RADIO_STATION, iNewStation,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(audioData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			if( GetMenuPreference(PREF_RADIO_STATION) == -1 )
			{
				CScaleformMgr::AddParamString( TheText.Get("MO_RADOFF") );
			}
			else
			{
				const audRadioStation* pStation = audRadioStation::GetStation( GetMenuPreference(PREF_RADIO_STATION) );
				if( pStation && !pStation->IsLocked() && !pStation->IsHidden() )
				{
					CScaleformMgr::AddParamString( TheText.Get(pStation->GetName()) );
				}
				else
				{
					CScaleformMgr::AddParamString( TheText.Get("MO_CSTM_STATION") );
				}
			}
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleAudioOutput( int iDirection, bool bForceUpdate )
{
	CMenuScreen& audioData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_AUDIO );
	int onScreenIndex = 0;
	int dataIndex = audioData.FindItemIndex(PREF_SPEAKER_OUTPUT, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_SPEAKER_OUTPUT, but totally expected it!" ) )
	{	
		CMenuItem& curItem = audioData.MenuItems[dataIndex];
		const int MAX_AVILBLE_OUTPUT_OPTIONS = 4;
		int iNewOutputType = GetMenuPreference(PREF_SPEAKER_OUTPUT) + iDirection;
		const char *sOptionTags[MAX_AVILBLE_OUTPUT_OPTIONS] = {"MO_SPEAKERS","MO_TVOUTP","MO_SHEAD","MO_SSTEREO"};

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if(iNewOutputType < 0)
			{
				iNewOutputType = MAX_AVILBLE_OUTPUT_OPTIONS - 1;
			}
			else if(iNewOutputType == MAX_AVILBLE_OUTPUT_OPTIONS)
			{
				iNewOutputType = 0;
			}

			if( iDirection != 0 && iNewOutputType != GetMenuPreference(PREF_SPEAKER_OUTPUT) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_SPEAKER_OUTPUT, iNewOutputType,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(audioData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate ) )
		{

			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewOutputType]) );
			CScaleformMgr::EndMethod();
		}
		
		SUIContexts::SetActive(UIATSTRINGHASH("HEADPHONE_ENABLED", 0x82B920A8), AreHeadphonesEnabled());
		
		UpdateAudioImage();
		if(iDirection != 0)
		{
			HandleSSWidth(0, PREF_SS_FRONT, true);
			HandleSSWidth(0, PREF_SS_REAR, true);
		}
	}
}

void CPauseMenu::HandleDialogueBoost(int iDirection, bool bForceUpdate)
{
	CMenuScreen& audioData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_AUDIO );
	int onScreenIndex = 0;
	int dataIndex = audioData.FindItemIndex(PREF_DIAG_BOOST, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_DIAG_BOOST, but totally expected it!" ) )
	{	
		CMenuItem& curItem = audioData.MenuItems[dataIndex];
		int iNewValue = GetMenuPreference(PREF_DIAG_BOOST) + (5*iDirection);

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if(iNewValue < 0)
				iNewValue = 0;
			else if(iNewValue > 10)
				iNewValue = 10;

			if( iDirection != 0 && iNewValue != GetMenuPreference(PREF_DIAG_BOOST) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_DIAG_BOOST, iNewValue,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(audioData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD,
			curItem.MenuPref, OPTION_DISPLAY_STYLE_SLIDER, iNewValue, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate))
		{
			CScaleformMgr::AddParamInt(10);

			eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

			CScaleformMgr::AddParamInt((s32)hudColour);
			CScaleformMgr::EndMethod();
		}
	}
}

u32 CPauseMenu::GetControllerContext()
{
	const int NUM_CONTEXTS = 7;
	static const u32 sContexts[NUM_CONTEXTS] = {
		ATSTRINGHASH("INPUT_TYPE_ON_FOOT", 0x6CF03930),		// Third Person
		ATSTRINGHASH("INPUT_TYPE_IN_VEHICLE", 0x49D1A78),	// Third Person
		ATSTRINGHASH("INPUT_TYPE_IN_PLANE", 0x32FAF583),	// Third Person
		ATSTRINGHASH("INPUT_TYPE_ON_FOOT", 0x6CF03930),		// First Person
		ATSTRINGHASH("INPUT_TYPE_IN_VEHICLE", 0x49D1A78),	// First Person
		ATSTRINGHASH("INPUT_TYPE_IN_PLANE", 0x32FAF583),	// First Person
		ATSTRINGHASH("INPUT_TYPE_CREATOR", 0xB33F5BEC)		// Mission Creator
	};

	return sContexts[GetMenuPreference(PREF_CONTROLS_CONTEXT)];
}

void CPauseMenu::SetupControllerPref()
{
	const int ON_FOOT = 0;
	const int IN_VEHICLE = 1;
	const int IN_AIRCRAFT = 2;
	const int IN_MISSION_CREATOR = 6;

	CPed* pPlayerPed = CGameWorld::FindLocalPlayerSafe();
	if(pPlayerPed)
	{
		CPlayerInfo *pPlayer = CGameWorld::FindLocalPlayer()->GetPlayerInfo();
		if(pPlayer)
		{
			int iFPSOffset = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) ? 3 : 0;
			if(pPlayer->GetPlayerPed()->GetIsOnFoot())
			{
				SetMenuPreference(PREF_CONTROLS_CONTEXT, ON_FOOT + iFPSOffset);
			}
			else if(pPlayer->GetPlayerPed()->GetIsInVehicle() && !pPlayer->IsFlyingAircraft())
			{
				SetMenuPreference(PREF_CONTROLS_CONTEXT, IN_VEHICLE + iFPSOffset);
			}
			else if(pPlayer->IsFlyingAircraft())
			{
				SetMenuPreference(PREF_CONTROLS_CONTEXT, IN_AIRCRAFT + iFPSOffset);
			}
		}
		else
		{
			SetMenuPreference(PREF_CONTROLS_CONTEXT, ON_FOOT);
		}

		if(CScriptHud::bUsingMissionCreator)
		{
			SetMenuPreference(PREF_CONTROLS_CONTEXT, IN_MISSION_CREATOR);
		}
	}
	else
	{
		SetMenuPreference(PREF_CONTROLS_CONTEXT, ON_FOOT);
	}
}

void CPauseMenu::HandleSSWidth(int iDirection, eMenuPref ePrefFrontBack, bool bForceUpdate)
{
	if(!SUIContexts::IsActive(UIATSTRINGHASH("SS_SUPPORTED",0xdbb2ce09)))
		return;

	CMenuScreen& audioData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_AUDIO );
	int onScreenIndex = 0;
	int dataIndex = audioData.FindItemIndex(ePrefFrontBack, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_SPEAKER_OUTPUT, but totally expected it!" ) )
	{	
		CMenuItem& curItem = audioData.MenuItems[dataIndex];
		const int MAX_AVILBLE_OUTPUT_OPTIONS = 3;
		int iNewOutputType;
#if RSG_PS3
		if(SUIContexts::IsActive(UIATSTRINGHASH("AUD_WIRELESSHEADSET",0xE32A2BF1)))
		{
			iNewOutputType = GetMenuPreference(ePrefFrontBack);
		}
		else
		{
			iNewOutputType = GetMenuPreference(ePrefFrontBack) + iDirection;
		}
#else
		iNewOutputType = GetMenuPreference(ePrefFrontBack) + iDirection;
#endif
		
		static const char *sFrontOptions[MAX_AVILBLE_OUTPUT_OPTIONS] = {"MO_SPK_WIDE", "MO_SPK_MEDIUM", "MO_SPK_CENTER"};
		static const char *sRearOptions[MAX_AVILBLE_OUTPUT_OPTIONS] = {"MO_SPK_REAR", "MO_SPK_MEDIUM", "MO_SPK_SIDE"};

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if(iNewOutputType < 0)
				iNewOutputType = MAX_AVILBLE_OUTPUT_OPTIONS - 1;
			else if(iNewOutputType >= MAX_AVILBLE_OUTPUT_OPTIONS)
				iNewOutputType = 0;

			if( iDirection != 0 && iNewOutputType != GetMenuPreference(ePrefFrontBack) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(ePrefFrontBack, iNewOutputType,  UPDATE_PREFS_FROM_MENU);
			}
		}

		const int OUTPUT_TYPE_SPEAKERS = 0;
#if RSG_PS3
		bool bEnabled = GetMenuPreference(PREF_SPEAKER_OUTPUT) == OUTPUT_TYPE_SPEAKERS && 
			SUIContexts::IsActive(UIATSTRINGHASH("SS_SUPPORTED",0xdbb2ce09)) &&
			!SUIContexts::IsActive(UIATSTRINGHASH("AUD_WIRELESSHEADSET",0xE32A2BF1));
#else
		bool bEnabled = GetMenuPreference(PREF_SPEAKER_OUTPUT) == OUTPUT_TYPE_SPEAKERS && SUIContexts::IsActive(UIATSTRINGHASH("SS_SUPPORTED",0xdbb2ce09));
#endif
		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(audioData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate ) )
		{
			if(ePrefFrontBack == PREF_SS_FRONT)
			{
				CScaleformMgr::AddParamString( TheText.Get(sFrontOptions[iNewOutputType]) );
			}
			else
			{
				CScaleformMgr::AddParamString( TheText.Get(sRearOptions[iNewOutputType]) );
			}
			CScaleformMgr::EndMethod();
		}

		if(iDirection != 0)
			UpdateAudioImage();
	}
}

bool CPauseMenu::AreHeadphonesEnabled()
{
	return GetMenuPreference(PREF_SPEAKER_OUTPUT) == 2;
}

void CPauseMenu::UpdateAudioImage()
{
	const int OUTPUT_TYPE_SPEAKERS = 0;

#if RSG_PS3
	bool bShowImage = GetMenuPreference(PREF_SPEAKER_OUTPUT) == OUTPUT_TYPE_SPEAKERS && 
		SUIContexts::IsActive(UIATSTRINGHASH("SS_SUPPORTED",0xdbb2ce09)) &&
		!SUIContexts::IsActive(UIATSTRINGHASH("AUD_WIRELESSHEADSET",0xE32A2BF1));
#else
	bool bShowImage = GetMenuPreference(PREF_SPEAKER_OUTPUT) == OUTPUT_TYPE_SPEAKERS && SUIContexts::IsActive(UIATSTRINGHASH("SS_SUPPORTED",0xdbb2ce09));
#endif
	if(GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).BeginMethod("SET_COLUMN_TITLE"))
	{
		CScaleformMgr::AddParamInt(PM_COLUMN_MIDDLE);
		CScaleformMgr::AddParamString("");
		CScaleformMgr::AddParamBool(bShowImage);
		CScaleformMgr::AddParamInt(GetMenuPreference(PREF_SS_FRONT)+1);
		CScaleformMgr::AddParamInt(GetMenuPreference(PREF_SS_REAR)+1);
		CScaleformMgr::AddParamInt(CNewHud::GetCurrentCharacterColour());
		CScaleformMgr::AddParamInt(CNewHud::GetCurrentCharacterColour());
		CScaleformMgr::EndMethod();
	}
}

#if RSG_PC
void CPauseMenu::HandleDXVersion(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_DXVERSION, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_DXVERSION, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int MAX_AVILBLE_DX_VERSIONS = 3;
		const char *sOptionTags[MAX_AVILBLE_DX_VERSIONS] = {
			"MO_GFX_DX10",
			"MO_GFX_DX101",
			"MO_GFX_DX11"
		};
		eDXLevelSupported maxDXSupported = CSettingsManager::GetInstance().HighestDXVersionSupported();
		int iNewIndex = GetMenuPreference(PREF_GFX_DXVERSION) + iDirection;

		if(iNewIndex < 0)
			iNewIndex = maxDXSupported;
		else if(iNewIndex > maxDXSupported)
			iNewIndex = 0;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_GFX_DXVERSION) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_DXVERSION, iNewIndex,  UPDATE_PREFS_FROM_MENU);

				if(iNewIndex != DX_VERSION_11_INDEX)
				{
					SetMenuPreference(PREF_GFX_MSAA, 0);
				}
			}

			HandleTessellation(0, true);
			HandleMSAA(PREF_GFX_MSAA, 0, true);
			HandleMSAA(PREF_GFX_REFLECTION_MSAA, 0, true);
			HandleGrassQuality(0, true);
			HandleTXAA(0, true);
			HandleShadowSoftness(0, true);
			HandleDOF(0, true);
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}


void CPauseMenu::HandleTessellation(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_TESSELLATION, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_TESSELLATION, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];

		const int NUM_AVILBLE_TESSELLATION_OPTIONS = 4;
		const char *sOptionTags[NUM_AVILBLE_TESSELLATION_OPTIONS] = {
			"MO_OFF",
			"MO_GFX_NORM",
			"MO_CS_HIGH",
			"MO_GFX_VHIGH",
		};

		int maxTessellationSupported = GetMenuPreference(PREF_GFX_DXVERSION) < DX_VERSION_11_INDEX ? 0 : NUM_AVILBLE_TESSELLATION_OPTIONS-1;
		int iNewIndex = GetMenuPreference(PREF_GFX_TESSELLATION) + iDirection;

		if(iNewIndex < 0)
			iNewIndex = maxTessellationSupported;
		else if(iNewIndex > maxTessellationSupported)
			iNewIndex = 0;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_GFX_TESSELLATION) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_TESSELLATION, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, GetMenuPreference(PREF_GFX_DXVERSION) == DX_VERSION_11_INDEX, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleMSAA(eMenuPref ePerf, int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(ePerf, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_MSAA, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_MSAA_OPTIONS = 4;
		const char *sOptionTags[NUM_AVILBLE_MSAA_OPTIONS] = {
			"MO_OFF",
			"MO_GFX_X2",
			"MO_GFX_X4",
			"MO_GFX_X8"
		};

		const int DX_VERSION_10_1_INDEX = 1;
		int maxMSAASupported = GetMenuPreference(PREF_GFX_DXVERSION) < DX_VERSION_10_1_INDEX ? 0 : NUM_AVILBLE_MSAA_OPTIONS-1;
		int iNewIndex = GetMenuPreference(ePerf) + iDirection;

		if(iNewIndex < 0)
			iNewIndex = maxMSAASupported;
		else if(iNewIndex > maxMSAASupported)
			iNewIndex = 0;

		if(maxMSAASupported == 0)
			UpdateMemoryBar();

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(ePerf) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(ePerf, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}

			HandleTXAA(0, true);
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, GetMenuPreference(PREF_GFX_DXVERSION) >= DX_VERSION_10_1_INDEX, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleScaling(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_SCALING, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_SCALING, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];

		const int NUM_OPTIONS = 10;
		const char *sOptionTags[NUM_OPTIONS] = {
			"MO_OFF",
			"MO_GFX_1o2",
			"MO_GFX_2o3",
			"MO_GFX_3o4",
			"MO_GFX_5o6",
			"MO_GFX_5o4",
			"MO_GFX_3o2",
			"MO_GFX_7o4",
			"MO_GFX_2o1",
			"MO_GFX_5o2"
		};

		int maxOptionSupported = NUM_OPTIONS-1;
		int iNewIndex = GetMenuPreference(PREF_GFX_SCALING) + iDirection;

		if(iNewIndex < 0)
			iNewIndex = maxOptionSupported;
		else if(iNewIndex > maxOptionSupported)
			iNewIndex = 0;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_GFX_SCALING) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_SCALING, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleTXAA(int iDirection, bool bForceUpdate)
{
	if(!SUIContexts::IsActive("GFX_SUPPORT_TXAA"))
	{
		SetMenuPreference(PREF_GFX_TXAA, 0);
		return;
	}

	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_TXAA, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_TXAA, but totally expected it!" ) )
	{
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_TXAA_OPTIONS = 2;
		const char *sOptionTags[NUM_AVILBLE_TXAA_OPTIONS] = {
			"MO_OFF",
			"MO_ON"
		};

		int iDXVersion = GetMenuPreference(PREF_GFX_DXVERSION);
		int iMSAAQuality = GetMenuPreference(PREF_GFX_MSAA);
		bool bActive = (iDXVersion >= DX_VERSION_11_INDEX) && (iMSAAQuality > 0 && iMSAAQuality < 3);
		int iNewIndex = GetMenuPreference(PREF_GFX_TXAA) + iDirection;
		if(!bActive)
		{
			iNewIndex = 0;
		}
		else
		{
			if(iNewIndex < 0)
				iNewIndex = NUM_AVILBLE_TXAA_OPTIONS - 1;
			else if(iNewIndex >= NUM_AVILBLE_TXAA_OPTIONS)
				iNewIndex = 0;
		}

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_GFX_TXAA) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_TXAA, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, bActive, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleGrassQuality(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_GRASS_QUALITY, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_GRASS_QUALITY, but totally expected it!" ) )
	{
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_GRASS_OPTIONS = 4;
		const char *sOptionTags[NUM_AVILBLE_GRASS_OPTIONS] = {
			"MO_GFX_NORM",
			"MO_CS_HIGH",
			"MO_GFX_VHIGH",
			"MO_GFX_ULTRA"
		};

		int iDXVersion = GetMenuPreference(PREF_GFX_DXVERSION);
		int iNewIndex = GetMenuPreference(PREF_GFX_GRASS_QUALITY) + iDirection;
		bool bActive = (iDXVersion >= DX_VERSION_11_INDEX);

		if(!bActive)
		{
			iNewIndex = 0;
		}
		else
		{
			if(iNewIndex < 0)
				iNewIndex = NUM_AVILBLE_GRASS_OPTIONS - 1;
			else if(iNewIndex >= NUM_AVILBLE_GRASS_OPTIONS)
				iNewIndex = 0;
		}

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_GFX_GRASS_QUALITY) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_GRASS_QUALITY, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, bActive, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}


void CPauseMenu::HandleShadowSoftness(int iDirection, bool bForceUpdate, bool bCheckOnly)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_SHADOW_SOFTNESS, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_SHADOW_SOFTNESS, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_SHADOW_SOFTNESS_OPTIONS = 6;
		const char *sOptionTags[NUM_AVILBLE_SHADOW_SOFTNESS_OPTIONS] = {
			"GFX_SHARP",
			"GFX_SOFT",
			"GFX_SOFTER",
			"GFX_SOFTEST",
			"GFX_AMD",
			"GFX_NVIDIA"
		};

		int iDXVersion = GetMenuPreference(PREF_GFX_DXVERSION);
		int iStereoEnabled = GetMenuPreference(PREF_VID_STEREO);
		int maxOptionsSupported = (iDXVersion >= DX_VERSION_11_INDEX) && (iStereoEnabled == 0) ? 5 : 3;
		int iNewIndex = GetMenuPreference(PREF_GFX_SHADOW_SOFTNESS) + iDirection;

		if(iNewIndex < 0)
			iNewIndex = maxOptionsSupported;
		else if(iNewIndex > maxOptionsSupported)
			iNewIndex = 0;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_GFX_SHADOW_SOFTNESS) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_SHADOW_SOFTNESS, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}

			// If we changed the adapter to a somethign non-nvidia, update the option and bail out of this function
			if(bCheckOnly && iNewIndex != GetMenuPreference(PREF_GFX_SHADOW_SOFTNESS))
			{
				SetMenuPreference(PREF_GFX_SHADOW_SOFTNESS, iNewIndex);
				return;
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandlePostFX(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_POST_FX, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_POST_FX, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_POST_FX_OPTIONS = 4;
		const char *sOptionTags[NUM_AVILBLE_POST_FX_OPTIONS] = {
			"MO_GFX_NORM",
			"MO_CS_HIGH",
			"MO_GFX_VHIGH",
			"MO_GFX_ULTRA"
		};

		int iNewIndex = GetMenuPreference(PREF_GFX_POST_FX) + iDirection;

		if(iNewIndex < 0)
			iNewIndex = NUM_AVILBLE_POST_FX_OPTIONS - 1;
		else if(iNewIndex >= NUM_AVILBLE_POST_FX_OPTIONS)
			iNewIndex = 0;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_GFX_POST_FX) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_POST_FX, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}

		HandleDOF(0, true);
		HandleMBStrength(0, true);
	}
}

void CPauseMenu::HandleMBStrength(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_MB_STRENGTH, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_MB_STRENGTH, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int POST_FX_INDEX_MB_CUTOFF = 0;
		bool bEnabled = GetMenuPreference(PREF_GFX_POST_FX) > POST_FX_INDEX_MB_CUTOFF;
		int iNewValue = GetMenuPreference(PREF_GFX_MB_STRENGTH) + iDirection;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if (bEnabled)
			{
				if(iNewValue < 0)
					iNewValue = 0;
				else if(iNewValue > 10)
					iNewValue = 10;
			}
			else
			{
				iNewValue = 0;
			}

			if( iDirection != 0 && iNewValue != GetMenuPreference(PREF_GFX_MB_STRENGTH) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_MB_STRENGTH, iNewValue,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD,
			curItem.MenuPref, OPTION_DISPLAY_STYLE_SLIDER, iNewValue, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate))
		{
			CScaleformMgr::AddParamInt(10);

			eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

			CScaleformMgr::AddParamInt((s32)hudColour);
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleDOF(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_GFX_DOF, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_GFX_DOF, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_DOF_OPTIONS = 2;
		const char *sOptionTags[NUM_AVILBLE_DOF_OPTIONS] = {
			"MO_OFF",
			"MO_ON"
		};

		const int POST_FX_INDEX_DOF_CUTOFF = 1;
		int iNewIndex = GetMenuPreference(PREF_GFX_DOF) + iDirection;
		bool bEnabled = GetMenuPreference(PREF_GFX_POST_FX) > POST_FX_INDEX_DOF_CUTOFF;
		int iDXVersion = GetMenuPreference(PREF_GFX_DXVERSION);
		bEnabled &= (iDXVersion >= DX_VERSION_11_INDEX);

		if(bEnabled)
		{
			if(iNewIndex < 0)
				iNewIndex = NUM_AVILBLE_DOF_OPTIONS - 1;
			else if(iNewIndex >= NUM_AVILBLE_DOF_OPTIONS)
				iNewIndex = 0;
		}
		else
		{
			iNewIndex = 0;
		}

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_GFX_DOF) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_GFX_DOF, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleShadowDistMult(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_ADV_GFX_SHADOWS_DIST_MULT, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_ADV_GFX_SHADOWS_DIST_MULT, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		bool bEnabled = GetMenuPreference(PREF_GFX_SHADOW_QUALITY) >= CSettings::High;
		int iNewValue = GetMenuPreference(PREF_ADV_GFX_SHADOWS_DIST_MULT) + iDirection;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if (bEnabled)
			{
				if(iNewValue < 0)
					iNewValue = 0;
				else if(iNewValue > 10)
					iNewValue = 10;
			}
			else
			{
				iNewValue = 0;
			}

			if( iDirection != 0 && iNewValue != GetMenuPreference(PREF_ADV_GFX_SHADOWS_DIST_MULT) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_ADV_GFX_SHADOWS_DIST_MULT, iNewValue,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD,
			curItem.MenuPref, OPTION_DISPLAY_STYLE_SLIDER, iNewValue, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate))
		{
			CScaleformMgr::AddParamInt(10);

			eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

			CScaleformMgr::AddParamInt((s32)hudColour);
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleVoiceInput(int iDirection, bool bForceUpdate)
{
	CMenuScreen& voiceData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT);
	int onScreenIndex = 0;
	int dataIndex = voiceData.FindItemIndex(PREF_VOICE_INPUT_DEVICE, &onScreenIndex);
	int iNewVCInput = GetMenuPreference(PREF_VOICE_INPUT_DEVICE) + iDirection;
	atArray<wchar_t*> list;

	if( uiVerifyf(dataIndex != -1, "Couldn't find voice input menu item" ) )
	{	
		CMenuItem& curItem = voiceData.MenuItems[dataIndex];

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			RVDeviceInfo devices[VoiceChat::MAX_DEVICES];
			int deviceCount = RVoice::ListDevices(devices, VoiceChat::MAX_DEVICES, RV_CAPTURE);

			if(deviceCount == 0 && GetMenuPreference(PREF_VOICE_TALK_ENABLED))
				SetItemPref(PREF_VOICE_TALK_ENABLED, 0,  UPDATE_PREFS_FROM_MENU);

			for(int i = 0; i < deviceCount; i++)
			{
				RVDeviceInfo* info = &devices[i];
				list.PushAndGrow(info->m_name);
			}

			if(iNewVCInput < 0)
				iNewVCInput = deviceCount - 1;
			else if(iNewVCInput >= deviceCount)
				iNewVCInput = 0;

			// don't play the tick sound on initial creation if we had an invalid station
			if( iDirection != 0 && iNewVCInput != GetMenuPreference(PREF_VOICE_INPUT_DEVICE) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VOICE_INPUT_DEVICE, iNewVCInput,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if(list.size() > 0)
		{
			if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(voiceData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
				, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, GetMenuPreference(PREF_VOICE_ENABLE) && GetMenuPreference(PREF_VOICE_TALK_ENABLED), TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
			{
				char deviceName[MAX_VOICE_DEVICE_LENGTH] = {0};
				char deviceNameFrontEnd[MAX_VOICE_DEVICE_FRONTEND_LENGTH] = {0};
				
				char16 deviceNameRaw[MAX_VOICE_DEVICE_LENGTH];
				memcpy(deviceNameRaw, list[iNewVCInput], sizeof(deviceNameRaw));

				WideToUtf8(deviceName, deviceNameRaw, MAX_VOICE_DEVICE_LENGTH);

				if(strlen(deviceName) >= MAX_VOICE_DEVICE_FRONTEND_LENGTH)
				{
					strncpy(deviceNameFrontEnd, deviceName, MAX_VOICE_DEVICE_FRONTEND_LENGTH - 5);
					strcat(deviceNameFrontEnd, "...\0");
				}
				else
				{
					strcpy(deviceNameFrontEnd, deviceName);
				}

				CScaleformMgr::AddParamString( deviceNameFrontEnd, false );
				CScaleformMgr::EndMethod();
			}
		}
		else
		{
			if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(voiceData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
				, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, GetMenuPreference(PREF_VOICE_ENABLE) && GetMenuPreference(PREF_VOICE_TALK_ENABLED), TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
			{
				CScaleformMgr::AddParamString(TheText.Get("SM_LCNONE"));
				CScaleformMgr::EndMethod();
			}
		}
	}
}

void CPauseMenu::HandleVoiceOutput(int iDirection, bool bForceUpdate)
{
	CMenuScreen& voiceData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT);
	int onScreenIndex = 0;
	int dataIndex = voiceData.FindItemIndex(PREF_VOICE_OUTPUT_DEVICE, &onScreenIndex);
	int iNewVCOutput = GetMenuPreference(PREF_VOICE_OUTPUT_DEVICE) + iDirection;
	atArray<wchar_t*> list;

	if( uiVerifyf(dataIndex != -1, "Couldn't find voice output menu item" ) )
	{	
		CMenuItem& curItem = voiceData.MenuItems[dataIndex];

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			RVDeviceInfo devices[VoiceChat::MAX_DEVICES];
			int num = RVoice::ListDevices(devices, VoiceChat::MAX_DEVICES, RV_PLAYBACK);

			for(int i = 0; i < num; i++)
			{
				RVDeviceInfo* info = &devices[i];
				list.PushAndGrow(info->m_name);
			}

			if(iNewVCOutput < 0)
				iNewVCOutput = num - 1;
			else if(iNewVCOutput >= num)
				iNewVCOutput = 0;

			// don't play the tick sound on initial creation if we had an invalid station
			if( iDirection != 0 && iNewVCOutput != GetMenuPreference(PREF_VOICE_OUTPUT_DEVICE) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VOICE_OUTPUT_DEVICE, iNewVCOutput,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if(list.size() > 0)
		{
			if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(voiceData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
				, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, GetMenuPreference(PREF_VOICE_ENABLE), TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
			{
				char deviceName[MAX_VOICE_DEVICE_LENGTH] = {0};
				char deviceNameFrontEnd[MAX_VOICE_DEVICE_FRONTEND_LENGTH] = {0};

				char16 deviceNameRaw[MAX_VOICE_DEVICE_LENGTH];
				memcpy(deviceNameRaw, list[iNewVCOutput], sizeof(deviceNameRaw));

				WideToUtf8(deviceName, deviceNameRaw, MAX_VOICE_DEVICE_LENGTH);

				if(strlen(deviceName) >= MAX_VOICE_DEVICE_FRONTEND_LENGTH)
				{
					strncpy(deviceNameFrontEnd, deviceName, MAX_VOICE_DEVICE_FRONTEND_LENGTH - 4);
					strcat(deviceNameFrontEnd, "...\0");
				}
				else
				{
					strcpy(deviceNameFrontEnd, deviceName);
				}

				CScaleformMgr::AddParamString( deviceNameFrontEnd, false );
				CScaleformMgr::EndMethod();
			}
		}
		else
		{
			if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(voiceData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
				, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, GetMenuPreference(PREF_VOICE_ENABLE) && GetMenuPreference(PREF_VOICE_TALK_ENABLED), TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
			{
				CScaleformMgr::AddParamString(TheText.Get("SM_LCNONE"));
				CScaleformMgr::EndMethod();
			}
		}
	}
}

void CPauseMenu::HandleVoiceChatMode(int iDirection, bool bForceUpdate /* = false */)
{
	CMenuScreen& voiceData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT);
	int onScreenIndex = 0;
	int dataIndex = voiceData.FindItemIndex(PREF_VOICE_CHAT_MODE, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find screen resolution menu item" ) )
	{
		CMenuItem& curItem = voiceData.MenuItems[dataIndex];
		atArray<CMenuDisplayValue>& displayValues = GetMenuArray().DisplayValues;
		
		for (s32 dvi = 0; dvi < displayValues.GetCount(); dvi++)
		{
			CMenuDisplayValue& curValue = displayValues[dvi];
			if (curValue.MenuOption == curItem.MenuOption)
			{
				s32 iNumberOfAllowsPrefSettings = curValue.MenuDisplayOptions.GetCount();

				int iChatPreference = Wrap(GetMenuPreference(PREF_VOICE_CHAT_MODE) + iDirection, 0, iNumberOfAllowsPrefSettings-1);

				if( iDirection != 0 )
				{
					PlaySound("NAV_LEFT_RIGHT");
					SetItemPref(PREF_VOICE_CHAT_MODE, iChatPreference,  UPDATE_PREFS_FROM_MENU);
				}

				const char* pText = iChatPreference == VoiceChat::CAPTURE_MODE_PUSH_TO_TALK ? TheText.Get("MO_VCHAT_MODE_PTT") : TheText.Get( curItem.cTextId.GetHash(), "");

				if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(voiceData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, 
					curItem.MenuPref, static_cast<eOPTION_DISPLAY_STYLE>(5), 0, true, pText, false, iDirection!=0 || bForceUpdate, false) )
				{
					// we could send all the options, but since we gotta override the text anyway, let's just send the relevant one
					CScaleformMgr::AddParamLocString(curValue.MenuDisplayOptions[iChatPreference].cTextId.GetHash());
				}
			
				CScaleformMgr::EndMethod();

				return;
			}
		}
	}
}

void CPauseMenu::HandleScreenType(int iDirection, bool bForceUpdate)
{
	CMenuScreen& notificationData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = notificationData.FindItemIndex(PREF_VID_SCREEN_TYPE, &onScreenIndex);
	int iScreenType = GetMenuPreference(PREF_VID_SCREEN_TYPE) + iDirection;

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_VID_SCREEN_TYPE, but totally expected it!" ) )
	{	

		CMenuItem& curItem = notificationData.MenuItems[dataIndex];
		int iNumberOfScreenModes = 3; //fullscreen, windowed, and borderless windowed

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if(iScreenType < 0)
				iScreenType = iNumberOfScreenModes - 1;
			else if(iScreenType >= iNumberOfScreenModes)
				iScreenType = 0;

			if (!FullscreenIsAllowed() && iScreenType == 0)
			{
				if (iDirection != 0)
				{
					iScreenType += iDirection;
				}
				else
				{
					iScreenType = 2;
				}

				if(iScreenType < 0)
					iScreenType = iNumberOfScreenModes - 1;
				else if(iScreenType >= iNumberOfScreenModes)
					iScreenType = 0;
			}

			if( iScreenType != GetMenuPreference(PREF_VID_SCREEN_TYPE) )
			{
				if (iDirection != 0)
				{
					PlaySound("NAV_LEFT_RIGHT");
				}
				Settings previousResolution = GetMenuGraphicsSettings();
				SetItemPref(PREF_VID_SCREEN_TYPE, iScreenType,  UPDATE_PREFS_FROM_MENU);
				HandleScreenResolution(0, true);
				SetItemPref(PREF_VID_SCREEN_TYPE, iScreenType,  UPDATE_PREFS_FROM_MENU);

				if(SUIContexts::IsActive("VID_STEREO"))
					Handle3DStereo(0, true);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(notificationData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate ) )
		{
			static const char *sScreenTypeOptions[3] = {"VID_FULLSCREEN", "VID_SCR_WIN", "VID_SCR_BORDERLESS"};
			CScaleformMgr::AddParamString(TheText.Get(sScreenTypeOptions[iScreenType]));
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::Handle3DStereo(int iDirection, bool bForceUpdate)
{
	if(!SUIContexts::IsActive("VID_STEREO"))
		return;

	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_VID_STEREO, &onScreenIndex);
	
	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_VID_STEREO, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_STEREO_OPTIONS = 2;
		const char *sOptionTags[NUM_AVILBLE_STEREO_OPTIONS] = {
			"MO_OFF",
			"MO_ON"
		};

		int iNewIndex = GetMenuPreference(PREF_VID_STEREO) + iDirection;
		bool bEnabled = GetMenuPreference(PREF_VID_SCREEN_TYPE) == 0; // Fullscreen

		if(bEnabled)
		{
			if(iNewIndex < 0)
				iNewIndex = NUM_AVILBLE_STEREO_OPTIONS - 1;
			else if(iNewIndex >= NUM_AVILBLE_STEREO_OPTIONS)
				iNewIndex = 0;
		}
		else
		{
			iNewIndex = 0;
		}

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_VID_STEREO) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VID_STEREO, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}

			Handle3DStereoConvergence(0, true);
			//Handle3DStereoSeparation(0, true);
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}

		HandleShadowSoftness(0, true);
	}
}

void CPauseMenu::Handle3DStereoConvergence(int iDirection, bool bForceUpdate)
{
	if(!SUIContexts::IsActive("VID_STEREO"))
		return;

	if(iDirection != 0)
		m_bStereoOverride = false;

	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_VID_STEREO_CONVERGENCE, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_VID_STEREO_CONVERGENCE, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_STEREO_OPTIONS = 20;

		int iNewIndex = GetMenuPreference(PREF_VID_STEREO_CONVERGENCE) + iDirection;
		bool bEnabled = GetMenuPreference(PREF_VID_STEREO) == TRUE;

		if(bEnabled)
		{
			if(iNewIndex < 0)
				iNewIndex = 0;
			else if(iNewIndex > NUM_AVILBLE_STEREO_OPTIONS)
				iNewIndex = NUM_AVILBLE_STEREO_OPTIONS;
		}
		else
		{
			iNewIndex = 0;
		}

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_VID_STEREO_CONVERGENCE) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VID_STEREO_CONVERGENCE, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD,
			curItem.MenuPref, OPTION_DISPLAY_STYLE_SLIDER, iNewIndex, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate))
		{
			CScaleformMgr::AddParamInt(NUM_AVILBLE_STEREO_OPTIONS);

			eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

			CScaleformMgr::AddParamInt((s32)hudColour);
			CScaleformMgr::EndMethod();
		}
	}
}

int CPauseMenu::GetStereoConvergenceFrontendValue(float fValue)
{
	if(fValue <= 0.001)						return 0;
	if(fValue > 0.001f && fValue <= 0.05f )	return 1;
	if(fValue > 0.05f && fValue <= 0.1f )	return 2;
	if(fValue > 0.1f && fValue <= 0.15f )	return 3;
	if(fValue > 0.15f && fValue <= 0.20f )	return 4;
	if(fValue > 0.20f && fValue <= 0.25f )	return 5;
	if(fValue > 0.25f && fValue <= 0.35f )	return 6;
	if(fValue > 0.35f && fValue <= 0.45f )	return 7;
	if(fValue > 0.45f && fValue <= 0.55f )	return 8;
	if(fValue > 0.55f && fValue <= 0.65f )	return 9;
	if(fValue > 0.65f && fValue <= 0.75f )	return 10;
	if(fValue > 0.75f && fValue <= 0.85f )	return 11;
	if(fValue > 0.85f && fValue <= 0.95f )	return 12;
	if(fValue > 0.95f && fValue <= 1.05f )	return 13;
	if(fValue > 1.05f && fValue <= 1.15f )	return 14;
	if(fValue > 1.15f && fValue <= 1.25f )	return 15;
	if(fValue > 1.25f && fValue <= 1.40f )	return 16;
	if(fValue > 1.40f && fValue <= 1.55f )	return 17;
	if(fValue > 1.55f && fValue <= 1.70f )	return 18;
	if(fValue > 1.70f && fValue <= 1.85f )	return 19;
	if(fValue > 1.85f && fValue <= 2.0f )	return 20;
	if(fValue > 2.0f)						return 20;

	return 0;
}

float CPauseMenu::GetStereoConvergenceBackendValue(int iValue)
{
	switch (iValue)
	{
	case 0:		return 0.001f;
	case 1:		return 0.05f;
	case 2:		return 0.1f;
	case 3:		return 0.15f;
	case 4:		return 0.20f;
	case 5:		return 0.25f;
	case 6:		return 0.35f;
	case 7:		return 0.45f;
	case 8:		return 0.55f;
	case 9:		return 0.65f;
	case 10:	return 0.75f;
	case 11:	return 0.85f;
	case 12:	return 0.95f;
	case 13:	return 1.05f;
	case 14:	return 1.15f;
	case 15:	return 1.25f;
	case 16:	return 1.40f;
	case 17:	return 1.55f;
	case 18:	return 1.70f;
	case 19:	return 1.85f;
	case 20:	return 2.0f;
	default:	return 0.0f;
	}
}


void CPauseMenu::Handle3DStereoSeparation(int iDirection, bool bForceUpdate)
{
	if(!SUIContexts::IsActive("VID_STEREO"))
		return;

	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_VID_STEREO_SEPARATION, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_VID_STEREO_SEPARATION, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_STEREO_OPTIONS = 10;

		int iNewIndex = GetMenuPreference(PREF_VID_STEREO_SEPARATION) + iDirection;
		bool bEnabled = GetMenuPreference(PREF_VID_STEREO) == TRUE;

		if(bEnabled)
		{
			if(iNewIndex < 0)
				iNewIndex = 0;
			else if(iNewIndex > NUM_AVILBLE_STEREO_OPTIONS)
				iNewIndex = NUM_AVILBLE_STEREO_OPTIONS;
		}
		else
		{
			iNewIndex = 0;
		}

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_VID_STEREO_SEPARATION) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VID_STEREO_SEPARATION, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD,
			curItem.MenuPref, OPTION_DISPLAY_STYLE_SLIDER, iNewIndex, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate))
		{
			CScaleformMgr::AddParamInt(NUM_AVILBLE_STEREO_OPTIONS);

			eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

			CScaleformMgr::AddParamInt((s32)hudColour);
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleSkipLandingPage(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_SAVEGAME);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_LANDING_PAGE, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_LANDING_PAGE, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_OPTIONS = 2;
		const char *sOptionTags[NUM_AVILBLE_OPTIONS] = {
			"MO_OFF",
			"MO_ON"
		};

		int iNewIndex = GetMenuPreference(PREF_LANDING_PAGE) + iDirection;

		if(iNewIndex < 0)
			iNewIndex = NUM_AVILBLE_OPTIONS - 1;
		else if(iNewIndex >= NUM_AVILBLE_OPTIONS)
			iNewIndex = 0;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_LANDING_PAGE) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_LANDING_PAGE, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}

			HandleStartupFlow(0, true);
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleStartupFlow(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_SAVEGAME);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_STARTUP_FLOW, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_STARTUP_FLOW, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int NUM_AVILBLE_STARTUP_OPTIONS = 2;
		const char *sOptionTags[NUM_AVILBLE_STARTUP_OPTIONS] = {
			"MO_LOAD_SP",
			"MO_LOAD_MP"
		};

		int iNewIndex = GetMenuPreference(PREF_STARTUP_FLOW) + iDirection;
		bool bEnabled = GetMenuPreference(PREF_LANDING_PAGE) == FALSE;

		if(bEnabled)
		{
			if(iNewIndex < 0)
				iNewIndex = NUM_AVILBLE_STARTUP_OPTIONS - 1;
			else if(iNewIndex >= NUM_AVILBLE_STARTUP_OPTIONS)
				iNewIndex = 0;
		}
		else
		{
			iNewIndex = 0;
		}

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_STARTUP_FLOW) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_STARTUP_FLOW, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleReplayMemoryAllocation(int REPLAY_ONLY(iDirection), bool REPLAY_ONLY(bForceUpdate))
{
#if GTA_REPLAY
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_REPLAY);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_REPLAY_MEM_LIMIT, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_REPLAY_MEM_LIMIT, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];

	
		static bool s_replayMemLimitOutOfBounds = false;
		int max_limit = (int)ReplayFileManager::GetMaxAvailableMemoryLimit();
		int min_limit = (int)ReplayFileManager::GetMinAvailableMemoryLimit();
		
		int currentLimit = CProfileSettings::GetInstance().GetInt(CProfileSettings::REPLAY_MEM_LIMIT);
		int iNewIndex = GetMenuPreference(PREF_REPLAY_MEM_LIMIT) + iDirection;

		if( ( currentLimit < min_limit || currentLimit > max_limit ) && iDirection == 0 )
		{
			iNewIndex = currentLimit;
			s_replayMemLimitOutOfBounds = true;
		}
		else
		{
			if( s_replayMemLimitOutOfBounds )
			{
				if(iNewIndex < min_limit || iNewIndex > max_limit)
				{
					if(iDirection == 1)
						iNewIndex = min_limit;
					else
						iNewIndex = max_limit;
				}
				s_replayMemLimitOutOfBounds = false;
			}
			else
			{
				if(iNewIndex < min_limit)
					iNewIndex = max_limit;
				else if(iNewIndex > max_limit)
					iNewIndex = min_limit;
			}
		}

		bool available = ReplayFileManager::CanFullfillSpaceRequirement(REPLAY_MEM_250MB);

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( !available )
			{
				SetItemPref(PREF_REPLAY_MEM_LIMIT, 0, UPDATE_PREFS_FROM_MENU);
			}
			else if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_REPLAY_MEM_LIMIT) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_REPLAY_MEM_LIMIT, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		size_t availableSpace = 0;
		char titleBuffer[64] = {0};

		if( ReplayFileManager::getUsedSpaceForClips(availableSpace) )
		{
			float size = (availableSpace >> 10) / 1024.0f;
			const char* prefix = "MO_REPLAY_MB";

			if( size >= 1000 )
			{
				size = (availableSpace >> 20) / 1024.0f;
				prefix = "MO_REPLAY_GB";
			}

			CNumberWithinMessage number[1];
			number[0].Set(size, 1);

			CSubStringWithinMessage SubStringArray[1];
			SubStringArray[0].SetTextLabel(prefix);

			CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get(curItem.cTextId.GetHash(), ""), &number[0], 1, &SubStringArray[0], 1, titleBuffer, 64);
		}		

		MemLimitInfo info;
		char itemBuffer[64] = {0};
		if( ReplayFileManager::GetMemLimitInfo((eReplayMemoryLimit)iNewIndex, info))
		{
			float availableSpace = ReplayFileManager::GetAvailableAndUsedClipSpaceInGB();
			const char* itemPrefix = info.m_Limit < eReplayMemoryLimit::REPLAY_MEM_1GB ? "MO_REPLAY_MB" : "MO_REPLAY_GB";
			const char* maxPrefix = "MO_REPLAY_GB";

			if( availableSpace < 1.0f )
			{
				availableSpace *= 1024;
				maxPrefix = "MO_REPLAY_MB";
			}

			CNumberWithinMessage number[1];
			number[0].Set(availableSpace, 1);

			CSubStringWithinMessage SubStringArray[3];
			SubStringArray[0].SetLiteralString(info.GetName(), CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE, false);
			SubStringArray[1].SetTextLabel(itemPrefix);
			SubStringArray[2].SetTextLabel(maxPrefix);

			CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("MO_REPLAY_MEM_LIMIT"), &number[0], 1, &SubStringArray[0], 3, itemBuffer, 64);
		}
		
		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, available, titleBuffer, false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( itemBuffer );
			CScaleformMgr::EndMethod();
		}

		UpdateProfileFromMenuOptions();
	}
#endif //  GTA_REPLAY
}

void CPauseMenu::HandleScreenResolution(int iDirection, bool bForceUpdate, bool bFindClosestMatch, u32 uPreviousScreenWidth, u32 uPreviousScreenHeight)
{
	CMenuScreen& videoData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = videoData.FindItemIndex(PREF_VID_RESOLUTION, &onScreenIndex);
	int iNewScreenResolution = GetMenuPreference(PREF_VID_RESOLUTION) + iDirection;

	if( uiVerifyf(dataIndex != -1, "Couldn't find screen resolution menu item" ) )
	{
		CMenuItem& curItem = videoData.MenuItems[dataIndex];

		atArray<grcDisplayWindow> &list = CSettingsManager::GetInstance().GetResolutionList(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR));
		if (CPauseMenu::GetMenuPreference(PREF_VID_SCREEN_TYPE) == 0)
			list = CSettingsManager::GetInstance().GetNativeResolutionList(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR));

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			// Cull the list of duplicate entries sine we're separating the refresh rate into its own menu item.

			int iDisplayModes = list.size();

			if(bFindClosestMatch)
			{
				bool bFoundClosest = false;

				for(int i = 0; i < iDisplayModes; i++)
				{
					grcDisplayWindow mode = list[i];
					if( mode.uWidth == uPreviousScreenWidth &&
						mode.uHeight == uPreviousScreenHeight)
					{
						iNewScreenResolution = i;
						bFoundClosest = true;
						break;
					}
				}

				if(!bFoundClosest)
				{
					iNewScreenResolution = 0; // Minimum resolution
				}
			}
			else
			{
				if(iNewScreenResolution < 0)
					iNewScreenResolution = iDisplayModes - 1;
				else if(iNewScreenResolution >= iDisplayModes)
					iNewScreenResolution = 0;
			}

			// don't play the tick sound on initial creation if we had an invalid station
			if( iDirection != 0 && iNewScreenResolution != GetMenuPreference(PREF_VID_RESOLUTION) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VID_RESOLUTION, iNewScreenResolution,  UPDATE_PREFS_FROM_MENU);
			}
			else
			{
				SetMenuPreference(PREF_VID_RESOLUTION, iNewScreenResolution);
			}
			HandleScreenRefreshRate(0, true);
			HandleVideoAspect(0, true);
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(videoData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate ) )
		{

			u32 width = list[iNewScreenResolution].uWidth;
			u32 height = list[iNewScreenResolution].uHeight;

			ConvertToActualResolution(width, height, iNewScreenResolution);
			char resolution[48] = {0};
#ifdef DO_ASPECT_LOOKUP
			struct rezLookup
			{
				float width;
				int height;
				bool PrintIfMatches( char* pszOut, u32 inWidth, u32 inHeight ) const
				{
					float inAspect = float(inWidth)/float(inHeight);
					float myAspect = width/height;
					if( IsClose(inAspect, myAspect, 0.05f) ) 
					{
						// if the width is an integer, (3:2) use %d
						if( width == floor(width) )
							sprintf(pszOut, "%i x %i (%d:%d)", inWidth, inHeight, int(width), height);
						else
						{
							// otherwise, (2.35:1) use a float.
							sprintf(pszOut, "%i x %i (%.2f:%d)", inWidth, inHeight,  width, height);

						}

						return true;
					}

					return false;
				}

			};

			static const rezLookup rezzes[] = { {5,4}, {4,3}, {3,2}, {16,10}, {5,3}, {16,9}, {17,9}, {21,9}, {2.35f,1}, {2.37f,1}, {2.39f,1}, {3,1} };
			for(int i=0; i < COUNTOF(rezzes); ++i)
			{
				if( rezzes[i].PrintIfMatches(resolution, width, height) )
					break;
			}

			// didn't print anything == no resolution match, so just skip it.
			if( resolution[0] == '\0' )
#endif // DO_ASPECT_LOOKUP
			{
				sprintf(resolution, "%i x %i", width, height);
			}

			CScaleformMgr::AddParamString( resolution );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleVideoAspect(int iDirection, bool bForceUpdate)
{
	CMenuScreen& gfxData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = gfxData.FindItemIndex(PREF_VID_ASPECT, &onScreenIndex);

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_VID_ASPECT, but totally expected it!" ) )
	{	
		CMenuItem& curItem = gfxData.MenuItems[dataIndex];
		const int MAX_AVILBLE_ASPECT_OPTIONS = 9;
		const char *sOptionTags[MAX_AVILBLE_ASPECT_OPTIONS] = {
			"VID_ASP_AUTO",
			"VID_ASP_32",
			"VID_ASP_43",
			"VID_ASP_53",
			"VID_ASP_54",
			"VID_ASP_169",
			"VID_ASP_1610",
			"VID_ASP_179",
			"VID_ASP_219"
		};
		float fSelectedPhysicalAspect = GetAspectOfCurrentSelectedResolution();
		int iMaxAspectSupported = fSelectedPhysicalAspect > (16.0f / 9.0f) ? 8 : 6;

		int iNewIndex = GetMenuPreference(PREF_VID_ASPECT) + iDirection;

		if(iNewIndex < 0)
			iNewIndex = iMaxAspectSupported;
		else if(iNewIndex > iMaxAspectSupported)
			iNewIndex = 0;

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if( iDirection != 0 && iNewIndex != GetMenuPreference(PREF_VID_ASPECT) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VID_ASPECT, iNewIndex,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(gfxData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0  || bForceUpdate) )
		{
			CScaleformMgr::AddParamString( TheText.Get(sOptionTags[iNewIndex]) );
			CScaleformMgr::EndMethod();
		}
	}
}

float CPauseMenu::GetAspectOfCurrentSelectedResolution()
{
	Settings currentSettings = GetMenuGraphicsSettings();
	float fWidth = (float)currentSettings.m_video.m_ScreenWidth;
	float fHeight = (float)currentSettings.m_video.m_ScreenHeight;

#if SUPPORT_MULTI_MONITOR
	if(GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		fWidth *= (1.0f / 3.0f);
	}
#endif // SUPPORT_MULTI_MONITOR

	return fWidth / fHeight;
}

void CPauseMenu::HandleScreenRefreshRate(int iDirection, bool bForceUpdate)
{
	CMenuScreen& videoData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = videoData.FindItemIndex(PREF_VID_REFRESH, &onScreenIndex);
	int iNewScreenRefreshRate = GetMenuPreference(PREF_VID_REFRESH) + iDirection;
	atArray<grcDisplayWindow> &resolutionsList = CSettingsManager::GetInstance().GetResolutionList(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR));
	atArray<grcDisplayWindow> refreshRateList;
	grcDisplayWindow currentDisplay;
	int iDisplayModes = 0;
	bool bEnabled = GetMenuPreference(PREF_VID_SCREEN_TYPE) == 0;

	if( uiVerifyf(dataIndex != -1, "Couldn't find screen resolution menu item" ) )
	{
		CMenuItem& curItem = videoData.MenuItems[dataIndex];
		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			// Cull the list of duplicate entries sine we're separating the refresh rate into its own menu item.
			GetDisplayInfoList(refreshRateList);

			// Find all the refresh rates for the current resolution
			currentDisplay = resolutionsList[GetMenuPreference(PREF_VID_RESOLUTION)];
			iDisplayModes = CullRefreshRateList(refreshRateList, currentDisplay.uWidth, currentDisplay.uHeight);

			if(iNewScreenRefreshRate < 0)
				iNewScreenRefreshRate = iDisplayModes - 1;
			else if(iNewScreenRefreshRate >= iDisplayModes)
				iNewScreenRefreshRate = 0;

			// don't play the tick sound on initial creation if we had an invalid station
			if( iDirection != 0 && iNewScreenRefreshRate != GetMenuPreference(PREF_VID_REFRESH) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VID_REFRESH, iNewScreenRefreshRate,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(videoData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, bEnabled, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate ) )
		{
			char refreshRate[32];
			float fRefreshRate;
			if (refreshRateList.size() == 0)
			{
				fRefreshRate = 60;
			}
			else
			{
				fRefreshRate= ((float)refreshRateList[iNewScreenRefreshRate].uRefreshRate.Numerator / (float)refreshRateList[iNewScreenRefreshRate].uRefreshRate.Denominator);
			}
			sprintf(refreshRate, "%0.0fHz", fRefreshRate);

			CScaleformMgr::AddParamString( bEnabled ? refreshRate : TheText.Get("VID_ASP_AUTO") );
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleVideoAdapter(int iDirection, bool bForceUpdate)
{
	CMenuScreen& notificationData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = notificationData.FindItemIndex(PREF_VID_ADAPTER, &onScreenIndex);
	int iNewAdapterIndex = GetMenuPreference(PREF_VID_ADAPTER) + iDirection;

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_VID_ADAPTER, but totally expected it!" ) )
	{	
		CMenuItem& curItem = notificationData.MenuItems[dataIndex];
		int iAvalibleAdapters = grcAdapterManager::GetInstance()->GetAdapterCount();

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if(iNewAdapterIndex < 0)
				iNewAdapterIndex = iAvalibleAdapters - 1;
			else if(iNewAdapterIndex >= iAvalibleAdapters)
				iNewAdapterIndex = 0;

			if( iDirection != 0 && iNewAdapterIndex != GetMenuPreference(PREF_VID_ADAPTER) )
			{
				PlaySound("NAV_LEFT_RIGHT");
				SetItemPref(PREF_VID_ADAPTER, iNewAdapterIndex,  UPDATE_PREFS_FROM_MENU);
				HandleShadowSoftness(0, false, true);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(notificationData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate ) )
		{

			CScaleformMgr::AddParamInt(iNewAdapterIndex + 1);
			CScaleformMgr::EndMethod();
		}

		HandleOutputMonitor(0, true);
	}
}

void CPauseMenu::HandleOutputMonitor(int iDirection, bool bForceUpdate)
{
	CMenuScreen& notificationData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS);
	int onScreenIndex = 0;
	int dataIndex = notificationData.FindItemIndex(PREF_VID_MONITOR, &onScreenIndex);
	int iNewOutputMonitor = GetMenuPreference(PREF_VID_MONITOR) + iDirection;

	if( uiVerifyf(dataIndex != -1, "Couldn't find PREF_SPEAKER_OUTPUT, but totally expected it!" ) )
	{	

		CMenuItem& curItem = notificationData.MenuItems[dataIndex];
		int iAvalibleMonitors = CSettingsManager::GetMonitorCount();

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if(iNewOutputMonitor < 0)
				iNewOutputMonitor = iAvalibleMonitors - 1;
			else if(iNewOutputMonitor >= iAvalibleMonitors)
				iNewOutputMonitor = 0;

			if(iNewOutputMonitor < 0)
				iNewOutputMonitor = 0;

			if( iNewOutputMonitor != GetMenuPreference(PREF_VID_MONITOR) )
			{
				if (iDirection != 0)
				{
					PlaySound("NAV_LEFT_RIGHT");
				}
				Settings previousResolution = GetMenuGraphicsSettings();
				SetItemPref(PREF_VID_MONITOR, iNewOutputMonitor,  UPDATE_PREFS_FROM_MENU);
				HandleScreenType(0, true);
				HandleScreenResolution(0, true, true, previousResolution.m_video.m_ScreenWidth, previousResolution.m_video.m_ScreenHeight);
				SetItemPref(PREF_VID_MONITOR, iNewOutputMonitor,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(notificationData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, curItem.MenuPref
			, OPTION_DISPLAY_STYLE_TEXTFIELD, 0, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate ) )
		{

			CScaleformMgr::AddParamInt(iNewOutputMonitor + 1);
			CScaleformMgr::EndMethod();
		}
	}
}

int CPauseMenu::CullRefreshRateList(atArray<grcDisplayWindow> &list, u32 uWidth, u32 uHeight)
{
	for(int i = 0; i < list.size(); ++i)
	{
		if(list[i].uWidth != uWidth || list[i].uHeight != uHeight)
		{
			list.Delete(i);
			i--;
		}
	}

	return list.size();
}

bool CPauseMenu::FullscreenIsAllowed()
{
	Settings vidSettings = CSettingsManager::GetInstance().GetUISettings();
	CSettingsManager::ConvertFromMonitorIndex(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR), vidSettings.m_video.m_AdapterIndex, vidSettings.m_video.m_OutputIndex);

	const grcAdapterD3D11* pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(vidSettings.m_video.m_AdapterIndex);
	const grcAdapterD3D11Output* pMonitorOutput = pAdapter->GetOutput(vidSettings.m_video.m_OutputIndex);

	int iDisplayModes = pMonitorOutput->GetModeCount();
	grcDisplayWindow oDisplayWindow;
	for (int mode = 0; mode < iDisplayModes; mode++)
	{
		pMonitorOutput->GetMode(&oDisplayWindow, mode);
		if (oDisplayWindow.uHeight < oDisplayWindow.uWidth)
			return true;
	}
	return false;
}

int CPauseMenu::GetDisplayInfoList(atArray<grcDisplayWindow> &list)
{
	Settings vidSettings = CSettingsManager::GetInstance().GetUISettings();
	CSettingsManager::ConvertFromMonitorIndex(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR), vidSettings.m_video.m_AdapterIndex, vidSettings.m_video.m_OutputIndex);

	const grcAdapterD3D11* pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(vidSettings.m_video.m_AdapterIndex);
	const grcAdapterD3D11Output* pMonitorOutput = pAdapter->GetOutput(vidSettings.m_video.m_OutputIndex);

	int iDisplayModes = pMonitorOutput->GetModeCount();
	grcDisplayWindow oDisplayWindow;
	for (int mode = 0; mode < iDisplayModes; mode++)
	{
		pMonitorOutput->GetMode(&oDisplayWindow, mode);
		if (oDisplayWindow.uHeight < oDisplayWindow.uWidth)
			list.PushAndGrow(oDisplayWindow);
	}

	if (list.size() == 0)
	{
		DXGI_OUTPUT_DESC desc;
		unsigned int dpiX, dpiY;
		grcAdapterD3D11Output::GetDesc(pAdapter->GetHighPart(), pAdapter->GetLowPart(), pMonitorOutput, desc, dpiX, dpiY);

		oDisplayWindow.uWidth = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
		oDisplayWindow.uHeight = (u32)((float)oDisplayWindow.uWidth / grcDevice::sm_minAspectRatio); 
		list.PushAndGrow(oDisplayWindow);
		if (vidSettings.m_video.m_AdapterIndex != GRCDEVICE.GetAdapterOrdinal() || vidSettings.m_video.m_OutputIndex != GRCDEVICE.GetOutputMonitor())
		{
			return 1;
		}
	}

	grcDisplayWindow customWindow;
	if (CSettingsManager::GetInstance().GetCommandLineResolution(customWindow))
	{
		bool foundMatch = false;
		for(int index = 0; index < list.size(); index++)
		{
			if (list[index].uWidth == customWindow.uWidth && list[index].uHeight == customWindow.uHeight)
				foundMatch = true;
		}

		if (!foundMatch) list.PushAndGrow(customWindow);
	}
	if (CSettingsManager::GetInstance().GetMultiMonitorResolution(customWindow, CPauseMenu::GetMenuPreference(PREF_VID_MONITOR)))
	{
		bool foundMatch = false;
		for(int index = 0; index < list.size(); index++)
		{
			if (list[index].uWidth == customWindow.uWidth && list[index].uHeight == customWindow.uHeight)
				foundMatch = true;
		}

		if (!foundMatch) list.PushAndGrow(customWindow);
	}
	
	/*

	RECT rcWindowRect;
	GetWindowRect( g_hwndMain, &rcWindowRect );
	u32 iWindowWidth = rcWindowRect.right - rcWindowRect.left;
	u32 iWindowHeight = rcWindowRect.bottom - rcWindowRect.top;

	RECT rcClientRect;
	GetClientRect( g_hwndMain, &rcClientRect );
	u32 iClientWidth = rcClientRect.right - rcClientRect.left;
	u32 iClientHeight = rcClientRect.bottom - rcClientRect.top;

	for(int index = 0; index < list.size(); index++)
	{
		if ((list[index].uWidth == iWindowWidth || list[index].uWidth == iClientWidth) &&
			(list[index].uHeight == iWindowHeight || list[index].uHeight == iClientHeight))
			return iDisplayModes;
	}

	customWindow.uWidth = iClientWidth;
	customWindow.uHeight = iClientHeight;
	customWindow.uRefreshRate = 60;

	list.PushAndGrow(customWindow);
*/
	return iDisplayModes;
}
#endif // RSG_PC

#if KEYBOARD_MOUSE_SUPPORT
void CPauseMenu::HandleMouseScale(int iDirection, eMenuPref curPref, bool bForceUpdate)
{
	CMenuScreen& controlData = GetScreenData(MENU_UNIQUE_ID_SETTINGS_MISC_CONTROLS);
	int onScreenIndex = 0;
	int dataIndex = controlData.FindItemIndex(curPref, &onScreenIndex);
	int iNewValue = GetMenuPreference(curPref) + (iDirection * 10);

	if( uiVerifyf(dataIndex != -1, "Couldn't find mouse sensitivity, but totally expected it!" ) )
	{

		CMenuItem& curItem = controlData.MenuItems[dataIndex];

		if( uiVerifyf(iDirection >= -1 && iDirection <= 1, "Received invalid direction %i, expected only -1,0,1!", iDirection) )
		{
			if(iNewValue < 0)
				iNewValue = 0;
			else if(iNewValue > MAX_MOUSE_SCALE)
				iNewValue = MAX_MOUSE_SCALE;

			if( iNewValue != GetMenuPreference(curPref) )
			{
				if (iDirection != 0)
				{
					PlaySound("NAV_LEFT_RIGHT");
				}
				SetItemPref(curPref, iNewValue,  UPDATE_PREFS_FROM_MENU);
			}
		}

		if( CScaleformMenuHelper::SET_DATA_SLOT(static_cast<PM_COLUMNS>(controlData.depth-1), onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD,
			curItem.MenuPref, OPTION_DISPLAY_STYLE_SLIDER, iNewValue, true, TheText.Get(curItem.cTextId.GetHash(),""), false, iDirection != 0 || bForceUpdate))
		{
			CScaleformMgr::AddParamInt(MAX_MOUSE_SCALE);

			eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

			CScaleformMgr::AddParamInt((s32)hudColour);
			CScaleformMgr::EndMethod();
		}
	}
}

void CPauseMenu::HandleMouseLeftRight(int iUniqueID, int iDirection)
{
	sm_iMenuPrefSelected = (eMenuPref)iUniqueID;
	sm_iMouseClickDirection = iDirection;
}


#endif // KEYBOARD_MOUSE_SUPPORT

int CPauseMenu::GetHairColourIndex()
{
#if KEYBOARD_MOUSE_SUPPORT
	return sm_iHairColourSelected;
#else
	return -1;
#endif
}


int CPauseMenu::GetMouseHoverIndex()
{
#if KEYBOARD_MOUSE_SUPPORT
	return sm_MouseHoverEvent.iIndex;
#else
	return -1;
#endif // KEYBOARD_MOUSE_SUPPORT
}

int CPauseMenu::GetMouseHoverMenuItemId()
{
#if KEYBOARD_MOUSE_SUPPORT
	return sm_MouseHoverEvent.iMenuItemId;
#else
	return -1;
#endif // KEYBOARD_MOUSE_SUPPORT
}

int CPauseMenu::GetMouseHoverUniqueId()
{
#if KEYBOARD_MOUSE_SUPPORT
	return sm_MouseHoverEvent.iUniqueId;
#else
	return -1;
#endif // KEYBOARD_MOUSE_SUPPORT
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::GenerateMenuData
// PURPOSE: adds menu data from the xml
/////////////////////////////////////////////////////////////////////////////////////
int CPauseMenu::GenerateMenuData(MenuScreenId iUniqueScreen, bool bUpdateOnly)
{

	MenuArrayIndex iActualScreen = GetActualScreen(iUniqueScreen);
	if( !uiVerifyf(iActualScreen != -1, "Apparently screen %s, %d doesn't have any data in PauseMenu.XML! This is very bad!", iUniqueScreen.GetParserName(), iUniqueScreen.GetValue()) )
	{
		sm_dropIntoMenuWhenStreamed = false;
		return 0;
	}

	CMenuScreen& curScreen = GetScreenDataByIndex(iActualScreen);
	PM_COLUMNS iDepth = static_cast<PM_COLUMNS>(curScreen.depth-1);

	if (iDepth == PM_COLUMN_LEFT)
	{
		SetCurrentScreen(iUniqueScreen);
	}

	if( !curScreen.HasDynamicMenu() || !curScreen.GetDynamicMenu()->InitScrollbar() )
		curScreen.INIT_SCROLL_BAR();

	if (curScreen.MenuItems.GetCount() > 0 && !bUpdateOnly)
	{
		CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(iDepth);
	}

	bool bAlreadyLinkedAtThisDepth = false;
	bool bQueueUpLayoutChanged = false;


	int iItemsAddedAtCurrentDepth = 0;
	int iChildItemsAdded = 0;
	for (s32 iCurrentOption = 0; iCurrentOption < curScreen.MenuItems.GetCount(); iCurrentOption++)
	{
		CMenuItem& curItem = curScreen.MenuItems[iCurrentOption];
		if( !curItem.CanShow() )
			continue;

		switch (curItem.MenuAction)
		{	
			//////////////////////////////////////////////////////////////////////////
			case MENU_OPTION_ACTION_SEPARATOR:
			{
				CScaleformMenuHelper::SET_DATA_SLOT(iDepth, iItemsAddedAtCurrentDepth, -1000 + PREF_OPTIONS_THRESHOLD
					, 0, eOPTION_DISPLAY_STYLE(6), 0, 0, curItem.cTextId.IsNotNull() ? TheText.Get(curItem.cTextId.GetHash(),
#if !__NO_OUTPUT
					atHashString(curItem.cTextId).TryGetCStr()
#else
					""
#endif // !__NO_OUTPUT
					) : "", true, bUpdateOnly, curItem.cTextId.IsNotNull());
			}
			break;

			//////////////////////////////////////////////////////////////////////////
			case MENU_OPTION_ACTION_FILL_CONTENT:
			{
				FillContent(curScreen.MenuScreen);

				// fill content is the only thing this type does
				continue;
			}
			break;

			//////////////////////////////////////////////////////////////////////////
			case MENU_OPTION_ACTION_FILL_CONTENT_FROM_SCRIPT:
			{
				if (sm_iCodeWantsScriptToControlScreen.GetValue() != curScreen.MenuScreen.GetValue())
				{
					// dont want to assert as we want to populate it with the latest regardless if we have already populated it
					//uiAssertf(sm_iCodeWantsScriptToControlScreen.GetValue()==MENU_UNIQUE_ID_INVALID, "Pause menu already has a screen script wants to fill out! Is already %i, but also wants %i!", sm_iCodeWantsScriptToControlScreen.GetValue(), curScreen.MenuScreen.GetValue());
					//			uiDebugf1("     Next Screen will fill content: %d at depth %d", iActualScreen, iDepth);
					sm_iCodeWantsScriptToControlScreen = curScreen.MenuScreen;
				}

				// fill from script is the only thing this type does
				continue;
			}
			break;

			//////////////////////////////////////////////////////////////////////////
			case MENU_OPTION_ACTION_LINK:
			{
				// recursively call this function to fill out the next pane
				if(!bAlreadyLinkedAtThisDepth && Verifyf(curItem.MenuUniqueId != iUniqueScreen, "Screen %d cannot link to itself!", iUniqueScreen.GetValue()))
				{
					// KLUDGE?
				//	iChildItemsAdded = GenerateMenuData(curItem.MenuUniqueId);

					bQueueUpLayoutChanged = true;

				}	
			}
			// FALLTHRU ON PURPOSE (link wants to add a menu item)

			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
			case MENU_OPTION_ACTION_TRIGGER:
			{
				bAlreadyLinkedAtThisDepth = true;
				if(curItem.MenuPref == PREF_GAMMA)
				{
					s32 iStartingPref = CPauseMenu::GetMenuPreference(PREF_GAMMA);
					if(CScaleformMenuHelper::SET_DATA_SLOT(iDepth, iItemsAddedAtCurrentDepth, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD
						, curItem.MenuPref, OPTION_DISPLAY_STYLE_SLIDER, iStartingPref, true, TheText.Get(curItem.cTextId.GetHash(),
						#if !__NO_OUTPUT
							atHashString(curItem.cTextId).TryGetCStr()
						#else
							""
						#endif // !__NO_OUTPUT
						), false, bUpdateOnly))
					{
						CScaleformMgr::AddParamInt(30);

						eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

						CScaleformMgr::AddParamInt((s32)hudColour);
						CScaleformMgr::EndMethod();
					}
				}
				else
				{
					CScaleformMenuHelper::SET_DATA_SLOT(iDepth, iItemsAddedAtCurrentDepth, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD
						, iItemsAddedAtCurrentDepth, OPTION_DISPLAY_NONE, 0, !curItem.HasFlag(InitiallyDisabled), TheText.Get(curItem.cTextId.GetHash(),
						#if !__NO_OUTPUT
							atHashString(curItem.cTextId).TryGetCStr()
						#else
							""
						#endif // !__NO_OUTPUT
						), true, bUpdateOnly);
				}
				

				break;
			}

			//////////////////////////////////////////////////////////////////////////

			case MENU_OPTION_ACTION_INCEPT:
			{
				bAlreadyLinkedAtThisDepth = true;

				// difference between here and above is that we set INCEPT as the menuID.
				CScaleformMenuHelper::SET_DATA_SLOT(iDepth, iItemsAddedAtCurrentDepth, MENU_UNIQUE_ID_INCEPT_TRIGGER + PREF_OPTIONS_THRESHOLD
					, curItem.MenuUniqueId.GetValue(), OPTION_DISPLAY_NONE, 0, !curItem.HasFlag(InitiallyDisabled), TheText.Get(curItem.cTextId.GetHash(),
					#if !__NO_OUTPUT
						atHashString(curItem.cTextId).TryGetCStr()
					#else
						""
					#endif // !__NO_OUTPUT
					));
				break;
			}
			//////////////////////////////////////////////////////////////////////////

			case MENU_OPTION_ACTION_PREF_CHANGE:
			{
				if( HandlePrefOverride( static_cast<eMenuPref>(curItem.MenuPref), 0, bUpdateOnly)  )
					break;

				s32 iNumberOfAllowsPrefSettings = 0;
				atArray<CMenuDisplayValue>& displayValues = GetMenuArray().DisplayValues;
				for (s32 j = 0; j < displayValues.GetCount(); j++)
				{
					if (displayValues[j].MenuOption == curItem.MenuOption)
					{
						iNumberOfAllowsPrefSettings = displayValues[j].MenuDisplayOptions.GetCount();
						break;
					}
				}

				s32 iCurrentPref = (s32)curItem.MenuPref;
				s32 iStartingPref = CPauseMenu::GetMenuPreference(iCurrentPref);
				if (iStartingPref >= iNumberOfAllowsPrefSettings && curItem.MenuOption != MENU_OPTION_SLIDER)
				{
					uiAssertf(0, "Menu %d - Preference %d has starting value of %d but has a range of 0 to %d options. If this is on PC you may need to delete your profile settings located at C:/Users/YourName/Documents/Rockstar Games/GTA V",
						iActualScreen, iCurrentPref, iStartingPref, iNumberOfAllowsPrefSettings - 1);

					iStartingPref = 0;
				}

				bool bItemActive = !curItem.HasFlag(InitiallyDisabled);
				
				if( curItem.MenuOption == MENU_OPTION_SLIDER)
				{
					if( CScaleformMenuHelper::SET_DATA_SLOT(iDepth, iItemsAddedAtCurrentDepth, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, 
						iCurrentPref, OPTION_DISPLAY_STYLE_SLIDER, iStartingPref, bItemActive, TheText.Get(curItem.cTextId.GetHash(),
						#if !__NO_OUTPUT
							atHashString(curItem.cTextId).TryGetCStr()
						#else
							""
						#endif // !__NO_OUTPUT
						), false, bUpdateOnly) )
					{
						if (iCurrentPref == PREF_CONTROLLER_SENSITIVITY || 
							iCurrentPref == PREF_LOOK_AROUND_SENSITIVITY ||
							iCurrentPref == PREF_FPS_LOOK_SENSITIVITY ||
							iCurrentPref == PREF_FPS_AIM_SENSITIVITY || 
							iCurrentPref == PREF_FPS_AIM_DEADZONE ||
							iCurrentPref == PREF_FPS_AIM_ACCELERATION ||
							iCurrentPref == PREF_AIM_DEADZONE ||
							iCurrentPref == PREF_AIM_ACCELERATION)
						{
							CScaleformMgr::AddParamInt(SENSITIVITY_SLIDER_MAX);
						}
						else if(iCurrentPref == PREF_SAFEZONE_SIZE)
						{
							CScaleformMgr::AddParamInt(SAFEZONE_SLIDER_MAX);
						}
						else
						{
							CScaleformMgr::AddParamInt(DEFAULT_SLIDER_MAX);
						}

						eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();

						CScaleformMgr::AddParamInt((s32)hudColour);
						CScaleformMgr::EndMethod();
					}
				}
#if RSG_PC
				else if( curItem.MenuOption == MENU_OPTION_VOICE_FEEDBACK)
				{
					UpdateVoiceBar(false);
				}
#endif // RSG_PC
				else
				{
					if( CScaleformMenuHelper::SET_DATA_SLOT(iDepth, iItemsAddedAtCurrentDepth, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD, 
						iCurrentPref, OPTION_DISPLAY_STYLE_TEXTFIELD, iStartingPref, bItemActive, TheText.Get(curItem.cTextId.GetHash(),
						#if !__NO_OUTPUT
							atHashString(curItem.cTextId).TryGetCStr()
						#else
							""
						#endif // !__NO_OUTPUT
						), false, bUpdateOnly) )
					{
						for (s32 j = 0; j < displayValues.GetCount(); j++)
						{
							if (displayValues[j].MenuOption == curItem.MenuOption)
							{
								for (s32 k = 0; k < displayValues[j].MenuDisplayOptions.GetCount(); k++)
								{
									CScaleformMgr::AddParamLocString(displayValues[j].MenuDisplayOptions[k].cTextId.GetHash());
								}

								break;
							}
						}
						CScaleformMgr::EndMethod();
					}
				}
				break;
			}

			case MENU_OPTION_ACTION_REFERENCE:
				// nothing at all
				continue;
			//////////////////////////////////////////////////////////////////////////
			default:
				uiAssertf(0, "Invalid MenuAction %d on screen %d option %d", (s32)curItem.MenuAction, iActualScreen, iCurrentOption);
				CScaleformMenuHelper::SET_DATA_SLOT(iDepth, iItemsAddedAtCurrentDepth, -1, -1, OPTION_DISPLAY_NONE, 0, true, "", true, bUpdateOnly);
				break;
		}

		// options that DO things instead of ADD things called continue, so they don't hit these sections
		iItemsAddedAtCurrentDepth++;

	} // END OF EVERY ITEM ITERATOR

	if(bUpdateOnly)
	{
		sm_dropIntoMenuWhenStreamed = false;
		return iItemsAddedAtCurrentDepth + iChildItemsAdded;
	}

	if( !curScreen.HasFlag(HandlesDisplayDataSlot) && curScreen.MenuItems.GetCount() > 0 && !bUpdateOnly )
		CScaleformMenuHelper::DISPLAY_DATA_SLOT(iDepth);

	sm_bProcessedContent = true;
	sm_bHasFocusedMenu = true;

	CScaleformMovieWrapper& pmContent = GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

	// some menus need this to be skipped to prevent the columns from being shown after being hidden previously (mostly error conditions)
	if( bQueueUpLayoutChanged && (!curScreen.HasDynamicMenu() || !curScreen.GetDynamicMenu()->ShouldBlockEntrance(iUniqueScreen)) )
		pmContent.CallMethod( "MENU_INTERACTION", PAD_NO_BUTTON_PRESSED );

	// if we're being incepted, we need to poke back into the menu so we can 
	if (iDepth == PM_COLUMN_LEFT )
	{
		SPauseMenuState& curState = sm_aMenuState.Top();
		if( curState.iMenuceptionDir != kMENUCEPT_LIMBO )
		{
			MENU_SHIFT_DEPTH(kMENUCEPT_DEEPER, true, true);

			if( curState.forceFocus != MENU_UNIQUE_ID_INVALID )
			{
				int onScreenIndex;
				int foundIndex = curScreen.FindItemIndex(curState.forceFocus, &onScreenIndex, true);
				if( foundIndex != -1 )
				{
					CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(iDepth, onScreenIndex, true);
					CScaleformMenuHelper::SET_COLUMN_FOCUS(iDepth, true, true, true );
				}
			}
			
			curState.iMenuceptionDir = kMENUCEPT_LIMBO;
			//curState.forceFocus = MENU_UNIQUE_ID_INVALID;
		}
		
	}

	if (sm_dropIntoMenuWhenStreamed)
	{
		sm_waitingForForceDropIntoMenu = false;
		sm_forceDropIntoMenu = !IsNavigatingContent() && curScreen.HasFlag(EnterMenuOnMouseClick);
	}

	sm_dropIntoMenuWhenStreamed = false;

	return iItemsAddedAtCurrentDepth + iChildItemsAdded;
}

bool CPauseMenu::HandlePrefOverride( eMenuPref pref, int iDirection, bool bForceUpdate )
{
	switch( pref )
	{
	case PREF_RADIO_STATION:		HandleRadioStation(iDirection, bForceUpdate);		return true; break;
	case PREF_SPEAKER_OUTPUT:		HandleAudioOutput(iDirection, bForceUpdate);		return true; break;
	case PREF_DIAG_BOOST:			HandleDialogueBoost(iDirection, bForceUpdate);		return true; break;
	case PREF_SS_FRONT:
	case PREF_SS_REAR:				HandleSSWidth(iDirection, pref, bForceUpdate);		return true; break;

#if RSG_PC
	case PREF_VOICE_OUTPUT_DEVICE:	HandleVoiceOutput(iDirection, bForceUpdate);		return true; break;
	case PREF_VOICE_INPUT_DEVICE:	HandleVoiceInput(iDirection, bForceUpdate);			return true; break;
	//case PREF_VOICE_CHAT_MODE:		HandleVoiceChatMode(iDirection, bForceUpdate);		return true; break;
	case PREF_VID_SCREEN_TYPE:		HandleScreenType(iDirection, bForceUpdate);			return true; break;
	case PREF_VID_RESOLUTION:		HandleScreenResolution(iDirection, bForceUpdate);	return true; break;
	case PREF_VID_ASPECT:			HandleVideoAspect(iDirection, bForceUpdate);		return true; break;
	case PREF_VID_REFRESH:			HandleScreenRefreshRate(iDirection, bForceUpdate);	return true; break;
	case PREF_VID_ADAPTER:			HandleVideoAdapter(iDirection, bForceUpdate);		return true; break;
	case PREF_VID_MONITOR:			HandleOutputMonitor(iDirection, bForceUpdate);		return true; break;
	case PREF_GFX_DXVERSION:		HandleDXVersion(iDirection, bForceUpdate);			return true; break;
	case PREF_GFX_TESSELLATION:		HandleTessellation(iDirection, bForceUpdate);		return true; break;
	case PREF_GFX_MSAA:				HandleMSAA(PREF_GFX_MSAA, iDirection, bForceUpdate);			return true; break;
	case PREF_GFX_SCALING:			HandleScaling(iDirection, bForceUpdate);			return true; break;
	case PREF_GFX_REFLECTION_MSAA:	HandleMSAA(PREF_GFX_REFLECTION_MSAA, iDirection, bForceUpdate);	return true; break;
	case PREF_GFX_TXAA:				HandleTXAA(iDirection, bForceUpdate);				return true; break;
	case PREF_GFX_SHADOW_SOFTNESS:	HandleShadowSoftness(iDirection, bForceUpdate);		return true; break;
	case PREF_GFX_GRASS_QUALITY:	HandleGrassQuality(iDirection, bForceUpdate);		return true; break;
	case PREF_GFX_POST_FX:			HandlePostFX(iDirection, bForceUpdate);				return true; break;
	case PREF_GFX_DOF:				HandleDOF(iDirection, bForceUpdate);				return true; break;
	case PREF_GFX_MB_STRENGTH:		HandleMBStrength(iDirection, bForceUpdate);			return true; break;
	case PREF_VID_STEREO:			Handle3DStereo(iDirection, bForceUpdate);			return true; break;
	case PREF_VID_STEREO_CONVERGENCE:	Handle3DStereoConvergence(iDirection, bForceUpdate);	return true; break;
	//case PREF_VID_STEREO_SEPARATION:	Handle3DStereoSeparation(iDirection, bForceUpdate);		return true; break;
	case PREF_LANDING_PAGE:			HandleSkipLandingPage(iDirection, bForceUpdate);	return true; break;
	case PREF_STARTUP_FLOW:			HandleStartupFlow(iDirection, bForceUpdate);		return true; break;
	case PREF_REPLAY_MEM_LIMIT:		HandleReplayMemoryAllocation(iDirection, bForceUpdate); return true; break;
	case PREF_ADV_GFX_SHADOWS_DIST_MULT:	HandleShadowDistMult(iDirection, bForceUpdate); return true; break;
#endif // RSG_PC
#if KEYBOARD_MOUSE_SUPPORT
	case PREF_MOUSE_ON_FOOT_SCALE:
	case PREF_MOUSE_DRIVING_SCALE:
	case PREF_MOUSE_PLANE_SCALE:
	case PREF_MOUSE_HELI_SCALE:	
	case PREF_MOUSE_SUB_SCALE:
		{
			HandleMouseScale(iDirection, pref, bForceUpdate);
			return true; 
			break;
		}

#endif
	case PREF_GAMMA:
		{
			CDisplayCalibration::LoadCalibrationMovie(GetMenuPreference(PREF_GAMMA));
			sm_bWaitOnDisplayCalibrationScreen = true;
			CDisplayCalibration::SetActive(true);
			return true; 
			break;
		}

	default:
		// nothing on purpose
		break;
	}



	return false;
}

void CPauseMenu::RestoreDefaults(MenuScreenId menuScreenID)
{
	switch(menuScreenID.GetValue())
	{
	case MENU_UNIQUE_ID_SETTINGS_AUDIO:
		RestoreAudioDefaults();
		break;
#if RSG_PC
	case MENU_UNIQUE_ID_SETTINGS_GRAPHICS:
		RestoreGraphicsDefaults();
		SUIContexts::SetActive("GFX_Dirty", DirtyGfxSettings() || DirtyAdvGfxSettings());
		break;
	case MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX:
		RestoreAdvancedGraphicsDefaults();
		SUIContexts::SetActive("GFX_Dirty", DirtyAdvGfxSettings() || DirtyAdvGfxSettings());
		break;
	case MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT:
		RestoreVoiceChatDefaults();
		break;
#endif // RSG_PC
#if KEYBOARD_MOUSE_SUPPORT
	case MENU_UNIQUE_ID_SETTINGS_MISC_CONTROLS:
		RestoreMiscControlsDefaults();
		break;
#endif //  KEYBOARD_MOUSE_SUPPORT
	case MENU_UNIQUE_ID_SETTINGS_FIRST_PERSON:
		RestoreFirstPersonDefaults();
		break;
	case MENU_UNIQUE_ID_SETTINGS_REPLAY:
		RestoreReplayDefaults();
		break;
	case MENU_UNIQUE_ID_SETTINGS_CONTROLS:
		RestoreControlDefaults();
		break;
	case MENU_UNIQUE_ID_SETTINGS_DISPLAY:
		RestoreDisplayDefaults();
		break;
	case MENU_UNIQUE_ID_SETTINGS_CAMERA:
		RestoreCameraDefaults();
		break;
	default:
		uiAssertf(0, "Tried to restore defaults on invalid screen %d", menuScreenID.GetValue());
		return;
		break;
	}

	GenerateMenuData(menuScreenID, true);
	PlaySound("SELECT");
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::GetInitialActualScreen
// PURPOSE: returns the actual screen we should start with
/////////////////////////////////////////////////////////////////////////////////////
const MenuScreenId& CPauseMenu::GetInitialScreen()
{
	return GetCurrentMenuVersionData().GetInitialScreen();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdatePlayerInfoAtTopOfScreen
// PURPOSE: updates the display of player info and time info at the top of the screen
//			update of this is based on the change of time, which should be frequent
//			enough since it will be every second.
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::UpdatePlayerInfoAtTopOfScreen(bool bForceUpdate, bool bSendToActionscript)
{
	if (GetCurrentMenuVersionHasFlag(kNoPlayerInfo) || CScriptHud::bUsingMissionCreator)  // dont display player info during Creator (1705567)
	{
		// do we need a hide here?
		return;
	}

#define __ENABLE_CLAN_IMAGE (0)  // no clan image now - 1561953 - see comment "adding note from Jeff"

	static s32 iPreviousHour = -1;
	static s32 iPreviousMins = -1;

	s32 iHour = CClock::GetHour();
	s32 iMinute = CClock::GetMinute();

	// get the date/time:
	char const * cAsciiDayString = TheText.Get(CClock::GetDayOfWeekTextId());

	if (!bForceUpdate)
	{
		if (!NetworkInterface::IsGameInProgress())
		{
			if (bActionScriptPopulated)
			{
				return;
			}
		}
		else
		{
			if ( (iHour == iPreviousHour) && (iMinute == iPreviousMins)  DURANGO_ONLY(&& (sm_displayNameReqID == CDisplayNamesFromHandles::INVALID_REQUEST_ID)) )  // time update added back in for MP - 1561953
			{
				return;  // dont re-display if they are the same as last frame
			}
			else
			{
				bActionScriptPopulated = false;
			}
		}
	}
	else
	{
		bActionScriptPopulated = false;
	}

#if __ENABLE_CLAN_IMAGE
	rlClanId myClanId = RL_INVALID_CLAN_ID;
#endif // #if __ENABLE_CLAN_IMAGE

	if (!bActionScriptPopulated)
	{
		char CashString[128] = {0};
		char cDateTimeString[64] = {"\0"};

		// get the cash:
		if ( !CTheScripts::GetIsInDirectorMode() && CGameWorld::FindLocalPlayer() && !CMiniMap::GetInPrologue() )
		{
			s64 iData = 0;
			if(NetworkInterface::IsInFreeMode())
			{
				iData = MoneyInterface::GetVCWalletBalance( );
			}
			else
			{
				iData = StatsInterface::GetIntStat(STAT_TOTAL_CASH.GetStatId());
			}

			char cashBuff[64];
			CFrontendStatsMgr::FormatInt64ToCash(iData, cashBuff, NELEM(cashBuff));

			if (NetworkInterface::IsGameInProgress())  // for 1438910
			{
				s64 iBankData = 0;
				
				if (!StatsInterface::CloudFileLoadPending(0))  // we want to default to 0 if this fails and not assert to match script
				{
					iBankData = MoneyInterface::GetVCBankBalance();
				}

				char bankBuff[64];
				CFrontendStatsMgr::FormatInt64ToCash(iBankData, bankBuff, NELEM(bankBuff));
				
				// build the string out of text from the text file & also the converted cash and bank cash
				safecpy(CashString, TheText.Get("MENU_PLYR_BANK"), NELEM(CashString));
				safecat(CashString, " ", NELEM(CashString));
				safecat(CashString, bankBuff, NELEM(CashString));
				safecat(CashString, "  ", NELEM(CashString));
				safecat(CashString, TheText.Get("MENU_PLYR_CASH"), NELEM(CashString));
				safecat(CashString, " ", NELEM(CashString));
				safecat(CashString, cashBuff, NELEM(CashString));
			}
			else
			{
				safecpy(CashString, cashBuff, NELEM(CashString));  // just standard cash
			}
		}

		if (!NetworkInterface::IsGameInProgress())
		{
#if !__FINAL
			// add the "percentage completed" stat onto the player name
			formatf(cDateTimeString, NELEM(cDateTimeString), "(%0.2f%% - DEV)  -  %s %02d:%02d", CStatsMgr::GetPercentageProgress(), cAsciiDayString, iHour, iMinute);
#else
			formatf(cDateTimeString, NELEM(cDateTimeString), "%s %02d:%02d", cAsciiDayString, iHour, iMinute);
#endif
		}
		else
		{
			formatf(cDateTimeString, NELEM(cDateTimeString), "%s %02d:%02d", cAsciiDayString, iHour, iMinute);
		}

		bool bPopulateActionScript = true;

#if __ENABLE_CLAN_IMAGE
		NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
#endif
		if (NetworkInterface::IsGameInProgress())
		{
#if __ENABLE_CLAN_IMAGE
			if (clanMgr.HasPrimaryClan())
			{
				const rlClanDesc* pMyClan = clanMgr.GetPrimaryClan();

				myClanId = pMyClan->m_Id;

				if (myClanId != RL_INVALID_CLAN_ID)
				{
					if (!sm_bClanTextureRequested)
					{
						clanMgr.RequestEmblemForClan(myClanId  ASSERT_ONLY(, "CPauseMenu"));
						sm_bClanTextureRequested = true;
					}

					bool bClanDataIsReady = clanMgr.IsEmblemForClanReady(myClanId);

					if (!bClanDataIsReady)
					{
						bPopulateActionScript = false;
					}
				}
			}
#endif // #if __ENABLE_CLAN_IMAGE
		}

		// Don't send the played headshot if the player is in animal form (i.e. an animal model), 
		// as the camera for the mugshot won't be set correctly. Alternatively, we could send a predefined
		// image for this case
		bool bPopulateActionScriptWithoutHeadshot = CTheScripts::GetPlayerIsInAnimalForm();

		if (sm_PedShotHandle == 0 || (!PEDHEADSHOTMANAGER.IsActive((s32)sm_PedShotHandle)))
		{
			bPopulateActionScript = false;
		}

#if RSG_PC
		// Fix for GTA5 Bug 2119716 - Franklin's name is missing the final character when displayed in Japanese
		// Ensure the string is long enough to store utf8 characters.
		char cPlayerName[MAX(RL_MAX_DISPLAY_NAME_BUF_SIZE, 52)] = " "; // default to a space
#else
		char cPlayerName[RL_MAX_DISPLAY_NAME_BUF_SIZE] = " "; // default to a space
#endif

		char cClanName[RL_CLAN_NAME_MAX_CHARS] = "";

		if (NetworkInterface::IsGameInProgress())
		{
			const int localGamerIndex = NetworkInterface::GetLocalGamerIndex(); 
			const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(localGamerIndex);

			if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
			{
#if RSG_DURANGO
				if(sm_displayNameReqID != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
				{
					int nameRetrievalStatus = CLiveManager::GetFindDisplayName().GetDisplayNames(sm_displayNameReqID, &sm_displayName, 1);
					switch(nameRetrievalStatus)
					{
						case CDisplayNamesFromHandles::DISPLAY_NAMES_SUCCEEDED:
							SetPlayerBlipName(sm_displayName);
							// fallthrough on purpose
						case CDisplayNamesFromHandles::DISPLAY_NAMES_FAILED:
							sm_displayNameReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
							break;
						case CDisplayNamesFromHandles::DISPLAY_NAMES_PENDING:
								bPopulateActionScript = false;
								break;
						default:
							break;
					}
				}
				else
#endif
				{
					// Use the cached display name
					safecpy(cPlayerName, sm_displayName, NELEM(sm_displayName));
				}

				if (clanDesc.IsValid())
				{
					safecpy(cClanName, clanDesc.m_ClanName, NELEM(cClanName));
				}
			}
		}
		else
		{
			// In director mode we don't show the player name in the pause menu because the player can be a variety of peds
			if(!CTheScripts::GetIsInDirectorMode())
			{
				CMiniMapBlip *pBlip = CMiniMap::GetBlip(CMiniMap::GetUniqueCentreBlipId());

				if (pBlip)
				{
					safecpy( cPlayerName, CMiniMap::GetBlipNameValue(pBlip), NELEM(cPlayerName) );
				}
			}
		}

		if (bPopulateActionScript && bSendToActionscript)
		{
			CScaleformMovieWrapper& pauseHeader = GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER);

			if (!bActionScriptPopulatedWithHeadshot)
			{
				if (NetworkInterface::IsGameInProgress())
				{
	#if __ENABLE_CLAN_IMAGE
					bool bHaveValidClanName = (strlen(cClanName) > 0);

					//SET_CREW_IMG(txd:String, crewTexturePath:String, show:Boolean)
					if (pauseHeader.BeginMethod( "SET_CREW_IMG"))
					{
						const char* pszEmblem = clanMgr.GetClanEmblemTXDNameForLocalGamerClan();
						pauseHeader.AddParamString(pszEmblem, false);
						pauseHeader.AddParamString(pszEmblem, false);
						pauseHeader.AddParam(bHaveValidClanName);
						pauseHeader.EndMethod();
					}
	#endif // #if __ENABLE_CLAN_IMAGE
				}

				//SET_CHAR_IMG(txd:String, crewTexturePath:String, show:Boolean)
				if (pauseHeader.BeginMethod( "SET_CHAR_IMG"))
				{
					const char* cPedHeadShotTextureName = bPopulateActionScriptWithoutHeadshot ? "" : PEDHEADSHOTMANAGER.GetTextureName((s32)sm_PedShotHandle);
					pauseHeader.AddParamString(cPedHeadShotTextureName, false);
					pauseHeader.AddParamString(cPedHeadShotTextureName, false);
					pauseHeader.AddParam(cPedHeadShotTextureName[0] != '\0');  // fixes 1212873
					pauseHeader.EndMethod();
				}
			}

			if (pauseHeader.BeginMethod( "SET_HEADING_DETAILS"))
			{
				pauseHeader.AddParamString(cPlayerName, false);
				pauseHeader.AddParamString(cDateTimeString, false);  // 955493 - we loose Clan Name, as requested by SteveW
				pauseHeader.AddParamString(CashString, false);
				pauseHeader.AddParam(!NetworkInterface::IsGameInProgress());  // TRUE if SP, FALSE if MP
				pauseHeader.AddParamString(cClanName, false);
				pauseHeader.EndMethod();
			}

			bActionScriptPopulated = true;
			bActionScriptPopulatedWithHeadshot = true;

			Displayf("CPauseMenu:: Player info at top of screen sent to Actionscript");
		}
	}

	iPreviousHour = iHour;
	iPreviousMins = iMinute;
}

void CPauseMenu::SetPlayerBlipName(const char* cName)
{
	// Set the centre blip to use that name for the blip from now on
	CMiniMapBlip *pBlip = CMiniMap::GetBlip(CMiniMap::GetUniqueCentreBlipId());
	if (pBlip)
	{
		CMiniMap::SetBlipNameValue(pBlip, cName, false);
		CPauseMenu::UpdatePauseMapLegend();
	}
}

// wrappers removed from the header so that we're not including CMapMenu in every file
void CPauseMenu::UpdatePauseMapLegend()					{ CMapMenu::UpdatePauseMapLegend(); }
s32  CPauseMenu::GetCurrentSelectedMissionCreatorBlip() { return CMapMenu::GetCurrentSelectedMissionCreatorBlip(); }
bool CPauseMenu::IsHoveringOnMissionCreatorBlip()		{ return CMapMenu::IsHoveringOnMissionCreatorBlip(); }


void CPauseMenu::SetupHeaderTextAndHighlights( CMenuScreen &data )
{
	CScaleformMovieWrapper& pauseHeader = GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER);
	if( !GetCurrentMenuVersionHasFlag(kNoHeaderText) )
	{
		for (s32 i = 0; i < data.MenuItems.GetCount(); i++)
		{
			if ( !data.MenuItems[i].CanShow() )
				continue;

			pauseHeader.CallMethod( "SET_MENU_HEADER_TEXT_BY_INDEX", i, TheText.Get(data.MenuItems[i].cTextId.GetHash(),"") );
		}
	}

	if( GetCurrentMenuVersionHasFlag(kAllHighlighted) )
		pauseHeader.CallMethod("SET_ALL_HIGHLIGHTS", true);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetupMenuHeadings
// PURPOSE: sets up the main menu screen before any menus are created
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetupMenuHeadings()
{
	CScaleformMovieWrapper& pauseContent = GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	CScaleformMovieWrapper& pauseHeader = GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER);

	MenuScreenId iUniqueInitialScreenId = GetInitialScreen();
	CMenuScreen& thisScreen = GetScreenData(iUniqueInitialScreenId);

	if (pauseHeader.BeginMethod( "SET_HEADER_TITLE" ) )
	{
		const atHashWithStringDev& header = GetCurrentMenuVersionData().GetMenuHeader();
		if( header.GetHash() != 0 )
		{
#if ARCHIVED_SUMMER_CONTENT_ENABLED   
			// CnC hack.
			if(sm_iCurrentMenuVersion == FE_MENU_VERSION_MP_PAUSE)
			{
				if(NetworkInterface::IsGameInProgress() && CNewHud::GetDisplayMode() == CNewHud::DM_ARCADE_CNC)
				{
					pauseHeader.AddParam(TheText.Get("FE_THDR_GTA_CNC"));
				}
				else
				{
					pauseHeader.AddParam(TheText.Get(header.GetHash(), ""));
				}
			}
			else
			{
				pauseHeader.AddParam(TheText.Get(header.GetHash(), ""));
			}
#endif
			pauseHeader.AddParam(TheText.Get(header.GetHash(), ""));
		}
		else
		{
			pauseHeader.AddParam(" ");
		}
		pauseHeader.EndMethod();
	}

	pauseHeader.CallMethod("SHOW_HEADING_DETAILS", true);
	
	UpdatePlayerInfoAtTopOfScreen(true, true);

	sm_TabLocked.Reset();
	sm_TabLocked.ResizeGrow(thisScreen.MenuItems.GetCount());
	for (s32 i = 0; i < thisScreen.MenuItems.GetCount(); i++)  // lock all tabs to start with
	{
		sm_TabLocked[i] = true;
	}

	if (pauseContent.BeginMethod( "BUILD_MENU") )
	{
		s32 menuSize = thisScreen.MenuItems.GetCount();

		for (s32 i = 0; i < menuSize; i++)
		{
			if ( thisScreen.MenuItems[i].CanShow() )
			{
				pauseContent.AddParam(thisScreen.MenuItems[i].MenuUniqueId + PREF_OPTIONS_THRESHOLD);
				sm_TabLocked[i] = false;  // unlock them as we build them
			}
		}

		pauseContent.EndMethod();
	}

	if (pauseContent.BeginMethod( "BUILD_MENU_GFX_FILES" ))
	{
		for (s32 i = 0; i < thisScreen.MenuItems.GetCount(); i++)
		{
			if ( thisScreen.MenuItems[i].CanShow() )
			{
				CMenuScreen& childScreen = GetScreenData( thisScreen.MenuItems[i].MenuUniqueId );
				const char* cGfxFilename = childScreen.GetGfxFilename();

				if ( cGfxFilename[0] != '\0')
				{
					pauseContent.AddParamString(cGfxFilename, false);  // string if this menu has a filename
				}
				else
				{
					pauseContent.AddParam(false);  // false if nothing there
				}
			}
		}

		pauseContent.EndMethod();
	}

	SetupHeaderTextAndHighlights(thisScreen);

	pauseHeader.CallMethod("SHOW_MENU", !GetCurrentMenuVersionHasFlag(kHideHeaders) );

	bool bDimmable = !(GetCurrentMenuVersionHasFlag(kNotDimmable) || thisScreen.HasFlag(NotDimmable));
	pauseContent.CallMethod("SET_DIMMABLE",  bDimmable);

	s32 iHudColourForHeaderItem = CNewHud::GetCurrentCharacterColour();
	for (s32 i = 0; i < thisScreen.MenuItems.GetCount(); i++)
	{
		if ( thisScreen.MenuItems[i].CanShow() )
		{
			pauseHeader.CallMethod("SET_MENU_ITEM_COLOUR", i, iHudColourForHeaderItem);
		}
	}
	
	pauseContent.CallMethod("MENU_SECTION_JUMP", sm_iCurrentHighlightedTabIndex, GetCurrentMenuVersionHasFlag(kMenuSectionJump), false);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::AreAllAssetsActive
// PURPOSE:	returns true if all required assets are loaded and ready, false otherwise
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::AreAllAssetsActive()
{
	// pretty quick check to do first
	if( MenuVersionHasHeadings() )
	{
		if( sm_iStreamingMovie == NO_STREAMING_MOVIE )
			return false;

		uiAssertf(sm_iStreamedMovie == NO_STREAMING_MOVIE, "Have have we already streamed the movie for screen %s?!", GetChildMovieHelper(sm_iStreamedMovie).GetRequestingScreen().GetParserName() );

		if( !GetChildMovieHelper(sm_iStreamingMovie).IsMovieReady() )
		{
			uiDebugf3("PauseMenu: Assets are loading... main movie %s", atHashString(GetChildMovieHelper(sm_iStreamingMovie).GetGfxFilename()).TryGetCStr());
			return false;
		}
	}

	MenuList& notReadyList = GetDynamicPauseMenu()->GetNotReadyList();
	int i = notReadyList.GetCount();
	while( i-- )
	{
		// if the movie's not done loading yet, then bail
		if( !notReadyList[i]->IsDoneLoading())
		{
			uiDebugf3("PauseMenu: Assets are loading... customMenu %s is not done loading", notReadyList[i]->GetMenuScreenId().GetParserName());
			return false;
		}
		// clear completed ones from the list
		notReadyList.Delete(i);
	}
	notReadyList.Reset();

	// check if all movies loaded (in reverse for speed, 'cuz why not!)
	const SharedComponentList& optionalComponents = GetDynamicPauseMenu()->GetOptionalSharedComponents();
	
	i = MAX_PAUSE_MENU_BASE_MOVIES;
	while( i-- )
	{
		if( !GetMovieWrapper( eNUM_PAUSE_MENU_MOVIES(i)).IsActive() )
		{
			// just fail for non-extra components
			if( i < PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_START 
			 || i > PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_END )
			{
				uiDebugf3("PauseMenu: Assets are loading... component #%i", i);
				return false;
			}

			// if we're in range for expected components, fail
			if( (i - PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_START) < optionalComponents.GetCount() )
			{
				uiDebugf3("PauseMenu: Assets are loading... optional component #%i", i);
				return false;
			}
			// otherwise, skip it
		}
	}

	// check for textures, kinda slow, so let's do that last
	strLocalIndex iTxdId = g_TxdStore.FindSlot(PAUSEMENU_TXD_PATH);
	if (iTxdId == -1)
	{
		uiDebugf3("PauseMenu: Assets are loading... an unrequested ");
		return false;
	}

	// This should not be a blocking load
	if( !g_TxdStore.HasObjectLoaded(iTxdId) )
	{
		uiDebugf3("PauseMenu: Assets are loading... ");
		return false;
	}


	

	// everything passed! Success.
	return true;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::MenuVersionHasHeadings
// PURPOSE:	whether this menu has any headings in it
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::MenuVersionHasHeadings()
{
	MenuScreenId iUniqueInitialScreenId = GetInitialScreen();
	
	if (iUniqueInitialScreenId != MENU_UNIQUE_ID_INVALID)
	{
		CMenuScreen& iHeaderData = GetScreenData(iUniqueInitialScreenId);
		if (iHeaderData.MenuItems.GetCount() > 0)
		{
			return true;
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetupMenuStructureFromXML
// PURPOSE:	creates the tree from xml data
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::SetupMenuStructureFromXML(int iMode)
{
	// initialise any arrays:
	sm_pCurrentMenuVersion = NULL;
	
	if(iMode == rage::INIT_CORE
		BANK_ONLY( || s_bDebugAlwaysReloadXML )
		)
	{
		DEV_ONLY( sm_bDebugFileFail = false );

		sysMemUseMemoryBucket b(MEMBUCKET_UI);

		CMenuArray& menuArray = GetMenuArray();
		menuArray.Reset();
#if __BANK
		if(s_pXMLTunablesGroup)
		{
			s_pXMLTunablesGroup->Destroy();
			s_pXMLTunablesGroup = NULL;
		}
#endif

		parSettings settings = PARSER.Settings();
		DEV_ONLY( settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, true) );
		DEV_ONLY( settings.SetFlag(parSettings::WARN_ON_UNUSED_DATA, true) );
		DEV_ONLY( settings.SetFlag(parSettings::WARN_ON_MINOR_VERSION_MISMATCH, true) );
		settings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA, true);

		if( !PARSER.LoadObject(PAUSEMENU_DATA_XML_FILENAME, "", menuArray, &settings) )
		{
			DEV_ONLY( sm_bDebugFileFail = true );
			return false;
		}

		if( !uiVerifyf(menuArray.MenuVersions.GetCount() != 0, "No pausemenu menu MenuVersions!") )
		{
			DEV_ONLY( sm_bDebugFileFail = true );
			return false;
		}

		if( !uiVerifyf(menuArray.DisplayValues.GetCount() != 0, "No pausemenu menu DisplayValues!") )
		{
			DEV_ONLY( sm_bDebugFileFail = true );
			return false;
		}
		if( !uiVerifyf(menuArray.MenuScreens.GetCount() != 0, "No pausemenu menu MenuScreens!") )
		{
			DEV_ONLY( sm_bDebugFileFail = true );
			return false;
		}

		BANK_ONLY( CreateXMLDrivenWidgets() );

	}

	return true;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RenderSpecificScreenBackground
// PURPOSE: render any overlays ontop based on menu item we are on
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RenderSpecificScreenBackground(const PauseMenuRenderDataExtra& UNUSED_PARAM(renderData) )
{
	// full black background on brightness calab:
/*	if (GetCurrentPane() == MENU_UNIQUE_ID_SETTINGS_DISPLAY && ShouldDisplayGamma())
	{
		CSprite2d::DrawRectGUI(fwRect(0.0f, 0.0f, 1.0f, 1.0f), Color32(0, 0, 0, 255));

		Vector2 vPos(0.136f, 0.075f);
		Vector2 vSize(0.728f, 0.783f);

		CSprite2d::DrawRectGUI(fwRect(vPos.x, vPos.y, vPos.x+vSize.x, vPos.y+vSize.y), Color32(20, 20, 20, 255));
	}*/
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RenderSpecificScreenOverlays
// PURPOSE: render any overlays ontop based on menu item we are on
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RenderSpecificScreenOverlays(const PauseMenuRenderDataExtra& renderData)
{
	PF_AUTO_PUSH_TIMEBAR("SpecificOverlays");
	if( !renderData.bCurrentScreenValid )
		return;

	renderData.DynamicMenuRender.Call( CallbackData(&renderData));
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetupCodeForPause
// PURPOSE:	gets the code ready to pause
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetupCodeForPause()
{
	if (!NetworkInterface::IsGameInProgress())
	{
		if (g_movieMgr.GetNumMovies() > 0)
		{
			g_movieMgr.PauseAll();
			g_movieMgr.UpdateFrame();
		}
	}

#if USE_DEFRAGMENTATION
	if (!NetworkInterface::IsGameInProgress())
		strStreamingEngine::GetDefragmentation()->SetDefragMode(strDefragmentation::DEFRAG_PAUSE);
#endif // USE_DEFRAGMENTATION

	sm_bScriptWasPaused = false;
	sm_bStartedUserPause = false;
	
	if(!NetworkInterface::IsNetworkOpen())
	{
#if FANCY_TIME_WARP_TECH
		if( !CutSceneManager::GetInstance()->IsRunning() && !GetCurrentMenuVersionHasFlag(kNoTimeWarp) && DynamicMenuExists() )

		{
			sm_iCallbacksPending = 1;
			GetDynamicPauseMenu()->SetTargetTime(TW_Slow, CFA(CPauseMenu::PauseTime));
			audNorthAudioEngine::StartPauseMenuSlowMo();   
		}
		else
#endif
		{
			// just pause instantly if a cutscene is running
			PauseTime();
		}

		sm_bStartedUserPause = true;
	}
	else
	{
		if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
		{
			CGameWorld::FindLocalPlayer()->GetPlayerInfo()->DisableControlsFrontend();
		}
	}

	CUserDisplay::AreaName.ForceSetToDisplay();
	CUserDisplay::DistrictName.ForceSetToDisplay();
}

void CPauseMenu::PauseTime()
{
	sm_iCallbacksPending = 0;

	if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
	{
		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->DisableControlsFrontend();
	}

	//Disable unneccesary render phases
	TogglePauseRenderPhases(false, OWNER_PAUSEMENU, __FUNCTION__ );

	//Change Script Pause
	if (fwTimer::IsScriptPaused())
	{
		fwTimer::EndScriptPause();
		sm_bScriptWasPaused = true;
	}


	//This is needed to restore the timers to what they were before the front end opened.
	//fwTimer::StoreCurrentTime();
	//Change User Pause
	fwTimer::StartUserPause();
}

void CPauseMenu::UnPauseTime()
{
	// kinda crappy interlock so we check for BOTH lerping functions to return
	--sm_iCallbacksPending;
#if OUTRO_EFFECT
	if( sm_iCallbacksPending == 0 )
	{
		GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("BEGIN_EXIT_PAUSE_MENU");
	}
#endif
	
	sm_bStartedUserPause = false;

	// this SEEMS smart. It's not. It's effectively delete this from a callback. Awful.
	//DeleteDynamicPause();
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetupCodeForUnPause
// PURPOSE:	gets the code ready to unpause
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetupCodeForUnPause()
{
#if USE_DEFRAGMENTATION
	if (!NetworkInterface::IsGameInProgress())
		strStreamingEngine::GetDefragmentation()->SetDefragMode(strDefragmentation::DEFRAG_NORMAL);
#endif // USE_DEFRAGMENTATION

	CUserDisplay::AreaName.ForceSetToDisplay();  // lets display the area name when we go back into game
	CUserDisplay::DistrictName.ForceSetToDisplay();

	if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
	{
		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->EnableControlsFrontend();
	}

	//Force an update of the controls to avoid persisting stale input values.
	CControlMgr::Update();

	if (!NetworkInterface::IsGameInProgress())
	{
		if (g_movieMgr.GetNumMovies() > 0)
		{
			g_movieMgr.ResumeAll();
		}
	}

	//Check for User Pause
	if(sm_bStartedUserPause)
	{
		
		//This is needed to restore the timers when the frontend closes.
		//fwTimer::RestoreCurrentTime(); 
		fwTimer::EndUserPause();
#if FANCY_TIME_WARP_TECH
		if( !CutSceneManager::GetInstance()->IsRunning() && DynamicMenuExists() )
		{
#if OUTRO_EFFECT
			sm_iCallbacksPending = 2;
			GetDynamicPauseMenu()->StartScaleEffect(DynamicPauseMenu::SE_Outro, CFA(CPauseMenu::UnPauseTime));
#else
			sm_iCallbacksPending = 1;
#endif
			GetDynamicPauseMenu()->SetTargetTime(TW_Normal, CFA(CPauseMenu::UnPauseTime) );

			audNorthAudioEngine::StopPauseMenuSlowMo();
		}
		else
#endif
		{
			if( DynamicMenuExists() )
				GetDynamicPauseMenu()->SetTargetTime(TW_Normal );

			sm_iCallbacksPending = 0;
			// just unpause instantly if a cutscene is running
#if OUTRO_EFFECT
			CloseComplete();
#endif
			UnPauseTime();
			DeleteDynamicPause();            
		}
	}

	//Check for Script Pause
	if (sm_bScriptWasPaused)
	{
		fwTimer::StartScriptPause();
		sm_bScriptWasPaused = false;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetItemPref
// PURPOSE:	Sets up the menu preference with the new value from ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetItemPref(s32 iPref, s32 iValue, UpdatePrefsSource source)
{
	if (iPref < MAX_MENU_PREFERENCES)
	{
		int iPreviousValue = GetMenuPreference(iPref);
		if (CPauseMenu::SetMenuPreference(iPref, iValue) || iPref != PREF_SAFEZONE_SIZE)  // any pref but only safezone pref if it changes
		{
			CPauseMenu::SetValueBasedOnPreference(iPref, source, iPreviousValue);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::LockMenuTab
// PURPOSE:	locks the menu tab from being able to highlight it and displays a padlock
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::LockMenuTab(s32 iTabNumber, bool bLock)
{
	if (Verifyf(iTabNumber>= 0 && iTabNumber < sm_TabLocked.GetCount(), "Tab %d is not a valid tab to lock/unlock",iTabNumber))
	{
		bool bWasLocked = sm_TabLocked[iTabNumber];
		sm_TabLocked[iTabNumber] = bLock;

		CScaleformMovieWrapper& pauseHeader = GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER);

		pauseHeader.CallMethod("LOCK_MENU_ITEM", iTabNumber, bLock);

		if( bLock && !bWasLocked )
		{
			
			if( sm_iCurrentHighlightedTabIndex == iTabNumber )
			{
				uiErrorf("Currently highlighted tab %s was locked from underneath us by script, probably. Did you open the menuversion with the wrong index in mind? Moving to the next available tab anyway.", GetCurrentHighlightedTabData().MenuScreen.GetParserName());
				TriggerSwitchPane(1);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::PreparePaneSwitch
// PURPOSE:	Preliminary validation and setup for switching to a new menu pane
//			
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::PreparePaneSwitch()
{
	s32 iNumberOfUnlockedTabsFound = 0;  // only allow movement if we have atleast 2 unlocked tabs
	for (s32 i = 0; ( (iNumberOfUnlockedTabsFound < 2) && (i < sm_TabLocked.GetCount()) ); i++)
	{
		if (!sm_TabLocked[i])
		{
			iNumberOfUnlockedTabsFound++;
		}
	}

	UnlockMenuControl();

	if (iNumberOfUnlockedTabsFound < 2)
	{
		uiDebugf1("Not enough unlocked tabs to bother.");
		return false;
	}

	// if they've never started it before, use the 'short' time
	if( !sm_iMenuSelectTimer.IsStarted())
		sm_iMenuSelectTimer.Start(MENU_PANE_MOVEMENT_INTERVAL_SHORT);
	else
		sm_iMenuSelectTimer.Start();

	return true;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::TriggerSwitchPane
// PURPOSE:	deals with switching between menu panes - and triggers the streaming of
//			any child movies required before actionscript is told its jumped
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::TriggerSwitchPane(s32 iMovement)
{
	uiDebugf1("TriggerSwitchPane(%i)", iMovement);

	if(!PreparePaneSwitch())
	{
		return;
	}

	//
	// now deal with highlighting manually:   Needs support from ActionScript
	//	

	//CMenuScreen& cPreviousScreen = GetCurrentHighlightedTabData();
	const MenuScreenId& iUniqueInitialScreenId = GetInitialScreen();
	CMenuScreen& thisScreen = GetScreenData(iUniqueInitialScreenId);

	s32 iMaxTab = thisScreen.MenuItems.GetCount();
	
	while ( iMaxTab > 0 && !thisScreen.MenuItems[iMaxTab-1].CanShow() )  // find last time that is shown
	{
		iMaxTab--;
	};

	do
	{
		sm_iCurrentHighlightedTabIndex += iMovement;

		if (sm_iCurrentHighlightedTabIndex >= iMaxTab)
		{
			sm_iCurrentHighlightedTabIndex = 0;
		}

		if (sm_iCurrentHighlightedTabIndex < 0)
		{
			sm_iCurrentHighlightedTabIndex = (iMaxTab-1);
		}
	} while (sm_TabLocked[sm_iCurrentHighlightedTabIndex]);  // skip any locked tabs
	
	KickoffSwitchPane((iMovement == 0), thisScreen);
}


void CPauseMenu::TriggerSwitchPaneWithTabIndex(s32 iNewTabIndex)
{
	const MenuScreenId& iUniqueInitialScreenId = GetInitialScreen();
	CMenuScreen& thisScreen = GetScreenData(iUniqueInitialScreenId);

	if(!uiVerifyf(iNewTabIndex >= 0 && iNewTabIndex < thisScreen.MenuItems.GetCount(), "Attempting to switch to an out of range menu index %d.", iNewTabIndex) ||
		!uiVerifyf(!sm_TabLocked[iNewTabIndex], "Attempting to switch to a locked menu index %d", iNewTabIndex) ||
		!PreparePaneSwitch())
	{
		return;
	}

	sm_dropIntoMenuWhenStreamed = true;
	sm_iCurrentHighlightedTabIndex = iNewTabIndex;

	KickoffSwitchPane(false, thisScreen);
}

void CPauseMenu::KickoffSwitchPane(bool bRenderContent, CMenuScreen& OUTPUT_ONLY(thisScreen))
{
	// Need to lose focus before the moving to the next tab, so special things like avatars can be removed.
	SetCurrentScreen(MENU_UNIQUE_ID_INVALID);
	SetCurrentPane(MENU_UNIQUE_ID_INVALID);
	/*
	if( IsCurrentScreenValid() )
	{
		if( GetCurrentScreenData().HasDynamicMenu() )
		{
			uiSpew("@@@@@ LOSEF: %s Switch", GetCurrentScreenData().MenuScreen.GetParserName());
			GetCurrentScreenData().GetDynamicMenu()->LoseFocus();
		}
		else
			uiSpew("????? LOSEF: %s Switch", GetCurrentScreenData().MenuScreen.GetParserName());

	}
	else
		uiSpew("????? LOSEF: -Invalid menu- Switch!");
		*/

	for(int i=0; i < PauseMenuRenderData::MAX_SPINNERS;++i)
		SetBusySpinner(false,PM_COLUMN_MAX, i); // wipe out the spinners

	// request new:
	CMenuScreen& cScreen = GetCurrentHighlightedTabData();
	const char* cGfxFilename = cScreen.GetGfxFilename();
//	const char* cGfxPreviousFilename = cPreviousScreen.GetGfxFilename();

//	if (cGfxFilename[0] != '\0' && strcmp(cGfxFilename, cGfxPreviousFilename))
	{
		OUTPUT_ONLY(sfDisplayf("STREAMED_PANE Started streaming '%s - %d'", TheText.Get(thisScreen.MenuItems[sm_iCurrentHighlightedTabIndex].cTextId.GetHash(),""), sm_iCurrentHighlightedTabIndex));

		KickoffStreamingChildMovie(cGfxFilename, cScreen.MenuScreen);
	}

	// automatically decept everything except the last level
	for(int i=0; i < sm_aMenuState.GetCount()-1; ++i)
		sm_aMenuState.Delete(i);
	// craziness handler
	if( !sm_aMenuState.empty() )
		sm_aMenuState.Top().iMenuceptionDir = kMENUCEPT_LIMBO;


	sm_pMsgToWarnOnTabChange = NULL;
	sm_bHasFocusedMenu = false;
	sm_bRenderContent = bRenderContent;
	sm_bMenuLayoutChangedEventOccurred = false;
	sm_bMenuTriggerEventOccurred = false;
	SUIContexts::Deactivate(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));


	GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("MENU_SECTION_JUMP", sm_iCurrentHighlightedTabIndex, false, false);
	PlaySound("TOGGLE_ON");

	// would like to do this but cant as various movies will still have outstanding refs.
/*		// remove any movies that are no longer required:
	for (s32 i = 0; i < sm_MenuArray.MenuScreens[iHeaderMenuId].MenuItems.GetCount(); i++)
	{
		if (i != sm_iStreamingMovie && i != iCurrentHighlightedPane && i != sm_iStreamedMovie)
		{
			if (sm_iMovieId[MAX_PAUSE_MENU_BASE_MOVIES+i].IsActive() )
			{
				sm_iMovieId[MAX_PAUSE_MENU_BASE_MOVIES+i].RemoveMovie();
			}
		}
	}*/
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateSwitchPane
// PURPOSE:	deals with switching between menu panes - and triggers the streaming of
//			any child movies required before actionscript is told its jumped
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::UpdateSwitchPane()
{
	if( sm_iStreamingMovie == NO_STREAMING_MOVIE )
		return;

	const CStreamMovieHelper& rHelper		= GetChildMovieHelper(sm_iStreamingMovie);
	const MenuScreenId&	rRequestingScreen	= rHelper.GetRequestingScreen();

	if( !uiVerifyf(rRequestingScreen != MENU_UNIQUE_ID_INVALID, "How the FRACK is an invalid requesting panel being set up!? We'll have to fix this, but in the meantime, let's not crash. You WILL load forever, though."))
		return;

	const CMenuScreen& rTargetScreen		= GetScreenData(rRequestingScreen);

	bool bSettleOnMenu = false;
	bool bInstantlySettleOnMenu = false;

#if RSG_ORBIS
	bool const c_isFriendsMenuRequested = rRequestingScreen == MENU_UNIQUE_ID_FRIENDS || rRequestingScreen == MENU_UNIQUE_ID_FRIENDS_MP;
	bool const c_isFriendsIntervalDifferent = c_isFriendsMenuRequested && sm_iFriendPaneMovementTunable != sm_iFriendPaneMovementInterval;
	int const c_friendIntervalValue = c_isFriendsIntervalDifferent ? sm_iFriendPaneMovementTunable : sm_iFriendPaneMovementInterval;

	int const c_menuPaneMovementInterval = c_isFriendsMenuRequested ? c_friendIntervalValue : MENU_PANE_MOVEMENT_INTERVAL ;
#else
	int const c_menuPaneMovementInterval = MENU_PANE_MOVEMENT_INTERVAL;
#endif

	if ( sm_bWaitingForFirstLayoutChanged || sm_iMenuSelectTimer.IsComplete(c_menuPaneMovementInterval,false) || rHelper.GetMenuceptionDir() != kMENUCEPT_LIMBO)
	{
		if( rHelper.IsMovieReady() )
		{
			bSettleOnMenu = true;
			sm_iMenuSelectTimer.Start();
		}
	}

	// these 2 script menus do not want the delay:
	if( GetCurrentMenuVersionHasFlag(kNoSwitchDelay) )
	{
		bSettleOnMenu = true;
		bInstantlySettleOnMenu = true;
	}

	if( bSettleOnMenu )
	{
		bool bNewGfx = true;
	
		uiDebugf1("STREAMED_PANE New pane '%s' has streamed movie '%s'", rHelper.GetRequestingScreen().GetParserName(), rHelper.GetGfxFilenameForDebug());

		if( sm_aMenuState.empty() || sm_aMenuState.Top().iMenuceptionDir == kMENUCEPT_LIMBO )
			SetNavigatingContent(false);

		if( !sm_bClosingDown )
		{
			if( sm_bRenderMenus )
			{
				if( sm_bRenderContent )
				{
					bNewGfx	= false;
				}
				else
				{
					bool bTabsAreColumns = GetCurrentMenuVersionHasFlag(kTabsAreColumns);
					SUIContexts::SetActive(UIATSTRINGHASH("TABS_ARE_COLUMNS", 0x54fedb77), bTabsAreColumns);

					if(bTabsAreColumns && sm_iStreamedMovie != NO_STREAMING_MOVIE)
					{
						bNewGfx = false;
					}
				}
			}


			// if this was for menuception, be sure to invoke the proper function
			if( rHelper.GetMenuceptionDir() != kMENUCEPT_LIMBO )
			{
				GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("LOAD_CHILD_PAGE"
					, rTargetScreen.cGfxFilename.c_str()
					, rRequestingScreen.GetValue()+PREF_OPTIONS_THRESHOLD
					, rHelper.GetMenuceptionDir()
					);
			}
			else
			{
					// will be turned on when Actionscript is ready
				CMenuScreen& rHeader = GetScreenData(GetInitialScreen());
				int iNewTabIndex = rHeader.FindItemIndex(rRequestingScreen);
				uiAssertf(iNewTabIndex!=-1, "Current menu %s doesn't have %s as a tab?.", GetInitialScreen().GetParserName(), rRequestingScreen.GetParserName());

				GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("MENU_SECTION_JUMP"
					, iNewTabIndex
					, bNewGfx
					, bInstantlySettleOnMenu
					);

				if( !sm_aMenuState.empty() )
					sm_aMenuState.Top().forceFocus = MENU_UNIQUE_ID_INVALID;
			}
		}
		else
		{
			uiDebugf1("Skipping MENU_SECTION_JUMP/LOADED_PAGE because we're shutting down.");
		}

		if( !bNewGfx || bInstantlySettleOnMenu )
		{
			sm_bRenderContent = true;
		}

		sm_iStreamedMovie = sm_iStreamingMovie;
		sm_iStreamingMovie = NO_STREAMING_MOVIE;
	}
}

bool CPauseMenu::AnyLayerHasFlag(eMenuScreenBits kFlag)
{
	// we don't have a stack yet, but this is roughly close?
	return GetCurrentScreenHasFlag(kFlag) || GetCurrentActivePanelHasFlag(kFlag);
}



void CPauseMenu::ScaleContentMovie(bool bScale)
{	
	if (bScale)
	{
		// fullscreen
		GFxMovieView::ScaleModeType scaleMode;
		if (!CHudTools::GetWideScreen())
			scaleMode = GFxMovieView::SM_NoBorder;		
		else
			scaleMode = GFxMovieView::SM_ShowAll;

		CScaleformMgr::ChangeMovieParams(CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).GetMovieID(), Vector2(0.0f,0.0f), Vector2(1.0f,1.0f), scaleMode);
	}
	else
	{
		const char* pszTuningToUse = GetCurrentMenuVersionHasFlag(kUseAlternateContentPos) ? "PAUSE_MENU_SP_CONTENT_ALT" : "PAUSE_MENU_SP_CONTENT";
		const SGeneralPauseDataConfig* pData = CPauseMenu::GetMenuArray().GeneralData.MovieSettings.Access(pszTuningToUse);

		if (uiVerifyf(pData, "No data config for %s!", pszTuningToUse))
		{
			Vector2 vMoviePos ( pData->vPos  );
			Vector2 vMovieSize( pData->vSize );

			CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(pData->HAlign), &vMoviePos, &vMovieSize);

			CScaleformMgr::ChangeMovieParams(CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).GetMovieID(), vMoviePos, vMovieSize, GFxMovieView::SM_ExactFit);
		}
	}

	// lets tell actionscript we have scaled or not scaled
	Vector2 safeZoneMin, safeZoneMax;
	CHudTools::GetMinSafeZone(safeZoneMin.x, safeZoneMin.y, safeZoneMax.x, safeZoneMax.y);

	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	

	if( pauseContent.BeginMethod("SET_CONTENT_SCALED") )
	{
		pauseContent.AddParam(bScale);
		pauseContent.AddParam(safeZoneMin.x);
		pauseContent.AddParam(safeZoneMin.y);
		pauseContent.AddParam(safeZoneMax.y);
		pauseContent.AddParam(safeZoneMax.y);
		pauseContent.EndMethod();
	}
	
	#if RSG_PC
	CScaleformMovieWrapper& pauseHeader = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER);
	if( pauseHeader.BeginMethod("LOCK_MOUSE_SUPPORT") )
	{
		pauseHeader.AddParam(!bScale); //mouseClicksOn
		pauseHeader.AddParam(!bScale); //rolloversOn
		pauseHeader.EndMethod();
	}
	#endif

	CNewHud::UpdateDisplayConfig(pauseContent.GetMovieID(), SF_BASE_CLASS_PAUSEMENU);  // send the screen info to the movie
	UpdateDisplayConfig();
}

void CPauseMenu::SetClosingAction(ClosingAction eNewAction)
{
	//uiAssertf(eNewAction==CA_None || sm_eClosingAction == CA_None, "Attempted to set TWO closing actions! This means you gotta make it a bitfield or some shit!");
	sm_eClosingAction = eNewAction;
}

void CPauseMenu::ShowTabChangeWarning(s32 iDirection, bool bIsTabIndex)
{
	datCallback acceptCB;
	if( bIsTabIndex )
		acceptCB = datCallback(CFA1(CPauseMenu::TriggerSwitchPaneWithTabIndex), reinterpret_cast<CallbackData>(iDirection) );
	else
		acceptCB = datCallback(CFA1(CPauseMenu::TriggerSwitchPane),             reinterpret_cast<CallbackData>(iDirection) );

	ShowConfirmationAlert(sm_pMsgToWarnOnTabChange, acceptCB);
	PlaySound("TOGGLE_ON");
}

void CPauseMenu::ShowConfirmationAlert(const char* pMessage, datCallback functionOnAccept )
{
	CWarningMessage::Data newData;
	newData.m_TextLabelHeading = "GLOBAL_ALERT_DEFAULT";
	newData.m_TextLabelBody = pMessage;
	newData.m_iFlags = FE_WARNING_YES_NO;
	newData.m_acceptPressed = functionOnAccept;
	newData.m_bCloseAfterPress = true;
	GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newData);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateInput
// PURPOSE:	sends any input (the current button pressed) to ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::UpdateInput(const bool bLockInputThisFrame)
{
#if __PPU
	if(ioPad::IsIntercepted())  // CLiveManager::IsSystemUiShowing();?? Should we be using this instead?
		return;
#endif

	if (sm_iLoadingAssetsPhase!=PMLP_DONE || !sm_bProcessedContent)
		return;

	u32 iInputFlag = CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE;

	if (bLockInputThisFrame)  // menu control is locked (or unlocked this frame), so dont allow any input apart from Switching the panes (fixes bug 976284)
	{
		if( !GetCurrentMenuVersionHasFlag(kNoTabChangeWhileLocked) && !GetCurrentMenuVersionHasFlag(kNoTabChange) )  // [originally] fixes 1000171
		{
			if (CPauseMenu::CheckInput(FRONTEND_INPUT_LB, false, iInputFlag))
			{
				if( sm_pMsgToWarnOnTabChange )
				{
					ShowTabChangeWarning(-1, false);
				}
				else
				{
#if RSG_PC
					if(DirtyGfxSettings() || DirtyAdvGfxSettings())
					{
						SetBackedWithGraphicChanges(true);

						if(GetCurrentScreen() != MENU_UNIQUE_ID_SETTINGS_GRAPHICS || GetCurrentScreen() != MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
						{
							uiAssertf(0,"Attempted to tab left and right with dirty graphics settings while not on a graphics screen. Please delete your settings.xml file and if you still get this message open a bug for Default UI Code");
							TriggerSwitchPane(-1);
						}
					}

					if(!DirtyGfxSettings() && !DirtyAdvGfxSettings())
#endif // RSG_PC
					{
						TriggerSwitchPane(-1);
					}
				}
			}

			if (CPauseMenu::CheckInput(FRONTEND_INPUT_RB, false, iInputFlag))
			{
				if( sm_pMsgToWarnOnTabChange )
				{
					ShowTabChangeWarning(1, false);
				}
				else
				{
#if RSG_PC
					if(DirtyGfxSettings() || DirtyAdvGfxSettings())
					{
						SetBackedWithGraphicChanges(true);

						if(GetCurrentScreen() != MENU_UNIQUE_ID_SETTINGS_GRAPHICS || GetCurrentScreen() != MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
						{
							uiAssertf(0,"Attempted to tab left and right with dirty graphics settings while not on a graphics screen. Please delete your settings.xml file and if you still get this message open a bug for Default UI Code");
							TriggerSwitchPane(1);
						}
					}
					
					if(!DirtyGfxSettings() && !DirtyAdvGfxSettings())
#endif // RSG_PC
					{
						TriggerSwitchPane(1);
					}
				}
			}
		}

#if RSG_PC
		// spoof a right click for PC in these circumstances
		// this WAS handled by Scaleform, but it is unreliable and grosser than it needs to be
		if( IsNavigatingContent() && CheckInput(FRONTEND_INPUT_CURSOR_BACK, true) )
		{
			MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER);
		}
#endif

		return;
	}

	// if we haven't started rendering yet, don't let control happen
	if( !sm_bRenderMenus )
		return;

	//
	// check "are you sure" on new game
	//
	if (sm_bWaitOnNewGameConfirmationScreen)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "MO_NEW", "NG_SURE", FE_WARNING_YES_NO);
		eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

		if( result == FE_WARNING_YES )
		{
			sm_bWaitOnNewGameConfirmationScreen = false;
			SetClosingAction(CA_StartNewGame);
			Close();
		}

		else if ( result == FE_WARNING_NO )
		{
			sm_bWaitOnNewGameConfirmationScreen = false;
		}

		return;
	}

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	if (sm_bWaitOnImportConfirmationScreen)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "MO_IMP", "MO_IMP_SURE", FE_WARNING_YES_NO);
		eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

		if( result == FE_WARNING_YES )
		{
			sm_bWaitOnImportConfirmationScreen = false;
#if GEN9_LANDING_PAGE_ENABLED
			CLandingPage::SetShouldLaunch(LandingPageConfig::eEntryPoint::SINGLEPLAYER_MIGRATION);
#else
			Assertf(0, "You tried to launch into the save game download page, this seems to not be available!!!");
#endif
			Close();
		}

		else if ( result == FE_WARNING_NO )
		{
			sm_bWaitOnImportConfirmationScreen = false;
		}

		return;
	}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if RSG_PC
	if (sm_bWaitOnExitToWindowsConfirmationScreen)
	{
		if (sm_iExitTimer == 0)
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS", "EXIT_SURE", FE_WARNING_YES_NO);
			eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

			if( result == FE_WARNING_YES )
			{
				sm_iExitTimer = fwTimer::GetSystemTimeInMilliseconds();

			}

			else if ( result == FE_WARNING_NO )
			{
				sm_bWantsToExitGame = false;
				sm_bWaitOnExitToWindowsConfirmationScreen = false;
			}

			return;
		}
		else
		{
			if (fwTimer::GetSystemTimeInMilliseconds() + EXIT_TO_WINDOWS_DELAY > sm_iExitTimer )
			{
				sm_iExitTimer = 0;
				CPauseMenu::SetGameWantsToExitToWindows(true);
				NetworkInterface::GetNetworkExitFlow().StartShutdownTasks();
				sm_bWaitOnExitToWindowsConfirmationScreen = false;
			}

			return;
		}
	}
#endif
	
	if (CWarningScreen::IsActive() || SReportMenu::GetInstance().IsActive())
		return;

#if RSG_PC
	if(STextInputBox::GetInstance().IsActive())
		return;
#endif // RSG_PC

#if GTA_REPLAY
	if(GetCurrentScreen() == MENU_UNIQUE_ID_REPLAY_EDITOR)
	{
		if(SUIContexts::IsActive("SHOW_VIDEO_BUTTON") && SUIContexts::IsActive("ON_VIDEO_TAB"))
		{
			if (CPauseMenu::CheckInput(FRONTEND_INPUT_X, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS | iInputFlag))
			{
				g_SystemUi.ShowWebBrowser(NetworkInterface::GetLocalGamerIndex(), CPauseVideoEditorMenu::GetButtonURL());
			}
		}
	}
#endif // GTA_REPLAY

	s32 pressed = PAD_NO_BUTTON_PRESSED;
	bool bScrollPressedOnPartyList = false;

	if (sm_bNavigatingContent && GetCurrentScreen() == MENU_UNIQUE_ID_MAP)
	{
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_UP, sm_bNavigatingContent, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS | iInputFlag))
			pressed = PAD_DPADUP;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN, sm_bNavigatingContent, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS | iInputFlag))
			pressed = PAD_DPADDOWN;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS | iInputFlag))
			pressed = PAD_DPADLEFT;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS | iInputFlag))
			pressed = PAD_DPADRIGHT;
#if KEYBOARD_MOUSE_SUPPORT
		GetScrollPressed(pressed, false);
#endif // KEYBOARD_MOUSE_SUPPORT
	}
	else if (sm_bNavigatingContent && GetCurrentScreen() == MENU_UNIQUE_ID_GALLERY)
	{
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_UP, false, iInputFlag))
			pressed = PAD_DPADUP;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN, false, iInputFlag))
			pressed = PAD_DPADDOWN;
	
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, iInputFlag))
				pressed = PAD_DPADLEFT;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, iInputFlag))
				pressed = PAD_DPADRIGHT;

#if KEYBOARD_MOUSE_SUPPORT
		GetScrollPressed(pressed, false);
#endif // KEYBOARD_MOUSE_SUPPORT
	}
	else if (sm_bNavigatingContent && GetCurrentScreen() == MENU_UNIQUE_ID_PARTY_LIST)
	{
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_UP, false, iInputFlag))
			pressed = PAD_DPADUP;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN, false, iInputFlag))
			pressed = PAD_DPADDOWN;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, iInputFlag))
			pressed = PAD_DPADLEFT;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, iInputFlag))
			pressed = PAD_DPADRIGHT;

#if KEYBOARD_MOUSE_SUPPORT
		bScrollPressedOnPartyList = GetScrollPressed(pressed, false);
#endif // KEYBOARD_MOUSE_SUPPORT
	}
	else
	{
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_UP, ShouldPlayNavigationSound(true), iInputFlag))
			pressed = PAD_DPADUP;

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN, ShouldPlayNavigationSound(false), iInputFlag))
			pressed = PAD_DPADDOWN;

		if (!sm_bNavigatingContent)
		{
			if ((!CControlMgr::GetMainFrontendControl().GetFrontendLB().IsDown()) &&  // fixes 1521523
				(!CControlMgr::GetMainFrontendControl().GetFrontendRB().IsDown()))
			{
				u32 iAlteredFlags = iInputFlag;
#if KEYBOARD_MOUSE_SUPPORT
				// On PC, if the user is using a keyboard to navigate the menu, allow the arrow keys to select tabs B* 1645391.
				if(CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetSource().m_DeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE)
#endif // KEYBOARD_MOUSE_SUPPORT
				{
					iAlteredFlags |= CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS;
				}
				if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, iAlteredFlags))  // dpad is used for same as L1 and R1 when in header menu
					pressed = PAD_LEFTSHOULDER1;

				if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, iAlteredFlags))
					pressed = PAD_RIGHTSHOULDER1;
			}

#if RSG_PC
			if (!sm_bMaxPayneMode && CPauseMenu::CheckInput(FRONTEND_INPUT_SELECT, false, iInputFlag))
			{
				g_rlPc.ShowUi();
			}
#endif
		}
		else
		{
			if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, iInputFlag))
				pressed = PAD_DPADLEFT;

			if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, iInputFlag))
				pressed = PAD_DPADRIGHT;

#if KEYBOARD_MOUSE_SUPPORT
			if(sm_iMouseClickDirection != 0)
			{
				pressed = sm_iMouseClickDirection > 0 ? PAD_DPADRIGHT : PAD_DPADLEFT;
				sm_iMouseClickDirection = 0;
			}

			GetScrollPressed(pressed, true);
#endif // KEYBOARD_MOUSE_SUPPORT
		}
	}

	bool bForceIntoMenuOnClick = false;
#if RSG_PC
	// on PC code does a couple of mouse checks when not navigating content:
	if (!IsNavigatingContent())
	{
		bool const c_mouseLeftClick = (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT) != 0;

		if (c_mouseLeftClick)
		{
			if(CControlMgr::GetMainFrontendControl().GetFrontendAccept().IsEnabled() && MenuVersionHasHeadings() )
			{
				// drop into the highlighted menu when we click the content movie with the mouse
				const Vector2 c_mousePos(ioMouse::GetNormX(), ioMouse::GetNormY());
				const char* pszTuningToUse = GetCurrentMenuVersionHasFlag(kUseAlternateContentPos) ? "PAUSE_MENU_SP_CONTENT_ALT" : PAUSEMENU_FILENAME_CONTENT;
				const SGeneralPauseDataConfig& c_pauseMenuContent = GetMenuArray().GeneralData.MovieSettings[pszTuningToUse];
				Vector2 c_movieSize = c_pauseMenuContent.vSize;
				Vector2 c_moviePos =  c_pauseMenuContent.vPos;

				CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(c_pauseMenuContent.HAlign), &c_moviePos, &c_movieSize);
				if (c_mousePos.x >= c_moviePos.x &&
					c_mousePos.x <= c_moviePos.x+c_movieSize.x &&
					c_mousePos.y >= c_moviePos.y &&
					c_mousePos.y <= 0.86f)  // bottom of the content
				{
					if (!sm_dropIntoMenuWhenStreamed)
					{
						bForceIntoMenuOnClick = !IsNavigatingContent();
					}
				}
			}
			else
				uiDebugf1("Would've attempted to click a menu but FRONTEND_ACCEPT is disabled");
		}
	}
#endif // #if RSG_PC
	
	if (IsNavigatingContent())  // if we are navigating content, then reset these flags
	{
		sm_waitingForForceDropIntoMenu = false;
		sm_forceDropIntoMenu = false;
	}
	else
	{
		if (sm_forceDropIntoMenu)  // move into state of waiting to force into menu when we can
		{
			sm_waitingForForceDropIntoMenu = true;
			sm_forceDropIntoMenu = false;
		}
		else
		{
			if (IsCurrentScreenValid() && !GetCurrentScreenData().HasFlag(EnterMenuOnMouseClick))
			{
				sm_waitingForForceDropIntoMenu = false;
			}
		}
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, false, iInputFlag) || 
		((sm_waitingForForceDropIntoMenu || bForceIntoMenuOnClick) && sm_iCodeWantsScriptToControlScreen == MENU_UNIQUE_ID_INVALID))
	{
		sm_dropIntoMenuWhenStreamed = false;

		if ( (GetCurrentScreen() != MENU_UNIQUE_ID_MAP) || (!sm_bNavigatingContent) )  // dont want to send this when we trigger a waypoint on the map screen
		{
			if (IsInSaveGameMenus())
			{
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
				if (sm_bUseManualSaveMenuToImportSavegame)
				{
					sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_IMPORT_SP_SAVE;
				}
				else
#endif	//	__ALLOW_IMPORT_OF_SP_SAVEGAMES
				{
					sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_SAVE;
				}
			}
			else if (IsInLoadGamePanel())
			{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
				if (sm_bUseManualLoadMenuToExportSavegame)
				{
					sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_EXPORT_SP_SAVE;
				}
				else
#endif	//	__ALLOW_EXPORT_OF_SP_SAVEGAMES
				{
					sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_LOAD;
				}
			}
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
			else if (IsInUploadSavegamePanel())
			{
				sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_EXPORT_SP_SAVE;
			}
#endif	//	__ALLOW_EXPORT_OF_SP_SAVEGAMES

			pressed = PAD_CROSS;
		}
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_X, false, iInputFlag))
	{
		if ( SUIContexts::IsActive(DELETE_SAVEGAME_CONTEXT) && (IsInLoadGamePanel() || IsInSaveGameMenus()) )
		{
			//	If the player presses X/Square then I set the flag but tell scaleform that they pressed A/Cross
			sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_DELETE;
			pressed = PAD_CROSS;
		}
		else
		{
			pressed = PAD_SQUARE;
		}
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_Y, false, iInputFlag) )
	{
		pressed = PAD_TRIANGLE;
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_BACK, false, iInputFlag))
		pressed = PAD_CIRCLE;

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_LT, false, iInputFlag))
		pressed = PAD_LEFTSHOULDER2;

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_RT, false, iInputFlag))
		pressed = PAD_RIGHTSHOULDER2;

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_LB, false, iInputFlag))
		pressed = PAD_LEFTSHOULDER1;

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_RB, false, iInputFlag))
		pressed = PAD_RIGHTSHOULDER1;

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_SELECT, false, iInputFlag))
		pressed = PAD_SELECT;

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_R3, false, iInputFlag))
		pressed = PAD_R3;

	// process individual input
	if( !sm_bMaxPayneMode && IsCurrentScreenValid() && sm_iStreamingMovie == NO_STREAMING_MOVIE)
	{
		CMenuScreen& curScreen = GetCurrentScreenData();
		// context menus get it first
		if( curScreen.HandleContextMenuInput(pressed) )
			return;

		#if RSG_PC
		if ( SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsMovieActive())
		{
			if (SMultiplayerChat::GetInstance().IsChatTyping())
				return;
		}
		#endif

		if (!sm_waitingForForceDropIntoMenu ||
			(GetCurrentScreen() != MENU_UNIQUE_ID_FRIENDS && GetCurrentScreen() != MENU_UNIQUE_ID_FRIENDS_MP))
		{
			// give dynamic menus second crack at overriding basic functionality
			if( curScreen.HasDynamicMenu() && curScreen.GetDynamicMenu()->UpdateInput(pressed) )
				return;
		}
	}

	if( sm_bMaxPayneMode && pressed != PAD_NO_BUTTON_PRESSED && pressed != PAD_TRIANGLE )
	{
		// for sanity, fake Circle presses as Triangle presses to back out of Max Payne Mode
		if( pressed == PAD_CIRCLE )
		{
			pressed = PAD_TRIANGLE;
		}
		else
		{
			uiDebugf3("User wanted to press %i, but we've hidden the menu so it's suppressed", pressed);
			pressed = PAD_NO_BUTTON_PRESSED;
		}
	}

	if (pressed != PAD_NO_BUTTON_PRESSED)
	{
		// Ignore input if we're menucepting and not changing tabs
		if(CPauseMenu::IsMenucepting() && pressed != PAD_LEFTSHOULDER1 && pressed != PAD_RIGHTSHOULDER1)
		{
			uiDebugf3("User wanted to press %i, but we've ignored the input because we're in the process of menucepting", pressed);
			return;
		}

		switch (pressed)
		{
			case PAD_CROSS:
			{
				// don't let you navigate into the menu if it hasn't loaded yet
				if( sm_bHasFocusedMenu && !GetCurrentActivePanelHasFlag(Input_NoAdvance) && !SUIContexts::IsActive(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54)) )
				{
					bool bGalleryOverride =  GetCurrentScreen() == MENU_UNIQUE_ID_GALLERY && !IsNavigatingContent();

					// Ensure we're not waiting on script to populate before menu cepting deeper
					if (!sm_bLockTheAcceptButton && !bGalleryOverride)
					{
						MENU_SHIFT_DEPTH(kMENUCEPT_DEEPER);
					}
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
					else
					{
						if (sm_bLockTheAcceptButton && GetCurrentActivePanel() == MENU_UNIQUE_ID_PROCESS_SAVEGAME)
						{
							sm_bDelayedEntryToExportSavegameMenu = true;
						}					
					}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
				}
				break;
			}

			case PAD_CIRCLE:
			{
				// don't let you press B if you're in the process of menucepting, because it breaks a few things.
				if (sm_bRenderMenus)
				{
					if( !(GetCurrentActivePanelHasFlag(Input_NoBack) 
						|| (GetCurrentActivePanelHasFlag(Input_NoBackIfNavigating) && IsNavigatingContent())
						)
						)
					{ 
						bool bAllowClose = GetCurrentMenuVersionHasFlag(kAllowBackingOut);

						sm_iLastValidPref = sm_iMenuPrefSelected;
						MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER, !bAllowClose);

						if (IsInSaveGameMenus())  // any layout change means close it down when in savegame screen (as there should only ever be one layout)
						{
							BackOutOfSaveGameList();

							Close();
						}
						else if (IsNavigatingContent())  // only play the back sound when we are navigating the content
						{
							PlaySound("BACK");
						}
						else
						{
							//
							// play exit sound
							//
							if (NetworkInterface::IsGameInProgress())
								g_FrontendAudioEntity.PlaySound("RESUME","HUD_FRONTEND_MP_SOUNDSET");
							else
								g_FrontendAudioEntity.PlaySound("RESUME",s_HudFrontendSoundset);
						}
					}
				}

				break;
			}

			case PAD_LEFTSHOULDER1:
			{
				if( !AnyLayerHasFlag(Input_NoTabChange) && !GetCurrentMenuVersionHasFlag(kNoTabChange) )
				{
					if( sm_pMsgToWarnOnTabChange )
					{
						ShowTabChangeWarning(-1, false);

					}
					else
					{
#if RSG_PC
						if(DirtyGfxSettings() || DirtyAdvGfxSettings())
						{
							SetBackedWithGraphicChanges(true);

							if(GetCurrentScreen() != MENU_UNIQUE_ID_SETTINGS_GRAPHICS || GetCurrentScreen() != MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
							{
								uiAssertf(0,"Attempted to tab left and right with dirty graphics settings while not on a graphics screen. Please delete your settings.xml file and if you still get this message open a bug for Default UI Code");
								TriggerSwitchPane(-1);
							}
						}

						if(!DirtyGfxSettings() && !DirtyAdvGfxSettings())
#endif // RSG_PC
						{
							TriggerSwitchPane(-1);
						}
					}
				}

				break;
			}

			case PAD_RIGHTSHOULDER1:
			{
				if( !AnyLayerHasFlag(Input_NoTabChange) && !GetCurrentMenuVersionHasFlag(kNoTabChange) )
				{
					if( sm_pMsgToWarnOnTabChange )
					{
						ShowTabChangeWarning(1, false);
					}
					else
					{
#if RSG_PC
						if(DirtyGfxSettings() || DirtyAdvGfxSettings())
						{
							SetBackedWithGraphicChanges(true);

							if(GetCurrentScreen() != MENU_UNIQUE_ID_SETTINGS_GRAPHICS || GetCurrentScreen() != MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
							{
								uiAssertf(0,"Attempted to tab left and right with dirty graphics settings while not on a graphics screen. Please delete your settings.xml file and if you still get this message open a bug for Default UI Code");
								TriggerSwitchPane(1);
							}
						}

						if(!DirtyGfxSettings() && !DirtyAdvGfxSettings())
#endif // RSG_PC
						{
							TriggerSwitchPane(1);
						}
					}
				}
				break;
			}

			case PAD_DPADLEFT:
			{
				if (sm_bNavigatingContent && !HandlePrefOverride(sm_iMenuPrefSelected, -1) )
				{
					GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_INPUT_EVENT", pressed );
				}
				break;
			}

			case PAD_DPADRIGHT:
			{
				if (sm_bNavigatingContent && !HandlePrefOverride(sm_iMenuPrefSelected, 1) )
				{
					GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_INPUT_EVENT", pressed );
				}
				break;
			}

			case PAD_TRIANGLE:
			{
				bool const c_isPlayCreditsSupported = SUIContexts::IsActive( CAN_PLAY_CREDITS );
				if( c_isPlayCreditsSupported )
				{
					// needs to be delayed as new layout info disabling CAN_PLAY_CREDITS comes from AS callback
					// as well as other input commands
					if (!sm_bDisplayCreditsScreenNextFrame)
					{
						sm_bDisplayCreditsScreenNextFrame = true;
					}
				}
				else
				{
                    if( GetCurrentMenuVersionHasFlag( kCanHide ) && !IsNavigatingContent() )
                    {
                        SetMaxPayneMode( !sm_bMaxPayneMode );
                    }
                    else
                    {
                        GetMovieWrapper( PAUSE_MENU_MOVIE_CONTENT ).CallMethod( "SET_INPUT_EVENT", pressed );
                    }
				}
				
				break;
			}

			default:
			{
				if(bScrollPressedOnPartyList)
				{
					CScaleformMovieWrapper &statMovie = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
					if(statMovie.BeginMethod("SET_COLUMN_INPUT_EVENT"))
					{
						statMovie.AddParam(PM_COLUMN_MIDDLE); // ColumnID
						statMovie.AddParam(pressed); // inputID
						statMovie.EndMethod();
					}
				}
				else
				{
					GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_INPUT_EVENT", pressed );
				}
			}
		}
	}
}

#if KEYBOARD_MOUSE_SUPPORT
bool CPauseMenu::GetScrollPressed(s32 &pressed, bool bPlaySound)
{
	bool bPressed = false;
	if(sm_iMouseScrollDirection != SCROLL_CLICK_NONE)
	{
		bPressed = true;
		if(bPlaySound)
		{
			PlaySound("NAV_LEFT_RIGHT");
		}

		switch (sm_iMouseScrollDirection)
		{
		case SCROLL_CLICK_LEFT:
			pressed = PAD_DPADLEFT;
			break;
		case SCROLL_CLICK_RIGHT:
			pressed = PAD_DPADRIGHT;
			break;
		case SCROLL_CLICK_UP:
			pressed = PAD_DPADUP;
			break;
		case SCROLL_CLICK_DOWN:
			pressed = PAD_DPADDOWN;
			break;
		default:
			bPressed = false;
			break;
		}

		sm_iMouseScrollDirection = SCROLL_CLICK_NONE;
	}

	return bPressed;
}
#endif // KEYBOARD_MOUSE_SUPPORT

//	Graeme - Bug 814188 - If the player cancels the 360 sign-in or device selection then we return to the game menu
//	I call this function to move the highlight from the "Load Game" to the "New Game" option
void CPauseMenu::MoveOffLoadGameMenuOption()
{
	if (uiVerifyf(IsLoadGameOptionHighlighted(), "CPauseMenu::MoveOffLoadGameMenuOption - this function should only be called when the Load Game menu option is highlighted"))
	{
		GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_INPUT_EVENT", PAD_DPADDOWN );
	}
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CPauseMenu::MoveOffUploadSavegameMenuOption()
{
	if (uiVerifyf(IsUploadSavegameOptionHighlighted(), "CPauseMenu::MoveOffUploadSavegameMenuOption - this function should only be called when the Upload Savegame menu option is highlighted"))
	{
		GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_INPUT_EVENT", PAD_DPADUP );
	}
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

void CPauseMenu::MENU_SHIFT_DEPTH(eMenuceptionDir dir, bool bDontFallOff /* = false */, bool bSkipInputSpamCheck /* = false */)
{
	sm_bAwaitingMenuShiftDepthResponse = GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "MENU_SHIFT_DEPTH", int(dir), bDontFallOff, bSkipInputSpamCheck );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::TriggerStore
// PURPOSE: Trigger the store to open
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::TriggerStore(bool bFromMP /* = false */)
{
#if RSG_PC
	(void)bFromMP;
	TriggerStorePC();
	return;
#else
#if RSG_ORBIS
    if(!NetworkInterface::HasRawPlayStationPlusPrivileges() &&
		NetworkInterface::IsPlayStationPlusPromotionEnabled() &&
		NetworkInterface::IsPlayStationPlusCustomUpsellEnabled() &&
       !NetworkInterface::DoesTunableDisablePauseStoreRedirect())
    {
		uiDebugf1("TriggerStore :: Redirecting to PS+ Upsell");
        CLiveManager::ShowAccountUpgradeUI();
    } 
    else
#endif
	if (IsStoreAvailable() && sm_eClosingAction==CA_None)
	{
		uiDebugf1("TriggerStore :: Starting Commerce");
			
		SetClosingAction(bFromMP ? CA_StartCommerceMP : CA_StartCommerce);
		sm_bUnpauseGameOnMenuClose = bFromMP ? true : false;
		Close();  // fix for 1115653 - we must always call BEGIN_EXIT_PAUSEMENU so actionscript has a chance to clear stuff up
		char cHtmlFormattedBusyString[256];
		char* html = CTextConversion::TextToHtml(TheText.Get("STORE_LOADING"),cHtmlFormattedBusyString,sizeof(cHtmlFormattedBusyString));
		CBusySpinner::On( html, SPINNER_ICON_LOADING, SPINNER_SOURCE_STORE_LOADING );
		
		// bFromMP doesn't seem to be for in MP, but for a specific call in MP ...so use proper check
		if (!NetworkInterface::IsGameInProgress())
			CPauseMenu::TogglePauseRenderPhases(false, OWNER_STORE, __FUNCTION__ );
	}
	else
	{
		uiDebugf1("TriggerStore :: No action taken");
	}
#endif
}

bool CPauseMenu::IsStoreAvailable()
{
	const rlGamerInfo* localGamer = NetworkInterface::GetActiveGamerInfo();

	//Have to check if someone has stolen our container memory.
#if COMMERCE_CONTAINER
	if (CLiveManager::GetCommerceMgr().ContainerGetMode() == cCommerceManager::CONTAINER_MODE_GENERIC)
	{
		return false;
	}
#endif

    if (localGamer && localGamer->IsOnline() && CLiveManager::IsOnline() && CLiveManager::IsOnlineRos() && cStoreScreenMgr::IsStoreScreenEnabled() )
	{
		return true;
	}

	return false;
}

s32 CPauseMenu::ConvertPadButtonNumberToInputType(s32 sPadValue)
{
	s32 retVal = 0;
	switch(sPadValue)
	{
	case PAD_LEFTSHOULDER1:
		retVal = INPUT_FRONTEND_LB;
		break;
	case PAD_LEFTSHOULDER2:
		retVal = INPUT_FRONTEND_LT;
		break;
	case PAD_RIGHTSHOULDER1:
		retVal = INPUT_FRONTEND_RB;
		break;
	case PAD_RIGHTSHOULDER2:
		retVal = INPUT_FRONTEND_RT;
		break;

	case PAD_DPADUP:
		retVal = INPUT_FRONTEND_UP;
		break;
	case PAD_DPADDOWN:
		retVal = INPUT_FRONTEND_DOWN;
		break;
	case PAD_DPADLEFT:
		retVal = INPUT_FRONTEND_LEFT;
		break;
	case PAD_DPADRIGHT:
		retVal = INPUT_FRONTEND_RIGHT;
		break;

	case PAD_SQUARE:
		retVal = INPUT_FRONTEND_X;
		break;
	case PAD_TRIANGLE:
		retVal = INPUT_FRONTEND_Y;
		break;
	case PAD_CROSS:
		retVal = INPUT_FRONTEND_ACCEPT;
		break;
	case PAD_CIRCLE:
		retVal = INPUT_FRONTEND_CANCEL;
		break;

	case PAD_START:
		retVal = INPUT_FRONTEND_PAUSE;
		break;
	case PAD_SELECT:
		retVal = INPUT_FRONTEND_SELECT;
		break;

	default:
		break;
	}

	return retVal;
}


void CPauseMenu::InitGlobalFrontendSprites()
{
	strLocalIndex iTxdId = strLocalIndex(g_TxdStore.FindSlot(CHudTools::GetFrontendTXDPath()));

	if (iTxdId != -1)
	{
		g_TxdStore.StreamingRequest(iTxdId, (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE));
		CStreaming::SetDoNotDefrag(iTxdId, g_TxdStore.GetStreamingModuleId());

		CStreaming::LoadAllRequestedObjects(true);

		// This should not be a blocking load
		if (g_TxdStore.HasObjectLoaded(iTxdId))
		{
			g_TxdStore.AddRef(iTxdId, REF_OTHER);
			g_TxdStore.PushCurrentTxd();
			g_TxdStore.SetCurrentTxd(iTxdId);
			if (Verifyf(fwTxd::GetCurrent(), "Current Texture Dictionary (id %u) is NULL ",iTxdId.Get()))
			{
				sm_MenuSprite[MENU_SPRITE_BACKGROUND_GRADIENT].SetTexture("gradient_background");
				sm_MenuSprite[MENU_SPRITE_SPINNER].SetTexture("spinner");
				sm_MenuSprite[MENU_SPRITE_SAVING_SPINNER].SetTexture("NewSaving");
			}

			g_TxdStore.PopCurrentTxd();
		}
	}
}


void CPauseMenu::ShutdownGlobalFrontendSprites()
{
	if (sm_MenuSprite[MENU_SPRITE_BACKGROUND_GRADIENT].GetTexture())
	{
		sm_MenuSprite[MENU_SPRITE_BACKGROUND_GRADIENT].Delete();
	}

	if (sm_MenuSprite[MENU_SPRITE_SPINNER].GetTexture())
	{
		sm_MenuSprite[MENU_SPRITE_SPINNER].Delete();
	}

	if (sm_MenuSprite[MENU_SPRITE_SAVING_SPINNER].GetTexture())
	{
		sm_MenuSprite[MENU_SPRITE_SAVING_SPINNER].Delete();
	}

	strLocalIndex iTxdId = strLocalIndex(g_TxdStore.FindSlot(CHudTools::GetFrontendTXDPath()));

	if (iTxdId != -1)
	{
		g_TxdStore.RemoveRef(iTxdId, REF_OTHER);
		g_TxdStore.PopCurrentTxd();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RequestSprites()
// PURPOSE:	requests the sprite txd to stream
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RequestSprites()
{
	if (!sm_MenuSprite[MENU_SPRITE_ARROW].GetTexture())
	{
		strLocalIndex iTxdId = g_TxdStore.FindSlot(PAUSEMENU_TXD_PATH);

		if (iTxdId != -1)
		{
			g_TxdStore.StreamingRequest(iTxdId, (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE));
			CStreaming::SetDoNotDefrag(iTxdId, g_TxdStore.GetStreamingModuleId());
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetupSprites()
// PURPOSE:	checks if the sprites are loaded and then set them up if it has
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetupSprites()
{
	if (!sm_MenuSprite[MENU_SPRITE_ARROW].GetTexture())
	{
		strLocalIndex iTxdId = strLocalIndex(g_TxdStore.FindSlot(PAUSEMENU_TXD_PATH));

		if (iTxdId != -1)
		{
			// This should not be a blocking load
			if (g_TxdStore.HasObjectLoaded(iTxdId))
			{
				g_TxdStore.AddRef(iTxdId, REF_OTHER);
				g_TxdStore.PushCurrentTxd();
				g_TxdStore.SetCurrentTxd(iTxdId);
				if (Verifyf(fwTxd::GetCurrent(), "Current Texture Dictionary (id %u) is NULL ",iTxdId.Get()))
				{
					sm_MenuSprite[MENU_SPRITE_ARROW].SetTexture("arrow_left");
				}

				g_TxdStore.PopCurrentTxd();
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RemoveSprites()
// PURPOSE:	removes the sprites
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RemoveSprites()
{
	bool bHadBeenSetup = false;

	if (sm_MenuSprite[MENU_SPRITE_ARROW].GetTexture())
	{
		sm_MenuSprite[MENU_SPRITE_ARROW].Delete();
		bHadBeenSetup = true;
	}

	strLocalIndex iTxdId = strLocalIndex(g_TxdStore.FindSlot(PAUSEMENU_TXD_PATH));

	if (iTxdId != -1 && bHadBeenSetup)
	{
		g_TxdStore.RemoveRef(iTxdId, REF_OTHER);
	}
}
	


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RenderBackground()
// PURPOSE:	Renders the background gradiant image
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RenderBackground(const PauseMenuRenderDataExtra& renderData)
{
	if (!renderData.bRenderBackgroundThisFrame)
		return;

	if( renderData.bVersionHasNoBackground && !renderData.bRenderBackgroundThisFrame )
		return;
	
	if(!renderData.bCustomWarningActive)
	{
		if (sm_MenuSprite[MENU_SPRITE_BACKGROUND_GRADIENT].GetTexture() && sm_MenuSprite[MENU_SPRITE_BACKGROUND_GRADIENT].GetShader())
		{
			sm_MenuSprite[MENU_SPRITE_BACKGROUND_GRADIENT].SetRenderState();

			SGeneralMovieData& sConfig = sm_PersistentData.Gradient;

			const Vector2& vPos = sConfig.vPos;
			const Vector2& vSize = sConfig.vSize;

			Vector2 v[4], uv[4];
			v[0].Set(vPos.x,vPos.y+vSize.y);
			v[1].Set(vPos.x,vPos.y);
			v[2].Set(vPos.x+vSize.x,vPos.y+vSize.y);
			v[3].Set(vPos.x+vSize.x,vPos.y);

#define __UV_OFFSET (0.002f)
			uv[0].Set(__UV_OFFSET,1.0f-__UV_OFFSET);
			uv[1].Set(__UV_OFFSET,__UV_OFFSET);
			uv[2].Set(1.0f-__UV_OFFSET,1.0f-__UV_OFFSET);
			uv[3].Set(1.0f-__UV_OFFSET,__UV_OFFSET);

			sm_MenuSprite[MENU_SPRITE_BACKGROUND_GRADIENT].Draw(v[0], v[1], v[2], v[3], uv[0], uv[1], uv[2], uv[3], CRGBA(255,255,255,sm_PersistentData.Gradient.flatAlpha));
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RenderAnimatedSpinner()
// PURPOSE:	Renders the animated spinner for when we are streaming/initialising
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetBusySpinner( bool bIsBusy, s8 column, int iWhichSpinner )
{
	sm_RenderData.iLoadingSpinnerDesired[iWhichSpinner] = bIsBusy ? column : PauseMenuRenderData::NO_SPINNER;
}

void CPauseMenu::RenderAnimatedSpinner(float xPosOverride, bool bSetStates /* = false */, bool bCheckFrame /* = true */)
{
	const CSpinnerData& rData = sm_PersistentData.Spinner;
	const Vector2& vSize = rData.vSize;
	Vector2 vPos = rData.vPos;

	if( xPosOverride != -1.0f )
		vPos.x = xPosOverride;
	RenderAnimatedSpinner(vSize, vPos, bSetStates, bCheckFrame);
}

void CPauseMenu::RenderAnimatedSpinner(Vector2 vPos, bool bSetStates /* = false */, bool bCheckFrame /* = true */)
{
	const CSpinnerData& rData = sm_PersistentData.Spinner;
	const Vector2& vSize = rData.vSize;
	RenderAnimatedSpinner(vSize, vPos, bSetStates, bCheckFrame);
}

void CPauseMenu::RenderAnimatedSpinner(Vector2 vSize, Vector2 vPos, bool bSetStates /* = false */, bool bCheckFrame /* = true */, bool bRenderSavingSpinner /* = false */)
{
	u32 spinnerToUse = MENU_SPRITE_SPINNER;

	if (sm_MenuSprite[spinnerToUse].GetTexture() && sm_MenuSprite[spinnerToUse].GetShader())
	{
		if (bSetStates)
		{
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetBlendState(CHudTools::GetStandardSpriteBlendState());
		}
		sm_MenuSprite[spinnerToUse].SetRenderState();
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_SIZE_ONLY, NULL, &vSize);

		static float fRotation = 0.0f;
		static u32 iFrameCounter = 0;

		if ( (!bCheckFrame) || (iFrameCounter != fwTimer::GetSystemFrameCount()) )
		{
			if (sm_time.GetMsTime() > SPIN_TIME)
			{
				if (bRenderSavingSpinner)
				{
					fRotation += (10 * DtoR);
				}
				else
				{
					fRotation -= (10 * DtoR);
				}
				sm_time.Reset();
			}
		}

		iFrameCounter = fwTimer::GetSystemFrameCount();

		if (fRotation > (360 * DtoR))
			fRotation -= (360 * DtoR);

		if (fRotation < 0)
			fRotation += (360 * DtoR);

		float cos_float = rage::Cosf(fRotation);
		float sin_float = rage::Sinf(fRotation);

		float WidthCos = vSize.x * cos_float;
		float WidthSin = vSize.x * sin_float;
		float HeightCos = vSize.y * cos_float;
		float HeightSin = vSize.y * sin_float;

		float fMin = 0.005f;
		float fMax = 1.0f - fMin;

		Vector2 v[4];
		Vector2 tc[4];

		tc[0].Set(fMin,fMax);
		tc[1].Set(fMin,fMin);
		tc[2].Set(fMax,fMax);
		tc[3].Set(fMax,fMin);

		v[0].Set(vPos.x - WidthCos - WidthSin, vPos.y - HeightSin + HeightCos);
		v[1].Set(vPos.x - WidthCos + WidthSin, vPos.y - HeightSin - HeightCos);
		v[2].Set(vPos.x + WidthCos - WidthSin, vPos.y + HeightSin + HeightCos);
		v[3].Set(vPos.x + WidthCos + WidthSin, vPos.y + HeightSin - HeightCos);

		if (bRenderSavingSpinner)
		{
			sm_MenuSprite[spinnerToUse].DrawSwitch(true, false, 0.0f, v[0], v[1], v[2], v[3], tc[0], tc[1], tc[2], tc[3], Color32(0xFFFFFFFF));
		}
		else
		{
			sm_MenuSprite[spinnerToUse].DrawSwitch(true, false, 0.0f, v[0], v[1], v[2], v[3], tc[2], tc[3], tc[0], tc[1], Color32(0xFFFFFFFF));
		}

		if (bSetStates)
		{
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			CSprite2d::ClearRenderState();
			CShaderLib::SetGlobalEmissiveScale(1.0f);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::RenderGeneralOverlays()
// PURPOSE:	Renders general pause menu overlays
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RenderGeneralOverlays(const PauseMenuRenderDataExtra& renderData)
{
	PF_AUTO_PUSH_TIMEBAR("General Overlays");
	// if nothing to draw, bail
	if( !(renderData.bArrowHeadersShow || renderData.bArrowContentLeftVisible || renderData.bArrowContentRightVisible) )
		return;

	// no sprite? bail
	CSprite2d& arrowSprite = sm_MenuSprite[MENU_SPRITE_ARROW];
	if(  !arrowSprite.GetTexture() || !arrowSprite.GetShader())
		return;

	arrowSprite.SetRenderState();
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

	CRGBA ArrowColor = CHudColour::GetRGBA(HUD_COLOUR_WHITE);  // fix for 1369862 (arrow colour)
	const Vector2& vHeaderSize = sm_PersistentData.Arrows.vSize;
	const Vector2 vContentSize(0.045f, 0.08f);

	Vector2 v[4];
	Vector2 uvs[4];
	uvs[0].Set(0.0f, 1.0f);
	uvs[1].Set(0.0f, 0.0f);
	uvs[2].Set(1.0f, 1.0f);
	uvs[3].Set(1.0f, 0.0f);

	// we want both header arrows drawn, though at diminished alpha if false (or not at all if hiddenAlpha is 0)
	if(renderData.bArrowHeaderLeftVisible || (renderData.bArrowHeadersShow && sm_PersistentData.Arrows.hiddenAlpha > 0))
	{
		bool bArrowFullAlpha = renderData.bArrowHeaderLeftVisible;
#if RSG_PC
		// on PC, if you're using the mouse, always highlight the arrows because you can always do them
		if(renderData.bArrowHeadersShow && CMousePointer::HasMouseInputOccurred())
			bArrowFullAlpha = true;
#endif
		u8 alpha(bArrowFullAlpha?sm_PersistentData.Arrows.shownAlpha : sm_PersistentData.Arrows.hiddenAlpha);
		alpha = u8(Clamp( int(alpha*renderData.fAlphaFader), 0, 255));
		CRGBA LocalArrow( ArrowColor );
		LocalArrow.SetAlpha(alpha);

		Vector2 vPos(sm_PersistentData.Arrows.vPos);

		Vector2 vSize( vHeaderSize );
		CScaleformMgr::ScalePosAndSize(vPos, vSize, renderData.fSizeScalar);

		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_CENTRE, &vPos, &vSize);

		v[0].Set(vPos.x,vPos.y+vSize.y);
		v[1].Set(vPos.x,vPos.y);
		v[2].Set(vPos.x+vSize.x,vPos.y+vSize.y);
		v[3].Set(vPos.x+vSize.x,vPos.y);

		MULTI_MONITOR_ONLY( arrowSprite.MoveToScreenGUI(v) );
		arrowSprite.Draw(v[0], v[1], v[2], v[3], uvs[0], uvs[1],uvs[2],uvs[3], LocalArrow);
	}

	if(renderData.bArrowHeaderRightVisible || (renderData.bArrowHeadersShow && sm_PersistentData.Arrows.hiddenAlpha > 0))
	{
		bool bArrowFullAlpha = renderData.bArrowHeaderRightVisible;
#if RSG_PC
		// on PC, if you're using the mouse, always highlight the arrows because you can always do them
		if(renderData.bArrowHeadersShow && CMousePointer::HasMouseInputOccurred())
			bArrowFullAlpha = true;
#endif
		u8 alpha(bArrowFullAlpha?sm_PersistentData.Arrows.shownAlpha : sm_PersistentData.Arrows.hiddenAlpha);
		alpha = u8(Clamp( int(alpha*renderData.fAlphaFader), 0, 255));
		CRGBA LocalArrow( ArrowColor );
		LocalArrow.SetAlpha(alpha);

		Vector2 vPos(sm_PersistentData.Arrows.vPosRight);

		Vector2 vSize( vHeaderSize );
		CScaleformMgr::ScalePosAndSize(vPos, vSize, renderData.fSizeScalar);
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_CENTRE, &vPos, &vSize);

		v[0].Set(vPos.x,vPos.y+vSize.y);
		v[1].Set(vPos.x,vPos.y);
		v[2].Set(vPos.x+vSize.x,vPos.y+vSize.y);
		v[3].Set(vPos.x+vSize.x,vPos.y);

		MULTI_MONITOR_ONLY( arrowSprite.MoveToScreenGUI(v) );
												// UVs flipped
		arrowSprite.Draw(v[0], v[1], v[2], v[3], uvs[2], uvs[3],uvs[0], uvs[1], LocalArrow);
	}

	if (renderData.bArrowContentLeftVisible )
	{
		Vector2 vPos(Selectf( renderData.vArrowContentLeftPosition.x, renderData.vArrowContentLeftPosition.x, 0.11f), 
					 Selectf( renderData.vArrowContentLeftPosition.y, renderData.vArrowContentLeftPosition.y, 0.5f));
	
		v[0].Set(vPos.x,vPos.y+vContentSize.y);
		v[1].Set(vPos.x,vPos.y);
		v[2].Set(vPos.x+vContentSize.x,vPos.y+vContentSize.y);
		v[3].Set(vPos.x+vContentSize.x,vPos.y);

 		MULTI_MONITOR_ONLY( arrowSprite.MoveToScreenGUI(v) );
		arrowSprite.Draw(v[0], v[1], v[2], v[3], uvs[0], uvs[1],uvs[2], uvs[3], ArrowColor);
	}

	if (renderData.bArrowContentRightVisible )
	{
		Vector2 vPos(Selectf( renderData.vArrowContentRightPosition.x, renderData.vArrowContentRightPosition.x, 0.845f),
					 Selectf( renderData.vArrowContentRightPosition.y, renderData.vArrowContentRightPosition.y, 0.5f));
		v[0].Set(vPos.x,vPos.y+vContentSize.y);
		v[1].Set(vPos.x,vPos.y);
		v[2].Set(vPos.x+vContentSize.x,vPos.y+vContentSize.y);
		v[3].Set(vPos.x+vContentSize.x,vPos.y);

		MULTI_MONITOR_ONLY( arrowSprite.MoveToScreenGUI(v) );
													// UVs flipped
		arrowSprite.Draw(v[0], v[1], v[2], v[3], uvs[2], uvs[3],uvs[0], uvs[1], ArrowColor);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsInSaveGameOrLoadGameMenus()
// PURPOSE:	returns whether we are in the quick save menu or load game menu
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsInSaveGameOrLoadGameMenus()
{
	bool bIsInSaveOrLoad = false;
	if (IsInSaveGameMenus())
	{
		bIsInSaveOrLoad = true;
	}
	else
	{
		MenuScreenId currentPanel = GetCurrentActivePanel();
		if (currentPanel == MENU_UNIQUE_ID_LOAD_GAME ||
			currentPanel == MENU_UNIQUE_ID_SAVE_GAME_LIST)
		{
			bIsInSaveOrLoad = true;
		}
	}

	return sm_bActive && bIsInSaveOrLoad;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsInOnlineScreen()
// PURPOSE:	returns whether we are on the "online" screen or not
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsInOnlineScreen()
{
	if (sm_bActive && GetCurrentScreen() == MENU_UNIQUE_ID_MISSION_CREATOR) // (this is actually the online menu!)
	{
		return true;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsInMapScreen()
// PURPOSE:	returns whether we are on the map screen or not
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsInMapScreen()
{
	if (sm_bActive && GetCurrentScreen() == MENU_UNIQUE_ID_MAP)
	{
		return true;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsInGalleryScreen()
// PURPOSE:	returns whether we are on the gallery screen or not
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsInGalleryScreen()
{
	if (sm_bActive && GetCurrentScreen() == MENU_UNIQUE_ID_GALLERY)
	{
		return true;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsInAudioScreen()
// PURPOSE:	returns whether we are on the audio screen or not
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsInAudioScreen()
{
	if (sm_bActive && GetCurrentScreen() == MENU_UNIQUE_ID_SETTINGS)
	{
		return true;
	} 

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::AllowRumble()
// PURPOSE:	Allows or disallows rumble during the pause menu.
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::AllowRumble(bool bAllowRumble)
{
	sm_bAllowRumble = bAllowRumble;
}

bool CPauseMenu::IsSP()
{
	return SUIContexts::IsActive(UIATSTRINGHASH("InSP",0xe13b14b9));
}

bool CPauseMenu::IsMP()
{
	return SUIContexts::IsActive(UIATSTRINGHASH("InMP",0x16754f5));
}

bool CPauseMenu::IsLobby()
{
	return SUIContexts::IsActive(UIATSTRINGHASH("InLobby",0x1002e537));
}

bool CPauseMenu::IsLoadGameOptionHighlighted()
{
	if (sm_bActive && GetCurrentActivePanel() == MENU_UNIQUE_ID_LOAD_GAME)
	{
		return true;
	}

	return false;
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CPauseMenu::IsUploadSavegameOptionHighlighted()
{
	if (sm_bActive && GetCurrentActivePanel() == MENU_UNIQUE_ID_PROCESS_SAVEGAME)
	{
		return true;
	}

	return false;
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

bool CPauseMenu::IsInLoadGamePanel()
{
	if (sm_bActive && GetCurrentActivePanel() == MENU_UNIQUE_ID_SAVE_GAME_LIST)		//	It seems like this ID is also used for the list of savegames in the Save menu
	{
		return true;
	}

	return false;
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CPauseMenu::IsInUploadSavegamePanel()
{
	if (sm_bActive && GetCurrentActivePanel() == MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST)
	{
		return true;
	}

	return false;
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


//	If an error occurs when either the Load Game menu option is highlighted, or the highlight is in the list of savegames to load then back out of them
//	An error can occur on 360 if the player chooses not to select a storage device from the system UI, or chooses not to sign in. If either of them occur then start a new manual load so that the player
//	doesn't have to navigate away from Load Game and back again
void CPauseMenu::BackOutOfLoadGamePanes()
{
	if (IsLoadGameOptionHighlighted() || IsInLoadGamePanel())
	{
		UnlockMenuControl();

		s8 ColumnForSpinner = GetColumnForSpinner();
		SetBusySpinner(false, ColumnForSpinner);

		if (IsInLoadGamePanel())
		{	//	Back out from the right-hand panel containing the list of savegames
			bool bAllowClose = GetCurrentMenuVersionHasFlag(kAllowBackingOut);
			MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER, !bAllowClose);
		}

#if RSG_DURANGO
		if (CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
		{	//	If the player manages to open the load menu in the 5 seconds between signing out of the active profile and the session restarting,
			//	they may attempt to load a savegame for a different profile.
			//	If they do sign in to a different profile, CSavegameInitialChecks::InitialChecks() will return an error.
			//	Catch that situation here and close the pause menu.
			Close();
		}
		else
#endif
		{
			if (CSavegameInitialChecks::GetPlayerChoseNotToSignIn() || CSavegameInitialChecks::GetPlayerChoseNotToSelectADevice())
			{
				PopulateLoadGameMenu(true);
			}
			else
			{	//	If some other error occurred then don't launch another manual load
				if (IsLoadGameOptionHighlighted())
				{	//	Move down from Load Game to New Game
					MoveOffLoadGameMenuOption();
				}
			}
		}
	}
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CPauseMenu::BackOutOfUploadSavegamePanes()
{
	if (IsUploadSavegameOptionHighlighted() || IsInUploadSavegamePanel())
	{
		UnlockMenuControl();

		s8 ColumnForSpinner = GetColumnForSpinner();
		SetBusySpinner(false, ColumnForSpinner);

		if (IsInUploadSavegamePanel())
		{	//	Back out from the right-hand panel containing the list of savegames
			bool bAllowClose = GetCurrentMenuVersionHasFlag(kAllowBackingOut);
			MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER, !bAllowClose);
		}

#if RSG_DURANGO
		if (CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
		{	//	If the player manages to open the Upload menu in the 5 seconds between signing out of the active profile and the session restarting,
			//	they may attempt to Upload a savegame for a different profile.
			//	If they do sign in to a different profile, CSavegameInitialChecks::InitialChecks() will return an error.
			//	Catch that situation here and close the pause menu.
			Close();
		}
		else
#endif
		{
			if (CSavegameInitialChecks::GetPlayerChoseNotToSignIn() || CSavegameInitialChecks::GetPlayerChoseNotToSelectADevice())
			{
				PopulateUploadSavegameMenu(true);
			}
			else
			{	//	If some other error occurred then don't launch another Upload savegame operation
				if (IsUploadSavegameOptionHighlighted())
				{	//	Move up from Upload Savegame to New Game
					MoveOffUploadSavegameMenuOption();
				}
			}
		}
	}
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

void CPauseMenu::GetLanguageSettings(u32& sysLanguage, u32& language)
{
	// work out what language we using
	sysLanguage = GetLanguageFromSystemLanguage();

	// default to english
	if(sysLanguage == LANGUAGE_UNDEFINED)
		sysLanguage = LANGUAGE_ENGLISH;

	language = sysLanguage;
	if(CProfileSettings::GetInstance().Exists(CProfileSettings::DISPLAY_LANGUAGE)) 
		language = CProfileSettings::GetInstance().GetInt(CProfileSettings::DISPLAY_LANGUAGE);

	const char* langString;
	if(PARAM_uilanguage.Get(langString))
	{
		language = TheText.GetLanguageFromName(langString);
		if(!Verifyf(language != LANGUAGE_UNDEFINED, "%s is not a valid language, using system language", langString))
			language = sysLanguage;
	}
}

//PURPOSE: initialise language preferences. This needs to be done earlier in the initialisation process than everything else
void CPauseMenu::InitLanguagePreferences()
{
	u32 sysLanguage, language; 
	GetLanguageSettings(sysLanguage, language);

	SetMenuPreference(PREF_PREVIOUS_LANGUAGE, language);
	SetMenuPreference(PREF_SYSTEM_LANGUAGE, sysLanguage);
	SetMenuPreference(PREF_CURRENT_LANGUAGE, language);

	sm_languagePreferenceSet = true; 
}

u32 CPauseMenu::GetLanguagePreference()
{
	// this isn't great - some network systems are initialised earlier in the core setup and pull
	// the language setting before the pause menu has initialised the value (which means it's always read
	// as LANGUAGE_ENGLISH). Instead of switching all of these systems to poll the pause menu before 
	// performing language centric operations, this function can be used to pull the correct language value
	// even if the pause menu has not been setup yet

	// if we've already set the language, return the preference directly
	if(sm_languagePreferenceSet)
	{
		return GetMenuPreference(PREF_CURRENT_LANGUAGE);
	}

	// otherwise, same as above
	u32 sysLanguage, language; 
	GetLanguageSettings(sysLanguage, language);

	return language; 
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	SetDefaultValues
// PURPOSE:	set the starting values of the game preferences
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetDefaultValues( bool WIN32PC_ONLY( bDefaultGraphics ) )
{
	bool bSafeZoneChanged = false;

	for (s32 i = 0; i < MAX_MENU_PREFERENCES; i++)
	{
		SetMenuPreference(i, 0);
	}

	SetMenuPreference(PREF_CINEMATIC_SHOOTING, FALSE);
	SetMenuPreference(PREF_AUTOSAVE, TRUE);
	SetMenuPreference(PREF_DISPLAY_HUD, TRUE);
	SetMenuPreference(PREF_SKFX, TRUE);
	SetMenuPreference(PREF_RETICULE, 0);
	SetMenuPreference(PREF_RETICULE_SIZE, 0);
	SetMenuPreference(PREF_DISPLAY_GPS, TRUE);
	SetMenuPreference(PREF_SNIPER_ZOOM, TRUE);

#if RSG_ORBIS
	ResetOrbisSafeZone();
#endif
#if __PS3
	bSafeZoneChanged = SetMenuPreference(PREF_SAFEZONE_SIZE, 0);	// 85%
#else
	if(sysAppContent::IsJapaneseBuild())
		bSafeZoneChanged = SetMenuPreference(PREF_SAFEZONE_SIZE, 0);	// 85%
	else
		bSafeZoneChanged = SetMenuPreference(PREF_SAFEZONE_SIZE, WIN32PC_SWITCH(7, 5));	// 90% on PC 95 otherwise
#endif
	SetMenuPreference(PREF_RADAR_MODE, 1);
	SetMenuPreference(PREF_RADIO_STATION, 0);
	SetMenuPreference(PREF_CONTROL_CONFIG, 0);  // STANDARD as default
	SetMenuPreference(PREF_CONTROL_CONFIG_FPS, 0);  // STANDARD as default
	SetMenuPreference(PREF_ALTERNATE_HANDBRAKE, FALSE);
	SetMenuPreference(PREF_ALTERNATE_DRIVEBY, TRUE);
	SetMenuPreference(PREF_INVERT_LOOK, 0);
	SetMenuPreference(PREF_TARGET_CONFIG, DEFAULT_TARGETING_MODE);  // 0= Easy (Soft Lock On, Reticule Slow Down, Wider Lock On Angle), 1= Normal (Soft Lock On, Reticule Slow Down), 2=Expert Lite (Free aim + assist) 3=Expert (Free Aim Only)
	SetMenuPreference(PREF_CONTROLLER_SENSITIVITY, 5);
	SetMenuPreference(PREF_LOOK_AROUND_SENSITIVITY, 5);
#if KEYBOARD_MOUSE_SUPPORT
	SetMenuPreference(PREF_INVERT_MOUSE, 0);
	SetMenuPreference(PREF_INVERT_MOUSE_FLYING, 0);
	SetMenuPreference(PREF_SWAP_ROLL_YAW_MOUSE_FLYING, 0);
	SetMenuPreference(PREF_INVERT_MOUSE_SUB, 0);
	SetMenuPreference(PREF_MOUSE_AUTOCENTER_BIKE, 10);
	SetMenuPreference(PREF_MOUSE_AUTOCENTER_CAR, 10);
	SetMenuPreference(PREF_MOUSE_AUTOCENTER_PLANE, 10);

#endif // KEYBOARD_MOUSE_SUPPORT

#if LIGHT_EFFECTS_SUPPORT
	SetMenuPreference(PREF_CONTROLLER_LIGHT_EFFECT, 1);
#endif // LIGHT_EFFECTS_SUPPORT

	SetMenuPreference(PREF_MUSIC_VOLUME, 9);
	SetMenuPreference(PREF_SFX_VOLUME, 9);
	SetMenuPreference(PREF_DIAG_BOOST, 0);
	SetMenuPreference(PREF_SS_FRONT, 1);
	SetMenuPreference(PREF_SS_REAR, 1);
	SetMenuPreference(PREF_GAMMA, 18);
	SetMenuPreference(PREF_CAMERA_HEIGHT, 0);

	SetMenuPreference(PREF_FPS_PERSISTANT_VIEW, FALSE);
	SetMenuPreference(PREF_FPS_FIELD_OF_VIEW, 5);
	SetMenuPreference(PREF_FPS_LOOK_SENSITIVITY, 5);
	SetMenuPreference(PREF_FPS_AIM_SENSITIVITY, 5);

	SetMenuPreference(PREF_FPS_AIM_DEADZONE, 14);
	SetMenuPreference(PREF_FPS_AIM_ACCELERATION, 7);
	SetMenuPreference(PREF_AIM_DEADZONE, 14);
	SetMenuPreference(PREF_AIM_ACCELERATION, 7);
	SetMenuPreference(PREF_FPS_AUTO_LEVEL, TRUE);
	
	SetMenuPreference(PREF_FPS_RAGDOLL, TRUE);
	SetMenuPreference(PREF_FPS_COMBATROLL, TRUE);
	SetMenuPreference(PREF_FPS_HEADBOB, TRUE);
	SetMenuPreference(PREF_FPS_THIRD_PERSON_COVER, FALSE);
	SetMenuPreference(PREF_HOOD_CAMERA, FALSE);
	SetMenuPreference(PREF_FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING, TRUE);

	SetMenuPreference(PREF_STARTUP_FLOW, 0);
	SetMenuPreference(PREF_LANDING_PAGE, TRUE);

	SetMenuPreference(PREF_FEED_PHONE, TRUE);
	SetMenuPreference(PREF_FEED_STATS, TRUE);
	SetMenuPreference(PREF_FEED_CREW, TRUE);
	SetMenuPreference(PREF_FEED_FRIENDS, TRUE);
	SetMenuPreference(PREF_FEED_SOCIAL, TRUE);
	SetMenuPreference(PREF_FEED_STORE, TRUE);
	SetMenuPreference(PREF_FEED_TOOLTIP, TRUE);
	SetMenuPreference(PREF_FEED_DELAY, 2);
	SetMenuPreference(PREF_FACEBOOK_UPDATES, FALSE);
	SetMenuPreference(PREF_MUSIC_VOLUME_IN_MP, 9);
	SetMenuPreference(PREF_GPS_SPEECH, 1); // 0=OFF, 1=

#if RSG_ORBIS
	SetMenuPreference(PREF_VOICE_SPEAKERS, TRUE);
	SetMenuPreference(PREF_CTRL_SPEAKER, 1);
	SetMenuPreference(PREF_CTRL_SPEAKER_HEADPHONE, 0);
	SetMenuPreference(PREF_CTRL_SPEAKER_VOL, 5);
	SetMenuPreference(PREF_PULSE_HEADSET, FALSE);
#endif // RSG_ORBIS

	// default to low dynamic range
	SetMenuPreference(PREF_HDR, 0);

	// Stereo as default
#if RSG_PC
	const int iStereoAudio = 3;
#else
	const int iStereoAudio = 1;
#endif
	SetMenuPreference(PREF_SPEAKER_OUTPUT, iStereoAudio);  
	
	SetMenuPreference(PREF_INTERACTIVE_MUSIC, TRUE);  
#if __PPU
	// default to no fade on PS3
	SetMenuPreference(PREF_VOICE_OUTPUT, 1);
	SetMenuPreference(PREF_VOICE_SPEAKERS, FALSE);
#endif // __PPU

#if RSG_CPU_X64
	// Default to no fade on NG platforms
	SetMenuPreference(PREF_VOICE_OUTPUT, 1);
#endif

#if RSG_PC
	SetMenuPreference(PREF_UR_AUTOSCAN, 1);
	SetMenuPreference(PREF_UR_PLAYMODE, 0);
	SetMenuPreference(PREF_AUDIO_MUTE_ON_FOCUS_LOSS, 1);
#endif

	SetMenuPreference(PREF_MP_CAMERA_ZOOM_ON_FOOT, CZ_THIRD_PERSON_MEDIUM);
	SetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_VEHICLE, CZ_THIRD_PERSON_MEDIUM);
	SetMenuPreference(PREF_MP_CAMERA_ZOOM_ON_BIKE, CZ_THIRD_PERSON_MEDIUM);
	SetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_BOAT, CZ_THIRD_PERSON_MEDIUM);
	SetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_AIRCRAFT, CZ_THIRD_PERSON_MEDIUM);
	SetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_HELI, CZ_THIRD_PERSON_MEDIUM);

	InitLanguagePreferences();

	SetMenuPreference(PREF_MEASUREMENT_SYSTEM, GetMenuPreference(PREF_CURRENT_LANGUAGE) != LANGUAGE_ENGLISH);

	// if profile doesn't have subtitle settings then set value based on what language is set
	if(!CProfileSettings::GetInstance().Exists(CProfileSettings::DISPLAY_SUBTITLES))
	{
		// if the console system language is english, then we default subtitles to OFF
		// if console language is any other language then default them to ON
		if (GetMenuPreference(PREF_SYSTEM_LANGUAGE) == LANGUAGE_ENGLISH)
		{
			SetMenuPreference(PREF_SUBTITLES, FALSE);
		}
		else
		{
			SetMenuPreference(PREF_SUBTITLES, TRUE);
		}	
	}
	SetMenuPreference(PREF_DOF, TRUE);
	SetMenuPreference(PREF_BIG_RADAR, FALSE);
	SetMenuPreference(PREF_BIG_RADAR_NAMES, FALSE);
	SetMenuPreference(PREF_SHOW_TEXT_CHAT, TRUE);

#if RSG_PC
	if ( bDefaultGraphics )
	{
		SetMenuPreference(PREF_GFX_VID_OVERRIDE, FALSE);
		RestoreGraphicsDefaults(true);
		RestoreAdvancedGraphicsDefaults(true);

		// Voice Chat
		SetMenuPreference(PREF_VOICE_ENABLE, 1);
		SetMenuPreference(PREF_VOICE_OUTPUT_DEVICE, 0);
		SetMenuPreference(PREF_VOICE_OUTPUT_VOLUME, 10);
		SetMenuPreference(PREF_VOICE_TALK_ENABLED, 1);
		SetMenuPreference(PREF_VOICE_INPUT_DEVICE, 0);
		SetMenuPreference(PREF_VOICE_CHAT_MODE, VoiceChat::CAPTURE_MODE_PUSH_TO_TALK);
		SetMenuPreference(PREF_VOICE_MIC_VOLUME, 10);
		SetMenuPreference(PREF_VOICE_MIC_SENSITIVITY, 10);
		SetMenuPreference(PREF_VOICE_SOUND_VOLUME, 10);
		SetMenuPreference(PREF_VOICE_MUSIC_VOLUME, 10);
	}
#endif // RSG_PC

#if KEYBOARD_MOUSE_SUPPORT
	SetMenuPreference(PREF_MOUSE_SUB, 0);
	SetMenuPreference(PREF_MOUSE_DRIVE, 0);
	SetMenuPreference(PREF_MOUSE_FLY, 0);
	SetMenuPreference(PREF_MOUSE_ON_FOOT_SCALE, 50);
	SetMenuPreference(PREF_MOUSE_DRIVING_SCALE, 50);
	SetMenuPreference(PREF_MOUSE_PLANE_SCALE, 50);
	SetMenuPreference(PREF_MOUSE_HELI_SCALE, 50);
	SetMenuPreference(PREF_MOUSE_SUB_SCALE, 50);
	SetMenuPreference(PREF_MOUSE_WEIGHT_SCALE, 0);
	SetMenuPreference(PREF_MOUSE_ACCELERATION, FALSE);
	SetMenuPreference(PREF_KBM_TOGGLE_AIM, FALSE);
	SetMenuPreference(PREF_ALT_VEH_MOUSE_CONTROLS, FALSE);
	SetMenuPreference(PREF_FPS_DEFAULT_AIM_TYPE, 0);
#endif // KEYBOARD_MOUSE_SUPPORT

	SetMenuPreference(PREF_FPS_VEH_AUTO_CENTER, TRUE);

#if GTA_REPLAY
	SetMenuPreference(PREF_REPLAY_MODE, CONTINUOUS);
	SetMenuPreference(PREF_REPLAY_MEM_LIMIT, REPLAY_MEM_25GB);
	SetMenuPreference(PREF_REPLAY_AUTO_RESUME_RECORDING, REPLAY_AUTO_RESUME_RECORDING_OFF);
	SetMenuPreference(PREF_REPLAY_AUTO_SAVE_RECORDING, REPLAY_AUTO_SAVE_RECORDING_OFF);
	SetMenuPreference(PREF_VIDEO_UPLOAD_PRIVACY, VIDEO_UPLOAD_PRIVACY_PUBLIC);
	SetMenuPreference(PREF_ROCKSTAR_EDITOR_TOOLTIP, TRUE);
#if RSG_PC
	SetMenuPreference(PREF_ROCKSTAR_EDITOR_EXPORT_GRAPHICS_UPGRADE, TRUE);
#endif
#endif // GTA_REPLAY

#if __PPU
	int value = 1;
	// The PS3 Documentation state that the accept/cancel buttons must be swapped in certain regions. This does not get set by the settings but is read from the console.
	uiVerify(cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN, &value)ASSERT_ONLY(==CELL_OK));
	SetMenuPreference(PREF_ACCEPT_IS_CROSS, value);

	uiVerify(cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_PAD_RUMBLE, &value)ASSERT_ONLY(==CELL_OK));
	SetMenuPreference(PREF_VIBRATION, value);
#elif RSG_ORBIS
	int value = 1;
	if(sysAppContent::IsJapaneseBuild() 
#if __BANK // BANK/DEV builds enable button swap when language is set to japanese thru -uilanguage. Final builds use param.sfo
		|| GetMenuPreference(PREF_CURRENT_LANGUAGE) == LANGUAGE_JAPANESE || PARAM_enableJPNButtonSwap.Get()
#endif
		)
	{
		value = 0;
	}
	SetMenuPreference(PREF_ACCEPT_IS_CROSS, value);
	SetMenuPreference(PREF_VIBRATION, TRUE);
#else
	SetMenuPreference(PREF_VIBRATION, TRUE);
	SetMenuPreference(PREF_ACCEPT_IS_CROSS, 1);
#endif

	for (s32 i = 0; i < MAX_MENU_PREFERENCES; i++)
	{
		if (i != PREF_SAFEZONE_SIZE || bSafeZoneChanged)  // if not safezone pref, or if it is, only if safezone has changed
		{
			SetValueBasedOnPreference(i, UPDATE_PREFS_INIT);
		}
	}

}

#if RSG_ORBIS
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	ResetOrbisSafeZone
// PURPOSE:	gets the current safe zone from sceSystemServiceGetDisplaySafeAreaInfo()
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::ResetOrbisSafeZone()
{
	SceSystemServiceDisplaySafeAreaInfo info;
	s32 errorCode = sceSystemServiceGetDisplaySafeAreaInfo(&info);
	if (errorCode == SCE_OK)
	{
		sm_bOrbisSafeZoneInError =  false;
		sm_fOrbisSafeZone = info.ratio < 0.98f ? info.ratio : 0.98f;
		return;
	}

	if (!sm_bOrbisSafeZoneInError && errorCode == SCE_SYSTEM_SERVICE_ERROR_NEED_DISPLAY_SAFE_AREA_SETTINGS)
	{
		// shouldn't have to pause game as it should lose focus
		ShowDisplaySafeAreaSettings();
		sm_bOrbisSafeZoneInError = true;
		
		// get message back of SCE_SYSTEM_SERVICE_EVENT_DISPLAY_SAFE_AREA_UPDATE
		// on that it'll run this function again to set the safezone and reset anything that uses the safe zone
	}
	
	sm_fOrbisSafeZone = 0.9f; // default ...we can't use anything custom created by prefs for PS4
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	UpdateOrbisSafeZone
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::UpdateOrbisSafeZone()
{
	// hopefully temporary, as the callback for a safe area change
	// isn't working in the SDK yet
	// https://ps4.scedev.net/forums/thread/16426/ ...this is how others deal with it

	SceSystemServiceDisplaySafeAreaInfo info;
	s32 errorCode = sceSystemServiceGetDisplaySafeAreaInfo(&info);
	if (errorCode == SCE_OK)
	{
		float safeZoneRatio = info.ratio;
		if (safeZoneRatio > 0.98f)
			safeZoneRatio = 0.98f;

		if (sm_fOrbisSafeZone != safeZoneRatio)
		{
			g_SysService.FakeEvent(sysServiceEvent::SAFEAREA_UPDATED);
		}
	}

	// checking to see if the safe area set up screen is gone
	// if so, refresh everything
	if (sm_bOrbisSafeZoneInError)
	{
		if (!g_SysService.IsUiOverlaid())
		{
			g_SysService.FakeEvent(sysServiceEvent::SAFEAREA_UPDATED);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	InitOrbisSafeZoneDelegate
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::InitOrbisSafeZoneDelegate()
{
	sm_SafeZoneDelegate.Bind(&CPauseMenu::HandleSafeZoneChange);
	g_SysService.AddDelegate(&sm_SafeZoneDelegate);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	ShutdownOrbisSafeZoneDelegate
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::ShutdownOrbisSafeZoneDelegate()
{
	g_SysService.RemoveDelegate(&sm_SafeZoneDelegate);
	sm_SafeZoneDelegate.Reset();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	HandleSafeZoneChange
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::HandleSafeZoneChange( sysServiceEvent* evt )
{
	if(evt != NULL)
	{
		if(evt->GetType() == sysServiceEvent::SAFEAREA_UPDATED)
		{
			ResetOrbisSafeZone();

			CPauseMenu::UpdateDisplayConfig();
			CNewHud::UpdateHudDisplayConfig();
			CMiniMap::UpdateDisplayConfig();
			if (CGameStreamMgr::GetGameStream())
				CGameStreamMgr::GetGameStream()->SetDisplayConfigChanged();

#if DEBUG_PAD_INPUT
			CDebug::UpdateDebugPadMoviePositions();
#endif // #if DEBUG_PAD_INPUT
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	ShowDisplaySafeAreaSettings
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::ShowDisplaySafeAreaSettings()
{
	sceSystemServiceShowDisplaySafeAreaSettings();
}

#endif

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	GetOfflineReasonAsLocKey
// PURPOSE:	Retrieves the reason online capabilities are unavailable (Orbis only at the moment)
/////////////////////////////////////////////////////////////////////////////////////
const char* CPauseMenu::GetOfflineReasonAsLocKey()
{
	const char* reasonAsLocKey = "NOT_CONNECTED";

	if(uiVerifyf(!CLiveManager::IsOnline(), "CPauseMenu::GetOfflineReasonAsLocKey - Attempting to request offline reason, but we're online!"))
	{
#if RSG_ORBIS
		const rlNpAuth::NpUnavailabilityReason reason = g_rlNp.GetNpAuth().GetNpUnavailabilityReason(NetworkInterface::GetLocalGamerIndex());

		switch(reason)
		{
			case rlNpAuth::NP_REASON_SYSTEM_UPDATE:
				reasonAsLocKey = "HUD_SYS_UPD_RQ";
				break;
			case rlNpAuth::NP_REASON_GAME_UPDATE:
				reasonAsLocKey = "HUD_GAME_UPD_RQ";
				break;
			case rlNpAuth::NP_REASON_AGE:
				reasonAsLocKey = "HUD_PROFILECHNG";
				break;
			case rlNpAuth::NP_REASON_CONNECTION:
				reasonAsLocKey = "HUD_DISCON";
				break;
			case rlNpAuth::NP_REASON_SIGNED_OUT:
			default:
				break;
		}
#endif
	}

	return reasonAsLocKey;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	RestoreControlDefaults
// PURPOSE:	set the starting values of the control preferences
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RestoreControlDefaults()
{
	SetItemPref(PREF_CONTROL_CONFIG, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_CINEMATIC_SHOOTING, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_ALTERNATE_HANDBRAKE, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_ALTERNATE_DRIVEBY, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_SNIPER_ZOOM, TRUE, UPDATE_PREFS_DEFAULTING);

	SetItemPref(PREF_AIM_DEADZONE, 14, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_AIM_ACCELERATION, 7, UPDATE_PREFS_DEFAULTING);

	if(CPauseMenu::IsSP())
		SetItemPref(PREF_TARGET_CONFIG, DEFAULT_TARGETING_MODE, UPDATE_PREFS_DEFAULTING);

#if __PPU
	int value = 1;
	
	uiVerify(cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_PAD_RUMBLE, &value)ASSERT_ONLY(==CELL_OK));
	SetItemPref(PREF_VIBRATION, value, UPDATE_PREFS_DEFAULTING);
#else
	SetItemPref(PREF_VIBRATION, TRUE, UPDATE_PREFS_DEFAULTING);
#endif
	SetItemPref(PREF_INVERT_LOOK, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_CONTROLLER_SENSITIVITY, 5, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_LOOK_AROUND_SENSITIVITY, 5, UPDATE_PREFS_DEFAULTING);

#if LIGHT_EFFECTS_SUPPORT
	// We only want to update this on PS4 as this is the menu it is on in the PS4 version. The PC has it in a different menu and we don't want
	// resetting this menu to change the setting there.
	ORBIS_ONLY(SetItemPref(PREF_CONTROLLER_LIGHT_EFFECT, 1, UPDATE_PREFS_DEFAULTING));
#endif // LIGHT_EFFECTS_SUPPORT

	SetItemPref(PREF_CONTROL_CONFIG_FPS, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_LOOK_SENSITIVITY, 5, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_AIM_SENSITIVITY, 5, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_AIM_DEADZONE, 14, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_AIM_ACCELERATION, 7, UPDATE_PREFS_DEFAULTING);

	UpdateProfileFromMenuOptions();
}

#if KEYBOARD_MOUSE_SUPPORT
void CPauseMenu::RestoreMiscControlsDefaults()
{
	SetItemPref(PREF_MOUSE_TYPE, ioMouse::RAW_MOUSE, UPDATE_PREFS_DEFAULTING);
	
	SetItemPref(PREF_MOUSE_SUB, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_DRIVE, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_FLY, 0, UPDATE_PREFS_DEFAULTING);

	SetItemPref(PREF_INVERT_MOUSE, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_INVERT_MOUSE_FLYING, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_SWAP_ROLL_YAW_MOUSE_FLYING, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_INVERT_MOUSE_SUB, 0, UPDATE_PREFS_DEFAULTING);

	SetItemPref(PREF_MOUSE_AUTOCENTER_BIKE, 10, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_AUTOCENTER_CAR, 10, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_AUTOCENTER_PLANE, 10, UPDATE_PREFS_DEFAULTING);

	SetItemPref(PREF_MOUSE_ON_FOOT_SCALE, 50, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_DRIVING_SCALE, 50, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_PLANE_SCALE, 50, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_HELI_SCALE, 50, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_SUB_SCALE, 50, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_WEIGHT_SCALE, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MOUSE_ACCELERATION, FALSE, UPDATE_PREFS_DEFAULTING);

	SetItemPref(PREF_KBM_TOGGLE_AIM, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_DEFAULT_AIM_TYPE, 0, UPDATE_PREFS_DEFAULTING);

	// We only want to update this on PC as this is the menu it is on in the PC version. The PS4 has it in a different menu and we don't want
	// resetting this menu to change the setting there.
	WIN32PC_ONLY(SetItemPref(PREF_CONTROLLER_LIGHT_EFFECT, 1, UPDATE_PREFS_DEFAULTING));
	SetItemPref(PREF_ALT_VEH_MOUSE_CONTROLS, FALSE, UPDATE_PREFS_DEFAULTING);
}
#endif // KEYBOARD_MOUSE_SUPPORT

void CPauseMenu::RestoreCameraDefaults()
{
	SetItemPref(PREF_FPS_PERSISTANT_VIEW, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_FIELD_OF_VIEW, 5, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_AUTO_LEVEL, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_RAGDOLL, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_COMBATROLL, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_HEADBOB, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_THIRD_PERSON_COVER, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_CAMERA_HEIGHT, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_HOOD_CAMERA, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_VEH_AUTO_CENTER, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING, TRUE, UPDATE_PREFS_DEFAULTING);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	RestoreDisplayDefaults
// PURPOSE:	set the starting values of the display preferences
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RestoreDisplayDefaults()
{
	SetItemPref(PREF_RADAR_MODE, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_DISPLAY_HUD, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_SKFX, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_RETICULE, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_RETICULE_SIZE, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_DISPLAY_GPS, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_GPS_SPEECH, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_BIG_RADAR, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_BIG_RADAR_NAMES, FALSE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_SHOW_TEXT_CHAT, TRUE, UPDATE_PREFS_DEFAULTING);
	
	SetItemPref(PREF_MEASUREMENT_SYSTEM, GetMenuPreference(PREF_CURRENT_LANGUAGE) != LANGUAGE_ENGLISH, UPDATE_PREFS_DEFAULTING);

#if __PS3
	SetItemPref(PREF_SAFEZONE_SIZE, 0, UPDATE_PREFS_DEFAULTING);	// 85%
#else
	if(sysAppContent::IsJapaneseBuild())
		SetItemPref(PREF_SAFEZONE_SIZE, 0, UPDATE_PREFS_DEFAULTING);	// 85%
	else
		SetItemPref(PREF_SAFEZONE_SIZE, WIN32PC_SWITCH(7, 5), UPDATE_PREFS_DEFAULTING);	// 90%
#endif

	if (GetMenuPreference(PREF_CURRENT_LANGUAGE) == LANGUAGE_ENGLISH)
	{
		SetItemPref(PREF_SUBTITLES, FALSE, UPDATE_PREFS_DEFAULTING);
	}
	else
	{
		SetItemPref(PREF_SUBTITLES, TRUE, UPDATE_PREFS_DEFAULTING);
	}
	SetItemPref(PREF_DOF, TRUE, UPDATE_PREFS_DEFAULTING);

	UpdateProfileFromMenuOptions(false);
}

void CPauseMenu::RestoreReplayDefaults()
{
#if GTA_REPLAY
	SetItemPref(PREF_REPLAY_MODE, CONTINUOUS, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_REPLAY_MEM_LIMIT, REPLAY_MEM_25GB, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_REPLAY_AUTO_RESUME_RECORDING, REPLAY_AUTO_RESUME_RECORDING_OFF, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_REPLAY_AUTO_SAVE_RECORDING, REPLAY_AUTO_SAVE_RECORDING_OFF, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VIDEO_UPLOAD_PRIVACY, VIDEO_UPLOAD_PRIVACY_PUBLIC, UPDATE_PREFS_DEFAULTING);
#endif //GTA_REPLAY
}

void CPauseMenu::RestoreFirstPersonDefaults()
{
	UpdateProfileFromMenuOptions();
}

#if RSG_PC
void CPauseMenu::RestoreVoiceChatDefaults()
{
	SetItemPref(PREF_VOICE_ENABLE, 1, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_OUTPUT, 1, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_OUTPUT_DEVICE, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_OUTPUT_VOLUME, 10, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_TALK_ENABLED, 1, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_INPUT_DEVICE, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_CHAT_MODE, VoiceChat::CAPTURE_MODE_PUSH_TO_TALK, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_MIC_VOLUME, 10, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_MIC_SENSITIVITY, 10, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_SOUND_VOLUME, 10, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_MUSIC_VOLUME, 2, UPDATE_PREFS_DEFAULTING);
}

void CPauseMenu::HandleResolutionChange()
{
	const int INSTRUCTIONAL_BUTTONS = 0;
	const int HEADER = 1;
	const int SHARED_COMPONENTS = 2;
	const int MAIN_CONTENT = 3;

	int MovieIds[4];
	MovieIds[INSTRUCTIONAL_BUTTONS] = sm_iBaseMovieId[PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS].GetMovieID();
	MovieIds[HEADER] = sm_iBaseMovieId[PAUSE_MENU_MOVIE_HEADER].GetMovieID();
	MovieIds[SHARED_COMPONENTS] = sm_iBaseMovieId[PAUSE_MENU_MOVIE_SHARED_COMPONENTS].GetMovieID();
	MovieIds[MAIN_CONTENT] = sm_iBaseMovieId[PAUSE_MENU_MOVIE_CONTENT].GetMovieID();

	SGeneralPauseDataConfig kConfig;
	SGeneralPauseDataConfig* configs[4];
	configs[INSTRUCTIONAL_BUTTONS] = GetMenuArray().GeneralData.MovieSettings.Access(PAUSEMENU_FILENAME_INSTRUCTIONAL_BUTTONS);
	configs[HEADER] = GetMenuArray().GeneralData.MovieSettings.Access(PAUSEMENU_FILENAME_HEADER);
	configs[SHARED_COMPONENTS] = GetMenuArray().GeneralData.MovieSettings.Access(PAUSEMENU_FILENAME_SHARED_COMPONENTS);
	configs[MAIN_CONTENT] = GetMenuArray().GeneralData.MovieSettings.Access(PAUSEMENU_FILENAME_CONTENT);

	for(int i = 0; i <= MAIN_CONTENT; ++i)
	{
		if( !configs[i] )
			configs[i] = &kConfig;

		Vector2 vMoviePos ( configs[i]->vPos  );
		Vector2 vMovieSize( configs[i]->vSize );

		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(configs[i]->HAlign), &vMoviePos, &vMovieSize);
		CScaleformMgr::UpdateMovieParams(MovieIds[i], vMoviePos, vMovieSize);
	}

	UpdateDisplayConfig();
	sm_iBaseMovieId[PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS].CallMethod("initScreenLayout");

	RedrawInstructionalButtons();
}

int CPauseMenu::GetResolutionIndex()
{
	atArray<grcDisplayWindow> &resolutionList = CSettingsManager::GetInstance().GetResolutionList(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR));

	RECT rcWindowRect;
	GetWindowRect( g_hwndMain, &rcWindowRect );
	u32 iWindowWidth = rcWindowRect.right - rcWindowRect.left;
	u32 iWindowHeight = rcWindowRect.bottom - rcWindowRect.top;

	RECT rcClientRect;
	GetClientRect( g_hwndMain, &rcClientRect );
	u32 iClientWidth = rcClientRect.right - rcClientRect.left;
	u32 iClientHeight = rcClientRect.bottom - rcClientRect.top;

	for(int iResolutionPrefValue = 0; iResolutionPrefValue < resolutionList.size(); iResolutionPrefValue++)
	{
		if ((resolutionList[iResolutionPrefValue].uWidth == iWindowWidth || resolutionList[iResolutionPrefValue].uWidth == iClientWidth) &&
			(resolutionList[iResolutionPrefValue].uHeight == iWindowHeight || resolutionList[iResolutionPrefValue].uHeight == iClientHeight))
			return iResolutionPrefValue;
	}

	return 0;
}

void CPauseMenu::RestoreGraphicsDefaults(bool bInit)
{
	Settings gfxSettings = CSettingsManager::GetInstance().GetSafeModeSettings();

	if(bInit)
	{
		gfxSettings.m_graphics = CSettingsManager::GetInstance().GetUISettings().m_graphics;
		gfxSettings.m_video = CSettingsManager::GetInstance().GetUISettings().m_video;
	}
	else
	{
		gfxSettings.m_graphics = SettingsDefaults::GetInstance().GetDefaultGraphics();
		gfxSettings.m_video = SettingsDefaults::GetInstance().GetDefaultVideo();
	}

	const int convertFromMSAA[] = {0, 0, 1, 1, 2, 2, 2, 2, 3};
	int iMSAA = convertFromMSAA[gfxSettings.m_graphics.m_MSAA];
	int iReflectionMSAA = convertFromMSAA[gfxSettings.m_graphics.m_ReflectionMSAA];

	int iAnisFilter = gfxSettings.m_graphics.m_AnisotropicFiltering > 0 ? Log2Floor((u32)gfxSettings.m_graphics.m_AnisotropicFiltering) : 0;
	int iCityDensity = (int)((gfxSettings.m_graphics.m_CityDensity * 10));
	int iPopVariety =  (int)((gfxSettings.m_graphics.m_VehicleVarietyMultiplier * 10));
	int iLODScale = (int)((gfxSettings.m_graphics.m_LodScale * 10));
	int iMBStrength = (int)(gfxSettings.m_graphics.m_MotionBlurStrength * 10);

	int iResolutionPrefValue = CSettingsManager::GetInstance().GetResolutionIndex();

	int iConvergence = GetStereoConvergenceFrontendValue(gfxSettings.m_video.m_Convergence);

	atArray<grcDisplayWindow> refreshRateList;
	GetDisplayInfoList(refreshRateList);
	int iRefreshRatePrefValue = 0;

	for(int i = 0; i < refreshRateList.size(); ++i)
	{
		if( refreshRateList[i].uWidth == (u32)gfxSettings.m_video.m_ScreenWidth && 
			refreshRateList[i].uHeight == (u32)gfxSettings.m_video.m_ScreenHeight)
		{
			if(refreshRateList[i].uRefreshRate != (u32)gfxSettings.m_video.m_RefreshRate)
			{
				iRefreshRatePrefValue++;
			}
			else
			{
				break;
			}
		}
	}

	CProfileSettings& settings = CProfileSettings::GetInstance();

	if(bInit)
	{
		if(settings.Exists(CProfileSettings::PC_GFX_VID_OVERRIDE))
			SetMenuPreference(PREF_GFX_VID_OVERRIDE, (settings.GetInt(CProfileSettings::PC_GFX_VID_OVERRIDE)));

		SetMenuPreference(PREF_VID_SCREEN_TYPE, gfxSettings.m_video.m_Windowed);
		SetMenuPreference(PREF_VID_RESOLUTION, iResolutionPrefValue);
		SetMenuPreference(PREF_VID_REFRESH, iRefreshRatePrefValue);
		SetMenuPreference(PREF_VID_ADAPTER, gfxSettings.m_video.m_AdapterIndex);
		SetMenuPreference(PREF_VID_MONITOR, CSettingsManager::ConvertToMonitorIndex(gfxSettings.m_video.m_AdapterIndex, gfxSettings.m_video.m_OutputIndex));
		SetMenuPreference(PREF_VID_VSYNC, gfxSettings.m_video.m_VSync);
		SetMenuPreference(PREF_VID_ASPECT, gfxSettings.m_video.m_AspectRatio);
		
		SetMenuPreference(PREF_VID_STEREO, gfxSettings.m_video.m_Stereo);
		SetMenuPreference(PREF_VID_STEREO_CONVERGENCE, iConvergence);
		//SetMenuPreference(PREF_VID_STEREO_SEPARATION, (int)(gfxSettings.m_video.m_Separation / 10.0f));

		// Whilst this is not really a vid setting, it is in the vid setting screen so we should reset.
		SetMenuPreference(PREF_VID_PAUSE_ON_FOCUS_LOSS, gfxSettings.m_video.m_PauseOnFocusLoss);

		SetMenuPreference(PREF_GFX_DXVERSION, gfxSettings.m_graphics.m_DX_Version);
		SetMenuPreference(PREF_GFX_CITY_DENSITY, iCityDensity);
		SetMenuPreference(PREF_GFX_POP_VARIETY, iPopVariety);
		SetMenuPreference(PREF_GFX_TEXTURE_QUALITY, gfxSettings.m_graphics.m_TextureQuality);
		SetMenuPreference(PREF_GFX_PARTICLES_QUALITY, gfxSettings.m_graphics.m_ParticleQuality);
		SetMenuPreference(PREF_GFX_GRASS_QUALITY,gfxSettings.m_graphics.m_GrassQuality);
		SetMenuPreference(PREF_GFX_SHADER_QUALITY, gfxSettings.m_graphics.m_ShaderQuality);
		SetMenuPreference(PREF_GFX_SHADOW_QUALITY, gfxSettings.m_graphics.m_ShadowQuality - 1);
		SetMenuPreference(PREF_GFX_REFLECTION_QUALITY, gfxSettings.m_graphics.m_ReflectionQuality);
		SetMenuPreference(PREF_GFX_REFLECTION_MSAA, iReflectionMSAA);
		SetMenuPreference(PREF_GFX_WATER_QUALITY, gfxSettings.m_graphics.m_WaterQuality);
		SetMenuPreference(PREF_GFX_SHADOW_SOFTNESS, gfxSettings.m_graphics.m_Shadow_SoftShadows);
		SetMenuPreference(PREF_GFX_FXAA, gfxSettings.m_graphics.m_FXAA_Enabled);
		SetMenuPreference(PREF_GFX_AMBIENT_OCCLUSION, gfxSettings.m_graphics.m_SSAO);
		SetMenuPreference(PREF_GFX_TESSELLATION, gfxSettings.m_graphics.m_Tessellation);
		SetMenuPreference(PREF_GFX_MSAA, iMSAA);
		SetMenuPreference(PREF_GFX_TXAA, gfxSettings.m_graphics.m_TXAA_Enabled);
		SetMenuPreference(PREF_GFX_ANISOTROPIC_FILTERING, iAnisFilter);
		SetMenuPreference(PREF_GFX_POST_FX, gfxSettings.m_graphics.m_PostFX);
		SetMenuPreference(PREF_GFX_DOF, gfxSettings.m_graphics.m_DoF);
		SetMenuPreference(PREF_GFX_MB_STRENGTH, iMBStrength);
		SetMenuPreference(PREF_GFX_DIST_SCALE, iLODScale);
	}
	else
	{
		if(settings.Exists(CProfileSettings::PC_GFX_VID_OVERRIDE))
			SetMenuPreference(PREF_GFX_VID_OVERRIDE, (settings.GetInt(CProfileSettings::PC_GFX_VID_OVERRIDE)));

		SetItemPref(PREF_VID_SCREEN_TYPE, gfxSettings.m_video.m_Windowed, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_RESOLUTION, iResolutionPrefValue, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_REFRESH, iRefreshRatePrefValue, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_ADAPTER, gfxSettings.m_video.m_AdapterIndex, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_MONITOR, CSettingsManager::ConvertToMonitorIndex(gfxSettings.m_video.m_AdapterIndex, gfxSettings.m_video.m_OutputIndex), UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_VSYNC, gfxSettings.m_video.m_VSync, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_ASPECT, gfxSettings.m_video.m_AspectRatio, UPDATE_PREFS_DEFAULTING); // BACK-END TODO
		
		SetItemPref(PREF_VID_STEREO, gfxSettings.m_video.m_Stereo, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_STEREO_CONVERGENCE, iConvergence, UPDATE_PREFS_DEFAULTING);
		//SetItemPref(PREF_VID_STEREO_SEPARATION, (int)(gfxSettings.m_video.m_Separation / 10.0f), UPDATE_PREFS_DEFAULTING);

		// Whilst this is not really a vid setting, it is in the vid setting screen so we should reset.
		SetItemPref(PREF_VID_PAUSE_ON_FOCUS_LOSS, gfxSettings.m_video.m_PauseOnFocusLoss, UPDATE_PREFS_DEFAULTING);

		SetItemPref(PREF_GFX_DXVERSION, gfxSettings.m_graphics.m_DX_Version, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_CITY_DENSITY, iCityDensity, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_POP_VARIETY, iPopVariety, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_TEXTURE_QUALITY, gfxSettings.m_graphics.m_TextureQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_PARTICLES_QUALITY, gfxSettings.m_graphics.m_ParticleQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_GRASS_QUALITY, gfxSettings.m_graphics.m_GrassQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_SHADER_QUALITY, gfxSettings.m_graphics.m_ShaderQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_SHADOW_QUALITY, gfxSettings.m_graphics.m_ShadowQuality - 1, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_REFLECTION_QUALITY, gfxSettings.m_graphics.m_ReflectionQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_REFLECTION_MSAA, iReflectionMSAA, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_WATER_QUALITY, gfxSettings.m_graphics.m_WaterQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_SHADOW_SOFTNESS, gfxSettings.m_graphics.m_Shadow_SoftShadows, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_FXAA, gfxSettings.m_graphics.m_FXAA_Enabled, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_AMBIENT_OCCLUSION, gfxSettings.m_graphics.m_SSAO, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_TESSELLATION, gfxSettings.m_graphics.m_Tessellation, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_MSAA, iMSAA, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_TXAA, gfxSettings.m_graphics.m_TXAA_Enabled, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_ANISOTROPIC_FILTERING, iAnisFilter, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_POST_FX, gfxSettings.m_graphics.m_PostFX, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_DOF, gfxSettings.m_graphics.m_DoF, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_MB_STRENGTH, iMBStrength, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_DIST_SCALE, iLODScale, UPDATE_PREFS_DEFAULTING);
	}
}

void CPauseMenu::RestoreAdvancedGraphicsDefaults(bool bInit)
{
	Settings gfxSettings = CSettingsManager::GetInstance().GetSafeModeSettings();

	if(bInit)
		gfxSettings.m_graphics = CSettingsManager::GetInstance().GetUISettings().m_graphics;
	else
		gfxSettings.m_graphics = SettingsDefaults::GetInstance().GetDefaultGraphics();

	float fMaxLOD = gfxSettings.m_graphics.m_MaxLodScale;
	int iMaxLOD = (int)(fMaxLOD * 10.0f);

	float	fMaxShadowDist = Saturate(gfxSettings.m_graphics.m_Shadow_Distance - 1.0f);
	int		iMaxShadowDist =(int)(fMaxShadowDist * 10.0f);

	if(gfxSettings.m_graphics.m_ShadowQuality <= CSettings::eSettingsLevel::High)
	{
		iMaxShadowDist = 0;
	}

	if(bInit)
	{
		SetMenuPreference(PREF_ADV_GFX_LONG_SHADOWS, gfxSettings.m_graphics.m_Shadow_LongShadows);
		SetMenuPreference(PREF_ADV_GFX_ULTRA_SHADOWS, gfxSettings.m_graphics.m_UltraShadows_Enabled);
		SetMenuPreference(PREF_ADV_GFX_HD_FLIGHT, gfxSettings.m_graphics.m_HdStreamingInFlight);
		SetMenuPreference(PREF_ADV_GFX_MAX_LOD, iMaxLOD);
		SetMenuPreference(PREF_ADV_GFX_SHADOWS_DIST_MULT, iMaxShadowDist);
		SetMenuPreference(PREF_GFX_SCALING, gfxSettings.m_graphics.m_SamplingMode);

	}
	else
	{
		SetItemPref(PREF_ADV_GFX_LONG_SHADOWS, gfxSettings.m_graphics.m_Shadow_LongShadows, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_ADV_GFX_ULTRA_SHADOWS, gfxSettings.m_graphics.m_UltraShadows_Enabled, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_ADV_GFX_HD_FLIGHT, gfxSettings.m_graphics.m_HdStreamingInFlight, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_ADV_GFX_MAX_LOD, iMaxLOD, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_ADV_GFX_SHADOWS_DIST_MULT, iMaxShadowDist, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_SCALING, gfxSettings.m_graphics.m_SamplingMode, UPDATE_PREFS_DEFAULTING);
		GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX, true);
	}
}

void CPauseMenu::DeviceLost()
{

}

void CPauseMenu::DeviceReset()
{
	RestoreGraphicsDefaults(true);
	RestoreAdvancedGraphicsDefaults(true);

	if(IsCurrentScreenValid() && GetCurrentScreenData().HasDynamicMenu() )
	{
		if (GetCurrentActivePanel() == MENU_UNIQUE_ID_SETTINGS_GRAPHICS || GetCurrentActivePanel() == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
		{
			HandleResolutionChange();
			GenerateMenuData(GetCurrentActivePanel(), true);
		}
		else if( IsInGalleryScreen() || sMiniMapMenuComponent.IsActive() )
		{
			CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP, true);
		}
		else if( IsInMapScreen() )
		{
			CMiniMap::SetMinimapModeState(MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP, true);
		}

		GetCurrentScreenData().GetDynamicMenu()->DeviceReset();
	}

	UpdateDisplayConfig();
}

void CPauseMenu::RestorePreviousGraphicsValues(bool bApplyRevert, bool bInit)
{
	if(bApplyRevert)
		CSettingsManager::GetInstance().RevertToPreviousSettings();

	Settings gfxSettings = CSettingsManager::GetInstance().GetUISettings();

	const int convertFromMSAA[] = {0, 0, 1, 1, 2, 2, 2, 2, 3};
	int iMSAA = convertFromMSAA[gfxSettings.m_graphics.m_MSAA];
	int iReflectionMSAA = convertFromMSAA[gfxSettings.m_graphics.m_ReflectionMSAA];

	int iAnisFilter = gfxSettings.m_graphics.m_AnisotropicFiltering > 0 ? Log2Floor((u32)gfxSettings.m_graphics.m_AnisotropicFiltering) : 0;
	int iCityDensity = (int)((gfxSettings.m_graphics.m_CityDensity * 10));
	int iPopVariety =  (int)((gfxSettings.m_graphics.m_VehicleVarietyMultiplier * 10));
	int iLODScale = (int)((gfxSettings.m_graphics.m_LodScale * 10));
	int iMBStrength = (int)(gfxSettings.m_graphics.m_MotionBlurStrength * 10);

	int iResolutionPrefValue = CSettingsManager::GetInstance().GetResolutionIndex();

	int iConvergence = GetStereoConvergenceFrontendValue(gfxSettings.m_video.m_Convergence);

	atArray<grcDisplayWindow> refreshRateList;
	GetDisplayInfoList(refreshRateList);
	int iRefreshRatePrefValue = 0;

	for(int i = 0; i < refreshRateList.size(); ++i)
	{
		if( refreshRateList[i].uWidth == (u32)gfxSettings.m_video.m_ScreenWidth && 
			refreshRateList[i].uHeight == (u32)gfxSettings.m_video.m_ScreenHeight)
		{
			if(refreshRateList[i].uRefreshRate != (u32)gfxSettings.m_video.m_RefreshRate)
				iRefreshRatePrefValue++;
			else
				break;
		}
	}

	if(bInit)
	{
		SetMenuPreference(PREF_VID_SCREEN_TYPE, gfxSettings.m_video.m_Windowed);
		SetMenuPreference(PREF_VID_RESOLUTION, iResolutionPrefValue);
		SetMenuPreference(PREF_VID_REFRESH, iRefreshRatePrefValue);
		SetMenuPreference(PREF_VID_ADAPTER, gfxSettings.m_video.m_AdapterIndex);
		SetMenuPreference(PREF_VID_MONITOR, CSettingsManager::ConvertToMonitorIndex(gfxSettings.m_video.m_AdapterIndex, gfxSettings.m_video.m_OutputIndex));
		SetMenuPreference(PREF_VID_VSYNC, gfxSettings.m_video.m_VSync);
		SetMenuPreference(PREF_VID_ASPECT, gfxSettings.m_video.m_AspectRatio);
		
		SetMenuPreference(PREF_VID_STEREO, gfxSettings.m_video.m_Stereo);
		SetMenuPreference(PREF_VID_STEREO_CONVERGENCE, iConvergence);
		//SetMenuPreference(PREF_VID_STEREO_SEPARATION, (int)(gfxSettings.m_video.m_Separation / 10.0f));

		// Whilst this is not really a vid setting, it is in the vid setting screen so we should reset.
		SetMenuPreference(PREF_VID_PAUSE_ON_FOCUS_LOSS, gfxSettings.m_video.m_PauseOnFocusLoss);

		SetMenuPreference(PREF_GFX_DXVERSION, gfxSettings.m_graphics.m_DX_Version);
		SetMenuPreference(PREF_GFX_CITY_DENSITY, iCityDensity);
		SetMenuPreference(PREF_GFX_POP_VARIETY, iPopVariety);
		SetMenuPreference(PREF_GFX_TEXTURE_QUALITY, gfxSettings.m_graphics.m_TextureQuality);
		SetMenuPreference(PREF_GFX_PARTICLES_QUALITY, gfxSettings.m_graphics.m_ParticleQuality);
		SetMenuPreference(PREF_GFX_GRASS_QUALITY, gfxSettings.m_graphics.m_GrassQuality);
		SetMenuPreference(PREF_GFX_SHADER_QUALITY, gfxSettings.m_graphics.m_ShaderQuality);
		SetMenuPreference(PREF_GFX_SHADOW_QUALITY, gfxSettings.m_graphics.m_ShadowQuality - 1);
		SetMenuPreference(PREF_GFX_REFLECTION_QUALITY, gfxSettings.m_graphics.m_ReflectionQuality);
		SetMenuPreference(PREF_GFX_REFLECTION_MSAA, iReflectionMSAA);
		SetMenuPreference(PREF_GFX_WATER_QUALITY, gfxSettings.m_graphics.m_WaterQuality);
		SetMenuPreference(PREF_GFX_SHADOW_SOFTNESS, gfxSettings.m_graphics.m_Shadow_SoftShadows);
		SetMenuPreference(PREF_GFX_FXAA, gfxSettings.m_graphics.m_FXAA_Enabled);
		SetMenuPreference(PREF_GFX_AMBIENT_OCCLUSION, gfxSettings.m_graphics.m_SSAO);
		SetMenuPreference(PREF_GFX_TESSELLATION, gfxSettings.m_graphics.m_Tessellation);
		SetMenuPreference(PREF_GFX_MSAA, iMSAA);
		SetMenuPreference(PREF_GFX_TXAA, gfxSettings.m_graphics.m_TXAA_Enabled);
		SetMenuPreference(PREF_GFX_ANISOTROPIC_FILTERING, iAnisFilter);
		SetMenuPreference(PREF_GFX_POST_FX, gfxSettings.m_graphics.m_PostFX);
		SetMenuPreference(PREF_GFX_DOF, gfxSettings.m_graphics.m_DoF);
		SetMenuPreference(PREF_GFX_MB_STRENGTH, iMBStrength);
		SetMenuPreference(PREF_GFX_DIST_SCALE, iLODScale);
	}
	else
	{
		SetItemPref(PREF_VID_SCREEN_TYPE, gfxSettings.m_video.m_Windowed, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_RESOLUTION, iResolutionPrefValue, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_REFRESH, iRefreshRatePrefValue, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_ADAPTER, gfxSettings.m_video.m_AdapterIndex, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_MONITOR, CSettingsManager::ConvertToMonitorIndex(gfxSettings.m_video.m_AdapterIndex, gfxSettings.m_video.m_OutputIndex), UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_VSYNC, gfxSettings.m_video.m_VSync, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_ASPECT, gfxSettings.m_video.m_AspectRatio, UPDATE_PREFS_DEFAULTING);
		
		SetItemPref(PREF_VID_STEREO, gfxSettings.m_video.m_Stereo, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_VID_STEREO_CONVERGENCE, iConvergence, UPDATE_PREFS_DEFAULTING);
		//SetItemPref(PREF_VID_STEREO_SEPARATION, (int)(gfxSettings.m_video.m_Separation / 10.0f), UPDATE_PREFS_DEFAULTING);

		// Whilst this is not really a vid setting, it is in the vid setting screen so we should reset.
		SetItemPref(PREF_VID_PAUSE_ON_FOCUS_LOSS, gfxSettings.m_video.m_PauseOnFocusLoss, UPDATE_PREFS_DEFAULTING);

		SetItemPref(PREF_GFX_DXVERSION, gfxSettings.m_graphics.m_DX_Version, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_CITY_DENSITY, iCityDensity, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_POP_VARIETY, iPopVariety, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_TEXTURE_QUALITY, gfxSettings.m_graphics.m_TextureQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_PARTICLES_QUALITY, gfxSettings.m_graphics.m_ParticleQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_GRASS_QUALITY, gfxSettings.m_graphics.m_GrassQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_SHADER_QUALITY, gfxSettings.m_graphics.m_ShaderQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_SHADOW_QUALITY, gfxSettings.m_graphics.m_ShadowQuality - 1, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_REFLECTION_QUALITY, gfxSettings.m_graphics.m_ReflectionQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_REFLECTION_MSAA, iReflectionMSAA, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_WATER_QUALITY, gfxSettings.m_graphics.m_WaterQuality, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_SHADOW_SOFTNESS, gfxSettings.m_graphics.m_Shadow_SoftShadows, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_FXAA, gfxSettings.m_graphics.m_FXAA_Enabled, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_AMBIENT_OCCLUSION, gfxSettings.m_graphics.m_SSAO, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_TESSELLATION, gfxSettings.m_graphics.m_Tessellation, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_MSAA, iMSAA, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_TXAA, gfxSettings.m_graphics.m_TXAA_Enabled, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_ANISOTROPIC_FILTERING, iAnisFilter, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_POST_FX, gfxSettings.m_graphics.m_PostFX, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_DOF, gfxSettings.m_graphics.m_DoF, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_MB_STRENGTH, iMBStrength, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_DIST_SCALE, iLODScale, UPDATE_PREFS_DEFAULTING);
		GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_GRAPHICS, true);
	}

	SUIContexts::Deactivate("GFX_Dirty");
}

void CPauseMenu::RestorePreviousAdvancedGraphicsValues(bool bApplyRevert, bool bInit)
{
	if(bApplyRevert)
		CSettingsManager::GetInstance().RevertToPreviousSettings();

	Settings gfxSettings = CSettingsManager::GetInstance().GetUISettings();

	float fMaxLOD = gfxSettings.m_graphics.m_MaxLodScale;
	int iMaxLOD = (int)(fMaxLOD * 10.0f);

	if(bInit)
	{
		SetMenuPreference(PREF_ADV_GFX_LONG_SHADOWS, gfxSettings.m_graphics.m_Shadow_LongShadows);
		SetMenuPreference(PREF_ADV_GFX_ULTRA_SHADOWS, gfxSettings.m_graphics.m_UltraShadows_Enabled);
		SetMenuPreference(PREF_ADV_GFX_HD_FLIGHT, gfxSettings.m_graphics.m_HdStreamingInFlight);
		SetMenuPreference(PREF_ADV_GFX_MAX_LOD, iMaxLOD);
		SetMenuPreference(PREF_GFX_SCALING, gfxSettings.m_graphics.m_SamplingMode);
	}
	else
	{
		SetItemPref(PREF_ADV_GFX_LONG_SHADOWS, gfxSettings.m_graphics.m_Shadow_LongShadows, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_ADV_GFX_ULTRA_SHADOWS, gfxSettings.m_graphics.m_UltraShadows_Enabled, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_ADV_GFX_HD_FLIGHT, gfxSettings.m_graphics.m_HdStreamingInFlight, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_ADV_GFX_MAX_LOD, iMaxLOD, UPDATE_PREFS_DEFAULTING);
		SetItemPref(PREF_GFX_SCALING, gfxSettings.m_graphics.m_SamplingMode, UPDATE_PREFS_DEFAULTING);

		GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX, true);
	}
}

bool CPauseMenu::DirtyGfxSettings()
{
	CGraphicsSettings ogs = CSettingsManager::GetInstance().GetUISettings().m_graphics;
	CGraphicsSettings cgs = GetMenuGraphicsSettings().m_graphics;
	CVideoSettings ovs = CSettingsManager::GetInstance().GetUISettings().m_video;
	CVideoSettings cvs = GetMenuGraphicsSettings().m_video;
	
	int oResIndex = CSettingsManager::GetInstance().GetResolutionIndex();
	int cResIndex = GetMenuPreference(PREF_VID_RESOLUTION);
	bool bDirtyConvergence = m_bStereoOverride ? false : ovs.m_Convergence != cvs.m_Convergence;

	bool bDirty =
		ovs.m_Windowed != cvs.m_Windowed ||
		oResIndex != cResIndex ||
		((ovs.m_RefreshRate != cvs.m_RefreshRate) && (cvs.m_Windowed == 0)) ||
		ovs.m_OutputIndex != cvs.m_OutputIndex ||
		ovs.m_AdapterIndex != cvs.m_AdapterIndex ||
		ovs.m_AspectRatio != cvs.m_AspectRatio ||
		ovs.m_VSync != cvs.m_VSync ||
		ovs.m_Stereo != cvs.m_Stereo ||
		bDirtyConvergence ||
		//ovs.m_Separation != cvs.m_Separation ||
		ovs.m_PauseOnFocusLoss != cvs.m_PauseOnFocusLoss ||
		ogs.m_DX_Version != cgs.m_DX_Version ||
		ogs.m_CityDensity != cgs.m_CityDensity ||
		ogs.m_PedVarietyMultiplier != cgs.m_PedVarietyMultiplier ||
		ogs.m_VehicleVarietyMultiplier != cgs.m_VehicleVarietyMultiplier ||
		ogs.m_DoF != cgs.m_DoF ||
		ogs.m_TextureQuality != cgs.m_TextureQuality ||
		ogs.m_ParticleQuality != cgs.m_ParticleQuality ||
		ogs.m_GrassQuality != cgs.m_GrassQuality ||
		ogs.m_ShaderQuality != cgs.m_ShaderQuality ||
		ogs.m_ShadowQuality != cgs.m_ShadowQuality ||
		ogs.m_ReflectionQuality != cgs.m_ReflectionQuality ||
		ogs.m_ReflectionMSAA != cgs.m_ReflectionMSAA ||
		ogs.m_WaterQuality != cgs.m_WaterQuality ||
		ogs.m_Shadow_SoftShadows != cgs.m_Shadow_SoftShadows ||
		ogs.m_FXAA_Enabled != cgs.m_FXAA_Enabled ||
		ogs.m_MSAA != cgs.m_MSAA ||
		ogs.m_SamplingMode != cgs.m_SamplingMode ||
		ogs.m_TXAA_Enabled != cgs.m_TXAA_Enabled ||
		ogs.m_SSAO != cgs.m_SSAO ||
		ogs.m_AnisotropicFiltering != cgs.m_AnisotropicFiltering ||
		ogs.m_LodScale != cgs.m_LodScale ||
		ogs.m_MotionBlurStrength != cgs.m_MotionBlurStrength ||
		ogs.m_PostFX != cgs.m_PostFX ||
		ogs.m_Tessellation != cgs.m_Tessellation ||
		ogs.m_Shadow_Distance != cgs.m_Shadow_Distance;


#if OUTPUT_DIRTY_GFX_COMP
	if( bDirty )
	{
		Displayf("Settings are dirty");

		Displayf("m_Windowed %d",ovs.m_Windowed != cvs.m_Windowed);
		Displayf("oResIndex %d",oResIndex != cResIndex);
		Displayf("m_RefreshRate %d",((ovs.m_RefreshRate != cvs.m_RefreshRate) && (cvs.m_Windowed == 0)));
		Displayf("m_OutputIndex %d",ovs.m_OutputIndex != cvs.m_OutputIndex);
		Displayf("m_AdapterIndex %d",ovs.m_AdapterIndex != cvs.m_AdapterIndex);
		Displayf("m_AspectRatio %d",ovs.m_AspectRatio != cvs.m_AspectRatio);
		Displayf("m_VSync %d",ovs.m_VSync != cvs.m_VSync);
		Displayf("m_Stereo %d",ovs.m_Stereo != cvs.m_Stereo);
		Displayf("m_Convergence %d",ovs.m_Convergence != cvs.m_Convergence);
		Displayf("m_Separation %d",ovs.m_Separation != cvs.m_Separation);
		Displayf("m_PauseOnFocusLoss %d",ovs.m_PauseOnFocusLoss != cvs.m_PauseOnFocusLoss);
		Displayf("m_DX_Version %d",ogs.m_DX_Version != cgs.m_DX_Version);
		Displayf("m_CityDensity %d",ogs.m_CityDensity != cgs.m_CityDensity);
		Displayf("m_PedVarietyMultiplier %d",ogs.m_PedVarietyMultiplier != cgs.m_PedVarietyMultiplier);
		Displayf("m_VehicleVarietyMultip %d",ogs.m_VehicleVarietyMultiplier != cgs.m_VehicleVarietyMultiplier);
		Displayf("m_TextureQuality %d",ogs.m_TextureQuality != cgs.m_TextureQuality);
		Displayf("m_ParticleQuality %d",ogs.m_ParticleQuality != cgs.m_ParticleQuality);
		Displayf("m_GrassQuality %d",ogs.m_GrassQuality != cgs.m_GrassQuality);
		Displayf("m_ShaderQuality %d",ogs.m_ShaderQuality != cgs.m_ShaderQuality);
		Displayf("m_ShadowQuality %d",ogs.m_ShadowQuality != cgs.m_ShadowQuality);
		Displayf("m_ReflectionQuality %d",ogs.m_ReflectionQuality != cgs.m_ReflectionQuality);
		Displayf("m_ReflectionMSAA %d",ogs.m_ReflectionMSAA != cgs.m_ReflectionMSAA);
		Displayf("m_WaterQuality %d",ogs.m_WaterQuality != cgs.m_WaterQuality);
		Displayf("m_Shadow_SoftShadows %d",ogs.m_Shadow_SoftShadows != cgs.m_Shadow_SoftShadows);
		Displayf("m_FXAA_Enabled %d",ogs.m_FXAA_Enabled != cgs.m_FXAA_Enabled);
		Displayf("m_MSAA %d",ogs.m_MSAA != cgs.m_MSAA);
		Displayf("m_SamplingMode %d",ogs.m_SamplingMode!= cgs.m_SamplingMode);
		Displayf("m_TXAA_Enabled %d",ogs.m_TXAA_Enabled != cgs.m_TXAA_Enabled);
		Displayf("m_SSAO %d",ogs.m_SSAO != cgs.m_SSAO);
		Displayf("m_AnisotropicFiltering %d",ogs.m_AnisotropicFiltering != cgs.m_AnisotropicFiltering);
		Displayf("m_LodScale %d",ogs.m_LodScale != cgs.m_LodScale);
		Displayf("m_MotionBlurStrength %d",ogs.m_MotionBlurStrength != cgs.m_MotionBlurStrength);
		Displayf("m_PostFX %d",ogs.m_PostFX != cgs.m_PostFX);
		Displayf("m_Tessellation %d",ogs.m_Tessellation != cgs.m_Tessellation);
	}
#endif // OUTPUT_DIRTY_GFX_COMP
	return bDirty;
}

bool CPauseMenu::DirtyAdvGfxSettings()
{
	CGraphicsSettings ogs = CSettingsManager::GetInstance().GetUISettings().m_graphics;
	CGraphicsSettings cgs = GetMenuGraphicsSettings().m_graphics;

	bool bDirty = 
		ogs.m_Shadow_LongShadows != cgs.m_Shadow_LongShadows ||
		ogs.m_UltraShadows_Enabled != cgs.m_UltraShadows_Enabled ||
		ogs.m_HdStreamingInFlight != cgs.m_HdStreamingInFlight ||
		ogs.m_MaxLodScale != cgs.m_MaxLodScale;
	return bDirty;
}

bool CPauseMenu::GfxScreenNeedCountDownWarning()
{
	CGraphicsSettings ogs = CSettingsManager::GetInstance().GetUISettings().m_graphics;
	CGraphicsSettings cgs = GetMenuGraphicsSettings().m_graphics;
	CVideoSettings ovs = CSettingsManager::GetInstance().GetUISettings().m_video;
	CVideoSettings cvs = GetMenuGraphicsSettings().m_video;

	bool bShowWarning = 
		(ogs.m_MSAA != cgs.m_MSAA && cgs.m_MSAA > 1) ||
		ogs.m_SamplingMode != cgs.m_SamplingMode || 
		ogs.m_PostFX != cgs.m_PostFX ||
		ovs.m_Windowed != cvs.m_Windowed ||
		ovs.m_ScreenWidth != cvs.m_ScreenWidth ||
		ovs.m_ScreenHeight != cvs.m_ScreenHeight ||
		ovs.m_RefreshRate != cvs.m_RefreshRate ||
		ovs.m_OutputIndex != cvs.m_OutputIndex ||
		ovs.m_AdapterIndex != cvs.m_AdapterIndex ||
		ovs.m_Stereo != cvs.m_Stereo;

	return bShowWarning;
}

void CPauseMenu::BackoutGraphicalChanges(bool bApplyRevert, bool bRefreshMenu)
{
	MenuArrayIndex index = GetActualScreen(GetCurrentActivePanel());

	if(index == MENU_UNIQUE_ID_INVALID)
	{
		RestorePreviousGraphicsValues(false, true);
		RestorePreviousAdvancedGraphicsValues(false,true);
	}
	else
	{
		CMenuScreen& activePanel = CPauseMenu::GetCurrentActivePanelData();
		if( activePanel.MenuScreen == MENU_UNIQUE_ID_SETTINGS_GRAPHICS ||
			activePanel.MenuScreen == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
		{
			bool bStdGfxPanel = activePanel.MenuScreen == MENU_UNIQUE_ID_SETTINGS_GRAPHICS;
			bool bAdvGfxPanel = activePanel.MenuScreen == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX;

			RestorePreviousGraphicsValues(bApplyRevert && bStdGfxPanel, !bRefreshMenu || !bStdGfxPanel);
			RestorePreviousAdvancedGraphicsValues(bApplyRevert && bAdvGfxPanel, !bRefreshMenu || !bAdvGfxPanel);
		}
		else
		{
			RestorePreviousGraphicsValues(bApplyRevert, true);
			RestorePreviousAdvancedGraphicsValues(bApplyRevert, true);
		}

		if(bRefreshMenu)
		{
			GenerateMenuData(activePanel.MenuScreen, true);
			RedrawInstructionalButtons();
		}
	}
}


void CPauseMenu::ConvertToActualResolution(u32& width, u32& height, int screenResolutionIndex)
{
	const int windowedMode = 1;
	if (GetMenuPreference(PREF_VID_SCREEN_TYPE) == windowedMode
	#if !__FINAL
		&& !PARAM_forceResolution.Get()
	#endif
		)
	{
		atArray<grcDisplayWindow> &resolutionsList = CSettingsManager::GetInstance().GetNativeResolutionList(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR));
		if (screenResolutionIndex < resolutionsList.size())
		{
			u32 windowStyle = GetWindowLong( g_hwndMain, GWL_STYLE );

			windowStyle &= ~WS_POPUP;
			windowStyle |= GRCDEVICE.GetWindowFlags();

			RECT testRect = {0, 0, 600, 400};
			::AdjustWindowRect(&testRect, windowStyle, false);

			LONG widthDiff = testRect.right - testRect.left - 600;
			LONG heightDiff = testRect.bottom - testRect.top - 400;

			LONG largestWidth = 0;
			LONG largestHeight = 0;

			for(int i = 0; i < resolutionsList.size(); ++i)
			{
				largestWidth = max(resolutionsList[i].uWidth, largestWidth);
				largestHeight = max(resolutionsList[i].uHeight, largestHeight);
			}				

			width = min(largestWidth - widthDiff, width);
			height = min(largestHeight - heightDiff, height);
		}
	}
}

Settings CPauseMenu::GetMenuGraphicsSettings(bool bForSaving)
{
	Settings gfxSettings = CSettingsManager::GetInstance().GetUISettings();

	if(bForSaving)
	{
		const int DX_VERSION_10_1_INDEX = 1;
		bool bDX11InstructionSet = GetMenuPreference(PREF_GFX_DXVERSION) >= DX_VERSION_11_INDEX;
		bool bDX101InstructionSet = GetMenuPreference(PREF_GFX_DXVERSION) >= DX_VERSION_10_1_INDEX;
		bool bPostFXLevel1 = GetMenuPreference(PREF_GFX_POST_FX) > 0;
		bool bPostFXLevel2 = GetMenuPreference(PREF_GFX_POST_FX) > 1;
		bool bFullscreen = GetMenuPreference(PREF_VID_SCREEN_TYPE) == 0;

		if(!bDX101InstructionSet)
		{
			SetMenuPreference(PREF_GFX_MSAA, 0);
			SetMenuPreference(PREF_GFX_REFLECTION_MSAA, 0);
		}

		if(!bDX11InstructionSet)
		{
			SetMenuPreference(PREF_GFX_TESSELLATION, 0);
			SetMenuPreference(PREF_GFX_GRASS_QUALITY, 0);
			SetMenuPreference(PREF_GFX_SHADOW_SOFTNESS, min(GetMenuPreference(PREF_GFX_SHADOW_SOFTNESS), 3));
			SetMenuPreference(PREF_GFX_DOF, 0);
		}

		bool bMSAASupported = GetMenuPreference(PREF_GFX_MSAA) < 3 && GetMenuPreference(PREF_GFX_MSAA) > 0;
		if(!bMSAASupported || !SUIContexts::IsActive("GFX_SUPPORT_TXAA"))
		{
			SetMenuPreference(PREF_GFX_TXAA, 0);
		}

		if(!bPostFXLevel1)
		{
			SetMenuPreference(PREF_GFX_MB_STRENGTH, 0);
		}

		if(!bPostFXLevel2)
		{
			SetMenuPreference(PREF_GFX_DOF, 0);
		}

		if(!bFullscreen || !SUIContexts::IsActive("VID_STEREO"))
		{
			SetMenuPreference(PREF_VID_STEREO, 0);
		}
	}
	
	atArray<grcDisplayWindow> &resolutionList = CSettingsManager::GetInstance().GetResolutionList(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR));
	atArray<grcDisplayWindow> refreshRateList;
	grcDisplayWindow currentResolution;
	grcDisplayWindow currentRefreshRate;
	GetDisplayInfoList(refreshRateList);
	int iCurrentRefreshPref = GetMenuPreference(PREF_VID_REFRESH);
	int iCurrentResolutionPref = GetMenuPreference(PREF_VID_RESOLUTION);
	int iMaxListSize = resolutionList.size();

	if(iCurrentResolutionPref >= iMaxListSize || iCurrentResolutionPref < 0)
		iCurrentResolutionPref = 0;
	
	currentResolution = resolutionList[iCurrentResolutionPref];
	
	CullRefreshRateList(refreshRateList, currentResolution.uWidth, currentResolution.uHeight);
	if(iCurrentRefreshPref >= refreshRateList.size())
		iCurrentRefreshPref = 0;

	if (refreshRateList.size() == 0)
		currentRefreshRate.uRefreshRate = 60;
	else
		currentRefreshRate = refreshRateList[iCurrentRefreshPref];

//	ConvertToActualResolution(currentResolution.uWidth, currentResolution.uHeight);

	gfxSettings.m_video.m_ScreenWidth = currentResolution.uWidth;
	gfxSettings.m_video.m_ScreenHeight = currentResolution.uHeight;
	gfxSettings.m_video.m_RefreshRate = currentRefreshRate.uRefreshRate;
	gfxSettings.m_video.m_Windowed = CPauseMenu::GetMenuPreference(PREF_VID_SCREEN_TYPE);
	CSettingsManager::ConvertFromMonitorIndex(CPauseMenu::GetMenuPreference(PREF_VID_MONITOR), gfxSettings.m_video.m_AdapterIndex, gfxSettings.m_video.m_OutputIndex);
	gfxSettings.m_video.m_VSync = CPauseMenu::GetMenuPreference(PREF_VID_VSYNC);
	gfxSettings.m_video.m_AspectRatio = (eAspectRatio)CPauseMenu::GetMenuPreference(PREF_VID_ASPECT);
	
	gfxSettings.m_video.m_Stereo = CPauseMenu::GetMenuPreference(PREF_VID_STEREO);
	gfxSettings.m_video.m_Convergence = m_bStereoOverride ? GRCDEVICE.GetCachedConvergenceDistance() : GetStereoConvergenceBackendValue(CPauseMenu::GetMenuPreference(PREF_VID_STEREO_CONVERGENCE));
	//gfxSettings.m_video.m_Separation = (float)CPauseMenu::GetMenuPreference(PREF_VID_STEREO_SEPARATION) * 10;
	
	gfxSettings.m_video.m_PauseOnFocusLoss = CPauseMenu::GetMenuPreference(PREF_VID_PAUSE_ON_FOCUS_LOSS);

	gfxSettings.m_graphics.m_DX_Version = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_DXVERSION);
	gfxSettings.m_graphics.m_CityDensity = (float)(CPauseMenu::GetMenuPreference(PREF_GFX_CITY_DENSITY)) / 10.0f;
    gfxSettings.m_graphics.m_PedVarietyMultiplier = (float)(CPauseMenu::GetMenuPreference(PREF_GFX_POP_VARIETY)) / 10.0f;
    gfxSettings.m_graphics.m_VehicleVarietyMultiplier = (float)(CPauseMenu::GetMenuPreference(PREF_GFX_POP_VARIETY)) / 10.0f;
	gfxSettings.m_graphics.m_TextureQuality = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_TEXTURE_QUALITY);
	gfxSettings.m_graphics.m_ShaderQuality = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_SHADER_QUALITY);
	gfxSettings.m_graphics.m_ShadowQuality = (CSettings::eSettingsLevel)(CPauseMenu::GetMenuPreference(PREF_GFX_SHADOW_QUALITY) + 1);
	gfxSettings.m_graphics.m_ReflectionQuality = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_REFLECTION_QUALITY);
	gfxSettings.m_graphics.m_WaterQuality = (CSettings::eSettingsLevel)(CPauseMenu::GetMenuPreference(PREF_GFX_WATER_QUALITY));
	gfxSettings.m_graphics.m_ParticleQuality = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_PARTICLES_QUALITY);
	gfxSettings.m_graphics.m_GrassQuality = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_GRASS_QUALITY);
	gfxSettings.m_graphics.m_FXAA_Enabled = CPauseMenu::GetMenuPreference(PREF_GFX_FXAA) != 0;
	gfxSettings.m_graphics.m_SSAO = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_AMBIENT_OCCLUSION);
	gfxSettings.m_graphics.m_Tessellation = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_TESSELLATION);

	int iMSAA = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_MSAA);
	int iReflectionMSAA = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_REFLECTION_MSAA);
	int iAnisFilter = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_ANISOTROPIC_FILTERING);

	const int convertToMSAA[] = {0, 2, 4, 8};
	gfxSettings.m_graphics.m_MSAA = convertToMSAA[iMSAA];
	gfxSettings.m_graphics.m_ReflectionMSAA = convertToMSAA[iReflectionMSAA];

	gfxSettings.m_graphics.m_SamplingMode = CPauseMenu::GetMenuPreference(PREF_GFX_SCALING);

	gfxSettings.m_graphics.m_TXAA_Enabled = GetMenuPreference(PREF_GFX_TXAA) != 0;
	gfxSettings.m_graphics.m_AnisotropicFiltering = iAnisFilter > 0 ? (int)pow(2.0f, (float)iAnisFilter) : 0;
	gfxSettings.m_graphics.m_Shadow_SoftShadows = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_SHADOW_SOFTNESS);
	gfxSettings.m_graphics.m_LodScale = (float)(CPauseMenu::GetMenuPreference(PREF_GFX_DIST_SCALE)) / 10.0f;
	gfxSettings.m_graphics.m_PostFX = (CSettings::eSettingsLevel)CPauseMenu::GetMenuPreference(PREF_GFX_POST_FX);
	gfxSettings.m_graphics.m_DoF = CPauseMenu::GetMenuPreference(PREF_GFX_DOF) != 0;
	gfxSettings.m_graphics.m_MotionBlurStrength = (float)(CPauseMenu::GetMenuPreference(PREF_GFX_MB_STRENGTH)) / 10.0f;

	// Advanced Settings
	float fMaxLOD = (float)GetMenuPreference(PREF_ADV_GFX_MAX_LOD) / 10.0f;
	float fMaxShadowDistance = (float)GetMenuPreference(PREF_ADV_GFX_SHADOWS_DIST_MULT) / 10.0f;

	if(gfxSettings.m_graphics.m_ShadowQuality <= CSettings::eSettingsLevel::High)
	{
		fMaxShadowDistance = 0.0f;
	}

	gfxSettings.m_graphics.m_Shadow_LongShadows = GetMenuPreference(PREF_ADV_GFX_LONG_SHADOWS) != 0;
	gfxSettings.m_graphics.m_UltraShadows_Enabled = GetMenuPreference(PREF_ADV_GFX_ULTRA_SHADOWS)  != 0;
	gfxSettings.m_graphics.m_HdStreamingInFlight = GetMenuPreference(PREF_ADV_GFX_HD_FLIGHT) != 0;
	gfxSettings.m_graphics.m_Shadow_Distance = 1.0f + fMaxShadowDistance;
	gfxSettings.m_graphics.m_MaxLodScale = fMaxLOD;

	return gfxSettings;
}

void CPauseMenu::UpdateMemoryBar(bool bInit)
{
	int iMemoryBarScreenIndex = 0;
	char memoryBuffer[300];

	Settings menuSettingsData = CSettingsManager::GetInstance().GetUISettings();
	MenuScreenId curScreen = GetCurrentActivePanel();
	CMenuScreen& screenData = GetScreenData(MENU_UNIQUE_ID_SETTINGS);
	screenData.FindItemIndex(PREF_MEMORY_BAR, &iMemoryBarScreenIndex);

	if((curScreen == MENU_UNIQUE_ID_SETTINGS_GRAPHICS) || (curScreen == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX))
		menuSettingsData = GetMenuGraphicsSettings();

	const int BYTES_TO_MEGABYTES = 1048576; //1024 * 1024
	int iNumCores = GRCDEVICE.GetGPUCount(false) ? GRCDEVICE.GetGPUCount(false) : 1;
	s64 iMemoryAvailable = ( ((grcAdapterD3D11*) grcAdapterManager::GetInstance()->GetAdapter(menuSettingsData.m_video.m_AdapterIndex))->GetAvailableVideoMemory() / BYTES_TO_MEGABYTES) * iNumCores;
	s64 iMemoryUsage = (SettingsDefaults::GetInstance().videoMemSizeFor(menuSettingsData) / BYTES_TO_MEGABYTES) * iNumCores;
	s64 iMemoryUsageDisplayed = iMemoryUsage;
	if(iMemoryUsage > iMemoryAvailable)
		iMemoryUsageDisplayed = iMemoryAvailable;

	float fPercent = (float)iMemoryUsage / (float)iMemoryAvailable;
	int iPercentage = (int)(fPercent * 100.0f);

	eHUD_COLOURS hudColour;
	if(iMemoryUsage <= (iMemoryAvailable * 0.75))
	{
		hudColour = HUD_COLOUR_GREEN;
		sm_bCanApply = true;
	}
	else if (iMemoryUsage > (iMemoryAvailable * 0.75) && iMemoryUsage < iMemoryAvailable)
	{
		hudColour = HUD_COLOUR_YELLOW;
		sm_bCanApply = true;
	}
	else if(iMemoryUsage >= iMemoryAvailable)
	{
		hudColour = HUD_COLOUR_RED;
		sm_bCanApply = false;
	}
	else
	{
		hudColour = HUD_COLOUR_REDDARK;
		sm_bCanApply = false;
	}

	sprintf(memoryBuffer, "%s: %d MB / %d MB",
		TheText.Get("GFX_VIDMEM"), 
		iMemoryUsage, 
		iMemoryAvailable);

	if(GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).BeginMethod("SET_VIDEO_MEMORY_BAR"))
	{
		CScaleformMgr::AddParamBool(bInit);
		CScaleformMgr::AddParamString(memoryBuffer);
		CScaleformMgr::AddParamInt(iPercentage);
		CScaleformMgr::AddParamInt(hudColour);
		CScaleformMgr::EndMethod();
	}
}

void CPauseMenu::UpdateVoiceBar(bool bUpdateSlot)
{
	int iVoiceBarScreenIndex = 0;
	Settings menuSettingsData = CSettingsManager::GetInstance().GetUISettings();
	MenuScreenId curScreen = GetCurrentActivePanel();
	CMenuScreen& screenData = GetScreenData(curScreen);
	screenData.FindItemIndex(PREF_VOICE_FEEDBACK, &iVoiceBarScreenIndex);

	float fLoudness = 0.0f;
	if( GetMenuPreference(PREF_VOICE_ENABLE) != 0 &&
		GetMenuPreference(PREF_VOICE_TALK_ENABLED) != 0)
	{
		fLoudness = NetworkInterface::GetVoice().GetPlayerLoudness(0);
	}
	int iLoudness = (int)(fLoudness * 100.0f);
	static int s_iLastLoudness = -1;


	if( (s_iLastLoudness != iLoudness || !bUpdateSlot) 
		&& CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_MIDDLE, iVoiceBarScreenIndex, ATSTRINGHASH("MEMORY_BAR", 0xB309CF70) + PREF_OPTIONS_THRESHOLD, 
			iVoiceBarScreenIndex, eOPTION_DISPLAY_STYLE(4), iLoudness, false, TheText.Get("VOX_FEEDBACK"), false, bUpdateSlot) )
	{
		CScaleformMgr::AddParamInt(100);
		
		eHUD_COLOURS hudColour = CNewHud::GetCurrentCharacterColour();
		CScaleformMgr::AddParamInt((s32)hudColour);

		CScaleformMgr::EndMethod();
		s_iLastLoudness = iLoudness;
	}
}

void CPauseMenu::UpdateVoiceSettings()
{
	FrontendVoiceSettings voiceSettings;
	u32 uInputDeviceId = 0;
	u32 uOutputDeviceId = 0;

	RVDeviceInfo inputDevices[VoiceChat::MAX_DEVICES];
	RVDeviceInfo outputDevices[VoiceChat::MAX_DEVICES];
	int iInputDeviceCount = RVoice::ListDevices(inputDevices, VoiceChat::MAX_DEVICES, RV_CAPTURE);
	int iOutputDeviceCount = RVoice::ListDevices(outputDevices, VoiceChat::MAX_DEVICES, RV_PLAYBACK);

	if(iInputDeviceCount > 0)
	{
		GUID id = inputDevices[GetMenuPreference(PREF_VOICE_INPUT_DEVICE)].m_id;
		uInputDeviceId = atDataHash((unsigned int*)&id, sizeof(id));
	}

	if(iOutputDeviceCount > 0)
	{
		GUID id = outputDevices[GetMenuPreference(PREF_VOICE_OUTPUT_DEVICE)].m_id;
		uOutputDeviceId = atDataHash((unsigned int*)&id, sizeof(id));
	}

	voiceSettings.m_bEnabled = GetMenuPreference(PREF_VOICE_ENABLE) == 1;
	voiceSettings.m_bOutputEnabled = GetMenuPreference(PREF_VOICE_OUTPUT) == 1;
	voiceSettings.m_bTalkEnabled = GetMenuPreference(PREF_VOICE_TALK_ENABLED) == 1;
	voiceSettings.m_iOutputVolume = GetMenuPreference(PREF_VOICE_OUTPUT_VOLUME);
	voiceSettings.m_iMicVolume = GetMenuPreference(PREF_VOICE_MIC_VOLUME);
	voiceSettings.m_iMicSensitivity = 10 -  GetMenuPreference(PREF_VOICE_MIC_SENSITIVITY);
	voiceSettings.m_uInputDevice = uInputDeviceId;
	voiceSettings.m_uOutputDevice = uOutputDeviceId;
	voiceSettings.m_eMode = (VoiceChat::CaptureMode)GetMenuPreference(PREF_VOICE_CHAT_MODE);

	NetworkInterface::GetVoice().AdjustSettings(voiceSettings);
}

void CPauseMenu::UpdateVoiceMuteOnLostFocus()
{
	// We set this independently of the other voicechat settings every frame in order to:
	//	1) Remove the dependency between on graphics from the voicechat code.
	//  2) Account for network re-initialization.
	NetworkInterface::GetVoice().SetMuteChatBecauseLostFocus((GetMenuPreference(PREF_AUDIO_MUTE_ON_FOCUS_LOSS) == 1) && GRCDEVICE.GetLostFocusForAudio());
}

bool CPauseMenu::GetCanApplySettings()
{
	bool bCanApply = false;
	MenuScreenId curScreen = GetCurrentActivePanel();
	s64 iCurrentMemory = 0;
	s64 iAttemptedMemory = 0;
	bool bWereMemoryRelatedSettingsChanged = true;

	if(curScreen == MENU_UNIQUE_ID_SETTINGS_GRAPHICS)
	{
		Settings currentSettingsData = CSettingsManager::GetInstance().GetUISettings();
		Settings attemptedSettingsData = GetMenuGraphicsSettings();
		
		const int BYTES_TO_MEGABYTES = 1048576; //1024 * 1024
		iCurrentMemory = SettingsDefaults::GetInstance().videoMemSizeFor(currentSettingsData)  / BYTES_TO_MEGABYTES;
		iAttemptedMemory = SettingsDefaults::GetInstance().videoMemSizeFor(attemptedSettingsData)  / BYTES_TO_MEGABYTES;

		if(currentSettingsData.AreVideoMemorySettingsSame(attemptedSettingsData))
		{
			bWereMemoryRelatedSettingsChanged = false;
		}
	}

	bCanApply = sm_bCanApply ||
				iAttemptedMemory < iCurrentMemory ||
				!bWereMemoryRelatedSettingsChanged ||
				GetMenuPreference(PREF_GFX_VID_OVERRIDE) != 0 ||
				PARAM_ignoreGfxLimit.Get();

	return bCanApply;
}
#endif // RSG_PC

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	RestoreAudioDefaults
// PURPOSE:	set the starting values of the audio preferences
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::RestoreAudioDefaults()
{
	SetItemPref(PREF_SFX_VOLUME, 9, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MUSIC_VOLUME, 9, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_MUSIC_VOLUME_IN_MP, 9, UPDATE_PREFS_DEFAULTING);
#if RSG_PC
	const int iStereoAudio = 3;
#else
	const int iStereoAudio = 1;
#endif
	SetItemPref(PREF_SPEAKER_OUTPUT, iStereoAudio, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_INTERACTIVE_MUSIC, TRUE, UPDATE_PREFS_DEFAULTING);
#if !RSG_PC
	SetItemPref(PREF_VOICE_OUTPUT, 0, UPDATE_PREFS_DEFAULTING);
#endif // !RSG_PC
	SetItemPref(PREF_RADIO_STATION, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_DIAG_BOOST, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_SS_FRONT, 1, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_SS_REAR, 1, UPDATE_PREFS_DEFAULTING);
#if __PPU
	// default to no fade on PS3
	SetItemPref(PREF_VOICE_OUTPUT, 1, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_VOICE_SPEAKERS, FALSE, UPDATE_PREFS_DEFAULTING);
#endif // __PPU

#if RSG_ORBIS
	SetItemPref(PREF_VOICE_SPEAKERS, TRUE, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_CTRL_SPEAKER, 1, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_CTRL_SPEAKER_HEADPHONE, 0, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_CTRL_SPEAKER_VOL, 5, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_PULSE_HEADSET, FALSE, UPDATE_PREFS_DEFAULTING);
#endif // RSG_ORBIS

#if RSG_CPU_X64
	// default to no fade on NG platforms
	SetItemPref(PREF_VOICE_OUTPUT, 1, UPDATE_PREFS_DEFAULTING);
#endif

#if RSG_PC
	SetItemPref(PREF_UR_AUTOSCAN, 1, UPDATE_PREFS_DEFAULTING);
	SetItemPref(PREF_UR_PLAYMODE, 0, UPDATE_PREFS_DEFAULTING);
#endif

	UpdateProfileFromMenuOptions();
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateMenuOptionsFromProfile()
// PURPOSE:	updates the menu option preferences from the player profile
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::UpdateMenuOptionsFromProfile()
{
	bool bSafeZoneChanged = false;

	// I'm told the intention is to set the default values initially and then populate with the valid values from the profile settings.
	// Adding the default call here since it was never being called as intended.
	SetDefaultValues(false);

	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid())
	{
		if(settings.Exists(CProfileSettings::CONTROLLER_VIBRATION)) 
			SetMenuPreference(PREF_VIBRATION, (settings.GetInt(CProfileSettings::CONTROLLER_VIBRATION) != 0) ? 1 : 0);
		if(settings.Exists(CProfileSettings::AXIS_INVERSION))
			SetMenuPreference(PREF_INVERT_LOOK, settings.GetInt(CProfileSettings::AXIS_INVERSION));
		if(settings.Exists(CProfileSettings::TARGETING_MODE)) 
			SetMenuPreference(PREF_TARGET_CONFIG, settings.GetInt(CProfileSettings::TARGETING_MODE));
		if(settings.Exists(CProfileSettings::CONTROLLER_AIM_SENSITIVITY)) 
			SetMenuPreference(PREF_CONTROLLER_SENSITIVITY, settings.GetInt(CProfileSettings::CONTROLLER_AIM_SENSITIVITY));
		if(settings.Exists(CProfileSettings::LOOK_AROUND_SENSITIVITY)) 
			SetMenuPreference(PREF_LOOK_AROUND_SENSITIVITY, settings.GetInt(CProfileSettings::LOOK_AROUND_SENSITIVITY));
#if KEYBOARD_MOUSE_SUPPORT
		if(settings.Exists(CProfileSettings::MOUSE_INVERSION)) 
			SetMenuPreference(PREF_INVERT_MOUSE, settings.GetInt(CProfileSettings::MOUSE_INVERSION));

		if(settings.Exists(CProfileSettings::MOUSE_INVERSION_FLYING)) 
			SetMenuPreference(PREF_INVERT_MOUSE_FLYING, settings.GetInt(CProfileSettings::MOUSE_INVERSION_FLYING));

		if(settings.Exists(CProfileSettings::MOUSE_INVERSION_SUB)) 
			SetMenuPreference(PREF_INVERT_MOUSE_SUB, settings.GetInt(CProfileSettings::MOUSE_INVERSION_SUB));

		if(settings.Exists(CProfileSettings::MOUSE_SWAP_ROLL_YAW_FLYING)) 
			SetMenuPreference(PREF_SWAP_ROLL_YAW_MOUSE_FLYING, settings.GetInt(CProfileSettings::MOUSE_SWAP_ROLL_YAW_FLYING));

		if(settings.Exists(CProfileSettings::MOUSE_AUTOCENTER_BIKE)) 
			SetMenuPreference(PREF_MOUSE_AUTOCENTER_BIKE, settings.GetInt(CProfileSettings::MOUSE_AUTOCENTER_BIKE));

		if(settings.Exists(CProfileSettings::MOUSE_AUTOCENTER_CAR)) 
			SetMenuPreference(PREF_MOUSE_AUTOCENTER_CAR, settings.GetInt(CProfileSettings::MOUSE_AUTOCENTER_CAR));

		if(settings.Exists(CProfileSettings::MOUSE_AUTOCENTER_PLANE)) 
			SetMenuPreference(PREF_MOUSE_AUTOCENTER_PLANE, settings.GetInt(CProfileSettings::MOUSE_AUTOCENTER_PLANE));

		if(settings.Exists(CProfileSettings::FPS_DEFAULT_AIM_TYPE))
			SetMenuPreference(PREF_FPS_DEFAULT_AIM_TYPE, (settings.GetInt(CProfileSettings::FPS_DEFAULT_AIM_TYPE)));
#endif // KEYBOARD_MOUSE_SUPPORT

#if LIGHT_EFFECTS_SUPPORT
		if(settings.Exists(CProfileSettings::CONTROLLER_LIGHT_EFFECT)) 
			SetMenuPreference(PREF_CONTROLLER_LIGHT_EFFECT, settings.GetInt(CProfileSettings::CONTROLLER_LIGHT_EFFECT));
#endif // LIGHT_EFFECTS_SUPPORT

#if __6AXISACTIVE
		if(settings.Exists(CProfileSettings::CONTROLLER_SIXAXIS_HELI)) 
			SetMenuPreference(PREF_SIXAXIS_HELI, settings.GetInt(CProfileSettings::CONTROLLER_SIXAXIS_HELI));
		if(settings.Exists(CProfileSettings::CONTROLLER_SIXAXIS_BIKE)) 
			SetMenuPreference(PREF_SIXAXIS_BIKE, settings.GetInt(CProfileSettings::CONTROLLER_SIXAXIS_BIKE));
		if(settings.Exists(CProfileSettings::CONTROLLER_SIXAXIS_BOAT)) 
			SetMenuPreference(PREF_SIXAXIS_BOAT, settings.GetInt(CProfileSettings::CONTROLLER_SIXAXIS_BOAT));
		if(settings.Exists(CProfileSettings::CONTROLLER_SIXAXIS_RELOAD)) 
			SetMenuPreference(PREF_SIXAXIS_RELOAD, settings.GetInt(CProfileSettings::CONTROLLER_SIXAXIS_RELOAD));
		if(settings.Exists(CProfileSettings::CONTROLLER_SIXAXIS_MISSION)) 
			SetMenuPreference(PREF_SIXAXIS_CALIBRATION, settings.GetInt(CProfileSettings::CONTROLLER_SIXAXIS_MISSION));
		if(settings.Exists(CProfileSettings::CONTROLLER_SIXAXIS_ACTIVITY)) 
			SetMenuPreference(PREF_SIXAXIS_ACTIVITY, settings.GetInt(CProfileSettings::CONTROLLER_SIXAXIS_ACTIVITY));
		if(settings.Exists(CProfileSettings::CONTROLLER_SIXAXIS_AFTERTOUCH)) 
			SetMenuPreference(PREF_SIXAXIS_AFTERTOUCH, settings.GetInt(CProfileSettings::CONTROLLER_SIXAXIS_AFTERTOUCH));
#endif // __6AXISACTIVE
		if(settings.Exists(CProfileSettings::CONTROLLER_CONTROL_CONFIG))
			SetMenuPreference(PREF_CONTROL_CONFIG, settings.GetInt(CProfileSettings::CONTROLLER_CONTROL_CONFIG));
		if(settings.Exists(CProfileSettings::CONTROLLER_CONTROL_CONFIG_FPS))
			SetMenuPreference(PREF_CONTROL_CONFIG_FPS, settings.GetInt(CProfileSettings::CONTROLLER_CONTROL_CONFIG_FPS));
		if(settings.Exists(CProfileSettings::DISPLAY_GAMMA)) 

			SetMenuPreference(PREF_GAMMA, (settings.GetInt(CProfileSettings::DISPLAY_GAMMA)));
		if(settings.Exists(CProfileSettings::DISPLAY_CAMERA_HEIGHT)) 
			SetMenuPreference(PREF_CAMERA_HEIGHT, (settings.GetInt(CProfileSettings::DISPLAY_CAMERA_HEIGHT)));
		if(settings.Exists(CProfileSettings::DISPLAY_SUBTITLES)) 
			SetMenuPreference(PREF_SUBTITLES, (settings.GetInt(CProfileSettings::DISPLAY_SUBTITLES)));
		if(settings.Exists(CProfileSettings::DISPLAY_DOF)) 
			SetMenuPreference(PREF_DOF, (settings.GetInt(CProfileSettings::DISPLAY_DOF)));
		if(settings.Exists(CProfileSettings::DISPLAY_RADAR_MODE)) 
			SetMenuPreference(PREF_RADAR_MODE, (settings.GetInt(CProfileSettings::DISPLAY_RADAR_MODE)));
		if(settings.Exists(CProfileSettings::DISPLAY_HUD_MODE)) 
			SetMenuPreference(PREF_DISPLAY_HUD, (settings.GetInt(CProfileSettings::DISPLAY_HUD_MODE)));
		if(settings.Exists(CProfileSettings::DISPLAY_SKFX)) 
			SetMenuPreference(PREF_SKFX, (settings.GetInt(CProfileSettings::DISPLAY_SKFX)));
		if(settings.Exists(CProfileSettings::DISPLAY_RETICULE)) 
			SetMenuPreference(PREF_RETICULE, (settings.GetInt(CProfileSettings::DISPLAY_RETICULE)));
		if(settings.Exists(CProfileSettings::DISPLAY_RETICULE_SIZE)) 
			SetMenuPreference(PREF_RETICULE_SIZE, (settings.GetInt(CProfileSettings::DISPLAY_RETICULE_SIZE)));
		if(settings.Exists(CProfileSettings::DISPLAY_LANGUAGE)) 
			SetMenuPreference(PREF_CURRENT_LANGUAGE, (settings.GetInt(CProfileSettings::DISPLAY_LANGUAGE)));
		if(settings.Exists(CProfileSettings::DISPLAY_GPS)) 
			SetMenuPreference(PREF_DISPLAY_GPS, (settings.GetInt(CProfileSettings::DISPLAY_GPS)));
		if(settings.Exists(CProfileSettings::DISPLAY_BIG_RADAR))
			SetMenuPreference(PREF_BIG_RADAR, (settings.GetInt(CProfileSettings::DISPLAY_BIG_RADAR)));
		if(settings.Exists(CProfileSettings::DISPLAY_BIG_RADAR_NAMES))
			SetMenuPreference(PREF_BIG_RADAR_NAMES, (settings.GetInt(CProfileSettings::DISPLAY_BIG_RADAR_NAMES)));
		if(settings.Exists(CProfileSettings::DISPLAY_TEXT_CHAT))
			SetMenuPreference(PREF_SHOW_TEXT_CHAT, (settings.GetInt(CProfileSettings::DISPLAY_TEXT_CHAT)));

		if(settings.Exists(CProfileSettings::DISPLAY_SAFEZONE_SIZE)) 
		{
			if (SetMenuPreference(PREF_SAFEZONE_SIZE, (SAFEZONE_SLIDER_MAX - settings.GetInt(CProfileSettings::DISPLAY_SAFEZONE_SIZE))))
			{
				bSafeZoneChanged = true;
			}
		}

		
		if(settings.Exists(CProfileSettings::START_UP_FLOW)) 
			SetMenuPreference(PREF_STARTUP_FLOW, (settings.GetInt(CProfileSettings::START_UP_FLOW)));
		if(settings.Exists(CProfileSettings::LANDING_PAGE)) 
			SetMenuPreference(PREF_LANDING_PAGE, (settings.GetInt(CProfileSettings::LANDING_PAGE)));

		if(settings.Exists(CProfileSettings::FEED_PHONE)) 
			SetMenuPreference(PREF_FEED_PHONE, (settings.GetInt(CProfileSettings::FEED_PHONE)));
		if(settings.Exists(CProfileSettings::FEED_STATS)) 
			SetMenuPreference(PREF_FEED_STATS, (settings.GetInt(CProfileSettings::FEED_STATS)));
		if(settings.Exists(CProfileSettings::FEED_CREW)) 
			SetMenuPreference(PREF_FEED_CREW, (settings.GetInt(CProfileSettings::FEED_CREW)));
		if(settings.Exists(CProfileSettings::FEED_FRIENDS)) 
			SetMenuPreference(PREF_FEED_FRIENDS, (settings.GetInt(CProfileSettings::FEED_FRIENDS)));
		if(settings.Exists(CProfileSettings::FEED_SOCIAL)) 
			SetMenuPreference(PREF_FEED_SOCIAL, (settings.GetInt(CProfileSettings::FEED_SOCIAL)));
		if(settings.Exists(CProfileSettings::FEED_STORE)) 
			SetMenuPreference(PREF_FEED_STORE, (settings.GetInt(CProfileSettings::FEED_STORE)));
		if(settings.Exists(CProfileSettings::FEED_TOOPTIP)) 
			SetMenuPreference(PREF_FEED_TOOLTIP, (settings.GetInt(CProfileSettings::FEED_TOOPTIP)));
		if(settings.Exists(CProfileSettings::FEED_DELAY)) 
			SetMenuPreference(PREF_FEED_DELAY, (settings.GetInt(CProfileSettings::FEED_DELAY)));

		if(settings.Exists(CProfileSettings::MEASUREMENT_SYSTEM)) 
			SetMenuPreference(PREF_MEASUREMENT_SYSTEM, (settings.GetInt(CProfileSettings::MEASUREMENT_SYSTEM)));

		if(settings.Exists(CProfileSettings::FPS_PERSISTANT_VIEW)) 
			SetMenuPreference(PREF_FPS_PERSISTANT_VIEW, (settings.GetInt(CProfileSettings::FPS_PERSISTANT_VIEW)));
		if(settings.Exists(CProfileSettings::FPS_FIELD_OF_VIEW)) 
			SetMenuPreference(PREF_FPS_FIELD_OF_VIEW, (settings.GetInt(CProfileSettings::FPS_FIELD_OF_VIEW)));
		if(settings.Exists(CProfileSettings::FPS_LOOK_SENSITIVITY)) 
			SetMenuPreference(PREF_FPS_LOOK_SENSITIVITY, (settings.GetInt(CProfileSettings::FPS_LOOK_SENSITIVITY)));
		if(settings.Exists(CProfileSettings::FPS_AIM_SENSITIVITY)) 
			SetMenuPreference(PREF_FPS_AIM_SENSITIVITY, (settings.GetInt(CProfileSettings::FPS_AIM_SENSITIVITY)));
		if(settings.Exists(CProfileSettings::FPS_AIM_DEADZONE)) 
			SetMenuPreference(PREF_FPS_AIM_DEADZONE, (settings.GetInt(CProfileSettings::FPS_AIM_DEADZONE)));
		if(settings.Exists(CProfileSettings::FPS_AIM_ACCELERATION)) 
			SetMenuPreference(PREF_FPS_AIM_ACCELERATION, (settings.GetInt(CProfileSettings::FPS_AIM_ACCELERATION)));
		if(settings.Exists(CProfileSettings::AIM_DEADZONE)) 
			SetMenuPreference(PREF_AIM_DEADZONE, (settings.GetInt(CProfileSettings::AIM_DEADZONE)));
		if(settings.Exists(CProfileSettings::AIM_ACCELERATION)) 
			SetMenuPreference(PREF_AIM_ACCELERATION, (settings.GetInt(CProfileSettings::AIM_ACCELERATION)));
		if(settings.Exists(CProfileSettings::FPS_AUTO_LEVEL)) 
			SetMenuPreference(PREF_FPS_AUTO_LEVEL, (settings.GetInt(CProfileSettings::FPS_AUTO_LEVEL)));

		if(settings.Exists(CProfileSettings::FPS_RAGDOLL)) 
			SetMenuPreference(PREF_FPS_RAGDOLL, (settings.GetInt(CProfileSettings::FPS_RAGDOLL)));
		if(settings.Exists(CProfileSettings::FPS_COMBATROLL)) 
			SetMenuPreference(PREF_FPS_COMBATROLL, (settings.GetInt(CProfileSettings::FPS_COMBATROLL)));
		if(settings.Exists(CProfileSettings::FPS_HEADBOB)) 
			SetMenuPreference(PREF_FPS_HEADBOB, (settings.GetInt(CProfileSettings::FPS_HEADBOB)));
		if(settings.Exists(CProfileSettings::FPS_THIRD_PERSON_COVER)) 
			SetMenuPreference(PREF_FPS_THIRD_PERSON_COVER, (settings.GetInt(CProfileSettings::FPS_THIRD_PERSON_COVER)));
		if(settings.Exists(CProfileSettings::HOOD_CAMERA)) 
			SetMenuPreference(PREF_HOOD_CAMERA, (settings.GetInt(CProfileSettings::HOOD_CAMERA)));
		if(settings.Exists(CProfileSettings::FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING)) 
			SetMenuPreference(PREF_FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING, (settings.GetInt(CProfileSettings::FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING)));

		if(settings.Exists(CProfileSettings::FACEBOOK_UPDATES)) 
			SetMenuPreference(PREF_FACEBOOK_UPDATES, (settings.GetInt(CProfileSettings::FACEBOOK_UPDATES)));

		if(settings.Exists(CProfileSettings::DISPLAY_AUTOSAVE_MODE)) 
			SetMenuPreference(PREF_AUTOSAVE, (settings.GetInt(CProfileSettings::DISPLAY_AUTOSAVE_MODE)));

		if(settings.Exists(CProfileSettings::CONTROLLER_CINEMATIC_SHOOTING)) 
			SetMenuPreference(PREF_CINEMATIC_SHOOTING, (settings.GetInt(CProfileSettings::CONTROLLER_CINEMATIC_SHOOTING)));
		if(settings.Exists(CProfileSettings::CONTROLLER_DUCK_HANDBRAKE)) 
			SetMenuPreference(PREF_ALTERNATE_HANDBRAKE, (settings.GetInt(CProfileSettings::CONTROLLER_DUCK_HANDBRAKE)));
		if(settings.Exists(CProfileSettings::CONTROLLER_DRIVEBY)) 
			SetMenuPreference(PREF_ALTERNATE_DRIVEBY, (settings.GetInt(CProfileSettings::CONTROLLER_DRIVEBY)));
		if(settings.Exists(CProfileSettings::CONTROLLER_SNIPER_ZOOM)) 
			SetMenuPreference(PREF_SNIPER_ZOOM, (settings.GetInt(CProfileSettings::CONTROLLER_SNIPER_ZOOM)));

		if(settings.Exists(CProfileSettings::AUDIO_SFX_LEVEL)) 
			SetMenuPreference(PREF_SFX_VOLUME, (settings.GetInt(CProfileSettings::AUDIO_SFX_LEVEL)));
		if(settings.Exists(CProfileSettings::AUDIO_MUSIC_LEVEL)) 
			SetMenuPreference(PREF_MUSIC_VOLUME, (settings.GetInt(CProfileSettings::AUDIO_MUSIC_LEVEL)));
		if(settings.Exists(CProfileSettings::AUDIO_DIALOGUE_BOOST))
			SetMenuPreference(PREF_DIAG_BOOST, (settings.GetInt(CProfileSettings::AUDIO_DIALOGUE_BOOST)));
		if(settings.Exists(CProfileSettings::AUDIO_SS_FRONT))
			SetMenuPreference(PREF_SS_FRONT, (settings.GetInt(CProfileSettings::AUDIO_SS_FRONT)));
		if(settings.Exists(CProfileSettings::AUDIO_SS_REAR))
			SetMenuPreference(PREF_SS_REAR, (settings.GetInt(CProfileSettings::AUDIO_SS_REAR)));
		if(settings.Exists(CProfileSettings::AUDIO_GPS_SPEECH)) 
			SetMenuPreference(PREF_GPS_SPEECH, (settings.GetInt(CProfileSettings::AUDIO_GPS_SPEECH)));
		if(settings.Exists(CProfileSettings::AUDIO_HIGH_DYNAMIC_RANGE)) 
			SetMenuPreference(PREF_HDR, (settings.GetInt(CProfileSettings::AUDIO_HIGH_DYNAMIC_RANGE)));
		if(settings.Exists(CProfileSettings::AUDIO_SPEAKER_OUTPUT)) 
			SetMenuPreference(PREF_SPEAKER_OUTPUT, (settings.GetInt(CProfileSettings::AUDIO_SPEAKER_OUTPUT)));
		if(settings.Exists(CProfileSettings::AUDIO_INTERACTIVE_MUSIC)) 
			SetMenuPreference(PREF_INTERACTIVE_MUSIC, (settings.GetInt(CProfileSettings::AUDIO_INTERACTIVE_MUSIC)));
		if(settings.Exists(CProfileSettings::AUDIO_CTRL_SPEAKER)) 
			SetMenuPreference(PREF_CTRL_SPEAKER, (settings.GetInt(CProfileSettings::AUDIO_CTRL_SPEAKER)));
		if(settings.Exists(CProfileSettings::AUDIO_CTRL_SPEAKER_HEADPHONE)) 
			SetMenuPreference(PREF_CTRL_SPEAKER_HEADPHONE, (settings.GetInt(CProfileSettings::AUDIO_CTRL_SPEAKER_HEADPHONE)));
		if(settings.Exists(CProfileSettings::AUDIO_CTRL_SPEAKER_VOL)) 
			SetMenuPreference(PREF_CTRL_SPEAKER_VOL, (settings.GetInt(CProfileSettings::AUDIO_CTRL_SPEAKER_VOL)));
		if(settings.Exists(CProfileSettings::AUDIO_PULSE_HEADSET)) 
			SetMenuPreference(PREF_PULSE_HEADSET, (settings.GetInt(CProfileSettings::AUDIO_PULSE_HEADSET)));
		if(settings.Exists(CProfileSettings::AUDIO_MUSIC_LEVEL_IN_MP)) 
			SetMenuPreference(PREF_MUSIC_VOLUME_IN_MP, (settings.GetInt(CProfileSettings::AUDIO_MUSIC_LEVEL_IN_MP)));
		if(settings.Exists(CProfileSettings::AUDIO_VOICE_OUTPUT)) 
			SetMenuPreference(PREF_VOICE_OUTPUT, (settings.GetInt(CProfileSettings::AUDIO_VOICE_OUTPUT)));
		if(settings.Exists(CProfileSettings::VOICE_THRU_SPEAKERS)) 
			SetMenuPreference(PREF_VOICE_SPEAKERS, (settings.GetInt(CProfileSettings::VOICE_THRU_SPEAKERS)));

#if RSG_PC		
		if(settings.Exists(CProfileSettings::AUDIO_UR_PLAYMODE)) 
			SetMenuPreference(PREF_UR_PLAYMODE, (settings.GetInt(CProfileSettings::AUDIO_UR_PLAYMODE)));
		if(settings.Exists(CProfileSettings::AUDIO_UR_AUTOSCAN)) 
			SetMenuPreference(PREF_UR_AUTOSCAN, (settings.GetInt(CProfileSettings::AUDIO_UR_AUTOSCAN)));
		if(settings.Exists(CProfileSettings::AUDIO_MUTE_ON_FOCUS_LOSS)) 
			SetMenuPreference(PREF_AUDIO_MUTE_ON_FOCUS_LOSS, (settings.GetInt(CProfileSettings::AUDIO_MUTE_ON_FOCUS_LOSS)));
#endif // RSG_PC

		if(settings.Exists(CProfileSettings::MP_CAMERA_ZOOM_ON_FOOT)) 
			SetMenuPreference(PREF_MP_CAMERA_ZOOM_ON_FOOT, (settings.GetInt(CProfileSettings::MP_CAMERA_ZOOM_ON_FOOT)));
		if(settings.Exists(CProfileSettings::MP_CAMERA_ZOOM_IN_VEHICLE)) 
			SetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_VEHICLE, (settings.GetInt(CProfileSettings::MP_CAMERA_ZOOM_IN_VEHICLE)));
		if(settings.Exists(CProfileSettings::MP_CAMERA_ZOOM_ON_BIKE)) 
			SetMenuPreference(PREF_MP_CAMERA_ZOOM_ON_BIKE, (settings.GetInt(CProfileSettings::MP_CAMERA_ZOOM_ON_BIKE)));
		if(settings.Exists(CProfileSettings::MP_CAMERA_ZOOM_IN_BOAT)) 
			SetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_BOAT, (settings.GetInt(CProfileSettings::MP_CAMERA_ZOOM_IN_BOAT)));
		if(settings.Exists(CProfileSettings::MP_CAMERA_ZOOM_IN_AIRCRAFT)) 
			SetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_AIRCRAFT, (settings.GetInt(CProfileSettings::MP_CAMERA_ZOOM_IN_AIRCRAFT)));
		if(settings.Exists(CProfileSettings::MP_CAMERA_ZOOM_IN_HELI)) 
			SetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_HELI, (settings.GetInt(CProfileSettings::MP_CAMERA_ZOOM_IN_HELI)));
		if(settings.Exists(CProfileSettings::FPS_VEH_AUTO_CENTER))
			SetMenuPreference(PREF_FPS_VEH_AUTO_CENTER, (settings.GetInt(CProfileSettings::FPS_VEH_AUTO_CENTER)));

#if KEYBOARD_MOUSE_SUPPORT
		// Misc Controls
		if(settings.Exists(CProfileSettings::MOUSE_TYPE))
			SetMenuPreference(PREF_MOUSE_TYPE, (settings.GetInt(CProfileSettings::MOUSE_TYPE)));
		
		if(settings.Exists(CProfileSettings::MOUSE_SUB))
			SetMenuPreference(PREF_MOUSE_SUB, (settings.GetInt(CProfileSettings::MOUSE_SUB)));
		if(settings.Exists(CProfileSettings::MOUSE_DRIVE))
			SetMenuPreference(PREF_MOUSE_DRIVE, (settings.GetInt(CProfileSettings::MOUSE_DRIVE)));
		if(settings.Exists(CProfileSettings::MOUSE_FLY))
			SetMenuPreference(PREF_MOUSE_FLY, (settings.GetInt(CProfileSettings::MOUSE_FLY)));

		if(settings.Exists(CProfileSettings::MOUSE_ON_FOOT_SCALE))
			SetMenuPreference(PREF_MOUSE_ON_FOOT_SCALE, (settings.GetInt(CProfileSettings::MOUSE_ON_FOOT_SCALE)));
		if(settings.Exists(CProfileSettings::MOUSE_DRIVING_SCALE))
			SetMenuPreference(PREF_MOUSE_DRIVING_SCALE, (settings.GetInt(CProfileSettings::MOUSE_DRIVING_SCALE)));
		if(settings.Exists(CProfileSettings::MOUSE_PLANE_SCALE))
			SetMenuPreference(PREF_MOUSE_PLANE_SCALE, (settings.GetInt(CProfileSettings::MOUSE_PLANE_SCALE)));
		if(settings.Exists(CProfileSettings::MOUSE_HELI_SCALE))
			SetMenuPreference(PREF_MOUSE_HELI_SCALE, (settings.GetInt(CProfileSettings::MOUSE_HELI_SCALE)));
		if(settings.Exists(CProfileSettings::MOUSE_SUB_SCALE))
			SetMenuPreference(PREF_MOUSE_SUB_SCALE, (settings.GetInt(CProfileSettings::MOUSE_SUB_SCALE)));
		if(settings.Exists(CProfileSettings::MOUSE_WEIGHT_SCALE))
			SetMenuPreference(PREF_MOUSE_WEIGHT_SCALE, (settings.GetInt(CProfileSettings::MOUSE_WEIGHT_SCALE)));
		if(settings.Exists(CProfileSettings::MOUSE_ACCELERATION))
			SetMenuPreference(PREF_MOUSE_ACCELERATION, (settings.GetInt(CProfileSettings::MOUSE_ACCELERATION)));
		if(settings.Exists(CProfileSettings::KBM_TOGGLE_AIM))
			SetMenuPreference(PREF_KBM_TOGGLE_AIM, (settings.GetInt(CProfileSettings::KBM_TOGGLE_AIM)));
		if(settings.Exists(CProfileSettings::KBM_ALT_VEH_MOUSE_CONTROLS))
			SetMenuPreference(PREF_ALT_VEH_MOUSE_CONTROLS, (settings.GetInt(CProfileSettings::KBM_ALT_VEH_MOUSE_CONTROLS)));
#endif // KEYBOARD_MOUSE_SUPPORT

#if RSG_PC
		// Graphics and video override
		if(settings.Exists(CProfileSettings::PC_GFX_VID_OVERRIDE))
			SetMenuPreference(PREF_GFX_VID_OVERRIDE, (settings.GetInt(CProfileSettings::PC_GFX_VID_OVERRIDE)));

		// Voice Chat
		if(settings.Exists(CProfileSettings::PC_VOICE_ENABLED))
			SetMenuPreference(PREF_VOICE_ENABLE, (settings.GetInt(CProfileSettings::PC_VOICE_ENABLED)));
		if(settings.Exists(CProfileSettings::PC_VOICE_OUTPUT_DEVICE))
			SetMenuPreference(PREF_VOICE_OUTPUT_DEVICE, (settings.GetInt(CProfileSettings::PC_VOICE_OUTPUT_DEVICE)));
		if(settings.Exists(CProfileSettings::PC_VOICE_OUTPUT_VOLUME))
			SetMenuPreference(PREF_VOICE_OUTPUT_VOLUME, (settings.GetInt(CProfileSettings::PC_VOICE_OUTPUT_VOLUME)));
		if(settings.Exists(CProfileSettings::PC_VOICE_TALK_ENABLED))
			SetMenuPreference(PREF_VOICE_TALK_ENABLED, (settings.GetInt(CProfileSettings::PC_VOICE_TALK_ENABLED)));
		if(settings.Exists(CProfileSettings::PC_VOICE_INPUT_DEVICE))
			SetMenuPreference(PREF_VOICE_INPUT_DEVICE, (settings.GetInt(CProfileSettings::PC_VOICE_INPUT_DEVICE)));
		if(settings.Exists(CProfileSettings::PC_VOICE_CHAT_MODE))
			SetMenuPreference(PREF_VOICE_CHAT_MODE, (settings.GetInt(CProfileSettings::PC_VOICE_CHAT_MODE)));
		if(settings.Exists(CProfileSettings::PC_VOICE_MIC_VOLUME))
			SetMenuPreference(PREF_VOICE_MIC_VOLUME, (settings.GetInt(CProfileSettings::PC_VOICE_MIC_VOLUME)));
		if(settings.Exists(CProfileSettings::PC_VOICE_MIC_SENSITIVITY))
			SetMenuPreference(PREF_VOICE_MIC_SENSITIVITY, (settings.GetInt(CProfileSettings::PC_VOICE_MIC_SENSITIVITY)));
		if(settings.Exists(CProfileSettings::PC_VOICE_SOUND_VOLUME))
			SetMenuPreference(PREF_VOICE_SOUND_VOLUME, (settings.GetInt(CProfileSettings::PC_VOICE_SOUND_VOLUME)));
		if(settings.Exists(CProfileSettings::PC_VOICE_MUSIC_VOLUME))
			SetMenuPreference(PREF_VOICE_MUSIC_VOLUME, (settings.GetInt(CProfileSettings::PC_VOICE_MUSIC_VOLUME)));

		Settings gfxSettings = CSettingsManager::GetInstance().GetUISettings();

		// Advanced Video / Graphics Settings
		RestorePreviousGraphicsValues(false, true);
		RestorePreviousAdvancedGraphicsValues(false, true);
#endif // RSG_PC

#if GTA_REPLAY
		if(settings.Exists(CProfileSettings::REPLAY_MODE))
			SetMenuPreference(PREF_REPLAY_MODE, (settings.GetInt(CProfileSettings::REPLAY_MODE)));

		if(settings.Exists(CProfileSettings::REPLAY_MEM_LIMIT))
			SetMenuPreference(PREF_REPLAY_MEM_LIMIT, (settings.GetInt(CProfileSettings::REPLAY_MEM_LIMIT)));

		if(settings.Exists(CProfileSettings::REPLAY_AUTO_RESUME_RECORDING))
			SetMenuPreference(PREF_REPLAY_AUTO_RESUME_RECORDING, (settings.GetInt(CProfileSettings::REPLAY_AUTO_RESUME_RECORDING)));

		if(settings.Exists(CProfileSettings::REPLAY_AUTO_SAVE_RECORDING))
			SetMenuPreference(PREF_REPLAY_AUTO_SAVE_RECORDING, (settings.GetInt(CProfileSettings::REPLAY_AUTO_SAVE_RECORDING)));

		if(settings.Exists(CProfileSettings::VIDEO_UPLOAD_PRIVACY))
			SetMenuPreference(PREF_VIDEO_UPLOAD_PRIVACY, (settings.GetInt(CProfileSettings::VIDEO_UPLOAD_PRIVACY)));

#if RSG_PC
		if(settings.Exists(CProfileSettings::VIDEO_EXPORT_GRAPHICS_UPGRADE))
			SetMenuPreference(PREF_ROCKSTAR_EDITOR_EXPORT_GRAPHICS_UPGRADE, (settings.GetInt(CProfileSettings::VIDEO_EXPORT_GRAPHICS_UPGRADE)));
#endif

		if(settings.Exists(CProfileSettings::ROCKSTAR_EDITOR_TOOLTIP)) 
			SetMenuPreference(PREF_ROCKSTAR_EDITOR_TOOLTIP, (settings.GetInt(CProfileSettings::ROCKSTAR_EDITOR_TOOLTIP)));
#endif // GTA_REPLAY

#if !__FINAL
		if( GetMenuPreference(PREF_CURRENT_LANGUAGE) != GetMenuPreference(PREF_PREVIOUS_LANGUAGE) )
		{
			SetMenuPreference(PREF_PREVIOUS_LANGUAGE, GetMenuPreference(PREF_CURRENT_LANGUAGE));
			if(!CLoadingScreens::IsInitialLoadingScreenActive())
			{
				gRenderThreadInterface.Flush();
				TheText.ReloadAfterLanguageChange();
			}
		}
#else
		SetMenuPreference(PREF_PREVIOUS_LANGUAGE, GetMenuPreference(PREF_CURRENT_LANGUAGE));
#endif

		// default to source being from profile change, but if we are in the process of changing session 
		// set to init as we don't want to update components that have not been initialised
		UpdatePrefsSource source = UPDATE_PREFS_PROFILE_CHANGE;
		if(CGame::IsChangingSessionState())
			source = UPDATE_PREFS_INIT;

		for (s32 i = 0; i < MAX_MENU_PREFERENCES; i++)
		{
			if (i != PREF_SAFEZONE_SIZE || bSafeZoneChanged)  // if not safezone pref, or if it is, only if safezone has changed
			{
				SetValueBasedOnPreference(i, source);
			}
		}

		UpdateDisplayConfig();

		CScaleformMovieWrapper* pSocialButtons = SocialClubMenu::GetButtonWrapper();
		// TECHNICALLY not needed IF the pause menu button is already active, since they (currently) share the same movie.
		// But really, they shouldn't be.
		if( pSocialButtons && !pSocialButtons->IsFree() )
		{
			CNewHud::UpdateDisplayConfig( pSocialButtons->GetMovieID(), SF_BASE_CLASS_PAUSEMENU);
		}


	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::UpdateProfileFromMenuOptions()
// PURPOSE:	updates the player profile from the menu option preferences
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::UpdateProfileFromMenuOptions(bool languageReload)
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	// Don't check to see if you have valid settings already. You need to be set this values still even if someone hasn't a loaded preference file 
	//if(settings.AreSettingsValid())
	{
		settings.Set(CProfileSettings::CONTROLLER_VIBRATION, GetMenuPreference(PREF_VIBRATION));
		settings.Set(CProfileSettings::AXIS_INVERSION, GetMenuPreference(PREF_INVERT_LOOK));
		settings.Set(CProfileSettings::TARGETING_MODE, GetMenuPreference(PREF_TARGET_CONFIG));
		settings.Set(CProfileSettings::CONTROLLER_AIM_SENSITIVITY, GetMenuPreference(PREF_CONTROLLER_SENSITIVITY));
		settings.Set(CProfileSettings::LOOK_AROUND_SENSITIVITY, GetMenuPreference(PREF_LOOK_AROUND_SENSITIVITY));
#if KEYBOARD_MOUSE_SUPPORT
		settings.Set(CProfileSettings::MOUSE_INVERSION, GetMenuPreference(PREF_INVERT_MOUSE));
		settings.Set(CProfileSettings::MOUSE_INVERSION_FLYING, GetMenuPreference(PREF_INVERT_MOUSE_FLYING));
		settings.Set(CProfileSettings::MOUSE_INVERSION_SUB, GetMenuPreference(PREF_INVERT_MOUSE_SUB));
		settings.Set(CProfileSettings::MOUSE_SWAP_ROLL_YAW_FLYING, GetMenuPreference(PREF_SWAP_ROLL_YAW_MOUSE_FLYING));

		settings.Set(CProfileSettings::MOUSE_AUTOCENTER_CAR, GetMenuPreference(PREF_MOUSE_AUTOCENTER_CAR));
		settings.Set(CProfileSettings::MOUSE_AUTOCENTER_BIKE, GetMenuPreference(PREF_MOUSE_AUTOCENTER_BIKE));
		settings.Set(CProfileSettings::MOUSE_AUTOCENTER_PLANE, GetMenuPreference(PREF_MOUSE_AUTOCENTER_PLANE));
#endif // KEYBOARD_MOUSE_SUPPORT

#if LIGHT_EFFECTS_SUPPORT
		settings.Set(CProfileSettings::CONTROLLER_LIGHT_EFFECT, GetMenuPreference(PREF_CONTROLLER_LIGHT_EFFECT));
#endif // LIGHT_EFFECTS_SUPPORT

		// dont load in the controller config setting as we want it to use standard all the time now
//		settings.Set(CProfileSettings::CONTROLLER_CONFIG, GetMenuOptionValue(PREF_CONTROL_CONFIG));

#if __6AXISACTIVE
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_HELI, GetMenuPreference(PREF_SIXAXIS_HELI));
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_BIKE, GetMenuPreference(PREF_SIXAXIS_BIKE));
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_BOAT, GetMenuPreference(PREF_SIXAXIS_BOAT));
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_MISSION, GetMenuPreference(PREF_SIXAXIS_CALIBRATION));
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_RELOAD, GetMenuPreference(PREF_SIXAXIS_RELOAD));
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_ACTIVITY, GetMenuPreference(PREF_SIXAXIS_ACTIVITY));
		settings.Set(CProfileSettings::CONTROLLER_SIXAXIS_AFTERTOUCH, GetMenuPreference(PREF_SIXAXIS_AFTERTOUCH));
#endif // __6AXISACTIVE
		settings.Set(CProfileSettings::CONTROLLER_CONTROL_CONFIG, GetMenuPreference(PREF_CONTROL_CONFIG));
		settings.Set(CProfileSettings::CONTROLLER_CONTROL_CONFIG_FPS, GetMenuPreference(PREF_CONTROL_CONFIG_FPS));

		settings.Set(CProfileSettings::DISPLAY_GAMMA, GetMenuPreference(PREF_GAMMA));
		settings.Set(CProfileSettings::DISPLAY_CAMERA_HEIGHT, GetMenuPreference(PREF_CAMERA_HEIGHT));
		settings.Set(CProfileSettings::DISPLAY_SUBTITLES, GetMenuPreference(PREF_SUBTITLES));
		settings.Set(CProfileSettings::DISPLAY_DOF, GetMenuPreference(PREF_DOF));
		settings.Set(CProfileSettings::DISPLAY_RADAR_MODE, GetMenuPreference(PREF_RADAR_MODE));
		settings.Set(CProfileSettings::DISPLAY_HUD_MODE, GetMenuPreference(PREF_DISPLAY_HUD));
		settings.Set(CProfileSettings::DISPLAY_SKFX, GetMenuPreference(PREF_SKFX));
		settings.Set(CProfileSettings::DISPLAY_RETICULE, GetMenuPreference(PREF_RETICULE));
		settings.Set(CProfileSettings::DISPLAY_RETICULE_SIZE, GetMenuPreference(PREF_RETICULE_SIZE));
		settings.Set(CProfileSettings::DISPLAY_GPS, GetMenuPreference(PREF_DISPLAY_GPS));
		settings.Set(CProfileSettings::DISPLAY_SAFEZONE_SIZE, SAFEZONE_SLIDER_MAX - GetMenuPreference(PREF_SAFEZONE_SIZE));
		settings.Set(CProfileSettings::DISPLAY_BIG_RADAR, GetMenuPreference(PREF_BIG_RADAR));
		settings.Set(CProfileSettings::DISPLAY_BIG_RADAR_NAMES, GetMenuPreference(PREF_BIG_RADAR_NAMES));
		settings.Set(CProfileSettings::DISPLAY_TEXT_CHAT, GetMenuPreference(PREF_SHOW_TEXT_CHAT));
		settings.Set(CProfileSettings::MEASUREMENT_SYSTEM, GetMenuPreference(PREF_MEASUREMENT_SYSTEM));

		settings.Set(CProfileSettings::START_UP_FLOW, GetMenuPreference(PREF_STARTUP_FLOW));
		settings.Set(CProfileSettings::LANDING_PAGE, GetMenuPreference(PREF_LANDING_PAGE));

		settings.Set(CProfileSettings::FEED_PHONE, GetMenuPreference(PREF_FEED_PHONE));
		settings.Set(CProfileSettings::FEED_STATS, GetMenuPreference(PREF_FEED_STATS));
		settings.Set(CProfileSettings::FEED_CREW, GetMenuPreference(PREF_FEED_CREW));
		settings.Set(CProfileSettings::FEED_FRIENDS, GetMenuPreference(PREF_FEED_FRIENDS));
		settings.Set(CProfileSettings::FEED_SOCIAL, GetMenuPreference(PREF_FEED_SOCIAL));
		settings.Set(CProfileSettings::FEED_STORE, GetMenuPreference(PREF_FEED_STORE));
		settings.Set(CProfileSettings::FEED_TOOPTIP, GetMenuPreference(PREF_FEED_TOOLTIP));
		settings.Set(CProfileSettings::FEED_DELAY, GetMenuPreference(PREF_FEED_DELAY));
		settings.Set(CProfileSettings::FACEBOOK_UPDATES, GetMenuPreference(PREF_FACEBOOK_UPDATES));
		
		// If system language is different then store it in the profile settings
		if(GetMenuPreference(PREF_SYSTEM_LANGUAGE) != GetMenuPreference(PREF_CURRENT_LANGUAGE))
		{
			settings.Set(CProfileSettings::DISPLAY_LANGUAGE, GetMenuPreference(PREF_CURRENT_LANGUAGE));
			settings.Set(CProfileSettings::NEW_DISPLAY_LANGUAGE, GetMenuPreference(PREF_CURRENT_LANGUAGE));
		}
		else
			settings.Clear(CProfileSettings::DISPLAY_LANGUAGE);

		settings.Set(CProfileSettings::DISPLAY_AUTOSAVE_MODE, GetMenuPreference(PREF_AUTOSAVE));
		settings.Set(CProfileSettings::CONTROLLER_CINEMATIC_SHOOTING, GetMenuPreference(PREF_CINEMATIC_SHOOTING));
		settings.Set(CProfileSettings::CONTROLLER_DUCK_HANDBRAKE, GetMenuPreference(PREF_ALTERNATE_HANDBRAKE));
		settings.Set(CProfileSettings::CONTROLLER_DRIVEBY, GetMenuPreference(PREF_ALTERNATE_DRIVEBY));
		settings.Set(CProfileSettings::CONTROLLER_SNIPER_ZOOM, GetMenuPreference(PREF_SNIPER_ZOOM));
		settings.Set(CProfileSettings::FPS_DEFAULT_AIM_TYPE, GetMenuPreference(PREF_FPS_DEFAULT_AIM_TYPE));

		settings.Set(CProfileSettings::AUDIO_SFX_LEVEL, GetMenuPreference(PREF_SFX_VOLUME));
		settings.Set(CProfileSettings::AUDIO_MUSIC_LEVEL, GetMenuPreference(PREF_MUSIC_VOLUME));
		settings.Set(CProfileSettings::AUDIO_DIALOGUE_BOOST, GetMenuPreference(PREF_DIAG_BOOST));
		settings.Set(CProfileSettings::AUDIO_SS_FRONT, GetMenuPreference(PREF_SS_FRONT));
		settings.Set(CProfileSettings::AUDIO_SS_REAR, GetMenuPreference(PREF_SS_REAR));
		settings.Set(CProfileSettings::AUDIO_GPS_SPEECH, GetMenuPreference(PREF_GPS_SPEECH));
		settings.Set(CProfileSettings::AUDIO_HIGH_DYNAMIC_RANGE, GetMenuPreference(PREF_HDR));
		settings.Set(CProfileSettings::AUDIO_SPEAKER_OUTPUT, GetMenuPreference(PREF_SPEAKER_OUTPUT));
		settings.Set(CProfileSettings::AUDIO_INTERACTIVE_MUSIC, GetMenuPreference(PREF_INTERACTIVE_MUSIC));
		settings.Set(CProfileSettings::AUDIO_MUSIC_LEVEL_IN_MP, GetMenuPreference(PREF_MUSIC_VOLUME_IN_MP));
		settings.Set(CProfileSettings::AUDIO_VOICE_OUTPUT, GetMenuPreference(PREF_VOICE_OUTPUT));
		settings.Set(CProfileSettings::VOICE_THRU_SPEAKERS, GetMenuPreference(PREF_VOICE_SPEAKERS));
		settings.Set(CProfileSettings::AUDIO_CTRL_SPEAKER, GetMenuPreference(PREF_CTRL_SPEAKER));
		settings.Set(CProfileSettings::AUDIO_CTRL_SPEAKER_VOL, GetMenuPreference(PREF_CTRL_SPEAKER_VOL));
		settings.Set(CProfileSettings::AUDIO_CTRL_SPEAKER_HEADPHONE, GetMenuPreference(PREF_CTRL_SPEAKER_HEADPHONE));
		settings.Set(CProfileSettings::AUDIO_PULSE_HEADSET, GetMenuPreference(PREF_PULSE_HEADSET));

#if RSG_PC
		settings.Set(CProfileSettings::AUDIO_UR_AUTOSCAN, GetMenuPreference(PREF_UR_AUTOSCAN));
		settings.Set(CProfileSettings::AUDIO_UR_PLAYMODE, GetMenuPreference(PREF_UR_PLAYMODE));
		settings.Set(CProfileSettings::AUDIO_MUTE_ON_FOCUS_LOSS, GetMenuPreference(PREF_AUDIO_MUTE_ON_FOCUS_LOSS));
#endif // RSG_PC

		settings.Set(CProfileSettings::FPS_PERSISTANT_VIEW, GetMenuPreference(PREF_FPS_PERSISTANT_VIEW));
		settings.Set(CProfileSettings::FPS_FIELD_OF_VIEW, GetMenuPreference(PREF_FPS_FIELD_OF_VIEW));
		settings.Set(CProfileSettings::FPS_LOOK_SENSITIVITY, GetMenuPreference(PREF_FPS_LOOK_SENSITIVITY));
		settings.Set(CProfileSettings::FPS_AIM_SENSITIVITY, GetMenuPreference(PREF_FPS_AIM_SENSITIVITY));
		settings.Set(CProfileSettings::FPS_AIM_DEADZONE, GetMenuPreference(PREF_FPS_AIM_DEADZONE));
		settings.Set(CProfileSettings::FPS_AIM_ACCELERATION, GetMenuPreference(PREF_FPS_AIM_ACCELERATION));
		settings.Set(CProfileSettings::AIM_DEADZONE, GetMenuPreference(PREF_AIM_DEADZONE));
		settings.Set(CProfileSettings::AIM_ACCELERATION, GetMenuPreference(PREF_AIM_ACCELERATION));
		settings.Set(CProfileSettings::FPS_AUTO_LEVEL, GetMenuPreference(PREF_FPS_AUTO_LEVEL));

		settings.Set(CProfileSettings::FPS_RAGDOLL, GetMenuPreference(PREF_FPS_RAGDOLL));
		settings.Set(CProfileSettings::FPS_COMBATROLL, GetMenuPreference(PREF_FPS_COMBATROLL));
		settings.Set(CProfileSettings::FPS_HEADBOB, GetMenuPreference(PREF_FPS_HEADBOB));
		settings.Set(CProfileSettings::FPS_THIRD_PERSON_COVER, GetMenuPreference(PREF_FPS_THIRD_PERSON_COVER));
		settings.Set(CProfileSettings::HOOD_CAMERA, GetMenuPreference(PREF_HOOD_CAMERA));
		settings.Set(CProfileSettings::FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING, GetMenuPreference(PREF_FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING));

		settings.Set(CProfileSettings::MP_CAMERA_ZOOM_ON_FOOT, GetMenuPreference(PREF_MP_CAMERA_ZOOM_ON_FOOT));
		settings.Set(CProfileSettings::MP_CAMERA_ZOOM_IN_VEHICLE, GetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_VEHICLE));
		settings.Set(CProfileSettings::MP_CAMERA_ZOOM_ON_BIKE, GetMenuPreference(PREF_MP_CAMERA_ZOOM_ON_BIKE));
		settings.Set(CProfileSettings::MP_CAMERA_ZOOM_IN_BOAT, GetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_BOAT));
		settings.Set(CProfileSettings::MP_CAMERA_ZOOM_IN_AIRCRAFT, GetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_AIRCRAFT));
		settings.Set(CProfileSettings::MP_CAMERA_ZOOM_IN_HELI, GetMenuPreference(PREF_MP_CAMERA_ZOOM_IN_HELI));
		settings.Set(CProfileSettings::FPS_VEH_AUTO_CENTER, GetMenuPreference(PREF_FPS_VEH_AUTO_CENTER));


#if KEYBOARD_MOUSE_SUPPORT
		settings.Set(CProfileSettings::MOUSE_TYPE, GetMenuPreference(PREF_MOUSE_TYPE));
		settings.Set(CProfileSettings::MOUSE_SUB, GetMenuPreference(PREF_MOUSE_SUB));
		settings.Set(CProfileSettings::MOUSE_DRIVE, GetMenuPreference(PREF_MOUSE_DRIVE));
		settings.Set(CProfileSettings::MOUSE_FLY, GetMenuPreference(PREF_MOUSE_FLY));
		settings.Set(CProfileSettings::MOUSE_ON_FOOT_SCALE, GetMenuPreference(PREF_MOUSE_ON_FOOT_SCALE));
		settings.Set(CProfileSettings::MOUSE_DRIVING_SCALE, GetMenuPreference(PREF_MOUSE_DRIVING_SCALE));
		settings.Set(CProfileSettings::MOUSE_PLANE_SCALE, GetMenuPreference(PREF_MOUSE_PLANE_SCALE));
		settings.Set(CProfileSettings::MOUSE_HELI_SCALE, GetMenuPreference(PREF_MOUSE_HELI_SCALE));
		settings.Set(CProfileSettings::MOUSE_SUB_SCALE, GetMenuPreference(PREF_MOUSE_SUB_SCALE));
		settings.Set(CProfileSettings::MOUSE_WEIGHT_SCALE, GetMenuPreference(PREF_MOUSE_WEIGHT_SCALE));
		settings.Set(CProfileSettings::MOUSE_ACCELERATION, GetMenuPreference(PREF_MOUSE_ACCELERATION));
		settings.Set(CProfileSettings::KBM_TOGGLE_AIM, GetMenuPreference(PREF_KBM_TOGGLE_AIM));
		settings.Set(CProfileSettings::KBM_ALT_VEH_MOUSE_CONTROLS, GetMenuPreference(PREF_ALT_VEH_MOUSE_CONTROLS));
#endif // KEYBOARD_MOUSE_SUPPORT

#if RSG_PC
		settings.Set(CProfileSettings::PC_VOICE_ENABLED, GetMenuPreference(PREF_VOICE_ENABLE));
		settings.Set(CProfileSettings::PC_VOICE_OUTPUT_DEVICE, GetMenuPreference(PREF_VOICE_OUTPUT_DEVICE));
		settings.Set(CProfileSettings::PC_VOICE_OUTPUT_VOLUME, GetMenuPreference(PREF_VOICE_OUTPUT_VOLUME));
		settings.Set(CProfileSettings::PC_VOICE_TALK_ENABLED, GetMenuPreference(PREF_VOICE_TALK_ENABLED));
		settings.Set(CProfileSettings::PC_VOICE_INPUT_DEVICE, GetMenuPreference(PREF_VOICE_INPUT_DEVICE));
		settings.Set(CProfileSettings::PC_VOICE_CHAT_MODE, GetMenuPreference(PREF_VOICE_CHAT_MODE));
		settings.Set(CProfileSettings::PC_VOICE_MIC_VOLUME, GetMenuPreference(PREF_VOICE_MIC_VOLUME));
		settings.Set(CProfileSettings::PC_VOICE_MIC_SENSITIVITY, GetMenuPreference(PREF_VOICE_MIC_SENSITIVITY));
		settings.Set(CProfileSettings::PC_VOICE_SOUND_VOLUME, GetMenuPreference(PREF_VOICE_SOUND_VOLUME));
		settings.Set(CProfileSettings::PC_VOICE_MUSIC_VOLUME, GetMenuPreference(PREF_VOICE_MUSIC_VOLUME));

		settings.Set(CProfileSettings::PC_GFX_VID_OVERRIDE, GetMenuPreference(PREF_GFX_VID_OVERRIDE));
#endif // RSG_PC

#if GTA_REPLAY
		settings.Set(CProfileSettings::REPLAY_MODE, GetMenuPreference(PREF_REPLAY_MODE));
		settings.Set(CProfileSettings::REPLAY_MEM_LIMIT, GetMenuPreference(PREF_REPLAY_MEM_LIMIT));
		settings.Set(CProfileSettings::REPLAY_AUTO_RESUME_RECORDING, GetMenuPreference(PREF_REPLAY_AUTO_RESUME_RECORDING));
		settings.Set(CProfileSettings::REPLAY_AUTO_SAVE_RECORDING, GetMenuPreference(PREF_REPLAY_AUTO_SAVE_RECORDING));
		settings.Set(CProfileSettings::VIDEO_UPLOAD_PRIVACY, GetMenuPreference(PREF_VIDEO_UPLOAD_PRIVACY));
		settings.Set(CProfileSettings::ROCKSTAR_EDITOR_TOOLTIP, GetMenuPreference(PREF_ROCKSTAR_EDITOR_TOOLTIP));
#if RSG_PC
		settings.Set(CProfileSettings::VIDEO_EXPORT_GRAPHICS_UPGRADE, GetMenuPreference(PREF_ROCKSTAR_EDITOR_EXPORT_GRAPHICS_UPGRADE));
#endif
#endif // GTA_REPLAY

		settings.Write();

		// move language change to here as loading the language files is causing us to run out of memory, so don't want to do it everytime 
		// we change the selection
		if(GetMenuPreference(PREF_CURRENT_LANGUAGE) != GetMenuPreference(PREF_PREVIOUS_LANGUAGE) && languageReload)
		{
			SetMenuPreference(PREF_PREVIOUS_LANGUAGE, GetMenuPreference(PREF_CURRENT_LANGUAGE));
			gRenderThreadInterface.Flush();
			TheText.ReloadAfterLanguageChange();
		}

	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	GetLanguageFromSystemLanguage
// PURPOSE:	Returns the language to use based on the system language identifier
/////////////////////////////////////////////////////////////////////////////////////
u32 CPauseMenu::GetLanguageFromSystemLanguage()
{
	sysLanguage language = LANGUAGE_UNDEFINED;

#if __BANK
	if( CLocalisation::m_eForceLanguage != MAX_LANGUAGES )
		return CLocalisation::m_eForceLanguage;
#endif

	language = g_SysService.GetSystemLanguage();
	return language;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	SetValuesBasedOnCurrentSettings
// PURPOSE:	goes through all preferences and apply the settings
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetValuesBasedOnCurrentSettings(CPauseMenu::UpdatePrefsSource source)
{
	for (s32 i = 0; i < MAX_MENU_PREFERENCES; i++)
	{
		SetValueBasedOnPreference(i, source);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	SetValuesBasedOnPreference
// PURPOSE:	sets values based on what preference has just changed this frame
//			values changed in here should only be GENERIC ones; any game-specific
//			prefs should be changed in CPauseMenu::SetGameSpecificValues()
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetValueBasedOnPreference(s32 iPrefChanged, CPauseMenu::UpdatePrefsSource source, int iPreviousValue)
{
	switch ((eMenuPref)iPrefChanged)
	{
		case PREF_VIBRATION:
		{
			if (source == UPDATE_PREFS_FROM_MENU)  // dont do this if we are defaulting the controls
			{
				CControlMgr::AllowPadsShaking();
				CControlMgr::StartPlayerPadShakeByIntensity(sm_RumbleDuration,1.0f);

				// We add 500 ms as there is a delay between the request and the update. This does not need to be precise but needs
				// to be long enough to feel the rumble.
				sm_EndAllowRumbleTime = fwTimer::GetSystemTimeInMilliseconds() + sm_RumbleDuration + 500;
			}
			break;
		}

		case PREF_SKFX:
		{
			s32 newSetting =  CPauseMenu::GetMenuPreference(PREF_SKFX);
			PostFX::TogglePedKillOverlay(newSetting == TRUE);
			break;
		}

		case PREF_INVERT_LOOK:
			{
				CControlMgr::SetDefaultLookInversions();

#if KEYBOARD_MOUSE_SUPPORT
				s32 newSetting =  CPauseMenu::GetMenuPreference(PREF_INVERT_LOOK);
				if(sm_LastLookInvertMode != newSetting)
				{
					sm_LastLookInvertMode = newSetting;

					CControlMgr::ReInitControls();

					// Disable the controls for a short while otherwise the option cycles too fast and the controls are re-set so they
					// are marked as just pressed again.
					StartInputCooldown(sm_uDisableInputDuration);
				}
#endif // KEYBOARD_MOUSE_SUPPORT
			}
			break;

		case PREF_RETICULE_SIZE:
			{
				if (CNewHud::IsHudComponentActive(NEW_HUD))  // fix for 2033526 - dont invoke if not active
				{
					if (CHudTools::BeginHudScaleformMethod(NEW_HUD_RETICLE, "SET_RETICULE_BOOST"))
					{
						CScaleformMgr::AddParamFloat(1.0f + ((float)GetMenuPreference(PREF_RETICULE_SIZE) / 10.0f));
						CScaleformMgr::EndMethod();
					}
				}
				break;
			}

		case PREF_GPS_SPEECH:
			{
				CGps::EnableAudio(CPauseMenu::GetMenuPreference(PREF_GPS_SPEECH));
				break;
			}

		case PREF_SAFEZONE_SIZE:
			{
				if(source != UPDATE_PREFS_INIT)
				{
					if(source == UPDATE_PREFS_FROM_MENU)
						CNewHud::EnableSafeZoneRender(2000);

					CProfileSettings::GetInstance().Set(CProfileSettings::DISPLAY_SAFEZONE_SIZE, SAFEZONE_SLIDER_MAX - GetMenuPreference(PREF_SAFEZONE_SIZE));
					
					UpdateDisplayConfig();
					CNewHud::UpdateHudDisplayConfig();
					CGameStreamMgr::GetGameStream()->SetDisplayConfigChanged();
					CMiniMap::UpdateDisplayConfig();

					if (CHudMarkerManager::Get())
					{
						CHudMarkerManager::Get()->UpdateMovieConfig();
					}

#if RSG_PC
					// Update Landing Screen
					if(SLegacyLandingScreen::IsInstantiated())
					{
						SLegacyLandingScreen::GetInstance().UpdateDisplayConfig();

						// Update Transition News (Displays on Landing Screen)
						if(CNetworkTransitionNewsController::Get().IsActive())
						{
							CNetworkTransitionNewsController::Get().UpdateDisplayConfig();
						}

						CLoadingScreens::UpdateDisplayConfig();
					}
					
					if(SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsMovieActive())
					{
						SMultiplayerChat::GetInstance().UpdateDisplayConfig();
					}
#endif // RSG_PC

#if DEBUG_PAD_INPUT
					CDebug::UpdateDebugPadMoviePositions();
#endif // #if DEBUG_PAD_INPUT
				}
				break;
			}

 		case PREF_GAMMA:
		{
 			// amount we adjust these values by:
 			#define DISPLAY_PREFERENCES_CALIBRATED_GAMMA_MIN (1.4f)
 			#define DISPLAY_PREFERENCES_CALIBRATED_GAMMA_MAX (3.0f)
 
 			float fGamma = rage::Max(0.0f, ((float)CPauseMenu::GetMenuPreference(PREF_GAMMA)) / 30.0f);
 			fGamma = DISPLAY_PREFERENCES_CALIBRATED_GAMMA_MIN + (DISPLAY_PREFERENCES_CALIBRATED_GAMMA_MAX - DISPLAY_PREFERENCES_CALIBRATED_GAMMA_MIN) * fGamma;
 
			if (!PARAM_setGamma.Get(fGamma))
			{
				PostFX::SetGammaFrontEnd(fGamma);
			}

 			if (!NetworkInterface::IsGameInProgress())
 			{
 				if (source == UPDATE_PREFS_DEFAULTING)
 				{
 					PostFX::Update();  // update the postfx so we see our changes instantly even though the game is paused
 				}
 			}
 			break;
 		}

		case PREF_RADIO_STATION:
			{
				if (source == UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
				{
					s32 stationId = CPauseMenu::GetMenuPreference(PREF_RADIO_STATION);
					if(stationId == -1 )
					{
						audDisplayf("Pause menu switching off radio");
						g_RadioAudioEntity.SwitchOffRadio();
					}
					else
					{
						audDisplayf("Pause menu tuning radio to index %d", stationId);
						g_RadioAudioEntity.RetuneToStation(audRadioStation::GetStation(stationId));
					}
				}
				// RADIOTODO: change this to save/restore the station hash
			}
			break;
		case PREF_FACEBOOK_UPDATES:
			{
#if RL_FACEBOOK_ENABLED
				if (!CLiveManager::GetFacebookMgr().IsBusy() && CLiveManager::GetFacebookMgr().IsProfileSettingEnabled())
				{
					CLiveManager::GetFacebookMgr().TryPostBoughtGame();
				}
#endif // RL_FACEBOOK_ENABLED
			}
			break;
		case PREF_CTRL_SPEAKER:
			{
				audDisplayf("Setting context: %d", CPauseMenu::GetMenuPreference(PREF_CTRL_SPEAKER));
				SUIContexts::SetActive(UIATSTRINGHASH("CTRL_SPEAKER",0x1DA4291), CPauseMenu::GetMenuPreference(PREF_CTRL_SPEAKER) != 0);
				CPauseMenu::RedrawInstructionalButtons();
			}
			break;
		case PREF_SPEAKER_OUTPUT:
			if(source== UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
			{
// 				CMenuScreen& audioData = GetScreenData( MENU_UNIQUE_ID_SETTINGS_AUDIO );
// 				int onScreenIndex = 0;

				GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_AUDIO, true);
// 				audioData.FindItemIndex(PREF_SPEAKER_OUTPUT, &onScreenIndex);
// 				CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_MIDDLE, onScreenIndex);
			}
			break;
		case PREF_CTRL_SPEAKER_HEADPHONE:
			{
				audDisplayf("Setting context: %d", CPauseMenu::GetMenuPreference(PREF_CTRL_SPEAKER_HEADPHONE));
				SUIContexts::SetActive(UIATSTRINGHASH("CTRL_SPEAKER",0x1DA4291), CPauseMenu::GetMenuPreference(PREF_CTRL_SPEAKER_HEADPHONE) != 0);
				CPauseMenu::RedrawInstructionalButtons();
			}
			break;
		case PREF_PULSE_HEADSET:
			if(source== UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
			{
				s32 iEnabled = CPauseMenu::GetMenuPreference(PREF_PULSE_HEADSET);
				sm_bWaitingOnPulseWarning = (iEnabled == TRUE);
			}
			break;
		case PREF_TARGET_CONFIG:
			if(source== UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
			{
				bool bEnabled = CPedTargetEvaluator::IsAssistedAim(CPauseMenu::GetMenuPreference(PREF_TARGET_CONFIG)) &&
								CPedTargetEvaluator::IsFreeAim(iPreviousValue) && RSG_PC;
				sm_bWaitingOnAimWarning = bEnabled;
			}
			break;
		case PREF_ALTERNATE_HANDBRAKE:
			{
				s32 newSetting =  CPauseMenu::GetMenuPreference(PREF_ALTERNATE_HANDBRAKE);

				if(newSetting != sm_LastDuckHandBrakeMode)
				{
					sm_LastDuckHandBrakeMode = newSetting;

					CControlMgr::ReInitControls();

					// Disable the controls for a short while otherwise the option cycles too fast and the controls are re-set so they
					// are marked as just pressed again.
					StartInputCooldown(sm_uDisableInputDuration);
					CControllerLabelMgr::AdjustLabels();
				}
			}
			break;
		case PREF_SNIPER_ZOOM:
		case PREF_ALTERNATE_DRIVEBY:
			if(source== UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
			{
				CControllerLabelMgr::AdjustLabels();
			}
			break;
		case PREF_CONTROL_CONFIG:
		case PREF_CONTROL_CONFIG_FPS:
			{
				s32 newTpsSetting =  CPauseMenu::GetMenuPreference(PREF_CONTROL_CONFIG);
				s32 newFpsSetting =  CPauseMenu::GetMenuPreference(PREF_CONTROL_CONFIG_FPS);

				if(newTpsSetting != sm_LastTpsControlsMode || newFpsSetting != sm_LastFpsControlsMode)
				{
					sm_LastTpsControlsMode = newTpsSetting;
					sm_LastFpsControlsMode = newFpsSetting;

					sm_LastControlConfigChanged = (eMenuPref)iPrefChanged; 

					CControlMgr::ReInitControls();

					// Disable the controls for a short while otherwise the option cycles too fast and the controls are re-set so they
					// are marked as just pressed again.
					StartInputCooldown(sm_uDisableInputDuration);

					int iPrefValueForContext = GetMenuPreference(PREF_CONTROLS_CONTEXT);
					const int TPS_CONTROLS_END_INDEX = 2;
					const int FPS_CONTROLS_END_INDEX = 5;

					if( (sm_LastControlConfigChanged == PREF_CONTROL_CONFIG && iPrefValueForContext <= TPS_CONTROLS_END_INDEX) ||
						(sm_LastControlConfigChanged == PREF_CONTROL_CONFIG_FPS && iPrefValueForContext > TPS_CONTROLS_END_INDEX && iPrefValueForContext <= FPS_CONTROLS_END_INDEX))
					{
						CControllerLabelMgr::SetLabelScheme(GetControllerContext(), GetMenuPreference(sm_LastControlConfigChanged));
					}
				}
			}
			break;

		case PREF_CONTROLS_CONTEXT:
			{
				int iPrefValue = GetMenuPreference(PREF_CONTROLS_CONTEXT);
				u32 iModePref = iPrefValue < 3 ? PREF_CONTROL_CONFIG : PREF_CONTROL_CONFIG_FPS;

				CControllerLabelMgr::SetLabelScheme(GetControllerContext(), GetMenuPreference(iModePref));
			}
			break;

#if LIGHT_EFFECTS_SUPPORT
		case PREF_CONTROLLER_LIGHT_EFFECT:
			{
				if(CPauseMenu::GetMenuPreference(PREF_CONTROLLER_LIGHT_EFFECT))
				{
					CControlMgr::GetPlayerMappingControl().SetLightEffectEnabled(true);
				}
				else
				{
					CControlMgr::GetPlayerMappingControl().SetLightEffectEnabled(false);
				}
			}
			break;

#endif // LIGHT_EFFECTS_SUPPORT

#if RSG_PC
		case PREF_VID_SCREEN_TYPE:
		case PREF_VID_RESOLUTION:
		case PREF_VID_REFRESH:
		case PREF_VID_ADAPTER:
		case PREF_VID_MONITOR:
		case PREF_VID_VSYNC:
		case PREF_VID_ASPECT:
		case PREF_VID_STEREO:
		case PREF_VID_STEREO_CONVERGENCE:
		//case PREF_VID_STEREO_SEPARATION:
		case PREF_VID_PAUSE_ON_FOCUS_LOSS:
		case PREF_GFX_DXVERSION:
		case PREF_GFX_CITY_DENSITY:
		case PREF_GFX_POP_VARIETY:
		case PREF_GFX_TEXTURE_QUALITY:
		case PREF_GFX_SHADER_QUALITY:
		case PREF_GFX_SHADOW_QUALITY:
		case PREF_GFX_REFLECTION_QUALITY:
		case PREF_GFX_REFLECTION_MSAA:
		case PREF_GFX_WATER_QUALITY:
		case PREF_GFX_PARTICLES_QUALITY:
		case PREF_GFX_GRASS_QUALITY:
		case PREF_GFX_SHADOW_SOFTNESS:
		case PREF_GFX_FXAA:
		case PREF_GFX_MSAA:
		case PREF_GFX_SCALING:
		case PREF_GFX_TXAA:
		case PREF_GFX_ANISOTROPIC_FILTERING:
		case PREF_GFX_AMBIENT_OCCLUSION:
		case PREF_GFX_TESSELLATION:
		case PREF_GFX_POST_FX:
		case PREF_GFX_DOF:
		case PREF_GFX_MB_STRENGTH:
		case PREF_GFX_DIST_SCALE:
		case PREF_ADV_GFX_LONG_SHADOWS:
		case PREF_ADV_GFX_ULTRA_SHADOWS:
		case PREF_ADV_GFX_HD_FLIGHT:
		case PREF_ADV_GFX_MAX_LOD:
		case PREF_ADV_GFX_SHADOWS_DIST_MULT:
			{
				if(source == UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
				{
					UpdateMemoryBar();
					SUIContexts::SetActive("GFX_Dirty", DirtyGfxSettings() || DirtyAdvGfxSettings());
					CPauseMenu::RedrawInstructionalButtons();
				}

				GRCDEVICE.SetAllowPauseOnFocusLoss(GetMenuPreference(PREF_VID_PAUSE_ON_FOCUS_LOSS) != 0);

				if (g_rlPc.GetGamepadManager())
				{
#if __FINAL
					g_rlPc.GetGamepadManager()->SetBackgroundInputEnabled(!GRCDEVICE.IsAllowPauseOnFocusLoss());
#else
					bool bGameWillPauseOnFocusLoss = GRCDEVICE.IsAllowPauseOnFocusLoss() && PARAM_BlockOnLostFocus.Get();
					g_rlPc.GetGamepadManager()->SetBackgroundInputEnabled(!bGameWillPauseOnFocusLoss);
#endif
				}
			}
			break;

		case PREF_GFX_VID_OVERRIDE:
			{
				if(source == UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
				{
					sm_bWaitingOnGfxOverrideWarning = GetMenuPreference(PREF_GFX_VID_OVERRIDE) == TRUE;
					CPauseMenu::RedrawInstructionalButtons();
					UpdateProfileFromMenuOptions();
				}
			}
			break;
		case PREF_VOICE_ENABLE:
		case PREF_VOICE_TALK_ENABLED:
			{
				if(source == UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
				{
					CMenuScreen& curScreen = GetScreenData(MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT);
					for (s32 iCurrentOption = 0; iCurrentOption < curScreen.MenuItems.GetCount(); iCurrentOption++)
					{
						CMenuItem& curItem = curScreen.MenuItems[iCurrentOption];
						const int DISABLE_FLAG = 1;
						const int ENABLE_FLAG = 0;

						if(curItem.MenuPref == PREF_VOICE_TALK_ENABLED)
						{
							// Disable if no input devices
							RVDeviceInfo inputDevices[VoiceChat::MAX_DEVICES];
							int iInputDeviceCount = RVoice::ListDevices(inputDevices, VoiceChat::MAX_DEVICES, RV_CAPTURE);
							curItem.m_Flags = (iInputDeviceCount == 0) || !GetMenuPreference(PREF_VOICE_ENABLE) ? DISABLE_FLAG : ENABLE_FLAG;
						}
						else if(curItem.MenuPref > PREF_VOICE_TALK_ENABLED)
						{
							curItem.m_Flags = !GetMenuPreference(PREF_VOICE_ENABLE) || !GetMenuPreference(PREF_VOICE_TALK_ENABLED) ? DISABLE_FLAG : ENABLE_FLAG;
						}
						else if(curItem.MenuPref > PREF_VOICE_ENABLE)
						{
							curItem.m_Flags = !GetMenuPreference(PREF_VOICE_ENABLE) ? DISABLE_FLAG : ENABLE_FLAG;
						}
					}

					GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT, true);
				}
			}
		case PREF_VOICE_CHAT_MODE:
		case PREF_VOICE_MIC_VOLUME:
		case PREF_VOICE_MIC_SENSITIVITY:
		case PREF_VOICE_INPUT_DEVICE:
		case PREF_VOICE_OUTPUT_VOLUME:
		case PREF_VOICE_OUTPUT_DEVICE:
			if(source == UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
			{
				UpdateVoiceSettings();
			}
			break;

		case PREF_MOUSE_TYPE:
			{
				ioMouse::MouseType newSetting = static_cast<ioMouse::MouseType>(CPauseMenu::GetMenuPreference(PREF_MOUSE_TYPE));
				if(ioMouse::GetMouseType() != newSetting)
				{
					ioMouse::SetMouseType(newSetting);
				}
			}
			break;
		case PREF_MOUSE_WEIGHT_SCALE:
			{
				float fMouseWeight = (float)GetMenuPreference(PREF_MOUSE_WEIGHT_SCALE) / (float)DEFAULT_SLIDER_MAX;
				ioMouse::sm_MouseWeightScale = fMouseWeight;
			}
			break;
#endif // RSG_PC

#if GTA_REPLAY
		case PREF_REPLAY_MEM_LIMIT:
			{
				eReplayMemoryLimit newSetting = (eReplayMemoryLimit)CPauseMenu::GetMenuPreference(PREF_REPLAY_MEM_LIMIT);

				if( sm_LastReplayMemoryLimit != newSetting )
				{
					CReplayMgr::UpdateAvailableDiskSpace();
					sm_LastReplayMemoryLimit = newSetting;
				}
			}
			break;
			
#endif // GTA_REPLAY

#if KEYBOARD_MOUSE_SUPPORT
		case PREF_INVERT_MOUSE:
			{
				s32 newSetting =  CPauseMenu::GetMenuPreference(PREF_INVERT_MOUSE);
				if(sm_LastMouseInvertMode != newSetting)
				{
					sm_LastMouseInvertMode = newSetting;

					CControlMgr::ReInitControls();

					// Disable the controls for a short while otherwise the option cycles too fast and the controls are re-set so they
					// are marked as just pressed again.
					StartInputCooldown(sm_uDisableInputDuration);
				}
			}
			break;

		case PREF_INVERT_MOUSE_FLYING:
			{
				s32 newSetting =  CPauseMenu::GetMenuPreference(PREF_INVERT_MOUSE_FLYING);
				if(sm_LastMouseInvertFlyingMode != newSetting)
				{
					sm_LastMouseInvertFlyingMode = newSetting;

					CControlMgr::ReInitControls();

					// Disable the controls for a short while otherwise the option cycles too fast and the controls are re-set so they
					// are marked as just pressed again.
					StartInputCooldown(sm_uDisableInputDuration);
				}
			}
			break;

		case PREF_INVERT_MOUSE_SUB:
			{
				s32 newSetting =  CPauseMenu::GetMenuPreference(PREF_INVERT_MOUSE_SUB);
				if(sm_LastMouseInvertSubMode != newSetting)
				{
					sm_LastMouseInvertSubMode = newSetting;

					CControlMgr::ReInitControls();

					// Disable the cCPauseMenu::RestoreMiscControlsDefaultsontrols for a short while otherwise the option cycles too fast and the controls are re-set so they
					// are marked as just pressed again.
					StartInputCooldown(sm_uDisableInputDuration);
				}
			}
			break;
		
		case PREF_SWAP_ROLL_YAW_MOUSE_FLYING:
			{
				s32 newSetting =  CPauseMenu::GetMenuPreference(PREF_SWAP_ROLL_YAW_MOUSE_FLYING);
				if(sm_LastMouseSwapRollYawFlyingMode != newSetting)
				{
					sm_LastMouseSwapRollYawFlyingMode = newSetting;

					CControlMgr::ReInitControls();

					// Disable the controls for a short while otherwise the option cycles too fast and the controls are re-set so they
					// are marked as just pressed again.
					StartInputCooldown(sm_uDisableInputDuration);
				}
			}
			break;

		case PREF_MOUSE_ON_FOOT_SCALE:
		case PREF_MOUSE_DRIVING_SCALE:
		case PREF_MOUSE_PLANE_SCALE:
		case PREF_MOUSE_HELI_SCALE:
		case PREF_MOUSE_SUB_SCALE:
			{
				if(source == UPDATE_PREFS_FROM_MENU || source == UPDATE_PREFS_DEFAULTING)
				{
					CControlMgr::ReInitControls();				
					// NOTE: We don't want to disable inputs for a long time here otherwise it would be painfully slow to update
					// the sensitivity setting. Not disabling it at all could be too fast at high framerates.
					StartInputCooldown(1);
				}
			}
			break;

		case PREF_MOUSE_DRIVE:
		case PREF_MOUSE_FLY:
		case PREF_MOUSE_SUB:
			{
				s32 newDriveMode = CPauseMenu::GetMenuPreference(PREF_MOUSE_DRIVE);
				s32 newFlyMode = CPauseMenu::GetMenuPreference(PREF_MOUSE_FLY);
				s32 newSubMode = CPauseMenu::GetMenuPreference(PREF_MOUSE_SUB);

				if( newDriveMode != sm_LastMouseDriveMode ||
					newFlyMode != sm_LastMouseFlyMode ||
					newSubMode != sm_LastMouseSubMode )
				{
					sm_LastMouseDriveMode = newDriveMode;
					sm_LastMouseFlyMode = newFlyMode;
					sm_LastMouseSubMode = newSubMode;
					CControlMgr::ReInitControls();

					// Disable the controls for a short while otherwise the option cycles too fast and the controls are re-set so they
					// are marked as just pressed again.
					StartInputCooldown(sm_uDisableInputDuration);
				}
			}
			break;

		case PREF_KBM_TOGGLE_AIM:
			{
				bool enabled = CPauseMenu::GetMenuPreference(PREF_KBM_TOGGLE_AIM) == TRUE;

				// We only update if the setting has changed. Otherwise we can kick the player out of toggle aim.
				if(CControlMgr::GetPlayerMappingControl().GetToggleAimEnabled() != enabled)
				{
					CControlMgr::GetPlayerMappingControl().SetToggleAimEnabled(enabled);
				}
			}
			break;

		case PREF_ALT_VEH_MOUSE_CONTROLS:
			{
				// Frank, put your stuff here and delete this comment. Swag.
			}
			break;

#endif // KEYBOARD_MOUSE_SUPPORT

#if KEYBOARD_MOUSE_SUPPORT && FPS_MODE_SUPPORTED
		case PREF_FPS_DEFAULT_AIM_TYPE:
			{
				s32 iNewFPSScopeState = CPauseMenu::GetMenuPreference(PREF_FPS_DEFAULT_AIM_TYPE);
				
				if (iNewFPSScopeState != sm_LastFPSScopeState)
				{
					sm_LastFPSScopeState = iNewFPSScopeState;
					CPlayerInfo::SetShouldUpdateScopeStateFromMenu(true);
				}
			}
			break;
#endif	// KEYBOARD_MOUSE_SUPPORT && FPS_MODE_SUPPORTED

		/* AF: Moved this to when the profile is updated as you exit the pause menu
		case PREF_CURRENT_LANGUAGE:
			{
				// flush render thread to ensure we arent using any of these text tags
				if(GetMenuPreference(PREF_CURRENT_LANGUAGE) != GetMenuPreference(PREF_PREVIOUS_LANGUAGE))
				{
					SetMenuPreference(PREF_PREVIOUS_LANGUAGE, GetMenuPreference(PREF_CURRENT_LANGUAGE));
					gRenderThreadInterface.Flush();
					TheText.ReloadAfterLanguageChange();
				}
			}
			break;*/

		default:
		{
			// dont need to do anything here, as there may be game specific changes
			// which will be set later in CPauseMenu::Update()
			break;
		}
	}
}

void CPauseMenu::StartInputCooldown(s32 duration)
{
	sm_inputCooldownTimer = duration;
}

bool CPauseMenu::IsInInputCooldown()
{
	if(sm_inputCooldownTimer != 0 && (sm_inputCooldownTimer -= fwTimer::GetNonPausedTimeStepInMilliseconds()) <= 0)
	{
		sm_inputCooldownTimer = 0;
	}

	return sm_inputCooldownTimer > 0;
}

// PURPOSE: Call update display config on pause menu movies
void CPauseMenu::UpdateDisplayConfig()
{
	CScaleformMovieWrapper& movie = GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS);
	if( !movie.IsFree() && movie.IsActive())
	{
		SGeneralPauseDataConfig kConfig;
		SGeneralPauseDataConfig* pConfigData = GetMenuArray().GeneralData.MovieSettings.Access("PAUSE_MENU_INSTRUCTIONAL_BUTTONS");
		if( !pConfigData )
			pConfigData = &kConfig;

		Vector2 vMoviePos ( pConfigData->vPos  );
		Vector2 vMovieSize( pConfigData->vSize );

		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(CHudTools::GetFormatFromString(pConfigData->HAlign), &vMoviePos, &vMovieSize);
		CScaleformMgr::OverrideOriginalMoviePosition(movie.GetMovieID(), vMoviePos);
		CScaleformMgr::UpdateMovieParams(movie.GetMovieID(), vMoviePos, vMovieSize);

#if SUPPORT_MULTI_MONITOR
		if(!GRCDEVICE.GetMonitorConfig().isMultihead())
#endif // SUPPORT_MULTI_MONITOR
		{
			if(movie.BeginMethod("SET_PADDING"))
			{
				float fPadding = 0;
#if RSG_PC
				// Horrible solution but it works. The instructional buttons should never have been translated prior to scaling
				//  in the Flash movie. Always scale and rotate first prior to translation, makes life easier for everyone.
				//  Especially in this case where the pause menu is also improperly set up for anything other than 16:9 and
				//  hacked to look okay in other aspect ratios. This fix is for a post-ship patch so it's too late for a proper fix.

				{
					// The following equations will give us approximate results, but are not accurate enough and will fail beyond 16:9.
					// If we find we really need to, we can use them to cover some unhandled cases.

					//// Using equation: y = mx + b, with m & b empirically found
					//const float fPaddingForWindowAspectRatioAtUIAspectRatio_5_4 = (-344.8421052631f)*(fWindowAspectRatio) + (400.05263158f);
					//
					//// Using equation: y1 = m(x1 - x0) + y0, with m empirically found
					//const float fPaddingForWindowAspectRatioAtCurrentUIAspectRatio = (403.578947368421f)*(fUIAspectRatio - 1.25f) + fPaddingForWindowAspectRatioAtUIAspectRatio_5_4;
					//
					//fPadding = fPaddingForWindowAspectRatioAtCurrentUIAspectRatio;
				}

				const float fWindowAspectRatio = (float)VideoResManager::GetUIWidth() / (float)VideoResManager::GetUIHeight();
				const float fUIAspectRatio = CHudTools::GetAspectRatio();
				const float fEpsilon = 0.03f;
		
				if(IsAspectRatioMatched(fUIAspectRatio, (3.0f/2.0f), fEpsilon))
				{
					if(fWindowAspectRatio >= (3.0f/2.0f))
					{
						if(IsAspectRatioMatched(fWindowAspectRatio, (3.0f/2.0f), fEpsilon))
							fPadding = 0.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/10.0f), fEpsilon))
							fPadding = -42.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (5.0f/3.0f), fEpsilon))
							fPadding = -65.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/9.0f), fEpsilon))
							fPadding = -103.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (21.0f/9.0f), fEpsilon))
							fPadding = -292.0f;
					}
				}
				else if(IsAspectRatioMatched(fUIAspectRatio, (4.0f/3.0f), fEpsilon))
				{
					if(fWindowAspectRatio >= (4.0f/3.0f))
					{
						if(IsAspectRatioMatched(fWindowAspectRatio, (4.0f/3.0f), fEpsilon))
							fPadding = -20.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (3.0f/2.0f), fEpsilon))
							fPadding = -79.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/10.0f), fEpsilon))
							fPadding = -114.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (5.0f/3.0f), fEpsilon))
							fPadding = -136.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/9.0f), fEpsilon))
							fPadding = -175.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (21.0f/9.0f), fEpsilon))
							fPadding = -362.0f;
					}
				}
				else if(IsAspectRatioMatched(fUIAspectRatio, (5.0f/3.0f), fEpsilon))
				{
					if(fWindowAspectRatio >= (5.0f/3.0f))
					{
						if(IsAspectRatioMatched(fWindowAspectRatio, (5.0f/3.0f), fEpsilon))
							fPadding = 0.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/9.0f), fEpsilon))
							fPadding = -39.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (21.0f/9.0f), fEpsilon))
							fPadding = -228.0f;
					}
				}
				else if(IsAspectRatioMatched(fUIAspectRatio, (5.0f/4.0f), fEpsilon))
				{
					if(fWindowAspectRatio >= (5.0f/4.0f))
					{
						if(IsAspectRatioMatched(fWindowAspectRatio, (5.0f/4.0f), fEpsilon))
							fPadding = -31.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (4.0f/3.0f), fEpsilon))
							fPadding = -60.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (3.0f/2.0f), fEpsilon))
							fPadding = -118.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/10.0f), fEpsilon))
							fPadding = -152.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (5.0f/3.0f), fEpsilon))
							fPadding = -175.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/9.0f), fEpsilon))
							fPadding = -213.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (21.0f/9.0f), fEpsilon))
							fPadding = -400.0f;
					}
				}
				else if(IsAspectRatioMatched(fUIAspectRatio, (16.0f/9.0f), fEpsilon))
				{
					if(fWindowAspectRatio >= (16.0f/9.0f))
					{
						if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/9.0f), fEpsilon))
							fPadding = 0.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (21.0f/9.0f), fEpsilon))
							fPadding = -189.0f;
					}
				}
				else if(IsAspectRatioMatched(fUIAspectRatio, (16.0f/10.0f), fEpsilon))
				{
					if(fWindowAspectRatio >= (16.0f/10.0f))
					{
						if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/10.0f), fEpsilon))
							fPadding = 0.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (5.0f/3.0f), fEpsilon))
							fPadding = -25.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (16.0f/9.0f), fEpsilon))
							fPadding = -64.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (21.0f/9.0f), fEpsilon))
							fPadding = -253.0f;
					}
				}
				else if(fUIAspectRatio == (17.0f/9.0f))
				{
					fPadding = 0.0f;
				}
				else if(IsAspectRatioMatched(fUIAspectRatio, (21.0f/9.0f), fEpsilon))
				{
					if(fWindowAspectRatio >= (17.0f/9.0f))
					{
						if(IsAspectRatioMatched(fWindowAspectRatio, (17.0f/9.0f), fEpsilon))
							fPadding = 0.0f;
						else if(IsAspectRatioMatched(fWindowAspectRatio, (21.0f/9.0f), fEpsilon))
							fPadding = 0.0f;
					}
				}
				else
				{
					if(!CHudTools::GetWideScreen())
					{
						fPadding = (120 * CHudTools::GetAspectRatio()) - 180;
					}
				}

				if(fPadding == 0.0f)
				{
					const float fPaddingForWindowAspectRatioAtUIAspectRatio_5_4 = (-344.8421052631f)*(fWindowAspectRatio) + (400.05263158f);
					const float fPaddingForWindowAspectRatioAtCurrentUIAspectRatio = (403.578947368421f)*(fUIAspectRatio - 1.25f) + fPaddingForWindowAspectRatioAtUIAspectRatio_5_4;

					fPadding = fPaddingForWindowAspectRatioAtCurrentUIAspectRatio;
				}
#else
				if(!CHudTools::GetWideScreen())
				{
					fPadding = (120 * CHudTools::GetAspectRatio()) - 180;
				}
#endif // RSG_PC

				CScaleformMgr::AddParamFloat(fPadding);
				CScaleformMgr::EndMethod();
			}
		}
		CScaleformMgr::SetMovieDisplayConfig(movie.GetMovieID(), SF_BASE_CLASS_PAUSEMENU);
	}
}

#if RSG_PC
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsAspectRatioMatched()
// PURPOSE:	Determines whether the provided aspect ratios match.
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsAspectRatioMatched(float fAspectRatio1, float fAspectRatio2, float fEpsilon)
{
	if(	fAspectRatio1 >= (fAspectRatio2 - fEpsilon) &&
		fAspectRatio1 <= (fAspectRatio2 + fEpsilon))
	{
		return true;
	}
	else
	{
		return false;
	}
}
#endif // RSG_PC

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::CloseAndLoadSavedGame()
// PURPOSE:	flags to close down the pause menu and start loading a saved game
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::CloseAndLoadSavedGame()
{
	iLoadNewGameTrigger = -1;

#if RSG_PC
	if(CutSceneManager::GetInstance())
	{
		if(CutSceneManager::GetAreBlindersUp())
		{
			CutSceneManager::StartMultiheadFade(false, false);
		}
	}
#endif // RSG_PC

	CGameStreamMgr::GetGameStream()->ForceFlushQueue();

	SetClosingAction(CA_StartSavedGame);
	Close();
}

void CPauseMenu::AbandonLoadingOfSavedGame()
{
	// not ENTIRELY sure this is smart?
	if( sm_eClosingAction == CA_StartSavedGame )
		SetClosingAction(CA_None);
}



void CPauseMenu::CloseAndWarp()
{
	SetClosingAction(CA_Warp);

	CGtaOldLoadingScreen::ForceLoadingRenderFunction(true);
	camInterface::FadeOut(0);

	gRenderThreadInterface.Flush();
	
	Close();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsActive()
// PURPOSE:	returns whether the pausemenu is active
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsActive(PM_ActiveFlags flags /* = DefaultFlags */)
{
	bool isActive = sm_bActive || sm_bClosingDown;
//	if( !isActive flags&PM_WaitUntilFullyFadedInANDOut)!=0 && sm_iCallbacksPending);

	if( isActive && (flags&PM_EssentialAssets) )
	{
		if(	(!GetMovieWrapper(PAUSE_MENU_MOVIE_HEADER).IsActive()) ||
			(!GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).IsActive()) ||
			(!GetMovieWrapper(PAUSE_MENU_MOVIE_SHARED_COMPONENTS).IsActive()) ||
			(!GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).IsActive()))
		{
			isActive = sm_bClosingDown;
		}
	}

	// if already thought active, check if we either didn't fade, or have received our callback
	if( isActive && (flags&PM_WaitUntilFullyFadedInOnly)!=0 )
		isActive = !sm_bStartedUserPause || sm_iCallbacksPending;

	if( !isActive && (flags&PM_SkipSocialClub)==0 )
		isActive = SocialClubMenu::IsActive();

#if !RSG_PC // PC store is part of SCUI so don't want to check this
	if( !isActive && (flags&PM_SkipStore)==0 )
		isActive = cStoreScreenMgr::IsStoreMenuOpen();
#endif

#if GTA_REPLAY
	if( !isActive && (flags&PM_SkipVideoEditor)==0 )
		isActive = CVideoEditorUi::IsActive();
#endif // GTA_REPLAY

	return isActive;
}

bool CPauseMenu::IsRestarting()
{
	if( !IsActive() )
		return false;

	return sm_bRestarting || sm_iLoadingAssetsPhase != PMLP_DONE || sm_bSetupStartingPane;
}

bool CPauseMenu::IsLoadingPhaseDone()
{
	return (sm_iLoadingAssetsPhase == PMLP_DONE);
}

CPauseMenu::scrPauseMenuScriptState CPauseMenu::GetStateForScript()
{
	if( sm_bClosingDown )
		return PM_SHUTTING_DOWN;

	if( cStoreScreenMgr::IsStoreMenuOpen() || sm_eClosingAction == CA_StartCommerceMP  || sm_eClosingAction == CA_StartCommerce  )
		return PM_IN_STORE;

	if( SocialClubMenu::IsActive() )
		return PM_IN_SC_MENU;

#if GTA_REPLAY
	if( CVideoEditorUi::IsActive() )
		return PM_IN_VIDEOEDITOR;
#endif // #if GTA_REPLAY

	// have to check this AFTER sm_bClosingDown and sc/the store
	if( !sm_bActive )
		return PM_INACTIVE;

	if( sm_bRestarting )
		return PM_RESTARTING;

	if( sm_iLoadingAssetsPhase != PMLP_DONE || sm_bSetupStartingPane )
		return PM_STARTING_UP;

	return PM_READY;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::IsPauseMenuControlDisabled()
// PURPOSE:	returns whether the pausemenu control is disabled
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::IsPauseMenuControlDisabled()
{
	return (IsMenuControlLocked());
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetContentArrowVisible()
// PURPOSE:	set content arrows as visible/not visible
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetContentArrowVisible(bool bLeft, bool bVisible)
{
	if (bLeft)
	{
		sm_RenderData.bArrowContentLeftVisible = bVisible;
	}
	else
	{
		sm_RenderData.bArrowContentRightVisible = bVisible;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SetContentArrowPosition()
// PURPOSE:	set content arrow position
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SetContentArrowPosition(bool bLeft, float fPositionX, float fPositionY)
{
	if (bLeft)
	{
		sm_RenderData.vArrowContentLeftPosition.Set(fPositionX, fPositionY);
	}
	else
	{
		sm_RenderData.vArrowContentRightPosition.Set(fPositionX, fPositionY);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::CheckIncomingFunctions()
// PURPOSE:	checks callbacks from ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args, s32 iArgCount)
{
#if OUTRO_EFFECT
	if(sm_bActive || sm_bClosingDown)
	{
		if( methodName == ATSTRINGHASH("PAUSE_MENU_WANTS_CLOSE",0x6928a5db))
		{
			Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			Displayf("Code has received PAUSE_MENU_WANTS_CLOSE");
			Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			Close();
			return;
		}

		if (methodName == ATSTRINGHASH("EXIT_PAUSE_MENU",0xdb896fe8))
		{
			Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			Displayf("Code has received EXIT_PAUSE_MENU");
			Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			if( sm_bClosingDown )
				CloseComplete();
			else
				TriggerMenuClosure();
			return;
		}
	}
	// do not process any incoming functions if not active
	else 
#endif
	if( !sm_bActive )
		return;

	if( IsCurrentScreenValid() 
		&& GetCurrentScreenData().HasDynamicMenu() 
		&& GetCurrentScreenData().GetDynamicMenu()->CheckIncomingFunctions(methodName, args) )
	{
		return;
	}

	if(sm_bWaitOnCreditsScreen)
	{
		return;
	}
	

	if (methodName == ATSTRINGHASH("PAUSE_MENU_READY_TO_RENDER",0xa1286f56))
	{
		STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
		STRVIS_SET_MARKER_TEXT("Pause menu ready to render");
		sm_bRenderMenus = true;

		// ensure these flags are reset:  (fix for 1784962)
		sm_bSetupStartingPane = false;  // no longer setting up starting pane as we have recieved PAUSE_MENU_READY_TO_RENDER

		return;
	}

	if (methodName == ATSTRINGHASH("MENU_SHIFT_DEPTH_PROCESSED",0xe07398c6))
	{
		sm_bAwaitingMenuShiftDepthResponse = false;
		
		if (IsNavigatingContent())
		{
			sm_waitingForForceDropIntoMenu = false;
		}
		return;
	}

	if (methodName == ATSTRINGHASH("SET_PAUSE_MENU_PREF",0x610e6168))
	{
		if (GetCurrentScreen() == MENU_UNIQUE_ID_SETTINGS)
		{
			if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber(), "SET_PAUSE_MENU_PREF params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
			{
				s32 iPrefId = (s32)args[1].GetNumber();
				s32 iPrefNewValue = (s32)args[2].GetNumber();

				if (iPrefNewValue != GetMenuPreference(iPrefId))  // different pref, so play a sound
				{
					uiDebugf1("PauseMenu: Actionscript requested pref %d changed to %d", iPrefId, iPrefNewValue);

					SetItemPref(iPrefId, iPrefNewValue,  UPDATE_PREFS_FROM_MENU);
					if(iPrefId == PREF_CTRL_SPEAKER_VOL)
					{
						PlaySound("PAD_SPEAKER_BEEP");
					}
					PlaySound("NAV_LEFT_RIGHT");
				}
			}
		}

		return;
	}

	// weird name because of short-sighted AS, means just to close the context menu
	if(methodName == ATSTRINGHASH("FRIENDITEM_SUBMENU_CLOSE", 0x9B18411D))
	{
		// spoof the input so we don't have code duplication
		if( IsCurrentScreenValid() )
			GetCurrentScreenData().HandleContextMenuInput(PAD_CIRCLE);

		return;
	}


#if KEYBOARD_MOUSE_SUPPORT
	if(sm_bWaitOnCreditsScreen)
	{
		if (methodName == ATSTRINGHASH("HANDLE_MOUSE_LEFT_RIGHT", 0x86DAF8C7))
		{
			if (GetCurrentScreen() == MENU_UNIQUE_ID_SETTINGS)
			{
				if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber(), "HANDLE_MOUSE_LEFT_RIGHT params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
				{
					int iPrefId = (int)args[1].GetNumber();
					int iDirection = (int)args[2].GetNumber();
					HandleMouseLeftRight(iPrefId, iDirection);	
				}
			}

			return;
		}

		if (methodName == ATSTRINGHASH("HANDLE_SCROLL_CLICK", 0x9CE8ECE9))
		{
			if (uiVerifyf(args[1].IsNumber(), "HANDLE_SCROLL_CLICK params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
			{
				if(IsNavigatingContent())
				{
					sm_iMouseScrollDirection = (int)args[1].GetNumber();
				}
			}
			return;
		}

		if (methodName == ATSTRINGHASH("HOVER_PAUSE_MENU_ITEM", 0x3BF83AD7))
		{
			if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber() && args[3].IsNumber(), "HOVER_PAUSE_MENU_ITEM params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3])))
			{
				sm_MouseHoverEvent.iIndex		= (int)args[1].GetNumber();
				sm_MouseHoverEvent.iMenuItemId	= (int)args[2].GetNumber();
				sm_MouseHoverEvent.iUniqueId	= (int)args[3].GetNumber();
			}
			return;
		}

		if (methodName == ATSTRINGHASH("HAIR_COLOUR_SELECT", 0x1060FB05))
		{
			if (args[1].IsNumber())
			{
				sm_iHairColourSelected = (int)args[1].GetNumber();
			}
		}

		if (methodName == ATSTRINGHASH("CLICK_PAUSE_MENU_ITEM", 0xdf78978c))
		{
			if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber() && args[3].IsNumber(), "CLICK_PAUSE_MENU_ITEM params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3])))
			{
				sm_MouseClickEvent.iIndex		= (int)args[1].GetNumber();
				sm_MouseClickEvent.iMenuItemId	= (int)args[2].GetNumber();
				sm_MouseClickEvent.iUniqueId	= (int)args[3].GetNumber();
			}
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	if (methodName == ATSTRINGHASH("GET_CONTROLLER_IMG", 0x40F20264))
	{
		char dirName[128];
		char imgName[128];

		// TODO: PC should support different controller images
		formatf(dirName, "pause_menu_pages_settings%s", SCALEFORM_MOVIE_PLATFORM_SUFFIX);
		formatf(imgName, "controller");

		if(GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).BeginMethod("SET_CONTROL_IMAGE"))
		{
			CScaleformMgr::AddParamString(dirName);
			CScaleformMgr::AddParamString(imgName);
			CScaleformMgr::EndMethod();
		}

		return;
	}

	

/*	if (methodName == ATSTRINGHASH("SET_MAP_SHOW_BLIPS",0xeb0ceabc))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsBool(), "SET_MAP_SHOW_BLIPS params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			s32 iUniqueId = (s32)args[1].GetNumber();
			bool bShowBlips = (s32)args[2].GetBool();

			uiDebugf3("PauseMenu: Actionscript SET_MAP_SHOW_BLIPS iUniqueId=%d bShowBlips=%s", iUniqueId, bShowBlips ? "TRUE" : "FALSE");

			CMapMenu::SetLegendItemActive(iUniqueId, bShowBlips, true);

			PlaySound("SELECT");
		}

		return;
	}*/

	if (methodName == ATSTRINGHASH("TRIGGER_PAUSE_MENU_EVENT",0x51556734))
	{
#if GTA_REPLAY
		if (CVideoEditorUi::IsActive() && !CPauseMenu::IsClosingDown())   // ignore pausemenu events from actionscript in the pausemenu behind when video editor is open
		{
			return;
		}
#endif // GTA_REPLAY

		if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber() && args[3].IsNumber(), "TRIGGER_PAUSE_MENU_EVENT params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3]) ))
		{
			MenuScreenId iTriggerId((s32)args[1].GetNumber() - PREF_OPTIONS_THRESHOLD);
			s32 iMenuIndex = (s32)args[2].GetNumber();

			s32 iNewMenuIndex = args[3].IsDefined() ? (s32)args[3].GetNumber() : MENU_UNIQUE_ID_INVALID;

			if (iNewMenuIndex != MENU_UNIQUE_ID_INVALID)
			{
				iNewMenuIndex -= PREF_OPTIONS_THRESHOLD;
			}

			MenuScreenId newMenuIndex(iNewMenuIndex);

			uiDebugf3("PauseMenu: Actionscript requested %d is triggered with menu index of %d. New menu index is %d", iTriggerId.GetValue(), iMenuIndex, iNewMenuIndex);
			
			TriggerEvent(iTriggerId, iMenuIndex, newMenuIndex);

			if( 
//#if GTA_VERSION >= 500
				!SUIContexts::IsActive(UIATSTRINGHASH("HACK_SKIP_SELECT_SOUND",0x9fb52674)) && 
//#endif
				(!AnyLayerHasFlag(Sound_NoAccept) || !IsNavigatingContent() ) )
				PlaySound("SELECT");
//#if GTA_VERSION >= 500		
			SUIContexts::Deactivate(UIATSTRINGHASH("HACK_SKIP_SELECT_SOUND",0x9fb52674));
//#endif

			// repopulate any instructional buttons
			if( ShouldDrawInstructionalButtons() )
				GetCurrentScreenData().DrawInstructionalButtons(iTriggerId, iMenuIndex);
		}

		return;
	}

	if( methodName == ATSTRINGHASH("MENUCEPTION_KICK",0xcd1e769b))
	{
		MenuceptionTheKick();
		return;
	}

	if (methodName == ATSTRINGHASH("SET_HEADER_ARROW_VISIBLE",0x53d74df))
	{
		// new hottness version
		if(Likely(iArgCount==4))
		{
			if (uiVerifyf(args[1].IsBool() && args[2].IsBool() && args[3].IsBool(), "SET_HEADER_ARROW_VISIBLE params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3])))
			{
				sm_RenderData.bArrowHeadersShow = args[1].GetBool();
				sm_RenderData.bArrowHeaderLeftVisible = args[2].GetBool();
				sm_RenderData.bArrowHeaderRightVisible = args[3].GetBool();
			}
		}
		// old-timey version
		else
		{
			if (uiVerifyf(args[1].IsBool() && args[2].IsBool(), "SET_HEADER_ARROW_VISIBLE params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
			{
				bool bLeftArrow = args[1].GetBool();
				bool bVisible = args[2].GetBool();
				if (bLeftArrow)
					sm_RenderData.bArrowHeaderLeftVisible = bVisible;
				else
					sm_RenderData.bArrowHeaderRightVisible = bVisible;
				sm_RenderData.bArrowHeadersShow = sm_RenderData.bArrowHeaderLeftVisible || sm_RenderData.bArrowHeaderRightVisible;
			}
		}

		return;
	}
#if !OUTRO_EFFECT
	if (methodName == ATSTRINGHASH("EXIT_PAUSE_MENU",0xdb896fe8))
	{
		if( !sm_bClosingDown 
			&& !NetworkInterface::IsGameInProgress()
			&& CutSceneManager::GetInstance()->IsRunning() 
			&& GetCurrentMenuVersion() != FE_MENU_VERSION_CUTSCENE_PAUSE
			&& !sm_bRestarting)  // fix for 1707721
		{
			OpenCorrectMenu();
		}
		else
		{
			Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			Displayf("Code has received EXIT_PAUSE_MENU");
			Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			TriggerMenuClosure();
		}
		return;
	}
#endif
	if (methodName == ATSTRINGHASH("RESTART_PAUSE_MENU",0x6a8cde71))
	{
		Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		Displayf("Code has received RESTART_PAUSE_MENU, jumping to %i", sm_iRestartingMenuVersion);
		Displayf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

		// cache this off so we can check it after we set our new menu
		bool bOldVersionHadFade = GetCurrentMenuVersionHasFlag(kFadesIn);

		SetCurrentMenuVersion(sm_iRestartingMenuVersion);

		if( bOldVersionHadFade != GetCurrentMenuVersionHasFlag(kFadesIn) )
		{
			if(GetCurrentMenuVersionHasFlag(kFadesIn) )
				PAUSEMENUPOSTFXMGR.StartFadeIn();
			else
				PAUSEMENUPOSTFXMGR.StartFadeOut();
		}

		OpenLite(sm_iRestartingHighlightTab);

		return;
	}

	if (methodName == ATSTRINGHASH("ENTER_SC_MENU",0x1b472479))
	{
		EnterSocialClub();
		return;
	}

	if (methodName == ATSTRINGHASH("LAYOUT_CHANGED",0xd9550e07) 
		|| methodName == ATSTRINGHASH("LAYOUT_CHANGED_FOR_SCRIPT_ONLY",0x02cbf996) 
		|| methodName == ATSTRINGHASH("LAYOUT_CHANGED_NO_LOAD",0x43ef4af5)
		|| methodName == ATSTRINGHASH("LAYOUT_CHANGE_INITIAL_FILL",0x8376a368) )
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber() && args[3].IsNumber() && args[4].IsNumber(), "LAYOUT_CHANGED params not compatible: %s %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3]), sfScaleformManager::GetTypeName(args[4])))
		{
			MenuScreenId iPreviousLayout((s32)args[1].GetNumber() - PREF_OPTIONS_THRESHOLD);
			MenuScreenId iNewLayout((s32)args[2].GetNumber() - PREF_OPTIONS_THRESHOLD); // dont use iNewLayout yet so dont initialise it yet in code
			s32 iUniqueId = (s32)args[3].GetNumber();
			s32 iDirection = (s32)args[4].GetNumber();
			bool bResponseNeeded = false;

			if (args[5].IsBool())
			{
				bResponseNeeded = args[5].GetBool();
			}

			if (bResponseNeeded && iNewLayout != MENU_UNIQUE_ID_MAP_LEGEND)
			{
				GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("MENU_INTERACTION_RESPONSE", iNewLayout.GetValue()+PREF_OPTIONS_THRESHOLD);
			}

			LayoutChanged(iNewLayout, iPreviousLayout, iUniqueId, iDirection, methodName);
		}

		return;
	}

	if (methodName == ATSTRINGHASH("IS_NAVIGATING_CONTENT",0x68249689))
	{
		if (uiVerifyf(args[1].IsBool(), "IS_NAVIGATING_CONTENT params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			sm_waitingForForceDropIntoMenu = false;
			
			
			bool bIsNavigating = args[1].GetBool();
			bool bClosePM = !bIsNavigating && IsNavigatingContent() && GetCurrentMenuVersionHasFlag(kAllowBackingOutInMenu);

			SetNavigatingContent(bIsNavigating);

			// bail out of MPM
			if( bIsNavigating && sm_bMaxPayneMode )
				SetMaxPayneMode(false);

			if( bClosePM )
			{
				Close();
			}
			else
			{
				// repopulate any instructional buttons
				RedrawInstructionalButtons();
			}
		}

		return;
	}

#if  RSG_PC
	if (methodName == ATSTRINGHASH("SET_CODE_MENU_INDEX",0x49b1109a))
	{
		if (uiVerifyf(args[1].IsNumber(), "SET_CODE_MENU_INDEX param not compatible.  Expecting Number but received %s", sfScaleformManager::GetTypeName(args[1])))
		{
			if( sm_pMsgToWarnOnTabChange )
				ShowTabChangeWarning((s32)args[1].GetNumber(), true);
			else
				TriggerSwitchPaneWithTabIndex((s32)args[1].GetNumber());
		}
	}
#endif
}


void CPauseMenu::LayoutChanged( MenuScreenId iNewLayout, MenuScreenId iPreviousLayout, s32 iUniqueId, s32 iDir, atHashWithStringBank methodName )
{
	if (sm_bRestarting || sm_bClosingDown) // if restarting then ignore any layout changed until fully restarted
	{
		return;
	}

	// find the matching request
	int iMatchingStreamRequest = NO_STREAMING_MOVIE;
	for(int i=0; i< MAX_PAUSE_MENU_CHILD_MOVIES;++i)
	{
		CStreamMovieHelper& wrapper = GetChildMovieHelper(i);
		if( !wrapper.IsFree() && wrapper.GetRequestingScreen() == iNewLayout )
		{
			iMatchingStreamRequest = i;
			break;
		}
	}

	if( iMatchingStreamRequest != NO_STREAMING_MOVIE )
	{
		if( sm_iStreamingMovie != NO_STREAMING_MOVIE 
			&& iMatchingStreamRequest != sm_iStreamingMovie )
		{
			uiDisplayf("Received %s for %s, but we're transitioning to %s, so ignoring it!", atHashString(methodName).TryGetCStr(), GetChildMovieHelper(iMatchingStreamRequest).GetRequestingScreen().GetParserName(), GetChildMovieHelper(sm_iStreamingMovie).GetRequestingScreen().GetParserName() );
			return;
		}
	}

	eLAYOUT_CHANGED_DIR iDirection = static_cast<eLAYOUT_CHANGED_DIR>(iDir);

	// don't cache off the menu layout changes if we're switching panels
	if( sm_bWaitingForFirstLayoutChanged || IsCurrentScreenValid() )
	{
		sm_bMenuLayoutChangedEventOccurred = true;
		sm_iMenuEventOccurredUniqueId[0] = iPreviousLayout.GetValue();
		sm_iMenuEventOccurredUniqueId[1] = iNewLayout.GetValue();
		sm_iMenuEventOccurredUniqueId[2] = iUniqueId;
	}

	//	This works for backing out, but if I now press X while Game is selected then Load Game is highlighted but the load menu isn't repopulated
	if ((iPreviousLayout == MENU_UNIQUE_ID_LOAD_GAME || iPreviousLayout == MENU_UNIQUE_ID_SAVE_GAME_LIST)
		&& (iNewLayout != MENU_UNIQUE_ID_LOAD_GAME && iNewLayout != MENU_UNIQUE_ID_SAVE_GAME_LIST))
	{
		BackOutOfSaveGameList();

		//	Graeme - commented out the following lines. I don't think they could ever be reached.
		//	if (iPreviousLayout == MENU_UNIQUE_ID_SAVE_GAME && iNewLayout != MENU_UNIQUE_ID_SAVE_GAME)
		//	{
		//		Close();
		//	}
	}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if ( (iPreviousLayout == MENU_UNIQUE_ID_PROCESS_SAVEGAME || iPreviousLayout == MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST)
		&& (iNewLayout != MENU_UNIQUE_ID_PROCESS_SAVEGAME && iNewLayout != MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST) )
	{
		BackOutOfSaveGameList();
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	SUIContexts::Deactivate(UIATSTRINGHASH("HasContextMenu",0x52536cbf));

	if( IsCurrentScreenValid() )
	{
		CMenuScreen& curData = GetCurrentScreenData();
		if( curData.HasDynamicMenu() )
		{
			CMenuBase* pMenu = curData.GetDynamicMenu();
			CContextMenu* curMenu = pMenu->GetContextMenu();
			SUIContexts::SetActive(UIATSTRINGHASH("HasContextMenu",0x52536cbf), 
				IsNavigatingContent() && curMenu && curMenu->HasOptions() && curMenu->GetTriggerMenuId() == iNewLayout );

			if( iDirection != LAYOUT_CHANGED_DIR_BACKWARDS || curData.HasFlag(LayoutChangedOnBack) )
			{
				// filter this out if you have a menu, and our context menu isn't involved
				if( !curMenu || !curMenu->IsOpen() || curMenu->GetContextMenuId() != iNewLayout )
				{
					pMenu->LayoutChanged(iPreviousLayout, iNewLayout, iUniqueId, iDirection);
				}
			}
		}
	}

#if __ALLOW_SP_CREDITS
	if (iPreviousLayout == MENU_UNIQUE_ID_CREDITS || 
		iPreviousLayout == MENU_UNIQUE_ID_LEGAL ||
		iPreviousLayout == MENU_UNIQUE_ID_CREDITS_LEGAL)
	{
		SUIContexts::Deactivate(CAN_PLAY_CREDITS);
		SUIContexts::Deactivate(HIDE_ACCEPTBUTTON_CONTEXT);
		RedrawInstructionalButtons();
		sm_bDisplayCreditsScreenNextFrame = false;
	}
#endif // __ALLOW_SP_CREDITS

	if (iDirection == LAYOUT_CHANGED_DIR_BACKWARDS)  // tell script the layout changed values but then exit out if going backwards
	{
		if ( (iNewLayout == MENU_UNIQUE_ID_LOAD_GAME) && (iPreviousLayout == MENU_UNIQUE_ID_SAVE_GAME_LIST) )
		{
			SetCurrentPane(iNewLayout);

			SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
			RedrawInstructionalButtons();
		}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		if ( (iNewLayout == MENU_UNIQUE_ID_PROCESS_SAVEGAME) && (iPreviousLayout == MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST) )
		{
			SetCurrentPane(iNewLayout);

//			SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
			RedrawInstructionalButtons();
		}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		if ( (iNewLayout == MENU_UNIQUE_ID_GAME) 
			&& ( (iPreviousLayout == MENU_UNIQUE_ID_LOAD_GAME) || (iPreviousLayout == MENU_UNIQUE_ID_NEW_GAME)
					|| (iPreviousLayout == MENU_UNIQUE_ID_PROCESS_SAVEGAME)) )
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
		if ( (iNewLayout == MENU_UNIQUE_ID_GAME) 
			&& ( (iPreviousLayout == MENU_UNIQUE_ID_LOAD_GAME) || (iPreviousLayout == MENU_UNIQUE_ID_NEW_GAME) ) )
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
		{
			SetCurrentPane(iNewLayout);
		}

		return;
	}

	#if RSG_PC
	if (iNewLayout == MENU_UNIQUE_ID_EXIT_TO_WINDOWS)
	{
		DisplayQuitGameAlertMessage();
		SetCurrentPane(iNewLayout);				//	Graeme - ensure that CurrentPane doesn't stay as MENU_UNIQUE_ID_LOAD_GAME when moving from Load Game to New Game
		return;									//	otherwise IsLoadGameOptionHighlighted() will return TRUE when New Game is highlighted
	}
	#endif

	if (iNewLayout == MENU_UNIQUE_ID_SAVE_GAME_LIST ||
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		iNewLayout == MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST || 
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		iNewLayout == MENU_UNIQUE_ID_IMPORT_SAVEGAME || 
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
#if __ALLOW_SP_CREDITS
		iNewLayout == MENU_UNIQUE_ID_CREDITS ||
		iNewLayout == MENU_UNIQUE_ID_LEGAL ||
		iNewLayout == MENU_UNIQUE_ID_CREDITS_LEGAL ||
#endif // __ALLOW_SP_CREDITS
		iNewLayout == MENU_UNIQUE_ID_NEW_GAME)

	{
		if (iNewLayout == MENU_UNIQUE_ID_NEW_GAME)
		{
			DisplayNewGameAlertMessage();
		}
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		else if (iNewLayout == MENU_UNIQUE_ID_IMPORT_SAVEGAME)
		{
			DisplayImportAlertMessage();
		}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
#if __ALLOW_SP_CREDITS
		else if (iNewLayout == MENU_UNIQUE_ID_CREDITS || 
				 iNewLayout == MENU_UNIQUE_ID_LEGAL ||
				 iNewLayout == MENU_UNIQUE_ID_CREDITS_LEGAL)
		{
			DisplayCreditsMessage(iNewLayout);
			SUIContexts::Activate(HIDE_ACCEPTBUTTON_CONTEXT);

			bool const c_isPlayCreditsSupported = IsPlayCreditsSupportedOnThisScreen( iNewLayout );
			SUIContexts::SetActive( CAN_PLAY_CREDITS, c_isPlayCreditsSupported );

			RedrawInstructionalButtons();
		}
#endif // __ALLOW_SP_CREDITS
		SetCurrentPane(iNewLayout);				//	Graeme - ensure that CurrentPane doesn't stay as MENU_UNIQUE_ID_LOAD_GAME when moving from Load Game to New Game
		return;									//	otherwise IsLoadGameOptionHighlighted() will return TRUE when New Game is highlighted
	}

#if GTA_REPLAY
	SUIContexts::SetActive("ON_VIDEO_TAB", iNewLayout == MENU_UNIQUE_ID_REPLAY_EDITOR);
#endif // GTA_REPLAY

	MenuArrayIndex curData = GetActualScreen(iNewLayout);

	if( sm_bWaitingForFirstLayoutChanged || (curData != MENU_UNIQUE_ID_INVALID && !GetScreenDataByIndex(curData).HasFlag(NeverGenerateMenuData)))
	{
		uiDebugf3("PauseMenu: Actionscript requested screen change to %d", iNewLayout.GetValue());
		if( (!sm_aMenuState.empty() && sm_aMenuState.Top().iMenuceptionDir != kMENUCEPT_LIMBO )
			|| (!IsNavigatingContent() && methodName != ATSTRINGHASH("LAYOUT_CHANGE_INITIAL_FILL",0x8376a368))
			)
		{
			// garbage collect on all 3 pausemenu movies when we change menu data
			for (s32 i = 0; i < MAX_PAUSE_MENU_BASE_MOVIES; i++)
			{
				// do not garbage collect on any child movies or the shared component movie (as this is also a child)
				if (i != PAUSE_MENU_MOVIE_SHARED_COMPONENTS 
					&& i < PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_START 
					&& i > PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_END )
				{
					if( sm_iBaseMovieId[i].IsActive() )
						sm_iBaseMovieId[i].ForceCollectGarbage();
				}
			}

			if( methodName != ATSTRINGHASH("LAYOUT_CHANGED_FOR_SCRIPT_ONLY", 0x02cbf996)
				&& methodName != ATSTRINGHASH("LAYOUT_CHANGED_NO_LOAD",0x43ef4af5) )
			{
				// perform a cleanup if we streamed our movie
				if( iMatchingStreamRequest != NO_STREAMING_MOVIE )
				{
					for(int i=0; i< MAX_PAUSE_MENU_CHILD_MOVIES;++i)
					{
						CStreamMovieHelper& wrapper = GetChildMovieHelper(i);
						if( !wrapper.IsFree() && i != iMatchingStreamRequest ) // && wrapper.IsMovieReady() 
						{
							uiDebugf1("LAYOUT_CHANGED cleanup: index %i, %s - %s", i, wrapper.GetRequestingScreen().GetParserName(), wrapper.GetGfxFilenameForDebug());
							wrapper.Remove();
						}
					}
				}
			}

			sfDisplayf("STREAMED_PANE Actionscript called LAYOUT_CHANGED and moved to new pane '%s - %d'", iNewLayout.GetParserName(), iNewLayout.GetValue());
		}

		SetCurrentPane(iNewLayout);
		GenerateMenuData(iNewLayout);

		if(iNewLayout == MENU_UNIQUE_ID_MAP)
		{
			CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SHOW_COLUMN", 1, false );  // clear stuff set up on left column when moving to the map screen - fixes 1538174
		}

		if(sm_bWaitingForFirstLayoutChanged && GetCurrentMenuVersionHasFlag(kAutoShiftDepth))
		{
			MENU_SHIFT_DEPTH(kMENUCEPT_DEEPER);
		}

		sm_bWaitingForFirstLayoutChanged = false;
	}

	// repopulate any instructional buttons
	if( ShouldDrawInstructionalButtons() )
		GetCurrentScreenData().DrawInstructionalButtons(iNewLayout, iUniqueId);

	if (sm_iStreamingMovie == NO_STREAMING_MOVIE || sm_iStreamedMovie == NO_STREAMING_MOVIE)  // only reset things if we are not streaming any more
	{
		sm_bRenderContent = true;
	}
}

bool CPauseMenu::WaitingForForceDropIntoMenu()
{
	return sm_dropIntoMenuWhenStreamed || sm_forceDropIntoMenu || sm_waitingForForceDropIntoMenu;
}

bool CPauseMenu::ShouldDrawInstructionalButtons()
{
	// We should only draw instructional buttons if we're not waiting for a menu to drop in when streamed
	return IsCurrentScreenValid() && 
			(!GetCurrentScreenData().HasFlag(EnterMenuOnMouseClick) || !WaitingForForceDropIntoMenu());
}


//////////////////////////////////////////////////////////////////////////
void CPauseMenu::RedrawInstructionalButtons(int iUniqueId)
{
	// repopulate any instructional buttons
	// the hope is that the index is immaterial
	if( ShouldDrawInstructionalButtons() )
	{
		GetCurrentScreenData().DrawInstructionalButtons(GetCurrentActivePanel(), iUniqueId);
		UpdateDisplayConfig();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPauseMenu::ClearInstructionalButtons()
{
	if(IsCurrentScreenValid() )
		GetCurrentScreenData().ClearInstructionalButtons();

	if (DynamicMenuExists())
	{
		GetDynamicPauseMenu()->SetLastButtonHash(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPauseMenu::OverrideButtonText(int iSlotIndex, const char* pString)
{
	if( GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).IsActive() )
	{
		GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).CallMethod("OVERRIDE_RESPAWN_TEXT", iSlotIndex, pString);
	}
}

//////////////////////////////////////////////////////////////////////////

void CPauseMenu::SetStoreAvailable(bool bIsAvailable)
{
	if (sm_bStoreAvailable != bIsAvailable)
	{
		sm_bStoreAvailable = bIsAvailable;
		SUIContexts::SetActive(UIATSTRINGHASH("STORE_AVAILABLE",0x8b24ff66), bIsAvailable);
	}
}

void CPauseMenu::SetNavigatingContent(bool bAreWe)
{
	sm_bNavigatingContent = bAreWe;
	SUIContexts::SetActive(UIATSTRINGHASH("NAVIGATING_CONTENT",0x8fadff36), bAreWe);

	if( IsCurrentScreenValid() && 
		GetCurrentScreenData().HasDynamicMenu() )
		GetCurrentScreenData().GetDynamicMenu()->OnNavigatingContent(bAreWe);
}

#if RSG_PS3
void CPauseMenu::HandleWirelessHeadsetContextChange()
{
	uiDisplayf("HandleWirelessHeadsetContextChange: %s", audNorthAudioEngine::IsWirelessHeadsetConnected() ? "connected" : "not connected");
	SUIContexts::SetActive(UIATSTRINGHASH("AUD_WIRELESSHEADSET",0xE32A2BF1), audNorthAudioEngine::IsWirelessHeadsetConnected());
	
	// this'd preferably be contained in the settings menu proper.
	if(sm_bActive && GetCurrentActivePanel() == MENU_UNIQUE_ID_SETTINGS_AUDIO)
	{
		if(GetMenuPrefSelected() == PREF_SPEAKER_OUTPUT)
		{
			SetMenuPrefSelected(PREF_WIRELESS_STEREO_HEADSET);
		}
		GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_AUDIO, true);
		UpdateAudioImage();
	}
}
#endif

void CPauseMenu::SetNoValidSaveGameFiles(bool bNoSaves)
{
	sm_bNoValidSaveGameFiles = bNoSaves;

	if (sm_bNoValidSaveGameFiles)
	{
		if (IsInLoadGamePanel())
		{
			BackOutOfLoadGamePanes();
		}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		if (IsInUploadSavegamePanel())
		{
			BackOutOfUploadSavegamePanes();
		}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

		SUIContexts::Activate(NO_SAVEGAMES_CONTEXT);
	}
	else
	{
		SUIContexts::Deactivate(NO_SAVEGAMES_CONTEXT);
	}

	RedrawInstructionalButtons();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::BackOutOfSaveGameList()
// PURPOSE:	flag to back out of savegame lists and unlocks any menu control
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::BackOutOfSaveGameList()
{
	uiDisplayf("CPauseMenu::BackOutOfSaveGameList has been called");
	if (sm_bSaveGameListSync)
	{
		sm_bQueueManualLoadASAP = false;
		sm_bQueueManualSaveASAP = false;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		sm_bQueueUploadSavegameASAP = false;
		uiDisplayf("CPauseMenu::BackOutOfSaveGameList - setting sm_bQueueManualLoadASAP, sm_bQueueManualSaveASAP and sm_bQueueUploadSavegameASAP to false");
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
		uiDisplayf("CPauseMenu::BackOutOfSaveGameList - setting sm_bQueueManualLoadASAP and sm_bQueueManualSaveASAP to false");
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

		SUIContexts::Deactivate(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
		SUIContexts::Deactivate(SELECT_STORAGE_DEVICE_CONTEXT);
		SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
		RedrawInstructionalButtons();

		bool bAtTopOfQueue = false;

		if (IsInSaveGameMenus())
		{
			uiAssertf(sm_ManualLoadStructure.GetStatus() != MEM_CARD_BUSY, "CPauseMenu::BackOutOfSaveGameList - Graeme - in the manual save menu so I don't expect the status of the manual load to be BUSY");
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
			uiAssertf(sm_ExportSPSave.GetStatus() != MEM_CARD_BUSY, "CPauseMenu::BackOutOfSaveGameList - Graeme - in the manual save menu so I don't expect the status of the ExportSPSave to be BUSY");
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
			if (CGenericGameStorage::ExistsInQueue(&sm_ImportSPSave, bAtTopOfQueue))
			{
				uiDisplayf("CPauseMenu::BackOutOfSaveGameList - ImportSPSave exists in queue");

				if (bAtTopOfQueue)
				{
					uiDisplayf("CPauseMenu::BackOutOfSaveGameList - ImportSPSave is at the top of the queue so setting its flag to quit as soon as possible");
					sm_ImportSPSave.SetQuitAsSoonAsNoSavegameOperationsAreRunning();
				}
				else
				{
					uiDisplayf("CPauseMenu::BackOutOfSaveGameList - ImportSPSave is not at the top of the queue so removing it from the queue now");
					CGenericGameStorage::RemoveFromSavegameQueue(&sm_ImportSPSave);
				}
			}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

			if (CGenericGameStorage::ExistsInQueue(&sm_ManualSaveStructure, bAtTopOfQueue))
			{
				uiDisplayf("CPauseMenu::BackOutOfSaveGameList - manual save exists in queue");

				if (bAtTopOfQueue)
				{
					uiDisplayf("CPauseMenu::BackOutOfSaveGameList - manual save is at the top of the queue so setting its flag to quit as soon as possible");
					sm_ManualSaveStructure.SetQuitAsSoonAsNoSavegameOperationsAreRunning();
				}
				else
				{
					uiDisplayf("CPauseMenu::BackOutOfSaveGameList - manual save is not at the top of the queue so removing it from the queue now");
					CGenericGameStorage::RemoveFromSavegameQueue(&sm_ManualSaveStructure);
				}
			}
		}
		else
		{
			uiAssertf(sm_ManualSaveStructure.GetStatus() != MEM_CARD_BUSY, "CPauseMenu::BackOutOfSaveGameList - Graeme - not in the manual save menu so I don't expect the status of the manual save to be BUSY");
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
			uiAssertf(sm_ImportSPSave.GetStatus() != MEM_CARD_BUSY, "CPauseMenu::BackOutOfSaveGameList - Graeme - not in the manual save menu so I don't expect the status of the ImportSPSave to be BUSY");
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
			if (CGenericGameStorage::ExistsInQueue(&sm_ExportSPSave, bAtTopOfQueue))
			{
				uiDisplayf("CPauseMenu::BackOutOfSaveGameList - ExportSPSave exists in queue");

				if (bAtTopOfQueue)
				{
					uiDisplayf("CPauseMenu::BackOutOfSaveGameList - ExportSPSave is at the top of the queue so setting its flag to quit as soon as possible");
					sm_ExportSPSave.SetQuitAsSoonAsNoSavegameOperationsAreRunning();
				}
				else
				{
					uiDisplayf("CPauseMenu::BackOutOfSaveGameList - ExportSPSave is not at the top of the queue so removing it from the queue now");
					CGenericGameStorage::RemoveFromSavegameQueue(&sm_ExportSPSave);
				}
			}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

			if (CGenericGameStorage::ExistsInQueue(&sm_ManualLoadStructure, bAtTopOfQueue))
			{
				uiDisplayf("CPauseMenu::BackOutOfSaveGameList - manual load exists in queue");

				if (bAtTopOfQueue)
				{
					uiDisplayf("CPauseMenu::BackOutOfSaveGameList - manual load is at the top of the queue so setting its flag to quit as soon as possible");
					sm_ManualLoadStructure.SetQuitAsSoonAsNoSavegameOperationsAreRunning();
				}
				else
				{
					uiDisplayf("CPauseMenu::BackOutOfSaveGameList - manual load is not at the top of the queue so removing it from the queue now");
					CGenericGameStorage::RemoveFromSavegameQueue(&sm_ManualLoadStructure);
				}
			}
		}

		UnlockMenuControl();
		sm_bSaveGameListSync = false;
	}
}

void CPauseMenu::ResetMenuItemTriggered()
{
	uiDisplayf("CPauseMenu::ResetMenuItemTriggered has been called");

	iLoadNewGameTrigger = -1;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::SaveSettings()
// PURPOSE:	saves settings on PC
/////////////////////////////////////////////////////////////////////////////////////
void CPauseMenu::SaveSettings()
{
#if RSG_PC
	FileHandle fid;

	CFileMgr::SetDir("");
	fid = CFileMgr::OpenFileForWriting("common:/DATA/SETTINGS.CFG");  // open for writing

	if (fid)
	{
//		CFileMgr::Write(fid, (char*)&CFrontEndPreferences::MenuPreferences, sizeof(CFrontEndPreferences::MenuPreferences));
//		CFileMgr::Write(fid, (char*)&CFrontEndPreferences::cNetworkPlayerName, sizeof(CFrontEndPreferences::cNetworkPlayerName));
//		CFileMgr::Write(fid, (char*)&CFrontEndPreferences::cNetworkGameName, sizeof(CFrontEndPreferences::cNetworkGameName));

		CFileMgr::CloseFile(fid);
	}
#endif
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::LoadSettings()
// PURPOSE:	load settings on PC
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::LoadSettings()
{
	bool bSuccess = false;
#if RSG_PC
	FileHandle fid;

	CFileMgr::SetDir("");
	fid = CFileMgr::OpenFile("common:/DATA/SETTINGS.CFG");  // open for reading

	if (fid)
	{
//		CFileMgr::Read(fid, (char*)&CFrontEndPreferences::MenuPreferences, sizeof(CFrontEndPreferences::MenuPreferences));
//		CFileMgr::Read(fid, (char*)&CFrontEndPreferences::cNetworkPlayerName, sizeof(CFrontEndPreferences::cNetworkPlayerName));
//		CFileMgr::Read(fid, (char*)&CFrontEndPreferences::cNetworkGameName, sizeof(CFrontEndPreferences::cNetworkGameName));
		CFileMgr::CloseFile(fid);

		bSuccess = true;
	}
/*
	const char *playerName = 0;
	if(PARAM_playername.Get(playerName))
	{
		strcpy(CFrontEndPreferences::cNetworkPlayerName, playerName);
		strcpy(CFrontEndPreferences::cNetworkGameName,   playerName);
	}
*/
#endif
	return bSuccess;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CPauseMenu::CheckInput()
// PURPOSE:	check control input
/////////////////////////////////////////////////////////////////////////////////////
bool CPauseMenu::CheckInput(eFRONTEND_INPUT input, bool bPlaySound, u32 OverrideFlags, bool bOverrideFrontendState, bool bCheckForButtonJustPressed, bool bCheckDisabledInput/*=false*/)
{
	bOverrideFrontendState = true;

	if (bCheckForButtonJustPressed)
	{
		uiAssertf( (input == FRONTEND_INPUT_ACCEPT) || (input == FRONTEND_INPUT_BACK), "CPauseMenu::CheckInput - bCheckForButtonJustPressed only supported for FRONTEND_INPUT_ACCEPT and FRONTEND_INPUT_BACK so far");
	}
	// return out if warning message is set
	if (CWarningScreen::IsActive() && (!(OverrideFlags & CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE)) )
	{
		return false;
	}
	if (CGenericGameStorage::IsStorageDeviceBeingAccessed() && (!(OverrideFlags & CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE)) )
	{
		return false;
	}

	static u32 s_pressedDownTimer = fwTimer::GetSystemTimeInMilliseconds();
	static u32 s_lastGameFrame = 0;
	bool bOnlyCheckForDown = false;
	s32 interval = (input == FRONTEND_INPUT_UP) ? s_iLastRefireTimeUp : (input == FRONTEND_INPUT_DOWN) ? s_iLastRefireTimeDn : BUTTON_PRESSED_DOWN_INTERVAL;

	if (s_lastGameFrame != fwTimer::GetSystemFrameCount() && fwTimer::GetSystemTimeInMilliseconds() > (s_pressedDownTimer + interval))
	{
		bOnlyCheckForDown = true;
	}

	bool bInputTriggered = false;

	// We use GetNorm() but we convert back to the old value range as the frontend might be heavely dependent on this range.
	s32 iXAxis = 0;
	s32 iYAxis = 0;
	s32 iYAxisR = 0;
	s32 iXAxisR = 0;

	bool const c_ignoreDpad = (OverrideFlags & CHECK_INPUT_OVERRIDE_FLAG_IGNORE_D_PAD) != 0;

	// Read options to check for analogue input -- this is currently only used so that warning screens and social club menu can check disabled input -- extend to other buttons if necessary
	// The default read option for checking anlogue input is DEFAULT_OPTIONS
	ioValue::ReadOptions analogueInputOptions = ioValue::DEFAULT_OPTIONS;
	if(bCheckDisabledInput)
	{
		analogueInputOptions.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
	}

	if (!(OverrideFlags & CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS))
	{
		iXAxis  = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetNorm(analogueInputOptions) * 128.0f);
		iYAxis  = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendUpDown().GetNorm(analogueInputOptions) * 128.0f);
		iYAxisR = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendRStickUpDown().GetNorm(analogueInputOptions) * 128.0f);
		iXAxisR = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendRStickLeftRight().GetNorm(analogueInputOptions) * 128.0f);
	}

	const s32 iPreviousXAxis  = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetLastNorm(analogueInputOptions) * 128.0f);
	const s32 iPreviousYAxis  = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendUpDown().GetLastNorm(analogueInputOptions) * 128.0f);
	const s32 iPreviousXAxisR = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendRStickLeftRight().GetLastNorm(analogueInputOptions) * 128.0f);
	const s32 iPreviousYAxisR = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendRStickUpDown().GetLastNorm(analogueInputOptions) * 128.0f);

	// Read options to check for input -- this is currently only used so that warning screens and social club menu can check disabled input -- extend to other buttons if necessary
	// The default read option for checking button input is NO_DEAD_ZONE
	ioValue::ReadOptions buttonInputOptions = ioValue::NO_DEAD_ZONE;
	if(bCheckDisabledInput)
	{
		buttonInputOptions.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
	}

	switch (input)
	{
		case FRONTEND_INPUT_UP:
		{
			if (iXAxis > -FRONTEND_ANALOGUE_THRESHOLD && iXAxis < FRONTEND_ANALOGUE_THRESHOLD)
			{
				if (bOnlyCheckForDown)
				{
					if (iYAxis < -FRONTEND_ANALOGUE_THRESHOLD || ( CControlMgr::GetMainFrontendControl().GetFrontendUp().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && !c_ignoreDpad ) )
					{
						bInputTriggered = true;
					}
				} 
				else if ((iPreviousYAxis > -FRONTEND_ANALOGUE_THRESHOLD && iYAxis < -FRONTEND_ANALOGUE_THRESHOLD) || 
					(CControlMgr::GetMainFrontendControl().GetFrontendUp().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && CControlMgr::GetMainFrontendControl().GetFrontendUp().WasUp(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && !c_ignoreDpad ))
				{
					bInputTriggered = true;
				}
			}

			if (s_lastGameFrame != fwTimer::GetSystemFrameCount())
			{
				// can't just do bInputTriggered because we may be waiting for an up
				if( iYAxis < -FRONTEND_ANALOGUE_THRESHOLD || ( CControlMgr::GetMainFrontendControl().GetFrontendUp().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && !c_ignoreDpad ) )
				{
					if( bInputTriggered )
					{
						s_iLastRefireTimeUp = rage::Max(s_iLastRefireTimeUp-BUTTON_PRESSED_REFIRE_ATTRITION, BUTTON_PRESSED_REFIRE_MINIMUM);
					}
				}
				else
				{
					s_iLastRefireTimeUp = BUTTON_PRESSED_DOWN_INTERVAL;
				}
			}

			break;
		}

		case FRONTEND_INPUT_DOWN:
		{
			if (iXAxis > -FRONTEND_ANALOGUE_THRESHOLD && iXAxis < FRONTEND_ANALOGUE_THRESHOLD)
			{
				if (bOnlyCheckForDown)
				{
					if (iYAxis > FRONTEND_ANALOGUE_THRESHOLD || ( CControlMgr::GetMainFrontendControl().GetFrontendDown().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && !c_ignoreDpad ) )
					{
						bInputTriggered = true;
					}
				}
				else if ((iPreviousYAxis < FRONTEND_ANALOGUE_THRESHOLD && iYAxis > FRONTEND_ANALOGUE_THRESHOLD) || 
					(CControlMgr::GetMainFrontendControl().GetFrontendDown().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && CControlMgr::GetMainFrontendControl().GetFrontendDown().WasUp(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && !c_ignoreDpad  ))
				{
					bInputTriggered = true;
				}
			}

			if (s_lastGameFrame != fwTimer::GetSystemFrameCount())
			{
				// can't just do bInputTriggered because we may be waiting for an up
				if( iYAxis > FRONTEND_ANALOGUE_THRESHOLD || ( CControlMgr::GetMainFrontendControl().GetFrontendDown().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && !c_ignoreDpad ) )
				{
					if( bInputTriggered )
					{
						s_iLastRefireTimeDn = rage::Max(s_iLastRefireTimeDn-BUTTON_PRESSED_REFIRE_ATTRITION, BUTTON_PRESSED_REFIRE_MINIMUM);
					}
				}
				else
				{
					s_iLastRefireTimeDn = BUTTON_PRESSED_DOWN_INTERVAL;
				}
			}

			break;
		}

		case FRONTEND_INPUT_LEFT:
		{
			if (iYAxis > -FRONTEND_ANALOGUE_THRESHOLD && iYAxis < FRONTEND_ANALOGUE_THRESHOLD)
			{
				if (bOnlyCheckForDown)
				{
					if (iXAxis < -FRONTEND_ANALOGUE_THRESHOLD || ( CControlMgr::GetMainFrontendControl().GetFrontendLeft().IsDown() && !c_ignoreDpad ) )
					{
						bInputTriggered = true;
					}
				}
				else if ((iPreviousXAxis > -FRONTEND_ANALOGUE_THRESHOLD && iXAxis < -FRONTEND_ANALOGUE_THRESHOLD) || 
					(CControlMgr::GetMainFrontendControl().GetFrontendLeft().IsDown() && CControlMgr::GetMainFrontendControl().GetFrontendLeft().WasUp() && !c_ignoreDpad ) )
				{
					bInputTriggered = true;
				}
			}

			break;
		}

		case FRONTEND_INPUT_RIGHT:
		{
			if (iYAxis > -FRONTEND_ANALOGUE_THRESHOLD && iYAxis < FRONTEND_ANALOGUE_THRESHOLD)
			{
				if (bOnlyCheckForDown)
				{
					if (iXAxis > FRONTEND_ANALOGUE_THRESHOLD || ( CControlMgr::GetMainFrontendControl().GetFrontendRight().IsDown() && !c_ignoreDpad ) )
					{
						bInputTriggered = true;
					}

				} 
				else if ( (iPreviousXAxis < FRONTEND_ANALOGUE_THRESHOLD && iXAxis > FRONTEND_ANALOGUE_THRESHOLD) || 
					(CControlMgr::GetMainFrontendControl().GetFrontendRight().IsDown() && CControlMgr::GetMainFrontendControl().GetFrontendRight().WasUp() && !c_ignoreDpad ) )
				{
					bInputTriggered = true;
				}
			}

			break;
		}

		case FRONTEND_INPUT_RUP:
		{
			if (bOnlyCheckForDown)
			{
				if (iYAxisR < -FRONTEND_ANALOGUE_THRESHOLD)
				{
					bInputTriggered = true;
				}
			}
			else if ( iPreviousYAxisR > -FRONTEND_ANALOGUE_THRESHOLD && iYAxisR < -FRONTEND_ANALOGUE_THRESHOLD )
			{
				bInputTriggered = true;
			}

			break;
		}

		case FRONTEND_INPUT_RDOWN:
		{
			if (bOnlyCheckForDown)
			{
				if (iYAxisR > FRONTEND_ANALOGUE_THRESHOLD)
				{
					bInputTriggered = true;
				}
			} else if ( iPreviousYAxisR < FRONTEND_ANALOGUE_THRESHOLD && iYAxisR > FRONTEND_ANALOGUE_THRESHOLD )
			{
				bInputTriggered = true;
			}

			break;
		}

		case FRONTEND_INPUT_RLEFT:
		{
			if (bOnlyCheckForDown)
			{
				if (iXAxisR < -FRONTEND_ANALOGUE_THRESHOLD)
				{
					bInputTriggered = true;
				}
			} else if ( iPreviousXAxisR > -FRONTEND_ANALOGUE_THRESHOLD && iXAxisR < -FRONTEND_ANALOGUE_THRESHOLD )
			{
				bInputTriggered = true;
			}

			break;
		}

		case FRONTEND_INPUT_RRIGHT:
		{
			if (bOnlyCheckForDown)
			{
				if (iXAxisR > FRONTEND_ANALOGUE_THRESHOLD)
				{
					bInputTriggered = true;
				}
			} else if ( iPreviousXAxisR < FRONTEND_ANALOGUE_THRESHOLD && iXAxisR > FRONTEND_ANALOGUE_THRESHOLD )
			{
				bInputTriggered = true;
			}

			break;
		}

		case FRONTEND_INPUT_ACCEPT:
		{
			bool bAcceptHasBeenPressed = false;

			if (bCheckForButtonJustPressed)
			{
				if (CControlMgr::GetMainFrontendControl().GetFrontendAccept().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && CControlMgr::GetMainFrontendControl().GetFrontendAccept().WasUp(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions))
				{
					bAcceptHasBeenPressed = true;
				}
			}
			else
			{
				if (CControlMgr::GetMainFrontendControl().GetFrontendAccept().IsUp(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && CControlMgr::GetMainFrontendControl().GetFrontendAccept().WasDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions)) 
				{
					bAcceptHasBeenPressed = true;
				}
			}

			if (bAcceptHasBeenPressed)
			{
				bInputTriggered = true;
			}
			break;
		}

		case FRONTEND_INPUT_X:
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendX().IsUp() && CControlMgr::GetMainFrontendControl().GetFrontendX().WasDown()) 
			{
				bInputTriggered = true;
			}
			break;
		}

		case FRONTEND_INPUT_Y:
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendY().IsUp() && CControlMgr::GetMainFrontendControl().GetFrontendY().WasDown()) 
			{
				bInputTriggered = true;
			}
			break;
		}

		case FRONTEND_INPUT_BACK:
		{
			const ioValue& cancelInput = CControlMgr::GetMainFrontendControl().GetFrontendCancel();
			if (bCheckForButtonJustPressed)
			{
				if (cancelInput.IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && cancelInput.WasUp(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions))
				{
					bInputTriggered = true;
				}
			}
			else
			{
				if (cancelInput.IsUp(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && cancelInput.WasDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions)) 
				{
					bInputTriggered = true;
				}
			}
#if RSG_PC
			// allowed fall through
		}
		
		case FRONTEND_INPUT_CURSOR_BACK:
		{
			const ioValue& cancelInput = CControlMgr::GetMainFrontendControl().GetFrontendCancel();
			// no implicit mouse conversion for the warning screens
			if( !CWarningScreen::IsActive() && cancelInput.IsEnabled() )
			{
				const ioValue& cursorCancel = CControlMgr::GetMainFrontendControl().GetCursorCancel();
				if (bCheckForButtonJustPressed)
				{
					if (cursorCancel.IsDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && cursorCancel.WasUp(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions))
					{
						bInputTriggered = true;
					}
				}
				else
				{
					if (cursorCancel.IsUp(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions) && cursorCancel.WasDown(ioValue::BUTTON_DOWN_THRESHOLD, buttonInputOptions)) 
					{
						bInputTriggered = true;
					}
				}
			}

#endif

			break;
		}

		case FRONTEND_INPUT_START:
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendEnterExit().IsReleased())
			{
				bInputTriggered = true;
				break;
			}

			if (CControlMgr::GetMainFrontendControl().GetFrontendEnterExitAlternate().IsReleased()) 
			{
				bInputTriggered = true;
				break;
			}


			break;
		}

		case FRONTEND_INPUT_SPECIAL_UP:
		{
			//if (CControlMgr::GetMainFrontendControl().GetPreviousYAxisR() > -FRONTEND_ANALOGUE_THRESHOLD && iYAxisR < -FRONTEND_ANALOGUE_THRESHOLD)
			if (iYAxisR < -FRONTEND_ANALOGUE_THRESHOLD)
			{
				bInputTriggered = true;
			}
			break;
		}

		case FRONTEND_INPUT_SPECIAL_DOWN:
		{
			//if (CControlMgr::GetMainFrontendControl().GetPreviousYAxisR() < FRONTEND_ANALOGUE_THRESHOLD && iYAxisR > FRONTEND_ANALOGUE_THRESHOLD)
			if (iYAxisR > FRONTEND_ANALOGUE_THRESHOLD)
			{
				bInputTriggered = true;
			}
			break;
		}

		case FRONTEND_INPUT_RT:
		{
			if (bOnlyCheckForDown)
			{
				if (CControlMgr::GetMainFrontendControl().GetFrontendRT().IsDown())
				{
					bInputTriggered = true;
				}
			} else 	if (CControlMgr::GetMainFrontendControl().GetFrontendRT().IsDown() && CControlMgr::GetMainFrontendControl().GetFrontendRT().WasUp()) 
			{
				bInputTriggered = true;
			}

			break;
		}

		case FRONTEND_INPUT_LT:
		{
			if (bOnlyCheckForDown)
			{
				if (CControlMgr::GetMainFrontendControl().GetFrontendLT().IsDown())
				{
					bInputTriggered = true;
				}
			} else if (CControlMgr::GetMainFrontendControl().GetFrontendLT().IsDown() && CControlMgr::GetMainFrontendControl().GetFrontendLT().WasUp()) 
			{
				bInputTriggered = true;
			}

			break;
		}
		case FRONTEND_INPUT_LB:
		{
			if (bOnlyCheckForDown)
			{
				if (CControlMgr::GetMainFrontendControl().GetFrontendLB().IsDown())
				{
					bInputTriggered = true;
				}
			} else if (CControlMgr::GetMainFrontendControl().GetFrontendLB().IsDown() && CControlMgr::GetMainFrontendControl().GetFrontendLB().WasUp()) 
			{
				bInputTriggered = true;
			}

			break;
		}

		case FRONTEND_INPUT_RB:
		{
#if RSG_PC
			if (CControlMgr::GetMainFrontendControl().GetFrontendSelect().IsDown())
			{
				// The SCUI hotkey is SELECT+RB. If the Select key is triggered, ignore this input
			}
			else
#endif
			if (bOnlyCheckForDown)
			{
				if (CControlMgr::GetMainFrontendControl().GetFrontendRB().IsDown())
				{
					bInputTriggered = true;
				}
			} 
			else if (CControlMgr::GetMainFrontendControl().GetFrontendRB().IsDown() && CControlMgr::GetMainFrontendControl().GetFrontendRB().WasUp()) 
			{
				bInputTriggered = true;
			}

			break;
		}

		case FRONTEND_INPUT_RT_SPECIAL:
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendRT().IsDown()) 
			{
				bInputTriggered = true;
			}
			break;
		}

		case FRONTEND_INPUT_LT_SPECIAL:
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendLT().IsDown()) 
			{
				bInputTriggered = true;
			}
			break;
		}

		case FRONTEND_INPUT_RSTICK_LEFT:
		{
			if (iXAxisR > FRONTEND_ANALOGUE_THRESHOLD)
			{
				bInputTriggered = true;
			}
		}
		break;

		case FRONTEND_INPUT_RSTICK_RIGHT:
		{
			if (iXAxisR < -FRONTEND_ANALOGUE_THRESHOLD)
			{
				bInputTriggered = true;
			}
		}
		break;

		case FRONTEND_INPUT_SELECT:
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendSelect().IsUp() && CControlMgr::GetMainFrontendControl().GetFrontendSelect().WasDown()) 
			{
				bInputTriggered = true;
			}
		}
		break;

		case FRONTEND_INPUT_R3:
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendRS().IsUp() && CControlMgr::GetMainFrontendControl().GetFrontendRS().WasDown()) 
			{
				bInputTriggered = true;
			}
		}
		break;

		case FRONTEND_INPUT_L3:
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendLS().IsUp() && CControlMgr::GetMainFrontendControl().GetFrontendLS().WasDown()) 
			{
				bInputTriggered = true;
			}
		}
		break;

		case FRONTEND_INPUT_CURSOR_ACCEPT:
		{
			if (CControlMgr::GetMainFrontendControl().GetCursorAccept().IsUp() && CControlMgr::GetMainFrontendControl().GetCursorAccept().WasDown()) 
			{
				bInputTriggered = true;
			}
		}
			break;

		default:
		{
			uiAssertf(0, "INVALID FRONTEND INPUT VALUE");
		}
	}

	if (bInputTriggered)
	{
		if (s_lastGameFrame != fwTimer::GetSystemFrameCount())
		{
			s_pressedDownTimer = fwTimer::GetSystemTimeInMilliseconds();  // reset the timer to check for holding button down
			s_lastGameFrame = fwTimer::GetSystemFrameCount();
		}

		// deal with any sound effects:
		if (bPlaySound)
		{
			PlayInputSound(input);
		}

#if RSG_PC
		CMousePointer::SetKeyPressed();

		if(!CMousePointer::HasMouseInputOccurred() && GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).IsActive())
		{
			GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("CLEAR_ALL_HOVER");
		}
#endif // #if RSG_PC
	}

	

	return (bInputTriggered);
}

void CPauseMenu::PlayInputSound(eFRONTEND_INPUT input)
{
	switch (input)
	{
	case FRONTEND_INPUT_UP:
	case FRONTEND_INPUT_DOWN:
		{
			g_FrontendAudioEntity.PlaySound("NAV_UP_DOWN",s_HudFrontendSoundset);
			break;
		}

	case FRONTEND_INPUT_LEFT:
	case FRONTEND_INPUT_RIGHT:
		{
			if (IsNavigatingContent())
				g_FrontendAudioEntity.PlaySound("NAV_LEFT_RIGHT",s_HudFrontendSoundset);
			else
				g_FrontendAudioEntity.PlaySound("TOGGLE_ON",s_HudFrontendSoundset);
			break;
		}

	case FRONTEND_INPUT_RUP:
	case FRONTEND_INPUT_RDOWN:
		{
			g_FrontendAudioEntity.PlaySound("NAV_UP_DOWN",s_HudFrontendSoundset);
			break;
		}

	case FRONTEND_INPUT_RLEFT:
	case FRONTEND_INPUT_RRIGHT:
	case FRONTEND_INPUT_X:
	case FRONTEND_INPUT_Y:
	case FRONTEND_INPUT_LB:
	case FRONTEND_INPUT_RB:
	case FRONTEND_INPUT_RT_SPECIAL:
	case FRONTEND_INPUT_LT_SPECIAL:
	case FRONTEND_INPUT_RSTICK_LEFT:
	case FRONTEND_INPUT_RSTICK_RIGHT:
	case FRONTEND_INPUT_L3:
	case FRONTEND_INPUT_RT:
		{
			g_FrontendAudioEntity.PlaySound("TOGGLE_ON",s_HudFrontendSoundset);
			break;
		}

	case FRONTEND_INPUT_ACCEPT:
		{
			if (IsNavigatingContent())
				g_FrontendAudioEntity.PlaySound("SELECT",s_HudFrontendSoundset);
			else
				g_FrontendAudioEntity.PlaySound("TOGGLE_ON",s_HudFrontendSoundset);
			break;
		}

	case FRONTEND_INPUT_BACK:
		{
			g_FrontendAudioEntity.PlaySound("BACK",s_HudFrontendSoundset);
			break;
		}

	case FRONTEND_INPUT_SPECIAL_UP:
	case FRONTEND_INPUT_SPECIAL_DOWN:
		{
			g_FrontendAudioEntity.PlaySound("HIGHLIGHT_NAV_UP_DOWN",s_HudFrontendSoundset);
			break;
		}

	case FRONTEND_INPUT_SELECT:
		{
			if (IsNavigatingContent())
				g_FrontendAudioEntity.PlaySound("SELECT",s_HudFrontendSoundset);
			else
				g_FrontendAudioEntity.PlaySound("TOGGLE_ON",s_HudFrontendSoundset);

			break;
		}

	default:
		{
			break;
		}
	}

	if( input != FRONTEND_INPUT_MAX )
	{
		g_FrontendAudioEntity.NotifyFrontendInput();
	}
}

bool CPauseMenu::ShouldPlayNavigationSound(bool navUp)
{
	bool retVal = sm_bNavigatingContent && !sm_bMaxPayneMode;

	if( SUIContexts::IsActive( UIATSTRINGHASH("Sound_NoUpDown",0x91ec312b) ) )
		return false;

	if(retVal)
	{
		if( IsCurrentScreenValid() )
		{
			if( GetCurrentScreenData().IsContextMenuOpen() )
			{
				retVal = true;
			}
			else if( GetCurrentScreenData().HasDynamicMenu())
			{
				CMenuBase* pMenu = GetCurrentScreenData().GetDynamicMenu();
				retVal = !pMenu->IsShowingFullWarningColumn() && pMenu->ShouldPlayNavigationSound(navUp);
			}
		}
	}

	return retVal;
}

void CPauseMenu::PlaySound(const char *pSoundString)
{
	if (pSoundString)
	{
		g_FrontendAudioEntity.PlaySound(pSoundString,s_HudFrontendSoundset);
		g_FrontendAudioEntity.NotifyFrontendInput();
	}
}

bool CPauseMenu::SetMenuPreference(s32 iPref, s32 iValue)
{
	if (iValue != CPauseMenu::GetMenuPreference(iPref))
	{
		uiDebugf1("PAUSEMENU: Menu Preference item %d changed from %d to %d", iPref, CPauseMenu::GetMenuPreference(iPref), iValue);

		sm_MenuPref[iPref] = iValue;

		return true;  // true, as it changed
	}

	return false;   // false - nothing changed
}

void CPauseMenu::EnterSocialClub()
{
	SocialClubMenu::CacheOffPMState();

	if(IsActive(PM_SkipSocialClub))
	{
		SetClosingAction(CA_StartSocialClub);
		sm_bUnpauseGameOnMenuClose = false;
		
		Close();
	}
	else
	{
		SocialClubMenu::Open();
	}
}

void CPauseMenu::ActivateSocialClubContext()
{
	if(!CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub())
	{
		SUIContexts::Activate(SC_CONTEXT_BUTTON);
	}
	else if(!CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate())
	{
		SUIContexts::Activate(SC_UPDATE_CONTEXT_BUTTON);
	}
}

void CPauseMenu::DeactivateSocialClubContext()
{
	SUIContexts::Deactivate(SC_CONTEXT_BUTTON);
	SUIContexts::Deactivate(SC_UPDATE_CONTEXT_BUTTON);
}

void CPauseMenu::RefreshSocialClubContext()
{
	bool hasCreds = NetworkInterface::HasValidRosCredentials();

	SUIContexts::SetActive(SC_CONTEXT_BUTTON, hasCreds && !CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub());
	SUIContexts::SetActive(SC_UPDATE_CONTEXT_BUTTON, hasCreds && !CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate());
}

void CPauseMenu::SetCurrentMissionActive(bool bActive)
{
	sm_bCurrentlyOnAMission = bActive;
}

void CPauseMenu::SetCurrentMissionLabel(const char* missionNameLabel, bool bIsLiteralString)
{
	formatf(sm_MissionNameLabel, MAX_LENGTH_OF_MISSION_TITLE, missionNameLabel);
	sm_bMissionLabelIsALiteralString = bIsLiteralString;
}

void CPauseMenu::SetCurrentMissionDescription(bool bActive, const char *pString1, const char *pString2, const char *pString3, const char *pString4, const char *pString5, const char *pString6, const char *pString7, const char *pString8)
{
	sm_bMissionDescriptionIsActive = bActive;

	safecpy(sm_MissionDescriptionString, "", NELEM(sm_MissionDescriptionString));

	bool bAtLeastOneValidString = false;
	if (pString1 && bActive)
	{
		bAtLeastOneValidString = true;
		safecat(sm_MissionDescriptionString, pString1, NELEM(sm_MissionDescriptionString));
	}

	if (pString2 && bActive)
	{
		bAtLeastOneValidString = true;
		safecat(sm_MissionDescriptionString, pString2, NELEM(sm_MissionDescriptionString));
	}

	if (pString3 && bActive)
	{
		bAtLeastOneValidString = true;
		safecat(sm_MissionDescriptionString, pString3, NELEM(sm_MissionDescriptionString));
	}

	if (pString4 && bActive)
	{
		bAtLeastOneValidString = true;
		safecat(sm_MissionDescriptionString, pString4, NELEM(sm_MissionDescriptionString));
	}

	if (pString5 && bActive)
	{
		bAtLeastOneValidString = true;
		safecat(sm_MissionDescriptionString, pString5, NELEM(sm_MissionDescriptionString));
	}

	if (pString6 && bActive)
	{
		bAtLeastOneValidString = true;
		safecat(sm_MissionDescriptionString, pString6, NELEM(sm_MissionDescriptionString));
	}

	if (pString7 && bActive)
	{
		bAtLeastOneValidString = true;
		safecat(sm_MissionDescriptionString, pString7, NELEM(sm_MissionDescriptionString));
	}

	if (pString8 && bActive)
	{
		bAtLeastOneValidString = true;
		safecat(sm_MissionDescriptionString, pString8, NELEM(sm_MissionDescriptionString));
	}

	if (bActive && !bAtLeastOneValidString)
	{
		uiAssertf(0, "CPauseMenu::SetCurrentMissionDescription called with TRUE but all four strings are NULL");
		sm_bMissionDescriptionIsActive = false;
	}
}

void CPauseMenu::DisplayNewGameAlertMessage()
{
//	CPauseMenu::SetBusySpinner(false, PM_COLUMN_MIDDLE_RIGHT);
	CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("PM_NEW_GAME"), TheText.Get("PM_PANE_NEW"));
}

void CPauseMenu::DisplayImportAlertMessage()
{
	CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("PM_PANE_IMP"), TheText.Get("PM_PANE_IMP_HELP"));
}

void CPauseMenu::DisplayCreditsMessage(MenuScreenId iMenuScreen)
{
	if (iMenuScreen == MENU_UNIQUE_ID_CREDITS_LEGAL)
	{
		CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("UI_FLOW_SP_CL_D"), TheText.Get("UI_FLOW_SP_CL"));
	}
	else if (iMenuScreen == MENU_UNIQUE_ID_LEGAL)
	{
		CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("UI_FLOW_SP_L_D"), TheText.Get("UI_FLOW_SP_L"));
	}
	else
	{
		CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("UI_FLOW_SP_C_D"), TheText.Get("UI_FLOW_SP_C"));
	}	
}

#if RSG_PC
void CPauseMenu::DisplayQuitGameAlertMessage()
{
	CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("PM_QUIT_GAME"), TheText.Get("PM_PANE_QUIT"));
}

void CPauseMenu::TriggerStorePC()
{
	//This is split because whilst it is a simple call for now, I dont trust extra logic not to sneak in here.
	cStoreScreenMgr::OpenPC();
}
#endif

void CPauseMenu::SetMaxPayneMode(const bool bNewSetting)
{
	sm_bMaxPayneMode = bNewSetting;
	SUIContexts::SetActive(UIATSTRINGHASH("MENU_OFF",0x8868d7fa), sm_bMaxPayneMode);
	PlayInputSound(FRONTEND_INPUT_Y);
	RedrawInstructionalButtons();

	//if(GetCurrentMenuVersionHasFlag(kFadesIn) )
	//{
	//	if( !sm_bMaxPayneMode )
	//		PAUSEMENUPOSTFXMGR.StartFadeIn();
	//	else
	//		PAUSEMENUPOSTFXMGR.StartFadeOut();
	//}
}

void CPauseMenu::RemoveAllMovies()
{
	// these must be removed in this order so that all the children get removed 1st, then the shared components, then the parent
	for (s32 iMovieCount = sm_iChildrenMovies.GetMaxCount()-1; iMovieCount>=0; --iMovieCount)
		sm_iChildrenMovies[iMovieCount].Remove();

	for (s32 iMovieCount = sm_iBaseMovieId.GetMaxCount()-1; iMovieCount>=0; --iMovieCount)
		sm_iBaseMovieId[iMovieCount].RemoveMovie();
}

//////////////////////////////////////////////////////////////////////////


bool CCutsceneMenu::UpdateInput(s32 eInput)
{
#if RSG_PC
	CControlMgr::GetMainFrontendControl().DisableInput(INPUT_FRONTEND_PAUSE_ALTERNATE);
#endif

	if( eInput == PAD_CROSS || 
		// LB B* 1563405: If you press Pause again enter the real pause menu
		// NOTE: B* 2050323: eInput will be PAD_CIRCLE as INPUT_FRONTEND_PAUSE and INPUT_FRONTEND_BACK (AKA PAD_CIRCLE) are both on the escape key.
		( (KEYBOARD_MOUSE_ONLY(eInput == PAD_CIRCLE ||) eInput == PAD_NO_BUTTON_PRESSED) &&
		CPauseMenu::CheckInput(FRONTEND_INPUT_START, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) )
		)
	{
		CPauseMenu::OpenCorrectMenu(true);
		return true;
	}

	if(eInput == PAD_CIRCLE )
	{
		CPauseMenu::Close();
		return true;
	}

	return false;
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CPauseMenu::AllowSavegamesToBeExported()
{
	if (!NetworkInterface::IsMigrationAvailable())
	{
		uiDebugf1("CPauseMenu::AllowSavegamesToBeExported - returning false because NetworkInterface::IsMigrationAvailable() returned false");
		return false;
	}

	if (!CGenericGameStorage::IsSaveGameMigrationCloudTextLoaded())
	{
		uiDebugf1("CPauseMenu::AllowSavegamesToBeExported - returning false because CGenericGameStorage::IsSaveGameMigrationCloudTextLoaded() returned false");
		return false;
	}

	// 3. Should I check that the cloud query for any existing uploaded or migrated savegame has finished?
	// 4. I did consider returning false if we're in Multiplayer. 
	//		I don't think that's necessary because it seems that the entire Game menu is hidden during MP
	// 5. I also considered hiding the Upload menu if there are no savegames. 
	//		It seems to work okay as it is. It behaves in the same way as the Load Game menu when there are no saves.
	return true;
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
bool CPauseMenu::AllowSavegamesToBeImported()
{
	return true;
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


// eof
