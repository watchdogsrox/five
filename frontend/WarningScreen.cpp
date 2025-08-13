/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WarningScreen.cpp
// PURPOSE : manages warning messages in the game
// AUTHOR  : Derek Payne
// STARTED : 10/07/2012
//
/////////////////////////////////////////////////////////////////////////////////

#include "frontend/WarningScreen.h"
#include "frontend/loadingscreens.h"
#include "frontend/scaleform/ScaleFormMgr.h"
#include "renderer/RenderThread.h"
#include "frontend/ui_channel.h"
#include "Frontend/HudTools.h"
#include "Frontend/NewHud.h"
#include "Frontend/PauseMenu.h"
#include "network/Live/livemanager.h"
#include "saveload/savegame_messages.h"
#include "script/thread.h"
#include "System/control.h"
#include "System/controlMgr.h"
#include "system/service.h"
#include "system/stack.h"
#include "text/GxtString.h"
#include "text/messages.h"
#include "fwsys/gameskeleton.h"
#include "audio/frontendaudioentity.h"
#include "frontend/BusySpinner.h"

#include "control/replay/replay.h"
#if GTA_REPLAY
#include "control/replay/ReplayRecording.h"
#endif	//GTA_REPLAY

#define WARNINGSCREEN_MOVIE_FILENAME	"POPUP_WARNING"
#define RETURN_FROM_SYSTEM_FRAME_DELAY (3)

PARAM(uiSuppressWarningCallstack, "[UI] Suppress the callstack printed when requesting a warning screen");

FRONTEND_OPTIMISATIONS();

CScaleformMovieWrapper CWarningScreen::ms_Movie;
u8	 CWarningScreen::ms_returnFromSystemFrameDelay = 0;
bool CWarningScreen::ms_bMovieStreaming = false;
bool CWarningScreen::ms_bActiveThisFrame = false;
bool CWarningScreen::ms_bWasActiveLastFrame = false;
bool CWarningScreen::ms_bInactiveThisFrame = false;
bool CWarningScreen::ms_bSetThisFrame = false;
bool CWarningScreen::ms_bSetOptions = false;
bool CWarningScreen::ms_bAllowSpinner = false;
bool CWarningScreen::ms_lastInputKeyboard = false;
bank_u32 CWarningScreen::ms_iPlayerInputDisableDuration = 500U; // disable all inputs for half a second when the warning screen exits!
bank_u32 CWarningScreen::ms_iFrontendInputDisableDuration = 50U;
char CWarningScreen::ms_cMessageString[MAX_CHARS_IN_MESSAGE];
char CWarningScreen::ms_cErrorMessage[MAX_CHARS_IN_MESSAGE];
u32 CWarningScreen::ms_uLastBodyPassed = 0;
atHashWithStringBank CWarningScreen::ms_cHeaderTextId;
atHashWithStringBank CWarningScreen::ms_cSubtextTextId;
eWarningMessageStyle CWarningScreen::ms_Style = WARNING_MESSAGE_STANDARD;
eWarningButtonFlags CWarningScreen::ms_iFlags = FE_WARNING_NONE;

atHashWithStringBank CWarningScreen::ms_cLastMessageStringHash;
atHashWithStringBank CWarningScreen::ms_cLastHeaderTextId;
atHashWithStringBank CWarningScreen::ms_cLastSubtextTextId;

bool CWarningScreen::ms_bBlackBackground = true;
bool CWarningScreen::ms_bUnpause = true;

atArray<CMenuButtonWithSound> CWarningScreen::ms_ButtonData;
atRangeArray<u64,4>	 CWarningScreen::ms_AffectedButtonMasks;

bool CWarningScreen::ms_bIsControllerDisconnectWarningScreenSet = false;

#if HAS_YT_DISCONNECT_MESSAGE
bool CWarningScreen::ms_bNeedToDisplayYTConnectionError = false;
bool CWarningScreen::ms_bNeedToRefreshYTConnectionError = false;
bool CWarningScreen::ms_bIsDisplayingYTConnectionError = false;
bool CWarningScreen::ms_bBlockYTConnectionError = false;
#endif
//////////////////////////////////////////////////////////////////////////

void CWarningScreen::Init(unsigned initMode)
{
	if( initMode == INIT_CORE )
	{
		REGISTER_FRONTEND_XML(CWarningScreen::HandleXML, "WarningScreen");

		// preload some entries so we can live happily due to startup procedures needing this nearly instantly 
		if( ms_ButtonData.empty() ) 
		{ 
			ms_ButtonData.ResizeGrow(2); 

			int iBitIndex = 1; 
			CMenuButtonWithSound& OKButton = ms_ButtonData[iBitIndex];   
			OKButton.m_ButtonInput = INPUT_FRONTEND_ACCEPT;
			OKButton.m_ButtonInputGroup = INPUTGROUP_INVALID;
			OKButton.m_RawButtonIcon = ICON_INVALID;
			OKButton.m_hButtonHash.SetFromString("IB_OK"); 

			int iButton = GetButtonMaskIndex(OKButton.m_ButtonInput); 
			if( iButton != -1 ) 
			{ 
				ms_AffectedButtonMasks[iButton] |= BIT_64(iBitIndex); 
			} 
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CWarningScreen::HandleXML( parTreeNode* pButtonMenu )
{
	parTreeNode::ChildNodeIterator pChildrenStart = pButtonMenu->BeginChildren();
	parTreeNode::ChildNodeIterator pChildrenEnd = pButtonMenu->EndChildren();

	// first, we have to find the highest bitindex in the list (we can't use NumChildren because bits may go dead)
	int iHighestBitIndex = -1;
	for(parTreeNode::ChildNodeIterator ci = pChildrenStart; ci != pChildrenEnd; ++ci)
	{
		parElement& rCurElement = (*ci)->GetElement();
		iHighestBitIndex = Max(rCurElement.FindAttributeIntValue("BitIndex", -1, true), iHighestBitIndex);
	}

	ms_ButtonData.Reset();
	ms_ButtonData.ResizeGrow(iHighestBitIndex+1); // +1 to adjust for Zero

	for(parTreeNode::ChildNodeIterator ci = pChildrenStart; ci != pChildrenEnd; ++ci)
	{
		parElement& rCurElement = (*ci)->GetElement();
		// check for bitIndex
		u32 iBitIndex = rCurElement.FindAttributeIntValue("BitIndex", -1, true);
		if( iBitIndex == -1 || !uiVerifyf(iBitIndex >= 0 && iBitIndex < 64, "Can only handle 64-bit values for bitindex! '%u' is just out of range!", iBitIndex) )
			continue;

		CMenuButtonWithSound& curSound = ms_ButtonData[iBitIndex];

		PARSER.LoadObject(*ci, curSound);

		uiAssertf(curSound.m_ButtonInput != UNDEFINED_INPUT || curSound.m_ButtonInputGroup != INPUTGROUP_INVALID || curSound.m_RawButtonIcon != ICON_INVALID,
			"Invalid button icon!");

		// set up sound
		char temp[32];
		curSound.m_Sound.SetFromString( rCurElement.FindAttributeStringValue("Sound", "SELECT", temp, 32) );

		int iButton = GetButtonMaskIndex(curSound.m_ButtonInput);
		if( iButton != -1 )
		{
			ms_AffectedButtonMasks[iButton] |= BIT_64(iBitIndex);
		}
	}
}

eFRONTEND_INPUT GetFrontendInputFromInstruction(InputType eButton)
{
	switch(eButton) {
	case INPUT_FRONTEND_ACCEPT:
		return FRONTEND_INPUT_ACCEPT;

	case INPUT_FRONTEND_CANCEL:
		return FRONTEND_INPUT_BACK;

	case INPUT_FRONTEND_X:
		return FRONTEND_INPUT_X;

	case INPUT_FRONTEND_Y:
		return FRONTEND_INPUT_Y;

	default:
		//uiAssertf(0, "Unhandled eButton! I don't know what index '%s' belongs in!", parser_eInstructionButtons_Strings[eButton] );
		return FRONTEND_INPUT_MAX;
	}
}

int CWarningScreen::GetButtonMaskIndex(InputType eButton)
{
	switch(eButton) {
	case INPUT_FRONTEND_ACCEPT:		return 0;
	case INPUT_FRONTEND_CANCEL:		return 1;
	case INPUT_FRONTEND_X:			return 2;
	case INPUT_FRONTEND_Y:			return 3;

	default:
		//uiAssertf(0, "Unhandled eButton! I don't know what index '%s' belongs in!", parser_eInstructionButtons_Strings[eButton] );
		return -1;
	}
}

bool CWarningScreen::IsFlagForButtonActive(InputType eButton, u64 flagsToTest)
{
	int iCheck = GetButtonMaskIndex(eButton);
	if( Unlikely(iCheck != -1) )
		return (ms_AffectedButtonMasks[iCheck] & flagsToTest) != 0;

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CWarningScreen::SetMessage(eWarningMessageStyle style,
								const atHashWithStringBank pTextLabelBody,
								const eWarningButtonFlags iFlags, 
								const bool bInsertNumber,
								const s32 NumberToInsert, 
								const char *pFirstSubString, 
								const char *pSecondSubString,
								const bool bBlackBackground,
								const bool bAllowSpinner,
								int errorNumber)
{
	SetMessageInternal(style,
					   0u,
					   pTextLabelBody,
					   0u,
					   iFlags, 
					   bInsertNumber,
					   NumberToInsert, 
					   pFirstSubString, 
					   pSecondSubString,
					   bBlackBackground,
					   bAllowSpinner,
					   errorNumber);
}

void CWarningScreen::SetMessageWithHeader( const eWarningMessageStyle style,
										   const atHashWithStringBank pTextLabelHeading,
										   const atHashWithStringBank pTextLabelBody, 
										   const eWarningButtonFlags iFlags, 
										   const bool bInsertNumber,
										   const s32 NumberToInsert, 
										   const char *pFirstSubString, 
										   const char *pSecondSubString,
										   const bool bBlackBackground,
										   const bool bAllowSpinner,
										   int errorNumber)
{
	SetMessageInternal( style,
						pTextLabelHeading,
						pTextLabelBody,
						0u,
						iFlags, 
						bInsertNumber,
						NumberToInsert, 
						pFirstSubString,
						pSecondSubString, 
						bBlackBackground,
						bAllowSpinner,
						errorNumber);
}


void CWarningScreen::SetMessageAndSubtext(  const eWarningMessageStyle style,
										    const atHashWithStringBank pTextLabelBody,
											const atHashWithStringBank pTextLabelSubtext,
											const eWarningButtonFlags iFlags, 
											const bool bInsertNumber,
											const s32 NumberToInsert, 
											const char *pFirstSubString, 
											const char *pSecondSubString,
											const bool bBlackBackground,
											const bool bAllowSpinner,
											int errorNumber)
{
	SetMessageInternal( style,
						0u,
						pTextLabelBody,
						pTextLabelSubtext,
						iFlags, 
						bInsertNumber,
						NumberToInsert, 
						pFirstSubString, 
						pSecondSubString,
						bBlackBackground,
						bAllowSpinner,
						errorNumber);

}

void CWarningScreen::SetMessageWithHeaderAndSubtext(const eWarningMessageStyle style,
													const atHashWithStringBank pTextLabelHeading,
												  const atHashWithStringBank pTextLabelBody, 
												  const atHashWithStringBank pTextLabelSubtext, 
												  const eWarningButtonFlags iFlags, 
												  const bool bInsertNumber,
												  const s32 NumberToInsert, 
												  const char *pFirstSubString, 
												  const char *pSecondSubString,
												  const bool bBlackBackground,
												  const bool bAllowSpinner,
												  int errorNumber)
{
	SetMessageInternal( style,
						pTextLabelHeading,
						pTextLabelBody,
						pTextLabelSubtext,
						iFlags, 
						bInsertNumber,
						NumberToInsert, 
						pFirstSubString, 
						pSecondSubString,
						bBlackBackground,
						bAllowSpinner,
						errorNumber);
}

void CWarningScreen::SetControllerReconnectMessageWithHeader(	const eWarningMessageStyle style,
																const atHashWithStringBank pTextLabelHeading,
																const atHashWithStringBank pTextLabelBody, 
																const eWarningButtonFlags iFlags, 
																const bool bInsertNumber,
																const s32 NumberToInsert, 
																const char *pFirstSubString, 
																const char *pSecondSubString,
																const bool bBlackBackground,
																const bool bAllowSpinner,
																int errorNumber)
{
	SetMessageInternal( style,
						pTextLabelHeading,
						pTextLabelBody,
						0u,
						iFlags, 
						bInsertNumber,
						NumberToInsert, 
						pFirstSubString, 
						pSecondSubString,
						bBlackBackground,
						bAllowSpinner,
						errorNumber);

	ms_bIsControllerDisconnectWarningScreenSet = true;
}

#if HAS_YT_DISCONNECT_MESSAGE
void CWarningScreen::SetYTConnectionLossMessage(bool display)
{
	if (ms_bNeedToDisplayYTConnectionError)
	{
		ms_bSetThisFrame = false;
		ms_bActiveThisFrame = true;
	}
	ms_bNeedToDisplayYTConnectionError = display;
}

void CWarningScreen::BlockYTConnectionLossMessage(bool blockMsg)
{
	ms_bBlockYTConnectionError = blockMsg;
}

bool CWarningScreen::UpdateYTConnectionLossMessage()
{
	if (ms_bNeedToDisplayYTConnectionError && !ms_bBlockYTConnectionError)
	{
		if (ms_Movie.IsFree())
		{
			SetMessageInternal(WARNING_MESSAGE_STANDARD,
				"SG_HDNG",
				CLiveManager::IsAccountSignedOutOfOnlineService() ? "YT_RESIGN" : "YT_DISCON",
				0u,
				FE_WARNING_OK,
				false,
				-1,
				NULL,
				NULL,
				true,
				false,
				0);
		}
		else if (ms_Movie.IsActive())
		{
			ms_bNeedToDisplayYTConnectionError = !CheckInput(INPUT_FRONTEND_ACCEPT, true);
			if (ms_bNeedToDisplayYTConnectionError)
			{
				CNewHud::UpdateDisplayConfig(ms_Movie.GetMovieID(), SF_BASE_CLASS_GENERIC);  // send the screen info to the movie

				if (ms_Movie.BeginMethod("SHOW_POPUP_WARNING"))
				{
					ms_Movie.AddParam(0);

					atHashWithStringBank headerTextId;
					headerTextId.SetFromString("SG_HDNG");
					ms_Movie.AddParamLocString(headerTextId.GetHash());

					ms_Movie.AddParamString(TheText.Get(CLiveManager::IsAccountSignedOutOfOnlineService() ? "YT_RESIGN" : "YT_DISCON"), false);

					ms_Movie.AddParamString("\0");

					ms_Movie.AddParam(true); // black bg
					ms_Movie.AddParam(WARNING_MESSAGE_STANDARD);

					ms_Movie.AddParamString("\0");

					ms_Movie.EndMethod();
				}

				SetInstructionalButtons(FE_WARNING_OK);

				ms_bNeedToRefreshYTConnectionError = false;
				ms_bSetThisFrame = true;

				ms_bIsDisplayingYTConnectionError = true;
				return true;
			}
		}

		// if there is a message behind the error, we need to force set the frame to false to set it again
		ms_bSetThisFrame = false;
		ms_bActiveThisFrame = true;

		for(int i = 0; i < ms_ButtonData.size(); ++i)
		{
			ms_ButtonData[i].m_DebounceCheck = true;
		}
	}

	ms_bIsDisplayingYTConnectionError = false;

	return false;
}
#endif

void CWarningScreen::CreateMovieIfNecessary()
{
	if(ms_Movie.IsFree())
	{
		const char *pFilename = WARNINGSCREEN_MOVIE_FILENAME;

// 		Vector2 vMoviePos(0.0f, 0.0f);
// 		Vector2 vMovieSize(1.0f, 1.0f);
// 
// 		CHudTools::AdjustForWidescreen(WIDESCREEN_FORMAT_TO_4_3_FROM_16_9, &vMoviePos, &vMovieSize, NULL);

		if (!CLoadingScreens::AreActive())
		{
			ms_Movie.CreateMovie(SF_BASE_CLASS_GENERIC, pFilename, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), false);  // fix for 1549486 - warning messages shouldnt get removed automatically
			ms_bMovieStreaming = true;
		}
		else
		{
			ms_Movie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, pFilename, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), false);  // fix for 1549486 - warning messages shouldnt get removed automatically
			CScaleformMgr::ForceMovieUpdateInstantly(ms_Movie.GetMovieID(), true);
			ms_bMovieStreaming = false;
		}
	}
}



void CWarningScreen::SetWarningMessageInUse()
{
	CreateMovieIfNecessary();

	ms_bActiveThisFrame = true;
	ms_bInactiveThisFrame = false;
	ms_bSetThisFrame = false;
	ms_bSetOptions = false;
	ms_bAllowSpinner = false;
}


bool CWarningScreen::SetMessageOptionItems( const int iHighlightIndex,
											const char *pNameString,
											const int iCash,
											const int iRp,
											const int iLvl,
											const int iCol)
{
	if (uiVerifyf(ms_bActiveThisFrame, "CWarningScreen::SetMessageOptionItems - A warning message is not active to set OptionItems on"))
	{
		if (ms_bActiveThisFrame && ms_Movie.IsActive())
		{
			if (!ms_bSetOptions)
			{
				if (ms_Movie.BeginMethod("SET_LIST_ROW"))
				{
					ms_Movie.AddParam(iHighlightIndex);
					ms_Movie.AddParamString(pNameString, false);
					ms_Movie.AddParam(iCash);
					ms_Movie.AddParam(iRp);
					ms_Movie.AddParam(iLvl);
					ms_Movie.AddParam(iCol);
					ms_Movie.EndMethod();

					return true;
				}
			}
		}
	}

	return false;
}

void CWarningScreen::RemoveMessageOptionItems()
{
	if (ms_Movie.IsActive())
	{
		ms_Movie.CallMethod("REMOVE_LIST_ITEMS");
	}
}

bool CWarningScreen::SetMessageOptionHighlight( const int iHighlightIndex )
{
	ms_bSetOptions = true;  // flag as in use

	if (uiVerifyf(ms_bActiveThisFrame, "CWarningScreen::SetMessageOptionHighlight - A warning message is not active to set any highlight on"))
	{
		if (ms_bActiveThisFrame && ms_Movie.IsActive() )
		{
			if (ms_Movie.BeginMethod("SET_LIST_HIGHLIGHT"))
			{
				ms_Movie.AddParam(iHighlightIndex);

				ms_Movie.EndMethod();

				return true;
			}
		}
	}

	return false;
}

void CWarningScreen::SetMessageInternal(const eWarningMessageStyle style,
										const atHashWithStringBank pTextLabelHeading,
										const atHashWithStringBank pTextLabelBody,
										const atHashWithStringBank pTextLabelSubtext,
										const eWarningButtonFlags iFlags,
										const bool bInsertNumber,
										const s32 NumberToInsert, 
										const char *pFirstSubString, 
										const char *pSecondSubString,
										const bool bBlackBackground,
										const bool bAllowSpinner,
										int errorNumber)
{

#if GTA_REPLAY
	// Prevent any replay recording when a warning screen is up
	ReplayRecordingScriptInterface::PreventRecordingThisFrame();
#endif	//GTA_REPLAY

	if (ms_bIsControllerDisconnectWarningScreenSet)
	{
		return;
	}
	CreateMovieIfNecessary();

	char previousMessageString[MAX_CHARS_IN_MESSAGE];
	atHashWithStringBank cPreviousHeaderTextId;
	atHashWithStringBank cPreviousSubtextTextId;
	eWarningButtonFlags iPreviousFlags;
	bool bPreviousBackground;

	if (ms_bSetThisFrame)  // if we already have a message set this frame, store a copy of it so we can check against it later on when we have built the new string
	{
		safecpy(previousMessageString, ms_cMessageString);
		cPreviousHeaderTextId = ms_cHeaderTextId;
		cPreviousSubtextTextId = ms_cSubtextTextId;
		iPreviousFlags = ms_iFlags;
		bPreviousBackground = ms_bBlackBackground;
	}
	else
	{
		previousMessageString[0] = 0;
		iPreviousFlags = FE_WARNING_NONE;
		bPreviousBackground = true;
	}

	ms_cHeaderTextId = pTextLabelHeading;
	ms_cSubtextTextId = pTextLabelSubtext;
	ms_cMessageString[0] = '\0';

	if(errorNumber != 0)
	{
		CNumberWithinMessage ErrorNumberArray[1];
		ErrorNumberArray[0].Set(errorNumber);
		
		CSubStringWithinMessage DummySubStringArray[1];

		CMessages::InsertNumbersAndSubStringsIntoString(
			TheText.Get("ERROR_CODE"),
			ErrorNumberArray, 1, 
			DummySubStringArray, 0, 
			ms_cErrorMessage, MAX_CHARS_IN_MESSAGE);
	}
	else
	{
		memset(ms_cErrorMessage, 0, MAX_CHARS_IN_MESSAGE);
	}
	
	if (pTextLabelBody.IsNotNull())
	{
		char *pString = NULL;
		
		// script try to pass in a space as the body text, this is there to make that work but it really shouldn't be needed
		if(pTextLabelBody != ATSTRINGHASH(" ",0x49dd93b2))
		{
#if __BANK && !__NO_OUTPUT
			pString = pTextLabelBody.TryGetCStr() ? TheText.Get(pTextLabelBody.TryGetCStr()) : TheText.Get(pTextLabelBody.GetHash(),"");
#else
			pString = TheText.Get(pTextLabelBody.GetHash(),"");
#endif
			Assertf(pString, "CWarningScreen::SetMessage - Can't find Key in the text file");
		}

		CNumberWithinMessage NumberArray[1];
		u32 NumberOfNumbersToInsert = 0;
		if (bInsertNumber)
		{
			NumberArray[NumberOfNumbersToInsert++].Set(NumberToInsert);
		}
		CSubStringWithinMessage SubStringArray[2];
		u32 NumberOfSubStringsToInsert = 0;
		if (pFirstSubString && pFirstSubString[0] != '\0')
		{
			SubStringArray[NumberOfSubStringsToInsert++].SetCharPointer(pFirstSubString);
		}

		if (pSecondSubString && pSecondSubString[0] != '\0')
		{
			SubStringArray[NumberOfSubStringsToInsert++].SetCharPointer(pSecondSubString);
		}

		CMessages::InsertNumbersAndSubStringsIntoString(pString, 
														NumberArray, NumberOfNumbersToInsert, 
														SubStringArray, NumberOfSubStringsToInsert, 
														ms_cMessageString, MAX_CHARS_IN_MESSAGE);

#if RSG_OUTPUT
		if (pTextLabelBody.GetHash() != ms_uLastBodyPassed)
		{
			uiDebugf1("WarningScreen::SetMessageInternal - Showing Popup"); 
			if (!PARAM_uiSuppressWarningCallstack.Get())
			{
				// make it easier to tell where a popup is being requested from
				scrThread::PrePrintStackTrace();
				sysStack::PrintStackTrace();
			}
			uiDebugf1("WarningScreen::SetMessageInternal - Popup Data...");			
			uiDebugf1("\tHeader: " HASHFMT " (%s)", HASHOUT(ms_cHeaderTextId), ms_cHeaderTextId.TryGetCStr() ? TheText.Get(ms_cHeaderTextId.TryGetCStr()) : "<no text label string>");
			uiDebugf1("\tMessage: " HASHFMT " (%s)", HASHOUT(pTextLabelBody), ms_cMessageString);
			uiDebugf1("\tSubtext: " HASHFMT " (%s)", HASHOUT(ms_cSubtextTextId), ms_cSubtextTextId.TryGetCStr() ? TheText.Get(ms_cSubtextTextId.TryGetCStr()) : "<no text label string>");
		}
#endif

		// capture the hasH value
		ms_uLastBodyPassed = pTextLabelBody.GetHash();
	}
	else if(!pTextLabelBody.IsNotNull() && style != WARNING_MESSAGE_REPORT_SCREEN)
	{
		Assertf(0, "CWarningScreen::SetMessage - Text label for body text is NULL. Is this allowed?");
	}

	ms_bBlackBackground = bBlackBackground;
	ms_Style = style;
	ms_iFlags = iFlags;
	ms_bActiveThisFrame = true;
	ms_bInactiveThisFrame = false;

	if (ms_bAllowSpinner != bAllowSpinner)
	{
		if (bAllowSpinner)
		{
			CBusySpinner::RegisterInstructionalButtonMovie(ms_Movie.GetMovieID());  // register this "instructional button" movie with the spinner system
		}
		else
		{
			CBusySpinner::UnregisterInstructionalButtonMovie(ms_Movie.GetMovieID()); // remove if it's a message on top of a message, and doesn't require this
		}
	}

	ms_bAllowSpinner = bAllowSpinner;

	// if the message is already set up this frame, then check if its a different message.   If it is, set it to re-populate the scaleform movie with the new message
	if (ms_bSetThisFrame)
	{
		if ( ( strcmp(ms_cMessageString, previousMessageString) != 0) ||
			 ms_cHeaderTextId != cPreviousHeaderTextId ||
			 ms_cSubtextTextId != cPreviousSubtextTextId ||
			 (ms_iFlags != iPreviousFlags) ||
			 (ms_bBlackBackground != bPreviousBackground))
		{
			if (ms_bSetOptions)
			{
				RemoveMessageOptionItems();  // if it is a different message then clear any existing messageoption items
			}

			ms_bSetThisFrame = false;
			ms_bSetOptions = false;
		}
	}
}

bool CWarningScreen::CheckInput(int iIndex, bool bHandleSound, int iExtraInputOverride /* = 0 */ )
{
	eFRONTEND_INPUT curInput = GetFrontendInputFromInstruction(ms_ButtonData[iIndex].m_ButtonInput);
	if( curInput != FRONTEND_INPUT_MAX )
	{
		if( CPauseMenu::CheckInput(curInput, false, CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE | CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | iExtraInputOverride, false, false, true) )
		{
			if(ms_ButtonData[iIndex].m_DebounceCheck == false)
			{
				if( bHandleSound )
					g_FrontendAudioEntity.PlaySound(ms_ButtonData[iIndex].m_Sound, "HUD_FRONTEND_DEFAULT_SOUNDSET");
				return true;
			}
		}
		else
		{
			// Always check for disabled input.  Reasoning -- if a warning screen is up, then nothing else in the game should have focus
			ioValue::ReadOptions options = ioValue::NO_DEAD_ZONE;
			options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);

			if( CControlMgr::GetMainFrontendControl().GetValue(ms_ButtonData[iIndex].m_ButtonInput).IsUp(ioValue::BUTTON_DOWN_THRESHOLD, options) )
			{
				ms_ButtonData[iIndex].m_DebounceCheck = false;
			}
		}
	}
	return false;
}

bool CWarningScreen::CheckInput(InputType input, bool bHandleSound, int iExtraInputOverride /* = 0 */ )
{
	if(input != UNDEFINED_INPUT)
	{
		for(int index = 0; index < ms_ButtonData.size(); ++index)
		{
			if(ms_ButtonData[index].m_ButtonInput == input)
			{
				return CheckInput(index, bHandleSound, iExtraInputOverride);
			}
		}
	}

	Assertf(false, "Cannot find CButtonBase for '%s'!", CControl::GetInputName(input));
	return false;
}

u32 CWarningScreen::GetActiveMessage()
{
	u32 uReturnHash = 0;

	if(IsActive())
	{
		uReturnHash = ms_uLastBodyPassed;
	}

	return uReturnHash;
}

#define WARNING_FLAG_SWITCH_CASE( partialName ) case FE_##partialName:             \
                                                    {                              \
                                                        result = UI_##partialName; \
                                                        break;                     \
                                                    }

eParsableWarningButtonFlags CWarningScreen::GetParsableFlagsFromFlag( eWarningButtonFlags const flag )
{
    eParsableWarningButtonFlags result = UI_WARNING_NONE;

    switch( flag )
    {
        WARNING_FLAG_SWITCH_CASE( WARNING_SELECT );
        WARNING_FLAG_SWITCH_CASE( WARNING_OK );
        WARNING_FLAG_SWITCH_CASE( WARNING_YES );
        WARNING_FLAG_SWITCH_CASE( WARNING_BACK );
        WARNING_FLAG_SWITCH_CASE( WARNING_CANCEL );
        WARNING_FLAG_SWITCH_CASE( WARNING_NO );
        WARNING_FLAG_SWITCH_CASE( WARNING_RETRY );
        WARNING_FLAG_SWITCH_CASE( WARNING_RESTART );
        WARNING_FLAG_SWITCH_CASE( WARNING_SKIP );
        WARNING_FLAG_SWITCH_CASE( WARNING_QUIT );
        WARNING_FLAG_SWITCH_CASE( WARNING_ADJUST_LEFTRIGHT );
        WARNING_FLAG_SWITCH_CASE( WARNING_IGNORE );
        WARNING_FLAG_SWITCH_CASE( WARNING_SHARE );
        WARNING_FLAG_SWITCH_CASE( WARNING_SIGNIN );
        WARNING_FLAG_SWITCH_CASE( WARNING_CONTINUE );
        WARNING_FLAG_SWITCH_CASE( WARNING_ADJUST_STICKLEFTRIGHT );
        WARNING_FLAG_SWITCH_CASE( WARNING_ADJUST_UPDOWN );
        WARNING_FLAG_SWITCH_CASE( WARNING_OVERWRITE );
        WARNING_FLAG_SWITCH_CASE( WARNING_SOCIAL_CLUB );
        WARNING_FLAG_SWITCH_CASE( WARNING_CONFIRM );
        WARNING_FLAG_SWITCH_CASE( WARNING_CONTNOSAVE );
        WARNING_FLAG_SWITCH_CASE( WARNING_RETRY_ON_A );
        WARNING_FLAG_SWITCH_CASE( WARNING_BACK_WITH_ACCEPT_SOUND );
        WARNING_FLAG_SWITCH_CASE( WARNING_RETURNSP );
        WARNING_FLAG_SWITCH_CASE( WARNING_CANCEL_ON_B );
        WARNING_FLAG_SWITCH_CASE( WARNING_EXIT );
        default:
            break;
    }

    return result;
}

#undef WARNING_FLAG_SWITCH_CASE

eWarningButtonFlags CWarningScreen::CheckAllInput(bool bHandleSound /* = true */, int iExtraInputOverride /* = 0 */)
{
	// just bail if it's a completely empty flag set
	if( ms_iFlags == FE_WARNING_NONE )
		return FE_WARNING_NONE;

#if HAS_YT_DISCONNECT_MESSAGE
	if (ms_bIsDisplayingYTConnectionError)
		return FE_WARNING_NONE;
#endif

	// process the inputs in A,B,X,Y order (for priority)
	// for each of our buttonflags
	for(int curButton=0; curButton < ms_AffectedButtonMasks.GetMaxCount(); ++curButton)
	{
		// for each of the buttons we're currently displaying
		u64 curFlags = ms_AffectedButtonMasks[curButton] & ms_iFlags;
		while( curFlags != 0 )
		{
			// for each input set
			int iIndex = Log2Floor(curFlags);
			// if current bit is set
			if( CheckInput( iIndex, bHandleSound, iExtraInputOverride) )
				return static_cast<eWarningButtonFlags>(BIT_64(iIndex));

			curFlags = BIT_CLEAR(iIndex, curFlags);
		}
	}

	return FE_WARNING_NONE;
}


void CWarningScreen::Update()
{
	if (ms_bMovieStreaming && !ms_Movie.IsFree())
		ms_bMovieStreaming = !ms_Movie.IsActive();

	// Reset the button denounce flags if it is a new message.
	atHashWithStringBank messageHash(ms_cMessageString);
	if( messageHash != ms_cLastMessageStringHash ||
		ms_cHeaderTextId != ms_cLastHeaderTextId ||
		ms_cLastSubtextTextId != ms_cSubtextTextId )
	{
		for(int i = 0; i < ms_ButtonData.size(); ++i)
		{
			ms_ButtonData[i].m_DebounceCheck = true;
		}
		ms_cLastMessageStringHash = messageHash;
		ms_cLastHeaderTextId = ms_cHeaderTextId;
		ms_cLastSubtextTextId = ms_cSubtextTextId;
	}

#if HAS_YT_DISCONNECT_MESSAGE
	// if returns true, then it's locked in it's own update, so return out early
	if (UpdateYTConnectionLossMessage())
		return;
#endif

	if(CPad::IsIntercepted())
	{
		ms_returnFromSystemFrameDelay = RETURN_FROM_SYSTEM_FRAME_DELAY;
		ms_bActiveThisFrame |= ms_bWasActiveLastFrame;
	}

	if( ms_Movie.IsActive() )
	{

#if GTA_REPLAY
		// Prevent any replay recording when a warning screen is up
		ReplayRecordingScriptInterface::PreventRecordingThisFrame();
#endif	//GTA_REPLAY

		ms_bInactiveThisFrame = true;

		if (!ms_bActiveThisFrame)
		{
			if (!ms_bSetThisFrame )
			{
				CBusySpinner::UnregisterInstructionalButtonMovie(ms_Movie.GetMovieID());
				ms_Movie.RemoveMovie();
				ms_bMovieStreaming = false;

				// Clear last shown message hashes
				ms_cLastMessageStringHash.Clear();
				ms_cLastHeaderTextId.Clear();
				ms_cLastSubtextTextId.Clear();
			}
			else
			{
				ms_Movie.CallMethod("REMOVE_LIST_ITEMS");
				ms_Movie.CallMethod("HIDE_POPUP_WARNING", 0);
				ms_bSetThisFrame = false;
				ms_bSetOptions = false;
			}
			// Disable the inputs a bit earlier than once the movie's unloaded
			CControlMgr::GetMainFrontendControl(false).DisableAllInputs();
		}
		else
		{
			if (!ms_bMovieStreaming)
			{
				if ((ms_iFlags & FE_WARNING_NOSOUND) == 0)
					CheckAllInput(true);

				if(!ms_bSetThisFrame)
				{
					CNewHud::UpdateDisplayConfig(ms_Movie.GetMovieID(), SF_BASE_CLASS_GENERIC);  // send the screen info to the movie

					if (ms_Movie.BeginMethod("SHOW_POPUP_WARNING"))
					{
						ms_Movie.AddParam(0);

#if __NO_OUTPUT || !__BANK
#define STRING_IF_AVAIL(x) ms_Movie.AddParamLocString(x.GetHash());
#else
#define STRING_IF_AVAIL(x) x.TryGetCStr() ? ms_Movie.AddParamLocString(x.TryGetCStr()) : ms_Movie.AddParamLocString(x.GetHash());
#endif

						if (ms_cHeaderTextId.IsNotNull())
						{
							STRING_IF_AVAIL(ms_cHeaderTextId);
						}
						else
						{
							ms_Movie.AddParamString("\0");
						}

						ms_Movie.AddParamString(ms_cMessageString, false);

						if (ms_cSubtextTextId.IsNotNull())
						{
							STRING_IF_AVAIL(ms_cSubtextTextId);
						}
						else
						{
							ms_Movie.AddParamString("\0");
						}

						ms_Movie.AddParam(ms_bBlackBackground);
						ms_Movie.AddParam((s32)ms_Style);

						ms_Movie.AddParamString(ms_cErrorMessage);

						ms_Movie.EndMethod();
					}

					if (ms_iFlags != FE_WARNING_SPINNER)
					{
						SetInstructionalButtons(ms_iFlags);
					}
					else
					{
						ClearInstructionalButtons();
					}

					WIN32PC_ONLY ( CMousePointer::SetMouseCursorStyle(eMOUSE_CURSOR_STYLE::MOUSE_CURSOR_STYLE_ARROW); )

					ms_bSetThisFrame = true;
				}
				else
				{
					UpdateInstructionalButtons();
				}
			}
		}
	}
	else if(ms_bInactiveThisFrame)
	{
		ms_bInactiveThisFrame = false;
		CControlMgr::GetMainFrontendControl(false).DisableAllInputs(ms_iFrontendInputDisableDuration);
	}

	ms_bUnpause = UNPAUSE_ALWAYS;

	if(ms_returnFromSystemFrameDelay ==  0)
	{
		ms_bWasActiveLastFrame = ms_bActiveThisFrame;
		ms_bActiveThisFrame = false;
	}
	else if(!CPad::IsIntercepted())
	{
		--ms_returnFromSystemFrameDelay;
		ms_bWasActiveLastFrame |= ms_bActiveThisFrame;
	}

	ms_bIsControllerDisconnectWarningScreenSet = false;

#if __XENON
	// the change below was causing xbox360 warning screens after bootup to fail to display, rolled back this particular change for that platform
	if (!gRenderThreadInterface.IsUsingDefaultRenderFunction())
	{
		CScaleformMgr::UpdateAtEndOfFrame();
	}
#else //__XENON
	extern bool g_GameRunning; // from app, this SEEMS to be the only way to really see if we're out of the initial loading state... 
	if (!gRenderThreadInterface.IsUsingDefaultRenderFunction() && !g_GameRunning)
	{
		CScaleformMgr::UpdateAtEndOfFrame(false);
	}
#endif //__XENON
}



void CWarningScreen::ClearInstructionalButtons(CScaleformMovieWrapper* pOverride /* = NULL */)
{
	CMenuButtonList ButtonsToUse;

	// remove any buttons that may already be there
	ButtonsToUse.Reset();
	ButtonsToUse.Draw( pOverride ? pOverride : &ms_Movie );
}



void CWarningScreen::SetInstructionalButtons(u64 iFlags, CScaleformMovieWrapper* pOverride /* = NULL */)
{
//	if (iFlags != FE_WARNING_NONE)
	{
		CMenuButtonList ButtonsToUse;

		ASSERT_ONLY(u64 iSanityField = 0;)

		// for all the bits that are turned on
		for(int i=0; iFlags; ++i, iFlags>>=1)
		{
			if( iFlags & 1 && i < ms_ButtonData.GetCount() && BIT_64(i) != FE_WARNING_NOSOUND)
			{
				ButtonsToUse.Add( ms_ButtonData[i] );

#if __ASSERT
				int iIndex = GetButtonMaskIndex(ms_ButtonData[i].m_ButtonInput);
				if( iIndex != -1 )
				{
					uiAssertf(!BIT_ISSET(iIndex, iSanityField), "Warning Screen was assigned button prompts with the same input! This simply will not work! Check your bitindices against frontend.xml/WarningScreen and try again!");
					iSanityField |= BIT_64(iIndex);
				}
#endif
			}
		}

		// sort those into whatever order code says
		ButtonsToUse.postLoad();

		// buttons GOOOOO
		ButtonsToUse.Draw( pOverride ? pOverride : &ms_Movie );
	}
}



void CWarningScreen::UpdateInstructionalButtons()
{
#if RSG_PC
	if (ms_iFlags != FE_WARNING_SPINNER)
	{
		bool const c_InputIsKeyboardMouse = CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource();

		if (c_InputIsKeyboardMouse != ms_lastInputKeyboard)
		{
			if (ms_Movie.IsActive())
			{
				ms_Movie.ForceCollectGarbage();
			}

			ClearInstructionalButtons();
			SetInstructionalButtons(ms_iFlags);
		}

		ms_lastInputKeyboard = c_InputIsKeyboardMouse;
	}
#endif  // #if RSG_PC
}



#if RSG_PC
void CWarningScreen::DeviceLost()
{

}

void CWarningScreen::DeviceReset()
{
	if(ms_Movie.IsActive())
	{
		CScaleformMgr::UpdateMovieParams(ms_Movie.GetMovieID());
	}
}
#endif //RSG_PC


void CWarningScreen::Render()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if !RSG_DURANGO
	// fix for 1817805 - need to revise this for orbis to work.  It fades out so we need to keep displaying the black background and not the warning when the system ui is up otherwise you see the game behind as system ui fades
	// doing this on the update() function also messes with the per frame checks causing flickering
	if (CLiveManager::IsSystemUiShowing())
	{
		if (ms_bBlackBackground)
		{
			GRCDEVICE.Clear(true, Color32(0xFF000000), false, 0, false, 0);
		}

		return;
	}
#endif // !RSG_DURANGO

//	Graeme - instead of checking ms_bSetThisFrame here on the render thread, check that CWarningScreen::CanRender() returns true before calling DLC_Add(CWarningScreen::Render) in RenderPhaseStd.cpp
//	There are a few other places that call CWarningScreen::Render() directly from the render thread. I've just changed them to check CWarningScreen::CanRender() too.
	{
		if (ms_bBlackBackground)  // always show black background behind as some odd aspect ratios, warning screens do not fully fill the screen
		{
			GRCDEVICE.Clear(true, Color32(0xFF000000), false, 0, false, 0);
		}
		ms_Movie.Render(true);

		if( ms_iFlags == FE_WARNING_SPINNER )
		{
			// render the 'code' spinner: for 1613055
			Vector2 vSize(0.0073125f, 0.013f);
			const Vector2 vPos = CHudTools::CalculateHudPosition(Vector2(0,0), vSize, 'R', 'B');
			CPauseMenu::RenderAnimatedSpinner(vSize, vPos, true, false, CSavingMessage::ShouldDisplaySavingSpinner()?true:false);
		}
	}
}



//
// name:		SetAndWaitOnMessageScreen
// description:	Display error message and wait for button press
bool CWarningScreen::SetAndWaitOnMessageScreen(eWarningMessageStyle style, const atHashWithStringBank pText, const eWarningButtonFlags iFlags, bool bInsertNumberIntoString, s32 iNumberToInsert, eWarningScreenUnpause eUnpause)
{
	bool bPositiveResult = true;

	// Display error message
	CGtaOldLoadingScreen::ForceLoadingRenderFunction(true);
	gRenderThreadInterface.Flush();
	
	CControlMgr::StopPlayerPadShaking();

	// Wait on <A> being pressed
	while(true)
	{
		Update();

		SetMessage(style, pText, iFlags, bInsertNumberIntoString, iNumberToInsert);

		fwTimer::Update();
		CControlMgr::Update();
		g_SysService.UpdateClass();

		if( IsFlagForButtonActive(INPUT_FRONTEND_ACCEPT, ms_iFlags) )
		{
			if(CControlMgr::GetMainFrontendControl(false).GetFrontendAccept().IsReleased())
			{
				bPositiveResult = true;
				break;
			}
		}

		if( IsFlagForButtonActive(INPUT_FRONTEND_CANCEL, ms_iFlags) )
		{
			if(CControlMgr::GetMainFrontendControl(false).GetFrontendCancel().IsReleased())
			{
				bPositiveResult = false;
				break;
			}
		}

		sysIpcSleep(16);
	}

	if ((eUnpause == UNPAUSE_ALWAYS) ||
		(bPositiveResult && eUnpause == UNPAUSE_ON_ACCEPT) ||
		(!bPositiveResult && eUnpause == UNPAUSE_ON_CANCEL))
	{
		ms_bUnpause = true;
	}
	else
	{
		ms_bUnpause = false;
	}

	CGtaOldLoadingScreen::ForceLoadingRenderFunction(false);
	
	Remove();

	//CControlMgr::GetMainFrontendControl(false).InitEmptyControls();  // clear any input before we return

	return bPositiveResult;
}

void CWarningScreen::SetFatalReadMessageWithHeader( const eWarningMessageStyle style, 
													const atHashWithStringBank pTextLabelHeading, 
													const atHashWithStringBank pTextLabelBody, 
													const eWarningButtonFlags iFlags, 
													const bool bInsertNumber, 
													const s32 NumberToInsert, 
													const char *pFirstSubString, 
													const char *pSecondSubString, 
													const bool bBlackBackground, 
													const bool bAllowSpinner,
													int errorNumber)
{
	bool bPositiveResult = true;

	// Display error message
	CGtaOldLoadingScreen::ForceLoadingRenderFunction(true);
	gRenderThreadInterface.Flush();

	CControlMgr::StopPlayerPadShaking();

	// Wait on <A> being pressed
	while(true)
	{
		Update();

		SetMessageInternal( style,
			pTextLabelHeading,
			pTextLabelBody,
			0u,
			iFlags, 
			bInsertNumber,
			NumberToInsert, 
			pFirstSubString,
			pSecondSubString, 
			bBlackBackground,
			bAllowSpinner,
			errorNumber);

		fwTimer::Update();
		CControlMgr::Update();
		g_SysService.UpdateClass();

		if( IsFlagForButtonActive(INPUT_FRONTEND_ACCEPT, ms_iFlags) )
		{
			if(CControlMgr::GetMainFrontendControl(false).GetFrontendAccept().IsReleased())
			{
				bPositiveResult = true;
				break;
			}
		}

		if( IsFlagForButtonActive(INPUT_FRONTEND_CANCEL, ms_iFlags) )
		{
			if(CControlMgr::GetMainFrontendControl(false).GetFrontendCancel().IsReleased())
			{
				bPositiveResult = false;
				break;
			}
		}

		sysIpcSleep(16);
	}

	CGtaOldLoadingScreen::ForceLoadingRenderFunction(false);
	Remove();	
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CWarningMessage::Data::UpdateCallbacks()
{
	m_onUpdate.Call();
#define IF_CHECK_BUTTON(rawButton, cb)\
	if( CWarningScreen::IsFlagForButtonActive(rawButton, m_iFlags) && CWarningScreen::CheckInput(rawButton, m_bCloseAfterPress, CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE)) \
	{ cb.Call(); if(m_bCloseAfterPress) return true; }

	 	 IF_CHECK_BUTTON(INPUT_FRONTEND_ACCEPT,	m_acceptPressed)
	else IF_CHECK_BUTTON(INPUT_FRONTEND_CANCEL,	m_declinePressed)
	else IF_CHECK_BUTTON(INPUT_FRONTEND_X,		m_xPressed)
	else IF_CHECK_BUTTON(INPUT_FRONTEND_Y,		m_yPressed)

	return false;
}

void CWarningMessage::Update()
{
	if(m_isActive)
	{
		if( m_data.UpdateCallbacks() )
		{
			Clear();
			return;
		}

		if(m_isActive)
		{
			CWarningScreen::SetMessageWithHeaderAndSubtext(m_data.m_Style,
				m_data.m_TextLabelHeading,
				m_data.m_TextLabelBody,
				m_data.m_TextLabelSubtext,
				m_data.m_iFlags,
				m_data.m_bInsertNumber,
				m_data.m_NumberToInsert, 
				m_data.m_FirstSubString.GetData(), 
				m_data.m_SecondSubString.GetData());
		}
	}
}

void CWarningMessage::Clear()
{
	m_isActive = false;
	m_data.Clear();
}

void CWarningMessage::SetMessage(const Data& rMessage)
{
	m_isActive = true;

	m_data = rMessage;
}

// eof
