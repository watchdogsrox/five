/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetFullscreenMenu.h
// PURPOSE : Acts as a fullscreen container for displaying a simple menu
//
// AUTHOR  : james.strain
// STARTED : 16/10/2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_GADGET_FULLSCREEN_MENU_H_
#define UI_GADGET_FULLSCREEN_MENU_H_

#if __BANK

// rage
#include "data/base.h"

// game
#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiColorScheme.h"
#include "debug/UiGadget/UiGadgetText.h"

class CUiGadgetFullscreenMenu : public CUiGadgetDummy
{
    typedef CUiGadgetDummy superclass;

public:
    CUiGadgetFullscreenMenu();
	CUiGadgetFullscreenMenu( CUiColorScheme const& colourScheme );
	virtual ~CUiGadgetFullscreenMenu();

    void AddMenuItem( char const * const label, datCallback const& callback );
    void RemoveAllMenuItems();

	void SetTitle( char const * const label ) 
    { 
        m_title.SetString( label ); 
        UpdateLayout();
    }
    
	virtual fwBox2D GetBounds() const { return m_background.GetBounds(); }

private: // declarations and variables
    CUiGadgetDummy              m_menuItemContainer;
	CUiGadgetBox				m_background;
	CUiGadgetText				m_title;

private: // methods
    void Initialize();

    void UpdateLayout()
    {
        UpdateTitleLayout();
        UpdateMenuItemLayout();
    }

    void UpdateTitleLayout();
    void UpdateMenuItemLayout();
    void UpdateMenuItemLayout_Small( int const childCount, Vector2 const& startPos, Vector2 const& contentArea );
    void UpdateMenuItemLayout_Large( int const childCount, Vector2 const& startPos, Vector2 const& contentArea );

    void CleanupMenuItems();
};

#endif //__BANK

#endif //UI_GADGET_FULLSCREEN_MENU_H_
