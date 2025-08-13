//
// filename:	PerformanceStats.h
// description:	Game Performance measurement tool.
//
// This is not run at the same time as the other sector tools because we might
// want to run this with Peds, Cars and Ambient Scripts etc.  This unfortunately
// duplicates much of the functionality in the CSectorTools class because of
// this and that we need additional internal passes to look in different
// directions at each sector.
//

#ifndef INC_PERFORMANCESTATS_H_
#define INC_PERFORMANCESTATS_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/array.h"
#include "streaming/streamingmodule.h"
// Game headers
#include "tools/SectorTools.h"

#if ( SECTOR_TOOLS_EXPORT )

// --- Defines ------------------------------------------------------------------

#define NUM_DIRECTIONS		(4)
#define NUM_GRC_STATS		(4)
#define NUM_TIMEBAR_STATS	(6)

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// Forward declare Rage ATL classes
namespace rage
{
	template <class _Type, int _Align, class _CounterType> class atArray;
	class atString;
	class fiStream;
}

// Core Forward Declarations
class camFollowPedCamera;

class CPerformanceStats
{
public:
	/**
	 * Performance Stats Mode enumeration
	 */
	enum etMode
	{
		kDisabled,				//!< Default disabled mode
		kExportSpecific,		//!< Export single specified block mode
		kExportArea				//!< Export area
	};

	/**
	 * Performance Stats Pass enumeration
	 */
	enum etPass
	{
		kNavmeshLoad,			//!< Navmesh-load pass
		kObjectLoad,			//!< Object-load pass
		kGameRunning,
		kGamePaused
	};

	static void		Init( );
	static void		Shutdown( );
	static void		SetupWorldCoordinates( );
	static void		Run( );
	static void		UpdateState( );
	static void		Update( );

	static inline etMode GetMode( ) { return m_eMode; }
	static inline etPass GetPass( ) { return m_ePass; }
	static inline bool	GetEnabled( ) { return m_bEnabled; }
	static inline u32 GetTimer( ) { return m_msTimer; }
	static inline u32 GetIntervalTimer( ) { return m_msIntervalTimer; }
	static inline void	SetEnabled( bool bEnable ) { m_bEnabled = bEnable; }

	static void		ScanAndExportSpecificSector( );
	static void		ScanAndExportArea( );
#if __BANK
	static void		InitWidgets( );
#endif
	static inline char*	GetOutputFolder( ) { return m_sOutputDirectory; }

private:
	/**
	 * Performance Stats Direction enumeration
	 */
	enum etDirection
	{
		kNorth	= 0,
		kEast	= 1,
		kSouth	= 2,
		kWest	= 3
	};

	struct TSectorData
	{
	public:
		int nCars;								// Number of cars in memory
		int nPeds;								// Number of peds in memory
		int nDummyCars;							// Number of dummy cars in memory
		int nPhysicalObjects;					// Number of physical objects in memory
		int nDummyObjects;						// Number of dummy objects in memory
		int nActiveObjects;						// Number of active physical objects in memory
		int nProcObjects;						// Number of procedural objects in memory
		int nBuildingObjects;					// Number of building objects in memory
		int nTreeObjects;						// Number of tree objects in memory
		int nProcGrassObjects;					// Number of procedural grass objects
		rage::atArray<atString> vScripts;		// Active scripts in sector

		// Default constructor
		TSectorData( )
		{
			this->Reset( );
		}

		// Reset method utility to reset all arrays
		void Reset( )
		{
			nCars = 0;
			nPeds = 0;
			nDummyCars = 0;
			nDummyObjects = 0;
			nActiveObjects = 0;
			nProcObjects = 0;
			nBuildingObjects = 0;
			nTreeObjects = 0;
			nProcGrassObjects = 0;
			vScripts.Reset( );
		}
	};

	struct TSectorSamples
	{
	public:

		rage::atRangeArray<rage::atRangeArray<rage::atArray<float>, NUM_TIMEBAR_STATS>, NUM_DIRECTIONS>	vTimeBarSamples;
		rage::atRangeArray<rage::atRangeArray<rage::atArray<float>, NUM_GRC_STATS>, NUM_DIRECTIONS> vGrcSamples;
		rage::atRangeArray<rage::atArray<float>, NUM_DIRECTIONS>	vRealFpsSamples;	// 2D array of RealFPS samples		

		// Default constructor
		TSectorSamples( )
		{
			this->Reset( );
		}

		// Reset method utility to reset all arrays
		void Reset( )
		{
			for ( int nDir = 0; nDir < NUM_DIRECTIONS; ++nDir )
			{
				// Reset TimeBar Samples
				for ( int nTimeBar = 0; nTimeBar < NUM_TIMEBAR_STATS; ++nTimeBar )
					this->vTimeBarSamples[nDir][nTimeBar].Reset( );

				// Reset Rage Grc Samples
				for ( int nRage = 0; nRage < NUM_GRC_STATS; ++nRage )
					this->vGrcSamples[nDir][nRage].Reset( );

				// Reset RealFPS Samples
				this->vRealFpsSamples[nDir].Reset( );
			}
		}
	};

	static void		SetMode( etMode eMode );
	static void		SetPlayerDirection( etDirection eDir );
	static void		SetCameraDirection( etDirection eDir );
	static void		SetOutputFolder( );
	static void		MoveToNextSector( );
	static void		TakeScreenshot( etDirection eDir );
	static void		Pause( bool bPause );
	static void		WriteLog( );
	static void		WriteSamples( fiStream* fpStream, const char* sTag, const TSectorSamples& samples );
	static void		SampleDelay( );
	static void		SampleInterval( );
	static bool		VecWithinCurrentSector( const rage::Vector3& pos );
	static void		ReadTimeBarSamples( rage::atRangeArray<rage::atArray<float>, NUM_TIMEBAR_STATS>& samples );
	static void		ReadGrcSamples( rage::atRangeArray<rage::atArray<float>, NUM_GRC_STATS>& samples );

	static float	m_startScanY;		//!< Height at which to cast down to find ground. (start)
	static float	m_endScanY;			//!< Height at which to cast down to find ground. (end)
	static float	m_scanYInc;			//!< Height at which to cast down to find ground. (the increment)
	static float	m_probeYEnd;		//!< Bottom of vertical probe	

	static int		m_nCurrentSectorX;	//!< Current sector x being checked
	static int		m_nCurrentSectorY;	//!< Current sector y being checked
	static int		m_nStartSectorX;	//!< Start sector x being checked
	static int		m_nStartSectorY;	//!< Start sector y being checked
	static int		m_nEndSectorX;		//!< End sector x being checked
	static int		m_nEndSectorY;		//!< End sector y being checked
	static etMode	m_eMode;			//!< Current state
	static etPass	m_ePass;			//!< Current pass
	static etDirection m_eDirection;	//!< Current direction
	static bool		m_bEnabled;			//!< Tool running status
	static u32	m_msTimer;			//!< Timer used to move between states
	static u32	m_msIntervalTimer;	//!< Sample interval timer
	static u32	m_msSampleDelay;	//!< User-defined sample delay
	static u32	m_msSampleInterval;	//!< Time interval between samples
	static char		m_sOutputDirectory[MAX_OUTPUT_PATH];
	static char		m_sScreenshotDirectory[MAX_OUTPUT_PATH];
	static char		m_sSampleDelay[20];
	static char		m_sSampleInterval[20];
	static char		m_vPassNames[4][15];
	static char		m_vFacingNames[NUM_DIRECTIONS][10];

	static TSectorData		m_SectorData;		//!< Per-sector one off data
	static TSectorSamples	m_RunningSamples;	//!< FPS sample data game loop running
	static TSectorSamples	m_PausedSamples;	//!< FPS sample data game paused

	static bool		m_bSectorOK;		//!<

	static char		m_description[SECTOR_TOOLS_MAX_DESC_LEN];
	static int		m_sectorNum;
	static char		m_result[SECTOR_TOOLS_MAX_RESULT_LEN];
	static char		m_startTime[SECTOR_TOOLS_MAX_START_TIME_LEN];

	// Progress file
	static rage::fiStream* m_pProgressFile;	//!< Progress file stream pointer
};

// --- Globals ------------------------------------------------------------------

#endif // SECTOR_TOOLS_EXPORT

#endif // !INC_PERFORMANCESTATS_H_
