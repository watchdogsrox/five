/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageConfig.cpp
// PURPOSE : Config header for the page-deck system. Contains some common defines
//           and helper functions.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

#include "renderer/rendertargets.h"

namespace uiPageConfig
{
    // Force 16:9 scaling on PC for now
#define USE_16_9_SCALING (!RSG_PC || 1)

    rage::Vec2f GetUIAreaDip()
    {
        // All Scaleform movies are authored at this resolution
        return Vec2f( float(1280.f), float(720.f) );
    }

    rage::Vec2f GetScreenArea()
    {
#if USE_16_9_SCALING
        // On console we only support 16:9, so we turn on default Scaleform scaling
        return GetUIAreaDip();
#else // USE_16_9_SCALING
        return GetPhysicalArea();        
#endif // USE_16_9_SCALING
    }

    rage::Vec2f GetPhysicalArea()
    {
        int const c_width = VideoResManager::GetUIWidth();
        int const c_height = VideoResManager::GetUIHeight();

        return Vec2f( float(c_width), float(c_height) );  
    }

    GFxMovieView::ScaleModeType GetScaleModeType()
    {
#if USE_16_9_SCALING
        // On console we only support 16:9, so we turn on default Scaleform scaling
        return GFxMovieView::SM_ShowAll;
#else // USE_16_9_SCALING
        return GFxMovieView::SM_NoScale;
#endif // USE_16_9_SCALING
    }

    rage::Vec2f GetDipScale()
    {
        Vec2f const c_screenArea = GetScreenArea();
        Vec2f const c_uiDipArea = GetUIAreaDip();

        return c_screenArea / c_uiDipArea;
    }

    char const * GetLabel( atHashString const defaultLabel, atHashString const overrideLabel )
    {
        char const * result = nullptr;
        if( overrideLabel.IsNotNull() )
        {
            result = TheText.Get( overrideLabel );
        }

        if( result == nullptr )
        {
            result = TheText.Get( defaultLabel );
        }

        return result;
    }

}

#endif // UI_PAGE_DECK_ENABLED
