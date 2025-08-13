/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PauseMenu.h
// PURPOSE : header for PauseMenu.cpp
// AUTHOR  : Derek Payne
// STARTED : 03/02/2011
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_PAUSEMENU_H_
#define INC_PAUSEMENU_H_

// Rage headers
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "system/language.h"


// Game headers
#include "frontend/input/uiInputEnums.h"
#include "Frontend/PauseMenuData.h"
#include "Network/Live/NetworkClan.h"

#include "audio/dynamicmixer.h"
#include "control/replay/ReplaySettings.h"
#include "control/replay/File/device_replay.h"
#include "Frontend/ContextMenu.h"
#include "Frontend/CMenuBase.h"
#include "Frontend/MiniMapCommon.h"  // MAX_BLIP_NAME_SIZE
#include "Frontend/Scaleform/ScaleFormMgr.h" // ScaleformMovieWrapper
#include "Frontend/ScaleformMenuHelper.h"	// PM_COLUMNS
#include "Frontend/TimeWarper.h"
#include "Frontend/UIMenuPed.h"
#include "Network/Live/FriendClanData.h"
#include "renderer/sprite2d.h"
#include "SaveLoad/savegame_queued_operations.h"
#include "Scaleform/scaleform.h"  // GFxValue
#include "text/messages.h"
#include "text/TextFile.h"
#include "stats/statsdatamgr.h"
#include "SimpleTimer.h"
#include "system/SettingsManager.h"
#include "System/control.h"

// Forward declarations
namespace rage
{
	class parTreeNode;
};

class CWarningMessage;
class sUIStatData;
#define UI_DEBUG_BANK_NAME	"ui"  // UI widget heading

#define __PRELOAD_PAUSE_MENU  (RSG_ORBIS || RSG_DURANGO || RSG_PC)

// map limit sizes:
#define PAUSEMAP_MAX_ZOOMED_IN	(35.0f)
#define PAUSEMAP_MAX_ZOOMED_OUT	(2.7f)
#define PAUSEMAP_START_SIZE		(3.5f)
#define PAUSEMAP_ZOOM_INTERIOR	(300.0f)
#define PAUSEMAP_ZOOM_GOLF_COURSE	(100.0f)
#define GALLERYMAP_START_SIZE	(30.0f)

#define __FRONTEND_PREFS_FOR_PLAYER_APPEARANCE	(__BANK)
#define __6AXISACTIVE (__PPU || RSG_ORBIS)

typedef u32 eFRONTEND_MENU_VERSION;

#define FE_MENU_VERSION_INVALID		            -1 // TODO_UI - This is technically wrong for hashs, as invalid should be zero. Can't change it due to script dependencies though...
#define FE_MENU_VERSION_SP_PAUSE                0xd528c7e2 //ATSTRINGHASH("FE_MENU_VERSION_SP_PAUSE",0xd528c7e2)  
#define FE_MENU_VERSION_LANDING_MENU            0xc28059c6 //ATSTRINGHASH("FE_MENU_VERSION_LANDING_MENU",0xc28059c6)
#define FE_MENU_VERSION_MP_PAUSE	            0xba33adb3 //ATSTRINGHASH("FE_MENU_VERSION_MP_PAUSE", 0xba33adb3 )
#define FE_MENU_VERSION_SAVEGAME	            0x809275cc //ATSTRINGHASH("FE_MENU_VERSION_SAVEGAME", 0x809275cc )
#define FE_MENU_VERSION_CREATOR_PAUSE           0x67bb8634 //ATSTRINGHASH("FE_MENU_VERSION_CREATOR_PAUSE", 0x67bb8634 )
#define FE_MENU_VERSION_CUTSCENE_PAUSE          0x21090930 //ATSTRINGHASH("FE_MENU_VERSION_CUTSCENE_PAUSE", 0x21090930 )
#define FE_MENU_VERSION_MP_CHARACTER_CREATION   0x5371ec95 //ATSTRINGHASH("FE_MENU_VERSION_MP_CHARACTER_CREATION", 0x5371ec95 )
#define FE_MENU_VERSION_MP_CHARACTER_SELECT     0x702edd8b //ATSTRINGHASH("FE_MENU_VERSION_MP_CHARACTER_SELECT", 0x702edd8b )

#define PAUSE_MENU_FEED_INVISIBLE_FRAMES 10	// Number of frames to keep the feed hidden after unpausing.

#define CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE				(1<<1)
#define CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE				(1<<2)
#define CHECK_INPUT_OVERRIDE_FLAG_RESTART_SAVED_GAME_STATE		(1<<3)
#define CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS		(1<<4)
#define CHECK_INPUT_OVERRIDE_FLAG_IGNORE_D_PAD					(1<<5)

#define MAX_PAUSE_MENU_CHILD_MOVIES (15)

enum eNUM_PAUSE_MENU_MOVIES  // the order here is important as it the movies are dependent on each other and need to be removed in the correct order of the enum
{
	PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS = 0,
	PAUSE_MENU_MOVIE_HEADER,
	PAUSE_MENU_MOVIE_SHARED_COMPONENTS,
	PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_START,
	PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_2,
	PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_3,
	PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_4,
	PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_5,
	PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_END,
	PAUSE_MENU_MOVIE_CONTENT,

	MAX_PAUSE_MENU_BASE_MOVIES
};

#define MAX_EXTRA_SHARED_MOVIES (PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_END - PAUSE_MENU_MOVIE_EXTRA_SHARED_COMPONENTS_START)

#define MAX_PAUSE_MENU_MOVIES (MAX_PAUSE_MENU_BASE_MOVIES + MAX_PAUSE_MENU_CHILD_MOVIES)

enum eMenuPref
{
	PREF_INVALID = -1,
	PREF_CONTROL_CONFIG = 0,
	PREF_CONTROL_CONFIG_FPS,
	PREF_VIBRATION,
	PREF_TARGET_CONFIG,
	PREF_INVERT_LOOK, // NOTE: this will be one of eMenuInvertPref above!
	PREF_CONTROLLER_SENSITIVITY,
	PREF_CONTROLLER_LIGHT_EFFECT,
	PREF_SFX_VOLUME,
	PREF_MUSIC_VOLUME,
	PREF_GPS_SPEECH,
	PREF_SPEAKER_OUTPUT,
	PREF_INTERACTIVE_MUSIC,
	PREF_RADIO_STATION,
	PREF_VOICE_OUTPUT,
	PREF_GAMMA,
	PREF_PCGAMEPAD,
	PREF_CAMERA_HEIGHT,
	PREF_FEED_PHONE,
	PREF_FEED_STATS,
	PREF_FEED_CREW,
	PREF_FEED_FRIENDS,
	PREF_FEED_SOCIAL,
	PREF_FEED_STORE,
	PREF_FEED_TOOLTIP,
	PREF_FEED_DELAY,
	PREF_DOF,
	PREF_SKFX,
	PREF_SUBTITLES,
	PREF_RADAR_MODE,
	PREF_DISPLAY_HUD,
	PREF_RETICULE,
	PREF_RETICULE_SIZE,
	PREF_DISPLAY_GPS,
	PREF_SAFEZONE_SIZE,
	PREF_CURRENT_LANGUAGE,
	PREF_AUTOSAVE,
	PREF_CINEMATIC_SHOOTING,
	PREF_MUSIC_VOLUME_IN_MP,
	PREF_FACEBOOK_UPDATES,

	PREF_PREVIOUS_LANGUAGE,
	PREF_SYSTEM_LANGUAGE,
	PREF_HDR,

	PREF_DIAG_BOOST,
	PREF_VOICE_SPEAKERS,
	PREF_SS_FRONT,
	PREF_SS_REAR,

	PREF_SNIPER_ZOOM,
	PREF_CONTROLS_CONTEXT,

	PREF_MP_CAMERA_ZOOM_ON_FOOT,
	PREF_MP_CAMERA_ZOOM_IN_VEHICLE,
	PREF_MP_CAMERA_ZOOM_ON_BIKE,
	PREF_MP_CAMERA_ZOOM_IN_BOAT,
	PREF_MP_CAMERA_ZOOM_IN_AIRCRAFT,
	PREF_MP_CAMERA_ZOOM_IN_HELI,

	PREF_STARTUP_FLOW,

	PREF_VID_SCREEN_TYPE,
	PREF_VID_RESOLUTION,
	PREF_VID_REFRESH,
	PREF_VID_ADAPTER,
	PREF_VID_MONITOR,
	PREF_VID_ASPECT,
	PREF_VID_VSYNC,
	PREF_VID_STEREO,
	PREF_VID_STEREO_CONVERGENCE,
	PREF_VID_STEREO_SEPARATION,
	PREF_VID_PAUSE_ON_FOCUS_LOSS,

	PREF_GFX_DXVERSION,

	PREF_GFX_TEXTURE_QUALITY,
	PREF_GFX_SHADER_QUALITY,
	PREF_GFX_SHADOW_QUALITY,
	PREF_GFX_REFLECTION_QUALITY,
	PREF_GFX_REFLECTION_MSAA,
	PREF_GFX_WATER_QUALITY,
	PREF_GFX_PARTICLES_QUALITY,
	PREF_GFX_GRASS_QUALITY,
	PREF_GFX_SHADOW_SOFTNESS,
	PREF_GFX_CITY_DENSITY,
	PREF_GFX_POP_VARIETY,
	PREF_GFX_FXAA,
	PREF_GFX_MSAA,
	PREF_GFX_SCALING,
	PREF_GFX_TXAA,
	PREF_GFX_ANISOTROPIC_FILTERING,
	PREF_GFX_AMBIENT_OCCLUSION,
	PREF_GFX_TESSELLATION,
	PREF_GFX_POST_FX,
	PREF_GFX_DOF,
	PREF_GFX_MB_STRENGTH,
	PREF_GFX_DIST_SCALE,

	PREF_ADV_GFX_LONG_SHADOWS,
	PREF_ADV_GFX_ULTRA_SHADOWS,
	PREF_ADV_GFX_HD_FLIGHT,
	PREF_ADV_GFX_MAX_LOD,
	PREF_ADV_GFX_SHADOWS_DIST_MULT,

	PREF_MEMORY_BAR,
	PREF_GFX_VID_OVERRIDE,

	PREF_VOICE_ENABLE,
	PREF_VOICE_OUTPUT_DEVICE,
	PREF_VOICE_OUTPUT_VOLUME,
	PREF_VOICE_SOUND_VOLUME,
	PREF_VOICE_MUSIC_VOLUME,
	PREF_VOICE_TALK_ENABLED,
	PREF_VOICE_FEEDBACK,
	PREF_VOICE_INPUT_DEVICE,
	PREF_VOICE_CHAT_MODE,
	PREF_VOICE_MIC_VOLUME,
	PREF_VOICE_MIC_SENSITIVITY,

	PREF_MOUSE_TYPE,
	PREF_MOUSE_DRIVE,
	PREF_MOUSE_FLY,
	PREF_MOUSE_SUB,
	PREF_MOUSE_ON_FOOT_SCALE,
	PREF_MOUSE_DRIVING_SCALE,
	PREF_MOUSE_PLANE_SCALE,
	PREF_MOUSE_HELI_SCALE,
	PREF_MOUSE_SUB_SCALE,	
	PREF_MOUSE_WEIGHT_SCALE,
	PREF_MOUSE_ACCELERATION,
	PREF_KBM_TOGGLE_AIM,
	PREF_ALT_VEH_MOUSE_CONTROLS,

	PREF_FPS_PERSISTANT_VIEW,
	PREF_FPS_FIELD_OF_VIEW,
	PREF_FPS_LOOK_SENSITIVITY,
	PREF_FPS_AIM_SENSITIVITY,
	PREF_FPS_AIM_DEADZONE,
	PREF_FPS_AIM_ACCELERATION,
	PREF_AIM_DEADZONE,
	PREF_AIM_ACCELERATION,
	PREF_FPS_AUTO_LEVEL,
	PREF_FPS_RAGDOLL,
	PREF_FPS_COMBATROLL,
	PREF_FPS_HEADBOB,
	PREF_FPS_THIRD_PERSON_COVER,
	PREF_HOOD_CAMERA,
	PREF_FPS_VEH_AUTO_CENTER,

	PREF_LOOK_AROUND_SENSITIVITY,
	PREF_INVERT_MOUSE,
	PREF_INVERT_MOUSE_FLYING,
	PREF_INVERT_MOUSE_SUB,
	PREF_SWAP_ROLL_YAW_MOUSE_FLYING,
	PREF_MOUSE_AUTOCENTER_BIKE,
	PREF_MOUSE_AUTOCENTER_CAR,
	PREF_MOUSE_AUTOCENTER_PLANE,

	PREF_RESTORE_DEFAULTS,

	PREF_REPLAY_MODE,
	PREF_REPLAY_MEM_LIMIT,
	PREF_REPLAY_AUTO_RESUME_RECORDING,
	PREF_REPLAY_AUTO_SAVE_RECORDING,
	PREF_VIDEO_UPLOAD_PRIVACY,

	PREF_BIG_RADAR,
	PREF_BIG_RADAR_NAMES,
	PREF_SHOW_TEXT_CHAT,

	PREF_SIXAXIS_HELI,
	PREF_SIXAXIS_BIKE,
	PREF_SIXAXIS_BOAT,
	PREF_SIXAXIS_RELOAD,
	PREF_SIXAXIS_CALIBRATION, 
	PREF_SIXAXIS_ACTIVITY,
	PREF_SIXAXIS_AFTERTOUCH,  

	// This is primarily for PS3 TRC[R062] but is defined here as other platforms this will be 0 (us-gb way).
	PREF_ACCEPT_IS_CROSS,

	PREF_WIRELESS_STEREO_HEADSET,
	PREF_CTRL_SPEAKER,
	PREF_CTRL_SPEAKER_VOL,
	PREF_PULSE_HEADSET,
	PREF_CTRL_SPEAKER_HEADPHONE,
	PREF_ALTERNATE_HANDBRAKE,
	PREF_ALTERNATE_DRIVEBY,
	PREF_LANDING_PAGE,
    PREF_UR_PLAYMODE,
	PREF_UR_AUTOSCAN,
	PREF_AUDIO_MUTE_ON_FOCUS_LOSS,

	PREF_ROCKSTAR_EDITOR_TOOLTIP,
	PREF_ROCKSTAR_EDITOR_EXPORT_GRAPHICS_UPGRADE,

	PREF_MEASUREMENT_SYSTEM,

	PREF_FPS_DEFAULT_AIM_TYPE,
	PREF_FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING,

	MAX_MENU_PREFERENCES
};

enum eCameraZoom {
	CZ_THIRD_PERSON_NEAR,
	CZ_THIRD_PERSON_MEDIUM,
	CZ_THIRD_PERSON_FAR,
	CZ_FIRST_PERSON
};

enum eScrollClickDirection
{
	SCROLL_CLICK_NONE = -1,
	SCROLL_CLICK_LEFT = 0,
	SCROLL_CLICK_RIGHT,
	SCROLL_CLICK_UP,
	SCROLL_CLICK_DOWN,
};

enum eNUM_PAUSE_MENU_PAUSEDRP_OWNERS
{
	OWNER_OVERRIDE		= 0,
	OWNER_SCRIPT		= BIT(0),
	OWNER_STORE			= BIT(1),
	OWNER_PAUSEMENU		= BIT(2),
	OWNER_CONTROLMGR	= BIT(3),
	OWNER_VIDEO			= BIT(4),
	OWNER_REPLAY		= BIT(5),
	OWNER_LOADING_SCR	= BIT(6)
#if RSG_PC
	,OWNER_SCRIPT_OVERRIDE = BIT(7)
#endif
};

enum eNUM_PAUSE_MENU_CREDITS_AND_LEGAL_STATES
{
	CREDITS_AND_LEGAL_HIDE			= -1,
	CREDITS_AND_LEGAL_SHOW_BOTH		=  0,
	CREDITS_AND_LEGAL_SHOW_CREDITS	=  1,
	CREDITS_AND_LEGAL_SHOW_LEGAL	=  2
};

/// Index into the MenuArray for direct access
typedef s32 MenuArrayIndex;


enum eSprites
{
	MENU_SPRITE_BACKGROUND_GRADIENT = 0,
	MENU_SPRITE_SPINNER,
	MENU_SPRITE_ARROW,
	MENU_SPRITE_SAVING_SPINNER,
	MAX_MENU_SPRITES
};

// Current platform
#if __XENON
#define PM_PLATFORM_ID	"xbox360"
#elif __PS3
#define PM_PLATFORM_ID	"ps3"
#elif __PSP2
#define PM_PLATFORM_ID	"vita"
#elif RSG_ORBIS
#define PM_PLATFORM_ID	"ps4"
#elif RSG_DURANGO
#define PM_PLATFORM_ID	"durango"
#elif __64BIT
#define PM_PLATFORM_ID	"x64"
#endif

#if __DEV
#define UIATSTRINGHASH(str,code) str
#else
#define UIATSTRINGHASH(str,code) ATSTRINGHASH(str,code)
#endif

#define SENSITIVITY_SLIDER_MAX 14
#if RSG_PC
#define SAFEZONE_SLIDER_MAX 10
#else
#define SAFEZONE_SLIDER_MAX 13
#endif // RSG_PC
#define DEFAULT_SLIDER_MAX 10

#define MAX_MOUSE_SCALE 200

class CMenuItem
{
public:
	CMenuItem() 
		: MenuPref(PREF_CONTROL_CONFIG)
		, MenuUniqueId(MENU_UNIQUE_ID_INVALID)
		, MenuOption(MENU_OPTION_SLIDER)
		, MenuAction(MENU_OPTION_ACTION_NONE)
	{
#if __BANK
		++sm_Count;
		sm_Allocs += sizeof(CMenuItem);
#endif
	}

#if __BANK
	~CMenuItem()
	{
		--sm_Count;
		sm_Allocs -= sizeof(CMenuItem);
	}
	static int sm_Count;
	static int sm_Allocs;
#endif

	MenuScreenId MenuUniqueId;
	atHashWithStringDev cTextId;
	UIContextList m_ContextHashes;
	u8 MenuPref;	// eMenuPref
	u8 MenuOption;	// eMenuOption
	u8 MenuAction;	// eMenuAction
	u8 m_Flags;

	bool HasFlag(eMenuItemBits whichFlag) const { return (m_Flags&(1<<whichFlag))!=0; }
	
	bool CanShow() const { return m_ContextHashes.CheckIfPasses(); }
	void preLoad(parTreeNode* pTreeNode);

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////


class CMenuVersion
{
	atHashWithStringDev m_MenuVersionId;
	atHashWithStringDev	m_MenuItemColour;
	atHashWithStringDev	m_MenuHeader;

	MenuScreenId		m_InitialScreen;
	atFixedBitSet32		m_VersionFlags;
	UIContextList		m_InitialContexts;

public:
	const atHashWithStringDev&	GetId()				const { return m_MenuVersionId; }
	const atHashWithStringDev&	GetMenuItemColour() const { return m_MenuItemColour; }
	const atHashWithStringDev&	GetMenuHeader()			const { return m_MenuHeader; }	


	const MenuScreenId&			GetInitialScreen() const { return m_InitialScreen; }
	const UIContextList&		GetInitialContexts() const { return m_InitialContexts; }

	bool HasFlag(eMenuVersionBits whichFlag) const { return m_VersionFlags.IsSet(whichFlag); }

	void preLoad(parTreeNode* pTreeNode);
	PAR_SIMPLE_PARSABLE;

#if __BANK
public:
	CMenuVersion()
	{
		++sm_Count;
		sm_Allocs += sizeof(CMenuVersion);
	}
	~CMenuVersion()
	{
		--sm_Count;
		sm_Allocs -= sizeof(CMenuVersion);
	}
	static int sm_Count;
	static int sm_Allocs;
#endif
};

//////////////////////////////////////////////////////////////////////////


class CMenuScreen
{
public:
	typedef CMenuBase*(*UIMenuCollectionFunc)(CMenuScreen& owner);
	typedef atMap<u32,UIMenuCollectionFunc> CollectionTypeMap;
	
	template <typename T> static CMenuBase* newFrame(CMenuScreen& owner)
	{
		return rage_new T(owner);
	}
	static void RegisterType(u32 hash, UIMenuCollectionFunc func)
	{
		if(!sm_MenuTypeMap.Access(hash))
		{
			sm_MenuTypeMap.Insert(hash, func);
		}
	}
	static void RegisterTypes();


	CMenuScreen() : m_pDynamicMenu(NULL) {
#if __BANK
		++sm_Count;
		sm_Allocs += sizeof(CMenuScreen);
#endif
	};


	~CMenuScreen()
	{
		DeleteDynamicMenu();
		delete m_pDynamicData;
		delete m_pContextMenu;
		delete m_pButtonList;
#if __BANK
		--sm_Count;
		sm_Allocs -= sizeof(CMenuScreen);
#endif
	}

	void CreateDynamicMenu();
	bool HasDynamicMenu() const { return m_pDynamicMenu != NULL; }
	CMenuBase* GetDynamicMenu() const { return m_pDynamicMenu; }
	void DeleteDynamicMenu() { delete m_pDynamicMenu; m_pDynamicMenu = NULL;}

	const char* GetGfxFilename() const { return cGfxFilename.c_str(); }

	bool HandleContextMenuTriggers(MenuScreenId	MenuTriggered, s32 iUniqueId);
	bool HandleContextMenuInput(s32 eInput);
	bool PromptContextMenu();
	void BackOutOfWarningMessageToContextMenu();

	// find the index of a given ID. Useful for when you need to update something filled out by XML
	int FindItemIndex(MenuScreenId findMatch,	int* onScreenIndex = NULL, bool bFilterForOnscreen = false ) const;
	int FindItemIndex(eMenuPref findMatch,		int* onScreenIndex = NULL ) const;
	int FindOnscreenItemIndex(int onScreenIndex, eMenuAction kActionFilter = MENU_OPTION_ACTION_NONE) const;

	bool WouldShowAtLeastTwoItems() const;

	void DrawInstructionalButtons(MenuScreenId iMenuScreen, s32 iUniqueId);
	void ClearInstructionalButtons();

	void INIT_SCROLL_BAR() const;
	static void INIT_SCROLL_BAR_Generic(const atFixedBitSet16& checkThis, eDepth depth);

	bool HasFlag	  (eMenuScreenBits whichFlag) const { return m_Flags.IsSet(whichFlag); }
	bool HasScrollFlag(eMenuScrollBits whichFlag) const { return m_ScrollBarFlags.IsSet(whichFlag); }

	bool FindDynamicData(atHashWithStringDev key, int& out_Value,			int			DefaultValue = 0)		const;
	bool FindDynamicData(atHashWithStringDev key, bool& out_Value,			bool		DefaultValue = false)	const;
	bool FindDynamicData(atHashWithStringDev key, const char*& out_Value,	const char* DefaultValue = NULL)	const;
	bool FindDynamicData(atHashWithStringDev key, atString& out_Value,		const char* DefaultValue = NULL)	const;

public:
	// runtime
	CMenuBase*	m_pDynamicMenu;

	// parsed
	atArray<CMenuItem>	MenuItems;
	CMenuButtonList*		m_pButtonList;
	CContextMenu*			m_pContextMenu;
	CPauseMenuDynamicData*	m_pDynamicData;

	atString				cGfxFilename;
	MenuScreenId			MenuScreen;
	eDepth					depth;
	atFixedBitSet16			m_Flags;
	atFixedBitSet16			m_ScrollBarFlags;

#if __BANK
	static int sm_Count;
	static int sm_Allocs;
#endif

	void postLoad();

	CContextMenu* GetContextMenu();
	const CContextMenu* GetContextMenu() const { return const_cast<const CContextMenu*>( const_cast<CMenuScreen*>(this)->GetContextMenu() ); }

	bool IsContextMenuOpen() const { return GetContextMenu() && GetContextMenu()->IsOpen(); }
protected:

	const atString* FindDatum( const atHashWithStringDev& key) const;

	static CollectionTypeMap sm_MenuTypeMap;
	PAR_SIMPLE_PARSABLE;

};

#define REGISTER_MENU_TYPE(id, class) CMenuScreen::RegisterType(atStringHash(id), CMenuScreen::newFrame<class>)


//////////////////////////////////////////////////////////////////////////

class CMenuArray
{
public:
	atArray<CMenuVersion> MenuVersions;
	atArray<CMenuScreen> MenuScreens;
	atArray<CMenuDisplayValue> DisplayValues;
	atArray<CContextMenuOption> ContextMenuOptions;
	CMenuButtonList DefaultButtonList;

	CGeneralPauseData GeneralData;

	const CContextMenuOption* GetContextMenuOption(atHashWithStringBank contextOption) const;

	void Reset()
	{
		MenuVersions.Reset();
		MenuScreens.Reset();
		DisplayValues.Reset();
		ContextMenuOptions.Reset();
		DefaultButtonList.Reset();
		GeneralData.MovieSettings.Reset();
	}
	void postLoad();
	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
struct NetAction : public datBase
{
private:
	struct EdgeStatus
	{
		netStatus				NowStatus;
		netStatus::StatusCode	LastStatus;

		EdgeStatus() : LastStatus(netStatus::NET_STATUS_NONE) {};

		bool CheckDirty()
		{ 
			if( LastStatus != NowStatus.GetStatus() )
			{
				Reconcile();
				return true;
			}
			return false;
		}
		void Reconcile()	{ LastStatus = NowStatus.GetStatus(); }

		netStatus* GetStatus() { return &NowStatus; }
		void Reset() { NowStatus.Reset(); Reconcile();}
	};

	void OnSuccess(CWarningMessage* pErrorMessage);

	NetworkClan::Delegate	m_NetworkClanDelegate;
	EdgeStatus				m_Status;
	NetworkClan::eNetworkClanOps m_Op;

	datCallback		m_CompleteCB;
	rlClanErrorCodes m_acceptableError;

	atString		m_First;
	atString		m_Second;
	u32				m_MessagePartialHash;

public:
	NetAction();
	~NetAction();


	netStatus* Set(const char* primaryMessage, const char* dataOne = NULL, const char* dataTwo = NULL, datCallback onCompleteCB = NullCB, rlClanErrorCodes acceptableError = RL_CLAN_SUCCESS);
	void SetExternal(NetworkClan::eNetworkClanOps externalOp, const char* primaryMessage, const char* dataOne = NULL, const char* dataTwo = NULL, datCallback onCompleteCB = NullCB, rlClanErrorCodes acceptableError = RL_CLAN_SUCCESS);

	void Update(CWarningMessage* pErrorMessage);
	void OnEvent_NetworkClan(const NetworkClan::NetworkClanEvent& evt);
	
	bool IsDirty();
	void Cancel(bool bResetToo = true);

	netStatus* GetStatus()			{ return m_Status.GetStatus(); }
};

typedef atArray<const atString*>		SharedComponentList;
typedef atArray<const CMenuBase*>		MenuList;


//////////////////////////////////////////////////////////////////////////
class DynamicPauseMenu : public datBase
{
public:
	DynamicPauseMenu();
	~DynamicPauseMenu();

	void Restart();

	void Update();
	void UpdateMenuPed();
	void Render();
	void RenderBGLayer(const PauseMenuRenderDataExtra& renderData);

	CFriendClanData& GetFriendClanData()	{ return m_friendClanData; }
	CWarningMessage& GetErrorMessage()		{ return *m_pErrorMessage; }
	NetAction&		 GetNetAction()			{ return m_NetAction; }
	CUIMenuPed&		 GetMenuPed()			{ return m_menuPed; }
	CUIMenuPed&		 GetLocalPlayerMenuPed(){ return m_localPlayerMenuPed; }
	const SharedComponentList& GetOptionalSharedComponents() { return m_OptionalSharedComponents; }
	MenuList&		GetNotReadyList()		{ return m_LoadingList; }

	bool IsLocalShowing() const {return m_showLocalAvatar;}
	void SetLocalPedShowing(bool showing) {m_showLocalAvatar = showing;}

	void AddRequiredMovie(const CMenuScreen& menuData);

	int GetLastNumberOfInstructionButtonsDrawn() const  { return m_iLastNumberOfInstructionButtonsDrawn; }
	void SetLastNumberOfInstructionButtonsDrawn(int i)  { m_iLastNumberOfInstructionButtonsDrawn = i; }

	u32 GetLastButtonHash() const { return m_uLastButtonHash; }
	void SetLastButtonHash(u32 newHash){ m_uLastButtonHash = newHash; }

	void SetLastIBData(int i, u32 newHash) { SetLastNumberOfInstructionButtonsDrawn(i); SetLastButtonHash(newHash); }

	void SetAvatarBGMode(bool isMiddle) {m_useMiddleAvatarBG=isMiddle;}
	void SetAvatarBGOn(bool isOn) {m_showAvatarBG=isOn;}
	void SetAvatarBGAlpha(float alpha);
	void SetTargetTime(TW_Dir newDir, datCallback CB = NullCB) { m_TimeDir = newDir; m_TimeWarper.SetCallback(CB); }

	void StartAudioTransition(bool bShouldBeQueued);

	enum SE_Dir
	{
		SE_Intro,
		SE_IntroBlips,
		SE_Outro,

		SE_Count
	};

	void StartScaleEffect(SE_Dir whichWay, datCallback CB = NullCB);
	bool GetScaleEffectPercent(float& fAlphaFader, float& fBlipFader, float& fSizeScalar);

	char* GetTimerMessage()				{ return m_szTimerMessage; }
	const char* GetTimerMessage() const 	{ return m_szTimerMessage; }
	const int GetTimerMessageSize() const	{ return NELEM(m_szTimerMessage); }

#if __BANK
	void AddWidgets(bkBank* pToThis);
	void StartScaleEffectDBG(SE_Dir whichWay);
#endif

private:
	
	void IntroComplete();
	void UpdateAudioTransition();

	enum eAudioTransitionStage
	{
		ATS_WaitingToBeSuppressed,
		ATS_WaitingToStopBeingSuppressed,
		ATS_ReadyToGo,
		ATS_Done
	};

	CFriendClanData m_friendClanData;
	NetAction		m_NetAction;
	CUIMenuPed		m_menuPed;
	CUIMenuPed		m_localPlayerMenuPed;
	CWarningMessage* m_pErrorMessage; // pointer so we don't have to include the file
	CTimeWarper		m_TimeWarper;
	SharedComponentList	m_OptionalSharedComponents;
	MenuList		m_LoadingList;
	char			m_szTimerMessage[300]; // sure hope this is a reasonable-enough size for a message
	u32				m_uLastButtonHash;
	int				m_iLastNumberOfInstructionButtonsDrawn;
	eAudioTransitionStage   m_eAudioTransitionStage;

	u32				m_uTimeStartedScaleEffect;
#if RSG_PC
	bool			m_LastInputKeyboard;
#endif


	bool m_useMiddleAvatarBG;
	bool m_showAvatarBG;
	bool m_showLocalAvatar;
	u8 m_avatarBGAlpha;
	TW_Dir	m_TimeDir;
	SE_Dir	m_EffectDir;
	datCallback m_EffectCompleteCB;
	float m_fAlphaFaderForMenuPed;

#if __BANK
	bkGroup*	m_pGroup;
	char m_dbgTitle[64];
	char m_dbgBody[64];
	char m_dbgSubstring[64];
	char m_dbgSubstring1[64];
	char m_dbgSubstring2[64];
	void DbgAddMessage();
	void DbgClearMessage();
#endif
};

enum eMenuceptionDir
{
	kMENUCEPT_SHALLOWER = -1,
	kMENUCEPT_LIMBO = 0,
	kMENUCEPT_DEEPER = 1
};

// for keeping track of multiply pushed states
// ie, Menuception
struct SPauseMenuState 
{
	SPauseMenuState() : iMenuceptionDir(kMENUCEPT_LIMBO) {};

	MenuScreenId currentScreen;
	MenuScreenId currentActivePanel;
	MenuScreenId forceFocus;
	eMenuceptionDir	iMenuceptionDir;
	
};

class CStreamMovieHelper
{
public:
	CStreamMovieHelper();
	~CStreamMovieHelper();

	void Remove();
	void Set(const char* cGfxFilename, MenuScreenId _newId, eMenuceptionDir _iMenuceptionDir = kMENUCEPT_LIMBO, bool bWaitForLoad = false);

	const eMenuceptionDir GetMenuceptionDir() const { return m_iMenuceptionDir; }
	const MenuScreenId& GetRequestingScreen() const { return m_requestingScreen; }
	const atHashWithStringBank& GetGfxFilename() const { return m_sGfxFilename; }

	const bool IsFree() const		{ return m_movieId.IsFree();	 }
	const bool IsMovieReady() const	{ return !HasGfx() || m_movieId.IsActive();	 }
	const bool HasGfx() const		{ return m_sGfxFilename.IsNotNull(); }

	const bool operator==(const CStreamMovieHelper& rhs) { return m_sGfxFilename == rhs.m_sGfxFilename || m_movieId.GetMovieID() == rhs.m_movieId.GetMovieID(); }
	const bool operator!=(const CStreamMovieHelper& rhs) { return m_sGfxFilename != rhs.m_sGfxFilename || m_movieId.GetMovieID() != rhs.m_movieId.GetMovieID(); }

#if !__NO_OUTPUT
	const char* GetGfxFilenameForDebug() const { const char* pszStringAttempt = atHashString(m_sGfxFilename).TryGetCStr(); return pszStringAttempt ? pszStringAttempt : "-no movie-"; }
#endif
	
#if __BANK
	void AddWidgets(bkBank* toThisBank, int iIndex);
#endif // __BANK

private:
	CScaleformMovieWrapper	m_movieId;
	MenuScreenId			m_requestingScreen;
	eMenuceptionDir			m_iMenuceptionDir;
	atHashWithStringBank	m_sGfxFilename;
#if __BANK
	atHashString			m_dbgScreenName;
#endif
};

//////////////////////////////////////////////////////////////////////////
// Values that can simply just be copied over to the render thread
struct PauseMenuRenderData
{
	PauseMenuRenderData() 
		: vArrowContentLeftPosition(-1.0f, -1.0f)
		, vArrowContentRightPosition(-1.0f, -1.0f)
	{Reset();}

	void Reset()
	{
		bArrowHeadersShow = false;
		bArrowHeaderLeftVisible = false;
		bArrowHeaderRightVisible = false;
		bArrowContentLeftVisible = false;
		bArrowContentRightVisible = false;

		// wipe out the spinners
		for(int i=0; i < MAX_SPINNERS; ++i )
			iLoadingSpinnerDesired[i] = NO_SPINNER;
	}

	void Set(const PauseMenuRenderData& rhs)
	{
		for(int i=0; i < MAX_SPINNERS; ++i )
			iLoadingSpinnerDesired[i] = rhs.iLoadingSpinnerDesired[i];

		vArrowContentLeftPosition	= rhs.vArrowContentLeftPosition;
		vArrowContentRightPosition	= rhs.vArrowContentRightPosition;
		bArrowHeadersShow			= rhs.bArrowHeadersShow;
		bArrowHeaderLeftVisible		= rhs.bArrowHeaderLeftVisible;
		bArrowHeaderRightVisible	= rhs.bArrowHeaderRightVisible;
		bArrowContentLeftVisible	= rhs.bArrowContentLeftVisible;
		bArrowContentRightVisible	= rhs.bArrowContentRightVisible;
	}

	static const int MAX_SPINNERS = 3;
	static const int NO_SPINNER = -1;
	atRangeArray<s8, MAX_SPINNERS>		iLoadingSpinnerDesired;

	Vector2 vArrowContentLeftPosition;
	Vector2 vArrowContentRightPosition;

	// use Alt-Click to block select the :1s if you wanna memory breakpoint
	bool bArrowHeadersShow				: 1;
	bool bArrowHeaderLeftVisible		: 1;
	bool bArrowHeaderRightVisible		: 1;
	bool bArrowContentLeftVisible		: 1;
	bool bArrowContentRightVisible		: 1;
};

// more advanced values that require some work
struct PauseMenuRenderDataExtra : public PauseMenuRenderData
{
	PauseMenuRenderDataExtra() : PauseMenuRenderData() {}; // just ensure everything is set up in CPauseMenu::SetRenderData and you'll be FIIIIIINE
	datCallback	DynamicMenuRender;
	float fAlphaFader;
	float fBlipAlphaFader;
	float fSizeScalar;
	

	// use Alt-Click to block select the :1s if you wanna memory breakpoint
	bool bIsActive						: 1;
	bool bClosingDown					: 1;
	bool bWaitOnCalibrationScreen		: 1;
	bool bWaitOnCreditsScreen			: 1;
#if RSG_PC
	bool bWaitOnPCGamepadScreen			: 1;
#endif
	bool bDisableSpinner				: 1;
	bool bRenderContent					: 1;
	bool bRenderHeader					: 1;
	bool bGalleryLoadingImage			: 1;
	bool bGalleryMaximizeEnabled		: 1;
	bool bVersionHasHeadings			: 1;
	bool bVersionHasNoBackground		: 1;
	bool bRenderBackgroundThisFrame		: 1;
	bool bWarningActive					: 1;
	bool bCustomWarningActive			: 1;
	bool bSocialClubActive				: 1;
	bool bStoreActive					: 1;
	bool bRestartingConditions			: 1;
	bool bRenderInstructionalButtons	: 1;
	bool bCurrentScreenValid			: 1;
	bool bRenderCustomMap				: 1;
	bool bHideMenu						: 1;
	bool bForceSpinner					: 1;
	bool bRenderBlackBackground			: 1;
};


enum PM_ActiveFlags
{

	PM_EssentialAssets		 = BIT(0)
	, PM_SkipSocialClub		 = BIT(1)
	, PM_SkipStore			 = BIT(2)
	, PM_SkipVideoEditor	 = BIT(3)
	, PM_WaitUntilFullyFadedInOnly = BIT(4)
	, PM_WaitUntilFullyFadedInANDOut = BIT(5)

	, PM_DefaultFlags = 0
};

class CPauseMenu
{
public:
	static const u32 MAX_LENGTH_OF_MISSION_TITLE = 64;

	enum UpdatePrefsSource {
		UPDATE_PREFS_INIT,
		UPDATE_PREFS_FROM_MENU,
		UPDATE_PREFS_PROFILE_CHANGE,
		UPDATE_PREFS_DEFAULTING,
	};

	// has to match commands_hud.sch's PAUSE_MENU_STATE
	// numbers are weird in case we need to add intermediate states later
	enum scrPauseMenuScriptState
	{
		PM_INACTIVE 	 =  0, 	// not initialized at all
		PM_STARTING_UP 	 =  5, 	// in the process of starting up (loading assets, etc. May enter this state during a restart)
		PM_RESTARTING 	 = 10,	// restarting (or has a restart queued)
		PM_READY 		 = 15,	// ready to have commands sent to it
		PM_IN_STORE 	 = 20,	// in the store screen
		PM_IN_SC_MENU 	 = 25,	// in social club screen
		PM_SHUTTING_DOWN = 30,	// is closing down (example, waiting up to 3 seconds for streaming to finish)
		PM_IN_VIDEOEDITOR = 35	// in video editor
	};

	struct QueuedOpenParams
	{
		eFRONTEND_MENU_VERSION m_iMenuVersion;
		eMenuScreen m_HighlightTab;
		bool m_bFailGently;
		bool m_bPauseGame;
		bool m_bActive;

		void Set(eFRONTEND_MENU_VERSION iMenuVersion, bool bPauseGame = true, eMenuScreen HighlightTab = MENU_UNIQUE_ID_INVALID, bool bFailGently = false)
		{
			m_iMenuVersion = iMenuVersion;
			m_HighlightTab = HighlightTab;
			m_bFailGently = bFailGently;
			m_bPauseGame = bPauseGame;
			m_bActive = true;
		}

		void Reset()
		{
			m_bActive = false;
		}
	};

	struct PMMouseEvent
	{
		static const PMMouseEvent INVALID_MOUSE_EVENT;

		int iIndex;
		int iMenuItemId;
		int iUniqueId;

		bool operator == (const PMMouseEvent& other) const { return iIndex == other.iIndex && iMenuItemId == other.iMenuItemId && iUniqueId == other.iUniqueId; }
		bool operator != (const PMMouseEvent& other) const { return !(*this == other); }
	};

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();


	static void HandlePersistentXML(parTreeNode* pNodeRead);
	static void SetRenderData(PauseMenuRenderDataExtra& renderData);
	static void RenderBackgroundContent(const PauseMenuRenderDataExtra& renderData);
	static void RenderForegroundContent(const PauseMenuRenderDataExtra& renderData);

	// IF YOU'RE HITTING AN ERROR REGARDING eMenuScreen, WAIT FOR THE COMPILE TO FINISH
	// AS IT'S GENERATED BY A PSC
	// \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/

	static void Restart(eFRONTEND_MENU_VERSION iMenuVersion, eMenuScreen HighlightTab = MENU_UNIQUE_ID_INVALID);
	static void Open(eFRONTEND_MENU_VERSION iMenuVersion, bool bPauseGame = true, eMenuScreen HighlightTab = MENU_UNIQUE_ID_INVALID, bool bFailGently = false);
	static void QueueOpen(eFRONTEND_MENU_VERSION iMenuVersion, bool bPauseGame = true, eMenuScreen HighlightTab = MENU_UNIQUE_ID_INVALID, bool bFailGently = false);
	static void ToggleFullscreenMap(bool bIsOpen);
	static void Close();

	static void SaveSettings();
	static bool LoadSettings();

	static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args, s32 iArgCount);
	static bool WaitingForForceDropIntoMenu();
	static bool ShouldDrawInstructionalButtons();
	static void RedrawInstructionalButtons(int iUniqueId = 0);
	static void ClearInstructionalButtons();
	static void OverrideButtonText(int iSlotIndex, const char* pString);

	static void EnterSocialClub();

	static bool IsActive(int flags) { return IsActive( static_cast<PM_ActiveFlags>(flags) ); } // compiler helper
	static bool IsActive(PM_ActiveFlags flags = PM_DefaultFlags); // Active when either the pause menu or SC menu is open.
	static bool IsRestarting();
	static bool IsLoadingPhaseDone();
	static scrPauseMenuScriptState GetStateForScript();

	static bool IsInSaveGameMenus() { return (IsActive() && GetCurrentMenuVersion() == FE_MENU_VERSION_SAVEGAME); }
	static bool IsInSaveGameOrLoadGameMenus();

	static bool IsInOnlineScreen();
	static bool IsInMapScreen();
	static bool IsInGalleryScreen();
	static bool IsInAudioScreen();

	static void AllowRumble(bool bAllowRumble);

	static void SetupMenuContexts();
	static bool SetCurrentMenuVersion(eFRONTEND_MENU_VERSION newVersion, bool bFailGently = false);
	static eFRONTEND_MENU_VERSION GetCurrentMenuVersion() { return sm_iCurrentMenuVersion; }
	static CMenuVersion& GetCurrentMenuVersionData() { return *sm_pCurrentMenuVersion; }
	static bool GetCurrentMenuVersionHasFlag(eMenuVersionBits kFlag) { return sm_pCurrentMenuVersion && GetCurrentMenuVersionData().HasFlag(kFlag); }
	static CMenuVersion* GetMenuVersionData(eFRONTEND_MENU_VERSION newVersion);

	static bool IsSP();
	static bool IsMP();
	static bool IsLobby();
	static bool IsPreLobby() { return IsLobby(); } // behaves the same (for now?)

	static bool IsLoadGameOptionHighlighted();
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static bool IsUploadSavegameOptionHighlighted();
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	static bool IsInLoadGamePanel();
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static bool IsInUploadSavegamePanel();
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	static void BackOutOfLoadGamePanes();
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static void BackOutOfUploadSavegamePanes();
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	static s8 GetColumnForSpinner();

	static void CloseAndLoadSavedGame();
	static void CloseAndWarp();
	static void AbandonLoadingOfSavedGame();

	static bool IsMenuControlLocked() { return sm_bMenuControlIsLocked; }

	static Vector3 GetPosition();
	static bool GetCodeWantsScriptToControl() { return sm_iCodeWantsScriptToControlScreen != MENU_UNIQUE_ID_INVALID; }
	static s32 GetScreenCodeWantsScriptToControl();

	static bool GetMenuTriggerEventOccurred() { return sm_bMenuTriggerEventOccurred; }
	static bool GetMenuLayoutChangedEventOccurred() { return sm_bMenuLayoutChangedEventOccurred; }

	static s32 GetEventOccurredUniqueId(s32 iNum);
	
	static void LockMenuControl(const bool bFlagAsBusy);
	static void UnlockMenuControl();

	static bool	CheckInput(eFRONTEND_INPUT input, bool bPlaySound = false, u32 OverrideFlags = 0, bool bOverrideFrontendState = false, bool bCheckForButtonJustPressed = false, bool bCheckDisabledInput = false);
	static void	PlayInputSound(eFRONTEND_INPUT input);
	static bool ShouldPlayNavigationSound(bool navUp);
	static bool AreHeadphonesEnabled();

	static void TogglePauseRenderPhases(bool on, eNUM_PAUSE_MENU_PAUSEDRP_OWNERS owner, const char* caller = nullptr );
	static void ResetPauseRenderPhases(WIN32PC_ONLY(eNUM_PAUSE_MENU_PAUSEDRP_OWNERS owner = OWNER_OVERRIDE));
	static void EnableTogglePauseRenderPhases(bool on) { sm_EnableTogglePauseRenderPhases = on; }
	static bool GetPauseRenderPhasesStatus() { return sm_PauseRenderPhasesStatus WIN32PC_ONLY(|| sm_GPUCountdownToPause != -1); }
#if RSG_DURANGO || RSG_PC
	static bool GetGPUCountdownToPauseStatus() { return sm_GPUCountdownToPause > 0; }
	static void ClearGPUCountdownToPauseStatus() { sm_GPUCountdownToPause = 0; }
	static void ResetPauseRenderPhasesStatus() { sm_PauseRenderPhasesStatus = false; }
#endif
	static void GrabPauseMenuOwnership(eNUM_PAUSE_MENU_PAUSEDRP_OWNERS newOwner) { if( sm_TogglePauseRPOwner != OWNER_OVERRIDE ) { sm_TogglePauseRPOwner = newOwner; } }

#if RSG_PC || RSG_DURANGO
	static void DecrementGPUCountDown();
#endif

	static int GetCurrentCharacter();
#if __BANK
	static void InitWidgets();
	static void CreateBankWidgets();
	static void CreateXMLDrivenWidgets();
	static void ShutdownWidgets();
	static void SendDebugFunctionToActionScript();
	static void DebugRescalePauseMenu();
	static void DebugOpenMenu();
	static void DebugRestartMenu();
	static void DebugPauseRenderPhases();
	static void DebugToggleCoronaFullscreenMap();
#endif // __BANK

	static bool SetMenuPreference(s32 iPref, s32 iValue);
	static s32 GetMenuPreference(s32 iPref) { return sm_MenuPref[iPref]; }

#if RSG_ORBIS
	static float GetOrbisSafeZone() { return sm_fOrbisSafeZone; }
	static void ResetOrbisSafeZone();
	static void UpdateOrbisSafeZone(); // temporary until proper callback in SDK 
	static void InitOrbisSafeZoneDelegate();
	static void ShutdownOrbisSafeZoneDelegate();
	static void HandleSafeZoneChange(sysServiceEvent* evt);
	static void ShowDisplaySafeAreaSettings();
#endif

private:
	static void GetLanguageSettings(u32& sysLanguage, u32& language);

public:

	static const char* GetOfflineReasonAsLocKey();
	static void InitLanguagePreferences();
	static u32 GetLanguagePreference();
	static void SetDefaultValues( bool bDefaultGraphics = true );
	static void RestoreCameraDefaults();
	static void RestoreControlDefaults();
	static void RestoreDisplayDefaults();
	static void RestoreAudioDefaults();
	static void RestoreReplayDefaults();
	static void RestoreFirstPersonDefaults();
	static void	UpdateMenuOptionsFromProfile();
	static void	UpdateProfileFromMenuOptions(bool languageReload = true);
	static void SetValuesBasedOnCurrentSettings(UpdatePrefsSource source);
	static void SetValueBasedOnPreference(s32 iPrefChanged, UpdatePrefsSource source, int iPreviousValue = -1);
	static u32 GetLanguageFromSystemLanguage();

#if RSG_PC
	static void HandleResolutionChange();

	static int GetResolutionIndex();

	static void RestoreGraphicsDefaults(bool bInit = false);
	static void RestoreAdvancedGraphicsDefaults(bool bInit = false);
	static void RestoreVoiceChatDefaults();

	static void DeviceLost();
	static void DeviceReset();

	static void RestorePreviousGraphicsValues(bool bApplyRevert = false, bool bInit = false);
	static void RestorePreviousAdvancedGraphicsValues(bool bApplyRevert = false, bool bInit = false);

	static bool DirtyGfxSettings();
	static bool DirtyAdvGfxSettings();
	static bool GfxScreenNeedCountDownWarning();
	static void BackoutGraphicalChanges(bool bApplyRevert = true, bool bRefreshMenu = true);
	
	static Settings GetMenuGraphicsSettings(bool bForSaving = false);

	static void UpdateMemoryBar(bool bInit = false);
	static void UpdateVoiceBar(bool bUpdateSlot = true);
	static void UpdateVoiceSettings();
	static void UpdateVoiceMuteOnLostFocus();
	static bool GetCanApplySettings();

	static void HandleDXVersion(int iDirection, bool bForceUpdate = false);
	static void Handle3DStereo(int iDirection, bool bForceUpdate = false);
	static void Handle3DStereoConvergence(int iDirection, bool bForceUpdate = false);
	static void Handle3DStereoSeparation(int iDirection, bool bForceUpdate = false);
	static void SetStereoOverride(bool bOverride) { m_bStereoOverride = bOverride; }
	static float GetStereoConvergenceBackendValue(int iValue);
	static int GetStereoConvergenceFrontendValue(float fValue);
	static void HandleShadowSoftness(int iDirection, bool bForceUpdate = false, bool bCheckOnly = false);
	static void HandleVoiceInput(int iDirection, bool bForceUpdate = false);
	static void HandleVoiceOutput(int iDirection, bool bForceUpdate = false);
	static void HandleVoiceChatMode(int iDirection, bool bForceUpdate = false);
	static void HandleTessellation(int iDirection, bool bForceUpdate = false);
	static void HandleMSAA(eMenuPref ePerf, int iDirection, bool bForceUpdate = false);
	static void HandleScaling(int iDirection, bool bForceUpdate = false);
	static void HandleTXAA(int iDirection, bool bForceUpdate = false);
	static void HandleGrassQuality(int iDirection, bool bForceUpdate = false);
	static void HandlePostFX(int iDirection, bool bForceUpdate = false);
	static void HandleDOF(int iDirection, bool bForceUpdate = false);
	static void HandleMBStrength(int iDirection, bool bForceUpdate = false);
	static void HandleShadowDistMult(int iDirection, bool bForceUpdate = false);

	static void ConvertToActualResolution(u32& width, u32& height, int screenResolutionIndex);
	static void HandleScreenType(int iDirection, bool bForceUpdate = false);
	static void HandleScreenResolution(int iDirection, bool bForceUpdate = false, bool bFindClosestMatch = false, u32 uPreviousScreenWidth = 0, u32 uPreviousScreenHeight = 0);
	static void HandleScreenRefreshRate(int iDirection, bool bForceUpdate = false);
	static void HandleVideoAdapter(int iDirection, bool bForceUpdate = false);
	static void HandleVideoAspect(int iDirection, bool bForceUpdate = false);
	static void HandleOutputMonitor(int iDirection, bool bForceUpdate = false);

	static void HandleSkipLandingPage(int iDirection, bool bForceUpdate = false);
	static void HandleStartupFlow(int iDirection, bool bForceUpdate = false);

	static void HandleReplayMemoryAllocation(int iDirection, bool bForceUpdate = false);

	static int CullRefreshRateList(atArray<grcDisplayWindow> &list, u32 uWidth, u32 uHeight);
	static float GetAspectOfCurrentSelectedResolution();

	static bool FullscreenIsAllowed();
	static int GetDisplayInfoList(atArray<grcDisplayWindow> &list);
#endif // RSG_PC

#if KEYBOARD_MOUSE_SUPPORT
	static void HandleMouseLeftRight(int iUniqueID, int iDirection);

	static void HandleMouseScale(int iDirection, eMenuPref curPref, bool bForceUpdate = false);
	static void RestoreMiscControlsDefaults();
	static const PMMouseEvent& GetClickEvent() { return sm_MouseClickEvent; }
#endif // KEYBOARD_MOUSE_SUPPORT

	static int GetHairColourIndex();
	static int GetMouseHoverIndex();
	static int GetMouseHoverMenuItemId();
	static int GetMouseHoverUniqueId();



	static void InsertSaveGameIntoMenu(s32 iIndex, char *pSaveGameNameGxt, char *pSaveGameDateGxt, bool bActive);
	static void ClearSaveGameMenu();
	static void FlagSaveGameMenuAsFinished(s32 iMenuItemToHighlight = -1);
	static bool IsPauseMenuControlDisabled();

	static bool HasMenuItemBeenTriggered() { return (iLoadNewGameTrigger >= 0); }
	static void ResetMenuItemTriggered();
	static s32 GetMenuItemTriggered() { return (iLoadNewGameTrigger); }

	static void SetGalleryMaximizeActive(bool bActive)	{ sm_bGalleryMaximizeEnabled = bActive; sm_bRenderContent = !bActive; }
	static bool GetGalleryMaximizeActive()				{ return sm_bGalleryMaximizeEnabled; }
	static void SetGalleryLoadingTexture(bool bLoading, bool bDisplayInstructionalButtons = false);

	// to clear, call SetWarnOnTabChange(NULL);
	static void SetWarnOnTabChange(const char* pszMessageToShow) { sm_pMsgToWarnOnTabChange = pszMessageToShow; }

	static void TriggerMenuClosure();

	static void CloseDisplayCalibrationScreen();
	static void CloseCreditsScreen();
	static bool IsPlayCreditsSupportedOnThisScreen( MenuScreenId const menuLayoutId );

#if RSG_PC
	static void ClosePCGamepadCalibrationScreen();
#endif

	enum ClosingAction
	{
		CA_None,
		CA_StartCommerce,
		CA_StartCommerceMP,
		CA_StartSocialClub,
		CA_Warp,
		CA_LeaveGame,
		CA_StartNewGame,
		CA_StartSavedGame,
	};

	enum eSavegameMenuChoice
	{
		SAVEGAME_MENU_CHOICE_NONE,
		SAVEGAME_MENU_CHOICE_LOAD,
		SAVEGAME_MENU_CHOICE_SAVE,
		SAVEGAME_MENU_CHOICE_DELETE

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		, SAVEGAME_MENU_CHOICE_EXPORT_SP_SAVE
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		, SAVEGAME_MENU_CHOICE_IMPORT_SP_SAVE
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
	};

	static ClosingAction sm_eClosingAction;
	static void SetClosingAction(ClosingAction eNewAction);
	

	static bool sm_bUnpauseGameOnMenuClose;
	static s32 iLoadNewGameTrigger;
	static bool sm_bWaitOnNewGameConfirmationScreen;
	static bool sm_bWaitOnDisplayCalibrationScreen;
	
	static bool sm_bDisplayCreditsScreenNextFrame;
	static bool sm_bWaitOnCreditsScreen;
	static MenuScreenId sm_eCreditsLaunchedFrom;
	static atString sm_currentMissionTextStem;

	static bool sm_bWaitingOnPulseWarning;
	static bool sm_bWaitingOnAimWarning;
	static bool sm_bWaitingOnGfxOverrideWarning;

	#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static bool sm_bWaitOnImportConfirmationScreen;
	#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	#if RSG_PC
	static bool sm_bWantsToExitGame;
	static bool sm_bWantsToRestartGame;
	static bool sm_bWaitOnExitToWindowsConfirmationScreen;
	static bool sm_bBackedWithChanges;
	static bool m_bSetStereo;
	static bool m_bIsStereoPossible;
	static bool m_bStereoOverride;

	static bool sm_bWaitOnPCGamepadCalibrationScreen;
	static u32 sm_iExitTimer;
	#endif

	static bool sm_bDisableSpinner;
	//Unfortunately screen specific bool for gallery menu.
	static bool sm_bGalleryMaximizeEnabled;
	static bool sm_bGalleryLoadingImage;
	static bool sm_bGalleryDisplayInstructionalButtons;
	static CSavegameQueuedOperation_ManualLoad sm_ManualLoadStructure;
	static CSavegameQueuedOperation_ManualSave sm_ManualSaveStructure;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static CSavegameQueuedOperation_ExportSinglePlayerSavegame sm_ExportSPSave;
	static bool sm_bUseManualLoadMenuToExportSavegame;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static CSavegameQueuedOperation_ImportSinglePlayerSavegame sm_ImportSPSave;
	static bool sm_bUseManualSaveMenuToImportSavegame;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

 	static void SetNoValidSaveGameFiles(bool bNoSaves);
 	static bool GetNoValidSaveGameFiles() { return sm_bNoValidSaveGameFiles; }

	static void BackOutOfSaveGameList();

	static void SetSavegameListIsBeingRedrawnAfterDeletingASavegame(bool bRedrawing) { sm_bSaveMenuIsBeingRedrawnAfterDeletingASavegame = bRedrawing; }
	static void SetPlayerWantsToLoadASavegame() { sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_LOAD; }
	static void SetPlayerWantsToSaveASavegame() { sm_SavegameMenuChoice = SAVEGAME_MENU_CHOICE_SAVE; }
	static bool PlayerHasChosenToSaveASavegame() { return (sm_SavegameMenuChoice == SAVEGAME_MENU_CHOICE_SAVE); }
	static bool PlayerHasChosenToDeleteASavegame() { return (sm_SavegameMenuChoice == SAVEGAME_MENU_CHOICE_DELETE); }

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static bool PlayerHasChosenToExportASavegame() { return (sm_SavegameMenuChoice == SAVEGAME_MENU_CHOICE_EXPORT_SP_SAVE); }
#endif	//	__ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static bool PlayerHasChosenToImportASavegame() { return (sm_SavegameMenuChoice == SAVEGAME_MENU_CHOICE_IMPORT_SP_SAVE); }
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

// 	static bool	GetInGamePreview() { return sm_bInGamePreview; }
// 	static void	SetInGamePreview(bool bValue) { sm_bInGamePreview = bValue; }

	static s32 GetContentMovieId();
	static s32 GetHeaderMovieId();
	static CScaleformMovieWrapper& GetMovieWrapper(eNUM_PAUSE_MENU_MOVIES movieId) { return sm_iBaseMovieId[movieId]; }
private:
	// do not expose to the outside world!
	static CStreamMovieHelper&		GetChildMovieHelper(int iHelperIndex) { return sm_iChildrenMovies[iHelperIndex]; }
	static void KickoffStreamingChildMovie(const char* pszFilename, MenuScreenId newScreen, eMenuceptionDir menuceptionDir = kMENUCEPT_LIMBO);
	static bool LoadBaseMovie(eNUM_PAUSE_MENU_MOVIES kMovieIndex, const char* fileName, const char* dataOverride = NULL);
public:

	static void ScaleContentMovie(bool bScale);
	static void UpdatePauseMapLegend();
	static s32 GetCurrentSelectedMissionCreatorBlip();
	static bool IsHoveringOnMissionCreatorBlip();

	static CUIMenuPed* GetDisplayPed() {Assert(sm_pDynamicPause); return sm_pDynamicPause ? &sm_pDynamicPause->GetMenuPed() : NULL;}
	static CUIMenuPed* GetLocalPlayerDisplayPed() {Assert(sm_pDynamicPause); return sm_pDynamicPause ? &sm_pDynamicPause->GetLocalPlayerMenuPed() : NULL;}

	static void SetRenderMenus(bool bRenderMenus) { sm_bRenderMenus = bRenderMenus; }

	static CMenuArray& GetMenuArray() { return sm_MenuArray; }
	static const CSpinnerData& GetSpinnerData() { return sm_PersistentData.Spinner; }

	static void SetContentArrowVisible(bool bLeft, bool bVisible);
	static void SetContentArrowPosition(bool bLeft, float fPositionX = -1.0f, float fPositionY = -1.0f);

	static bool HasProcessedContent() { return sm_bProcessedContent; }
	static bool IsNavigatingContent() { return sm_bNavigatingContent; }
	static bool IsClosingDown() { return sm_bClosingDown; }


	static void RequestSprites();
	static void SetupSprites();
	static void RemoveSprites();

	static u32 GetControllerContext();

	static DynamicPauseMenu* GetDynamicPauseMenu() {Assert(sm_pDynamicPause); return sm_pDynamicPause;}
	static bool DynamicMenuExists() {return sm_pDynamicPause != NULL;}

	static void LockMenuTab(s32 iTabNumber, bool bLock);

	static const MenuScreenId& GetInitialScreen();

	static void SetCurrentMissionActive(bool bActive);
	static void SetCurrentMissionLabel(const char* missionNameLabel, bool bIsLiteralString);
	static bool GetCurrenMissionActive() { return sm_bCurrentlyOnAMission; }
	static char* GetCurrentMissionLabel() { return sm_MissionNameLabel; }
	static bool GetCurrentMissionLabelIsALiteralString() { return sm_bMissionLabelIsALiteralString; }

	static void SetCurrentMissionDescription(bool bActive, const char *pString1, const char *pString2, const char *pString3, const char *pString4, const char *pString5, const char *pString6, const char *pString7, const char *pString8);
	static bool GetCurrentMissionDescriptionIsActive() { return sm_bMissionDescriptionIsActive; }
	static const char *GetCurrentMissionDescription() { return sm_MissionDescriptionString; }

	static void PlaySound(const char *pSoundString);

	#if RSG_PC
	static bool GetGameWantsToExitToWindows()	{ return sm_bWantsToExitGame; }
	static void SetGameWantsToExitToWindows(bool bShutdown)	{ sm_bWantsToExitGame = bShutdown; }
	static bool GetGameWantsToRestart()			{ return sm_bWantsToRestartGame; }
	static void SetGameWantsToRestart(bool bRestart) { sm_bWantsToRestartGame = bRestart; }
	static bool GetBackedWithGraphicChanges() { return sm_bBackedWithChanges; }
	static void SetBackedWithGraphicChanges(bool bValue) { sm_bBackedWithChanges = bValue; }
	#endif

	static bool ShouldDisplayGamma() { return sm_iMenuPrefSelected == PREF_GAMMA; }
	static bool ShouldPlayMusic() { return sm_iMenuPrefSelected == PREF_MUSIC_VOLUME || sm_iMenuPrefSelected == PREF_MUSIC_VOLUME_IN_MP; }
	static bool ShouldPlaySfx() { return sm_iMenuPrefSelected == PREF_SFX_VOLUME; }
	static bool ShouldPlayRadioStation() { return sm_iMenuPrefSelected == PREF_RADIO_STATION && GetMenuPreference(PREF_RADIO_STATION) != -1; }

	static void SetMenuPrefSelected(eMenuPref newSetting) { sm_iMenuPrefSelected = newSetting; }
	static eMenuPref GetMenuPrefSelected() { return sm_iMenuPrefSelected; }
	static void SetLastValidPref(eMenuPref newSetting) { sm_iLastValidPref = newSetting; }
	static eMenuPref GetLastValidPref() { return sm_iLastValidPref; }
	static bool HandlePrefOverride( eMenuPref pref, int iDirection, bool bForceUpdate = false );

	static bool IsScreenDataValid(MenuScreenId menuId) { return GetActualScreen(menuId) != MENU_UNIQUE_ID_INVALID; }

	static CMenuScreen& GetScreenData(MenuScreenId menuId)					{ return GetScreenDataByIndex(GetActualScreen(menuId)); }
	static CMenuScreen& GetScreenDataByIndex(MenuArrayIndex iActualArrayID) { return GetMenuArray().MenuScreens[iActualArrayID]; }
	static CMenuScreen& GetCurrentScreenData()								{ return GetScreenData(GetCurrentScreen()); }
	static bool			GetCurrentScreenHasFlag(eMenuScreenBits kFlag);

	static void ActivateSocialClubContext();
	static void DeactivateSocialClubContext();
	static void RefreshSocialClubContext();

	// sets a given column as busy, with up to N displayed at a time
	static void SetBusySpinner(bool bIsBusy, s8 column = PauseMenuRenderData::NO_SPINNER, int iWhichSpinner = 0);

	// Lowlevel versions of the spinner. We have a wrapper that'll work better
	static void RenderAnimatedSpinner(float xOverride = -1.0f, bool bSetStates = false, bool bCheckFrame = true);
	static void RenderAnimatedSpinner(Vector2 vPos, bool bSetStates = false, bool bCheckFrame = true);
	static void RenderAnimatedSpinner(Vector2 vSize, Vector2 vPos, bool bSetStates = false, bool bCheckFrame = true, bool bRenderSavingSpinner = false);

	static int GenerateMenuData(MenuScreenId iUniqueScreen, bool bUpdateOnly = false);
	static void RestoreDefaults(MenuScreenId menuScreenID);

    static void TriggerStore(bool bFromMP = false);
    static bool IsStoreAvailable();


	static s32 ConvertPadButtonNumberToInputType(s32 sPadValue);

	static bool HasFirstLayoutChangedOccurred() { return !sm_bWaitingForFirstLayoutChanged; }

	static void SetPauseMenuWarningScreenActive(bool b) { sm_PauseMenuWarningScreen = b;}
	static bool GetPauseMenuWarningScreenActive() { return sm_PauseMenuWarningScreen;}

	static void SetNavigatingContent(bool bAreWe);

	static MenuScreenId GetMenuceptionForceFocus();
	static CMenuScreen& GetCurrentHighlightedTabData()	{return GetTabData(sm_iCurrentHighlightedTabIndex); }

	static bool IsCurrentScreenValid();

	static void OpenCorrectMenu(bool bSkipCutscene = false);

	static bool IsMenucepting();
	static void MenuceptionGoDeeper(MenuScreenId newMenu);
	static void MenuceptionTheKick();

    static void SetupCodeForPause();
	static void SetupCodeForUnPause();

	static void MENU_SHIFT_DEPTH(eMenuceptionDir dir, bool bDontFallOff = false, bool bSkipInputSpamCheck = false);

	static void ShowConfirmationAlert(const char* pMessage, datCallback functionOnAccept );

private:
	static void ResetAllGlobalMenuVariables(bool bResetPed);
	static void InitGlobalFrontendSprites();
	static void ShutdownGlobalFrontendSprites();

	static bool IsGameInStateWhereItCanBePaused();
	static void SetItemPref(s32 iPref, s32 iValue, UpdatePrefsSource source);

	static MenuArrayIndex GetActualScreen(MenuScreenId iUniqueScreen);

	static void FillContent(MenuScreenId iPaneId);
	static void TriggerEvent(MenuScreenId iUniqueId, s32 iMenuIndex, MenuScreenId newMenuIndex);
	static void LayoutChanged( MenuScreenId iNewLayout, MenuScreenId iPreviousLayout, s32 iUniqueId, s32 iDirection, atHashWithStringBank methodName );
	
	static void SetCurrentScreen(MenuScreenId iUniqueScreen);
	static void SetCurrentPane(MenuScreenId iUniqueScreen);

	static void ShowTabChangeWarning(s32 iDirection, bool bIsTabIndex);

	// returns the data for the currently ACTIVE PANEL (deepest child)
public:
	static MenuScreenId GetCurrentActivePanel();
	static MenuScreenId GetCurrentScreen();
	static CMenuScreen& GetCurrentActivePanelData();

	static s32 GetLastControlConfigChanged() { return sm_LastControlConfigChanged; }
	
#if RSG_PS3	
	static void HandleWirelessHeadsetContextChange();
#endif

private:
	static bool			GetCurrentActivePanelHasFlag(eMenuScreenBits kFlag);

	static void SetStoreAvailable(bool bIsAvailable);
	
	// Returns the data for the currently highlighted MAIN TAB, by index of item on screen
	static CMenuScreen& GetTabData(s32 iScreen)			{return GetScreenData( GetScreenData(GetInitialScreen() ).MenuItems[iScreen].MenuUniqueId);}


	static bool AnyLayerHasFlag(eMenuScreenBits kFlag);

	static void GetFullStatName(char* szStatName, const sStatData* pActualStat, const sUIStatData* pStat);
	static bool MenuVersionHasHeadings();
	static bool SetupMenuStructureFromXML(int iMode);
	static void UpdatePlayerInfoAtTopOfScreen(bool bForceUpdate, bool bSendToActionscript);

	static void SetPlayerBlipName(const char* cName);
	static void SetupMenuHeadings();
	static void SetupHeaderTextAndHighlights( CMenuScreen &data );
	static void OpenLite(MenuScreenId defaultHighlight);
	static void RecurseRequiredMovies(CMenuScreen& data, atArray<MenuScreenId>* BANK_ONLY(aCycleCheck));
	static bool HandleLoadingState();
#if __PRELOAD_PAUSE_MENU
	static bool HandlePreloadingState();
#endif
	static void RemoveAllMovies();
	static bool HandleCreationAndLoadingState();
	static bool AreAllAssetsActive();
	static bool PreparePaneSwitch();
	static void TriggerSwitchPane(s32 iMovement);
	static void TriggerSwitchPaneWithTabIndex(s32 iNewTabIndex);
	static void KickoffSwitchPane(bool bRenderContent, CMenuScreen& OUTPUT_ONLY(thisScreen));
	static void UpdateSwitchPane();
	static void UpdateInput(const bool bLockInputThisFrame);
	static void StartInputCooldown(s32 duration);
	static bool IsInInputCooldown();

	static void SetMaxPayneMode(const bool bNewMode);

	static void PauseTime();
	static void UnPauseTime();

	static void PopulateLoadGameMenu(bool bRepopulate);
	static void PopulateSaveGameMenu();
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static void PopulateUploadSavegameMenu(bool bRepopulate);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	static void SetupControllerPref();
	static void HandleRadioStation( int iDirection, bool bForceUpdate = false );
	static void HandleAudioOutput( int iDirection, bool bForceUpdate = false );
	static void HandleDialogueBoost(int iDirection, bool bForceUpdate = false);
	static void HandleSSWidth(int iDirection, eMenuPref ePrefFrontBack, bool bForceUpdte = false);
	static void UpdateAudioImage();
	
	static void CheckWhatToDoWhenClosed();
	static void CloseInternal();
	static void CloseInternal2(bool bForceIt = false);
	static void CloseAndPlayProperSoundCB();
	static void DestroyAllDynamicMenus();
	static void DeleteDynamicPause();


	static void RenderSpecificScreenOverlays(const PauseMenuRenderDataExtra& renderData);
	static void RenderSpecificScreenBackground(const PauseMenuRenderDataExtra& renderData);
	static void RenderBackground(const PauseMenuRenderDataExtra& renderData);
	static void RenderGeneralOverlays(const PauseMenuRenderDataExtra& renderData);

#if KEYBOARD_MOUSE_SUPPORT
	static bool GetScrollPressed(s32 &pressed, bool bPlaySound);
#endif // KEYBOARD_MOUSE_SUPPORT

	static void SetFlaggedAsBusy(bool bBusy);

	static void QueueSavegameOperationIfNecessary();
	static void SetPauseRenderPhases(bool bPause WIN32PC_ONLY(,eNUM_PAUSE_MENU_PAUSEDRP_OWNERS owner = OWNER_OVERRIDE));
#if RSG_PC
public:
	static void RePauseRenderPhases();
private:
#endif

	// set display config to pause menu
	static void UpdateDisplayConfig();
#if RSG_PC
	static bool IsAspectRatioMatched(float fAspectRatio1, float fAspectRatio2, float fEpsilon);
#endif // RSG_PC

	static void DisplayNewGameAlertMessage();
	static void DisplayImportAlertMessage();
	static void DisplayCreditsMessage(MenuScreenId iMenuScreen);

	#if RSG_PC
	static void DisplayQuitGameAlertMessage();
	static void TriggerStorePC();
	#endif

	static PM_COLUMNS GetColumnForSavegameMenu();
	
	static void MoveOffLoadGameMenuOption();
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static void MoveOffUploadSavegameMenuOption();

	static bool AllowSavegamesToBeExported();
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES

	static bool AllowSavegamesToBeImported();
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static PauseMenuRenderData	sm_RenderData;

	//static s32 sm_iBackgroundFader;
	static s32 sm_inputCooldownTimer;
	static CSystemTimer sm_iMenuSelectTimer;
	static bool sm_bRenderContent;
	static bool sm_bStoreAvailable;
 	static bool	sm_bNoValidSaveGameFiles;	//	This is for the special case on PS3 where there are no valid saves to load
										 	//	and SaveGameListScreenSetup returns immediately because it doesn't have to scan
											//	the hard disk after the first scan. Don't disable FRONTEND_MENU_OPTIONS in this case.

	static eMenuPref sm_iMenuPrefSelected;
	static eMenuPref sm_iLastValidPref;

	static bool sm_forceDropIntoMenu;
	static bool sm_waitingForForceDropIntoMenu;
	static bool sm_dropIntoMenuWhenStreamed;

	static s32 sm_PedShotHandle;

	static MenuScreenId sm_DefaultHighlightTab;
	static bool sm_bClanTextureRequested;

	static bool sm_bCloseMenus;
	static u8 sm_updateDelay;

	static s32 sm_iCurrentHighlightedTabIndex;
	static s32 sm_iStreamingMovie;
	static s32 sm_iStreamedMovie;

	static bool sm_bHasFocusedMenu;
	static bool sm_bWaitingForFirstLayoutChanged;

#if KEYBOARD_MOUSE_SUPPORT
	static int sm_iMouseClickDirection;
	static int sm_iMouseScrollDirection;

	static PMMouseEvent sm_MouseHoverEvent;
	static PMMouseEvent sm_MouseClickEvent;
	static int sm_iHairColourSelected;
#endif // KEYBOARD_MOUSE_SUPPORT
	
	static bool sm_bActive;
	static eFRONTEND_MENU_VERSION sm_iCurrentMenuVersion;
	static eFRONTEND_MENU_VERSION sm_iRestartingMenuVersion;
	static CMenuVersion* sm_pCurrentMenuVersion;
	static MenuScreenId sm_iRestartingHighlightTab;
	static s32 sm_MenuPref[MAX_MENU_PREFERENCES];
	static char sm_displayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];

#if RSG_ORBIS
	static float sm_fOrbisSafeZone;
	static bool sm_bOrbisSafeZoneInError;
	static ServiceDelegate sm_SafeZoneDelegate;
#elif RSG_DURANGO
	static int sm_displayNameReqID;		// We currently only put in display name requests for DURANGO
	static bool sm_bHelpLoginPromptActive;
#endif
	
	static sysTimer sm_PauseMenuCloseTimer;

	static int sm_TogglePauseRPOwner;
	static bool sm_EnableTogglePauseRenderPhases;
	static bool sm_PauseRenderPhasesStatus;
	static bool sm_PauseMenuWarningScreen;
#if RSG_PC
	static int sm_PauseUpdateWaterGPUCount;
#endif
	static sysTimer sm_time;

#if RSG_PC || RSG_DURANGO
	static int sm_GPUCountdownToPause;
#endif

	static bool sm_bScriptWasPaused;
	static bool sm_bStartedUserPause;
	static const char* sm_pMsgToWarnOnTabChange;
	static u8   sm_iCallbacksPending;

	static s32 sm_iMenuEventOccurredUniqueId[3];
	static s32 sm_iMenuEventOccurredMenuIndex;
	static bool sm_bMenuTriggerEventOccurred;
	static bool sm_bMenuLayoutChangedEventOccurred;

	static MenuScreenId sm_iCodeWantsScriptToControlScreen;
	static bool sm_bMenuControlChangedThisFrame;
	static bool sm_bMenuControlIsLocked;
	static bool sm_bLockTheAcceptButton;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static bool sm_bDelayedEntryToExportSavegameMenu;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static s8 sm_SpinnerColumnForFlaggedAsBusy;

	static atArray<SPauseMenuState> sm_aMenuState;

	static bool sm_bCurrentScreenSelected;
	static atRangeArray<CScaleformMovieWrapper,MAX_PAUSE_MENU_BASE_MOVIES> sm_iBaseMovieId;
	static atRangeArray<CStreamMovieHelper, MAX_PAUSE_MENU_CHILD_MOVIES> sm_iChildrenMovies;

	static char sm_MissionNameLabel[MAX_LENGTH_OF_MISSION_TITLE];
	static bool sm_bMissionLabelIsALiteralString;
	static bool sm_bCurrentlyOnAMission;

	static const u32 MAX_LENGTH_OF_MISSION_DESCRIPTION = 512;  // we need to support up to 512 chars here - fixes 1630094
	static char sm_MissionDescriptionString[MAX_LENGTH_OF_MISSION_DESCRIPTION];
	static bool sm_bMissionDescriptionIsActive;

	static bool sm_bProcessedContent;
	static bool sm_bRenderMenus;
	static bool sm_bClosingDown;
	static bool sm_bClosingDownPart2;
	static char sm_PauseMenuCloseDelay;
	static bool sm_bCurrentMenuNeedsFading;
	static bool sm_bRestarting;
	static bool sm_bMaxPayneMode;
	static int  sm_iLoadingAssetsPhase;

#if RSG_ORBIS
	static int sm_iFriendPaneMovementInterval;
	static int sm_iFriendPaneMovementTunable;
#endif

#if __PRELOAD_PAUSE_MENU
	static int  sm_iPreloadingAssetsPhase;
#endif
	static bool sm_bSetupStartingPane;
	static bool sm_bNavigatingContent;

	static CSprite2d sm_MenuSprite[MAX_MENU_SPRITES];

	static bool sm_bSaveGameListSync;

	static bool sm_bQueueManualLoadASAP;
	static bool sm_bQueueManualSaveASAP;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static bool sm_bQueueUploadSavegameASAP;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	static bool sm_bAwaitingMenuShiftDepthResponse;

#if RSG_PC
	static bool sm_bCanApply;
#endif // RSG_PC

	static eSavegameMenuChoice sm_SavegameMenuChoice;
	static bool sm_bSaveMenuIsBeingRedrawnAfterDeletingASavegame;

	static DynamicPauseMenu* sm_pDynamicPause;
	static CPauseMenuPersistentData sm_PersistentData;
	static CMenuArray		sm_MenuArray;

	static atArray<bool> sm_TabLocked;
	static bool			 sm_bAllowRumble;
	static u32			 sm_EndAllowRumbleTime;
	static bank_u32		 sm_RumbleDuration;
	static s32			 sm_LastTpsControlsMode;
	static s32			 sm_LastFpsControlsMode;
	static s32			 sm_LastControlConfigChanged;
	static s32			 sm_LastDuckHandBrakeMode;

	static QueuedOpenParams	sm_QueuedOpenParams;
	static audScene* sm_frontendMenuScene;

#if GTA_REPLAY
	static eReplayMemoryLimit	sm_LastReplayMemoryLimit;
#endif	// GTA_REPLAY

#if KEYBOARD_MOUSE_SUPPORT
	// When we have mouse support we need to re-build the controls to ensure the mouse is setup correctly.
	static s32			 sm_LastLookInvertMode;
	static s32			 sm_LastMouseInvertMode;
	static s32			 sm_LastMouseInvertFlyingMode;
	static s32			 sm_LastMouseInvertSubMode;
	static s32			 sm_LastMouseSwapRollYawFlyingMode;
	static s32			 sm_LastMouseOnFootSensitivity;
	static s32			 sm_LastMouseDrivingSensitivity;
	static s32			 sm_LastMousePlaneSensitivity;
	static s32			 sm_LastMouseHeliSensitivity;

	static s32			sm_LastMouseDriveMode; // Currently only on/off.
	static s32			sm_LastMouseFlyMode; // Currently only on/off.
	static s32			sm_LastMouseSubMode; // Currently only on/off.
#endif // KEYBOARD_MOUSE_SUPPORT

#if KEYBOARD_MOUSE_SUPPORT && FPS_MODE_SUPPORTED
	static s32			sm_LastFPSScopeState;
#endif	// KEYBOARD_MOUSE_SUPPORT && FPS_MODE_SUPPORTED

	static bool			sm_languagePreferenceSet;

public:
	static BankInt32 FRONTEND_ANALOGUE_THRESHOLD;
	static int GetMaxSpinnerColumn() { return sm_PersistentData.Spinner.Offsets.GetCount(); }
	static u32 sm_uLastResumeTime;
	static u32 sm_uTimeInputsDisabledFromClose;
	static bank_u32 sm_uDisableInputDuration;
	static u32 sm_languageFromSystemLanguage;
};


class CCutsceneMenu : public CMenuBase
{
public:
	CCutsceneMenu(CMenuScreen& owner) : CMenuBase(owner) {};
	~CCutsceneMenu() {};

	virtual bool UpdateInput(s32 eInput);
};

#endif // INC_PAUSEMENU_H_

// eof
