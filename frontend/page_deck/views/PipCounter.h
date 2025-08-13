/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PipCounter.h
// PURPOSE : Quick wrapper for the common pip counter interface for showing a
//           N of M counter
//
// AUTHOR  : james.strain
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PIP_COUNTER_H
#define UI_PIP_COUNTER_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CPipCounter final : public CPageLayoutItem
{
public:
    CPipCounter()
        : m_pipCount( 0 )
        , m_currentPip( INDEX_NONE )
    {}
    ~CPipCounter() {}

    void SetCount( int const itemCount );
    void ClearPips();

    void SetCurrentItem( int const index );
    void IncrementCurrentItem();
    void DecrementCurrentItem();

    // Matches Scaleform counterpart
    float GetPipSize() const { return 8.f; }

    bool IsOnFirstPip() const { return m_pipCount > 0 && m_currentPip == 0; }
    bool IsOnLastPip() const { return m_pipCount > 0 && m_currentPip == m_pipCount - 1; }

private: // declarations and variables
    int m_pipCount;
    int m_currentPip;
    NON_COPYABLE( CPipCounter );

private: // methods
    char const * GetSymbol() const final { return "paginationCounter"; }

    void SetCountInternal( int const itemCount );
    void ClearPipsInternal();
    void SetState( int const index, bool const focused );

    // Magic Number is calculated as
    // 15px for pip + spacing
    // 15px * 32 = 480px
    // Fits in an 800x600 frame for PC, but still large enough to be visually stupid
    int GetMaxPipCount() const{ return 32; }

    void ResolveLayoutDerived( Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx ) final;
    void ShutdownDerived() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_PIP_COUNTER_H
