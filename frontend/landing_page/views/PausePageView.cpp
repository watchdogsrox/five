/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PausePageView.cpp
// PURPOSE : Page view that renders the GTAV pause menu.
//
// AUTHOR  : james.strain
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "PausePageView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "PausePageView_parser.h"

// framework
#include "fwui/Common/fwuiScaling.h"
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/NewHud.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/PauseMenu.h"

FWUI_DEFINE_TYPE( CPausePageView, 0x52623DFA );

#define PAUSE_BG_TXD atHashString( "FRONTEND_LANDING_BASE", 0x2C19FF35 )
#define PAUSE_BG_TEXTURE "SETTINGS_BACKGROUND"

void CPausePageView::RenderDerived() const
{
    if( CPauseMenu::IsActive() )
    {
        // TODO - Temp until we have a fade between landing and settings
        GRCDEVICE.Clear(true, CRGBA(0,0,0,255), false, 0.0f, 0);

        if( m_background.GetTexture() && m_background.GetShader() )
        {
            grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
            grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
            grcStateBlock::SetBlendState(m_blendStateHandle);

            m_background.SetRenderState();

            Vec2f const c_position( 0.f, 0.f );
            Vec2f const c_dipExtents = uiPageConfig::GetUIAreaDip();
            Vec2f const c_physicalExtents = uiPageConfig::GetPhysicalArea(); 
            Vec2f const c_screenExtents = fwuiScaling::CalculateDimensions( fwuiScaling::UI_SCALE_CLIP, c_dipExtents, c_physicalExtents );
            
            Vector2 v[4], uv[4];
            v[0].Set(c_position.GetX(),c_position.GetY()+c_screenExtents.GetY());
            v[1].Set(c_position.GetX(),c_position.GetY());
            v[2].Set(c_position.GetX()+c_screenExtents.GetX(),c_position.GetY()+c_screenExtents.GetY());
            v[3].Set(c_position.GetX()+c_screenExtents.GetX(),c_position.GetY());

            // This was borrowed from Editor.cpp for drawing full-screen backgrounds for the editor.
            // Unsure why the small UV offset was used there and original dev is long gone,
            // and I don't have time for extensive testing for why this was added.
#define __UV_OFFSET (0.002f)
            uv[0].Set(__UV_OFFSET,1.0f-__UV_OFFSET);
            uv[1].Set(__UV_OFFSET,__UV_OFFSET);
            uv[2].Set(1.0f-__UV_OFFSET,1.0f-__UV_OFFSET);
            uv[3].Set(1.0f-__UV_OFFSET,__UV_OFFSET);

            m_background.Draw(v[0], v[1], v[2], v[3], uv[0], uv[1], uv[2], uv[3], CRGBA(255,255,255,255));

            grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
            grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
            grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
            CSprite2d::ClearRenderState();
            CShaderLib::SetGlobalEmissiveScale(1.0f, true);
        }

        PauseMenuRenderDataExtra newData;
        CPauseMenu::SetRenderData(newData);

        CPauseMenu::RenderBackgroundContent(newData);
        CPauseMenu::RenderForegroundContent(newData);

        // SafeZone rendering
        if(CNewHud::ShouldDrawSafeZone())
        {
            CNewHud::DrawSafeZone();
        }
    }
}

void CPausePageView::OnEnterStartDerived()
{
    InitSprites();
}

void CPausePageView::OnExitStartDerived()
{
    CleanupSprites();
}

void CPausePageView::OnShutdown()
{
    CleanupSprites();
}

void CPausePageView::InitSprites()
{
    grcBlendStateDesc bsd;

    //	The remaining BlendStates should all have these two flags set
    bsd.BlendRTDesc[0].BlendEnable = true;
    bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
    bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
    bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
    bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_SRCALPHA;
    bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
    bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
    bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
    m_blendStateHandle = grcStateBlock::CreateBlendState(bsd);

    strLocalIndex const c_txdIndex = g_TxdStore.FindSlot(PAUSE_BG_TXD);
    if (uiVerifyf( c_txdIndex.IsValid(), "Invalid or missing slot " HASHFMT, HASHOUT(PAUSE_BG_TXD)) )
    {
        g_TxdStore.AddRef(c_txdIndex, REF_OTHER);
        g_TxdStore.PushCurrentTxd();
        g_TxdStore.SetCurrentTxd(c_txdIndex);
        m_background.SetTexture( PAUSE_BG_TEXTURE );
        g_TxdStore.PopCurrentTxd();
    }
}

void CPausePageView::CleanupSprites()
{
    m_background.Delete();

    strLocalIndex const c_txdIndex = g_TxdStore.FindSlot(PAUSE_BG_TXD);
    if (uiVerifyf( c_txdIndex.IsValid(), "Invalid or missing slot " HASHFMT, HASHOUT(PAUSE_BG_TXD)) )
    {
        g_TxdStore.RemoveRef( c_txdIndex, REF_OTHER);
    }

    grcStateBlock::DestroyBlendState(m_blendStateHandle);
    m_blendStateHandle = grcStateBlock::BS_Invalid;
}

#endif //  GEN9_LANDING_PAGE_ENABLED
