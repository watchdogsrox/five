#include "frontend/ScaleformMenuHelper.h"

#include "frontend/PauseMenu.h"
#include "text/TextFile.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/ui_channel.h"

#if __ASSERT
#include "atl/bitset.h"

rage::atFixedBitSet16 s_ClearedFields;
PARAM(sf_assertOnDisplayDataSlot, "Will assert if scaleform is told to display data slot without clearing it first");
#endif

void CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMNS column, s32 iMenuIndex, bool bSetUniqueId /* = false */)
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if(pauseContent.BeginMethod( "SET_COLUMN_HIGHLIGHT" ))
	{
		pauseContent.AddParam(static_cast<int>(column));
		pauseContent.AddParam(iMenuIndex);
		pauseContent.AddParam(0); // ignored
		pauseContent.AddParam(bSetUniqueId);
		pauseContent.EndMethod();
	}
}

bool CScaleformMenuHelper::SET_DATA_SLOT_NO_LABEL(PM_COLUMNS column, int iMenuIndex, int iMenuId, int iUniqueId, eOPTION_DISPLAY_STYLE eMenuType, int iInitialIndex, int iActive, bool bCallEndMethod /* = true */, bool bIsUpdate /* = false */)
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if( pauseContent.BeginMethod(bIsUpdate ? "UPDATE_SLOT" : "SET_DATA_SLOT") )
	{
		pauseContent.AddParam(static_cast<int>(column));  // The View the slot is to be added to
		pauseContent.AddParam(iMenuIndex);  // The numbered slot the information is to be added to
		pauseContent.AddParam(iMenuId);  // The menu ID
		pauseContent.AddParam(iUniqueId);  // The unique ID
		pauseContent.AddParam(eMenuType);  // The Menu item type (see below)
		pauseContent.AddParam(iInitialIndex);  // The initial index of the slot (0 default, can be 0,1,2...x in a multiple selection)
		pauseContent.AddParam(iActive);  // active or inactive
		
		if( bCallEndMethod )
			pauseContent.EndMethod();

		return true;
	}

	return false;
}

bool CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMNS column, int iMenuIndex, int iMenuId, int iUniqueId, eOPTION_DISPLAY_STYLE eMenuType, int iInitialIndex, int iActive, const char* pszLabel, bool bCallEndMethod /* = true */, bool bIsUpdate /* = false */, bool bParseString /* = true */)
{
	if( SET_DATA_SLOT_NO_LABEL(column, iMenuIndex, iMenuId, iUniqueId, eMenuType, iInitialIndex, iActive, false, bIsUpdate) )
	{
		CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
		pauseContent.AddParamString(pszLabel, bParseString);  // The text label
		if( bCallEndMethod )
			pauseContent.EndMethod();

		return true;
	}
	return false;
}

void CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PM_COLUMNS column )
{
	ASSERT_ONLY(s_ClearedFields.Set(column));

	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if(pauseContent.BeginMethod( "SET_DATA_SLOT_EMPTY" ))
	{
		pauseContent.AddParam(static_cast<int>(column));
		pauseContent.EndMethod();
	}
}


bool CScaleformMenuHelper::SET_COLUMN_TITLE(PM_COLUMNS column, const char* pParam, bool bCallEndMethod /* = true */)
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if( pauseContent.BeginMethod( "SET_COLUMN_TITLE") )
	{
		pauseContent.AddParam(static_cast<int>(column));  // The View the slot is to be added to
		pauseContent.AddParam(pParam);
		if( bCallEndMethod )
			pauseContent.EndMethod();
		return true;
	}

	return false;
}

bool CScaleformMenuHelper::DISPLAY_DATA_SLOT(PM_COLUMNS column)
{
	uiAssertf(!PARAM_sf_assertOnDisplayDataSlot.Get() || s_ClearedFields.GetAndClear(column), "Column %i was not SET_DATA_SLOT_EMPTY'd before setting it! THIS PROBABLY MEANS YOU CALLED IT TWICE. Which is (probably) very bad and (probably) wastes memory", column);
	return CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "DISPLAY_DATA_SLOT", static_cast<int>(column) );
}

void CScaleformMenuHelper::CLEAR_DATA_SLOT(PM_COLUMNS column)
{
	CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "CLEAR_DATA_SLOT", static_cast<int>(column) );
}

void CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMNS column, bool isFocused, bool bNavigable, bool bRemoveOldFocus)
{
	CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_COLUMN_FOCUS", static_cast<int>(column), isFocused, bNavigable, bRemoveOldFocus);
}

void CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMNS column, bool bBusy)
{
	CPauseMenu::SetBusySpinner(bBusy, static_cast<s8>(column));
}

void CScaleformMenuHelper::SHOW_COLUMN(PM_COLUMNS column, bool bVisible)
{
	CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SHOW_COLUMN", column, bVisible);
}

void CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMNS column, int iPosition, int iMaxPosition, int iMaxVisible /* = -1 */)
{
	CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_COLUMN_SCROLL", column, iPosition, iMaxPosition, iMaxVisible);
}

void CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMNS column, const char* pszCaption)
{
	CScaleformMovieWrapper& wrapper = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if( wrapper.BeginMethod( "SET_COLUMN_SCROLL" ) )
	{
		wrapper.AddParam(column);
		wrapper.AddParam(0); // position (ignored)
		wrapper.AddParam(0); // max position (ignored)
		wrapper.AddParam(0); // max visible (also ignored)
		wrapper.AddParamString(pszCaption);
		wrapper.EndMethod();
	}
}

void CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMNS column)
{
	// according to the flash guy, this hides it.
	SET_COLUMN_SCROLL(column, -1, -1, -1);
}

void CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMNS startingColumn, int ColumnWidth, const char* bodyString, const char* titleString,int ColumnPxHeightOverride /*=0*/, const char* txdStr /*=""*/, const char* imgStr /*=""*/, PM_WARNING_IMG_ALIGNMENT imgAlignment/*=IMG_ALIGN_NONE*/, const char* footerStr/*=""*/, bool bRequestTXD/*=false*/)
{
	CScaleformMovieWrapper& wrapper = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if(wrapper.BeginMethod("SHOW_WARNING_MESSAGE"))
	{
		wrapper.AddParam(true);
		wrapper.AddParam(startingColumn);
		wrapper.AddParam(ColumnWidth);
		wrapper.AddParam(CScaleformMovieWrapper::Param(bodyString, false));
		wrapper.AddParam(titleString);
		wrapper.AddParam(ColumnPxHeightOverride > 0 ? ColumnPxHeightOverride : 430);
		wrapper.AddParam(txdStr);
		wrapper.AddParam(imgStr);
		wrapper.AddParam(imgAlignment);
		wrapper.AddParam(footerStr);
		wrapper.AddParam(bRequestTXD);
		wrapper.EndMethod();
	}
};

void CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(bool isVisible)
{
	CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SHOW_WARNING_MESSAGE", isVisible);
};

// eof
