/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TabController.h
// PURPOSE : Quick wrapper for the common tab controller interface
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef TAB_CONTROLLER_H
#define TAB_CONTROLLER_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage 
#include "atl/array.h"

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/page_deck/uiPageEnums.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"

class CTabController final : public CPageLayoutItem
{
public:
    CTabController()
        : m_currentTab( INDEX_NONE )
        , m_wrapping( false )
    {}
    CTabController( bool const wrapping )
        : m_currentTab( INDEX_NONE )
        , m_wrapping( wrapping )
    {}
    ~CTabController() {}

    void ActivateTab( u32 const index );
    int GetActiveTabIndex() const { return m_currentTab; }
    void ClearActiveTab();

    bool SwapToNextTab();
    bool SwapToPreviousTab();

    void AddTabItem( atHashString const label );
    int GetTabCount() const { return m_tabState.GetCount(); }

    void PlayRevealAnimation( int const sequenceIndex ) final;

private: // declarations and variables
    struct TabState final
    {
        bool m_enabled;

        TabState()
            : m_enabled(true) {}
    };

    atArray< TabState > m_tabState;
    int                 m_currentTab;
    bool                m_wrapping;
    NON_COPYABLE( CTabController );

private: // methods
    bool SwapTab( int const delta );
    void SetTabStateInternal( int const tabIdx, uiPageDeck::eItemFocusState const itemState );

    void TriggerAudio( bool const tabSwapStarted, int const delta );
    static char const * const GetAudioSoundset();

    char const * GetSymbol() const final { return "tabController"; }
    void ShutdownDerived() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // TAB_CONTROLLER_H
