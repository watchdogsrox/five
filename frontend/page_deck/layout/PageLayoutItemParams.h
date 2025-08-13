/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageLayoutItemParams.h
// PURPOSE : Params that are used to affect content layout
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_LAYOUT_ITEM_PARAMS_H
#define PAGE_LAYOUT_ITEM_PARAMS_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"

class CPageLayoutItemParams final
{
public:
    CPageLayoutItemParams()
        : m_x( 0 )
        , m_y( 0 )
        , m_horizontalSpan( 1 )
        , m_verticalSpan( 1 )
    {}
    CPageLayoutItemParams( int const x, int const y, int const hSpan, int const vSpan )
        : m_x( x )
        , m_y( y )
        , m_horizontalSpan( hSpan )
        , m_verticalSpan( vSpan )
    {}
    ~CPageLayoutItemParams() {}

    int GetX() const { return m_x; }
    int GetY() const { return m_y; }
    int GetHorizontalSpan() const { return m_horizontalSpan; }
    int GetVerticalSpan() const { return m_verticalSpan; }

private: // declarations and variables
    int m_x;
    int m_y;
    int m_horizontalSpan;
    int m_verticalSpan;
    CComplexObject m_displayObject;

private: // methods
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_LAYOUT_ITEM_PARAMS_H
