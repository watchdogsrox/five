/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Keyboard.cpp
// PURPOSE : manages keyboard game states
// AUTHOR  : Derek Payne
// STARTED : 26/05/2003
// REVISED : 10/11/2005
//
/////////////////////////////////////////////////////////////////////////////////

#include "system/keyboard.h"

// C headers
#include <stdio.h>
#include <string.h>
// Rage headers
#include "grcore/debugdraw.h"
#include "input\input.h"
#include "input/mapper.h"
#include "parsercore/streamxml.h"
#include "string/unicode.h"
#include "system/param.h"
#include "system/threadtype.h"

// framework headers

// Game headers
#include "debug/debug.h"

#include "text/messages.h"
#include "text/text.h"
#include "Text/TextConversion.h"
#include "system/control.h"


#if !__FINAL
PARAM(kblevels, "[game] Start game in Level Keyboard Mode");
PARAM(kbmodebutton, "[game] Sets the button to be used for switching keyboard modes.");
PARAM(kbDebugOnlySwitch, "[game] Toggle only between game and debug keyboard modes.");
PARAM(kbDontUseControlModeSwitch, "[game] Don't use a Control+Key press to toggle game modes.");

// used for finding what key to use for switching between keyboard modes.
#include "parser/memberenumdata.h"
extern ::rage::parEnumData parser_rage__ioMapperParameter_Data;

atString CKeyboard::ms_ThreadlStack;
sysCriticalSectionToken	CKeyboard::ms_Lock;
const char*				CKeyboard::ms_keyNames[KEY_MAX_CODES];

#if __WIN32PC
PARAM(kbgame, "[game] Start game in Game Keyboard Mode");

// the mouse is needed so we can indicate when to lock the mouse in dev mode.
#include "input/mouse.h"
#endif // __WIN32PC
#endif // !__FINAL

// The default key for switching between keyboard modes.
rage::ioMapperParameter CKeyboard::ms_keyboardModeButton = rage::KEY_TAB;

////////////////////////////////////////////////////////////////////////////
// name:	Init
//
// purpose: initialises the keyboard and keyboard states
////////////////////////////////////////////////////////////////////////////
void CKeyboard::Init()
{
#if !__FINAL
	sysCriticalSection lock(m_KeyUsageCs);
	for (u32 mode=0 ; mode<KEYBOARD_MODE_OVERRIDE ; ++mode)
	{
		for (u32 c = 0; c < KEY_MAX_CODES; ++c) 
		{
			for (u32 u = 0; u < KEY_MAX_USES; ++u) 
			{
				key_usage[mode][c][u] = NULL;
				key_dept[mode][c][u]  = NULL;
			}
		}
	}

	m_currentKeyboardMode = KEYBOARD_MODE_DEBUG;
#	if __WIN32PC
	if(PARAM_kbgame.Get())
		m_currentKeyboardMode = KEYBOARD_MODE_GAME;

	ioMouse::SetAbsoluteOnly(m_currentKeyboardMode != KEYBOARD_MODE_GAME);

	m_lastKeyboardMode = -1;
#	endif // __WIN32PC
	ioMapper::SetEnableKeyboard(m_currentKeyboardMode == KEYBOARD_MODE_GAME);

	const char* keyName;
	if(PARAM_kbmodebutton.Get(keyName))
	{
		bool found = false;
		for(u32 i = 0; i < parser_rage__ioMapperParameter_Data.m_NumEnums && !found; ++i)
		{
			if(strcmp(keyName, parser_rage__ioMapperParameter_Data.m_Names[i]) == 0)
			{
				found = true;
				// check if the parameter is actually a key.
				if((parser_rage__ioMapperParameter_Data.m_Enums[i].m_Value & IOMT_KEYBOARD) == IOMT_KEYBOARD)
				{
					ms_keyboardModeButton = static_cast<ioMapperParameter>(parser_rage__ioMapperParameter_Data.m_Enums[i].m_Value);
				}
			}
		}
	}

	// Copy the key names in the static buffer
	for(s32 key=1; key<KEY_MAX_CODES; ++key)
	{
		const char* paramName = NULL;

		for(u32 i = 0; i < parser_rage__ioMapperParameter_Data.m_NumEnums; ++i)
		{
			if(parser_rage__ioMapperParameter_Data.m_Enums[i].m_Value == static_cast<ioMapperParameter>(key))
				paramName = parser_rage__ioMapperParameter_Data.m_Names[i];
		}

		if(paramName != NULL && strcmp(paramName, "") != 0)
		{
			if(Verifyf(strnicmp("KEY_", paramName, 4) == 0, "Invalid Key Code %d", key))
			{
				paramName += 4;
			}
			ms_keyNames[key] = paramName;
		}
		else
		{
			//Assertf(false, "Invalid Key Code %d", key);
			ms_keyNames[key] = "Invalid Key";
		}
	}

#else
		m_currentKeyboardMode = KEYBOARD_MODE_GAME;
#endif // !__FINAL

	inputBufferChanged = false;
	m_keyInput[0] = '\0';
}



////////////////////////////////////////////////////////////////////////////
// name:	Update
//
// purpose: updates the current state of the current key pressed on the
//			keyboard, so we know if it is still pressed or not.
////////////////////////////////////////////////////////////////////////////
void CKeyboard::Update()
{
#if !__FINAL
	static atArray<int> m_fakeKeypressesCopy;
	// clear keypresses from previous frame
	while(m_fakeKeypressesCopy.GetCount())
	{
		int c = m_fakeKeypressesCopy.Pop();
		KEYBOARD.SetKey(c, false);
	}
	m_fakeKeypressesCopy = m_fakeKeypresses;

	// set fake keypresses
	while(m_fakeKeypresses.GetCount())
	{
		int c = m_fakeKeypresses.Pop();
		KEYBOARD.SetKey(c, true);
	}


	if(m_currentKeyboardMode < KEYBOARD_MODE_OVERRIDE)
	{
		// cycle through keyboards modes, we use KEYBOARD directly here so it
		// works in game mode.
		if( KEYBOARD.KeyPressed(ms_keyboardModeButton) != 0 && KEYBOARD.KeyDown(KEY_LMENU) == 0 && KEYBOARD.KeyDown(KEY_RMENU) == 0
			WIN32PC_ONLY(&& (PARAM_kbDontUseControlModeSwitch.Get() || KEYBOARD.KeyDown(KEY_LCONTROL) || KEYBOARD.KeyDown(KEY_RCONTROL)) ))
		{
			if(PARAM_kbDebugOnlySwitch.Get())
			{
				if(m_currentKeyboardMode == KEYBOARD_MODE_GAME)
				{
					m_currentKeyboardMode = KEYBOARD_MODE_DEBUG;
				}
				else
				{
					m_currentKeyboardMode = KEYBOARD_MODE_GAME;
				}
			}
			else
			{
				m_currentKeyboardMode++;
			}


			if(m_currentKeyboardMode == KEYBOARD_LAST_MODE+1)
				m_currentKeyboardMode = KEYBOARD_FIRST_MODE;

#if __WIN32PC && !__FINAL
			if(m_currentKeyboardMode == KEYBOARD_MODE_GAME)
				ioMouse::SetAbsoluteOnly(false);
			else if(m_lastKeyboardMode == KEYBOARD_MODE_GAME)
				ioMouse::SetAbsoluteOnly(true);

			m_lastKeyboardMode = m_currentKeyboardMode;
#endif // __WIN32PC && !__FINAL
			WIN32PC_ONLY(if(!CSystem::IsThisThreadId(SYS_THREAD_RENDER)))
			{
				ioMapper::SetEnableKeyboard(m_currentKeyboardMode == KEYBOARD_MODE_GAME);

				char keyboardMode[50];
				CKeyboard::FillStringWithKeyboardMode(keyboardMode, 50);

				CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, keyboardMode);
			}
		}
	}
#endif

#if __WIN32PC
	// get input from keyboard
	KEYBOARD.GetBufferedInput(m_NewKeyInput, 16);
	ConvertInputBufferIntoASCII(m_NewKeyInput, m_NewKeyInput);

	if (strcmp(m_NewKeyInput, m_keyInput) == 0)
		inputBufferChanged = false;
	else
		inputBufferChanged = true;
	strcpy(m_keyInput, m_NewKeyInput);
#endif
}

////////////////////////////////////////////////////////////////////////////
// name:	Shutdown
//
// purpose: Frees the usage and dept memory we previously allocated
////////////////////////////////////////////////////////////////////////////
void CKeyboard::Shutdown()
{
#if !__FINAL
	for (u32 mode=0 ; mode<KEYBOARD_MODE_OVERRIDE ; ++mode)
	{
		for (u32 c = 0; c < KEY_MAX_CODES; ++c) 
		{
			for (u32 u = 0; u < KEY_MAX_USES; ++u) 
			{
				if (key_usage[mode][c][u])
				{
					delete key_usage[mode][c][u];
					delete key_dept[mode][c][u];
				}
			}
		}
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////
// name:	ConvertInputBufferIntoASCII
//
// purpose: Convert the input buffer into ASCII
/////////////////////////////////////////////////////////////////////////////
void CKeyboard::ConvertInputBufferIntoASCII(char *input, char *output)
{
	output = input;
	while (*input != '\0')
	{
		switch (*(u8*)input)
		{
			case KEY_ESCAPE          : 
				break;

			case KEY_NUMPAD1         :
			case KEY_1               :
				*output = '1';
				break;
			case KEY_NUMPAD2         :
			case KEY_2               :
				*output = '2';
				break;
			case KEY_NUMPAD3         :
			case KEY_3               :
				*output = '3';
				break;
			case KEY_NUMPAD4         :
			case KEY_4               :
				*output = '4';
				break;
			case KEY_NUMPAD5         :
			case KEY_5               :
				*output = '5';
				break;
			case KEY_NUMPAD6         :
			case KEY_6               :
				*output = '6';
				break;
			case KEY_NUMPAD7         :
			case KEY_7               :
				*output = '7';
				break;
			case KEY_NUMPAD8         :
			case KEY_8               :
				*output = '8';
				break;
			case KEY_NUMPAD9         :
			case KEY_9               :
				*output = '9';
				break;
			case KEY_NUMPAD0         :
			case KEY_0               :
				*output = '0';
				break;
			case KEY_SUBTRACT        :
			case KEY_MINUS           :
				*output = '-';
				break;
			case KEY_EQUALS          :
				*output = '=';
				break;
			case KEY_TAB             :
				*output = 0;
				break;
			case KEY_Q               :
				*output = 'q';
				break;
			case KEY_W               :
				*output = 'w';
				break;
			case KEY_E               :
				*output = 'e';
				break;
			case KEY_R               :
				*output = 'r';
				break;
			case KEY_T               :
				*output = 't';
				break;
			case KEY_Y               :
				*output = 'y';
				break;
			case KEY_U               :
				*output = 'u';
				break;
			case KEY_I               :
				*output = 'i';
				break;
			case KEY_O               :
				*output = 'o';
				break;
			case KEY_P               :
				*output = 'p';
				break;
			case KEY_LBRACKET        :
				*output = '[';
				break;
			case KEY_RBRACKET        :
				*output = ']';
				break;
			case KEY_A               :
				*output = 'a';
				break;
			case KEY_S               :
				*output = 's';
				break;
			case KEY_D               :
				*output = 'd';
				break;
			case KEY_F               :
				*output = 'f';
				break;
			case KEY_G               :
				*output = 'g';
				break;
			case KEY_H               :
				*output = 'h';
				break;
			case KEY_J               :
				*output = 'j';
				break;
			case KEY_K               :
				*output = 'k';
				break;
			case KEY_L               :
				*output = 'l';
				break;
			case KEY_SEMICOLON       :
				*output = ';';
				break;
			case KEY_APOSTROPHE      :
				*output = '\'';
				break;
			case KEY_GRAVE           :
				*output = '#';
				break;
			case KEY_RSHIFT          :
			case KEY_LSHIFT          :
				*output = 0;
				break;
			case KEY_BACKSLASH       :
				*output = 92;
				break;
			case KEY_Z               :
				*output = 'z';
				break;
			case KEY_X               :
				*output = 'x';
				break;
			case KEY_C               :
				*output = 'c';
				break;
			case KEY_V               :
				*output = 'v';
				break;
			case KEY_B               :
				*output = 'b';
				break;
			case KEY_N               :
				*output = 'n';
				break;
			case KEY_M               :
				*output = 'm';
				break;
			case KEY_COMMA           :
				*output = ',';
				break;
			case KEY_PERIOD          :
			case KEY_DECIMAL         :
				*output = '.';
				break;
			case KEY_SLASH           :
				*output = 47;
				break;
			case KEY_MULTIPLY        :
				*output = 42;
				break;
			case KEY_SPACE           :
				*output = ' ';
				break;
			case KEY_ADD             :
				*output = '+';
				break;
// 			case KEY_AT              :
// 				*output = '@';
// 				break;
// 			case KEY_COLON           :
// 				*output = ':';
// 				break;
// 			case KEY_UNDERLINE       :
// 				*output = '_';
// 				break;
			case KEY_DIVIDE          :
				*output = 47;
				break;
			default:
				*output = *input;
				break;

		}	
		input++;
		output++;
	}
}

////////////////////////////////////////////////////////////////////////////
// name:	GetAlteredKeyboardMode
//
// purpose: returns the current keyboard mode, taking into account ctrl, shift and alt keys
////////////////////////////////////////////////////////////////////////////
s32 CKeyboard::GetAlteredKeyboardMode(s32 NOTFINAL_ONLY(key_code))
{
#if !__FINAL
	if(m_currentKeyboardMode == KEYBOARD_MODE_DEBUG)
	{
		const bool bControl = (KEYBOARD.KeyDown(KEY_LCONTROL) && key_code != KEY_LCONTROL) || (KEYBOARD.KeyDown(KEY_RCONTROL) && key_code != KEY_RCONTROL);
		const bool bShift   = (KEYBOARD.KeyDown(KEY_LSHIFT  ) && key_code != KEY_LSHIFT  ) || (KEYBOARD.KeyDown(KEY_RSHIFT  ) && key_code != KEY_RSHIFT  );
		const bool bAlt     = (KEYBOARD.KeyDown(KEY_LMENU   ) && key_code != KEY_LMENU   ) || (KEYBOARD.KeyDown(KEY_RMENU   ) && key_code != KEY_RMENU   );

		if      (bControl && bShift && bAlt) { return KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT; }
		else if (bControl && bShift)         { return KEYBOARD_MODE_DEBUG_CNTRL_SHIFT; }
		else if (bControl && bAlt)           { return KEYBOARD_MODE_DEBUG_CNTRL_ALT; }
		else if (bShift   && bAlt)           { return KEYBOARD_MODE_DEBUG_SHIFT_ALT; }
		else if (bControl)                   { return KEYBOARD_MODE_DEBUG_CTRL; }
		else if (bShift)                     { return KEYBOARD_MODE_DEBUG_SHIFT; }
		else if (bAlt)                       { return KEYBOARD_MODE_DEBUG_ALT; }
	}
#if GTA_REPLAY
	else if(m_currentKeyboardMode == KEYBOARD_MODE_REPLAY)
	{
		const bool bControl = (KEYBOARD.KeyDown(KEY_LCONTROL) && key_code != KEY_LCONTROL) || (KEYBOARD.KeyDown(KEY_RCONTROL) && key_code != KEY_RCONTROL);
		const bool bShift   = (KEYBOARD.KeyDown(KEY_LSHIFT  ) && key_code != KEY_LSHIFT  ) || (KEYBOARD.KeyDown(KEY_RSHIFT  ) && key_code != KEY_RSHIFT  );
		const bool bAlt     = (KEYBOARD.KeyDown(KEY_LMENU   ) && key_code != KEY_LMENU   ) || (KEYBOARD.KeyDown(KEY_RMENU   ) && key_code != KEY_RMENU   );

		if      (bControl && bShift && bAlt) { return KEYBOARD_MODE_REPLAY_CNTRL_SHIFT_ALT; }
		else if (bControl && bShift)         { return KEYBOARD_MODE_REPLAY_CNTRL_SHIFT; }
		else if (bControl && bAlt)           { return KEYBOARD_MODE_REPLAY_CNTRL_ALT; }
		else if (bShift   && bAlt)           { return KEYBOARD_MODE_REPLAY_SHIFT_ALT; }
		else if (bControl)                   { return KEYBOARD_MODE_REPLAY_CTRL; }
		else if (bShift)                     { return KEYBOARD_MODE_REPLAY_SHIFT; }
		else if (bAlt)                       { return KEYBOARD_MODE_REPLAY_ALT; }
	}
#endif
#endif
	return m_currentKeyboardMode; 
}

////////////////////////////////////////////////////////////////////////////
// name:	GetKeyJustDown
//
// purpose: returns the status of the passed key scan code
////////////////////////////////////////////////////////////////////////////
bool CKeyboard::GetKeyJustDown(s32 key_code, s32 use_mode, const char *BANK_ONLY(usage), const char *BANK_ONLY(dept))
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
#if __BANK
	if(use_mode < KEYBOARD_MODE_OVERRIDE)
	{
		for (u32 u = 0; u < KEY_MAX_USES; ++u)
		{
			if (key_usage[use_mode][key_code][u] == NULL)
			{
				USE_DEBUG_MEMORY();
				const char* _usage	= usage ? usage : "UNDEFINED. Please change this !";
				const char* _dept	= dept ? dept : "UNDEFINED. Please change this !";
				size_t usageLen = strlen(_usage)+1;
				size_t deptLen  = strlen(_dept)+1;
				key_usage[use_mode][key_code][u] = rage_new char[usageLen];
				key_dept[use_mode][key_code][u]  = rage_new char[deptLen];
				safecpy(key_usage[use_mode][key_code][u], _usage, usageLen);
				safecpy(key_dept[use_mode][key_code][u], _dept, deptLen);
			}
		}
	}
	controlDebugf2("Testing CKeyboard::GetKeyJustDown() for %s.", usage);
#endif

	if (!IsCorrectKeyboardMode(use_mode, key_code)) 
	{
		controlDebugf2("Test for %s failed. Use mode %d but needed %d.",
			CControl::GetParameterText(static_cast<ioMapperParameter>(key_code)),
			GetAlteredKeyboardMode(key_code),
			use_mode);

		NOTFINAL_ONLY(DebugOutputScriptCallStack());
		return false;
	}

	bool result = (KEYBOARD.KeyPressed(key_code) != 0);

	controlDebugf2("%s is %sJust Down.",
		CControl::GetParameterText(static_cast<ioMapperParameter>(key_code)),
		(result ? "" : "Not "));

	NOTFINAL_ONLY(DebugOutputScriptCallStack());

	return result;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetKeyJustUp
//
// purpose: returns the status of the passed key scan code
////////////////////////////////////////////////////////////////////////////
bool CKeyboard::GetKeyJustUp(s32 key_code, s32 use_mode, const char *BANK_ONLY(usage), const char *BANK_ONLY(dept))
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
#if __BANK
	if(use_mode < KEYBOARD_MODE_OVERRIDE)
	{
		for (u32 u = 0; u < KEY_MAX_USES; ++u)
		{
			if (key_usage[use_mode][key_code][u] == NULL)
			{
				USE_DEBUG_MEMORY();
				const char* _usage	= usage ? usage : "UNDEFINED. Please change this !";
				const char* _dept	= dept ? dept : "UNDEFINED. Please change this !";
				size_t usageLen = strlen(_usage)+1;
				size_t deptLen  = strlen(_dept)+1;
				key_usage[use_mode][key_code][u] = rage_new char[usageLen];
				key_dept[use_mode][key_code][u]  = rage_new char[deptLen];
				safecpy(key_usage[use_mode][key_code][u], _usage, usageLen);
				safecpy(key_dept[use_mode][key_code][u], _dept, deptLen);
			}
		}
	}
	controlDebugf2("Testing CKeyboard::GetKeyJustUp() for %s.", usage);
#endif

	if (!IsCorrectKeyboardMode(use_mode, key_code)) 
	{
		controlDebugf2("Test for %s failed. Use mode %d but needed %d.",
			CControl::GetParameterText(static_cast<ioMapperParameter>(key_code)),
			GetAlteredKeyboardMode(key_code),
			use_mode);

		NOTFINAL_ONLY(DebugOutputScriptCallStack());
		return false;
	}

	bool result = (KEYBOARD.KeyReleased(key_code) != 0);

	controlDebugf2("%s is %sJust Up.",
		CControl::GetParameterText(static_cast<ioMapperParameter>(key_code)),
		(result ? "" : "Not "));

	NOTFINAL_ONLY(DebugOutputScriptCallStack());

	return result;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetKeyUp
//
// purpose: returns the status of the passed key scan code
////////////////////////////////////////////////////////////////////////////
bool CKeyboard::GetKeyUp(s32 key_code, s32 use_mode, const char *BANK_ONLY(usage), const char *BANK_ONLY(dept))
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
#if __BANK
	if(use_mode < KEYBOARD_MODE_OVERRIDE)
	{
		for (u32 u = 0; u < KEY_MAX_USES; ++u)
		{
			if (key_usage[use_mode][key_code][u] == NULL)
			{
				USE_DEBUG_MEMORY();
				const char* _usage	= usage ? usage : "UNDEFINED. Please change this !";
				const char* _dept	= dept ? dept : "UNDEFINED. Please change this !";
				size_t usageLen = strlen(_usage)+1;
				size_t deptLen  = strlen(_dept)+1;
				key_usage[use_mode][key_code][u] = rage_new char[usageLen];
				key_dept[use_mode][key_code][u]  = rage_new char[deptLen];
				safecpy(key_usage[use_mode][key_code][u], _usage, usageLen);
				safecpy(key_dept[use_mode][key_code][u], _dept, deptLen);
			}
		}
	}
	controlDebugf2("Testing CKeyboard::GetKeyUp() for %s.", usage);
#endif

	if (!IsCorrectKeyboardMode(use_mode, key_code)) 
	{
		controlDebugf2("Test for %s failed. Use mode %d but needed %d.",
			CControl::GetParameterText(static_cast<ioMapperParameter>(key_code)),
			GetAlteredKeyboardMode(key_code),
			use_mode);

		NOTFINAL_ONLY(DebugOutputScriptCallStack());
		return false;
	}

	bool result = (KEYBOARD.KeyUp(key_code) != 0);

	controlDebugf2("%s is %sUp.",
		CControl::GetParameterText(static_cast<ioMapperParameter>(key_code)),
		(result ? "" : "Not "));

	NOTFINAL_ONLY(DebugOutputScriptCallStack());

	return result;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetKeyDown
//
// purpose: returns the status of the passed key scan code
////////////////////////////////////////////////////////////////////////////
bool CKeyboard::GetKeyDown(s32 key_code, s32 use_mode, const char *BANK_ONLY(usage), const char *BANK_ONLY(dept))
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
#if __BANK
	if(use_mode < KEYBOARD_MODE_OVERRIDE)
	{
		for (u32 u = 0; u < KEY_MAX_USES; ++u)
		{
			if (key_usage[use_mode][key_code][u] == NULL)
			{
				USE_DEBUG_MEMORY();
				const char* _usage	= usage ? usage : "UNDEFINED. Please change this !";
				const char* _dept	= dept ? dept : "UNDEFINED. Please change this !";
				size_t usageLen = strlen(_usage)+1;
				size_t deptLen  = strlen(_dept)+1;
				key_usage[use_mode][key_code][u] = rage_new char[usageLen];
				key_dept[use_mode][key_code][u]  = rage_new char[deptLen];
				safecpy(key_usage[use_mode][key_code][u], _usage, usageLen);
				safecpy(key_dept[use_mode][key_code][u], _dept, deptLen);
			}
		}
	}
	controlDebugf2("Testing CKeyboard::GetKeyDown() for %s.", usage);
#endif

	if (!IsCorrectKeyboardMode(use_mode, key_code)) 
	{
		controlDebugf2("Test for %s failed. Use mode %d but needed %d.",
			CControl::GetParameterText(static_cast<ioMapperParameter>(key_code)),
			GetAlteredKeyboardMode(key_code),
			use_mode);

		NOTFINAL_ONLY(DebugOutputScriptCallStack());
		return false;
	}

	bool result = (KEYBOARD.KeyDown(key_code) != 0);

	controlDebugf2("%s is %sDown.",
		CControl::GetParameterText(static_cast<ioMapperParameter>(key_code)),
		(result ? "" : "Not "));

	NOTFINAL_ONLY(DebugOutputScriptCallStack());

	return result;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetCurrentKeyPressed
//
// purpose: returns what key is currently pressed
////////////////////////////////////////////////////////////////////////////
const char* CKeyboard::GetKeyPressedBuffer(s32 use_mode)
{
	if (use_mode != m_currentKeyboardMode && use_mode != KEYBOARD_MODE_OVERRIDE) 
		return NULL;

#if __WIN32
	// standard win32 input
	return m_keyInput;
#else
#if !__FINAL
	// on consoles, we need to do it manually - nasty but debug only (for NODE VIEWER only at the moment)
	// DP: do not put this shite in the final game, if we want to do this in the final thing, we should
	// find a nicer way to do it.
	for (s32 iKeyCode = KEY_ESCAPE; iKeyCode < KEY_CHATPAD_ORANGE_SHIFT; iKeyCode++)
	{
		if (GetKeyJustDown(iKeyCode, use_mode))
		{
			m_NewKeyInput[0] = iKeyCode;
			m_NewKeyInput[1] = '\0';

			ConvertInputBufferIntoASCII(m_NewKeyInput, m_NewKeyInput);
			return m_NewKeyInput;
		}
	}
#endif
	return NULL;
#endif
}





bool CKeyboard::IsCorrectKeyboardMode( s32 use_mode, s32 key_code )
{
	return use_mode == GetAlteredKeyboardMode(key_code) 
		|| ( use_mode == KEYBOARD_MODE_OVERRIDE 
		KEYBOARD_MOUSE_ONLY(&& m_currentKeyboardMode != KEYBOARD_MODE_GAME) ); // Do not allow KEYBOARD_MODE_OVERRIDE when the keyboard could be used for playing the game.
}

#if !__FINAL

// some debug functions:

/////////////////////////////////////////////////////////////////////////////
// name:	GetKeyUsage
//
// purpose: returns the string explaining the usage for this key (debug only)
/////////////////////////////////////////////////////////////////////////////
const char *CKeyboard::GetKeyUsage(s32 key_code, s32 use_mode, char* buffer, u32 maxBufferLen, u32 use)
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
	Assertf(key_code > 0, "CKeyboard::GetKeyUsage - key_code 0 is not a valid key");
	if (key_usage[use_mode][key_code][use])
		safecpy(buffer, key_usage[use_mode][key_code][use], maxBufferLen);
	else
		safecpy(buffer, "Key Usage undefined", maxBufferLen);
	return buffer;
}

/////////////////////////////////////////////////////////////////////////////
// name:	GetKeyDept
//
// purpose: returns the department that use the key in code
/////////////////////////////////////////////////////////////////////////////
const char *CKeyboard::GetKeyDept(s32 key_code, s32 use_mode, char* buffer, u32 maxBufferLen, u32 use)
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
	Assertf(key_code > 0, "CKeyboard::GetKeyDept - key_code 0 is not a valid key");
	if (key_dept[use_mode][key_code][use])
		safecpy(buffer, key_dept[use_mode][key_code][use], maxBufferLen);
	else
		safecpy(buffer, "Key Dept undefined", maxBufferLen);
	return buffer;
}

/////////////////////////////////////////////////////////////////////////////
// name:	GetKeyName
//
// purpose: returns a string with the actual key name (used in debug
//			menus) - should be used to replace use of KeyToAscii
//			
/////////////////////////////////////////////////////////////////////////////
const char* CKeyboard::GetKeyName(s32 key_code, char* pName, s32 iMaxChars)
{
	Assertf(key_code > 0, "CKeyboard::GetKeyName - key_code 0 is not a valid key");
	
	if (ms_keyNames[key_code])
		safecpy(pName, ms_keyNames[key_code], iMaxChars);
	else
		safecpy(pName, "Key Name undefined", iMaxChars);
	return pName;
}

/////////////////////////////////////////////////////////////////////////////
// name:	HasUsage
//
// purpose: returns a true if the key is used, false otherwise
//			
/////////////////////////////////////////////////////////////////////////////
bool CKeyboard::HasKeyUsage(s32 key_code, s32 use_mode, u32 use)
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
	Assertf(key_code > 0, "CKeyboard::HasKeyUsage - key_code 0 is not a valid key");
	return key_usage[use_mode][key_code][use] != NULL;
}

//
// name:		PrintDebugKeys
// description:	Print the debug key bindings
void CKeyboard::PrintDebugKeys()
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
#if DEBUG_DRAW
	char key_name[32];
	char key_usage[512];
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG))
			grcDebugDraw::AddDebugOutput("%s: %s", GetKeyName(i, key_name, 32), GetKeyUsage(i, KEYBOARD_MODE_DEBUG, key_usage, 512));
	}
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CTRL))
			grcDebugDraw::AddDebugOutput("Ctrl+%s: %s", GetKeyName(i, key_name, 32), GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CTRL, key_usage, 512));
	}
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT))
			grcDebugDraw::AddDebugOutput("Shift+%s: %s", GetKeyName(i, key_name, 32), GetKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT, key_usage, 512));
	}
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_ALT))
			grcDebugDraw::AddDebugOutput("Alt+%s: %s", GetKeyName(i, key_name, 32), GetKeyUsage(i, KEYBOARD_MODE_DEBUG_ALT, key_usage, 512));
	}
#endif	// DEBUG_DRAW
}

//
// name:		PrintMarketingKeys
// description:	Print the marketing debug key bindings
void CKeyboard::PrintMarketingKeys()
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
#if DEBUG_DRAW
	char key_name[16];
	char key_usage[512];
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_MARKETING))
		{
			grcDebugDraw::AddDebugOutput("%s: %s", GetKeyName(i, key_name, 16), GetKeyUsage(i, KEYBOARD_MODE_MARKETING, key_usage, 512));
		}
	}
#endif	// DEBUG_DRAW
}

//
// name:		DumpDebugKeysToXML
// description:	Output the debug key to an XML stream
void CKeyboard::DumpDebugKeysToXML(fiStream* stream)
{
	NOTFINAL_ONLY(sysCriticalSection lock(m_KeyUsageCs));
	char key_name[32];
	char key_usage[512];
	char key_dept[512];

	fprintf(stream, "<Mappings>\n");

	// normal Keys
	fprintf(stream, "<Debug>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
				GetKeyName(i, key_name, 32),
				GetKeyDept(i, KEYBOARD_MODE_DEBUG, key_dept, 512),
				GetKeyUsage(i, KEYBOARD_MODE_DEBUG, key_usage, 512));
		}
	}
	fprintf(stream, "</Debug>\n");

	// Ctrl Keys
	fprintf(stream, "<Debug_Ctrl>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CTRL))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
				GetKeyName(i, key_name, 32),
				GetKeyDept(i, KEYBOARD_MODE_DEBUG_CTRL, key_dept, 512),
				GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CTRL, key_usage, 512));
		}
	}
	fprintf(stream, "</Debug_Ctrl>\n");
	
	// Shift Keys
	fprintf(stream, "<Debug_Shift>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
				GetKeyName(i, key_name, 32),
				GetKeyDept(i, KEYBOARD_MODE_DEBUG_SHIFT, key_dept, 512),
				GetKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT, key_usage, 512));
		}
	}
	fprintf(stream, "</Debug_Shift>\n");
	
	// Alt Keys
	fprintf(stream, "<Debug_Alt>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_ALT))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
				GetKeyName(i, key_name, 32),
				GetKeyDept(i, KEYBOARD_MODE_DEBUG_ALT, key_dept, 512),
				GetKeyUsage(i, KEYBOARD_MODE_DEBUG_ALT, key_usage, 512));
		}
	}
	fprintf(stream, "</Debug_Alt>\n");
	
	// Ctrl + Shift Keys
	fprintf(stream, "<Debug_Ctrl_Shift>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
				GetKeyName(i, key_name, 32),
				GetKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT, key_dept, 512),
				GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT, key_usage, 512));
		}
	}
	fprintf(stream, "</Debug_Ctrl_Shift>\n");

	// Ctrl + Alt Keys
	fprintf(stream, "<Debug_Ctrl_Alt>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_ALT))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
				GetKeyName(i, key_name, 32),
				GetKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_ALT, key_dept, 512),
				GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_ALT, key_usage, 512));
		}
	}
	fprintf(stream, "</Debug_Ctrl_Alt>\n");

	// Shift + Alt Keys
	fprintf(stream, "<Debug_Shift_Alt>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT_ALT))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
				GetKeyName(i, key_name, 32),
				GetKeyDept(i, KEYBOARD_MODE_DEBUG_SHIFT_ALT, key_dept, 512),
				GetKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT_ALT, key_usage, 512));
		}
	}
	fprintf(stream, "</Debug_Shift_Alt>\n");

	// Shift + Alt Keys
	fprintf(stream, "<Debug_Ctrl_Shift_Alt>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
					GetKeyName(i, key_name, 32),
					GetKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT, key_dept, 512),
					GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT, key_usage, 512));
		}
	}
	fprintf(stream, "</Debug_Ctrl_Shift_Alt>\n");

	fprintf(stream, "<Marketing>\n");
	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		if(HasKeyUsage(i, KEYBOARD_MODE_MARKETING))
		{
			fprintf(stream, "<mapping Key=\"%s\" Dept=\"%s\" Usage=\"%s\"/>\n",
				GetKeyName(i, key_name, 32),
				GetKeyDept(i, KEYBOARD_MODE_MARKETING, key_dept, 512),
				GetKeyUsage(i, KEYBOARD_MODE_MARKETING, key_usage, 512));
		}
	}
	fprintf(stream, "</Marketing>\n");

	fprintf(stream, "</Mappings>\n");
}

char* CKeyboard::FillStringWithKeyboardMode(char* pString, s32 length)
{
	switch (m_currentKeyboardMode)
	{
	case KEYBOARD_MODE_DEBUG :
		strncpy(pString, "Keyboard : Debug", length);
		break;
	case KEYBOARD_MODE_DEBUG_SHIFT :
		strncpy(pString, "Keyboard : Debug Shift", length);
		break;
	case KEYBOARD_MODE_DEBUG_CTRL :
		strncpy(pString, "Keyboard : Debug Ctrl", length);
		break;
	case KEYBOARD_MODE_DEBUG_ALT :
		strncpy(pString, "Keyboard : Debug Alt", length);
		break;
	case KEYBOARD_MODE_GAME :
		strncpy(pString, "Keyboard : Game", length);
		break;
	case KEYBOARD_MODE_MARKETING :
		strncpy(pString, "Keyboard : Marketing", length);
		break;
	case KEYBOARD_MODE_OVERRIDE :
		strncpy(pString, "Keyboard : Override", length);
		break;
#if GTA_REPLAY
	case KEYBOARD_MODE_REPLAY:
		strncpy(pString, "Keyboard : Replay", length);
		break;
#endif
	default :
		strncpy(pString, "Keyboard : Unknown", length);
		break;
	}
	return pString;
}

void CKeyboard::SetCurrentMode( s32 mode )
{
	m_currentKeyboardMode = mode;
#if !__FINAL && __WIN32PC
	ioMouse::SetAbsoluteOnly(m_currentKeyboardMode != KEYBOARD_MODE_GAME);

	m_lastKeyboardMode = m_currentKeyboardMode;
#endif // !__FINAL && __WIN32PC
	ioMapper::SetEnableKeyboard(m_currentKeyboardMode == KEYBOARD_MODE_GAME);
}

void CKeyboard::ChanneledOutputForStackTrace(const char* fmt, ...)
{
	char buffer[256];
	va_list args;
	va_start(args, fmt);
	vformatf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	ms_ThreadlStack += buffer;
	ms_ThreadlStack += "\n";
}

void CKeyboard::DebugOutputScriptCallStack()
{
	if(Channel_Control.MaxLevel >= DIAG_SEVERITY_DEBUG3 && sysThreadType::IsUpdateThread() && scrThread::GetActiveThread())
	{
		sysCriticalSection critSec(ms_Lock);

		// PrintStackTrace does not exist in final.
		scrThread::GetActiveThread()->PrintStackTrace(ChanneledOutputForStackTrace);
		controlDebugf3("---- With stack: \n%s", ms_ThreadlStack.c_str());
		ms_ThreadlStack.Clear();
	}
}

#endif
