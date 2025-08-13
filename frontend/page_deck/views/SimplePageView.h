/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SimplePageView.h
// PURPOSE : Page view that does very minimal placement and navigation.
//           Intended for whiteboxing/prototyping only, so robust but rough.
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef SIMPLE_PAGE_VIEW_H
#define SIMPLE_PAGE_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/SingleLayoutView.h"

class CDynamicLayoutContext;

class CSimplePageView final : public CSingleLayoutView
{
    FWUI_DECLARE_DERIVED_TYPE( CSimplePageView, CSingleLayoutView );
public:
    CSimplePageView();
    virtual ~CSimplePageView();

private: // declarations and variables
    CPageGridSimple         m_topLevelLayout;
    CPageGridSimple         m_contentLayout;
    CComplexObject          m_contentObject;
    PAR_PARSABLE;
    NON_COPYABLE( CSimplePageView );

private: // methods
    void RecalculateGrids();
    void GetContentArea( Vec2f_InOut pos, Vec2f_InOut size ) const;

    void PopulateDerived( IPageItemProvider& itemProvider ) final;
    void CleanupDerived() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // SIMPLE_PAGE_VIEW_H
