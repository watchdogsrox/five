#ifndef __OVERVIEW_SCREEN_H
#define __OVERVIEW_SCREEN_H

#if __BANK

#include "atl/array.h"

#include "fwmaths/rect.h"

#include "streaming/streamingdebuggraph.h"

#include "system/AutoGPUCapture.h"

//----------------------------------------------------

#define GET_TEXT_SCREEN_WIDTH() (grcViewport::GetDefaultScreen()->GetWidth())
#define GET_TEXT_SCREEN_HEIGHT() (grcViewport::GetDefaultScreen()->GetHeight())
#define GET_TEXT_SCREEN_COORDS(x,y) Vector2( (x / GET_TEXT_SCREEN_WIDTH()), (y / GET_TEXT_SCREEN_HEIGHT()) )
#define ADJUSTED_DEBUG_CHAR_WIDTH()  (COverviewScreenManager::GetCurFontScale()*DEBUG_CHAR_WIDTH)
#define ADJUSTED_DEBUG_CHAR_HEIGHT() (COverviewScreenManager::GetCurFontScale()*DEBUG_CHAR_HEIGHT)

#define NUM_PERCENTILES 8
#define NUM_SAMPLES 100
#define NUM_PERCENTILE_SAMPLES 10

//----------------------------------------------------

class COverviewScreenBase
{
	friend class COverviewScreenManager;
public:
	COverviewScreenBase() : dump(false) {}
	virtual ~COverviewScreenBase() {}

	virtual void Update() {};
	virtual void Render(fwRect &rect, bool invisible) = 0;

	virtual const char *GetScreenTitle() = 0;

	void DumpData() { dump = true; }

protected:
	bool dump;
};

//----------------------------------------------------

class COverviewScreenFPS : public COverviewScreenBase
{
public:

	enum
	{
		BOUND_BY_UPDATE = 0,
		BOUND_BY_RENDER,
		BOUND_BY_GPU,
		BOUND_BY_GPU_FLIP,
		BOUND_BY_GPU_DRAWABLE_SPU,
		BOUND_BY_GPU_BUBBLE,

		BOUND_BY_COUNT
	};

	COverviewScreenFPS();
	virtual ~COverviewScreenFPS();

	void Update();
	void Render(fwRect &rect, bool invisible);

	const char *GetScreenTitle()  { return "FPS"; }

private:

	void UpdateFPSData();
	void RenderBoundInfo(fwRect &rect);
	void RenderBoundHistogram(fwRect &rect);
	void RenderFPSHistogram(fwRect &rect);
	void RenderTimebarBuckets(fwRect &rect);

	// Constants for FPS and bound calcs
	static float m_fLastFPS;
	static int m_LastTimeStep;
	static Color32 m_FpsColor;
	static float m_fWaitForMain;
	static float m_fWaitForRender;
	static float m_fWaitForGPU;
	static float m_fEndDraw;
	static float m_fWaitForDrawSpu;
	static float m_fFifoStallTimeSpu;

	// Percentiles
	static const unsigned int m_Percentiles[NUM_PERCENTILES];
	atArray<unsigned int> m_TimeStepHistory;
	MetricsCapture::SampledValue<float> m_PercentileSamples[NUM_PERCENTILES];
	unsigned int m_LastPercentile;

	// Histogram of bound
	unsigned int m_LastBound;
	float m_fLastBoundTime;
	int m_BoundHist[BOUND_BY_COUNT];

	// Bound names
	static const char *m_pBoundStrings[BOUND_BY_COUNT];	
};

//----------------------------------------------------

class COverviewScreenMemory : public COverviewScreenBase
{
public:

	COverviewScreenMemory();
	virtual ~COverviewScreenMemory();

	void Update();
	void Render(fwRect &rect, bool invisible);

	const char *GetScreenTitle()  { return "Memory"; }

private:

	void RenderMemoryBuckets(fwRect &rect);
	void RenderResourceHeapPages(fwRect &rect);
	void RenderMemoryUsedFree(fwRect &rect);
	void FormatMemoryUsedFree(char* pText, const char* pszTitle, sysMemAllocator* pAllocator);
	void FormatMemoryUsedFree(char* pText, const char* pszTitle, sysMemPoolAllocator* pAllocator);
	void FormatMemoryUsedFree(char* pText, const char* pszTitle, size_t usedBytes, size_t freeBytes, size_t totalBytes);
	void FormatMemoryUsedFree(char* pText, const char* pszTitle, size_t usedBytes);
	void RenderStreamingModuleMemory(fwRect &rect);
	void RenderStreamingBandwidth(fwRect &rect);

	struct ModuleMemoryResults
	{
		ModuleMemoryResults()
		{
			loadedMemPhys = 0;
			loadedMemVirt = 0;
			requiredMemPhys = 0;
			requiredMemVirt = 0;
		}

		int loadedMemPhys;
		int loadedMemVirt;
		int requiredMemPhys;
		int requiredMemVirt;
	};
	static int m_currentModuleToUpdate;
	ModuleMemoryResults	m_ModuleMemResults[CStreamGraphBuffer::MAX_TRACKED_MODULES];

};

//----------------------------------------------------

class COverviewScreenVehicles : public COverviewScreenBase
{
public:

	COverviewScreenVehicles();
	virtual ~COverviewScreenVehicles();

	void Update();
	void Render(fwRect &rect, bool invisible);

	const char *GetScreenTitle()  { return "Vehicles"; }

private:

	void RenderVehicleDebugInfo(fwRect &rect, bool invisible);

	void CountVehicles(u32& mission, u32& parked, u32& parkedMission, u32& other);
};

//----------------------------------------------------

class COverviewScreenPeds : public COverviewScreenBase 
{
public:

	COverviewScreenPeds();
	virtual ~COverviewScreenPeds();

	void Update();
	void Render(fwRect &rect, bool invisible);

	const char *GetScreenTitle()  { return "Peds"; }

private:

	void RenderPedDebugInfo(fwRect &rect, bool invisible);

	void CountPeds(u32& mission, u32& parked, u32& parkedMission, u32& other);

};
//----------------------------------------------------

class COverviewScreenDrawCalls : public COverviewScreenBase
{
public:
	COverviewScreenDrawCalls() : COverviewScreenBase() {}
	~COverviewScreenDrawCalls() {}

	//void Update();
	void Render(fwRect &rect, bool invisible);

	const char *GetScreenTitle()  { return "Draw Calls"; }

private:

};

//----------------------------------------------------

class COverviewScreenCmdLineArgs : public COverviewScreenBase
{
public:
	COverviewScreenCmdLineArgs() : COverviewScreenBase() {}
	~COverviewScreenCmdLineArgs() {}

	//void Update();
	void Render(fwRect &rect, bool invisible);

	const char *GetScreenTitle()  { return "Command Line Arguments"; }

private:

};

//----------------------------------------------------

class COverviewScreenKeyboard : public COverviewScreenBase
{
public:
	COverviewScreenKeyboard() : COverviewScreenBase() { bDisplayCtrlKey = bDisplayShiftKey = bDisplayAltKey = true; }
	~COverviewScreenKeyboard() {}

	//void Update();
	void Render(fwRect &rect, bool invisible);

	const char *GetScreenTitle()  { return "Keyboard"; }

	bool bDisplayCtrlKey;
	bool bDisplayShiftKey;
	bool bDisplayAltKey;

private:

	bool FilterKeyDept(int key, int keyboardMode);
};

//----------------------------------------------------

//NOTE: not actually used as yet, but good to know which is which
enum
{
	OVERVIEW_SCREEN_NONE = 0,
	OVERVIEW_SCREEN_FPS,
	OVERVIEW_SCREEN_MEMORY,
	OVERVIEW_SCREEN_VEHICLES,
	OVERVIEW_SCREEN_PEDS,
	OVERVIEW_SCREEN_DRAWCALLS,
	OVERVIEW_SCREEN_KEYBOARD,

	OVERVIEW_SCREEN_COUNT
};

enum
{
	OVERVIEW_FORMAT_BYTE = 0,
	OVERVIEW_FORMAT_KILOBYTE,
	OVERVIEW_FORMAT_MEGABYTE,

	OVERVIEW_FORMAT_COUNT
};

class COverviewScreenManager
{
public:
	static void Init();
	static void Shutdown();

	static void Update();
	static void Render();

	static void DumpPedAndVehicleData();
	static void DumpVehicleData();
	static void DumpPedData();

	static float GetCurFontScale()	{ return sm_fontscale; }
	static int GetDisplayFormat()	{ return m_OverviewScreenFormat; }

	static size_t ConvertToDisplayFormat(size_t val);
	static void DrawToggleDisplay(fwRect &rect);

	static void		AppendToDebugInfoStr(const char* debugInfo)	{ sm_DebugInfoStr += debugInfo; }
	static atString	GetDebugInfoStr()							{ return sm_DebugInfoStr; }
	static bool		DumpFile()									{ return sm_DumpFile; }
	static bool		ResetFPS()									{ return sm_ResetFPS; }
	static bool		ShowDupe()									{ return sm_ShowDupe; }
	static bool		ShowCompPeds()								{ return sm_ShowComponentPeds; }
	static bool		ShowStreamPeds()							{ return sm_ShowStreamPeds; }
	static bool		OrderByMemoryUse()							{ return sm_OrderByMemoryUse; }
	static bool		OrderByName()								{ return sm_OrderByName; }
	static void		SetResetFpsFlag(bool flag)					{ sm_ResetFPS = flag; }
	static void		WriteToFile(const char* path);

	static void InitWidgets();
	static void ResetFpsCB();
	static void SaveToLogMemoryCB();
	static void SaveToLogVehiclesCB();
	static void SaveToLogPedsCB();

	static char sm_LogFileNameMemory[RAGE_MAX_PATH];
	static char sm_LogFileNameVehicles[RAGE_MAX_PATH];
	static char sm_LogFileNamePeds[RAGE_MAX_PATH];
	static char sm_VehicleFilterStr[32];
	static char sm_VehicleDepsFilterStr[32];
	static char sm_PedFilterStr[32];
	static char sm_PedDepsFilterStr[32];
	static char sm_KeyboardDeptFilterStr[32];

private:

	static void DrawBackground(fwRect &rect);
	static void DrawTitle(fwRect &rect, COverviewScreenBase *pScreen);

	static int	m_OverviewScreenID;
	static int	m_OverviewScreenFormat;
	static atArray<COverviewScreenBase*> m_pOverViewScreens;

	// Used to prevent update and render going pear shaped
	static rage::sysIpcMutex sm_Mutex;

    static float sm_xScroll;
	static float sm_yScroll;

	static float sm_fontscale;
	static atString sm_DebugInfoStr;

	static bool sm_DumpFile;
	static bool sm_ResetFPS;
	static bool sm_ShowDupe;
	static bool sm_ShowStreamPeds;
	static bool sm_ShowComponentPeds;
	static bool sm_OrderByMemoryUse;
	static bool sm_OrderByName;
};

//----------------------------------------------------

#endif	//__BANK


#endif	//__OVERVIEW_SCREEN_H

