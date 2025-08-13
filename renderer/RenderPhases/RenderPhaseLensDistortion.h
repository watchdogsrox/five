//
// filename:	RenderPhaseLenDistortion.h
// description:	Render Phase to handle Oculus Len Distortion
//

#ifndef INC_RENDERPHASELENDISTORTION_H_
#define INC_RENDERPHASELENDISTORTION_H_

// --- Include Files ------------------------------------------------------------
#include "vector/matrix44.h"
#include "vector/vector2.h"
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "renderer/RenderPhases/renderphase.h"

// C headers
// Rage headers
// Game headers


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CRenderPhaseLensDistortion : public CRenderPhase
{
public:
	CRenderPhaseLensDistortion(CViewport* pGameViewport);

	static void InitClass();

	virtual void BuildDrawList();
	virtual void UpdateViewport();
#if __BANK
	static void InitWidgets();
#endif // __BANK
	static void SetRenderTargets(grcRenderTarget* pFinalTarget, grcRenderTarget* pSrcLeft, grcRenderTarget* pSrcRight);
	static void Render();
	static void DrawEye(bool bLeft);

	static float GetFOV() { return sm_fFOV; }
	static float GetAspectRatio() { return sm_fAspectMod; }

private:
	static grcRenderTarget* sm_Target;
	static grcRenderTarget* sm_SrcLeft;
	static grcRenderTarget* sm_SrcRight;

	static grmShader* sm_pShader;

	static grcEffectTechnique sm_LensDistortionTechnique;

	static grcEffectVar sm_TexTransformId;
	static grcEffectVar sm_LensCenterId;
	static grcEffectVar sm_ScreenCenterId;
	static grcEffectVar sm_ScreenSizeHalfScaleId;
	static grcEffectVar sm_ScaleId;
	static grcEffectVar sm_ScaleInId;
	static grcEffectVar sm_HwdWarpParamId;
	static grcEffectVar sm_ChromAbParamId;

	static grcEffectVar sm_SourceTexId;

	static Matrix44		sm_matTexTransform;
	static Vector2		sm_vLenCenter;
	static Vector2		sm_vScreenCenter;
	static Vector2		sm_vHalfScreenSizeScale;
	static Vector2		sm_vScale;
	static Vector2		sm_vScaleIn;
	static Vector4		sm_vHwdWarpParam;
	static Vector4		sm_vChromAbParam;

	static bool			sm_ChromaticAbberration;

	static float		sm_fAspectMod;
	static float		sm_fFOV;
#if __BANK
	static bool			sm_EnablePhase;
#endif // __BANK
};

#endif // !INC_RENDERPHASELENDISTORTION_H_
