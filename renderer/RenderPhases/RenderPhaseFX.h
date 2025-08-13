//
// filename:	RenderPhaseFX.h
// description: renderphase classes
//

#ifndef INC_RENDERPHASEFX_H_
#define INC_RENDERPHASEFX_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "rmptfx/ptxshader.h"
#include "grcore/stateblock.h"

// Game headers
#include "RenderPhase.h"
#include "modelinfo/PedModelInfo.h"

// --- Defines ------------------------------------------------------------------
#define SEETHROUGH_LAYER_WIDTH		800
#define SEETHROUGH_LAYER_HEIGHT		450
#define SEETHROUGH_HEAT_SCALE_COLD	(0.0f)
#define SEETHROUGH_HEAT_SCALE_WARM	(0.75f)
// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

//
// name:		RenderPhaseSeeThrough
// description:	SeeThrough effect target
class RenderPhaseSeeThrough : public CRenderPhaseScanned
{
public:
	RenderPhaseSeeThrough(CViewport* pGameViewport);
	~RenderPhaseSeeThrough();

	virtual void UpdateViewport();
	virtual void BuildRenderList();
	virtual void BuildDrawList();
	virtual u32 GetCullFlags() const;

#if __BANK
	virtual void AddWidgets(bkGroup& group);
#endif // __BANK



	static inline void SetRenderTargetColor(grcRenderTarget *target) { sm_color = target; }
	static inline void SetRenderTargetBlur(grcRenderTarget *target) { sm_blur = target; }
	static inline void SetRenderTargetDepth(grcRenderTarget *target) { sm_depth = target; }

	static void InitClass();
	static void InitSession();
	static void UpdateVisualDataSettings();

	static inline grcRenderTarget *GetColorRT() { return sm_color; }
	static inline grcRenderTarget *GetBlurRT() { return sm_blur; }
	static RenderPhaseSeeThrough * GetRenderPhase() { return sm_phase; }

	static inline bool GetState() { return sm_enabled; }
	static inline void SetState(bool state) { sm_enabled = state; }

	static inline float GetPedQuadSize() { return sm_pedQuadSize; }
	static inline float GetPedQuadStartDistance() { return sm_pedQuadStartDistance; }
	static inline float GetPedQuadEndDistance() { return sm_pedQuadEndDistance; }
	static inline float GetPedQuadScale() { return sm_pedQuadScale; }

	static inline float GetFadeStartDistance() { return sm_fadeStartDistance; }
	static inline void SetFadeStartDistance(float value) { sm_fadeStartDistance=value; }
	static inline float GetFadeEndDistance() { return sm_fadeEndDistance; }
	static inline void SetFadeEndDistance(float value) { sm_fadeEndDistance=value; }
	static inline float GetMaxThickness() { return sm_maxThickness; }
	static inline void SetMaxThickness(float value) { sm_maxThickness=Clamp(value,1.0f,10000.0f); } // Clamped to min=1 so that we don't divide by 0 in the shader
													
	static inline float GetMinNoiseAmount() { return sm_minNoiseAmount; }
	static inline void SetMinNoiseAmount(float value) { sm_minNoiseAmount=value; }
	static inline float GetMaxNoiseAmount() { return sm_maxNoiseAmount; }
	static inline void SetMaxNoiseAmount(float value) { sm_maxNoiseAmount=value; }
	static inline float GetHiLightIntensity() { return sm_hiLightIntensity; }
	static inline void SetHiLightIntensity(float value) { sm_hiLightIntensity=value; }
	static inline float GetHiLightNoise() { return sm_hiLightNoise; }
	static inline void SetHiLightNoise(float value) { sm_hiLightNoise=value; }
													
	static inline Color32 GetColorNear() { return sm_colorNear; }
	static inline void SetColorNear(Color32 color) { sm_colorNear = color; }
	static inline Color32 GetColorFar() { return sm_colorFar; }
	static inline void SetColorFar(Color32 color) { sm_colorFar = color; }
	
	static inline Color32 GetColorVisibleBase() { return sm_colorVisibleBase; }
	static inline Color32 GetColorVisibleWarm() { return sm_colorVisibleWarm; }
	static inline Color32 GetColorVisibleHot() { return sm_colorVisibleHot; }
	static inline void SetColorVisibleBase(Color32 color) { sm_colorVisibleBase = color; }
	static inline void SetColorVisibleWarm(Color32 color) { sm_colorVisibleWarm = color; }
	static inline void SetColorVisibleHot(Color32 color) { sm_colorVisibleHot = color; }
	
	static inline void SetHeatScale(unsigned int idx,float scale) { Assert(idx < TB_COUNT); sm_heatScale[idx] = Clamp(scale, SEETHROUGH_HEAT_SCALE_COLD, SEETHROUGH_HEAT_SCALE_WARM); }
	static inline float GetHeatScale(unsigned int idx) { Assert(idx < TB_COUNT); return sm_heatScale[idx]; }
	
	static inline void ResetHeatScale() { sm_heatScale[0] = 0.0f; sm_heatScale[1] = 0.5f; sm_heatScale[2] = 0.75f; sm_heatScale[3] = 1.0f; }

	static void SetPedQuadsRenderState();

	static void PrepareMainPass();
	static void FinishMainPass();

	static void SetGlobals();

private:

	static void InitRenderStateBlocks();

	static RenderPhaseSeeThrough *sm_phase;

	static grcRenderTarget *sm_color;
	static grcRenderTarget *sm_blur;
	static grcRenderTarget *sm_depth;
	
	static s32 sm_TechniqueGroupId;
	
	static bool sm_enabled;
	
	static float sm_pedQuadSize;
	static float sm_pedQuadStartDistance;
	static float sm_pedQuadEndDistance;
	static float sm_pedQuadScale;
	
	static float sm_fadeStartDistance;
	static float sm_fadeEndDistance;
	static float sm_maxThickness;
	
	static float sm_minNoiseAmount;
	static float sm_maxNoiseAmount;
	static float sm_hiLightIntensity;
	static float sm_hiLightNoise;

	static Color32 sm_colorNear;
	static Color32 sm_colorFar;
	static Color32 sm_colorVisibleBase;
	static Color32 sm_colorVisibleWarm;
	static Color32 sm_colorVisibleHot;
	
	static float sm_heatScale[TB_COUNT];
	
	static void ptFxSimpleRender(grcRenderTarget* backBuffer, grcRenderTarget *depthBuffer);
	
	static ptxShaderTemplateOverride sm_ptxPointShaderOverride;
	static ptxShaderTemplateOverride sm_ptxTrailShaderOverride;

	// state blocks for ped quad rendering
	static grcRasterizerStateHandle		sm_pedQuadsRS;
	static grcBlendStateHandle			sm_pedQuadsBS;
	static grcDepthStencilStateHandle	sm_pedQuadsDS;

	// state blocks for main pass 
	static grcRasterizerStateHandle		sm_mainPassRS;
	static grcBlendStateHandle			sm_mainPassBS;
	static grcDepthStencilStateHandle	sm_mainPassDS;

	static grcRasterizerStateHandle		sm_exitMainPassRS;
	static grcBlendStateHandle			sm_exitMainPassBS;
	static grcDepthStencilStateHandle	sm_exitMainPassDS;

};


#endif // !INC_RENDERPHASEFX_H_
