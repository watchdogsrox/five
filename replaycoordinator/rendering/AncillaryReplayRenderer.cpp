/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : AncillaryReplayRenderer.cpp
// PURPOSE : Provides additional rendering functionality used by the replay
//			 system when encoding a video.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "AncillaryReplayRenderer.h"

#if GTA_REPLAY

// rage
#include "grcore/quads.h"
#include "grcore/viewport.h"
#include "math/amath.h"
#include "vector/color32.h"

// game
#include "renderer/PostProcessFX.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/rendertargets.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "replaycoordinator/replay_coordinator_channel.h"
#include "replaycoordinator/storage/Montage.h"
#include "replaycoordinator/storage/MontageText.h"
#include "ReplayPostFxRegistry.h"
#include "timecycle/TimeCycle.h"

#include "frontend/VideoEditor/ui/TextTemplate.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

static const u8 ms_maxTextLines = 4;
static const u8 ms_maxFrameBuffers = 2;

struct TransitionInfo
{
	TransitionInfo() : m_active( false ) { }

	bool m_active;
	ReplayMarkerTransitionType m_transitionType;
	float m_fraction;
};
static TransitionInfo sm_transitionInfo[ms_maxFrameBuffers];
static u8 ms_transitionInfoFrameBuffer = 0;

struct OverlayTextLineInfo
{
	atString m_text;
	Vector2 m_position;
	Vector2 m_scale;
	CRGBA m_color;
	s32 m_style;
};

#if USE_CODE_TEXT
struct OverlayTextInfo
{
	OverlayTextInfo() : lineCount(0) { }

	u32 lineCount;
	OverlayTextLineInfo m_lineInfo[ms_maxTextLines];
};
static OverlayTextInfo sm_overlayTextInfo[ms_maxFrameBuffers];
static u8 ms_overlayTextInfoFrameBuffer = 0;
#endif // #if USE_CODE_TEXT

rage::grcBlendStateHandle CAncillaryReplayRenderer::sm_textureBlendState = grcStateBlock::BS_Invalid;

#if USE_TEXT_CANVAS
s32 CAncillaryReplayRenderer::s_previousTextIndex = -1;
#endif

void CAncillaryReplayRenderer::ResetOverlays()
{
#if USE_TEXT_CANVAS
    s_previousTextIndex = -1;
    CTextTemplate::UnloadTemplate();
#endif
}

void CAncillaryReplayRenderer::UpdateOverlays( CMontage const& montage, s32 const activeClipIndex, float const clipTime, bool const shouldRender )
{
#if USE_CODE_TEXT
	OverlayTextInfo* overlayTextInfo = &sm_overlayTextInfo[ms_overlayTextInfoFrameBuffer];
#endif // #if USE_CODE_TEXT
	CMontageText const * activeText = NULL;

	if (shouldRender)
	{
		s32 activeTextIndex = montage.GetActiveTextIndex( activeClipIndex, clipTime );

#if USE_TEXT_CANVAS
		if (activeTextIndex >= 0)
		{
			if (activeTextIndex != s_previousTextIndex)
			{
#endif // #if USE_TEXT_CANVAS
				activeText = ( activeTextIndex < 0 || (size_t)activeTextIndex > montage.GetTextCount() ) ? 
														NULL : montage.GetText( activeTextIndex );
#if USE_TEXT_CANVAS
				s_previousTextIndex = activeTextIndex;
			}
		}
		else
		{
			s_previousTextIndex = -1;
			CTextTemplate::UnloadTemplate();
		}
#endif // #if USE_TEXT_CANVAS

	}
#if USE_TEXT_CANVAS
	else
	{
		s_previousTextIndex = -1;
		CTextTemplate::UnloadTemplate();
	}
#endif // #if USE_TEXT_CANVAS

	if( activeText )
	{
#if USE_CODE_TEXT
		overlayTextInfo->lineCount = activeText->GetMaxLineCount();
		for( u32 lineIndex = 0; lineIndex < overlayTextInfo->lineCount; ++lineIndex )
		{
			if( activeText->IsLineSet( lineIndex ) )
			{
				// dont render text that starts on last ms of 1st clip if there is another clip after (fixes 2355689)
				float const c_textStartTime = (float)activeText->getStartTimeMs();
				float const c_clipEndTimeMs = montage.GetTrimmedTimeToClipMs(activeClipIndex) + montage.GetClip(activeClipIndex)->GetTrimmedTimeMs();

				if ( activeClipIndex == montage.GetClipCount()-1 || c_textStartTime != c_clipEndTimeMs )
				{
					overlayTextInfo->m_lineInfo[lineIndex].m_text = activeText->getText( lineIndex );
					overlayTextInfo->m_lineInfo[lineIndex].m_position = activeText->getTextPosition( lineIndex );
					overlayTextInfo->m_lineInfo[lineIndex].m_scale = activeText->getTextScale( lineIndex );
					overlayTextInfo->m_lineInfo[lineIndex].m_color = activeText->getTextColor( lineIndex );
					overlayTextInfo->m_lineInfo[lineIndex].m_style = activeText->getTextStyle( lineIndex );
				}
			}
		}
#endif // #if USE_CODE_TEXT

#if USE_TEXT_CANVAS
		sEditedTextProperties canvasText[MAX_TEXT_LINES];

		s32 const c_lineCount = activeText->GetMaxLineCount();

		s32 usedLineCount = 0;
		for (s32 lineIndex = 0; lineIndex < c_lineCount; lineIndex++)
		{
			if( activeText->IsLineSet( lineIndex ) )
			{
				// dont render text that starts on last ms of 1st clip if there is another clip after (fixes 2355689)
				float const c_textStartTime = (float)activeText->getStartTimeMs();
				float const c_clipEndTimeMs = montage.GetTrimmedTimeToClipMs(activeClipIndex) + montage.GetClip(activeClipIndex)->GetTrimmedTimeMs();

				if ( activeClipIndex == montage.GetClipCount()-1 || c_textStartTime != c_clipEndTimeMs )
				{
					usedLineCount++;

					strcpy(canvasText[lineIndex].m_text, activeText->getText( lineIndex ));
					canvasText[lineIndex].m_position = activeText->getTextPosition( lineIndex );
					canvasText[lineIndex].m_scale = activeText->getTextScale( lineIndex ).y;
					CRGBA colorRGB = activeText->getTextColor( lineIndex );
					colorRGB.SetAlpha(255);
					canvasText[lineIndex].m_hudColor = CHudColour::GetColourFromRGBA(colorRGB);
					canvasText[lineIndex].m_colorRGBA = activeText->getTextColor( lineIndex );
					canvasText[lineIndex].m_style = activeText->getTextStyle( lineIndex );
					canvasText[lineIndex].m_alpha = activeText->getTextColor( lineIndex ).GetAlphaf();
				}
			}
		}

		// text templates currently only support 2 lines...
		if (usedLineCount == 1)
		{
			CTextTemplate::SetupTemplate(canvasText[0]);
		}
		else if (usedLineCount >= 2)
		{
			CTextTemplate::SetupTemplate(canvasText[0], canvasText[1]);
		}
#endif // USE_TEXT_CANVAS
	}
	else
	{
#if USE_CODE_TEXT
		overlayTextInfo->lineCount = 0;
#endif // #if USE_CODE_TEXT
	}

#if USE_CODE_TEXT
	ms_overlayTextInfoFrameBuffer = (++ms_overlayTextInfoFrameBuffer) % ms_maxFrameBuffers;
#endif // #if USE_CODE_TEXT
}

void CAncillaryReplayRenderer::RenderOverlays( )
{
// keep function since we may have overlays other than just text in future
#if USE_CODE_TEXT
	u8 frameBuffer = (ms_overlayTextInfoFrameBuffer+1) % ms_maxFrameBuffers;
	OverlayTextInfo* overlayTextInfo = &sm_overlayTextInfo[frameBuffer];
	if( overlayTextInfo->lineCount > 0 )
	{
		for( u32 lineIndex = 0; lineIndex < overlayTextInfo->lineCount; ++lineIndex )
		{
			OverlayTextRenderer::RenderText( overlayTextInfo->m_lineInfo[lineIndex].m_text.c_str(), 
				overlayTextInfo->m_lineInfo[lineIndex].m_position, 
				overlayTextInfo->m_lineInfo[lineIndex].m_scale, 
				overlayTextInfo->m_lineInfo[lineIndex].m_color, 
				overlayTextInfo->m_lineInfo[lineIndex].m_style, 
				Vector2(0.0f, 1.0f) );
		}	
	}
#endif // USE_CODE_TEXT
}

void CAncillaryReplayRenderer::UpdateTransitions( CMontage const& montage, s32 const activeClipIndex, float const clipTime )
{
	TransitionInfo* transitionInfo = &sm_transitionInfo[ms_transitionInfoFrameBuffer];

	if( montage.IsValidClipIndex( activeClipIndex ) )
	{
		transitionInfo->m_active = true;
		montage.GetActiveTransition( activeClipIndex, clipTime, transitionInfo->m_transitionType, transitionInfo->m_fraction );
	}
	else
	{
		transitionInfo->m_active = false;
	}

	ms_transitionInfoFrameBuffer = (++ms_transitionInfoFrameBuffer) % ms_maxFrameBuffers;
}

void CAncillaryReplayRenderer::RenderTransitions( )
{
	u8 frameBuffer = (ms_transitionInfoFrameBuffer+1) % ms_maxFrameBuffers;
	TransitionInfo* transitionInfo = &sm_transitionInfo[frameBuffer];

	if( transitionInfo->m_active )
	{
		if( transitionInfo->m_transitionType != MARKER_TRANSITIONTYPE_MAX && 
			transitionInfo->m_transitionType != MARKER_TRANSITIONTYPE_NONE )
		{
			GRC_ALLOC_SCOPE_AUTO_PUSH_POP();

			grcBlendStateHandle prevBlendState = grcStateBlock::BS_Active;
			grcDepthStencilStateHandle prevDepthStencilState = grcStateBlock::DSS_Active;

			grcStateBlock::SetBlendState( sm_textureBlendState );
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);

			// TODO - Once bitmap support is in we want to move the default screen to wrap bitmap and transition rendering
			PUSH_DEFAULT_SCREEN();
			RenderFadeInternal( (float)VideoResManager::GetUIWidth(), (float)VideoResManager::GetUIHeight(), transitionInfo->m_fraction );
			POP_DEFAULT_SCREEN();

			// restore blend state
			grcStateBlock::SetBlendState(prevBlendState);
			grcStateBlock::SetDepthStencilState(prevDepthStencilState);
		}
	}
}

void CAncillaryReplayRenderer::EnableReplayDefaultEffects()
{
	g_timeCycle.EnableReplayExtraEffects();
}

void CAncillaryReplayRenderer::DisableReplayDefaultEffects()
{
	g_timeCycle.DisableReplayExtraEffects();
}

void CAncillaryReplayRenderer::UpdateActiveReplayFx( CMontage const& montage, s32 const activeClipIndex, float const clipTime )
{
	s32 activeEffect = INDEX_NONE;
	float effectIntensity = 0.f;
	float saturation = 0.f;
	float contrast = 0.f;
	float brightness = 0.f;
	float vignette = 0.f;

	bool const c_hasEffect = montage.GetActiveEffectParameters( activeClipIndex, clipTime, activeEffect, 
										effectIntensity, saturation, contrast, brightness, vignette );

	g_timeCycle.SetReplaySaturationIntensity( saturation / 100.f );
	g_timeCycle.SetReplayContrastIntensity( contrast / 100.f );
	g_timeCycle.SetReplayBrightnessIntensity( brightness / 100.f );
	g_timeCycle.SetReplayVignetteIntensity( vignette / 100.f );

	CReplayPostFxData const* fxData = c_hasEffect ? CReplayCoordinator::GetPostFxRegistry().GetEffect( (u32)activeEffect ) : NULL;
	static CReplayPostFxData const* previousFxData = NULL;

	if( fxData && activeEffect > 0 )
	{
		g_timeCycle.SetReplayModifierHash( fxData->GetNameHash(), effectIntensity / 100.f );
	}
	else
	{
		g_timeCycle.SetReplayModifierHash( 0, 0.f );
	}

	if(previousFxData != fxData)
	{
		PostFX::ResetAdaptedLuminance();
	}

	previousFxData = fxData;
}

void CAncillaryReplayRenderer::ResetActiveReplayFx()
{
	PostFX::ResetAdaptedLuminance();
	g_timeCycle.SetReplayModifierHash( 0, 0.f );
}

void CAncillaryReplayRenderer::InitializeTextureBlendState()
{
	if( sm_textureBlendState != grcStateBlock::BS_Invalid )
		return;

	grcBlendStateDesc blendStateBlockDesc;

	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendEnable = 1;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendOp = grcRSV::BLENDOP_ADD;
	blendStateBlockDesc.IndependentBlendEnable = 1;

	sm_textureBlendState = grcStateBlock::CreateBlendState(blendStateBlockDesc);
}

void CAncillaryReplayRenderer::RenderFadeInternal( float const width, float const height, float const fraction )
{
	// TODO - Fades to other colours?
	RenderQuadInternal( width, height, rage::Color32( 0.f, 0.f, 0.f, fraction ), NULL );
}

void CAncillaryReplayRenderer::RenderQuadInternal( float const width, float const height, rage::Color32 const& colour, rage::grcTexture* texture /*= NULL */ )
{
	grcBindTexture( texture );
	grcDrawSingleQuadf( 0, 0, width, height, 0.f, 0.f, 0.f, 1.f, 1.f, colour );
}

#endif // GTA_REPLAY
