/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageViewHost.h
// PURPOSE : Host class for all visual pages. Has initial setup logic for 
//           scaleform and debug views
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_VIEW_HOST_H
#define PAGE_VIEW_HOST_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "system/noncopyable.h"

// game
#include "debug/UiGadget/UiGadgetBase.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"

class CPageViewHost : public IPageViewHost
{
protected:
    CPageViewHost() {}
    virtual ~CPageViewHost() {}

    void Update();
    void UpdateInstructionalButtons();
    void Render();
    
protected:
    void InitializeSceneRoot( CComplexObject& sceneRoot );
    bool IsSceneRootInitialized() const;
    void ShutdownSceneRoot();

private: // declarations and variables
    typedef atArray<IPageView*> PageViewCollection;
    PageViewCollection          m_activePages;
    CComplexObject  m_sceneRoot;

    NON_COPYABLE( CPageViewHost );

private: // methods
    virtual void UpdateDerived() {};
    virtual void UpdateInstructionalButtonsDerived() {}

    void RegisterActivePage( IPageView& pageView ) final;
    bool IsRegistered( IPageView const& pageView ) const;
    void UnregisterActivePage( IPageView& pageView ) final;
    bool CanCreateContent() const final { return IsSceneRootInitialized(); }
    CComplexObject& GetSceneRoot() final { return m_sceneRoot; }

#if RSG_BANK
    void DebugRender();
#endif // RSG_BANK
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_VIEW_HOST_H
