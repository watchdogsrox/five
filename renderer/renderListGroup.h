// Title	:	RenderListGroup.h
// Authors	:	John Whyte, Alessandro Piva
// Started	:	31/07/2006
#ifndef RENDERLISTGROUP_H
#define	RENDERLISTGROUP_H	1

#include "fwrenderer/renderlistgroup.h"

////////////////////////////////
// Constants and enumerations //
////////////////////////////////

enum {
	MAX_NUM_RENDER_PHASES = 24,			// assuming a max of 24 renderphases - careful here because we need to store 1 bit per phase for lots of vis stuff
};

enum RenderPassId
{
	RPASS_VISIBLE,		//opaque
	RPASS_LOD,			//opaque
	RPASS_CUTOUT,
	RPASS_DECAL,
	RPASS_FADING,		//now includes screen door and alpha fade
	RPASS_ALPHA, 
	RPASS_WATER,
	RPASS_TREE,
	RPASS_NUM_RENDER_PASSES
};

enum RenderPhaseType
{
	RPTYPE_SHADOW,
	RPTYPE_REFLECTION,
	RPTYPE_MAIN,		// For GBuf phase and the like
	RPTYPE_STREAMING,
	RPTYPE_HEIGHTMAP,
	RPTYPE_DEBUG,
	RPTYPE_NUM_RENDER_PHASE_TYPES
};

class CGtaRenderListGroup
{
public:
	static void Init(unsigned initMode);

	// Use this version if you are adding entities during the sort job - they will be added to an intermediate list and then added to the
	// render list once the sort job completes
	static void DeferredAddEntity(int list, fwEntity* pEntity, u32 subphaseVisFlags, float sortVal, fwRenderPassId id, u16 flags);
	static void ProcessDeferredAdditions();
};

#endif // defined RENDERLISTGROUP_H
