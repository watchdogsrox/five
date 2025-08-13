/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IItemTextureInterface.h
// PURPOSE : Interface for working with textures from different sources on items
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_ITEM_TEXTURE_INTERFACE_H
#define I_ITEM_TEXTURE_INTERFACE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwui/Interfaces/Interface.h"

// rage
#include "system/lambda.h"

// game
#include "frontend/page_deck/items/uiCloudSupport.h"

class IItemTextureInterface
{
    FWUI_DECLARE_INTERFACE( IItemTextureInterface );
public: // declarations
    typedef LambdaRef< void( u32 const index, char const * const txd, char const * const texture )> PerTextureLambda;

public: // methods

    virtual char const * GetTxd() const = 0;
    virtual char const * GetTexture() const  = 0;
    bool IsTextureAvailable() const
    {
        char const * const c_txd = GetTxd();
        char const * const c_texture = GetTexture();

        bool const c_result = uiCloudSupport::IsTextureLoaded( c_texture, c_txd );
        return c_result;
    }

    void ForEachFeatureTexture( PerTextureLambda action ) { ForEachFeatureTextureDerived( action ); }
    virtual bool RequestFeatureTexture( rage::u32 const index ) { return IsFeatureTextureAvailable( index ); };
    virtual bool IsFeatureTextureAvailable( rage::u32 const index ) const
    {
        char const * const c_txd = GetFeatureTxd( index );
        char const * const c_texture = GetFeatureTexture( index );

        bool const c_result = uiCloudSupport::IsTextureLoaded( c_texture, c_txd );
        return c_result;
    }
    virtual void ReleaseFeatureTexture( rage::u32 const UNUSED_PARAM(index) ) { };

    virtual char const * GetTagTxd() const { return ""; }
    virtual char const * GetTagTexture() const { return ""; }

private:
    virtual void ForEachFeatureTextureDerived( PerTextureLambda action ) = 0;
    virtual char const * GetFeatureTxd( rage::u32 const UNUSED_PARAM(index) ) const { return nullptr; }
    virtual char const * GetFeatureTexture( rage::u32 const UNUSED_PARAM(index) ) const { return nullptr; }
};

FWUI_DEFINE_INTERFACE( IItemTextureInterface );

#endif // UI_PAGE_DECK_ENABLED

#endif // I_ITEM_TEXTURE_INTERFACE_H
