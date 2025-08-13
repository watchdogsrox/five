//
// name:		system.cpp
// description:	Class grouping all the system level classes
//
#include "system.h"

#if __WIN32PC
#include "system/xtl.h"
#include "grcore/config.h"
#include <windows.h>
#if __D3D
	#include "grcore/config.h"
	#include "system/param.h"
#include "system/d3d9.h"
#endif
#include "grcore/texturepc.h"
#endif

#include "file/tcpip.h"
#include "grcore/texturedefault.h"
#include "grcore/fastquad.h"

#if __PPU
#include <sys/timer.h>
#include "rline/rlnp.h"
namespace rage 
{ 
	extern rlNp g_rlNp;
	extern bool g_IsExiting; 
}
#endif 

// Rage headers
#include "audioengine/engine.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "diag/output.h"
#include "file/asset.h"
#include "file/remote.h"
#include "fwdrawlist/drawlist.h"
#include "fwrenderer/instancing.h"
#include "fwrenderer/renderthread.h"
#include "grcore/device.h"
#include "grcore/font.h"
#include "grcore/gfxcontext.h"
#include "grcore/gputrace_gnm.h"
#include "grcore/texture.h"
#include "grcore/viewport.h"
#include "grmodel/model.h"
#include "grmodel/setup.h"
#include "grmodel/shader.h"
#include "move/move.h"
#include "network/Live/LiveManager.h"
#include "network/Live/NetworkTelemetry.h"
#include "paging/dictionary.h"
#include "paging/streamer.h"
#include "parser/manager.h"
#include "phbound/bound.h"
#include "grprofile/drawmanager.h"
#include "profile/profiler.h"
#include "grprofile/stats.h"
#include "profile/timebars.h"
#if RSG_DURANGO
#include "rline/durango/rlxbltitleid_interface.h"
#endif
#include "system/appcontent.h"
#include "system/bootmgr.h"
#include "system/memory.h"
#include "system/param.h"
#include "system/systeminfo.h"
#include "system/task.h"
#include "system/threadregistry.h"
#include "system/timemgr.h"
#include "system/timer.h"
#include "system/userlist.h"
#include "system/xtl.h"

// Framework headers
#include "fwsys/timer.h"
#include "fwdebug/picker.h"
#include "video/media_transcoding_allocator.h"
#include "video/VideoPlayback.h"
#include "video/VideoPlaybackThumbnailManager.h"

// Game headers
#include "camera/viewports/ViewportManager.h"
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "control/videorecording/videorecording.h"
#include "core/game.h"
#include "debug/AutomatedTest.h"
#include "debug/BudgetDisplay.h"
#include "debug/GtaPicker.h"
#include "frontend/HudTools.h"
#include "frontend/ProfileSettings.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "game/config.h"
#include "Peds/pedDefines.h"				// For setting MAXNOOFPEDS from configuration file.
#include "renderer/DrawLists/drawList.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/renderThread.h"
#include "renderer/lights/lights.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "shaders/shaderlib.h"
#include "streaming/streaming.h" 
#include "streaming/streamingvisualize.h" 
#include "system/controlmgr.h"
#include "system/filemgr.h"
#include "system/SettingsManager.h"
#include "system/threadpriorities.h"
#include "streaming/streamingdefs.h"
#include "timecycle/TimeCycle.h"
#include "tools/SmokeTest.h"
#include "Renderer/PostProcessFX.h"
#include "vfx/misc/MovieManager.h"
#include "renderer/Util/RenderDocManager.h"

#if __PS3
#include <sys/dbg.h>
extern "C" int snIsDebuggerRunning (void);
#endif //__PS3

#if __D3D11
#include "grcore/texture_d3d11.h"
#endif //__D3D11
#include "Vehicles/vehiclepopulation.h"		// For setting CVehiclePopulation::ms_maxNumberOfCarsInUse from configuration file.
#include "vfx/misc/LODLightManager.h"
#include "vfx/misc/Coronas.h"
#include "vfx/misc/FogVolume.h"
#include "vfx/particles/PtFxManager.h"

#if !__NO_OUTPUT
#include "Debug/Debug.h"
#endif	//!__NO_OUTPUT

#define FORCE_FRAME_LIMIT (RSG_PC||RSG_DURANGO) // Please do not set this based on !RSG_FINAL -- that just leads to trouble

#define FRAME_LIMIT_BEFORE_PRESENT (FORCE_FRAME_LIMIT && IS_CONSOLE)	 // Console traditionally runs the frame limiter before Present (and I don't want to change that just now)
#define FRAME_LIMIT_AFTER_PRESENT (FORCE_FRAME_LIMIT && RSG_PC && 1)	 // PC seems to get better results from limiting after Present (I suspect this is because the sleep sometimes over shoots just a bit and we miss v-sync)

#if (__WIN32PC)
#define DEFAULT_FRAME_LIMIT 0
#define DEFAULT_FRAME_LOCK 1
#else
#define DEFAULT_FRAME_LIMIT 2  // Limit frame rate using busy-wait loop below (only if DEFAULT_FRAME_LIMIT is enabled)
#define DEFAULT_FRAME_LOCK 2   // Lock the back buffer flips 
#endif

NOSTRIP_XPARAM(fullscreen);
NOSTRIP_XPARAM(windowed);
NOSTRIP_XPARAM(ignoreprofile);
PARAM(altGameConfig, "Use alternative gameconfig xml");
NOSTRIP_PC_PARAM(numTaskSchedulers, "Set the number of task scheduler threads");

#define GAMECFG_FILENAME	"common:/data/gameconfig.xml"

#if RSG_ORBIS && !__FINAL
PARAM(enableGpuTraceCapture,"Enable Razor GPU Trace Capture");
static bool g_requestedTraceCapture = false;
static int g_traceCaptureState = -1;
#endif // RSG_ORBIS

namespace rage {
	NOSTRIP_XPARAM(hdr);
	NOSTRIP_XPARAM(width);
	NOSTRIP_XPARAM(height);
#if RSG_ORBIS || RSG_DURANGO
	NOSTRIP_XPARAM(multiSample);
	NOSTRIP_XPARAM(multiFragment);
#endif
	NOSTRIP_PC_XPARAM(frameLimit);
	NOSTRIP_XPARAM(clearhddcache);
#if __PPU
	NOSTRIP_XPARAM(nodepth);
	NOSTRIP_XPARAM(force720);
	NOSTRIP_XPARAM(MSAA);
#endif // __PPU	

#if !__FINAL && __XENON
	extern size_t g_sysExtraMemory;
#endif // !__FINAL && __XENON
}

bool CSystem::ms_bDebuggerDetected = false;

// --- Structure/Class Definitions ----------------------------------------------
// Custom draw list commands
class dlCmdBeginRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_BeginRender,
	};
#if RSG_ORBIS && !__FINAL
	dlCmdBeginRender() 
	{
		m_traceCapture = g_requestedTraceCapture;
		g_requestedTraceCapture = false;
	}
#endif // RSG_ORBIS && !__FINAL

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & cmd)	{ ((dlCmdBeginRender &) cmd).Execute(); }
	void Execute();

#if RSG_ORBIS && !__FINAL
private:
	bool m_traceCapture;
#endif // RSG_ORBIS && !__FINAL
};

void dlCmdBeginRender::Execute()
{
#if RSG_ORBIS && !__FINAL
	if( m_traceCapture )
	{
		g_traceCaptureState = 0;
	}
#endif // RSG_ORBIS && !__FINAL

	CSystem::BeginRender();
}


class dlCmdEndRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_EndRender,
	};

	s32 GetCommandSize()							{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/);
};

void dlCmdEndRender::ExecuteStatic(dlCmdBase &)
{
#if GRCCONTEXT_ALLOC_SCOPES_SUPPORTED
	// When executing a drawlist, the base allocation scope is locked.
	// Temporarily unlock to prevent asserts when submitting the context.
	Assert(grcGfxContext::isLockedAllocScope());
	grcGfxContext::unlockAllocScope();
#endif

	CSystem::EndRender();

#if GRCCONTEXT_ALLOC_SCOPES_SUPPORTED
	grcGfxContext::lockAllocScope();
#endif
}


// --- Static Globals -----------------------------------------------------------
#if FORCE_FRAME_LIMIT
	s32 gFrameLimit = DEFAULT_FRAME_LIMIT;
#endif // FORCE_FRAME_LIMIT
static sysTimer gFrameTimer;

#if __D3D && __WIN32PC
static D3DDEVINFO_RESOURCEMANAGER  gD3dResourceData;
#endif

#if __BANK && __XENON
static void EndRingBufferMemUsage();
static void StartRingBufferMemUsage();
#endif


// ---- thread local ------
__THREAD bool gb_mainThread = false;

// --- Code ---------------------------------------------------------------------


void SetWindowsSystemParameters()
{
#if __WIN32PC
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);
#endif
}

void InitMemoryBuckets()
{
	grcTexture::sm_MemoryBucket = MEMBUCKET_RENDER;
	grmModel::sm_MemoryBucket = MEMBUCKET_RENDER;
	phBound::sm_MemoryBucket = MEMBUCKET_PHYSICS;
	sfScaleformManager::sm_MemoryBucket = MEMBUCKET_SCALEFORM;
	mvMove::sm_MemoryBucket = MEMBUCKET_ANIMATION;
}

#if __XENON && !__OPTIMIZED
 extern "C" BOOL D3D__DisableConstantOverwriteCheck;
#endif

#if BACKTRACE_ENABLED

// Adapted from RDR2's CDebug::GetBuildConfigName()
const char* GetBuildConfigName()
{
#if __EDITOR == 1 || __TOOL == 1 || __EXPORTER == 1
	return "Custom";
#elif __DEV	== 1 && __OPTIMIZED	== 0 && __BANK == 1 && __ASSERT	== 1 && __FINAL == 0 && __MASTER == 0 && __PROFILE == 0
	return "Debug";
#elif __DEV == 1 && __OPTIMIZED == 1 && __BANK == 1 && __ASSERT == 1 && __FINAL == 0 && __MASTER == 0 && __PROFILE == 0
	return "Beta";
#elif __DEV == 0 && __OPTIMIZED == 1 && __BANK == 1 && __ASSERT == 0 && __FINAL == 0 && __MASTER == 0 && __PROFILE == 0
	return "BankRelease";
#elif __DEV == 0 && __OPTIMIZED	== 1 && __BANK == 0 && __ASSERT	== 0 && __FINAL == 0 && __MASTER == 0 && __PROFILE == 1
	return "Profile";
#elif __DEV	== 0 && __OPTIMIZED	== 1 && __BANK == 0 && __ASSERT	== 0 && __FINAL == 0 && __MASTER == 0 && __PROFILE == 0
	return "Release";
#elif __DEV == 0 && __OPTIMIZED	== 1 && __BANK == 0 && __ASSERT	== 0 && __FINAL == 1 && __MASTER == 0 && __PROFILE == 0
	return "Final";
#elif __DEV	== 0 && __OPTIMIZED	== 1 && __BANK == 0 && __ASSERT	== 0 && __FINAL == 1 && __MASTER == 1 && __PROFILE == 0
	return "Master";
#else 
	return "Custom";
#endif
}

void SetBacktraceVersionInfo()
{
	sysException::SetAnnotation("build_type", GetBuildConfigName());
	sysException::SetAnnotation("build_version", CDebug::GetVersionNumber());

	// Split the raw version into components and set each as a separate annotation
	const char* rawVersion = CDebug::GetRawVersionNumber();
	int versionComponents[4];
	int componentCount = sscanf(rawVersion, "%d.%d.%d.%d", &versionComponents[0], &versionComponents[1], &versionComponents[2], &versionComponents[3]);

	static const char* const keyNames[] = { "build_version_a", "build_version_b", "build_version_c", "build_version_d" };
	for (int i = 0; i < componentCount; i++)
	{
		sysException::SetAnnotation(keyNames[i], versionComponents[i]);
	}

#if RSG_PC
	char osVersion[64] = { 0 };
	CSystemInfo::GetShortWindowsVersion(osVersion, sizeof(osVersion));
	sysException::SetAnnotation("os_version", osVersion);
#endif

}
#endif // BACKTRACE_ENABLED

// This is called from the render thread, sychronously, to finish initialisation that needs to happen on that thread.
void CSystem::RenderThreadInit()
{
#if RSG_PC && NV_SUPPORT
	NvAPI_Status status = NVAPI_ERROR;
	status = NvAPI_Initialize();

	if (status != NVAPI_OK)
	{
		grcWarningf("Failed to Initialize NVidia API Lib");
	}
#endif

	GetRageSetup()->BeginGfx(InWindow());

#if !RSG_PC
	// Frame lock value needs to be set after grcDevice::Init is called.  Don't
	// call this for PC though, as it messes with settings loaded from xml right
	// now.  May need to re-address this in the future though.
	GRCDEVICE.SetFrameLock(DEFAULT_FRAME_LOCK, false);
#endif

#if __XENON
	static u16 adjustmentCurve[256] = {
		0,768,1472,2240,2944,3584,4160,4672,5120,5568,5952,6336,6720,7104,7424,7744,8064,8320,8640,8896,9152,9408,9664,9920,10176,
		10496,10752,11008,11264,11520,11776,12032,12288,12544,12800,13056,13312,13568,13824,14080,14336,14592,14848,15104,15360,15552,
		15808,16064,16320,16576,16832,17088,17344,17600,17856,18112,18368,18624,18816,19072,19328,19584,19840,20096,20352,20608,20864,
		21056,21312,21568,21824,22080,22336,22592,22784,23040,23296,23552,23808,24064,24320,24512,24768,25024,25280,25536,25792,25984,
		26240,26496,26752,27008,27200,27456,27712,27968,28224,28416,28672,28928,29184,29440,29632,29888,30144,30400,30592,30848,31104,
		31360,31552,31808,32064,32320,32576,32768,33024,33280,33536,33728,33984,34240,34432,34688,34944,35200,35392,35648,35904,36160,
		36352,36608,36864,37120,37312,37568,37824,38016,38272,38528,38784,38976,39232,39488,39680,39936,40192,40384,40640,40896,41152,
		41344,41600,41856,42048,42304,42560,42752,43008,43264,43456,43712,43968,44160,44416,44672,44864,45120,45376,45568,45824,46080,
		46272,46528,46784,46976,47232,47488,47680,47936,48192,48384,48640,48896,49088,49344,49536,49792,50048,50240,50496,50752,50944,
		51200,51456,51648,51904,52096,52352,52608,52800,53056,53312,53504,53760,53952,54208,54464,54656,54912,55168,55360,55616,55808,
		56064,56320,56512,56768,56960,57216,57472,57664,57920,58112,58368,58624,58816,59072,59264,59520,59776,59968,60224,60416,60672,
		60864,61120,61376,61568,61824,62016,62272,62528,62720,62976,63168,63424,63616,63872,64128,64320,64576,64768,65024,65216,65472 };

	D3DGAMMARAMP ramp;
	for (u32 i = 0; i < 256; i++)
	{
		ramp.red[i] = adjustmentCurve[i];
		ramp.green[i] = adjustmentCurve[i];
		ramp.blue[i] = adjustmentCurve[i];
	}
	GRCDEVICE.GetCurrent()->SetGammaRamp(0, 0, &ramp);
#endif

	GetRageSetup()->CreateDefaultFactories();

#if RSG_PC
	GRCDEVICE.InitializeStereo();
#endif

#if DEVICE_EQAA
	eqaa::Init();
#endif
#if RSG_ORBIS
	grcRenderTargetGNM::InitMipShader();
#endif

	// Look up the shader variable constants
	CShaderLib::PreInit();

#if !__NO_OUTPUT	
#if RSG_PC
	CShaderLib::SetStereoTexture();
#endif
	RenderBuildInformation();
#endif // !__NO_OUTPUT

#if BACKTRACE_ENABLED
	SetBacktraceVersionInfo();
#endif

	grcTexture::InitFastMipMapper();

	CSprite2d::Init(INIT_CORE); // depends on GetRageSetup()->CreateDefaultFactories

	CStreaming::InitStreamingEngine();	// Must be called before we initialize gDrawListMgr

	gDrawListMgr = rage_new CDrawListMgr();
	DRAWLISTMGR->Initialise();

	OcclusionQueries::Init();

	// initialise renderthread and drawlist mgr
	CGtaRenderThreadGameInterface::InitClass();

#if defined(VIDEO_RECORDING_ENABLED) && VIDEO_RECORDING_ENABLED
	VideoRecording::RenderThreadInit();
#endif
}
//
// name:		CSystem::Init
// description:	Initialise system level classes
bool CSystem::Init(const char* pAppName)
{
	USE_MEMBUCKET(MEMBUCKET_SYSTEM);

#if __XENON
	SetThreadPriority(GetCurrentThread(), PRIO_HIGHEST);
#endif
#if __XENON && !__OPTIMIZED
	//D3D__DisableConstantOverwriteCheck = TRUE;  -- Should be safe now
#endif

	// allowed only during Render Thread, plus this is default state of model cullback anyway:
//	grmModel::SetModelCullback( grmModel::ModelVisibleWithAABB );

	//@@: location CSYSTEM_INIT_INIT_MEMORY_BUCKETS
	InitMemoryBuckets();
	
	SetThisThreadId(SYS_THREAD_UPDATE);
	SetThisThreadId(SYS_THREAD_RENDER);
	gb_mainThread = true;

	pgDictionaryBase::SetListOwnerThread(g_CurrentThreadId);

	//Set the maximum number of archives/packs that can be mounted by the game.
	rage::fiDevice::SetMaxMounts(MAX_ARCHIVES_MOUNTED);

	//Set the maximum number of extra mounts (DLC packs) that can be mounted by the game.
	rage::fiDevice::SetMaxExtraMounts(MAX_EXTRA_MOUNTS);
 
	// The parser needs to be initialized before fwConfigManager.
	INIT_PARSER; // no dependencies

	// Even though Init() might not do anything, we call it to do a Eager Instantiation
	// (see http://www.codeproject.com/Articles/96942/Singleton-Design-Pattern-and-Thread-Safety).
	// However, we cannot use the DCLP approach as its not actually thread safe,
	// see http://www.aristeia.com/Papers/DDJ_Jul_Aug_2004_revised.pdf for a detailed explanation.
	sysUserList::GetInstance().Init();
	g_SysService.InitClass();

	ORBIS_ONLY( g_rlNp.InitServiceDelegate(); )

	CStreaming::InitStreamingVisualize();
	CFileMgr::Initialise();

	// AppContent relies on CFileMgr init
#if RSG_DURANGO
	// The TitleID is read from the AppXManifest using rlXblTitleId::GetXboxLiveConfigurationTitleId().
	// The Japanese title is hard coded below. The game compares the title IDs at runtime in sysAppContent::Init.
	// If a match is found between the Japanese title ID and the runtime title ID, we know we're using the Japanese build.
	sysAppContent::SetTitleId(rlXblTitleId::GetXboxLiveConfigurationTitleId());
	sysAppContent::SetJapaneseTitleId(0x3E961D5B); // from the XDP GTAV Japanese config. Change for other titles.
#endif
	sysAppContent::Init();

#if !__FINAL
	const char* gameConfigPath;
	if(PARAM_altGameConfig.Get(gameConfigPath))
	{
		if(Verifyf(ASSET.Exists(gameConfigPath,""),"Specified altGameConfig file was not found. Falling back to GTAV gameconfig"))
			fwConfigManagerImpl<CGameConfig>::InitClass(gameConfigPath);
		else
			fwConfigManagerImpl<CGameConfig>::InitClass(GAMECFG_FILENAME);
	}
	else
#endif
	{
		fwConfigManagerImpl<CGameConfig>::InitClass(GAMECFG_FILENAME);
	}

	fwInstanceBuffer::InitClass();

	// Apply override from configuration file.
	sysIpcPriority streamerThreadPriority = PRIO_STREAMER_THREAD;

	// number of handles used by streamer has to be a power of two
	const s32 cpuId = __XENON || RSG_DURANGO ? 1 : 0;
	pgStreamer::InitClass(cpuId, streamerThreadPriority);

	// If we're using SysTrayRFS, we should reduce the thread priority for the reader - SysTrayRFS has considerable
	// CPU overhead and interferes with the render thread.
#if __PPU && !__FINAL && 0
	if (!sysBootManager::IsBootedFromDisc())
	{
		// Are we using SysTrayRFS?
		const fiDevice *device = fiDevice::GetDevice("common:/data")->GetLowLevelDevice();

		if (device == &fiDeviceRemote::GetInstance())
		{
			Warningf("Using SysTrayRFS - decreasing DVD Reader thread priority to normal");

			pgStreamer::SetReaderThreadPriority(pgStreamer::OPTICAL, PRIO_BELOW_NORMAL);
		}
	}
#endif // __PPU && !__FINAL

	InitAfterConfigLoad();

	// enable HDR
#if __D3D // || __PPU // we don't want grctfA16B16G16R16F front buffer on PSN
	PARAM_hdr.Set("1");
#endif

	// enable floating point zbuffer on 360
#if __XENON
	PARAM_fpz.Set("1");
#endif

// HACK for B*1536627, "[PT] Crash During Father/Son. game crashes whilst driving near the golf course."
//
// Silently ignoring errors in non-final builds, then crashing in final builds
// is just stupid.  Ok, it's not completely silently ignored, but it is just a
// grcErrorf, which is basically the same thing.  Need to make the error much
// louder so that they all get fixed before we start testing final builds.
//
// #if !__D3D11 && __FINAL
// 	// Simply die on unloaded shaders.
// 	grcEffect::SetEnableFallback(false);
// #endif

#if __XENON && 1 //force device size to be 1280x720 and let the aspect ratio and video scaler sort out the rest
	if(!PARAM_width.Get())
		PARAM_width.Set("1280");
	if(!PARAM_height.Get())
		PARAM_height.Set("720");
#endif

#if RSG_ORBIS || RSG_DURANGO
	if(!PARAM_width.Get())
		PARAM_width.Set("1920");
	if(!PARAM_height.Get())
		PARAM_height.Set("1080");
	if(!PARAM_multiSample.Get())
		PARAM_multiSample.Set("4");
	if(!PARAM_multiFragment.Get())
		PARAM_multiFragment.Set("1");
#endif

#if __PPU
	PARAM_MSAA.Set("MSAA_NONE");
	if(!PARAM_nodepth.Get())
		PARAM_nodepth.Set("1");
	if(!PARAM_force720.Get())
		PARAM_force720.Set("1");
#endif // __PPU
#if FORCE_FRAME_LIMIT
	PARAM_frameLimit.Get(gFrameLimit);
#endif

#if RENDERDOC_ENABLED
	RenderDocManager::Init();
#endif

	grcDevice::InitSingleton();

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	// Load PC settings
	CSettingsManager& settingsManager = CSettingsManager::GetInstance();
	settingsManager.Initialize();
	const Settings& settings = settingsManager.GetSettings();

	GRCDEVICE.SetSize(settings.m_video.m_ScreenWidth, settings.m_video.m_ScreenHeight);
	GRCDEVICE.InitGlobalWindow();
#if RSG_PC
	grcEffect::SetShaderQuality(settings.m_graphics.m_ShaderQuality);
#endif // RSG_PC
#endif

#if !RSG_PC
	VideoResManager::Init();

	if(!CHudTools::GetWideScreen())
		GRCDEVICE.SetLetterBox(false);
#endif

#if __WIN32PC && __BANK
	// Our "native" windows are incompatible with render thread owning the D3D device.  Use RAG or local widgets.
	bkWidget::DisableWindows();
#endif

#if VIDEO_RECORDING_ENABLED && RSG_DURANGO 

#if defined(USE_MMF_XMEM_OVERRIDE_INSTANCE) && USE_MMF_XMEM_OVERRIDE_INSTANCE
    
#if defined(USE_MMF_DECODE_HOOK_ONLY) && USE_MMF_DECODE_HOOK_ONLY

    MEDIA_MEMORY_WRAPPER.Init( sysMemManager::GetInstance().GetReplayAllocator() );

#else

	const CConfigMediaTranscoding& mediaTranscodingConfig = CGameConfig::Get().GetConfigMediaTranscoding();

    // Initialize our memory blocks for media 
    int const c_smallMemSize = mediaTranscodingConfig.m_TranscodingSmallObjectBuffer;
	int const c_smallMaxPointers = mediaTranscodingConfig.m_TranscodingSmallObjectMaxPointers;
    int const c_memSize = mediaTranscodingConfig.m_TranscodingBuffer;
    int const c_maxPointers = mediaTranscodingConfig.m_TranscodingMaxPointers;
	if( Verifyf( c_smallMemSize > 0 && c_memSize > 0 && c_smallMaxPointers > 0 && c_maxPointers > 0,
        "CConfigMediaTranscoding - Invalid values for Media Transcoding memory - %d small objects (MAX), %d small object bytes, %d objects (MAX), %d objects size",
        c_smallMaxPointers, c_smallMemSize, c_maxPointers, c_memSize ) )
	{
		MEDIA_MEMORY_WRAPPER.Init( c_smallMemSize * 1024, (size_t)c_smallMaxPointers, c_memSize * 1024, (size_t)c_maxPointers, sysMemManager::GetInstance().GetReplayAllocator() );
	}

#endif // defined(USE_MMF_DECODE_HOOK_ONLY) && USE_MMF_DECODE_HOOK_ONLY

    rage::MediaTranscodingMemorySystem::SetXMemInstance( &MEDIA_MEMORY_WRAPPER );
    sysMemSetXMemHooks( eXALLOCAllocatorId_Video, rage::MediaTranscodingMemorySystem::sysMemXMemAllocHook, rage::MediaTranscodingMemorySystem::sysMemXMemFreeHook);

#endif // defined(USE_MMF_XMEM_OVERRIDE_INSTANCE) && USE_MMF_XMEM_OVERRIDE_INSTANCE

#endif // VIDEO_RECORDING_ENABLED && RSG_DURANGO && USE_MMF_XMEM_ALLOC_HOOK

	grcSetupInstance = rage_new grmSetup;

	GRCDEVICE.SetDefaultEffectName("common:/shaders/im");
#if DEVICE_EQAA
	eqaa::SetEffectName("common:/shaders/ClearCS");
#endif

	GetRageSetup()->Init( CFileMgr::GetRootDirectory(), pAppName);

#if __BANK
	settingsManager.CreateWidgets();
#endif

	INIT_BUDGETDISPLAY;

	grcEffect::SetDefaultPath("common:/shaders");
#if __XENON
	// DW - just a guess at num segments so far. 0 for segment sizes yields default of 32
	// We're talking Kb here. SetRingBufferParameters actually does a a shift by 10
	GRCDEVICE.SetRingBufferParameters(64,7168,64,true);	
#endif


	{
		USE_MEMBUCKET(MEMBUCKET_RENDER);

		gRenderThreadInterface.Init(PRIO_RENDER_THREAD,RenderThreadInit);

		CSystem::ClearThisThreadId(SYS_THREAD_RENDER); // this is no longer the render thread
#if __BANK && RSG_PC
		grcResourceCache::GetInstance().InitWidgets();
#endif // __BANK

	}

#if RSG_PC
	VideoResManager::Init();

	if(!CHudTools::GetWideScreen())
		GRCDEVICE.SetLetterBox(false);

	GRCDEVICE.InitFocusQueryThread();
#endif

	USE_MEMBUCKET(MEMBUCKET_DEFAULT);

	fwTimer::Init(); // no dependencies
	CControlMgr::Init(INIT_CORE);// no dependencies

#if ENABLE_AUTOMATED_TESTS
	AutomatedTest::Init();
#endif // ENABLE_AUTOMATED_TESTS

	// if both left and right shoulder pads are pressed then clear the disk cache
	CControlMgr::Update();
	if(CControlMgr::GetPlayerPad() && CControlMgr::GetPlayerPad()->GetLeftShoulder1() && CControlMgr::GetPlayerPad()->GetRightShoulder1())
	{
		OUTPUT_ONLY(printf("Clearing HDD cache\n"));
		PARAM_clearhddcache.Set("1");
		PARAM_ignoreprofile.Set("1");
	}

	SetWindowsSystemParameters();// no dependencies

	// create default task scheduler
	// Determine the priority, from the default value or an override in the configuration file.
	sysIpcPriority sysTaskPri = CGameConfig::Get().GetThreadPriority(ATSTRINGHASH("systask", 0x0461fa38d), SYS_DEFAULT_TASK_PRIORITY);

#	if __DEV
		const unsigned stackSize = 0x18000;
#	else
		const unsigned stackSize = 0x10000;
#	endif
	const unsigned scratchSize = 0x8000;

	u32 cpuMask = SYS_DEFAULT_TASK_THREAD_MASK;
#if RSG_PC
	u32 numTaskSchedulers = SYS_DEPENDENCY_TASKACTIVENUMWORKERS;
	if (PARAM_numTaskSchedulers.Get(numTaskSchedulers))
	{
		numTaskSchedulers = (numTaskSchedulers > 1) ? numTaskSchedulers : 1;
	}

	cpuMask = (1 << numTaskSchedulers) - 1;
#endif // RSG_PC

	sysTaskManager::AddScheduler("gtaSched", stackSize, scratchSize, sysTaskPri, cpuMask,2);

#if __BANK
	g_PickerManager.InitClass(rage_new CGtaPickerInterface());
#endif	//	__BANK

	// Set the main thread's priority and core affinity.
	const CThreadSetup* pThreadSetup = CGameConfig::Get().GetThreadSetup(ATSTRINGHASH("Update", 0x0b6a7a93e));
	if(pThreadSetup)
	{
		sysIpcSetThreadPriority(sysIpcGetThreadId(), pThreadSetup->GetPriority(sysIpcGetCurrentThreadPriority()));
#if RSG_DURANGO || RSG_ORBIS
		sysIpcSetThreadProcessorMask(sysIpcGetThreadId(), pThreadSetup->GetCpuAffinityMask( (1<<0) ));
#endif
	}

#if RSG_PC
	if(CSettingsManager::GetInstance().IsLowQualitySystem())
	{
		// AMD min spec video card + 4-core CPU. Extremely render thread 
		// bound in this case because of single-threaded driver, we hard-code 
		// a swap of main thread and render thread priorities in this case.
		const CThreadSetup* pUpdateThreadSetup = CGameConfig::Get().GetThreadSetup(ATSTRINGHASH("Update", 0x0b6a7a93e));
		const CThreadSetup* pRenderThreadSetup = CGameConfig::Get().GetThreadSetup(ATSTRINGHASH("Render", 0x8EE0C0F));
		if(pUpdateThreadSetup && pRenderThreadSetup)
		{
			// Set Update thread prio to render thread prio.
			sysIpcSetThreadPriority(sysIpcGetThreadId(), pRenderThreadSetup->GetPriority(sysIpcGetCurrentThreadPriority()));

			// Set Render thread prio to update thread prio.
			sysIpcSetThreadPriority(gRenderThreadInterface.GetRenderThreadId(), pUpdateThreadSetup->GetPriority(sysIpcGetCurrentThreadPriority()));
		}
	}
#endif

	// Now is a great time to let the command line override existing thread priorities, and add the main thread.
#if THREAD_REGISTRY
	sysThreadRegistry::RegisterThread(sysIpcGetThreadId(), sysIpcGetCurrentThreadPriority(), "Main Thread", "update", true);
	sysThreadRegistry::AdjustPriosBasedOnParams();
#endif // THREAD_REGISTRY

	InitDLCCommands();

#if RSG_ORBIS && !__FINAL
	if( PARAM_enableGpuTraceCapture.Get() )
	{
		grcGpuTraceGNM::InitClass();

		SceRazorGpuThreadTraceParams params;
		grcGpuTraceGNM::InitThreadTraceParams(&params, grcGpuTraceGNM::grcTraceFrequencyLow);

		SceRazorGpuErrorCode ret = 0;
		uint32_t *streamingCounters = params.streamingCounters;

		ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterDrawnBusy, SCE_RAZOR_GPU_BROADCAST_ALL);
		ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterDrawnPixel, SCE_RAZOR_GPU_BROADCAST_ALL);

		int numCounters = streamingCounters - params.streamingCounters;
		Assert(ret == SCE_OK);

		grcGpuTraceGNM::InitThreadTrace(&params, numCounters);
	}
#endif // RSG_ORBIS && !__FINAL

	return true;
}

// name:		CSystem::RegisterDrawListCommands
// description:	Register all custom draw list commands managed by CSystem
void CSystem::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdBeginRender);
	DLC_REGISTER_EXTERNAL(dlCmdEndRender);

// TODO: When this function gets moved to framework, please update UpdateTask functions in clothManager.cpp and ropeManager.cpp to call it so we don't have to copy-paste code around
// Svetli
#if RSG_PC &&__D3D11
	// DX11 TODO:- Find a place within rage or the framework where this can be called so it`s not game specific.
	// This means dlCmdGrcDeviceUpdateBuffer::AddBufferUpdateCommandToDrawList() is called whenever a vertex/index buffer
	// is altered on this thread. The render thread then passes the new contents to the GPU by executing a DL command.
	grcBufferD3D11::SetInsertUpdateCommandIntoDrawListFunc(dlCmdGrcDeviceUpdateBuffer::AddBufferUpdate);
	// Occasionally a buffer is updated and deleted in the same frame. This mechanism avoids operating on a deleted buffer.
	grcBufferD3D11::SetCancelPendingUpdateGPUCopy(dlCmdGrcDeviceUpdateBuffer::CancelBufferUpdate);
	grcBufferD3D11::SetAllocTempMemory(dlCmdGrcDeviceUpdateBuffer::AllocTempMemory);

	// Similar, only for textures.
	grcTextureD3D11::SetInsertUpdateCommandIntoDrawListFunc(dlCmdGrcDeviceUpdateBuffer::AddTextureUpdate);
	grcTextureD3D11::SetCancelPendingUpdateGPUCopy(dlCmdGrcDeviceUpdateBuffer::CancelBufferUpdate);
	grcTextureD3D11::SetAllocTempMemory(dlCmdGrcDeviceUpdateBuffer::AllocTempMemory);
#endif // RSG_PC && __D3D11
}

#if RSG_ORBIS &!__FINAL
void CSystem::StartGPUTraceCapture()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	g_requestedTraceCapture = true;
}
#endif // RSG_ORBIS &!__FINAL


#if !__NO_OUTPUT
void CSystem::CenteredPrint(float yPos, const char *string)
{
	int width = grcFont::GetCurrent().GetStringWidth(string, istrlen(string));

	grcFont::GetCurrent().Draw((float) (GRCDEVICE.GetWidth() - width) / 2.0f, yPos, string);
}

void CSystem::RenderBuildInformation()
{
	if (!CSystem::BeginRender())
	{
		return;
	}

	{
		GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __PS3
		grcTextureFactory::GetInstance().BindDefaultRenderTargets();
#elif RSG_DURANGO && _XDK_VER >= 10542
		// Feb XDK unbinds RT and depth/stencil on present
		grcTextureFactory::GetInstance().LockRenderTarget(0, grcTextureFactory::GetInstance().GetBackBuffer(), grcTextureFactory::GetInstance().GetBackBufferDepth());
#endif

		const grcViewport* pVp = grcViewport::GetDefaultScreen();
		grcViewport::SetCurrent(pVp);

		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);

		grcLightState::SetEnabled(false);
		grcWorldIdentity();

#if RSG_ORBIS || RSG_DURANGO || RSG_PC
		GRCDEVICE.Clear( true, Color32(0,0,32,255), false, 1.0f, false, 0 );
#elif __PS3
		GRCDEVICE.ClearRect(0, 0, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), true, Color32(0,0,32,255),false,1.0f,false,0);
#else //__PS3
		GRCDEVICE.ClearRect(0, 0, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), 0.0f, Color32(0,0,32,255));
#endif //__PS3

		// Force a flush of these shader variables to make sure they're being set
		CShaderLib::SetGlobalAlpha(0.0f);
		CShaderLib::SetGlobalEmissiveScale(0.0f);

		CShaderLib::SetGlobalAlpha(1.0f);
		CShaderLib::SetGlobalEmissiveScale(1.0f);

#if __XENON
		GRCDEVICE.SetColorExpBiasNow(0);
#endif // __XENON

		PUSH_DEFAULT_SCREEN();
		grcFont::GetCurrent().SetInternalColor(Color32(0xffffffff));
		grcFont::GetCurrent().SetInternalScale(1.5f);

		float fontHeight = (float) grcFont::GetCurrent().GetHeight();

		// "Animate" the title.
		u32 animCycle = (sysTimer::GetSystemMsTime() / 1000) & 3;
		const char *suffix = "...";
		const char *suffix2 = "   ";
		char title[64];
		formatf(title, "Now loading Grand Theft Auto V%s%s", &suffix[3 - animCycle], &suffix2[animCycle]);

		CenteredPrint(100.0f, title);

		CenteredPrint(130.0f, RSG_PLATFORM " " RSG_CONFIGURATION);

		const char* cVersionNumber = CDebug::GetVersionNumber();
		if( cVersionNumber[0] != 0 )
		{
			formatf(title, "v%s", cVersionNumber);
			CenteredPrint(145.0f, title);
		}

		const char *localHost = fiDeviceTcpIp::GetLocalHost();
		formatf(title, "Workstation IP: %s", localHost);
		CenteredPrint(170.0f, title);

		CenteredPrint(210.0f, "Command line arguments:");

		const float yTop = 230.0f;
		float yPos = yTop;

		int argCount = sysParam::GetArgCount();
		for (int x=1; x<argCount; x++)
		{
			const char *arg = sysParam::GetArg(x);
			// Skip disabled arguments.
			if (arg[1] == 'X')
			{
				continue;
			}

			CenteredPrint(yPos, arg);
			yPos += fontHeight;
		}

		POP_DEFAULT_SCREEN();

#if RSG_DURANGO && _XDK_VER >= 10542
		// Feb XDK unbinds RT and depth/stencil on present
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif
		grcFont::GetCurrent().SetInternalScale(1.0f);

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	}
	CSystem::EndRender();
}
#endif // !__NO_OUTPUT

void CSystem::InitAfterConfigLoad()
{
	const fwConfigManager& mgr = fwConfigManager::GetInstance();

	fwArchetypeManager::SetMaxArchetypes(mgr.GetSizeOfPool(ATSTRINGHASH("fwArchetypePooledMap", 0x76bc4f20), CONFIGURED_FROM_FILE));

	// Determine the value of MAXNOOFPEDS and VEHICLE_POOL_SIZE, from the
	// configuration file.
	CPoolHelpers::SetPedPoolSize(mgr.GetSizeOfPool(ATSTRINGHASH("Peds", 0x8da12117), CONFIGURED_FROM_FILE));
	CPoolHelpers::SetVehiclePoolSize(mgr.GetSizeOfPool(ATSTRINGHASH("Vehicles", 0x74b31be3), CONFIGURED_FROM_FILE));
	CVehiclePopulation::ms_maxNumberOfCarsInUse = VEHICLE_POOL_SIZE;
}

//
// name:		CSystem::Shutdown
// description:	Shutdown system level classes
void CSystem::Shutdown()
{
#if defined(VIDEO_RECORDING_ENABLED) && VIDEO_RECORDING_ENABLED 
	VideoRecording::RenderThreadCleanup();
#endif

	g_SysService.ShutdownClass();
	sysUserList::GetInstance().Shutdown();
	sysAppContent::Shutdown();

	SHUTDOWN_PARSER;
	
	CShaderLib::Shutdown();
	CControlMgr::Shutdown(SHUTDOWN_CORE);

#if __BANK
	g_PickerManager.ShutdownClass();
#endif	//	__BANK

	pgStreamer::ShutdownClass();	// Used to be done within strStreamingEngine::Shutdown().

	grmSetup* setup = GetRageSetup();
	if(setup)
	{
 		setup->DestroyFactories();
//		setup->EndGfx();
		setup->Shutdown();
		delete GetRageSetup();
		grcSetupInstance = NULL;
	}

#if VIDEO_RECORDING_ENABLED && RSG_DURANGO && USE_MMF_XMEM_OVERRIDE_INSTANCE

	sysMemSetXMemHooks( eXALLOCAllocatorId_Video, XMemAllocDefault, XMemFreeDefault);
	rage::MediaTranscodingMemorySystem::SetXMemInstance( NULL );
	MEDIA_MEMORY_WRAPPER.Shutdown();

#endif // VIDEO_RECORDING_ENABLED && RSG_DURANGO && USE_MMF_XMEM_ALLOC_HOOK

	fwInstanceBuffer::ShutdownClass();

	fwConfigManager::ShutdownClass();

#if RENDERDOC_ENABLED
	RenderDocManager::Shutdown();
#endif
}

//
// name:		CSystem::BeginUpdate
// description:	Called at the beginning of the update 
void CSystem::BeginUpdate()
{
	USE_MEMBUCKET(MEMBUCKET_SYSTEM);
	// DW / JG - because the widget code can call viewport sphere visibility code via the streaming cleanup 
	// of create car, I have decided to put this here so that all widget code by default will assume access 
	// to viewports is the GAME viewport.
	CViewport *pVp = gVpMan.GetGameViewport();
	if (pVp)
		gVpMan.PushViewport(pVp);

	// Give the reservation back when non-gameplay menu is active (this is a Microsoft requirement)
	DURANGO_ONLY( GRCDEVICE.SetNuiGPUReservation(CPauseMenu::IsActive()) );

	GetRageSetup()->BeginUpdate();

	if (pVp)
		gVpMan.PopViewport();

	fwTimer::Update();

	g_SysService.UpdateClass();

#if __XENON && __ASSERT
	MEMORYSTATUS ms;
	GlobalMemoryStatus(&ms);
	size_t remain = ms.dwAvailPhys >> 10;
	Assertf(remain > 100, "Free title memory below safe value (100k) : %dk", remain);		// e.g. MP voice chat requires 80k of title free mem...
#endif //__XENON && __ASSERT

#if __PS3
	static u32 frameCount = 0;
	frameCount = (frameCount+1)%2000;
	if (frameCount == 0)
	{
		if (snIsDebuggerRunning() != 0)
		{
			Displayf("debugger detected");
			ms_bDebuggerDetected = true;
			CNetworkTelemetry::AppendDebuggerDetect(1);
		}
	}
#endif //__PS3
}

//
// name:		CSystem::EndUpdate
// description:	Called at the end of the update 
void CSystem::EndUpdate()
{
	USE_MEMBUCKET(MEMBUCKET_SYSTEM);
	GetRageSetup()->EndUpdate(fwTimer::IsGamePaused()?0.0f:fwTimer::GetTimeStepInMilliseconds());

	//grcDebugDraw::AddDebugOutput("None: %d Nonresident: %d", grcTexture::None->GetRefCount(), grcTexture::Nonresident->GetRefCount());
	// Add-on to rage's changelist #130399:
	//grcTexture::None->SetRefCount(32768);
	//grcTexture::Nonresident->SetRefCount(32768);

	fwInstanceBuffer::NextFrame();
}

//
// name:		CSystem::BeginRender
// description:	Called at the beginning of the render 
bool CSystem::BeginRender()
{
	USE_MEMBUCKET(MEMBUCKET_SYSTEM);
	if (!GetRageSetup()->BeginDraw(false))
	{
		return false;
	}
	
#if __BANK && __XENON
	StartRingBufferMemUsage();
#endif

#if RSG_ORBIS && !__FINAL
	if( PARAM_enableGpuTraceCapture.Get() )
	{
		if( g_traceCaptureState == 0 )
		{
			grcGpuTraceGNM::Start();
			g_traceCaptureState++;
		}
	}
#endif // RSG_ORBIS && !__FINAL
	
#if __D3D11 && RSG_PC
	grcTextureD3D11::SetInsertUpdateCommandIntoDrawListFunc(dlCmdGrcDeviceUpdateBuffer::AddTextureUpdate);
	grcTextureD3D11::SetCancelPendingUpdateGPUCopy(dlCmdGrcDeviceUpdateBuffer::CancelBufferUpdate);
#endif //__D3D11 && RSG_PC

    VIDEO_PLAYBACK_ONLY( VideoPlayback::UpdatePreRender() );
    VIDEO_PLAYBACK_ONLY( VideoPlaybackThumbnailManager::UpdateThumbnailFrames() );

#if __D3D11 && RSG_PC
	grcTextureD3D11::SetInsertUpdateCommandIntoDrawListFunc(NULL);
	grcTextureD3D11::SetCancelPendingUpdateGPUCopy(NULL);
#endif

	CScaleformMgr::BeginFrame();

#	if RSG_PC && __D3D11
		DECALMGR.PrimaryRenderThreadBeginFrameCallback();
#	endif

	return true;
}

// drawlist version of begin render
void CSystem::AddDrawList_BeginRender()
{
	gDrawListMgr->BeginDrawListCreation();

	DLC(dlCmdNewDrawList, (DL_BEGIN_DRAW));
#if RSG_PC && __D3D11
	dlCmdGrcDeviceUpdateBuffer::OnBeginRender();
#endif // RSG_PC && __D3D11

#if DEBUG_DRAW
	grcDebugDraw::Update();
#endif // DEBUG_DRAW

	DLC_Add(CRenderTargets::ResetRenderThreadInfo);
	g_timeCycle.SetupRenderThreadFrameInfo();
	Lights::SetupRenderthreadFrameInfo();
	CRenderPhaseCascadeShadowsInterface::ResetRenderThreadInfo();

	CVisualEffects::SetupRenderThreadFrameInfo();
	g_ptFxManager.BeginFrame();

	DLC(CSetDrawableLODCalcPos, (camInterface::GetPos()));
	DLC(dlCmdBeginRender, ());
// 	if (g_draw_cached_entities)
// 		DLC(CCreateRenderListGroupDC, ());
	CParaboloidShadow::UpdateActiveCachesfromStatic();
#if SHADOW_ENABLE_ASYNC_CLEAR
	DLC_Add(CRenderPhaseCascadeShadowsInterface::ClearShadowMapsAsync);
#endif
	gDrawListMgr->EndCurrentDrawList();
}


// Zeno frame limiter
inline void FrameLimiter() 
{
#if FORCE_FRAME_LIMIT 
	PF_PUSH_TIMEBAR("Frame Limit");
	// PC should frame-limits here any time it's in full-screen mode; windowed mode will do a manual WaitForVBlank at a lower level in grcDevice::EndFrame
	if ( ((gFrameLimit > 0) || RSG_PC) DEV_ONLY(|| fwTimer::GetForceMinimumFrameRate() || fwTimer::GetForceMaximumFrameRate()) )
	{
		// frame limiter. Game runs at max speed of some many frames per second. This has to be at the end of this function
		float frameTime = gFrameTimer.GetTime();
		#if RSG_PC
			const float fRefreshRate = static_cast<float>( grcDevice::GetRefreshRate() );
			const float fVSync = static_cast<float>( CSettingsManager::GetInstance().GetSettings().m_video.m_VSync );
			
			// If v-sync is disabled (fVSync==0) or we get an invalid refresh rate then use 1/256 which was the previously hardcoded minFrameTime for PC
			float minFrameTime = (fVSync > 0.0f) && (fRefreshRate > 0.0f)  ? (fVSync / fRefreshRate) : (1.0f / 256.0f); 
		#else
			float minFrameTime = gFrameLimit/60.0f;
		#endif

		// Let us out of the busy wait early enough to meet up with the v-sync. This is necessary since
		// we still have a bit of work to do after the busy wait before we call Present in GetRageSetup()->EndDraw()
		const float slopTime = minFrameTime * 0.05f;

		#if !__FINAL && !RSG_PC
			// debug to let us force a slow and/or uneven frame rate
			if(fwTimer::GetForceMaximumFrameRate())
				minFrameTime = fwTimer::GetMinimumFrameTime();
			if(fwTimer::GetForceMinimumFrameRate())
				minFrameTime = fwTimer::GetMaximumFrameTime();
			else if(fwTimer::GetForceUnevenFrameRate() && fwTimer::GetSystemFrameCount()&1)
				minFrameTime = fwTimer::GetMaximumFrameTime();
		#endif

		while ((frameTime+slopTime) < minFrameTime)
		{
			// Sleep for half the difference
			int zeno = int((minFrameTime-frameTime)*1000)/2; 
			sysIpcSleep(zeno);

			frameTime = gFrameTimer.GetTime();
		}

		// frame timer has to be at the top of this function
		gFrameTimer.Reset();
	}
	PF_POP_TIMEBAR();
#endif // USE_FRAME_LIMIT
}


//
// name:		CSystem::EndRender
// description:	Called at the end of the render 
void CSystem::EndRender()
{
	USE_MEMBUCKET(MEMBUCKET_SYSTEM);

	// reset default vp mapping to Screen() as expected by stuff in grcSetup::EndDraw() (e.g. RAG mouse cursor):
	CSprite2d::ResetDefaultScreenVP();

	CScaleformMgr::EndFrame();

#if __BANK && __XENON
	EndRingBufferMemUsage();
#endif

#if FRAME_LIMIT_BEFORE_PRESENT
#if RSG_DURANGO
	if (CLoadingScreens::GetCurrentContext() != LOADINGSCREEN_CONTEXT_INTRO_MOVIE)
#endif
	FrameLimiter();
#endif


#if !__FINAL	
	PUSH_DEFAULT_SCREEN();		
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	STREAM_TICKER_ONLY( StrTicker::Draw(60.0f,300.0f) );	
	POP_DEFAULT_SCREEN();
#endif	


	PF_PUSH_TIMEBAR_DETAIL("grmSetup EndDraw");
	GetRageSetup()->EndDraw();
	PF_POP_TIMEBAR_DETAIL();

#if FRAME_LIMIT_AFTER_PRESENT
	FrameLimiter();
#endif

#if __PPU
	// only this thread should pick up the base signal - all over threads should hang *only* on detection of g_IsExiting, which is set below
	if (GetRageSetup()->WantExit())
	{
		printf("*** User quit or disc eject\n");
		printf("*** Calling exit...\n");
		g_IsExiting = true;
		g_movieMgr.Shutdown(SHUTDOWN_CORE);
		sysIpcSleep(1000);		// Hacky, but give the worker threads enough time to see the flag before the OS starts tearing things down.
		// And wait for the trophy thread, 'cos that takes longer
		g_rlNp.GetTrophy().Shutdown();
		_Exit(0);
	}
#endif

#if RSG_ORBIS && !__FINAL
	if( PARAM_enableGpuTraceCapture.Get() )
	{
		switch( g_traceCaptureState )
		{
			case 1:
				grcGpuTraceGNM::Stop();
				g_traceCaptureState++;
				break;
			case 2:
				{
					static int traceCapturecount = 0;
					char capturename[128];
					sprintf(capturename,"capture_%d",traceCapturecount++);
					grcGpuTraceGNM::Save(capturename);
					g_traceCaptureState++;
				}
				break;
			case 3:
				grcGpuTraceGNM::Reset();
				g_traceCaptureState++;
				break;
			default:
				/* NoOp */
				break;
		}
	}
#endif // RSG_ORBIS && !__FINAL
}

void CSystem::AddDrawList_EndRender(){
#if RSG_PC && __D3D11
	dlCmdGrcDeviceUpdateBuffer::OnEndRender();
#endif // RSG_PC && __D3D11
	DLC( dlCmdNewDrawList, (DL_END_DRAW));
	DLC( dlCmdInsertEndFence, ());
	DLC( dlCmdEndRender, ());
	g_timeCycle.ClearRenderFrameInfo();
	Lights::ClearRenderthreadFrameInfo();
	CVisualEffects::ClearRenderThreadFrameInfo();
	gDrawListMgr->EndCurrentDrawList();
	gDrawListMgr->EndDrawListCreation();
}

//
// name:		CSystem::WantToExit
// description:	Returns if there is a reason to exit 
bool CSystem::WantToExit()
{
	bool bWantToExit;
#if __WIN32PC
	bWantToExit = GetRageSetup()->WantExit();
#else
	bWantToExit = false; // INTENTIONALLY not hooked up to user quit any longer: GetRageSetup()->WantExit();
#endif

#if !__FINAL
	bWantToExit = bWantToExit || SmokeTests::HasSmokeTestFinished();
#endif

#if RSG_PC && __DEV
	bWantToExit |= CSettingsManager::ResetTestsCompleted();
#endif
//	if (!bWantToExit) 
//		bWantToExit = (CFrontEnd::GetState() == FRONTEND_STATE_EXITAPP); // to do - new pause menu doesnt have this state...
#if RSG_PC
	bWantToExit = bWantToExit || CPauseMenu::GetGameWantsToExitToWindows();
#endif

	return bWantToExit;

//	return GetRageSetup()->WantExit();
}

bool CSystem::WantToRestart()
{
#if RSG_PC
	return CPauseMenu::GetGameWantsToRestart();
#else
	return false;
#endif // RSG_PC
}

bool CSystem::IsDisplayingSystemUI()
{
	return (CLiveManager::IsSystemUiShowing());
}

// set the current thread id for this thread
void CSystem::SetThisThreadId(eThreadId currThread) { 

	Assert(currThread != SYS_THREAD_INVALID); 
	sysThreadType::AddCurrentThreadType(currThread); 
}

void CSystem::ClearThisThreadId(eThreadId currThread) {
	Assert(currThread != SYS_THREAD_INVALID);

	sysThreadType::ClearCurrentThreadType(currThread); 

	Assert(sysThreadType::GetCurrentThreadType() != SYS_THREAD_INVALID);
}

bool CSystem::InWindow()
{
#if __WIN32PC
    bool inWindow = true;

#if !__FINAL
    if (PARAM_fullscreen.Get())
		inWindow = false;
	else if (PARAM_windowed.Get())
		inWindow = true;
#else
	inWindow = false;
	if (PARAM_windowed.Get())
		inWindow = true;
#endif

    return inWindow;
#else
	return false;
#endif
}

#if (1 == __BANK) && (1 == __XENON)

//------------------------------------------
// name:		RingBuffer stats system.
// description: Give usage of the ringbuffer a usage greater or equal than the ring buffer size 
//				is not always a bad thing : If the GPU and the CPU sync is going fine, part of 
//				the buffer can be reused during the same frame.
//
//		NOTE: this moved from viewportManager.cpp, since we need it as close to the beginin and end of the frame as possible in the render thread
//
#if __XENON
static u32 RingBufferStart = 0xFFFFFFFF;
static u32 RingBufferEnd = 0x0;
#endif // __XENON

static u32 Global_RingBufferUsage_Min = 0xFFFFFFFF;
static u32 Global_RingBufferUsage_Max = 0x0;
static u64 Global_RingBufferUsage_Accum = 0x0;
static u32 Global_RingBufferUsage_Hit = 0x0;

static u32 Recent_RingBufferUsage_Min = 0xFFFFFFFF;
static u32 Recent_RingBufferUsage_Max = 0x0;
static u32 Recent_RingBufferUsage_Accum = 0x0;
static u32 Recent_RingBufferUsage_Hit = 0x0;

static bool Global_Reset = false;
static bool Render_RingBuffer_Stats = false;
static u32 Recent_ResetHitCount = 30*3;

static u32 CurrentFrame_RingStart;
static u32 CurrentFrame_RingLimitStart;


void CSystem::SetupRingBufferWidgets(bkBank& bank)
{
	bank.PushGroup("RingBuffer",false);
	bank.AddToggle("Render stats",&Render_RingBuffer_Stats);
	bank.AddToggle("Reset stats",&Global_Reset);
	bank.AddSlider("Reset recent stats frame count",&Recent_ResetHitCount,1,3500,1);
	bank.PopGroup();
}

#if __XENON
const u32 CalcRingUsage(const u32 ringStart, const u32 ringEnd)
{
	if( ringEnd < ringStart )
	{
		// Looped : Not counted, there's something funny in the normal usage..
		return (RingBufferEnd - ringStart) + (ringEnd - RingBufferStart);
		//return 0xFFFFFFFF;
	}

	return ringEnd - ringStart;
}
#elif __PPU
const u32 CalcRingUsage()
{
	return g_commandBufferUsage;
}
#endif  // __XENON || __PPU

inline u32 CalcRingSize(void)
{
#if __XENON
	return RingBufferEnd - RingBufferStart;
#elif __PPU
	return g_bufferSize;
#endif
}

void DisplayRingStats(void)
{
	static int StartX = 100;
	static int StartY = 10;

	int y = StartY;
	int x = StartX;

	const float ringSize = (float)(CalcRingSize());

	char debugText[50];

#if __XENON	
	sprintf(debugText,"Ring Size : %4dKb (%p - %p)",(s32)(ringSize/1024.0f),RingBufferStart,RingBufferEnd);
#else
	sprintf(debugText,"Ring Size : %4dKb",(s32)(ringSize/1024.0f));
#endif	
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	int savedY = y;
	sprintf(debugText,"Since Reset");
	grcDebugDraw::PrintToScreenCoors(debugText,x,y);
	y++;

	sprintf(debugText,"Min: %4dKb (%d%%%%)",(s32)(Global_RingBufferUsage_Min/1024.0f),(s32)((100*Global_RingBufferUsage_Min)/ringSize));
	grcDebugDraw::PrintToScreenCoors(debugText,x,y);
	y++;

	sprintf(debugText,"Max: %4dKb (%d%%%%)",(s32)(Global_RingBufferUsage_Max/1024.0f),(s32)((100*Global_RingBufferUsage_Max)/ringSize));
	grcDebugDraw::PrintToScreenCoors(debugText,x,y);
	y++;

	float avgUsage = ((float)Global_RingBufferUsage_Accum)/((float)Global_RingBufferUsage_Hit);
	sprintf(debugText,"Avg: %4dKb (%d%%%%)",(s32)(avgUsage/1024.0f),(s32)((100*avgUsage)/ringSize));
	grcDebugDraw::PrintToScreenCoors(debugText,x,y);

	x += 16;
	y = savedY;
	sprintf(debugText,"Over %d frames",Recent_ResetHitCount);
	grcDebugDraw::PrintToScreenCoors(debugText,x,y);
	y++;

	sprintf(debugText,"Min: %4dKb (%d%%%%)",(s32)(Recent_RingBufferUsage_Min/1024.0f),(s32)((100*Recent_RingBufferUsage_Min)/ringSize));
	grcDebugDraw::PrintToScreenCoors(debugText,x,y);
	y++;

	sprintf(debugText,"Max: %4dKb (%d%%%%)",(s32)(Recent_RingBufferUsage_Max/1024.0f),(s32)((100*Recent_RingBufferUsage_Max)/ringSize));
	grcDebugDraw::PrintToScreenCoors(debugText,x,y);
	y++;

	avgUsage = ((float)Recent_RingBufferUsage_Accum)/((float)Recent_RingBufferUsage_Hit);
	sprintf(debugText,"Avg: %4dKb (%d%%%%)",(s32)(avgUsage/1024.0f),(s32)((100*avgUsage)/ringSize));
	grcDebugDraw::PrintToScreenCoors(debugText,x,y);
	y++;
}

static void StartRingBufferMemUsage()
{
	if( Render_RingBuffer_Stats)
	{
		DisplayRingStats();
	}

	CurrentFrame_RingStart = (u32)GRCDEVICE.GetCurrent()->m_pRing;
	CurrentFrame_RingLimitStart = (u32)GRCDEVICE.GetCurrent()->m_pRingLimit;
}
static void EndRingBufferMemUsage()
{
	bool resetUsage = false;

	if( Recent_RingBufferUsage_Hit >= Recent_ResetHitCount )
	{
		resetUsage = true;
	}

#if __XENON
	const u32 ringEnd = (u32)GRCDEVICE.GetCurrent()->m_pRing;
	const u32 ringLimitEnd = (u32)GRCDEVICE.GetCurrent()->m_pRingLimit;
	if( CurrentFrame_RingStart < RingBufferStart )
	{
		RingBufferStart = CurrentFrame_RingStart;
		resetUsage = true;
	}

	if( ringEnd < RingBufferStart )
	{
		RingBufferStart = ringEnd;
		resetUsage = true;
	}

	if( CurrentFrame_RingLimitStart > RingBufferEnd )
	{
		RingBufferEnd = CurrentFrame_RingLimitStart;
		resetUsage = true;
	}

	if( ringLimitEnd > RingBufferEnd )
	{
		RingBufferEnd = ringLimitEnd;
		resetUsage = true;
	}
#endif // __XENON

	if( resetUsage )
	{
		Recent_RingBufferUsage_Min = 0xFFFFFFFF;
		Recent_RingBufferUsage_Max = 0x0;
		Recent_RingBufferUsage_Accum = 0x0;
		Recent_RingBufferUsage_Hit = 0x0;
	}
	if( Global_Reset )
	{
		Global_RingBufferUsage_Min = 0xFFFFFFFF;
		Global_RingBufferUsage_Max = 0x0;
		Global_RingBufferUsage_Accum = 0x0;
		Global_RingBufferUsage_Hit = 0x0;

		Global_Reset = false;
	}

#if __XENON
	const u32 ringUsage = CalcRingUsage(CurrentFrame_RingStart,ringEnd);
#else // __XENON	
	const u32 ringUsage = CalcRingUsage();
#endif // __XENON	
	if( ringUsage != 0xFFFFFFFF )
	{
		if( ringUsage < Recent_RingBufferUsage_Min )
		{
			Recent_RingBufferUsage_Min = ringUsage;
		}

		if( ringUsage > Recent_RingBufferUsage_Max )
		{
			Recent_RingBufferUsage_Max = ringUsage;
		}

		if( ringUsage < Global_RingBufferUsage_Min )
		{
			Global_RingBufferUsage_Min = ringUsage;
		}

		if( ringUsage > Global_RingBufferUsage_Max )
		{
			Global_RingBufferUsage_Max = ringUsage;
		}

		Recent_RingBufferUsage_Accum += ringUsage;
		Recent_RingBufferUsage_Hit++;

		Global_RingBufferUsage_Accum += ringUsage;
		Global_RingBufferUsage_Hit++;
	}
}

#endif // (1 == __XENON) && (1 == __BANK)

