//
// filename:	TracingStats.cpp
// 
// description:	Classes supporting a 'trace' of a variable or 'stat' over time, output to TTY.
//
//				A trace of your stat would typically be pumped to the TTY, you can choose base class methods 
//				to control frequency to output it. Or just do this yourself. 
//				
//				It might be good practice to avoid pumping out tracing stats every single frame. (*)
//
//				Add classes as you see fit to support your custom derived class of tracingStat.
//				the base class supports common functionality for these classes making 
//				it fairly simple to add new types of sampled stats, 
//				eg. sampling every n frames or sampling every n ms.
//
//				The tools supporting the tty output are designed to calculate averages min & maxes of such values.
//				;however should you wish to do such sampling capture yourself there is no 
//				reason not to do this internally in your class if you desire so.
//
//				* Sampling buffers are available so you can easily append a sample to a buffer.
//				when full the sample buffer will spit out its data in max,min,mean form with some tracing stat 
//				name mangling in order to represent it properly. This does make the names of 
//				tracing stats quite long-winded but seems to be required to properly qualify the inherent
//				complexity of our game, its branches, its tools, and statistics in general.
//
// author:		Derek Ward - Jan 2009

// Include self first
#include "tools/TracingStats.h"

// Game includes
#include "control/gamelogic.h"
#include "peds/ped.h"
#include "scene/scene.h"
#include "tools/smoketest.h"
#include "vehicles/vehicle.h"

// Rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "grprofile/timebars.h"

#if TRACING_STATS

// Preprocessor directives
#define TRACING_STAT_BANK_NAME	"TracingStats"
#define TRACING_STAT_TTY_COLOUR "NamedColor:Purple"
#define TS_PERF_FILE			"Perf"
#define TS_MEM_FILE				"Mem"

// define the channel where tracing stat data is squirted out to.
RAGE_DEFINE_CHANNEL(TracingStats)

// Globals
CTracingStats gTracingStats;
const s32 CTracingStats::DEFAULT_LIFE = -1; // -1 is immortal.

// Params
PARAM(TracingStats,						"Switch the entire Tracing stat system on/off");
	PARAM(TracingStatAll,				"Enable All Tracing stats");
		PARAM(TracingStatPerformance,	"Enable Performance Tracing stats");
		PARAM(TracingStatMemorySizes,	"Enable Memory Size Tracing stats");

//---------------------------------------------------------------------------------
#if __BANK
void CTracingStat::CreateBankToggle()
{
	bkBank* bank = BANKMGR.FindBank(TRACING_STAT_BANK_NAME);
	if (bank)
		bank->AddToggle(m_name, &m_bActive);
}
#endif // __BANK

//---------------------------------------------------------------------------------
CTracingStat::CTracingStat(const char* const pName, 
						   const char* const pFileName, 
						   const bool bAppendLevelNameToFile,
						   const bool bActive, 
						   const s32 life, 
						   const u32 scheduleFrequency)
 :	m_bActive(bActive), 
	m_nTimesOutput(0), 
	m_frameCounter(0), 
	m_lastOutputTime(0), 
	m_scheduleFrequency(scheduleFrequency), 
	m_frameCounterReset(0),
	m_life(life),
	m_bAppendLevelNameToFile(bAppendLevelNameToFile)
{ 
	strcpy(m_name,""); 
	strcpy(m_fileName,""); 

	if (pName) 
		strcpy(m_name,pName); 

	if (pFileName) 
		strcpy(m_fileName,pFileName); 
		
#if __BANK	
	CreateBankToggle(); 
#endif // __BANK	
}

//---------------------------------------------------------------------------------
// Output a float to the TTY for a tracing stat
void CTracingStat::OutputTTY(	const float f, 
								const char* const pAppend, 
								const char* const pAppend2) 
{ 
	char str[255];
	sprintf(str,"%f",f);
	OutputTTY(str,pAppend,pAppend2); 	
}

//---------------------------------------------------------------------------------
// Output an s32 to the TTY for a tracing stat
void CTracingStat::OutputTTY(	const s32 i, 
								const char* const pAppend, 
								const char* const pAppend2) 
{ 
	char str[255];
	sprintf(str,"%d",i);
	OutputTTY(str,pAppend,pAppend2); 	
}

//---------------------------------------------------------------------------------
// Output a string to the TTY for a tracing stat
void CTracingStat::OutputTTY(	const char* const pVal, 
								const char* const pAppend, 
								const char* const pAppend2) 
{ 
	if (!pVal)
		return;

	char append[255] = "";	
	if (pAppend)
		strcat(append,pAppend);

	if (pAppend2)
		strcat(append,pAppend2);

	// now replace any spaces in the resultant string with underbars. ( I can't think what C function one should use that already can do this ! ) 
	int len = istrlen(append);
	for (s32 i=0;i<len;i++)
	{
		if (append[i]==' ' || append[i]==':')
		{
			append[i]='_';
		}
	}
	
	m_lastOutputTime = fwTimer::GetSystemTimeInMilliseconds();
	char levelAppend[255] ="";
	if (m_bAppendLevelNameToFile) 
	{
		s32 levelNum = CGameLogic::GetCurrentLevelIndex();
		const char *pLevelName = CScene::GetLevelsData().GetFriendlyName(levelNum);
		if (pLevelName)
		{
			sprintf(levelAppend,"_%s",pLevelName);
		}
		else
		{
			Errorf("what the fuck! no level name for level %d",levelNum);
		}
	} 

	tsDisplayf("(%d)(%dms)%s.%s=%s{%s%s}\n",m_nTimesOutput,m_lastOutputTime,m_name,append,pVal,m_fileName,levelAppend); 
	m_nTimesOutput++; 		
}

//---------------------------------------------------------------------------------
void CTracingStats::Init() 
{ 
	ProcessParams();

	if (!PARAM_TracingStats.Get())
		return;

#if __BANK
	BANKMGR.CreateOutputWindow( TRACING_STAT_BANK_NAME, TRACING_STAT_TTY_COLOUR); 

	bkBank& bank = BANKMGR.CreateBank(TRACING_STAT_BANK_NAME);
	bank.AddToggle("System Enabled", &m_bActive);
#endif // __BANK

	Register();
}

//---------------------------------------------------------------------------------
void CTracingStats::Shutdown()
{
	if (!PARAM_TracingStats.Get())
		return;

	for (s32 i=0;i<m_tracedStats.GetCount();i++)
	{
		delete m_tracedStats[i];
		m_tracedStats.Delete(i);
	}
}

//---------------------------------------------------------------------------------
void CTracingStats::Process()
{
	if (!m_bActive)
		return;

	if (!PARAM_TracingStats.Get())
		return;

	for (s32 i=0;i<m_tracedStats.GetCount();i++)
	{
		if (!m_tracedStats[i])
			continue;

		if (!m_tracedStats[i]->IsAlive())
			continue;

		if (!m_tracedStats[i]->FrameCounterUpdate())
			continue;

		if (!m_tracedStats[i]->ScheduleExpired())
			continue;

		if (!m_tracedStats[i]->IsActive())
			continue;
		
		m_tracedStats[i]->Process();
		m_tracedStats[i]->PostProcess();
	}	
}

//---------------------------------------------------------------------------------
// check params set and hook up hierarchically, maybe this functionality could exist 
// in the param stuff from RAGE?
void CTracingStats::ProcessParams()
{
	if (PARAM_TracingStatAll.Get())
	{
		PARAM_TracingStatPerformance.Set("1");
		PARAM_TracingStatMemorySizes.Set("1");
	}
}
//################################################################################
// INSERT YOUR TRACING STAT CLASS CREATION HERE
void CTracingStats::Register()
{
	m_tracedStats.PushAndGrow(rage_new CTracingStatMemorySizes	(	"Memory.Sizes",						// name
																	TS_MEM_FILE,						// file identifier																	
																	PARAM_TracingStatMemorySizes.Get(), // active?
																	true,								// Append level name to file
																	-1,									// life (-1 is immortal)
																	0,									// frequency (0 is every frame, 30 is every 30 frames)
																	3000));								// sample size ( buffered output )

	m_tracedStats.PushAndGrow(rage_new CTracingStatPerformance	(	"Performance",
																	TS_PERF_FILE,
																	PARAM_TracingStatPerformance.Get(),
																	true,								// Append level name to file
																	-1,									// life (-1 is immortal)
																	0,									// frequency (0 is every frame, 30 is every 30 frames)
																	3000));								// sample size ( buffered output )
}
//################################################################################
// INSERT YOUR TRACING STAT PROCESSES HERE

//---------------------------------------------------------------------------------
void	CTracingStatPerformance::Process( )
{ 
	float val;
	val = 1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep() );
	StoreSample(val,0,"REAL_FPS");
	//OutputTTY(m_stat[0].m_val,"REAL_FPS"); 

#if RAGE_TIMEBARS
	int nCallCount = 0;
	const int NUM_TIMING_BARS = 33;
	static const char *str[NUM_TIMING_BARS] = {	"render",				// 1
									"World Process",		// 2
									"Physics",				// 3
									"Update",				// 4
									"Streaming",			// 5
									"Game Update",			// 6
									"GameLogic",			// 7 
									"Script",				// 8
									"Render Update",		// 9 
									"WarpShadow",			// 10
									"Occlusion",			// 11
									"Water Processing",		// 12
									"PostFx",				// 13
									"BuildRenderList",		// 14
									"Water block",			// 15
									"PreRender",			// 16
									"MegaHistory Update",	// 17
									"NoteNodes Update",		// 18
									"Sync Tasks",			// 19
									"Object pop process",	// 20
									"Navmesh Ped Tracking",	// 21
									"CPed::ProcessControl",	// 22
									"PlantsMgr::Render",	// 23
									"Animation",			// 24
									"Ped ai",				// 25
									"CarCtrl",				// 26
									"Scene update",			// 27
									"ConstructRenderList",	// 28
									"Render Scene",			// 29
									"Lights",				// 30
									"Render Entities",		// 31
									"CTaskManager::Process",// 32
									"Veh Pop Process"		// 33
								};
	for (s32 bucket=0;bucket<NUM_TIMING_BARS;bucket++)
	{		
		g_pfTB.GetFunctionTotals( str[bucket], nCallCount, val );
		StoreSample(val,bucket+1,str[bucket]);
		//OutputTTY(m_stat[bucket+1].m_val,str[bucket]); 
	}
#endif // RAGE_TIMEBARS
}

//---------------------------------------------------------------------------------
void	CTracingStatMemorySizes::Process( )
{ 
	const rage::eMemoryType allocators[MAX_ALLOCATORS] = {	MEMTYPE_GAME_VIRTUAL, 
															MEMTYPE_GAME_PHYSICAL, 
															MEMTYPE_RESOURCE_VIRTUAL, 
															MEMTYPE_RESOURCE_PHYSICAL }; 
	
	const char allocStr[MAX_ALLOCATORS][30]				=  {"GAME_VIRTUAL", 
															"GAME_PHYSICAL", 
															"RESOURCE_VIRTUAL", 																										
															"RESOURCE_PHYSICAL" };

	const int buckets[NUM_MEM_BUCKETS] = {	MEMBUCKET_DEFAULT,
											MEMBUCKET_ANIMATION,
											MEMBUCKET_STREAMING,
											MEMBUCKET_WORLD,
											MEMBUCKET_GAMEPLAY,
											MEMBUCKET_FX,
											MEMBUCKET_RENDER,
											MEMBUCKET_PHYSICS,
											MEMBUCKET_AUDIO,
											MEMBUCKET_NETWORK,
											MEMBUCKET_SYSTEM,
											MEMBUCKET_SCALEFORM,
											MEMBUCKET_SCRIPT,
											MEMBUCKET_RESOURCE,
											MEMBUCKET_DEBUG,
										};

	char concat[255];
	int id = 0;

	// Memory buckets
	for (s32 b=0;b<NUM_MEM_BUCKETS;b++)
	{
		MemoryBucketIds bucket = (MemoryBucketIds)b;
		for (s32 a=0;a<MAX_ALLOCATORS;a++)
		{			
			
			sprintf(concat,"%s.%s.%s",allocStr[a],sysMemGetBucketName(bucket),"Used");
			StoreSample((s32)(sysMemAllocator::GetMaster().GetAllocator(a)->GetMemoryUsed(buckets[bucket])/MB),id,concat);
			id++;
		}
	}

	// Heap stats
	for (s32 i=0;i<MAX_ALLOCATORS;i++)
	{
		sprintf(concat,"%s.%s",allocStr[i],"LargestAvailableBlock");
		StoreSample((s32)(sysMemAllocator::GetCurrent().GetAllocator(allocators[i])->GetLargestAvailableBlock()/MB),id,concat);
		id++;

		sprintf(concat,"%s.%s",allocStr[i],"Available");
		StoreSample((s32)(sysMemAllocator::GetCurrent().GetAllocator(allocators[i])->GetMemoryAvailable()/MB),id,concat);
		id++;

		sprintf(concat,"%s.%s",allocStr[i],"Used");
		StoreSample((s32)(sysMemAllocator::GetCurrent().GetAllocator(allocators[i])->GetMemoryUsed()/MB),id,concat);
		id++;
	}
}

//=========================================================================================
// The inits and shutdowns of tracing stat classes need stubbed out even if they do nothing
// this enables the easy macro for class definition to work effectively.
void	CTracingStatPerformance::Init( )		{ }
void	CTracingStatMemorySizes::Init( )		{ }

//=================================================================================
void	CTracingStatPerformance::Shutdown( )	{ }
void	CTracingStatMemorySizes::Shutdown( )	{ }

//=================================================================================

//---------------------------------------------------------------------------------

#endif //TRACING_STATS
