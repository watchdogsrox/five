/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IPageView.h
// PURPOSE : Interface for a specific view
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_PAGE_VIEW_H
#define I_PAGE_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "data/base.h"

// framework
#include "fwui/Interfaces/Interface.h"

class IPageView : public rage::datBase /* TODO_UI - Exists for RAG callbacks, not needed once Scaleform UI up and running fully */
{
    FWUI_DECLARE_INTERFACE( IPageView );
public: // methods

    bool HasCustomRender() const { return HasCustomRenderDerived(); }
    void Render() const { RenderDerived(); }
    void UpdateInstructionalButtons() 
    { 
        if( IsViewFocused() )
        {
            UpdateInstructionalButtonsDerived(); 
        }
    }

#if RSG_BANK
    void DebugRender() const { DebugRenderDerived(); }
#endif 
    
private:
    virtual bool IsViewFocused() const { return false; }
    virtual bool HasCustomRenderDerived() const { return false; }
    virtual void RenderDerived() const {};
    virtual void UpdateInstructionalButtonsDerived() {}

#if RSG_BANK
    virtual void DebugRenderDerived() const {}
#endif 
};

FWUI_DEFINE_INTERFACE( IPageView );

#endif // UI_PAGE_DECK_ENABLED

#endif // I_PAGE_VIEW_H
