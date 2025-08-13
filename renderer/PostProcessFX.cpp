// PS3 Base : Composite AA : 6.75ms
// PS3 No AA: 2.58ms
// PS3 Second Pass FXAA : 2.64 + 2.75 = 5.39ms
// PS3 Second Pass FXAA 2 : 2.63 + 2.11 = 4.74ms

// 360 Base : Composite AA : 7.5ms
// 360 No AA: 3.02ms
// 360 Second Pass FXAA : 3.0 + 0.5 + 4.45 = 8.1 ms

//  
// renderer/PostProcessFX.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved. 
// 
// Post-processing effects
//
#include "renderer/PostProcessFX.h"
#include "renderer/PostProcessFXHelper.h"
#include "render_channel.h"

// System
#if __XENON
#include "system/xtl.h"
#define DBG 0
#include <xgraphics.h>
#endif // __XENON

// Rage headers
#include "atl/functor.h"
#include "atl/hashstring.h"
#include "profile/profiler.h"
#include "profile/timebars.h"
#include "grcore/allocscope.h"
#include "grcore/debugdraw.h"
#include "grcore/gfxcontext.h"
#include "grcore/im.h"
#include "grcore/image.h"
#include "grcore/image_dxt.h"
#include "grcore/quads.h"
#include "grcore/fastquad.h"
#include "grcore/stateblock.h"
#include "grcore/texturedefault.h"
#include "system/param.h" 
#include "system/memory.h"
#include "grmodel/setup.h"
#include "math/float16.h"

#if DISPLAY_NETWORK_INFO
#include "net/nethardware.h"
#endif

#if __XENON
#include "grcore/gprallocation.h"
#include "grcore/texturexenon.h"
#endif // __XENON

#if __PS3
#include "grcore/texture.h"
#include "grcore/texturegcm.h"
#endif

// Framework headers
#include "fwsys/timer.h"
#include "fwscene/stores/txdstore.h"
#include "vfx/vfxutil.h"
#include "vfx/clouds/CloudHat.h"
#include "vfx/VfxHelper.h"

// Game headers
#include "camera/base/BaseObject.h"
#include "camera/base/BaseCamera.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/aim/AimCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/system/CameraManager.h"
#include "control/videorecording/videorecording.h"
#include "core/game.h"
#include "cutscene/CutSceneManagerNew.h"
#include "cutscene/CutSceneCameraEntity.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugView.h"
#include "debug/Debug.h"
#include "Debug/BudgetDisplay.h"
#include "debug/BugstarIntegration.h"
#include "debug/FrameDump.h"
#include "debug/LightProbe.h"
#include "debug/SceneGeometryCapture.h"
#include "debug/TiledScreenCapture.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/NewHud.h"
#include "game/weather.h"
#include "network/NetworkInterface.h"
#include "network/Live/livemanager.h"
#include "peds/PlayerInfo.h"
#include "peds/ped.h"
#include "frontend/DisplayCalibration.h"
#include "frontend/MobilePhone.h"
#include "frontend/PauseMenu.h"
#include "renderer/LensArtefacts.h"
#include "renderer/Renderer.h"
#include "renderer/rendertargets.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Util/ShmooFile.h"
#include "renderer/lights/lights.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/sprite2d.h"
#include "renderer/Water.h"
#include "renderer/SSAO.h"
#include "renderer/ScreenshotManager.h"
#include "renderer/UI3DDrawManager.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "scene//playerswitch/PlayerSwitchInterface.h"
#include "scene/world/GameWorld.h"
#include "TimeCycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "camera/viewports/viewportManager.h"
#include "vfx/misc/LensFlare.h"
#include "vfx/particles/PtFxManager.h"
#include "vfx/systems/VfxLens.h"
#include "camera/helpers/Frame.h"

#if GTA_REPLAY
#include "camera/replay/ReplayDirector.h"
#include "control/replay/IReplayMarkerStorage.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "replaycoordinator/ReplayCoordinator.h"
#endif // #if GTA_REPLAY

#if __D3D11
#include "grcore/rendertarget_d3d11.h"
#include "grcore/texturefactory_d3d11.h"
#endif
#if RSG_ORBIS
#include "grcore/buffer_gnm.h"
#endif
#if RSG_ORBIS && SUPPORT_RENDERTARGET_DUMP
#include "grcore/texturefactory_gnm.h"
#endif

#include "AdaptiveDOF.h"

#if USE_SCREEN_WATERMARK
#include "Peds/ped.h"
#include "frontend/HudTools.h"
#include "frontend/Scaleform/ScaleformMgr.h"
#include "text/text.h"
#include "text/TextConversion.h"
#include "text/textFile.h"
#include "text/TextFormat.h"
#include "scaleform/scaleform.h"
#include "script/script_hud.h"
PARAM(no_screen_watermark,"Disable screen watermark");
#endif

#if USE_NV_TXAA
#define __GFSDK_OS_WINDOWS__
#define __GFSDK_DX11__
#include "../../3rdParty/NVidia/NVTXAA/include/GFSDK_TXAA.h"
#include "../../3rdParty/NVidia/nvapi.h"
extern TxaaCtxDX g_txaaContext;
gfsdk_U32 g_TXAADevMode = TXAA_MODE_TOTAL;

static grcTexture* g_TXAAResolve = NULL;
PARAM(txaaEnable,"Enable TXAA [0,1]");
PARAM(txaaDevMode,"0-11 see header file");
#endif

RENDER_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#define POSTFX_FOGRAY		(RSG_PC || RSG_DURANGO || (RSG_ORBIS && (SCE_ORBIS_SDK_VERSION >= 0x01600071u)))
#define POSTFX_TILED_LIGHTING_ENABLED ((TILED_LIGHTING_ENABLED)  && (__XENON || __PS3 || __D3D11 || RSG_ORBIS))
#define POSTFX_USE_TILEDFOG 0 && POSTFX_TILED_LIGHTING_ENABLED

#define DEBUG_MOTIONBLUR	(0 && __BANK)
#define DEBUG_HDR_BUFFER	(0)
#define USE_FXAA 1

#define FILM_EFFECT (RSG_PC)
PARAM(postfxusesFP16, "Force FP16 render targets for postfx");

PARAM(simplepfx,"Use simplified postfx shader");
#if !__FINAL && (__D3D11 || RSG_ORBIS)
PARAM(DX11Use8BitTargets,"Use 8 bit targets in DX11.");
PARAM(disableBokeh,"Disable Bokeh effects in DX11");
#endif // !__FINAL && (__D3D11 || RSG_ORBIS)
PARAM(dontDelayFlipSwitch,"Don't delay flip : wait flip will be done at the end of the frame, like normal people do.");
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
PARAM(filmeffect, "Enable the film effect shader");
#endif

PARAM(pcTiledPostFx, "Use tiling for PostFX shading");

#if USE_HQ_ANTIALIASING
PARAM(fxaaMode,"0: Off, 1: Default, 2: HQ FXAA");
#endif // USE_HQ_ANTIALIASING

NOSTRIP_PARAM(setGamma, "Allow to force a specific gamma setting on boot.");

EXT_PF_GROUP(RenderPhase);
PF_TIMER(PostFx,RenderPhase);

PF_PAGE(ExposureAndLumPage, "Exposure and Luminance");
PF_GROUP(ExposureAndLum);
PF_LINK(ExposureAndLumPage, ExposureAndLum);
PF_VALUE_FLOAT(UpdateExposure, ExposureAndLum);
PF_VALUE_FLOAT(Luminance, ExposureAndLum);
PF_VALUE_FLOAT(RenderExposure, ExposureAndLum);
PF_VALUE_INT(CalculationType, ExposureAndLum);

#if LIGHT_VOLUME_USE_LOW_RESOLUTION
extern grcRenderTarget* m_volumeOffscreenBuffer;
extern grcRenderTarget* m_volumeReconstructionBuffer;
#endif

namespace rage {
extern bool s_NeedWaitFlip;
}

extern bool gLastGenMode;

namespace PostFX {


PostFXParamBlock::paramBlock PostFXParamBlock::ms_params[2];

PedKillOverlay				g_pedKillOverlay;
ScreenBlurFade				g_screenBlurFade;
BulletImpactOverlay			g_bulletImpactOverlay;
Adaptation					g_adaptation;
DofTODOverrideHelper		g_cutsceneDofTODOverride;

grcRenderTarget*			g_prevExposureRT = NULL;
grcRenderTarget*			g_currentExposureRT = NULL;

int							g_resetAdaptedLumCount = NUM_RESET_FRAMES;
bool						g_enableLightRays=false;
bool						g_lightRaysAboveWaterLine=true; 
bool						g_enableNightVision=false;
#if __BANK
bool						g_overrideNightVision=false;
#endif
bool						g_enableSeeThrough=false;
bool						g_overrideDistanceBlur=false;
float						g_distanceBlurOverride=0.0f;

bool						g_scriptHighDOFOverrideToggle=false;
bool						g_scriptHighDOFOverrideEnableDOF=false;
Vector4						g_scriptHighDOFOverrideParams=Vector4(0.0f,0.0f,0.0f,0.0f);

#if __ASSERT
sysIpcCurrentThreadId		g_FogRayCascadeShadowMapDownsampleThreadId = sysIpcCurrentThreadIdInvalid;
#endif

#if __PS3
u16							g_RTPoolIdPostFX = kRTPoolIDInvalid;
#endif

#if __BANK
bool						g_Override = false;
bool						g_UseTiledTechniques = true;
#if __PS3
bool						g_PS3UseCompositeTiledTechniques = true;
#endif
bool						g_ForceUnderWaterTech = false;
bool						g_ForceAboveWaterTech = false;
bool						g_ForceUseSimple = false;
bool						g_AutoExposureSkip = false;
float						g_overwrittenExposure = -3.0f;				
bool						g_JustExposure = false;
bool						g_delayFlipWait = true;

bool						g_bDebugTriggerPulseEffect = false;
u32							g_debugPulseEffectRampUpDuration = 0;
u32							g_debugPulseEffectHoldDuration = 10;
u32							g_debugPulseEffectRampDownDuration = 350;
Vector4						g_debugPulseEffectParams = Vector4(-0.3f,0.0f,0.1f,0.0f);

bool						g_DrawCOCOverlay = false;
float						g_cocOverlayAlphaVal = 1.0f;
static grcEffectVar			g_cocOverlayAlpha;
#endif //__BANK

static BankBool				g_fadeSSLROffscreen = true;
static BankFloat			g_fadeSSLROffscreenValue = 1.3f;

bool						g_setExposureToTarget = false;
BANK_ONLY(static char		adaptionInfoStr[255]);

atArray<historyValues>		g_historyArray;
s8							g_historyIndex = 0;
s8							g_historySize = 0;

bool						g_enableExposureTweak = true;
bool						g_forceExposureReadback = false;

float						g_filmicTonemapParams[TONEMAP_VAR_COUNT];

Vector4						g_sniperSightDefaultDOFParams;
bool						g_sniperSightDefaultEnabled = true;

bool						g_sniperSightOverrideEnabled = false;
bool						g_sniperSightOverrideDisableDOF = true;
Vector4						g_sniperSightOverrideDOFParams = Vector4(0.0f, 0.0f, 0.0f, 2180.0f);

bool						g_DefaultMotionBlurEnabled = false;
float						g_DefaultMotionBlurStrength = 0.05f;

#if RSG_PC
static __THREAD PostFX::eResolvePause		g_DoPauseResolve = PostFX::NOT_RESOVE_PAUSE;
static bool g_RequestResetDOFRenderTargets					 = false;
#else
static PostFX::eResolvePause				g_DoPauseResolve = PostFX::NOT_RESOVE_PAUSE;
#endif

#if RSG_PC
// force postfx setting to High at most
// need to do this to avoid flickering caused by dof effects in multi-GPUs
#define GET_POSTFX_SETTING(s)	(PostFX::g_DoPauseResolve == PostFX::IN_RESOLVE_PAUSE ? Min(s,CSettings::Medium) : s)
#else
#define GET_POSTFX_SETTING(s)	(s)
#endif

#if DEBUG_MOTIONBLUR
bool						g_debugPrintMotionBlur = false;
#endif

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
bool						g_FXAAEnable	= true;
#else
bank_bool					g_FXAAEnable	= true;
#endif
#if USE_NV_TXAA
bool						g_TXAAEnable = false;
#endif // USE_NV_TXAA

MLAA_ONLY(MLAA g_MLAA;)

typedef enum
{
	AA_FXAA_DEFAULT = 0,
#if USE_HQ_ANTIALIASING
	AA_FXAA_HQ,
#endif // USE_HQ_ANTIALIASING
	AA_FXAA_UI
} AntiAliasingEnum;

AntiAliasingEnum g_AntiAliasingType	= AA_FXAA_DEFAULT;

#if USE_HQ_ANTIALIASING
float g_AntiAliasingSwitchDistance = 150.f;
static grcEffectTechnique g_HQAATiledTechnique;
static grcEffectVar g_HQAA_srcMapID;
#endif

#if FILM_EFFECT
bool						g_EnableFilmEffect;
float						g_BaseDistortionCoefficient;
float						g_BaseCubeDistortionCoefficient;
float						g_ChromaticAberrationOffset;
float						g_ChromaticAberrationCubeOffset;
float						g_FilmNoise;
float						g_FilmVignettingIntensity;
float						g_FilmVignettingRadius;
float						g_FilmVignettingContrast;
#endif

bool						g_UseSubSampledAlpha = (RSG_ORBIS || RSG_DURANGO);
bool						g_UseSinglePassSSA = (RSG_ORBIS || RSG_DURANGO || RSG_PC);
bool						g_UseSSAOnFoliage = false;
#if SSA_USES_CONDITIONALRENDER
static BankBool				g_UseConditionalSSA = true;
#endif
DECLARE_MTR_THREAD bool			g_MarkingSubSamples=false;

#if __XENON
static BankBool				g_UsePacked7e3IntBackBuffer = true;
#endif

float						g_gammaFrontEnd = 2.2f;

float						g_linearExposure;
float						g_averageLinearExposure;


float						g_sunExposureAdjust = 0.0f;

bool						g_noiseOverride = false;
float						g_noisinessOverride = 0.0f;

bool						g_useAutoColourCompression = true;

#if RSG_PC
bool						g_allowDOFInReplay = true;
#endif

grcTexture*					g_pNoiseTexture = NULL;
grcTexture*					g_pDamageOverlayTexture = NULL;

AnimatedPostFX				g_defaultFlashEffect;

Vector4 					g_vInitialPulseParams		= Vector4(0.0f, 0.0f, 0.0f, 0.0f);
Vector4 					g_vTargetPulseParams		= Vector4(0.0f, 0.0f, 0.0f, 0.0f);
u32   						g_uPulseStartTime			= 0;
u32   						g_uPulseRampUpDuration		= 0;
u32   						g_uPulseRampDownDuration	= 0;
u32   						g_uPulseHoldDuration		= 0;

float						g_fSniperScopeOverrideStrength	= 1.0f;

float						g_motionBlurMaxVelocityMult = 2.0f;
bool						g_motionBlurCutTestDisabled = false;
bool						g_bMotionBlurOverridePrevMat = false;
Matrix34					g_motionBlurPrevMatOverride;

float						g_bloomThresholdExposureDiffMin;
float						g_bloomThresholdExposureDiffMax;
float						g_bloomThresholdMin;
float						g_bloomThresholdPower;

static float				g_fograyJitterStart = 800.0f;
static float				g_fograyJitterEnd = 1100.0f;

float						g_bloomLargeBlurBlendMult = 0.5f;
float						g_bloomMedBlurBlendMult = 0.5f;
float						g_bloomSmallBlurBlendMult = 1.0f;

bool						g_bloomEnable = true;

float						g_cachedDefaultMotionBlur = 0.0f;

#if GTA_REPLAY
//When paused during replay we still want to update some effects when in the effects menu of the editor.
bool						g_UpdateReplayEffectsWhilePaused = false;
#endif

// For drawing damage overlay in slices
struct grcSliceVertex {
	float x, y, z;
	unsigned cpv;
	float u, v, q;
	void Set(float x_,float y_,float z_,unsigned cpv_,float u_,float v_,float q_) XENON_ONLY(volatile)
	{
		x = x_; y = y_; z = z_; cpv = cpv_; u = u_; v = v_; q = q_;
	}
};
// vert decl for slice vertices
grcVertexDeclaration*		g_damageSliceDecl;

grcRenderTarget*			LumDownsampleRT[5];
int							LumDownsampleRTCount;

#if AVG_LUMINANCE_COMPUTE
grcRenderTarget*			LumCSDownsampleUAV[6];
int							LumCSDownsampleUAVCount;
#endif

#if (RSG_PC) 
grcRenderTarget*			ImLumDownsampleRT[5];
int							ImLumDownsampleRTCount;				
#endif		

grcRenderTarget*			FogOfWarRT0;
grcRenderTarget*			FogOfWarRT1;
grcRenderTarget*			FogOfWarRT2;

grcRenderTarget*			FogRayRT;
grcRenderTarget*			FogRayRT2;

#if RSG_PC
// for nvstereo
grcRenderTarget*			CenterReticuleDist;
#endif
};

static atHashString			DefaultFlashEffectModName;

static grmShader*			PostFXShader;
static grcEffectTechnique	PostFXTechnique;
#if GENERATE_SHMOO
static int					PostFxShmoo;
#else 
#define						PostFxShmoo (-1)
#endif

#if MSAA_EDGE_PASS
static grmShader*			PostFXShaderMS0;
static grcDepthStencilStateHandle edgeMaskDepthEffectFace_DS;
#endif // MSAA_EDGE_PASS

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
static grmShader*			dofComputeShader;
static grcEffectTechnique	dofComputeTechnique;
#endif

#if DOF_DIFFUSION
static grmShader*			dofDiffusionShader;
static grcEffectTechnique	dofDiffusiontechnique;
#endif

#if BOKEH_SUPPORT
static grcEffectTechnique	PostFXBokehTechnique;
#endif
#if FILM_EFFECT
static grcEffectTechnique	PostFXFilmEffectTechnique;
#endif

#if BOKEH_SUPPORT
strLocalIndex g_BokehTxdSlot;
#endif
#if WATERMARKED_BUILD
strLocalIndex g_TxdSlot;
#endif

static int					CloudDepthTechGroup;

#if ADAPTIVE_DOF
PostFX::AdaptiveDOF			AdaptiveDepthOfField;
bool						WasAdaptiveDofProcessedOnPreviousUpdate = false;
#endif

#if __BANK
#define NUM_DEBUG_PARAMS	2
static Vector4				DebugParams[NUM_DEBUG_PARAMS];
static grcEffectVar			DebugParamsId[NUM_DEBUG_PARAMS];
#endif

//// Variables
// Miscs
static grcEffectVar			TexelSizeId;
static grcEffectVar			GBufferTexture0ParamId;

#if AVG_LUMINANCE_COMPUTE
static grcEffectVar			LumCSDownsampleSrcId;
static grcEffectVar			LumCSDownsampleInitSrcId;
static grcEffectVar			LumCSDownsampleDstId;	
static grcEffectVar			LumCSDownsampleTexSizeId;

static grmShader*			avgLuminanceShader;
static grcEffectTechnique	avgLuminanceTechnique;
#endif

// DOF
static grcEffectVar			DofProjId;
static grcEffectVar			DofShearId;
static grcEffectVar			DofParamsId;
static grcEffectVar			HiDofParamsId;
static grcEffectVar			HiDofSmallBlurId;
static grcEffectVar			HiDofMiscParamsId;
#if ADAPTIVE_DOF_OUTPUT_UAV
static grcEffectVar			PostFXAdaptiveDOFParamsBufferVar;
static grcEffectVar			DofComputeAdaptiveDOFParamsBufferVar;
#endif //ADAPTIVE_DOF_OUTPUT_UAV
#if BOKEH_SUPPORT
static grcEffectVar			BokehBrightnessParams;
static grcEffectVar			BokehParams1;
static grcEffectVar			BokehParams2;
static grcEffectVar			DOFTargetSize;
static grcEffectVar			RenderTargetSize;
static grcEffectVar			BokehEnableVar;
static grcEffectVar			BokehSpritePointBufferVar;
static grcEffectVar			BokehSortedIndexBufferVar;
static grcEffectVar			GaussianWeightsBufferVar;
static grcEffectVar			LuminanceDownsampleOOSrcDstSizeId;
static grcEffectVar			BokehAlphaCutoffVar;
static grcEffectVar			BokehGlobalAlphaVar;
static grcEffectVar			BokehSortLevelVar;
static grcEffectVar			BokehSortLevelMaskVar;
#if BOKEH_SORT_BITONIC_TRANSPOSE
static grcEffectVar			BokehSortTransposeMatWidthVar;
static grcEffectVar			BokehSortTransposeMatHeightVar;
#endif

static grcEffectVar			PostFXAdaptiveDofEnvBlurParamsVar;
static grcEffectVar			PostFXAdaptiveDofCustomPlanesParamsVar;

enum eBokeh_Shapes
{
	BOKEH_SHAPE_HEXAGON = 0,
	BOKEH_SHAPE_OCTAGON,
	BOKEH_SHAPE_CIRCLE,
	BOKEH_SHAPE_CROSS,
	BOKEH_SHAPE_MAX
};
grcTextureHandle m_BokehShapeTextureSheet;
#endif

#if DOF_TYPE_CHANGEABLE_IN_RAG
static grcEffectVar			CurrentDOFTechniqueVar;
#endif

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
static grcEffectVar			DofKernelSize;
#if COC_SPREAD
static grcEffectVar			DofCocSpreadKernelRadius;
#endif
static grcEffectVar			DofProjCompute;
static grcEffectVar			DofSkyWeightModifierVar;
static grcEffectVar			DofLumFilterParams;
static grcEffectVar			DofRenderTargetSizeVar;
static grcEffectVar			fourPlaneDofVar;
#endif //DOF_COMPUTE_GAUSSIAN
#if DOF_DIFFUSION
static grcEffectVar			dofDiffusion_lastbufferSizeVar;
static grcEffectVar			dofDiffusion_DiffusionRadiusVar;
static grcEffectVar			dofDiffusion_txABC_ID;	//0
static grcEffectVar			dofDiffusion_txCOC_ID;	//1
static grcEffectVar			dofDiffusion_txX_ID;	//2
static grcEffectVar			dofDiffusion_txY_ID;	//3
#endif //DOF_DIFFUSION

// HDR
static grcEffectVar			BloomParamsId;
static grcEffectVar			FilmicId[2];

static grcEffectVar			tonemapParams;
static grcEffectVar			brightTonemapParams[2];
static grcEffectVar			darkTonemapParams[2];

// Noise
static grcEffectVar			NoiseParamsId;

// Motion Blur
static grcEffectVar			DirectionalMotionBlurParamsId;
static grcEffectVar			DirectionalMotionBlurIterParamsId;

static grcEffectVar			MBPrevViewProjMatrixXId;
static grcEffectVar			MBPrevViewProjMatrixYId;
static grcEffectVar			MBPrevViewProjMatrixWId;

// Night Vision
static grcEffectVar			lowLumId;
static grcEffectVar			highLumId;
static grcEffectVar			topLumId;

static grcEffectVar			scalerLumId;

static grcEffectVar			offsetLumId;
static grcEffectVar			offsetLowLumId;
static grcEffectVar			offsetHighLumId;

static grcEffectVar			noiseLumId;
static grcEffectVar			noiseLowLumId;
static grcEffectVar			noiseHighLumId;

static grcEffectVar			bloomLumId;

static grcEffectVar			colorLumId;
static grcEffectVar			colorLowLumId;
static grcEffectVar			colorHighLumId;

#if RSG_PC
static grcEffectVar			globalFreeAimDirId;
#endif

// Light rays
static grcEffectVar			globalFograyParamId;
static grcEffectVar			globalFograyFadeParamId;
static grcEffectVar			lightrayParamsId;
static grcEffectVar			lightrayParams2Id;

static grcEffectVar			sslrParamsId;	 // additive reducer, blit size, ray Length, enable intensity map for non SSLR light rays
static grcEffectVar			sslrCenterId;
static grcEffectVar			SSLRTextureId;
static grcEffectVar			GLRTextureId;

// HeatHaze
static grcEffectVar			heatHazeParamsId;
static grcEffectVar			HeatHazeTex1ParamsId;
static grcEffectVar			HeatHazeTex2ParamsId;
static grcEffectVar			HeatHazeOffsetParamsId;

// SeeThrough
static grcEffectVar			seeThroughParamsId;
static grcEffectVar			seeThroughColorNearId;
static grcEffectVar			seeThroughColorFarId;
static grcEffectVar			seeThroughColorVisibleBaseId;
static grcEffectVar			seeThroughColorVisibleWarmId;
static grcEffectVar			seeThroughColorVisibleHotId;

// AA
static grcEffectVar			rcpFrameId;

// Vignetting (test)
static grcEffectVar			VignettingParamsId;
static grcEffectVar			VignettingColorId;

// Damage Overlay Params
static grcEffectVar			DamageOverlayMiscId;
static grcEffectVar			DamageOverlayForwardId;
static grcEffectVar			DamageOverlayTangentId;
static grcEffectVar			DamageOverlayCenterPosId;

// Lens Gradient 
static grcEffectVar			GradientFilterColTopId;
static grcEffectVar			GradientFilterColMiddleId;
static grcEffectVar			GradientFilterColBottomId;

// Scanline
static grcEffectVar			ScanlineFilterParamsId;

// Colour correction
static grcEffectVar			ColorCorrectId;
static grcEffectVar			ColorShiftId;
static grcEffectVar			DesaturateId;
static grcEffectVar			GammaId;

// Exposure calculation
static grcEffectVar			ExposureParams0Id;
static grcEffectVar			ExposureParams1Id;
static grcEffectVar			ExposureParams2Id;
static grcEffectVar			ExposureParams3Id;

#if FILM_EFFECT
//lens effects
static grcEffectVar			LensDistortionId;
#endif
//// Samplers

// Miscs
static grcEffectVar			GBufferTextureId0;

#if USE_FXAA
static grcEffectVar			FXAABackBuffer;
#endif // USE_FXAA

#if PTFX_APPLY_DOF_TO_PARTICLES
static grcEffectVar			PtfxDepthBuffer;
static grcEffectVar			PtfxAlphaBuffer;
#endif // PTFX_APPLY_DOF_TO_PARTICLES

#if __XENON
static grcEffectVar			GBufferTextureId2;
#endif // __XENON
static grcEffectVar			GBufferTextureIdDepth;
static grcEffectVar			ResolvedTextureIdDepth;
static grcEffectVar			GBufferTextureIdSSAODepth;

#if DEVICE_MSAA
static grcEffectVar			TextureHDR_AA;
#endif	//DEVICE_MSAA

static grcEffectVar			TextureID_0;

#if __PPU
static grcEffectVar			TextureID_0a;
#endif // __PPU
static grcEffectVar			TextureID_1;

static grcEffectVar			TextureID_v0;
static grcEffectVar			TextureID_v1;

#if __XENON	|| __D3D11 || RSG_ORBIS
static grcEffectVar			StencilCopyTextureId;
static grcEffectVar			TiledDepthTextureId;
#endif

#if !__PS3
static grcEffectVar			BloomTexelSizeId;
#endif

static grcEffectVar			PostFxTexture2Id;
static grcEffectVar			PostFxTexture3Id;


// MotionBlur
static grcEffectVar			PostFxMotionBlurTextureID;

// HDR
static grcEffectVar			BloomTextureID;
static grcEffectVar			BloomTextureGID;

// Motion Blur
static grcEffectVar			JitterTextureId;
static grcEffectVar			MBPerspectiveShearParams0Id;
static grcEffectVar			MBPerspectiveShearParams1Id;
static grcEffectVar			MBPerspectiveShearParams2Id;

// Heat Haze
static grcEffectVar			HeatHazeTextureId;
static grcEffectVar			PostFxHHTextureID;
static grcEffectVar			HeatHazeMaskTextureID;

// Screen Blur Fade
static grcEffectVar			ScreenBlurFadeID;

// Lens Distortion
static grcEffectVar			DistortionParamsID;

// Lens Artefacts
static grcEffectVar			LensArtefactParams0ID;
static grcEffectVar			LensArtefactParams1ID;
static grcEffectVar			LensArtefactParams2ID;
static grcEffectVar			LensArtefactTextureID;

// Light Streaks
static grcEffectVar			LightStreaksColorShift0ID;
static grcEffectVar			LightStreaksBlurColWeightsID;
static grcEffectVar			LightStreaksBlurDirID;

// Blur Vignette	
static grcEffectVar			BlurVignettingParamsID;

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
static grcEffectVar			dofComputeColorTex;
static grcEffectVar			dofComputeDepthBlurTex;
static grcEffectVar			dofComputeOutputTex;
static grcEffectVar			dofComputeDepthBlurHalfResTex;
static grcEffectVar			dofBlendParams;
#if COC_SPREAD
static grcEffectVar			cocComputeOutputTex;
#endif
#endif //DOF_COMPUTE_GAUSSIAN

#if POSTFX_UNIT_QUAD
namespace quad
{
	static grcEffectVar			Position;
	static grcEffectVar			TexCoords;
	static grcEffectVar			Scale;
#if USE_IMAGE_WATERMARKS
	static grcEffectVar			Alpha;
#endif
}
namespace exposure_quad
{
	static grcEffectVar			Position;
}
#endif

// Render Targets
static grcRenderTarget*		HalfScreen0;
static grcRenderTarget*		SSLRCutout;
static grcRenderTarget*		LRIntensity;
#if BOKEH_SUPPORT
#if RSG_DURANGO && __BANK
static grcRenderTarget*		AltBokehDepthBlur;
#endif
static grcRenderTarget*		BokehDepthBlur;
static grcRenderTarget*		BokehDepthBlurHalfRes;
#if COC_SPREAD
static grcRenderTarget*		BokehDepthBlurHalfResTemp;
#endif
static grcRenderTarget*		BokehPoints;
static grcRenderTarget*		BokehPointsTemp;
static grcTexture*			BokehNoneRenderedTexture;
#endif //BOKEH_SUPPORT
#if DOF_DIFFUSION
//Should be able to reduce the number needed by added in packing code
grcRenderTarget*			dofDiffusionHorzABC[16];
grcRenderTarget*			dofDiffusionHorzX[16];
grcRenderTarget*			dofDiffusionHorzY[16];
grcRenderTarget*			dofDiffusionVertABC[16];
grcRenderTarget*			dofDiffusionVertX[16];
grcRenderTarget*			dofDiffusionVertY[16];
#endif
#if FILM_EFFECT
static grcRenderTarget*		PreFilmEffect; 
#endif
#if !__PS3
static grcRenderTarget*		LRIntensity2;
#endif

#if RSG_PC 
	#define MAX_EXP_TARGETS	(3 + MAX_GPUS)
	#define NUM_EXP_TARGETS (3 + GRCDEVICE.GetGPUCount())
#else
	#define MAX_EXP_TARGETS	(3)
	#define NUM_EXP_TARGETS MAX_EXP_TARGETS
#endif // RSG_PC

#if __PPU
	static ALIGNAS(16) float* CurExposureRTBasePtr[MAX_EXP_TARGETS] ;
#endif

#if __XENON
	static ALIGNAS(16) Float16Vec4* CurExposureRTBasePtr[MAX_EXP_TARGETS] ;
#endif // __PPU	|| __XENON

static grcRenderTarget*		LumRT = NULL;

#if __D3D11
static grcTexture*			CurExposureTex[MAX_EXP_TARGETS] = {NULL};
#endif
static grcRenderTarget*		CurExposureRT[MAX_EXP_TARGETS];
static u32					CurExportRTIndex = 0;
static bool					ExposureInitialised = false;
static grcTexture*			JitterTexture;

static grcTexture*			HeatHazeTexture;

static grcRenderTarget*		DepthOfField0;
static grcRenderTarget*		DepthOfField1;
static grcRenderTarget*		DepthOfField2;

static grcRenderTarget*		HeatHaze0;
static grcRenderTarget*		HeatHaze1;

static grcRenderTarget*		BloomBuffer;
static grcRenderTarget*		BloomBufferUnfiltered;
static grcRenderTarget*		BloomBufferHalf0;
static grcRenderTarget*		BloomBufferHalf1;
static grcRenderTarget*		BloomBufferQuarter0;
static grcRenderTarget*		BloomBufferQuarter1;
static grcRenderTarget*		BloomBufferEighth0;
static grcRenderTarget*		BloomBufferEighth1;
static grcRenderTarget*		BloomBufferSixteenth0;
static grcRenderTarget*		BloomBufferSixteenth1;
#if RSG_DURANGO
static grcRenderTarget*		BloomBufferQuarterX;

static grcRenderTarget* DSBB2;
static grcRenderTarget* DSBB4;
static grcRenderTarget* DSBB8;
static grcRenderTarget* DSBB16;
#endif

static grcRenderTarget*		LensArtefactsFinalBuffer;
static grcRenderTarget*		LensArtefactSrcBuffers[LENSARTEFACT_BUFFER_COUNT];
static grcRenderTarget*		LensArtefactsBuffers0[LENSARTEFACT_BUFFER_COUNT];
static grcRenderTarget*		LensArtefactsBuffers1[LENSARTEFACT_BUFFER_COUNT];

static grcVertexDeclaration*	sm_FogVertexDeclaration;
static grcVertexBuffer*			sm_FogVertexBuffer;

#if BOKEH_SUPPORT
static u32						s_BokehAllocatedSize = 0;
static u32						s_BokehMaxElements = 0;
#if RSG_PC
static const u32				s_BokehBufferStride	= 36;
#else
static const u32				s_BokehBufferStride	= 32;
#endif
static const u32				s_BokehIndexListBufferStride	= 8;
static const u32				s_BokehBucketsNumAddedBufferStride	= 4;

static grcEffectVar				BokehSortOffsetsBufferVar;
static grcEffectVar				BokehSortedBufferVar;

# if RSG_ORBIS
static grcEffectVar				BokehOutputPointBufferVar;
static grcBufferUAV				s_BokehAccumBuffer( grcBuffer_Structured, true );
static grcBufferBasic			s_indirectArgsBuffer( grcBuffer_Raw, true );
static grcBufferUAV				s_BokehIndexListBuffer( grcBuffer_Structured, true );
static grcBufferUAV				s_bokehNumAddedToBuckets( grcBuffer_Structured, true );
static grcBufferUAV				s_bokehSortedIndexListBuffer( grcBuffer_Structured, true );
#if BOKEH_SORT_BITONIC_TRANSPOSE
static grcBufferUAV				s_bokehSortedIndexListBuffer2( grcBuffer_Structured, true );
#endif //BOKEH_SORT_BITONIC_TRANSPOSE
# else	//RSG_ORBIS

#  if RSG_DURANGO
static grcEffectVar				BokehOutputPointBufferVar;
#  endif
static grcBufferUAV				s_BokehAccumBuffer;
static grcBufferBasic			s_indirectArgsBuffer;
static grcBufferUAV				s_BokehIndexListBuffer;
static grcBufferUAV				s_bokehNumAddedToBuckets;
static grcBufferUAV				s_bokehSortedIndexListBuffer;
#if BOKEH_SORT_BITONIC_TRANSPOSE
static grcBufferUAV				s_bokehSortedIndexListBuffer2;
#endif //BOKEH_SORT_BITONIC_TRANSPOSE
static u32						s_curAddedBucket = 0;
# endif	//RSG_ORBIS

#if __BANK
sysTimer s_BokehRenderCountTimer;
#endif
#endif	//BOKEH_SUPPORT

#if RSG_ORBIS
static grcBufferUAV				s_GaussianWeightsBuffer( grcBuffer_Structured, true );
static grcBufferUAV				s_GaussianWeightsCOCBuffer( grcBuffer_Structured, true );
#else
static grcBufferUAV				s_GaussianWeightsBuffer;
static grcBufferUAV				s_GaussianWeightsCOCBuffer;
#endif


#if FXAA_CUSTOM_TUNING
static grcEffectVar				fxaaBlurinessId;
static grcEffectVar				fxaaConsoleEdgeSharpnessId;
static grcEffectVar				fxaaConsoleEdgeThresholdId;
static grcEffectVar				fxaaConsoleEdgeThresholdMinId;
static grcEffectVar				fxaaQualitySubpixId;
static grcEffectVar				fxaaEdgeThresholdId;
static grcEffectVar				fxaaEdgeThresholdMinId;

static float					fxaaBluriness;
static float					fxaaConsoleEdgeSharpness;
static float					fxaaConsoleEdgeThreshold;
static float					fxaaConsoleEdgeThresholdMin;
static float					fxaaQualitySubpix;
static float					fxaaEdgeThreshold;
static float					fxaaEdgeThresholdMin;
#endif

bool							g_fpvMotionBlurEnable = false;
bool							g_fpvMotionBlurEnableInVehicle = false;
bool							g_fpvMotionBlurEnableDynamicUpdate = false;
bool							g_fpvMotionBlurDrawDebug = false;
bool							g_fpvMotionBlurOverrideTagData = false;
grcRenderTarget*				fpvMotionBlurTarget[2];


static Vector4					fpvMotionBlurWeights;
static float					fpvMotionBlurSize;
static float					fpvMotionVelocityMaxSize;

static Vec2V					fpvMotionBlurVelocity;
static Vec3V					fpvMotionBlurPrevCameraPos[2];
static Mat44V					fpvMotionBlurPrevModelViewMtx;

static bool						fpvMotionBlurFirstFrame;

static Vec3V					fpvMotionBlurCurrCameraPos;

static grcEffectVar				fpvMotionBlurWeightsId;
static grcEffectVar				fpvMotionBlurVelocityId;
static grcEffectVar				fpvMotionBlurSizeId;


#if BOKEH_SUPPORT || DOF_COMPUTE_GAUSSIAN
void InitBokehDOF();
#endif // BOKEH_SUPPORT || DOF_COMPUTE_GAUSSIAN


#if USE_SCREEN_WATERMARK
static grcBlendStateHandle		ms_watermarkBlendState = grcStateBlock::BS_Invalid;
static PostFX::WatermarkParams	ms_watermarkParams;

void PostFX::WatermarkParams::Init()
{
	alphaDay = 0.25f;
	alphaNight = 0.055f;
	textColor = Color32(255, 255, 255, 64); 
	textSize = 4.0f; 
	textPos = Vector2(0.135f, 3.894f);
#if	DISPLAY_NETWORK_INFO
	netTextColor = Color32(255, 255, 255, 64); 
	netTextSize = 0.9f;
	netTextPos = Vector2(0.398f, 0.51f);
#endif
	useOutline = true;

	if (CHudTools::GetWideScreen() == false)
	{
		textPos.x = 0.187f;
	}

	text[0] = 0;
#if __BANK
	bForceWatermark = false; 
	bUseDebugText = false;
	debugText[0] = 0;
#endif

}
#endif // USE_PHOTO_WATERMARK

static bool g_bRunBlurVignettingTiled = (0 && (RSG_ORBIS || RSG_DURANGO));

//	If you change this, don't forget to change the following string array.
//
enum						PostFXPass
{
			pp_lum_4x3_conversion,
			pp_lum_4x3,
			pp_lum_4x5,
			pp_lum_5x5,
			pp_lum_2x2,
			pp_lum_3x3,
			pp_min,
			pp_max,
			pp_maxx,
			pp_maxy,
			pp_composite,
			pp_composite_mb,
			pp_composite_highdof,
			pp_composite_mb_highdof,
			pp_composite_noise,
			pp_composite_mb_noise,
			pp_composite_highdof_noise,
			pp_composite_mb_highdof_noise,
			pp_composite_nv,
			pp_composite_mb_nv,
			pp_composite_highdof_nv,
			pp_composite_mb_highdof_nv,
			pp_composite_hh,
			pp_composite_mb_hh,
			pp_composite_highdof_hh,
			pp_composite_mb_highdof_hh,
			pp_composite_noise_hh,
			pp_composite_mb_noise_hh,
			pp_composite_highdof_noise_hh,
			pp_composite_mb_highdof_noise_hh,
			pp_composite_nv_hh,
			pp_composite_mb_nv_hh,
			pp_composite_highdof_nv_hh,
			pp_composite_mb_highdof_nv_hh,
			pp_composite_shallowhighdof,
			pp_composite_mb_shallowhighdof,
			pp_composite_noise_shallowhighdof,
			pp_composite_noise_mb_shallowhighdof,
			pp_composite_shallowhighdof_nv,
			pp_composite_mb_shallowhighdof_nv,
			pp_composite_shallowhighdof_hh,
			pp_composite_mb_shallowhighdof_hh,
			pp_composite_noise_shallowhighdof_hh,
			pp_composite_noise_mb_shallowhighdof_hh,
			pp_composite_shallowhighdof_nv_hh,
			pp_composite_mb_shallowhighdof_nv_hh,
			pp_composite_ee,												
			pp_composite_mb_ee,						
			pp_composite_highdof_ee,				
			pp_composite_mb_highdof_ee,				
			pp_composite_hh_ee,						
			pp_composite_mb_hh_ee,					
			pp_composite_highdof_hh_ee,				
			pp_composite_mb_highdof_hh_ee,			
			pp_composite_shallowhighdof_ee,			
			pp_composite_mb_shallowhighdof_ee,		
			pp_composite_shallowhighdof_hh_ee,		
			pp_composite_mb_shallowhighdof_hh_ee,	
			pp_composite_seethrough,
			pp_composite_tiled,
			pp_composite_tiled_noblur,
			pp_depthfx,
			pp_depthfx_tiled,
#if RSG_PC
			pp_depthfx_nogather,
			pp_depthfx_tiled_nogather,
#endif
			pp_simple,
			pp_passthrough,
			pp_lightrays1,
			pp_sslrcutout,
			pp_sslrcutout_tiled,
			pp_sslrextruderays,
			pp_fogray,
			pp_fogray_high,
#if RSG_PC
			pp_fogray_nogather,
			pp_fogray_high_nogather,
#endif
			pp_shadowmapblit,
			pp_calcbloom0,
			pp_calcbloom1,
			pp_calcbloom_seethrough,
			pp_sslrblur,
			pp_subsampleAlpha,
			pp_subsampleAlphaSinglePass,
			pp_DownSampleBloom,
			pp_DownSampleStencil,
			pp_DownSampleCoc,
			pp_Blur,
			pp_DOFCoC,
			pp_HeatHaze,
			pp_HeatHaze_Tiled,
			pp_HeatHazeDilateBinary,
			pp_HeatHazeDilate,
			pp_HeatHazeWater,
			pp_GaussBlur_Hor,
			pp_GaussBlur_Ver,
			pp_BloomComposite,
			pp_BloomComposite_SeeThrough,
			pp_gradientfilter,
			pp_scanlinefilter,
			pp_AA,
#if RSG_PC
			pp_AA_sm50,
#endif
#if __PS3
			pp_AA_720p,
#endif
			pp_UIAA,
			pp_JustExposure,
			pp_CopyDepth,
			pp_DamageOverlay,
			pp_exposure,
			pp_exposure_reset,
			pp_copy,
#if !__FINAL
			pp_exposure_set,
#endif
			pp_composite_highdof_blur_tiled,
			pp_composite_highdof_noblur_tiled,
			pp_subsampleAlphaUI,
			pp_lens_distortion,
			pp_lens_artefacts,
			pp_lens_artefacts_combined,
			pp_light_streaks_blur_low,
			pp_light_streaks_blur_med,
			pp_light_streaks_blur_high,
			pp_blur_vignetting,
			pp_blur_vignetting_tiled,
			pp_blur_vignetting_blur_hor_tiled,
			pp_blur_vignetting_blur_ver_tiled,
			pp_bloom_min,
			pp_motion_blur_fpv,
			pp_motion_blur_fpv_ds,
			pp_motion_blur_fpv_composite,
			pp_GaussBlurBilateral_Hor,
			pp_GaussBlurBilateral_Ver,
			pp_GaussBlurBilateral_Hor_High,
			pp_GaussBlurBilateral_Ver_High,
#if RSG_PC
			pp_centerdist,
#endif
			pp_count
};

#if AVG_LUMINANCE_COMPUTE
enum PostFXAvgLuminancePass
{
	pp_luminance_downsample_init,
	pp_luminance_downsample,
	count
};
#endif

#if BOKEH_SUPPORT
enum PostFXBokehPass
{
	pp_Bokeh_DepthBlur,		//generates target containing linear depth and blurred backbuffer
#if ADAPTIVE_DOF_GPU
	pp_Bokeh_DepthBlurAdaptive, //generates COC differently when using adaptive dof
#endif
	pp_Bokeh_DepthBlurDownsample,
	pp_Bokeh_Generation,	//determines where bokeh should be applied
	pp_Bokeh_Sprites,		//Draw bokeh sprites with geometry shader
#if ADAPTIVE_DOF_OUTPUT_UAV
	pp_Bokeh_Sprites_Adaptive,
#endif
	pp_Bokeh_Downsample,	//Downsample backbuffer for use with Bokeh
	pp_Bokeh_count
};
#endif // BOKEH_SUPPORT

enum PostFXCompute
{
	pp_Bokeh_ComputeBitonicSort = 0
#if BOKEH_SORT_BITONIC_TRANSPOSE
	, pp_Bokeh_ComputeBitonicSortTranspose
#endif //BOKEH_SORT_BITONIC_TRANSPOSE
};

#if DOF_DIFFUSION
enum PostFXDofDiffusionPass
{
	//Vertical Phase
	pp_DOF_Diffusion_InitialReduceVert4,
	pp_DOF_Diffusion_ReduceVert,
	pp_DOF_Diffusion_ReduceFinalVert2,
	pp_DOF_Diffusion_ReduceFinalVert3,
	pp_DOF_Diffusion_SolveVert,
	pp_DOF_FinalSolveVert4,
	//Horizontal Phase
	pp_DOF_Diffusion_InitialReduceHorz4,
	pp_DOF_Diffusion_ReduceHorz,
	pp_DOF_Diffusion_ReduceFinalHorz2,
	pp_DOF_Diffusion_ReduceFinalHorz3,
	pp_DOF_Diffusion_SolveHorz,
	pp_DOF_FinalSolveHorz4,

	pp_DOF_Diffusion_count
};
#endif

#if DOF_COMPUTE_GAUSSIAN
enum PostFXDofComputeGaussianPass
{
#if COC_SPREAD
	pp_DOF_ComputeGaussian_COCSpread_H,
	pp_DOF_ComputeGaussian_COCSpread_V,
#endif
	pp_DOF_ComputeGaussian_Blur_H,
	pp_DOF_ComputeGaussian_Blur_V,
#if ADAPTIVE_DOF_OUTPUT_UAV
	pp_DOF_ComputeGaussian_Blur_H_Adaptive,
	pp_DOF_ComputeGaussian_Blur_V_Adaptive,
	pp_DOF_ComputeGaussian_Blur_H_Adaptive_LumFilter,
#endif //ADAPTIVE_DOF_OUTPUT_UAV
	pp_DOF_ComputeGaussian_Blur_H_LumFilter,
	pp_DOF_ComputeGaussian_MSAA_Blend,
	pp_DOF_ComputeGaussian_MSAA_BlendMS0,
#if ADAPTIVE_DOF_OUTPUT_UAV
	pp_DOF_ComputeGaussian_MSAA_Blend_Adaptive,
	pp_DOF_ComputeGaussian_MSAA_Blend_AdaptiveMS0,
#endif //ADAPTIVE_DOF_OUTPUT_UAV
	pp_DOF_ComputeGaussian_COC_Overlay, //!__SHADER_FINAL
#if ADAPTIVE_DOF_OUTPUT_UAV
	pp_DOF_ComputeGaussian_COC_Overlay_Adaptive //!__SHADER_FINAL
#endif //ADAPTIVE_DOF_OUTPUT_UAV
};
#endif

#if FILM_EFFECT
enum PostFXFilmEffectPass
{
	pp_filmeffect,
	pp_filmeffectcount,
};
#endif

#if RAGE_TIMEBARS || ENABLE_PIX_TAGGING
const char * passName[] = {	
			"pp_lum_4x3_conversion",
			"pp_lum_4x3",
			"pp_lum_4x5",
			"pp_lum_5x5",
			"pp_lum_2x2",
			"pp_lum_3x3",
			"pp_min",
			"pp_max",
			"pp_maxx",
			"pp_maxy",
			"pp_composite",
			"pp_composite_mb",
			"pp_composite_highdof",
			"pp_composite_mb_highdof",
			"pp_composite_noise",
			"pp_composite_mb_noise",
			"pp_composite_highdof_noise",
			"pp_composite_mb_highdof_noise",
			"pp_composite_nv",
			"pp_composite_mb_nv",
			"pp_composite_highdof_nv",
			"pp_composite_mb_highdof_nv",
			"pp_composite_hh",
			"pp_composite_mb_hh",
			"pp_composite_highdof_hh",
			"pp_composite_mb_highdof_hh",
			"pp_composite_noise_hh",
			"pp_composite_mb_noise_hh",
			"pp_composite_highdof_noise_hh",
			"pp_composite_mb_highdof_noise_hh",
			"pp_composite_nv_hh",
			"pp_composite_mb_nv_hh",
			"pp_composite_highdof_nv_hh",
			"pp_composite_mb_highdof_nv_hh",
			"pp_composite_shallowhighdof",
			"pp_composite_mb_shallowhighdof",
			"pp_composite_noise_shallowhighdof",
			"pp_composite_noise_mb_shallowhighdof",
			"pp_composite_shallowhighdof_nv",
			"pp_composite_mb_shallowhighdof_nv",
			"pp_composite_shallowhighdof_hh",
			"pp_composite_mb_shallowhighdof_hh",
			"pp_composite_noise_shallowhighdof_hh",
			"pp_composite_noise_mb_shallowhighdof_hh",
			"pp_composite_shallowhighdof_nv_hh",
			"pp_composite_mb_shallowhighdof_nv_hh",
			"pp_composite_ee",						
			"pp_composite_mb_ee",						
			"pp_composite_highdof_ee",				
			"pp_composite_mb_highdof_ee",				
			"pp_composite_hh_ee",						
			"pp_composite_mb_hh_ee",					
			"pp_composite_highdof_hh_ee",				
			"pp_composite_mb_highdof_hh_ee",			
			"pp_composite_shallowhighdof_ee",			
			"pp_composite_mb_shallowhighdof_ee",		
			"pp_composite_shallowhighdof_hh_ee",		
			"pp_composite_mb_shallowhighdof_hh_ee",	
			"pp_composite_seethrough",
			"pp_composite_tiled",
			"pp_composite_tiled_noblur",
			"pp_depthfx",
			"pp_depthfx_tiled",
#if RSG_PC
			"pp_depthfx_nogather",
			"pp_depthfx_tiled",
#endif
			"pp_simple",
			"pp_passthrough",
			"pp_lightrays1",
			"pp_sslrcutout",
			"pp_sslrcutout_tiled",
			"pp_sslrextruderays",
			"pp_fogray",
			"pp_fogray_high",
#if RSG_PC
			"pp_fogray_nogather",
			"pp_fogray_high_nogather",
#endif
			"pp_shadowmapblit",
			"pp_calcbloom0",
			"pp_calcbloom1",
			"pp_calcbloom_seethrough",
			"pp_sslrblur",
			"pp_subsampleAlpha",
			"pp_subsampleAlphaSinglePass",
			"pp_DownSampleBloom",
			"pp_DownSampleStencil",
			"pp_DownSampleCoc",
			"pp_Blur",
			"pp_DOFCoC",
			"pp_HeatHaze",
			"pp_HeatHaze_Tiled",
			"pp_HeatHazeDilateBinary",
			"pp_HeatHazeDilate",
			"pp_HeatHazeWater",
			"pp_GaussBlur_Hor",
			"pp_GaussBlur_Ver",
			"pp_BloomComposite",
			"pp_BloomComposite_SeeThrough",
			"pp_gradientfilter",
			"pp_scanlinefilter",
			"pp_AA",
#if RSG_PC
			"pp_AA_sm50",
#endif
#if __PS3
			"pp_AA_720p",
#endif 
			"pp_UIAA",
			"pp_JustExposure",
			"pp_CopyDepth",
			"pp_DamageOverlay",
			"pp_exposure",
			"pp_exposure_reset",
			"pp_copy",
#if !__FINAL
			"pp_exposure_set",
#endif
			"pp_composite_highdof_blur_tiled",
			"pp_composite_highdof_noblur_tiled",
			"pp_subsampleAlphaUI",
			"pp_lens_distortion",
			"pp_lens_artefacts",
			"pp_lens_artefacts_combined",
			"pp_light_streaks_blur_low",
			"pp_light_streaks_blur_med",
			"pp_light_streaks_blur_high",
			"pp_blur_vignetting",
			"pp_blur_vignetting_tiled",
			"pp_blur_vignetting_blur_hor_tiled",
			"pp_blur_vignetting_blur_ver_tiled",
			"pp_bloom_min",
			"pp_motion_blur_fpv",
			"pp_motion_blur_fpv_ds",
			"pp_motion_blur_fpv_composite",
			"pp_GaussBlurBilateral_Hor",
			"pp_GaussBlurBilateral_Ver",
			"pp_GaussBlurBilateral_Hor_High",
			"pp_GaussBlurBilateral_Ver_High"
#if RSG_PC
			,"pp_centerdist"
#endif
};

CompileTimeAssert(NELEM(passName) == pp_count);

#if BOKEH_SUPPORT
const char * bokehPassName[pp_Bokeh_count] = {	
	"pp_BokehDepthBlur",
#if ADAPTIVE_DOF_GPU
	"pp_BokehDepthBlurAdaptive",
#endif
	"pp_BokehDepthBlurDownsample",
	"pp_BokehGeneration",
	"pp_BokehSprites",
#if ADAPTIVE_DOF_OUTPUT_UAV
	"pp_BokehSprites_Adaptive",
#endif //ADAPTIVE_DOF_OUTPUT_UAV
	"pp_Bokeh_Downsample"
};

CompileTimeAssert(NELEM(bokehPassName) == pp_Bokeh_count);
#endif // BOKEH_SUPPORT

#if FILM_EFFECT
const char * filmEffectPassName[pp_filmeffectcount] = {	
	"pp_filmeffect",
};
#endif

#endif // RAGE_TIMEBARS

enum dofTechniques
{
	dof_console,
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
	dof_computeGaussian,
	dof_diffusion,
#endif
	dof_count
};
dofTechniques CurrentDOFTechnique;

#if DOF_TYPE_CHANGEABLE_IN_RAG
bool ProcessDOFChangeOnRenderThread = false;

const char * dofTechniqueNames[dof_count] = {	
	"Console"
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION	
	,
	"Gaussian Compute Shader",
	"Diffusion"
#endif
};

CompileTimeAssert(NELEM(dofTechniqueNames) == dof_count);
#endif // DOF_TYPE_CHANGEABLE_IN_RAG

#define BM_MotionBlur		(0x00001)
#define BM_HighDof			(0x00002)
#define BM_NightVision		(0x00004)
#define BM_Noise			(0x00008)
#define BM_HeatHaze			(0x00010)
#define BM_HighDofShallow	(0x00020)
#define BM_ExtraEffects		(0x00040)

#define UseAll		(BM_ExtraEffects | BM_HighDofShallow | BM_HeatHaze | BM_Noise | BM_NightVision | BM_HighDof | BM_MotionBlur)

static PostFXPass PassLookupTable[UseAll+1] = {
/* EE0 SD0 HH0 NN0 NV0 HD0 MB0*/	pp_composite,								//00
/* EE0 SD0 HH0 NN0 NV0 HD0 MB1*/	pp_composite_mb,							//01
/* EE0 SD0 HH0 NN0 NV0 HD1 MB0*/	pp_composite_highdof,						//02
/* EE0 SD0 HH0 NN0 NV0 HD1 MB1*/	pp_composite_mb_highdof,					//03
/* EE0 SD0 HH0 NN0 NV1 HD0 MB0*/	pp_composite_nv,							//04
/* EE0 SD0 HH0 NN0 NV1 HD0 MB1*/	pp_composite_mb_nv,							//05
/* EE0 SD0 HH0 NN0 NV1 HD1 MB0*/	pp_composite_highdof_nv,					//06
/* EE0 SD0 HH0 NN0 NV1 HD1 MB1*/	pp_composite_mb_highdof_nv,					//07
/* EE0 SD0 HH0 NN1 NV0 HD0 MB0*/	pp_composite_noise,							//08
/* EE0 SD0 HH0 NN1 NV0 HD0 MB1*/	pp_composite_mb_noise,						//09
/* EE0 SD0 HH0 NN1 NV0 HD1 MB0*/	pp_composite_highdof_noise,					//10
/* EE0 SD0 HH0 NN1 NV0 HD1 MB1*/	pp_composite_mb_highdof_noise,				//11
/* EE0 SD0 HH0 NN1 NV1 HD0 MB0*/	pp_composite_nv,							//12 Nightvision is always noisy...
/* EE0 SD0 HH0 NN1 NV1 HD0 MB1*/	pp_composite_mb_nv,							//13 Nightvision is always noisy...
/* EE0 SD0 HH0 NN1 NV1 HD1 MB0*/	pp_composite_highdof_nv,					//14 Nightvision is always noisy...
/* EE0 SD0 HH0 NN1 NV1 HD1 MB1*/	pp_composite_mb_highdof_nv,					//15 Nightvision is always noisy...
/* EE0 SD0 HH1 NN0 NV0 HD0 MB0*/	pp_composite_hh,							//16
/* EE0 SD0 HH1 NN0 NV0 HD0 MB1*/	pp_composite_mb_hh,							//17
/* EE0 SD0 HH1 NN0 NV0 HD1 MB0*/	pp_composite_highdof_hh,					//18
/* EE0 SD0 HH1 NN0 NV0 HD1 MB1*/	pp_composite_mb_highdof_hh,					//19
/* EE0 SD0 HH1 NN0 NV1 HD0 MB0*/	pp_composite_nv_hh,							//20
/* EE0 SD0 HH1 NN0 NV1 HD0 MB1*/	pp_composite_mb_nv_hh,						//21
/* EE0 SD0 HH1 NN0 NV1 HD1 MB0*/	pp_composite_highdof_nv_hh,					//22
/* EE0 SD0 HH1 NN0 NV1 HD1 MB1*/	pp_composite_mb_highdof_nv_hh,				//23
/* EE0 SD0 HH1 NN1 NV0 HD0 MB0*/	pp_composite_noise_hh,						//24
/* EE0 SD0 HH1 NN1 NV0 HD0 MB1*/	pp_composite_mb_noise_hh,					//25
/* EE0 SD0 HH1 NN1 NV0 HD1 MB0*/	pp_composite_highdof_noise_hh,				//26
/* EE0 SD0 HH1 NN1 NV0 HD1 MB1*/	pp_composite_mb_highdof_noise_hh,			//27
/* EE0 SD0 HH1 NN1 NV1 HD0 MB0*/	pp_composite_nv_hh,							//28 Nightvision is always noisy...
/* EE0 SD0 HH1 NN1 NV1 HD0 MB1*/	pp_composite_mb_nv_hh,						//29 Nightvision is always noisy...
/* EE0 SD0 HH1 NN1 NV1 HD1 MB0*/	pp_composite_highdof_nv_hh,					//30 Nightvision is always noisy...
/* EE0 SD0 HH1 NN1 NV1 HD1 MB1*/	pp_composite_mb_highdof_nv_hh,				//31 Nightvision is always noisy...
/* EE0 SD1 HH0 NN0 NV0 HD0 MB0*/	pp_composite_shallowhighdof,				//32
/* EE0 SD1 HH0 NN0 NV0 HD0 MB1*/	pp_composite_mb_shallowhighdof,				//33
/* EE0 SD1 HH0 NN0 NV0 HD1 MB0*/	pp_composite_shallowhighdof_nv,				//34
/* EE0 SD1 HH0 NN0 NV0 HD1 MB1*/	pp_composite_mb_shallowhighdof,				//35
/* EE0 SD1 HH0 NN0 NV1 HD0 MB0*/	pp_composite_shallowhighdof_nv,				//36
/* EE0 SD1 HH0 NN0 NV1 HD0 MB1*/	pp_composite_mb_shallowhighdof_nv,			//37
/* EE0 SD1 HH0 NN0 NV1 HD1 MB0*/	pp_composite_shallowhighdof_nv,				//38
/* EE0 SD1 HH0 NN0 NV1 HD1 MB1*/	pp_composite_mb_shallowhighdof_nv,			//39
/* EE0 SD1 HH0 NN1 NV0 HD0 MB0*/	pp_composite_noise_shallowhighdof,			//40
/* EE0 SD1 HH0 NN1 NV0 HD0 MB1*/	pp_composite_noise_mb_shallowhighdof,		//41
/* EE0 SD1 HH0 NN1 NV0 HD1 MB0*/	pp_composite_noise_shallowhighdof,			//42	
/* EE0 SD1 HH0 NN1 NV0 HD1 MB1*/	pp_composite_noise_mb_shallowhighdof,		//43	
/* EE0 SD1 HH0 NN1 NV1 HD0 MB0*/	pp_composite_shallowhighdof_nv,				//44	
/* EE0 SD1 HH0 NN1 NV1 HD0 MB1*/	pp_composite_mb_shallowhighdof_nv,			//45	
/* EE0 SD1 HH0 NN1 NV1 HD1 MB0*/	pp_composite_shallowhighdof_nv,				//46	
/* EE0 SD1 HH0 NN1 NV1 HD1 MB1*/	pp_composite_mb_shallowhighdof_nv,			//47	
/* EE0 SD1 HH1 NN0 NV0 HD0 MB0*/	pp_composite_shallowhighdof_hh,				//48	
/* EE0 SD1 HH1 NN0 NV0 HD0 MB1*/	pp_composite_mb_shallowhighdof_hh,			//49	
/* EE0 SD1 HH1 NN0 NV0 HD1 MB0*/	pp_composite_shallowhighdof_hh,				//50	
/* EE0 SD1 HH1 NN0 NV0 HD1 MB1*/	pp_composite_mb_shallowhighdof_hh,			//51	
/* EE0 SD1 HH1 NN0 NV1 HD0 MB0*/	pp_composite_shallowhighdof_nv_hh,			//52	
/* EE0 SD1 HH1 NN0 NV1 HD0 MB1*/	pp_composite_mb_shallowhighdof_nv_hh,		//53	
/* EE0 SD1 HH1 NN0 NV1 HD1 MB0*/	pp_composite_shallowhighdof_nv_hh,			//54	
/* EE0 SD1 HH1 NN0 NV1 HD1 MB1*/	pp_composite_mb_shallowhighdof_nv_hh,		//55	
/* EE0 SD1 HH1 NN1 NV0 HD0 MB0*/	pp_composite_noise_shallowhighdof_hh,		//56	
/* EE0 SD1 HH1 NN1 NV0 HD0 MB1*/	pp_composite_noise_mb_shallowhighdof_hh,	//57	
/* EE0 SD1 HH1 NN1 NV0 HD1 MB0*/	pp_composite_noise_shallowhighdof_hh,		//58	
/* EE0 SD1 HH1 NN1 NV0 HD1 MB1*/	pp_composite_noise_mb_shallowhighdof_hh,	//59	
/* EE0 SD1 HH1 NN1 NV1 HD0 MB0*/	pp_composite_shallowhighdof_nv_hh,			//60	
/* EE0 SD1 HH1 NN1 NV1 HD0 MB1*/	pp_composite_mb_shallowhighdof_nv_hh,		//61	
/* EE0 SD1 HH1 NN1 NV1 HD1 MB0*/	pp_composite_shallowhighdof_nv_hh,			//63	
/* EE0 SD1 HH1 NN1 NV1 HD1 MB1*/	pp_composite_mb_shallowhighdof_nv_hh,		//63
/* EE1 SD0 HH0 NN0 NV0 HD0 MB0*/	pp_composite_ee,							//64
/* EE1 SD0 HH0 NN0 NV0 HD0 MB1*/	pp_composite_mb_ee,							//65
/* EE1 SD0 HH0 NN0 NV0 HD1 MB0*/	pp_composite_highdof_ee,					//66
/* EE1 SD0 HH0 NN0 NV0 HD1 MB1*/	pp_composite_mb_highdof_ee,					//67
/* EE1 SD0 HH0 NN0 NV1 HD0 MB0*/	pp_composite_nv,							//68
/* EE1 SD0 HH0 NN0 NV1 HD0 MB1*/	pp_composite_mb_nv,							//69
/* EE1 SD0 HH0 NN0 NV1 HD1 MB0*/	pp_composite_highdof_nv,					//70
/* EE1 SD0 HH0 NN0 NV1 HD1 MB1*/	pp_composite_mb_highdof_nv,					//71
/* EE1 SD0 HH0 NN1 NV0 HD0 MB0*/	pp_composite_ee,							//72 Extra effects passes always include noise
/* EE1 SD0 HH0 NN1 NV0 HD0 MB1*/	pp_composite_mb_ee,							//73
/* EE1 SD0 HH0 NN1 NV0 HD1 MB0*/	pp_composite_highdof_ee,					//74
/* EE1 SD0 HH0 NN1 NV0 HD1 MB1*/	pp_composite_mb_highdof_ee,					//75
/* EE1 SD0 HH0 NN1 NV1 HD0 MB0*/	pp_composite_nv,							//76 Nightvision is always noisy...
/* EE1 SD0 HH0 NN1 NV1 HD0 MB1*/	pp_composite_mb_nv,							//77 Nightvision is always noisy...
/* EE1 SD0 HH0 NN1 NV1 HD1 MB0*/	pp_composite_highdof_nv,					//78 Nightvision is always noisy...
/* EE1 SD0 HH0 NN1 NV1 HD1 MB1*/	pp_composite_mb_highdof_nv,					//79 Nightvision is always noisy...
/* EE1 SD0 HH1 NN0 NV0 HD0 MB0*/	pp_composite_hh_ee,							//80
/* EE1 SD0 HH1 NN0 NV0 HD0 MB1*/	pp_composite_mb_hh_ee,						//81
/* EE1 SD0 HH1 NN0 NV0 HD1 MB0*/	pp_composite_highdof_hh_ee,					//82
/* EE1 SD0 HH1 NN0 NV0 HD1 MB1*/	pp_composite_mb_highdof_hh_ee,				//83
/* EE1 SD0 HH1 NN0 NV1 HD0 MB0*/	pp_composite_nv_hh,							//84
/* EE1 SD0 HH1 NN0 NV1 HD0 MB1*/	pp_composite_mb_nv_hh,						//85
/* EE1 SD0 HH1 NN0 NV1 HD1 MB0*/	pp_composite_highdof_nv_hh,					//86
/* EE1 SD0 HH1 NN0 NV1 HD1 MB1*/	pp_composite_mb_highdof_nv_hh,				//87
/* EE1 SD0 HH1 NN1 NV0 HD0 MB0*/	pp_composite_hh_ee,							//88
/* EE1 SD0 HH1 NN1 NV0 HD0 MB1*/	pp_composite_mb_hh_ee,						//89
/* EE1 SD0 HH1 NN1 NV0 HD1 MB0*/	pp_composite_highdof_hh_ee,					//90
/* EE1 SD0 HH1 NN1 NV0 HD1 MB1*/	pp_composite_mb_highdof_hh_ee,				//91
/* EE1 SD0 HH1 NN1 NV1 HD0 MB0*/	pp_composite_nv_hh,							//92 Nightvision is always noisy...
/* EE1 SD0 HH1 NN1 NV1 HD0 MB1*/	pp_composite_mb_nv_hh,						//93 Nightvision is always noisy...
/* EE1 SD0 HH1 NN1 NV1 HD1 MB0*/	pp_composite_highdof_nv_hh,					//94 Nightvision is always noisy...
/* EE1 SD0 HH1 NN1 NV1 HD1 MB1*/	pp_composite_mb_highdof_nv_hh,				//95 Nightvision is always noisy...
/* EE1 SD1 HH0 NN0 NV0 HD0 MB0*/	pp_composite_shallowhighdof_ee,				//96
/* EE1 SD1 HH0 NN0 NV0 HD0 MB1*/	pp_composite_mb_shallowhighdof_ee,			//97
/* EE1 SD1 HH0 NN0 NV0 HD1 MB0*/	pp_composite_shallowhighdof_nv,				//98
/* EE1 SD1 HH0 NN0 NV0 HD1 MB1*/	pp_composite_mb_shallowhighdof_ee,			//99
/* EE1 SD1 HH0 NN0 NV1 HD0 MB0*/	pp_composite_shallowhighdof_nv,				//100
/* EE1 SD1 HH0 NN0 NV1 HD0 MB1*/	pp_composite_mb_shallowhighdof_nv,			//101
/* EE1 SD1 HH0 NN0 NV1 HD1 MB0*/	pp_composite_shallowhighdof_nv,				//102
/* EE1 SD1 HH0 NN0 NV1 HD1 MB1*/	pp_composite_mb_shallowhighdof_nv,			//103
/* EE1 SD1 HH0 NN1 NV0 HD0 MB0*/	pp_composite_shallowhighdof_ee,				//104
/* EE1 SD1 HH0 NN1 NV0 HD0 MB1*/	pp_composite_mb_shallowhighdof_ee,			//105
/* EE1 SD1 HH0 NN1 NV0 HD1 MB0*/	pp_composite_shallowhighdof_ee,				//106	
/* EE1 SD1 HH0 NN1 NV0 HD1 MB1*/	pp_composite_mb_shallowhighdof_ee,			//107	
/* EE1 SD1 HH0 NN1 NV1 HD0 MB0*/	pp_composite_shallowhighdof_nv,				//108	
/* EE1 SD1 HH0 NN1 NV1 HD0 MB1*/	pp_composite_mb_shallowhighdof_nv,			//109	
/* EE1 SD1 HH0 NN1 NV1 HD1 MB0*/	pp_composite_shallowhighdof_nv,				//110	
/* EE1 SD1 HH0 NN1 NV1 HD1 MB1*/	pp_composite_mb_shallowhighdof_nv,			//111	
/* EE1 SD1 HH1 NN0 NV0 HD0 MB0*/	pp_composite_shallowhighdof_hh_ee,			//112	
/* EE1 SD1 HH1 NN0 NV0 HD0 MB1*/	pp_composite_mb_shallowhighdof_hh_ee,		//113	
/* EE1 SD1 HH1 NN0 NV0 HD1 MB0*/	pp_composite_shallowhighdof_hh_ee,			//114	
/* EE1 SD1 HH1 NN0 NV0 HD1 MB1*/	pp_composite_mb_shallowhighdof_hh_ee,		//115	
/* EE1 SD1 HH1 NN0 NV1 HD0 MB0*/	pp_composite_shallowhighdof_nv_hh,			//116	
/* EE1 SD1 HH1 NN0 NV1 HD0 MB1*/	pp_composite_mb_shallowhighdof_nv_hh,		//117	
/* EE1 SD1 HH1 NN0 NV1 HD1 MB0*/	pp_composite_shallowhighdof_nv_hh,			//118	
/* EE1 SD1 HH1 NN0 NV1 HD1 MB1*/	pp_composite_mb_shallowhighdof_nv_hh,		//119	
/* EE1 SD1 HH1 NN1 NV0 HD0 MB0*/	pp_composite_shallowhighdof_hh_ee,			//120	
/* EE1 SD1 HH1 NN1 NV0 HD0 MB1*/	pp_composite_mb_shallowhighdof_hh_ee,		//121	
/* EE1 SD1 HH1 NN1 NV0 HD1 MB0*/	pp_composite_shallowhighdof_hh_ee,			//122	
/* EE1 SD1 HH1 NN1 NV0 HD1 MB1*/	pp_composite_mb_shallowhighdof_hh_ee,		//123	
/* EE1 SD1 HH1 NN1 NV1 HD0 MB0*/	pp_composite_shallowhighdof_nv_hh,			//124	
/* EE1 SD1 HH1 NN1 NV1 HD0 MB1*/	pp_composite_mb_shallowhighdof_nv_hh,		//125	
/* EE1 SD1 HH1 NN1 NV1 HD1 MB0*/	pp_composite_shallowhighdof_nv_hh,			//126	
/* EE1 SD1 HH1 NN1 NV1 HD1 MB1*/	pp_composite_mb_shallowhighdof_nv_hh,		//127

};

static PostFXPass			FXAAPassToUse = pp_AA;

static PostFXPass			LumDownsamplePass[5];

static Matrix44				MotionBlurCurViewProjMat;
static Matrix44				MotionBlurPrevViewProjMat;
static Matrix44				MotionBlurPrevViewProjMatrix[2];
static Matrix44				MotionBlurBufferedPrevViewProjMat;
static Vec3V				MotionBlurPreviousCameraDir;
static Vector2				NoiseEffectRandomValues;

#if __BANK
static bool					NoDOF = false;
static bool					NoMotionBlur = false;
static bool					NoBloom = false;

PostFX::PostFXParamBlock::paramBlock	editedSettings;


static bkText*				toneMapInfo;
static char					toneMapInfoStr[255];

static bool					g_DebugBulletImpactTrigger = false;
static bool					g_DebugBulletImpactUseCursorPos = false;
static bool					g_DebugBulletImpactUseEnduranceIndicator = false;
static Vector3				g_DebugBulletImpactPos = Vector3(0.0f, 0.0f, 0.0f);
#endif // __BANK

#if AVG_LUMINANCE_COMPUTE
static bool				AverageLumunianceComputeEnable = true;
#endif

#if BOKEH_SUPPORT
static bool				BokehEnable = true;
static bool				BokehGenerated = false;
static float			BokehBrightnessExposureMin = -3.0f;
static float			BokehBrightnessExposureMax = 3.0f;
static float			BokehBlurThresholdVal = 0.25f;
static float			MaxBokehSizeVar = 20.0f;
static float			bokehSizeMultiplier = 1.5f;
static float			BokehShapeExposureRangeMin = -3.0f;
static float			BokehShapeExposureRangeMax = 3.0f;
static float			BokehGlobalAlpha = 1.0f;
static float			BokehAlphaCutoff = 0.15f;
#if __BANK
static int				bokehShapeOverride = -1;
#endif
#if __BANK
static char				bokehCountString[64];
static bool				BokehDebugOverlay = false;
#endif
#endif

static int				DofKernelSizeVar = 5;
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
static int				DofShallowKernelSizeVar = DOF_MaxSampleRadius;
#if ADAPTIVE_DOF
static int				DofAdaptiveKernelSizeVar = DOF_MaxSampleRadius;
#endif
#if COC_SPREAD
static int				DofCocSpreadKernelRadiusVar = 5;
#endif
static float			DofSkyWeightModifier = 0.6f;
static bool				DofUsePreAlphaDepth = true;

static bool				DofLumFilteringEnable		= true;
static float			DofLumContrastThreshold		= 20.0f;
static float			DofMaxLuminance				= 10.0f;

#if DOF_COMPUTE_GAUSSIAN
bool					sSetupGaussianWeights = false;
#endif
#endif //DOF_COMPUTE_GAUSSIAN

// state blocks for ProcessDepthFX
static grcRasterizerStateHandle		exitDepthFXRasterizerState				= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			exitDepthFXBlendState					= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	exitDepthFXDepthStencilState			= grcStateBlock::DSS_Invalid;
static grcRasterizerStateHandle		depthFXRasterizerState					= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			depthFXBlendState						= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	depthFXDepthStencilState				= grcStateBlock::DSS_Invalid;

// state blocks for ProcessSubSampleAlpha
static grcRasterizerStateHandle		exitSubSampleAlphaRasterizerState		= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			exitSubSampleAlphaBlendState			= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	exitSubSampleAlphaDepthStencilState		= grcStateBlock::DSS_Invalid;
static grcRasterizerStateHandle		subSampleAlphaRasterizerState			= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			subSampleAlphaBlendState				= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	subSampleAlphaDSState					= grcStateBlock::DSS_Invalid;
static grcDepthStencilStateHandle	subSampleAlphaWithStencilCullDSState	= grcStateBlock::DSS_Invalid;

// state blocks for ProcessNonDepthFX
static grcRasterizerStateHandle		exitNonDepthFXRasterizerState			= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			exitNonDepthFXBlendState				= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	exitNonDepthFXDepthStencilState			= grcStateBlock::DSS_Invalid;
static grcRasterizerStateHandle		nonDepthFXRasterizerState				= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			nonDepthFXBlendState					= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	nonDepthFXDepthStencilState				= grcStateBlock::DSS_Invalid;

#if BOKEH_SUPPORT
static grcBlendStateHandle			bokehBlendState					= grcStateBlock::BS_Invalid;
#endif

#if USE_IMAGE_WATERMARKS
static grcBlendStateHandle			imageWatermarkBlendState						= grcStateBlock::BS_Invalid;
#endif

// Damage overlay global params exposed in visual settings

PostFX::BulletImpactOverlay::Settings	PostFX::BulletImpactOverlay::ms_settings[PostFX::BulletImpactOverlay::DOT_COUNT];

float	PostFX::BulletImpactOverlay::ms_screenSafeZoneLength = 1.0f;
Vector4 PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[PostFX::BulletImpactOverlay::NUM_ENTRIES];
float	PostFX::BulletImpactOverlay::ms_frozenDamageOffsetMult[PostFX::BulletImpactOverlay::NUM_ENTRIES];

void GetPostFXDefaultRenderTargetFormat(grcTextureFormat& format, int& bpp)
{
#if __BANK
	if (PARAM_postfxusesFP16.Get())
	{
		bpp = 64;
		format = grctfA16B16G16R16F;
	}
	else
#endif
	{
		bpp = 32;
		format = grctfR11G11B10F;
	}
}

void PostFX::UpdateDefaultMotionBlur()
{

	float headingOffset = 0.0f;
	float pitchOffset = 0.0f;

	const atArray<tRenderedCameraObjectSettings>& renderedCameras = camInterface::GetRenderedCameras();

	const s32 numRenderedCameras = renderedCameras.GetCount();
	for(s32 i=0; i<numRenderedCameras; i++)
	{
		const camBaseObject* renderedCamera = renderedCameras[i].m_Object;
		if(renderedCamera)
		{
			const camControlHelper* controlHelper = NULL;

			if(renderedCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
			{
				controlHelper = static_cast<const camThirdPersonCamera*>(renderedCamera)->GetControlHelper();
			}
			else if(renderedCamera->GetIsClassId(camAimCamera::GetStaticClassId()))
			{
				controlHelper = static_cast<const camAimCamera*>(renderedCamera)->GetControlHelper();
			}
			else if(renderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
			{
				controlHelper = static_cast<const camCinematicMountedCamera*>(renderedCamera)->GetControlHelper();
			}

			if(controlHelper)
			{
				const float blendLevel = renderedCameras[i].m_BlendLevel;
				headingOffset  += blendLevel  * controlHelper->GetLookAroundHeadingOffset();
				pitchOffset += blendLevel  * controlHelper->GetLookAroundPitchOffset();
			}
		}
	}

	const float lookAroundOffset = Sqrtf((headingOffset * headingOffset) + (pitchOffset * pitchOffset));
	const float lookAroundSpeed = lookAroundOffset * fwTimer::GetCamInvTimeStep();
	float lengthScale = WIN32PC_SWITCH(Lerp(CSettingsManager::GetInstance().GetSettings().m_graphics.m_MotionBlurStrength, 1.0f, 4.0f), 1.0f);

	// Don't go over 1 or the effect breaks
	g_cachedDefaultMotionBlur = Saturate(lookAroundSpeed*g_DefaultMotionBlurStrength*lengthScale);

}

// Motion Blur Strength. Usually in range: 0.0 ( no blur ), 1.0 ( full blur )
void PostFX::SetMotionBlur()
{
#if __BANK
	if (g_Override==false)
#endif // __BANK
	{
		const camFrame& frame = gVpMan.GetCurrentGameViewportFrame();
		float motionBlurStrength = frame.GetMotionBlurStrength();

		const float maxFactor = 10.0f;
		const float powerFactor = 2.0f;

		// Don't go over 1 or the effect breaks
		float motionBlurLength = Saturate(powf(motionBlurStrength,powerFactor)*maxFactor);

		// Override with default motion blur if nothing else is driving it already
		if (g_DefaultMotionBlurEnabled && motionBlurLength == 0.0f && !rage::ioHeadTracking::IsMotionTrackingEnabled())
		{
#if RSG_PC
			if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Medium)
#endif // RSG_PC
			{
				motionBlurLength = g_cachedDefaultMotionBlur;
			}
		}

		PostFXParamBlock::GetUpdateThreadParams().m_isForcingMotionBlur = camInterface::IsRenderedCameraForcingMotionBlur();
		PostFXParamBlock::GetUpdateThreadParams().m_directionalBlurLength = Max(motionBlurLength,g_timeCycle.GetMotionBlurLength());
		PostFXParamBlock::GetUpdateThreadParams().m_directionalBlurGhostingFade = 0.2f; // tbr
		PostFXParamBlock::GetUpdateThreadParams().m_directionalBlurMaxVelocityMult = g_motionBlurMaxVelocityMult;

	}
}

static void PackHiDofParameters(Vector4& hiDofParameters)
{
	const float nearStartEndRange = hiDofParameters.y - hiDofParameters.x;
	hiDofParameters.y = (nearStartEndRange > 0.0f ? 1.0f/nearStartEndRange : 1.0f);

	const float farStartEndRange = hiDofParameters.w - hiDofParameters.z;
	hiDofParameters.w = (farStartEndRange > 0.0f ? 1.0f/farStartEndRange : 1.0f);
}

static void InitDepthFXStateBlocks()
{
	// render state
	grcRasterizerStateDesc rsd;
	rsd.CullMode = grcRSV::CULL_NONE;
	rsd.FillMode = grcRSV::FILL_SOLID;
	depthFXRasterizerState = grcStateBlock::CreateRasterizerState(rsd,"depthFXRasterizerState");

	grcBlendStateDesc bsd;
	bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	bsd.BlendRTDesc[0].SrcBlend = XENON_SWITCH(grcRSV::BLEND_ONE, grcRSV::BLEND_SRCALPHA);   // we will premultiply in the shader for significantly reduced banding on 360
	bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	bsd.BlendRTDesc[0].BlendEnable = 1;
	bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	depthFXBlendState = grcStateBlock::CreateBlendState(bsd, "depthFXBlendState");

	grcDepthStencilStateDesc dssd;
	dssd.DepthEnable = TRUE;
	dssd.DepthWriteMask = FALSE;
	dssd.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);
	
	dssd.StencilEnable = TRUE;
	dssd.StencilReadMask = 0x07;
	dssd.StencilWriteMask = 0x00;
	dssd.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	dssd.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	depthFXDepthStencilState = grcStateBlock::CreateDepthStencilState(dssd, "depthFXDepthStencilState");

#if MSAA_EDGE_PASS
	dssd.StencilReadMask |= EDGE_FLAG;
	dssd.FrontFace.StencilFunc = dssd.BackFace.StencilFunc = grcRSV::CMP_GREATER;
	edgeMaskDepthEffectFace_DS = grcStateBlock::CreateDepthStencilState(dssd, "edgeMaskDepthEffectFace_DS");
#endif //MSAA_EDGE_PASS

	// exit state blocks
	grcRasterizerStateDesc exitRsd;
	exitRsd.CullMode = grcRSV::CULL_NONE;
	exitRsd.FillMode = grcRSV::FILL_SOLID;
	exitDepthFXRasterizerState = grcStateBlock::CreateRasterizerState(exitRsd, "exitDepthFXRasterizerState");

	grcBlendStateDesc exitBsd;
	exitBsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	exitBsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	exitBsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	exitBsd.BlendRTDesc[0].BlendEnable = 1;
	exitDepthFXBlendState = grcStateBlock::CreateBlendState(exitBsd, "exitDepthFXBlendState");

	grcDepthStencilStateDesc exitDssd;
	exitDssd.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	exitDepthFXDepthStencilState = grcStateBlock::CreateDepthStencilState(exitDssd, "exitDepthFXDepthStencilState");
}

static void InitSubSampleAlphaStateBlocks()
{
	// render state
	grcRasterizerStateDesc rsd;
	rsd.CullMode = grcRSV::CULL_NONE;
	rsd.FillMode = grcRSV::FILL_SOLID;
#if __XENON
	rsd.HalfPixelOffset = 1;
#endif	
	subSampleAlphaRasterizerState = grcStateBlock::CreateRasterizerState(rsd, "subSampleAlphaRasterizerState");
	
	grcBlendStateDesc bsd;
	bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	bsd.BlendRTDesc[0].BlendEnable = 0;
	subSampleAlphaBlendState = grcStateBlock::CreateBlendState(bsd, "subSampleAlphaBlendState");

	grcDepthStencilStateDesc dssd;
	dssd.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	dssd.DepthEnable = 0;
	dssd.DepthWriteMask = 0;
	subSampleAlphaDSState = grcStateBlock::CreateDepthStencilState(dssd, "subSampleAlphaDSState");

	dssd.StencilEnable = 1;
	dssd.StencilReadMask = DEFERRED_MATERIAL_SPECIALBIT;
	dssd.StencilWriteMask = 0x0;
	dssd.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	dssd.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	dssd.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
	dssd.FrontFace.StencilFunc = __XENON ? grcRSV::CMP_NOTEQUAL : grcRSV::CMP_EQUAL;
	dssd.BackFace = dssd.FrontFace;
	subSampleAlphaWithStencilCullDSState = grcStateBlock::CreateDepthStencilState(dssd, "subSampleAlphaWithStencilCullDSState");

	// exit state blocks
	grcRasterizerStateDesc exitRsd;
	exitRsd.CullMode = grcRSV::CULL_NONE;
	exitRsd.FillMode = grcRSV::FILL_SOLID;
	exitSubSampleAlphaRasterizerState = grcStateBlock::CreateRasterizerState(exitRsd, "exitSubSampleAlphaRasterizerState");

	grcBlendStateDesc exitBsd;
	exitBsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	exitBsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	exitBsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	//exitBsd.BlendRTDesc[0].BlendEnable = 1;
	exitSubSampleAlphaBlendState = grcStateBlock::CreateBlendState(exitBsd, "exitSubSampleAlphaBlendState");

	grcDepthStencilStateDesc exitDssd;
	exitDssd.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	exitSubSampleAlphaDepthStencilState = grcStateBlock::CreateDepthStencilState(exitDssd, "exitSubSampleAlphaDepthStencilState");
}

static void InitNonDepthFXStateBlocks()
{
	// render state
	grcRasterizerStateDesc rsd;
	rsd.CullMode = grcRSV::CULL_NONE;
	rsd.FillMode = grcRSV::FILL_SOLID;
#if __XENON
	rsd.HalfPixelOffset = 1;
#endif	
	nonDepthFXRasterizerState = grcStateBlock::CreateRasterizerState(rsd, "nonDepthFXRasterizerState");

	grcBlendStateDesc bsd;
	bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	bsd.BlendRTDesc[0].BlendEnable = 0;
	nonDepthFXBlendState = grcStateBlock::CreateBlendState(bsd, "nonDepthFXBlendState");

	grcDepthStencilStateDesc dssd;
	dssd.DepthEnable = 0;
	dssd.DepthFunc = grcRSV::CMP_LESS;
	dssd.DepthWriteMask = 0;
	nonDepthFXDepthStencilState = grcStateBlock::CreateDepthStencilState(dssd, "nonDepthFXDepthStencilState");

	// exit state blocks
	grcRasterizerStateDesc exitRsd;
	exitRsd.CullMode = grcRSV::CULL_NONE;
	exitRsd.FillMode = grcRSV::FILL_SOLID;
	exitNonDepthFXRasterizerState = grcStateBlock::CreateRasterizerState(exitRsd, "exitNonDepthFXRasterizerState");

	grcBlendStateDesc exitBsd;
	exitBsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	exitBsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	exitBsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	exitBsd.BlendRTDesc[0].BlendEnable = 1;
	exitNonDepthFXBlendState = grcStateBlock::CreateBlendState(exitBsd, "exitNonDepthFXBlendState");

	grcDepthStencilStateDesc exitDssd;
	exitDssd.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	exitNonDepthFXDepthStencilState = grcStateBlock::CreateDepthStencilState(exitDssd, "exitNonDepthFXDepthStencilState");

#if BOKEH_SUPPORT
	if( BokehEnable )
	{
		grcBlendStateDesc bokblend;
		bokblend.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		bokblend.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		bokblend.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_ONE;
		bokblend.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		bokblend.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
		bokblend.BlendRTDesc[0].BlendEnable = 1;

		bokehBlendState = grcStateBlock::CreateBlendState(bokblend, "bokehBlendState");
	}
#endif

#if USE_IMAGE_WATERMARKS
	grcBlendStateDesc watermarkBlend;
	watermarkBlend.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	watermarkBlend.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	watermarkBlend.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	watermarkBlend.BlendRTDesc[0].BlendEnable = 1;
	imageWatermarkBlendState = grcStateBlock::CreateBlendState(watermarkBlend, "watermarkBlendState");
#endif

#if USE_SCREEN_WATERMARK
	grcBlendStateDesc blendStateBlockDesc;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendEnable = 1;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendOp = grcRSV::BLENDOP_ADD;
	blendStateBlockDesc.IndependentBlendEnable = 1;

	ms_watermarkBlendState = grcStateBlock::CreateBlendState(blendStateBlockDesc);
#endif
}

static void InitRenderStateBlocks()
{
	InitDepthFXStateBlocks();
	InitSubSampleAlphaStateBlocks();
	InitNonDepthFXStateBlocks();
}


static void SetNonDepthFXStateBlocks() 
{
	grcStateBlock::SetRasterizerState(nonDepthFXRasterizerState);
	grcStateBlock::SetBlendState(nonDepthFXBlendState);
	grcStateBlock::SetDepthStencilState(nonDepthFXDepthStencilState);
}

// helper function to set a texture variable and set the texture size global at the same time
void SetSrcTextureAndSize(grcEffectVar var, const grcTexture * tex)
{
	PostFXShader->SetVar(var, tex);
#if __PPU || __D3D11 || RSG_ORBIS
	PostFXShader->SetVar(TexelSizeId, 
		Vector4(1.0f / float(tex->GetWidth()), 1.0f / float(tex->GetHeight()),
					   float(tex->GetWidth()),		  float(tex->GetHeight())));
#endif
}

#if DOF_DIFFUSION

void RecreateDOFDiffusionRenderTargets(grcTextureFactory::CreateParams &params, u32 bpp, u32 width, u32 height)
{
	for(u32 i = 0; i < 16; i++)
	{
		delete dofDiffusionHorzABC[i]; dofDiffusionHorzABC[i] = NULL;
		delete dofDiffusionHorzX[i]; dofDiffusionHorzX[i] = NULL;
		delete dofDiffusionHorzY[i]; dofDiffusionHorzY[i] = NULL;
		delete dofDiffusionVertABC[i]; dofDiffusionVertABC[i] = NULL;
		delete dofDiffusionVertX[i]; dofDiffusionVertX[i] = NULL;
		delete dofDiffusionVertY[i]; dofDiffusionVertY[i] = NULL;
	}

#define DIFFUSION_DOF_ABC_FORMAT				grctfA32B32G32R32F			//Can probably get away with 16f here
	// #define DIFFUSION_DOF_ABC_VIEW_FORMAT_PACKED	DXGI_FORMAT_R32G32B32A32_UINT
	// #define DIFFUSION_DOF_ABC_VIEW_FORMAT_NORMAL	DXGI_FORMAT_R32G32B32A32_FLOAT
#define DIFFUSION_DOF_XY_FORMAT					grctfA16B16G16R16F

	u32 dofWidth = width;
	u32 dofHeight = height;
	char targetName[64];
	grcTextureFormat dstFormat = grctfA16B16G16R16F;
	for( int i = 0; i< 16; ++i )
	{
		dofDiffusionHorzABC[i]    = NULL;
		dofDiffusionHorzX[i]      = NULL;
		dofDiffusionHorzY[i]		 = NULL;
		dofWidth            = width >> i;
		if( dofWidth > 1 )
		{
			params.Format = DIFFUSION_DOF_ABC_FORMAT;
			if( i > 1 )
			{
				sprintf(targetName, "dofDiffusionHorzABC[%d]", i );
				dofDiffusionHorzABC[i] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, targetName, grcrtPermanent, dofWidth, dofHeight, bpp, &params, 0 WIN32PC_ONLY(, dofDiffusionHorzABC[i]));
			}

			if( i != 0 )
				params.Format = DIFFUSION_DOF_XY_FORMAT;
			else
				params.Format = dstFormat;

			if( i != 1 )
			{			
				sprintf(targetName, "dofDiffusionHorzY[%d]", i );
				dofDiffusionHorzY[i] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, targetName, grcrtPermanent, dofWidth, dofHeight, bpp, &params, 0 WIN32PC_ONLY(, dofDiffusionHorzY[i]));
			}

			dofWidth >>= 1;
			params.Format = DIFFUSION_DOF_XY_FORMAT;

			if( dofWidth > 1 && i > 0 )
			{
				sprintf(targetName, "dofDiffusionHorzX[%d]", i );
				dofDiffusionHorzX[i] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, targetName, grcrtPermanent, dofWidth, dofHeight, bpp, &params, 0 WIN32PC_ONLY(, dofDiffusionHorzX[i]));
			}
		}
	}

	dofWidth = width;
	dofHeight = height;

	for( int i = 0; i< 16; ++i )
	{
		dofDiffusionVertABC[i]  = NULL;
		dofDiffusionVertX[i]    = NULL;
		dofDiffusionVertY[i]    = NULL;
		dofHeight = height >> i;
		if( dofHeight > 1 )
		{
			params.Format = DIFFUSION_DOF_ABC_FORMAT;
			if( i > 1 )
			{
				sprintf(targetName, "dofDiffusionVertABC[%d]", i );
				dofDiffusionVertABC[i] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, targetName, grcrtPermanent, dofWidth, dofHeight, bpp, &params, 0 WIN32PC_ONLY(, dofDiffusionVertABC[i]));
			}

			params.Format = DIFFUSION_DOF_XY_FORMAT;
			if( i > 1 )
			{
				sprintf(targetName, "dofDiffusionVertY[%d]", i );
				dofDiffusionVertY[i] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, targetName, grcrtPermanent, dofWidth, dofHeight, bpp, &params, 0 WIN32PC_ONLY(, dofDiffusionVertY[i]));
			}

			dofHeight >>= 1;

			if( dofHeight > 1 && i > 0 )
			{
				sprintf(targetName, "dofDiffusionVertX[%d]", i );
				dofDiffusionVertX[i] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, targetName, grcrtPermanent, dofWidth, dofHeight, bpp, &params, 0 WIN32PC_ONLY(, dofDiffusionVertX[i]));
			}
		}
	}
}

#endif //DOF_DIFFUSION

void RecreateDOFRenderTargets(grcTextureFactory::CreateParams &params, u32 bpp, u32 width, u32 height)
{
	PostFX::SetGaussianBlur();

	int dofTargetWidth = width/4;
	int dofTargetHeight = height/4;

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
	if( CurrentDOFTechnique == dof_computeGaussian || CurrentDOFTechnique == dof_diffusion)
	{
		dofTargetWidth = width;
		dofTargetHeight = height;
# if DOF_COMPUTE_GAUSSIAN
		params.UseAsUAV = true;
# endif
	}
#endif //DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
	
	if (!DepthOfField1 || !DepthOfField2 || dofTargetWidth != DepthOfField1->GetWidth() || dofTargetHeight != DepthOfField1->GetHeight())
	{
		// on PC the targets will get re-used
#if !RSG_PC
		if (DepthOfField1)
		{
			delete DepthOfField1;
			DepthOfField1 = NULL;
		}
		if (DepthOfField2)
		{
			delete DepthOfField2;
			DepthOfField2 = NULL;
		}
#endif //!RSG_PC
		ASSERT_ONLY(RESOURCE_CACHE_ONLY(bool prevSafeCreate = grcResourceCache::GetInstance().sm_SafeCreate));
		ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
		DepthOfField1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DOF 1", grcrtPermanent, dofTargetWidth, dofTargetHeight, bpp, &params, 1 WIN32PC_ONLY(, DepthOfField1));
		DepthOfField2 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DOF 2", grcrtPermanent, dofTargetWidth, dofTargetHeight, bpp, &params, 1 WIN32PC_ONLY(, DepthOfField2));
		ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(prevSafeCreate);))
	}

#if DOF_COMPUTE_GAUSSIAN
	params.UseAsUAV = false;
#endif
}

#if BOKEH_SUPPORT
void RecreateBokehRenderTargets(grcTextureFactory::CreateParams &params, int bpp, u32 width, u32 height)
{
#if BOKEH_SUPPORT
		BokehEnable = (GRCDEVICE.GetDxFeatureLevel() >= 1100)  NOTFINAL_ONLY(&& !PARAM_disableBokeh.Get());
#if RSG_PC
		BokehEnable &= (CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::High) ? true : false;
#endif // RSG_PC
#endif

	if( BokehEnable )
	{
		// BokehPoints store sprite transparency in alpha channel
		bpp = 64;
		params.Format = grctfA16B16G16R16F;
		params.StereoRTMode = grcDevice::STEREO;

		BokehPoints = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bokeh Points", grcrtPermanent, width, height, bpp, &params, 0 WIN32PC_ONLY(, BokehPoints));
#if RSG_DURANGO
		params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_6;
#endif
		
		// Revert back to whatever the default format is (e.g: FP11 or FP16)
		GetPostFXDefaultRenderTargetFormat(params.Format, bpp);

		BokehPointsTemp = !RSG_DURANGO ? BloomBuffer : CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bokeh Points Temp", grcrtPermanent, width/2, height/2, bpp, &params, 0 WIN32PC_ONLY(, BokehPointsTemp));
#if RSG_DURANGO
		params.ESRAMPhase = 0;
#endif
		params.StereoRTMode = grcDevice::AUTO;
		//small texture to use when no bokeh rendering

		if (BokehNoneRenderedTexture)
		{
			BokehNoneRenderedTexture->Release();
			BokehNoneRenderedTexture = NULL;
		}

		grcImage* image = grcImage::Create(4, 4, 1, grcImage::DXT1, grcImage::STANDARD, 0, 0);
		DXT::DXT1_BLOCK* block = reinterpret_cast<DXT::DXT1_BLOCK*>(image->GetBits());
		block->SetColour(DXT::ARGB8888(0,0,0,0));
		BokehNoneRenderedTexture = grcTextureFactory::GetInstance().Create(image);
		image->Release();

		params.Format=grctfG16R16F;
#if COC_SPREAD
		if( CurrentDOFTechnique == dof_computeGaussian || CurrentDOFTechnique == dof_diffusion)
			params.UseAsUAV = true;
#endif
#if RSG_DURANGO
		params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_5 | grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_6;
		params.ESRAMMaxSize = 4 * 1024 * 1024;
#endif
		BokehDepthBlur = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "BokehDepthBlur", grcrtPermanent, width, height, bpp, &params, 0 WIN32PC_ONLY(, BokehDepthBlur));		

#if RSG_DURANGO
		params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_6;
		params.ESRAMMaxSize = 0;
#endif
		BokehDepthBlurHalfRes = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "BokehDepthBlurHalfRes", grcrtPermanent, width/4, height/4, bpp, &params, 0 WIN32PC_ONLY(, BokehDepthBlurHalfRes));

#if RSG_DURANGO
		params.ESRAMPhase = 0;
#endif
#if RSG_DURANGO && __BANK
		AltBokehDepthBlur = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "AltBokehDepthBlur", grcrtPermanent, width, height, bpp, &params, 0 WIN32PC_ONLY(, AltBokehDepthBlur));		
#endif

#if COC_SPREAD
		if( CurrentDOFTechnique == dof_computeGaussian || CurrentDOFTechnique == dof_diffusion)
		{
#if RSG_DURANGO
			params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_6;
#endif
			BokehDepthBlurHalfResTemp = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "BokehDepthBlurHalfResTemp", grcrtPermanent, width/4, height/4, bpp, &params, 0 WIN32PC_ONLY(, BokehDepthBlurHalfResTemp));
			params.UseAsUAV = false;
#if RSG_DURANGO
			params.ESRAMPhase = 0;
#endif
		}
#endif		
	}
	else
	{
		if( BokehPoints )
		{
			delete BokehPoints;
			BokehPoints = NULL;
		}
		if( BokehPointsTemp )
		{
			// Shared with Bloom Buffer now
			DURANGO_ONLY(delete BokehPointsTemp;)
			BokehPointsTemp = NULL;
		}
#if RSG_DURANGO && __BANK
		if( AltBokehDepthBlur )
		{
			delete AltBokehDepthBlur;
			AltBokehDepthBlur = NULL;
		}
#endif
		if (BokehNoneRenderedTexture)
		{
			BokehNoneRenderedTexture->Release();
			BokehNoneRenderedTexture = NULL;
		}

		if( BokehDepthBlur )
		{
			delete BokehDepthBlur;
			BokehDepthBlur = NULL;
#if BOKEH_SUPPORT
			dofComputeShader->SetVar(dofComputeDepthBlurTex, grcTexture::None);
#endif
		}
		if( BokehDepthBlurHalfRes )
		{
			delete BokehDepthBlurHalfRes;
			BokehDepthBlurHalfRes = NULL;
		}
#if COC_SPREAD
		if( BokehDepthBlurHalfResTemp )
		{
			delete BokehDepthBlurHalfResTemp;
			BokehDepthBlurHalfResTemp = NULL;
		}
#endif
	}
}
#endif

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
#if RSG_PC
static bool targetsInitialized = false;
#endif

static void CreateScreenBasedRenderTargets(int iWidth, int iHeight)
{
#if RSG_PC
	targetsInitialized = true;
#endif
	//Create render targets.
	grcTextureFactory::CreateParams params;
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 
	params.UseFloat = true;

	params.InTiledMemory	= true;
	params.IsSwizzled		= false;
	params.UseFloat = true;
	params.EnableCompression = false;

	int bpp = 64;

#if !__FINAL && (__D3D11 || RSG_ORBIS)
	if( PARAM_DX11Use8BitTargets.Get() )
	{
		bpp = 32;
		params.Format = grctfA8R8G8B8;
		params.UseFloat = false;
		params.IsSRGB = true;
	}
	else
#endif // !__FINAL && (__D3D11 || RSG_ORBIS)
	{

		// Use FP11 unless we're running with postfxusesFP16
		GetPostFXDefaultRenderTargetFormat(params.Format, bpp);
	}

#if RSG_PC
	params.StereoRTMode = grcDevice::STEREO;
#endif

	HalfScreen0=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Half Screen 0", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params, 0 WIN32PC_ONLY(, HalfScreen0));

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_7 | grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_8 |
 		grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_9;
#endif
	
	WIN32PC_ONLY(if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Medium))
		BloomBuffer=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params, 1 WIN32PC_ONLY(, BloomBuffer));

#if RSG_DURANGO
	params.ESRAMPhase = 0;
#endif
	WIN32PC_ONLY(if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Medium))
		BloomBufferUnfiltered=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Unfiltered", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferUnfiltered));

	RecreateDOFRenderTargets(params, bpp, iWidth, iHeight);

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_7 | grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_8;
	BloomBufferHalf0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Half 0", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferHalf0));
#else
	// Bloom Half 0 shared 
	BloomBufferHalf0 = HalfScreen0;
#endif

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_7;
#endif
	BloomBufferHalf1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Half 1", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferHalf1));

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_4;
	BloomBufferQuarterX = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Quarter X", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferQuarterX));

	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_7 | grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_8;
#endif
	BloomBufferQuarter0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Quarter 0", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferQuarter0));

#if (RSG_PC)
	grcTextureFormat DefaultFormat = params.Format;
#endif

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_7;
#endif

	BloomBufferQuarter1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Quarter 1", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferQuarter1));
	BloomBufferEighth1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Eighth 1", grcrtPermanent, iWidth/8, iHeight/8, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferEighth1));
	BloomBufferSixteenth1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Sixteenth 1", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferSixteenth1));

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_7 | grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_8;
#endif

	BloomBufferEighth0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Eighth 0", grcrtPermanent, iWidth/8, iHeight/8, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferEighth0));
	BloomBufferSixteenth0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Bloom Buffer Sixteenth 0", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params, 1 WIN32PC_ONLY(, BloomBufferSixteenth0));

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_3;
	DSBB2  = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DSBB2" , grcrtPermanent, iWidth/2 , iHeight/2 , bpp, &params, 1 WIN32PC_ONLY(, DSBB2));
	DSBB4  = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DSBB4" , grcrtPermanent, iWidth/4 , iHeight/4 , bpp, &params, 1 WIN32PC_ONLY(, DSBB4));
	DSBB8  = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DSBB8" , grcrtPermanent, iWidth/8 , iHeight/8 , bpp, &params, 1 WIN32PC_ONLY(, DSBB8));
	DSBB16 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DSBB16", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params, 1 WIN32PC_ONLY(, DSBB16));
	params.ESRAMPhase = 0;
#endif

#if !RSG_DURANGO && LIGHT_VOLUME_USE_LOW_RESOLUTION
	m_volumeOffscreenBuffer = BloomBufferHalf0;
	m_volumeReconstructionBuffer = BloomBufferHalf1;
#endif

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_8 | grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_9;
#endif
	WIN32PC_ONLY(if(LENSARTEFACTSMGR.IsActive()))
		LensArtefactsFinalBuffer						= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Lens Artefact Final Buffer", grcrtPermanent, iWidth/2, iHeight/2, bpp, &params, 1 WIN32PC_ONLY(, LensArtefactsFinalBuffer));
#if RSG_DURANGO
	params.ESRAMPhase = 0;
#endif
	WIN32PC_ONLY(if(LENSARTEFACTSMGR.IsActive()))
	{
		LensArtefactsBuffers0[LENSARTEFACT_HALF]		= BloomBufferHalf0;
		LensArtefactsBuffers1[LENSARTEFACT_HALF]		= BloomBufferHalf1;
	}

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_8;
#endif

	WIN32PC_ONLY(if(LENSARTEFACTSMGR.IsActive()))
	{
		LensArtefactsBuffers0[LENSARTEFACT_QUARTER]		=  CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Lens Artefact 4x Buffer 0", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params, 1 WIN32PC_ONLY(, LensArtefactsBuffers0[LENSARTEFACT_QUARTER]));
		LensArtefactsBuffers1[LENSARTEFACT_QUARTER]		=  CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Lens Artefact 4x Buffer 1", grcrtPermanent, iWidth/4, iHeight/4, bpp, &params, 1 WIN32PC_ONLY(, LensArtefactsBuffers1[LENSARTEFACT_QUARTER]));
		LensArtefactsBuffers0[LENSARTEFACT_EIGHTH]		=  CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Lens Artefact 8x Buffer 0", grcrtPermanent, iWidth/8, iHeight/8, bpp, &params, 1 WIN32PC_ONLY(, LensArtefactsBuffers0[LENSARTEFACT_EIGHTH]));
		LensArtefactsBuffers1[LENSARTEFACT_EIGHTH]		=  CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Lens Artefact 8x Buffer 1", grcrtPermanent, iWidth/8, iHeight/8, bpp, &params, 1 WIN32PC_ONLY(, LensArtefactsBuffers1[LENSARTEFACT_EIGHTH]));
		LensArtefactsBuffers0[LENSARTEFACT_SIXTEENTH]	=  CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Lens Artefact 16x Buffer 0", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params, 1 WIN32PC_ONLY(, LensArtefactsBuffers0[LENSARTEFACT_SIXTEENTH]));
		LensArtefactsBuffers1[LENSARTEFACT_SIXTEENTH]	=  CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Lens Artefact 16x Buffer 1", grcrtPermanent, iWidth/16, iHeight/16, bpp, &params, 1 WIN32PC_ONLY(, LensArtefactsBuffers1[LENSARTEFACT_SIXTEENTH]));
	}
#if RSG_DURANGO
	params.ESRAMPhase = 0;
#endif

	WIN32PC_ONLY(if(LENSARTEFACTSMGR.IsActive()))
	{
		LensArtefactSrcBuffers[LENSARTEFACT_HALF]		= BloomBuffer;
		LensArtefactSrcBuffers[LENSARTEFACT_QUARTER]	= BloomBufferQuarter0;
		LensArtefactSrcBuffers[LENSARTEFACT_EIGHTH]		= BloomBufferEighth0;
		LensArtefactSrcBuffers[LENSARTEFACT_SIXTEENTH]	= BloomBufferSixteenth0;
	}

	// Set render targets and shader data for lens artefacts system
	LENSARTEFACTSMGR.SetRenderTargets(LensArtefactsFinalBuffer, BloomBufferUnfiltered, LensArtefactSrcBuffers,
		LensArtefactsBuffers0, LensArtefactsBuffers1);

	DepthOfField0 =	BloomBufferHalf1;

#if FILM_EFFECT
	PreFilmEffect = !PostFX::g_EnableFilmEffect ? NULL : CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "LensDistortion", grcrtPermanent, iWidth, iHeight, bpp, &params, 0,  WIN32PC_ONLY(PreFilmEffect));
#endif

#if BOKEH_SUPPORT
	RecreateBokehRenderTargets(params, bpp, iWidth, iHeight);
#endif

#if DOF_DIFFUSION
	if( CurrentDOFTechnique == dof_diffusion)
	{
		RecreateDOFDiffusionRenderTargets(params, bpp, iWidth, iHeight);
	}
#endif //DOF_DIFFUSION

	params.Format=grctfL8;
	HeatHaze0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "HeatHaze 1", grcrtPermanent, iWidth/8, iHeight/8, 8, &params, 1 WIN32PC_ONLY(, HeatHaze0));
	HeatHaze1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "HeatHaze 2", grcrtPermanent, iWidth/8, iHeight/8, 8, &params, 1 WIN32PC_ONLY(, HeatHaze1));
	
#if __WIN32PC
	params.StereoRTMode = grcDevice::STEREO;
#endif

	params.Format = grctfA8R8G8B8;
	params.UseFloat = false;
	SSLRCutout=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "SSLR Cutout", grcrtPermanent, 256, 256, 8, &params, 1 WIN32PC_ONLY(, SSLRCutout));
	LRIntensity=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "SSLR Intensity", grcrtPermanent, iWidth/4, iHeight/4, 8, &params, 1 WIN32PC_ONLY(, LRIntensity));
	LRIntensity2=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "SSLR Intensity2", grcrtPermanent, iWidth/4, iHeight/4, 8, &params, 1 WIN32PC_ONLY(, LRIntensity2));

#if __WIN32PC
	params.StereoRTMode = grcDevice::AUTO;
	if(grcEffect::GetShaderQuality() > 0)
#endif
	{
		params.Format = grctfG16R16F;
		params.UseFloat = true;
		PostFX::FogRayRT=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Global FogRay", grcrtPermanent, iWidth/2, iHeight/2, 32, &params, 1 WIN32PC_ONLY(, PostFX::FogRayRT));
		PostFX::FogRayRT2=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Global FogRay 2", grcrtPermanent, iWidth/2, iHeight/2, 32, &params, 1 WIN32PC_ONLY(, PostFX::FogRayRT2));
	}	

	params.Format = grctfR32F;
	params.UseFloat = false;
	params.Lockable = false;

	PostFX::FogOfWarRT0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Fog of War 0", grcrtPermanent, 64, 64, 32, &params, 3 WIN32PC_ONLY(, PostFX::FogOfWarRT0));
	PostFX::FogOfWarRT1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Fog of War 1", grcrtPermanent, 16, 16, 32, &params, 3 WIN32PC_ONLY(, PostFX::FogOfWarRT1));
	PostFX::FogOfWarRT2 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Fog of War 2", grcrtPermanent, 4, 4, 32, &params, 3 WIN32PC_ONLY(, PostFX::FogOfWarRT2));

	// Depending on the device resolution, we need a different number of passes and filter sizes
	// to correctly downsample the luminance; the code below can only handle 1080, 900 and 720.
	// Any other resolution defaults to the 720 behaviour.
	int deviceHeight = GRCDEVICE.GetHeight();

#if RSG_PC
	if (deviceHeight > 1080)
	{
		deviceHeight = 1080;
	}
	else if (deviceHeight > 900)
	{
		deviceHeight = 900;
	}
	else
	{
		deviceHeight = 720;
	}
#endif

	int LumDownsampleRTWidth[5];
	int LumDownsampleRTHeight[5];

	switch (deviceHeight)
	{
		case 1080:
		{
			LumDownsampleRTWidth[0]		= 120;
			LumDownsampleRTHeight[0]	= 90;
			LumDownsampleRTWidth[1]		= 30;
			LumDownsampleRTHeight[1]	= 30;
			LumDownsampleRTWidth[2]		= 10;
			LumDownsampleRTHeight[2]	= 10;
			LumDownsampleRTWidth[3]		= 5;
			LumDownsampleRTHeight[3]	= 5;
			LumDownsampleRTWidth[4]		= 1;
			LumDownsampleRTHeight[4]	= 1;

			LumDownsamplePass[0] = pp_lum_4x3_conversion;
			LumDownsamplePass[1] = pp_lum_4x3;
			LumDownsamplePass[2] = pp_lum_3x3;
			LumDownsamplePass[3] = pp_lum_2x2;
			LumDownsamplePass[4] = pp_lum_5x5;
			PostFX::LumDownsampleRTCount = 5;

			break;
		}


		case 900:
		{
			LumDownsampleRTWidth[0]		= 100;
			LumDownsampleRTHeight[0]	= 75;
			LumDownsampleRTWidth[1]		= 25;
			LumDownsampleRTHeight[1]	= 25;
			LumDownsampleRTWidth[2]		= 5;
			LumDownsampleRTHeight[2]	= 5;
			LumDownsampleRTWidth[3]		= 1;
			LumDownsampleRTHeight[3]	= 1;
			
			LumDownsamplePass[0] = pp_lum_4x3_conversion;
			LumDownsamplePass[1] = pp_lum_4x3;
			LumDownsamplePass[2] = pp_lum_5x5;
			LumDownsamplePass[3] = pp_lum_5x5;
			PostFX::LumDownsampleRTCount = 4;

			break;
		}

		case 720:
		default:
		{
			LumDownsampleRTWidth[0]		= 80;
			LumDownsampleRTHeight[0]	= 60;
			LumDownsampleRTWidth[1]		= 20;
			LumDownsampleRTHeight[1]	= 20;
			LumDownsampleRTWidth[2]		= 10;
			LumDownsampleRTHeight[2]	= 10;
			LumDownsampleRTWidth[3]		= 5;
			LumDownsampleRTHeight[3]	= 5;
			LumDownsampleRTWidth[4]		= 1;
			LumDownsampleRTHeight[4]	= 1;

			LumDownsamplePass[0] = pp_lum_4x3_conversion;
			LumDownsamplePass[1] = pp_lum_4x3;
			LumDownsamplePass[2] = pp_lum_2x2;
			LumDownsamplePass[3] = pp_lum_2x2;
			LumDownsamplePass[4] = pp_lum_5x5;
			PostFX::LumDownsampleRTCount = 5;

			if (deviceHeight != 720)
			{
				Warningf("Resolution not supported for luminance downsample targets (%d)", deviceHeight);
			}
			break;
		}
	}	

	#if AVG_LUMINANCE_COMPUTE
	if(AverageLumunianceComputeEnable)
	{		
		float fdeviceWidth = (float)(iWidth/4);
		float fdeviceHeight = (float)(iHeight/4);

		float maxSize = rage::Max(fdeviceWidth, fdeviceHeight);
		
		PostFX::LumCSDownsampleUAVCount = 0;

		params.UseAsUAV = true;
		while ( maxSize > 16.0f )
		{			
			Assertf(PostFX::LumCSDownsampleUAVCount < 5, "Exceeded maximum number of 5 for UAVs for luminance downsampling.");

			maxSize = ceilf(maxSize / 16.0f);
						
			fdeviceWidth = ceilf(fdeviceWidth / 16.0f);
			fdeviceHeight = ceilf(fdeviceHeight / 16.0f); 

			char szLumDownName[256];
			formatf(szLumDownName, 254, "LumDownsampleUAV %d", PostFX::LumCSDownsampleUAVCount);

			PostFX::LumCSDownsampleUAV[PostFX::LumCSDownsampleUAVCount]=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, szLumDownName, grcrtPermanent, (int)fdeviceWidth, (int)fdeviceHeight, 32, &params, 3  WIN32PC_ONLY(, PostFX::LumCSDownsampleUAV[PostFX::LumCSDownsampleUAVCount]) );			

			PostFX::LumCSDownsampleUAVCount++;
		}				

		//1x1 
		params.UseFloat = true; 
		#if RSG_DURANGO
				params.ESRAMPhase = 0;	// final buffer is tiny and hangs around too long 
		#endif
		PostFX::LumCSDownsampleUAV[PostFX::LumCSDownsampleUAVCount]=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "LumDownsampleUAV Final", grcrtPermanent, 1, 1, 32, &params, 3  WIN32PC_ONLY(, PostFX::LumCSDownsampleUAV[PostFX::LumCSDownsampleUAVCount]) );			
		
		params.UseAsUAV = false;
	}	
	else
	{
#endif	//AVG_LUMINANCE_COMPUTE

#if (RSG_PC)
	int iWidth4 = iWidth/4;
	int iHeight4 = iHeight/4;

	int lumWidth = LumDownsampleRTWidth[0];
	int lumHeight = LumDownsampleRTHeight[0];

	PostFX::ImLumDownsampleRTCount = 0;

	params.Format = DefaultFormat;

	while (iWidth4/4 > lumWidth || iHeight4/4 > lumHeight)
	{
		Assertf(PostFX::ImLumDownsampleRTCount < 5, "Exceeded maximum number of 5 for intermediate textures for luminance downsampling.");

		iWidth4 /= 4;
		iHeight4 /= 4;

		int imLumRTWidth = rage::Max(lumWidth*4, iWidth4);
		int imLumRTHeight = rage::Max(lumHeight*3, iHeight4);

		char szLumDownName[256];
		formatf(szLumDownName, 254, "ImLumDownsample %d", PostFX::ImLumDownsampleRTCount);


		PostFX::ImLumDownsampleRT[PostFX::ImLumDownsampleRTCount]=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, szLumDownName, grcrtPermanent, imLumRTWidth, imLumRTHeight, bpp, &params, 3 WIN32PC_ONLY(, PostFX::ImLumDownsampleRT[PostFX::ImLumDownsampleRTCount]));
		PostFX::ImLumDownsampleRTCount++;
	}
	params.Format = grctfR32F;
#endif

#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_4;
#endif

	for (int i = 0; i < PostFX::LumDownsampleRTCount; i++)
	{
		if (i == PostFX::LumDownsampleRTCount - 1) 
		{ 
			params.UseFloat = true; 
#if RSG_DURANGO
			params.ESRAMPhase = 0;	// final buffer is tiny and hangs around too long 
#endif
		}
		char szLumDownName[256];
		formatf(szLumDownName, 254, "LumDownsample %d", i);
		PostFX::LumDownsampleRT[i]=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, szLumDownName, grcrtPermanent, LumDownsampleRTWidth[i], LumDownsampleRTHeight[i], 32, &params, 3 WIN32PC_ONLY(, PostFX::LumDownsampleRT[i]));
	}
#if AVG_LUMINANCE_COMPUTE
	}

	if (AverageLumunianceComputeEnable)
	{
		LumRT = PostFX::LumCSDownsampleUAV[PostFX::LumCSDownsampleUAVCount];
	}
	else 		
#endif //AVG_LUMINANCE_COMPUTE
	{
		LumRT = PostFX::LumDownsampleRT[PostFX::LumDownsampleRTCount - 1];
	}

	params.Format = grctfA32B32G32R32F;
	params.InTiledMemory = false;
	params.IsSwizzled = false;
	params.EnableCompression = false;
#if RSG_DURANGO
	params.ESRAMPhase = 0;
#endif

	for (u32 i = 0; i < MAX_EXP_TARGETS; i++)
	{
		char szName[256];
		formatf(szName, "Current Exposure %d", i);
#if __D3D11
		grcTextureFactory::TextureCreateParams TextureParams(
			grcTextureFactory::TextureCreateParams::SYSTEM, 
			grcTextureFactory::TextureCreateParams::LINEAR,	
			grcsRead|grcsWrite, 
			NULL, 
			grcTextureFactory::TextureCreateParams::RENDERTARGET, 
			grcTextureFactory::TextureCreateParams::MSAA_NONE);
		if (!CurExposureTex[i]) {
			BANK_ONLY(grcTexture::SetCustomLoadName(szName);)
			CurExposureTex[i] = grcTextureFactory::GetInstance().Create( 1, 1, grctfA32B32G32R32F, NULL, 1, &TextureParams );
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
		}
		CurExposureRT[i] = grcTextureFactory::GetInstance().CreateRenderTarget(szName, CurExposureTex[i]->GetTexturePtr() WIN32PC_ONLY(, CurExposureRT[i]));			
#else		
		CurExposureRT[i] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, szName, grcrtPermanent, 1, 1, 128, &params, 3 WIN32PC_ONLY(, CurExposureRT[i]));
#endif
	}

#if RSG_PC
	params.Format = grctfR32F;
	PostFX::CenterReticuleDist = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Center Reticule Dist", grcrtPermanent, 1, 1, 32, &params, 0 WIN32PC_ONLY(, PostFX::CenterReticuleDist));
#endif

#if ADAPTIVE_DOF
	AdaptiveDepthOfField.CreateRenderTargets();
#endif

	grcTextureFactory::CreateParams mb_params;
	mb_params.Multisample = 0;
	mb_params.HasParent = true;
	mb_params.Parent = NULL; 
	mb_params.Format = grctfA8R8G8B8;
#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_LIGHTING;
#endif

	WIN32PC_ONLY(if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Medium))
	{
		for (int i = 0; i < 2; i++)
		{
			fpvMotionBlurTarget[i] = grcTextureFactory::GetInstance().CreateRenderTarget(
				"FPV Motion Blur Targets", grcrtPermanent, iWidth / 2, iHeight / 2, 32, &mb_params WIN32PC_ONLY(, fpvMotionBlurTarget[i]));			
		}
	}

#if USE_NV_TXAA	
	if( PostFX::g_TXAAEnable )
	{
		grcRenderTarget *src = grcTextureFactory::GetInstance().GetBackBuffer();
		grcTextureFactory::CreateParams params;
		params.Multisample = src->GetMSAA();
		params.Format = static_cast<const grcRenderTargetPC*>(src)->GetFormat();

		params.StereoRTMode = grcDevice::STEREO;

		const int w = (VideoResManager::IsSceneScaled()) ? iWidth : src->GetWidth();
		const int h = (VideoResManager::IsSceneScaled()) ? iHeight : src->GetHeight();
		const int bpp = src->GetBitsPerPixel();
		g_TXAAResolve = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "TXAAResolve", grcrtPermanent, w,h, bpp, &params, 0);
	}
#endif // USE_NV_TXAA

}

#endif

static void CreateRenderTargets(int iWidth, int iHeight)
{	
	RAGE_TRACK(grPostFx);
	RAGE_TRACK(grPostFxRenderTargets);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	// set render target descriptions
	grcTextureFactory::CreateParams params;
	params.Multisample = grcDevice::MSAA_None;
	params.HasParent = true;
	params.Parent = NULL; 
	params.UseFloat = true;

	//Initialise targets that can be recreated
	#if BOKEH_SUPPORT
	BokehPoints = NULL;
	BokehNoneRenderedTexture = NULL;
	BokehDepthBlur = NULL;
	BokehDepthBlurHalfRes = NULL;
	BokehPointsTemp = NULL;
	#if COC_SPREAD
	BokehDepthBlurHalfResTemp = NULL;
	#endif
	#endif //BOKEH_SUPPORT
	#if DOF_DIFFUSION
	for(u32 i = 0; i < 16; i++)
	{
		dofDiffusionHorzABC[i] = NULL;
		dofDiffusionHorzX[i] = NULL;
		dofDiffusionHorzY[i] = NULL;
		dofDiffusionVertABC[i] = NULL;
		dofDiffusionVertX[i] = NULL;
		dofDiffusionVertY[i] = NULL;
	}
	#endif //DOF_DIFFUSION
	DepthOfField1 = NULL;
	DepthOfField2 = NULL;

	CreateScreenBasedRenderTargets(iWidth, iHeight);

	grcImage* image = grcImage::Create(32, 32, 1, grcImage::L8, grcImage::STANDARD, 0, 0);
	Assert(image);

	for (u32 y = 0; y < 32; y++)
		for (u32 x = 0; x < 32; x++)
			image->SetPixel(x, y, static_cast<u8>((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 255.0f));

	grcTextureFactory::TextureCreateParams jitterParams(grcTextureFactory::TextureCreateParams::SYSTEM, grcTextureFactory::TextureCreateParams::LINEAR, grcsRead | grcsWrite);
	BANK_ONLY(grcTexture::SetCustomLoadName("Jitter");)
	JitterTexture = rage::grcTextureFactory::GetInstance().Create(image, &jitterParams);
	BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
	Assert(JitterTexture);

	LastSafeRelease(image);
}

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS

static void DeleteScreenBasedTextures()
{
#if __D3D11
	for (u32 i = 0; i < MAX_EXP_TARGETS; i++)
	{
		SafeRelease(CurExposureTex[i]);
	}
#endif

#if USE_NV_TXAA
	SafeRelease(g_TXAAResolve);
#endif // USE_NV_TXAA


}

static void DeleteScreenBasedRenderTargets()
{
	RAGE_TRACK(grPostFx);
	RAGE_TRACK(grPostFxRenderTargets);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

#if RSG_PC
	delete PostFX::CenterReticuleDist; PostFX::CenterReticuleDist = NULL;
#endif
	delete HalfScreen0; HalfScreen0 = NULL;

	DepthOfField0 =	NULL;
	delete DepthOfField1; DepthOfField1 = NULL;
	delete DepthOfField2; DepthOfField2 = NULL;

	SAFE_DELETE(BloomBuffer);
	SAFE_DELETE(BloomBufferUnfiltered);
	DURANGO_ONLY(SAFE_DELETE(BloomBufferHalf0);)
	SAFE_DELETE(BloomBufferHalf1);
	SAFE_DELETE(BloomBufferQuarter0);
	SAFE_DELETE(BloomBufferQuarter1);
	DURANGO_ONLY(SAFE_DELETE(BloomBufferQuarterX);)
	DURANGO_ONLY(SAFE_DELETE(DSBB2);)
	DURANGO_ONLY(SAFE_DELETE(DSBB4);)
	DURANGO_ONLY(SAFE_DELETE(DSBB8);)
	DURANGO_ONLY(SAFE_DELETE(DSBB16);)
	SAFE_DELETE(BloomBufferEighth0);
	SAFE_DELETE(BloomBufferEighth1);
	SAFE_DELETE(BloomBufferSixteenth0);
	SAFE_DELETE(BloomBufferSixteenth1);
	SAFE_DELETE(LensArtefactsFinalBuffer);

	SAFE_DELETE(LensArtefactsBuffers0[LENSARTEFACT_QUARTER]);
	SAFE_DELETE(LensArtefactsBuffers0[LENSARTEFACT_EIGHTH]);
	SAFE_DELETE(LensArtefactsBuffers0[LENSARTEFACT_SIXTEENTH]);
	SAFE_DELETE(LensArtefactsBuffers1[LENSARTEFACT_QUARTER]);
	SAFE_DELETE(LensArtefactsBuffers1[LENSARTEFACT_EIGHTH]);
	SAFE_DELETE(LensArtefactsBuffers1[LENSARTEFACT_SIXTEENTH]);

#if FILM_EFFECT
	delete PreFilmEffect; PreFilmEffect = NULL;
#endif

#if BOKEH_SUPPORT
	if( BokehEnable )
	{
		delete BokehPoints; BokehPoints = NULL;
		DURANGO_ONLY(delete BokehPointsTemp;)  // Shared with Bloom Buffer now
		BokehPointsTemp = NULL;

		if (BokehNoneRenderedTexture) {
			BokehNoneRenderedTexture->Release();
			BokehNoneRenderedTexture = NULL;
		}

		delete BokehDepthBlur; BokehDepthBlur = NULL;
#if RSG_DURANGO && __BANK
		delete AltBokehDepthBlur; AltBokehDepthBlur = NULL;
#endif
		delete BokehDepthBlurHalfRes; BokehDepthBlurHalfRes = NULL;
#if COC_SPREAD
		delete BokehDepthBlurHalfResTemp; BokehDepthBlurHalfResTemp = NULL;
#endif
	}

#endif


#if DOF_DIFFUSION
	for(u32 i = 0; i < 16; i++)
	{
		delete dofDiffusionHorzABC[i]; dofDiffusionHorzABC[i] = NULL;
		delete dofDiffusionHorzX[i]; dofDiffusionHorzX[i] = NULL;
		delete dofDiffusionHorzY[i]; dofDiffusionHorzY[i] = NULL;
		delete dofDiffusionVertABC[i]; dofDiffusionVertABC[i] = NULL;
		delete dofDiffusionVertX[i]; dofDiffusionVertX[i] = NULL;
		delete dofDiffusionVertY[i]; dofDiffusionVertY[i] = NULL;
	}
#endif

	delete HeatHaze0; HeatHaze0 = NULL;
	delete HeatHaze1; HeatHaze1 = NULL;

	delete SSLRCutout; SSLRCutout = NULL;
	delete LRIntensity; LRIntensity = NULL;
	delete LRIntensity2; LRIntensity2 = NULL;
	delete PostFX::FogRayRT; PostFX::FogRayRT = NULL;
	delete PostFX::FogRayRT2; PostFX::FogRayRT2 = NULL;
	
#if AVG_LUMINANCE_COMPUTE
	if (AverageLumunianceComputeEnable)
	{
		for (int i = 0; i <= PostFX::LumCSDownsampleUAVCount; i++)
		{
			SAFE_DELETE(PostFX::LumCSDownsampleUAV[i]);
		}
	}	
#endif //AVG_LUMINANCE_COMPUTE
	for (int i = 0; i < PostFX::LumDownsampleRTCount; i++)
	{
		SAFE_DELETE(PostFX::LumDownsampleRT[i]);
	}

#if (RSG_PC)
	if (!AverageLumunianceComputeEnable)
	{
		for (int i = 0; i < PostFX::ImLumDownsampleRTCount; i++)
		{
			SAFE_DELETE(PostFX::ImLumDownsampleRT[i]);
		}
	}	
#endif

	SAFE_DELETE(PostFX::FogOfWarRT0);
	SAFE_DELETE(PostFX::FogOfWarRT1);
	SAFE_DELETE(PostFX::FogOfWarRT2);

	for (u32 i = 0; i < MAX_EXP_TARGETS; i++)
	{
		SAFE_DELETE(CurExposureRT[i]);
	}
	
	DeleteScreenBasedTextures();
}

#if !RSG_ORBIS	// unused
static void DeleteRenderTargets()
{
	RAGE_TRACK(grPostFx);
	RAGE_TRACK(grPostFxRenderTargets);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	DeleteScreenBasedRenderTargets();

	SafeRelease(JitterTexture);
}
#endif	//!RSG_ORBIS

#endif	// __WIN32PC || RSG_DURANGO || RSG_ORBIS

static void RecreateFogVertexBuffer()
{
	static dev_s32 fogTilesWidth	= 8;
	static dev_s32 fogTilesHeight	= 5;

	s32 verts = ((fogTilesWidth + 1)*2 + 2)*fogTilesHeight;
	float *const waterVertices = reinterpret_cast<float*>(alloca(verts * 2 * sizeof(float)));

	float dX	=  2.0f/fogTilesWidth;
	float dY	= -2.0f/fogTilesHeight;
	float y		= 1.0f;
	s32 index	= 0;

	for(s32 i = 0; i < fogTilesHeight; i++)
	{
		float x	= -1.0f;

		waterVertices[index++] = x;
		waterVertices[index++] = y;

		float y1 = y + dY;

		for(s32 j = 0; j < fogTilesWidth + 1; j++)
		{
			waterVertices[index++] = x;
			waterVertices[index++] = y;
			waterVertices[index++] = x;
			waterVertices[index++] = y1;
			x = x + dX;
		}

		waterVertices[index++] = x - dX;
		waterVertices[index++] = y1;

		y = y1;
	}

	Assertf(index == verts*2, "%d == %d", index, verts*2);
	
	if (sm_FogVertexBuffer)
	{
		delete sm_FogVertexBuffer;
	}

	grcFvf fvf;
	fvf.SetPosChannel(true, grcFvf::grcdsFloat2);

#if RSG_PC
	sm_FogVertexBuffer = grcVertexBuffer::CreateWithData(verts, fvf, grcsBufferCreate_NeitherReadNorWrite, waterVertices, false);
#else
	sm_FogVertexBuffer = grcVertexBuffer::CreateWithData(verts, fvf, waterVertices);
#endif
	
}

static void PostFXBlitHelper(grmShader *const shader, grcEffectTechnique technique, u32 pass,  const Vector2 &uvOffset = Vector2(0.f,0.f), const Vector2 &uvOffsetEnd = Vector2(1.f,1.f))
{
#if POSTFX_UNIT_QUAD
	shader->SetVar(quad::Position,	FastQuad::Pack(-1.f,1.f,1.f,-1.f));
	shader->SetVar(quad::TexCoords,	FastQuad::Pack(uvOffset.x,uvOffset.y,uvOffsetEnd.x,uvOffsetEnd.y));
	shader->SetVar(quad::Scale,		Vector4(1.0f,1.0f,1.0f,1.0f));
# if USE_IMAGE_WATERMARKS
	shader->SetVar(quad::Alpha,		1.0f);
# endif
#endif //POSTFX_UNIT_QUAD

	AssertVerify(shader->BeginDraw(grmShader::RMC_DRAW,true,technique));
	ShmooHandling::BeginShmoo(PostFxShmoo, shader, (int)pass);
	shader->Bind((int)pass);

#if POSTFX_UNIT_QUAD
	FastQuad::Draw(true);
#else
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, uvOffset.x,uvOffset.y,uvOffsetEnd.x,uvOffsetEnd.y, Color32(1.f,1.f,1.f,1.f));
#endif

	shader->UnBind();
	ShmooHandling::EndShmoo(PostFxShmoo);
	shader->EndDraw();
}

#if __D3D11
static void PostFXBlit(grcRenderTarget* target, grcEffectTechnique technique, u32 pass, bool lockDepth=false, bool bClearColour = false, const char** RAGE_TIMEBARS_ONLY(passNameArray) = NULL, bool resolve = true, const Vector2 &uvOffset = Vector2(0.0f,0.0f), const Vector2 &uvOffsetEnd = Vector2(1.0f,1.0f), const grcBufferUAV* UAVs[grcMaxUAVViews] = NULL)
#else
#if RSG_ORBIS
static void PostFXBlit(grcRenderTarget* target, grcEffectTechnique technique, u32 pass, bool lockDepth=false, bool bClearColour = false, const char** RAGE_TIMEBARS_ONLY(passNameArray) = NULL, bool resolve = true, const Vector2 &uvOffset = Vector2(0.0f,0.0f), const Vector2 &uvOffsetEnd = Vector2(1.0f,1.0f), bool bLockSingleTarget = false)
#else
static void PostFXBlit(grcRenderTarget* target, grcEffectTechnique technique, u32 pass, bool lockDepth=false, bool bClearColour = false, const char** RAGE_TIMEBARS_ONLY(passNameArray) = NULL, bool resolve = true, const float uvOffsetScalar = 0.0f)
#endif
#endif
{
#if RAGE_TIMEBARS
	if( passNameArray )
		PF_PUSH_MARKER(passNameArray[pass]);
	else
		PF_PUSH_MARKER(passName[pass]);
#endif // RAGE_TIMEBARS

#if __XENON
	int dstWidth, dstHeight;

	if (target)
	{
		dstWidth=target->GetWidth();
		dstHeight=target->GetHeight();
	}
	else
	{
		dstWidth=VideoResManager::GetNativeWidth();
		dstHeight=VideoResManager::GetNativeHeight();
	}
#endif

#if __D3D11
	grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance()); 
	if( UAVs )
	{
		const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
			target
		};

		UINT initialCounts[grcMaxUAVViews] = { 0 };
		if(lockDepth)
			textureFactory11.LockMRTWithUAV(NULL, target, NULL, NULL, UAVs, initialCounts);
		else
			textureFactory11.LockMRTWithUAV(RenderTargets, NULL, NULL, NULL, UAVs, initialCounts);
	}
	else 
#endif
	if (target)
	{
#if RSG_ORBIS
		if (bLockSingleTarget)
		{
			const grcRenderTargetGNM *const pColor = static_cast<const grcRenderTargetGNM*>(target);
			const sce::Gnm::RenderTarget *const sceColor = pColor->GetColorTarget();
			GRCDEVICE.LockSingleTarget(sceColor);
		}
		else
#endif
		{
			if(lockDepth)
				grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, target);
			else
				grcTextureFactory::GetInstance().LockRenderTarget(0, target, NULL);
		}
	}

#if !(__D3D11 || RSG_ORBIS)
	const Vector2 uvOffset(uvOffsetScalar, uvOffsetScalar);
	const Vector2 uvOffsetEnd(1.0f + uvOffsetScalar, 1.0f + uvOffsetScalar);
#endif
	PostFXBlitHelper(PostFXShader, technique, pass, uvOffset, uvOffsetEnd);

	if (target)
	{
#if RSG_ORBIS
		if (bLockSingleTarget)
		{
			static_cast<grcTextureFactoryGNM&>( grcTextureFactory::GetInstance() ).RelockRenderTargets();
		}
		else
#endif
		{
		grcResolveFlags resolveFlags;
		resolveFlags.ClearColor		= bClearColour;
#if RSG_SM_50
		(void) resolve;
#else
		resolveFlags.NeedResolve	= resolve;
#endif //RSG_SM_50
		grcTextureFactory::GetInstance().UnlockMRT();
		}
	}else
	{
#if RSG_ORBIS && SUPPORT_RENDERTARGET_DUMP
		static_cast<grcTextureFactoryGNM&>(grcTextureFactory::GetInstance()).UnlockBackBuffer();
#endif	//RSG_ORBIS && SUPPORT_RENDERTARGET_DUMP
	}
		
#if RAGE_TIMEBARS
	PF_POP_MARKER();
#endif // RAGE_TIMEBARS
}

static void PostFXBlit(grcRenderTarget* target, grcEffectTechnique technique, u32 pass, bool lockDepth, bool needToClear, Color32 ClearColor, const Vector3 &ll, const Vector3 &lr, const Vector3 &ur,const Vector3 &ul, const char** RAGE_TIMEBARS_ONLY(passNameArray) = NULL)
{
#if RAGE_TIMEBARS
	if( passNameArray )
		PF_PUSH_MARKER(passNameArray[pass]);
	else
		PF_PUSH_MARKER(passName[pass]);
#endif // RAGE_TIMEBARS

	Color32 color(1.0f,1.0f,1.0f,1.0f);

#if __XENON
	int dstWidth, dstHeight;

	if (target)
	{
		dstWidth=target->GetWidth();
		dstHeight=target->GetHeight();
	}
	else
	{
		dstWidth=VideoResManager::GetNativeWidth();
		dstHeight=VideoResManager::GetNativeHeight();	
	}
#endif

	if (target)
	{
		if(lockDepth)
			grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, target);
		else
			grcTextureFactory::GetInstance().LockRenderTarget(0, target, NULL);
	}

	if (needToClear)
		GRCDEVICE.Clear(true, ClearColor, false, 0.0f, false);

	AssertVerify(PostFXShader->BeginDraw(grmShader::RMC_DRAW,true,technique));
	ShmooHandling::BeginShmoo(PostFxShmoo, PostFXShader, (int)pass);
	PostFXShader->Bind((int)pass);

	grcBegin(rage::drawTriStrip, 4);
	grcVertex(ll.x,ll.y,ll.z,0,0,0,color,0.0f,0.0f);
	grcVertex(lr.x,lr.y,lr.z,0,0,0,color,1.0f,0.0f);
	grcVertex(ul.x,ul.y,ul.z,0,0,0,color,0.0f,1.0f);
	grcVertex(ur.x,ur.y,ur.z,0,0,0,color,1.0f,1.0f);
	grcEnd();

	PostFXShader->UnBind();
	ShmooHandling::EndShmoo(PostFxShmoo);
	PostFXShader->EndDraw();

	if (target)
	{
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}

#if RAGE_TIMEBARS
	PF_POP_MARKER();
#endif // RAGE_TIMEBARS

}

void PostFX::SimpleBlitWrapper(grcRenderTarget* pDestTarget, const grcTexture* pSrcTarget, u32 pass)
{
	SetNonDepthFXStateBlocks();

	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(pSrcTarget->GetWidth()),1.0f/float(pSrcTarget->GetHeight()),0.0, 0.0f));
	PostFXShader->SetVar(TextureID_0, pSrcTarget);
	PostFXBlit(pDestTarget, PostFXTechnique, pass, false, false);
}

static void PostFXGradientFilter(grcRenderTarget* target, grcEffectTechnique technique, u32 pass, const PostFX::PostFXParamBlock::paramBlock& settings)
{
	Vec4V vTopColour = RCC_VEC4V(settings.m_lensGradientColTop);
	Vec4V vMidColour = RCC_VEC4V(settings.m_lensGradientColMiddle);
	Vec4V vBotColour = RCC_VEC4V(settings.m_lensGradientColBottom);
	vTopColour.SetW(ScalarV(V_ONE));
	vMidColour.SetW(ScalarV(V_ONE));
	vBotColour.SetW(ScalarV(V_ONE));

	// Check for colours close to white as the time cycle never quite returns one
	unsigned int bAnyColourCloseToWhite =	IsCloseAll(vTopColour, Vec4V(V_ONE), ScalarV(V_FLT_SMALL_2)) & 
											IsCloseAll(vMidColour, Vec4V(V_ONE), ScalarV(V_FLT_SMALL_2)) &
											IsCloseAll(vBotColour, Vec4V(V_ONE), ScalarV(V_FLT_SMALL_2));

	if (bAnyColourCloseToWhite == 0)
	{
		PostFXBlit(target, technique, pass, false, false, NULL, false);
	}
}


#if BOKEH_SUPPORT

static void SortBokehSprites()
{
	//Generate sorted bucket list
	PIXBegin(0, "bokeh sort");
	PostFXShader->SetVarUAV( BokehSortOffsetsBufferVar, &s_bokehNumAddedToBuckets, 0);
	PostFXShader->SetVarUAV( BokehSortedBufferVar, &s_bokehSortedIndexListBuffer, 0);
	PostFXShader->SetVar(BokehSpritePointBufferVar, &s_BokehAccumBuffer);

#if RSG_DURANGO
	g_grcCurrentContext->GpuSendPipelinedEvent(D3D11X_GPU_PIPELINED_EVENT_PS_PARTIAL_FLUSH);
#endif

	// Sort the data
	// First sort the rows for the levels <= to the block size
	for( u32 level = 2 ; level <= BOKEH_SORT_BITONIC_BLOCK_SIZE ; level = level * 2 )
	{
		PostFXShader->SetVar(BokehSortLevelVar, (float)level);
		PostFXShader->SetVar(BokehSortLevelMaskVar, (float)level);

#if BOKEH_SORT_BITONIC_TRANSPOSE
		u32 groupCountX = s_BokehMaxElements / BOKEH_SORT_BITONIC_BLOCK_SIZE;
#else
		u32 groupCountX = BOKEH_SORT_NUM_BUCKETS;
#endif
		u32 groupCountY = 1;
		GRCDEVICE.RunComputation( "Bokeh bitonic sort", *PostFXShader, pp_Bokeh_ComputeBitonicSort, groupCountX, groupCountY, 1);		
# if __D3D11 || RSG_ORBIS
		GRCDEVICE.SynchronizeComputeToCompute();
# endif
	}

#if BOKEH_SORT_BITONIC_TRANSPOSE

	const u32 transposeMatrixWidth = BOKEH_SORT_BITONIC_BLOCK_SIZE;
	const u32 transposeMatrixHeight = s_BokehMaxElements / BOKEH_SORT_BITONIC_BLOCK_SIZE;

	// Then sort the rows and columns for the levels > than the block size
	// Transpose. Sort the Columns. Transpose. Sort the Rows.
	for( u32 level = (BOKEH_SORT_BITONIC_BLOCK_SIZE * 2) ; level <= s_BokehMaxElements ; level = level * 2 )
	{
		PostFXShader->SetVar(BokehSortLevelVar, (float)level / (float)BOKEH_SORT_BITONIC_BLOCK_SIZE);
		PostFXShader->SetVar(BokehSortLevelMaskVar, (float)((level & ~s_BokehMaxElements) / BOKEH_SORT_BITONIC_BLOCK_SIZE));
		PostFXShader->SetVar(BokehSortTransposeMatWidthVar, (float)transposeMatrixWidth);
		PostFXShader->SetVar(BokehSortTransposeMatHeightVar, (float)transposeMatrixHeight);

		// Transpose the data from buffer 1 into buffer 2
		PostFXShader->SetVar( BokehSortedIndexBufferVar, &s_bokehSortedIndexListBuffer);
		PostFXShader->SetVarUAV( BokehSortedBufferVar, &s_bokehSortedIndexListBuffer2, 0);

		u32 groupCountX = transposeMatrixWidth / BOKEH_SORT_TRANSPOSE_BLOCK_SIZE;
		u32 groupCountY = transposeMatrixHeight / BOKEH_SORT_TRANSPOSE_BLOCK_SIZE;
		GRCDEVICE.RunComputation( "Bokeh bitonic sort transpose", *PostFXShader, pp_Bokeh_ComputeBitonicSortTranspose, groupCountX, groupCountY, 1);		

# if __D3D11 || RSG_ORBIS
		GRCDEVICE.SynchronizeComputeToCompute();
# endif

		// Sort the transposed column data
		groupCountX = s_BokehMaxElements / BOKEH_SORT_BITONIC_BLOCK_SIZE;
		groupCountY = 1;
		GRCDEVICE.RunComputation( "Bokeh bitonic sort", *PostFXShader, pp_Bokeh_ComputeBitonicSort, groupCountX, groupCountY, 1);

# if __D3D11 || RSG_ORBIS
		GRCDEVICE.SynchronizeComputeToCompute();
# endif
		PostFXShader->SetVar(BokehSortLevelVar, (float)BOKEH_SORT_BITONIC_BLOCK_SIZE);
		PostFXShader->SetVar(BokehSortLevelMaskVar, (float)level);
		PostFXShader->SetVar(BokehSortTransposeMatWidthVar, (float)transposeMatrixHeight);
		PostFXShader->SetVar(BokehSortTransposeMatHeightVar, (float)transposeMatrixWidth);

		// Transpose the data from buffer 2 back into buffer 1
		PostFXShader->SetVar( BokehSortedIndexBufferVar, &s_bokehSortedIndexListBuffer2);
		PostFXShader->SetVarUAV( BokehSortedBufferVar, &s_bokehSortedIndexListBuffer, 0);
		groupCountX = transposeMatrixHeight / BOKEH_SORT_TRANSPOSE_BLOCK_SIZE;
		groupCountY = transposeMatrixWidth / BOKEH_SORT_TRANSPOSE_BLOCK_SIZE;
		GRCDEVICE.RunComputation( "Bokeh bitonic sort transpose", *PostFXShader, pp_Bokeh_ComputeBitonicSortTranspose, groupCountX, groupCountY, 1);

# if __D3D11 || RSG_ORBIS
		GRCDEVICE.SynchronizeComputeToCompute();
# endif
		// Sort the row data
		groupCountX = s_BokehMaxElements / BOKEH_SORT_BITONIC_BLOCK_SIZE;
		groupCountY = 1;
		GRCDEVICE.RunComputation( "Bokeh bitonic sort", *PostFXShader, pp_Bokeh_ComputeBitonicSort, groupCountX, groupCountY, 1);

# if __D3D11 || RSG_ORBIS
		GRCDEVICE.SynchronizeComputeToCompute();
# endif
	}

#endif //BOKEH_SORT_BITONIC_TRANSPOSE


# if __D3D11 || RSG_ORBIS
	GRCDEVICE.SynchronizeComputeToGraphics();
# endif

	PIXEnd();
}


static void BokehBlitWithGeometryShader(grcRenderTarget* target, grcEffectTechnique technique, u32 pass, bool lockDepth, bool bClearColour, const char** RAGE_TIMEBARS_ONLY(passNameArray))
{
	// Copy the count from the bokeh point AppendStructuredBuffer to our indirect arguments buffer.
	// This lets us issue a draw call with number of vertices == the number of points in
	// the buffer, without having to copy anything back to the CPU.
	GRCDEVICE.CopyStructureCount( &s_indirectArgsBuffer, 0, &s_BokehAccumBuffer );

#if (RSG_DURANGO || RSG_ORBIS)
#if RSG_DURANGO
	u32* pBuff = (u32*) s_indirectArgsBuffer.LockAll();
#else
	u32* pBuff = (u32*) s_indirectArgsBuffer.GetAddress();
#endif
	u32 bokehRenderCount = *pBuff;
#if __BANK
	//Check that the we're not trying to generate more bokeh sprites than the size of the structured buffer.
	//If this happens we'll just be missing bokeh sprites.
	//Just do this on consoles as we can directly access the memory and any asserts will apply to all platforms.
	if( bokehRenderCount >= s_BokehMaxElements )
	{		
		if( s_BokehRenderCountTimer.GetMsTime() > 4000.0f )
		{
			grcWarningf("Bokeh Render count too high, needs reducing");		
			s_BokehRenderCountTimer.Reset();
		}
	}
	else
	{
		s_BokehRenderCountTimer.Reset();
	}
	
	sprintf(bokehCountString, "%d", bokehRenderCount);
#endif
#endif

	BokehGenerated = false;

#if (RSG_DURANGO || RSG_ORBIS)
	if( bokehRenderCount != 0 )
#endif
	{
		SortBokehSprites();

#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	PF_PUSH_TIMEBAR_DETAIL(passNameArray[pass]);
#else
	RAGE_TIMEBARS_ONLY((void)passNameArray;)
#endif // RAGE_TIMEBARS

		//Render bokeh with geometry shader
		Color32 color(0.0f,0.0f,0.0f,0.0f);

		const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
			target
		};

	#if RSG_ORBIS
		grcTextureFactoryGNM& textureFactory	= static_cast<grcTextureFactoryGNM&>(grcTextureFactory::GetInstance()); 
	#else
		grcTextureFactoryDX11& textureFactory	= static_cast<grcTextureFactoryDX11&>(grcTextureFactory::GetInstance()); 
	#endif
		if (target)
		{
			if(lockDepth)
				textureFactory.LockMRT(NULL, target);
			else
				textureFactory.LockMRT(RenderTargets, NULL);
		}

		//Could possibly stop previous pass writing out colour so dont need to do this
		GRCDEVICE.Clear( bClearColour, color, false, false, false, false );


		PostFXShader->SetVar(BokehSpritePointBufferVar, &s_BokehAccumBuffer);	
		PostFXShader->SetVar(BokehSortedIndexBufferVar, &s_bokehSortedIndexListBuffer);
		

		AssertVerify(PostFXShader->BeginDraw(grmShader::RMC_DRAW,true,technique));
		PostFXShader->Bind((int)pass);

		grcStateBlock::SetBlendState(bokehBlendState);

		GRCDEVICE.DrawWithGeometryShader( &s_indirectArgsBuffer );

		PostFXShader->UnBind();
		PostFXShader->EndDraw();		

		if (target)
		{
			grcResolveFlags resolveFlags;
			resolveFlags.ClearColor = true;
			grcTextureFactory::GetInstance().UnlockRenderTarget(0, (bClearColour ? &resolveFlags : NULL));
		}

		grcStateBlock::SetBlendState(nonDepthFXBlendState);

		BokehGenerated = true;

#if RAGE_TIMEBARS
		PF_POP_TIMEBAR_DETAIL();
#endif // RAGE_TIMEBARS
	}
}

#endif //BOKEH_SUPPORT



void GetNormScreenPosFromWorldPos(const Vector3& worldPos, float& x, float& y)
{
	const grcViewport* pVp = gVpMan.GetRenderGameGrcViewport();

	if (pVp == NULL)
	{
		// have 2d position culled
		x = -2.0f;
		y = -2.0f;
		return;
	}

	pVp->Transform((Vec3V&)worldPos, x, y);
	x /= SCREEN_WIDTH;
	y /= SCREEN_HEIGHT;

	// bring to [-1, 1] range
	x = x*2.0f-1.0f;
	y = y*2.0f-1.0f;
}

void GetNoiseParams(const PostFX::PostFXParamBlock::paramBlock& settings, Vector4& noiseParams)
{
	const float noisiness = (true == PostFX::g_noiseOverride) ? PostFX::g_noisinessOverride : settings.m_noise;
	float noiseSizeMult = Lerp(settings.m_noiseSize, 16.0f, 2.0f);

	noiseParams.x = NoiseEffectRandomValues.x;
	noiseParams.y = NoiseEffectRandomValues.y;
	noiseParams.z = noisiness;
	noiseParams.w = noiseSizeMult;
}

//////////////////////////////////////////////////////////////////////////
//
void PostFX::BulletImpactOverlay::Update()
{
	PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetUpdateThreadParams();

	// reset all the entries
	settings.m_damageEnabled[0] = false;
	settings.m_damageEnabled[1] = false;
	settings.m_damageEnabled[2] = false;
	settings.m_damageEnabled[3] = false;
	settings.m_drawDamageOverlayAfterHud = m_drawAfterHud || CNewHud::IsSniperSightActive();
	settings.m_damageEndurance[0] = false;
	settings.m_damageEndurance[1] = false;
	settings.m_damageEndurance[2] = false;
	settings.m_damageEndurance[3] = false;

	if (IsAnyUpdating() == false || m_disabled || CPhoneMgr::CamGetState())
	{
		return;
	}

	const PostFX::BulletImpactOverlay::Settings& dmgSettings = settings.m_fpvMotionBlurActive ? g_bulletImpactOverlay.ms_settings[1] : g_bulletImpactOverlay.ms_settings[0];

	// cache player position
	Vector3 playerPos = CGameWorld::FindLocalPlayerCoors();
	const float playerPosVerticalBias = 0.1f;
	playerPos.z += 	playerPosVerticalBias;
	settings.m_damagePlayerPos.Set(playerPos);

	for (int i = 0; i < NUM_ENTRIES; i++)
	{
		Entry* pEntry = &m_entries[i];

		// update
		UpdateTiming(pEntry, dmgSettings);

		// skip if it's just finished or if sniper sight is up
		if (pEntry->m_startTime	== 0)
		{
			continue;
		}

		// enable bullet impact
		settings.m_damageFrozen[i] = pEntry->m_bFrozen;
		settings.m_damageEnabled[i] = true;
		settings.m_damageFiringPos[i].Set(pEntry->m_firingWeaponPos);
		settings.m_damageFiringPos[i].w = pEntry->m_fFadeLevel;
		settings.m_damageEndurance[i] = pEntry->m_bIsEnduranceDamage;

		// Freeze after first update
		pEntry->m_bFrozen = true;
	}
}

static bool PostFXSegmentSegmentIntersection(const Vector2& A0, const Vector2& A1, const Vector2& B0, const Vector2& B1, Vector2& intPt)
{
	float  s, t;
	float num, denom;

	denom = A0.x*(B1.y-B0.y) + A1.x*(B0.y-B1.y) + B1.x*(A1.y-A0.y) + B0.x*(A0.y-A1.y);
	if (denom == 0.0f)
		return false;

	num = A0.x*(B1.y-B0.y) + B0.x*(A0.y-B1.y) + B1.x*(B0.y-A0.y);
	s = num/denom;

	num = -(A0.x * (B0.y-A1.y) + A1.x*(A0.y-B0.y) + B0.x*(A1.y-A0.y));
	t = num / denom;

	bool bInt = false;

	if ((0.0 < s) && (s < 1.0) && (0.0 < t) && (t < 1.0))
		bInt = true;
	
	intPt.x = A0.x + s*(A1.x-A0.x);
	intPt.y = A0.y + s*(A1.y-A0.y);

	return bInt;
}

#define POSTFX_DAMAGEOVERLAY_DEBUG (__BANK && 1)
#if POSTFX_DAMAGEOVERLAY_DEBUG
static void DamageOverlayDebugRender(Vector2& playerScreenPos, Vector2& firingScreenPos, Vector2& hitScreenDir, Vector2& screenEdgeIntPt, Vector2& screenEdgeTest1, float baseOffscrenBiasMult)
{
	if (PostFX::g_bulletImpactOverlay.m_bEnableDebugRender)
	{
		Vector2 textPos(0.1f, 0.1f);
		char pName[256];

		const Color32 intersectionCol = Color32(0xffff0000);
		const Color32 playerPosCol = Color32(0xff00ff00);
		const Color32 firingPosCol = Color32(0xff0000ff);

		// Print player pos
		sprintf(&pName[0],"PLAYER POS: %2.2f, %2.2f", playerScreenPos.x, playerScreenPos.y);
		grcDebugDraw::Text(textPos, playerPosCol, pName, true);
		textPos.y += 0.02f;
		// Print firing pos
		sprintf(&pName[0],"FIRING POS: %2.2f, %2.2f", firingScreenPos.x, firingScreenPos.y);
		grcDebugDraw::Text(textPos, firingPosCol, pName, true);
		textPos.y += 0.02f;
		// Print intersection point
		sprintf(&pName[0],"INT PT: %2.2f, %2.2f", screenEdgeIntPt.x, screenEdgeIntPt.y);
		grcDebugDraw::Text(textPos, intersectionCol, pName, true);
		textPos.y += 0.02f;
		// Print screenEdgeTest1
		sprintf(&pName[0],"EDGETEST PT: %2.2f, %2.2f", screenEdgeTest1.x,  screenEdgeTest1.y);
		grcDebugDraw::Text(textPos, Color32(255,0,0,255), pName, true);
		textPos.y += 0.02f;
		// Print hit direction
		sprintf(&pName[0],"HIT DIR: %2.2f, %2.2f", hitScreenDir.x,  hitScreenDir.y);
		grcDebugDraw::Text(textPos, Color32(255,255,255,255), pName, true);
		textPos.y += 0.02f;

		// Print dot product
		sprintf(&pName[0],"O/S MULT: %2.2f", baseOffscrenBiasMult);
		grcDebugDraw::Text(textPos, Color32(255,255,255,255), pName, true);

		// Draw intersection point
		grcDebugDraw::Circle(Vector2(screenEdgeIntPt.x*0.5f + 0.5f, (screenEdgeIntPt.y*0.5f + 0.5f)), 0.05f, intersectionCol);
		// Draw player pos
		grcDebugDraw::Circle(Vector2(playerScreenPos.x*0.5f + 0.5f, (playerScreenPos.y*0.5f + 0.5f)), 0.05f, playerPosCol);
		// Draw firing pos
		grcDebugDraw::Circle(Vector2(firingScreenPos.x*0.5f + 0.5f, (firingScreenPos.y*0.5f + 0.5f)), 0.05f, firingPosCol);
		// Draw screenEdgeTest1
		grcDebugDraw::Circle(Vector2(screenEdgeTest1.x*0.5f + 0.5f, (screenEdgeTest1.y*0.5f + 0.5f)), 0.05f, firingPosCol);
	}
}
#endif 

static void PostFXDamageOverlay(grcRenderTarget* target, grcEffectTechnique technique, u32 pass, const PostFX::PostFXParamBlock::paramBlock& settings)
{
	const PostFX::BulletImpactOverlay::Settings& dmgSettings = settings.m_fpvMotionBlurActive ? PostFX::g_bulletImpactOverlay.ms_settings[1] : PostFX::g_bulletImpactOverlay.ms_settings[0];

#if RAGE_TIMEBARS
	PF_PUSH_MARKER(passName[pass]);
#endif // RAGE_TIMEBARS
	if (PostFX::g_pDamageOverlayTexture == NULL)
		return;

	if (target)
	{
		grcTextureFactory::GetInstance().LockRenderTarget(0, target, NULL);
	}

	const float aspectRatio = CHudTools::GetAspectRatio();
	const grcViewport* pPrevVP = grcViewport::GetCurrent();
	grcViewport damageOverlayVP;
	damageOverlayVP.Ortho(-aspectRatio, aspectRatio, 1.0f, -1.0f, 0.0f, 1.0f);

#if SUPPORT_MULTI_MONITOR
	if(GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		damageOverlayVP.SetWindow(mon.uGridPosX * mon.getWidth(), mon.uGridPosY * mon.getHeight(), mon.getWidth(), mon.getHeight());
	}
#endif // SUPPORT_MULTI_MONITOR
	grcViewport::SetCurrent(&damageOverlayVP);

	// Set shader constants
	Vector4 damageOverlayMiscParams;
	damageOverlayMiscParams.Zero();
	PostFXShader->SetVar(DamageOverlayMiscId, damageOverlayMiscParams);
	PostFXShader->SetVar(TextureID_1, PostFX::g_pDamageOverlayTexture);
	
	// Bind the shader
	// no shmoo for this - not fullscreen
	AssertVerify(PostFXShader->BeginDraw(grmShader::RMC_DRAW,true,technique));
	PostFXShader->Bind((int)pass);

	GRCDEVICE.SetVertexDeclaration(PostFX::g_damageSliceDecl); 

	const Vector2 L0 = Vector2(-1.0f, -1.0f);
	const Vector2 L1 = Vector2(-1.0f, 1.0f);
	const Vector2 R0 = Vector2(1.0f, -1.0f);
	const Vector2 R1 = Vector2(1.0f, 1.0f);
	const Vector2 T0 = Vector2(-1.0f, -1.0f);
	const Vector2 T1 = Vector2(1.0f, -1.0f);
	const Vector2 B0 = Vector2(-1.0f, 1.0f);
	const Vector2 B1 = Vector2(1.0f, 1.0f);

	Vector3 playeWorldPos = Vector3(settings.m_damagePlayerPos.x, settings.m_damagePlayerPos.y, settings.m_damagePlayerPos.z);

	Vector2 playerScreenPos;
	GetNormScreenPosFromWorldPos(playeWorldPos, playerScreenPos.x, playerScreenPos.y);

	// If player is partially or fully off-screen or outside the predefined "safe zone", 
	// fall back to using the head position (should almost always be on screen). 
	// If that fails, fall back to using the bottom-centre of the screen (0.0, 0.5).
	const float screenSafeZoneLength = PostFX::BulletImpactOverlay::ms_screenSafeZoneLength;
	if (Abs(playerScreenPos.x) > screenSafeZoneLength || Abs(playerScreenPos.y) > screenSafeZoneLength)
	{
		TUNE_GROUP_BOOL(POST_FX, bEnableDamageOverlayPositionFix, true);
		if (bEnableDamageOverlayPositionFix)
		{
			bool bValidHeadPosition = false;
			if (!camInterface::GetGameplayDirector().GetFirstPersonShooterCamera())
			{
				const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
				if (pLocalPlayer)
				{
					Vector3 headPosition = pLocalPlayer->GetBonePositionCached(BONETAG_HEAD);
					Vector2 headScreenPos;
					GetNormScreenPosFromWorldPos(headPosition, headScreenPos.x, headScreenPos.y);
					if (Abs(headScreenPos.x) <= screenSafeZoneLength && Abs(headScreenPos.y) <= screenSafeZoneLength)
					{
						bValidHeadPosition = true;
						playeWorldPos = headPosition;
						playerScreenPos = headScreenPos;
					}
				}
			}

			TUNE_GROUP_BOOL(POST_FX, bForceUseCameraPositionForDamageOverlay, false);
			if (!bValidHeadPosition || bForceUseCameraPositionForDamageOverlay)
			{
				// Head position is invalid; use the bottom-centre of the screen as the player position.
				// Push camera position forwards slightly to ensure it's "on-screen". Fixes issues with setting firingWorldPos below (assumes player pos is on-screen).
				const grcViewport* pVp = gVpMan.GetRenderGameGrcViewport();
				float fNearClipDist = pVp ? pVp->GetNearClip() : 0.1f;
				Vector3 camPos = camInterface::GetPos() + (camInterface::GetFront() * fNearClipDist);
				playeWorldPos = camPos;
				playerScreenPos.Zero();
			}
		}
		else
		{
			Vector3 camPos = camInterface::GetPos();
			playeWorldPos = camPos;
			playerScreenPos.Zero();
		}
	}


	static Vector3	ms_colourTop;
	static Vector3	ms_colourBottom;

	float globalAlphaBottom = dmgSettings.globalAlphaBottom;
	float globalAlphaTop = dmgSettings.globalAlphaTop;

	if (NetworkInterface::IsGameInProgress())
	{
		globalAlphaBottom += globalAlphaBottom*0.20f;
		globalAlphaTop += globalAlphaTop*0.20f;

		globalAlphaBottom = Saturate(globalAlphaBottom);
		globalAlphaTop = Saturate(globalAlphaTop);
	}

	Color32 colBot(0, 0, 0);
	Color32 colTop(0, 0, 0);

	for (int i = 0; i < PostFX::BulletImpactOverlay::GetNumEntries(); i++)
	{
		if (settings.m_damageEnabled[i] == false)
			continue;

		if (settings.m_damageEndurance[i] == true)
		{
			colBot = Color32(dmgSettings.colourBottomEndurance.x, dmgSettings.colourBottomEndurance.y, dmgSettings.colourBottomEndurance.z, globalAlphaBottom);
			colTop = Color32(dmgSettings.colourTopEndurance.x, dmgSettings.colourTopEndurance.y, dmgSettings.colourTopEndurance.z, globalAlphaTop);
		}
		else
		{
			colBot = Color32(dmgSettings.colourBottom.x, dmgSettings.colourBottom.y, dmgSettings.colourBottom.z, globalAlphaBottom);
			colTop= Color32(dmgSettings.colourTop.x, dmgSettings.colourTop.y, dmgSettings.colourTop.z, globalAlphaTop);
		}

		// Take into account current entry's alpha
		colBot.SetAlpha(((u8)(colBot.GetAlphaf()*settings.m_damageFiringPos[i].w*255.0f)));
		colTop.SetAlpha(((u8)(colTop.GetAlphaf()*settings.m_damageFiringPos[i].w*255.0f)));

		// Adjust firing world position to ensure it's within screen bounds (assuming the player ped is!)
		Vector3 firingWorldPos = Vector3(settings.m_damageFiringPos[i].x, settings.m_damageFiringPos[i].y, settings.m_damageFiringPos[i].z);
		Vector3 hitWorldDir =  playeWorldPos-firingWorldPos;

		if (hitWorldDir.FiniteElements() == false)
		{
			continue;
		}

		hitWorldDir.NormalizeSafe();
		firingWorldPos = playeWorldPos - hitWorldDir*0.1f;

		// Get firing position in screen space
		Vector2 firingScreenPos;
		GetNormScreenPosFromWorldPos(firingWorldPos, firingScreenPos.x, firingScreenPos.y);

		// Compute direction vector in screen space
		Vector2 hitScreenDir = playerScreenPos-firingScreenPos;
		hitScreenDir.NormalizeSafe();

		// If the firing position is within the screen bounds we won't get an intersection, 
		// so always push it further away enough to make sure it's off-screen.
		Vector2 screenEdgeTest0 = playerScreenPos;
		Vector2 screenEdgeTest1 = firingScreenPos - (hitScreenDir*10.0f);
		Vector2 screenEdgeIntPt;

		bool bIntFound = false;
		bool bVerticalEdge = false;

		if (!bIntFound)
		{
			bIntFound = PostFXSegmentSegmentIntersection(T0, T1, screenEdgeTest0, screenEdgeTest1, screenEdgeIntPt);
		}
		if (!bIntFound)
		{
			bIntFound = PostFXSegmentSegmentIntersection(B0, B1, screenEdgeTest0, screenEdgeTest1, screenEdgeIntPt);
		}
		if (!bIntFound)
		{
			bIntFound = PostFXSegmentSegmentIntersection(L0, L1, screenEdgeTest0, screenEdgeTest1, screenEdgeIntPt);
			bVerticalEdge = bIntFound;
		}
		if (!bIntFound)
		{
			bIntFound = PostFXSegmentSegmentIntersection(R0, R1, screenEdgeTest0, screenEdgeTest1, screenEdgeIntPt);
			bVerticalEdge = bIntFound;
		}

		const u32 vertexColTop = colTop.GetDeviceColor();
		const u32 vertexColBot = colBot.GetDeviceColor();

		// Derive vectors for sprite
		Vector3 spritePos(screenEdgeIntPt.x, -screenEdgeIntPt.y, 0.0f);
		Vector3 spriteForward(-hitScreenDir.x, hitScreenDir.y, 0.0f);
		Vector3 spriteSide;
		Vector3 spriteUp = Vector3(0.0f, 0.0f, -1.0f);
		spriteSide.Cross(spriteUp, spriteForward);
		spriteSide.NormalizeSafe();

		// Scale to length
		const float spriteLength =  dmgSettings.spriteLength;
		const float spriteBaseWidth =  dmgSettings.spriteBaseWidth;
		const float spriteTipWidth =  dmgSettings.spriteTipWidth;
		spriteForward *= spriteLength;

		// If current sprite is frozen, used previously cached data
		if (settings.m_damageFrozen[i])
		{
			spritePos.x		= PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[i].x;
			spritePos.y		= PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[i].y;
			spriteForward.x = PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[i].z;
			spriteForward.y = PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[i].w;
			spriteSide.Cross(spriteUp, spriteForward);
			spriteSide.NormalizeSafe();
		}
		else
		{
			// Cache sprite data for next frames
			PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[i].x = spritePos.x;	
			PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[i].y = spritePos.y;	
			PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[i].z = spriteForward.x; 
			PostFX::BulletImpactOverlay::ms_frozenDamagePosDir[i].w = spriteForward.y;
		}

		// Compute vertices
		Vector3 verts[6];
		verts[0] = spritePos - (spriteSide*spriteBaseWidth);
		verts[1] = spritePos + (spriteSide*spriteBaseWidth);
		verts[2] = spritePos + (spriteSide*spriteTipWidth) - spriteForward;
		verts[3] = spritePos - (spriteSide*spriteTipWidth) - spriteForward;
		
		// verts for extra quad to avoid clipping
		verts[4] = spritePos + (spriteSide*spriteTipWidth) + spriteForward*10.0f;
		verts[5] = spritePos - (spriteSide*spriteTipWidth) + spriteForward*10.0f;

		// Compute UVs for projective interpolation
		const Vector2 v0 = Vector2(verts[0].x, verts[0].y);
		const Vector2 v1 = Vector2(verts[1].x, verts[1].y);
		const Vector2 v2 = Vector2(verts[2].x, verts[2].y);
		const Vector2 v3 = Vector2(verts[3].x, verts[3].y);
		Vector2 int02_31;
		PostFXSegmentSegmentIntersection(v0, v2, v3, v1, int02_31);

		float dist[4];
		dist[0] = (int02_31-Vector2(verts[0].x, verts[0].y)).Mag();
		dist[1] = (int02_31-Vector2(verts[1].x, verts[1].y)).Mag();
		dist[2] = (int02_31-Vector2(verts[2].x, verts[2].y)).Mag(); 
		dist[3] = (int02_31-Vector2(verts[3].x, verts[3].y)).Mag(); 
		float q[4];
		q[0] = (dist[0]+dist[2])/dist[2];
		q[1] = (dist[1]+dist[3])/dist[3];
		q[2] = (dist[2]+dist[0])/dist[0];
		q[3] = (dist[3]+dist[1])/dist[1];

		// Force vertices from the sprite base slightly out of screen
		float baseOffscrenBiasMult = dmgSettings.angleScalingMult*Abs(Dot(hitScreenDir, bVerticalEdge ? Vector2(0.0f, 1.0f) : Vector2(1.0f, 0.0f)));

		if (settings.m_damageFrozen[i])
			baseOffscrenBiasMult = PostFX::BulletImpactOverlay::ms_frozenDamageOffsetMult[i];
		else
			PostFX::BulletImpactOverlay::ms_frozenDamageOffsetMult[i] = baseOffscrenBiasMult;

		Vector3 dir30 = verts[0]-verts[3];
		Vector3 dir21 = verts[1]-verts[2];
		verts[0] += dir30*baseOffscrenBiasMult;
		verts[1] += dir21*baseOffscrenBiasMult;


		grcDrawMode drawMode = drawQuads;
		u32 numVerts = 8;
#if !API_QUADS_SUPPORT
		drawMode = drawTriStrip;
		numVerts = 10;
#endif
		PostFX::grcSliceVertex *rVerts = (PostFX::grcSliceVertex*) GRCDEVICE.BeginVertices(drawMode, numVerts, sizeof(PostFX::grcSliceVertex)); 
			int vertexIndex = 0;

			Vector3 uv0 = Vector3(0.0f, 0.0f, 1.0f)*q[0];
			Vector3 uv1 = Vector3(1.0f, 0.0f, 1.0f)*q[1];
			Vector3 uv2 = Vector3(1.0f, 1.0f, 1.0f)*q[2];
			Vector3 uv3 = Vector3(0.0f, 1.0f, 1.0f)*q[3];

#if API_QUADS_SUPPORT
			rVerts[vertexIndex++].Set(verts[0].x, verts[0].y, 0.0f, vertexColBot, uv0.x, uv0.y, uv0.z); // 0 
			rVerts[vertexIndex++].Set(verts[1].x, verts[1].y, 0.0f, vertexColBot, uv1.x, uv1.y, uv1.z); // 1

			rVerts[vertexIndex++].Set(verts[2].x, verts[2].y, 0.0f, vertexColTop, uv2.x, uv2.y, uv2.z); // 2
			rVerts[vertexIndex++].Set(verts[3].x, verts[3].y, 0.0f, vertexColTop, uv3.x, uv3.y, uv3.z); // 4

			// avoid clipping
			rVerts[vertexIndex++].Set(verts[0].x, verts[0].y, 0.0f, vertexColBot, uv0.x, uv0.y, uv0.z); // 0 
			rVerts[vertexIndex++].Set(verts[1].x, verts[1].y, 0.0f, vertexColBot, uv1.x, uv1.y, uv1.z); // 1
			rVerts[vertexIndex++].Set(verts[4].x, verts[4].y, 0.0f, vertexColBot, uv0.x, uv0.y, uv0.z); // 0 
			rVerts[vertexIndex++].Set(verts[5].x, verts[5].y, 0.0f, vertexColBot, uv1.x, uv1.y, uv1.z); // 1
#else
			rVerts[vertexIndex++].Set(verts[0].x, verts[0].y, 0.0f, vertexColBot, uv0.x, uv0.y, uv0.z); // 0 
			rVerts[vertexIndex++].Set(verts[1].x, verts[1].y, 0.0f, vertexColBot, uv1.x, uv1.y, uv1.z); // 1

			rVerts[vertexIndex++].Set(verts[3].x, verts[3].y, 0.0f, vertexColTop, uv3.x, uv3.y, uv3.z); // 4
			rVerts[vertexIndex++].Set(verts[2].x, verts[2].y, 0.0f, vertexColTop, uv2.x, uv2.y, uv2.z); // 2

			//degenerates
			rVerts[vertexIndex++].Set(verts[2].x, verts[2].y, 0.0f, vertexColTop, uv2.x, uv2.y, uv2.z); // 2
			rVerts[vertexIndex++].Set(verts[0].x, verts[0].y, 0.0f, vertexColBot, uv0.x, uv0.y, uv0.z); // 0 

			// avoid clipping
			rVerts[vertexIndex++].Set(verts[0].x, verts[0].y, 0.0f, vertexColBot, uv0.x, uv0.y, uv0.z); // 0 
			rVerts[vertexIndex++].Set(verts[1].x, verts[1].y, 0.0f, vertexColBot, uv1.x, uv1.y, uv1.z); // 1

			rVerts[vertexIndex++].Set(verts[5].x, verts[5].y, 0.0f, vertexColBot, uv1.x, uv1.y, uv1.z); // 1
			rVerts[vertexIndex++].Set(verts[4].x, verts[4].y, 0.0f, vertexColBot, uv0.x, uv0.y, uv0.z); // 0
#endif

		GRCDEVICE.EndVertices(rVerts+numVerts);

	#if POSTFX_DAMAGEOVERLAY_DEBUG
		DamageOverlayDebugRender(playerScreenPos, firingScreenPos, hitScreenDir, screenEdgeIntPt, screenEdgeTest1, baseOffscrenBiasMult);
	#endif
	}

	// unbind
	PostFXShader->UnBind();
	PostFXShader->EndDraw();

	// unlock, no resolve 
	if (target)
	{
		grcResolveFlags resolveFlags;
		resolveFlags.NeedResolve	= false;
		resolveFlags.ClearColor = false;
		resolveFlags.ClearDepthStencil = false;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	}

	grcViewport::SetCurrent(pPrevVP);

#if RAGE_TIMEBARS
	PF_POP_MARKER();
#endif // RAGE_TIMEBARS
}

float FilmicTonemap(const float inputValue, const Vector4& srcFilmic0, const Vector4& srcFilmic1) 
{
	float A = srcFilmic0.x;
	float B = srcFilmic0.y;
	float C = srcFilmic0.z;
	float D = srcFilmic0.w;
	float E = srcFilmic1.x;
	float F = srcFilmic1.y;

	return (((inputValue*(A*inputValue+C*B)+D*E)/(inputValue*(A*inputValue+B)+D*F))-E/F);
}

float PostFX::FilmicTweakedTonemap(const float inputValue)
{
	const PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetRenderThreadParams();
	return FilmicTonemap(inputValue, settings.m_filmicParams[0], settings.m_filmicParams[1]);
}

void PostFX::GetFilmicParams(Vector4& filmic0, Vector4& filmic1)
{
	const PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetRenderThreadParams();

	// source terms
	Vector4 srcFilmic0 = settings.m_filmicParams[0];
	Vector4 srcFilmic1 = settings.m_filmicParams[1];

	// remap filmicId[0]
	Vector4 filmicParams0 = srcFilmic0;
	filmicParams0.z = 1.0f / FilmicTonemap(settings.m_filmicParams[1].GetZ(), settings.m_filmicParams[0], settings.m_filmicParams[1]);
	filmicParams0.w = 1.0f / settings.m_filmicParams[1].GetZ(); // one over white point

	// remap filmicId[1]
	Vector4 filmicParams1;
	filmicParams1.x = srcFilmic0.z*srcFilmic0.y; // C_mul_B
	filmicParams1.y = srcFilmic0.w*srcFilmic1.x; // D_mul_E
	filmicParams1.z = srcFilmic0.w*srcFilmic1.y; // D_mul_F
	filmicParams1.w = srcFilmic1.x/srcFilmic1.y; // E_div_F

	filmic0 		= filmicParams0;
	filmic1 		= filmicParams1;
}

void PostFX::GetDefaultFilmicParams(Vector4& filmic0, Vector4& filmic1)
{
	// source terms
	const Vector4 srcFilmic0(	GetTonemapParam(TONEMAP_FILMIC_A_BRIGHT),
								GetTonemapParam(TONEMAP_FILMIC_B_BRIGHT),
								GetTonemapParam(TONEMAP_FILMIC_C_BRIGHT),
								GetTonemapParam(TONEMAP_FILMIC_D_BRIGHT));

	const Vector4 srcFilmic1(	GetTonemapParam(TONEMAP_FILMIC_E_BRIGHT),
								GetTonemapParam(TONEMAP_FILMIC_F_BRIGHT),
								GetTonemapParam(TONEMAP_FILMIC_W_BRIGHT),
								0.0f );

	// remap filmicId[0]
	Vector4 filmicParams0 = srcFilmic0;
	filmicParams0.z = 1.0f / FilmicTonemap(srcFilmic1.GetZ(), srcFilmic0, srcFilmic1);
	filmicParams0.w = 1.0f / srcFilmic1.GetZ(); // one over white point

	// remap filmicId[1]
	Vector4 filmicParams1;
	filmicParams1.x = srcFilmic0.z*srcFilmic0.y; // C_mul_B
	filmicParams1.y = srcFilmic0.w*srcFilmic1.x; // D_mul_E
	filmicParams1.z = srcFilmic0.w*srcFilmic1.y; // D_mul_F
	filmicParams1.w = srcFilmic1.x/srcFilmic1.y; // E_div_F

	filmic0 		= filmicParams0;
	filmic1 		= filmicParams1;
}

void PostFX::SetFilmicParams()
{
	Vector4 filmicParams0, filmicParams1;

	GetFilmicParams(filmicParams0, filmicParams1);

	PostFXShader->SetVar(FilmicId[0], filmicParams0);
	PostFXShader->SetVar(FilmicId[1], filmicParams1);

	const PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetRenderThreadParams();	

	PostFXShader->SetVar(darkTonemapParams[0], settings.m_darkTonemapParams[0]);
	PostFXShader->SetVar(darkTonemapParams[1], settings.m_darkTonemapParams[1]);

	PostFXShader->SetVar(brightTonemapParams[0], settings.m_brightTonemapParams[0]);
	PostFXShader->SetVar(brightTonemapParams[1], settings.m_brightTonemapParams[1]);

	PostFXShader->SetVar(tonemapParams, settings.m_tonemapParams);
}

void PostFX::DrawBulletImpactOverlaysAfterHud()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	const PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetRenderThreadParams();

	bool bDrawDamageOverlay = settings.m_drawDamageOverlayAfterHud && (settings.m_damageEnabled[0] || settings.m_damageEnabled[1] || settings.m_damageEnabled[2] || settings.m_damageEnabled[3]);
	if (bDrawDamageOverlay)
	{
		grcBlendStateHandle prevBS = grcStateBlock::BS_Active;
		grcDepthStencilStateHandle prevDDS = grcStateBlock::DSS_Active;
		grcRasterizerStateHandle prevRS = grcStateBlock::RS_Active;

		SetNonDepthFXStateBlocks();
		PostFXDamageOverlay(NULL,  PostFXTechnique, pp_DamageOverlay, settings);

		grcStateBlock::SetStates(prevRS, prevDDS, prevBS);
	}
}

#if __BANK
void PostFX::SetDebugParams()
{
	for (int i = 0; i < NUM_DEBUG_PARAMS; i++)
	{
		PostFXShader->SetVar(DebugParamsId[i], DebugParams[i]);
	}
}
#endif

void PostFX::SetBloomParams()
{
	const PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetRenderThreadParams();

	float ooMaxMinusMin = 1.0f;
	float threshold = settings.m_bloomThresholdMax;

	if (settings.m_bloomThresholdWidth > 0.0f)
	{
		const float maxThreshold = Max<float>(0.0f, settings.m_bloomThresholdMax + (settings.m_bloomThresholdWidth * 0.5f));
		const float minThreshold = Max<float>(0.0f, settings.m_bloomThresholdMax - (settings.m_bloomThresholdWidth * 0.5f));

		ooMaxMinusMin = 1.0f / (maxThreshold - minThreshold);
		threshold = minThreshold;
	}

	Vector4 BloomParams = Vector4(
		threshold, 
		settings.m_bloomIntensity, 
		ooMaxMinusMin, 
		0.0f);

	#if __BANK //for screenshots
	if (NoBloom==true) 
	{
		BloomParams.y = 0.0f;
	}
	#endif

	PostFXShader->SetVar(BloomParamsId, BloomParams);
}

void PostFX::SetExposureParams()
{
	const float timeStep = fwTimer::GetTimeStep();

	const PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetRenderThreadParams();

	Vector4 e0 = Vector4(settings.m_exposureCurveA,      settings.m_exposureCurveB,    settings.m_exposureCurveOffset,            settings.m_exposureTweak);
	Vector4 e1 = Vector4(settings.m_exposureMin,         settings.m_exposureMax,       settings.m_exposurePush,					  timeStep);
	Vector4 e2 = Vector4(0.0f,							 settings.m_averageExposure,   settings.m_averageTimestep,			      settings.m_adaptionStepMult);
	Vector4 e3 = Vector4(settings.m_adaptionMaxStepSize, settings.m_adaptionThreshold, settings.m_adaptionMinStepSize,            BANK_SWITCH(g_overwrittenExposure, 0.0f));

	PostFXShader->SetVar(TextureID_v0, LumRT);
	PostFXShader->SetVar(TextureID_v1, g_prevExposureRT);

	PostFXShader->SetVar(ExposureParams0Id, e0);
	PostFXShader->SetVar(ExposureParams1Id, e1);
	PostFXShader->SetVar(ExposureParams2Id, e2);
	PostFXShader->SetVar(ExposureParams3Id, e3);
}

#if RSG_PC

void PostFX::DeviceLost()
{
}

void PostFX::DeviceReset()
{
	if (targetsInitialized)
	{
		SetGaussianBlur();
		CreateScreenBasedRenderTargets(VideoResManager::GetSceneWidth(), VideoResManager::GetSceneHeight());
		g_resetAdaptedLumCount = NUM_RESET_FRAMES;
	}
}
#endif

void PostFX::ResetMSAAShaders()
{
	delete PostFXShader;
	InitMSAAShaders();
}

void PostFX::InitMSAAShaders()
{
	ASSET.PushFolder("common:/shaders");

	PostFXShader = grmShaderFactory::GetInstance().Create();

	PostFXShader->Load(
#if DEVICE_MSAA
		GRCDEVICE.GetMSAA()>1 ? "postfxMS" :
#endif
		"postfx");

	GENSHMOO_ONLY(PostFxShmoo = )ShmooHandling::Register("postfx", PostFXShader, true, 0.05f);

	PostFXTechnique = PostFXShader->LookupTechnique("PostFx");	
	CloudDepthTechGroup = grmShader::FindTechniqueGroupId("clouddepth");

#if MSAA_EDGE_PASS
	if (DeferredLighting::IsEdgePassEnabled())
	{
		PostFXShaderMS0 = grmShaderFactory::GetInstance().Create();
		PostFXShaderMS0->Load("postfxMS0");
	}
#endif //MSAA_EDGE_PASS

	// Get variable handles
	// Miscs

	// handle for the variable that provides the size of one texel
	TexelSizeId = PostFXShader->LookupVar("TexelSize");

	GBufferTexture0ParamId = PostFXShader->LookupVar("GBufferTexture0Param");

#if __BANK
	for (int i = 0; i < NUM_DEBUG_PARAMS; i++)
	{
		DebugParams[i] = Vector4(0.0, 0.0, 0.0, 0.0);

		char name[16];
		sprintf(name, "debugParams%d", i);
		DebugParamsId[i] = PostFXShader->LookupVar(name, false);
	}
#endif

	// DOF
	DofProjId		= PostFXShader->LookupVar("DOF_PROJ");
	DofShearId		= PostFXShader->LookupVar("DOF_SHEAR");
	DofParamsId		= PostFXShader->LookupVar("DOF_PARAMS"); 
	HiDofParamsId	= PostFXShader->LookupVar("HI_DOF_PARAMS");
	HiDofSmallBlurId = PostFXShader->LookupVar("hiDofUseSmallBlurOnly");
	HiDofMiscParamsId = PostFXShader->LookupVar("hiDofMiscParams");

#if BOKEH_SUPPORT
#if RSG_PC
	if ((GRCDEVICE.GetDxFeatureLevel() >= 1100)  NOTFINAL_ONLY(&& !PARAM_disableBokeh.Get()))
#else
	if( BokehEnable )
#endif
	{
		PostFXBokehTechnique = PostFXShader->LookupTechnique("PostFx_Bokeh");

		BokehBrightnessParams = PostFXShader->LookupVar("BokehBrightnessParams");
		BokehParams1 = PostFXShader->LookupVar("BokehParams1");
		BokehParams2 = PostFXShader->LookupVar("BokehParams2");

		DOFTargetSize = PostFXShader->LookupVar("DOFTargetSize");
		RenderTargetSize = PostFXShader->LookupVar("RenderTargetSize");
		BokehEnableVar = PostFXShader->LookupVar("BokehEnableVar");
		BokehGlobalAlphaVar = PostFXShader->LookupVar("BokehGlobalAlpha");
		BokehAlphaCutoffVar = PostFXShader->LookupVar("BokehAlphaCutoff");
		BokehSpritePointBufferVar = PostFXShader->LookupVar("BokehSpritePointBuffer");	
		BokehSortedIndexBufferVar = PostFXShader->LookupVar("BokehSortedIndexBuffer");	
		BokehSortOffsetsBufferVar = PostFXShader->LookupVar("BokehNumAddedToBuckets");	
		BokehSortedBufferVar = PostFXShader->LookupVar("BokehSortedListBuffer");	
		BokehSortLevelVar = PostFXShader->LookupVar("BokehSortLevel");
		BokehSortLevelMaskVar = PostFXShader->LookupVar("BokehSortLevelMask");
#if BOKEH_SORT_BITONIC_TRANSPOSE
		BokehSortTransposeMatWidthVar = PostFXShader->LookupVar("BokehSortTransposeMatWidth");
		BokehSortTransposeMatHeightVar = PostFXShader->LookupVar("BokehSortTransposeMatHeight");
#endif
# if RSG_DURANGO || RSG_ORBIS
		BokehOutputPointBufferVar = PostFXShader->LookupVar("BokehPointBuffer");
# endif
		PostFXAdaptiveDofEnvBlurParamsVar = PostFXShader->LookupVar("PostFXAdaptiveDofEnvBlurParams");
		PostFXAdaptiveDofCustomPlanesParamsVar = PostFXShader->LookupVar("PostFXAdaptiveDofCustomPlanesParams");
	}
#endif

#if ADAPTIVE_DOF_OUTPUT_UAV
	PostFXAdaptiveDOFParamsBufferVar = PostFXShader->LookupVar("AdaptiveDOFParamsBuffer");
#endif //ADAPTIVE_DOF_OUTPUT_UAV
#if DOF_TYPE_CHANGEABLE_IN_RAG
	CurrentDOFTechniqueVar = PostFXShader->LookupVar("currentDOFTechnique");
#endif
	// HDR
	BloomParamsId = PostFXShader->LookupVar("BloomParams");
	FilmicId[0] = PostFXShader->LookupVar("Filmic0");
	FilmicId[1] = PostFXShader->LookupVar("Filmic1");

	brightTonemapParams[0] = PostFXShader->LookupVar("BrightTonemapParams0");
	brightTonemapParams[1] = PostFXShader->LookupVar("BrightTonemapParams1");

	darkTonemapParams[0] = PostFXShader->LookupVar("DarkTonemapParams0");
	darkTonemapParams[1] = PostFXShader->LookupVar("DarkTonemapParams1");

	tonemapParams = PostFXShader->LookupVar("TonemapParams");

	// Noise	
	NoiseParamsId = PostFXShader->LookupVar("NoiseParams");

	// Motion Blur
	DirectionalMotionBlurParamsId = PostFXShader->LookupVar( "DirectionalMotionBlurParams" );
	DirectionalMotionBlurIterParamsId = PostFXShader->LookupVar( "DirectionalMotionBlurIterParams" );

	MBPrevViewProjMatrixXId =  PostFXShader->LookupVar("MBPrevViewProjMatrixX");
	MBPrevViewProjMatrixYId =  PostFXShader->LookupVar("MBPrevViewProjMatrixY");
	MBPrevViewProjMatrixWId =  PostFXShader->LookupVar("MBPrevViewProjMatrixW");

	// Night Vision
	lowLumId = PostFXShader->LookupVar("lowLum");
	highLumId = PostFXShader->LookupVar("highLum");
	topLumId = PostFXShader->LookupVar("topLum");

	scalerLumId = PostFXShader->LookupVar("scalerLum");

	offsetLumId = PostFXShader->LookupVar("offsetLum");
	offsetLowLumId = PostFXShader->LookupVar("offsetLowLum");
	offsetHighLumId = PostFXShader->LookupVar("offsetHighLum");

	noiseLumId = PostFXShader->LookupVar("noiseLum");
	noiseLowLumId = PostFXShader->LookupVar("noiseLowLum");
	noiseHighLumId = PostFXShader->LookupVar("noiseHighLum");

	bloomLumId = PostFXShader->LookupVar("bloomLum");

	colorLumId = PostFXShader->LookupVar("colorLum");
	colorLowLumId = PostFXShader->LookupVar("colorLowLum");
	colorHighLumId = PostFXShader->LookupVar("colorHighLum");

#if RSG_PC
	globalFreeAimDirId = PostFXShader->LookupVar("globalFreeAimDir");
#endif
	//light rays
	globalFograyParamId = PostFXShader->LookupVar("globalFogRayParam");
	globalFograyFadeParamId = PostFXShader->LookupVar("globalFogRayFadeParam");
	lightrayParamsId =	PostFXShader->LookupVar("lightrayParams");
	lightrayParams2Id=	PostFXShader->LookupVar("lightrayParams2");
	sslrParamsId = PostFXShader->LookupVar("sslrParams");
	sslrCenterId = PostFXShader->LookupVar("sslrCenter");
	SSLRTextureId = PostFXShader->LookupVar("SSLRTex");
	GLRTextureId = PostFXShader->LookupVar("GLRTex");

	//HeatHaze
	heatHazeParamsId = PostFXShader->LookupVar("HeatHazeParams");
	HeatHazeTex1ParamsId = PostFXShader->LookupVar("HeatHazeTex1Params");
	HeatHazeTex2ParamsId = PostFXShader->LookupVar("HeatHazeTex2Params");
	HeatHazeOffsetParamsId = PostFXShader->LookupVar("HeatHazeOffsetParams");

	//See Through
	seeThroughParamsId = PostFXShader->LookupVar("seeThroughParams");
	seeThroughColorNearId = PostFXShader->LookupVar("seeThroughColorNear");
	seeThroughColorFarId = PostFXShader->LookupVar("seeThroughColorFar"); 
	seeThroughColorVisibleBaseId = PostFXShader->LookupVar("seeThroughColorVisibleBase");
	seeThroughColorVisibleWarmId = PostFXShader->LookupVar("seeThroughColorVisibleWarm");
	seeThroughColorVisibleHotId = PostFXShader->LookupVar("seeThroughColorVisibleHot");

	//Vignetting
	VignettingParamsId = PostFXShader->LookupVar("VignettingParams");
	VignettingColorId = PostFXShader->LookupVar("VignettingColor");

	//Damage Overlay
	DamageOverlayMiscId = PostFXShader->LookupVar("DamageOverlayMisc");
	DamageOverlayForwardId = PostFXShader->LookupVar("DamageOverlayForward");
	DamageOverlayTangentId = PostFXShader->LookupVar("DamageOverlayTangent");
	DamageOverlayCenterPosId = PostFXShader->LookupVar("DamageOverlayCenterPos");

	// Gradient
	GradientFilterColTopId = PostFXShader->LookupVar("GradientFilterColTop");
	GradientFilterColMiddleId = PostFXShader->LookupVar("GradientFilterColMiddle");
	GradientFilterColBottomId = PostFXShader->LookupVar("GradientFilterColBottom");

	// Scanline
	ScanlineFilterParamsId	= PostFXShader->LookupVar("ScanlineFilterParams");

	// AA
	rcpFrameId = PostFXShader->LookupVar("rcpFrame");

	// Colour correction
	ColorCorrectId = PostFXShader->LookupVar("ColorCorrectHighLum");
	ColorShiftId = PostFXShader->LookupVar("ColorShiftLowLum");
	DesaturateId = PostFXShader->LookupVar("Desaturate");
	GammaId = PostFXShader->LookupVar("Gamma");

	// Exposure calculation
	ExposureParams0Id =  PostFXShader->LookupVar("ExposureParams0");
	ExposureParams1Id =  PostFXShader->LookupVar("ExposureParams1");
	ExposureParams2Id =  PostFXShader->LookupVar("ExposureParams2");
	ExposureParams3Id =  PostFXShader->LookupVar("ExposureParams3");

#if FILM_EFFECT
	LensDistortionId = PostFXShader->LookupVar("LensDistortionParams");
	PostFXFilmEffectTechnique = PostFXShader->LookupTechnique("PostFx_FilmEffect");
#endif

	// Luminance
	LuminanceDownsampleOOSrcDstSizeId = PostFXShader->LookupVar("LuminanceDownsampleOOSrcDstSize");

	// Get sampler handles
	// Miscs
#if USE_FXAA	    
	FXAABackBuffer = PostFXShader->LookupVar("BackBufferTexture");
#endif // USE_FXAA

#if PTFX_APPLY_DOF_TO_PARTICLES
	PtfxDepthBuffer = PostFXShader->LookupVar("PtfxDepthMapTexture");
	PtfxAlphaBuffer = PostFXShader->LookupVar("PtfxAlphaMapTexture");	
#endif
	GBufferTextureId0 = PostFXShader->LookupVar("gbufferTexture0");

#if __XENON
	GBufferTextureId2 = PostFXShader->LookupVar("gbufferTexture2"); 		
#endif // __XENON
	GBufferTextureIdDepth = PostFXShader->LookupVar("gbufferTextureDepth"); 
	ResolvedTextureIdDepth = PostFXShader->LookupVar("ResolvedDepthTexture");
	GBufferTextureIdSSAODepth = PostFXShader->LookupVar("gbufferTextureSSAODepth"); 

#if DEVICE_MSAA
	TextureHDR_AA = PostFXShader->LookupVar("HDRTextureAA");
#endif	//DEVICE_MSAA

	TextureID_0 = PostFXShader->LookupVar("PostFxTexture0");
#if __PPU
	TextureID_0a = PostFXShader->LookupVar("PostFxTexture0a");
#endif // __PPU		
	TextureID_1 = PostFXShader->LookupVar("PostFxTexture1");

	TextureID_v0 = PostFXShader->LookupVar("PostFxTextureV0");
	TextureID_v1 = PostFXShader->LookupVar("PostFxTextureV1");

	PostFxTexture2Id = PostFXShader->LookupVar("PostFxTexture2");
	PostFxTexture3Id = PostFXShader->LookupVar("PostFxTexture3");

#if __XENON || __D3D11 || RSG_ORBIS
	StencilCopyTextureId = PostFXShader->LookupVar("StencilCopyTexture");
	TiledDepthTextureId = PostFXShader->LookupVar("PostFxTextureV0");
#endif // __XENON || __D3D11 || RSG_ORBIS

#if !__PS3
	BloomTexelSizeId = PostFXShader->LookupVar("BloomTexelSize");
#endif

	// Motion Blur
	PostFxMotionBlurTextureID = PostFXShader->LookupVar("PostFxMotionBlurTexture");

	// HDR
	BloomTextureID = PostFXShader->LookupVar("BloomTexture");
	BloomTextureGID = PostFXShader->LookupVar("BloomTextureG");

	// Motion Blur
	JitterTextureId = PostFXShader->LookupVar("JitterTexture");
	MBPerspectiveShearParams0Id = PostFXShader->LookupVar("MBPerspectiveShearParams0");
	MBPerspectiveShearParams1Id = PostFXShader->LookupVar("MBPerspectiveShearParams1");
	MBPerspectiveShearParams2Id = PostFXShader->LookupVar("MBPerspectiveShearParams2");

	// Heat Haze
	HeatHazeTextureId = PostFXShader->LookupVar("HeatHazeTexture");
	PostFxHHTextureID = PostFXShader->LookupVar("PostFxHHTexture");
	HeatHazeMaskTextureID = PostFXShader->LookupVar("BackBuffer");

	// Screen Blur Fade
	ScreenBlurFadeID = PostFXShader->LookupVar("ScreenBlurFade");

	// Lens Distortion
	DistortionParamsID = PostFXShader->LookupVar("DistortionParams");

	// Lens Artefacts
	LensArtefactParams0ID = PostFXShader->LookupVar("LensArtefactsParams0");
	LensArtefactParams1ID = PostFXShader->LookupVar("LensArtefactsParams1");
	LensArtefactParams2ID = PostFXShader->LookupVar("LensArtefactsParams2");
	LensArtefactTextureID = PostFXShader->LookupVar("LensArtefactsTex");

	// Light Streaks
	LightStreaksColorShift0ID		= PostFXShader->LookupVar("LightStreaksColorShift0");
	LightStreaksBlurColWeightsID	= PostFXShader->LookupVar("LightStreaksBlurColWeights");
	LightStreaksBlurDirID			= PostFXShader->LookupVar("LightStreaksBlurDir");

	// Blur Vignette	
	BlurVignettingParamsID = PostFXShader->LookupVar("BlurVignettingParams");

	ASSET.PopFolder();

#if POSTFX_UNIT_QUAD
	quad::Position	= PostFXShader->LookupVar("QuadPosition");
	quad::TexCoords	= PostFXShader->LookupVar("QuadTexCoords");
	quad::Scale = PostFXShader->LookupVar("QuadScale");
#if USE_IMAGE_WATERMARKS
	quad::Alpha = PostFXShader->LookupVar("QuadAlpha");
#endif
#endif

	// SetShaderData expects the same order and number of passes as the LensArtefactShaderPass enumeration
	u32 lensArtefactShaderPasses[] = { pp_lens_artefacts, pp_BloomComposite, pp_light_streaks_blur_low, pp_light_streaks_blur_med, pp_light_streaks_blur_high, pp_lens_artefacts_combined };
	LENSARTEFACTSMGR.SetShaderData(PostFXShader, &lensArtefactShaderPasses[0]);

#if FXAA_CUSTOM_TUNING
	fxaaBlurinessId = PostFXShader->LookupVar("Bluriness");
	fxaaConsoleEdgeSharpnessId = PostFXShader->LookupVar("ConsoleEdgeSharpness");
	fxaaConsoleEdgeThresholdId = PostFXShader->LookupVar("ConsoleEdgeThreshold");
	fxaaConsoleEdgeThresholdMinId = PostFXShader->LookupVar("ConsoleEdgeThresholdMin");
	fxaaQualitySubpixId = PostFXShader->LookupVar("QualitySubpix");
	fxaaEdgeThresholdId = PostFXShader->LookupVar("EdgeThreshold");
	fxaaEdgeThresholdMinId = PostFXShader->LookupVar("EdgeThresholdMin");

	fxaaBluriness = PS3_SWITCH(0.33f, 0.5f);
	fxaaConsoleEdgeSharpness = 8.0f;
	fxaaConsoleEdgeThreshold = 0.125f;
	fxaaConsoleEdgeThresholdMin = 0.05f;
	fxaaQualitySubpix = 0.317f; // 0.75f;
	fxaaEdgeThreshold = 0.333f; // 0.166f;
	fxaaEdgeThresholdMin = 0.0833f;
#endif

	fpvMotionBlurWeightsId = PostFXShader->LookupVar("fpvMotionBlurWeights");
	fpvMotionBlurVelocityId = PostFXShader->LookupVar("fpvMotionBlurVelocity");
	fpvMotionBlurSizeId = PostFXShader->LookupVar("fpvMotionBlurSize");
	fpvMotionBlurVelocity = Vec2V(V_ZERO);
	fpvMotionBlurPrevCameraPos[0] = Vec3V(V_ZERO);
	fpvMotionBlurPrevCameraPos[1] = Vec3V(V_ZERO);
	fpvMotionBlurCurrCameraPos = Vec3V(V_ZERO);
	fpvMotionBlurPrevModelViewMtx = Mat44V(V_IDENTITY);

	fpvMotionBlurWeights = Vector4(1.0f, 32.0f, 1.0f, 1.0f);
	fpvMotionBlurSize = 1.0f;
	fpvMotionVelocityMaxSize = 32.0f;
	fpvMotionBlurVelocity = Vec2V(0.0f, 0.0f);

	fpvMotionBlurFirstFrame = true;

#if DEVICE_MSAA
	g_UseSubSampledAlpha = CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shader_SSA && (GRCDEVICE.GetMSAA() == grcDevice::MSAA_None);
#endif
}

void PostFX::SetGaussianBlur()
{
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
		bool bGaussian = (GRCDEVICE.GetDxFeatureLevel() >=1100) ? true : false;
#if RSG_PC
		bGaussian &= (((GET_POSTFX_SETTING(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX)) >= CSettings::High) && (GRCDEVICE.IsStereoEnabled() == false)) ? true : false;
#endif // RSG_PC

		//default to computeGaussian
		if(bGaussian)
			CurrentDOFTechnique = dof_computeGaussian;
		else
			CurrentDOFTechnique = dof_console;
		
		if (bGaussian)
		{
#if !DOF_TYPE_CHANGEABLE_IN_RAG
	#if DOF_COMPUTE_GAUSSIAN
			CurrentDOFTechnique = dof_computeGaussian;
	#endif
	#if DOF_DIFFUSION
			CurrentDOFTechnique = dof_diffusion;
	#endif
#endif //!DOF_TYPE_CHANGEABLE_IN_RAG
		}
#if ADAPTIVE_DOF && DOF_COMPUTE_GAUSSIAN
	if( bGaussian && AdaptiveDepthOfField.IsEnabled())
		CurrentDOFTechnique = dof_computeGaussian;
#endif //ADAPTIVE_DOF && DOF_COMPUTE_GAUSSIAN
#endif //DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
}

#if RSG_PC
void PostFX::SetRequireResetDOFRenderTargets(bool request)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	g_RequestResetDOFRenderTargets = request;
}

bool PostFX::GetRequireResetDOFRenderTargets()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	return g_RequestResetDOFRenderTargets;
}

void PostFX::ResetDOFRenderTargets()
{
	u32 width = VideoResManager::GetSceneWidth();
	u32 height = VideoResManager::GetSceneHeight();

	grcTextureFactory::CreateParams params;
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 
	params.UseFloat = true;
	params.InTiledMemory	= true;
	params.IsSwizzled		= false;
	params.UseFloat = true;
	params.EnableCompression = false;

#if RSG_PC
	params.StereoRTMode = grcDevice::STEREO;	
#endif	
	
	int bpp;
	GetPostFXDefaultRenderTargetFormat(params.Format, bpp);

	RecreateDOFRenderTargets(params, bpp, width, height);
}
#endif

void PostFX::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		AnimPostFXManager::ClassInit();
		OverlayTextRenderer::InitializeTextBlendState();
		PhonePhotoEditor::ClassInit();
		LensArtefacts::ClassInit();
#if USE_SCREEN_WATERMARK
		ms_watermarkParams.Init();
#endif
		DefaultFlashEffectModName = atHashString("PlayerSwitchNeutralFlash",0x63EC45A2);
		g_defaultFlashEffect.Set(DefaultFlashEffectModName, POSTFX_IN_HOLD_OUT, 0U, 0U, 0U, 0U);

#if __BANK
		g_ForceUseSimple = PARAM_simplepfx.Get(); // set this before rag widget, so it will inherit this value as the default
		g_pedKillOverlay.m_disable = false;
#endif // __BANK		
		InitRenderStateBlocks();

        Displayf("sizeof(paramBlock) = %" SIZETFMT "d",sizeof(PostFXParamBlock::paramBlock));

#if __XENON
	    grcGPRAllocation::Init();
#endif

	    RAGE_TRACK(grPostFx);

		InitMSAAShaders();

	    ASSET.PushFolder("common:/shaders");

#if ADAPTIVE_DOF
		AdaptiveDepthOfField.Init();
#endif

		SetGaussianBlur();

#if RSG_PC
		u32	shaderModelMajor, shaderModelMinor;

		GRCDEVICE.GetDxShaderModel(shaderModelMajor, shaderModelMinor);
		if(shaderModelMajor == 5)
		{
			FXAAPassToUse = pp_AA_sm50;
		}
#endif

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
		dofComputeShader = grmShaderFactory::GetInstance().Create();
		dofComputeShader->Load(
			"dofCompute");
		dofComputeTechnique = dofComputeShader->LookupTechnique("DOF_Compute");
#endif //DOF_COMPUTE_GAUSSIAN
#if DOF_DIFFUSION
		dofDiffusionShader = grmShaderFactory::GetInstance().Create();
		dofDiffusionShader->Load(
			"dofDiffusion");
		dofDiffusiontechnique = dofDiffusionShader->LookupTechnique("DOF_Diffusion", RSG_PC ? false : true);
#endif

#if AVG_LUMINANCE_COMPUTE
		AverageLumunianceComputeEnable = (GRCDEVICE.GetDxFeatureLevel() >= 1100);

		avgLuminanceShader = grmShaderFactory::GetInstance().Create();
		avgLuminanceShader->Load(
			"avgLuminanceCompute");
		avgLuminanceTechnique = avgLuminanceShader->LookupTechnique("AvgLuminanceTechnique", AverageLumunianceComputeEnable);
#endif

#if BOKEH_SUPPORT || WATERMARKED_BUILD
		
		g_BokehTxdSlot = vfxUtil::InitTxd("graphics");

#if WATERMARKED_BUILD
		g_TxdSlot = vfxUtil::InitTxd("graphics_pc");
		grcTexture *pTexture = g_TxdStore.Get(g_TxdSlot)->Lookup("bokeh_pent");
		if(pTexture)
			pTexture->AddRef();

		pTexture = g_TxdStore.Get(g_TxdSlot)->Lookup("bokeh_deca");
		if(pTexture)
			pTexture->AddRef();
#endif

#endif

#if BOKEH_SUPPORT
#if RSG_PC
		if ((GRCDEVICE.GetDxFeatureLevel() >= 1100)  NOTFINAL_ONLY(&& !PARAM_disableBokeh.Get()))
#else
		if( BokehEnable )
#endif
		{
			m_BokehShapeTextureSheet = g_TxdStore.Get(g_BokehTxdSlot)->Lookup("BOKEH_8sided_iris_4x4sheet");
			Assert(m_BokehShapeTextureSheet);
			if(m_BokehShapeTextureSheet)
				m_BokehShapeTextureSheet->AddRef();
		}
#endif

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
		DofKernelSize = dofComputeShader->LookupVar("kernelRadius");
		GaussianWeightsBufferVar = dofComputeShader->LookupVar("GaussianWeights");
		DofProjCompute = dofComputeShader->LookupVar("dofProjCompute");
		DofSkyWeightModifierVar = dofComputeShader->LookupVar("dofSkyWeightModifier");
		DofLumFilterParams = dofComputeShader->LookupVar("LumFilterParams");
		DofRenderTargetSizeVar = dofComputeShader->LookupVar("dofRenderTargetSize");
		fourPlaneDofVar = dofComputeShader->LookupVar("fourPlaneDof");
#if COC_SPREAD
		DofCocSpreadKernelRadius = dofComputeShader->LookupVar("cocSpreadKernelRadius");
#endif
#if ADAPTIVE_DOF_OUTPUT_UAV
		DofComputeAdaptiveDOFParamsBufferVar = dofComputeShader->LookupVar("AdaptiveDOFParamsBuffer");
#endif //ADAPTIVE_DOF_OUTPUT_UAV
#if __BANK
		g_cocOverlayAlpha = dofComputeShader->LookupVar("cocOverlayAlpha");
#endif
#endif //DOF_COMPUTE_GAUSSIAN
#if DOF_DIFFUSION
		dofDiffusion_lastbufferSizeVar = dofDiffusionShader->LookupVar("LastBufferSize");
		dofDiffusion_DiffusionRadiusVar = dofDiffusionShader->LookupVar("DiffusionRadius");
		dofDiffusion_txABC_ID = dofDiffusionShader->LookupVar("txABC_Texture");
		dofDiffusion_txCOC_ID = dofDiffusionShader->LookupVar("txCOC_Texture");
		dofDiffusion_txX_ID = dofDiffusionShader->LookupVar("txX_Texture");
		dofDiffusion_txY_ID = dofDiffusionShader->LookupVar("txYn_Texture");
#endif //DOF_DIFFUSION

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
		dofComputeColorTex = dofComputeShader->LookupVar("ColorTexture");
		dofComputeDepthBlurTex = dofComputeShader->LookupVar("DepthBlurTexture");
		dofComputeOutputTex = dofComputeShader->LookupVar("DOFOutputTexture");
		dofComputeDepthBlurHalfResTex = dofComputeShader->LookupVar("DepthBlurHalfResTexture");
		dofBlendParams = dofComputeShader->LookupVar("DOFBlendParams");
#if COC_SPREAD
		cocComputeOutputTex = dofComputeShader->LookupVar("COCOutputTexture");
#endif
#endif //DOF_COMPUTE_GAUSSIAN

#if AVG_LUMINANCE_COMPUTE
		if (AverageLumunianceComputeEnable)
		{
			LumCSDownsampleTexSizeId = avgLuminanceShader->LookupVar("DstTextureSize");

			LumCSDownsampleSrcId = avgLuminanceShader->LookupVar("AvgLuminanceTexture");
			LumCSDownsampleInitSrcId = avgLuminanceShader->LookupVar("HDRColorTexture");
			LumCSDownsampleDstId = avgLuminanceShader->LookupVar("AverageLuminanceOut");
		}		
#endif

		ASSET.PopFolder();

	    // Load heat haze textures
		HeatHazeTexture = CShaderLib::LookupTexture("heat_haze");
	    Assert(HeatHazeTexture);
	    if (HeatHazeTexture)
	    {
		    HeatHazeTexture->AddRef();
	    }  

#if __PS3
		// Main RT mempool for
		grcRTPoolCreateParams params;
		params.m_Size = 16 * 16 * 16;
		params.m_HeapCount = 1;
		#if __GCM
			params.m_Type = grcrtPermanent;
			params.m_BitDepth = 128;
			params.m_Alignment =  gcm::GetSurfaceAlignment(false, false);
			params.m_PhysicalMem = false;
			params.m_Zculled = false;
			params.m_Pitch =  gcm::GetSurfaceTiledPitch(1, 128, false);	
			params.m_Tiled = false; 
			params.m_Swizzled = false;
			params.m_Compression = 0;
		#endif
		g_RTPoolIdPostFX = grcRenderTargetPoolMgr::CreatePool("PostFX Main Pool", params);
#endif
		CreateRenderTargets(VideoResManager::GetSceneWidth(), VideoResManager::GetSceneHeight());		

		// SetShaderData expects the same order and number of passes as the LensArtefactShaderPass enumeration
		u32 lensArtefactShaderPasses[] = { pp_lens_artefacts, pp_BloomComposite, pp_light_streaks_blur_low, pp_light_streaks_blur_med, pp_light_streaks_blur_high, pp_lens_artefacts_combined };
		LENSARTEFACTSMGR.SetShaderData(PostFXShader, &lensArtefactShaderPasses[0]);

		LENSARTEFACTSMGR.SetRenderCallback(MakeFunctor(PostFX::SimpleBlitWrapper));

#if __BANK
	    g_delayFlipWait = !PARAM_dontDelayFlipSwitch.Get();
#endif

		g_bulletImpactOverlay.Init();


		grcVertexElement elements[] =
		{
			grcVertexElement(0, grcVertexElement::grcvetPosition, 0, 8, grcFvf::grcdsFloat2)
		};
		sm_FogVertexDeclaration = GRCDEVICE.CreateVertexDeclaration(elements, sizeof(elements)/sizeof(grcVertexElement));
		
		RecreateFogVertexBuffer();

#if ADAPTIVE_DOF_GPU && ADAPTIVE_DOF_OUTPUT_UAV 
		WIN32PC_ONLY(if(GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50)))
			AdaptiveDepthOfField.InitiliaseOnRenderThread();
#endif //ADAPTIVE_DOF_OUTPUT_UAV
#if BOKEH_SUPPORT || DOF_COMPUTE_GAUSSIAN
		WIN32PC_ONLY(if(GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50)))
			InitBokehDOF();
#endif // BOKEH_SUPPORT || DOF_COMPUTE_GAUSSIAN

#if ENABLE_MLAA
		g_MLAA.Initialize(RSG_ORBIS ? CRenderTargets::GetUIBackBuffer() : NULL);
#endif
		float fGamma;
		if (PARAM_setGamma.Get(fGamma))
		{
			PostFX::SetGammaFrontEnd(fGamma);
		}
    }
    else if(initMode == INIT_SESSION)
    {
	    // Reset all to default state
	    ResetAdaptedLuminance();
	    g_enableLightRays=false;
        g_enableNightVision=false;
	    g_enableSeeThrough=false;

#if DEVICE_MSAA 
		
#if USE_HQ_ANTIALIASING

		g_AntiAliasingType = AA_FXAA_DEFAULT;
		g_FXAAEnable = CSettingsManager::GetInstance().GetSettings().m_graphics.m_FXAA_Enabled;
#if USE_NV_TXAA
		g_TXAAEnable = CSettingsManager::GetInstance().GetSettings().m_graphics.m_TXAA_Enabled;
#endif // USE_NV_TXAA

#if !__FINAL
		u32 fxaaMode = 1;
		if (PARAM_fxaaMode.Get(fxaaMode))
		{
			if (fxaaMode == 0)
			{
				g_AntiAliasingType = AA_FXAA_DEFAULT;
				g_FXAAEnable = false;
			}
			else if (fxaaMode == 1)
			{
				g_AntiAliasingType = AA_FXAA_DEFAULT;
				g_FXAAEnable = true;
			}
			else if (fxaaMode == 2)
			{
				g_AntiAliasingType = AA_FXAA_HQ;
				g_FXAAEnable = true; 
			}
		}
#endif
#endif // USE_HQ_ANTIALIASING
		
#endif

#if FILM_EFFECT
		g_EnableFilmEffect = false;
		g_BaseDistortionCoefficient = -0.028f;
		g_BaseCubeDistortionCoefficient = 1.2f;
		g_ChromaticAberrationOffset = 0.038f;
		g_ChromaticAberrationCubeOffset = 0.720f;
		g_FilmNoise = 0.05f;
		g_FilmVignettingIntensity = 1.0f;
		g_FilmVignettingRadius = 1.5f;
		g_FilmVignettingContrast = 2.0f;
#endif

#if __BANK
	    g_Override = false;
#endif

	    g_noiseOverride = false;
	    g_noisinessOverride = 0.0f;

		if (g_pNoiseTexture == NULL)
		{
			// make sure Lights has been initialised
			g_pNoiseTexture = CShaderLib::LookupTexture("waternoise");
			g_pNoiseTexture->AddRef();
		}

		if (g_pDamageOverlayTexture == NULL)
		{
			g_pDamageOverlayTexture =  CShaderLib::LookupTexture("dmg_overlay_fade");
			if (g_pDamageOverlayTexture)
				g_pDamageOverlayTexture->AddRef();
		}

#if USE_HQ_ANTIALIASING
		g_HQAATiledTechnique = CSprite2d::GetShader()->LookupTechnique("HighQualityAA_Tiled");
		g_HQAA_srcMapID	= CSprite2d::GetShader()->LookupVar("TransparentSrcMap");
#endif

		g_screenBlurFade.Init();
		ANIMPOSTFXMGR.Reset();
		PAUSEMENUPOSTFXMGR.Reset();

#if RSG_PC
		if(GRCDEVICE.UsingMultipleGPUs())
			ResetDOFRenderTargets();
#endif
	}

	CHighQualityScreenshot::Init(initMode);

#if FXAA_CUSTOM_TUNING
	fxaaBlurinessId = PostFXShader->LookupVar("Bluriness");
	fxaaConsoleEdgeSharpnessId = PostFXShader->LookupVar("ConsoleEdgeSharpness");
	fxaaConsoleEdgeThresholdId = PostFXShader->LookupVar("ConsoleEdgeThreshold");
	fxaaConsoleEdgeThresholdMinId = PostFXShader->LookupVar("ConsoleEdgeThresholdMin");
	fxaaQualitySubpixId = PostFXShader->LookupVar("QualitySubpix");
	fxaaEdgeThresholdId = PostFXShader->LookupVar("EdgeThreshold");
	fxaaEdgeThresholdMinId = PostFXShader->LookupVar("EdgeThresholdMin");

	fxaaBluriness = PS3_SWITCH(0.33f, 0.5f);
	fxaaConsoleEdgeSharpness = 8.0f;
	fxaaConsoleEdgeThreshold = 0.125f;
	fxaaConsoleEdgeThresholdMin = 0.05f;
	fxaaQualitySubpix = 0.317f; // 0.75f;
	fxaaEdgeThreshold = 0.333f; // 0.166f;
	fxaaEdgeThresholdMin = 0.0833f;
#endif

#if USE_NV_TXAA
#if !__FINAL
	PARAM_txaaDevMode.Get(g_TXAADevMode);		// But this one works.
#endif // !__FINAL
#endif

	g_UseSubSampledAlpha = (CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shader_SSA && (GRCDEVICE.GetMSAA() == grcDevice::MSAA_None)) || RSG_DURANGO || RSG_ORBIS;
}

void PostFX::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {

		AnimPostFXManager::ClassShutdown();
		PhonePhotoEditor::ClassShutdown();
		LensArtefacts::ClassShutdown();

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
		DeleteScreenBasedRenderTargets();
#endif

#if __XENON
	    grcGPRAllocation::Terminate();
#endif // __XENON

	    delete PostFXShader;

#if MSAA_EDGE_PASS
		if (PostFXShaderMS0)
		{
			delete PostFXShaderMS0;
			PostFXShaderMS0 = NULL;
		}
#endif // MSAA_EDGE_PASS

        if(HeatHazeTexture)
        {
            HeatHazeTexture->Release();
            HeatHazeTexture = 0;
        }

		if (g_pNoiseTexture) 
		{
			g_pNoiseTexture->Release();
			g_pNoiseTexture=NULL;
		}

		if (g_pDamageOverlayTexture)
		{
			g_pDamageOverlayTexture->Release();
			g_pDamageOverlayTexture = NULL;
		}

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
		delete dofComputeShader;
		dofComputeShader = NULL;
#endif
#if DOF_DIFFUSION
		delete dofDiffusionShader;
		dofDiffusionShader = NULL;
#endif

#if ADAPTIVE_DOF
		AdaptiveDepthOfField.DeleteRenderTargets();
#endif

#if AVG_LUMINANCE_COMPUTE
		delete avgLuminanceShader;
		avgLuminanceShader = NULL;
#endif

#if ENABLE_MLAA
		g_MLAA.Shutdown(!RSG_ORBIS);
#endif

		vfxUtil::ShutdownTxd("graphics_pc");
    }
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
		CHighQualityScreenshot::Shutdown(shutdownMode);
//		CLowQualityScreenshot::Shutdown(shutdownMode);
	}
}

#if ADAPTIVE_DOF_CPU
bool UseAdaptiveDofCPU()
{
	return (CPhoneMgr::CamGetState() && CPhoneMgr::GetDOFState() == true);
}
#endif

#if DOF_TYPE_CHANGEABLE_IN_RAG
void ProcessDOFTechniqueChange()
{
	ProcessDOFChangeOnRenderThread = true;
}

void ProcessDOFChangeRenderThread()
{
	u32 width = VideoResManager::GetSceneWidth();
	u32 height = VideoResManager::GetSceneHeight();

	grcTextureFactory::CreateParams params;
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 
	params.UseFloat = true;
	params.InTiledMemory	= true;
	params.IsSwizzled		= false;
	params.UseFloat = true;
	params.EnableCompression = false;

#if RSG_PC
	params.StereoRTMode = grcDevice::STEREO;	
#endif	
	
	int bpp;
	GetPostFXDefaultRenderTargetFormat(params.Format, bpp);

	RecreateDOFRenderTargets(params, bpp, width, height);
#if BOKEH_SUPPORT
	RecreateBokehRenderTargets(params, bpp, width, height);
#endif

	switch (CurrentDOFTechnique)
	{
	case dof_console:		
		break;
#if DOF_COMPUTE_GAUSSIAN
	case dof_computeGaussian:
		break;
#endif
#if DOF_DIFFUSION
	case dof_diffusion:
		RecreateDOFDiffusionRenderTargets(params, bpp, width, height);
		break;
#endif
	default:
		break;
	}

	ProcessDOFChangeOnRenderThread = false;
}
#endif

#if __BANK

static bkButton *ms_addPostFxWidgets = NULL;
static bkGroup *ms_postFxGroup = NULL;

static bool gFrameCapture = false;
static bool gFrameCaptureSingle = false;
static char gFrameCapPath[255] = "X:\\framecap\\frame";
static bool gFrameUseCutsceneNameForPath = false;
static bool gFrameCapJpeg = true;
static bool gFrameCaptureOnCutsceneStart = false;
static float gFrameCaptureFPS = 30.0f;
static bool gFrameCaptureMBSync = true;
static bool gFrameCaptureBinkMode = false;
static bool gFrameCaptureCutsceneMode = false;

static bool gTriggerScreenBlurFadeIn = false;
static bool gTriggerScreenBlurFadeOut = false;
static bool gDisableScreenBlurFadeOut = false;

void PostFX::AddWidgetsOnDemand()
{
	bkBank &bk = *BANKMGR.FindBank("Renderer"); 
	
	// remove old button:
	if(ms_addPostFxWidgets)
	{
		bk.Remove(*((bkWidget*)ms_addPostFxWidgets));
		ms_addPostFxWidgets = NULL;
	}
	
	bk.SetCurrentGroup(*ms_postFxGroup);

	bk.AddToggle("Delay flip wait",&g_delayFlipWait);
	bk.AddToggle("Override Timecycle", &g_Override);
#if __PS3
	bk.AddToggle("Use Default Composite Tiled Technique", &g_PS3UseCompositeTiledTechniques);
#endif
	bk.AddToggle("Use Tile Classification Data", &g_UseTiledTechniques);

	bk.AddToggle("Enable light rays above water", &g_lightRaysAboveWaterLine);

	bk.AddSeparator();
	bk.AddToggle("Skip auto exposure", &g_AutoExposureSkip); 
	bk.AddSlider("Exposure", &g_overwrittenExposure, -16.0f, 16.0f, 0.01f); 
	bk.AddSlider("Current Gamma", &PostFX::g_gammaFrontEnd, 0.0001f, 100.0f, 0.0001f);
	bk.AddToggle("No tone-mapping", &g_JustExposure);
	bk.AddToggle("Force Simple Post Fx", &g_ForceUseSimple);
	bk.AddSlider("Color compression", g_adaptation.GetUpdatePackScalarPtr(),0.0f,256.0f,0.5f);
	bk.AddToggle("Enable auto colour compression", &g_useAutoColourCompression);
	bk.AddSeparator();

#if __XENON
	bk.AddToggle("Use Packed 7e3Int Back Buffer", &g_UsePacked7e3IntBackBuffer);
#endif

	bk.PushGroup("Continuous Frame Capture", false);
		bk.AddToggle("Continuous Frame Capture", &gFrameCapture);
		bk.AddToggle("Wait for cutscene start", &gFrameCaptureOnCutsceneStart);
		bk.AddToggle("MotionBuilder frame sync", &gFrameCaptureMBSync);
		bk.AddToggle("Single Frame Capture", &gFrameCaptureSingle);
		bk.AddText("Filepath prefix", &gFrameCapPath[0], 255);
		bk.AddToggle("Use cutscene name for path", &gFrameUseCutsceneNameForPath);
		bk.AddToggle("Use JPEG instead of PNG", &gFrameCapJpeg);
		bk.AddSlider("FPS", &gFrameCaptureFPS, 5.0f, 120.0f, 1.0f);
		bk.AddToggle("Bink Mode", &gFrameCaptureBinkMode);
		bk.AddToggle("Cutscene Mode", &gFrameCaptureCutsceneMode);
	bk.PopGroup();

	bk.PushGroup("First Person Motion Blur");
		bk.AddToggle("Enabled", &g_fpvMotionBlurEnable);
		bk.AddSlider("Fade Strength", &fpvMotionBlurWeights.x, 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Weight 1", &fpvMotionBlurWeights.y, 0.0f, 1024.0f, 0.01f);
		bk.AddSlider("Velocity Blend", &fpvMotionBlurWeights.z, 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Noisiness", &fpvMotionBlurWeights.w, 0.0f, 1.0f, 0.01f);
		bk.AddVector("Velocity", &fpvMotionBlurVelocity, -1024.0f, 1024.0f, 0.01f);
		bk.AddSlider("Blur Size", &fpvMotionBlurSize, 0.0f, 256.0f, 0.01f);
		bk.AddSlider("Maximum Velocity", &fpvMotionVelocityMaxSize, 0.0f, 256.0f, 0.01f);
		bk.AddToggle("Enable when in vehicle", &g_fpvMotionBlurEnableInVehicle);
		bk.AddToggle("Enable dynamic velocity update", &g_fpvMotionBlurEnableDynamicUpdate);
		bk.AddToggle("Draw velocity/position debug", &g_fpvMotionBlurDrawDebug);
		bk.AddToggle("Override TAG data", &g_fpvMotionBlurOverrideTagData);
	bk.PopGroup();

	bk.PushGroup("SSA", false);
		bk.AddToggle("Use Sub Sampled Alpha", &g_UseSubSampledAlpha);
	#if SSA_USES_CONDITIONALRENDER
		bk.AddToggle("Use Conditional Rendering with SSA", &g_UseConditionalSSA);
	#endif
		bk.AddToggle("Use Single Pass SSA",&g_UseSinglePassSSA);
		bk.AddToggle("Use SSA On Foliage", &g_UseSSAOnFoliage);
	bk.PopGroup();	

	const char* pStrAATypes[] = 
	{
		"FXAA Default", 
#if USE_HQ_ANTIALIASING
		"FXAA HQ", 
#endif // USE_HQ_ANTIALIASING
		"FXAA UI"
	};
	bk.PushGroup("FXAA", false);
		bk.AddToggle("Enable FXAA Pass",	&g_FXAAEnable);
		bk.AddCombo("FXAA Type", (s32*)&g_AntiAliasingType, (s32)(sizeof(pStrAATypes)/sizeof(char*)), pStrAATypes );
		#if USE_HQ_ANTIALIASING
			bk.AddSlider("FXAA Switch Distance ", &g_AntiAliasingSwitchDistance, 0.f, 10000.f, 1.f);
		#endif
		#if FXAA_CUSTOM_TUNING
			bk.PushGroup("FXAA Tuning");
				bk.AddSlider("Bluriness", &fxaaBluriness, 0.33f, 0.5f, 0.01f); // Max=0.5 Min = 0.33 Step=0.01 Default=0.5         
				bk.AddSlider("ConsoleEdgeSharpness", &fxaaConsoleEdgeSharpness, 2.0f, 8.0f, 2.0f); // Max=8, Min=2 Step=*2, Default=8                  
				bk.AddSlider("ConsoleEdgeThreshold", &fxaaConsoleEdgeThreshold, 0.125f, 0.250f, 0.125f); // Max=0.25, Min=0.125 Step=0.125 Default=0.125     
				bk.AddSlider("ConsoleEdgeThresholdMin", &fxaaConsoleEdgeThresholdMin, 0.02f, 0.08f, 0.01f); // Max=0.08, Min=0.02 Step=0.01 Default=0.05        

				bk.AddSlider("Quality Sub-pix", &fxaaQualitySubpix, 0.0f, 1.0f, 0.01f); // Max=1.0, Min=0.0 Step=0.01 Default=0.75
				bk.AddSlider("Edge Threshold", &fxaaEdgeThreshold, 0.063f, 1.0f, 0.003f); // Max=0.333, Min=0.063 Step=0.003 Default 0.166
				bk.AddSlider("Edge Threshold Min", &fxaaEdgeThresholdMin, 0.0312f, 0.0833f, 0.001f); // Max=0.0833, Min=0.0312 Step=0.001 Default=0.0833
			bk.PopGroup();
		#endif
	bk.PopGroup();

	MLAA_ONLY(MLAA::AddWidgets(bk);)

#if USE_NV_TXAA
	bk.PushGroup("TXAA", false);
		bk.AddToggle("Enable TXAA",	&g_TXAAEnable);
		bk.AddSlider("Mode", &g_TXAADevMode, 0, TXAA_MODE_TOTAL, 1);
	bk.PopGroup();
#endif

	bk.PushGroup("Debug", true);
		bk.PushGroup("Vector", true);
		for (int i = 0; i < NUM_DEBUG_PARAMS; i++)
		{
			char name[64];
			sprintf(name, "Vector %d", i);
			bk.AddVector(name, &DebugParams[i], -1024.0f, 1024.0f, 0.01f);
		}
		bk.PopGroup();
		bk.PushGroup("Colour", true);
		for (int i = 0; i < NUM_DEBUG_PARAMS; i++)
		{
			char name[64];
			sprintf(name, "Colour %d", i);
			bk.AddColor(name, &DebugParams[i]);
		}
		bk.PopGroup();
	bk.PopGroup();

	bk.PushGroup("Light Rays", false);
		bk.AddToggle("Force Using Underwater Technique", &g_ForceUnderWaterTech);
		bk.AddToggle("Force Using Abovewater Technique", &g_ForceAboveWaterTech);
		bk.AddToggle("Fade if Sun is Off Screen", &g_fadeSSLROffscreen);
		bk.AddSlider("Offscreen Fade Factor", &g_fadeSSLROffscreenValue,0.1f,4.0f,0.01f,NullCB,"Light Ray Fade Factor. 1.0 means fade at edge of the screen. Less than 1.0 fades before the edge.");
	
		bk.AddSlider("Light Ray Additive Reducer", &editedSettings.m_sslrParams.x,0.1f,8.0f,0.01f);
		bk.AddSlider("SSLR Blit Size",&editedSettings.m_sslrParams.y,0.1f,20.0f,0.1f);
		bk.AddSlider("SSLR Ray Length",&editedSettings.m_sslrParams.z,0.1f,20.0f,0.1f);
	bk.PopGroup();

	ANIMPOSTFXMGR.AddWidgets(bk);

	bk.PushGroup("Fog Ray");
	bk.AddSlider("FogRay Pow", &editedSettings.m_fograyContrast, 1.0f, 10.0f, 0.01f, NullCB, "Global Fogray Pow." );
	bk.AddSlider("FogRay Intensity", &editedSettings.m_fograyIntensity, 0.0f, 1.0f, 0.001f, NullCB, "Global Fogray intensity." );
	bk.AddSlider("FogRay Density", &editedSettings.m_fograyDensity, 0.0f, 20.0f, 0.001f, NullCB, "Global Fogray density." );
	bk.AddSlider("FogRay Jitter Start", &g_fograyJitterStart, 0.0f, 2000.0f, 10.0f, NullCB, "Global Fogray jitter start");
	bk.AddSlider("FogRay Jitter End", &g_fograyJitterEnd, 0.0f, 3000.0f, 10.0f, NullCB, "Global Fogray jitter end");
	bk.PopGroup();

	bk.PushGroup("Colour Filters", false);
	if ( bkRemotePacket::IsConnectedToRag() )
	{
		bk.AddColor("Color Correction",reinterpret_cast<Vector3*>(&editedSettings.m_colorCorrect), NullCB, "Global color correction." );
		bk.AddColor("Color Shift",reinterpret_cast<Vector3*>(&editedSettings.m_colorShift), NullCB, "Global color shift." );
	}
	else
	{
		bk.AddSlider("Color Correction Red Channel", &editedSettings.m_colorCorrect.x, 0.0f, 1.0f, 0.01f, NullCB, "Global color correction (Red)." );
		bk.AddSlider("Color Correction Green Channel", &editedSettings.m_colorCorrect.y, 0.0f, 1.0f, 0.01f, NullCB, "Global color correction (Green)." );
		bk.AddSlider("Color Correction Blue Channel", &editedSettings.m_colorCorrect.z, 0.0f, 1.0f, 0.01f, NullCB, "Global color correction (Blue)." );

		bk.AddSlider("Color Shift Red Channel", &editedSettings.m_colorShift.x, 0.0f, 1.0f, 0.01f, NullCB, "Global color shift (Red)." );
		bk.AddSlider("Color Shift Green Channel", &editedSettings.m_colorShift.y, 0.0f, 1.0f, 0.01f, NullCB, "Global color shift (Green)." );
		bk.AddSlider("Color Shift Blue Channel", &editedSettings.m_colorShift.z, 0.0f, 1.0f, 0.01f, NullCB, "Global color shift (Blue)." );
	}
	bk.AddSlider("Saturation", &editedSettings.m_deSaturate, 0.0f, 1.0f, 0.01f, NullCB, "Color saturation." );
	bk.PopGroup();

	bk.PushGroup("Bloom");
		bk.AddToggle("Enable bloom", &g_bloomEnable);
		bk.AddSeparator();
		bk.AddSlider("Bloom Threshold (Width)", &editedSettings.m_bloomThresholdWidth, 0.0f, 10.0f, 0.01f, NullCB);
		bk.AddSlider("Bloom Threshold (Max)", &editedSettings.m_bloomThresholdMax, 0.0f, 3.0f, 0.01f, NullCB);
		bk.AddSlider("Bloom Threshold Power", &g_bloomThresholdPower, -16.0f, 16.0f, 0.01f);
		bk.AddSlider("Bloom Intensity", &editedSettings.m_bloomIntensity, 0.0f, 10.0f, 0.1f, NullCB, "Intensity of blurring." );
		bk.AddSeparator();
		bk.AddSlider("Exposure diff. (Min)", &g_bloomThresholdExposureDiffMin, 0.0f, 64.0f, 0.01f);
		bk.AddSlider("Exposure diff. (Max)", &g_bloomThresholdExposureDiffMax, 0.0f, 64.0f, 0.01f);
		bk.AddSeparator();
		bk.AddSlider("Bloom Large Blur Blend Mult", &g_bloomLargeBlurBlendMult, 0.0f, 5.0f, 0.01f);
		bk.AddSlider("Bloom Medium Blur Blend Mult", &g_bloomMedBlurBlendMult, 0.0f, 5.0f, 0.01f);
		bk.AddSlider("Bloom Small Blur Blend Mult", &g_bloomSmallBlurBlendMult, 0.0f, 5.0f, 0.01f);		
	bk.PopGroup();

	bk.PushGroup("Tone mapping", false);
		toneMapInfo = bk.AddText("Info", toneMapInfoStr, 128);
		bk.AddText("Current Lum", g_adaptation.GetUpdateLumPtr(), true);
		bk.AddText("Exposure", g_adaptation.GetUpdateExposurePtr(), true);
		bk.AddSlider("Exposure Tweak", &editedSettings.m_exposureTweak, -32.0f, 32.0f, 0.01f, NullCB, "Tweak to exposure from timecycle");
		bk.AddToggle("Exposure Tweak Enabled", &g_enableExposureTweak);
		bk.AddToggle("Force Exposure Readback", &g_forceExposureReadback);
		bk.AddSlider("Exposure Push", &editedSettings.m_exposurePush, -1.0f, 1.0f, 0.01f, NullCB, "Tweak to exposure to pull/push away form 0");
		bk.AddVector("Filmic (A, B, C, D)", &editedSettings.m_filmicParams[0], -16.0f, 16.0f, 0.01f, NullCB, "Filmic parameters" );        
		bk.AddVector("Filmic (E, F, W, -)", &editedSettings.m_filmicParams[1], -16.0f, 16.0f, 0.01f, NullCB, "Filmic parameters" );        
		bk.AddSlider("Exposure (Scale)", &editedSettings.m_exposureCurveA, -256.0f, 256.0f, 0.001f);        
		bk.AddSlider("Exposure (Power)", &editedSettings.m_exposureCurveB,  1e-6f, 256.0f, 0.001f);        
		bk.AddSlider("Exposure (Max)", &editedSettings.m_exposureCurveOffset, -256.0f, 256.0f, 0.001f);        
		bk.AddSlider("Min exposure", &editedSettings.m_exposureMin, -16.0f, 16.0f, 0.1f);        
		bk.AddSlider("Max exposure", &editedSettings.m_exposureMax, -16.0f, 16.0f, 0.1f);        
	bk.PopGroup();

	bk.PushGroup("Adaption");
		bk.AddTitle("All values in f-stops per second");
		bk.AddSlider("Min step size", &editedSettings.m_adaptionMinStepSize, 0.0f, 150.0f, 0.01f);
		bk.AddSlider("Max step size", &editedSettings.m_adaptionMaxStepSize, 0.0f, 150.0f, 0.01f);
		bk.AddSlider("Step size multiplier", &editedSettings.m_adaptionStepMult, 0.0, 300.0f, 0.01f);
		bk.AddSlider("Threshold", &editedSettings.m_adaptionThreshold, 0.0f, 150.0f, 0.01f);
		bk.AddSlider("Num frames to average", &editedSettings.m_adaptionNumFramesAvg, 1, 32, 1);
		bk.AddToggle("Set exposure to target (no adaption)", &g_setExposureToTarget);
		bk.AddText("Info", adaptionInfoStr, 255, true);
	bk.PopGroup();

	bk.PushGroup("Noise", false);
		bk.AddSlider("Noise",&editedSettings.m_noise,0.0f,50.0f,0.01f);
		bk.AddSlider("Noise Size",&editedSettings.m_noiseSize, 0.0f, 1.0f,0.01f);

		bk.AddSlider("Scanline Intensity", &editedSettings.m_scanlineParams.x, 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Scanline Frequency 1", &editedSettings.m_scanlineParams.y, 0.0f, 4000.0f, 0.01f);
		bk.AddSlider("Scanline Frequency 2", &editedSettings.m_scanlineParams.z, 0.0f, 4000.0f, 0.01f);
		bk.AddSlider("Scanline Speed", &editedSettings.m_scanlineParams.w, 0.0f, 1.0f, 0.01f);
	bk.PopGroup();

	bk.PushGroup("Distance Blur (simple DOF)", false);
		bk.AddSlider("Start Distance",&editedSettings.m_dofParameters[0],0.0f,5000.0f,0.5f);
		bk.AddSlider("Blur Strength",&editedSettings.m_dofParameters[2],0.0f,1.0f,0.01f);
	bk.PopGroup();

	bk.PushGroup("Environment Blur", false);
		bk.AddSlider("In", &editedSettings.m_EnvironmentalBlurParams[0], -20.0f, g_MaxAdaptiveDofPlaneDistance, 0.01f);
		bk.AddSlider("Out", &editedSettings.m_EnvironmentalBlurParams[1], -20.0f, g_MaxAdaptiveDofPlaneDistance, 0.01f);
		bk.AddSlider("Blur Size", &editedSettings.m_EnvironmentalBlurParams[2], g_MinDofBlurDiskRadius, g_MaxDofBlurDiskRadius+1.0f, 1.0f);
	bk.PopGroup();

	bk.PushGroup("Vignetting", false);
		bk.AddSlider("Intensity", &editedSettings.m_vignettingParams[0], 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Radius", &editedSettings.m_vignettingParams[1], 0.0f, 2000.0f, 0.01f);
		bk.AddSlider("Contrast", &editedSettings.m_vignettingParams[2], 0.0f, 1.0f, 0.01f);
		bk.AddColor("Colour",  &editedSettings.m_vignettingColour, NullCB, "Vignette Colour", NULL, true);
	bk.PopGroup();

	bk.PushGroup("Colour Gradient", false);
		bk.AddColor("Top Colour", reinterpret_cast<Vector3*>(&editedSettings.m_lensGradientColTop), NullCB, "Top Gradient Colour", NULL, true);
		bk.AddColor("Middle Colour", reinterpret_cast<Vector3*>(&editedSettings.m_lensGradientColMiddle), NullCB, "Middle Gradient Colour", NULL, true);
		bk.AddColor("Bottom Colour", reinterpret_cast<Vector3*>(&editedSettings.m_lensGradientColBottom), NullCB, "Bottom Gradient Colour", NULL, true);

		bk.AddSlider("Top-Middle Gradient Midpoint", &editedSettings.m_lensGradientColTop[3], 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Middle Colour Midpoint", &editedSettings.m_lensGradientColMiddle[3], 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Middle-Bottom Gradient Midpoint", &editedSettings.m_lensGradientColBottom[3], 0.0f, 1.0f, 0.01f);
	bk.PopGroup();

	bk.PushGroup("Damage Overlay", false);
		bk.AddToggle("Enable Debug Rendering", &g_bulletImpactOverlay.m_bEnableDebugRender);
		bk.AddToggle("Render After Hud", &g_bulletImpactOverlay.m_drawAfterHud);
		bk.AddToggle("Disable Damage Overlay", &g_bulletImpactOverlay.m_disabled );
		bk.AddToggle("Trigger Bullet Impact", &g_DebugBulletImpactTrigger);
		bk.AddToggle("Use Cursor Position", &g_DebugBulletImpactUseCursorPos);
		bk.AddToggle("Use Endurance Indicator", &g_DebugBulletImpactUseEnduranceIndicator);
		bk.AddVector("Firing Weapon Pos", &g_DebugBulletImpactPos, -4000.0f, 4000.0f, 0.01f);
		bk.AddSlider("Screen Safe Zone Length", &g_bulletImpactOverlay.ms_screenSafeZoneLength, 0.0f, 1.0f, 0.001f);
		bk.AddSeparator();

		bk.PushGroup("Default Cam Settings");
			bk.AddSlider("Ramp Up Duration", &g_bulletImpactOverlay.ms_settings[0].rampUpDuration, 0, 5000, 1);
			bk.AddSlider("Hold Duration", &g_bulletImpactOverlay.ms_settings[0].holdDuration, 0, 5000, 1);
			bk.AddSlider("Ramp Down Duration", &g_bulletImpactOverlay.ms_settings[0].rampDownDuration, 0, 5000, 1);
			bk.AddSeparator();

			bk.AddSlider("Sprite Length", &g_bulletImpactOverlay.ms_settings[0].spriteLength, 0.001f, 0.5f, 0.001f);
			bk.AddSlider("Sprite Base Width", &g_bulletImpactOverlay.ms_settings[0].spriteBaseWidth, 0.001f, 0.5f, 0.001f);
			bk.AddSlider("Sprite Tip Width", &g_bulletImpactOverlay.ms_settings[0].spriteTipWidth, 0.001f, 0.5f, 0.001f);
			bk.AddSlider("Angle Scaling Mult", &g_bulletImpactOverlay.ms_settings[0].angleScalingMult, 0.0f, 10.0f, 0.001f);
			bk.AddSeparator();

			bk.AddColor("Colour Top", reinterpret_cast<Vector3*>(&g_bulletImpactOverlay.ms_settings[0].colourTop), NullCB, "Damage Colour Top", NULL, true);
			bk.AddColor("Colour Bottom", reinterpret_cast<Vector3*>(&g_bulletImpactOverlay.ms_settings[0].colourBottom), NullCB, "Damage Colour Bottom", NULL, true);
			bk.AddColor("Colour Top Endurance", reinterpret_cast<Vector3*>(&g_bulletImpactOverlay.ms_settings[0].colourTopEndurance), NullCB, "Damage Colour Top Endurance", NULL, true);
			bk.AddColor("Colour BottomEndurance ", reinterpret_cast<Vector3*>(&g_bulletImpactOverlay.ms_settings[0].colourBottomEndurance), NullCB, "Damage Colour Bottom Endurance", NULL, true);
			bk.AddSlider("Global Alpha Top", &g_bulletImpactOverlay.ms_settings[0].globalAlphaTop, 0.0f, 1.0f, 0.01f);
			bk.AddSlider("Global Alpha Bottom", &g_bulletImpactOverlay.ms_settings[0].globalAlphaBottom, 0.0f, 1.0f, 0.01f);
		bk.PopGroup();
		
		bk.PushGroup("First Person Cam Settings");
			bk.AddSlider("Ramp Up Duration", &g_bulletImpactOverlay.ms_settings[1].rampUpDuration, 0, 5000, 1);
			bk.AddSlider("Hold Duration", &g_bulletImpactOverlay.ms_settings[1].holdDuration, 0, 5000, 1);
			bk.AddSlider("Ramp Down Duration", &g_bulletImpactOverlay.ms_settings[1].rampDownDuration, 0, 5000, 1);
			bk.AddSeparator();

			bk.AddSlider("Sprite Length", &g_bulletImpactOverlay.ms_settings[1].spriteLength, 0.001f, 0.5f, 0.001f);
			bk.AddSlider("Sprite Base Width", &g_bulletImpactOverlay.ms_settings[1].spriteBaseWidth, 0.001f, 0.5f, 0.001f);
			bk.AddSlider("Sprite Tip Width", &g_bulletImpactOverlay.ms_settings[1].spriteTipWidth, 0.001f, 0.5f, 0.001f);
			bk.AddSlider("Angle Scaling Mult", &g_bulletImpactOverlay.ms_settings[1].angleScalingMult, 0.0f, 10.0f, 0.001f);
			bk.AddSeparator();

			bk.AddColor("Colour Top", reinterpret_cast<Vector3*>(&g_bulletImpactOverlay.ms_settings[1].colourTop), NullCB, "Damage Colour Top", NULL, true);
			bk.AddColor("Colour Bottom", reinterpret_cast<Vector3*>(&g_bulletImpactOverlay.ms_settings[1].colourBottom), NullCB, "Damage Colour Bottom", NULL, true);
			bk.AddColor("Colour Top Endurance", reinterpret_cast<Vector3*>(&g_bulletImpactOverlay.ms_settings[1].colourTopEndurance), NullCB, "Damage Colour Top Endurance", NULL, true);
			bk.AddColor("Colour BottomEndurance ", reinterpret_cast<Vector3*>(&g_bulletImpactOverlay.ms_settings[1].colourBottomEndurance), NullCB, "Damage Colour Bottom Endurance", NULL, true);
			bk.AddSlider("Global Alpha Top", &g_bulletImpactOverlay.ms_settings[1].globalAlphaTop, 0.0f, 1.0f, 0.01f);
			bk.AddSlider("Global Alpha Bottom", &g_bulletImpactOverlay.ms_settings[1].globalAlphaBottom, 0.0f, 1.0f, 0.01f);
		bk.PopGroup();
	bk.PopGroup();

	bk.PushGroup("Lens Distortion/Chromatic Aberration", false);
		bk.AddSlider("Sniper Scope Override Strength", &g_fSniperScopeOverrideStrength, 0.0f, 1.0f, 0.001f);
		bk.AddSlider("Lens Distortion Coefficient", &editedSettings.m_distortionParams[0], -1.0f, 1.0f, 0.001f);
		bk.AddSlider("Lens Distortion Cube Coefficient", &editedSettings.m_distortionParams[1], -1.0f, 1.0f, 0.001f);
		bk.AddSlider("Colour Aberration Coefficient", &editedSettings.m_distortionParams[2], -1.0f, 1.0f, 0.001f);
		bk.AddSlider("Colour Aberration Cube Coefficient", &editedSettings.m_distortionParams[3], -1.0f, 1.0f, 0.001f);
		bk.AddToggle("Disable On Kill Effect", &g_pedKillOverlay.m_disable);
		bk.AddToggle("Trigger Test Pulse Effect", &g_bDebugTriggerPulseEffect);
		bk.AddVector("Pulse Effect Params", &g_debugPulseEffectParams, -1.0f, 1.0f, 0.001f);
		bk.AddSlider("Pulse Effect Ramp Up Duration", &g_debugPulseEffectRampUpDuration, 0, 10000, 1);
		bk.AddSlider("Pulse Effect Hold Duration", &g_debugPulseEffectHoldDuration, 0, 10000, 1);
		bk.AddSlider("Pulse Effect Ramp Down Duration", &g_debugPulseEffectRampDownDuration, 0, 10000, 1);
	bk.PopGroup();

	bk.PushGroup("Blur Vignetting", false);
		bk.AddToggle("Run Tiled", &g_bRunBlurVignettingTiled);
		bk.AddSlider("Intensity", &editedSettings.m_blurVignettingParams[0], 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Radius", &editedSettings.m_blurVignettingParams[1], 0.0f, 2000.0f, 0.01f);
	bk.PopGroup();

	bk.PushGroup("Screen Blur Fade", false);
		bk.AddSlider("Time Cycle Driven Fade", &editedSettings.m_screenBlurFade, 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Screen Blur Fade Duration", g_screenBlurFade.GetDurationPtr(), 0.0f, 10000.0f, 0.01f);
		bk.AddToggle("Screen Blur Fade In", &gTriggerScreenBlurFadeIn);
		bk.AddToggle("Screen Blur Fade Out", &gTriggerScreenBlurFadeOut);
		bk.AddToggle("Disable Screen Blur Fade", &gDisableScreenBlurFadeOut);
	bk.PopGroup();

	bk.PushGroup("HQ DOF", false);
#if DOF_TYPE_CHANGEABLE_IN_RAG
		bk.AddCombo ("DOF Type",        (int*)&CurrentDOFTechnique, 3, dofTechniqueNames, ProcessDOFTechniqueChange);
		bk.AddSeparator();
#endif
		bk.AddToggle("Use Hi Dof", &editedSettings.m_HighDof );
		bk.AddToggle("Use Small Blur", &editedSettings.m_HighDofForceSmallBlur);
		bk.AddSlider("Near Start", &editedSettings.m_hiDofParameters[0], 0.0f, 25000.0f, 0.1f);
		bk.AddSlider("Near End", &editedSettings.m_hiDofParameters[1], 0.0f, 25000.0f, 0.1f);
		bk.AddSlider("Far Start", &editedSettings.m_hiDofParameters[2], 0.0f, 25000.0f, 1.0f);
		bk.AddSlider("Far End", &editedSettings.m_hiDofParameters[3], 0.0f, 25000.0f, 1.0f);
		bk.AddToggle("Use Shallow Dof", &editedSettings.m_ShallowDof);
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
		bk.AddSlider("COC Size", &DofKernelSizeVar, 0, DOF_MaxSampleRadius, 1);
		bk.AddSlider("Shallow DOF COC Size", &DofShallowKernelSizeVar, 0, DOF_MaxSampleRadius, 1);
		bk.AddSeparator();
		bk.AddToggle("Draw COC Overlay", &g_DrawCOCOverlay );	
		bk.AddSlider("COC Overlay Alpha", &g_cocOverlayAlphaVal, 0.0f, 1.0f, 0.01f);
		bk.AddSeparator();
		bk.AddToggle("Use Pre-Alpha Depth", &DofUsePreAlphaDepth);
		bk.AddSeparator();
		bk.AddSlider("Sky weight modifier", &DofSkyWeightModifier, 0.01f, 1.0f, 0.01f);
		bk.AddSeparator();
		bk.AddToggle("Dof Enable luminance filtering", &DofLumFilteringEnable);		
		bk.AddSlider("Dof Luminance contrast threshold", &DofLumContrastThreshold, 0.0f, 100.0f, 1.0f);
		bk.AddSlider("Dof Maximum luminance", &DofMaxLuminance, 0.0f, 100.0f, 0.1f);
		bk.AddSeparator();
		bk.PushGroup("Cutscenes");
			bk.AddSeparator();
			bk.AddToggle("Use Cutscene DOF Override", &editedSettings.m_DOFOverrideEnabled);
			bk.AddSlider("Cutscene DOF Override", &editedSettings.m_DOFOverride, 1.0f, DOF_MaxSampleRadius, 0.01f);
			bk.AddSeparator();
			g_cutsceneDofTODOverride.AddWidgets(bk);
		bk.PopGroup();		


		//Hard coded this to allow loop unrolling for optimisations.
 //#if COC_SPREAD
 //		bk.AddSlider("Near blur COC Size", &DofCocSpreadKernelRadiusVar, 0, DOF_MaxSampleRadiusCOC, 1);
 //#endif
#endif
		
#if BOKEH_SUPPORT
		if( BokehEnable )
		{
			bk.AddSeparator();
			bk.PushGroup("Bokeh", true);
			bk.AddToggle("Enable", &BokehEnable);
			bk.AddToggle("Debug Overlay", &BokehDebugOverlay);
			bk.AddText("Bokeh count", bokehCountString, 64);

			bk.AddSlider("Brightness Threshold Min",&editedSettings.m_BokehBrightnessThresholdMin,0.0f,10.0,0.01f);
			bk.AddSlider("Brightness Threshold Max",&editedSettings.m_BokehBrightnessThresholdMax,0.0f,10.0,0.01f);
			bk.AddSlider("Brightness Exposure Range Min",&BokehBrightnessExposureMin,-5.0f,5.0f,0.1f);
			bk.AddSlider("Brightness Exposure Range Max",&BokehBrightnessExposureMax,-5.0f,5.0f,0.1f);
			bk.AddSlider("Brightness Threshold Fade Range Min", &editedSettings.m_BokehBrightnessFadeThresholdMin, 0.0f, 10.0f, 0.01f);
			bk.AddSlider("Brightness Threshold Fade Range Max", &editedSettings.m_BokehBrightnessFadeThresholdMax, 0.0f, 10.0f, 0.01f);

			bk.AddSeparator();
			bk.AddSlider("Shape override test",&bokehShapeOverride,-1,15,1);
			bk.AddSlider("Shape Exposure Range Min",&BokehShapeExposureRangeMin,-5.0f,5.0f,0.1f);
			bk.AddSlider("Shape Exposure Range Max",&BokehShapeExposureRangeMax,-5.0f,5.0f,0.1f);

			bk.AddSeparator();

			bk.AddSlider("COC Threhold",&BokehBlurThresholdVal,0.0f,5.0,0.05f);
#if !(DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION)
			bk.AddSlider("Max Size",&MaxBokehSizeVar,0.0f,100.0,1.0f);
#endif
			bk.AddSlider("Size Multiplier", &bokehSizeMultiplier, 1.0f, 10.0f, 0.01f);

			bk.AddSlider("Global Alpha", &BokehGlobalAlpha, 0.0f, 1.0f, 0.01f);
			bk.AddSlider("Alpha Cutoff", &BokehAlphaCutoff, 0.0f, 1.0f, 0.15f);

			bk.PopGroup();
		}
#endif

	bk.PopGroup();

	bk.PushGroup("Motion Blur", false);
		#if DEBUG_MOTIONBLUR
			bk.AddToggle("Debug Motion Blur", &g_debugPrintMotionBlur);
		#endif
		bk.AddSeparator();
		bk.AddToggle("Default Motion Blur Enabled", &g_DefaultMotionBlurEnabled);
		bk.AddSlider("Default Motion Blur Strength", &g_DefaultMotionBlurStrength,  0.0f, 1.0f, 0.01f, NullCB, "Def Motion Blur Strength" );  
		bk.AddSeparator();
		bk.AddSlider("Directional Blur Length", &editedSettings.m_directionalBlurLength,  0.0f, 10.0f, 0.1f, NullCB, "Blurs length." );        
		bk.AddSlider("Directional Blur Ghosting Fade", &editedSettings.m_directionalBlurGhostingFade,  0.0f, 1.0f, 0.1f, NullCB, "Ghosting fade." );      
		bk.AddSlider("Directional Blur Max. Velocity Multiplier", &editedSettings.m_directionalBlurMaxVelocityMult, 0.0f, 10.0f, 0.1f, NullCB, "Max vel multiplier" );
	bk.PopGroup();

	bk.PushGroup("Night Vision", false);
		bk.AddToggle("Enable", &editedSettings.m_nightVision);
		bk.AddToggle("Override", &g_overrideNightVision);
		bk.AddSeparator();

		bk.AddSlider("Low Lum", &editedSettings.m_lowLum, 0.0f, 2.0f, 0.01f);
		bk.AddSlider("High Lum", &editedSettings.m_highLum, 0.0f, 2.0f, 0.01f);
		bk.AddSlider("Top Lum", &editedSettings.m_topLum, 0.0f, 2.0f, 0.01f);

		bk.AddSlider("Scaler Lum", &editedSettings.m_scalerLum, 0.0f, 256.0f, 0.01f);

		bk.AddSlider("Lum Offset", &editedSettings.m_offsetLum, -2.0f, 2.0f, 0.01f);
		bk.AddSlider("Low Lum Offset", &editedSettings.m_offsetLowLum, -2.0f, 2.0f, 0.01f);
		bk.AddSlider("High Lum Offset", &editedSettings.m_offsetHighLum, -2.0f, 2.0f, 0.01f);

		bk.AddSlider("Noise", &editedSettings.m_noiseLum, 0.0f, 1.0f, 0.01f);
		bk.AddSlider("Low Lum Noise", &editedSettings.m_noiseLowLum, 0.0f, 1.0f, 0.01f);
		bk.AddSlider("High Lum Noise", &editedSettings.m_noiseHighLum, 0.0f, 1.0f, 0.01f);

		bk.AddSlider("Bloom", &editedSettings.m_bloomLum, 0.0f, 4.0f, 0.01f);

		bk.AddColor("Color",reinterpret_cast<Vector3*>(&editedSettings.m_colorLum), NullCB );
		bk.AddColor("Low Lum Color",reinterpret_cast<Vector3*>(&editedSettings.m_colorLowLum), NullCB );
		bk.AddColor("High Lum Color",reinterpret_cast<Vector3*>(&editedSettings.m_colorHighLum), NullCB );
	bk.PopGroup();

	bk.PushGroup("Heat Haze", false);
		bk.AddSlider("Start range",&editedSettings.m_HeatHazeParams.x,0.0f,5000.0f,0.1f);
		bk.AddSlider("Far range",&editedSettings.m_HeatHazeParams.y,0.0f,5000.0f,0.1f);
		bk.AddSlider("Min Intensity", &editedSettings.m_HeatHazeParams.z, 0.0f, 2.0f, 0.01f);
		bk.AddSlider("Max Intensity", &editedSettings.m_HeatHazeParams.w, 0.0f, 2.0f, 0.01f);

		bk.AddSlider("Displacement Scale U", &editedSettings.m_HeatHazeOffsetParams.x, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("Displacement Scale V", &editedSettings.m_HeatHazeOffsetParams.y, -128.0f, 128.0f, 0.01f);

		bk.AddSlider("Tex1 U scale", &editedSettings.m_HeatHazeTex1Params.x, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("Tex1 V scale", &editedSettings.m_HeatHazeTex1Params.y, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("Tex1 U offset", &editedSettings.m_HeatHazeTex1Params.z, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("Tex1 V offset", &editedSettings.m_HeatHazeTex1Params.w, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("Tex2 U scale", &editedSettings.m_HeatHazeTex2Params.x, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("Tex2 V scale", &editedSettings.m_HeatHazeTex2Params.y, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("Tex2 U offset", &editedSettings.m_HeatHazeTex2Params.z, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("Tex2 V offset", &editedSettings.m_HeatHazeTex2Params.w, -128.0f, 128.0f, 0.01f);

		bk.AddSlider("Tex1 U Frequency offset",	&editedSettings.m_HeatHazeTex1Anim.x,-10.0f * PI, +10.0f * PI, PI/10.0f);
		bk.AddSlider("Tex1 U Frequency", &editedSettings.m_HeatHazeTex1Anim.y,-10.0f * PI, +10.0f * PI, PI/10.0f);
		bk.AddSlider("Tex1 U Amplitude", &editedSettings.m_HeatHazeTex1Anim.z,-64.0f,64.0f,0.1f);
		bk.AddSlider("Tex1 V Scrolling speed", &editedSettings.m_HeatHazeTex1Anim.w,-64.0f,64.0f,0.1f);

		bk.AddSlider("Tex2 U Frequency offset",	&editedSettings.m_HeatHazeTex2Anim.x,-10.0f * PI, +10.0f * PI, PI/10.0f);
		bk.AddSlider("Tex2 U Frequency", &editedSettings.m_HeatHazeTex2Anim.y,-10.0f * PI, +10.0f * PI, PI/10.0f);
		bk.AddSlider("Tex2 U Amplitude", &editedSettings.m_HeatHazeTex2Anim.z,-64.0f,64.0f,0.1f);
		bk.AddSlider("Tex2 V Scrolling speed", &editedSettings.m_HeatHazeTex2Anim.w,-64.0f,64.0f,0.1f);

		bk.AddSlider("extra Z", &editedSettings.m_HeatHazeOffsetParams.z, -128.0f, 128.0f, 0.01f);
		bk.AddSlider("extra W", &editedSettings.m_HeatHazeOffsetParams.w, -128.0f, 128.0f, 0.01f);
	bk.PopGroup();

	bk.PushGroup("SeeThrough", false);
		bk.AddToggle("Enable", &editedSettings.m_seeThrough);
		bk.AddSeparator();
		bk.AddSlider("Depth Start", &editedSettings.m_SeeThroughDepthParams.x, 0.0f, 6000.0f,0.1f);
		bk.AddSlider("Depth End", &editedSettings.m_SeeThroughDepthParams.y, 0.0f, 6000.0f,0.1f);
		bk.AddSlider("Thickness", &editedSettings.m_SeeThroughDepthParams.z, 1.0f, 10000.0f,0.1f);
		bk.AddSlider("Min Noise", &editedSettings.m_SeeThroughParams.x, 0.0f, 2.0f,0.1f);
		bk.AddSlider("Max Noise", &editedSettings.m_SeeThroughParams.y, 0.0f, 2.0f,0.1f);
		bk.AddSlider("HiLight Intensity", &editedSettings.m_SeeThroughParams.z, 0.0f, 10.0f,0.1f);
		bk.AddSlider("HiLight Noise", &editedSettings.m_SeeThroughParams.w, 0.0f, 2.0f,0.1f);
		bk.AddColor("Near Color",&editedSettings.m_SeeThroughColorNear);
		bk.AddColor("Far Color",&editedSettings.m_SeeThroughColorFar);
		bk.AddColor("Visible Color Base",&editedSettings.m_SeeThroughColorVisibleBase);
		bk.AddColor("Visible Color Warm",&editedSettings.m_SeeThroughColorVisibleWarm);
		bk.AddColor("Visible Color Hot",&editedSettings.m_SeeThroughColorVisibleHot);
	bk.PopGroup();

#if FILM_EFFECT
	bk.PushGroup("Film Effect", false);
	bk.AddToggle("Enabled", &g_EnableFilmEffect);
	bk.AddSlider("Base Distortion Coefficient", &g_BaseDistortionCoefficient, -2.0f, 2.0f, 0.01f);
	bk.AddSlider("Base Cube Distortion Coefficient", &g_BaseCubeDistortionCoefficient, -2.0f, 2.0f, 0.01f);
	bk.AddSlider("Chromatic Aberration Offset", &g_ChromaticAberrationOffset, -0.1f, 0.1f, 0.01f);
	bk.AddSlider("Chromatic Aberration Cube Offset", &g_ChromaticAberrationCubeOffset, -2.0f, 2.0f, 0.01f);
	bk.AddSlider("Film Noise", &g_FilmNoise, 0.0f, 0.2f, 0.01f);
	bk.AddSlider("Film Vignetting Intensity", &g_FilmVignettingIntensity,  0.0f, 2.0f, 0.01f);
	bk.AddSlider("Film Vignetting Radius", &g_FilmVignettingRadius, 0.0f, 40.0f, 0.01f);
	bk.AddSlider("Film Vignetting Contrast", &g_FilmVignettingContrast, 0.0f, 2.0f, 0.01f);
	bk.PopGroup();
#endif

#if ADAPTIVE_DOF
	AdaptiveDepthOfField.AddWidgets(bk, editedSettings);
#endif

#if USE_SCREEN_WATERMARK
	bk.PushGroup("Screen Watermark");
	bk.AddToggle("Enable", &ms_watermarkParams.bForceWatermark);
	bk.AddSeparator();
	bk.AddSlider("Day Alpha", &ms_watermarkParams.alphaDay, 0.01f, 1.0f, 0.001f);
	bk.AddSlider("Night Alpha", &ms_watermarkParams.alphaNight, 0.01f, 1.0f, 0.001f);
	bk.AddSeparator();
	bk.AddColor("Text Color", &ms_watermarkParams.textColor);
	bk.AddVector("Text Pos", &ms_watermarkParams.textPos, 0.0f, 2000.0f, 0.001f);
	bk.AddSlider("Text Size", &ms_watermarkParams.textSize, 0.1f, 200.0f, 0.001f);
	bk.AddToggle("Use Outline", &ms_watermarkParams.useOutline);

#if	DISPLAY_NETWORK_INFO
	bk.AddColor("Net Info Text Color", &ms_watermarkParams.netTextColor);
	bk.AddVector("Net Info Text Pos", &ms_watermarkParams.netTextPos, 0.0f, 2000.0f, 0.001f);
	bk.AddSlider("Net Info Text Size", &ms_watermarkParams.netTextSize, 0.1f, 200.0f, 0.001f);
#endif

	bk.AddSeparator();
	bk.AddToggle("Use Debug String", &ms_watermarkParams.bUseDebugText);
	bk.AddText("Debug String", &ms_watermarkParams.debugText[0], 16);

	bk.PopGroup();
#endif

	PHONEPHOTOEDITOR.AddWidgets(bk);

	LENSARTEFACTSMGR.AddWidgets(bk);
	bk.UnSetCurrentGroup(*ms_postFxGroup);
}

void PostFX::AddWidgets(bkBank &bk)
{
	ms_postFxGroup = bk.PushGroup("Post FX", false);
	ms_addPostFxWidgets = bk.AddButton("Create PostFX widgets", CFA1(PostFX::AddWidgetsOnDemand));
	bk.PopGroup();

	//for screen shots
	bkBank *pBank = BANKMGR.FindBank("Optimization"); 
	pBank->AddToggle("turn off depth of field",&NoDOF );
	pBank->AddToggle("turn off motion blur",&NoMotionBlur );
	pBank->AddToggle("turn off bloom", &NoBloom);
}

bool& PostFX::GetSimplePfxRef()
{
	return g_ForceUseSimple;
}
#endif //BANK

void PostFX::SetFograyParams(bool bShadowActive)
{
	PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetUpdateThreadParams();
#if __BANK
	if (g_Override==false)
#endif // __BANK
	{
		const tcKeyframeUncompressed& currKeyframe = (!sysThreadType::IsRenderThread() ? g_timeCycle.GetCurrUpdateKeyframe() : g_timeCycle.GetCurrRenderKeyframe());

		settings.m_fograyContrast = currKeyframe.GetVar(TCVAR_FOG_FOGRAY_CONTRAST);
		settings.m_fograyIntensity = currKeyframe.GetVar(TCVAR_FOG_FOGRAY_INTENSITY);
		settings.m_fograyDensity = currKeyframe.GetVar(TCVAR_FOG_FOGRAY_DENSITY);
	}
	settings.m_bShadowActive = bShadowActive;
	CRenderPhaseCascadeShadows::ComputeFograyFadeRange(settings.m_fograyFadeStart, settings.m_fograyFadeEnd); 

	settings.m_bFogRayMiniShadowMapGenerated = (settings.m_fograyIntensity > 0.0f) && settings.m_bShadowActive;
}

void PostFX::LockAdaptiveDofDistance(bool lock)
{
#if ADAPTIVE_DOF
	AdaptiveDepthOfField.SetLockFocusDistance(lock);
#else 
	(void)lock;
#endif
}

// DOF parameters
void PostFX::SetDofParams()
{

#if __BANK
	if (g_Override==false)
#endif // __BANK
	{
		PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetUpdateThreadParams();

		// sniper sight first
		if (settings.m_sniperSightActive)
		{
			settings.m_HighDof = true;
			settings.m_ShallowDof = true;
			settings.m_hiDofParameters = settings.m_sniperSightDOFParams;

			return;
		}

		const tcKeyframeUncompressed& currKeyframe = (!sysThreadType::IsRenderThread() ? g_timeCycle.GetCurrUpdateKeyframe() : g_timeCycle.GetCurrRenderKeyframe());
		const camFrame& frame = gVpMan.GetCurrentGameViewportFrame();

		// TBR: might want to define a new interface to set high dof
		settings.m_AdaptiveDofCustomPlanesBlendLevel = frame.GetDofPlanesBlendLevel();
		if( settings.m_screenBlurFade > 0.0 )
			settings.m_AdaptiveDofCustomPlanesBlendLevel = settings.m_screenBlurFade;

		const bool camHiDof = settings.m_AdaptiveDofCustomPlanesBlendLevel >= (1.0f - SMALL_FLOAT);
		const bool tcHiDof = currKeyframe.GetVar(TCVAR_DOF_ENABLE_HQDOF) > 0.5f;

		settings.m_AdaptiveDof = false;
		settings.m_HighDof = camHiDof || tcHiDof;

		settings.m_EnvironmentalBlurParams.x = currKeyframe.GetVar(TCVAR_ENV_BLUR_IN);
		settings.m_EnvironmentalBlurParams.y = currKeyframe.GetVar(TCVAR_ENV_BLUR_OUT);
		settings.m_EnvironmentalBlurParams.z = currKeyframe.GetVar(TCVAR_ENV_BLUR_SIZE);

	#if GTA_REPLAY
		// TBR: This logic should be in the camera code, and here we just do what camera flags say.
		// If replay's active and the DOF mode has been changed to anything other than default, then
		// let DOF have priority over environmental blur (which is probably being used by one of the filters).
		const bool bReplayActive = CReplayMgr::IsPlaying() || CReplayMgr::IsEditModeActive();
		if (bReplayActive)
		{
			bool bHasDofChanged = false;
			const IReplayMarkerStorage* pMarkerStorage = CReplayMarkerContext::GetMarkerStorage();
			const sReplayMarkerInfo* pMarkerInfo = (pMarkerStorage ? pMarkerStorage->TryGetMarker(camReplayDirector::GetCurrentMarkerIndex()) : NULL);
			if (pMarkerInfo)
			{
				bHasDofChanged = (pMarkerInfo->GetFocusMode() != DEFAULT_MARKER_DOF_FOCUS || pMarkerInfo->GetDofMode() != DEFAULT_MARKER_DOF_MODE);
			}
			if (bHasDofChanged)
			{
				settings.m_EnvironmentalBlurParams.x = 10000.000f;
				settings.m_EnvironmentalBlurParams.y = 10000.000f;
				settings.m_EnvironmentalBlurParams.z = 1.0f;
			}
		}
	#endif //GTA_REPLAY

		settings.m_AdaptiveDofBlurDiskRadiusPowerFactorNear = frame.GetDofBlurDiskRadiusPowerFactorNear();

		const bool scriptHiDof = settings.m_HighDofScriptOverrideToggle;

		settings.m_DOFOverride = 1.0f;
		settings.m_DOFOverrideEnabled = false;

		if ( scriptHiDof )
		{
			settings.m_HighDof = settings.m_HighDofScriptOverrideEnableDOF;
			settings.m_ShallowDof = false;
			settings.m_HighDofForceSmallBlur = false;
		}
		else if (frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldOverrideDofBlurDiskRadius))
		{
			settings.m_DOFOverride = frame.GetDofBlurDiskRadius(); 
			settings.m_DOFOverrideEnabled = true;
		}

		settings.m_HighDof = settings.m_HighDof;

#if RSG_PC
		if(CReplayMgr::IsEditModeActive() && !PostFX::UseDOFInReplay())
		{
			settings.m_HighDof = false;
		}
#endif

		if(settings.m_HighDof)
		{
			if ( scriptHiDof )
			{
				settings.m_hiDofParameters = settings.m_hiDofScriptParameters;
			}
			else if( camHiDof )
			{
				//Use a high-quality four-plane DOF effect.
				frame.ComputeDofPlanes(settings.m_hiDofParameters);

				settings.m_HighDofForceSmallBlur = frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseLightDof);
			
				// light dof version excludes the shallow mode
				settings.m_ShallowDof = (settings.m_HighDofForceSmallBlur == false && frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseShallowDof));
			}
			else if( tcHiDof )
			{
				settings.m_hiDofParameters[0] = currKeyframe.GetVar(TCVAR_DOF_HQDOF_NEARPLANE_OUT);
				settings.m_hiDofParameters[1] = currKeyframe.GetVar(TCVAR_DOF_HQDOF_NEARPLANE_IN);
				settings.m_hiDofParameters[2] = currKeyframe.GetVar(TCVAR_DOF_HQDOF_FARPLANE_OUT);
				settings.m_hiDofParameters[3] = currKeyframe.GetVar(TCVAR_DOF_HQDOF_FARPLANE_IN);
				
				settings.m_HighDofForceSmallBlur = currKeyframe.GetVar(TCVAR_DOF_HQDOF_SMALLBLUR) > 0.5f;
				settings.m_ShallowDof = currKeyframe.GetVar(TCVAR_DOF_HQDOF_SHALLOWDOF) > 0.5f;
			}
			else
			{
				Assertf(settings.m_HighDof == false,"HiDof is on, but no one turned it on");
			}
		}
#if ADAPTIVE_DOF
		else if(AdaptiveDepthOfField.IsEnabled())
		{
			//Use a high-quality four-plane DOF effect.
			frame.ComputeDofPlanes(settings.m_hiDofParameters);

			// light dof version excludes the shallow mode
			settings.m_ShallowDof = (settings.m_HighDofForceSmallBlur == false && frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseShallowDof));

#if ADAPTIVE_DOF_CPU
			bool bGaussian = (GRCDEVICE.GetDxFeatureLevel() >=1100) ? true : false;
#if RSG_PC
			bGaussian &= ((GET_POSTFX_SETTING(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX) >= CSettings::High) && (GRCDEVICE.IsStereoEnabled() == false)) ? true : false;
#endif // RSG_PC
			if (!bGaussian && UseAdaptiveDofCPU())
			{
				AdaptiveDepthOfField.CalculateCPUParams(settings.m_hiDofParameters, settings.m_DOFOverride);
				settings.m_ShallowDof = (settings.m_HighDofForceSmallBlur == false && CPhoneMgr::CamGetShallowDofModeState());
			}
#endif

			settings.m_AdaptiveDof = true;

#if RSG_PC
			if(CReplayMgr::IsEditModeActive() && !PostFX::UseDOFInReplay())
			{
				settings.m_AdaptiveDof = false;
			}
			else
			{
				if (GET_POSTFX_SETTING(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX) < CSettings::High)
				{
					if (!UseAdaptiveDofCPU())
					{
						settings.m_AdaptiveDof = false;
					}
				}
			}
#endif // RSG_PC
		}
#endif
		else
		{
			settings.m_SimpleDof = frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseSimpleDof);

			// Let timecycle drive simple DOF settings unless the camera frame
			// says otherwise
			if (settings.m_SimpleDof == false)
			{
				//Use a simple DOF effect; just a blur based upon the far DOF plane.
				const float timeCycleDofStrength = currKeyframe.GetVar(TCVAR_DOF_BLUR_FAR);
				const float timeCycleFarDof = currKeyframe.GetVar(TCVAR_DOF_FAR);
				float farDof = timeCycleFarDof;

				farDof = rage::Max(farDof, 0.1f);

				const float ooLength = 1.0f/(farDof+farDof*0.5f);
				settings.m_dofParameters.Set(farDof, ooLength, timeCycleDofStrength, 0.0f);
			}
			else
			{
				// TODO: grab these values from camera frame
				
				//const float farFarPlane = 1.0f;
				const float blurStrength = 1.0f;			
			
				Vector4 DofPlanes; 

				frame.ComputeDofPlanes(DofPlanes); 
				
				const float oofarFarPlane = 1.0f/(DofPlanes.w);
				const float farNearPlane = DofPlanes.z;

				settings.m_dofParameters.Set(farNearPlane, oofarFarPlane, blurStrength, 0.0f);
			}

			settings.m_ShallowDof = false;
			settings.m_HighDofForceSmallBlur = false;
		}
	}
}

void RunGaussianBlur9x9(grcRenderTarget* pDestTarget, grcRenderTarget* pDestTargetScratch, int numPasses)
{
	for (int i = 0; i < numPasses; i++)
	{
		// 9x9 Gaussian blur (horizontal)
		PostFXShader->SetVar(BloomTexelSizeId,Vector4(1.0f/float(pDestTarget->GetWidth()),1.0f/float(pDestTarget->GetHeight()),0.0, 0.0f));
		PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(pDestTarget->GetWidth()),1.0f/float(pDestTarget->GetHeight()),0.0, 0.0f));
		PostFXShader->SetVar(TextureID_0, pDestTarget);
		PostFXBlit(pDestTargetScratch, PostFXTechnique, pp_GaussBlur_Hor, false, true);

		// 9x9 Gaussian blur (vertical)
		PostFXShader->SetVar(TextureID_0, pDestTargetScratch);
		PostFXBlit(pDestTarget, PostFXTechnique, pp_GaussBlur_Ver, false, true);
	}
}

void ProcessBloomBuffer(grcRenderTarget* pDestTarget, const grcTexture* pSrcTarget, PostFXPass pass)
{
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(pSrcTarget->GetWidth()),1.0f/float(pSrcTarget->GetHeight()),0.0, 0.0f));
	PostFXShader->SetVar(TextureID_0, pSrcTarget);
	PostFXBlit(pDestTarget, PostFXTechnique, pass, false, false);
}

void ProcessBlurVignetting(grcRenderTarget* pDestTarget, const PostFX::PostFXParamBlock::paramBlock& settings)
{
	PF_PUSH_MARKER("ProcessBlurVignetting");

	grcRenderTarget* pFinalTarget = pDestTarget;
	grcRenderTarget* pBlurTarget = HalfScreen0;
	grcRenderTarget* pBlurScratchTarget = BloomBufferHalf1;

	// Downsample
	ProcessBloomBuffer(pBlurTarget, pDestTarget, pp_DownSampleBloom);

	// Blur Vignetting
	const int numPasses = 2;
	const float blurVignettingBias = 1.0f-settings.m_blurVignettingParams.x;
	const float blurVignettingRadius = settings.m_blurVignettingParams.y;
	PostFXShader->SetVar(BlurVignettingParamsID, Vector4(blurVignettingBias, blurVignettingRadius, 1.0f, 0.0f));

#if POSTFX_TILED_LIGHTING_ENABLED
	if (g_bRunBlurVignettingTiled && BANK_SWITCH(PostFX::g_UseTiledTechniques == true, CTiledLighting::IsEnabled()))
	{
		const char* pPassNameHor = NULL;
		const char* pPassNameVer = NULL;
		const char* pPassNameFinal = NULL;
	#if RAGE_TIMEBARS
		pPassNameHor = passName[pp_blur_vignetting_blur_hor_tiled];
		pPassNameVer = passName[pp_blur_vignetting_blur_ver_tiled];
		pPassNameFinal = passName[pp_blur_vignetting_tiled];
	#endif
	
		for (int i = 0; i < numPasses; i++)
		{
			// 9x9 Gaussian blur (horizontal)
			PostFXShader->SetVar(BloomTexelSizeId,Vector4(1.0f/float(pBlurTarget->GetWidth()),1.0f/float(pBlurTarget->GetHeight()),0.0, 0.0f));
			PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(pBlurTarget->GetWidth()),1.0f/float(pBlurTarget->GetHeight()),0.0, 0.0f));
			PostFXShader->SetVar(TextureID_0, pBlurTarget);
		#if __D3D11 || RSG_ORBIS
			PostFXShader->SetVar(TiledDepthTextureId, CTiledLighting::GetClassificationTexture());
		#endif // __D3D11 || RSG_ORBIS
			PostFXShader->SetVar(BlurVignettingParamsID, Vector4(blurVignettingBias, blurVignettingRadius, 1.0f, 0.0f));
		
			CTiledLighting::RenderTiles(pBlurScratchTarget, PostFXShader, PostFXTechnique, pp_blur_vignetting_blur_hor_tiled, pPassNameHor, false);

			// 9x9 Gaussian blur (vertical)
			PostFXShader->SetVar(TextureID_0, pBlurScratchTarget);
			CTiledLighting::RenderTiles(pBlurTarget, PostFXShader, PostFXTechnique, pp_blur_vignetting_blur_ver_tiled, pPassNameVer, false);
		}

		PostFXShader->SetVar(PostFxTexture3Id, pBlurTarget);
		PostFXShader->SetVar(BlurVignettingParamsID, Vector4(blurVignettingBias, blurVignettingRadius, 1.0f, 0.0f));
		CTiledLighting::RenderTiles(pFinalTarget, PostFXShader, PostFXTechnique, pp_blur_vignetting_tiled, pPassNameFinal, false);
	}
	else
#endif
	{
		RunGaussianBlur9x9(pBlurTarget, pBlurScratchTarget, numPasses);

		PostFXShader->SetVar(PostFxTexture3Id, pBlurTarget);
		PostFXBlit(pFinalTarget, PostFXTechnique, pp_blur_vignetting, false, false);
	}
	



	PF_POP_MARKER();
}

void BlurBloomBuffer(grcRenderTarget* BloomBufferDst, bool bSeeThroughPass = false)
{
	PF_PUSH_MARKER("BlurBloomBuffer");

	SetNonDepthFXStateBlocks();

	static int numBlurPasses2x	= 1;
	static int numBlurPasses4x	= 1;
	static int numBlurPasses8x	= 2;
	static int numBlurPasses16x = 2;

	PostFXPass upsamplePass = (bSeeThroughPass ? pp_BloomComposite_SeeThrough : pp_BloomComposite);
	PostFXPass downsamplePass = pp_DownSampleBloom;

	// Downsample 2x
	ProcessBloomBuffer(BloomBufferHalf0, BloomBufferDst, downsamplePass);
	// Blur 2x
	RunGaussianBlur9x9(BloomBufferHalf0, BloomBufferHalf1, numBlurPasses2x);
	
	// Downsample 2x to 4x
	ProcessBloomBuffer(BloomBufferQuarter0, BloomBufferHalf0, downsamplePass);
	// Blur 4x
	RunGaussianBlur9x9(BloomBufferQuarter0, BloomBufferQuarter1, numBlurPasses4x);
		
	// Downsample 4x to 8x
	ProcessBloomBuffer(BloomBufferEighth0, BloomBufferQuarter0, downsamplePass);
	// Blur 8x
	RunGaussianBlur9x9(BloomBufferEighth0, BloomBufferEighth1, numBlurPasses8x);
	
	// Downsample 8x to 16x
	ProcessBloomBuffer(BloomBufferSixteenth0, BloomBufferEighth0, downsamplePass);
	// Blur 16x
	RunGaussianBlur9x9(BloomBufferSixteenth0, BloomBufferSixteenth1, numBlurPasses16x);

	Vector4 BloomParams;
	BloomParams.Zero();

	// Upsample and composite 16x into 8x
	BloomParams.w = PostFX::g_bloomLargeBlurBlendMult;
	PostFXShader->SetVar(BloomParamsId, BloomParams);
	ProcessBloomBuffer(BloomBufferEighth0, BloomBufferSixteenth0, upsamplePass);
	// Upsample and composite 8x into 4x
	BloomParams.w = PostFX::g_bloomMedBlurBlendMult;
	PostFXShader->SetVar(BloomParamsId, BloomParams);
	ProcessBloomBuffer(BloomBufferQuarter0, BloomBufferEighth0, upsamplePass);
	// Upsample and composite 4x into 2x
	BloomParams.w = PostFX::g_bloomSmallBlurBlendMult;
	PostFXShader->SetVar(BloomParamsId, BloomParams);
	ProcessBloomBuffer(BloomBufferHalf0, BloomBufferQuarter0, upsamplePass);

	// Blur 2x again
	RunGaussianBlur9x9(BloomBufferHalf0, BloomBufferHalf1, numBlurPasses2x);

	// Upsample and composite 2x into BloomBufferDst
	ProcessBloomBuffer(BloomBufferDst, BloomBufferHalf0, upsamplePass);


	PF_POP_MARKER();
}

#if DOF_COMPUTE_GAUSSIAN
float CalculateGaussianWeight(int sampleDist, int kernel)
{
	float sigma = kernel / 3.0f;
	float g = 1.0f / sqrt(2.0f * PI * sigma * sigma);
	return (g * exp(-(sampleDist * sampleDist) / (2 * sigma * sigma)));
}

//weightOffset used to provide a stronger blur with less samples, without it you typically get values very small towards the edge of the blur which add very little contribution, this eliminates them.
void SetupGaussianWeights(grcBufferUAV& pWeightsBuffer, u32 weightOffset)
{
	int uMaxGaussianElements = DOF_MaxSampleRadius;
	for( u32 i = 0; i <= DOF_MaxSampleRadius; i++)
		uMaxGaussianElements += i+1;

	float pWeights[(DOF_MaxSampleRadius*DOF_MaxSampleRadius)+DOF_MaxSampleRadius];
	float* pWeightsDest = pWeights;
	u32 size = 0;
	//Store the offsets in the first DOF_MaxSampleRadius slots;
	u32 kernel = 1;
	for( kernel = 1; kernel <= DOF_MaxSampleRadius; kernel++)
	{
		int offset = DOF_MaxSampleRadius;
		for( int i = 1; i < kernel; i++)
			offset += (i+1);

		*pWeightsDest = (float)offset;
		pWeightsDest++;
		size++;
	}

	u32 kernelSize = 1;
	for( u32 i = kernelSize; i <= DOF_MaxSampleRadius; i++)
	{
		for( u32 j = 0; j <= i; j++)
		{
			*pWeightsDest = CalculateGaussianWeight(j, i + weightOffset);
			pWeightsDest++;
			size++;
		}
	}

	# if RSG_ORBIS
		const sce::Gnm::SizeAlign weightsSizeAlign = { uMaxGaussianElements * sizeof(float), 16 };
		pWeightsBuffer.Allocate( weightsSizeAlign, true, pWeights, sizeof(float) );
	# elif RSG_DURANGO
		pWeightsBuffer.Initialise( uMaxGaussianElements, sizeof(float), grcBindShaderResource|grcBindUnorderedAccess, NULL, grcBufferMisc_BufferStructured);
		float *pDest = (float *)pWeightsBuffer.Lock(grcsWrite, 0, uMaxGaussianElements, NULL);
		memcpy(pDest, pWeights, sizeof(float)*uMaxGaussianElements);
		pWeightsBuffer.Unlock(grcsWrite);
	# else // RSG_P	C
		pWeightsBuffer.Initialise( uMaxGaussianElements, sizeof(float), grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWrite, grcsBufferSync_None, pWeights, false, grcBufferMisc_BufferStructured );
	# endif
}
#endif //DOF_COMPUTE_GAUSSIAN

#if BOKEH_SUPPORT || DOF_COMPUTE_GAUSSIAN
void InitBokehDOF()
{
#if BOKEH_SORT_BITONIC_TRANSPOSE
	s_BokehMaxElements = BOKEH_SORT_TRANSPOSE_BLOCK_SIZE * BOKEH_SORT_BITONIC_BLOCK_SIZE;
#else
	s_BokehMaxElements = BOKEH_SORT_NUM_BUCKETS * BOKEH_SORT_BITONIC_BLOCK_SIZE;
#endif
	const u32 uMaxElements = s_BokehMaxElements;
	const u32 uBufferSize = uMaxElements * s_BokehBufferStride;
	if( s_BokehAllocatedSize != uBufferSize )
	{
		static ALIGNAS(16) u32 bufferInit[4] = {0,1,0,0};
# if RSG_ORBIS
		const sce::Gnm::SizeAlign accumSizeAlign = { uBufferSize, 16 };
		s_BokehAccumBuffer.Allocate( accumSizeAlign, true, NULL, s_BokehBufferStride );
		const sce::Gnm::SizeAlign indirectSizeAlign = { 16, sce::Gnm::kAlignmentOfIndirectArgsInBytes };
		s_indirectArgsBuffer.Allocate( indirectSizeAlign, true, bufferInit );

		const sce::Gnm::SizeAlign IndexSizeAlign = { uMaxElements * s_BokehIndexListBufferStride, 16 };
		s_BokehIndexListBuffer.Allocate( IndexSizeAlign, true, NULL, s_BokehIndexListBufferStride );
		s_bokehSortedIndexListBuffer.Allocate( IndexSizeAlign, true, NULL, s_BokehIndexListBufferStride );
#if BOKEH_SORT_BITONIC_TRANSPOSE
		s_bokehSortedIndexListBuffer2.Allocate( IndexSizeAlign, true, NULL, s_BokehIndexListBufferStride );
#endif //BOKEH_SORT_BITONIC_TRANSPOSE
		const sce::Gnm::SizeAlign bucketsSizeAlign = { BOKEH_SORT_NUM_BUCKETS * s_BokehBucketsNumAddedBufferStride, 16 };
		s_bokehNumAddedToBuckets.Allocate( bucketsSizeAlign, true, NULL, s_BokehBucketsNumAddedBufferStride );

# elif RSG_DURANGO
		s_BokehAccumBuffer.Initialise( uMaxElements, s_BokehBufferStride, grcBindShaderResource|grcBindUnorderedAccess, NULL, grcBufferMisc_BufferStructured, grcBuffer_UAV_FLAG_COUNTER);
		s_BokehIndexListBuffer.Initialise( uMaxElements, s_BokehIndexListBufferStride, grcBindShaderResource|grcBindUnorderedAccess, NULL, grcBufferMisc_BufferStructured, 0);
		s_bokehSortedIndexListBuffer.Initialise( uMaxElements, s_BokehIndexListBufferStride, grcBindShaderResource|grcBindUnorderedAccess, NULL, grcBufferMisc_BufferStructured, 0);
#if BOKEH_SORT_BITONIC_TRANSPOSE
		s_bokehSortedIndexListBuffer2.Initialise( uMaxElements, s_BokehIndexListBufferStride, grcBindShaderResource|grcBindUnorderedAccess, NULL, grcBufferMisc_BufferStructured, 0);
#endif //BOKEH_SORT_BITONIC_TRANSPOSE
		s_bokehNumAddedToBuckets.Initialise( BOKEH_SORT_NUM_BUCKETS, s_BokehBucketsNumAddedBufferStride, grcBindShaderResource|grcBindUnorderedAccess, NULL, grcBufferMisc_BufferStructured, 0);

		s_indirectArgsBuffer.Initialise( sizeof(int) * 4, sizeof(int), grcBindShaderResource, NULL, grcBufferMisc_DrawIndirectArgs);
		u32 *pDest = (u32 *)s_indirectArgsBuffer.Lock(grcsWrite, 0, 0, NULL);
		memcpy(pDest, bufferInit, sizeof(bufferInit));
		s_indirectArgsBuffer.Unlock(grcsWrite);
# else // RSG_P	C
		s_BokehAccumBuffer.Initialise( uMaxElements, s_BokehBufferStride, grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWrite, grcsBufferSync_None, NULL, false, grcBufferMisc_BufferStructured, grcBuffer_UAV_FLAG_COUNTER );
		s_indirectArgsBuffer.Initialise( 1, 16, grcBindNone, grcsBufferCreate_ReadWrite, grcsBufferSync_None, bufferInit, false, grcBufferMisc_DrawIndirectArgs );

		s_BokehIndexListBuffer.Initialise( uMaxElements, s_BokehIndexListBufferStride, grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWrite, grcsBufferSync_None, NULL, false, grcBufferMisc_BufferStructured, 0 );		
#if BOKEH_SORT_BITONIC_TRANSPOSE
		s_bokehSortedIndexListBuffer2.Initialise( uMaxElements, s_BokehIndexListBufferStride, grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWrite, grcsBufferSync_None, NULL, false, grcBufferMisc_BufferStructured, 0 );
#endif //BOKEH_SORT_BITONIC_TRANSPOSE
		s_bokehNumAddedToBuckets.Initialise( BOKEH_SORT_NUM_BUCKETS, s_BokehBucketsNumAddedBufferStride, grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWrite, grcsBufferSync_None, NULL, false, grcBufferMisc_BufferStructured, 0 );
		s_bokehSortedIndexListBuffer.Initialise( uMaxElements, s_BokehIndexListBufferStride, grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWrite, grcsBufferSync_None, NULL, false, grcBufferMisc_BufferStructured, 0 );
# endif
		s_BokehAllocatedSize = uBufferSize;
	}

#if DOF_COMPUTE_GAUSSIAN
	if( !sSetupGaussianWeights )
	{
		SetupGaussianWeights(s_GaussianWeightsBuffer, 0);
		SetupGaussianWeights(s_GaussianWeightsCOCBuffer, 4);
		sSetupGaussianWeights = true;
	}
#endif
}

void ProcessBokehPreDOF(const PostFX::PostFXParamBlock::paramBlock &settings, float screenBlurFadeLevel)
{
	const u32 width = VideoResManager::GetSceneWidth();
	const u32 height = VideoResManager::GetSceneHeight();

	Vec2V DOFTargetSizeVar = Vec2V((float)width,(float)height);					
	Vec2V RenderTargetSizeVar = Vec2V((float)width,(float)height);	

#if BOKEH_SORT_BITONIC_TRANSPOSE
	s_BokehMaxElements = BOKEH_SORT_TRANSPOSE_BLOCK_SIZE * BOKEH_SORT_BITONIC_BLOCK_SIZE;
#else
	s_BokehMaxElements = BOKEH_SORT_NUM_BUCKETS * BOKEH_SORT_BITONIC_BLOCK_SIZE;
#endif
	ASSERT_ONLY(const u32 uMaxElements = s_BokehMaxElements;)
	ASSERT_ONLY(const u32 uBufferSize = uMaxElements * s_BokehBufferStride;)
	Assertf(( s_BokehAllocatedSize == uBufferSize ), "InitBokehDOF"); 

	//Setup shader constants
	const Vector4 exposureParams(BokehBrightnessExposureMax, BokehBrightnessExposureMin, settings.m_BokehBrightnessThresholdMax, settings.m_BokehBrightnessThresholdMin);
	PostFXShader->SetVar(BokehBrightnessParams, exposureParams);
	PostFXShader->SetVar(DOFTargetSize, DOFTargetSizeVar);

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
	//Set the max bokeh size to the kernel size we then scale this by the coc in the shader
	if( CurrentDOFTechnique == dof_computeGaussian || CurrentDOFTechnique == dof_diffusion)
	{
		float kernelSize = (float)DofKernelSizeVar;
		if( settings.m_DOFOverrideEnabled )
			kernelSize = settings.m_DOFOverride;
		else if( settings.m_ShallowDof )
			kernelSize = (float)DofShallowKernelSizeVar;
		else if( settings.m_AdaptiveDof )
			kernelSize = (float)DofAdaptiveKernelSizeVar;

		if(PostFX::ExtraEffectsRequireBlur())
			kernelSize = Lerp(screenBlurFadeLevel, kernelSize, (float)DOF_MaxSampleRadius);

		if (settings.m_AdaptiveDof)
		{
			MaxBokehSizeVar = bokehSizeMultiplier;
		}
		else
		{
			MaxBokehSizeVar = kernelSize * bokehSizeMultiplier;
		}
	}
	else
#endif
	{
		float kernelSize = (float)DofKernelSizeVar;

		if( settings.m_DOFOverrideEnabled )
			kernelSize = settings.m_DOFOverride;

		if (settings.m_AdaptiveDof)
		{
			MaxBokehSizeVar = bokehSizeMultiplier;
		}
		else
		{
			MaxBokehSizeVar = kernelSize * bokehSizeMultiplier;
		}		
	}
	const Vector4 bokehParams(settings.m_BokehBrightnessFadeThresholdMin, settings.m_BokehBrightnessFadeThresholdMax, BokehBlurThresholdVal, MaxBokehSizeVar);
	PostFXShader->SetVar(BokehParams1, bokehParams);

	float bokehShapeOverrideVal = -1.0f;
	float debugOverlay = 0.0f;
#if __BANK
	bokehShapeOverrideVal = (float)bokehShapeOverride;
	debugOverlay = BokehDebugOverlay ? 1.0f : 0.0f;
#endif
	const Vector4 bokehParams2(BokehShapeExposureRangeMax, BokehShapeExposureRangeMin, bokehShapeOverrideVal, debugOverlay);
	PostFXShader->SetVar(BokehParams2, bokehParams2);

	PostFXShader->SetVar(BokehAlphaCutoffVar, BokehAlphaCutoff);
	PostFXShader->SetVar(BokehGlobalAlphaVar, BokehGlobalAlpha);

	PostFXShader->SetVar(RenderTargetSize, RenderTargetSizeVar);

	//render depth blur pass
	const grcTexture *backBuffer = CRenderTargets::GetBackBuffer();
#if DEVICE_MSAA
	if( GRCDEVICE.GetMSAA()>1 )
	{	
		backBuffer = CRenderTargets::GetBackBufferCopy(false);
	}
#endif	//DEVICE_MSAA	
	PostFXShader->SetVar(TextureID_0, backBuffer);	//linear sampling
	PostFXShader->SetVar(JitterTextureId,  backBuffer ); //points sampling

	grcRenderTarget* pDepth = CRenderTargets::GetDepthResolved();
	if( DofUsePreAlphaDepth )
		pDepth = CRenderTargets::GetDepthBuffer_PreAlpha();

	PostFXShader->SetVar(PostFxTexture2Id, pDepth);
	
#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
		PostFXShader->SetVar(PtfxDepthBuffer,  g_ptFxManager.GetPtfxDepthTexture() );
		PostFXShader->SetVar(PtfxAlphaBuffer,  g_ptFxManager.GetPtfxAlphaTexture() );	
	}
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	u32 pass = pp_Bokeh_DepthBlur;
#if ADAPTIVE_DOF_GPU

	if(settings.m_AdaptiveDof)
	{
#if ADAPTIVE_DOF_OUTPUT_UAV
		PostFXShader->SetVar(PostFXAdaptiveDOFParamsBufferVar, AdaptiveDepthOfField.GetParamsRT());
#else
		PostFXShader->SetVar(TextureID_v0, AdaptiveDepthOfField.GetParamsRT());
#endif //ADAPTIVE_DOF_OUTPUT_UAV

		Vector4 adaptiveDofEnvBlurParams(settings.m_EnvironmentalBlurParams);
		adaptiveDofEnvBlurParams.SetW(screenBlurFadeLevel);
		PostFXShader->SetVar(PostFXAdaptiveDofEnvBlurParamsVar, adaptiveDofEnvBlurParams);

		//Compute the blur radius required for any custom DOF planes.
		//NOTE: This is ugly, but necessary, due to the way animated cutscenes, scripts and the time-cycle system override the DOF planes.
		float customPlanesBlurDiskRadius = (float)DofKernelSizeVar;
		if(settings.m_DOFOverrideEnabled)
		{
			customPlanesBlurDiskRadius = settings.m_DOFOverride;
		}
		else if(settings.m_ShallowDof)
		{
			customPlanesBlurDiskRadius = (float)DofShallowKernelSizeVar;
		}

		Vector4 adaptiveDofCustomPlanesParams(settings.m_AdaptiveDofCustomPlanesBlendLevel, customPlanesBlurDiskRadius, settings.m_AdaptiveDofBlurDiskRadiusPowerFactorNear, 0.0f);
		PostFXShader->SetVar(PostFXAdaptiveDofCustomPlanesParamsVar, adaptiveDofCustomPlanesParams);

		pass = pp_Bokeh_DepthBlurAdaptive;
	}
#endif

	PostFXBlit(BokehDepthBlur, PostFXBokehTechnique, pass, false, false, 
#if RAGE_TIMEBARS		
		(bokehPassName)
#else
		NULL
#endif
		);

	PostFXShader->SetVar(FXAABackBuffer,  BokehDepthBlur );
	pass = pp_Bokeh_DepthBlurDownsample;
	PostFXBlit(BokehDepthBlurHalfRes, PostFXBokehTechnique, pass, false, false, 
#if RAGE_TIMEBARS		
		(bokehPassName)
#else
		NULL
#endif
		);	
}

void ProcessBokehPostDOF(const PostFX::PostFXParamBlock::paramBlock &settings)
{
	GRCDEVICE.ClearUAV(false, &s_bokehNumAddedToBuckets);
	GRCDEVICE.ClearUAV(false, &s_bokehSortedIndexListBuffer);
	
	//Generate Bokeh sprites in UAV for use in geometry shader
	PostFXShader->SetVar(GBufferTextureIdSSAODepth, BokehDepthBlur);
#if __D3D11
	const grcBufferUAV* UAVs[grcMaxUAVViews] = {
		&s_BokehAccumBuffer,
		&s_bokehSortedIndexListBuffer,
		&s_bokehNumAddedToBuckets
	};	
#endif
#if RSG_DURANGO || RSG_ORBIS
	PostFXShader->SetVarUAV(BokehOutputPointBufferVar, &s_BokehAccumBuffer, 0);
	PostFXShader->SetVarUAV(BokehSortedBufferVar, &s_bokehSortedIndexListBuffer, 0);
	PostFXShader->SetVarUAV(BokehSortOffsetsBufferVar, &s_bokehNumAddedToBuckets, 0);
#endif	//platforms

	//render depth blur pass
	const grcTexture *backBuffer = CRenderTargets::GetBackBuffer();
#if DEVICE_MSAA
	if( GRCDEVICE.GetMSAA()>1 )
	{	
		backBuffer = CRenderTargets::GetBackBufferCopy(false);
	}
#endif	//DEVICE_MSAA

	PostFXShader->SetVar(TextureID_0, backBuffer);	//linear sampling
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(backBuffer->GetWidth()),1.0f/float(backBuffer->GetHeight()),0.0, 0.0f));
	PostFXBlit(BokehPointsTemp, PostFXBokehTechnique, pp_Bokeh_Downsample, false, false,  
#if RAGE_TIMEBARS		
		(bokehPassName)
#else
	NULL
#endif
		,false
		);

	PostFXShader->SetVar(TextureID_0, BokehPointsTemp);	//linear sampling
	PostFXShader->SetVar(JitterTextureId,  BokehPointsTemp ); //points sampling
	if (BokehPointsTemp != NULL)
		PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(BokehPointsTemp->GetWidth()),1.0f/float(BokehPointsTemp->GetHeight()),0.0, 0.0f));

	PostFXShader->SetVar(TextureID_v0, PostFX::g_currentExposureRT);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default_WriteMaskNone);

	//HalfScreen0 not actually written to here as colour writes disabled - we still need its contents. Just re-using the target as its the correct size.
#if RSG_PC
	CShaderLib::SetStereoTexture();
#endif
	PostFXBlit(HalfScreen0, PostFXBokehTechnique, pp_Bokeh_Generation, false, false,  
#if RAGE_TIMEBARS		
		(bokehPassName)
#else

		NULL
#endif
		,false, Vector2(0.0f,0.0f), Vector2(1.0f,1.0f)
#if __D3D11
		,UAVs
#endif
		);


	grcStateBlock::SetBlendState(nonDepthFXBlendState);

	//Render Bokeh sprites with geometry shader
	PostFXShader->SetVar(HeatHazeTextureId, m_BokehShapeTextureSheet);
	PostFXShader->SetVar(TextureID_0, BokehDepthBlur);

	u32 bokehSpritePass = pp_Bokeh_Sprites;
#if ADAPTIVE_DOF_OUTPUT_UAV
	if( settings.m_AdaptiveDof)
		bokehSpritePass = pp_Bokeh_Sprites_Adaptive;
#endif //ADAPTIVE_DOF_OUTPUT_UAV

#if RSG_PC
	CShaderLib::SetStereoTexture();
#endif
	BokehBlitWithGeometryShader(BokehPoints, PostFXBokehTechnique, bokehSpritePass, false, true, 
#if RAGE_TIMEBARS		
		(bokehPassName)
#else
		NULL
#endif
		);
#if RSG_DURANGO && __BANK
	if (PostFX::g_DrawCOCOverlay && AltBokehDepthBlur)
	{
		ESRAMManager::ESRAMCopyOut(AltBokehDepthBlur, BokehDepthBlur);
	}
#endif
}

#endif //BOKEH_SUPPORT


#if DOF_DIFFUSION

void DiffusionDOFDraw(u32 shaderPass)
{
	// #if POSTFX_UNIT_QUAD
	// 		dofDiffusionShader->SetVar(quad::Position,	FastQuad::Pack(-1.f,1.f,1.f,-1.f));
	// 		dofDiffusionShader->SetVar(quad::TexCoords,	FastQuad::Pack(0.f,0.f,1.f,1.f));
	// 		dofDiffusionShader->SetVar(quad::Scale,		Vector4(1.0f,1.0f,1.0f,1.0f));
	// #endif
	AssertVerify(dofDiffusionShader->BeginDraw(grmShader::RMC_DRAW,true,dofDiffusiontechnique)); // MIGRATE_FIXME
	dofDiffusionShader->Bind((int)shaderPass);
	// #if POSTFX_UNIT_QUAD
	// 		FastQuad::Draw(true);
	// #else
	Color32 color(1.0f,1.0f,1.0f,1.0f);
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f,color);
	//#endif
	dofDiffusionShader->UnBind();
	dofDiffusionShader->EndDraw();
}
void ProcessDiffusionDOFCS( grcRenderTarget* pOutputImage)
{
	grcTextureFactory& textureFactory = grcTextureFactory::GetInstance(); 

	PIXBegin(0, "Diffusion DOF");

	u32 screenWidth = GRCDEVICE.GetWidth();
	u32 screenHeight = GRCDEVICE.GetHeight();
	//bool bPacked = false;
	u32 shaderPass = 0;
	s32 i = 0;

	dofDiffusionShader->SetVar( dofDiffusion_DiffusionRadiusVar, (float)DofKernelSizeVar);

	grcRenderTarget* pSourceImage = CRenderTargets::GetBackBuffer();

	PIXBegin(0, "Vertical Phase");
	// VERTICAL phase
	// iterate until system is small enough
	for( i = 1; ; ++i )
	{
		if( ( screenHeight >> i ) < 2 )
			break;

		float lastBufferSize = float(( screenHeight >> (i-1) ));
		dofDiffusionShader->SetVar( dofDiffusion_lastbufferSizeVar, lastBufferSize);

		if( i == 2 )
			continue;

		if( i == 1 )
		{
			shaderPass = /*bPacked? g_pPackedInitialReduceVert4PS :*/ pp_DOF_Diffusion_InitialReduceVert4;
		}
		else //if( i == 2 )
		{
			shaderPass = /*bPacked? g_pPackedReduceVertPS :*/ pp_DOF_Diffusion_ReduceVert;
		}

		if( i > 1 )
		{
			/*if( bPacked )
			{
				const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
					g_pVertABCXRTV[i]
				};
				textureFactory.LockMRT(RenderTargets, NULL);
			}
			else*/
			{
				const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
					dofDiffusionVertABC[i],
					dofDiffusionVertX[i-1]
				};
				textureFactory.LockMRT(RenderTargets, NULL);

			}
		}
		else
		{
			/*if( bPacked )
			{
				const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
					g_pVertABCXRTV[i+1]
				};
				textureFactory11.LockMRT(RenderTargets, NULL);
			}
			else*/
			{
				const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
					dofDiffusionVertABC[i+1],
					dofDiffusionVertX[i]
				};
				textureFactory.LockMRT(RenderTargets, NULL);
			}
		}

		if( (i-1) == 0 )
		{
			dofDiffusionShader->SetVar( dofDiffusion_txABC_ID, dofDiffusionHorzABC[0]);
			//bPacked ? g_pHorzABCXSRV[0]:g_pHorzABCSRV[0];
		}
		else
		{
			dofDiffusionShader->SetVar( dofDiffusion_txABC_ID, dofDiffusionVertABC[i-1]);
			//bPacked ? g_pVertABCXSRV[i-1]:g_pVertABCSRV[i-1];
		}
#if BOKEH_SUPPORT
		dofDiffusionShader->SetVar( dofDiffusion_txCOC_ID, BokehDepthBlur);
#endif
		//pSRV[1] = DepthStencilTextureSRV;	//Already calculated coc so use that instead.
		if( i == 1 )
			dofDiffusionShader->SetVar( dofDiffusion_txX_ID, pSourceImage);
		else
		{
			dofDiffusionShader->SetVar( dofDiffusion_txX_ID, dofDiffusionVertX[i-2]);
		}

		DiffusionDOFDraw(shaderPass);

		textureFactory.UnlockMRT();
	}

	PIXEnd(); // Vertical phase

	--i;

	PIXBegin(0, "Vertical - solve final small system");

	// solve final small system
	{
		const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
			dofDiffusionVertY[i]
		};
		textureFactory.LockMRT(RenderTargets, NULL);

		dofDiffusionShader->SetVar( dofDiffusion_txABC_ID, dofDiffusionVertABC[i]);		///*bPacked ? g_pVertABCXSRV[i]:*/g_pVertABCSRV[i];
		dofDiffusionShader->SetVar( dofDiffusion_txX_ID, dofDiffusionVertX[i-1]);

		if( ( screenHeight >> ( i ) ) == 2 )
		{
			shaderPass = /*bPacked?g_pPackedReduceFinalVert2PS:*/ pp_DOF_Diffusion_ReduceFinalVert2;
		}
		else
		{
			shaderPass = /*bPacked?g_pPackedReduceFinalVert3PS:*/ pp_DOF_Diffusion_ReduceFinalVert3;
		}

		DiffusionDOFDraw(shaderPass);

		textureFactory.UnlockMRT();
	}


	PIXEnd();

	PIXBegin(0, "Vertical - Expand Solutions");
	// expand solutions

	shaderPass = /*bPacked?g_pPackedSolveVertPS:*/ pp_DOF_Diffusion_SolveVert;

	for(i-=1; i >=0; --i )
	{
		if( i == 0 )
			continue;

		if( i == 1 )
		{
			const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
				dofDiffusionHorzY[i-1]
			};
			textureFactory.LockMRT(RenderTargets, NULL);
		}
		else
		{
			if( i != 0 )
			{
				const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
					dofDiffusionVertY[i]
				};
				textureFactory.LockMRT(RenderTargets, NULL);
			}
			else
			{
				const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
					dofDiffusionHorzY[i-1]
				};
				textureFactory.LockMRT(RenderTargets, NULL);
			}
		}

		if( i == 1 )
			shaderPass = pp_DOF_FinalSolveVert4;

		if( i == 0 )
		{
			///*bPacked ? g_pHorzABCXSRV[0]:*/g_pHorzABCSRV[0];
			dofDiffusionShader->SetVar( dofDiffusion_txABC_ID, dofDiffusionHorzABC[0]);
		}
		else
		{
			//*bPacked ? g_pVertABCXSRV[i]:*/g_pVertABCSRV[i];
			dofDiffusionShader->SetVar( dofDiffusion_txABC_ID, dofDiffusionVertABC[i]);
		}
		//pSRV[1] = DepthStencilTextureSRV;
		if( i > 1 )
		{
			//pSRV[2] = g_pVertXSRV[i-1];
			dofDiffusionShader->SetVar( dofDiffusion_txX_ID, dofDiffusionVertX[i-1]);
		}
		else
		{
			//pSRV[2] = SourceImageSRV;
			//pSRV[4] = g_pVertXSRV[i-2];
			dofDiffusionShader->SetVar( dofDiffusion_txX_ID, pSourceImage);
			//dofDiffusionShader->SetVar( dofDiffusion_txX_ID, CRenderTargets::GetBackBuffer());
			
		}
		dofDiffusionShader->SetVar( dofDiffusion_txY_ID, dofDiffusionVertY[i+1]);

		DiffusionDOFDraw(shaderPass);

		textureFactory.UnlockMRT();
	}

	PIXEnd();	

	PIXBegin(0, "Horizontal Phase");
	// HORIZONTAL PHASE of Diffusion DOF
	// iterate until system is small enough
	for( i = 1; ; ++i )
	{
		if( ( screenWidth >> i ) < 2 )
			break;

		float lastBufferSize = float(( screenWidth >> (i-1) ));
		dofDiffusionShader->SetVar( dofDiffusion_lastbufferSizeVar, lastBufferSize);

		// skip second pass as we have alreadz reduced by a factor of 4
		if( i == 2 )
			continue;

		if( i == 1 )
		{
			shaderPass = /*bPacked?g_pPackedInitialReduceHorz4PS:*/ pp_DOF_Diffusion_InitialReduceHorz4;
		}
		else
		{
			shaderPass = /*bPacked?g_pPackedReduceHorzPS:*/pp_DOF_Diffusion_ReduceHorz;
		}

		if( i > 1 )
		{
			/*if( bPacked )
			{
				pRTV[0] = g_pHorzABCXRTV[i];
				pRTV[1] = 0;
				pd3dImmediateContext->OMSetRenderTargets(2, pRTV, NULL );
			}
			else*/
			{
				
				const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
					dofDiffusionHorzABC[i],
					dofDiffusionHorzX[i-1]
				};
				textureFactory.LockMRT(RenderTargets, NULL);
			}
		}
		else // first pass => reduce factor 4
		{
			/*if( bPacked )
			{
				pRTV[0] = g_pHorzABCXRTV[i+1];
				pRTV[1] = 0;
				pd3dImmediateContext->OMSetRenderTargets(2, pRTV, NULL );
			}
			else*/
			{
				const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
					dofDiffusionHorzABC[i+1],
					dofDiffusionHorzX[i]
				};
				textureFactory.LockMRT(RenderTargets, NULL);
			}
		}

		dofDiffusionShader->SetVar( dofDiffusion_txABC_ID, dofDiffusionHorzABC[i-1]); 
		//pSRV[1] = DepthStencilTextureSRV;
		if( i == 1 )
			dofDiffusionShader->SetVar( dofDiffusion_txX_ID, dofDiffusionHorzY[0]); 
		else
		{
			dofDiffusionShader->SetVar( dofDiffusion_txX_ID, dofDiffusionHorzX[i-2]); 
		}

		DiffusionDOFDraw(shaderPass);

		textureFactory.UnlockMRT();
	}

	--i;

	PIXEnd();	

	PIXBegin(0, "Horizontal - solve final small system");

	// solve final small system
	{
		const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
			dofDiffusionHorzY[i]
		};
		textureFactory.LockMRT(RenderTargets, NULL);

		dofDiffusionShader->SetVar( dofDiffusion_txABC_ID, dofDiffusionHorzABC[i]);
		//pSRV[1] = DepthStencilTextureSRV;
		dofDiffusionShader->SetVar( dofDiffusion_txX_ID, dofDiffusionHorzX[i-1]);

		if( ( screenWidth >> ( i )  ) == 2 )
		{
			shaderPass = /*bPacked ? g_pPackedReduceFinalHorz2PS:*/ pp_DOF_Diffusion_ReduceFinalHorz2;
		}
		else
		{
			shaderPass = /*bPacked ? g_pPackedReduceFinalHorz3PS:*/ pp_DOF_Diffusion_ReduceFinalHorz3;
		}

		DiffusionDOFDraw(shaderPass);

		textureFactory.UnlockMRT();
	}

	PIXEnd();	

	PIXBegin(0, "Horizontal - expand solutions");

	// expand solutions
	shaderPass = /*bPacked?g_pPackedSolveHorzPS:*/pp_DOF_Diffusion_SolveHorz;


	for(i-=1; i >=0; --i )
	{
		if( i == 0 )
			continue;

		if( i == 1 )
		{
			const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
				pOutputImage
			};
			textureFactory.LockMRT(RenderTargets, NULL);
		}
		else
		{
			const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
				dofDiffusionHorzY[i]
			};
			textureFactory.LockMRT(RenderTargets, NULL);
		}

		if( i == 1 )
			shaderPass = pp_DOF_FinalSolveHorz4;

		
		dofDiffusionShader->SetVar( dofDiffusion_txABC_ID, dofDiffusionHorzABC[i]);		// /*bPacked ? g_pHorzABCXSRV[i]:*/g_pHorzABCSRV[i];
		//pSRV[1] = DepthStencilTextureSRV;
		if( i > 1 )
		{
			dofDiffusionShader->SetVar( dofDiffusion_txX_ID, dofDiffusionHorzX[i-1]);
		}
		else
		{
			dofDiffusionShader->SetVar( dofDiffusion_txX_ID, dofDiffusionHorzY[0]);
// 			pSRV[4] = g_pHorzABCSRV[i+1];
// 			pSRV[5] = g_pHorzXSRV[i];
		}
		dofDiffusionShader->SetVar( dofDiffusion_txY_ID, dofDiffusionHorzY[i+1]);

		DiffusionDOFDraw(shaderPass);

		textureFactory.UnlockMRT();
	}

	PIXEnd();

	PIXEnd();

}
#endif //DOF_DIFFUSION

#if DOF_COMPUTE_GAUSSIAN

void ProcessComputeGaussianDOF(const PostFX::PostFXParamBlock::paramBlock &settings, float screenBlurFadeLevel)
{
	PIXBegin(0, "DOF Compute Gaussian");

	u32 GridSize = DOF_ComputeGridSize;

	/*DOF*/
	float kernelSize = (float)DofKernelSizeVar;
	if( settings.m_DOFOverrideEnabled )
		kernelSize = settings.m_DOFOverride;
	else if( settings.m_ShallowDof )
		kernelSize = (float)DofShallowKernelSizeVar;
	else if( settings.m_AdaptiveDof )
		kernelSize = (float)DofAdaptiveKernelSizeVar;

	if(PostFX::ExtraEffectsRequireBlur())
		kernelSize = Lerp(screenBlurFadeLevel, kernelSize, (float)DOF_MaxSampleRadius);

	const Vector4 projParams = ShaderUtil::CalculateProjectionParams();
	dofComputeShader->SetVar(DofProjCompute, projParams);
	dofComputeShader->SetVar(DofLumFilterParams, Vector2(DofLumContrastThreshold, DofMaxLuminance));
	dofComputeShader->SetVar(DofSkyWeightModifierVar, DofSkyWeightModifier);
	dofComputeShader->SetVar(DofRenderTargetSizeVar, Vector4((float)DepthOfField1->GetWidth(), (float)DepthOfField1->GetHeight(), 1.0f/(float)DepthOfField1->GetWidth(), 1.0f/(float)DepthOfField1->GetHeight()));
	
	float fourPlaneDof = 1.0f;
	if( settings.m_AdaptiveDof )
		fourPlaneDof = 0.0f;
	dofComputeShader->SetVar(fourPlaneDofVar, fourPlaneDof);

	dofComputeShader->SetVar( DofKernelSize, (float)kernelSize);
#if ADAPTIVE_DOF_OUTPUT_UAV
	dofComputeShader->SetVar( DofComputeAdaptiveDOFParamsBufferVar, AdaptiveDepthOfField.GetParamsRT());
#endif //ADAPTIVE_DOF_OUTPUT_UAV

	grcRenderTarget* backBuffer = CRenderTargets::GetBackBuffer();
	if( GRCDEVICE.GetMSAA()>1)
	{	
		backBuffer = CRenderTargets::GetBackBufferCopy(false);
	}

	// Horizontal pass
	dofComputeShader->SetVar(dofComputeColorTex, backBuffer);
# if BOKEH_SUPPORT
	dofComputeShader->SetVar(dofComputeDepthBlurTex, BokehEnable ? BokehDepthBlur : grcTexture::NoneBlack);
	dofComputeShader->SetVar(dofComputeDepthBlurHalfResTex, BokehEnable ? BokehDepthBlurHalfRes : grcTexture::NoneBlack);
# endif

	dofComputeShader->SetVarUAV( dofComputeOutputTex, static_cast<grcTextureUAV*>( DepthOfField1 ) );

	u32 groupCountX = (u32)((DepthOfField1->GetWidth() / (float)GridSize) + 0.5f);
	groupCountX += (DepthOfField1->GetHeight() % GridSize) > 0 ? 1 : 0;
	u32 groupCountY = DepthOfField1->GetHeight();

	u32 horzPass = pp_DOF_ComputeGaussian_Blur_H;
	if(DofLumFilteringEnable)
		horzPass = pp_DOF_ComputeGaussian_Blur_H_LumFilter;
#if ADAPTIVE_DOF_OUTPUT_UAV
	if( settings.m_AdaptiveDof )
	{
		horzPass = pp_DOF_ComputeGaussian_Blur_H_Adaptive;

		if(DofLumFilteringEnable)
			horzPass = pp_DOF_ComputeGaussian_Blur_H_Adaptive_LumFilter;
	}
#endif //ADAPTIVE_DOF_OUTPUT_UAV

	GRCDEVICE.RunComputation( "DOF Compute - H", *dofComputeShader, horzPass, groupCountX, groupCountY, 1);

# if SUPPORT_RENDERTARGET_DUMP
		static_cast<grcRenderTargetDefault*>(DepthOfField1)->SaveTarget();
# endif

	// Vertical pass
	dofComputeShader->SetVar(dofComputeColorTex, DepthOfField1);

	dofComputeShader->SetVarUAV( dofComputeOutputTex, static_cast<grcTextureUAV*>( DepthOfField2 ) );

	groupCountX = DepthOfField2->GetWidth();
	groupCountY = (u32)((DepthOfField2->GetHeight() / (float)GridSize) + 0.5f);
	groupCountY += (DepthOfField2->GetHeight() % GridSize) > 0 ? 1 : 0;

	u32 vertPass = pp_DOF_ComputeGaussian_Blur_V;
#if ADAPTIVE_DOF_OUTPUT_UAV
	if( settings.m_AdaptiveDof )
		vertPass = pp_DOF_ComputeGaussian_Blur_V_Adaptive;
#endif //ADAPTIVE_DOF_OUTPUT_UAV

	GRCDEVICE.RunComputation( "DOF Compute - V", *dofComputeShader, vertPass, groupCountX, groupCountY, 1);

# if SUPPORT_RENDERTARGET_DUMP
		static_cast<grcRenderTargetDefault*>(DepthOfField2)->SaveTarget();
# endif

# if __D3D11 || RSG_ORBIS
	GRCDEVICE.SynchronizeComputeToGraphics();
# endif
	PIXEnd();
};
#endif //DOF_COMPUTE_GAUSSIAN

#if COC_SPREAD
void COCSpread(u32 GridSize)
{ 
	if (!BokehDepthBlurHalfResTemp)
		return;

	dofComputeShader->SetVar( DofRenderTargetSizeVar, Vector4((float)BokehDepthBlurHalfRes->GetWidth(), (float)BokehDepthBlurHalfRes->GetHeight(),  1.0f/(float)BokehDepthBlurHalfRes->GetWidth(), 1.0f/(float)BokehDepthBlurHalfRes->GetHeight()));

	dofComputeShader->SetVar( DofCocSpreadKernelRadius, (float)DofCocSpreadKernelRadiusVar);
	//horizontal pass
	dofComputeShader->SetVar(dofComputeDepthBlurHalfResTex, BokehDepthBlurHalfRes);
	dofComputeShader->SetVarUAV( cocComputeOutputTex, static_cast<grcTextureUAV*>( BokehDepthBlurHalfResTemp ) );

	unsigned groupCountX = (u32)((BokehDepthBlurHalfResTemp->GetWidth() / (float)GridSize) + 0.5f);
	groupCountX += (BokehDepthBlurHalfResTemp->GetHeight() % GridSize) > 0 ? 1 : 0;
	unsigned groupCountY = BokehDepthBlurHalfResTemp->GetHeight();

	GRCDEVICE.RunComputation( "DOF Compute COC spread - H", *dofComputeShader, pp_DOF_ComputeGaussian_COCSpread_H, groupCountX, groupCountY, 1);

# if SUPPORT_RENDERTARGET_DUMP
	static_cast<grcRenderTargetDefault*>(BokehDepthBlurHalfResTemp)->SaveTarget();
# endif

	//vertical pass
	dofComputeShader->SetVar(dofComputeDepthBlurHalfResTex, BokehDepthBlurHalfResTemp);
	dofComputeShader->SetVarUAV( cocComputeOutputTex, static_cast<grcTextureUAV*>( BokehDepthBlurHalfRes ) );

	groupCountX = BokehDepthBlurHalfRes->GetWidth();
	groupCountY = (u32)((BokehDepthBlurHalfRes->GetHeight() / (float)GridSize) + 0.5f);
	groupCountY += (BokehDepthBlurHalfRes->GetHeight() % GridSize) > 0 ? 1 : 0;

	GRCDEVICE.RunComputation( "DOF Compute COC spread - V", *dofComputeShader, pp_DOF_ComputeGaussian_COCSpread_V, groupCountX, groupCountY, 1);

# if SUPPORT_RENDERTARGET_DUMP
	static_cast<grcRenderTargetDefault*>(BokehDepthBlurHalfRes)->SaveTarget();
# endif
# if __D3D11 || RSG_ORBIS
	GRCDEVICE.SynchronizeComputeToGraphics();
# endif
}
#endif //COC_SPREAD

void LastGenDOF(grcRenderTarget*& pDOFLargeBlur, grcRenderTarget*& pDOFMedBlur)
{
	// 1) downsample and initialise near CoC
	PostFXShader->SetVar(TextureID_0, HalfScreen0);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(HalfScreen0->GetWidth()),1.0f/float(HalfScreen0->GetHeight()),0.0, 0.0f));
	PostFXBlit(DepthOfField0, PostFXTechnique, pp_DownSampleBloom);

	// 2) blur colour + initial near CoC
	PostFXShader->SetVar(TextureID_0, DepthOfField0);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(DepthOfField0->GetWidth()),1.0f/float(DepthOfField0->GetHeight()),0.0, 0.0f));
	PostFXBlit(DepthOfField1, PostFXTechnique, pp_Blur);


	// 3) compute actual near CoC
	PostFXShader->SetVar(TextureID_0, DepthOfField0);
	PostFXShader->SetVar(TextureID_1, DepthOfField1);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(DepthOfField0->GetWidth()),1.0f/float(DepthOfField0->GetHeight()),0.0, 0.0f));
	PostFXBlit(DepthOfField2, PostFXTechnique, pp_DOFCoC);

	// slightly cheaper, uglier
	//// 4) blur colour + near CoC
	//PostFXShader->SetVar(TextureID_0, DepthOfField2);
	//PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(DepthOfField2->GetWidth()),1.0f/float(DepthOfField2->GetHeight()),0.0, 0.0f));
	//PostFXBlit(DepthOfField0, PostFXTechnique, pp_Blur);

	//// 5) second blur (optional)
	//PostFXShader->SetVar(TextureID_0, DepthOfField0);
	//PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(DepthOfField2->GetWidth()),1.0f/float(DepthOfField2->GetHeight()),0.0, 0.0f));
	//PostFXBlit(DepthOfField1, PostFXTechnique, pp_Blur);


	// 4) blur med
	PostFXShader->SetVar(TextureID_0, DepthOfField2);
	PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(DepthOfField2->GetWidth()),1.0f/float(DepthOfField2->GetHeight()),0.0, 0.0f));
	PostFXBlit(DepthOfField0, PostFXTechnique, pp_GaussBlur_Hor);

	// 5) blur med
	PostFXShader->SetVar(TextureID_0, DepthOfField0);
	PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(DepthOfField2->GetWidth()),1.0f/float(DepthOfField2->GetHeight()),0.0, 0.0f));
	PostFXBlit(DepthOfField2, PostFXTechnique, pp_GaussBlur_Ver);

	// 6) blur colour + near CoC
	PostFXShader->SetVar(TextureID_0, DepthOfField2);
	PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(DepthOfField2->GetWidth()),1.0f/float(DepthOfField2->GetHeight()),0.0, 0.0f));
	PostFXBlit(DepthOfField0, PostFXTechnique, pp_GaussBlur_Hor);

	// 7) second blur (optional)
	PostFXShader->SetVar(TextureID_0, DepthOfField0);
	PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(DepthOfField2->GetWidth()),1.0f/float(DepthOfField2->GetHeight()),0.0, 0.0f));
	PostFXBlit(DepthOfField1, PostFXTechnique, pp_GaussBlur_Ver);

	pDOFLargeBlur = DepthOfField1;
	pDOFMedBlur = DepthOfField2;
}

void ProcessHighDOFBuffers(const PostFX::PostFXParamBlock::paramBlock &settings, grcRenderTarget*& pDOFLargeBlur, grcRenderTarget*& pDOFMedBlur, grcRenderTarget*& currExposureRT, float screenBlurFadeLevel)
{
	PF_AUTO_PUSH_TIMEBAR("ProcessHighDOFBuffers");

	#if __PS3
	grcEffectVar BloomTexelSizeId = TexelSizeId;
	#endif

	SetNonDepthFXStateBlocks();

#if DOF_TYPE_CHANGEABLE_IN_RAG
	PostFXShader->SetVar(CurrentDOFTechniqueVar, (float)CurrentDOFTechnique);
#endif

	Vector4 hiDofParameters = settings.m_hiDofParameters;
	PackHiDofParameters(hiDofParameters);

	if( PostFX::ExtraEffectsRequireBlur() )
		hiDofParameters.Lerp(screenBlurFadeLevel, Vector4(0.0f, 0.0f, 0.0f, 0.0f));

	PostFXShader->SetVar(HiDofParamsId, hiDofParameters);

#if ADAPTIVE_DOF_GPU
	if( settings.m_AdaptiveDof && CurrentDOFTechnique != dof_console )
	{
		AdaptiveDepthOfField.ProcessAdaptiveDofGPU(settings, currExposureRT, WasAdaptiveDofProcessedOnPreviousUpdate);
	}

	WasAdaptiveDofProcessedOnPreviousUpdate = settings.m_AdaptiveDof;
#else
	(void) currExposureRT;
#endif


#if BOKEH_SUPPORT || DOF_COMPUTE_GAUSSIAN
	if( BokehEnable || CurrentDOFTechnique == dof_computeGaussian )
	{		
		ProcessBokehPreDOF(settings, screenBlurFadeLevel);
	}
#endif //BOKEH_SUPPORT

#if DOF_COMPUTE_GAUSSIAN
#if RSG_PC
	PostFX::SetGaussianBlur();
#endif // RSG_PC

	if( CurrentDOFTechnique == dof_computeGaussian )
	{		
#if COC_SPREAD
		dofComputeShader->SetVar(GaussianWeightsBufferVar, &s_GaussianWeightsCOCBuffer);
		COCSpread(DOF_ComputeGridSizeCOC);
#endif 

		dofComputeShader->SetVar(GaussianWeightsBufferVar, &s_GaussianWeightsBuffer);
		ProcessComputeGaussianDOF(settings, screenBlurFadeLevel);

		pDOFLargeBlur = DepthOfField2;
	}
	else 
#endif //DOF_COMPUTE_GAUSSIAN
#if DOF_DIFFUSION
	if( CurrentDOFTechnique == dof_diffusion )
	{
#if COC_SPREAD
		COCSpread(DOF_ComputeGridSizeCOC);
#endif
		ProcessDiffusionDOFCS(DepthOfField2);
		pDOFLargeBlur = DepthOfField2;
	}
	else
#endif //DOF_DIFFUSION
	{
		LastGenDOF(pDOFLargeBlur, pDOFMedBlur);
	}

#if DEVICE_MSAA && DOF_COMPUTE_GAUSSIAN
	if (GRCDEVICE.GetMSAA()>1 && (CurrentDOFTechnique == dof_computeGaussian))
	{
		grcDepthStencilStateHandle currentDSS = grcStateBlock::DSS_Active;
		grcBlendStateHandle currentBS = grcStateBlock::BS_Active;
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
		
		dofComputeShader->SetVar(dofComputeColorTex, pDOFLargeBlur);
		dofComputeShader->SetVar(dofBlendParams, !(DRAWLISTMGR->IsExecutingHudDrawList() || CLoadingScreens::AreActive()));
		CRenderTargets::LockSceneRenderTargets();
		
		u32 MSAABlendPass = pp_DOF_ComputeGaussian_MSAA_Blend;
#if ADAPTIVE_DOF_OUTPUT_UAV
		if( settings.m_AdaptiveDof )
			MSAABlendPass = pp_DOF_ComputeGaussian_MSAA_Blend_Adaptive;
#endif //ADAPTIVE_DOF_OUTPUT_UAV

#if MSAA_EDGE_PASS
		if (DeferredLighting::IsEdgePassActiveForPostfx() && AssertVerify(PostFXShaderMS0) BANK_ONLY(&& (DebugDeferred::m_OptimizeDofBlend & (1<<EM_IGNORE))))
		{
			BANK_ONLY(if (DebugDeferred::m_OptimizeDofBlend & (1<<EM_EDGE0)))
			{
				grcStateBlock::SetDepthStencilState(DeferredLighting::m_directionalEdgeMaskEqualPass_DS, EDGE_FLAG);
				ShaderUtil::StartShader("DoF MSAA Blend (edges)", dofComputeShader, dofComputeTechnique, MSAABlendPass);
				grcDrawSingleQuadf(0.f,0.f,1.f,1.f, 0.0f, 0.0, 0.0, 0.0f, 0.0f, Color32(0));
				ShaderUtil::EndShader(dofComputeShader);
			}
			BANK_ONLY(if (DebugDeferred::m_OptimizeDofBlend & (1<<EM_FACE0)))
			{
				MSAABlendPass += 1;
				grcStateBlock::SetDepthStencilState(DeferredLighting::m_directionalEdgeMaskEqualPass_DS, 0);
				ShaderUtil::StartShader("DoF MSAA Blend (non-edges)", dofComputeShader, dofComputeTechnique, MSAABlendPass);
				grcDrawSingleQuadf(0.f,0.f,1.f,1.f, 0.0f, 0.0, 0.0, 0.0f, 0.0f, Color32(0));
				ShaderUtil::EndShader(dofComputeShader);
			}
		}else
#endif //MSAA_EDGE_PASS
		{
			ShaderUtil::StartShader("DoF MSAA Blend", dofComputeShader, dofComputeTechnique, MSAABlendPass);
			grcDrawSingleQuadf(0.f,0.f,1.f,1.f, 0.0f, 0.0, 0.0, 0.0f, 0.0f, Color32(0));
			ShaderUtil::EndShader(dofComputeShader);
		}
		
		CRenderTargets::UnlockSceneRenderTargets();
		grcStateBlock::SetDepthStencilState(currentDSS);
		grcStateBlock::SetBlendState(currentBS);
	}
#endif // DEVICE_MSAA && DOF_COMPUTE_GAUSSIAN

#if BOKEH_SUPPORT
	if( BokehEnable )
	{		
		ProcessBokehPostDOF(settings);
	}
#endif //BOKEH_SUPPORT

}

void ProcessHeatHazeBuffers(const PostFX::PostFXParamBlock::paramBlock& settings)
{
	PF_AUTO_PUSH_TIMEBAR("ProcessHeatHazeBuffers");
	SetNonDepthFXStateBlocks();
	//////////////////////////////////////////////////////////////////////////
	// heat haze pre-pass 

	if (Water::IsCameraUnderwater())
	{
		PostFXShader->SetVar(heatHazeParamsId, settings.m_HeatHazeParams);
		PostFXBlit(HeatHaze0, PostFXTechnique, pp_HeatHazeWater);
	}
	else
	{
		// compute initial intensity mask for heat haze
		const grcTexture *const gbuf1 = GBuffer::GetTarget(GBUFFER_RT_1);
		PostFXShader->SetVar(GBufferTextureId0, gbuf1);
		PostFXShader->SetVar(heatHazeParamsId, settings.m_HeatHazeParams);
		PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/gbuf1->GetWidth(),1.0f/gbuf1->GetHeight(),0.0, 0.0f));


	#if POSTFX_TILED_LIGHTING_ENABLED
		if (BANK_SWITCH(PostFX::g_UseTiledTechniques == true, CTiledLighting::IsEnabled()))
		{
			const char* pPassName = NULL;
		#if RAGE_TIMEBARS
			pPassName = passName[pp_HeatHaze_Tiled];
		#endif

		#if __D3D11 || RSG_ORBIS
			PostFXShader->SetVar(TiledDepthTextureId, CTiledLighting::GetClassificationTexture());
		#endif // __D3D11 || RSG_ORBIS

			CTiledLighting::RenderTiles(HeatHaze0, PostFXShader, PostFXTechnique, pp_HeatHaze_Tiled, pPassName, true);
		}
		else
	#endif
		{
			PostFXBlit(HeatHaze0, PostFXTechnique, pp_HeatHaze);
		}

		// dilate heat haze intensity mask
		PostFXShader->SetVar(TextureID_v0, HeatHaze0);
		PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(HeatHaze0->GetWidth()),1.0f/float(HeatHaze0->GetHeight()),0.0, 0.0f));
		PostFXBlit(HeatHaze1, PostFXTechnique, pp_HeatHazeDilateBinary);

		// 2nd dilation + "fade"
		PostFXShader->SetVar(TextureID_v0, HeatHaze1);
		PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(HeatHaze1->GetWidth()),1.0f/float(HeatHaze1->GetHeight()),0.0, 0.0f));
		PostFXBlit(HeatHaze0, PostFXTechnique, pp_HeatHazeDilate);

		// restore texture
		PostFXShader->SetVar(GBufferTextureId0, GBuffer::GetTarget(GBUFFER_RT_0));
	}
}


void SimpleBlur(grcRenderTarget* pBlur0, grcRenderTarget* pBlur1)
{
	SetNonDepthFXStateBlocks();

	PostFXShader->SetVar(TextureID_0, pBlur0);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(pBlur0->GetWidth()),1.0f/float(pBlur0->GetHeight()),0.0, 0.0f));
#if !__PS3
	PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(pBlur0->GetWidth()),1.0f/float(pBlur0->GetHeight()),0.0, 0.0f));
#endif
	PostFXBlit(pBlur1, PostFXTechnique, pp_Blur);

	PostFXShader->SetVar(TextureID_0, pBlur1);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(pBlur1->GetWidth()),1.0f/float(pBlur1->GetHeight()),0.0, 0.0f));
#if !__PS3
	PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(pBlur1->GetWidth()),1.0f/float(pBlur1->GetHeight()),0.0, 0.0f));
#endif
	PostFXBlit(pBlur0, PostFXTechnique, pp_Blur);
}


void PostFX::SetFlashParameters(float UNUSED_PARAM(fMinIntensity), float UNUSED_PARAM(fMaxIntensity), u32 rampUpDuration, u32 holdDuration, u32 rampDownDuration)
{
	g_defaultFlashEffect.Set(DefaultFlashEffectModName, POSTFX_IN_HOLD_OUT, 0U, rampUpDuration, holdDuration, rampDownDuration);
	g_defaultFlashEffect.Start();
}

void PostFX::UpdatePulseEffect()
{
	if (g_uPulseStartTime > 0)
	{
		const u32 c_uCurrentTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		float fInterp = 0.0f;
		if (c_uCurrentTime < g_uPulseStartTime + g_uPulseRampUpDuration)
		{
			// Ramping up
			fInterp = (float)(c_uCurrentTime - g_uPulseStartTime) / (float)g_uPulseRampUpDuration;
		}
		else if (c_uCurrentTime < g_uPulseStartTime + g_uPulseRampUpDuration + g_uPulseHoldDuration)
		{
			fInterp = 1.0f;
		}
		else if (c_uCurrentTime < g_uPulseStartTime + g_uPulseRampUpDuration + g_uPulseHoldDuration + g_uPulseRampDownDuration)
		{
			fInterp = (float)(g_uPulseRampDownDuration - (c_uCurrentTime - g_uPulseStartTime - g_uPulseRampUpDuration - g_uPulseHoldDuration)) / (float)g_uPulseRampDownDuration;
		}
		else
		{
			// Done, so disable the flash effect.
			g_uPulseStartTime = 0;
			g_vInitialPulseParams.Zero();
			g_vTargetPulseParams.Zero();
		}

		PostFXParamBlock::GetUpdateThreadParams().m_distortionParams = Lerp(fInterp, g_vInitialPulseParams, g_vTargetPulseParams);
	}
}

void PostFX::SetPulseParameters(const Vector4& pulseParams, u32 rampUpDuration, u32 holdDuration, u32 rampDownDuration)
{
	g_vInitialPulseParams.Zero();
	g_vTargetPulseParams = pulseParams;

	g_uPulseStartTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	g_uPulseRampUpDuration = rampUpDuration;
	g_uPulseHoldDuration = holdDuration;
	g_uPulseRampDownDuration = rampDownDuration;
}

void PostFX::SetMotionBlurPrevMatrixOverride(const Matrix34& UNUSED_PARAM(mat))
{
	Assertf(0, "PostFX::SetMotionBlurPrevMatrixOverride: not implemented");
}

#if __BANK
static bool IsTiledScreenCaptureOrPanorama()
{
	return TiledScreenCapture::IsEnabled() || SceneGeometryCapture::IsCapturingPanorama() || LightProbe::IsCapturingPanorama();
}
#endif // __BANK

// Is motion blur active? 
bool  PostFX::IsMotionBlurEnabled()
{
	const PostFXParamBlock::paramBlock& rSettings = PostFXParamBlock::GetRenderThreadParams();

	static dev_bool debugForceMotionBlurON = false;
	static dev_bool debugForceMotionBlurOFF = false;

	const bool hasMotionBlur = (rSettings.m_directionalBlurLength > 0.0f || debugForceMotionBlurON)
		&& !CPauseMenu::GetPauseRenderPhasesStatus() // do not allow motion blur when rendering is frozen
		&& !debugForceMotionBlurOFF
		&& !g_screenBlurFade.IsPlaying()
		&& (!camInterface::IsRenderedCameraInsideVehicle() || rSettings.m_isForcingMotionBlur)
#if RSG_PC
		&& (CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Medium)
#endif
#if __BANK
		&& !NoMotionBlur
		&& !TiledScreenCapture::IsEnabled()
		&& !IsTiledScreenCaptureOrPanorama()
#endif // __BANK
		;

	return hasMotionBlur;
}

static __forceinline float SmoothStep(float x, float a, float b)
{
	x = (Clamp<float>(x, a, b) - a)/(b - a);
	return x*x*(3.0f - 2.0f*x);
}

void PostFX::GetFPVPos()
{
	if (g_fpvMotionBlurEnableDynamicUpdate)
	{
		Vec3V currPos = Vec3V(V_ZERO);

		CPed* pPed = CGameWorld::FindLocalPlayer();

		CPedWeaponManager *weaponManager = pPed->GetWeaponManager();
		bool usedWeapon = false;
		if( weaponManager )
		{
			CPedEquippedWeapon* equippedWeapon = weaponManager->GetPedEquippedWeapon();
			if( equippedWeapon )
			{
				CObject *weaponObject = equippedWeapon->GetObject();
				if( weaponObject )
				{
					currPos = weaponObject->GetTransform().GetPosition();
					usedWeapon = true;
				}
			}
		}

		if (!usedWeapon)
		{
			CPed* pPed = CGameWorld::FindLocalPlayer();
			pPed->GetBonePositionVec3V(currPos, BONETAG_R_HAND);
		}

		const grcViewport* vp = gVpMan.GetUpdateGameGrcViewport();
		const Mat44V view = vp->GetViewMtx();
		fpvMotionBlurCurrCameraPos = Multiply(view, Vec4V(currPos, ScalarV(V_ONE))).GetXYZ();
	}
}

void PostFX::Update()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const int ub=gRenderThreadInterface.GetUpdateBuffer();

	LENSARTEFACTSMGR.Update();
	PHONEPHOTOEDITOR.Update();
	PAUSEMENUPOSTFXMGR.Update();
	g_screenBlurFade.Update();

#if USE_SCREEN_WATERMARK
	UpdateScreenWatermark();
#endif

#if __BANK
	g_UseTiledTechniques = g_UseTiledTechniques && CTiledLighting::IsEnabled();
#endif

#if __BANK
	if (gFrameCapture || gFrameCaptureSingle)
	{
		CFrameDump::StartCapture(gFrameCapPath, gFrameCapJpeg, gFrameCaptureSingle, gFrameCaptureOnCutsceneStart, gFrameUseCutsceneNameForPath, gFrameCaptureMBSync, gFrameCaptureBinkMode, gFrameCaptureCutsceneMode, gFrameCaptureFPS, &gFrameCapture);
		gFrameCaptureSingle = false;
	}
	
	if (!gFrameCapture)
		CFrameDump::StopCapture();
#endif

	// light ray switch
#if __BANK
	if (g_Override==false) //only auto switch when not overridden
#endif // __BANK
	{	
		if ((camInterface::GetPos().z<0.0f || g_lightRaysAboveWaterLine==true) && 
			(Lights::GetUpdateDirectionalLight().GetIntensity()>0.0f)
#if __BANK
			&& IsTiledScreenCaptureOrPanorama()==false
#endif // __BANK
			)
		{
			g_enableLightRays=true;
		}
		else
		{
			g_enableLightRays=false;
		}
	}

	const camFrame& frame = gVpMan.GetCurrentGameViewportFrame();

	// Motion Blur/noise
	bool gamePaused = fwTimer::IsGamePaused();
	if (!gamePaused REPLAY_ONLY( || (CReplayMgr::IsEditModeActive() && g_UpdateReplayEffectsWhilePaused))) 
	{
		NoiseEffectRandomValues.x = (static_cast<float>(rand())/static_cast<float>(RAND_MAX));
		NoiseEffectRandomValues.y = (static_cast<float>(rand())/static_cast<float>(RAND_MAX));
	}

	if (!gamePaused) 
	{
		Mat34V camMtx = RCC_MAT34V(camInterface::GetMat());

		MotionBlurPrevViewProjMat=MotionBlurCurViewProjMat;

		Mat44V vProjMat = gVpMan.GetUpdateGameGrcViewport()->GetCompositeMtx();
		MotionBlurCurViewProjMat = RCC_MATRIX44(vProjMat);

		// Check for extreme camera orientation change or camera cuts
		bool hasCut = frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);

		float cosAng= Dot(MotionBlurPreviousCameraDir, camMtx.b()).Getf();
		if (cosAng<0.0f || (hasCut && !g_motionBlurCutTestDisabled) )
		{
			MotionBlurPrevViewProjMat=MotionBlurCurViewProjMat;
		}
		MotionBlurPreviousCameraDir = camMtx.b();

		MotionBlurPrevViewProjMatrix[ub]=MotionBlurPrevViewProjMat;
		MotionBlurBufferedPrevViewProjMat=MotionBlurPrevViewProjMatrix[ub];
	}
	else
	{
		MotionBlurPrevViewProjMatrix[ub]=MotionBlurBufferedPrevViewProjMat;
	}

	PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetUpdateThreadParams();

#if __BANK
	if (g_Override==false)
#endif // __BANK
	{
		const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();

		Vector3 currLightrayCol(currKeyframe.GetVar(TCVAR_LIGHT_RAY_COL_R), currKeyframe.GetVar(TCVAR_LIGHT_RAY_COL_G), currKeyframe.GetVar(TCVAR_LIGHT_RAY_COL_B)); 
		Vector4 currLightrayParams;
		
		bool useUnderwater	= Water::IsCameraUnderwater();
		
		if ( useUnderwater ) 
		{
			currLightrayParams=Vector4(currLightrayCol.x*currKeyframe.GetVar(TCVAR_LIGHT_RAY_UNDERWATER_MULT), currLightrayCol.y*currKeyframe.GetVar(TCVAR_LIGHT_RAY_UNDERWATER_MULT), currLightrayCol.z*currKeyframe.GetVar(TCVAR_LIGHT_RAY_UNDERWATER_MULT), currKeyframe.GetVar(TCVAR_LIGHT_RAY_DIST) );
		}
		else
		{
			// compute fogray density based on height falloff start and height falloff
			const grcViewport *viewport = gVpMan.GetUpdateGameGrcViewport();
			float lightrayDensityMult = 1.0f;
			if (viewport)
			{
				const float fullDist = viewport->GetCameraMtx().GetM23f() ;
				float dist = Max(0.0f, fullDist - currKeyframe.GetVar(TCVAR_LIGHT_RAY_HEIGHTFALLOFFSTART));
				float deltaDist = fullDist * (dist/fullDist);
				float t = deltaDist * (currKeyframe.GetVar(TCVAR_LIGHT_RAY_HEIGHTFALLOFF) / 1000.0f);
				lightrayDensityMult = (Abs(deltaDist) > 0.01f) ? (1.0f - exp(-t)) / t : 1.0f;
			}

			currLightrayParams=Vector4(currLightrayCol.x*currKeyframe.GetVar(TCVAR_LIGHT_RAY_MULT)*lightrayDensityMult, currLightrayCol.y*currKeyframe.GetVar(TCVAR_LIGHT_RAY_MULT), currLightrayCol.z*currKeyframe.GetVar(TCVAR_LIGHT_RAY_MULT), currKeyframe.GetVar(TCVAR_LIGHT_RAY_DIST) );
		}
		Vector4	currSSLRParams(currKeyframe.GetVar(TCVAR_LIGHT_RAY_ADD_REDUCER),currKeyframe.GetVar(TCVAR_LIGHT_RAY_BLIT_SIZE),currKeyframe.GetVar(TCVAR_LIGHT_RAY_RAY_LENGTH),0.0f);

		bool bSniperSightActive = g_sniperSightDefaultEnabled && CNewHud::IsSniperSightActive();

		// reset sniper sight DOF params
		settings.m_sniperSightActive = bSniperSightActive;
		settings.m_sniperSightDOFParams = g_sniperSightDefaultDOFParams;

		// check if an override is active
		if (bSniperSightActive && g_sniperSightOverrideEnabled)
		{	
			settings.m_sniperSightActive = !g_sniperSightOverrideDisableDOF;
			settings.m_sniperSightDOFParams = g_sniperSightOverrideDOFParams;
		}

		settings.m_HighDofScriptOverrideToggle = g_scriptHighDOFOverrideToggle;
		settings.m_HighDofScriptOverrideEnableDOF = g_scriptHighDOFOverrideEnableDOF;
		settings.m_hiDofScriptParameters = g_scriptHighDOFOverrideParams;

		// Setup tonemap settings
		Vector4 darkTonemapParams0 = Vector4(
			GetTonemapParam(TONEMAP_FILMIC_A_DARK),
			GetTonemapParam(TONEMAP_FILMIC_B_DARK),
			GetTonemapParam(TONEMAP_FILMIC_C_DARK),
			GetTonemapParam(TONEMAP_FILMIC_D_DARK));
		Vector4 darkTonemapParams1 = Vector4(
			GetTonemapParam(TONEMAP_FILMIC_E_DARK),
			GetTonemapParam(TONEMAP_FILMIC_F_DARK),
			GetTonemapParam(TONEMAP_FILMIC_W_DARK),
			0.0f);
		float darkExposure = GetTonemapParam(TONEMAP_FILMIC_EXPOSURE_DARK);

		Vector4 brightTonemapParams0 = Vector4(
			GetTonemapParam(TONEMAP_FILMIC_A_BRIGHT),
			GetTonemapParam(TONEMAP_FILMIC_B_BRIGHT),
			GetTonemapParam(TONEMAP_FILMIC_C_BRIGHT),
			GetTonemapParam(TONEMAP_FILMIC_D_BRIGHT));
		Vector4 brightTonemapParams1 = Vector4(
			GetTonemapParam(TONEMAP_FILMIC_E_BRIGHT),
			GetTonemapParam(TONEMAP_FILMIC_F_BRIGHT),
			GetTonemapParam(TONEMAP_FILMIC_W_BRIGHT),
			0.0f);
		float brightExposure = GetTonemapParam(TONEMAP_FILMIC_EXPOSURE_BRIGHT);

		const bool darkOverriden = (currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_DARK) > 0.0f);
		const bool brightOverriden = (currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_BRIGHT) > 0.0f);

		if (darkOverriden || brightOverriden)
		{
			darkTonemapParams0 = Vector4(	
				currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_A_DARK), 
				currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_B_DARK), 
				currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_C_DARK),
				currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_D_DARK));

			darkTonemapParams1 = Vector4(	
				currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_E_DARK), 
				currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_F_DARK), 
				currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_W_DARK),
				0.0f);

			darkExposure = currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_EXPOSURE_DARK);

			if (darkOverriden && !brightOverriden)
			{
				brightTonemapParams0 = darkTonemapParams0;
				brightTonemapParams1 = darkTonemapParams1;
				brightExposure = darkExposure;
			}

			if (!darkOverriden && brightOverriden)
			{
				Assertf(false, "Only bright TM settings were overrwitten instead of both dark and bright");
			}

			if (darkOverriden && brightOverriden)
			{
				brightTonemapParams0 = Vector4(	
					currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_A_BRIGHT), 
					currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_B_BRIGHT), 
					currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_C_BRIGHT),
					currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_D_BRIGHT));

				brightTonemapParams1 = Vector4(	
					currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_E_BRIGHT), 
					currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_F_BRIGHT), 
					currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_W_BRIGHT),
					0.0f);

				brightExposure = currKeyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_EXPOSURE_BRIGHT);

				Assertf(darkExposure > brightExposure, "Dark exposure (%f) needs to be greater than Bright Exposure (%f) when overriding TM", darkExposure, brightExposure);
			}
		}

		// Blend between dark and bright
		const float currentExposure = PostFX::GetUpdateExposure();
		float range = (darkExposure == brightExposure) ? 1.0f : (darkExposure - brightExposure);
		const float t = Saturate((currentExposure - brightExposure) / range);

		settings.m_filmicParams[0].Set(Lerp(t, brightTonemapParams0, darkTonemapParams0));
		settings.m_filmicParams[1].Set(Lerp(t, brightTonemapParams1, darkTonemapParams1));
		
		settings.m_tonemapParams.x = 1.0f / range;
		settings.m_tonemapParams.y = -brightExposure / range;		

		settings.m_brightTonemapParams[0].Set(brightTonemapParams0);
		settings.m_brightTonemapParams[1].Set(brightTonemapParams1);

		settings.m_darkTonemapParams[0].Set(darkTonemapParams0);
		settings.m_darkTonemapParams[1].Set(darkTonemapParams1);

		// Color Correction
		settings.m_colorCorrect.Set(Vector4(
			currKeyframe.GetVar(TCVAR_POSTFX_CORRECT_COL_R), 
			currKeyframe.GetVar(TCVAR_POSTFX_CORRECT_COL_G), 
			currKeyframe.GetVar(TCVAR_POSTFX_CORRECT_COL_B),
			currKeyframe.GetVar(TCVAR_POSTFX_CORRECT_CUTOFF)));
		settings.m_colorShift.Set(Vector4(
			currKeyframe.GetVar(TCVAR_POSTFX_SHIFT_COL_R), 
			currKeyframe.GetVar(TCVAR_POSTFX_SHIFT_COL_G),
			currKeyframe.GetVar(TCVAR_POSTFX_SHIFT_COL_B),
			currKeyframe.GetVar(TCVAR_POSTFX_SHIFT_CUTOFF)));
		settings.m_deSaturate = currKeyframe.GetVar(TCVAR_POSTFX_DESATURATION);

		// Adjust for the front-end
		const bool useDefaultGamma = (CPhotoManager::IsProcessingScreenshot() 
			VIDEO_RECORDING_ENABLED_ONLY ( || ( VideoRecording::IsRecording() && !CReplayCoordinator::ShouldShowLoadingScreen() ) ) );

		settings.m_gamma = useDefaultGamma ? (1.0f / 2.2f) : (1.0f / g_gammaFrontEnd);

		#if __BANK
		if (CDebug::IsTakingScreenshot() || 
			CSystem::GetRageSetup()->IsTakingScreenshot() ||
			CBugstarIntegration::IsTakingScreenshot())
			settings.m_gamma = 1.0f / 2.2f;
		#endif

		// HDR
		settings.m_bloomThresholdMax = currKeyframe.GetVar(TCVAR_POSTFX_BRIGHT_PASS_THRESH);
		settings.m_bloomThresholdWidth = currKeyframe.GetVar(TCVAR_POSTFX_BRIGHT_PASS_THRESH_WIDTH);
		settings.m_bloomIntensity = currKeyframe.GetVar(TCVAR_POSTFX_INTENSITY_BLOOM);
		settings.m_exposureTweak = currKeyframe.GetVar(TCVAR_POSTFX_EXPOSURE);
		settings.m_exposureMin = currKeyframe.GetVar(TCVAR_POSTFX_EXPOSURE_MIN);
		settings.m_exposureMax = currKeyframe.GetVar(TCVAR_POSTFX_EXPOSURE_MAX);

		// Night Vision
		settings.m_lowLum = currKeyframe.GetVar(TCVAR_NV_LOWLUM);
		settings.m_highLum = currKeyframe.GetVar(TCVAR_NV_HIGHLUM);
		settings.m_topLum = currKeyframe.GetVar(TCVAR_NV_TOPLUM);
		settings.m_scalerLum = currKeyframe.GetVar(TCVAR_NV_SCALERLUM);
		settings.m_offsetLum = currKeyframe.GetVar(TCVAR_NV_OFFSETLUM);
		settings.m_offsetLowLum = currKeyframe.GetVar(TCVAR_NV_OFFSETLOWLUM);
		settings.m_offsetHighLum = currKeyframe.GetVar(TCVAR_NV_OFFSETHIGHLUM);
		settings.m_noiseLum = currKeyframe.GetVar(TCVAR_NV_NOISELUM);
		settings.m_noiseLowLum = currKeyframe.GetVar(TCVAR_NV_NOISELOWLUM);
		settings.m_noiseHighLum = currKeyframe.GetVar(TCVAR_NV_NOISEHIGHLUM);
		settings.m_bloomLum = currKeyframe.GetVar(TCVAR_NV_BLOOMLUM);
		settings.m_colorLum.Set(	currKeyframe.GetVar(TCVAR_NV_COLORLUM_R),
									currKeyframe.GetVar(TCVAR_NV_COLORLUM_G),
									currKeyframe.GetVar(TCVAR_NV_COLORLUM_B),0.0f);
		settings.m_colorLowLum.Set(	currKeyframe.GetVar(TCVAR_NV_COLORLOWLUM_R),
									currKeyframe.GetVar(TCVAR_NV_COLORLOWLUM_G),
									currKeyframe.GetVar(TCVAR_NV_COLORLOWLUM_B),0.0f);
		settings.m_colorHighLum.Set(currKeyframe.GetVar(TCVAR_NV_COLORHIGHLUM_R),
									currKeyframe.GetVar(TCVAR_NV_COLORHIGHLUM_G),
									currKeyframe.GetVar(TCVAR_NV_COLORHIGHLUM_B),0.0f);
		
		// Scanline 
		settings.m_scanlineParams.x = currKeyframe.GetVar(TCVAR_POSTFX_SCANLINEINTENSITY);
		settings.m_scanlineParams.y = currKeyframe.GetVar(TCVAR_POSTFX_SCANLINE_FREQUENCY_0);
		settings.m_scanlineParams.z = currKeyframe.GetVar(TCVAR_POSTFX_SCANLINE_FREQUENCY_1);
		settings.m_scanlineParams.w = currKeyframe.GetVar(TCVAR_POSTFX_SCANLINE_SPEED);

		// Heat Haze
		settings.m_HeatHazeParams.x = currKeyframe.GetVar(TCVAR_HH_RANGESTART);				//	Start range 
		settings.m_HeatHazeParams.y = currKeyframe.GetVar(TCVAR_HH_RANGEEND);				//	Far range 
		settings.m_HeatHazeParams.z = currKeyframe.GetVar(TCVAR_HH_MININTENSITY);			//	Min Intensity 
		settings.m_HeatHazeParams.w = currKeyframe.GetVar(TCVAR_HH_MAXINTENSITY);			//	Max Intensity 
		settings.m_HeatHazeOffsetParams.x = currKeyframe.GetVar(TCVAR_HH_DISPU);			//	Displacement Scale U 
		settings.m_HeatHazeOffsetParams.y = currKeyframe.GetVar(TCVAR_HH_DISPV);			//	Displacement Scale V 
		settings.m_HeatHazeTex1Params.x = currKeyframe.GetVar(TCVAR_HH_TEX1_USCALE);		//	Tex1 U scale 
		settings.m_HeatHazeTex1Params.y = currKeyframe.GetVar(TCVAR_HH_TEX1_VSCALE);		//	Tex1 V scale 
		settings.m_HeatHazeTex1Params.z = currKeyframe.GetVar(TCVAR_HH_TEX1_UOFFSET);		//	Tex1 U offset 
		settings.m_HeatHazeTex1Params.w = currKeyframe.GetVar(TCVAR_HH_TEX1_VOFFSET);		//	Tex1 V offset 
		settings.m_HeatHazeTex2Params.x = currKeyframe.GetVar(TCVAR_HH_TEX2_USCALE);		//	Tex2 U scale 
		settings.m_HeatHazeTex2Params.y = currKeyframe.GetVar(TCVAR_HH_TEX2_VSCALE);		//	Tex2 V scale 
		settings.m_HeatHazeTex2Params.z = currKeyframe.GetVar(TCVAR_HH_TEX2_UOFFSET);		//	Tex2 U offset 
		settings.m_HeatHazeTex2Params.w = currKeyframe.GetVar(TCVAR_HH_TEX2_VOFFSET);		//	Tex2 V offset 
		settings.m_HeatHazeTex1Anim.x = currKeyframe.GetVar(TCVAR_HH_TEX1_OFFSET);			//	Tex1 U Frequency offset 
		settings.m_HeatHazeTex1Anim.y = currKeyframe.GetVar(TCVAR_HH_TEX1_FREQUENCY);		//	Tex1 U Frequency 
		settings.m_HeatHazeTex1Anim.z = currKeyframe.GetVar(TCVAR_HH_TEX1_AMPLITUDE);		//	Tex1 U Amplitude 
		settings.m_HeatHazeTex1Anim.w = currKeyframe.GetVar(TCVAR_HH_TEX1_SCROLLING);		//	Tex1 V Scrolling speed 
		settings.m_HeatHazeTex2Anim.x = currKeyframe.GetVar(TCVAR_HH_TEX2_OFFSET);			//	Tex2 U Frequency offset 
		settings.m_HeatHazeTex2Anim.y = currKeyframe.GetVar(TCVAR_HH_TEX2_FREQUENCY);		//	Tex2 U Frequency 
		settings.m_HeatHazeTex2Anim.z = currKeyframe.GetVar(TCVAR_HH_TEX2_AMPLITUDE);		//	Tex2 U Amplitude 
		settings.m_HeatHazeTex2Anim.w = currKeyframe.GetVar(TCVAR_HH_TEX2_SCROLLING);		//	Tex2 V Scrolling speed 
 
		// Noise
		settings.m_noise = currKeyframe.GetVar(TCVAR_POSTFX_NOISE);
		settings.m_noiseSize = currKeyframe.GetVar(TCVAR_POSTFX_NOISE_SIZE);

		
		// Vignetting
		settings.m_vignettingParams.x = currKeyframe.GetVar(TCVAR_POSTFX_VIGNETTING_INTENSITY); // Vignetting Intensity
		settings.m_vignettingParams.y = currKeyframe.GetVar(TCVAR_POSTFX_VIGNETTING_RADIUS);	// Vignetting Radius
		settings.m_vignettingParams.z = currKeyframe.GetVar(TCVAR_POSTFX_VIGNETTING_CONTRAST);	// Vignetting Contrast
		settings.m_vignettingColour.x = currKeyframe.GetVar(TCVAR_POSTFX_VIGNETTING_COL_R);		// Vignetting Colour
		settings.m_vignettingColour.y = currKeyframe.GetVar(TCVAR_POSTFX_VIGNETTING_COL_G);		// Vignetting Colour
		settings.m_vignettingColour.z = currKeyframe.GetVar(TCVAR_POSTFX_VIGNETTING_COL_B);		// Vignetting Colour

		// Lens Artefacts
		settings.m_lensArtefactsGlobalMultiplier		= currKeyframe.GetVar(TCVAR_LENS_ARTEFACTS_INTENSITY);			// Lens Artefacts Global Multiplier
		settings.m_lensArtefactsMinExposureMultiplier	= currKeyframe.GetVar(TCVAR_LENS_ARTEFACTS_INTENSITY_MIN_EXP);	// Lens Artefacts Exposure Based Min Multiplier
		settings.m_lensArtefactsMaxExposureMultiplier	= currKeyframe.GetVar(TCVAR_LENS_ARTEFACTS_INTENSITY_MAX_EXP);  // Lens Artefacts Exposure Based Max Multiplier

#if __BANK
		if(CutSceneManager::GetInstance()->IsPlaying() && CutSceneManager::GetInstance()->IsIsolating())
		{
			settings.m_bloomIntensity = 0.0f;
			settings.m_lensArtefactsGlobalMultiplier = 0.0f;
			settings.m_HighDofScriptOverrideEnableDOF = false;
		}
#endif

		// Colour gradient
		const float midPoint = currKeyframe.GetVar(TCVAR_POSTFX_GRAD_MIDPOINT);
		const float topMiddleMidPoint = currKeyframe.GetVar(TCVAR_POSTFX_GRAD_TOP_MIDDLE_MIDPOINT);
		const float middleBottomMidPoint = currKeyframe.GetVar(TCVAR_POSTFX_GRAD_MIDDLE_BOTTOM_MIDPOINT);

		settings.m_lensGradientColTop.Set(currKeyframe.GetVar(TCVAR_POSTFX_GRAD_TOP_COL_R),			// Gradient Top Colour Red
										currKeyframe.GetVar(TCVAR_POSTFX_GRAD_TOP_COL_G),			// Gradient Top Colour Green
										currKeyframe.GetVar(TCVAR_POSTFX_GRAD_TOP_COL_B),			// Gradient Top Colour Blue
										topMiddleMidPoint);											// Gradient Top-Middle Midpoint
										
		settings.m_lensGradientColMiddle.Set(currKeyframe.GetVar(TCVAR_POSTFX_GRAD_MIDDLE_COL_R),	// Gradient Middle Colour Red
											currKeyframe.GetVar(TCVAR_POSTFX_GRAD_MIDDLE_COL_G),	// Gradient Middle Colour Green
											currKeyframe.GetVar(TCVAR_POSTFX_GRAD_MIDDLE_COL_B),	// Gradient Middle Colour Blue
											midPoint);												// Gradient Middle Colour Position


		settings.m_lensGradientColBottom.Set(currKeyframe.GetVar(TCVAR_POSTFX_GRAD_BOTTOM_COL_R),	// Gradient Bottom Colour Red
											currKeyframe.GetVar(TCVAR_POSTFX_GRAD_BOTTOM_COL_G),	// Gradient Bottom Colour Green
											currKeyframe.GetVar(TCVAR_POSTFX_GRAD_BOTTOM_COL_B),	// Gradient Bottom Colour Blue
											middleBottomMidPoint);									// Gradient Middle-Bottom Midpoint

		if(g_timeCycle.GetLightningOverrideUpdate())
		{
			settings.m_SunDirection = RCC_VECTOR3(g_timeCycle.GetLightningDirectionUpdate());
		}
		else
		{
			settings.m_SunDirection = RCC_VECTOR3(g_timeCycle.GetSunSpriteDirection());
		}
	
		float lightrayIntensity = currLightrayParams.x + currLightrayParams.y + currLightrayParams.z; // not real intesity, but used as a threshold
	
		settings.m_lightRaysActive = (true == g_enableLightRays) && (lightrayIntensity >0.0f) && (false == g_enableNightVision);

		if (settings.m_lightRaysActive && !useUnderwater)
		{
			// adjust sslr when in sniper scope mode, since the it pops when the horizon touches the top of the screen if the sun is offscreen.
			// also disable sslr when the sun is behind the camera, since it does nothing then anyway
			if (const grcViewport * vp = gVpMan.GetUpdateGameGrcViewport())
			{
				float sslrAlpha = 1.0f;
				Vec3V sunInView = Multiply(vp->GetViewMtx(),Vec4V(VECTOR3_TO_VEC3V(settings.m_SunDirection),ScalarV(V_ZERO))).GetXYZ();
				
				if (IsLessThanAll(sunInView.GetZ(),ScalarV(V_ZERO)))
				{	
					bool enableFade = g_fadeSSLROffscreen;
					float scale = 1.0f/g_fadeSSLROffscreenValue;

					if (!enableFade) // if not forcing it, check for sniper mode
					{
						// the sun is in front of the camera, see if we're in sniper mode
						const camBaseCamera* activeCamera = camInterface::GetDominantRenderedCamera();
						if (activeCamera && activeCamera->GetIsClassId(camAimCamera::GetStaticClassId()))
						{
							if (const camControlHelper *helper = static_cast<const camAimCamera*>(activeCamera)->GetControlHelper())
							{
								if (helper->IsUsingZoomInput()) // for sniper mode only: fade off as the sun get near the edge of the screen
								{
									enableFade = true;
									scale = 1.0f; 
								}
							}
						}
					}

					if (enableFade)
					{
						sunInView /= Vec3V(ScalarV(vp->GetTanHFOV()), ScalarV(vp->GetTanVFOV()), ScalarV(V_ONE)) * sunInView.GetZ(); // normalize to view volume
						sslrAlpha = Min(1.0f,scale*Max(Abs(sunInView.GetXf()),Abs(sunInView.GetYf())));
						sslrAlpha = 1-sslrAlpha*sslrAlpha*sslrAlpha;
					}
				}
				else
				{
					// the sun is behind the camera, so we can skip the sslr work altogether
					sslrAlpha = 0.0f;
				}

				if (sslrAlpha*lightrayIntensity <= 0.00f)  // if the adjusted amount is less than the threshold, just turn them off.
					settings.m_lightRaysActive = false;
				else
					currLightrayParams *= Vector4(sslrAlpha,sslrAlpha,sslrAlpha,1.0f);	
			}
		}

		settings.m_lightrayParams.Set(currLightrayParams);
		settings.m_sslrParams.Set(currSSLRParams);
	
#if __BANK
		settings.m_nightVision = g_enableNightVision != g_overrideNightVision;
#else
		settings.m_nightVision = g_enableNightVision;
#endif // __BANK
	
		// Don't allow effect to render if the game is paused, since we don't have depth/stencil
		settings.m_seeThrough = RenderPhaseSeeThrough::GetState() && (CPauseMenu::GetPauseRenderPhasesStatus() == false);
		
		settings.m_SeeThroughDepthParams = Vector4(	RenderPhaseSeeThrough::GetFadeStartDistance(),
													RenderPhaseSeeThrough::GetFadeEndDistance(),
													RenderPhaseSeeThrough::GetMaxThickness(),
													1.0f);

		settings.m_SeeThroughParams = Vector4(	RenderPhaseSeeThrough::GetMinNoiseAmount(),
												RenderPhaseSeeThrough::GetMaxNoiseAmount(),
												RenderPhaseSeeThrough::GetHiLightIntensity(),
												RenderPhaseSeeThrough::GetHiLightNoise() );

		settings.m_SeeThroughColorNear = VEC4V_TO_VECTOR4(RenderPhaseSeeThrough::GetColorNear().GetRGBA());
		settings.m_SeeThroughColorFar = VEC4V_TO_VECTOR4(RenderPhaseSeeThrough::GetColorFar().GetRGBA());
		settings.m_SeeThroughColorVisibleBase = VEC4V_TO_VECTOR4(RenderPhaseSeeThrough::GetColorVisibleBase().GetRGBA());
		settings.m_SeeThroughColorVisibleWarm = VEC4V_TO_VECTOR4(RenderPhaseSeeThrough::GetColorVisibleWarm().GetRGBA());
		settings.m_SeeThroughColorVisibleHot = VEC4V_TO_VECTOR4(RenderPhaseSeeThrough::GetColorVisibleHot().GetRGBA());

		// Distortion
		settings.m_distortionParams.Set(currKeyframe.GetVar(TCVAR_LENS_DISTORTION_COEFF),
										currKeyframe.GetVar(TCVAR_LENS_DISTORTION_CUBE_COEFF),
										currKeyframe.GetVar(TCVAR_LENS_CHROMATIC_ABERRATION_COEFF),
										currKeyframe.GetVar(TCVAR_LENS_CHROMATIC_ABERRATION_CUBE_COEFF));

		// Blur Vignetting
		settings.m_blurVignettingParams.Set(currKeyframe.GetVar(TCVAR_BLUR_VIGNETTING_INTENSITY),
											currKeyframe.GetVar(TCVAR_BLUR_VIGNETTING_RADIUS),
											0.0f,
											0.0f);
	
		// Screen Blur 
		settings.m_screenBlurFade = currKeyframe.GetVar(TCVAR_SCREEN_BLUR_INTENSITY);

		// Allow the camera system to push the screen blur
		const float fullScreenBlurBlendLevelForCamera = frame.GetDofFullScreenBlurBlendLevel();
		settings.m_screenBlurFade = Lerp(fullScreenBlurBlendLevelForCamera, settings.m_screenBlurFade, 1.0f);

		// Damage Overlay
		settings.m_damageOverlayMisc.Zero();
		settings.m_damageEnabled[0] = false;
		settings.m_damageEnabled[1] = false;
		settings.m_damageEnabled[2] = false;
		settings.m_damageEnabled[3] = false;
		settings.m_drawDamageOverlayAfterHud = false;
		settings.m_damageFiringPos[0].Zero();
		settings.m_damageFiringPos[1].Zero();
		settings.m_damageFiringPos[2].Zero();
		settings.m_damageFiringPos[3].Zero();
		settings.m_damagePlayerPos.Zero();

		// Exposure adjusts
		settings.m_exposurePush = 0.0f;
		settings.m_averageTimestep = 1.0f;

		// Adaption for colour compression
		if (g_useAutoColourCompression)
		{
			const float invExposure = pow(2.0f, -g_adaptation.GetUpdateExposure());
			g_adaptation.SetUpdatePackScalar(invExposure * settings.m_filmicParams[1].z);
		}

		// Cutscene specific overrides
		CutSceneManager* csMgr = CutSceneManager::GetInstance();
		Assert(csMgr);

		if(csMgr->IsCutscenePlayingBack())
		{
			const CCutSceneCameraEntity* cam = csMgr->GetCamEntity();
			if( cam )
			{
				settings.m_exposurePush = cam->GetExposurePush();
			}
		}

	#if __BANK
		if (g_DebugBulletImpactUseCursorPos && CDebugScene::GetMouseLeftPressed())
		{
			g_DebugBulletImpactTrigger = true;
		}

		if (g_DebugBulletImpactTrigger)
		{
			if (g_DebugBulletImpactUseCursorPos)
			{
				if (CDebugScene::GetMouseLeftReleased())
				{
					Vector3 worldPos;
					CDebugScene::GetWorldPositionUnderMouse(worldPos);
					g_bulletImpactOverlay.RegisterBulletImpact(worldPos, 1.0f, g_DebugBulletImpactUseEnduranceIndicator);
					g_DebugBulletImpactTrigger = false;
				}
			}
			else
			{
				g_bulletImpactOverlay.RegisterBulletImpact(g_DebugBulletImpactPos, 1.0f, g_DebugBulletImpactUseEnduranceIndicator);
				g_DebugBulletImpactTrigger = false;
			}
		}
	#endif

		settings.m_BokehBrightnessThresholdMax = currKeyframe.GetVar(TCVAR_BOKEH_BRIGHTNESS_MAX);
		settings.m_BokehBrightnessThresholdMin = currKeyframe.GetVar(TCVAR_BOKEH_BRIGHTNESS_MIN);
		settings.m_BokehBrightnessFadeThresholdMax = currKeyframe.GetVar(TCVAR_BOKEH_FADE_MAX);
		settings.m_BokehBrightnessFadeThresholdMin = currKeyframe.GetVar(TCVAR_BOKEH_FADE_MIN);

		LENSARTEFACTSMGR.SetGlobalIntensity(settings.m_lensArtefactsGlobalMultiplier);

		g_bulletImpactOverlay.Update();
		g_cutsceneDofTODOverride.Update();

		// Don't update certain parameters when render phases are paused
		if (CPauseMenu::GetPauseRenderPhasesStatus())
		{
			settings.m_SunDirection = PostFXParamBlock::GetParams(1-ub).m_SunDirection;
			settings.m_lightRaysActive = PostFXParamBlock::GetParams(1-ub).m_lightRaysActive;
			settings.m_lightrayParams = PostFXParamBlock::GetParams(1-ub).m_lightrayParams;
			settings.m_sslrParams = PostFXParamBlock::GetParams(1-ub).m_sslrParams;
		}
	}

	// If we have a camera cut / adaption reset, reset the exposure next frame
	const camBaseCamera* activeCamera = camInterface::GetDominantRenderedCamera();
	
	if (activeCamera && 
		((camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation)) > 0))
	{
		g_resetAdaptedLumCount = NUM_RESET_FRAMES;
	}

	if (g_PlayerSwitch.IsActive())
	{
		g_resetAdaptedLumCount = NUM_RESET_FRAMES;
	}

	// Reset array if needed
	if (g_historyArray.GetCapacity() != settings.m_adaptionNumFramesAvg)
	{
		g_historyArray.Reset();
		g_historyArray.Resize(settings.m_adaptionNumFramesAvg);
		g_historyIndex = 0;
		g_historySize = 0;
	}

	settings.m_isCameraCut = false;
	settings.m_justUseCalculated = false;

	bool resetRequired = (g_resetAdaptedLumCount > 0);

#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		g_setExposureToTarget = true;
	}
#endif

	if (resetRequired)
	{
		PF_SET(CalculationType, 0);

		g_historyIndex = 0;
		g_historySize = 0;

		settings.m_isCameraCut = true;
		settings.m_nextHistoryIndex = 0.0f;
		settings.m_historySize = 0.0f;

		settings.m_averageExposure = 0.0f;
		settings.m_averageTimestep = 1.0f;
	}
	else if (g_setExposureToTarget)
	{
		settings.m_averageExposure = 0.0f;
		settings.m_averageTimestep = 1.0f;
	}
	else
	{
		PF_SET(CalculationType, 1);

		float timeStep = fwTimer::GetTimeStep();

		// Update history
		settings.m_nextHistoryIndex = g_historyIndex;

		if (!CPauseMenu::GetPauseRenderPhasesStatus())
		{
			g_historyArray[g_historyIndex].exposure = g_adaptation.GetUpdateExposure();
			g_historyArray[g_historyIndex].luminance = g_adaptation.GetUpdateLum();
			g_historyArray[g_historyIndex].timestep = timeStep;
			g_historyIndex++;

			g_historySize = rage::Max(g_historySize, g_historyIndex);
			g_historyIndex = g_historyIndex % settings.m_adaptionNumFramesAvg;
		}
		else
		{
			resetRequired = true;
		}

		settings.m_historySize = g_historySize;

		// Get avg. exposure and timestep
		float avgExposure = 0.0f;
		float avgTimeStep = 0.0f;

		for (u32 i = 0; i < g_historySize; i++)
		{
			avgExposure += g_historyArray[i].exposure;
			avgTimeStep += g_historyArray[i].timestep;
		}

		float historySize = rage::Max((float)g_historySize, 1.0f);

		settings.m_averageExposure = avgExposure / historySize;
		settings.m_averageTimestep = avgTimeStep / historySize;
	}

	settings.m_adaptationResetRequired = resetRequired;

	if (g_resetAdaptedLumCount > 0 && !fwTimer::IsGamePaused())
	{
		g_resetAdaptedLumCount--;
	}

	// Exposure push when we look at the sun
	if (!resetRequired && g_enableExposureTweak)
	{
		settings.m_exposureTweak += 
			g_sunExposureAdjust * 
			g_LensFlare.GetSunVisibility() * 
			Saturate(Lights::GetUpdateDirectionalLight().GetIntensity() / 64.0f);
	}

	#if __BANK
	sprintf(
		adaptionInfoStr, 
		"Cur Lum : %.2f, Current Exposure : %.4f, Target Exposure : %.4f, Diff : %.2f",
		g_adaptation.GetUpdateLum(),
		g_adaptation.GetUpdateExposure(),
		g_adaptation.GetUpdateTargetExposure(),
		g_adaptation.GetUpdateTargetExposure() - g_adaptation.GetUpdateExposure());

	#endif //__BANK

	PF_SET(UpdateExposure, g_adaptation.GetUpdateExposure());
	PF_SET(Luminance, g_adaptation.GetUpdateLum());

	g_defaultFlashEffect.Update();

#if __BANK

	if( false == g_Override )
	{
		sysMemCpy(&editedSettings,&settings,sizeof(PostFXParamBlock::paramBlock)); 
	}
	else
	{
		editedSettings.m_adaptationResetRequired = settings.m_adaptationResetRequired;
		editedSettings.m_averageExposure = settings.m_averageExposure;
		editedSettings.m_averageTimestep = settings.m_averageTimestep;
		editedSettings.m_historySize = settings.m_historySize;
		editedSettings.m_nextHistoryIndex = settings.m_nextHistoryIndex;

		// Let the LENSARTEFACTSMGR widget drive the intensity multipliers
		editedSettings.m_lensArtefactsGlobalMultiplier = LENSARTEFACTSMGR.GetGlobalIntensity();
		editedSettings.m_lensArtefactsMinExposureMultiplier = LENSARTEFACTSMGR.GetExposureMinForFade();
		editedSettings.m_lensArtefactsMaxExposureMultiplier = LENSARTEFACTSMGR.GetExposureMaxForFade();

		sysMemCpy(&settings,&editedSettings,sizeof(PostFXParamBlock::paramBlock));
	}

	if (settings.m_directionalBlurLength>0.0f)
	{
		grcDebugDraw::AddDebugOutput("POSTFX WARNING: Motion Blur is ON!");
	}
	
	if( settings.m_HighDof )
	{
		grcDebugDraw::AddDebugOutput("POSTFX WARNING: HQ DOF (%s, %s)", 
																		settings.m_HighDofForceSmallBlur ?  "SB" : "DB",
																		settings.m_ShallowDof ? "SD" : "DD" );
	}

	sprintf(toneMapInfoStr, "Exposure = %.2f | Exposure adjustment = %.2f | Dir intensity = %.2f | Actual maximum = %.2f", 
		g_adaptation.GetUpdateExposure(), 
		g_adaptation.GetUpdatePowTwoExposure(),
		Lights::GetUpdateDirectionalLight().GetIntensity(),
		Lights::GetUpdateDirectionalLight().GetIntensity() * g_adaptation.GetUpdatePowTwoExposure());
#endif // __BANK

	UpdatePulseEffect();

	float exposure = g_adaptation.GetUpdateExposure();
	float exposureMax = settings.m_exposureMax;
	float exposureMin = settings.m_exposureMin;

	if(exposureMin == exposureMax)
	{
		exposureMax += 0.0001f;			// B*7172555 - PS4: Error: NaN detected in shader parameters! from DL_MIRROR_REFLECTION
	}

	g_linearExposure = Saturate((exposure - exposureMin)/(exposureMax - exposureMin));

	// Come up with an average linear exposure value
	float averageExposur = resetRequired ? g_adaptation.GetUpdateExposure() : settings.m_averageExposure; 
	g_averageLinearExposure =  Saturate((averageExposur - exposureMin)/(exposureMax - exposureMin));
	
	settings.m_timeStep = fwTimer::GetTimeStep();
	settings.m_time = (float)fwTimer::GetTimeInMilliseconds();

	settings.m_AdaptiveDofTimeStep = fwTimer::IsGamePaused() ? 0.0f : fwTimer::GetTimeStep_NonScaledClipped();

#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		settings.m_AdaptiveDofTimeStep = fwTimer::GetTimeStep_NonPausedNonScaledClipped();
		if(fwTimer::IsUserPaused() && g_UpdateReplayEffectsWhilePaused)
		{
			settings.m_timeStep = fwTimer::GetNonPausedTimeStep();
			settings.m_time = (float)fwTimer::GetNonPausedTimeInMilliseconds();
		}
	}
#endif //GTA_REPLAY

	// Adjust bloom threshold based on exposure diff so we have more will adaptation is kicking in
	const float exposureDiff = (g_adaptation.GetUpdateTargetExposure() - g_adaptation.GetUpdateExposure());
	if (exposureDiff < 0.0f && g_bloomThresholdExposureDiffMin != g_bloomThresholdExposureDiffMax && !settings.m_adaptationResetRequired)
	{
		float t = 0.0f;

		if (g_bloomThresholdExposureDiffMin >= g_bloomThresholdExposureDiffMax)
		{
			Assertf(false,"Bloom threshold exposure difference boudaries are not correct, revise visualsettings.dat");
			t = g_bloomThresholdExposureDiffMin;
		}
		else
		{
			t = pow(SmoothStep(-exposureDiff, g_bloomThresholdExposureDiffMin, g_bloomThresholdExposureDiffMax), g_bloomThresholdPower);
		}

		settings.m_bloomThresholdMax = Saturate(Lerp(
			t, 
			settings.m_bloomThresholdMax, 
			settings.m_bloomThresholdMax - g_bloomThresholdMin));
	}

#if __BANK

	if (g_bDebugTriggerPulseEffect)
	{
		g_bDebugTriggerPulseEffect = false;
		SetPulseParameters(g_debugPulseEffectParams, g_debugPulseEffectRampUpDuration, g_debugPulseEffectHoldDuration, g_debugPulseEffectRampDownDuration);
	}

	if (gDisableScreenBlurFadeOut)
	{
		g_screenBlurFade.Reset();
		gDisableScreenBlurFadeOut = false;
		gTriggerScreenBlurFadeOut = false;
		gTriggerScreenBlurFadeIn = false;
	}
	if (gTriggerScreenBlurFadeOut || gTriggerScreenBlurFadeIn)
	{
		g_screenBlurFade.Trigger(gTriggerScreenBlurFadeOut);
		gTriggerScreenBlurFadeOut = false;
		gTriggerScreenBlurFadeIn = false;
	}
#endif

#if ADAPTIVE_DOF_CPU
	bool bGaussian = (GRCDEVICE.GetDxFeatureLevel() >=1100) ? true : false;
#if RSG_PC
	bGaussian &= ((GET_POSTFX_SETTING(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX) >= CSettings::High) && (GRCDEVICE.IsStereoEnabled() == false)) ? true : false;
#endif
	if (!bGaussian && UseAdaptiveDofCPU())
		AdaptiveDepthOfField.ProcessAdaptiveDofCPU();
#endif

	UpdateDefaultMotionBlur();

	PostFX::SetDofParams();
	PostFX::SetMotionBlur();
	PostFX::SetFograyParams(CRenderPhaseCascadeShadowsInterface::IsShadowActive());

	Vec3V currCameraPos = Vec3V(V_ZERO);
	Vec3V prevCameraPos = Vec3V(V_ZERO);

	float blurSize = fpvMotionBlurSize;
	Vector4 weights = fpvMotionBlurWeights;
	float maxVelocity = fpvMotionVelocityMaxSize;

	bool enableMotionBlur = g_fpvMotionBlurEnable;

	if (g_fpvMotionBlurEnableDynamicUpdate)
	{
		currCameraPos = fpvMotionBlurCurrCameraPos;
		prevCameraPos = fpvMotionBlurPrevCameraPos[0];

		if (!fwTimer::IsGamePaused())
		{
			fpvMotionBlurPrevCameraPos[1] = fpvMotionBlurPrevCameraPos[0];
			fpvMotionBlurPrevCameraPos[0] = currCameraPos;
		}
		else
		{
			prevCameraPos = fpvMotionBlurPrevCameraPos[1];
			currCameraPos = fpvMotionBlurPrevCameraPos[0];
		}
	}
	else
	{
		const CClipEventTags::CMotionBlurEventTag::MotionBlurData* mbData = CClipEventTags::GetCurrentMotionBlur();
		if (mbData != NULL)
		{
			currCameraPos = mbData->GetCurrentPosition();
			prevCameraPos = mbData->GetLastPosition();
			if (!g_fpvMotionBlurOverrideTagData)
			{
				blurSize = mbData->GetCurrentBlurAmount();
				weights = Vector4(1.0f, 0.0f, mbData->GetVelocityBlend(), mbData->GetNoisiness());
				maxVelocity = mbData->GetMaximumVelocity();
			}
			enableMotionBlur = true;
		}
		else
		{
			fpvMotionBlurVelocity = Vec2V(0.0f, 0.0f);
			fpvMotionBlurFirstFrame = true;
		}
	}

	Vec4V plane = BuildPlane(Vec3V(V_ZERO), Vec3V(0.0f, 0.0f, -1.0f));

	Vec3V ssCurrPos = PlaneProject(plane, currCameraPos);
	Vec3V ssPrevPos = PlaneProject(plane, prevCameraPos);

	Vec2V diff = Subtract(ssCurrPos.GetXY(), ssPrevPos.GetXY());
	Vec2V ssPosDiff = Scale(diff, Vec2V(SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f));
	ssPosDiff = Scale(ssPosDiff, Vec2V(1.0f, -1.0f));

#if __BANK
	if (g_fpvMotionBlurDrawDebug)
	{
		Vec2V debugCurrPos = ssCurrPos.GetXY() * Vec2V(V_HALF) + Vec2V(V_HALF);
		Vec2V debugPrevPos = ssPrevPos.GetXY() * Vec2V(V_HALF) + Vec2V(V_HALF);
		grcDebugDraw::Circle(debugCurrPos, 0.01f, Color32(255, 0, 0, 128));
		grcDebugDraw::Circle(debugPrevPos, 0.01f, Color32(0, 0, 255, 128));
		grcDebugDraw::Line(debugCurrPos, debugPrevPos, Color32(255, 0, 0, 255), Color32(0, 0, 255, 255));
	}
#endif
	
	if (enableMotionBlur && fpvMotionBlurFirstFrame)
	{
		fpvMotionBlurVelocity = ssPosDiff;
		fpvMotionBlurFirstFrame = false;
	}

	Vec2V velocity = 
		Scale(fpvMotionBlurVelocity, ScalarV(1.0f - weights.z)) + 
		Scale(ssPosDiff, ScalarV(weights.z)) * 
		ScalarV((1.0f / fwTimer::GetTimeStep()) / 30.0f);

	velocity = Clamp(velocity,
		Vec2V(-maxVelocity, -maxVelocity),
		Vec2V( maxVelocity,  maxVelocity));

	settings.m_fpvMotionBlurVelocity = velocity;

	if (!fwTimer::IsGamePaused())
	{
		fpvMotionBlurVelocity = velocity;
	}

	settings.m_fpvMotionBlurWeights = weights;
	settings.m_fpvMotionBlurSize = blurSize;

	bool applyFPVMB = camInterface::IsRenderingFirstPersonShooterCamera();

	if (activeCamera && activeCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		applyFPVMB = static_cast<const camCinematicMountedCamera*>(activeCamera)->IsFirstPersonCamera();
	}

#if RSG_PC
	applyFPVMB &= (GET_POSTFX_SETTING(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX) >= CSettings::Medium) ? true : false;
#endif

	settings.m_fpvMotionBlurActive = applyFPVMB && enableMotionBlur && !CNewHud::IsWeaponWheelActive();
}

#if __BANK
void PostFX::RenderDofDebug(grcRenderTarget* pDepthBlurTarget, const PostFX::PostFXParamBlock::paramBlock &settings)
{
#if ADAPTIVE_DOF_GPU
	AdaptiveDepthOfField.DrawFocusGrid();
#endif //ADAPTIVE_DOF_GPU
#if DOF_COMPUTE_GAUSSIAN
	if( g_DrawCOCOverlay )
	{
#if RSG_DURANGO && __BANK
		if (AltBokehDepthBlur)
		{
			ESRAMManager::ESRAMCopyIn(BokehDepthBlur, AltBokehDepthBlur);
		}
#endif
		grcDepthStencilStateHandle currentDSS = grcStateBlock::DSS_Active;
		grcBlendStateHandle currentBS = grcStateBlock::BS_Active;
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

		dofComputeShader->SetVar( g_cocOverlayAlpha, (float)g_cocOverlayAlphaVal);

		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		dofComputeShader->SetVar(dofComputeColorTex, pDepthBlurTarget);

		u32 COCOverlayPass = pp_DOF_ComputeGaussian_COC_Overlay;
#if ADAPTIVE_DOF_OUTPUT_UAV
		if( settings.m_AdaptiveDof )
			COCOverlayPass = pp_DOF_ComputeGaussian_COC_Overlay_Adaptive;
#endif //ADAPTIVE_DOF_OUTPUT_UAV
		ShaderUtil::StartShader("DoF COC Overlay", dofComputeShader, dofComputeTechnique, COCOverlayPass);

		grcDrawSingleQuadf(0.f,0.f,1.f,1.f, 0.0f, 0.0, 0.0, 0.0f, 0.0f, Color32(0));
		ShaderUtil::EndShader(dofComputeShader);

		grcStateBlock::SetDepthStencilState(currentDSS);
		grcStateBlock::SetBlendState(currentBS);
	}
#endif //DOF_COMPUTE_GAUSSIAN
}
#endif //__BANK

void PostFX::ProcessUISubSampleAlpha(grcRenderTarget* pDstColorTarget, grcRenderTarget* pSrcColorTarget, grcRenderTarget* pSrcAlphaTarget, grcRenderTarget* pDepthStencilTarget, const Vector4 &scissorRect)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PIXBegin(0, "SSA Combine");
	GRCDBG_PUSH("SSA Combine");

	grcResolveFlags flags;
	#if SSA_USES_EARLYSTENCIL
		grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, pDepthStencilTarget); // Binding just the depth buffer saves ~0.025ms on PS3

		GRCDEVICE.SetScissor((int)scissorRect.x, (int)scissorRect.y, (int)scissorRect.z, (int)scissorRect.w);

		CRenderTargets::ConvertAlphaMaskToStencilMask( 0, pSrcAlphaTarget );
		grcStateBlock::SetRasterizerState(subSampleAlphaRasterizerState);
		grcStateBlock::SetBlendState(subSampleAlphaBlendState);		
		grcStateBlock::SetDepthStencilState(subSampleAlphaWithStencilCullDSState, DEFERRED_MATERIAL_SPECIALBIT);

		// unlock target
		#if __XENON
			flags.NeedResolve = true;
		#endif
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

		#if __PS3
			if (pSrcColorTarget != pDstColorTarget)
			{
				grcTextureFactory::GetInstance().LockRenderTarget(0, pSrcColorTarget, NULL); // Re-bind the color buffer
				GRCDEVICE.SetScissor((int)scissorRect.x, (int)scissorRect.y, (int)scissorRect.z, (int)scissorRect.w);
				CSprite2d copyBuffer;
				copyBuffer.BeginCustomList(CSprite2d::CS_BLIT_NOFILTERING, pDstColorTarget);
				grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f,Color32(255, 255, 255, 255));
				copyBuffer.EndCustomList();
				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			}
		#endif

#if RSG_PC 
		Assertf((pSrcColorTarget != pDstColorTarget), "Make sure source contains copy of destination");
		Assertf((pSrcAlphaTarget != pDstColorTarget), "Source alpha can not share with destination texture");
#endif // RSG_PC
	#else // SSA_USES_EARLYSTENCIL

		grcStateBlock::SetRasterizerState(subSampleAlphaRasterizerState);
		grcStateBlock::SetBlendState(subSampleAlphaBlendState);
		grcStateBlock::SetDepthStencilState(subSampleAlphaDSState);
	#endif // SSA_USES_EARLYSTENCIL
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstColorTarget, pDepthStencilTarget); // Re-bind the color buffer

	grcRenderTarget* target = pSrcColorTarget;
#if RSG_DURANGO || RSG_PC
	DURANGO_ONLY(if(!GRCDEVICE.GetMSAA()))
	{
		float GBufferSizeX = 1.0f;
		float GBufferSizeY = 1.0f;
		if(pSrcAlphaTarget)
		{
			GBufferSizeX = (float)pSrcAlphaTarget->GetWidth();
			GBufferSizeY = (float)pSrcAlphaTarget->GetHeight();			
		}
		PostFXShader->SetVar(GBufferTexture0ParamId, Vec4V(GBufferSizeX, GBufferSizeY, 1.0f/GBufferSizeX, 1.0f/GBufferSizeY));
		PostFXShader->SetVar(GBufferTextureId0, pSrcAlphaTarget);
	}
#endif
	PostFXShader->SetVar(TextureID_0, target);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(target->GetWidth()),1.0f/float(target->GetHeight()), 0.0, 0.0f));
	PostFXShader->SetVar(GBufferTextureIdSSAODepth, pSrcAlphaTarget);

	GRCDEVICE.SetScissor((int)scissorRect.x, (int)scissorRect.y, (int)scissorRect.z, (int)scissorRect.w);

#if DEVICE_MSAA
	if(GRCDEVICE.GetMSAA()>1)
	{
		PostFXShader->SetVar(TextureHDR_AA, target);
	}
#endif

	PostFXBlit(NULL, PostFXTechnique, pp_subsampleAlphaUI, false);

	#if SSA_USES_EARLYSTENCIL
		// Disable early-stencil
		XENON_ONLY( GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZENABLE, false ) ); 
		DeferredLighting::SetHiStencilCullState( DeferredLighting::CULL_OFF );

		#if __XENON
			D3DRECT srcRect = { 0, 0, pDstColorTarget->GetWidth(), pDstColorTarget->GetHeight()};
			D3DPOINT dstPoint = { 0, 0 };
			GRCDEVICE.GetCurrent()->Resolve(
				D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, 
				&srcRect, 
				pDstColorTarget->GetTexturePtr(), 
				&dstPoint, 
				0, 0, NULL, 0.0f, 0L, NULL);
			flags.NeedResolve = false;
		#endif
	#endif // SSA_USES_EARLYSTENCIL
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

	GRCDBG_POP();
	PIXEnd();
}

void PostFX::ProcessSubSampleAlpha(grcRenderTarget* PS3_ONLY(pColorTarget), grcRenderTarget* PS3_ONLY(pDepthStencilTarget))
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PIXBegin(0, "SSA Combine");
	GRCDBG_PUSH("SSA Combine");

	#if SSA_USES_CONDITIONALRENDER	
		// The SSA Conditional Render path is only implemented efficiently for platforms that are doing early-stencil culling with SSA Combine.
		CompileTimeAssert( SSA_USES_EARLYSTENCIL );
	#endif

	#if DEVICE_GPU_WAIT
		// If we don't wait for writes to finish, we get graphical issues in the sky (particularly at night: B*1878659) due to
		// read after write hazzards. I think the fact that we are reading/writing the same render target here confuses
		// the Durango driver's built in hazard tracking. 
		GRCDEVICE.GpuWaitOnPreviousWrites();
	#endif

	if ( g_UseSubSampledAlpha)
	{
#if RSG_PC
		// Swap backbuffers to avoid copy
		CRenderTargets::UnlockSceneRenderTargets();
		CRenderTargets::SwapBackBuffer();
		CRenderTargets::LockSceneRenderTargets();
#endif

		// Use Early-Stencil culling on the consoles to only apply SSA Combine to pixels that need it. Not sure this will be 
		// worth the trouble on PC? We also support conditional rendering on the 360 & PS3.
		#if SSA_USES_CONDITIONALRENDER
		// Only do SSA Combine if there are SSA pixels visible (conditional rendering path is enabled and valid) and we're not shmooing
		if (g_UseConditionalSSA && DeferredLighting::HasValidSSAQuery() && !ShmooHandling::IsShmooEnable(PostFxShmoo)) 
		{
			grcRenderTarget* pBackBuffer = XENON_SWITCH(CRenderTargets::GetBackBuffer7e3toInt(true), CRenderTargets::GetBackBuffer());
			// Set both texture and texel size parameter.
			SetSrcTextureAndSize(TextureID_0, pBackBuffer);
			PostFXShader->SetVar(GBufferTextureId0, GBuffer::GetTarget(GBUFFER_RT_0));

			#if __XENON
				// On 360, the predicate is only valid for the 2nd tile (the 2nd tile's predicate overwrite the 1st tile's predicate).
				D3DRECT pTilingRects[NUM_TILES];
				GBuffer::CalculateTiles(pTilingRects, NUM_TILES);

				// Top tile
				GRCDEVICE.SetScissor(pTilingRects[0].x1, pTilingRects[0].y1, pTilingRects[0].x2-pTilingRects[0].x1, pTilingRects[0].y2-pTilingRects[0].y1 );
				CRenderTargets::ConvertAlphaMaskToStencilMask( 0, GBuffer::GetTarget(GBUFFER_RT_0) );
				grcStateBlock::SetRasterizerState(subSampleAlphaRasterizerState);
				grcStateBlock::SetBlendState(subSampleAlphaBlendState);		
				grcStateBlock::SetDepthStencilState(subSampleAlphaWithStencilCullDSState, DEFERRED_MATERIAL_SPECIALBIT);
				PostFXBlit(NULL,PostFXTechnique, g_UseSinglePassSSA ? pp_subsampleAlphaSinglePass :  pp_subsampleAlpha,	false);

				// Bottom tile. 
				GRCDEVICE.SetScissor(pTilingRects[1].x1, pTilingRects[1].y1, pTilingRects[1].x2-pTilingRects[1].x1, pTilingRects[1].y2-pTilingRects[1].y1 );
				DeferredLighting::BeginSSAConditionalRender();
					CRenderTargets::ConvertAlphaMaskToStencilMask( 0, GBuffer::GetTarget(GBUFFER_RT_0) );
					grcStateBlock::SetRasterizerState(subSampleAlphaRasterizerState);
					grcStateBlock::SetBlendState(subSampleAlphaBlendState);		
					grcStateBlock::SetDepthStencilState(subSampleAlphaWithStencilCullDSState, DEFERRED_MATERIAL_SPECIALBIT);
					PostFXBlit(NULL,PostFXTechnique, g_UseSinglePassSSA ? pp_subsampleAlphaSinglePass :  pp_subsampleAlpha,	false);
				DeferredLighting::EndSSAConditionalRender();

				// Reset scissor
				GRCDEVICE.SetScissor(0, 0, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight());
			#else // __XENON
				DeferredLighting::BeginSSAConditionalRender();
					PS3_ONLY(grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, pDepthStencilTarget)); // Binding just the depth buffer saves ~0.025ms on PS3
					CRenderTargets::ConvertAlphaMaskToStencilMask( 0, GBuffer::GetTarget(GBUFFER_RT_0) );
					grcStateBlock::SetRasterizerState(subSampleAlphaRasterizerState);
					grcStateBlock::SetBlendState(subSampleAlphaBlendState);		
					grcStateBlock::SetDepthStencilState(subSampleAlphaWithStencilCullDSState, DEFERRED_MATERIAL_SPECIALBIT);
					PS3_ONLY(grcTextureFactory::GetInstance().LockRenderTarget(0, pColorTarget, pDepthStencilTarget)); // Re-bind the color buffer
					PostFXBlit(NULL,PostFXTechnique, g_UseSinglePassSSA ? pp_subsampleAlphaSinglePass :  pp_subsampleAlpha,	false);
				DeferredLighting::EndSSAConditionalRender();
			#endif // __XENON
		}
		else // Non-conditional rendering path
		#endif // #if SSA_USES_CONDITIONALRENDER
		{
			#if SSA_USES_EARLYSTENCIL
				PS3_ONLY(grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, pDepthStencilTarget)); // Binding just the depth buffer saves ~0.025ms on PS3
				CRenderTargets::ConvertAlphaMaskToStencilMask( 0, GBuffer::GetTarget(GBUFFER_RT_0) );
				grcStateBlock::SetRasterizerState(subSampleAlphaRasterizerState);
				grcStateBlock::SetBlendState(subSampleAlphaBlendState);		
				grcStateBlock::SetDepthStencilState(subSampleAlphaWithStencilCullDSState, DEFERRED_MATERIAL_SPECIALBIT);
				PS3_ONLY( grcTextureFactory::GetInstance().LockRenderTarget(0, pColorTarget, pDepthStencilTarget) ); // Re-bind the color buffer
			#else // SSA_USES_EARLYSTENCIL
				grcStateBlock::SetRasterizerState(subSampleAlphaRasterizerState);
				grcStateBlock::SetBlendState(subSampleAlphaBlendState);
				grcStateBlock::SetDepthStencilState(subSampleAlphaDSState);
			#endif // SSA_USES_EARLYSTENCIL

			const grcTexture* pBackBuffer = NULL;
#if RSG_PC
			pBackBuffer = CRenderTargets::GetBackBufferCopy();
#else
			// We don't sample outside of the 2x2 pixel quad, so there is no read/write hazard here (safe to read/write the back buffer)
			pBackBuffer = CRenderTargets::GetBackBuffer();
#endif
			
			// Set both texture and texel size parameter.
			SetSrcTextureAndSize(TextureID_0, pBackBuffer);

			float GBufferSizeX = (float)GBuffer::GetTarget(GBUFFER_RT_0)->GetWidth();
			float GBufferSizeY = (float)GBuffer::GetTarget(GBUFFER_RT_0)->GetHeight();
			PostFXShader->SetVar(GBufferTexture0ParamId, Vec4V(GBufferSizeX, GBufferSizeY, 1.f/GBufferSizeX, 1.f/GBufferSizeY));

			// Using GBufferTextureSamplerSSAODepth because it's one of the non-MSAA samplers available.
			// We can't use GBufferTextureSampler0, since this shader is postfxMS, and the GBufferTexture0 is MSAA.
			//PostFXShader->SetVar(GBufferTextureId0, GBuffer::GetTarget(GBUFFER_RT_0));
			PostFXShader->SetVar(GBufferTextureIdSSAODepth, GBuffer::GetTarget(GBUFFER_RT_0));

#if DEVICE_MSAA
			if(GRCDEVICE.GetMSAA()>1)
			{
				PostFXShader->SetVar(TextureHDR_AA, CRenderTargets::GetBackBuffer());
			}
#endif

			PostFXBlit(NULL,PostFXTechnique, g_UseSinglePassSSA ? pp_subsampleAlphaSinglePass :  pp_subsampleAlpha,	false);
		}

		#if SSA_USES_EARLYSTENCIL
			// Disable early-stencil
			XENON_ONLY( GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZENABLE, false ) ); 
			DeferredLighting::SetHiStencilCullState( DeferredLighting::CULL_OFF );
		#endif
	}

	grcStateBlock::SetRasterizerState(exitSubSampleAlphaRasterizerState);
	grcStateBlock::SetBlendState(exitSubSampleAlphaBlendState);
	grcStateBlock::SetDepthStencilState(exitSubSampleAlphaDepthStencilState);

	GRCDBG_POP();
	PIXEnd();
}


static void SetFograyPostFxParamsHelper(grmShader *const shader, grcEffectVar param, grcEffectVar fadeParam, float fograyIntensity, float fograyContrast, float fograyFadeStart, float fograyFadeEnd)
{
	Vector4 globalfograyParam = Vector4(fograyIntensity, 0.0f, fograyContrast, 0.0f);
	const float jitterDivisor = 1.0f/(PostFX::g_fograyJitterEnd - PostFX::g_fograyJitterStart);
	Vector4 globalfograyFadeParam = Vector4(fograyFadeStart, 1.0f/(fograyFadeEnd - fograyFadeStart), jitterDivisor, -PostFX::g_fograyJitterStart * jitterDivisor);
	shader->SetVar(param, globalfograyParam);
	shader->SetVar(fadeParam, globalfograyFadeParam);
}

void PostFX::SetFograyPostFxParams(float fograyIntensity, float fograyContrast, float fograyFadeStart, float fograyFadeEnd)
{
	SetFograyPostFxParamsHelper(PostFXShader, globalFograyParamId, globalFograyFadeParamId, fograyIntensity, fograyContrast, fograyFadeStart, fograyFadeEnd);
}

static void ProcessDepthRender(grmShader *const shader)
{
	if(!shader->BeginDraw(grmShader::RMC_DRAW, true, PostFXTechnique))
		return;
	
#if RSG_PC
	u32	shaderModelMajor, shaderModelMinor;

	GRCDEVICE.GetDxShaderModel(shaderModelMajor, shaderModelMinor);
#endif

	PostFXPass	pass = WIN32PC_SWITCH((shaderModelMajor == 5) ? pp_depthfx : pp_depthfx_nogather, pp_depthfx);

	ShmooHandling::BeginShmoo(PostFxShmoo, shader, (int)pass);
	shader->Bind(pass);
	GRCDEVICE.DrawPrimitive(drawTriStrip, sm_FogVertexDeclaration, *sm_FogVertexBuffer, 0, sm_FogVertexBuffer->GetVertexCount());
	shader->UnBind();
	ShmooHandling::EndShmoo(PostFxShmoo);
	shader->EndDraw();
}

void PostFX::ProcessDepthFX()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	PF_PUSH_MARKER("PostFX::ProcessDepthFX");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if POSTFX_FOGRAY
	const PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetRenderThreadParams();
	bool bFogRay = (WIN32PC_ONLY((grcEffect::GetShaderQuality() > 0) && ) (settings.m_fograyIntensity > 0.0f) && settings.m_bShadowActive);

#if DEVICE_MSAA
	// need to resolve depth for fogray (due to water rendering, etc ...)
	if (bFogRay && GRCDEVICE.GetMSAA()>1 WIN32PC_ONLY(&& Water::IsDepthUpdated()))
		CRenderTargets::ResolveDepth(depthResolve_Farthest);
#endif
#endif

	grcStateBlock::SetRasterizerState(depthFXRasterizerState);
	grcStateBlock::SetBlendState(depthFXBlendState);

	const Vector4 projParams = ShaderUtil::CalculateProjectionParams();
	PostFXShader->SetVar(DofProjId, projParams);

	const Vector2 dofShear = grcViewport::GetCurrent()->GetPerspectiveShear();
	PostFXShader->SetVar(DofShearId, Vector4(dofShear.x,dofShear.y,0.f,0.f) );

#if POSTFX_FOGRAY
	if (bFogRay)
	{
		CShaderLib::SetFogRayIntensity(settings.m_fograyIntensity);
		// copy cascade shadowmap to mini map
		PostFXShader->SetVar(ResolvedTextureIdDepth, CRenderPhaseCascadeShadowsInterface::GetShadowMap()); 

		grcRenderTarget* shadowmap = CRenderPhaseCascadeShadowsInterface::GetShadowMapMini(SM_MINI_FOGRAY);

		//This is done before the GPU Particle FX render (during PostFX::ProcessDepthFX). If in case this is moved to after Ptfx Render, we'll have to change the logic there
#if RSG_ORBIS
		PostFXBlit(shadowmap, PostFXTechnique, pp_shadowmapblit, false, false, NULL, false, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f), true); 
#else
		PostFXBlit(shadowmap, PostFXTechnique, pp_shadowmapblit, false, false, NULL, false, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f)); 
#endif

#if __ASSERT
		//Used for making sure that other systems using this shadow map will be on the same thread
		g_FogRayCascadeShadowMapDownsampleThreadId = sysIpcGetCurrentThreadId();
#endif

		CRenderPhaseCascadeShadowsInterface::SetSharedShadowMap(shadowmap);

	//#if DEVICE_MSAA
	//	#if __D3D11
	//		const grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthBufferResolved_ReadOnly();
	//	#else
	//		const grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthResolved();
	//	#endif
	////#else
	//	const grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthBuffer();
	//#endif

		PostFXShader->SetVar(ResolvedTextureIdDepth, CRenderTargets::GetDepthResolved()); 

		// fog ray param
		SetFograyPostFxParams(settings.m_fograyIntensity, settings.m_fograyContrast, settings.m_fograyFadeStart, settings.m_fograyFadeEnd);

#if RSG_ORBIS
		PostFXBlit(FogRayRT, PostFXTechnique, pp_fogray,false,true,NULL,false,Vector2(0.0f,0.0f),Vector2(1.0f,1.0f),true);
#else
		u32 FogRayPass = pp_fogray;

#if RSG_PC
		u32	shaderModelMajor, shaderModelMinor;

		GRCDEVICE.GetDxShaderModel(shaderModelMajor, shaderModelMinor);

		FogRayPass = (shaderModelMajor == 5) ? pp_fogray : pp_fogray_nogather;

		if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Ultra)
			FogRayPass = (shaderModelMajor == 5) ? pp_fogray_high : pp_fogray_high_nogather;
#endif
		PostFXBlit(FogRayRT, PostFXTechnique, FogRayPass,false,true);
#endif

		PostFXShader->SetVar(TextureID_0, FogRayRT);
		PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(FogRayRT->GetWidth()),1.0f/float(FogRayRT->GetHeight()),0.0, 0.0f));

#if RSG_ORBIS
		PostFXBlit(FogRayRT2, PostFXTechnique, pp_GaussBlurBilateral_Hor,false,false,NULL,false,Vector2(0.0f,0.0f),Vector2(1.0f,1.0f),true);
#else
		u32 BlurHorPass = pp_GaussBlurBilateral_Hor;
#if RSG_PC
		if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Ultra)
			BlurHorPass = pp_GaussBlurBilateral_Hor_High;
#endif
		PostFXBlit(FogRayRT2, PostFXTechnique, BlurHorPass);
#endif

		PostFXShader->SetVar(TextureID_0, FogRayRT2);
		PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(FogRayRT2->GetWidth()),1.0f/float(FogRayRT2->GetHeight()),0.0, 0.0f));

#if RSG_ORBIS
		PostFXBlit(FogRayRT, PostFXTechnique, pp_GaussBlurBilateral_Ver,false,false,NULL,false,Vector2(0.0f,0.0f),Vector2(1.0f,1.0f),true);
#else
		u32 BlurVerPass = pp_GaussBlurBilateral_Ver;

#if RSG_PC
		if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Ultra)
			BlurVerPass = pp_GaussBlurBilateral_Ver_High;
#endif
		PostFXBlit(FogRayRT, PostFXTechnique, BlurVerPass);
#endif

	}
	else
	{
		CRenderTargets::UnlockSceneRenderTargets();

		WIN32PC_ONLY(if(grcEffect::GetShaderQuality() > 0))
		{
			// if fogray intensity is greater than 0, and no dir. shadow active, clear out fogray RT
			// since some shaders use fogray RT when fogray intensity is > 0.0f
			grcTextureFactory::GetInstance().LockRenderTarget(0, FogRayRT, NULL);
			GRCDEVICE.Clear(true, Color32(1.0f,1.0f,1.0f,1.0f),false, 0.0f, false);
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		}
		
		CRenderTargets::LockSceneRenderTargets(true);
	}
#endif

	grcStateBlock::SetDepthStencilState(depthFXDepthStencilState, DEFERRED_MATERIAL_CLEAR); 

#if __XENON
	GRCGPRALLOCATION->SetGPRAllocation(CGPRCounts::ms_DepthFXPsRegs);  // this should probably go higher up so it can go around the sky and clouds as well.
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZENABLE, false);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE, false);
#endif // __XENON

#if !RSG_PC && !RSG_DURANGO && !__PS3 &&!RSG_ORBIS
	CRenderTargets::RefreshStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0x07);
#endif

	//use updated depth buffer
#if __PPU	
	PostFXShader->SetVar(GBufferTextureIdDepth, CRenderTargets::GetDepthBufferAsColor());
#elif __D3D11
	// On Win32PC (DX9/DX10) we use a copy of the depth buffer to avoid a read/write hazard. 
	// On DX11 we no longer need to copy, just using a read-only depth view
	grcTexture *const texDepth = CRenderTargets::LockReadOnlyDepth(false);	//note: actually locks in DX11
	PostFXShader->SetVar(GBufferTextureIdDepth, texDepth); 
#else
	PostFXShader->SetVar(GBufferTextureIdDepth, CRenderTargets::GetDepthBuffer()); 
#endif // __PPU, __D3D11

	//const Vector4 projParams = ShaderUtil::CalculateProjectionParams();
	//PostFXShader->SetVar(DofProjId, projParams);

	//const Vector2 dofShear = grcViewport::GetCurrent()->GetPerspectiveShear();
	//PostFXShader->SetVar(DofShearId, Vector4(dofShear.x,dofShear.y,0.f,0.f) );

	g_timeCycle.SetShaderData();

#if __XENON
	PostFX::SetLDR10bitHDR10bit();
#elif __PS3
	PostFX::SetLDR8bitHDR16bit();
#else
	PostFX::SetLDR16bitHDR16bit();
#endif

#if __PPU
	// Fence to be sure that the depth buffer's ready...
	grcFenceHandle fence = GRCDEVICE.InsertFence();
	GRCDEVICE.GPUBlockOnFence(fence);
#endif // __PPU

#if __XENON
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC );
#endif

#if POSTFX_FOGRAY
	// fog ray param
	PostFXShader->SetVar(TextureID_1, FogRayRT);
	float fogRayIntensity = settings.m_fograyIntensity;
	CShaderLib::SetFogRayIntensity(fogRayIntensity);
	SetFograyPostFxParams(fogRayIntensity, settings.m_fograyContrast, settings.m_fograyFadeStart, settings.m_fograyFadeEnd);
#endif

#if POSTFX_USE_TILEDFOG
	if (BANK_SWITCH(PostFX::g_UseTiledTechniques == true, CTiledLighting::IsEnabled()))
	{
		PostFXPass	pass = WIN32PC_SWITCH((shaderModelMajor == 5) ? pp_depthfx_tiled : pp_depthfx_tiled_nogather, pp_depthfx_tiled);

		const char* pPassName = NULL;
	#if RAGE_TIMEBARS
		pPassName = passName[pass];
	#endif

#if __D3D11 || RSG_ORBIS
		PostFXShader->SetVar(TiledDepthTextureId, CTiledLighting::GetClassificationTexture());
#endif // __D3D11 || RSG_ORBIS

		CTiledLighting::RenderTiles(NULL, PostFXShader, PostFXTechnique, pass, pPassName, false, false, false);
	}
	else
#endif
	{
#if MSAA_EDGE_PASS
		if (DeferredLighting::IsEdgePassActiveForPostfx() && AssertVerify(PostFXShaderMS0) BANK_ONLY(&& (DebugDeferred::m_OptimizeDepthEffects & (1<<EM_IGNORE))))
		{
			BANK_ONLY(if (DebugDeferred::m_OptimizeDepthEffects & (1<<EM_EDGE0)))
			{
				grcStateBlock::SetDepthStencilState(DeferredLighting::m_directionalEdgeMaskEqualPass_DS, EDGE_FLAG);
				ProcessDepthRender(PostFXShader);
			}
			BANK_ONLY(if (DebugDeferred::m_OptimizeDepthEffects & (1<<EM_FACE0)))
			{
#if RSG_PC
				u32	shaderModelMajor, shaderModelMinor;

				GRCDEVICE.GetDxShaderModel(shaderModelMajor, shaderModelMinor);
#endif
				PostFXPass	pass = WIN32PC_SWITCH((shaderModelMajor == 5) ? pp_depthfx : pp_depthfx_nogather, pp_depthfx);

				grcStateBlock::SetDepthStencilState(edgeMaskDepthEffectFace_DS, DEFERRED_MATERIAL_CLEAR);	// Face: s < 7
				PostFXShaderMS0->CopyPassData(PostFXTechnique, pass, PostFXShader);
				ProcessDepthRender(PostFXShaderMS0);
			}
		}else
#endif //MSAA_EDGE_PASS
		{
			ProcessDepthRender(PostFXShader);
		}
	}

#if __D3D11
	CRenderTargets::UnlockReadOnlyDepth();	//note: actually unlocks in DX11
#endif

#if __XENON
	// set the pixel shader threads to 112 -> vertex shader can only use 16 threads
	GRCGPRALLOCATION->SetGPRAllocation(0);
#endif

#if !__WIN32PC && !RSG_DURANGO && !RSG_ORBIS
	CRenderTargets::ClearStencilCull( DEFERRED_MATERIAL_CLEAR );
#endif
	grcStateBlock::SetRasterizerState(exitDepthFXRasterizerState);
	grcStateBlock::SetBlendState(exitDepthFXBlendState);
	grcStateBlock::SetDepthStencilState(exitDepthFXDepthStencilState);

	PF_POP_MARKER();
}


#if __DEV && __PS3
void CRenderPhaseCascadeShadows_DumpRenderTargetsAfterSetWaitFlip();
#endif // __DEV && __PS3

bool NeedSDDownsample()
{
#if __PS3
	return !((VideoResManager::GetUIHeight() == VideoResManager::GetNativeHeight()) && (VideoResManager::GetUIWidth() == VideoResManager::GetNativeWidth()));
#else
	return false;
#endif //__PS3
}

void GetScanlineParams(const PostFX::PostFXParamBlock::paramBlock& settings, Vector4& outParams)
{
	outParams = settings.m_scanlineParams;

	// we're blending two sets of scanlines, no need to halve the intensity in the shader
	outParams.x *= 0.5f; 

	// adjust speed
	outParams.w = settings.m_time*0.001f*settings.m_scanlineParams.w;

}

void PrimeBloomBufferFrom(const grcTexture * pSrcBuffer)
{		
	PostFXShader->SetVar(TextureID_v1, PostFX::g_currentExposureRT);
	SetSrcTextureAndSize(BloomTextureID, pSrcBuffer);		
	SetNonDepthFXStateBlocks();	
	PostFX::SetBloomParams();				
	PostFXBlit(BloomBufferHalf0, PostFXTechnique, pp_calcbloom0);

	// Run a min filter to remove bright pixels
	ProcessBloomBuffer(BloomBuffer, BloomBufferHalf0, pp_bloom_min);		

	// Keep a copy of the unblurred bloom buffer around
	ProcessBloomBuffer(BloomBufferUnfiltered, BloomBuffer, pp_DownSampleBloom);
}

void ComputeLuminance()
{
	PF_AUTO_PUSH_TIMEBAR("ComputeLuminance");

#if RSG_DURANGO
	grcRenderTarget* BloomBufferQuarterDS = BloomBufferQuarterX;
#else
	grcRenderTarget* BloomBufferQuarterDS = BloomBufferQuarter0;
#endif

	// Half to quarter size max downsample
	SetNonDepthFXStateBlocks();
	PostFXShader->SetVar(TextureID_0, HalfScreen0);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(HalfScreen0->GetWidth()),1.0f/float(HalfScreen0->GetHeight()), 0.0, 0.0f));
	PostFXBlit(BloomBufferQuarterDS, PostFXTechnique, pp_max);
		
#if AVG_LUMINANCE_COMPUTE	
	if(AverageLumunianceComputeEnable)
	{
		#if RSG_DURANGO
			g_grcCurrentContext->GpuSendPipelinedEvent(D3D11X_GPU_PIPELINED_EVENT_PS_PARTIAL_FLUSH);
		#endif

		u32 GroupCountX = (u32)((float)BloomBufferQuarterDS->GetWidth() / 16.0f);
		u32 GroupCountY = (u32)((float)BloomBufferQuarterDS->GetHeight() / 16.0f);

	
		GroupCountX += (BloomBufferQuarterDS->GetWidth() % 16) > 0 ? 1 : 0;
		GroupCountY += (BloomBufferQuarterDS->GetHeight() % 16) > 0 ? 1 : 0;

		//avgLuminanceShader->SetVar(LumCSDownsampleSrcId, BloomBufferQuarterDS);
		avgLuminanceShader->SetVar(LumCSDownsampleInitSrcId, BloomBufferQuarterDS);
		avgLuminanceShader->SetVar(LumCSDownsampleTexSizeId, Vec4f(float(PostFX::LumCSDownsampleUAV[0]->GetWidth()),float(PostFX::LumCSDownsampleUAV[0]->GetHeight()), float(BloomBufferQuarterDS->GetWidth()), float(BloomBufferQuarterDS->GetHeight())));
		avgLuminanceShader->SetVarUAV(LumCSDownsampleDstId, static_cast< grcTextureUAV*>(PostFX::LumCSDownsampleUAV[0]));
		GRCDEVICE.RunComputation("Luminance Compute CS Conversion", *avgLuminanceShader, pp_luminance_downsample_init, GroupCountX, GroupCountY, 1);

	# if __D3D11 || RSG_ORBIS
		GRCDEVICE.SynchronizeComputeToCompute();
	# endif

		for(u32 i = 1; i < PostFX::LumCSDownsampleUAVCount; i++)
		{
			GroupCountX = (u32)(PostFX::LumCSDownsampleUAV[i-1]->GetWidth() / 16.0f);
			GroupCountY = (u32)(PostFX::LumCSDownsampleUAV[i-1]->GetHeight() / 16.0f);

			GroupCountX += (PostFX::LumCSDownsampleUAV[i-1]->GetWidth() % 16) > 0 ? 1 : 0;
			GroupCountY += (PostFX::LumCSDownsampleUAV[i-1]->GetHeight() % 16) > 0 ? 1 : 0;

			avgLuminanceShader->SetVar(LumCSDownsampleSrcId, PostFX::LumCSDownsampleUAV[i-1]);
			avgLuminanceShader->SetVar(LumCSDownsampleTexSizeId, Vec4f(float(PostFX::LumCSDownsampleUAV[i]->GetWidth()),float(PostFX::LumCSDownsampleUAV[i]->GetHeight()), float(PostFX::LumCSDownsampleUAV[i-1]->GetWidth()), float(PostFX::LumCSDownsampleUAV[i-1]->GetHeight())));
			avgLuminanceShader->SetVarUAV(LumCSDownsampleDstId, static_cast< grcTextureUAV*>(PostFX::LumCSDownsampleUAV[i]));
			GRCDEVICE.RunComputation("Luminance Compute CS", *avgLuminanceShader, pp_luminance_downsample, GroupCountX, GroupCountY, 1);

	# if __D3D11 || RSG_ORBIS
			GRCDEVICE.SynchronizeComputeToCompute();
	# endif
		}

		//write to 1x1
		const u32 last_uav = PostFX::LumCSDownsampleUAVCount-1;
		avgLuminanceShader->SetVar(LumCSDownsampleSrcId, PostFX::LumCSDownsampleUAV[last_uav]);
		avgLuminanceShader->SetVar(LumCSDownsampleTexSizeId, Vec4f(float(PostFX::LumCSDownsampleUAV[last_uav+1]->GetWidth()),float(PostFX::LumCSDownsampleUAV[last_uav+1]->GetWidth()), float(PostFX::LumCSDownsampleUAV[last_uav]->GetWidth()), float(PostFX::LumCSDownsampleUAV[last_uav]->GetHeight())));
		avgLuminanceShader->SetVarUAV(LumCSDownsampleDstId, static_cast< grcTextureUAV*>(PostFX::LumCSDownsampleUAV[PostFX::LumCSDownsampleUAVCount]));
		GRCDEVICE.RunComputation("Luminance Compute CS Final", *avgLuminanceShader, pp_luminance_downsample, 1, 1, 1);

	# if __D3D11 || RSG_ORBIS
		GRCDEVICE.SynchronizeComputeToGraphics();
	# endif
	}
	else
	{
#endif //AVG_LUMINANCE_COMPUTE
	// Initial downsample luminance pass
	SetNonDepthFXStateBlocks();
	PostFXShader->SetVar(TextureID_v0, BloomBufferQuarterDS);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(BloomBufferQuarterDS->GetWidth()),1.0f/float(BloomBufferQuarterDS->GetHeight()), float(BloomBufferQuarterDS->GetWidth()), float(BloomBufferQuarterDS->GetHeight())));
	
	Vector4 lumDownsampleSrcDstOOSize = Vector4(1.0f/float(BloomBufferQuarterDS->GetWidth()), 1.0f/float(BloomBufferQuarterDS->GetHeight()), 1.0f/float(PostFX::LumDownsampleRT[0]->GetWidth()), 1.0f/float(PostFX::LumDownsampleRT[0]->GetHeight()));
#if (RSG_PC)
	for (int i = 0; i < PostFX::ImLumDownsampleRTCount; i++)
	{		
		if(i != 0) 
		{
			PostFXShader->SetVar(TextureID_0,PostFX::ImLumDownsampleRT[i-1]);
			PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(PostFX::ImLumDownsampleRT[i-1]->GetWidth()),1.0f/float(PostFX::ImLumDownsampleRT[i-1]->GetHeight()),0.0, 0.0f));
		}		
		
		PostFXBlit(PostFX::ImLumDownsampleRT[i], PostFXTechnique, pp_max, false, false);
	}		
	if(PostFX::ImLumDownsampleRTCount > 0)
	{
		PostFXShader->SetVar(TextureID_v0, PostFX::ImLumDownsampleRT[PostFX::ImLumDownsampleRTCount-1]);
		lumDownsampleSrcDstOOSize = Vector4(1.0f/float(PostFX::ImLumDownsampleRT[PostFX::ImLumDownsampleRTCount-1]->GetWidth()), 1.0f/float(PostFX::ImLumDownsampleRT[PostFX::ImLumDownsampleRTCount-1]->GetHeight()), 1.0f/float(PostFX::LumDownsampleRT[0]->GetWidth()), 1.0f/float(PostFX::LumDownsampleRT[0]->GetHeight()));
	}	
#endif //(RSG_PC	&& !AVG_LUMINANCE_COMPUTE)	
	PostFXShader->SetVar(LuminanceDownsampleOOSrcDstSizeId, lumDownsampleSrcDstOOSize);
	PostFXBlit(PostFX::LumDownsampleRT[0], PostFXTechnique, LumDownsamplePass[0], false, false, NULL, true);

	// Luminance downsample passes down to 1x1
	for (int i = 1; i < PostFX::LumDownsampleRTCount; i++)
	{
		SetNonDepthFXStateBlocks();
		Vector4 lumDownsampleSrcDstOOSize = Vector4(
			1.0f / float(PostFX::LumDownsampleRT[i-1]->GetWidth()), 1.0f / float(PostFX::LumDownsampleRT[i-1]->GetHeight()), 
			1.0f / float(PostFX::LumDownsampleRT[i-0]->GetWidth()), 1.0f / float(PostFX::LumDownsampleRT[i-0]->GetHeight()));
		PostFXShader->SetVar(LuminanceDownsampleOOSrcDstSizeId, lumDownsampleSrcDstOOSize);
		PostFXShader->SetVar(TextureID_v0, PostFX::LumDownsampleRT[i-1]);
		PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(PostFX::LumDownsampleRT[i-1]->GetWidth()),1.0f/float(PostFX::LumDownsampleRT[i-1]->GetHeight()), 0.0, 0.0f));
		PostFXBlit(PostFX::LumDownsampleRT[i], PostFXTechnique, LumDownsamplePass[i], false, false, NULL, true);
	}
#if AVG_LUMINANCE_COMPUTE
}
#endif

}

#if USE_NV_TXAA
void CopyToNvMatrix2(const Mat44V& mIn, gfsdk_float4x4& mOut)
{
	mOut._11 = mIn.GetM00f();
	mOut._21 = mIn.GetM10f();
	mOut._31 = mIn.GetM20f();
	mOut._41 = mIn.GetM30f();

	mOut._12 = mIn.GetM01f();
	mOut._22 = mIn.GetM11f();
	mOut._32 = mIn.GetM21f();
	mOut._42 = mIn.GetM31f();

	mOut._13 = mIn.GetM02f();
	mOut._23 = mIn.GetM12f();
	mOut._33 = mIn.GetM22f();
	mOut._43 = mIn.GetM32f();

	mOut._14 = mIn.GetM03f();
	mOut._24 = mIn.GetM13f();
	mOut._34 = mIn.GetM23f();
	mOut._44 = mIn.GetM33f();
}

template <class T>
#ifdef DEBUG
void AssertDXInterfaceType(IUnknown* pI)
{
	T* pCast = NULL;
	HRESULT hr = pI->QueryInterface(_uuidof(T), (void**)&pCast);
	Assert(SUCCEEDED(hr));
}
#else
void AssertDXInterfaceType(IUnknown*)	{}
#endif

bool PostFX::IsTXAAAvailable()
{
	const bool isNV = GRCDEVICE.GetManufacturer() == NVIDIA;
	const bool isDX11 = GRCDEVICE.GetDxFeatureLevel() >= 1100;
	const bool isMSAA = GRCDEVICE.GetMSAA() == grcDevice::MSAA_2 || GRCDEVICE.GetMSAA() == grcDevice::MSAA_4;
	const bool isSupportedByCard = GRCDEVICE.GetTXAASupported();
	return ( isNV && isDX11 && isMSAA && isSupportedByCard );
}

static void ResolveTXAA(grcRenderTarget* pColourDst, grcRenderTargetMSAA* pColourSrc)
{
	ID3D11ShaderResourceView* pSrcSRV = pColourSrc->GetTextureView();

	grcRenderTargetDX11* hackyCastDst = reinterpret_cast<grcRenderTargetDX11*>(pColourDst);
	const grcDeviceView* devView = hackyCastDst->GetTargetView();
	ID3D11RenderTargetView* pDstRTV = reinterpret_cast<ID3D11RenderTargetView*>(const_cast<grcDeviceView*>(devView));

	// TXAA works best with a min depth resolve.  Using the debugger, depth seems to be resolved *three* times per frame.  
	// The last one to this target does appear to be a min resolve.  It's not clear how to guarantee min without redundantly
	// resolving depth here ourselves or redesigning the interface to track the contents of the resolved target.
	ID3D11ShaderResourceView* pDepthSRV = CRenderTargets::GetDepthResolved()->GetTextureView();

	// We need a casting frenzy from Rage types to DX above.  Paranoid check that it worked.
	AssertDXInterfaceType<ID3D11DeviceContext>(rage::g_grcCurrentContext);
	AssertDXInterfaceType<ID3D11RenderTargetView>(pDstRTV);
	AssertDXInterfaceType<ID3D11ShaderResourceView>(pSrcSRV);
	AssertDXInterfaceType<ID3D11ShaderResourceView>(pDepthSRV);
	AssertDXInterfaceType<ID3D11ShaderResourceView>(pSrcSRV);

	grcViewport* vp = grcViewport::GetCurrent();
	if (vp != NULL)
	{
		Mat44V_ConstRef view = vp->GetViewMtx();
		Mat44V_ConstRef proj = vp->GetProjection();
		Mat44V viewProj;
		Multiply(viewProj,proj,view);

		// TBD: the previous view proj matrix probably shouldn't be a local static.
		gfsdk_float4x4 nvViewProj;
		CopyToNvMatrix2(viewProj, nvViewProj);
		static gfsdk_float4x4 oldNvViewProj = nvViewProj;

		const gfsdk_F32 w = (gfsdk_F32) pColourSrc->GetWidth();
		const gfsdk_F32 h = (gfsdk_F32) pColourSrc->GetHeight();

		// We need a copy of the previous frame's TXAA output.
		ID3D11ShaderResourceView* pPreviousSRV = g_TXAAResolve->GetTextureView();

		// If the dev mode is not TXAA_MODE_TOTAL, then use that.  Otherwise, choose a mode based on the MSAA count.
		gfsdk_U32 mode = g_TXAADevMode;
		if (mode == TXAA_MODE_TOTAL)
		{
			switch (GRCDEVICE.GetMSAA())
			{
				// z_2x and z_4x are the only valid non-dev modes (m could be but we are not supplying motion vectors from the game.
				case 2: mode = TXAA_MODE_Z_2xTXAA; break;
				case 4: mode = TXAA_MODE_Z_4xTXAA; break;
			}
		}

		D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"NV TXAA resolve");
		TxaaResolveDX(&g_txaaContext, rage::g_grcCurrentContext, 
			pDstRTV,					// Destination texture.
			pSrcSRV,					// Source MSAA texture shader resource view.
			pDepthSRV,					// Source motion vector or depth texture SRV.
			pPreviousSRV,				// SRV to destination from prior frame.
			mode,						// TXAA_MODE_* select TXAA resolve mode.
			w,							// Source/destination texture dimensions in pixels.
			h,
			0.1f,						// first depth position 0-1 in Z buffer
			0.9f,						// second depth position 0-1 in Z buffer 
			0.125f,						// first depth position motion limit in pixels
			32.0f,						// second depth position motion limit in pixels
			&(nvViewProj._11),			// matrix for world to view projection (current frame)
			&(oldNvViewProj._11));		// matrix for world to view projection (prior frame)
		D3DPERF_EndEvent();

		oldNvViewProj = nvViewProj;

		// going nuclear on cached states.
		GRCDEVICE.ResetContext();
		GRCDEVICE.ClearCachedState();

		grcStateBlock::MakeDirty();

		PIXBegin(0, "NV TXAA copy result");

		NVDX_ObjectHandle pNvAPIObj = 0;
		ID3D11Device* pDev = NULL;
		g_grcCurrentContext->GetDevice(&pDev);

		if (pDev)
		{
			// We indicate to the driver when we start and end rendering to g_TXAAResolve.  If the driver knows when we
			// have finished updating this resource for the frame, it can "early push" it to other GPUs in the system.
			NvAPI_D3D_GetObjectHandleForResource(pDev, g_TXAAResolve->GetTexturePtr(), &pNvAPIObj);
			if (pNvAPIObj)
				NvAPI_D3D_BeginResourceRendering(pDev, pNvAPIObj, NVAPI_D3D_RR_FLAG_FORCE_KEEP_CONTENT);
		}

		Assert(g_TXAAResolve != NULL);
		g_TXAAResolve->Copy(pColourDst);
		if (pDev && pNvAPIObj)
			NvAPI_D3D_EndResourceRendering(pDev, pNvAPIObj, 0);
		PIXEnd();
	}

}
#endif // USE_NV_TXAA

#if RSG_PC
void PostFX::DoPauseResolve(PostFX::eResolvePause PauseState)
{
	if ( CLandingPageArbiter::ShouldGameRunLandingPageUpdate() && (PauseState == UNRESOLVE_PAUSE) && (g_DoPauseResolve != IN_RESOLVE_PAUSE) && GRCDEVICE.UsingMultipleGPUs())
		return;

	g_DoPauseResolve = PauseState;
}

PostFX::eResolvePause PostFX::GetPauseResolve()
{
	return g_DoPauseResolve;
}
#endif

void PostFX::ProcessNonDepthFX()
{
	PF_AUTO_PUSH_TIMEBAR("PostFX::ProcessNonDepthFX");

	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	// KS/AH - we used to wait for flip HERE!. 

#if RSG_PC
	CShaderLib::SetStereoTexture();
#endif

	PF_PUSH_TIMEBAR("Initialize Exposure");
	if (!ExposureInitialised)
	{
		grcTextureLock lock;
		for (int i = 0; i < MAX_EXP_TARGETS; i++)
		{
			#if __D3D11
				grcTextureD3D11* currentTarget = (grcTextureD3D11*)CurExposureTex[i];
				currentTarget->UpdateCPUCopy();
			#else
				grcRenderTarget* currentTarget = CurExposureRT[i];
			#endif

			#if RSG_PC
			if(currentTarget->MapStagingBufferToBackingStore(true))
			{
			#endif
			if (currentTarget->LockRect(0,0,lock,grcsWrite))
			{
				float* currentRT = (float*)lock.Base;
				currentRT[0] = 0.0f;
				currentRT[1] = 0.0f;
				currentRT[2] = 0.0f;
				currentRT[3] = 0.0f;
				currentTarget->UnlockRect(lock);
			}
			#if RSG_PC
			}
			#endif
		}

		ExposureInitialised = true;
	}
	PF_POP_TIMEBAR(); //"Initialize Exposure"

	if( CPauseMenu::GetPauseRenderPhasesStatus() )
	{
#if PTFX_APPLY_DOF_TO_PARTICLES
		if (CPtFxManager::UseParticleDOF())
			g_ptFxManager.ClearPtfxDepthAlphaMap();
#endif //PTFX_APPLY_DOF_TO_PARTICLES
	}

	CShaderLib::UpdateGlobalGameScreenSize();

#if DEVICE_MSAA
	//We need the min depth here for the adaptive dof and depth of field blur.
	if( GRCDEVICE.GetMSAA()>1)
		CRenderTargets::ResolveDepth(depthResolve_Closest);
#endif

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()


#if DOF_TYPE_CHANGEABLE_IN_RAG
		if( ProcessDOFChangeOnRenderThread )
			ProcessDOFChangeRenderThread();
#endif

	grcStateBlock::SetRasterizerState(nonDepthFXRasterizerState);
	grcStateBlock::SetBlendState(nonDepthFXBlendState);
	grcStateBlock::SetDepthStencilState(nonDepthFXDepthStencilState);

	int rb=gRenderThreadInterface.GetRenderBuffer();

	const bool bUseExtraEffects = AreExtraEffectsActive();
	const bool bExtraEffectsBlur = ExtraEffectsRequireBlur();

	const PostFXParamBlock::paramBlock& settings = PostFXParamBlock::GetRenderThreadParams();
	
	// scanline filter
	Vector4 scanlineParams;
	GetScanlineParams(settings, scanlineParams);
	PostFXShader->SetVar(ScanlineFilterParamsId, scanlineParams);

	float screenBlurFadeLevel = settings.m_screenBlurFade;

	// screen blur fade
	if (g_screenBlurFade.IsPlaying())
	{
		screenBlurFadeLevel = Lerp(g_screenBlurFade.GetFadeLevel(), screenBlurFadeLevel, 1.0f);
	}

	PostFXShader->SetVar(ScreenBlurFadeID, screenBlurFadeLevel);

	PostFX::SetLDR16bitHDR16bit();

	PostFXShader->SetVar(GBufferTextureId0, GBuffer::GetTarget(GBUFFER_RT_0));
	PostFXShader->SetVar(GBufferTextureIdDepth, CRenderTargets::GetDepthBuffer());

	Matrix44 MBPrevViewProjMatrix = MotionBlurPrevViewProjMatrix[rb];

	// transpose so we can simply dot vectors in shader
	MBPrevViewProjMatrix.Transpose();

	PostFXShader->SetVar(MBPrevViewProjMatrixXId, MBPrevViewProjMatrix.a);
	PostFXShader->SetVar(MBPrevViewProjMatrixYId, MBPrevViewProjMatrix.b);
	PostFXShader->SetVar(MBPrevViewProjMatrixWId, MBPrevViewProjMatrix.d);

	Vec4V projParams;
	Vec3V shearProj0, shearProj1, shearProj2;
	DeferredLighting::GetProjectionShaderParams(projParams, shearProj0, shearProj1, shearProj2);

	PostFXShader->SetVar(MBPerspectiveShearParams0Id, shearProj0);
	PostFXShader->SetVar(MBPerspectiveShearParams1Id, shearProj1);
	PostFXShader->SetVar(MBPerspectiveShearParams2Id, shearProj2);

	// Reduce blur length to compensate for low framerate
	const float lengthReduction = Clamp((1.0f/30.0f)/settings.m_timeStep,0.0f,1.0f);

	Vector4 motionBlurParams(	settings.m_directionalBlurLength*lengthReduction, 
								settings.m_directionalBlurGhostingFade, 
								1.0f,
								1.0f);

	const float motionBlurNumSamples = WIN32PC_ONLY((CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::Ultra) ? 16.0f :) 6.0f;
	Vector4 motionBlurSamplesParams( motionBlurNumSamples, 1.0f / motionBlurNumSamples,  0.0f, 0.0f);

	PostFXShader->SetVar( DirectionalMotionBlurParamsId, motionBlurParams);
	PostFXShader->SetVar( DirectionalMotionBlurIterParamsId, motionBlurSamplesParams );

	BANK_ONLY(SetDebugParams();)

	const bool hasMotionBlur = IsMotionBlurEnabled();
	const bool hasHeatHaze0 = (settings.m_HeatHazeOffsetParams.x != 0.0f || settings.m_HeatHazeOffsetParams.y != 0.0f)
		&& !camInterface::GetDebugDirector().IsFreeCamActive()
#if __BANK
		&& IsTiledScreenCaptureOrPanorama()==false
#endif // __BANK
		;

	bool hasHeatHaze1 = true;
	if (Water::IsCameraUnderwater())
	{
		// disable HeatHaze for stromberg when underwater in first person camera mode (B*4119583 - Stromberg - Water camera effect in first person mode):
		if(camInterface::IsDominantRenderedCameraAnyFirstPersonCamera())
		{
			CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
			if (pPlayerPed)
			{
				CVehicle *pVehicle = pPlayerPed->GetVehiclePedInside();
				if(pVehicle)
				{
					if(pVehicle->GetVehicleType()==VEHICLE_TYPE_SUBMARINECAR)
					{
						hasHeatHaze1 = false;
					}
				}
			}
		}
	}

	const bool hasHeatHaze = hasHeatHaze0 && hasHeatHaze1;



#if __PS3 || __XENON || RSG_ORBIS || __D3D11
	u32 prevIndex = ((CurExportRTIndex - 1) + NUM_EXP_TARGETS) % NUM_EXP_TARGETS;
#endif

#if RSG_ORBIS
	grcTextureLock lock;
	if (CurExposureRT[prevIndex]->LockRect(0,0,lock,grcsRead))
	{
		float* currentRT = (float*)lock.Base;

		g_adaptation.SetRenderPowTwoExposure(currentRT[0]);
		g_adaptation.SetRenderExposure(currentRT[1]);
		g_adaptation.SetRenderTargetExposure(currentRT[2]);
		g_adaptation.SetRenderLum(currentRT[3]);

		PF_SET(RenderExposure, currentRT[1]);
		CurExposureRT[prevIndex]->UnlockRect(lock);
	}
#endif

#if RSG_DURANGO
	grcTextureD3D11* currentTarget = (grcTextureD3D11*)CurExposureTex[prevIndex];
	currentTarget->UpdateCPUCopy();

	grcTextureLock lock;
	if (currentTarget->LockRect(0,0, lock, grcsRead))
	{
		float* currentRT = (float*)lock.Base;

		g_adaptation.SetRenderPowTwoExposure(currentRT[0]);
		g_adaptation.SetRenderExposure(currentRT[1]);
		g_adaptation.SetRenderTargetExposure(currentRT[2]);
		g_adaptation.SetRenderLum(currentRT[3]);

		PF_SET(RenderExposure, currentRT[1]);
		currentTarget->UnlockRect(lock);
	}
#endif

#if RSG_PC
	if (!GRCDEVICE.UsingMultipleGPUs() || g_forceExposureReadback)
	{
		PF_PUSH_TIMEBAR_BUDGETED("PostFX - Cur Exp GPU Read Back", 0.4f);

		ASSERT_ONLY(bool targetLocked = false;)
		for( u32 i = 0; i < NUM_EXP_TARGETS; i++ )
		{
			prevIndex = ((CurExportRTIndex - (1+i)) + NUM_EXP_TARGETS) % NUM_EXP_TARGETS;

			grcTextureD3D11* currentTarget = (grcTextureD3D11*)CurExposureTex[prevIndex];
			if( currentTarget->MapStagingBufferToBackingStore(true) )
			{
				grcTextureLock lock;
				if (currentTarget->LockRect( 0, 0, lock, grcsRead ))
				{
					float* currentRT = (float*)lock.Base;

					g_adaptation.SetRenderPowTwoExposure(currentRT[0]);
					g_adaptation.SetRenderExposure(currentRT[1]);
					g_adaptation.SetRenderTargetExposure(currentRT[2]);
					g_adaptation.SetRenderLum(currentRT[3]);

					PF_SET(RenderExposure, currentRT[1]);

					currentTarget->UnlockRect(lock);
					ASSERT_ONLY(targetLocked = true;)
				}
				break;
			}
		}
		PF_POP_TIMEBAR();
		#if __ASSERT
			if(!targetLocked)
				Warningf("Failed to find an exposure target that wasnt being rendered to");
		#endif
	}
#endif

	// Temporary fix while I work out WHY we have bad frames sometimes
	if (g_adaptation.GetRenderExposure() != g_adaptation.GetRenderExposure() && 
		g_adaptation.GetRenderExposure() > 100.0f)
	{
		renderWarningf("We got QNANs in the exposur RT again");
		g_adaptation.SetRenderPowTwoExposure(1.0f);
		g_adaptation.SetRenderExposure(0.0f);
		g_adaptation.SetRenderTargetExposure(0.0f);
		g_adaptation.SetRenderLum(1.0f);
	}

	PostFX::g_prevExposureRT = CurExposureRT[CurExportRTIndex];
	CurExportRTIndex = (CurExportRTIndex + 1) % NUM_EXP_TARGETS;
	PostFX::g_currentExposureRT = CurExposureRT[CurExportRTIndex];

#if __D3D11 || RSG_ORBIS

	const grcTexture *backBuffer = CRenderTargets::GetBackBuffer();
	#if DEVICE_MSAA
	if( GRCDEVICE.GetMSAA()>1)
	{	
		bool doResolve = true;
		//When we go into the pause menu dont resolve the backbuffer to keep the original unchanged
		//otherwise the backbuffer gets progressively more blurred by the dof msaa blend.
		if(DRAWLISTMGR->IsExecutingHudDrawList() || CLoadingScreens::AreActive())
		{
			doResolve = false;
		}

		backBuffer = CRenderTargets::GetBackBufferCopy(doResolve);
	}
	#endif	//DEVICE_MSAA
#elif __PPU
	grcRenderTarget *backBuffer = CRenderTargets::GetBackBuffer();
#elif __XENON
	grcRenderTarget *backBuffer = g_UsePacked7e3IntBackBuffer ? CRenderTargets::GetBackBuffer7e3toInt(false) : grcTextureFactory::GetInstance().GetBackBuffer();
#else
	#error "[PostFX] Missed a platform in back-buffer selection"
#endif

	// downsample to quarter-size screen
	const PostFXPass downSamplePass = (hasMotionBlur ? pp_DownSampleStencil : pp_passthrough);

	SetExposureParams();
	SetNonDepthFXStateBlocks();
	PostFXShader->SetVar(TextureID_0, backBuffer);
	PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/float(backBuffer->GetWidth()),1.0f/float(backBuffer->GetHeight()), 0.0, 0.0f));
#if __D3D11 || RSG_ORBIS
	PostFXShader->SetVar(StencilCopyTextureId, CRenderTargets::GetDepthBuffer_Stencil());
#endif
	PostFXBlit(HalfScreen0, PostFXTechnique, downSamplePass); 
	
#if RSG_PC
	// compute center 
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{
		CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed && pPlayerPed->GetPlayerInfo() && camInterface::GetDominantRenderedCamera())
		{
			CPlayerPedTargeting& rTargeting = pPlayerPed->GetPlayerInfo()->GetTargeting();
			const Vector3 aimDir = rTargeting.GetFreeAimTargetDir();

			const Vector3 aimDirT = camInterface::GetDominantRenderedCamera()->GetFrame().GetFront();
			(void)aimDirT;

			const PostFXPass centerDist = pp_centerdist;

			PostFXShader->SetVar(ResolvedTextureIdDepth, CRenderTargets::GetDepthResolved());

			PostFXShader->SetVar(globalFreeAimDirId, Vector4(/*aimDir*/aimDirT));
			PostFXBlit(CenterReticuleDist, PostFXTechnique, centerDist);
		}
	}
#endif

#if __XENON
	//set LDR exp bias for 7e3 format
	GRCDEVICE.SetColorExpBiasNow(CRenderer::GetBackbufferColorExpBiasLDR());
#endif

	// Compute Luminance
	ComputeLuminance();

	// Exposure
	SetExposureParams();
	SetNonDepthFXStateBlocks();

#if __BANK
	if (g_AutoExposureSkip || IsTiledScreenCaptureOrPanorama())
	{
		PostFXBlit(g_currentExposureRT, PostFXTechnique, pp_exposure_set);
	}
	else
#endif
	{
		if (settings.m_adaptationResetRequired)
		{
			PostFXBlit(g_currentExposureRT, PostFXTechnique, pp_exposure_reset);
		}
		else
		{
			PostFXBlit(g_currentExposureRT, PostFXTechnique, pp_exposure);
		}
	}

#if RSG_PC
	grcTextureD3D11* UpdateContentsTarget = (grcTextureD3D11*)CurExposureTex[CurExportRTIndex];
	UpdateContentsTarget->CopyFromGPUToStagingBuffer();
#endif // RSG_PC
	
	// process high DOF
	const Vector4 projParamsA = ShaderUtil::CalculateProjectionParams();
	PostFXShader->SetVar(DofProjId, projParamsA);

	grcRenderTarget* pDOFLargeBlur = NULL;
	grcRenderTarget* pDOFMedBlur = NULL;
	Vector4 hiDofMiscParams;
#if ADAPTIVE_DOF_GPU && ADAPTIVE_DOF_OUTPUT_UAV 
	AdaptiveDepthOfField.InitiliaseOnRenderThread();
#endif //ADAPTIVE_DOF_OUTPUT_UAV
	if (settings.m_HighDof || settings.m_AdaptiveDof || bExtraEffectsBlur)
	{
		PostFXShader->SetVar(TextureID_v1, g_prevExposureRT);
		hiDofMiscParams.x = settings.m_hiDofParameters.y;
		hiDofMiscParams.y = settings.m_hiDofParameters.z;
		ProcessHighDOFBuffers(settings, pDOFLargeBlur, pDOFMedBlur, g_currentExposureRT, screenBlurFadeLevel);
	}
	
	if (hasHeatHaze)
	{
		ProcessHeatHazeBuffers(settings);
	}

	grcViewport *vp = grcViewport::GetCurrent();

	if( true == settings.m_nightVision )
	{
		PostFXShader->SetVar(lowLumId, settings.m_lowLum);
		PostFXShader->SetVar(highLumId, settings.m_highLum);
		PostFXShader->SetVar(topLumId, settings.m_topLum);
								
		PostFXShader->SetVar(scalerLumId, settings.m_scalerLum);
								
		PostFXShader->SetVar(offsetLumId, settings.m_offsetLum);
		PostFXShader->SetVar(offsetLowLumId, settings.m_offsetLowLum);
		PostFXShader->SetVar(offsetHighLumId, settings.m_offsetHighLum);
								
		float noiseLum = (true == g_noiseOverride) ? g_noisinessOverride : settings.m_noiseLum;
		float noiseLowLum = (true == g_noiseOverride) ? g_noisinessOverride : settings.m_noiseLowLum;
		float noiseHighLum = (true == g_noiseOverride) ? g_noisinessOverride : settings.m_noiseHighLum;

		PostFXShader->SetVar(noiseLumId, noiseLum);
		PostFXShader->SetVar(noiseLowLumId, noiseLowLum);
		PostFXShader->SetVar(noiseHighLumId, noiseHighLum);
								
		PostFXShader->SetVar(bloomLumId, settings.m_bloomLum);

		PostFXShader->SetVar(colorLumId, settings.m_colorLum);
		PostFXShader->SetVar(colorLowLumId, settings.m_colorLowLum);
		PostFXShader->SetVar(colorHighLumId, settings.m_colorHighLum);
	}

	SetFilmicParams();

	PostFXShader->SetVar(TextureID_v1, g_currentExposureRT);

	SetBloomParams();
	PostFXShader->SetVar(ColorCorrectId, settings.m_colorCorrect);

#if FILM_EFFECT
	PostFXShader->SetVar(LensDistortionId, Vector4(g_BaseDistortionCoefficient, g_BaseCubeDistortionCoefficient, g_ChromaticAberrationOffset, g_ChromaticAberrationCubeOffset));
#endif

	PostFXShader->SetVar(ColorShiftId, settings.m_colorShift);
	PostFXShader->SetVar(DesaturateId, settings.m_deSaturate);
	PostFXShader->SetVar(GammaId, settings.m_gamma);
	PostFXShader->SetVar(sslrParamsId, Vector4(0.0f, 0.0f,0.0f, 0.0f)); // disable intensity buffer in final combines

	Vector4 noiseParams;
	GetNoiseParams(settings, noiseParams);

#if __BANK
	for (int i = 0; i < NUM_DEBUG_PARAMS; i++)
	{
		PostFXShader->SetVar(DebugParamsId[i], DebugParams[i]);
	}
#endif
	bool bBloomEnabled = true;
	#if __BANK
		bBloomEnabled = g_bloomEnable;
	#endif // __BANK
	#if RSG_PC
		bBloomEnabled &= (GET_POSTFX_SETTING(CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX) >= CSettings::Medium) ? true : false;
	#endif // RSG_PC

	if( settings.m_seeThrough )
	{
		PostFXShader->SetVar(GBufferTextureIdSSAODepth, SSAO::GetSSAOTexture());

	#if __D3D11 || RSG_ORBIS
		PostFXShader->SetVar(StencilCopyTextureId, CRenderTargets::GetDepthBuffer_Stencil());
	#endif // __XENON			

		CShaderLib::ForceScalarGlobals2(settings.m_SeeThroughDepthParams);

		SetSrcTextureAndSize(BloomTextureID, RenderPhaseSeeThrough::GetColorRT());
		SetNonDepthFXStateBlocks();
		PostFXBlit(RenderPhaseSeeThrough::GetBlurRT(), PostFXTechnique, pp_calcbloom_seethrough);

		SetSrcTextureAndSize(BloomTextureGID, RenderPhaseSeeThrough::GetBlurRT());
		SetNonDepthFXStateBlocks();
		PostFXBlit(RenderPhaseSeeThrough::GetColorRT(), PostFXTechnique, pp_calcbloom1);
	}
	else if (settings.m_lightRaysActive)
	{
		PostFXShader->SetVar(GBufferTextureIdSSAODepth, CRenderTargets::GetDepthBufferQuarterLinear());

		if (g_pNoiseTexture!=NULL) //lightrays now use noise
		{
			PostFXShader->SetVar(TextureID_1, g_pNoiseTexture);
			PostFXShader->SetVar(NoiseParamsId, noiseParams);
		}
		//new light ray control params
		SetSrcTextureAndSize(BloomTextureID, BloomBufferQuarter0);
	
		bool useUnderwater	= Water::IsCameraUnderwater();

#if __BANK
		useUnderwater=useUnderwater||g_ForceUnderWaterTech;
		useUnderwater=useUnderwater&&!g_ForceAboveWaterTech;
#endif //__BANK

		useUnderwater = useUnderwater && !CLoadingScreens::AreActive();

		Vector4 lightRayParams = settings.m_lightrayParams;
		if(useUnderwater)
		{
			const CLightSource &dirLight = Lights::GetRenderDirectionalLight();
			float brightness = Max(-dirLight.GetDirection().GetZ(), 0.0f);
			Vector4 underwaterParamsMask = Vector4(brightness, brightness, brightness, 1.0f);
			lightRayParams *= underwaterParamsMask;
		}
		PostFXShader->SetVar(lightrayParamsId, lightRayParams);

		PostFXShader->SetVar(sslrParamsId,  Vector4(settings.m_sslrParams.x, settings.m_sslrParams.z, 1.0f, vp->GetFarClip())); 
		BANK_ONLY(SetDebugParams();)

		grcRenderTarget *sslrRT = NULL;
		if(useUnderwater)
		{
			grcRenderTarget* mini = CRenderPhaseCascadeShadowsInterface::GetShadowMapMini();

			CRenderPhaseCascadeShadowsInterface::SetSharedShadowMap(mini);

			static dev_float timeScale=0.01f;
			PostFXShader->SetVar(lightrayParams2Id, Vector4(Water::GetWaterLevel() + 1.0f, TIME.GetElapsedTime()*timeScale, 0.0f, 0.0f));
			PostFXShader->SetVar(TextureID_v1, g_currentExposureRT);

			SetNonDepthFXStateBlocks();
			PostFXBlit(LRIntensity2, PostFXTechnique, pp_lightrays1);

			// need to blur the buffer to blend the dithered lightray samples
			SetSrcTextureAndSize(BloomTextureID, LRIntensity2);
			SetNonDepthFXStateBlocks();		
			PostFXBlit(LRIntensity, PostFXTechnique, pp_sslrblur); 
	
			// do the max blur filter to reduce haloing
			SetSrcTextureAndSize(BloomTextureID, LRIntensity);
			SetNonDepthFXStateBlocks();
			PostFXBlit(LRIntensity2, PostFXTechnique, pp_maxx);
			SetSrcTextureAndSize(BloomTextureID, LRIntensity2);
			PostFXBlit(LRIntensity, PostFXTechnique, pp_maxy);
			
			if (bBloomEnabled)
			{
				PrimeBloomBufferFrom(backBuffer);
			}
			sslrRT = LRIntensity;
		}
		else
		{
			PF_PUSH_MARKER("sslr");
			const Matrix34 &camMatrix = RCC_MATRIX34(vp->GetCameraMtx());

			Vector3 position = (settings.m_SunDirection * 10.0f + camMatrix.d);  // 10.0f should be adjustable
	
			rage::Vector3 acc = camMatrix.a * settings.m_sslrParams.y;
			rage::Vector3 up =  camMatrix.b * -settings.m_sslrParams.y;

			PostFXShader->SetVar(SSLRTextureId, SSLRCutout);
			PostFXShader->SetVar(sslrCenterId, position);
			PostFXShader->SetVar(BloomTextureID, LRIntensity); // borrow the intensity buffer during the cutout generation

			// render the clouds into a lowres buffer
			PF_PUSH_MARKER("clouds");
			grcTextureFactory::GetInstance().LockRenderTarget(0, LRIntensity, NULL);
			GRCDEVICE.Clear(true, Color32(255,255,255,255), false, 0.0f, false);
			
			// should we turn off z buffer here?
			int oldForceTechId = grmShader::GetForcedTechniqueGroupId();
			grmShader::SetForcedTechniqueGroupId(CloudDepthTechGroup);
			
			// clouds viewprojection adjust the world mtx to be camera relative, so we need to move the camera to the origin
			Mat34V oldCamMtx = vp->GetCameraMtx();
			Mat34V newCamMtx = oldCamMtx;
			newCamMtx.SetCol3(Vec3V(V_ZERO));
			vp->SetCameraMtx(newCamMtx);
			vp->SetWorldIdentity();

			CLOUDHATMGR->SetCameraPos(oldCamMtx.GetCol3());
			CLOUDHATMGR->Draw(CClouds::GetCurrentRenderCloudFrame(), false, 1.0f);  // tell CLOUDHATMGR we're not in reflection mode, so it does not replace the technique group.

			grmShader::SetForcedTechniqueGroupId(oldForceTechId);
			vp->SetCameraMtx(oldCamMtx);
			vp->SetWorldIdentity();

			Water::RenderWaterFLODDisc(Water::pass_flodblack, Water::GetCurrentDefaultWaterHeight());

			grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			PF_POP_MARKER();
	
			// render depth cutout (combined with the clouds)
			SetNonDepthFXStateBlocks();

		#if POSTFX_TILED_LIGHTING_ENABLED
			if (BANK_SWITCH(PostFX::g_UseTiledTechniques == true, CTiledLighting::IsEnabled()) 
#if RSG_PC
				&& !(GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
#endif
				)
			{
				const char* pPassName = NULL;
			#if RAGE_TIMEBARS
				pPassName = passName[pp_sslrcutout_tiled];
			#endif
#if __D3D11 || RSG_ORBIS
				PostFXShader->SetVar(TiledDepthTextureId, CTiledLighting::GetClassificationTexture());
#endif // __D3D11 || RSG_ORBIS
				CTiledLighting::RenderTiles(SSLRCutout, PostFXShader, PostFXTechnique, pp_sslrcutout_tiled, pPassName, true);
			}
			else
		#endif
			{
				PostFXBlit(SSLRCutout, PostFXTechnique, pp_sslrcutout, false, false);
			}


			// put the light rays into the intensity buffer
			SetNonDepthFXStateBlocks();
			PostFXBlit(LRIntensity2, PostFXTechnique, pp_sslrextruderays, false, true, Color32(0,0,0,0),
				position - acc - up, position + acc - up, position + acc + up, position - acc + up); 
			
			// need to blur the intensity buffer.
			// chaning to guassian blur to remove horizontal pattern on light ray (+0.03ms)
			PostFXShader->SetVar(TextureID_0, LRIntensity2);
			PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(LRIntensity2->GetWidth()),1.0f/float(LRIntensity2->GetHeight()),0.0, 0.0f));
			PostFXBlit(LRIntensity, PostFXTechnique, pp_GaussBlur_Hor);

			PostFXShader->SetVar(TextureID_0, LRIntensity);
			PostFXShader->SetVar(BloomTexelSizeId, Vector4(1.0f/float(LRIntensity->GetWidth()),1.0f/float(LRIntensity->GetHeight()),0.0, 0.0f));
			PostFXBlit(LRIntensity2, PostFXTechnique, pp_GaussBlur_Ver);
			
			if (bBloomEnabled)
			{
				PrimeBloomBufferFrom(backBuffer);
			}
			
			PF_POP_MARKER();

			sslrRT = LRIntensity2;
		}

		// Scale the light rays color to the value used in postfx.fx for Composite
		PostFXShader->SetVar(lightrayParamsId, lightRayParams * Vector4(4.0f, 4.0f, 4.0f, 1.0f));

		PostFXShader->SetVar(SSLRTextureId, sslrRT);
	}
	else
	{
		if (bBloomEnabled)
		{
			PostFXShader->SetVar(SSLRTextureId, LRIntensity);
			PrimeBloomBufferFrom(backBuffer);
		}
	}

	//PostFXShader->SetVar(SSLRTextureId, LRIntensity);

	grcRenderTarget *bloom = NULL;
	if( settings.m_seeThrough )
	{
		// blur the bloom buffer
		grcRenderTarget* SeeThruBloomBuffer = RenderPhaseSeeThrough::GetColorRT();
		BlurBloomBuffer(SeeThruBloomBuffer, true);
		bloom = SeeThruBloomBuffer;
	}
	else
	{		
		if (bBloomEnabled)
		{
			BlurBloomBuffer(BloomBuffer);
		}
		bloom = BloomBuffer;
	}

	Vector4 dofParams;
	dofParams.Set(settings.m_dofParameters);

	if (g_overrideDistanceBlur)
	{
		dofParams.SetZ(g_distanceBlurOverride);
		g_overrideDistanceBlur = false;
	}

#if __BANK //for screenshots
	if (NoDOF == true) 
	{
		// set blur strength to 0
		dofParams.z = 0.0f;
	}
#endif

	// Lens artefacts
	bool bUseLensArtefacts = LENSARTEFACTSMGR.IsActive();
	Vector4 lensArtefactsParams0, lensArtefactsParams1, lensArtefactsParams2;
	lensArtefactsParams0.Zero();
	lensArtefactsParams1.Zero();
	lensArtefactsParams2.Zero();

	if (bUseLensArtefacts && hasHeatHaze == false)
	{
		// Prime lens artefacts buffer
		LENSARTEFACTSMGR.Render();

		// Set values driven by timecycle
		LENSARTEFACTSMGR.SetExposureMinMaxFadeMultipliers(	settings.m_exposureMin, settings.m_exposureMax, 
															settings.m_lensArtefactsMinExposureMultiplier, settings.m_lensArtefactsMaxExposureMultiplier);

		// Get data for exposure based fade multiplier
		LENSARTEFACTSMGR.GetExposureBasedOpacityMultipliers(lensArtefactsParams0, lensArtefactsParams1);

		// Bind output and params for main composite pass
		lensArtefactsParams2.w = settings.m_lensArtefactsGlobalMultiplier;
		PostFXShader->SetVar(LensArtefactParams0ID, lensArtefactsParams0);
		PostFXShader->SetVar(LensArtefactParams1ID, lensArtefactsParams1);
		PostFXShader->SetVar(LensArtefactParams2ID, lensArtefactsParams2);
		PostFXShader->SetVar(HeatHazeTextureId, LENSARTEFACTSMGR.GetRenderTarget());
	}
	// Set black texture for lens artefacts and dummy params
	else
	{
		Vector4 lensArtefactsParams0 = Vector4(Vector4::ZeroType);
		Vector4 lensArtefactsParams1 = Vector4(Vector4::ZeroType);
		PostFXShader->SetVar(LensArtefactParams0ID, lensArtefactsParams0);
		PostFXShader->SetVar(LensArtefactParams1ID, lensArtefactsParams1);
		PostFXShader->SetVar(LensArtefactParams2ID, lensArtefactsParams2);
		PostFXShader->SetVar(HeatHazeTextureId, grcTexture::NoneBlack);
	}

	PostFXShader->SetVar(DofParamsId, dofParams);

	const Vector4 dofProj = ShaderUtil::CalculateProjectionParams();
	PostFXShader->SetVar(DofProjId, dofProj);

	const Vector2 dofShear = grcViewport::GetCurrent()->GetPerspectiveShear();
	PostFXShader->SetVar(DofShearId, Vector4(dofShear.x,dofShear.y,0.f,0.f) );

	SetSrcTextureAndSize(TextureID_0, backBuffer);
	bool bBloomCalculated = bBloomEnabled || settings.m_seeThrough;
	PostFXShader->SetVar(BloomTextureID, bBloomCalculated ? bloom : grcTexture::NoneBlack);

	PostFXShader->SetVar(TextureID_v0, LumRT);

	Vector4 hiDofParameters = settings.m_hiDofParameters;
	PackHiDofParameters(hiDofParameters);
	PostFXShader->SetVar(HiDofParamsId, hiDofParameters);

	SetFilmicParams();
	SetBloomParams();

#if USE_FXAA
	PostFXShader->SetVar(FXAABackBuffer, backBuffer);
#endif // USE_FXAA

#if DEVICE_MSAA
	PostFXShader->SetVar(TextureHDR_AA, CRenderTargets::GetBackBuffer());
#endif	//DEVICE_MSAA

	// If TXAA is on, that overrides FXAA and MLAA.
	const bool TXAAEnabled = USE_NV_TXAA TXAA_ONLY(&& g_TXAAEnable && IsTXAAAvailable()) && (g_DoPauseResolve != PostFX::IN_RESOLVE_PAUSE);
	const bool FXAAEnabled = !TXAAEnabled && g_FXAAEnable && USE_FXAA && (g_DoPauseResolve != PostFX::IN_RESOLVE_PAUSE);
	const bool MLAAEnabled = !TXAAEnabled && !FXAAEnabled && MLAA_ONLY(g_MLAA.IsEnabled() || ) 0 && USE_FXAA && (g_DoPauseResolve != PostFX::IN_RESOLVE_PAUSE);

	// This buffer must have the real fmask associated with it (we resolve MSAA/EQAA after the postfx composite pass and before the FXAA pass).
	grcRenderTarget *FXTarget	= CRenderTargets::GetUIBackBuffer();
	if(TXAAEnabled || FXAAEnabled || MLAAEnabled || GRCDEVICE.GetMSAA() || bUseExtraEffects) 
		FXTarget	= CRenderTargets::GetOffscreenBuffer1();

#if __PS3
	BANK_ONLY( if( g_delayFlipWait) )
	{
		GCM_DEBUG(cellGcmSetWaitFlip(GCM_CONTEXT));
		s_NeedWaitFlip = false;
	}
#endif // __PS3

#if __DEV
	// Either use command-line parameter or when LastGen is enabled
	if( gLastGenMode )
	{
		SSAO::Enable(settings.m_seeThrough);
	}

	bool applySimplePfx  = g_ForceUseSimple || 
		( gLastGenMode && !(settings.m_nightVision || settings.m_seeThrough) );

	CShaderLib::UpdateGlobalGameScreenSize();

	if( applySimplePfx BANK_ONLY( || g_JustExposure))
	{
#if __XENON
		//set LDR exp bias for 7e3 format
		GRCDEVICE.SetColorExpBiasNow(CRenderer::GetBackbufferColorExpBiasLDR());
#endif

		PostFXShader->SetVar(TextureID_v0, LumRT);
	
	#if __BANK
		if (g_JustExposure)
		{
			SetNonDepthFXStateBlocks();
			PostFXBlit(FXTarget, PostFXTechnique, pp_JustExposure);
		}
		else
	#endif
		{
			SetNonDepthFXStateBlocks();
			PostFXBlit(FXTarget, PostFXTechnique, pp_simple);
		}
	}
	else
#endif // __DEV	
	{
		// Extra effects passes always use noise, so we always need to update the noise parameters if we're running with any of these
		const bool hasNoise = (noiseParams.z>0.0f) || bUseExtraEffects;

		// We have no depth when paused: skip DOF
		const bool bSkipHighDofTechniques = XENON_SWITCH(CPauseMenu::GetPauseRenderPhasesStatus(), false);

		int techniqueIdx = 0;
		if( (settings.m_HighDof || settings.m_AdaptiveDof) && !bSkipHighDofTechniques ) 
		{			
			if (settings.m_ShallowDof && settings.m_HighDofForceSmallBlur == false) 
			{
				techniqueIdx |= BM_HighDofShallow;
			} 
			else 
			{
				techniqueIdx |= BM_HighDof;
			}
		}
		if( hasMotionBlur ) techniqueIdx |= BM_MotionBlur;
		if( settings.m_nightVision ) techniqueIdx |= BM_NightVision;
		if( hasHeatHaze ) techniqueIdx |= BM_HeatHaze;
		if( hasNoise )	techniqueIdx |= BM_Noise;

		if (bUseExtraEffects)
		{
			techniqueIdx |= BM_ExtraEffects;
		}

		// Vignetting
		const float vignettingBias = 1.0f-settings.m_vignettingParams.x;
		const float vignettingRadius = settings.m_vignettingParams.y;
		const float vignettingContrast = 1.0f + ((vignettingRadius-1.0f)*settings.m_vignettingParams.z);

		// pack some gradient filter terms in unused components
		const float ooMiddleColPos = 1.0f/settings.m_lensGradientColMiddle.w;
		const float ooOneMinusMiddleColPos = 1.0f/(1.0f-settings.m_lensGradientColMiddle.w);

		PostFXShader->SetVar(VignettingParamsId, Vector4(vignettingBias, vignettingRadius, vignettingContrast, ooOneMinusMiddleColPos));

		Vector4 vignettingColour(settings.m_vignettingColour);
		vignettingColour.w = ooMiddleColPos;
		PostFXShader->SetVar(VignettingColorId, vignettingColour);

		// Lens Gradient Filter

		Vec4V vColTop(RCC_VEC4V(settings.m_lensGradientColTop));
		Vec4V vColMiddle(RCC_VEC4V(settings.m_lensGradientColMiddle));
		Vec4V vColBottom(RCC_VEC4V(settings.m_lensGradientColBottom));
		Vec4V vGamma(settings.m_gamma, settings.m_gamma, settings.m_gamma, settings.m_gamma);

		Vec4V vColTopGamma		= Pow(vColTop, vGamma);
		Vec4V vColMiddleGamma	= Pow(vColMiddle, vGamma);
		Vec4V vColBottomGamma	= Pow(vColBottom, vGamma);

		vColTopGamma.SetW(SubtractScaled(ScalarV(V_ONE),vColTop.GetW(), ScalarV(V_TWO)));
		vColBottomGamma.SetW(SubtractScaled(ScalarV(V_ONE),vColBottom.GetW(), ScalarV(V_TWO)));
		vColMiddleGamma.SetW(vColMiddle.GetW());


		PostFXShader->SetVar(GradientFilterColTopId, vColTopGamma);
		PostFXShader->SetVar(GradientFilterColBottomId, vColBottomGamma);
		PostFXShader->SetVar(GradientFilterColMiddleId, vColMiddleGamma);

		// Distortion
		PostFXShader->SetVar(DistortionParamsID, BANK_ONLY(IsTiledScreenCaptureOrPanorama() ? Vector4(Vector4::ZeroType) :) settings.m_distortionParams);

		// Blur Vignetting
		const float blurVignettingBias = 1.0f-settings.m_blurVignettingParams.x;
		const float blurVignettingRadius = settings.m_blurVignettingParams.y;
		PostFXShader->SetVar(BlurVignettingParamsID, Vector4(blurVignettingBias, blurVignettingRadius, bUseExtraEffects ? 1.0f : 0.0f, 0.0f));


		if( settings.m_HighDof || settings.m_AdaptiveDof || bExtraEffectsBlur )
		{
			PostFXShader->SetVar(HiDofSmallBlurId, (settings.m_HighDofForceSmallBlur ? 1.0f : 0.0f));

			// dof samplers
			PostFXShader->SetVar(PostFxTexture2Id, pDOFMedBlur);
			PostFXShader->SetVar(PostFxTexture3Id, pDOFLargeBlur);

#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
			//With these DOF types we use the blurred target as the hdrsampler.
			//When we're using screen blur we need both the blurred and non blurred version so dont do this then.
			if( (CurrentDOFTechnique == dof_computeGaussian || CurrentDOFTechnique == dof_diffusion) )
			{
				PostFXShader->SetVar(TextureID_0, pDOFLargeBlur);
			}
#endif
		}
		else
		{
			PostFXShader->SetVar(PostFxTexture3Id, backBuffer);
		}

		PostFXPass pass = PassLookupTable[techniqueIdx];
		if( settings.m_seeThrough ) // force see through
		{
			pass = pp_composite_seethrough;

			if (g_pNoiseTexture != NULL)
			{
				PostFXShader->SetVar(TextureID_1, g_pNoiseTexture);
				PostFXShader->SetVar(NoiseParamsId, noiseParams);
			}

			PostFXShader->SetVar(seeThroughParamsId, settings.m_SeeThroughParams);
			PostFXShader->SetVar(seeThroughColorNearId, settings.m_SeeThroughColorNear);
			PostFXShader->SetVar(seeThroughColorFarId, settings.m_SeeThroughColorFar);
			PostFXShader->SetVar(seeThroughColorVisibleBaseId, settings.m_SeeThroughColorVisibleBase);
			PostFXShader->SetVar(seeThroughColorVisibleWarmId, settings.m_SeeThroughColorVisibleWarm);
			PostFXShader->SetVar(seeThroughColorVisibleHotId, settings.m_SeeThroughColorVisibleHot);
			PostFXShader->SetVar(PostFxTexture2Id, RenderPhaseSeeThrough::GetBlurRT());
		}

		// also used for distance blur
		const grcTexture* motionBlurTarget = HalfScreen0;
#if DOF_COMPUTE_GAUSSIAN || DOF_DIFFUSION
		if( CurrentDOFTechnique == dof_computeGaussian || CurrentDOFTechnique == dof_diffusion )
		{
			motionBlurTarget = backBuffer;
			if( settings.m_HighDof || settings.m_AdaptiveDof)
				motionBlurTarget = DepthOfField2;
		}
		else
		{
			motionBlurTarget = backBuffer;
		}
#endif
		PostFXShader->SetVar(PostFxMotionBlurTextureID, motionBlurTarget);


		if (hasMotionBlur)
		{
			PostFXShader->SetVar(JitterTextureId, JitterTexture);
#if __XENON			
			PostFXShader->SetVar(StencilCopyTextureId, CRenderTargets::GetDepthBufferAsColor());
#elif __D3D11 || RSG_ORBIS
			PostFXShader->SetVar(StencilCopyTextureId, CRenderTargets::GetDepthBuffer_Stencil());
#endif // __XENON			
		}

		if( settings.m_nightVision )
		{
			if (g_pNoiseTexture!=NULL)
			{
				PostFXShader->SetVar(TextureID_1, g_pNoiseTexture);
				PostFXShader->SetVar(NoiseParamsId, Vector4(NoiseEffectRandomValues.x, NoiseEffectRandomValues.y, 10.0f, 1.0f));
			}
		}
		
		if( hasHeatHaze )
		{
			const float t_time=settings.m_time / 1000.0f;

			Vector4 HeatHazeParams = settings.m_HeatHazeParams;
			Vector4 HeatHazeTex1Params = settings.m_HeatHazeTex1Params;
			Vector4 HeatHazeTex2Params = settings.m_HeatHazeTex2Params;
			Vector4 HeatHazeOffsetParams = settings.m_HeatHazeOffsetParams;
			
			const Matrix34 &camMatrix = RCC_MATRIX34(vp->GetCameraMtx());
			float cosAngA=camMatrix.a.x;
			const float cosAngC=camMatrix.c.z;

#if !__FINAL
			// Fix for an occasional assert that triggers due to a precision issues
			cosAngA = Clamp(cosAngA, -1.0f, 1.0f);
#endif

			const float AngA = Acosf(cosAngA)/(PI);
			const float HeatHazeTextureWidth = (float)HeatHazeTexture->GetWidth();
			const float HeatHazeTextureHeight = (float)HeatHazeTexture->GetHeight();

			HeatHazeTex1Params.z -= (AngA) * HeatHazeTex1Params.x;
			HeatHazeTex1Params.w += cosAngC * HeatHazeTex1Params.y;

			HeatHazeTex2Params.z -= (AngA) * HeatHazeTex2Params.x;
			HeatHazeTex2Params.w += cosAngC * HeatHazeTex2Params.y;
			
			HeatHazeTex1Params.z += Sinf(settings.m_HeatHazeTex1Anim.x + settings.m_HeatHazeTex1Anim.y * t_time) * (settings.m_HeatHazeTex1Anim.z/HeatHazeTextureWidth);
			HeatHazeTex1Params.z -= Floorf(HeatHazeTex1Params.z);
			HeatHazeTex1Params.w += (settings.m_HeatHazeTex1Anim.w/HeatHazeTextureHeight) * t_time;
			HeatHazeTex1Params.w -= Floorf(HeatHazeTex1Params.w);

			HeatHazeTex2Params.z += Sinf(settings.m_HeatHazeTex2Anim.x + settings.m_HeatHazeTex2Anim.y * t_time) * (settings.m_HeatHazeTex2Anim.z/HeatHazeTextureWidth);
			HeatHazeTex2Params.z -= Floorf(HeatHazeTex2Params.z);
			HeatHazeTex2Params.w += (settings.m_HeatHazeTex2Anim.w/HeatHazeTextureHeight) * t_time;
			HeatHazeTex2Params.w -= Floorf(HeatHazeTex2Params.w);

			HeatHazeOffsetParams.x *= 1.0f/ (float)backBuffer->GetWidth();
			HeatHazeOffsetParams.y *= 1.0f/ (float)backBuffer->GetHeight();
			
			float angleFade = 1.0f;
			bool bIsUnderWater = CVfxHelper::GetGameCamWaterDepth()>0.0f;

			// use angle fade only if not under water
			if (!bIsUnderWater)
			{
				angleFade = 1.0f-Clamp(fabs(camMatrix.c.z), 0.0f, 1.0f);
				angleFade*=angleFade;
			}

			HeatHazeOffsetParams.z = angleFade;

			PostFXShader->SetVar(heatHazeParamsId,HeatHazeParams);
			PostFXShader->SetVar(HeatHazeTex1ParamsId,HeatHazeTex1Params);
			PostFXShader->SetVar(HeatHazeTex2ParamsId,HeatHazeTex2Params);
			PostFXShader->SetVar(HeatHazeOffsetParamsId,HeatHazeOffsetParams);

			PostFXShader->SetVar(PostFxHHTextureID, backBuffer);
			PostFXShader->SetVar(HeatHazeTextureId, HeatHazeTexture);
			PostFXShader->SetVar(HeatHazeMaskTextureID, HeatHaze0);
		}

		if( hasNoise )
		{			
			if (g_pNoiseTexture!=NULL)
			{
				PostFXShader->SetVar(TextureID_1, g_pNoiseTexture);
				PostFXShader->SetVar(NoiseParamsId, noiseParams);
			}
		}

		if( settings.m_HighDof || settings.m_AdaptiveDof || bExtraEffectsBlur )
		{
#if BOKEH_SUPPORT	
			PostFXShader->SetVar( BokehEnableVar, BokehEnable);
			if( BokehEnable && BokehGenerated )
			{				
				PostFXShader->SetVar( BloomTextureGID, BokehPoints);
			}
			else
			{
				//If bokeh is disabled or no bokeh is drawn use the black 1x1			
				PostFXShader->SetVar( BloomTextureGID, BokehNoneRenderedTexture);
			}
#endif
		}

		PostFXShader->SetVar(TextureID_v1, g_currentExposureRT);

#if __BANK || __WIN32PC || RSG_DURANGO || RSG_ORBIS

#if FILM_EFFECT
		//if the fxaa is disabled, then we apply the film effect here
		if(FXAAEnabled == false && MLAAEnabled == false && (g_EnableFilmEffect || PARAM_filmeffect.Get()))
			ApplyFilmEffect(pass, NULL, settings);
		else
#endif //FILM_EFFECT
#endif
		{
			//=============== Main PostFX Composite Pass ======================================

			PostFXShader->SetVar(TextureID_v1, g_currentExposureRT);
			SetNonDepthFXStateBlocks();
#if __D3D11 || RSG_ORBIS
# if MSAA_EDGE_PASS
			if (DeferredLighting::IsEdgePassActiveForPostfx() &&
				GetPauseResolve() != NOT_RESOVE_PAUSE && GetPauseResolve() != UNRESOLVE_PAUSE &&
				PostFXShaderMS0)
			{
				grcTextureFactory::GetInstance().LockRenderTarget(0, FXTarget, NULL);
				grcStateBlock::SetDepthStencilState(DeferredLighting::m_directionalEdgeMaskEqualPass_DS, 0);
				PostFXShaderMS0->CopyPassData(PostFXTechnique, pass, PostFXShader);
				PostFXBlitHelper(PostFXShaderMS0, PostFXTechnique, pass);
				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			}else
			if (DeferredLighting::IsEdgePassActiveForPostfx() && AssertVerify(PostFXShaderMS0)
				BANK_ONLY(&& (DebugDeferred::m_OptimizeComposite & (1<<EM_IGNORE))))
			{
				PIX_AUTO_TAG(1, passName[pass]);
				grcTextureFactory::GetInstance().LockRenderTarget(0, FXTarget, 
#  if __D3D11
					GRCDEVICE.IsReadOnlyDepthAllowed() ? CRenderTargets::GetDepthBuffer_ReadOnly() :
#  endif
					CRenderTargets::GetDepthBufferCopy());
				// edges
				BANK_ONLY(if (DebugDeferred::m_OptimizeComposite & (1<<EM_EDGE0)))
				{
					grcStateBlock::SetDepthStencilState(DeferredLighting::m_directionalEdgeMaskEqualPass_DS, EDGE_FLAG);
					PostFXBlitHelper(PostFXShader, PostFXTechnique, pass);
				}
				// non-edges
				BANK_ONLY(if (DebugDeferred::m_OptimizeComposite & (1<<EM_FACE0)))
				{
					grcStateBlock::SetDepthStencilState(DeferredLighting::m_directionalEdgeMaskEqualPass_DS, 0);
					PostFXShaderMS0->CopyPassData(PostFXTechnique, pass, PostFXShader);
					PostFXBlitHelper(PostFXShaderMS0, PostFXTechnique, pass);
				}
				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				grcStateBlock::SetDepthStencilState(nonDepthFXDepthStencilState);
			}else
# endif //MSAA_EDGE_PASS
			{
				PostFXBlit(FXTarget, PostFXTechnique, pass);
			}
#else //__D3D11 || RSG_ORBIS
			// If we're using the default composite pass, use tile classification data to select where to apply distance blur
			// Performance was slightly worse on 360, so only do this on ps3
		#if __PS3 && POSTFX_TILED_LIGHTING_ENABLED			
			if (pass == pp_composite BANK_ONLY(&& g_PS3UseCompositeTiledTechniques && g_UseTiledTechniques) && CTiledLighting::IsEnabled())
			{
				const char* pPassName = NULL;
				pass = pp_composite_tiled;
			#if RAGE_TIMEBARS
				pPassName = passName[pass];
			#endif

				// distance blur ON
				dofParams.w = 0.0f;
				PostFXShader->SetVar(DofParamsId, dofParams);	
				CTiledLighting::RenderTiles(FXTarget, PostFXShader, PostFXTechnique, pass, pPassName, false, false, false);

				pass = pp_composite_tiled_noblur;
			#if RAGE_TIMEBARS
				pPassName = passName[pass];
			#endif
				// distance blur OFF
				dofParams.w = 1.0f;
				PostFXShader->SetVar(DofParamsId, dofParams);
				CTiledLighting::RenderTiles(FXTarget, PostFXShader, PostFXTechnique, pass, pPassName, false, false, false);
			}
			else if (pass == pp_composite_highdof BANK_ONLY(&& g_PS3UseCompositeTiledTechniques && g_UseTiledTechniques))
			{
				const char* pPassName = NULL;
				pass = pp_composite_highdof_blur_tiled;
			#if RAGE_TIMEBARS
				pPassName = passName[pass];
			#endif

				// hq dof ON
				hiDofMiscParams.w = 0.0f;
				PostFXShader->SetVar(HiDofMiscParamsId, hiDofMiscParams);	
				CTiledLighting::RenderTiles(FXTarget, PostFXShader, PostFXTechnique, pass, pPassName, false, false, false);

				pass = pp_composite_highdof_noblur_tiled;
			#if RAGE_TIMEBARS
				pPassName = passName[pass];
			#endif
				//  hq dof OFF
				hiDofMiscParams.w = 1.0f;
				PostFXShader->SetVar(HiDofMiscParamsId, hiDofMiscParams);
				CTiledLighting::RenderTiles(FXTarget, PostFXShader, PostFXTechnique, pass, pPassName, false, false, false);
			}
			else
		#endif
			{
				PostFXBlit(FXTarget, PostFXTechnique, pass, false, false, NULL, false);
			}
#endif
			//=================================================================================
		}

		PostFXGradientFilter(FXTarget, PostFXTechnique, pp_gradientfilter, settings);

		//============== Scanline Filter ===============

		// Extra effects passes always include scanline filter, but night vision doesn't.
		if(settings.m_scanlineParams.x > 0.01f && (bUseExtraEffects == false || settings.m_nightVision))
		{
			Vector4 scanlineParams;
			GetScanlineParams(settings, scanlineParams);

			PostFXShader->SetVar(ScanlineFilterParamsId, scanlineParams);
#if __D3D11 || RSG_ORBIS
			PostFXBlit(FXTarget, PostFXTechnique, pp_scanlinefilter);
#else
			PostFXBlit(FXTarget, PostFXTechnique, pp_scanlinefilter, false, false, NULL, false);
#endif // __D3D11 || RSG_ORBIS
		}
		//==============================================
	}

#if DYNAMIC_BB
	if(CPauseMenu::GetGPUCountdownToPauseStatus() && !DRAWLISTMGR->IsExecutingHudDrawList())
	{
		// copy the BackBuffer from ESRAM to DRAM ready for pause
		CRenderTargets::FlushAltBackBuffer();
		CPauseMenu::ClearGPUCountdownToPauseStatus();
	}
#endif

	grcRenderTarget *const finalRT = CRenderTargets::GetUIBackBuffer();
	grcRenderTarget* UIRT = finalRT;
#if DEVICE_MSAA && POSTFX_SEPARATE_LENS_DISTORTION_PASS
	const bool bSeparateLensDistortionPass = bUseExtraEffects;
	if (bSeparateLensDistortionPass)
	{
		// Using a spare RGBA8 single-sampled buffer.  Note that we don't care
		// about the contents, so make sure we call the _SingleSample version of
		// the function, not _Resolved.
#if RSG_DURANGO
		// use a one-off ESRAM temp buffer
		UIRT = CRenderTargets::GetOffscreenBuffer_SingleSample(3);
#else
		UIRT = CRenderTargets::GetOffscreenBuffer_SingleSample(2);
#endif
		Assert(UIRT);
	}
#endif // DEVICE_MSAA && POSTFX_SEPARATE_LENS_DISTORTION_PASS

	if(FXAAEnabled || MLAAEnabled)
	{
#if __PS3
#if __BANK
		if( g_delayFlipWait )
#endif // __BANK		
		{
			GCM_DEBUG(cellGcmSetWaitFlip(GCM_CONTEXT));
			s_NeedWaitFlip = false;
		}
#endif // __PS3

		//Resolve the Composite target
		Assert(FXTarget);
#if DEVICE_MSAA
# if RSG_ORBIS
		if (CRenderTargets::GetOffscreenBuffer1() == GBuffer::GetTarget(GBUFFER_RT_0))
		{
			// don't call FinishRendering() on GBUFFER0, causing another FMask decompression
			static_cast<grcRenderTargetMSAA*>(FXTarget)->Resolve(CRenderTargets::GetOffscreenBuffer1_Resolved());
		}else
# endif //RSG_ORBIS
		{
			CRenderTargets::ResolveOffscreenBuffer1();
		}
		FXTarget = CRenderTargets::GetOffscreenBuffer1_Resolved();
#else
		FXTarget = CRenderTargets::GetOffscreenBuffer1();
#endif //DEVICE_MSAA

#if FXAA_CUSTOM_TUNING
		PostFXShader->SetVar(fxaaBlurinessId, fxaaBluriness);
		PostFXShader->SetVar(fxaaConsoleEdgeSharpnessId, fxaaConsoleEdgeSharpness);
		PostFXShader->SetVar(fxaaConsoleEdgeThresholdId, fxaaConsoleEdgeThreshold);
		PostFXShader->SetVar(fxaaConsoleEdgeThresholdMinId, fxaaConsoleEdgeThresholdMin);

		PostFXShader->SetVar(fxaaQualitySubpixId, fxaaQualitySubpix);
		PostFXShader->SetVar(fxaaEdgeThresholdId, fxaaEdgeThreshold);
		PostFXShader->SetVar(fxaaEdgeThresholdMinId, fxaaEdgeThresholdMin);
#endif

#if USE_FXAA
		PostFXShader->SetVar(FXAABackBuffer, FXTarget);
		
		// rcpFrameId
		float rcpWidth = 1.0f / (float)VideoResManager::GetSceneWidth();
		float rcpHeight = 1.0f / (float)VideoResManager::GetSceneHeight();
		PostFXShader->SetVar(rcpFrameId, Vector2(rcpWidth, rcpHeight));
#endif	
		PostFXShader->SetVar(TextureID_v1, g_currentExposureRT);
		SetNonDepthFXStateBlocks();

		PostFXPass eAATechnique;
		switch ( g_AntiAliasingType )
		{
			case AA_FXAA_DEFAULT : eAATechnique = PS3_SWITCH( pp_AA_720p, FXAAPassToUse ); break; // PS3 uses a special hard-coded AA shader for 720p. See shader entry point for comments/justification.
			case AA_FXAA_UI : eAATechnique = pp_UIAA; break;
			default : eAATechnique = PS3_SWITCH( pp_AA_720p, FXAAPassToUse ); break;
		}

#if FILM_EFFECT
		if(g_EnableFilmEffect || PARAM_filmeffect.Get())
			ApplyFilmEffect(eAATechnique, NULL, settings);
		else
#endif //FILM_EFFECT
		{
#if ENABLE_MLAA
			if (MLAAEnabled)
			{
				g_MLAA.AntiAlias(UIRT,
#if RSG_ORBIS
					CRenderTargets::GetDepthResolved(),
#else
					CRenderTargets::GetDepthBufferResolved_ReadOnly(),
#endif
					FXTarget, CRenderTargets::GetDepthResolved()
					);
			}
			else
#endif // ENABLE_MLAA
			{
#if USE_HQ_ANTIALIASING
				if ( g_AntiAliasingType == AA_FXAA_HQ )
				{
#if __PS3 
					PF_PUSH_MARKER("MULTI-TYPE FXAA");
					Vector4 dummy;
					dummy.Zero();
					Vector4 range(g_AntiAliasingSwitchDistance, 0.f,0.f, 1.f);
				
					CSprite2d::GetShader()->SetVar(g_HQAA_srcMapID, FXTarget);

					CSprite2d::SetGeneralParams(range, dummy);					
					CTiledLighting::RenderTiles(UIRT, CSprite2d::GetShader(), g_HQAATiledTechnique, 0, "FXAA Sharp", false, false, false);

					range.w=0.f;     
					CSprite2d::SetGeneralParams(range, dummy);

					CTiledLighting::RenderTiles(UIRT, CSprite2d::GetShader(), g_HQAATiledTechnique, 1, "FXAA Soft", false, false, false);

					range.y=1.f;
					CSprite2d::SetGeneralParams(range, dummy);
					CTiledLighting::RenderTiles(UIRT, CSprite2d::GetShader(), g_HQAATiledTechnique, 2, "FXAA Blit", false, false, false);

					PF_POP_MARKER();
#else
					grcTextureFactory::GetInstance().LockRenderTarget(0, UIRT, NULL);

					CSprite2d hqaa;
					hqaa.BeginCustomList(CSprite2d::CS_HIGHQUALITY_AA, FXTarget);
						grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f,Color32(255, 255, 255, 255));
					hqaa.EndCustomList();

					grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif
				}
				else
#endif // USE_HQ_ANTIALIASING
				{
					PostFXShader->SetVar(FXAABackBuffer, FXTarget);
					PostFXBlit(UIRT, PostFXTechnique, eAATechnique, false, false, NULL, 
						#if __D3D11 
							NULL 
						#else
							false
						#endif
						);			
				}
			}
		}
	}					// if(FXAAEnabled || MLAAEnabled)
#if DEVICE_MSAA
	else if(GRCDEVICE.GetMSAA()) 
	{
		if (FXTarget != NULL)
		{
			grcRenderTargetMSAA* MSAATarget = static_cast<grcRenderTargetMSAA*>( FXTarget );
#if USE_NV_TXAA
			if (TXAAEnabled)
			{								
				grcRenderTarget* pTXAAOutRT = VideoResManager::IsSceneScaled() ? CRenderTargets::GetOffscreenBuffer_SingleSample(3) : UIRT;

				ResolveTXAA(pTXAAOutRT, MSAATarget);

				if(VideoResManager::IsSceneScaled())
				{
					PostFXShader->SetVar(TextureID_0, pTXAAOutRT);
					PostFXBlit(UIRT, PostFXTechnique, pp_copy, false, false, NULL,
#if __D3D11 
						NULL 
#else
						false
#endif
						);
				}
			}
			else // Plain old MSAA resolve.
#endif
			{
				// We still want to resolve here, even for 4s1f EQAA (this is the one resolve we do for EQAA)
#if RSG_PC
				if(VideoResManager::IsSceneScaled())
				{
					grcRenderTarget* pResolveRT = CRenderTargets::GetOffscreenBuffer_SingleSample(3);
					MSAATarget->Resolve(pResolveRT);

					PostFXShader->SetVar(TextureID_0, pResolveRT);
					PostFXBlit(UIRT, PostFXTechnique, pp_copy, false, false, NULL,
#if __D3D11 
						NULL 
#else
						false
#endif
						);
				}
				else
#endif
				MSAATarget->Resolve( UIRT );
			}
		}
		//FXTarget = NULL;
	}
#endif // DEVICE_MSAA
	else
	{
		UIRT = FXTarget;
	}

	// Depth buffer
	#if RSG_PC || RSG_DURANGO
		#if __D3D11
			grcRenderTarget *depthRT = (GRCDEVICE.IsReadOnlyDepthAllowed()) ? CRenderTargets::GetDepthBuffer_ReadOnly() : CRenderTargets::GetDepthBufferCopy();
		#else
			grcRenderTarget *depthRT = CRenderTargets::GetDepthBufferCopy();
		#endif
	#else
		grcRenderTarget *depthRT = CRenderTargets::GetDepthBuffer();
	#endif

	#if __D3D11 || RSG_ORBIS
		grcRenderTarget *stencilRT = CRenderTargets::GetDepthBuffer_Stencil();
	#endif

	if (settings.m_fpvMotionBlurActive)
	{
		BANK_ONLY(SetDebugParams();)

		// Copy to 0
		SetSrcTextureAndSize(StencilCopyTextureId, stencilRT);
		SetSrcTextureAndSize(TextureID_0, UIRT);
		SetSrcTextureAndSize(TextureID_v0, UIRT);	
		SetSrcTextureAndSize(GBufferTextureIdDepth, depthRT);
		PostFXBlit(fpvMotionBlurTarget[0], PostFXTechnique, pp_motion_blur_fpv, false, false);

		// Composite
		PostFXShader->SetVar(JitterTextureId, JitterTexture);
		PostFXShader->SetVar(fpvMotionBlurWeightsId, settings.m_fpvMotionBlurWeights);
		PostFXShader->SetVar(fpvMotionBlurVelocityId, settings.m_fpvMotionBlurVelocity);
		PostFXShader->SetVar(fpvMotionBlurSizeId, settings.m_fpvMotionBlurSize);
		SetSrcTextureAndSize(TextureID_0, fpvMotionBlurTarget[0]);
		PostFXBlit(fpvMotionBlurTarget[1], PostFXTechnique, pp_motion_blur_fpv_ds, false, false);

		// Final composite
		Vec4V weights = Vec4V(Mag(settings.m_fpvMotionBlurVelocity).Getf(), 0.0f, 0.0f, 0.0f);
		PostFXShader->SetVar(fpvMotionBlurWeightsId, weights);

		PostFXShader->SetVar(JitterTextureId, JitterTexture);
		PostFXShader->SetVar(fpvMotionBlurVelocityId, settings.m_fpvMotionBlurVelocity);
		PostFXShader->SetVar(fpvMotionBlurSizeId, settings.m_fpvMotionBlurSize);
		SetSrcTextureAndSize(TextureID_0, fpvMotionBlurTarget[1]);
		SetSrcTextureAndSize(TextureID_v0, fpvMotionBlurTarget[1]);
		PostFXBlit(UIRT, PostFXTechnique, pp_motion_blur_fpv_composite, false, false);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	}

#if DEVICE_MSAA && POSTFX_SEPARATE_LENS_DISTORTION_PASS
	if (bSeparateLensDistortionPass)
	{
#if RSG_PC
		const float currentAspect = ((float)finalRT->GetWidth() / (float)finalRT->GetHeight());
		const float maxAspect = 1.0f / 5.0f;
		const float scaleCoef = Clamp(1.0f - Saturate((currentAspect * maxAspect) - (2.0f * maxAspect)), 0.5f, 1.0f);
#else
		const float scaleCoef = 1.0f;
#endif

		PostFXShader->SetVar(TextureID_0, UIRT);
		PostFXShader->SetVar(TexelSizeId, Vector4(1.0f/finalRT->GetWidth(), 1.0f/finalRT->GetHeight(), scaleCoef, 0.f));
		PostFXBlit(finalRT, PostFXTechnique, pp_lens_distortion, false, false);
	}

	const bool bNeedsBlurVignetting = (settings.m_blurVignettingParams.x > 0.0f);
	if (bNeedsBlurVignetting)
	{
		ProcessBlurVignetting(finalRT, settings);
	}
#endif // DEVICE_MSAA && POSTFX_SEPARATE_LENS_DISTORTION_PASS
		
#if __BANK
	RenderDofDebug(pDOFLargeBlur, settings);
#endif

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	//============================ Copy to the Replay thumbnail buffer ========================
	CreateReplayThumbnail();
	//=========================================================================================
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif // GTA_REPLAY

	bool bDrawDamageOverlay = !settings.m_drawDamageOverlayAfterHud && (settings.m_damageEnabled[0] || settings.m_damageEnabled[1] || settings.m_damageEnabled[2] || settings.m_damageEnabled[3]);
	if (bDrawDamageOverlay)
	{
		PostFXDamageOverlay(finalRT, PostFXTechnique, pp_DamageOverlay, settings);
	}

	// Draw preview border
	if (CLiveManager::IsSystemUiShowing() == false && CPauseMenu::IsActive() == false && CPhoneMgr::CamGetState() && CPhotoManager::IsProcessingScreenshot() == false)
	{
		grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetUIBackBuffer(), NULL);
			PHONEPHOTOEDITOR.RenderBorder(true);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}

	//============================ Copy to the screenshot buffer ==============================
	CPhotoManager::Execute(CRenderTargets::GetUIBackBuffer());
	PHONEPHOTOEDITOR.RenderGalleryEdit();
	//=========================================================================================

#if __XENON
	// set the pixel shader threads to 112 -> vertex shader can only use 16 threads
	GRCGPRALLOCATION->SetGPRAllocation(0);
#endif
	
	grcStateBlock::SetRasterizerState(exitNonDepthFXRasterizerState);
	grcStateBlock::SetBlendState(exitNonDepthFXBlendState);
	grcStateBlock::SetDepthStencilState(exitNonDepthFXDepthStencilState);
	CShaderLib::ResetScalarGlobals2();
}


grcTexture* GetValidBackBufferForSampling()
{
	// Only need to copy on PC, the consoles will allow you to do Read/Modify/Write even if it's not guaranteed to be safe... unless
	// we have multi-fragment MSAA, in which case the copy/resolve is necessary on the consoles too.
	const bool bDoBackBufferCopy = RSG_PC ? true : ( MSAA_ONLY( GRCDEVICE.GetMSAA() > 1 ? true : ) false );

	grcTexture* backBuffer;

	if ( bDoBackBufferCopy )
	{
		ORBIS_ONLY( CRenderTargets::UnlockSceneRenderTargets() );
	}

	if ( bDoBackBufferCopy )
	{
		backBuffer = CRenderTargets::GetBackBufferCopy(true);
	}
	else
	{
		backBuffer = CRenderTargets::GetBackBuffer();
	}

	if ( bDoBackBufferCopy )
	{
		ORBIS_ONLY( CRenderTargets::LockSceneRenderTargets() );
	}

	return backBuffer;
}


void PostFX::DownSampleBackBuffer(int numDownSamples)
{
	//Calling function should make sure they lock/unlock the render targets
#if !RSG_DURANGO
	grcRenderTarget* DSBB2  = BloomBufferHalf0;
	grcRenderTarget* DSBB4  = BloomBufferQuarter0;
	grcRenderTarget* DSBB8  = BloomBufferEighth0;
	grcRenderTarget* DSBB16 = BloomBufferSixteenth0;
#endif

	if (numDownSamples < 1) return;

	grcTexture* BackBuffer = GetValidBackBufferForSampling();	// don't call this more than once, on PC it will copy and resolve any MSAA each time
	if (DSBB2 == NULL || BackBuffer == NULL)
	{
		Assertf(0, "DownSampleBackBuffer: Null backbuffer");
		return;
	}

	PostFXShader->SetVar(TextureID_v1, grcTexture::NoneWhite);

	SimpleBlitWrapper(DSBB2, BackBuffer, pp_passthrough);

	if (DSBB4 && numDownSamples > 1)
	{
		SimpleBlitWrapper(DSBB4, DSBB2, pp_passthrough);
	}

	if (DSBB8 && numDownSamples > 2)
	{
		SimpleBlitWrapper(DSBB8, DSBB4, pp_passthrough);
	}

	if (DSBB16 && numDownSamples > 3)
	{
		SimpleBlitWrapper(DSBB16, DSBB8, pp_passthrough);
	}
}

grcTexture* PostFX::GetDownSampledBackBuffer(int numDownSamples)
{
#if !RSG_DURANGO
	grcRenderTarget* DSBB2  = BloomBufferHalf0;
	grcRenderTarget* DSBB4  = BloomBufferQuarter0;
	grcRenderTarget* DSBB8  = BloomBufferEighth0;
	grcRenderTarget* DSBB16 = BloomBufferSixteenth0;
#endif

	if (numDownSamples == 0)
		return GetValidBackBufferForSampling();
	if (numDownSamples == 1)
		return DSBB2;
	if (numDownSamples == 2)
		return DSBB4;
	if (numDownSamples == 3)
		return DSBB8;
	if (numDownSamples == 4)
		return DSBB16;

	return NULL;
}

#if FILM_EFFECT
void PostFX::ApplyFilmEffect(int normalPass, grcRenderTarget* output, const PostFX::PostFXParamBlock::paramBlock& settings)
{
#if __BANK
	if (IsTiledScreenCaptureOrPanorama())
	{
		return;
	}
#endif // __BANK

	//apply the normal pass into a special film effect rendertarget
	SetNonDepthFXStateBlocks();
	PostFXBlit(PreFilmEffect, PostFXTechnique, (PostFXPass)normalPass);

	//render the film effect
	PostFXShader->SetVar(TextureID_0, PreFilmEffect);
	if(g_FilmNoise > 0.0f)
	{
		PostFXShader->SetVar(TextureID_1, g_pNoiseTexture);
		PostFXShader->SetVar(NoiseParamsId, Vector4(NoiseEffectRandomValues.x, NoiseEffectRandomValues.y, g_FilmNoise, 1.0f));
	}

	if(g_FilmVignettingIntensity > 0.0f)
	{
		const float vignettingBias = 1.0f-g_FilmVignettingIntensity;
		const float vignettingRadius = g_FilmVignettingRadius;
		const float vignettingContrast = 1.0f + ((vignettingRadius-1.0f)*g_FilmVignettingContrast);
		PostFXShader->SetVar(VignettingParamsId, Vector4(vignettingBias, vignettingRadius, sqrt(vignettingContrast), 0.0f));
		PostFXShader->SetVar(VignettingColorId, settings.m_vignettingColour);
	}

	//apply the film effects to the output
	SetNonDepthFXStateBlocks();
	PostFXBlit(output, PostFXFilmEffectTechnique, pp_filmeffect);
}
#endif

#if USE_SCREEN_WATERMARK

bool PostFX::IsWatermarkEnabled()
{
	if (PARAM_no_screen_watermark.Get())
		return false;

	bool waterMarkEnabled;

	// draw watermark is there's an ongoing network session or we're in one of the MP menus
	eFRONTEND_MENU_VERSION menuVersion =  CPauseMenu::GetCurrentMenuVersion();
	const bool bMenuNeedsWatermark = (menuVersion == FE_MENU_VERSION_MP_CHARACTER_CREATION) ||  (menuVersion == FE_MENU_VERSION_MP_CHARACTER_SELECT);
	waterMarkEnabled = NetworkInterface::IsNetworkOpen() || CScriptHud::bUsingMissionCreator || bMenuNeedsWatermark BANK_ONLY(|| ms_watermarkParams.bForceWatermark);

#if WATERMARKED_BUILD
	waterMarkEnabled = true;
#endif

	return waterMarkEnabled;
}

void PostFX::UpdateScreenWatermark()
{

	ms_watermarkParams.bEnabled = IsWatermarkEnabled();

	if (ms_watermarkParams.bEnabled == false)
	{
		return;
	}

	float dayNightBalance = Lights::CalculateTimeFade(18.0f, 18.5f, 6.0f, 6.5f);
	float curAlpha = Lerp(dayNightBalance, ms_watermarkParams.alphaDay, ms_watermarkParams.alphaNight);
	ms_watermarkParams.textColor.SetAlpha((u8)(curAlpha*255.0f));


	// get the gamer tag string
	CPed* pPed = CGameWorld::FindLocalPlayer();
	const char* pText = "INVALID";

	if (pPed != NULL 
		&& pPed->GetPlayerInfo() != NULL
		&& pPed->GetPlayerInfo()->m_GamerInfo.IsValid()
		&& pPed->GetPlayerInfo()->m_GamerInfo.GetName() != NULL)
	{
		pText = pPed->GetPlayerInfo()->m_GamerInfo.GetName();
	}

	sprintf(&(ms_watermarkParams.text[0]), "%s", pText);

}

void PostFX::RenderScreenWatermark()
{
	if (ms_watermarkParams.bEnabled == false)
	{
		return;
	}

	// setup the text layout/format
	Color32 textCol = ms_watermarkParams.textColor;
	Vector2 textPos = ms_watermarkParams.textPos;
	Vector2 textSize = Vector2(ms_watermarkParams.textSize, ms_watermarkParams.textSize);
	textPos = CHudTools::CalculateHudPosition(textPos,textSize,'L','B');
	bool useOutline = ms_watermarkParams.useOutline;

#if __BANK
	const char* pStr = ms_watermarkParams.bUseDebugText ?  &(ms_watermarkParams.debugText[0]) : &(ms_watermarkParams.text[0]);
#else
	const char* pStr = &(ms_watermarkParams.text[0]);
#endif

	if (strlen(pStr) > 12)
	{
		textSize *= 0.84f;
	}

	CTextLayout watermarkStringLayout;
	watermarkStringLayout.SetOrientation(FONT_LEFT);
	watermarkStringLayout.SetStyle(FONT_STYLE_CONDENSED);
	watermarkStringLayout.SetColor(textCol);
	watermarkStringLayout.SetScale(&textSize);
	watermarkStringLayout.SetEdge(useOutline);
	watermarkStringLayout.SetEdgeCutout(useOutline);

	char watermarkString[RL_MAX_NAME_BUF_SIZE+1]={'\0'};
	
	safecpy(watermarkString, pStr);

	float charHeight = CTextFormat::GetCharacterHeight(watermarkStringLayout);
	textPos.y -= charHeight*0.5f;

	CRenderTargets::LockUIRenderTargets();

	// override Scaleform blend state, we don't want to write alpha
	sfScaleformManager* pScaleformMgr = CScaleformMgr::GetMovieMgr();
	sfRendererBase& scRenderer = pScaleformMgr->GetRageRenderer();
	scRenderer.OverrideBlendState(true);

	grcBlendStateHandle prevBlendState = grcStateBlock::BS_Active;
	grcStateBlock::SetBlendState(ms_watermarkBlendState);

	// render watermark text
	watermarkStringLayout.Render(&textPos, watermarkString);

	//if we are using an outline, the first time we render the text we just render the outline
	//with the main part of the text cut out. Then we render the text normal without the
	//outline, otherwise it can look darker with the edge render along with the normal text
	if(useOutline)
	{
		watermarkStringLayout.SetEdge(false);
		watermarkStringLayout.SetEdgeCutout(false);

		watermarkStringLayout.Render(&textPos, watermarkString);
	}

#if DISPLAY_NETWORK_INFO
	netIpAddress ip;

	netHardware::GetPublicIpAddress(&ip);
	char ipPubStr[netIpAddress::MAX_STRING_BUF_SIZE];
	ip.Format(ipPubStr);

	netHardware::GetLocalIpAddress(&ip);
	char ipLocalStr[netIpAddress::MAX_STRING_BUF_SIZE];
	ip.Format(ipLocalStr);

	u8 macAddr[6];
	netHardware::GetMacAddress(macAddr);

	char netAddressString[255];

	sprintf_s(netAddressString, "LIP %s EIP %s MAC %X:%X:%X:%X:%X:%X", ipLocalStr, ipPubStr, macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

	Color32 netTextCol = ms_watermarkParams.netTextColor;
	netTextCol.SetAlpha(textCol.GetAlpha());
	Vector2 netTextPos = ms_watermarkParams.netTextPos;
	Vector2 netTextSize = Vector2(ms_watermarkParams.netTextSize, ms_watermarkParams.netTextSize);

	CTextLayout netAddressStringLayout = watermarkStringLayout;

	netAddressStringLayout.SetColor(netTextCol);
	netAddressStringLayout.SetScale(netTextSize);

	charHeight = CTextFormat::GetCharacterHeight(netAddressStringLayout);
	textPos.y -= charHeight*0.5f;

	netAddressStringLayout.SetEdge(useOutline);
	netAddressStringLayout.SetEdgeCutout(useOutline);

	netAddressStringLayout.Render(&netTextPos, netAddressString);

	//if we are using an outline, the first time we render the text we just render the outline
	//with the main part of the text cut out. Then we render the text normal without the
	//outline, otherwise it can look darker with the edge render along with the normal text
	if(useOutline)
	{
		netAddressStringLayout.SetEdge(false);
		netAddressStringLayout.SetEdgeCutout(false);

		netAddressStringLayout.Render(&netTextPos, netAddressString);
	}
#endif

	CText::Flush();

	// restore blend state
	grcStateBlock::SetBlendState(prevBlendState);
	scRenderer.OverrideBlendState(false);

	CRenderTargets::UnlockUIRenderTargets();

#if USE_IMAGE_WATERMARKS
	grcTexture *WatermarkTexture = g_TxdStore.Get(g_TxdSlot)->Lookup("bokeh_pent");
	grcTexture *HiddenWatermarkTexture = g_TxdStore.Get(g_TxdSlot)->Lookup("bokeh_deca");

	Vec2f watermarkPos = GetWatermarkPostion();

	float alpha = ms_watermarkParams.textColor.GetAlpha() / 256.0f;

	RenderWatermarkImage(WatermarkTexture, watermarkPos, true, alpha);

	RenderWatermarkImage(HiddenWatermarkTexture, Vec2f(0.25f, 0.0f), false, 1.0f);
	RenderWatermarkImage(HiddenWatermarkTexture, Vec2f(0.75f, 0.0f), false, 1.0f);
#endif

}
#endif // USE_SCREEN_WATERMARK

#if USE_IMAGE_WATERMARKS
void PostFX::RenderWatermarkImage(const grcTexture* watermark, const Vec2f& pos, bool scale, float alpha)
{
	Assertf(watermark, "Error, null watermark texture");

	if(watermark == NULL)
		return;

	float screenWidth = scale ? 1280.0f : (float)GRCDEVICE.GetWidth();
	float screenHeight =  scale ? 720.0f : (float)GRCDEVICE.GetHeight();
	float textureWidth = (float)watermark->GetWidth();
	float textureHeight = (float)watermark->GetHeight();

	float scaleX = textureWidth * (1.0f / screenWidth); 
	float scaleY = textureHeight * (1.0f / screenHeight); 

	AssertVerify(PostFXShader->BeginDraw(grmShader::RMC_DRAW, false, PostFXTechnique));
#if POSTFX_UNIT_QUAD
	PostFXShader->SetVar(quad::Position,	FastQuad::Pack(-1.0f + pos.GetX() * 2, 
															   1.0f - pos.GetY() * 2, 
															   1.0f + pos.GetX() * 2 ,
															   -1.0f - pos.GetY() * 2));
	PostFXShader->SetVar(quad::TexCoords, FastQuad::Pack(0.0f, 0.0f, 1.0f, 1.0f));
	PostFXShader->SetVar(quad::Scale, Vector4(scaleX, scaleY, 1.0f, 1.0f));
	PostFXShader->SetVar(quad::Alpha, alpha);
#endif
	PostFXShader->SetVar(TextureID_0, watermark);

	PostFXShader->Bind((int)pp_passthrough);
	grcStateBlock::SetBlendState(imageWatermarkBlendState);

#if POSTFX_UNIT_QUAD
	FastQuad::Draw(false);
#endif

	PostFXShader->UnBind();
	PostFXShader->EndDraw();

	grcStateBlock::SetBlendState(nonDepthFXBlendState);
}
#endif // USE_IMAGE_WATERMARKS

void PostFX::UpdateVisualDataSettings()
{
	if (g_visualSettings.GetIsLoaded())
	{
		g_filmicTonemapParams[TONEMAP_FILMIC_A_BRIGHT] = g_visualSettings.Get("Tonemapping.bright.filmic.A", 0.22f);
		g_filmicTonemapParams[TONEMAP_FILMIC_B_BRIGHT] = g_visualSettings.Get("Tonemapping.bright.filmic.B", 0.3f);
		g_filmicTonemapParams[TONEMAP_FILMIC_C_BRIGHT] = g_visualSettings.Get("Tonemapping.bright.filmic.C", 0.10f);
		g_filmicTonemapParams[TONEMAP_FILMIC_D_BRIGHT] = g_visualSettings.Get("Tonemapping.bright.filmic.D", 0.20f);
		g_filmicTonemapParams[TONEMAP_FILMIC_E_BRIGHT] = g_visualSettings.Get("Tonemapping.bright.filmic.E", 0.01f);
		g_filmicTonemapParams[TONEMAP_FILMIC_F_BRIGHT] = g_visualSettings.Get("Tonemapping.bright.filmic.F", 0.30f);
		g_filmicTonemapParams[TONEMAP_FILMIC_W_BRIGHT] = g_visualSettings.Get("Tonemapping.bright.filmic.W", 4.00f);
		g_filmicTonemapParams[TONEMAP_FILMIC_EXPOSURE_BRIGHT] = g_visualSettings.Get("Tonemapping.bright.filmic.exposure", -2.0f);

		g_filmicTonemapParams[TONEMAP_FILMIC_A_DARK] = g_visualSettings.Get("Tonemapping.dark.filmic.A", 0.22f);
		g_filmicTonemapParams[TONEMAP_FILMIC_B_DARK] = g_visualSettings.Get("Tonemapping.dark.filmic.B", 0.3f);
		g_filmicTonemapParams[TONEMAP_FILMIC_C_DARK] = g_visualSettings.Get("Tonemapping.dark.filmic.C", 0.10f);
		g_filmicTonemapParams[TONEMAP_FILMIC_D_DARK] = g_visualSettings.Get("Tonemapping.dark.filmic.D", 0.20f);
		g_filmicTonemapParams[TONEMAP_FILMIC_E_DARK] = g_visualSettings.Get("Tonemapping.dark.filmic.E", 0.01f);
		g_filmicTonemapParams[TONEMAP_FILMIC_F_DARK] = g_visualSettings.Get("Tonemapping.dark.filmic.F", 0.30f);
		g_filmicTonemapParams[TONEMAP_FILMIC_W_DARK] = g_visualSettings.Get("Tonemapping.dark.filmic.W", 4.00f);
		g_filmicTonemapParams[TONEMAP_FILMIC_EXPOSURE_DARK] = g_visualSettings.Get("Tonemapping.dark.filmic.exposure", 1.0f);
		
		float sniperSightEnabled		= g_visualSettings.Get("misc.SniperRifleDof.Enabled", 0.0f);
		g_sniperSightDefaultEnabled		= sniperSightEnabled > 0.0f;
		g_sniperSightDefaultDOFParams.x	= g_visualSettings.Get("misc.SniperRifleDof.NearStart", 0.0f);
		g_sniperSightDefaultDOFParams.y	= g_visualSettings.Get("misc.SniperRifleDof.NearEnd", 0.0f);
		g_sniperSightDefaultDOFParams.z	= g_visualSettings.Get("misc.SniperRifleDof.FarStart", 0.0f);
		g_sniperSightDefaultDOFParams.w	= g_visualSettings.Get("misc.SniperRifleDof.FarEnd", 2180.0f);

		PostFXParamBlock::UpdateVisualSettings();
		g_sunExposureAdjust = g_visualSettings.Get("Adaptation.sun.exposure.tweak", 0.0f);

		g_bloomThresholdMin = g_visualSettings.Get("bloom.threshold.min", 0.3f);
		g_bloomThresholdPower = g_visualSettings.Get("bloom.threshold.power", 1.0f);
		g_bloomThresholdExposureDiffMin = g_visualSettings.Get("bloom.threshold.expsoure.diff.min", 0.1f);
		g_bloomThresholdExposureDiffMax = g_visualSettings.Get("bloom.threshold.expsoure.diff.max", 1.0f);
		
		g_bloomLargeBlurBlendMult = g_visualSettings.Get("bloom.blurblendmult.large", 0.5f);
		g_bloomMedBlurBlendMult = g_visualSettings.Get("bloom.blurblendmult.med", 0.5f);
		g_bloomSmallBlurBlendMult = g_visualSettings.Get("bloom.blurblendmult.small", 1.0f);

		g_bulletImpactOverlay.ms_screenSafeZoneLength = g_visualSettings.Get("misc.DamageOverlay.ScreenSafeZoneLength", 1.0f);

		g_bulletImpactOverlay.ms_settings[0].rampUpDuration = (u32)g_visualSettings.Get("misc.DamageOverlay.RampUpDuration", 50);
		g_bulletImpactOverlay.ms_settings[0].holdDuration = (u32)g_visualSettings.Get("misc.DamageOverlay.HoldDuration", 400);
		g_bulletImpactOverlay.ms_settings[0].rampDownDuration = (u32)g_visualSettings.Get("misc.DamageOverlay.RampDownDuration", 100);
		g_bulletImpactOverlay.ms_settings[0].colourBottom.x = g_visualSettings.Get("misc.DamageOverlay.ColorBottom.red", 0.54f);
		g_bulletImpactOverlay.ms_settings[0].colourBottom.y = g_visualSettings.Get("misc.DamageOverlay.ColorBottom.green", 0.06f);
		g_bulletImpactOverlay.ms_settings[0].colourBottom.z = g_visualSettings.Get("misc.DamageOverlay.ColorBottom.blue", 0.00f);
		g_bulletImpactOverlay.ms_settings[0].colourTop.x = g_visualSettings.Get("misc.DamageOverlay.ColorTop.red", 0.54f);
		g_bulletImpactOverlay.ms_settings[0].colourTop.y = g_visualSettings.Get("misc.DamageOverlay.ColorTop.green", 0.06f);
		g_bulletImpactOverlay.ms_settings[0].colourTop.z = g_visualSettings.Get("misc.DamageOverlay.ColorTop.blue", 0.00f);
		g_bulletImpactOverlay.ms_settings[0].colourBottomEndurance.x = g_visualSettings.Get("misc.DamageOverlay.colourBottomEndurance.red", 0.99f);
		g_bulletImpactOverlay.ms_settings[0].colourBottomEndurance.y = g_visualSettings.Get("misc.DamageOverlay.colourBottomEndurance.green", 0.59f);
		g_bulletImpactOverlay.ms_settings[0].colourBottomEndurance.z = g_visualSettings.Get("misc.DamageOverlay.colourBottomEndurance.blue", 0.32f);
		g_bulletImpactOverlay.ms_settings[0].colourTopEndurance.x = g_visualSettings.Get("misc.DamageOverlay.colourTopEndurance.red", 0.99f);
		g_bulletImpactOverlay.ms_settings[0].colourTopEndurance.y = g_visualSettings.Get("misc.DamageOverlay.colourTopEndurance.green", 0.59f);
		g_bulletImpactOverlay.ms_settings[0].colourTopEndurance.z = g_visualSettings.Get("misc.DamageOverlay.colourTopEndurance.blue", 0.32f);
		g_bulletImpactOverlay.ms_settings[0].globalAlphaBottom = g_visualSettings.Get("misc.DamageOverlay.GlobalAlphaBottom", 1.0f);
		g_bulletImpactOverlay.ms_settings[0].globalAlphaTop = g_visualSettings.Get("misc.DamageOverlay.GlobalAlphaTop", 0.75f);
		g_bulletImpactOverlay.ms_settings[0].spriteLength = g_visualSettings.Get("misc.DamageOverlay.SpriteLength", 0.25f);
		g_bulletImpactOverlay.ms_settings[0].spriteBaseWidth = g_visualSettings.Get("misc.DamageOverlay.SpriteBaseWidth", 0.2f);
		g_bulletImpactOverlay.ms_settings[0].spriteTipWidth = g_visualSettings.Get("misc.DamageOverlay.SpriteTipWidth", 0.05f);
		g_bulletImpactOverlay.ms_settings[0].angleScalingMult = g_visualSettings.Get("misc.DamageOverlay.AngleScalingMult", 0.85f);

		g_bulletImpactOverlay.ms_settings[1].rampUpDuration = (u32)g_visualSettings.Get("misc.DamageOverlay.FP.RampUpDuration", 50);
		g_bulletImpactOverlay.ms_settings[1].holdDuration = (u32)g_visualSettings.Get("misc.DamageOverlay.FP.HoldDuration", 400);
		g_bulletImpactOverlay.ms_settings[1].rampDownDuration = (u32)g_visualSettings.Get("misc.DamageOverlay.FP.RampDownDuration", 100);
		g_bulletImpactOverlay.ms_settings[1].colourBottom.x = g_visualSettings.Get("misc.DamageOverlay.FP.ColorBottom.red", 0.54f);
		g_bulletImpactOverlay.ms_settings[1].colourBottom.y = g_visualSettings.Get("misc.DamageOverlay.FP.ColorBottom.green", 0.06f);
		g_bulletImpactOverlay.ms_settings[1].colourBottom.z = g_visualSettings.Get("misc.DamageOverlay.FP.ColorBottom.blue", 0.00f);
		g_bulletImpactOverlay.ms_settings[1].colourTop.x = g_visualSettings.Get("misc.DamageOverlay.FP.ColorTop.red", 0.54f);
		g_bulletImpactOverlay.ms_settings[1].colourTop.y = g_visualSettings.Get("misc.DamageOverlay.FP.ColorTop.green", 0.06f);
		g_bulletImpactOverlay.ms_settings[1].colourTop.z = g_visualSettings.Get("misc.DamageOverlay.FP.ColorTop.blue", 0.00f);
		g_bulletImpactOverlay.ms_settings[1].colourBottomEndurance.x = g_visualSettings.Get("misc.DamageOverlay.colourBottomEndurance.red", 0.99f);
		g_bulletImpactOverlay.ms_settings[1].colourBottomEndurance.y = g_visualSettings.Get("misc.DamageOverlay.colourBottomEndurance.green", 0.59f);
		g_bulletImpactOverlay.ms_settings[1].colourBottomEndurance.z = g_visualSettings.Get("misc.DamageOverlay.colourBottomEndurance.blue", 0.32f);
		g_bulletImpactOverlay.ms_settings[1].colourTopEndurance.x = g_visualSettings.Get("misc.DamageOverlay.colourTopEndurance.red", 0.99f);
		g_bulletImpactOverlay.ms_settings[1].colourTopEndurance.y = g_visualSettings.Get("misc.DamageOverlay.colourTopEndurance.green", 0.59f);
		g_bulletImpactOverlay.ms_settings[1].colourTopEndurance.z = g_visualSettings.Get("misc.DamageOverlay.colourTopEndurance.blue", 0.32f);
		g_bulletImpactOverlay.ms_settings[1].globalAlphaBottom = g_visualSettings.Get("misc.DamageOverlay.FP.GlobalAlphaBottom", 1.0f);
		g_bulletImpactOverlay.ms_settings[1].globalAlphaTop = g_visualSettings.Get("misc.DamageOverlay.FP.GlobalAlphaTop", 0.75f);
		g_bulletImpactOverlay.ms_settings[1].spriteLength = g_visualSettings.Get("misc.DamageOverlay.FP.SpriteLength", 0.25f);
		g_bulletImpactOverlay.ms_settings[1].spriteBaseWidth = g_visualSettings.Get("misc.DamageOverlay.FP.SpriteBaseWidth", 0.2f);
		g_bulletImpactOverlay.ms_settings[1].spriteTipWidth = g_visualSettings.Get("misc.DamageOverlay.FP.SpriteTipWidth", 0.05f);
		g_bulletImpactOverlay.ms_settings[1].angleScalingMult = g_visualSettings.Get("misc.DamageOverlay.FP.AngleScalingMult", 0.85f);

#if BOKEH_SUPPORT
		BokehBrightnessExposureMin = g_visualSettings.Get("bokeh.brightnessExposureMin", -3.0f);
		BokehBrightnessExposureMax = g_visualSettings.Get("bokeh.brightnessExposureMax", 1.0f);
		BokehBlurThresholdVal = g_visualSettings.Get("bokeh.cocThreshold", 0.05f);
		bokehSizeMultiplier = g_visualSettings.Get("bokeh.sizeMultiplier", 1.5f);
		BokehGlobalAlpha = g_visualSettings.Get("bokeh.globalAlpha", 1.0f);
		BokehAlphaCutoff = g_visualSettings.Get("bokeh.alphaCutoff", 0.15f);

		BokehShapeExposureRangeMin = g_visualSettings.Get("bokeh.shapeExposureRangeMin", -3.0f);
		BokehShapeExposureRangeMax = g_visualSettings.Get("bokeh.shapeExposureRangeMax", 1.0f);
		
#endif // BOKEH_SUPPORT

		g_DefaultMotionBlurEnabled = ((g_visualSettings.Get("defaultmotionblur.enabled", 0.0f)) > 0.0f);
		g_DefaultMotionBlurStrength = g_visualSettings.Get("defaultmotionblur.strength", 0.04f);

		fpvMotionBlurWeights = Vector3(
			g_visualSettings.Get("fpv.motionblur.weight0", 1.0f),
			g_visualSettings.Get("fpv.motionblur.weight1", 0.0f),
			g_visualSettings.Get("fpv.motionblur.weight2", 0.8f),
			g_visualSettings.Get("fpv.motionblur.weight3", 1.0f));

		fpvMotionVelocityMaxSize = g_visualSettings.Get("fpv.motionblur.max.velocity", 32.0f),

#if ADAPTIVE_DOF
		AdaptiveDepthOfField.UpdateVisualDataSettings();
#endif
	}
}

// Update visual setting after reload. This should only be called from PostFX::UpdateVisualDataSettings()
void PostFX::PostFXParamBlock::UpdateVisualSettings()
{
	PostFXParamBlock::ms_params[0].m_exposureCurveA = g_visualSettings.Get("Exposure.curve.scale", -1.0668907449987120E+02f);
	PostFXParamBlock::ms_params[0].m_exposureCurveB = g_visualSettings.Get("Exposure.curve.power", 4.1897015173052825E-03f);
	PostFXParamBlock::ms_params[0].m_exposureCurveOffset = g_visualSettings.Get("Exposure.curve.offset", 1.0460364172443734E+02f);

	PostFXParamBlock::ms_params[1].m_exposureCurveA = PostFXParamBlock::ms_params[0].m_exposureCurveA;
	PostFXParamBlock::ms_params[1].m_exposureCurveB =  PostFXParamBlock::ms_params[0].m_exposureCurveB;
	PostFXParamBlock::ms_params[1].m_exposureCurveOffset =  PostFXParamBlock::ms_params[0].m_exposureCurveOffset;

	PostFXParamBlock::ms_params[0].m_adaptionMinStepSize = g_visualSettings.Get("Adaptation.min.step.size", 0.15f);
	PostFXParamBlock::ms_params[0].m_adaptionMaxStepSize = g_visualSettings.Get("Adaptation.max.step.size", 15.0f);
	PostFXParamBlock::ms_params[0].m_adaptionStepMult = g_visualSettings.Get("Adaptation.step.size.mult", 9.0f);
	PostFXParamBlock::ms_params[0].m_adaptionThreshold = g_visualSettings.Get("Adaptation.threshold", 0.0f);

	PostFXParamBlock::ms_params[1].m_adaptionMinStepSize = PostFXParamBlock::ms_params[0].m_adaptionMinStepSize;
	PostFXParamBlock::ms_params[1].m_adaptionMaxStepSize = PostFXParamBlock::ms_params[0].m_adaptionMaxStepSize;
	PostFXParamBlock::ms_params[1].m_adaptionStepMult = PostFXParamBlock::ms_params[0].m_adaptionStepMult;
	PostFXParamBlock::ms_params[1].m_adaptionThreshold = PostFXParamBlock::ms_params[0].m_adaptionThreshold;
}

void PostFX::SetLDR8bitHDR8bit()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	float packScalar8 = PostFX::GetPackScalar8bit();

	const float toneMapScaler = 1.0f / packScalar8;
	const float unToneMapScaler = packScalar8;

	Vector4 scalers(
		toneMapScaler, 
		unToneMapScaler,
		toneMapScaler, 
		unToneMapScaler);

	CShaderLib::SetGlobalToneMapScalers(scalers);
}

void PostFX::SetLDR16bitHDR16bit()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Vector4 scalers(
		1.0f, 
		1.0f,
		1.0f, 
		1.0f);

	CShaderLib::SetGlobalToneMapScalers(scalers);
}

#if __XENON
void PostFX::SetLDR8bitHDR10bit()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	float packScalar8 = PostFX::GetPackScalar8bit();

	const float toneMapScaler8 = 1.0f / packScalar8;
	const float unToneMapScaler8 = packScalar8;

	float packScalar10 = PostFX::GetPackScalar10bit();

	const float toneMapScaler10 = 1.0f / packScalar10;
	const float unToneMapScaler10 = packScalar10;

	Vector4 scalers(
		toneMapScaler8, 
		unToneMapScaler8,
		toneMapScaler10,
		unToneMapScaler10);

	CShaderLib::SetGlobalToneMapScalers(scalers);
}

void PostFX::SetLDR10bitHDR10bit()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	float packScalar10 = PostFX::GetPackScalar10bit();	

	const float toneMapScaler = 1.0f / packScalar10;
	const float unToneMapScaler = packScalar10;

	Vector4 scalers(
		toneMapScaler, 
		unToneMapScaler,
		toneMapScaler,
		unToneMapScaler);

	CShaderLib::SetGlobalToneMapScalers(scalers);
}
#endif

void PostFX::SetLDR8bitHDR16bit()
{
#if DEBUG_HDR_BUFFER
	SetLDR8bitHDR8bit();
#else
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	float packScalar8 = PostFX::GetPackScalar8bit();

	const float toneMapScaler = 1.0f / packScalar8;
	const float unToneMapScaler = packScalar8;

	Vector4 scalers(
		toneMapScaler, 
		unToneMapScaler,
		1.0f, 
		1.0f);

	CShaderLib::SetGlobalToneMapScalers(scalers);
#endif	//DEBUG_HDR_BUFFER
}

//////////////////////////////////////////////////////////////////////////
// PedKillOverlay

//////////////////////////////////////////////////////////////////////////

PostFX::PedKillOverlay::PedKillOverlay()
{
	m_effectName = atHashString("PedGunKill", 0x9cfb4c6e);
	m_disable = false;
}

void PostFX::PedKillOverlay::Trigger()
{
	if( m_disable )
		return;

	// Don't trigger this if any effect stack is already running or else
	// we might get weird blending/tc mods overriding each other
	if (ANIMPOSTFXMGR.IsIdle() == false)
	{
		return;
	}

	ANIMPOSTFXMGR.Start(m_effectName, 0U, false, false, false, 0U, AnimPostFXManager::kPedKill);
}
//////////////////////////////////////////////////////////////////////////
//
void PostFX::ScreenBlurFade::Init()
{
	m_state = (int)SBF_IDLE;
	m_fFadeLevel = 0.0f;
	m_fDuration = 250.0f;
}

//////////////////////////////////////////////////////////////////////////
//
void PostFX::ScreenBlurFade::UpdateOnLoadingScreen()
{
	if (m_state == (int)SBF_IDLE)
	{
		return;
	}

	// automatically turn effect off if it was fading out
	if (m_state == (int)SBF_FADING_OUT && m_fFadeLevel == 1.0f)
	{
		Reset();
		return;
	}

	m_fFadeLevel += (((float)fwTimer::GetTimeStepInMilliseconds())/m_fDuration);
	if (m_fFadeLevel >= 1.0f)
	{
		m_fFadeLevel = 1.0f;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PostFX::ScreenBlurFade::Update()
{

	if (m_state == (int)SBF_IDLE)
	{
		return;
	}

	// automatically turn effect off if it was fading out
	if (m_state == (int)SBF_FADING_OUT && m_fFadeLevel == 1.0f)
	{
		Reset();
		return;
	}

	m_fFadeLevel += (((float)fwTimer::GetTimeStepInMilliseconds())/m_fDuration);
	if (m_fFadeLevel >= 1.0f)
	{
		m_fFadeLevel = 1.0f;
	}
}


//////////////////////////////////////////////////////////////////////////
//
void PostFX::ScreenBlurFade::Trigger(bool bFadeOut)
{
	if (m_state != (int)SBF_IDLE && bFadeOut == false)
	{
		return;
	}

	if (bFadeOut && m_state != (int)SBF_FADING_IN) 
	{
		return;
	}

	m_state = (int)(bFadeOut ? SBF_FADING_OUT : SBF_FADING_IN);
	m_fFadeLevel = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
//
void PostFX::BulletImpactOverlay::Entry::Reset()
{
	m_firingWeaponPos.Zero();
	m_fFadeLevel = 0.0f;
	m_fIntensity = 0.0f;
	m_startTime	= 0;
	m_bFrozen = false;
}

//////////////////////////////////////////////////////////////////////////
PostFX::BulletImpactOverlay::Entry* PostFX::BulletImpactOverlay::GetFreeEntry()
{
	for (int i = 0; i < NUM_ENTRIES; i++)
	{
		if (m_entries[i].m_startTime == 0)
		{
			return &m_entries[i];
		}
	}

	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
void PostFX::BulletImpactOverlay::Init()
{

	for (int i = 0; i < NUM_ENTRIES; i++)
	{
		m_entries[i].Reset();
	}
	
#if __BANK
	m_bEnableDebugRender = false;
#endif

	m_drawAfterHud = false;

	// create the vertex declaration
	grcVertexElement elem[] = {
		grcVertexElement(0, grcVertexElement::grcvetPosition,	0, 12, grcFvf::grcdsFloat3),
		grcVertexElement(0, grcVertexElement::grcvetColor,		0, 4, grcFvf::grcdsColor),
		grcVertexElement(0, grcVertexElement::grcvetTexture,	0, 12, grcFvf::grcdsFloat3)
	};
	g_damageSliceDecl = GRCDEVICE.CreateVertexDeclaration(elem,sizeof(elem)/sizeof(grcVertexElement));
	
	for (int i = 0; i < DOT_COUNT; i++)
	{
		ms_settings[i].rampUpDuration = 100;
		ms_settings[i].rampDownDuration = 700;
		ms_settings[i].holdDuration = 200;
		ms_settings[i].spriteLength = 0.25f;
		ms_settings[i].spriteBaseWidth = 0.2f;
		ms_settings[i].spriteTipWidth = 0.05f;
		ms_settings[i].globalAlphaBottom = 1.0f;
		ms_settings[i].globalAlphaTop = 1.0f;
		ms_settings[i].angleScalingMult = 0.85f;
	}

	m_disabled = false;
}

//////////////////////////////////////////////////////////////////////////
//
void PostFX::BulletImpactOverlay::RegisterBulletImpact(const Vector3& vWeaponPos, f32 damageIntensity, bool bIsEnduranceDamage)
{
	if( m_disabled )
	{
		weaponDebugf3("BulletImpactOverlay::RegisterBulletImpact - m_disabled");
		return;
	}

	Entry* pEntry = GetFreeEntry();

	if (pEntry == NULL)
	{
		weaponDebugf3("BulletImpactOverlay::RegisterBulletImpact - pEntry == NULL");
		return;
	}

	pEntry->m_firingWeaponPos = vWeaponPos;
	pEntry->m_fFadeLevel = 0.0f;
	pEntry->m_fIntensity = damageIntensity;
	pEntry->m_startTime	= 1;
	pEntry->m_bIsEnduranceDamage = bIsEnduranceDamage;
	weaponDebugf3("BulletImpactOverlay::RegisterBulletImpact - Triggered");
}

//////////////////////////////////////////////////////////////////////////
//
void PostFX::BulletImpactOverlay::UpdateTiming(Entry* pEntry, const Settings& settings)
{
	if (pEntry->m_startTime	== 1)
	{	
		pEntry->m_startTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	}

	if (pEntry->m_startTime != 0)
	{
		const u32 c_uCurrentTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		float fInterp = 0.0f;
		if (c_uCurrentTime < pEntry->m_startTime + settings.rampUpDuration)
		{
			// Ramping up
			fInterp = (float)(c_uCurrentTime - pEntry->m_startTime) / (float)settings.rampUpDuration;
		}
		else if (c_uCurrentTime < pEntry->m_startTime + settings.rampUpDuration + settings.holdDuration)
		{
			fInterp = 1.0f;
		}
		else if (c_uCurrentTime < pEntry->m_startTime + settings.rampUpDuration + settings.holdDuration + settings.rampDownDuration)
		{
			fInterp = (float)(settings.rampDownDuration - (c_uCurrentTime - pEntry->m_startTime - settings.rampUpDuration - settings.holdDuration)) / (float)settings.rampDownDuration;
		}
		else
		{
			pEntry->Reset();
		}

		pEntry->m_fFadeLevel = fInterp;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PostFX::Adaptation::Synchronise()
{
	ms_bufferIndex ^= 1;
}

//////////////////////////////////////////////////////////////////////////
//
void PostFX::ApplyFXAA(grcRenderTarget *pDstRT, grcRenderTarget *pSrcRT, const Vector4 &scissorRect)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PostFXShader->SetVar(FXAABackBuffer, pSrcRT);

	// rcpFrameId
	const float rcpWidth = 1.0f / pSrcRT->GetWidth();
	const float rcpHeight = 1.0f / pSrcRT->GetHeight();

	PostFXShader->SetVar(rcpFrameId, Vector2(rcpWidth, rcpHeight));

	SetNonDepthFXStateBlocks();

	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, NULL);

	GRCDEVICE.SetScissor((int)scissorRect.x, (int)scissorRect.y, (int)scissorRect.z, (int)scissorRect.w);

	PostFXBlit(NULL, PostFXTechnique, FXAAPassToUse, false, false, NULL, 
		#if __D3D11 || RSG_ORBIS
			false 
		#else
			XENON_SWITCH(true, false)
		#endif
		);			

	grcResolveFlags resolveFlags;
	resolveFlags.ClearColor		= false;
	resolveFlags.NeedResolve	= XENON_SWITCH(true, false);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
}

#if PTFX_APPLY_DOF_TO_PARTICLES || APPLY_DOF_TO_ALPHA_DECALS
void PostFX::SetupBlendForDOF(grcBlendStateDesc &blendStateDesc, bool forAlphaPass)
{
	blendStateDesc.IndependentBlendEnable = 1;

	//use a different set of blend states for the alpha pass, 
	//as its rendered back to front its better just to use the last value thats written in
	if(forAlphaPass)
	{
	//Setup blend state for particle depth
	blendStateDesc.BlendRTDesc[1].BlendEnable = 1;
		blendStateDesc.BlendRTDesc[1].DestBlend = grcRSV::BLEND_ZERO;
		blendStateDesc.BlendRTDesc[1].SrcBlend = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[1].BlendOp = grcRSV::BLENDOP_ADD;
		blendStateDesc.BlendRTDesc[1].SrcBlendAlpha = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[1].DestBlendAlpha = grcRSV::BLEND_ZERO;
		blendStateDesc.BlendRTDesc[1].BlendOpAlpha = grcRSV::BLENDOP_ADD;

		//Setup blend state for particle alpha
		blendStateDesc.BlendRTDesc[2].BlendEnable = 1;
		blendStateDesc.BlendRTDesc[2].DestBlend = grcRSV::BLEND_ZERO;
		blendStateDesc.BlendRTDesc[2].SrcBlend = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[2].BlendOp = grcRSV::BLENDOP_ADD;
		blendStateDesc.BlendRTDesc[2].SrcBlendAlpha = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[2].DestBlendAlpha = grcRSV::BLEND_ZERO;
		blendStateDesc.BlendRTDesc[2].BlendOpAlpha = grcRSV::BLENDOP_ADD;
	}
	else
	{
		//Setup blend state for particle depth
		blendStateDesc.BlendRTDesc[1].BlendEnable = 1;
		blendStateDesc.BlendRTDesc[1].DestBlend = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[1].SrcBlend = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[1].BlendOp = grcRSV::BLENDOP_MAX;
		blendStateDesc.BlendRTDesc[1].SrcBlendAlpha = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[1].DestBlendAlpha = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[1].BlendOpAlpha = grcRSV::BLENDOP_MAX;

		//Setup blend state for particle alpha
		blendStateDesc.BlendRTDesc[2].BlendEnable = 1;
		blendStateDesc.BlendRTDesc[2].DestBlend = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[2].SrcBlend = grcRSV::BLEND_INVSRCALPHA;
		blendStateDesc.BlendRTDesc[2].BlendOp = grcRSV::BLENDOP_ADD;
		blendStateDesc.BlendRTDesc[2].SrcBlendAlpha = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[2].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		blendStateDesc.BlendRTDesc[2].BlendOpAlpha = grcRSV::BLENDOP_ADD;
	}
}
#endif //PTFX_APPLY_DOF_TO_PARTICLES || APPLY_DOF_TO_ALPHA_DECALS


#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS

void PostFX::CreateReplayThumbnail()
{
	CReplayThumbnail::CreateReplayThumbnail();
}

#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif // GTA_REPLAY
