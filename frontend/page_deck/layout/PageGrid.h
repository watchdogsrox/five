/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageGrid.h
// PURPOSE : Layout type for organizing items into grid of NxM slots, with
//           parGen adjustable configurations
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_GRID_H
#define PAGE_GRID_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"
#include "system/lambda.h"

// game
#include "frontend/page_deck/layout/PageLayoutBase.h"
#include "frontend/page_deck/uiPageEnums.h"

#define MAX_GRID_SIZE_DEFAULT (32678.f)

// Shared definition for row/columns
class PageGridSizingPacket
{
public: // methods
    PageGridSizingPacket()
        : m_size( 1.f )
        , m_minSize( 0.f )
        , m_maxSize( MAX_GRID_SIZE_DEFAULT )
        , m_calculatedSize( -1.f )
        , m_type( uiPageDeck::SIZING_TYPE_STAR )
    {

    }

    PageGridSizingPacket( float const size, float const minSize, float const maxSize, uiPageDeck::eGridSizingType const sizingType )
        : m_size( size )
        , m_minSize( minSize )
        , m_maxSize( maxSize )
        , m_calculatedSize( -1.f )
        , m_type( sizingType )
    {

    }
    virtual ~PageGridSizingPacket() {}

    float GetMinSize() const { return m_minSize; }
    void SetMinSize( float const minSize ) { m_minSize = minSize; }

    float GetSize() const { return m_size; }
    void SetSize( float const size ) { m_size = size; }

    float GetMaxSize() const { return m_maxSize; }
    void SetMaxSize( float const maxSize ) { m_maxSize = maxSize; }

    uiPageDeck::eGridSizingType GetSizingType() const { return m_type; }
    void SetSizingType( uiPageDeck::eGridSizingType const sizingType ) { m_type = sizingType; }

    bool IsFixedSize() const { return m_type == uiPageDeck::SIZING_TYPE_DIP; }
    bool IsStarSize() const { return m_type == uiPageDeck::SIZING_TYPE_STAR; }

    float GetDefaultSize() const;
    float GetCalculatedSize() const{ return m_calculatedSize; }
    void SetCalculatedSize( float const calculatedSize ) { m_calculatedSize = calculatedSize; }
    void SetCalculatedSizeClamped( float const calculatedSize ) 
    { 
        m_calculatedSize = Clamp( calculatedSize, m_minSize, m_maxSize ); 
    }
    float GetCalculatedOffset() const{ return m_calculatedOffset; }
    void SetCalculatedOffset( float const calculatedOffset ) { m_calculatedOffset = calculatedOffset; }

private: // Declarations and variables
    float                       m_size;
    float                       m_minSize;
    float                       m_maxSize;
    float                       m_calculatedSize; 
    float                       m_calculatedOffset; 
    uiPageDeck::eGridSizingType m_type;
    PAR_PARSABLE;
};

// Col extension, for type safety
class PageGridCol final : public PageGridSizingPacket
{
    typedef PageGridSizingPacket superclass;
public: // methods
    PageGridCol(): superclass() {}
    PageGridCol( float const size, float const minSize, float const maxSize, uiPageDeck::eGridSizingType const sizingType )
        : superclass( size, minSize, maxSize, sizingType ) {}
    virtual ~PageGridCol() {}
private: // Declarations and variables
    PAR_PARSABLE;
};

// Row extension, for type safety
class PageGridRow final : public PageGridSizingPacket
{
    typedef PageGridSizingPacket superclass;
public: // methods
    PageGridRow(): superclass() {}
    PageGridRow( float const size, float const minSize, float const maxSize, uiPageDeck::eGridSizingType const sizingType )
        : superclass( size, minSize, maxSize, sizingType ) {}
    virtual ~PageGridRow() {}
private: // Declarations and variables
    PAR_PARSABLE;
};

/*
    For those joining the class from RDR, this class is a Fisher-Price version
    of UIGrid from the RDR layout engine, but combined natively with the GridParams object.

    It also is not a hierarchical node setup, but rather a static positioner based on pre-defined
    configurations. So a lot of the logic is much simpler
*/
class CPageGrid : public CPageLayoutBase
{
    typedef CPageLayoutBase superclass;

public:
    CPageGrid();
    virtual ~CPageGrid();
    
    void ResetRows();
    void ResetColumns();

    void AddRow( uiPageDeck::eGridSizingType const sizingType = uiPageDeck::SIZING_TYPE_STAR, float const size = 1.f, float const minSize = 0.f, float const maxSize = MAX_GRID_SIZE_DEFAULT );
    void AddCol( uiPageDeck::eGridSizingType const sizingType = uiPageDeck::SIZING_TYPE_STAR, float const size = 1.f, float const minSize = 0.f, float const maxSize = MAX_GRID_SIZE_DEFAULT );

    void GetCell( Vec2f_InOut out_pos, Vec2f_InOut out_size, CPageLayoutItemParams const& params ) const;
    void GetCell( Vec2f_InOut out_pos, Vec2f_InOut out_size, size_t const col, size_t const row, size_t const hSpan = 1, size_t const vSpan = 1 ) const;

    size_t GetColumnCount() const { return m_columns.size(); }
    size_t GetRowCount() const { return m_rows.size(); }

protected: // declarations and variables
    typedef atArray<PageGridCol>                    ColumnCollection;
    typedef ColumnCollection::iterator              ColumnIterator;
    typedef ColumnCollection::const_iterator        ConstColumnIterator;
    typedef atArray<PageGridRow>                    RowCollection;
    typedef RowCollection::iterator                 RowIterator;
    typedef RowCollection::const_iterator           ConstRowIterator;

    // These are protected so that we can access them in a derived class in the PSC
    ColumnCollection m_columns;
    RowCollection m_rows;

protected: // methods
    void ResetTo1x1();

    void ResetDerived() override
    {
        ResetRows();
        ResetColumns();
        ResetTo1x1();
    }
    char const * const GetSymbolDerived( CPageItemBase const& pageItem ) const override;

    CPageLayoutItemBase* GetItemByCell( int const x, int const y, int const searchStartIndex = 0, bool const useSpan = false );
    CPageLayoutItemBase const * GetItemByCell( int const x, int const y, int const searchStartIndex = 0, bool const useSpan = false ) const;
    CPageLayoutItemBase*  GetItemByColumn( int const x, int const ySuggested );
    CPageLayoutItemBase const*  GetItemByColumn( int const x, int const ySuggested ) const;
    CPageLayoutItemBase*  GetItemByRow( int const xSuggested, int const y );
    CPageLayoutItemBase const*  GetItemByRow( int const xSuggested, int const y ) const;

private: // declarations and variables
    typedef LambdaRef< void( PageGridCol& )>        PerColLambda;
    typedef LambdaRef< void( PageGridCol const& )>  PerColConstLambda;
    typedef LambdaRef< void( PageGridRow& )>        PerRowLambda;
    typedef LambdaRef< void( PageGridRow const& )>  PerRowConstLambda;
    PAR_PARSABLE;
    NON_COPYABLE( CPageGrid );

private: // methods

    PageGridCol * TryGetColumnDefinition( size_t const index )
    {
        PageGridCol* const result = index < GetColumnCount() ? &m_columns[ (u32)index ] : nullptr;
        return result;
    }
    PageGridCol const * TryGetColumnDefinition( size_t const index ) const
    {
        PageGridCol const * const c_result = index < GetColumnCount() ? &m_columns[ (u32)index ] : nullptr;
        return c_result;
    }

    PageGridRow * TryGetRowDefinition( size_t const index )
    {
        PageGridRow* const result = index < GetRowCount() ? &m_rows[ (u32)index ] : nullptr;
        return result;
    }
    PageGridRow const * TryGetRowDefinition( size_t const index ) const
    {
        PageGridRow const * const c_result = index < GetRowCount() ? &m_rows[ (u32)index ] : nullptr;
        return c_result;
    }

    void GetItemPositionAndSizeDerived( int const index, Vec2f_InOut out_pos, Vec2f_InOut out_size ) const final;

    void ForEachCol( PerColLambda action );
    void ForEachCol( PerColConstLambda action ) const;
    void ForEachRow( PerRowLambda action );
    void ForEachRow( PerRowConstLambda action ) const;

    Vec2f GetExtentsDerived() const final;
    void RecalculateLayoutInternal( Vec2f_In extents ) final;
    void PositionItem( CPageLayoutItemBase& item, CPageLayoutItemParams const& params ) const;
    rage::fwuiNavigationResult HandleDigitalNavigationDerived( rage::fwuiNavigationParams const& navParams ) final;
    rage::fwuiNavigationResult HandleDigitalNavigationByRaycast( rage::fwuiNavigationParams const& navParams );
    rage::fwuiNavigationResult HandleDigitalNavigationByIndices( rage::fwuiNavigationParams const& navParams );

#if RSG_BANK
    void DebugRenderDerived() const final;
    void RenderGridCells() const;
#endif 
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_GRID_H
