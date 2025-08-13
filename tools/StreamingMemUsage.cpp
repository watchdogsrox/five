//
// filename:	StreamingMemUsage.cpp
// author:		David Muir <david.muir@rockstarnorth.com>
// date:		17 July 2007
// description:	Streaming memory usage statistics generation.
//

// --- Include Files ------------------------------------------------------------

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "atl/array.h"
#include "file/stream.h"
#include "grcore/debugdraw.h"
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "StreamingMemUsage.h"
#include "game/Clock.h"
#include "Objects/DummyObject.h"
#include "Objects/object.h"
#include "pathserver/PathServer.h"
#include "Peds/Ped.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/water.h"
#include "scene/AnimatedBuilding.h"
#include "scene/Building.h"
#include "scene/scene.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/PortalInst.h"
#include "scene/world/gameWorld.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"

// Platform SDK headers
#if __XENON
#include "system/xtl.h"
#elif __PPU
//#include <time.h>
#include <cell/rtc.h>
//#include <netex/libnetctl.h>

#endif // Platform SDK headers

#if SECTOR_TOOLS_EXPORT

// --- Defines ------------------------------------------------------------------

#undef STATSROOT_NETWORK // ubfix
#define STATSROOT_NETWORK		( "K:\\streamGTA5\\stats\\memstats" )

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

bool							CStreamingMemUsage::m_bEnabled = false;
CStreamingMemUsage::etMode		CStreamingMemUsage::m_eMode = CStreamingMemUsage::kDisabled;
CStreamingMemUsage::etPass		CStreamingMemUsage::m_ePass = CStreamingMemUsage::kNavmeshLoad;
char							CStreamingMemUsage::m_sOutputDirectory[MAX_OUTPUT_PATH];
u32							CStreamingMemUsage::m_msTimer = 0L;
rage::fiStream*					CStreamingMemUsage::m_pProgressFile = NULL;
bool							CStreamingMemUsage::m_bSectorOK = true;
int								CStreamingMemUsage::m_nCurrentSectorX = 0;
int								CStreamingMemUsage::m_nCurrentSectorY = 0;
int								CStreamingMemUsage::m_nStartSectorX = 0;
int								CStreamingMemUsage::m_nStartSectorY = 0;
int								CStreamingMemUsage::m_nEndSectorX = 0;
int								CStreamingMemUsage::m_nEndSectorY = 0;

float							CStreamingMemUsage::m_startScanY = 700; 
float							CStreamingMemUsage::m_endScanY	= -500; 
float							CStreamingMemUsage::m_scanYInc	= -100;
float							CStreamingMemUsage::m_probeYEnd	= -500; // How far down to cast

char							CStreamingMemUsage::m_description[SECTOR_TOOLS_MAX_DESC_LEN];
int								CStreamingMemUsage::m_sectorNum = 0;
char							CStreamingMemUsage::m_result[SECTOR_TOOLS_MAX_RESULT_LEN];
char							CStreamingMemUsage::m_startTime[SECTOR_TOOLS_MAX_START_TIME_LEN];

// --- Code ---------------------------------------------------------------------

void 
CStreamingMemUsage::Init( )
{
	m_bEnabled = false;
	m_nCurrentSectorX = SECTORTOOLS_WORLD_WORLDTOSECTORX( 0.0f );
	m_nCurrentSectorY = SECTORTOOLS_WORLD_WORLDTOSECTORY( 0.0f );
	m_msTimer = 0L;
	m_nStartSectorX = 60;
	m_nStartSectorY = 60;
	m_nEndSectorX = 0;
	m_nEndSectorY = 0;
	m_sectorNum = 0;
}

void 
CStreamingMemUsage::Shutdown( )
{
	m_bEnabled = false;
}

void
CStreamingMemUsage::Run( )
{
	if ( !m_bEnabled )
		return;
	if ( kDisabled == m_eMode )
		return;

	TStoreMemoryUsage sPhysicsUsage;
	TStoreMemoryUsage sStaticBoundsUsage;
	
	TPoolMemoryUsage sPhysicalUsage;
	TPoolMemoryUsage sPortalUsage;
	TPoolMemoryUsage sInteriorUsage;
	TPoolMemoryUsage sDummyUsage;
	TPoolMemoryUsage sBuildingUsage;
	TPoolMemoryUsage sAnimatedBuildUsage;

	bool bWriteLog = false;

	CPed* pPlayer = FindPlayerPed( );
	Assertf( pPlayer, "Unable to find player ped." );
	float fX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( m_nCurrentSectorX );
	float fY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( m_nCurrentSectorY );
	Vector3 vSectorCentre( fX, fY, 500.0f );
	static Vector3 vOut;

	if ( ( kNavmeshLoad  == m_ePass ) && ( 0 == m_msTimer ) )
	{
		// Each time we move to a new sector set game clock dependent 
		// on user-defined var
		if ( CSectorTools::GetMidnight() || (PARAM_runmem.Get() && PARAM_midnight.Get()) )
		{
			if ( CLOCK_MODE_DYNAMIC == CClock::GetMode() )
				CClock::SetTime(0, 0, 0);
			else if ( CLOCK_MODE_FIXED == CClock::GetMode() )
				CClock::SetTimeSample(0);
		}
		else
		{
			if ( CLOCK_MODE_DYNAMIC == CClock::GetMode() )
				CClock::SetTime(12, 0, 0);
			else if ( CLOCK_MODE_FIXED == CClock::GetMode() )
				CClock::SetTimeSample(2);
		}

		strcpy(m_description,"");
		strcpy(m_result,"ERROR: COLLISION/STREAMING");

		Vector3 vGuessGround = vSectorCentre;

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
				Displayf("*** MEMSTAT_TRACE 0 *** WorldProbe::FindGroundZForCoord ok %f %f %f hit %f %f %f", probeDesc.GetStart().x,probeDesc.GetStart().y,probeDesc.GetStart().z,vOut.x,vOut.y,vOut.z);

				CEntity *pEntity = CPhysics::GetEntityFromInst(probeResult[0].GetHitInst());
				if (pEntity)
				{
					if (CModelInfo::IsValidModelInfo(pEntity->GetModelIndex()))
					{
						snprintf(m_description, SECTOR_TOOLS_MAX_DESC_LEN, "%s ModelIndex %d ModelDesc %s",m_description, pEntity->GetModelIndex(), CModelInfo::GetBaseModelInfoName(pEntity->GetModelId()) );			
					}
					else
					{
						snprintf(m_description, SECTOR_TOOLS_MAX_DESC_LEN,"%s model index %d ", m_description, pEntity->GetModelIndex());
					}
				}
				else
				{
					snprintf(m_description, SECTOR_TOOLS_MAX_DESC_LEN,"%s : No Model",m_description);
				}

				pPlayer->Teleport( vOut, pPlayer->GetCurrentHeading() );
				break;
			}
			else
			{
				Displayf("*** MEMSTAT_TRACE 1 *** WorldProbe::FindGroundZForCoord failed %f %f %f", probeDesc.GetStart().x,probeDesc.GetStart().y,probeDesc.GetStart().z);

				snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s WorldProbe::FindGroundZForCoord failed %f %f %f",m_description, probeDesc.GetStart().x,probeDesc.GetStart().y,probeDesc.GetStart().z);

				// check for water
				Vector3 waterLevel;
				Vector3 endPos = vGuessGround;
				endPos.z = m_probeYEnd;
				static bool foundWater = false;

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
	else if ( ( kObjectLoad == m_ePass ) && ( 0 == m_msTimer ) )
	{
		// We have waited a short amount of time since the navmesh load
		// state so we can test if we have a valid location in this sector.
		// If we have a valid position then continue, otherwise mark sector 
		// as invalid and move onto the next.
		EGetClosestPosRet ret = CPathServer::GetClosestPositionForPed( vOut, vOut, 25.0f );
		if ( ENoPositionFound != ret ) 
		{
			m_bSectorOK = true;
			strcpy(m_result,"ERROR: NAVMESH ISSUE");
			snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s CPathServer::GetClosestPositionForPed succeeded at %f %f %f", m_description, vOut.x, vOut.y, vOut.z);
		}
		else
		{
			snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s CPathServer::GetClosestPositionForPed failed at %f %f %f", m_description, vOut.x, vOut.y, vOut.z);
		}

		if ( m_bSectorOK )
		{
			char sLogFilename[MAX_OUTPUT_PATH];
			formatf( sLogFilename, MAX_OUTPUT_PATH, "%s\\strmmemuse_sector_%d_%d.mem",
				m_sOutputDirectory, m_nCurrentSectorX, m_nCurrentSectorY );

			fiStream* fpLogFile = fiStream::Open( sLogFilename );
			if ( fpLogFile )
			{
				// Don't scan sectors we have already done for this date.
				fpLogFile->Close( );
				m_bSectorOK = false;
			}
			// Teleport the player if the node position is within it, otherwise we
			// set a flag to indicate to the UpdateState function to skip this
			// sector and move along.
			else if ( CSectorTools::VecWithinCurrentSector( vOut, m_nCurrentSectorX, m_nCurrentSectorY ) )
			{
				m_bSectorOK = true;
				pPlayer->Teleport( vOut, pPlayer->GetCurrentHeading() );

				INSTANCE_STORE.GetBoxStreamer().EnsureLoadedAboutPos(VECTOR3_TO_VEC3V(vOut));
				g_StaticBoundsStore.GetBoxStreamer().EnsureLoadedAboutPos(VECTOR3_TO_VEC3V(vOut));
				bWriteLog = true;
				strcpy(m_result,"OK");
			}
			else
			{
				snprintf(m_description,SECTOR_TOOLS_MAX_DESC_LEN,"%s CSectorTools::VecWithinCurrentSector failed %f %f %f",m_description, vOut.x, vOut.y, vOut.z);
				strcpy(m_result,"ERROR: NOT IN SECTOR");
				m_bSectorOK = false;
				bWriteLog = true;
			}
		}
		else
		{
			bWriteLog = true;
		}
	}
	else if ( ( kStatsCapture == m_ePass ) && ( 0 == m_msTimer ) )
	{
		// Load all objects
		CStreaming::LoadAllRequestedObjects( );

		// Collect stats
		atArray<TPoolMemoryUsage> poolMemoryUsage;
		poolMemoryUsage.Reset( );

		// Collect model info ids
		atArray<u32> modelInfosIds;
		atArray<u32> modelInfosInteriorIds;

		// Fetch Model Info Indices for Animated Buildings
		
		fwPool<CAnimatedBuilding>* pAnimatedBuildPool = CAnimatedBuilding::GetPool( );
		modelInfosIds.Reset( );
		modelInfosInteriorIds.Reset( );
		sAnimatedBuildUsage.nNumTotalObjects = pAnimatedBuildPool->GetNoOfUsedSpaces( );
		GetSectorModelInfoIdsForPool( pAnimatedBuildPool, modelInfosIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorInteriorModelInfoIdsForPool( pAnimatedBuildPool, modelInfosInteriorIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorMemoryUsageForModelInfoIds( modelInfosIds, modelInfosInteriorIds, sAnimatedBuildUsage );
		GetSectorObjectsForPool( pAnimatedBuildPool, sAnimatedBuildUsage.vObjects, m_nCurrentSectorX, m_nCurrentSectorY );

		// Fetch Model Info Indices for Buildings
		fwPool<CBuilding>* pBuildingPool = CBuilding::GetPool( );
		modelInfosIds.Reset( );
		modelInfosInteriorIds.Reset( );
		sBuildingUsage.nNumTotalObjects = pBuildingPool->GetNoOfUsedSpaces( );
		GetSectorModelInfoIdsForPool( pBuildingPool, modelInfosIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorInteriorModelInfoIdsForPool( pBuildingPool, modelInfosInteriorIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorMemoryUsageForModelInfoIds( modelInfosIds, modelInfosInteriorIds, sBuildingUsage ); 
		GetSectorObjectsForPool( pBuildingPool, sBuildingUsage.vObjects, m_nCurrentSectorX, m_nCurrentSectorY );

		// Fetch Model Info Indices for Dummy Objects

		fwPool<CDummyObject>* pDummyPool = CDummyObject::GetPool( );
		modelInfosIds.Reset( );
		modelInfosInteriorIds.Reset( );
		sDummyUsage.nNumTotalObjects = pDummyPool->GetNoOfUsedSpaces( );
		GetSectorModelInfoIdsForPool( pDummyPool, modelInfosIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorInteriorModelInfoIdsForPool( pDummyPool, modelInfosInteriorIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorMemoryUsageForModelInfoIds( modelInfosIds, modelInfosInteriorIds, sDummyUsage );
		GetSectorObjectsForPool( pDummyPool, sDummyUsage.vObjects, m_nCurrentSectorX, m_nCurrentSectorY );

		// Fetch Model Info Indices for Interiors

		fwPool<CInteriorInst>* pInteriorPool = CInteriorInst::GetPool( );
		modelInfosIds.Reset( );
		modelInfosInteriorIds.Reset( );
		sInteriorUsage.nNumTotalObjects = pInteriorPool->GetNoOfUsedSpaces( );
		GetSectorModelInfoIdsForPool( pInteriorPool, modelInfosIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorInteriorModelInfoIdsForPool( pInteriorPool, modelInfosInteriorIds, m_nCurrentSectorX, m_nCurrentSectorY );
		// Do not fetch memory usage for this interior pool.
		//GetSectorMemoryUsageForModelInfoIds( modelInfosIds, modelInfosInteriorIds, sInteriorUsage );
		GetSectorObjectsForPoolNoMemoryStats( pInteriorPool, sInteriorUsage.vObjects, m_nCurrentSectorX, m_nCurrentSectorY );

		// Fetch Model Info Indices for Portals

		fwPool<CPortalInst>* pPortalPool = CPortalInst::GetPool( );
		modelInfosIds.Reset( );
		modelInfosInteriorIds.Reset( );
		sPortalUsage.nNumTotalObjects = pPortalPool->GetNoOfUsedSpaces( );
		GetSectorModelInfoIdsForPool( pPortalPool, modelInfosIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorInteriorModelInfoIdsForPool( pPortalPool, modelInfosInteriorIds, m_nCurrentSectorX, m_nCurrentSectorY );
		// Do not fetch memory usage for this portal pool.
		//GetSectorMemoryUsageForModelInfoIds( modelInfosIds, modelInfosInteriorIds, sPortalUsage );
		GetSectorObjectsForPoolNoMemoryStats( pPortalPool, sPortalUsage.vObjects, m_nCurrentSectorX, m_nCurrentSectorY );

		// Fetch Model Info Indices for Physical Objects

		CObject::Pool* pObjectPool = CObject::GetPool( );

		modelInfosIds.Reset( );
		modelInfosInteriorIds.Reset( );
		sPhysicalUsage.nNumTotalObjects = (s32) pObjectPool->GetNoOfUsedSpaces( );

#if !__CONSOLE
		GetSectorModelInfoIdsForPool( pObjectPool, modelInfosIds, m_nCurrentSectorX, m_nCurrentSectorY );
		GetSectorInteriorModelInfoIdsForPool( pObjectPool, modelInfosInteriorIds, m_nCurrentSectorX, m_nCurrentSectorY );		
		GetSectorObjectsForPool( pObjectPool, sPhysicalUsage.vObjects, m_nCurrentSectorX, m_nCurrentSectorY );
#endif
		GetSectorMemoryUsageForModelInfoIds( modelInfosIds, modelInfosInteriorIds, sPhysicalUsage );
		
		// Fetch Static Bounds Store Memory Usage
		GetStoreMemoryStats( g_StaticBoundsStore, sStaticBoundsUsage );
		GetStoreObjects( g_StaticBoundsStore.GetStreamingModuleId(), sStaticBoundsUsage.vObjects );

		bWriteLog = true;
	}

	if (bWriteLog)
		WriteLog( sAnimatedBuildUsage, sBuildingUsage, sDummyUsage,
				  sInteriorUsage, sPortalUsage, sPhysicalUsage,
				  sPhysicsUsage, sStaticBoundsUsage );


	UpdateState( );
}

void
CStreamingMemUsage::Update( )
{
#if DEBUG_DRAW
	if ( !m_bEnabled )
		return;

	if ( kDisabled != m_eMode )
	{
		char mode[20];
		char pass[20];
		switch (m_eMode)
		{
		case kDisabled: 
			snprintf( mode, 20, "Disabled" );
			break;
		case kExportArea:
			snprintf( mode, 20, "Area" );
			break;
		}		

		switch (m_ePass)
		{
		case kNavmeshLoad:
			snprintf( pass, 20, "Navmesh Load" );
			break;
		case kObjectLoad:
			snprintf( pass, 20, "Object Load" );
			break;
		case kStatsCapture:
			snprintf( pass, 20, "Capture" );
			break;
		}

		grcDebugDraw::AddDebugOutput( Color32(1.0f, 0.0f, 0.0f), 
			"Memory Usage Statistics Running [ %d, %d ].  Mode: %s.  Pass: %s.  Stream delay: %d ms; timer: %d ms.", 
			m_nCurrentSectorX, m_nCurrentSectorY, mode, pass, CSectorTools::GetStreamDelay(), 
			fwTimer::GetTimeInMilliseconds( ) - m_msTimer );
	}
#endif // DEBUG_DRAW
}

void
CStreamingMemUsage::ScanAndExportArea( )
{
	SetOutputFolder( );

	// If we have started automatically then create a progress file
	if ( PARAM_runmem.Get() )
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
		Assertf( m_pProgressFile, "Failed to create Memory Stats progress file (%s).", sProgressFilename );
		fprintf( m_pProgressFile, "START\n" );
		m_pProgressFile->Flush( );
	}

	SetEnabled( true );
	SetMode( kExportArea );

	if ( PARAM_runmem.Get() )
		SetupWorldCoordinates( );
}

bool
CStreamingMemUsage::GetEnabled( )
{
	return ( m_bEnabled );
}

//-----------------------------------------------------------------------------------------
// Private Methods
//-----------------------------------------------------------------------------------------

void
CStreamingMemUsage::SetMode( etMode eMode )
{
	m_eMode = eMode;

	if ( kExportArea == m_eMode )
	{
		// Fetch new start/end world coordinates
		const rage::Vector3& vStartCoords = CSectorTools::GetWorldStart( );
		const rage::Vector3& vEndCoords = CSectorTools::GetWorldEnd( );
		
		int sX = SECTORTOOLS_WORLD_WORLDTOSECTORX( vStartCoords.x );
		int eX = SECTORTOOLS_WORLD_WORLDTOSECTORX( vEndCoords.x );
		int sY = SECTORTOOLS_WORLD_WORLDTOSECTORY( vStartCoords.y );
		int eY = SECTORTOOLS_WORLD_WORLDTOSECTORY( vEndCoords.y );

		m_nStartSectorX = MIN( sX, eX );
		m_nStartSectorY = MIN( sY, eY );
		m_nEndSectorX = MAX( sX, eX );
		m_nEndSectorY = MAX( sY, eY );

		m_nCurrentSectorX = m_nStartSectorX;
		m_nCurrentSectorY = m_nStartSectorY;
	}
	else if ( kDisabled == m_eMode )
	{
		// Nothing additional required when disabled.
	}
	else
	{
		Assertf( false, "Unrecognised Streaming Memory Usage mode.  Contact Tools." );
	}
}

#if __BANK
void 
CStreamingMemUsage::InitWidgets( )
{
	bkBank* pWidgetBank = BANKMGR.FindBank( "Sector Tools" );
	if (AssertVerify(pWidgetBank))
	{
		pWidgetBank->PushGroup( "Streaming and Memory Usage" );
			pWidgetBank->AddButton( "Scan and Export Area", datCallback(CFA(ScanAndExportArea)) );
		pWidgetBank->PopGroup( );
	}
}
#endif // __BANK

void
CStreamingMemUsage::MoveToNextSector( )
{
	if ( kExportArea == m_eMode )
	{	
		// Log progress if we have started automatically to log our just finished sector.
		if ( m_pProgressFile && PARAM_runmem.Get() )
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

	m_ePass  = kNavmeshLoad;
}

void 
CStreamingMemUsage::SetOutputFolder( )
{
	char sOutputPath[MAX_OUTPUT_PATH];
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

	if ( CSectorTools::GetMidnight() || (PARAM_runmem.Get() && PARAM_midnight.Get()) )
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
	Assertf( pLevelName, "Running Streaming Memory Stats but have not specified a level name." );

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
	pDevice->MakeDirectory( sOutputPath );

	strncpy( m_sOutputDirectory, sOutputPath, MAX_OUTPUT_PATH );
}

void
CStreamingMemUsage::SetupWorldCoordinates( )
{
	if ( PARAM_rx.IsInitialized() )
		PARAM_rx.Get( m_nCurrentSectorX );

	if ( PARAM_ry.IsInitialized() )
		PARAM_ry.Get( m_nCurrentSectorY );
}

void		
CStreamingMemUsage::UpdateState( )
{
	// In ExportArea Mode, we need to handle switching between our passes,
	// and also switching to the next sector.
	if ( kExportArea == m_eMode )
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
			if ( !m_bSectorOK )
			{
				// Stop if we are at the end.
				if ( ( m_nEndSectorX == m_nCurrentSectorX ) &&
					( m_nEndSectorY == m_nCurrentSectorY ) )
				{
					m_msTimer = 0L;
					m_bSectorOK = false;
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
				CPed* pPlayer = FindPlayerPed( );
				if ( pPlayer )
					pPlayer->SetFixedPhysics( true, true );

				// Initialise timer
				if ( 0 == m_msTimer )
				{
					m_msTimer = fwTimer::GetTimeInMilliseconds( );
				}
				// Only change to PreSettle if we have exceeded our load period
				if ( (fwTimer::GetTimeInMilliseconds() - m_msTimer) > CSectorTools::GetStreamDelay() )
				{
					m_ePass = kStatsCapture;
					m_msTimer = 0L;
				}
			}
		}
		else if ( kStatsCapture == m_ePass )
		{			
			// Initialise timer
			if ( 0 == m_msTimer )
			{
				m_msTimer = fwTimer::GetTimeInMilliseconds( );
			}
			// Only change to post settle if we have exceeded our settle period
			if ( (fwTimer::GetTimeInMilliseconds() - m_msTimer) > CSectorTools::GetSettleDelay() )
			{
				// Switch back to ObjectLoad pass
				m_msTimer = 0L;
				m_bSectorOK = false;
				m_ePass = kNavmeshLoad;

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
		}
		else
		{
			Assertf( false, "Invalid pass.  Internal Error." );
		}
	}

	// Check if we have disabled ourselves to reactivate player ped physics
	if ( kDisabled == m_eMode )
	{
		CPed* pPlayer = FindPlayerPed( );
		Assert( pPlayer );
		if ( pPlayer )
			pPlayer->SetFixedPhysics( false, false );

		if ( m_pProgressFile && PARAM_runmem.Get() )
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
CStreamingMemUsage::GetSectorMemoryUsageForModelInfoIds( rage::atArray<u32>& modelInfosIds, 
														 rage::atArray<u32>& modelInfosInteriorIds, 
														 TPoolMemoryUsage& poolMemoryUsage )
{
	// Get all objects
	CStreamingDebug::GetMemoryStatsByAllocation( modelInfosIds, 
									(poolMemoryUsage.nTotal), 
									(poolMemoryUsage.nTotalVirtual),
									(poolMemoryUsage.nDrawable), 
									(poolMemoryUsage.nDrawableVirtual), 
									(poolMemoryUsage.nTexture), 
									(poolMemoryUsage.nTextureVirtual), 
									(poolMemoryUsage.nBounds),
									(poolMemoryUsage.nBoundsVirtual) );

	poolMemoryUsage.nNumModelInfos = modelInfosIds.GetCount();
	
	// Get Interior objects
	CStreamingDebug::GetMemoryStatsByAllocation( modelInfosInteriorIds, 
									(poolMemoryUsage.nTotalInterior), 
									(poolMemoryUsage.nTotalInteriorVirtual), 
									(poolMemoryUsage.nDrawableInterior), 
									(poolMemoryUsage.nDrawableInteriorVirtual), 
									(poolMemoryUsage.nTextureInterior), 
									(poolMemoryUsage.nTextureInteriorVirtual), 
									(poolMemoryUsage.nBoundsInterior),
									(poolMemoryUsage.nBoundsInteriorVirtual) );

	poolMemoryUsage.nNumModelInfosInterior = modelInfosInteriorIds.GetCount( );
}

// DHM :: See CStreamingDebug::DisplayObjectsInMemory for reference
void CStreamingMemUsage::GetStoreObjects( s32 moduleId, rage::atArray<TStoreObject>& objects )
{
	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);
	rage::atArray<strIndex> objInMemory;

	for(s32 i=0; i<pModule->GetCount(); i++)
	{
		if ( pModule->HasObjectLoaded(strLocalIndex(i)) && 
			 pModule->IsObjectInImage(strLocalIndex(i)) )
		{
			strIndex idx = pModule->GetStreamingIndex(strLocalIndex(i));
			objInMemory.PushAndGrow( idx );
		}
	}

	GetStoreObjectsList( objInMemory, objects );
}

// DHM :: See CStreamingDebug::DisplayObjectList for reference
void
CStreamingMemUsage::GetStoreObjectsList( const rage::atArray<strIndex>& objsInMemory, rage::atArray<TStoreObject>& objects )
{
	for ( int n = 0; n < objsInMemory.GetCount(); ++n )
	{
		strIndex obj = objsInMemory[n];		
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(obj);

		TStoreObject storeObj(strStreamingEngine::GetInfo().GetObjectName(obj), info.ComputeVirtualSize(obj), info.ComputePhysicalSize(obj));
		objects.PushAndGrow( storeObj );
	}
}

//-----------------------------------------------------------------------------------------
// Private Methods :: Logging Functions
//-----------------------------------------------------------------------------------------

void		
CStreamingMemUsage::WriteLog( const TPoolMemoryUsage& sAnimatedUsage,
							  const TPoolMemoryUsage& sBuildingUsage,
							  const TPoolMemoryUsage& sDummyUsage,
							  const TPoolMemoryUsage& sInteriorUsage,
							  const TPoolMemoryUsage& sPortalUsage,
							  const TPoolMemoryUsage& sPhysicalUsage,
							  const TStoreMemoryUsage& sPhysicsUsage,
							  const TStoreMemoryUsage& sStaticBoundsUsage)
{
	int nX = m_nCurrentSectorX;
	int nY = m_nCurrentSectorY;
	float fX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( nX );
	float fY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( nY );
	Vector3 vSectorCentre( fX, fY, 25.0f );

	CPed* pPlayer = FindPlayerPed( );
	Assert( pPlayer );
	Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	char filename[MAX_OUTPUT_PATH];
	snprintf( filename, MAX_OUTPUT_PATH, "%s/strmmemuse_sector_%d_%d.mem", m_sOutputDirectory, nX, nY );
	fiStream* fpOutputFile = fiStream::Create( filename );

	CSectorTools::WriteCommon(fpOutputFile,m_sectorNum,m_startTime,m_description,m_result, nX, nY , vSectorCentre, vPlayerPos);
	m_sectorNum++;

	fpOutputFile->Flush( );

	WritePoolMemoryUsage( fpOutputFile, "Animated Building", sAnimatedUsage );
	WritePoolMemoryUsage( fpOutputFile, "Building", sBuildingUsage );
	WritePoolMemoryUsage( fpOutputFile, "Dummy Object", sDummyUsage );
	WritePoolMemoryUsage( fpOutputFile, "Interior", sInteriorUsage );
	WritePoolMemoryUsage( fpOutputFile, "Portal", sPortalUsage );
	WritePoolMemoryUsage( fpOutputFile, "Physical Object", sPhysicalUsage );

	WriteStoreMemoryUsage( fpOutputFile, "Physics Store", sPhysicsUsage );
	WriteStoreMemoryUsage( fpOutputFile, "Static Bounds Store", sStaticBoundsUsage );		

	fprintf( fpOutputFile, "</sector>\n" );

	fpOutputFile->Flush( );
	fpOutputFile->Close( );
}

void
CStreamingMemUsage::WritePoolMemoryUsage( rage::fiStream* stream, 
										  const char* sPoolName,
										  const TPoolMemoryUsage& usage )
{
	fprintf( stream, "\t<pool name=\"%s\" totalcount=\"%d\" count=\"%d\" interiorCount=\"%d\">\n", 
			sPoolName, usage.nNumTotalObjects, usage.nNumModelInfos, usage.nNumModelInfosInterior );
	fprintf( stream, "\t\t<memory name=\"%s\" count=\"%d\" countVirtual=\"%d\" interiorCount=\"%d\" interiorCountVirtual=\"%d\" />\n", 
			"total", usage.nTotal, usage.nTotalVirtual, usage.nTotalInterior, usage.nTotalInteriorVirtual );
	fprintf( stream, "\t\t<memory name=\"%s\" count=\"%d\" countVirtual=\"%d\" interiorCount=\"%d\" interiorCountVirtual=\"%d\" />\n", 
			"drawable", usage.nDrawable, usage.nDrawableVirtual, usage.nDrawableInterior, usage.nDrawableInteriorVirtual );
	fprintf( stream, "\t\t<memory name=\"%s\" count=\"%d\" countVirtual=\"%d\" interiorCount=\"%d\" interiorCountVirtual=\"%d\" />\n", 
			"texture", usage.nTexture, usage.nTextureVirtual, usage.nTextureInterior, usage.nTextureInteriorVirtual );
	fprintf( stream, "\t\t<memory name=\"%s\" count=\"%d\" countVirtual=\"%d\" interiorCount=\"%d\" interiorCountVirtual=\"%d\" />\n", 
			"bounds", usage.nBounds, usage.nBoundsVirtual, usage.nBoundsInterior, usage.nBoundsInteriorVirtual );
	fprintf( stream, "\t\t<objects>\n" );
	for ( int i = 0; i < usage.vObjects.GetCount(); ++i )
	{
		fprintf( stream, "\t\t\t<object name=\"%s\" drawable=\"%d\" texture=\"%d\" bounds=\"%d\" drawableVirtual=\"%d\" textureVirtual=\"%d\" boundsVirtual=\"%d\" isInterior=\"%d\" />\n", 
				 usage.vObjects[i].sName.c_str(), usage.vObjects[i].nDrawable,
				 usage.vObjects[i].nTexture, usage.vObjects[i].nBounds, 
				 usage.vObjects[i].nDrawableVirtual, usage.vObjects[i].nTextureVirtual,
				 usage.vObjects[i].nBoundsVirtual, usage.vObjects[i].bInterior );
	}
	fprintf( stream, "\t\t</objects>\n" );
	fprintf( stream, "\t</pool>\n" );
}

void
CStreamingMemUsage::WriteStoreMemoryUsage( rage::fiStream* stream, 
										   const char* sStoreName,
										   const TStoreMemoryUsage& usage )
{
	fprintf( stream, "\t<store name=\"%s\">\n", sStoreName );
	fprintf( stream, "\t\t<memory name=\"%s\" count=\"%d\" />\n", "total", usage.nTotal );
	fprintf( stream, "\t\t<memory name=\"%s\" count=\"%d\" />\n", "virtual", usage.nVirtual );
	fprintf( stream, "\t\t<memory name=\"%s\" count=\"%d\" />\n", "physical", usage.nPhysical );
	fprintf( stream, "\t\t<memory name=\"%s\" count=\"%d\" />\n", "largest", usage.nLargest );
	fprintf( stream, "\t\t<store_objects>\n" );
	for ( int i = 0; i < usage.vObjects.GetCount(); ++i )
	{
		fprintf( stream, "\t\t\t<store_object name=\"%s\" virtual=\"%d\" physical=\"%d\" />\n", 
			usage.vObjects[i].sName.c_str(), usage.vObjects[i].nVirtual, usage.vObjects[i].nPhysical );
	}
	fprintf( stream, "\t\t</store_objects>\n" );
	fprintf( stream, "\t</store>\n" );
}

#endif // SECTOR_TOOLS_EXPORT
