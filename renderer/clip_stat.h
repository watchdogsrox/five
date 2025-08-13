#ifndef CLIP_STAT_H
#define CLIP_STAT_H

#if		__DEV
#define	INC_STAT(x) x++
#else
#define	INC_STAT(x) 
#endif

class	CRenderPhase;
struct	CClipStat
{
	public:
	int	clip_early_reject;
	int	clip_outside_frustum;
	int	clip_above_water;
	int	clip_below_water;
	int	clip_occluded;
	int	check_clip;
	int	check_visible;
	int clip_offscreen;
	int clip_stream;
	int clip_stream_but_loaded;
	int clip_visible;
	int	clip_stream_request_count;
	int	clip_calc_bound;
	int	clip_use_cache_bound;
	int	sphere_mistake;
	int check_sphere;
	int	check_fast_accept;
	int	check_fast_reject;
	int check_fast_unknown;
	int	clip_stream_check_visible;

	int	setup_entity;
	int	setup_map_entity;
	int	setup_map_entity_lod;
	int	setup_map_entity_invisible;
	int	setup_map_entity_nostream;
	int	setup_map_entity_ignored;
	int	setup_map_entity_vis_dist;
	int	setup_map_entity_vis_dist_fade;
	int	setup_map_entity_clip1;
	int	setup_map_entity_clip2;
	int	setup_map_entity_stream;
	int	setup_map_entity_vis_notloaded;
	int	nod;  //count of objects that are not a lod :)
	int	lod;  // 
	int	slod; 
	int	clod; // is a nod and a child of a lod
	int	cslod; // is a lod and a child of a slod

	int	VisibleCount;
	int	VisibleLodCount;
	int	VisibleDecalCount;
	int	UnderwaterCount;
	int	InvisibleCount;
	int	AlphaCount;
	int	FadeCount;

	void DumpOutput(bool /*Reset*/ =false,CRenderPhase * /*pRenderPhase*/ =NULL) { /* No Op*/ }
};


#endif

