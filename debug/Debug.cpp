// Title	:	Debug.cpp
// Author	:	G. Speirs
// Started	:	10/08/99

#include "debug.h"

// STD headers
#if __XENON
#if defined NDEBUG && __ASSERT
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif
#include "system/xtl.h"
#include "tracerecording.h"
#endif

// Rage headers
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/msgbox.h"
#include "debug/FrameDump.h"
#include "file/device_installer.h"
#include "diag/output.h"
#include "grcore/debugdraw.h"
#include "fwrenderer/renderlistgroup.h"
#include "fragment/tune.h"
#include "grcore/font.h"
#include "grcore/image.h"
#include "grmodel/setup.h"
#include "grprofile/ekg.h"
#include "grprofile/profiler.h"
#include "grprofile/timebars.h"
#include "parser/psodebug.h"
#include "rmptfx/ptxdebug.h"
#include "streaming/packfilemanager.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingloader.h"
#include "streaming/streamingvisualize.h"
#include "system/ipc.h"
#include "system/exec.h"
#include "system/threadtype.h"
#include "grcore/debugdraw.h"
#include "system/stack.h"
#include "diag/tracker.h"		// RAGE_TRACK()
#if __WIN32PC
#include "input/keyboard.h"
#include "input/mouse.h"
#include "grcore/diag_d3d.h"
#endif

//framework headers
#include "entity/archetypemanager.h"

// Game headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "audio/dynamicmixer.h"
#include "audio/northaudioengine.h"
#include "animation/debug/animviewer.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/debug/FreeCamera.h"
#include "camera/Viewports/ViewportManager.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/frame.h"
#include "control/gamelogic.h"
#include "control/replay/ReplayControl.h"
#include "cutscene/CutSceneManagerNew.h"
#include "cutfile/cutfile2.h"
#include "debug/BankDebug.h"
#include "Debug/BlockView.h"
#include "Debug/BudgetDisplay.h"
#include "Debug/DebugDraw.h"
#include "Debug/DebugGlobals.h"
#include "Debug/DebugLocation.h"
#include "Debug/DebugRecorder.h"
#include "Debug/DebugScene.h"
#include "Debug/GtaPicker.h"
#include "debug/LightProbe.h"
#include "Debug/VectorMap.h"
#include "Debug/OverviewScreen.h"
#include "Debug/Rendering/DebugCamera.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/SceneGeometryCapture.h"
#include "event/EventNetwork.h"
#include "frontend/PauseMenu.h"
#include "frontend/MiniMap.h"
#include "frontend/hud_colour.h"
#include "frontend/NewHud.h"
#include "frontend/scaleform/scaleformmgr.h"
#include "frontend/GameStreamMgr.h"
#include "fwnet/neteventmgr.h"
#include "game/clock.h"
#include "game/weather.h"
#include "game/zones.h"
#include "Network/Network.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Bandwidth/NetworkBandwidthManager.h"
#include "Network/Debug/NetworkDebug.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/Objects/Entities/NetObjPlayer.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "Network/Voice/NetworkVoice.h"
#include "objects/object.h"
#include "peds/ped.h"
#include "peds/pedfactory.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "peds/PedPopulation.h"
#include "peds/PlayerInfo.h"
#include "Peds/Rendering/PedVariationDebug.h"
#include "physics/physics.h"
#include "physics/debugplayback.h"
#include "renderer/renderListGroup.h"
#include "renderer/drawlists/drawList.h"
#include "renderer/PlantsMgr.h"
#include "scene/AnimatedBuilding.h"
#include "scene/entities/compEntity.h"
#include "scene/RefMacros.h"
#include "scene/RegdRefTypes.h"
#include "scene/scene.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/LoadScene.h"
#include "scene/world/gameworld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/WarpManager.h"
#include "script/commands_streaming.h"
#include "shaders/CustomShaderEffectProp.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingvisualize.h"
#include "system/bootmgr.h"
#include "system/controlMgr.h"
#include "system/AutoGPUCapture.h"
#include "system/SettingsManager.h"
#include "system/ThreadPriorities.h"
#include "vehicleAi/pathfind.h"
#include "vehicles/train.h"
#include "vehicles/vehiclefactory.h"
#include "vehicles/vehiclepopulation.h"
#include "fwdebug/picker.h"
#include "fwsys/timer.h"
#include "scene/debug/PostScanDebug.h"
#include "physics/WorldProbe/worldprobe.h"
#include "text/messages.h"
#include "control/videorecording/videorecording.h"


#include "script/commands_socialclub.h"

#include "text/TextConversion.h"
#include "system/bootmgr.h"
#include "system/tmcommands.h"

// bugstar headers
#if BUGSTAR_INTEGRATION_ACTIVE

// Rage Headers
#include "qa/bugstarinterface.h"

// Game Headers
#include "BugstarIntegration.h"
#include "debug/bugstar/MissionLookup.h"

bool CDebug::ms_invincibleBugger = false;
bool CDebug::ms_showInvincibleBugger = true;
bool CDebug::ms_disableSpaceBugScreenshots = false;
u32 CDebug::ms_screenShotFrameDelay = 0;

PARAM(bugstardescpedvar, "[debug] Prints local player ped variation data to description on new bug creation");

#endif //BUGSTAR_INTEGRATION_ACTIVE

RAGE_DEFINE_CHANNEL(debug) 

AI_OPTIMISATIONS()

// EJ: Poor man's FPS capture
#define ENABLE_FPS_CAPTURE (0)

PARAM(debugstart, "Start at this co-ordinates with specific camera orientation");
PARAM(dontshowinvinciblebugger, "[debug] Don't show the invincible bugger message");
PARAM(peddebugvisualiser, "[debug] Default value for ped debug visualiser");
PARAM(dontdisplayforplayer, "[debug] Don't display information for players in the ped debug visualiser");
PARAM(enableBoundByGPUFlip, "PC-only: Enable us to be bound by the time spent in Present() - otherwise this is rolled into Render Thread");
PARAM(capturecard, "[debug] Capturecard is installed");
PARAM(debugrespot, "[debug] Debug MP vehicle respotting code");
PARAM(debugrespawninvincibility, "[debug] Debug MP respawn invincibility");
PARAM(debugdoublekill, "[debug] Debug MP double kill");
PARAM(debugexplosion, "[debug] Debug explosion at the player location");
PARAM(debuglicenseplate, "[debug] Debug license plate retrieval from SCS license plate system");
PARAM(debugaircraftcarrier, "[debug] Debug aircraft carrier");
PARAM(debugradiotest, "[debug] Debug radio test");
PARAM(dbgfontscale, "[debug] Init the debug fontscale with a specific value");
PARAM(dumpPedInfoOnBugCreation, "[debug] Dump ped info when a bug is created");
PARAM(displayNetInfoOnBugCreation, "[debug] Enable display of net object info when a bug is created");
PARAM(ekgdefault, "Startup ekg with the page");
PARAM(disableTimeSkipBreak, "Disable time skip detection");

PARAM(DR_RecordForBug, "Attach any debug recording to the bug, param is filename to store tmp recording in");
PARAM(licensePlate, "Text for the license plate");

#if BUGSTAR_INTEGRATION_ACTIVE
PARAM(disableSpaceBugScreenshots, "[debug] Prevents screenshots being added to bugs created using the Space bar");
#endif //BUGSTAR_INTEGRATION_ACTIVE

XPARAM(installremote);
XPARAM(gond);

#if RSG_DURANGO
namespace rage
{
	XPARAM(forceboothdd);
	XPARAM(forceboothddloose);
};
#endif

DR_ONLY(GameRecorder* CDebug::ms_GameRecorder);

namespace rage
{
	XPARAM(nopopups);
}

namespace rage {
	extern Vector2 g_previewCoords;
}
char CDebug::m_iVersionId[32] = {0};
char CDebug::m_sRawVersionId[32] = {0};
char CDebug::m_sOnlineVersionId[32] = {0};
#if !__FINAL
char CDebug::m_sFullVersionId[32];
#endif
char CDebug::m_iInstallVersionId[32];
int CDebug::m_iSavegameVersionId;

#if DEBUG_PAD_INPUT
//url:bugstar:4559363 - Can we bring the RDR style input debug back into GTAV?
//data source files:		CONTROLLER_TEST.fla/.as/.swf/.gfx

s32 CDebug::m_iScaleformMovieId = -1;

Vector2 m_vPos = Vector2( 0.04f, 0.145f );
float fMovieWidth = 0.097f;
float fMovieHeight = 0.403f;
float fSizeScaler = 0.8f;
Vector2 m_vSize = Vector2( fMovieWidth * fSizeScaler, fMovieHeight * fSizeScaler );
#endif

#if GTA_REPLAY
u32	CDebug::m_iReplayVersionId = 0;
#endif //GTA_REPLAY

char CDebug::ms_currentMissionTag[256];
char CDebug::ms_currentMissionName[256];

float CDebug::sm_fWaitForRender = 0.0f;
float CDebug::sm_fWaitForGPU = 0.0f;
float CDebug::sm_fWaitForMain = 0.0f;
float CDebug::sm_fDrawlistQPop = 0.0f;
float CDebug::sm_fEndDraw = 0.0f;
float CDebug::sm_fWaitForDrawSpu = 0.0f;
float CDebug::sm_fFifoStallTimeSpu = 0.0f;

bool CDebug::sm_enableBoundByGPUFlip = false;

#if __ASSERT
int CDebug::sm_iBlockedPopups = 0;
#endif // __ASSERT

#if __BANK

bkBank* CDebug::ms_pRageDebugChannelBank;
bkBank* CDebug::ms_pToolsBank;
CBankDebug* CDebug::ms_pBankDebug;
bool CDebug::m_debugParamUsed = false;
float CDebug::sm_dbgFontscale = 1.0f;
static bkBank* s_LiveEditBank;
static bool g_bDisplayTimescale = true;

#if RSG_DURANGO
bool CDebug::sm_bDisplayLightSensorQuad = false;
#endif

static const u32 s_numRenderPhases = 5;
const char* s_renderPhaseNames[] = {"DL_DRAWSCENE", "DL_GBUF", "DL_SHADOWS", "DL_REFLECTIONMAP", "DL_WATER_REFLECTION" };
dlDrawListMgr::drawlistStatData s_renderPhaseData[s_numRenderPhases];

bool CDebug::sm_debugrespot = false;
bool CDebug::sm_debugrespawninvincibility = false;
bool CDebug::sm_debugdoublekill = false;
bool CDebug::sm_debugexplosion = false;
bool CDebug::sm_debuglicenseplate = false;
bool CDebug::sm_debugaircraftcarrier = false;
bool CDebug::sm_debugradiotest = false;
int CDebug::sm_token = 0;
#endif // __BANK

#if !__FINAL

debugLocationList ms_locationList;
bool gSmoothPerformanceResults = true;
#if __BANK
static bool s_renderObjects = false;
static bool s_renderShaders = false;
#if RSG_DURANGO
static bool s_showDebugTimeInfo = false;
#endif // RSG_DURANGO
#endif // __BANK

#endif // !__FINAL

extern bool	g_render_lock;
extern bool gLastGenModeState;

void SwitchToSPVehicles()
{
	const CDataFileMgr::ContentChangeSet *changeSet = DATAFILEMGR.GetContentChangeSet("MPtoSP");

	Assert(changeSet);

	CFileLoader::ExecuteContentChangeSet(*changeSet);
}

void SwitchToMPVehicles()
{
	const CDataFileMgr::ContentChangeSet *changeSet = DATAFILEMGR.GetContentChangeSet("SPtoMP");

	Assert(changeSet);

	CFileLoader::ExecuteContentChangeSet(*changeSet);
}

#if DR_ENABLED

bool CDebug::IsDebugRecorderDisplayEnabled()
{
	if(ms_GameRecorder && ms_GameRecorder->IsDisplayOn())
	{
		return true;
	}
	return false;
}

#endif	// DR_ENABLED

//////////////////////////////////////////////////////////////////////////
#if BUGSTAR_INTEGRATION_ACTIVE
const char* gBugstarClassCombo[7] = {
	"A", "B", "C", "D", "TODO", "WISH", "TASK"
};
static atString gBugstarDescription;
static char gBugstarSummary[256] = "";
static char gBugstarOwner[256] = "";
static s32 gBugstarClass = 4;
static bool gBugstarPedVariationInfo = false;

bool s_requestAddBug = false;

void CDebug::RequestAddBug(){
	s_requestAddBug = true;
}

void CDebug::FillOutBugDetails(qaBug& bug)
{
	char tmp[256];
	Vector3 posn = camInterface::GetPos();
	Vector3 front = camInterface::GetFront();
	s32 hour = CClock::GetHour();
	s32 minute = CClock::GetMinute();

#if __XENON 
	bug.SetField(BUG_REP_PLATFORM,(void*)"Xbox 360",8); 
#elif __PPU 
	bug.SetField(BUG_REP_PLATFORM,(void*)"PS3",3); 
#elif __WIN32PC 
	bug.SetField(BUG_REP_PLATFORM,(void*)"PC",2); 
#elif RSG_ORBIS
	bug.SetField(BUG_REP_PLATFORM,(void*)"PS4",3); 
#elif RSG_DURANGO
	bug.SetField(BUG_REP_PLATFORM,(void*)"Xbox One",8); 
#endif

	bug.SetProduct("GTA 5");

	if(CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsOnARandomEvent() )
	{
		formatf(tmp,sizeof(tmp),"%s",CMissionLookup::GetMissionId(CDebug::ms_currentMissionName));
		bug.BeginField("mission","text");
		bug.BeginCustomFieldData();	
		bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
		bug.EndCustomFieldData();	
		bug.EndField();	
	}

	// POS
	formatf(tmp,sizeof(tmp),"%f",posn.x);
	bug.BeginField("XCoord","text");
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();	

	formatf(tmp,sizeof(tmp),"%f",posn.y);
	bug.BeginField("YCoord","text");	
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();	

	formatf(tmp,sizeof(tmp),"%f",posn.z);
	bug.BeginField("ZCoord","text");	
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();	

	// FRONT
	formatf(tmp,sizeof(tmp),"%f",front.x);
	bug.BeginField("XFront","text");
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();	

	formatf(tmp,sizeof(tmp),"%f",front.y);
	bug.BeginField("YFront","text");	
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();	

	formatf(tmp,sizeof(tmp),"%f",front.z);
	bug.BeginField("ZFront","text");	
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();

	// Hours
	formatf(tmp,sizeof(tmp),"%d",hour);
	bug.BeginField("TODHour","text");
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();	
	// Minutes
	formatf(tmp,sizeof(tmp),"%d",minute);
	bug.BeginField("TODMin","text");	
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();	

	if (CBlockView::GetNumberOfBlocks())
	{
		FileHandle fHandle = CFileMgr::OpenFile("common:/data/levels/gta5_liberty/vector_map.xml");
		if (fHandle)
		{
			bool exit = false;
			atString blockName, splineCount, pointCount;
			char line[256];
			while (CFileMgr::ReadLine(fHandle, line, 256) && !exit)
			{
				atString str(line);
				str.Trim();
				if (str.StartsWith("<section"))
				{
					atString dump, dump2, dump3;

					// 1 : Extract the name from the section 
					str.Split(dump, dump2, '"');
					dump2.Split(blockName, dump, '"');


					// 2 : Extract the spline count from the section
					dump.Split(dump2, dump3, "spline_count=\"");
					dump3.Split(splineCount, dump, '"');


					// 3 : For each spline, extract the points and see if we are inside the polygon defined by those
					int splines = atoi(splineCount.c_str());
					CFileMgr::ReadLine(fHandle, line, 256);			// ignore the line defining the colour of the section
					for (int i = 0; i < splines; ++i)
					{
						// get the number of points
						CFileMgr::ReadLine(fHandle, line, 256);
						atString str(line), pointCount;
						str.Trim();
						if (str.StartsWith("<spline point_count=\""))
						{
							str.Split(dump, dump2, '"');
							dump2.Split(pointCount, dump, '"');
							int points = atoi(pointCount.c_str());


							// 4 : construct the polygon from the set of points
							atArray<Vec2V> polygon;
							polygon.Reserve(points+1);
							spdAABB aabb;
							for (int pointIdx = 0; pointIdx < points; ++pointIdx)
							{
								CFileMgr::ReadLine(fHandle, line, 256);
								atString str(line), xValue, yValue;
								str.Trim();
								str.Split(dump, dump2, '"');
								dump2.Split(xValue, dump, '"');
								dump.Split(dump2, dump3, "y=\"");
								dump3.Split(yValue, dump, '"');
								Vec2V pt(static_cast<float>(atof(xValue.c_str())), static_cast<float>(atof(yValue.c_str()))); 
								polygon.Push( pt );

								aabb.GrowPoint( Vec3V(pt.GetXf(), pt.GetYf(), 0.0f) );
							}
							polygon.Push(polygon[0]);

							Vec2V pos(V_ZERO);
							camDebugDirector& debugDirector = camInterface::GetDebugDirector();
							if (debugDirector.IsFreeCamActive())
								pos = Vec2V(debugDirector.GetFreeCamFrame().GetPosition().x, debugDirector.GetFreeCamFrame().GetPosition().y);
							else
								pos = Vec2V(CGameWorld::FindLocalPlayerCentreOfWorld().x, CGameWorld::FindLocalPlayerCentreOfWorld().y);


							// 5 : Test if we are in the polygon only if we're in the aabb (for performance reasons)
							if (aabb.ContainsPointFlat(pos))
							{
								int wn = 0;
								for (int j=0; j<polygon.GetCount()-1; ++j)
								{
									Vec2V v1 = polygon[j];
									Vec2V v2 = polygon[j+1];
									if (v1.GetYf() <= pos.GetYf())
									{
										if (v2.GetYf()  > pos.GetYf())			// an upward crossing
											if ((v2.GetXf() - v1.GetXf()) * (pos.GetYf() - v1.GetYf()) - (pos.GetXf() - v1.GetXf()) * (v2.GetYf() - v1.GetYf()) > 0.0f)	// pos left of edge
												++wn;							// have a valid up intersect
									}
									else
									{
										if (v2.GetYf()  <= pos.GetYf())			// a downward crossing
											if ((v2.GetXf() - v1.GetXf()) * (pos.GetYf() - v1.GetYf()) - (pos.GetXf() - v1.GetXf()) * (v2.GetYf() - v1.GetYf()) < 0.0f)	// pos right of edge
												--wn;							// have a valid down intersect
									}
								}
								if(wn != 0)										// if we are inside the polygon
								{
									strcpy(tmp, blockName.c_str());
									exit = true;
								}
							}

							polygon.Reset();
							CFileMgr::ReadLine(fHandle, line, 256);			// ignore the </spline> tag line
						}
						else
						{
							Assertf(false, "No spline defined for the block %s", blockName.c_str());
						}
					}
				}
			}
			CFileMgr::CloseFile(fHandle);
		}
		else
		{
			formatf(tmp,sizeof(tmp),"%s",(const char*)CBlockView::GetBlockName(CBlockView::GetCurrentBlockInside()));
		}
	}
	else
		tmp[0] = '\0';
	bug.BeginField("grid","text");
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();

	// Write out the streaming video ID if it's present, otherwise send "1".
	if (CBugstarIntegration::GetStreamingVideoId(tmp, sizeof(tmp)) == false)
	{
		strcpy(tmp, "1");
	}
	bug.BeginField("video_id","text");
	bug.BeginCustomFieldData();	
	bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
	bug.EndCustomFieldData();	
	bug.EndField();

	if (GetVersionNumber())
	{
		formatf(tmp,sizeof(tmp),"%s - All", GetVersionNumber());
		bug.BeginField("Found In","text");	
		bug.BeginCustomFieldData();	
		bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
		bug.EndCustomFieldData();	
		bug.EndField();	
	}

	// MP Console log files...
	if(CBugstarIntegration::AddConsoleLogsToBug())
	{
		int logFileNumber = diagChannel::GetLogFileNumber();
		int minLogFileNumber = rage::Max(0, logFileNumber - 2);

		char origConsoleLogNoExtention[256] = "\0";

		strcpy(origConsoleLogNoExtention, diagChannel::GetLogFileName()); 
		strtok(origConsoleLogNoExtention, ".");

		if(logFileNumber)
		{
			char* underscore = strrchr(origConsoleLogNoExtention,'_');
			if(underscore)
			{
				*underscore = '\0';
			}
		}
	
		// we want the mpconsolelog fields to be mpconsolelogs1, mpconsolelogs2, mpconsolelogs3
		int fieldnamedigit = 1;
		for(int i = logFileNumber; i >= minLogFileNumber; --i)
		{
			tmp[0] = '\0';

			if(!i)
			{
				formatf(tmp,sizeof(tmp),"%s.log", origConsoleLogNoExtention);
			}
			else
			{
				if(origConsoleLogNoExtention)
				{
					formatf(tmp,sizeof(tmp),"%s_%d.log", origConsoleLogNoExtention, i);
				}
			}

			FileHandle handle = CFileMgr::OpenFile(tmp);
			if( CFileMgr::IsValidFileHandle(handle) )
			{
				char buffer[32] = "\0";
				formatf(buffer, 32, "mpconsolelogs%d", fieldnamedigit);
				++fieldnamedigit;

				bug.BeginField(buffer, "text");
				bug.BeginCustomFieldData();
				bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
				bug.EndCustomFieldData();
				bug.EndField();

				CFileMgr::CloseFile(handle);	
			}
		}

// 		AddNetLogToBug(NetworkInterface::GetMessageLog(), bug, fieldnamedigit);
// 		AddNetLogToBug(NetworkInterface::GetArrayManager().GetLog(), bug, fieldnamedigit);
// 		AddNetLogToBug(NetworkInterface::GetEventManager().GetLog(), bug, fieldnamedigit);
// 		AddNetLogToBug(NetworkInterface::GetObjectManagerLog(), bug, fieldnamedigit);
// 		AddNetLogToBug(NetworkInterface::GetObjectManager().GetReassignMgr().GetLog(), bug, fieldnamedigit);
// 		AddNetLogToBug(NetworkInterface::GetPlayerMgr().GetLog(), bug, fieldnamedigit);
// 		AddNetLogToBug(NetworkInterface::GetBandwidthManager().GetLog(), bug, fieldnamedigit);
// 		if(CTheScripts::GetScriptHandlerMgr().GetLog())
// 		{
// 			AddNetLogToBug(*CTheScripts::GetScriptHandlerMgr().GetLog(), bug, fieldnamedigit);
// 		}
	}

#if __BANK
	if (CAILogManager::ShouldAutoAddLogWhenBugAdded())
	{
		int fieldnamedigit = 1;
		AddNetLogToBug(CAILogManager::GetLog(), bug, fieldnamedigit);
	}
#endif // __BANK
}

void CDebug::AddNetLogToBug(const netLoggingInterface& logInterface, qaBug& bug, int& fieldNameDigit)
{
	char logNameNoExtention[256] = "";
	char tmp[256] = "";

	logInterface.GetFileName(tmp, sizeof(tmp));
	formatf(logNameNoExtention, "%s%s", CFileMgr::GetCurrDirectory(), tmp);
	// Remove extension
	char* extSeperator = strrchr(logNameNoExtention,'.');
	if (extSeperator)
	{
		*extSeperator = '\0';
	}

	for(int logPostfix = 1; /*break when it gets to an invalid file*/; ++logPostfix)
	{
		// The first log is logName.log, the second one is logName_2.log, Nth is logName_N.log
		if(logPostfix == 1) 
		{
			formatf(tmp,sizeof(tmp),"%s.log", logNameNoExtention);
		}
		else
		{
			formatf(tmp,sizeof(tmp),"%s_%d.log", logNameNoExtention, logPostfix);
		}

		FileHandle handle = CFileMgr::OpenFile(tmp);
		if( CFileMgr::IsValidFileHandle(handle) )
		{
			CFileMgr::CloseFile(handle);	

			char buffer[32] = "\0";
			formatf(buffer, 32, "mpconsolelogs%d", fieldNameDigit++);

			bug.BeginField(buffer, "text");
			bug.BeginCustomFieldData();
			bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
			bug.EndCustomFieldData();
			bug.EndField();
		}
		else
		{
			break;
		}
	}
}

void CDebug::DumpPedInfo()
{
	static const char* Names[] = {
		"PED_TASK_PRIORITY_PHYSICAL_RESPONSE",
		"PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP",
		"PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP",
		"PED_TASK_PRIORITY_PRIMARY",
		"PED_TASK_PRIORITY_DEFAULT",
		"PED_TASK_PRIORITY_MAX"
	};
	CPed::Pool* pPool = CPed::GetPool();
	if(!Verifyf(pPool, "Ped pool doesn't exist!?"))
	{
		return;
	}
	Displayf("========================================================================");
	Displayf("= PED DUMP BEGIN");
	Displayf("========================================================================");
	for( s32 pedIndex = 0; pedIndex < pPool->GetSize(); pedIndex++ )
	{
		CPed* pPed = pPool->GetSlot(pedIndex);
		if(pPed)
		{
			// Ped Name
			Displayf("%s:%s%s", pPed->GetLogName(), pPed->IsNetworkClone() ? " [Clone]" : "", pPed->IsAPlayerPed() ? " [Player]" : "");

			// Model
			Displayf("\tModel: %s", pPed->GetModelName());

			// Position
			Vector3 pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Displayf("\tPosition: %0.2f, %0.2f, %0.2f", pos.x, pos.y, pos.z);

			// Tasks
			CPedIntelligence* pIntel = pPed->GetPedIntelligence();
			CPedTaskManager* pTaskMgr = pIntel ? static_cast<CPedTaskManager*>(pIntel->GetTaskManager()) : NULL;
			if(Verifyf(pTaskMgr, "No task manager for %s", pPed->GetLogName()))
			{
				for(int p = PED_TASK_PRIORITY_PHYSICAL_RESPONSE; p < PED_TASK_PRIORITY_MAX; ++p)
				{
					CTask* task = static_cast<CTask*>(pTaskMgr->GetTask(PED_TASK_TREE_PRIMARY, p));
					if(!task)
						continue;
					Displayf("\t%s:", Names[p]);
					while(task)
					{
						if(task->IsClonedFSMTask())
						{
							// Format: TASK_NAME (TIME_IN_TASK) : STATE_NAME (TIME_IN_STATE) [NetState: STATE_FROM_NETWORK]
							CTaskFSMClone* cloneTask = static_cast<CTaskFSMClone*>(task);
							bool isLocal = !pPed->IsNetworkClone() || cloneTask->IsRunningLocally();
							Displayf("\t\t%s (%0.2f) : %s (%0.2f)%s%s%s", TASKCLASSINFOMGR.GetTaskName(cloneTask->GetTaskType()), cloneTask->GetTimeRunning(), cloneTask->GetCurrentStateName(), cloneTask->GetTimeInState(),
								!isLocal ? " [NetState: " : "", !isLocal ? cloneTask->GetStateName(cloneTask->GetStateFromNetwork()) : "", !isLocal ? "]" : "");
						}
						else
						{
							// Format: TASK_NAME (TIME_IN_TASK) : STATE_NAME (TIME_IN_STATE)
							Displayf("\t\t%s (%0.2f) : %s (%0.2f)", TASKCLASSINFOMGR.GetTaskName(task->GetTaskType()), task->GetTimeRunning(), task->GetCurrentStateName(), task->GetTimeInState());
						}
						task = task->GetSubTask();
					}
				}
			}
		}
	}
	Displayf("========================================================================");
	Displayf("= PED DUMP END");
	Displayf("========================================================================");
}

//
// name:		AddBug
// description: add the bug to Bug*
void CDebug::AddBug(bool requestAddBug, int bugType, bool requestDebugText, bool forceIgnoreVideoCapture, bool forceIgnoreScreenshot, bool forceSecondScreenshot)
{
	static bool ignoreVideo = false;
	static bool ignoreScreenshot = false;
	static bool captureSecondScreenshot = false;
	static bool sendSecondScreenshot = false;
	static bool debugTextForced = false;
	static bool createBug = true;
	static bool addBug = false;
#if __BANK
	static bool prevDisplayNetInfo = false;
#endif

	if(requestDebugText)
		debugTextForced = true;

	if (forceIgnoreVideoCapture)
		ignoreVideo = true;

	if (forceIgnoreScreenshot)
	{
		ignoreScreenshot = true;
	}
	else
	{
		ignoreScreenshot = false;
	}

	if (forceSecondScreenshot)
	{
		captureSecondScreenshot = true;
		sendSecondScreenshot = true;
	}

#if __BANK
	// if the debug text is not displayed and we need it to display and request a bug next frame
	if(!grcDebugDraw::GetDisplayDebugText() && debugTextForced)
	{
		grcDebugDraw::SetDisplayDebugText(true);
	}
#endif // __BANK

	// Add in a bug
	if (requestAddBug)
	{
#if __BANK
		if(PARAM_displayNetInfoOnBugCreation.Get() && !addBug)
		{
			// Cache the current setting
			prevDisplayNetInfo = NetworkDebug::GetDebugDisplaySettings().m_displayObjectInfo;
			// Enable network object info (so it shows up in the screenshot)
			NetworkDebug::GetDebugDisplaySettings().m_displayObjectInfo = true;
		}
#endif

		addBug = true;
		ms_screenShotFrameDelay = 3;
		return;
	}

	if (ms_screenShotFrameDelay > 0 && addBug)
	{
		ms_screenShotFrameDelay--;
		return;
	}

	if (!addBug)
	{
		return;
	}

	HANG_DETECT_SAVEZONE_ENTER();

#if !__NO_OUTPUT
	const unsigned startTimestamp = sysTimer::GetSystemMsTime();
	Displayf("AddBug - Adding Bug");
#endif

	// Capturing screenshot/communicating with bugstar prevents
	// audio vram requests from being serviced, leading to stream starvation.
	// Mute and pause the mixer for the duration to prevent this.
	if(audDriver::GetMixer())
	{
		audDriver::GetMixer()->SetPaused(true);
	}

	// the debug text was forced for this bug. turn it off again
#if __BANK
	if(debugTextForced)
	{
		debugTextForced = false;
		grcDebugDraw::SetDisplayDebugText(false);
	}
#endif // __BANK

	// Reset adding bug
	if (addBug)
	{
		addBug = false;
	}

#if DR_ENABLED
	const char* debugRecordingName = 0;
	if (PARAM_DR_RecordForBug.Get(debugRecordingName))
	{
		//If we've been recording data - add it to the bug
		//Added early as this need un-fragmented debug heap for a big save file buffer
		if (debugPlayback::DebugRecorder::GetInstance() && debugPlayback::DebugRecorder::GetInstance()->GetFirstFramePtr())
		{
			debugPlayback::DebugRecorder::GetInstance()->Save(debugRecordingName);
		}
	}
#endif	//DR_ENABLED

#if !__WIN32PC
	// if remote server is not running start it up
	if(!fiRemoteServerIsRunning)
		fiDeviceRemote::InitClass(0, NULL);
#endif

#if RSG_DURANGO
	char* shotName   = "X:/lastBug.jpg";
	char* shotNameF9 = "X:/lastBugF9.jpg";

	if(PARAM_forceboothdd.Get() || PARAM_forceboothddloose.Get())
	{
		shotName = "D:/lastBug.jpg";
	}
#else
	const char *shotName = "X:/lastBug.jpg";
	const char *shotNameF9 = "X:/lastBugF9.jpg";
#endif

	if(!ignoreScreenshot)
	{
		// if we take a second screenshot we need to wait 3 more frames
		if (captureSecondScreenshot && ms_screenShotFrameDelay == 0)
		{
			addBug = true;
			ms_screenShotFrameDelay = 3;
			captureSecondScreenshot = false;
			createBug = false;

#if RSG_DURANGO
			shotNameF9 = "X:/lastBugF9.jpg";
			if(PARAM_forceboothdd.Get() || PARAM_forceboothddloose.Get())
			{
				shotNameF9 = "D:/lastBugF9.jpg";
			}
#else
			shotNameF9 = "X:/lastBugF9.jpg";
#endif
			USE_DEBUG_MEMORY();

#if RSG_PC
			ASSERT_ONLY(sysMemStreamingCount++;)
			ASSERT_ONLY(sysMemAllowResourceAlloc++;)
			sysMemAllocator& temp_sma = sysMemAllocator::GetCurrent();
			sysMemAllocator::SetCurrent(*sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL));
#endif
			if (!GRCDEVICE.CaptureScreenShotToJpegFile(shotNameF9)) 
			{
				Warningf("Unable to take screenshot for bug, will use most recently taken shot instead.");
			}
#if RSG_PC
			sysMemAllocator::SetCurrent(temp_sma);
			ASSERT_ONLY(sysMemAllowResourceAlloc--;)
			ASSERT_ONLY(sysMemStreamingCount--;)
#endif
			// Remove the Mission Flow layout
			CControlMgr::GetKeyboard().FakeKeyPress(KEY_F9);
		}
		else
		{
			// Save JPEG image
			createBug = true;
			USE_DEBUG_MEMORY();

#if RSG_PC
			ASSERT_ONLY(sysMemStreamingCount++;)
			ASSERT_ONLY(sysMemAllowResourceAlloc++;)
			sysMemAllocator& temp_sma = sysMemAllocator::GetCurrent();
			sysMemAllocator::SetCurrent(*sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL));
#endif
			if (!GRCDEVICE.CaptureScreenShotToJpegFile(shotName)) 
			{
				Warningf("Unable to take screenshot for bug, will use most recently taken shot instead.");
			}
#if RSG_PC
			sysMemAllocator::SetCurrent(temp_sma);
			ASSERT_ONLY(sysMemAllowResourceAlloc--;)
			ASSERT_ONLY(sysMemStreamingCount--;)
#endif
		}
	}

	// Create Bug
	if (createBug)
	{
#if __BANK
		// Restore the disply object info settings after the screenshot
		if(PARAM_displayNetInfoOnBugCreation.Get())
		{
			NetworkDebug::GetDebugDisplaySettings().m_displayObjectInfo = prevDisplayNetInfo;
		}
#endif

		qaBugstarInterface BUGSTAR;
		qaBug bug;
		char tmp[128];

		FillOutBugDetails(bug);

		bug.SetSeverity(gBugstarClassCombo[gBugstarClass]);

		// either get the owner of the current mission or get the current block we are inside along with the owner & add it to the bug details
		if(bugType == BUG_TYPE_LEVELS && 
			( CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsOnARandomEvent() ) )
		{
			bug.SetOwner(CMissionLookup::GetMissionOwner(CDebug::ms_currentMissionName));
		}
		if(gBugstarOwner[0] == '\0' && CBlockView::GetNumberOfBlocks())
		{
			bug.SetOwner(CBlockView::GetBlockUser(CBlockView::GetCurrentBlockInside()));
		}
		else
		{
			bug.SetOwner(gBugstarOwner);
		}

		if (gBugstarDescription.GetLength())
		{
			bug.SetDescription(gBugstarDescription.c_str());
			gBugstarDescription.Clear();
		}
		else
		{
			bug.SetDescription(gBugstarSummary);
		}

		bug.SetSummary(gBugstarSummary);

		formatf(tmp,sizeof(tmp),"%d", static_cast<int>(ignoreVideo));
		bug.BeginField("IgnoreVideo","text");	
		bug.BeginCustomFieldData();	
		bug.AddCustomFieldData((void*)tmp,sizeof(char) * strlen(tmp));
		bug.EndCustomFieldData();	
		bug.EndField();	
		ignoreVideo = false;

#if !__FINAL
		if(CReplayMgr::IsEditModeActive())
		{
			char clipSourcePath[REPLAYPATHLENGTH];
			ReplayFileManager::getClipDirectory(clipSourcePath, true);

			const char* clipSourceName = CReplayMgrInternal::sm_lastLoadedClip.c_str();
			const char* clipDumpStore = ReplayFileManager::GetDumpFolder();
			char clipDumpStorePlatform[RAGE_MAX_PATH];
			sprintf(clipDumpStorePlatform, "%s\\%s\\", clipDumpStore, RSG_PLATFORM_ID);

			const char* extensions[] = {".clip", ".jpg"};

			for(int i = 0; i < sizeof(extensions) / sizeof(char*); ++i)
			{
				char fileName[RAGE_MAX_PATH];
				sprintf(fileName, "%s%s", clipSourceName, extensions[i]);
				ReplayFileManager::DumpReplayFile(clipSourcePath, fileName, clipDumpStorePlatform);

				char dumpedFilepath[RAGE_MAX_PATH];
				sprintf(dumpedFilepath, "%s%s", clipDumpStorePlatform, fileName);
				if(rage::ASSET.Exists(dumpedFilepath, ""))
				{
					const rage::fiDevice *pDevice = fiDevice::GetDevice(dumpedFilepath);
					if(pDevice)
					{
						char clippath[RAGE_MAX_PATH] = "";
						pDevice->FixRelativeName(clippath, RAGE_MAX_PATH, dumpedFilepath);
						if(rage::ASSET.Exists(clippath, ""))	// just being cautious - better than the user getting a cryptic bugstar error if something went wrong
						{
							bug.BeginField("Filepath", "text");
							bug.BeginCustomFieldData();
							bug.AddCustomFieldData((void*)clippath, sizeof(char) * strlen(clippath));
							bug.EndCustomFieldData();
							bug.EndField();
						}
					}
				}
			}
		}
#endif // !__FINAL

#if RSG_PC
		// attach the settings.xml file to the bug
		if (rage::ASSET.Exists(CSettingsManager::sm_settingsPath, ""))
		{
			const rage::fiDevice *pDevice=fiDevice::GetDevice(CSettingsManager::sm_settingsPath);
			if (pDevice)
			{
				char settingspath[RAGE_MAX_PATH]="";
				pDevice->FixRelativeName(settingspath,RAGE_MAX_PATH,CSettingsManager::sm_settingsPath);
				if(rage::ASSET.Exists(settingspath, ""))	// just being cautious - better than the user getting a cryptic bugstar error if something went wrong
					bug.SetLogFile(settingspath);
			}
		}
#endif

#if DR_ENABLED
		if (debugRecordingName)
		{ 
			bug.SetLogFile(debugRecordingName);
		}
#endif

		if(!ignoreScreenshot)
		{
			if (sendSecondScreenshot)
			{
				sendSecondScreenshot = false;
				BUGSTAR.CreateBug(bug, shotName, -1, shotNameF9);
			}
			else
			{
				BUGSTAR.CreateBug(bug, shotName);
			}
		}
		else
		{
			BUGSTAR.CreateBug(bug, NULL);
		}
		ignoreScreenshot = false;

		if(PARAM_dumpPedInfoOnBugCreation.Get())
		{
			DumpPedInfo();
		}
#if __BANK
		if (NetworkInterface::IsGameInProgress())
		{
			NetworkDebug::AddFailMark("BUGSTAR BUG ADDED");
		}
#endif // __BANK
	}

	if(audDriver::GetMixer())
	{
		audDriver::GetMixer()->SetPaused(false);
	}
	HANG_DETECT_SAVEZONE_EXIT(NULL);

#if !__NO_OUTPUT
	Displayf("AddBug - Bottom, TimeTaken: %u", sysTimer::GetSystemMsTime() - startTimestamp);
#endif
}

#if RSG_DURANGO && __BANK
extern "C" {
WINBASEAPI
VOID
WINAPI
GetSystemTimePreciseAsFileTime(
    _Out_ LPFILETIME lpSystemTimeAsFileTime
	);
}
#endif // RSG_DURANGO && __BANK

void CDebug::HandleBugRequests()
{
#if RSG_DURANGO && __BANK
	static u32 frame = 1;
	static u32 framesShown = 1;
	static time_t wallInitial;
	static ULONGLONG gtcInitial;
	static LARGE_INTEGER qpcInitial;
	static LARGE_INTEGER qpfInitial;
	static ULARGE_INTEGER gstInitial;
	static u64 gtcSum, qpcSum, gstSum;
	if(frame == 1)
	{
		wallInitial = time(NULL);
		QueryPerformanceCounter(&qpcInitial);
		QueryPerformanceFrequency(&qpfInitial);
		gtcInitial = GetTickCount64();
		FILETIME gstInitialFT;
		GetSystemTimePreciseAsFileTime(&gstInitialFT);
		gstInitial.HighPart = gstInitialFT.dwHighDateTime;
		gstInitial.LowPart = gstInitialFT.dwLowDateTime;
	}
	else
	{
		u64 tscBegin, tscEnd;

		LARGE_INTEGER qpf;
		QueryPerformanceFrequency(&qpf);
		if(qpf.QuadPart != qpfInitial.QuadPart)
		{
			diagLoggedPrintf("QueryPerformanceFrequency changed from %" SIZETFMT " to %" SIZETFMT "\n",
				(size_t)qpfInitial.QuadPart, (size_t)qpf.QuadPart);
			qpfInitial = qpf;
		}

		time_t wall = time(NULL);
		float wallSec = (float)(wall - wallInitial);

		LARGE_INTEGER qpc;
		tscBegin = __rdtsc();
		QueryPerformanceCounter(&qpc);
		tscEnd = __rdtsc();
		u64 tscQPC = tscEnd-tscBegin;
		qpcSum += tscQPC;
		float qpcSec = (float)(qpc.QuadPart - qpcInitial.QuadPart) / (float)qpf.QuadPart;

		tscBegin = __rdtsc();
		ULONGLONG gtc = GetTickCount64();
		tscEnd = __rdtsc();
		u64 tscGTC = tscEnd-tscBegin;
		gtcSum += tscGTC;
		float gtcSec = (float)(gtc - gtcInitial) / 1000.0f;

		FILETIME gstFT;
		tscBegin = __rdtsc();
		GetSystemTimePreciseAsFileTime(&gstFT);
		tscEnd = __rdtsc();
		u64 tscGST = tscEnd-tscBegin;
		gstSum += tscGST;
		ULARGE_INTEGER gst;
		gst.HighPart = gstFT.dwHighDateTime;
		gst.LowPart = gstFT.dwLowDateTime;
		float gstSec = (float)((double)(gst.QuadPart - gstInitial.QuadPart) * 1.0e-7);

		// R*TM can cause GetTickCount to drift slightly
		if(!PARAM_rockstartargetmanager.Get())
		{
			if(fabsf(qpcSec - wallSec) > 5.0f || fabsf(gtcSec - wallSec) > 5.0f || fabsf(gstSec - wallSec) > 5.0f)
			{
				static bool loggedDiscrepancy = false;
				if(!loggedDiscrepancy)
				{
					loggedDiscrepancy = true;
					diagLoggedPrintf("Timers out of sync! Wall: %f, QueryPerformanceCounter: %f, GetTickCount: %f, GetSystemTimePreciseAsFileTime: %f\n",
						wallSec, qpcSec, gtcSec, gstSec);

					if(sysBootManager::IsDebuggerPresent() && !PARAM_disableTimeSkipBreak.Get())
					{
						__debugbreak(); // Don't Assert(), since we care about BankRelease as much as Beta
					}
				}
			}
		}

		if(s_showDebugTimeInfo)
		{
			char buff[256];
			formatf(buff, "Frame %d:\n"
				"Wall: %f\n"
				"QPC : %f (%d, %d)\n"
				"GTC : %f (%d, %d)\n"
				"GSTP: %f (%d, %d)\n",
				frame, wallSec,
				qpcSec, (int)tscQPC, (int)(qpcSum / framesShown),
				gtcSec, (int)tscGTC, (int)(gtcSum / framesShown),
				gstSec, (int)tscGST, (int)(gstSum / framesShown)
			);
			Vector2 vPos(0.1f, 0.15f);
			grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());
			grcDebugDraw::Text(vPos, Color_white, buff, true, 1.0f, 1.0f);
			grcDebugDraw::TextFontPop();
			++framesShown;
		}
	}
	++frame;
#endif // RSG_DURANGO && __BANK

	bool requestAddBug = s_requestAddBug;
	bool requestDebugText = false;
	bool forceIgnoreVideoCapture = false;
	bool forceIgnoreScreenshot = false;
	bool writeGameplayLog = false;

	bool forceSecondScreenshot = false;
	int bugType = BUG_TYPE_ART;

	s_requestAddBug = false;

	if (ms_disableSpaceBugScreenshots)
		forceIgnoreScreenshot = true;

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SPACE, KEYBOARD_MODE_DEBUG, "Add bug to bugstar", "bugstar"))
	{
		STRVIS_MARK_BUG_REPORTED();
		requestAddBug = true;
		bugType = BUG_TYPE_ART;	
		writeGameplayLog = true; 
		// If "capturecard" is defined, then set to not capture a screenshot for default SPACEBAR usage
		if(PARAM_capturecard.Get())
		{
			forceIgnoreScreenshot = true;
		}
	}

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SPACE, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT, "Add bug to bugstar with second screenshot", "bugstar"))
	{
		STRVIS_MARK_BUG_REPORTED();
		requestAddBug = true;
		writeGameplayLog = true;
		forceSecondScreenshot = true;
		bugType = BUG_TYPE_ART;	
		// If "capturecard" is defined, then set to not capture a screenshot for default SPACEBAR usage
		if(PARAM_capturecard.Get())
		{
			forceIgnoreScreenshot = true;
		}
#if RSG_ORBIS
		ioKeyboard::OverrideKey(KEY_F9, true);
#else
		CControlMgr::GetKeyboard().FakeKeyPress(KEY_F9);
#endif // RSG_ORBIS
	}
	
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SPACE, KEYBOARD_MODE_DEBUG_CTRL, "Add bug to bugstar without video", "bugstar"))
	{	
		STRVIS_MARK_BUG_REPORTED();
		requestAddBug = true;
		writeGameplayLog = true;
		bugType = BUG_TYPE_ART;	
		forceIgnoreVideoCapture = true;
	}

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SPACE, KEYBOARD_MODE_DEBUG_ALT, "Add bug with report", "bugstar"))
	{	
		STRVIS_MARK_BUG_REPORTED();
		requestAddBug = true;
		writeGameplayLog = true;
		bugType = BUG_TYPE_ART;	
	}

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_1, KEYBOARD_MODE_DEBUG_CTRL, "Sets the console streaming video settings for Bugstar", "bugstar"))
	{
		CBugstarIntegration::SetStreamingVideoSettings();
	}

#if __BANK
	if (CAILogManager::ShouldSpewOutputLogWhenBugAdded())
	{
		if (writeGameplayLog)
		{
			CAILogManager::PrintAllGameplayLogs();	// Needs to be done on main thread as regreffed pointers shouldn't be accessed on the render thread
			CControlMgr::PrintDisabledInputsToGameplayLog();
		}
	}

#endif // __BANK	

#if SYSTMCMD_ENABLE
	if(PARAM_rockstartargetmanager.Get() && CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SPACE, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Add bug with dump file"))
	{
		sysTmCmdCreateDump();
	}
#endif // SYSTMCMD_ENABLE
	
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_LBRACKET, KEYBOARD_MODE_DEBUG, "Add with debug text to bugstar", "bugstar"))
	{
		requestAddBug = true;
		requestDebugText = true;
	}
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RBRACKET, KEYBOARD_MODE_DEBUG, "Add level bug to bugstar", "bugstar"))
	{
		requestAddBug = true;
		bugType = BUG_TYPE_LEVELS;
	}

#if BUGSTAR_INTEGRATION_ACTIVE
	static sysTimer lastBugTime;

	if (requestAddBug && lastBugTime.GetMsTime() < 5500)
	{
		Displayf("Just added a bug %f - call back later...", lastBugTime.GetMsTime());
		requestAddBug = false;
	}
	
	
	if (requestAddBug) {
		Warningf("Adding a bug - making player invulnerable for 10 seconds.");

		BANK_ONLY(audNorthAudioEngine::GetDynamicMixer().PrintDebug());

		if (ms_showInvincibleBugger)
		{
			char *pString = TheText.Get("BUG_INVIN");
			CGameStreamMgr::GetGameStream()->PostTicker( pString, false );
		}
		ms_invincibleBugger = true;
		lastBugTime.Reset();
	}
	bool bWasInvincible = ms_invincibleBugger;
	bool bStillInvincible = (lastBugTime.GetMsTime() < 10000);

	if (bWasInvincible && !bStillInvincible)
	{
		if (ms_showInvincibleBugger)
		{
			char *pString = TheText.Get("BUG_NOINVIN");
			CGameStreamMgr::GetGameStream()->PostTicker( pString, false );
		}
		ms_invincibleBugger = false;
	}

#endif // BUGSTAR_INTEGRATION_ACTIVE

	if (NetworkInterface::IsGameInProgress() || NetworkInterface::IsInSinglePlayerPrivateSession())
	{
		gBugstarDescription = "Gamer Tag: ";

		CPed* player = CGameWorld::FindLocalPlayer();
		if (player && player->GetPlayerInfo())
		{
			gBugstarDescription += player->GetPlayerInfo()->m_GamerInfo.GetName();
		}
		else
		{
			gBugstarDescription += "Not specified";
		}

		gBugstarDescription += " / Platform: " RSG_PLATFORM " " RSG_CONFIGURATION " v";
		gBugstarDescription += m_sRawVersionId;
		gBugstarDescription += "\n\n";
	}
	else
	{
		gBugstarDescription = "";
	}

#if __BANK
#if RSG_PC
	gBugstarDescription += atString(CDebugBar::GetSystemInfo());
	gBugstarDescription += "\n";
#endif

	if (g_PickerManager.IsEnabled() && g_PickerManager.GetNumberOfEntities())
	{
		gBugstarDescription += "Selected:\n";

		for (int i = 0; i < g_PickerManager.GetNumberOfEntities(); ++i)
		{
			atString name;
			atString packfile;
			CEntity* entity = static_cast<CEntity*>(g_PickerManager.GetEntity(i));

			// the entities can be NULL
			if (entity)
			{
				EntityDetails_GetModelName(entity, name);
				EntityDetails_GetPackfile(entity, packfile);

				Vec3V pos = entity->GetTransform().GetPosition();
				gBugstarDescription += atVarString("%s (in '%s') at %2.3f,%2.3f,%2.3f\n", 
					name.c_str(), 
					packfile.c_str(), 
					VEC3V_ARGS(pos));
			}

			if (i > 20) // don't blow our string
			{
				gBugstarDescription += "...\n";
				break;
			}
		}

		gBugstarDescription += "\n";
	}

	if (gBugstarPedVariationInfo)
	{
		CPed* pSelectedPed = NULL;
		CEntity* pSelectedEntity = CDebugScene::FocusEntities_Get(0);
		if (pSelectedEntity && pSelectedEntity->GetIsTypePed())
		{
			pSelectedPed = static_cast<CPed*>(pSelectedEntity);
		}
		else
		{
			pSelectedPed = CGameWorld::FindLocalPlayer();
		}

		if (pSelectedPed)
		{
			CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pSelectedPed->GetBaseModelInfo());

			gBugstarDescription += atVarString("Ped Variation (%s):\n", pSelectedPed->GetModelName());
			for (u32 i = 0; i < PV_MAX_COMP; i++)
			{
				if (pModelInfo->GetVarInfo()->IsValidDrawbl(i, pSelectedPed->GetPedDrawHandler().GetVarData().GetPedCompIdx((ePedVarComp)i)))
				{
					gBugstarDescription += atVarString("%s: (D:%d, T:%d, P:%d)\n", varSlotNames[i], pSelectedPed->GetPedDrawHandler().GetVarData().GetPedCompIdx((ePedVarComp)i),
																									pSelectedPed->GetPedDrawHandler().GetVarData().GetPedTexIdx((ePedVarComp)i),
																									pSelectedPed->GetPedDrawHandler().GetVarData().GetPedPaletteIdx((ePedVarComp)i));
				}
			}

			gBugstarDescription += "\nPed Props:";
			bool bHasProps = false;
			for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
			{
				eAnchorPoints anchor = pSelectedPed->GetPedDrawHandler().GetPropData().GetAnchor(i);
				if (anchor != ANCHOR_NONE)
				{
					bHasProps = true;
					s32 prop = pSelectedPed->GetPedDrawHandler().GetPropData().GetPropId(i);
					s32 texture = pSelectedPed->GetPedDrawHandler().GetPropData().GetTextureId(i);
					u8 distribution = pModelInfo->GetVarInfo()->GetPropTexDistribution((u8)anchor, (u8)prop, (u8)texture);
					gBugstarDescription += atVarString("\n%s: (D:%d, T:%d, P:%d)", propSlotNamesClean[anchor], prop, texture, distribution);
				}
			}
			
			if (!bHasProps)
			{
				gBugstarDescription += " None";
			}
		}
	}

#endif // __BANK

	if (gDrawListMgr->IsBuildingDrawList()){
		DLC_Add( AddBug, requestAddBug, bugType, requestDebugText, forceIgnoreVideoCapture, forceIgnoreScreenshot, forceSecondScreenshot);
	} else {
		AddBug(requestAddBug, bugType, requestDebugText, forceIgnoreVideoCapture, forceIgnoreScreenshot, forceSecondScreenshot);
	}
}

//////////////////////////////////////////////////////////////////////////
#endif  // BUGSTAR_INTEGRATION_ACTIVE
//////////////////////////////////////////////////////////////////////////


// 
// name: SetCurrentMissionTag
// description: sets the mission tag up for debug display. automatically setup by the 'SetMissionFlag' command retrieved from script
void CDebug::SetCurrentMissionTag(const char *missionTag)
{
#if STREAMING_VISUALIZE
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
	char markerString[128];
	formatf(markerString, "Start mission %s", missionTag);
	STRVIS_SET_MARKER_TEXT(missionTag);
#endif // STREAMING_VISUALIZE

	strcpy(ms_currentMissionTag,missionTag);
	strcpy(ms_currentMissionName,missionTag);
}

// 
// name: SetCurrentMissionName
// description: sets the mission name up for bugstar to use
void CDebug::SetCurrentMissionName(const char *missionName)
{
	SetCurrentMissionTag(missionName);
}

const char *CDebug::GetPackageType()
{
	if (fiDeviceInstaller::GetIsBootedFromHdd())
	{
		return "HDD Install";
	}
	else if (sysBootManager::IsBootedFromDisc())
	{
		return "Disc Build";
	}
	else if (sysBootManager::IsPackagedBuild())
	{
		return "Packaged Build";
	}
	else
	{
		return "Loose Files (SysTrayRFS)";
	}
}

const char *CDebug::GetFullVersionString(char *outBuffer, size_t bufferSize)
{
	safecpy(outBuffer, GetVersionNumber(), bufferSize);
	return outBuffer;
}


extern const char *diagAssertTitle;

// Name			:	SetVersionNumber
// Purpose		:	sets the games version number from the version.txt file
// Parameters	:	nothing
// Returns		:	true if success, false if failed
bool CDebug::SetVersionNumber()
{
	#define STANDARD_VERSION_NUMBER_FILENAME	"common:/data/version.txt"

	char cLine[64];
	char* pLine;
	FileHandle fid;

	CFileMgr::SetDir("");
	fid = CFileMgr::OpenFile(STANDARD_VERSION_NUMBER_FILENAME);

	if (!CFileMgr::IsValidFileHandle(fid))
	{
//		don't assert for now until everyone has syncd. Assertf(CFileMgr::IsValidFileHandle(fid), "No version number file found (DATA\\VERSION.TXT)" );
		m_iVersionId[0] = '\0';  // we couldnt get a version id so set to nothing
		return false;
	}

	while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) )
	{
		if(*pLine == '#' || *pLine == '\0') continue;

		sscanf(pLine, "%s", cLine);

		// read in version number:
		if (!strcmp(cLine, "[VERSION_NUMBER]"))
		{
			pLine = CFileMgr::ReadLine(fid);
			sscanf(pLine, "%s", m_iVersionId);  // sets the version number

			// Strip out any extraneous suffix characters (i.e. "dev_xx") to set the raw version ID
			int i = 0;
			for(; i < NELEM(m_iVersionId)-1 && i < NELEM(m_sRawVersionId)-1; ++i)
			{
				if((fwTextUtil::IsNumeric(m_iVersionId[i]) || m_iVersionId[i] == '.'))
				{
					m_sRawVersionId[i] = m_iVersionId[i];
				}
				else
				{
					break;
				}
			}

			m_sRawVersionId[i] = '\0';

#if !__FINAL
			formatf(m_sFullVersionId, "%s-%c%s", m_iVersionId, RSG_PLATFORM_CODE, RSG_CONFIGURATION_CODE);
#endif
		}

		if (!strcmp(cLine, "[ONLINE_VERSION_NUMBER]"))
		{
			pLine = CFileMgr::ReadLine(fid);
			sscanf(pLine, "%s", m_sOnlineVersionId);
		}

		// read in version number:
		if (!strcmp(cLine, "[INSTALL_VERSION_NUMBER]"))
		{
			pLine = CFileMgr::ReadLine(fid);
			sscanf(pLine, "%s", m_iInstallVersionId);  // sets the install version number
		}

		if (!strcmp(cLine, "[SAVEGAME_VERSION_NUMBER]"))
		{
			pLine = CFileMgr::ReadLine(fid);
			sscanf(pLine, "%d", &m_iSavegameVersionId);  // sets the savegame version number
		}

#if GTA_REPLAY
		if (!strcmp(cLine, "[REPLAY_VERSION_NUMBER]"))
		{
			pLine = CFileMgr::ReadLine(fid);
			sscanf(pLine, "%d", &m_iReplayVersionId);  // sets the replay version number
		}
#endif	//GTA_REPLAY

	}

	Displayf("\n*********************************************\n");
	Displayf("Version %s, Save version %d \n", m_iVersionId, m_iSavegameVersionId);
	Displayf("Build timestamp is: " __TIME__ " " __DATE__ "\n");
	Displayf("***********************************************\n");

	static char assertVersion[128];
	formatf(assertVersion, "GTAV "
#if __PS3
		"PS3 "
#elif __XENON
		"Xbox360 "
#elif __64BIT
#if RSG_DURANGO
		"Durango "
#elif RSG_ORBIS
		"Orbis "
#else
		"x64 "
#endif
#else
		"x86 "
#endif
		RSG_CONFIGURATION
		" %s", m_iVersionId);
	diagAssertTitle = assertVersion;

	CFileMgr::CloseFile(fid);

	// Tell our favorite memory tracker about this.
#if RAGE_TRACKING
	if (diagTracker::GetCurrent())
	{
		const char *packageType = GetPackageType();

		char versionString[32];
		GetFullVersionString(versionString, sizeof(versionString));

		diagTracker::GetCurrent()->SetVersionInfo(versionString, packageType);
	}
#endif // RAGE_TRACKING

	return true;
}

#if USE_PROFILER
void CDebug::RockyCaptureControlCallback(Profiler::ControlState state, u32 /*mode*/)
{
	if (state == Profiler::ROCKY_START)
	{
		static char foundInVersion[64];
		formatf(foundInVersion, "%s - All", GetVersionNumber());
		Profiler::AttachCaptureInfo("Version", foundInVersion);
		Profiler::AttachCaptureInfo("Bugstar Project Name", "GTA 5 DLC");
		Profiler::AttachCaptureInfo("Bugstar Project Id", "282246");
		Profiler::AttachCaptureInfo("Build Config", RSG_CONFIGURATION);
		Profiler::AttachCaptureInfo("Platform", RSG_PLATFORM_ID);
		Profiler::AttachCaptureInfo("Multiplayer Mode", NetworkInterface::IsGameInProgress() ? "MP" : "SP");

		// Render Resolution
		char resolution[128] = { 0 };
		formatf(resolution, "%dx%d", GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight());
		Profiler::AttachCaptureInfo("Render Resolution", resolution);
	}
}
#endif

#if !__FINAL
PARAM(traceframe, "Frame to do a trace on");

#if __XENON
PARAM(pgolitetrace, "Trace main/render threads during automated metrics captures");
bool CDebug::sm_bPgoLiteTrace = false;
u32 CDebug::sm_iPgoLiteTraceTime = 0;
bool CDebug::sm_bTraceUpdateThread = false;
bool CDebug::sm_bStartedUpdateTrace = false;
unsigned int CDebug::sm_iUpdateTraceCount = 0;
bool CDebug::sm_bTraceRenderThread = false;
bool CDebug::sm_bStartedRenderTrace = false;
unsigned int CDebug::sm_iRenderTraceCount = 0;
static const char gTraceDirectory[] = "devkit:\\trace";
#endif // __XENON

void CDebug::Update()
{
	Update_CDEBUG();

#if __XENON && !__FINAL
	// check trace on frame parameter
	u32 frame = 0;
	if(PARAM_traceframe.Get(frame) && fwTimer::GetSystemFrameCount() == frame)
	{
		sm_bTraceUpdateThread = true;
	}

	if(sm_iPgoLiteTraceTime != 0 && sm_iPgoLiteTraceTime <= fwTimer::GetTimeInMilliseconds())
	{
		sm_bTraceUpdateThread = true;
		sm_iPgoLiteTraceTime = 0;
	}

	TraceThread(sm_bTraceUpdateThread, sm_bStartedUpdateTrace, "update_trace", sm_iUpdateTraceCount);
#endif

#if __ASSERT
	UpdateNoPopups(false);
#endif // __ASSERT
}

void CDebug::UpdateLoading()
{
#if __ASSERT
	UpdateNoPopups(true);
#endif // __ASSERT
}

#if __ASSERT
void CDebug::UpdateNoPopups(bool UNUSED_PARAM(loading))
{
	if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_P, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT, "Toggle nopopups") )
	{
		if (PARAM_nopopups.Get())
		{
			Displayf("enabling popups: -nopopups OFF");
			PARAM_nopopups.Set(NULL);
		}
		else
		{
			Displayf("disabling popups: -nopopups ON");
			PARAM_nopopups.Set("");
		}
	}

	//sm_iBlockedPopups = diagChannel::DetectBlockedPopups(loading);
}
#endif // __ASSERT

void CDebug::Init(unsigned initMode)
{
	Init_CDEBUG(initMode);

#if(BUGSTAR_INTEGRATION_ACTIVE) 
	if (PARAM_disableSpaceBugScreenshots.Get())
		ms_disableSpaceBugScreenshots = true;
#endif

#if __XENON && !__FINAL
	const fiDevice *pDevice = fiDevice::GetDevice(gTraceDirectory, false);

	if(pDevice != NULL)
	{
		sm_bPgoLiteTrace = PARAM_pgolitetrace.Get();

		if(sm_bPgoLiteTrace)
		{
			pDevice->DeleteDirectory(gTraceDirectory, true);
		}

		pDevice->MakeDirectory(gTraceDirectory);
	}
#endif // __XENON && !__FINAL
}

#if __XENON
void CDebug::TraceThread(bool& bTrace, bool& bTraceStarted, const char* pFilename, unsigned int& iTraceCount)
{
	// if started trace
	if(bTraceStarted)
	{
		XTraceStopRecording();
		bTraceStarted = false;
	}

	// if a trace recording has been requested
	if(bTrace)
	{
#define TRACE_FILENAME_PADDING sizeof(gTraceDirectory) - 1 + sizeof("\\_4294967295.pix2")

		Assertf(strlen(pFilename) + TRACE_FILENAME_PADDING <= 256, "Filename too long");

		char buffer[256];

		sprintf(buffer, "%s\\%s_%u.pix2", gTraceDirectory, pFilename, iTraceCount);

		XTraceStartRecording(buffer);
		bTraceStarted = true;
		bTrace = false;
		++iTraceCount;
	}
}
#endif

#endif //!__FINAL

#if ENABLE_CDEBUG && __BANK
static bool gDisplayTimerBars = grcDebugDraw::GetDisplayDebugText();
#elif RAGE_TIMEBARS || DEBUG_DRAW
static bool gDisplayTimerBars = false;
#endif 

#if ENABLE_CDEBUG
#if __BANK
s32 CDebug::m_iBugNumber = -1;
#endif

#if !__FINAL

#if __BANK
int gCurrentDisplayObjectIndex;
int gCurrentDisplayObjectCount;
static char** gCurrentDisplayObjectList;
char gCurrentDisplayObject[STORE_NAME_LENGTH];
char gStreamModelName[STORE_NAME_LENGTH];
strModelRequest gStreamModelReq;
static bkCombo* gDisplayObjectListCombo;
static bool gActivateDisplayObject = false;
static bool gDisplaySafeZone = false;
static bool gDisplayMapZone = false;
static bool gDisplaySpawnNodes = false;


static int gPackfileSource;
static const char *gPackfileSourceDummyList[] = { "[Press button above to populate]" };
static const char *gObjectToCreateDummyList[] = { "[Select packfile to populate]" };
static bkCombo *gPackfileSourceCombo;
static bkCombo *gObjectToCreateCombo;
static const char **gPackfileSourceList;
static int *gPackfileMapping;					// Maps the combo index to the actual packfile index
static int gPackfileSourceCount;

static const char **gObjectNameList;
static fwModelId *gObjectModelIdList;
static int gObjectToCreateCount;
static int gObjectToCreateIndex;

#if __ASSERT
const char* CDebug::GetNoPopupsMessage(bool loading)
{
	if (loading)
	{
		UpdateLoading();
	}

	if (PARAM_nopopups.Get() || CDebug::sm_iBlockedPopups)
	{
		static char msg[32] = "";
		if (PARAM_nopopups.Get())
			strcpy(msg, "No Popups");
		else
			sprintf(msg, "Blocked Popups (%d)", CDebug::sm_iBlockedPopups);
		return msg;
	}
	else
	{
		return NULL;
	}
}
#endif // __ASSERT

#if __DEV
bool CDebug::GetDisplaySpawnNodes()
{
	return gDisplaySpawnNodes;
}
bool CDebug::GetDisplayTimerBars()
{
	return gDisplayTimerBars;
}

#endif
#if __BANK
void CDebug::SetDisplayTimerBars(bool bEnable)
{
	gDisplayTimerBars = bEnable;
}
#endif

RegdEnt gpDisplayObject(NULL);

#if __D3D && __WIN32PC
static bool showd3dresourceinfo = false;
#endif


static void DisplaySafeZone()
{
	CSystem::GetRageSetup()->EnableSafeZoneDraw(gDisplaySafeZone, CHudTools::GetSafeZoneSize());
}

void DeleteObject()
{
	if(gpDisplayObject)
	{
		CGameWorld::Remove(gpDisplayObject);

		if(gpDisplayObject->GetIsTypeVehicle())
			CVehicleFactory::GetFactory()->Destroy((CVehicle*)gpDisplayObject.Get());
		else if(gpDisplayObject->GetIsTypePed())
			CPedFactory::GetFactory()->DestroyPed((CPed*)gpDisplayObject.Get());
		else if(gpDisplayObject->GetIsTypeObject())
			CObjectPopulation::DestroyObject((CObject*)gpDisplayObject.Get());
		else
			delete gpDisplayObject;

		gpDisplayObject = NULL;
	}
}

void FixupObject(CEntity *pObject, const Matrix34 &testMat)
{
	if(pObject)
	{
		if (!pObject->GetTransform().IsMatrix())
		{
			#if ENABLE_MATRIX_MEMBER
			Mat34V matrix(MATRIX34_TO_MAT34V(testMat));			
			pObject->SetTransform(matrix);
			pObject->SetTransformScale(1.0f, 1.0f);
			#else
			Mat34V matrix(MATRIX34_TO_MAT34V(testMat));
			fwTransform *trans = rage_new fwMatrixTransform(matrix);
			pObject->SetTransform(trans);
			#endif
		}

		pObject->SetMatrix(testMat, true);

		if (fragInst* pInst = pObject->GetFragInst())
		{
			pInst->SetResetMatrix(testMat);
		}

		CGameWorld::Add(pObject, CGameWorld::OUTSIDE );
		
		if ( pObject->GetIsDynamic() )
		{
			CPortalTracker*		pPT = static_cast< CDynamicEntity* >( pObject )->GetPortalTracker();
			pPT->ScanUntilProbeTrue();
			pPT->Update( VEC3V_TO_VECTOR3( pObject->GetTransform().GetPosition() ) );
		}
	}

	// create physics for static xrefs
	if (pObject && pObject->GetIsTypeBuilding()
		&& pObject->GetCurrentPhysicsInst()==NULL && pObject->IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT))
	{
		// create physics
		pObject->InitPhys();
		if (pObject->GetCurrentPhysicsInst())
		{
			pObject->AddPhysics();
		}
	}

	//If the entity being is an env cloth then we need to transform the cloth
	//verts to the entity frame.
	if(pObject && pObject->GetCurrentPhysicsInst() && pObject->GetCurrentPhysicsInst()->GetArchetype())
	{
		// TODO: Should this really be checking if it has the GTA_ENVCLOTH_OBJECT_TYPE flag, not that it has *only* the GTA_ENVCLOTH_OBJECT_TYPE flag?
		if(PHLEVEL->GetInstanceTypeFlags(pObject->GetCurrentPhysicsInst()->GetLevelIndex()) == ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE)
		{
			//Now transform the cloth verts to the frame of the cloth entity.
			phInst* pEntityInst=pObject->GetCurrentPhysicsInst();
			Assertf(dynamic_cast<fragInst*>(pEntityInst), "Physical with cloth must be a frag");
			fragInst* pEntityFragInst=static_cast<fragInst*>(pEntityInst);
			Assertf(pEntityFragInst->GetCached(), "Cloth entity has no cache entry");
			if(pEntityFragInst->GetCached())
			{
				fragCacheEntry* pEntityCacheEntry=pEntityFragInst->GetCacheEntry();
				Assertf(pEntityCacheEntry, "Cloth entity has a null cache entry");
				fragHierarchyInst* pEntityHierInst=pEntityCacheEntry->GetHierInst();
				Assertf(pEntityHierInst, "Cloth entity has a null hier inst");
				environmentCloth* pEnvCloth=pEntityHierInst->envCloth;
				Assertf( pEnvCloth, "Object/entity is marked as cloth but doesn't have cloth attached" );
				if( pEnvCloth )
				{
					Matrix34 clothMatrix = MAT34V_TO_MATRIX34(pObject->GetMatrix());								
					pEnvCloth->SetInitialPosition( VECTOR3_TO_VEC3V(clothMatrix.d) );
				}
			}
		}
	}//	if(pObject && pObject->GetCurrentPhysicsInst() && pObject->GetCurrentPhysicsInst()->GetArchetype())...

	if (pObject && pObject->GetCurrentPhysicsInst() && gActivateDisplayObject)
	{
		PHSIM->ActivateObject(pObject->GetCurrentPhysicsInst());
	}
}

u32 gLegionCountX = 10;
u32 gLegionCountY = 10;
float gLegionSpacing = 0.f;
Vector3 gDisplayOffset(0, 0, 0);
atArray<RegdEnt> gLegion;

void DisplayObject(bool bDeletePrevious)
{
	if(gpDisplayObject && bDeletePrevious)
	{
		DeleteObject();
	}

	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromName(gCurrentDisplayObject, &modelId);
	if(pModelInfo==NULL)
	{
		Displayf("Couldn't get model info for '%s'.", gCurrentDisplayObject);
		return;
	}

	gpDisplayObject = NULL;

	Matrix34 testMat;
	testMat.Identity();

	Vector3 vecCreateOffset = camInterface::GetFront();
	vecCreateOffset.z = 0.0f;
	vecCreateOffset *= 2.0f + pModelInfo->GetBoundingSphereRadius();

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	testMat.d = debugDirector.IsFreeCamActive() ? camInterface::GetPos() : CGameWorld::FindLocalPlayerCoors();
	testMat.d += vecCreateOffset + gDisplayOffset;

	if(debugDirector.IsFreeCamActive())
		testMat.Set3x3(camInterface::GetMat());
	else if(CGameWorld::FindLocalPlayer())
		testMat.Set3x3(MAT34V_TO_MATRIX34(CGameWorld::FindLocalPlayer()->GetMatrix()));

	if(pModelInfo)
	{
		//Allow cut scene props to be displayed, cutscene props have the prefix "CS" so use this to identify cut scene props 
		bool bIsCutSceneObject = false; 
		if (strnicmp("cs", pModelInfo->GetModelName(), 2) == 0)
		{
			bIsCutSceneObject = true;
		}

		if(pModelInfo->GetModelType()==MI_TYPE_VEHICLE || pModelInfo->GetModelType()==MI_TYPE_PED || pModelInfo->GetIsTypeObject()||bIsCutSceneObject)
		{
			bool bForceLoad = false;
			if(!CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				bForceLoad = true;
			}

			if(bForceLoad)
			{
				CStreaming::LoadAllRequestedObjects(true);
			}
		}

		if(pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
		{
			gpDisplayObject = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_DEBUG, POPTYPE_TOOL, &testMat);
			if (gpDisplayObject)
			{
				u8 nNewColour1, nNewColour2, nNewColour3, nNewColour4, nNewColour5, nNewColour6;
				CVehicleModelInfo* vmi = (CVehicleModelInfo*)pModelInfo;

                CVehicle* veh = (CVehicle*)gpDisplayObject.Get();
				vmi->ChooseVehicleColourFancy(veh, nNewColour1, nNewColour2, nNewColour3, nNewColour4, nNewColour5, nNewColour6);
				veh->SetBodyColour1(nNewColour1); 
				veh->SetBodyColour2(nNewColour2); 
				veh->SetBodyColour3(nNewColour3); 
				veh->SetBodyColour4(nNewColour4);
				veh->SetBodyColour5(nNewColour5);
				veh->SetBodyColour6(nNewColour6);
				veh->UpdateBodyColourRemapping();
			}
		}
		else if(pModelInfo->GetModelType()==MI_TYPE_PED)
		{
			const CControlledByInfo localAiControl(false, false);
			gpDisplayObject = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, &testMat, true, true, false);
		}
		else if(pModelInfo->GetIsTypeObject()||bIsCutSceneObject)
		{
			gpDisplayObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_DEBUG, false);
		}
		else
		{
			if (pModelInfo->GetModelType() == MI_TYPE_COMPOSITE)
			{
				gpDisplayObject = rage_new CCompEntity( ENTITY_OWNEDBY_DEBUG );
				gpDisplayObject->GetLodData().SetLodType(LODTYPES_DEPTH_ORPHANHD);
			}
			else if(pModelInfo->GetClipDictionaryIndex() != -1 && pModelInfo->GetHasAnimation())
			{
				gpDisplayObject = rage_new CAnimatedBuilding( ENTITY_OWNEDBY_DEBUG );
			}
			else
			{
				gpDisplayObject = rage_new CBuilding( ENTITY_OWNEDBY_DEBUG );
			}
			
			gpDisplayObject->SetModelId(modelId);
		}
	}

	FixupObject(gpDisplayObject, testMat);

	if (gpDisplayObject != NULL)
	{
		gpDisplayObject->ProtectStreamedArchetype();
	}
}

void DeleteLegion()
{
	// free old list
	for (s32 i = 0; i < gLegion.GetCount(); ++i)
	{
		if (gLegion[i])
		{
			CGameWorld::Remove(gLegion[i]);

			if(gLegion[i]->GetIsTypeVehicle())
				CVehicleFactory::GetFactory()->Destroy((CVehicle*)gLegion[i].Get());
			else if(gLegion[i]->GetIsTypePed())
				CPedFactory::GetFactory()->DestroyPed((CPed*)gLegion[i].Get());
			else if(gLegion[i]->GetIsTypeObject())
				CObjectPopulation::DestroyObject((CObject*)gLegion[i].Get());
			else
				delete gLegion[i];
		}
	}
	gLegion.Reset();
}

void DisplayLegionObject()
{
    DeleteLegion();

	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromName(gCurrentDisplayObject, &modelId);
	if(pModelInfo==NULL)
	{
		Displayf("Couldn't get model info for '%s'.", gCurrentDisplayObject);
		return;
	}

	if (gpDisplayObject)
		DeleteObject();

	u32 totalCount = gLegionCountX * gLegionCountY;
	gLegion.Reserve(totalCount);

	float totalSpacing = gLegionSpacing + pModelInfo->GetBoundingSphereRadius();
    Vector3 fwd = camInterface::GetFront();
    fwd.z = 0.f;
    fwd.Normalize();
    Vector3 up(0.f, 0.f, 1.f);
    Vector3 side;
	side.Cross(fwd, up);
    side.Normalize();

    fwd *= totalSpacing;
    side *= totalSpacing;

	for (u32 x = 0; x < gLegionCountX; ++x)
	{
		for (u32 y = 0; y < gLegionCountY; ++y)
		{
			gDisplayOffset = ((float)x * fwd) + ((float)y * side);
			DisplayObject(false);
			gLegion.Push(gpDisplayObject);
		}
	}

	gpDisplayObject = NULL;
}


void PlaceLastObjectOnGround()
{
	if(gpDisplayObject && gpDisplayObject->GetIsTypeObject())
		(static_cast<CObject*>(gpDisplayObject.Get()))->PlaceOnGroundProperly();
	else if (gLegion.GetCount() > 0 && gLegion[0]->GetIsTypeObject())
	{
		for (s32 i = 0; i < gLegion.GetCount(); ++i)
			(static_cast<CObject*>(gLegion[0].Get()))->PlaceOnGroundProperly();
	}
}

void RepairLastObject()
{
	if(gpDisplayObject && gpDisplayObject->GetIsTypeObject())
        (static_cast<CObject*>(gpDisplayObject.Get()))->Fix();
    else if (gLegion.GetCount() > 0 && gLegion[0]->GetIsTypeObject())
    {
        for (s32 i = 0; i < gLegion.GetCount(); ++i)
            (static_cast<CObject*>(gLegion[0].Get()))->Fix();
    }
}

bool gCacheTunedFragObject = true;

void FragTuneLastObject()
{
	if(gpDisplayObject)
	{
		if (fragInst* pInst = gpDisplayObject->GetFragInst())
		{
			if (gCacheTunedFragObject && !pInst->GetCached())
			{
				pInst->PutIntoCache();
			}

			CPhysics::CreateFragTuneBank();
			FRAGTUNE->SetType(const_cast<fragType*>(pInst->GetType()), 0, pInst);
		}
	}
}

void SelectDisplayObject()
{
	strncpy(gCurrentDisplayObject, gCurrentDisplayObjectList[gCurrentDisplayObjectIndex], sizeof(gCurrentDisplayObject));
	gCurrentDisplayObject[sizeof(gCurrentDisplayObject) - 1] = '\0';
	DisplayObject(true);
}

void DisplayObjectDeleteOld() { DisplayObject(true); }
void DisplayObjectKeepOld() { DisplayObject(false); }

#endif // __BANK

PARAM(nomsgs, "[debug] Don't output log messages to the console");
PARAM(nowarnings, "[debug] Don't output warnings to the console");
namespace rage {
XPARAM(tracker);
}

void CDebug::SetPrinterFn()
{
	u32 outputMask = diagOutput::OUTPUT_ALL;
	if(PARAM_nowarnings.Get())
		outputMask &= ~diagOutput::OUTPUT_WARNINGS;
	if(PARAM_nomsgs.Get())
		outputMask &= ~diagOutput::OUTPUT_DISPLAYS;
	diagOutput::SetOutputMask(outputMask);
}

#if __ASSERT
namespace rage
{
    //extern void (*g_ExternalPreAssertHandler)(const char *exp,const char *file,unsigned line, const char* message);
    extern bool (*diagExternalCallback)(const diagChannel&,diagSeverity&,const char*,int,const char*);
	extern int (*diagLogBugCallback)(const char*,const char*,u32,int);
}
extern bool (*spuLogBugCallback)(int spu, const char* summary,const char* details, u32 assetId, bool editBug);
extern int (*spuAssertCallback)(int spu, const char* summary, const char* assertText, int defaultIfNoServer);
#endif // __ASSERT

#if __PFDRAW
PARAM(pfdraw, "Memory reserved for RAGE profile draw, i.e. items controlled within the widget bank \"rage - Profile Draw\"");
#endif

XPARAM(bugAsserts);

XPARAM(debugtrain);

#endif // !__FINAL
#endif // ENABLE_CDEBUG
void CDebug::InitSystem(unsigned )
{
#if __ASSERT
	if(PARAM_bugAsserts.Get())
	{
		diagLogBugCallback = OnLogBug;
#if __PS3
		spuAssertCallback = OnSpuAssert;
#endif
	}
#endif
	SetVersionNumber(); 
#if STREAMING_VISUALIZE
	char fullVersion[32];
	STRVIS_SET_EXTRA_CLIENT_INFO(GetFullVersionString(fullVersion, sizeof(fullVersion)), GetPackageType());
#endif // STREAMING_VISUALIZE
}
#if ENABLE_CDEBUG
#if !__FINAL

void CDebug::Init_CDEBUG(unsigned /*initMode*/)
{
	USE_DEBUG_MEMORY();

#if !__FINAL
	ms_currentMissionName[0] = '\0';
	ms_currentMissionTag[0] = '\0';

	DebugDraw::Init();
	CVectorMap::InitClass();

#if RAGE_TIMEBARS
	//g_pfTB.SetTextCallback(&TimerBarTextCallback);
	g_pfTB.SetTimebarDimensions(0.22f, 0.86f, 0.01f, 0.1f);
#endif // RAGE_TIMEBARS

	m_iSavegameVersionId = -1;

	m_iScaleformMovieId = -1;

	CSystem::GetRageSetup()->SetScreenShotNamingConvention( grcSetup::NUMBERED_SCREENSHOTS );

#if __BANK
#if(BUGSTAR_INTEGRATION_ACTIVE) 
	gBugstarOwner[0] = '\0';  // make sure its empty before we create the bank
#endif
	CDebug::m_iBugNumber = -1;

	sm_enableBoundByGPUFlip = PARAM_enableBoundByGPUFlip.Get();
	
	DR_ONLY(ms_GameRecorder = rage_new GameRecorder;);
	DR_ONLY(ms_GameRecorder->Init(););
	
	gDrawListMgr->RegisterStatExtraction(s_numRenderPhases, s_renderPhaseNames, s_renderPhaseData);
	ms_showInvincibleBugger = !PARAM_dontshowinvinciblebugger.Get();

	if (PARAM_peddebugvisualiser.Get())
	{
		int debugvalue = 0;
		PARAM_peddebugvisualiser.Get(debugvalue);
		CPedDebugVisualiser::SetDebugDisplay((CPedDebugVisualiser::ePedDebugVisMode)debugvalue);
	}

	if (PARAM_dontdisplayforplayer.Get())
	{
		CPedDebugVisualiserMenu::ms_menuFlags.m_bDontDisplayForPlayer = true;
	}

	sm_debugrespot = PARAM_debugrespot.Get();
	sm_debugrespawninvincibility = PARAM_debugrespawninvincibility.Get();
	sm_debugdoublekill = PARAM_debugdoublekill.Get();
	sm_debugexplosion = PARAM_debugexplosion.Get();
	sm_debuglicenseplate = PARAM_debuglicenseplate.Get();
	sm_debugaircraftcarrier = PARAM_debugaircraftcarrier.Get();
	sm_debugradiotest = PARAM_debugradiotest.Get();

	CTrain::sm_bDebugTrain = PARAM_debugtrain.Get();

	PARAM_dbgfontscale.Get(sm_dbgFontscale);
#endif // __BANK

#endif // !__FINAL

#if __STATS
	GetRageProfiler().Init();

	const char* defaultEKG = NULL;
	if( PARAM_ekgdefault.Get(defaultEKG))
	{
		pfPage* apiPage = GetRageProfiler().GetPageByName(defaultEKG);
		if( apiPage )
		{
			GetRageProfileRenderer().GetEkgMgr(0)->SetPage(apiPage);
		}	
	}

#endif // __STATS

#if __PFDRAW
	int nPfDrawMem = 16 * 1024;
	PARAM_pfdraw.Get(nPfDrawMem);
	GetRageProfileDraw().Init(nPfDrawMem, true, grcBatcher::BUF_FULL_IGNORE);
	GetRageProfileDraw().SetEnabled(true);
#endif

#if ENABLE_STATS_CAPTURE
	MetricsCapture::Init();
#endif // ENABLE_STATS_CAPTURE

}

//
// name:		CDebug::Shutdown
// description:	Shutdown debug code
void CDebug::Shutdown(unsigned /*shutdownMode*/)
{
	DR_ONLY(delete ms_GameRecorder;);

	CVectorMap::Shutdown();
	CVectorMap::ShutdownClass();
	DebugDraw::Shutdown();

#if __PFDRAW
	GetRageProfileDraw().Shutdown();
#endif

#if __STATS
	GetRageProfiler().Shutdown();
#endif

#if ENABLE_STATS_CAPTURE
	MetricsCapture::Shutdown();
#endif // ENABLE_STATS_CAPTURE
}


// EJ: FPS Capture
#if ENABLE_FPS_CAPTURE
bool g_bFPSCapture = false;
float g_fMaxCaptures = 100.0f;
float g_fCaptureData[1000];
int g_nTotalCaptures = 0;

static void StartFPSCapture()
{
	g_bFPSCapture = true;
}

static void CaptureFPS()
{
	if (!g_bFPSCapture)
		return;

	if (g_nTotalCaptures >= g_fMaxCaptures)
	{
		float fTotalFPS = 0.0f;
		for (int i = 0; i < g_nTotalCaptures; ++i)
			fTotalFPS += g_fCaptureData[i];

		const float fAverageFPS = fTotalFPS / g_nTotalCaptures;
		Displayf(" *** Captures: %d, FPS: %0.3f *** ", g_nTotalCaptures, fAverageFPS);

		g_bFPSCapture = false;
		g_nTotalCaptures = 0;
		return;
	}

	const float fFPS = 1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep());
	g_fCaptureData[g_nTotalCaptures++] = fFPS;
}
#endif

extern void populateCarWithDumbRandomsCB(void);
extern void populateCarWithRandomsCB(void);
void FindEntityFromPointer();
//
// name:		CDebug::Update
// description:	Update debug code
void CDebug::Update_CDEBUG()
{
#if ENABLE_FPS_CAPTURE
	CaptureFPS();
#endif

	PF_START_TIMEBAR("debug update");
	// Update the currently active CTestCase
	CTestCase::UpdateTestCase();

	//if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_D, KEYBOARD_MODE_DEBUG_ALT))
	//{
	//	DisplayObjectDeleteOld();
	//}

#if __BANK
	if (!bkManager::IsEnabled())
	{
		return;
	}

	// Set the fontscale every time since it may have changed via rag
	grcFont::GetCurrent().SetInternalScale(sm_dbgFontscale);

#if WORLD_PROBE_DEBUG
	if(!fwTimer::IsGamePaused())
	{
		WorldProbe::GetShapeTestManager()->ResetPausedRenderBuffer();
	}
#endif // WORLD_PROBE_DEBUG
#endif // __BANK

#if DEBUG_DISPLAY_BAR
	CDebug::UpdateReleaseInfoToScreen();
#endif
	CTask::VerifyTaskCounts();
	CEvent::VerifyEventCounts();

//  dont display the version number here anymore
//	grcDebugDraw::AddDebugOutput("v%s\n", CDebug::GetVersionNumber());  // output a version number

#if !__FINAL
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SEMICOLON, KEYBOARD_MODE_DEBUG, "Toggle LASTGEN"))
	{
		gLastGenModeState = !gLastGenModeState;
	}

	// toggle camera vertical axis - todo 11421 jimmy database
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_Y, KEYBOARD_MODE_DEBUG, "Toggle invert look"))
	{
		// cycle the different inversions.
		s32 pref = CPauseMenu::GetMenuPreference(PREF_INVERT_LOOK);

		if(pref == 0)
		{
			pref = 1;
		}
		else
		{
			pref = 0;
		}

		Displayf("[Debug::Keyboard] User just pressed Y key to toggle invert camera look, now it is %s", pref == 1 ? "inverted" : "not inverted");

		CPauseMenu::SetMenuPreference(PREF_INVERT_LOOK, pref);

		CControlMgr::SetDefaultLookInversions();
	}
	
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_BACK, KEYBOARD_MODE_DEBUG, "Lock Render List"))
	{
		g_render_lock=!g_render_lock;
		if(g_render_lock)
		{
			CSetDrawableLODCalcPos::SetRenderLockCamPos(camInterface::GetPos());		// store current cam position for locked LOD calcs

			if (!fwTimer::IsGamePaused())
			{
				fwTimer::StartUserPause(); //ensure game is paused
			}
			if (!CStreaming::IsStreamingPaused())
			{
				CStreaming::SetStreamingPaused(true);
			}
		}
		else
		{
			if(fwTimer::IsGamePaused())
			{
				fwTimer::EndUserPause();
			}
			if (CStreaming::IsStreamingPaused())
			{
				CStreaming::SetStreamingPaused(false);
			}
		}

		DebugCamera::SetCameraLock(g_render_lock);
	}

#if !__FINAL
	if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_R, KEYBOARD_MODE_DEBUG_ALT, "Toggle GTA/RDR free camera mode") )
	{
		if(camInterface::GetDebugDirector().GetFreeCam())
			camInterface::GetDebugDirector().GetFreeCam()->SetUseRdrMapping(!camInterface::GetDebugDirector().GetFreeCam()->GetUseRdrMapping());
	}
#endif

#if __BANK
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_V, KEYBOARD_MODE_DEBUG_CTRL, "Visualise Peds In Vehicle"))
	{
		CPedDebugVisualiserMenu::ms_menuFlags.m_bVisualisePedsInVehicles = !CPedDebugVisualiserMenu::ms_menuFlags.m_bVisualisePedsInVehicles;
	}

	const bool bPopulateLastVehicleDumb = CControlMgr::GetKeyboard().GetKeyJustDown(KEY_P, KEYBOARD_MODE_DEBUG_SHIFT_ALT, "Populate last vehicle dumb");

	XPARAM(nocheats);

	if( !PARAM_nocheats.Get() &&
		(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_U, KEYBOARD_MODE_DEBUG, "Create car")||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_U, KEYBOARD_MODE_DEBUG_SHIFT, "Create car+warp")||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_U, KEYBOARD_MODE_DEBUG_CTRL, "Create car+populate") ||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_U, KEYBOARD_MODE_DEBUG_ALT, "Create car+populate dumb") ||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_P, KEYBOARD_MODE_DEBUG_SHIFT, "Populate last vehicle") ||
		bPopulateLastVehicleDumb))
	{
		CVehicleFactory::CreateBank();
		if (!CControlMgr::GetKeyboard().GetKeyJustDown(KEY_P, KEYBOARD_MODE_DEBUG_SHIFT, "Populate last vehicle") && 
			!bPopulateLastVehicleDumb)
		{
			CVehicleFactory::CreateCar();
		}
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_U, KEYBOARD_MODE_DEBUG_SHIFT, "Create car+warp"))
		{
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if (pPlayer && CVehicleFactory::ms_pLastCreatedVehicle)
			{
				TUNE_GROUP_INT(VEHICLE_DEBUG, SEAT_TO_WARP_INTO, -1, -1, 16, 1)
				const s32 iSeatIndex = CVehicleFactory::ms_pLastCreatedVehicle->IsSeatIndexValid(SEAT_TO_WARP_INTO) ? SEAT_TO_WARP_INTO : 0;
				pPlayer->GetPedIntelligence()->FlushImmediately(true);
				pPlayer->SetPedInVehicle(CVehicleFactory::ms_pLastCreatedVehicle, iSeatIndex, CPed::PVF_Warp);
				CVehicleFactory::ms_pLastCreatedVehicle->SwitchEngineOn(true);
			}

#if __BANK
			if (PARAM_licensePlate.Get() && CVehicleFactory::ms_pLastCreatedVehicle)
			{
				CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(CVehicleFactory::ms_pLastCreatedVehicle->GetDrawHandler().GetShaderEffect());
				Assert(pShader);
				const char * license = NULL;
				PARAM_licensePlate.Get(license);
				pShader->SetLicensePlateText(license);
			}
#endif 
		}
		else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_U, KEYBOARD_MODE_DEBUG_CTRL, "Create car+populate"))
		{
			CPedVariationDebug::ms_UseDefaultRelationshipGroups = true;
			CPed::CreateBank();
			populateCarWithRandomsCB();
			CPedVariationDebug::ms_UseDefaultRelationshipGroups = false;
		}
		else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_U, KEYBOARD_MODE_DEBUG_ALT, "Create car+populate dumb"))
		{
			CPedVariationDebug::ms_UseDefaultRelationshipGroups = true;
			CPed::CreateBank();
			populateCarWithDumbRandomsCB();
			CPedVariationDebug::ms_UseDefaultRelationshipGroups = false;
		}
		else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_P, KEYBOARD_MODE_DEBUG_SHIFT, "Populate last vehicle"))
		{
			CPedVariationDebug::ms_UseDefaultRelationshipGroups = true;
			CPed::CreateBank();
			populateCarWithRandomsCB();
			CPedVariationDebug::ms_UseDefaultRelationshipGroups = false;
		}
		else if (bPopulateLastVehicleDumb)
		{
			CPedVariationDebug::ms_UseDefaultRelationshipGroups = true;
			CPed::CreateBank();
			populateCarWithDumbRandomsCB();
			CPedVariationDebug::ms_UseDefaultRelationshipGroups = false;
		}
		if (NetworkInterface::IsGameInProgress())
			GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_VEHICLE));
	}

	if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_H, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT, "Create horse+warp") )
	{
		CPed::CreateBank();
		CPedVariationDebug::SetTypeIdByName("A_C_Horse");

		CPedVariationDebug::CreatePedCB();
		if(CPedFactory::GetLastCreatedPed() && 
			CPedFactory::GetLastCreatedPed()->GetCapsuleInfo()->IsQuadruped() && 
			CPedFactory::GetLastCreatedPed()->GetPedModelInfo()->GetLayoutInfo())
		{
			CPed * pPlayerPed = FindPlayerPed();
			if(pPlayerPed)
			{
				pPlayerPed->SetPedOnMount(CPedFactory::GetLastCreatedPed());
			}
		}
	}
	else if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_H, KEYBOARD_MODE_DEBUG_SHIFT_ALT, "Create horse+populate") )
	{
		CPed::CreateBank();

		s32 lastName = CPedVariationDebug::currPedNameSelection;

		CPedVariationDebug::SetTypeIdByName("A_C_Horse");
		CPedVariationDebug::CreatePedCB();

		CPed* pHorse = CPedFactory::GetLastCreatedPed();

		CPedVariationDebug::currPedNameSelection = lastName;
		CPedVariationDebug::CreatePedCB();

		CPed* pRider = CPedFactory::GetLastCreatedPed();
		if (pRider && pHorse)
		{
			pRider->SetPedOnMount(pHorse);
		}
	}

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DECIMAL, KEYBOARD_MODE_DEBUG, "Create ped")||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DECIMAL, KEYBOARD_MODE_DEBUG_SHIFT, "Create ped+next")||
		(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DECIMAL, KEYBOARD_MODE_DEBUG_CTRL, "Create ped+control") && !NetworkInterface::IsGameInProgress()))
	{
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DECIMAL, KEYBOARD_MODE_DEBUG_SHIFT, "Create ped+next"))
		{
			CPedVariationDebug::CycleTypeId();
		}

        Displayf("******************************************************");
        Displayf("CREATING DEBUG PED - PLAYER PRESSED \"Create ped\" KEY");
        Displayf("******************************************************");

		CPed::CreateBank();
		CPedVariationDebug::CreatePedCB();
		
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DECIMAL, KEYBOARD_MODE_DEBUG_CTRL, "Create ped+control"))
		{
			if(CPedFactory::GetLastCreatedPed())
			{
				CPed * pPlayerPed = FindPlayerPed();
				if(pPlayerPed)
				{
					CGameWorld::ChangePlayerPed(*pPlayerPed, *CPedFactory::GetLastCreatedPed());
				}
			}
		}
	}

	TUNE_FLOAT(KILL_HOSTILE_PEDS_RADIUS, 15.0f, 0.0f, 100.0f, 1.0f);
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_K, KEYBOARD_MODE_DEBUG, "Kill Hostile Peds"))
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (pPlayer)
		{
			const Vec3V vPlayerPosition = pPlayer->GetTransform().GetPosition();
			const ScalarV svKillRadius = ScalarVFromF32(KILL_HOSTILE_PEDS_RADIUS);

			s32 PedPoolIndex = CPed::GetPool()->GetSize();
			while (PedPoolIndex--)
			{
				CPed* pPed = CPed::GetPool()->GetSlot(PedPoolIndex);
				if(pPed && !pPed->IsDead())
				{
					if (IsLessThanAll(Dist(pPed->GetTransform().GetPosition(), vPlayerPosition), svKillRadius))
					{
						bool bShouldKill = false;

						const CRelationshipGroup* pRelGroup = pPed->GetPedIntelligence()->GetRelationshipGroup();
						const int playerRelationshipGroupIndex = pPlayer->GetPedIntelligence()->GetRelationshipGroupIndex();
						if( pRelGroup)
						{
							if( pRelGroup->CheckRelationship( (eRelationType)ACQUAINTANCE_TYPE_PED_WANTED, playerRelationshipGroupIndex ) 
								|| pRelGroup->CheckRelationship( (eRelationType)ACQUAINTANCE_TYPE_PED_HATE, playerRelationshipGroupIndex ))
							{
								bShouldKill = true;
							}
						}

						if (pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == pPlayer)
						{
							bShouldKill = true;
						}

						if (bShouldKill && !pPed->IsNetworkClone())
						{
							pPed->SetHealth(0.0f);
						}
					}
				}
			}
		}
	}
#endif

#if __BANK
	

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_R, KEYBOARD_MODE_DEBUG_SHIFT, "display ped population multipliers"))
	{
		CPedPopulation::ms_bDisplayMultipliers = !CPedPopulation::ms_bDisplayMultipliers;
	}
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_R, KEYBOARD_MODE_DEBUG, "display paths"))
	{
		static bool bDisplayPaths = false;
		
		// 3 stage vehicle population debug - Paths/Mulipliers, Multipliers Only, Off
		bool displayMultipliers = ThePaths.bDisplayMultipliers;
		bool displayPaths = ThePaths.bDisplayPathsDebug_Allow;

		if(!bDisplayPaths)
		{
			// turn on everything
			bDisplayPaths = true;
			
			displayMultipliers = true;	
			displayPaths = true;

			CTrain::sm_bDisplayTrainAndTrackDebug = true;
		}
		else if(displayMultipliers && displayPaths)
		{
			// turn off paths, but leave on multipliers
			displayPaths = false;
			displayMultipliers = true;

			CTrain::sm_bDisplayTrainAndTrackDebug = false;
		}
		else
		{
			// turn everything off
			bDisplayPaths = false;

			displayMultipliers = false;
			displayPaths = false;

			CTrain::sm_bDisplayTrainAndTrackDebug = false;
		}
		
		ThePaths.bDisplayPathsDebug_Allow = displayPaths;
		ThePaths.bDisplayMultipliers = displayMultipliers;
		ThePaths.bDisplayPathsDebug_Nodes_AllowDebugDraw = displayPaths;
		ThePaths.bDisplayPathsDebug_Links_AllowDebugDraw = displayPaths;
	}
#endif

#if __BANK
	if (CTrain::sm_bDebugTrain)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG))
		{
			CTrain::PerformDebugTeleportEngineCB();
		}
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG_SHIFT))
		{
			CTrain::PerformDebugTeleportCB();
		}
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG_ALT))
		{
			CTrain::PerformDebugTeleportToRoof();
		}
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG_CTRL))
		{
			CTrain::PerformDebugTeleportIntoSeatCB();
		}		
	}

	if (sm_debugrespot)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG))
		{
			CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
			if (pLocalPlayer && pLocalPlayer->GetIsDrivingVehicle())
			{
				CVehicle* pVehicle = pLocalPlayer->GetMyVehicle();
				if (pVehicle)
				{
					pVehicle->SetRespotCounter(5000);
				}
			}
		}
	}

	if (sm_debugrespawninvincibility)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG))
		{
			CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
			if (pLocalPlayer && pLocalPlayer->GetNetworkObject())
			{
				CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(pLocalPlayer->GetNetworkObject());
				if (pNetObjPlayer)
				{
					pNetObjPlayer->SetRespawnInvincibilityTimer(5000);
				}
			}
		}
	}

	if (sm_debugdoublekill)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_K, KEYBOARD_MODE_DEBUG))
		{
			CPed *pPedLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
			if (pPedLocalPlayer && pPedLocalPlayer->GetPlayerInfo())
			{
				//Just enough to keep the player alive -- 100 is the baseline
				pPedLocalPlayer->SetHealth(101.f);

				//Fire at the closest player
				Vector3 vecTarget(Vector3::ZeroType);
				CEntity* pEntity = pPedLocalPlayer->GetPlayerInfo()->GetTargeting().FindLockOnTarget();
				CPed* pPedVictim = NULL;
				if (pEntity && pEntity->GetIsTypePed())
					pPedVictim = static_cast<CPed*>(pEntity);

				if (pPedVictim)
				{
					pPedVictim->GetBonePosition(vecTarget, BONETAG_HEAD);

					Vector3 vecStart, vecEnd;

					CWeapon* pEquippedWeapon = pPedLocalPlayer->GetWeaponManager()->GetEquippedWeapon();
					CObject* pWeaponObject = pPedLocalPlayer->GetWeaponManager()->GetEquippedWeaponObject();
					if( pEquippedWeapon && pWeaponObject )
					{
						Matrix34 matWeapon;
						if(pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ) != -1)
						{
							pWeaponObject->GetGlobalMtx( pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ), matWeapon );
						}
						else
						{
							pWeaponObject->GetMatrixCopy( matWeapon );
						}

						// Check to see if the target vector is valid
						bool bvecTargetIsZero = vecTarget.IsZero();
						if( !bvecTargetIsZero )
						{
							// calculate the END of the firing vector using the specific position
							pEquippedWeapon->CalcFireVecFromPos(pPedLocalPlayer, matWeapon, vecStart, vecEnd, vecTarget);

							// fire the weapon
							CWeapon::sFireParams params( pPedLocalPlayer, matWeapon, &vecStart, &vecTarget );
							params.iFireFlags.SetFlag( CWeapon::FF_ForceBulletTrace );
							params.iFireFlags.SetFlag( CWeapon::FF_SetPerfectAccuracy );
							pEquippedWeapon->Fire( params );
						}
					}
				}
			}
		}
	}

	if (sm_debugexplosion)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG))
		{
			CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
			if (pLocalPlayer)
			{
				CExplosionManager::CExplosionArgs explosionArgs(EXP_TAG_GRENADE, VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()));

				explosionArgs.m_sizeScale = 50.0f;
				explosionArgs.m_bMakeSound = true;
				explosionArgs.m_bNoFx = false;
				explosionArgs.m_fCamShake = false;

				CExplosionManager::AddExplosion(explosionArgs);	
			}
		}
	}

	if (sm_debuglicenseplate)
	{
		if (socialclub_commands::CommandCheckLicensePlate_IsValid(sm_token))
		{
			if (!socialclub_commands::CommandCheckLicensePlate_IsPending(sm_token))
			{
				if (socialclub_commands::CommandCheckLicensePlate_PassedValidityCheck(sm_token))
				{
					sm_token = 0;
					Displayf("debuglicenseplate--PASSED");
				}
				else
				{
					sm_token = 0;
					Displayf("debuglicenseplate--FAILED");
				}
			}
		}
		else if (sm_token == 0)
		{
			if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG))
			{
				CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
				if (pLocalPlayer && pLocalPlayer->GetIsDrivingVehicle())
				{
					CVehicle* pVehicle = pLocalPlayer->GetMyVehicle();
					if (pVehicle)
					{
						CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
						if (pShader)
						{
							const char* license = pShader->GetLicensePlateText();
							if (license)
							{
								Displayf("debuglicenseplate--PLATE[%s]-->invoke CommandCheckLicensePlate",license);
								sm_token = 1;
								socialclub_commands::CommandCheckLicensePlate(license, sm_token);
							}
						}
					}
				}
			}
		}
	}

	if (sm_debugaircraftcarrier)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG))
		{
			Displayf("SPAWNED aircraft carrier");
			streaming_commands::RequestIpl("hei_carrier");
			streaming_commands::RequestIpl("hei_Carrier_int1");
			streaming_commands::RequestIpl("hei_Carrier_int2");
			streaming_commands::RequestIpl("hei_Carrier_int3");
			streaming_commands::RequestIpl("hei_Carrier_int4");
			streaming_commands::RequestIpl("hei_Carrier_int5");
			streaming_commands::RequestIpl("hei_Carrier_int6");
		}
	}

#if NA_RADIO_ENABLED
	if (sm_debugradiotest)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG_SHIFT))
		{
			Displayf("RESET SetOverrideSpecatedVehicleRadio");
			NetworkInterface::SetOverrideSpectatedVehicleRadio(false);
		}
		else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG))
		{
			CVehicle* pVehicle = NULL;

			bool bLocalPlayer = false;
			CPed *pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
			if (pPlayer && pPlayer->GetMyVehicle())
			{
				bLocalPlayer = true;
				pVehicle = pPlayer->GetMyVehicle();
			}		
			else
			{
				pPlayer = CGameWorld::FindFollowPlayer();
				if (pPlayer && pPlayer->GetMyVehicle())
					pVehicle = pPlayer->GetMyVehicle();
			}

			if(pVehicle && pVehicle->GetVehicleAudioEntity())
			{
				if (!bLocalPlayer)
					NetworkInterface::SetOverrideSpectatedVehicleRadio(true);

				if (pVehicle->GetVehicleAudioEntity()->IsRadioSwitchedOn())
				{
					Displayf("%s : SET RADIO OFF",bLocalPlayer ? "LOCALPLAYER" : "FOLLOWPLAYER : SET SetOverrideSpectatedVehicleRadio");
					
					pVehicle->GetVehicleAudioEntity()->SetRadioOffState(true);
				}
				else
				{
					Displayf("%s : SET RADIO ON : SET RADIO STATION COUNTRY",bLocalPlayer ? "LOCALPLAYER" : "FOLLOWPLAYER : SET SetOverrideSpectatedVehicleRadio");

					pVehicle->GetVehicleAudioEntity()->SetRadioOffState(false);

					pVehicle->GetVehicleAudioEntity()->SetScriptRequestedRadioStation();
					pVehicle->GetVehicleAudioEntity()->SetRadioStation(audRadioStation::FindStation("RADIO_06_COUNTRY"));
				}
			}
		}
	}
#endif

#endif

#if __DEV
	TUNE_BOOL(ENABLE_MOVE_NETWORK_RELOAD_PRESSING_L, false);
	if (ENABLE_MOVE_NETWORK_RELOAD_PRESSING_L && CControlMgr::GetKeyboard().GetKeyJustDown(KEY_L, KEYBOARD_MODE_DEBUG, "Reload Move Networks"))
	{
		CAnimViewer::ReloadAllNetworks();
	}
#endif // __DEV

#if __BANK
	TUNE_GROUP_FLOAT(VEHICLE_ON_SIDE_DEBUG, RAISE_AMOUNT, 1.0f, 0.0f, 5.0f, 0.1f);
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F7, KEYBOARD_MODE_DEBUG, "force player car upside down"))
	{
		if (CVehicle* pVehicle = FindPlayerVehicle())
		{
			Matrix34	carMat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
			Matrix34	carOriginalMat = carMat;
			carMat.c = -carMat.c;
			carMat.a = -carMat.a;
			carMat.d.z += RAISE_AMOUNT;
			pVehicle->SetMatrix(carMat);
			pVehicle->UpdateGadgetsAfterTeleport(carOriginalMat);
		}
	}
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F7, KEYBOARD_MODE_DEBUG_CTRL, "force player car on left side"))
	{
		if (CVehicle* pVehicle = FindPlayerVehicle())
		{
			Matrix34	carMat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
			Matrix34	carOriginalMat = carMat;
			carMat.RotateLocalY(-HALF_PI);
			carMat.d.z += RAISE_AMOUNT;
			pVehicle->SetMatrix(carMat);
			pVehicle->UpdateGadgetsAfterTeleport(carOriginalMat);
		}
	}
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F7, KEYBOARD_MODE_DEBUG_ALT, "force player car on right side"))
	{
		if (CVehicle* pVehicle = FindPlayerVehicle())
		{
			Matrix34	carMat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
			Matrix34	carOriginalMat = carMat;
			carMat.RotateLocalY(HALF_PI);
			carMat.d.z += 1.0f;
			pVehicle->SetMatrix(carMat);
			pVehicle->UpdateGadgetsAfterTeleport(carOriginalMat);
		}
	}
#endif

#if __BANK
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG, "reload all shaders"))
	{
		CDrawListMgr::RequestShaderReload();
	}

    if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_N, KEYBOARD_MODE_DEBUG_CTRL, "Display network info"))
    {
        NetworkDebug::ToggleDisplayObjectInfo();
    }

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_V, KEYBOARD_MODE_DEBUG_CTRL, "Display network visibility info"))
	{
		NetworkDebug::ToggleDisplayObjectVisibilityInfo();
	}

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_B))  // toggle timer bars with B
	{
		gDisplayTimerBars = !gDisplayTimerBars;
	}

#if DR_ENABLED
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_D, KEYBOARD_MODE_DEBUG_CTRL, "Toggle debug recorder display"))
	{
		ms_GameRecorder->ToggleDisplay();
	}
#endif
#endif

#if __BANK
#if RSG_PS3
	s32 keyCutsceneLighting = KEY_KANJI; // equivalent to KEY_GRAVE (keyboard grave accent and tilde)
#else
	s32 keyCutsceneLighting = KEY_GRAVE;
#endif

	if (CControlMgr::GetKeyboard().GetKeyJustDown(keyCutsceneLighting, KEYBOARD_MODE_DEBUG, "Cutscene Lighting Budget"))
	{
		DebugLights::CycleCutsceneDebugInfo();
	}
#endif // __BANK
#if __BANK
	// Verbose failmark
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SPACE, KEYBOARD_MODE_DEBUG_SHIFT, "Add a verbose fail mark"))
	{
		grcDebugDraw::Text(0.f, 0.f, Color_blue, "FAILMARK", false, 3.0f, 3.0f, 120); //display on-screen visual notification to the user that this is happening - so they get some feedback. lavalley
		NetworkDebug::AddFailMark("FAILMARK");
	}
#endif

#endif

	// Call CDebugScene::CheckMouse() even in BankRelease builds.
	// But in BankRelease *only* if mouse has been clicked, as there's some overhead
	// and we don't want to skew profile results by calling it ever frame.
	// In __DEV builds this is called every frame to update WhatAmILookingAt, etc.
#if !__FINAL
	CDebugScene::CheckMouse();
#endif

#if __DEV
	
	if(gpDisplayObject)
	{
		CDebugScene::PrintInfoAboutEntity(gpDisplayObject);
	}

#if __DEV
	if (gDisplayMapZone)
	{
		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		Vector3 vecCamPos = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CGameWorld::FindLocalPlayerCoors();

		const char* areaName = CMapAreas::GetAreaNameFromPosition(vecCamPos);
		grcDebugDraw::AddDebugOutput(areaName);
		grcDebugDraw::AddDebugOutput("\n");
	}
#endif // __DEV

	if (C2dEffect::m_PrintStats)
	{
		C2dEffect::PrintDebugStats();
	}

#if __BANK
	C2dEffectDebugRenderer::Update();
#endif	//__BANK

	#if __D3D && __WIN32PC && 0
		if (showd3dresourceinfo==true)
		{
#if 0
			D3DRTYPE_SURFACE                =  1,
			D3DRTYPE_VOLUME                 =  2,
			D3DRTYPE_TEXTURE                =  3,
			D3DRTYPE_VOLUMETEXTURE          =  4,
			D3DRTYPE_CUBETEXTURE            =  5,
			D3DRTYPE_VERTEXBUFFER           =  6,
			D3DRTYPE_INDEXBUFFER            =  7,

			BOOL    bThrashing;             /* indicates if thrashing */
			DWORD   ApproxBytesDownloaded;  /* Approximate number of bytes downloaded by resource manager */
			DWORD   NumEvicts;              /* number of objects evicted */
			DWORD   NumVidCreates;          /* number of objects created in video memory */
			DWORD   LastPri;                /* priority of last object evicted */
			DWORD   NumUsed;                /* number of objects set to the device */
			DWORD   NumUsedInVidMem;        /* number of objects set to the device, which are already in video memory */
			// Persistent data
			DWORD   WorkingSet;             /* number of objects in video memory */
			DWORD   WorkingSetBytes;        /* number of bytes in video memory */
			DWORD   TotalManaged;           /* total number of managed objects */
			DWORD   TotalBytes;             /* total number of bytes of managed objects */
#endif
			grcDebugDraw::AddDebugOutput("TX TH:%d DL:%d EV:%d VC:%d LP:%d US:%d/%d OBJ:%d/%d MEM:%d/%d\n", CSystem::ms_d3dResourceData.stats[3].bThrashing, CSystem::ms_d3dResourceData.stats[3].ApproxBytesDownloaded, CSystem::ms_d3dResourceData.stats[3].NumEvicts, CSystem::ms_d3dResourceData.stats[3].NumVidCreates, CSystem::ms_d3dResourceData.stats[3].LastPri, CSystem::ms_d3dResourceData.stats[3].NumUsed, CSystem::ms_d3dResourceData.stats[3].NumUsedInVidMem, CSystem::ms_d3dResourceData.stats[3].WorkingSet, CSystem::ms_d3dResourceData.stats[3].TotalManaged, CSystem::ms_d3dResourceData.stats[3].WorkingSetBytes, CSystem::ms_d3dResourceData.stats[3].TotalBytes);
			grcDebugDraw::AddDebugOutput("VB TH:%d DL:%d EV:%d VC:%d LP:%d US:%d/%d OBJ:%d/%d MEM:%d/%d\n", CSystem::ms_d3dResourceData.stats[6].bThrashing, CSystem::ms_d3dResourceData.stats[6].ApproxBytesDownloaded, CSystem::ms_d3dResourceData.stats[6].NumEvicts, CSystem::ms_d3dResourceData.stats[6].NumVidCreates, CSystem::ms_d3dResourceData.stats[6].LastPri, CSystem::ms_d3dResourceData.stats[6].NumUsed, CSystem::ms_d3dResourceData.stats[6].NumUsedInVidMem, CSystem::ms_d3dResourceData.stats[6].WorkingSet, CSystem::ms_d3dResourceData.stats[6].TotalManaged, CSystem::ms_d3dResourceData.stats[6].WorkingSetBytes, CSystem::ms_d3dResourceData.stats[6].TotalBytes);
			grcDebugDraw::AddDebugOutput("IB TH:%d DL:%d EV:%d VC:%d LP:%d US:%d/%d OBJ:%d/%d MEM:%d/%d\n", CSystem::ms_d3dResourceData.stats[7].bThrashing, CSystem::ms_d3dResourceData.stats[7].ApproxBytesDownloaded, CSystem::ms_d3dResourceData.stats[7].NumEvicts, CSystem::ms_d3dResourceData.stats[7].NumVidCreates, CSystem::ms_d3dResourceData.stats[7].LastPri, CSystem::ms_d3dResourceData.stats[7].NumUsed, CSystem::ms_d3dResourceData.stats[7].NumUsedInVidMem, CSystem::ms_d3dResourceData.stats[7].WorkingSet, CSystem::ms_d3dResourceData.stats[7].TotalManaged, CSystem::ms_d3dResourceData.stats[7].WorkingSetBytes, CSystem::ms_d3dResourceData.stats[7].TotalBytes);
		}
	#endif
#endif
		// object placement bindings
#if __BANK
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_P, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
		{
			DisplayObjectAndPlaceOnGround();
		}
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_O, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
		{
			DeleteObject();
		}
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_L, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
		{
			FragTuneLastObject();
		}
#endif // __BANK

#if __BANK
		if (PARAM_debugstart.Get() && !m_debugParamUsed && g_SceneToGBufferPhaseNew)
		{
			float params[16];
			PARAM_debugstart.GetArray(&params[0], 16);
			SetDebugParams(params, 16);
			m_debugParamUsed = true;
		}

#endif

#if DR_ENABLED
	if (ms_GameRecorder)
	{
		ms_GameRecorder->SetRecorderFrameCount( fwTimer::GetFrameCount() );
		ms_GameRecorder->SetRecorderNetworkTime( NetworkInterface::IsNetworkOpen() ? NetworkInterface::GetNetworkTime() : 0 );	}
#endif // DR_ENABLED

#if DEBUG_BANK
	FindEntityFromPointer();
#endif

	// QA debug physics rendering
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_P, KEYBOARD_MODE_DEBUG_CTRL))
	{
		phMaterialDebug::ToggleQAMode();
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_C, KEYBOARD_MODE_DEBUG_CTRL))
	{
		phMaterialDebug::ToggleRenderNonClimbableCollision();
	}

	// debug particle effect rendering
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F, KEYBOARD_MODE_DEBUG_CTRL))
	{
		ptxDebug::ToggleQuickDebug();
	}

#if !__FINAL
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_I, KEYBOARD_MODE_DEBUG_ALT))
	{
		CInteriorProxy::EnableAndUncapAllInteriors();
	}
#endif

	BUDGET_DRAW();
}

//
// name:		CDebug::RenderUpdate
// description:	
void CDebug::RenderUpdate()
{
#if __XENON && !__FINAL
	TraceThread(sm_bTraceRenderThread, sm_bStartedRenderTrace, "render_trace", sm_iRenderTraceCount);

	// If we're tracing the main thread with -pgolitetrace enabled, automatically trace the render thread next
	if(sm_bPgoLiteTrace && sm_bStartedUpdateTrace)
	{
		sm_bTraceRenderThread = true;
	}
#endif

#if __BANK
	// Process bugstar screenshots
	CBugstarIntegration::ProcessScreenShot();
#endif
}



//
// name:		CDebug::StartWithDebugParams
// description:	
void CDebug::SetDebugParams(float *data, u32 numElements)
{
	if (numElements == 16) // Pos + Full matrix + Hour
	{
		debugLocation debLoc;
		
		CStreaming::SetIsPlayerPositioned(true);
		debLoc.Set(data);
		debLoc.BeamMeUp();
	}
}


//
// name:		CDebug::CameraDump
// description:	
void CDebug::DumpDebugParams()
{
	debugLocation debLoc;
	
	if( debLoc.SetFromCurrent() )
	{
		char buffer[255];
		
		debLoc.WriteOut(buffer);
		
		Displayf("-debugstart=%s", buffer);
	}
	else
	{
		Displayf("[Debug Params] Need to have the debug camera active for this");
	}
}


//
// name:		CDebug::RenderProfiler
// description:	
void CDebug::RenderProfiler()
{
#if __STATS
	if (PFSTATS.IsActive() || GetRageProfileRenderer().AnyEkgsActive())
	{
		PFSTATS.Cull();

		// Draw the ekg graphs.
		GetRageProfileRenderer().Draw();
#if !__FINAL
		if (GetRageProfiler().m_Enabled)
		{
			grcDebugDraw::SetDisplayDebugText(false);
		}
#endif // !__FINAL
	}
#endif // !__STATS
}

#endif

#if !__FINAL

#if __WIN32PC && 0
void CDebug::DebugSystemError(const char* , s32 )
{
	if(GetLastError() != ERROR_SUCCESS)
	{
		char msgBuff[256];
		Assertf(0, ("System error: %s", diagErrorCodes::Win32ErrorToString(GetLastError(), msgBuff, NELEM(msgBuff))));
	}
}
#endif // __WIN32PC

#if ( DEBUG_RENDER_BUG_LIST )
//
// name: RenderBugList
// description:	Render all the bugs stored in the current buglist
void CDebug::RenderTheBugList()
{
	CBugstarIntegration::RenderTheBuglist();
}
#endif // DEBUG_RENDER_BUG_LIST

#if __ASSERT

static sysTimer s_AssertTimer;
static float s_TimeSinceLastAssert = FLT_MAX;
//
// name:		OnAssertionFired
// description:	Callback for when an assertion is about to fire
static bool OnAssertionFired(const diagChannel &channel,diagSeverity &severity,const char*,int,const char*message)
{
    if(severity <= channel.PopupLevel)
    {
		STRVIS_MARK_ASSERT(message);
		(void) message;

		s_TimeSinceLastAssert = s_AssertTimer.GetTimeAndReset();
        if(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && NetworkInterface::IsNetworkOpen())
        {
            static bool flushingLogFiles = false;

            if(!flushingLogFiles)
            {
                flushingLogFiles = true;

                Displayf("Frame %d, Assertion fired, flushing log files...\r\n", fwTimer::GetFrameCount());
		        NetworkInterface::GetPlayerMgr().GetLog().Log("Assertion fired, flushing log files...\r\n");
		        NetworkInterface::GetObjectManager().GetLog().Log("Assertion fired, flushing log files...\r\n");
		        NetworkInterface::GetObjectManager().GetReassignMgr().GetLog().Log("Assertion fired, flushing log files...\r\n");
		        NetworkInterface::GetEventManager().GetLog().Log("Assertion fired, flushing log files...\r\n");
		        NetworkInterface::GetArrayManager().GetLog().Log("Assertion fired, flushing log files...\r\n");
		        NetworkInterface::GetMessageLog().Log("Assertion fired, flushing log files...\r\n");

                if(CTheScripts::GetScriptHandlerMgr().GetLog())
                {
                    CTheScripts::GetScriptHandlerMgr().GetLog()->Log("Assertion fired, flushing log files...\r\n");
                }

#if ENABLE_NETWORK_LOGGING
                CNetwork::OnAssertionFired();
#endif // ENABLE_NETWORK_LOGGING

                flushingLogFiles = false;
            }
        }

#if RSG_PC
		if(!PARAM_nopopups.Get())
		{
			ioKeyboard::FlushKeyboard();
			ioMouse::OnAssert();
		}
#endif // RSG_PC
    }

#if __BANK
	strStreamingEngine::GetLoader().ResetLoadAllBailTimeout();
#endif // __BANK

    // returning true indicates to the channel system that we still want an assertion popup to be displayed
    return true;
}

#endif // __ASSERT
#endif // ENABLE_CDEBUG

#if __ASSERT

static atString s_assertCallstack;
static atString s_minimalAssertCallstack;
static int s_callstackDepth;

static void InitGenerateCallstack()
{
	s_callstackDepth = 0;
}

//
//void CDebug::GenerateCallstack(u32 OUTPUT_ONLY(addr),const char *OUTPUT_ONLY(sym),u32 OUTPUT_ONLY(offset))
static void GenerateCallstack(size_t OUTPUT_ONLY(addr),const char *OUTPUT_ONLY(sym),size_t OUTPUT_ONLY(offset))
{
	char buffer[256];
	formatf(buffer, "%8" SIZETFMT "x - %s+%" SIZETFMT "x\n", addr, sym, offset, NELEM(buffer));
	s_assertCallstack += buffer;

	// Only record first 4 entries on callstack
	if(s_callstackDepth < 4)
	{
		formatf(buffer, "%s\n", sym, NELEM(buffer));
		// replace "(void)" in 360 callstacks with "()"
		char* voidFunc = const_cast<char*>(strstr(buffer, "(void)"));
		if(voidFunc)
			strcpy(voidFunc, "()");

		s_minimalAssertCallstack += buffer;
		s_callstackDepth++;
	}
}

#if RSG_PS3
static void GenerateCallstackSpu(u32 UNUSED_PARAM(frameNumber), u32 lsAddr, u32 UNUSED_PARAM(lsOffset), const char *sym, u32 symOffset)
{
	GenerateCallstack(lsAddr, sym, symOffset);
}
#endif

static atString s_scriptCallstack;
void DumpThreadState(const char* fmt, ...)
{
	char buffer[256];
	va_list args;
	va_start(args,fmt);
	vformatf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	s_scriptCallstack += buffer;
	s_scriptCallstack += "\n";
}

#define LOG_INTRO_SIZE 15*1024
#define LOG_OUTRO_SIZE 185*1024
#define LOG_INBETWEEN_TEXT "\n.\n.\n.\n.\n.\n.\n"
#define LOG_INBETWEEN_TEXT_SIZE 13
#define LOG_BUFFER_SIZE (LOG_INTRO_SIZE + LOG_INBETWEEN_TEXT_SIZE + LOG_OUTRO_SIZE + 1)

char* LoadOutputLog(int& logSize)
{
	USE_DEBUG_MEMORY();

	const char* logFilename = diagChannel::GetLogFileName();
	diagChannel::FlushLogFile();
	fiStream* pLogStream = ASSET.Open(logFilename, "");
	if(pLogStream)
	{
		char* pBuffer = rage_new char[LOG_BUFFER_SIZE];
		char* pPosn = pBuffer;
		int fileSize = pLogStream->Size();
		if(fileSize < LOG_BUFFER_SIZE)
		{
			int count = pLogStream->Read(pPosn, fileSize);
			pPosn += count;
		}
		else
		{
			int count = pLogStream->Read(pPosn, LOG_INTRO_SIZE);
			pPosn += count;
			strcpy(pPosn, LOG_INBETWEEN_TEXT);
			pPosn += LOG_INBETWEEN_TEXT_SIZE;
			pLogStream->Seek(fileSize - LOG_OUTRO_SIZE);
			count = pLogStream->Read(pPosn, LOG_OUTRO_SIZE);
			pPosn += count;
		}
		pLogStream->Close();

		// temporarily save out to file to see what I've got
		//ASSET.CreateLeadingPath("assert.log");
		//fiStream* pStream = ASSET.Create("assert.log", "");
		//pStream->Write(pBuffer, pPosn - pBuffer);
		//pStream->Close();

		logSize = ptrdiff_t_to_int(pPosn - pBuffer);
		*pPosn = '\0';
		return pBuffer;
	}
	return NULL;
}

//
// name:
// description: Callback for after the assert dialog is shown to update/create a bug in bugstar
int CDebug::OnLogBug(const char* summary, const char* details, u32 assertId, int dialogResult)
{
	USE_DEBUG_MEMORY();

	// Generate a callstack
	s_assertCallstack = "";
	s_minimalAssertCallstack = "";

	InitGenerateCallstack();
	sysStack::PrintStackTrace(GenerateCallstack, __XENON ? 4 : 3);

	// Extend the assert id with the callstack
	assertId = atStringHash(s_minimalAssertCallstack.c_str(), assertId);

	OnLogBug(summary, details, s_assertCallstack.c_str(), assertId, (dialogResult == IDCUSTOM4));

	// Modify the result to something appropriate if IDCUSTOM4 was returned
	if (dialogResult == IDCUSTOM4)
	{
		dialogResult = fiRemoteShowMessageBox(summary,diagAssertTitle?diagAssertTitle:sysParam::GetProgramName(), MB_ABORTRETRYIGNORE | MB_ICONQUESTION | MB_TOPMOST | MB_DEFBUTTON3 ,dialogResult);
	}

	s_AssertTimer.Reset();
	return dialogResult;
}

#if __PS3

void SpuCallstackFromAssertString(sys_spu_thread_t thread, const char *text, void (*printFn)(u32,u32,u32,const char*,u32));

static struct {
	int m_spu;
	const char* m_summary;
	const char* m_details;
	u32 m_assertId;
	bool m_showBug;
} s_spuBugDetails;

//
// name:
// description: Callback for after the assert dialog is shown to update/create a bug in bugstar
int CDebug::OnSpuAssert(int spu, const char* summary, const char* assertText, int defaultIfNoServer)
{
	USE_DEBUG_MEMORY();

	s_TimeSinceLastAssert = s_AssertTimer.GetTimeAndReset();

	int answer = fiRemoteShowAssertMessageBox(assertText,diagAssertTitle? diagAssertTitle : sysParam::GetProgramName(),MB_ABORTRETRYIGNORE | MB_ICONQUESTION | MB_TOPMOST | MB_DEFBUTTON3,defaultIfNoServer);

	// Generate a callstack
	s_assertCallstack = "";
	s_minimalAssertCallstack = "";

	InitGenerateCallstack();
	SpuCallstackFromAssertString(spu, summary, GenerateCallstackSpu);

	// Extend the assert id with the callstack
	u32 assertId = atStringHash(s_minimalAssertCallstack.c_str(), atStringHash(summary));

	const bool showBug = (answer == IDCUSTOM4);
	OnLogBug(summary, summary, s_assertCallstack.c_str(), assertId, showBug);

	// Modify the result to something appropriate if IDCUSTOM4 was returned
	if (answer == IDCUSTOM4)
	{
		answer = fiRemoteShowMessageBox(assertText,diagAssertTitle?diagAssertTitle:sysParam::GetProgramName(),MB_ABORTRETRYIGNORE | MB_ICONQUESTION | MB_TOPMOST | MB_DEFBUTTON3 | MB_SETFOREGROUND,defaultIfNoServer);
	}
	else
		answer = IDIGNORE;

	s_AssertTimer.Reset();
	return answer;
}

#endif // __PS3

sysCriticalSectionToken s_OnLogBugCS;

bool CDebug::OnLogBug(const char* summary, const char* details, const char* callstack, u32 assertId, bool showBug)
{
	SYS_CS_SYNC(s_OnLogBugCS);

	// On PC, bugs are queued before some systems have fully initialised, so throwing an assert
	// causes another assert, ad infinitum.
	static bool preventingRecursion = false;
	if (preventingRecursion)
		return false;
	else
		preventingRecursion = true;

	Vector3 posn(0.0f, 0.0f, 0.0f);
	if (gVpMan.GetGameViewport() != NULL)
	{
		posn = VEC3V_TO_VECTOR3(gVpMan.GetGameViewport()->GetGrcViewport().GetCameraPosition());
	}

	int missionId = 0;
	if(CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsOnARandomEvent() )
	{
		missionId = CMissionLookup::GetBugstarId(CDebug::GetCurrentMissionName());
	}

	// Commented out as there isn't currently a way to query bugstar via rest for a version id based on a name
	int buildId = 0;
	if (GetVersionNumber())
	{
		buildId = CBugstarIntegration::GetBuildLookup().GetBugstarId(GetVersionNumber());
	}
	int mapId = 0;
	const char* levelBugstarName = CScene::GetLevelsData().GetBugstarName(CGameLogic::GetRequestedLevelIndex());
	if(levelBugstarName)
	{
		mapId = CBugstarIntegration::GetMapLookup().GetBugstarId(levelBugstarName);
	}
/*
	// Commented out as there isn't currently a way to query bugstar via rest for a grid id based on a name
	if (CBlockView::GetNumberOfBlocks())
		formatf(tmp,sizeof(tmp),"%s",(const char*)CBlockView::GetBlockName(CBlockView::GetCurrentBlockInside()));
	else
		tmp[0] = '\0';
*/

	// bugstar summary is limited to 255 characters
	char bugSummary[255];
	formatf(bugSummary, sizeof(bugSummary), "%s", summary);

	s_scriptCallstack = "";
	if(scrThread::GetActiveThread())
	{
		scrThread::GetActiveThread()->PrintStackTrace(DumpThreadState);
	}

	static char fullCallstack[2048];
	formatf(fullCallstack, sizeof(fullCallstack), "%sCallstack:\n%s", s_scriptCallstack.c_str(), callstack);

	//const char* log = GetOutputLog();
	int logSize;
	char* logBuffer = LoadOutputLog(logSize);

#if __WIN32PC && RSG_DXDIAG_ENABLED
	int dxDiagSize;
	char* dxDiagBuffer;
	DXDiag::DumpDXDiagByCategory(&dxDiagBuffer, DXDiag::System | DXDiag::Display);
	dxDiagSize = istrlen(dxDiagBuffer);
#endif

	Displayf("Assert ID: %d", assertId);

	CBugstarAssertUpdateQuery::BugDesc bugDesc;
	bugDesc.assertId = assertId;
	bugDesc.logBuffer = logBuffer;
	bugDesc.logSize = logSize;
	bugDesc.position = posn;
	bugDesc.buildId = buildId;
	bugDesc.mapId = mapId;
	bugDesc.missionId = missionId;
	bugDesc.timeSinceLastBug = s_TimeSinceLastAssert;

#if __WIN32PC
#if RSG_DXDIAG_ENABLED
	bugDesc.dxDiagBuffer = dxDiagBuffer;
	bugDesc.dxDiagSize = dxDiagSize;
#else
	bugDesc.dxDiagBuffer = NULL;
	bugDesc.dxDiagSize = 0;
#endif
#endif

	char const* gamerTag = NULL;
	CPed* player = CGameWorld::FindLocalPlayer();
	if(player && player->GetPlayerInfo())
	{
		gamerTag = player->GetPlayerInfo()->m_GamerInfo.GetName();
	}
	
	CBugstarAssertUpdateQuery* pQuery = rage_new CBugstarAssertUpdateQuery(summary, details, fullCallstack, gamerTag ? gamerTag : "No Gamer Tag", bugDesc, showBug);
	pQuery->AddQueryToQueue();

	preventingRecursion = false;

	return true;
}

#endif // __ASSERT


#if __BANK
// Name			:	JumpToBugCoords
// Purpose		:	parses the bug CSV file, figures out where the XYZ values are and teleports the player
// Parameters	:	none
// Returns		:	Nothing
void CDebug::JumpToBugCoords()
{
	if (m_iBugNumber == -1)
		return;

	bool bFoundBugCoords = false;
	Vector3 vBugPosition;
	char tempString[1024];

	FileHandle fid;
	char* pLine;

	CFileMgr::SetDir("");
	fid = CFileMgr::OpenFile("common:/DATA/Bug List.CSV", "rb");

	if (!CFileMgr::IsValidFileHandle(fid))
	{
		bkMessageBox("Jump To Bug Coords", "Cannot find your bug csv (common:/DATA/Bug List.csv)", bkMsgOk, bkMsgError);

		m_iBugNumber = -1;
		return;
	}

	s32 iTabX = -1;

	bool bFoundAllTabs = false;
	bool bFoundBugNumber = false;
	
	while( !bFoundBugCoords && ((pLine = CFileMgr::ReadLine(fid, false)) != NULL)  )
	{
		if (!bFoundAllTabs)
		{
			// need to find out where the X,Y,Z columns are - so count the commas on the 1st line:
			s32 iCommaCount = 0;

			for (s32 i = 0; i < 1024 && pLine != NULL && !bFoundAllTabs; i++)
			{
				sscanf(pLine, "%s", tempString);

				tempString[5] = '\0';  // terminate so we should have a string of "X,Y,Z"

				if (!strcmp(tempString, "X,Y,Z"))
				{
					// yes we found the string
					iTabX = iCommaCount;  // store number of tabs along the X,Y,Z begins
					bFoundAllTabs = true;
				}

				if (*pLine == ',')
				{
					iCommaCount++;
				}

				pLine++;
			}
		}
		else
		// we have found the tabs so lets go to the bug number and get the coords for it
		{
			s32 NewBugNumber;
			char* pBugNumber = pLine;
			pBugNumber++;
			sscanf(pBugNumber, "%d", &NewBugNumber);

			if (NewBugNumber == CDebug::m_iBugNumber)
			{
				bFoundBugNumber = true;

				// we found the correct bug number so get the XYZ coords:
				s32 iTabCounter = 0;
				for (s32 i = 0; i < 1024 && pLine != NULL; i++)
				{
					while (iTabCounter != iTabX)
					{
						if (*pLine == '"')
						{
							do
							{
								pLine++;
							} while (*pLine != '"');
							iTabCounter++;
							pLine++;
						}

						pLine++;
					}

					if (iTabCounter == iTabX && *pLine == '"')
					{
						sscanf(pLine, "%*c%f%*c%*c%*c%f%*c%*c%*c%f", &vBugPosition.x,&vBugPosition.y,&vBugPosition.z);
						bFoundBugCoords = true;
						break;
					}
				}
			}
		}
	}

	CFileMgr::CloseFile(fid);

	// if we found some bug coords, then teleport player to these positions:
	if (bFoundBugCoords)
	{
		formatf(tempString, "You will now be warped to position %0.2f,%0.2f,%0.2f (Bug #%d)", vBugPosition.x,vBugPosition.y,vBugPosition.z,m_iBugNumber, NELEM(tempString));
		bkMessageBox("Jump To Bug Coords", tempString, bkMsgOk, bkMsgInfo);

		CVehicle *pVehicle = FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true);

		g_SceneStreamerMgr.LoadScene(vBugPosition);
		CPhysics::LoadAboutPosition(RCC_VEC3V(vBugPosition));

		if (pVehicle)
		{
			gRenderListGroup.Clear();  // clear any renderlists as this stuff probably has deleted entities that they are pointing to!
			pVehicle->Teleport(vBugPosition, 0.0f, false);
		}
		else if(AssertVerify(FindPlayerPed()))////will change this to teleport once the ped stuff is okay
		{
			gRenderListGroup.Clear();  // clear any renderlists as this stuff probably has deleted entities that they are pointing to!
			FindPlayerPed()->Teleport(vBugPosition, 0.0f);
		}
	}
	else
	{
		if (bFoundBugNumber)
		{
			formatf(tempString, "Bug #%d does not have any coords!", m_iBugNumber, NELEM(tempString));
		}
		else
		{
			formatf(tempString, "Bug #%d does not exist in your csv file!", m_iBugNumber, NELEM(tempString));
		}
		
		bkMessageBox("Jump To Bug Coords", tempString, bkMsgOk, bkMsgError);
	}

	m_iBugNumber = -1;  // reset to -1;
}
#endif // __BANK

#if !__FINAL
// dump as much useful info as possible about this entity
void CDebug::DumpEntity(const CEntity* pEntity){

	if (pEntity){
		Vector3	pos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		Displayf	("\nDump Entity\n");
		Displayf	("-----------\n");
		Displayf	("Addr: %p\n",pEntity);
		Displayf	("Pos : (%.2f,%.2f,%.2f)\n",pos.GetX(), pos.GetY(), pos.GetZ());

		if (NetworkUtils::IsNetworkClone(pEntity)){
			Displayf("Clone : Yes\n");
		} else {
			Displayf("Clone : No\n");
		}

		if (pEntity->m_nFlags.bInMloRoom){
			Displayf("Inside : Yes\n");
		} else {
			Displayf("Inside : No\n");
		}

		if (pEntity->GetIsTypeDummyObject()){
			Displayf("Dummy Obj: Yes\n");
		} else { 
			Displayf("Dummy Obj: No\n");
		}

		if ( pEntity->GetIsPhysical() && static_cast< const CPhysical* >( pEntity )->GetChildAttachment() )
		{
			Displayf("Attached: Yes\n");
		}

		netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

		if (pNetObj){
			Displayf("Network Obj: %s\n", pNetObj->GetLogName());
		} else { 
			Displayf("Network Obj: No\n");
		}

		if (pEntity->GetIsTypeObject()){
			const CObject* pObject = static_cast<const CObject*>(pEntity);
			Displayf("Is managed proc object: %d\n", pObject->GetIsAnyManagedProcFlagSet());
			Displayf("Object created by %d\n",pObject->GetOwnedBy());
		} else if (pEntity->GetIsTypeVehicle()){
			Displayf("Vehicle created by %d\n",static_cast<const CVehicle*>(pEntity)->PopTypeGet());
		} else if (pEntity->GetIsTypePed()){
			Displayf("Ped created by %d\n", static_cast<const CPed*>(pEntity)->PopTypeGet());
		}

		CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
		Assert(pMI);

		if (pMI){
			Displayf("Name : %s\n",pMI->GetModelName());
			Displayf("Type : %d\n",pMI->GetModelType());
		}
		Displayf	("-<end>-----\n\n");
	}
}
#endif

#endif // ENABLE_CDEBUG

#if !__FINAL && TRACK_COLLISION_TYPE_PAIRS
PARAM(dumpcollisionstats, "Enable dumping of physics collision statistics to network drive");

namespace rage {

extern void DumpCollisionTypePairTable(fiStream* stream);
extern void ClearCollisionTypePairTable();

}

void CDebug::DumpCollisionStatsToNetwork(const char* missionName)
{
	if (PARAM_dumpcollisionstats.Get())
	{
		char userName[64];
		sysGetEnv("USERNAME",userName,sizeof(userName));

		char finalPath[RAGE_MAX_PATH];
		formatf(finalPath, "N:\\RSGEDI\\Studio_Shared\\CollisionStats\\%s\\%s-%s-%d",
			__DATE__ __TIME__,
			userName,
			missionName,
			TIME.GetFrameCount()
			);

		// Normalize it -- invalid characters need to go.
		char* ptr = finalPath + 2;	// We need to skip the first colon.

		while (*ptr)
		{
			if (*ptr == ':' || *ptr == ' ')
			{
				*ptr = '_';
			}

			ptr++;
		}

		ASSET.CreateLeadingPath(finalPath);

		fiStream* stream = ASSET.Create(finalPath, "txt", false);

		if (stream)
		{
			DumpCollisionTypePairTable(stream);
			ClearCollisionTypePairTable();

			stream->Close();
			Displayf("Collision statistics have been sent to the network.");
		}
		else
		{
			Errorf("ERROR writing the collision statistics file to the network");
		}

	}
}
#endif // __FINAL

//////////////////////////////////////////////////////////////////////////
#if DEBUG_DISPLAY_BAR

// this function can hold all the debug stuff that gets directly rendered on release builds
// ie player coords, version numbers etc
bank_float s_object_x = 0.16f;
bank_float s_object_y = 0.925f; 
bank_float s_object_scale = 1.0f;
bank_float s_shadercount_x = 0.16f;
bank_float s_shadercount_y = 0.905f; 
bank_float s_shadercount_scale = 1.0f;

void CDebug::UpdateReleaseInfoToScreen()
{
#if DEBUG_DRAW
 	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F11, KEYBOARD_MODE_DEBUG))
	{
 		s_renderObjects = !s_renderObjects;
 		CPostScanDebug::SetPvsStatsEnabled(s_renderObjects || s_renderShaders);
 	}
 	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F11, KEYBOARD_MODE_DEBUG_SHIFT))
 	{
 		s_renderShaders = !s_renderShaders;
 		CPostScanDebug::SetPvsStatsEnabled(s_renderShaders || s_renderShaders);
 	}
#endif // DEBUG_DRAW
}


dev_float s_fpsBar_x = 0.6f;
dev_float s_fpsBar_y = 0.892f;

#if RAGE_TIMEBARS
static void SmoothTimer(float &dest,float src) {
	const float beta = 1/100.0f;
	const float alpha = 1.0f - beta;

	if (fabsf(dest - src) > 5.0f)
		dest = src;
	else
		dest = alpha * dest + beta * src;
}
#endif


#if DEBUG_PAD_INPUT
//
// name:		CDebug::UpdateDebugScaleformInfo
// description:	updates any DEBUG scaleform movies (currently just the debug pad movie)
void CDebug::UpdateDebugScaleformInfo()
{
	UpdateDebugPadToScaleform();
}



//
// name:		CDebug::RenderDebugScaleformInfo
// description:	renders any DEBUG scaleform movies
void CDebug::RenderDebugScaleformInfo()
{
	if (CControlMgr::IsDebugPadValuesOn())
	{
		if (m_iScaleformMovieId != -1 && CScaleformMgr::IsMovieActive(m_iScaleformMovieId))
		{
			CScaleformMgr::RenderMovie(m_iScaleformMovieId);
		}
	}
}

//
// name:		CDebug::UpdateDebugPadMoviePositions
// description:	updates the debug controller movie position - this is called when the safezone changes, so it adjusts accordingly
void CDebug::UpdateDebugPadMoviePositions()
{
	if (m_iScaleformMovieId != -1 && CScaleformMgr::IsMovieActive(m_iScaleformMovieId))
	{
		Vector2 vPos = Vector2( CMiniMap::GetCurrentMiniMapMaskSize().x + m_vPos.x, m_vPos.y);

		CScaleformMgr::ChangeMovieParams(m_iScaleformMovieId, CHudTools::CalculateHudPosition(vPos, m_vSize, 'L', 'B'), m_vSize, GFxMovieView::SM_ExactFit);
	}
}

//
// name:		CDebug::UpdateDebugPadToScaleform
// description:	updates the controller button debug movie
void CDebug::UpdateDebugPadToScaleform()
{
	if (CControlMgr::IsDebugPadValuesOn())
	{
		if (m_iScaleformMovieId == -1)
		{
			Vector2 vPos = Vector2( CMiniMap::GetCurrentMiniMapMaskSize().x + m_vPos.x, m_vPos.y );

			m_iScaleformMovieId = CScaleformMgr::CreateMovie("CONTROLLER_TEST", CHudTools::CalculateHudPosition(vPos, m_vSize, 'L', 'B'), m_vSize, false);  // fix 1806044 - this needs to persist between sessions
		}
		else
		{
			if ( !CPauseMenu::IsActive() )
			{
			if (CScaleformMgr::IsMovieActive(m_iScaleformMovieId))
			{
				// call shit on it
				CPad *pPad = CControlMgr::GetPlayerPad();
				if (pPad)
				{
					if (pPad->GetLeftShoulder1())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(4);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetLeftShoulder2())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(5);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetRightShoulder1())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(6);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetRightShoulder2())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(7);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetDPadUp())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(8);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetDPadDown())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(9);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetDPadLeft())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(10);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetDPadRight())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(11);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetStart())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(12);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetSelect() ORBIS_ONLY(|| pPad->GetTouchClick()))
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(13);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetButtonSquare())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(14);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetButtonTriangle())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(15);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetButtonCross())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(16);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetButtonCircle())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(17);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetShockButtonL())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(18);
							CScaleformMgr::EndMethod();
						}
					}

					if (pPad->GetShockButtonR())
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_INPUT_EVENT"))
						{
							CScaleformMgr::AddParamInt(19);
							CScaleformMgr::EndMethod();
						}
					}

					s32 XAxis = pPad->GetLeftStickX();
					s32 YAxis = pPad->GetLeftStickY();

					static bool bSentOriginLeft = true;
					static bool bSentOriginRight = true;

					if (!bSentOriginLeft)
					{
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_ANALOG_STICK_INPUT"))
						{
							CScaleformMgr::AddParamInt(1);
							CScaleformMgr::AddParamInt(XAxis);
							CScaleformMgr::AddParamInt(YAxis);
							CScaleformMgr::EndMethod();
						}
					}

					if (XAxis == 0 && YAxis == 0)
						bSentOriginLeft = true;
					else
						bSentOriginLeft = false;

					XAxis = pPad->GetRightStickX();
					YAxis = pPad->GetRightStickY();

					if (!bSentOriginRight)
					{	
						if (CScaleformMgr::BeginMethod(m_iScaleformMovieId, SF_BASE_CLASS_GENERIC, "SET_ANALOG_STICK_INPUT"))
						{
							CScaleformMgr::AddParamInt(0);
							CScaleformMgr::AddParamInt(XAxis);
							CScaleformMgr::AddParamInt(YAxis);
							CScaleformMgr::EndMethod();
						}
					}

					if (XAxis == 0 && YAxis == 0)
						bSentOriginRight = true;
					else
						bSentOriginRight = false;
				}
			}
		}
	}
	}
	else
	{
		if (m_iScaleformMovieId != -1)
		{
			if (CScaleformMgr::RequestRemoveMovie(m_iScaleformMovieId))
			{
				m_iScaleformMovieId = -1;
			}
		}
	}
}
#endif // #if DEBUG_PAD_INPUT

#if DEBUG_FPS
PARAM(fps, "Display frame rate, even in Release builds");

__forceinline static void SmoothFPSTimer (float &dest, float src) 
{
	const float beta = 1.0f/static_cast<float>(DESIRED_FRAME_RATE);
	const float alpha = 1.0f - beta;

	if (fabsf(dest - src) > 5.0f)
		dest = src;
	else
		dest = alpha * dest + beta * src;
}

void CDebug::RenderFpsCounter()
{
	if ( PARAM_fps.Get() )
	{
		//GRC_VIEWPORT_AUTO_PUSH_POP();
		//grcViewport::SetCurrent(grcViewport::GetDefaultScreen());
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);

		static float fLastFPS = static_cast<float>(DESIRED_FRAME_RATE);
		float fFPS = (1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep()));
		SmoothFPSTimer(fLastFPS, fFPS);

		char pTmpBuffer[128];

		// Green/Yellow/Red fps reporting based on how good or miserable things are going. Blue if system GPU reservation is enabled on Durango.
		const float fRefreshRate = static_cast<float>( grcDevice::GetRefreshRate() );
		const float fVSync = static_cast<float>( CSettingsManager::GetInstance().GetSettings().m_video.m_VSync );
		const float fTargetFrameTime = (fVSync > 0.0f) && (fRefreshRate > 0.0f) ? (fVSync / fRefreshRate) : (1.0f / DESIRED_FRAME_RATE);

		const float fDesiredFrameRate = 1.0f / fTargetFrameTime;
		const float fGreenFPSThreshold = fDesiredFrameRate * (fDesiredFrameRate >= 59.0f ? 0.8333f : 0.9f);    // 50 or 27 fps
		const float fYellowFPSThreshold = DESIRED_FRAME_RATE * (fDesiredFrameRate >= 59.0f ? 0.5f : 0.8333f);  // 30 or 20 fps

		const Color32 GREEN_FPS_COLOR = Color32(80,255,80);
		const Color32 YELLOW_FPS_COLOR = Color32(255,255,80);
		const Color32 RED_FPS_COLOR = Color32(255,80,80);

		Color32 fpsColor = (fLastFPS >= fGreenFPSThreshold) ? GREEN_FPS_COLOR : ((fLastFPS >= fYellowFPSThreshold) ? YELLOW_FPS_COLOR : RED_FPS_COLOR);

		// Blue when GPU reservation is enabled
		DURANGO_ONLY( const Color32 BLUE_FPS_COLOR = Color32(64,127,255) );
		DURANGO_ONLY( const bool bGPUReserved = grcDevice::GetNuiGPUReservation() );
		DURANGO_ONLY( fpsColor = bGPUReserved ? BLUE_FPS_COLOR : fpsColor ); 
		const char* pStrExtraMsg = DURANGO_ONLY( bGPUReserved ? " [GPU RESERVATION]" : ) "";

		formatf(pTmpBuffer, "FPS: %04.1f (%4.2fms)%s", fLastFPS, (1.0f/fLastFPS)*1000.0f, pStrExtraMsg);

		const float fXPos = DURANGO_ONLY( bGPUReserved ? 0.413f : ) 0.471f;
		const float fYPos = RSG_PC ? 0.9425f : 0.885f;

		CTextLayout FPSDebugText;
		FPSDebugText.SetScale(Vector2(0.1625f, 0.375f));
		FPSDebugText.SetColor(fpsColor);
		FPSDebugText.SetDropShadow(true);
		FPSDebugText.Render(Vector2(fXPos, fYPos), pTmpBuffer);
		CText::Flush();
	}
}

#endif // DEBUG_FPS


void CDebug::RenderReleaseInfoToScreen()
{
#if __BANK
	if (s_renderObjects)
	{
		const CPostScanDebugSimplePvsStats& stats = CPostScanDebug::GetPvsStats();

		int gbufferCount = stats.m_numEntitiesVis_GBUF;
		int reflectCount = stats.m_numEntitiesVis_REFLECTIONS;
		int shadowCount = stats.m_numEntitiesVis_SHADOWS;
		int drawListCount = gbufferCount + shadowCount + reflectCount;

		char text[256];

		{
			formatf(text, "Objects: %d (Shadow-%d, Reflect-%d - TOTAL %d)", gbufferCount, shadowCount, reflectCount, drawListCount, NELEM(text));
		}


		CTextLayout ReleaseTextLayout;
		Vector2 vTextPos = Vector2(s_object_x, s_object_y);
		Vector2 vTextSize = Vector2(0.12f, 0.32f);
		ReleaseTextLayout.SetScale(&vTextSize);
		ReleaseTextLayout.SetColor(Color_white);
		ReleaseTextLayout.SetWrap(Vector2(0.0f, 0.96f));
		ReleaseTextLayout.SetOrientation(FONT_RIGHT);

		ReleaseTextLayout.Render(&vTextPos, text );		
	}
#endif // __BANK

#if __BANK
	if (s_renderShaders)
	{
		const CPostScanDebugSimplePvsStats& stats = CPostScanDebug::GetPvsStats();

		int totalShaderCount = stats.m_numShadersInPvs_TOTAL;
		int gbufShaderCount = stats.m_numShadersVis_GBUF;
		int fwdAlphaShaderCount = stats.m_numShadersVis_ALPHA;
		int selectedShaderCount = stats.m_numShadersInPvs_SELECTED;

		char text[256];

		fwEntity* pEntity = g_PickerManager.GetSelectedEntity();
		if (pEntity)
		{
			u32 gbuf = s_renderPhaseData[1].drawCalls + s_renderPhaseData[1].skinnedDrawCalls;
			u32 gbufSel = s_renderPhaseData[1].entityDrawCalls;
			u32 drawScene = s_renderPhaseData[0].drawCalls + s_renderPhaseData[0].skinnedDrawCalls;
			u32 drawSceneSel = s_renderPhaseData[0].entityDrawCalls;
			u32 shadows = s_renderPhaseData[2].drawCalls + s_renderPhaseData[2].skinnedDrawCalls;
			u32 shadowsSel = s_renderPhaseData[2].entityDrawCalls;
			formatf(text, "Drawcalls(selected): (Gbuf-%4d(%4d), DrawScene-%4d(%4d), Shadows-%4d(%4d) - TOTAL %4d (G-%d, F-%d, S-%d - T %d)",
				gbuf, gbufSel, drawScene, drawSceneSel, shadows, shadowsSel, s_renderPhaseData[0].totalDrawCalls,
				gbufShaderCount, fwdAlphaShaderCount, selectedShaderCount, totalShaderCount, NELEM(text));
		}
		else
		{
			u32 gbuf = s_renderPhaseData[1].drawCalls + s_renderPhaseData[1].skinnedDrawCalls;
			u32 drawScene = s_renderPhaseData[0].drawCalls + s_renderPhaseData[0].skinnedDrawCalls;
			u32 shadows = s_renderPhaseData[2].drawCalls + s_renderPhaseData[2].skinnedDrawCalls;
			formatf(text, "Drawcalls: (Gbuf-%4d, DrawScene-%4d, Shadows-%4d - TOTAL %4d (%d %d %d)",
				gbuf, drawScene, shadows, s_renderPhaseData[0].totalDrawCalls,
				gbufShaderCount, fwdAlphaShaderCount, totalShaderCount, NELEM(text));
		}

		CTextLayout ReleaseTextLayout;
		Vector2 vTextPos = Vector2(s_shadercount_x, s_shadercount_y);
		Vector2 vTextSize = Vector2(0.12f, 0.32f);
		ReleaseTextLayout.SetScale(&vTextSize);
		ReleaseTextLayout.SetColor(Color_white);
		ReleaseTextLayout.SetWrap(Vector2(0.0f, 0.96f));
		ReleaseTextLayout.SetOrientation(FONT_RIGHT);

		ReleaseTextLayout.Render(&vTextPos, text );		
	}
#endif // __BANK

#if !__FINAL
	if(CStreaming::IsStreamingPaused() && !g_render_lock)
	{
		CTextLayout StreamingDebugText;

		StreamingDebugText.SetScale(Vector2(0.5f, 1.0f));
		StreamingDebugText.SetBackground(TRUE);
		StreamingDebugText.SetBackgroundColor(CRGBA(0,0,0,128));
		StreamingDebugText.SetColor(CRGBA(225,80,80,255));
		StreamingDebugText.Render(Vector2(0.65f, 0.8f), "Streaming Paused");
		CText::Flush();
	}

#if __ASSERT
	if (SceneGeometryCapture::IsCapturingPanorama() ||
		LightProbe::IsCapturingPanorama())
	{
		// don't display nopopups message if we're capturing panorama
	}
#if __BANK
	else if (CFrameDump::IsCapturing())
	{
		// don't display nopopups message if we're capturing cutscenes
	}
#endif // __BANK
	else
	{
		const char* msg = GetNoPopupsMessage(false);

		if (msg)
		{
			CTextLayout PopupDebugText;
			PopupDebugText.SetScale(Vector2(0.5f, 1.0f));
			PopupDebugText.SetColor(CRGBA(173,255,47,255));
			PopupDebugText.Render(Vector2(0.55f, 0.75f), msg);
			CText::Flush();
		}

#if GTA_REPLAY
		msg = CReplayControl::GetRecordingDisabledMessage();

		if (msg)
		{
			CTextLayout replayDebugText;
			replayDebugText.SetScale(Vector2(0.5f, 1.0f));
			replayDebugText.SetColor(CRGBA(255,0,0,255));
			replayDebugText.Render(Vector2(0.002f, 0.87f), msg);
			CText::Flush();
		}
#endif // __GTA_REPLAY
	}
#endif // __ASSERT

#if BUGSTAR_INTEGRATION_ACTIVE
	char const* streamingVideoMessage = CBugstarIntegration::GetStreamingVideoMessage();

	if (streamingVideoMessage != nullptr)
	{
		CTextLayout debugText;
		debugText.SetScale(Vector2(0.5f, 1.0f));
		debugText.SetColor(CRGBA(0xff, 0x00, 0x00, 0xa0));
		debugText.Render(Vector2(0.025f, 0.025f), streamingVideoMessage);
		CText::Flush();
	}
#endif // BUGSTAR_INTEGRATION_ACTIVE

#if LIVE_STREAMING
	if(strStreamingEngine::GetLive().GetPatched())
	{
		CTextLayout StreamingDebugText;

		StreamingDebugText.SetScale(Vector2(0.5f, 1.0f));
		StreamingDebugText.SetColor(CRGBA(225,80,80,255));
		StreamingDebugText.Render(g_previewCoords, "Preview");
		CText::Flush();
	}
#endif // LIVE_STREAMING

#if __BANK || __TOOL

	if ( g_bDisplayTimescale && fwTimer::GetDebugTimeScale() < 1.0f)
	{
		CTextLayout TimerDebugText;
		TimerDebugText.SetScale(Vector2(0.25f, 0.5f));
		TimerDebugText.SetColor(CRGBA(212,255,33,255));
		char timerText[32];
		sprintf(timerText, "Timescale : %f", fwTimer::GetDebugTimeScale());
		TimerDebugText.Render(Vector2(0.55f, 0.25f), timerText);
		CText::Flush();
	}

	if(CutfileNonParseData::m_FileTuningFlags.GetAllFlags() > 0 )
	{
		Vector2 vTuningCoords(0.7f, 0.85f);
		CTextLayout StreamingDebugText;

		StreamingDebugText.SetScale(Vector2(0.5f, 1.0f));
		StreamingDebugText.SetColor(CRGBA(225,80,80,255));
		StreamingDebugText.Render(vTuningCoords, "Tuning");
		CText::Flush();
	}
#endif // __BANK || __TOOL

#if (RAGE_TRACKING || STREAMING_VISUALIZE)

	char	textBuffer[64];
	u32		displayFlags = 0;
	enum
	{
		MEMVIS = 1,
		STREAMVIS
	};

#if RAGE_TRACKING
	if( diagTracker::GetCurrent() )
	{
		displayFlags |= MEMVIS;
	}
#endif	// RAGE_TRACKING
	
#if STREAMING_VISUALIZE
	if(strStreamingVisualize::IsInstantiated())
	{
		displayFlags |= STREAMVIS;
	}
#endif

	if( displayFlags == (MEMVIS + STREAMVIS) )
	{
		formatf(textBuffer, "Mem+Streaming Visualize", NELEM(textBuffer));
	}
	else if( displayFlags == MEMVIS )
	{
		formatf(textBuffer, "Mem Visualize", NELEM(textBuffer));
	}
	else if(displayFlags == STREAMVIS )
	{
		formatf(textBuffer, "Streaming Visualize", NELEM(textBuffer));
	}
	if( displayFlags != 0 )
	{
		if (strStreamingAllocator::GetSloshState())
		{
			CTextLayout StreamingDebugText;
			StreamingDebugText.SetScale(Vector2(0.5f, 1.0f));
			StreamingDebugText.SetColor(CRGBA(225,80,80,255));
			StreamingDebugText.Render(Vector2(0.55f, 0.70f), "Mem Slosh" );
			CText::Flush();
		}

		CTextLayout StreamingVisualiseDebugText;
		StreamingVisualiseDebugText.SetScale(Vector2(0.5f, 1.0f));
		StreamingVisualiseDebugText.SetColor(CRGBA(0,212,135,255));
		StreamingVisualiseDebugText.Render(Vector2(0.55f, 0.75f), textBuffer );
		CText::Flush();
	}

#endif // (RAGE_TRACKING || STREAMING_VISUALIZE)



#if RAGE_TIMEBARS
	bool bDisplayTimers = gDisplayTimerBars;

	if (bDisplayTimers)
	{
		CTextLayout StreamingDebugText;
		StreamingDebugText.SetScale(Vector2(0.2f, 0.3f));
		StreamingDebugText.SetBackground(TRUE);
		StreamingDebugText.SetBackgroundColor(CRGBA(0,0,0,128));

		float fFps = 1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep());
		const float fSignificantAmountOfTime = 2.0f;
		static Vector2 vPos(0.05f, 0.88f);

		vPos.x = s_fpsBar_x;
		vPos.y = s_fpsBar_y;

		static float fLastFPS = 30.0f;
		static float fWaitForMain = 0.0f;
		static float fWaitForRender = 0.0f;
		static float fWaitForGPU = 0.0f;
		static float fEndDraw = 0.0f;
		static float fWaitForDrawSpu = 0.0f;
		static float fFifoStallTimeSpu = 0.0f;

		
		const char astrBound[][32] = { "Update", "Render Thread", "GPU", "GPU Flip", "GPU DrawableSpu", "GPU Bubble" };
		const char* pszLimiter = NULL;

		if (gSmoothPerformanceResults)
		{
			float totalWaitForMain = sm_fWaitForMain + sm_fDrawlistQPop;

			SmoothTimer(fLastFPS, fFps);
			SmoothTimer(fWaitForMain, totalWaitForMain);
			SmoothTimer(fWaitForRender,sm_fWaitForRender);
			SmoothTimer(fWaitForGPU, sm_fWaitForGPU);
			SmoothTimer(fEndDraw, sm_fEndDraw);
			SmoothTimer(fWaitForDrawSpu, sm_fWaitForDrawSpu);
			SmoothTimer(fFifoStallTimeSpu, sm_fFifoStallTimeSpu);
		}
		else
		{
			fLastFPS = fFps;
			fWaitForMain = sm_fWaitForMain + sm_fDrawlistQPop;
			fWaitForRender = sm_fWaitForRender;
			fWaitForGPU = sm_fWaitForGPU;
			fEndDraw = sm_fEndDraw;
			fWaitForDrawSpu = sm_fWaitForDrawSpu;
			fFifoStallTimeSpu = sm_fFifoStallTimeSpu;
		}

		// fWaitForMain also contains GPU time...
		fWaitForMain -= Max(fWaitForGPU,fWaitForDrawSpu,fFifoStallTimeSpu);
		fWaitForMain = Max(fWaitForMain,0.0f);

#if __D3D11 || RSG_ORBIS
		// If fEndDraw (time spent inside Present()) isn't significant enough, ensure we ignore it.
		if (fEndDraw < fSignificantAmountOfTime || !PARAM_enableBoundByGPUFlip.Get())
			fEndDraw = 0.0f;
		fEndDraw = (fEndDraw > fSignificantAmountOfTime) ? fEndDraw : 0.0f;
		float fBoundTime = Max(fWaitForMain,fWaitForRender,fWaitForGPU,fEndDraw);
		int maxIndex = MaximumIndex(fWaitForMain,fWaitForRender,fWaitForGPU,fEndDraw);
		pszLimiter = &astrBound[maxIndex][0];
#else
		float fBoundTime = Max(fWaitForGPU,fEndDraw,fWaitForDrawSpu,fFifoStallTimeSpu);;
		if (fWaitForRender < fWaitForMain)
		{	// Bound by main thread
			pszLimiter = &astrBound[0][0];
			fBoundTime = fWaitForMain;
		}
		else
		{
			if (Max(fEndDraw,fWaitForDrawSpu,fFifoStallTimeSpu) > fSignificantAmountOfTime)
			{	// GPU, Gpu flip, other bound
				int index = MaximumIndex(fWaitForGPU,fEndDraw,fWaitForDrawSpu,fFifoStallTimeSpu);
				pszLimiter = &astrBound[index+2][0];
			}
			else
			{	// Render thread bound
				pszLimiter = &astrBound[1][0];
				fBoundTime = fWaitForRender;
			}
		}
#endif // __D3D11 || RSG_ORBIS

		const float fDesiredFrameRate = DESIRED_FRAME_RATE - 1;

		Color32 col = Color32(80,255,80);

		if (fLastFPS >= fDesiredFrameRate)
		{
			col = Color32(80,255,80);
		}
		else
		{
			col = Color32(255,80,80); StreamingDebugText.SetColor(CRGBA(255,80,80,255));
		}

#if DEBUG_DRAW
		if ( gDisplayTimerBars )
		{
			char buffer[128];
#if __D3D11 || RSG_ORBIS
		formatf(buffer,"FPS: %04.1f (%4.2fms) Bound %04.2f ms by %s", fLastFPS, (1.0f/fLastFPS)*1000.0f, fBoundTime, pszLimiter, NELEM(buffer));
#else
		formatf(buffer,"FPS: %04.1f Bound %04.2f ms by %s", fLastFPS, fBoundTime, pszLimiter, NELEM(buffer));
#endif // __WIN32PC
			grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());
			grcDebugDraw::Text(vPos, Color_white, buffer, true, 1.0f, 1.0f);
			grcDebugDraw::Text(vPos, col, buffer, false, 1.0f, 1.0f);

			float fYInc = ((float)grcSetup::GetFixedWidthFont()->GetHeight()/grcDevice::GetHeight())*1.25f;
			vPos += Vector2(0.0f, fYInc);
			formatf(buffer,"Wait for main Thread %4.2fms",fWaitForMain);
			grcDebugDraw::Text(vPos, Color_white, buffer, true, 1.0f, 1.0f);

			vPos += Vector2(0.0f,fYInc);
			formatf(buffer,"Wait for render thread %4.2fms",fWaitForRender);
			grcDebugDraw::Text(vPos, Color_white, buffer, true, 1.0f, 1.0f);

			vPos += Vector2(0.0f,fYInc);
			formatf(buffer,"Wait for GPU %4.2fms",fWaitForGPU);
			grcDebugDraw::Text(vPos, Color_white, buffer, true, 1.0f, 1.0f);

			grcDebugDraw::TextFontPop();
		}
#else
		(void)fBoundTime;
#endif 
	}
#endif

#endif //!__FINAL
}
#endif // DEBUG_DISPLAY_BAR

//////////////////////////////////////////////////////////////////////////
#if DEBUG_SCREENSHOTS
static bool g_bRequestTakeScreenShot = false;
static bool g_bScreenshotPNG = false;
static bool g_bNextScreenshotPNG = false;
PARAM(screenshotpng,"Take screenshots in PNG format instead of JPG");

static void TakeScreenShot()
{
	if (!g_bRequestTakeScreenShot)
		return;

	g_bRequestTakeScreenShot = false;

	if (PARAM_screenshotpng.Get() || g_bNextScreenshotPNG)
	{
		Displayf("Taking screenshot in PNG format");
		CSystem::GetRageSetup()->TakeScreenShotPNG();
	}
	else
	{
		Displayf("Taking screenshot in JPG format");
		CSystem::GetRageSetup()->TakeScreenShotJPG();
	}

	g_bNextScreenshotPNG = g_bScreenshotPNG;
}

void CDebug::HandleScreenShotRequests()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SYSRQ, KEYBOARD_MODE_DEBUG, "{Take Screenshot") ||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SNAPSHOT, KEYBOARD_MODE_DEBUG, "{Take Screenshot") ||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SYSRQ, KEYBOARD_MODE_MARKETING, "Take screenshot") ||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SNAPSHOT, KEYBOARD_MODE_MARKETING, "Take screenshot"))
	{
		g_bRequestTakeScreenShot = true;
		g_bNextScreenshotPNG = g_bScreenshotPNG;
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SYSRQ, KEYBOARD_MODE_DEBUG_SHIFT, "Take screenshot") ||
		CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SNAPSHOT, KEYBOARD_MODE_DEBUG_SHIFT, "Take screenshot"))
	{
		g_bRequestTakeScreenShot = true;
		g_bNextScreenshotPNG = true;
	}

	// Request the screen shot from the main thread, it'll still get handled from the render thread appropriately.
	// Also when done this way, we can query for the screen shot location from the Update thread where it won't crash the game.
	TakeScreenShot();
}

void CDebug::RequestTakeScreenShot(bool bPng /*= false*/){
	g_bRequestTakeScreenShot = true;
	g_bNextScreenshotPNG = bPng;
}

//////////////////////////////////////////////////////////////////////////
#endif // DEBUG_SCREENSHOTS
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
#if DEBUG_BANK
//////////////////////////////////////////////////////////////////////////

void ForceCrash(void)
{
	__builtin_trap();
}

static char gCurrentWarpPosition[256];
static float gWarpStreamingRadius = 600.0f;

void WarpPlayer(void)
{
	Vector3 vecPos(VEC3_ZERO);
	Vector3 vecVel(VEC3_ZERO);

	Vector3 vecCamPos(VEC3_ZERO);
	Vector3 vecCamFront(VEC3_ZERO);
	float	hour = 0, minute = 0;

	float fHeading = 0.0f;

	// Try to parse the line.
	int nItems = 0;
	{
		//	Make a copy of this string as strtok will destroy it.
		Assertf( (strlen(gCurrentWarpPosition) < 256), "Warp position line is too long to copy.");
		char warpPositionLine[256];
		strncpy(warpPositionLine, gCurrentWarpPosition, 256);

		// Get the locations to store the float values into.
		float* apVals[15] = {
								&vecPos.x, &vecPos.y, &vecPos.z,					//	3
								&fHeading,											//	4
								&vecVel.x, &vecVel.y, &vecVel.z,					//	7
								&vecCamPos.x, &vecCamPos.y, &vecCamPos.z,			//	10
								&vecCamFront.x, &vecCamFront.y, &vecCamFront.z,		//	13
								&hour, &minute										//	15
							};

		// Parse the line.
		char* pToken = NULL;
		const char* seps = " ,\t";
		pToken = strtok(warpPositionLine, seps);
		while(pToken)
		{
			// Try to read the token as a float.
			int success = sscanf(pToken, "%f", apVals[nItems]);
			Assertf(success, "Unrecognized token '%s' in warp position line.",pToken);
			if(success)
			{
				++nItems;
				Assertf((nItems < 16), "Too many tokens in warp position line." );
			}
			pToken = strtok(NULL, seps);
		}
	}

	// If we were only provided with 2 items (x, y), determine the z coord
	if (nItems == 2)
	{
		float fTest = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vecPos.x, vecPos.y);
		vecPos.z = fTest;

		CPhysics::LoadAboutPosition(RCC_VEC3V(vecPos));
		g_SceneStreamerMgr.LoadScene(vecPos);
		gRenderListGroup.Clear();  // clear any renderlists as this stuff probably has deleted entities that they are pointing to!

		bool bFoundPosition = false;
		float fGround = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, vecPos.x, vecPos.y, 1000.0f, &bFoundPosition);
		if(bFoundPosition)
		{
			vecPos.z = fGround;
		}

		FindPlayerPed()->Teleport(vecPos, 0.0f);
	}
	else if(nItems)
	{
		// Prevent double presses coming through from RAG (B*970723)
		if( CWarpManager::IsActive() )
		{
			// Early out, warp is already going
			return;
		}

		bool success = CWarpManager::SetWarp(vecPos, vecVel, fHeading, true, true, gWarpStreamingRadius);

		if( success )
		{
			// B*1391089
			CWarpManager::SetNoLoadScene();

			CPed *pPlayerPed = FindPlayerPed();
			if( pPlayerPed )
			{
				Vector3 pedPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
				float d2 = (vecPos - pedPos).Mag2();
				if(d2 < (500.0f*500.0f))
				{
					CWarpManager::FadeOutAtStart(false);
					CWarpManager::FadeInWhenComplete(false);
				}
			}

			// Some error checking, was originally assumed that B* wouldn't send fields that don't exist, but it does, and init's them to zero! ARRRGGGHHHH!!!
			// B*1041052
			//	Plan:	1..	Check if forward is all zeros, if it is, skip both camera and time of day.
			//			2..	If forward isn't all zero's, normalise
			bool setCamForward = true;
			if(nItems >= 13)
			{
				float	forwardLength = vecCamFront.Mag();
				if( forwardLength <= 0.1f )
				{
					// Assmune zero
					setCamForward = false;
				}
				else
				{
					vecCamFront.Normalize();
				}
				
				if( setCamForward )
				{
					// We must have had a camera pos and orientation
					CWarpManager::SetPostWarpCameraPos(vecCamPos, vecCamFront);
				}
			}
			if(nItems >= 15 && setCamForward)
			{
				// We must have had a time in hours/minutes
				CWarpManager::SetPostWarpTimeOfDay((s32)hour,(s32)minute);
			}
		}
	}
}

static char gFindEntityPointer[11];
void FindEntityFromPointer()
{
	if (strlen(gFindEntityPointer) < 10 || gFindEntityPointer[0] != '0' || gFindEntityPointer[1] != 'x')
		return;

	size_t ptrVal = strtoul(&gFindEntityPointer[2], NULL, 16);
	CEntity* ent = (CEntity*)ptrVal;

	// try our best not to crash
	if (!sysMemAllocator::GetMaster().IsValidPointer(ent) && !sysMemAllocator::GetCurrent().IsValidPointer(ent) && !strStreamingEngine::GetAllocator().IsValidPointer(ent))
		return;
	if (ent->GetType() < 0 || ent->GetType() >= ENTITY_TYPE_TOTAL)
		return;
	if (!ent->IsArchetypeSet())
		return;

	grcDebugDraw::SetDoDebugZTest(false);
	grcDebugDraw::Sphere(ent->GetTransform().GetPosition(), ent->GetBoundSphere().GetRadiusf(), Color_GreenYellow, false);
}

#if ENABLE_CDEBUG
namespace object_commands{float CommandGetObjectFragmentDamageHealth(int ObjectIndex, bool bHealthPercentageByMass);};

void DebugLastObject(void)
{
	if(gpDisplayObject)
	{
		int nObjectIndex = CTheScripts::GetGUIDFromEntity(*((CEntity*)gpDisplayObject.Get()));
		static bool TEST_OBJECT_FRAGMENT_STATE_BYMASS = false;
		float fResult = object_commands::CommandGetObjectFragmentDamageHealth(nObjectIndex, TEST_OBJECT_FRAGMENT_STATE_BYMASS);
		formatf(gCurrentWarpPosition, "%f", fResult, NELEM(gCurrentWarpPosition));
	}
}
#endif // ENABLE_CDEBUG

#define DISPLAY_OBJECTS_FILENAME	"common:/data/displayobjects.txt"
const static int MAX_DISPLAY_OBJECTS = 128;

void LoadDisplayObjectsList()
{
	for (int i = 1; i < gCurrentDisplayObjectCount; ++i)
	{
		delete [] gCurrentDisplayObjectList[i];
	}
	gCurrentDisplayObjectCount = 1;

	if (!gCurrentDisplayObjectList)
	{
		gCurrentDisplayObjectList = rage_new char*[MAX_DISPLAY_OBJECTS];
		gCurrentDisplayObjectList[0] = rage_new char[1];
		gCurrentDisplayObjectList[0][0] = '\0';
	}

	CFileMgr::SetDir("");
	FileHandle fid = INVALID_FILE_HANDLE;
	if (ASSET.Exists(DISPLAY_OBJECTS_FILENAME, ""))
	{ 
		fid = CFileMgr::OpenFile(DISPLAY_OBJECTS_FILENAME);
	}

	if (CFileMgr::IsValidFileHandle(fid))
	{
		int iNumObjects = 1;
		char* pLine;
		while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) )
		{
			if( *pLine != 0 )			
			{

				if (!Verifyf(iNumObjects < MAX_DISPLAY_OBJECTS, "Too may objects in " DISPLAY_OBJECTS_FILENAME))
				{
					break;
				}

				size_t iLength = strlen(pLine);
				gCurrentDisplayObjectList[iNumObjects] = rage_new char[iLength + 1];
				strncpy(gCurrentDisplayObjectList[iNumObjects], pLine, iLength);
				gCurrentDisplayObjectList[iNumObjects][iLength] = '\0';
				++iNumObjects;
			}
		}

		gCurrentDisplayObjectCount = iNumObjects;

		CFileMgr::CloseFile(fid);
	}
}

void ReloadDisplayObjectsList()
{
	// Reload the list - this call will automatically free up the previous one.
	LoadDisplayObjectsList();

	// If the list got shorter, make sure we don't point to an invalid entry.
	gCurrentDisplayObjectIndex = Min(gCurrentDisplayObjectIndex, gCurrentDisplayObjectCount-1);

	// Update RAG.
	gDisplayObjectListCombo->UpdateCombo("Display Object List", &gCurrentDisplayObjectIndex, gCurrentDisplayObjectCount, (const char**)gCurrentDisplayObjectList, &SelectDisplayObject);
}

void SaveDisplayObjectList()
{
	CFileMgr::SetDir("");
	FileHandle fid = CFileMgr::OpenFileForWriting(DISPLAY_OBJECTS_FILENAME);

	if (CFileMgr::IsValidFileHandle(fid))
	{
		for (int i = 1; i < gCurrentDisplayObjectCount; ++i)
		{
			CFileMgr::Write(fid, gCurrentDisplayObjectList[i], istrlen(gCurrentDisplayObjectList[i]));
			CFileMgr::Write(fid, "\n", istrlen("\n"));
		}

		CFileMgr::CloseFile(fid);
	}
}

void AddObjectToList()
{
	if (!Verifyf(gCurrentDisplayObjectCount < MAX_DISPLAY_OBJECTS, "Display object list full (increase MAX_DISPLAY_OBJECTS)"))
	{
		return;
	}

	for (int i = 1; i < gCurrentDisplayObjectCount; ++i)
	{
		if (!stricmp(gCurrentDisplayObjectList[i], gCurrentDisplayObject))
		{
			// Already in the list
			return;
		}
	}

	size_t iLength = strlen(gCurrentDisplayObject);
	char* newObject = rage_new char[iLength + 1];
	gCurrentDisplayObjectList[gCurrentDisplayObjectCount++] = newObject;
	strncpy(newObject, gCurrentDisplayObject, iLength);
	newObject[iLength] = '\0';

	SaveDisplayObjectList();
	gDisplayObjectListCombo->UpdateCombo("Display Object List", &gCurrentDisplayObjectIndex, gCurrentDisplayObjectCount, (const char**)gCurrentDisplayObjectList, &SelectDisplayObject);
	gCurrentDisplayObjectIndex = gCurrentDisplayObjectCount - 1;
}

void RemoveObjectFromList()
{
	if (gCurrentDisplayObjectIndex < 1 || gCurrentDisplayObjectIndex >= gCurrentDisplayObjectCount)
	{
		// Not removable
		return;
	}

	delete [] gCurrentDisplayObjectList[gCurrentDisplayObjectIndex];

	// Determine the new size of the list
	--gCurrentDisplayObjectCount;

	// Copy the rest down
	for (int i = gCurrentDisplayObjectIndex; i < gCurrentDisplayObjectCount; ++i)
	{
		gCurrentDisplayObjectList[i] = gCurrentDisplayObjectList[i + 1];
	}

	SaveDisplayObjectList();
	gDisplayObjectListCombo->UpdateCombo("Display Object List", &gCurrentDisplayObjectIndex, gCurrentDisplayObjectCount, (const char**)gCurrentDisplayObjectList, &SelectDisplayObject);
	strncpy(gCurrentDisplayObject, gCurrentDisplayObjectList[gCurrentDisplayObjectIndex], sizeof(gCurrentDisplayObject));
}

#if DEBUG_BANK
PARAM(displayobject, "[debug] Display object name");
#endif // DEBUG_BANK

void UpdateStrModelRequest()
{
	if(gStreamModelReq.HasLoaded())
	{
		DeleteObject();			// destroy the debug object if it is streamed - just in case gStreamModelReq was holding the archetype in memory
		gCurrentDisplayObject[0] = '\0';
	}

	gStreamModelReq.Release();

	gStreamModelReq.Request(gStreamModelName, STRFLAG_PRIORITY_LOAD|STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);

	while(gStreamModelReq.IsValid() && !gStreamModelReq.HasLoaded())
	{
		CStreaming::LoadAllRequestedObjects();
	}
}

void InitLiveEditWidgets()
{
	s_LiveEditBank = &BANKMGR.CreateBank("_LiveEdit_");

	// Add bugstar section to live edit bank
	s_LiveEditBank->PushGroup("Bugstar", false);
	s_LiveEditBank->AddText("Warp Player x y z h vx vy vz", gCurrentWarpPosition, sizeof(gCurrentWarpPosition), false, &WarpPlayer);
	s_LiveEditBank->AddButton("Warp now", WarpPlayer);
	CBugstarIntegration::InitLiveEditWidgets(*s_LiveEditBank);
	s_LiveEditBank->PopGroup();

#if __DEV
	// Add MoVE network preview section to live edit bank
	CAnimViewer::AddNetworkPreviewLiveEditWidgets(s_LiveEditBank);
#endif // __DEV

	Assert(CDebug::GetLiveEditBank());
#if PLANTSMGR_DATA_EDITOR
	gPlantMgrEditor.InitWidgets(*CDebug::GetLiveEditBank(), "PlantsMgr Data Edit");
#endif
}

static void TakeScreenshotCB()
{
	CDebug::RequestTakeScreenShot(g_bScreenshotPNG);
}

static void SimulateReadError()
{
	if(fiDevice::sm_FatalReadError)
	{
		fiDevice::sm_FatalReadError();
	}
}

void CDebug::InitWidgets()
{
#if DEBUG_BANK
	bkBank& bank = BANKMGR.CreateBank("Debug");

#if RSG_DURANGO && __BANK
	bank.AddToggle("Show Timer Comparison", &s_showDebugTimeInfo);
#endif

	gDrawListMgr->SetupWidgets(bank);

	bank.AddButton("Reload Shaders", CDrawListMgr::RequestShaderReload);

#if ENABLE_FPS_CAPTURE
	// EJ: FPS Capture
	bank.AddSlider("Captures", &g_fMaxCaptures, 100.0f, 1000.0f, 1.0f);
	bank.AddButton("Start Capture FPS", &StartFPSCapture);	
#endif

#if DEBUG_SCREENSHOTS
	bank.AddButton("Screen shot", TakeScreenshotCB);
	bank.AddToggle("PNG Format", &g_bScreenshotPNG);
#endif // DEBUG_SCREENSHOTS

	bank.AddButton("Crash now...", ForceCrash);

	bank.AddText("Warp Player x y z h vx vy vz", gCurrentWarpPosition, sizeof(gCurrentWarpPosition), false, &WarpPlayer);
	bank.AddSlider("Warp Player Range", &gWarpStreamingRadius, 10.0f, 5000.0f, 1.0f);
	bank.AddButton("Warp now", WarpPlayer);

	bank.AddSlider("Debug fontscale", &sm_dbgFontscale, 0.1f, 5.0f, 0.1f);

	bank.AddToggle("Display timescale", &g_bDisplayTimescale);


	bank.AddText("Find entity from pointer", gFindEntityPointer, sizeof(gFindEntityPointer), false, &FindEntityFromPointer);

// 	bank.PushGroup("TEMP - Vehicle.rpf test");
// 	bank.AddButton("Switch to SP vehicles", datCallback(SwitchToSPVehicles));
// 	bank.AddButton("Switch to MP vehicles", datCallback(SwitchToMPVehicles));
// 	bank.PopGroup();

#if RSG_DURANGO
	bank.PushGroup("Light Sensor");
	{
		bank.AddToggle("Display Light Sensor Square", &sm_bDisplayLightSensorQuad);
	}
	bank.PopGroup();
#endif

#if(BUGSTAR_INTEGRATION_ACTIVE) 
	bank.PushGroup("Bugstar");
	CBugstarIntegration::InitWidgets(bank);

	bank.PushGroup("Add bug");
	bank.AddButton("Add bug", RequestAddBug);
	bank.AddText("Summary", gBugstarSummary, sizeof(gBugstarSummary));
	bank.AddCombo("Class", &gBugstarClass, 7, gBugstarClassCombo);
	bank.AddText("Owner", gBugstarOwner, sizeof(gBugstarOwner));		
	bank.AddSlider("Jump to Bug Number Coords", &CDebug::m_iBugNumber, -1, 299999, 1, &CDebug::JumpToBugCoords);
	bank.AddToggle("Include Selected Ped Variation Info", &gBugstarPedVariationInfo);
	gBugstarPedVariationInfo = PARAM_bugstardescpedvar.Get();
	bank.PopGroup();
	bank.PopGroup();
#endif //(BUGSTAR_INTEGRATION_ACTIVE)

#if ENABLE_CDEBUG
	grcDebugDraw::AddWidgets(bank);
	bank.PushGroup("Start-up");
		bank.AddButton("Dump startup params", CFA(CDebug::DumpDebugParams));
	bank.PopGroup();
	
	ms_locationList.Init(&bank);
	bank.AddButton("Dump ped and vehicle memory data", datCallback(CFA(COverviewScreenManager::DumpPedAndVehicleData)));
#if ENABLE_STATS_CAPTURE
	MetricsCapture::SetLocationList(&ms_locationList);
	MetricsCapture::AddWidgets(bank);
#endif // ENABLE_STATS_CAPTURE

#if __BANK
	CInteriorProxy::AddWidgets(bank);
	C2dEffectDebugRenderer::InitWidgets(bank);
	CMPObjectUsage::AddWidgets(bank);
#endif	//__BANK

	LoadDisplayObjectsList();
	gDisplayObjectListCombo = bank.AddCombo("Display Object List", &gCurrentDisplayObjectIndex, gCurrentDisplayObjectCount, (const char**)gCurrentDisplayObjectList, &SelectDisplayObject);
	bank.AddButton("Reload Display Object List", datCallback(ReloadDisplayObjectsList));

	bank.AddToggle("Display timer bars", &gDisplayTimerBars);
	bank.AddToggle("Smooth Performance Limiting Results", &gSmoothPerformanceResults);
#if RSG_ORBIS
	bank.AddButton("Start GPU trace capture", datCallback(CFA(CSystem::StartGPUTraceCapture)));
#endif // RSG_ORBIS

	if (PARAM_displayobject.Get())
	{
		const char* displayobject = NULL;
		PARAM_displayobject.Get(displayobject);
		strcpy(gCurrentDisplayObject, displayobject);
	}

	bank.AddButton("Populate packfile source list", datCallback(CFA(CDebug::PopulatePackfileSourceCombo)));
	gPackfileSourceCombo = bank.AddCombo("Packfile source for object", &gPackfileSource, 1, gPackfileSourceDummyList, NullCB);
	gObjectToCreateCombo = bank.AddCombo("Object to create", &gObjectToCreateIndex, 1, gObjectToCreateDummyList, NullCB);

	bank.AddText("strModelRequest test", gStreamModelName, sizeof(gStreamModelName), false, &UpdateStrModelRequest);
	bank.AddText("Display Object", gCurrentDisplayObject, sizeof(gCurrentDisplayObject), false, &DisplayObjectDeleteOld);
	bank.AddSlider("Legion count X", &gLegionCountX, 1, 100, 1);
	bank.AddSlider("Legion count Y", &gLegionCountY, 1, 100, 1);
	bank.AddSlider("Legion spacing", &gLegionSpacing, 0.f, 10.f, 0.1f);
	bank.AddButton("Display legion", DisplayLegionObject);
	bank.AddButton("Delete legion", DeleteLegion);
	bank.AddButton("Add object to list",AddObjectToList);
	bank.AddButton("Remove object from list",RemoveObjectFromList);
	bank.AddSeparator();
	bank.AddButton("Display Object Again", &DisplayObjectKeepOld);
	bank.AddButton("Display and place on ground", &DisplayObjectAndPlaceOnGround);
	bank.AddButton("Delete Last Object", &DeleteObject);
	bank.AddButton("Debug Last Object", DebugLastObject);
	bank.AddButton("Place last object on ground",PlaceLastObjectOnGround);
	bank.AddButton("Repair last object",RepairLastObject);
	bank.AddButton("Frag tune last object",FragTuneLastObject);
	bank.AddToggle("Activate Display Object", &gActivateDisplayObject);
	bank.AddToggle("Cache tuned frag object",&gCacheTunedFragObject);
	bank.AddToggle("Display Safe Zone", &gDisplaySafeZone, &DisplaySafeZone);
	//bank.AddSlider("Simulate Read Error", &fiDevice::sm_InjectReadError, 0, 1, 1);
	bank.AddButton("Simulate Read Error", SimulateReadError );
#if __XENON
	bank.AddToggle("Trace update thread", &sm_bTraceUpdateThread);
	bank.AddToggle("Trace render thread", &sm_bTraceRenderThread);
#endif // __XENON
#if __DEV
	bank.AddToggle("2dEffect stats", &C2dEffect::m_PrintStats);
	bank.AddToggle("Allow Display of Vector Map", &CVectorMap::m_bAllowDisplayOfMap);
#endif // __DEV
	bank.AddToggle("Display Map Zone", &gDisplayMapZone);

	bank.AddToggle("Display network spawn nodes", &gDisplaySpawnNodes);

	bank.AddButton("Dump hashed strings", CFA(atHashStringNamespaceSupport::SaveAllHashStrings));

	CDebugScene::AddWidgets(bank);

	bank.PushGroup("Measuring Tool",false);
	bank.AddToggle( "Turn on tool", &CPhysics::ms_bDebugMeasuringTool );
	bank.AddText("Pos1", CPhysics::ms_Pos1, sizeof(CPhysics::ms_Pos1), false, datCallback(CFA1(CPhysics::UpdateDebugPositionOne)));
	bank.AddText("Ptr1", CPhysics::ms_Ptr1, sizeof(CPhysics::ms_Ptr1), false);
	bank.AddText("Pos2", CPhysics::ms_Pos2, sizeof(CPhysics::ms_Pos2), false, datCallback(CFA1(CPhysics::UpdateDebugPositionTwo)));
	bank.AddText("Ptr2", CPhysics::ms_Ptr2, sizeof(CPhysics::ms_Ptr2), false);
	bank.AddText("Diff", CPhysics::ms_Diff, sizeof(CPhysics::ms_Diff), false);
	bank.AddText("HeadingBetween (Radians)", CPhysics::ms_HeadingDiffRadians, sizeof(CPhysics::ms_HeadingDiffRadians), false);
	bank.AddText("HeadingBetween (Degrees)", CPhysics::ms_HeadingDiffDegrees, sizeof(CPhysics::ms_HeadingDiffDegrees), false);
	bank.AddText("Distance", CPhysics::ms_Distance, sizeof(CPhysics::ms_Distance), false);
	bank.AddText("Horizontal dist", CPhysics::ms_HorizDistance, sizeof(CPhysics::ms_HorizDistance), false);
	bank.AddText("Vertical dist", CPhysics::ms_VerticalDistance, sizeof(CPhysics::ms_VerticalDistance), false);
	bank.AddText("Normal1", CPhysics::ms_Normal1, sizeof(CPhysics::ms_Normal1), false);
	bank.AddText("Normal2", CPhysics::ms_Normal2, sizeof(CPhysics::ms_Normal2), false);
	bank.PopGroup();

#if __D3D && __WIN32PC 
	bank.AddToggle("show d3d resource info", &showd3dresourceinfo);
	bank.AddToggle("PC only: Enable bound by GPU Flip (time spent in Present())", &sm_enableBoundByGPUFlip);
#endif // __D3D && __WIN32PC 

#if __BANK
	CVectorMap::AddWidgets(bank);
#if __ASSERT
	//g_ExternalPreAssertHandler = CDebug::OnAssertionFired;
    diagExternalCallback = OnAssertionFired;
#endif // __ASSERT    
#endif // __DEV  

#endif // ENABLE_CDEBUG

#if CSE_PROP_EDITABLEVALUES
	bkBank& propBank = BANKMGR.CreateBank("Props");
	CCustomShaderEffectProp::InitWidgets(propBank);
#endif //CSE_PROP_EDITABLEVALUES...

	// add the Rage widgets to get constant screenshots & viewer at the bottom:
	bank.PushGroup("Screenshot Viewer" ,false);	
		CSystem::GetRageSetup()->AddScreenShotViewerWidgets(bank);
	bank.PopGroup();

	AddCrashWidgets(&bank);

	bkBank* pBank = BANKMGR.FindBank("Optimization");
	CTestCase::AddWidgets(*pBank);

	InitLiveEditWidgets();
	InitToolsWidgets();

	psoDebugMismatchedFiles::InitClass();
#endif // DEBUG_BANK
}

bkBank* CDebug::GetLiveEditBank()
{
	return s_LiveEditBank;
}

void CDebug::PopulatePackfileSourceCombo()
{
	if (gPackfileSourceCombo)
	{
		ClearPackfileSourceCombo();

		// Get a list of all packfiles.
		int count = strPackfileManager::GetImageFileCount();

		gPackfileSourceList = rage_new const char *[count];
		gPackfileMapping = rage_new int[count];

		int finalCount = 0;

		// We want to ignore packfiles that don't contain models, so let's create a quick bitmask
		// to see which packfiles contain models.
		atBitSet usedPackfiles;

		usedPackfiles.Init(count);

		strStreamingModule *module = fwArchetypeManager::GetStreamingModule();
		int archetypes = module->GetCount();

		for (int x=0; x<archetypes; x++)
		{
			if (!CModelInfo::GetModelInfoFromLocalIndex(x))
			{
				continue;
			}

			/// strIndex index = module->GetStreamingIndex(x);
			// Now the archetype itself is not going to be in this packfile (it's a dummy streamable), but its
			// dependencies will.
			strIndex deps[256];
			int depCount = module->GetDependencies(strLocalIndex(x), deps, 256);

			for (int dep=0; dep<depCount; dep++)
			{
				strStreamingInfo &depInfo = strStreamingEngine::GetInfo().GetStreamingInfoRef(deps[dep]);
				strLocalIndex imageIndex = strPackfileManager::GetImageFileIndexFromHandle(depInfo.GetHandle());

				if (imageIndex.Get() != -1)
				{
					usedPackfiles.Set(imageIndex.Get());
				}
			}
		}

		for (int x=0; x<count; x++)
		{
			if (usedPackfiles.IsSet(x))
			{
				strStreamingFile *file = strPackfileManager::GetImageFile(x);
				Assert(file->m_name.GetCStr());
				gPackfileSourceList[finalCount] = file->m_name.GetCStr();
				gPackfileMapping[finalCount] = x;

				finalCount++;
			}
		}

		gPackfileSourceCount = finalCount;
		gPackfileSourceCombo->UpdateCombo(gPackfileSourceCombo->GetTitle(), &gPackfileSource, gPackfileSourceCount, gPackfileSourceList, 0, datCallback(CFA(CDebug::PopulateObjectCombo)));
	}
}

void CDebug::ClearPackfileSourceCombo()
{
	if (gPackfileSourceCombo)
	{
		gPackfileSourceCombo->UpdateCombo(gPackfileSourceCombo->GetTitle(), &gPackfileSource, 1, gPackfileSourceDummyList, 0, NullCB);
	}

	delete[] gPackfileSourceList;
	gPackfileSourceList = NULL;

	delete[] gPackfileMapping;
	gPackfileMapping = NULL;
}

void CDebug::ClearObjectComo()
{
	if (gObjectToCreateCombo)
	{
		gObjectToCreateCombo->UpdateCombo(gObjectToCreateCombo->GetTitle(), &gObjectToCreateIndex, 1, gObjectToCreateDummyList, 0, NullCB);
	}

	delete[] gObjectNameList;
	delete[] gObjectModelIdList;

	gObjectNameList = NULL;
	gObjectModelIdList = NULL;
}

void CDebug::PopulateObjectCombo()
{
	if (gPackfileSourceList)
	{
		if (gPackfileSource >= 0 && gPackfileSource < gPackfileSourceCount)
		{
			int packfileIndex = gPackfileMapping[gPackfileSource];

			// Find all the props within this file.
			strStreamingModule *module = fwArchetypeManager::GetStreamingModule();

			for (int pass=0; pass<2; pass++)
			{
				int count = module->GetCount();
				int finalCount = 0;

				for (int x=0; x<count; x++)
				{
					if (!CModelInfo::GetModelInfoFromLocalIndex(x))
					{
						continue;
					}

					/// strIndex index = module->GetStreamingIndex(x);
					// Now the archetype itself is not going to be in this packfile (its a dummy streamable), but its
					// dependencies will.
					strIndex deps[256];
					int depCount = module->GetDependencies(strLocalIndex(x), deps, 256);
					bool isInRpf = false;

					for (int dep=0; dep<depCount; dep++)
					{
						strStreamingInfo &depInfo = strStreamingEngine::GetInfo().GetStreamingInfoRef(deps[dep]);
						strLocalIndex imageIndex = strPackfileManager::GetImageFileIndexFromHandle(depInfo.GetHandle());

						if (imageIndex.Get() == packfileIndex)
						{
							isInRpf = true;
							break;
						}
					}

					if (isInRpf)
					{
						fwModelId modelId((strLocalIndex(x)));
						CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfo(modelId);

						if (pass == 1)
						{
							gObjectNameList[finalCount] = modelInfo->GetModelName();
							gObjectModelIdList[finalCount] = modelId;
						}

						finalCount++;
					}
				}

				if (pass == 0)
				{
					// End of first pass - we now know how many objects there are.
					gObjectNameList = rage_new const char *[finalCount];
					gObjectModelIdList = rage_new fwModelId[finalCount];
					gObjectToCreateCount = finalCount;
				}
			}

			// We now have a list of names and model IDs. Let's populate the combo.
			gObjectToCreateCombo->UpdateCombo(gObjectToCreateCombo->GetTitle(), &gObjectToCreateIndex, gObjectToCreateCount, gObjectNameList, 0, datCallback(CFA(CDebug::PickObjectToSpawn)));

			// And refresh display object
			PickObjectToSpawn();
		}
	}
}

void CDebug::PickObjectToSpawn()
{
	if (gObjectModelIdList)
	{
		if (gObjectToCreateIndex >= 0 && gObjectToCreateIndex < gObjectToCreateCount)
		{
			fwModelId modelId(gObjectModelIdList[gObjectToCreateIndex]);
			CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfo(modelId);

			safecpy(gCurrentDisplayObject, modelInfo->GetModelName());
			DisplayObject(true);
		}
	}
}

void CDebug::DisplayObjectAndPlaceOnGround()
{
	DisplayObject(false); PlaceLastObjectOnGround();
}

void CDebug::InitRageDebugChannelWidgets()
{
	ms_pRageDebugChannelBank = BANKMGR.FindBank("rage - Debug Channels");
	if (!ms_pRageDebugChannelBank)
	{
		ms_pRageDebugChannelBank = &BANKMGR.CreateBank("rage - Debug Channels");

		ASSERT_ONLY(ms_pRageDebugChannelBank->AddButton("Clear ignored asserts", datCallback(CFA(diagChannel::ClearIgnoredAsserts))));

		ms_pRageDebugChannelBank->AddButton("Create debug channel widgets", datCallback(CFA1(CDebug::AddRageDebugChannelWidgets), ms_pRageDebugChannelBank));
	}
}

void CDebug::InitToolsWidgets()
{
	ms_pToolsBank = BANKMGR.FindBank("Tools");
	if (!ms_pToolsBank)
	{
		ms_pToolsBank = &BANKMGR.CreateBank("Tools");
		ms_pToolsBank->AddButton("Create tools widgets", datCallback(CFA1(CDebug::AddToolsWidgets), ms_pToolsBank));
	}
}

void CDebug::AddRageDebugChannelWidgets(bkBank *bank)
{
	bkWidget *pWidget = BANKMGR.FindWidget("rage - Debug Channels/Create debug channel widgets");
	if(pWidget == NULL)
		return;
	pWidget->Destroy();

	diagChannel::InitWidgets(*bank);
}

void CDebug::AddToolsWidgets(bkBank *bank)
{
	bkWidget *pWidget = BANKMGR.FindWidget("Tools/Create tools widgets");
	if(pWidget == NULL)
		return;
	pWidget->Destroy();

	if (ms_pBankDebug == NULL)
	{
		ms_pBankDebug = new CBankDebug();
		ms_pBankDebug->InitWidgets(*bank);
	}
}

void CDebug::ShutdownRageDebugChannelWidgets()
{
	ClearObjectComo();
	ClearPackfileSourceCombo();

	if (ms_pRageDebugChannelBank)
	{
		ms_pRageDebugChannelBank->Destroy();
		ms_pRageDebugChannelBank = NULL;
	}

	gPackfileSourceCombo = NULL;
	gObjectToCreateCombo = NULL;
}


//////////////////////////////////////////////////////////////////////////
// CMPObjectUsage

#if __BANK

bool	CMPObjectUsage::m_bActive = false;

void	CMPObjectUsage::AddWidgets(bkBank &bank)
{
	bank.AddToggle("Show Objects Not Used in MP", &m_bActive);
}

bool CMPObjectUsage::IsUsedInMP(CEntity *pEntity)
{
	return pEntity->GetIsUsedInMP();
}

bool CMPObjectUsage::ShouldDisplayOnMap(CEntity *pEntity)
{
	if( pEntity->GetIsTypeObject() )
	{
		if( !IsUsedInMP(pEntity) )
		{
			return true;
		}
	}
	return false;
}
#endif	//__BANK


//////////////////////////////////////////////////////////////////////////
// CTestCase

s32 CTestCase::ms_TestCaseIndex = 0;
s32 CTestCase::ms_CurrTestCaseIndex = -1;


//////////////////////////////////////////////////////////////////////////
// Test Cases - shared variables (could move into CTestCase now)

bool s_bPushedAllowPedGeneration = false;
bool s_bPushedDisableRandomTrains = false;
bool s_bPushedNoCops = false;
int s_iPushedMaxCars = -1;
int s_iPushedMaxAmbientPeds = -1;
int s_iPushedMaxScenarioPeds = -1;
bool s_bTeleportPlayerForTest = true;
bool s_bPreventPlayerProcessForTest = false;
int s_iPushedMaxAmbientVehicles = -1;
int s_iMaxVehicles = 100;
int s_iMaxAmbientPeds = 100;


//////////////////////////////////////////////////////////////////////////
// Cars Test Cases

float s_fCarsVehiclesPerMeterDensityLookup[16] = {};

static void InitCarsCore()
{
	if(s_bTeleportPlayerForTest)
	{
		Vector3 position(1051.808105f,-1169.786499f,55.518246f);

		Matrix34 camera(	0.999738f,		0.022884f,		0.000000f,
			-0.021861f,		0.955070f,		-0.295573f,
			-0.006764f,		0.295496f,		0.955320f,
			1051.854126f,	-1173.770142f,	57.747871f);

		debugLocation loc;
		loc.Set(position, camera, 14);
		loc.BeamMeUp();
	}

	if(!NetworkInterface::IsGameInProgress())
	{
		CVehiclePopulation::RemoveAllVehsHard();
		CPedPopulation::RemoveAllPedsHardNotPlayer();
	}

	s_iPushedMaxCars = CVehiclePopulation::ms_overrideNumberOfCars;
	CVehiclePopulation::ms_overrideNumberOfCars = s_iMaxVehicles;

	s_iPushedMaxAmbientVehicles = CVehiclePopulation::ms_iAmbientVehiclesUpperLimit;
	CVehiclePopulation::ms_iAmbientVehiclesUpperLimit = s_iMaxVehicles;

	s_bPushedAllowPedGeneration = gbAllowAmbientPedGeneration;
	gbAllowAmbientPedGeneration = false;

	s_bPushedDisableRandomTrains = CTrain::sm_bDisableRandomTrains;
	CTrain::sm_bDisableRandomTrains = true;

	s_bPushedNoCops = CVehiclePopulation::ms_noPoliceSpawn;
	CVehiclePopulation::ms_noPoliceSpawn = true;

	for (int i = 0; i < NELEM(CVehiclePopulation::ms_fVehiclesPerMeterDensityLookup); ++i)
	{
		s_fCarsVehiclesPerMeterDensityLookup[i] = CVehiclePopulation::ms_fVehiclesPerMeterDensityLookup[i];
		CVehiclePopulation::ms_fVehiclesPerMeterDensityLookup[i] = 0.3f;
	}

	CVehiclePopulation::ForcePopulationInit();
}

static void ShutdownCars()
{
	CVehiclePopulation::ms_overrideNumberOfCars = s_iPushedMaxCars;
	CVehiclePopulation::ms_iAmbientVehiclesUpperLimit = s_iPushedMaxAmbientVehicles;

	CTrain::sm_bDisableRandomTrains = s_bPushedDisableRandomTrains;
	CVehiclePopulation::ms_noPoliceSpawn = s_bPushedNoCops;

	gbAllowAmbientPedGeneration = s_bPushedAllowPedGeneration;

#if __DEV
	CDebugScene::bDisplayVehiclesToBeStreamedOutOnVMap = false;
	CDebugScene::bDisplayVehicleCollisionsOnVMap = false;
#endif

	for (int i = 0; i < NELEM(CVehiclePopulation::ms_fVehiclesPerMeterDensityLookup); ++i)
	{
		CVehiclePopulation::ms_fVehiclesPerMeterDensityLookup[i] = s_fCarsVehiclesPerMeterDensityLookup[i];
	}

	if(s_bTeleportPlayerForTest)
	{
		camInterface::GetDebugDirector().DeactivateFreeCam();
		CControlMgr::SetDebugPadOn(false);
	}

	// Restore normal LOD
	CVehicleAILodManager::ms_bForceDummyCars = false;
	CVehicleAILodManager::ms_bUseDummyLod = true;
	CVehicleAILodManager::ms_bAllSuperDummys = false;
	CVehicleAILodManager::ms_bUseSuperDummyLod = true;
	CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = true;
}

static void InitCarsLodNormal()
{
	CVehicleAILodManager::ms_bForceDummyCars = false;
	CVehicleAILodManager::ms_bUseDummyLod = true;
	CVehicleAILodManager::ms_bAllSuperDummys = false;
	CVehicleAILodManager::ms_bUseSuperDummyLod = true;
	CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = true;
	InitCarsCore();
}

static void InitCarsForceLodReal()
{
	CVehicleAILodManager::ms_bForceDummyCars = false;
	CVehicleAILodManager::ms_bUseDummyLod = false;
	CVehicleAILodManager::ms_bAllSuperDummys = false;
	CVehicleAILodManager::ms_bUseSuperDummyLod = false;
	CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = false;
	InitCarsCore();
}

static void InitCarsForceLodDummy()
{
	CVehicleAILodManager::ms_bForceDummyCars = true;
	CVehicleAILodManager::ms_bUseDummyLod = true;
	CVehicleAILodManager::ms_bAllSuperDummys = false;
	CVehicleAILodManager::ms_bUseSuperDummyLod = false;
	CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = false;
	InitCarsCore();
}

static void InitCarsForceLodSuperDummy()
{
	CVehicleAILodManager::ms_bForceDummyCars = false;
	CVehicleAILodManager::ms_bUseDummyLod = false;
	CVehicleAILodManager::ms_bAllSuperDummys = true;
	CVehicleAILodManager::ms_bUseSuperDummyLod = true;
	CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = false;
	InitCarsCore();
}

static void InitCarsForceLodSuperDummyInactive()
{
	CVehicleAILodManager::ms_bForceDummyCars = false;
	CVehicleAILodManager::ms_bUseDummyLod = false;
	CVehicleAILodManager::ms_bAllSuperDummys = true;
	CVehicleAILodManager::ms_bUseSuperDummyLod = true;
	CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = true;
	InitCarsCore();
}

static void UpdateCarsCloseOnly()
{
	static float fKeepDist2 = square(150.0f);
	Vector3 vPlayerPos = CGameWorld::FindLocalPlayerCoors();
	CVehicle::Pool *pool = CVehicle::GetPool();
	for(int i=0; i<pool->GetSize(); i++)
	{
		if(CVehicle * pVeh = pool->GetSlot(i))
		{
			if(vPlayerPos.Dist2(VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()))>fKeepDist2)
			{
				CVehicleFactory::GetFactory()->Destroy(pVeh);
			}
		}
	}
}

static void UpdateCarsCycleLod()
{
	CVehicleAILodManager::ms_iTimeBetweenConversionAttempts = 0;

	enum { eLodReal, eLodDummy, eLodSuperDummy, eLodLast };
	static int iLodDurationMs = 1000;
	static int iLod = -1;
	static int iLodTimeRemainingMs = 0;
	if((iLodTimeRemainingMs -= fwTimer::GetTimeStepInMilliseconds()) < 0)
	{
		iLodTimeRemainingMs = iLodDurationMs;
		iLod = (iLod + 1) % eLodLast;
		switch(iLod)
		{
			case eLodReal:
				CVehicleAILodManager::ms_bForceDummyCars = false;
				CVehicleAILodManager::ms_bUseDummyLod = false;
				CVehicleAILodManager::ms_bAllSuperDummys = false;
				CVehicleAILodManager::ms_bUseSuperDummyLod = false;
				CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = false;
				break;
			case eLodDummy:
				CVehicleAILodManager::ms_bForceDummyCars = true;
				CVehicleAILodManager::ms_bUseDummyLod = true;
				CVehicleAILodManager::ms_bAllSuperDummys = false;
				CVehicleAILodManager::ms_bUseSuperDummyLod = false;
				CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = false;
				break;
			case eLodSuperDummy:
				CVehicleAILodManager::ms_bForceDummyCars = false;
				CVehicleAILodManager::ms_bUseDummyLod = false;
				CVehicleAILodManager::ms_bAllSuperDummys = true;
				CVehicleAILodManager::ms_bUseSuperDummyLod = true;
				CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode = true;
				break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Peds Test Cases

float	Backup_ms_fPhysicsAndEntityPedLodDistance;
float	Backup_ms_fMotionTaskPedLodDistance;
float	Backup_ms_fMotionTaskPedLodDistanceOffscreen;
float	Backup_ms_fEventScanningLodDistance;
float	Backup_ms_fRagdollInVehicleDistance;
s32		Backup_ms_overrideNumberOfParkedCars;

void	StoreNormalPedVars()
{
	Backup_ms_fPhysicsAndEntityPedLodDistance = CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance;
	Backup_ms_fMotionTaskPedLodDistance = CPedAILodManager::ms_fMotionTaskPedLodDistance;
	Backup_ms_fMotionTaskPedLodDistanceOffscreen = CPedAILodManager::ms_fMotionTaskPedLodDistanceOffscreen;
	Backup_ms_fEventScanningLodDistance = CPedAILodManager::ms_fEventScanningLodDistance;
	Backup_ms_fRagdollInVehicleDistance = CPedAILodManager::ms_fRagdollInVehicleDistance;
	Backup_ms_overrideNumberOfParkedCars = CVehiclePopulation::ms_overrideNumberOfParkedCars;
}

void	RestoreNormalPedVars()
{
	CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance = Backup_ms_fPhysicsAndEntityPedLodDistance;
	CPedAILodManager::ms_fMotionTaskPedLodDistanceOffscreen = Backup_ms_fMotionTaskPedLodDistanceOffscreen;
	CPedAILodManager::ms_fEventScanningLodDistance = Backup_ms_fEventScanningLodDistance;
	CPedAILodManager::ms_fRagdollInVehicleDistance = Backup_ms_fRagdollInVehicleDistance;
	CVehiclePopulation::ms_overrideNumberOfParkedCars = Backup_ms_overrideNumberOfParkedCars;
}

static void InitPedsCore()
{
	// It would be nice to get the same scene every time... this didn't do it.
	//fwRandom::SetRandomSeed(42);
	//g_ReplayRand.SetFullSeed(42);

	if(s_bTeleportPlayerForTest)
	{
		Vector3 vPlayerPos(159.4f,-986.9f,30.2f);			// Persian square, maybe?

		Matrix34 cameraMtx;
		cameraMtx.Identity();
		cameraMtx.d.Set(vPlayerPos - Vector3(2.0f,0,-1.0f));
		cameraMtx.MakeRotateZ(270 * DtoR);

		debugLocation loc;
		loc.Set(vPlayerPos,cameraMtx,14);
		loc.BeamMeUp();
	}

	s_bPushedAllowPedGeneration = gbAllowAmbientPedGeneration;
	gbAllowAmbientPedGeneration = true;

	CVehiclePopulation::RemoveAllVehsHard();
	CPedPopulation::RemoveAllPedsHardNotPlayer();

	s_iPushedMaxCars = CVehiclePopulation::ms_overrideNumberOfCars;
	CVehiclePopulation::ms_overrideNumberOfCars = 0;

	s_bPushedDisableRandomTrains = CTrain::sm_bDisableRandomTrains;
	CTrain::sm_bDisableRandomTrains = true;

	s_bPushedNoCops = CVehiclePopulation::ms_noPoliceSpawn;
	CVehiclePopulation::ms_noPoliceSpawn = true;

	s_iPushedMaxAmbientPeds = CPedPopulation::ms_popCycleOverrideNumberOfAmbientPeds;
	s_iPushedMaxScenarioPeds = CPedPopulation::ms_popCycleOverrideNumberOfScenarioPeds;
	CPedPopulation::ms_popCycleOverrideNumberOfAmbientPeds = s_iMaxAmbientPeds;
	CPedPopulation::ms_popCycleOverrideNumberOfScenarioPeds = 0;

	CPedPopulation::ForcePopulationInit();
}

static void ShutdownPeds()
{
	CPedPopulation::ms_popCycleOverrideNumberOfAmbientPeds = s_iPushedMaxAmbientPeds;
	CPedPopulation::ms_popCycleOverrideNumberOfScenarioPeds = s_iPushedMaxScenarioPeds;

	CVehiclePopulation::ms_overrideNumberOfCars = s_iPushedMaxCars;
	CTrain::sm_bDisableRandomTrains = s_bPushedDisableRandomTrains;
	CVehiclePopulation::ms_noPoliceSpawn = s_bPushedNoCops;

	gbAllowAmbientPedGeneration = s_bPushedAllowPedGeneration;

	if(s_bTeleportPlayerForTest)
	{
		camInterface::GetDebugDirector().DeactivateFreeCam();
		CControlMgr::SetDebugPadOn(false);
	}

	RestoreNormalPedVars();
};


static void InitPedsLowLOD()
{
	StoreNormalPedVars();
	CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance = 0.0f;
	CPedAILodManager::ms_fMotionTaskPedLodDistance = 0.0f;
	CPedAILodManager::ms_fMotionTaskPedLodDistanceOffscreen = 0.0f;
	CPedAILodManager::ms_fEventScanningLodDistance = 0.0f;
	CPedAILodManager::ms_fRagdollInVehicleDistance = 0.0f;
	CVehiclePopulation::ms_overrideNumberOfParkedCars = 0;
	InitPedsCore();
}

static void InitPedsNormalLOD()
{
	StoreNormalPedVars();	// Store 'em so they can be put back
	// Default values
	//	CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance = 30.0f;
	//	CPedAILodManager::ms_fMotionTaskPedLodDistance = 30.0f;
	//	CPedAILodManager::ms_fEventScanningLodDistance = 30.0f;
	//	CPedAILodManager::ms_fRagdollInVehicleDistance = 30.0f;
	CVehiclePopulation::ms_overrideNumberOfParkedCars = 0;
	InitPedsCore();
}

static void InitPedsHighLOD()
{
	StoreNormalPedVars();
	CPedAILodManager::ms_fPhysicsAndEntityPedLodDistance = 1000.0f;
	CPedAILodManager::ms_fMotionTaskPedLodDistance = 1000.0f;
	CPedAILodManager::ms_fMotionTaskPedLodDistanceOffscreen = 1000.0f;
	CPedAILodManager::ms_fEventScanningLodDistance = 1000.0f;
	CPedAILodManager::ms_fRagdollInVehicleDistance = 1000.0f;
	CVehiclePopulation::ms_overrideNumberOfParkedCars = 0;
	InitPedsCore();
}


//////////////////////////////////////////////////////////////////////////
// Pershing Square

void InitPershingSquare()
{
	if(s_bTeleportPlayerForTest)
	{
		Vector3 vPlayerPos(132.5f,-986.1f,29.4f);

		Matrix34 cameraMtx;
		cameraMtx.Identity();
		cameraMtx.d.Set(vPlayerPos - Vector3(3.0f,0,-1.0f));
		cameraMtx.MakeRotateZ(200 * DtoR);

		debugLocation loc;
		loc.Set(vPlayerPos, cameraMtx, 14);
		loc.BeamMeUp();
	}

	if(!NetworkInterface::IsGameInProgress())
	{
		CVehiclePopulation::RemoveAllVehsHard();
		CPedPopulation::RemoveAllPedsHardNotPlayer();
	}

	s_iPushedMaxCars = CVehiclePopulation::ms_overrideNumberOfCars;
	CVehiclePopulation::ms_overrideNumberOfCars = s_iMaxVehicles;

	s_iPushedMaxAmbientVehicles = CVehiclePopulation::ms_iAmbientVehiclesUpperLimit;
	CVehiclePopulation::ms_iAmbientVehiclesUpperLimit = s_iMaxVehicles;

	s_bPushedAllowPedGeneration = gbAllowAmbientPedGeneration;
	gbAllowAmbientPedGeneration = true;

	s_bPushedDisableRandomTrains = CTrain::sm_bDisableRandomTrains;
	CTrain::sm_bDisableRandomTrains = false;

	s_bPushedNoCops = CVehiclePopulation::ms_noPoliceSpawn;
	CVehiclePopulation::ms_noPoliceSpawn = true;

	CVehiclePopulation::ForcePopulationInit();

	CPedPopulation::ms_popCycleOverrideNumberOfAmbientPeds = s_iMaxAmbientPeds;
	CPedPopulation::ms_popCycleOverrideNumberOfScenarioPeds = 0;

	CPedPopulation::ForcePopulationInit();
}

void ShutdownPershingSquare()
{
	CVehiclePopulation::ms_overrideNumberOfCars = s_iPushedMaxCars;
	CVehiclePopulation::ms_iAmbientVehiclesUpperLimit = s_iPushedMaxAmbientVehicles;

	CTrain::sm_bDisableRandomTrains = s_bPushedDisableRandomTrains;
	CVehiclePopulation::ms_noPoliceSpawn = s_bPushedNoCops;

	CPedPopulation::ms_popCycleOverrideNumberOfAmbientPeds = s_iPushedMaxAmbientPeds;
	CPedPopulation::ms_popCycleOverrideNumberOfScenarioPeds = s_iPushedMaxScenarioPeds;
	gbAllowAmbientPedGeneration = s_bPushedAllowPedGeneration;

	if(s_bTeleportPlayerForTest)
	{
		camInterface::GetDebugDirector().DeactivateFreeCam();
		CControlMgr::SetDebugPadOn(false);
	}

	RestoreNormalPedVars();
}

///////////////////////////////////////////////////////////////////////////////
// Test cases

struct TestCase
{
	const char* name;	

	void (*InitFn)();
	void (*ShutdownFn)();
	void (*UpdateFn)();

	const char* desc;
};

static TestCase s_TestCases[] =
{
	//	name								init function						shutdown function	update function		description
	{	"Cars, LOD Normal",					&InitCarsLodNormal,					&ShutdownCars,		NULL,				"Heavy traffic location, vehicle pop maxed, normal LOD settings."},
	{	"Cars, LOD Real",					&InitCarsForceLodReal,				&ShutdownCars,		NULL,				"Heavy traffic location, vehicle pop maxed, LOD force to Real."},
	{	"Cars, LOD Real (close only)",		&InitCarsForceLodReal,				&ShutdownCars,		&UpdateCarsCloseOnly,"Heavy traffic location, vehicle pop maxed, LOD force to Real.  Delete vehicles that are far away to prevent fallthrough."},
	{	"Cars, LOD Dummy",					&InitCarsForceLodDummy,				&ShutdownCars,		NULL,				"Heavy traffic location, vehicle pop maxed, LOD force to Dummy."},
	{	"Cars, LOD Super Dummy",			&InitCarsForceLodSuperDummy,		&ShutdownCars,		NULL,				"Heavy traffic location, vehicle pop maxed, LOD force to Super Dummy."},
	{	"Cars, LOD Super Dummy Inactive",	&InitCarsForceLodSuperDummyInactive,&ShutdownCars,		NULL,				"Heavy traffic location, vehicle pop maxed, LOD force to Super Dummy Inactive."},
	{	"Cars, Cycle LOD",					&InitCarsLodNormal,					&ShutdownCars,		&UpdateCarsCycleLod,"Heavy traffic location, vehicle pop maxed, cycle through vehicle LODs."},
	{	"Peds, All Low LODs",				&InitPedsLowLOD,					&ShutdownPeds,		NULL,				"Heavy ped density. Force All to Low LODs"},
	{	"Peds, Normal LODs",				&InitPedsNormalLOD,					&ShutdownPeds,		NULL,				"Heavy ped density. Normal LODs"},
	{	"Peds, All High LODs",				&InitPedsHighLOD,					&ShutdownPeds,		NULL,				"Heavy ped density. Force All to High LODs"},
	{	"Pershing Square, Cars and Peds Normal", &InitPershingSquare,			&ShutdownPershingSquare, NULL,			"Pershing Square, vehicle pop maxed, normal LOD settings."}
};

void CTestCase::StopTestCase()
{
	if (ms_CurrTestCaseIndex != -1)
	{
		if (s_TestCases[ms_CurrTestCaseIndex].ShutdownFn) 
		{
			s_TestCases[ms_CurrTestCaseIndex].ShutdownFn();
		}
	}
	ms_CurrTestCaseIndex = -1;
}

void CTestCase::StartTestCase()
{
	StopTestCase();
	ms_CurrTestCaseIndex = ms_TestCaseIndex;
	if (ms_CurrTestCaseIndex != -1)
	{
		if (s_TestCases[ms_CurrTestCaseIndex].InitFn) 
		{
			s_TestCases[ms_CurrTestCaseIndex].InitFn();
		}

		Vector2 position(0.1f, 0.2f);
		grcDebugDraw::Text(position, Color_white, s_TestCases[ms_CurrTestCaseIndex].desc, false, 1.0f, 1.0f, 60);
	}
}

void CTestCase::SetTestCase()
{
	if (ms_TestCaseIndex != ms_CurrTestCaseIndex)
	{
		StopTestCase();
	}
}

void CTestCase::UpdateTestCase()
{
	if (ms_CurrTestCaseIndex != -1)
	{
		if (s_TestCases[ms_CurrTestCaseIndex].UpdateFn)
		{
			s_TestCases[ms_CurrTestCaseIndex].UpdateFn();
		}
	}
}

void CTestCase::AddWidgets(bkBank &bank)
{
	atArray<const char*> names;
	for (int i = 0; i < NELEM(s_TestCases); ++i)
	{
		names.PushAndGrow(s_TestCases[i].name);
	}

	bank.AddCombo("Test Cases", &ms_TestCaseIndex, names.GetCount(), names.GetElements(), SetTestCase);
	bank.AddButton("Start", StartTestCase);
	bank.AddButton("Stop", StopTestCase);
	bank.AddToggle("Teleport player for test", &s_bTeleportPlayerForTest);
	bank.AddToggle("Stop Player Processing", &s_bPreventPlayerProcessForTest);
	bank.AddSlider("Shared - Max Num Vehicles", &s_iMaxVehicles, 0, 200, 1);
	bank.AddSlider("Shared - Max Num Ambient Peds", &s_iMaxAmbientPeds, 0, 200, 1);
}

//////////////////////////////////////////////////////////////////////////
// Intentional crashing

void Crash()
{
	*(volatile char*)(0x0) = 0; // Sabotage
}

void RunOutOfMemory()
{
	while (true)
	{
		rage_new char[64 << 10]; // Leak 
	}
}

void CDebug::AddCrashWidgets(bkBank* bank)
{
	bank->PushGroup("Intentional Crashing");
	bank->AddButton("Crash", datCallback(Crash));
	bank->AddButton("Run out of memory", datCallback(RunOutOfMemory));
	bank->PopGroup();
}

//////////////////////////////////////////////////////////////////////////
#endif // DEBUG_BANK
//////////////////////////////////////////////////////////////////////////

#if !__FINAL
//
// name:		CDebug::UpdateProfiler
// description:	
void CDebug::UpdateProfiler()
{
	PF_PUSH_TIMEBAR("update profiler");

#if __STATS
	GetRageProfileRenderer().EndFrame();
	GetRageProfileRenderer().BeginFrame();
	GetRageProfileRenderer().Update();

	GetRageProfileRenderer().UpdateInput();
#endif

	float time = static_cast<float>(fwTimer::GetTimeInMilliseconds());
	ms_locationList.Update(time);
#if ENABLE_STATS_CAPTURE
	MetricsCapture::Update(time);
#endif // ENABLE_STATS_CAPTURE

	PF_POP_TIMEBAR();
}

void CDebug::InitProfiler(unsigned /*initMode*/)
{
#if USE_PROFILER
	Profiler::RegisterCaptureControlCallback(RockyCaptureControlCallback);
#endif

#if !__BANK
	ms_locationList.Init(NULL);
#if ENABLE_STATS_CAPTURE
	MetricsCapture::Init();
	MetricsCapture::SetLocationList(&ms_locationList);
#endif // ENABLE_STATS_CAPTURE
#endif // !__BANK	
}


#endif // !__FINAL

// eof

