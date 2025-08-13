/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TabController.cpp
// PURPOSE : Quick wrapper for the common tab controller interface
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "TabController.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/input/uiInputCommon.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CTabController::ActivateTab( u32 const index )
{
    if( index < GetTabCount() )
    {
        SetTabStateInternal( index, uiPageDeck::STATE_FOCUSED );
        m_currentTab = index;
    }
}

void CTabController::ClearActiveTab()
{
    if( m_currentTab >= 0 )
    {
        if( m_currentTab < GetTabCount() )
        {
            SetTabStateInternal( m_currentTab, uiPageDeck::STATE_UNFOCUSED );
        }

        m_currentTab = INDEX_NONE;
    }
}

bool CTabController::SwapToNextTab()
{ 
    return SwapTab( 1 );
}

bool CTabController::SwapToPreviousTab()
{
    return SwapTab( -1 );
}

void CTabController::AddTabItem( atHashString const label )
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        char const * const c_categoryName = TheText.Get( label );
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "ADD_TAB_ITEM" );
        CScaleformMgr::AddParamString( c_categoryName, rage::StringLength(c_categoryName), false );
        CScaleformMgr::EndMethod();

        TabState newTabState;
        m_tabState.PushAndGrow( newTabState );
    }
}

void CTabController::PlayRevealAnimation( int const UNUSED_PARAM(sequenceIndex) )
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "ANIMATE_IN" );
        CScaleformMgr::EndMethod();
    }
}

bool CTabController::SwapTab( int const delta )
{
    bool tabChanged = false;

    int const c_maxTabs = GetTabCount();
    int const c_startingTab = m_currentTab;
    int targetTab = m_currentTab + delta;
    for( ; targetTab != c_startingTab; targetTab += delta )
    {
        if( targetTab < 0 )
        {
            targetTab = m_wrapping ? c_maxTabs - 1 : 0;
        }
        if( targetTab >= c_maxTabs )
        {
            targetTab = m_wrapping ? 0 : c_maxTabs - 1;
        }

        if( targetTab >= 0 && targetTab < c_maxTabs && 
            m_tabState[targetTab].m_enabled )
        {
            break;
        }
    }

    if( targetTab != c_startingTab )
    {
        ClearActiveTab();
        ActivateTab( targetTab );
        tabChanged = true;
    }
    else if( targetTab < 0 )
    {
        ClearActiveTab();
    }

    TriggerAudio( tabChanged, delta );
    return tabChanged;
}

void CTabController::SetTabStateInternal( int const tabIdx, uiPageDeck::eItemFocusState const itemState )
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_TAB_STATE" );
        CScaleformMgr::AddParamInt( tabIdx );
        CScaleformMgr::AddParamInt( itemState );
        CScaleformMgr::AddParamBool( m_tabState[tabIdx].m_enabled );
        CScaleformMgr::EndMethod();
    }
}

void CTabController::ShutdownDerived()
{ 
	 m_tabState.Reset();
}

void CTabController::TriggerAudio(bool const tabSwapStarted, int const delta)
{
    char const * const c_audioSoundset = GetAudioSoundset();

    fwuiInput::eHandlerResult const c_asHandlerResult = tabSwapStarted ? fwuiInput::ACTION_HANDLED : fwuiInput::ACTION_IGNORED;
    char const * const c_directionalTrigger = delta > 0 ? "Change_Page_Right" : "Change_Page_Left";
    uiInput::TriggerInputAudio( c_asHandlerResult, c_audioSoundset, c_directionalTrigger, "Error" );
}

char const * const CTabController::GetAudioSoundset()
{
    // TODO_UI - We should pass this as a param and store on a local member if we 
    // need to expand this tech outside of the landing page
    return "UI_Landing_Screen_Extended_Sounds";
}

#endif // UI_PAGE_DECK_ENABLED
