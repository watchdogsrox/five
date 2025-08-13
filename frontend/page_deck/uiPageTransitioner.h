/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageTransitioner.h
// PURPOSE : Handles moving between CPageBases and updating a stack of
//           active parentage.
// 
// AUTHOR  : james.strain
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PAGE_TRANSITIONER_H
#define UI_PAGE_TRANSITIONER_H

// game
#include "frontend/page_deck/uiPageConfig.h"
#include "frontend/page_deck/uiPageLink.h"

#if UI_PAGE_DECK_ENABLED

class CPageBase;

class uiPageTransitioner final
{
public:
    explicit uiPageTransitioner();
    ~uiPageTransitioner() { Shutdown(); }

    void AbandonTransition();
    void Shutdown();
	
    bool PushTransition( uiPageLink const& pageLink, uiPageConfig::PageMap const& pageMap );
    bool TransitionToParent();

    bool StepTransition( float const deltaMs );
    bool HasTransition() const;
    bool HasStackedPages() const { return m_pageStack.GetCount() > 0; }
    bool HasTransition( uiPageLink const& pageLink ) const;
    bool IsStacked( CPageBase const& targetPage ) const
    {
        bool const c_found = IsStacked( &targetPage );
        return c_found;
    }
    bool CanBackOut() const;

    CPageBase * GetFocusedPage() { return m_pageStack.GetCount() > 0 ? m_pageStack.Top() : nullptr; }
    CPageBase const * GetFocusedPage() const { return m_pageStack.GetCount() > 0 ? m_pageStack.Top() : nullptr; }

    void ForEachStackedPage_ExceptFocused( uiPageConfig::PageVisitorLambda action );
    void ForEachStackedPage_ExceptFocused( uiPageConfig::PageVisitorConstLambda action ) const;

private:
    struct TransitionInfo
    {
        CPageBase const * m_transitionSource;
		uiPageConfig::ConstPageCollection m_transitionSteps;
		uiPageLink  m_pageLink;
        int         m_stepsRemaining;
        bool        m_toParent;

        TransitionInfo()
            : m_transitionSource( nullptr )
            , m_stepsRemaining( 0 )
            , m_toParent( false )
        {
        }

        TransitionInfo( CPageBase const * const sourcePage, CPageBase const * const parentPage, uiPageLink const& pageLink, int const stepsRemaining )
            : m_transitionSource( sourcePage )
			, m_pageLink(pageLink)
            , m_stepsRemaining( stepsRemaining )
            , m_toParent( true )
        {
			m_transitionSteps.PushAndGrow(parentPage);
        }

        TransitionInfo( CPageBase const * const sourcePage, uiPageConfig::ConstPageCollection const& transitionSteps, uiPageLink const& pageLink )
            : m_transitionSource( sourcePage )
            , m_transitionSteps( transitionSteps )
			, m_pageLink(pageLink)
            , m_stepsRemaining( transitionSteps.GetCount() )
            , m_toParent( false )
        {
        }

        bool IsComplete() const { return m_stepsRemaining == 0; }
        bool IsTransitionToParent() const { return m_toParent; }
        bool IsFinalDestination() const { return m_stepsRemaining == 1; }
		bool IsNestedTransition() const { return m_transitionSteps.GetCount() > 1 && !IsFinalDestination(); }
        bool StepTransition()
        {
            if( m_stepsRemaining > 0 )
            {
                --m_stepsRemaining;
            }

            return IsComplete();
        }
    };

    typedef rage::atArray<TransitionInfo>   TransitionCollection;
    TransitionCollection                    m_transitionCollection;
    uiPageConfig::PageCollection            m_pageStack;
    float                                   m_transitionTimeout;

private:
    void CleanupTransitionInfo();

    CPageBase* GetSourceForTransition();

    CPageBase * GetCurrentTransitionSource()
    {
        uiPageTransitioner const * const c_constThis = const_cast<uiPageTransitioner* const>(this);
        return const_cast<CPageBase*>(c_constThis->GetCurrentTransitionSource());
    }
    CPageBase const * GetCurrentTransitionSource() const;

    CPageBase * GetCurrentTransitionTarget()
    {
        uiPageTransitioner const * const c_constThis = const_cast<uiPageTransitioner* const>(this);
        return const_cast<CPageBase*>(c_constThis->GetCurrentTransitionTarget());
    }
    CPageBase const * GetCurrentTransitionTarget() const;

	bool IsImmediateLink() const;
    bool IsInterruptibleLink() const { return true; }

    bool IsTransitionToParent() const;
    int  GetStackedIndex( CPageBase const * const page ) const;
    bool IsStacked( CPageBase const * const page ) const { return GetStackedIndex( page ) >= 0; }
    bool IsOnTopOfStack( CPageBase const * const page ) const;
    bool IsParent( CPageBase const * const potentialParent, CPageBase const * const potentialChild ) const;

    void UpdateTransitionTarget();

    void UnstackAllPages();
	bool IsPageValidForTransition(CPageBase* const * const page, uiPageId pageId);
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_PAGE_TRANSITIONER_H
