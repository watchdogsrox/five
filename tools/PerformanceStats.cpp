//
// filename:	PerformanceStats.cpp
// description:	Game Performance measurement tool.
//
// This is not run at the same time as the other sector tools because we might
// want to run this with Peds, Cars and Ambient Scripts etc.  This unfortunately
// duplicates much of the functionality in the CSectorTools class because of
// this and that we need additional internal passes to look in different
// directions at each sector.
//

// --- Include Files ----------------------------------------------------------------------

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "atl/array.h"
#include "grcore/debugdraw.h"
#include "fwscene/stores/staticboundsstore.h"
#include "grmodel/setup.h"

//framework headers
#include "fwscene/stores/mapdatastore.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "control/gamelogic.h"
#include "grprofile/timebars.h"
#include "tools/PerformanceStats.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "Objects/DummyObject.h"
#include "Objects/object.h"
#include "Objects/ProcObjects.h"
#include "pathserver/PathServer.h"
#include "Peds/ped.h"
#include "Peds/Ped.h"
#include "Peds/pedpopulation.h"
#include "physics\physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/PlantsMgr.h"
#include "renderer/water.h"
#include "scene/scene.h"
#include "scene/world/gameWorld.h"
#include "script/gta_thread.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "streaming/streaming.h"		// For CStreaming::LoadAllRequestedObjects(), etc. /FF
#include "vehicles/vehicle.h"
#include "vehicles/vehiclepopulation.h"

// Platform SDK headers
#if __XENON
#include "system/xtl.h"
#elif __PPU
#include <time.h>
#include <cell/rtc.h>
#include <netex/libnetctl.h>
#endif // Platform SDK headers

#if ( SECTOR_TOOLS_EXPORT )

// --- Defines ----------------------------------------------------------------------------

#define STATSROOT_NETWORK	( "K:\\streamGTA5\\stats\\perfstats" )
#define RAD2DEG				( 180.0f / PI )

// --- Constants --------------------------------------------------------------------------

// --- Structure/Class Definitions --------------------------------------------------------

// --- Globals ----------------------------------------------------------------------------

// --- Static Globals ---------------------------------------------------------------------

static const char* gsTimeBarPerfStats[NUM_TIMEBAR_STATS][2] = {
	// TimeBar Name,					XML/DB Output String
	{ "Update",							"update" },
	{ "Game Update",					"game_update" },
	{ "World Process Control",			"world_process_control" },
	{ "Physics",						"physics" },
	{ "BuildRenderList",				"build_render_list" },
	{ "render",							"render" }
};

// --- Static class members ---------------------------------------------------------------
float								CPerformanceStats::m_startScanY = 700; 
float								CPerformanceStats::m_endScanY	= -500; 
float								CPerformanceStats::m_scanYInc	= -10;
float								CPerformanceStats::m_probeYEnd	= -500; // How far down to cast

		
int									CPerformanceStats::m_nCurrentSectorX = 60;
int									CPerformanceStats::m_nCurrentSectorY = 60;
int									CPerformanceStats::m_nStartSectorX = 60;
int									CPerformanceStats::m_nStartSectorY = 60;
int									CPerformanceStats::m_nEndSectorX = 60;
int									CPerformanceStats::m_nEndSectorY = 60;
CPerformanceStats::etMode			CPerformanceStats::m_eMode = kDisabled;
CPerformanceStats::etPass			CPerformanceStats::m_ePass = kNavmeshLoad;
CPerformanceStats::etDirection		CPerformanceStats::m_eDirection = kNorth;
bool								CPerformanceStats::m_bEnabled = false;
u32								CPerformanceStats::m_msTimer = 0L;
u32								CPerformanceStats::m_msIntervalTimer = 0L;
CPerformanceStats::TSectorData		CPerformanceStats::m_SectorData;
CPerformanceStats::TSectorSamples	CPerformanceStats::m_RunningSamples;
CPerformanceStats::TSectorSamples	CPerformanceStats::m_PausedSamples;
u32								CPerformanceStats::m_msSampleDelay = 5000;
u32								CPerformanceStats::m_msSampleInterval = 500;
char								CPerformanceStats::m_sOutputDirectory[MAX_OUTPUT_PATH];
char								CPerformanceStats::m_sScreenshotDirectory[MAX_OUTPUT_PATH];
char								CPerformanceStats::m_sSampleDelay[20];
char								CPerformanceStats::m_sSampleInterval[20];
char								CPerformanceStats::m_vPassNames[4][15] = { "Navmesh Load", "Loading", "Game Running", "Game Paused" };
char								CPerformanceStats::m_vFacingNames[NUM_DIRECTIONS][10] = { "North", "East", "South", "West" };
bool								CPerformanceStats::m_bSectorOK = true;
rage::fiStream*						CPerformanceStats::m_pProgressFile = NULL;
char								CPerformanceStats::m_description[SECTOR_TOOLS_MAX_DESC_LEN];
int									CPerformanceStats::m_sectorNum = 0;
char								CPerformanceStats::m_result[SECTOR_TOOLS_MAX_RESULT_LEN];
char								CPerformanceStats::m_startTime[SECTOR_TOOLS_MAX_START_TIME_LEN];

PARAM(randSpawnMin,	"min world coords to test player teleport");
PARAM(randSpawnMax,	"max world coords to test player teleport");

// --- Code -------------------------------------------------------------------------------

void 
CPerformanceStats::Init( )
{
	m_bEnabled = false;
	m_RunningSamples.Reset( );
	m_PausedSamples.Reset( );
}

void 
CPerformanceStats::Shutdown( )
{
	m_bEnabled = false;
}

void
CPerformanceStats::SetupWorldCoordinates( )
{
	if ( PARAM_rx.IsInitialized() )
		PARAM_rx.Get( m_nCurrentSectorX );

	if ( PARAM_ry.IsInitialized() )
		PARAM_ry.Get( m_nCurrentSectorY );
}

void
CPerformanceStats::Run( )
{
	if ( !m_bEnabled )
		return;
	if ( kDisabled == GetMode( ) )
		return;

	CPed* pPlayer = FindPlayerPed( );
	Assertf( pPlayer, "Unable to find player ped." );
	float fX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( m_nCurrentSectorX );
	float fY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( m_nCurrentSectorY );
	Vector3 vSectorCentre( fX, fY, 0.0f );
	static Vector3 vOut;

	//Disable player controls so that controller input cannot affect the tests.
	if(pPlayer)
	{
		CPlayerInfo* pPlayerInfo = pPlayer->GetPlayerInfo();
		if(pPlayerInfo)
		{
			pPlayerInfo->DisableControlsScript();
		}
		CPlayerInfo::ms_bDebugPlayerInvincible = true;
	}

	if ( ( kNavmeshLoad == GetPass() ) && ( 0 == GetTimer() ) )
	{
		// In ObjectLoad pass and before our Load Timer has been started
		// Warp player to new sectors coordinates and load objects from
		// streaming system.
		g_weather.ForceTypeNow( 0, CWeather::FT_None );

		// Each time we move to a new sector set game clock dependent 
		// on user-defined var
		if ( CSectorTools::GetMidnight() || (PARAM_runperf.Get() && PARAM_midnight.Get()) )
		{
			if (CClock::GetMode()==CLOCK_MODE_DYNAMIC)
				CClock::SetTime(0, 0, 0);
			else if (CClock::GetMode()==CLOCK_MODE_FIXED)
				CClock::SetTimeSample(0);
		}
		else
		{
			if (CClock::GetMode()==CLOCK_MODE_DYNAMIC)
				CClock::SetTime(12, 0, 0);
			else if (CClock::GetMode()==CLOCK_MODE_FIXED)
				CClock::SetTimeSample(2);
		}	
		
		Vector3 vGuessGround = vSectorCentre;

		// Just for testing this shit - randomised start locations.
		float xyz[3];
		if(PARAM_randSpawnMin.GetArray(xyz, 3))
		{
			Vector3 _min(xyz[0],xyz[1],xyz[2]);
			if (PARAM_randSpawnMax.GetArray(xyz, 3))			
			{
				Vector3 _max(xyz[0],xyz[1],xyz[2]);
				vGuessGround  = Vector3(fwRandom::GetRandomNumberInRange(_min.x, _max.x),fwRandom::GetRandomNumberInRange(_min.y, _max.y),fwRandom::GetRandomNumberInRange(_min.z, _max.z));
			}
		}


		strcpy(m_description,"");
		strcpy(m_result,"ERROR: COLLISION/STREAMING");

		// DW - Request collision and ipls to load in - otherwise collision will NOT be loaded - leading to lost capture data.
		// Total pain in the ass - it takes some doing to get the height of the ground reliably!
		// More than likely some overkill here.
		// Also had some unusual and unexpected results, I think it's possible that even though I thought the stores are entirely 2D quadtrees, somewhere somehow there is a naughty height check?!
		// 200 metres or something? possibly PHYSICS_BB_EXTRASIZE but I'm not 100% 
		for (s32 i=(s32)m_startScanY;i>(s32)m_endScanY;i+=(s32)m_scanYInc)
		{
			vGuessGround.z = float(i);

			INSTANCE_STORE.GetBoxStreamer().RequestAboutPos(RCC_VEC3V(vGuessGround));
			CStreaming::LoadAllRequestedObjects();

			g_StaticBoundsStore.GetBoxStreamer().RequestAboutPos(RCC_VEC3V(vGuessGround));
			CStreaming::LoadAllRequestedObjects();

			pPlayer->Teleport( vGuessGround, pPlayer->GetCurrentHeading() );
			CStreaming::LoadAllRequestedObjects( );		

			WorldProbe::CShapeTestFixedResults<> probeResult;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vGuessGround, Vector3(vGuessGround.x, vGuessGround.y, m_probeYEnd));
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetExcludeEntity(NULL);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeDesc.SetContext(WorldProbe::LOS_Unspecified);

			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				vOut = probeResult[0].GetHitPosition( );
				vOut.z += 0.5f;
				Displayf("*** PERFSTAT_TRACE 0 *** WorldProbe::FindGroundZForCoord ok %f %f %f hit %f %f %f", probeDesc.GetStart().x,probeDesc.GetStart().y,probeDesc.GetStart().z,vOut.x,vOut.y,vOut.z);
				CEntity* pEntity = CPhysics::GetEntityFromInst(probeResult[0].GetHitInst());
				if (pEntity)
				{
					if (CModelInfo::IsValidModelInfo(pEntity->GetModelIndex()))
					{
						snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s ModelIndex %d ModelDesc %s",m_description, pEntity->GetModelIndex(), CModelInfo::GetBaseModelInfoName(pEntity->GetModelId()) );
					}
					else
					{
						snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s model index %d ", m_description, pEntity->GetModelIndex());
					}
				}
				else
				{
					snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s No Model", m_description);
				}
				pPlayer->Teleport( vOut, pPlayer->GetCurrentHeading() );
				break;
			}
			else
			{
				Displayf("*** PERFSTAT_TRACE 1 *** WorldProbe::FindGroundZForCoord failed %f %f %f", probeDesc.GetStart().x,probeDesc.GetStart().y,probeDesc.GetStart().z);

				// check for water
				Vector3 waterLevel;
				Vector3 endPos = vGuessGround;
				endPos.z = m_probeYEnd;
				static bool foundWater = false;
				snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s WorldProbe::FindGroundZForCoord failed %f %f %f",m_description, probeDesc.GetStart().x,probeDesc.GetStart().y,probeDesc.GetStart().z);

				foundWater = Water::TestLineAgainstWater(vGuessGround, endPos, &waterLevel);
				if (foundWater)
				{
					vOut = waterLevel;
					Displayf("*** PERFSTAT_TRACE X *** Found Water %f %f %f", waterLevel.x,waterLevel.y,waterLevel.z);
					pPlayer->Teleport( vOut, pPlayer->GetCurrentHeading() );
					snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s Water",m_description);
				}
			}
		}
	}
	else if ( ( kObjectLoad == GetPass() ) && ( 0 == GetTimer() ) )
	{
		// Teleport player to nearest position on nav-mesh for the specified
		// sector.  We don't want the player falling through the map or in
		// the middle of a sector for these stats.		
		EGetClosestPosRet ret = CPathServer::GetClosestPositionForPed( vOut, vOut, 25.0f );		
		if ( ENoPositionFound != ret ) 
		{
			Displayf("*** PERFSTAT_TRACE 2 *** CPathServer::GetClosestPositionForPed ok %f %f %f",vOut.x,vOut.y,vOut.z);
			m_bSectorOK = true;			
			snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s CPathServer::GetClosestPositionForPed succeeded at %f %f %f", m_description, vOut.x, vOut.y, vOut.z);
		}
		else
		{
			strcpy(m_result,"ERROR: NAVMESH ISSUE");
			Displayf("*** PERFSTAT_TRACE 2 *** CPathServer::GetClosestPositionForPed failed %f %f %f",vOut.x,vOut.y,vOut.z);
			snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s CPathServer::GetClosestPositionForPed failed at %f %f %f", m_description, vOut.x, vOut.y, vOut.z);
		}


		if ( m_bSectorOK )
		{
			char sLogFilename[MAX_OUTPUT_PATH];
			formatf( sLogFilename, MAX_OUTPUT_PATH, "%s\\sector_%d_%d.xml",
				m_sOutputDirectory, m_nCurrentSectorX, m_nCurrentSectorY );

			fiStream* fpLogFile = fiStream::Open( sLogFilename );
			if ( fpLogFile )
			{
				// Don't scan sectors we have already done for this date.
				fpLogFile->Close( );
				m_bSectorOK = false;
				Displayf("*** PERFSTAT_TRACE 3 *** file exists %s",sLogFilename);
			}
			// Teleport the player if the node position is within it, otherwise we
			// set a flag to indicate to the UpdateState function to skip this
			// sector and move along.
			else if ( VecWithinCurrentSector( vOut ) ) // DW - for NOW I just want to get a full capture - lets not invalidate stuff, then refine it's accuracy later.
			{
				Displayf("*** PERFSTAT_TRACE 5 *** sector %d %d (%f,%f)",m_nCurrentSectorX,m_nCurrentSectorY,m_nCurrentSectorX*50.0f,m_nCurrentSectorY*50.0f);
				
				if ( VecWithinCurrentSector( vOut ))
					Displayf("*** PERFSTAT_TRACE 5 *** Vec is in sector %f %f %f",vOut.x,vOut.y,vOut.z);
				else
					Displayf("*** PERFSTAT_TRACE 5 ERROR *** Vec is NOT in sector %f %f %f",vOut.x,vOut.y,vOut.z);

				m_bSectorOK = true;
				pPlayer->Teleport( vOut, pPlayer->GetCurrentHeading() );

				if ( PARAM_halfpop.Get( ) )
				{
					CPedPopulation::SetAmbientPedDensityMultiplier( 0.5f );
					CPedPopulation::SetScenarioPedDensityMultipliers( 0.5f, 0.5f );

					CVehiclePopulation::SetParkedCarDensityMultiplier( 0.5f );
					CVehiclePopulation::SetRandomVehDensityMultiplier( 0.5f );
				}

				pPlayer->GetPlayerWanted()->SetWantedLevel( VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), WANTED_CLEAN, 0, WL_REMOVING );
				pPlayer->SetHealth( 100.0f );

				// Load all objects
				CStreaming::LoadAllRequestedObjects( );
				strcpy(m_result,"OK");
			}
			else
			{
				m_bSectorOK = false;
				strcpy(m_result,"ERROR: NOT IN SECTOR");
				snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s CSectorTools::VecWithinCurrentSector failed %f %f %f",m_description, vOut.x, vOut.y, vOut.z);
				Displayf("*** PERFSTAT_TRACE 6 *** the sector was ok but now it's not.");
				WriteLog( );
			}
		}
		else
		{
			Displayf("*** PERFSTAT_TRACE 7 *** the sector is not ok");
			WriteLog( );
		}
	}
	else if ( ( kGamePaused == GetPass( ) ) || ( kGameRunning == GetPass() ) )
	{
		// Just entered this pass/direction so we orientate the player and the camera.
		if ( 0 == GetTimer() )
		{
			//NOTE: It is not possible to orient the player ped when the game is paused.
			if ( kGameRunning == GetPass() )
			{
				SetPlayerDirection( m_eDirection );
			}

			SetCameraDirection( m_eDirection );

			if ( kGameRunning == GetPass( ) && kSouth == m_eDirection )
			{
				m_SectorData.Reset();
				m_SectorData.nPhysicalObjects = (int) CObject::GetPool()->GetNoOfUsedSpaces();
				m_SectorData.nDummyObjects = (int) CDummyObject::GetPool()->GetNoOfUsedSpaces();
				m_SectorData.nPeds = (int) CPed::GetPool()->GetNoOfUsedSpaces();
				
				// Get active physical objects
				CObject::Pool* pObjPool = CObject::GetPool( );
				for ( int nObj = 0; nObj < pObjPool->GetSize(); ++nObj )
				{
					CObject* pObj = pObjPool->GetSlot( nObj );
					if ( pObj && !pObj->GetIsStatic() )
						++m_SectorData.nActiveObjects;
				}

				// Get procedural objects
				m_SectorData.nProcObjects = g_procObjMan.GetNumActiveObjects();

				// Get cars
				m_SectorData.nCars = 0;
				m_SectorData.nDummyCars = 0;
				CVehicle::Pool* pVehPool = CVehicle::GetPool( );
				for ( int nVehicle = 0; nVehicle < pVehPool->GetSize(); ++nVehicle )
				{
					CVehicle* pVehicle = pVehPool->GetSlot( nVehicle );
					if ( pVehicle && pVehicle->IsDummy() )
						++m_SectorData.nDummyCars;
					else if (pVehicle && !pVehicle->IsDummy() )
						++m_SectorData.nCars;
				}

				// Get buildings and trees
				m_SectorData.nBuildingObjects = 0;
				m_SectorData.nTreeObjects = 0;
				CBuilding::Pool* pBuildingPool = CBuilding::GetPool( );
				if ( pBuildingPool )
				{
					int nTreeObjs = 0;
					for ( int nObj = 0; nObj < pBuildingPool->GetSize(); ++nObj )
					{
						CBuilding* pBuilding = pBuildingPool->GetSlot( nObj );
						if ( !pBuilding )
							continue;

						strLocalIndex nIndex = strLocalIndex(pBuilding->GetModelIndex( ));
						if ( nIndex == fwModelId::MI_INVALID )
							continue;

						CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(nIndex));
						if ( !pModelInfo )
							continue;

						if ( pModelInfo->GetIsTree() )
						{
							// Only counting trees in current sector
							const Vector3 pos = VEC3V_TO_VECTOR3(pBuilding->GetTransform().GetPosition());
							if ( CSectorTools::VecWithinCurrentSector( pos, m_nCurrentSectorX, m_nCurrentSectorY ) )
								++nTreeObjs;
						}
					}
					m_SectorData.nBuildingObjects = pBuildingPool->GetNoOfUsedSpaces( );
					m_SectorData.nTreeObjects = nTreeObjs;
				}

				// Get procedural grass objects
				u32 nTris = 0;
				u32 nGrass = 0;
				gPlantMgr.DbgGetCPlantMgrInfo( &nTris, &nGrass );
				m_SectorData.nProcGrassObjects = nGrass;

				// Get scripts
				for ( int nScript = 0; nScript < GtaThread::GetThreadCount(); ++nScript )
				{
					GtaThread& thread = static_cast<GtaThread&>( GtaThread::GetThreadByIndex(nScript) );

					if ( rage::scrThread::RUNNING == thread.GetState() )
//						&& ( thread.m_ScriptName.GetLength() ) )
					{
						m_SectorData.vScripts.PushAndGrow( atString(thread.GetScriptName()) );
					}
				}
			}
		}
		else
		{
			// Determine if a sample interval has passed so we record a sample.
			// Otherwise we don't do anything this game loop.
			if ( 0 == GetIntervalTimer() )
			{
				// Initialise timer
				m_msIntervalTimer = fwTimer::GetSystemTimeInMilliseconds( );
			}
			else if ( ( fwTimer::GetSystemTimeInMilliseconds() - GetIntervalTimer() ) > m_msSampleInterval )
			{
				// Take RealFPS Sample
				float fRealFPS = ( 1.0f / rage::Max( 0.001f, fwTimer::GetSystemTimeStep( ) ) );

				if ( kGamePaused == GetPass( ) )
				{
					ReadTimeBarSamples( m_PausedSamples.vTimeBarSamples[m_eDirection] );
					ReadGrcSamples( m_PausedSamples.vGrcSamples[m_eDirection] );
					m_PausedSamples.vRealFpsSamples[m_eDirection].PushAndGrow( fRealFPS );
				}
				else if ( kGameRunning == GetPass( ) )
				{
					ReadTimeBarSamples( m_RunningSamples.vTimeBarSamples[m_eDirection] );
					ReadGrcSamples( m_RunningSamples.vGrcSamples[m_eDirection] );
					m_RunningSamples.vRealFpsSamples[m_eDirection].PushAndGrow( fRealFPS );
				}
				else
				{
					Assertf( false, "Invalid state.  No appropriate samples for pass." );
				}

				// Initialise timer for next interval.
				m_msIntervalTimer = fwTimer::GetSystemTimeInMilliseconds( );
			}
		}
	}

	UpdateState( );
}

void
CPerformanceStats::UpdateState( )
{
	if ( ( kExportArea == m_eMode ) || ( kExportSpecific == m_eMode ) )
	{
		if ( kNavmeshLoad == m_ePass )
		{
			// Initialise timer
			if ( 0 == m_msTimer )
				m_msTimer = fwTimer::GetTimeInMilliseconds( );

			snprintf(m_startTime,SECTOR_TOOLS_MAX_START_TIME_LEN,"%d",m_msTimer);

			// Only change to kObjectLoad if we have exceeded our navmesh load period.
			if ( (fwTimer::GetTimeInMilliseconds() - m_msTimer) > CSectorTools::GetNavmeshLoadDelay() )
			{
				m_ePass = kObjectLoad;
				m_msTimer = 0L;
			}
		}
		else if ( kObjectLoad == m_ePass )
		{
			// First check to see if this sector doesn't have a navmesh
			// node within.  If not then we skip it by staying in the
			// same state and moving onto the next sector - unless we are
			// at the end.
			if ( !m_bSectorOK )
			{
				// Stop if we are at the end.
				if ( ( m_nEndSectorX == m_nCurrentSectorX ) &&
					 ( m_nEndSectorY == m_nCurrentSectorY ) )
				{
					m_ePass = kNavmeshLoad;
					m_eMode = kDisabled;
				}
				// Otherwise move along
				else 
				{
					m_msTimer = 0L;
					m_bSectorOK = false;
					m_ePass = kNavmeshLoad;
					MoveToNextSector( );
				}
			}
			else
			{
				// Initialise timer
				if ( 0 == GetTimer() )
				{
					m_msTimer = fwTimer::GetSystemTimeInMilliseconds( );
				}
				// Only change to North if we have exceeded our load period
				if ( (fwTimer::GetSystemTimeInMilliseconds() - GetTimer()) > m_msSampleDelay )
				{
					m_ePass = kGameRunning;
					m_eDirection = kNorth;
					m_msTimer = 0L;
				}

			}
		}
		else if ( ( kGamePaused == m_ePass ) || ( kGameRunning == m_ePass ) )
		{
			if ( 0 == GetTimer( ) )
			{
				m_msTimer = fwTimer::GetSystemTimeInMilliseconds( );
			}
			else if ( (fwTimer::GetSystemTimeInMilliseconds() - GetTimer()) > m_msSampleDelay )
			{
				// Finished the sample period for this pass and view direction.  
				// Lets take a screenshot if we are running and move on.
				if ( kGameRunning == GetPass( ) )
					TakeScreenshot( m_eDirection );

				// If we are already looking West and in the Game Paused pass
				// then we need to move onto the next sector.
				if ( ( kWest == m_eDirection ) && ( kGamePaused == m_ePass ) )
				{
					WriteLog( );

					// Reset for next sector or end
					m_ePass = kObjectLoad;
					m_eDirection = kNorth;
					Pause( false );
					
					if ( ( m_nEndSectorX == m_nCurrentSectorX ) &&
						 ( m_nEndSectorY == m_nCurrentSectorY ) )
					{
						m_eMode = kDisabled;
					}
					else 
					{
						MoveToNextSector( );
					}
				}
				// Otherwise we go to the next direction, possibly updating our
				// pass to Game Paused if we are looking West.
				else
				{
					switch ( m_eDirection )
					{
					case kNorth:
						m_eDirection = kEast;
						break;
					case kEast:
						m_eDirection = kSouth;
						break;
					case kSouth:
						m_eDirection = kWest;
						break;
					case kWest:
						m_eDirection = kNorth;
						m_ePass = kGamePaused;
						Pause( true );
						break;
					default:
						Assertf( false, "Invalid state." );
					}
				}
				m_msTimer = 0L;
			}
		}
	}
	else
	{
		Assertf( false, "Invalid mode.  Contact tools." );
	}

	// Check if we have disabled ourselves to ensure we unpause the game.
	if ( kDisabled == m_eMode )
	{
		if ( fwTimer::IsGamePaused() )
		{
			Pause( false );
		}

		// Reset weather
		g_weather.ForceTypeClear();

		if ( m_pProgressFile && PARAM_runperf.Get() )
		{
			fprintf( m_pProgressFile, "%d %d\n", m_nCurrentSectorX, m_nCurrentSectorY );
			fprintf( m_pProgressFile, "END\n" );
			m_pProgressFile->Flush( );
			m_pProgressFile->Close( );
			m_pProgressFile = NULL;
		}
	}
}

void
CPerformanceStats::Update( )
{
#if DEBUG_DRAW
	if ( !m_bEnabled )
		return;

	if ( kDisabled != m_eMode )
	{
		grcDebugDraw::AddDebugOutput( Color32(1.0f, 1.0f, 0.0f), 
			"Performance Statistics Capture Running [ %d, %d ]. Pass: %s.  Direction: %s.  Stream delay: %d ms. Sample period: %d ms.  Sample interval: %d ms.", 
			m_nCurrentSectorX, m_nCurrentSectorY, m_vPassNames[m_ePass], m_vFacingNames[m_eDirection], 
			CSectorTools::GetStreamDelay(), m_msSampleDelay, m_msSampleInterval );
		grcDebugDraw::AddDebugOutput( Color32(1.0f, 1.0f, 0.0f),
			"Do not move the player or viewpoint as you will interfere with the Performance Capture." );
	}
#endif
}

void
CPerformanceStats::ScanAndExportSpecificSector( )
{
	SetOutputFolder( );
	m_RunningSamples.Reset( );
	m_PausedSamples.Reset( );

	g_weather.ForceTypeNow( 0, CWeather::FT_None );
	SetEnabled( true );
	SetMode( kExportSpecific );
}

void
CPerformanceStats::ScanAndExportArea( )
{
	SetOutputFolder( );
	m_RunningSamples.Reset( );
	m_PausedSamples.Reset( );

	// If we have started automatically then create a progress file
	if ( PARAM_runperf.Get() )
	{
		char sIPAddress[MAX_OUTPUT_PATH];
#if __XENON
		XNADDR xnaddr;
		DWORD dw = XNetGetDebugXnAddr(&xnaddr);

		if ( dw & (XNET_GET_XNADDR_DHCP | XNET_GET_XNADDR_PPPOE | XNET_GET_XNADDR_STATIC) )
		{
			snprintf( sIPAddress, MAX_OUTPUT_PATH, "%d.%d.%d.%d",
				xnaddr.ina.S_un.S_un_b.s_b1,
				xnaddr.ina.S_un.S_un_b.s_b2,
				xnaddr.ina.S_un.S_un_b.s_b3,
				xnaddr.ina.S_un.S_un_b.s_b4 );
		}
#elif __PPU
		const char* sIp = CSectorTools::GetIp();
		snprintf( sIPAddress, MAX_OUTPUT_PATH, "%s", sIp );
#endif
		char sProgressFilename[MAX_OUTPUT_PATH];
		formatf( sProgressFilename, MAX_OUTPUT_PATH, "%s\\%s.txt", m_sOutputDirectory, sIPAddress );
		m_pProgressFile = fiStream::Create( sProgressFilename );
		fprintf( m_pProgressFile, "START\n" );
		m_pProgressFile->Flush( );
	}

	SetEnabled( true );
	SetMode( kExportArea );

	if ( PARAM_runperf.Get() )
		SetupWorldCoordinates( );
}

#if __BANK
void
CPerformanceStats::InitWidgets( )
{
	snprintf( m_sSampleDelay, 20, "%d", m_msSampleDelay );
	snprintf( m_sSampleInterval, 20,  "%d", m_msSampleInterval );

	bkBank* pWidgetBank = BANKMGR.FindBank( "Sector Tools" );
	if (AssertVerify(pWidgetBank))
	{
		pWidgetBank->PushGroup( "Game Performance/Screenshot Logger" );
			pWidgetBank->AddText( "Sample Delay (ms):", m_sSampleDelay, MAX_SECTOR_COORDS, false, datCallback(CFA(SampleDelay)) );
			pWidgetBank->AddText( "Sample Interval (ms):", m_sSampleInterval, MAX_SECTOR_COORDS, false, datCallback(CFA(SampleInterval)) );
			pWidgetBank->AddButton( "Scan and Export Selected Sector", datCallback(CFA(ScanAndExportSpecificSector)) );
			pWidgetBank->AddButton( "Scan and Export Area", datCallback(CFA(ScanAndExportArea)) );
		pWidgetBank->PopGroup( );
	}
}
#endif // __BANK

void
CPerformanceStats::SetMode( etMode eMode )
{
	m_eMode = eMode;

	if ( kExportArea == m_eMode )
	{
		// Fetch new start/end world coordinates
		const Vector3& vStart = CSectorTools::GetWorldStart( );
		const Vector3& vEnd = CSectorTools::GetWorldEnd( );

		int sX = SECTORTOOLS_WORLD_WORLDTOSECTORX( vStart.x );
		int eX = SECTORTOOLS_WORLD_WORLDTOSECTORX( vEnd.x );
		int sY = SECTORTOOLS_WORLD_WORLDTOSECTORY( vStart.y );
		int eY = SECTORTOOLS_WORLD_WORLDTOSECTORY( vEnd.y );

		m_nStartSectorX = MIN( sX, eX );
		m_nStartSectorY = MIN( sY, eY );
		m_nEndSectorX = MAX( sX, eX );
		m_nEndSectorY = MAX( sY, eY );

		m_nCurrentSectorX = m_nStartSectorX;
		m_nCurrentSectorY = m_nStartSectorY;
	}
	else if ( kExportSpecific == m_eMode )
	{
		m_nCurrentSectorX = (int)CSectorTools::GetSector( ).x;
		m_nCurrentSectorY = (int)CSectorTools::GetSector( ).y;
		m_nEndSectorX = m_nCurrentSectorX;
		m_nEndSectorY = m_nCurrentSectorY;
	}
	else if ( kDisabled == m_eMode )
	{
		// Nothing additional required when disabled.
	}
	else
	{
		Assertf( false, "Unrecognised Performance Stats mode.  Contact Tools." );
	}
}

void
CPerformanceStats::SetPlayerDirection( etDirection eDir )
{
	CPed* pPlayer = FindPlayerPed( );
	if ( !pPlayer || pPlayer->GetUsingRagdoll( ) ) //NOTE: We can't usefully orient the ped when ragdolling.
		return;
	
	float fAngle = -( 2.0f * ( static_cast<float>( eDir ) ) * PI ) / 4.0f;
	fAngle = fwAngle::LimitRadianAngleFast(fAngle);

	pPlayer->SetDesiredHeading( fAngle );
	pPlayer->SetHeading( fAngle );
}

void
CPerformanceStats::SetCameraDirection( etDirection eDir )
{
	CPed* pPlayer = FindPlayerPed( );
	if ( !pPlayer )
		return;

	float fPedAngle = pPlayer->GetTransform().GetHeading( );

	float fDesiredAngle = -( 2.0f * ( static_cast<float>( eDir ) ) * PI ) / 4.0f;
	fDesiredAngle = fwAngle::LimitRadianAngleFast(fDesiredAngle);

	float fRelativeAngle = fDesiredAngle - fPedAngle;
	fRelativeAngle = fwAngle::LimitRadianAngleFast(fRelativeAngle);

	camInterface::GetGameplayDirector().SetRelativeCameraHeading(fRelativeAngle);
	camInterface::GetGameplayDirector().SetRelativeCameraPitch(0.0f);
}

void 
CPerformanceStats::SetOutputFolder( )
{
	char sOutputPath[MAX_OUTPUT_PATH];
	char sImagePath[MAX_OUTPUT_PATH];
	char sPlatform[MAX_OUTPUT_PATH];
	char sGameTime[MAX_OUTPUT_PATH];
	char sDate[MAX_OUTPUT_PATH];

	// Get platform string, and platform-specific time string
	
#if __PPU
	
	CellRtcDateTime clk;
	cellRtcGetCurrentClockLocalTime(&clk);
	formatf( sDate, MAX_OUTPUT_PATH, "%4d%02d%02d", 
		(clk.year), (clk.month), clk.day );
	formatf( sPlatform, MAX_OUTPUT_PATH, "%s", "ps3" );
#elif __XENON
	SYSTEMTIME time;
	GetLocalTime( &time );
	formatf( sDate, MAX_OUTPUT_PATH, "%4d%02d%02d", time.wYear, time.wMonth, time.wDay );
	formatf( sPlatform, MAX_OUTPUT_PATH, "%s", "xenon" );
#endif

	// DW - lets just use one date - because if the stats where captured over several days it confuses the hell out of me.
	// Can't be certain if the monitor file was created on one date and finalised in other what might happen.
	const char* sDateParam = NULL;
	XPARAM(usedate);
	PARAM_usedate.Get( sDateParam );
	if (sDateParam && strlen(sDateParam)>0)
	{		
		strcpy(sDate,sDateParam);
	}

	if ( CSectorTools::GetMidnight( ) )
		snprintf( sGameTime, MAX_OUTPUT_PATH, "midnight" );
	else
		snprintf( sGameTime, MAX_OUTPUT_PATH, "noon" );

	const char* pLevelName = NULL;
	s32 iRequestedLevelIndex = -1;
	PARAM_level.Get( iRequestedLevelIndex );
	if ( iRequestedLevelIndex <= 0 || iRequestedLevelIndex > CScene::GetLevelsData().GetCount() )
	{
		PARAM_level.Get( pLevelName );
	}
	Assertf( pLevelName, "Running Performance Stats but have not specified a level name." );

	// Create directory in components if it doesn't already exist
	const fiDevice* pDevice = fiDevice::GetDevice( sOutputPath, false );
	Assertf( pDevice, "Failed to get device for network path.  Contact tools." );

	formatf( sOutputPath, MAX_OUTPUT_PATH, "%s\\%s", STATSROOT_NETWORK, pLevelName );
	pDevice->MakeDirectory( sOutputPath );

	formatf( sOutputPath, MAX_OUTPUT_PATH, "%s\\%s\\%s", STATSROOT_NETWORK, 
		pLevelName, sPlatform );
	pDevice->MakeDirectory( sOutputPath );

	formatf( sOutputPath, MAX_OUTPUT_PATH, "%s\\%s\\%s\\%s", STATSROOT_NETWORK, 
		pLevelName, sPlatform, sGameTime );
	pDevice->MakeDirectory( sOutputPath );

	formatf( sOutputPath, MAX_OUTPUT_PATH, "%s\\%s\\%s\\%s\\%s", STATSROOT_NETWORK, 
		pLevelName, sPlatform, sGameTime, sDate );
	formatf( sImagePath, MAX_OUTPUT_PATH, "%s\\%s\\%s\\%s", STATSROOT_NETWORK,
		pLevelName, sPlatform, sGameTime, "screenshots" );
	pDevice->MakeDirectory( sOutputPath );
	pDevice->MakeDirectory( sImagePath );

	strncpy( m_sOutputDirectory, sOutputPath, MAX_OUTPUT_PATH );
	strncpy( m_sScreenshotDirectory, sImagePath, MAX_OUTPUT_PATH );
}

void 
CPerformanceStats::MoveToNextSector( )
{
	// DW - To fix a bug we have to make sure we are in this navmesh mode - else we get problems with the static vOut... 
	m_ePass  = kNavmeshLoad;

	// Clear sector samples as we are moving on.
	m_RunningSamples.Reset( );
	m_PausedSamples.Reset( );
	m_bSectorOK = true;

	if ( kExportSpecific == m_eMode )
	{
		Assertf( false, "Cannot move to next sector in Export Specific sector mode." );
	}
	else if ( kExportArea == m_eMode )
	{	
		// Log progress if we have started automatically to log our just finished sector.
		if ( m_pProgressFile && PARAM_runperf.Get() )
		{
			fprintf( m_pProgressFile, "%d %d\n", m_nCurrentSectorX, m_nCurrentSectorY );
			m_pProgressFile->Flush( );
		}

		// If at end of a grid row, return to start sector x if in group
		// export mode, otherwise return to start of next row (x=0)
		if ( m_nCurrentSectorX == m_nEndSectorX )
		{
			m_nCurrentSectorX = m_nStartSectorX;
			++m_nCurrentSectorY;
		}
		// Otherwise, move along row
		else
		{
			++m_nCurrentSectorX;
		}
	}
	else 
	{
		Assertf( false, "Invalid Mode.  Internal Error." );
	}
}

void
CPerformanceStats::TakeScreenshot( etDirection eDir )
{
	char sImageFilename[MAX_OUTPUT_PATH];

	// Prepare filename based on current sector and view direction.
	switch ( eDir )
	{
	case kNorth:
		formatf( sImageFilename, MAX_OUTPUT_PATH, "%s\\sector_%d_%d_%c", 
				m_sScreenshotDirectory, m_nCurrentSectorX, m_nCurrentSectorY, 'N' );
		break;
	case kEast:
		formatf( sImageFilename, MAX_OUTPUT_PATH, "%s\\sector_%d_%d_%c", 
				m_sScreenshotDirectory, m_nCurrentSectorX, m_nCurrentSectorY, 'E' );
		break;
	case kSouth:
		formatf( sImageFilename, MAX_OUTPUT_PATH, "%s\\sector_%d_%d_%c", 
				m_sScreenshotDirectory, m_nCurrentSectorX, m_nCurrentSectorY, 'S' );
		break;
	case kWest:
		formatf( sImageFilename, MAX_OUTPUT_PATH, "%s\\sector_%d_%d_%c", 
				m_sScreenshotDirectory, m_nCurrentSectorX, m_nCurrentSectorY, 'W' );
		break;
	default:
		Assertf( false, "Invalid pass to be taking a screenshot.  Internal error." );
		return;
	}

	CSystem::GetRageSetup()->SetScreenShotNamingConvention( grcSetup::OVERWRITE_SCREENSHOT );
	CSystem::GetRageSetup()->TakeScreenShot( sImageFilename, true );
}

void
CPerformanceStats::Pause( bool bPause )
{
	if ( bPause )
	{
		fwTimer::StartUserPause( );
	}
	else
	{
		fwTimer::EndUserPause( );
	}
}

void 
CPerformanceStats::WriteLog()
{
	int nX = m_nCurrentSectorX;
	int nY = m_nCurrentSectorY;
	float fX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( nX );
	float fY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( nY );
	Vector3 vSectorCentre( fX, fY, 0.0f );

	CPed* pPlayer = FindPlayerPed( );
	Assert( pPlayer );
	Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	
	char sLogFilename[MAX_OUTPUT_PATH];

	formatf( sLogFilename, MAX_OUTPUT_PATH, "%s\\sector_%d_%d.xml",
			m_sOutputDirectory, m_nCurrentSectorX, m_nCurrentSectorY );

	fiStream* fpLogFile = fiStream::Create( sLogFilename );	

	CSectorTools::WriteCommon(fpLogFile, m_sectorNum, m_startTime, m_description, m_result, nX, nY , vSectorCentre, vPlayerPos);
	m_sectorNum++;

	// Write Sector Data
	fprintf( fpLogFile, "\t<cars count=\"%d\" dummy=\"%d\" />\n", m_SectorData.nCars, m_SectorData.nDummyCars );
	fprintf( fpLogFile, "\t<objects count=\"%d\" dummy=\"%d\" active=\"%d\" proc=\"%d\" buildings=\"%d\" trees=\"%d\" grass=\"%d\" />\n", 
			m_SectorData.nPhysicalObjects, m_SectorData.nDummyObjects, 
			m_SectorData.nActiveObjects, m_SectorData.nProcObjects,
			m_SectorData.nBuildingObjects, m_SectorData.nTreeObjects,
			m_SectorData.nProcGrassObjects );

	// Write Scripts Sector Data
	fprintf( fpLogFile, "\t<scripts count=\"%d\">\n", m_SectorData.vScripts.GetCount() );
	for ( int nScript = 0; nScript < m_SectorData.vScripts.GetCount(); ++nScript )
		fprintf( fpLogFile, "\t\t<script name=\"%s\" />\n", m_SectorData.vScripts[nScript].c_str() );
	fprintf( fpLogFile, "\t</scripts>\n" );

	// Write FPS samples
	WriteSamples( fpLogFile, "pausedSamples", m_PausedSamples );
	WriteSamples( fpLogFile, "runningSamples", m_RunningSamples );

	fprintf( fpLogFile, "</sector>\n" );
	fpLogFile->Flush( );
	fpLogFile->Close( );
}

void
CPerformanceStats::WriteSamples( fiStream* fpStream, const char* sTag, const TSectorSamples& samples )
{
	fprintf( fpStream, "\t<%s>\n", sTag );
	
	for ( int nDir = 0; nDir < NUM_DIRECTIONS; ++nDir )
	{
		fprintf( fpStream, "\t\t<direction name=\"%s\" count=\"%d\">\n",
				m_vFacingNames[nDir], samples.vRealFpsSamples[nDir].GetCount() );
		for ( int nSample = 0; nSample < samples.vRealFpsSamples[nDir].GetCount(); ++nSample )
		{
			fprintf( fpStream, "\t\t\t<sample realfps=\"%.2f\" ", samples.vRealFpsSamples[nDir][nSample] );
			
			// Loop through all of the TimeBars
			for ( int nTimeBar = 0; nTimeBar < NUM_TIMEBAR_STATS; ++nTimeBar )
			{
				fprintf( fpStream, "%s=\"%.2f\" ", 
						 gsTimeBarPerfStats[nTimeBar][1], 
						 samples.vTimeBarSamples[nDir][nTimeBar][nSample] );
			}

			// Loop through all the Rage GRC Setup Samples
			fprintf( fpStream, "rage_update=\"%.2f\" ", samples.vGrcSamples[nDir][0][nSample] );
			fprintf( fpStream, "rage_draw=\"%.2f\" ", samples.vGrcSamples[nDir][1][nSample] );
			fprintf( fpStream, "rage_total=\"%.2f\" ", samples.vGrcSamples[nDir][2][nSample] );
			fprintf( fpStream, "rage_gpu=\"%.2f\" ", samples.vGrcSamples[nDir][3][nSample] );
			
			fprintf( fpStream, " />\n" );
		}
		fprintf( fpStream, "\t\t</direction>\n" );
	}

	fprintf( fpStream, "\t</%s>\n", sTag );
}

void
CPerformanceStats::SampleDelay( )
{
	sscanf( m_sSampleDelay, "%d", &m_msSampleDelay );
}

void
CPerformanceStats::SampleInterval( )
{
	sscanf( m_sSampleInterval, "%d", &m_msSampleInterval );
}

bool
CPerformanceStats::VecWithinCurrentSector( const rage::Vector3& pos )
{
	float fX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( m_nCurrentSectorX );
	float fY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( m_nCurrentSectorY );
	float fLeft = fX - ( SECTORTOOLS_WORLD_WIDTHOFSECTOR / 2.0f );
	float fRight = fX + ( SECTORTOOLS_WORLD_WIDTHOFSECTOR / 2.0f );
	float fTop = fY + ( SECTORTOOLS_WORLD_DEPTHOFSECTOR / 2.0f );
	float fBottom = fY - ( SECTORTOOLS_WORLD_DEPTHOFSECTOR / 2.0f );

	return ( ( fLeft <= pos.x ) && ( fRight >= pos.x ) &&
		( fBottom <= pos.y ) && ( fTop >= pos.y ) );
}

void
CPerformanceStats::ReadTimeBarSamples( rage::atRangeArray<rage::atArray<float>, NUM_TIMEBAR_STATS>& samples )
{
	float fTimeBarSample = 0.0f;

	for ( int nTimeBar = 0; nTimeBar < NUM_TIMEBAR_STATS; ++nTimeBar )
	{
#if RAGE_TIMEBARS
		int nCallCount = 0;
		g_pfTB.GetFunctionTotals( gsTimeBarPerfStats[nTimeBar][0], nCallCount, fTimeBarSample );
#endif // RAGE_TIMEBARS
		samples[nTimeBar].PushAndGrow( fTimeBarSample );
	}
}

void
CPerformanceStats::ReadGrcSamples( rage::atRangeArray<rage::atArray<float>, NUM_GRC_STATS>& samples )
{
	rage::grmSetup* pSetup = CSystem::GetRageSetup();

	samples[0].PushAndGrow( pSetup->GetUpdateTime() );
	samples[1].PushAndGrow( pSetup->GetDrawTime() );
	samples[2].PushAndGrow( pSetup->GetTotalTime() );
	samples[3].PushAndGrow( pSetup->GetGpuTime() );
}

#endif // SECTOR_TOOLS_EXPORT
