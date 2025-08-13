//
// debug/FrameDump.cpp
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
//
// Helper class to dump rendered game frames as individual screenshots.
// Intended particularly for use with cutscenes.

#if __BANK

#include "FrameDump.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/Bar.h"
#include "fwsys/timer.h"
#include "grcore/setup.h"
#include "grmodel/setup.h"



const char* CFrameDump::pathPrefix = NULL;
bool        CFrameDump::useJpeg;
bool        CFrameDump::wasCapturing = false;
bool        CFrameDump::singleFrameCapture = false;
bool		CFrameDump::firstRun = true;
bool        CFrameDump::waitForCutscene = false;
bool		CFrameDump::useCutsceneNameForPath = false;
bool		CFrameDump::MBSync = true;
bool		CFrameDump::binkMode = false;
bool		CFrameDump::cutsceneMode = false;
bool*		CFrameDump::recordCutscene = NULL;


void CFrameDump::StartCapture(const char* pathPrefix, bool useJpeg, bool singleFrame, bool waitForCutscene, bool useCutsceneNameForPath, bool MBSync, bool binkMode, bool cutsceneMode, float fps, bool* recordCutscene)
{
	CFrameDump::useJpeg = useJpeg;
	CFrameDump::pathPrefix = pathPrefix;
	CFrameDump::singleFrameCapture = singleFrame;
    CFrameDump::waitForCutscene = waitForCutscene;
	CFrameDump::useCutsceneNameForPath = useCutsceneNameForPath;
	CFrameDump::MBSync = MBSync;
	CFrameDump::binkMode = binkMode;
	CFrameDump::cutsceneMode = cutsceneMode;
	CFrameDump::recordCutscene = recordCutscene;


#if !__FINAL
#if RSG_PC
	fwTimer::SetForcedFps(fps);
#elif __PS3
	fwTimer::SetForcedFps(fps);
#else
	fwTimer::SetForcedFps(fps);
#endif
#endif // !__FINAL

	CutSceneManager::GetInstance()->SetUseExternalTimeStep(true);
	CutSceneManager::GetInstance()->SetExternalTimeStep(1.0f/fps);
	if ( firstRun )
	{
		CutSceneManager::GetInstance()->SetRenderMBFrame(true);

		// After this point, if you change this through RAG its fine.
		firstRun = false;
	}

	if(binkMode)
	{
		CDebugBar::SetDisplayReleaseDebugText(DEBUG_DISPLAY_STATE_OFF);
		CutSceneManager::GetInstance()->SetRenderMBFrame(false);
	}
	else if(cutsceneMode)
	{
		CDebugBar::SetDisplayReleaseDebugText(DEBUG_DISPLAY_STATE_OFF);
		CutSceneManager::GetInstance()->SetRenderMBFrame(true);
	}

	wasCapturing = true;
}

void CFrameDump::StopCapture()
{
	if (!wasCapturing || singleFrameCapture)
		return;
	wasCapturing = false;

	CDebugBar::SetDisplayReleaseDebugText(DEBUG_DISPLAY_STATE_STANDARD);
	CutSceneManager::GetInstance()->SetRenderMBFrame(false);

	pathPrefix = NULL;
	CutSceneManager::GetInstance()->SetUseExternalTimeStep(false);
	CutSceneManager::GetInstance()->SetExternalTimeStep(0.0f);
#if !__FINAL
	fwTimer::UnsetForcedFps();
#endif

	CutSceneManager* mgr = CutSceneManager::GetInstance();
	if (mgr && mgr->IsCutscenePlayingBack())
		mgr->Play();
}

void CFrameDump::Update()
{
	// The path prefix not being NULL indicates capture is enabled
	if (!pathPrefix)
		return;

	CutSceneManager* mgr = CutSceneManager::GetInstance();

	char cutscenePathPrefix[RAGE_MAX_PATH];
	if(useCutsceneNameForPath)
	{
		strcpy(cutscenePathPrefix, "X:\\framecap\\");
		strcat(cutscenePathPrefix, mgr->GetCutsceneName());
		strcat(cutscenePathPrefix, "\\frame");

		ASSET.CreateLeadingPath(cutscenePathPrefix);

		pathPrefix = cutscenePathPrefix;
	}

	grmSetup* setup = CSystem::GetRageSetup();
	if (setup && !setup->ShouldTakeScreenshot())
	{
        if (mgr->IsCutscenePlayingBack() && !singleFrameCapture)
        {
            static u32 lastFrame = 0xFFFFFFFF;
            u32 currentFrame = mgr->GetMBFrame();
            if (currentFrame != lastFrame || !MBSync)
            {
                lastFrame = currentFrame;
                setup->TakeScreenShot(pathPrefix, useJpeg);
                waitForCutscene = false;

				if (mgr->GetCutSceneCurrentFrame() >= mgr->GetEndFrame())
				{
					*recordCutscene = false;
				}
            }
        }
        else if (!waitForCutscene)
        {
            setup->TakeScreenShot(pathPrefix, useJpeg);
        }

        if (singleFrameCapture)
		{
            pathPrefix = NULL;
			singleFrameCapture = false;
		}
	}

}

#endif
