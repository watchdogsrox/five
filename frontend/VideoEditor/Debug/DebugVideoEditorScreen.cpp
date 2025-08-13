/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : DebugVideoEditorScreen.h
// PURPOSE : Debug version of the video editor, allows access to basic editing functionality.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "DebugVideoEditorScreen.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#if __BANK

// rage
#include "bank/bkmgr.h"

// game
#include "debug/UiGadget/UiGadgetList.h"
#include "debug/UiGadget/UiGadgetWindow.h"
#include "frontend/VideoEditor/Core/VideoEditorProject.h"
#include "frontend/VideoEditor/Core/VideoEditorUtil.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "UiGadgetFileSelector.h"
#include "UiGadgetFullscreenOverlay.h"
#include "UiGadgetPlaybackControls.h"
#include "UiGadgetVideoEditorPane.h"

FRONTEND_OPTIMISATIONS();

// If you move this out of the ui bank, be sure to update ShutdownWidgets to clean up appropriately.
#define VIDEO_EDITOR_BANK_NAME "ui"

CUiColorScheme CDebugVideoEditorScreen::ms_colourScheme;

float CDebugVideoEditorScreen::ms_debugWindowX				= 16.f;
float CDebugVideoEditorScreen::ms_debugWindowY				= 16.f;
float CDebugVideoEditorScreen::ms_debugWindowWidth			= 714.f;
float CDebugVideoEditorScreen::ms_debugWindowHeight			= 190.f;

CUiGadgetWindow* CDebugVideoEditorScreen::ms_debugWindow					= NULL;
CUiGadgetList* CDebugVideoEditorScreen::ms_topLevelMenu						= NULL;
CUiGadgetFileSelector* CDebugVideoEditorScreen::ms_fileSelector				= NULL;
CUiGadgetPlaybackControls* CDebugVideoEditorScreen::ms_playbackControls		= NULL;
CUiGadgetVideoEditorPane* CDebugVideoEditorScreen::ms_videoEditorPane		= NULL;
CUiGadgetFullscreenOverlay* CDebugVideoEditorScreen::ms_debugLoading		= NULL;

CRawClipFileView * CDebugVideoEditorScreen::ms_clipFileView					= NULL;
CVideoProjectFileView * CDebugVideoEditorScreen::ms_videoProjectFileView	= NULL;
CVideoEditorProject* CDebugVideoEditorScreen::ms_project					= NULL;

CDebugVideoEditorScreen::eState CDebugVideoEditorScreen::ms_state = CDebugVideoEditorScreen::INVALID;
bool CDebugVideoEditorScreen::ms_enabled = false;

void CDebugVideoEditorScreen::Init( unsigned initMode )
{
	UNUSED_VAR(initMode);
}

void CDebugVideoEditorScreen::Update()
{
	if( ms_enabled && !ms_debugWindow )
	{
		open();
	}
	else if( !ms_enabled && ms_debugWindow )
	{
		close();
	}

	if( isOpen() )
	{
		if( ms_debugLoading )
		{
			ms_debugLoading->SetActive( CVideoEditorInterface::ShouldShowLoadingScreen() );
		}

		if( CVideoEditorInterface::IsPreviewingClip() )
		{
			setState( VIDEO_PROJECT_EDITOR_PREVIEW_CLIP );
		}

		updateTopLevelMenuInput();

		if( ms_fileSelector )
		{
			ms_fileSelector->Update();
		}
	}
}

void CDebugVideoEditorScreen::Shutdown( unsigned shutdownMode )
{
	UNUSED_VAR(shutdownMode);
	close();
}

void CDebugVideoEditorScreen::InitWidgets()
{
	bkBank *bank = BANKMGR.FindBank( VIDEO_EDITOR_BANK_NAME );

	if( !bank )  // create the bank if not found
	{
		bank = &BANKMGR.CreateBank( VIDEO_EDITOR_BANK_NAME );
	}

	if( bank )
	{
		bank->AddButton("Create Debug Video Editor Widgets", &CDebugVideoEditorScreen::CreateBankWidgets);
	}
}

void CDebugVideoEditorScreen::ShutdownWidgets()
{
	// CPauseMenu owns the ui bank, so nothing to do here.
}

void CDebugVideoEditorScreen::CreateBankWidgets()
{
	static bool bDebugVideoEditorBankCreated = false;

	bkBank *bank = BANKMGR.FindBank( VIDEO_EDITOR_BANK_NAME );
	if ( !bDebugVideoEditorBankCreated && bank )
	{
		bank->PushGroup("Debug Video Editor");

		bank->AddToggle( "Toggle debug video editor", &CDebugVideoEditorScreen::ms_enabled );

		bank->PopGroup();

		bDebugVideoEditorBankCreated = true;
	}
}

void CDebugVideoEditorScreen::open()
{
	if( !ms_debugWindow )
	{
		ms_debugWindow = rage_new CUiGadgetWindow( ms_debugWindowX, ms_debugWindowY, 
													ms_debugWindowWidth, ms_debugWindowHeight, ms_colourScheme );

		if( ms_debugWindow )
		{
			ms_debugWindow->SetTitle( "Debug Video Editor" );
			
			CVideoEditorInterface::Activate();

			// Create all our menus in one go
			createTopLevelMenu();
			createFileSelector();
			createPlaybackControls();
			createVideoEditorPane();

			if( !ms_debugLoading )
			{
				ms_debugLoading = rage_new CUiGadgetFullscreenOverlay( ms_colourScheme );
				if( ms_debugLoading )
				{
					ms_debugLoading->SetText( "LOADING");
					g_UiGadgetRoot.AttachChild( ms_debugLoading );
				}
			}

			g_UiGadgetRoot.AttachChild( ms_debugWindow );

			// Start at the top!
			setState( TOP_LEVEL );
		}
	}

	ms_enabled = ms_debugWindow != NULL;
}

void CDebugVideoEditorScreen::close()
{
	if( ms_debugLoading )
	{
		cleanupUiGadget< CUiGadgetFullscreenOverlay >( ms_debugLoading );
	}

	if( ms_debugWindow )
	{
		CVideoEditorInterface::ReleaseRawClipFileView( ms_clipFileView );
		CVideoEditorInterface::ReleaseVideoProjectFileView( ms_videoProjectFileView );

		// Destroy menu elements
		cleanupTopLevelMenu();
		cleanupFileSelector();
		cleanupPlaybackControls();
		cleanupVideoEditorPane();

		CVideoEditorInterface::Deactivate();

		// Now cleanup the window itself
		cleanupUiGadget<CUiGadgetWindow>( ms_debugWindow );
	}

	ms_enabled = false;
	ms_state = INVALID;
}

void CDebugVideoEditorScreen::createTopLevelMenu()
{
	if( !ms_topLevelMenu && ms_debugWindow )
	{
		float afColumnOffsets[] = { 0.0f };
		ms_topLevelMenu = rage_new CUiGadgetList( ms_debugWindowX, ms_debugWindowY + 16.f, 130.f, 4, 1, afColumnOffsets, ms_colourScheme );
		if( ms_topLevelMenu )
		{
			ms_topLevelMenu->SetScrollbarVisibility( false );
			ms_topLevelMenu->SetSelectBehaviourToggles( false );
			ms_debugWindow->AttachChild( ms_topLevelMenu );
		}
	}
}

void CDebugVideoEditorScreen::updateTopLevelMenu()
{
	if( !ms_topLevelMenu || !ms_debugWindow )
		return;

	switch( ms_state )
	{
	case TOP_LEVEL:
		{
			ms_debugWindow->SetSize( ms_topLevelMenu->GetWidth(), ms_debugWindowHeight );

			ms_topLevelMenu->SetNumEntries( 4 );
			ms_topLevelMenu->SetCell( 0, 0, "Raw Clip Viewer" );
			ms_topLevelMenu->SetCell( 0, 1, "Video Project Editor" );
			ms_topLevelMenu->SetCell( 0, 2, "Auto-generate Video" );
			ms_topLevelMenu->SetCell( 0, 3, "Exit" );

			break;
		}
	case VIDEO_PROJECT_EDITOR_PREVIEW_CLIP:
	case VIDEO_PROJECT_EDITOR_PREVIEW_PROJECT:
	case CLIP_VIEWER_VIEWING_CLIP:
		{
			ms_debugWindow->SetSize( ms_topLevelMenu->GetWidth(), ms_debugWindowHeight );

			ms_topLevelMenu->SetNumEntries( 1 );
			ms_topLevelMenu->SetCell( 0, 0, "Back" );
			ms_topLevelMenu->SetCellVisible( 0, 1, false );
			ms_topLevelMenu->SetCellVisible( 0, 2, false );
			ms_topLevelMenu->SetCellVisible( 0, 3, false );

			break;
		}
	case AUTO_GENERATE_BAKE_VIDEO:
	case VIDEO_PROJECT_EDITOR_BAKE_VIDEO:
		{
			ms_debugWindow->SetSize( ms_topLevelMenu->GetWidth(), ms_debugWindowHeight );

			ms_topLevelMenu->SetNumEntries( 2 );
			ms_topLevelMenu->SetCell( 0, 0, "Pause" );
			ms_topLevelMenu->SetCell( 0, 1, "Back" );
			ms_topLevelMenu->SetCellVisible( 0, 2, false );
			ms_topLevelMenu->SetCellVisible( 0, 3, false );

			break;
		}
	case CLIP_VIEWER_SELECT_CLIP:
		{
			ms_debugWindow->SetSize( ms_debugWindowWidth, ms_debugWindowHeight );

			ms_topLevelMenu->SetNumEntries( 1 );
			ms_topLevelMenu->SetCell( 0, 0, "Back" );
			ms_topLevelMenu->SetCellVisible( 0, 1, false );
			ms_topLevelMenu->SetCellVisible( 0, 2, false );
			ms_topLevelMenu->SetCellVisible( 0, 3, false );

			break;
		}
	case VIDEO_PROJECT_EDITOR_SELECT_PROJECT:
		{
			ms_debugWindow->SetSize( ms_debugWindowWidth, ms_debugWindowHeight );

			ms_topLevelMenu->SetNumEntries( 2 );
			ms_topLevelMenu->SetCell( 0, 0, "New" );
			ms_topLevelMenu->SetCell( 0, 1, "Back" );
			ms_topLevelMenu->SetCellVisible( 0, 2, false );
			ms_topLevelMenu->SetCellVisible( 0, 3, false );

			break;
		}
	case VIDEO_PROJECT_EDITOR_EDIT_PROJECT:
		{
			ms_debugWindow->SetSize( ms_debugWindowWidth, ms_debugWindowHeight );

			ms_topLevelMenu->SetNumEntries( 4 );
			ms_topLevelMenu->SetCell( 0, 0, "Save" );
			ms_topLevelMenu->SetCell( 0, 1, "Preview Video" );
			ms_topLevelMenu->SetCell( 0, 2, "Bake video" );
			ms_topLevelMenu->SetCell( 0, 3, "Back" );

			break;
		}
	case AUTO_GENERATE_SELECT_SIZE:
		{
			ms_debugWindow->SetSize( ms_topLevelMenu->GetWidth(), ms_debugWindowHeight );

			ms_topLevelMenu->SetNumEntries( 4 );
			ms_topLevelMenu->SetCell( 0, 0, "Small" );
			ms_topLevelMenu->SetCell( 0, 1, "Medium" );
			ms_topLevelMenu->SetCell( 0, 2, "Large" );
			ms_topLevelMenu->SetCell( 0, 3, "Back" );

			break;
		}
	default:
		{
			// do nothing
		}
	}
}

void CDebugVideoEditorScreen::updateTopLevelMenuInput()
{
	if( ms_topLevelMenu &&ms_topLevelMenu->UserProcessClick() )
	{
		int const currentTopLevelIndex = ms_topLevelMenu->GetCurrentIndex();

		if( ms_state == TOP_LEVEL )
		{
			switch( currentTopLevelIndex )
			{
			case 0: // Raw Clip Viewer
				{
					setState( CLIP_VIEWER_SELECT_CLIP );
					ms_topLevelMenu->SetCurrentIndex( -1 );

					char path[RAGE_MAX_PATH] = { 0 };
					CVideoEditorUtil::getClipDirectory(path);
					CVideoEditorInterface::GrabRawClipFileView( path, ms_clipFileView );
					uiAssert( ms_clipFileView );
					ms_fileSelector->SetFileView( (CThumbnailFileView*)ms_clipFileView );
					ms_fileSelector->resetSelection();

					break;
				}
			case 1: // Video Project Editor
				{
					setState( VIDEO_PROJECT_EDITOR_SELECT_PROJECT );
					ms_topLevelMenu->SetCurrentIndex( -1 );

					// Grab video project view!
					char path[RAGE_MAX_PATH] = { 0 };
					CVideoEditorUtil::getVideoProjectDirectory(path);
					CVideoEditorInterface::GrabVideoProjectFileView( path, ms_videoProjectFileView );
					uiAssert( ms_videoProjectFileView );
					ms_fileSelector->SetFileView( (CThumbnailFileView*)ms_videoProjectFileView );
					ms_fileSelector->resetSelection();

					break;
				}
			case 2: // Auto-generate 
				{
					setState( AUTO_GENERATE_SELECT_SIZE );
					ms_topLevelMenu->SetCurrentIndex( -1 );

					break;
				}
			case 3: // Exit
			default:
				{
					ms_topLevelMenu->SetCurrentIndex( -1 );
					ms_enabled = false;
					return;
				}
			}
		}
		else if( ms_state == CLIP_VIEWER_SELECT_CLIP )
		{
			CVideoEditorInterface::ReleaseRawClipFileView( ms_clipFileView );

			// Back up a level!
			setState( TOP_LEVEL );
			ms_topLevelMenu->SetCurrentIndex( -1 );
		}
		else if( ms_state == CLIP_VIEWER_VIEWING_CLIP )
		{
			CVideoEditorInterface::KillPlaybackOrBake();

			// Back up a level!
			setState( TOP_LEVEL );
			ms_topLevelMenu->SetCurrentIndex( -1 );
		}
		else if( ms_state == VIDEO_PROJECT_EDITOR_SELECT_PROJECT )
		{
			if( currentTopLevelIndex == 0 )
			{
				CVideoEditorInterface::GrabNewProject( "DebugProject", ms_project );
				setState( VIDEO_PROJECT_EDITOR_EDIT_PROJECT );
			}
			else
			{
				// Back up a level!
				setState( TOP_LEVEL );
			}

			ms_topLevelMenu->SetCurrentIndex( -1 );
			CVideoEditorInterface::ReleaseVideoProjectFileView( ms_videoProjectFileView );
		}
		else if( ms_state == VIDEO_PROJECT_EDITOR_EDIT_PROJECT )
		{
			switch( currentTopLevelIndex )
			{
			case 0: // save project
				{
					if( ms_project )
					{
						ms_project->SaveProject( ms_project->GetName() );
					}

					break;
				}
			case 1: // Preview Project
				{
					setState( VIDEO_PROJECT_EDITOR_PREVIEW_PROJECT );
					CVideoEditorInterface::PlayProjectPreview();

					break;
				}
			case 2: // Bake out the video
				{
					setState( VIDEO_PROJECT_EDITOR_BAKE_VIDEO );
					CVideoEditorInterface::StartBakeProject( "DebugVideoEditorProject" );

					break;
				}
			case 3: // Back up a level!
			default:
				{
					// Change state then release editor, to ensure textures are released
					setState( TOP_LEVEL );
					CVideoEditorInterface::ReleaseProject( ms_project );
					ms_topLevelMenu->SetCurrentIndex( -1 );

					break;
				}
			}

			ms_topLevelMenu->SetCurrentIndex( -1 );
		}
		else if( ms_state == VIDEO_PROJECT_EDITOR_PREVIEW_PROJECT )
		{
			CVideoEditorInterface::KillPlaybackOrBake();

			// Back up a level!
			setState( VIDEO_PROJECT_EDITOR_EDIT_PROJECT );
			ms_topLevelMenu->SetCurrentIndex( -1 );
		}
		else if( ms_state == VIDEO_PROJECT_EDITOR_PREVIEW_CLIP )
		{
			CVideoEditorInterface::KillPlaybackOrBake();

			// Back up a level!
			setState( VIDEO_PROJECT_EDITOR_EDIT_PROJECT );
			ms_topLevelMenu->SetCurrentIndex( -1 );
		}
		else if( ms_state == VIDEO_PROJECT_EDITOR_BAKE_VIDEO )
		{
			if( currentTopLevelIndex == 0 )
			{
				if( CVideoEditorInterface::IsRenderingPaused() )
				{
					CVideoEditorInterface::ResumeBake();
					ms_topLevelMenu->SetCell( 0, 0, "Pause" );
				}
				else
				{
					CVideoEditorInterface::PauseBake();
					ms_topLevelMenu->SetCell( 0, 0, "Resume" );
				}

				ms_topLevelMenu->SetCurrentIndex( -1 );
			}
			else
			{
				CVideoEditorInterface::KillPlaybackOrBake();

				// Back up a level!
				setState( VIDEO_PROJECT_EDITOR_EDIT_PROJECT );
				ms_topLevelMenu->SetCurrentIndex( -1 );
			}
		}
		else if ( ms_state == AUTO_GENERATE_SELECT_SIZE )
		{
			switch( currentTopLevelIndex )
			{
			case 0: // small
				{
					setState( AUTO_GENERATE_BAKE_VIDEO );
					ms_topLevelMenu->SetCurrentIndex( -1 );
					CVideoEditorInterface::AutogenerateAndBakeProject( CReplayEditorParameters::AUTO_GENERATION_SMALL );
					ms_project = CVideoEditorInterface::GetActiveProject();
					break;
				}
			case 1: // medium
				{
					setState( AUTO_GENERATE_BAKE_VIDEO );
					ms_topLevelMenu->SetCurrentIndex( -1 );
					CVideoEditorInterface::AutogenerateAndBakeProject( CReplayEditorParameters::AUTO_GENERATION_MEDIUM );
					ms_project = CVideoEditorInterface::GetActiveProject();

					break;
				}
			case 2: // large
				{
					setState( AUTO_GENERATE_BAKE_VIDEO );
					ms_topLevelMenu->SetCurrentIndex( -1 );
					CVideoEditorInterface::AutogenerateAndBakeProject( CReplayEditorParameters::AUTO_GENERATION_LARGE );
					ms_project = CVideoEditorInterface::GetActiveProject();

					break;
				}
			case 3: // Extra large
				{
					setState( AUTO_GENERATE_BAKE_VIDEO );
					ms_topLevelMenu->SetCurrentIndex( -1 );
					CVideoEditorInterface::AutogenerateAndBakeProject( CReplayEditorParameters::AUTO_GENERATION_EXTRA_LARGE );
					ms_project = CVideoEditorInterface::GetActiveProject();

					break;
				}
			case 4: // Back!
			default:
				{
					setState( TOP_LEVEL );
					ms_topLevelMenu->SetCurrentIndex( -1 );
					break;
				}
			}
		}
		else if( ms_state == AUTO_GENERATE_BAKE_VIDEO )
		{
			if( currentTopLevelIndex == 0 )
			{
				if( CVideoEditorInterface::IsRenderingPaused() )
				{
					CVideoEditorInterface::ResumeBake();
					ms_topLevelMenu->SetCell( 0, 0, "Pause" );
				}
				else
				{
					CVideoEditorInterface::PauseBake();
					ms_topLevelMenu->SetCell( 0, 0, "Resume" );
				}

				ms_topLevelMenu->SetCurrentIndex( -1 );
			}
			else
			{
				CVideoEditorInterface::ReleaseProject( ms_project );
				CVideoEditorInterface::KillPlaybackOrBake();

				// Back up a level!
				setState( TOP_LEVEL );
				ms_topLevelMenu->SetCurrentIndex( -1 );
			}
		}
	}
}

void CDebugVideoEditorScreen::cleanupTopLevelMenu()
{
	cleanupUiGadget<CUiGadgetList>( ms_topLevelMenu );
}

void CDebugVideoEditorScreen::createFileSelector()
{
	if( !ms_fileSelector && ms_debugWindow )
	{
		float const xOffset = ms_topLevelMenu ? ms_topLevelMenu->GetWidth() : 0.f;
		ms_fileSelector = rage_new CUiGadgetFileSelector( ms_debugWindowX + xOffset, ms_debugWindowY + 16.f, CVideoEditorInterface::GetImageHandler(), ms_colourScheme );
		if( ms_fileSelector )
		{
			ms_debugWindow->AttachChild( ms_fileSelector );
			ms_fileSelector->SetActive( false );
		}
	}
}

void CDebugVideoEditorScreen::cleanupFileSelector()
{
	cleanupUiGadget( ms_fileSelector );
}

void CDebugVideoEditorScreen::createPlaybackControls()
{
	if( !ms_playbackControls && ms_debugWindow )
	{
		float const xOffset = ms_topLevelMenu ? ms_topLevelMenu->GetWidth() : 0.f;
		ms_playbackControls = rage_new CUiGadgetPlaybackControls( ms_debugWindowX + xOffset, ms_debugWindowY + 15.f, ms_colourScheme );
		if( ms_playbackControls )
		{
			ms_debugWindow->AttachChild( ms_playbackControls );
			ms_playbackControls->SetActive( false );
		}
	}
}

void CDebugVideoEditorScreen::cleanupPlaybackControls()
{
	cleanupUiGadget( ms_playbackControls );
}

void CDebugVideoEditorScreen::createVideoEditorPane()
{
	if( !ms_videoEditorPane && ms_debugWindow )
	{
		float const xOffset = ms_topLevelMenu ? ms_topLevelMenu->GetWidth() : 0.f;
		ms_videoEditorPane = rage_new CUiGadgetVideoEditorPane( ms_debugWindowX + xOffset, ms_debugWindowY + 15.f, ms_colourScheme );
		if( ms_videoEditorPane )
		{
			ms_debugWindow->AttachChild( ms_videoEditorPane );
			ms_videoEditorPane->SetActive( false );
		}
	}
}

void CDebugVideoEditorScreen::cleanupVideoEditorPane()
{
	cleanupUiGadget( ms_videoEditorPane );
}

void CDebugVideoEditorScreen::setState( eState const stateToSet )
{
	if( ms_state == stateToSet )
		return;

	if( ms_fileSelector )
	{
		if( stateToSet == CLIP_VIEWER_SELECT_CLIP )
		{
			ms_fileSelector->SetActive( true );
			ms_fileSelector->SetClipSelectedCallback( datCallback(onClipViewerLoadClip) );
		}
		else if( stateToSet == VIDEO_PROJECT_EDITOR_SELECT_PROJECT )
		{
			ms_fileSelector->SetActive( true );
			ms_fileSelector->SetClipSelectedCallback( datCallback(onVideoProjectEditorLoad) );
		}
		else
		{
			ms_fileSelector->SetFileView( NULL );
			ms_fileSelector->SetActive( false );
			ms_fileSelector->SetClipSelectedCallback( NullCB );
		}
	}

	if( ms_playbackControls )
	{
		if( stateToSet == CLIP_VIEWER_VIEWING_CLIP || 
			stateToSet == VIDEO_PROJECT_EDITOR_PREVIEW_CLIP || stateToSet == VIDEO_PROJECT_EDITOR_PREVIEW_PROJECT )
		{
			ms_playbackControls->SetActive( true );
		}
		else
		{
			ms_playbackControls->SetActive( false );
		}
	}

	if( ms_videoEditorPane )
	{
		if( stateToSet == VIDEO_PROJECT_EDITOR_EDIT_PROJECT )
		{
			ms_videoEditorPane->PrepareForEditing();
			ms_videoEditorPane->SetActive( true );
		}
		else 
		{
			ms_videoEditorPane->cleanup();
			ms_videoEditorPane->SetActive( false );
		}
	}

	ms_state = stateToSet;
	updateTopLevelMenu();
}

void CDebugVideoEditorScreen::onClipViewerLoadClip( void* callbackData )
{
	UNUSED_VAR( callbackData );
	uiAssertf( ms_state == CLIP_VIEWER_SELECT_CLIP, "CDebugVideoEditorScreen::onClipViewerLoadClip - Logic Error, callback from wrong state!" );
	if( ms_state == CLIP_VIEWER_SELECT_CLIP )
	{
		int const c_clipIndex = ms_fileSelector ? ms_fileSelector->GetSelectedIndex() : -1;
		if( c_clipIndex >= 0 )
		{
			char pathBuffer[ RAGE_MAX_PATH ];
			if( ms_clipFileView->getFileName( c_clipIndex, pathBuffer ) )
			{
				/* TODO - Raw loading not supported. Move over to using the preview project system per the actual UI.
				if(	uiVerifyf( CVideoEditorInterface::LoadRawClipForPlayback( pathBuffer ), "CDebugVideoEditorScreen::onClipViewerLoadClip - Failed to load clip %s", pathBuffer ) )
				{
					CVideoEditorInterface::ReleaseRawClipFileView( ms_clipFileView );
					setState( CLIP_VIEWER_VIEWING_CLIP );
				}*/
			}
		}	
	}
}

void CDebugVideoEditorScreen::onVideoProjectEditorLoad( void* callbackData )
{
	UNUSED_VAR( callbackData );
	uiAssertf( ms_state == VIDEO_PROJECT_EDITOR_SELECT_PROJECT, "CDebugVideoEditorScreen::onVideoProjectEditorLoad - Logic Error, callback from wrong state!" );
	if( ms_state == VIDEO_PROJECT_EDITOR_SELECT_PROJECT )
	{
		int const c_projectIndex = ms_fileSelector ? ms_fileSelector->GetSelectedIndex() : -1;
		if( c_projectIndex >= 0 )
		{
			char pathBuffer[ RAGE_MAX_PATH ];
			if( ms_videoProjectFileView->getFileName( c_projectIndex, pathBuffer ) )
			{
				if( CVideoEditorInterface::StartLoadProject( pathBuffer ) )
				{
					while( CVideoEditorInterface::IsLoadingProject() ) {}

					if( !CVideoEditorInterface::HasClipLoadFailed() )
					{
						CVideoEditorInterface::ReleaseVideoProjectFileView( ms_videoProjectFileView );
						setState( VIDEO_PROJECT_EDITOR_EDIT_PROJECT );
					}
				}
			}
		}	
	}
}

#endif // __BANK

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
