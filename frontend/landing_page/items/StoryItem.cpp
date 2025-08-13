/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StoryItem.cpp
// PURPOSE : Card specialization for entering Story mode
//
// AUTHOR  : james.strain
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "StoryItem.h"
#if UI_PAGE_DECK_ENABLED
#include "StoryItem_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/landing_page/messages/uiLandingPageMessages.h"

FWUI_DEFINE_TYPE( CStoryItem, 0xA392A0F6 );

char const * CStoryItem::GetItemTitleText() const 
{
    char const * result = nullptr;

    bool const c_storyAvailable = IsStoryModeAvailable();
    if( c_storyAvailable )
    {
        result = superclass::GetItemTitleText();
    }
    else
    {
        result = m_storyUpsellTitle.IsNotNull() ? TheText.Get( m_storyUpsellTitle ) : nullptr;
    }

    return result;
}

char const * CStoryItem::GetDescriptionText() const 
{
    char const * result = nullptr;

    bool const c_storyAvailable = IsStoryModeAvailable();
    if( c_storyAvailable )
    {
        result = superclass::GetDescriptionText();
    }
    else
    {
        result = m_storyUpsellDescription.IsNotNull() ? TheText.Get( m_storyUpsellDescription ) : nullptr;
    }

    return result;
}

bool CStoryItem::IsStoryModeAvailable()
{
    bool const c_result = CLandingPageArbiter::IsStoryModeAvailable();
    return c_result;
}

void CStoryItem::ForceUpdateVisuals()
{
    MarkDetailsSet( false );
}

void CStoryItem::OnPageEventDerived(uiPageDeckMessageBase& msg)
{	
	FWUI_MESSAGE_CALL_REF(uiRefreshStoryItemMsg, msg, ForceUpdateVisuals);
}

#endif // UI_PAGE_DECK_ENABLED
