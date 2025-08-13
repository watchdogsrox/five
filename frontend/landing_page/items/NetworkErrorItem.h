/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : NetworkErrorItem.h
// PURPOSE : Used to represent network errors (Not signed in, no cable, etc...)
//
// AUTHOR  : james.strain
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef NETWORK_ERROR_ITEM_H
#define NETWORK_ERROR_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/ParallaxCardItem.h"

class CNetworkErrorItem : public CParallaxCardItem
{
    FWUI_DECLARE_DERIVED_TYPE(CNetworkErrorItem, CParallaxCardItem);

    enum eCardErrorType
    {
        DEFAULT,
        NOT_SIGNED_IN,
        NO_LINK_CONNECTION,
        NOT_SIGNED_ONLINE,
        NO_PERMISSIONS,
        NO_SCS_PRIVILEGE,
		ROS_BANNED,
		ROS_SUSPENDED,

        ERROR_TYPE_COUNT
    };

public: // Declarations and variables
    struct SErrorDetails
    {
        atHashString m_title;
        atHashString m_description;
        atHashString m_tooltip;
        PAR_SIMPLE_PARSABLE;
    };

public:
    CNetworkErrorItem();
    virtual ~CNetworkErrorItem() {}

    bool UpdateErrorType();

    char const * GetItemTitleText() const final;
    char const * GetDescriptionText() const final;
    char const * GetTooltipText() const final;

    // TODO - Return true for not signed in
    bool IsEnabled() const final { return false; }

private: // declarations and variables

    SErrorDetails m_defaultError;
    SErrorDetails m_signInError;
    SErrorDetails m_connectionLinkError;
    SErrorDetails m_signOnlineError;
    SErrorDetails m_scsPrivilegeError;
	SErrorDetails m_permissionsError;
	SErrorDetails m_rosBannedError;
	SErrorDetails m_rosSuspendedError;

    eCardErrorType m_currentErrorType;

    PAR_PARSABLE;
    NON_COPYABLE( CNetworkErrorItem );

private:
    SErrorDetails const& GetCurrentErrorDetail() const;
    void UpdateVisualsDerived( CComplexObject& displayObject ) final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // NETWORK_ERROR_ITEM_H
