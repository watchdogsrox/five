/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerPillarItem.cpp
// PURPOSE : Represents a pillar item in the Career Selection view, handles 
//           interactive music changes specific to this view
//
// AUTHOR  : brian.beacom
// STARTED : September 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "CareerPillarItem.h"
#if UI_PAGE_DECK_ENABLED
// framework
#include "fwutil/xmacro.h"

// game
#include "audio/music/musicplayer.h"

FWUI_DEFINE_TYPE( CCareerPillarItem, 0xAE2FDDCC);


void CCareerPillarItem::OnFocusGainedDerived(CComplexObject& displayObject)
{
    superclass::OnFocusGainedDerived(displayObject);
#if IS_GEN9_PLATFORM
    if (m_interactiveMusicTrigger)
    {
        g_InteractiveMusicManager.GetEventManager().TriggerEvent(m_interactiveMusicTrigger.c_str());
    }
#endif
}

#endif // UI_PAGE_DECK_ENABLED
