/////////////////////////////////////////////////////////////////////////////////
//
// FILE	   : UiGadgetVideoEditorPane.cpp
// PURPOSE : A compound gadget used for editing a video project.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "UiGadgetVideoEditorPane.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#if __BANK

// game
#include "replaycoordinator/ReplayCoordinator.h"
#include "replaycoordinator/storage/Clip.h"
#include "replaycoordinator/storage/Montage.h"
#include "frontend/VideoEditor/Core/VideoEditorProject.h"
#include "frontend/VideoEditor/Core/VideoEditorUtil.h"
#include "frontend/VideoEditor/Views/RawClipFileView.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "replaycoordinator/rendering/ReplayPostFxRegistry.h"
#include "text/text.h"

FRONTEND_OPTIMISATIONS();

char const * const CUiGadgetVideoEditorPane::sc_randomText[ sc_randomStringCount ] = 
{
	"Such Replay!", "Much Edits"
};

CUiGadgetVideoEditorPane::CUiGadgetVideoEditorPane( float const fX, float const fY, CUiColorScheme const& colourScheme )
	: CUiGadgetBase()
	, m_clipPreviewA( fX + 5.f, fY, 208.f, 117.f, Color32( 128, 128, 128, 255 ) )
	, m_transitionLabel( fX + m_clipPreviewA.GetWidth() + 25.f, fY + 102.f, colourScheme.GetColor( CUiColorScheme::COLOR_LIST_ENTRY_TEXT ), "No Transition" )
	, m_clipPreviewB( m_transitionLabel.GetBounds().x1 + 45.f, fY, 208.f, 117.f, Color32( 128, 128, 128, 255 ) )
	, m_clipSelectA( m_clipPreviewA.GetPos().x, fY + m_clipPreviewA.GetHeight(), 4.f, 4.f, colourScheme, "Change Clip A" )
	, m_playClipA( m_clipPreviewA.GetPos().x, m_clipSelectA.GetBounds().y1 + 4.f, 1.f, 4.f, colourScheme, "Play Clip" )
	, m_transitionSelect( m_transitionLabel.GetPos().x, fY + m_clipPreviewA.GetHeight(), 4.f, 4.f, colourScheme, "Change Transition" )
	, m_clipSelectB( m_clipPreviewB.GetPos().x, fY + m_clipPreviewB.GetHeight(), 4.f, 4.f, colourScheme, "Change Clip B" )
	, m_playClipB( m_clipPreviewB.GetPos().x, m_clipSelectB.GetBounds().y1 + 4.f, 1.f, 4.f, colourScheme, "Play Clip" )
	, m_clipSelector( fX, fY, CVideoEditorInterface::GetImageHandler(), colourScheme, true )
	, m_toggleText( m_transitionLabel.GetPos().x, m_playClipB.GetBounds().y0, 1.f, 4.f, colourScheme, "Add Random Text" )
	, m_toggleFx( m_transitionLabel.GetPos().x, m_toggleText.GetBounds().y1 + 4.f, 1.f, 4.f, colourScheme, "Add FX" )
	, m_background( fX, fY, 500.f, 170.f, colourScheme.GetColor( CUiColorScheme::COLOR_WINDOW_BG ) )
	, m_state( EDITING )
	, m_pendingState( INVALID )
	, m_clipFileView( NULL )
	, m_clipSelectionIndex( 0 )
{
	m_background.AttachChild( &m_toggleFx );
	m_background.AttachChild( &m_toggleText );
	m_background.AttachChild( &m_playClipA );
	m_background.AttachChild( &m_clipPreviewA );
	m_background.AttachChild( &m_transitionLabel );
	m_background.AttachChild( &m_playClipB );
	m_background.AttachChild( &m_clipPreviewB );
	m_background.AttachChild( &m_clipSelectA );
	m_background.AttachChild( &m_transitionSelect );
	m_background.AttachChild( &m_clipSelectB );
	
	AttachChild( &m_clipSelector );
	AttachChild( &m_background );

	m_transitionSelect.SetPressedCallback( onTransitionSelectPressed );
	m_clipSelectA.SetPressedCallback( datCallback( onRequestClipSelector, NULL, true ) );
	m_clipSelectB.SetPressedCallback( datCallback( onRequestClipSelector, NULL, true ) );

	m_playClipA.SetPressedCallback( datCallback( onPreviewClip, NULL, true ) );
	m_playClipB.SetPressedCallback( datCallback( onPreviewClip, NULL, true ) );

	m_clipSelector.SetCancelCallback( datCallback( onCancelClipSelector, (void*)this, false ) );
	m_clipSelector.SetClipSelectedCallback( datCallback( onConfirmClipSelector, (void*)this, false ) );

	m_toggleText.SetPressedCallback( datCallback( onToggleText, NULL, true ) );
	m_toggleFx.SetPressedCallback( datCallback( onToggleFx, NULL, true ) );

	setState( EDITING );
}

CUiGadgetVideoEditorPane::~CUiGadgetVideoEditorPane()
{

}

void CUiGadgetVideoEditorPane::cleanup()
{
	CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
	if( project && project->GetClipCount() > 0 )
	{
		if( m_clipPreviewA.getTexture() != NULL )
		{
			m_clipPreviewA.setTexture( NULL );
			project->ReleaseClipThumbnail( 0 );
		}

		if( m_clipPreviewB.getTexture() != NULL )
		{
			m_clipPreviewB.setTexture( NULL );
			project->ReleaseClipThumbnail( 1 );
		}
	}
}

void CUiGadgetVideoEditorPane::PrepareForEditing()
{
	cleanup();

	m_pendingState = INVALID;
	setState( EDITING );

	CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
	if( project )
	{
		if( project->GetClipCount() > 0 )
		{
			project->RequestClipThumbnail( 0 );
		}

		if( project->GetClipCount() > 1 )
		{
			project->RequestClipThumbnail( 1 );
		}
	}
}

void CUiGadgetVideoEditorPane::Update()
{
	if( m_pendingState != INVALID )
	{
		setState( m_pendingState );
		m_pendingState = INVALID;
	}

	bool showClipAButton = false;
	bool showClipBButton = false;

	CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
	if( project )
	{
		updateTextToggle( *project );
		updateFxToggle( *project );

		// Update our thumbnails
		if(  project->GetClipCount() > 0 )
		{
			updateClipPreview( m_clipPreviewA, *project, 0 );
			showClipAButton = true;
		
			if(  project->GetClipCount() > 1 )
			{
				updateClipPreview( m_clipPreviewB, *project, 1 );
				showClipBButton = true;
			}

			m_transitionLabel.SetString( project->GetEndTransition( 0 ) == MARKER_TRANSITIONTYPE_NONE ? 
				"No Transition" : "Fade in-out" );
		}
	}

	m_playClipA.SetActive( showClipAButton );
	m_playClipB.SetActive( showClipBButton );
}

rage::fwBox2D CUiGadgetVideoEditorPane::GetBounds() const
{
	return m_background.GetBounds();
}

void CUiGadgetVideoEditorPane::onTransitionSelectPressed( void* callbackData )
{
	UNUSED_VAR( callbackData );

	CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
	if( project && project->GetClipCount() > 0 )
	{
		ReplayMarkerTransitionType activeTransition;
		ReplayMarkerTransitionType newTransition;


		activeTransition = project->GetEndTransition( 0 );
		newTransition = ( activeTransition == MARKER_TRANSITIONTYPE_NONE ) ? 
			MARKER_TRANSITIONTYPE_FADEOUT : MARKER_TRANSITIONTYPE_NONE;

		project->SetEndTransition( 0, newTransition );
		project->ValidateTransitionDuration( 0 );

		if( project->GetClipCount() > 1 )
		{
			activeTransition = project->GetStartTransition( 1 );

			newTransition = ( activeTransition == MARKER_TRANSITIONTYPE_NONE ) ? 
				MARKER_TRANSITIONTYPE_FADEIN : MARKER_TRANSITIONTYPE_NONE;

			project->SetStartTransition( 1, newTransition );
			project->ValidateTransitionDuration( 1 );
		}
	}
}

void CUiGadgetVideoEditorPane::onRequestClipSelector( void* callbackData )
{
	CUiGadgetButton* caller = callbackData ? static_cast<CUiGadgetButton*>(callbackData) : NULL;
	CUiGadgetVideoEditorPane* parent = caller ? static_cast<CUiGadgetVideoEditorPane*>( caller->GetParent()->GetParent() ) : NULL;

	if( parent )
	{
		parent->onRequestClipSelector( caller );
	}
}

void CUiGadgetVideoEditorPane::onCancelClipSelector( void* callbackData )
{
	CUiGadgetVideoEditorPane* pThis = callbackData ? static_cast<CUiGadgetVideoEditorPane*>( callbackData ) : NULL;
	if( pThis )
	{
		pThis->m_pendingState = EDITING;
	}
}

void CUiGadgetVideoEditorPane::onConfirmClipSelector( void* callbackData )
{
	CUiGadgetVideoEditorPane* pThis = callbackData ? static_cast<CUiGadgetVideoEditorPane*>( callbackData ) : NULL;
	if( pThis )
	{
		pThis->updateClipInSlot();
		pThis->m_pendingState = EDITING;
	}
}

void CUiGadgetVideoEditorPane::onPreviewClip( void* callbackData )
{
	CUiGadgetButton* caller = callbackData ? static_cast<CUiGadgetButton*>(callbackData) : NULL;
	CUiGadgetVideoEditorPane* parent = caller ? static_cast<CUiGadgetVideoEditorPane*>( caller->GetParent()->GetParent() ) : NULL;

	if( parent )
	{
		parent->onPreviewClip( caller );
	}
}

void CUiGadgetVideoEditorPane::onToggleText( void* callbackData )
{
	CUiGadgetButton* caller = callbackData ? static_cast<CUiGadgetButton*>(callbackData) : NULL;
	CUiGadgetVideoEditorPane* parent = caller ? static_cast<CUiGadgetVideoEditorPane*>( caller->GetParent()->GetParent() ) : NULL;

	if( parent )
	{
		parent->onToggleText();
	}
}

void CUiGadgetVideoEditorPane::onToggleFx( void* callbackData )
{
	CUiGadgetButton* caller = callbackData ? static_cast<CUiGadgetButton*>(callbackData) : NULL;
	CUiGadgetVideoEditorPane* parent = caller ? static_cast<CUiGadgetVideoEditorPane*>( caller->GetParent()->GetParent() ) : NULL;

	if( parent )
	{
		parent->onToggleFx();
	}
}

void CUiGadgetVideoEditorPane::onRequestClipSelector( CUiGadgetButton const* button )
{
	if( button )
	{
		m_pendingState = SELECTING_CLIP;
		m_clipSelectionIndex = ( button == &m_clipSelectA ) ? 0 : 1;
	}
}

void CUiGadgetVideoEditorPane::onPreviewClip( CUiGadgetButton const* button )
{
	if( button )
	{
		int const clipToPlay = ( button == &m_clipSelectA ) ? 0 : 1;

		CVideoEditorInterface::PlayClipPreview( clipToPlay, true );
		m_pendingState = VIEWING_CLIP;
	}
}

void CUiGadgetVideoEditorPane::onToggleText()
{
	CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
	if( project )
	{
		size_t textCount = project->GetOverlayTextCount();
		if( textCount == 0 )
		{
			char const * const textToAdd = ( rand() & 1 ) == 0 ? sc_randomText[ 0 ] : sc_randomText[ 1 ];
			
			CTextOverlayHandle textHandle = project->AddOverlayText( 1000, 10000 );
			textHandle.SetLine( 0, textToAdd, Vector2( 0.5f, 0.5f ), Vector2(1.f,1.f), CRGBA( 128, 128, 128, 255 ), FONT_STYLE_PRICEDOWN );
		}
		else
		{
			while( textCount > 0 )
			{
				project->DeleteOverlayText( 0 );
				textCount = project->GetOverlayTextCount();
			}
		}
	}
}

void CUiGadgetVideoEditorPane::onToggleFx()
{
	CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
	IReplayMarkerStorage* markerStorage = project ? project->GetClipMarkers( 0 ) : NULL;
	if( markerStorage )
	{
		size_t markerCount = markerStorage->GetMarkerCount();
		if( markerCount == 0 )
		{
			s32 dummyIndex;
			float markerTime = 0;
			u32 const effectCount = (u32)CReplayCoordinator::GetPostFxRegistry().GetEffectCount();
			float const markerTimeSlice = (float)project->GetClipNonDilatedDurationMs( 0 ) / effectCount;

			for( u32 index = 0; index < effectCount; ++index, markerTime += markerTimeSlice )
			{
				sReplayMarkerInfo* newMarker = rage_new sReplayMarkerInfo();
				if( newMarker )
				{
					newMarker->SetNonDilatedTimeMs( markerTime );
					newMarker->SetActivePostFx( (u8)index );

					markerStorage->AddMarker( newMarker, false, index == effectCount - 1, false, dummyIndex );
				}
			}
		}
		else
		{
			while( markerCount > 0 )
			{
				markerStorage->RemoveMarker( 0 );
				markerCount = markerStorage->GetMarkerCount();
			}
		}
	}
}

void CUiGadgetVideoEditorPane::updateTextToggle( CVideoEditorProject const& project )
{
	m_toggleText.SetLabel( project.GetOverlayTextCount() == 0 ? "Add Random Text" : "Remove Text" );
}

void CUiGadgetVideoEditorPane::updateFxToggle( CVideoEditorProject const & project )
{
	IReplayMarkerStorage const* markerStorage = project.GetClipCount() > 0 ? project.GetClipMarkers( 0 ) : NULL;
	m_toggleFx.SetLabel( markerStorage && markerStorage->GetMarkerCount() == 0 ? "Add FX" : "Remove FX" );
}

void CUiGadgetVideoEditorPane::updateClipPreview( CUiGadgetImage& targetImage, CVideoEditorProject& project, u32 const clipIndex )
{
	if( targetImage.getTexture() == NULL )
	{
		if( project.IsClipThumbnailLoaded( clipIndex ) )
		{
			targetImage.setTexture( project.GetClipThumbnailTexture( clipIndex ) );
		}
	}
}

void CUiGadgetVideoEditorPane::releaseClipPreview( CUiGadgetImage& targetImage, CVideoEditorProject& project, u32 const clipIndex )
{
	targetImage.setTexture( NULL );

	if( clipIndex < project.GetClipCount() )
	{
		project.ReleaseClipThumbnail( clipIndex );
	}
}

void CUiGadgetVideoEditorPane::updateClipInSlot()
{
	if( m_state == SELECTING_CLIP && m_clipFileView )
	{
		CVideoEditorProject* project = CVideoEditorInterface::GetActiveProject();
		if( project && m_clipSelectionIndex <= project->GetClipCount() )
		{
			int const c_clipIndex = m_clipFileView ? m_clipSelector.GetSelectedIndex() : -1;
			if( c_clipIndex >= 0 )
			{
				char pathBuffer[ RAGE_MAX_PATH ];
				if( m_clipFileView->getFileName( c_clipIndex, pathBuffer ) )
				{
					u64 const ownerId = m_clipFileView->getOwnerId( c_clipIndex );
					releaseClipPreview( m_clipSelectionIndex == 0 ? m_clipPreviewA : m_clipPreviewB, *project, m_clipSelectionIndex );
					project->DeleteClip( m_clipSelectionIndex );
					project->AddClip( ownerId, pathBuffer, m_clipSelectionIndex );
					project->RequestClipThumbnail( m_clipSelectionIndex );
				}
			}	
		}
	}
}

void CUiGadgetVideoEditorPane::setState( EditorState stateToSet )
{
	switch ( stateToSet )
	{
	case EDITING:
		{
			m_background.SetActive( true );
			m_background.SetChildrenActive( true );
			m_clipSelector.SetActive( false );

			m_clipSelector.SetFileView( NULL );
			CVideoEditorInterface::ReleaseRawClipFileView( m_clipFileView );
			m_clipSelector.SetFileView( NULL );

			break;
		}
	case VIEWING_CLIP:
		{
			m_background.SetActive( false );
			m_background.SetChildrenActive( false );
			m_clipSelector.SetFileView( NULL );

			break;
		}
	case SELECTING_CLIP:
		{
			m_background.SetActive( false );
			m_background.SetChildrenActive( false );
			m_clipSelector.SetActive( true );

			CVideoEditorInterface::ReleaseRawClipFileView( m_clipFileView );
			char path[RAGE_MAX_PATH] = { 0 };
			CVideoEditorUtil::getClipDirectory(path);
			CVideoEditorInterface::GrabRawClipFileView( path, m_clipFileView );
			m_clipSelector.SetFileView( (CThumbnailFileView*)m_clipFileView );

			break;
		}

	default:
		break;
	}

	m_state = stateToSet;
}

#endif //__BANK

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
