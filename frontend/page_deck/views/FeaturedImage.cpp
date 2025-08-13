/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FeaturedImage.cpp
// PURPOSE : Quick wrapper for the common featured image interface
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "FeaturedImage.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/page_deck/items/CardItem.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

bool CFeaturedImage::FeatureTextureInfo::UpdateUsableState()
{
    bool const c_usable = uiCloudSupport::IsTextureLoaded( m_texture, m_txd );
    m_usable = c_usable;
    return m_usable;
}

void CFeaturedImage::Set( CPageItemBase const * const pageItem )
{
    Reset();

    if( pageItem )
    {
        m_textureInterface = pageItem->GetTextureInterface();
        if( m_textureInterface )
        {
            auto textureFunc = [this]( u32 const index, char const * const txd, char const * const texture )
            {
                // NOTE - Currently I request all here, but if we ever support cloud items with multiple images we likely
                // want to request as we switch image and only request the starting image here
                bool const c_usable = m_textureInterface->RequestFeatureTexture( index );
                Add( txd, texture, c_usable );
            };

            m_textureInterface->ForEachFeatureTexture( textureFunc );

            int const c_count = GetFeatureTextureCount();
            m_pipCounter.SetCount( c_count );
            m_pipCounter.SetCurrentItem( m_currentImageIdx );
        }
    }
}

void CFeaturedImage::Reset()
{
    if( m_textureInterface )
    {
        auto textureFunc = [this]( u32 const index, char const * const UNUSED_PARAM(txd), char const * const UNUSED_PARAM(texture) )
        {
            m_textureInterface->ReleaseFeatureTexture( index );
        };
        m_textureInterface->ForEachFeatureTexture( textureFunc );
    }

    m_textureInterface = nullptr;
    m_featureTextureCollection.ResetCount();
	m_pipCounter.ClearPips();
    m_currentImageIdx = INDEX_NONE;

    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        displayObject.SetVisible( false );
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_IMAGE" );
        CScaleformMgr::AddParamString( "", false );
        CScaleformMgr::AddParamString( "", false );
        CScaleformMgr::EndMethod();
    }
}

void CFeaturedImage::UpdateInstructionalButtons() const
{
    int const c_imageCount = GetFeatureTextureCount();
    if( c_imageCount > 1 )
    {
        char const * const c_label = TheText.Get( ATSTRINGHASH( "IB_IMAGES", 0x44815CD9 ) );
        CVideoEditorInstructionalButtons::AddButton( INPUT_FRONTEND_RIGHT_AXIS_X, c_label, true );
    }
}

void CFeaturedImage::UpdateVisuals()
{
    if (m_currentImageIdx >= 0 && m_currentImageIdx < m_featureTextureCollection.GetCount())
    {
        FeatureTextureInfo& featureInfo = m_featureTextureCollection[m_currentImageIdx];
        featureInfo.UpdateUsableState();

        if (featureInfo.m_usable != featureInfo.m_displayed)
        {
            ApplyCurrentImage();
        }
    }
}

bool CFeaturedImage::UpdateInput()
{
    bool imageChanged = false;

    int const c_imageCount = GetFeatureTextureCount();
    if( c_imageCount > 1 )
    {
        int imageDelta = 0;

        if( CPauseMenu::CheckInput( FRONTEND_INPUT_RLEFT ) )
        {
            imageDelta = -1;
        }
        else if( CPauseMenu::CheckInput( FRONTEND_INPUT_RRIGHT ) )
        {
            imageDelta = 1;
        }

        if( imageDelta != 0 )
        {
            int newIdx = m_currentImageIdx + imageDelta;

            bool const c_isWrapping = IsWrapping();
            if( c_isWrapping )
            {
                newIdx = newIdx >= c_imageCount ? 0 : newIdx;
                newIdx = newIdx < 0 ? c_imageCount - 1 : newIdx;
            }
            else
            {
                newIdx = Clamp( newIdx, 0, c_imageCount - 1 );
            }

            if( newIdx != m_currentImageIdx )
            {
                m_currentImageIdx = newIdx;
                m_pipCounter.SetCurrentItem( m_currentImageIdx );
                imageChanged = true;
                ApplyCurrentImage();
            }
        }
    }

    return imageChanged;
}

bool CFeaturedImage::ValidateTextureIndex( int const idx ) const
{
    int const c_count = GetFeatureTextureCount();
    return uiVerifyf( idx >= 0 && idx < c_count, "Texture index out of range %u/%u", idx, c_count );
}

void CFeaturedImage::Add( char const * const txd, char const * const texture, bool const usable )
{
    FeatureTextureInfo& newInfo = m_featureTextureCollection.Grow();
    newInfo.m_txd = txd;
    newInfo.m_texture = texture;
    newInfo.m_usable = usable;

    if( m_currentImageIdx == INDEX_NONE )
    {
        m_currentImageIdx = 0;
        ApplyCurrentImage();
    }
}

void CFeaturedImage::ApplyCurrentImage()
{
    ApplyImage( m_currentImageIdx );
}   

void CFeaturedImage::ApplyImage( int const idx )
{
    if( ValidateTextureIndex( idx ) )
    {
        CComplexObject& displayObject = GetDisplayObject();
        if( displayObject.IsActive() )
        {
            FeatureTextureInfo& featureInfo = m_featureTextureCollection[idx];
            bool const c_usable = featureInfo.UpdateUsableState();
            displayObject.SetVisible( c_usable );

            if( c_usable )
            {
                char const * const txd = featureInfo.m_txd;
                char const * const texture = featureInfo.m_texture;
                featureInfo.m_displayed = true;

                CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_IMAGE" );
                CScaleformMgr::AddParamString( txd, false );
                CScaleformMgr::AddParamString( texture, false );
                CScaleformMgr::EndMethod();
            }
            else
            {
                featureInfo.m_displayed = false;
            }
        }
    }
}

void CFeaturedImage::InitializeDerived()
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        m_pipCounter.Initialize( displayObject );
    }
}

void CFeaturedImage::ShutdownDerived()
{
    m_pipCounter.Shutdown();
    Reset();
}

void CFeaturedImage::ResolveLayoutDerived(Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx)
{
    superclass::ResolveLayoutDerived( screenspacePosPx, localPosPx, sizePx );

    float const c_pipSize = m_pipCounter.GetPipSize();
    float const c_padding = GetPipPadding();
    float const c_bottom = GetImageSize();
    Vec2f const c_pipPos( localPosPx.GetX(), c_bottom - c_pipSize -c_padding );
    m_pipCounter.ResolveLayout( screenspacePosPx, c_pipPos, sizePx );
}

#endif // UI_PAGE_DECK_ENABLED
