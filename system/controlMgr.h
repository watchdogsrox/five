//
// name:		ControlMgr.h
// description:	Class encapsulating all game controllers (pads, mouse, keyboard) etc
#ifndef CONTROL_MGR_H_
#define CONTROL_MGR_H_

// Rage headers
#include "atl\functor.h"
#include "grcore\debugdraw.h"
#include "input\input.h"
#include "input\mapper.h"
#include "input\keys.h"
#include "input/virtualkeyboard.h"
#include "system/ipc.h"
#include "system/criticalsection.h"

// Game headers
#include "frontend/PauseMenu.h"
#include "system/control.h"
#include "system/pad.h"
#include "system/keyboard.h"
#include "system/service.h"

#define STANDARD_USER_DEFINED_CONTROLS_FILENAME		"common:/data/controls/controls"
#define STANDARD_DEFAULT_CONTROLS_FILENAME		"common:/data/controls/default"  // gets appended with the config number
#define ENABLE_DEBUG_CONTROLS		(!__FINAL)

// using this we can change the defaults file used. Over 2 means custom
enum
{
	CONTROLS_CONFIG_DEFAULT = 0,
	CONTROLS_CONFIG_360_STD,
	CONTROLS_CONFIG_360_NEW,
	CONTROLS_CONFIG_MAX
};


#define NUM_PADS	(ioPad::MAX_PADS)

enum
{
	// NOTE: - If you add other player controls then add it to the bottom of CControlMgr::InitWidgets().
	CONTROL_MAIN_PLAYER = 0,
	CONTROL_EMPTY,				// All controls in here are clear. This is used when the controls of this player are disabled.
#if ENABLE_DEBUG_CONTROLS
	CONTROL_DEBUG,  // additional control layout for debug pad on PC
#endif
	MAX_CONTROL
};

#define OVERLAY_KEYBOARD (RSG_PC && __STEAM_BUILD)

#if OVERLAY_KEYBOARD
#define OVERLAY_KEYBOARD_ONLY(...)	__VA_ARGS__
#else
#define OVERLAY_KEYBOARD_ONLY(...)
#endif // OVERLAY_KEYBOARD

#define NUM_PLAYERS		(1)  // note this does NOT include the debug map

enum eCONTROL_METHOD
{
	CONTROL_METHOD_JOYPAD = 0,
	CONTROL_METHOD_MOUSE_KEYS,
	MAX_CONTROL_METHODS
};

class CControlMgr
{
	friend class CControl;
	
public:
	static void Init(unsigned initMode);
	static void ReInitControls();
	static void SetDefaultLookInversions();
	static void Shutdown(unsigned shutdownMode);

	static void Update();
	static void StartUpdateThread();
	static void WaitForUpdateThread();

#if RSG_PC
	static void UpdateScuiInput();
#endif

#if __BANK
	static void InitWidgets();
	static void LoadCustomControls();
#endif

	static CPad& GetDebugPad() {return m_debugPad;}
	static CPad* GetPlayerPad() {
		if(sm_PlayerPadIndex != -1) 
			return &m_pads[sm_PlayerPadIndex]; 
		return NULL;
	}

	static int GetNumberOfConnectedPads();

	static CKeyboard& GetKeyboard() {return m_keyboard;}

	static bool IsDisabledControl(CControl *pC) {return ((&m_controls[CONTROL_EMPTY]) == pC);} 

	static void SetMainPlayerIndex(s32 index);
	static s32 GetMainPlayerIndex() {return sm_PlayerPadIndex;}

	static CControl& GetMainPlayerControl(bool bAllowDisabling = true);// returns local main player's control
	static CControl& GetMainFrontendControl(bool bAllowDisabling = true);// returns local main player's control for frontend (dont mind whether the controls are disabled)
	static CControl& GetMainCameraControl(bool bAllowDisabling = true); // Camera controls need to be independently disabled on some rare occasions i'm afraid - DW
	static CControl& GetEmptyControl() { return m_controls[CONTROL_EMPTY]; }// returns empty controls

	// NOTE: This should only be using for retrieving control mappings when player control might be disabled.
	static CControl& GetPlayerMappingControl() { return m_controls[CONTROL_MAIN_PLAYER]; }
	
#if ENABLE_DEBUG_CONTROLS
	static CControl& GetDebugControl();  // returns the debug control	
#endif

#if HAS_TRIGGER_RUMBLE
	static void StartPlayerPadTriggerShake(u32 durationLeft, s32 freqLeft, u32 durationRight, s32 freqRight, s32 controller = CONTROL_MAIN_PLAYER );
#endif // #if HAS_TRIGGER_RUMBLE

	static void StartPlayerPadShake(u32 Duration0, s32 MotorFrequency0 = 0, u32 Duration1 = 0, s32 MotorFrequency1 = 0, s32 DelayAfterThisOne = 0, s32 controller = CONTROL_MAIN_PLAYER);
	static void StartPlayerPadShake_Distance(u32 Duration, s32 Frequency, float X, float Y, float Z, s32 controller = CONTROL_MAIN_PLAYER);
	static void StartPlayerPadShaking(s32 MotorFrequency0 = 0, s32 MotorFrequency1 = 0, s32 controller = CONTROL_MAIN_PLAYER);

	// This will scale up rumble depending on the platform to fit an intensity value between 0 and 1
	static void StartPlayerPadShakeByIntensity(u32 iDuration, float fIntensity, s32 iDelayAfterThisOne = 0, s32 iController = CONTROL_MAIN_PLAYER);

	static void StopPlayerPadShaking(bool bForce = false, s32 iController = CONTROL_MAIN_PLAYER);
	static bool IsPlayerPadShaking(s32 controller = CONTROL_MAIN_PLAYER);
	static void StopPadsShaking();
	static void AllowPadsShaking();
	static bool ShouldStopPadsShaking();
	static void SetShakeWhenControllerDisabled(bool bShake = true);

#if __WIN32PC
	static void ApplyDirectionalForce(float X, float Y, s32 timeMS, s32 controller = CONTROL_MAIN_PLAYER);
#endif // __WIN32PC

#if DEBUG_PAD_INPUT
	static void SetDebugPadOn(bool bOn) {sm_bDebugPad = bOn;}
	static bool IsDebugPadOn() {return sm_bDebugPad;}
	static bool IsDebugPadValuesOn();
#endif

	static void SetProfileChangeFunctions(Functor0 init, Functor0 shutdown);

	static ioVirtualKeyboard::eKBState GetVirtualKeyboardState();
	static char* GetVirtualKeyboardResult();

	static void ShowVirtualKeyboard(const ioVirtualKeyboard::Params& params);
	static ioVirtualKeyboard::eKBState UpdateVirtualKeyboard();
	static void CancelVirtualKeyboard();

#if !__FINAL
	static void SetAllowTildeCharacterFromVirtualKeyboard(bool bAllowTilde)	{ sm_bAllowTildeCharacterFromVirtualKeyboard = bAllowTilde; }
#endif	//	!__FINAL

	static void SetFontBitFieldForDisplayingOnscreenKeyboardResult(s32 fontBitField) { sm_fontBitField = fontBitField; }

#if RSG_DURANGO
	static bool IsConstrained() { sysCriticalSection lock(sm_ConstrainedCs); return sm_IsGameConstrained; }
#endif // RSG_DURANGO

#if RAGE_SEC_ENABLE_YUBIKEY
	static bool UpdateYubikey();
#endif // RAGE_SEC_ENABLE_YUBIKEY

	static bool IsShowingControllerDisconnectMessage() { return sm_bNoPadMessageActive; }
	static bool CheckForControllerDisconnect();
	static bool IsShowingKeyboardTextbox();

private:

	enum eFontBitField
	{
//		FONT_BIT_STANDARD = 1,	//	The game already replaces characters in the onscreen keyboard result that aren't supported in the Standard font
		FONT_BIT_CURSIVE = 2,
		FONT_BIT_ROCKSTAR_TAG = 4,
		FONT_BIT_LEADERBOARD = 8,
		FONT_BIT_CONDENSED = 16,
		FONT_BIT_FIXED_WIDTH_NUMBERS = 32,
		FONT_BIT_CONDENSED_NOT_GAMERNAME = 64,

		FONT_BIT_PRICEDOWN = 128  // gta font
	};


	static CPad& GetPad(s32 num) {return m_pads[num];}
	static CControl& GetControl(s32 num) {return m_controls[num];} 

	static void SaveAsDefaultsButton();
	static void LoadDefaultsButton();

	static void ReplaceCharacterIfUnsupportedInFont(char16 &input, s32 fontStyle, eFontBitField fontBit, const char* pFontName, int characterIndex);
	static char *WideToSafeUtf8(char *out,const char16 *in,int length);

	static void UpdateThread(void* param);

#if RSG_DURANGO
	static void HandleConstrainedChanged(sysServiceEvent* evt);
#endif // RSG_DURANGO

	static sysIpcThreadId m_updateThreadId;
	static sysIpcCurrentThreadId sm_ThreadId;
	static sysIpcSema m_runUpdateSema;
	static sysIpcSema m_updateFinishedSema;
	static sysCriticalSectionToken m_Cs;

	static CControl m_controls[MAX_CONTROL];
	static CPad m_pads[NUM_PADS];
	static CPad m_debugPad;
	static CKeyboard m_keyboard;
//	static eCONTROL_METHOD m_ControlMethod;
	static bool sm_bDebugPad;
	static s32 sm_PlayerPadIndex;
	static s32 sm_DebugPadIndex;
	static bool	sm_bNoPadMessageActive;
	static volatile bool sm_bShutdownThreads;
#if DEBUG_PAD_INPUT
	static bool sm_bDebugPadValues;
#endif

	// The duration to disable inputs for when the input is intercepted (xbox/ps3 overlay is active).
	static bank_u32 sm_InputDisableDuration;

	static Functor0 sm_initProfileFn;
	static Functor0 sm_shutdownProfileFn;

	static bool sm_bShakeWhenControllerDisabled; // Used to override controller disabled check

	static ioVirtualKeyboard sm_VirtualKeyboard;
	static char sm_VirtualKeyboardResult[ioVirtualKeyboard::VIRTUAL_KEYBOARD_RESULT_BUFFER_SIZE];
	static bool sm_bVirtualKeyboardIsEnteringAPassword;

#if OVERLAY_KEYBOARD
	// We are using the overlay keyboard (e.g. steam big picture keyboard).
	static bool sm_bUsingOverlayKeyboard;
#endif // USING_OVERLAY_KEYBOARD

#if RSG_DURANGO
	static sysCriticalSectionToken sm_ConstrainedCs;
	static ServiceDelegate sm_Delegate;
	static bool sm_IsGameConstrained;
#endif // RSG_DURANGO

#if __BANK
	// Used to test the on screen keyboard.
	const static u32 MAX_KEYBOARD_TEXT = 25;
	static bool sm_bCheckKeyboard;
	static char sm_DefaultVirtualKeyboardText[MAX_KEYBOARD_TEXT];
	static int  sm_VirtualKeyboardTextLimit;
	static s32	sm_KeyboardMode;
	static void DebugShowVirtualKeyboard();
#endif // __BANK

#if __BANK
public:

	static void PrintDisabledInputsToGameplayLog();
	static bool IsPrintingToGameplayLog() { return sm_bPrintToGameplayLog; }

private:

	static void DisplayDisabledInputs();

	// Updated sm_InputDislayTypeNames if a new mode is added.
	enum InputDisplayType
	{
		IDT_NONE,
		IDT_ALL,
		IDT_NONZERO,
		IDT_DISABLED_ONLY,

		// The number of display options.
		IDT_AMOUNT
	};

	// This is a text description of the enum above for rag. This must be updated if a new mode is added.
	static const char* sm_InputDislayTypeNames[IDT_AMOUNT];

	static InputDisplayType sm_eInputDisplayType;

	static float sm_InputDisplayScale;
	static float sm_InputDisplayX;
	static s32 sm_InputDislayMaxLength;
	static bool sm_bPrintToGameplayLog;
#endif // __BANK


#if !__FINAL
	static bool sm_bAllowTildeCharacterFromVirtualKeyboard;
#endif	//	!__FINAL

	static s32 sm_fontBitField;	//	Taken from eFontBitField

public:
#if __BANK
	static s32 sm_nSaveAsDefaultNum;

	static u32 sm_iShakeDuration0;
	static u32 sm_iShakeDuration1;
	static u32 sm_iShakeFrequency0;
	static u32 sm_iShakeFrequency1;
	static s32  sm_iShakeDelayAfterThisOne;
	static float  sm_fMultiplier;
	static bool   sm_bOverrideShakeFreq0;
	static bool   sm_bOverrideShakeFreq1;
	static bool   sm_bOverrideShakeDur0;
	static bool   sm_bOverrideShakeDur1;
	static bool   sm_bOverrideShakeDelay;
	static bool   sm_bUseThisMultiplier;

	static float sm_fDebugShakeIntensity;
	static u32 sm_iDebugShakeDuration;
	static u32 sm_iDebugMotorToShake;
	static bool sm_bDebugShakeMotor;
	static void DebugShakeCallback();
	static void DebugShakeUsingOverridesCallback();
	
	// Path to custom .meta input settings file
	static char sm_CustomInputMetaFilePath[RAGE_MAX_PATH];

	// Used to switch between different hard coded controller mappings
	static s32 sm_iHardcodedMappingIndex;
	static void HardcodedMappingCallback();
#endif
};

#if HAS_TRIGGER_RUMBLE
inline void CControlMgr::StartPlayerPadTriggerShake(u32 durationLeft, s32 freqLeft, u32 durationRight, s32 freqRight, s32 controller )
{
	m_controls[controller].StartTriggerShake( durationLeft, freqLeft, durationRight, freqRight );
}
#endif // HAS_TRIGGER_RUMBLE

inline void CControlMgr::StartPlayerPadShake(u32 Duration0, s32 MotorFrequency0, u32 Duration1, s32 MotorFrequency1, s32 DelayAfterThisOne, s32 controller)
{
#if __BANK
	if (sm_bOverrideShakeFreq0)
		MotorFrequency0 = sm_iShakeFrequency0;
	if (sm_bOverrideShakeFreq1)
		MotorFrequency1 = sm_iShakeFrequency1;
	if (sm_bOverrideShakeDur0)
		Duration0 = sm_iShakeDuration0;
	if (sm_bOverrideShakeDur1)
		Duration1 = sm_iShakeDuration1;
	if (sm_bOverrideShakeDelay)
		DelayAfterThisOne = sm_iShakeDelayAfterThisOne;
#endif // __BANK

	m_controls[controller].StartPlayerPadShake(Duration0, MotorFrequency0, Duration1, MotorFrequency1, DelayAfterThisOne);
}

inline void CControlMgr::StartPlayerPadShake_Distance(u32 Duration, s32 Frequency, float X, float Y, float Z, s32 controller)
{
#if __BANK
	if (sm_bOverrideShakeFreq0)
		Frequency = sm_iShakeFrequency0;
	if (sm_bOverrideShakeFreq1)
		Frequency = sm_iShakeFrequency1;
	if (sm_bOverrideShakeDur0)
		Duration = sm_iShakeDuration0;
	if (sm_bOverrideShakeDur1)
		Duration = sm_iShakeDuration1;
#endif // __BANK

	m_controls[controller].StartPlayerPadShake_Distance(Duration, Frequency, X, Y, Z);
}

inline void CControlMgr::StartPlayerPadShaking(s32 MotorFrequency0, s32 MotorFrequency1, s32 controller)
{
#if __BANK
	if (sm_bOverrideShakeFreq0)
		MotorFrequency0 = sm_iShakeFrequency0;
	if (sm_bOverrideShakeFreq1)
		MotorFrequency1 = sm_iShakeFrequency1;
#endif // __BANK

	m_controls[controller].StartShaking(MotorFrequency0, MotorFrequency1);
}

inline void CControlMgr::StopPlayerPadShaking(bool bForce, s32 iController)
{
	m_controls[iController].StopPlayerPadShaking(bForce);
}

inline bool CControlMgr::IsPlayerPadShaking(s32 controller)
{
	return m_controls[controller].IsPlayerPadShaking();
}

inline void CControlMgr::SetShakeWhenControllerDisabled(bool bShake)
{
	sm_bShakeWhenControllerDisabled = bShake;
}

#if __WIN32PC
inline void CControlMgr::ApplyDirectionalForce(float X, float Y, s32 timeMS, s32 controller)
{
	m_controls[controller].ApplyDirectionalForce(X, Y, timeMS);
}
#endif // __WIN32PC
#endif
