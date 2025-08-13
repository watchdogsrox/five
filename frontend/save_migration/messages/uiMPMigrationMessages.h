/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiMPMigrationMessages.h
// PURPOSE : Contains the  messages used by MP Migration
// NOTES   : Not expecting more than uiStartMPMigrationMessage
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef UI_MP_MIGRATION_MESSAGES
#define UI_MP_MIGRATION_MESSAGES

#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED

// rage
#include "parser/macros.h"

// game
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/page_deck/uiPageLink.h"


class uiStartMPMigrationMessage : public uiGoToPageMessage
{
	FWUI_DECLARE_MESSAGE_DERIVED(uiStartMPMigrationMessage, uiGoToPageMessage);
public:
	uiStartMPMigrationMessage() {}
	explicit uiStartMPMigrationMessage(uiPageLink const& linkInfo)
		: uiGoToPageMessage(linkInfo)
	{
	}
	virtual ~uiStartMPMigrationMessage() {}


private:
	PAR_PARSABLE;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_MP_MIGRATION_MESSAGES
