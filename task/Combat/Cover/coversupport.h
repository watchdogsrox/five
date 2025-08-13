//
// task/Combat/Cover/coversupport.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_COMBAT_COVER_COVERSUPPORT_H
#define TASK_COMBAT_COVER_COVERSUPPORT_H

#include "vectormath/classes.h"

//-----------------------------------------------------------------------------

namespace CCoverSupport
{
	bool HemisphereIntersectsAABB(Vec3V_In hemisphereCenterV, ScalarV_In hemisphereRadiusV,
			Vec3V_In planeNormalV, Vec3V_In boxMinWorldV, Vec3V_In boxMaxWorldV);

	bool SphereIntersectsAABB(Vec3V_In sphereCenterV, ScalarV_In sphereRadiusV,
			Vec3V_In boxMinWorldV, Vec3V_In boxMaxWorldV);

}	// namespace CCoverSupport

//-----------------------------------------------------------------------------

#endif	// TASK_COMBAT_COVER_COVERSUPPORT_H

// End of file 'task/Combat/Cover/coversupport.h'

