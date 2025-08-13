//
// system/StreamingInstall.winrt.cpp
//
// Copyright (C) 2014 Rockstar Games.  All Rights Reserved.
//

#include "StreamingInstall.winrt.h"

#if RSG_ORBIS
#include "audio/northaudioengine.h"
#include "system/controlMgr.h"
#include "frontend/WarningScreen.h"
#endif // RSG_ORBIS
#include "frontend/loadingscreens.h"
#include "fwrenderer/renderthread.h"
#include "grcore/debugdraw.h"
#include "grcore/light.h"
#include "grcore/setup.h"
#include "grcore/stateblock.h"
#include "grcore/viewport.h"
#include "grprofile/timebars.h"
#include "grmodel/setup.h"
#include "network/Network.h"
#include "renderer/color.h"
#include "renderer/sprite2d.h"
#include "system/system.h"
#include "text/text.h"

#if RSG_ORBIS
#include <libsysmodule.h>
#include <playgo.h>
#pragma comment(lib, "libScePlayGo_stub_weak.a")

ScePlayGoHandle s_playGoHandle = -1;
ScePlayGoInitParams s_playGoParams;
u32 s_numChunks = 0;
ScePlayGoChunkId s_chunkIds[100] = {0};
#elif RSG_DURANGO
#using <windows.winmd>
Windows::Xbox::Management::Deployment::PackageTransferWatcher^ s_playGoTransferWatcher = nullptr;
// windows.winmd plays havoc with the game code for some reason, so #including northaudioengine.h here causes weird errors
void InitialiseAudioForInstall();
#endif

u32 s_playGoPercentComplete = 0;

static grcDepthStencilStateHandle s_installerState_DS;
static grcRasterizerStateHandle s_installerState_R;
static grcBlendStateHandle s_installerState_B;

bool StreamingInstall::ms_installedThisSession = false;

#if RSG_DURANGO

Windows::Foundation::EventRegistrationToken s_ProgressToken;
Windows::Foundation::EventRegistrationToken s_ProcessCompleteToken;

// Init Handler
void StreamingInstall::InitTransferPackageWatcher()
{
	s_playGoPercentComplete = 0;

	s_playGoTransferWatcher = Windows::Xbox::Management::Deployment::PackageTransferWatcher::Create(Windows::ApplicationModel::Package::Current);

	Displayf("[PLAYGO] TransferWatcher Initialized");

	s_ProgressToken = s_playGoTransferWatcher->ProgressChanged += ref new Windows::Foundation::TypedEventHandler<Windows::Xbox::Management::Deployment::PackageTransferWatcher^, Windows::Xbox::Management::Deployment::ProgressChangedEventArgs^>(
		[&] (Windows::Xbox::Management::Deployment::PackageTransferWatcher^ ptm, Windows::Xbox::Management::Deployment::ProgressChangedEventArgs^ args)
	{
		Displayf("[PLAYGO] TransferWatcher::ProgressChanged Called");
		s_playGoPercentComplete = args->PercentComplete;
	});

//	s_ProcessCompleteToken = s_playGoTransferWatcher->TransferCompleted += ref new Windows::Foundation::TypedEventHandler<Windows::Xbox::Management::Deployment::PackageTransferWatcher^, Windows::Xbox::Management::Deployment::TransferCompletedEventArgs^>(
//		[&] (Windows::Xbox::Management::Deployment::PackageTransferWatcher^ ptm, Windows::Xbox::Management::Deployment::TransferCompletedEventArgs^)
//	{
//		Displayf("[PLAYGO] TransferWatcher::TransferCompleted Called");
//		s_playGoPercentComplete = 100;
//	});

	ms_installedThisSession = s_playGoPercentComplete > 0 && s_playGoPercentComplete < 100;
}

// Shutdown Handler
void StreamingInstall::ShutdownTransferPackageWatcher()
{
	if( s_playGoTransferWatcher != nullptr)
	{
		s_playGoTransferWatcher->ProgressChanged -= s_ProgressToken;
		s_playGoTransferWatcher->TransferCompleted -= s_ProcessCompleteToken;
		s_playGoTransferWatcher = nullptr;
	}
}

// Resume Delegate
static ServiceDelegate	s_StreamingInstallResumeFromSuspendDelegate;

// The actual delegate
void sysStreamingInstallResumeFromSuspend(sysServiceEvent* evnt)
{
	if(evnt != NULL)
	{
		switch(evnt->GetType())
		{
		case sysServiceEvent::RESUME_IMMEDIATE:
			// Shutdown Handler
			StreamingInstall::ShutdownTransferPackageWatcher();
			// And Start it again
			StreamingInstall::InitTransferPackageWatcher();
			break;

		default:
			break;
		}
	}
}


// Added Delegate
void	StreamingInstall::AddResumeDelegate()
{
	s_StreamingInstallResumeFromSuspendDelegate.Bind(&sysStreamingInstallResumeFromSuspend);
	g_SysService.AddDelegate(&s_StreamingInstallResumeFromSuspendDelegate);
}

// Remove Delegate
void	StreamingInstall::RemoveResumeDelegate()
{
	g_SysService.RemoveDelegate(&s_StreamingInstallResumeFromSuspendDelegate);
	s_StreamingInstallResumeFromSuspendDelegate.Unbind();
}

#endif	//RSG_DURANGO


bool StreamingInstall::Init()
{
#if RSG_ORBIS
	if (sceSysmoduleLoadModule(SCE_SYSMODULE_PLAYGO) != SCE_OK) 
	{
		Quitf("Failed to load PlayGo module!");
	}

	s_playGoParams.bufAddr = new u8[SCE_PLAYGO_HEAP_SIZE];
	s_playGoParams.bufSize = SCE_PLAYGO_HEAP_SIZE;
	s_playGoParams.reserved = 0;

	if (scePlayGoInitialize(&s_playGoParams) != SCE_OK)
	{
        Displayf("[PLAYGO] Failed to initialize PlayGo library!");
	}

	s_numChunks = 0;
	if (scePlayGoOpen(&s_playGoHandle, NULL) != SCE_OK)
	{
        s_playGoHandle = -1;
        Displayf("[PLAYGO] Failed to open PlayGo package!");
	}
	else
	{
        ScePlayGoToDo chunks[100];
        if (scePlayGoGetToDoList(s_playGoHandle, chunks, 100, &s_numChunks) != SCE_OK)
		{
			Displayf("[PLAYGO] Failed to get PlayGo ToDo list!");
		}
        Assertf(s_numChunks <= 100, "Too many PlayGo chunks, increase the array size!");

        // numChunks will be set to the number of PlayGo chunks that are still scheduled to change their locus.
        // i.e. they need to be copied somewhere. the copy only happens psn->hdd or odd->hdd, and in either way we need
        // to wait for the copy to finish before we can play the game, since we don't support streaming from odd
        // while playing.
        Displayf("[PLAYGO] Found %d PlayGo chunks in the ToDo list", s_numChunks);

        // save chunk ids so we can later check the progress
		for (u32 i = 0; i < s_numChunks; ++i)
			s_chunkIds[i] = chunks[i].chunkId;
	}

    ms_installedThisSession = s_numChunks > 0;

	return s_numChunks == 0;
#elif RSG_DURANGO

	// Start the package transfer watcher
	InitTransferPackageWatcher();

	// Add the ResumeFromSuspend Delegate
	AddResumeDelegate();

    // when 0, we don't even have the initial payload installed, meaning we run a non-packaged
    // build (streamed or deployed). this shouldn't happen in retail since we can't boot the game until the
    // // initial payload is installed, putting the percentage at >0
	Displayf("[PLAYGO] Percent complete: %d", s_playGoPercentComplete);

    return s_playGoPercentComplete == 0 || s_playGoPercentComplete >= 100;
#else
    return true;
#endif
}

bool StreamingInstall::HasInstallFinished()
{
	if (!ms_installedThisSession)
		return true;
#if RSG_ORBIS
    if (s_playGoHandle == -1)
        return true;

	ScePlayGoProgress progress;
	s32 ret = scePlayGoGetProgress(s_playGoHandle, s_chunkIds, s_numChunks, &progress);
	if (ret != SCE_OK )
	{
		Displayf("[PLAYGO] Failed to get progress for PlayGo chunks! (%d)", ret);
		return true;
	}

    s_playGoPercentComplete = (u32)(100.f * progress.progressSize / (float)progress.totalSize);

	Displayf("[PLAYGO] Progress: %lu / %lu", progress.progressSize, progress.totalSize);
    return progress.progressSize >= progress.totalSize;
#elif RSG_DURANGO
	Displayf("[PLAYGO] Progress: %d%%", s_playGoPercentComplete);
    return s_playGoPercentComplete >= 100;
#else
    return true;
#endif
}

void StreamingInstall::BlockUntilFinished()
{
    if (HasInstallFinished())
        return;

#if RSG_ORBIS
	CLoadingScreens::SleepUntilPastLegalScreens();
	CLoadingScreens::Suspend();

	while(true)
	{
		PF_FRAMEINIT_TIMEBARS(0);

		if(UpdateSleepWarning())
		{
			CWarningScreen::Update();
			break;
		}

		fwTimer::Update();
		CControlMgr::Update();
		CWarningScreen::Update();

		PF_FRAMEEND_TIMEBARS();

		sysIpcSleep(16);
	}
	CLoadingScreens::Resume();
#endif	// RSG_ORBIS

	// we need our own render function to flip the buffers while we block for the install to finish
	RenderFn oldRenderFunc = gRenderThreadInterface.GetRenderFunction();
	gRenderThreadInterface.SetRenderFunction(MakeFunctor(RenderInstallProgress));
	gRenderThreadInterface.Flush();

#if RSG_DURANGO
	sysTimer audioTimer;
	bool hasInitialisedAudio = false;
#endif

	while (!HasInstallFinished())
	{
		CNetwork::Update(true);

		CLoadingScreens::KeepAliveUpdate();

		sysIpcSleep(16);

#if RSG_DURANGO
		// Wait until after the bink movie, then initialise audio to start the loading music
		if(audioTimer.GetTime() > 15.f && !hasInitialisedAudio)
		{
			InitialiseAudioForInstall();
			hasInitialisedAudio = true;
		}
#endif // RSG_DURANGO
	}


	gRenderThreadInterface.SetRenderFunction(oldRenderFunc);
	gRenderThreadInterface.Flush();
}

void StreamingInstall::RenderInstallProgress()
{
	if (CLoadingScreens::GetLegalMainMovieIndex() == LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL)
	{
		if (CLoadingScreens::GetCurrentContext() != LOADINGSCREEN_CONTEXT_LEGALMAIN) // Only override the actual loading screen (not the legal text)
		{
			char infoStr[256] = {0};
			formatf(infoStr, "%d %s", s_playGoPercentComplete, "%");
            float percentComplete = (float)s_playGoPercentComplete;
			CLoadingScreens::SetInstallerInfo(percentComplete, infoStr);
		}

		CLoadingScreens::RenderLoadingScreens();
		return;
	}

	PF_FRAMEINIT_TIMEBARS(0);
	TIME.Update();

	CSystem::BeginRender();

	const grcViewport* pVp = grcViewport::GetDefaultScreen();

	GRCDEVICE.Clear(true, Color32(0,0,0), false, 0, false, 0);

	if (!s_installerState_R)
		CreateStateBlocks();

	grcViewport::SetCurrent(pVp);
	grcStateBlock::SetRasterizerState(s_installerState_R);
	grcStateBlock::SetBlendState(s_installerState_B);
	grcStateBlock::SetDepthStencilState(s_installerState_DS);
	grcLightState::SetEnabled(false);
	grcWorldIdentity();

	CTextLayout textLayout;
	Vector2 textPos(0.5f, 0.6f);
	Vector2 textSize = Vector2(0.33f, 0.4785f);
	CRGBA textColor(100, 250, 100, 255);
	textLayout.SetScale(&textSize);
	textLayout.SetColor(textColor);
	textLayout.SetOrientation(FONT_CENTRE);

	char debugText[1024];
	formatf(debugText, "Installing content: %d%s", s_playGoPercentComplete, "%");
	textLayout.Render(&textPos, debugText);


	CText::Flush();

	Vector2 sliderSize(400.f, 20.f);
	Vector2 totalSliderPos(440.f, 460.f);
	Vector2 fileSliderPos(440.f, 535.f);

	CRGBA shadow(0, 0, 0, 100);
	CRGBA front(100, 250, 100, 250);
	CRGBA back(80, 200, 80, 80);

	CSprite2d::DrawRect(fwRect(totalSliderPos.x, totalSliderPos.y, (totalSliderPos.x + sliderSize.x), (totalSliderPos.y + sliderSize.y)), back);  // back of slider
	float totalSliderValue = sliderSize.x * (10.f / 100.f);
	CSprite2d::DrawRect(fwRect(totalSliderPos.x, totalSliderPos.y, (totalSliderPos.x + totalSliderValue), (totalSliderPos.y + sliderSize.y)), front);  // front of slider

	CSprite2d::DrawRect(fwRect(fileSliderPos.x, fileSliderPos.y, (fileSliderPos.x + sliderSize.x), (fileSliderPos.y + sliderSize.y)), back);  // back of slider
	float fileSliderValue = sliderSize.x * (5.f / 10.f);
	CSprite2d::DrawRect(fwRect(fileSliderPos.x, fileSliderPos.y, (fileSliderPos.x + fileSliderValue), (fileSliderPos.y + sliderSize.y)), front);  // front of slider

	grcViewport::SetCurrent(NULL);
	CSystem::EndRender();
}

#if RSG_ORBIS
bool StreamingInstall::UpdateSleepWarning()
{
	bool bAccepted = false;

	CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, "WARN_SLEEP_TIMER", static_cast<eWarningButtonFlags>(FE_WARNING_OK | FE_WARNING_NOSOUND));
	if (CWarningScreen::CheckAllInput(false) == FE_WARNING_OK)
	{
		CWarningScreen::Remove();

		if(!audNorthAudioEngine::InitClass())
		{
			Errorf("Failed to initialise audio");
		}

		bAccepted = true;
	}

	return bAccepted;
}
#endif // RSG_ORBIS


void StreamingInstall::CreateStateBlocks()
{
	// state blocks for install screen (create on first use
	grcRasterizerStateDesc rsd;
	rsd.CullMode = grcRSV::CULL_NONE;
	rsd.FillMode = grcRSV::FILL_SOLID;
	rsd.HalfPixelOffset = 1;
	s_installerState_R = grcStateBlock::CreateRasterizerState(rsd);

	grcBlendStateDesc bsd;
	bsd.BlendRTDesc[0].BlendEnable = 1;
	bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	s_installerState_B = grcStateBlock::CreateBlendState(bsd);

	grcDepthStencilStateDesc dssd;
	dssd.DepthWriteMask = 0;
	dssd.DepthEnable = 0;
	s_installerState_DS = grcStateBlock::CreateDepthStencilState(dssd);
}

void StreamingInstall::Shutdown()
{
#if RSG_ORBIS
    if (s_playGoHandle == -1)
        return;

	Displayf("[PLAYGO] Closing PlayGo handle.");
	if (scePlayGoClose(s_playGoHandle) != SCE_OK)
	{
		Displayf("[PLAYGO] Failed to close PlayGo handle!");
	}

	if (scePlayGoTerminate() != SCE_OK)
	{
		Displayf("[PLAYGO] Terminating PlayGo library.");
	}

    s_playGoHandle = -1;

	delete[] (u8*)s_playGoParams.bufAddr;
	s_playGoParams.bufSize = 0;

    if (sceSysmoduleIsLoaded(SCE_SYSMODULE_PLAYGO) == SCE_SYSMODULE_LOADED)
	{
        if (sceSysmoduleUnloadModule(SCE_SYSMODULE_PLAYGO) != SCE_OK)
		{
            Displayf("[PLAYGO] Failed to unload PlayGo module!");
		}
	}
#elif RSG_DURANGO
	RemoveResumeDelegate();
	ShutdownTransferPackageWatcher();
#endif
}
