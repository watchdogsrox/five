//
// filename:	SectorTools.h
// description:	
//

#ifndef INC_OBJECTCHECKER_H_
#define INC_OBJECTCHECKER_H_

// --- Include Files ----------------------------------------------------------------------
#include "file/stream.h"
#include "vector/vector2.h"

// Game headers
#include "SectorToolsParam.h"

#if (SECTOR_TOOLS_EXPORT)

// --- Defines ----------------------------------------------------------------------------

#define MAX_OUTPUT_PATH						(512L)
#define MAX_SECTOR_COORDS					(40L)

#define MAX_IP_LENGTH						(12)
#define SECTORTOOLS_NAVMESH_DELAY			(2000L)

#define SECTORTOOLS_WORLD_WIDTHOFSECTOR		(CSectorTools::GetSectorDimX())
#define SECTORTOOLS_WORLD_DEPTHOFSECTOR		(CSectorTools::GetSectorDimY())

// Mapping Worldspace to Sectorspace (bottom-left)
#define SECTORTOOLS_WORLD_WORLDTOSECTORX(x) ((s32) rage::Floorf(((x) / SECTORTOOLS_WORLD_WIDTHOFSECTOR)))
#define SECTORTOOLS_WORLD_WORLDTOSECTORY(y) ((s32) rage::Floorf(((y) / SECTORTOOLS_WORLD_DEPTHOFSECTOR)))

// Mapping Sectorspace to Worldspace (sector bottom-left)
#define SECTORTOOLS_WORLD_SECTORTOWORLDX_MIN(x)		((x) * SECTORTOOLS_WORLD_WIDTHOFSECTOR)
#define SECTORTOOLS_WORLD_SECTORTOWORLDY_MIN(y)		((y) * SECTORTOOLS_WORLD_DEPTHOFSECTOR)
// Mapping Sectorspace to Worldspace (sector top-right)
#define SECTORTOOLS_WORLD_SECTORTOWORLDX_MAX(x)		(SECTORTOOLS_WORLD_SECTORTOWORLDX_MIN(x) + SECTORTOOLS_WORLD_WIDTHOFSECTOR)
#define SECTORTOOLS_WORLD_SECTORTOWORLDY_MAX(y)		(SECTORTOOLS_WORLD_SECTORTOWORLDY_MIN(y) + SECTORTOOLS_WORLD_DEPTHOFSECTOR)
// Mapping Worldspace to Sectorspace (sector centre points)
#define SECTORTOOLS_WORLD_SECTORTOWORLDX_CENTRE(x)	(SECTORTOOLS_WORLD_SECTORTOWORLDX_MIN(x) + (SECTORTOOLS_WORLD_WIDTHOFSECTOR / 2.0f))
#define SECTORTOOLS_WORLD_SECTORTOWORLDY_CENTRE(y)	(SECTORTOOLS_WORLD_SECTORTOWORLDY_MIN(y) + (SECTORTOOLS_WORLD_DEPTHOFSECTOR / 2.0f))

#define SECTOR_TOOLS_MAX_DESC_LEN			(4096L)
#define SECTOR_TOOLS_MAX_RESULT_LEN			(100L)
#define SECTOR_TOOLS_MAX_START_TIME_LEN		(100L)

// --- Structure/Class Definitions --------------------------------------------------------

/**
* @brief Parent Object Checker
*/
class CSectorTools
{
public:
	static void						Init(unsigned initMode);
	static void						Shutdown(unsigned shutdownMode);

	static void						Run( );
	static void						Update( );

	static void						SetupWorldCoordinates( );
	static void						SetupIp( );

	static void						SetEnabled( bool set ) { m_bEnabled = set; }
	static bool						GetEnabled( ) { return m_bEnabled; }
	static bool						GetMidnight( );
	static u32					GetStreamDelay( );
	static u32					GetSettleDelay( );
	static u32					GetNavmeshLoadDelay( );
	static const rage::Vector2&		GetSector( );
	static const rage::Vector3&		GetWorldStart( );
	static const rage::Vector3&		GetWorldEnd( );

	static char*					GetIp( ) { return m_sIp; }

	static const char*				GetTTY(int numLines);
	static char*					GetCommandLines(char *commandLine);

	// Utility Methods (for use by other Object Checkers)
	static bool						VecWithinCurrentSector( const rage::Vector3& pos, int nX, int nY );

	static void						WriteCommon(fiStream* fpOutputFile, int sectorNum, char* startTime, char* description, char* result, int nx, int ny, Vector3& vSectorCentre, Vector3& vPlayerPos);

	static float					GetSectorDimX() { return m_sectorDim.x; }
	static float					GetSectorDimY() { return m_sectorDim.y; }

	static bool						IsFinished();

#if __BANK
	static void						InitWidgets();
#endif

private:

	//----------------------------------------------------------------------------------------
	// Sector Tools : RAG UI Data Refresh
	//----------------------------------------------------------------------------------------

	static void						UpdateStreamDelay( );
	static void						UpdateSettleDelay( );
	static void						UpdateSectorCoords( );
	static void						UpdateStartCoords( );
	static void						UpdateEndCoords( );

	//----------------------------------------------------------------------------------------
	// Sector Tools : State
	//----------------------------------------------------------------------------------------

	static bool			m_bEnabled;

	//----------------------------------------------------------------------------------------
	// Sector Tools : RAG UI Configuration Data
	//----------------------------------------------------------------------------------------

	static bool			m_bMidnight;
	static u32		m_msSettleDelay;	//!< Physics settle period
	static u32		m_msStreamDelay;	//!< Object streaming settle period
	static u32		m_msNavmeshLoadDelay;

	// Sector / World coordinate storage
	static rage::Vector2 m_vSectorCoords;	//!< Single sector coordinates
	static rage::Vector3 m_vStartCoords;	//!< Start group coordinates
	static rage::Vector3 m_vEndCoords;		//!< End group coordinates
	static rage::Vector2 m_sectorDim;		//!< Sector dimensions	

	// RAG Widget string storage
	static char			m_sStreamDelay[MAX_SECTOR_COORDS];
	static char			m_sSettleDelay[MAX_SECTOR_COORDS];	
	static char			m_sSectorCoords[MAX_SECTOR_COORDS];
	static char			m_sStartCoords[MAX_SECTOR_COORDS];
	static char			m_sEndCoords[MAX_SECTOR_COORDS];

	// PS3 IP storage
	static char			m_sIp[MAX_IP_LENGTH];
};

#endif // SECTOR_TOOLS_EXPORT

#endif // !INC_OBJECTCHECKER_H_
