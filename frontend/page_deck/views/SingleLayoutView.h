/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SingleLayoutView.h
// PURPOSE : Base view that is designed for views that use a single content layout
//
// AUTHOR  : james.strain
// STARTED : April 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef SINGLE_LAYOUT_VIEW_H
#define SINGLE_LAYOUT_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/PageTitle.h"
#include "frontend/page_deck/views/PopulatablePageView.h"

class CDynamicLayoutContext;

class CSingleLayoutView : public CPopulatablePageView
{
    FWUI_DECLARE_DERIVED_TYPE( CSingleLayoutView, CPopulatablePageView );
public:
    CSingleLayoutView();
    virtual ~CSingleLayoutView();

protected:
    void InitializeTitle( CComplexObject& parentObject, CPageLayoutBase& layout, CPageLayoutItemParams const& params );
    void ShutdownTitle();

    void SetTitleLiteral( char const * const titleText );
    void ClearTitle();

    void InitializeLayoutContext( IPageItemProvider& itemProvider, CComplexObject& visualRoot, CPageLayoutBase& layoutRoot );
    void ShutdownLayoutContext();

    void CleanupDerived() override;

private: // declarations and variables
    CPageTitle              m_pageTitle;
    CDynamicLayoutContext*  m_dynamicLayoutContext;
    PAR_PARSABLE;
    NON_COPYABLE( CSingleLayoutView );

private: // methods
    void UpdateDerived( float const deltaMs ) final;

    void OnFocusGainedDerived() final;
    void OnFocusLostDerived() final;

    bool IsPopulatedDerived() const final;

    void UpdateInputDerived() final;
    void UpdateInstructionalButtonsDerived() final;

    void UpdatePromptsAndTooltip();
};

#endif // UI_PAGE_DECK_ENABLED

#endif // SINGLE_LAYOUT_VIEW_H
