#ifndef __INC_TEXT_INPUT_BOX_H__
#define __INC_TEXT_INPUT_BOX_H__
#if RSG_PC

// Rage headers
#include "data/callback.h"
#include "atl/singleton.h"
#include "input/virtualkeyboard.h"

// Game headers
#include "text/messages.h"
#include "text/GxtString.h"
#include "Frontend/CMenuBase.h"
#include "Scaleform/ScaleFormMgr.h"

class CTextInputBox : public ::rage::datBase
{
public:
	CTextInputBox();
	~CTextInputBox() {};

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	void Open(const ioVirtualKeyboard::Params& params);
	void Close();
	void HandleInput();
	void SetCursorLocation(int iCursorLocation);
	
	void UpdateDisplayParam();

    // DO NOT DO THIS! UTF-8 functions use alloca, so you return stack memory
    // If you need to access the string as UTF-8 get the char16* and convert it yourself
	//char* GetText() { USES_CONVERSION; return WIDE_TO_UTF8(m_Text); }

	char16* GetTextChar16() { return m_Text; }
    u32 GetMaxLength() const { return m_uMaxLength; }

	void SetAllowComma(bool bAllowComma) { m_bAllowComma = bAllowComma; }

	ioVirtualKeyboard::eKBState GetState() { return m_eState; }
	void DestroyState() { m_eState = ioVirtualKeyboard::kKBState_INVALID; }

	void SetEnabled(bool isEnabled) { m_bEnabled = isEnabled; }
	bool IsActive() { return m_bActive; }

	static void Update();
	static void Render();
	static void HandleXML(parTreeNode* pButtonMenu);

	static void OnSignOut();

	datCallback m_AcceptCallback;
	datCallback m_DeclineCallback;

#if __BANK
	static void InitWidgets();
	static void CreateBankWidgets();
	static void DebugOpen();
	static void DebugClose();
	static void ReadjustOrientation();
#endif //__BANK

	static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);
	static void ClearInstructionalButtons(CScaleformMovieWrapper* pOverride = NULL);
	static void SetInstructionalButtons(u64 iFlags, CScaleformMovieWrapper* pOverride = NULL);
	static int GetButtonMaskIndex(InputType eButton);

private:
	void UpdateMenu();
	void RenderMenu();

	bool UpdateInput();

	void LoadTextInputBoxMovie();
	void RemoveTextInputBoxMovie();

	void SetInvalidCharacters();
	void SetValidCharacters();
	bool CheckCharacterValidity(rage::char16 c);
	bool IsCharacterValid(rage::char16 c);
	bool IsCharacterInvalid(rage::char16 c);

	void ShiftArrayBack(int iIndex);
	void ShiftArrayForward(int iIndex);
	int StringLengthChar16(char16* str);

	bool m_bEnabled;
	bool m_bActive;
	bool m_bImeTextInputHasFocus;
	bool m_bWaitForEnterRelease;
	bool m_bWaitForBackRelease;
	char16 m_Text[ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD + 1];
	ioVirtualKeyboard::eKBState m_eState;

	bool m_bCursorThisFrame;
	bool m_bCursorPreviousFrame;
	bool m_bLastInputKeyboard;
	bool m_bAllowComma;
	u32 m_uLastInputTime;
	u32 m_uKeyHeldDelay;
	u32 m_uCursorLocation;
	u32 m_uMaxLength;
	ioVirtualKeyboard::eTextType m_eTextType;

	atArray<char16> m_ValidCharacters;
	atArray<char16> m_InvalidCharacters;

	
	// buttons are indexed by bit value, ie, BIT(0), BIT(1), etc.
	static atArray<CMenuButtonWithSound>	ms_ButtonData;
	static atRangeArray<u64,4>	ms_AffectedButtonMasks; // only 4 for A,B,X,Y (for now?)
	static CScaleformMovieWrapper ms_Movie;


	int m_iMovieID;
};

typedef atSingleton<CTextInputBox> STextInputBox;

#endif
#endif // __INC_TEXT_INPUT_BOX_H__