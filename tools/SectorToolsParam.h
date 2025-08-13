//
// filename:	SectorToolsParam.h
// author:		David Muir <david.muir@rockstarnorth.com>
// date:		21 June 2007
// description:	PARAM extern variable for all Sector Tools
//

#ifndef INC_OBJECTCHECKERPARAM_H_
#define INC_OBJECTCHECKERPARAM_H_

// --- Include Files ------------------------------------------------------------

// Rage headers
#include "system/param.h"

// --- Defines ------------------------------------------------------------------

#define SECTOR_TOOLS_EXPORT		(!__FINAL)

// --- Globals ------------------------------------------------------------------

#if (SECTOR_TOOLS_EXPORT)
	XPARAM( sectortools );
	XPARAM( midnight );
	XPARAM( runmem );
	XPARAM( runperf );
	XPARAM( sx );
	XPARAM( sy );
	XPARAM( ex );
	XPARAM( ey );
	XPARAM( rx );
	XPARAM( ry );
	XPARAM( statsip );
	XPARAM( halfpop );
	XPARAM( level );
	XPARAM( sectortools_input );
	XPARAM( sectortools_output );
	XPARAM( sectortools_entities );

	XPARAM( runjuncmem );
	XPARAM( juncstart );
	XPARAM( juncend );
	XPARAM( juncsrc );
	XPARAM( juncdst );
	XPARAM( junccap );
	XPARAM( juncstartupspin );
	XPARAM( juncsettlespin );
	XPARAM( juncsettimeeachframe );
	XPARAM( juncprecapdelay );
	XPARAM( juncspinrate );
	XPARAM( junccosts );
#endif

#endif // !INC_OBJECTCHECKERPARAM_H_
