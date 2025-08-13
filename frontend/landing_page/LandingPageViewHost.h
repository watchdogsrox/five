/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageViewHost.h
// PURPOSE : View portion of the landing page
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LANDING_PAGE_VIEW_HOST_H
#define LANDING_PAGE_VIEW_HOST_H

// rage
#include "system/noncopyable.h"

// game
#include "frontend/landing_page/LandingPageConfig.h"
#include "frontend/page_deck/PageViewHost.h"
#include "frontend/Scaleform/ScaleFormMgr.h"

#if GEN9_LANDING_PAGE_ENABLED

class IPageMessageHandler;

class CLandingPageViewHost : public CPageViewHost
{
    typedef CPageViewHost superclass;
public:

    void LoadPersistentAssets();
    bool ArePersistentAssetsReady() const;
    void UnloadPersistentAssets();

private: // declarations and variables
    friend class CLandingPage; // private creation
    IPageMessageHandler*        m_messageHandler;
    CScaleformMovieWrapper      m_movie;
    strRequest                  m_txdRequest;

private: // methods
    CLandingPageViewHost() 
        : superclass()
        , m_messageHandler(nullptr)
    {}
    virtual ~CLandingPageViewHost() {}

    void Initialize( IPageMessageHandler& messageHandler );
    bool IsInitialized() const { return m_messageHandler != nullptr; }
    void Shutdown();
     
    bool HandleMessage( uiPageDeckMessageBase& msg ) final;
    void UpdateDerived() final;

    NON_COPYABLE( CLandingPageViewHost );
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // LANDING_PAGE_VIEW_HOST_H
