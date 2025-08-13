//
// filename:	ObjectPositionChecker.h
// author:		David Muir <david.muir@rockstarnorth.com>
// date:		21 June 2007
// description:	
//

#ifndef INC_OBJECTPOSITIONCHECKER_H_
#define INC_OBJECTPOSITIONCHECKER_H_

// Game headers
#include "SectorToolsParam.h"
#include "scene/world/gameWorld.h"

#if (SECTOR_TOOLS_EXPORT)

// --- Include Files ----------------------------------------------------------------------

// Game headers
#include "tools/SectorTools.h"

// --- Defines ----------------------------------------------------------------------------

// --- Constants --------------------------------------------------------------------------

// --- Structure/Class Definitions --------------------------------------------------------

// Rage ATL forward declarations
namespace rage
{
	template<class _Type,int _Align,class _CounterType> class atArray;
	class atString;
	class Vector3;
}

/**
 * @brief Physics-based Object Position Checker
 */
class CObjectPositionChecker
{
public:
	static void		Init( );
	static void		Shutdown( );

	static void		Run( );
	static void		Update( );

	static bool		GetEnabled( ) { return m_bEnabled; }

	static void		ScanAndExportSpecificSector( );
	static void		ScanAndExportArea( );
	static void		AbortScan( );

#if __BANK
	static void		InitWidgets( );
#endif

private:
	/**
	 * Mode enumeration
	 */
	enum etMode
	{
		kDisabled,				//!< Default disabled mode
		kExportSpecific,		//!< Export single sector
		kExportArea				//!< Export area
	};

	/**
	 * Pass enumeration
	 */
	enum etPass
	{
		kObjectLoad,			//!< Object-load pass (used to prevent empty sectors taking time)
		kPreSettle,				//!< Pre-physics settle pass
		kPostSettle				//!< Post-physics settle pass
	};

	struct Coord
	{
		float sx;
		float sy;
		float ex;
		float ey;
	};

	static void		GetAutomaticCoords( );
	static void		SetMode( etMode eMode );
	static void		SetEnabled( bool bEnable ) { m_bEnabled = bEnable; }
	static void		SetScanInProgress( bool bInProgress ) { m_bScanInProgress = bInProgress; }
	static void		MoveToNextSector( );
	static void		SetUserOutputFolder( );	
	static void		SetEntitiesFile( );	
	static void		GetEntityGuids( );

	static void		UpdateState( );

	static void		WriteLog( const rage::atArray<rage::atString>& vObjectNames, 
							  const rage::atArray<rage::Matrix34>& vObjectMatInitial,
							  const rage::atArray<rage::Matrix34>& vObjectMatFinal,
							  int nPoolObjects );

	static bool		IsInterestingObject( CObject* pObject );

	//-------------------------------------------------------------------------------------
	// State
	//-------------------------------------------------------------------------------------
	static atArray<Coord>	m_atCoords;
	static bool				m_bEnabled;
	static bool				m_bAutomatic;
	static bool				m_bMoreAutoCoordsToDo;
	static etMode			m_eMode;
	static etPass			m_ePass;
	static char				m_sUserOutputDirectory[MAX_OUTPUT_PATH];
	static atString			m_sInputFile;
	static bool				m_bRestrictToSpecificEntities;
	static char				m_sEntityRestrictionFile[MAX_OUTPUT_PATH];
	static atArray<u32>		m_atEntityGuids;
	static bool				m_bScanInProgress;

	//-------------------------------------------------------------------------------------
	// State Tracking
	//-------------------------------------------------------------------------------------

	static u32	m_msTimer;			//!< Timer used to move between states

	//-------------------------------------------------------------------------------------
	// Position Tracking
	//-------------------------------------------------------------------------------------

	static int		m_nCurrentSectorX;	//!< Current sector x being checked
	static int		m_nCurrentSectorY;	//!< Current sector y being checked
	static int		m_nStartSectorX;	//!< Start sector x being checked
	static int		m_nStartSectorY;	//!< Start sector y being checked
	static int		m_nEndSectorX;		//!< End sector x being checked
	static int		m_nEndSectorY;		//!< End sector y being checked
};

// --- Globals ----------------------------------------------------------------------------

#endif // SECTOR_TOOLS_EXPORT

#endif // !INC_OBJECTPOSITIONCHECKER_H_

// End of file
