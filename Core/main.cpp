// 
// name:		main.cpp
// description:	contains entry function
//

#include "file/device_installer.h"
#include "fwutil/Gen9Settings.h"
#include "input/keyboard.h"
#include "input/keys.h"
#include "profile/rocky.h"
#include "system/memory.h"
#include "system/buddyallocator_config.h"
#include "system/platform.h"
#include "system/smallocator.h"
#include "system/threadtype.h"
#include "system/tmcommands.h"
#include "diag/tracker.h"

#include "streaming/streamingvisualize.h"

#if !__FINAL && RSG_ORBIS
#include <libdbg.h>
#endif

#define PGKNOWNREFPOOLSIZE ((__XENON || __PS3) ? 0x4000 : 0x8800)

#if RSG_PC || RSG_DURANGO || RSG_ORBIS

#if __64BIT

#if !defined(POOL_HEAP_SIZE)
# define POOL_HEAP_SIZE			(24*1024)
#endif

#if USE_SPARSE_MEMORY
	#define SIMPLE_HEAP_SIZE		(620*1024)
#elif RSG_PC
	#define SIMPLE_HEAP_SIZE		(474*1024)
#elif RSG_DURANGO
	#define SIMPLE_HEAP_SIZE		(596*1024)
#elif RSG_ORBIS
	#define SIMPLE_HEAP_SIZE		(596*1024)
#else
	#define SIMPLE_HEAP_SIZE		(768*1024)
#endif

#if RSG_ORBIS
	#define FLEX_HEAP_SIZE (22 * 1024)
#endif

#if RSG_ORBIS || RSG_DURANGO
#if ENABLE_DEBUG_HEAP
#define HEMLIS_HEAP_SIZE (17 * 1024)
#else
#define HEMLIS_HEAP_SIZE (220 * 1024)
#endif
#endif

#if ENABLE_DEBUG_HEAP && (RSG_DURANGO || RSG_ORBIS || RSG_PC)
#define RECORDER_HEAP_SIZE (20 * 1024)
#endif

#if USE_SPARSE_MEMORY && !FREE_PHYSICAL_RESOURCES
#define SIMPLE_VIRTUAL_SIZE		(0)
#elif RSG_ORBIS
#define SIMPLE_VIRTUAL_SIZE		(0)
#elif RSG_DURANGO
#define SIMPLE_VIRTUAL_SIZE		(0)
#else
#define SIMPLE_VIRTUAL_SIZE		(1024*1024)
#endif

#if USE_SPARSE_MEMORY && !FREE_PHYSICAL_RESOURCES
	#define SIMPLE_PHYSICAL_SIZE	(8192 * 1024)
#elif RSG_PC
	#define SIMPLE_PHYSICAL_SIZE	(256 *1024)
#elif RSG_ORBIS || RSG_DURANGO
	#define SIMPLE_PHYSICAL_SIZE	((2482 * 1024) - SIMPLE_VIRTUAL_SIZE)
#else
	#define SIMPLE_PHYSICAL_SIZE	(0)
#endif

#else // __64BIT
	#define SIMPLE_HEAP_SIZE		(196*1024) 

	#define ONE_HEAP 0
	#if ONE_HEAP
		#define SIMPLE_VIRTUAL_SIZE		0
		#define SIMPLE_PHYSICAL_SIZE	(1024*1024)
	#else
		#define SIMPLE_PHYSICAL_SIZE	(256*1024)
		#define SIMPLE_VIRTUAL_SIZE		(1024*1024)
	#endif // ONE_HEAP
#endif // __64BIT

#elif __PPU   
#include "grcore/wrapper_gcm.h"
#include "grcore/effect_config.h"
#include "renderer/psnvramrendertargettotal.h"

#if SMALL_ALLOCATOR_DEBUG_MIN_ALIGNMENT
# define DEBUG_MEMORY   (6)
#else
# define DEBUG_MEMORY   (4)
#endif

#define HEADER_HEAP_SIZE			(512)

// Game Virtual - The fixed amount of memory reserved for the normal game heap in Kb.
#if __DEV
# if !__OPTIMIZED
#  define GAME_VIRTUAL				(((67+DEBUG_MEMORY)*1024) + 256)		// Debug
# else
#  define GAME_VIRTUAL				(((67+DEBUG_MEMORY)*1024) + 256)		// Beta
# endif
#elif __BANK
# define GAME_VIRTUAL				(((67+DEBUG_MEMORY)*1024) + 256)		// Bank release
#elif !__FINAL
# define GAME_VIRTUAL				((67 * 1024) + 256)					// Release 
#else
# define GAME_VIRTUAL				((62 * 1024) + 512)					// Final (3.5 MB decrease)
#endif 

// Define a realistic upper bound
#define VIRTUAL_HEAP_SIZE ((91 << 10) + 768)

// Audio physical - the fixed amount of VRAM reserved for audio wave assets in Kb
#define AUDIO_PHYSICAL				(14 * 1024 + 512)

// VRAM reserved for render targets.  Specified in Kb.
#define VRAM_GAME_RESERVED          (PSN_VRAM_RENDER_TARGET_TOTAL_KB)

// Game physical - The fixed amount of memory reserved for the physical game heap in Kb.
CompileTimeAssert(SPU_GCM_FIFO);
#define GAME_PHYSICAL				(77952 - VRAM_GAME_RESERVED)

//////////////////////////////////////////////////////////////////////////
// Rage specification.
#define SIMPLE_HEAP_SIZE			(GAME_VIRTUAL)

// Resource physical expressed as VRAM minus what is we need for game physical.
#define SIMPLE_PHYSICAL_SIZE		(VRAM_SIZE - GAME_PHYSICAL - VRAM_GAME_RESERVED - AUDIO_PHYSICAL)

// Gcm buffer size.
# define GCM_BUFFER_SIZE			(3*1024)

// Amount of memory reserved for memory containers. Must be multiple of 1Mb. 
#define EXTERNAL_HEAP_SIZE			(0)

// Amount of memory to reserve for thread stacks, in kilobytes.  Must be multiple of 64k.
#define THREAD_STACK_SIZE			(1472)

#elif __XENON
#define HEADER_HEAP_SIZE			(1024)

#if __BANK
	// If you get crashes allocating backing store textures, you need to make SIMPLE_PHYSICAL_SIZE *smaller*
	#if __DEV // Beta
		#define SIMPLE_HEAP_SIZE		((96 << 10) + 768)
		#define SIMPLE_PHYSICAL_SIZE	((266 << 10) + 512)	// Resource Virtual - Keep checking amount of title free pages in pix
		#define SIMPLE_POOL_SIZE		((7 << 10) + 512)
	#else // Bank Release
		#define SIMPLE_HEAP_SIZE		((96 << 10) + 768) 
		#define SIMPLE_PHYSICAL_SIZE	((266 << 10) + 512)	// bank release
		#define SIMPLE_POOL_SIZE		((7 << 10) + 512)
	#endif //_DEV
#elif __PROFILE || !__FINAL	// Profile, Release
	#define SIMPLE_HEAP_SIZE		((101 << 10) + 512)		// pools are getting allocated out of here too (unlike in the __BANK builds)
	#define SIMPLE_PHYSICAL_SIZE	((266 << 10) + 512)		// keep checking amount of title free pages in pix 
	#define SIMPLE_POOL_SIZE		((0 << 10) + 0)		// release is getting really close on XTL memory remaining.
#else // Final
	#define SIMPLE_HEAP_SIZE		((92 << 10) + 512)		// pools are getting allocated out of here too (unlike in the __BANK builds)	
	#define SIMPLE_PHYSICAL_SIZE	((275 << 10) + 512)		// keep checking amount of title free pages in pix 	
	#define SIMPLE_POOL_SIZE		((6 << 10) + 768)
#endif

// 
// DON'T FUCK WITH THIS UNLESS YOU KNOW WHAT YOU'RE DOING!
// We have optimized the heap sizes to reduce TLB misses. Change this and you could fuck up performance (by as much as 1+ ms/frame)
#define SIMPLE_COMBINED_SIZE	(SIMPLE_HEAP_SIZE + SIMPLE_PHYSICAL_SIZE)
//
#if !__BANK
CompileTimeAssert(SIMPLE_COMBINED_SIZE % (16*1024) == 0);
#endif

#endif 

// ====================== Platform Dependent =========================
// Set the debug heap size, in kilobytes (default is 8M).  Current use appears to be about 4.4M
//#define DEBUG_HEAP_SIZE (((RSG_PC || RSG_DURANGO || RSG_ORBIS) ? 512 : 60) * 1024)
#define DEBUG_HEAP_SIZE (((RSG_PC || RSG_DURANGO || RSG_ORBIS) ? 220 : 60) * 1024)
// ====================== Platform Dependent =========================

#if RSG_DURANGO || RSG_ORBIS || RSG_PC
#define REPLAY_HEAP_SIZE (196 << 20)
#endif

#if RSG_DURANGO
// DON'T FUCK WITH THIS UNLESS YOU KNOW WHAT YOU'RE DOING!
// We have optimized the heap sizes to reduce TLB misses. Change this and you could fuck up performance (by as much as 1+ ms/frame)
CompileTimeAssert(SIMPLE_HEAP_SIZE % (4 << 10) == 0);
CompileTimeAssert(DEBUG_HEAP_SIZE % (4 << 10) == 0);
CompileTimeAssert(REPLAY_HEAP_SIZE % (4 << 20) == 0);
#endif

#if !__NO_OUTPUT
static void EarlyInit();

#define EARLY_INIT_CALLBACK EarlyInit
#endif
 
#define	NO_XMEM_WRAPPERS

#if RSG_PC
#define WANT_PORTCULLIS
#endif

// rage headers
#include "audiohardware/driver.h"
#include "system/main.h"
#include "system/tinyheap.h"
#include "streaming/streaming_channel.h"
#include "streaming/streamingengine.h"
#include "string/string.h"

// game headers
#include "app.h"
#include "file/stream.h"

#if __WIN32PC
#include "grcore/device.h"
#include "system/xtl.h"
#include "dwmapi.h"
#include "diag/diagerrorcodes.h"
#include "fwutil/Gen9Settings.h"
#include "system/SystemInfo.h"
#pragma comment (lib,"Dwmapi.lib")

#endif

#if __XENON
#include "system/xtl.h"
# if _XDK_VER < 21256
//https://devstar.rockstargames.com/wiki/index.php/Project_Setup
# error "GTA5 requires at least XeDK version 21256 or better installed on your PC in order to build correctly."
# endif

#elif RSG_DURANGO
#include <xdk.h>
# if _XDK_VER < 11068	// pretending we only need June until the build machines can be updated (manifest file will still fail)
//http://rsgediwiki1/durango/index.php/Main_Page
# error "Game requires Xbox One XDK version 6.2.11291 (July XDK QFE3) or better installed on your PC in order to build correctly."
# endif
# define RSG_MIN_DURANGO_SYS    (_XDK_VER)

#elif __PPU
# if CELL_SDK_VERSION < 0x430001
//https://devstar.rockstargames.com/wiki/index.php/Project_Setup
# error "GTA5 requires at least PS3 SDK version 430.001 or better installed on your PC in order to build correctly."
# endif
namespace rage {  
#if __GERMAN_BUILD
char* XEX_TITLE_ID = "BLES01807";
char* XEX_TITLE_ID_HDD = "NPEB01283";
#elif __AUSSIE_BUILD
char* XEX_TITLE_ID = "BLES01807";	// fake; necessary for savegame and disk cache code.
char* XEX_TITLE_ID_HDD = "NPEB01283";
#elif __EUROPEAN_BUILD
char* XEX_TITLE_ID = "BLES01807";	// fake; necessary for savegame and disk cache code.
char* XEX_TITLE_ID_HDD = "NPEB01283";
#elif __AMERICAN_BUILD
char* XEX_TITLE_ID = "BLUS31156";	// fake; necessary for savegame and disk cache code.
char* XEX_TITLE_ID_HDD = "NPUB31154";
#elif __JAPANESE_BUILD
char* XEX_TITLE_ID = "BLJM61019";	// fake; necessary for savegame and disk cache code.
char* XEX_TITLE_ID_HDD = "NPEB01283"; // TODO: don't have this yet
#else
#error "Build needs either __<region>_BUILD set to 1"
#endif

// this is incompatible with unity builds so I just modified streambuffers.cpp to have these numbers in the first place.
// override the number of stream buffers used by the application (the default can be found in streambuffers.cpp in the RageCore library
// in the file directory. I have increased this limit for console builds due to the number of network log files generated by the network game
//#if IS_CONSOLE
//DECLARE_STREAM_BUFFERS(16)
//#else
//DECLARE_STREAM_BUFFERS(64)
//#endif
}
#elif RSG_ORBIS
	// sdk 1.7 is minimum - note that gta5 requires a special patch (1.700.601) to be applied to 1.700.081 - 1.700.141
	#define RSG_MIN_ORBIS_SDK 0x01700601u	// minimum sdk required for compiling
	#define RSG_MIN_ORBIS_SYS 0x01750061u	// lowest supported firmware version
	#if SCE_ORBIS_SDK_VERSION < RSG_MIN_ORBIS_SDK
	#error "Game requires PS4 SDK version 1.700.601 or better installed on your PC in order to build correctly."
	#endif
namespace rage {  
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-writable-strings"
char *XEX_TITLE_ID = "CUSA00411";	// default to european, gets overridden later
#pragma GCC diagnostic pop
}
#endif	//__XENON
#include "system\performancetimer.h"

#if __PPU && !__FINAL
#include "system/stack.h"

extern "C" {

extern void __real_exit (int code);

void __wrap_exit (int UNUSED_PARAM(code))
{
  sysStack::PrintStackTrace();
  
  Assertf(false,"Calling Exit: This might or might not be a problem");
  
  __debugbreak();
}

};
#endif // __PPU && !__FINAL

NOSTRIP_FINAL_LOGGING_PARAM(output,"[startup] Enable all output even in !__DEV builds");
NOSTRIP_FINAL_LOGGING_PARAM(nooutput, "[startup] Disable all output even in __DEV builds");
PARAM(noaero, "[startup] Disable Windows' Aero theme");
PARAM(nokeyboardhook, "[startup] Don't disable the Windows key with a low-level keyboard hook.");
PARAM(forcekeyboardhook, "[startup] Force disabling the Windows key with a low-level keyboard hook, even with debugger attached.");

#if __WIN32PC
static HHOOK g_keyboardHook;
static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
		// Check for pressing / releasing either Windows key, and having focus
		KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
		if ((wParam == WM_KEYUP || wParam == WM_KEYDOWN) && ((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)) && GRCDEVICE.GetHasFocus())
		{
			return 1; // Eat the keystroke
		}
	}
	return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

static void RemoveKeyboardHook()
{
	UnhookWindowsHookEx(g_keyboardHook);
}

static void AddKeyboardHook()
{
	g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

	atexit(RemoveKeyboardHook);
}

#endif


CApp g_myApp;

namespace rage
{
	XPARAM(nomtplacement);
#	if RAGE_TRACKING
		XPARAM(memvisualize);
#	endif
#	if STREAMING_VISUALIZE
		XPARAM(streamingvisualize);
#	endif
#	if RSG_DURANGO || RSG_ORBIS
		XPARAM(forceboothdd);
		XPARAM(forceboothddloose);
#	endif
}
XPARAM(playgoemu);

int Main()
{
	// Should never reach here.
	Assertf(false, "GTA V uses App style entry points.\n");
	return 0;
}

bool Main_Prologue()
{
#if !__FINAL
#if RSG_DURANGO
	Displayf("XB1 XDK version:    %u", _XDK_VER);
	OSVERSIONINFOEXW osVersionInfoEx;
	osVersionInfoEx.dwOSVersionInfoSize = sizeof(osVersionInfoEx);
	if(!GetVersionExW((OSVERSIONINFOW*)&osVersionInfoEx))
		Quitf("Failed to get os version info, 0x%08x",GetLastError());
	Displayf("XB1 system version: %u",osVersionInfoEx.dwBuildNumber);
	if(osVersionInfoEx.dwBuildNumber<RSG_MIN_DURANGO_SYS)
		Quitf("Unsupported system software version [%u] - must be at least %u",osVersionInfoEx.dwBuildNumber,RSG_MIN_DURANGO_SYS);
#elif RSG_ORBIS
	Displayf("PS4 SDK version:     %x.%03x.%03x",(SCE_ORBIS_SDK_VERSION&0xff000000u)>>24,(SCE_ORBIS_SDK_VERSION&0xfff000u)>>12,SCE_ORBIS_SDK_VERSION&0xfffu);
#if SCE_ORBIS_SDK_VERSION >= 0x01600071u
	// Call IsDebuggerPresent() to ensure IsDevkit is initialised
	sysBootManager::IsDebuggerPresent();
	if(sysBootManager::IsDevkit())
	{
		char sysversion[SCE_SYSTEM_SOFTWARE_VERSION_LEN+1];
		u32 hexver = 0;
		sceDbgGetSystemSwVersion(sysversion,SCE_SYSTEM_SOFTWARE_VERSION_LEN+1);
		for(int i=SCE_SYSTEM_SOFTWARE_VERSION_LEN,j=0;i>=0;i--)
			if(sysversion[i]>='0' && sysversion[i]<='9')
				hexver+=(sysversion[i]-'0')*pow(16,j++);
		Displayf("PS4 system version: %s (minimum supported version: %x.%03x.%03x)",sysversion,(RSG_MIN_ORBIS_SYS&0xff000000u)>>24,(RSG_MIN_ORBIS_SYS&0xfff000u)>>12,RSG_MIN_ORBIS_SYS&0xfffu);
		if(hexver<RSG_MIN_ORBIS_SYS && hexver!=0x01730000u)	// also allow special version for gta5 that may still be in use (but shouldn't be)
			Quitf("Unsupported system software version [%s] - must be at least %x.%03x.%03x",sysversion,(RSG_MIN_ORBIS_SYS&0xff000000u)>>24,(RSG_MIN_ORBIS_SYS&0xfff000u)>>12,RSG_MIN_ORBIS_SYS&0xfffu);
	}
#endif
#endif
#endif	// !__FINAL

	// GTAV doesn't want output on in release builds.
#if !__NO_OUTPUT
	if ((!__DEV && !__FINAL && !PARAM_output.Get()) ||
		(__DEV && PARAM_nooutput.Get())
	#if __FINAL_LOGGING
		|| (__FINAL && PARAM_nooutput.Get())
	#endif	// __FINAL_LOGGING
		)
	{
		Displayf("*** Output turned OFF, removing all output to popups, logs and tty.  Use -output to restore normal display.");
#if __XENON
		Displayf("*** Press LB+RB+LS+RS to toggle output on and off.");
#elif __PS3 || RSG_ORBIS
		Displayf("*** Press L1+R1+L3+R3 to toggle output on and off.");
#endif
		diagChannel::SetOutput(false);
	}
#endif // !__NO_OUTPUT


#if SYSTMCMD_ENABLE
	if (PARAM_rockstartargetmanager.Get())
	{
		static const char *const configFmt =
			"<config>"
#				if RSG_DURANGO
					"<pdb>%s</pdb>"
					"<fullpackagename>%s</fullpackagename>"
#					if GTA_VERSION/100 == 5
						"<symbol_server>N:\\RSGEDI\\Symbol\\Symbol_Server</symbol_server>"
#					endif
#				endif
			"</config>";
#		if !RSG_DURANGO
			const char *const config = configFmt;
#		else
			char config[512];
			snprintf(config, sizeof(config), configFmt, sysTmGetPdb(), sysTmGetFullPackageName());
			config[sizeof(config)-1] = '\0';
#		endif
		sysTmCmdConfig(config);

#		if RSG_ORBIS
			// Determine whether or not we want to have the submitDone exception
			// enabled.  Same logic applies when not using R*TM, but we don't
			// have any other way to change (or even read) the setting, so
			// withou R*TM it is up to the user to get it right.
			bool submitDoneException = __BANK ? false : true;
			if (!submitDoneException)
				Displayf("Requesting R*TM disable the submitDone exception due to build type (" RSG_CONFIGURATION_LOWERCASE ").");
#			if RAGE_TRACKING
				if (PARAM_memvisualize.Get()) {
					submitDoneException = false;
					Displayf("Requesting R*TM disable the submitDone exception due to mem visualize.");
				}
#			endif
#			if STREAMING_VISUALIZE
				if (PARAM_streamingvisualize.Get()) {
					submitDoneException = false;
					Displayf("Requesting R*TM disable the submitDone exception due to streaming visualize.");
				}
#			endif
			if (!PARAM_forceboothdd.Get() && !PARAM_forceboothddloose.Get() && !PARAM_playgoemu.Get()) {
				submitDoneException = false;
				Displayf("Requesting R*TM disable the submitDone exception due to not booting from hdd.");
			}
			if (submitDoneException)
				Displayf("Requesting R*TM enable the submitDone exception.");
			sysTmCmdSubmitDoneExceptionEnabled(submitDoneException);
#		endif
	}
#endif

#if __WIN32PC
	if (!PARAM_output.Get() ||
		PARAM_nooutput.Get())
	{
		HWND handle = GetConsoleWindow();
		if (handle)
			ShowWindow(handle, SW_HIDE);
	}

#if !__FINAL && __64BIT
	if(!PARAM_nokeyboardhook.Get() && (!sysBootManager::IsDebuggerPresent() || PARAM_forcekeyboardhook.Get()))
#else
	if (!PARAM_nokeyboardhook.Get())
#endif
	{
		AddKeyboardHook();
	}

#if !__D3D11_1
	if (PARAM_noaero.Get())
	{
		// Note: AH: Turn off Desktop Window Manager this disables compositing of the Windows screen in
		// Win7 and can help performance in windowed mode if lots of other desktop rendering is happening 
		// simultaneously (using the Aero Win7 theme, for example).
		// Most helpful for profiling and probably mostly affects the older graphics cards.
		// See http://discussms.hosting.lsoft.com/SCRIPTS/WA-MSD.EXE?A2=DIRECTXDEV;e61eaeea.1105b for more details
		// and slide 5 http://download.microsoft.com/download/b/8/c/b8cdd181-478c-4341-8d55-878438dc222f/Performance%20Considerations%20for%20Graphics%20on%20Windows.zip
		BOOL dwmEnabled;

		HRESULT hr = DwmIsCompositionEnabled( &dwmEnabled );
		if( dwmEnabled )
		{
			hr = DwmEnableComposition( DWM_EC_DISABLECOMPOSITION );
			Assert( hr == S_OK );
		}
	}
#endif // __D3D11_1 // Depreacated

#endif

	// Rage turns this on by default, we want it off for the game.
	sysMemAllowResourceAlloc = false;

#if !__DEV && !__FINAL
	XPARAM(output);
	if (!PARAM_output.Get())
		PARAM_output.Set("0");
#endif

#if __ASSERT && (__XENON || __PS3)
	const char *literal_string_constant = "Literal String Constant";
	static char data_test[] = "Not a String Constant";
	static char bss_test[16];
	char stack_test[16] = "";
	char *heap_test = rage_new char[16];
	heap_test[0] = 0;
	Assertf(IsConstString(literal_string_constant),"IsConstString(%p) is broken on this platform, will waste memory (_rodata_end is %p).",literal_string_constant,_rodata_end);
	AssertMsg(!IsConstString(data_test),"IsConstString() is broken on this platform (data), must be fixed.");
	AssertMsg(!IsConstString(bss_test),"IsConstString() is broken on this platform (bss), must be fixed.");
	AssertMsg(!IsConstString(stack_test),"IsConstString() is broken on this platform (stack), must be fixed.");
	AssertMsg(!IsConstString(heap_test),"IsConstString() is broken on this platform (heap), must be fixed.");
	delete [] heap_test;
#endif

#if __WIN32PC && !__TOOL && !__RESOURCECOMPILER
	diagErrorCodes::LoadLanguageFile();
	CSystemInfo::Initialize();
#endif

	return true;
}

bool Main_OneLoopIteration()
{
	PROFILER_FRAME("MainThread");

#if !__NO_OUTPUT
	// We want a way to crash the game even when running with no target manager
	// (eg. console set to release mode), so we can get what ever coredump the
	// system OS supports.  While this seems like an odd spot to put it, we do
	// want it only in game code (not samples), but should be all games (so not
	// part fof App::RunOneIteration()).
#if RSG_PC
	if (ioKeyboard::KeyDown(KEY_CONTROL) && ioKeyboard::KeyDown(KEY_SHIFT) && ioKeyboard::KeyDown(KEY_DELETE))
	{
		Displayf("The player has pressed Ctrl+Shift+Delete so call __debugbreak()");
#else
	if (ioKeyboard::KeyDown(KEY_CONTROL) && ioKeyboard::KeyDown(KEY_ALT) && ioKeyboard::KeyDown(KEY_DELETE))
	{
		Displayf("The player has pressed Ctrl+Alt+Delete so call __debugbreak()");
#endif
		__debugbreak();
	}
#endif

#if BACKTRACE_ENABLED
	BACKTRACE_TEST_CRASH;
#endif

	return g_myApp.RunOneIteration();
}


void Main_Epilogue()
{

}

SET_APP_ENTRY_POINTS(Main_Prologue, Main_OneLoopIteration, Main_Epilogue);

/*
extern void rage::SetProjectEntryPoints(bool (*pProjectPrologue)(void), bool (*pProjectDoOneLoop)(void), void (*pProjectEpilogue)(void));

// Called during global construtors to set the entry points.
static struct InitGameEntryPoints_t 
{
	InitGameEntryPoints_t()
	{ 
		rage::SetProjectEntryPoints(Main_Prologue, Main_OneLoopIteration, Main_Epilogue);
	}
	~InitGameEntryPoints_t()
	{
	}
} InitGameEntryPointsInstance;
*/

#if __XENON

#include "system/XMemWrapper.h"

void* WINAPI XMemAlloc(unsigned long dwSize, unsigned long  dwAllocAttributes)
{
	return XMemAllocCustom(dwSize, dwAllocAttributes);
}

void WINAPI XMemFree(void *pAddress, unsigned long dwAllocAttributes)
{
	return XMemFreeCustom(pAddress, dwAllocAttributes);
}

SIZE_T WINAPI XMemSize (void *pAddress, unsigned long dwAllocAttributes)
{
	return XMemSizeCustom(pAddress, dwAllocAttributes);
}

#endif // __XENON

#if !__NO_OUTPUT
#include "diag/channel.h"
#include "fwsys/timer.h"

static unsigned GetGameFrame()
{
	return fwTimer::GetFrameCount(); 
}

static void EarlyInit()
{
	rage::diagGetGameFrame = &GetGameFrame;
}
#endif

#include "system/exception.h"

#if BACKTRACE_ENABLED

#if RSG_DURANGO
#include "system/BacktraceDumpUpload.winrt.h"
#endif // RSG_DURANGO

class GameBacktraceConfig : public sysException::BacktraceConfig
{
public:
	GameBacktraceConfig(const char* token, const char* submissionUrl) : BacktraceConfig(token, submissionUrl) {}

	virtual void OnSetup() {}
	virtual bool ShouldSubmitDump(const wchar_t* logFilename);
	virtual bool UploadDump(const wchar_t* dumpFilename, const wchar_t* logFilename, const atMap<atString, atString>* annotations, const char* url);
};

#include "Network/Cloud/Tunables.h"

static void OnBacktraceEnabledTunableSet(bool enabled)
{
	Displayf("[Backtrace] Tunable just set: backtrace enabled: %s", enabled ? "true" : "false");

	// Disable if the kill-switch is set.  We don't re-enable Backtrace later if the kill switch changes.
	if (!enabled)
	{
		sysException::DisableBacktrace();
	}
}

bool GameBacktraceConfig::ShouldSubmitDump(const wchar_t* path)
{
	CApp::WriteCrashContextLog(path);
	CApp::CollectAdditionalAttributes();

	// Default values
	bool enabled = true;
	float rateLimit = 100.0f;

	// Try to override with tunable values
	if (Tunables::IsInstantiated())
	{
		CTunables* tunables = &Tunables::GetInstance();

		// Tunable to disable Backtrace.  We shouldn't get here if this is set, due to the callback function (above),
		// but check anyway.
		if (tunables->Access(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_BACKTRACE", 0xB3627BBC), enabled))
		{
			Displayf("[Backtrace] Tunable set: Backtrace enabled: %s", enabled ? "true" : "false");
			OnBacktraceEnabledTunableSet(enabled);
		}
		else
		{
			Displayf("[Backtrace] Tunable not set: Backtrace enabled");
		}

		// Tunable to set rate limit
		if (tunables->Access(CD_GLOBAL_HASH, ATSTRINGHASH("BACKTRACE_RATE_LIMIT", 0x136ADE12), rateLimit))
		{
			Displayf("[Backtrace] Tunable set: Backtrace rate limit: %f", rateLimit);
		}
		else
		{
			Displayf("[Backtrace] Tunable not set: Backtrace rate limit");
		}
	}
	else
	{
		Displayf("[Backtrace] Tunables not initialized yet");
	}

	// Early-out if disabled
	if (!enabled)
	{
		Displayf("[Backtrace] Upload to Backtrace is disabled");
		return false;
	}

	// Seed random number generator, discarding first 100 values as distribution is poor
	mthRandom rnd(sysTimer::GetSystemMsTime());
	for (int i = 0; i < 100; i++)
		rnd.GetInt();

	// Test rate limit against RNG
	if (rnd.GetFloat() * 100.0f < rateLimit)
	{
		Displayf("[Backtrace] Upload to Backtrace passed rate limit");
		return true;
	}
	else
	{
		Displayf("[Backtrace] Upload to Backtrace failed rate limit");
		return false;
	}
}

// Upload the dump in a callback function, both to avoid RageCode depending on RageNet, but also because this has to be done in a
// WinRT-enabled file.
bool GameBacktraceConfig::UploadDump(const wchar_t* dumpPath, const wchar_t* logPath, const atMap<atString, atString>* annotations, const char* url)
{
#if RSG_DURANGO
	return BacktraceDumpUpload::UploadDump(dumpPath, logPath, annotations, url);
#else
	(void)dumpPath;
	(void)logPath;
	(void)annotations;
	(void)url;
	return false;
#endif
}

// This sets the game Backtrace token and upload URL in a static initializer, as they need to be available before RAGE-level code runs
// Tokens from: https://backtrace.rockstargames.com/p/RDR2/settings/submission/tokens or https://bob.sp.backtrace.io/p/sandbox/settings/submission/tokens

// Internal instance URL:                                            https://backtrace.rockstargames.com:6098
// External Bob instance URL (with Bob sandbox token):               https://submit.backtrace.io/bob/9e4851f293570690ac5efea82e69aecb6629ab6bc125ffcdac230d4dfb2282db/minidump
// External Bob instance URL (with Bob RDR2 token):                  https://submit.backtrace.io/bob/f6cc6561434e06ca34f129f5f6d9f0d8ae046864024e708b22b89b5b89ddc39a/minidump
// External Paradise instance URL (with Paradise sandbox token):     https://submit.backtrace.io/paradise/aef040908377c11bd4d8499790e8d227b57198354ef85fc01e747f7821f5ae7e/minidump
// External Paradise instance URL (with Paradise GTAV token):        https://submit.backtrace.io/paradise/b7468900ead8ac96224f5ed076abc812690d9a690eb5446d5b880a92343e85df/minidump
// External Paradise instance URL (with Paradise GTAV-XBS token):    https://submit.backtrace.io/paradise/c33e1d24da2985b5bc33ab957b08f5e5a75b9c6a08336f39f47ff9ba31f356c0/minidump
// External Paradise instance URL (with Paradise GTAV-XB1 token):    https://submit.backtrace.io/paradise/26221444628213a334170087e91e55aa5daf9219e3285be6844674d9f8e47115/minidump

// Internal RDR2 token:                     a055a1848074e274b78b14c5a3f6fbff729250a740cc7a4567d2c96662208aae
// Internal TestProject token:              cedc6a8c203ed626c149b02e32d04b69674c90304c6eb881896e37fe72554de5
// Internal TestProject2 token:             34f9a6e1b31b0dbd84afb3a4ad00a52a4c9d1c143887e42b19ab9b5a0e20ec0d
// External Bob sandbox token:              9e4851f293570690ac5efea82e69aecb6629ab6bc125ffcdac230d4dfb2282db
// External Bob RDR2 token:                 f6cc6561434e06ca34f129f5f6d9f0d8ae046864024e708b22b89b5b89ddc39a
// External Paradise sandbox token:         aef040908377c11bd4d8499790e8d227b57198354ef85fc01e747f7821f5ae7e
// External Paradise GTAV token:            b7468900ead8ac96224f5ed076abc812690d9a690eb5446d5b880a92343e85df
// External Paradise GTAV-XBS master token: c33e1d24da2985b5bc33ab957b08f5e5a75b9c6a08336f39f47ff9ba31f356c0
// External Paradise GTAV-XBS final token:  ec8379f5f46beab0f241cb6cbd6a0f83bd3f0e6534defa9102c1b1583eadd826
// External Paradise GTAV-XB1 master token: 26221444628213a334170087e91e55aa5daf9219e3285be6844674d9f8e47115
// External Paradise GTAV-XB1 final token:  eabd42626dd15b6e5dc704e8b1a8755475054a57e755567057b98189962bdc3e

#if RSG_SCARLETT
#	if __MASTER
#		define BACKTRACE_TOKEN "c33e1d24da2985b5bc33ab957b08f5e5a75b9c6a08336f39f47ff9ba31f356c0"
#		define BACKTRACE_SUBMISSION_URL "https://submit.backtrace.io/paradise/" BACKTRACE_TOKEN "/minidump"
#	else
#		define BACKTRACE_TOKEN "ec8379f5f46beab0f241cb6cbd6a0f83bd3f0e6534defa9102c1b1583eadd826"
#		define BACKTRACE_SUBMISSION_URL "https://submit.backtrace.io/paradise/" BACKTRACE_TOKEN "/minidump"
#	endif // __MASTER
#elif RSG_DURANGO
#	if __MASTER
#		define BACKTRACE_TOKEN "26221444628213a334170087e91e55aa5daf9219e3285be6844674d9f8e47115"
#		define BACKTRACE_SUBMISSION_URL "https://submit.backtrace.io/paradise/" BACKTRACE_TOKEN "/minidump"
#	else
#		define BACKTRACE_TOKEN "eabd42626dd15b6e5dc704e8b1a8755475054a57e755567057b98189962bdc3e"
#		define BACKTRACE_SUBMISSION_URL "https://submit.backtrace.io/paradise/" BACKTRACE_TOKEN "/minidump"
#	endif // __MASTER
#elif RSG_PC
#		define BACKTRACE_TOKEN "b7468900ead8ac96224f5ed076abc812690d9a690eb5446d5b880a92343e85df"
#		define BACKTRACE_SUBMISSION_URL "https://submit.backtrace.io/paradise/" BACKTRACE_TOKEN "/minidump"
#else
#		define BACKTRACE_TOKEN "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef"
#		define BACKTRACE_SUBMISSION_URL ""
#endif // RSG_SCARLETT

static GameBacktraceConfig s_backtraceTokenSetter(
	BACKTRACE_TOKEN,
	BACKTRACE_SUBMISSION_URL
	);

#endif // BACKTRACE_ENABLED