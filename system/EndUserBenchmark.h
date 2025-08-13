#ifndef __ENDUSER_BENCHMARKS__H__
#define __ENDUSER_BENCHMARKS__H__

#define	COLLECT_END_USER_BENCHMARKS	(1 && RSG_PC)

#define MAX_FRAME_TIME_MS			( 2000 )
#define MAX_FRAME_TIME_COUNT		( 36000 )

#if COLLECT_END_USER_BENCHMARKS

#include "AutoGPUCapture.h"

class EndUserBenchmarks
{
public:

	#define BENCHMARK_RESULT_WRITE_PATH	"user:/Benchmarks"

	class fpsResult
	{
	public:
		float	min;
		float	max;
		float	average;
		int		frameCount;
		int		frameTimeSlots[MAX_FRAME_TIME_MS];	// frameTimes[x] counts the number of frames that took <'x'ms.
		float*	frameTimesMs;
	};

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();

	static void ResetBenchmarks();
	static void	StartBenchmark();
	static void	StopBenchmark();

	static void	OutputBenchmarkResults();

#if __BANK
	// DEBUG widgets for testing purposes
	static void	InitWidgets();
#endif

	static void SetWasStartedFromPauseMenu(eFRONTEND_MENU_VERSION menu)	{ m_bMenuTrigger = menu; }
	static bool GetWasStartedFromPauseMenu(eFRONTEND_MENU_VERSION menu)	{ return m_bMenuTrigger == menu; }
	static bool GetWasBenchmarkOnCommandline();

	static s32	GetIterationCount();
	static s32	GetSinglePassInfo();

	static bool IsBenchmarkScriptRunning();

private:

	static void	AccumulateFPSData();

	static void StartBenchmarkScript();
	static bool IsScriptRunning();

	static eFRONTEND_MENU_VERSION m_bMenuTrigger;

	static bool	m_bActive;
	static bool	m_bStartedFromCommandline;
	static int	m_StartTestCounter;
	static strLocalIndex m_scriptID;

	static MetricsCapture::SampledValue <float> m_fpsSample;
	static atArray<fpsResult>					m_fpsResults;
	static int									m_frameTimeSlots[MAX_FRAME_TIME_MS];
	static float*								m_pFrameTimesMs;
};

#endif	//COLLECT_END_USER_BENCHMARKS

#endif // !__ENDUSER_BENCHMARKS__H__
