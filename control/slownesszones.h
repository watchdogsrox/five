
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    slownesszones.h
// PURPOSE : Contains the zones on the map that slow cars down.
// AUTHOR :  Obbe.
// CREATED : 24/9/07
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SLOWNESSZONES_H_
#define _SLOWNESSZONES_H_

// Forward declarations
namespace rage { class atString; }

// rage includes
#include "bank/bank.h"
#include "spatialdata/aabb.h"

// framework includes

// game includes

#define MAX_NUM_SLOWNESS_ZONES	(86)

/////////////////////////////////////////////////////////////////////////////////
// A class that contains the info for one slowness zone
/////////////////////////////////////////////////////////////////////////////////

class CSlownessZone
{
public:
	spdAABB 		m_bBox;
	PAR_SIMPLE_PARSABLE
};


/////////////////////////////////////////////////////////////////////////////////
// A class to contain the functions.
/////////////////////////////////////////////////////////////////////////////////

class CSlownessZoneManager
{

public:
#if __BANK
	static void ZoneUpdateCB();
	static void CheckCurrentZoneRangeCB();
	static void Save();
	static void AddZone();
	static void DeleteZone();
	static void CentreAroundPlayer();
	static void WarpPlayerToCentre();
	static void InitWidgets(bkBank& bank);

	// Bank build data:
	static bool	m_bDisplayDebugInfo;
	static bool m_bDisplayZonesOnVMap;
	static bool m_bModifySelectedZone;
	static int m_currentZone;
	static Vector3 m_currentZoneMin;
	static Vector3 m_currentZoneMax;
#endif

	// Delete the data
	static void Reset();

	// Load the data
	static void Load();

	static void Update();
	static void Init(unsigned initMode);
	static void Shutdown();

	static bool GetFileName( atString &filename );

	atArray<CSlownessZone>	m_aSlownessZone;
	static CSlownessZoneManager	m_SlownessZonesManagerInstance;

	PAR_SIMPLE_PARSABLE

private:
	static void Reload();	
};



#endif
