/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PopulatablePageView.h
// PURPOSE : Base class for page views with default population mechanics
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef POPULATABLE_PAGE_VIEW_H
#define POPULATABLE_PAGE_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "system/noncopyable.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/PageViewBase.h"

class CPageItemBase;
class IPageItemProvider;

class CPopulatablePageView : public CPageViewBase
{
    FWUI_DECLARE_DERIVED_TYPE( CPopulatablePageView, CPageViewBase );
public:
    virtual ~CPopulatablePageView()  {}

protected:
    CPopulatablePageView()
        : superclass()
    { }

    void Populate( IPageItemProvider& itemProvider ) { PopulateDerived( itemProvider ); }
    bool IsPopulated() const { return IsPopulatedDerived(); }
    void Cleanup() { CleanupDerived(); };

    void OnEnterStartDerived() override;
    bool OnEnterUpdateDerived( float const deltaMs ) override;
    bool OnEnterTimeoutDerived() override;

    void OnExitCompleteDerived() override { Cleanup(); }

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CPopulatablePageView );

private: // methods
    void OnShutdown() final;

    virtual void PopulateDerived( IPageItemProvider& UNUSED_PARAM(itemProvider) ) { }
    virtual bool IsPopulatedDerived() const { return true; }
    virtual void CleanupDerived() { }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // POPULATABLE_PAGE_VIEW_H
