// ===============================================
// renderer/shadows/paraboloidshadows_shared.h
// (c) 2014 Rockstar
// ===============================================

#ifndef _RENDERER_SHADOWS_PARABOLOIDSHADOWS_SHARED_H_
#define _RENDERER_SHADOWS_PARABOLOIDSHADOWS_SHARED_H_

// NOTE: if any of these values change, you need to recompile shaders and C++ code
#define ENABLE_FORWARD_LOCAL_LIGHT_SHADOWING		(1)
#if defined(__SHADERMODEL)
#if __SHADERMODEL < 40  // 
#undef  ENABLE_FORWARD_LOCAL_LIGHT_SHADOWING		
#define ENABLE_FORWARD_LOCAL_LIGHT_SHADOWING		(0)
#endif
#endif

#define LOCAL_SHADOWS_OVERLAP_RENDER_TARGETS		((RSG_DURANGO || RSG_ORBIS) && 1)  // not sure if possible on PCs 
#define LOCAL_SHADOWS_MAX_FORWARD_SHADOWS			(8)  // 2, 4 or 8

#define USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS 		(0)	  //NOTE: Currently we're having problems with the static cache copy on Durango and Orbis, and SM4.0 does not work with cubemap arrays (SM4.1 does though)


#define MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS			(4)		
#define MAX_ACTIVE_LORES_PARABOLOID_SHADOWS			(4)

#if LOCAL_SHADOWS_OVERLAP_RENDER_TARGETS
	#define MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS	(8)		// try to keep this at 8 or more.   (each entry is 3M on Orbis/Durango)
	#define MAX_STATIC_CACHED_LORES_PARABOLOID_SHADOWS	(48)	// best to keep this at 48 or more. (every 4 entries is 3M on Orbis/Durango)
#else
	// on platforms without overlap, we'll be more conservative with memory
	#define MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS	(8)		// 8 is good, but these are very expensive, 6 will work okay. for each drop in hires cache entries, you can add 4 lores entries without changing the memory balance
	#define MAX_STATIC_CACHED_LORES_PARABOLOID_SHADOWS	(32)	// should be at least 32, 40+ would be better each 4 is 6M
#endif

#define MAX_ACTIVE_LORES_OVERLAPPING				(MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS)		// first MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS low res active entries overlap the hires one, so we can use them if the hires are not needed

#define MAX_ACTIVE_PARABOLOID_SHADOWS				(MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS+MAX_ACTIVE_LORES_PARABOLOID_SHADOWS)	// the active shadow entries

#define MAX_ACTIVE_CACHED_PARABOLOID_SHADOWS		(MAX_ACTIVE_PARABOLOID_SHADOWS+MAX_ACTIVE_LORES_OVERLAPPING)				 // active cache entries (some low overlap some high and cannot be used simultaneously)
#define MAX_STATIC_CACHED_PARABOLOID_SHADOWS		((MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS+MAX_STATIC_CACHED_LORES_PARABOLOID_SHADOWS))   

#define SHADOW_CACHE_ACTIVE_STATIC_UPDATE_RESERVE	(1)		// the number of active slots we reserve for refreshing static cache entries. this should be at least 1, to prevent scene with many active lights from starving the cache and causing popping of distant lights since they have not even static shadow

#define MAX_CACHED_PARABOLOID_SHADOWS				(MAX_ACTIVE_CACHED_PARABOLOID_SHADOWS + MAX_STATIC_CACHED_PARABOLOID_SHADOWS)  

#endif // _RENDERER_SHADOWS_PARABOLOIDSHADOWS_SHARED_H_
