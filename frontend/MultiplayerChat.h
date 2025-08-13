#if RSG_PC
#ifndef __MULTIPLAYER_CHAT
#define __MULTIPLAYER_CHAT

//Rage
#include "atl/queue.h"
#include "rline/rl.h"


//Game
#include "Network/Live/NetworkClan.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "system/controlMgr.h"

#define MAX_CHAT_MESSAGE_SIZE 140

class CMultiplayerChat
{
public: 
	CMultiplayerChat();
	~CMultiplayerChat();


	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	
	// Intentionally done seperately to isolate actually loading the movie explicitly when in MP only.
	// Init (INIT_SESSION) will get called on start up...so...that's out.
	static void InitSession();
	static void ShutdownSession();
	
	static void Update();
	void UpdateInput();
	static void Render();
	static void UpdateAtEndOfFrame();

	void OverrideColor(bool const c_onOff, eHUD_COLOURS const c_hudColor = HUD_COLOUR_WHITE);
	void DisableTextChat(bool b)				{ m_bDisabledByScript = b; }
	void ForceAbortTextChat();
	bool ShouldDisableInputSinceChatIsTyping()  { return IsChatTyping() && CControlMgr::GetMainFrontendControl(false).WasKeyboardMouseLastKnownSource();}
	bool IsChatTyping()							{ return m_bIsTyping || m_iChatBackCount != 0; }
	bool IsMovieActive()						{ return m_Movie.IsActive(); }
	void IsTeamBasedJob(bool b)					{ m_bIsTeamJob = b; }

	void OverrideTeamLocalizationString(u32 uKeyHash = 0);

	void RT_RenderHelper();

	void UpdateDisplayConfig();

#if __BANK
	static void InitWidgets();
	static void CreateBankWidgets();
	static void DebugUpdateDisplayConfig();
#endif // __BANK

protected:
	void OnEvent(CNetworkTextChat* textChat, const rlGamerHandle& gamerHandle, const char* text, bool bTeamOnly);

private:
	enum eTypeMode
	{
		eTypeMode_None = 0,
		eTypeMode_Team,
		eTypeMode_All
	};

	enum eFocusMode
	{
		eFocusMode_Lobby = 0,
		eFocusMode_Game
	};

	bool LoadMovie();
	bool UnloadMovie();

	void UpdateInputWhenNotTyping();
	void UpdateInputWhenTyping();

	void UpdateAll();

	void SetFocus(int eVisibility, eTypeMode eType);
	bool ValidateText(const char* szMessage);

private:
	int StrLenChar16(const char16* str);
	bool IsAllowedToChat();

	int m_iHideChatWindowCount;
	int m_iChatBackCount;
	int m_iMessageSize;
	u32 m_uKeyHeldDelay;
	u32 m_uOverrideTeamHash;

	bool m_bIsTyping;
	bool m_bIsTeamJob;
	bool m_bDisabledByScript;
	bool m_bImeTextInputHasFocus;
	
	eFocusMode m_ePreviousFocusState;

	eTypeMode m_eTypeMode;
	CScaleformMovieWrapper m_Movie;
	char16 m_szMessage[MAX_CHAT_MESSAGE_SIZE+1];
	eHUD_COLOURS m_hudColor;  // store the hudColor to use to support 2653365
	bool m_bIsColorOverridden;

	CNetworkTextChat::Delegate m_TextChatDelegate;

	eFocusMode GetFocusMode();
};


typedef atSingleton<CMultiplayerChat> SMultiplayerChat;



#endif
#endif
