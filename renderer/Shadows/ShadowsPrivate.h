// =================================
// renderer/shadows/shadowsprivate.h
// (c) 2010 RockstarNorth
// =================================

#ifndef _RENDERER_SHADOWS_SHADOWSPRIVATE_H_
#define _RENDERER_SHADOWS_SHADOWSPRIVATE_H_

// Rage headers
#include "bank/bank.h"
#include "file/asset.h"
#include "math/float16.h"
#include "system/system.h"
#include "system/timemgr.h"
#include "system/xtl.h"
#include "system/param.h"
#include "system/memory.h"
#include "grcore/device.h"
#include "grcore/im.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grcore/viewport.h"
#include "grcore/config.h"
#include "grcore/effect.h"
#include "grcore/effect_config.h"
#include "grcore/effect_values.h"
#include "grcore/quads.h"
#if __PPU
#include "grcore/texturegcm.h"
#endif
#include "rmcore/drawable.h"
#include "grmodel/shader.h"
#include "grmodel/shaderfx.h"
#include "grmodel/shadergroup.h"
#include "grmodel/shadergroupvar.h"
#include "grmodel/grmodelspu.h"
#if __D3D
#include "system/d3d9.h"
#endif


// Game headers
#include "debug/debugglobals.h"
#include "camera/viewports/viewportmanager.h"
#include "shaders/shaderlib.h"
#include "renderer/lights/lights.h"
#include "renderer/rendertargets.h"
#include "renderer/sprite2d.h"
#include "renderer/deferred/deferredlighting.h"
#include "renderer/drawlists/drawlistmgr.h"
#include "renderer/shadows/shadows.h"

// ================================================================================================

#if __XENON

class D3DResolveStructs
{
public:
	D3DPOINT   mPoint;
	float      mPad[2];
	D3DRECT    mRect;
	D3DVECTOR4 mClearColour;

	__forceinline D3DResolveStructs(int x, int y, int w, int h)
	{
		mPoint.x = x;
		mPoint.y = y;

		mRect.x1 = 0;
		mRect.y1 = 0;
		mRect.x2 = w;
		mRect.y2 = h;

		mClearColour.x = 0.0f;
		mClearColour.y = 0.0f;
		mClearColour.z = 0.0f;
		mClearColour.w = 1.0f;
	}
};

#endif // __XENON

struct CShaderFunc
{
#if 0	// these don't compile
	template<class T_arg1> static inline void SetSetShaderGroupVarInContext(grmShaderGroup* pShaderGroup, grmShaderGroupVar var, const T_arg1& arg1)
	{
		Assert(pShaderGroup);

		if (!gDrawListMgr.IsBuildingDrawList()) // execute immediately if called from a render thread
		{
			pShaderGroup->SetVar(var, arg1);
		}
		else // place in current drawlist otherwise
		{
			DLC(T_SetShaderGroupVar_1Arg<T_arg1>, (pShaderGroup, var, arg1));
		}
	}

	template<class T_arg1> static inline void SetSetShaderGroupVarInContext(grmShaderGroup* pShaderGroup, grmShaderGroupVar var, T_arg1* arg1, s32 count)
	{
		Assert(pShaderGroup);

		if (!gDrawListMgr.IsBuildingDrawList()) // execute immediately if called from a render thread
		{
			pShaderGroup->SetVar(var, arg1, count);
		}
		else // place in current drawlist otherwise
		{
			DLC(T_SetShaderGroupVarArray<T_arg1>, (pShaderGroup, var, arg1, count));
		}
	}
#endif
};

__forceinline Matrix44 Matrix44InvertOrtho(const Matrix34& mat) // old and crusty vectormath
{
	Matrix34 inv;
	inv.FastInverse(mat);
	Matrix44 m44;
	Convert(m44, inv);
	return m44;
}

__forceinline void Mat44VInvertOrtho(Mat44V_InOut outMtx, Mat34V_ConstRef inMtx)
{
	Mat34V temp;
	InvertTransformOrtho(temp,inMtx);
	outMtx.SetCols(	Vec4V(temp.GetCol0(),ScalarV(V_ZERO)),
					Vec4V(temp.GetCol1(),ScalarV(V_ZERO)),
					Vec4V(temp.GetCol2(),ScalarV(V_ZERO)),
					Vec4V(temp.GetCol3(),ScalarV(V_ONE)));

}

#endif // _RENDERER_SHADOWS_SHADOWSPRIVATE_H_
