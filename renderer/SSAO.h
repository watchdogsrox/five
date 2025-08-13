//
// Screen Space Ambient Occlusion header:
//
// 2010/05/13 - initial; 
//
//
//
#ifndef __SSAO_H__
#define __SSAO_H__

namespace rage
{
	class grcRenderTarget;
	class Vector4;
	class grmShader;
	class bkBank;
};

#include "grcore/effect_typedefs.h"
#include "renderer/SpuPM/SpuPmMgr.h"
#include "grcore/texture.h"
#include "atl/singleton.h"
#include "grcore/fastquad.h"
#include "timecycle/TimeCycle.h"
#include "renderer/SSAO_shared.h"

// MR SSAO can render to the ambient of the g-buffer or prepare AO values in a texture.
#define MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE 0x1
// Make sure to keep it in sync with the shader (SSAO.fx)
#define SUPPORT_QSSSAO2	(__D3D11 || RSG_ORBIS)
#define SUPPORT_MRSSAO	(__D3D11 || RSG_ORBIS)
#define SUPPORT_HDAO	(__D3D11 || RSG_ORBIS)
#define SUPPORT_HDAO2	(__D3D11 || RSG_ORBIS)
#define SSAO_USEDEPTHRESOLVE	(DEVICE_MSAA)

#if SUPPORT_QSSSAO2
#	define SUPPORT_QSSSAO2_ONLY(...)    __VA_ARGS__
#else
#	define SUPPORT_QSSSAO2_ONLY(...)
#endif

// ----------------------------------------------------------------------------------------------- //
// ENUMERATIONS
// ----------------------------------------------------------------------------------------------- //

enum ssao_technique
{
	ssaotechnique_qsssao,
	ssaotechnique_cpssao,
	ssaotechnique_pmssao,
	ssaotechnique_cpqsmix,
	ssaotechnique_cp4Dirqsmix,
	ssaotechnique_cp8Dirqsmix,
#if __D3D11 || RSG_ORBIS
	ssaotechnique_mrssao,
	ssaotechnique_hdao,
	ssaotechnique_hdao2,
	ssaotechnique_qsssao2,
#if SUPPORT_HBAO
	ssaotechnique_hbao,
#endif // SUPPORT_HBAO
#endif	//__D3D11 || RSG_ORBIS	
	ssaotechnique_count
};

enum ssao_pass
{
	ssao_downscale = 0,
#if RSG_PC
	ssao_downscale_sm50,
#endif
	ssao_linearizedepth,
	ssao_downscale_fromlineardepth,
	ssao_upscale,
	ssao_upscale_isolate,
	ssao_upscale_isolate_no_power,
	ssao_process,
	ssao_blur_bilateral_x,
	ssao_blur_bilateral_y,
	cpssao_downscale,
	cpssao_process,
	cpssao_upscale,
	cpssao_upscale_isolate,
	cpssao_upscale_isolate_no_power,
	pmssao_downscale,
	pmssao_process,
	cpqsmix_qs_ssao_process,
	cpqsmix_cp_ssao_process_and_combine_with_qs,
	cpqsmix_cp4Directions_ssao_process_and_combine_with_qs,
	cpqsmix_cp8Directions_ssao_process_and_combine_with_qs,
#if SUPPORT_HBAO
	hbao_solo,
	hbao_normal,
	hbao_normal_hybrid,
	hbao_hybrid,
	ssao_blur_gauss_bilateral_x,
	ssao_blur_gauss_bilateral_y,
	ssao_blur_gauss_bilateral_y_temporal,
	hbao_temporal_copy,
	hbao_continuity_mask,
	hbao_dilate_continuity_mask_sm50,
	hbao_dilate_continuity_mask_nogather,
#endif
#if SUPPORT_MRSSAO
	mrssao_downscale,
	mrssao_downscale_from_gbuffer,
	mrssao_compute_ao,
	mrssao_compute_ao_stippledkernel,
	mrssao_compute_ao_starkernel,
	mrssao_compute_ao_from_gbuffer,
	mrssao_compute_ao_from_gbuffer_stippledkernel,
	mrssao_compute_ao_from_gbuffer_starkernel,
	mrssao_compute_ao_and_combine,
	mrssao_compute_ao_and_combine_stippledkernel,
	mrssao_compute_ao_and_combine_starkernel,
	mrssao_compute_ao_from_gbuffer_and_combine,
	mrssao_compute_ao_from_gbuffer_and_combine_stippledkernel,
	mrssao_compute_ao_from_gbuffer_and_combine_starkernel,
	mrssao_upsample_ao_from_lower_level,
	mrssao_blur_ao,
	mrssao_blur_ao_and_invert_then_apply_power,
	mrssao_apply,
	mrssao_apply_isolate,
	mrssao_apply_isolate_no_power,
	mrssao_blur_ao_and_apply,
	mrssao_apply_isolate_blur,
#endif //SUPPORT_MRSSAO

#if __BANK
	ssao_downscale_ortho,
#if RSG_PC
	ssao_downscale_ortho_sm50,
#endif // RSG_PC
	ssao_linearizedepth_ortho,
	cpssao_downscale_ortho,
	cpssao_process_ortho,
	pmssao_downscale_ortho,
	pmssao_process_ortho,
#	if SUPPORT_QSSSAO2
	ssao_process_enhanced,
	ssao_blur_bilateral_enhanced,
#	endif	//SUPPORT_QSSSAO2
#	if SUPPORT_MRSSAO
	mrssao_downscale_from_gbuffer_ortho,
	mrssao_compute_ao_from_gbuffer_ortho,
	mrssao_compute_ao_from_gbuffer_and_combine_ortho,
#	endif //SUPPORT_MRSSAO
#endif // __BANK
	ssao_passcount
};


enum ssao_variables
{	
	ssao_project_params			= 0,
	ssao_normaloffset,
	ssao_offsetscale0,
	ssao_offsetscale1,
	ssao_deferredlighttexture1,
	ssao_deferredlighttexture0p,
	ssao_gbuffertexture2,
	ssao_gbuffertexturedepth,
	ssao_deferredlighttexture2,
	ssao_deferredlighttexture,
	ssao_offsettexture,
	ssao_deferredlightscreensize,
	ssao_strength,
	ssao_cpqsmix_qs_fadein,
	ssao_lineartexture1,
	ssao_pointtexture1,
	ssao_pointtexture2,
	ssao_pointtexture3,
	ssao_pointtexture4,
	ssao_pointtexture5,
	ssao_targetsize,
	ssao_falloffandkernel,
	ssao_msaa_pointtexture1,
	ssao_msaa_pointtexture2,
	ssao_msaa_pointtexture1_dim,
	ssao_msaa_pointtexture2_dim,

#if SSAO_USEDEPTHRESOLVE
	depth_resolve_texture,
#endif // SSAO_USEDEPTHSAMPLE0

#if SSAO_UNIT_QUAD
	ssao_quad_position,
	ssao_quad_texcoords,
#endif

#if SUPPORT_HBAO
	ssao_extra_params0,
	ssao_extra_params1,
	ssao_extra_params2,	
	ssao_extra_params3,
	ssao_extra_params4,
	ssao_perspective_shear_params0,
	ssao_perspective_shear_params1,
	ssao_perspective_shear_params2,
	ssao_prev_viewproj,
	ssao_stencil_pointtexture,
#endif

	ssao_variables_count
};

enum hdao_pass
{
	hdao_compute_high,
	hdao_compute_med,
	hdao_compute_low,
	hdao_alternative_high,
	hdao_alternative_med,
	hdao_alternative_low,
	hdao_blur_fixed,
	hdao_blur_fixed_2d,
	hdao_blur_variable,
	hdao_blur_smart,
};

enum hdao2_pass
{
	hdao2_downscale,
	hdao2_compute_high,
	hdao2_compute_medium,
	hdao2_compute_low,
	hdao2_filter_hor,
	hdao2_filter_ver,
	hdao2_apply,
	hdao2_apply_dark,
};


/*--------------------------------------------------------------------------------------------------*/
/* Multi-resolution SSAO par gen classes.															*/
/*--------------------------------------------------------------------------------------------------*/

class ResolutionGroup	{
public:
	ResolutionGroup()
	: m_Dim(1.f,1.f)
	{}
	~ResolutionGroup()
	{}
public:
	Vector2 m_Dim;
};

#define MRSSAO_NUM_RESOLUTIONS	9

// Note that this class cannot be inside of #if SUPPORT_MRSSAO, otherwise we'll get a parser error.
class MR_SSAO_Parameters : public ResolutionGroup
{
public:
	MR_SSAO_Parameters()
	{
		// Radius in  world space units.
		m_FallOffRadius = 0.5f;
		// Kernel as a percentage of screen size.
		m_KernelSizePercentage = 0.8593f;
		// Actual kernel size is (2*m_MaxKernel_Lod0 + 1) at LOD 0. 
		m_MaxKernel_Lod0 = 1;
		m_MaxKernel_Lod1 = 1;
		// The LOD at which to apply to the screen (default is 0 meaning full screen LOD).
		m_LodToApply = 1;
		// The lowest LOD.
		m_BottomLod = MRSSAO_NUM_RESOLUTIONS-1;
		// Normal weight power (actual power is 2*m_NormalWeightPower, it must be even).
		m_NormalWeightPower = 4;
		// Depth weight power.
		m_DepthWeightPower = 2.0f;
		m_Strength = 3.0f;
		// Use full kernel.
		m_KernelType = 0;
	}
	~MR_SSAO_Parameters()
	{
	}

public:
	float m_FallOffRadius;
	float m_KernelSizePercentage;
	int m_MaxKernel_Lod0;
	int m_MaxKernel_Lod1;
	int m_LodToApply;
	int m_BottomLod;
	int m_NormalWeightPower;
	float m_DepthWeightPower;
	float m_Strength;
	int m_KernelType;
	PAR_SIMPLE_PARSABLE;
};

enum {
	MR_SSAO_CONFIG_MAX_RESOLUTIONS = 3,
	MR_SSAO_CONFIG_MAX_SCENE_TYPES = 2
};

class MR_SSAO_ScenePreset
{
public:
	MR_SSAO_ScenePreset()
	{
	}
	~MR_SSAO_ScenePreset()
	{
	}
public:
	// Low, medium and high resolutions
	MR_SSAO_Parameters m_Resolutions[MR_SSAO_CONFIG_MAX_RESOLUTIONS];
	PAR_SIMPLE_PARSABLE;
};

class MR_SSAO_AllScenePresets
{
public:
	MR_SSAO_AllScenePresets()
	{
	}
	~MR_SSAO_AllScenePresets()
	{
	}
public:
	// Low, medium and high.
	MR_SSAO_ScenePreset m_SceneType[MR_SSAO_CONFIG_MAX_SCENE_TYPES];
	PAR_SIMPLE_PARSABLE;
};

/*--------------------------------------------------------------------------------------------------*/
/* QS-SSAO, HDAO, and HDAO2 Parameters																*/
/*--------------------------------------------------------------------------------------------------*/

enum {
	QS_SSAO_CONFIG_MAX_RESOLUTIONS	= 3,
	HDAO_CONFIG_MAX_QUALITY_LEVELS	= 3,
	HDAO_CONFIG_MAX_BLUR_TYPES		= 3,
};

class QS_SSAO_Parameters : public ResolutionGroup
{
public:
	QS_SSAO_Parameters()
	: m_BlurPasses(1)
	{}
	~QS_SSAO_Parameters()
	{}

public:
	int		m_BlurPasses;
	PAR_SIMPLE_PARSABLE;
};

class QS_SSAO_ScenePreset
{
public:
	QS_SSAO_ScenePreset()
	{}
	~QS_SSAO_ScenePreset()
	{}
public:
	// Low, medium and high resolutions
	QS_SSAO_Parameters m_Resolutions[QS_SSAO_CONFIG_MAX_RESOLUTIONS];
	PAR_SIMPLE_PARSABLE;
};

class HDAO_Parameters
{
public:
	HDAO_Parameters()
	: m_Quality("Medium")
	, m_Alternative(true)
	, m_BlurType("Variable")
	, m_Strength(1.f)
	, m_RejectRadius(0.5f)
	, m_AcceptRadius(0.03f)
	, m_FadeoutDist(3.f)
	, m_NormalScale(0.1f)
	, m_WorldSpace(0.5f)
	, m_TargetScale(0.1f)
	, m_RadiusScale(1.f)
	, m_BaseWeight(1.f)
	{}
	~HDAO_Parameters()
	{}

public:
	atString	m_Quality;
	bool		m_Alternative;
	atString	m_BlurType;
	float		m_Strength;
	float		m_RejectRadius;
	float		m_AcceptRadius;
	float		m_FadeoutDist;
	float		m_NormalScale;
	float		m_WorldSpace;
	float		m_TargetScale;
	float		m_RadiusScale;
	float		m_BaseWeight;
	PAR_SIMPLE_PARSABLE;
};

class HDAO_ScenePreset
{
public:
	HDAO_ScenePreset()
	{}
	~HDAO_ScenePreset()
	{}
public:
	// Low, medium and high quality
	HDAO_Parameters m_QualityLevels[HDAO_CONFIG_MAX_QUALITY_LEVELS];
	PAR_SIMPLE_PARSABLE;
};

class HDAO2_Parameters
{
public:
	HDAO2_Parameters()
	: m_Quality("Medium")
	, m_UseLowRes(true)
	, m_UseFilter(true)
	, m_Strength(0.5f)
	, m_RejectRadius(0.8f)
	, m_AcceptRadius(0.003f)
	, m_FadeoutDist(3.f)
	, m_NormalScale(0.05f)
	, m_RadiusScale(1.0f)
	{}
	~HDAO2_Parameters()
	{}

public:
	atString	m_Quality;
	bool		m_UseLowRes;
	bool		m_UseFilter;
	float		m_Strength;
	float		m_RejectRadius;
	float		m_AcceptRadius;
	float		m_FadeoutDist;
	float		m_NormalScale;
	float		m_RadiusScale;
	PAR_SIMPLE_PARSABLE;
};

class HDAO2_ScenePreset
{
public:
	HDAO2_ScenePreset()
	{}
	~HDAO2_ScenePreset()
	{}
public:
	// Low, medium and high quality
	HDAO2_Parameters m_QualityLevels[HDAO_CONFIG_MAX_QUALITY_LEVELS];
	PAR_SIMPLE_PARSABLE;
};

/*--------------------------------------------------------------------------------------------------*/
/* CP-QS-mix-SSAO Parameters																		*/
/*--------------------------------------------------------------------------------------------------*/

class CPQSMix_Parameters
{
public:
	CPQSMix_Parameters()
	{
		m_CPRelativeStrength = 1.0f;
		m_QSRelativeStrength = 1.0f;
		m_QSFadeInStart = 0.5f;
		m_QSFadeInEnd = 1.5f;	
		m_QSRadius = 32.0f;
		m_CPRadius = 16.0f;
		m_CPNormalOffset = 16.0f;
	}
	~CPQSMix_Parameters()
	{}
public:
	float m_CPRelativeStrength;
	float m_QSRelativeStrength;
	float m_QSFadeInStart;
	float m_QSFadeInEnd;
	float m_QSRadius;
	float m_CPRadius;
	float m_CPNormalOffset;
	PAR_SIMPLE_PARSABLE;
};

/*--------------------------------------------------------------------------------------------------*/
/* HBAO Parameters																					*/
/*--------------------------------------------------------------------------------------------------*/
class HBAO_Parameters
{
public:
	HBAO_Parameters() 
		: m_HBAORelativeStrength(1.0f)
		, m_CPRelativeStrength(1.0f)
		, m_HBAORadius0(0.1f)
		, m_HBAORadius1(1.0f)
		, m_HBAOBlendDistance(15.0f)
		, m_CPRadius(1.0f)
		, m_MaxPixels(50.0f)
		, m_CutoffPixels(3.0f)
		, m_FoliageStrength(1.0f)
		, m_HBAOFalloffExponent(1.0f)
		, m_CPStrengthClose(0.0f)
		, m_CPBlendDistanceMin(20.0f)
		, m_CPBlendDistanceMax(50.0f)
	{}

	~HBAO_Parameters()
	{}	
public:
	float m_HBAORelativeStrength;
	float m_CPRelativeStrength;
	float m_HBAORadius0;
	float m_HBAORadius1;
	float m_HBAOBlendDistance;
	float m_CPRadius;
	float m_MaxPixels;
	float m_CutoffPixels;
	float m_FoliageStrength;
	float m_HBAOFalloffExponent;
	float m_CPStrengthClose;
	float m_CPBlendDistanceMin;
	float m_CPBlendDistanceMax;
	PAR_SIMPLE_PARSABLE;
};

/************************************************************************************************************************************/
/* class SSAO.																														*/
/************************************************************************************************************************************/

class SSAO
{
public:
	// Functions
	static void InitializeOffsetsTexture();
	static void CreateRenderTargets();
	static void DeleteRenderTargets();

	static void ResetShaders();
	static void InitShaders();

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif

#if SSAO_USEDEPTHRESOLVE
	static void ResolveDepthSample0();
#endif // SSAO_USEDEPTHSAMPLE0

	static void	Update();
	static void	CalculateAndApply(bool bDoCalculate=true, bool bDoApply=true);

// ----------------------------------------------------------------------------------------------- //
// Console SSAO functions.
// ----------------------------------------------------------------------------------------------- //

	static void	CalculateAndApply_Console(bool bDoCalculate=true, bool bDoApply=true);

	static void	Downscale();
	static void	Apply();
	static void Process();
	static void ProcessQS(bool isCPQSMix);
	static void ProcessCP();
	static void ProcessPM();
	static void ProcessCPQRMix(ssao_technique technique);
	static float GetCPQSStrength(TimeCycleVar_e varType, float tc_override);
#if __PS3
	static void	ProcessSPU();
#endif //__PS3

	static void	SetStrength(float strength)				{ m_strength = strength; }

	static bool	Enabled()								{ return m_enable; }
	static void	Enable(bool enabled)					{ m_enable = enabled; }
#if __PS3
	static bool	SpuEnabledUT()							{ FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));		return m_enableSpu;		}
	static bool	SpuEnabled()							{ FastAssert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	return m_enableSpuRT;	}
#endif // __PPU

#if __BANK
	static void	AddWidgets(bkBank &bk);
	static void	Debug();
	static void	Debug_Console();
	static void SetEnable(bool bEnable)					{ m_enable = bEnable; }
	static void SetOverrideTechnique(ssao_technique te)	{ m_TechniqueOverrideEnable = true; m_ForcedSSAOTechnique = te; }
	static const char** GetSSAOTechniqueNames();
	static void DisableOverrideTechnique()				{ m_TechniqueOverrideEnable = false; }
	static bool Isolated()								{ return m_isolate; }
	static void SetIsolate(bool bIsolate)				{ m_isolate = bIsolate; }
	static void SetOverrideIntensity(float intensity)	{ m_strengthOverrideEnable = true; m_strengthOverride = intensity; }
	static void DisableOverrideIntensity()				{ m_strengthOverrideEnable = false; }
	static void OnComboBox_OverrideTechnique(void);
#endif

	// misc
#if __BANK
	static void SaveParams_QS(void);
	static void LoadParams_QS(void);
	static void OnComboBox_QS(void);
#endif
	static void UpdateSettingsToUse_QS(int Resolution);

#if __BANK
	static void SaveParams_CPQSMix(void);
#endif
	static void LoadParams_CPQSMix(void);

// ----------------------------------------------------------------------------------------------- //
// Multi-resolution SSAO data.
// ----------------------------------------------------------------------------------------------- //

#if SUPPORT_MRSSAO
	static void Init_MRSSAO();
	static void Shutdown_MRSSAO();
	static void CreateRendertargets_MRSSAO();
	static void DestroyRendertargets_MRSSAO();
	static void CalculateAndApply_MRSSAO(bool bDoCalculate, bool bDoApply);

	// Rendering helpers.
	static ssao_pass GetComputeAOPass_MRSSAO(int LOD, int KernelSize);
	static ssao_pass GetComputeAOAndCombinePass_MRSSAO(int LOD, int KernelSize);
	static void RenderToTarget_MRSSAO(grcRenderTarget *pTarget, const char pDebugStr[], ssao_pass Pass);
	static void RenderToMultiTarget_MRSSAO(grcRenderTarget *pColour0, grcRenderTarget *pColour1, const char pDebugStr[], ssao_pass Pass);
	static void RenderToMultiTarget4_MRSSAO(grcRenderTarget *pColour0, grcRenderTarget *pColour1, grcRenderTarget *pColour2, grcRenderTarget *pColour3, const char pDebugStr[], ssao_pass Pass);
	static void DrawFullScreenQuad_MRSSAO(const char pDebugStr[], ssao_pass Pass);

	// Misc functions.
	static void UpdateSettingsToUse_MRSSAO(int Preset, int Resolution);

#	if __BANK
	static void AddWidgets_MRSSAO(bkBank &bk);

	static void SaveParams_MRSSAO(void);
	static void LoadParams_MRSSAO(void);
	static void OnComboBox_MRSSAO(void);
	static void OnComboBox_MRSSAO_KernelType(void);

	static void OnParamsToClipboard_MRSSAO(void);
	static void OnClipboardToParams_MRSSAO(void);
	static void OnScenePresetToClipboard_MRSSAO(void);
	static void OnClipboardToScenePreset_MRSSAO(void);
	static void Debug_MRSSAO();
#	endif // __BANK
#endif // SUPPORT_MRSSAO

// ----------------------------------------------------------------------------------------------- //
// HDAO data.
// ----------------------------------------------------------------------------------------------- //
#if SUPPORT_HDAO	
	static void Reset_HDAOShaders();
	static void Init_HDAOShaders();
	static void Init_HDAO(bool bCore);
	static void Shutdown_HDAO(bool bCore);
	static void CalculateAndApply_HDAO(bool bDoCalculate, bool bDoApply);
	static void RenderToTarget_HDAO(grcRenderTarget *pTarget, const char pDebugStr[], bool apply, hdao_pass Pass);
	static void DrawFullScreenQuad_HDAO(const char pDebugStr[], bool apply, hdao_pass Pass);

#	if __BANK
	static void AddWidgets_HDAO(bkBank &bk);
	static void Debug_HDAO();
	// Misc functions.
	static void SaveParams_HDAO(void);
	static void LoadParams_HDAO(void);
	static void OnComboBox_HDAO(void);
#	endif	//__BANK

	static int	GetQualityLevel_HDAO(const atString &QualityString);
	static void	UpdateSettingsToUse_HDAO(const int QualityLevel);
#endif // SUPPORT_HDAO

#if SUPPORT_HDAO2
	static void Reset_HDAO2Shaders();
	static void Init_HDAO2Shaders();
	static void Init_HDAO2(bool bCore);
	static void Shutdown_HDAO2(bool bCore);
	static void CreateRendertargets_HDAO2();
	static void DestroyRendertargets_HDAO2();
	static void CalculateAndApply_HDAO2(bool bDoCalculate, bool bDoApply);
	static void DrawFullScreenQuad_HDAO2(GPUTimebarIds tid, const char pDebugStr[], hdao2_pass Pass);

#	if __BANK
	static void AddWidgets_HDAO2(bkBank &bk);
	static void Debug_HDAO2();

	// Misc functions.
	static void SaveParams_HDAO2(void);
	static void LoadParams_HDAO2(void);
	static void OnComboBox_HDAO2(void);
#	endif //__BANK

	static int	GetQualityLevel_HDAO2(const atString &QualityString);
	static void	UpdateSettingsToUse_HDAO2(const int QualityLevel);
#endif // SUPPORT_HDAO2

	// ----------------------------------------------------------------------------------------------- //
#if RSG_PC
	static bool UseNextGeneration(const Settings &settings);
#endif
#if SUPPORT_HBAO
	static void Init_HBAO(bool bCore);
	static void InitShaders_HBAO();
	static void Shutdown_HBAO(bool bCore);
	static void CreateRendertargets_HBAO();
	static void DestroyRendertargets_HBAO();
	static void CalculateAndApply_HBAO(bool bDoCalculate, bool bDoApply);

#	if __BANK
	static void AddWidgets_HBAO(bkBank &bk);
	static void Debug_HBAO();

	// Misc functions.
	static void SaveParams_HBAO(void);
	static void LoadParams_HBAO(void);	
#	endif //__BANK
#endif // SUPPORT_HBAO

	static void DownscaleDepthToQuarterTarget();
#if SSAO_USE_LINEAR_DEPTH_TARGETS
	static void CreateFullScreenLinearDepth();
	static void CreateQuaterLinearDepth();
#endif // SSAO_USE_LINEAR_DEPTH_TARGETS

#if __XENON
	static grcRenderTarget*	GetSSAORT();
#endif //__XENON
	static const grcTexture* GetSSAOTexture();

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	static void SetMSSAPointTexture1(grcRenderTarget *pRenderTarget);
	static void SetMSSAPointTexture2(grcRenderTarget *pRenderTarget);
#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS

protected:
	// Variables
	static bool				m_enable;
	static float			m_strength;
#if __BANK
	static bool				m_TechniqueOverrideEnable;
	static ssao_technique	m_ForcedSSAOTechnique;
	static bool				m_isolate;
	static bool				m_strengthOverrideEnable;
	static float			m_strengthOverride;
#endif // __BANK
#if __PS3
	static bool				m_enableSpu;
	static bool				m_enableSpuRT;
	static float			m_SpuTimeTotal;
	static float			m_SpuTimePerSpu;
	static int				m_SpuNumUnitsUtilized;
#endif

#if RSG_PC
	static bool				m_Initialized;
	static bool				m_useNextGenVersion;
#endif

#if SPUPMMGR_PS3
	static grcSpuDestTexture spuGBufferSSAO_0;
#endif

	static grmShader* m_shaderSSAO;
	static grcEffectTechnique m_techniqueSSAO;
	static grcEffectVar m_variables[ssao_variables_count];

	static grcBlendStateHandle m_SSAO_B;
	static grcBlendStateHandle m_Apply_B;
	static grcDepthStencilStateHandle m_SSAO_DS;
	static grcDepthStencilStateHandle m_SSAO_Ptfx_DS;	
	static grcRasterizerStateHandle m_SSAO_R;

/*--------------------------------------------------------------------------------------------------*/
/* Multi-resolution SSAO data.																		*/
/*--------------------------------------------------------------------------------------------------*/

#if SUPPORT_MRSSAO
	// The depths for each LOD (LOD 0 is the depth buffer).
	static grcRenderTarget *m_Depths[MRSSAO_NUM_RESOLUTIONS];
	// The normals for each LOD (LOD 0 points to the G-buffer. Subsequent LODs are in view-space).
	static grcRenderTarget *m_Normals[MRSSAO_NUM_RESOLUTIONS];
	// The position x & y  each LOD (LOD 0 points to the NULL).
	static grcRenderTarget *m_PositionsX[MRSSAO_NUM_RESOLUTIONS];
	static grcRenderTarget *m_PositionsY[MRSSAO_NUM_RESOLUTIONS];

	// AO for each resolution(except the mip-map 0).
	static grcRenderTarget *m_AOs[MRSSAO_NUM_RESOLUTIONS];
	// Blurred AO for each resolution.
	static grcRenderTarget *m_BlurredAOs[MRSSAO_NUM_RESOLUTIONS];

	static int sm_MR_CurrentScenePreset;
	static int sm_MR_CurrentResolution;

#	if __BANK
	static int sm_MR_ScenePresetBeingEdited;
	static int sm_MR_ResolutionBeingEdited;
	static MR_SSAO_Parameters sm_Clipboard_Params;
	static MR_SSAO_ScenePreset sm_Clipboard_ScenePreset;
#	endif //__BANK

	// The current parameters to use.
	static MR_SSAO_Parameters sm_MR_CurrentSettings;
	// Scene "presets".
	static MR_SSAO_AllScenePresets sm_MR_ScenePresets;
#endif //SUPPORT_MRSSAO

/*--------------------------------------------------------------------------------------------------*/
/* QS-SSAO and HDAO data.																		*/
/*--------------------------------------------------------------------------------------------------*/
	static QS_SSAO_ScenePreset	sm_QS_Settings;
	static QS_SSAO_Parameters	sm_QS_CurrentSettings;
#if __BANK
	static int					sm_QS_ResolutionBeingEdited;
	static int					sm_QS_CurrentResolution;
#endif	//__BANK

	static CPQSMix_Parameters	sm_CPQSMix_CurrentSettings;

#if SUPPORT_HDAO
	static HDAO_ScenePreset		sm_HDAO_Settings;
	static HDAO_Parameters		sm_HDAO_CurrentSettings;
	static int					sm_HDAO_CurrentQualityLevel;
#	if __BANK
	static int					sm_HDAO_QualityLevelBeingEdited;
#	endif	//__BANK
#endif	//SUPPORT_HDAO

#if SUPPORT_HDAO
	static HDAO2_ScenePreset	sm_HDAO2_Settings;
	static HDAO2_Parameters		sm_HDAO2_CurrentSettings;
	static int					sm_HDAO2_CurrentQualityLevel;
#	if __BANK
	static int					sm_HDAO2_QualityLevelBeingEdited;
#	endif	//__BANK
#endif	//SUPPORT_HDAO

#if SUPPORT_HBAO
	static	HBAO_Parameters		sm_HBAO_CurrentSettings; 
#endif
};

#endif //__SSAO_H__...


