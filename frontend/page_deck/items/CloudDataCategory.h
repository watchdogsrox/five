/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CloudDataCategory.h
// PURPOSE : An item category made at least partially of downloaded data
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CLOUD_DATA_CATEGORY_H
#define CLOUD_DATA_CATEGORY_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "net/time.h"

// game
#include "frontend/page_deck/items/StaticDataCategory.h"

class CPageItemBase;
class CSocialClubFeedTilesRow;

class CCloudDataCategory : public CPageItemCategoryBase
{
    typedef CPageItemCategoryBase superclass;

public:
    CCloudDataCategory();
    virtual ~CCloudDataCategory();

    RequestStatus RequestItemData(RequestOptions const options) final;
    RequestStatus UpdateRequest() final;
    RequestStatus MarkAsTimedOut() final;
    RequestStatus GetRequestStatus() const final;

protected:
    void AddCloudItem(CPageItemBase* item);
    void ClearCloudData();
    void Failed();
    void Succeeded();

    bool HasCloudItems() const { return m_cloudItems.GetCount() > 0; }

    virtual bool FillData(CSocialClubFeedTilesRow& row);
    virtual bool LoadRichData(CSocialClubFeedTilesRow& row);
    virtual void OnFailed();
    virtual void OnSucceeded();

    void ForEachItemDerived(superclass::PerItemLambda action) final;
    void ForEachItemDerived(superclass::PerItemConstLambda action) const final;

protected: // declarations and variables
    typedef rage::atArray<CPageItemBase*>   ItemCollection;
    typedef ItemCollection::iterator        ItemIterator;
    typedef ItemCollection::const_iterator  ConstItemIterator;

private:
    ItemCollection  m_cloudItems;

    RequestOptions m_requestedOptions;
    RequestStatus m_requestStatus;
    netTimeout m_metaDataTimeout;
    bool m_AwaitingTicket;

    PAR_PARSABLE;
    atHashString m_cloudSource;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CLOUD_DATA_CATEGORY_H
