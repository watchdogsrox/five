/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetFileSelector.cpp
// PURPOSE : A compound gadget used for selecting files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#if __BANK

// rage
#include "file/limits.h"

// framework
#include "fwScene/stores/imposedTextureDictionary.h"

// game
#include "frontend/VideoEditor/Core/ImposedImageHandler.h"
#include "frontend/VideoEditor/Views/ThumbnailFileView.h"
#include "UiGadgetFileSelector.h"

FRONTEND_OPTIMISATIONS();

strStreamingObjectName const CUiGadgetFileSelector::sc_previewTextureName( "FILE_PREVIEW_TEXTURE" );

CUiGadgetFileSelector::CUiGadgetFileSelector( float const fX, float const fY, CImposedImageHandler& imageHandler, CUiColorScheme const& colourScheme, bool allowCancel )
	: CUiGadgetBase()
	, m_fileView( NULL )
	, m_imageHandler( imageHandler )
	, m_fileList( fX, fY, colourScheme )
	, m_previewPane( fX + m_fileList.GetWidth() + 20.f, fY, 256.f, 144.f, Color32( 255, 255, 255, 255 ) )
	, m_confirmButton( m_previewPane.GetPos().x, fY + m_previewPane.GetHeight() + 2.f, 8.f, 5.f, colourScheme, "Confirm" )
	, m_cancelButton( m_confirmButton.GetBounds().x1 + 5.f, fY + m_previewPane.GetHeight() + 2.f, 8.f, 5.f, colourScheme, "Cancel" )
	, m_oldFileIndex( -1 )
{
	AttachChild( &m_fileList );
	AttachChild( &m_previewPane );
	AttachChild( &m_confirmButton );
	AttachChild( &m_cancelButton );

	m_confirmButton.SetActive( false );
	m_cancelButton.SetActive( allowCancel );
}

CUiGadgetFileSelector::~CUiGadgetFileSelector()
{
	cleanupPreviewPane();
}

void CUiGadgetFileSelector::resetSelection()
{
	cleanupPreviewPane();
	m_fileList.resetSelection();
	m_confirmButton.SetActive( false );
	m_oldFileIndex = -1;
}

void CUiGadgetFileSelector::Update()
{
	if( IsActive() && m_fileView )
	{
		int const c_selectedIndex = GetSelectedIndex();
		bool const indexChanged = m_oldFileIndex != c_selectedIndex;

		if( !indexChanged && m_previewPane.getTexture() == NULL && c_selectedIndex >= 0 )
		{
			m_previewPane.setTexture( m_fileView->isThumbnailLoaded( c_selectedIndex ) ? 
				m_imageHandler.getTexture( m_fileView->getThumbnailName( c_selectedIndex ) ) : NULL );
		}

		if( indexChanged )
		{
			cleanupPreviewPane();
			m_oldFileIndex = c_selectedIndex;

			if( c_selectedIndex >= 0 )
			{
				initializePreviewPane();
				m_confirmButton.SetActive( true );
			}
		}	
	}
}

rage::fwBox2D CUiGadgetFileSelector::GetBounds() const
{
	Vector2 vPos = m_fileList.GetPos();
	return fwBox2D( vPos.x, vPos.y, vPos.x + m_fileList.GetWidth() + m_previewPane.GetWidth(), m_fileList.GetBounds().y1 );
}

void CUiGadgetFileSelector::initializePreviewPane()
{
	if( m_fileView )
	{
		m_fileView->requestThumbnail( GetSelectedIndex() );
	}
}

void CUiGadgetFileSelector::cleanupPreviewPane()
{
	m_previewPane.setTexture( NULL );

	if( m_fileView && m_oldFileIndex >= 0 )
	{
		m_fileView->releaseThumbnail( m_oldFileIndex );
	}
}

#endif //__BANK
