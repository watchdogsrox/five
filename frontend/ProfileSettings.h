//
// filename:	ProfileSettings.h
// description:	
//

#ifndef INC_PROFILESETTINGS_H_
#define INC_PROFILESETTINGS_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/functor.h"
#include "atl/array.h"
#include "rline/rlgamerinfo.h"
namespace rage
{
    class rlPresence;
    class rlPresenceEvent;
    class bkBank;
}
// Game headers
#include "Network/Live/livemanager.h"
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/savegame_queued_operations.h"
#include "system/control.h"

// --- Defines ------------------------------------------------------------------

#define USE_SETTINGS_MANAGER	(RSG_PC || RSG_DURANGO || RSG_ORBIS)

#ifndef __6AXISACTIVE
#define __6AXISACTIVE (__PPU || RSG_ORBIS)
#endif // __6AXISACTIVE

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CProfileSettings
{
public:
	struct CustomSetting {
		s32 id;
		union {
			s32 ivalue;
			float fvalue;
		};
	};
	// Don't change these id values as they will screw peoples profiles settings
	// Some values that DO NOT appear here are used in the gen9 codebase. Be sure
	// to check the gen9 codebase to ensure newly added stats are unused to prevent
	// build integration issues.
	enum ProfileSettingId {
		INVALID_PROFILE_SETTING = -1,
		TARGETING_MODE = 0,
		AXIS_INVERSION = 1,
		CONTROLLER_VIBRATION = 2,
		CONTROLLER_SENSITIVITY = 3,
		CONTROLLER_SNIPER_INVERT_UNUSED = 4,

#if __6AXISACTIVE
		CONTROLLER_SIXAXIS_HELI = 5,
		CONTROLLER_SIXAXIS_BIKE = 6,
		CONTROLLER_SIXAXIS_BOAT = 7,
		CONTROLLER_SIXAXIS_RELOAD = 8,
		CONTROLLER_SIXAXIS_MISSION = 9,
		CONTROLLER_SIXAXIS_ACTIVITY = 10,
		CONTROLLER_SIXAXIS_AFTERTOUCH = 11,
#endif // __6AXISACTIVE
		CONTROLLER_CONTROL_CONFIG = 12,
		CONTROLLER_AIM_SENSITIVITY = 13,
		LOOK_AROUND_SENSITIVITY = 14,
#if KEYBOARD_MOUSE_SUPPORT
		MOUSE_INVERSION = 15,
#endif // KEYBOARD_MOUSE_SUPPORT

#if LIGHT_EFFECTS_SUPPORT
		CONTROLLER_LIGHT_EFFECT = 16,
#endif // LIGHT_EFFECTS_SUPPORT
		CONTROLLER_SNIPER_ZOOM = 17,

		CONTROLLER_CONTROL_CONFIG_FPS = 20,

#if KEYBOARD_MOUSE_SUPPORT
		//Add mouse inversion settings here so we don't mess up peoples profiles
		MOUSE_INVERSION_FLYING = 21,
		MOUSE_INVERSION_SUB = 22,

		MOUSE_AUTOCENTER_CAR = 23,
		MOUSE_AUTOCENTER_BIKE = 24,
		MOUSE_AUTOCENTER_PLANE = 25,

		MOUSE_SWAP_ROLL_YAW_FLYING = 26,
#endif // KEYBOARD_MOUSE_SUPPORT

#if __XENON
        GAMERCARD_REGION =50,
#endif //#if __XENON

		VOICE_THRU_SPEAKERS = 100,
		VOICE_MUTED = 101,
		VOICE_VOLUME = 102,

		DISPLAY_SUBTITLES = 203,
		DISPLAY_RADAR_MODE = 204,
		DISPLAY_HUD_MODE = 205,
		DISPLAY_LANGUAGE = 206,
		DISPLAY_GPS = 207,
		DISPLAY_AUTOSAVE_MODE = 208,
		DISPLAY_HANDBRAKE_CAM = 209,
		LEGACY_DO_NOT_USE_DISPLAY_GAMMA = 210,
		CONTROLLER_CINEMATIC_SHOOTING = 211,
		DISPLAY_SAFEZONE_SIZE = 212,
		DISPLAY_GAMMA = 213,
		DISPLAY_LEGACY_UNUSED1 = 214,
		DISPLAY_LEGACY_UNUSED2 = 215,
		DISPLAY_LEGACY_UNUSED3 = 216,
		DISPLAY_LEGACY_UNUSED4 = 217,
		DISPLAY_LEGACY_UNUSED5 = 218,
		DISPLAY_LEGACY_UNUSED6 = 219,
		DISPLAY_CAMERA_HEIGHT = 220,
		DISPLAY_BIG_RADAR = 221,
		DISPLAY_BIG_RADAR_NAMES = 222,
		CONTROLLER_DUCK_HANDBRAKE = 223,
		DISPLAY_DOF = 224,
		CONTROLLER_DRIVEBY = 225,
		DISPLAY_SKFX = 226,
		MEASUREMENT_SYSTEM = 227,
		NEW_DISPLAY_LANGUAGE = 228,

		FPS_DEFAULT_AIM_TYPE = 229,
		FPS_PERSISTANT_VIEW = 230,
		FPS_FIELD_OF_VIEW = 231,
		FPS_LOOK_SENSITIVITY = 232,
		FPS_AIM_SENSITIVITY = 233,
		FPS_RAGDOLL = 234,
		FPS_COMBATROLL = 235,
		FPS_HEADBOB = 236,
		FPS_THIRD_PERSON_COVER = 237,
		FPS_AIM_DEADZONE = 238,
		FPS_AIM_ACCELERATION = 239,
		AIM_DEADZONE = 240,
		AIM_ACCELERATION = 241,
		FPS_AUTO_LEVEL = 242,
		HOOD_CAMERA = 243,
		FPS_VEH_AUTO_CENTER = 244,
		FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING = 245,

		// Gen9 is using values 246-250. Do not use these.

		DISPLAY_TEXT_CHAT = 251,

		AUDIO_SFX_LEVEL = 300,
		AUDIO_MUSIC_LEVEL = 301,
		AUDIO_VOICE_OUTPUT = 302,
		AUDIO_GPS_SPEECH = 303,
		AUDIO_HIGH_DYNAMIC_RANGE = 304,
		AUDIO_SPEAKER_OUTPUT = 305,
		AUDIO_MUSIC_LEVEL_IN_MP = 306,
		AUDIO_INTERACTIVE_MUSIC = 307,
		AUDIO_DIALOGUE_BOOST = 308,
		AUDIO_VOICE_SPEAKERS = 309,
		AUDIO_SS_FRONT = 310,
		AUDIO_SS_REAR = 311,
		AUDIO_CTRL_SPEAKER = 312,
		AUDIO_CTRL_SPEAKER_VOL = 313,
		AUDIO_PULSE_HEADSET = 314,
		AUDIO_CTRL_SPEAKER_HEADPHONE = 315,

#if RSG_PC
		AUDIO_UR_PLAYMODE = 316,
		AUDIO_UR_AUTOSCAN = 317,
		AUDIO_MUTE_ON_FOCUS_LOSS = 318,
#endif

		DISPLAY_RETICULE = 412,
		CONTROLLER_CONFIG = 413,
		DISPLAY_FLICKER_FILTER = 414,
		DISPLAY_RETICULE_SIZE = 415,

		NUM_SINGLEPLAYER_GAMES_STARTED = 450,

		DLC_LOADED = 500,

		SAVE_MIGRATION_TRANSACTION_ID_WARNING = 501,
		SAVE_MIGRATION_TRANSACTION_ID = 502,
		SAVE_MIGRATION_CLEAR_STATS = 503,
		SAVE_MIGRATION_LAST_TRANSACTION_ID = 504,
		SAVE_MIGRATION_LAST_TRANSACTION_STATE = 505,

		AMBISONIC_DECODER = 600,
		//...All these values are used for the ambisonic decoder 
		AMBISONIC_DECODER_END = 677,
		AMBISONIC_DECODER_TYPE = 678,

		PC_GRAPHICS_LEVEL = 700, // Currently 0,1,2,3,4 = Low, Med, High, Ultra, Custom
		PC_SYSTEM_LEVEL = 701,
		PC_AUDIO_LEVEL = 702,

		PC_GFX_VID_OVERRIDE = 710,

		PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_HIGH32 = 711, //s64 value for the last successful hardware stats upload.
		PC_LAST_HARDWARE_STATS_UPLOAD_POSIXTIME_LOW32  = 712,

		PC_VOICE_ENABLED = 720,
		PC_VOICE_OUTPUT_DEVICE = 721,
		PC_VOICE_OUTPUT_VOLUME = 722,
		PC_VOICE_TALK_ENABLED = 723,
		PC_VOICE_INPUT_DEVICE = 724,
		PC_VOICE_CHAT_MODE = 725,
		PC_VOICE_MIC_VOLUME = 726,
		PC_VOICE_MIC_SENSITIVITY = 727,
		PC_VOICE_SOUND_VOLUME = 728,
		PC_VOICE_MUSIC_VOLUME = 729,

		// PC but could potentially be other platforms (eg. MAC).
		MOUSE_TYPE = 750,
		MOUSE_SUB= 751,
		MOUSE_DRIVE= 752,
		MOUSE_FLY= 753,
		MOUSE_ON_FOOT_SCALE = 754,
		MOUSE_DRIVING_SCALE = 755,
		MOUSE_PLANE_SCALE = 756,
		MOUSE_HELI_SCALE = 757,
		KBM_TOGGLE_AIM = 758,
		KBM_ALT_VEH_MOUSE_CONTROLS = 759,
		MOUSE_SUB_SCALE = 760,
		MOUSE_WEIGHT_SCALE = 761,
		MOUSE_ACCELERATION = 762,

		FEED_PHONE = 800,
		FEED_STATS= 801,
		FEED_CREW = 802,
		FEED_FRIENDS = 803,
		FEED_SOCIAL = 804,
		FEED_STORE = 805,
		FEED_TOOPTIP = 806,
		FEED_DELAY = 807,

		START_UP_FLOW = 810,
		LANDING_PAGE = 811,

		TICKER_JOHN_MARSTON_IS_AVAILABLE = 850, // it's ticker to tell the player that john marston is available in the character creator
												// appears the first time the player connected up to the social club and then never again.

		TELEMETRY_MPCHARACTER_COUNTER_0 = 860, // These counters go up for each character deleted so that we can keep track
		TELEMETRY_MPCHARACTER_COUNTER_1 = 861, //   of the unique character for mp metrics.
		TELEMETRY_MPCHARACTER_COUNTER_2 = 862, // These can be used by metric processing so that we know if the stats are 
		TELEMETRY_MPCHARACTER_COUNTER_3 = 863, //   relative to a new character.
		TELEMETRY_MPCHARACTER_COUNTER_4 = 864, // 

		GAMER_PLAYED_LAST_GEN = 865, // Indicates if the player has played last gen, 1 - 360, 3 - ps3.
		GAMER_HAS_SPECIALEDITION_CONTENT = 866,  // Indicates if the player is entitled to special edition content

		FACEBOOK_LINKED_HINT = 899, // Returns a value != 0 if the last Facebook App Permissions call returned successful.
		FACEBOOK_UPDATES = 900,

		ROS_WENT_DOWN_NOT_NET = 901, // this is set to TRUE when the ROS link goes down but not the platform network conection
		FACEBOOK_POSTED_BOUGHT_GAME = 902,	// Set true after the player has posted the "Bought GTAV" action/object to facebook.
		PROLOGUE_COMPLETE = 903,
		FACEBOOK_POSTED_ALL_VEHICLES_DRIVEN = 904,
		EULA_VERSION = 905,
		TOS_VERSION = 907,
		PRIVACY_VERSION = 908,
		JOB_ACTIVITY_ID_CHAR0 = 909, //Id of the UGC activity started so we know if the player has pulled the plug.
		JOB_ACTIVITY_ID_CHAR1 = 910, //Id of the UGC activity started so we know if the player has pulled the plug.
		JOB_ACTIVITY_ID_CHAR2 = 911, //Id of the UGC activity started so we know if the player has pulled the plug.
		JOB_ACTIVITY_ID_CHAR3 = 912, //Id of the UGC activity started so we know if the player has pulled the plug.
		JOB_ACTIVITY_ID_CHAR4 = 913, //Id of the UGC activity started so we know if the player has pulled the plug.
		FREEMODE_PROLOGUE_COMPLETE_CHAR0 = 914, //Returns a value !=0 if the prologue was done.
		FREEMODE_PROLOGUE_COMPLETE_CHAR1 = 915, //Returns a value !=0 if the prologue was done.
		FREEMODE_PROLOGUE_COMPLETE_CHAR2 = 916, //Returns a value !=0 if the prologue was done.
		FREEMODE_PROLOGUE_COMPLETE_CHAR3 = 917, //Returns a value !=0 if the prologue was done.
		FREEMODE_PROLOGUE_COMPLETE_CHAR4 = 918, //Returns a value !=0 if the prologue was done.

		MP_FLUSH_POSIXTIME_HIGH32 = 919, //s64 value for the last mp successful profile stats flush.
		MP_FLUSH_POSIXTIME_LOW32  = 920,

		MP_CLOUD0_POSIXTIME_HIGH32 = 921, //s64 value for the last mp successful cloud save.
		MP_CLOUD0_POSIXTIME_LOW32  = 922,
		MP_CLOUD1_POSIXTIME_HIGH32 = 923, //s64 value for the last mp successful cloud save.
		MP_CLOUD1_POSIXTIME_LOW32  = 924,
		MP_CLOUD2_POSIXTIME_HIGH32 = 925, //s64 value for the last mp successful cloud save.
		MP_CLOUD2_POSIXTIME_LOW32  = 926,
		MP_CLOUD3_POSIXTIME_HIGH32 = 927, //s64 value for the last mp successful cloud save.
		MP_CLOUD3_POSIXTIME_LOW32  = 928,

		MP_CLOUD_POSIXTIME_HIGH32 = 931, //s64 value for the last mp successful cloud save.
		MP_CLOUD_POSIXTIME_LOW32  = 932,

		MP_AWD_CREATOR_RACES_DONE  = 933,
		MP_AWD_CREATOR_DM_DONE     = 934,
		MP_AWD_CREATOR_CTF_DONE    = 935,
		MP_CREATOR_RACES_SAVED     = 936,
		MP_CREATOR_DM_SAVED        = 937,
		MP_CREATOR_CTF_SAVED       = 938,

		SP_CHOP_MISSION_COMPLETE   = 939,

		FREEMODE_STRAND_PROGRESSION_STATUS_CHAR0 = 940,
		FREEMODE_STRAND_PROGRESSION_STATUS_CHAR1 = 941,

		MP_CAMERA_ZOOM_ON_FOOT = 950,
		MP_CAMERA_ZOOM_IN_VEHICLE = 951,
		MP_CAMERA_ZOOM_ON_BIKE = 952,
		MP_CAMERA_ZOOM_IN_BOAT = 953,
		MP_CAMERA_ZOOM_IN_AIRCRAFT = 954,
		MP_CAMERA_ZOOM_IN_HELI = 955,

		REPLAY_MEM_LIMIT = 956,
		REPLAY_MODE = 957,
		REPLAY_AUTO_RESUME_RECORDING = 958,

		VIDEO_UPLOAD_PAUSE = 959,
		VIDEO_UPLOAD_PRIVACY = 960,
		ROCKSTAR_EDITOR_TOOLTIP = 961,
		VIDEO_EXPORT_GRAPHICS_UPGRADE = 962,
		ROCKSTAR_EDITOR_TUTORIAL_FLAGS = 963, // s32 of flags representing if R* Editor tutorials have been seen
		REPLAY_AUTO_SAVE_RECORDING = 964,
		REPLAY_VIDEOS_CREATED = 965,

		BANDWIDTH_TEST_TIME = 970,

		SC_PRIO_SEEN_NEXT_SLOT = 980,
		SC_PRIO_SEEN_FIRST = 981,
		SC_PRIO_SEEN_0 = SC_PRIO_SEEN_FIRST,
		SC_PRIO_SEEN_1 = 982,
		SC_PRIO_SEEN_2 = 983,
		SC_PRIO_SEEN_3 = 984,
		SC_PRIO_SEEN_4 = 985,
		SC_PRIO_SEEN_5 = 986,
		SC_PRIO_SEEN_6 = 987,
		SC_PRIO_SEEN_7 = 988,
		SC_PRIO_SEEN_8 = 989,
		SC_PRIO_SEEN_LAST = SC_PRIO_SEEN_8
	};
	typedef Functor0 CallbackFn;

	CProfileSettings() : 	
		m_readCallback(NULL),
		m_valid(false),
		m_reading(false),
		m_readPending(false),
		m_writing(false),
		m_writePending(false),
		m_hasChanged(false),
		m_personalSettingsOnly(false),
#if USE_PROFILE_SAVEGAME
		m_bDontWriteProfile(false),
#endif
#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
		m_bMainProfileSaveIsRequired(false),
#endif
		m_localIndex(-1),
		m_timeOfLastWrite(0)
	{}

	// RETURNS:	Reference to instance.
	static CProfileSettings& GetInstance() { FastAssert(sm_Instance); return *sm_Instance; }
	static bool IsInstantiated() { return sm_Instance != NULL; }

	static void InitClass();
	static void ShutdownClass();

#if __BANK && __XENON
	static void InitWidgets();
	static bool GetUpdateProfileWhenNavigatingSettingsMenu() { return sm_bUpdateProfileWhenNavigatingSettingsMenu; }
#endif	//	__BANK && __XENON

#if RSG_PC
	static void InitDelegator();
#endif

	bool AreSettingsValid() const {return m_valid;}
	void SetReadCallback(CallbackFn fn) {m_readCallback = fn;}

	void Read(bool bPersonalSettingsOnly = false);
	void Write(bool bForceWrite=false);
	void WriteNow(bool bForceWrite=false);
	void UpdateProfileSettings(bool bWait, bool ignoreStreamingState = false);
	void Default();

	// Access functions
    //
    //*Note* that calling Set() while a read is pending will result in the
    //new value being overwritten by the value being read.  Call IsReading()
    //before calling Set(), and defer the new setting until the read is
    //finished.
	void Set(ProfileSettingId id, int value);
	void Set(ProfileSettingId id, float value);
	float GetFloat(ProfileSettingId id);
	int GetInt(ProfileSettingId id);

	bool Exists(ProfileSettingId id);
	void Clear(ProfileSettingId id);

    bool IsReading() const;

    bool IsWriting() const;

    //Retrieves the user's 2-digit ISO 3166 country code (plus null term) for this profile,
    //and returns true if it succeeds (or false otherwise)
    bool GetCountry(char (&countryCode)[3]);

	int GetReplayTutorialFlags();
	void UpdateReplayTutorialFlags( s32 const tutorialFlag );

	static void OnPresenceEvent(const rlPresenceEvent* evt);

#if USE_PROFILE_SAVEGAME
	static char *GetFilenameOfPSProfile();
	static char16 *GetDisplayNameOfPSProfile();
	static int GetSizeOfPSProfileFile();
#endif

#if __PPU
	void SetDontWriteProfile(bool bDontWriteProfile) { m_bDontWriteProfile = bDontWriteProfile; };
	bool GetDontWriteProfile() { return m_bDontWriteProfile; }
#endif

	void SetWritePending(const bool set){m_writePending = set;}

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
	void RequestMainProfileSave() { m_bMainProfileSaveIsRequired = true; }
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

private:
	bool StartWrite(bool ignoreStreamingState = false);
	bool FinishWrite(bool bWait);

	bool StartRead();
	bool FinishRead(bool bWait);
	s32 FindSetting(ProfileSettingId id);
	CustomSetting* FindSettingPtr(ProfileSettingId id);

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
	void UpdateMainProfileSave();
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

#if RSG_PC
	bool StartSavePcProfileThread();
	void EndSavePcProfileThread();
	bool CheckSavePcProfileThread();
#endif	//	RSG_PC


	CallbackFn m_readCallback;
	bool m_valid;
	bool m_reading;
	bool m_writing;
	bool m_readPending;
	bool m_writePending;
	bool m_hasChanged;
	bool m_personalSettingsOnly;
#if USE_PROFILE_SAVEGAME
	bool m_bDontWriteProfile;
	CSavegameQueuedOperation_ProfileLoad m_ProfileLoadStructure;
	CSavegameQueuedOperation_ProfileSave m_ProfileSaveStructure;

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
	bool m_bMainProfileSaveIsRequired;
	CSavegameQueuedOperation_ProfileSave m_MainProfileSaveStructure;
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

#endif

	static CProfileSettings *sm_Instance;
	rlGamerId m_gamerId;
	s32 m_localIndex;
	u32 m_timeOfLastWrite;
	atArray<CustomSetting> m_settings;

#if __BANK && __XENON
	static bool sm_bDisplayDebugText;
	static bool sm_bUpdateProfileWhenNavigatingSettingsMenu;
	static s32 sm_MinimumTimeBetweenProfileSaves;
#else	//	__BANK && __XENON
	static const s32 sm_MinimumTimeBetweenProfileSaves = (1000*60*5);
#endif	//	__BANK && __XENON
};

// --- Globals ------------------------------------------------------------------

// required for this class to interface with the game skeleton code
class CProfileSettingsWrapper
{
public:

    static void Update() { CProfileSettings::GetInstance().UpdateProfileSettings(false); }
};

#endif // !INC_PROFILESETTINGS_H_
