// rage
#include <algorithm> // qsort
#include "parser/treenode.h"
#include "parser/memberenumdata.h"

// game
#include "CMenuBase.h"
#include "CMenuBase_parser.h"
#include "Frontend/HudTools.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/PauseMenuData.h"
#include "Frontend/ui_channel.h"
#include "text/GxtString.h"
#include "text/TextFormat.h"


extern rage::parEnumData parser_eMenuScreen_Data;
extern rage::parEnumData parser_eInstructionButtons_Data;

FRONTEND_MENU_OPTIMISATIONS()

#if __BANK
int CMenuButton::sm_Allocs = 0;
int CMenuButton::sm_Count  = 0;

int CMenuButtonList::sm_Allocs = 0;
int CMenuButtonList::sm_Count  = 0;

int CMenuBase::sm_Allocs = 0;
int CMenuBase::sm_Count  = 0;
#endif

void MenuScreenId::SetFromXML(const char* str)
{
	// if there's a dedicated enum value, use that first.
	// failing that, hash it so it's extensible.

	using namespace rage;
	parEnumData::ValueFromNameStatus status = parEnumData::VFN_EMPTY;
	m_iValue = parser_eMenuScreen_Data.ValueFromName(str,NULL,&status);

	Assertf(status != parEnumData::VFN_NUMERIC,	"We don't really support numeric menuIds! of %s!", str);
	Assertf(status != parEnumData::VFN_EMPTY,	"Empty/unparseable tag of '%s' encountered!", str);

	// otherwise, catch the error case and hash it instead
	if( status == parEnumData::VFN_ERROR )
	{
		m_iValue = atStringHash(str);
		Assertf(m_iValue < MENU_UNIQUE_ID_START || m_iValue >= eMenuScreen_MAX_VALUE, "Crazy hash collision! '%s' hashes to the value of an enum! Better change its name", str);
	}
}

const char* MenuScreenId::GetParserName() const
{
	parser_eMenuScreen_Data.VerifyNamesExist(true, "getting UI name");
	const char* pszNameAttempt = parser_eMenuScreen_Data.NameFromValueUnsafe(m_iValue);
	if( pszNameAttempt != NULL)
		return pszNameAttempt;

	static char hashName[12];
	formatf(hashName, COUNTOF(hashName), "0x%08x", m_iValue);
	return hashName;
}

CMenuButton::CMenuButton()
: m_ButtonInput(UNDEFINED_INPUT)
, m_ButtonInputGroup(INPUTGROUP_INVALID)
, m_RawButtonIcon(ICON_INVALID)
, m_Clickable(true)
{
#if __BANK
	++sm_Count;
	sm_Allocs += sizeof(CMenuButton);
#endif
};

#if __BANK
CMenuButton::~CMenuButton()
{
	--sm_Count;
	sm_Allocs -= sizeof(CMenuButton);
}
#endif

enum eIconSource
{
	INPUT_ICON = 0,
	INPUTGROUP_ICON,
	RAW_ICON,
	MAX_ICON_SOURCES,
};

// NOTES:	Determines the (reverse) order of buttons to appear along the prompt strip.
//			The sub array (columns) specifies the InputType, InputGroup or the
//			eInstructionButtons icon. Only one can be set per row.
const static int s_InputAndInputGroupPriority [][MAX_ICON_SOURCES] = {
	{INPUT_FRONTEND_LB,		INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_RB,		INPUTGROUP_INVALID,				ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_FRONTEND_BUMPERS,	ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_SCRIPT_BUMPERS,		ICON_INVALID},
	{INPUT_FRONTEND_LT,		INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_RT,		INPUTGROUP_INVALID,				ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_FRONTEND_TRIGGERS,	ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_SCRIPT_TRIGGERS,		ICON_INVALID},
	{INPUT_FRONTEND_LS,		INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_RS,		INPUTGROUP_INVALID,				ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_FRONTEND_STICKS,		ICON_INVALID},
	{INPUT_FRONTEND_PAUSE,	INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_SELECT, INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_UP,		INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_DOWN,	INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_LEFT,	INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_RIGHT,	INPUTGROUP_INVALID,				ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_FRONTEND_DPAD_ALL,	ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_FRONTEND_DPAD_UD,	ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_FRONTEND_DPAD_LR,	ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_INVALID,				ARROW_ALL},
	{UNDEFINED_INPUT,		INPUTGROUP_INVALID,				ARROW_LEFTRIGHT},
	{UNDEFINED_INPUT,		INPUTGROUP_INVALID,				ARROW_UPDOWN},
	{INPUT_FRONTEND_X,		INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_Y,		INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_CANCEL,	INPUTGROUP_INVALID,				ICON_INVALID},
	{INPUT_FRONTEND_ACCEPT,	INPUTGROUP_INVALID,				ICON_INVALID},
	{UNDEFINED_INPUT,		INPUTGROUP_INVALID,				ICON_SPINNER},
	{UNDEFINED_INPUT,		INPUTGROUP_INVALID,				NOTHING}
 };

static int FindPriority(const CMenuButton* button)
{
	eIconSource index;
	int test;
	if(button->m_ButtonInput != UNDEFINED_INPUT)
	{
		index = INPUT_ICON;
		test = button->m_ButtonInput;
	}
	else if(button->m_ButtonInputGroup != INPUTGROUP_INVALID)
	{
		index = INPUTGROUP_ICON;
		test = button->m_ButtonInputGroup;
	}
	else
	{
		uiAssertf(button->m_RawButtonIcon != ICON_INVALID, "Invalid InputType, InputGroup and eInstructionButtons!");
		index = RAW_ICON;
		test = button->m_RawButtonIcon;
	}

	for(int i=0; i < COUNTOF(s_InputAndInputGroupPriority); ++i)
	{
		if( test == s_InputAndInputGroupPriority[i][index] )
			return i + MAX_INPUTS;
	}

	// not found, just use original value
	uiWarningf( "Unknown InputType (%d), InputGroup (%d) and eInstructionButtons (%d) in order list!",
			   button->m_ButtonInput,
			   button->m_ButtonInputGroup,
			   button->m_RawButtonIcon );

	return test;
}

int CMenuButton::PrioritySort(const CMenuButton* a, const CMenuButton* b)
{
	int AdjA = FindPriority(a);
	int AdjB = FindPriority(b);

	if( AdjA < AdjB )
		return -1;

	return 1;
}



void CMenuButton::preLoad(parTreeNode* parNode)
{
	parTreeNode* pMenus = parNode->FindChildWithName("Contexts");
	if( pMenus )
	{
		m_ContextHashes.ParseFromString(pMenus->GetData());
	}
}

void CMenuButton::SET_DATA_SLOT( int slotIndex, CScaleformMovieWrapper& instructionButtons)
{
#if KEYBOARD_MOUSE_SUPPORT
	bool clickableEnabled = instructionButtons.CallMethod("TOGGLE_MOUSE_BUTTONS", true);
#endif // KEYBOARD_MOUSE_SUPPORT

	if( instructionButtons.BeginMethod("SET_DATA_SLOT") )
	{
		instructionButtons.AddParam(slotIndex);

		const char* pString = NULL;
		char buffer[64];

		if(m_ButtonInput != UNDEFINED_INPUT)
		{ 
			CTextFormat::GetInputButtons(m_ButtonInput, buffer, COUNTOF(buffer), NULL, 0);
			instructionButtons.AddParamString(buffer);
		}
		else if(m_ButtonInputGroup != INPUTGROUP_INVALID)
		{
			CTextFormat::GetInputGroupButtons(m_ButtonInputGroup, buffer, COUNTOF(buffer), NULL, 0);
			instructionButtons.AddParamString(buffer);
		}
		else
		{
			uiAssertf(m_RawButtonIcon != ICON_INVALID, "Invalid input, input group and raw button icon!");
			// special cases for things that don't exist in the SF
			switch(m_RawButtonIcon)
			{
				case NOTHING:
				case ICON_SPINNER:
					if( m_hButtonHash == ATSTRINGHASH("PM_DYNAMICSTRING",0x7dcd618d) )
						pString = CPauseMenu::GetDynamicPauseMenu() ? CPauseMenu::GetDynamicPauseMenu()->GetTimerMessage() : "";
					else if(m_hButtonHash.IsNull())
						pString = "";
			
				default:
					instructionButtons.AddParam(m_RawButtonIcon);
					break;
			}
		}

		if( !pString )
			pString = TheText.Get(m_hButtonHash, NULL);
		

		instructionButtons.AddParamString(pString);

#if KEYBOARD_MOUSE_SUPPORT
		if(clickableEnabled)
		{
			instructionButtons.AddParam(m_ButtonInput != UNDEFINED_INPUT && m_Clickable);
			instructionButtons.AddParam(m_ButtonInput);
		}
#endif // KEYBOARD_MOUSE_SUPPORT
		instructionButtons.EndMethod();
	}
	
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


void CMenuButtonList::preLoad(parTreeNode* pTree)
{
	// don't attempt to clone the defaults IF WE'RE THE DEFAULTS
	if( pTree->GetElement().FindAttributeBoolValue("AreTheDefaults", false) )
		return;

	CopyBitsFrom( CPauseMenu::GetMenuArray().DefaultButtonList );


	// tack on default button prompts if so desired
	if( parTreeNode* pButtons = pTree->FindChildWithName("ButtonPrompts") )
	{
		if( pButtons->GetElement().FindAttributeBoolValue("IncludeDefaults", false) )
		{
			bool bIncludeNavigate = pButtons->GetElement().FindAttributeBoolValue("IncludeNavigate", true);
			// this is probably a bit silly, but grab the DefaultButttonPrompts and append their partreenodes
			// then when we create it, we're done
			// aww, we can't find via attribute possession.
			//if( parTreeNode* pDefault = pButtons->FindFromXPath("//*[@AreTheDefaults]") )
			if( parTreeNode* pDefault = pButtons->FindFromXPath("/*/DefaultButtonList/ButtonPrompts") )
			{
				parTreeNode::ChildNodeIterator pStart = pDefault->BeginChildren();
				parTreeNode::ChildNodeIterator pEnd = pDefault->EndChildren();

				for(parTreeNode::ChildNodeIterator ci = pStart; ci != pEnd; ++ci)
				{
					parTreeNode* pCurNode = (*ci);

					if(bIncludeNavigate || !pCurNode->GetElement().FindAttributeBoolValue("IsNavigate", false) )
					{
						parTreeNode* pClone = pCurNode->Clone();
						pClone->AppendAsChildOf(pButtons);
					}
				}
			}
		}
	}
}

void CMenuButtonList::postLoad()
{
	m_ButtonPrompts.QSort(0,-1, CMenuButton::PrioritySort);
}

void CMenuButtonList::Draw(CScaleformMovieWrapper* pOverride /* = NULL */)
{
	static const u32 DynamicStringHash = ATSTRINGHASH("PM_DYNAMICSTRING",0x7dcd618d);
#if __ASSERT
	const int MAX_MENU_ICONS = 20;
	atFixedArray<atHashString, MAX_MENU_ICONS> icons;
#endif

	atArray<CMenuButton*> showList(0, m_ButtonPrompts.GetCount());
	u32 displayHash = 0;
	for(int i=0; i < m_ButtonPrompts.GetCount(); ++i )
	{
		CMenuButton& curPrompt = m_ButtonPrompts[i];
		
		if(curPrompt.CanShow())
		{
			showList.Push( &curPrompt );

			if(!pOverride)
			{
				// create a checksum/CRC/hash of the buttons we're about to show to see if we really need to change them
				displayHash = atPartialDataHash( reinterpret_cast<const char*>(&curPrompt.m_ButtonInput),	sizeof(eInstructionButtons),		displayHash );
				if( curPrompt.m_hButtonHash == DynamicStringHash )
					displayHash = atPartialStringHash( CPauseMenu::GetDynamicPauseMenu()->GetTimerMessage(), displayHash);
				else
					displayHash = atPartialDataHash( reinterpret_cast<const char*>(&curPrompt.m_hButtonHash), sizeof(atHashWithStringBank),		displayHash );
			}
			
#if __ASSERT
			const u32 BUFFER_SIZE = 64;
			char buffer[BUFFER_SIZE] = {0};

			if(curPrompt.m_RawButtonIcon != ICON_INVALID)
			{
				uiAssertf( curPrompt.m_ButtonInput == UNDEFINED_INPUT && curPrompt.m_ButtonInputGroup == INPUTGROUP_INVALID,
					"Only m_ButtonInput, m_ButtonInputGroup or m_RawButtonIcon can be set at a time (with '%s', '%s', %s)!",
					CControl::GetInputName(curPrompt.m_ButtonInput),
					CControl::GetInputGroupName(curPrompt.m_ButtonInputGroup),
					parser_eInstructionButtons_Data.NameFromValueUnsafe(curPrompt.m_RawButtonIcon) );

				formatf(buffer, "b_%d", curPrompt.m_RawButtonIcon);
			}
			else if( curPrompt.m_ButtonInput != UNDEFINED_INPUT )
			{
				uiAssertf( curPrompt.m_ButtonInputGroup == INPUTGROUP_INVALID,
					"Only m_ButtonInput, m_ButtonInputGroup or m_RawButtonIcon can be set at a time (with '%s', '%s', %s)!",
					CControl::GetInputName(curPrompt.m_ButtonInput),
					CControl::GetInputGroupName(curPrompt.m_ButtonInputGroup),
					parser_eInstructionButtons_Data.NameFromValueUnsafe(curPrompt.m_RawButtonIcon) );

				CTextFormat::GetInputButtons(curPrompt.m_ButtonInput, buffer, BUFFER_SIZE, NULL, 0);
			}
			else
			{
				uiAssertf( curPrompt.m_ButtonInputGroup != INPUTGROUP_INVALID,
					"Only m_ButtonInput, m_ButtonInputGroup or m_RawButtonIcon can be set at a time (with '%s', '%s', %s)!",
					CControl::GetInputName(curPrompt.m_ButtonInput),
					CControl::GetInputGroupName(curPrompt.m_ButtonInputGroup),
					parser_eInstructionButtons_Data.NameFromValueUnsafe(curPrompt.m_RawButtonIcon) );

				CTextFormat::GetInputGroupButtons(curPrompt.m_ButtonInputGroup, buffer, BUFFER_SIZE, NULL, 0);
			}

			// Search the string for all icons.
			char* start = buffer;
			while(*start != 0)
			{
				// Move on to the separator or the end.
				char* cur = start;
				while(*cur != KEY_FMT_SEPERATOR[0] && *cur != 0)
				{
					++cur;
				}

				// If its a separator then NULL it and move over.
				if(*cur == KEY_FMT_SEPERATOR[0])
				{
					*cur = 0;
					++cur;
				}
				
				// See if the icon is already added.
				atHashString hash(start);
				int error = icons.Find(hash);
				if(error != -1)
				{
					atString errorString;
					const CMenuButton& curErrorPrompt = m_ButtonPrompts[error];
					uiErrorf("CONFLICT: Encoded %s, index: %d, string: '%s', contexts: '%s'", start, error, curErrorPrompt.m_hButtonHash.TryGetCStr(), curErrorPrompt.m_ContextHashes.GetString(errorString).c_str());
					uiAssertf(0, "Instructional buttons ended up with 2+ '%s' prompts active. This probably will look bad, or not even show up at all! Check ui errors above for which ones", start);
				}
				else
				{
					icons.Push(hash);
				}

				// Move on to next icon.
				start = cur;
			}
#endif
		}
	}

#if RSG_PC
	// teeny, tiny twiddle for last input so it'll differ if we change inputs
	if( CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource() )
		displayHash += 1;
#endif

	displayHash = atFinalizeHash(displayHash);

	CScaleformMovieWrapper& instructionButtons = pOverride ? *pOverride : CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS);

	if( pOverride || CPauseMenu::GetDynamicPauseMenu()->GetLastButtonHash() != displayHash )
	{
		instructionButtons.CallMethod("CLEAR_ALL");

		for(int i=0; i < showList.GetCount(); ++i )
		{
			// items are specified in reverse order, so reverse the count.
			showList[i]->SET_DATA_SLOT(showList.GetCount() - 1 - i, instructionButtons);
		}
	
		instructionButtons.CallMethod("SET_CLEAR_SPACE", m_WrappingPoint);

		instructionButtons.CallMethod("SET_MAX_WIDTH", CHudTools::GetSafeZoneSize());

		instructionButtons.CallMethod("DRAW_INSTRUCTIONAL_BUTTONS", 0); // nobody used m_Type

		if( !pOverride)
		{
			CPauseMenu::GetDynamicPauseMenu()->SetLastIBData(showList.GetCount(), displayHash);
		}
	}
	else if( CPauseMenu::GetDynamicPauseMenu()->GetLastButtonHash() == displayHash )
	{
		uiDisplayf("Didn't refresh pause menu buttons because we'd already shown it!");
	}
}

void CMenuButtonList::Clear(CScaleformMovieWrapper* pOverride /* = NULL */)
{
	CScaleformMovieWrapper& instructionButtons = pOverride ? *pOverride : CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS);

	if (instructionButtons.IsActive())
	{
		instructionButtons.CallMethod("CLEAR_ALL");
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CMenuBase::CMenuBase(CMenuScreen& owner) : fwRefAwareBase()
	, m_Owner(owner)
	, m_hasWarning(0)
{
#if __BANK
	++sm_Count;
	sm_Allocs += sizeof(CMenuBase);
#endif

	ClearColumnVisibilities();
}

CContextMenu* CMenuBase::GetContextMenu()
{
	return GetOwner().m_pContextMenu;
}

const CContextMenu* CMenuBase::GetContextMenu() const
{
	return GetOwner().m_pContextMenu;
}

MenuScreenId CMenuBase::GetMenuScreenId() const
{
	return m_Owner.MenuScreen;
}

void CMenuBase::ShowColumnWarning(PM_COLUMNS startingColumn, int ColumnWidth, const char* bodyStringToShow, const char* titleStringToShow/*=""*/, int ColumnPxHeightOverride /*=0*/, const char* txdStr /*=""*/, const char* imgStr /*=""*/, PM_WARNING_IMG_ALIGNMENT imgAlignment/*=0*/, const char* footerStr/*=""*/)
{
	m_hasWarning = ColumnWidth;
	CScaleformMenuHelper::SHOW_WARNING_MESSAGE(startingColumn, ColumnWidth, bodyStringToShow, titleStringToShow, ColumnPxHeightOverride, txdStr, imgStr, imgAlignment, footerStr);
}

void CMenuBase::ShowColumnWarning_Loc(PM_COLUMNS startingColumn, int ColumnWidth, const char* bodyStringToShow, const char* titleStringToShow/*=""*/, int ColumnPxHeightOverride /*=0*/, const char* txdStr /*=""*/, const char* imgStr /*=""*/, PM_WARNING_IMG_ALIGNMENT imgAlignment/*=0*/, const char* footerStr/*=""*/, bool bRequestTXD/*=false*/)
{
	m_hasWarning = ColumnWidth;
	CScaleformMenuHelper::SHOW_WARNING_MESSAGE(startingColumn, ColumnWidth, TheText.Get(bodyStringToShow), TheText.Get(titleStringToShow), ColumnPxHeightOverride, txdStr, imgStr, imgAlignment, footerStr, bRequestTXD);
}

void CMenuBase::ShowColumnWarning_EX(PM_COLUMNS columnFields, const char* bodyStringToLoc, u32 uTitleStringToLoc)
{
	SHOW_WARNING_MESSAGE_EX(columnFields, bodyStringToLoc, uTitleStringToLoc);
}

void CMenuBase::ShowColumnWarning_EX(PM_COLUMNS columnFields, const char* bodyStringToLoc, const char* titleStringToLoc, int ColumnPxHeightOverride)
{

	SUIContexts::Activate( "WARNING_ACTIVE" );
	SHOW_WARNING_MESSAGE_EX(columnFields, bodyStringToLoc, titleStringToLoc, ColumnPxHeightOverride);
}

void CMenuBase::ClearWarningColumn()
{
	if( m_hasWarning )
	{
		if( SUIContexts::IsActive( "WARNING_ACTIVE" ) )
		{
			SUIContexts::Deactivate( "WARNING_ACTIVE" );
		}

	//	CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_LEFT, 1, "", "");
		CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(false);
		m_hasWarning = 0;
	}
}

void CMenuBase::ClearWarningColumn_EX(PM_COLUMNS columnFields)
{
	if( m_hasWarning )
	{
		HandleColumns(columnFields, true, m_hasWarning);
		CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(false);
		m_hasWarning = 0;
	}
}

void CMenuBase::ClearColumnVisibilities()
{
	m_columnVisibility.Reset();
}

void CMenuBase::ClearColumnDisplayed()
{
	m_columnDisplayed.Reset();
}

void CMenuBase::SHOW_COLUMN(PM_COLUMNS column, bool bVisible)
{
	if(uiVerifyf(0 <= column && (int)column < MAX_MENU_COLUMNS, "CMenuBase::SHOW_COLUMN - column=%d, MAX_MENU_COLUMNS=%d", column, MAX_MENU_COLUMNS))
	{
		if (m_columnVisibility.IsSet(column) != bVisible)
		{
			m_columnVisibility.Set(column, bVisible);
			CScaleformMenuHelper::SHOW_COLUMN(column, bVisible);
		}
	}
	else
	{
		CScaleformMenuHelper::SHOW_COLUMN(column, bVisible);
	}
}

void CMenuBase::SET_DATA_SLOT_EMPTY(PM_COLUMNS column)
{
	if(uiVerifyf(0 <= column && (int)column < MAX_MENU_COLUMNS, "CMenuBase::SHOW_COLUMN - column=%d, MAX_MENU_COLUMNS=%d", column, MAX_MENU_COLUMNS))
	{
		if (m_columnDisplayed.GetAndClear(column))
		{
			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(column);
		}
	}
	else
	{
		CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(column);
	}
}

void CMenuBase::DISPLAY_DATA_SLOT(PM_COLUMNS column)
{
	if(uiVerifyf(0 <= column && (int)column < MAX_MENU_COLUMNS, "CMenuBase::SHOW_COLUMN - column=%d, MAX_MENU_COLUMNS=%d", column, MAX_MENU_COLUMNS))
	{
		if (!m_columnDisplayed.GetAndSet(column) )
		{
			CScaleformMenuHelper::DISPLAY_DATA_SLOT(column);
		}
	}
	else
	{
		CScaleformMenuHelper::DISPLAY_DATA_SLOT(column);
	}
}

void CMenuBase::HandleColumns(PM_COLUMNS& whichColumns, bool bShowOrHide, int& width)
{
	width = 1;
	switch( whichColumns )
	{
	case PM_COLUMN_LEFT:		SHOW_COLUMN(PM_COLUMN_LEFT,   bShowOrHide);	break;
	case PM_COLUMN_MIDDLE:		SHOW_COLUMN(PM_COLUMN_MIDDLE, bShowOrHide);	break;
	case PM_COLUMN_RIGHT:		SHOW_COLUMN(PM_COLUMN_RIGHT,  bShowOrHide);	break;

	case PM_COLUMN_LEFT_MIDDLE:
		SHOW_COLUMN(PM_COLUMN_LEFT,	  bShowOrHide);
		SHOW_COLUMN(PM_COLUMN_MIDDLE, bShowOrHide);
		width = 2;
		whichColumns = PM_COLUMN_LEFT;
		break;

	case PM_COLUMN_MIDDLE_RIGHT:
		SHOW_COLUMN(PM_COLUMN_MIDDLE, bShowOrHide);
		SHOW_COLUMN(PM_COLUMN_RIGHT,  bShowOrHide);
		whichColumns = PM_COLUMN_MIDDLE;
		width = 2;
		break;

	case PM_COLUMN_MAX:			
		SHOW_COLUMN(PM_COLUMN_LEFT,	  bShowOrHide);
		SHOW_COLUMN(PM_COLUMN_MIDDLE, bShowOrHide);
		SHOW_COLUMN(PM_COLUMN_RIGHT,  bShowOrHide);
		whichColumns = PM_COLUMN_LEFT;
		width = 3;
		break;
	default:
		Assertf(0, "Don't really know how to handle that column you sent me (%i)", whichColumns);
		break;
	}
}

void CMenuBase::SHOW_WARNING_MESSAGE_EX(PM_COLUMNS columnFields, const char* bodyStringToLoc, u32 uTitleStringToLoc)
{
	HandleColumns(columnFields, false, m_hasWarning);

	u32 uBodyLoc = bodyStringToLoc ? atStringHash(bodyStringToLoc) : 0;
	const char* bodyString = uBodyLoc ? TheText.Get(bodyStringToLoc) : "";
	CScaleformMenuHelper::SHOW_WARNING_MESSAGE(columnFields, m_hasWarning, bodyString, TheText.Get(uTitleStringToLoc,""));
}

void CMenuBase::SHOW_WARNING_MESSAGE_EX(PM_COLUMNS columnFields, const char* bodyStringToLoc, const char* titleStringToLoc, int ColumnPxHeightOverride)
{
	HandleColumns(columnFields, false, m_hasWarning);

	u32 uBodyLoc = bodyStringToLoc ? atStringHash(bodyStringToLoc) : 0;
	const char* bodyString = uBodyLoc ? TheText.Get(bodyStringToLoc) : "";
	CScaleformMenuHelper::SHOW_WARNING_MESSAGE(columnFields, m_hasWarning, bodyString, TheText.Get(titleStringToLoc), ColumnPxHeightOverride);
}

void CMenuBase::HIDE_WARNING_MESSAGE_EX(PM_COLUMNS columnsAffected)
{
	int width;
	HandleColumns(columnsAffected, true, width);

	CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(false);
	m_hasWarning = 0;
}

// eof

