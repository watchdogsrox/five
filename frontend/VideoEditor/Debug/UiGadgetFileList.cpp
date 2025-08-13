/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetFileList.cpp
// PURPOSE : Gadget used for listing details about files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#if __BANK

// rage
#include "file/limits.h"

// game
#include "frontend/VideoEditor/Views/FileViewFilter.h"
#include "UiGadgetFileList.h"

FRONTEND_OPTIMISATIONS();

static float sc_columnOffsets[] = { 0.0f, 105.f };
static char const * sc_columnNames[] = { "Name", "Timestamp" };

CUiGadgetFileList::CUiGadgetFileList( float const fX, float const fY, CUiColorScheme const& colourScheme )
	: CUiGadgetBase()
	, m_fileList( fX, fY, 240.f, 14, 2, sc_columnOffsets, colourScheme, sc_columnNames )
	, m_fileView( NULL )
	, m_selectedIndex( -1 )
{
	resetSelection();

	m_fileList.SetSelectBehaviourToggles( false );
	AttachChild( &m_fileList );
}

CUiGadgetFileList::~CUiGadgetFileList()
{
	
}

void CUiGadgetFileList::resetSelection()
{
	m_fileList.SetCurrentIndex( -1 );
	m_selectedIndex = -1;
}

void CUiGadgetFileList::Update()
{
	if( IsActive() && m_fileView )
	{
		if( m_fileList.UserProcessClick() )
		{
			m_selectedIndex = m_fileList.GetCurrentIndex();
		}
		else if( m_fileList.GetNumEntries() != m_fileView->getUnfilteredFileCount() )
		{
			refreshList();
		}
	}
}

rage::fwBox2D CUiGadgetFileList::GetBounds() const
{
	Vector2 vPos = m_fileList.GetPos();
	return fwBox2D( vPos.x, vPos.y, vPos.x + m_fileList.GetWidth(), m_fileList.GetBounds().y1 );
}

void CUiGadgetFileList::cellUpdate( CUiGadgetText* pResult, u32 row, u32 col, void* extraCallbackData )
{
	if( !pResult || !extraCallbackData )
		return;

	CFileViewFilter const* viewFilter = static_cast<CFileViewFilter*>(extraCallbackData);
	if( !viewFilter )
		return;

	size_t const maxRows = viewFilter->getUnfilteredFileCount();

	if( row	>= maxRows )
	{
		pResult->SetString( "" );
	}
	else
	{
		char buffer[ RAGE_MAX_PATH ];

		// Name Column
		if( col == 0 )
		{
			viewFilter->getFileName( row, buffer );
			pResult->SetString( buffer );
		}
		else // Assume date as we only have 2 columns
		{
			viewFilter->getFileDateString( row, buffer );
			pResult->SetString( buffer );
		}
	}
}

void CUiGadgetFileList::refreshList()
{
	if( m_fileView )
	{
		m_fileList.SetNumEntries( (u32)m_fileView->getUnfilteredFileCount() );
		m_fileList.SetUpdateCellCB( CUiGadgetFileList::cellUpdate, m_fileView );
	}
}

#endif //__BANK
