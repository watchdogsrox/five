/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PipCounter.cpp
// PURPOSE : Quick wrapper for the common pip counter interface for showing a
//           N of M counter
//
// AUTHOR  : james.strain
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PipCounter.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CPipCounter::SetCount(int const itemCount)
{
    if( m_pipCount != itemCount )
    {

        if( itemCount > 0 )
        {
            int const c_maxPips = GetMaxPipCount();
            uiAssertf( itemCount < c_maxPips, "Requesting %d/%d pips, clamping range", itemCount, c_maxPips );

            m_pipCount = rage::Min( itemCount, c_maxPips );
            SetCountInternal( m_pipCount );
            SetState( 0, false );
        }
        else
        {
            ClearPips();
        }
    }
}

void CPipCounter::ClearPips()
{
    if( m_pipCount > 0 )
    {
        m_pipCount = 0;
		m_currentPip = INDEX_NONE;
        ClearPipsInternal();
    }
}

void CPipCounter::SetCurrentItem(int const index)
{
    if( m_pipCount > 0 )
    {
        if( m_currentPip != index )
        {
            if( !uiVerifyf( index >= 0, "%d/%d is out of range", index, m_pipCount ) ||
                !uiVerifyf( index < m_pipCount, "%d/%d is out of range", index, m_pipCount ) )
            {
                SetCurrentItem( 0 );
            }

            m_currentPip = index;
            SetState( m_currentPip, true );
        }
    }
}

void CPipCounter::IncrementCurrentItem()
{
    if( m_pipCount > 0 )
    {
        if( m_currentPip < 0 )
        {
            SetCurrentItem( 0 );
        }

        ++m_currentPip;
        m_currentPip = m_currentPip >= m_pipCount ? 0 : m_currentPip;
        SetState( m_currentPip, true );
    }
}

void CPipCounter::DecrementCurrentItem()
{
    if( m_pipCount > 0 )
    {
        if( m_currentPip < 0 )
        {
            SetCurrentItem( 0 );
        }

        --m_currentPip;
        m_currentPip = m_currentPip < 0 ? m_pipCount - 1 : m_currentPip;
        SetState( m_currentPip, true );
    }
}

void CPipCounter::SetCountInternal( int const itemCount )
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_NUMBER_OF_PIPS" );
        CScaleformMgr::AddParamInt( itemCount );
        CScaleformMgr::EndMethod();

        bool const c_visible = itemCount > 1;
        displayObject.SetVisible( c_visible );
    }
}

void CPipCounter::ClearPipsInternal()
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "CLEAR_PIPS" );
        CScaleformMgr::EndMethod();
        displayObject.SetVisible( false );
    }
}

void CPipCounter::SetState( int const index, bool const focused )
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        int const c_scaleformState = focused ? 1 : 0;
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_PIP_STATE" );
        CScaleformMgr::AddParamInt( index );
        CScaleformMgr::AddParamInt( c_scaleformState );
        CScaleformMgr::AddParamBool( true ); // Unused, but required
        CScaleformMgr::EndMethod();
    }
}

void CPipCounter::ResolveLayoutDerived(Vec2f_In UNUSED_PARAM(screenspacePosPx), Vec2f_In localPosPx, Vec2f_In sizePx)
{
    CComplexObject& displayObject = GetDisplayObject();

    // The Scaleform counterpart is self centering, so we need to center based on the parent container size
    float const c_hSizePx = sizePx.GetX() / 2.f;
    float const c_xCentered =  c_hSizePx + localPosPx.GetX();
    Vec2f const c_position( c_xCentered, localPosPx.GetY() );
    displayObject.SetPositionInPixels( c_position );
}

void CPipCounter::ShutdownDerived()
{
    ClearPips();
}

#endif // UI_PAGE_DECK_ENABLED
