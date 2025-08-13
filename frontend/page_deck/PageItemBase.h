/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageItemBase.h
// PURPOSE : Base for items that can be represented on a page_deck menu
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_ITEM_BASE_H
#define PAGE_ITEM_BASE_H


#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "system/noncopyable.h"

// framework
#include "fwui/Foundation/fwuiInstanceId.h"
#include "fwui/Foundation/fwuiTypeId.h"

// game
#include "frontend/page_deck/items/IItemStatsInterface.h"
#include "frontend/page_deck/items/IItemStickerInterface.h"
#include "frontend/page_deck/items/IItemTextureInterface.h"
#include "frontend/page_deck/uiPageEnums.h"

class CComplexObject;
class IPageMessageHandler;
class uiPageDeckMessageBase;

class CPageItemBase
{
    FWUI_DECLARE_BASE_TYPE( CPageItemBase );
public:
    virtual ~CPageItemBase();

    virtual char const * GetItemTitleText() const;
    virtual char const * GetPageTitleText() const { return GetItemTitleText(); }
    virtual char const * GetTooltipText() const;	
    virtual char const * GetDescriptionText() const { return ""; }
    virtual char const * GetSymbol() const { return nullptr; }

    void OnReveal( CComplexObject& displayObject, int const sequenceIndex );
    void OnHoverGained( CComplexObject& displayObject );
    void OnHoverLost( CComplexObject& displayObject );
    void OnFocusGained( CComplexObject& displayObject );
    void OnFocusLost( CComplexObject& displayObject );
    bool OnSelected( CComplexObject& displayObject, IPageMessageHandler& messageHandler );

    void SetDisplayObjectContent( CComplexObject& displayObject );
    void UpdateVisuals( CComplexObject& displayObject );

    void SetOnSelectMessage( uiPageDeckMessageBase*& message );
    void DestroyOnSelectMessage();

    IItemStatsInterface const* GetStatsInterface() const
    {
        CPageItemBase * const nonConstThis = const_cast<CPageItemBase*>(this);
        return nonConstThis->GetStatsInterfaceDerived();
    }

    IItemStickerInterface const* GetStickerInterface() const
    {
        CPageItemBase * const nonConstThis = const_cast<CPageItemBase*>(this);
        return nonConstThis->GetStickerInterfaceDerived();
    }

    IItemTextureInterface* GetTextureInterface() const 
    { 
        CPageItemBase * const nonConstThis = const_cast<CPageItemBase*>(this);
        return nonConstThis->GetTextureInterfaceDerived(); 
    }

    virtual bool IsEnabled() const { return m_onSelectMessage != nullptr; }

	void OnPageEvent(uiPageDeckMessageBase& msg);

protected:
    CPageItemBase()
        : FWUI_INSTANCE_ID_CONSTRUCTOR_DEFAULT()
        , m_onSelectMessage( nullptr )
    {
    }

    atHashString GetItemTitle() const 
    { 
        return m_title.IsNotNull() ? m_title : getId(); 
    }
    virtual atHashString GetPageTitle() const { return GetItemTitle(); }
    virtual atHashString GetTooltip() const { return m_tooltip; }

private: // declarations and variables
    FWUI_DECLARE_INSTANCE_ID_PROTECTED();

    uiPageDeckMessageBase*              m_onSelectMessage;
    atHashString                        m_title;
    atHashString                        m_tooltip;
    PAR_PARSABLE;
    NON_COPYABLE( CPageItemBase );

private: // methods
    virtual void OnRevealDerived( CComplexObject& displayObject, int const sequenceIndex ) = 0;
    virtual void OnHoverGainedDerived( CComplexObject& displayObject ) = 0;
    virtual void OnHoverLostDerived( CComplexObject& displayObject ) = 0;
    virtual void OnFocusGainedDerived( CComplexObject& displayObject ) = 0;
    virtual void OnFocusLostDerived( CComplexObject& displayObject ) = 0;
    virtual void OnSelectedDerived( CComplexObject& displayObject ) = 0;
    virtual void SetDisplayObjectContentDerived( CComplexObject& displayObject ) = 0;
    virtual void UpdateVisualsDerived( CComplexObject& displayObject ) = 0;

	virtual void OnPageEventDerived(uiPageDeckMessageBase& UNUSED_PARAM(msg)) {};

	virtual IItemStatsInterface* GetStatsInterfaceDerived() { return nullptr; }
    virtual IItemStickerInterface* GetStickerInterfaceDerived() { return nullptr; }
    virtual IItemTextureInterface* GetTextureInterfaceDerived() { return nullptr; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_ITEM_BASE_H
