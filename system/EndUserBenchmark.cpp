#include "EndUserBenchmark.h"

#if COLLECT_END_USER_BENCHMARKS

#include "time.h"
#include "fwsys/timer.h"
#include "grcore/diag_d3d.h"
#include "grcore\adapter_d3d11.h"
#include "script\script.h"
#include "script\streamedscripts.h"
#include "script\thread.h"
#include "camera/CamInterface.h"
#include "network/NetworkInterface.h"

#if __BANK
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/bkmgr.h"
#endif

#include "Network/Live/NetworkTelemetry.h"

eFRONTEND_MENU_VERSION					EndUserBenchmarks::m_bMenuTrigger = 0;
bool									EndUserBenchmarks::m_bActive = false;
bool									EndUserBenchmarks::m_bStartedFromCommandline = false;
int										EndUserBenchmarks::m_StartTestCounter = 0;
strLocalIndex							EndUserBenchmarks::m_scriptID = strLocalIndex(-1);

MetricsCapture::SampledValue <float>	EndUserBenchmarks::m_fpsSample;
atArray<EndUserBenchmarks::fpsResult>	EndUserBenchmarks::m_fpsResults;
int										EndUserBenchmarks::m_frameTimeSlots[MAX_FRAME_TIME_MS];
float*									EndUserBenchmarks::m_pFrameTimesMs = NULL;

NOSTRIP_PC_PARAM(benchmark,				"Starts the benchmark test from the command line");
NOSTRIP_PC_PARAM(benchmarkFrameTimes,	"Optionally output the individual frame times from the benchmark");
NOSTRIP_PC_PARAM(benchmarkIterations,	"Specifies the number of iterations to run the benchmark for");
NOSTRIP_PC_PARAM(benchmarkPass,			"Specifies an individual benchmark scene test should be done, and which test that should be");

void EndUserBenchmarks::Init(unsigned /*initMode*/)
{
	m_bActive = false;
	ResetBenchmarks();

#if __BANK
	InitWidgets();
#endif
}

void EndUserBenchmarks::Shutdown(unsigned /*shutdownMode*/)
{
	if (m_bActive)
	{
		StopBenchmark();
		ResetBenchmarks();
		Assert(m_pFrameTimesMs == NULL);
	}
	m_bActive = false;
}

bool EndUserBenchmarks::GetWasBenchmarkOnCommandline()
{
	return PARAM_benchmark.Get();
}

void EndUserBenchmarks::Update()
{
#if !__FINAL
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S,KEYBOARD_MODE_DEBUG_CNTRL_SHIFT,"Start Benchmark mode"))
	{
		if (!m_bActive)
		{
			ResetBenchmarks();
			StartBenchmark();
		}
		else
		{
			StopBenchmark();
			OutputBenchmarkResults();
		}
	} else
#endif // !__FINAL
	if( m_bActive )
	{
		if (m_fpsSample.Samples() < MAX_FRAME_TIME_COUNT-1)
		{
			// Collect FPS data
			AccumulateFPSData();
		}
		else
		{
			Errorf("Ran out of samples. Dumping benchmark .txt file");
			StopBenchmark();
			OutputBenchmarkResults();
		}
	}
	else if( (PARAM_benchmark.Get() && m_bStartedFromCommandline == false) || GetWasStartedFromPauseMenu(FE_MENU_VERSION_LANDING_MENU))
	{
		if( CNetwork::GetGoStraightToMultiplayer() )
		{
			m_bStartedFromCommandline = true;	// Set to true for MP game, so if we go back to SP this still won't fire.
		}
		else
		{
			// We must wait for the script global blocks to be init'd
			scrValue **globals = scrProgram::GetGlobalsBlocks();
			if( globals && *globals && !IsScriptRunning() /* && camInterface::IsFadedIn() */)
			{
				// Start it
				StartBenchmarkScript();
				m_bStartedFromCommandline = true;
			}
		}
	}
}

void EndUserBenchmarks::ResetBenchmarks()
{
	for(int result=0; result<m_fpsResults.size(); ++result)
	{
		fpsResult &thisResult = m_fpsResults[result];
		delete [] thisResult.frameTimesMs;
	}

	m_fpsResults.Reset();

	delete [] m_pFrameTimesMs;
	m_pFrameTimesMs = NULL;

	m_bMenuTrigger = 0u;
}

void EndUserBenchmarks::StartBenchmark()
{
	Assertf(m_bActive == false, "EndUserBenchmarks::StartBenchmark(): Trying to start a benchmark run when one is already running");
	for(int i=0; i<MAX_FRAME_TIME_MS; ++i)
		m_frameTimeSlots[i] = 0;

	m_fpsSample.Reset();
	m_bActive = true;
	m_pFrameTimesMs = rage_new float[MAX_FRAME_TIME_COUNT];
}

void EndUserBenchmarks::StopBenchmark()
{
	Assertf(m_bActive == true, "EndUserBenchmarks::StopBenchmark(): Trying to stop a benchmark run that hasn't been started");
	m_bActive = false;

	// Store accumulated fps result
	fpsResult &result = m_fpsResults.Grow();
	result.average = m_fpsSample.Mean();
	result.max = m_fpsSample.Max();
	result.min = m_fpsSample.Min();

	result.frameCount = m_fpsSample.Samples();
	for(int i=0; i<MAX_FRAME_TIME_MS; ++i)
		result.frameTimeSlots[i] = m_frameTimeSlots[i];

	// Pass the memory to the fpsResult.
	result.frameTimesMs = m_pFrameTimesMs;
	m_pFrameTimesMs = NULL;
}

void EndUserBenchmarks::OutputBenchmarkResults()
{
	// Generate a filename + path
	time_t	timeStamp;
	time(&timeStamp);
	tm* date = localtime(&timeStamp);
	char filenameAndPath[RAGE_MAX_PATH];
	sprintf(filenameAndPath, "%s/Benchmark-%02d-%02d-%02d-%02d-%02d-%02d", BENCHMARK_RESULT_WRITE_PATH, date->tm_year-100, date->tm_mon+1, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec );

	// Create the folder
	ASSET.CreateLeadingPath(filenameAndPath);

	// Save out our file
	fiStream *outHandle = ASSET.Create(filenameAndPath,"txt");
	if( outHandle != INVALID_HANDLE_VALUE )
	{
		char* tempString;

		fprintf(outHandle, "Frames Per Second (Higher is better) Min, Max, Avg\r\n");
		for(int i=0;i<m_fpsResults.size();i++)
		{
			fpsResult &thisResult = m_fpsResults[i];
			fprintf(outHandle, "Pass %d, %f, %f, %f\r\n", i, thisResult.min, thisResult.max, thisResult.average);
		}

		fprintf(outHandle,"\n");

		fprintf(outHandle, "Time in milliseconds(ms). (Lower is better). Min, Max, Avg\r\n");
		for(int i=0;i<m_fpsResults.size();i++)
		{
			fpsResult &thisResult = m_fpsResults[i];
			fprintf(outHandle, "Pass %d, %f, %f, %f\r\n", i, 1000.0f/thisResult.max, 1000.0f/thisResult.min, 1000.0f/thisResult.average);
		}

		fprintf(outHandle, "\nFrames under 16ms (for 60fps): \n");
		for(int i=0;i<m_fpsResults.size();i++)
		{
			fpsResult &thisResult = m_fpsResults[i];

			int frameCount=0;

			for(int frameTime=0; frameTime < 16; ++frameTime)
			{
				frameCount += thisResult.frameTimeSlots[frameTime];
			}
			fprintf(outHandle, "Pass %d: %d/%d frames (%.2f%%)\n", i, frameCount, thisResult.frameCount, ((float)frameCount/(float)thisResult.frameCount)*100.0f);
		}

		fprintf(outHandle, "\nFrames under 33ms (for 30fps): \n" );
		for(int i=0;i<m_fpsResults.size();i++)
		{
			fpsResult &thisResult = m_fpsResults[i];

			int frameCount=0;

			for(int frameTime=0; frameTime < 33; ++frameTime)
			{
				frameCount += thisResult.frameTimeSlots[frameTime];
			}
			fprintf(outHandle, "Pass %d: %d/%d frames (%.2f%%)\n", i, frameCount, thisResult.frameCount, ((float)frameCount/(float)thisResult.frameCount)*100.0f);
		}

		fprintf(outHandle, "\n");

		int percentiles[] = { 50, 75, 80, 85, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99 };
		int numPercentiles = _countof(percentiles);

		for(int i=0;i<m_fpsResults.size();i++)
		{
			fpsResult &thisResult = m_fpsResults[i];

			fprintf(outHandle,"Percentiles in ms for pass %d\n", i);

			for(int percentileNum=0; percentileNum<numPercentiles; ++percentileNum)
			{
				// Count the number of frames inside the 'n'th percentile.
				int sampleCountRequiredForPercentile = (int)((float)(thisResult.frameCount / (100.0f/(float)percentiles[percentileNum])));
				int sampleCountUnderPercentile=0;
				int numMs;
				for(numMs=0; numMs<MAX_FRAME_TIME_MS; ++numMs)
				{
					if(sampleCountUnderPercentile + thisResult.frameTimeSlots[numMs] >= sampleCountRequiredForPercentile)
						break;
					sampleCountUnderPercentile += thisResult.frameTimeSlots[numMs];
				}
				fprintf(outHandle, "%d%%,\t%.2f\n", percentiles[percentileNum], (float)numMs);
			}

			fprintf(outHandle, "\n", i);
		}

		fprintf(outHandle, "=== SYSTEM ===\n");

		DXDiag::DumpDisplayableInfoString(&tempString, true);
		fprintf(outHandle, "%s",tempString);
		delete [] tempString;

		DXGI_ADAPTER_DESC oAdapterDesc;
		GRCDEVICE.GetAdapterDescription(oAdapterDesc);
		fprintf(outHandle, "Graphics Card Vendor Id 0x%x with Device ID 0x%x\n\n", oAdapterDesc.VendorId, oAdapterDesc.DeviceId);

		CSettingsManager::GetInstance().GraphicsConfigSource(tempString);
		delete [] tempString;

		// Output the game settings
		const Settings & CurrentSettings = CSettingsManager::GetInstance().GetSettings();
		const CVideoSettings & VideoSettings = CurrentSettings.m_video;

		fprintf(outHandle, "=== SETTINGS ===\n");
		fprintf(outHandle, "Display: %dx%d (%s) @ %dHz VSync %s", VideoSettings.m_ScreenWidth, VideoSettings.m_ScreenHeight, VideoSettings.m_Windowed ? "Windowed" : "FullScreen", VideoSettings.m_RefreshRate, VideoSettings.m_VSync ? "ON" : "OFF");
		if (VideoSettings.m_Stereo)
			fprintf(outHandle, "NVSTEREO");
		fprintf(outHandle,"\n");
		CurrentSettings.m_graphics.OutputSettings(outHandle);

		outHandle->Close();
	}

	if (PARAM_benchmarkFrameTimes.Get())
	{
		for(u32 pass=0; pass<m_fpsResults.size(); ++pass)
		{
			// Save out our files
			sprintf(filenameAndPath, "%s/FrameTimes-Pass%d-%02d-%02d-%02d-%02d-%02d-%02d", BENCHMARK_RESULT_WRITE_PATH, pass, date->tm_year-100, date->tm_mon+1, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec );

			fiStream *outHandle = ASSET.Create(filenameAndPath,"txt");
			if( outHandle != INVALID_HANDLE_VALUE )
			{
				fpsResult &thisResult = m_fpsResults[pass];

				fprintf(outHandle, "Frame\tTime(ms)\n");
				for(u32 frameNo=0;frameNo<thisResult.frameCount; ++frameNo)
				{
					fprintf(outHandle, "%d\t%.3f\n", frameNo, thisResult.frameTimesMs[frameNo]);
				}

				outHandle->Close();
			}
		}
	}

	CNetworkTelemetry::AppendBenchmarkMetrics(m_fpsResults);
}


s32	EndUserBenchmarks::GetIterationCount()
{
	const char *pParam;
	if( PARAM_benchmarkIterations.Get(pParam) )
	{
		s32 paramVal = atoi(pParam);
		return paramVal;
	}
	return -1;
}

s32	EndUserBenchmarks::GetSinglePassInfo()
{
	const char *pParam;
	if( PARAM_benchmarkPass.Get(pParam) )
	{
		s32 paramVal = atoi(pParam);
		return paramVal;
	}
	return -1;
}

void EndUserBenchmarks::AccumulateFPSData()
{
	float fSystemTime = fwTimer::GetSystemTimeStep();

	m_pFrameTimesMs[m_fpsSample.Samples()] = fSystemTime * 1000.0f;

	float fps = 1.0f / rage::Max(0.001f, fSystemTime);
	m_fpsSample.AddSample(fps);

	int numMs = (int)(fSystemTime * 1000.0f);	// To ms
	numMs = rage::Min(numMs, MAX_FRAME_TIME_MS-1);
	m_frameTimeSlots[numMs]++;
}


void EndUserBenchmarks::StartBenchmarkScript()
{
	m_scriptID = g_StreamedScripts.FindSlot("benchmark");
	if(Verifyf(m_scriptID != -1, "Graphics benchmarking script does not exist"))
	{
		g_StreamedScripts.StreamingRequest(m_scriptID, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
		CStreaming::LoadAllRequestedObjects();
		if(Verifyf(g_StreamedScripts.HasObjectLoaded(m_scriptID), "Failed to stream in benchmarking script"))
		{
			CTheScripts::GtaStartNewThreadOverride("benchmark", NULL, 0, GtaThread::GetDefaultStackSize());
		}
	}
}

bool EndUserBenchmarks::IsBenchmarkScriptRunning()
{
	if(m_scriptID == -1)
	{
		// We must wait for the script global blocks to be init'd
		scrValue **globals = scrProgram::GetGlobalsBlocks();
		if(globals && *globals)
		{
			m_scriptID = g_StreamedScripts.FindSlot("benchmark");
		}
	}

	return IsScriptRunning();
}

bool EndUserBenchmarks::IsScriptRunning()
{
	bool bIsRunning = false;

	if(m_scriptID != -1)
	{
		bIsRunning = g_StreamedScripts.GetNumRefs(strLocalIndex(m_scriptID)) > 0;
	}

	return bIsRunning;
}

#if __BANK
void	EndUserBenchmarks::InitWidgets()
{
	bkBank * pBank = &BANKMGR.CreateBank("End User Benchmarks");
	if(pBank)
	{
		pBank->AddButton("Reset Benchmarks",			ResetBenchmarks);
		pBank->AddButton("Start Benchmark",				StartBenchmark);
		pBank->AddButton("Stop Benchmark",				StopBenchmark);
		pBank->AddButton("Output Benchmark Results",	OutputBenchmarkResults);
	}
}
#endif	//__BANK

#endif	//COLLECT_END_USER_BENCHMARKS
