#if !__FINAL

// Framework hdrs
#include "fwscene/world/WorldLimits.h"

// Rage hdrs
#include "bank/button.h"
#include "diag/output.h"
#include "grcore/image.h"
#include "grmodel/setup.h"
#include "grprofile/timebars.h"
// Game hdrs
#include "Camera/camInterface.h"
#include "Camera/debug/debugdirector.h"
#include "Camera/helpers/frame.h"
#include "Control/GameLogic.h"
#include "Core/Game.h"
#include "cutscene/CutSceneManagerNew.h"
#include "frontend/MiniMap.h"
#include "Game/clock.h"
#include "Game/weather.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPopulation.h"
#include "Peds/Ped.h"
#include "peds/rendering/pedvariationDebug.h"
#include "Physics/gtaInst.h"
#include "scene/scene.h"
#include "scene/animatedbuilding.h"
#include "scene/WarpManager.h"
#include "SectorTools.h"
#include "streaming/streaming.h"		// For CStreaming::LoadAllRequestedObjects(), etc.
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingmodulemgr.h"
#include "streaming/streamingdebug.h"
#include "text/TextConversion.h"
#include "Tools/smokeTest.h"
#include "Tools/TracingStats.h"
#include "Tools/ObjectProfiler.h"
#include "Vehicles/VehicleFactory.h"
#include "Weapons/Explosion.h"
#include "system/autogpucapture.h"


// DON'T COMMIT
//OPTIMISATIONS_OFF()


RAGE_DEFINE_CHANNEL(AssetTest)

BANK_ONLY(extern void UpdateAvailableCarsCB();)
extern void CreateNextCar(bool bOnlyIfTheresRoom);

namespace SmokeTests
{

// function prototypes
void InitRageBuilderUnitTest();
void ProcessRageBuilderUnitTest();
void InitBuildState();
void ProcessBuildState();
void InitWanderingTest();
void ProcessWanderingTest();
void InitWarpingTest();
void ProcessWarpingTest();
void ProcessWarping(const int iWarpPeriod);
void InitExplosionTest();
void ProcessExplosionTest();
void ProcessCutsceneTest();
void InitLoadVehicles();
void InitLoadPeds();
void ProcessRestartTest();
void ProcessLevelLoad();
void ProcessLevelReload();
void InitShutdownTest();
void ProcessMemoryTest();
void InitPedPics();
void ProcessPedPics();
void InitCodebuilderStats();
void ProcessCodebuilderStats();
void CaptureCodebuilderStatsState();
void InitJunctionMemStats();
void ProcessJunctionMemStats();
void InitObjectProfiler();
void ProcessObjectProfiler();

PARAM(smoketestwantedlevel, "Set wanted level when initializing the smoketest." );
PARAM(smoketest, "Set up a stress-test situation to see if the game survives!  Use with a value (smoketest=n|name) or on its own to select the first." );
PARAM(smokeDuration, "Duration of smoke test to run ");
PARAM(automatedtest, "Tell us that an automated test is running");
PARAM(warpingInterval, "Set up time between warps for smoketest=warping (default 60 seconds)");

#if __BANK // ped pics params
PARAM(pedpiccampos,						"[debug] ped pics : camera position");
PARAM(pedpicdisttocrowd,				"[debug] ped pics : dist to ped crowd created");
PARAM(pedpiccrowdSize,					"[debug] ped pics : crowd dimensions");
PARAM(pedpicgroupuniformdistribution,	"[debug] ped pics : density / distribution of crowd.");
PARAM(pedpicstartidx,					"[debug] ped pics : start index to capture.");
PARAM(pedpicmaxcaptures,				"[debug] ped pics : max picture captures to try, -1 is unlimited.");
PARAM(pedpicheading,					"[debug] ped pics : heading of camera");
PARAM(pedpicpitch,						"[debug] ped pics : pitch of camera");
PARAM(pedpicroll,						"[debug] ped pics : roll of camera");
PARAM(pedpichour,						"[debug] ped pics : hour of day");
PARAM(pedpicminute,						"[debug] ped pics : minute of day");
PARAM(pedpicsecond,						"[debug] ped pics : second of day");
PARAM(pedpicfov,						"[debug] ped pics : field fo view");
#endif // __BANK

SmokeTestDef g_SmokeTests[] =
{
	// name					// init function			// update function			// description
	{"wandering",			&InitWanderingTest,			&ProcessWanderingTest,		"The player is set to wander about the world invincible with a high wanted level."},
	{"warping",				&InitWarpingTest,			&ProcessWarpingTest,		"Player is teleported around random areas of the world and set to wander at each location with a high wanted level for a specified time. Any errors are reported."},
	{"explosion",			&InitExplosionTest,			&ProcessExplosionTest,		"Player wanders about with explosions going on about the player every second, cars get created periodically."},
	{"restart",				NULL,						&ProcessRestartTest,		"A level ( the same one ) is reloaded many times so to determine memory leaks and errors upon reloading itself."},
	{"cutscene",			NULL,						&ProcessCutsceneTest,		"Plays each cutscene"},
	{"loadvehicles",		&InitLoadVehicles,			NULL,						"For all vehicles... Stream the vehicles in. Instance them. Destroy instance. Remove vehicles and their dependencies from streaming. Record memory heaps before and after. "},
	{"loadpeds",			&InitLoadPeds,				NULL,						"For all peds... Stream the peds in. Instance them. Destroy instance. Remove peds and their dependencies from streaming. Record memory heaps before and after."},
	{"levelload",			NULL,						&ProcessLevelLoad,			"Cycles through various specified levels for a specified time. Memory usage is displayed between each level to determine if there are memory leaks."},
	{"levelreload",			NULL,						&ProcessLevelReload,	    "Reloads the default level. Memory usage is displayed between each load to determine if there are memory leaks."},
	{"shutdown",			&InitShutdownTest,			NULL,						"The level is shutdown, any errors are reported."},
	{"memory",				NULL,						&ProcessMemoryTest,			"Display memory usage periodically."},
	{"validatescripts",		NULL,						NULL,						"Loads all .sco files to cause them to be checked for unrecognised native commands by scrThread::Validate inside scrProgram::Load"},
	{"pedpics",				&InitPedPics,				&ProcessPedPics,			"Takes pictures of groups of peds in different variations."},
	{"buildstate",			&InitBuildState,			&ProcessBuildState,			"Prints 'build state' to tty in a standard parsable form."},
	{"ragebuilder_unittest",&InitRageBuilderUnitTest,	&ProcessRageBuilderUnitTest,"performs the tests required for checking if assets converted with latest ragebuilder executable are sound."},
	{"codebuilder_stats",	&InitCodebuilderStats,		&ProcessCodebuilderStats,	"performs statistic gathering after a codebuilder has made a successful build"},
	{"junction_mem_stats",	&InitJunctionMemStats,		&ProcessJunctionMemStats,	"runs the sector tools as a smoketest gathering streaming memory stats captured at junctions"},
	{"object_profile",		&InitObjectProfiler,		&ProcessObjectProfiler,		"displays list of objects consecutively, rotating 360 degree's and capturing GPU timing data"},
	// ensure last entry has a NULL name
	{NULL,				NULL,					NULL,					""}
};

bool s_terminateSmokeTest		= false;
float g_smokeTestDuration		=	-1.0f;
s32 g_iTimeBeforeStarting		=	10000;
SmokeTestDef* g_pSmokeTestDef	=	NULL;
bool g_bHasParam				=	true;
Vector3 g_pedPicCamStartPos		= 	Vector3(-1362.0f,-1121.0f,4.75f);
int g_automatedSmokeTest		= -1;
int g_defaultWantedLevel		= 4;

SmokeTestDef* GetSmokeTestDef(const char* const pStr)
{
	SmokeTestDef* pDef = &g_SmokeTests[0];

	// search through smoke test list
	while(pDef->pName != NULL)
	{
		if(!stricmp(pDef->pName, pStr))
		{
			return pDef;
		}
		pDef++;
	}

	return NULL;
}

void DisplaySmokeTestStarted(const char* const pStr)
{
	char commandLine[1024];
	strcpy(commandLine,"");
	for (s32 i=0;i<sysParam::GetArgCount();i++)
	{
		safecat(commandLine,sysParam::GetArg(i));
		safecat(commandLine," ");
	}

	atDisplayf("Smoke Test %s started Duration %d CommandLine %s",pStr,(int)g_smokeTestDuration, commandLine );
	atDisplayf("Smoke Test Description : %s", g_pSmokeTestDef->pDesc);
}

void PrintSkeletonData()
{
	Displayf("\nSKELETON DATA");
	CVehicle::PrintSkeletonData();
	CPed::PrintSkeletonData();
	CObject::PrintSkeletonData();
	CAnimatedBuilding::PrintSkeletonData();
}

void DisplaySmokeTestFinished()
{
	PrintSkeletonData();

	atDisplayf("Smoke Test finished");
}


//
// name:		CSmokeTests::ProcessSmokeTest
// description:
void ProcessSmokeTest()
{
	PF_START_TIMEBAR_DETAIL("process smoketest");
	if(!g_bHasParam)
		return;

	const char* pSmokeTestName = NULL;

	if (PARAM_automatedtest.Get())
	{
		g_automatedSmokeTest = 0;
		int smoketest = 0;
		if (PARAM_automatedtest.Get(smoketest))
			g_automatedSmokeTest = smoketest;
	}

	if (!PARAM_smoketest.Get(pSmokeTestName))
	{
		return;
	}

	g_automatedSmokeTest = 0;

	// wait 10 seconds before starting smoke tests
	if(g_iTimeBeforeStarting > 0)
	{
		g_iTimeBeforeStarting -= fwTimer::GetTimeStepInMilliseconds();
		DEBUG_DRAW_ONLY(grcDebugDraw::AddDebugOutput( Color32(1.0f, 1.0f, 0.0f), "SmokeTest %s is about to start.", pSmokeTestName));
		return;
	}

	// if no definition setup then get command line parameter and search through smoke test list
	// for related smoke test
	if(g_pSmokeTestDef == NULL)
	{
		// get smoke test duration value
		PARAM_smokeDuration.Get(g_smokeTestDuration);

		const char* pStr = NULL;
		if(PARAM_smoketest.Get(pStr))
		{
			g_pSmokeTestDef = GetSmokeTestDef(pStr);

			// if smoke test name wasn't found
			if(g_pSmokeTestDef == NULL)
			{
				atDisplayf("Unrecognised smoke tests %s", pStr);
				g_bHasParam = false;
				return;
			}

			if (g_pSmokeTestDef->InitFn || g_pSmokeTestDef->UpdateFn)
			{
				DisplaySmokeTestStarted(pStr);
			}

			// call initialisation function
			if(g_pSmokeTestDef->InitFn != NULL)
				g_pSmokeTestDef->InitFn();
		}
		else
		{
			// no smoke test parameter
			g_bHasParam = false;
		}
	}
	else
	{
		DEBUG_DRAW_ONLY(grcDebugDraw::AddDebugOutput( Color32(1.0f, 1.0f, 0.0f), "SmokeTest Running : %s", pSmokeTestName));
		DEBUG_DRAW_ONLY(grcDebugDraw::AddDebugOutput( Color32(1.0f, 1.0f, 0.0f), "SmokeTest Description : %s", g_pSmokeTestDef->pDesc));

		if(g_pSmokeTestDef->UpdateFn != NULL)
		{
			g_pSmokeTestDef->UpdateFn();
		}
	}

	// Sanity check the heap every frame during smoke tests.
	// This causes contention with path finding so just run with -sanitycheckupdate instead
	// if you really want it.
	// sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_GAME_VIRTUAL)->SanityCheck();
}

//
// name:		IsSmokeTestFinished
// description:	Return if smoke tests have finished
bool HasSmokeTestFinished()
{
	static rage::sysTimer timer;
	bool bWantToExit = false;

	if ( g_smokeTestDuration != -1 || s_terminateSmokeTest )
	{
		bWantToExit = ( g_smokeTestDuration > 0 && g_smokeTestDuration < timer.GetTime() );
		DEBUG_DRAW_ONLY(grcDebugDraw::AddDebugOutput( Color32(1.0f, 1.0f, 0.0f), "SmokeTest duration %d/%d.", g_smokeTestDuration, timer.GetTime()));
		if ( bWantToExit || s_terminateSmokeTest)
		{
			if (bWantToExit)
			{
				Errorf("Smoketest being terminated because 'smoketestduration' has elapsed (%2.3f) - bug Derek Ward", g_smokeTestDuration);	
			}
		
			const char* pSmokeTestName = NULL;
			PARAM_smoketest.Get(pSmokeTestName);
			// shutdown is a special case smoketest where we care about the
			// results of the shutdown
			if(pSmokeTestName == NULL || stricmp(pSmokeTestName, "shutdown"))
			{
				// note can't use displayf as could get filtered out
				if (g_pSmokeTestDef->InitFn || g_pSmokeTestDef->UpdateFn)
				{
					DisplaySmokeTestFinished();
					printf("***  TESTER IGNORE REST OF ERRORS ***");
				}
			}
		}
		DEBUG_DRAW_ONLY(grcDebugDraw::AddDebugOutput( Color32(1.0f, 1.0f, 0.0f), "Smoke Duration : %d seconds.", g_smokeTestDuration));
	}
	else
	{
		const char* pSmokeTestName = NULL;
		if (PARAM_smoketest.Get(pSmokeTestName))
		{
			DEBUG_DRAW_ONLY(grcDebugDraw::AddDebugOutput( Color32(1.0f, 1.0f, 0.0f), "Smoke Duration : controlled externally"));
		}
	}
	return bWantToExit || s_terminateSmokeTest;
}

void FinishSmokeTest()
{
	s_terminateSmokeTest = true;
}

//********************************************************
//	"smoke-tests" which can be run from the command line

void DisplayMemoryUsage(const char* const pStrFilter = NULL,
									 const bool bDisplayBuckets = true,
									 const bool bDisplayHeapsAvailable = true,
									 const bool bDisplayHeapsLargestAvailableBlock = true)
{
	const rage::eMemoryType allocators[MAX_ALLOCATORS] = {	MEMTYPE_GAME_VIRTUAL,
															MEMTYPE_GAME_PHYSICAL,
															MEMTYPE_RESOURCE_VIRTUAL,
															MEMTYPE_RESOURCE_PHYSICAL };

	const char allocStr[MAX_ALLOCATORS][30]				=  {"GAME_VIRTUAL",
															"GAME_PHYSICAL",
															"RESOURCE_VIRTUAL",
															"RESOURCE_PHYSICAL" };

	const char allocStrShort[MAX_ALLOCATORS][5]			=  {"GV",
															"GP",
															"RV",
															"RP" };


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

	char concat[1024],temp[1024];

	if (bDisplayBuckets)
	{
		atDisplayf("Used Memory in Buckets.");
		// Memory buckets
		for (s32 b=0;b<NUM_MEM_BUCKETS;b++)
		{
			float val[MAX_ALLOCATORS];
			MemoryBucketIds bucket = (MemoryBucketIds)b;
			sprintf(concat,"%s",sysMemGetBucketName(bucket));

			if (!pStrFilter || !strcmp(pStrFilter,sysMemGetBucketName(bucket)))
			{
				for (s32 a=0;a<MAX_ALLOCATORS;a++)
				{
					val[a] = (float)sysMemAllocator::GetMaster().GetAllocator(a)->GetMemoryUsed(buckets[b])/(float)MB;
					sprintf(temp,"\t%s %7.3fMB",allocStrShort[a],val[a]);
					safecat(concat,temp);
				}
			}

			atDisplayf("%s", concat);
		}
		atDisplayf(" ");
	}

	{
		// Heap stats
		if (bDisplayHeapsLargestAvailableBlock)
		{
			atDisplayf("Largest Available Block.");
			for (s32 i=0;i<MAX_ALLOCATORS;i++)
			{
				if (!pStrFilter || !strcmp(pStrFilter,allocStr[i]))
				{
					sprintf(concat,"%s.%s",allocStr[i],"LargestAvailableBlock");
					float val = (float)sysMemAllocator::GetCurrent().GetAllocator(allocators[i])->GetLargestAvailableBlock()/(float)MB;
					if (val>0.0f)
					{
						atDisplayf("%s = \t%7.3fMB ",concat,val);
					}
				}
			}
			atDisplayf(" ");
		}


		if (bDisplayHeapsAvailable)
		{
			atDisplayf("Available.");
			for (s32 i=0;i<MAX_ALLOCATORS;i++)
			{
				if (!pStrFilter || !strcmp(pStrFilter,allocStr[i]))
				{
					sprintf(concat,"%s.%s",allocStr[i],"Available");
					float val = (float)sysMemAllocator::GetCurrent().GetAllocator(allocators[i])->GetMemoryAvailable()/(float)MB;
					if (val>0.0f)
					{
						atDisplayf("%s = \t%7.3fMB ",concat,val);
					}
				}
			}
		}
	}
}

void ProcessMemoryTest()
{
	static u32 timeToDisplayMemory = 5000;
	static u32 delayToDisplayMemory = 60000;
	u32 t = fwTimer::GetTimeInMilliseconds();

	if(t > timeToDisplayMemory)
	{
		atDisplayf("Memory Report at time %d seconds",t/1000);
		DisplayMemoryUsage();
		timeToDisplayMemory = t + delayToDisplayMemory;
	}
}

static void DisplayMemoryAvailable()
{
#if __PS3 || RSG_PC
	atDisplayf("Memory available GAME_VIRTUAL %dK  RESOURCE_VIRTUAL %dK  GAME_PHYSICAL %dK  RESOURCE_PHYSICAL %dK",
		sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryAvailable() / 1024,
		sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryAvailable() / 1024,
		sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_GAME_PHYSICAL)->GetMemoryAvailable() / 1024,
		sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryAvailable() / 1024);
#else
	atDisplayf("Memory available GAME_VIRTUAL %" SIZETFMT "uK RESOURCE_VIRTUAL %" SIZETFMT "uK",
		sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryAvailable() / 1024,
		sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryAvailable() / 1024);
#endif
}

//
// name:		ProcessLevelLoad
// description:	Check for memory leaks while cycling through the levels
void ProcessLevelLoad()
{
	static u32 timeOfNextLevelLoad = 5000;
	static u32 delayOfNextLevelLoad  = 50000;
	u32 t = fwTimer::GetTimeInMilliseconds();
	#define MAX_LEVELS_CYCLE 2
	static u32 levels[MAX_LEVELS_CYCLE] = {1,2};
	static u32 levelNum = 0;

	if(t > timeOfNextLevelLoad)
	{
        PF_BEGIN_STARTUPBAR();

		levelNum++;
		levelNum%=MAX_LEVELS_CYCLE;
		atDisplayf("Smoke Test level %s", CScene::GetLevelsData().GetFriendlyName(levels[levelNum]));

		DisplayMemoryAvailable();

        atDisplayf("Restarting level %s", CScene::GetLevelsData().GetFriendlyName(levels[levelNum]));
        CGame::ChangeLevel(levels[levelNum]);
		timeOfNextLevelLoad = t + delayOfNextLevelLoad;
	}
}

//
// name:		ProcessLevelLoad
// description:	Check for memory leaks while cycling through the levels
void ProcessLevelReload()
{
	static u32 timeOfNextLevelLoad = 5000;
	static u32 delayOfNextLevelLoad  = 50000;
	u32 t = fwTimer::GetTimeInMilliseconds();

	if(t > timeOfNextLevelLoad)
	{
        PF_BEGIN_STARTUPBAR();

		atDisplayf("Smoke Test level %s", CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetRequestedLevelIndex()));

		DisplayMemoryAvailable();

        atDisplayf("Restarting level %s", CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetRequestedLevelIndex()));
		CGameSessionStateMachine::SetForceInitLevel();
		CGame::StartNewGame(CGameLogic::GetRequestedLevelIndex());
		timeOfNextLevelLoad = t + delayOfNextLevelLoad;
	}
}

#if __BANK
static char fn[255] = "";
#endif // #if __BANK

//
// name:		InitPedPics
// description:	take pictures of groups of peds.
void InitPedPics()
{
#if __BANK
//	DEBUG_DRAW_ONLY(grcDebugDraw::SetDisplayDebugText(true));
	CMiniMap::SetDisplayGameMiniMap(false);
	float pos[3];
	if(PARAM_pedpiccampos.GetArray(pos, 3))
	{
		g_pedPicCamStartPos = Vector3(pos[0], pos[1], pos[2]);
	}
#endif // #if __BANK
}

//
// name:		ProcessPedPics
// description:	take pictures of groups of peds.
void ProcessPedPics()
{
#if __BANK
	const s32		frameTickerDefault		= 5;									// frames to wait before picture is taken after being paused ( gets rid of motion blur )
	const char		pedPicsPath[MAX_PATH]	= "X:\\pedpics";						// where to dump pics
	const float		textSize				= 2.5f;									// size/scale of text on screen
	const Vector2	textPos					= Vector2(0.05f,0.05f);					// where to print text on screen
	const Color32	textColour				= Color32(1.0f, 0.0f, 0.0f);			// colour of text on screen
	const bool		textBackground			= true;									// draw a background behind the text
	const u32		delayTillPic			= 500;									// delay after loading peds till their piccy is taken
	const bool		createExactGroupSize	= false;								// something to do with the crowd creation.
	const bool		wanderAfterCreate		= true;									// peds wander about after being created.
	const u32		t						= fwTimer::GetTimeInMilliseconds();
	const u32		numPedNames				= CPedVariationDebug::GetNumPedNames();

	static s32		numCaptured				= 0;
	static u32		timeLoaded				= 0;
	static u32		pedPicId				= 0;
	static bool		load					= true;
	static s32		frameTicker				= frameTickerDefault;
	static s32		maxCaptures				= -1;

	static Vector3	startPos				= g_pedPicCamStartPos;					// where the camera is positioned.
	static u32		crowdSize				= 3;									// crowd size ( fickle )
	static float	distToCrowd				= 6.0f;									// distance crowd is created form the camera
	static float	groupUniformDistribution= 1.5f;									// spacing between peds ( -1 for old method - can't understand it though )
	static float	heading					= 0.0f;
	static float	pitch					= 0.0f;
	static float	roll					= 0.0f;
	static float	fov						= 60.0f;
	static s32	hour						= 12;
	static s32	minute						= 0;
	static s32	second						= 0;


	if (maxCaptures>=0 && numCaptured>=maxCaptures)
	{
		return;
	}

	if (numCaptured==0)
	{
		// Tweakables
		float fVal;
		int iVal;

		if (CPed::ms_pCreateButton)
		{
			CPed::ms_pCreateButton->Activate();
			return;
		}

		if (PARAM_pedpicstartidx.Get(iVal))
		{
			pedPicId = iVal;
		}

		if (PARAM_pedpicmaxcaptures.Get(iVal))
		{
			maxCaptures = iVal;
		}

		if(PARAM_pedpicdisttocrowd.Get(fVal))
		{
			distToCrowd = fVal;
		}

		if(PARAM_pedpicgroupuniformdistribution.Get(fVal))
		{
			groupUniformDistribution = fVal;
		}

		if(PARAM_pedpiccrowdSize.Get(iVal))
		{
			crowdSize = iVal;
		}

		if(PARAM_pedpicheading.Get(fVal))
		{
			heading = fVal;
		}

		if(PARAM_pedpicpitch.Get(fVal))
		{
			pitch = fVal;
		}

		if(PARAM_pedpicroll.Get(fVal))
		{
			roll = fVal;
		}

		if(PARAM_pedpichour.Get(iVal))
		{
			hour = iVal;
		}

		if(PARAM_pedpicminute.Get(iVal))
		{
			minute = iVal;
		}

		if(PARAM_pedpicsecond.Get(iVal))
		{
			second = iVal;
		}

		if(PARAM_pedpicfov.Get(fVal))
		{
			fov = fVal;
		}

	}

	// Load in peds.
	if (load)
	{
		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		debugDirector.ActivateFreeCam();
		camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();
		freeCamFrame.SetWorldMatrixFromHeadingPitchAndRoll( heading, pitch, roll );
		freeCamFrame.SetPosition(startPos);
		freeCamFrame.SetFov(fov);
		CClock::SetTime(hour, minute, second);
		g_weather.ForceTypeClear();

		CPedVariationDebug::SetDistToCreateCrowdFromCamera(distToCrowd);
		CPedVariationDebug::SetCrowdSize(crowdSize);
		CPedVariationDebug::SetCreateExactGroupSize(createExactGroupSize);
		CPedVariationDebug::SetWanderAfterCreate(wanderAfterCreate);
		CPedVariationDebug::SetCrowdSpawnLocation(freeCamFrame.GetPosition()+(freeCamFrame.GetFront()*distToCrowd));
		CPedVariationDebug::SetCrowdUniformDistribution(groupUniformDistribution);

		if (pedPicId < numPedNames)
		{
			pedPicId++;
			CPedVariationDebug::SetCrowdPedSelection(0,pedPicId);
			if (!strstr(CPedVariationDebug::pedNames[pedPicId],"CS_") &&
				!strstr(CPedVariationDebug::pedNames[pedPicId],"cs_") &&
				!strstr(CPedVariationDebug::pedNames[pedPicId],"slod_human") &&
				!strstr(CPedVariationDebug::pedNames[pedPicId],"slod_small_quadped") &&
				!strstr(CPedVariationDebug::pedNames[pedPicId],"slod_large_quadped") &&
				!strstr(CPedVariationDebug::pedNames[pedPicId],"singlemeshman"))
			{
				createCrowdCB();
				createCrowdCB(); // deliberately called twice
			}
			timeLoaded = t;
		}
		else
		{
			FinishSmokeTest();
			return;
		}
		load = false;
	}

	sprintf(fn,"PedPic [%d of %d] %s",pedPicId,numPedNames,CPedVariationDebug::pedNames[pedPicId]);

	if(!load && t > timeLoaded + delayTillPic)
	{
		fwTimer::StartUserPause( );
		frameTicker--;

		if (frameTicker<=0)
		{
			atDisplayf("PED ON DISPLAY IS %s",fn);

			// Take piccy
			rage::grmSetup* pSetup = CSystem::GetRageSetup();
			if (pSetup)
			{
				char path[MAX_PATH],fullpath[MAX_PATH];
				const fiDevice* pDevice = fiDevice::GetDevice( path, false );
				Assertf( pDevice, "Failed to get device" );

				formatf( path, MAX_PATH, pedPicsPath );
				pDevice->MakeDirectory( path );
				sprintf(fullpath,"%s\\%s",path,fn);

#if __PS3 && HACK_GTA4
				grcImage::SetUseGammaCorrection(false);
#endif
				pSetup->SetScreenShotNamingConvention( grcSetup::OVERWRITE_SCREENSHOT );
				pSetup->TakeScreenShot( fullpath, true );
				numCaptured++;

				// resume the game - no pause
				fwTimer::EndUserPause( );
			}

			// Destroy peds we created
			CPedPopulation::RemoveAllRandomPeds();
			fwPool<CPed> *pedPool = CPed::GetPool();
			for(s32 i = 0; i < pedPool->GetSize(); i++)
			{
				CPed *ped = pedPool->GetSlot(i);
				if(ped && !ped->IsPlayer())
				{
					CPedFactory::GetFactory()->DestroyPed(ped);
					ped = NULL;
				}
			}

			// schedule next pic.
			load = true;
			frameTicker = frameTickerDefault;
		}
	}

	DEBUG_DRAW_ONLY(grcDebugDraw::Text(textPos,textColour, fn, textBackground,textSize,textSize));

#endif
}

//
// name:		RemoveObjectAndDependencies
// description:	Remove a streamed object and all of its dependencies
// parameters:	index is the streaming index
void RemoveObjectAndDependencies(strIndex index)
{
	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
	strLocalIndex objIndex = pModule->GetObjectIndex(index);

	// if we can delete this (flags not set, reference count is zero)
	if(!(strStreamingEngine::GetInfo().GetObjectFlags(index) & STR_DONTDELETE_MASK) &&
		pModule->GetNumRefs(objIndex) == 0)
	{
		pModule->StreamingRemove(objIndex);
	}

	strIndex deps[STREAMING_MAX_DEPENDENCIES];
	s32 numDeps = pModule->GetDependencies(objIndex, &deps[0], STREAMING_MAX_DEPENDENCIES);
	while(numDeps--)
	{
		RemoveObjectAndDependencies(deps[numDeps]);
	}

}

//
// name:		LoadAllPedModelInfos
// description:	Load all peds and create an instance of each
template<class T> void LoadAllFromStaticStore(fwArchetypeDynamicFactory<T>& store, void (*InstanceFn)(u32 index))
{
	// clear out streaming request list so there are no entries to pollute the results
	CStreaming::LoadAllRequestedObjects();

	atArray<T*> typeArray;
	store.GatherPtrs(typeArray);

	for(s32 i=0; i<typeArray.GetCount(); i++)
	{
		//T& entry = store.GetEntry(i);
		T& entry = *typeArray[i];
		fwModelId modelId =	CModelInfo::GetModelIdFromName(entry.GetModelName());
		Assert(modelId.IsValid());

		if(!strcmp("player", entry.GetModelName()))
			continue;

		atDisplayf("Loading %s", entry.GetModelName());
		if(!modelId.IsValid())
		{
			atDisplayf("%s is not in any img file", entry.GetModelName());
			continue;
		}
		// If ped is already in memory don't stream it
		bool bInMemory = CModelInfo::HaveAssetsLoaded(modelId);

		if(!bInMemory)
		{
			// load and remove object once to check if memory leak occurs with every load of the object
			CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD);
			// remove any requests in the streaming list
			CStreaming::LoadAllRequestedObjects();
			// instance object
			InstanceFn(modelId.GetModelIndex());
			// ensure object and all its dependencies aren't in memory
			strLocalIndex transientLocalIndex = CModelInfo::LookupLocalIndex(modelId);
			RemoveObjectAndDependencies(strStreamingEngine::GetInfo().GetModuleMgr().GetModule(CModelInfo::GetStreamingModuleId())->GetStreamingIndex(transientLocalIndex));
		}

		// store memory before loading
		size_t gameVirtual = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed();
		size_t rscVirtual = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed();

		// stream ped model
		if(!bInMemory)
		{
			// request and load object
			CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD);
			CStreaming::LoadAllRequestedObjects();
		}

		// store memory before instancing
		size_t gameVirtual1 = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed();
		size_t rscVirtual1 = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed();

		// instance object
		InstanceFn(modelId.GetModelIndex());

		// compare memory after deleting instance
		size_t gameVirtual2 = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed();//rage/jimmy/dev/rage/base/src/paging/streamer.cpp
		size_t rscVirtual2 = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed();

		if(gameVirtual1 != gameVirtual2 || rscVirtual1 != rscVirtual2)
			atDisplayf("Memory leak (Game Virtual %" SIZETFMT "d) (Resource Virtual %" SIZETFMT "d) when instancing %s", gameVirtual2 - gameVirtual1, rscVirtual2 - rscVirtual1, entry.GetModelName());

		// remove ped model
		// If ped was already in memory don't remove it
		if(!bInMemory){
			strLocalIndex transientLocalIndex = CModelInfo::LookupLocalIndex(modelId);
			RemoveObjectAndDependencies(strStreamingEngine::GetInfo().GetModuleMgr().GetModule(CModelInfo::GetStreamingModuleId())->GetStreamingIndex(transientLocalIndex));
		}

		// compare memory after deleting model
		gameVirtual2 = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed();
		rscVirtual2 = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed();

		if(gameVirtual != gameVirtual2 || rscVirtual != rscVirtual2)
			atDisplayf("Memory leak (Game Virtual %" SIZETFMT "d) (Resource Virtual %" SIZETFMT "d) when loading %s", gameVirtual2 - gameVirtual, rscVirtual2 - rscVirtual, entry.GetModelName());

	}

	// Exit
	FinishSmokeTest();
}


static void InstancePed(u32 index)
{
	Matrix34 identity(Matrix34::IdentityType);
	// instance and delete ped
	fwModelId modelId((strLocalIndex(index)));
	CPed* pPed = CPedFactory::GetFactory()->CreatePed(CControlledByInfo(false, false), modelId, &identity, false, true, false);
	CPedFactory::GetFactory()->DestroyPed(pPed);
}

static void InstanceVehicle(u32 index)
{
	Matrix34 identity(Matrix34::IdentityType);
	// instance and delete vehicle
	fwModelId modelId((strLocalIndex(index)));
	CVehicle* pVehicle = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_OTHER, POPTYPE_RANDOM_AMBIENT, &identity);
	CVehicleFactory::GetFactory()->Destroy(pVehicle);
}

//
// name:		InitLoadVehicles
// description:	Check for problems while loading and instancing all the peds
void InitLoadPeds()
{
	LoadAllFromStaticStore<CPedModelInfo>(CModelInfo::GetPedModelInfoStore(), &InstancePed);
}

//
// name:		InitLoadVehicles
// description:	Check for problems while loading and instancing all the vehicles
void InitLoadVehicles()
{
	LoadAllFromStaticStore<CVehicleModelInfo>(CModelInfo::GetVehicleModelInfoStore(), &InstanceVehicle);
}

//
// name:		InitWanderingTest
// description:	Check for crashes while wandering about the world
void InitWanderingTest()
{

	CPed * pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		pPlayer->GetPedIntelligence()->ClearTasks();
		CTask * pTaskWander = pPlayer->ComputeWanderTask(*pPlayer);
		pPlayer->GetPedIntelligence()->AddTaskDefault(pTaskWander);

		CPlayerInfo::ms_bDebugPlayerInvincible = true;

		CWanted * pWanted = pPlayer->GetPlayerWanted();
		int wantedLevel = g_defaultWantedLevel;
		PARAM_smoketestwantedlevel.Get(wantedLevel);
		pWanted->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), wantedLevel, 0, WL_FROM_TEST);
	}
}

void ProcessWanderingTest()
{
	int interval = (60*5) * 1000;	// 5 mins in ms
	ProcessWarping(interval);
}


//
// name:		InitWarpingTest
// description:	Check for crashes while warping the player about the world
void InitWarpingTest()
{
	CPed * pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		pPlayer->GetPedIntelligence()->ClearTasks();
		CTask * pTaskWander = pPlayer->ComputeWanderTask(*pPlayer);
		pPlayer->GetPedIntelligence()->AddTaskDefault(pTaskWander);

		CPlayerInfo::ms_bDebugPlayerInvincible = true;

		CWanted * pWanted = pPlayer->GetPlayerWanted();
		int wantedLevel = g_defaultWantedLevel;
		PARAM_smoketestwantedlevel.Get(wantedLevel);
		pWanted->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), wantedLevel, 0, WL_FROM_TEST);
	}
}

void ProcessWarping(const int iWarpPeriod)
{
	static int iTimer=iWarpPeriod;

	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer)
	{
		return;
	}

	if(CWarpManager::IsActive())
	{
		iTimer=iWarpPeriod;
		return;
	}

	iTimer -= fwTimer::GetTimeStepInMilliseconds();
	if(iTimer <= 0)
	{
		Vector3 vPos;

		bool bFoundPosition = false;
		while(!bFoundPosition)
		{
			vPos.x = fwRandom::GetRandomNumberInRange(-3870.0f, 4064.0f);
			vPos.y = fwRandom::GetRandomNumberInRange(-5658.0f, 7904.0f);
			vPos.z = 1000.0f;

			float fGround = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, vPos.x, vPos.y, vPos.z, &bFoundPosition);
			if(bFoundPosition)
			{
				vPos.z = fGround;
			}
			else
			{
				strStreamingEngine::GetLoader().LoadRequestedObjects();
			}
		}

		pPlayer->Teleport(vPos, pPlayer->GetCurrentHeading());

		pPlayer->GetPedIntelligence()->ClearTasks();
		CTask * pTaskWander = pPlayer->ComputeWanderTask(*pPlayer);
		pPlayer->GetPedIntelligence()->AddTaskDefault(pTaskWander);

		CWanted * pWanted = pPlayer->GetPlayerWanted();
		int wantedLevel = g_defaultWantedLevel;
		PARAM_smoketestwantedlevel.Get(wantedLevel);
		pWanted->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), wantedLevel, 0, WL_FROM_TEST);

		iTimer=iWarpPeriod;
	}
}

void ProcessWarpingTest()
{
	int interval = 60;
	PARAM_warpingInterval.Get(interval);
	ProcessWarping(interval * 1000);
}

//
// name:		InitExplosionTest
// description:	Check for crashes while wandering around and having random explosions occur all over the place
void InitExplosionTest()
{
	CPed * pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		pPlayer->GetPedIntelligence()->ClearTasks();
		CTask * pTaskWander = pPlayer->ComputeWanderTask(*pPlayer);
		pPlayer->GetPedIntelligence()->AddTaskDefault(pTaskWander);

		CPlayerInfo::ms_bDebugPlayerInvincible = true;

		BANK_ONLY(UpdateAvailableCarsCB();)
	}
}

void ProcessExplosionTest()
{
	static u32 timeOfNextExplosion = 1000;
	if(fwTimer::GetTimeInMilliseconds() > timeOfNextExplosion)
	{
		Vector3 pos = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()) + Vector3((fwRandom::GetRandomTrueFalse() ? -1.f : 1.f) * fwRandom::GetRandomNumberInRange(5.f, 15.f),
			(fwRandom::GetRandomTrueFalse() ? -1.f : 1.f) * fwRandom::GetRandomNumberInRange(5.f, 15.f), 0.f);

		CExplosionManager::CExplosionArgs explosionArgs(EXP_TAG_HI_OCTANE, pos);

		explosionArgs.m_sizeScale = 50.f;
		explosionArgs.m_fCamShake = 0.0f;

		CExplosionManager::AddExplosion(explosionArgs);

		timeOfNextExplosion = fwTimer::GetTimeInMilliseconds() + (u32)fwRandom::GetRandomNumberInRange(200, 1000);
	}
	ProcessWarping(60*1000*10);	// Only warp every 10 mins
#if __BANK
	static u32 timeOfNextCar = 1000;
	if(fwTimer::GetTimeInMilliseconds() > timeOfNextCar)
	{
		CreateNextCar(true);
		timeOfNextCar = fwTimer::GetTimeInMilliseconds() + (u32)fwRandom::GetRandomNumberInRange(10000, 20000);
	}
#endif
}


void ProcessCutsceneTest()
{
#if __BANK
	CutSceneManager::GetInstance()->CutsceneSmokeTest();
#endif // __BANK
}

//
// name:		ProcessRestartTest
// description:	Check for memory leaks while restarting the level
void ProcessRestartTest()
{
	static sysTimer timer;
	if(timer.GetTime() > 60.0f)
	{
		timer.Reset();
		DisplayMemoryAvailable();
		const char *pLevelName = CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetCurrentLevelIndex());
		atDisplayf("Restarting level %s", pLevelName);
        CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());
	}
	DEBUG_DRAW_ONLY(grcDebugDraw::AddDebugOutput("Restarting in %f secs", 60.0f - timer.GetTime()));
}

//
// name:		InitShutdownTest
// description:	Test if shutdown crashes or asserts
void InitShutdownTest()
{
	FinishSmokeTest();
}

//
// name:		InitBuildState
// description:	Record the overall state of the build.
void InitBuildState()
{
}

void ProcessBuildState()
{
	static sysTimer timer;
	static float timeOfBuildStateCapture = 60.0f;
	if(timer.GetTime() > timeOfBuildStateCapture && timeOfBuildStateCapture>0.0f)
	{
		timeOfBuildStateCapture = -1.0f; // invalidate

		sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);

		if (pAllocator)
		{
			// DW - discrepency has been seen between these calls where Used or Available mem will change ~16K - this just leads to confusion.
			// simple solution :- read the values and pre-cache... although it seems to suggest something cookie is going on with allocator - defrag?, another thread?, atDisplayf allocs mem?
			size_t available = CStreamingDebug::GetMainMemoryAvailable() / 1024;
			size_t used = CStreamingDebug::GetMainMemoryUsed() / 1024;
			atDisplayf("BuildState:GH Total=%" SIZETFMT "u",(available + used));
			atDisplayf("BuildState:GH Used=%" SIZETFMT "u",used);
			atDisplayf("BuildState:GH Left=%" SIZETFMT "u",available);
		}

		atDisplayf("BuildState:Default=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_DEFAULT)/1024);
		atDisplayf("BuildState:Animation=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_ANIMATION)/1024);
		atDisplayf("BuildState:Streaming=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_STREAMING)/1024);
		atDisplayf("BuildState:World=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_WORLD)/1024);
		atDisplayf("BuildState:Gameplay=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_GAMEPLAY)/1024);
		atDisplayf("BuildState:FX=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_FX)/1024);
		atDisplayf("BuildState:Rendering=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_RENDER)/1024);
		atDisplayf("BuildState:Physics=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_PHYSICS)/1024);
		atDisplayf("BuildState:Audio=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_AUDIO)/1024);
		atDisplayf("BuildState:Network=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_NETWORK)/1024);
		atDisplayf("BuildState:System=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_SYSTEM)/1024);
		atDisplayf("BuildState:Scaleform=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_SCALEFORM)/1024);
		atDisplayf("BuildState:Script=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_SCRIPT)/1024);
		atDisplayf("BuildState:Resource=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_RESOURCE)/1024);
		atDisplayf("BuildState:Debug=%" SIZETFMT "u",CStreamingDebug::GetMainMemoryUsed(MEMBUCKET_DEBUG)/1024);

		atDisplayf("BuildState:Bounds=%d",0);
		atDisplayf("BuildState:Pools=%d",0);
		atDisplayf("BuildState:AI=%d",0);
		atDisplayf("BuildState:Process=%d",0);
		atDisplayf("BuildState:Pathfinding=%d",0);

		const char *platform = "PC";
#if __XENON
		platform = "Xbox360";
#elif __PS3
		platform = "PS3";
#endif

		// Create the path
		char outputPath[RAGE_MAX_PATH];
		sprintf(outputPath, "X:/gta5/docs/Code/BuildState/%s/%s/", RSG_CONFIGURATION, platform);

#if RAGE_TRACKING
		if (diagTracker::GetCurrent()) 
		{
			char memVisPath[RAGE_MAX_PATH];
			sprintf(memVisPath, "%smemvisualize/dummy.txt", outputPath);
			atDisplayf("Dumping memvis files to %s", memVisPath);
			diagTracker::GetCurrent()->FullReport(memVisPath);
			atDisplayf("Dumping memvis completed");
		}
		else
		{
			Errorf("diagTracker::GetCurrent is not set");
		}
#endif // RAGE_TRACKING

#if __BANK
		// Save out SceneCosts.txt
		char outputFile[RAGE_MAX_PATH];
		sprintf(outputFile, "%s%s", outputPath, "SceneCosts.txt");
		fiStream *pLogFileHandle = ASSET.Create(outputFile, "");	// Default location
		if( pLogFileHandle )
		{
			ResourceMemoryUseDump::BuildSceneCostStreamingIndices();
			ResourceMemoryUseDump::DumpSceneCostDetails(pLogFileHandle);
			fflush(pLogFileHandle);
			pLogFileHandle->Close();
		}
		else
		{
			Errorf("Could not create output file %s for writing.", outputFile);
		}

		// Save out ModuleInfo.txt
		sprintf(s_DumpOutputFile, "%sModuleInfo.txt", outputPath);	// Default location
		CStreamingDebug::DumpModuleInfoAllObjects();
#endif

		DisplaySmokeTestFinished();
	}
}


//
// name:		RageBuilderUnitTest
void InitRageBuilderUnitTest()
{
}

void ProcessRageBuilderUnitTest()
{
	static sysTimer timer;
	static float timeOfRageBuilderUnitTestCapture = 60.0f;
	if(timer.GetTime() > timeOfRageBuilderUnitTestCapture && timeOfRageBuilderUnitTestCapture>0.0f)
	{
		timeOfRageBuilderUnitTestCapture = -1.0f; // invalidate

		DisplaySmokeTestFinished();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

//
// Codebuilder Capture of stats
// A smoketest that is designated to run after successful codebuilder builds.
// - Maintained by runtime team.
// - Writes capture to an external file (* NOT the database directly! *)
// - Analysis of database is performed by external script.



////////////////////////////////////////////////////////////////////////////////////////////////////
static bool s_bCodebuilderStatsEnabled = false;
static bool s_bCodebuilderStatsEnded = false;

// Control Logic
bool SmokeTestStarted()
{
	return s_bCodebuilderStatsEnabled;
}

bool SmokeTestEnded()
{
	return s_bCodebuilderStatsEnded;
}

void SmokeTestEnd()
{
	s_bCodebuilderStatsEnabled = false;
}

void InitCodebuilderStats()
{
	s_bCodebuilderStatsEnabled = true;
}

void ProcessCodebuilderStats()
{
	if (!s_bCodebuilderStatsEnabled && !s_bCodebuilderStatsEnded)
	{
#if ENABLE_STATS_CAPTURE
		MetricsCapture::SmokeTestSaveResults("stats");
#endif // ENABLE_STATS_CAPTURE
		s_bCodebuilderStatsEnded = true;
		FinishSmokeTest();
	}
}

// Capture the state of the game
// - this is an example
void CaptureCodebuilderStatsState()
{
	char filename[255] = "stats.xml";

	fiStream* stream = filename ? fiStream::Create(filename, fiDeviceLocal::GetInstance()) : NULL;
	if (!stream)
	{
		Errorf("Could not create '%s'", filename);
		return;
	}

	fprintf(stream, "<?xml version='1.0' encoding='utf-8'?>\n");

	// Note we MUST tag the time, date on - since every capture needs to be different - otherwise we revert unchanged files when we submit them.
	char date_time[64];
	sprintf(date_time, "%d %d %d %d:%d:%d", CClock::GetYear(),CClock::GetMonth(), CClock::GetHour(), CClock::GetDay(), CClock::GetMinute(), CClock::GetSecond());

	fprintf(stream, "<STATS capture_time='%s'>\n",date_time);

	{
		sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);

		if (pAllocator)
		{
			// good practice is to grab the state in one go without making other calls since these may have influence on memory allocated etc.
			size_t available = pAllocator->GetMemoryAvailable() / 1024;
			size_t used = pAllocator->GetMemoryUsed() / 1024;
			size_t total = (available + used);

			fprintf(stream, "<MEMORY>\n");
			{
				fprintf(stream, "<MEMTYPE_GAME_VIRTUAL>\n");
				{
					fprintf(stream, "<TOTAL>%d</TOTAL>\n",total);
					fprintf(stream, "<USED>%d</USED>\n",used);
					fprintf(stream, "<AVAILABLE>%d</AVAILABLE>\n",available);
					fprintf(stream, "<TEST>%d</TEST>\n",available);
				}
				fprintf(stream, "</MEMTYPE_GAME_VIRTUAL>\n");
			}
			fprintf(stream, "</MEMORY>\n");
		}
	}

	fprintf(stream, "</STATS>\n");
	fflush(stream);
}




//====================================================================================
// STREAMING MEMORY STATS
void InitJunctionMemStats()
{
	CSectorTools::SetEnabled(false); // off by default - wait till we are ready to capture.
}

void ProcessJunctionMemStats()
{
	static sysTimer timer;
	static float timeOfJunctionMemStatsCapture = 60.0f;
	if(timer.GetTime() > timeOfJunctionMemStatsCapture && timeOfJunctionMemStatsCapture>0.0f)
	{
		timeOfJunctionMemStatsCapture = -1.0f; // invalidate
		CSectorTools::SetEnabled(true); // now it is enabled.
	}
	
	if(CSectorTools::IsFinished())
	{
		DisplaySmokeTestFinished();
	}
}

//====================================================================================
// OBJECT PROFILER

#include "ObjectProfiler.h"

void InitObjectProfiler()
{
#if __BANK
	CObjectProfiler::Init();
#endif	//__BANK
}

void ProcessObjectProfiler()
{
#if __BANK
	if( CObjectProfiler::Process() )
#endif	//__BANK
	{
		DisplaySmokeTestFinished();
	}
}

};	// namespace

#endif // #if !__FINAL
