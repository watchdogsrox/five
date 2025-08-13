/*
PathServer_SPU
This will contain ps3 spu code for pathfinding, when I get a chance to look at this.
99% of the pathfinding time is spent in a fairly simple loop in CPathServer::FindPath(),
and should be fairly easy to port to spu.  Memory is randomly accessed across approx 2mb
so the only real option is to use a tailored local-store & mem-manager, DMA'ing required
data when needed - or maybe a little before.
*/

/*
#ifndef PATHSERVER_SPU_H
#define PATHSERVER_SPU_H

// Tasks should only depend on taskheader.h, not task.h (which is the dispatching system)
#include "system/taskheader.h"

DECLARE_TASK_INTERFACE(PathServer_FindPathSPU);

#endif
*/
