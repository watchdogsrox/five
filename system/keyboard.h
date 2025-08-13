/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Keyboard.cpp
// PURPOSE : header file
// AUTHOR  : Derek Payne
// STARTED : 26/05/2003
// REVISED : 10/11/2005
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_KEYBOARD_H_
#define INC_KEYBOARD_H_

// Rage headers
#include "atl\array.h"
#include "atl\string.h"
#include "input/mapper_defs.h"

//replay include
#include "control/replay/replay.h"

//
// various keyboard modes available (tab toggles between these)
//
enum {
	KEYBOARD_MODE_DEBUG = 0,	// standard keyboard mode
	KEYBOARD_FIRST_MODE = KEYBOARD_MODE_DEBUG,
	KEYBOARD_MODE_GAME,			// game/script keyboard mode
	KEYBOARD_MODE_MARKETING,	// marketing tools keyboard mode
#if GTA_REPLAY
	KEYBOARD_MODE_REPLAY,
	KEYBOARD_LAST_MODE = KEYBOARD_MODE_REPLAY,
#else
	KEYBOARD_LAST_MODE = KEYBOARD_MODE_MARKETING,
#endif
	// NOTE: these have to match "ENUM KEYBOARD_MODIFIER" in commands_pad.sch
	KEYBOARD_MODE_DEBUG_SHIFT = 13, // standard keyboard mode with shift down
	KEYBOARD_MODE_DEBUG_CTRL  = 14, // standard keyboard mode with ctrl down
	KEYBOARD_MODE_DEBUG_ALT   = 15, // standard keyboard mode with alt down

	// all after here are INTERNAL modes (used in code)
	KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT,
	KEYBOARD_MODE_DEBUG_CNTRL_SHIFT,
	KEYBOARD_MODE_DEBUG_CNTRL_ALT,
	KEYBOARD_MODE_DEBUG_SHIFT_ALT,
#if GTA_REPLAY
	KEYBOARD_MODE_REPLAY_SHIFT,
	KEYBOARD_MODE_REPLAY_CTRL,
	KEYBOARD_MODE_REPLAY_ALT, 
	KEYBOARD_MODE_REPLAY_CNTRL_SHIFT_ALT,
	KEYBOARD_MODE_REPLAY_CNTRL_SHIFT,
	KEYBOARD_MODE_REPLAY_CNTRL_ALT,
	KEYBOARD_MODE_REPLAY_SHIFT_ALT,
#endif
	KEYBOARD_MODE_OVERRIDE,		// to make it always accept (except for game mode, url:bugstar:826351).
};

#define KEY_MAX_CODES	0xff
#define KEY_MAX_USES	4

//
// name:		CKeyboard
// description:	Wrapper class for Rage keyboard class
class CKeyboard
{
public:
	CKeyboard() {}

#if !__FINAL
	void SetCurrentMode(s32 mode);
#endif

	void Init();
	void Update();
	void Shutdown();

	bool GetKeyJustDown(s32 key_code, s32 use_mode = KEYBOARD_MODE_DEBUG, const char *usage = NULL, const char *dept = NULL);
	bool GetKeyDown(    s32 key_code, s32 use_mode = KEYBOARD_MODE_DEBUG, const char *usage = NULL, const char *dept = NULL);
	bool GetKeyJustUp(  s32 key_code, s32 use_mode = KEYBOARD_MODE_DEBUG, const char *usage = NULL, const char *dept = NULL);
	bool GetKeyUp(      s32 key_code, s32 use_mode = KEYBOARD_MODE_DEBUG, const char *usage = NULL, const char *dept = NULL);

	bool SetKeyJustDown(s32 key_code, s32 use_mode = KEYBOARD_MODE_DEBUG, const char *usage = NULL);

	const char* GetKeyPressedBuffer(s32 use_mode);

	s32 GetKeyboardMode() {return m_currentKeyboardMode;}
	s32 GetAlteredKeyboardMode(s32 key_code=0);
#if !__FINAL
	// some debug functions:
	const char *GetKeyUsage(s32 key_code, s32 use_mode, char* buffer, u32 maxBufferLen, u32 use=0);
	const char *GetKeyDept(s32 key_code, s32 use_mode, char* buffer, u32 maxBufferLen, u32 use=0);
	const char *GetKeyName(s32 key_code, char* pName, s32 iMaxChars);
	bool HasKeyUsage(s32 key_code, s32 use_mode, u32 use=0);

	void PrintDebugKeys();
	void PrintMarketingKeys();
	void DumpDebugKeysToXML(fiStream* stream);

	char *FillStringWithKeyboardMode(char* pString, s32 length);
	void FakeKeyPress(int c) {m_fakeKeypresses.PushAndGrow(c);}
#endif

private:
	bool  inputBufferChanged;
	void	ConvertInputBufferIntoASCII(char *input, char *output);
	bool IsCorrectKeyboardMode( s32 use_mode, s32 key_code );

	s32 m_currentKeyboardMode;
	char m_keyInput[16];
	char m_NewKeyInput[16];
	bool ms_bDebugOn;

	static ioMapperParameter ms_keyboardModeButton;
#if !__FINAL
	// Debug helper function to aid outputting the current scripts call stack.
	static void DebugOutputScriptCallStack();

	// borrowed from Debug.cpp::DumpThreadState
	static void ChanneledOutputForStackTrace(const char* fmt, ...);

	// each keyboard mode has a max number of keys that can be used a certain amount of times.
	// (we're trying to avoid having a key used everywhere for different things)
	char *key_usage[KEYBOARD_MODE_OVERRIDE][KEY_MAX_CODES][KEY_MAX_USES];	// string explaining current use of this key#
	char *key_dept[KEYBOARD_MODE_OVERRIDE][KEY_MAX_CODES][KEY_MAX_USES];	// code department using the key
	
	static const char* ms_keyNames[KEY_MAX_CODES];
	sysCriticalSectionToken m_KeyUsageCs;

	atArray<int> m_fakeKeypresses;
	static atString ms_ThreadlStack;
	static sysCriticalSectionToken ms_Lock;
#if __WIN32PC
	s32 m_lastKeyboardMode;
#endif
#endif
};


#endif // INC_KEYBOARD_H_
