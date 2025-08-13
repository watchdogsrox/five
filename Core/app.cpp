//
// name:		app.cpp
// description:	Basic application startup and shutdown
//
// rage headers
#if __XENON
#include "system/xtl.h"
#endif

#include "diag/tracker_remote2.h"
#include "grprofile/timebars.h"
#include "input/dongle.h"
#include "net/nethardware.h"
#include "profile/telemetry.h"
#include "rline/rlnp.h"
#include "rline/rlnptrophy.h"
#include "rline/scmatchmaking/rlscmatchmanager.h"
#include "system/controlMgr.h"
#include "system/fileMgr.h"
#include "system/StreamingInstall.winrt.h"
#include "system/param.h"
#include "system/system.h"
#include "telemetry/telemetry_rage.h"
#include "bink/movie.h"


//framework
#include "fwnet/netremotelog.h"
#include "fwsys/timer.h"

// game headers
#include "app.h"
#include "game.h"
#include "memorytest.h"

#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "audiohardware/driver.h"
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "control/gamelogic.h"
#include "Control/replay/Replay.h"
#include "core/dongle.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/debug.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/loadingscreens.h"
#include "frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "frontend/WarningScreen.h"
#include "game/Clock.h"
#include "ik/IkManager.h"
#include "modelinfo/mlomodelinfo.h"
#include "network/live/livemanager.h"
#include "network/events/NetworkEventTypes.h"
#include "network/Objects/NetworkObjectPopulationMgr.h"
#include "network/Sessions/NetworkSession.h"
#include "tools/SectorTools.h"
#include "tools/TracingStats.h"
#include "objects/DummyObject.h"
#include "pathserver/navgenparam.h"
#include "profile/cputrace.h"
#include "renderer/renderThread.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/DrawLists/drawListNY.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "saveload/savegame_autoload.h"
#include "saveload/GenericGameStorage.h"
#include "scene/scene.h"
#include "script/script.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "system/EntitlementManager.h"
#include "system/PMCPerfControl.h"
#include "system/gcmPerfmon.h"
#include "system/param.h"
#include "system/taskscheduler.h"
#include "system/telemetry.h"
#include "vehicles/vehiclepopulation.h"
#include "security/ragesecengine.h"
#include "Weapons/Projectiles/Projectile.h"
#if  __PPU
#include "sn/libsn.h"
#include <stdlib.h> // for malloc_stats
#include <sys/memory.h>

#include <np.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_gamecontent.h>
#endif // __PPU

#if __WIN32PC && __STEAM_BUILD
#pragma warning(disable: 4265)
#include "../../3rdParty/Steam/public/steam/steam_api.h"
#pragma warning(error: 4265)
#pragma comment(lib, "steam_api64.lib")
#include "data/certificateverify.h"
#include "diag/seh.h"
#endif

#if EPIC_API_SUPPORTED
#include "system/epic.h"
#endif

#if RSG_LAUNCHER_CHECK

#define CHECK_BOOTSTRAP_PROCESS "PlayGTAV.exe"
#define CHECK_PARENT_PROCESS	"GTAVLauncher.exe"
#define CHECK_MTL_PROCESS		"Launcher.exe"
#define CHECK_MTL_STEAMHELPER	"RockstarSteamHelper.exe"

#include "system/xtl.h"
#include <tlhelp32.h>
#include <Psapi.h>
#endif // RSG_LAUNCHER_CHECK

#include "Control/replay/ReplaySettings.h"
#include "Network/Objects/NetworkObjectMgr.h"

#if !__FINAL
PARAM(trackerreport, "[GAME] Filename for the initial memory track");
namespace rage 
{
	XPARAM(forceboothdd);
	XPARAM(noSocialClub);
	XPARAM(BlockOnLostFocus);
	extern char* XEX_TITLE_ID_HDD;	//	defined in gta5\src\dev\game\Core\main.cpp
}

#endif // !__FINAL

#if RAGE_TIMEBARS
#if __FINAL_LOGGING
NOSTRIP_PARAM(dumptimebarsonlongmainthread, "[app] Dump timebars if the main thread takes a certain amount of time (optionally time in ms as argument)");
NOSTRIP_PARAM(disableframemarkers, "[PROFILE] Disable frame encapsulating user markers, make Razor dump all per frame timings into one large bucket");
#else
PARAM(dumptimebarsonlongmainthread, "[app] Dump timebars if the main thread takes a certain amount of time (optionally time in ms as argument)");
PARAM(disableframemarkers, "[PROFILE] Disable frame encapsulating user markers, make Razor dump all per frame timings into one large bucket");
#endif
#endif // RAGE_TIMEBARS

#if RSG_PC
NOSTRIP_PC_XPARAM(benchmark);
#endif

#if RAGE_TRACKING
NOSTRIP_XPARAM(autoMemTrack);
#endif // RAGE_TRACKING

// Builds given to Natural Motion must be dongled. The NM_BUILD preprocessor variable is be defined locally when
// creating a build from NM.
#ifdef NM_BUILD
#define USE_DONGLE	(ENABLE_DONGLE & 1)
#else // NM_BUILD
#define USE_DONGLE	(ENABLE_DONGLE & 1)
#endif // NM_BUILD

#if USE_DONGLE
PARAM(testdongle, "dongle string to encode");
#endif
#if ENABLE_DONGLE
static const char* sEncodeString = "testingtesting123";
static const char* sMCFileName = "root.sys";
#endif
#if !__FINAL
sysTimer initTimer;
#endif
#if __BANK
bool g_maponlyHogEnabled = false;
bool g_cheapmodeHogEnabled = false;
bool g_maponlyExtraHogEnabled = false;
bool g_stealExtraMemoryEnabled = false;
#endif

#if RSG_ORBIS || BACKTRACE_ENABLED
static const size_t sCoredumpHandlerStackSize = 16 * 1024;
static char s_ThreadlStack[256];
static char s_debugInfoSummary[1024];

static void ChanneledOutputForStackTrace(const char* fmt, ...)
{
	char buffer[256];
	va_list args;
	va_start(args, fmt);
	vformatf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	safecat(s_ThreadlStack, buffer);
	safecat(s_ThreadlStack, "\n");
}

#if RSG_ORBIS
static void WriteUserDataAsText(const char* format, ...)
{
	static char formatted_text[1024]; // temporary log buffer
	va_list ap;
	va_start(ap, format);
	int size = vsnprintf(formatted_text, sizeof(formatted_text), format, ap);
	va_end(ap);

	sceCoredumpWriteUserData(formatted_text, size);
}
#else
static HANDLE s_crashContextHandle = INVALID_HANDLE_VALUE;
static void WriteUserDataAsText(const char* format, ...)
{
	if (s_crashContextHandle == INVALID_HANDLE_VALUE)
		return;

	static char formatted_text[1024]; // temporary log buffer
	va_list ap;
	va_start(ap, format);
	int size = vsnprintf(formatted_text, sizeof(formatted_text), format, ap);
	va_end(ap);

	WriteFile(s_crashContextHandle, formatted_text, (DWORD)size, NULL, NULL);
}
#endif

//The maximum total size for user data is 524288 bytes (512 Kb)
static int CoredumpHandler(void * UNUSED_PARAM(pCommon))
{
	WriteUserDataAsText("###### DEBUG INFO ######\n\n");

	//-------
// 	int streamVirtual=0, streamPhysical=0;
// 	CStreamingDebug::CalculateRequestedObjectSizes(streamVirtual, streamPhysical);
// 	streamVirtual  >>= 10;
// 	streamPhysical >>= 10;
	u32 milliseconds = fwTimer::GetSystemTimeInMilliseconds();
	u32 seconds = (u32) (milliseconds / 1000) % 60 ;
	u32 minutes = (u32) ((milliseconds / (1000*60)) % 60);
	u32 hours   = (u32) ((milliseconds / (1000*60*60)) % 24);
	char gameState[32];
	CApp* app = CApp::GetInstance();
	if (app)
	{
		switch (app->GetState())
		{
		case CApp::State_InitSystem:		sprintf(gameState, "System Init");		break;
		case CApp::State_InitGame:			sprintf(gameState, "Game Init");		break;
		case CApp::State_RunGame:			sprintf(gameState, "Game Running");		break;
		case CApp::State_ShutdownGame:		sprintf(gameState, "Game Shutdown");	break;
		case CApp::State_ShutdownSystem:	sprintf(gameState, "System Shutdown");	break;
		default:							sprintf(gameState, "Unknown");			break;
		}
	}
	WriteUserDataAsText("============ SYSTEM INFO ============\n");
	WriteUserDataAsText("Game State : %s\n", gameState);
	WriteUserDataAsText("System Time (%dh %dm %ds)\n", hours, minutes, seconds);
	WriteUserDataAsText("Raw System Time (%dms)\n", sysTimer::GetSystemMsTime());
	WriteUserDataAsText("Frame (%d) - Paused (%s)\n", fwTimer::GetFrameCount(), fwTimer::IsGamePaused()?"yes":"no");
	WriteUserDataAsText("Streaming Requests (%d)\n", strStreamingEngine::GetInfo().GetNumberRealObjectsRequested_Cached());
	WriteUserDataAsText("  HDD: %s, ODD: %s\n", IsDeviceLocked(pgStreamer::HARDDRIVE)?"locked":"unlocked", IsDeviceLocked(pgStreamer::OPTICAL)?"locked":"unlocked");
	//WriteUserDataAsText("Streaming Memory (Physical:%dKb | Virtual:%dKb)\n", streamPhysical, streamVirtual);
	strStreamingAllocator& strAlloc = strStreamingEngine::GetAllocator();
	WriteUserDataAsText("Streaming Memory Occupancy :\n");
	WriteUserDataAsText("  Physical : used(%dMb) - available(%dMb)\n", strAlloc.GetPhysicalMemoryUsed_Cached()>>20, strAlloc.GetPhysicalMemoryAvailable_Cached()>>20);
	WriteUserDataAsText("  Virtual  : used(%dMb) - available(%dMb)\n", strAlloc.GetVirtualMemoryUsed_Cached()>>20, strAlloc.GetVirtualMemoryAvailable_Cached()>>20);
	WriteUserDataAsText("  External : physical used(%dMb) - virtual used (%dMb)\n\n", strAlloc.GetExternalPhysicalMemoryUsed_Cached()>>20, strAlloc.GetExternalVirtualMemoryUsed_Cached()>>20);

	//-------
	WriteUserDataAsText("============ MISC INFO ============\n");
	WriteUserDataAsText("Version : %s\n", CDebug::GetVersionNumber());
		
	// Player and Camera Pos, Game time, current vehicle/mission
	Vector3 pos   = CGameWorld::FindLocalPlayerCoors_Safe();
	Vector3 posn  = camInterface::GetPos();
	Vector3 front = camInterface::GetFront();
	WriteUserDataAsText("Player Position (%f;%f;%f)\n", pos.x, pos.y, pos.z);
	WriteUserDataAsText("Camera Position (%f;%f;%f)\n", posn.x, posn.y, posn.z);
	WriteUserDataAsText("Camera Front (%f;%f;%f)\n", front.x, front.y, front.z);
	WriteUserDataAsText("Game Time (%d:%d)\n", CClock::GetHour(), CClock::GetMinute());
	WriteUserDataAsText("Wanted Level : %d\n", CGameWorld::sm_localPlayerWanted);
	WriteUserDataAsText("IK : %u\n", CBaseIkManager::GetDebugInfoAsUInt());

	// Arrest State
	{
		char arrested[32];
		eArrestState arrestState = CGameWorld::sm_localPlayerArrestState;
		switch (arrestState)
		{
		case eArrestState::ArrestState_Arrested:	sprintf(arrested, "Arrested");	break;
		case eArrestState::ArrestState_Max:			sprintf(arrested, "Max");		break;
		case eArrestState::ArrestState_None:
		default:									sprintf(arrested, "None");		break;
		}
		WriteUserDataAsText("Arrest State : %s\n", arrested);
	}
	
	// Player Health and Death State
	{
		char deathStr[32];
		eDeathState deathState = CGameWorld::sm_localPlayerDeathState;
		switch (deathState)
		{
		case eDeathState::DeathState_Alive:			sprintf(deathStr, "Alive");		break;
		case eDeathState::DeathState_Dead:			sprintf(deathStr, "Dead");		break;
		case eDeathState::DeathState_Dying:			sprintf(deathStr, "Dying");		break;
		case eDeathState::DeathState_Max:			sprintf(deathStr, "Max");		break;
		default:									sprintf(deathStr, "Unknown");	break;
		}
		WriteUserDataAsText("Player Health : %0.2f/%0.2f\n", CGameWorld::sm_localPlayerHealth, CGameWorld::sm_localPlayerMaxHealth);
		WriteUserDataAsText("Death State : %s\n", deathStr);
	}

	// Language
	{
		char sysLanguage[32];
		u32 language = CPauseMenu::sm_languageFromSystemLanguage;
		switch (language)
		{
		case LANGUAGE_ENGLISH:				sprintf(sysLanguage, "English");			break;
		case LANGUAGE_FRENCH:				sprintf(sysLanguage, "French");				break;
		case LANGUAGE_GERMAN:				sprintf(sysLanguage, "German");				break;
		case LANGUAGE_ITALIAN:				sprintf(sysLanguage, "Italian");			break;
		case LANGUAGE_SPANISH:				sprintf(sysLanguage, "Spanish");			break;
		case LANGUAGE_PORTUGUESE:			sprintf(sysLanguage, "Portuguese");			break;
		case LANGUAGE_POLISH:				sprintf(sysLanguage, "Polish");				break;
		case LANGUAGE_RUSSIAN:				sprintf(sysLanguage, "Russian");			break;
		case LANGUAGE_KOREAN:				sprintf(sysLanguage, "Korean");				break;
		case LANGUAGE_CHINESE_TRADITIONAL:  sprintf(sysLanguage, "Chinese");			break;
		case LANGUAGE_CHINESE_SIMPLIFIED:	sprintf(sysLanguage, "ChineseSimplified");	break;
		case LANGUAGE_JAPANESE:				sprintf(sysLanguage, "Japanese");			break;
		case LANGUAGE_MEXICAN:				sprintf(sysLanguage, "Mexican");			break;
		case LANGUAGE_UNDEFINED:
		default:							sprintf(sysLanguage, "Undefined");			break;
		}
		WriteUserDataAsText("System Language : %s\n", sysLanguage);
	}

	WriteUserDataAsText("Current Mission : %s\n", (CTheScripts::GetPlayerIsOnAMission() || CTheScripts::GetPlayerIsOnARandomEvent()) ? CDebug::GetCurrentMissionName() : "None");
	WriteUserDataAsText("Current Vehicle : %s\n", CGameWorld::GetFollowPlayerCarName() ? CGameWorld::GetFollowPlayerCarName() : "None");

	// __BANK has a lot more info since we can afford it
#if __BANK
	if( CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning() )
	{
		const CutSceneManager::DebugRenderState& cutsceneState = CutSceneManager::GetDebugRenderState();
		WriteUserDataAsText("Current Cutscene : %s\n", cutsceneState.cutsceneName);
	}
	else
		WriteUserDataAsText("Current Cutscene : None\n");
	
	CDebugScene::GetDebugSummary(s_debugInfoSummary, true);
	WriteUserDataAsText("%s", s_debugInfoSummary);
#else
	//num peds
	CPed::Pool * pPedPool = CPed::GetPool();
	if (pPedPool)
	{
		int totalCount = pPedPool->GetNoOfUsedSpaces();
		int otherCount   = CPedPopulation::ms_nNumOnFootOther + CPedPopulation::ms_nNumInVehOther;
		int missionCount = 0;
		int reusedCount = 0;
		int reusePool = 0;
		for(int i=0; i<pPedPool->GetSize(); i++)
		{
			if(const CPed * pPed = pPedPool->GetSlot(i))
			{
				missionCount += (pPed->PopTypeIsMission() ? 1 : 0);
				reusedCount += (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedWasReused) ? 1 : 0);
				reusePool += (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool) ? 1 : 0);
			}
		}

		WriteUserDataAsText("Peds: %d/%d (mission: %d, scenario (foot/car) %d/%d, ambient: (foot/car) %d/%d, other: %d, reusePool: %d) Reused: %d\n",
			totalCount, pPedPool->GetSize(), missionCount,
			CPedPopulation::ms_nNumOnFootScenario,
			CPedPopulation::ms_nNumInVehScenario,
			CPedPopulation::ms_nNumOnFootAmbient,
			CPedPopulation::ms_nNumInVehAmbient, otherCount, reusePool, reusedCount);
	}

	//num cars
	CVehicle::Pool * pVehiclePool = CVehicle::GetPool();
	if (pVehiclePool)
	{
		int iVehiclePoolSize, iTotalNumVehicles, iNumMission, iNumParked, iNumAmbient, iNumReal, iNumRealTraffic, iNumDummy, iNumSuperDummy, iNumInReusePool, iNumBeingDeleted, iNumNetRegistered, iNumUnknown; 
		iTotalNumVehicles = 0;
		iNumMission = 0;
		iNumParked = 0;
		iNumAmbient = 0;
		iNumReal = 0;
		iNumRealTraffic = 0;
		iNumDummy = 0;
		iNumSuperDummy = 0;
		iNumInReusePool = 0;
		iNumBeingDeleted = 0;
		iNumNetRegistered = 0;
		iNumUnknown = 0;
		s32 i = (s32) pVehiclePool->GetSize();
		while(i--)
		{
			CVehicle * pVehicle = pVehiclePool->GetSlot(i);
			if (pVehicle)
			{
				iTotalNumVehicles++;

				if(pVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
				{
					iNumBeingDeleted++;
				}

				if(pVehicle->GetNetworkObject())
				{
					iNumNetRegistered++;
				}

				if(pVehicle->GetIsInReusePool()) 
				{
					// in reuse pool
					iNumInReusePool++;
					continue; // don't count vehicles in the reuse pool towards the population counts
				}

				if (pVehicle->PopTypeIsMission())
				{
					if (pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation)
						iNumParked++;
					else
						iNumMission++;
				}
				else if(pVehicle->PopTypeIsRandom())
				{
					if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
						iNumParked++;
					else
						iNumAmbient++;
				}
				else
				{
					iNumUnknown++;
				}

				switch(pVehicle->GetVehicleAiLod().GetDummyMode())
				{
				case VDM_REAL:
					{
						iNumReal++;
						if(pVehicle->PopTypeGet() == POPTYPE_RANDOM_AMBIENT || pVehicle->PopTypeGet()== POPTYPE_RANDOM_PATROL)
						{
							iNumRealTraffic++;
						}
						break;
					}
				case VDM_DUMMY: { iNumDummy++; break; }
				case VDM_SUPERDUMMY: { iNumSuperDummy++; break; }
				default: { Assert(false); }
				}

			}
		}
		iVehiclePoolSize = (int) pVehiclePool->GetSize();
		WriteUserDataAsText("Cars: %d/%d/%d (ambient: %d, mission: %d, parked: %d, reusePool: %d unknown: %d deleted: %d net: %d) (real: %d(%d traffic), dummy: %d, superdummy: %d)\n",
			iTotalNumVehicles, iVehiclePoolSize, CVehiclePopulation::GetDesiredNumberAmbientVehicles_Cached(), iNumAmbient,
			iNumMission, iNumParked, iNumInReusePool, iNumUnknown, iNumBeingDeleted, iNumNetRegistered,
			iNumReal, iNumRealTraffic, iNumDummy, iNumSuperDummy);
	}

	// Objects
	CObject::Pool* objPool = CObject::GetPool();
	if (objPool)
	{
		int totalObjects = (int) objPool->GetNoOfUsedSpaces();
		int numOfPickups, numOfDoors, numOfWeapons, numOfParachutes, numOfCarParachutes, numOfHelmets, numOfProjectiles, numOfProjectileClones, numOfProjectileThrowns, numOfOwnedByScript, numOfOwnedByFrag, numOfOwnedByTemp, numOfOwnedByRandom, numOfOwnedByGame, numBeingDeleted;
		numOfPickups = 0;
		numOfDoors = 0;
		numOfWeapons = 0;
		numOfParachutes = 0;
		numOfCarParachutes = 0;
		numOfHelmets = 0;
		numOfProjectiles = 0;
		numOfProjectileThrowns = 0;
		numOfProjectileClones = 0;
		numBeingDeleted=0;
		numOfOwnedByScript = 0;
		numOfOwnedByFrag = 0;
		numOfOwnedByTemp = 0;
		numOfOwnedByRandom = 0;
		numOfOwnedByGame = 0;

		s32 i = (s32) totalObjects;
		while(i--)
		{
			CObject * pObject = objPool->GetSlot(i);
			if (pObject)
			{
				if(pObject->IsPickup())
				{
					numOfPickups++;
				}
				if(pObject->IsADoor())
				{
					numOfDoors++;
				}
				if(pObject->GetWeapon())
				{
					numOfWeapons++;
				}
				if(pObject->GetIsParachute())
				{
					numOfParachutes++;
				}
				if(pObject->GetIsCarParachute())
				{
					numOfCarParachutes++;
				}
				if(pObject->m_nObjectFlags.bIsHelmet)
				{
					numOfHelmets++;
				}
				if(pObject->GetAsProjectile())
				{
					numOfProjectiles++;

					const CProjectile* pProjectile = static_cast<const CObject *>(pObject)->GetAsProjectile();
					if(pProjectile && pProjectile->GetNetworkIdentifier().IsClone())
					{
						numOfProjectileClones++;
					}
				}
				if(pObject->GetAsProjectileThrown())
				{
					numOfProjectileThrowns++;
				}
				if(pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
				{
					numOfOwnedByScript++;
				}
				if(pObject->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
				{
					numOfOwnedByFrag++;
				}
				if(pObject->GetOwnedBy() == ENTITY_OWNEDBY_TEMP)
				{
					numOfOwnedByTemp++;
				}
				if(pObject->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM)
				{
					numOfOwnedByRandom++;
				}
				if(pObject->GetOwnedBy() == ENTITY_OWNEDBY_GAME)
				{
					numOfOwnedByGame++;
				}
				if(pObject->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
				{
					numBeingDeleted++;
				}
			}
		}

		WriteUserDataAsText("Objects: %d/%d (Pickup:(%d), Doors:(%d), Weapons:(%d), Parachutes:(%d), CarParachutes:(%d), Helmets:(%d), Projectiles:(%d/%d/%d), OwnedByScript:(%d), OwnedByFrag:(%d), OwnedByTemp:(%d), OwnedByRand:(%d), OwnedByGame:(%d), BeingDeleted:(%d)",
			totalObjects, objPool->GetSize(), numOfPickups, numOfDoors, numOfWeapons, numOfParachutes, numOfCarParachutes, numOfHelmets, numOfProjectiles, numOfProjectileThrowns, numOfProjectileClones, numOfOwnedByScript, numOfOwnedByFrag, numOfOwnedByTemp, numOfOwnedByRandom, numOfOwnedByGame, numBeingDeleted);
	}

	CDummyObject::Pool* dummyObjPool = CDummyObject::GetPool();
	if(dummyObjPool)
	{
		int totalDummy = (int) dummyObjPool->GetNoOfUsedSpaces();
		WriteUserDataAsText("Dummy Objects: %d/%d\n",
			totalDummy, dummyObjPool->GetSize());
	}

	// Display active (and total) peds, ragdolls, objects, and vehicles.
	phLevelNew* phLvl = CPhysics::GetLevel();
	if (phLvl)
	{
		s32 instPedsActive = 0;
		s32 instPedsTotal = 0;
		s32 instVehiclesActive = 0;
		s32 instVehiclesTotal = 0;
		s32 instObjectsTotal = 0;
		s32 instObjectsActive = 0;
		s32 instRagdollsActive = 0;
		s32 instRagdollsTotal = 0;
		for(int i = 0; i < phLvl->GetMaxObjects(); ++i)
		{
			if(!phLvl->IsNonexistent(i))
			{
				phInst* pInst = phLvl->GetInstance(i);
				CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
				if(pEntity && pInst->GetClassType() != PH_INST_MAPCOL)
				{
					bool bActive = phLvl->IsActive(pInst->GetLevelIndex());
					if(pEntity->GetIsTypeVehicle())
					{
						instVehiclesTotal++;
						instVehiclesActive += bActive ? 1 : 0;
					}
					if(pEntity->GetIsTypeObject())
					{
						instObjectsTotal++;
						instObjectsActive += bActive ? 1 : 0;
					}
					if(pEntity->GetIsTypePed())
					{
						instPedsTotal++;
						instPedsActive += bActive ? 1 : 0;
						if(((CPed*)pEntity)->GetRagdollState()==RAGDOLL_STATE_PHYS)
						{
							instRagdollsTotal++;
							instRagdollsActive += bActive ? 1 : 0;
						}
					}
				}
			}
		}

		WriteUserDataAsText("Physics instances (active/total): objects %d/%d, cars: %d/%d, peds: %d/%d, ragdolls: %d/%d\n",
			instObjectsActive, instObjectsTotal, instVehiclesActive, instVehiclesTotal,
			instPedsActive, instPedsTotal, instRagdollsActive, instRagdollsTotal);
	}
#endif

	// Not available in final builds :(
#if !__NO_OUTPUT
	CInteriorInst* pIntInst = NULL;
	s32 roomIdx = -1;
	pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	roomIdx  = CPortalVisTracker::GetPrimaryRoomIdx();

	if (pIntInst && roomIdx>0 && roomIdx!=INTLOC_INVALID_INDEX)
	{
		// get the interior and room names
		CMloModelInfo *pModelInfo = reinterpret_cast<CMloModelInfo*>(pIntInst->GetBaseModelInfo());

		const char* interiorName = "Unable to find Interior";
		if (pModelInfo)
		{
			interiorName = pModelInfo->GetModelName();
		}

		const char* roomName = "Unable To Find Room";
		if (pModelInfo)
		{
			roomName = pModelInfo->GetRooms()[roomIdx].m_name;
		}

		CInteriorProxy* pInteriorProxy = pIntInst->GetProxy();
		WriteUserDataAsText("Interior:<%s (%d)>; Room:<%s (%d)>; Group:<%d>\n\n", interiorName, CInteriorProxy::GetPool()->GetJustIndex(pInteriorProxy), roomName, roomIdx, pInteriorProxy->GetGroupId());
	}
	else
	{
		WriteUserDataAsText("Exterior\n\n");
	}
#endif

	//-------
	WriteUserDataAsText("============ NETWORK INFO ============\n");
	WriteUserDataAsText("Network Opening : %s\n", NetworkInterface::IsNetworkOpening() ? "Yes" : "No");
	WriteUserDataAsText("Network Open : %s\n", NetworkInterface::IsNetworkOpen() ? "Yes" : "No");
	WriteUserDataAsText("Network Closing : %s\n", NetworkInterface::IsNetworkClosing() ? "Yes" : "No");
	WriteUserDataAsText("Network Close Pending : %s\n", NetworkInterface::IsNetworkClosePending() ? "Yes" : "No");
	WriteUserDataAsText("Network Forcing Crash On Shutdown While Processing : %s\n", NetworkInterface::IsForcingCrashOnShutdownWhileProcessing() ? "Yes" : "No");

	if (NetworkInterface::IsGameInProgress())
	{
		WriteUserDataAsText("Game In Progress : Yes\n");
		WriteUserDataAsText("Number of physical players : %d\n", NetworkInterface::GetNumPhysicalPlayers());
		// 	WriteUserDataAsText("Network session state : %d\n", CNetwork::GetNetworkSession().GetStateAsInt());
		// 	WriteUserDataAsText("Network session transition state: %d\n", CNetwork::GetNetworkSession().GetTransitionStateAsInt());
		WriteUserDataAsText("Network session valid : %s\n", CNetwork::IsNetworkSessionValid() ? "Yes" : "No");
		if(CNetwork::IsNetworkSessionValid())
		{
			WriteUserDataAsText(CNetwork::GetNetworkSession().IsSessionActive() ? "Network Session Active : Yes\n" : "Network Session Active : No\n");
			WriteUserDataAsText("Network session transition member count : %d\n", CNetwork::GetNetworkSession().GetTransitionMemberCount());
			WriteUserDataAsText("Network Session in transition : %s\n", CNetwork::GetNetworkSession().IsTransitionActive() ? "Yes" : "No");
		}
		
		s32 fDesiredNumAmbientVehicles = static_cast<s32>(CVehiclePopulation::GetDesiredNumberAmbientVehicles());
		s32	TotalAmbientCarsOnMap      = CVehiclePopulation::ms_numRandomCars;

		s32 TargetNumLocalVehicles = CNetworkObjectPopulationMgr::GetLocalTargetNumVehicles();
		s32 TotalNumLocalVehicles  = CNetworkObjectPopulationMgr::GetNumCachedLocalVehicles();

		s32 pedMemoryUse     = CPopulationStreaming::GetCachedTotalMemUsedByPeds()     >> 20;
		s32 vehicleMemoryUse = CPopulationStreaming::GetCachedTotalMemUsedByVehicles() >> 20;
		s32 pedBudget        = gPopStreaming.GetCurrentMemForPeds()                    >> 20;
		s32 vehicleBudget    = gPopStreaming.GetCurrentMemForVehicles()                >> 20;

		WriteUserDataAsText("Ped Memory Use/Budget : %d/%d\n", pedMemoryUse, pedBudget);
		WriteUserDataAsText("Vehicle Memory Use/Budget : %d/%d\n", vehicleMemoryUse, vehicleBudget);
		WriteUserDataAsText("Reassignment in progress : %s\n", netObjectReassignMgr::IsReassignmentInProgress_Cached() ? "Yes" : "No");
		WriteUserDataAsText("Total Ambient Cars On Map : %d\n", TotalAmbientCarsOnMap);
		WriteUserDataAsText("Desired Number of Ambient Vehicles : %d\n", fDesiredNumAmbientVehicles);
		WriteUserDataAsText("Total Number of Local Vehicles : %d\n", TotalNumLocalVehicles);
		WriteUserDataAsText("Target Number of Local Vehicles : %d\n\n", TargetNumLocalVehicles);
				
		netGameEvent::Pool *pEventPool = netGameEvent::GetPool();
		if (pEventPool)
		{
			s32 usedEvents = pEventPool->GetNoOfUsedSpaces();
			s32 freeSpaces = pEventPool->GetNoOfFreeSpaces();
			int total = usedEvents+freeSpaces;
			WriteUserDataAsText("Network Event Usage: %d/%d\n", usedEvents, total);
		}

		const static int NUM_EVENT_TYPES_TO_LOG = 5;

		for(int i=0; i < NUM_EVENT_TYPES_TO_LOG; i++)
		{
			if (CNetworkEventsDumpInfo::ms_eventTypesCount[i].count > 0)
			{
				WriteUserDataAsText("Network Events Type: %s  Count %d\n", CNetworkEventsDumpInfo::ms_eventTypesCount[i].eventName, CNetworkEventsDumpInfo::ms_eventTypesCount[i].count);
			}
		}

		for (auto it = CNetworkEventsDumpInfo::ms_scriptedEvents.Begin(); it != CNetworkEventsDumpInfo::ms_scriptedEvents.End(); ++it)
		{
			WriteUserDataAsText("Network Scripted Events: %s  Count %d\n", (*it).eventName, (*it).count);
		}

		for(int i = 0; i < (int)NetworkObjectTypes::NUM_NET_OBJ_TYPES; i++)
		{
			WriteUserDataAsText("Sync Tree %d", i);
			CProjectSyncTree* tree = NetworkInterface::GetObjectManager().GetSyncTree((NetworkObjectType)i);

			if (tree)
			{
				const atFixedArray<netSyncDataNode*, netSyncDataNode::MAX_SYNC_DATA_NODES>& nodes = tree->GetDataNodes();

				for (int sn = 0; sn < nodes.GetCount(); sn++)
				{
					auto dataNode = nodes[sn];
					if (dataNode)
					{
						WriteUserDataAsText("\tNode %d Updated: %s\n", sn, nodes[sn]->GetWasUpdated() ? "TRUE" : "FALSE");
					}
					else
					{
						WriteUserDataAsText("\tNode %d is null\n", sn);
					}
				}
			}
			else
			{
				WriteUserDataAsText("\tTree is null\n");
			}
		}
	}
	else
	{
		WriteUserDataAsText("Game In Progress : No\n\n");
	}

	//-------
	WriteUserDataAsText("============ SCALEFORM INFO ============\n");
	WriteUserDataAsText("Last Movie : %s\n",                      CScaleformMgr::sm_LastMovieName);
	WriteUserDataAsText("Last ActionScript Method : %s\n",        CScaleformMgr::sm_LastMethodName);
	if (CScaleformMgr::sm_LastMethodParamName)
		WriteUserDataAsText("Last ActionScript Method Params : %s\n", CScaleformMgr::sm_LastMethodParamName);
	WriteUserDataAsText("Is Inside ActionScript : %s\n", CScaleformMgr::sm_bIsInsideActionScript?"yes":"no");
	WriteUserDataAsText("Active Movies\n");
	for(s32 movieIdx=0 ; movieIdx<CScaleformMgr::sm_ScaleformArray.GetCount(); ++movieIdx)
	{
		if (CScaleformMgr::IsMovieActive(movieIdx))
			WriteUserDataAsText(" - MovieId(%3d) Filename(%s)\n", CScaleformMgr::sm_ScaleformArray[movieIdx].iMovieId, CScaleformMgr::sm_ScaleformArray[movieIdx].cFilename);
	}

	//-------
	WriteUserDataAsText("\n============ SCRIPT THREAD INFO ============\n");
	WriteUserDataAsText("Number of scrThreads : %d\n", GtaThread::CountActiveThreads());
	// Not thread safe !
// 	WriteUserDataAsText("scrThreads stack traces :\n\n");
// 	for (int threadIdx = 0; threadIdx < scrThread::GetThreadCount(); ++threadIdx)
// 	{
// 		scrThread::GetThreadByIndex(threadIdx).PrintStackTrace(ChanneledOutputForStackTrace);
// 		if (s_ThreadlStack[0] != 0)
// 		{
// 			WriteUserDataAsText(s_ThreadlStack);
// 			s_ThreadlStack[0] = 0;
// 			WriteUserDataAsText("----\n");
// 		}
// 	}

	if (scrThread::GetActiveThreadGlobal())
	{
		GtaThread *pActiveThread = (GtaThread*) scrThread::GetActiveThreadGlobal();
		WriteUserDataAsText("Active scrThread %s PC=%d Handler=0x%p State=%d\n", pActiveThread->GetScriptName(), pActiveThread->GetProgramCounter(0), pActiveThread->m_Handler, (s32) pActiveThread->GetState());
		WriteUserDataAsText("Active scrThread StackTrace :\n");
		pActiveThread->PrintStackTrace(ChanneledOutputForStackTrace);
		if (s_ThreadlStack[0] != 0)
		{
			WriteUserDataAsText(s_ThreadlStack);
			s_ThreadlStack[0] = 0;
			WriteUserDataAsText("\n");
		}
	}
	else
	{
		WriteUserDataAsText("No Active scrThread\n");
	}

	//-------
	WriteUserDataAsText("============ MOVIE INFO ============\n");
	bwMovie::CoredumpHandler(BACKTRACE_ONLY(s_crashContextHandle));

	//-------
	WriteUserDataAsText("\n###### END OF DEBUG INFO ######");

	return 0;
}
#endif //RSG_ORBIS || BACKTRACE_ENABLED

static PMCPerfControl*	s_perfControl;

// This bool is set on the end of the FsmEnter. It is used by various systems involved in tight loops
// (Warning Screen, Game Session State machine) to drive some specific rendering behaviour. Please
// be careful of side effects if modiying g_gameRunning
bool g_GameRunning = false;

CApp* CApp::sm_Self = NULL;

// A dummy sync point to be able to perform tuner captures from the main thread's point of view
// and perform loading tuner captures.
#if RSG_PC
bool CApp::IsGameRunning()
{
	return (sm_Self && (sm_Self->GetState() == State_RunGame) && camInterface::IsFadedIn() && !fwTimer::IsGamePaused()) ? true : false;
}
#endif // RSG_PC

fwFsm::Return CApp::UpdateState(const s32 state, const s32, const fwFsm::Event event)
{
	fwFsmBegin

		fwFsmState(State_InitSystem)
			fwFsmOnUpdate
				return InitSystem();

		fwFsmState(State_InitGame)
			fwFsmOnUpdate
				return InitGame();

		fwFsmState(State_RunGame)
			fwFsmOnEnter
				return RunGame_OnEnter();
			fwFsmOnUpdate
				return RunGame();

		fwFsmState(State_ShutdownGame)
			fwFsmOnUpdate
				return ShutdownGame();

		fwFsmState(State_ShutdownSystem)
			fwFsmOnUpdate
				return ShutdownSystem();

	fwFsmEnd
}

#if __PPU
void CheckGameContent()
{
	int ret = 0;
	unsigned int type = 0;
	unsigned int attributes = 0;
	CellGameContentSize content_size;

	char directoryName[CELL_GAME_DIRNAME_SIZE];

	cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_GAME);

	ret = cellGameBootCheck(&type, &attributes, &content_size, directoryName);

	if (CELL_GAME_RET_OK == ret)
	{
		char contentInfoPath[CELL_GAME_PATH_MAX];
		char usrdirPath[CELL_GAME_PATH_MAX];

		ret = cellGameContentPermit(contentInfoPath, usrdirPath);

		if (CELL_GAME_RET_OK == ret)
		{
			if (CELL_GAME_GAMETYPE_HDD == type)
			{
				Displayf("contentInfoPath = %s", contentInfoPath);
				Displayf("usrdirPath = %s", usrdirPath);
				Warningf("I AM TRYING TO BOOT FROM THE HARD DRIVE.  THIS MAY NOT BE WHAT YOU WANTED.");
				Warningf("In the XMB, Debug Settings > Game Type (Debugger) should be 'Disc Boot Game' and GameContentUtil Boot Path (Debugger) should be 'For Development'.");
				if (strlen(usrdirPath) > 0)
				{
					if (usrdirPath[strlen(usrdirPath)-1] != '/')
					{
						strncat(usrdirPath, "/", CELL_GAME_PATH_MAX);
					}
				}

				CFileMgr::SetGameHddBootPath(usrdirPath);
			}
		}
		else
		{
			Assertf(0, "CheckGameContent - cellGameContentPermit failed");
		}
	}
	else
	{
		Assertf(0, "CheckGameContent - cellGameBootCheck failed");
	}

	cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);

#if !__FINAL
	if (PARAM_forceboothdd.Get())
	{
		char buf[128] = {0};
		formatf(buf, "/dev_hdd0/game/%s/USRDIR/", XEX_TITLE_ID_HDD);
		CFileMgr::SetGameHddBootPath(buf);
	}
#endif // !__FINAL
}
#endif // __PPU

// PURPOSE: Initialise the basic systems.
fwFsm::Return CApp::InitSystem()
{
#if RSG_ORBIS
	s_ThreadlStack[0] = 0;
	s_debugInfoSummary[0] = 0;
	ASSERT_ONLY( int ret = ) sceCoredumpRegisterCoredumpHandler(CoredumpHandler, sCoredumpHandlerStackSize, (void*)0);
	Assert(ret == SCE_OK);
#endif //RSG_ORBIS

	//@@: location CAPP_INITSYSTEM
#if RSG_PC && __ASSERT
	// On PC, we need to enable queueing bugs for logging as soon as possible
	diagLogBugCallback = CDebug::OnLogBug;
#endif

#if RSG_PC
	diagTerminateCallback = CApp::WriteExitFile;
#endif

#if RSG_PC && __STEAM_BUILD
	rtry
	{
		wchar_t steamPath[MAX_PATH] = {0};
		rcheck(GetModuleFileNameW(NULL, steamPath, sizeof(steamPath)), catchall, );

		wchar_t* lastSlash = wcsrchr(steamPath, '\\');
		if (lastSlash == NULL)
		{
			lastSlash = wcsrchr(steamPath, '/');
		}

		if (lastSlash != NULL)
		{
			lastSlash[1] = L'\0';
		}

		safecat(steamPath, L"steam_api64.dll");

		rverify(CertificateVerify::Verify(steamPath, NULL), catchall, gnetError("Certification Verification failed for steam"));
		
		rcheck(gnetVerifyf(SteamAPI_Init(), "SteamAPI_Init failed - Is Steam running and are you signed in to a R* developer account?"), catchall, );
		rverify(SteamUser(), catchall, gnetError("SteamUser() was null"));
		g_rlPc.RefreshSteamAuthTicket(false);
	}
	rcatchall
	{
		Errorf("Could not initialize Steam API!! Make sure you have the Steam_api64.dll in the same directory as the game and you launched the game through Steam");
	}

#endif

#if EPIC_API_SUPPORTED
	const char* epicProductName = "GTAV"; // arbitrary, matches value set in launcher
	const char* epicProductVersion = "1.0"; // arbitrary, matches value set in launcher
	const char* epicProductId = "68d2cc08f9a94b8fb51af4f5cfa6d41b";
	const char* epicSandboxId = "0584d2013f0149a791e7b9bad0eec102";
	const char* epicClientId = "c19208b6eec5423d951186aee0ad7b9c";
	const char* epicClientSecret = "YaEL5raCmwNB4TuZf5AU";
	const char* epicDeploymentId = "f8cb4ad3fab34f4ca902480ae7875a82";

	if (!g_SysEpic.InitClass(epicProductName, epicProductVersion, epicProductId, epicSandboxId, epicClientId, epicClientSecret, epicDeploymentId))
	{
		Errorf("Could not initialize Epic API!! Make sure you have the EOSSDK-Win64-Shipping.dll in the same directory as the game and you launched the game through Epic");
	}
#endif

#if RSG_LAUNCHER_CHECK 
	if (!CheckParentProcess())
	{
		// We don't want crash dumps for launcher check
		RAGE_PC_FATAL_SILENT(ERR_NO_LAUNCHER);
	}
#endif

#if RAGE_TIMEBARS
	float maxAllowedTime = 0.0f;

	// If the user wants to dump timebars if the main thread takes a long time...
	if (PARAM_dumptimebarsonlongmainthread.Get())
	{
		// See if the user also specified the amount of ms that we may take.
		PARAM_dumptimebarsonlongmainthread.Get(maxAllowedTime);

		// If not, default to 200ms.
		if (!maxAllowedTime)
		{
			maxAllowedTime = 200.0f;
		}
	}
#endif // RAGE_TIMEBARS

	PF_INIT_TIMEBARS("Main", MAIN_THREAD_MAX_TIMEBARS, maxAllowedTime);

	PF_INIT_STARTUPBAR("Startup", 1000);
	PF_BEGIN_STARTUPBAR();
	PF_START_STARTUPBAR("System Init");
 
	pm_Init();
	
	CDebug::SetPrinterFn();
	// need to call this before the drawlist mgr is initialised
	InitNYDrawListDebug();

#if !__FINAL || !__NO_OUTPUT
	netRemoteLog::InitClass("gta5");
#endif

#if __PPU
	// Copied from rage\base\src\rline\rlnp.cpp
	ASSERT_ONLY(int errorCode = ) sceNp2Init(rlNp::GetSceNpPoolSize(), rlNp::GetSceNpPoolMemory());
	Assertf(errorCode == 0, "CApp::InitGame - sceNp2Init failed - error code = %x", errorCode);

	CheckGameContent();
#endif // __PPU


#if !__FINAL || !RSG_PC
	if(!CSystem::Init("Fuzzy"))
#else
	if(!CSystem::Init("Grand Theft Auto V"))
#endif
	{
		SetState(State_ShutdownGame);
		return fwFsm::Continue;
	}

#if __STEAM_BUILD
	int steamAppId = CGameConfig::Get().GetConfigOnlineServices().m_SteamAppId;
	if (SteamAPI_RestartAppIfNecessary(steamAppId))
	{
		// We don't want crash dumps for steam check
		RAGE_PC_FATAL_SILENT(ERR_NO_STEAM);
	}
#endif

	// Dongle Validation
#if ENABLE_DONGLE
#if USE_DONGLE 
	CheckDongle();
#endif
	const char* cVersionNumber = CDebug::GetVersionNumber();
	if(cVersionNumber && !strncmp(cVersionNumber, "NM", 2))
		CheckDongle();
#endif

	InitNYDrawList();

	SetState(State_InitGame);
	return fwFsm::Continue;
}

XPARAM(maponly);
XPARAM(maponlyextra);
XPARAM(cheapmode);
XPARAM(nocars);
XPARAM(nopeds);
XPARAM(noambient);
XPARAM(marketing);
XPARAM(stealextramemory);

// PURPOSE: Initialise the game systems.
fwFsm::Return CApp::InitGame()
{
	//does this need to be called? doing so here seems to remove all the 
	//time stuff tracked from CApp::InitSystem() 
    //PF_BEGIN_STARTUPBAR();

	PF_START_STARTUPBAR("PreLoadingScreensInit");
#if RSG_PC
	CGame::InitCallbacks();
#endif
	CGame::PreLoadingScreensInit();

	// map the desired platform: device
#if __PS3 && !__FINAL
	sys_memory_info_t mem_info;
	sys_memory_get_user_memory_size( &mem_info );	
	bool runningOnDevKit = mem_info.total_user_memory > 256 << 20;
#else 
	bool runningOnDevKit = !__FINAL;
#endif // __PS3


	if ( runningOnDevKit && (PARAM_maponly.Get() || PARAM_maponlyextra.Get() || PARAM_cheapmode.Get())){
#if !__FINAL && !defined(NAVGEN_TOOL)
		// alloc 2x10Mb chunks
		CFileMgr::AllocateDummyResourceMem();
		PARAM_nocars.Set("true");
		PARAM_nopeds.Set("true");
		PARAM_noambient.Set("true");
		BANK_ONLY(g_maponlyHogEnabled = true);

		if (PARAM_cheapmode.Get())
		{
			BANK_ONLY(g_cheapmodeHogEnabled = true;)
		}	

		if (PARAM_maponlyextra.Get())
		{
			CFileMgr::AllocateExtraResourceMem((10 << 20) + (4 << 20));
			BANK_ONLY(g_maponlyExtraHogEnabled = true;)
		}
#endif //!__FINAL
	}

#if !__FINAL && !defined(NAVGEN_TOOL)
	int size = 0;
	if (PARAM_stealextramemory.Get(size) && size)
	{
		CFileMgr::AllocateExtraResourceMem(size);
		BANK_ONLY(g_stealExtraMemoryEnabled = true;)
	}
#endif //!__FINAL
#if(BUGSTAR_INTEGRATION_ACTIVE)
	CBugstarIntegration::Init();
#endif

#if __XENON && __INCLUDE_MEMORY_TEST_IN_XENON_BUILD
	// Perform startup memory test if LT+RT+LB+RB are held down on either pad
	CMemoryTest::StartTest();
#endif

#if TRACING_STATS
	gTracingStats.Init();
#endif

	PF_START_STARTUPBAR("CText init");
	CText::Init(INIT_CORE);			
	
	PF_START_STARTUPBAR("CLoadingScreens init");
	CLoadingScreens::Init( LOADINGSCREEN_CONTEXT_LEGALMAIN, 0);
	
	if (!StreamingInstall::HasInstallFinished())
	{
		CWarningScreen::Init(INIT_CORE);
		CGameStreamMgr::Init(INIT_CORE);

		PF_PUSH_STARTUPBAR("Streaming install");
        StreamingInstall::BlockUntilFinished();
        CFileMgr::CleanupAsyncInstall();
		PF_POP_STARTUPBAR();
	}
#if RSG_DURANGO
	else
	{
		// to shutdown the resume watcher and package transfer watcher started at init.
		// Durango only, to avoid messing with other platforms (B*2564598)
		StreamingInstall::Shutdown();
	}
#endif	// RSG_DURANGO

	PF_START_STARTUPBAR("Game Init");
	CGame::Init();

#if RSG_PC
	//Allow device resets to occur now that the game is fully initialized.  Request a devcie reset through the settings manager to ensure that all game properties are properly initialized.
//	GRCDEVICE.SetIgnoreDeviceReset(false);
//	CSettingsManager::GetInstance().RequestNewSettings(CSettingsManager::GetInstance().GetCopyOfSettings());
#endif

	SetState(State_RunGame);
	return fwFsm::Continue;
}


// PURPOSE: Game initialiation before game loop
fwFsm::Return CApp::RunGame_OnEnter()
{
	//////////////////////////////////////////////////////////////////////////
	// required for various Sony TRC bugs related to deleting of trophy data
	// and attempting to re-unlock trophies. This could all go away if we stop
	// caching trophy bits in the saved game profile.
#if RSG_NP
	// wait until trophy status is known (installed or not) before loading profile
	rlNpTrophy::WaitUntilTrophyStatusIsKnown();

	// write out profile if the trophy data has been re-installed on startup (due to being missing)
	if (rlNpTrophy::GetHasTrophyDataBeenReinstalled())
	{
		CLiveManager::GetAchMgr().ClearAchievements();
		CProfileSettings::GetInstance().WriteNow(true);
		rlNpTrophy::SetHasTrophyDataBeenReinstalled(false);
	}
#endif
	//////////////////////////////////////////////////////////////////////////

	// This shouldn't be here, we've already called this function much much sooner than this and we're throwing out nearly 30s of data.
	// PF_BEGIN_STARTUPBAR();

	CGame::UpdateSessionState();

#if !__FINAL && RAGE_TRACKING
	const char* trackerReport = NULL;
	if (PARAM_trackerreport.Get(trackerReport))
		RAGE_TRACK_REPORT_NAME(trackerReport);
#endif

	RAGETRACE_INIT();
	RAGETRACE_INITTHREAD("Main", 1024, 1);

	//gRenderThreadInterface.Init();		// create the render thread and let it take control of the d3d device

	PF_START_STARTUPBAR("Setup devices");
	CFileMgr::SetupDevicesAfterInit();

#if __PPU
	struct std::malloc_managed_size mms;
	std::malloc_stats(&mms);
	if (mms.current_system_size > 0) {
		Errorf("**** current system usage %dK; in use is %dk",mms.current_system_size>>10,mms.current_inuse_size>>10);
	}
#endif

	s_perfControl = rage_new PMCPerfControl();

#if !__FINAL
	Printf("Game init took %f seconds\n", initTimer.GetTime());
#endif

#if !__FINAL
	for(s32 bucket = 0; bucket < 16; ++bucket)
	{
		sysMemAllocator::SetBucketName(bucket, sysMemGetBucketName((MemoryBucketIds)bucket));
	}
#endif

	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->UpdateMemorySnapshot();
	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->UpdateMemorySnapshot();
	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_PHYSICAL)->UpdateMemorySnapshot();
	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->UpdateMemorySnapshot();

	g_GameRunning = true;

	PF_FINISH_STARTUPBAR();

	SetInstance(this);
#if RSG_PC
	grcResourceCache::SetGameRunningCallback(IsGameRunning);

	GRCDEVICE.SetDisablePauseOnLostFocusSystemOverride(false);
	GRCDEVICE.LockDeviceResetAvailability();

	if (g_rlPc.GetGamepadManager())
	{
#if __FINAL
		g_rlPc.GetGamepadManager()->SetBackgroundInputEnabled(!GRCDEVICE.IsAllowPauseOnFocusLoss());
#else
		bool bGameWillPauseOnFocusLoss = GRCDEVICE.IsAllowPauseOnFocusLoss() && PARAM_BlockOnLostFocus.Get();
		g_rlPc.GetGamepadManager()->SetBackgroundInputEnabled(!bGameWillPauseOnFocusLoss);
#endif
	}
#endif // RSG_PC

	return fwFsm::Continue;
}


// PURPOSE: Main game loop.
fwFsm::Return CApp::RunGame()
	{
#if RAGE_TIMEBARS
		if (!PARAM_disableframemarkers.Get())
		{
			char buffer[32];
			formatf(buffer, "Frame %u", fwTimer::GetFrameCount());
			pfTimebarFuncs::g_PushMarkerCb(buffer, 0);
		}
#endif // RAGE_TIMEBARS

#if STALL_DETECTION
		if (!	(	CPauseMenu::IsActive()
			||	camInterface::IsFadedOut() 
			||	CLoadingScreens::AreActive()
			)
			)
		{
			m_StallDetection.BeginFrame();
		}
#endif // STALL_DETECTION

#if RAGE_TRACKING
		if ( PARAM_autoMemTrack.Get() )
		{	
			static int loopCount = 0;
			
			if( loopCount == 60 )
			{
				diagTracker* t = diagTracker::GetCurrent();
				
				if( t )
				{
					((diagTrackerRemote2*)t)->RequestDump();
				}
			}
			
			loopCount++;
		}
#endif


		PF_FRAMEINIT_TIMEBARS(gDrawListMgr->GetUpdateThreadFrameNumber());
		RAGETRACE_FRAMESTART();
		//@@: location CAPP_RUNGAME
		PF_PUSH_TIMEBAR_BUDGETED("System begin", 0.1f);
		s_perfControl->StartFrame();

#if RAGE_TIMEBARS
		// Indicate what kind of build this is in PIX/Tuner, so we now what's going on when looking at captures.
		{
			char buildType[256];
			char version[32];

			formatf(buildType, RSG_CONFIGURATION " %s %s", CDebug::GetPackageType(), CDebug::GetFullVersionString(version, sizeof(version)));
			pfAutoMarker versionMarker(buildType, 6);
		}
#endif // RAGE_TIMEBARS

#if GTA_REPLAY
		CReplayMgr::StartFrame();
#endif // GTA_REPLAY

#if __PPU && __DEV
		snSafeLoadPoint();
#endif

		RAGETRACE_PUSHNAME("CSystem::BeginUpdate");
		CSystem::BeginUpdate();
		RAGETRACE_POP();

#if (SECTOR_TOOLS_EXPORT)
		CSectorTools::Run( );
#endif // OBJECT_CHECKER_EXPORT
		PF_POP_TIMEBAR(); // "System begin"

		CGame::Update();

#if DEBUG_SCREENSHOTS
		PF_START_TIMEBAR_DETAIL("Screenshots");
		CDebug::HandleScreenShotRequests();
#endif

		pm_Display();

		//***RENDERTHREAD LOOP********************************************
		CTaskScheduler::StartBatchingTasks(5);

		gRenderThreadInterface.GPU_IdleSection();		// call stuff which needs done when GPU is idle
		gRenderThreadInterface.Synchronise();

		CControlMgr::StartUpdateThread();
		CGame::Render();		// it's called render, but it won't really - it just builds the draw lists & contexts
		CControlMgr::WaitForUpdateThread();
		//******************************************************

#if USE_TELEMETRY
		CTelemetry::Update();
		TelemetryUpdate();
#endif

		//@@: location CAPP_RUNGAME_AUDIO_WAIT
		PF_PUSH_TIMEBAR_BUDGETED("AudioWait", 1.0f);
		if (!CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame()
			UI_LANDING_PAGE_ONLY(&& !CLandingPageArbiter::IsLandingPageActive())
			)
		{
			audNorthAudioEngine::FinishUpdate();
		}
		PF_POP_TIMEBAR();

#if GTA_REPLAY
		if(audDriver::GetMixer() && audDriver::GetMixer()->IsCapturing() && audDriver::GetMixer()->IsFrameRendering())
		{
			const float kfFrameStep = 1.0f / 31.0f;
			// Only trigger an extra engine update when exporting at 30fps or less to make sure enough audio is generated this frame
			if(audDriver::GetMixer()->GetTotalAudioTimeNs() < audDriver::GetMixer()->GetTotalVideoTimeNs() && CReplayMgr::GetExportFrameStep() >= kfFrameStep)
			{
				g_AudioEngine.TriggerUpdate();
				g_AudioEngine.WaitForAudioFrame();
			}

			CReplayPlaybackController const& c_replayPlaybackController = CReplayCoordinator::GetReplayPlaybackController();

			while (audDriver::GetMixer()->GetTotalAudioTimeNs() < audDriver::GetMixer()->GetTotalVideoTimeNs() && c_replayPlaybackController.IsExportingAudioFrameCaptureAllowed())
			{
				audDriver::GetMixer()->EnterReplaySwitchLock();
				audDriver::GetMixer()->TriggerUpdate();
				audDriver::GetMixer()->WaitOnMixBuffers();
				audDriver::GetMixer()->ExitReplaySwitchLock();
				//Displayf("Catch Up Audio: %LLu chasing (%LLu)", 
				//	NANOSECONDS_TO_ONE_H_NS_UNITS(audDriver::GetMixer()->GetTotalAudioTimeNs()), 
				//	NANOSECONDS_TO_ONE_H_NS_UNITS(audDriver::GetMixer()->GetTotalVideoTimeNs()));			
			}
		}

		if(audDriver::GetMixer()->m_ShouldStartFixedFrameRender)
		{
			audDriver::GetMixer()->m_ShouldStartFixedFrameRender = false;
			audDriver::GetMixer()->SetReplayMixerModeRendering();
		}
		
		if(audDriver::GetMixer()->m_ShouldStopFixedFrameRender)
		{
			audDriver::GetMixer()->m_ShouldStopFixedFrameRender = false;
			audDriver::GetMixer()->SetReplayMixerModeLoading();
		}
#endif

#if __DEV
		CGameWorld::AllowDelete(true);
#endif

#if GTA_REPLAY
		PF_PUSH_TIMEBAR_IDLE("ReplayMgrPostRender");
		CReplayMgr::PostRender();
		PF_POP_TIMEBAR();
#endif
		s_perfControl->EndFrame();

#if TRACING_STATS
		gTracingStats.Process();
#endif

#if RSG_PC
		CApp::CheckExit();
#else
		if (CApp::WantToExit())
		{
			SetState(State_ShutdownGame);
		}
#endif


#if __PS3 && !__FINAL
	pfDummySync();
#endif // __PS3 && !__FINAL

	RAGETRACE_PUSHNAME("CSystem::EndUpdate");
	CSystem::EndUpdate();
	RAGETRACE_POP();

	m_StallDetection.EndFrame();

#if RAGE_TIMEBARS
	if (!PARAM_disableframemarkers.Get())
	{
		pfTimebarFuncs::g_PopMarkerCb(0);
	}
#endif // RAGE_TIMEBARS

	return fwFsm::Continue;
}

#if RSG_PC

static bool sbDisabledScui = false;
static bool sbClosedScui = false;
static bool sbWarningAccept = false;
static bool sbWarningTriggered = false;

void GracefullyShutdownNetwork()
{
#if __STEAM_BUILD
	if (SteamUser())
	{
		CSteamID steamId = SteamUser()->GetSteamID();
		SteamUser()->EndAuthSession(steamId);
	}
#endif

#if EPIC_API_SUPPORTED
	g_SysEpic.ShutdownClass();
#endif

	CNetwork::Bail(sBailParameters(BAIL_EXIT_GAME, BAIL_CTX_EXIT_GAME_FROM_APP));
	CNetwork::ShutdownConnectionManager();
	rlShutdown();
	netShutdown();
}

void ProcessScuiCloseLogic()
{
	if(g_rlPc.GetUiInterface())
	{
		if (!sbClosedScui && g_rlPc.GetUiInterface()->IsVisible())
		{
			Displayf("Closing SCUI for Quit Game message");
			g_rlPc.GetUiInterface()->CloseUi();
			sbClosedScui = true;
		}

		if (!sbDisabledScui)
		{
			Displayf("Disabling SCUI for Quit Game message");
			g_rlPc.GetUiInterface()->EnableHotkey(false);
			sbDisabledScui = true;
		}
	}
}

bool CApp::IsShutdownConfirmed()
{
	return sbWarningAccept || CPauseMenu::GetGameWantsToExitToWindows();
}

bool CApp::IsScuiDisabledForShutdown()
{
	return sbDisabledScui || sbClosedScui || sbWarningTriggered;
}

void CApp::CheckExit()
{
	// ===========================================
	//   PROCESSING THE STARTING OF THE EXIT FLOW 
	// ===========================================
	if (GRCDEVICE.QueryCloseWindow() || g_rlPc.HasUserRequestedShutdownViaScui() || CSystem::WantToRestart())
	{
		if(!NetworkInterface::GetNetworkExitFlow().IsExitFlowRunning())
		{
			// Network game is in progress, start the exit flow
			if(NetworkInterface::IsGameInProgress())
			{
				NetworkInterface::GetNetworkExitFlow().StartExitSaveFlow();
			}
			// If network game is not in progress, the user may have requested a restart. Thus, they have already
			// accepted the warning
			else if (CSystem::WantToRestart())
			{
				sbWarningTriggered = false;
				sbWarningAccept = true;
			}
			// Otherwise just go for straight to the warning message
			else
			{
				#if RSG_PC
				if (PARAM_benchmark.Get())
				{
					sbWarningTriggered = false;
					sbWarningAccept = true;
				}
				else
				#endif
				{
					sbWarningTriggered = true;
				}
			}
		}

		// If we have yet to close the SCUI, but the interface is valid and visible, close it and disable reopening.
		ProcessScuiCloseLogic();
	}

	// ===========================================
	// PROCESSING THE UI WARNINGS OF THE EXIT FLOW
	// ===========================================
	// If the exit flow is running and it hasn't finished, display the spinner warning
	if (NetworkInterface::GetNetworkExitFlow().IsSaveFlowRunning() && !NetworkInterface::GetNetworkExitFlow().IsSaveFlowFinished())
	{
		// Game Restart comes from the Pause Menu which the user has already accepted
		if (CSystem::WantToRestart())
		{
			CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS2", NetworkInterface::GetNetworkExitFlow().GetExitString(), FE_WARNING_SPINNER);
		}
		else
		{
			CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS2", "EXIT_SURE_2", FE_WARNING_SPINNER);
		}
	}
	else if (NetworkInterface::GetNetworkExitFlow().IsShutdownTasksRunning() && !NetworkInterface::GetNetworkExitFlow().IsReadyToShutdown())
	{
		CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS2", NetworkInterface::GetNetworkExitFlow().GetExitString(), FE_WARNING_SPINNER);
	}
	else if (NetworkInterface::GetNetworkExitFlow().IsReadyToShutdown())
	{
		CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS2", NetworkInterface::GetNetworkExitFlow().GetExitString(), FE_WARNING_SPINNER);
		sbWarningAccept = true;
	}

	// ===========================================
	//    PROCESSING THE INPUT OF THE EXIT FLOW
	// ===========================================
	if (NetworkInterface::GetNetworkExitFlow().IsSaveFlowFinished() || sbWarningTriggered)
	{
		// If we have yet to close the SCUI, but the interface is valid and visible, close it and disable reopening.
		ProcessScuiCloseLogic();

		if(sbWarningTriggered)
		{
			// Entitlement Manager worker thread is not safe, and must be allowed to finish.
#if RSG_ENTITLEMENT_ENABLED
			if (CEntitlementManager::IsReadyToShutdown())
			{
				CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS2", "EXIT_SURE_2", FE_WARNING_YES_NO);
			}
			else
#endif
			{
				CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS2", "EXIT_SURE_2", FE_WARNING_SPINNER);
			}
		}
		else if (CSystem::WantToRestart())
		{
			NetworkInterface::GetNetworkExitFlow().StartShutdownTasks();
		}
		else
		{
			// Warning messages for Network game, based on whether the save succeeded
			if(NetworkInterface::GetNetworkExitFlow().GetExitFlowState() == NetworkExitFlow::ETD_SAVE_SUCCESS)
			{
				CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS2", "EXIT_SURE_2", FE_WARNING_YES_NO);
			}
			else if(NetworkInterface::GetNetworkExitFlow().GetExitFlowState() == NetworkExitFlow::ETD_SAVE_FAILED)
			{
				CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_FAILED_ON_QUIT", FE_WARNING_YES_NO);
			}
			else
			{
				Assertf(0, "Exit State is incorrect");
			}
		}

		eWarningButtonFlags result = CWarningScreen::CheckAllInput(false);

		if (result == FE_WARNING_YES)
		{
			sbWarningTriggered = false;
			GRCDEVICE.ClearCloseWindow();

			NetworkInterface::GetNetworkExitFlow().StartShutdownTasks();
		}

		if (result == FE_WARNING_NO)
		{
			sbWarningTriggered = false;
			sbWarningAccept = false;
			GRCDEVICE.ClearCloseWindow();

			// Clear SCUI shutdown flags
			sbClosedScui = false;
			sbDisabledScui = false;
			g_rlPc.SetUserRequestedShutdownViaScui(false);

			// Re-Enable SCUI hotkey
			if(g_rlPc.GetUiInterface())
			{
				g_rlPc.GetUiInterface()->EnableHotkey(true);
			}

			// Reset Network shutdown
			if(NetworkInterface::GetNetworkExitFlow().IsExitFlowRunning())
			{
				NetworkInterface::GetNetworkExitFlow().ResetExitFlowState();
			}
		}
	}

	if (CSystem::WantToExit())
	{
		GRCDEVICE.ClearCloseWindow();
	}

	// ===========================================
	//    PROCESSING THE RESTART/SHUT DOWN
	// ===========================================
	if (IsShutdownConfirmed() && !NetworkInterface::GetNetworkExitFlow().IsShutdownTasksRunning())
	{
		bool bWantRestart = false;
		if (CSystem::WantToRestart())
		{
			if (g_rlPc.GetProfileManager() != NULL && g_rlPc.GetProfileManager()->IsSignedIn())
			{
				g_rlPc.GetProfileManager()->CreateSigninTransferProfile();
			}

			if (g_rlPc.GetDownloaderPipe() && g_rlPc.GetDownloaderPipe()->IsConnected())
			{
				g_rlPc.GetDownloaderPipe()->RestartGame();
			}
			else
			{
				bWantRestart = true;
			}
		}

#if USE_RAGESEC
		// By now the tasks and plugins should be canceled but there is a case where
		// not all are so this will help with that. Two updates for good measure
		if (rageSecPluginManager::CancelTasks())
		{
			netTask::Update();
			netTask::Update();
		}
#endif

		GracefullyShutdownNetwork();

#if USE_RAGESEC
		// Turns out on PC we just call Exit(), and I want to do proper memory 
		// sanitization / cleanup / DLL-handle-freeing, so I need to inject
		// myself and shutdown rageSecEngine
		rageSecEngine::Shutdown(rage::SHUTDOWN_CORE);
#endif

		CGenericGameStorage::FinishAllUnfinishedSaveGameOperations();

		WriteExitFile(NULL);

		if (bWantRestart)
		{
			Restart();
		}

		ExitProcess(0);
	}
}

void CApp::Restart()
{
#if RSG_PC
	const int MODULE_FILENAME_LENGTH = 1024;

	TCHAR strModuleFileName[MODULE_FILENAME_LENGTH];

	DWORD dwResult = GetModuleFileName(NULL, strModuleFileName, MODULE_FILENAME_LENGTH);

	if ((dwResult > 0) && (dwResult < MODULE_FILENAME_LENGTH))
	{
		LPTSTR lpCommandLine = GetCommandLine();
		LPTSTR lpCommandLineArg = strstr(lpCommandLine, ".exe");

		if (lpCommandLineArg == NULL)
		{
			lpCommandLineArg = strstr(lpCommandLine, ".EXE");
		}

		if (lpCommandLineArg)
		{
			lpCommandLineArg += 4;

			if (*lpCommandLineArg == '\"')
			{
				lpCommandLineArg++;
			}
		}

		SHELLEXECUTEINFO oShellExecuteInfo;

		memset(&oShellExecuteInfo, 0, sizeof(SHELLEXECUTEINFO));

		oShellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		oShellExecuteInfo.lpVerb = "open";
		oShellExecuteInfo.lpFile = strModuleFileName;
		oShellExecuteInfo.lpParameters = lpCommandLineArg;
		oShellExecuteInfo.nShow = SW_SHOW;

		ShellExecuteEx(&oShellExecuteInfo);
	}
#endif RSG_PC
}

void CApp::WriteExitFile(const char* error)
{
	static bool s_bOpenedExitFile = false;

	// If we've created the out-file already, and have no additional info, there's nothing to do
	if (s_bOpenedExitFile && !error)
	{
		return;
	}

	// Grab the current pid
	DWORD pid = GetCurrentProcessId();
	if (pid == 0)
	{
		return;
	}

	// Check for an in-file
	char path_buffer[MAX_PATH] = {0};
	int formattedLength = snprintf(path_buffer, MAX_PATH, "%s\\in-%u", CFileMgr::GetAppDataRootDirectory(), (unsigned int)pid);
	if (formattedLength >= MAX_PATH)
	{
		return;
	}

	if (!ASSET.Exists(path_buffer, ""))
	{
		// No in-file; we're probably not being run from the launcher
		return;
	}

	// Create the full file path
	formattedLength = snprintf(path_buffer, MAX_PATH, "%s\\out-%u", CFileMgr::GetAppDataRootDirectory(), (unsigned int)pid);

	if (formattedLength >= MAX_PATH)
	{
		return;
	}

	FileHandle handle = NULL;
	
	// If we've already written an out-file this session, append rather than re-create
	if (s_bOpenedExitFile)
	{
		handle = CFileMgr::OpenFileForAppending(path_buffer);
	}
	else
	{
		handle = CFileMgr::OpenFileForWriting(path_buffer);
	}

	if (handle != NULL)
	{
		s_bOpenedExitFile = true;

		// If we have an error, write it out to the out-file
		if (error != NULL)
		{
			CFileMgr::Write(handle, error, static_cast<s32>(strlen(error)));
		}

		CFileMgr::CloseFile(handle);
	}

}

#endif

// PURPOSE: Shutdown game systems.
fwFsm::Return CApp::ShutdownGame()
{
#if __PPU
	// Copied from rage\base\src\rline\rlnp.cpp
	sceNp2Term();
#endif // __PPU

	delete s_perfControl;
	s_perfControl = NULL;
	CGame::Shutdown();
	SetState(State_ShutdownSystem);

	return fwFsm::Continue;
}


// PURPOSE: Shutdown low level systems.
fwFsm::Return CApp::ShutdownSystem()
{
#if RSG_ORBIS
	ASSERT_ONLY( int ret = ) sceCoredumpUnregisterCoredumpHandler();
	Assert(ret == SCE_OK);
#endif //RSG_ORBIS

	CSystem::Shutdown();

#if !__FINAL || !__NO_OUTPUT
	netRemoteLog::ShutdownClass();
#endif

#if TRACING_STATS
	gTracingStats.Shutdown();
#endif

	Displayf("Goodbye");
	return fwFsm::Quit;
}



// PURPOSE: Check whether some system has indicated we need to shutdown.
bool CApp::WantToExit()
{
	return CSystem::WantToExit();
}

// PURPOSE: Check for a valid dongle.
void CApp::CheckDongle()
{
#if ENABLE_DONGLE
	char debugString [500];
	bool checkAuthenticity = true;

#if __PPU || RSG_ORBIS || RSG_DURANGO
	checkAuthenticity = CDongle::ValidateCodeFile(sEncodeString, debugString, sMCFileName);
#else
	checkAuthenticity = ioDongle::ReadCodeFile(sEncodeString, sMCFileName, debugString);
#endif // __PPU

	if(!checkAuthenticity)
	{
		Quitf("Auth fail!");
		while(1){} // Endless Loop
	}
#endif //ENABLE_DONGLE
}

#if RSG_LAUNCHER_CHECK
bool CApp::CheckParentProcess()
{
	PROCESSENTRY32	procentry;
	HMODULE			hMod ;
	char			szFileName[ MAX_PATH ] ;
	DWORD			dwSize2 = 0, pid = 0, crtpid = 0, parentpid = 0;
	BOOL			bContinue = FALSE;
	bool			bConfirmed = false;
	HANDLE			hSnapShot, hProcess;

	atString		launcherProcess;
	atString		bootstrapProcess;
	atString		mtlProcess;
	atString		mtlSteamHelper;

	crtpid = GetCurrentProcessId();

	if (crtpid == 0)
	{
		Errorf("Could not get process ID from the game.");
		return false;
	}

	hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapShot == INVALID_HANDLE_VALUE)
	{
		Errorf("Could not get a valid snapshot of the process.");
		return false;
	}

	procentry.dwSize = sizeof(PROCESSENTRY32);
	bContinue = Process32First(hSnapShot, &procentry);
	if (!bContinue)
	{
		Errorf("Could not get the process information from the snapshot.");
		return false;
	}

	while (bContinue)
	{
		if (crtpid == procentry.th32ProcessID)
			pid = procentry.th32ParentProcessID;

		procentry.dwSize = sizeof(PROCESSENTRY32);
		bContinue = !pid && Process32Next(hSnapShot, &procentry);
	}

	parentpid = pid;
	hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, parentpid);

	if (!hProcess)
	{
		Errorf("Could not do OpenProcess on parent process.");
		return false;
	}

	launcherProcess = CHECK_PARENT_PROCESS;
	launcherProcess.Lowercase();

	bootstrapProcess = CHECK_BOOTSTRAP_PROCESS;
	bootstrapProcess.Lowercase();
	
	mtlProcess = CHECK_MTL_PROCESS;
	mtlProcess.Lowercase();

	mtlSteamHelper = CHECK_MTL_STEAMHELPER;
	mtlSteamHelper.Lowercase();

	bConfirmed = false;
	if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &dwSize2))
	{
		if (GetModuleFileNameEx(hProcess, hMod, szFileName, sizeof(szFileName)))
		{
			const char* exeName = ASSET.FileName(szFileName);
			atString currentName(exeName);
			currentName.Lowercase();

			if (currentName == launcherProcess ||
				currentName == bootstrapProcess ||
				currentName == mtlProcess ||
				currentName == mtlSteamHelper)
			{
				bConfirmed = true;
			}
		}
	}

	CloseHandle(hProcess);

	if (!bConfirmed)
	{
		Errorf("Parent process name does not match the one we are looking for: %s or %s", launcherProcess.c_str(), bootstrapProcess.c_str());
	}

	return bConfirmed;

}

#endif

#if BACKTRACE_ENABLED && (RSG_PC || RSG_DURANGO)
void CApp::WriteCrashContextLog(const wchar_t* path)
{
	s_crashContextHandle = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (s_crashContextHandle != INVALID_HANDLE_VALUE)
	{
		CoredumpHandler(NULL);
		CloseHandle(s_crashContextHandle);
		s_crashContextHandle = INVALID_HANDLE_VALUE;
	}
}

void CApp::CollectAdditionalAttributes()
{
	u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();

	// Game length
	sysException::SetAnnotation("game_lifetime", currentTime);
	// Last Launched Script
	rage::sysException::BacktraceScriptData lastLaunchedScript = sysException::BacktraceGetLastLaunchedScriptData();
	sysException::SetAnnotation("last_launched_script_id", lastLaunchedScript.m_LastLaunchedScriptProgramId);
	sysException::SetAnnotation("time_since_last_launched_script", currentTime - lastLaunchedScript.m_TimeSinceLastLaunchedScript);

	// Network info
	const char* networkState = "unknown";
	if (NetworkInterface::IsGameInProgress())
	{
		networkState = "game_in_progress";
	}
	else if (NetworkInterface::IsNetworkOpening())
	{
		networkState = "opening";
	}
	else if (NetworkInterface::IsNetworkOpen())
	{
		networkState = "open";
	}
	else if (NetworkInterface::IsNetworkClosing())
	{
		networkState = "closing";
	}
	else if (NetworkInterface::IsNetworkClosePending())
	{
		networkState = "close_pending";
	}
	sysException::SetAnnotation("network_state", networkState);

	// Number of players
	int activePlayers;
	if (!NetworkInterface::IsInAnyMultiplayer())
	{
		// We are not in a Multiplayer session so player count is either 0 or 1,
		// If the ped factory has not been init yet, then we are 0
		// Otherwise it depends if we can find a local player
		if(CPedFactory::GetFactory() == NULL)
		{
			activePlayers = 0;
		}
		else if(CGameWorld::FindLocalPlayer())
		{
			activePlayers = 1;
		}
		else
		{
			activePlayers = 0;
		}
	}
	else
	{
		activePlayers = NetworkInterface::GetNumActivePlayers();
	}
	sysException::SetAnnotation("active_players", activePlayers);

	// Ped Entity Count
	sysException::SetAnnotation("entity_count_peds", CPed::GetPool() ? CPed::GetPool()->GetNoOfUsedSpaces() : 0);

	// Vehicles Entity Count
	s32 totalVehicles = 0;
	CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
	if (pVehiclePool)
	{
		s32 i = (s32)pVehiclePool->GetSize();
		while (i--)
		{
			if (pVehiclePool->GetSlot(i))
			{
				++totalVehicles;
			}
		}
	}
	sysException::SetAnnotation("entity_count_vehicles", totalVehicles);

	// Objects Entity Count
	sysException::SetAnnotation("entity_count_objects", CObject::GetPool() ? CObject::GetPool()->GetNoOfUsedSpaces() : 0);

#if RSG_SCARLETT
	const char* gfxModes[] = {
		"Performance",
		"Quality",
		"Performance RT 1440",
		"Performance RT",
	};
	sysException::SetAnnotation("rendering_mode", gfxModes[CSettingsManager::GetInstance().GetPriority()]);
#endif // RSG_SCARLETT
}
#endif //BACKTRACE_ENABLED && (RSG_PC || RSG_XBOX)