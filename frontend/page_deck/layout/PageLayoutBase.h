/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageLayoutBase.h
// PURPOSE : Base for scene layout primitives
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_LAYOUT_BASE_H
#define PAGE_LAYOUT_BASE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "parser/macros.h"
#include "system/noncopyable.h"
#include "vectormath/vec2f.h"

// framework
#include "fwtl/regdrefs.h"
#include "fwui/Foundation/fwuiTypeId.h"
#include "fwui/Input/fwuiInputEnums.h"
#include "fwui/Input/fwuiNavigationContext.h"
#include "fwui/Input/fwuiNavigationParams.h"
#include "fwui/Input/fwuiNavigationResult.h"

// game
#include "frontend/input/uiInputEnums.h"
#include "frontend/page_deck/layout/PageLayoutItemParams.h"
#include "frontend/page_deck/uiPageEnums.h"
#include "frontend/Scaleform/ScaleformComplexObjectMgr.h"

class CPageItemBase;
class CPageLayoutItemBase;
class IPageMessageHandler;

class CPageLayoutBase : public fwRefAwareBase
{
    FWUI_DECLARE_BASE_TYPE( CPageLayoutBase );

public:
    virtual ~CPageLayoutBase() { UnregisterAll(); }

    void Reset() 
    { 
        ResetDerived(); 
    }

    void RegisterItem( CPageLayoutItemBase& item, CPageLayoutItemParams const& params );
    bool IsItemRegistered( CPageLayoutItemBase const& item ) const;
    void UnregisterItem( CPageLayoutItemBase const& item );
    void UnregisterAll();

    void RecalculateLayout( Vec2f_In position, Vec2f_In extents );
    void NotifyOfLayoutUpdate();

    CPageLayoutItemParams GenerateParams( CPageItemBase const& pageItem ) const;
    char const * const GetSymbol( CPageItemBase const& pageItem ) const;

    void SetDefaultFocus();
    void InvalidateFocus();
    void RefreshFocusVisuals();
    void ClearFocusVisuals();
    char const * GetFocusTooltip() const;
    void UpdateInstructionalButtons( atHashString const acceptLabelOverride = atHashString() ) const;

    void PlayEnterAnimation();

    fwuiInput::eHandlerResult HandleDigitalNavigation( int const deltaX, int const deltaY );

    // Arguably a "layout" class shouldn't handle actioning input. Navigation makes sense because it's
    // spatial, but actioning is just focus. I opted for this approach as it avoids leaking out implementation
    // details such as grabbing out the focused item
    fwuiInput::eHandlerResult HandleInputAction( eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler );

    Vec2f GetPosition() const { return m_position; }
    Vec2f GetExtents() const { return GetExtentsDerived(); }

#if RSG_BANK
    void DebugRender() const;
#endif 
protected: // declarations and variables
    typedef LambdaRef< void( CPageLayoutItemBase& )>          PerLayoutItemLambda;
    typedef LambdaRef< void( CPageLayoutItemBase const& )>    PerLayoutItemConstLambda;

protected:
    CPageLayoutBase()
        : m_position( Vec2f::ZERO )
    {}

    CPageLayoutItemBase* GetItemByIndex( int const index );
    CPageLayoutItemBase const * GetItemByIndex( int const index ) const;
    CPageLayoutItemParams const * GetItemParamsByIndex( int const index ) const;
    int GetItemCount() const { return m_registeredItems.GetCount(); }
    int GetIndex( CPageLayoutItemBase const & item ) const;

    int GetClosestIntersectingItemIdx( Vec2f_In p0, Vec2f_In p1, Vec2f_In p2, int const indexToSkip ) const;

    void GetItemPositionAndSize( int const index, Vec2f_InOut out_pos, Vec2f_InOut out_size ) const;
    virtual char const * const GetSymbolDerived( CPageItemBase const& pageItem ) const;

    void ForEachLayoutItemDerived( PerLayoutItemLambda action );
    void ForEachLayoutItemDerived( PerLayoutItemConstLambda action ) const;

    virtual void PlayEnterAnimationDerived();

private: // declarations and variables
    atArray<CPageLayoutItemBase*>   m_registeredItems;
    atArray<CPageLayoutItemParams>  m_registeredItemParams;
    Vec2f                           m_position;
    rage::fwuiNavigationContext     m_navigationContext;

    PAR_PARSABLE;
    NON_COPYABLE( CPageLayoutBase );

private: // methods

    virtual int GetDefaultItemFocusIndex() const;
    virtual void GetItemPositionAndSizeDerived( int const index, Vec2f_InOut out_pos, Vec2f_InOut out_size ) const = 0;
    rage::fwuiNavigationResult GetNavDetailsForItem( int const index ) const;

    virtual void ResetDerived() = 0;
    virtual Vec2f GetExtentsDerived() const = 0;
    virtual void RecalculateLayoutInternal( Vec2f_In extents ) = 0;
    virtual void PositionItem( CPageLayoutItemBase& item, CPageLayoutItemParams const& params ) const = 0;
    virtual CPageLayoutItemParams GenerateParamsDerived( CPageItemBase const& pageItem ) const;

#if RSG_BANK
    virtual void DebugRenderDerived() const {}
#endif 

    static void OnNavigatedToItem( CPageLayoutItemBase* const oldItem, CPageLayoutItemBase * const newItem );
    static fwuiInput::eHandlerResult OnActionItem( CPageLayoutItemBase& item, eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler );
    virtual rage::fwuiNavigationResult HandleDigitalNavigationDerived( rage::fwuiNavigationParams const& navParams );

    int HandleDigitalNavigationSimple( int const delta, bool const isWrapping, int const existingIndex );
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_LAYOUT_BASE_H
