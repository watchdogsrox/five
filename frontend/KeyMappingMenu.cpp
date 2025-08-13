#include "Frontend/KeyMappingMenu.h"

#if KEYBOARD_MOUSE_SUPPORT

#include "Frontend/PauseMenu.h"
#include "Frontend/ScaleformMenuHelper.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/ButtonEnum.h"
#include "Frontend/ui_channel.h"
#include "system/control.h"
#include "system/controlMgr.h"
#include "Frontend/UIContexts.h"

#define INVALID_RETURN_INDEX -1
#define RESTORE_DEFAULTS_ID -3000 // just a random number to get it to behave
#define SF_SECONDARY_INPUT_OFFSET 1000

//OPTIMISATIONS_OFF()
FRONTEND_OPTIMISATIONS()

PARAM(kbAutoResolveConflicts, "Never display keybinding conflicts with a warning screen");

bool CKeyMappingMenu::sm_bAutoResolveConflicts = false;

CKeyMappingMenu::CKeyMappingMenu(CMenuScreen& owner) 
	: CMenuBase(owner)
	, m_curMappingInput(UNDEFINED_INPUT)
	, m_lastConflict(UNDEFINED_INPUT)
	, m_curCategory()
	, m_iKeysInCategory(0)
	, m_iCurBindingIndex(-1)
	, m_bInSubMenu(false)
	, m_bTouchedAControl(false)
#if __BANK
	, m_pMyGroup(NULL)
#endif
{

#if !__FINAL
	if( PARAM_kbAutoResolveConflicts.Get() )
	{
		sm_bAutoResolveConflicts = !PARAM_kbAutoResolveConflicts.Get();
		PARAM_kbAutoResolveConflicts.Set(NULL);
	}
#endif

}

CKeyMappingMenu::~CKeyMappingMenu()
{
#if __BANK
	if( m_pMyGroup )
	{
		m_pMyGroup->Destroy();
		m_pMyGroup = NULL;
	}
#endif
}

void CKeyMappingMenu::Update()
{
	CControl& control = CControlMgr::GetPlayerMappingControl();

	if( control.IsMappingInProgress() )
	{
		control.DisableAllInputs();
		CControlMgr::GetMainFrontendControl().DisableAllInputs();


		uiAssertf(control.GetMappingInProgress() == m_curMappingInput, "Currently mapping %s, but expected %s!", CControl::GetInputName(control.GetMappingInProgress()), CControl::GetInputName(m_curMappingInput));
		ioValue::ReadOptions options(ioValue::NO_DEAD_ZONE);
		options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);

		// we have to check for the controls here because PauseMenu earlies out on us and we don't get UpdateInput called
		if( control.GetFrontendCancel().IsReleased(ioValue::BUTTON_DOWN_THRESHOLD, options) && m_curMappingInput != UNDEFINED_INPUT )
		{
			control.CancelCurrentMapping();

			SendControlToScaleform(m_curMappingInput, m_iCurBindingIndex, true, 0);
			CPauseMenu::PlayInputSound( FRONTEND_INPUT_BACK );
		}
		// handle conflicts with a warning screen
		else if( control.HasNewMappingConflict() )
		{
			CWarningMessage& rMessage = CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage();
			const CControl::MappingConflictList& conflictList = control.GetMappingConflictList();
			bool isMappable = control.AreConflictingInputsMappable();

			if( sm_bAutoResolveConflicts && isMappable )
			{
				AcceptConflict();
			}
			else if( !rMessage.IsActive() )
			{
				const char* conflictingInputName = CControl::GetInputName(conflictList[0].m_Input);

				CWarningMessage::Data messageData;
				messageData.m_TextLabelHeading = "CWS_WARNING";

				// build the first input's button string display "~INPUT_VEH_DUCK:0~"
				{
					atStringBuilder controlButtonString;
					controlButtonString.Append("~");
					controlButtonString.Append(conflictingInputName);
					controlButtonString.Append(":");

					// We only need 4 as index is 8bit so max 3 chars plus null.
					char indexStr[4] = {0};
					formatf(indexStr, "%d", conflictList[0].m_MappingIndex);
					controlButtonString.Append(indexStr);
					controlButtonString.Append("~");

					messageData.m_FirstSubString.Set( controlButtonString.ToString() ); // just send the conflicting key. Since it's the same as the one we're binding ('cuz it's a conflict), this will "just work"
				}

				char secondPart[MAX_CHARS_IN_EXTENDED_MESSAGE];

				int subCount = 0;
				const char* pszMessageToUse = nullptr;

				CSubStringWithinMessage SubStringArray[3];
				atStringBuilder listOfConflicts;

				if(!isMappable)
				{
					messageData.m_TextLabelBody = "UNMAPPABLE_CONFLICT_BODY";
					pszMessageToUse = "UNMAPPABLE_CONFLICT_BODY_PT2";
					SubStringArray[subCount++].SetTextLabel(conflictingInputName);
					SubStringArray[subCount++].SetTextLabel(CControl::GetInputName(m_curMappingInput));

					messageData.m_iFlags = FE_WARNING_OK;
					messageData.m_acceptPressed = datCallback(MFA(CKeyMappingMenu::IgnoreConflict), this);

				}
				else
				{
					messageData.m_acceptPressed = datCallback(MFA(CKeyMappingMenu::AcceptConflict), this);
					messageData.m_declinePressed = datCallback(MFA(CKeyMappingMenu::IgnoreConflict), this);	
					messageData.m_iFlags = FE_WARNING_OK_CANCEL;

					if( conflictList.size() == 1 )
					{
						// just one conflict is easy
						messageData.m_TextLabelBody = "MAPPING_CONFLICT_BODY";
						pszMessageToUse = "MAPPING_CONFLICT_BODY_PT2";
						SubStringArray[subCount++].SetTextLabel(conflictingInputName);
						SubStringArray[subCount++].SetTextLabel(CControl::GetInputName(m_curMappingInput));
					}
					else
					{
						messageData.m_TextLabelBody = "MAPPING_MULTI_CONFLICT_BODY";
						pszMessageToUse = "MAPPING_MULTI_CONFLICT_BODY_PT2";
						SubStringArray[subCount++].SetTextLabel(CControl::GetInputName(m_curMappingInput));			

						int conflictIndex=0;
						for(;conflictIndex < conflictList.size() && conflictIndex < conflictList.GetMaxCount()-1; ++conflictIndex) // -1 so we know if there's "more"
						{
							listOfConflicts.Append(TheText.Get(CControl::GetInputName(conflictList[conflictIndex].m_Input)));
							listOfConflicts.Append("\n");
						}

						if( conflictIndex < conflictList.size() )
							listOfConflicts.Append( TheText.Get("MAPPING_MULTI_CONFLICT_BODY_ETC") );

						SubStringArray[subCount++].SetCharPointer(listOfConflicts.ToString());
					}
				}
				
				// last value is always the desired end control
				
				CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get(pszMessageToUse), 
					NULL, 0, 
					SubStringArray, subCount, 
					secondPart, MAX_CHARS_IN_EXTENDED_MESSAGE);
				messageData.m_SecondSubString.Set( secondPart );

				rMessage.SetMessage(messageData);
			} // no message up
		} // no conflict
	}
	// no longer mapping
	else if( m_curMappingInput != UNDEFINED_INPUT )
	{
		control.DisableAllInputs();
		CControlMgr::GetMainFrontendControl().DisableAllInputs();

		AdjustWarningIndicators();

		// make sure we re-establish the buttons
		if(m_bInSubMenu)
		{
			SetInSubmenu(true);
		}

		m_curMappingInput = UNDEFINED_INPUT;
		m_lastConflict = UNDEFINED_INPUT;
		// clear the warning message that may have appeared if we were resolving conflicts. Prevents a flash if there's chained conflicts
		CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
	}
}

bool CKeyMappingMenu::UpdateInput(s32 eInput)
{
	if( eInput == PAD_CIRCLE && !m_bInSubMenu )
	{
		if( !m_conflictingInputs.empty() )
		{
			if( SUIContexts::IsActive("HadMappingConflict") )
			{
				CWarningMessage::Data messageData;
				messageData.m_TextLabelHeading = "CWS_WARNING";
				messageData.m_TextLabelBody = "UNMAPPED_WARNING_NO_ESCAPE";
				messageData.m_iFlags = FE_WARNING_OK;
				messageData.m_bCloseAfterPress = true;
				CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(messageData);
				CPauseMenu::PlayInputSound( FRONTEND_INPUT_BACK );
			}
			else
			{
				CPauseMenu::ShowConfirmationAlert("CONFLICT_WARNING", datCallback(MFA(CKeyMappingMenu::AcceptBackingOut), this));
				CPauseMenu::PlayInputSound( FRONTEND_INPUT_BACK );
			}
			return true;
		}
		else if( !m_unmappedInputs.empty())
		{
			CPauseMenu::ShowConfirmationAlert("UNMAPPED_WARNING", datCallback(MFA(CKeyMappingMenu::AcceptBackingOut), this));
			CPauseMenu::PlayInputSound( FRONTEND_INPUT_BACK );
			return true;
		}
	}


	// check if they want to delete an item
	if( m_curMappingInput == UNDEFINED_INPUT && CControlMgr::GetMainFrontendControl().GetValue(INPUT_FRONTEND_DELETE).IsReleased() )
	{
		if( m_bInSubMenu )
		{
			rage::InputType input;
			u8 mappingSlot;
			if( GetInputForCurrentSelection(input, mappingSlot) && IsInputBound(input, mappingSlot) && CControl::IsInputMappable(input) )
			{
				if (input == INPUT_FRONTEND_SOCIAL_CLUB_SECONDARY && mappingSlot == 0)
				{
					// B* 2349103
					//  The SCUI hotkey is not technically bindable away from HOME, so we want to prevent this one field
					//  from being deleted. The alternating (mappingSlot == 1) is still fine.
					return true;
				}
			
				CControlMgr::GetPlayerMappingControl().DeleteInputMapping(input, CControl::PC_KEYBOARD_MOUSE, mappingSlot);
				AdjustWarningIndicators(true);
				CPauseMenu::PlayInputSound( FRONTEND_INPUT_X );

				SUIContexts::Deactivate("KEYMAP_CAN_DELETE");
				CPauseMenu::RedrawInstructionalButtons();
				m_bTouchedAControl = true;
				return true;
			}
		}
	}

	return false;
}

bool CKeyMappingMenu::ShouldBlockCloseAttempt(const char*& rpWarningMessageToDisplay, bool& bCanContinue)
{
	if( !m_conflictingInputs.empty() )
	{
		bCanContinue = false;
		rpWarningMessageToDisplay = "UNMAPPED_TU_WARNING";
		return true;
	}
	else if( !m_unmappedInputs.empty() )
	{
		bCanContinue = true;
		rpWarningMessageToDisplay = "UNMAPPED_WARNING";
		return true;
	}

	return false;
}

void CKeyMappingMenu::AcceptBackingOut()
{
	CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER);
}

void CKeyMappingMenu::AcceptConflict()
{
	CControlMgr::GetPlayerMappingControl().ReplaceMappingConflict();
}

void CKeyMappingMenu::IgnoreConflict()
{
	CControlMgr::GetPlayerMappingControl().CancelCurrentMapping();
	CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
	CPauseMenu::PlayInputSound( FRONTEND_INPUT_BACK );
}


bool CKeyMappingMenu::Populate(MenuScreenId newScreenId)
{
	if( newScreenId.GetValue() != MENU_UNIQUE_ID_KEYMAP)
	{
		return false;
	}

	m_bTouchedAControl = false;

	// if they had some unsaved controls last time...
	if(!SUIContexts::IsActive("HadMappingConflict") && CControlMgr::GetPlayerMappingControl().HasAutoSavedUserControls())
	{
		CWarningMessage::Data messageData;
		messageData.m_TextLabelHeading = "CWS_WARNING";
		messageData.m_TextLabelBody = "MAPPING_AUTOSAVE_LOADED";
		messageData.m_iFlags = FE_WARNING_YES_NO_CANCEL;
		messageData.m_bCloseAfterPress = true;
		messageData.m_acceptPressed  = datCallback(MFA(CKeyMappingMenu::AcceptAutosave), this);
		messageData.m_xPressed       = datCallback(MFA(CKeyMappingMenu::RejectAutosave), this);
		messageData.m_declinePressed = datCallback(CFA(CPauseMenu::MenuceptionTheKick));
		CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(messageData);

		return true;
	}

	if( CScaleformMenuHelper::SET_COLUMN_TITLE(PM_COLUMN_MIDDLE, TheText.Get("MAPPING_HDR"), false) )
	{
		CScaleformMgr::AddParamLocString("MAPPING_HDR_PRIM");
		CScaleformMgr::AddParamLocString("MAPPING_HDR_SEC");
		CScaleformMgr::EndMethod();
	}


	int lastAddedIndex = AdjustWarningIndicators(false);

	// add in restore defaults
	CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_LEFT, lastAddedIndex, PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_KEYMAP_LIST, RESTORE_DEFAULTS_ID, eOPTION_DISPLAY_STYLE(5), 0, true, TheText.Get("MAPPING_DEFAULTS"));

	CScaleformMenuHelper::DISPLAY_DATA_SLOT(PM_COLUMN_LEFT);

	SHOW_COLUMN(PM_COLUMN_LEFT, true);
	SHOW_COLUMN(PM_COLUMN_MIDDLE, true);

	// necessary to spoof "gain focus"
	CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "MENU_INTERACTION", PAD_NO_BUTTON_PRESSED );

#if RSG_PC
	// Block Social Club Hotkey
	g_rlPc.GetUiInterface()->EnableHotkey(false);
#endif

	return true;
}

void CKeyMappingMenu::AcceptAutosave()
{
	CControl& control = CControlMgr::GetPlayerMappingControl();
	control.SetInitialDefaultMappings(true, true);
	control.DeleteAutoSavedUserControls();
	Populate(MENU_UNIQUE_ID_KEYMAP);
	m_bTouchedAControl = true;
}

void CKeyMappingMenu::RejectAutosave()
{
	CControl& control = CControlMgr::GetPlayerMappingControl();
	control.DeleteAutoSavedUserControls();
	Populate(MENU_UNIQUE_ID_KEYMAP);
	m_bTouchedAControl = true;
}

int CKeyMappingMenu::AdjustWarningIndicators(bool bIsUpdate /* = true */)
{
	m_conflictingInputs.Reset();
	m_unmappedInputs.Reset();
	CControlMgr::GetPlayerMappingControl().GetUnmappedInputs(m_unmappedInputs);
	CControlMgr::GetPlayerMappingControl().GetConflictingInputs(m_conflictingInputs);

	// get all of the data-driven binding categories
	CControl::MappingCategoriesList mcl;
	CControl::GetMappingCategories(mcl);
	int iMapIndex = 0;
	for(; iMapIndex < mcl.GetCount(); ++iMapIndex)
	{		
		int iShowMissingIcon = 0;
		if( !m_unmappedInputs.empty() || !m_conflictingInputs.empty() )
		{
			CControl::InputList cil;
			CControl::GetCategoryInputs(mcl[iMapIndex], cil);
			for(InputType eMissingInput : m_unmappedInputs)
			{
				if( cil.Find(eMissingInput) != -1 )
				{
					iShowMissingIcon = 1;
					break;
				}
			}

			for(InputType eConflictInput : m_conflictingInputs)
			{
				if( cil.Find(eConflictInput) != -1 )
				{
					iShowMissingIcon = 1; /// may want to differentiate these icons...
					break;
				}
			}
		}
		CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMN_LEFT
			, iMapIndex
			, PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_KEYMAP_LIST
			, mcl[iMapIndex]
			, OPTION_DISPLAY_NONE
			, iShowMissingIcon
			, true
			, TheText.Get(mcl[iMapIndex], mcl[iMapIndex].TryGetCStr())
			, true
			, bIsUpdate
			);
	}

	if( !bIsUpdate )
		m_curCategory = mcl[0];

	// draw the middle pane
	UpdateCategory(m_curCategory, bIsUpdate);

	if( !m_conflictingInputs.empty() )
		CPauseMenu::SetWarnOnTabChange( "CONFLICT_WARNING");

	else if( !m_unmappedInputs.empty() )
		CPauseMenu::SetWarnOnTabChange( "UNMAPPED_WARNING" );
	
	else
		CPauseMenu::SetWarnOnTabChange( NULL );


	// display a warning hint
	CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod("SET_DESCRIPTION", 0, (!m_conflictingInputs.empty() || !m_unmappedInputs.empty()) ? TheText.Get("UNMAPPED_INPUTS_HINT") : "");

	return iMapIndex;
}

bool CKeyMappingMenu::CheckIncomingFunctions(atHashWithStringBank UNUSED_PARAM(methodName), const GFxValue* UNUSED_PARAM(args))
{
	return false;
}


void CKeyMappingMenu::AdjustScrollbar(bool bAreWe)
{
	// technically the scroll column exists on the left column
	if( bAreWe && m_iKeysInCategory > 16 ) // maximum number visible at once
	{
		CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMN_MIDDLE, 0, m_iKeysInCategory+1); // we don't display a number here, so whatever
	}
	else
	{
		CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMN_MIDDLE);
	}
}

bool CKeyMappingMenu::InitScrollbar()
{
	if( CPauseMenu::IsScreenDataValid(MENU_UNIQUE_ID_KEYMAP_LISTITEM) )
	{
		CMenuScreen& rData = CPauseMenu::GetScreenData(MENU_UNIQUE_ID_KEYMAP_LISTITEM);
		rData.INIT_SCROLL_BAR();
	}
	return false;
}


void CKeyMappingMenu::LayoutChanged( MenuScreenId OUTPUT_ONLY(iPreviousLayout), MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir)
{
	uiDebugf3("CKeyMappingMenu::LayoutChanged - Prev: %i, New: %i, Unique: %i, Dir: %i", iPreviousLayout.GetValue(), iNewLayout.GetValue(), iUniqueId, eDir);
	if( iNewLayout == MENU_UNIQUE_ID_KEYMAP_LIST )
	{
		if( iUniqueId == RESTORE_DEFAULTS_ID )
		{
			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_MIDDLE);
			CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_MIDDLE, false);
			CScaleformMenuHelper::DISPLAY_DATA_SLOT(PM_COLUMN_MIDDLE);
			m_curCategory.Clear();
		}
		else if(eDir != LAYOUT_CHANGED_DIR_BACKWARDS)
		{
			UpdateCategory( atHashWithStringNotFinal(iUniqueId), false);
		}

		SetInSubmenu(false);
		CPauseMenu::RedrawInstructionalButtons();
	}
	else if(iNewLayout == MENU_UNIQUE_ID_KEYMAP_LISTITEM)
	{
		m_iCurHighlightedId = iUniqueId;

		SetInSubmenu(true);
	}
	AdjustScrollbar(true);
}

void CKeyMappingMenu::LoseFocus()
{
	uiDebugf3("CKeyMappingMenu::LoseFocus");
	CControl& control = CControlMgr::GetPlayerMappingControl();
	control.CancelCurrentMapping();
	// if they touched something, save it out 
	if( m_bTouchedAControl )
	{
		// the true and false is for saving out to the autosave backup save
		if( !control.SaveUserControls(false) )
		{
			control.SaveUserControls(true);
		}
	}
	else
	{
		uiDisplayf("Didn't save out controls because they didn't touch anything");
	}
	// We actually need to reload the controls all the time.
	control.SetInitialDefaultMappings(true, false);

	CPauseMenu::SetWarnOnTabChange( NULL );
	m_conflictingInputs.Reset();
	m_unmappedInputs.Reset();
	SetInSubmenu(false);
	m_bTouchedAControl = false;

#if RSG_PC
	// Re-Enable Social Club Hotkey
	g_rlPc.GetUiInterface()->EnableHotkey(true);
#endif
}

void CKeyMappingMenu::UpdateCategory(const atHashWithStringNotFinal& uCategory, bool bIsUpdate)
{
	m_curCategory = uCategory;
	
	if( !bIsUpdate )
	{
		CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMN_MIDDLE);
		CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMN_MIDDLE, true);
	}
	
	CControl::InputList cil;
	CControl::GetCategoryInputs(uCategory, cil);

	m_iKeysInCategory = 0;

	for(s32 iInputIndex = 0; iInputIndex < cil.GetCount(); ++iInputIndex)
	{
		SendControlToScaleform(cil[iInputIndex], m_iKeysInCategory++, bIsUpdate, 0);
	}

	if( !bIsUpdate )
	{
		CScaleformMenuHelper::DISPLAY_DATA_SLOT(PM_COLUMN_MIDDLE);
		m_iCurHighlightedId = 0;
	}
}

bool CKeyMappingMenu::TriggerEvent(MenuScreenId MenuId, s32 iUniqueId)
{
	// player has initiated a binding action!
	if( MenuId == MENU_UNIQUE_ID_KEYMAP_LISTITEM )
	{
		m_iCurHighlightedId = iUniqueId;
		
		u8 mappingSlot;
		int iSlotBinding;
		if( GetInputForCurrentSelection(m_curMappingInput, mappingSlot, &m_iCurBindingIndex, &iSlotBinding) )
		{
			if (m_curMappingInput == INPUT_FRONTEND_SOCIAL_CLUB_SECONDARY && mappingSlot == 0)
			{
				// B* 2349103
				//  The SCUI hotkey is not technically bindable away from HOME, so we want to prevent this one field
				//  from being edited. The alternating (mappingSlot == 1) is still fine.
				return true;
			}

			CControlMgr::GetPlayerMappingControl().StartInputMappingScan(m_curMappingInput, CControl::PC_KEYBOARD_MOUSE, mappingSlot);
			SendControlToScaleform(m_curMappingInput, m_iCurBindingIndex, true, iSlotBinding);
			CPauseMenu::PlayInputSound( FRONTEND_INPUT_ACCEPT );

			SUIContexts::Activate("HIDE_ACCEPTBUTTON");
			SUIContexts::Deactivate("KEYMAP_CAN_DELETE");
			CPauseMenu::RedrawInstructionalButtons();
			m_bTouchedAControl = true;
		}

		
		return true;
	}
	else if( MenuId == MENU_UNIQUE_ID_KEYMAP_LIST )
	{
		// restore the defaults
		if( iUniqueId == RESTORE_DEFAULTS_ID )
		{
			CWarningMessage::Data messageData;
			messageData.m_TextLabelHeading = "CWS_WARNING";
			messageData.m_TextLabelBody = "MAPPING_RESTORE_DEFAULTS";
			messageData.m_iFlags = FE_WARNING_YES_NO;
			messageData.m_bCloseAfterPress = true;
			messageData.m_acceptPressed = datCallback(MFA(CKeyMappingMenu::AcceptRestoreDefaults), this);

			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(messageData);
		}
		else
		{
			SetInSubmenu(true);
		}
	}

	return false;
}

bool CKeyMappingMenu::GetInputForCurrentSelection(InputType& outInput, u8& outMappingSlot, int* pOutCurSelectionIndex /*= NULL*/, int* pOutSlotBinding /*= NULL*/) const
{
	CControl::InputList cil;
	CControl::GetCategoryInputs(m_curCategory, cil);

	int iAdjIndex = m_iCurHighlightedId;
	if( iAdjIndex < SF_SECONDARY_INPUT_OFFSET )
	{
		outMappingSlot = 0;
		if(pOutSlotBinding) *pOutSlotBinding = 1;
	}
	else
	{
		iAdjIndex -= SF_SECONDARY_INPUT_OFFSET;
		outMappingSlot = 1;
		if(pOutSlotBinding) *pOutSlotBinding = 2;
	}

	if(pOutCurSelectionIndex) *pOutCurSelectionIndex = iAdjIndex;
	
	if( uiVerifyf(iAdjIndex>= 0 && iAdjIndex < cil.GetCount(), "Triggered an action on a bad input ID!") )
	{
		outInput = cil[iAdjIndex];
		return true;
	}

	outInput = UNDEFINED_INPUT;
	return false;
}

void CKeyMappingMenu::AcceptRestoreDefaults()
{
	CControlMgr::GetPlayerMappingControl().RestoreDefaultMappings();
	CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, 0, true);
	CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_LEFT, true, true, true );
	AdjustWarningIndicators();
	m_bTouchedAControl = true;
}

void CKeyMappingMenu::SendControlToScaleform(InputType input, int iSlotIndex, bool bIsUpdate, int slotBinding)
{
	const char* inputName = CControl::GetInputName(input);
	bool bShowWarning = m_unmappedInputs.Find(input) != -1 || m_conflictingInputs.Find(input) != -1;

	CScaleformMenuHelper::SET_DATA_SLOT(
		PM_COLUMN_MIDDLE,
		iSlotIndex,
		PREF_OPTIONS_THRESHOLD + MENU_UNIQUE_ID_KEYMAP_LISTITEM,
		iSlotIndex,
		static_cast<eOPTION_DISPLAY_STYLE>(slotBinding),
		bShowWarning?1:0,
		CControl::IsInputMappable(input) ? 1 : -1, // SF is weird and treats -1 as bad... instead of, say, 0.
		TheText.Get(inputName), false, bIsUpdate);

	SendInput(input, 0);
	SendInput(input, 1);

	CScaleformMgr::EndMethod();
}

bool CKeyMappingMenu::IsInputBound(InputType input, u8 mappingSlot) const
{
	ioSource source = CControlMgr::GetPlayerMappingControl().GetInputSource(input, ioSource::IOMD_KEYBOARD_MOUSE, mappingSlot, false);
	return source.m_Device != IOMS_UNDEFINED && source.m_Parameter != ioSource::UNDEFINED_PARAMETER && source.m_Parameter != KEY_NULL;
}

void CKeyMappingMenu::SendInput(InputType input, u8 slot)
{
	// verify we actually have an input bound for this.
	if(IsInputBound(input, slot))
	{
		atStringBuilder builder;

		builder.Append("~");
		builder.Append(CControl::GetInputName(input));
		builder.Append("~");
		CScaleformMgr::AddParamString(builder.ToString(), false);
	}
	else
	{
		CScaleformMgr::AddParamString("",false); // send an empty string
	}
}

void CKeyMappingMenu::SetInSubmenu(bool bInIt)
{
	m_bInSubMenu = bInIt;
	if( m_bInSubMenu )
	{
		InputType input;
		u8 slot;
		if( GetInputForCurrentSelection(input, slot) )
		{
			if (input == INPUT_FRONTEND_SOCIAL_CLUB_SECONDARY && slot == 0)
			{
				// B* 2349103
				//  The SCUI hotkey is not technically bindable away from HOME, so we want to prevent this delete prompt from displaying
				SUIContexts::Deactivate("KEYMAP_CAN_DELETE");
			}
			else
			{
				SUIContexts::SetActive("KEYMAP_CAN_DELETE", IsInputBound(input, slot) && CControl::IsInputMappable(input) );
			}
			
			SUIContexts::SetActive("HIDE_ACCEPTBUTTON", !CControl::IsInputMappable(input));
		}
	}
	else
	{
		SUIContexts::Deactivate("KEYMAP_CAN_DELETE");
		SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");
	}

	CPauseMenu::RedrawInstructionalButtons();
}

#if __BANK
void CKeyMappingMenu::AddWidgets(bkBank* ToThisBank)
{
	if(m_pMyGroup)
		return;

	m_pMyGroup = ToThisBank->PushGroup("Keymapping Screen");
	ToThisBank->AddToggle("Auto accept conflicts", &sm_bAutoResolveConflicts);
	ToThisBank->PopGroup();
}
#endif // __BANK

#endif // KEYBOARD_MOUSE_SUPPORT