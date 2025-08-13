#include "WaitingForProcessPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "WaitingForProcessPage_parser.h"

// game
#include "frontend/SocialClubMenu.h"
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE(CWaitingForProcessPage, 0xD5167C43);

CWaitingForProcessPage::CWaitingForProcessPage() :
	superclass()
{
}

void CWaitingForProcessPage::UpdateDerived(float const UNUSED_PARAM(deltaMs))
{
	if (IsFocused())
	{
		const bool c_isComplete = HasProcessCompleted();
		if (c_isComplete)
		{
			IPageMessageHandler& messageHandler = GetMessageHandler();
			if (m_onCompleteAction != nullptr)
			{
				messageHandler.HandleMessage(*m_onCompleteAction);
			}
			else
			{
				uiPageDeckBackMessage backMsg;
				IPageMessageHandler& messageHandler = GetMessageHandler();
				messageHandler.HandleMessage(backMsg);
			}
		}
	}
}

bool CWaitingForProcessPage::HasProcessCompleted() const
{
	const bool c_isComplete = HasProcessCompletedDerived();
	return c_isComplete;
}

#endif // GEN9_LANDING_PAGE_ENABLED
