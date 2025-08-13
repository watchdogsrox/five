//
// Screen Space Ambient Occlusion main file:
//
// 2010/05/13 - initial, born from split with DeferredLightingHelper; 
//

// rage
#if __BANK
	#include "bank/bkmgr.h"
#endif
#if __PS3
	#include "grcore/wrapper_gcm.h"
	#include "grcore/texturegcm.h"
#endif
#if __XENON
	#include "grcore/wrapper_d3d.h"
#endif
#if RSG_ORBIS
	#include "grcore/texture_gnm.h"
	#include "grcore/rendertarget_gnm.h"
#endif
#include "grcore/AA_shared.h"
#include "grcore/allocscope.h"
#include "grcore/effect_values.h"
#include "grcore/effect.h"
#include "grcore/quads.h"
#include "grcore/device.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grmodel/shader.h"
#include "system/xtl.h"
#include "system/task.h"
#include "system/memory.h"
#include "file/asset.h"
#include "bank/bank.h"

// game
#include "core/game.h"
#include "debug/TiledScreenCapture.h"
#include "debug/Rendering//DebugLighting.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/Rendering/DebugDeferred.h"
#include "renderer/renderTargets.h"
#include "renderer/water.h"
#include "renderer/lights/lights.h"
#include "renderer/SSAO.h"
#include "renderer/SSAO_shared.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Util/ShmooFile.h"
#include "vfx/particles/PtFxManager.h"
#include "system/system.h"
#include "timecycle/TimeCycle.h"
#include "system/SettingsManager.h"

/*
===================================================================================================
Acronyms:
QSSSAO - Quarter Screen SSAO
CPSSAO – Channel Packed SSAO - Samples 4 AO pixels in one pass. Fast on the 360.
PMSSAO – Position Mapped SSAO - Use camera space position buffer and Gbuffer normals. Fast on the PS3 (PC?).
MRSSAO – Multi Resolution SSAO
HDAO – High Definition Ambient Occlusion, based on AMD sample
HDAO2 - Updated HDAO, using compute shaders
===================================================================================================
*/

#if RSG_ORBIS || RSG_DURANGO
#define SSAO_FORCE_TECHNIQUE	ssaotechnique_hbao
#endif // RSG_ORBIS || RSG_DURANGO

// ----------------------------------------------------------------------------------------------- //
// VARIABLES
// ----------------------------------------------------------------------------------------------- //

#if __PS3
#include "SPU/SSAOSPU.h"
#endif

grmShader* SSAO::m_shaderSSAO = NULL;
#if GENERATE_SHMOO
static int					SSAOShmoo;
#else 
#define						SSAOShmoo (-1)
#endif

grcEffectTechnique SSAO::m_techniqueSSAO;
grcEffectTechnique m_OffsetTechnique;
grcEffectVar SSAO::m_variables[ssao_variables_count];

#if SUPPORT_HBAO
namespace hbao
{
	static float fAngBias						= 32.0f;

	static float fTemporalThreshold				= 0.1f;	
	static float fContinuityThreshold			= 0.1f;
	
	static float fHBAOMinMulSwitch				= 1.0f;

	static bool	 bIsTemporalFirstUsed			= true;	
	static Mat44V	matPrevViewProj;

	grcRenderTarget* HistoryRT = NULL;
	grcRenderTarget* ContinuityRT[2] = { NULL };

	//optional stuff
	static bool bIsTemporalEnabled			= true;
	static bool bIsHybridEnabled			= true;
	static bool bIsUseNormalsEnabled		= false;
}
#endif //SUPPORT_HBAO

#if SUPPORT_HDAO
#define HDAO_PROFILE_SIZE	0
namespace hdao
{
	static grmShader* shader = NULL;
	static grcEffectTechnique technique, techniqueApply;
	// shader variables
	static grcEffectVar
		varDepthTexture, varNormalTexture, varOcclusionTexture,
		varProjectionParams, varProjectionShear,
		varTargetSize, varBlurDirection,
		varComputeParams, varApplyParams, varValleyParams, varExtraParams, varOcclusionTextureParams;
	// major parameters
	static float fBlurThreshold = 0.f;
	static int iBlur = 0;
#	if __BANK
	// minor parameters
	static float fDotOffset, fDotScale, fDotPower;
#	endif	//__BANK
#	if HDAO_PROFILE_SIZE
	static grcGpuTimer gpuTimer;
	static int nFramesAveraging = 50;
	static int nCurrentFrame = 0;
	static float fTimeComputeStore[HDAO_PROFILE_SIZE];
	static float fTimeApplyStore[HDAO_PROFILE_SIZE];
	static float fTimeComputeAvg = 0.f;
	static float fTimeApplyAvg = 0.f;
#	endif	//HDAO_PROFILE_SIZE
}
#endif	//SUPPORT_HDAO

#if SUPPORT_HDAO2
namespace hdao2
{
	static grmShader* shader;
	static grcEffectTechnique technique;
	grcRenderTarget* Normal;
	grcRenderTarget* Depth;
	grcRenderTarget* ResultAO;
	grcRenderTarget* TempAO1;
	grcRenderTarget* TempAO2;
	grcRenderTarget* GBuffer1_Resolved;

	static grcEffectVar
		varResultTexture, varOcclusionTexture,
		varDepthTexture, varNormalTexture,
		varOrigDepthTexture, varOrigNormalTexture,
		varParams1, varParams2,
		varProjectionParams, varProjectionShear, varTargetSize, varOrigDepthTexParams;
}
#endif	//SUPPORT_HDAO2

grcRenderTarget* m_PositionRT;


#if __PS3
#define NUMSSAOBUFFERS (3)
#else
#define NUMSSAOBUFFERS (2)
#endif

grcRenderTarget* m_SSAORT[NUMSSAOBUFFERS];
#if RSG_DURANGO && SSAO_USE_ESRAM_TARGETS
grcRenderTarget* m_SSAORT_ESRAM[NUMSSAOBUFFERS];
#endif // RSG_DURANGO && SSAO_USE_ESRAM_TARGETS
grcRenderTarget* m_CPQSMixFullScreenRT = NULL;

#if SSAO_USE_LINEAR_DEPTH_TARGETS
grcRenderTarget* m_FullScreenLinearDepthRT = NULL;
#endif // SSAO_USE_LINEAR_DEPTH_TARGETS

float			SSAO::m_strength		= 3.0f;
bool			SSAO::m_enable			= true;

static ssao_technique	m_CurrentSSAOTechnique	= ssaotechnique_qsssao;
static ssao_pass		m_SSAO_downscalePass	= ssao_downscale;

#if __BANK
bool			SSAO::m_TechniqueOverrideEnable	= false;
ssao_technique	SSAO::m_ForcedSSAOTechnique		= ssaotechnique_qsssao;
bool			SSAO::m_isolate					= false;
bool			SSAO::m_strengthOverrideEnable	= false;
float			SSAO::m_strengthOverride		= 3.0f;


static bkGroup *s_QS_BankGroup;
#if SUPPORT_MRSSAO
static bkGroup *s_MRSSAO_BankGroup;
#endif
#if SUPPORT_HDAO
static bkGroup *s_HDAO_BankGroup;
#endif
#if SUPPORT_HDAO2
static bkGroup *s_HDAO2_BankGroup;
#endif
#if SUPPORT_HBAO
static bkGroup *s_HBAO_BankGroup;
#endif

static const char *m_SSAOTechniqueNames[] = 
{
	"QSSSAO",
	"CPSSAO",
	"PMSSAO",
	"CP-QR SSAO mix",
	"CP-QR SSAO mix (4 Dir)",
	"CP-QR SSAO mix (8 Dir)",
#if __D3D11 || RSG_ORBIS
	"MRSSAO",
	"HDAO",
	"HDAO2",
	"QSSSAO-enhanced",
#if SUPPORT_HBAO
	"HBAO"
#endif // SUPPORT_HBAO
#endif	//__D3D11 || RSG_ORBIS
};
CompileTimeAssert(NELEM(m_SSAOTechniqueNames) == ssaotechnique_count);

const char** SSAO::GetSSAOTechniqueNames() { return m_SSAOTechniqueNames; }

#endif //__BANK

grcRenderTarget*	m_OffsetRT		= NULL;
grcTexture*	m_OffsetTexture			= NULL;

#if RSG_PC
bool m_DelayInitializeOffsetTexture = false;
#endif

#if __PS3
bool	SSAO::m_enableSpu			= false && SPUPMMGR_PS3;
bool	SSAO::m_enableSpuRT			= false;
float	SSAO::m_SpuTimeTotal		= 0.0f;
float	SSAO::m_SpuTimePerSpu		= 0.0f;
int		SSAO::m_SpuNumUnitsUtilized = 0;
#endif

#if RSG_PC
bool SSAO::m_Initialized = false;
bool SSAO::m_useNextGenVersion = true;
#endif

#if SPUPMMGR_PS3
	grcSpuDestTexture SSAO::spuGBufferSSAO_0;
#endif

grcBlendStateHandle			SSAO::m_SSAO_B;
grcBlendStateHandle			SSAO::m_Apply_B;
PS3_ONLY(grcDepthStencilStateHandle	m_Foliage_DS);
grcDepthStencilStateHandle	SSAO::m_SSAO_DS;
grcRasterizerStateHandle	SSAO::m_SSAO_R;

	QS_SSAO_ScenePreset	SSAO::sm_QS_Settings;
	QS_SSAO_Parameters	SSAO::sm_QS_CurrentSettings;
	CPQSMix_Parameters	SSAO::sm_CPQSMix_CurrentSettings;

#if SUPPORT_HDAO
	HDAO_ScenePreset	SSAO::sm_HDAO_Settings;
	HDAO_Parameters		SSAO::sm_HDAO_CurrentSettings;
#endif	//SUPPORT_HDAO
#if SUPPORT_HDAO2
	HDAO2_ScenePreset	SSAO::sm_HDAO2_Settings;
	HDAO2_Parameters	SSAO::sm_HDAO2_CurrentSettings;
#endif	//SUPPORT_HDAO2
#if SUPPORT_HBAO
	HBAO_Parameters		SSAO::sm_HBAO_CurrentSettings;
#endif

#if __BANK
	int					SSAO::sm_QS_ResolutionBeingEdited;
	int					SSAO::sm_QS_CurrentResolution;
#endif	//__BANK

#if SUPPORT_HDAO
	int					SSAO::sm_HDAO_CurrentQualityLevel;
#	if __BANK
	int					SSAO::sm_HDAO_QualityLevelBeingEdited;
#	endif	//__BANK
#endif	//SUPPORT_HDAO

#if SUPPORT_HDAO2
	int					SSAO::sm_HDAO2_CurrentQualityLevel;
#	if __BANK
	int					SSAO::sm_HDAO2_QualityLevelBeingEdited;
#	endif	//__BANK
#endif	//SUPPORT_HDAO


PARAM(qsssao_res,"Which QS-SSAO res to use(0 = low, 1 = med, 2 = high).");
#if SUPPORT_HDAO
PARAM(hdao_quality,"Which HDAO quality level to use(0 = low, 1 = med, 2 = high).");
#endif

// ----------------------------------------------------------------------------------------------- //
// HELPER FUNCTIONS
// ----------------------------------------------------------------------------------------------- //

static Vector4 CalculateProjectionParams_PreserveTanFOV()
{
	Vector4 projParams = ShaderUtil::CalculateProjectionParams();
#if __BANK
	if (TiledScreenCapture::IsEnabled() && TiledScreenCapture::GetSSAOScale() >= 0.0f)
	{
		projParams.y = TiledScreenCapture::GetSSAOScale()/sqrtf(3.0f);//tanf(DtoR*0.5f*60.0f);
		projParams.x = projParams.y*(float)GRCDEVICE.GetWidth()/(float)GRCDEVICE.GetHeight();
	}
#endif // __BANK
	return projParams;
}

static int GetSSAOPass(int pass
#if SUPPORT_QSSSAO2
	,bool enhanced = false
#endif	//SUPPORT_QSSSAO2
	)
{
	int newPass = pass;

#if __BANK
#	if SUPPORT_QSSSAO2
	if (enhanced)
	{
		switch (pass)
		{
		case ssao_process			: newPass = ssao_process_enhanced; break;
		case ssao_blur_bilateral_x	: newPass = ssao_blur_bilateral_enhanced; break;
		case ssao_blur_bilateral_y	: newPass = ssao_blur_bilateral_enhanced; break;
		}
	}
#	endif //SUPPORT_QSSSAO2

	if (TiledScreenCapture::IsEnabledOrthographic())
	{
		switch (pass)
		{
		case ssao_downscale								: newPass = ssao_downscale_ortho; break;
#if RSG_PC
		case ssao_downscale_sm50						: newPass = ssao_downscale_ortho_sm50; break;
#endif
		case ssao_linearizedepth						: newPass = ssao_linearizedepth_ortho; break;
		case cpssao_downscale							: newPass = cpssao_downscale_ortho; break;
		case cpssao_process								: newPass = cpssao_process_ortho; break;
		case pmssao_downscale							: newPass = pmssao_downscale_ortho; break;
		case pmssao_process								: newPass = pmssao_process_ortho; break;
		case cpqsmix_qs_ssao_process					 : grcAssertf(0, "cpqsmix_qs_ssao_process...Not supported yet."); break;
		case cpqsmix_cp_ssao_process_and_combine_with_qs : grcAssertf(0, "cpqsmix_cp_ssao_process_and_combine_with_qs...Not supported yet."); break;
#	if SUPPORT_MRSSAO
		case mrssao_downscale_from_gbuffer				: newPass = mrssao_downscale_from_gbuffer_ortho; break;
		case mrssao_compute_ao_from_gbuffer				: newPass = mrssao_compute_ao_from_gbuffer_ortho; break;
		case mrssao_compute_ao_from_gbuffer_and_combine	: newPass = mrssao_compute_ao_from_gbuffer_and_combine_ortho; break;
#	endif //SUPPORT_MRSSAO
		}
	}
#elif RSG_PC || RSG_DURANGO
	enhanced;
#endif // __BANK

	return newPass;
}

inline int GetHDAOPass(int pass)
{
	// TODO -- support orthographic for HDAO
	return pass;
}

template<class T,unsigned N>
int ChooseResolution(const T (&resolutions)[N], unsigned Width, unsigned Height)	{
	float bestDistance = 0.f;
	unsigned bestIndex = N;
	const Vector2 resVector( (float)Width, (float)Height );

	for (unsigned i=0; i!=N; ++i)	{
		const float curDistance = resVector.Dist2( resolutions[i].m_Dim );
		if (!i || bestDistance > curDistance)
		{
			bestIndex = i;
			bestDistance = curDistance;
		}
	}
	return bestIndex;
}

template<int N>
static int findString(const char *const str, const char *const (&strArray)[N])	{
	int id;
	for(id = 0; id<N; ++id)	{
		if (!strcmp(strArray[id],str))
			return id;
	}
	Assertf(id<N,"Unable to find string %s",str);
	return 0;
}

#if __BANK
void SSAO::OnComboBox_OverrideTechnique(void)
{
	//TODO: close groups if possible
	//s_QS_BankGroup	->SetClosed();
	//s_MRSSAO_BankGroup->SetClosed();
	//s_HDAO_BankGroup	->SetClosed();
}

void SSAO::SaveParams_QS(void)
{
	sm_QS_Settings.m_Resolutions[sm_QS_CurrentResolution] = sm_QS_CurrentSettings;
	PARSER.SaveObject("platform:/data/QS_SSAOSettings", "xml", &sm_QS_Settings, parManager::XML);
}
void SSAO::LoadParams_QS(void)
{
	PARSER.LoadObject("platform:/data/QS_SSAOSettings", "xml", sm_QS_Settings);
	SSAO::UpdateSettingsToUse_QS(sm_QS_ResolutionBeingEdited);
}
void SSAO::OnComboBox_QS(void)
{
	sm_QS_Settings.m_Resolutions[sm_QS_CurrentResolution] = sm_QS_CurrentSettings;
	SSAO::UpdateSettingsToUse_QS(sm_QS_ResolutionBeingEdited);
}
#endif	//__BANK

void SSAO::UpdateSettingsToUse_QS(int Resolution)
{
	BANK_ONLY( sm_QS_CurrentResolution = Resolution );
	sm_QS_CurrentSettings = sm_QS_Settings.m_Resolutions[Resolution];
}

#if __BANK

void SSAO::SaveParams_CPQSMix(void)
{
	PARSER.SaveObject("common:/data/CPQSMix_SSAOSettings", "xml", &sm_CPQSMix_CurrentSettings, parManager::XML);
}

#endif //__BANK

void SSAO::LoadParams_CPQSMix(void)
{
	PARSER.LoadObject("common:/data/CPQSMix_SSAOSettings", "xml", sm_CPQSMix_CurrentSettings);
}

// ----------------------------------------------------------------------------------------------- //
// CLASS FUNCTIONS
// ----------------------------------------------------------------------------------------------- //

void SSAO::InitializeOffsetsTexture()
{
	struct OffsetTexturePixel
	{
		Float16 x;
		Float16 y;
	};

	grcTextureLock lock;
	if (m_OffsetTexture->LockRect(0, 0, lock, grcsWrite | grcsAllowVRAMLock))
	{
		OffsetTexturePixel* offsetPixels = (OffsetTexturePixel*)lock.Base;

		for(s32 i = 0; i < 4*4; i++)
		{
			OffsetTexturePixel pixel;
			float angle = ((float)i + 0.45f)*3.14f*0.5f*3.14f;
			pixel.x = Float16((float)rage::Sinf(angle));
			pixel.y = Float16((float)rage::Cosf(angle));
			offsetPixels[i] = pixel;
		}
		m_OffsetTexture->UnlockRect(lock);
	}
}

#define SSAODitherSize 32
#define SSOAPositionSize 16

#if RSG_PC
bool SSAO::UseNextGeneration(const Settings &settings)
{
	// HB
	return	settings.m_graphics.m_SSAO == CSettings::High ||
		(	settings.m_graphics.m_SSAO == CSettings::Medium && settings.m_graphics.m_MSAA);
}
#endif //RSG_PC

void SSAO::CreateRenderTargets()
{
#if RSG_PC
	m_Initialized = true;
	m_useNextGenVersion = UseNextGeneration(CSettingsManager::GetInstance().GetSettings());
	m_enable = CSettingsManager::GetInstance().GetSettings().m_graphics.m_SSAO != CSettings::Low;

	if (!m_enable) return;
#endif //RSG_PC

	grcTextureFactory::CreateParams params;
	params.HasParent				= true;
	params.Parent					= GBuffer::GetTarget(GBUFFER_RT_3);
	params.AllocateFromPoolOnCreate	= false;
#if __XENON
	params.PoolHeap					= kSSAOWaterHeap;
	params.PoolID					= CRenderTargets::GetRenderTargetPoolID(XENON_RTMEMPOOL_GENERAL1);
#endif

	params.Format			= grctfG16R16F;
	m_OffsetRT				= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO Dither",
		grcrtPermanent,
		SSAODitherSize,
		SSAODitherSize,
		32,
		&params);

#if __XENON
	params.Format			= grctfA16B16G16R16F_NoExpand;
#else
	params.Format			= grctfA16B16G16R16F;
#endif //__XENON

	m_PositionRT			= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO Position",
		grcrtPermanent,
		SSOAPositionSize,//not suppose to be using this target yet, pool first
		SSOAPositionSize,//not suppose to be using this target yet, pool first
		64, 
		&params);

	s32 width	= VideoResManager::GetSceneWidth();
	s32 height	= VideoResManager::GetSceneHeight();

#if __XENON

	params.Format			= grctfL8;
	m_SSAORT[0]				= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL1, "SSAO", grcrtPermanent, width/2, height/2, 8, &params, kSSAOWaterHeap);

	params.Format			= grctfG16R16;
	m_SSAORT[1]				= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL1, "SSAO Temp", grcrtPermanent, width/2, height/2, 32, &params, kTiledLightingHeap3);
	m_SSAORT[1]->AllocateMemoryFromPool();

#elif __PS3

	params.Format			= grctfL8;
	m_SSAORT[0]				= grcTextureFactory::GetInstance().CreateRenderTarget("SSAO 0", grcrtPermanent, width/2, height/2, 8, &params);

	params.Format			= grctfG16R16;
	params.AllocateFromPoolOnCreate = false;
	m_SSAORT[1]				= CRenderTargets::CreateRenderTarget(	PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED,	"SSAO Temp 0", 
																	grcrtPermanent, width/2,	height/2, 32, &params, 9);
	m_SSAORT[1]->AllocateMemoryFromPool();

	m_SSAORT[2]				= CRenderTargets::CreateRenderTarget(	PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED, "SSAO Temp 1",
																	grcrtPermanent, width/2,	height/2, 32, &params, 9);
	m_SSAORT[2]->AllocateMemoryFromPool();

#if SPUPMMGR_PS3
	spuGBufferSSAO_0.SetTexture(m_SSAORT[0]);
#endif //SPUPMMGR_PS3

#else // !__XENON & !__PS3
	
#if __WIN32PC
	params.StereoRTMode = grcDevice::STEREO;
#endif

	params.Format			= grctfL8;
	m_SSAORT[0]				= grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO 0", grcrtPermanent, width/2, height/2, 8, &params);
	params.IsSwizzled		= true;
	m_SSAORT[1]				= grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO 1", grcrtPermanent, width/2, height/2, 8, &params);

#if RSG_DURANGO && SSAO_USE_ESRAM_TARGETS
	grcTextureFactory::CreateParams params_ESRAM = params;
	params_ESRAM.IsSwizzled = false;
	params_ESRAM.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_LIGHTING_0;
#if SUPPORT_HBAO
	m_SSAORT_ESRAM[0]		= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO 0", grcrtPermanent, width/2, height/2, 8, &params_ESRAM);
	m_SSAORT_ESRAM[1]		= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO 1", grcrtPermanent, width/2, height/2, 8, &params_ESRAM);
#else
	m_SSAORT_ESRAM[0]		= grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO 0", grcrtPermanent, width/2, height/2, 8, &params_ESRAM);
	m_SSAORT_ESRAM[1]		= grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO 1", grcrtPermanent, width/2, height/2, 8, &params_ESRAM);
#endif
#endif // RSG_DURANGO && SSAO_USE_ESRAM_TARGETS

#if __WIN32PC
	params.StereoRTMode = grcDevice::AUTO;
#endif	
	//------------------------------------------------------------------------------------------//

#endif //__XENON elif __PS3

#if SUPPORT_HBAO
	m_CPQSMixFullScreenRT	= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(	"CP/QS mix SSAO final", grcrtPermanent, width, height, 8, &params);
#else
	m_CPQSMixFullScreenRT	= grcTextureFactory::GetInstance().CreateRenderTarget(	"CP/QS mix SSAO final", grcrtPermanent, width, height, 8, &params);
#endif
	

#if SSAO_USE_LINEAR_DEPTH_TARGETS
	params.Format				= grctfR32F;
#if RSG_DURANGO && SSAO_USE_ESRAM_TARGETS
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_LIGHTING_0;
#endif // RSG_DURANGO && SSAO_USE_ESRAM_TARGETS

#if SUPPORT_HBAO
	m_FullScreenLinearDepthRT	= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget( "SSAO Full Screen linear", grcrtPermanent, width, height, 32, &params);
#else
	m_FullScreenLinearDepthRT	= grcTextureFactory::GetInstance().CreateRenderTarget( "SSAO Full Screen linear", grcrtPermanent, width, height, 32, &params);
#endif

#if RSG_DURANGO
	params.ESRAMPhase = 0;
#endif // RSG_DURANGO

#endif // SSAO_USE_LINEAR_DEPTH_TARGETS

	//------------------------------------------------------------------------------------------//
	BANK_ONLY(grcTexture::SetCustomLoadName("SSAO_OffsetTexture");)
#if __PS3 || __XENON
	grcImage* offsetImage = grcImage::Create(4, 4, 1, grcImage::G16R16F, grcImage::STANDARD, 0, 0);
	m_OffsetTexture = grcTextureFactory::GetInstance().Create(offsetImage);
#else
	grcTextureFactory::TextureCreateParams offsetParams(grcTextureFactory::TextureCreateParams::SYSTEM, grcTextureFactory::TextureCreateParams::LINEAR, grcsRead | grcsWrite);
	m_OffsetTexture = grcTextureFactory::GetInstance().Create(4, 4, grctfG16R16F, NULL, 1U, &offsetParams);
#endif
	BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)

#if !RSG_PC
	InitializeOffsetsTexture();
#else
	m_DelayInitializeOffsetTexture = true;
#endif
}

void SSAO::DeleteRenderTargets()
{
	delete m_OffsetRT; m_OffsetRT = NULL;
	delete m_PositionRT; m_PositionRT = NULL;


#if SSAO_USE_LINEAR_DEPTH_TARGETS
	if(m_FullScreenLinearDepthRT)
	{
		delete m_FullScreenLinearDepthRT;
		m_FullScreenLinearDepthRT = NULL;
	}
#endif// SSAO_USE_LINEAR_DEPTH_TARGETS

	if(m_CPQSMixFullScreenRT)
	{
		delete m_CPQSMixFullScreenRT;
		m_CPQSMixFullScreenRT = NULL;
	}

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	delete m_SSAORT[0]; m_SSAORT[0] = NULL;
	delete m_SSAORT[1]; m_SSAORT[1] = NULL;
#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS

	//------------------------------------------------------------------------------------------//

	SafeRelease(m_OffsetTexture);
}

void SSAO::ResetShaders()
{
	delete m_shaderSSAO;
	InitShaders();
}

void SSAO::InitShaders()
{
	// Create and load the shader.
	ASSET.PushFolder("common:/shaders");
	m_shaderSSAO = grmShaderFactory::GetInstance().Create();
	m_shaderSSAO->Load(
#if DEVICE_MSAA
		GRCDEVICE.GetMSAA()>1 ? "SSAO_MS" :  // Use the non-msaa version of the shader for 4s1f EQAA
#endif
		"SSAO");

	GENSHMOO_ONLY(SSAOShmoo = )ShmooHandling::Register("SSAO",m_shaderSSAO, false); // 1 technique : passcount, 1 technique 1 pass.
	ASSET.PopFolder();

	m_techniqueSSAO		= m_shaderSSAO->LookupTechnique("SSAO");
	m_OffsetTechnique	= m_shaderSSAO->LookupTechnique("offset");

#if RSG_PC
	u32	shaderModelMajor, shaderModelMinor;

	GRCDEVICE.GetDxShaderModel(shaderModelMajor, shaderModelMinor);
	if(shaderModelMajor == 5)
	{
		m_SSAO_downscalePass = ssao_downscale_sm50;
	}
#endif

	// Variables
	m_variables[ssao_project_params]			= m_shaderSSAO->LookupVar("projectionParams");
	m_variables[ssao_normaloffset]				= m_shaderSSAO->LookupVar("NormalOffset");
	m_variables[ssao_offsetscale0]				= m_shaderSSAO->LookupVar("OffsetScale0");
	m_variables[ssao_offsetscale1]				= m_shaderSSAO->LookupVar("OffsetScale1");
	m_variables[ssao_strength]					= m_shaderSSAO->LookupVar("SSAOStrength");
	m_variables[ssao_cpqsmix_qs_fadein]			= m_shaderSSAO->LookupVar("CPQSMix_QSFadeIn");
	m_variables[ssao_offsettexture]				= m_shaderSSAO->LookupVar("OffsetTexture");
	m_variables[ssao_deferredlighttexture1]		= m_shaderSSAO->LookupVar("deferredLightTexture1");
	m_variables[ssao_deferredlighttexture0p]	= m_shaderSSAO->LookupVar("deferredLightTexture0P");
	m_variables[ssao_gbuffertexture2]			= m_shaderSSAO->LookupVar("gbufferTexture2");
	m_variables[ssao_gbuffertexturedepth]		= m_shaderSSAO->LookupVar("gbufferTextureDepth");
	m_variables[ssao_deferredlighttexture2]		= m_shaderSSAO->LookupVar("deferredLightTexture2");
	m_variables[ssao_deferredlighttexture]		= m_shaderSSAO->LookupVar("deferredLightTexture");
	m_variables[ssao_deferredlightscreensize]	= m_shaderSSAO->LookupVar("deferredLightScreenSize");
	m_variables[ssao_pointtexture1]				= m_shaderSSAO->LookupVar("PointTexture1");
	m_variables[ssao_pointtexture2]				= m_shaderSSAO->LookupVar("PointTexture2");
	m_variables[ssao_lineartexture1]			= m_shaderSSAO->LookupVar("LinearTexture1");
	m_variables[ssao_pointtexture4]				= m_shaderSSAO->LookupVar("PointTexture4");
	m_variables[ssao_pointtexture5]				= m_shaderSSAO->LookupVar("PointTexture5");
	m_variables[ssao_msaa_pointtexture1]		= m_shaderSSAO->LookupVar("MSAAPointTexture1");
	m_variables[ssao_msaa_pointtexture2]		= m_shaderSSAO->LookupVar("MSAAPointTexture2");
	m_variables[ssao_msaa_pointtexture1_dim]	= m_shaderSSAO->LookupVar("MSAAPointTexture1_Dim");
	m_variables[ssao_msaa_pointtexture2_dim]	= m_shaderSSAO->LookupVar("MSAAPointTexture2_Dim");

#if SSAO_USEDEPTHRESOLVE
	m_variables[depth_resolve_texture]			= m_shaderSSAO->LookupVar("DepthResolveTexture");
#endif // SSAO_USEDEPTHSAMPLE0

#if SUPPORT_HBAO
	InitShaders_HBAO();
#endif
}

void SSAO::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		sysMemUseMemoryBucket b(MEMBUCKET_RENDER);

		InitShaders();

		//------------------------------------------------------------------------------------------//
		CreateRenderTargets();

		// Setup state blocks
		grcBlendStateDesc ApplyBlendState;
#if __PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS
		ApplyBlendState.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RED + grcRSV::COLORWRITEENABLE_GREEN;
		ApplyBlendState.BlendRTDesc[GBUFFER_RT_0].BlendEnable = TRUE;
		ApplyBlendState.BlendRTDesc[GBUFFER_RT_0].DestBlend = grcRSV::BLEND_SRCALPHA;
		ApplyBlendState.BlendRTDesc[GBUFFER_RT_0].SrcBlend = grcRSV::BLEND_ZERO;
#else
		ApplyBlendState.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
#endif //__PS3
		m_Apply_B = grcStateBlock::CreateBlendState(ApplyBlendState);

		grcBlendStateDesc SSAOBlendState;
		SSAOBlendState.BlendRTDesc[GBUFFER_RT_0].BlendEnable = TRUE;
		SSAOBlendState.BlendRTDesc[GBUFFER_RT_0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		SSAOBlendState.BlendRTDesc[GBUFFER_RT_0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		m_SSAO_B = grcStateBlock::CreateBlendState(SSAOBlendState);

#if __PS3
		grcDepthStencilStateDesc foliageDSDesc;
		foliageDSDesc.DepthEnable					= true;
		foliageDSDesc.DepthWriteMask				= false;
		foliageDSDesc.DepthFunc						= grcRSV::CMP_LESS;//DEFAULT
		foliageDSDesc.StencilEnable					= true;
		foliageDSDesc.StencilWriteMask				= 0xFF;//DEFAULT
		foliageDSDesc.StencilReadMask				= 0xFF;//DEFAULT

		foliageDSDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_INVERT;
		foliageDSDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;//DEFAULT
		foliageDSDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_KEEP;
		foliageDSDesc.FrontFace.StencilFunc			= grcRSV::CMP_GREATEREQUAL;
		foliageDSDesc.BackFace						= foliageDSDesc.FrontFace;
		m_Foliage_DS = grcStateBlock::CreateDepthStencilState(foliageDSDesc, "m_Foliage_DS");
#endif //__PS3

		grcDepthStencilStateDesc SSAODepthStencilState;
		SSAODepthStencilState.DepthWriteMask = FALSE;
		SSAODepthStencilState.DepthEnable = FALSE;
		SSAODepthStencilState.DepthFunc = grcRSV::CMP_LESS;
		m_SSAO_DS = grcStateBlock::CreateDepthStencilState(SSAODepthStencilState);

		grcRasterizerStateDesc SSAORasterizerState;
		SSAORasterizerState.CullMode = grcRSV::CULL_NONE;
		m_SSAO_R = grcStateBlock::CreateRasterizerState(SSAORasterizerState);
	}

	int QS_Resolution = 1;
	PARAM_qsssao_res.Get(QS_Resolution);

	if((QS_Resolution < 0) || (QS_Resolution >= QS_SSAO_CONFIG_MAX_RESOLUTIONS))
	{
		AssertMsg(false, "SSAO::Init()...Resolution out of range!");
		QS_Resolution = 0;
	}
	BANK_ONLY( sm_QS_ResolutionBeingEdited = QS_Resolution );

	// Load CPQS-mix settings.
	LoadParams_CPQSMix();

#if RSG_PC
	PARSER.LoadObject("platform:/data/QS_SSAOSettings", "xml", sm_QS_Settings);
#endif	//RSG_PC
	UpdateSettingsToUse_QS(QS_Resolution);

#if SUPPORT_MRSSAO
	if (initMode == INIT_CORE)
	{
		Init_MRSSAO();
	}
#endif	//SUPPORT_MRSSAO
#if SUPPORT_HDAO
	Init_HDAO(initMode == INIT_CORE);
#endif
#if SUPPORT_HDAO2
	Init_HDAO2(initMode == INIT_CORE);
#endif
#if SUPPORT_HBAO
	Init_HBAO(initMode == INIT_CORE);
#endif	
}

// ----------------------------------------------------------------------------------------------- //

void SSAO::Shutdown(unsigned shutdownMode)
{
	const bool bCore = shutdownMode == SHUTDOWN_CORE;
#if SUPPORT_MRSSAO
	Shutdown_MRSSAO();
#endif	//SUPPORT_MRSSAO
#if SUPPORT_HDAO
	Shutdown_HDAO(bCore);
#endif	//SUPPORT_HDAO
#if SUPPORT_HDAO2
	Shutdown_HDAO2(bCore);
#endif	//SUPPORT_HDAO2

	if (bCore)
	{
		DeleteRenderTargets();

		delete m_shaderSSAO;
		m_shaderSSAO = NULL;
	}
}

// ----------------------------------------------------------------------------------------------- //

void SSAO::Update()
{

}

#if RSG_PC
void SSAO::DeviceLost()
{
	if (m_Initialized)
	{
		DestroyRendertargets_HDAO2();
		DestroyRendertargets_MRSSAO();
#if SUPPORT_HBAO
		DestroyRendertargets_HBAO();
#endif		
		DeleteRenderTargets();
	}
}

void SSAO::DeviceReset()
{
	if (m_Initialized)
	{
		CreateRenderTargets();
		CreateRendertargets_MRSSAO();
		CreateRendertargets_HDAO2();
#if SUPPORT_HBAO
		CreateRendertargets_HBAO();
#endif
	}
}
#endif	//RSG_PC

// ----------------------------------------------------------------------------------------------- //

#if __PS3

#if __DEV
	static bool s_SpuTimeDB=false;
	static u32  s_SpuTime0[4] ;	// [0-3]=SPU0/1/2/3 overall
	static u32  s_SpuTime1[4] ;
#endif

//
//
// SPU SSAO
//
void SSAO::ProcessSPU()
{

SSAOSpuParams ssaoParams;

	// kick off SPU job:
	const grcViewport *vp = grcViewport::GetCurrent();
	ssaoParams.m_px = vp->GetTanHFOV();
	ssaoParams.m_py = vp->GetTanVFOV();

	ssaoParams.m_ssaoStrength = m_strength;

#if __DEV
	// SSAO time stats in microseconds
	u32 spuTimeUs[4];
	if(s_SpuTimeDB)
	{
		spuTimeUs[0]	= cellAtomicStore32(&s_SpuTime1[0], 0);
		spuTimeUs[1]	= cellAtomicStore32(&s_SpuTime1[1], 0);
		spuTimeUs[2]	= cellAtomicStore32(&s_SpuTime1[2], 0);
		spuTimeUs[3]	= cellAtomicStore32(&s_SpuTime1[3], 0);
		ssaoParams.m_spuProcessingTime = &s_SpuTime1[0];
	}
	else
	{
		spuTimeUs[0]	= cellAtomicStore32(&s_SpuTime0[0], 0);
		spuTimeUs[1]	= cellAtomicStore32(&s_SpuTime0[1], 0);
		spuTimeUs[2]	= cellAtomicStore32(&s_SpuTime0[2], 0);
		spuTimeUs[3]	= cellAtomicStore32(&s_SpuTime0[3], 0);
		ssaoParams.m_spuProcessingTime = &s_SpuTime0[0];
	}
	s_SpuTimeDB = !s_SpuTimeDB;
	#if __BANK
		// update every 8th frame:
		static u32 delay=8;
		if( !(--delay) )
		{
			delay=8;
			m_SpuTimeTotal				= float((spuTimeUs[0]+spuTimeUs[1]+spuTimeUs[2]+spuTimeUs[3])/1000.0f);
			m_SpuNumUnitsUtilized		= (spuTimeUs[0]?1:0) + (spuTimeUs[1]?1:0) + (spuTimeUs[2]?1:0) + (spuTimeUs[3]?1:0);
			if(m_SpuNumUnitsUtilized==0)
			{
				m_SpuNumUnitsUtilized = 1;
			}
			m_SpuTimePerSpu				= m_SpuTimeTotal / float(m_SpuNumUnitsUtilized);
		}
	#endif //__BANK...
#else  // __DEV
	ssaoParams.m_spuProcessingTime = NULL;
#endif //__DEV...


#if SPUPMMGR_PS3
	spuGBufferSSAO_0.WaitForSPU();

	CElfInfo* SSAOSpuElf = (CElfInfo*)spupmSSAOSPU;
	#if __DEV
		SSAOSpuElf = CSpuPmMgr::GetReloadedElf(RELOAD_SSAOSPU);
		if(SSAOSpuElf==NULL) { SSAOSpuElf = (CElfInfo*)spupmSSAOSPU; }
	#endif
	CSpuPmMgr::PostProcessTexture(SSAOSpuElf, 4, NULL,
		(void*)&ssaoParams, sizeof(ssaoParams),
		&spuGBufferSSAO_0, (grcRenderTargetGCM*)CRenderTargets::GetDepthBufferQuarterLinear(),
		0, 0, m_SSAORT[0]->GetWidth(), m_SSAORT[0]->GetHeight(), 4
		#if ENABLE_PIX_TAGGING
		,"SSAO_SPU"
		#endif
		);
#endif //SPUPMMGR_PS3...

} // end of ProcessSPU()...
#endif //__PS3...


// ----------------------------------------------------------------------------------------------- //

#if SSAO_USEDEPTHRESOLVE
void SSAO::ResolveDepthSample0()
{
#if SSAO_OUTPUT_DEPTH
	CRenderTargets::ResolveDepth(depthResolve_Sample0);
#else
	Errorf("Needs implementing if this is ever turned off again!");
#endif
	
}
#endif // SSAO_USEDEPTHSAMPLE0

void SSAO::CalculateAndApply(bool bDoCalculate, bool bDoApply)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if SSAO_USEDEPTHRESOLVE
	if(GRCDEVICE.GetMSAA()>1 WIN32PC_ONLY(&& CRenderPhaseCascadeShadows::IsNeedResolve()))
		ResolveDepthSample0();
#endif // SSAO_USEDEPTHSAMPLE0

	// Set strength from time-cycle.

	// DURANGO:- Possible optimization - create the quater target from the full screen linear (which is is ESRAM).


	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
	m_strength = currKeyframe.GetVar(TCVAR_LIGHT_SSAO_INTEN) * 0.25f;
	
#if defined(SSAO_FORCE_TECHNIQUE)
	m_CurrentSSAOTechnique = SSAO_FORCE_TECHNIQUE;
#elif (RSG_PC && __64BIT)
	if(m_useNextGenVersion == true)
		m_CurrentSSAOTechnique = ssaotechnique_hbao; // Use same as Durango/Orbis.
	else
		m_CurrentSSAOTechnique = (ssao_technique)(s32)(currKeyframe.GetVar(TCVAR_LIGHT_SSAO_TYPE) + 0.5f); // Use same as ps3/360.
#else
	m_CurrentSSAOTechnique = (ssao_technique)(s32)(currKeyframe.GetVar(TCVAR_LIGHT_SSAO_TYPE) + 0.5f);
#endif // defined(SSAO_FORCE_TECHNIQUE)

#if __BANK
	if (m_strengthOverrideEnable)
		m_strength = m_strengthOverride * 0.25f;

	if(m_TechniqueOverrideEnable)
		m_CurrentSSAOTechnique	= m_ForcedSSAOTechnique;

	if(m_CurrentSSAOTechnique == ssaotechnique_cpssao)//CPSSAO strength relative to the current technique
		m_strength	= m_strength/2.0f;
#endif // __BANK

	m_shaderSSAO->SetVar(m_variables[ssao_strength], m_strength);


	if(m_enable == false)
	{
		DownscaleDepthToQuarterTarget(); 
		return;
	}

	if(m_strength < 0.01f)
	{
		if (!((currKeyframe.GetVar(TCVAR_LIGHT_RAY_MULT) > 0.0f) || (currKeyframe.GetVar(TCVAR_LIGHT_RAY_UNDERWATER_MULT) > 0.0f)))
		{
			DownscaleDepthToQuarterTarget(); 
			return;
		}
	}

#if SSAO_MAKE_QUARTER_LINEAR_FROM_FULL_SCREEN_LINEAR
	// Only CPQS mix techniques create a full screen linearized depth.
	if((m_CurrentSSAOTechnique != ssaotechnique_cpqsmix) && (m_CurrentSSAOTechnique != ssaotechnique_cp4Dirqsmix) && (m_CurrentSSAOTechnique != ssaotechnique_cp8Dirqsmix))
#endif // SSAO_MAKE_QUARTER_LINEAR_FROM_FULL_SCREEN_LINEAR
		DownscaleDepthToQuarterTarget(); 

	switch(m_CurrentSSAOTechnique)
	{
	case ssaotechnique_qsssao:
#if SUPPORT_QSSSAO2
	case ssaotechnique_qsssao2:
#endif // SUPPORT_QSSSAO2
	case ssaotechnique_cpssao:
	case ssaotechnique_pmssao:
	case ssaotechnique_cpqsmix:
	case ssaotechnique_cp4Dirqsmix:
	case ssaotechnique_cp8Dirqsmix:
		{
			CalculateAndApply_Console(bDoCalculate, bDoApply);
			break;
		}
#if SUPPORT_MRSSAO
	case ssaotechnique_mrssao:
		{
			CalculateAndApply_MRSSAO(bDoCalculate, bDoApply);
			break;
		}
#endif	//SUPPORT_MRSSAO
#if SUPPORT_HDAO
	case ssaotechnique_hdao:
		{
			CalculateAndApply_HDAO(bDoCalculate, bDoApply);
			break;
		}
#endif	//SUPPORT_HDAO
#if SUPPORT_HDAO2
	case ssaotechnique_hdao2:
		{
			CalculateAndApply_HDAO2(bDoCalculate, bDoApply);
			break;
		}
#endif	//SUPPORT_HDAO2
#if SUPPORT_HBAO
	case ssaotechnique_hbao:
		{
			CalculateAndApply_HBAO(bDoCalculate, bDoApply);
			break;
		}
#endif
	default:
		{
			Assertf(false, "SSAO::CalculateAndApply()...Invalid SSAO technique (%d).", m_CurrentSSAOTechnique);
			break;
		}
	}
}


void SSAO::CalculateAndApply_Console(bool bDoCalculate, bool bDoApply)
{
	if(bDoCalculate)
	{
#if __PS3
		if (SpuEnabled())
			ProcessSPU();	// SPU SSAO
		else
#endif //__PS3
			Process();
	}

	if(bDoApply)
	{
		// Apply to ambient gbuffer target
#if __PS3
		grcTextureFactory::GetInstance().LockRenderTarget(0, GBuffer::GetTarget(GBUFFER_RT_3), GBuffer::GetDepthTarget());
#else
		GBuffer::LockTarget_AmbScaleMod();
#endif //__PS3

		Apply();

#if __PS3
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#else
		GBuffer::UnlockTarget_AmbScaleMod(true);
#endif //__XENON
	}
}

// ----------------------------------------------------------------------------------------------- //
void SSAO::ProcessQS(bool isCPQSMix)
{
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)

#if SSAO_USE_LINEAR_DEPTH_TARGETS
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], CRenderTargets::GetDepthBufferQuarterLinear());	
#else // SSAO_USE_LINEAR_DEPTH_TARGETS
	SetMSSAPointTexture1(GBuffer::GetDepthTarget());
	// This is used by the bilateral blur below.
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], CRenderTargets::GetDepthBufferQuarterLinear());
#endif // SSAO_USE_LINEAR_DEPTH_TARGETS

#else // (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], CRenderTargets::GetDepthBufferQuarterLinear());
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)

	grcResolveFlags* resolveFlags = NULL;
#if __XENON
	grcResolveFlags expResolveFlag;
	expResolveFlag.ColorExpBias	= -5;
	resolveFlags = &expResolveFlag;
#endif

#if __XENON
	grcRenderTarget* SSAORT		= m_SSAORT[1];
	grcRenderTarget* BlurXRT	= m_SSAORT[1];
	grcRenderTarget* BlurYRT	= m_SSAORT[0];
#elif __PS3
	grcRenderTarget* SSAORT		= m_SSAORT[1];
	grcRenderTarget* BlurXRT	= m_SSAORT[2];
	grcRenderTarget* BlurYRT	= m_SSAORT[0];
#else
	grcRenderTarget* SSAORT		= m_SSAORT[0];
	grcRenderTarget* BlurXRT	= m_SSAORT[1];
	grcRenderTarget* BlurYRT	= m_SSAORT[0];

	#if RSG_DURANGO && SSAO_USE_ESRAM_TARGETS
		if(isCPQSMix)
		{
			SSAORT	= m_SSAORT_ESRAM[0];
			BlurXRT	= m_SSAORT_ESRAM[1];
			BlurYRT	= m_SSAORT_ESRAM[0];
		}
	#endif // RSG_DURANGO && SSAO_USE_ESRAM_TARGETS

#endif

#if RSG_PC && (!RSG_FINAL)
	if((!SSAORT) || (!BlurXRT) || (!BlurYRT))
		return;
#endif

#if SUPPORT_QSSSAO2
	bool enhanced = false;
	enhanced = m_CurrentSSAOTechnique == ssaotechnique_qsssao2;
#endif // SUPPORT_QSSSAO2

	grcTextureFactory::GetInstance().LockRenderTarget(0, SSAORT, NULL );
	CShaderLib::UpdateGlobalDevScreenSize();

	if(isCPQSMix)
		ShaderUtil::StartShader("QSSSAO Process", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(cpqsmix_qs_ssao_process),true,SSAOShmoo);
	else
		ShaderUtil::StartShader("QSSSAO Process", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_process SUPPORT_QSSSAO2_ONLY(,enhanced)),true,SSAOShmoo);

	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, resolveFlags);

	static dev_bool debugEnableQSBlurX	= true;
	static dev_bool debugEnableQSBlurY	= true;
	
#if SSAO_USE_LINEAR_DEPTH_TARGETS
	if(isCPQSMix)
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], m_FullScreenLinearDepthRT);
	else
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], CRenderTargets::GetDepthBufferQuarterLinear());
#endif

	BANK_ONLY(for (int i = 0; i < sm_QS_CurrentSettings.m_BlurPasses; i++))
	{
		if (debugEnableQSBlurX)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, BlurXRT, NULL );

			m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture0p], SSAORT);

			ShaderUtil::StartShader("SSAO Blur X", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_blur_bilateral_x SUPPORT_QSSSAO2_ONLY(,enhanced)),true,SSAOShmoo);
			grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
			ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);

			grcTextureFactory::GetInstance().UnlockRenderTarget(0, resolveFlags); //unlock and resolve
		}

		if (debugEnableQSBlurY)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, BlurYRT, NULL );

			m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture0p], BlurXRT);

			ShaderUtil::StartShader("SSAO Blur Y", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_blur_bilateral_y SUPPORT_QSSSAO2_ONLY(,enhanced)),true,SSAOShmoo);
			grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
			ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);

			grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve
		}
	}
}

void SSAO::ProcessCP()
{
#if RSG_PC
	if (m_DelayInitializeOffsetTexture)
	{
		InitializeOffsetsTexture();
		m_DelayInitializeOffsetTexture = false;
	}
#endif

	SetMSSAPointTexture1(GBuffer::GetTarget(GBUFFER_RT_1));
#if SSAO_USE_LINEAR_DEPTH_TARGETS
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], CRenderTargets::GetDepthBufferQuarterLinear());
#else // !SSAO_USE_LINEAR_DEPTH_TARGETS
	SetMSSAPointTexture2(GBuffer::GetDepthTarget());
#endif //!SSAO_USE_LINEAR_DEPTH_TARGETS

#if __PS3
	grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetBackBuffer(),	GBuffer::GetDepthTarget());
#else
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_SSAORT[0],						NULL);
#endif //__PS3
	CShaderLib::UpdateGlobalDevScreenSize();

	ShaderUtil::StartShader("CPSSAO Process",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(cpssao_process),true,SSAOShmoo);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve
}

void SSAO::ProcessPM()
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_OffsetRT,		NULL);
	ShaderUtil::StartShader("QSSSAO Downscale",	m_shaderSSAO, m_OffsetTechnique, GetSSAOPass(m_SSAO_downscalePass),true,SSAOShmoo);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve

	m_shaderSSAO->SetVar(m_variables[ssao_offsettexture], m_OffsetRT);

	grcTextureFactory::GetInstance().LockRenderTarget(0, m_PositionRT,		NULL);
	ShaderUtil::StartShader("QSSSAO Downscale",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(pmssao_downscale),true,SSAOShmoo);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve

	//m_shaderSSAO->SetVar(m_variables[ssao_msaa_pointtexture1], GBuffer::GetTarget(GBUFFER_RT_1));
	SetMSSAPointTexture1(GBuffer::GetTarget(GBUFFER_RT_1));
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], m_PositionRT);

#if __PS3
	grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetBackBuffer(),	GBuffer::GetDepthTarget());
#else
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_SSAORT[0],						NULL);
#endif //__PS3
	ShaderUtil::StartShader("CPSSAO Process",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(pmssao_process),true,SSAOShmoo);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve
}

void SSAO::ProcessCPQRMix(ssao_technique technique)
{
	grcAssert((technique >= ssaotechnique_cpqsmix) && (technique <= ssaotechnique_cp8Dirqsmix));

#if RSG_PC
	if (m_DelayInitializeOffsetTexture)
	{
		InitializeOffsetsTexture();
		m_DelayInitializeOffsetTexture = false;
	}
#endif

#if SSAO_USE_LINEAR_DEPTH_TARGETS
	CreateFullScreenLinearDepth();
#if SSAO_MAKE_QUARTER_LINEAR_FROM_FULL_SCREEN_LINEAR
	DownscaleDepthToQuarterTarget();
#endif // SSAO_MAKE_QUARTER_LINEAR_FROM_FULL_SCREEN_LINEAR
#endif // SSAO_USE_LINEAR_DEPTH_TARGETS

	// Set QR strength.
	m_shaderSSAO->SetVar(m_variables[ssao_strength], m_strength*GetCPQSStrength(TCVAR_LIGHT_SSAO_QS_STRENGTH, sm_CPQSMix_CurrentSettings.m_QSRelativeStrength));
	// Set fade in start and end.
	float fadeEnd = sm_CPQSMix_CurrentSettings.m_QSFadeInEnd;

	// Ensure a valid range.
	if(fadeEnd - sm_CPQSMix_CurrentSettings.m_QSFadeInStart < 0.01f)
		fadeEnd = sm_CPQSMix_CurrentSettings.m_QSFadeInStart + 0.01f;

	float denominator = 1.0f/(fadeEnd - sm_CPQSMix_CurrentSettings.m_QSFadeInStart);

	m_shaderSSAO->SetVar(m_variables[ssao_cpqsmix_qs_fadein], Vec4V(sm_CPQSMix_CurrentSettings.m_QSFadeInStart, denominator, sm_CPQSMix_CurrentSettings.m_CPNormalOffset, sm_CPQSMix_CurrentSettings.m_CPRadius));
	// Do QR SSAO processing to SSAO target 0 as normal.
	ProcessQS(true);

	// Set CP strength.
	m_shaderSSAO->SetVar(m_variables[ssao_strength], m_strength*GetCPQSStrength(TCVAR_LIGHT_SSAO_CP_STRENGTH, sm_CPQSMix_CurrentSettings.m_CPRelativeStrength));
	// Set source textures.
	SetMSSAPointTexture1(GBuffer::GetTarget(GBUFFER_RT_1));
#if SSAO_USE_LINEAR_DEPTH_TARGETS
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], m_FullScreenLinearDepthRT);
#else // !SSAO_USE_LINEAR_DEPTH_TARGETS
	SetMSSAPointTexture2(GBuffer::GetDepthTarget());
#endif //!SSAO_USE_LINEAR_DEPTH_TARGETS

#if RSG_DURANGO && SSAO_USE_ESRAM_TARGETS
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture3], m_SSAORT_ESRAM[0]); // Pass in SSAO target 0 as a parameter.
	m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1], m_SSAORT_ESRAM[0]); // Pass in SSAO target 0 as a parameter.
#else // RSG_DURANGO && SSAO_USE_ESRAM_TARGETS
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture3], m_SSAORT[0]); // Pass in SSAO target 0 as a parameter.
	m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1], m_SSAORT[0]); // Pass in SSAO target 0 as a parameter.
#endif // RSG_DURANGO && SSAO_USE_ESRAM_TARGETS

	m_shaderSSAO->SetVar(m_variables[ssao_offsettexture], m_OffsetTexture);

	// Combine QR and CP SSAO to SSAO target 1.
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_CPQSMixFullScreenRT, NULL);
	CShaderLib::UpdateGlobalDevScreenSize();

	if(technique == ssaotechnique_cp4Dirqsmix)
		ShaderUtil::StartShader("CPSSAO Process",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(cpqsmix_cp4Directions_ssao_process_and_combine_with_qs),true,SSAOShmoo);
	else if(technique == ssaotechnique_cp8Dirqsmix)
		ShaderUtil::StartShader("CPSSAO Process (N dir)",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(cpqsmix_cp8Directions_ssao_process_and_combine_with_qs),true,SSAOShmoo);
	else
		ShaderUtil::StartShader("CPSSAO Process (4 dir)",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(cpqsmix_cp_ssao_process_and_combine_with_qs),true,SSAOShmoo);

	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve
}


float SSAO::GetCPQSStrength(TimeCycleVar_e varType, float tc_override)
{
	(void)tc_override;
#if __BANK
	if(m_strengthOverrideEnable)
		return tc_override;
	else
#endif
	{
		const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
		return currKeyframe.GetVar(varType);
	}
}


void SSAO::Process()
{
	switch (m_CurrentSSAOTechnique)
	{
	case ssaotechnique_qsssao:
#if SUPPORT_QSSSAO2
	case ssaotechnique_qsssao2:
#endif
		ProcessQS(false); break;
	case ssaotechnique_cpssao:
		ProcessCP(); break;
	case ssaotechnique_pmssao:
		ProcessPM(); break;
	case  ssaotechnique_cpqsmix:
		ProcessCPQRMix(ssaotechnique_cpqsmix); break;
	case  ssaotechnique_cp4Dirqsmix:
		ProcessCPQRMix(ssaotechnique_cp4Dirqsmix); break;
	case  ssaotechnique_cp8Dirqsmix:
		ProcessCPQRMix(ssaotechnique_cp8Dirqsmix); break;
	default:
		Assertf(false, "SSAO::Process()...Invalid SSAO technique (%d).", m_CurrentSSAOTechnique);
	}
}

// ----------------------------------------------------------------------------------------------- //

void SSAO::Apply()
{
#if __PS3
	const bool bNoDirLight	=	((Lights::GetRenderDirectionalLight().GetIntensity() == 0.0f) || 
								!Lights::GetRenderDirectionalLight().IsCastShadows());
	if (BANK_ONLY(DebugDeferred::m_enableFoliageLightPass &&) !bNoDirLight)
		CRenderTargets::ResetStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0xFB);
	else
		CRenderTargets::ResetStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0xFF);

	grcStateBlock::SetDepthStencilState(m_Foliage_DS,DEFERRED_MATERIAL_ALL);
#else //__PS3
	grcStateBlock::SetDepthStencilState(m_SSAO_DS);
#endif //__PS3

	grcStateBlock::SetBlendState(m_Apply_B);
	grcStateBlock::SetRasterizerState(m_SSAO_R);


	// Only if enabled
	if (!m_enable)
		return;
	
#if SPUPMMGR_PS3
	spuGBufferSSAO_0.WaitForSPU();
#endif

	//Set the AO Gbuffer Target
#if !__PS3 && !__WIN32PC && !RSG_DURANGO && !RSG_ORBIS
			m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexture2], GBuffer::GetTarget(GBUFFER_RT_3));
#endif //!__PS3

	if(m_CurrentSSAOTechnique == ssaotechnique_cpssao || m_CurrentSSAOTechnique == ssaotechnique_pmssao)
	{
#if __PS3
		m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],			CRenderTargets::GetBackBuffer());
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1],			CRenderTargets::GetBackBuffer());
#else
		m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],			m_SSAORT[0]);
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1],			m_SSAORT[0]);
#endif //__PS3
		ShaderUtil::StartShader("CPSSAO Upscale",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(cpssao_upscale),true,SSAOShmoo);
	}
	else
	{
		m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture2],	m_SSAORT[0]);
		ShaderUtil::StartShader("SSAO Upscale",		m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_upscale),true,SSAOShmoo);
	}

	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);


#if __PS3
	m_enableSpuRT = m_enableSpu;
#endif
}


#if __BANK
static const char *g_QS_Resolutions[QS_SSAO_CONFIG_MAX_RESOLUTIONS] = 
{
	"Low",
	"Medium",
	"High",
};
#endif

#if SUPPORT_HDAO
static const char *g_HDAO_QualityLevels[HDAO_CONFIG_MAX_QUALITY_LEVELS] = 
{
	"Low",
	"Medium",
	"High",
};

static const char *g_HDAO_BlurTypes[HDAO_CONFIG_MAX_BLUR_TYPES] = 
{
	"Variable",
	"Single-pass",
	"Depth-aware",
};
#endif	//SUPPORT_HDAO

// ----------------------------------------------------------------------------------------------- //
#if __BANK
// ----------------------------------------------------------------------------------------------- //

#if __PS3
static void ReloadSSAOSpuElfCB()
{
	DebugDeferred::m_spupmReloadElfSlot = RELOAD_SSAOSPU;
	sprintf(DebugDeferred::m_spupmReloadElfName, "x:/gta5/src/dev/game/vs_project2_lib/spu-obj/SSAOSPU.elf");
	DebugDeferred::m_spupmDoReloadElf = true;
}
#endif

void SSAO::AddWidgets(bkBank &bk)
{
	bk.PushGroup("SSAO", false);
		bk.AddToggle(	"Enable",				&m_enable);
		bk.AddToggle(	"Isolate",				&m_isolate);
		bk.AddToggle(	"Override Intensity",	&m_strengthOverrideEnable);
		bk.AddSlider(	"Intensity",			&m_strengthOverride, 0.0f, 20.0f, 0.1f);
		bk.AddToggle(	"Override Technique",	&m_TechniqueOverrideEnable );
		bk.AddCombo(	"Technique",			(int*)&m_ForcedSSAOTechnique, NELEM(m_SSAOTechniqueNames), m_SSAOTechniqueNames, datCallback(CFA(SSAO::OnComboBox_OverrideTechnique)) );
	#if __PS3
		bk.PushGroup("SSAO SPU", false);
			bk.AddToggle("Enable SSAO SPU",				&m_enableSpu);
			bk.AddToggle("Enable hot elf reloading",	&DebugDeferred::m_spupmEnableReloadingElf);
			bk.AddButton("Reload SSAOSPU elf",			ReloadSSAOSpuElfCB);
			bk.PushGroup("SPU Timings:", false);
				bk.AddSlider("Total time (ms)",				&m_SpuTimeTotal,		0.0f, 16.0f, 0.1f);
				bk.AddSlider("Num hw units utilized",		&m_SpuNumUnitsUtilized, 0, 4, 1);
				bk.AddSlider("Time per SPU (ms)",			&m_SpuTimePerSpu,		0.0f, 16.0f, 0.1f);
			bk.PopGroup();
		bk.PopGroup();
	#endif //__PS3
	bkGroup *const g = s_QS_BankGroup = bk.AddGroup("=== QS-SSAO settings ===",false);
	g->AddCombo("QS Preset", &SSAO::sm_QS_ResolutionBeingEdited, QS_SSAO_CONFIG_MAX_RESOLUTIONS, g_QS_Resolutions, 0, datCallback(CFA(SSAO::OnComboBox_QS)));
	g->AddButton("Save", SSAO::SaveParams_QS);
	g->AddButton("Load", SSAO::LoadParams_QS);
	g->AddSlider("Blur Passes",	&sm_QS_CurrentSettings.m_BlurPasses, 0, 5, 1);

	bkGroup *const pCPQSMixGroup = bk.AddGroup("=== CPQS-Mix-SSAO settings ===",false);
	pCPQSMixGroup->AddButton("Save", SSAO::SaveParams_CPQSMix);
	pCPQSMixGroup->AddButton("Load", SSAO::LoadParams_CPQSMix);
	pCPQSMixGroup->AddSlider("CP relative strength",	&sm_CPQSMix_CurrentSettings.m_CPRelativeStrength, 0.0f, 5.0f, 0.01f);
	pCPQSMixGroup->AddSlider("QS relative strength",	&sm_CPQSMix_CurrentSettings.m_QSRelativeStrength, 0.0f, 5.0f, 0.01f);
	pCPQSMixGroup->AddSlider("QS fade in start",		&sm_CPQSMix_CurrentSettings.m_QSFadeInStart, 0.0f, 50.0f, 0.01f);
	pCPQSMixGroup->AddSlider("QS fade in end",			&sm_CPQSMix_CurrentSettings.m_QSFadeInEnd, 0.0f, 50.0f, 0.01f);
	pCPQSMixGroup->AddSlider("QS radius",				&sm_CPQSMix_CurrentSettings.m_QSRadius, 0.0f, 50.0f, 0.01f);
	pCPQSMixGroup->AddSlider("CP radius",				&sm_CPQSMix_CurrentSettings.m_CPRadius, 0.0f, 50.0f, 0.01f);
	pCPQSMixGroup->AddSlider("CP normal offset",		&sm_CPQSMix_CurrentSettings.m_CPNormalOffset, 0.0f, 50.0f, 0.01f);

#if SUPPORT_MRSSAO
		AddWidgets_MRSSAO(bk);
	#endif
	#if SUPPORT_HDAO
		AddWidgets_HDAO(bk);
	#endif
	#if SUPPORT_HDAO2
		AddWidgets_HDAO2(bk);
	#endif
	#if SUPPORT_HBAO
		AddWidgets_HBAO(bk);
	#endif
	bk.PopGroup();
}

// ----------------------------------------------------------------------------------------------- //

void SSAO::Debug()
{
	switch(m_CurrentSSAOTechnique)
	{
	case ssaotechnique_qsssao:
#if SUPPORT_QSSSAO2
	case ssaotechnique_qsssao2:
#endif //SUPPORT_QSSSAO2
	case ssaotechnique_cpssao:
	case ssaotechnique_pmssao:
	case ssaotechnique_cpqsmix:
	case ssaotechnique_cp4Dirqsmix:
	case ssaotechnique_cp8Dirqsmix:
		{
			Debug_Console();
			break;
		}
#if SUPPORT_MRSSAO
	case ssaotechnique_mrssao:
		{
			Debug_MRSSAO();
			break;
		}
#endif	//SUPPORT_MRSSAO
#if SUPPORT_HDAO
	case ssaotechnique_hdao:
		{
			Debug_HDAO();
			break;
		}
#endif	//SUPPORT_HDAO
#if SUPPORT_HDAO2
	case ssaotechnique_hdao2:
		{
			Debug_HDAO2();
			break;
		}
#endif	//SUPPORT_HDAO2
#if SUPPORT_HBAO
	case ssaotechnique_hbao:
		{
			Debug_HBAO();
			break;
		}
#endif
	default:
		{
			Assertf(false, "SSAO::Debug()...Invalid SSAO technique (%d).", m_CurrentSSAOTechnique);
			break;
		}
	}
}


void SSAO::Debug_Console()
{
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

#if SPUPMMGR_PS3
	spuGBufferSSAO_0.WaitForSPU();
#endif

	switch (m_CurrentSSAOTechnique)
	{
	case ssaotechnique_qsssao:
		{
			m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture2],	m_SSAORT[0]);

		#if __PS3
			m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	GBuffer::GetTarget(GBUFFER_RT_3));
		#else // __PS3
			m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	m_SSAORT[0]);
		#endif //__PS3
			// NOTE:- qsssao has power correctly applied during the process stage.
			ShaderUtil::StartShader("CPSSAO Debug",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_upscale_isolate_no_power),true,SSAOShmoo);
			break;
		}
	case ssaotechnique_cpssao:
	case ssaotechnique_pmssao:
		{
			m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture2],	m_SSAORT[0]);

		#if __PS3
			m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	GBuffer::GetTarget(GBUFFER_RT_3));
		#else // __PS3
			m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	m_SSAORT[0]);
		#endif //__PS3
			// NOTE:- cpssao_upscale_isolate applies SSAO strength/power - it should be in the source texture so it gets applied to the scene properly.
			ShaderUtil::StartShader("CPSSAO Debug",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(cpssao_upscale_isolate),true,SSAOShmoo);
			break;
		}
	case ssaotechnique_cpqsmix:
	case ssaotechnique_cp4Dirqsmix:
	case ssaotechnique_cp8Dirqsmix:
		{
			m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture2],	m_CPQSMixFullScreenRT);

		#if __PS3
			m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	GBuffer::GetTarget(GBUFFER_RT_3));
		#else // __PS3
			m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	m_CPQSMixFullScreenRT);
		#endif //__PS3
			// NOTE:- ssaotechnique_cpqrmix has power correctly applied during the process stage.
			ShaderUtil::StartShader("CPSSAO Debug",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(cpssao_upscale_isolate_no_power),true,SSAOShmoo);
			break;
		}
	default:
		break;
	}

	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
}

// ----------------------------------------------------------------------------------------------- //
#endif
// ----------------------------------------------------------------------------------------------- //

#include "SSAO_parser.h"

#if SUPPORT_MRSSAO

#if RSG_ORBIS
template<typename T>
T min(T a, T b)	{ return a>b ? b : a; }
#endif	//RSG_ORBIS

PARAM(ssao_preset,"Which SSAO preset to use(0 - 5).");
PARAM(ssao_res,"Which SSAO res to use(0 = low, 1 = med, 2 = high).");

// Turn this on the view AO targets in PIX.
#define MR_SSAO_USE_FULL_COLOUR_TARGETS 0

#define MR_SSAO_FORCE_MAX_LOD1 0
#define MR_SSAO_BLUR_TO_TARGET 1

// The depths for each LOD (LOD 0 is the depth buffer).
grcRenderTarget *SSAO::m_Depths[MRSSAO_NUM_RESOLUTIONS] = { NULL };
//The normals for each LOD (LOD 0 points to the G-buffer. Subsequent LODs are in view-space).
grcRenderTarget *SSAO::m_Normals[MRSSAO_NUM_RESOLUTIONS] = { NULL };

// The position x & y  each LOD (LOD 0 points to the NULL).
grcRenderTarget *SSAO::m_PositionsX[MRSSAO_NUM_RESOLUTIONS] = { NULL };
grcRenderTarget *SSAO::m_PositionsY[MRSSAO_NUM_RESOLUTIONS] = { NULL };


// AO for each resolution(except the mip-map 0).
grcRenderTarget *SSAO::m_AOs[MRSSAO_NUM_RESOLUTIONS] = { NULL };
// Blurred AO for each resolution.
grcRenderTarget *SSAO::m_BlurredAOs[MRSSAO_NUM_RESOLUTIONS] = { NULL };

int SSAO::sm_MR_CurrentScenePreset = 0;
int SSAO::sm_MR_CurrentResolution = 0;

// The current parameters to use.
MR_SSAO_Parameters SSAO::sm_MR_CurrentSettings;
// Scene "presets".
MR_SSAO_AllScenePresets SSAO::sm_MR_ScenePresets;


void SSAO::Init_MRSSAO()
{
	// Variables
	m_variables[ssao_project_params]			= m_shaderSSAO->LookupVar("projectionParams");
	m_variables[ssao_strength]					= m_shaderSSAO->LookupVar("SSAOStrength");

	m_variables[ssao_deferredlighttexture1]		= m_shaderSSAO->LookupVar("deferredLightTexture1");
	m_variables[ssao_deferredlighttexture0p]	= m_shaderSSAO->LookupVar("deferredLightTexture0P");
	m_variables[ssao_gbuffertexture2]			= m_shaderSSAO->LookupVar("gbufferTexture2");
	m_variables[ssao_gbuffertexturedepth]		= m_shaderSSAO->LookupVar("gbufferTextureDepth");
	m_variables[ssao_deferredlighttexture2]		= m_shaderSSAO->LookupVar("deferredLightTexture2");
	m_variables[ssao_deferredlighttexture]		= m_shaderSSAO->LookupVar("deferredLightTexture");
	m_variables[ssao_deferredlightscreensize]	= m_shaderSSAO->LookupVar("deferredLightScreenSize");
	m_variables[ssao_pointtexture1]				= m_shaderSSAO->LookupVar("PointTexture1");
	m_variables[ssao_pointtexture2]				= m_shaderSSAO->LookupVar("PointTexture2");
	m_variables[ssao_pointtexture3]				= m_shaderSSAO->LookupVar("PointTexture3");
	m_variables[ssao_pointtexture4]				= m_shaderSSAO->LookupVar("PointTexture4");
	m_variables[ssao_pointtexture5]				= m_shaderSSAO->LookupVar("PointTexture5");
	m_variables[ssao_lineartexture1]			= m_shaderSSAO->LookupVar("LinearTexture1");
	m_variables[ssao_targetsize]		 		= m_shaderSSAO->LookupVar("TargetSizeParam");
	m_variables[ssao_falloffandkernel]			= m_shaderSSAO->LookupVar("FallOffAndKernelParam");
#if SSAO_UNIT_QUAD
	m_variables[ssao_quad_position]				= m_shaderSSAO->LookupVar("QuadPosition");
	m_variables[ssao_quad_texcoords]			= m_shaderSSAO->LookupVar("QuadTexCoords");
#endif

	//------------------------------------------------------------------------------------------//

	CreateRendertargets_MRSSAO();

	const unsigned screenWidth	= VideoResManager::GetSceneWidth();
	const unsigned screenHeight	= VideoResManager::GetSceneHeight();

	//------------------------------------------------------------------------------------------//

	// Load in the settings.
	PARSER.LoadObject("platform:/data/MR_SSAOSettings", "xml", SSAO::sm_MR_ScenePresets);

	//------------------------------------------------------------------------------------------//

	int Preset = 0, Res = 0;
	PARAM_ssao_preset.Get(Preset);

	if (!PARAM_ssao_res.Get(Res))
	{
		Res = ChooseResolution( sm_MR_ScenePresets.m_SceneType[Preset].m_Resolutions, screenWidth, screenHeight );
	}

	if((Preset < 0) || (Preset >= MR_SSAO_CONFIG_MAX_SCENE_TYPES))
	{
		AssertMsg(false, "SSAO::Init()...Preset out of range!");
		Preset = 0;
	}

	if((Res < 0) || (Res >= MR_SSAO_CONFIG_MAX_RESOLUTIONS))
	{
		AssertMsg(false, "SSAO::Init()...Res out of range!");
		Res = 0;
	}

#if __BANK
	sm_MR_ScenePresetBeingEdited = Preset;
	sm_MR_ResolutionBeingEdited = Res;
#endif	//__BANK
	UpdateSettingsToUse_MRSSAO(Preset, Res);
}


//------------------------------------------------------------------------------
void SSAO::Shutdown_MRSSAO()
{
	DestroyRendertargets_MRSSAO();
}


//------------------------------------------------------------------------------
void SSAO::CreateRendertargets_MRSSAO()
{
	if (!m_enable) return;


	grcTextureFactory::CreateParams params;
	params.HasParent		= true;
	params.Parent			= NULL;
	params.Format			= grctfR32F;

	grcTextureFactory::CreateParams paramsNormalTexture;
	paramsNormalTexture.HasParent	= true;
	paramsNormalTexture.Parent		= NULL;
	paramsNormalTexture.Format		= MR_SSAO_USE_PACKED_NORMALS  ? grctfR32F : grctfA8R8G8B8;

	const unsigned screenWidth	= VideoResManager::GetSceneWidth();
	const unsigned screenHeight	= VideoResManager::GetSceneHeight();

	for(int i=0; i<MRSSAO_NUM_RESOLUTIONS; i++)
	{
		char DepthName[256];
		char NormalName[256];
		char AOName[256];
		char BlurredAOName[256];
		char PosXName[256];
		char PosYName[256];

		sprintf(DepthName, "SSAO Viewspace positions LOD %d", i);
		sprintf(NormalName, "SSAO Normal LOD %d", i);
		sprintf(AOName, "SSAO AO LOD %d", i);
		sprintf(BlurredAOName, "Blurred SSAO AO LOD %d", i);
		sprintf(PosXName, "SSAO Position X LOD %d", i);
		sprintf(PosYName, "SSAO Position Y LOD %d", i);

		if(i==0)
		{
			m_Depths[0] = GBuffer::GetDepthTarget();
			m_Normals[0] = GBuffer::GetTarget(GBUFFER_RT_1);
			m_PositionsX[0] = NULL;
			m_PositionsY[0] = NULL;
		}
		else
		{
			params.Format = grctfR32F;

			if(i!=1)
			{
				m_Depths[i] = !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(DepthName, grcrtPermanent, 
					screenWidth >> i, screenHeight >> i, 32, &params);
			}
			else
			{
				// Use the console half-size depth buffer.
				m_Depths[i] = CRenderTargets::GetDepthBufferQuarterLinear();
			}

			m_Normals[i] = !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(NormalName, grcrtPermanent, 
				screenWidth >> i, screenHeight >> i, 32, &paramsNormalTexture);

			m_PositionsX[i] = !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(PosXName, grcrtPermanent, 
				screenWidth >> i, screenHeight >> i, 32, &params);

			m_PositionsY[i] = !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(PosYName, grcrtPermanent, 
				screenWidth >> i, screenHeight >> i, 32, &params);
		}

	#ifdef MR_SSAO_USE_FULL_COLOUR_TARGETS
		params.Format = grctfA8R8G8B8;
	#else
		params.Format = grctfL8;
	#endif

		if(i==0)
		{
		#if MR_SSAO_FORCE_MAX_LOD1
			m_AOs[i] = NULL;
			m_BlurredAOs[i] = NULL;
		#else //MR_SSAO_FORCE_MAX_LOD1

			m_AOs[i] = !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(AOName, grcrtPermanent, 
				screenWidth >> i, screenHeight >> i, 32, &params);

		#if !MR_SSAO_BLUR_TO_TARGET || MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE
			m_BlurredAOs[i] = !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(BlurredAOName, grcrtPermanent, 
				screenWidth >> i, screenHeight >> i, 32, &params);
		#endif // !MR_SSAO_BLUR_TO_TARGET || MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE

		#endif //MR_SSAO_FORCE_MAX_LOD1
		}
		else
		{
			m_AOs[i] = !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(AOName, grcrtPermanent, 
				screenWidth >> i, screenHeight >> i, 32, &params);

			m_BlurredAOs[i] = !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget(BlurredAOName, grcrtPermanent, 
				screenWidth >> i, screenHeight >> i, 32, &params);
		}
	}
}


//------------------------------------------------------------------------------
void SSAO::DestroyRendertargets_MRSSAO()
{
	// These are the G-buffer targets, we don`t own them.
	m_Depths[0] = NULL;
	m_Normals[0] = NULL;

	// This is shared between console and PC MR SSAO.
	m_Depths[1] = NULL;

	for(int i=MRSSAO_NUM_RESOLUTIONS-1; i>=0; i--)
	{
		if(m_BlurredAOs[i])
		{
			delete m_BlurredAOs[i];
			m_BlurredAOs[i] = NULL;
		}

		if(m_AOs[i])
		{
			delete m_AOs[i];
			m_AOs[i] = NULL;
		}

		if(m_Normals[i])
		{
			delete m_Normals[i];
			m_Normals[i] = NULL;
		}

		if(m_Depths[i])
		{
			delete m_Depths[i];
			m_Depths[i] = NULL;
		}

		if(m_PositionsX[i])
		{
			delete m_PositionsX[i];
			m_PositionsX[i] = NULL;
		}

		if(m_PositionsY[i])
		{
			delete m_PositionsY[i];
			m_PositionsY[i] = NULL;
		}
	}
}


//------------------------------------------------------------------------------
void SSAO::CalculateAndApply_MRSSAO(bool bDoCalculate, bool bDoApply)
{
	(void)bDoApply;
	(void)bDoCalculate;

	int i;
	// Set projection parameters.
	const Vector4 projParams = CalculateProjectionParams_PreserveTanFOV();
	m_shaderSSAO->SetVar(m_variables[ssao_project_params], projParams);

	// Set the SSAO fall off radius and max kernel.
	float MaxKernelSize = ((VideoResManager::GetSceneWidth()*sm_MR_CurrentSettings.m_KernelSizePercentage)/100.0f) - 1.0f;
	Vector4 FallOfAndKernel = Vector4(sm_MR_CurrentSettings.m_FallOffRadius, MaxKernelSize, (float)(2*sm_MR_CurrentSettings.m_NormalWeightPower), sm_MR_CurrentSettings.m_DepthWeightPower);
	m_shaderSSAO->SetVar(m_variables[ssao_falloffandkernel], FallOfAndKernel);

	// Set the "strength".
	m_shaderSSAO->SetVar(m_variables[ssao_strength], sm_MR_CurrentSettings.m_Strength);

	//------------------------------------------------------------------------------------------//

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_SSAO_DS);
	grcStateBlock::SetRasterizerState(m_SSAO_R);

	if(sm_MR_CurrentSettings.m_BottomLod > 0)
	{
		// Down scale from the G-buffer into LOD1.
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture4], GBuffer::GetTarget(GBUFFER_RT_1));
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture5], GBuffer::GetDepthTarget());
		RenderToMultiTarget4_MRSSAO(m_Depths[1], m_Normals[1], m_PositionsX[1], m_PositionsY[1], "SSAO Down scale from G-buffer", mrssao_downscale_from_gbuffer);
	}

	// Down-scale into smaller mips.
	for(i=2; i<=sm_MR_CurrentSettings.m_BottomLod; i++)
	{
		// Use the "previous" depths and normals.
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1], m_Normals[i-1]);
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], m_Depths[i-1]);
		m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexture2], m_PositionsX[i-1]);
		m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexturedepth], m_PositionsY[i-1]);

		RenderToMultiTarget4_MRSSAO(m_Depths[i], m_Normals[i], m_PositionsX[i], m_PositionsY[i], "SSAO down scale.", mrssao_downscale);
	}

	//------------------------------------------------------------------------------------------//

	//grcTextureFactory::GetInstance().LockRenderTarget(0, m_SSAORT[0], NULL );

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_SSAO_DS);
	grcStateBlock::SetRasterizerState(m_SSAO_R);

	int LodToApply = Min(sm_MR_CurrentSettings.m_BottomLod, sm_MR_CurrentSettings.m_LodToApply);
#if MR_SSAO_FORCE_MAX_LOD1
	// Cap to 1/4 max on 360.
	LodToApply = max(1, LodToApply);
#endif //MR_SSAO_FORCE_MAX_LOD1

	for(i=sm_MR_CurrentSettings.m_BottomLod; i>=LodToApply; i--)
	{
		int iMaxKernel = (int)MaxKernelSize;

		if(i == 0)
		{
			// Send up the max kernel for LOD 0.
			iMaxKernel = (int)sm_MR_CurrentSettings.m_MaxKernel_Lod0;
			FallOfAndKernel = Vector4(sm_MR_CurrentSettings.m_FallOffRadius, (float)sm_MR_CurrentSettings.m_MaxKernel_Lod0, (float)(2*sm_MR_CurrentSettings.m_NormalWeightPower), sm_MR_CurrentSettings.m_DepthWeightPower);
		}
		else if(i == 1)
		{
			// Send up the max kernel for LOD 1.
			iMaxKernel = (int)sm_MR_CurrentSettings.m_MaxKernel_Lod1;
			FallOfAndKernel = Vector4(sm_MR_CurrentSettings.m_FallOffRadius, (float)sm_MR_CurrentSettings.m_MaxKernel_Lod1, (float)(2*sm_MR_CurrentSettings.m_NormalWeightPower), sm_MR_CurrentSettings.m_DepthWeightPower);
		}
		else
		{
			FallOfAndKernel = Vector4(sm_MR_CurrentSettings.m_FallOffRadius, MaxKernelSize, (float)(2*sm_MR_CurrentSettings.m_NormalWeightPower), sm_MR_CurrentSettings.m_DepthWeightPower);
		}
		m_shaderSSAO->SetVar(m_variables[ssao_falloffandkernel], FallOfAndKernel);

		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], m_Depths[i]);
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1], m_Normals[i]);
		m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexture2], m_PositionsX[i]);
		m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexturedepth], m_PositionsY[i]);
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture4], GBuffer::GetTarget(GBUFFER_RT_1));
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture5], GBuffer::GetDepthTarget());

		if(i == (sm_MR_CurrentSettings.m_BottomLod))
		{
			// Compute the lowest mip-level.
			RenderToTarget_MRSSAO(m_AOs[i], "SSAO compute", GetComputeAOPass_MRSSAO(i, iMaxKernel));
		}
		else
		{
			// Combine with lower mip-levels.
			m_shaderSSAO->SetVar(m_variables[ssao_pointtexture3], m_BlurredAOs[i+1]);
			RenderToTarget_MRSSAO(m_AOs[i], "SSAO compute and combine", GetComputeAOAndCombinePass_MRSSAO(i, iMaxKernel));
		}

#if MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE
		// Apply a 3x3 kernel blur to the AO.
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1],	m_AOs[i]);
		m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	m_AOs[i]);

		// Invert the AO in the final LOD ready for reading by the lighting shader.
		if(i == LodToApply)
			RenderToTarget_MRSSAO(m_BlurredAOs[i], "SSAO blur and apply", mrssao_blur_ao_and_apply);
		else
			RenderToTarget_MRSSAO(m_BlurredAOs[i], "SSAO blur", mrssao_blur_ao);

#else // MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE

	#if MR_SSAO_BLUR_TO_TARGET
		// Not blurring only if the final ApplyAndBlur can make it
		if(i != LodToApply || i > 1)
	#endif //MR_SSAO_BLUR_TO_TARGET
		{
			// Apply a 3x3 kernel blur to the AO.
			m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1],	m_AOs[i]);
			m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	m_AOs[i]);
			RenderToTarget_MRSSAO(m_BlurredAOs[i], "SSAO blur", mrssao_blur_ao);
		}
#endif // MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE

	}

	//------------------------------------------------------------------------------------------//

#if !MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE
	// Apply to ambient g-buffer target
	GBuffer::LockTarget_AmbScaleMod();

	grcStateBlock::SetBlendState(m_Apply_B);
	grcStateBlock::SetDepthStencilState(m_SSAO_DS);
	grcStateBlock::SetRasterizerState(m_SSAO_R);

	float W = (float)VideoResManager::GetSceneWidth();
	float H = (float)VideoResManager::GetSceneHeight();
	Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
	m_shaderSSAO->SetVar(m_variables[ssao_targetsize], TargetSize);

#if !MR_SSAO_BLUR_TO_TARGET
	m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture1], m_BlurredAOs[LodToApply]);
	m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture2], m_BlurredAOs[LodToApply]);
	SSAO::DrawFullScreenQuad_MRSSAO("SSAO Apply", mrssao_apply);
#else //!MR_SSAO_BLUR_TO_TARGET
	if (LodToApply <= 1)
	{
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1],	m_AOs[LodToApply]);
		m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	m_AOs[LodToApply]);
		SSAO::DrawFullScreenQuad_MRSSAO("SSAO Blur and apply", mrssao_blur_ao_and_apply);
	}else
	{
		// Has to be blurred already by this point
		m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture2], m_BlurredAOs[LodToApply]);
		SSAO::DrawFullScreenQuad_MRSSAO("SSAO Apply", mrssao_apply);
	}
#endif // !MR_SSAO_BLUR_TO_TARGET

	GBuffer::UnlockTarget_AmbScaleMod(true);

#endif // !MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE
}


//------------------------------------------------------------------------------
ssao_pass SSAO::GetComputeAOPass_MRSSAO(int LOD, int KernelSize)
{
	(void)LOD;
	(void)KernelSize;

	if(LOD != 0)
	{
		return (ssao_pass)((int)mrssao_compute_ao + sm_MR_CurrentSettings.m_KernelType);
	}
	else
	{
		return (ssao_pass)((int)mrssao_compute_ao_from_gbuffer + sm_MR_CurrentSettings.m_KernelType);
	}
}

ssao_pass SSAO::GetComputeAOAndCombinePass_MRSSAO(int LOD, int KernelSize)
{
	(void)LOD;
	(void)KernelSize;

	if(LOD != 0)
	{
		return (ssao_pass)((int)mrssao_compute_ao_and_combine + sm_MR_CurrentSettings.m_KernelType);
	}
	else
	{
		return (ssao_pass)((int)mrssao_compute_ao_from_gbuffer_and_combine + sm_MR_CurrentSettings.m_KernelType);
	}
}

/*--------------------------------------------------------------------------------------------------*/
// Rendering helpers.
/*--------------------------------------------------------------------------------------------------*/

void SSAO::RenderToTarget_MRSSAO(grcRenderTarget *pTarget, const char pDebugStr[], ssao_pass Pass)
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, pTarget, NULL );

	float W = (float)pTarget->GetWidth();
	float H = (float)pTarget->GetHeight();
	Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
	m_shaderSSAO->SetVar(m_variables[ssao_targetsize], TargetSize);

	DrawFullScreenQuad_MRSSAO(pDebugStr, Pass);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
}


//------------------------------------------------------------------------------
void SSAO::RenderToMultiTarget_MRSSAO(grcRenderTarget *pColour0, grcRenderTarget *pColour1, const char pDebugStr[], ssao_pass Pass)
{
	const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
		pColour0,
		pColour1,
		NULL,
		NULL
	};

	grcTextureFactory::GetInstance().LockMRT(RenderTargets, NULL);

	float W = (float)pColour0->GetWidth();
	float H = (float)pColour0->GetHeight();
	Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
	m_shaderSSAO->SetVar(m_variables[ssao_targetsize], TargetSize);

	DrawFullScreenQuad_MRSSAO(pDebugStr, Pass);

	grcTextureFactory::GetInstance().UnlockMRT(NULL);
}


void SSAO::RenderToMultiTarget4_MRSSAO(grcRenderTarget *pColour0, grcRenderTarget *pColour1, grcRenderTarget *pColour2, grcRenderTarget *pColour3, const char pDebugStr[], ssao_pass Pass)
{
	const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
		pColour0,
		pColour1,
		pColour2,
		pColour3
	};

	grcTextureFactory::GetInstance().LockMRT(RenderTargets, NULL);

	float W = (float)pColour0->GetWidth();
	float H = (float)pColour0->GetHeight();
	Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
	m_shaderSSAO->SetVar(m_variables[ssao_targetsize], TargetSize);

	DrawFullScreenQuad_MRSSAO(pDebugStr, Pass);

	grcTextureFactory::GetInstance().UnlockMRT(NULL);
}


//------------------------------------------------------------------------------
void SSAO::DrawFullScreenQuad_MRSSAO(const char pDebugStr[], ssao_pass Pass)
{
#if SSAO_UNIT_QUAD
	m_shaderSSAO->SetVar(m_variables[ssao_quad_position],	FastQuad::Pack(-1.f,1.f,1.f,-1.f));
	m_shaderSSAO->SetVar(m_variables[ssao_quad_texcoords],	FastQuad::Pack(0.f,0.f,1.f,1.f));
#endif
	ShaderUtil::StartShader(pDebugStr, m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(Pass));
#if SSAO_UNIT_QUAD
	FastQuad::Draw(true);
#else
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
#endif	//SSAO_UNIT_QUAD
	ShaderUtil::EndShader(m_shaderSSAO);
}


/*--------------------------------------------------------------------------------------------------*/
// Misc functions.																						
/*--------------------------------------------------------------------------------------------------*/

void SSAO::UpdateSettingsToUse_MRSSAO(int Preset, int Resolution)
{
	// 	Record the "index".
	sm_MR_CurrentResolution = Resolution;
	sm_MR_CurrentScenePreset = Preset;

	// Copy over the current settings to use.
	sm_MR_CurrentSettings = sm_MR_ScenePresets.m_SceneType[sm_MR_CurrentScenePreset].m_Resolutions[sm_MR_CurrentResolution];
}

// ----------------------------------------------------------------------------------------------- //
#if __BANK
// ----------------------------------------------------------------------------------------------- //

int SSAO::sm_MR_ScenePresetBeingEdited = 0;
int SSAO::sm_MR_ResolutionBeingEdited = 0;
MR_SSAO_Parameters SSAO::sm_Clipboard_Params;
MR_SSAO_ScenePreset SSAO::sm_Clipboard_ScenePreset;

static const char *g_SceneTypes[MR_SSAO_CONFIG_MAX_SCENE_TYPES] = 
{
	"Default - outdoor",
	"Default - indoor",
};

static const char *g_Resolutions[MR_SSAO_CONFIG_MAX_RESOLUTIONS] = 
{
	"Low",
	"Medium",
	"High",
};

static const char *g_KernelTypes[MR_SSAO_KERNEL_MAX] = 
{
	"Full",
	"Stippled",
	"Star",
};

void SSAO::AddWidgets_MRSSAO(bkBank &bk)
{
	bkGroup *const g = s_MRSSAO_BankGroup = bk.AddGroup("=== MRSSAO settings ===",false);
	g->AddCombo("Scene type", &SSAO::sm_MR_ScenePresetBeingEdited, MR_SSAO_CONFIG_MAX_SCENE_TYPES, g_SceneTypes, 0, datCallback(CFA(SSAO::OnComboBox_MRSSAO)));
	g->AddCombo("Screen Res.", &SSAO::sm_MR_ResolutionBeingEdited, 3, g_Resolutions, 0, datCallback(CFA(SSAO::OnComboBox_MRSSAO)));
	g->AddCombo("Kernel type.", &SSAO::sm_MR_CurrentSettings.m_KernelType, MR_SSAO_KERNEL_MAX, g_KernelTypes, 0, datCallback(CFA(SSAO::OnComboBox_MRSSAO_KernelType)));

	//g->AddSlider("Target width",	&SSAO::sm_MR_CurrentSettings.m_Dim.x, 640.f, 4000.f, 16.f);
	//g->AddSlider("Target height",	&SSAO::sm_MR_CurrentSettings.m_Dim.y, 480.f, 2000.f, 16.f);
	g->AddSlider("Radius",			&SSAO::sm_MR_CurrentSettings.m_FallOffRadius, 0.0f, 500.0f, 0.01f);
	// Specify granularity of about 1 pixel of a 1280x720 screen.
	g->AddSlider("MaxKernel % of Screen", &SSAO::sm_MR_CurrentSettings.m_KernelSizePercentage, 0.0f, 3.0f, 100.0f/1280.0f);
	g->AddSlider("MaxKernel - LOD 0", &SSAO::sm_MR_CurrentSettings.m_MaxKernel_Lod0, 1, 5, 1);
	g->AddSlider("MaxKernel - LOD 1", &SSAO::sm_MR_CurrentSettings.m_MaxKernel_Lod1, 1, 10, 1);

#if MR_SSAO_FORCE_MAX_LOD1
	g->AddSlider("LodToApply", &SSAO::sm_MR_CurrentSettings.m_LodToApply, 1, MRSSAO_NUM_RESOLUTIONS-1, 1);
	g->AddSlider("Lowest LOD", &SSAO::sm_MR_CurrentSettings.m_BottomLod, 1, MRSSAO_NUM_RESOLUTIONS-1, 1);
#else
	g->AddSlider("LodToApply", &SSAO::sm_MR_CurrentSettings.m_LodToApply, 0, MRSSAO_NUM_RESOLUTIONS-1, 1);
	g->AddSlider("Lowest LOD", &SSAO::sm_MR_CurrentSettings.m_BottomLod, 0, MRSSAO_NUM_RESOLUTIONS-1, 1);
#endif

	//g->AddSlider("Normal Weight Power", &SSAO::sm_MR_CurrentSettings.m_NormalWeightPower, 1, 8, 1);
	g->AddSlider("Depth Weight Power", &SSAO::sm_MR_CurrentSettings.m_DepthWeightPower, 0.0f, 16.0f, 0.25f);
	g->AddSlider("Strength", &SSAO::sm_MR_CurrentSettings.m_Strength, 0.0f, 20.0f, 0.05f);

	g->AddButton("Save", SSAO::SaveParams_MRSSAO);
	g->AddButton("Load", SSAO::LoadParams_MRSSAO);

	g->AddButton("To Clipboard", SSAO::OnParamsToClipboard_MRSSAO);
	g->AddButton("From Clipboard", SSAO::OnClipboardToParams_MRSSAO);

	g->AddButton("Scene -> Clipboard", SSAO::OnScenePresetToClipboard_MRSSAO);
	g->AddButton("Clipboard -> Scene", SSAO::OnClipboardToScenePreset_MRSSAO);

	g->AddToggle("Isolate", &SSAO::m_isolate);
}


// ----------------------------------------------------------------------------------------------- //
void SSAO::SaveParams_MRSSAO(void)
{
	// "Grab" the current settings.
	sm_MR_ScenePresets.m_SceneType[sm_MR_CurrentScenePreset].m_Resolutions[sm_MR_CurrentResolution] = sm_MR_CurrentSettings;
	PARSER.SaveObject("platform:/data/MR_SSAOSettings", "xml", &SSAO::sm_MR_ScenePresets, parManager::XML);
}


// ----------------------------------------------------------------------------------------------- //
void SSAO::LoadParams_MRSSAO(void)
{
	PARSER.LoadObject("platform:/data/MR_SSAOSettings", "xml", SSAO::sm_MR_ScenePresets);
	SSAO::UpdateSettingsToUse_MRSSAO(SSAO::sm_MR_ScenePresetBeingEdited, SSAO::sm_MR_ResolutionBeingEdited);
}


// ----------------------------------------------------------------------------------------------- //
void SSAO::OnComboBox_MRSSAO(void)
{	
	// "Grab" the current settings.
	sm_MR_ScenePresets.m_SceneType[sm_MR_CurrentScenePreset].m_Resolutions[sm_MR_CurrentResolution] = sm_MR_CurrentSettings;
	// Specify the new set to use.
	SSAO::UpdateSettingsToUse_MRSSAO(sm_MR_ScenePresetBeingEdited, sm_MR_ResolutionBeingEdited);
}


void SSAO::OnComboBox_MRSSAO_KernelType(void)
{
}


// ----------------------------------------------------------------------------------------------- //
void SSAO::OnParamsToClipboard_MRSSAO(void)
{
	sm_Clipboard_Params = sm_MR_CurrentSettings;
}


void SSAO::OnClipboardToParams_MRSSAO(void)
{
	sm_MR_CurrentSettings = sm_Clipboard_Params;
}


// ----------------------------------------------------------------------------------------------- //
void SSAO::OnScenePresetToClipboard_MRSSAO(void)
{
	// "Grab" the current settings.
	sm_MR_ScenePresets.m_SceneType[sm_MR_CurrentScenePreset].m_Resolutions[sm_MR_CurrentResolution] = sm_MR_CurrentSettings;
	sm_Clipboard_ScenePreset = sm_MR_ScenePresets.m_SceneType[sm_MR_CurrentScenePreset];
}

void SSAO::OnClipboardToScenePreset_MRSSAO(void)
{
	sm_MR_ScenePresets.m_SceneType[sm_MR_CurrentScenePreset] = sm_Clipboard_ScenePreset;
	SSAO::UpdateSettingsToUse_MRSSAO(sm_MR_ScenePresetBeingEdited, sm_MR_ResolutionBeingEdited);
}


// ----------------------------------------------------------------------------------------------- //
void SSAO::Debug_MRSSAO()
{
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	int LodToApply = Min(sm_MR_CurrentSettings.m_BottomLod, sm_MR_CurrentSettings.m_LodToApply);
#if MR_SSAO_FORCE_MAX_LOD1
	// Cap to 1/4 max on 360.
	LodToApply = max(1, LodToApply);
#endif //MR_SSAO_FORCE_MAX_LOD1

	float W = (float)VideoResManager::GetSceneWidth();
	float H = (float)VideoResManager::GetSceneHeight();
	Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
	m_shaderSSAO->SetVar(m_variables[ssao_targetsize], TargetSize);

#if MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1], m_BlurredAOs[LodToApply]);
	m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1], m_BlurredAOs[LodToApply]);
	SSAO::DrawFullScreenQuad_MRSSAO("SSAO isolate no power", mrssao_apply_isolate_no_power);
#else // MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE
#if !MR_SSAO_BLUR_TO_TARGET
	m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture1], m_BlurredAOs[LodToApply]);
	SSAO::DrawFullScreenQuad_MRSSAO("SSAO isolate", mrssao_apply_isolate);
#else // !MR_SSAO_BLUR_TO_TARGET
	m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1], m_AOs[LodToApply]);
	m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1], m_AOs[LodToApply]);
	SSAO::DrawFullScreenQuad_MRSSAO("SSAO isolate blur", mrssao_apply_isolate_blur);
#endif // !MR_SSAO_BLUR_TO_TARGET
#endif // MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE

}

#endif //__BANK
#endif	//SUPPORT_MRSSAO


/*--------------------------------------------------------------------------------------------------*/
/*	AMD HDAO																						*/
/*--------------------------------------------------------------------------------------------------*/

#if SUPPORT_HDAO
#if __BANK
int SSAO::GetQualityLevel_HDAO(const atString &QualityString)	{
	int iQuality = 0;
	while (strcmp( g_HDAO_QualityLevels[iQuality], QualityString ))
	{
		if (++iQuality >= HDAO_CONFIG_MAX_QUALITY_LEVELS)
		{
			Assertf( false, "HDAO Quality level not found: %s", QualityString.c_str() );
			return 0;
		}
	}
	return iQuality;
}

void SSAO::SaveParams_HDAO(void)
{
	sm_HDAO_CurrentSettings.m_BlurType = g_HDAO_BlurTypes[hdao::iBlur];
	sm_HDAO_Settings.m_QualityLevels[sm_HDAO_CurrentQualityLevel] = sm_HDAO_CurrentSettings;
	PARSER.SaveObject("platform:/data/HDAOSettings", "xml", &sm_HDAO_Settings, parManager::XML);
}
void SSAO::LoadParams_HDAO(void)
{
	PARSER.LoadObject("platform:/data/HDAOSettings", "xml", sm_HDAO_Settings);
	SSAO::UpdateSettingsToUse_HDAO(sm_HDAO_QualityLevelBeingEdited);

}
void SSAO::OnComboBox_HDAO(void)
{
	sm_HDAO_Settings.m_QualityLevels[sm_HDAO_CurrentQualityLevel] = sm_HDAO_CurrentSettings;
	SSAO::UpdateSettingsToUse_HDAO(sm_HDAO_QualityLevelBeingEdited);
}
#endif	//__BANK

void SSAO::UpdateSettingsToUse_HDAO(int QualityLevel)
{
	sm_HDAO_CurrentQualityLevel = QualityLevel;
	sm_HDAO_CurrentSettings = sm_HDAO_Settings.m_QualityLevels[QualityLevel];
	hdao::iBlur = findString( sm_HDAO_CurrentSettings.m_BlurType, g_HDAO_BlurTypes );
}

void SSAO::UpdateSettingsToUse_HDAO2(int QualityLevel)
{
	sm_HDAO2_CurrentQualityLevel = QualityLevel;
	sm_HDAO2_CurrentSettings = sm_HDAO2_Settings.m_QualityLevels[QualityLevel];
}

void SSAO::Reset_HDAOShaders()
{
	delete hdao::shader;
	Init_HDAOShaders();
}

void SSAO::Init_HDAOShaders()
{
	ASSET.PushFolder("common:/shaders");
	hdao::shader = grmShaderFactory::GetInstance().Create();
	hdao::shader->Load( GRCDEVICE.GetMSAA()>1 ? "HDAO_MS" : "HDAO" ); // Use the non-msaa version of the shader for 4s1f EQAA
	ASSET.PopFolder();

	hdao::technique			= hdao::shader->LookupTechnique( "HDAO" );
	hdao::techniqueApply	= hdao::shader->LookupTechnique( "HDAO_Apply" );

	// Variables
	hdao::varDepthTexture			= hdao::shader->LookupVar("depthTexture");
	hdao::varNormalTexture			= hdao::shader->LookupVar("normalTexture");
	hdao::varOcclusionTexture		= hdao::shader->LookupVar("occlusionTexture");
	hdao::varProjectionParams		= hdao::shader->LookupVar("projectionParams");
	hdao::varProjectionShear		= hdao::shader->LookupVar("projectionShear");
	hdao::varTargetSize				= hdao::shader->LookupVar("targetSize");
	hdao::varBlurDirection			= hdao::shader->LookupVar("blurDirection");
	hdao::varComputeParams			= hdao::shader->LookupVar("HDAOComputeParams");
	hdao::varApplyParams			= hdao::shader->LookupVar("HDAOApplyParams");
	hdao::varValleyParams			= hdao::shader->LookupVar("HDAOValleyParams");
	hdao::varExtraParams			= hdao::shader->LookupVar("HDAOExtraParams");
	hdao::varOcclusionTextureParams	= hdao::shader->LookupVar("occlusionTexParams");
}

void SSAO::Init_HDAO(bool bCore)
{
	if (!bCore)
	{
		return;
	}

	Init_HDAOShaders();

	Vector4 tmp;
	hdao::shader->GetVar( hdao::varApplyParams,	tmp );
	hdao::fBlurThreshold	= tmp.z;
	sm_HDAO_CurrentSettings.m_Strength	= tmp.w;
#if __BANK
	hdao::shader->GetVar( hdao::varComputeParams, tmp );
	sm_HDAO_CurrentSettings.m_RejectRadius	= tmp.x;
	sm_HDAO_CurrentSettings.m_AcceptRadius	= tmp.y;
	sm_HDAO_CurrentSettings.m_FadeoutDist	= tmp.z;
	sm_HDAO_CurrentSettings.m_NormalScale	= tmp.w;
	hdao::shader->GetVar( hdao::varValleyParams, tmp );
	hdao::fDotOffset	= tmp.x;
	hdao::fDotScale		= tmp.y;
	hdao::fDotPower		= tmp.z;
	hdao::shader->GetVar( hdao::varExtraParams, tmp );
	sm_HDAO_CurrentSettings.m_WorldSpace	= tmp.x;
	sm_HDAO_CurrentSettings.m_TargetScale = tmp.y;
	sm_HDAO_CurrentSettings.m_RadiusScale = tmp.z;
	sm_HDAO_CurrentSettings.m_BaseWeight	= tmp.w;
#endif	//__BANK

	int Quality = 1;
	PARAM_hdao_quality.Get(Quality);

	if((Quality < 0) || (Quality >= HDAO_CONFIG_MAX_QUALITY_LEVELS))
	{
		AssertMsg(false, "SSAO::Init_HDAO()...Resolution out of range!");
		Quality = 0;
	}

	// Load in the settings.
	PARSER.LoadObject("platform:/data/HDAOSettings", "xml", sm_HDAO_Settings);
	BANK_ONLY( sm_HDAO_QualityLevelBeingEdited = Quality );
	UpdateSettingsToUse_HDAO(Quality);
}

void SSAO::Shutdown_HDAO(bool bCore)
{
	if (bCore)
	{
		delete hdao::shader;
		hdao::shader = NULL;
	}
}

void SSAO::CalculateAndApply_HDAO(bool bCalculate, bool bApply)
{
#if __BANK
	hdao::shader->SetVar( hdao::varComputeParams,	Vector4(
		sm_HDAO_CurrentSettings.m_RejectRadius,
		sm_HDAO_CurrentSettings.m_AcceptRadius,
		sm_HDAO_CurrentSettings.m_FadeoutDist,
		sm_HDAO_CurrentSettings.m_NormalScale ));
	hdao::shader->SetVar( hdao::varValleyParams,	Vector4( hdao::fDotOffset, hdao::fDotScale, hdao::fDotPower, 0.f ));
	hdao::shader->SetVar( hdao::varExtraParams,		Vector4(
		sm_HDAO_CurrentSettings.m_WorldSpace,
		sm_HDAO_CurrentSettings.m_TargetScale,
		sm_HDAO_CurrentSettings.m_RadiusScale,
		sm_HDAO_CurrentSettings.m_BaseWeight ));
#endif	//__BANK
	
	// Set projection parameters
	const Vector4 projParams = CalculateProjectionParams_PreserveTanFOV();
	hdao::shader->SetVar( hdao::varProjectionParams, projParams );

	// Set perspective shear
	const grcViewport *const vp = grcViewport::GetCurrent();
	const Vector2 shear = vp->GetPerspectiveShear();
	const Vector4 shearParams( shear.x, shear.y, 0.f, 0.f );
	hdao::shader->SetVar( hdao::varProjectionShear, shearParams );

	// 1: compute for half res
#if HDAO_PROFILE_SIZE
	if (++hdao::nCurrentFrame >= hdao::nFramesAveraging)
		hdao::nCurrentFrame = 0;
	hdao::gpuTimer.Start();
#endif
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_SSAO_DS);
	grcStateBlock::SetRasterizerState(m_SSAO_R);

	hdao::shader->SetVar( hdao::varDepthTexture,	GBuffer::GetDepthTarget() );
	hdao::shader->SetVar( hdao::varNormalTexture,	GBuffer::GetTarget(GBUFFER_RT_1) );

	if (bCalculate)
	{
		const hdao_pass mainPasses[] = { hdao_compute_high, hdao_compute_med, hdao_compute_low };
		const hdao_pass altPasses[]	= { hdao_alternative_high, hdao_alternative_med, hdao_alternative_low };
		const int iQuality = findString( sm_HDAO_CurrentSettings.m_Quality, g_HDAO_QualityLevels );
		Assert( iQuality < sizeof(mainPasses)/sizeof(mainPasses[0]) && sizeof(mainPasses)==sizeof(altPasses) );
		RenderToTarget_HDAO( m_SSAORT[0], "HDAO: Compute at half resolution", false,
			(sm_HDAO_CurrentSettings.m_Alternative ? altPasses : mainPasses)[iQuality] );
	}
	
#if HDAO_PROFILE_SIZE
	const float fComputeTime = hdao::gpuTimer.Stop();
	hdao::fTimeComputeStore[hdao::nCurrentFrame] = fComputeTime;
	hdao::gpuTimer.Start();
#endif

	const hdao_pass blurPasses[] = { hdao_blur_variable, hdao_blur_fixed_2d, hdao_blur_smart };
	Assert( hdao::iBlur>=0 && hdao::iBlur < sizeof(blurPasses)/sizeof(blurPasses[0]) );
	const grcBlendStateHandle &applyBlendState	= bApply ? m_Apply_B : grcStateBlock::BS_Default;
	grcRenderTarget *const applyTarget			= bApply ? GBuffer::GetTarget(GBUFFER_RT_3) : m_SSAORT[1];

	hdao::shader->SetVar( hdao::varOcclusionTextureParams, Vector4( 1.0f/(float)m_SSAORT[0]->GetWidth(), 1.0f/(float)m_SSAORT[0]->GetHeight(), 0.0f, 0.0f) );

	if (hdao::iBlur == 1)	// single-pass
	{
		// 2: blur and apply
		grcStateBlock::SetBlendState(applyBlendState);
		hdao::shader->SetVar( hdao::varOcclusionTexture, m_SSAORT[0]);
		hdao::shader->SetVar( hdao::varApplyParams,	 Vector4(0.f,0.f,hdao::fBlurThreshold,sm_HDAO_CurrentSettings.m_Strength) );
		RenderToTarget_HDAO( applyTarget, "HDAO: Blur and apply", false, hdao_blur_fixed_2d );
	}else
	{
		if (bCalculate)
		{
			// 2: blur X
			hdao::shader->SetVar( hdao::varOcclusionTexture, m_SSAORT[0]);
			hdao::shader->SetVar( hdao::varApplyParams,	 Vector4(1.f,0.f,hdao::fBlurThreshold,0.f) );
			RenderToTarget_HDAO( m_SSAORT[1], "HDAO: Horizontal blur", false, blurPasses[hdao::iBlur] );
		
			// 3: blur Y
			hdao::shader->SetVar( hdao::varOcclusionTexture, m_SSAORT[1]);
			hdao::shader->SetVar( hdao::varApplyParams,	 Vector4(0.f,1.f,hdao::fBlurThreshold,0.f) );
			RenderToTarget_HDAO( m_SSAORT[0], "HDAO: Vertical blur", false, blurPasses[hdao::iBlur] );
		}
		// 4: apply
		grcStateBlock::SetBlendState(applyBlendState);
		hdao::shader->SetVar( hdao::varOcclusionTexture, m_SSAORT[0]);
		hdao::shader->SetVar( hdao::varApplyParams,	 Vector4(0.f,0.f,0.f,sm_HDAO_CurrentSettings.m_Strength) );
		RenderToTarget_HDAO( applyTarget, "HDAO: Apply", true, hdao_blur_fixed_2d );
	}

#if HDAO_PROFILE_SIZE
	const float fApplyTime = hdao::gpuTimer.Stop();
	hdao::fTimeComputeStore[hdao::nCurrentFrame] = fApplyTime;
	float fComputeTotal = 0.f, fApplyTotal = 0.f;
	for (int i=0; i<hdao::nFramesAveraging; ++i)
	{
		fComputeTotal += hdao::fTimeComputeStore[i];
		fApplyTotal += hdao::fTimeApplyStore[i];
	}
	hdao::fTimeComputeAvg	= fComputeTotal	/ hdao::nFramesAveraging;
	hdao::fTimeApplyAvg		= fApplyTotal	/ hdao::nFramesAveraging;
#endif	//HDAO_PROFILE_SIZE
}

void SSAO::RenderToTarget_HDAO(grcRenderTarget *pTarget, const char pDebugStr[], bool apply, hdao_pass Pass)
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, pTarget, NULL );

	const float W = (float)pTarget->GetWidth();
	const float H = (float)pTarget->GetHeight();
	Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
	hdao::shader->SetVar( hdao::varTargetSize, TargetSize );

	DrawFullScreenQuad_HDAO(pDebugStr, apply, Pass);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
}

void SSAO::DrawFullScreenQuad_HDAO(const char pDebugStr[], bool apply, hdao_pass Pass)
{
	ShaderUtil::StartShader(pDebugStr, hdao::shader, apply ? hdao::techniqueApply : hdao::technique, GetHDAOPass(apply ? 0 : Pass));
#if SSAO_UNIT_QUAD
	FastQuad::Draw(true);
#else
	grcBeginQuads(1);
	grcDrawQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	grcEndQuads();
#endif	//SSAO_UNIT_QUAD
	ShaderUtil::EndShader(hdao::shader);
}


#if __BANK

void SSAO::AddWidgets_HDAO(bkBank &bk)
{
	bkGroup *const g = s_HDAO_BankGroup = bk.AddGroup("=== HDAO settings ===",false);
	g->AddCombo("Preset", &SSAO::sm_HDAO_QualityLevelBeingEdited, HDAO_CONFIG_MAX_QUALITY_LEVELS, g_HDAO_QualityLevels, 0, datCallback(CFA(SSAO::OnComboBox_HDAO)));
	g->AddButton("Save", SSAO::SaveParams_HDAO);
	g->AddButton("Load", SSAO::LoadParams_HDAO);

#if HDAO_PROFILE_SIZE
	g->AddSlider("Avg over frames", &hdao::nFramesAveraging, 1,HDAO_PROFILE_SIZE,1);
	g->AddText("Avg Compute time",	&hdao::fTimeComputeAvg,	true);
	g->AddText("Avg Apply time",	&hdao::fTimeApplyAvg,	true);
#endif	//HDAO_PROFILE

	HDAO_Parameters &par = sm_HDAO_CurrentSettings;
	g->AddTitle("(c) == configurable, (a) == alternative mode only");
	g->AddToggle("Alternative",	&par.m_Alternative);
	g->AddCombo("[c] Blur",	&hdao::iBlur, HDAO_CONFIG_MAX_BLUR_TYPES, g_HDAO_BlurTypes, 0);
	g->AddSlider("[c] Strength",			&par.m_Strength, 0.01f, 5.0f, 0.1f);
	g->AddSlider("Blur depth threshold",	&hdao::fBlurThreshold, 0.0f, 0.1f, 0.001f);
	g->AddSlider("[c] Reject radius",		&par.m_RejectRadius, 0.0f, 1.f, 0.1f);
	g->AddSlider("[c] Accept radius",		&par.m_AcceptRadius, 0.0f, 1.f, 0.01f);
	g->AddSlider("[c] Fade-out distance",	&par.m_FadeoutDist, 0.f, 10.f, 0.1f);
	g->AddSlider("[c] Normal scale",		&par.m_NormalScale, 0.f, 1.f, 0.01f);
	g->AddSlider("Dot offset",				&hdao::fDotOffset, -1.f, 1.f, 0.01f);
	g->AddSlider("Dot scale",				&hdao::fDotScale, 0.f, 10.f, 0.1f);
	g->AddSlider("Dot power",				&hdao::fDotPower, 0.5f, 10.f, 0.5f);
	g->AddSlider("[ac] World space",		&par.m_WorldSpace, 0.f, 1.f, 0.1f);
	g->AddSlider("[ac] Target scale",		&par.m_TargetScale, 0.01f, 0.1f, 0.01f);
	g->AddSlider("[ac] Radius scale",		&par.m_RadiusScale, 0.5f, 5.f, 0.1f);
	g->AddSlider("[ac] Base weight",		&par.m_BaseWeight, 0.f, 1.f, 0.1f);
}

void SSAO::Debug_HDAO()
{
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	const float W = (float)GRCDEVICE.GetWidth();
	const float H = (float)GRCDEVICE.GetHeight();
	Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
	hdao::shader->SetVar( hdao::varTargetSize, TargetSize );
	hdao::shader->SetVar( hdao::varOcclusionTexture, m_SSAORT[0]);

	if (hdao::iBlur == 1)
	{
		hdao::shader->SetVar( hdao::varApplyParams,	 Vector4(0.f,0.f,hdao::fBlurThreshold,sm_HDAO_CurrentSettings.m_Strength) );
		DrawFullScreenQuad_HDAO( "HDAO: Blur and apply (debug)", false, hdao_blur_fixed_2d );
	}else
	{
		hdao::shader->SetVar( hdao::varApplyParams,	 Vector4(0.f,0.f,0.f,sm_HDAO_CurrentSettings.m_Strength) );
		DrawFullScreenQuad_HDAO( "HDAO: Apply (debug)", true, hdao_blur_fixed_2d );
	}
}
#endif	//__BANK

#endif	//SUPPORT_HDAO

#if SUPPORT_HDAO2
#if __BANK
void SSAO::SaveParams_HDAO2(void)
{
	sm_HDAO2_Settings.m_QualityLevels[sm_HDAO2_CurrentQualityLevel] = sm_HDAO2_CurrentSettings;
	PARSER.SaveObject("platform:/data/HDAO2Settings", "xml", &sm_HDAO2_Settings, parManager::XML);
}
void SSAO::LoadParams_HDAO2(void)
{
	PARSER.LoadObject("platform:/data/HDAO2Settings", "xml", sm_HDAO2_Settings);
	SSAO::UpdateSettingsToUse_HDAO2(sm_HDAO2_QualityLevelBeingEdited);

}
void SSAO::OnComboBox_HDAO2(void)
{
	sm_HDAO2_Settings.m_QualityLevels[sm_HDAO2_CurrentQualityLevel] = sm_HDAO2_CurrentSettings;
	SSAO::UpdateSettingsToUse_HDAO2(sm_HDAO2_QualityLevelBeingEdited);
}
#endif	//__BANK

void SSAO::Reset_HDAO2Shaders()
{
	delete hdao2::shader;
	Init_HDAO2Shaders();
}

void SSAO::Init_HDAO2Shaders()
{
	// Load shader
	ASSET.PushFolder("common:/shaders");
	hdao2::shader = grmShaderFactory::GetInstance().Create();
	hdao2::shader->Load( GRCDEVICE.GetMSAA()>1 ? "HDAO2_MS" : "HDAO2" ); // Use the non-msaa version of the shader for 4s1f EQAA
	hdao2::technique	= hdao2::shader->LookupTechnique( "HDAO" );
	ASSET.PopFolder();

	// Initialize variables
	hdao2::varResultTexture			= hdao2::shader->LookupVar("g_ResultTexture");
	hdao2::varOcclusionTexture		= hdao2::shader->LookupVar("occlusionTexture");
	hdao2::varDepthTexture			= hdao2::shader->LookupVar("depthTexture");
	hdao2::varNormalTexture			= hdao2::shader->LookupVar("normalTexture");
	hdao2::varOrigDepthTexture		= hdao2::shader->LookupVar("depthOrigTexture");
	hdao2::varOrigNormalTexture		= hdao2::shader->LookupVar("normalOrigTexture");
	hdao2::varParams1				= hdao2::shader->LookupVar("HDAO_Params1");
	hdao2::varParams2				= hdao2::shader->LookupVar("HDAO_Params2");
	hdao2::varProjectionParams		= hdao2::shader->LookupVar("projectionParams");
	hdao2::varProjectionShear		= hdao2::shader->LookupVar("projectionShear");
	hdao2::varTargetSize			= hdao2::shader->LookupVar("targetSize");
	hdao2::varOrigDepthTexParams	= hdao2::shader->LookupVar("origDepthTexParams");
}

void SSAO::Init_HDAO2(bool bCore)
{
	if (!bCore)
	{
		return;
	}

	// Load settings
	int Quality = 1;
	PARAM_hdao_quality.Get(Quality);

	if((Quality < 0) || (Quality >= HDAO_CONFIG_MAX_QUALITY_LEVELS))
	{
		AssertMsg(false, "SSAO::Init_HDAO2()...Resolution out of range!");
		Quality = 0;
	}

	PARSER.LoadObject("platform:/data/HDAO2Settings", "xml", sm_HDAO2_Settings);
	BANK_ONLY( sm_HDAO2_QualityLevelBeingEdited = Quality );
	UpdateSettingsToUse_HDAO2(Quality);
	
	CreateRendertargets_HDAO2();
	Init_HDAO2Shaders();

	Vector4 tmp;
	hdao2::shader->GetVar( hdao2::varParams1,	tmp );
	sm_HDAO2_CurrentSettings.m_Strength		= tmp.x;
	sm_HDAO2_CurrentSettings.m_NormalScale	= tmp.y;
	hdao2::shader->GetVar( hdao2::varParams2,	tmp );
	sm_HDAO2_CurrentSettings.m_RejectRadius	= tmp.x;
	sm_HDAO2_CurrentSettings.m_AcceptRadius	= tmp.y;
	sm_HDAO2_CurrentSettings.m_FadeoutDist	= 1.f / tmp.z;
}

void SSAO::Shutdown_HDAO2(bool bCore)
{
	if (bCore)
	{
		delete hdao2::shader;
		hdao2::shader = NULL;
		DestroyRendertargets_HDAO2();
	}
}


void SSAO::CreateRendertargets_HDAO2()
{
	if (!m_enable) return;

	// Initialize render targets
	int width	= GRCDEVICE.GetWidth()	>> 1;
	int height	= GRCDEVICE.GetHeight()	>> 1;
	grcTextureFactory::CreateParams params;
	int bpp = 32;
	params.HasParent	= true;
	params.Parent		= NULL;
	params.Format		= grctfR11G11B10F;
	hdao2::Normal		= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget( "HDAO2_Normal",	grcrtPermanent, width, height, bpp, &params);
	params.Format		= SSAO_OUTPUT_DEPTH ? grctfD24S8 : grctfR32F;
	hdao2::Depth		= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget( "HDAO2_Depth",	SSAO_OUTPUT_DEPTH ? grcrtDepthBuffer : grcrtPermanent, width, height, bpp, &params);

	params.Format		= grctfA8R8G8B8;
	hdao2::GBuffer1_Resolved = !__DEV ? NULL : (GRCDEVICE.GetMSAA()>1 ?
		grcTextureFactory::GetInstance().CreateRenderTarget( "HDAO2_GBuffer1_Resolved",	grcrtPermanent, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), bpp, &params) :
		GBuffer::GetTarget( GBUFFER_RT_1 ));

	if (GRCDEVICE.GetMSAA()>1 && HDAO2_MULTISAMPLE_FILTER)
	{
		params.Format		= grctfA8R8G8B8;
		bpp = 32;
	}else
	{
		params.Format		= grctfL8;
		bpp = 8;
	}

	params.UseAsUAV		= true;
	hdao2::ResultAO		= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget( "HDAO2_Result",	grcrtPermanent, width, height, bpp, &params);
	width	= GRCDEVICE.GetWidth();
	height	= GRCDEVICE.GetHeight();
	hdao2::TempAO1		= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget( "HDAO2_Temp1",	grcrtPermanent, width, height, bpp, &params);
	hdao2::TempAO2		= !__DEV ? NULL : grcTextureFactory::GetInstance().CreateRenderTarget( "HDAO2_Temp2",	grcrtPermanent, width, height, bpp, &params);

}

void SSAO::DestroyRendertargets_HDAO2()
{
	if(hdao2::TempAO2)
	{
		delete hdao2::TempAO2;
		hdao2::TempAO2 = NULL;
	}

	if(hdao2::TempAO1)
	{
		delete hdao2::TempAO1;
		hdao2::TempAO1 = NULL;
	}

	if(hdao2::ResultAO)
	{
		delete hdao2::ResultAO;
		hdao2::ResultAO = NULL;
	}

	if(GRCDEVICE.GetMSAA()>1)
	{
		if(hdao2::GBuffer1_Resolved)
		{
			delete hdao2::GBuffer1_Resolved;
			hdao2::GBuffer1_Resolved = NULL;
		}
	}
	if(hdao2::Depth)
	{
		delete hdao2::Depth;
		hdao2::Depth = NULL;
	}
	if(hdao2::Normal)
	{
		delete hdao2::Normal;
		hdao2::Normal = NULL;
	}
}


static void ComputeShaderPass(const GPUTimebarIds tid, const char pDebugStr[], grcRenderTarget *const target, const int pass, int groupSizeX, int groupSizeY)
{
#if DETAIL_SSAO_GPU_TIMEBARS
	dlCmdPushGPUTimebar(tid, pDebugStr).Execute();
#else
	(void)tid;
#endif
	hdao2::shader->SetVarUAV( hdao2::varResultTexture, static_cast<grcTextureUAV*>( target ) );
	
	GRCDEVICE.RunComputation( pDebugStr, *hdao2::shader, (u8)pass-1,	//converting to CS program ID
		(target->GetWidth()-1)	/groupSizeX + 1,
		(target->GetHeight()-1)	/groupSizeY + 1,
		1 );
#if DETAIL_SSAO_GPU_TIMEBARS
	dlCmdPopGPUTimebar().Execute();
#endif
}


void SSAO::CalculateAndApply_HDAO2(bool bCalculate, bool bApply)
{
	Assertf( hdao2::shader, "HDAO2 is not initialized" );
	grcRenderTarget *computeTarget = NULL;
	
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_SSAO_DS);
	grcStateBlock::SetRasterizerState(m_SSAO_R);
	
	// Set projection parameters
	const Vector4 projParams = CalculateProjectionParams_PreserveTanFOV();
	hdao2::shader->SetVar( hdao2::varProjectionParams, projParams );

	// Set perspective shear
	const grcViewport *const vp = grcViewport::GetCurrent();
	const Vector2 shear = vp->GetPerspectiveShear();
	const Vector4 shearParams( shear.x, shear.y, 0.f, 0.f );
	hdao2::shader->SetVar( hdao2::varProjectionShear, shearParams );

	hdao2::shader->SetVar( hdao2::varOrigNormalTexture,	GBuffer::GetTarget(GBUFFER_RT_1) );
	hdao2::shader->SetVar( hdao2::varOrigDepthTexture,	GBuffer::GetDepthTarget() );

	float depthSizeX = (float)GBuffer::GetDepthTarget()->GetWidth();
	float depthSizeY = (float)GBuffer::GetDepthTarget()->GetHeight();
	hdao2::shader->SetVar(hdao2::varOrigDepthTexParams, Vector4(depthSizeX, depthSizeY, 1.f/depthSizeX, 1.f/depthSizeY));

	if (GRCDEVICE.GetMSAA()>1)
	{
		// resolve normals
		static_cast<grcRenderTargetMSAA*>( GBuffer::GetTarget(GBUFFER_RT_1) )->Resolve( hdao2::GBuffer1_Resolved );
	}

	hdao2::shader->SetVar( hdao2::varNormalTexture,	hdao2::GBuffer1_Resolved );
	hdao2::shader->SetVar( hdao2::varDepthTexture,	CRenderTargets::GetDepthResolved() );

	//step-1: downsample depth,normal
	if (sm_HDAO2_CurrentSettings.m_UseLowRes)
	{	
#if SSAO_OUTPUT_DEPTH
		grcStateBlock::SetDepthStencilState( grcStateBlock::DSS_ForceDepth );
		grcTextureFactory::GetInstance().LockRenderTarget( 0, hdao2::Normal, hdao2::Depth );
#else
		const grcRenderTarget* rendertargets[grcmrtColorCount] = {
			hdao2::Normal,
			hdao2::Depth,
			NULL,
			NULL
		};

		grcTextureFactory::GetInstance().LockMRT( rendertargets, NULL );
#endif	//SSAO_OUTPUT_DEPTH
		{
			const float W = (float)hdao2::Normal->GetWidth();
			const float H = (float)hdao2::Normal->GetHeight();
			Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
			hdao2::shader->SetVar( hdao2::varTargetSize, TargetSize );
		}

		DrawFullScreenQuad_HDAO2( GT_SSAO_PREPARE, "HDAO2: Down-sample", hdao2_downscale );

#if SSAO_OUTPUT_DEPTH
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		grcStateBlock::SetDepthStencilState(m_SSAO_DS);
#else
		grcTextureFactory::GetInstance().UnlockMRT();
#endif	//SSAO_OUTPUT_DEPTH

		hdao2::shader->SetVar( hdao2::varNormalTexture,	hdao2::Normal );
		hdao2::shader->SetVar( hdao2::varDepthTexture,	hdao2::Depth );
		computeTarget = hdao2::ResultAO;
	}
	else
	{
		computeTarget = hdao2::TempAO2;
	}

	if (bCalculate)
	{
		if (false)
		{
			//step-2a: clean result texture
			grcTextureFactory::GetInstance().LockRenderTarget(0,hdao2::ResultAO,NULL);
			GRCDEVICE.Clear(true,Color32(0),false,0.f,false,0);
			const grcResolveFlags flags;
			grcTextureFactory::GetInstance().UnlockRenderTarget(0,&flags);
		}

		//step-2: compute CS
		const hdao2_pass mainPasses[] = { hdao2_compute_low, hdao2_compute_medium, hdao2_compute_high };
		const int iQuality = findString( sm_HDAO2_CurrentSettings.m_Quality, g_HDAO_QualityLevels );
		Assert( iQuality < sizeof(mainPasses)/sizeof(mainPasses[0]) );
		const hdao2_pass computePass = mainPasses[iQuality];

		float kX = 1.f, kY = 1.f;
		// scaling the radius would require bumping HDAO2_GROUP_TEXEL_OVERLAP,
		// which introduces a whole lot of issues with compute shader constants
		if (false && sm_HDAO2_CurrentSettings.m_RadiusScale != 0.f)
		{	// compute radius scale
			const int originalWidth = 640, originalHeight = 360;
			kX = computeTarget->GetWidth()	* sm_HDAO2_CurrentSettings.m_RadiusScale / originalWidth;
			kY = computeTarget->GetHeight()	* sm_HDAO2_CurrentSettings.m_RadiusScale / originalHeight;
		}
		// update compute parameters
		hdao2::shader->SetVar( hdao2::varParams1,	Vector4(
			sm_HDAO2_CurrentSettings.m_Strength,
			sm_HDAO2_CurrentSettings.m_NormalScale,
			kX, kY ));
		hdao2::shader->SetVar( hdao2::varParams2,	Vector4(
			sm_HDAO2_CurrentSettings.m_RejectRadius,
			sm_HDAO2_CurrentSettings.m_AcceptRadius,
			1.f / sm_HDAO2_CurrentSettings.m_FadeoutDist,
			0.f ));

		ComputeShaderPass( GT_SSAO_COMPUTE, "HDAO2: Compute", computeTarget, computePass, HDAO2_GROUP_THREAD_DIM, HDAO2_GROUP_THREAD_DIM );

		//step-3: filter X and Y
		if (sm_HDAO2_CurrentSettings.m_UseFilter)
		{
			hdao2::shader->SetVar( hdao2::varNormalTexture,	GBuffer::GetTarget(GBUFFER_RT_1) );
			hdao2::shader->SetVar( hdao2::varDepthTexture,	CRenderTargets::GetDepthResolved() );
			const float W = (float)hdao2::TempAO1->GetWidth();
			const float H = (float)hdao2::TempAO1->GetHeight();
			Vector4 TargetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
			hdao2::shader->SetVar( hdao2::varTargetSize, TargetSize );
			hdao2::shader->SetVar( hdao2::varOcclusionTexture, computeTarget );
			ComputeShaderPass( GT_SSAO_FILTER, "HDAO2: FilterX", hdao2::TempAO1,	hdao2_filter_hor, HDAO2_RUN_SIZE, HDAO2_RUN_LINES );
			hdao2::shader->SetVar( hdao2::varOcclusionTexture, hdao2::TempAO1 );
			ComputeShaderPass( GT_SSAO_FILTER, "HDAO2: FilterY", hdao2::TempAO2,	hdao2_filter_ver, HDAO2_RUN_LINES, HDAO2_RUN_SIZE );
			computeTarget = hdao2::TempAO2;
		}
		//step-3-final: sync
		GRCDEVICE.SynchronizeComputeToGraphics();
	}

	if (bApply)
	{
		//step-4: apply
		grcStateBlock::SetBlendState(m_Apply_B);
		hdao2::shader->SetVar( hdao2::varOcclusionTexture, computeTarget );	
		grcTextureFactory::GetInstance().LockRenderTarget( 0, GBuffer::GetTarget(GBUFFER_RT_3), NULL );
		DrawFullScreenQuad_HDAO2( GT_SSAO_FILTER, "HDAO2: Apply", hdao2_apply );
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}
}

void SSAO::DrawFullScreenQuad_HDAO2(GPUTimebarIds tid, const char pDebugStr[], hdao2_pass Pass)
{
#if DETAIL_SSAO_GPU_TIMEBARS
	dlCmdPushGPUTimebar(tid, pDebugStr).Execute();
#else
	(void)tid;
#endif
	ShaderUtil::StartShader(pDebugStr, hdao2::shader, hdao2::technique, GetHDAOPass(Pass));
#if SSAO_UNIT_QUAD
	FastQuad::Draw(true);
#else
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
#endif	//SSAO_UNIT_QUAD
	ShaderUtil::EndShader(hdao2::shader);
#if DETAIL_SSAO_GPU_TIMEBARS
	dlCmdPopGPUTimebar().Execute();
#endif
}

#if __BANK
void SSAO::AddWidgets_HDAO2(bkBank &bk)
{
	bkGroup *const g = s_HDAO2_BankGroup = bk.AddGroup("=== HDAO-2 settings ===",false);
	g->AddCombo("Preset", &SSAO::sm_HDAO2_QualityLevelBeingEdited, HDAO_CONFIG_MAX_QUALITY_LEVELS, g_HDAO_QualityLevels, 0, datCallback(CFA(SSAO::OnComboBox_HDAO2)));
	g->AddButton("Save", SSAO::SaveParams_HDAO2);
	g->AddButton("Load", SSAO::LoadParams_HDAO2);

	HDAO2_Parameters &par = sm_HDAO2_CurrentSettings;
	g->AddToggle("Compute low resolution",	&par.m_UseLowRes);
	g->AddToggle("Use filtering",			&par.m_UseFilter);
	g->AddSlider("Strength",			&par.m_Strength, 0.01f, 5.0f, 0.5f);
	g->AddSlider("Reject radius",		&par.m_RejectRadius, 0.0f, 2.f, 0.8f);
	g->AddSlider("Accept radius",		&par.m_AcceptRadius, 0.0f, 1.f, 0.003f);
	g->AddSlider("Fade-out distance",	&par.m_FadeoutDist, 0.f, 10.f, 0.6f);
	g->AddSlider("Normal scale",		&par.m_NormalScale, 0.f, 1.f, 0.05f);
	//g->AddSlider("Radius scale",		&par.m_RadiusScale, 0.f, 5.f, 0.1f);
}

void SSAO::Debug_HDAO2()
{
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	hdao2::shader->SetVar( hdao2::varOcclusionTexture, GetSSAOTexture() );	
	DrawFullScreenQuad_HDAO2( GT_SSAO_FILTER, "HDAO2: Apply (debug)", hdao2_apply );
}

#endif //__BANK
#endif	//SUPPORT_HDAO2

/*--------------------------------------------------------------------------------------------------*/
#if SUPPORT_HBAO

void SSAO::Init_HBAO(bool bCore)
{
	if (!bCore)
	{
		return;
	}

	InitShaders_HBAO();

	CreateRendertargets_HBAO();

	// Load in the settings.	
	PARSER.LoadObject("common:/data/HBAOSettings", "xml", sm_HBAO_CurrentSettings);
}

void SSAO::InitShaders_HBAO()
{
	//Variables
	m_variables[ssao_extra_params0]				= m_shaderSSAO->LookupVar("ExtraParams0");
	m_variables[ssao_extra_params1]				= m_shaderSSAO->LookupVar("ExtraParams1");
	m_variables[ssao_extra_params2]				= m_shaderSSAO->LookupVar("ExtraParams2");
	m_variables[ssao_extra_params3]				= m_shaderSSAO->LookupVar("ExtraParams3");
	m_variables[ssao_extra_params4]				= m_shaderSSAO->LookupVar("ExtraParams4");

	m_variables[ssao_perspective_shear_params0]	= m_shaderSSAO->LookupVar("PerspectiveShearParams0");
	m_variables[ssao_perspective_shear_params1]	= m_shaderSSAO->LookupVar("PerspectiveShearParams1");
	m_variables[ssao_perspective_shear_params2]	= m_shaderSSAO->LookupVar("PerspectiveShearParams2");

	m_variables[ssao_prev_viewproj]				= m_shaderSSAO->LookupVar("PrevViewProj");

	m_variables[ssao_stencil_pointtexture]		= m_shaderSSAO->LookupVar("StencilCopyTexture");
}

void SSAO::Shutdown_HBAO(bool bCore)
{
	if (bCore)
	{
		DestroyRendertargets_HBAO();
	}
}

void SSAO::CreateRendertargets_HBAO()
{
	WIN32PC_ONLY(if(!m_useNextGenVersion) { return; })

	s32 width	= VideoResManager::GetSceneWidth();
	s32 height	= VideoResManager::GetSceneHeight();

	grcTextureFactory::CreateParams params;
	params.HasParent				= true;
	params.Parent					= GBuffer::GetTarget(GBUFFER_RT_3);
	params.AllocateFromPoolOnCreate	= false;
	params.Format			= grctfL8;
	hbao::ContinuityRT[0]		= grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO Continuity mask", grcrtPermanent, width/2, height/2, 8, &params);
	hbao::ContinuityRT[1]		= grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO Continuity mask Dilated", grcrtPermanent, width/2, height/2, 8, &params);
	params.Format			= grctfG16R16F;

#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		// can't use this in multi-GPU setting
		hbao::bIsTemporalEnabled = false;
		hbao::bIsTemporalFirstUsed = false;
		hbao::HistoryRT = NULL;
	}
	else
#endif
	{
		hbao::HistoryRT			= grcTextureFactory::GetInstance().CreateRenderTarget(	"SSAO History", grcrtPermanent, width/2, height/2, 32, &params);
		hbao::bIsTemporalFirstUsed = true;
	}
}

void SSAO::DestroyRendertargets_HBAO()
{
	if(hbao::ContinuityRT[0])
	{
		delete hbao::ContinuityRT[0];
		hbao::ContinuityRT[0] = NULL;
	}
	if(hbao::ContinuityRT[1])
	{
		delete hbao::ContinuityRT[1];
		hbao::ContinuityRT[1] = NULL;
	}
	if(hbao::HistoryRT)
	{
		delete hbao::HistoryRT;
		hbao::HistoryRT = NULL;
	}
}

void SSAO::CalculateAndApply_HBAO(bool bDoCalculate, bool bDoApply)
{
	if(bDoCalculate)
	{
		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], CRenderTargets::GetDepthBufferQuarterLinear());
		grcResolveFlags* resolveFlags = NULL;

		grcRenderTarget* SSAORT		= m_SSAORT[0];
		grcRenderTarget* BlurXRT	= m_SSAORT[1];
		grcRenderTarget* BlurYRT	= m_SSAORT[0];

		grcTextureFactory::GetInstance().LockRenderTarget(0, SSAORT, NULL );
		CShaderLib::UpdateGlobalDevScreenSize();


		const float tanAngBias = tanf(hbao::fAngBias * 3.14159265f / 180.0f);		

		const float hbaoRadius0			= rage::Max(sm_HBAO_CurrentSettings.m_HBAORadius0, 1e-3f);
		const float hbaoRadius1			= rage::Max(sm_HBAO_CurrentSettings.m_HBAORadius1, 1e-3f);
		const float cpRadius			= sm_HBAO_CurrentSettings.m_CPRadius;
		const float invBlendDistance	= 1.0f / rage::Max(sm_HBAO_CurrentSettings.m_HBAOBlendDistance, 1e-3f);

		const float hbaoStrength		= sm_HBAO_CurrentSettings.m_HBAORelativeStrength * m_strength;
		const float cpStrength			= sm_HBAO_CurrentSettings.m_CPRelativeStrength * m_strength;
		const float foliageStrength		= 1.0f - sm_HBAO_CurrentSettings.m_FoliageStrength;

		const float hbaoMaxPixels		= sm_HBAO_CurrentSettings.m_MaxPixels;
		const float invCutoffPixel		= 1.0f / rage::Max(sm_HBAO_CurrentSettings.m_CutoffPixels, 1.0f);		

		const float hbaoFalloffExponent = rage::Max(sm_HBAO_CurrentSettings.m_HBAOFalloffExponent, 1e-3f);
		const float CPBlendMul	= 1.0f / rage::Max(sm_HBAO_CurrentSettings.m_CPBlendDistanceMax - sm_HBAO_CurrentSettings.m_CPBlendDistanceMin, 1e-3f);
		const float CPBlendAdd	= -sm_HBAO_CurrentSettings.m_CPBlendDistanceMin * CPBlendMul;
		const float cpStrengthClose		= sm_HBAO_CurrentSettings.m_CPStrengthClose * m_strength;

		const Vec4f ssaoParams0(hbaoRadius0, hbaoRadius1, cpRadius, invBlendDistance);		
		const Vec4f ssaoParams1(hbaoStrength, cpStrength, hbaoMaxPixels, invCutoffPixel);		
		const Vec4f ssaoParams2(hbao::fHBAOMinMulSwitch, tanAngBias, hbao::fTemporalThreshold, hbao::fContinuityThreshold);
		const Vec4f	ssaoParams3(foliageStrength, hbaoFalloffExponent, cpStrengthClose, 0.0f);
		const Vec4f	ssaoParams4(CPBlendMul, CPBlendAdd, 0.0f, 0.0f);

		m_shaderSSAO->SetVar(m_variables[ssao_extra_params0], ssaoParams0);
		m_shaderSSAO->SetVar(m_variables[ssao_extra_params1], ssaoParams1);
		m_shaderSSAO->SetVar(m_variables[ssao_extra_params2], ssaoParams2);
		m_shaderSSAO->SetVar(m_variables[ssao_extra_params3], ssaoParams3);
		m_shaderSSAO->SetVar(m_variables[ssao_extra_params4], ssaoParams4);

		int pass = hbao_solo;

		if(hbao::bIsHybridEnabled && !hbao::bIsUseNormalsEnabled)
		{
			pass = hbao_hybrid;
		}
		else if(!hbao::bIsHybridEnabled && hbao::bIsUseNormalsEnabled)
		{
			pass = hbao_normal;
		}
		else if(hbao::bIsUseNormalsEnabled && hbao::bIsHybridEnabled)
		{
			pass = hbao_normal_hybrid;
		}

#if __D3D11 || RSG_ORBIS
		m_shaderSSAO->SetVar(m_variables[ssao_stencil_pointtexture], CRenderTargets::GetDepthBuffer_Stencil());
#endif
		SetMSSAPointTexture1(GBuffer::GetTarget(GBUFFER_RT_1));
		ShaderUtil::StartShader("HBAO Process", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(pass),true,SSAOShmoo);
		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
		ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, resolveFlags);

		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], CRenderTargets::GetDepthBufferQuarterLinear());

		//continuity mask
		grcTextureFactory::GetInstance().LockRenderTarget(0, hbao::ContinuityRT[0], NULL);
		ShaderUtil::StartShader("HBAO Continuity Mask", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(hbao_continuity_mask),true,SSAOShmoo);
		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
		ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, resolveFlags);

		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1], hbao::ContinuityRT[0]);

		//dilate continuity mask
		int continuity_ssao_pass = hbao_dilate_continuity_mask_sm50;
#if RSG_PC
		u32	shaderModelMajor, shaderModelMinor;
		

		GRCDEVICE.GetDxShaderModel(shaderModelMajor, shaderModelMinor);
		if(shaderModelMajor != 5)
		{
			continuity_ssao_pass = hbao_dilate_continuity_mask_nogather;
		}
#endif
		grcTextureFactory::GetInstance().LockRenderTarget(0, hbao::ContinuityRT[1], NULL);
		ShaderUtil::StartShader("HBAO Dilate Continuity Mask", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(continuity_ssao_pass),true,SSAOShmoo);
		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
		ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, resolveFlags);


		m_shaderSSAO->SetVar(m_variables[ssao_pointtexture2], CRenderTargets::GetDepthBufferQuarterLinear());
		grcTextureFactory::GetInstance().LockRenderTarget(0, BlurXRT, NULL );
		m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture0p], SSAORT);
		ShaderUtil::StartShader("HBAO Gauss Blur X", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_blur_gauss_bilateral_x),true,SSAOShmoo);
		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
		ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, resolveFlags); 

		if(hbao::bIsTemporalEnabled && hbao::bIsTemporalFirstUsed)
		{
			hbao::bIsTemporalFirstUsed = false;			
			hbao::matPrevViewProj =  grcViewport::GetCurrent()->GetCompositeMtx();

			grcTextureFactory::GetInstance().LockRenderTarget(0, hbao::HistoryRT, NULL );	
			GRCDEVICE.Clear(true, Color32(1.0f, 0.0f, 0.0f, 0.0f), false, 1.0f, 0);
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);				
		}

		grcTextureFactory::GetInstance().LockRenderTarget(0, BlurYRT, NULL );							
		m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture0p], BlurXRT);

		if(hbao::bIsTemporalEnabled)
		{					
			Vec4V projParams;
			Vec3V shearProj0, shearProj1, shearProj2;
			DeferredLighting::GetProjectionShaderParams(projParams, shearProj0, shearProj1, shearProj2);

			m_shaderSSAO->SetVar(m_variables[ssao_perspective_shear_params0], shearProj0);
			m_shaderSSAO->SetVar(m_variables[ssao_perspective_shear_params1], shearProj1);
			m_shaderSSAO->SetVar(m_variables[ssao_perspective_shear_params2], shearProj2);

			m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1], hbao::HistoryRT); 			

			m_shaderSSAO->SetVar(m_variables[ssao_prev_viewproj], hbao::matPrevViewProj);
			m_shaderSSAO->SetVar(m_variables[ssao_pointtexture1], hbao::ContinuityRT[1]);

			ShaderUtil::StartShader("HBAO Gauss Blur Y Temporal", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_blur_gauss_bilateral_y_temporal),true,SSAOShmoo);
		}
		else
		{
			ShaderUtil::StartShader("HBAO Gauss Blur Y", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_blur_gauss_bilateral_y),true,SSAOShmoo);
		}
		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
		ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);

		grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve

		if(hbao::bIsTemporalEnabled)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, hbao::HistoryRT, NULL );

			m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture0p], BlurYRT);

			ShaderUtil::StartShader("SSAO Temporal Copy", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(hbao_temporal_copy),true,SSAOShmoo);
			grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
			ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);

			grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		}

		Mat44V vProjMat = grcViewport::GetCurrent()->GetCompositeMtx();
		hbao::matPrevViewProj = vProjMat;
	}

	if(bDoApply)
	{
		//GBuffer::LockTarget_AmbScaleMod();
		Apply();
		//GBuffer::UnlockTarget_AmbScaleMod(true);
	}
}

#if __BANK
void SSAO::AddWidgets_HBAO(bkBank &bk)
{
	bkGroup *const g = s_HBAO_BankGroup = bk.AddGroup("=== HBAO settings ===",false);

	HBAO_Parameters &par = sm_HBAO_CurrentSettings;
	g->AddButton("Save", SSAO::SaveParams_HBAO);
	g->AddButton("Load", SSAO::LoadParams_HBAO);

	g->AddSlider("HBAO Radius 0", &par.m_HBAORadius0, 0.0f, 5.0f, 0.1f);
	g->AddSlider("HBAO Radius 1", &par.m_HBAORadius1, 0.0f, 5.0f, 0.1f);
	g->AddSlider("HBAO Blend distance (r0 -> r1)", &par.m_HBAOBlendDistance, 0.01f, 50.0f, 0.1f);
	g->AddSlider("HBAO Max pixels", &par.m_MaxPixels, 1.0f, 200.0f, 5.0f);
	g->AddSlider("HBAO Relative Strength", &par.m_HBAORelativeStrength, 0.0f, 5.0f, 0.5f);
	g->AddSlider("CP Relative Strength", &par.m_CPRelativeStrength, 0.0f, 5.0f, 0.5f);
	g->AddSlider("CP Radius", &par.m_CPRadius, 0.0f, 20.0f, 0.5f);
	g->AddSlider("CP Strength Close", &par.m_CPStrengthClose, 0.0f, 5.0f, 0.5f);
	g->AddSlider("CP Blend Distance Min", &par.m_CPBlendDistanceMin, 0.001f, 100.0f, 5.0f);
	g->AddSlider("CP Blend Distance Max", &par.m_CPBlendDistanceMax, 0.001f, 100.0f, 5.0f);
	g->AddSlider("HBAO Hybrid cutoff pixels", &par.m_CutoffPixels, 1.0f, 20.0f, 1.0f);
	g->AddSlider("HBAO Foliage strength", &par.m_FoliageStrength, 0.0f, 1.0f, 0.1f);
	g->AddSlider("HBAO Falloff Exponent", &par.m_HBAOFalloffExponent, 0.001f, 5.0f, 0.2f);

	g->AddSlider("[dev] Angle bias", &hbao::fAngBias, 0.0f, 90.0f, 1.0f);

#if RSG_PC
	if (!GRCDEVICE.UsingMultipleGPUs())
#endif
	g->AddToggle("[dev] Use temporal filtering", &hbao::bIsTemporalEnabled);

	g->AddSlider("[dev] Switch min - mul for HBAO-CP Blending", &hbao::fHBAOMinMulSwitch, 0.0, 1.0f, 1.0f);
	g->AddSlider("[dev] Temporal threshold", &hbao::fTemporalThreshold, 0.0f, 1.0f, 0.1f);
	g->AddSlider("[dev] Continuity threshold", &hbao::fContinuityThreshold, 0.1f, 10.0f, 0.1f);
	g->AddToggle("[dev] Use hybrid solution", &hbao::bIsHybridEnabled);

	g->AddToggle("[dev] Use Normals for HBAO", &hbao::bIsUseNormalsEnabled);	
}

void SSAO::Debug_HBAO()
{
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	m_shaderSSAO->SetVar(m_variables[ssao_deferredlighttexture2],	m_SSAORT[0]);
	m_shaderSSAO->SetVar(m_variables[ssao_lineartexture1],	m_SSAORT[0]);

	ShaderUtil::StartShader("HBAO Debug",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_upscale_isolate_no_power),true,SSAOShmoo);			
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
}

void SSAO::SaveParams_HBAO(void)
{
	PARSER.SaveObject("common:/data/HBAOSettings", "xml", &sm_HBAO_CurrentSettings, parManager::XML);
}
void SSAO::LoadParams_HBAO(void)
{
	PARSER.LoadObject("common:/data/HBAOSettings", "xml", sm_HBAO_CurrentSettings);
}
#endif //__BANK
#endif //SUPPORT_HBAO

/*--------------------------------------------------------------------------------------------------*/

void SSAO::DownscaleDepthToQuarterTarget()
{
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

	const Vector4 projParams = CalculateProjectionParams_PreserveTanFOV();
	m_shaderSSAO->SetVar(m_variables[ssao_project_params], projParams);
	Vector4 ooscreenSize	= Vector4(0.0f, 0.0f, 1.0f/VideoResManager::GetSceneWidth(), 1.0f/VideoResManager::GetSceneHeight());
	m_shaderSSAO->SetVar(m_variables[ssao_deferredlightscreensize],	ooscreenSize);

	float projX				= projParams.GetX();
	float projY				= projParams.GetY();
	float xScale			= 2.0f/VideoResManager::GetSceneWidth();
	float yScale			= 2.0f/VideoResManager::GetSceneHeight();
	Vector4 normalOffset	= Vector4(xScale*projX, 0.0f, 0.0f, yScale*projY);
	m_shaderSSAO->SetVar(m_variables[ssao_normaloffset],	normalOffset);
	
	if((m_CurrentSSAOTechnique >= ssaotechnique_cpqsmix) && (m_CurrentSSAOTechnique <= ssaotechnique_cp8Dirqsmix))
	{
		xScale					*= sm_CPQSMix_CurrentSettings.m_QSRadius;
		yScale					*= sm_CPQSMix_CurrentSettings.m_QSRadius;
	}
	else
	{
		xScale					*= 32.0f;
		yScale					*= 32.0f;
	}
	Vector4	offsetScale0	= Vector4( 0.75f*xScale,  0.75f*yScale,  -0.25f*xScale,  -0.25f*yScale);//75%, 25%
	Vector4	offsetScale1	= Vector4( 1.00f*xScale, -1.00f*yScale,  -0.50f*xScale,   0.50f*yScale);//100%, 50%
	m_shaderSSAO->SetVar(m_variables[ssao_offsetscale0],	offsetScale0);
	m_shaderSSAO->SetVar(m_variables[ssao_offsetscale1],	offsetScale1);

#if __PS3
	m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexturedepth], CRenderTargets::GetDepthBufferQuarterAsColor());
#else
#if SSAO_USEDEPTHRESOLVE
	if(GRCDEVICE.GetMSAA()>1)
	{
		m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexturedepth], CRenderTargets::GetDepthResolved());
	}
	else
#endif	//SSAO_USEDEPTHRESOLVE
		m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexturedepth], GBuffer::GetDepthTarget());
#endif //__PS3

	grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetDepthBufferQuarterLinear(),	NULL);
#if DEVICE_GPU_WAIT //corruption is visible on the bottom corner without a fence here
	GRCDEVICE.GpuWaitOnPreviousWrites();
#endif
	CShaderLib::UpdateGlobalDevScreenSize();
	ShaderUtil::StartShader("SSAO Depth Downsample",	m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(m_SSAO_downscalePass),true,SSAOShmoo);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve
}


#if SSAO_USE_LINEAR_DEPTH_TARGETS
void SSAO::CreateFullScreenLinearDepth()
{
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

	const Vector4 projParams = CalculateProjectionParams_PreserveTanFOV();
	m_shaderSSAO->SetVar(m_variables[ssao_project_params], projParams);
	Vector4 ooscreenSize	= Vector4(0.0f, 0.0f, 1.0f/VideoResManager::GetSceneWidth(), 1.0f/VideoResManager::GetSceneHeight());
	m_shaderSSAO->SetVar(m_variables[ssao_deferredlightscreensize],	ooscreenSize);

	float projX				= projParams.GetX();
	float projY				= projParams.GetY();
	float xScale			= 2.0f/VideoResManager::GetSceneWidth();
	float yScale			= 2.0f/VideoResManager::GetSceneHeight();
	Vector4 normalOffset	= Vector4(xScale*projX, 0.0f, 0.0f, yScale*projY);
	m_shaderSSAO->SetVar(m_variables[ssao_normaloffset],	normalOffset);

	if((m_CurrentSSAOTechnique >= ssaotechnique_cpqsmix) && (m_CurrentSSAOTechnique <= ssaotechnique_cp8Dirqsmix))
	{
		xScale					*= sm_CPQSMix_CurrentSettings.m_QSRadius;
		yScale					*= sm_CPQSMix_CurrentSettings.m_QSRadius;
	}
	else
	{
		xScale					*= 32.0f;
		yScale					*= 32.0f;
	}
	Vector4	offsetScale0	= Vector4( 0.75f*xScale,  0.75f*yScale,  -0.25f*xScale,  -0.25f*yScale);//75%, 25%
	Vector4	offsetScale1	= Vector4( 1.00f*xScale, -1.00f*yScale,  -0.50f*xScale,   0.50f*yScale);//100%, 50%
	m_shaderSSAO->SetVar(m_variables[ssao_offsetscale0],	offsetScale0);
	m_shaderSSAO->SetVar(m_variables[ssao_offsetscale1],	offsetScale1);


#if SSAO_USEDEPTHRESOLVE
	if(GRCDEVICE.GetMSAA()>1)
	{
		m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexturedepth], CRenderTargets::GetDepthResolved());
	}
	else
#endif	//SSAO_USEDEPTHRESOLVE
		m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexturedepth], GBuffer::GetDepthTarget());

#if DEVICE_GPU_WAIT && SSAO_MAKE_QUARTER_LINEAR_FROM_FULL_SCREEN_LINEAR //corruption is visible on the bottom corner without a fence here
	GRCDEVICE.GpuWaitOnPreviousWrites();
#endif // DEVICE_GPU_WAIT && SSAO_MAKE_QUARTER_LINEAR_FROM_FULL_SCREEN_LINEAR

	grcTextureFactory::GetInstance().LockRenderTarget(0, m_FullScreenLinearDepthRT,	NULL);
	ShaderUtil::StartShader("SSAO Full screen linear depth", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_linearizedepth),true,SSAOShmoo);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve
}


void SSAO::CreateQuaterLinearDepth()
{
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

	const Vector4 projParams = CalculateProjectionParams_PreserveTanFOV();
	m_shaderSSAO->SetVar(m_variables[ssao_project_params], projParams);
	Vector4 ooscreenSize	= Vector4(0.0f, 0.0f, 1.0f/VideoResManager::GetSceneWidth(), 1.0f/VideoResManager::GetSceneHeight());
	m_shaderSSAO->SetVar(m_variables[ssao_deferredlightscreensize],	ooscreenSize);
	m_shaderSSAO->SetVar(m_variables[ssao_gbuffertexturedepth], m_FullScreenLinearDepthRT);

	grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetDepthBufferQuarterLinear(),	NULL);
	ShaderUtil::StartShader("SSAO 1/4 linear depth", m_shaderSSAO, m_techniqueSSAO, GetSSAOPass(ssao_downscale_fromlineardepth),true,SSAOShmoo);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
	ShaderUtil::EndShader(m_shaderSSAO,true,SSAOShmoo);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve
}
#endif // SSAO_USE_LINEAR_DEPTH_TARGETS

#if __XENON
grcRenderTarget* SSAO::GetSSAORT()
{
	return m_SSAORT[0];
}
#endif //__XENON

const grcTexture* SSAO::GetSSAOTexture()
{
	if(!m_enable || m_strength < 0.01f)
		return grcTexture::NoneWhite;

	switch(m_CurrentSSAOTechnique)
	{
	case ssaotechnique_qsssao:
#if SUPPORT_QSSSAO2
	case ssaotechnique_qsssao2:
#endif // SUPPORT_QSSSAO2
	case ssaotechnique_cpssao:
	case ssaotechnique_pmssao:
		{
			return m_SSAORT[0];
		}
	case ssaotechnique_cpqsmix:
	case ssaotechnique_cp4Dirqsmix:
	case ssaotechnique_cp8Dirqsmix:
		{
			return m_CPQSMixFullScreenRT;
		}
#if SUPPORT_MRSSAO
	case ssaotechnique_mrssao:
		{
			int LodToApply = 1;
			LodToApply = min(sm_MR_CurrentSettings.m_BottomLod, sm_MR_CurrentSettings.m_LodToApply);
	#if MR_SSAO_FORCE_MAX_LOD1
			LodToApply = max(1, LodToApply);
	#endif //MR_SSAO_FORCE_MAX_LOD1

	#if !MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE
			grcAssertf(0, "SSAO::GetSSAOTexture()...MR SSAO not configured properly. Change MR_SSAO_CONFIGURE_FOR_GET_SSAO_TEXTURE to 1.");
	#endif
			return m_BlurredAOs[LodToApply];
		}
#endif //SUPPORT_MRSSAO
#if SUPPORT_HDAO
	case ssaotechnique_hdao:
		{
			return m_SSAORT[1];
		}
#endif	//SUPPORT_HDAO
#if SUPPORT_HDAO2
	case ssaotechnique_hdao2:
		{
			return sm_HDAO2_CurrentSettings.m_UseFilter || !sm_HDAO2_CurrentSettings.m_UseLowRes ? hdao2::TempAO2 : hdao2::ResultAO;
		}
#endif	//SUPPORT_HDAO2
#if SUPPORT_HBAO
	case ssaotechnique_hbao:
		{
			return m_SSAORT[0];
		}
#endif  //SUPPORT_HBAO
	default:
		{
			Assertf(false, "SSAO::GetSSAOTexture()...Invalid SSAO technique (%d).", m_CurrentSSAOTechnique);
			return NULL;
		}
	}
}


#if RSG_PC || RSG_DURANGO || RSG_ORBIS

void SSAO::SetMSSAPointTexture1(grcRenderTarget *pRenderTarget)
{
	Vector4 Dim = Vector4((float)pRenderTarget->GetWidth(), (float)pRenderTarget->GetHeight(), 0.0f, 0.0f);
	m_shaderSSAO->SetVar(m_variables[ssao_msaa_pointtexture1_dim], Dim);
	m_shaderSSAO->SetVar(m_variables[ssao_msaa_pointtexture1], pRenderTarget);
}

void SSAO::SetMSSAPointTexture2(grcRenderTarget *pRenderTarget)
{
	Vector4 Dim = Vector4((float)pRenderTarget->GetWidth(), (float)pRenderTarget->GetHeight(), 0.0f, 0.0f);
	m_shaderSSAO->SetVar(m_variables[ssao_msaa_pointtexture2_dim], Dim);
	m_shaderSSAO->SetVar(m_variables[ssao_msaa_pointtexture2], pRenderTarget);
}

#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS
