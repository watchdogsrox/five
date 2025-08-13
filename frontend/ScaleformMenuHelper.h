#ifndef INC_SCALEFORM_MENU_HELPER_H_
#define INC_SCALEFORM_MENU_HELPER_H_

#include "text/TextFile.h"

enum eOPTION_DISPLAY_STYLE
{
	OPTION_DISPLAY_NONE = 0,
	OPTION_DISPLAY_STYLE_TEXTFIELD,
	OPTION_DISPLAY_STYLE_SLIDER,
	MAX_OPTION_DISPLAY_STYLES
};

enum PM_COLUMNS
{
	PM_COLUMN_LEFT,
	PM_COLUMN_MIDDLE,
	PM_COLUMN_RIGHT,
	PM_COLUMN_EXTRA3,	// some pages have extra columns
	PM_COLUMN_EXTRA4,	// some pages have extra columns

	PM_COLUMN_LEFT_MIDDLE,
	PM_COLUMN_MIDDLE_RIGHT,

	PM_COLUMN_MAX,
};

// Match with PAUSE_MENU_SP_CONTENT.as
enum PM_WARNING_IMG_ALIGNMENT
{
	IMG_ALIGN_NONE,
	IMG_ALIGN_TOP,
	IMG_ALIGN_RIGHT
};

class CScaleformMenuHelper
{
public:
	// kept in all caps to match SF stuff for easy searching
	
	static void SET_COLUMN_HIGHLIGHT(PM_COLUMNS column, s32 iMenuIndex, bool bSetUniqueId = false);
	static void SET_DATA_SLOT_EMPTY(PM_COLUMNS column);
	static bool SET_DATA_SLOT(		   PM_COLUMNS column, int iMenuIndex, int iMenuId, int iUniqueId, eOPTION_DISPLAY_STYLE eMenuType, int iInitialIndex, int iActive, const char* pszLabel, bool bCallEndMethod = true, bool bIsUpdate = false, bool bParseString = true);
	static bool SET_DATA_SLOT_NO_LABEL(PM_COLUMNS column, int iMenuIndex, int iMenuId, int iUniqueId, eOPTION_DISPLAY_STYLE eMenuType, int iInitialIndex, int iActive,							bool bCallEndMethod = true, bool bIsUpdate = false);
	static bool DISPLAY_DATA_SLOT(PM_COLUMNS column);
	static void CLEAR_DATA_SLOT(PM_COLUMNS column);
	static bool SET_COLUMN_TITLE(PM_COLUMNS column, const char* pParam, bool bCallEndMethod = true );
	static void SET_COLUMN_FOCUS(PM_COLUMNS column, bool isFocused, bool bNavigable = false, bool bRemoveOldFocus = true);
	static void SHOW_COLUMN(PM_COLUMNS column, bool bVisible);

	// DEPRECATED. Use CPauseMenu::SetBusySpinner instead
	static void SET_BUSY_SPINNER(PM_COLUMNS column, bool bBusy);

	static void SET_COLUMN_SCROLL(PM_COLUMNS column, int iPosition, int iMaxPosition, int iMaxVisible = -1);
	static void SET_COLUMN_SCROLL(PM_COLUMNS column, const char* pszCaption);
	static void HIDE_COLUMN_SCROLL(PM_COLUMNS column);

	static void SHOW_WARNING_MESSAGE(PM_COLUMNS startingColumn, int ColumnWidth, const char* bodyString,  const char* titleString, int ColumnPxHeight = 0, const char* txdStr ="", const char* imgStr ="", PM_WARNING_IMG_ALIGNMENT imgAlignment = IMG_ALIGN_NONE, const char* footerStr = "", bool bRequestTXD = false);
	static void SET_WARNING_MESSAGE_VISIBILITY(bool isVisible); // 

private:
};


#endif // INC_SCALEFORM_MENU_HELPER_H_

// eof
