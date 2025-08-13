#ifndef __SHADER_UTIL_H__
#define __SHADER_UTIL_H__

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage includes
#include "grcore/effect_typedefs.h"
#include "grcore/quads.h"
#include "grcore/texture.h"
#include "camera/viewports/Viewport.h"

#include "renderer/util/shmoofile.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //
namespace rage
{
	class grmShader;
	class grcViewport;
}

// =============================================================================================== //
// CLASS
// =============================================================================================== //

class ShaderUtil
{
public:

	static bool StartShader(const char *name, const grmShader *shader, const grcEffectTechnique tech, const u32 pass, bool addPixLabel=true, int shmooIdx=-1);
	static void EndShader(const grmShader *shader, bool addPixLabel=true, int shmooIdx=-1);

	__forceinline static Vector4 CalculateProjectionParams(const grcViewport *vp = NULL)
	{
		// Set projection parameters
		const grcViewport *viewport = (vp == NULL) ? grcViewport::GetCurrent() : vp;
		float n = viewport->GetNearClip();
		float f = viewport->GetFarClip();

		if (Likely(viewport->IsPerspective()))
		{
			float h = viewport->GetTanHFOV();
			float v = viewport->GetTanVFOV();

			// This version is more optimized
			// If you want to go back to the above version - make sure you change getLinearDepth in common.fxh too
			float Q = f / (f - n);
#if __XENON
			return Vector4(h, v, n * Q, Q - 1.0f);
#else
			return Vector4(h, v, -n * Q, -Q);
#endif
		}
		else
		{
			return Vector4(0.0f, 0.0f, n, f - n);
		}
	}

	__forceinline static ScalarV_Out GetLinearDepth(const grcViewport* vp, ScalarV_In sampleDepth)
	{
		const Vec4V proj = VECTOR4_TO_VEC4V(CalculateProjectionParams(vp));

		if (Likely(vp->IsPerspective()))
		{
			// see 'getLinearDepth' in common.fxh
			return proj.GetZ()/(sampleDepth + proj.GetW());
		}
		else
		{
			// see 'getLinearDepthOrtho' in common.fxh
			return AddScaled(proj.GetZ(), proj.GetW(), sampleDepth);
		}
	}

	__forceinline static ScalarV_Out GetSampleDepth(const grcViewport* vp, ScalarV_In linearDepth)
	{
#if 1
		const Vec4V proj = VECTOR4_TO_VEC4V(CalculateProjectionParams(vp));

		if (Likely(vp->IsPerspective()))
		{
			// see 'getSampleDepth' in common.fxh
			return SubtractScaled(proj.GetZ(), proj.GetW(), linearDepth)/linearDepth;
		}
		else
		{
			// see 'getSampleDepthOrtho' in common.fxh
			return (linearDepth - proj.GetZ())/proj.GetW();
		}
#else
		// reference implementation
		const Vec3V camPos = +vp.GetCameraMtx().GetCol3();
		const Vec3V camDir = -vp.GetCameraMtx().GetCol2();
		return TransformProjective(vp->GetCompositeMtx(), AddScaled(camPos, camDir, linearDepth)).GetZ();
#endif
	}

	__forceinline static void DrawNormalizedQuad()
	{
		grcDrawSingleQuadf(0, 0, 1, 1, 0, 0, 0, 1, 1, Color32(255,255,255,255));
	}

	__forceinline static void DrawScreenQuad(const Vector2 &pos, const Vector2 &size)
	{
		const float sx = 1.0f / (float)GRCDEVICE.GetWidth();
		const float sy = 1.0f / (float)GRCDEVICE.GetHeight();

		float x1 = pos.x;
		float y1 = GRCDEVICE.GetHeight() - pos.y;
		float x2 = (pos.x + size.x);
		float y2 = GRCDEVICE.GetHeight() - (pos.y + size.y);

		grcDrawSingleQuadf(x1 * sx, y1 * sy, x2 * sx, y2 * sy, 0, 0, 0, 1, 1, Color32(255,255,255,255));
	}

	static void DumpShaderState(char* label);
};

// ----------------------------------------------------------------------------------------------- //
// Up-Sampling helper. Capable of bilinear, bilaterial, and hybrid bilinear/bilateral up-sampling.
// The hybrid method masks off edge pixels and only runs the more expensive bilateral pass on the
// edge pixels. The hybrid method also supports masking the over-all up-sampling region based on the
// alpha channel of the low resolution source buffer. When doing hybrid up-sampling using an alpha
// mask region, this technique requires both Hi-Stencil and Hi-Z (as well as a spare depth/stencil
// buffer) as it stomps on depth and stencil bits to create and utilize these masks.
class UpsampleHelper
{
public:

	static void Init();
	
	struct UpsampleParams
	{
		UpsampleParams();
		const grcRenderTarget* lowResUpsampleSource;
		const grcRenderTarget* highResUpsampleDestination;
		const grcRenderTarget* lowResDepthTarget;
		const grcRenderTarget* highResDepthTarget;
		
		grcCompositeRenderTargetHelper::CompositeRenderTargetBlendType_e blendType;
		bool lockTarget;
		bool unlockTarget;
		bool maskUpsampleByAlpha; // Set to TRUE if we only want to up-sample low res pixels where alpha>0
	};

	static void PerformUpsample(const UpsampleParams &params);

#if __BANK
	static void AddWidgets(bkBank *bk);
#endif

private:
	static grcEffectVar			ms_texelSizeId;
	static grcEffectVar			ms_bilateralCoefficientId;
	static grcEffectVar			ms_bilateralEdgeThresholdId;

	static grcEffectTechnique	ms_BilinearUpSampleTechnique;
	static grcEffectTechnique	ms_BilateralUpSampleTechnique;
	static grcEffectTechnique	ms_BilinearBilateralUpSampleTechnique;
	static grcEffectTechnique	ms_BilinearBilateralUpSampleTechniqueWithAlphaDiscard;
	
	static grcEffectVar			ms_lowResDepthTextureId;
	static grcEffectVar			ms_hiResDepthTexrcEffectVar;   
	static grcEffectVar			ms_hiResDepthTextureId;
	static grcEffectVar			ms_DepthTextureId;
	
	static bool					ms_ShaderInitialized;

	static void PerformBilinearUpsample(const UpsampleParams &params);
	static void PerformBilateralUpsample(const UpsampleParams &params);
	static void PerformBilinearBilateralComboUpsample(const UpsampleParams &params);
	static void SetupBilateralShader(const UpsampleParams &params);
	static void SetupBilateralCoefficientInShader();

#if __BANK

	static float						ms_bilateralCoefficientValue;
	static float						ms_bilateralEdgeThresholdValue;
	static int							ms_CompositeTechniqueID;
#endif


};

// ----------------------------------------------------------------------------------------------- //

#endif
