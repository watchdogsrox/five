/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageLayoutItem.h
// PURPOSE : Base class for an item that can be laid out by our small layout engine
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_LAYOUT_ITEM_H
#define PAGE_LAYOUT_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "system/noncopyable.h"
#include "vectormath/vec2f.h"

// framework
#include "fwui/Input/fwuiInputEnums.h"
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleformComplexObjectMgr.h"
#include "frontend/ui_channel.h"

class IPageMessageHandler;

// Base interface, which just requires a way to resolve a layout
class CPageLayoutItemBase
{
public:
    CPageLayoutItemBase() {}
    virtual ~CPageLayoutItemBase() {}

    // We pass a local and screen-space as the Scaleform scene graph will already compound our positions,
    // but our layout resolve does not
    void ResolveLayout( Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx )
    {
        ResolveLayoutDerived( screenspacePosPx, localPosPx, sizePx );
    }

    // Interactive items can be focused/hovered/selected
    virtual bool IsInteractive() const { return false; }

    // Enabled/Disabled is independent of interactive, as we can focus a disabled item 
    // to be told why it is disabled. We may also run alternate logic on selection of a disabled item
    virtual bool IsEnabled() const { return true; }

    virtual void OnHoverGained() {}
    virtual void OnHoverLost() {}
    virtual void OnFocusGained() {}
    virtual void OnFocusLost() {}

    virtual void PlayRevealAnimation( int const UNUSED_PARAM(sequenceIndex) ) {}

    virtual fwuiInput::eHandlerResult OnSelected( IPageMessageHandler& UNUSED_PARAM(messageHandler) ) { return fwuiInput::ACTION_NOT_HANDLED; }
    virtual char const * GetTooltop() const { return nullptr; }

#if RSG_BANK
    void DebugRender() const { DebugRenderDerived(); }
#endif 

private: // declarations and variables
    NON_COPYABLE( CPageLayoutItemBase );

private: // methods
    virtual void ResolveLayoutDerived( Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx ) = 0;

#if RSG_BANK
    virtual void DebugRenderDerived() const {}
#endif 
};

// A more appropriate base for most items, which includes a complex object for visualization
class CPageLayoutItem : public CPageLayoutItemBase
{
public:
    CPageLayoutItem() {}
    virtual ~CPageLayoutItem() 
    { 
        uiFatalAssertf( !IsInitialized(), "Still initialized at destruction. This is bad, as we can no longer call polymorphic shutdown" );
        m_displayObject.Release();
    }

    void Initialize( CComplexObject& parentObject );
    bool IsInitialized() const { return m_displayObject.IsActive(); }
    void Shutdown();

    void PlayRevealAnimation( int const sequenceIndex ) override;

protected:
    CComplexObject& GetDisplayObject() { return m_displayObject; }
    CComplexObject const& GetDisplayObject() const { return m_displayObject; }

    void ResolveLayoutDerived( Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx ) override;

private: // declarations and variables
    CComplexObject m_displayObject;
    NON_COPYABLE( CPageLayoutItem );

private: // methods
    virtual void InitializeDerived() {}
    virtual void ShutdownDerived() {}
    virtual char const * GetSymbol() const { return nullptr; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_LAYOUT_ITEM_H
