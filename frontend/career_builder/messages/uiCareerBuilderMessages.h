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

#ifndef UI_CAREER_BUILDER_MESSAGES
#define UI_CAREER_BUILDER_MESSAGES

#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED

// rage
#include "parser/macros.h"

// game
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/page_deck/uiPageLink.h"
#include "Peds/CriminalCareer/CriminalCareerDefs.h"

class uiSelectCareerMessage : public uiPageDeckMessageBase
{
	FWUI_DECLARE_MESSAGE_DERIVED(uiSelectCareerMessage, uiPageDeckMessageBase);
public:
	uiSelectCareerMessage() {}
	explicit uiSelectCareerMessage(const CriminalCareerDefs::CareerIdentifier& careerId, uiPageLink const& linkInfo) :
		m_careerIdentifier(careerId),
		m_pageLink(linkInfo)
	{
	}
	virtual ~uiSelectCareerMessage() {}

	const CriminalCareerDefs::CareerIdentifier& GetCareerIdentifier() const { return m_careerIdentifier; }
	uiPageLink const& GetLinkInfo() const { return m_pageLink; }

private:
	CriminalCareerDefs::CareerIdentifier m_careerIdentifier;
	uiPageLink m_pageLink;
};

class uiSelectCareerItemMessage : public uiPageDeckMessageBase
{
	FWUI_DECLARE_MESSAGE_DERIVED(uiSelectCareerItemMessage, uiPageDeckMessageBase);
public:
	uiSelectCareerItemMessage() {}
	explicit uiSelectCareerItemMessage(const CriminalCareerDefs::ItemIdentifier& itemId) :
		m_itemIdentifier(itemId)
	{
	}
	virtual ~uiSelectCareerItemMessage() {}

	const CriminalCareerDefs::ItemIdentifier& GetItemIdentifier() const { return m_itemIdentifier; }

private:
	CriminalCareerDefs::ItemIdentifier m_itemIdentifier;
};

class uiSelectMpCharacterSlotMessage : public uiPageDeckMessageBase
{
	FWUI_DECLARE_MESSAGE_DERIVED(uiSelectMpCharacterSlotMessage, uiPageDeckMessageBase);
public:
	uiSelectMpCharacterSlotMessage() {}
	explicit uiSelectMpCharacterSlotMessage(const int chosenMpCharacterSlot, uiPageLink const& linkInfo) :
		m_chosenMpCharacterSlot(chosenMpCharacterSlot),
		m_pageLink(linkInfo)
	{
	}
	virtual ~uiSelectMpCharacterSlotMessage() {}

	const int GetChosenMpCharacterSlot() const { return m_chosenMpCharacterSlot; }
	uiPageLink const& GetLinkInfo() const { return m_pageLink; }

private:
	int m_chosenMpCharacterSlot;
	uiPageLink m_pageLink;
};


#endif // UI_PAGE_DECK_ENABLED

#endif // UI_CAREER_BUILDER_MESSAGES
