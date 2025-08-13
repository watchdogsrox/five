/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PillarPageView.h
// PURPOSE : Page view for showing N pillar items
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PILLAR_PAGE_VIEW_H
#define PILLAR_PAGE_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/SingleLayoutView.h"

class CDynamicLayoutContext;

class CPillarPageView final : public CSingleLayoutView
{
    FWUI_DECLARE_DERIVED_TYPE( CPillarPageView, CSingleLayoutView );
public:
    CPillarPageView();
    virtual ~CPillarPageView();

private: // declarations and variables
    CPageGridSimple         m_topLevelLayout;
    CPageGridSimple         m_contentLayout;
    CComplexObject          m_contentObject;
    PAR_PARSABLE;
    NON_COPYABLE( CPillarPageView );

private: // methods
    void RecalculateGrids();
    void GetContentArea( Vec2f_InOut pos, Vec2f_InOut size ) const;

    void PopulateDerived( IPageItemProvider& itemProvider ) final;
    void CleanupDerived() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PILLAR_PAGE_VIEW_H
