/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : AncillaryReplayRenderer.h
// PURPOSE : Provides additional rendering functionality used by the replay
//			 system when encoding a video.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#ifndef ANCILLARY_REPLAY_RENDERER_H_
#define ANCILLARY_REPLAY_RENDERER_H_

// rage
#include "grcore/stateblock.h"

// game
#include "frontend/VideoEditor/ui/TextTemplate.h"

namespace rage
{
	class Color32;
	class grcTexture;
}

class CMontage;

class CAncillaryReplayRenderer
{
public: // methods

    static void ResetOverlays();

	static void UpdateOverlays( CMontage const& montage, s32 const activeClipIndex, float const clipTime, bool const shouldRender );
	static void UpdateTransitions( CMontage const& montage, s32 const activeClipIndex, float const clipTime );

	//! Render Functionality
	static void RenderOverlays();
	static void RenderTransitions();

	static void EnableReplayDefaultEffects();
	static void DisableReplayDefaultEffects();

	static void UpdateActiveReplayFx( CMontage const& montage, s32 const activeClipIndex, float const clipTime );
	static void ResetActiveReplayFx();

	static void InitializeTextureBlendState();

private: // declarations and variables
	static grcBlendStateHandle	sm_textureBlendState;

#if USE_TEXT_CANVAS
    static s32 s_previousTextIndex;
#endif // #if USE_TEXT_CANVAS

private: // methods

	static void RenderFadeInternal( float const width, float const height, float const fraction );
	static void RenderQuadInternal( float const width, float const height, rage::Color32 const& colour, rage::grcTexture* texture = NULL );

};

#endif // ANCILLARY_REPLAY_RENDERER_H_

#endif // GTA_REPLAY
