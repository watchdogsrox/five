//
// filename:	SectorTools.cpp
// author:		David Muir <david.muir@rockstarnorth.com>
// date:		21 June 2007
// description:
//



// --- Include Files ----------------------------------------------------------------------

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "atl/array.h"

// Game headers
#include "core/game.h"
#include "SectorTools.h"
#include "Peds/Ped.h"
#include "scene/world/gameWorld.h"
#include "tools/ObjectPositionChecker.h"
#include "tools/StreamingMemJunctionStats.h"
#include "tools/StreamingMemUsage.h"
#include "tools/PerformanceStats.h"

#if (SECTOR_TOOLS_EXPORT)

// Platform SDK headers
#if __XENON
#include "system/xtl.h"
#elif __PPU
#include <time.h>
#endif // Platform SDK headers

// --- Defines ----------------------------------------------------------------------------

PARAM( sectortools, "Start game with the Sector Tools enabled." );
PARAM( midnight, "Run automatic stats tools at midnight." );
PARAM( runmem, "Automatically start memory statistics tool." );
PARAM( runjuncmem, "Automatically start junction memory statistics tool." );
PARAM( runperf, "Automatically start performance statistics tool." );
PARAM( sx, "Start x world coordinate." );
PARAM( sy, "Start y world coordinate." );
PARAM( ex, "End x world coordinate." );
PARAM( ey, "End y world coordinate." );
PARAM( rx, "Sector X coordinate to resume." );
PARAM( ry, "Sector Y coordinate to resume." );
PARAM( statsip, "Pass PS3 IP for stats hack." );
PARAM( halfpop, "Half population density" );
PARAM( usedate, "Specify the date to use as opposed to the system date. For use as the network folder to dump stats." );
PARAM( buildlabel, "Specify the label that was fetched in Perforce used to build the code." );
PARAM( builddate, "Specify the date the build was made." );
PARAM( buildversion, "Specify version(s) of what is running." );
PARAM( buildlaunchtime, "The time the game was launched." );
PARAM( sectordimx, "Sector x dimension." );
PARAM( sectordimy, "Sector y dimension." );
PARAM( sectortools_input, "Sector tools input file");
PARAM( sectortools_output, "Sector tools output file");
PARAM( sectortools_entities, "Sector tools entity restriction file");

PARAM( juncstart, "Junction # to start with" );
PARAM( juncend, "Junction # to end on" );
PARAM( juncsrc, "Junction source csv filename" );
PARAM( juncdst, "Junction destination csv filename" );
PARAM( junccap, "Junction capture type ( simple or spin )" );
PARAM( juncstartupspin, "Junction number of startup spins" );
PARAM( juncsettlespin, "Junction number of settle spins" );
PARAM( juncsettimeeachframe, "Junction smoke - sets the time every frame to 12:00");
PARAM( juncprecapdelay, "Junction smoke - precapture delay in seconds");
PARAM( juncspinrate, "Junction smoke - rate of spin per frame in radians");
PARAM( junccosts, "Junction scene cost folder - place to save cost tracker logs");


#define PHYSICS_SETTLE_TIME		( 6L )		// Physics settle delay time (seconds)
#define OBJECT_LOAD_TIME		( 8L )		// Object load time (seconds)

// --- Constants --------------------------------------------------------------------------

// --- Structure/Class Definitions --------------------------------------------------------

// --- Globals ----------------------------------------------------------------------------

// --- Static Globals ---------------------------------------------------------------------

// --- Static class members ---------------------------------------------------------------

bool					CSectorTools::m_bEnabled = false;
bool					CSectorTools::m_bMidnight = false;
u32						CSectorTools::m_msSettleDelay = ( 1000L * PHYSICS_SETTLE_TIME );
u32						CSectorTools::m_msStreamDelay = ( 1000L * OBJECT_LOAD_TIME );
Vector2					CSectorTools::m_vSectorCoords;
Vector3					CSectorTools::m_vStartCoords;
Vector3					CSectorTools::m_vEndCoords;
char					CSectorTools::m_sStreamDelay[MAX_SECTOR_COORDS]; // deliberate test of bin stats
char					CSectorTools::m_sSettleDelay[MAX_SECTOR_COORDS];
char					CSectorTools::m_sSectorCoords[MAX_SECTOR_COORDS];
char					CSectorTools::m_sStartCoords[MAX_SECTOR_COORDS];
char					CSectorTools::m_sEndCoords[MAX_SECTOR_COORDS];
char					CSectorTools::m_sIp[MAX_IP_LENGTH];
Vector2					CSectorTools::m_sectorDim;

// --- Code -------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------
// Public Methods
//-----------------------------------------------------------------------------------------

void
CSectorTools::Init(unsigned initMode)
{	
    if(initMode == INIT_CORE)
    {
	    // Initialise data
	    m_bEnabled = PARAM_sectortools.Get();

	    m_vStartCoords = Vector3( 0.0f, 0.0f, 0.0f );
	    m_vEndCoords = Vector3( 0.0f, 0.0f, 0.0f );

		m_sectorDim.x = 50.0f;
		m_sectorDim.y = 50.0f;

	    // Initialise our coordinates if we were given any on the command-line
	    SetupWorldCoordinates( );
	    SetupIp( );

	    // Initialise sub-tools
	    CObjectPositionChecker::Init( );
	    CStreamingMemUsage::Init( );
	    CPerformanceStats::Init( );
		CStreamingMemJunctionStats::Init( );

	    m_bMidnight = PARAM_midnight.Get();

	    Assertf( !( PARAM_runmem.Get() && PARAM_runperf.Get() ), "Cannot specify runperf and runmem" );
	    if ( PARAM_runmem.Get() && PARAM_sectortools.Get() )
		    CStreamingMemUsage::ScanAndExportArea( );
	    else if ( PARAM_runperf.Get() && PARAM_sectortools.Get() )
		    CPerformanceStats::ScanAndExportArea( );
    }
}

void
CSectorTools::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		CStreamingMemJunctionStats::Shutdown( );
	    CStreamingMemUsage::Shutdown( );
	    CObjectPositionChecker::Shutdown( );
	    CPerformanceStats::Shutdown( );
    }
}

void
CSectorTools::Run( )
{
	if (!m_bEnabled)
		return;

	Assertf( !( CObjectPositionChecker::GetEnabled() && CPerformanceStats::GetEnabled( ) ),
			   "Cannot have two Sector Tools running at once." );
	Assertf( !( CObjectPositionChecker::GetEnabled() && CStreamingMemUsage::GetEnabled( ) ),
			   "Cannot have two Sector Tools running at once." );
	Assertf( !( CPerformanceStats::GetEnabled() && CStreamingMemUsage::GetEnabled( ) ),
			   "Cannot have two Sector Tools running at once." );

	// DW - I'm sure the player has been somehow killed whilst under water, leading a whole load o' shite to deal with!
	CPlayerInfo::ms_bDebugPlayerInvincible = true;

	if ( CObjectPositionChecker::GetEnabled( ) )
		CObjectPositionChecker::Run( );
	else if ( CStreamingMemUsage::GetEnabled( ) )
		CStreamingMemUsage::Run( );
	else if ( CPerformanceStats::GetEnabled( ) )
		CPerformanceStats::Run( );
	else if ( CStreamingMemJunctionStats::GetEnabled( ) )
		CStreamingMemJunctionStats::Run( );
}

void
CSectorTools::Update( )
{
	if (!m_bEnabled)
		return;

	Assertf( !( CObjectPositionChecker::GetEnabled() && CPerformanceStats::GetEnabled( ) ),
		"Cannot have two Sector Tools running at once." );
	Assertf( !( CObjectPositionChecker::GetEnabled() && CStreamingMemUsage::GetEnabled( ) ),
		"Cannot have two Sector Tools running at once." );
	Assertf( !( CPerformanceStats::GetEnabled() && CStreamingMemUsage::GetEnabled( ) ),
		"Cannot have two Sector Tools running at once." );

	// DW - I'm sure the player has been somehow killed whilst under water, leading a whole load o' shite to deal with!
	CPlayerInfo::ms_bDebugPlayerInvincible = true;

	if ( CObjectPositionChecker::GetEnabled( ) )
		CObjectPositionChecker::Update( );
	else if ( CStreamingMemUsage::GetEnabled( ) )
		CStreamingMemUsage::Update( );
	else if ( CPerformanceStats::GetEnabled( ) )
		CPerformanceStats::Update( );
	else if ( CStreamingMemJunctionStats::GetEnabled( ) )
		CStreamingMemJunctionStats::Update( );	
}

void
CSectorTools::SetupWorldCoordinates( )
{
	if ( PARAM_sx.IsInitialized() )
		PARAM_sx.Get( m_vStartCoords.x );

	if ( PARAM_sy.IsInitialized() )
		PARAM_sy.Get( m_vStartCoords.y );

	if ( PARAM_ex.IsInitialized() )
		PARAM_ex.Get( m_vEndCoords.x );

	if ( PARAM_ey.IsInitialized() )
		PARAM_ey.Get( m_vEndCoords.y );

	if ( PARAM_rx.IsInitialized() )
		PARAM_rx.Get( m_vSectorCoords.x );

	if ( PARAM_ry.IsInitialized() )
		PARAM_ry.Get( m_vSectorCoords.y );

	if ( PARAM_sectordimx.IsInitialized() )
		PARAM_sectordimx.Get( m_sectorDim.x );

	if ( PARAM_sectordimy.IsInitialized() )
		PARAM_sectordimy.Get( m_sectorDim.y );
}

void
CSectorTools::SetupIp( )
{
	const char* sIp = NULL;
	if ( PARAM_statsip.IsInitialized() )
	{
		if ( PARAM_statsip.Get( sIp ) )
		{
			formatf(m_sIp, NELEM(m_sIp) - 1, sIp);
		}
	}
}

#if __BANK
void
CSectorTools::InitWidgets( )
{
	if (!m_bEnabled)
		return;

	sprintf( m_sStreamDelay, "%d", m_msStreamDelay );
	sprintf( m_sSectorCoords, "%d %d", (int)m_vSectorCoords.x, (int)m_vSectorCoords.y );
	sprintf( m_sSettleDelay, "%d", m_msSettleDelay );
	sprintf( m_sStartCoords, "%.2f %.2f %.2f", m_vStartCoords.x, m_vStartCoords.y, m_vStartCoords.z );
	sprintf( m_sEndCoords, "%.2f %.2f %.2f", m_vEndCoords.x, m_vEndCoords.y, m_vEndCoords.z );

	// Find or create widget bank
	bkBank* pWidgetBank = BANKMGR.FindBank( "Sector Tools" );
	if (AssertVerify(!pWidgetBank ))
		pWidgetBank = &BANKMGR.CreateBank( "Sector Tools" );

	pWidgetBank->AddToggle( "Run at Midnight", &m_bMidnight, NullCB, "" );
	pWidgetBank->AddText( "Object Stream Delay (ms):", m_sStreamDelay, MAX_SECTOR_COORDS, false, datCallback(CFA(UpdateStreamDelay)) );
	pWidgetBank->AddText( "Physics Settle Delay (ms):", m_sSettleDelay, MAX_SECTOR_COORDS, false, datCallback(CFA(UpdateSettleDelay)) );
	pWidgetBank->AddText( "Select sector X Y to Scan:", m_sSectorCoords, MAX_SECTOR_COORDS, false, datCallback(CFA(UpdateSectorCoords)) );
	pWidgetBank->AddText( "Start X Y Z World Coordinates:", m_sStartCoords, MAX_SECTOR_COORDS, false, datCallback(CFA(UpdateStartCoords)) );
	pWidgetBank->AddText( "End X Y Z World Coordinates:", m_sEndCoords, MAX_SECTOR_COORDS, false, datCallback(CFA(UpdateEndCoords)) );

	// Initialise sub-tools
	CObjectPositionChecker::InitWidgets( );
	CStreamingMemUsage::InitWidgets( );
	CPerformanceStats::InitWidgets( );
	CStreamingMemJunctionStats::InitWidgets( );
}
#endif // __BANK

bool
CSectorTools::GetMidnight( )
{
	return m_bMidnight;
}

u32
CSectorTools::GetStreamDelay( )
{
	UpdateStreamDelay( );
	return m_msStreamDelay;
}

u32
CSectorTools::GetSettleDelay( )
{
	UpdateSettleDelay( );
	return m_msSettleDelay;
}

u32
CSectorTools::GetNavmeshLoadDelay( )
{
	return SECTORTOOLS_NAVMESH_DELAY;
}

const rage::Vector2&
CSectorTools::GetSector( )
{
	UpdateSectorCoords( );
	return m_vSectorCoords;
}

const rage::Vector3&
CSectorTools::GetWorldStart( )
{
	UpdateStartCoords( );
	return m_vStartCoords;
}

const rage::Vector3&
CSectorTools::GetWorldEnd( )
{
	UpdateEndCoords( );
	return m_vEndCoords;
}

bool
CSectorTools::VecWithinCurrentSector( const rage::Vector3& pos, int nX, int nY )
{
	float fX = SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE( nX );
	float fY = SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE( nY );
	float fLeft = fX - ( SECTORTOOLS_WORLD_WIDTHOFSECTOR / 2.0f );
	float fRight = fX + ( SECTORTOOLS_WORLD_WIDTHOFSECTOR / 2.0f );
	float fTop = fY + ( SECTORTOOLS_WORLD_DEPTHOFSECTOR / 2.0f );
	float fBottom = fY - ( SECTORTOOLS_WORLD_DEPTHOFSECTOR / 2.0f );

	return ( ( fLeft <= pos.x ) && ( fRight >= pos.x ) &&
		     ( fBottom <= pos.y ) && ( fTop >= pos.y ) );
}

//-----------------------------------------------------------------------------------------
// Private Methods
//-----------------------------------------------------------------------------------------

void
CSectorTools::UpdateStreamDelay( )
{
	sscanf( m_sStreamDelay, "%d", &m_msStreamDelay );
}

void
CSectorTools::UpdateSettleDelay( )
{
	sscanf( m_sSettleDelay, "%d", &m_msSettleDelay );
}

void
CSectorTools::UpdateSectorCoords( )
{
	sscanf( m_sSectorCoords, "%f %f", &m_vSectorCoords.x, &m_vSectorCoords.y );
}

void
CSectorTools::UpdateStartCoords( )
{
	sscanf( m_sStartCoords, "%f %f %f", &m_vStartCoords.x, &m_vStartCoords.y, &m_vStartCoords.z );
}

void
CSectorTools::UpdateEndCoords( )
{
	sscanf( m_sEndCoords, "%f %f %f", &m_vEndCoords.x, &m_vEndCoords.y, &m_vEndCoords.z );
}

char* CSectorTools::GetCommandLines(char *commandLine)
{
	if (commandLine)
	{
		strcpy(commandLine,"");
		for (s32 i=0;i<sysParam::GetArgCount();i++)
		{
			strcat(commandLine,sysParam::GetArg(i));
			strcat(commandLine," ");
		}
	}
	return commandLine;
}

bool CSectorTools::IsFinished()
{
	return false;
}

const char* CSectorTools::GetTTY(int UNUSED_PARAM(numLines))
{
	return ("DW: To do provide a way of pumping out last #n lines of TTY\nNeeds to support multline too.\nNeeds to support multline too.");
}

void CSectorTools::WriteCommon(fiStream* fpOutputFile, int sectorNum, char* startTime, char* description, char* result, int nX, int nY, Vector3& vSectorCentre, Vector3& vPlayerPos)
{
	if (!fpOutputFile)
		return;

	char endTime[100];
	char sPlatform[100];
	char commandLine[2048];
	const char* sBuildLabel = NULL;
	const char* sBuildDate = NULL;
	const char* sBuildVersion = CDebug::GetVersionNumber();
	const char* sBuildLaunchTime = NULL;

	sprintf(endTime,"%d",fwTimer::GetTimeInMilliseconds());

	XPARAM(buildlabel);
	XPARAM(builddate);
	XPARAM(buildlaunchtime);
	XPARAM(buildversion);

	PARAM_buildlabel.Get( sBuildLabel );
	PARAM_builddate.Get( sBuildDate );
	PARAM_buildversion.Get( sBuildVersion );
	PARAM_buildlaunchtime.Get( sBuildLaunchTime );

 	fprintf( fpOutputFile, "<?xml version = \"1.0\"?>\n" );

	fprintf( fpOutputFile, "<sector x=\"%d\" y=\"%d\">\n" ,nX, nY);

	// DW: this data doesn't change across sectors in capture
	// deliberate decision to do this as it should reduce complexity of development on the web app much easier.
	fprintf( fpOutputFile,	"\t<build exe=\"%s\" label=\"%s\" date=\"%s\" version=\"%s\" launchtime=\"%s\" />\n",
							sysParam::GetProgramName(),
							sBuildLabel,
							sBuildDate,
							sBuildVersion,
							sBuildLaunchTime );

	// DW: same goes for this too ( see above )

#if __PPU
	strcpy(sPlatform,"ps3");
#elif __XENON
	strcpy(sPlatform,"xbox360");
#else
	strcpy(sPlatform,"win32");
#endif

	fprintf( fpOutputFile,	"\t<machine ip=\"%s\" platform=\"%s\" commandline=\"%s\"/>\n",GetIp(), sPlatform, GetCommandLines(commandLine) );

	fprintf( fpOutputFile,	"\t<capture starttime=\"%s\" endtime=\"%s\" sector_num=\"%d\" desc=\"%s\" result=\"%s\" tty=\"%s\"/>\n",
							startTime,
							endTime,
							sectorNum,
							description,
							result,
							GetTTY(10)
							);

	fprintf( fpOutputFile, "\t<centre x=\"%f\" y=\"%f\" z=\"%f\" />\n", vSectorCentre.x, vSectorCentre.y, vSectorCentre.z );
	fprintf( fpOutputFile, "\t<min x=\"%f\" y=\"%f\" />\n", SECTORTOOLS_WORLD_SECTORTOWORLDX_MIN(nX), SECTORTOOLS_WORLD_SECTORTOWORLDY_MIN(nY) );
	fprintf( fpOutputFile, "\t<max x=\"%f\" y=\"%f\" />\n", SECTORTOOLS_WORLD_SECTORTOWORLDX_MAX(nX), SECTORTOOLS_WORLD_SECTORTOWORLDY_MAX(nY) );
	fprintf( fpOutputFile, "\t<player x=\"%f\" y=\"%f\" z=\"%f\" />\n", vPlayerPos.x, vPlayerPos.y, vPlayerPos.z );
}


#endif // SECTOR_TOOLS_EXPORT

// End of file
