//=============================================================================================
// renderer/MLAA.h
// 
// Copyright (C) 2013 Rockstar Games.  All Rights Reserved. 
// 
// Morphological Anti-Aliasing
//
//=============================================================================================

#ifndef _MLAA_H_
#define _MLAA_H_

#define ENABLE_MLAA (0 && (RSG_ORBIS || RSG_PC || RSG_DURANGO)) 

#if ENABLE_MLAA
#define MLAA_ONLY(x)	x
#define MLAA_SWITCH(__if__,__else__)	__if__
#else
#define MLAA_ONLY(x)	
#define MLAA_SWITCH(__if__,__else__)	__else__
#endif // ENABLE_MLAA

#if ENABLE_MLAA

//=============================================================================================
// Helper class for doing MLAA 
//=============================================================================================
class MLAA
{
public:

	// Contructor
	MLAA();

	// Destructor
	~MLAA();

	// Initialize MLAA helper and allocate local buffers
	bool Initialize(grcRenderTarget* poScratchTarget = NULL);

	// Free local buffers and shut down
	void Shutdown(bool releaseScratchTarget);

	// Anti-Alias a render target. 
	void AntiAlias( grcRenderTarget* pDstColor, grcRenderTarget* pDstDepthReadOnly, const grcTexture* pSrcColor, const grcRenderTarget* pSrcDepthStencil );
	
#if __BANK
	// Setup BANK
	static void AddWidgets ( bkBank &bk );
#endif //__BANK

	bool IsEnabled() const { return m_bEnable; }

#if __BANK
	typedef void (*MLAAFilterOverrideCB)(const grcTexture*);

	// Override the filter kernel (while still using the edge kernel)
	static void SetKernelOverrideCB( MLAAFilterOverrideCB pFilterCallback ) {m_pFilterOverrideCB = pFilterCallback; }
#endif

private:

	// Create the look-up table textures
	bool CreateTextures(grcRenderTarget* poScratchTarget = NULL);

	// Draw a screen aligned quad with the specified technique
	static bool DrawScreenQuad ( grcRenderTarget* pColorTarget, grcRenderTarget* pDepthStencilTarget, grmShader* pShader, grcEffectTechnique technique );

	grmShader* m_pShader;		// The MLAA shader

	// Shader techniques
	grcEffectTechnique m_EdgeDetectionTechnique;
	grcEffectTechnique m_DepthEdgeDetectionTechnique;
	grcEffectTechnique m_BlendWeightCalculationTechnique;
	grcEffectTechnique m_AntiAliasingTechnique;
	grcEffectTechnique m_NonAliasedCopyTechnique;


	// Shader parameters
	grcEffectVar m_TexelSizeParam;
	grcEffectVar m_LuminanceThresholdParam;
	grcEffectVar m_RelativeLuminanceThresholdParams;
	grcEffectVar m_PointSamplerParam;
	grcEffectVar m_LinearSamplerParam;
	grcEffectVar m_LUTSamplerParam;

	// Look up table texture
	grcTexture* m_pAreaLUTTexture;

	// Scratch buffer for holding edge weights
	grcRenderTarget* m_pScratchRenderTarget;

	grcDepthStencilStateHandle m_CreateStencilMask_DS;
	grcDepthStencilStateHandle m_UseStencilMask_DS;
	grcDepthStencilStateHandle m_DoNotUseStencilMask_DS;


	static bool	m_bEnable; // Toggle MLAA on/off

#if __BANK
	static bool m_bOverrideFilterKernel;
	static MLAAFilterOverrideCB m_pFilterOverrideCB;
#endif

	static float m_fLuminanceThreshold; // Luminance threshold used by edge detection
	static bool m_bDepthEdgeDetection; // Toggle between depth and luminance based edge detection
	static float m_fLuminanceThresholdBase;
	static float m_fLuminanceThresholdScale;

	static bool	m_bVisualizeEdges;
	static bool m_bUseHiStencil;
};

#endif // ENABLE_MLAA
#endif //_MLAA_H_
