#include "system/telemetry.h"

#include "bank/bkmgr.h"
#include "file/tcpip.h"
#include "fwtl/assetstore.h"
#include "fwutil/xmacro.h"
#include "grprofile/timebars.h"
#include "network/Live/NetworkTelemetry.h"
#include "profile/profiler.h"
#include "system/ControlMgr.h"
#include "system/new.h"
#include "system/memory.h"
#include "system/param.h"
#include "system/simpleallocator.h"
#include "system/buddyallocator_config.h"
#include "telemetry/telemetry_rage.h"
#include "fwdrawlist/drawlistmgr.h"

#if USE_TELEMETRY

PARAM(radtelemetry, "[profile] Enable RAD Telemetry on startup");

bool CTelemetry::sm_PlotCPU = false;
bool CTelemetry::sm_PlotMemory = false;
bool CTelemetry::sm_PlotEKG = false;
bool CTelemetry::sm_RenderSummary = false;

static bool g_CaptureStarted = false;

void ToggleCapture()
{
#if USE_SPARSE_MEMORY
	AssertMsg(0, "Can not use USE_SPARSE_MEMORY (Sparse memory allocator with Telemetry)");
	Errorf("Can not use USE_SPARSE_MEMORY (Sparse memory allocator with Telemetry)");
#else
	if(g_CaptureStarted)
	{
		TelemetryStop();
	}
	else
	{
		const char * appName = "GTA5";
		unsigned int arenaSize = RSG_SM_50 ? (64 * 1024 * 1024) : (2 * 1024 * 1024);
		
		TelemetryStart(appName, arenaSize);
	}
	
	g_CaptureStarted = !g_CaptureStarted;
	Printf("Telemetry %s\n", g_CaptureStarted ? "Enabled" : "Disabled");
#endif // defined(USE_SPARSE_MEMORY)
}

void CTelemetry::Init()
{
	if (PARAM_radtelemetry.Get() && !g_CaptureStarted)
	{
		ToggleCapture();
	}
}

#if __BANK
void CTelemetry::InitWidgets()
{
	bkBank* bank = &BANKMGR.CreateBank("Telemetry", 0, 0, false); 
	if(Verifyf(bank, "Failed to create Telemetry bank"))
	{
		bank->AddButton("Create Telemetry Widgets", datCallback(CFA1(CTelemetry::AddWidgets), bank));
	}
}

void CTelemetry::AddWidgets(bkBank& bank)
{
	// destroy first widget which is the create button
	bkWidget* widget = BANKMGR.FindWidget("Telemetry/Create Telemetry Widgets");
	if(widget == NULL)
	{
		return;
	}
	widget->Destroy();

	bank.AddButton("Toggle Capture", &ToggleCapture);
	bank.AddToggle("Plot CPU", &sm_PlotCPU);
	bank.AddToggle("Plot Memory", &sm_PlotMemory);
	bank.AddToggle("Plot EKG", &sm_PlotEKG);
	bank.AddToggle("Render Summary", &sm_RenderSummary);
}
#endif

#if RAGE_TIMEBARS

// cpu timings
struct CPUTiming
{
	const char * name;
	int timingSet;
};

static CPUTiming cpuTimings[] = 
{
	{"Start", 1},                               // Main: 
	{"Debug", 1},                               // Main: Update
	{"Front End", 1},                           // Main: Update
	{"Network", 1},                             // Main: Update
	{"Live Manager", 1},                        // Main: Update
	{"Streaming", 1},                           // Main: Update
	{"Veh Pop Process", 1},                     // Main: Update,GameUpdate
	{"VehicleAILodMgr Update", 1},              // Main: Update,GameUpdate
	{"Veh Mng Pretend Occ", 1},                 // Main: Update,GameUpdate
	{"Object pop process", 1},                  // Main: Update,GameUpdate
	{"Ped Pop Process", 1},                     // Main: Update,GameUpdate
	{"PedAILodMgr Update", 1},                  // Main: Update,GameUpdate
	{"World Process", 1},                       // Main: Update,GameUpdate,Scene update
	{"Animation", 1},                           // Main: Update,GameUpdate,Scene update
	{"Process all car intelligence", 1},        // Main: Update,GameUpdate,Scene update
	{"AI Update", 1},                           // Main: Update,GameUpdate,Scene update
	{"End Animation", 1},                       // Main: Update,GameUpdate,Scene update
	{"Physics", 1},                             // Main: Update,GameUpdate,Scene update
	{"Ped ai", 1},                              // Main: Update,GameUpdate,Scene update
	{"Pathserver Process", 1},                  // Main: Update,GameUpdate,Scene update
	{"Cover", 1},                               // Main: Update,GameUpdate,Scene update
	{"Misc", 1},                                // Main: Update,GameUpdate,Scene update
	{"Tiled Lighting", 1},                      // Main: Update,GameUpdate,Scene update
	{"Deferred Lighting", 1},                   // Main: Update,GameUpdate,Scene update
	{"Occlusion", 1},                           // Main: Update,GameUpdate,Scene update
	{"Tex LOD", 1},                             // Main: Update,GameUpdate,Scene update
	{"References", 1},                          // Main: Update,GameUpdate,Scene update
	{"Game Logic", 1},                          // Main: Update,GameUpdate
	{"Script", 1},                              // Main: Update,GameUpdate
	{"ProcessAfterMovement", 1},                // Main: Update,GameUpdate
	{"Render Update", 1},                       // Main: Update
	{"PlantsMgr::UpdateEnd", 1},                // Main: Update
	{"Visibility", 1},                          // Main: 
	{"Wait for GPU", 1},                        // Main: 
	{"Wait for render", 1},                     // Main: 
	{"AudioWait", 1},                           // Main: 
	{"Safe Execution", 1},                      // Main: 
	{"Build Drawlist", 1},                      // Main: 
	{"Start", 2},                               // Render: 
	{"DebugUpdate", 2},                         // Render: 
	{"SpuPmMgr update", 2},                     // Render: 
	{"DL_BEGIN_DRAW", 2},                       // Render: 
	{"DL_PREAMBLE", 2},                         // Render: 
	{"DL_PRE_RENDER_VP", 2},                    // Render: 
	{"DL_PED_DAMAGE_GEN", 2},                   // Render: 
	{"DL_RAIN_COLLISION_MAP", 2},               // Render: 
	{"DL_RAIN_UPDATE", 2},                      // Render: 
	{"DL_SCRIPT", 2},                           // Render: 
	{"DL_GBUF", 2},                             // Render: 
	{"DL_SHADOWS", 2},                          // Render: 
	{"DL_CLOUD_GEN", 2},                        // Render: 
	{"DL_WATER_REFLECTION", 2},                 // Render: 
	{"DL_WATER_SURFACE", 2},                    // Render: 
	{"DL_REFLECTION_MAP", 2},                   // Render: 
	{"DL_LIGHTING", 2},                         // Render: 
	{"DL_DRAWSCENE", 2},                        // Render: 
	{"DL_DEBUG", 2},                            // Render: 
	{"Scaleform Movie Render (MiniMap)", 2},    // Render: DL_STD
	{"Scaleform Movie Render (Hud)", 2},        // Render: DL_STD
	{"DL_TIMEBARS", 2},                         // Render: 
	{"DL_END_DRAW", 2},                         // Render: 
	{"Wait for Main", 2},                       // Render: 
	{NULL, -1},                                 // 
};

static bool IsValidCPUTiming(const CPUTiming & cpuTiming)
{
	return cpuTiming.name != NULL && cpuTiming.timingSet != -1;
}

static const char* TimingSetNameFromIndex(int timingSet)
{
	return g_pfTB.GetSetName(timingSet);
}

#endif // RAGE_TIMEBARS

void CTelemetry::PlotCPUTimings()
{
#if RAGE_TIMEBARS
	for(int i = 0; IsValidCPUTiming(cpuTimings[i]); ++i)
	{
		CPUTiming & timing = cpuTimings[i];
		float cpuTime = 0;
		int callcount = 0;
		g_pfTB.GetFunctionTotals(timing.name, callcount, cpuTime, timing.timingSet);
		TELEMETRY_PLOT_MS(cpuTime, "timing/%s/%s (cpu timing)", TimingSetNameFromIndex(timing.timingSet), timing.name);
	}
#endif
}

struct MemoryBucket
{
	const char * name;
	int bucket;
};

static MemoryBucket memoryBuckets[] = 
{
	{"Default", MEMBUCKET_DEFAULT},
	{"Animation", MEMBUCKET_ANIMATION},
	{"Streaming", MEMBUCKET_STREAMING},
	{"World", MEMBUCKET_WORLD},
	{"Gameplay", MEMBUCKET_GAMEPLAY},
	{"FX", MEMBUCKET_FX},
	{"Rendering", MEMBUCKET_RENDER},
	{"Physics", MEMBUCKET_PHYSICS},
	{"Audio", MEMBUCKET_AUDIO},
	{"Network", MEMBUCKET_NETWORK},
	{"System", MEMBUCKET_SYSTEM},
	{"Scaleform", MEMBUCKET_SCALEFORM},
	{"Script", MEMBUCKET_SCALEFORM},
	{"Resource", MEMBUCKET_RESOURCE},
	{"Debug", MEMBUCKET_DEBUG},
	{ NULL, -1 }
};

bool IsValidMemoryBucket(const MemoryBucket & memoryBucket)
{
	return memoryBucket.name != NULL && memoryBucket.bucket != -1;
}

void CTelemetry::PlotMemoryBuckets()
{
	sysMemAllocator & master = sysMemAllocator::GetMaster();

	for(int i = 0; IsValidMemoryBucket(memoryBuckets[i]); ++i)
	{
		MemoryBucket & bucket = memoryBuckets[i];

		int bucketIndex = bucket.bucket;
		u32 gameVirtual = (u32)master.GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed(bucketIndex);
		u32 gamePhysical = (u32)master.GetAllocator(MEMTYPE_GAME_PHYSICAL)->GetMemoryUsed(bucketIndex);
		u32 resourceVirtual = (u32)master.GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed(bucketIndex);
		u32 resourcePhysical = (u32)master.GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryUsed(bucketIndex);
		
		TELEMETRY_PLOT_MEMORY(gameVirtual, "memory/buckets/game_virtual/%s (memory bucket)", bucket.name);
		TELEMETRY_PLOT_MEMORY(gamePhysical, "memory/buckets/game_physical/%s (memory bucket)", bucket.name);
		TELEMETRY_PLOT_MEMORY(resourceVirtual, "memory/buckets/resource_virtual/%s (memory bucket)", bucket.name);
		TELEMETRY_PLOT_MEMORY(resourcePhysical, "memory/buckets/resource_physical/%s (memory bucket)", bucket.name);
	}
}

void CTelemetry::AddRenderSummary(CBlobBuffer & buffer)
{
	int numDrawListTypes = gDrawListMgr->GetDrawListTypeCount();
	buffer.WriteU32(numDrawListTypes);

	for(int i = 0; i < numDrawListTypes; i++)
	{
		buffer.WriteString(gDrawListMgr->GetDrawListName(i));
		buffer.WriteFloat(gDrawListMgr->GetDrawListGPUTime(i));
		buffer.WriteU32(gDrawListMgr->GetDrawListEntityCount(i));
	}
	
}

void CTelemetry::Update()
{
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T,KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
	{
		ToggleCapture();
	}

	if(sm_PlotCPU)
	{
		PlotCPUTimings();
	}

	if(sm_PlotMemory)
	{
		PlotMemoryBuckets();
	}

#if __STATS
	if(sm_PlotEKG)
	{
		pfPage * page = NULL;
		page = GetRageProfiler().GetNextPage(page, false);
		while(page)
		{
			pfGroup * group = NULL;
			group = page->GetNextGroup(group);
			while(group)
			{
				if(group->GetActive())
				{
					pfTimer * timer = NULL;
					timer = group->GetNextTimer(timer);
					while(timer)
					{
						if(timer->GetActive())
						{
							TELEMETRY_PLOT_MS(timer->GetFrameTime(), "EKG/%s/%s", page->GetName(), timer->GetName());
						}
						timer = group->GetNextTimer(timer);
					}

					pfValue * value = NULL;
					value = group->GetNextValue(value);
					while(value)
					{
						s32 index = -1;
						if(value->GetActive())
						{
							TELEMETRY_PLOT_FLOAT(value->GetAsFloat(), "EKG/%s/%s", page->GetName(), value->GetName(index));
						}
						value = group->GetNextValue(value);
					}
				}
				group = page->GetNextGroup(group);
			}
			page = GetRageProfiler().GetNextPage(page, false);
		}
	}
#endif

	CBlobBuffer buffer("rendersummary:1");
	if(sm_RenderSummary) AddRenderSummary(buffer);

	if(buffer.GetDataSizeInBytes() != 0)
	{
		TELEMETRY_ADD_BLOB(buffer, "Rage Blob", "Frame Blob");
	}
}

#endif // USE_TELEMETRY
