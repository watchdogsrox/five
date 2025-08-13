//
// filename:	ObjectPositionChecker.cpp
// author:		David Muir <david.muir@rockstarnorth.com>
// date:		21 June 2007
// description:	
//
 
// Game headers
#include "ObjectPositionChecker.h"

#if ( SECTOR_TOOLS_EXPORT )

// --- Include Files ----------------------------------------------------------------------

// Rage headers
#include "atl/array.h"
#include "atl/string.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "grcore/debugdraw.h"
#include "vector/vector3.h"
#include "vector/matrix34.h"

// Game headers
#include "debug/BlockView.h"	
#include "SectorTools.h"
#include "game/Clock.h"
#include "vehicleAi/pathfind.h"
#include "Objects/DummyObject.h"
#include "Objects/object.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Peds/Ped.h"
#include "streaming/streaming.h"


// --- Constants --------------------------------------------------------------------------

// --- Structure/Class Definitions --------------------------------------------------------

// --- Globals ----------------------------------------------------------------------------

// --- Static Globals ---------------------------------------------------------------------

// --- Static class members ---------------------------------------------------------------
atArray<CObjectPositionChecker::Coord>	CObjectPositionChecker::m_atCoords;
atString								CObjectPositionChecker::m_sInputFile;
bool									CObjectPositionChecker::m_bEnabled = false;
bool									CObjectPositionChecker::m_bAutomatic;
bool									CObjectPositionChecker::m_bMoreAutoCoordsToDo;
CObjectPositionChecker::etMode			CObjectPositionChecker::m_eMode = CObjectPositionChecker::kDisabled;
CObjectPositionChecker::etPass			CObjectPositionChecker::m_ePass = CObjectPositionChecker::kObjectLoad;
char									CObjectPositionChecker::m_sUserOutputDirectory[MAX_OUTPUT_PATH];
u32										CObjectPositionChecker::m_msTimer = 0L;
int										CObjectPositionChecker::m_nCurrentSectorX = 0;
int										CObjectPositionChecker::m_nCurrentSectorY = 0;
int										CObjectPositionChecker::m_nStartSectorX = 0;
int										CObjectPositionChecker::m_nStartSectorY = 0;
int										CObjectPositionChecker::m_nEndSectorX = 0;
int										CObjectPositionChecker::m_nEndSectorY = 0;
bool									CObjectPositionChecker::m_bRestrictToSpecificEntities = false;
char									CObjectPositionChecker::m_sEntityRestrictionFile[MAX_OUTPUT_PATH];
atArray<u32>							CObjectPositionChecker::m_atEntityGuids;
bool									CObjectPositionChecker::m_bScanInProgress = false;

// --- Code -------------------------------------------------------------------------------

void
CObjectPositionChecker::Init( )
{
	m_bEnabled = false;
	m_nCurrentSectorX = SECTORTOOLS_WORLD_WORLDTOSECTORX( 0.0f );
	m_nCurrentSectorY = SECTORTOOLS_WORLD_WORLDTOSECTORY( 0.0f );
	m_msTimer = 0L;
	m_nStartSectorX = 60;
	m_nStartSectorY = 60;
	m_nEndSectorX = 0;
	m_nEndSectorY = 0;

	if ( PARAM_sectortools_input.IsInitialized() )
	{
		const char* sInputFile = NULL;
		if ( PARAM_sectortools_input.Get(sInputFile) )
		{
			m_sInputFile = sInputFile;
			m_bAutomatic = true;
			m_bScanInProgress = true;
			GetAutomaticCoords();
			if(m_atCoords.GetCount() > 0)
				m_bMoreAutoCoordsToDo = true;
			SetEnabled(true);
		}		
	}
	else
	{
		m_sInputFile = "";
		m_bAutomatic = false;
		m_bMoreAutoCoordsToDo = false;
		SetEnabled(false);
	}

	// If no file is specified but we're doing automatic execution, set an output
	// folder to use
	if ( PARAM_sectortools_output.IsInitialized() )
	{
		const char* sOutputDir = NULL;
		if ( PARAM_sectortools_output.Get( sOutputDir ) )
		{
			safecpy(m_sUserOutputDirectory, sOutputDir, sizeof(m_sUserOutputDirectory));
			safecat(m_sUserOutputDirectory, "\\", sizeof(m_sUserOutputDirectory));
			Assert(ASSET.CreateLeadingPath(m_sUserOutputDirectory ));
		}		
	}
	else
	{
		safecpy(m_sUserOutputDirectory, "common:/data/object_position_checker/", sizeof(m_sUserOutputDirectory));
	}

	// Check if we are restricting the run through to particular entities.
	if ( PARAM_sectortools_entities.IsInitialized() )
	{
		const char* sEntitiesFile = NULL;
		if ( PARAM_sectortools_entities.Get(sEntitiesFile) )
		{
			safecpy(m_sEntityRestrictionFile, sEntitiesFile, sizeof(m_sEntityRestrictionFile));
			m_bRestrictToSpecificEntities = true;
		}
	}
}

void
CObjectPositionChecker::Shutdown( )
{
	m_bEnabled = false;
	m_atCoords.Reset();
}

void
CObjectPositionChecker::GetAutomaticCoords( )
{
	FileHandle fid;
	char* pLine;
	m_atCoords.Reset();

	CFileMgr::SetDir("");
	fid = CFileMgr::OpenFile(m_sInputFile.c_str()); // open for reading

	if (!CFileMgr::IsValidFileHandle(fid))
	{
		Assertf(0, "Could not open %s file for object position checking!", m_sInputFile.c_str() );
		return;
	}

	if (fid)
	{	
		// Co-ords are space separated in the text file: sx sy ex ey
		while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) )
		{
			Displayf("%s", pLine);
			Coord newCoords = {	static_cast<float>(atof(strtok(pLine, " "))),
								static_cast<float>(atof(strtok(NULL, " "))),
								static_cast<float>(atof(strtok(NULL, " "))), 
								static_cast<float>(atof(strtok(NULL, " ")))	};
			m_atCoords.PushAndGrow(newCoords);

			Displayf("Object pos checker coords - sx:%f sy:%f ex:%f ey:%f", newCoords.sx, newCoords.sy, newCoords.ex, newCoords.ey);
		}
		CFileMgr::CloseFile(fid);
	}
}

void
CObjectPositionChecker::GetEntityGuids()
{
	FileHandle fid;
	char* pLine;
	m_atEntityGuids.Reset();

	CFileMgr::SetDir("");
	fid = CFileMgr::OpenFile(m_sEntityRestrictionFile); // open for reading

	if (!CFileMgr::IsValidFileHandle(fid))
	{
		Assertf(0, "Could not open %s file for entity guids!", m_sEntityRestrictionFile );
		return;
	}

	if (fid)
	{	
		// Each line contains a single entity guid.
		while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) )
		{
			Displayf("%s", pLine);
			u32 guid = static_cast<u32>(atol(pLine));
			m_atEntityGuids.PushAndGrow(guid);

			Displayf("Object pos checker entity - %u", guid);
		}
		CFileMgr::CloseFile(fid);
	}
}

void
CObjectPositionChecker::Run( )
{
	if ( !m_bEnabled )
		return;
	
	// Check to make sure player exists if we are running this automatically
	if (m_bAutomatic) 
	{
		CPed* pPlayer = FindPlayerPed( );
		if (!pPlayer)
			return;

		if(m_bMoreAutoCoordsToDo)
		{
			SetMode( kExportArea );
			m_bMoreAutoCoordsToDo = false;
		}
	}

	if ( kDisabled == m_eMode )
		return;

	if ( kObjectLoad == m_ePass )
	{
		float fX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( m_nCurrentSectorX );
		float fY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( m_nCurrentSectorY );
		float fZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, fX, fY, 500.0f);

		// Warp player to new sector centre, or to the closest path node possible
		CPed* pPlayer = FindPlayerPed( );
		Assert( pPlayer );

		fZ += ( pPlayer->GetBoundingBoxMax().z - pPlayer->GetBoundingBoxMin().z ) / 2 + 0.25f;

		Vector3 vSectorCentre( fX, fY, fZ );
		pPlayer->Teleport( vSectorCentre, pPlayer->GetCurrentHeading() );

		// Load all objects
		CStreaming::LoadAllRequestedObjects( );

		// Each time we move to a new sector set game clock dependent 
		// on user-defined var
		if ( CSectorTools::GetMidnight() || (PARAM_runmem.Get() && PARAM_midnight.Get()) )
		{
			if (CClock::GetMode()==CLOCK_MODE_DYNAMIC)
			{
				CClock::SetTime(0, 0, 0);
			}
			else if (CClock::GetMode()==CLOCK_MODE_FIXED)
			{
				CClock::SetTimeSample(0);
			}
		}
		else
		{
			if (CClock::GetMode()==CLOCK_MODE_DYNAMIC)
			{
				CClock::SetTime(12, 0, 0);
			}
			else if (CClock::GetMode()==CLOCK_MODE_FIXED)
			{
				CClock::SetTimeSample(2);
			}
		}
	}
	// In PreSettle pass and before our settle timer has been started,
	// check to see if we have any CObject's in the pool, if not we 
	// abort this sector as nothing will be logged.
	else if ( ( kPreSettle == m_ePass ) && ( 0 == m_msTimer ) )
	{
		CObject::Pool* pObjPool = CObject::GetPool( );
		int nObjectPoolUsedSlots = (int) pObjPool->GetNoOfUsedSpaces( );
		int nObjectPoolSize = (int) pObjPool->GetSize( );
		bool bHasInterestingObjects = false;

		// Check for interesting objects
		for ( int nObj = 0; nObj < nObjectPoolSize; ++nObj )
		{
			// Get pool active object
			CObject* pObject = pObjPool->GetSlot( nObj );
			if ( !pObject )
				continue;

			// Only process interesting objects
			if ( IsInterestingObject( pObject ) )
			{
				bHasInterestingObjects = true;

				// Before activating our object lets attempt to place it on 
				// the ground using a z-probe.
				Vector3 objPos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
				float fZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, objPos.x, objPos.y, objPos.z );
				objPos.z = fZ;				

				pObject->ActivatePhysics( );
			}
		}

		// Write empty file
		if ( ( 0 == nObjectPoolUsedSlots ) || ( !bHasInterestingObjects ) )
		{
			atArray<Matrix34> vObjectMatInitial;
			atArray<Matrix34> vObjectMatFinal;
			atArray<atString> vObjectNames;

			WriteLog( vObjectNames, vObjectMatInitial, vObjectMatFinal, nObjectPoolUsedSlots );
			
			// Skip pass as nothing to log...
			m_ePass = kPostSettle;
		}
	}	
	// In PostSettle pass we look at our CObject's and CDummyObject's recording
	// their positions and writing out to file.
	else if ( kPostSettle == m_ePass )
	{
		CObject::Pool* pObjPool = CObject::GetPool( );
		int nObjectPoolUsedSlots = (int) pObjPool->GetNoOfUsedSpaces( );
		int nObjectPoolSize = (int) pObjPool->GetSize( );

		atArray<Matrix34> vObjectMatInitial;
		atArray<Matrix34> vObjectMatFinal;
		atArray<atString> vObjectNames;

		// Get final object positions
		for ( int nObj = 0; nObj < nObjectPoolSize; ++nObj )
		{
			// Get pool active object
			CObject* pObject = pObjPool->GetSlot( nObj );
			if ( !pObject )
				continue;

			// Only process interesting objects
			if ( !IsInterestingObject( pObject ) )
				continue;

			CDummyObject* pDummy = pObject->GetRelatedDummy( );
			if ( !pDummy )
				continue;

			vObjectNames.PushAndGrow( atString(CModelInfo::GetBaseModelInfoName( pObject->GetModelId( ) ) ) );
			vObjectMatInitial.PushAndGrow( MAT34V_TO_MATRIX34(pDummy->GetMatrix( ) ));
			vObjectMatFinal.PushAndGrow( MAT34V_TO_MATRIX34(pObject->GetMatrix( )) );	

			pObject->SetFixedPhysics( true, true );
		}	

		Assert( vObjectNames.GetCount() == vObjectMatInitial.GetCount() );
		Assert( vObjectNames.GetCount() == vObjectMatFinal.GetCount() );

		// Write export log
		WriteLog( vObjectNames, vObjectMatInitial, vObjectMatFinal, nObjectPoolUsedSlots );
	}

	UpdateState( );
}

void
CObjectPositionChecker::Update( )
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
			sprintf( mode, "Disabled" );
			break;
		case kExportArea:
			sprintf( mode, "Area" );
			break;
		case kExportSpecific:
			sprintf( mode, "Specific" );
			break;
		}		

		switch (m_ePass)
		{
		case kObjectLoad:
			sprintf( pass, "Object Load" );
			break;
		case kPreSettle:
			sprintf( pass, "Pre-Settle" );
			break;
		case kPostSettle:
			sprintf( pass, "Post-Settle" );
			break;
		}

		grcDebugDraw::AddDebugOutput( Color32(1.0f, 0.0f, 0.0f), 
			"Object Position Checker Running [ %d, %d ].  Mode: %s.  Pass: %s.  Stream delay: %d ms, Physics delay: %d ms.", 
			m_nCurrentSectorX, m_nCurrentSectorY, mode, pass, CSectorTools::GetStreamDelay(), CSectorTools::GetSettleDelay() );
	}
#endif
}

void
CObjectPositionChecker::ScanAndExportSpecificSector( )
{
	SetEnabled( true );
	SetScanInProgress( true );

	if (m_bRestrictToSpecificEntities)
		GetEntityGuids();

	SetMode( kExportSpecific );
}

void
CObjectPositionChecker::ScanAndExportArea( )
{
	SetEnabled( true );
	SetScanInProgress( true );

	if (m_bRestrictToSpecificEntities)
		GetEntityGuids();

	SetMode( kExportArea );
}

void
CObjectPositionChecker::AbortScan( )
{
	SetMode( kDisabled );
	SetScanInProgress( false );
	m_atCoords.Reset();
}

//-----------------------------------------------------------------------------------------
// Private Methods
//-----------------------------------------------------------------------------------------

void
CObjectPositionChecker::SetMode( etMode eMode )
{
	m_eMode = eMode;

	if ( kExportSpecific == m_eMode )
	{
		m_nCurrentSectorX = (int)CSectorTools::GetSector( ).x;
		m_nCurrentSectorY = (int)CSectorTools::GetSector( ).y;

		CPed* pPlayer = FindPlayerPed( );
		Assert( pPlayer );
		pPlayer->SetFixedPhysics( true, false );
	}
	else if ( kExportArea == m_eMode )
	{
		// Fetch new start/end world coordinates
		float startX, startY, endX, endY;
		if(m_bAutomatic)
		{
			// Make sure there's a coord struct to use otherwise return
			if(m_atCoords.GetCount() > 0)
			{
				Coord newCoords = m_atCoords.Pop();
				startX = newCoords.sx;
				startY = newCoords.sy;
				endX = newCoords.ex;
				endY = newCoords.ey;
			}
			else
			{
				m_eMode = kDisabled;
				return;
			}
		}
		else
		{
			const rage::Vector3& vStartCoords = CSectorTools::GetWorldStart( );
			const rage::Vector3& vEndCoords = CSectorTools::GetWorldEnd( );
			startX = vStartCoords.x;
			startY = vStartCoords.y;
			endX = vEndCoords.x;
			endY = vEndCoords.y;
		}		

		int sX = SECTORTOOLS_WORLD_WORLDTOSECTORX(	startX	);
		int eX = SECTORTOOLS_WORLD_WORLDTOSECTORX(	endX	);
		int sY = SECTORTOOLS_WORLD_WORLDTOSECTORY(	startY	);
		int eY = SECTORTOOLS_WORLD_WORLDTOSECTORY(	endY	);

		m_nStartSectorX = MIN( sX, eX );
		m_nStartSectorY = MIN( sY, eY );
		m_nEndSectorX = MAX( sX, eX );
		m_nEndSectorY = MAX( sY, eY );

		m_nCurrentSectorX = m_nStartSectorX;
		m_nCurrentSectorY = m_nStartSectorY;

		CPed* pPlayer = FindPlayerPed( );
		Assert( pPlayer );
		pPlayer->SetFixedPhysics( true, false );
	}
	else if ( kDisabled == m_eMode )
	{
		CPed* pPlayer = FindPlayerPed( );
		Assert( pPlayer );
		pPlayer->SetFixedPhysics( false, false );
	}
	else
	{
		Assertf( false, "Unrecognised Streaming Memory Usage mode.  Contact Tools." );
	}
}

#if __BANK
void
CObjectPositionChecker::InitWidgets( )
{
	// Find or create widget bank
	bkBank* pWidgetBank = BANKMGR.FindBank( "Sector Tools" );
	if (AssertVerify(pWidgetBank))
	{
		pWidgetBank->PushGroup( "Physical Object Position Checker" );
			pWidgetBank->AddButton( "Set output directory", datCallback(CFA(SetUserOutputFolder)) );
			pWidgetBank->AddText( "Output directory:", m_sUserOutputDirectory, MAX_OUTPUT_PATH, false );
			pWidgetBank->AddToggle( "Restrict to Specific Entities", &m_bRestrictToSpecificEntities );
			pWidgetBank->AddButton( "Set entity restriction file", datCallback(CFA(SetEntitiesFile)) );
			pWidgetBank->AddText( "Entity Restriction File:", m_sEntityRestrictionFile, MAX_OUTPUT_PATH, false );
			pWidgetBank->AddButton( "Scan and Export Selected Sector", datCallback(CFA(ScanAndExportSpecificSector)) );
			pWidgetBank->AddButton( "Scan and Export Area", datCallback(CFA(ScanAndExportArea)) );
			pWidgetBank->AddToggle( "Scan In Progress (read-only)", &m_bScanInProgress );
			pWidgetBank->AddButton( "Abort Running Scan", datCallback(CFA(AbortScan)) );
		pWidgetBank->PopGroup( );
	}
}
#endif // __BANK
void
CObjectPositionChecker::MoveToNextSector( )
{
	if ( kExportArea == m_eMode )
	{	
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
CObjectPositionChecker::SetUserOutputFolder( )
{
#if __BANK
	if(m_bAutomatic == false)
	{
		BANKMGR.OpenFile( m_sUserOutputDirectory, MAX_OUTPUT_PATH, "*.*", true, "Object Checker File" );
		
		// Remove the selected filename, leaving only the path
		char* ptr = &m_sUserOutputDirectory[ strlen(m_sUserOutputDirectory)-1 ];
		while (ptr && ptr != m_sUserOutputDirectory && *ptr != '/' && *ptr != '\\') ptr--;
		if ( *ptr == '\\' || *ptr == '/' )
			*ptr = 0;
	}
#endif // __BANK
}

void
CObjectPositionChecker::SetEntitiesFile( )
{
#if __BANK
	if(m_bAutomatic == false)
	{
		BANKMGR.OpenFile( m_sEntityRestrictionFile, MAX_OUTPUT_PATH, "*.*", true, "Entity Restriction File" );
	}
#endif // __BANK
}

void
CObjectPositionChecker::UpdateState( )
{
	m_bScanInProgress = true;

	// In ExportAll Mode, we increment to our next block.  This will have to 
	// change when we start using a finer grain of "grid".
	// In ExportGroup Mode, we need to handle switching between our passes,
	// and also switching to the next sector in our group of 4.
	if ( kExportArea == m_eMode )
	{
		if ( kObjectLoad == m_ePass )
		{
			// Initialise timer
			if ( 0 == m_msTimer )
			{
				m_msTimer = fwTimer::GetTimeInMilliseconds( );
			}
			// Only change to PreSettle if we have exceeded our load period
			if ( (fwTimer::GetTimeInMilliseconds() - m_msTimer) > CSectorTools::GetStreamDelay() )
			{
				m_ePass = kPreSettle;
				m_msTimer = 0L;
			}
		}
		else if ( kPreSettle == m_ePass )
		{
			// Initialise timer
			if ( 0 == m_msTimer )
			{
				m_msTimer = fwTimer::GetTimeInMilliseconds( );
			}
			// Only change to post settle if we have exceeded our settle period
			if ( (fwTimer::GetTimeInMilliseconds() - m_msTimer) > CSectorTools::GetSettleDelay() )
			{
				m_ePass = kPostSettle;
				m_msTimer = 0L;
			}
		}
		else if ( kPostSettle == m_ePass )
		{
			// Switch back to PreSettle pass
			m_ePass = kObjectLoad;

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
		else
			Assertf( false, "Invalid pass.  Internal Error." );
	}
	// In ExportSpecific Mode, we only need to handle switching between our
	// two passes.
	else if ( kExportSpecific == m_eMode )
	{
		if ( kObjectLoad == m_ePass )
		{
			// Initialise timer
			if ( 0 == m_msTimer )
			{
				m_msTimer = fwTimer::GetTimeInMilliseconds( );
			}
			// Only change to PreSettle if we have exceeded our load period
			if ( (fwTimer::GetTimeInMilliseconds() - m_msTimer) > CSectorTools::GetStreamDelay() )
			{
				m_ePass = kPreSettle;
				m_msTimer = 0L;
			}
		}
		else if ( kPreSettle == m_ePass )
		{
			// Initialise timer
			if ( 0 == m_msTimer )
			{
				m_msTimer = fwTimer::GetTimeInMilliseconds( );
			}
			// Only change to post settle if we have exceeded our settle period
			if ( (fwTimer::GetTimeInMilliseconds() - m_msTimer) > CSectorTools::GetSettleDelay() )
			{
				m_ePass = kPostSettle;
				m_msTimer = 0L;
			}
		}
		else if ( kPostSettle == m_ePass )
		{
			m_eMode = kDisabled;
			m_ePass = kObjectLoad;
		}
		else
			Assertf( false, "Invalid pass.  Internal Error." );
	}

	// Check if we have disabled ourselves to reactivate player ped physics
	if ( kDisabled == m_eMode )
	{
		CPed* pPlayer = FindPlayerPed( );
		Assert( pPlayer );
		pPlayer->SetFixedPhysics(false, false);

		// If automatic execution then see if we need to run it again for
		// new coords.  Otherwise turn off the automatic flag too.
		if(m_bAutomatic && m_atCoords.GetCount() > 0)
			m_bMoreAutoCoordsToDo = true;
		else
		{
			m_bAutomatic = false;
			m_bScanInProgress = false;
			m_atCoords.Reset();
		}
	}
}

void
CObjectPositionChecker::WriteLog( const atArray<atString>& vObjectNames, 
								  const atArray<Matrix34>& vObjectMatInitial,
								  const atArray<Matrix34>& vObjectMatFinal,
								  int nPoolObjects )
{
	int nX = m_nCurrentSectorX;
	int nY = m_nCurrentSectorY;
	float fX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( nX );
	float fY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( nY );
	Vector3 vSectorCentre( fX, fY, 25.0f );

	CPed* pPlayer = FindPlayerPed( );
	Assert( pPlayer );
	Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	s32 blockNum = CBlockView::GetCurrentBlockInside(vSectorCentre);
	const char *blockName = CBlockView::GetBlockName(blockNum);

	// Output File (to be read into Max later)
	char filename[MAX_OUTPUT_PATH];
	sprintf( filename, "%s/%s_objectposchk_sector_%d_%d.objchk", m_sUserOutputDirectory, blockName, nX, nY );
	fiStream* fpOutputFile = fiStream::Create( filename );
	fprintf( fpOutputFile, "# Object Position Checker\n" );
	fprintf( fpOutputFile, "# Sector                    : %d %d\n", nX, nY );
	fprintf( fpOutputFile, "# Sector World Centre       : %f %f %f\n", vSectorCentre.x, vSectorCentre.y, vSectorCentre.z );
	fprintf( fpOutputFile, "# Sector Min Coords         : %f %f\n", SECTORTOOLS_WORLD_SECTORTOWORLDX_MIN(nX), SECTORTOOLS_WORLD_SECTORTOWORLDY_MIN(nY) );
	fprintf( fpOutputFile, "# Sector Max Coords         : %f %f\n", SECTORTOOLS_WORLD_SECTORTOWORLDX_MAX(nX), SECTORTOOLS_WORLD_SECTORTOWORLDY_MAX(nY) );
	fprintf( fpOutputFile, "# Block Name(Sector Centre) : %s\n", blockName);
	fprintf( fpOutputFile, "# Actual Player Pos         : %f %f %f\n", vPlayerPos.x, vPlayerPos.y, vPlayerPos.z );
	fprintf( fpOutputFile, "# Number of Objects         : %d\n", vObjectNames.GetCount() );
	fprintf( fpOutputFile, "# Objects in Pool           : %d\n", nPoolObjects );
	fprintf( fpOutputFile, "# --------------------------------------------------------------------------------\n" );
	fprintf( fpOutputFile, "\n" );
	fpOutputFile->Flush( );

	fprintf( fpOutputFile, "# Sector Objects\n" );
	fprintf( fpOutputFile, "# name, ix, iy, iz, fx, fy, fz, iqx, iqy, iqz, iqw, fqx, fqy, fqz, fqw\n\n" );
	for ( int nObj = 0; nObj < vObjectNames.GetCount(); ++nObj )
	{
		Vector3 vInitial( vObjectMatInitial[nObj].d ); // Initial position
		Vector3 vFinal( vObjectMatFinal[nObj].d ); // Final position
		Quaternion qInitial;
		Quaternion qFinal;

		vObjectMatInitial[nObj].ToQuaternion( qInitial );
		vObjectMatFinal[nObj].ToQuaternion( qFinal );

		fprintf( fpOutputFile, "%s, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f\n", 
			vObjectNames[nObj].c_str(), 
			vInitial.x, vInitial.y, vInitial.z, 
			vFinal.x, vFinal.y, vFinal.z,
			qInitial.x, qInitial.y, qInitial.z, qInitial.w,
			qFinal.x, qFinal.y, qFinal.z, qFinal.w );
	}
	fprintf( fpOutputFile, "\n# End of Sector\n" );
	fpOutputFile->Flush( );
	fpOutputFile->Close( );
}

bool
CObjectPositionChecker::IsInterestingObject( CObject* pObject )
{
	// Check that it is a valid object we should be doing a position check on.
	// Currently we should ignore "Is Fixed" objects (obviously), objects that
	// are marked "Has Door Physics" and "rooted" objects.

	// Is Fixed
	if ( pObject->GetIsAnyFixedFlagSet() )
		return false;

	// Door Objects
	if ( pObject->IsADoor() )
		return false;
	
	// Check whether we are restricting the pass to certain entities.
	u32 objectGuid;
	if ( pObject->GetGuid(objectGuid) )
	{
		if ( m_bRestrictToSpecificEntities && m_atEntityGuids.Find(objectGuid) == -1)
			return false;
	}

	// Rooted
	phInst* pPhysInst = pObject->GetCurrentPhysicsInst( );
	if ( pPhysInst != NULL && ( !pPhysInst->IsInLevel() || !CPhysics::GetLevel()->IsInactive(pPhysInst->GetLevelIndex()) ) )
		return false;

	fragInstGta* pFragInst = dynamic_cast<fragInstGta*>( pPhysInst );
	if ( pFragInst )
	{
		if ( pFragInst->GetTypePhysics()->GetMinMoveForce() > 0.0f )
			return false;
	}

	// Get dummy from active object
	CDummyObject* pDummy = pObject->GetRelatedDummy( );
	// It is ok to have a NULL dummy, so don't assert, just move on...
	if ( !pDummy )
		return false;

	// Ensure our dummy object is in our current sector
	Vector3 dummyPos = VEC3V_TO_VECTOR3(pDummy->GetTransform().GetPosition());
	int nX = m_nCurrentSectorX;
	int nY = m_nCurrentSectorY;
	float fSectorCentreX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( nX );
	float fSectorCentreY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( nY );
	float fSectorLeft = fSectorCentreX - ( SECTORTOOLS_WORLD_WIDTHOFSECTOR / 2.0f );
	float fSectorRight = fSectorCentreX + ( SECTORTOOLS_WORLD_WIDTHOFSECTOR / 2.0f );
	float fSectorTop = fSectorCentreY + ( SECTORTOOLS_WORLD_DEPTHOFSECTOR / 2.0f );
	float fSectorBottom = fSectorCentreY - ( SECTORTOOLS_WORLD_DEPTHOFSECTOR / 2.0f );
	if ( ( dummyPos.x < fSectorLeft ) || ( dummyPos.x > fSectorRight ) ||
		 ( dummyPos.y < fSectorBottom ) || ( dummyPos.y > fSectorTop ) )
		 return false;

	return true;
}

#endif // OBJECT_CHECKER_EXPORT

// End of file
