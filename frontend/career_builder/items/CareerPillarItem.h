/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerPillarItem.h
// PURPOSE : Represents a pillar item in the Career Selection view, handles 
//           interactive music changes specific to this view
//
// AUTHOR  : brian.beacom
// STARTED : September 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CAREER_PILLAR_ITEM_H
#define CAREER_PILLAR_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/CloudCardItem.h"

class CCareerPillarItem : public CCloudCardItem
{
    FWUI_DECLARE_DERIVED_TYPE(CCareerPillarItem, CCloudCardItem);

public:
    CCareerPillarItem() : superclass() {}
    ConstString& InteractiveMusicTrigger() { return m_interactiveMusicTrigger; }

private: // declarations and variables
    ConstString m_interactiveMusicTrigger;

private:
    virtual void OnFocusGainedDerived(CComplexObject& displayObject) final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CAREER_PILLAR_ITEM_H
