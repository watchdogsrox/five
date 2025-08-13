/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageViewBase.h
// PURPOSE : Base class for page views, which take page contents and visualize them
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_VIEW_BASE_H
#define PAGE_VIEW_BASE_H

#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "system/noncopyable.h"

// framework
#include "fwui/Foundation/fwuiTypeId.h"
#include "fwui/Input/fwuiInputEnums.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageView.h"
#include "frontend/page_deck/uiPageHotkey.h"
#include "frontend/page_deck/views/Tooltip.h"

class CPageItemBase;
class CPageLayoutBase;
class CPageLayoutItemParams;
class IPageItemProvider;
class IPageViewHost;

class CPageViewBase : public IPageView
{
    typedef IPageView superclass;
    FWUI_DECLARE_BASE_TYPE( CPageViewBase );
public:
    enum eBackoutMode
    {
        BACKOUT_DEFAULT,
        BACKOUT_WITH_WARNING,
        BACKOUT_BLOCKED
    };

public:
    virtual ~CPageViewBase() 
    {
        // Do not delete m_itemCollection, we do not own it
        uiFatalAssertf( m_itemProvider == nullptr, "Item Provider still set on destruction, this view was not exited correctly" );
#if RSG_BANK
        m_viewHost = nullptr;
        m_itemProvider = nullptr;
#endif
    }

    void Initialize( IPageViewHost& viewHost );
    bool IsInitialized() const { return m_viewHost != nullptr; }
    void Shutdown();

    void OnEnterStart( IPageItemProvider& itemProvider );
    bool OnEnterUpdate( float const deltaMs );
    void OnEnterComplete();
    void OnEnterTimeout();

    void OnFocusGained( bool const focusFromEnter );
    void Update( float const deltaMs );
    void UpdateInput();
    void UpdateVisuals();
    void OnFocusLost();

    void OnExitStart();
    bool OnExitUpdate( float const deltaMs );
    void OnExitComplete( IPageItemProvider& itemCollection );

protected:
    CPageViewBase();

    IPageItemProvider& GetItemProvider() { uiFatalAssertf( m_itemProvider, "Item Provider not valid yet" ); return *m_itemProvider; }
    IPageItemProvider const& GetItemProvider() const { uiFatalAssertf( m_itemProvider, "Item Provider not valid yet" ); return *m_itemProvider; }
    IPageItemProvider* TryGetItemProvider() { return m_itemProvider; }
    IPageItemProvider const* TryGetItemProvider() const { return m_itemProvider; }

    IPageViewHost& GetViewHost() { uiFatalAssertf( m_viewHost, "View host not valid yet" ); return *m_viewHost; }
    IPageViewHost const& GetViewHost() const { uiFatalAssertf( m_viewHost, "View host not valid yet" ); return *m_viewHost; }

    bool IsViewFocused() const final { return m_focused; }

    void TriggerInputAudio( fwuiInput::eHandlerResult const actionResult, char const * const actionedTrigger, char const * const ignoredTrigger ) const;
    static char const * const GetAudioSoundset();

    void UpdateInstructionalButtonsDerived() override;

    void InitializeTooltip( CComplexObject& parentObject, CPageLayoutBase& layout, CPageLayoutItemParams const& params );
    void ShutdownTooltip();

    void SetTooltipLiteral( char const * const tooltipText, bool isRichText );
    void RefreshPrompts();
    void ClearPromptsAndTooltip();
    void ClearTooltip();

    CComplexObject& GetPageRoot() { return m_pageRoot; }
    CComplexObject const& GetPageRoot() const { return m_pageRoot; }

    atHashString GetAcceptPromptLabelOverride() const{ return m_acceptLabel; }

    bool AreAllItemTexturesAvailable() const;

    virtual bool OnEnterUpdateDerived( float const deltaMs );
    virtual bool OnExitUpdateDerived( float const deltaMs );
    virtual bool OnEnterTimeoutDerived();
    bool IsTransitionTimeoutExpired() const { return m_timeoutMs > 5000.f; }

private: // declarations and variables
    typedef LambdaRef< void( uiPageHotkey& )> HotkeyLambda;
    typedef LambdaRef< void( uiPageHotkey const& )> HotkeyConstLambda;
    atArray<uiPageHotkey*>  m_hotkeyMapping;
    CComplexObject          m_pageRoot;     // If we want fades or such, we need all our content under one root
    CTooltip                m_tooltip;      // Most pages desire a tool-tip, so simplest to bake it into the base
    IPageViewHost *         m_viewHost;
    IPageItemProvider *     m_itemProvider; // Transient - only valid for duration we are entered through to exited. 
    atHashString            m_backOutLabel;
    atHashString            m_backoutWarningLabel;
    atHashString            m_acceptLabel;
    float                   m_timeoutMs;
    eBackoutMode            m_backoutMode;
    bool                    m_focused;
    bool                    m_pendingBackoutWarning;
    PAR_PARSABLE;
    NON_COPYABLE( CPageViewBase );

private: // methods
    bool IsBackoutAllowed() const { return m_backoutMode != BACKOUT_BLOCKED; }

    char const * GetAcceptPromptText() const;
    char const * GetBackoutPromptText() const;
    bool HasValidState() const { return m_itemProvider != nullptr; }
    virtual void UpdateInputDerived() {}
    virtual void UpdateVisualsDerived() {};

    virtual void OnEnterStartDerived() {};
    virtual void OnEnterCompleteDerived() {};

    virtual void PlayEnterAnimation() {};

    virtual void OnFocusLostDerived() {};
    virtual void OnFocusGainedDerived(){};
    virtual void UpdateDerived( float const UNUSED_PARAM(deltaMs) ) {}; 

    virtual void OnExitStartDerived() {};
    virtual void OnExitCompleteDerived() {};

    void ForEachHotkey( HotkeyLambda action );
    void ForEachHotkey( HotkeyConstLambda action ) const;
    void CleanupHotkeys();

    virtual void OnShutdown() {};
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_VIEW_BASE_H
