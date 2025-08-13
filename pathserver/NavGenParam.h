#ifndef NAVGENPARAM_H
#define NAVGENPARAM_H

//***********************************************************************************
//	NavGenParam.h
//	This file just holds the PARAM extern variable for the "-navgen" switch
//***********************************************************************************

#include "system\param.h"

#if __SPU
#define NAVMESH_EXPORT	0
#else
#define NAVMESH_EXPORT	(__WIN32PC && !__FINAL)
#endif

#if NAVMESH_EXPORT || defined(NAVGEN_TOOL)

XPARAM(navgen);

#endif	// NAVMESH_EXPORT

#endif	// NAVGENPARAM_H
