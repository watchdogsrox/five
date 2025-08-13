#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
	#define USE_UI_TECHNIQUES
#endif

#define USE_BATCH_INSTANCING

//Hack to make batch shader work! To reduce drawlist allocations & additional CS invocations, batch system limits draws to cutout bucket.
#if  !__MAX
property int __rage_drawbucket = 3;
#endif

#include "../normal_spec.fx"
