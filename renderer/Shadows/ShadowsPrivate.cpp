// ===================================
// renderer/shadows/shadowsprivate.cpp
// (c) 2010 RockstarNorth
// ===================================

// Game headers
#include "renderer/util/util.h"
#include "renderer/rendertargets.h"
#include "renderer/shadows/shadowsprivate.h"
#include "debug/debugdraw/debugvolume.h"

RENDER_OPTIMISATIONS()

// ================================================================================================

CShadows gShadows;

#if __BANK

void CShadowInfo::BankAddWidgets(bkBank& bk)
{
	bk.AddSlider("Shadow Depth Range", &m_shadowDepthRange,  0.0f, 2048.0f, 1.000f);
}

void CShadowInfo::BankUpdate(const CShadowInfo& src)
{
	m_shadowDepthRange = src.m_shadowDepthRange;
}

__COMMENT(static) void CShadows::AddWidgets(bkBank& bk)
{
	CParaboloidShadow::BankAddWidgets(bk);
}

#endif // __BANK
