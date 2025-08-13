/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IPageViewHost.h
// PURPOSE : Interface for a view host, allowing access to elements shared
//           across a view.
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_PAGE_VIEW_HOST_H
#define I_PAGE_VIEW_HOST_H


#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"

// game
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"

class CUiGadgetBase;
class IPageView;

class IPageViewHost : public IPageMessageHandler
{
    FWUI_DECLARE_INTERFACE( IPageViewHost );
public: // methods

    virtual void RegisterActivePage( IPageView& pageView ) = 0;
    virtual bool IsRegistered( IPageView const& pageView ) const = 0;
    virtual void UnregisterActivePage( IPageView& pageView ) = 0;

    virtual bool CanCreateContent() const = 0;
    virtual CComplexObject& GetSceneRoot() = 0;
};

FWUI_DEFINE_INTERFACE( IPageViewHost );

#endif // UI_PAGE_DECK_ENABLED

#endif // I_PAGE_VIEW_HOST_H
