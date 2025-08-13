/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageDeckMessages.h
// PURPOSE : Contains the core messages used by the page deck system. Primary
//           intent is for view > controller communications
//
// NOTES   : If you have a specialized enough set of messages, don't dump them here.
//           Be a responsible developer and create a new h/cpp/psc in the correct domain
// 
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PAGE_DECK_MESSAGES_H
#define UI_PAGE_DECK_MESSAGES_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "parser/macros.h"
#include "system/noncopyable.h"

// framework
#include "fwui/Foundation/Messaging/fwuiMessageBase.h"

// game
#include "frontend/page_deck/uiPageLink.h"

class uiPageDeckMessageBase : public rage::fwuiMessageBase
{
    FWUI_DECLARE_MESSAGE_DERIVED( uiPageDeckMessageBase, fwuiMessageBase );
public:
    virtual ~uiPageDeckMessageBase() { }

    // The id of the button or action that generated the message
    atHashString GetTrackingId() const { return m_trackingId; }

    void SetTrackingId(atHashString trackingId) { m_trackingId = trackingId; }

protected:
    uiPageDeckMessageBase() { }

private:
    atHashString m_trackingId;
    PAR_PARSABLE;
    NON_COPYABLE(uiPageDeckMessageBase);
};

class uiGoToPageMessage : public uiPageDeckMessageBase
{
    FWUI_DECLARE_MESSAGE_DERIVED( uiGoToPageMessage, uiPageDeckMessageBase );
public:
    uiGoToPageMessage() {}
    explicit uiGoToPageMessage( uiPageLink const& linkInfo ) : m_pageLink( linkInfo ) {}
    virtual ~uiGoToPageMessage() {}

    uiPageLink const& GetLinkInfo() const { return m_pageLink; }

private:
    uiPageLink m_pageLink;
    PAR_PARSABLE;
};

class uiPageDeckAcceptMessage final : public uiGoToPageMessage
{
    FWUI_DECLARE_MESSAGE_DERIVED( uiPageDeckAcceptMessage, uiGoToPageMessage );
public:
    uiPageDeckAcceptMessage();
    virtual ~uiPageDeckAcceptMessage() {}

private:
    PAR_PARSABLE;
};

class uiPageDeckCanBackOutMessage final : public uiPageDeckMessageBase
{
    FWUI_DECLARE_MESSAGE_DERIVED( uiPageDeckCanBackOutMessage, uiPageDeckMessageBase );

public:
    uiPageDeckCanBackOutMessage()
        : m_result( false )
    { }
    virtual ~uiPageDeckCanBackOutMessage() {}

    bool GetResult() const { return m_result; }
    void SetResult( bool const result ) { m_result = result; }
private:
    bool m_result;

};
class uiPageDeckBackMessage final : public uiGoToPageMessage
{
    FWUI_DECLARE_MESSAGE_DERIVED( uiPageDeckBackMessage, uiGoToPageMessage );

public:
    uiPageDeckBackMessage();
    virtual ~uiPageDeckBackMessage() {}

private:
    PAR_PARSABLE;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_PAGE_DECK_MESSAGES_H
