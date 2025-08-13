// ==========================
// renderer/shadows/shadows.h
// (c) 2010 RockstarNorth
// ==========================

#ifndef _RENDERER_SHADOWS_SHADOWS_H_
#define _RENDERER_SHADOWS_SHADOWS_H_

#if !__SPU

// Rage headers
#include "grcore/texture.h"
#include "grmodel/shader.h"

#include "renderer/Shadows/ShadowTypes.h"

#define DEBUG_SHADOW_BUFFER_SIZE 0 // 0 = default

// ================================================================================================

class CShadowInfo
{
public:
	enum eShadowFacet 
	{
		SHADOW_FACET_RIGHT = 0,
		SHADOW_FACET_LEFT  = 1,
		SHADOW_FACET_DOWN  = 2,
		SHADOW_FACET_UP    = 3,
		SHADOW_FACET_FRONT = 4,
		SHADOW_FACET_BACK  = 5,
	};


	eLightShadowType m_type;
	float       m_tanFOV;				// used for cone, not cubemap
	float       m_shadowDepthRange;  
	u8			m_facetVisibility;		// bits for each facet's screen visibility
	Mat34V		m_shadowMatrix;
	Mat44V		m_shadowMatrixP[2];		// second projection matrix is for hemisphere lights (side use different projection than down)
	float		m_nearClip;
	float		m_depthBias;
	float		m_slopeScaleDepthBias;
#if __BANK
	void BankAddWidgets(bkBank& bk);
	void BankUpdate(const CShadowInfo& src);
#endif
};

class CShadows // CSysShadows?
{
public:
#if __BANK
	static void AddWidgets(bkBank& bk);
#endif
};

extern CShadows gShadows;

class CLightSource;

// Game headers
#include "renderer/shadows/paraboloidshadows.h"

#endif // !__SPU
#endif // _RENDERER_SHADOWS_SHADOWS_H_
