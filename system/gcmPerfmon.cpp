#if __PPU

//SCEE headers
#include "grcore/wrapper_gcm.h"
#include <cell/gcm_pm.h>
#include <sys/gpio.h>

//Rage headers
#include "grcore/device.h"
#include "atl/map.h"
#include "atl/string.h"

//Core headers
#include "gcmPerfmon.h"
#include "debug/Debug.h"


#if USE_GCMPERFMON

#define PERFMON_CELL_SDK_VERSION 0x180002

#if (PERFMON_CELL_SDK_VERSION != CELL_SDK_VERSION)
#error "Non Matching version between CELL SDK and perf monitor wrapper. Please review any timer changes and update the code accordingly."
#endif // (PERFMON_SDK_VERSION != CELL_SDK_VERSION)

// Defines
#define CYCLE_COUNT_PER_S 500000000 // we got a 500Mhz GPU here...


// Timer Definition 
uint32_t pm_Events_IDs[CELL_GCM_PM_DOMAIN_NUM][4];

// Timer Result
u32 pm_Cycles[CELL_GCM_PM_DOMAIN_NUM];
u32 pm_Events[CELL_GCM_PM_DOMAIN_NUM][4];

// Strings
static const char *gcmPM_Domain_Str[] = {
									"HCLK",
									"GCLK",
									"RCLK",
									"SCLK",
									"MCLK",
									"ICLK" 
								};
								
// Map List for every domain : we need to use a map as the counter IDs are not linear...
static rage::atMap<u32, atString> gcmPM_Table_Str[CELL_GCM_PM_DOMAIN_NUM];

// So has to make the setup code a tad bit easier to read...
#define gcmPM_HCLK_Str gcmPM_Table_Str[CELL_GCM_PM_DOMAIN_HCLK]
#define gcmPM_GCLK_Str gcmPM_Table_Str[CELL_GCM_PM_DOMAIN_GCLK]
#define gcmPM_RCLK_Str gcmPM_Table_Str[CELL_GCM_PM_DOMAIN_RCLK]
#define gcmPM_SCLK_Str gcmPM_Table_Str[CELL_GCM_PM_DOMAIN_SCLK]
#define gcmPM_MCLK_Str gcmPM_Table_Str[CELL_GCM_PM_DOMAIN_MCLK]
#define gcmPM_ICLK_Str gcmPM_Table_Str[CELL_GCM_PM_DOMAIN_ICLK]

// Handy RegEx to regenerate the setup code, as the counter IDs, number and names will probably changes..
//   ^.#<CELL_GCM_PM_{[A-Z]^4}_{[a-zA-Z0-9_]*}>.#$
//   \tgcmPM_\1_Str[CELL_GCM_PM_\1_\2] = atString("\2");


static void pm_String_Setup()
{
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_SETUP2RASTER_ACTIVE] = atString("SETUP2RASTER_ACTIVE");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_SETUP2RASTER_IDLE] = atString("SETUP2RASTER_IDLE");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_SETUP2RASTER_STALLING] = atString("SETUP2RASTER_STALLING");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_SETUP2RASTER_STARVING] = atString("SETUP2RASTER_STARVING");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_INSTRUCTION_ISSUED_VPE0] = atString("INSTRUCTION_ISSUED_VPE0");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_PRIMITIVECOUNT] = atString("PRIMITIVECOUNT");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_CRASTER_OUTPUT_TILE_COUNT] = atString("CRASTER_OUTPUT_TILE_COUNT");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_CRASTER_SEARCH_MODE] = atString("CRASTER_SEARCH_MODE");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_GEOM_ATTRIBUTE_COUNT] = atString("IDX_GEOM_ATTRIBUTE_COUNT");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_VTX_CACHE_HIT] = atString("IDX_VTX_CACHE_HIT");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_VTX_CACHE_MISS] = atString("Pre-trans cache (vertex cache) misses");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_SETUP_POINTS] = atString("SETUP_POINTS");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_SETUP_CULLS] = atString("SETUP_CULLS");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_SETUP_LINES] = atString("SETUP_LINES");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_SETUP_TRIANGLES] = atString("Triangles proc by Setup ");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_VPC_CULL_PRIMITIVE] = atString("VPC_CULL_PRIMITIVE");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_VPC_CULL_WOULD_CLIP] = atString("VPC_CULL_WOULD_CLIP");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_FETCH_FBI] = atString("IDX_FETCH_FBI");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_FETCH_SYS] = atString("IDX_FETCH_SYS");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_VAB_CMD_LOAD] = atString("VAB_CMD_LOAD");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_VAB_CMD_TRI] = atString("VAB_CMD_TRI");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_RASTERIZEDPIXELCOUNT_1] = atString("RASTERIZEDPIXELCOUNT_1");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_RASTERIZEDPIXELCOUNT_2] = atString("RASTERIZEDPIXELCOUNT_2");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_POST_TRANSFORM_CACHE_HIT_1] = atString("IDX_POST_TRANSFORM_CACHE_HIT_1");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_POST_TRANSFORM_CACHE_HIT_2] = atString("IDX_POST_TRANSFORM_CACHE_HIT_2");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_POST_TRANSFORM_CACHE_MISS_1] = atString("IDX_POST_TRANSFORM_CACHE_MISS_1");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_POST_TRANSFORM_CACHE_MISS_2] = atString("IDX_POST_TRANSFORM_CACHE_MISS_2");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_VERTEX_COUNT_1] = atString("IDX_VERTEX_COUNT_1");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_IDX_VERTEX_COUNT_2] = atString("IDX_VERTEX_COUNT_2");

	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_TOTAL_TESTED_1] = atString("Tiles tested by Zcull");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_TOTAL_TESTED_2] = atString("Tiles tested by Zcull");

	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_Z_TRIVIALLY_REJECTED_1] = atString("Tiles trivially rejected");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_Z_TRIVIALLY_REJECTED_2] = atString("Tiles trivially rejected");

	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_STENCIL_CULLED_1] = atString("Tiles stencil culled");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_STENCIL_CULLED_2] = atString("Tiles stencil culled");

	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_Z_TRIVIALLY_ACCEPTED_1] = atString("Tiles trivially accepted");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_Z_TRIVIALLY_ACCEPTED_2] = atString("Tiles trivially accepted");

	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_NOT_CULLED_BUT_FAILS_ZTEST_1] = atString("Tiles not culled although pixels failing Z test");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_NOT_CULLED_BUT_FAILS_ZTEST_2] = atString("Tiles not culled although pixels failing Z test");

	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_DEPTH_BOUNDS_CULLED_1] = atString("Tiles culled with depth bounds");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_DEPTH_BOUNDS_CULLED_2] = atString("Tiles culled with depth bounds");

	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_NEAR_FAR_CLIPPED_1] = atString("Tiles clipped by Zmin/Zmax");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_NEAR_FAR_CLIPPED_2] = atString("Tiles clipped by Zmin/Zmax");

	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_TOTAL_CULLED_1] = atString("Tiles culled total");
	gcmPM_GCLK_Str[CELL_GCM_PM_GCLK_ZCULL_TILES_TOTAL_CULLED_2] = atString("Tiles culled total");


	gcmPM_RCLK_Str[CELL_GCM_PM_RCLK_CROP_COMP_WRITE] = atString("Num of comp. packets written to FP");
	gcmPM_RCLK_Str[CELL_GCM_PM_RCLK_ZROP_COMP_READ_COMP_WRITE] = atString("Packets read from and written to a comp area");
	gcmPM_RCLK_Str[CELL_GCM_PM_RCLK_ZROP_NO_READ_COMP_WRITE] = atString("Packets not read but written to a comp area");
	gcmPM_RCLK_Str[CELL_GCM_PM_RCLK_ZROP_UNCOMP_READ_COMP_WRITE] = atString("Packets read from comp area but not written");
	gcmPM_RCLK_Str[CELL_GCM_PM_RCLK_ZROP_TOTAL_WRITTEN] = atString("Write packets pass through ZROP");

	gcmPM_RCLK_Str[CELL_GCM_PM_RCLK_CROP_SUBPACKET_COUNT] = atString("Num of col subpackets from C-buffer to CROP");


	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_SQD_ENDSEG_ANY] = atString("SQD_ENDSEG_ANY");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_SQD_ENDSEG_RASTER_TIMEOUT] = atString("SQD_ENDSEG_RASTER_TIMEOUT");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_TEXTURE_ANISO_COUNT_01] = atString("TEXTURE_ANISO_COUNT_01");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_TEXTURE_ANISO_COUNT_02] = atString("TEXTURE_ANISO_COUNT_02");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_TEXTURE_ANISO_COUNT_04] = atString("TEXTURE_ANISO_COUNT_04");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_TEXTURE_ANISO_COUNT_08] = atString("TEXTURE_ANISO_COUNT_08");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_TEXTURE_ANISO_COUNT_16] = atString("Num of aniso > 13 fetches");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_TEXTURE_QUAD_COUNT] = atString("TEXTURE_QUAD_COUNT");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET1_MISS_COUNT] = atString("Num L2 tex cache misses due to t-fetch from local mem");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET1_REQUEST_COUNT] = atString("L2_TEXTURE_CACHE_SET1_REQUEST_COUNT");

	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET2_MISS_COUNT] = atString("L2_TEXTURE_CACHE_SET2_MISS_COUNT");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET2_REQUEST_COUNT] = atString("L2_TEXTURE_CACHE_SET2_REQUEST_COUNT");

	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_SHADER_ALL_QUADS] = atString("SHADER_ALL_QUADS");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_SHADER_FIRST_PASS_QUADS] = atString("SHADER_FIRST_PASS_QUADS");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_SIF_FETCH_TEXTURE_PASS_COUNT] = atString("Num of instruction fetches");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_SIF_FETCH_NO_TEXTURE_PASS_COUNT] = atString("Num of shader passes ex. SIF0");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_FINERASTERBUNDLES] = atString("FINERASTERBUNDLES");

	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET3_MISS_COUNT] = atString("L2_TEXTURE_CACHE_SET3_MISS_COUNT");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET3_REQUEST_COUNT] = atString("L2_TEXTURE_CACHE_SET3_REQUEST_COUNT");

	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET4_MISS_COUNT] = atString("L2_TEXTURE_CACHE_SET4_MISS_COUNT");
	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET4_REQUEST_COUNT] = atString("L2_TEXTURE_CACHE_SET4_REQUEST_COUNT");

	gcmPM_SCLK_Str[CELL_GCM_PM_SCLK_PREROP_VALID_PIXEL] = atString("PREROP_VALID_PIXEL");

	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_RW_COUNT1] = atString("FB_RW_COUNT1");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_RW_COUNT2] = atString("FB_RW_COUNT2");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_IDLE] = atString("FB_IDLE");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_RW_IDLE] = atString("FB_RW_IDLE");

	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_STALLED] = atString("FB_STALLED");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_COMPRESSED_FAST_CLEAR] = atString("Fast clears on comp area");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_EXPANDED_FAST_CLEAR] = atString("Fast clears on non-comp area");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_COMPRESSED_WRITE] = atString("FB_COMPRESSED_WRITE");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_NORMAL_WRITE] = atString("FB_NORMAL_WRITE");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_COMPRESSED_READ] = atString("FB_COMPRESSED_READ");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_NORMAL_READ] = atString("FB_NORMAL_READ");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_CROP_READ] = atString("Color reads from local memory for color blending");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_CROP_WRITE] = atString("Color writes to local memory");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_TEXTURE_READ] = atString("FB_TEXTURE_READ");
	gcmPM_MCLK_Str[CELL_GCM_PM_MCLK_FB_ZROP_READ] = atString("FB_ZROP_READ");

	gcmPM_ICLK_Str[CELL_GCM_PM_ICLK_SYS_NORMAL_WRITE] = atString("num of writes to main mem");
	gcmPM_ICLK_Str[CELL_GCM_PM_ICLK_SYS_NORMAL_READ] = atString("num of reads from main mem");
}

void pm_Init() 
{
	memset(pm_Events,0,sizeof(pm_Events));
	memset(pm_Cycles,0,sizeof(pm_Cycles));

	// Setup string tables for timer display...
	pm_String_Setup();

	// this stuff is restricted to four slots


	// Setup some default events to be recorded. this should cover most of our needs...
	//
	// all the counters that have a _1 and _2 ending need to set both into slot 1 and 2 ... not into 0 or 3
	//
#define TRIVIALLYCULLEDTILES 1
	pm_EventSet(
				CELL_GCM_PM_DOMAIN_GCLK, 
				CELL_GCM_PM_GCLK_SETUP_TRIANGLES,
#if ZCULLTILESTOTAL
				CELL_GCM_PM_GCLK_ZCULL_TILES_TOTAL_CULLED_1,
				CELL_GCM_PM_GCLK_ZCULL_TILES_TOTAL_CULLED_2,
#elif VERTEXCOUNT
				CELL_GCM_PM_GCLK_IDX_VERTEX_COUNT_1,
				CELL_GCM_PM_GCLK_IDX_VERTEX_COUNT_2,
#elif TRIVIALLYCULLEDTILES
				CELL_GCM_PM_GCLK_ZCULL_TILES_Z_TRIVIALLY_REJECTED_1,
				CELL_GCM_PM_GCLK_ZCULL_TILES_Z_TRIVIALLY_REJECTED_2,
#endif
				CELL_GCM_PM_GCLK_IDX_VTX_CACHE_MISS
				);

	pm_EventSet(CELL_GCM_PM_DOMAIN_SCLK,
				CELL_GCM_PM_SCLK_SIF_FETCH_TEXTURE_PASS_COUNT,
				CELL_GCM_PM_SCLK_SIF_FETCH_NO_TEXTURE_PASS_COUNT,
				CELL_GCM_PM_SCLK_L2_TEXTURE_CACHE_SET1_MISS_COUNT,
				CELL_GCM_PM_SCLK_TEXTURE_ANISO_COUNT_16);

	pm_EventSet(CELL_GCM_PM_DOMAIN_RCLK,
				CELL_GCM_PM_RCLK_CROP_COMP_WRITE,
				CELL_GCM_PM_RCLK_ZROP_COMP_READ_COMP_WRITE,
				CELL_GCM_PM_RCLK_ZROP_NO_READ_COMP_WRITE,
				CELL_GCM_PM_RCLK_ZROP_UNCOMP_READ_COMP_WRITE);

	pm_EventSet(CELL_GCM_PM_DOMAIN_MCLK,
				CELL_GCM_PM_MCLK_FB_CROP_READ,
				CELL_GCM_PM_MCLK_FB_CROP_WRITE,
				CELL_GCM_PM_MCLK_COMPRESSED_FAST_CLEAR,
				CELL_GCM_PM_MCLK_EXPANDED_FAST_CLEAR);

	pm_EventSet(CELL_GCM_PM_DOMAIN_ICLK,
				CELL_GCM_PM_ICLK_SYS_NORMAL_WRITE,
				CELL_GCM_PM_ICLK_SYS_NORMAL_READ,
				0,
				0);

	// And initialize the actual library
	cellGcmInitPerfMon();
	
	// Setup our default events
	pm_EventSetup();
	
	Displayf("PerfMon Initialized %x - %x",PERFMON_CELL_SDK_VERSION, CELL_SDK_VERSION);
}

void pm_Display(int startX, int startY)
{
	uint64_t dipSwitch = 0L;
	sys_gpio_get (SYS_GPIO_DIP_SWITCH_DEVICE_ID,&dipSwitch);
	bool detailed = 0x1 == ((dipSwitch & SYS_GPIO_DIP_SWITCH_USER_AVAILABLE_BITS) & 0x1);
	
	char str[1024];
	int x = startX;
	int y = startY;

	extern bool bLightsToScreen;

	sprintf(str,"GCM_PM Timer Results: Switch GPI 0 On (%s)", (true == bLightsToScreen) ? "light" : "scene" );
	grcDebugDraw::PrintToScreenCoors(str,x,y);
	y++;

	if(true == detailed)
	{
		for(int i = 0;i < CELL_GCM_PM_DOMAIN_NUM; i++)
		{
			// if it is HCLK-> not supported so far by PM
			if(gcmPM_Domain_Str[i] != "HCLK")
			{
				float msTime = ((float)pm_Cycles[i] / (float)CYCLE_COUNT_PER_S) * 1000.0f;
				sprintf(str,"Domain %s - %f", gcmPM_Domain_Str[i], msTime);
				grcDebugDraw::PrintToScreenCoors(str,x,y);
				y++;

				sprintf(str,"  Cycles %d", pm_Cycles[i]);
				grcDebugDraw::PrintToScreenCoors(str,x,y);
				y++;

				for(int j= 0;j < 4;j++)
				{
					u32 eventID = pm_Events_IDs[i][j];

					// if the name string is empty skip it 
					if(gcmPM_Table_Str[i][eventID] != "")
					{
						sprintf(str,"  %s %d", (const char *)gcmPM_Table_Str[i][eventID], pm_Events[i][j]);
						grcDebugDraw::PrintToScreenCoors(str,x,y);
						y++;
					}
				}
			}
		}
	}
	else
	{ // simplified display : just print out the ms count
		for(int i = 0;i < CELL_GCM_PM_DOMAIN_NUM; i++)
		{
			if(gcmPM_Domain_Str[i] != "HCLK")
			{
				float msTime = ((float)pm_Cycles[i] / (float)CYCLE_COUNT_PER_S) * 1000.0f;
				sprintf(str,"Domain %s - %f", gcmPM_Domain_Str[i], msTime);
				grcDebugDraw::PrintToScreenCoors(str,x,y);
				y++;
			}
		}
	}
}
#endif // USE_GCMPERFMON

#endif // __PPU
