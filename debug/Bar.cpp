//
// debug/bar.cpp
//
// Copyright (C) 1999-2014 Rockstar North.  All Rights Reserved. 
//

#include "debug/bar.h"

#if FINAL_DISPLAY_BAR

// rage
#include "file/device.h"
#include "file/device_installer.h"
#include "grcore/allocscope.h"
#include "grcore/debugdraw.h"
#include "system/bootmgr.h"
#include "system/ipc.h"
#if __WIN32PC
#include "input/keyboard.h"
#include "grcore/diag_d3d.h"
#endif

//framework
#include "fwnet/netremotelog.h"
// game
#include "debug/blockview.h"
#include "debug/LightProbe.h"
#include "debug/SceneGeometryCapture.h"
#include "control/gamelogic.h"
#include "cutscene/cutscenemanagernew.h"
#include "game/clock.h"
#include "game/weather.h"
#include "game/zones.h"
#include "frontend/hudtools.h"
#include "frontend/loadingscreens.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingrequestlist.h"
#include "system/controlmgr.h"
#include "system/settingsmanager.h"
#include "system/ThreadPriorities.h"
#include "text/textconversion.h"
#include "vehicles/vehiclepopulation.h"
#include "Network/Network.h"
#include "Network/Bandwidth/NetworkBandwidthManager.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "modelinfo\PedModelInfo.h"
#include "streaming/populationstreaming.h"

PARAM(debugdisplaystate, "[debug] Control what debug information to display! Use with a value (debugdisplaystate=off|standard|coords_only).");
PARAM(nodisplayframecount, "[debug] Don't display the current frame count in RenderReleaseInfoToScreen()");
PARAM(nodisplayconvergence, "[debug] Don't display the current convergence in RenderReleaseInfoToScreen()");
PARAM(displaykeyboardmode, "[debug] Don't display the current keyboard mode in RenderReleaseInfoToScreen()");
PARAM(displayDebugBarMemoryInKB, "[debug] Display the memory in KB instead of MB in RenderReleaseInfoToScreen()");
PARAM(fpsborder, "[debug] Draw a colored border representing FPS threshold in RenderReleaseInfoToScreen()");
PARAM(smoothframerate, "Smooth the FPS display");
PARAM(disableTodDebugModeDisplay, "Disables additional debug character for clock / weather");
PARAM(displayDebugBarDisplayMemoryPeaks, "[debug] Display the script memory peaks RenderReleaseInfoToScreen()");

#if RSG_PC && __BANK
PARAM(displayVidMem, "[debug] Display the amount of video memory in use and the total available");
PARAM(demobuild, "makes modifications specifically for demo builds");
#endif // RSG_PC && __BANK

XPARAM(installremote);
XPARAM(cleaninstall);
XPARAM(gond);
XPARAM(netremotelogging);
XPARAM(nooutput);
PARAM(emergencystop, "Pause the game and streaming during an emergency");
extern bool g_SuspendEmergencyStop;

#define DEFAULT_DEBUG_GAP (0.007f)

// A SysTrayRFS of less than that (in MB/s) is considered bad and will make the "SY" in the debug bar go red.
const float BAD_SYSTRAYRFS_THRESHOLD = 7.0f;

//////////////////////////////////////////////////////////////////////////
u32 CEmergencyBar::ms_state = 0;
u32 CEmergencyBar::ms_timer = 0;
Color32 CEmergencyBar::ms_aBgColors[STATE_TOTAL] =
{
	Color32(0, 0, 0, 255),			//STATE_NORMAL
	Color32(200, 0, 0, 255),		//STATE_STREAMING_EMERGENCY
	Color32(255, 166, 0, 255),		//STATE_STREAMING_FRAGMENTATION_EMERGENCY
	Color32(200, 200, 0, 255),		//STATE_SCRIPT_EMERGENCY
	Color32(240, 0, 240, 255)		//STATE_SCRIPT_REQUEST_EMERGENCY
};
Color32 CEmergencyBar::ms_aTextColors[STATE_TOTAL] =
{
	Color32(255, 255, 255, 255),	//STATE_NORMAL
	Color32(255, 255, 255, 255),	//STATE_STREAMING_EMERGENCY
	Color32(255, 255, 255, 255),	//STATE_STREAMING_FRAGMENTATION_EMERGENCY
	Color32(0, 0, 0, 255),			//STATE_SCRIPT_EMERGENCY
	Color32(0, 0, 0, 255)			//STATE_SCRIPT_REQUEST_EMERGENCY
};

void CEmergencyBar::Update()
{
#if !__FINAL
	if (strStreamingEngine::GetLoader().GetMsTimeSinceLastEmergency() < 1000)
	{
		CEmergencyBar::SetState(CEmergencyBar::STATE_STREAMING_EMERGENCY);
		if(PARAM_emergencystop.Get() BANK_ONLY(&& !g_SuspendEmergencyStop))
		{
			if (!fwTimer::IsGamePaused())
			{
				fwTimer::StartUserPause(); //ensure game is paused
			}
			if (!CStreaming::IsStreamingPaused())
			{
				CStreaming::SetStreamingPaused(true);
			}
		}			
	}
	else if (strStreamingEngine::GetLoader().GetMsTimeSinceLastFragmentationEmergency() < 1000)
	{
		CEmergencyBar::SetState(CEmergencyBar::STATE_STREAMING_FRAGMENTATION_EMERGENCY);
	}
#if __SCRIPT_MEM_CALC
	else if (CTheScripts::GetScriptHandlerMgr().GetMsTimeSinceLastEmergency() < 1000)
	{
		CEmergencyBar::SetState(CEmergencyBar::STATE_SCRIPT_EMERGENCY);
	}
	else if (CTheScripts::GetScriptHandlerMgr().GetMsTimeSinceLastRequestEmergency() < 1000)
	{
		CEmergencyBar::SetState(CEmergencyBar::STATE_SCRIPT_REQUEST_EMERGENCY);
	}
#endif	//	__SCRIPT_MEM_CALC

	switch (ms_state)
	{
	case STATE_NORMAL:
		break;
	case STATE_STREAMING_EMERGENCY:
	case STATE_STREAMING_FRAGMENTATION_EMERGENCY:
	case STATE_SCRIPT_EMERGENCY:
	case STATE_SCRIPT_REQUEST_EMERGENCY:
		ms_timer++;
		if (ms_timer>50) { SetState(STATE_NORMAL); }
		break;
	default:
		break;
	}
#endif // !__FINAL
}

//////////////////////////////////////////////////////////////////////////
PARAM(debugInfoBarPosition, "[debug] Y Position at which debug info bar should render");
#if RSG_PC
static float s_fReleaseDebugTextPosition = 0.955f;  // PCs typically don't need to worry about overscan; good values for pc: 0.955 = two lines, 0.98 = one line
#else
static float s_fReleaseDebugTextPosition = 0.94f;  // fine for HD Tv's
#endif

static bool s_Enabled = !__FINAL || FINAL_DISPLAY_BAR;
static bool s_IsStreamingPriority = false;
static int s_StreamVirtual = 0;
static int s_StreamPhysical = 0;
static int s_NumStreamingRequests = 0;
#if !RSG_FINAL
	static int s_NumStreamCriticalRequests;
	static int s_NumStreamNormalRequests;
	static int s_NumStreamMaxRequests;
	static int s_NumStreamLoading;
	static int s_NumStreamMaxLoading;
	static bool s_WaitingForBuffer;
	static bool s_Decompressing;
#endif
eDEBUG_DISPLAY_STATE CDebugBar::sm_displayReleaseDebugText;
static int s_frameCount = 0;
#if __BANK
static int s_ScriptVirtual = 0;
static int s_ScriptPhysical = 0;
static int s_CurrentBlockInside = 0;
extern bool g_maponlyHogEnabled;
extern bool g_cheapmodeHogEnabled;
extern bool g_maponlyExtraHogEnabled;
extern bool g_stealExtraMemoryEnabled;
#endif // __BANK

#if __WIN32PC && __BANK
char* CDebugBar::sm_systemInfo = NULL;
bool s_bHaveSystemInfo = false;
bool s_bGettingSystemInfo = false;
#endif


void CDebugBar::Init(unsigned /*initMode*/)
{
	sm_displayReleaseDebugText = DEBUG_DISPLAY_STATE_STANDARD;  // Les wants this ON as default now
	const char *debugdisplaystate = NULL;
	if(PARAM_debugdisplaystate.Get(debugdisplaystate))
	{
		if(stricmp(debugdisplaystate, "off") == 0)
		{
			sm_displayReleaseDebugText = DEBUG_DISPLAY_STATE_OFF;
		}
		else if(stricmp(debugdisplaystate, "standard") == 0)
		{
			sm_displayReleaseDebugText = DEBUG_DISPLAY_STATE_STANDARD;
		}
		else if(stricmp(debugdisplaystate, "coords_only") == 0)
		{
			sm_displayReleaseDebugText = DEBUG_DISPLAY_STATE_COORDS_ONLY;
		}
	}

#if DEBUG_DISPLAY_BAR 
	PARAM_debugInfoBarPosition.Get(s_fReleaseDebugTextPosition);
#endif
}

void CDebugBar::Update()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD0, KEYBOARD_MODE_OVERRIDE))
	{
		s32 displayState = (s32)GetDisplayReleaseDebugText();

		if (displayState < ((s32)MAX_DEBUG_DISPLAY_STATES)-1)
		{
			displayState++;
		}
		else
		{
			displayState = 0;
		}

		SetDisplayReleaseDebugText((eDEBUG_DISPLAY_STATE)displayState);
	}

	CEmergencyBar::Update();
	CStreamingDebug::CalculateRequestedObjectSizes(s_StreamVirtual, s_StreamPhysical);

	s_StreamVirtual >>= 10;
	s_StreamPhysical >>= 10;

	s_NumStreamingRequests = strStreamingEngine::GetInfo().GetNumberRealObjectsRequested();

#if !RSG_FINAL
		pgStreamer::GetQueueStats(s_NumStreamCriticalRequests, s_NumStreamNormalRequests, s_NumStreamMaxRequests, s_WaitingForBuffer, s_Decompressing);
		s_NumStreamLoading = 0;
		s_NumStreamMaxLoading = 0;
		for(int i=0; i<strStreamingLoaderManager::DEVICE_COUNT; ++i)
		{
			int num = 0, max = 0;
			strStreamingEngine::GetLoader().GetLoader((strStreamingLoaderManager::Device)i).GetLoaderStats(num, max);
			s_NumStreamLoading += num;
			s_NumStreamMaxLoading += max;
		}
#endif

#if __BANK
	if (CBlockView::GetNumberOfBlocks())
		s_CurrentBlockInside = CBlockView::GetCurrentBlockInside();

	if (PARAM_displayDebugBarDisplayMemoryPeaks.Get())
	{
		s_ScriptVirtual = CTheScripts::GetScriptHandlerMgr().GetTotalScriptVirtualMemoryPeak();
		s_ScriptPhysical = CTheScripts::GetScriptHandlerMgr().GetTotalScriptPhysicalMemoryPeak();
	}
	else
	{
		s_ScriptVirtual = CTheScripts::GetScriptHandlerMgr().GetTotalScriptVirtualMemory();
		s_ScriptPhysical = CTheScripts::GetScriptHandlerMgr().GetTotalScriptPhysicalMemory();
	}
	

	s_ScriptVirtual >>= 10;
	s_ScriptPhysical >>= 10;
#endif // __BANK
	s_IsStreamingPriority = strStreamingEngine::GetIsLoadingPriorityObjects();
#if __BANK

#if RSG_PC && __BANK && __D3D11

#if !__TOOL
	static int lastWidth = 0, lastHeight = 0;
	int screenWidth = GRCDEVICE.GetWidth(), screenHeight = GRCDEVICE.GetHeight();

	if (lastWidth != screenWidth || lastHeight != screenHeight)
	{
		lastWidth = screenWidth;
		lastHeight = screenHeight;
		s_bHaveSystemInfo = false;
	}
#endif // !__TOOL

	if (!s_bHaveSystemInfo && !s_bGettingSystemInfo)
	{
		s_bGettingSystemInfo = true;
		struct Closure
		{
			static void ThreadEntry(void*)
			{
				char* sysInfo;
				DXDiag::DumpDisplayableInfoString(&sysInfo, false);

				Displayf("DxDiag: %s", sysInfo);


				char* settingsInfo;
				CSettingsManager::GetInstance().GraphicsConfigSource(settingsInfo);

				Displayf("%s", settingsInfo);

				size_t len = strlen(sysInfo);
				size_t settingslen = strlen(settingsInfo);

				char* combined = rage_new char[len + settingslen + 1];
				strcpy(combined, sysInfo);
				strcat(combined, settingsInfo);
				delete[] sysInfo;
				delete[] settingsInfo;
				sysInfo = combined;

				len = strlen(sysInfo);

				int tildeCount = 0;
				for (int i = 0 ; i < len; i++)
					if (sysInfo[i] == '~')
						tildeCount++;

				if (tildeCount > 0)
				{
					char* temp = rage_new char[len + 1 + tildeCount];

					char* ptr = temp;
					for (int i = 0; i < len+1; i++)
					{
						char c = sysInfo[i];
						(*ptr++) = c;
						if (c == '~')
							(*ptr++) = c;
					}

					delete[] sysInfo;
					sysInfo = temp;
				}

				CDebugBar::SetSystemInfo(sysInfo);
				s_bHaveSystemInfo = true;
				s_bGettingSystemInfo = false;
			}
		};

		if(!PARAM_demobuild.Get())
			rage::sysIpcCreateThread(&Closure::ThreadEntry, NULL, 32*1024, PRIO_SYSTEM_INFO_THREAD, "System Info Thread");
	}
#endif // __WIN32PC
#endif // __BANK
	s_frameCount = fwTimer::GetFrameCount();
}

void CDebugBar::RenderBackground(float fPosition)
{
	bool bBuildingDrawList = gDrawListMgr->IsBuildingDrawList();

	float fFpsThreshold = 20.0f;

	if (PARAM_fpsborder.Get(fFpsThreshold) || PARAM_fpsborder.Get())
	{
		float fFpsBorderSize = 1.0f - fPosition - 0.01f;
		Color32 fpsBorderColor = (fwTimer::GetSystemTimeStep() <= 1.0f / fFpsThreshold) ? Color32(0, 200, 0, 255) : Color32(200, 0, 0, 255);

		if (bBuildingDrawList)
		{
			DLC ( CDrawRectDC, (fwRect(0.0f, 1.0f, fFpsBorderSize, 0.0f), fpsBorderColor) );
			DLC ( CDrawRectDC, (fwRect(0.0f, fFpsBorderSize, 1.0f, 0.0f), fpsBorderColor) );
			DLC ( CDrawRectDC, (fwRect(1.0f - fFpsBorderSize, 1.0f, 1.0f, 0.0f), fpsBorderColor) );
		}
		else
		{
			CSprite2d::DrawRect(fwRect(0.0f, 1.0f, fFpsBorderSize, 0.0f), fpsBorderColor);
			CSprite2d::DrawRect(fwRect(0.0f, fFpsBorderSize, 1.0f, 0.0f), fpsBorderColor);
			CSprite2d::DrawRect(fwRect(1.0f - fFpsBorderSize, 1.0f, 1.0f, 0.0f), fpsBorderColor);
		}
	}

	// render quad behind:
	Color32 bgColor = PARAM_nooutput.Get() ? Color32(0, 0, 200) : CEmergencyBar::GetBgColor();
	if (bBuildingDrawList)
	{
		DLC ( CDrawRectDC, (fwRect(0.0f, 1.0f, 1.0f, fPosition), bgColor) );
	}
	else
	{
		CSprite2d::DrawRect(fwRect(0.0f, 1.0f, 1.0f, fPosition), bgColor);
	}
}

void CDebugBar::Render_DISPLAY_STATE_STANDARD()
{
	if (!camInterface::IsInitialized())
		return;

	static float fDebugStringPedMaxWidth = 0.0f;
	static float fDebugStringCamMaxWidth = 0.0f;
	static float fDebugStringFpsMaxWidth = 0.0f;

	float fNewPos = 0.0f;

	Color32& txtColor = CEmergencyBar::GetTextColor();

	CTextLayout ReleaseTextLayout;
	char debugText[1024];

	Vector2 vTextPos;
	Vector2 vTextSize = Vector2(0.10f, 0.30f);
	float fPosition = s_fReleaseDebugTextPosition;

	// shitty TVs need to have this slightly higher, so use the radar position
	if (!GRCDEVICE.GetHiDef())
	{
#if RSG_PC
		fPosition = 0.94f;	// Even low res on PC generally won't have much overscan
		vTextSize = Vector2(0.10f, 0.30f);	// and is hires enough for bigger text
#else
		fPosition = 0.917f;
		vTextSize = Vector2(0.20f, 0.20f);
#endif
	}
	vTextSize *= 0.9f;

	float x1,x2,y1,y2;
	CHudTools::GetMinSafeZone(x1,y1,x2,y2);
	fPosition = ::Max(y2, fPosition);  

#if RSG_PC	// may apply to other platforms after testing
	fPosition = ::Min(fPosition, s_fReleaseDebugTextPosition);	// keep debug bar onscreen
#endif

	RenderBackground(fPosition);

	vTextPos = Vector2(0.03f, fPosition);

	ReleaseTextLayout.SetScale(&vTextSize);
	ReleaseTextLayout.SetColor(txtColor);

	bool bRenderCutsceneInfo = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());

	if (!bRenderCutsceneInfo)
	{
		// player pos:
		Vector3 vPlayerPos = CPlayerInfo::ms_cachedMainPlayerPos;

		formatf(debugText, "P:%0.1f,%0.1f,%0.1f", vPlayerPos.x, vPlayerPos.y, vPlayerPos.z, NELEM(debugText));

		fNewPos = 0.03f + ReleaseTextLayout.GetStringWidthOnScreen(debugText, true);
	} 
	else
	{
		formatf(debugText, " ", NELEM(debugText)); 

		fNewPos = 0.03f;
	}

	// reset the adjustment if the gap is now too large
	if (fNewPos > fDebugStringPedMaxWidth + DEFAULT_DEBUG_GAP || fNewPos < fDebugStringPedMaxWidth - DEFAULT_DEBUG_GAP)
		fDebugStringPedMaxWidth = 0.0f;

	fDebugStringPedMaxWidth = ::Max(fDebugStringPedMaxWidth, fNewPos);

	ReleaseTextLayout.Render(&vTextPos, debugText);

	// vers number, block info, clock:
#if !__FINAL
	char streamingMem[16]={0};
#if ENABLE_DEBUG_HEAP
	if(g_sysExtraStreamingMemory>0)
	{
		formatf(streamingMem,"XM:%u ",g_sysExtraStreamingMemory>>20);
	}
#endif
#if __BANK
	camDebugDirector* debugDirector = camInterface::GetDebugDirectorSafe();
	bool bRenderAdditionalCameraInfo = false;
	bool bRenderMissionInfo = false;
	if (debugDirector && !NetworkInterface::IsGameInProgress())
	{
		bRenderAdditionalCameraInfo = debugDirector->IsFreeCamActive();
		bRenderMissionInfo = (CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsOnARandomEvent() );

		if(bRenderCutsceneInfo || bRenderAdditionalCameraInfo)
			formatf(debugText, "v%s %s%02d:%02d%s",CDebug::GetFullVersionNumber(), streamingMem, CClock::GetHour(),CClock::GetMinute(),PARAM_disableTodDebugModeDisplay.Get() ? "" : CClock::GetDisplayCode(), NELEM(debugText));
		else if(bRenderMissionInfo)
			formatf(debugText, "v%s %s(%d %s) %02d:%02d%s",CDebug::GetFullVersionNumber(), streamingMem, s_CurrentBlockInside, CBlockView::GetBlockName(s_CurrentBlockInside), CClock::GetHour(),CClock::GetMinute(),PARAM_disableTodDebugModeDisplay.Get() ? "" : CClock::GetDisplayCode(), NELEM(debugText));
		else
			formatf(debugText, "v%s %s(%d %s %s) %02d:%02d%s",CDebug::GetFullVersionNumber(), streamingMem, s_CurrentBlockInside, CBlockView::GetBlockName(s_CurrentBlockInside), CBlockView::GetBlockUser(s_CurrentBlockInside), CClock::GetHour(),CClock::GetMinute(),PARAM_disableTodDebugModeDisplay.Get() ? "" : CClock::GetDisplayCode(), NELEM(debugText));
	}
	else
		formatf(debugText, "v%s %s",CDebug::GetFullVersionNumber(), streamingMem, NELEM(debugText));
#else // __BANK
	debugText[0] = 0;

	if (!NetworkInterface::IsGameInProgress())
		formatf(debugText, "v%s %s%02d:%02d%s",CDebug::GetFullVersionNumber(), streamingMem, CClock::GetHour(),CClock::GetMinute(),PARAM_disableTodDebugModeDisplay.Get() ? "" : CClock::GetDisplayCode(), NELEM(debugText));

#endif // __BANK

	char weatherString[64];

	formatf(weatherString, " %s", g_weather.GetTypeName(g_weather.GetPrevTypeIndex()), NELEM(weatherString));
	weatherString[7] = '\0';  // limit name to 6 chars
	safecat(debugText, weatherString, NELEM(debugText));

	formatf(weatherString, ">%s", g_weather.GetTypeName(g_weather.GetNextTypeIndex()), NELEM(weatherString));
	weatherString[8] = '\0';  // limit name to 6 chars
	safecat(debugText, weatherString, NELEM(debugText));

	formatf(weatherString, "(%0.2f%s)", g_weather.GetInterp(), PARAM_disableTodDebugModeDisplay.Get() ? "" : g_weather.GetDisplayCode(), NELEM(weatherString));
	safecat(debugText, weatherString, NELEM(debugText));

	// Bandwidth
	char bandwidthToChar='L';
	unsigned bandwidth = CNetwork::GetBandwidthManager().GetTargetUpstreamBandwidth();
	if(bandwidth>512)
		bandwidthToChar='M';
	if(bandwidth>768)
		bandwidthToChar='H';
	char bandwidthLimit[5];
	formatf(bandwidthLimit, " B:%c", bandwidthToChar, NELEM(bandwidthLimit));
	safecat(debugText, bandwidthLimit, NELEM(debugText));
#endif // !__FINAL

	// bandwidth and vehicles
	if (NetworkInterface::IsGameInProgress())
	{
		s32 fDesiredNumAmbientVehicles = CVehiclePopulation::GetDesiredNumberAmbientVehicles_Cached();
		s32	TotalAmbientCarsOnMap      = CVehiclePopulation::ms_numRandomCars;
		//s32 TotalNumOfVehicles         = CNetworkObjectPopulationMgr::GetTotalNumVehiclesSafe();
		//s32 BandwidthTargetNumVehicles = CNetworkObjectPopulationMgr::GetBandwidthTargetNumVehicles();

		s32 TargetNumLocalVehicles        = CNetworkObjectPopulationMgr::GetLocalTargetNumVehicles();
		s32 TotalNumLocalVehicles         = CNetworkObjectPopulationMgr::GetNumCachedLocalVehicles();

		s32 pedMemoryUse     = CPopulationStreaming::GetCachedTotalMemUsedByPeds()     >> 20;
		s32 vehicleMemoryUse = CPopulationStreaming::GetCachedTotalMemUsedByVehicles() >> 20;
		s32 pedBudget        = gPopStreaming.GetCurrentMemForPeds()                    >> 20;
		s32 vehicleBudget    = gPopStreaming.GetCurrentMemForVehicles()                >> 20;

		char colorPedBudget     = pedMemoryUse>=pedBudget? 'r' : 'g';
		char colorVehicleBudget = vehicleMemoryUse>=vehicleBudget? 'r' : 'g';

		char colorVehicle   = TotalAmbientCarsOnMap>=fDesiredNumAmbientVehicles ? 'r' : 'g';
		//char colorBandwidth = BandwidthTargetNumVehicles <= 20 ? 'r' : 's';
		char colorVehicle2  = TotalNumLocalVehicles>=TargetNumLocalVehicles ? 'r' : 'g';

		float fObjects = ((float)NetworkInterface::GetObjectManager().GetTooManyObjectsCountdown()) / 1000.0f;

		char reassignmentInProgress = CNetwork::GetObjectManager().GetReassignMgr().IsReassignmentInProgress_Cached() ? 'R' : ' ';

		char text[512];
		//formatf(text, " (~%c~%d/%d~s~ %d - ~%c~%d~s~) %02d:%02d", colorVehicle, TotalAmbientCarsOnMap, fDesiredNumAmbientVehicles, TotalNumOfVehicles, colorBandwidth, BandwidthTargetNumVehicles, CClock::GetHour(),CClock::GetMinute(), NELEM(debugText));
#if __BANK || __FINAL
		formatf(text,    "%c p~%c~%d/%d~s~v~%c~%d/%d~s~ (~%c~%d/%d~s~ ~%c~%d/%d~s~ %.2f) %02d:%02d",
#else
		formatf(text, "v%s %c p~%c~%d/%d~s~v~%c~%d/%d~s~ (~%c~%d/%d~s~ ~%c~%d/%d~s~ %.2f) %02d:%02d", CDebug::GetFullVersionNumber(),
#endif
			reassignmentInProgress,
			colorPedBudget, pedMemoryUse, pedBudget,
			colorVehicleBudget, vehicleMemoryUse, vehicleBudget,
			colorVehicle, TotalAmbientCarsOnMap, fDesiredNumAmbientVehicles, 
			colorVehicle2, TotalNumLocalVehicles, TargetNumLocalVehicles, fObjects, CClock::GetHour(),CClock::GetMinute(), NELEM(debugText));
		safecat(debugText, text, NELEM(debugText));
	}

#if !__FINAL
#if __BANK
	if (g_stealExtraMemoryEnabled)
	{
		safecat(debugText, " ME", NELEM(debugText));
	}
	else if (g_cheapmodeHogEnabled)
	{
		safecat(debugText, " M2", NELEM(debugText));
	}
	else if (g_maponlyExtraHogEnabled)
	{
		safecat(debugText, " M3", NELEM(debugText));
	}
	else if (g_maponlyHogEnabled)
	{
		safecat(debugText, " M1", NELEM(debugText));
	}
	else
#endif // __BANK
	{
		if (fiDeviceInstaller::GetIsBootedFromHdd())
		{
			if (PARAM_cleaninstall.Get())
				safecat(debugText, " HC", NELEM(debugText));
			else
				safecat(debugText, " HD", NELEM(debugText));
		}
		else if (PARAM_cleaninstall.Get())
		{
			safecat(debugText, " IC", NELEM(debugText));
		}
		else if (sysBootManager::IsBootedFromDisc())
		{
			safecat(debugText, " DI", NELEM(debugText));
		}
		else if (sysBootManager::IsPackagedBuild())
		{
			safecat(debugText, " PK", NELEM(debugText));
		}
		else
		{
			float throughPut = static_cast<const fiDeviceRemote &>(fiDeviceRemote::GetInstance()).GetAverageThroughput();

			// If we have shitty SysTrayRFS throughput, make it red.
			if (throughPut < BAD_SYSTRAYRFS_THRESHOLD * 1024.0f * 1024.0f)
			{
				safecat(debugText, "~r~", NELEM(debugText));
			}

			safecat(debugText, " SY~s~", NELEM(debugText));

			// We could display the actual throughput here, but let's not deface the debug bar unless we have to.
			// It's useful to uncomment the code below if the throughput is persistently shit to get an idea of what it really is.
/*			char timer[128];
			formatf(timer, " %.1f MB/s", throughPut);
			safecat(debugText, timer, NELEM(debugText));*/
		}
	}
	PS3_ONLY(safecat(debugText, ".", NELEM(debugText));)
#endif // !__FINAL


		if (!CGameLogic::IsGameStateInPlay())
		{
			safecat(debugText, " -NO PLAY-", NELEM(debugText));  // to aid debugging bugs like 472934
		}

		if(EXTRACONTENT.GetContentByDevice("dlcHighResPack:/"))
		{
			safecat(debugText, " ** HI-RES **", NELEM(debugText));
		}

		const float fIDPos = RSG_PC ? 0.5 : 0.0f;
		ReleaseTextLayout.SetWrap(Vector2(fIDPos, 0.96f));
		ReleaseTextLayout.SetOrientation(FONT_RIGHT);
		const float fNextLine = RSG_PC ? 0.02f : 0.0f;
		vTextPos = Vector2(fIDPos, fPosition + fNextLine);

		ReleaseTextLayout.Render(&vTextPos, debugText);

		// camera coords:
		Vector3 vCamPos = camInterface::GetPos();
#if __BANK
		Vector3 vCamDir = Vector3(0,0,0);
		
		char cutSceneName[70] = {0};
		if (bRenderCutsceneInfo)
		{
			const CutSceneManager::DebugRenderState& cutsceneState = CutSceneManager::GetDebugRenderState();
			bRenderMissionInfo = false;  // no need for mission info here - fix for 740523

			float fTime = fwTimer::IsGamePaused() ? cutsceneState.cutSceneCurrentTime : cutsceneState.cutScenePreviousTime; 
			
			u32 CurrentFrame = u32(floorf(fTime*CUTSCENE_FPS)); 

			u32 currentTime = (u32)(fTime * 1000.0f); 

			currentTime = currentTime / 1000; 

			u32 CurentMinutes = currentTime / 60; 
			u32 CurrentSeconds = currentTime % 60; 
			float fCurrent30Frame = fmodf( fTime, 60.0f );
			fCurrent30Frame -= (float)CurrentSeconds;
			u32 Current30Frame = int(fCurrent30Frame * 30.0f);

			u32 TotalTime = (u32)(cutsceneState.totalSeconds*1000.0f); 
			TotalTime = TotalTime / 1000; 

			u32 TotalMinutes = TotalTime / 60; 
			u32 TotalSeconds = TotalTime % 60; 
			u32 TotalFrames = (u32)(cutsceneState.totalSeconds * 30.0f); 
			float fTotal30Frame = cutsceneState.totalSeconds * 30.0f;
			fTotal30Frame -= (float)TotalFrames;
			u32 Total30Frame = int(fTotal30Frame * 30.0f);

			if(cutsceneState.currentConcatSectionIdx >= 0)
			{	
				formatf(cutSceneName, 
					"%s (%s:%d/%d) %02d:%02d:%02d/%02d:%02d:%02d (%d/%d) %.1f/%.1f", 
					cutsceneState.cutsceneName, cutsceneState.concatDataSceneName, cutsceneState.currentConcatSectionIdx + 1, cutsceneState.concatSectionCount, 
					CurentMinutes, 
					CurrentSeconds,
					Current30Frame,
					TotalMinutes, 
					TotalSeconds,
					Total30Frame,
					CurrentFrame,
					TotalFrames, cutsceneState.cutscenePlayTime, cutsceneState.cutsceneDuration, NELEM(cutSceneName));

			}
			else
			{
				formatf(cutSceneName, "%s %02d:%02d:%02d/%02d:%02d:%02d (%d/%d) %.1f/%.1f", 
					cutsceneState.cutsceneName, 
					CurentMinutes, 
					CurrentSeconds,
					Current30Frame,
					TotalMinutes, 
					TotalSeconds,
					Total30Frame,
					CurrentFrame,
					TotalFrames, cutsceneState.cutscenePlayTime, cutsceneState.cutsceneDuration, NELEM(cutSceneName));
			}
		}
#endif // __BANK

#if __BANK
		if(bRenderAdditionalCameraInfo && debugDirector)
		{
			const camFrame& freeCamFrame = debugDirector->GetFreeCamFrame();

			freeCamFrame.GetWorldMatrix().ToEulersXYZ(vCamDir);
			vCamDir *= RtoD; //Convert as level guys work in Degrees, not Radians.

			float fFOV = freeCamFrame.GetFov();

			formatf(debugText, "C:%0.1f,%0.1f,%0.1f  Dir:%0.1f,%0.1f,%0.1f  Fov:%0.1f",vCamPos.x, vCamPos.y, vCamPos.z, vCamDir.x, vCamDir.y, vCamDir.z, fFOV, NELEM(debugText));
#if __D3D11 || RSG_ORBIS
			//add milliseconds onto PC version
			char msText[64];
			formatf( msText, " (%3.1fms)", fwTimer::GetSystemTimeStep() * 1000.0f , NELEM(msText));
			safecat(debugText, msText, NELEM(debugText) );
#endif // __D3D11 || RSG_ORBIS__D3D11 || RSG_ORBIS		
			if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
			{
				char cutsceneText[256];
				formatf(cutsceneText,"  %s", cutSceneName, NELEM(cutsceneText));
				safecat(debugText, cutsceneText, NELEM(debugText));
			}
		}
		else
#endif // __BANK
		{
			if (CPauseMenu::IsActive())
			{
				formatf(debugText, "MC:%0.2f,%0.2f", CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y, NELEM(debugText));
			}
			else
			{
				formatf(debugText, "C:%0.1f,%0.1f,%0.1f",vCamPos.x, vCamPos.y, vCamPos.z, NELEM(debugText));
#if __BANK
				if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
				{
					char cutsceneText[256];
					formatf(cutsceneText,"  %s", cutSceneName, NELEM(cutsceneText));
					safecat(debugText, cutsceneText, NELEM(debugText));
				}
#endif // __BANK
			}
		}

		ReleaseTextLayout.SetWrap(Vector2(0.0f, 1.0f));
		ReleaseTextLayout.SetOrientation(FONT_LEFT);

		fNewPos = DEFAULT_DEBUG_GAP + fDebugStringPedMaxWidth+ReleaseTextLayout.GetStringWidthOnScreen(debugText, true);

		// reset the adjustment if the gap is now too large
		if (fNewPos > fDebugStringCamMaxWidth + DEFAULT_DEBUG_GAP || fNewPos < fDebugStringCamMaxWidth - DEFAULT_DEBUG_GAP)
			fDebugStringCamMaxWidth = 0.0f;

		fDebugStringCamMaxWidth = Max(fDebugStringCamMaxWidth, fNewPos);
		vTextPos = Vector2(fDebugStringPedMaxWidth, fPosition);
		ReleaseTextLayout.Render(&vTextPos, debugText);

		//==================================================================================================================================
		// FPS at fixed position otherwise its hard to read
		static float fFramePercentage = 0.01f;
		static float fAverageFps = 30.0f;
		if (PARAM_smoothframerate.Get())
		{	
			fAverageFps = (fAverageFps * (1.0f - fFramePercentage)) + ((1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep())) * fFramePercentage);
		}
		else
		{
			fAverageFps = (1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep()));
		}
		formatf(debugText, "Fps:%3.1f", fAverageFps, NELEM(debugText));

#if __D3D11 || RSG_ORBIS
		//add milliseconds onto PC version
		char msText[64];
		formatf( msText, " (%3.1fms)", fwTimer::GetSystemTimeStep() * 1000.0f, NELEM(msText));
		safecat(debugText, msText, NELEM(debugText));
#endif	// __D3D11 || RSG_ORBIS

		ReleaseTextLayout.SetWrap(Vector2(0.0f, 1.0f));
		ReleaseTextLayout.SetOrientation(FONT_LEFT);

		fNewPos = DEFAULT_DEBUG_GAP + fDebugStringCamMaxWidth+ReleaseTextLayout.GetStringWidthOnScreen(debugText, true);

		// reset the adjustment if the gap is now too large
		if (fNewPos > fDebugStringFpsMaxWidth + DEFAULT_DEBUG_GAP || fNewPos < fDebugStringFpsMaxWidth - DEFAULT_DEBUG_GAP)
			fDebugStringFpsMaxWidth = 0.0f;

		fDebugStringFpsMaxWidth = Max(fDebugStringFpsMaxWidth, fNewPos);
		vTextPos = Vector2(fDebugStringCamMaxWidth, fPosition);
		ReleaseTextLayout.Render(&vTextPos, debugText);



		char cTempString[256];

#if !__FINAL
		const char *srlIndicator = gStreamingRequestList.IsActive() ? "|" : " ";

#if __BANK
		if(PARAM_displayDebugBarMemoryInKB.Get())
			formatf(cTempString, "Mem:(M%05dk,V%05dk)%sScr:(V%06dk,M%06dk)",s_StreamPhysical, s_StreamVirtual, srlIndicator, s_ScriptVirtual, s_ScriptPhysical, NELEM(cTempString));
		else
			formatf(cTempString, "Mem:(M%02dM,V%02dM)%sScr:(V%02dM,M%02dM)",s_StreamPhysical/1024, s_StreamVirtual/1024, srlIndicator, s_ScriptVirtual/1024, s_ScriptPhysical/1024, NELEM(cTempString));
#else // __BANK
		if(PARAM_displayDebugBarMemoryInKB.Get())
			formatf(cTempString, "Mem:(M%dk,V%dk)%s",s_StreamPhysical, s_StreamVirtual, srlIndicator);
		else
			formatf(cTempString, "Mem:(M%02dM,V%02dM)%s",s_StreamPhysical/1024, s_StreamVirtual/1024, srlIndicator);
#endif // __BANK

		safecpy (debugText, cTempString, NELEM(debugText));
#endif
		char streamState[4];
		if(s_IsStreamingPriority)
			safecpy(streamState, "p");
		else
			safecpy(streamState, " ");

		#if RSG_FINAL
			formatf(cTempString, " Req:%d %s", s_NumStreamingRequests, streamState, NELEM(cTempString));
		#else
			if( s_Decompressing )
			{
				safecat(streamState, "d", NELEM(streamState));
			}
			else
			{
				safecat(streamState, " ", NELEM(streamState));
			}
			if( s_WaitingForBuffer )
			{
				safecat(streamState, "w", NELEM(streamState));
			}
			else
			{
				safecat(streamState, " ", NELEM(streamState));
			}

			formatf(cTempString, " Req:%d %s L:%02d, C:%02d, N:%02d", s_NumStreamingRequests, streamState, s_NumStreamLoading, s_NumStreamCriticalRequests, s_NumStreamNormalRequests, NELEM(cTempString));
		#endif

		safecat (debugText, cTempString, NELEM(debugText));

#if RSG_PC && __BANK
		if (PARAM_displayVidMem.Get())
		{
			formatf(cTempString, "Vid:%dM/%dM", (s32)(rage::grcResourceCache::GetInstance().GetTotalUsedMemory(rage::MT_Video) / (1024UL * 1024UL)), (s32)(rage::grcResourceCache::GetInstance().GetTotalMemory(rage::MT_Video)  / (1024UL * 1024UL)));
			safecat(debugText, cTempString, NELEM(debugText));
		}
#endif
		// #if !__FINAL
		// 		if(CStreaming::IsStreamingPaused())
		// 			safecat(debugText, " Streaming Paused  ", NELEM(debugText));
		// #endif

#if !__FINAL		
		if(PARAM_displaykeyboardmode.Get() && !NetworkInterface::IsGameInProgress())
		{
			char keyboardString[10];
			switch (CControlMgr::GetKeyboard().GetKeyboardMode())
			{
			case KEYBOARD_MODE_DEBUG:
				formatf(keyboardString, " KeyB:D");
				break;
			case KEYBOARD_MODE_GAME:
				formatf(keyboardString, " KeyB:G");
				break;
			case KEYBOARD_MODE_MARKETING:
				formatf(keyboardString, " KeyB:M");
				break;
#if GTA_REPLAY
			case KEYBOARD_MODE_REPLAY:
				formatf(keyboardString, " KeyB:R");
				break;
#endif
			default:
				break;
			}
			safecat (debugText, keyboardString, NELEM(debugText));
		}
#endif

        if(!PARAM_nodisplayframecount.Get())
		{
			formatf(cTempString, " F:%07d", s_frameCount, NELEM(cTempString));
			safecat (debugText, cTempString, NELEM(debugText));
		}

#if RSG_PC
		if (!PARAM_nodisplayconvergence.Get() && GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled())
		{
			formatf(cTempString, " Convergence: %3.3f", GRCDEVICE.GetConvergenceDistance(), NELEM(cTempString));
			safecat (debugText, cTempString, NELEM(debugText));
		}
#endif

#if __BANK
		// display the scripted name:
		if (bRenderMissionInfo)
		{
			const char* missionTag = CDebug::GetCurrentMissionName();
			if (missionTag[0] != '\0')
			{
				formatf(cTempString, "  %s", missionTag);
				safecat (debugText, cTempString);
			}
		}
#endif // __BANK

		ReleaseTextLayout.SetWrap(Vector2(0.0f, 1.0f));
		ReleaseTextLayout.SetOrientation(FONT_LEFT);

		vTextPos = Vector2(fDebugStringFpsMaxWidth, fPosition);
		ReleaseTextLayout.Render(&vTextPos, debugText);


#if __WIN32PC && __BANK
		if (s_bHaveSystemInfo && CDebugBar::GetSystemInfo() != NULL)
		{
			ReleaseTextLayout.SetWrap(Vector2(0.0f, 1.0f));
			ReleaseTextLayout.SetOrientation(FONT_RIGHT);

			ReleaseTextLayout.SetDropShadow(true);

			vTextPos = Vector2(1.0f, 0.0f);
			ReleaseTextLayout.Render(&vTextPos, CDebugBar::GetSystemInfo());

			ReleaseTextLayout.SetDropShadow(false);
		}
#endif //  __WIN32PC && __BANK

		CText::Flush();
}

void CDebugBar::Render_DEBUG_DISPLAY_STATE_COORDS_ONLY()
{
	CTextLayout ReleaseTextLayout;

	Color32& boxColor = CEmergencyBar::GetBgColor();

	ReleaseTextLayout.SetScale(Vector2(0.84f, 0.25f)*0.9f);
	ReleaseTextLayout.SetDropShadow(true);
	ReleaseTextLayout.SetOrientation(FONT_RIGHT);
	ReleaseTextLayout.SetWrap(Vector2(-1.0f, 0.05f));

	if (boxColor != Color32(0, 0, 0, 255))
	{
		Color32& txtColor = CEmergencyBar::GetTextColor();

		ReleaseTextLayout.SetBackground(true);
		ReleaseTextLayout.SetBackgroundColor(boxColor);
		ReleaseTextLayout.SetColor(txtColor);
		ReleaseTextLayout.SetDropShadow(false);
	}

	char debugText[1024];

	// player pos:
	Vector3 vPlayerPos = CPlayerInfo::ms_cachedMainPlayerPos;
	formatf(debugText, "%0.1f~n~%0.1f~n~%0.1f", vPlayerPos.x, vPlayerPos.y, vPlayerPos.z, NELEM(debugText));
	ReleaseTextLayout.Render(Vector2(0.0f, 0.74f),debugText);

	// fps & weather:

	char weatherdebugText[1024];
	weatherdebugText[0] = '\0';
#if !__FINAL
	char weatherString[64];


	formatf(weatherString, " %s", g_weather.GetTypeName(g_weather.GetPrevTypeIndex()), NELEM(weatherString));
	weatherString[7] = '\0';  // limit name to 6 chars
	safecat(weatherdebugText, weatherString, NELEM(weatherdebugText));

	formatf(weatherString, "~n~%s", g_weather.GetTypeName(g_weather.GetNextTypeIndex()), NELEM(weatherString));
	weatherString[10] = '\0';  // limit name to 6 chars
	safecat(weatherdebugText, weatherString, NELEM(weatherdebugText));

	// 		sprintf(weatherString, "(%0.2f)", g_weather.GetInterp());
	// 		strcat(weatherdebugText, weatherString);
#endif // !__FINAL

	float fFps = (1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep()));
	formatf(debugText, "%0.1f~n~%02d:%02d~n~%s", fFps, CClock::GetHour(),CClock::GetMinute(), weatherdebugText, NELEM(debugText));
	ReleaseTextLayout.Render(Vector2(0.0f, 0.88f), debugText);

	CText::Flush();
}

void CDebugBar::Render()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __BANK
	if (SceneGeometryCapture::IsCapturingPanorama())
		return;
	if (LightProbe::IsCapturingPanorama())
		return;
#endif
	if (CLoadingScreens::AreActive())  // do not attempt to display debug bar during loadingscreens
		return;

	if (s_Enabled)
	{
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);

		if (GetDisplayReleaseDebugText() == DEBUG_DISPLAY_STATE_STANDARD )
		{
			Render_DISPLAY_STATE_STANDARD();
		}
		else if (GetDisplayReleaseDebugText() == DEBUG_DISPLAY_STATE_COORDS_ONLY)
		{
			Render_DEBUG_DISPLAY_STATE_COORDS_ONLY();
		}

	}
}

#endif // FINAL_DISPLAY_BAR
