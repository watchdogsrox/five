#ifndef SHADOW_TYPES_H_
#define SHADOW_TYPES_H_

#define HEMISPHERE_SHADOW_CUTOFF (M_SQRT3)

enum eShadowType // same as eEdgeShadowType
{
	SHADOWTYPE_NONE = 0,
	SHADOWTYPE_CASCADE_SHADOWS,
	SHADOWTYPE_PARABOLOID_SHADOWS,
};

enum eDeferredShadowType // these were being passed around as 0,1,2 ints .. need to give them sensible names
{
	DEFERRED_SHADOWTYPE_NONE=0,
	DEFERRED_SHADOWTYPE_POINT,
	DEFERRED_SHADOWTYPE_HEMISPHERE, 
	DEFERRED_SHADOWTYPE_SPOT, 
};

enum eLightShadowType
{
	SHADOW_TYPE_NONE		= 0, 
	SHADOW_TYPE_POINT		= 1,
	SHADOW_TYPE_HEMISPHERE	= 2,
	SHADOW_TYPE_SPOT		= 3, 
};

__forceinline eDeferredShadowType GetDeferredShadowTypeCommon(eLightShadowType lightShadowType)
{
	switch(lightShadowType)
	{
	case SHADOW_TYPE_POINT: 
		return DEFERRED_SHADOWTYPE_POINT;
	case SHADOW_TYPE_HEMISPHERE: 
		return DEFERRED_SHADOWTYPE_HEMISPHERE;
	case SHADOW_TYPE_SPOT: 
		return DEFERRED_SHADOWTYPE_SPOT;
	default:
		Assertf(0, "GetDeferredShadowTypeCommon got invalid type");
		return DEFERRED_SHADOWTYPE_NONE;// shouldn't get here
	}
}

#endif
