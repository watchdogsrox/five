/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BinkPageView.cpp
// PURPOSE : Page view that renders a configured bink.
//
// AUTHOR  : brian.beacom
// STARTED : September 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "BinkPageView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "BinkPageView_parser.h"

// framework
#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "audiosoundtypes/externalstreamsound.h"
#include "audiosoundtypes/sound.h"
// game
#include "frontend/BinkDefs.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "script/commands_graphics.h"
#include "vfx/misc/MovieManager.h"

PARAM(skipLandingPageBinks, "Skip all binks in landing page UI framework");

FWUI_DEFINE_TYPE(CBinkPageView, 0xA296B734);

void CBinkPageView::OnEnterStartDerived()
{
#if __BANK
	if (PARAM_skipLandingPageBinks.Get())
	{
		// If movie doesn't load properly we just bail
		uiDisplayf("CBinkPageView::OnEnterStartDerived - Deliberately skipping bink as skipLandingPageBinks is set.");
		return;
	}
#endif

	m_movie = bwMovie::Create();
	m_movie->SetControlledByAnother();
#if RSG_PC
	// if there was no audio device when the game started the bink may have not have re-initialized correctly
	if (!bwMovie::IsUsingRAGEAudio())
	{
		bwMovie::Init();
	}
#endif
	bwMovie::bwMovieParams movieParams;
	safecpy(movieParams.pFileName, m_movieName.c_str());
	movieParams.extraFlags = 0;
	movieParams.ioSize = FRONTEND_BINK_READ_AHEAD_BUFFER_SIZE;
	
	if (uiVerifyf(m_movie->SetMovieSync(movieParams), "CBinkPageView::OnEnterStartDerived - Movie could not be set"))
	{
		m_movie->SetLoaded(true);
		m_movie->SetLooping(false);
		CreateSound();
		u32 timeMs = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		m_movie->Play(timeMs);
	}
}


void CBinkPageView::CreateSound()
{
#if IS_GEN9_PLATFORM
	g_FrontendAudioEntity.StopLandingPageTune(false);
#endif
	// TODO:url:bugstar:7293941: This should mute the frontend music making the above redundant however at present it also mutes the bink, audio scene needs set up
	//DYNAMICMIXER.StartScene(ATSTRINGHASH("GEN9_LANDING_PAGE_SCENE", 0xCB2287E9), &m_binkPlaybackScene);
	if (bwMovie::IsUsingRAGEAudio())
	{
		audDisplayf("CBinkPageView::CreateSound - Trying to play bink audio using game engine");
		const bwAudio::Info* info = m_movie->FindAudio();
		if (info && info->ringBuffer)
		{
			audSoundInitParams initParams;

			Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());

			// for now force the sound to play with a 2d pan
#if IS_GEN9_PLATFORM && AUD_GEN9_AMBISONICS
			initParams.HorizontalPan = 0;
#else
			initParams.Pan = 0;
#endif // AUD_GEN9_AMBISONICS
			audDisplayf("CBinkPageView::CreateSound - Trying to play bink audio FE using game engine");

			const char* soundName = info->numChannels == 1 ? "BINK_MONO_SOUND" : "BINK_STEREO_SOUND";

			g_FrontendAudioEntity.CreateSound_PersistentReference(soundName, &m_pSound, &initParams);

			if (m_pSound)
			{
				Assert(m_pSound->GetSoundTypeID() == ExternalStreamSound::TYPE_ID);

				audExternalStreamSound* str = reinterpret_cast<audExternalStreamSound*>(m_pSound);

				str->InitStreamPlayer(info->ringBuffer, info->numChannels, info->sampleRate);
				str->PrepareAndPlay();
				audDisplayf("CBinkPageView::CreateSound - Playing bink audio using game engine");
			}
		}
		m_movie->SetVolume(-6.f);
	}
	else
	{
		audWarningf("CBinkPageView::CreateSound - Trying to play bink audio w/o game engine - is loud by default so setting down");
		m_movie->SetVolume(-28.f);
	}
	
}


void CBinkPageView::UpdateDerived(float const UNUSED_PARAM(deltaMs))
{
	if (!IsViewFocused())
	{
		return;
	}

	if (m_movie)
	{
		u32 timeMs = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		if (m_movie->IsLoaded() && m_movie->GetFramesRemaining() > 0)
		{
			m_movie->UpdateMovieTime(timeMs);
			
		}
		else if (m_movie->IsPlaying())
		{
			m_movie->Stop();
			uiDebugf1("CBinkPageView::UpdateDerived - Bink has finished.");
		}
	}

	if (!m_movie || (!m_movie->IsPlaying() && !m_movie->AreFencesPending()))
	{
		uiDisplayf("CBinkPageView::UpdateDerived - Cleaning up bink memory.");
		// OnExit methods aren't called as long as screen as onstack but binks are one-shot, so release resources now
		// Audio teardown
		if (m_pSound)
		{
			m_pSound->StopAndForget();
			m_pSound = NULL;
		}
		if (m_binkPlaybackScene)
		{
			m_binkPlaybackScene->Stop();
			m_binkPlaybackScene = NULL;
		}
#if IS_GEN9_PLATFORM
		g_FrontendAudioEntity.StartLandingPageTune();
#endif
		// Release bink
		if (m_movie)
		{
			bwMovie::Destroy(m_movie, false);
			m_movie = NULL;
		}

		// Navigation
		IPageViewHost& viewHost = GetViewHost();
		if (m_onFinishMessage)
		{
			viewHost.HandleMessage(*m_onFinishMessage);
		}
		else
		{
			uiPageDeckBackMessage backMsg;
			viewHost.HandleMessage(backMsg);
		}
		m_isCleanedUp = true;
	}
}


void CBinkPageView::RenderDerived() const
{
	if (!m_movie || !m_movie->IsPlaying())
	{
		return;
	}
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

#if SUPPORT_MULTI_MONITOR
	// BB@DND: This is untested in Gen9 but used elsewhere and looks sensible 
	const GridMonitor& mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
	float width = (float)mon.getWidth();
	float height = width * ((float)1080.0f / (float)1920.0f);
	float offsetX = (float)mon.uLeft;
	float offsetY = mon.uTop + 0.5f * (mon.getHeight() - height);
#else //SUPPORT_MULTI_MONITOR
	float width = (float)VideoResManager::GetNativeWidth();
	float height = width * ((float)1080.0f / (float)1920.0f);
	float offsetX = 0.f;
	float offsetY = ((float)VideoResManager::GetNativeHeight() - height) / 2.f;
#endif //SUPPORT_MULTI_MONITOR

	Vector2 p1(offsetX, offsetY + height);
	Vector2 p2(offsetX, offsetY);
	Vector2 p3(offsetX + width, offsetY + height);
	Vector2 p4(offsetX + width, offsetY);
	Color32 color = Color_white;

	GRCDBG_PUSH("LPBink");
	if (m_movie)
	{

		m_movie->BeginDraw(RSG_DURANGO ? false : true);
		CSprite2d::DrawSwitch(false, false, 0.f, p1, p2, p3, p4, color);

		if (m_movie->IsWithinBlackout())
		{
			CSprite2d::DrawSwitch(false, false, 0.f, p1, p2, p3, p4, Color_black);
		}

		m_movie->EndDraw();
		bwMovie::ShaderEndDraw(m_movie);
	}
	GRCDBG_POP();
	
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
}

#endif //  GEN9_LANDING_PAGE_ENABLED
