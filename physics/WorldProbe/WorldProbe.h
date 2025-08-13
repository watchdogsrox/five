#ifndef WORLD_PROBE_H
#define WORLD_PROBE_H

// To use any of WorldProbe's functionality, just include this header.

#include "physics/WorldProbe/wpcommon.h"

//#include "physics/WorldProbe/shapetestmgr.h"
#if !__SPU
#include "physics/WorldProbe/debug.h"
#endif // !__SPU

#include "physics/WorldProbe/cullvolumeboxdesc.h"
#include "physics/WorldProbe/cullvolumecapsuledesc.h"

#include "physics/WorldProbe/shapetestprobedesc.h"
#include "physics/WorldProbe/shapetestspheredesc.h"
#include "physics/WorldProbe/shapetestcapsuledesc.h"
#include "physics/WorldProbe/shapetestbatchdesc.h"
#include "physics/WorldProbe/shapetestbounddesc.h"
#include "physics/WorldProbe/shapetestboundingboxdesc.h"

#include "physics/WorldProbe/shapetestresults.h"

#include "physics/WorldProbe/shapetest.h"
//#include "physics/WorldProbe/shapetesttask.h"

#endif // WORLD_PROBE_H
