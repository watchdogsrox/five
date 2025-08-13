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
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef DEBUG_VIDEO_EDITOR_SCREEN_H_
#define DEBUG_VIDEO_EDITOR_SCREEN_H_

#if __BANK

// game
#include "debug/UiGadget/UiColorScheme.h"
#include "debug/UiGadget/UiGadgetBase.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"

class CUiGadgetButton;
class CUiGadgetFileSelector;
class CUiGadgetList;
class CUiGadgetPlaybackControls;
class CUiGadgetText;
class CUiGadgetVideoEditorPane;
class CUiGadgetWindow;
class CUiGadgetFullscreenOverlay;

class CRawClipFileView;
class CVideoProjectFileView;
class CVideoEditorProject;

class CDebugVideoEditorScreen
{
public:

	static void Init(unsigned initMode);
	static void Update();
	static void Shutdown(unsigned shutdownMode);

	static void InitWidgets();
	static void ShutdownWidgets();

private: // declarations and variables
	enum eState
	{
		INVALID,

		TOP_LEVEL,

		CLIP_VIEWER_SELECT_CLIP,
		CLIP_VIEWER_VIEWING_CLIP,

		VIDEO_PROJECT_EDITOR_SELECT_PROJECT,

		VIDEO_PROJECT_EDITOR_EDIT_PROJECT,
		VIDEO_PROJECT_EDITOR_PREVIEW_CLIP,
		VIDEO_PROJECT_EDITOR_PREVIEW_PROJECT,
		VIDEO_PROJECT_EDITOR_BAKE_VIDEO,

		AUTO_GENERATE_SELECT_SIZE,
		AUTO_GENERATE_BAKE_VIDEO,

		MAX
	};

	static CUiColorScheme ms_colourScheme;

	static float ms_debugWindowX;
	static float ms_debugWindowY;
	static float ms_debugWindowWidth;
	static float ms_debugWindowHeight;

	static CUiGadgetWindow*				ms_debugWindow;
	static CUiGadgetList*				ms_topLevelMenu;
	static CUiGadgetFileSelector*		ms_fileSelector;
	static CUiGadgetPlaybackControls*	ms_playbackControls;
	static CUiGadgetVideoEditorPane*	ms_videoEditorPane;
	static CUiGadgetFullscreenOverlay*	ms_debugLoading;

	static CRawClipFileView*			ms_clipFileView;
	static CVideoProjectFileView*		ms_videoProjectFileView;
	static CVideoEditorProject*			ms_project;

	static eState	ms_state;
	static bool		ms_enabled;

private: // methods

	static void CreateBankWidgets();

	static void open();
	inline static bool isOpen() { return ms_enabled && ms_debugWindow; }
	static void close();

	static void createTopLevelMenu();
	static void updateTopLevelMenu();
	static void updateTopLevelMenuInput();
	static void cleanupTopLevelMenu();

	static void createFileSelector();
	static void fileMenuCellUpdate( CUiGadgetText* pResult, u32 row, u32 col );
	static void cleanupFileSelector();

	static void createPlaybackControls();
	static void cleanupPlaybackControls();

	static void createVideoEditorPane();
	static void cleanupVideoEditorPane();

	static void setState( eState const stateToSet );

	static void onClipViewerLoadClip( void* callbackData );
	static void onVideoProjectEditorLoad( void* callbackData );

	template< class _gadgetType >
	static void cleanupUiGadget( _gadgetType*& gadgetToCleanup )
	{
		if( gadgetToCleanup )
		{
			if( gadgetToCleanup->GetParent() )
			{
				gadgetToCleanup->DetachFromParent();
			}

			delete gadgetToCleanup;
			gadgetToCleanup = NULL;
		}
	}
};

#endif // __BANK

#endif // DEBUG_VIDEO_EDITOR_SCREEN_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
