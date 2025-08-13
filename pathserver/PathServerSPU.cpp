#if	!(__XENON||__WIN32PC)
#if __SPU  

//#include "PathServerSPU.h"

// GTA Includes
#include "basetypes.h"
#include "renderer\Renderer.h"			// JW- had to move this up here to get a successful compile...
#include "scene/world/gameWorld.h"

// Rage Includes
#include "math/intrinsics.h"
#include "profile/cellspurstrace.h"
#include "system/taskheader.h"
#include "vector/vector3_consts_spu.cpp"

// Framework headers
//#include "fwmaths/Maths.h"

// PS3 Includes
#include <cell/dma.h>
#include <cell/atomic.h>
#include <math.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include <stdint.h>



void PathServer_FindPathSPU(sysTaskParameters &TaskParams)
{

}

#endif	//__SPU
#endif	//!(__XENON||__WIN32PC)



