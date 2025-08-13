/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageBase.h
// PURPOSE : Base class for pages in our page deck system, roughly speaking it
//           represents a view-model portion. Contains basic params
//           shared across all pages.
//
// NOTE    : It also carries a view object, as for data org sake it seemed simpler
//           to define the view we want alongside the page. However since the
//           page is effectively just view-model info, this isn't really an issue.
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_BASE_H
#define PAGE_BASE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "system/lambda.h"
#include "system/noncopyable.h"

// framework
#include "fwui/Foundation/fwuiTypeId.h"

// game
#include "frontend/page_deck/IPageItemProvider.h"
#include "frontend/page_deck/uiPageId.h"

class CPageViewBase;
class IPageMessageHandler;
class IPageViewHost;

class CPageBase : public IPageItemProvider
{
    FWUI_DECLARE_BASE_TYPE( CPageBase );
public:
    virtual ~CPageBase();

    void Initialize( uiPageId const id, IPageViewHost& viewHost );
    bool IsInitialized() const { return m_messageHandler != nullptr; }
    void Shutdown();

    uiPageId GetId() const { return m_id; }
    char const * GetTitleString() const final;

    void EnterPage( bool const immediate );
    void OnEnterTimedOut();
    void GainFocus( bool const focusFromEnter );
    void Update( float const deltaMs );
    void LoseFocus();
    void ExitPage( bool const immediate );

    bool ShouldEnter() const { return m_statePhase == STATE_REQUIRES_ENTER; }
    bool IsEntering() const { return m_statePhase == STATE_ENTERING; }
    bool IsEnteringComplete() const { return m_statePhase == STATE_ACTIVE; }
    bool IsActive() const { return m_statePhase == STATE_ACTIVE; }
    bool IsFocused() const { return m_focused; }
    bool ShouldExit() const { return IsEntering() || IsActive(); }
    bool CanExit() const { return m_statePhase == STATE_ACTIVE; }
    bool IsExiting() const { return m_statePhase == STATE_EXITING; }
    bool IsExitComplete() const { return m_statePhase == STATE_REQUIRES_ENTER; }

    virtual float GetTransitionTimeout() const;

    bool IsTransitoryPage() const;

protected:
    CPageBase();
    atHashString GetTitle() const { return m_title; }
	void SetTitle(atHashString title) { m_title = title; }

    IPageMessageHandler& GetMessageHandler() { return *m_messageHandler; }
    IPageMessageHandler const& GetMessageHandler() const { return *m_messageHandler; }

private: // declarations and variables
    enum eStatePhase
    {
        STATE_REQUIRES_ENTER,
        STATE_ENTERING,
        STATE_ACTIVE,
        STATE_EXITING
    };

    IPageMessageHandler* m_messageHandler;
    CPageViewBase * m_view;
    uiPageId m_id;
    atHashString m_title;
    eStatePhase	 m_statePhase;
    bool m_focused;
    PAR_PARSABLE;
    NON_COPYABLE( CPageBase );

private: // methods

    void UpdateStatePhase( float const deltaMs, bool const immediate );

    void OnEnterStart();
    virtual void OnEnterStartDerived() {};
    bool OnEnterUpdate( float const deltaMs );
    virtual bool OnEnterUpdateDerived( float const UNUSED_PARAM( deltaMs ) ) { return true; };
    void OnEnterComplete();
    virtual void OnEnterCompleteDerived() {};

    void OnFocusGained( bool const focusFromEnter );
    virtual void OnFocusGainedDerived(){};
    void UpdateInternal( float const deltaMs );
    virtual void UpdateDerived( float const UNUSED_PARAM(deltaMs) ) {}; 
    void OnFocusLost();
    virtual void OnFocusLostDerived() {};

    void OnExitStart();
    virtual void OnExitStartDerived() {};
    bool OnExitUpdate( float const deltaMs );
    virtual bool OnExitUpdateDerived( float const UNUSED_PARAM(deltaMs) ) { return true; };
    void OnExitComplete();
    virtual void OnExitCompleteDerived() {};

    virtual bool IsTransitoryPageDerived() const = 0;
};

// Helper class for any page types that lack items and categories
class CCategorylessPageBase : public CPageBase
{
    typedef CPageBase superclass;
public:
    virtual ~CCategorylessPageBase() {};
protected:
    CCategorylessPageBase() : superclass() {};

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CCategorylessPageBase );

private: // methods
    size_t GetCategoryCount() const final { return 0; }
    void ForEachCategoryDerived( superclass::PerCategoryLambda UNUSED_PARAM(action) ) final {}
    void ForEachCategoryDerived( superclass::PerCategoryConstLambda UNUSED_PARAM(action) ) const final {}
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_BASE_H
