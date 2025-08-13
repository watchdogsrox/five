#if RSG_PC

// Rage header
#include "input/eventq.h"
#include "frontend/ui_channel.h"
#include "fwutil/ProfanityFilter.h"

// game header
#include "MultiplayerChat.h"
#include "PauseMenu.h"
#include "frontend/TextInputBox.h"
#include "Frontend/ui_channel.h"
#include "network/Voice/NetworkVoice.h"

#define KEY_HELD_THRESHOLD 100
#define CHATWIDGET_MOVIE_NAME "MULTIPLAYER_CHAT"

static int HIDE_CHAT_WINDOW_TIME  = 10000;
static int BACK_OUT_TIME = 100;


const char sChatScopeLocalization[3][32] = {
	"",
	"MP_CHAT_TEAM",
	"MP_CHAT_ALL"
};

enum {
	eVisibilityState_Hidden = 0,
	eVisibilityState_Default,
	eVisibilityState_Typing
};

CMultiplayerChat::CMultiplayerChat() : 
	m_bIsTyping(false),
	m_iChatBackCount(0),
	m_iHideChatWindowCount(0),
	m_bIsTeamJob(false),
	m_bDisabledByScript(false),
	m_bImeTextInputHasFocus(false),
	m_uKeyHeldDelay(0)
{
	m_ePreviousFocusState = eFocusMode_Game;
	memset(m_szMessage,0 ,sizeof(char16)*(MAX_CHAT_MESSAGE_SIZE+1));
	m_hudColor = HUD_COLOUR_WHITE;
	m_bIsColorOverridden = false;
	m_iMessageSize = 0;

	if (!m_TextChatDelegate.IsBound())
		m_TextChatDelegate.Bind(this, &CMultiplayerChat::OnEvent);
}

CMultiplayerChat::~CMultiplayerChat()
{
	if (m_TextChatDelegate.IsBound())
		m_TextChatDelegate.Unbind();
}

void CMultiplayerChat::Init(unsigned UNUSED_PARAM(initMode))
{
	if(!SMultiplayerChat::IsInstantiated())
	{
		SMultiplayerChat::Instantiate();
	}
}

void CMultiplayerChat::InitSession()
{
	if(SMultiplayerChat::IsInstantiated())
	{
		SMultiplayerChat::GetInstance().LoadMovie();
	}
}

void CMultiplayerChat::Shutdown(unsigned UNUSED_PARAM(initMode))
{
	SMultiplayerChat::GetInstance().UnloadMovie();
}

void CMultiplayerChat::ShutdownSession()
{
	if (SMultiplayerChat::IsInstantiated())
	{
		SMultiplayerChat::GetInstance().UnloadMovie();
	}
}

bool CMultiplayerChat::LoadMovie()
{
	if (m_Movie.IsActive())
		return true;

	m_Movie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, CHATWIDGET_MOVIE_NAME);
	CScaleformMgr::ForceMovieUpdateInstantly(m_Movie.GetMovieID(), true);
	CScaleformMgr::ChangeMovieParams(m_Movie.GetMovieID(), Vector2(0.0f, 0.0f), Vector2(1.0f,1.0f), GFxMovieView::SM_ExactFit);

	mpchatDebugf1("Loading movie: %s", CHATWIDGET_MOVIE_NAME);
	NetworkInterface::GetTextChat().AddDelegate(&m_TextChatDelegate);

	UpdateDisplayConfig();

	return m_Movie.IsActive();
}

bool CMultiplayerChat::UnloadMovie()
{
	if (!m_Movie.IsActive())
		return true;

	mpchatDebugf1("Removing movie: %s", CHATWIDGET_MOVIE_NAME);
	
	NetworkInterface::GetTextChat().RemoveDelegate(&m_TextChatDelegate);

	m_Movie.RemoveMovie();
	return m_Movie.IsActive();
}

void CMultiplayerChat::UpdateDisplayConfig()
{
	if(!m_Movie.IsActive())
		return;

	m_Movie.SetDisplayConfig(CScaleformMgr::SDC::UseUnAdjustedSafeZone);
}

//***********************************************************************
// Update 
//***********************************************************************
void CMultiplayerChat::Update()
{
	if (NetworkInterface::IsSessionActive() && SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsMovieActive())
	{
		SMultiplayerChat::GetInstance().UpdateAll();
		SMultiplayerChat::GetInstance().UpdateInput();
	}
}

void CMultiplayerChat::OverrideColor(bool const c_onOff, eHUD_COLOURS const c_hudColor)
{
	m_bIsColorOverridden = c_onOff;
	m_hudColor = c_hudColor;
}

void CMultiplayerChat::ForceAbortTextChat()
{
	if (m_Movie.BeginMethod("ABORT_TEXT"))
	{
		CScaleformMgr::EndMethod();
				
		m_iChatBackCount =  fwTimer::GetSystemTimeInMilliseconds();
		m_iHideChatWindowCount = fwTimer::GetSystemTimeInMilliseconds();

		memset(m_szMessage,0 ,sizeof(char16)*MAX_CHAT_MESSAGE_SIZE);

		m_hudColor = HUD_COLOUR_WHITE;
		m_bIsColorOverridden = false;

		SetFocus(eVisibilityState_Default, eTypeMode_None);
		m_iMessageSize = 0;
	}
}

void CMultiplayerChat::UpdateAll()
{
	if(GetFocusMode() != m_ePreviousFocusState)
	{
		m_ePreviousFocusState = GetFocusMode();
		if(m_Movie.BeginMethod("SET_FOCUS_MODE"))
		{
			m_Movie.AddParam(m_ePreviousFocusState);
			m_Movie.EndMethod();
		}
		
		SetFocus(eVisibilityState_Default, eTypeMode_None);
		m_iHideChatWindowCount = fwTimer::GetSystemTimeInMilliseconds();
	}

	if (m_iHideChatWindowCount > 0)
	{
		if (fwTimer::GetSystemTimeInMilliseconds() > m_iHideChatWindowCount + HIDE_CHAT_WINDOW_TIME)
		{
			SetFocus(eVisibilityState_Hidden, eTypeMode_None);
			m_iHideChatWindowCount = 0;
		}
	}

	if (m_iChatBackCount > 0)
	{
		if (fwTimer::GetSystemTimeInMilliseconds() > m_iChatBackCount + BACK_OUT_TIME)
		{
			if ((m_iChatBackCount != 0) && !m_bIsTyping)
			{
				m_iChatBackCount = 0;
			}	
		}
	}
}
//***********************************************************************
// Checks for input
//***********************************************************************
void CMultiplayerChat::UpdateInput()
{
	if (!IsAllowedToChat())
	{
		if (m_bIsTyping)
		{
			ForceAbortTextChat();
		}

		if (!m_bIsTyping && m_bDisabledByScript && m_iHideChatWindowCount != 0)
		{
			m_iChatBackCount =  0;
			m_iHideChatWindowCount = 0;
			
			SetFocus(eVisibilityState_Hidden, eTypeMode_None);
		}
		
		return;
	}
	
	if (!m_bIsTyping)
	{
		UpdateInputWhenNotTyping();
		m_iMessageSize = 0;
	}
	else
	{
		if(CNewHud::UpdateImeText())
			m_bImeTextInputHasFocus = true;

		UpdateInputWhenTyping();
	}
}


//***********************************************************************
// Checks for input when not typing in the chat window.
//***********************************************************************
bool CMultiplayerChat::IsAllowedToChat()
{
	bool bAllowedToChat = true;

	if (!m_Movie.IsActive())
	{
		bAllowedToChat = false;
	}
	else if (CNewHud::IsWeaponWheelVisible() || CPhoneMgr::IsDisplayed() || CNewHud::IsRadioWheelActive() || m_bDisabledByScript)
	{
		bAllowedToChat = false;
	}
	else if (STextInputBox::GetInstance().IsActive())
	{
		bAllowedToChat = false;
	}
	else if (!SUIContexts::IsActive(ATSTRINGHASH("IN_CORONA_MENU", 0x5849F1FA)) && CPauseMenu::IsActive())
	{
		bAllowedToChat = false;
	}
	else if(CPauseMenu::GetMenuPreference(PREF_SHOW_TEXT_CHAT) == FALSE)
	{
		bAllowedToChat = false;
	}
	
	return bAllowedToChat;
}

//***********************************************************************
// Checks for input when not typing in the chat window.
//***********************************************************************
void CMultiplayerChat::UpdateInputWhenNotTyping()
{
	if (KEYBOARD.KeyPressed(KEY_PAGEUP))
	{
		if (m_Movie.BeginMethod("PAGE_UP"))
			m_Movie.EndMethod();

	}
	else if (KEYBOARD.KeyPressed(KEY_PAGEDOWN))
	{
		if (m_Movie.BeginMethod("PAGE_DOWN"))
			m_Movie.EndMethod();
	}
	else if (CControlMgr::GetMainFrontendControl().GetMPTextChatTeam().IsReleased())
	{
		mpchatDebugf3("Team Chat pressed.");
		
		SetFocus(eVisibilityState_Typing, eTypeMode_Team);
		memset(m_szMessage,0 ,sizeof(char16)*MAX_CHAT_MESSAGE_SIZE);
		m_iHideChatWindowCount = 0;

	}
	else if (CControlMgr::GetMainFrontendControl().GetMPTextChatAll().IsReleased())
	{
		mpchatDebugf3("All Chat pressed.");

		SetFocus(eVisibilityState_Typing, eTypeMode_All);
		memset(m_szMessage,0 ,sizeof(char16)*MAX_CHAT_MESSAGE_SIZE);
		m_iHideChatWindowCount = 0;
	}
}


//***********************************************************************
// Check for input when typing in the chat window.
//***********************************************************************
void CMultiplayerChat::UpdateInputWhenTyping()
{
	ioValue::ReadOptions options;
	options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);

	const CControl& control = CControlMgr::GetMainFrontendControl();
	const ioValue& accept = control.GetFrontendAccept();
	const ioValue& exit   = control.GetFrontendEnterExitAlternate();
	const ioValue& cancel = control.GetFrontendCancel();

	if ( m_ePreviousFocusState == eFocusMode_Game)
	{
		if(CControlMgr::GetMainPlayerControl(false).WasKeyboardMouseLastKnownSource())
		{
			CControlMgr::GetMainPlayerControl(false).DisableAllInputs();
			CControlMgr::GetMainFrontendControl(false).DisableAllInputs();
		}
	}
	else if ( m_ePreviousFocusState == eFocusMode_Lobby )
	{
		if(CControlMgr::GetMainFrontendControl(false).WasKeyboardMouseLastKnownSource())
		{
			CControlMgr::GetMainPlayerControl(false).DisableAllInputs(300);
			CControlMgr::GetMainFrontendControl(false).DisableAllInputs(300);
		}
	}

	if(m_bImeTextInputHasFocus)
	{
		// Clear IME input focus when both buttons are up. This will be reset next frame if it still has input focus. Note, we require two frames up so
		// that IsReleased() does not cause the input window to close.
		m_bImeTextInputHasFocus = ioKeyboard::ImeIsInProgress() ||
			accept.IsDown(ioValue::BUTTON_DOWN_THRESHOLD, options) ||
			accept.WasDown(ioValue::BUTTON_DOWN_THRESHOLD, options) ||
			exit.IsDown(ioValue::BUTTON_DOWN_THRESHOLD, options) ||
			exit.WasDown(ioValue::BUTTON_DOWN_THRESHOLD, options);
	}
	else
	{
		if (control.GetFrontendAccept().IsReleased(ioValue::BUTTON_DOWN_THRESHOLD, options))
		{
			if(m_szMessage[0]=='\0')
			{
				memset(m_szMessage,0 ,sizeof(char16)*MAX_CHAT_MESSAGE_SIZE);
				SetFocus(eVisibilityState_Default, eTypeMode_None);
				m_iMessageSize = 0;
				m_iHideChatWindowCount = fwTimer::GetSystemTimeInMilliseconds();

				return;
			}
			else
			{
				if (m_Movie.BeginMethod("COMPLETE_TEXT"))
				{
					m_Movie.AddParamString("ENTER");
					CScaleformMgr::EndMethod();

					m_iHideChatWindowCount = fwTimer::GetSystemTimeInMilliseconds();
					USES_CONVERSION;
					char* utfMsg = WIDE_TO_UTF8(m_szMessage);

					mpchatDebugf1("Message before filter is applied to be sent: %s.", utfMsg);
					mpchatAssertf( m_szMessage[0] != '\0', "We do handle this case but lets be sure.  If this shows we're sending a message that's empty.  This is bad.");

					bool bTextSubmitted = NetworkInterface::GetTextChat().SubmitText(
						NetworkInterface::GetLocalPlayer()->GetGamerInfo(), 
						fwProfanityFilter::GetInstance().AsteriskProfanity(utfMsg),
						m_eTypeMode==eTypeMode_Team);

					if (bTextSubmitted)
					{
						mpchatAssertf( m_szMessage[0] != '\0', "Empty message sent.  This is not good.");
						mpchatDebugf1("Message added by local player with text: %s.", utfMsg);
					}
					else
					{
						mpchatDebugf1("Message WAS NOT added by local player with text: %s.", utfMsg);
					}

					memset(m_szMessage,0 ,sizeof(char16)*MAX_CHAT_MESSAGE_SIZE);
					SetFocus(eVisibilityState_Default, eTypeMode_None);
					m_iMessageSize = 0;
				}
			}
		}

		if (CControlMgr::GetKeyboard().GetKeyDown(KEY_BACK, KEYBOARD_MODE_GAME) && 
			fwTimer::GetSystemTimeInMilliseconds() - m_uKeyHeldDelay > KEY_HELD_THRESHOLD)
		{
			if (m_Movie.BeginMethod("DELETE_TEXT"))
			{
				CScaleformMgr::EndMethod();

				if(m_iMessageSize > 0)
				{
					m_iMessageSize--;
					m_szMessage[m_iMessageSize] ='\0';
				}
			}

			m_uKeyHeldDelay = fwTimer::GetSystemTimeInMilliseconds();
		}

		if( (cancel.GetLastSource().m_DeviceIndex != ioSource::IOMD_KEYBOARD_MOUSE && cancel.IsReleased(ioValue::BUTTON_DOWN_THRESHOLD, options)) ||
			(exit.GetLastSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && exit.IsReleased(ioValue::BUTTON_DOWN_THRESHOLD, options)))
		{
			if (m_Movie.BeginMethod("ABORT_TEXT"))
			{
				CScaleformMgr::EndMethod();

				m_iChatBackCount =  fwTimer::GetSystemTimeInMilliseconds();
				m_iHideChatWindowCount = fwTimer::GetSystemTimeInMilliseconds();

				memset(m_szMessage,0 ,sizeof(char16)*MAX_CHAT_MESSAGE_SIZE);
				SetFocus(eVisibilityState_Default, eTypeMode_None);
				m_iMessageSize = 0;
			}
		}
	}


	const char16 *pInputBuffer = ioKeyboard::GetInputText();

	// There is no PC text.
	if(pInputBuffer == NULL || pInputBuffer[0] == '\0')
	{
		return;
	}

	for(u32 i = 0; i < ioKeyboard::TEXT_BUFFER_LENGTH && pInputBuffer[i] != '\0'; ++i)
	{
		// add each character.
		if (pInputBuffer[i])
		{
			if (CTextFormat::DoAllCharactersExistInFont(FONT_STYLE_STANDARD, pInputBuffer,ioKeyboard::TEXT_BUFFER_LENGTH) && \
				(pInputBuffer[i] > 31) && m_iMessageSize < MAX_CHAT_MESSAGE_SIZE &&
				CTextFormat::IsValidDisplayCharacter(pInputBuffer[i]))
			{
				if (m_Movie.BeginMethod("ADD_TEXT") )
				{
					USES_CONVERSION;
					char16 tmpBuffer[2];
					tmpBuffer[0] = pInputBuffer[i];
					tmpBuffer[1] = '\0';

					if(CTextFormat::IsUnsupportedSpace(tmpBuffer[0]))
						tmpBuffer[0] = ' ';

					int iCursorLocation = StrLenChar16(m_szMessage);
					m_szMessage[iCursorLocation] = pInputBuffer[i];
					m_Movie.AddParamString(WIDE_TO_UTF8(tmpBuffer));
					m_Movie.EndMethod();

					m_iMessageSize++;
				}
			}
		}
	}
}

CMultiplayerChat::eFocusMode CMultiplayerChat::GetFocusMode()
{
	bool bInLobby;
	bInLobby = 	SUIContexts::IsActive(ATSTRINGHASH("IN_CORONA_MENU", 0x5849F1FA));

	if(bInLobby)
		return eFocusMode_Lobby;

	return eFocusMode_Game;
}

//***********************************************************************
// Check for input when typing in the chat window.
//***********************************************************************
void CMultiplayerChat::SetFocus(int iVisibility, eTypeMode eType)
{
	m_bIsTyping = iVisibility == eVisibilityState_Typing;
	m_eTypeMode = eType;

	if (m_bIsTyping)
	{
		ioKeyboard::SetKeyboardLayoutSwitchingEnable(true); 
	}
	else
	{
		ioKeyboard::SetKeyboardLayoutSwitchingEnable(false); 
	}

	if (m_Movie.BeginMethod("SET_FOCUS"))
	{
		m_Movie.AddParam(iVisibility);
		m_Movie.AddParam(eType);

		if (m_uOverrideTeamHash && eType == eTypeMode_Team)
			m_Movie.AddParamString(TheText.Get(m_uOverrideTeamHash, sChatScopeLocalization[eType]));
		else
			m_Movie.AddParamString(TheText.Get(sChatScopeLocalization[eType]));

		m_Movie.AddParamString(NetworkInterface::GetLocalGamerName());
		
		if (m_bIsColorOverridden)
		{
			m_Movie.AddParam(m_hudColor);
		}
		else
		{
			if (!NetworkInterface::IsActivitySession())
			{
				m_Movie.AddParam(HUD_COLOUR_WHITE);
			}
			else
			{
				m_Movie.AddParam(HUD_COLOUR_FRIENDLY);
			}
		}

		m_Movie.EndMethod();
	}
}

void CMultiplayerChat::OverrideTeamLocalizationString(u32 uKeyHash)
{
	m_uOverrideTeamHash = uKeyHash;
}

void CMultiplayerChat::OnEvent(CNetworkTextChat* UNUSED_PARAM(textChat), const rlGamerHandle& gamerHandle, const char* text, bool bTeamOnly)
{
	if (!text)
	{
		mpchatDebugf3("New message aborted.  No text.");
		return;
	}

	mpchatDebugf3("New message recieved.");
	
	const bool isInTransition = CNetwork::GetNetworkSession().IsTransitionEstablished() && !CNetwork::GetNetworkSession().IsTransitionLeaving();
	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
	int iColourID = 0;

	// If for some reason the player disconnects or is removed from the game before a message is received,
	// abort the message since we won't be able to get all the relevant information needed for a complete
	// message.
	if ((!pPlayer && !isInTransition) || !pLocalPlayer)
	{
		mpchatDebugf1("New message aborted. Unable to access the player or local player.");
		return;
	}

	int transitionPlayerIndex = -1;
	rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
	if (isInTransition)
	{
		unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for (unsigned i = 0; i < nGamers; i++)
		{
			if (aGamers[i].GetGamerHandle() == gamerHandle)
			{
				transitionPlayerIndex = i;
				break;
			}
		}

		if (transitionPlayerIndex < 0)
		{
			mpchatAssertf(transitionPlayerIndex >= 0, "CMultiplayerChat::OnEvent() - In a transition but unable to find transition remote player info.");
			return;
		}
	}

	const rage::rlGamerInfo& playerGamerInfo = isInTransition ? aGamers[transitionPlayerIndex] : pPlayer->GetGamerInfo();

	// Voicechat mute rules apply to voice chat too
	CNetworkVoice& voice = NetworkInterface::GetVoice();
	
	if (voice.IsGamerMutedByMe(playerGamerInfo.GetGamerHandle()))
	{
		mpchatDebugf1("Message from player: %s blocked due to mute status.", playerGamerInfo.GetDisplayName());
		return;
	}

	if (m_Movie.BeginMethod("ADD_MESSAGE"))
	{
		m_Movie.AddParamString(playerGamerInfo.GetName());
		m_Movie.AddParamString(text);

		if (bTeamOnly)
		{

			if (m_uOverrideTeamHash)
				m_Movie.AddParamString(TheText.Get(m_uOverrideTeamHash,sChatScopeLocalization[1]));
			else
				m_Movie.AddParamString(TheText.Get(sChatScopeLocalization[1]));
		}
		else
		{
			m_Movie.AddParamString(TheText.Get(sChatScopeLocalization[2]));
		}
		
		m_Movie.AddParam(bTeamOnly);

		if (!NetworkInterface::IsActivitySession())
		{
			iColourID = HUD_COLOUR_WHITE; 
		}
		else
		{
			// Mimic scripts behaviour here since colour is set via script based on a few minor rulesets.
			if (isInTransition)
			{
				if (pPlayer && pPlayer->IsLocal())
				{
					iColourID = HUD_COLOUR_FRIENDLY;
				}
				else
				{
					iColourID = HUD_COLOUR_NET_PLAYER1;
				}
			}
			else
			{
				if (pPlayer)
				{
					if (!m_bIsTeamJob)
					{
						if (pPlayer->IsLocal())
						{
							iColourID = HUD_COLOUR_FRIENDLY;
							mpchatDebugf1("Message from player: %s colour set to HUD_COLOUR_FRIENDLY. (no team job)", playerGamerInfo.GetDisplayName());

						}
						else 
						{
							iColourID = HUD_COLOUR_NET_PLAYER1;
							mpchatDebugf1("Message from player: %s colour set to HUD_COLOUR_NET_PLAYER1 (no team job).", playerGamerInfo.GetDisplayName());

						}
					}
					else
					{
						if (pPlayer->GetTeam() == pLocalPlayer->GetTeam())
						{
							mpchatDebugf1("Message from player: %s colour set to HUD_COLOUR_FRIENDLY(team job).", playerGamerInfo.GetDisplayName());
							iColourID = HUD_COLOUR_FRIENDLY;
						}
						else
						{
							switch (pPlayer->GetTeam())
							{
								case 1:
									iColourID = HUD_COLOUR_NET_PLAYER2;
									mpchatDebugf1("Message from player: %s colour set to HUD_COLOUR_NET_PLAYER2(team job).", playerGamerInfo.GetDisplayName());
									break;
								case 2:
									iColourID = HUD_COLOUR_NET_PLAYER3;
									mpchatDebugf1("Message from player: %s colour set to HUD_COLOUR_NET_PLAYER3(team job).", playerGamerInfo.GetDisplayName());

									break;
								default:
									iColourID = HUD_COLOUR_NET_PLAYER1;
									mpchatDebugf1("Message from player: %s colour set to HUD_COLOUR_NET_PLAYER1(team job).", playerGamerInfo.GetDisplayName());
									break;
							}
						}
					}
				}
			}
		}

		m_Movie.AddParam(iColourID); 
		m_Movie.EndMethod();

		if (!IsChatTyping())
		{
			SetFocus(eVisibilityState_Default, eTypeMode_None);
			m_iHideChatWindowCount = fwTimer::GetSystemTimeInMilliseconds();
		}

		mpchatDebugf1("Message from player: %s added with colourID:%d and text: %s.", playerGamerInfo.GetDisplayName(), iColourID, text);
	}
}

int CMultiplayerChat::StrLenChar16(const char16* str)
{
	int iCount = 0;
	while(str[iCount] != '\0')
	{
		iCount++;
	}

	return iCount;
}

void CMultiplayerChat::Render()
{
	if(CPauseMenu::GetMenuPreference(PREF_SHOW_TEXT_CHAT))
	{
		if(SMultiplayerChat::IsInstantiated() && SMultiplayerChat::GetInstance().IsMovieActive())
		{
			SMultiplayerChat::GetInstance().RT_RenderHelper();
		}
	}	
}

void CMultiplayerChat::RT_RenderHelper()
{
	m_Movie.Render();
}

#if __BANK
void CMultiplayerChat::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);
	if (!pBank)  // create the bank if not found
	{
		pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
	}

	if (pBank)
	{
		pBank->AddButton("Create Multiplayer Chat widgets", &CMultiplayerChat::CreateBankWidgets);
	}
}

void CMultiplayerChat::CreateBankWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);
	if(pBank)
	{
		pBank->PushGroup("Multiplayer Chat");
		{
			pBank->AddButton("Update Display Config", &CMultiplayerChat::DebugUpdateDisplayConfig);
		}
		pBank->PopGroup();
	}
}

void CMultiplayerChat::DebugUpdateDisplayConfig()
{
	SMultiplayerChat::GetInstance().UpdateDisplayConfig();
}
#endif // __BANK


#endif
