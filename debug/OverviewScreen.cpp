#if __BANK

#include "OverviewScreen.h"
#include "fwsys/gameskeleton.h"
#include "grprofile/timebars.h"
#include "profile/element.h"
#include "text/TextConversion.h"

#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "streaming/populationstreaming.h"

#include "modelinfo/VehicleModelInfoVariation.h"

//#include "peds/Population.h"
#include "peds/PopCycle.h"
#include "Peds/Ped.h"
#include "Peds/pedpopulation.h"
#include "Peds/rendering/PedVariationStream.h"

#include "vehicles/vehiclepopulation.h"

#include "scene/world/GameWorld.h"
#include "string/stringutil.h"
#include "system/controlMgr.h"
#include "system/simpleallocator.h"
#include "system/memmanager.h"
#include "network/network.h"

#include "performance/clearinghouse.h"

#if RSG_ORBIS
#include "grcore/wrapper_gnm.h"
#include "system/memory.h"
#endif // RSG_ORBIS

// DON'T COMMIT!
//OPTIMISATIONS_OFF()

#define GET_SCREEN_COORDS(x,y) GET_TEXT_SCREEN_COORDS(x,y)

#define PERCENTILE_BAR_MIN 30.0f
#define PERCENTILE_BAR_MAX 66.667f

//----------------------------------------------------
//	OVERVIEW SCREEN FPS
//----------------------------------------------------

const char *COverviewScreenFPS::m_pBoundStrings[COverviewScreenFPS::BOUND_BY_COUNT] = 
{
	"Update", "Render", "GPU", "GPU Flip", "GPU DrawableSpu", "GPU Bubble"
};

const unsigned int COverviewScreenFPS::m_Percentiles[NUM_PERCENTILES] = 
{
	50, 66, 75, 80, 90, 95, 98, 99
};

float COverviewScreenFPS::m_fLastFPS = 30.0f;
int COverviewScreenFPS::m_LastTimeStep = 33;
Color32 COverviewScreenFPS::m_FpsColor = Color_white;
float COverviewScreenFPS::m_fWaitForMain = 0.0f;
float COverviewScreenFPS::m_fWaitForRender = 0.0f;
float COverviewScreenFPS::m_fWaitForGPU = 0.0f;
float COverviewScreenFPS::m_fEndDraw = 0.0f;
float COverviewScreenFPS::m_fWaitForDrawSpu = 0.0f;
float COverviewScreenFPS::m_fFifoStallTimeSpu = 0.0f;

COverviewScreenFPS::COverviewScreenFPS() : COverviewScreenBase()
{
	m_TimeStepHistory.Reserve(NUM_SAMPLES);
	m_LastPercentile = 0;
}

COverviewScreenFPS::~COverviewScreenFPS()
{
}

int IntegerSort(const u32 *a, const u32 *b)
{
	if(*a < *b)
	{
		return -1;
	}
	else if(*a > *b)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void COverviewScreenFPS::UpdateFPSData()
{
	m_LastTimeStep = fwTimer::GetSystemTimeStepInMilliseconds();

	float leastDifference = FLT_MAX;

	for(unsigned int i = 0; i < NUM_PERCENTILES; ++i)
	{
		float difference = Abs(m_LastTimeStep - m_PercentileSamples[i].Mean());

		if(difference < leastDifference)
		{
			m_LastPercentile = i;
			leastDifference = difference;
		}
	}

	m_TimeStepHistory.Push(m_LastTimeStep);

	if(m_TimeStepHistory.GetCount() == NUM_SAMPLES)
	{
		m_TimeStepHistory.QSort(0, NUM_SAMPLES, IntegerSort);

		bool halved = false;

		for(unsigned int i = 0; i < NUM_PERCENTILES; ++i)
		{
			if(m_PercentileSamples[i].Samples() == NUM_PERCENTILE_SAMPLES)
			{
				// Give more weight to recent values by occasionally halving the number of samples
				m_PercentileSamples[i].HalveSamples();
				halved = true;
			}

			m_PercentileSamples[i].AddSample(static_cast<float>(m_TimeStepHistory[static_cast<unsigned int>(((m_Percentiles[i] / 100.0f) * NUM_SAMPLES) + 0.5f) - 1]));
		}

		if(halved)
		{
			for(unsigned int i = 0; i < BOUND_BY_COUNT; ++i)
			{
				m_BoundHist[i] /= 2;
			}
		}

		m_TimeStepHistory.ResetCount();
	}

	// Add in the stuff for saying where we're bound (robbed from Debug.cpp)
	float fFps = 1.0f / rage::Max(0.01f, fwTimer::GetSystemTimeStep());
	const float fSignificantAmountOfTime = 1.0f;

	m_fLastFPS = fFps;

	const float fGreenFPSThreshold = DESIRED_FRAME_RATE * 0.9f;
	const float fYellowFPSThreshold = DESIRED_FRAME_RATE * 0.8333f;

	m_FpsColor = (m_fLastFPS >= fGreenFPSThreshold) ? Color32(80,255,80) : ((m_fLastFPS >= fYellowFPSThreshold) ? Color32(255,255,80) : Color32(255,80,80));

	unsigned int limiterStringIndex;

	// TODO: most of these don't need to be static members
	m_fWaitForMain = CDebug::sm_fWaitForMain + CDebug::sm_fDrawlistQPop;
	m_fWaitForRender = CDebug::sm_fWaitForRender;
	m_fWaitForGPU = CDebug::sm_fWaitForGPU;
	m_fEndDraw = CDebug::sm_fEndDraw;
	m_fWaitForDrawSpu = CDebug::sm_fWaitForDrawSpu;
	m_fFifoStallTimeSpu = CDebug::sm_fFifoStallTimeSpu;
	float fBoundTime = Max(m_fWaitForGPU,m_fEndDraw,m_fWaitForDrawSpu,m_fFifoStallTimeSpu);
	if (m_fWaitForMain > Max(m_fWaitForRender, fBoundTime))
	{
		limiterStringIndex = 0;
		fBoundTime = m_fWaitForMain;
	}
	else
	{
		if (fBoundTime > fSignificantAmountOfTime)
		{
			unsigned int index = MaximumIndex(m_fWaitForGPU,m_fEndDraw,m_fWaitForDrawSpu,m_fFifoStallTimeSpu);
			limiterStringIndex = index+2;
		}
		else
		{
			limiterStringIndex = 1;
			fBoundTime = m_fWaitForRender;
		}
	}

	m_LastBound = limiterStringIndex;
	m_fLastBoundTime = fBoundTime;
	m_BoundHist[limiterStringIndex]++;
}

void COverviewScreenFPS::Update()
{
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_X, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT) || COverviewScreenManager::ResetFPS())
	{
		for(unsigned int i = 0; i < BOUND_BY_COUNT; ++i)
		{
			m_BoundHist[i] = 0;
		}

		for(unsigned int i = 0; i < NUM_PERCENTILES; ++i)
		{
			m_PercentileSamples[i].Reset();
		}

		m_TimeStepHistory.ResetCount();

		perfClearingHouse::Reset();

		COverviewScreenManager::SetResetFpsFlag(false);
	}

	UpdateFPSData();
}


void COverviewScreenFPS::RenderBoundInfo(fwRect &rect)
{
	char buffer[64];

	float debugCharWidth = static_cast<float>(ADJUSTED_DEBUG_CHAR_WIDTH()) / GET_TEXT_SCREEN_WIDTH();

	float x = rect.left / GET_TEXT_SCREEN_WIDTH();
	float y = rect.top / GET_TEXT_SCREEN_HEIGHT();

	sprintf(buffer, "FPS: %04.1f (%ims)", m_fLastFPS, m_LastTimeStep);

	grcDebugDraw::Text(Vector2(x, y), m_FpsColor, buffer, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());

	x = rect.right / GET_TEXT_SCREEN_WIDTH();

	grcDebugDraw::Text(Vector2(x - (debugCharWidth * 20), y), Color_white, "Reset: Ctrl+Shift+X", false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());

	rect.top += ADJUSTED_DEBUG_CHAR_HEIGHT() * 3;
}

void COverviewScreenFPS::RenderBoundHistogram(fwRect &rect)
{
	// Get the total number of samples
	int numSamples = 0;
	for(int i=0;i<BOUND_BY_COUNT;i++)
	{
		numSamples += m_BoundHist[i];
	}
	if( numSamples == 0 )
	{
		return;	// No samples... don't render anything
	}

	float debugCharWidth = static_cast<float>(ADJUSTED_DEBUG_CHAR_WIDTH()) / GET_TEXT_SCREEN_WIDTH();
	float debugCharHeight = static_cast<float>(ADJUSTED_DEBUG_CHAR_HEIGHT()) / GET_TEXT_SCREEN_HEIGHT();

	float barWidth = debugCharWidth * 10;
	float barHeight = debugCharHeight * 0.6f;

	float x = (rect.left / GET_TEXT_SCREEN_WIDTH()) + (debugCharWidth * 20) + barWidth;
	float y = rect.top / GET_TEXT_SCREEN_HEIGHT();

	char buffer[2][64];

	Color32 color = Color_white;

	grcDebugDraw::Text(Vector2(x, y), color, "BOUND BY", false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());

	y += debugCharHeight * 2;

	for(unsigned int i = 0; i < BOUND_BY_COUNT; ++i)
	{
		if(i == m_LastBound)
		{
			color = m_FpsColor;
			sprintf(buffer[0], "Bound %5.2fms by", m_fLastBoundTime);
		}
		else
		{
			strcpy(buffer[0], "");
			color = Color_white;
		}

		sprintf(buffer[1], "%-18s%s", buffer[0], m_pBoundStrings[i]);

		float percentage = static_cast<float>(m_BoundHist[i]) / numSamples;

		sprintf(buffer[0], "%-34s%.1f%%", buffer[1], percentage * 100.0f);

		grcDebugDraw::Text(Vector2(x, y), color, buffer[0], false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());

		fwRect barRect(x + (debugCharWidth * 37), y + barHeight, x + (debugCharWidth * 37) + (barWidth * percentage), y);

		if(gDrawListMgr->IsBuildingDrawList())
		{
			DLC(CDrawRectDC, (barRect, color));
		}
		else
		{
			CSprite2d::DrawRect(barRect, color);
		}

		y += debugCharHeight;
	}
}

void COverviewScreenFPS::RenderFPSHistogram(fwRect &rect)
{
	float debugCharWidth = static_cast<float>(ADJUSTED_DEBUG_CHAR_WIDTH()) / GET_TEXT_SCREEN_WIDTH();
	float debugCharHeight = static_cast<float>(ADJUSTED_DEBUG_CHAR_HEIGHT()) / GET_TEXT_SCREEN_HEIGHT();

	float barWidth = debugCharWidth * 10;
	float barHeight = debugCharHeight * 0.6f;

	float x = rect.left / GET_TEXT_SCREEN_WIDTH();
	float y = rect.top / GET_TEXT_SCREEN_HEIGHT();

	char buffer[2][64];

	Color32 color = Color_white;

	grcDebugDraw::Text(Vector2(x, y), color, "TIME STEP PERCENTILES", false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());

	y += debugCharHeight * 2;

	for(unsigned int i = 0; i < NUM_PERCENTILES; ++i)
	{
		if(i == m_LastPercentile)
		{
			color = m_FpsColor;
			sprintf(buffer[0], "%ims", m_LastTimeStep);
		}
		else
		{
			strcpy(buffer[0], "");
			color = Color_white;
		}

		sprintf(buffer[1], "%-6s%i%%", buffer[0], m_Percentiles[i]);

		sprintf(buffer[0], "%-10s%.2fms", buffer[1], m_PercentileSamples[i].Mean());

		grcDebugDraw::Text(Vector2(x, y), color, buffer[0], false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());

		fwRect barRect(x + (debugCharWidth * 17), y + barHeight, x + (debugCharWidth * 17) + (barWidth * Min((Max(m_PercentileSamples[i].Mean(), PERCENTILE_BAR_MIN * 1.1f) - PERCENTILE_BAR_MIN) / (PERCENTILE_BAR_MAX - PERCENTILE_BAR_MIN), 1.0f)), y);

		if(gDrawListMgr->IsBuildingDrawList())
		{
			DLC(CDrawRectDC, (barRect, color));
		}
		else
		{
			CSprite2d::DrawRect(barRect, color);
		}

		y += debugCharHeight;
	}

	rect.top = (y + debugCharHeight) * GET_TEXT_SCREEN_HEIGHT();
}

void COverviewScreenFPS::RenderTimebarBuckets(fwRect &rect)
{
#if	!RAGE_TIMEBARS

	UNUSED_VAR(rect);

#else	//RAGE_TIMEBARS

	char theText[256];

	// Hardcoded, but could search for them if needed
	const int	timeBarMain = 1;
	const int	timeBarRender = 2;
	const int	timeBarGPU = 3;

	// Draw the timebar data titles
	float xPos = rect.left;		// Top left off all timebar bucket rendering
	float yPos = rect.top + 12*ADJUSTED_DEBUG_CHAR_HEIGHT();

	Color32 textColour = Color32(255, 255, 255, 255);

	float x = xPos;
	float y = yPos;

	// Draw the axis legend
	sprintf(theText,"%32s(ms)%8s(ms)%8s(ms)",g_pfTB.GetSetName(timeBarMain), g_pfTB.GetSetName(timeBarRender), g_pfTB.GetSetName(timeBarGPU));
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	x = xPos;
	y = yPos + ADJUSTED_DEBUG_CHAR_HEIGHT();
	const pfTimeBars::sTimebarFrame &mainFrame = g_pfTB.GetTimeBar(timeBarMain).GetRenderTimebarFrame();
	const pfTimeBars::sTimebarFrame &renderFrame = g_pfTB.GetTimeBar(timeBarRender).GetRenderTimebarFrame();
	const pfTimeBars::sTimebarFrame &gpuFrame = g_pfTB.GetTimeBar(timeBarGPU).GetRenderTimebarFrame();
	for(int i=0;i<rage::TIMEBAR_BUCKET_COUNT;i++)
	{
		sprintf(theText,"%-24s%12.4f%12.4f%12.4f",	rage::timebarBucketNames[i], 
													mainFrame.m_Buckets.m_Buckets[i],
													renderFrame.m_Buckets.m_Buckets[i],
													gpuFrame.m_Buckets.m_Buckets[i] );

		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

#endif	//RAGE_TIMEBARS
}

void COverviewScreenFPS::Render(fwRect &rect, bool UNUSED_PARAM(invisible))
{
	grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());

	RenderBoundInfo(rect);
	RenderBoundHistogram(rect);
	RenderFPSHistogram(rect);
	//RenderTimebarBuckets(rect);
	perfClearingHouse::Render(rect, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());

	grcDebugDraw::TextFontPop();
}

//----------------------------------------------------
//	OVERVIEW SCREEN MEMORY
//----------------------------------------------------

int COverviewScreenMemory::m_currentModuleToUpdate = 0;

COverviewScreenMemory::COverviewScreenMemory() : COverviewScreenBase()
{
}

COverviewScreenMemory::~COverviewScreenMemory()
{
}

void COverviewScreenMemory::Update()
{
}

void COverviewScreenMemory::RenderMemoryBuckets(fwRect &rect)
{
	char theText[200];

	float x = rect.left;
	float y = rect.top + (28*ADJUSTED_DEBUG_CHAR_HEIGHT());
	Color32 textColour = Color32(255, 255, 255, 255);

	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, "Memory Buckets", false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	for(int i=0;i<16;i++)
	{
		int virtualMem = (s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed(i);
		sprintf(theText, "%10s %8d\n", sysMemGetBucketName(MemoryBucketIds(i)), (int)COverviewScreenManager::ConvertToDisplayFormat(virtualMem));
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}
}

void COverviewScreenMemory::RenderResourceHeapPages(fwRect &rect)
{
	char theText[200];

	float x = rect.left + 65*ADJUSTED_DEBUG_CHAR_WIDTH();
	float y = rect.top + ADJUSTED_DEBUG_CHAR_HEIGHT();
	Color32 textColour = Color32(255, 255, 255, 255);

	sysMemDistribution distV;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryDistribution(distV);
#if __PS3
	sysMemDistribution distP;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryDistribution(distP);
#endif // __PS3

	sprintf(theText, "Used/Free Virtual" PS3_ONLY(", U/F Phys")"\n" );
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());

	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	for (int s=12; s<24; s++) 
	{
#if __PS3
		sprintf(theText,"%5dk: %5u/%-5u %5u/%-5u\n", 1<<(s-10),distV.UsedBySize[s],distV.FreeBySize[s],distP.UsedBySize[s],distP.FreeBySize[s]);
#else // __PS3
		sprintf(theText,"%5dk: %5u/%-5u\n", 1<<(s-10),distV.UsedBySize[s],distV.FreeBySize[s]);
#endif // __PS3
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}
}

namespace rage {extern sysMemAllocator* g_pResidualAllocator;}
namespace rage {extern sysMemAllocator* g_rlAllocator;}

#if __XENON
namespace rage
{
	extern sysMemAllocator* g_pXenonPoolAllocator;
}
#endif

void COverviewScreenMemory::FormatMemoryUsedFree(char* pText, const char* pszTitle, sysMemAllocator* pAllocator)
{
	streamAssert(pszTitle && pAllocator);

	sprintf(pText,"%24s %8d %8d %8d %8d %5d%%\n", 
		pszTitle, 
		(int) (COverviewScreenManager::ConvertToDisplayFormat(pAllocator->GetMemoryUsed())), 
		(int) (COverviewScreenManager::ConvertToDisplayFormat(pAllocator->GetMemoryAvailable())), 
		(int) (COverviewScreenManager::ConvertToDisplayFormat(pAllocator->GetHeapSize())), 
		(int) (COverviewScreenManager::ConvertToDisplayFormat(pAllocator->GetHighWaterMark(false))), 
		(int) pAllocator->GetFragmentation());
}

void COverviewScreenMemory::FormatMemoryUsedFree(char* pText, const char* pszTitle, sysMemPoolAllocator* pAllocator)
{
	streamAssert(pszTitle && pAllocator);

	sprintf(pText, "%24s %8d %8d %8d %8d %8d %8d %8d %8d %5d%%\n", 
		pszTitle, 
		(int) (COverviewScreenManager::ConvertToDisplayFormat(pAllocator->GetMemoryUsed())), 
		(int) (COverviewScreenManager::ConvertToDisplayFormat(pAllocator->GetMemoryAvailable())), 
		(int) (COverviewScreenManager::ConvertToDisplayFormat(pAllocator->GetHeapSize())), 
		(int) (COverviewScreenManager::ConvertToDisplayFormat(pAllocator->GetPeakMemoryUsed())), 
		(int) pAllocator->GetNoOfUsedSpaces(),
		(int) pAllocator->GetNoOfFreeSpaces(),
		(int) pAllocator->GetPeakSlotsUsed(),
		(int) pAllocator->GetSize(),
		(int) pAllocator->GetFragmentation());
}

void COverviewScreenMemory::FormatMemoryUsedFree(char* pText, const char* pszTitle, size_t usedBytes, size_t freeBytes, size_t totalBytes)
{
	streamAssert(pszTitle);

	sprintf(pText,"%24s %8d %8d %8d\n", pszTitle,
		(int) (COverviewScreenManager::ConvertToDisplayFormat(usedBytes)),
		(int) (COverviewScreenManager::ConvertToDisplayFormat(freeBytes)),
		(int) (COverviewScreenManager::ConvertToDisplayFormat(totalBytes)));
}

void COverviewScreenMemory::FormatMemoryUsedFree(char* pText, const char* pszTitle, size_t usedBytes)
{
	streamAssert(pszTitle);

	sprintf(pText,"%24s %8d\n", pszTitle, (int) (COverviewScreenManager::ConvertToDisplayFormat(usedBytes)));
}

void COverviewScreenMemory::RenderMemoryUsedFree(fwRect &rect)
{
	char text[200];
	Color32 color = Color32(255, 255, 255, 255);
	float x = rect.left;
	float y = rect.top + (ADJUSTED_DEBUG_CHAR_HEIGHT());

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Main
	//
	sprintf(text, "%24s %8s %8s %8s %8s %6s\n", "MAIN", "USED", "FREE", "TOTAL", "HIGH", "FRAG");
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	size_t usedBytes = 0;
	// Game Virtual
	sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);
	if (pAllocator)
	{
		FormatMemoryUsedFree(text, "Game Virtual", pAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();

		sysMemSimpleAllocator* pSimpleAllocator = reinterpret_cast<sysMemSimpleAllocator*>(pAllocator);
		usedBytes = pSimpleAllocator->GetSmallocatorTotalSize();
		FormatMemoryUsedFree(text, "Smallocator", usedBytes);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	// Debug Virtual
#if ENABLE_DEBUG_HEAP
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);
	if (pAllocator)
	{
		FormatMemoryUsedFree(text, "Debug Virtual", pAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}
#endif

	// Resource Virtual
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	if (pAllocator)
	{
		FormatMemoryUsedFree(text, "Resource Virtual", pAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	// Streaming Virtual
#if ONE_STREAMING_HEAP
	usedBytes = (size_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed();
	size_t totalBytes = (size_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
#else
	usedBytes = (size_t) strStreamingEngine::GetAllocator().GetVirtualMemoryUsed();
	size_t totalBytes = (size_t) strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable();
#endif

	size_t freeBytes = totalBytes - usedBytes;
	FormatMemoryUsedFree(text, "Streaming Virtual", usedBytes, freeBytes, totalBytes);
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	// External Virtual
#if ONE_STREAMING_HEAP
	usedBytes = (s32) strStreamingEngine::GetAllocator().GetExternalPhysicalMemoryUsed();	
#else
	usedBytes = (s32) strStreamingEngine::GetAllocator().GetExternalVirtualMemoryUsed();
#endif

	FormatMemoryUsedFree(text, "External Virtual", usedBytes);
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	// Slosh Virtual
	if (pAllocator)
	{
		FormatMemoryUsedFree(text, "Slosh Virtual", (s32) pAllocator->GetMemoryAvailable() - freeBytes);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	// Frag Cache
	FormatMemoryUsedFree(text, "Frag Cache", FRAGCACHEALLOCATOR);
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	// System
#if RSG_ORBIS
	size_t used = 0;
	int32_t result = 0;
	void* pAddress = NULL;
	SceKernelVirtualQueryInfo info;

	while ((result = sceKernelVirtualQuery(pAddress, SCE_KERNEL_VQ_FIND_NEXT, &info, sizeof(SceKernelVirtualQueryInfo))) != SCE_KERNEL_ERROR_EACCES)
	{
		pAddress = info.end;
		const size_t size = reinterpret_cast<size_t>(info.end) - reinterpret_cast<size_t>(info.start);				
		used += size;
	}

	FormatMemoryUsedFree(text, "System", used, sceKernelGetDirectMemorySize() - used, sceKernelGetDirectMemorySize());
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

#elif RSG_DURANGO
	TITLEMEMORYSTATUS status;
	status.dwLength =  sizeof(TITLEMEMORYSTATUS);
	TitleMemoryStatus(&status);

	FormatMemoryUsedFree(text, "System", status.ullTotalMem - status.ullAvailMem, status.ullAvailMem, status.ullTotalMem);
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
#endif

#if RSG_ORBIS
	usedBytes = getVideoPrivateMemory().GetMemoryUsed();
	freeBytes = getVideoPrivateMemory().GetMemoryAvailable();
	totalBytes = usedBytes + freeBytes;
	FormatMemoryUsedFree(text, "Video Memory", usedBytes, freeBytes, totalBytes);
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
#endif // RSG_ORBIS

	//////////////////////////////////////////////////////////////////////////
	// REPLAY ALLOCATOR
	//
#if GTA_REPLAY
	pAllocator = sysMemManager::GetInstance().GetReplayAllocator();
	if (pAllocator)
	{
		FormatMemoryUsedFree(text, "Replay", pAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}
#endif	//GTA_REPLAY

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// NETWORK
	//
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, "", false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	sprintf(text, "%24s %8s %8s %8s %8s %6s\n", "NETWORK", "USED", "FREE", "TOTAL", "HIGH", "FRAG");
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	// Network
	pAllocator = CNetwork::GetNetworkHeap();
	if (pAllocator)
	{
		FormatMemoryUsedFree(text, "Network", pAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	pAllocator = g_rlAllocator;
	if (pAllocator)
	{
		FormatMemoryUsedFree(text, "RLine", pAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	pAllocator = &CNetwork::GetSCxnMgrAllocator();
	if (pAllocator)
	{
		FormatMemoryUsedFree(text,"SCxnMgr", pAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// POOL ALLOCATOR
	//
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, "", false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	sprintf(text, "%24s %8s %8s %8s %8s %8s %8s %8s %8s %6s\n", "POOL ALLOCATOR", "USED", "FREE", "TOTAL", "HIGH", "USED", "FREE", "PEAK", "TOTAL", "FRAG");
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	sysMemPoolAllocator* pPoolAllocator = audVehicleAudioEntity::GetAllocator();
	if (pPoolAllocator)
	{
		FormatMemoryUsedFree(text, "audVehicleAudioEntity", pPoolAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	size_t objSize = CObject::GetPool()->GetStorageSize();
	FormatMemoryUsedFree(text, "CObject",	CObject::GetPool()->GetNoOfUsedSpaces() * objSize,
											CObject::GetPool()->GetNoOfFreeSpaces() * objSize,
											CObject::GetPool()->GetSize() * objSize);
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(text);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	pPoolAllocator = CTask::GetAllocator();
	if (pPoolAllocator)
	{
		FormatMemoryUsedFree(text, "CTask", pPoolAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	pPoolAllocator = CTaskInfo::GetAllocator();
	if (pPoolAllocator)
	{
		FormatMemoryUsedFree(text, "CTaskInfo", pPoolAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

	pPoolAllocator = CVehicle::GetAllocator();
	if (pPoolAllocator)
	{
		FormatMemoryUsedFree(text, "CVehicle", pPoolAllocator);
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(text);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), color, text, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}
}


extern	void GetLoadedSizesSafe(strStreamingModule &module, strLocalIndex objIndex, int &outVirtSize, int &outPhysSize);

void COverviewScreenMemory::RenderStreamingModuleMemory(fwRect &rect)
{
	char theText[200];
	Color32 textColour = Color32(255, 255, 255, 255);
	float x = rect.left + (25*ADJUSTED_DEBUG_CHAR_WIDTH());
	float y = rect.top + (27*ADJUSTED_DEBUG_CHAR_HEIGHT());

	strStreamingInfoManager &strInfoManager = strStreamingEngine::GetInfo();
	strStreamingModuleMgr &moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();

	int numModules = moduleMgr.GetNumberOfModules();

	sprintf(theText, "%-16s%22s%22s\n", "MODULE", "Loaded", "Referenced/Required" );
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	// Loop around each module each frame so it's not so stupidly slow
	int moduleId = m_currentModuleToUpdate;
	m_currentModuleToUpdate = (m_currentModuleToUpdate+1) % numModules;

	// DEBUG
	int Dphys = 0, Dvirt = 0;

	//for(int moduleId=0;moduleId<numModules;moduleId++)	// Dont' update all in one go, too slow
	{
		strStreamingModule *pModule = moduleMgr.GetModule(moduleId);

		ModuleMemoryResults &results = m_ModuleMemResults[moduleId];
		results.loadedMemPhys = 0;
		results.loadedMemVirt = 0;
		results.requiredMemPhys = 0;
		results.requiredMemVirt = 0;

		// Types to track:- Loaded, Referenced/Required.
		for(s32 objIdx = 0; objIdx < pModule->GetCount(); ++objIdx)
		{
			if(pModule->IsObjectInImage(strLocalIndex(objIdx)))
			{
				strIndex streamingIndex = pModule->GetStreamingIndex(strLocalIndex(objIdx));
				strStreamingInfo *info = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);

				if(	!info->IsFlagSet(STRFLAG_INTERNAL_DUMMY) )
				{

					bool bLoaded  = strInfoManager.HasObjectLoaded(streamingIndex);
					bool bReferenced = bLoaded && ( pModule->GetNumRefs( strLocalIndex(objIdx) ) > 0 || info->GetDependentCount() > 0 );
					bool bRequired = pModule->IsObjectRequired( strLocalIndex(objIdx) );

					int virt;	// = info->ComputeVirtualSize(true);
					int phys;	// = info->ComputePhysicalSize(true);

					GetLoadedSizesSafe(*pModule, strLocalIndex(objIdx), virt, phys );

					if(bLoaded)
					{
						results.loadedMemPhys += phys;
						results.loadedMemVirt += virt;

						// DEBUG
						Dphys += phys>>10;
						Dvirt += virt>>10;
					}

					if(bReferenced || bRequired)
					{
						results.requiredMemPhys += phys;
						results.requiredMemVirt += virt;
					}
				}
			}
		}
	}

	sprintf(theText, "%-16s%11s%11s%11s%11s\n", "", "Main", "Vram", "Main", "Vram" );
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	for(int moduleId=0;moduleId<numModules;moduleId++)
	{
		strStreamingModule *pModule = moduleMgr.GetModule(moduleId);
		ModuleMemoryResults &results = m_ModuleMemResults[moduleId];

		const char *pModuleName = pModule->GetModuleName();
		sprintf(theText, "%-16s%10u%10u%10u%10u\n", pModuleName,
			(u32)COverviewScreenManager::ConvertToDisplayFormat(results.loadedMemVirt),
			(u32)COverviewScreenManager::ConvertToDisplayFormat(results.loadedMemPhys),
			(u32)COverviewScreenManager::ConvertToDisplayFormat(results.requiredMemVirt),
			(u32)COverviewScreenManager::ConvertToDisplayFormat(results.requiredMemPhys));
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}

}

// Externs
EXT_PF_VALUE_INT(PendingSize);
EXT_PF_VALUE_INT(CompletedSize);
EXT_PF_VALUE_INT(FailedSize);
EXT_PF_VALUE_INT(DeletedSize);

void COverviewScreenMemory::RenderStreamingBandwidth(fwRect &STATS_ONLY(rect))
{
#if __STATS
	char theText[200];
	Color32 textColour = Color32(255, 255, 255, 255);
	float x = rect.left;
	float y = rect.top + (24*ADJUSTED_DEBUG_CHAR_HEIGHT());

	// Get the values
	int PendingSize = PF_VALUE_VAR(PendingSize).GetValue();
	int CompletedSize = PF_VALUE_VAR(CompletedSize).GetValue();
	int FailedSize = PF_VALUE_VAR(FailedSize).GetValue();
	int DeletedSize = PF_VALUE_VAR(DeletedSize).GetValue();
	
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, "", false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	sprintf(theText,"%-12s%11s%11s%11s%11s\n", "Throughput","pending", "completed", "failed", "deleted" );
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	
	sprintf(theText,"%-12s%10uk%10uk%10uk%10uk\n", "Streaming:",
			(u32)COverviewScreenManager::ConvertToDisplayFormat(PendingSize),
			(u32)COverviewScreenManager::ConvertToDisplayFormat(CompletedSize),
			(u32)COverviewScreenManager::ConvertToDisplayFormat(FailedSize),
			(u32)COverviewScreenManager::ConvertToDisplayFormat(DeletedSize) );
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColour, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
#endif
}

void COverviewScreenMemory::Render(fwRect &rect, bool UNUSED_PARAM(invisible))
{
	grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());

	COverviewScreenManager::DrawToggleDisplay(rect);
	RenderMemoryUsedFree(rect);
	RenderResourceHeapPages(rect);
	RenderStreamingBandwidth(rect);
	RenderMemoryBuckets(rect);
	RenderStreamingModuleMemory(rect);

	grcDebugDraw::TextFontPop();

	if (COverviewScreenManager::DumpFile())
		COverviewScreenManager::WriteToFile(COverviewScreenManager::sm_LogFileNameMemory);
}


//----------------------------------------------------
//	OVERVIEW SCREEN VEHICLES
//----------------------------------------------------

void CalculateDependencyCost(strLocalIndex assetIndex, s32 streamingModuleId, atArray<strIndex>& allDeps, u32& totalMain, u32& totalVram)
{
	strIndex backingNewDepsStore[STREAMING_MAX_DEPENDENCIES*4];
	atUserArray<strIndex> newDeps(backingNewDepsStore, STREAMING_MAX_DEPENDENCIES*4);
	CStreaming::GetObjectAndDependencies(assetIndex, streamingModuleId, newDeps, NULL, 0);

	for (s32 i = 0; i < newDeps.GetCount(); ++i)
	{
		if (allDeps.Find(newDeps[i]) == -1)
		{
			totalMain += strStreamingEngine::GetInfo().GetObjectVirtualSize(newDeps[i]);
			totalVram += strStreamingEngine::GetInfo().GetObjectPhysicalSize(newDeps[i]);

			if( Verifyf( allDeps.GetCapacity() > allDeps.GetCount(), "OverviewScreen:CalculateDependencyCost: Passed deps array has insufficient space. Skipping!" ) )
			{
				allDeps.Append() = newDeps[i];
			}
		}
	}
}

COverviewScreenVehicles::COverviewScreenVehicles() : COverviewScreenBase()
{
}

COverviewScreenVehicles::~COverviewScreenVehicles()
{
}

void COverviewScreenVehicles::Update()
{
}

struct sResAssetData
{
    const char* name;
    u32 main;
    u32 vram;
};
void COverviewScreenVehicles::RenderVehicleDebugInfo(fwRect &rect, bool invisible)
{
	char theText[200];
	Color32 textColor = Color32(255, 255, 255, 255);
    Color32 itemColor = Color32(80, 255, 80, 255);
    Color32 fadedColor = Color32(170, 170, 170, 200);

	float x = rect.left;
	float y = rect.top;

	if (invisible)
		y += rect.bottom + 1;

	fiStream* fp = NULL;
	if (dump)
	{
        char file[64] = {0};

        bool found = false;
        int count = 0;
        sprintf(file, "vehicle_mem_info_%d", count);

        while (!found)
        {
            if (ASSET.Exists(file, "txt"))
            {
                sprintf(file, "vehicle_mem_info_%d", count);
                count++;
            }
            else
                found = true;
        };

        fp = ASSET.Create(file, "txt");

		dump = false;
	}

	sprintf(theText, "\n%s\n", "=========================================================================");
	if (fp)
		fp->Write(theText, (int)strlen(theText));

	CPed* player = FindPlayerPed();
	if (player && fp)
	{
		sprintf(theText, "Player position: %.2f, %.2f, %.2f\n",
			player->GetTransform().GetPosition().GetXf(),
			player->GetTransform().GetPosition().GetYf(),
			player->GetTransform().GetPosition().GetZf());

		fp->Write(theText, (int)strlen(theText));
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
	}


	// Count the number of cars in the world.
	u32	MissionCars = 0, OtherCars = 0, ParkedCars = 0, ParkedMissionCars = 0;
    CountVehicles(MissionCars, ParkedCars, ParkedMissionCars, OtherCars);

    sprintf(theText, "TOTAL %3d / %3d    AMBIENT %2d / %2d    PARKED %2d / %2d    MISSION %2d\n",
            MissionCars + ParkedCars + OtherCars, (int) CVehicle::GetPool()->GetSize(),
            OtherCars, CVehiclePopulation::ms_overrideNumberOfCars != -1 ? CVehiclePopulation::ms_overrideNumberOfCars : (int)CVehiclePopulation::GetDesiredNumberAmbientVehicles(),
            ParkedCars + ParkedMissionCars, CPopCycle::GetCurrentMaxNumParkedCars(),
			MissionCars);
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));


    CLoadedModelGroup cars;
    cars.Merge(&gPopStreaming.GetAppropriateLoadedCars(), &gPopStreaming.GetInAppropriateLoadedCars(), &gPopStreaming.GetSpecialCars(), &gPopStreaming.GetLoadedBoats());
    CLoadedModelGroup allLoadedCars;
    allLoadedCars.Merge(&cars, &gPopStreaming.GetDiscardedCars());

	if (COverviewScreenManager::OrderByMemoryUse())
	{
		allLoadedCars.SortCarsByMemoryUse();
	}
	else if (COverviewScreenManager::OrderByName())
	{
		allLoadedCars.SortCarsByName();
	}

    u32 totalVehicleModels = allLoadedCars.CountMembers();
    
    s32 weaponMain = gPopStreaming.GetTotalMainUsedByWeapons();
    s32 weaponVram = gPopStreaming.GetTotalVramUsedByWeapons();
    sprintf(theText, "TOTAL loaded models %3d    Main RAM %8d    VRAM %8d    TOTAL %8d\n",
            totalVehicleModels,
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalMainUsedByVehicles() - weaponMain),
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalVramUsedByVehicles() - weaponVram),
			(int)COverviewScreenManager::ConvertToDisplayFormat((gPopStreaming.GetTotalMainUsedByVehicles() + gPopStreaming.GetTotalVramUsedByVehicles()) - (weaponMain + weaponVram)));
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

    u32 regAssetMain = gPopStreaming.GetTotalMainUsedByVehicles() - gPopStreaming.GetTotalStreamedMainUsedByVehicles() - weaponMain;
    u32 regAssetVram = gPopStreaming.GetTotalVramUsedByVehicles() - gPopStreaming.GetTotalStreamedVramUsedByVehicles() - weaponVram;
    sprintf(theText, "Regular assets             Main RAM %8d    VRAM %8d    Total %8d\n",
			(int)COverviewScreenManager::ConvertToDisplayFormat(regAssetMain),
			(int)COverviewScreenManager::ConvertToDisplayFormat(regAssetVram),
			(int)COverviewScreenManager::ConvertToDisplayFormat(regAssetMain + regAssetVram));
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

    sprintf(theText, "Streamed mod assets        Main RAM %8d    VRAM %8d    Total %8d\n",
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalStreamedMainUsedByVehicles()),
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalStreamedVramUsedByVehicles()),
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalStreamedMainUsedByVehicles() + gPopStreaming.GetTotalStreamedVramUsedByVehicles()));
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

    // calculate resident asset sizes
    atArray<sResAssetData> residentAssets;
    residentAssets.Reserve(CVehicleModelInfo::GetResidentObjects().GetCount());

    char buf[128] = {0};
    u32 totalResidentMain = 0;
    u32 totalResidentVram = 0;
	for (s32 i = 0; i < CVehicleModelInfo::GetResidentObjects().GetCount(); ++i)
	{
        sResAssetData data;
        data.main = data.vram = 0;
		strIndex index = CVehicleModelInfo::GetResidentObjects()[i];
		strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(index, data.main, data.vram);
		data.name = strStreamingEngine::GetInfo().GetObjectName(index, buf, sizeof(buf));

		totalResidentMain += data.main;
		totalResidentVram += data.vram;

        residentAssets.Push(data);
	}

    sprintf(theText, "Resident assets            Main RAM %8d    VRAM %8d    Total %8d\n",
			(int)COverviewScreenManager::ConvertToDisplayFormat(totalResidentMain),
			(int)COverviewScreenManager::ConvertToDisplayFormat(totalResidentVram),
			(int)COverviewScreenManager::ConvertToDisplayFormat(totalResidentMain + totalResidentVram));
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

    for (s32 i = 0; i < residentAssets.GetCount(); ++i)
    {
        sprintf(theText, "%17s (%d/%d)    Main RAM %8d    VRAM %8d    Total %8d\n",
				residentAssets[i].name,
				i + 1,
				residentAssets.GetCount(),
				(int)COverviewScreenManager::ConvertToDisplayFormat(residentAssets[i].main),
				(int)COverviewScreenManager::ConvertToDisplayFormat(residentAssets[i].vram),
				(int)COverviewScreenManager::ConvertToDisplayFormat(residentAssets[i].main + residentAssets[i].vram));
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
        grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
        y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		if (fp)
			fp->Write(theText, (int)strlen(theText));
    }
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();


    // print info on each vehicle
    char nameBuf[32] = {0};

	strIndex backingAllDepsStore[STREAMING_MAX_DEPENDENCIES*12];
	atUserArray<strIndex> allDeps(backingAllDepsStore, STREAMING_MAX_DEPENDENCIES*12);

	strIndex backingDepStore[STREAMING_MAX_DEPENDENCIES*8];
	atUserArray<strIndex> deps(backingDepStore, STREAMING_MAX_DEPENDENCIES*8);

	strIndex backingHDDepsStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> hdDeps(backingHDDepsStore, STREAMING_MAX_DEPENDENCIES);

	for (s32 i = 0; i < totalVehicleModels; ++i)
	{
		strLocalIndex modelIndex = strLocalIndex(allLoadedCars.GetMember(i));
		fwModelId modelId(modelIndex);
		CVehicleModelInfo* modelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

		deps.ResetCount();
        // find out HD status
        bool isHd = false;
		u32 virtualSizeHd  = 0;
		u32 physicalSizeHd = 0;
		if (modelInfo->GetAreHDFilesLoaded())
		{
			fwModelId modelId(modelIndex);
			hdDeps.ResetCount();
			CModelInfo::GetObjectAndDependencies(modelId, hdDeps, NULL, 0);


			// if hd assets aren't dependencies, add them to the memory. some vehicles don't have hd assets so these
			// indices are the regular txd/frag indices, don't want to count them twice
			if (modelInfo->GetHDTxdIndex() != -1)
			{
				strIndex hdTxdIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDTxdIndex()));

				if (hdDeps.Find(hdTxdIndex) == -1)
				{
					isHd = true;
					deps.Append() = hdTxdIndex;
					virtualSizeHd += CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
					physicalSizeHd += CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
				}
			}

			if (modelInfo->GetHDFragmentIndex() != -1)
			{
				strIndex hdFragIndex = g_FragmentStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDFragmentIndex()));

				if (hdDeps.Find(hdFragIndex) == -1)
				{
					isHd = true;
					deps.Append() = hdFragIndex;
					virtualSizeHd += CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
					physicalSizeHd += CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDFragmentIndex()), g_FragmentStore.GetStreamingModuleId());
				}
			}
		}

		// get potential mods
		u32 totalVirtualMods = 0;
		u32 totalPhysicalMods = 0;
		for (s32 f = 0; f < modelInfo->GetNumVehicleInstances(); ++f)
		{
			if (modelInfo->GetVehicleInstance(f))
			{
				CVehicleStreamRenderGfx* gfx = modelInfo->GetVehicleInstance(f)->GetVehicleDrawHandler().GetVariationInstance().GetVehicleRenderGfx();
				if (gfx)
				{
					for (u8 g = 0; g < VMT_RENDERABLE + MAX_LINKED_MODS; ++g)
					{
						if (gfx->GetFragIndex(g) != -1)
							CalculateDependencyCost(gfx->GetFragIndex(g), g_FragmentStore.GetStreamingModuleId(), deps, totalVirtualMods, totalPhysicalMods);
					}
				}
			}
		}

		// Find size of the vehicle model
		u32 virtualSize  = 0;
		u32 physicalSize = 0;
		CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize, CVehicleModelInfo::GetResidentObjects().GetElements(), CVehicleModelInfo::GetResidentObjects().GetCount(), true);

        virtualSize += virtualSizeHd + totalVirtualMods;
        physicalSize += physicalSizeHd + totalPhysicalMods;
		
		// do we filter the vehicle name in RAG ?
		if (strlen(COverviewScreenManager::sm_VehicleFilterStr) > 0)
		{
			strlwr(COverviewScreenManager::sm_VehicleFilterStr);
			char lowerName[128];
			strcpy(lowerName, CModelInfo::GetBaseModelInfoName(modelId));
			strlwr(lowerName);

			// We skip if the vehicle name doesn't contain the filter
			if (strstr(lowerName, COverviewScreenManager::sm_VehicleFilterStr) == NULL)
				continue;
		}

        formatf(nameBuf, "%s%s", CModelInfo::GetBaseModelInfoName(modelId), isHd ? "(HD)" : "");
        sprintf(theText, "%28s            Main %8d    Vram %8d    Total %8d\n", nameBuf,
				(int)COverviewScreenManager::ConvertToDisplayFormat(virtualSize),
				(int)COverviewScreenManager::ConvertToDisplayFormat(physicalSize),
				(int)COverviewScreenManager::ConvertToDisplayFormat(virtualSize + physicalSize));
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
        grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), itemColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
        y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		if (fp)
			fp->Write(theText, (int)strlen(theText));

		CModelInfo::GetObjectAndDependencies(modelId, deps, CVehicleModelInfo::GetResidentObjects().GetElements(), CVehicleModelInfo::GetResidentObjects().GetCount());

		for (s32 f = 0; f < deps.GetCount(); ++f)
		{
			u32 virtualMemory = strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[f]);
			u32 physicalMemory = strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[f]);
			const char* name = strStreamingEngine::GetInfo().GetObjectName(deps[f], buf, sizeof(buf));

			// do we filter the name in RAG ?
			if (strlen(COverviewScreenManager::sm_VehicleDepsFilterStr) > 0)
			{
				strlwr(COverviewScreenManager::sm_VehicleDepsFilterStr);
				char lowerName[128];
				strcpy(lowerName, name);
				strlwr(lowerName);

				// We skip if the name doesn't contain the filter
				if (strstr(lowerName, COverviewScreenManager::sm_VehicleDepsFilterStr) == NULL)
					continue;
			}

			bool dupe = false;
			if (allDeps.Find(deps[f]) != -1)
				dupe = true;

			// do we show the duplicates ?
			if (!dupe || (dupe && COverviewScreenManager::ShowDupe()))
			{
				sprintf(theText, "%28s (%2d/%2d)    Main %8d    Vram %8d    Total %8d\n", name, f + 1, deps.GetCount(),
						(int)COverviewScreenManager::ConvertToDisplayFormat(virtualMemory),
						(int)COverviewScreenManager::ConvertToDisplayFormat(physicalMemory),
						(int)COverviewScreenManager::ConvertToDisplayFormat(virtualMemory + physicalMemory));
				if (COverviewScreenManager::DumpFile())
				{
					COverviewScreenManager::AppendToDebugInfoStr(theText);
				}
				grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), dupe ? fadedColor : textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
				y += ADJUSTED_DEBUG_CHAR_HEIGHT();
				if (fp)
					fp->Write(theText, (int)strlen(theText));
			}

			if( Verifyf( allDeps.GetCapacity() > allDeps.GetCount(), "OverviewScreenVehicles:RenderVehicleDebugInfo: deps array has insufficient space [size %d]. Skipping!", allDeps.GetCapacity() ) )
			{
				allDeps.Append() = deps[f];
			}
		}
	}
	if (fp)
		fp->Close();
}

void COverviewScreenVehicles::CountVehicles(u32& mission, u32& parked, u32& parkedMission, u32& other)
{
    CVehicle::Pool& pool = *CVehicle::GetPool();
    CVehicle* pVehicle;
    s32 i = (s32) pool.GetSize();
    while(i--)
    {
        pVehicle = pool.GetSlot(i);
        if (pVehicle)
        {
            if (pVehicle->PopTypeIsMission())
            {
                if (pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation)
                {
                    parkedMission++;
                }
                else
                {
                    mission++;
                }
            }
            else
            {
                if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
                {
                    parked++;
                }
                else
                {
                    other++;
                }
            }
        }
    }
}

void COverviewScreenVehicles::Render(fwRect &rect, bool invisible)
{
	grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());

	COverviewScreenManager::DrawToggleDisplay(rect);
	RenderVehicleDebugInfo(rect, invisible);

	grcDebugDraw::TextFontPop();

	if (COverviewScreenManager::DumpFile())
		COverviewScreenManager::WriteToFile(COverviewScreenManager::sm_LogFileNameVehicles);
}

//----------------------------------------------------
//	OVERVIEW SCREEN PEDS
//----------------------------------------------------

COverviewScreenPeds::COverviewScreenPeds() : COverviewScreenBase()
{
}

COverviewScreenPeds::~COverviewScreenPeds()
{
}

void COverviewScreenPeds::Update()
{
}

void COverviewScreenPeds::RenderPedDebugInfo(fwRect &rect, bool invisible)
{
	char theText[200];
	Color32 textColor = Color32(255, 255, 255, 255);
	Color32 itemColor = Color32(80, 255, 80, 255);
	Color32 fadedColor = Color32(170, 170, 170, 200);

	float x = rect.left;
	float y = rect.top;

	if (invisible)
		y += rect.bottom + 1;

	fiStream* fp = NULL;
	if (dump)
	{
        char file[64] = {0};

        bool found = false;
        int count = 0;
        sprintf(file, "ped_mem_info_%d", count);

        while (!found)
        {
            if (ASSET.Exists(file, "txt"))
            {
                sprintf(file, "ped_mem_info_%d", count);
                count++;
            }
            else
                found = true;
        };

        fp = ASSET.Create(file, "txt");

		dump = false;
	}

	sprintf(theText, "\n%s\n", "=========================================================================");
	if (fp)
		fp->Write(theText, (int)strlen(theText));

	CPed* player = FindPlayerPed();
	if (player && fp)
	{
		sprintf(theText, "Player position: %.2f, %.2f, %.2f\n",
			player->GetTransform().GetPosition().GetXf(),
			player->GetTransform().GetPosition().GetYf(),
			player->GetTransform().GetPosition().GetZf());

		fp->Write(theText, (int)strlen(theText));
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
	}


	sprintf(theText, "TOTAL %3d / %3d    Desired Ambient %2d    Desired Scenario %2d    Desired Cops %2d\n",
		CPed::GetPool()->GetNoOfUsedSpaces(), CPed::GetPool()->GetSize(),
		CPopCycle::GetCurrentMaxNumAmbientPeds(),
		CPopCycle::GetCurrentMaxNumScenarioPeds(),
		CPopCycle::GetCurrentMaxNumCopPeds());
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

	sprintf(theText, "[onFoot/InVeh] Cops:%2d/%2d, Swat:%d/%d\n",
		CPedPopulation::ms_nNumOnFootCop, CPedPopulation::ms_nNumInVehCop,
		CPedPopulation::ms_nNumOnFootSwat, CPedPopulation::ms_nNumInVehSwat);
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

	sprintf(theText, "[onFoot/InVeh] Ambient:%2d/%2d Scenario:%2d/%2d Other:%2d/%2d\n",CPedPopulation::ms_nNumOnFootAmbient,CPedPopulation::ms_nNumInVehAmbient,
		CPedPopulation::ms_nNumOnFootScenario,CPedPopulation::ms_nNumInVehScenario,
		CPedPopulation::ms_nNumOnFootOther, CPedPopulation::ms_nNumInVehOther );
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));


	CLoadedModelGroup allLoadedPeds;
	allLoadedPeds.Merge(&gPopStreaming.GetAppropriateLoadedPeds(), &gPopStreaming.GetInAppropriateLoadedPeds(), &gPopStreaming.GetSpecialLoadedPeds(), &gPopStreaming.GetDiscardedPeds());
	
	// need to add player ped in list, it's not stored there by default
	if (CGameWorld::GetMainPlayerInfo() && CGameWorld::GetMainPlayerInfo()->GetPlayerPed())
	{
		allLoadedPeds.AddMember(CGameWorld::GetMainPlayerInfo()->GetPlayerPed()->GetModelId().GetModelIndex());
	}

	if (COverviewScreenManager::OrderByMemoryUse())
	{
		allLoadedPeds.SortPedsByMemoryUse();
	}
	else if (COverviewScreenManager::OrderByName())
	{
		allLoadedPeds.SortPedsByName();
	}
	
	u32 totalPedModels = allLoadedPeds.CountMembers();

	sprintf(theText, "TOTAL loaded models %3d    Main RAM %8d    VRAM %8d    TOTAL %8d\n",
			totalPedModels,
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalMainUsedByPeds()),
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalVramUsedByPeds()),
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalMainUsedByPeds() + gPopStreaming.GetTotalVramUsedByPeds()));
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

	u32 regAssetMain = gPopStreaming.GetTotalMainUsedByPeds() - gPopStreaming.GetTotalStreamedMainUsedByPeds();
	u32 regAssetVram = gPopStreaming.GetTotalVramUsedByPeds() - gPopStreaming.GetTotalStreamedVramUsedByPeds();
	sprintf(theText, "Regular assets             Main RAM %8d    VRAM %8d    Total %8d\n",
			(int)COverviewScreenManager::ConvertToDisplayFormat(regAssetMain),
			(int)COverviewScreenManager::ConvertToDisplayFormat(regAssetVram),
			(int)COverviewScreenManager::ConvertToDisplayFormat(regAssetMain + regAssetVram));
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

	sprintf(theText, "Streamed ped assets        Main RAM %8d    VRAM %8d    Total %8d\n",
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalStreamedMainUsedByPeds()),
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalStreamedVramUsedByPeds()),
			(int)COverviewScreenManager::ConvertToDisplayFormat(gPopStreaming.GetTotalStreamedMainUsedByPeds() + gPopStreaming.GetTotalStreamedVramUsedByPeds()));
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

	// calculate resident asset sizes
	atArray<sResAssetData> residentAssets;
	residentAssets.Reserve(CPedModelInfo::GetResidentObjects().GetCount());

	char buf[128] = {0};
	u32 totalResidentMain = 0;
	u32 totalResidentVram = 0;
	for (s32 i = 0; i < CPedModelInfo::GetResidentObjects().GetCount(); ++i)
	{
		sResAssetData data;
		data.main = data.vram = 0;
		strIndex index = CPedModelInfo::GetResidentObjects()[i];
		strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(index, data.main, data.vram);
		data.name = strStreamingEngine::GetInfo().GetObjectName(index, buf, sizeof(buf));

		totalResidentMain += data.main;
		totalResidentVram += data.vram;

		residentAssets.Push(data);
	}

	sprintf(theText, "Resident assets            Main RAM %8d    VRAM %8d    Total %8d\n",
			(int)COverviewScreenManager::ConvertToDisplayFormat(totalResidentMain),
			(int)COverviewScreenManager::ConvertToDisplayFormat(totalResidentVram),
			(int)COverviewScreenManager::ConvertToDisplayFormat(totalResidentMain + totalResidentVram));
	if (COverviewScreenManager::DumpFile())
	{
		COverviewScreenManager::AppendToDebugInfoStr(theText);
	}
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	if (fp)
		fp->Write(theText, (int)strlen(theText));

	for (s32 i = 0; i < residentAssets.GetCount(); ++i)
	{
		sprintf(theText, "%17s (%d/%d)Main RAM %8d    VRAM %8d    Total %8d\n",
				residentAssets[i].name,
				i + 1,
				residentAssets.GetCount(),
				(int)COverviewScreenManager::ConvertToDisplayFormat(residentAssets[i].main),
				(int)COverviewScreenManager::ConvertToDisplayFormat(residentAssets[i].vram),
				(int)COverviewScreenManager::ConvertToDisplayFormat(residentAssets[i].main + residentAssets[i].vram));
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
        if (fp)
            fp->Write(theText, (int)strlen(theText));
	}
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();

	// print info on each ped
	char nameBuf[32] = {0};

	strIndex backingAllDepsStore[STREAMING_MAX_DEPENDENCIES*12];
	atUserArray<strIndex> allDeps(backingAllDepsStore, STREAMING_MAX_DEPENDENCIES*12);

	strIndex backingDepsStore[STREAMING_MAX_DEPENDENCIES*12];
	atUserArray<strIndex> deps(backingDepsStore, STREAMING_MAX_DEPENDENCIES*12);

	strIndex backingHDDepsStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> hdDeps(backingHDDepsStore, STREAMING_MAX_DEPENDENCIES);

	for (s32 i = 0; i < totalPedModels; ++i)
	{
		strLocalIndex modelIndex = strLocalIndex(allLoadedPeds.GetMember(i));
		fwModelId modelId(modelIndex);
		CPedModelInfo* modelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

		// skip if we don't want to see comp peds or stream peds, or both
		if ((modelInfo->GetIsStreamedGfx() && !COverviewScreenManager::ShowStreamPeds()) || (!modelInfo->GetIsStreamedGfx() && !COverviewScreenManager::ShowCompPeds()))
			continue;

		deps.ResetCount();

		// find out HD status
		bool isHd = false;
		u32 virtualSizeHd  = 0;
		u32 physicalSizeHd = 0;
		if (modelInfo->GetAreHDFilesLoaded())
		{
			hdDeps.ResetCount();
			fwModelId modelId(modelIndex);
			CModelInfo::GetObjectAndDependencies(modelId, hdDeps, NULL, 0);

			if (modelInfo->GetHDTxdIndex() != -1)
			{
				strIndex hdTxdIndex = g_TxdStore.GetStreamingIndex(strLocalIndex(modelInfo->GetHDTxdIndex()));

				// if hd assets aren't dependencies, add them to the memory. some peds don't have hd assets so these
				// indices are the regular txd indices, don't want to count them twice
				if (hdDeps.Find(hdTxdIndex) == -1)
				{
					isHd = true;
					deps.Append() = hdTxdIndex;
					virtualSizeHd += CStreaming::GetObjectVirtualSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
					physicalSizeHd += CStreaming::GetObjectPhysicalSize(strLocalIndex(modelInfo->GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
				}
			}
		}

		// get streamed assets
		u32 totalVirtualStreamed = 0;
		u32 totalPhysicalStreamed = 0;
		for (s32 f = 0; f < modelInfo->GetNumPedInstances(); ++f)
		{
			if (modelInfo->GetPedInstance(f))
			{
				CPedStreamRenderGfx* gfx = modelInfo->GetPedInstance(f)->GetPedDrawHandler().GetPedRenderGfx();
				if (gfx)
				{
                    for (s32 g = 0; g < PV_MAX_COMP; ++g)
					{
                        // drawables
						if (gfx->m_dwdIdx[g] != -1)
						{
							CalculateDependencyCost(strLocalIndex(gfx->m_dwdIdx[g]), g_DwdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
						}
						if (gfx->m_hdDwdIdx[g] != -1)
						{
							CalculateDependencyCost(strLocalIndex(gfx->m_hdDwdIdx[g]), g_DwdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
							isHd = true;
						}

                        // textures
						if (gfx->m_txdIdx[g] != -1)
						{
							CalculateDependencyCost(strLocalIndex(gfx->m_txdIdx[g]), g_TxdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
						}
						if (gfx->m_hdTxdIdx[g] != -1)
						{
							CalculateDependencyCost(strLocalIndex(gfx->m_hdTxdIdx[g]), g_TxdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
							isHd = true;
						}

                        // cloth
						if (gfx->m_cldIdx[g] != -1)
						{
							CalculateDependencyCost(strLocalIndex(gfx->m_cldIdx[g]), g_ClothStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
						}

						// first person alternate drawables
						if (gfx->m_fpAltIdx[g] != -1)
						{
							CalculateDependencyCost(strLocalIndex(gfx->m_fpAltIdx[g]), g_DwdStore.GetStreamingModuleId(), deps, totalVirtualStreamed, totalPhysicalStreamed);
						}
					}

                    // head blend data
                    for (u32 g = 0; g < HBS_MAX; ++g)
                    {
                        if (gfx->m_headBlendIdx[g] > -1)
						{
							CalculateDependencyCost(strLocalIndex(gfx->m_headBlendIdx[g]), (g < HBS_HEAD_DRAWABLES) ? g_DwdStore.GetStreamingModuleId() : (g < HBS_MICRO_MORPH_SLOTS ? g_TxdStore.GetStreamingModuleId() : g_DrawableStore.GetStreamingModuleId()), deps, totalVirtualStreamed, totalPhysicalStreamed);
						}
                    }
				}
			}
		}

		// Find size of the ped model
		u32 virtualSize  = 0;
		u32 physicalSize = 0;
		CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize, CVehicleModelInfo::GetResidentObjects().GetElements(), CVehicleModelInfo::GetResidentObjects().GetCount(), true);

		virtualSize += virtualSizeHd + totalVirtualStreamed;
		physicalSize += physicalSizeHd + totalPhysicalStreamed;

		// do we filter the name of the peds in RAG ?
		if (strlen(COverviewScreenManager::sm_PedFilterStr) > 0)
		{
			strlwr(COverviewScreenManager::sm_PedFilterStr);
			char lowerName[128];
			strcpy(lowerName, CModelInfo::GetBaseModelInfoName(modelId));
			strlwr(lowerName);

			// We skip if the ped name doesn't contain the filter
			if (strstr(lowerName, COverviewScreenManager::sm_PedFilterStr) == NULL)
				continue;
		}

		formatf(nameBuf, "%s%s", CModelInfo::GetBaseModelInfoName(modelId), isHd ? "(HD)" : "");
		sprintf(theText, "%38s            Main %8d    Vram %8d    Total %8d\n", nameBuf,
				(int)COverviewScreenManager::ConvertToDisplayFormat(virtualSize),
				(int)COverviewScreenManager::ConvertToDisplayFormat(physicalSize),
				(int)COverviewScreenManager::ConvertToDisplayFormat(virtualSize + physicalSize));
		if (COverviewScreenManager::DumpFile())
		{
			COverviewScreenManager::AppendToDebugInfoStr(theText);
		}
		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), itemColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
        if (fp)
            fp->Write(theText, (int)strlen(theText));

		CModelInfo::GetObjectAndDependencies(modelId, deps, CVehicleModelInfo::GetResidentObjects().GetElements(), CVehicleModelInfo::GetResidentObjects().GetCount());

		for (s32 f = 0; f < deps.GetCount(); ++f)
		{
			u32 virtualMemory = strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[f]);
			u32 physicalMemory = strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[f]);
			const char* name = strStreamingEngine::GetInfo().GetObjectName(deps[f], buf, sizeof(buf));

			// do we filter the name in RAG ?
			if (strlen(COverviewScreenManager::sm_PedDepsFilterStr) > 0)
			{
				strlwr(COverviewScreenManager::sm_PedDepsFilterStr);
				char lowerName[128];
				strcpy(lowerName, name);
				strlwr(lowerName);

				// We skip if the name doesn't contain the filter
				if (strstr(lowerName, COverviewScreenManager::sm_PedDepsFilterStr) == NULL)
					continue;
			}

			bool dupe = false;
			if (allDeps.Find(deps[f]) != -1)
				dupe = true;

			// do we show the duplicates ?
			if (!dupe || (dupe && COverviewScreenManager::ShowDupe()))
			{
				sprintf(theText, "%38s (%2d/%2d)    Main %8d    Vram %8d    Total %8d\n", name, f + 1, deps.GetCount(),
						(int)COverviewScreenManager::ConvertToDisplayFormat(virtualMemory),
						(int)COverviewScreenManager::ConvertToDisplayFormat(physicalMemory),
						(int)COverviewScreenManager::ConvertToDisplayFormat(virtualMemory + physicalMemory));
				if (COverviewScreenManager::DumpFile())
				{
					COverviewScreenManager::AppendToDebugInfoStr(theText);
				}
				grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), dupe ? fadedColor : textColor, theText, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
				y += ADJUSTED_DEBUG_CHAR_HEIGHT();
				if (fp)
					fp->Write(theText, (int)strlen(theText));
			}

			if( Verifyf( allDeps.GetCapacity() > allDeps.GetCount(), "COverviewScreenPeds::RenderPedDebugInfo:  deps array has insufficient space. Skipping!" ) )
			{
				allDeps.Append() = deps[f];
			}
		}
	}

	if (fp)
		fp->Close();
}

void COverviewScreenPeds::Render(fwRect &rect, bool invisible)
{
	grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());

	COverviewScreenManager::DrawToggleDisplay(rect);
	RenderPedDebugInfo(rect, invisible);

	grcDebugDraw::TextFontPop();

	if (COverviewScreenManager::DumpFile())
		COverviewScreenManager::WriteToFile(COverviewScreenManager::sm_LogFileNamePeds);
}


//----------------------------------------------------
//	OVERVIEW SCREEN DRAW CALLS
//----------------------------------------------------

#if __PS3
const char* drawableContextNames[] =
{
	"NONE",
	"LOD",
	"SLOD1",
	"SLOD2",
	"SLOD3",
	"SLOD4",
	"PROPS",
	"VEG",
	"PEDS",
	"VEHICLES",	
};
CompileTimeAssert(NELEM(drawableContextNames) == DCC_MAX_CONTEXT);
#endif

void COverviewScreenDrawCalls::Render(fwRect & PS3_ONLY(rect), bool UNUSED_PARAM(invisible))
{
#if __PS3
	static float bucketNamesWidth = 184.0f;
	static float categoryBorder = 2.0f;
	static float rowHeight = 16.0f;
	static float textScale = 0.34f;

	char theText[200];
	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(textScale, textScale));

	// Draw the timebar data titles
	float xPos = rect.left;		// Top left off all timebar bucket rendering
	float yPos = rect.top+60;

	Color32 textColour = Color32(255, 255, 255, 255);

	// Make sure the enum and strings match
	Assertf(stricmp(drawableContextNames[DCC_PEDS], "PEDS") == 0, "Values in COverviewScreenDrawCalls::drawableContextNames no longer match the enumeration in grcorespu.h");

	// Draw the context type
	float x = xPos + bucketNamesWidth;
	float y = yPos;
	for(int i=DCC_NO_CATEGORY;i<DCC_MAX_CONTEXT;i++)
	{
		sprintf(theText,"%s",drawableContextNames[i]);
		DebugTextLayout.SetColor(textColour);
		DebugTextLayout.Render(GET_SCREEN_COORDS(x, y), theText);
		float fWidth = GET_TEXT_SCREEN_WIDTH() * DebugTextLayout.GetStringWidthOnScreen(theText, true);
		x += fWidth + categoryBorder;
	}
	sprintf(theText,"Total");
	DebugTextLayout.SetColor(textColour);
	DebugTextLayout.Render(GET_SCREEN_COORDS(x, y), theText);

	const int maxPhase = gDrawListMgr->GetDrawListTypeCount() - 1;
	bool isScannedPhase;

	// Draw the renderphase names
	x = xPos;
	y = yPos + rowHeight;
	for(int dl = 1; dl < gDrawListMgr->GetDrawListTypeCount(); dl++)
	{
		// Kind of ugly, but it skips render phases that don't scan for entitys
		for(int phase = 0; phase < RENDERPHASEMGR.GetRenderPhaseCount(); phase++)
		{
			isScannedPhase = false;
			CRenderPhase &renderPhase = (CRenderPhase &) RENDERPHASEMGR.GetRenderPhase(phase);
			if(renderPhase.GetDrawListType() == dl)
			{
				if(renderPhase.IsScanRenderPhase())
				{
					isScannedPhase = true;
				}
				break;
			}
		}

		if(isScannedPhase || dl == maxPhase)
		{
			sprintf(theText,"%s", gDrawListMgr->GetDrawListName(dl));
			DebugTextLayout.SetColor(textColour);
			DebugTextLayout.Render(GET_SCREEN_COORDS(x, y), theText);
			y += rowHeight;
		}
	}


	// Draw the data
	textColour = Color32(0, 255, 255, 255);
	x = xPos + bucketNamesWidth;
	y = yPos + rowHeight;
	int totals[DCC_MAX_CONTEXT] = {0};
	
	for(int dl = 1; dl < maxPhase; dl++)
	{
		// Kind of ugly, but it skips render phases that don't scan for entitys
		isScannedPhase = false;
		for(int phase = 0; phase < RENDERPHASEMGR.GetRenderPhaseCount(); phase++)
		{
			CRenderPhase &renderPhase = (CRenderPhase &) RENDERPHASEMGR.GetRenderPhase(phase);
			if(renderPhase.GetDrawListType() == dl)
			{
				if(renderPhase.IsScanRenderPhase())
				{
					isScannedPhase = true;
				}
				break;
			}
		}

		if(isScannedPhase || dl == maxPhase)
		{
			int rowTotal = 0;
#if DRAWABLESPU_STATS
			const spuDrawableStats&	stats = gDrawListMgr->GetDrawableStats(dl);
#elif DRAWABLE_STATS
			const drawableStats& stats = gDrawListMgr->GetDrawableStats(dl);
#endif
			for(int j=DCC_NO_CATEGORY;j<DCC_MAX_CONTEXT;j++)
			{
				int stat = stats.DrawCallsPerContext[j];
				totals[j] +=  stat;
				rowTotal += stat;
				if(stat > 0)
				{
					sprintf(theText,"%4d", stat);
					DebugTextLayout.SetColor(textColour);
					DebugTextLayout.Render(GET_SCREEN_COORDS(x, y), theText);
				}
				
				sprintf(theText,"%s",drawableContextNames[j]);
				float fWidth = GET_TEXT_SCREEN_WIDTH() * DebugTextLayout.GetStringWidthOnScreen(theText, true);
				x += fWidth + categoryBorder;
			}
			sprintf(theText,"%4d", rowTotal);
			DebugTextLayout.SetColor(textColour);
			DebugTextLayout.Render(GET_SCREEN_COORDS(x, y), theText);

			x = xPos + bucketNamesWidth;
			y += rowHeight;
		}
	}

	for(int j=DCC_NO_CATEGORY;j<DCC_MAX_CONTEXT;j++)
	{
		sprintf(theText,"%4d", totals[j]);
		DebugTextLayout.SetColor(textColour);
		DebugTextLayout.Render(GET_SCREEN_COORDS(x, y), theText);

		sprintf(theText,"%s",drawableContextNames[j]);
		float fWidth = GET_TEXT_SCREEN_WIDTH() * DebugTextLayout.GetStringWidthOnScreen(theText, true);
		x += fWidth + categoryBorder;
	}
#endif
}

//----------------------------------------------------
//	OVERVIEW SCREEN COMMAND LINE ARGUMENTS
//----------------------------------------------------

void COverviewScreenCmdLineArgs::Render(fwRect &rect, bool UNUSED_PARAM(invisible))
{
	Color32 textColor = Color32(255, 255, 255, 255);

	float x = rect.left;
	float y = rect.top;

	int argCount = sysParam::GetArgCount();
	for (int i=1; i<argCount; i++)
	{
		const char *arg = sysParam::GetArg(i);
		// Skip disabled arguments.
		if (arg[1] == 'X')
		{
			continue;
		}

		grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, arg, false, COverviewScreenManager::GetCurFontScale(), COverviewScreenManager::GetCurFontScale());
		y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	}
}

//----------------------------------------------------
//	OVERVIEW SCREEN KEYBOARD
//----------------------------------------------------

void COverviewScreenKeyboard::Render(fwRect &rect, bool UNUSED_PARAM(invisible))
{
	Color32 textColor     = Color_white;
	Color32 subTitleColor = Color_red3;

	float x = rect.left;
	float y = rect.top;

	float curFontScale = COverviewScreenManager::GetCurFontScale();

	// display the header
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), subTitleColor, "KEYS", false, curFontScale, curFontScale);
	x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), subTitleColor, "DEPT", false, curFontScale, curFontScale);
	x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
	grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), subTitleColor, "USAGE", false, curFontScale, curFontScale);

	x = rect.left;
	y += ADJUSTED_DEBUG_CHAR_HEIGHT();
	
	char theText[64];
	char key_name[32];
	char key_usage[256];

	for(s32 i=1; i<KEY_MAX_CODES; i++)
	{
		CControlMgr::GetKeyboard().GetKeyName(i, key_name, 32);

		// normal key
		if(CControlMgr::GetKeyboard().HasKeyUsage(i, KEYBOARD_MODE_DEBUG) && FilterKeyDept(i, KEYBOARD_MODE_DEBUG))
		{
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, key_name, false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyDept(i, KEYBOARD_MODE_DEBUG, key_usage, 256), false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyUsage(i, KEYBOARD_MODE_DEBUG, key_usage, 256), false, curFontScale, curFontScale);
			x = rect.left;
			y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		}

		// Ctrl + Key
		if(bDisplayCtrlKey && CControlMgr::GetKeyboard().HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CTRL) && FilterKeyDept(i, KEYBOARD_MODE_DEBUG_CTRL))
		{
			sprintf(theText,"CTRL + %s", key_name);
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyDept(i, KEYBOARD_MODE_DEBUG_CTRL, key_usage, 256), false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CTRL, key_usage, 256), false, curFontScale, curFontScale);
			x = rect.left;
			y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		}

		// Shift + Key
		if(bDisplayShiftKey && CControlMgr::GetKeyboard().HasKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT) && FilterKeyDept(i, KEYBOARD_MODE_DEBUG_SHIFT))
		{
			sprintf(theText,"SHIFT + %s", key_name);
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyDept(i, KEYBOARD_MODE_DEBUG_SHIFT, key_usage, 256), false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT, key_usage, 256), false, curFontScale, curFontScale);
			x = rect.left;
			y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		}

		// Alt + Key
		if(bDisplayAltKey && CControlMgr::GetKeyboard().HasKeyUsage(i, KEYBOARD_MODE_DEBUG_ALT) && FilterKeyDept(i, KEYBOARD_MODE_DEBUG_ALT))
		{
			sprintf(theText,"ALT + %s",key_name);
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyDept(i, KEYBOARD_MODE_DEBUG_ALT, key_usage, 256), false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyUsage(i, KEYBOARD_MODE_DEBUG_ALT, key_usage, 256), false, curFontScale, curFontScale);
			x = rect.left;
			y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		}

		// Ctrl + Shift + Key
		if(bDisplayCtrlKey && bDisplayShiftKey && CControlMgr::GetKeyboard().HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT) && FilterKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
		{
			sprintf(theText,"CTRL + SHIFT + %s", key_name);
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT, key_usage, 256), false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT, key_usage, 256), false, curFontScale, curFontScale);
			x = rect.left;
			y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		}

		// Ctrl + Alt + Key
		if(bDisplayCtrlKey && bDisplayAltKey && CControlMgr::GetKeyboard().HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_ALT) && FilterKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_ALT))
		{
			sprintf(theText,"CTRL + ALT + %s", key_name);
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_ALT, key_usage, 256), false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_ALT, key_usage, 256), false, curFontScale, curFontScale);
			x = rect.left;
			y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		}

		// Shift + Alt + Key
		if(bDisplayShiftKey && bDisplayAltKey && CControlMgr::GetKeyboard().HasKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT_ALT) && FilterKeyDept(i, KEYBOARD_MODE_DEBUG_SHIFT_ALT))
		{
			sprintf(theText,"SHIFT + ALT + %s", key_name);
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyDept(i, KEYBOARD_MODE_DEBUG_SHIFT_ALT, key_usage, 256), false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyUsage(i, KEYBOARD_MODE_DEBUG_SHIFT_ALT, key_usage, 256), false, curFontScale, curFontScale);
			x = rect.left;
			y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		}

		// Ctrl + Shift + Alt + Key
		if(bDisplayCtrlKey && bDisplayShiftKey && bDisplayAltKey && CControlMgr::GetKeyboard().HasKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT) && FilterKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT))
		{
			sprintf(theText,"CTRL + SHIFT + ALT + %s", key_name);
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, theText, false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyDept(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT, key_usage, 256), false, curFontScale, curFontScale);
			x += 30*ADJUSTED_DEBUG_CHAR_WIDTH();
			grcDebugDraw::Text(GET_SCREEN_COORDS(x, y), textColor, CControlMgr::GetKeyboard().GetKeyUsage(i, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT, key_usage, 256), false, curFontScale, curFontScale);
			x = rect.left;
			y += ADJUSTED_DEBUG_CHAR_HEIGHT();
		}
	}
}

//
// name:		FilterKeyDept
// description:	we filter the keys depending on the given department
bool COverviewScreenKeyboard::FilterKeyDept(int key, int keyboardMode)
{
	if (COverviewScreenManager::sm_KeyboardDeptFilterStr[0] != '\0')
	{
		const u32 MAX_LEN = 128;
		char lowerName[MAX_LEN];
		
		CControlMgr::GetKeyboard().GetKeyDept(key, keyboardMode, lowerName, MAX_LEN);

		if (stristr(lowerName, COverviewScreenManager::sm_KeyboardDeptFilterStr) == NULL)
			return false;
	}
	return true;
}

//----------------------------------------------------

COverviewScreenFPS				g_OverviewScreenFPS;			// FPS overview screen
COverviewScreenMemory			g_OverviewScreenMemory;			// Memory overview screen
COverviewScreenVehicles			g_OverviewScreenVehicles;		// Vehicles overview screen
COverviewScreenPeds				g_OverviewScreenPeds;			// Peds overview screen
COverviewScreenDrawCalls		g_OverviewScreenDrawCalls;		// DrawCalls overview screen
COverviewScreenCmdLineArgs		g_OverviewScreenCmdLineArgs;	// Command line arguments overview screen
COverviewScreenKeyboard			g_OverviewScreenKeyboard;		// Keyboard overview screen


//----------------------------------------------------
// Overview Screen Manager
//----------------------------------------------------

int COverviewScreenManager::m_OverviewScreenID = OVERVIEW_SCREEN_NONE;
int	COverviewScreenManager::m_OverviewScreenFormat = OVERVIEW_FORMAT_KILOBYTE;
atArray<COverviewScreenBase*> COverviewScreenManager::m_pOverViewScreens;

rage::sysIpcMutex COverviewScreenManager::sm_Mutex = NULL;
float COverviewScreenManager::sm_xScroll = 0.f;
float COverviewScreenManager::sm_yScroll = 0.f;
float COverviewScreenManager::sm_fontscale = 1.f;
bool COverviewScreenManager::sm_DumpFile          = false;
bool COverviewScreenManager::sm_ResetFPS          = false;
bool COverviewScreenManager::sm_ShowDupe          = true;
bool COverviewScreenManager::sm_ShowStreamPeds    = true;
bool COverviewScreenManager::sm_ShowComponentPeds = true;
bool COverviewScreenManager::sm_OrderByMemoryUse  = false;
bool COverviewScreenManager::sm_OrderByName       = false;
char COverviewScreenManager::sm_LogFileNameMemory[RAGE_MAX_PATH]   = "X:\\OverviewScreen_Memory.txt";
char COverviewScreenManager::sm_LogFileNameVehicles[RAGE_MAX_PATH] = "X:\\OverviewScreen_Vehicles.txt";
char COverviewScreenManager::sm_LogFileNamePeds[RAGE_MAX_PATH]     = "X:\\OverviewScreen_Peds.txt";
char COverviewScreenManager::sm_VehicleFilterStr[32]      = "";
char COverviewScreenManager::sm_VehicleDepsFilterStr[32]  = "";
char COverviewScreenManager::sm_PedFilterStr[32]          = "";
char COverviewScreenManager::sm_PedDepsFilterStr[32]      = "";
char COverviewScreenManager::sm_KeyboardDeptFilterStr[32] = "";

atString COverviewScreenManager::sm_DebugInfoStr;

void COverviewScreenManager::Init()
{
	m_pOverViewScreens.PushAndGrow(NULL);					// No screen, saves on adding -1 to everything related to m_OverviewScreenID
	m_pOverViewScreens.PushAndGrow(&g_OverviewScreenFPS);
	m_pOverViewScreens.PushAndGrow(&g_OverviewScreenMemory);
	m_pOverViewScreens.PushAndGrow(&g_OverviewScreenVehicles);
	m_pOverViewScreens.PushAndGrow(&g_OverviewScreenPeds);
	m_pOverViewScreens.PushAndGrow(&g_OverviewScreenDrawCalls);
	m_pOverViewScreens.PushAndGrow(&g_OverviewScreenCmdLineArgs);
	m_pOverViewScreens.PushAndGrow(&g_OverviewScreenKeyboard);

	sm_Mutex = rage::sysIpcCreateMutex();

	sm_xScroll   = 0.f;
	sm_yScroll   = 0.f;
	sm_fontscale = 1.0f;

	sm_DebugInfoStr.Clear();
}

void COverviewScreenManager::Shutdown()
{
    sysIpcDeleteMutex(sm_Mutex);

	m_pOverViewScreens.Reset();		// Clear out registered screens
}

// Update updates regardless of whether any screens are on or not.
// This is needed to collect and make histogram data
void COverviewScreenManager::Update()
{
	rage::sysIpcLockMutex(sm_Mutex);

	// Check keyboard for toggles
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_O, KEYBOARD_MODE_DEBUG_SHIFT, "Next Overview Screen"))
	{
		m_OverviewScreenID = (m_OverviewScreenID+1) % m_pOverViewScreens.size();
		// reset the UI layout scroll
		sm_xScroll = 0.f;
		sm_yScroll = 0.f;
	}

	for(int i=0;i<m_pOverViewScreens.size();i++)
	{
		COverviewScreenBase *pCurrentScreen = m_pOverViewScreens[i];

		if(pCurrentScreen)
		{
			pCurrentScreen->Update();
		}
	}

	rage::sysIpcUnlockMutex(sm_Mutex);

}

void COverviewScreenManager::Render()
{
	if(m_OverviewScreenID == OVERVIEW_SCREEN_NONE)
	{
		for (s32 i = 0; i < m_pOverViewScreens.GetCount(); ++i)
		{
			if (m_pOverViewScreens[i] && m_pOverViewScreens[i]->dump)
			{
                fwRect dummy;
                m_pOverViewScreens[i]->Render(dummy, true);
			}
		}
		return;
	}

	rage::sysIpcLockMutex(sm_Mutex);

	// Adjust the position of the debug text on screen
	if (CControlMgr::GetKeyboard().GetKeyDown(KEY_UP, KEYBOARD_MODE_DEBUG, "Scroll display down", "Overview Screen"))
		sm_yScroll = rage::Min(sm_yScroll + (ADJUSTED_DEBUG_CHAR_HEIGHT() * 20.f * fwTimer::GetSystemTimeStep()), 0.f);
	else if (CControlMgr::GetKeyboard().GetKeyDown(KEY_DOWN, KEYBOARD_MODE_DEBUG, "Scroll display up", "Overview Screen"))
		sm_yScroll = sm_yScroll - (ADJUSTED_DEBUG_CHAR_HEIGHT() * 20.f * fwTimer::GetSystemTimeStep());
	else if (CControlMgr::GetKeyboard().GetKeyDown(KEY_LEFT, KEYBOARD_MODE_DEBUG, "Scroll display right", "Overview Screen"))
		sm_xScroll = rage::Min(sm_xScroll + (ADJUSTED_DEBUG_CHAR_WIDTH() * 20.f * fwTimer::GetSystemTimeStep()), 0.f);
	else if (CControlMgr::GetKeyboard().GetKeyDown(KEY_RIGHT, KEYBOARD_MODE_DEBUG, "Scroll display left", "Overview Screen"))
		sm_xScroll = sm_xScroll - (ADJUSTED_DEBUG_CHAR_WIDTH() * 20.f * fwTimer::GetSystemTimeStep());
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RETURN, KEYBOARD_MODE_DEBUG, "Reset Scroll", "Overview Screen"))
	{
		sm_xScroll = 0.f;
		sm_yScroll = 0.f;
	}

	// Adjust the fontscale (only for this widget)
	if (CControlMgr::GetKeyboard().GetKeyDown(KEY_ADD, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Increase fontscale", "Overview Screen"))
		sm_fontscale += 0.05f;
	else if (CControlMgr::GetKeyboard().GetKeyDown(KEY_SUBTRACT, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Decrease fontscale", "Overview Screen"))
		sm_fontscale -= 0.05f;
	else if (CControlMgr::GetKeyboard().GetKeyDown(KEY_MULTIPLY, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Reset Scale", "Overview Screen"))
		sm_fontscale = 1.0f;

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_K, KEYBOARD_MODE_DEBUG, "Toggle Display Format", "Overview Screen"))
	{
		m_OverviewScreenFormat = (m_OverviewScreenFormat+1) % OVERVIEW_FORMAT_COUNT;
	}
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_K, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Dump Debug Info Into File", "Overview Screen"))
	{
		if (m_OverviewScreenID == OVERVIEW_SCREEN_MEMORY || m_OverviewScreenID == OVERVIEW_SCREEN_PEDS || m_OverviewScreenID == OVERVIEW_SCREEN_VEHICLES)
			sm_DumpFile = true;
	}

	COverviewScreenBase *pCurrentScreen = m_pOverViewScreens[m_OverviewScreenID];

	float x = GET_TEXT_SCREEN_WIDTH() * 0.5f;
	float y = GET_TEXT_SCREEN_HEIGHT() * 0.5f;
	fwRect bgRect(x - 550, y + 380, x + 550, y - 380);	// TODO:: Ditch constants and use scaled size for screen?

	// Draw a default backdrop for all screens here?
	DrawBackground(bgRect);

    bgRect.top  += sm_yScroll;
	bgRect.left += sm_xScroll;
	DrawTitle(bgRect,pCurrentScreen);

	// Update the rect to account for the title
	bgRect.top += ADJUSTED_DEBUG_CHAR_HEIGHT() * 2;
	// Make a small border
	bgRect.left += ADJUSTED_DEBUG_CHAR_HEIGHT();
	bgRect.right -= ADJUSTED_DEBUG_CHAR_HEIGHT();

	pCurrentScreen->Render(bgRect, false);

	rage::sysIpcUnlockMutex(sm_Mutex);
}

void COverviewScreenManager::DrawBackground(fwRect &rect)
{
	Color32 bgColour(0, 0, 0, 128);
	CSprite2d::DrawRectSlow(rect, bgColour );
}

void COverviewScreenManager::DrawTitle(fwRect &rect, COverviewScreenBase *pScreen)
{
	char theText[200];
	sprintf(theText,"OVERVIEW: %s", pScreen->GetScreenTitle());
	grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());
	grcDebugDraw::Text(GET_SCREEN_COORDS(rect.left, rect.top), Color_white, theText, false, 2.0f*COverviewScreenManager::GetCurFontScale(), 2.0f*COverviewScreenManager::GetCurFontScale());
	grcDebugDraw::TextFontPop();
}

void COverviewScreenManager::DrawToggleDisplay(fwRect &rect)
{
	char text[50];
	float debugCharWidth = static_cast<float>(ADJUSTED_DEBUG_CHAR_WIDTH()) / GET_TEXT_SCREEN_WIDTH();
	float x = rect.right / GET_TEXT_SCREEN_WIDTH();
	float y = rect.top / GET_TEXT_SCREEN_HEIGHT();

	sprintf(text,"Dump To File (Ctrl+Alt+K)");
	grcDebugDraw::Text(Vector2(x - (debugCharWidth * 40), y - 3*debugCharWidth), Color32(255,80,80), text, false, 1.4f*COverviewScreenManager::GetCurFontScale(), 1.4f*COverviewScreenManager::GetCurFontScale());
	
	if (COverviewScreenManager::GetDisplayFormat() == OVERVIEW_FORMAT_BYTE)
	{
		sprintf(text,"Display in %s", "Bytes (K to Toggle)");
	}
	else if (COverviewScreenManager::GetDisplayFormat() == OVERVIEW_FORMAT_KILOBYTE)
	{
		sprintf(text,"Display in %s", "KiloBytes (K to Toggle)");
	}
	else
	{
		sprintf(text,"Display in %s", "MegaBytes (K to Toggle)");
	}
	grcDebugDraw::Text(Vector2(x - (debugCharWidth * 40), y - debugCharWidth), Color32(255,80,80), text, false, 1.4f*COverviewScreenManager::GetCurFontScale(), 1.4f*COverviewScreenManager::GetCurFontScale());
}

void COverviewScreenManager::DumpPedAndVehicleData()
{
	DumpPedData();
	DumpVehicleData();
}

void COverviewScreenManager::DumpVehicleData()
{
	g_OverviewScreenVehicles.DumpData();
}

void COverviewScreenManager::DumpPedData()
{
	g_OverviewScreenPeds.DumpData();
}

size_t COverviewScreenManager::ConvertToDisplayFormat(size_t val)
{
	if (m_OverviewScreenFormat == OVERVIEW_FORMAT_BYTE)
		return val;
	else if (m_OverviewScreenFormat == OVERVIEW_FORMAT_KILOBYTE)
		return (val >> 10);
	else
		return ((val >> 10) >> 10);
}

void COverviewScreenManager::WriteToFile(const char* pszFileName)
{
	if (!pszFileName)
		return;

	fiStream* logStream = pszFileName[0] ? ASSET.Create(pszFileName, "") : NULL;

	if (!logStream)
	{
		Errorf("Could not create '%s'", pszFileName);
		return;
	}

	logStream->Write(sm_DebugInfoStr.c_str(), sm_DebugInfoStr.GetLength());
	logStream->Close();

	sm_DumpFile = false;
	sm_DebugInfoStr.Clear();
}

void COverviewScreenManager::InitWidgets()
{
	// Set up some bank stuff.
	bkBank &bank = BANKMGR.CreateBank("Overview Screen");

	const char* overviewScreenList[OVERVIEW_SCREEN_COUNT+1] =
	{
		"None",
		"FPS",
		"Memory",
		"Vehicles",
		"Peds",
		"Drawcalls",
		"Command Line Arguments",
		"Keyboard"
	};
	CompileTimeAssert(NELEM(overviewScreenList) == OVERVIEW_SCREEN_COUNT+1);
	
	const char* overviewScreenFormatList[OVERVIEW_FORMAT_COUNT] =
	{
		"Bytes",
		"KiloBytes",
		"MegaBytes"
	};
	CompileTimeAssert(NELEM(overviewScreenFormatList) == OVERVIEW_FORMAT_COUNT);

	bank.AddCombo("Override Screen", &m_OverviewScreenID, NELEM(overviewScreenList), overviewScreenList);
	bank.AddCombo("Display in ", &m_OverviewScreenFormat, NELEM(overviewScreenFormatList), overviewScreenFormatList);
	bank.AddSlider("Fontscale", &sm_fontscale, 0.1f, 5.0f, 0.05f);
	bank.AddToggle("Show shared assets", &sm_ShowDupe);
	bank.AddToggle("Order Assets By Name", &sm_OrderByName);
	bank.AddToggle("Order Assets By Memory use", &sm_OrderByMemoryUse);
	
	// FPS --------
	bank.PushGroup("FPS", false);
	{
		bank.AddButton("Reset FPS data", ResetFpsCB);
	}
	bank.PopGroup();
	
	// MEMORY --------
	bank.PushGroup("Memory", false);
	{
		bank.AddText("Memory Log file", sm_LogFileNameMemory, 256);
		bank.AddButton("Save to log", SaveToLogMemoryCB);
	}
	bank.PopGroup();
	
	// VEHICLES --------
	bank.PushGroup("Vehicles", false);
	{
		bank.PushGroup("Filters", false);
		{
			bank.AddText("Vehicle Name Containing ", sm_VehicleFilterStr, 32);
			bank.AddText("Vehicle Asset Containing", sm_VehicleDepsFilterStr, 32);
		}
		bank.PopGroup();
		bank.AddText("Vehicles Log file", sm_LogFileNameVehicles, 256);
		bank.AddButton("Save to log", SaveToLogVehiclesCB);
	}
	bank.PopGroup();

	// PEDS --------
	bank.PushGroup("Peds", false);
	{
		bank.PushGroup("Filters", false);
		{
			bank.AddText("Ped Name Containing ", sm_PedFilterStr, 32);
			bank.AddText("Ped Asset Containing", sm_PedDepsFilterStr, 32);
		}
		bank.PopGroup();
		bank.AddToggle("Show Component Peds", &sm_ShowComponentPeds);
		bank.AddToggle("Show Stream Peds", &sm_ShowStreamPeds);
		bank.AddText("Peds Log file", sm_LogFileNamePeds, 256);
		bank.AddButton("Save to log", SaveToLogPedsCB);
	}
	bank.PopGroup();

	// KEYBOARD --------
	bank.PushGroup("Keyboard", false);
	{
		bank.AddText("Key Dept Containing", sm_KeyboardDeptFilterStr, 32);
		bank.AddToggle("Display Ctrl Keys",  &g_OverviewScreenKeyboard.bDisplayCtrlKey);
		bank.AddToggle("Display Shift Keys", &g_OverviewScreenKeyboard.bDisplayShiftKey);
		bank.AddToggle("Display Alt Keys",   &g_OverviewScreenKeyboard.bDisplayAltKey);
	}
	bank.PopGroup();
}

void COverviewScreenManager::ResetFpsCB()
{
	SetResetFpsFlag(true);
}

void COverviewScreenManager::SaveToLogMemoryCB()
{
	m_OverviewScreenID = OVERVIEW_SCREEN_MEMORY;
	sm_DumpFile = true;
}

void COverviewScreenManager::SaveToLogVehiclesCB()
{
	m_OverviewScreenID = OVERVIEW_SCREEN_VEHICLES;
	sm_DumpFile = true;
}

void COverviewScreenManager::SaveToLogPedsCB()
{
	m_OverviewScreenID = OVERVIEW_SCREEN_PEDS;
	sm_DumpFile = true;
}

//----------------------------------------------------

#endif	//__BANK
